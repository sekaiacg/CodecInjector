#include <jni.h>

#include "port_api.h"
#include "ssc_codec_state.h"
#include "ssc_interface.h"

extern "C" __attribute__((visibility("default"), used)) JNIEXPORT void JNICALL
Java_io_github_wxxsfxyzm_galaxybudsport_ssc_SscNative_nativeSetUhq(JNIEnv*, jclass,
                                                                    jboolean preferred) {
    sscport::set_uhq_preferred(preferred);
    port::info("uhq_preferred", preferred, 0);
}

extern "C" __attribute__((visibility("default"), used)) JNIEXPORT void JNICALL
Java_io_github_wxxsfxyzm_galaxybudsport_ssc_SscNative_nativeSetQualityMode(JNIEnv*, jclass,
                                                                           jint mode) {
    sscport::set_quality_mode(static_cast<uint32_t>(mode));
    port::info("quality_mode", mode, 0);
}
