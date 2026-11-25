#include "ssc_vendor_dispatch.h"
#include "ssc_negotiation.h"

#include <string.h>

namespace sscport {
namespace {
constexpr int32_t kSscIndex = 18;
constexpr size_t kCodecInfoSize = 45;
}

int32_t VendorDispatch::source_index(const uint8_t* codec) const {
    return is_ssc_codec(codec) ? kSscIndex : system_.source_index(codec);
}

bool VendorDispatch::source_valid(const uint8_t* codec) const {
    // Both ordinary SSC (48 kHz) and SSC UHQ (96 kHz) capabilities and configurations are valid.
    return is_ssc_codec(codec) || system_.source_valid(codec);
}

bool VendorDispatch::init_capability(int32_t index, void* capability) const {
    if (index != kSscIndex) return system_.init_capability(index, capability);
    auto* bytes = static_cast<uint8_t*>(capability);
    memset(bytes, 0, kCodecInfoSize);
    make_48k_capability(bytes);
    return true;
}

const EncoderInterface* VendorDispatch::encoder(const uint8_t* codec) const {
    return is_ssc_codec(codec) ? encoder_ : system_.encoder(codec);
}

bool VendorDispatch::build_header(const uint8_t* codec, BtHdr* packet,
                                  uint16_t frames) const {
    return is_ssc_codec(codec) ? true : system_.build_header(codec, packet, frames);
}

bool VendorDispatch::packet_timestamp(const uint8_t* codec, const uint8_t* data,
                                      uint32_t* timestamp) const {
    if (!is_ssc_codec(codec)) return system_.packet_timestamp(codec, data, timestamp);
    memcpy(timestamp, data, sizeof(*timestamp));
    return true;
}

}  // namespace sscport
