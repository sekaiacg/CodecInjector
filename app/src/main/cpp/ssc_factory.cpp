#include "ssc_factory.h"

namespace sscport {
namespace {
constexpr int32_t kSscIndex = 18;
thread_local Factory* active_factory = nullptr;
}

bool Factory::initialize(void* codecs) {
    // Scope the synthetic factory answer to the thread executing init. Other
    // calls continue to see the provider's actual capabilities.
    Factory* previous = active_factory;
    active_factory = this;
    collision_ = false;
    constructed_ = false;
    const bool result = callbacks_.initialize_system(codecs);
    active_factory = previous;
    constructed_ = result && constructed_ && !collision_;
    return result;
}

bool Factory::supports(int32_t index) {
    const bool supported = callbacks_.system_supports(index);
    if (active_factory != this || index != kSscIndex) return supported;
    collision_ = supported;
    return true;
}

void Factory::construct_extension(void* storage, int32_t index, bool source) {
    if (active_factory != this || index != kSscIndex || collision_) {
        callbacks_.construct_system_extension(storage, index, source);
        return;
    }
    callbacks_.construct_ssc_base(storage);
    *static_cast<const CodecTable**>(storage) = table_;
    // The current factory passes false for index 18 even though it subsequently
    // places this index in the source list. Initialize the source object here:
    // the extension branch does not invoke its virtual init method.
    static_cast<uint8_t*>(storage)[0x1ff] = 1;
    constructed_ = table_->init(storage);
}

}  // namespace sscport
