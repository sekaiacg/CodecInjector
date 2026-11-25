#include "ssc_frame.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>

using namespace sscport;

static uint8_t input[kPcmBytes];
static size_t offset;
static size_t available;
static int calls;
static int emitted;

static uint32_t read_pcm(uint8_t* out, uint32_t requested) {
    const auto n = std::min<size_t>(requested, available);
    std::memcpy(out, input + offset, n);
    offset += n;
    available -= n;
    return static_cast<uint32_t>(n);
}

static int32_t encode_frame(void*, const void* pcm, int32_t samples,
                            uint8_t* output, int32_t bitrate) {
    const auto* data = static_cast<const int32_t*>(pcm);
    assert(samples == 864 && bitrate == 192000);
    assert(data[0] == -8388608 && data[1] == 8388607);
    ++calls;
    output[0] = 42;
    return 1;
}

static bool emit_frame(void*, const uint8_t* payload, size_t size, uint32_t pcm_bytes) {
    assert(size == 1 && payload[0] == 42 && pcm_bytes == kPcmBytes);
    ++emitted;
    return true;
}

int main() {
    input[2] = 0x80;
    input[3] = 0xff;
    input[4] = 0xff;
    input[5] = 0x7f;
    Frame frame;
    uint8_t output[1024]{};
    int32_t size = 0;
    available = 100;
    assert(frame.encode(read_pcm, encode_frame, nullptr, 192000, output, &size) == Result::kNeedPcm);
    assert(frame.encode(read_pcm, encode_frame, nullptr, 192000, output, &size) == Result::kNeedPcm);
    assert(calls == 0);
    available = kPcmBytes - 100;
    assert(frame.encode(read_pcm, encode_frame, nullptr, 192000, output, &size) == Result::kEncoded);
    assert(calls == 1 && size == 1 && output[0] == 42);
    offset = 0;
    available = 3;
    assert(frame.encode(read_pcm, encode_frame, nullptr, 192000, output, &size) == Result::kNeedPcm);
    frame.reset();
    offset = 0;
    available = kPcmBytes;
    assert(frame.encode(read_pcm, encode_frame, nullptr, 192000, output, &size) == Result::kEncoded);
    assert(calls == 2);

    Source source(48000);
    offset = 0;
    available = kPcmBytes;
    assert(source.tick(10000, read_pcm, encode_frame, nullptr, 192000, emit_frame, nullptr) == Result::kNeedPcm);
    assert(emitted == 0);
    assert(source.tick(10000, read_pcm, encode_frame, nullptr, 192000, emit_frame, nullptr) == Result::kEncoded);
    assert(emitted == 1);

    source.reset();
    offset = 0;
    available = 100;
    assert(source.tick(20000, read_pcm, encode_frame, nullptr, 192000, emit_frame, nullptr) == Result::kNeedPcm);
    available = kPcmBytes - 100;
    assert(source.tick(0, read_pcm, encode_frame, nullptr, 192000, emit_frame, nullptr) == Result::kEncoded);
    assert(emitted == 2);

    Source uhq_source(96000);
    offset = 0;
    available = kPcmBytes;
    assert(uhq_source.tick(10000, read_pcm, encode_frame, nullptr, 192000, emit_frame, nullptr) == Result::kEncoded);
    assert(emitted == 3);
}
