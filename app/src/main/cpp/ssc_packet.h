#pragma once

#include <stddef.h>
#include <stdint.h>

namespace sscport {

struct BtHdr {
    uint16_t event;
    uint16_t len;
    uint16_t offset;
    uint16_t layer_specific;
};

using Allocate = void* (*)(size_t);
using Enqueue = bool (*)(BtHdr*, size_t, uint32_t);

// Local experiment matching the earlier observed SSC packet contract.
// The enqueue callback owns the packet regardless of its return value.
class Packetizer {
public:
    Packetizer(uint16_t mtu, Allocate allocate, Enqueue enqueue)
        : mtu_(mtu), allocate_(allocate), enqueue_(enqueue) {}
    bool emit(const uint8_t* frame, size_t size, uint32_t pcm_bytes);
    void reset() { packet_count_ = 0; }
    static bool emit_callback(void* context, const uint8_t* frame,
                              size_t size, uint32_t pcm_bytes);

private:
    uint16_t mtu_;
    Allocate allocate_;
    Enqueue enqueue_;
    uint32_t packet_count_ = 0;
};

}  // namespace sscport
