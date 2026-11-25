#include "aeld_codec_state.h"

#include "aeld_codec_info.h"
#include "codec_object.h"

#include <string.h>

namespace aeld {
namespace {
using namespace port::codec_object;

constexpr int32_t kBitrate = 265000;

void set_audio(void* object, size_t offset) {
    port::codec_object::set_audio(object, offset, kIndex, port::codec_object::kRate48k, kBits16, kStereo);
}

int32_t bitrate(const void*) { return kBitrate; }
// Apple's AU aggregation marks RTP packets as AAC does.
bool rtp_marker(const void*) { return true; }
bool no_offload_extension(const void*) { return false; }
void debug(void*, int32_t) {}
}  // namespace

bool init_codec(void* object) {
    uint8_t capability[kInfoSize];
    make_capability(capability);
    set_audio(object, kLocal);
    set_info(field(object, kOta), capability, kInfoSize);
    return true;
}

bool set_peer_capability(void* object, const uint8_t* peer) {
    if (!supports_profile(peer)) return false;
    set_audio(object, kLocal);
    set_audio(object, kSelectable);
    set_info(field(object, kPeerCapability), peer, kInfoSize);
    return true;
}

uint8_t select_codec(void* object, const uint8_t* peer, bool is_capability, uint8_t* selected) {
    if (!is_aac_eld(peer)) return kBadCodec;
    uint8_t config[kInfoSize];
    if (is_capability) {
        if (!make_config(peer, config)) return kUnsupported;
    } else {
        // A configuration chosen by the peer: accept exactly the profile this source sends.
        Info info;
        if (!parse(peer, &info) || info.object_type != kObjectAacEld ||
            info.sample_rates != kInfoRate48k || info.channels != kChannelsStereo) {
            return kUnsupported;
        }
        memcpy(config, peer, kInfoSize);
    }
    set_info(selected, config, kInfoSize);
    set_info(field(object, kOta), config, kInfoSize);
    set_info(field(object, kPeerConfig), peer, kInfoSize);
    set_audio(object, kCurrent);
    return kSuccess;
}

port::CodecTable make_codec_table(void (*destroy)(void*), void (*delete_object)(void*)) {
    return {destroy, delete_object, bitrate, rtp_marker, no_offload_extension,
            select_codec, set_peer_capability, init_codec, debug};
}

}  // namespace aeld
