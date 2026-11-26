#include "ssc_negotiation.h"

#include <string.h>

namespace ssc {
namespace {
constexpr uint8_t kPrefix[] = {0x09, 0x00, 0xff, 0x75, 0x00,
                               0x00, 0x00, 0x03, 0x01};
constexpr uint8_t kNormal = 0x04;
constexpr uint8_t kUhq = 0x02;
constexpr uint8_t kReserved = 0x01;
constexpr uint8_t kBitrateLimit = 0x08;
}

void make_48k_capability(uint8_t* capability) {
    memcpy(capability, kPrefix, sizeof(kPrefix));
    capability[9] = kNormal | kBitrateLimit;
}

bool is_ssc_codec(const uint8_t* codec) {
    return memcmp(codec, kPrefix, sizeof(kPrefix)) == 0;
}

bool select_48k_capability(const uint8_t* peer, uint8_t* selected) {
    if (!is_ssc_codec(peer) || !(peer[9] & kNormal))
        return false;
    memcpy(selected, kPrefix, sizeof(kPrefix));
    selected[9] = kNormal | (peer[9] & kBitrateLimit);
    return true;
}

bool peer_supports_uhq(const uint8_t* peer) {
    return is_ssc_codec(peer) && (peer[9] & kUhq) && !(peer[9] & kReserved);
}

bool select_uhq_capability(const uint8_t* peer, uint8_t* selected) {
    if (!peer_supports_uhq(peer)) return false;
    memcpy(selected, kPrefix, sizeof(kPrefix));
    selected[9] = kUhq | (peer[9] & kBitrateLimit);
    return true;
}

}  // namespace ssc
