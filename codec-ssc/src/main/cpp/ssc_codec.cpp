// SSC as a core codec plugin: SSC codec information and negotiation, and the software encoder (the
// open SSC encoder in open/), with Dolby spatial rendering before encoding.

#include "ssc_codec.h"

#include <string.h>

#include "port_api.h"
#include "ssc_codec_state.h"
#include "ssc_codec_table.h"
#include "ssc_interface.h"
#include "ssc_negotiation.h"
#include "ssc_open.h"

namespace ssc {
namespace {
using port::info;

unsigned int read_logs;
unsigned int encode_logs;
unsigned int enqueue_logs;
unsigned int spatial_logs;
int32_t logged_bitrate;
Read system_read;
Enqueue system_enqueue;
Encode real_encode;
const EncoderInterface* underlying_encoder;
EncoderInterface observed_encoder{};

bool first(unsigned int* counter, unsigned int limit) {
    return __atomic_fetch_add(counter, 1U, __ATOMIC_RELAXED) < limit;
}

uint32_t observe_pcm_read(uint8_t* output, uint32_t size) {
    uint32_t result = system_read(output, size);
    if (first(&read_logs, 3U)) info("ssc_pcm_read", static_cast<int>(result), static_cast<int>(size));
    return result;
}

int32_t observe_encode(void* encoder, const void* pcm, int32_t samples, uint8_t* output,
                       int32_t bitrate) {
    int32_t rendered[kSamplesPerChannel * kChannels];
    if (samples == static_cast<int32_t>(kSamplesPerChannel)) {
        memcpy(rendered, pcm, sizeof(rendered));
        port::spatial().process(rendered, kSamplesPerChannel);
        pcm = rendered;
        if (first(&spatial_logs, 1U)) info("spatial_process", port::spatial().running(), samples);
    }
    if (bitrate != logged_bitrate) {
        logged_bitrate = bitrate;
        info("ssc_bitrate", bitrate, samples);
    }
    int32_t result = real_encode(encoder, pcm, samples, output, bitrate);
    if (first(&encode_logs, 3U)) info("ssc_encode", result, samples);
    return result;
}

bool observe_enqueue(port::BtHdr* packet, size_t frames, uint32_t pcm_bytes) {
    bool result = system_enqueue(packet, frames, pcm_bytes);
    if (first(&enqueue_logs, 3U)) info("ssc_enqueue", result, static_cast<int>(frames));
    return result;
}

void observe_encoder_init(const PeerParams* peer, void* codec, Read read, Enqueue enqueue) {
    system_read = read;
    system_enqueue = enqueue;
    read_logs = encode_logs = enqueue_logs = spatial_logs = 0;
    logged_bitrate = 0;
    const bool uhq = codec && codec_is_uhq(codec);
    const uint32_t rate = uhq ? 96000 : 48000;
    info("dolby_started", port::spatial().start(rate, kSamplesPerChannel), uhq);
    info("ssc_encoder_init", peer->mtu, static_cast<int>(rate));
    // The stack's view of the link: peer EDR support and 3 Mbps EDR (off when either side lacks it
    // or the peer is on the stack's 2 Mbps-only interop list). The controller still picks each
    // packet's type by link quality.
    info("ssc_link", peer->is_edr, peer->supports_3mbps);
    port::note_encoder_rate(rate);
    underlying_encoder->init(peer, codec, observe_pcm_read, observe_enqueue);
}

void observe_encoder_cleanup() {
    port::spatial().stop();
    underlying_encoder->cleanup();
    info("ssc_encoder_cleanup", 0, 0);
}

bool bind_encoder(const port::StackApi& stack) {
    if (!stack.allocate) {
        info("ssc_encoder_unavailable", 0, 0);
        return false;
    }
    real_encode = ssc_open_encode;
    underlying_encoder = bind({ssc_open_create, ssc_open_destroy, observe_encode, stack.allocate});
    observed_encoder = *underlying_encoder;
    observed_encoder.init = observe_encoder_init;
    observed_encoder.cleanup = observe_encoder_cleanup;
    return true;
}

bool source_valid(const uint8_t* codec) {
    // Both ordinary SSC (48 kHz) and SSC UHQ (96 kHz) capabilities and configurations are valid.
    return is_ssc_codec(codec);
}

bool init_capability(uint8_t* capability) {
    make_48k_capability(capability);
    return true;
}

const EncoderInterface* encoder() { return &observed_encoder; }

// SSC packets carry no codec header in front of the frame.
bool build_header(const uint8_t*, port::BtHdr*, uint16_t) { return true; }

// The packetizer stores the frame sequence number at the start of the packet data.
bool packet_timestamp(const uint8_t*, const uint8_t* data, uint32_t* timestamp) {
    memcpy(timestamp, data, sizeof(*timestamp));
    return true;
}

const port::CodecPlugin kPlugin{
    "SSC",
    18,
    UINT64_C(0x00000001030075ff),
    1000000,  // selected on a fresh connection, so the stack starts its software session
    make_ssc_codec_table,
    bind_encoder,
    is_ssc_codec,
    source_valid,
    init_capability,
    encoder,
    build_header,
    packet_timestamp,
};
}  // namespace

const port::CodecPlugin* plugin() { return &kPlugin; }

}  // namespace ssc
