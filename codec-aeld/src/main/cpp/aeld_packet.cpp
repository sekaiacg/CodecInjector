#include "aeld_packet.h"

#include <string.h>

namespace aeld {

void write_au_header(uint8_t* out, uint16_t sequence, uint16_t size) {
    out[0] = static_cast<uint8_t>(0xb0 | ((sequence >> 8) & 0x0f));
    out[1] = static_cast<uint8_t>(sequence & 0xff);
    out[2] = static_cast<uint8_t>(0x10 | ((size >> 8) & 0x0f));
    out[3] = static_cast<uint8_t>(size & 0xff);
}

Packetizer::Packetizer(uint16_t mtu, Allocate allocate, Enqueue enqueue)
    : allocate_(allocate), enqueue_(enqueue) {
    capacity_ = mtu > kRtpHeaderSize && mtu - kRtpHeaderSize < kPayloadMax ? mtu - kRtpHeaderSize
                                                                           : kPayloadMax;
}

bool Packetizer::add(const uint8_t* au, size_t size) {
    if (!au || size == 0 || size > kAuSizeMax || kAuHeaderSize + size > capacity_) return false;
    if (bytes_ + kAuHeaderSize + size > capacity_ && !flush()) return false;
    write_au_header(staged_ + bytes_, sequence_, static_cast<uint16_t>(size));
    memcpy(staged_ + bytes_ + kAuHeaderSize, au, size);
    bytes_ += kAuHeaderSize + size;
    ++frames_;
    sequence_ = static_cast<uint16_t>((sequence_ + 1) & 0x0fff);
    return frames_ < kFramesPerPacket || flush();
}

bool Packetizer::flush() {
    if (frames_ == 0) return true;
    auto* packet = static_cast<BtHdr*>(allocate_(kBufferBytes));
    const uint32_t frames = frames_;
    const size_t bytes = bytes_;
    const uint32_t timestamp = timestamp_;
    timestamp_ += frames * kSamplesPerFrame;
    frames_ = 0;
    bytes_ = 0;
    if (!packet) return false;
    memset(packet, 0, sizeof(BtHdr) + kMediaOffset);
    packet->len = static_cast<uint16_t>(bytes);
    packet->offset = kMediaOffset;
    packet->layer_specific = static_cast<uint16_t>(frames);
    auto* data = reinterpret_cast<uint8_t*>(packet + 1);
    // A2DP_GetPacketTimestamp reads the RTP timestamp from the start of the data.
    memcpy(data, &timestamp, sizeof(timestamp));
    memcpy(data + kMediaOffset, staged_, bytes);
    // A refusal (the stack flushing its queue on suspend, or the queue full) drops the packet;
    // the stream itself goes on.
    enqueue_(packet, frames, frames * kPcmBytesPerFrame);
    return true;
}

void Packetizer::discard() {
    frames_ = 0;
    bytes_ = 0;
}

void Packetizer::reset() {
    discard();
    sequence_ = 1;
    timestamp_ = 0;
}

}  // namespace aeld
