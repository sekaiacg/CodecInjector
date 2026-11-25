#include "vendor_dispatch.h"

#include <string.h>

namespace port {

int32_t VendorDispatch::source_index(const uint8_t* codec) const {
    const CodecPlugin* plugin = codec_for_info(codec);
    return plugin ? plugin->index : system_.source_index(codec);
}

bool VendorDispatch::source_valid(const uint8_t* codec) const {
    const CodecPlugin* plugin = codec_for_info(codec);
    return plugin ? plugin->source_valid(codec) : system_.source_valid(codec);
}

bool VendorDispatch::init_capability(int32_t index, void* capability) const {
    const CodecPlugin* plugin = codec_for_index(index);
    if (!plugin) return system_.init_capability(index, capability);
    auto* bytes = static_cast<uint8_t*>(capability);
    memset(bytes, 0, kCodecInfoSize);
    return plugin->init_capability(bytes);
}

const EncoderInterface* VendorDispatch::encoder(const uint8_t* codec) const {
    const CodecPlugin* plugin = codec_for_info(codec);
    return plugin ? plugin->encoder() : system_.encoder(codec);
}

bool VendorDispatch::build_header(const uint8_t* codec, BtHdr* packet, uint16_t frames) const {
    const CodecPlugin* plugin = codec_for_info(codec);
    return plugin ? plugin->build_header(codec, packet, frames)
                  : system_.build_header(codec, packet, frames);
}

bool VendorDispatch::packet_timestamp(const uint8_t* codec, const uint8_t* data,
                                      uint32_t* timestamp) const {
    const CodecPlugin* plugin = codec_for_info(codec);
    return plugin ? plugin->packet_timestamp(codec, data, timestamp)
                  : system_.packet_timestamp(codec, data, timestamp);
}

}  // namespace port
