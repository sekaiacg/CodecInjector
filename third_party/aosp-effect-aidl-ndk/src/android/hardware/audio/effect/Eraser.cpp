/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: $ANDROID_HOME/build-tools/37.0.0/aidl --lang=ndk --structured --stability=vintf --min_sdk_version=31 --version=3 --hash=bfeb16c941085370dc6b68bcd7aec2dbb9435f49 -I effect3 -I types4 -I fmq1 -I common2 -I acommon4 -I eraser1 -o gen/src -h gen/include effect3/android/hardware/audio/effect/Eraser.aidl
 *
 * DO NOT CHECK THIS FILE INTO A CODE TREE (e.g. git, etc..).
 * ALWAYS GENERATE THIS FILE FROM UPDATED AIDL COMPILER
 * AS A BUILD INTERMEDIATE ONLY. THIS IS NOT SOURCE CODE.
 */
#include "aidl/android/hardware/audio/effect/Eraser.h"

#include <cstdint>
#include <android/binder_parcel.h>
#include <android/binder_parcel_utils.h>
#include <android/binder_status.h>
#include <aidl/android/media/audio/eraser/BnEraserCallback.h>
#include <aidl/android/media/audio/eraser/BpEraserCallback.h>
#include <aidl/android/media/audio/eraser/IEraserCallback.h>

namespace aidl {
namespace android {
namespace hardware {
namespace audio {
namespace effect {
const char* Eraser::descriptor = "android.hardware.audio.effect.Eraser";

binder_status_t Eraser::readFromParcel(const AParcel* _parcel) {
  binder_status_t _aidl_ret_status;
  int32_t _aidl_tag;
  if ((_aidl_ret_status = ::ndk::AParcel_readData(_parcel, &_aidl_tag)) != STATUS_OK) return _aidl_ret_status;
  switch (static_cast<Tag>(_aidl_tag)) {
  case vendor: {
    ::aidl::android::hardware::audio::effect::VendorExtension _aidl_value;
    if ((_aidl_ret_status = ::ndk::AParcel_readData(_parcel, &_aidl_value)) != STATUS_OK) return _aidl_ret_status;
    if constexpr (std::is_trivially_copyable_v<::aidl::android::hardware::audio::effect::VendorExtension>) {
      set<vendor>(_aidl_value);
    } else {
      // NOLINTNEXTLINE(performance-move-const-arg)
      set<vendor>(std::move(_aidl_value));
    }
    return STATUS_OK; }
  case capability: {
    ::aidl::android::media::audio::eraser::Capability _aidl_value;
    if ((_aidl_ret_status = ::ndk::AParcel_readData(_parcel, &_aidl_value)) != STATUS_OK) return _aidl_ret_status;
    if constexpr (std::is_trivially_copyable_v<::aidl::android::media::audio::eraser::Capability>) {
      set<capability>(_aidl_value);
    } else {
      // NOLINTNEXTLINE(performance-move-const-arg)
      set<capability>(std::move(_aidl_value));
    }
    return STATUS_OK; }
  case configuration: {
    ::aidl::android::media::audio::eraser::Configuration _aidl_value;
    if ((_aidl_ret_status = ::ndk::AParcel_readData(_parcel, &_aidl_value)) != STATUS_OK) return _aidl_ret_status;
    if constexpr (std::is_trivially_copyable_v<::aidl::android::media::audio::eraser::Configuration>) {
      set<configuration>(_aidl_value);
    } else {
      // NOLINTNEXTLINE(performance-move-const-arg)
      set<configuration>(std::move(_aidl_value));
    }
    return STATUS_OK; }
  }
  return STATUS_BAD_VALUE;
}
binder_status_t Eraser::writeToParcel(AParcel* _parcel) const {
  binder_status_t _aidl_ret_status = ::ndk::AParcel_writeData(_parcel, static_cast<int32_t>(getTag()));
  if (_aidl_ret_status != STATUS_OK) return _aidl_ret_status;
  switch (getTag()) {
  case vendor: return ::ndk::AParcel_writeData(_parcel, get<vendor>());
  case capability: return ::ndk::AParcel_writeData(_parcel, get<capability>());
  case configuration: return ::ndk::AParcel_writeData(_parcel, get<configuration>());
  }
  __assert2(__FILE__, __LINE__, __PRETTY_FUNCTION__, "can't reach here");
}

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
const char* Eraser::Id::descriptor = "android.hardware.audio.effect.Eraser.Id";

binder_status_t Eraser::Id::readFromParcel(const AParcel* _parcel) {
  binder_status_t _aidl_ret_status;
  int32_t _aidl_tag;
  if ((_aidl_ret_status = ::ndk::AParcel_readData(_parcel, &_aidl_tag)) != STATUS_OK) return _aidl_ret_status;
  switch (static_cast<Tag>(_aidl_tag)) {
  case vendorExtensionTag: {
    ::aidl::android::hardware::audio::effect::VendorExtension _aidl_value;
    if ((_aidl_ret_status = ::ndk::AParcel_readData(_parcel, &_aidl_value)) != STATUS_OK) return _aidl_ret_status;
    if constexpr (std::is_trivially_copyable_v<::aidl::android::hardware::audio::effect::VendorExtension>) {
      set<vendorExtensionTag>(_aidl_value);
    } else {
      // NOLINTNEXTLINE(performance-move-const-arg)
      set<vendorExtensionTag>(std::move(_aidl_value));
    }
    return STATUS_OK; }
  case commonTag: {
    ::aidl::android::hardware::audio::effect::Eraser::Tag _aidl_value;
    if ((_aidl_ret_status = ::ndk::AParcel_readData(_parcel, &_aidl_value)) != STATUS_OK) return _aidl_ret_status;
    if constexpr (std::is_trivially_copyable_v<::aidl::android::hardware::audio::effect::Eraser::Tag>) {
      set<commonTag>(_aidl_value);
    } else {
      // NOLINTNEXTLINE(performance-move-const-arg)
      set<commonTag>(std::move(_aidl_value));
    }
    return STATUS_OK; }
  }
  return STATUS_BAD_VALUE;
}
binder_status_t Eraser::Id::writeToParcel(AParcel* _parcel) const {
  binder_status_t _aidl_ret_status = ::ndk::AParcel_writeData(_parcel, static_cast<int32_t>(getTag()));
  if (_aidl_ret_status != STATUS_OK) return _aidl_ret_status;
  switch (getTag()) {
  case vendorExtensionTag: return ::ndk::AParcel_writeData(_parcel, get<vendorExtensionTag>());
  case commonTag: return ::ndk::AParcel_writeData(_parcel, get<commonTag>());
  }
  __assert2(__FILE__, __LINE__, __PRETTY_FUNCTION__, "can't reach here");
}

}  // namespace effect
}  // namespace audio
}  // namespace hardware
}  // namespace android
}  // namespace aidl
