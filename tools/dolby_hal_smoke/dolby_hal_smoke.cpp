// Standalone smoke: process PCM with the device's own Dolby Spatializer through the AIDL audio
// effect HAL (android.hardware.audio.effect.IFactory/default), i.e. the licensed implementation
// running in its normal HAL process. Minimal client-side FMQ (SynchronizedReadWrite) + EventFlag.
//
// usage: dolby_hal_smoke list
//        dolby_hal_smoke run <impl-uuid> <in.wav> <out.wav> [in714] [src=<ch>] [yaw=<deg>] [sweep]
//                        [level=0|1|2] [ht=0|1]

#include <dlfcn.h>
#include <linux/futex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <atomic>
#include <optional>
#include <string>
#include <vector>

#include <aidl/android/hardware/audio/effect/IEffect.h>
#include <aidl/android/hardware/audio/effect/IFactory.h>
#include <android/binder_ibinder.h>

// Platform-only libbinder_ndk symbol referenced by the generated stable-interface code; it is only
// called for local binders, which this client never creates. The device library provides the real
// one when present.
extern "C" __attribute__((weak, visibility("hidden"))) void AIBinder_markVintfStability(AIBinder*) {}

using aidl::android::hardware::audio::effect::CommandId;
using aidl::android::hardware::audio::effect::Descriptor;
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
using aidl::android::media::audio::common::AudioFormatDescription;
using aidl::android::media::audio::common::AudioFormatType;
using aidl::android::media::audio::common::AudioUuid;
using aidl::android::media::audio::common::HeadTracking;
using aidl::android::media::audio::common::PcmType;
using aidl::android::media::audio::common::Spatialization;

namespace {

int kRate = 48000;  // rate=<hz>
constexpr int kBlock = 480;
// V1 effects wait for kEventFlagNotEmpty (0x1); V2+ (reopen-capable) for kEventFlagDataMqNotEmpty (1 << 11).
constexpr uint32_t kEventFlagNotEmpty = 0x1 | (1u << 11);

std::string uuidString(const AudioUuid& u) {
    char s[64];
    snprintf(s, sizeof s, "%08x-%04x-%04x-%04x-", u.timeLow, u.timeMid & 0xffff,
             u.timeHiAndVersion & 0xffff, u.clockSeq & 0xffff);
    std::string out = s;
    for (auto b : u.node) {
        snprintf(s, sizeof s, "%02x", b & 0xff);
        out += s;
    }
    return out;
}

bool parseUuid(const char* s, AudioUuid* u) {
    unsigned a, b, c, d, n[6];
    if (sscanf(s, "%8x-%4x-%4x-%4x-%2x%2x%2x%2x%2x%2x", &a, &b, &c, &d, &n[0], &n[1], &n[2], &n[3],
               &n[4], &n[5]) != 10)
        return false;
    u->timeLow = int(a);
    u->timeMid = int(b);
    u->timeHiAndVersion = int(c);
    u->clockSeq = int(d);
    u->node.assign(n, n + 6);
    return true;
}

// Minimal SynchronizedReadWrite FMQ endpoint (grantors: read ptr, write ptr, data, event flag).
template <typename T>
class Queue {
public:
    bool map(const MQDescriptor<T, SynchronizedReadWrite>& d) {
        if (d.grantors.size() < 3) return false;
        read_ = static_cast<std::atomic<uint64_t>*>(mapGrantor(d, 0));
        write_ = static_cast<std::atomic<uint64_t>*>(mapGrantor(d, 1));
        data_ = static_cast<uint8_t*>(mapGrantor(d, 2));
        capacity_ = size_t(d.grantors[2].extent);
        quantum_ = size_t(d.quantum);
        if (d.grantors.size() > 3) flag_ = static_cast<std::atomic<uint32_t>*>(mapGrantor(d, 3));
        return read_ && write_ && data_ && quantum_ == sizeof(T);
    }
    size_t availableToRead() const {
        return size_t(write_->load(std::memory_order_acquire) - read_->load(std::memory_order_relaxed)) /
               quantum_;
    }
    size_t availableToWrite() const {
        return (capacity_ - size_t(write_->load(std::memory_order_relaxed) -
                                   read_->load(std::memory_order_acquire))) / quantum_;
    }
    bool write(const T* items, size_t count) {
        if (count > availableToWrite()) return false;
        uint64_t w = write_->load(std::memory_order_relaxed);
        copyIn(size_t(w % capacity_), reinterpret_cast<const uint8_t*>(items), count * quantum_);
        write_->store(w + count * quantum_, std::memory_order_release);
        return true;
    }
    bool read(T* items, size_t count) {
        if (count > availableToRead()) return false;
        uint64_t r = read_->load(std::memory_order_relaxed);
        copyOut(size_t(r % capacity_), reinterpret_cast<uint8_t*>(items), count * quantum_);
        read_->store(r + count * quantum_, std::memory_order_release);
        return true;
    }
    std::atomic<uint32_t>* flag() const { return flag_; }

private:
    static void* mapGrantor(const MQDescriptor<T, SynchronizedReadWrite>& d, size_t index) {
        const auto& g = d.grantors[index];
        if (g.fdIndex < 0 || size_t(g.fdIndex) >= d.handle.fds.size()) return nullptr;
        long page = sysconf(_SC_PAGESIZE);
        int64_t base = (g.offset / page) * page;
        size_t length = size_t(g.offset - base + g.extent);
        void* p = mmap(nullptr, length, PROT_READ | PROT_WRITE, MAP_SHARED,
                       d.handle.fds[g.fdIndex].get(), base);
        if (p == MAP_FAILED) return nullptr;
        return static_cast<uint8_t*>(p) + (g.offset - base);
    }
    void copyIn(size_t at, const uint8_t* src, size_t n) {
        size_t first = std::min(n, capacity_ - at);
        memcpy(data_ + at, src, first);
        memcpy(data_, src + first, n - first);
    }
    void copyOut(size_t at, uint8_t* dst, size_t n) {
        size_t first = std::min(n, capacity_ - at);
        memcpy(dst, data_ + at, first);
        memcpy(dst + first, data_, n - first);
    }
    std::atomic<uint64_t>* read_ = nullptr;
    std::atomic<uint64_t>* write_ = nullptr;
    uint8_t* data_ = nullptr;
    std::atomic<uint32_t>* flag_ = nullptr;
    size_t capacity_ = 0, quantum_ = 0;
};

void wake(std::atomic<uint32_t>* word, uint32_t bits) {
    word->fetch_or(bits);
    syscall(__NR_futex, reinterpret_cast<uint32_t*>(word), FUTEX_WAKE_BITSET, INT32_MAX, nullptr,
            nullptr, bits);
}

std::shared_ptr<IFactory> factory() {
    using Check = AIBinder* (*)(const char*);
    void* lib = dlopen("libbinder_ndk.so", RTLD_NOW);
    auto check = lib ? reinterpret_cast<Check>(dlsym(lib, "AServiceManager_checkService")) : nullptr;
    if (!check) return nullptr;
    ndk::SpAIBinder binder(check("android.hardware.audio.effect.IFactory/default"));
    return binder.get() ? IFactory::fromBinder(binder) : nullptr;
}

AudioConfig config(int32_t layout) {
    AudioConfig c;
    c.base.sampleRate = kRate;
    c.base.channelMask = AudioChannelLayout::make<AudioChannelLayout::layoutMask>(layout);
    c.base.format.type = AudioFormatType::PCM;
    c.base.format.pcm = PcmType::FLOAT_32_BIT;
    c.frameCount = kBlock;
    return c;
}

Parameter spatializerParam(Spatializer value) {
    return Parameter::make<Parameter::specific>(
        Parameter::Specific::make<Parameter::Specific::spatializer>(value));
}

bool readWav(const char* path, std::vector<float>* out) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    std::vector<uint8_t> d;
    uint8_t buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) d.insert(d.end(), buf, buf + n);
    fclose(f);
    if (d.size() < 44) return false;
    out->resize((d.size() - 44) / 4);
    memcpy(out->data(), d.data() + 44, out->size() * 4);
    return true;
}

void writeWav(const char* path, const std::vector<float>& s) {
    FILE* f = fopen(path, "wb");
    if (!f) return;
    uint32_t dataLen = s.size() * 4, riffLen = 36 + dataLen, fmtLen = 16, rate = kRate,
             byteRate = kRate * 8;
    uint16_t fmt = 3, ch = 2, align = 8, bits = 32;
    fwrite("RIFF", 1, 4, f); fwrite(&riffLen, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    fwrite(&fmtLen, 4, 1, f); fwrite(&fmt, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&rate, 4, 1, f);
    fwrite(&byteRate, 4, 1, f); fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&dataLen, 4, 1, f); fwrite(s.data(), 4, s.size(), f);
    fclose(f);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) return 2;
    auto f = factory();
    if (!f) {
        printf("RESULT no_factory\n");
        return 1;
    }
    if (!strcmp(argv[1], "list")) {
        std::vector<Descriptor> all;
        auto st = f->queryEffects(std::nullopt, std::nullopt, std::nullopt, &all);
        printf("queryEffects ok=%d count=%zu\n", st.isOk(), all.size());
        for (auto& d : all)
            printf("  type=%s impl=%s name=%s implementor=%s\n", uuidString(d.common.id.type).c_str(),
                   uuidString(d.common.id.uuid).c_str(), d.common.name.c_str(),
                   d.common.implementor.c_str());
        return 0;
    }
    if (strcmp(argv[1], "run") || argc < 5) return 2;
    AudioUuid impl;
    if (!parseUuid(argv[2], &impl)) return 2;
    bool in714 = false, sweep = false;
    int src = -1, level = 1, ht = 1;
    double yawDeg = 0;
    for (int i = 5; i < argc; ++i) {
        if (!strcmp(argv[i], "in714")) in714 = true;
        if (!strcmp(argv[i], "sweep")) sweep = true;
        if (!strncmp(argv[i], "src=", 4)) src = atoi(argv[i] + 4);
        if (!strncmp(argv[i], "yaw=", 4)) yawDeg = atof(argv[i] + 4);
        if (!strncmp(argv[i], "level=", 6)) level = atoi(argv[i] + 6);
        if (!strncmp(argv[i], "ht=", 3)) ht = atoi(argv[i] + 3);
        if (!strncmp(argv[i], "rate=", 5)) kRate = atoi(argv[i] + 5);
    }
    const int32_t inLayout = in714 ? AudioChannelLayout::LAYOUT_7POINT1POINT4
                                   : AudioChannelLayout::LAYOUT_STEREO;
    const int inCh = in714 ? 12 : 2;

    std::shared_ptr<IEffect> effect;
    auto st = f->createEffect(impl, &effect);
    if (!st.isOk() || !effect) {
        printf("RESULT create_failed %s\n", st.getDescription().c_str());
        return 1;
    }
    Parameter::Common common;
    common.session = 1;
    common.ioHandle = 1;
    common.input = config(inLayout);
    common.output = config(AudioChannelLayout::LAYOUT_STEREO);
    IEffect::OpenEffectReturn ret;
    st = effect->open(common, std::nullopt, &ret);
    if (!st.isOk()) {
        printf("RESULT open_failed %s\n", st.getDescription().c_str());
        return 1;
    }
    Queue<IEffect::Status> status;
    Queue<float> input, output;
    if (!status.map(ret.statusMQ) || !input.map(ret.inputDataMQ) || !output.map(ret.outputDataMQ) ||
        !status.flag()) {
        printf("RESULT fmq_map_failed\n");
        return 1;
    }
    auto set = [&](Spatializer v, const char* what) {
        auto s = effect->setParameter(spatializerParam(v));
        printf("  setParameter %s -> %s\n", what, s.isOk() ? "ok" : s.getDescription().c_str());
    };
    {
        // Tell the effect it renders for Bluetooth A2DP (legacy AUDIO_DEVICE_OUT_BLUETOOTH_A2DP).
        AudioDeviceDescription device;
        device.type = AudioDeviceType::OUT_DEVICE;
        device.connection = AudioDeviceDescription::CONNECTION_BT_A2DP;
        auto s = effect->setParameter(
            Parameter::make<Parameter::deviceDescription>(std::vector<AudioDeviceDescription>{device}));
        printf("  setParameter device -> %s\n", s.isOk() ? "ok" : s.getDescription().c_str());
    }
    set(Spatializer::make<Spatializer::spatializationLevel>(Spatialization::Level(level)), "level");
    set(Spatializer::make<Spatializer::spatializationMode>(Spatialization::Mode::BINAURAL), "mode");
    set(Spatializer::make<Spatializer::headTrackingConnectionMode>(
            HeadTracking::ConnectionMode::FRAMEWORK_PROCESSED), "connection");
    set(Spatializer::make<Spatializer::headTrackingMode>(
            ht ? HeadTracking::Mode::RELATIVE_WORLD : HeadTracking::Mode::DISABLED), "ht_mode");
    st = effect->command(CommandId::START);
    printf("  command START -> %s\n", st.isOk() ? "ok" : st.getDescription().c_str());

    std::vector<float> in;
    if (!readWav(argv[3], &in)) {
        printf("RESULT bad_input\n");
        return 1;
    }
    const size_t frames = in.size() / 2 / kBlock * kBlock;
    std::vector<float> out(frames * 2, 0.0f), block(kBlock * inCh), outBlock(kBlock * 2);
    size_t produced = 0, timeouts = 0, poseFail = 0;
    for (size_t at = 0; at < frames; at += kBlock) {
        if (ht) {
            double yaw = sweep ? -90.0 + 180.0 * double(at) / double(frames) : yawDeg;
            HeadTracking::SensorData data = HeadTracking::SensorData::make<
                HeadTracking::SensorData::headToStage>(
                std::array<float, 6>{0, 0, 0, 0, 0, float(yaw * M_PI / 180.0)});
            auto s = effect->setParameter(spatializerParam(
                Spatializer::make<Spatializer::headTrackingSensorData>(data)));
            if (!s.isOk()) ++poseFail;
        }
        std::fill(block.begin(), block.end(), 0.0f);
        for (int k = 0; k < kBlock; ++k) {
            if (src >= 0) {
                block[k * inCh + src] = in[(at + k) * 2];
            } else {
                block[k * inCh] = in[(at + k) * 2];
                block[k * inCh + 1] = in[(at + k) * 2 + 1];
            }
        }
        if (!input.write(block.data(), block.size())) {
            printf("RESULT input_full at=%zu\n", at);
            break;
        }
        wake(status.flag(), kEventFlagNotEmpty);
        IEffect::Status s{};
        int waited = 0;
        while (status.availableToRead() == 0 && waited < 200) {
            usleep(500);
            ++waited;
        }
        if (!status.read(&s, 1)) {
            ++timeouts;
            continue;
        }
        size_t n = std::min<size_t>(size_t(s.fmqProduced), output.availableToRead());
        n = std::min(n, outBlock.size());
        output.read(outBlock.data(), n);
        size_t copy = std::min(n, out.size() - produced);
        memcpy(&out[produced], outBlock.data(), copy * sizeof(float));
        produced += copy;
        if (at == 0) printf("  first status=%d consumed=%d produced=%d\n", s.status, s.fmqConsumed,
                            s.fmqProduced);
    }
    effect->command(CommandId::STOP);
    effect->close();
    f->destroyEffect(effect);
    double e = 0;
    for (float v : out) e += double(v) * v;
    writeWav(argv[4], out);
    printf("RESULT ok frames=%zu produced_samples=%zu timeouts=%zu pose_fail=%zu out_rms_db=%.2f\n",
           frames, produced, timeouts, poseFail, 10 * log10(e / out.size() + 1e-20));
    return 0;
}
