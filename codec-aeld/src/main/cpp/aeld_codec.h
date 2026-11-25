#pragma once

#include "codec_plugin.h"

namespace aeld {

// Apple AAC-ELD for AirPods, encoded with the stack's own FDK AAC encoder.
const port::CodecPlugin* plugin();

}  // namespace aeld
