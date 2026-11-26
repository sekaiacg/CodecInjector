#include "ssc_packet.h"

#include <string.h>

namespace ssc {

constexpr size_t kBufferBytes = 0x1010;
constexpr uint16_t kMediaOffset = 23;
static_assert(sizeof(BtHdr) == 8);

bool Packetizer::emit(const uint8_t* frame, size_t size, uint32_t pcm_bytes) {
    if (size == 0 || size > mtu_ || size > kBufferBytes - sizeof(BtHdr) - kMediaOffset)
        return false;
    auto* packet = static_cast<BtHdr*>(allocate_(kBufferBytes));
    if (!packet) return false;
    memset(packet, 0, kBufferBytes);
    packet->len = static_cast<uint16_t>(size);
    packet->offset = kMediaOffset;
    auto* bytes = reinterpret_cast<uint8_t*>(packet + 1);
    const uint32_t sequence = ++packet_count_;
    memcpy(bytes, &sequence, sizeof(sequence));
    memcpy(bytes + kMediaOffset, frame, size);
    return enqueue_(packet, 1, pcm_bytes);
}

bool Packetizer::emit_callback(void* context, const uint8_t* frame,
                               size_t size, uint32_t pcm_bytes) {
    return static_cast<Packetizer*>(context)->emit(frame, size, pcm_bytes);
}

}  // namespace ssc
