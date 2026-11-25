package io.github.wxxsfxyzm.galaxybudsport.core;

import android.bluetooth.BluetoothCodecConfig;
import android.util.Log;
import io.github.libxposed.api.XposedInterface;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Makes Settings recognise the module's vendor codecs:
 * - Bluetooth developer options: each codec in the legacy codec list (bluetooth_select_a2dp_codec,
 *   the one HyperOS shows), its current index, and selection through the config store;
 * - Bluetooth device details: the codec label.
 * Codec configs built from a vendor codec's int type need BluetoothCodecType.createFromType, which
 * the framework only knows for built-in codecs; CodecTypes covers that.
 */
final class SettingsCodecBridge {
    private static final String TAG = "SscPortSettings";
    private static final String CODEC_CONTROLLER =
            "com.android.settings.development.BluetoothAudioCodecPreferenceController";
    private static final String CONTROLLER_BASE =
            "com.android.settings.development.AbstractBluetoothA2dpPreferenceController";
    private static final String DEVICE_PREFERENCE =
            "com.android.settings.bluetooth.BluetoothDevicePreference";

    private static List<CodecInfo> codecs = List.of();

    private SettingsCodecBridge() {
    }

    private static CodecInfo forConfig(BluetoothCodecConfig config) {
        for (CodecInfo codec : codecs) if (codec.matches(config)) return codec;
        return null;
    }

    private static CodecInfo forName(Object value) {
        for (CodecInfo codec : codecs) if (codec.name.equals(String.valueOf(value))) return codec;
        return null;
    }

    private static CodecInfo forIndex(int index) {
        for (CodecInfo codec : codecs) if (codec.index == index) return codec;
        return null;
    }

    static void install(XposedInterface xposed, ClassLoader loader, List<CodecInfo> list) {
        codecs = List.copyOf(list);
        try {
            installCodecList(xposed, loader);
        } catch (ReflectiveOperationException error) {
            Log.e(TAG, "stage=settings_codec event=list_failed error=" + error);
        }
        try {
            installDeviceLabel(xposed, loader);
        } catch (ReflectiveOperationException error) {
            Log.e(TAG, "stage=settings_codec event=label_failed error=" + error);
        }
    }

    private static void installCodecList(XposedInterface xposed, ClassLoader loader)
            throws ReflectiveOperationException {
        Class<?> controller = Class.forName(CODEC_CONTROLLER, false, loader);
        Class<?> base = Class.forName(CONTROLLER_BASE, false, loader);
        Class<?> screenClass = Class.forName("androidx.preference.PreferenceScreen", false, loader);
        Method listValues = controller.getDeclaredMethod("getListValues");
        Method listSummaries = controller.getDeclaredMethod("getListSummaries");
        Method currentIndex = controller.getDeclaredMethod(
                "getCurrentA2dpSettingIndex", BluetoothCodecConfig.class);
        Method write = controller.getDeclaredMethod("writeConfigurationValues", Object.class);
        Method display = base.getDeclaredMethod("displayPreference", screenClass);
        Field preferenceField = base.getDeclaredField("mPreference");
        preferenceField.setAccessible(true);
        Field listValuesField = base.getDeclaredField("mListValues");
        listValuesField.setAccessible(true);
        Field storeField = base.getDeclaredField("mBluetoothA2dpConfigStore");
        storeField.setAccessible(true);

        // The controller copies these into mListValues / mListSummaries when it is constructed.
        xposed.hook(listValues).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                .intercept(chain -> append((String[]) chain.proceed()));
        xposed.hook(listSummaries).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                .intercept(chain -> append((String[]) chain.proceed()));
        // The drop-down's entries / entry values come from XML arrays; selection maps the chosen
        // value back by findIndexOfValue, so the codecs are appended there too.
        xposed.hook(display).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                .intercept(chain -> {
                    Object result = chain.proceed();
                    if (!controller.isInstance(chain.getThisObject())) return result;
                    Object preference = preferenceField.get(chain.getThisObject());
                    if (preference == null) return result;
                    Class<?> c = preference.getClass();
                    CharSequence[] entries = (CharSequence[]) c.getMethod("getEntries").invoke(preference);
                    CharSequence[] values = (CharSequence[]) c.getMethod("getEntryValues").invoke(preference);
                    c.getMethod("setEntries", CharSequence[].class).invoke(preference, (Object) append(entries));
                    c.getMethod("setEntryValues", CharSequence[].class)
                            .invoke(preference, (Object) append(values));
                    return result;
                });
        xposed.hook(currentIndex).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                .intercept(chain -> {
                    // Always continue the chain: codec features hook this method too, to learn the
                    // current codec.
                    Object result = chain.proceed();
                    CodecInfo codec = forConfig((BluetoothCodecConfig) chain.getArg(0));
                    if (codec == null) return result;
                    String[] values = (String[]) listValuesField.get(chain.getThisObject());
                    int index = values == null ? -1 : Arrays.asList(values).indexOf(codec.name);
                    return index >= 0 ? index : result;
                });
        xposed.hook(write).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                .intercept(chain -> {
                    CodecInfo codec = forName(chain.getArg(0));
                    if (codec == null) return chain.proceed();
                    Object store = storeField.get(chain.getThisObject());
                    store.getClass().getMethod("setCodecType", int.class).invoke(store, codec.index);
                    store.getClass().getMethod("setCodecPriority", int.class).invoke(store, 1_000_000);
                    return null;
                });
        Log.w(TAG, "stage=settings_codec event=list_installed");
    }

    private static void installDeviceLabel(XposedInterface xposed, ClassLoader loader)
            throws ReflectiveOperationException {
        Class<?> preference = Class.forName(DEVICE_PREFERENCE, false, loader);
        Method recognizable = preference.getDeclaredMethod(
                "getRecognizableCodecType", BluetoothCodecConfig.class);
        Method name = preference.getDeclaredMethod("getCodecName", int.class);
        xposed.hook(recognizable).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                .intercept(chain -> {
                    CodecInfo codec = forConfig((BluetoothCodecConfig) chain.getArg(0));
                    return codec != null ? codec.index : chain.proceed();
                });
        xposed.hook(name).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                .intercept(chain -> {
                    CodecInfo codec = forIndex(((Number) chain.getArg(0)).intValue());
                    return codec != null ? codec.name : chain.proceed();
                });
        Log.w(TAG, "stage=settings_codec event=label_installed");
    }

    private static String[] append(String[] values) {
        if (values == null) return null;
        List<String> out = new ArrayList<>(Arrays.asList(values));
        for (CodecInfo codec : codecs) if (!out.contains(codec.name)) out.add(codec.name);
        return out.toArray(new String[0]);
    }

    private static CharSequence[] append(CharSequence[] values) {
        if (values == null) return null;
        List<CharSequence> out = new ArrayList<>(Arrays.asList(values));
        for (CodecInfo codec : codecs) {
            boolean present = false;
            for (CharSequence value : values) present |= codec.name.contentEquals(value);
            if (!present) out.add(codec.name);
        }
        return out.toArray(new CharSequence[0]);
    }
}
