#include "ssc_registration.h"

namespace sscport {
namespace {
constexpr size_t kExtensionAllocation = 0x218;
RegistrationCallbacks callbacks{};
thread_local Factory* current_factory;

void destroy(void* object) { callbacks.destroy_base(object); }
void delete_object(void* object) {
    destroy(object);
    callbacks.free_object(object, kExtensionAllocation);
}
CodecTable codec_table = make_ssc_codec_table(destroy, delete_object);
}

void bind_registration(const RegistrationCallbacks& system_callbacks) {
    callbacks = system_callbacks;
}

bool initialize_registration(void* codecs) {
    Factory factory(callbacks.factory, &codec_table);
    Factory* previous = current_factory;
    current_factory = &factory;
    bool result = factory.initialize(codecs);
    current_factory = previous;
    return result;
}

bool registration_supports(int32_t index) {
    return current_factory ? current_factory->supports(index)
                           : callbacks.factory.system_supports(index);
}

void registration_construct(void* storage, int32_t index, bool source) {
    if (current_factory) {
        current_factory->construct_extension(storage, index, source);
    } else {
        callbacks.factory.construct_system_extension(storage, index, source);
    }
}

}  // namespace sscport
