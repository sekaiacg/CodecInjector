#pragma once

#include <stddef.h>
#include <stdint.h>

#include "codec_plugin.h"
#include "dolby_hal_stage.h"

namespace port {

// Module log line (tag CodecInjector, "stage=module event=...").
void info(const char* event, int a, int b);

// The Dolby Spatializer stage, shared by all codecs: started and stopped by a codec's encoder,
// switched and steered from Java.
DolbyHalStage& spatial();

// Called by a codec's encoder init with the PCM rate of the session being set up (before the stack's
// setup_codec), so the audio HAL session is restarted when the rate changes.
void note_encoder_rate(uint32_t rate);

}  // namespace port

// Defined by the app: the codecs the module adds, in order.
void port_codecs(const port::CodecPlugin* const** codecs, size_t* count);
