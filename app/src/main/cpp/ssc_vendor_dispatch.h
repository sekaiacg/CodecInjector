#pragma once

#include "ssc_interface.h"

namespace sscport {

// An isolated model of the existing system dispatch points. Callbacks represent
// the unmodified system implementations; this does not install any hooks.
struct SystemVendorDispatch {
    int32_t (*source_index)(const uint8_t*);
    bool (*source_valid)(const uint8_t*);
    bool (*init_capability)(int32_t, void*);
    const EncoderInterface* (*encoder)(const uint8_t*);
    bool (*build_header)(const uint8_t*, BtHdr*, uint16_t);
    bool (*packet_timestamp)(const uint8_t*, const uint8_t*, uint32_t*);
};

class VendorDispatch {
public:
    VendorDispatch(SystemVendorDispatch system, const EncoderInterface* encoder)
        : system_(system), encoder_(encoder) {}

    int32_t source_index(const uint8_t* codec) const;
    bool source_valid(const uint8_t* codec) const;
    // For SSC, capability points to the system's 45-byte codec-info area.
    bool init_capability(int32_t index, void* capability) const;
    const EncoderInterface* encoder(const uint8_t* codec) const;
    bool build_header(const uint8_t* codec, BtHdr* packet, uint16_t frames) const;
    bool packet_timestamp(const uint8_t* codec, const uint8_t* data,
                          uint32_t* timestamp) const;

private:
    SystemVendorDispatch system_;
    const EncoderInterface* encoder_;
};

}  // namespace sscport
