#include "dolby_hal_stage.h"

#include <android/log.h>
#include <dlfcn.h>
#include <linux/futex.h>
#include <math.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include <atomic>
#include <condition_variable>
#include <thread>
#include <vector>

#include <aidl/android/hardware/audio/effect/IEffect.h>
#include <aidl/android/hardware/audio/effect/IFactory.h>

// The generated IEffect/IFactory sources also contain the service (Bn) side, which marks binders
// VINTF-stable through a platform-only libbinder_ndk symbol the NDK stubs do not export. Only the
// client side is used here, so this hidden stub only satisfies the link and is never called.
extern "C" __attribute__((weak, visibility("hidden"))) void AIBinder_markVintfStability(AIBinder*) {}

namespace port {
namespace {

using aidl::android::hardware::audio::effect::CommandId;
using aidl::android::hardware::audio::effect::IEffect;
using aidl::android::hardware::audio::effect::IFactory;
using aidl::android::hardware::audio::effect::Parameter;
using aidl::android::hardware::audio::effect::Spatializer;
using aidl::android::hardware::common::fmq::MQDescriptor;
using aidl::android::hardware::common::fmq::SynchronizedReadWrite;
using aidl::android::media::audio::common::AudioChannelLayout;
using aidl::android::media::audio::common::AudioConfig;
using aidl::android::media::audio::common::AudioDeviceDescription;
using aidl::android::media::audio::common::AudioDeviceType;
using aidl::android::media::audio::common::AudioFormatType;
using aidl::android::media::audio::common::AudioUuid;
using aidl::android::media::audio::common::HeadTracking;
using aidl::android::media::audio::common::PcmType;
using aidl::android::media::audio::common::Spatialization;

constexpr char kTag[] = "SscPort";
constexpr char kFactory[] = "android.hardware.audio.effect.IFactory/default";
// V1 effects wait for kEventFlagNotEmpty (0x1); V2+ (reopen-capable) for kEventFlagDataMqNotEmpty.
constexpr uint32_t kWakeBits = 0x1 | (1u << 11);
constexpr float kScale = 8388608.0f;  // Q8.23
// Measured on stereo pink noise: rendered output is about 8.5 dB above the input.
constexpr float kOutputGain = 0.5f;
// Frames are pipelined: each call submits the current frame and returns the previous one, which
// had a whole frame period (18 ms at 48 kHz) to be processed, so the encoder rarely waits.
constexpr int kWaitUs = 8000;
constexpr int kMaxConsecutiveFailures = 50;

// AOSP Spatializer type; the Dolby implementation registers with the same UUID.
AudioUuid dolbySpatializer() {
    AudioUuid u;
    u.timeLow = int32_t(0xccd4cf09);
    u.timeMid = 0xa79d;
    u.timeHiAndVersion = 0x46c2;
    u.clockSeq = 0x9aae;
    u.node = {0x06, 0xa1, 0x69, 0x8d, 0x6c, 0x8f};
    return u;
}

// Minimal SynchronizedReadWrite FMQ endpoint (grantors: read ptr, write ptr, data, event flag).
template <typename T>
class Queue {
public:
    ~Queue() {
        for (auto& m : maps_) munmap(m.first, m.second);
    }
    bool map(const MQDescriptor<T, SynchronizedReadWrite>& d) {
        if (d.grantors.size() < 3 || size_t(d.quantum) != sizeof(T)) return false;
        read_ = static_cast<std::atomic<uint64_t>*>(grantor(d, 0));
        write_ = static_cast<std::atomic<uint64_t>*>(grantor(d, 1));
        data_ = static_cast<uint8_t*>(grantor(d, 2));
        capacity_ = size_t(d.grantors[2].extent);
        if (d.grantors.size() > 3) flag_ = static_cast<std::atomic<uint32_t>*>(grantor(d, 3));
        return read_ && write_ && data_ && capacity_ > 0;
    }
    size_t readable() const {
        return size_t(write_->load(std::memory_order_acquire) - read_->load(std::memory_order_relaxed)) /
               sizeof(T);
    }
    size_t writable() const {
        return (capacity_ - size_t(write_->load(std::memory_order_relaxed) -
                                   read_->load(std::memory_order_acquire))) / sizeof(T);
    }
    bool write(const T* items, size_t count) {
        if (count > writable()) return false;
        uint64_t w = write_->load(std::memory_order_relaxed);
        copyIn(size_t(w % capacity_), reinterpret_cast<const uint8_t*>(items), count * sizeof(T));
        write_->store(w + count * sizeof(T), std::memory_order_release);
        return true;
    }
    bool read(T* items, size_t count) {
        if (count > readable()) return false;
        uint64_t r = read_->load(std::memory_order_relaxed);
        copyOut(size_t(r % capacity_), reinterpret_cast<uint8_t*>(items), count * sizeof(T));
        read_->store(r + count * sizeof(T), std::memory_order_release);
        return true;
    }
    void drain() {
        read_->store(write_->load(std::memory_order_acquire), std::memory_order_release);
    }
    std::atomic<uint32_t>* flag() const { return flag_; }

private:
    void* grantor(const MQDescriptor<T, SynchronizedReadWrite>& d, size_t index) {
        const auto& g = d.grantors[index];
        if (g.fdIndex < 0 || size_t(g.fdIndex) >= d.handle.fds.size()) return nullptr;
        const int64_t page = sysconf(_SC_PAGESIZE);
        const int64_t base = (g.offset / page) * page;
        const size_t length = size_t(g.offset - base + g.extent);
        void* p = mmap(nullptr, length, PROT_READ | PROT_WRITE, MAP_SHARED,
                       d.handle.fds[g.fdIndex].get(), base);
        if (p == MAP_FAILED) return nullptr;
        maps_.push_back({p, length});
        return static_cast<uint8_t*>(p) + (g.offset - base);
    }
    void copyIn(size_t at, const uint8_t* src, size_t n) {
        const size_t first = n < capacity_ - at ? n : capacity_ - at;
        memcpy(data_ + at, src, first);
        memcpy(data_, src + first, n - first);
    }
    void copyOut(size_t at, uint8_t* dst, size_t n) {
        const size_t first = n < capacity_ - at ? n : capacity_ - at;
        memcpy(dst, data_ + at, first);
        memcpy(dst + first, data_, n - first);
    }
    std::vector<std::pair<void*, size_t>> maps_;
    std::atomic<uint64_t>* read_ = nullptr;
    std::atomic<uint64_t>* write_ = nullptr;
    uint8_t* data_ = nullptr;
    std::atomic<uint32_t>* flag_ = nullptr;
    size_t capacity_ = 0;
};

void wake(std::atomic<uint32_t>* word, uint32_t bits) {
    word->fetch_or(bits);
    syscall(__NR_futex, reinterpret_cast<uint32_t*>(word), FUTEX_WAKE_BITSET, INT32_MAX, nullptr,
            nullptr, bits);
}

std::shared_ptr<IFactory> factory() {
    using Check = AIBinder* (*)(const char*);
    static Check check = [] {
        void* lib = dlopen("libbinder_ndk.so", RTLD_NOW);
        return lib ? reinterpret_cast<Check>(dlsym(lib, "AServiceManager_checkService")) : nullptr;
    }();
    if (!check) return nullptr;
    ndk::SpAIBinder binder(check(kFactory));
    return binder.get() ? IFactory::fromBinder(binder) : nullptr;
}

AudioConfig stereo(uint32_t rate, size_t frames) {
    AudioConfig c;
    c.base.sampleRate = int32_t(rate);
    c.base.channelMask = AudioChannelLayout::make<AudioChannelLayout::layoutMask>(
        AudioChannelLayout::LAYOUT_STEREO);
    c.base.format.type = AudioFormatType::PCM;
    c.base.format.pcm = PcmType::FLOAT_32_BIT;
    c.frameCount = int64_t(frames);
    return c;
}

Parameter spatializer(Spatializer value) {
    return Parameter::make<Parameter::specific>(
        Parameter::Specific::make<Parameter::Specific::spatializer>(value));
}

float fromBits(uint32_t bits) {
    float v;
    memcpy(&v, &bits, sizeof v);
    return v;
}

}  // namespace

struct DolbyHalStage::Session {
    std::shared_ptr<IFactory> factory;
    std::shared_ptr<IEffect> effect;
    Queue<IEffect::Status> status;
    Queue<float> input;
    Queue<float> output;
    std::vector<float> current, previous, result;
    size_t frames = 0;
    bool pending = false;  // a submitted frame whose result is not collected yet
    bool stale = false;    // the pending result belongs to a frame already passed through
    int failures = 0;
    unsigned processed = 0;

    // Head pose goes through its own thread: setParameter is a blocking binder call.
    std::thread poser;
    std::mutex poseMutex;
    std::condition_variable poseCv;
    bool quit = false;
    std::atomic<unsigned> poseSets{0};
    std::atomic<long> maxPoseUs{0};

    // Diagnostics, reported every kReportFrames frames.
    unsigned windowFrames = 0, writeFull = 0, timeouts = 0, badStatus = 0, clipped = 0;
    long maxWaitUs = 0;
    float peak = 0;
};

namespace {
constexpr unsigned kReportFrames = 500;
long elapsedUs(const timespec& a) {
    timespec b;
    clock_gettime(CLOCK_MONOTONIC, &b);
    return (b.tv_sec - a.tv_sec) * 1000000L + (b.tv_nsec - a.tv_nsec) / 1000;
}
}  // namespace

DolbyHalStage::DolbyHalStage() = default;
DolbyHalStage::~DolbyHalStage() { stop(); }

bool DolbyHalStage::running() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return session_ != nullptr;
}

void DolbyHalStage::set_yaw(float radians) {
    uint32_t bits;
    memcpy(&bits, &radians, sizeof bits);
    __atomic_store_n(&yaw_bits_, bits, __ATOMIC_RELAXED);
}

bool DolbyHalStage::start(uint32_t sample_rate, size_t frames) {
    stop();
    auto s = std::make_unique<Session>();
    s->factory = factory();
    if (!s->factory) {
        __android_log_print(ANDROID_LOG_WARN, kTag, "stage=dolby event=no_factory");
        return false;
    }
    auto st = s->factory->createEffect(dolbySpatializer(), &s->effect);
    if (!st.isOk() || !s->effect) {
        __android_log_print(ANDROID_LOG_WARN, kTag, "stage=dolby event=create_failed %s",
                            st.getDescription().c_str());
        return false;
    }
    Parameter::Common common;
    common.session = 0;
    common.ioHandle = 0;
    common.input = stereo(sample_rate, frames);
    common.output = stereo(sample_rate, frames);
    IEffect::OpenEffectReturn ret;
    st = s->effect->open(common, std::nullopt, &ret);
    if (!st.isOk() || !s->status.map(ret.statusMQ) || !s->input.map(ret.inputDataMQ) ||
        !s->output.map(ret.outputDataMQ) || !s->status.flag()) {
        __android_log_print(ANDROID_LOG_WARN, kTag, "stage=dolby event=open_failed %s",
                            st.getDescription().c_str());
        s->factory->destroyEffect(s->effect);
        return false;
    }
    // The effect must know it renders for Bluetooth A2DP: it then loads the headphone tuning and
    // honors head tracking (for the speaker it uses a downmix matrix instead).
    AudioDeviceDescription device;
    device.type = AudioDeviceType::OUT_DEVICE;
    device.connection = AudioDeviceDescription::CONNECTION_BT_A2DP;
    s->effect->setParameter(
        Parameter::make<Parameter::deviceDescription>(std::vector<AudioDeviceDescription>{device}));
    s->effect->setParameter(spatializer(Spatializer::make<Spatializer::spatializationLevel>(
        Spatialization::Level::MULTICHANNEL)));
    s->effect->setParameter(spatializer(Spatializer::make<Spatializer::spatializationMode>(
        Spatialization::Mode::BINAURAL)));
    s->effect->setParameter(spatializer(Spatializer::make<Spatializer::headTrackingConnectionMode>(
        HeadTracking::ConnectionMode::FRAMEWORK_PROCESSED)));
    s->effect->setParameter(spatializer(Spatializer::make<Spatializer::headTrackingMode>(
        HeadTracking::Mode::RELATIVE_WORLD)));
    st = s->effect->command(CommandId::START);
    if (!st.isOk()) {
        __android_log_print(ANDROID_LOG_WARN, kTag, "stage=dolby event=start_failed %s",
                            st.getDescription().c_str());
        s->effect->close();
        s->factory->destroyEffect(s->effect);
        return false;
    }
    s->frames = frames;
    s->current.assign(frames * 2, 0.0f);
    s->previous.assign(frames * 2, 0.0f);
    s->result.assign(frames * 2, 0.0f);

    Session* raw = s.get();
    const uint32_t* yawBits = &yaw_bits_;
    s->poser = std::thread([raw, yawBits] {
        float sent = NAN;
        std::unique_lock<std::mutex> lock(raw->poseMutex);
        while (!raw->quit) {
            raw->poseCv.wait_for(lock, std::chrono::milliseconds(20));
            if (raw->quit) break;
            const float yaw = fromBits(__atomic_load_n(yawBits, __ATOMIC_RELAXED));
            if (fabsf(yaw - sent) < 0.002f) continue;
            lock.unlock();
            timespec t0;
            clock_gettime(CLOCK_MONOTONIC, &t0);
            HeadTracking::SensorData data = HeadTracking::SensorData::make<
                HeadTracking::SensorData::headToStage>(std::array<float, 6>{0, 0, 0, 0, 0, yaw});
            if (raw->effect->setParameter(spatializer(
                    Spatializer::make<Spatializer::headTrackingSensorData>(data))).isOk()) {
                sent = yaw;
            }
            const long us = elapsedUs(t0);
            raw->poseSets++;
            if (us > raw->maxPoseUs.load()) raw->maxPoseUs = us;
            lock.lock();
        }
    });

    std::lock_guard<std::mutex> lock(mutex_);
    session_ = std::move(s);
    __android_log_print(ANDROID_LOG_WARN, kTag, "stage=dolby event=started rate=%u frames=%zu",
                        sample_rate, frames);
    return true;
}

void DolbyHalStage::stop() {
    std::unique_ptr<Session> s;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        s = std::move(session_);
    }
    if (!s) return;
    {
        std::lock_guard<std::mutex> lock(s->poseMutex);
        s->quit = true;
    }
    s->poseCv.notify_all();
    if (s->poser.joinable()) s->poser.join();
    s->effect->command(CommandId::STOP);
    s->effect->close();
    s->factory->destroyEffect(s->effect);
    __android_log_print(ANDROID_LOG_WARN, kTag, "stage=dolby event=stopped processed=%u",
                        s->processed);
}

void DolbyHalStage::process(int32_t* pcm, size_t frames) {
    std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock() || !session_ || !__atomic_load_n(&enabled_, __ATOMIC_RELAXED)) return;
    Session& s = *session_;
    // An unresponsive HAL is not waited on again; the next session start creates a new effect.
    if (frames != s.frames || s.failures >= kMaxConsecutiveFailures) return;
    const size_t samples = frames * 2;
    for (size_t i = 0; i < samples; ++i) s.current[i] = pcm[i] / kScale;

    // 1. Collect the result of the frame submitted by the previous call.
    bool processed = false;
    if (s.pending) {
        timespec start;
        clock_gettime(CLOCK_MONOTONIC, &start);
        while (s.status.readable() == 0 && elapsedUs(start) <= kWaitUs) usleep(100);
        const long waited = elapsedUs(start);
        if (waited > s.maxWaitUs) s.maxWaitUs = waited;
        IEffect::Status status{};
        if (s.status.read(&status, 1)) {
            s.pending = false;
            const bool good = status.status == 0 && size_t(status.fmqProduced) == samples &&
                              s.output.read(s.result.data(), samples);
            if (!good) {
                s.badStatus++;
                s.output.drain();
            }
            processed = good && !s.stale;
            s.stale = false;
        } else {
            s.timeouts++;
            s.stale = true;  // the late result will be discarded when it arrives
        }
    }

    // 2. Submit the current frame.
    if (!s.pending) {
        if (s.input.write(s.current.data(), samples)) {
            wake(s.status.flag(), kWakeBits);
            s.pending = true;
        } else {
            s.writeFull++;
            s.input.drain();
        }
    }

    // 3. Output the previous frame: processed, or passed through when its result is missing.
    if (processed) {
        s.failures = 0;
        for (size_t i = 0; i < samples; ++i) {
            const float out = s.result[i] * kOutputGain;
            const float mag = fabsf(out);
            if (mag > s.peak) s.peak = mag;
            if (mag >= 1.0f) s.clipped++;
            const float v = out * kScale;
            pcm[i] = v >= 8388607.0f ? 8388607 : v <= -8388608.0f ? -8388608 : int32_t(v);
        }
        if (s.processed++ == 0) {
            __android_log_print(ANDROID_LOG_WARN, kTag, "stage=dolby event=first_frame frames=%zu",
                                frames);
        }
    } else {
        if (s.processed > 0 && ++s.failures == kMaxConsecutiveFailures) {
            __android_log_print(ANDROID_LOG_WARN, kTag, "stage=dolby event=unresponsive");
        }
        for (size_t i = 0; i < samples; ++i) {
            const float v = s.previous[i] * kScale;
            pcm[i] = v >= 8388607.0f ? 8388607 : v <= -8388608.0f ? -8388608 : int32_t(v);
        }
    }
    s.previous.swap(s.current);

    if (++s.windowFrames >= kReportFrames) {
        __android_log_print(ANDROID_LOG_WARN, kTag,
                            "stage=dolby event=stats frames=%u write_full=%u timeouts=%u bad=%u "
                            "clipped=%u peak=%.3f max_wait_us=%ld pose_sets=%u max_pose_us=%ld",
                            s.windowFrames, s.writeFull, s.timeouts, s.badStatus, s.clipped, s.peak,
                            s.maxWaitUs, s.poseSets.exchange(0), s.maxPoseUs.exchange(0));
        s.windowFrames = s.writeFull = s.timeouts = s.badStatus = s.clipped = 0;
        s.maxWaitUs = 0;
        s.peak = 0;
    }
}

}  // namespace port
