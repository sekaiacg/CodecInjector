#include "ssc_factory.h"
#include "ssc_negotiation.h"
#include "ssc_vendor_dispatch.h"

#include <cassert>
#include <cstdlib>
#include <cstring>

using namespace sscport;

namespace {
alignas(8) uint8_t object[0x218];
uint8_t selected[45];
Factory* factory;
VendorDispatch* dispatch;
int encoded_frames;
int sent_frames;
int destroys;

bool system_supports(int32_t) { return false; }
void system_extension(void*, int32_t, bool) { assert(false); }
void base(void* storage) { std::memset(storage, 0, sizeof(object)); }
bool system_init(void*) {
    assert(factory->supports(18));
    factory->construct_extension(object, 18, false);
    return true;
}
int32_t other_index(const uint8_t*) { return 29; }
bool other_valid(const uint8_t*) { return false; }
bool other_capability(int32_t, void*) { return false; }
const EncoderInterface* other_encoder(const uint8_t*) { return nullptr; }
bool other_header(const uint8_t*, BtHdr*, uint16_t) { return false; }
bool other_timestamp(const uint8_t*, const uint8_t*, uint32_t*) { return false; }
void* create(int32_t rate, int32_t channels, int32_t bits, int32_t* error) {
    assert(rate == 44100 && channels == 2 && bits == 24);
    *error = 0;
    return &encoded_frames;
}
void destroy(void*) { ++destroys; }
uint32_t read(uint8_t* data, uint32_t size) {
    std::memset(data, 0, size);
    return size;
}
int32_t encode(void*, const void*, int32_t samples, uint8_t* output, int32_t) {
    assert(samples == 864);
    ++encoded_frames;
    std::memset(output, 0x42, 484);
    return 484;
}
bool enqueue(BtHdr* packet, size_t frames, uint32_t) {
    assert(frames == 1 && packet->len == 484);
    uint32_t timestamp = 0;
    auto* data = reinterpret_cast<uint8_t*>(packet + 1);
    assert(dispatch->packet_timestamp(selected, data, &timestamp));
    assert(timestamp == static_cast<uint32_t>(sent_frames + 1));
    assert(dispatch->build_header(selected, packet, 1));
    assert(data[packet->offset] == 0x42);
    ++sent_frames;
    std::free(packet);
    return true;
}
}

int main() {
    CodecTable table = make_ssc_codec_table(nullptr, nullptr);
    Factory local_factory({system_init, system_supports, system_extension, base}, &table);
    factory = &local_factory;
    assert(local_factory.initialize(nullptr) && local_factory.constructed());

    const auto* encoder = bind({create, destroy, encode, std::malloc});
    VendorDispatch local_dispatch({other_index, other_valid, other_capability,
                                   other_encoder, other_header, other_timestamp}, encoder);
    dispatch = &local_dispatch;
    uint8_t peer[45]{};
    make_48k_capability(peer);
    peer[9] = 0x3e;
    assert(dispatch->source_index(peer) == 18 && dispatch->source_valid(peer));
    assert(table.peer_capability(object, peer));
    assert(table.select(object, peer, true, selected) == 0 && selected[9] == 0x0c);
    assert(dispatch->encoder(selected) == encoder);

    const PeerParams params{true, true, 1005};
    encoder->init(&params, object, read, enqueue);
    for (uint64_t t = 10000; t <= 1000000; t += 10000) encoder->send_frames(t);
    encoder->cleanup();
    assert(encoded_frames == 55 && sent_frames == 55 && destroys == 1);
}
