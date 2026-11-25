#include "ssc_codec_state.h"
#include "ssc_negotiation.h"

#include <stddef.h>
#include <string.h>

#include "codec_object.h"

namespace sscport {
using namespace port::codec_object;

namespace {

bool uhq_preferred = false;

void set_audio(void* object, size_t offset, uint32_t rates) {
    port::codec_object::set_audio(object, offset, 18, rates, kBits24, kStereo);
}

void set_ota(uint8_t* target, const uint8_t* source) { set_info(target, source, 10); }

}  // namespace

bool init_ssc_codec(void* object) {
    uint8_t capability[10];
    make_48k_capability(capability);
    set_audio(object, kLocal, kRate48k | kRate96k);
    set_ota(field(object, kOta), capability);
    return true;
}

bool set_ssc_peer_capability(void* object, const uint8_t* peer) {
    uint8_t selected[10];
    const bool uhq = peer_supports_uhq(peer);
    if (!select_48k_capability(peer, selected) && !uhq) return false;
    set_audio(object, kLocal, kRate48k | kRate96k);
    set_audio(object, kSelectable, uhq ? kRate48k | kRate96k : kRate48k);
    set_ota(field(object, kPeerCapability), peer);
    return true;
}

uint8_t select_ssc_codec(void* object, const uint8_t* peer,
                         bool is_capability, uint8_t* selected) {
    uint8_t config[10];
    if (!is_ssc_codec(peer)) return kBadCodec;
    bool uhq;
    if (is_capability) {
        const uint32_t requested = get_u32(object, kUser + 8);
        const bool want = requested == kRate96k || (requested != kRate48k &&
                __atomic_load_n(&uhq_preferred, __ATOMIC_RELAXED));
        uhq = want ? select_uhq_capability(peer, config) : false;
        if (!uhq && !select_48k_capability(peer, config)) {
            if (!select_uhq_capability(peer, config)) return kUnsupported;
            uhq = true;
        }
    } else {
        // A configuration chosen by the peer: the UHQ form has 0x02 without the capability-only
        // sample-rate bits.
        uhq = (peer[9] & 0x02) && !(peer[9] & 0x04);
        if (uhq ? !select_uhq_capability(peer, config) : !select_48k_capability(peer, config))
            return kUnsupported;
    }
    set_ota(selected, config);
    set_ota(field(object, kOta), config);
    set_ota(field(object, kPeerConfig), peer);
    set_audio(object, kCurrent, uhq ? kRate96k : kRate48k);
    return 0;
}

void set_uhq_preferred(bool preferred) {
    __atomic_store_n(&uhq_preferred, preferred, __ATOMIC_RELAXED);
}

bool codec_is_uhq(const void* object) {
    const uint8_t mode = static_cast<const uint8_t*>(object)[kOta + 9];
    return (mode & 0x02) && !(mode & 0x04);
}

}  // namespace sscport
