#include "aeld_codec_info.h"

#include <string.h>

namespace aeld {
namespace {
void serialize(const Info& in, uint8_t* out) {
    memset(out, 0, kInfoSize);
    out[0] = kLosc;
    out[1] = 0x00;
    out[2] = 0xff;
    out[3] = kVendorApple & 0xff;
    out[4] = (kVendorApple >> 8) & 0xff;
    out[7] = kCodecAacEld & 0xff;
    out[8] = kCodecAacEld >> 8;
    out[9] = in.object_type >> 8;
    out[10] = in.object_type & 0xff;
    const uint16_t rate_channels = static_cast<uint16_t>((in.sample_rates << 4) | in.channels);
    out[11] = rate_channels >> 8;
    out[12] = rate_channels & 0xff;
    const uint32_t bitrate = (in.bitrate & kBitrateMask) | (in.vbr ? kVbr : 0);
    out[14] = (bitrate >> 16) & 0xff;
    out[15] = (bitrate >> 8) & 0xff;
    out[16] = bitrate & 0xff;
}
}  // namespace

bool is_aac_eld(const uint8_t* info) {
    return info && info[0] >= kLosc && info[1] == 0x00 && info[2] == 0xff &&
           info[3] == (kVendorApple & 0xff) && info[4] == (kVendorApple >> 8) && info[5] == 0 &&
           info[6] == 0 && info[7] == (kCodecAacEld & 0xff) && info[8] == (kCodecAacEld >> 8);
}

bool parse(const uint8_t* info, Info* out) {
    if (!is_aac_eld(info) || info[0] != kLosc) return false;
    out->object_type = static_cast<uint16_t>((info[9] << 8) | info[10]);
    const uint16_t rate_channels = static_cast<uint16_t>((info[11] << 8) | info[12]);
    out->sample_rates = rate_channels >> 4;
    out->channels = rate_channels & 0x0f;
    const uint32_t bitrate = (uint32_t{info[14]} << 16) | (uint32_t{info[15]} << 8) | info[16];
    out->vbr = (bitrate & kVbr) != 0;
    out->bitrate = bitrate & kBitrateMask;
    return true;
}

bool supports_profile(const uint8_t* info) {
    Info parsed;
    return parse(info, &parsed) && (parsed.object_type & kObjectAacEld) &&
           (parsed.sample_rates & kInfoRate48k) && (parsed.channels & kChannelsStereo);
}

void make_capability(uint8_t* out) {
    serialize({kObjectAacEld, kInfoRate48k, kChannelsMono | kChannelsStereo, true, kAdvertisedBitrate},
              out);
}

bool make_config(const uint8_t* peer, uint8_t* out) {
    if (!supports_profile(peer)) return false;
    serialize({kObjectAacEld, kInfoRate48k, kChannelsStereo, true, kAdvertisedBitrate}, out);
    return true;
}

}  // namespace aeld
