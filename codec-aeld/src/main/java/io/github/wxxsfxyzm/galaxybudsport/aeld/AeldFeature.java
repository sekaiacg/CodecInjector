package io.github.wxxsfxyzm.galaxybudsport.aeld;

import android.bluetooth.BluetoothAdapter;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.util.Log;
import io.github.libxposed.api.XposedInterface;
import io.github.wxxsfxyzm.galaxybudsport.core.BluetoothHost;
import io.github.wxxsfxyzm.galaxybudsport.core.CodecFeature;
import io.github.wxxsfxyzm.galaxybudsport.core.CodecInfo;
import io.github.wxxsfxyzm.galaxybudsport.core.NativeBridge;
import io.github.wxxsfxyzm.galaxybudsport.core.PortRuntime;

/**
 * Apple AAC-ELD for AirPods. The codec itself is native (codec type, Settings codec list and label
 * come from the core). Spatial audio is chosen per device on HyperOS' AirPods page (off, fixed, head
 * tracking) and applied when that device's AAC-ELD stream is configured. Head tracking is not
 * implemented yet and renders like fixed.
 */
public final class AeldFeature implements CodecFeature {
    static final CodecInfo CODEC = new CodecInfo(17, 0x0000008001004cffL, "AAC-ELD");
    private static final String TAG = "SscPort";

    @Override public CodecInfo codec() {
        return CODEC;
    }

    @Override public void onSettings(XposedInterface xposed, ClassLoader loader) {
        try {
            AirPodsSettingsBridge.install(xposed, loader);
        } catch (Throwable error) {
            Log.e("SscPortSettings", "stage=airpods_settings error=" + error);
        }
    }

    @Override public void onBluetooth(BluetoothHost host) {
        SharedPreferences prefs = host.context.createDeviceProtectedStorageContext()
                .getSharedPreferences("aeld_port", Context.MODE_PRIVATE);
        host.context.registerReceiver(new BroadcastReceiver() {
            @Override public void onReceive(Context ctx, Intent intent) {
                if (!PortRuntime.SETTINGS.equals(getSentFromPackage())) {
                    Log.w(TAG, "stage=airpods_spatial rejected_sender=" + getSentFromPackage());
                    return;
                }
                String address = intent.getStringExtra(AirPodsSettingsBridge.EXTRA_ADDRESS);
                if (address == null || !BluetoothAdapter.checkBluetoothAddress(address)) return;
                int mode = intent.getIntExtra(AirPodsSettingsBridge.EXTRA_MODE,
                        AirPodsSettingsBridge.MODE_DEFAULT);
                prefs.edit().putInt(AirPodsSettingsBridge.key(address), mode).apply();
                try {
                    if (address.equalsIgnoreCase(host.activeDevice().getAddress())
                            && CODEC.matches(host.codecStatus(host.activeDevice()).getCodecConfig())) {
                        apply(mode);
                    }
                } catch (Exception error) {
                    Log.w(TAG, "stage=airpods_spatial active=" + error.getClass().getSimpleName());
                }
            }
        }, new IntentFilter(AirPodsSettingsBridge.ACTION_SPATIAL), null, host.handler,
                Context.RECEIVER_EXPORTED);
        host.addCodecListener((device, status) -> {
            if (!CODEC.matches(status.getCodecConfig())) return;
            int mode = prefs.getInt(AirPodsSettingsBridge.key(device.getAddress()),
                    AirPodsSettingsBridge.MODE_DEFAULT);
            host.handler.post(() -> apply(mode));
        });
    }

    private static void apply(int mode) {
        NativeBridge.setSpatial(mode != AirPodsSettingsBridge.MODE_OFF);
        // No AirPods head tracking yet: both modes face forward.
        NativeBridge.setYaw(0f);
        Log.w(TAG, "stage=airpods_spatial mode=" + mode);
    }
}
