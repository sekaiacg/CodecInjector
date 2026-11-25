#include "aeld_source.h"

#include <string.h>

namespace aeld {

size_t Source::due(uint64_t now_us) {
    if (!started_) {
        started_ = true;
        last_us_ = now_us;
        carry_us_ = 0;
        return 1;
    }
    if (now_us <= last_us_) {
        last_us_ = now_us;
        return 0;
    }
    const uint64_t elapsed = now_us - last_us_;
    last_us_ = now_us;
    if (elapsed > kMaxElapsedMicros) {
        carry_us_ = 0;
        return 1;
    }
    const uint64_t total = carry_us_ + elapsed;
    carry_us_ = total % kFrameMicros;
    const uint64_t frames = total / kFrameMicros;
    return frames > kMaxFramesPerTick ? kMaxFramesPerTick : static_cast<size_t>(frames);
}

Result Source::tick(uint64_t now_us, Read read, Process process, EncodeFrame encode, void* context,
                    Packetizer* packets) {
    Result result = Result::kIdle;
    for (size_t n = due(now_us); n > 0; --n) {
        const uint32_t remaining = static_cast<uint32_t>(kPcmBytesPerFrame - buffered_);
        const uint32_t count = read(pcm_ + buffered_, remaining);
        if (count > remaining) return Result::kReadError;
        buffered_ += count;
        if (buffered_ != kPcmBytesPerFrame) return Result::kNeedPcm;
        buffered_ = 0;
        int16_t samples[kSamplesPerFrame * 2];
        memcpy(samples, pcm_, sizeof(samples));
        if (process) process(samples, kSamplesPerFrame);
        uint8_t au[kAuSizeMax + 1];
        const int size = encode(context, samples, au, sizeof(au));
        if (size < 0 || static_cast<size_t>(size) > kAuSizeMax) return Result::kEncodeError;
        if (size == 0) continue;
        if (!packets->add(au, static_cast<size_t>(size))) return Result::kSendError;
        result = Result::kSent;
    }
    return result;
}

void Source::reset() {
    buffered_ = 0;
    last_us_ = 0;
    carry_us_ = 0;
    started_ = false;
}

}  // namespace aeld
