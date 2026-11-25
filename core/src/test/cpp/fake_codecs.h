#pragma once

// Two fake vendor codecs (indexes 30 and 31) for the core tests. Codec information byte 3 holds the
// vendor marker 0xa0 + n; the capability gets 0xc0 + n at byte 9.

#include <cstring>

#include "codec_plugin.h"

namespace fake {

inline bool init_ran[2];
inline port::EncoderInterface encoders[2];

template <int N>
bool init(void* object) {
    init_ran[N] = true;
    static_cast<uint8_t*>(object)[0x181] = static_cast<uint8_t>(0xc0 + N);
    return true;
}
template <int N>
port::CodecTable table(void (*destroy)(void*), void (*delete_object)(void*)) {
    port::CodecTable t{};
    t.destroy = destroy;
    t.delete_object = delete_object;
    t.init = init<N>;
    return t;
}
inline bool bind(const port::StackApi&) { return true; }
template <int N>
bool matches(const uint8_t* info) { return info[3] == 0xa0 + N; }
inline bool valid(const uint8_t*) { return true; }
template <int N>
bool capability(uint8_t* bytes) {
    bytes[3] = static_cast<uint8_t>(0xa0 + N);
    bytes[9] = static_cast<uint8_t>(0xc0 + N);
    return true;
}
template <int N>
const port::EncoderInterface* encoder() { return &encoders[N]; }
inline bool header(const uint8_t*, port::BtHdr*, uint16_t) { return true; }
template <int N>
bool timestamp(const uint8_t*, const uint8_t*, uint32_t* out) {
    *out = 100 + N;
    return true;
}

template <int N>
const port::CodecPlugin plugin{"FAKE", 30 + N, 0x1000 + N, 1000000, table<N>, bind,
                               matches<N>, valid, capability<N>, encoder<N>, header, timestamp<N>};

inline void use_both() {
    const port::CodecPlugin* list[] = {&plugin<0>, &plugin<1>};
    port::set_codecs(list, 2);
}

}  // namespace fake
