#pragma once

#include <stddef.h>
#include <stdint.h>

namespace sscport {

constexpr size_t kSamplesPerChannel = 864;
constexpr size_t kChannels = 2;
constexpr size_t kPcmBytes = kSamplesPerChannel * kChannels * 3;

// The caller owns scheduling, the encoder, and a sufficiently large output
// buffer: the private encoder API does not accept an output capacity.
using Read = uint32_t (*)(uint8_t*, uint32_t);
using Encode = int32_t (*)(void*, const void*, int32_t, uint8_t*, int32_t);

enum class Result { kNeedPcm, kEncoded, kReadError, kEncodeError, kEmitError };

class Frame {
public:
    Result encode(Read read, Encode encode, void* encoder, int32_t bitrate,
                  uint8_t* output, int32_t* output_size);
    void reset();

private:
    uint8_t pcm_[kPcmBytes]{};
    size_t buffered_ = 0;
};

// Called by the existing source timer; this class does not schedule callbacks.
// The caller's emit function builds the SSC packet and passes it to the stock
// enqueue callback. Return false to stop processing the current tick.
// A delayed callback sends at most four frames; remaining time is carried
// into later ticks. A short read returns immediately and retains partial PCM.
using Emit = bool (*)(void*, const uint8_t*, size_t, uint32_t);

class Source {
public:
    explicit Source(uint32_t sample_rate) : sample_rate_(sample_rate) {}
    void set_sample_rate(uint32_t sample_rate) { sample_rate_ = sample_rate; }
    Result tick(uint64_t elapsed_us, Read read, Encode encode, void* encoder,
                int32_t bitrate, Emit emit, void* emit_context);
    void reset();

private:
    Frame frame_;
    uint32_t sample_rate_;
    uint64_t sample_micros_ = 0;
};

}  // namespace sscport
