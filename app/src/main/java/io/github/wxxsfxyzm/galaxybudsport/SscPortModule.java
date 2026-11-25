package io.github.wxxsfxyzm.galaxybudsport;

import android.Manifest;
import android.app.ActivityThread;
import android.app.BroadcastOptions;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothCodecType;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothCodecConfig;
import android.bluetooth.BluetoothCodecStatus;
import android.bluetooth.BluetoothDevice;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Handler;
import android.os.Looper;
import android.os.Parcel;
import android.util.Log;
import java.lang.reflect.Method;
import java.lang.reflect.Field;
import java.util.Optional;
import io.github.libxposed.api.XposedModule;
import io.github.libxposed.api.XposedInterface;
import io.github.libxposed.api.XposedModuleInterface;

public final class SscPortModule extends XposedModule {
    private static final String PROCESS = "com.android.bluetooth";
    private static final String SELECT_SSC = "io.github.wxxsfxyzm.galaxybudsport.action.SELECT_SSC";
    private static final String SELECT_AAC = "io.github.wxxsfxyzm.galaxybudsport.action.SELECT_AAC";
    private static final String SPATIAL_ON = "io.github.wxxsfxyzm.galaxybudsport.action.SPATIAL_ON";
    private static final String SPATIAL_OFF = "io.github.wxxsfxyzm.galaxybudsport.action.SPATIAL_OFF";
    private boolean bluetooth;
    private boolean loaded;
    private ClassLoader bluetoothClassLoader;

    private static native void nativeSetSpatial(boolean on);
    private static native void nativeSetYaw(float radians);
    private static native void nativeSetUhq(boolean preferred);
    private static native void nativeSetQualityMode(int mode);
    private static final String SETTINGS = "com.android.settings";
    private static volatile int qualityMode;

    /** Bitrate mode chosen in developer options; adaptive again on every new connection. */
    private static void setQualityMode(Context context, int mode) {
        qualityMode = mode >= 0 && mode <= 3 ? mode : 0;
        nativeSetQualityMode(qualityMode);
        BroadcastOptions options = BroadcastOptions.makeBasic();
        options.setShareIdentityEnabled(true);
        context.sendBroadcast(new Intent(SettingsQualityBridge.ACTION_STATE)
                .setPackage(SETTINGS)
                .putExtra(SettingsQualityBridge.EXTRA_MODE, qualityMode), null, options.toBundle());
    }

    private void registerQuality(Context context, Handler handler) {
        nativeSetQualityMode(0);
        IntentFilter filter = new IntentFilter();
        filter.addAction(SettingsQualityBridge.ACTION_SET);
        filter.addAction(SettingsQualityBridge.ACTION_QUERY);
        context.registerReceiver(new BroadcastReceiver() {
            @Override public void onReceive(Context ctx, Intent intent) {
                if (!SETTINGS.equals(getSentFromPackage())) {
                    Log.w("SscPort", "stage=ssc_quality rejected_sender=" + getSentFromPackage());
                    return;
                }
                int mode = SettingsQualityBridge.ACTION_SET.equals(intent.getAction())
                        ? intent.getIntExtra(SettingsQualityBridge.EXTRA_MODE, 0) : qualityMode;
                setQualityMode(ctx, mode);
            }
        }, filter, null, handler, Context.RECEIVER_EXPORTED);
    }

    private static final long SSC_CODEC_ID = 0x00000001030075ffL;

    private void selectCodec(int requestedIndex) throws ReflectiveOperationException {
        Object service = a2dpService();
        Class<?> serviceClass = service.getClass();
        Method getActive = serviceClass.getDeclaredMethod("getActiveDevice");
        getActive.setAccessible(true);
        BluetoothDevice device = (BluetoothDevice) getActive.invoke(service);
        Method getStatus = serviceClass.getDeclaredMethod("getCodecStatus", BluetoothDevice.class);
        getStatus.setAccessible(true);
        BluetoothCodecStatus status = (BluetoothCodecStatus) getStatus.invoke(service, device);
        BluetoothCodecConfig selected = null;
        for (BluetoothCodecConfig candidate : status.getCodecsSelectableCapabilities()) {
            if (candidate.getCodecType() == requestedIndex) { selected = candidate; break; }
        }
        if (selected == null) throw new IllegalStateException("codec_not_selectable");
        BluetoothCodecConfig request = new BluetoothCodecConfig.Builder()
                .setExtendedCodecType(selected.getExtendedCodecType())
                .setCodecPriority(BluetoothCodecConfig.CODEC_PRIORITY_HIGHEST)
                .setSampleRate(selected.getSampleRate())
                .setBitsPerSample(selected.getBitsPerSample())
                .setChannelMode(selected.getChannelMode())
                .build();
        Method select = serviceClass.getDeclaredMethod("setCodecConfigPreference",
                BluetoothDevice.class, BluetoothCodecConfig.class);
        select.setAccessible(true);
        select.invoke(service, device, request);
        Log.w("SscPort", "stage=codec_select requested=" + requestedIndex);
    }

    private Object a2dpService() throws ReflectiveOperationException {
        Class<?> adapterClass = Class.forName("com.android.bluetooth.btservice.AdapterService", false,
                bluetoothClassLoader);
        Object adapter = adapterClass.getMethod("deprecatedGetAdapterService").invoke(null);
        Optional<?> a2dp = (Optional<?>) adapterClass.getMethod("getA2dpService").invoke(adapter);
        return a2dp.orElseThrow();
    }

    /**
     * Renegotiates the device's SSC stream at 96 kHz (UHQ) or 48 kHz when its current rate differs.
     * Does nothing while the device is not streaming SSC.
     */
    private void selectSscRate(BluetoothDevice device, boolean uhq)
            throws ReflectiveOperationException {
        Object service = a2dpService();
        Class<?> serviceClass = service.getClass();
        Method getStatus = serviceClass.getDeclaredMethod("getCodecStatus", BluetoothDevice.class);
        getStatus.setAccessible(true);
        BluetoothCodecStatus status = (BluetoothCodecStatus) getStatus.invoke(service, device);
        BluetoothCodecConfig current = status == null ? null : status.getCodecConfig();
        BluetoothCodecType type = current == null ? null : current.getExtendedCodecType();
        if (type == null || type.getCodecId() != SSC_CODEC_ID) {
            Log.w("SscPort", "stage=uhq_select skipped=not_ssc");
            return;
        }
        int rate = uhq ? BluetoothCodecConfig.SAMPLE_RATE_96000 : BluetoothCodecConfig.SAMPLE_RATE_48000;
        if (current.getSampleRate() == rate) return;
        BluetoothCodecConfig request = new BluetoothCodecConfig.Builder()
                .setExtendedCodecType(type)
                .setCodecPriority(BluetoothCodecConfig.CODEC_PRIORITY_HIGHEST)
                .setSampleRate(rate)
                .setBitsPerSample(current.getBitsPerSample())
                .setChannelMode(current.getChannelMode())
                .build();
        Method select = serviceClass.getDeclaredMethod("setCodecConfigPreference",
                BluetoothDevice.class, BluetoothCodecConfig.class);
        select.setAccessible(true);
        select.invoke(service, device, request);
        Log.w("SscPort", "stage=uhq_select rate=" + (uhq ? 96000 : 48000));
    }

    /**
     * Per-device UHQ, kept in the Bluetooth process so it applies before the manager runs.
     * Codec selection in the native stack does not know the peer, so the preference of the device
     * that is connecting is handed to it at CONNECTING (before AVDTP configuration); the first
     * codec configuration of each connection is then checked once and renegotiated if it does
     * not match (e.g. two devices connecting at the same time). Later codec changes are the user's.
     */
    private void registerUhq(Context context, Handler handler) {
        android.content.SharedPreferences prefs = context.createDeviceProtectedStorageContext()
                .getSharedPreferences("ssc_port", Context.MODE_PRIVATE);
        // Addresses with an A2DP connection in progress or up, and those whose first SSC codec
        // configuration was already checked; both cleared on disconnect.
        java.util.Set<String> pendingCheck = java.util.concurrent.ConcurrentHashMap.newKeySet();
        java.util.Set<String> checked = java.util.concurrent.ConcurrentHashMap.newKeySet();
        nativeSetUhq(false);
        context.registerReceiver(new BroadcastReceiver() {
            @Override public void onReceive(Context ctx, Intent intent) {
                if (!BUDS_MANAGER.equals(getSentFromPackage())) {
                    Log.w("SscPort", "stage=buds_uhq rejected_sender=" + getSentFromPackage());
                    return;
                }
                String address = intent.getStringExtra(BudsSettingsBridge.EXTRA_ADDRESS);
                if (address == null || !BluetoothAdapter.checkBluetoothAddress(address)) return;
                boolean on = intent.getBooleanExtra(BudsSettingsBridge.EXTRA_ENABLED, false);
                boolean changed = prefs.getBoolean(BudsSettingsBridge.uhqKey(address), false) != on;
                prefs.edit().putBoolean(BudsSettingsBridge.uhqKey(address), on).apply();
                Log.w("SscPort", "stage=buds_uhq enabled=" + on + " changed=" + changed);
                if (!changed) return;
                nativeSetUhq(on);
                try {
                    selectSscRate(BluetoothAdapter.getDefaultAdapter().getRemoteDevice(address), on);
                } catch (Exception error) {
                    Log.e("SscPort", "stage=uhq_select error=" + error.getClass().getSimpleName());
                }
            }
        }, new IntentFilter(BudsSettingsBridge.ACTION_UHQ), null, handler, Context.RECEIVER_EXPORTED);

        // The A2DP connection and codec broadcasts are not delivered to receivers registered in
        // the Bluetooth process itself, so A2dpService's own callbacks are hooked instead.
        try {
            Class<?> service = Class.forName("com.android.bluetooth.a2dp.A2dpService", false,
                    bluetoothClassLoader);
            Method connection = service.getDeclaredMethod("onConnectionStateChangedFromNative",
                    BluetoothDevice.class, int.class, int.class);
            Method codec = service.getDeclaredMethod("codecConfigUpdated",
                    BluetoothDevice.class, BluetoothCodecStatus.class, boolean.class);
            hook(connection).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                    .intercept(chain -> {
                        BluetoothDevice device = (BluetoothDevice) chain.getArg(0);
                        int state = (Integer) chain.getArg(1);
                        if (device != null && (state == BluetoothProfile.STATE_CONNECTING
                                || state == BluetoothProfile.STATE_CONNECTED)) {
                            if (pendingCheck.add(device.getAddress())) {
                                // New connection: before AVDTP configuration.
                                nativeSetUhq(prefs.getBoolean(
                                        BudsSettingsBridge.uhqKey(device.getAddress()), false));
                                handler.post(() -> setQualityMode(context, 0));
                            }
                        } else if (device != null && state == BluetoothProfile.STATE_DISCONNECTED) {
                            pendingCheck.remove(device.getAddress());
                            checked.remove(device.getAddress());
                        }
                        return chain.proceed();
                    });
            hook(codec).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                    .intercept(chain -> {
                        Object result = chain.proceed();
                        BluetoothDevice device = (BluetoothDevice) chain.getArg(0);
                        BluetoothCodecStatus status = (BluetoothCodecStatus) chain.getArg(1);
                        BluetoothCodecConfig config = status == null ? null : status.getCodecConfig();
                        BluetoothCodecType type = config == null ? null : config.getExtendedCodecType();
                        if (device == null || type == null || type.getCodecId() != SSC_CODEC_ID) {
                            return result;
                        }
                        // First SSC configuration of this connection: correct the rate once.
                        if (!checked.add(device.getAddress())) return result;
                        boolean uhq = prefs.getBoolean(
                                BudsSettingsBridge.uhqKey(device.getAddress()), false);
                        handler.post(() -> {
                            nativeSetUhq(uhq);
                            try {
                                selectSscRate(device, uhq);
                            } catch (Exception error) {
                                Log.e("SscPort", "stage=uhq_select error="
                                        + error.getClass().getSimpleName());
                            }
                        });
                        return result;
                    });
            Log.w("SscPort", "stage=uhq_connection installed=1");
        } catch (ReflectiveOperationException error) {
            Log.e("SscPort", "stage=uhq_connection error=" + error.getClass().getSimpleName());
        }
    }

    @Override
    public void onModuleLoaded(XposedModuleInterface.ModuleLoadedParam param) {
        bluetooth = PROCESS.equals(param.getProcessName());
        if (bluetooth || SETTINGS.equals(param.getProcessName())) installSscCodecType();
        if (bluetooth) installCodecType();
        if (param.getProcessName() != null && param.getProcessName().startsWith(BUDS_MANAGER)) {
            installBudsTap();
        }
    }

    private static final String BUDS_MANAGER = "com.samsung.accessory.budsunitemgr";

    private void installBudsTap() {
        try {
            for (String name : new String[] {"createInsecureRfcommSocketToServiceRecord",
                    "createRfcommSocketToServiceRecord"}) {
                Method create = BluetoothDevice.class.getDeclaredMethod(name, java.util.UUID.class);
                hook(create).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                        .intercept(chain -> {
                            Object socket = chain.proceed();
                            if (socket instanceof android.bluetooth.BluetoothSocket
                                    && BudsPoseTap.BUDS_SPP.equals(chain.getArg(0))) {
                                BudsPoseTap.track((android.bluetooth.BluetoothSocket) socket);
                            }
                            return socket;
                        });
            }
            Method input = android.bluetooth.BluetoothSocket.class.getDeclaredMethod("getInputStream");
            hook(input).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                    .intercept(chain -> BudsPoseTap.wrapInput(
                            (android.bluetooth.BluetoothSocket) chain.getThisObject(),
                            (java.io.InputStream) chain.proceed()));
            Method output = android.bluetooth.BluetoothSocket.class.getDeclaredMethod("getOutputStream");
            hook(output).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                    .intercept(chain -> BudsPoseTap.wrapOutput(
                            (android.bluetooth.BluetoothSocket) chain.getThisObject(),
                            (java.io.OutputStream) chain.proceed()));
            Log.w("SscPortBuds", "stage=buds_tap event=installed");
        } catch (ReflectiveOperationException error) {
            Log.e("SscPortBuds", "stage=buds_tap error=" + error.getClass().getSimpleName());
        }
    }

    /**
     * BluetoothCodecType.createFromType only knows the built-in codecs; codec configs built from
     * the int type 18 (the stack, Settings' config store) need it to return SSC.
     */
    private void installSscCodecType() {
        Parcel parcel = Parcel.obtain();
        try {
            parcel.writeInt(18);
            parcel.writeLong(SSC_CODEC_ID);
            parcel.writeString("SSC");
            parcel.setDataPosition(0);
            BluetoothCodecType type = BluetoothCodecType.CREATOR.createFromParcel(parcel);
            Method method = BluetoothCodecType.class.getDeclaredMethod("createFromType", int.class);
            hook(method).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                    .intercept(chain -> ((Number) chain.getArg(0)).intValue() == 18
                            ? type : chain.proceed());
            Log.w("SscPort", "stage=ssc_codec_type installed=1");
        } catch (ReflectiveOperationException error) {
            Log.e("SscPort", "stage=ssc_codec_type error=" + error.getClass().getSimpleName());
        } finally {
            parcel.recycle();
        }
    }

    private void installCodecType() {
        try {
            Field encoderFormat = BluetoothCodecConfig.class.getDeclaredField("mEncoderFormat");
            encoderFormat.setAccessible(true);
            Method writeStatus = BluetoothCodecStatus.class.getDeclaredMethod(
                    "writeToParcel", Parcel.class, int.class);
            hook(writeStatus).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                    .intercept(chain -> {
                        BluetoothCodecStatus status = (BluetoothCodecStatus) chain.getThisObject();
                        BluetoothCodecConfig config = status.getCodecConfig();
                        BluetoothCodecType codecType = config == null ? null : config.getExtendedCodecType();
                        if (codecType == null || codecType.getCodecId() != 0x00000001030075ffL)
                            return chain.proceed();
                        synchronized (config) {
                            int saved = encoderFormat.getInt(config);
                            encoderFormat.setInt(config, 0x7f000000);
                            try { return chain.proceed(); }
                            finally { encoderFormat.setInt(config, saved); }
                        }
                    });
            Log.w("SscPort", "stage=codec_status_type installed=1");
        } catch (ReflectiveOperationException error) {
            Log.e("SscPort", "stage=codec_status_type error=" + error.getClass().getSimpleName());
        }
    }

    @Override
    public void onPackageReady(XposedModuleInterface.PackageReadyParam param) {
        if (SETTINGS.equals(param.getPackageName()) && param.isFirstPackage()) {
            try {
                SettingsCodecBridge.install(this, param.getClassLoader());
                SettingsQualityBridge.install(this, param.getClassLoader());
            } catch (Throwable error) {
                Log.e("SscPortSettings", "stage=settings_quality error=" + error);
            }
            return;
        }
        if (BUDS_MANAGER.equals(param.getPackageName()) && param.isFirstPackage()) {
            try {
                BudsSettingsBridge.install(this, param.getApplicationInfo().sourceDir,
                        param.getClassLoader());
            } catch (Throwable error) {
                Log.e("SscPortBuds", "stage=buds_settings error=" + error);
            }
            return;
        }
        if (!bluetooth || loaded || !param.isFirstPackage()
                || !PROCESS.equals(param.getPackageName())) return;
        loaded = true;
        bluetoothClassLoader = param.getClassLoader();
        System.loadLibrary("sscport");
        Handler handler = new Handler(Looper.getMainLooper());
        handler.post(new Runnable() {
            int attempts;
            @Override public void run() {
                // The Bluetooth application object exists shortly after package load.
                Context context = ActivityThread.currentApplication();
                if (context == null) {
                    if (++attempts < 120) handler.postDelayed(this, 250);
                    else Log.e("SscPort", "stage=control context_unavailable");
                    return;
                }
                IntentFilter filter = new IntentFilter();
                filter.addAction(SELECT_SSC);
                filter.addAction(SELECT_AAC);
                filter.addAction(SPATIAL_ON);
                filter.addAction(SPATIAL_OFF);
                context.registerReceiver(new BroadcastReceiver() {
                    @Override public void onReceive(Context ctx, Intent intent) {
                        if (intent == null || !PROCESS.equals(intent.getPackage())) return;
                        try {
                            String action = intent.getAction();
                            if (SPATIAL_ON.equals(action) || SPATIAL_OFF.equals(action)) {
                                nativeSetSpatial(SPATIAL_ON.equals(action));
                                return;
                            }
                            selectCodec(SELECT_SSC.equals(action) ? 18 : 1);
                        } catch (Exception error) {
                            Log.e("SscPort", "stage=codec_select error="
                                    + error.getClass().getSimpleName());
                        }
                    }
                }, filter, Manifest.permission.DUMP, handler, Context.RECEIVER_EXPORTED);
                // Switches from the Buds manager's settings page; accepted only from it.
                context.registerReceiver(new BroadcastReceiver() {
                    @Override public void onReceive(Context ctx, Intent intent) {
                        if (!BUDS_MANAGER.equals(getSentFromPackage())) {
                            Log.w("SscPort", "stage=buds_spatial rejected_sender="
                                    + getSentFromPackage());
                            return;
                        }
                        boolean on = intent.getBooleanExtra(BudsSettingsBridge.EXTRA_ENABLED, true);
                        nativeSetSpatial(on);
                    }
                }, new IntentFilter(BudsSettingsBridge.ACTION_SPATIAL), null, handler,
                        Context.RECEIVER_EXPORTED);
                registerUhq(context, handler);
                registerQuality(context, handler);
                PoseLink.serve(context, handler, SscPortModule::nativeSetYaw);
                Log.w("SscPort", "stage=control registered=1");
            }
        });
    }
}
