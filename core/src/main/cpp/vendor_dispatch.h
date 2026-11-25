#pragma once

#include "codec_plugin.h"

namespace port {

// The stack's vendor codec dispatch points; the callbacks are the unmodified implementations.
struct SystemVendorDispatch {
    int32_t (*source_index)(const uint8_t*);
    bool (*source_valid)(const uint8_t*);
    bool (*init_capability)(int32_t, void*);
    const EncoderInterface* (*encoder)(const uint8_t*);
    bool (*build_header)(const uint8_t*, BtHdr*, uint16_t);
    bool (*packet_timestamp)(const uint8_t*, const uint8_t*, uint32_t*);
};

// Routes each dispatch point to the registered codec that owns the codec information, and
// everything else to the system.
class VendorDispatch {
public:
    explicit VendorDispatch(SystemVendorDispatch system) : system_(system) {}

    int32_t source_index(const uint8_t* codec) const;
    bool source_valid(const uint8_t* codec) const;
    // capability points to the stack's kCodecInfoSize codec-info area.
    bool init_capability(int32_t index, void* capability) const;
    const EncoderInterface* encoder(const uint8_t* codec) const;
    bool build_header(const uint8_t* codec, BtHdr* packet, uint16_t frames) const;
    bool packet_timestamp(const uint8_t* codec, const uint8_t* data, uint32_t* timestamp) const;

private:
    SystemVendorDispatch system_;
};

}  // namespace port
