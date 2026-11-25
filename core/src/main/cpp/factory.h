#pragma once

#include "codec_plugin.h"

namespace port {

// Adds the registered codecs to A2dpCodecs::init. The system allocates and owns each object; the
// base constructor callback constructs a valid A2dpCodecConfig base in that storage.
struct FactoryCallbacks {
    bool (*initialize_system)(void* codecs);
    bool (*system_supports)(int32_t index);
    void (*construct_system_extension)(void* storage, int32_t index, bool source);
    void (*construct_base)(void* storage, const CodecPlugin& codec);
};

class Factory {
public:
    // tables[i] is the vtable of codec_at(i).
    Factory(FactoryCallbacks callbacks, const CodecTable* tables)
        : callbacks_(callbacks), tables_(tables) {}
    bool initialize(void* codecs);
    bool supports(int32_t index);
    void construct_extension(void* storage, int32_t index, bool source);
    // Whether the codec with this index was constructed during init (not whether the system then
    // kept it in its enabled-codec map or source list).
    bool constructed(int32_t index) const;

private:
    FactoryCallbacks callbacks_;
    const CodecTable* tables_;
    bool collision_[kMaxCodecs] = {};
    bool constructed_[kMaxCodecs] = {};
};

}  // namespace port
