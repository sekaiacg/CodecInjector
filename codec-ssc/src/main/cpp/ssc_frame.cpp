#include "ssc_frame.h"

namespace ssc {

Result Frame::encode(Read read, Encode encode, void* encoder, int32_t bitrate,
                     uint8_t* output, int32_t* output_size) {
    const uint32_t remaining = static_cast<uint32_t>(kPcmBytes - buffered_);
    const uint32_t count = read(pcm_ + buffered_, remaining);
    if (count > remaining) return Result::kReadError;
    buffered_ += count;
    if (buffered_ != kPcmBytes) return Result::kNeedPcm;

    int32_t samples[kSamplesPerChannel * kChannels];
    for (size_t i = 0; i < kSamplesPerChannel * kChannels; ++i) {
        const uint8_t* p = pcm_ + i * 3;
        int32_t value = static_cast<int32_t>(p[0]) |
                        (static_cast<int32_t>(p[1]) << 8) |
                        (static_cast<int32_t>(p[2]) << 16);
        if (value & 0x800000) value -= 0x1000000;
        samples[i] = value;
    }
    const int32_t size = encode(encoder, samples, kSamplesPerChannel, output, bitrate);
    buffered_ = 0;
    if (size <= 0 || size > 4096) return Result::kEncodeError;
    *output_size = size;
    return Result::kEncoded;
}

void Frame::reset() { buffered_ = 0; }

Result Source::tick(uint64_t elapsed_us, Read read, Encode encode, void* encoder,
                    int32_t bitrate, Emit emit, void* emit_context) {
    sample_micros_ += elapsed_us * sample_rate_;
    constexpr uint64_t frame_micros = kSamplesPerChannel * UINT64_C(1000000);
    constexpr size_t max_frames_per_tick = 4;
    Result result = Result::kNeedPcm;
    for (size_t sent = 0; sent < max_frames_per_tick &&
                          sample_micros_ >= frame_micros; ++sent) {
        uint8_t output[4096];
        int32_t output_size = 0;
        result = frame_.encode(read, encode, encoder, bitrate, output, &output_size);
        if (result != Result::kEncoded) return result;
        sample_micros_ -= frame_micros;
        if (!emit(emit_context, output, static_cast<size_t>(output_size), kPcmBytes))
            return Result::kEmitError;
    }
    return result;
}

void Source::reset() {
    frame_.reset();
    sample_micros_ = 0;
}

}  // namespace ssc
