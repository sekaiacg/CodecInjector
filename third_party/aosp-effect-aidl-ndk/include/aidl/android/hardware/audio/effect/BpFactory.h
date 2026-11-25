/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: $ANDROID_HOME/build-tools/37.0.0/aidl --lang=ndk --structured --stability=vintf --min_sdk_version=31 --version=3 --hash=bfeb16c941085370dc6b68bcd7aec2dbb9435f49 -I effect3 -I types4 -I fmq1 -I common2 -I acommon4 -I eraser1 -o gen/src -h gen/include effect3/android/hardware/audio/effect/IFactory.aidl
 *
 * DO NOT CHECK THIS FILE INTO A CODE TREE (e.g. git, etc..).
 * ALWAYS GENERATE THIS FILE FROM UPDATED AIDL COMPILER
 * AS A BUILD INTERMEDIATE ONLY. THIS IS NOT SOURCE CODE.
 */
#pragma once

#include "aidl/android/hardware/audio/effect/IFactory.h"

#include <android/binder_ibinder.h>

namespace aidl {
namespace android {
namespace hardware {
namespace audio {
namespace effect {
class BpFactory : public ::ndk::BpCInterface<IFactory> {
public:
  explicit BpFactory(const ::ndk::SpAIBinder& binder);
  virtual ~BpFactory();

  ::ndk::ScopedAStatus queryEffects(const std::optional<::aidl::android::media::audio::common::AudioUuid>& in_type, const std::optional<::aidl::android::media::audio::common::AudioUuid>& in_implementation, const std::optional<::aidl::android::media::audio::common::AudioUuid>& in_proxy, std::vector<::aidl::android::hardware::audio::effect::Descriptor>* _aidl_return) override;
  ::ndk::ScopedAStatus queryProcessing(const std::optional<::aidl::android::hardware::audio::effect::Processing::Type>& in_type, std::vector<::aidl::android::hardware::audio::effect::Processing>* _aidl_return) override;
  ::ndk::ScopedAStatus createEffect(const ::aidl::android::media::audio::common::AudioUuid& in_implUuid, std::shared_ptr<::aidl::android::hardware::audio::effect::IEffect>* _aidl_return) override;
  ::ndk::ScopedAStatus destroyEffect(const std::shared_ptr<::aidl::android::hardware::audio::effect::IEffect>& in_handle) override;
  ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) override;
  ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) override;
  int32_t _aidl_cached_version = -1;
  std::string _aidl_cached_hash = "-1";
  std::mutex _aidl_cached_hash_mutex;
};
}  // namespace effect
}  // namespace audio
}  // namespace hardware
}  // namespace android
}  // namespace aidl
