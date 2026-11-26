#pragma once

#include "codec_plugin.h"

namespace ssc {

// Samsung Seamless Codec (vendor 0x0075, codec 0x0103) as a core codec plugin, index 18.
const port::CodecPlugin* plugin();

}  // namespace ssc
