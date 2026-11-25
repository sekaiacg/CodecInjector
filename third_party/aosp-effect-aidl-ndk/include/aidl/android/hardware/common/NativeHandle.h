/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: $ANDROID_HOME/build-tools/37.0.0/aidl --lang=ndk --structured --stability=vintf --min_sdk_version=31 --version=2 --hash=c32ddfdeb69c6e4a8a45519e6f9a39c4b66fd99f -I effect3 -I types4 -I fmq1 -I common2 -I acommon4 -I eraser1 -o gen/src -h gen/include common2/android/hardware/common/NativeHandle.aidl
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
#include <android/binder_parcelable_utils.h>
#include <android/binder_to_string.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl {
namespace android {
namespace hardware {
namespace common {
class NativeHandle {
public:
  typedef std::false_type fixed_size;
  static const char* descriptor;

  std::vector<::ndk::ScopedFileDescriptor> fds;
  std::vector<int32_t> ints;

  binder_status_t readFromParcel(const AParcel* parcel);
  binder_status_t writeToParcel(AParcel* parcel) const;

  inline bool operator==(const NativeHandle& _rhs) const {
    return std::tie(fds, ints) == std::tie(_rhs.fds, _rhs.ints);
  }
  inline bool operator<(const NativeHandle& _rhs) const {
    return std::tie(fds, ints) < std::tie(_rhs.fds, _rhs.ints);
  }
  inline bool operator!=(const NativeHandle& _rhs) const {
    return !(*this == _rhs);
  }
  inline bool operator>(const NativeHandle& _rhs) const {
    return _rhs < *this;
  }
  inline bool operator>=(const NativeHandle& _rhs) const {
    return !(*this < _rhs);
  }
  inline bool operator<=(const NativeHandle& _rhs) const {
    return !(_rhs < *this);
  }

  static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
  inline std::string toString() const {
    std::ostringstream _aidl_os;
    _aidl_os << "NativeHandle{";
    _aidl_os << "fds: " << ::android::internal::ToString(fds);
    _aidl_os << ", ints: " << ::android::internal::ToString(ints);
    _aidl_os << "}";
    return _aidl_os.str();
  }
};
}  // namespace common
}  // namespace hardware
}  // namespace android
}  // namespace aidl
