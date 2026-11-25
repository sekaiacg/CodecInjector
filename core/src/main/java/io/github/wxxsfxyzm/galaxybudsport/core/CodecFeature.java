package io.github.wxxsfxyzm.galaxybudsport.core;

import io.github.libxposed.api.XposedInterface;
import io.github.libxposed.api.XposedModuleInterface;

/**
 * The Java side of one codec module. The core installs what every codec needs (codec types in the
 * framework, Settings' codec list and label) and calls these at the matching points.
 */
public interface CodecFeature {
    CodecInfo codec();

    /** Every process in scope, as the module is loaded into it. */
    default void onProcess(XposedInterface xposed, String processName) {
    }

    /** Packages other than Bluetooth and Settings, for example a companion app. */
    default void onPackage(XposedInterface xposed, XposedModuleInterface.PackageReadyParam param) {
    }

    /** The Settings package. */
    default void onSettings(XposedInterface xposed, ClassLoader loader) {
    }

    /** The Bluetooth process, once its application context exists and the native module is loaded. */
    default void onBluetooth(BluetoothHost host) {
    }
}
