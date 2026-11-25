#include "ssc_codec_table.h"
#include "ssc_codec_state.h"

namespace sscport {
namespace {
int32_t bitrate(const void*) { return 192000; }
bool no_marker(const void*) { return false; }
bool no_offload_extension(const void*) { return false; }
void debug(void*, int32_t) {}
}  // namespace

CodecTable make_ssc_codec_table(void (*destroy)(void*),
                                void (*delete_object)(void*)) {
    return {destroy, delete_object, bitrate, no_marker, no_offload_extension,
            select_ssc_codec, set_ssc_peer_capability, init_ssc_codec, debug};
}

}  // namespace sscport
