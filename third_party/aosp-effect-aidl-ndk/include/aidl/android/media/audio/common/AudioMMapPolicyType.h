/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: $ANDROID_HOME/build-tools/37.0.0/aidl --lang=ndk --structured --stability=vintf --min_sdk_version=31 --version=4 --hash=af71e6ae2c6861fc2b09bb477e7285e6777cd41c -I effect3 -I types4 -I fmq1 -I common2 -I acommon4 -I eraser1 -o gen/src -h gen/include types4/android/media/audio/common/AudioMMapPolicyType.aidl
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
namespace common {
enum class AudioMMapPolicyType : int32_t {
  DEFAULT = 1,
  EXCLUSIVE = 2,
};

}  // namespace common
}  // namespace audio
}  // namespace media
}  // namespace android
}  // namespace aidl
namespace aidl {
namespace android {
namespace media {
namespace audio {
namespace common {
[[nodiscard]] static inline std::string toString(AudioMMapPolicyType val) {
  switch(val) {
  case AudioMMapPolicyType::DEFAULT:
    return "DEFAULT";
  case AudioMMapPolicyType::EXCLUSIVE:
    return "EXCLUSIVE";
  default:
    return std::to_string(static_cast<int32_t>(val));
  }
}
}  // namespace common
}  // namespace audio
}  // namespace media
}  // namespace android
}  // namespace aidl
namespace ndk {
namespace internal {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++17-extensions"
template <>
constexpr inline std::array<aidl::android::media::audio::common::AudioMMapPolicyType, 2> enum_values<aidl::android::media::audio::common::AudioMMapPolicyType> = {
  aidl::android::media::audio::common::AudioMMapPolicyType::DEFAULT,
  aidl::android::media::audio::common::AudioMMapPolicyType::EXCLUSIVE,
};
#pragma clang diagnostic pop
}  // namespace internal
}  // namespace ndk
