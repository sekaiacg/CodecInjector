/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: $ANDROID_HOME/build-tools/37.0.0/aidl --lang=ndk --structured --stability=vintf --min_sdk_version=31 --version=1 --hash=d0f24b98624bc07be92c5dc38302f967c522ff14 -I effect3 -I types4 -I fmq1 -I common2 -I acommon4 -I eraser1 -o gen/src -h gen/include eraser1/android/media/audio/eraser/IEraserCallback.aidl
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
#include <aidl/android/media/audio/eraser/ClassificationMetadataList.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl::android::media::audio::eraser {
class ClassificationMetadataList;
}  // namespace aidl::android::media::audio::eraser
namespace aidl {
namespace android {
namespace media {
namespace audio {
namespace eraser {
class IEraserCallbackDelegator;

class IEraserCallback : public ::ndk::ICInterface {
public:
  typedef IEraserCallbackDelegator DefaultDelegator;
  static const char* descriptor;
  IEraserCallback();
  virtual ~IEraserCallback();

  static inline const int32_t version = 1;
  static inline const std::string hash = "d0f24b98624bc07be92c5dc38302f967c522ff14";
  static constexpr uint32_t TRANSACTION_onClassifierUpdate = FIRST_CALL_TRANSACTION + 0;

  static std::shared_ptr<IEraserCallback> fromBinder(const ::ndk::SpAIBinder& binder);
  static binder_status_t writeToParcel(AParcel* parcel, const std::shared_ptr<IEraserCallback>& instance);
  static binder_status_t readFromParcel(const AParcel* parcel, std::shared_ptr<IEraserCallback>* instance);
  virtual ::ndk::ScopedAStatus onClassifierUpdate(int32_t in_soundSourceId, const ::aidl::android::media::audio::eraser::ClassificationMetadataList& in_metadata) = 0;
  virtual ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) = 0;
};
class IEraserCallbackDefault : public IEraserCallback {
public:
  ::ndk::ScopedAStatus onClassifierUpdate(int32_t in_soundSourceId, const ::aidl::android::media::audio::eraser::ClassificationMetadataList& in_metadata) override;
  ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) override;
  ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) override;
  ::ndk::SpAIBinder asBinder() override;
  bool isRemote() override;
};
}  // namespace eraser
}  // namespace audio
}  // namespace media
}  // namespace android
}  // namespace aidl
