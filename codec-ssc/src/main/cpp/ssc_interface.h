#pragma once

#include "ssc_frame.h"
#include "ssc_packet.h"

namespace sscport {

using port::EncoderInterface;
using port::PeerParams;

using Create = void* (*)(int32_t, int32_t, int32_t, int32_t*);
using Destroy = void (*)(void*);

struct Dependencies {
    Create create;
    Destroy destroy;
    Encode encode;
    Allocate allocate;
};

// Bind a single serialized software-source session. The caller owns codec
// registration, library loading and the system timer.
const EncoderInterface* bind(const Dependencies& dependencies);
// Bitrate mode from developer options: 0 adaptive, 1/2/3 fixed high/medium/low (see BitrateLadder).
void set_quality_mode(uint32_t mode);

}  // namespace sscport
