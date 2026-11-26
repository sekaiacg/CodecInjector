package io.github.wxxsfxyzm.codecinjector.ssc;

import android.util.Log;
import io.github.libxposed.api.XposedInterface;

/**
 * Logs of the Buds manager side to logcat and, for the key events, to the LSPosed module log as
 * well, so that a test report from a phone without adb only needs the LSPosed log export.
 */
final class BudsLog {
    static final String TAG = "CodecInjectorBuds";
    private static volatile XposedInterface xposed;

    private BudsLog() {
    }

    static void attach(XposedInterface module) {
        xposed = module;
    }

    /** Key event: logcat and the LSPosed log. */
    static void w(String message) {
        Log.w(TAG, message);
        XposedInterface module = xposed;
        if (module != null) module.log(Log.WARN, TAG, message);
    }

    static void e(String message) {
        Log.e(TAG, message);
        XposedInterface module = xposed;
        if (module != null) module.log(Log.ERROR, TAG, message);
    }

    /** High-rate diagnostics: logcat only. */
    static void verbose(String message) {
        Log.w(TAG, message);
    }
}
