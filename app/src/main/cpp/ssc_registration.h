#pragma once

#include "ssc_factory.h"
#include <stddef.h>

namespace sscport {

// All callbacks are supplied by the eventual system adapter. The caller
// allocates objects; destroy_base and free_object must match that allocator.
struct RegistrationCallbacks {
    FactoryCallbacks factory;
    void (*destroy_base)(void* object);
    void (*free_object)(void* object, size_t size);
};

// Single process-wide binding, established before any system callback.
// Rebinding while objects exist is not supported.
void bind_registration(const RegistrationCallbacks& callbacks);
bool initialize_registration(void* codecs);
bool registration_supports(int32_t index);
void registration_construct(void* storage, int32_t index, bool source);

}  // namespace sscport
