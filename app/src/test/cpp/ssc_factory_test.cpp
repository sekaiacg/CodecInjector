#include "ssc_factory.h"

#include <cassert>
#include <cstring>
#include <thread>

using namespace sscport;
namespace {
Factory* factory;
bool system_has_ssc;
bool system_init_result = true;
int base_calls;
int extension_calls;
alignas(8) uint8_t object[0x218];

bool system_supports(int32_t index) { return index == 18 && system_has_ssc; }
void system_extension(void*, int32_t, bool) { ++extension_calls; }
void ssc_base(void* storage) {
    ++base_calls;
    std::memset(storage, 0, sizeof(object));
}
bool system_init(void*) {
    assert(!factory->supports(6));
    if (!system_has_ssc) {
        std::thread other([] { assert(!factory->supports(18)); });
        other.join();
    }
    assert(factory->supports(18));
    factory->construct_extension(object, 18, false);
    return system_init_result;
}
}

int main() {
    CodecTable table = make_ssc_codec_table(nullptr, nullptr);
    Factory local({system_init, system_supports, system_extension, ssc_base}, &table);
    factory = &local;
    assert(!local.supports(18));
    assert(local.initialize(nullptr));
    assert(local.constructed() && base_calls == 1 && extension_calls == 0);
    assert(*reinterpret_cast<const CodecTable**>(object) == &table);
    assert(object[0x1ff] == 1 && object[0x181] == 0x0c);
    assert(!local.supports(18));
    system_has_ssc = true;
    assert(local.initialize(nullptr));
    assert(!local.constructed() && base_calls == 1 && extension_calls == 1);
    system_has_ssc = false;
    system_init_result = false;
    assert(!local.initialize(nullptr));
    assert(!local.constructed());
    assert(!local.supports(18));
}
