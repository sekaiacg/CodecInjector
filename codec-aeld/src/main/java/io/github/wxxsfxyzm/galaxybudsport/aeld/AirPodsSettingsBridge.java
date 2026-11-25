package io.github.wxxsfxyzm.galaxybudsport.aeld;

import android.app.BroadcastOptions;
import android.bluetooth.BluetoothDevice;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.util.Log;
import io.github.libxposed.api.XposedInterface;
import java.io.File;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * HyperOS' AirPods page (plugin.settings.java.airpods.MiuiAirpodsFragment, in a Qigsaw split that
 * Settings loads at runtime):
 * - the system spatial audio switch (greyed out with AAC-ELD, and for the system Spatializer) is
 *   hidden and replaced by a drop-down for our spatial audio: off, fixed, head tracking;
 * - the AAC switch is removed: it rewrites the AAC whitelist, and switching it with AAC-ELD active
 *   leaves the stream silent.
 * The choice is kept per device in Settings and sent to the Bluetooth process, which applies it.
 */
final class AirPodsSettingsBridge {
    static final String ACTION_SPATIAL = "io.github.wxxsfxyzm.galaxybudsport.action.AIRPODS_SPATIAL";
    static final String EXTRA_ADDRESS = "address";
    static final String EXTRA_MODE = "mode";
    static final int MODE_OFF = 0;
    static final int MODE_FIXED = 1;
    static final int MODE_HEAD_TRACKING = 2;
    static final int MODE_DEFAULT = MODE_FIXED;

    private static final String TAG = "SscPortSettings";
    private static final String FRAGMENT = "plugin.settings.java.airpods.MiuiAirpodsFragment";
    private static final String CODEC = "plugin.settings.java.airpods.AirPodsCodec";
    private static final String SPLIT_LOADER =
            "com.iqiyi.android.qigsaw.core.splitload.SplitDexClassLoader";
    private static final String SPLIT_DEX_LOADER =
            "com.iqiyi.android.qigsaw.core.splitload.SplitCompatDexLoader";
    private static final String KEY_SYSTEM_SPATIAL = "SpatialAudio";
    private static final String KEY_GROUP = "switchConfig";
    private static final String KEY_SPATIAL = "galaxybudsport_spatial";
    private static final String PREFS = "galaxybudsport_airpods";
    private static final AtomicBoolean installed = new AtomicBoolean();

    private AirPodsSettingsBridge() {
    }

    static String key(String address) {
        return "spatial_" + address.toUpperCase(Locale.ROOT);
    }

    /**
     * Hooks the AirPods page in whichever class loader holds it: Settings' own, or the Qigsaw split
     * class loader created when the page is first opened.
     */
    static void install(XposedInterface xposed, ClassLoader host) {
        if (tryInstall(xposed, host)) return;
        try {
            Class<?> splitLoader = Class.forName(SPLIT_LOADER, false, host);
            for (Constructor<?> constructor : splitLoader.getDeclaredConstructors()) {
                xposed.hook(constructor).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                        .intercept(chain -> {
                            Object result = chain.proceed();
                            tryInstall(xposed, (ClassLoader) chain.getThisObject());
                            return result;
                        });
            }
        } catch (ReflectiveOperationException error) {
            Log.w(TAG, "stage=airpods_settings split_loader=" + error.getClass().getSimpleName());
        }
        try {
            // Single class loader mode: the split's dex is added to an existing loader.
            Method load = Class.forName(SPLIT_DEX_LOADER, false, host).getDeclaredMethod("load",
                    ClassLoader.class, File.class, List.class);
            xposed.hook(load).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                    .intercept(chain -> {
                        Object result = chain.proceed();
                        tryInstall(xposed, (ClassLoader) chain.getArg(0));
                        return result;
                    });
        } catch (ReflectiveOperationException error) {
            Log.w(TAG, "stage=airpods_settings dex_loader=" + error.getClass().getSimpleName());
        }
        Log.w(TAG, "stage=airpods_settings event=waiting_for_split");
    }

    private static boolean tryInstall(XposedInterface xposed, ClassLoader loader) {
        if (loader == null || installed.get()) return installed.get();
        Class<?> fragment;
        Class<?> codec;
        try {
            fragment = Class.forName(FRAGMENT, false, loader);
            codec = Class.forName(CODEC, false, loader);
        } catch (ClassNotFoundException | LinkageError error) {
            return false;
        }
        if (!installed.compareAndSet(false, true)) return true;
        try {
            Class<?> listener = Class.forName(
                    "androidx.preference.Preference$OnPreferenceChangeListener", false, loader);
            Class<?> group = Class.forName("androidx.preference.PreferenceGroup", false, loader);
            Method addAac = codec.getDeclaredMethod("addPreferencesForSpecialCodec", boolean.class,
                    listener, group);
            xposed.hook(addAac).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                    .intercept(chain -> null);
            Method init = fragment.getDeclaredMethod("initSpatialAudio");
            xposed.hook(init).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                    .intercept(chain -> {
                        showDropDown(chain.getThisObject(), loader);
                        return null;
                    });
            // Refreshes the system switch from the system Spatializer; it stays hidden.
            Method update = fragment.getDeclaredMethod("updateSpatialAudio");
            xposed.hook(update).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                    .intercept(chain -> null);
            Log.w(TAG, "stage=airpods_settings event=installed loader="
                    + loader.getClass().getSimpleName());
        } catch (ReflectiveOperationException error) {
            Log.e(TAG, "stage=airpods_settings error=" + error);
        }
        return true;
    }

    private static void showDropDown(Object fragment, ClassLoader loader) throws Exception {
        Method find = findMethod(fragment.getClass(), "findPreference", CharSequence.class);
        if (find.invoke(fragment, KEY_SPATIAL) != null) return;
        Object system = find.invoke(fragment, KEY_SYSTEM_SPATIAL);
        Object group = system != null ? system.getClass().getMethod("getParent").invoke(system)
                : find.invoke(fragment, KEY_GROUP);
        BluetoothDevice device = (BluetoothDevice) field(fragment, "mDevice");
        if (group == null || device == null) {
            Log.w(TAG, "stage=airpods_settings event=no_anchor group=" + (group != null));
            return;
        }
        Class<?> preference = Class.forName("androidx.preference.Preference", false, loader);
        Context context = (Context) preference.getMethod("getContext").invoke(
                system != null ? system : group);
        String address = device.getAddress();
        SharedPreferences prefs = prefs(context);
        int mode = prefs.getInt(key(address), MODE_DEFAULT);

        Class<?> dropDownClass = Class.forName("miuix.preference.DropDownPreference", false, loader);
        Object dropDown = dropDownClass.getConstructor(Context.class).newInstance(context);
        boolean chinese = Locale.getDefault().getLanguage().equals("zh");
        CharSequence title = system != null
                ? (CharSequence) preference.getMethod("getTitle").invoke(system)
                : chinese ? "空间音频" : "Spatial audio";
        CharSequence[] entries = chinese
                ? new CharSequence[] {"关闭", "固定", "头部跟踪"}
                : new CharSequence[] {"Off", "Fixed", "Head tracking"};
        preference.getMethod("setKey", String.class).invoke(dropDown, KEY_SPATIAL);
        preference.getMethod("setTitle", CharSequence.class).invoke(dropDown, title);
        preference.getMethod("setPersistent", boolean.class).invoke(dropDown, false);
        if (system != null) {
            preference.getMethod("setOrder", int.class).invoke(dropDown,
                    preference.getMethod("getOrder").invoke(system));
            preference.getMethod("setVisible", boolean.class).invoke(system, false);
        }
        dropDownClass.getMethod("setEntries", CharSequence[].class).invoke(dropDown, (Object) entries);
        dropDownClass.getMethod("setEntryValues", CharSequence[].class).invoke(dropDown,
                (Object) new CharSequence[] {"0", "1", "2"});
        dropDownClass.getMethod("setValue", String.class).invoke(dropDown, String.valueOf(mode));

        Class<?> listener = Class.forName("androidx.preference.Preference$OnPreferenceChangeListener",
                false, loader);
        Object onChange = Proxy.newProxyInstance(loader, new Class<?>[] {listener},
                (proxy, method, args) -> {
                    if (!"onPreferenceChange".equals(method.getName())) {
                        return method.getName().equals("hashCode") ? System.identityHashCode(proxy)
                                : method.getName().equals("equals") ? proxy == args[0] : null;
                    }
                    int selected = Integer.parseInt(String.valueOf(args[1]));
                    prefs.edit().putInt(key(address), selected).apply();
                    send(context, address, selected);
                    Log.w(TAG, "stage=airpods_settings event=spatial mode=" + selected);
                    return true;
                });
        preference.getMethod("setOnPreferenceChangeListener", listener).invoke(dropDown, onChange);
        Class.forName("androidx.preference.PreferenceGroup", false, loader)
                .getMethod("addPreference", preference).invoke(group, dropDown);
        // The Bluetooth process may have lost its copy (data cleared): keep it in step.
        send(context, address, mode);
        Log.w(TAG, "stage=airpods_settings event=spatial_shown mode=" + mode);
    }

    private static SharedPreferences prefs(Context context) {
        return context.getApplicationContext().createDeviceProtectedStorageContext()
                .getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }

    private static void send(Context context, String address, int mode) {
        BroadcastOptions options = BroadcastOptions.makeBasic();
        options.setShareIdentityEnabled(true);
        context.sendBroadcast(new Intent(ACTION_SPATIAL).setPackage("com.android.bluetooth")
                .putExtra(EXTRA_ADDRESS, address).putExtra(EXTRA_MODE, mode), null,
                options.toBundle());
    }

    private static Method findMethod(Class<?> type, String name, Class<?>... parameters)
            throws NoSuchMethodException {
        for (Class<?> c = type; c != null; c = c.getSuperclass()) {
            try {
                Method method = c.getDeclaredMethod(name, parameters);
                method.setAccessible(true);
                return method;
            } catch (NoSuchMethodException ignored) {
                // superclass
            }
        }
        throw new NoSuchMethodException(name);
    }

    private static Object field(Object object, String name) throws ReflectiveOperationException {
        for (Class<?> c = object.getClass(); c != null; c = c.getSuperclass()) {
            try {
                Field field = c.getDeclaredField(name);
                field.setAccessible(true);
                return field.get(object);
            } catch (NoSuchFieldException ignored) {
                // superclass
            }
        }
        throw new NoSuchFieldException(name);
    }
}
