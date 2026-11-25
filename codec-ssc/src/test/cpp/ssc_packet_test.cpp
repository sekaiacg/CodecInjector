#include "ssc_frame.h"
#include "ssc_packet.h"

#include <cassert>
#include <cstdlib>
#include <cstring>

using namespace sscport;

static unsigned enqueue_count;
static bool enqueue_result = true;

static bool enqueue(BtHdr* packet, size_t frames, uint32_t pcm_bytes) {
    assert(packet->event == 0 && packet->len == 484 && packet->offset == 23);
    assert(packet->layer_specific == 0 && frames == 1 && pcm_bytes == kPcmBytes);
    auto* bytes = reinterpret_cast<uint8_t*>(packet + 1);
    uint32_t sequence;
    std::memcpy(&sequence, bytes, sizeof(sequence));
    assert(sequence == ++enqueue_count);
    for (size_t i = 0; i < 484; ++i) assert(bytes[23 + i] == 0x5a);
    std::free(packet);
    return enqueue_result;
}

static uint32_t read_pcm(uint8_t* pcm, uint32_t size) {
    std::memset(pcm, 0, size);
    return size;
}

static int32_t encode(void*, const void*, int32_t samples, uint8_t* output, int32_t) {
    assert(samples == 864);
    std::memset(output, 0x5a, 484);
    return 484;
}

int main() {
    uint8_t frame[484];
    std::memset(frame, 0x5a, sizeof(frame));
    Packetizer packetizer(484, std::malloc, enqueue);
    assert(packetizer.emit(frame, sizeof(frame), kPcmBytes));
    enqueue_result = false;
    assert(!packetizer.emit(frame, sizeof(frame), kPcmBytes));
    assert(enqueue_count == 2);
    Packetizer small_mtu(483, std::malloc, enqueue);
    assert(!small_mtu.emit(frame, sizeof(frame), kPcmBytes));
    assert(enqueue_count == 2);
    enqueue_result = true;
    Packetizer source_packets(484, std::malloc, enqueue);
    enqueue_count = 0;
    Source source(48000);
    assert(source.tick(20000, read_pcm, encode, nullptr, 192000,
                       Packetizer::emit_callback, &source_packets) == Result::kEncoded);
    assert(enqueue_count == 1);
    enqueue_result = false;
    assert(source.tick(20000, read_pcm, encode, nullptr, 192000,
                       Packetizer::emit_callback, &source_packets) == Result::kEmitError);
    assert(enqueue_count == 2);
}
