#include "aeld_source.h"

#include <cassert>
#include <cstdlib>
#include <cstring>

using namespace aeld;

namespace {
uint32_t available = UINT32_MAX;
uint32_t read_bytes;
int processed;
int encoded;
int sent;
int16_t first_sample;

uint32_t read(uint8_t* out, uint32_t size) {
    const uint32_t n = size < available ? size : available;
    for (uint32_t i = 0; i < n; ++i) out[i] = static_cast<uint8_t>(read_bytes + i);
    read_bytes += n;
    if (available != UINT32_MAX) available -= n;
    return n;
}
void process(int16_t* pcm, size_t frames) {
    assert(frames == kSamplesPerFrame);
    ++processed;
    first_sample = pcm[0];
}
int encode(void*, const int16_t*, uint8_t* out, size_t capacity) {
    assert(capacity > kAuSizeMax);
    ++encoded;
    memset(out, 1, 331);
    return 331;
}
bool enqueue(BtHdr* packet, size_t frames, uint32_t) {
    sent += static_cast<int>(frames);
    std::free(packet);
    return true;
}
}  // namespace

int main() {
    Source source;
    Packetizer packets(1200, std::malloc, enqueue);
    // First tick: one frame; then one per 10 ms, the remainder carried.
    assert(source.tick(1000000, read, process, encode, nullptr, &packets) == Result::kSent);
    assert(encoded == 1 && processed == 1);
    for (uint64_t t = 1000000 + 7000; t <= 2000000; t += 7000)
        source.tick(t, read, process, encode, nullptr, &packets);
    assert(encoded == 1 + 99 && sent == 99);  // 0.994 s of 7 ms ticks: 99 frames, 3 per packet
    // A long gap restarts with one frame; a burst is capped at eight.
    source.tick(3000000, read, process, encode, nullptr, &packets);
    assert(encoded == 101);
    source.tick(3150000, read, process, encode, nullptr, &packets);
    assert(encoded == 109);
    // Short read: the partial frame is kept.
    available = 1000;
    assert(source.tick(3160000, read, process, encode, nullptr, &packets) == Result::kNeedPcm);
    assert(encoded == 109);
    available = UINT32_MAX;
    const uint32_t before = read_bytes;
    assert(source.tick(3170000, read, process, encode, nullptr, &packets) == Result::kSent);
    assert(encoded == 110 && read_bytes - before == kPcmBytesPerFrame - 1000);
    // Clock going back: nothing this tick.
    assert(source.tick(3100000, read, process, encode, nullptr, &packets) == Result::kIdle);
    // An MTU of 1005 holds two 331-byte AUs with their headers, not three (1005 + 12 RTP bytes).
    Source narrow;
    Packetizer two(1005, std::malloc, enqueue);
    sent = 0;
    for (uint64_t t = 0; t <= 100000; t += 10000) narrow.tick(t + 1, read, nullptr, encode, nullptr, &two);
    assert(sent == 10 && two.flush() && sent == 11);
    source.reset();
    assert(source.due(5000000) == 1 && source.due(5009999) == 0 && source.due(5010000) == 1);
}
