package io.github.wxxsfxyzm.codecinjector.ssc;

import android.app.BroadcastOptions;
import android.bluetooth.BluetoothCodecConfig;
import android.bluetooth.BluetoothCodecType;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import io.github.libxposed.api.XposedInterface;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

/**
 * "SSC playback quality" in developer options, next to LDAC playback quality (ported from the
 * earlier app/ module). The choice goes to the Bluetooth process by broadcast and only changes
 * the encoder bitrate; it never renegotiates the codec. Like other codecs' quality settings it is
 * adaptive again on every new connection; the Bluetooth process reports the current mode.
 */
final class SettingsQualityBridge {
    static final String ACTION_SET = "io.github.wxxsfxyzm.codecinjector.action.SSC_QUALITY_SET";
    static final String ACTION_QUERY = "io.github.wxxsfxyzm.codecinjector.action.SSC_QUALITY_QUERY";
    static final String ACTION_STATE = "io.github.wxxsfxyzm.codecinjector.action.SSC_QUALITY_STATE";
    static final String EXTRA_MODE = "mode";
    private static final String TAG = "CodecInjectorSettings";
    private static final String KEY = "bluetooth_a2dp_ssc_bitrate_mode";
    private static final String LDAC_KEY = "bluetooth_select_a2dp_ldac_playback_quality";
    private static final String CATEGORY_KEY = "debug_networking_category";
    private static final String CODEC_CONTROLLER =
            "com.android.settings.development.BluetoothAudioCodecPreferenceController";
    private static final String CONTROLLER_BASE =
            "com.android.settings.development.AbstractBluetoothA2dpPreferenceController";
    // The drop-down used by the neighbouring Bluetooth codec preferences.
    private static final String DROPDOWN =
            "com.android.settingslib.miuisettings.preference.miuix.DropDownPreference";
    private static final long SSC_CODEC_ID = 0x00000001030075ffL;

    private static Object preference;
    private static int mode;
    private static boolean ssc;
    private static boolean uhq = true;
    private static boolean receiverRegistered;

    private SettingsQualityBridge() {
    }

    static void install(XposedInterface xposed, ClassLoader loader) throws ReflectiveOperationException {
        Class<?> controllerClass = Class.forName(CODEC_CONTROLLER, false, loader);
        Class<?> baseClass = Class.forName(CONTROLLER_BASE, false, loader);
        Class<?> screenClass = Class.forName("androidx.preference.PreferenceScreen", false, loader);
        Method display = baseClass.getDeclaredMethod("displayPreference", screenClass);
        Method currentIndex = controllerClass.getDeclaredMethod(
                "getCurrentA2dpSettingIndex", BluetoothCodecConfig.class);
        xposed.hook(display).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                .intercept(chain -> {
                    Object result = chain.proceed();
                    if (controllerClass.isInstance(chain.getThisObject())) {
                        schedule(chain.getArg(0), loader, 0);
                    }
                    return result;
                });
        // Called with the current codec config whenever the codec state is shown or changes.
        xposed.hook(currentIndex).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                .intercept(chain -> {
                    Object result = chain.proceed();
                    BluetoothCodecConfig config = (BluetoothCodecConfig) chain.getArg(0);
                    BluetoothCodecType type = config == null ? null : config.getExtendedCodecType();
                    ssc = type != null && type.getCodecId() == SSC_CODEC_ID;
                    uhq = ssc && config.getSampleRate() == BluetoothCodecConfig.SAMPLE_RATE_96000;
                    refresh();
                    return result;
                });
        Log.w(TAG, "stage=settings_quality event=installed");
    }

    private static void schedule(Object screen, ClassLoader loader, int attempt) {
        new Handler(Looper.getMainLooper()).postDelayed(() -> {
            try {
                ensure(screen, loader);
            } catch (Throwable error) {
                // The LDAC preference may not be attached to its parent yet.
                if (attempt < 5) schedule(screen, loader, attempt + 1);
                else Log.e(TAG, "stage=settings_quality event=insert_failed error=" + error);
            }
        }, attempt == 0 ? 0 : 50);
    }

    private static void ensure(Object screen, ClassLoader loader) throws ReflectiveOperationException {
        Method find = screen.getClass().getMethod("findPreference", CharSequence.class);
        Object existing = find.invoke(screen, KEY);
        Context context = (Context) screen.getClass().getMethod("getContext").invoke(screen);
        registerReceiver(context);
        if (existing != null) {
            bind(existing, loader);
            return;
        }
        Object ldac = find.invoke(screen, LDAC_KEY);
        Object group = find.invoke(screen, CATEGORY_KEY);
        if (ldac == null || group == null) throw new IllegalStateException("ldac_or_category_missing");
        int count = ((Number) group.getClass().getMethod("getPreferenceCount").invoke(group)).intValue();
        Method get = group.getClass().getMethod("getPreference", int.class);
        int ldacIndex = -1;
        for (int i = 0; i < count; i++) {
            Object sibling = get.invoke(group, i);
            if (sibling == ldac) ldacIndex = i;
            sibling.getClass().getMethod("setOrder", int.class).invoke(sibling, i * 2);
        }
        if (ldacIndex < 0) throw new IllegalStateException("ldac_not_attached");
        Class<?> dropdown = Class.forName(DROPDOWN, false, loader);
        Object created = dropdown.getConstructor(Context.class).newInstance(context);
        Class<?> c = created.getClass();
        c.getMethod("setKey", String.class).invoke(created, KEY);
        c.getMethod("setTitle", CharSequence.class).invoke(created, "蓝牙音频 SSC 编解码器：播放质量");
        c.getMethod("setPersistent", boolean.class).invoke(created, false);
        c.getMethod("setEntryValues", CharSequence[].class)
                .invoke(created, (Object) new CharSequence[] {"0", "1", "2", "3"});
        Class<?> preferenceClass = Class.forName("androidx.preference.Preference", false, loader);
        group.getClass().getMethod("addPreference", preferenceClass).invoke(group, created);
        c.getMethod("setOrder", int.class).invoke(created, ldacIndex * 2 + 1);
        bind(created, loader);
        Log.w(TAG, "stage=settings_quality event=inserted class=" + dropdown.getName());
    }

    private static void bind(Object target, ClassLoader loader) throws ReflectiveOperationException {
        Class<?> listenerClass = Class.forName(
                "androidx.preference.Preference$OnPreferenceChangeListener", false, loader);
        Object listener = Proxy.newProxyInstance(loader, new Class<?>[] {listenerClass},
                (proxy, method, args) -> {
                    switch (method.getName()) {
                        case "onPreferenceChange":
                            if (!ssc) return false;
                            int selected = Integer.parseInt(String.valueOf(args[1]));
                            send(target, new Intent(ACTION_SET).putExtra(EXTRA_MODE, selected));
                            mode = selected;
                            refresh();
                            return true;
                        case "hashCode":
                            return System.identityHashCode(proxy);
                        case "equals":
                            return proxy == args[0];
                        case "toString":
                            return "SscQualityListener";
                        default:
                            return null;
                    }
                });
        target.getClass().getMethod("setOnPreferenceChangeListener", listenerClass)
                .invoke(target, listener);
        preference = target;
        refresh();
        send(target, new Intent(ACTION_QUERY));
    }

    private static void registerReceiver(Context context) {
        if (receiverRegistered) return;
        receiverRegistered = true;
        context.getApplicationContext().registerReceiver(new BroadcastReceiver() {
            @Override public void onReceive(Context ctx, Intent intent) {
                if (!"com.android.bluetooth".equals(getSentFromPackage())) return;
                mode = intent.getIntExtra(EXTRA_MODE, 0);
                refresh();
            }
        }, new IntentFilter(ACTION_STATE), null, new Handler(Looper.getMainLooper()),
                Context.RECEIVER_EXPORTED);
    }

    private static void send(Object target, Intent intent) throws ReflectiveOperationException {
        Context context = (Context) target.getClass().getMethod("getContext").invoke(target);
        BroadcastOptions options = BroadcastOptions.makeBasic();
        options.setShareIdentityEnabled(true);
        context.sendBroadcast(intent.setPackage("com.android.bluetooth"), null, options.toBundle());
    }

    private static CharSequence[] entries() {
        return uhq
                ? new CharSequence[] {"尽可能提供更佳音质（自适应比特率）", "偏重音频质量（584kbps）",
                        "兼顾音频和连接质量（291kbps）", "偏重连接质量（152kbps）"}
                : new CharSequence[] {"尽可能提供更佳音质（自适应比特率）", "偏重音频质量（192kbps）",
                        "兼顾音频和连接质量（128kbps）", "偏重连接质量（88kbps）"};
    }

    private static void refresh() {
        Object target = preference;
        if (target == null) return;
        try {
            Class<?> c = target.getClass();
            CharSequence[] entries = entries();
            c.getMethod("setEntries", CharSequence[].class).invoke(target, (Object) entries);
            c.getMethod("setEnabled", boolean.class).invoke(target, ssc);
            c.getMethod("setValue", String.class).invoke(target, Integer.toString(mode));
            c.getMethod("setSummary", CharSequence.class)
                    .invoke(target, ssc ? entries[mode] : "仅在使用 SSC 时可用");
        } catch (ReflectiveOperationException error) {
            Log.e(TAG, "stage=settings_quality event=refresh_failed error=" + error);
        }
    }
}
