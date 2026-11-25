// Feasibility probe: can the FDK AAC encoder built into the Bluetooth stack (libbluetooth_jni.so)
// encode Apple's AAC-ELD profile? Loads the library, resolves the FDK entry points from its
// MiniDebugInfo with the module's own resolver, opens an encoder with the profile used for AirPods
// (AOT 39 ER AAC-ELD, 48 kHz stereo, 480-sample frames, CBR 265 kbps, no SBR, raw access units) and
// encodes a test tone. Run on the device as root.
//
// usage: aeld_fdk_probe [path-to-libbluetooth_jni.so] [bitrate]

#include <dlfcn.h>
#include <link.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aacenc_lib.h"
#include "elf_symbols.h"

namespace {

using Open = AACENC_ERROR (*)(HANDLE_AACENCODER*, UINT, UINT);
using Close = AACENC_ERROR (*)(HANDLE_AACENCODER*);
using SetParam = AACENC_ERROR (*)(HANDLE_AACENCODER, AACENC_PARAM, UINT);
using Encode = AACENC_ERROR (*)(HANDLE_AACENCODER, const AACENC_BufDesc*, const AACENC_BufDesc*,
                                const AACENC_InArgs*, AACENC_OutArgs*);
using Info = AACENC_ERROR (*)(HANDLE_AACENCODER, AACENC_InfoStruct*);

struct Image {
    const char* name;
    uintptr_t base;
};

int find(dl_phdr_info* info, size_t, void* data) {
    auto* image = static_cast<Image*>(data);
    if (info->dlpi_name && strstr(info->dlpi_name, image->name)) {
        image->base = info->dlpi_addr;
        return 1;
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    const char* path = argc > 1 ? argv[1] : "/apex/com.android.bt/lib64/libbluetooth_jni.so";
    const UINT bitrate = argc > 2 ? static_cast<UINT>(atoi(argv[2])) : 265000;
    void* lib = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!lib) {
        printf("RESULT dlopen_failed %s\n", dlerror());
        return 1;
    }
    Image image{"libbluetooth_jni.so", 0};
    dl_iterate_phdr(find, &image);
    void* open_fn = nullptr;
    void* close_fn = nullptr;
    void* set_fn = nullptr;
    void* encode_fn = nullptr;
    void* info_fn = nullptr;
    port::SymbolQuery queries[] = {
        {"aacEncOpen", &open_fn},
        {"aacEncClose", &close_fn},
        {"aacEncoder_SetParam", &set_fn},
        {"aacEncEncode", &encode_fn},
        {"aacEncInfo", &info_fn},
    };
    size_t found = port::resolve_symbols(path, image.base, queries, 5);
    printf("resolved %zu/5 base=0x%lx\n", found, static_cast<unsigned long>(image.base));
    if (found != 5) return 1;
    auto open = reinterpret_cast<Open>(open_fn);
    auto close = reinterpret_cast<Close>(close_fn);
    auto set = reinterpret_cast<SetParam>(set_fn);
    auto encode = reinterpret_cast<Encode>(encode_fn);
    auto info = reinterpret_cast<Info>(info_fn);

    HANDLE_AACENCODER handle = nullptr;
    AACENC_ERROR err = open(&handle, 0x01, 2);
    printf("aacEncOpen -> 0x%x\n", err);
    if (err != AACENC_OK) return 1;
    const struct {
        AACENC_PARAM param;
        UINT value;
        const char* name;
    } params[] = {
        {AACENC_AOT, 39, "AOT=39 (ER AAC-ELD)"},
        {AACENC_SAMPLERATE, 48000, "SAMPLERATE=48000"},
        {AACENC_CHANNELMODE, MODE_2, "CHANNELMODE=2"},
        {AACENC_BITRATE, bitrate, "BITRATE"},
        {AACENC_BITRATEMODE, 0, "BITRATEMODE=CBR"},
        {AACENC_GRANULE_LENGTH, 480, "GRANULE_LENGTH=480"},
        {AACENC_SBR_MODE, 0, "SBR_MODE=off"},
        {AACENC_AFTERBURNER, 0, "AFTERBURNER=0"},
        {AACENC_TRANSMUX, TT_MP4_RAW, "TRANSMUX=raw"},
    };
    for (const auto& p : params) {
        err = set(handle, p.param, p.value);
        printf("  set %-22s -> 0x%x\n", p.name, err);
        if (err != AACENC_OK) return 1;
    }
    err = encode(handle, nullptr, nullptr, nullptr, nullptr);
    printf("init encode -> 0x%x\n", err);
    if (err != AACENC_OK) return 1;
    AACENC_InfoStruct ii{};
    err = info(handle, &ii);
    printf("info -> 0x%x frameLength=%u maxOutBufBytes=%u delay=%u confSize=%u\n", err,
           ii.frameLength, ii.maxOutBufBytes, ii.nDelay, ii.confSize);

    constexpr int kFrames = 480;
    INT_PCM pcm[kFrames * 2];
    UCHAR out[4096];
    long total = 0;
    int encoded = 0, minSize = 1 << 30, maxSize = 0;
    for (int frame = 0; frame < 300; ++frame) {
        for (int i = 0; i < kFrames; ++i) {
            double t = (frame * kFrames + i) / 48000.0;
            INT_PCM v = static_cast<INT_PCM>(12000 * sin(2 * M_PI * 997 * t));
            pcm[2 * i] = v;
            pcm[2 * i + 1] = static_cast<INT_PCM>(8000 * sin(2 * M_PI * 4410 * t));
        }
        void* inPtr = pcm;
        INT inId = IN_AUDIO_DATA, inSize = sizeof(pcm), inEl = sizeof(INT_PCM);
        void* outPtr = out;
        INT outId = OUT_BITSTREAM_DATA, outSize = sizeof(out), outEl = 1;
        AACENC_BufDesc inDesc{1, &inPtr, &inId, &inSize, &inEl};
        AACENC_BufDesc outDesc{1, &outPtr, &outId, &outSize, &outEl};
        AACENC_InArgs inArgs{kFrames * 2, 0};
        AACENC_OutArgs outArgs{};
        err = encode(handle, &inDesc, &outDesc, &inArgs, &outArgs);
        if (err != AACENC_OK) {
            printf("encode frame %d -> 0x%x\n", frame, err);
            return 1;
        }
        if (outArgs.numOutBytes > 0) {
            ++encoded;
            total += outArgs.numOutBytes;
            if (outArgs.numOutBytes < minSize) minSize = outArgs.numOutBytes;
            if (outArgs.numOutBytes > maxSize) maxSize = outArgs.numOutBytes;
        }
    }
    close(&handle);
    printf("RESULT ok frames=%d au_bytes min=%d max=%d avg=%.1f => %.1f kbps (target %u)\n",
           encoded, minSize, maxSize, encoded ? double(total) / encoded : 0.0,
           encoded ? double(total) * 8 / encoded / 0.010 / 1000 : 0.0, bitrate / 1000);
    return 0;
}
