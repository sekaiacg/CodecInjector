#pragma once

#include "ssc_codec_table.h"

namespace sscport {

// Model only: the system allocates and owns the object. The base constructor
// callback must construct a valid current-system base object in that storage.
struct FactoryCallbacks {
    bool (*initialize_system)(void* codecs);
    bool (*system_supports)(int32_t index);
    void (*construct_system_extension)(void* storage, int32_t index, bool source);
    void (*construct_ssc_base)(void* storage);
};

class Factory {
public:
    Factory(FactoryCallbacks callbacks, const CodecTable* table)
        : callbacks_(callbacks), table_(table) {}
    bool initialize(void* codecs);
    bool supports(int32_t index);
    void construct_extension(void* storage, int32_t index, bool source);
    // This only reports construction during init, not inclusion in the
    // system's enabled-codec map or source list.
    bool constructed() const { return constructed_; }

private:
    FactoryCallbacks callbacks_;
    const CodecTable* table_;
    bool collision_ = false;
    bool constructed_ = false;
};

}  // namespace sscport
