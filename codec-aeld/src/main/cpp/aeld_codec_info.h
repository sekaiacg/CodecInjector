#pragma once

// Apple AAC-ELD codec information element (vendor 0x004c, codec 0x8001), as AirPods advertise it:
//   [0]       LOSC (16)
//   [1]       media type (audio, 0x00)
//   [2]       media codec type (vendor, 0xff)
//   [3..6]    vendor ID, little endian
//   [7..8]    codec ID, little endian
//   [9..10]   object type bitmap, big endian (0x0080 = AAC-ELD)
//   [11..12]  sample rate bitmap (high 12 bits, 0x008 = 48 kHz) and channels (low 4 bits:
//             0x8 mono, 0x4 stereo), big endian
//   [13]      reserved
//   [14..16]  VBR flag (bit 23) and maximum bitrate (bits 22..0), big endian

#include <stddef.h>
#include <stdint.h>

namespace aeld {

constexpr size_t kInfoSize = 17;
constexpr uint8_t kLosc = 16;
constexpr uint32_t kVendorApple = 0x004c;
constexpr uint16_t kCodecAacEld = 0x8001;
constexpr uint16_t kObjectAacEld = 0x0080;
constexpr uint16_t kInfoRate48k = 0x008;
constexpr uint8_t kChannelsMono = 0x8;
constexpr uint8_t kChannelsStereo = 0x4;
constexpr uint32_t kVbr = 0x800000;
constexpr uint32_t kBitrateMask = 0x7fffff;
// What Apple's source advertises and configures (the encoder itself runs CBR 265 kbps).
constexpr uint32_t kAdvertisedBitrate = 256000;

struct Info {
    uint16_t object_type;
    uint16_t sample_rates;
    uint8_t channels;
    bool vbr;
    uint32_t bitrate;
};

// Media type, codec type, vendor and codec ID only (A2DP_VendorSourceCodecIndex's question).
bool is_aac_eld(const uint8_t* info);
bool parse(const uint8_t* info, Info* out);
// Whether the element allows the one profile this source sends: AAC-ELD, 48 kHz, stereo.
bool supports_profile(const uint8_t* info);
// The local SEP capability: AAC-ELD, 48 kHz, mono and stereo, VBR, 256 kbps.
void make_capability(uint8_t* out);
// The configuration for that profile; false when the peer does not support it.
bool make_config(const uint8_t* peer, uint8_t* out);

}  // namespace aeld
