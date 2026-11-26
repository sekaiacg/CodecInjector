package io.github.wxxsfxyzm.codecinjector.core;

import android.bluetooth.BluetoothCodecConfig;
import android.bluetooth.BluetoothCodecStatus;
import android.bluetooth.BluetoothCodecType;
import android.os.Parcel;
import android.util.Log;
import io.github.libxposed.api.XposedInterface;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Framework-side codec types for the module's vendor codecs. */
final class CodecTypes {
    private static final String TAG = "CodecInjector";

    private CodecTypes() {
    }

    /**
     * BluetoothCodecType.createFromType only knows the built-in codecs; codec configs built from a
     * vendor codec's int type (the stack, Settings' config store) need it to return that codec.
     */
    static void installCreateFromType(XposedInterface xposed, List<CodecInfo> codecs) {
        Map<Integer, BluetoothCodecType> types = new HashMap<>();
        for (CodecInfo codec : codecs) types.put(codec.index, create(codec));
        try {
            Method method = BluetoothCodecType.class.getDeclaredMethod("createFromType", int.class);
            xposed.hook(method).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                    .intercept(chain -> {
                        BluetoothCodecType type = types.get(((Number) chain.getArg(0)).intValue());
                        return type != null ? type : chain.proceed();
                    });
            Log.w(TAG, "stage=codec_type installed=" + types.size());
        } catch (ReflectiveOperationException error) {
            Log.e(TAG, "stage=codec_type error=" + error.getClass().getSimpleName());
        }
    }

    /**
     * HyperOS adds mEncoderFormat to BluetoothCodecConfig; for a vendor codec the audio service needs
     * it to report the AOSP software path (0x7f000000) when the status is sent out of the Bluetooth
     * process.
     */
    static void installStatusEncoderFormat(XposedInterface xposed, List<CodecInfo> codecs) {
        try {
            Field encoderFormat = BluetoothCodecConfig.class.getDeclaredField("mEncoderFormat");
            encoderFormat.setAccessible(true);
            Method writeStatus = BluetoothCodecStatus.class.getDeclaredMethod(
                    "writeToParcel", Parcel.class, int.class);
            xposed.hook(writeStatus).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                    .intercept(chain -> {
                        BluetoothCodecStatus status = (BluetoothCodecStatus) chain.getThisObject();
                        BluetoothCodecConfig config = status.getCodecConfig();
                        if (!ours(config, codecs)) return chain.proceed();
                        synchronized (config) {
                            int saved = encoderFormat.getInt(config);
                            encoderFormat.setInt(config, 0x7f000000);
                            try {
                                return chain.proceed();
                            } finally {
                                encoderFormat.setInt(config, saved);
                            }
                        }
                    });
            Log.w(TAG, "stage=codec_status_type installed=1");
        } catch (ReflectiveOperationException error) {
            Log.e(TAG, "stage=codec_status_type error=" + error.getClass().getSimpleName());
        }
    }

    private static boolean ours(BluetoothCodecConfig config, List<CodecInfo> codecs) {
        for (CodecInfo codec : codecs) if (codec.matches(config)) return true;
        return false;
    }

    private static BluetoothCodecType create(CodecInfo codec) {
        // The constructor is hidden; the Parcelable form is (native type, codec ID, name).
        Parcel parcel = Parcel.obtain();
        try {
            parcel.writeInt(codec.index);
            parcel.writeLong(codec.codecId);
            parcel.writeString(codec.name);
            parcel.setDataPosition(0);
            return BluetoothCodecType.CREATOR.createFromParcel(parcel);
        } finally {
            parcel.recycle();
        }
    }
}
