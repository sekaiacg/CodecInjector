/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: $ANDROID_HOME/build-tools/37.0.0/aidl --lang=ndk --structured --stability=vintf --min_sdk_version=31 --version=1 --hash=d0f24b98624bc07be92c5dc38302f967c522ff14 -I effect3 -I types4 -I fmq1 -I common2 -I acommon4 -I eraser1 -o gen/src -h gen/include eraser1/android/media/audio/eraser/Mode.aidl
 *
 * DO NOT CHECK THIS FILE INTO A CODE TREE (e.g. git, etc..).
 * ALWAYS GENERATE THIS FILE FROM UPDATED AIDL COMPILER
 * AS A BUILD INTERMEDIATE ONLY. THIS IS NOT SOURCE CODE.
 */
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <android/binder_enums.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl {
namespace android {
namespace media {
namespace audio {
namespace eraser {
enum class Mode : int8_t {
  ERASER = 0,
  CLASSIFIER = 1,
};

}  // namespace eraser
}  // namespace audio
}  // namespace media
}  // namespace android
}  // namespace aidl
namespace aidl {
namespace android {
namespace media {
namespace audio {
namespace eraser {
[[nodiscard]] static inline std::string toString(Mode val) {
  switch(val) {
  case Mode::ERASER:
    return "ERASER";
  case Mode::CLASSIFIER:
    return "CLASSIFIER";
  default:
    return std::to_string(static_cast<int8_t>(val));
  }
}
}  // namespace eraser
}  // namespace audio
}  // namespace media
}  // namespace android
}  // namespace aidl
namespace ndk {
namespace internal {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++17-extensions"
template <>
constexpr inline std::array<aidl::android::media::audio::eraser::Mode, 2> enum_values<aidl::android::media::audio::eraser::Mode> = {
  aidl::android::media::audio::eraser::Mode::ERASER,
  aidl::android::media::audio::eraser::Mode::CLASSIFIER,
};
#pragma clang diagnostic pop
}  // namespace internal
}  // namespace ndk
