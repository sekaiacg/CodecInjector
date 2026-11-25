#include "factory.h"

#include <cassert>
#include <cstring>
#include <thread>

#include "fake_codecs.h"

using namespace port;
namespace {
Factory* factory;
bool system_has_first;
bool system_init_result = true;
int base_calls;
int extension_calls;
const CodecPlugin* last_base;
alignas(8) uint8_t objects[2][kCodecObjectSize];

bool system_supports(int32_t index) { return index == 30 && system_has_first; }
void system_extension(void*, int32_t, bool) { ++extension_calls; }
void base(void* storage, const CodecPlugin& codec) {
    ++base_calls;
    last_base = &codec;
    std::memset(storage, 0, kCodecObjectSize);
}
bool system_init(void*) {
    assert(!factory->supports(6));
    if (!system_has_first) {
        // Other threads keep the provider's real answer.
        std::thread other([] { assert(!factory->supports(30)); });
        other.join();
    }
    assert(factory->supports(30) && factory->supports(31));
    factory->construct_extension(objects[0], 30, false);
    factory->construct_extension(objects[1], 31, false);
    return system_init_result;
}
}  // namespace

int main() {
    fake::use_both();
    CodecTable tables[] = {fake::table<0>(nullptr, nullptr), fake::table<1>(nullptr, nullptr)};
    Factory local({system_init, system_supports, system_extension, base}, tables);
    factory = &local;
    assert(!local.supports(30));
    assert(local.initialize(nullptr));
    assert(local.constructed(30) && local.constructed(31) && !local.constructed(6));
    assert(base_calls == 2 && extension_calls == 0 && last_base == &fake::plugin<1>);
    assert(*reinterpret_cast<const CodecTable**>(objects[0]) == &tables[0]);
    assert(*reinterpret_cast<const CodecTable**>(objects[1]) == &tables[1]);
    assert(objects[0][0x1ff] == 1 && objects[0][0x181] == 0xc0 && objects[1][0x181] == 0xc1);
    assert(!local.supports(30));
    // The provider already has index 30: that codec is left to the system, 31 is still added.
    system_has_first = true;
    assert(local.initialize(nullptr));
    assert(!local.constructed(30) && local.constructed(31));
    assert(base_calls == 3 && extension_calls == 1);
    system_has_first = false;
    system_init_result = false;
    assert(!local.initialize(nullptr));
    assert(!local.constructed(30) && !local.constructed(31));
}
