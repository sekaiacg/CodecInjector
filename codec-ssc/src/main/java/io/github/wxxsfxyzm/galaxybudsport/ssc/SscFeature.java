package io.github.wxxsfxyzm.galaxybudsport.ssc;

import android.app.BroadcastOptions;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothCodecConfig;
import android.bluetooth.BluetoothCodecStatus;
import android.bluetooth.BluetoothCodecType;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothProfile;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.util.Log;
import io.github.libxposed.api.XposedInterface;
import io.github.libxposed.api.XposedModuleInterface;
import io.github.wxxsfxyzm.galaxybudsport.core.BluetoothHost;
import io.github.wxxsfxyzm.galaxybudsport.core.CodecFeature;
import io.github.wxxsfxyzm.galaxybudsport.core.CodecInfo;
import io.github.wxxsfxyzm.galaxybudsport.core.NativeBridge;
import io.github.wxxsfxyzm.galaxybudsport.core.PortRuntime;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Samsung Seamless Codec with Galaxy Buds: the Buds manager's 360 audio, head tracking and UHQ
 * switches, per-device UHQ, and the SSC playback quality setting.
 */
public final class SscFeature implements CodecFeature {
    static final String BUDS_MANAGER = "com.samsung.accessory.budsunitemgr";
    static final CodecInfo CODEC = new CodecInfo(18, 0x00000001030075ffL, "SSC");
    private static final String TAG = "SscPort";
    private static volatile int qualityMode;
    // The Buds manager's last 360 audio switch, reapplied when an SSC stream is configured: the
    // spatial stage is shared with other codecs (AirPods choose their own).
    private static volatile Boolean budsSpatial;

    @Override public CodecInfo codec() {
        return CODEC;
    }

    @Override public void onProcess(XposedInterface xposed, String processName) {
        if (processName.startsWith(BUDS_MANAGER)) BudsPoseTap.install(xposed);
    }

    @Override public void onPackage(XposedInterface xposed, XposedModuleInterface.PackageReadyParam param) {
        if (!BUDS_MANAGER.equals(param.getPackageName())) return;
        try {
            BudsSettingsBridge.install(xposed, param.getApplicationInfo().sourceDir,
                    param.getClassLoader());
        } catch (Throwable error) {
            Log.e("SscPortBuds", "stage=buds_settings error=" + error);
        }
    }

    @Override public void onSettings(XposedInterface xposed, ClassLoader loader) {
        try {
            SettingsQualityBridge.install(xposed, loader);
        } catch (Throwable error) {
            Log.e("SscPortSettings", "stage=settings_quality error=" + error);
        }
    }

    @Override public void onBluetooth(BluetoothHost host) {
        // 360 audio switch from the Buds manager's settings page; accepted only from it.
        host.context.registerReceiver(new BroadcastReceiver() {
            @Override public void onReceive(Context ctx, Intent intent) {
                if (!BUDS_MANAGER.equals(getSentFromPackage())) {
                    Log.w(TAG, "stage=buds_spatial rejected_sender=" + getSentFromPackage());
                    return;
                }
                budsSpatial = intent.getBooleanExtra(BudsSettingsBridge.EXTRA_ENABLED, true);
                NativeBridge.setSpatial(budsSpatial);
            }
        }, new IntentFilter(BudsSettingsBridge.ACTION_SPATIAL), null, host.handler,
                Context.RECEIVER_EXPORTED);
        host.addCodecListener((device, status) -> {
            Boolean spatial = budsSpatial;
            if (spatial != null && CODEC.matches(status.getCodecConfig())) {
                host.handler.post(() -> NativeBridge.setSpatial(spatial));
            }
        });
        registerUhq(host);
        registerQuality(host);
        PoseLink.serve(host.context, host.handler, NativeBridge::setYaw);
    }

    /** Bitrate mode chosen in developer options; adaptive again on every new connection. */
    private static void setQualityMode(Context context, int mode) {
        qualityMode = mode >= 0 && mode <= 3 ? mode : 0;
        SscNative.nativeSetQualityMode(qualityMode);
        BroadcastOptions options = BroadcastOptions.makeBasic();
        options.setShareIdentityEnabled(true);
        context.sendBroadcast(new Intent(SettingsQualityBridge.ACTION_STATE)
                .setPackage(PortRuntime.SETTINGS)
                .putExtra(SettingsQualityBridge.EXTRA_MODE, qualityMode), null, options.toBundle());
    }

    private static void registerQuality(BluetoothHost host) {
        SscNative.nativeSetQualityMode(0);
        IntentFilter filter = new IntentFilter();
        filter.addAction(SettingsQualityBridge.ACTION_SET);
        filter.addAction(SettingsQualityBridge.ACTION_QUERY);
        host.context.registerReceiver(new BroadcastReceiver() {
            @Override public void onReceive(Context ctx, Intent intent) {
                if (!PortRuntime.SETTINGS.equals(getSentFromPackage())) {
                    Log.w(TAG, "stage=ssc_quality rejected_sender=" + getSentFromPackage());
                    return;
                }
                int mode = SettingsQualityBridge.ACTION_SET.equals(intent.getAction())
                        ? intent.getIntExtra(SettingsQualityBridge.EXTRA_MODE, 0) : qualityMode;
                setQualityMode(ctx, mode);
            }
        }, filter, null, host.handler, Context.RECEIVER_EXPORTED);
    }

    private static boolean hasMandatory(BluetoothCodecStatus status) {
        for (BluetoothCodecConfig config : status.getCodecsSelectableCapabilities()) {
            if (config.getCodecType() == BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC) return true;
        }
        return false;
    }

    /**
     * Renegotiates the device's SSC stream at 96 kHz (UHQ) or 48 kHz when its current rate differs.
     * Does nothing while the device is not streaming SSC.
     */
    private static void selectSscRate(BluetoothHost host, BluetoothDevice device, boolean uhq)
            throws ReflectiveOperationException {
        BluetoothCodecStatus status = host.codecStatus(device);
        BluetoothCodecConfig current = status == null ? null : status.getCodecConfig();
        if (!CODEC.matches(current)) {
            Log.w(TAG, "stage=uhq_select skipped=not_ssc");
            return;
        }
        BluetoothCodecType type = current.getExtendedCodecType();
        int rate = uhq ? BluetoothCodecConfig.SAMPLE_RATE_96000 : BluetoothCodecConfig.SAMPLE_RATE_48000;
        if (current.getSampleRate() == rate) return;
        host.setCodecConfigPreference(device, new BluetoothCodecConfig.Builder()
                .setExtendedCodecType(type)
                .setCodecPriority(BluetoothCodecConfig.CODEC_PRIORITY_HIGHEST)
                .setSampleRate(rate)
                .setBitsPerSample(current.getBitsPerSample())
                .setChannelMode(current.getChannelMode())
                .build());
        Log.w(TAG, "stage=uhq_select rate=" + (uhq ? 96000 : 48000));
    }

    /**
     * Per-device UHQ, kept in the Bluetooth process so it applies before the manager runs. Codec
     * selection in the native stack does not know the peer, so the preference of the device that is
     * connecting is handed to it at CONNECTING (before AVDTP configuration); the first SSC codec
     * configuration of each connection is then checked once and renegotiated if it does not match
     * (e.g. two devices connecting at the same time). Later codec changes are the user's.
     */
    private static void registerUhq(BluetoothHost host) {
        SharedPreferences prefs = host.context.createDeviceProtectedStorageContext()
                .getSharedPreferences("ssc_port", Context.MODE_PRIVATE);
        // Addresses with an A2DP connection in progress or up, and those whose first SSC codec
        // configuration was already checked; both cleared on disconnect.
        Set<String> pendingCheck = ConcurrentHashMap.newKeySet();
        Set<String> checked = ConcurrentHashMap.newKeySet();
        SscNative.nativeSetUhq(false);
        host.context.registerReceiver(new BroadcastReceiver() {
            @Override public void onReceive(Context ctx, Intent intent) {
                if (!BUDS_MANAGER.equals(getSentFromPackage())) {
                    Log.w(TAG, "stage=buds_uhq rejected_sender=" + getSentFromPackage());
                    return;
                }
                String address = intent.getStringExtra(BudsSettingsBridge.EXTRA_ADDRESS);
                if (address == null || !BluetoothAdapter.checkBluetoothAddress(address)) return;
                boolean on = intent.getBooleanExtra(BudsSettingsBridge.EXTRA_ENABLED, false);
                boolean changed = prefs.getBoolean(BudsSettingsBridge.uhqKey(address), false) != on;
                prefs.edit().putBoolean(BudsSettingsBridge.uhqKey(address), on).apply();
                Log.w(TAG, "stage=buds_uhq enabled=" + on + " changed=" + changed);
                if (!changed) return;
                SscNative.nativeSetUhq(on);
                try {
                    selectSscRate(host, BluetoothAdapter.getDefaultAdapter().getRemoteDevice(address), on);
                } catch (Exception error) {
                    Log.e(TAG, "stage=uhq_select error=" + error.getClass().getSimpleName());
                }
            }
        }, new IntentFilter(BudsSettingsBridge.ACTION_UHQ), null, host.handler,
                Context.RECEIVER_EXPORTED);

        host.addConnectionListener((device, state) -> {
            String address = device.getAddress();
            if (state == BluetoothProfile.STATE_CONNECTING || state == BluetoothProfile.STATE_CONNECTED) {
                // New connection: before AVDTP configuration.
                if (pendingCheck.add(address)) {
                    SscNative.nativeSetUhq(prefs.getBoolean(BudsSettingsBridge.uhqKey(address), false));
                    host.handler.post(() -> setQualityMode(host.context, 0));
                }
            } else if (state == BluetoothProfile.STATE_DISCONNECTED) {
                pendingCheck.remove(address);
                checked.remove(address);
            }
        });
        host.addCodecListener((device, status) -> {
            // First SSC configuration of this connection: correct the rate once. When the buds open
            // the stream, their SET_CONFIGURATION is reported before the peer's capabilities are
            // known; A2dpCodecConfig refuses a preference until SBC is selectable ("must have
            // mandatory codec before changing"), so wait for the report that has it.
            if (!CODEC.matches(status.getCodecConfig()) || !hasMandatory(status)
                    || !checked.add(device.getAddress())) {
                return;
            }
            boolean uhq = prefs.getBoolean(BudsSettingsBridge.uhqKey(device.getAddress()), false);
            host.handler.post(() -> {
                SscNative.nativeSetUhq(uhq);
                try {
                    selectSscRate(host, device, uhq);
                } catch (Exception error) {
                    Log.e(TAG, "stage=uhq_select error=" + error.getClass().getSimpleName());
                }
            });
        });
        Log.w(TAG, "stage=uhq_connection installed=1");
    }
}
