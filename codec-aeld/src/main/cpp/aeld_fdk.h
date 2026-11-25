#pragma once

// The FDK AAC encoder that the Bluetooth stack image already contains (its AAC codec uses it),
// resolved by symbol name, configured for the profile Apple sources send to AirPods:
// AOT 39 (ER AAC-ELD), 48 kHz stereo, 480-sample frames, CBR 265 kbps, no SBR, no afterburner,
// raw access units. 512-sample frames make AirPods stop after about 0.1 s, and SBR is unstable.
// The ABI below is FDK's public aacenc_lib.h interface.

#include <stddef.h>
#include <stdint.h>

namespace aeld {

namespace fdk {
using Handle = void*;
using Error = uint32_t;
struct BufDesc {
    int32_t count;
    void** buffers;
    int32_t* identifiers;
    int32_t* sizes;
    int32_t* element_sizes;
};
struct InArgs {
    int32_t samples;
    int32_t ancillary_bytes;
};
struct OutArgs {
    int32_t bytes;
    int32_t samples;
    int32_t ancillary_bytes;
    int32_t reservoir_bits;
};
using Open = Error (*)(Handle*, uint32_t modules, uint32_t channels);
using Close = Error (*)(Handle*);
using SetParam = Error (*)(Handle, uint32_t param, uint32_t value);
using Encode = Error (*)(Handle, const BufDesc*, const BufDesc*, const InArgs*, OutArgs*);

constexpr uint32_t kAot = 0x0100;
constexpr uint32_t kBitrate = 0x0101;
constexpr uint32_t kBitrateMode = 0x0102;
constexpr uint32_t kSampleRate = 0x0103;
constexpr uint32_t kSbrMode = 0x0104;
constexpr uint32_t kGranuleLength = 0x0105;
constexpr uint32_t kChannelMode = 0x0106;
constexpr uint32_t kAfterburner = 0x0200;
constexpr uint32_t kTransmux = 0x0300;
constexpr int32_t kInAudioData = 0;
constexpr int32_t kOutBitstreamData = 3;
}  // namespace fdk

struct FdkApi {
    fdk::Open open;
    fdk::Close close;
    fdk::SetParam set_param;
    fdk::Encode encode;
};

constexpr uint32_t kEncoderBitrate = 265000;
constexpr uint32_t kFrameSamples = 480;

// Resolves the FDK entry points from the stack image; false when any is missing.
bool resolve_fdk(const char* image, uintptr_t base, FdkApi* api);

class EldEncoder {
public:
    void bind(const FdkApi& api) { api_ = api; }
    bool open();
    void close();
    bool is_open() const { return handle_ != nullptr; }
    // Encodes one 480-sample interleaved stereo frame; returns the AU size, 0 when the encoder
    // produced no AU for it, or -1 on failure.
    int encode(const int16_t* pcm, uint8_t* out, size_t capacity);
    uint32_t last_error() const { return error_; }

private:
    FdkApi api_{};
    fdk::Handle handle_ = nullptr;
    uint32_t error_ = 0;
};

}  // namespace aeld
