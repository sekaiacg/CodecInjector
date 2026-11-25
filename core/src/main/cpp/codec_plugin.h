#pragma once

#include "a2dp_types.h"

namespace port {

// Stack services a codec can use once the stack is resolved.
struct StackApi {
    Allocate allocate;  // the stack's osi_malloc, for packet buffers
    const char* image;  // path of the loaded Bluetooth stack image
    uintptr_t base;     // its load bias, for resolve_symbols
};

// A vendor A2DP source codec added to the stack. Each codec module defines one; the app lists them.
struct CodecPlugin {
    const char* name;      // codec name the stack reports (e.g. "SSC")
    int32_t index;         // btav_a2dp_codec_index_t the codec takes
    uint64_t codec_id;     // the stack's 64-bit CodecId
    int32_t priority;      // initial codec priority
    // The A2dpCodecConfig vtable; destroy/delete are the stack's, supplied by the core.
    CodecTable (*make_table)(void (*destroy)(void*), void (*delete_object)(void*));
    // Loads the encoder; returning false leaves the codec out.
    bool (*bind)(const StackApi& stack);
    // Whether a codec information element is this codec's.
    bool (*matches)(const uint8_t* codec_info);
    bool (*source_valid)(const uint8_t* codec_info);
    // Fills the local SEP capability (a zeroed kCodecInfoSize area).
    bool (*init_capability)(uint8_t* capability);
    const EncoderInterface* (*encoder)();
    bool (*build_header)(const uint8_t* codec_info, BtHdr* packet, uint16_t frames);
    bool (*packet_timestamp)(const uint8_t* codec_info, const uint8_t* data, uint32_t* timestamp);
};

constexpr size_t kMaxCodecs = 8;

// The codecs in use, set once before the stack is hooked.
void set_codecs(const CodecPlugin* const* codecs, size_t count);
size_t codec_count();
const CodecPlugin* codec_at(size_t position);
// Position of the codec with this stack index, or -1.
int codec_position(int32_t index);
const CodecPlugin* codec_for_index(int32_t index);
const CodecPlugin* codec_for_info(const uint8_t* codec_info);

}  // namespace port
