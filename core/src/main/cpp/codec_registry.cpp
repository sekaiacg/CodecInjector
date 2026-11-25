#include "codec_plugin.h"

namespace port {
namespace {
const CodecPlugin* codecs[kMaxCodecs];
size_t count;
}  // namespace

void set_codecs(const CodecPlugin* const* list, size_t size) {
    count = 0;
    for (size_t i = 0; i < size && count < kMaxCodecs; ++i) {
        if (list[i]) codecs[count++] = list[i];
    }
}

size_t codec_count() { return count; }

const CodecPlugin* codec_at(size_t position) { return position < count ? codecs[position] : nullptr; }

int codec_position(int32_t index) {
    for (size_t i = 0; i < count; ++i) {
        if (codecs[i]->index == index) return static_cast<int>(i);
    }
    return -1;
}

const CodecPlugin* codec_for_index(int32_t index) {
    int position = codec_position(index);
    return position < 0 ? nullptr : codecs[position];
}

const CodecPlugin* codec_for_info(const uint8_t* codec_info) {
    if (!codec_info) return nullptr;
    for (size_t i = 0; i < count; ++i) {
        if (codecs[i]->matches(codec_info)) return codecs[i];
    }
    return nullptr;
}

}  // namespace port
