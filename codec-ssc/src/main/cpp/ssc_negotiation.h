#pragma once

#include <stdint.h>

namespace sscport {

// The codec information element contains the AVDTP length/header, Samsung
// vendor/codec ID, and the selected mode. The output is the 48 kHz selection.
bool select_48k_capability(const uint8_t* peer, uint8_t* selected);
// SSC UHQ (96 kHz): the peer capability carries profile bit 0x02 and not the reserved 0x01. The
// selected form is 0x02 plus the peer's bitrate-limitation bit, as Samsung's stack selects it.
bool peer_supports_uhq(const uint8_t* peer);
bool select_uhq_capability(const uint8_t* peer, uint8_t* selected);
bool is_ssc_codec(const uint8_t* codec);
void make_48k_capability(uint8_t* capability);

}  // namespace sscport
