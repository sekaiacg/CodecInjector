#pragma once

#include <stddef.h>
#include <stdint.h>

namespace port {

// Android 17 Bluetooth stack types the module plugs codecs into.

struct BtHdr {
    uint16_t event;
    uint16_t len;
    uint16_t offset;
    uint16_t layer_specific;
};
static_assert(sizeof(BtHdr) == 8);

using Allocate = void* (*)(size_t);
// The stack's PCM read and packet enqueue callbacks given to a source encoder.
using Read = uint32_t (*)(uint8_t*, uint32_t);
using Enqueue = bool (*)(BtHdr*, size_t, uint32_t);

struct PeerParams {
    bool is_edr;
    bool supports_3mbps;
    uint16_t mtu;
};
static_assert(sizeof(PeerParams) == 4);
static_assert(offsetof(PeerParams, mtu) == 2);

// Source encoder interface: the eight-slot table observed on Android 17.
struct EncoderInterface {
    void (*init)(const PeerParams*, void* codec, Read, Enqueue);
    void (*cleanup)();
    void (*feeding_reset)();
    void (*feeding_flush)();
    uint64_t (*interval_ms)();
    int (*effective_frame_size)();
    void (*send_frames)(uint64_t);
    void (*set_transmit_queue_length)(size_t);
};
static_assert(sizeof(EncoderInterface) == 8 * sizeof(void*));

// A2dpCodecConfig virtual function order on Android 17, separate from the encoder interface.
struct CodecTable {
    void (*destroy)(void*);
    void (*delete_object)(void*);
    int32_t (*bitrate)(const void*);
    bool (*rtp_marker)(const void*);
    bool (*uses_offload_extension)(const void*);
    uint8_t (*select)(void*, const uint8_t*, bool, uint8_t*);
    bool (*peer_capability)(void*, const uint8_t*);
    bool (*init)(void*);
    void (*debug)(void*, int32_t);
};
static_assert(sizeof(CodecTable) == 9 * sizeof(void*));

// Codec information element size and the A2dpCodecConfigExt allocation the stack uses.
constexpr size_t kCodecInfoSize = 45;
constexpr size_t kCodecObjectSize = 0x218;

}  // namespace port
