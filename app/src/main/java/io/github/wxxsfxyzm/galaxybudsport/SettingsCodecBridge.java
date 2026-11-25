package io.github.wxxsfxyzm.galaxybudsport;

import android.bluetooth.BluetoothCodecConfig;
import android.bluetooth.BluetoothCodecType;
import android.util.Log;
import io.github.libxposed.api.XposedInterface;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.Arrays;

/**
 * Makes Settings recognise SSC (ported from the earlier app/ module, without its OS3 fingerprint
 * and flag gates):
 * - Bluetooth developer options: SSC in the legacy codec list (bluetooth_select_a2dp_codec, the
 *   one HyperOS shows), its current index, and selection through the config store;
 * - Bluetooth device details: the codec label.
 * Codec configs built from the int type 18 need BluetoothCodecType.createFromType(18), which the
 * framework only knows for built-in codecs; SscPortModule.installSscCodecType covers that.
 */
final class SettingsCodecBridge {
    static final String SSC = "SSC";
    static final int SSC_INDEX = 18;
    static final long SSC_CODEC_ID = 0x00000001030075ffL;
    private static final String TAG = "SscPortSettings";
    private static final String CODEC_CONTROLLER =
            "com.android.settings.development.BluetoothAudioCodecPreferenceController";
    private static final String CONTROLLER_BASE =
            "com.android.settings.development.AbstractBluetoothA2dpPreferenceController";
    private static final String DEVICE_PREFERENCE =
            "com.android.settings.bluetooth.BluetoothDevicePreference";

    private SettingsCodecBridge() {
    }

    static boolean isSsc(BluetoothCodecConfig config) {
        BluetoothCodecType type = config == null ? null : config.getExtendedCodecType();
        return type != null && type.getCodecId() == SSC_CODEC_ID;
    }

    static void install(XposedInterface xposed, ClassLoader loader) {
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
        // value back by findIndexOfValue, so SSC is appended there too.
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
                    // Always continue the chain: SettingsQualityBridge hooks this method too, to
                    // learn the current codec.
                    Object result = chain.proceed();
                    if (!isSsc((BluetoothCodecConfig) chain.getArg(0))) return result;
                    String[] values = (String[]) listValuesField.get(chain.getThisObject());
                    int index = values == null ? -1 : Arrays.asList(values).indexOf(SSC);
                    return index >= 0 ? index : result;
                });
        xposed.hook(write).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                .intercept(chain -> {
                    if (!SSC.equals(String.valueOf(chain.getArg(0)))) return chain.proceed();
                    Object store = storeField.get(chain.getThisObject());
                    store.getClass().getMethod("setCodecType", int.class).invoke(store, SSC_INDEX);
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
                .intercept(chain -> isSsc((BluetoothCodecConfig) chain.getArg(0))
                        ? SSC_INDEX : chain.proceed());
        xposed.hook(name).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                .intercept(chain -> ((Number) chain.getArg(0)).intValue() == SSC_INDEX
                        ? SSC : chain.proceed());
        Log.w(TAG, "stage=settings_codec event=label_installed");
    }

    private static String[] append(String[] values) {
        if (values == null || Arrays.asList(values).contains(SSC)) return values;
        String[] out = Arrays.copyOf(values, values.length + 1);
        out[values.length] = SSC;
        return out;
    }

    private static CharSequence[] append(CharSequence[] values) {
        if (values == null) return null;
        for (CharSequence value : values) if (SSC.contentEquals(value)) return values;
        CharSequence[] out = Arrays.copyOf(values, values.length + 1);
        out[values.length] = SSC;
        return out;
    }
}
