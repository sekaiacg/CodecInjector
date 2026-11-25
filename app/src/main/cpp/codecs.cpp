// The codecs this module adds to the Bluetooth stack.

#include "aeld_codec.h"
#include "port_api.h"
#include "ssc_codec.h"

void port_codecs(const port::CodecPlugin* const** codecs, size_t* count) {
    static const port::CodecPlugin* const kCodecs[] = {
        sscport::plugin(),
        aeld::plugin(),
    };
    *codecs = kCodecs;
    *count = sizeof(kCodecs) / sizeof(kCodecs[0]);
}
