/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: $ANDROID_HOME/build-tools/37.0.0/aidl --lang=ndk --structured --stability=vintf --min_sdk_version=31 --version=3 --hash=bfeb16c941085370dc6b68bcd7aec2dbb9435f49 -I effect3 -I types4 -I fmq1 -I common2 -I acommon4 -I eraser1 -o gen/src -h gen/include effect3/android/hardware/audio/effect/IEffect.aidl
 *
 * DO NOT CHECK THIS FILE INTO A CODE TREE (e.g. git, etc..).
 * ALWAYS GENERATE THIS FILE FROM UPDATED AIDL COMPILER
 * AS A BUILD INTERMEDIATE ONLY. THIS IS NOT SOURCE CODE.
 */
#pragma once

#include "aidl/android/hardware/audio/effect/IEffect.h"

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
class BnEffect : public ::ndk::BnCInterface<IEffect> {
public:
  BnEffect();
  virtual ~BnEffect();
  ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) final;
  ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) final;
protected:
  ::ndk::SpAIBinder createBinder() override;
private:
};
class IEffectDelegator : public BnEffect {
public:
  explicit IEffectDelegator(const std::shared_ptr<IEffect> &impl) : _impl(impl) {
     int32_t _impl_ver = 0;
     if (!impl->getInterfaceVersion(&_impl_ver).isOk()) {;
        __assert2(__FILE__, __LINE__, __PRETTY_FUNCTION__, "Delegator failed to get version of the implementation.");
     }
     if (_impl_ver != IEffect::version) {
        __assert2(__FILE__, __LINE__, __PRETTY_FUNCTION__, "Mismatched versions of delegator and implementation is not allowed.");
     }
  }

  ::ndk::ScopedAStatus open(const ::aidl::android::hardware::audio::effect::Parameter::Common& in_common, const std::optional<::aidl::android::hardware::audio::effect::Parameter::Specific>& in_specific, ::aidl::android::hardware::audio::effect::IEffect::OpenEffectReturn* _aidl_return) override {
    return _impl->open(in_common, in_specific, _aidl_return);
  }
  ::ndk::ScopedAStatus close() override {
    return _impl->close();
  }
  ::ndk::ScopedAStatus getDescriptor(::aidl::android::hardware::audio::effect::Descriptor* _aidl_return) override {
    return _impl->getDescriptor(_aidl_return);
  }
  ::ndk::ScopedAStatus command(::aidl::android::hardware::audio::effect::CommandId in_commandId) override {
    return _impl->command(in_commandId);
  }
  ::ndk::ScopedAStatus getState(::aidl::android::hardware::audio::effect::State* _aidl_return) override {
    return _impl->getState(_aidl_return);
  }
  ::ndk::ScopedAStatus setParameter(const ::aidl::android::hardware::audio::effect::Parameter& in_param) override {
    return _impl->setParameter(in_param);
  }
  ::ndk::ScopedAStatus getParameter(const ::aidl::android::hardware::audio::effect::Parameter::Id& in_paramId, ::aidl::android::hardware::audio::effect::Parameter* _aidl_return) override {
    return _impl->getParameter(in_paramId, _aidl_return);
  }
  ::ndk::ScopedAStatus reopen(::aidl::android::hardware::audio::effect::IEffect::OpenEffectReturn* _aidl_return) override {
    return _impl->reopen(_aidl_return);
  }
protected:
private:
  std::shared_ptr<IEffect> _impl;
};

}  // namespace effect
}  // namespace audio
}  // namespace hardware
}  // namespace android
}  // namespace aidl
