#include "aeld_fdk.h"

#include "elf_symbols.h"

namespace aeld {

bool resolve_fdk(const char* image, uintptr_t base, FdkApi* api) {
    void* open = nullptr;
    void* close = nullptr;
    void* set = nullptr;
    void* encode = nullptr;
    port::SymbolQuery queries[] = {
        {"aacEncOpen", &open},
        {"aacEncClose", &close},
        {"aacEncoder_SetParam", &set},
        {"aacEncEncode", &encode},
    };
    if (port::resolve_symbols(image, base, queries, 4) != 4) return false;
    api->open = reinterpret_cast<fdk::Open>(open);
    api->close = reinterpret_cast<fdk::Close>(close);
    api->set_param = reinterpret_cast<fdk::SetParam>(set);
    api->encode = reinterpret_cast<fdk::Encode>(encode);
    return true;
}

bool EldEncoder::open() {
    close();
    if (!api_.open) return false;
    fdk::Handle handle = nullptr;
    // Modules 0x01: the AAC core only.
    error_ = api_.open(&handle, 0x01, 2);
    if (error_ != 0 || !handle) return false;
    const struct {
        uint32_t param;
        uint32_t value;
    } params[] = {
        {fdk::kAot, 39},
        {fdk::kSampleRate, 48000},
        {fdk::kChannelMode, 2},
        {fdk::kBitrate, kEncoderBitrate},
        {fdk::kBitrateMode, 0},  // CBR
        {fdk::kGranuleLength, kFrameSamples},
        {fdk::kSbrMode, 0},
        {fdk::kAfterburner, 0},
        {fdk::kTransmux, 0},  // raw access units
    };
    for (const auto& p : params) {
        error_ = api_.set_param(handle, p.param, p.value);
        if (error_ != 0) {
            api_.close(&handle);
            return false;
        }
    }
    // Applies the configuration.
    error_ = api_.encode(handle, nullptr, nullptr, nullptr, nullptr);
    if (error_ != 0) {
        api_.close(&handle);
        return false;
    }
    handle_ = handle;
    return true;
}

void EldEncoder::close() {
    if (handle_) api_.close(&handle_);
    handle_ = nullptr;
}

int EldEncoder::encode(const int16_t* pcm, uint8_t* out, size_t capacity) {
    if (!handle_) return -1;
    void* in_buffer = const_cast<int16_t*>(pcm);
    int32_t in_id = fdk::kInAudioData;
    int32_t in_size = static_cast<int32_t>(kFrameSamples * 2 * sizeof(int16_t));
    int32_t in_element = sizeof(int16_t);
    void* out_buffer = out;
    int32_t out_id = fdk::kOutBitstreamData;
    int32_t out_size = static_cast<int32_t>(capacity);
    int32_t out_element = 1;
    const fdk::BufDesc in{1, &in_buffer, &in_id, &in_size, &in_element};
    const fdk::BufDesc output{1, &out_buffer, &out_id, &out_size, &out_element};
    const fdk::InArgs args{static_cast<int32_t>(kFrameSamples * 2), 0};
    fdk::OutArgs result{};
    error_ = api_.encode(handle_, &in, &output, &args, &result);
    if (error_ != 0 || result.bytes < 0 || static_cast<size_t>(result.bytes) > capacity) return -1;
    return result.bytes;
}

}  // namespace aeld
