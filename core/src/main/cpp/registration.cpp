#include "registration.h"

namespace port {
namespace {
RegistrationCallbacks callbacks{};
CodecTable tables[kMaxCodecs];
thread_local Factory* current_factory;

void destroy(void* object) { callbacks.destroy_base(object); }
void delete_object(void* object) {
    destroy(object);
    callbacks.free_object(object, kCodecObjectSize);
}
}  // namespace

void bind_registration(const RegistrationCallbacks& system_callbacks) {
    callbacks = system_callbacks;
    for (size_t i = 0; i < codec_count(); ++i) {
        tables[i] = codec_at(i)->make_table(destroy, delete_object);
    }
}

bool initialize_registration(void* codecs) {
    Factory factory(callbacks.factory, tables);
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

}  // namespace port
