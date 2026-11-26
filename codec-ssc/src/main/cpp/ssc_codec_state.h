#pragma once

#include <stdint.h>

namespace ssc {

// Operations on an already constructed Android 17 A2dpCodecConfig object.
// Construction, synchronization and registration belong to the caller.
bool init_ssc_codec(void* object);
bool set_ssc_peer_capability(void* object, const uint8_t* peer);
// Ordinary SSC is 48 kHz; SSC UHQ (96 kHz) is selected when the framework explicitly requests
// 96 kHz, or when nothing is requested and UHQ is preferred (the Buds manager's UHQ switch).
uint8_t select_ssc_codec(void* object, const uint8_t* peer,
                         bool is_capability, uint8_t* selected);
void set_uhq_preferred(bool preferred);
// Whether the codec object's selected (OTA) configuration is SSC UHQ.
bool codec_is_uhq(const void* object);

}  // namespace ssc
