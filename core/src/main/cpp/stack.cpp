// Bluetooth stack adapter: resolves the stack by symbol name when libbluetooth_jni.so loads, adds
// the registered vendor codecs to A2DP codec initialization and vendor dispatch, and restarts the
// audio HAL software session when a codec's PCM rate changes.

#include "port_api.h"

#include <android/log.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <link.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "elf_symbols.h"
#include "registration.h"
#include "vendor_dispatch.h"

namespace port {
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
unsigned int supports_logs;
unsigned int factory_answer_logs;
unsigned int vendor_logs;
DolbyHalStage stage;

bool logs_left(unsigned int* counter) {
    return __atomic_fetch_add(counter, 1U, __ATOMIC_RELAXED) < 32U;
}

bool original_init(void* codecs) { return reinterpret_cast<Init>(init_backup)(codecs); }
bool original_supports(int32_t index) {
    bool result = reinterpret_cast<Supports>(supports_backup)(index);
    if (codec_for_index(index) && logs_left(&supports_logs)) info("provider_supports", result, index);
    return result;
}
void original_construct(void* object, int32_t index, bool source) {
    reinterpret_cast<Construct>(construct_backup)(object, index, source);
}
void construct_base(void* object, const CodecPlugin& codec) {
    // libc++ short string holding the codec name.
    struct ShortString { uint8_t bytes[24]; } name{};
    const size_t length = strnlen(codec.name, sizeof(name.bytes) - 2);
    name.bytes[0] = static_cast<uint8_t>(length * 2);
    memcpy(name.bytes + 1, codec.name, length);
    memset(object, 0, kCodecObjectSize);
    base_construct(object, codec.index, codec.codec_id, &name, codec.priority);
    info("codec_constructed", codec.index, 0);
}
void destroy_base(void* object) { base_destroy(object); }
void free_object(void* object, size_t size) { sized_delete(object, size); }

// Audio HAL software session restart on a PCM rate change. When the stack reconfigures the active
// stream it does not end the HAL session: setup_codec only sends the new PCM config as an update,
// which this audio HAL drops for A2DP software sessions ("ReportAudioConfigChanged invalid
// non-a2dpConfig type"), so the reopened output keeps the old rate (96 kHz PCM into a 48 kHz encoder
// sounds an octave low, the reverse plays fast). A session started afresh carries the right config,
// so after setup_codec the session is ended and started again when the rate changed.
using SessionCall = void (*)();
using SetupCodec = bool (*)(const void*);
void* setup_codec_backup;
void* start_session_backup;
void* end_session_backup;
uint32_t encoder_rate;       // rate reported by the last codec encoder init
bool encoder_rate_pending;   // that init precedes the next setup_codec
uint32_t configured_rate;    // rate of the last setup_codec, 0 when not a registered codec
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
    configured_rate = encoder_rate_pending ? encoder_rate : 0;
    encoder_rate_pending = false;
    if (session_started && session_rate && configured_rate && session_rate != configured_rate) {
        info("session_restart", static_cast<int>(session_rate), static_cast<int>(configured_rate));
        observe_end_session();
        observe_start_session();
    }
    return result;
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
                         system_encoder, system_header, system_timestamp});
int32_t route_index(const uint8_t* codec) {
    int32_t result = dispatch.source_index(codec);
    if (codec_for_info(codec) && logs_left(&vendor_logs)) info("vendor_source_index", result, 0);
    return result;
}
bool route_valid(const uint8_t* codec) {
    bool result = dispatch.source_valid(codec);
    if (codec_for_info(codec) && logs_left(&vendor_logs)) info("vendor_valid", result, codec[9]);
    return result;
}
bool route_init_capability(int32_t index, void* capability) {
    bool result = dispatch.init_capability(index, capability);
    if (codec_for_index(index) && logs_left(&vendor_logs)) info("vendor_sep_capability", result, index);
    return result;
}
const EncoderInterface* route_encoder(const uint8_t* codec) {
    const EncoderInterface* result = dispatch.encoder(codec);
    if (codec_for_info(codec) && logs_left(&vendor_logs)) info("vendor_encoder_interface", result != nullptr, 0);
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
    bool result = initialize_registration(codecs);
    info("codec_init_end", result, 0);
    if (result) {
        for (size_t i = 0; i < codec_count(); ++i) {
            info("codec_enabled", is_supported(codecs, codec_at(i)->index), codec_at(i)->index);
        }
    }
    return result;
}
bool observe_supports(int32_t index) {
    bool result = registration_supports(index);
    if (codec_for_index(index) && logs_left(&factory_answer_logs)) info("factory_answer", result, index);
    return result;
}
void observe_construct(void* object, int32_t index, bool source) {
    registration_construct(object, index, source);
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

// Optional: without these symbols the codecs still work, only a rate change needs a reconnect.
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

// Keeps the codecs whose encoder could be loaded.
size_t bind_codecs(const StackApi& stack) {
    const CodecPlugin* const* list = nullptr;
    size_t count = 0;
    port_codecs(&list, &count);
    const CodecPlugin* usable[kMaxCodecs];
    size_t kept = 0;
    for (size_t i = 0; i < count && kept < kMaxCodecs; ++i) {
        const bool bound = list[i]->bind(stack);
        info("codec_bound", bound, list[i]->index);
        if (bound) usable[kept++] = list[i];
    }
    set_codecs(usable, kept);
    return kept;
}

// Resolves and hooks the Bluetooth JNI image once; runs synchronously when the image is loaded, so
// the hooks are in place before JNI_OnLoad and the stack's codec initialization.
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
    if (bind_codecs({reinterpret_cast<Allocate>(allocate_target), kImage, image.base}) == 0) {
        info("no_codecs", 0, 0);
        return;
    }
    base_construct = reinterpret_cast<BaseConstruct>(base_construct_target);
    base_destroy = reinterpret_cast<BaseDestroy>(base_destroy_target);
    sized_delete = reinterpret_cast<SizedDelete>(sized_delete_target);
    is_supported = reinterpret_cast<IsSupported>(is_supported_target);
    bind_registration({{original_init, original_supports, original_construct, construct_base},
                       destroy_base, free_object});
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
    for (int i = 0; i < 9; ++i) {
        int result = hook(targets[i], replacements[i], backups[i]);
        if (result != 0 || !*backups[i]) {
            if (result == 0) unhook(targets[i]);
            for (int j = i - 1; j >= 0; --j) unhook(targets[j]);
            info("hook_failed", i, 0);
            return;
        }
    }
    info("installed", 9, static_cast<int>(codec_count()));
    install_session_restart(image.base);
}
void library_loaded(const char* name, void*) {
    if (!name) return;
    size_t length = strlen(name), suffix = sizeof("libbluetooth_jni.so") - 1;
    if (length >= suffix && strcmp(name + length - suffix, "libbluetooth_jni.so") == 0) install();
}
}  // namespace

void info(const char* event, int a, int b) {
    __android_log_print(ANDROID_LOG_WARN, "CodecInjector", "stage=module event=%s pid=%d a=%d b=%d",
                        event, getpid(), a, b);
}

DolbyHalStage& spatial() { return stage; }

void note_encoder_rate(uint32_t rate) {
    encoder_rate = rate;
    encoder_rate_pending = true;
}

}  // namespace port

using NativeOnModuleLoaded = void (*)(const char*, void*);
struct NativeAPIEntries {
    uint32_t version;
    port::Hook hook_func;
    port::Unhook unhook_func;
};
extern "C" __attribute__((visibility("default"), used))
NativeOnModuleLoaded native_init(const NativeAPIEntries* api) {
    if (!port::target_process() || !api) return nullptr;
    port::hook = api->hook_func;
    port::unhook = api->unhook_func;
    port::install();  // the image may already be loaded; otherwise library_loaded() installs it
    return port::library_loaded;
}
