package io.github.wxxsfxyzm.galaxybudsport.aeld;

import android.app.BroadcastOptions;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
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

    @Override public java.util.List<String> poseSources() {
        return java.util.List.of(AirPodsChannelTap.PROCESS);
    }

    @Override public void onProcess(XposedInterface xposed, String processName) {
        if (AirPodsChannelTap.PROCESS.equals(processName)) {
            AirPodsChannelTap.install(xposed, new AirPodsPose.Tracker());
        }
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
                    BluetoothDevice active = host.activeDevice();
                    if (active != null && address.equalsIgnoreCase(active.getAddress())
                            && CODEC.matches(host.codecStatus(active).getCodecConfig())) {
                        apply(host.context, mode, active);
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
            host.handler.post(() -> apply(host.context, mode, device));
        });
    }

    private static void apply(Context context, int mode, BluetoothDevice device) {
        NativeBridge.setSpatial(mode != AirPodsSettingsBridge.MODE_OFF);
        NativeBridge.setYaw(0f);
        NativeBridge.setPitch(0f);
        // The tap in HyperOS' AirPods service runs the motion session over its AACP channel.
        BroadcastOptions options = BroadcastOptions.makeBasic();
        options.setShareIdentityEnabled(true);
        context.sendBroadcast(new Intent(AirPodsChannelTap.ACTION_TRACKING)
                .setPackage(AirPodsChannelTap.PROCESS)
                .putExtra(AirPodsChannelTap.EXTRA_ADDRESS, device.getAddress())
                .putExtra(AirPodsChannelTap.EXTRA_ENABLED,
                        mode == AirPodsSettingsBridge.MODE_HEAD_TRACKING), null, options.toBundle());
        Log.w(TAG, "stage=airpods_spatial mode=" + mode);
    }
}
