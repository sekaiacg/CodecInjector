#include "ssc_codec_state.h"
#include "ssc_negotiation.h"

#include <stddef.h>
#include <string.h>

namespace sscport {
namespace {

constexpr size_t kCurrent = 0x60;
constexpr size_t kLocal = 0x98;
constexpr size_t kSelectable = 0xd0;
constexpr size_t kUser = 0x108;
constexpr size_t kOta = 0x178;
constexpr size_t kPeerCapability = 0x1a5;
constexpr size_t kPeerConfig = 0x1d2;
constexpr size_t kOtaSize = 45;
constexpr uint32_t k48k = 2;
constexpr uint32_t k96k = 8;
constexpr uint32_t k24bit = 2;
constexpr uint32_t kStereo = 2;
constexpr uint8_t kBadCodec = 0x29;
constexpr uint8_t kUnsupported = 0xc3;

uint8_t* field(void* object, size_t offset) {
    return static_cast<uint8_t*>(object) + offset;
}

void set_u32(void* object, size_t offset, uint32_t value) {
    memcpy(field(object, offset), &value, sizeof(value));
}

uint32_t get_u32(void* object, size_t offset) {
    uint32_t value;
    memcpy(&value, field(object, offset), sizeof(value));
    return value;
}

bool uhq_preferred = false;

void set_audio(void* object, size_t offset, uint32_t rates) {
    set_u32(object, offset, 18);
    set_u32(object, offset + 8, rates);
    set_u32(object, offset + 12, k24bit);
    set_u32(object, offset + 16, kStereo);
}

void set_ota(uint8_t* target, const uint8_t* source) {
    memset(target, 0, kOtaSize);
    memcpy(target, source, 10);
}

}  // namespace

bool init_ssc_codec(void* object) {
    uint8_t capability[10];
    make_48k_capability(capability);
    set_audio(object, kLocal, k48k | k96k);
    set_ota(field(object, kOta), capability);
    return true;
}

bool set_ssc_peer_capability(void* object, const uint8_t* peer) {
    uint8_t selected[10];
    const bool uhq = peer_supports_uhq(peer);
    if (!select_48k_capability(peer, selected) && !uhq) return false;
    set_audio(object, kLocal, k48k | k96k);
    set_audio(object, kSelectable, uhq ? k48k | k96k : k48k);
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
        const bool want = requested == k96k || (requested != k48k &&
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
    set_audio(object, kCurrent, uhq ? k96k : k48k);
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
