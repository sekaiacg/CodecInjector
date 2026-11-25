#include "ssc_registration.h"

#include <cassert>
#include <cstring>

using namespace sscport;
namespace {
alignas(8) unsigned char objects[2][0x218];
bool collision;
int constructed;
int destroyed;
int freed;
int system_constructed;

bool supports(int32_t index) { return collision && index == 18; }
void system_construct(void*, int32_t, bool) { ++system_constructed; }
void base_construct(void* object) {
    ++constructed;
    std::memset(object, 0, sizeof(objects[0]));
}
void base_destroy(void*) { ++destroyed; }
void free_object(void*, size_t size) { assert(size == 0x218); ++freed; }
bool init(void* context) {
    int index = *static_cast<int*>(context);
    assert(registration_supports(18));
    registration_construct(objects[index], 18, false);
    return true;
}
}

int main() {
    bind_registration({{init, supports, system_construct, base_construct},
                       base_destroy, free_object});
    assert(!registration_supports(18));
    int first = 0;
    int second = 1;
    assert(initialize_registration(&first));
    assert(initialize_registration(&second));
    assert(constructed == 2 && system_constructed == 0);
    assert(!registration_supports(18));
    auto* first_table = *reinterpret_cast<const CodecTable**>(objects[0]);
    auto* second_table = *reinterpret_cast<const CodecTable**>(objects[1]);
    assert(first_table == second_table);
    assert(objects[0][0x1ff] == 1 && objects[1][0x181] == 0x0c);
    first_table->destroy(objects[0]);
    second_table->delete_object(objects[1]);
    assert(destroyed == 2 && freed == 1);
    collision = true;
    assert(initialize_registration(&first));
    assert(system_constructed == 1 && constructed == 2);
    registration_construct(objects[0], 6, true);
    assert(system_constructed == 2);
}
