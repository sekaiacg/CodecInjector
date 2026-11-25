/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: $ANDROID_HOME/build-tools/37.0.0/aidl --lang=ndk --structured --stability=vintf --min_sdk_version=31 --version=3 --hash=bfeb16c941085370dc6b68bcd7aec2dbb9435f49 -I effect3 -I types4 -I fmq1 -I common2 -I acommon4 -I eraser1 -o gen/src -h gen/include effect3/android/hardware/audio/effect/State.aidl
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
namespace hardware {
namespace audio {
namespace effect {
enum class State : int8_t {
  INIT = 0,
  IDLE = 1,
  PROCESSING = 2,
  DRAINING = 3,
};

}  // namespace effect
}  // namespace audio
}  // namespace hardware
}  // namespace android
}  // namespace aidl
namespace aidl {
namespace android {
namespace hardware {
namespace audio {
namespace effect {
[[nodiscard]] static inline std::string toString(State val) {
  switch(val) {
  case State::INIT:
    return "INIT";
  case State::IDLE:
    return "IDLE";
  case State::PROCESSING:
    return "PROCESSING";
  case State::DRAINING:
    return "DRAINING";
  default:
    return std::to_string(static_cast<int8_t>(val));
  }
}
}  // namespace effect
}  // namespace audio
}  // namespace hardware
}  // namespace android
}  // namespace aidl
namespace ndk {
namespace internal {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++17-extensions"
template <>
constexpr inline std::array<aidl::android::hardware::audio::effect::State, 4> enum_values<aidl::android::hardware::audio::effect::State> = {
  aidl::android::hardware::audio::effect::State::INIT,
  aidl::android::hardware::audio::effect::State::IDLE,
  aidl::android::hardware::audio::effect::State::PROCESSING,
  aidl::android::hardware::audio::effect::State::DRAINING,
};
#pragma clang diagnostic pop
}  // namespace internal
}  // namespace ndk
