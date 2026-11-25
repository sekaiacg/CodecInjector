#include "ssc_interface.h"
#include "ssc_codec_state.h"
#include "ssc_bitrate.h"

namespace sscport {
namespace {

Dependencies deps{};
Source source(48000);
Packetizer packets(0, nullptr, nullptr);
Read read_pcm;
void* encoder;
uint16_t peer_mtu;
uint64_t previous_tick;
BitrateLadder ladder;
// The stack sends one queued packet per timer tick. A frame is 864 samples per channel: 18 ms at
// 48 kHz (under one frame per 10 ms tick) but 9 ms at 96 kHz, so UHQ ticks every 9 ms; with a
// 10 ms tick the transmit queue grows by about 0.11 packets per tick until the stack flushes it.
uint64_t tick_ms = 10;
uint32_t quality_mode = BitrateLadder::kAdaptive;
bool faulted;

void cleanup() {
    if (encoder) deps.destroy(encoder);
    encoder = nullptr;
    read_pcm = nullptr;
    peer_mtu = 0;
    previous_tick = 0;
    faulted = false;
    source.reset();
}

void init(const PeerParams* peer, void* codec, Read read, Enqueue enqueue) {
    cleanup();
    // Samsung's stack: create(96000, ...) for SSC UHQ, create(44100, ...) for ordinary 48 kHz SSC;
    // 864 samples per channel per frame in both. UHQ uses the top of its bitrate table.
    const bool uhq = codec && codec_is_uhq(codec);
    source.set_sample_rate(uhq ? 96000 : 48000);
    ladder.reset(uhq);
    tick_ms = uhq ? 9 : 10;
    int32_t error = 0;
    encoder = deps.create(uhq ? 96000 : 44100, 2, 24, &error);
    if (!encoder || error != 0) {
        if (encoder) deps.destroy(encoder);
        encoder = nullptr;
        return;
    }
    read_pcm = read;
    peer_mtu = peer->mtu;
    packets = Packetizer(peer_mtu, deps.allocate, enqueue);
}

void reset() {
    source.reset();
    packets.reset();
    previous_tick = 0;
    faulted = false;
}

uint64_t interval_ms() { return tick_ms; }
int effective_frame_size() { return peer_mtu; }

void send_frames(uint64_t timestamp_us) {
    if (!encoder || faulted) return;
    const uint64_t elapsed = previous_tick && timestamp_us >= previous_tick
            ? timestamp_us - previous_tick : tick_ms * 1000;
    previous_tick = timestamp_us;
    const int32_t bitrate = ladder.bitrate(static_cast<BitrateLadder::Mode>(
            __atomic_load_n(&quality_mode, __ATOMIC_RELAXED)));
    const Result result = source.tick(elapsed, read_pcm, deps.encode, encoder,
                                      bitrate, Packetizer::emit_callback, &packets);
    faulted = result == Result::kReadError || result == Result::kEncodeError ||
              result == Result::kEmitError;
}

void set_transmit_queue_length(size_t length) { ladder.report_queue(length); }

const EncoderInterface interface{
    init, cleanup, reset, reset, interval_ms, effective_frame_size,
    send_frames, set_transmit_queue_length,
};

}  // namespace

void set_quality_mode(uint32_t mode) {
    __atomic_store_n(&quality_mode, mode <= BitrateLadder::kFixedLow ? mode
                                                                   : BitrateLadder::kAdaptive,
                     __ATOMIC_RELAXED);
}

const EncoderInterface* bind(const Dependencies& dependencies) {
    deps = dependencies;
    return &interface;
}

}  // namespace sscport
