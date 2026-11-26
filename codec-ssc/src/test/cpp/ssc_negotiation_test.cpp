#include "ssc_negotiation.h"

#include <cassert>
#include <cstring>

using namespace ssc;

int main() {
    uint8_t local[10]{};
    make_48k_capability(local);
    assert(local[9] == 0x0c);
    uint8_t peer[10];
    std::memcpy(peer, local, sizeof(peer));
    peer[9] = 0x3e;
    uint8_t selected[10]{};
    assert(select_48k_capability(peer, selected));
    assert(selected[9] == 0x0c && std::memcmp(selected, local, sizeof(local)) == 0);
    peer[9] = 0x04;
    assert(select_48k_capability(peer, selected));
    assert(selected[9] == 0x04);
    peer[9] = 0x0a;
    assert(!select_48k_capability(peer, selected));
    peer[9] = 0x3e;
    peer[3] = 0x00;
    assert(!select_48k_capability(peer, selected));
}
