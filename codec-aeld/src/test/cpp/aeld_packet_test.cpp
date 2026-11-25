#include "aeld_packet.h"

#include <cassert>
#include <cstdlib>
#include <cstring>

using namespace aeld;

namespace {
BtHdr* packets[16];
size_t frames[16];
uint32_t pcm[16];
int count;
bool accept = true;

bool enqueue(BtHdr* packet, size_t n, uint32_t bytes) {
    if (count == 16) {
        std::free(packet);
        return accept;
    }
    packets[count] = packet;
    frames[count] = n;
    pcm[count] = bytes;
    ++count;
    return accept;
}
void* allocate(size_t size) {
    assert(size == kBufferBytes);
    return std::malloc(size);
}
uint32_t timestamp_of(const BtHdr* packet) {
    uint32_t value;
    memcpy(&value, packet + 1, sizeof(value));
    return value;
}
const uint8_t* payload(const BtHdr* packet) {
    return reinterpret_cast<const uint8_t*>(packet + 1) + packet->offset;
}
}  // namespace

int main() {
    uint8_t header[4];
    write_au_header(header, 0x123, 0x2a5);
    assert(header[0] == 0xb1 && header[1] == 0x23 && header[2] == 0x12 && header[3] == 0xa5);

    Packetizer p(1005, allocate, enqueue);
    assert(p.capacity() == 993);
    uint8_t au[400];
    for (size_t i = 0; i < sizeof(au); ++i) au[i] = static_cast<uint8_t>(i);
    // Three AUs per packet; sequence from 1, timestamp +1440 per packet.
    for (int i = 0; i < 6; ++i) assert(p.add(au, 300));
    assert(count == 2);
    for (int k = 0; k < 2; ++k) {
        const BtHdr* packet = packets[k];
        assert(packet->offset == kMediaOffset && packet->len == 3 * 304 &&
               packet->layer_specific == 3 && frames[k] == 3 && pcm[k] == 3 * 1920);
        assert(timestamp_of(packet) == static_cast<uint32_t>(k) * 1440);
        const uint8_t* data = payload(packet);
        for (int a = 0; a < 3; ++a) {
            const uint16_t seq = static_cast<uint16_t>(1 + k * 3 + a);
            assert(data[a * 304] == (0xb0 | (seq >> 8)) && data[a * 304 + 1] == (seq & 0xff));
            assert(data[a * 304 + 2] == 0x11 && data[a * 304 + 3] == 0x2c);
            assert(memcmp(data + a * 304 + 4, au, 300) == 0);
        }
    }
    // An AU that does not fit goes into the next packet: 2 x 404 then 404 > 993 - 808.
    assert(p.add(au, 400) && p.add(au, 400) && count == 2);
    assert(p.add(au, 400) && count == 3 && packets[2]->layer_specific == 2 &&
           timestamp_of(packets[2]) == 2880);
    assert(p.flush() && count == 4 && packets[3]->layer_specific == 1 &&
           timestamp_of(packets[3]) == 2880 + 960);
    assert(p.flush() && count == 4);
    // Limits.
    uint8_t big[kAuSizeMax + 1]{};
    assert(!p.add(big, kAuSizeMax + 1) && !p.add(big, 0));
    Packetizer small(300, allocate, enqueue);
    assert(!small.add(big, 300) && small.add(big, 284));
    // Sequence wraps 4095 -> 0; reset returns to 1.
    for (int i = 0; i < count; ++i) std::free(packets[i]);
    count = 0;
    Packetizer wrap(1005, allocate, enqueue);
    for (int i = 0; i < 4094; ++i) wrap.add(au, 10);
    assert(wrap.sequence() == 4095);
    wrap.add(au, 10);
    assert(wrap.sequence() == 0);
    wrap.reset();
    assert(wrap.sequence() == 1 && wrap.timestamp() == 0);
    // A refused packet is still the stack's.
    for (int i = 0; i < count; ++i) std::free(packets[i]);
    count = 0;
    accept = false;
    Packetizer refused(1005, allocate, enqueue);
    refused.add(au, 10);
    refused.add(au, 10);
    assert(refused.add(au, 10) && count == 1 && refused.sequence() == 4);
    for (int i = 0; i < count; ++i) std::free(packets[i]);
}
