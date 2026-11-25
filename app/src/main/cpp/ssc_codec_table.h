#pragma once

#include <stdint.h>

namespace sscport {

// Android 17 codec object virtual function order, separate from the encoder
// interface. Destructor callbacks must be supplied by the eventual integration.
struct CodecTable {
    void (*destroy)(void*);
    void (*delete_object)(void*);
    int32_t (*bitrate)(const void*);
    bool (*rtp_marker)(const void*);
    bool (*uses_offload_extension)(const void*);
    uint8_t (*select)(void*, const uint8_t*, bool, uint8_t*);
    bool (*peer_capability)(void*, const uint8_t*);
    bool (*init)(void*);
    void (*debug)(void*, int32_t);
};
static_assert(sizeof(CodecTable) == 9 * sizeof(void*));

CodecTable make_ssc_codec_table(void (*destroy)(void*),
                                void (*delete_object)(void*));

}  // namespace sscport
