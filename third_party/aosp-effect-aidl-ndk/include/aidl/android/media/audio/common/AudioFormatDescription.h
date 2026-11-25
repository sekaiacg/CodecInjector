/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: $ANDROID_HOME/build-tools/37.0.0/aidl --lang=ndk --structured --stability=vintf --min_sdk_version=31 --version=4 --hash=af71e6ae2c6861fc2b09bb477e7285e6777cd41c -I effect3 -I types4 -I fmq1 -I common2 -I acommon4 -I eraser1 -o gen/src -h gen/include types4/android/media/audio/common/AudioFormatDescription.aidl
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
#include <aidl/android/media/audio/common/AudioFormatType.h>
#include <aidl/android/media/audio/common/PcmType.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl {
namespace android {
namespace media {
namespace audio {
namespace common {
class AudioFormatDescription {
public:
  typedef std::false_type fixed_size;
  static const char* descriptor;

  ::aidl::android::media::audio::common::AudioFormatType type = ::aidl::android::media::audio::common::AudioFormatType::DEFAULT;
  ::aidl::android::media::audio::common::PcmType pcm = ::aidl::android::media::audio::common::PcmType::DEFAULT;
  std::string encoding;

  binder_status_t readFromParcel(const AParcel* parcel);
  binder_status_t writeToParcel(AParcel* parcel) const;

  inline bool operator==(const AudioFormatDescription& _rhs) const {
    return std::tie(type, pcm, encoding) == std::tie(_rhs.type, _rhs.pcm, _rhs.encoding);
  }
  inline bool operator<(const AudioFormatDescription& _rhs) const {
    return std::tie(type, pcm, encoding) < std::tie(_rhs.type, _rhs.pcm, _rhs.encoding);
  }
  inline bool operator!=(const AudioFormatDescription& _rhs) const {
    return !(*this == _rhs);
  }
  inline bool operator>(const AudioFormatDescription& _rhs) const {
    return _rhs < *this;
  }
  inline bool operator>=(const AudioFormatDescription& _rhs) const {
    return !(*this < _rhs);
  }
  inline bool operator<=(const AudioFormatDescription& _rhs) const {
    return !(_rhs < *this);
  }

  static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
  inline std::string toString() const {
    std::ostringstream _aidl_os;
    _aidl_os << "AudioFormatDescription{";
    _aidl_os << "type: " << ::android::internal::ToString(type);
    _aidl_os << ", pcm: " << ::android::internal::ToString(pcm);
    _aidl_os << ", encoding: " << ::android::internal::ToString(encoding);
    _aidl_os << "}";
    return _aidl_os.str();
  }
};
}  // namespace common
}  // namespace audio
}  // namespace media
}  // namespace android
}  // namespace aidl
