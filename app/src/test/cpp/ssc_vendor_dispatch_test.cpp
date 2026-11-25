#include "ssc_vendor_dispatch.h"
#include "ssc_negotiation.h"

#include <cassert>
#include <cstring>

using namespace sscport;

namespace {
int calls[6]{};
EncoderInterface system_encoder{};
int32_t system_index(const uint8_t*) { ++calls[0]; return 6; }
bool system_valid(const uint8_t*) { ++calls[1]; return false; }
bool system_init(int32_t index, void*) { ++calls[2]; return index == 6; }
const EncoderInterface* system_get_encoder(const uint8_t*) {
    ++calls[3];
    return &system_encoder;
}
bool system_header(const uint8_t*, BtHdr* packet, uint16_t frames) {
    ++calls[4];
    packet->layer_specific = frames;
    return false;
}
bool system_timestamp(const uint8_t*, const uint8_t*, uint32_t* timestamp) {
    ++calls[5];
    *timestamp = 99;
    return false;
}
}

int main() {
    EncoderInterface ssc_encoder{};
    VendorDispatch dispatch({system_index, system_valid, system_init,
                             system_get_encoder, system_header, system_timestamp},
                            &ssc_encoder);
    uint8_t ssc[45];
    std::memset(ssc, 0xa5, sizeof(ssc));
    assert(dispatch.init_capability(18, ssc));
    assert(ssc[9] == 0x0c && ssc[44] == 0);
    assert(dispatch.source_index(ssc) == 18);
    assert(dispatch.source_valid(ssc));
    assert(dispatch.encoder(ssc) == &ssc_encoder);
    BtHdr packet{1, 2, 3, 4};
    BtHdr before = packet;
    assert(dispatch.build_header(ssc, &packet, 1));
    assert(std::memcmp(&packet, &before, sizeof(packet)) == 0);
    uint32_t input_timestamp = 42;
    uint32_t timestamp = 0;
    assert(dispatch.packet_timestamp(ssc, reinterpret_cast<uint8_t*>(&input_timestamp),
                                     &timestamp));
    assert(timestamp == 42);
    ssc[9] = 0x3e;
    assert(dispatch.source_valid(ssc));
    ssc[9] = 0x0a;
    assert(dispatch.source_valid(ssc));
    for (int count : calls) assert(count == 0);

    uint8_t other[45]{};
    assert(dispatch.source_index(other) == 6);
    assert(!dispatch.source_valid(other));
    assert(dispatch.init_capability(6, other));
    assert(dispatch.encoder(other) == &system_encoder);
    assert(!dispatch.build_header(other, &packet, 7));
    assert(packet.layer_specific == 7);
    assert(!dispatch.packet_timestamp(other, nullptr, &timestamp) && timestamp == 99);
    for (int count : calls) assert(count == 1);
}
