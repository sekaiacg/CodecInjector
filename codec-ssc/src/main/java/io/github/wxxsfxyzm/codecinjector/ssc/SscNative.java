package io.github.wxxsfxyzm.codecinjector.ssc;

/** The SSC codec plugin's native controls (in the module's native library). */
final class SscNative {
    private SscNative() {
    }

    /** Whether SSC UHQ (96 kHz) is selected when the framework requests no sample rate. */
    static native void nativeSetUhq(boolean preferred);

    /** Bitrate mode: 0 adaptive, 1/2/3 fixed high/medium/low. */
    static native void nativeSetQualityMode(int mode);
}
