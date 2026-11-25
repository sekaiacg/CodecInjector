#include "factory.h"

namespace port {
namespace {
thread_local Factory* active_factory = nullptr;
}

bool Factory::initialize(void* codecs) {
    // Scope the synthetic factory answers to the thread executing init. Other calls continue to see
    // the provider's actual capabilities.
    Factory* previous = active_factory;
    active_factory = this;
    for (size_t i = 0; i < kMaxCodecs; ++i) collision_[i] = constructed_[i] = false;
    const bool result = callbacks_.initialize_system(codecs);
    active_factory = previous;
    for (size_t i = 0; i < kMaxCodecs; ++i) {
        constructed_[i] = result && constructed_[i] && !collision_[i];
    }
    return result;
}

bool Factory::supports(int32_t index) {
    const bool supported = callbacks_.system_supports(index);
    const int position = codec_position(index);
    if (active_factory != this || position < 0) return supported;
    collision_[position] = supported;
    return true;
}

void Factory::construct_extension(void* storage, int32_t index, bool source) {
    const int position = codec_position(index);
    if (active_factory != this || position < 0 || collision_[position]) {
        callbacks_.construct_system_extension(storage, index, source);
        return;
    }
    callbacks_.construct_base(storage, *codec_at(position));
    *static_cast<const CodecTable**>(storage) = &tables_[position];
    // The factory passes source=false for vendor indexes even though it then places them in the
    // source list, and the extension branch does not invoke the virtual init: initialize the source
    // object here.
    static_cast<uint8_t*>(storage)[0x1ff] = 1;
    constructed_[position] = tables_[position].init(storage);
}

bool Factory::constructed(int32_t index) const {
    const int position = codec_position(index);
    return position >= 0 && constructed_[position];
}

}  // namespace port
