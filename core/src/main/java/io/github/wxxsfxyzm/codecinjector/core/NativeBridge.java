package io.github.wxxsfxyzm.codecinjector.core;

/** The native core in the Bluetooth process (loaded by PortRuntime). */
public final class NativeBridge {
    private NativeBridge() {
    }

    private static native void nativeSetSpatial(boolean on);

    private static native void nativeSetYaw(float radians);

    private static native void nativeSetPitch(float radians);

    /** Turns Dolby spatial rendering on or off. */
    public static void setSpatial(boolean on) {
        nativeSetSpatial(on);
    }

    /** Head yaw in radians, AOSP convention (positive = head turned left). */
    public static void setYaw(float radians) {
        nativeSetYaw(radians);
    }

    /** Head pitch in radians (positive = looking up). */
    public static void setPitch(float radians) {
        nativeSetPitch(radians);
    }
}
