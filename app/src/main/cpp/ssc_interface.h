#pragma once

#include "ssc_frame.h"
#include "ssc_packet.h"

namespace sscport {

struct PeerParams {
    bool is_edr;
    bool supports_3mbps;
    uint16_t mtu;
};
static_assert(sizeof(PeerParams) == 4);
static_assert(offsetof(PeerParams, mtu) == 2);

using Create = void* (*)(int32_t, int32_t, int32_t, int32_t*);
using Destroy = void (*)(void*);

struct Dependencies {
    Create create;
    Destroy destroy;
    Encode encode;
    Allocate allocate;
};

// Matches the eight-slot Opus source encoder table observed on Android 17.
struct EncoderInterface {
    void (*init)(const PeerParams*, void*, Read, Enqueue);
    void (*cleanup)();
    void (*feeding_reset)();
    void (*feeding_flush)();
    uint64_t (*interval_ms)();
    int (*effective_frame_size)();
    void (*send_frames)(uint64_t);
    void (*set_transmit_queue_length)(size_t);
};
static_assert(sizeof(EncoderInterface) == 8 * sizeof(void*));

// Bind a single serialized software-source session. The caller owns codec
// registration, library loading and the system timer.
const EncoderInterface* bind(const Dependencies& dependencies);
// Bitrate mode from developer options: 0 adaptive, 1/2/3 fixed high/medium/low (see BitrateLadder).
void set_quality_mode(uint32_t mode);

}  // namespace sscport
