/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: $ANDROID_HOME/build-tools/37.0.0/aidl --lang=ndk --structured --stability=vintf --min_sdk_version=31 --version=4 --hash=af71e6ae2c6861fc2b09bb477e7285e6777cd41c -I effect3 -I types4 -I fmq1 -I common2 -I acommon4 -I eraser1 -o gen/src -h gen/include types4/android/media/audio/common/AudioDeviceAddress.aidl
 *
 * DO NOT CHECK THIS FILE INTO A CODE TREE (e.g. git, etc..).
 * ALWAYS GENERATE THIS FILE FROM UPDATED AIDL COMPILER
 * AS A BUILD INTERMEDIATE ONLY. THIS IS NOT SOURCE CODE.
 */
#include "aidl/android/media/audio/common/AudioDeviceAddress.h"

#include <cstdint>
#include <android/binder_parcel.h>
#include <android/binder_parcel_utils.h>
#include <android/binder_status.h>

namespace aidl {
namespace android {
namespace media {
namespace audio {
namespace common {
const char* AudioDeviceAddress::descriptor = "android.media.audio.common.AudioDeviceAddress";

binder_status_t AudioDeviceAddress::readFromParcel(const AParcel* _parcel) {
  binder_status_t _aidl_ret_status;
  int32_t _aidl_tag;
  if ((_aidl_ret_status = ::ndk::AParcel_readData(_parcel, &_aidl_tag)) != STATUS_OK) return _aidl_ret_status;
  switch (static_cast<Tag>(_aidl_tag)) {
  case id: {
    std::string _aidl_value;
    if ((_aidl_ret_status = ::ndk::AParcel_readData(_parcel, &_aidl_value)) != STATUS_OK) return _aidl_ret_status;
    if constexpr (std::is_trivially_copyable_v<std::string>) {
      set<id>(_aidl_value);
    } else {
      // NOLINTNEXTLINE(performance-move-const-arg)
      set<id>(std::move(_aidl_value));
    }
    return STATUS_OK; }
  case mac: {
    std::vector<uint8_t> _aidl_value;
    if ((_aidl_ret_status = ::ndk::AParcel_readData(_parcel, &_aidl_value)) != STATUS_OK) return _aidl_ret_status;
    if constexpr (std::is_trivially_copyable_v<std::vector<uint8_t>>) {
      set<mac>(_aidl_value);
    } else {
      // NOLINTNEXTLINE(performance-move-const-arg)
      set<mac>(std::move(_aidl_value));
    }
    return STATUS_OK; }
  case ipv4: {
    std::vector<uint8_t> _aidl_value;
    if ((_aidl_ret_status = ::ndk::AParcel_readData(_parcel, &_aidl_value)) != STATUS_OK) return _aidl_ret_status;
    if constexpr (std::is_trivially_copyable_v<std::vector<uint8_t>>) {
      set<ipv4>(_aidl_value);
    } else {
      // NOLINTNEXTLINE(performance-move-const-arg)
      set<ipv4>(std::move(_aidl_value));
    }
    return STATUS_OK; }
  case ipv6: {
    std::vector<int32_t> _aidl_value;
    if ((_aidl_ret_status = ::ndk::AParcel_readData(_parcel, &_aidl_value)) != STATUS_OK) return _aidl_ret_status;
    if constexpr (std::is_trivially_copyable_v<std::vector<int32_t>>) {
      set<ipv6>(_aidl_value);
    } else {
      // NOLINTNEXTLINE(performance-move-const-arg)
      set<ipv6>(std::move(_aidl_value));
    }
    return STATUS_OK; }
  case alsa: {
    std::vector<int32_t> _aidl_value;
    if ((_aidl_ret_status = ::ndk::AParcel_readData(_parcel, &_aidl_value)) != STATUS_OK) return _aidl_ret_status;
    if constexpr (std::is_trivially_copyable_v<std::vector<int32_t>>) {
      set<alsa>(_aidl_value);
    } else {
      // NOLINTNEXTLINE(performance-move-const-arg)
      set<alsa>(std::move(_aidl_value));
    }
    return STATUS_OK; }
  }
  return STATUS_BAD_VALUE;
}
binder_status_t AudioDeviceAddress::writeToParcel(AParcel* _parcel) const {
  binder_status_t _aidl_ret_status = ::ndk::AParcel_writeData(_parcel, static_cast<int32_t>(getTag()));
  if (_aidl_ret_status != STATUS_OK) return _aidl_ret_status;
  switch (getTag()) {
  case id: return ::ndk::AParcel_writeData(_parcel, get<id>());
  case mac: return ::ndk::AParcel_writeData(_parcel, get<mac>());
  case ipv4: return ::ndk::AParcel_writeData(_parcel, get<ipv4>());
  case ipv6: return ::ndk::AParcel_writeData(_parcel, get<ipv6>());
  case alsa: return ::ndk::AParcel_writeData(_parcel, get<alsa>());
  }
  __assert2(__FILE__, __LINE__, __PRETTY_FUNCTION__, "can't reach here");
}

}  // namespace common
}  // namespace audio
}  // namespace media
}  // namespace android
}  // namespace aidl
