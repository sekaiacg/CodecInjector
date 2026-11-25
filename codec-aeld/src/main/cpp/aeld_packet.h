#pragma once

// Apple AAC-ELD media packets: up to three access units per RTP packet, each behind a 4-byte header
//   byte 0  0xb0 | sequence bits 11..8
//   byte 1  sequence bits 7..0
//   byte 2  0x10 | AU length bits 11..8
//   byte 3  AU length bits 7..0
// The 12-bit sequence starts at 1 after a reset and wraps 4095 -> 0. Apple's receiver rejects AUs
// over 0x7ff bytes. The RTP timestamp advances 480 per access unit.

#include <stddef.h>
#include <stdint.h>

#include "a2dp_types.h"

namespace aeld {

using port::Allocate;
using port::BtHdr;
using port::Enqueue;

constexpr size_t kAuHeaderSize = 4;
constexpr size_t kAuSizeMax = 0x7ff;
constexpr size_t kFramesPerPacket = 3;
constexpr uint32_t kSamplesPerFrame = 480;
constexpr uint32_t kPcmBytesPerFrame = kSamplesPerFrame * 2 * 2;  // stereo 16-bit
// The stack's media buffer: BT_HDR, the space it fills with AVDTP/RTP headers, then the payload.
constexpr size_t kBufferBytes = 0x1010;
constexpr uint16_t kMediaOffset = 23;
constexpr size_t kPayloadMax = kBufferBytes - sizeof(BtHdr) - kMediaOffset;
constexpr size_t kRtpHeaderSize = 12;

void write_au_header(uint8_t* out, uint16_t sequence, uint16_t size);

// Builds packets in stack buffers and hands each to the stack's enqueue callback, which owns the
// packet regardless of its result.
class Packetizer {
public:
    Packetizer() = default;
    Packetizer(uint16_t mtu, Allocate allocate, Enqueue enqueue);
    // Payload bytes one packet can carry for this peer.
    size_t capacity() const { return capacity_; }
    // Adds one access unit, sending the open packet first when the AU does not fit, and sending
    // the packet once it holds three. False when the AU is invalid or no packet buffer was
    // available; a packet the stack refuses is dropped.
    bool add(const uint8_t* au, size_t size);
    // Sends a partly filled packet (false when no packet buffer was available).
    bool flush();
    // Drops the open packet; keeps the sequence and timestamp.
    void discard();
    // New stream: drops the open packet, sequence back to 1, timestamp to 0.
    void reset();
    uint16_t sequence() const { return sequence_; }
    uint32_t timestamp() const { return timestamp_; }

private:
    Allocate allocate_ = nullptr;
    Enqueue enqueue_ = nullptr;
    size_t capacity_ = 0;
    uint8_t staged_[kPayloadMax];
    size_t bytes_ = 0;
    uint32_t frames_ = 0;
    uint16_t sequence_ = 1;
    uint32_t timestamp_ = 0;
};

}  // namespace aeld
