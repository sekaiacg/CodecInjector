#pragma once

#include <stdint.h>

#include "a2dp_types.h"

namespace ssc {

using port::CodecTable;

// The SSC A2dpCodecConfig vtable; destroy/delete are the stack's.
CodecTable make_ssc_codec_table(void (*destroy)(void*),
                                void (*delete_object)(void*));

}  // namespace ssc
