/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: $ANDROID_HOME/build-tools/37.0.0/aidl --lang=ndk --structured --stability=vintf --min_sdk_version=31 --version=3 --hash=bfeb16c941085370dc6b68bcd7aec2dbb9435f49 -I effect3 -I types4 -I fmq1 -I common2 -I acommon4 -I eraser1 -o gen/src -h gen/include effect3/android/hardware/audio/effect/IFactory.aidl
 *
 * DO NOT CHECK THIS FILE INTO A CODE TREE (e.g. git, etc..).
 * ALWAYS GENERATE THIS FILE FROM UPDATED AIDL COMPILER
 * AS A BUILD INTERMEDIATE ONLY. THIS IS NOT SOURCE CODE.
 */
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <android/binder_interface_utils.h>
#include <aidl/android/hardware/audio/effect/Descriptor.h>
#include <aidl/android/hardware/audio/effect/IEffect.h>
#include <aidl/android/hardware/audio/effect/Processing.h>
#include <aidl/android/media/audio/common/AudioUuid.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl::android::hardware::audio::effect {
class Descriptor;
class IEffect;
class Processing;
}  // namespace aidl::android::hardware::audio::effect
namespace aidl::android::media::audio::common {
class AudioUuid;
}  // namespace aidl::android::media::audio::common
namespace aidl {
namespace android {
namespace hardware {
namespace audio {
namespace effect {
class IFactoryDelegator;

class IFactory : public ::ndk::ICInterface {
public:
  typedef IFactoryDelegator DefaultDelegator;
  static const char* descriptor;
  IFactory();
  virtual ~IFactory();

  static inline const int32_t version = 3;
  static inline const std::string hash = "bfeb16c941085370dc6b68bcd7aec2dbb9435f49";
  static constexpr uint32_t TRANSACTION_queryEffects = FIRST_CALL_TRANSACTION + 0;
  static constexpr uint32_t TRANSACTION_queryProcessing = FIRST_CALL_TRANSACTION + 1;
  static constexpr uint32_t TRANSACTION_createEffect = FIRST_CALL_TRANSACTION + 2;
  static constexpr uint32_t TRANSACTION_destroyEffect = FIRST_CALL_TRANSACTION + 3;

  static std::shared_ptr<IFactory> fromBinder(const ::ndk::SpAIBinder& binder);
  static binder_status_t writeToParcel(AParcel* parcel, const std::shared_ptr<IFactory>& instance);
  static binder_status_t readFromParcel(const AParcel* parcel, std::shared_ptr<IFactory>* instance);
  virtual ::ndk::ScopedAStatus queryEffects(const std::optional<::aidl::android::media::audio::common::AudioUuid>& in_type, const std::optional<::aidl::android::media::audio::common::AudioUuid>& in_implementation, const std::optional<::aidl::android::media::audio::common::AudioUuid>& in_proxy, std::vector<::aidl::android::hardware::audio::effect::Descriptor>* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus queryProcessing(const std::optional<::aidl::android::hardware::audio::effect::Processing::Type>& in_type, std::vector<::aidl::android::hardware::audio::effect::Processing>* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus createEffect(const ::aidl::android::media::audio::common::AudioUuid& in_implUuid, std::shared_ptr<::aidl::android::hardware::audio::effect::IEffect>* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus destroyEffect(const std::shared_ptr<::aidl::android::hardware::audio::effect::IEffect>& in_handle) = 0;
  virtual ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) = 0;
};
class IFactoryDefault : public IFactory {
public:
  ::ndk::ScopedAStatus queryEffects(const std::optional<::aidl::android::media::audio::common::AudioUuid>& in_type, const std::optional<::aidl::android::media::audio::common::AudioUuid>& in_implementation, const std::optional<::aidl::android::media::audio::common::AudioUuid>& in_proxy, std::vector<::aidl::android::hardware::audio::effect::Descriptor>* _aidl_return) override;
  ::ndk::ScopedAStatus queryProcessing(const std::optional<::aidl::android::hardware::audio::effect::Processing::Type>& in_type, std::vector<::aidl::android::hardware::audio::effect::Processing>* _aidl_return) override;
  ::ndk::ScopedAStatus createEffect(const ::aidl::android::media::audio::common::AudioUuid& in_implUuid, std::shared_ptr<::aidl::android::hardware::audio::effect::IEffect>* _aidl_return) override;
  ::ndk::ScopedAStatus destroyEffect(const std::shared_ptr<::aidl::android::hardware::audio::effect::IEffect>& in_handle) override;
  ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) override;
  ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) override;
  ::ndk::SpAIBinder asBinder() override;
  bool isRemote() override;
};
}  // namespace effect
}  // namespace audio
}  // namespace hardware
}  // namespace android
}  // namespace aidl
