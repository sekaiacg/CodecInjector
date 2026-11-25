#include "aeld_codec_info.h"
#include "aeld_codec_state.h"
#include "codec_object.h"

#include <cassert>
#include <cstring>

using namespace aeld;
using namespace port::codec_object;

namespace {
// AirPods Pro 2: AAC-ELD, 48 kHz, mono + stereo, VBR, 256 kbps.
const uint8_t kAirPods[kInfoSize] = {0x10, 0x00, 0xff, 0x4c, 0x00, 0x00, 0x00, 0x01, 0x80,
                                     0x00, 0x80, 0x00, 0x8c, 0x00, 0x83, 0xe8, 0x00};
alignas(8) uint8_t object[port::kCodecObjectSize];
}  // namespace

int main() {
    uint8_t capability[kInfoSize];
    make_capability(capability);
    assert(memcmp(capability, kAirPods, kInfoSize) == 0);
    assert(is_aac_eld(kAirPods) && supports_profile(kAirPods));
    Info info;
    assert(parse(kAirPods, &info) && info.object_type == kObjectAacEld &&
           info.sample_rates == kInfoRate48k && info.channels == (kChannelsMono | kChannelsStereo) &&
           info.vbr && info.bitrate == 256000);

    uint8_t config[kInfoSize];
    assert(make_config(kAirPods, config));
    assert(config[12] == 0x84 && memcmp(config, kAirPods, 12) == 0 &&
           memcmp(config + 13, kAirPods + 13, 4) == 0);

    uint8_t other[kInfoSize];
    memcpy(other, kAirPods, kInfoSize);
    other[3] = 0x75;  // Samsung
    assert(!is_aac_eld(other));
    memcpy(other, kAirPods, kInfoSize);
    other[12] = 0x88;  // mono only
    assert(is_aac_eld(other) && !supports_profile(other) && !make_config(other, config));
    memcpy(other, kAirPods, kInfoSize);
    other[11] = 0x00;
    other[12] = 0x4c;  // 44.1 kHz only
    assert(!supports_profile(other));

    // Codec object: capability, selection, peer-chosen configuration.
    assert(init_codec(object));
    assert(get_u32(object, kLocal) == kIndex && get_u32(object, kLocal + 8) == port::codec_object::kRate48k &&
                     get_u32(object, kLocal + 12) == kBits16 && memcmp(object + kOta, kAirPods, kInfoSize) == 0);
    assert(set_peer_capability(object, kAirPods));
    assert(get_u32(object, kSelectable + 8) == port::codec_object::kRate48k);
    assert(!set_peer_capability(object, other));
    uint8_t selected[port::kCodecInfoSize];
    memset(selected, 0xa5, sizeof(selected));
    assert(select_codec(object, kAirPods, true, selected) == kSuccess);
    assert(selected[12] == 0x84 && selected[17] == 0 && selected[44] == 0);
    assert(memcmp(object + kOta, selected, kInfoSize) == 0);
    assert(get_u32(object, kCurrent) == kIndex && get_u32(object, kCurrent + 16) == kStereo);
    assert(select_codec(object, config, false, selected) == kSuccess);
    assert(select_codec(object, kAirPods, false, selected) == kUnsupported);  // mono + stereo
    memcpy(other, kAirPods, kInfoSize);
    other[3] = 0x75;
    assert(select_codec(object, other, true, selected) == kBadCodec);
}
