// AAC-ELD as a core codec plugin: Apple's codec information and negotiation, and a software
// encoder (the stack's FDK, Apple's AU framing) with Dolby spatial rendering before encoding.

#include "aeld_codec.h"

#include <string.h>

#include "aeld_codec_info.h"
#include "aeld_codec_state.h"
#include "aeld_fdk.h"
#include "aeld_packet.h"
#include "aeld_source.h"
#include "port_api.h"

namespace aeld {
namespace {
using port::EncoderInterface;
using port::info;
using port::PeerParams;

FdkApi fdk_api{};
EldEncoder encoder;
Packetizer packets;
Source source;
Read read_pcm;
Enqueue system_enqueue;
Allocate allocate;
uint16_t peer_mtu;
bool faulted;
unsigned int enqueue_logs;
uint32_t enqueued;

bool first(unsigned int* counter, unsigned int limit) {
    return __atomic_fetch_add(counter, 1U, __ATOMIC_RELAXED) < limit;
}

int16_t to_pcm16(int32_t q823) {
    const int32_t value = (q823 + 0x80) >> 8;
    return static_cast<int16_t>(value > 32767 ? 32767 : value < -32768 ? -32768 : value);
}

// Dolby works on Q8.23 samples (the SSC encoder's input format).
void spatialize(int16_t* pcm, size_t frames) {
    if (!port::spatial().running()) return;
    int32_t samples[kFrameSamples * 2];
    for (size_t i = 0; i < frames * 2; ++i) samples[i] = int32_t{pcm[i]} * 256;
    port::spatial().process(samples, frames);
    for (size_t i = 0; i < frames * 2; ++i) pcm[i] = to_pcm16(samples[i]);
}

int encode_frame(void*, const int16_t* pcm, uint8_t* out, size_t capacity) {
    return encoder.encode(pcm, out, capacity);
}

bool observe_enqueue(port::BtHdr* packet, size_t frames, uint32_t pcm_bytes) {
    const bool result = system_enqueue(packet, frames, pcm_bytes);
    ++enqueued;
    if (first(&enqueue_logs, 3U)) info("aeld_enqueue", result, packet->len);
    return result;
}

void cleanup() {
    port::spatial().stop();
    encoder.close();
    packets.reset();
    source.reset();
    read_pcm = nullptr;
    peer_mtu = 0;
    faulted = false;
}

void init(const PeerParams* peer, void*, Read read, Enqueue enqueue) {
    cleanup();
    if (!encoder.open()) {
        info("aeld_encoder_open_failed", static_cast<int>(encoder.last_error()), 0);
        return;
    }
    read_pcm = read;
    system_enqueue = enqueue;
    peer_mtu = peer->mtu;
    packets = Packetizer(peer_mtu, allocate, observe_enqueue);
    enqueue_logs = 0;
    enqueued = 0;
    // No head tracking for AirPods yet: render facing forward.
    port::spatial().set_yaw(0.0f);
    info("dolby_started", port::spatial().start(48000, kFrameSamples), 0);
    info("aeld_encoder_init", peer_mtu, static_cast<int>(packets.capacity()));
    port::note_encoder_rate(48000);
}

// Suspend / start: AAC-ELD predicts across frames, so after a gap the decoder only recovers from a
// fresh encoder and a new AU sequence.
void feeding_reset() {
    source.reset();
    packets.reset();
    if (encoder.is_open() && !encoder.open()) {
        info("aeld_encoder_open_failed", static_cast<int>(encoder.last_error()), 1);
    }
    faulted = false;
}

void feeding_flush() {
    source.reset();
    packets.discard();
}

uint64_t interval_ms() { return 10; }
int effective_frame_size() { return static_cast<int>(packets.capacity()); }

void send_frames(uint64_t timestamp_us) {
    if (!encoder.is_open() || !read_pcm || faulted) return;
    const Result result = source.tick(timestamp_us, read_pcm, spatialize, encode_frame, nullptr,
                                      &packets);
    if (result == Result::kReadError || result == Result::kEncodeError ||
        result == Result::kSendError) {
        faulted = true;
        info("aeld_fault", static_cast<int>(result), static_cast<int>(encoder.last_error()));
    }
}

void set_transmit_queue_length(size_t) {}

const EncoderInterface kEncoder{
    init, cleanup, feeding_reset, feeding_flush, interval_ms, effective_frame_size,
    send_frames, set_transmit_queue_length,
};

bool bind(const port::StackApi& stack) {
    if (!stack.allocate || !resolve_fdk(stack.image, stack.base, &fdk_api)) {
        info("aeld_fdk_unavailable", 0, 0);
        return false;
    }
    allocate = stack.allocate;
    encoder.bind(fdk_api);
    return true;
}

bool source_valid(const uint8_t* codec) { return supports_profile(codec); }

bool init_capability(uint8_t* capability) {
    make_capability(capability);
    return true;
}

const EncoderInterface* get_encoder() { return &kEncoder; }

// No codec header: the payload is the AU-header/AU sequence.
bool build_header(const uint8_t*, port::BtHdr*, uint16_t) { return true; }

bool packet_timestamp(const uint8_t*, const uint8_t* data, uint32_t* timestamp) {
    memcpy(timestamp, data, sizeof(*timestamp));
    return true;
}

const port::CodecPlugin kPlugin{
    "AAC-ELD",
    kIndex,
    UINT64_C(0x0000008001004cff),  // 0xff | vendor << 8 | codec << 24
    1000000,  // AirPods offer AAC and AAC-ELD; prefer AAC-ELD
    make_codec_table,
    bind,
    is_aac_eld,
    source_valid,
    init_capability,
    get_encoder,
    build_header,
    packet_timestamp,
};
}  // namespace

const port::CodecPlugin* plugin() { return &kPlugin; }

}  // namespace aeld
