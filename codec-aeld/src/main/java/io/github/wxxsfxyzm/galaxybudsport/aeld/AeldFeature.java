package io.github.wxxsfxyzm.galaxybudsport.aeld;

import io.github.wxxsfxyzm.galaxybudsport.core.CodecFeature;
import io.github.wxxsfxyzm.galaxybudsport.core.CodecInfo;

/**
 * Apple AAC-ELD for AirPods. The codec itself is native (codec type, Settings codec list and label
 * come from the core); Dolby spatial rendering is on, facing forward, until AirPods head tracking.
 */
public final class AeldFeature implements CodecFeature {
    static final CodecInfo CODEC = new CodecInfo(17, 0x0000008001004cffL, "AAC-ELD");

    @Override public CodecInfo codec() {
        return CODEC;
    }
}
