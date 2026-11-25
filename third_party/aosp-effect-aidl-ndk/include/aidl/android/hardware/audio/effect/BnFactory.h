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
#include <cassert>

#ifndef __BIONIC__
#ifndef __assert2
#define __assert2(a,b,c,d) ((void)0)
#endif
#endif

namespace aidl {
namespace android {
namespace hardware {
namespace audio {
namespace effect {
class BnFactory : public ::ndk::BnCInterface<IFactory> {
public:
  BnFactory();
  virtual ~BnFactory();
  ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) final;
  ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) final;
protected:
  ::ndk::SpAIBinder createBinder() override;
private:
};
class IFactoryDelegator : public BnFactory {
public:
  explicit IFactoryDelegator(const std::shared_ptr<IFactory> &impl) : _impl(impl) {
     int32_t _impl_ver = 0;
     if (!impl->getInterfaceVersion(&_impl_ver).isOk()) {;
        __assert2(__FILE__, __LINE__, __PRETTY_FUNCTION__, "Delegator failed to get version of the implementation.");
     }
     if (_impl_ver != IFactory::version) {
        __assert2(__FILE__, __LINE__, __PRETTY_FUNCTION__, "Mismatched versions of delegator and implementation is not allowed.");
     }
  }

  ::ndk::ScopedAStatus queryEffects(const std::optional<::aidl::android::media::audio::common::AudioUuid>& in_type, const std::optional<::aidl::android::media::audio::common::AudioUuid>& in_implementation, const std::optional<::aidl::android::media::audio::common::AudioUuid>& in_proxy, std::vector<::aidl::android::hardware::audio::effect::Descriptor>* _aidl_return) override {
    return _impl->queryEffects(in_type, in_implementation, in_proxy, _aidl_return);
  }
  ::ndk::ScopedAStatus queryProcessing(const std::optional<::aidl::android::hardware::audio::effect::Processing::Type>& in_type, std::vector<::aidl::android::hardware::audio::effect::Processing>* _aidl_return) override {
    return _impl->queryProcessing(in_type, _aidl_return);
  }
  ::ndk::ScopedAStatus createEffect(const ::aidl::android::media::audio::common::AudioUuid& in_implUuid, std::shared_ptr<::aidl::android::hardware::audio::effect::IEffect>* _aidl_return) override {
    return _impl->createEffect(in_implUuid, _aidl_return);
  }
  ::ndk::ScopedAStatus destroyEffect(const std::shared_ptr<::aidl::android::hardware::audio::effect::IEffect>& in_handle) override {
    return _impl->destroyEffect(in_handle);
  }
protected:
private:
  std::shared_ptr<IFactory> _impl;
};

}  // namespace effect
}  // namespace audio
}  // namespace hardware
}  // namespace android
}  // namespace aidl
