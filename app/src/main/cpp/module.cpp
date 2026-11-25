#include <android/log.h>
#include <dlfcn.h>
#include <elf.h>
#include <fcntl.h>
#include <link.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <jni.h>
#include "elf_symbols.h"
#include "dolby_hal_stage.h"
#include "ssc_registration.h"
#include "ssc_vendor_dispatch.h"
#include "ssc_negotiation.h"
#include "ssc_codec_state.h"

namespace {
using Hook = int (*)(void*, void*, void**);
using Unhook = int (*)(void*);
using Init = bool (*)(void*);
using Supports = bool (*)(int32_t);
using Construct = void (*)(void*, int32_t, bool);
using BaseConstruct = void (*)(void*, int32_t, uint64_t, const void*, int32_t);
using BaseDestroy = void (*)(void*);
using SizedDelete = void (*)(void*, size_t);
using IsSupported = bool (*)(void*, int32_t);
using namespace sscport;
constexpr char kImage[] = "/apex/com.android.bt/lib64/libbluetooth_jni.so";
Hook hook;
Unhook unhook;
void* init_backup;
void* supports_backup;
void* construct_backup;
void* vendor_index_backup;
void* vendor_valid_backup;
void* vendor_init_backup;
void* vendor_encoder_backup;
void* header_backup;
void* timestamp_backup;
BaseConstruct base_construct;
BaseDestroy base_destroy;
SizedDelete sized_delete;
IsSupported is_supported;
bool registration_enabled;
bool software_enabled;
unsigned int supports_logs;
unsigned int factory_answer_logs;
unsigned int construct_logs;
unsigned int vendor_logs;
unsigned int read_logs;
unsigned int encode_logs;
unsigned int enqueue_logs;
Read system_read;
Enqueue system_enqueue;
Encode real_encode;
const EncoderInterface* underlying_encoder;
EncoderInterface observed_encoder{};

void info(const char* event, int a, int b) {
    __android_log_print(ANDROID_LOG_WARN, "SscPort",
                        "stage=module event=%s pid=%d a=%d b=%d",
                        event, getpid(), a, b);
}

bool original_init(void* codecs) { return reinterpret_cast<Init>(init_backup)(codecs); }
bool original_supports(int32_t index) {
    bool result = reinterpret_cast<Supports>(supports_backup)(index);
    if (index == 18 && __atomic_fetch_add(&supports_logs, 1U, __ATOMIC_RELAXED) < 32U)
        info("provider_index18", result, 0);
    return result;
}
void original_construct(void* object, int32_t index, bool source) {
    reinterpret_cast<Construct>(construct_backup)(object, index, source);
}
void construct_ssc_base(void* object) {
    struct ShortString { uint8_t bytes[24]; } name{};
    name.bytes[0] = 6;
    memcpy(name.bytes + 1, "SSC", 3);
    memset(object, 0, 0x218);
    // The software experiment needs SSC selected on a fresh connection so
    // that the system starts its software session naturally.
    base_construct(object, 18, UINT64_C(0x00000001030075ff), &name,
                   software_enabled ? 1000000 : 1);
    info("ssc_base_constructed", 18, 0);
}
void destroy_ssc_base(void* object) { base_destroy(object); }
void free_ssc_object(void* object, size_t size) { sized_delete(object, size); }

uint32_t observe_pcm_read(uint8_t* output, uint32_t size) {
    uint32_t result = system_read(output, size);
    if (__atomic_fetch_add(&read_logs, 1U, __ATOMIC_RELAXED) < 3U)
        info("ssc_pcm_read", static_cast<int>(result), static_cast<int>(size));
    return result;
}
DolbyHalStage dolby;
unsigned int spatial_logs;
int32_t logged_bitrate;

int32_t observe_encode(void* encoder, const void* pcm, int32_t samples,
                       uint8_t* output, int32_t bitrate) {
    int32_t rendered[kSamplesPerChannel * kChannels];
    if (samples == static_cast<int32_t>(kSamplesPerChannel)) {
        memcpy(rendered, pcm, sizeof(rendered));
        dolby.process(rendered, kSamplesPerChannel);
        pcm = rendered;
        if (__atomic_fetch_add(&spatial_logs, 1U, __ATOMIC_RELAXED) < 1U)
            info("spatial_process", dolby.running(), samples);
    }
    if (bitrate != logged_bitrate) {
        logged_bitrate = bitrate;
        info("ssc_bitrate", bitrate, samples);
    }
    int32_t result = real_encode(encoder, pcm, samples, output, bitrate);
    if (__atomic_fetch_add(&encode_logs, 1U, __ATOMIC_RELAXED) < 3U)
        info("ssc_encode", result, samples);
    return result;
}
bool observe_enqueue(BtHdr* packet, size_t frames, uint32_t pcm_bytes) {
    bool result = system_enqueue(packet, frames, pcm_bytes);
    if (__atomic_fetch_add(&enqueue_logs, 1U, __ATOMIC_RELAXED) < 3U)
        info("ssc_enqueue", result, static_cast<int>(frames));
    return result;
}
// Audio HAL software session restart on an SSC sample-rate change. When the stack reconfigures the
// active stream it does not end the HAL session: setup_codec only sends the new PCM config as an
// update, which this audio HAL drops for A2DP software sessions ("ReportAudioConfigChanged invalid
// non-a2dpConfig type"), so the reopened output keeps the old rate (96 kHz PCM into a 48 kHz
// encoder sounds an octave low, the reverse plays fast). A session started afresh carries the
// right config, so after setup_codec the session is ended and started again when the rate changed.
using SessionCall = void (*)();
using SetupCodec = bool (*)(const void*);
void* setup_codec_backup;
void* start_session_backup;
void* end_session_backup;
uint32_t ssc_rate;           // rate of the last SSC encoder init
bool ssc_setup_pending;      // that init precedes the next setup_codec
uint32_t configured_rate;    // rate of the last setup_codec, 0 when not SSC
uint32_t session_rate;       // rate the running HAL session was started with, 0 when unknown
bool session_started;

void observe_start_session() {
    reinterpret_cast<SessionCall>(start_session_backup)();
    session_started = true;
    session_rate = configured_rate;
}
void observe_end_session() {
    reinterpret_cast<SessionCall>(end_session_backup)();
    session_started = false;
    session_rate = 0;
}
bool observe_setup_codec(const void* config) {
    bool result = reinterpret_cast<SetupCodec>(setup_codec_backup)(config);
    configured_rate = ssc_setup_pending ? ssc_rate : 0;
    ssc_setup_pending = false;
    if (session_started && session_rate && configured_rate && session_rate != configured_rate) {
        info("session_restart", static_cast<int>(session_rate), static_cast<int>(configured_rate));
        observe_end_session();
        observe_start_session();
    }
    return result;
}

void observe_encoder_init(const PeerParams* peer, void* codec, Read read, Enqueue enqueue) {
    system_read = read;
    system_enqueue = enqueue;
    read_logs = encode_logs = enqueue_logs = spatial_logs = 0;
    logged_bitrate = 0;
    const bool uhq = codec && sscport::codec_is_uhq(codec);
    info("dolby_started", dolby.start(uhq ? 96000 : 48000, kSamplesPerChannel), uhq);
    info("ssc_encoder_init", peer->mtu, uhq ? 96000 : 48000);
    ssc_rate = uhq ? 96000 : 48000;
    ssc_setup_pending = true;
    underlying_encoder->init(peer, codec, observe_pcm_read, observe_enqueue);
}
void observe_encoder_cleanup() {
    dolby.stop();
    underlying_encoder->cleanup();
    info("ssc_encoder_cleanup", 0, 0);
}

int32_t system_index(const uint8_t* codec) {
    return reinterpret_cast<int32_t (*)(const uint8_t*)>(vendor_index_backup)(codec);
}
bool system_valid(const uint8_t* codec) {
    return reinterpret_cast<bool (*)(const uint8_t*)>(vendor_valid_backup)(codec);
}
bool system_init_capability(int32_t index, void* capability) {
    return reinterpret_cast<bool (*)(int32_t, void*)>(vendor_init_backup)(index, capability);
}
const EncoderInterface* system_encoder(const uint8_t* codec) {
    return reinterpret_cast<const EncoderInterface* (*)(const uint8_t*)>(vendor_encoder_backup)(codec);
}
bool system_header(const uint8_t* codec, BtHdr* packet, uint16_t frames) {
    return reinterpret_cast<bool (*)(const uint8_t*, BtHdr*, uint16_t)>(header_backup)(codec, packet, frames);
}
bool system_timestamp(const uint8_t* codec, const uint8_t* data, uint32_t* timestamp) {
    return reinterpret_cast<bool (*)(const uint8_t*, const uint8_t*, uint32_t*)>(timestamp_backup)(codec, data, timestamp);
}
VendorDispatch dispatch({system_index, system_valid, system_init_capability,
                         system_encoder, system_header, system_timestamp}, nullptr);
int32_t route_index(const uint8_t* codec) {
    int32_t result = dispatch.source_index(codec);
    if (is_ssc_codec(codec) && __atomic_fetch_add(&vendor_logs, 1U, __ATOMIC_RELAXED) < 32U)
        info("ssc_source_index", result, 0);
    return result;
}
bool route_valid(const uint8_t* codec) {
    bool result = dispatch.source_valid(codec);
    if (is_ssc_codec(codec) && __atomic_fetch_add(&vendor_logs, 1U, __ATOMIC_RELAXED) < 32U)
        info("ssc_vendor_valid", result, codec[9]);
    return result;
}
bool route_init_capability(int32_t index, void* capability) {
    bool result = dispatch.init_capability(index, capability);
    if (index == 18 && __atomic_fetch_add(&vendor_logs, 1U, __ATOMIC_RELAXED) < 32U)
        info("ssc_sep_capability", result, 0);
    return result;
}
const EncoderInterface* route_encoder(const uint8_t* codec) {
    const EncoderInterface* result = dispatch.encoder(codec);
    if (is_ssc_codec(codec) && __atomic_fetch_add(&vendor_logs, 1U, __ATOMIC_RELAXED) < 32U)
        info("ssc_encoder_interface", result != nullptr, 0);
    return result;
}
bool route_header(const uint8_t* codec, BtHdr* packet, uint16_t frames) {
    return dispatch.build_header(codec, packet, frames);
}
bool route_timestamp(const uint8_t* codec, const uint8_t* data, uint32_t* timestamp) {
    return dispatch.packet_timestamp(codec, data, timestamp);
}

bool observe_init(void* codecs) {
    info("codec_init_begin", 0, 0);
    bool result = registration_enabled
            ? sscport::initialize_registration(codecs) : original_init(codecs);
    info("codec_init_end", result, 0);
    if (registration_enabled && result)
        info("ssc_enabled_map", is_supported(codecs, 18), 0);
    return result;
}
bool observe_supports(int32_t index) {
    bool result = registration_enabled
            ? sscport::registration_supports(index) : original_supports(index);
    if (registration_enabled && index == 18 &&
        __atomic_fetch_add(&factory_answer_logs, 1U, __ATOMIC_RELAXED) < 32U)
        info("factory_answer_index18", result, 0);
    return result;
}
void observe_construct(void* object, int32_t index, bool source) {
    if (index == 18 && __atomic_fetch_add(&construct_logs, 1U, __ATOMIC_RELAXED) < 32U)
        info("extension_index18", source, 0);
    if (registration_enabled) sscport::registration_construct(object, index, source);
    else original_construct(object, index, source);
}

struct Image {
    bool found;
    uintptr_t base;
};
int find_image(dl_phdr_info* entry, size_t, void* context) {
    if (!entry->dlpi_name || strcmp(entry->dlpi_name, kImage) != 0) return 0;
    auto* image = static_cast<Image*>(context);
    image->found = true;
    image->base = entry->dlpi_addr;
    return 1;
}
bool target_process() {
    char cmdline[64]{};
    int fd = open("/proc/self/cmdline", O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;
    ssize_t count = read(fd, cmdline, sizeof(cmdline) - 1);
    close(fd);
    return count > 0 && strcmp(cmdline, "com.android.bluetooth") == 0;
}
// Optional: without these symbols SSC still works, only a rate change needs a reconnect.
void install_session_restart(uintptr_t base) {
    void* setup_target = nullptr;
    void* start_target = nullptr;
    void* end_target = nullptr;
    SymbolQuery queries[] = {
        {"_ZN9bluetooth5audio4a2dp11setup_codecERKNS1_24ahal_codec_configurationE", &setup_target},
        {"_ZN9bluetooth5audio4a2dp13start_sessionEv", &start_target},
        {"_ZN9bluetooth5audio4a2dp11end_sessionEv", &end_target},
    };
    constexpr size_t kCount = sizeof(queries) / sizeof(queries[0]);
    if (resolve_symbols(kImage, base, queries, kCount) != kCount) {
        info("session_restart_unavailable", 0, 0);
        return;
    }
    void* targets[] = {start_target, end_target, setup_target};
    void* replacements[] = {reinterpret_cast<void*>(&observe_start_session),
                            reinterpret_cast<void*>(&observe_end_session),
                            reinterpret_cast<void*>(&observe_setup_codec)};
    void** backups[] = {&start_session_backup, &end_session_backup, &setup_codec_backup};
    for (size_t i = 0; i < kCount; ++i) {
        if (hook(targets[i], replacements[i], backups[i]) != 0 || !*backups[i]) {
            for (size_t j = 0; j < i; ++j) unhook(targets[j]);
            info("session_restart_hook_failed", static_cast<int>(i), 0);
            return;
        }
    }
    info("session_restart_installed", 3, 0);
}

// Resolves and hooks the Bluetooth JNI image once; runs synchronously when the image is loaded,
// so the hooks are in place before JNI_OnLoad and the stack's codec initialization.
void install() {
    static bool attempted;
    if (attempted) return;
    Image image{};
    dl_iterate_phdr(find_image, &image);
    if (!image.found) return;
    attempted = true;
    void* init_target = nullptr;
    void* supports_target = nullptr;
    void* construct_target = nullptr;
    void* vendor_index_target = nullptr;
    void* vendor_valid_target = nullptr;
    void* vendor_init_target = nullptr;
    void* vendor_encoder_target = nullptr;
    void* header_target = nullptr;
    void* timestamp_target = nullptr;
    void* base_construct_target = nullptr;
    void* base_destroy_target = nullptr;
    void* sized_delete_target = nullptr;
    void* is_supported_target = nullptr;
    void* allocate_target = nullptr;
    SymbolQuery queries[] = {
        {"_ZN10A2dpCodecs4initEv", &init_target},
        {"_ZN9bluetooth5audio4a2dp8provider14supports_codecE23btav_a2dp_codec_index_t",
         &supports_target},
        {"_ZN18A2dpCodecConfigExtC1E23btav_a2dp_codec_index_tb", &construct_target},
        {"_Z27A2DP_VendorSourceCodecIndexPKh", &vendor_index_target},
        {"_Z29A2DP_IsVendorSourceCodecValidPKh", &vendor_valid_target},
        {"_Z26A2DP_VendorInitCodecConfig23btav_a2dp_codec_index_tP14AvdtpSepConfig",
         &vendor_init_target},
        {"_Z30A2DP_VendorGetEncoderInterfacePKh", &vendor_encoder_target},
        {"_Z21A2DP_BuildCodecHeaderPKhP6BT_HDRt", &header_target},
        {"_Z23A2DP_GetPacketTimestampPKhS0_Pj", &timestamp_target},
        {"_ZN15A2dpCodecConfigC2E23btav_a2dp_codec_index_tN9bluetooth4a2dp7CodecIdERKNSt3__1"
         "12basic_stringIcNS4_11char_traitsIcEENS4_9allocatorIcEEEE26btav_a2dp_codec_priority_t",
         &base_construct_target},
        {"_ZN15A2dpCodecConfigD2Ev", &base_destroy_target},
        {"_ZdlPvm", &sized_delete_target},
        {"_ZN10A2dpCodecs16isSupportedCodecE23btav_a2dp_codec_index_t", &is_supported_target},
        {"_Z10osi_mallocm", &allocate_target},
    };
    constexpr size_t kQueries = sizeof(queries) / sizeof(queries[0]);
    size_t resolved = resolve_symbols(kImage, image.base, queries, kQueries);
    for (size_t i = 0; i < kQueries; ++i)
        if (!*queries[i].address) info("symbol_missing", static_cast<int>(i), 0);
    if (resolved != kQueries) return;
    registration_enabled = true;
    software_enabled = true;
    if (registration_enabled) {
        base_construct = reinterpret_cast<BaseConstruct>(base_construct_target);
        base_destroy = reinterpret_cast<BaseDestroy>(base_destroy_target);
        sized_delete = reinterpret_cast<SizedDelete>(sized_delete_target);
        is_supported = reinterpret_cast<IsSupported>(is_supported_target);
        sscport::bind_registration({
            {original_init, original_supports, original_construct, construct_ssc_base},
            destroy_ssc_base, free_ssc_object});
    }
    if (software_enabled) {
        void* library = dlopen("libScalable_Encoder.so", RTLD_NOW | RTLD_LOCAL);
        auto allocate = reinterpret_cast<Allocate>(allocate_target);
        auto create = library ? reinterpret_cast<Create>(dlsym(library, "ssc_encoder_create")) : nullptr;
        auto destroy = library ? reinterpret_cast<Destroy>(dlsym(library, "ssc_encoder_destroy")) : nullptr;
        auto encode = library ? reinterpret_cast<Encode>(dlsym(library, "ssc_encode")) : nullptr;
        if (!allocate || !create || !destroy || !encode) {
            if (library) dlclose(library);
            info("software_dependencies_unavailable", 0, 0);
            return;
        }
        real_encode = encode;
        underlying_encoder = bind({create, destroy, observe_encode, allocate});
        observed_encoder = *underlying_encoder;
        observed_encoder.init = observe_encoder_init;
        observed_encoder.cleanup = observe_encoder_cleanup;
        dispatch = VendorDispatch({system_index, system_valid, system_init_capability,
                                   system_encoder, system_header, system_timestamp},
                                  &observed_encoder);
    }
    void* targets[] = {supports_target,      construct_target,      vendor_index_target,
                       vendor_valid_target,  vendor_init_target,    vendor_encoder_target,
                       header_target,        timestamp_target,      init_target};
    void* replacements[] = {reinterpret_cast<void*>(&observe_supports),
                            reinterpret_cast<void*>(&observe_construct),
                            reinterpret_cast<void*>(&route_index),
                            reinterpret_cast<void*>(&route_valid),
                            reinterpret_cast<void*>(&route_init_capability),
                            reinterpret_cast<void*>(&route_encoder),
                            reinterpret_cast<void*>(&route_header),
                            reinterpret_cast<void*>(&route_timestamp),
                            reinterpret_cast<void*>(&observe_init)};
    void** backups[] = {&supports_backup, &construct_backup, &vendor_index_backup,
                        &vendor_valid_backup, &vendor_init_backup, &vendor_encoder_backup,
                        &header_backup, &timestamp_backup, &init_backup};
    bool active[] = {true, true, software_enabled, software_enabled, software_enabled,
                     software_enabled, software_enabled, software_enabled, true};
    for (int i = 0; i < 9; ++i) {
        if (!active[i]) continue;
        int result = hook(targets[i], replacements[i], backups[i]);
        if (result != 0 || !*backups[i]) {
            if (result == 0) unhook(targets[i]);
            for (int j = i - 1; j >= 0; --j) if (active[j]) unhook(targets[j]);
            info("hook_failed", i, 0);
            return;
        }
    }
    info("installed", software_enabled ? 9 : 3, registration_enabled);
    install_session_restart(image.base);
}
void library_loaded(const char* name, void*) {
    if (!name) return;
    size_t length = strlen(name), suffix = sizeof("libbluetooth_jni.so") - 1;
    if (length >= suffix && strcmp(name + length - suffix, "libbluetooth_jni.so") == 0) install();
}
}  // namespace

extern "C" __attribute__((visibility("default"), used)) JNIEXPORT void JNICALL
Java_io_github_wxxsfxyzm_galaxybudsport_SscPortModule_nativeSetSpatial(JNIEnv*, jclass, jboolean on) {
    dolby.set_enabled(on);
    info("spatial_enabled", on, 0);
}

extern "C" __attribute__((visibility("default"), used)) JNIEXPORT void JNICALL
Java_io_github_wxxsfxyzm_galaxybudsport_SscPortModule_nativeSetYaw(JNIEnv*, jclass, jfloat radians) {
    dolby.set_yaw(radians);
}

using NativeOnModuleLoaded = void (*)(const char*, void*);
struct NativeAPIEntries { uint32_t version; Hook hook_func; Unhook unhook_func; };
extern "C" __attribute__((visibility("default"), used))
NativeOnModuleLoaded native_init(const NativeAPIEntries* api) {
    if (!target_process() || !api) return nullptr;
    hook = api->hook_func;
    unhook = api->unhook_func;
    install();  // the image may already be loaded; otherwise library_loaded() installs on load
    return library_loaded;
}

extern "C" __attribute__((visibility("default"), used)) JNIEXPORT void JNICALL
Java_io_github_wxxsfxyzm_galaxybudsport_SscPortModule_nativeSetUhq(JNIEnv*, jclass, jboolean preferred) {
    sscport::set_uhq_preferred(preferred);
    info("uhq_preferred", preferred, 0);
}

extern "C" __attribute__((visibility("default"), used)) JNIEXPORT void JNICALL
Java_io_github_wxxsfxyzm_galaxybudsport_SscPortModule_nativeSetQualityMode(JNIEnv*, jclass, jint mode) {
    sscport::set_quality_mode(static_cast<uint32_t>(mode));
    info("quality_mode", mode, 0);
}
