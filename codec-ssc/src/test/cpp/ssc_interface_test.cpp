#include "ssc_interface.h"

#include <cassert>
#include <cstdlib>
#include <cstring>

using namespace sscport;

static int created;
static int destroyed;
static int reads;
static int packets;
static bool fail_enqueue;
static bool fail_encode;

static void* create(int32_t rate, int32_t channels, int32_t mode, int32_t* error) {
    assert(rate == 44100 && channels == 2 && mode == 24);
    *error = 0;
    ++created;
    return &created;
}

static void destroy(void* encoder) {
    assert(encoder == &created);
    ++destroyed;
}

static uint32_t read_pcm(uint8_t* data, uint32_t size) {
    ++reads;
    std::memset(data, 0, size);
    return size;
}

static int32_t encode(void* encoder, const void*, int32_t samples,
                      uint8_t* output, int32_t bitrate) {
    assert(encoder == &created && samples == 864 && bitrate == 192000);
    if (fail_encode) return 4097;
    std::memset(output, 1, 484);
    return 484;
}

static bool enqueue(BtHdr* packet, size_t frames, uint32_t pcm_bytes) {
    assert(packet->len == 484 && packet->offset == 23 && frames == 1);
    assert(pcm_bytes == kPcmBytes);
    ++packets;
    std::free(packet);
    return !fail_enqueue;
}

int main() {
    const Dependencies dependencies{create, destroy, encode, std::malloc};
    const EncoderInterface* iface = bind(dependencies);
    const PeerParams peer{true, true, 1005};
    iface->init(&peer, nullptr, read_pcm, enqueue);
    assert(created == 1 && iface->interval_ms() == 10);
    assert(iface->effective_frame_size() == 1005);
    iface->send_frames(10000);
    assert(packets == 0 && reads == 0);
    iface->send_frames(20000);
    assert(packets == 1 && reads == 1);
    fail_enqueue = true;
    iface->send_frames(40000);
    assert(packets == 2 && reads == 2);
    iface->send_frames(60000);
    assert(packets == 2 && reads == 2);
    fail_enqueue = false;
    iface->feeding_reset();
    iface->send_frames(70000);
    iface->send_frames(80000);
    assert(packets == 3 && reads == 3);
    iface->cleanup();
    assert(destroyed == 1);
    iface->send_frames(90000);
    assert(packets == 3);

    iface->init(&peer, nullptr, read_pcm, enqueue);
    fail_encode = true;
    iface->send_frames(10000);
    iface->send_frames(20000);
    assert(packets == 3);
    iface->send_frames(40000);
    assert(packets == 3);
    iface->cleanup();
    assert(destroyed == 2);

    fail_encode = false;
    iface->init(&peer, nullptr, read_pcm, enqueue);
    iface->send_frames(10000);
    const int before_delay = packets;
    iface->send_frames(210000);
    assert(packets == before_delay + 4);
    iface->send_frames(220000);
    assert(packets == before_delay + 8);
    iface->cleanup();
    assert(destroyed == 3);
}
