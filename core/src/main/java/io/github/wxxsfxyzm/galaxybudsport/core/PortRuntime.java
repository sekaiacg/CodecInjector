package io.github.wxxsfxyzm.galaxybudsport.core;

import android.Manifest;
import android.app.ActivityThread;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import io.github.libxposed.api.XposedModule;
import io.github.libxposed.api.XposedModuleInterface;
import java.util.ArrayList;
import java.util.List;

/**
 * Runs the module in each process: installs what every codec needs and hands each codec feature
 * its processes and packages.
 */
public final class PortRuntime {
    public static final String BLUETOOTH = "com.android.bluetooth";
    public static final String SETTINGS = "com.android.settings";
    // Debug controls (adb, needs android.permission.DUMP):
    //   am broadcast -p com.android.bluetooth -a <prefix>.SELECT_CODEC --ei index 18
    //   am broadcast -p com.android.bluetooth -a <prefix>.SPATIAL_ON | SPATIAL_OFF
    private static final String ACTION_PREFIX = "io.github.wxxsfxyzm.galaxybudsport.action.";
    private static final String SELECT_CODEC = ACTION_PREFIX + "SELECT_CODEC";
    private static final String SPATIAL_ON = ACTION_PREFIX + "SPATIAL_ON";
    private static final String SPATIAL_OFF = ACTION_PREFIX + "SPATIAL_OFF";
    private static final String TAG = "SscPort";

    private final XposedModule module;
    private final String nativeLibrary;
    private final List<CodecFeature> features;
    private final List<CodecInfo> codecs = new ArrayList<>();
    private boolean bluetooth;
    private boolean bluetoothLoaded;

    public PortRuntime(XposedModule module, String nativeLibrary, List<CodecFeature> features) {
        this.module = module;
        this.nativeLibrary = nativeLibrary;
        this.features = List.copyOf(features);
        for (CodecFeature feature : this.features) codecs.add(feature.codec());
    }

    public void onModuleLoaded(XposedModuleInterface.ModuleLoadedParam param) {
        String process = param.getProcessName();
        bluetooth = BLUETOOTH.equals(process);
        if (bluetooth || SETTINGS.equals(process)) CodecTypes.installCreateFromType(module, codecs);
        if (bluetooth) CodecTypes.installStatusEncoderFormat(module, codecs);
        if (process == null) return;
        for (CodecFeature feature : features) feature.onProcess(module, process);
    }

    public void onPackageReady(XposedModuleInterface.PackageReadyParam param) {
        if (!param.isFirstPackage()) return;
        String name = param.getPackageName();
        if (SETTINGS.equals(name)) {
            SettingsCodecBridge.install(module, param.getClassLoader(), codecs);
            for (CodecFeature feature : features) feature.onSettings(module, param.getClassLoader());
            return;
        }
        if (BLUETOOTH.equals(name)) {
            if (bluetooth && !bluetoothLoaded) startBluetooth(param.getClassLoader());
            return;
        }
        for (CodecFeature feature : features) feature.onPackage(module, param);
    }

    private void startBluetooth(ClassLoader loader) {
        bluetoothLoaded = true;
        System.loadLibrary(nativeLibrary);
        Handler handler = new Handler(Looper.getMainLooper());
        handler.post(new Runnable() {
            int attempts;

            @Override public void run() {
                // The Bluetooth application object exists shortly after package load.
                Context context = ActivityThread.currentApplication();
                if (context == null) {
                    if (++attempts < 120) handler.postDelayed(this, 250);
                    else Log.e(TAG, "stage=control context_unavailable");
                    return;
                }
                BluetoothHost host = new BluetoothHost(module, context, handler, loader);
                registerDebugControls(host);
                for (CodecFeature feature : features) feature.onBluetooth(host);
                Log.w(TAG, "stage=control registered=1");
            }
        });
    }

    private static void registerDebugControls(BluetoothHost host) {
        IntentFilter filter = new IntentFilter();
        filter.addAction(SELECT_CODEC);
        filter.addAction(SPATIAL_ON);
        filter.addAction(SPATIAL_OFF);
        host.context.registerReceiver(new BroadcastReceiver() {
            @Override public void onReceive(Context ctx, Intent intent) {
                if (intent == null || !BLUETOOTH.equals(intent.getPackage())) return;
                String action = intent.getAction();
                if (SPATIAL_ON.equals(action) || SPATIAL_OFF.equals(action)) {
                    NativeBridge.setSpatial(SPATIAL_ON.equals(action));
                    return;
                }
                try {
                    host.selectCodec(intent.getIntExtra("index", -1));
                } catch (Exception error) {
                    Log.e(TAG, "stage=codec_select error=" + error.getClass().getSimpleName());
                }
            }
        }, filter, Manifest.permission.DUMP, host.handler, Context.RECEIVER_EXPORTED);
    }
}
