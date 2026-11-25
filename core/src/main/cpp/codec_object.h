#pragma once

// Fields of the Android 17 A2dpCodecConfig object a vendor codec fills in, and the
// BluetoothCodecConfig bit values they hold.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "a2dp_types.h"

namespace port::codec_object {

// btav_a2dp_codec_config_t blocks: codec type, priority, sample rate, bits per sample, channel mode.
constexpr size_t kCurrent = 0x60;
constexpr size_t kLocal = 0x98;
constexpr size_t kSelectable = 0xd0;
constexpr size_t kUser = 0x108;
// Codec information elements (kCodecInfoSize each).
constexpr size_t kOta = 0x178;
constexpr size_t kPeerCapability = 0x1a5;
constexpr size_t kPeerConfig = 0x1d2;

constexpr uint32_t kRate48k = 0x02;
constexpr uint32_t kRate96k = 0x08;
constexpr uint32_t kBits16 = 0x01;
constexpr uint32_t kBits24 = 0x02;
constexpr uint32_t kStereo = 0x02;

// A2DP status codes returned by the codec table's select.
constexpr uint8_t kSuccess = 0x00;
constexpr uint8_t kBadCodec = 0x29;
constexpr uint8_t kUnsupported = 0xc3;

inline uint8_t* field(void* object, size_t offset) { return static_cast<uint8_t*>(object) + offset; }
inline const uint8_t* field(const void* object, size_t offset) {
    return static_cast<const uint8_t*>(object) + offset;
}

inline uint32_t get_u32(const void* object, size_t offset) {
    uint32_t value;
    memcpy(&value, field(object, offset), sizeof(value));
    return value;
}

inline void set_u32(void* object, size_t offset, uint32_t value) {
    memcpy(field(object, offset), &value, sizeof(value));
}

// Writes one config block; the priority is the stack's.
inline void set_audio(void* object, size_t offset, int32_t index, uint32_t rates, uint32_t bits,
                      uint32_t channels) {
    set_u32(object, offset, static_cast<uint32_t>(index));
    set_u32(object, offset + 8, rates);
    set_u32(object, offset + 12, bits);
    set_u32(object, offset + 16, channels);
}

// Copies a codec information element of `size` bytes, zero-filling the rest.
inline void set_info(uint8_t* target, const uint8_t* source, size_t size) {
    memset(target, 0, kCodecInfoSize);
    memcpy(target, source, size < kCodecInfoSize ? size : kCodecInfoSize);
}

}  // namespace port::codec_object
