#include "registration.h"

#include <cassert>
#include <cstring>

#include "fake_codecs.h"

using namespace port;
namespace {
alignas(8) unsigned char objects[2][kCodecObjectSize];
bool collision;
int constructed;
int destroyed;
int freed;
int system_constructed;

bool supports(int32_t index) { return collision && index == 30; }
void system_construct(void*, int32_t, bool) { ++system_constructed; }
void base_construct(void* object, const CodecPlugin&) {
    ++constructed;
    std::memset(object, 0, kCodecObjectSize);
}
void base_destroy(void*) { ++destroyed; }
void free_object(void*, size_t size) {
    assert(size == kCodecObjectSize);
    ++freed;
}
bool init(void*) {
    assert(registration_supports(30) && registration_supports(31));
    registration_construct(objects[0], 30, false);
    registration_construct(objects[1], 31, false);
    return true;
}
}  // namespace

int main() {
    fake::use_both();
    bind_registration({{init, supports, system_construct, base_construct}, base_destroy, free_object});
    assert(!registration_supports(30));
    assert(initialize_registration(nullptr));
    assert(constructed == 2 && system_constructed == 0);
    assert(!registration_supports(30));
    auto* first = *reinterpret_cast<const CodecTable**>(objects[0]);
    auto* second = *reinterpret_cast<const CodecTable**>(objects[1]);
    assert(first != second && first->init == fake::init<0> && second->init == fake::init<1>);
    assert(objects[0][0x1ff] == 1 && objects[1][0x181] == 0xc1);
    first->destroy(objects[0]);
    second->delete_object(objects[1]);
    assert(destroyed == 2 && freed == 1);
    collision = true;
    assert(initialize_registration(nullptr));
    assert(system_constructed == 1 && constructed == 3);
    registration_construct(objects[0], 6, true);
    assert(system_constructed == 2);
}
