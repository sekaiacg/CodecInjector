#include <jni.h>

#include "port_api.h"

extern "C" __attribute__((visibility("default"), used)) JNIEXPORT void JNICALL
Java_io_github_wxxsfxyzm_galaxybudsport_core_NativeBridge_nativeSetSpatial(JNIEnv*, jclass,
                                                                           jboolean on) {
    port::spatial().set_enabled(on);
    port::info("spatial_enabled", on, 0);
}

extern "C" __attribute__((visibility("default"), used)) JNIEXPORT void JNICALL
Java_io_github_wxxsfxyzm_galaxybudsport_core_NativeBridge_nativeSetYaw(JNIEnv*, jclass,
                                                                       jfloat radians) {
    port::spatial().set_yaw(radians);
}

extern "C" __attribute__((visibility("default"), used)) JNIEXPORT void JNICALL
Java_io_github_wxxsfxyzm_galaxybudsport_core_NativeBridge_nativeSetPitch(JNIEnv*, jclass,
                                                                         jfloat radians) {
    port::spatial().set_pitch(radians);
}
