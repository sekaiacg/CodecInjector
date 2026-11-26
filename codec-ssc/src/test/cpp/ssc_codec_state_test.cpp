#include "ssc_codec_state.h"
#include "ssc_codec_table.h"
#include "ssc_negotiation.h"

#include <cassert>
#include <cstdint>
#include <cstring>

using namespace ssc;

static uint32_t read32(const uint8_t* data, size_t offset) {
    uint32_t value;
    std::memcpy(&value, data + offset, sizeof(value));
    return value;
}

int main() {
    alignas(8) uint8_t object[0x218]{};
    uint8_t capability[45]{};
    uint8_t selected[45]{};
    make_48k_capability(capability);
    CodecTable table = make_ssc_codec_table(nullptr, nullptr);
    assert(table.init(object));
    assert(table.bitrate(object) == 192000);
    assert(!table.rtp_marker(object) && !table.uses_offload_extension(object));
    assert(read32(object, 0x98) == 18 && read32(object, 0xa0) == 0x0a);
    assert(read32(object, 0xa4) == 2 && read32(object, 0xa8) == 2);
    assert(object[0x181] == 0x0c);
    capability[9] = 0x3e;
    assert(table.peer_capability(object, capability));
    assert(object[0x1ae] == 0x3e);
    assert(read32(object, 0xd8) == 0x0a);
    // Nothing requested: ordinary 48 kHz SSC.
    assert(table.select(object, capability, true, selected) == 0);
    assert(selected[9] == 0x0c && object[0x181] == 0x0c && !codec_is_uhq(object));
    assert(read32(object, 0x60) == 18 && read32(object, 0x68) == 2);
    assert(object[0x1db] == 0x3e);
    // Nothing requested, UHQ preferred: 96 kHz.
    set_uhq_preferred(true);
    assert(select_ssc_codec(object, capability, true, selected) == 0);
    assert(selected[9] == 0x0a && codec_is_uhq(object) && read32(object, 0x68) == 8);
    // Explicit 48 kHz wins over the preference.
    uint32_t rate = 2;
    std::memcpy(object + 0x110, &rate, sizeof(rate));
    assert(select_ssc_codec(object, capability, true, selected) == 0 && selected[9] == 0x0c);
    set_uhq_preferred(false);
    // Explicit 96 kHz.
    rate = 8;
    std::memcpy(object + 0x110, &rate, sizeof(rate));
    assert(select_ssc_codec(object, capability, true, selected) == 0 && selected[9] == 0x0a);
    // Peer without UHQ: 96 kHz request falls back to 48 kHz.
    capability[9] = 0x0c;
    assert(select_ssc_codec(object, capability, true, selected) == 0 && selected[9] == 0x0c);
    // Peer-chosen configurations.
    assert(select_ssc_codec(object, capability, false, selected) == 0 && selected[9] == 0x0c);
    capability[9] = 0x0a;
    assert(select_ssc_codec(object, capability, false, selected) == 0 && selected[9] == 0x0a);
    // Reserved bit set: no UHQ.
    capability[9] = 0x03;
    assert(select_ssc_codec(object, capability, true, selected) == 0xc3);
    capability[3] = 0;
    assert(select_ssc_codec(object, capability, true, selected) == 0x29);
    assert(!set_ssc_peer_capability(object, capability));
}
