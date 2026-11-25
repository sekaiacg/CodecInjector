#pragma once

#include <stdint.h>

#include "a2dp_types.h"

namespace aeld {

constexpr int32_t kIndex = 17;  // a free source codec index on this stack (UNKNOWN in its name table)

// The A2dpCodecConfig table operations for an already constructed codec object.
bool init_codec(void* object);
bool set_peer_capability(void* object, const uint8_t* peer);
uint8_t select_codec(void* object, const uint8_t* peer, bool is_capability, uint8_t* selected);
port::CodecTable make_codec_table(void (*destroy)(void*), void (*delete_object)(void*));

}  // namespace aeld
