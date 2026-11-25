#pragma once

// Paces AAC-ELD frames from the stack's timer: each call produces the 10 ms frames that elapsed
// (carrying the remainder), at most eight; after a gap over 200 ms (pause, suspend) the backlog is
// dropped and the stream restarts with one frame instead of a burst that would drain the audio
// HAL again. A short PCM read keeps the partial frame for the next call.

#include <stddef.h>
#include <stdint.h>

#include "a2dp_types.h"
#include "aeld_packet.h"

namespace aeld {

using port::Read;

constexpr size_t kMaxFramesPerTick = 8;
constexpr uint64_t kFrameMicros = 10000;
constexpr uint64_t kMaxElapsedMicros = 200000;

// One frame of interleaved stereo 16-bit PCM in place before encoding (spatial rendering).
using Process = void (*)(int16_t* pcm, size_t frames);
// Encodes one frame; returns the AU size, 0 for no AU, or -1 on failure.
using EncodeFrame = int (*)(void* context, const int16_t* pcm, uint8_t* out, size_t capacity);

enum class Result { kIdle, kNeedPcm, kSent, kReadError, kEncodeError, kSendError };

class Source {
public:
    // Frames due at this timer tick.
    size_t due(uint64_t now_us);
    Result tick(uint64_t now_us, Read read, Process process, EncodeFrame encode, void* context,
                Packetizer* packets);
    void reset();

private:
    uint8_t pcm_[kPcmBytesPerFrame]{};
    size_t buffered_ = 0;
    uint64_t last_us_ = 0;
    uint64_t carry_us_ = 0;
    bool started_ = false;
};

}  // namespace aeld
