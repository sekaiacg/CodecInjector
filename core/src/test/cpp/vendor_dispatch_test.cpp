#include "vendor_dispatch.h"

#include <cassert>
#include <cstring>

#include "fake_codecs.h"

using namespace port;

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
}  // namespace

int main() {
    fake::use_both();
    VendorDispatch dispatch({system_index, system_valid, system_init, system_get_encoder,
                             system_header, system_timestamp});
    uint8_t codec[kCodecInfoSize];
    for (int n = 0; n < 2; ++n) {
        std::memset(codec, 0xa5, sizeof(codec));
        assert(dispatch.init_capability(30 + n, codec));
        assert(codec[3] == 0xa0 + n && codec[9] == 0xc0 + n && codec[44] == 0);
        assert(dispatch.source_index(codec) == 30 + n);
        assert(dispatch.source_valid(codec));
        assert(dispatch.encoder(codec) == &fake::encoders[n]);
        BtHdr packet{1, 2, 3, 4};
        assert(dispatch.build_header(codec, &packet, 1));
        uint32_t timestamp = 0;
        assert(dispatch.packet_timestamp(codec, nullptr, &timestamp) && timestamp == 100u + n);
    }
    for (int count : calls) assert(count == 0);

    uint8_t other[kCodecInfoSize]{};
    BtHdr packet{};
    uint32_t timestamp = 0;
    assert(dispatch.source_index(other) == 6);
    assert(!dispatch.source_valid(other));
    assert(dispatch.init_capability(6, other));
    assert(dispatch.encoder(other) == &system_encoder);
    assert(!dispatch.build_header(other, &packet, 7) && packet.layer_specific == 7);
    assert(!dispatch.packet_timestamp(other, nullptr, &timestamp) && timestamp == 99);
    for (int count : calls) assert(count == 1);
}
