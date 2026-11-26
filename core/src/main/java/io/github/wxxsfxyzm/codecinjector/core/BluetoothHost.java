package io.github.wxxsfxyzm.codecinjector.core;

import android.bluetooth.BluetoothCodecConfig;
import android.bluetooth.BluetoothCodecStatus;
import android.bluetooth.BluetoothDevice;
import android.content.Context;
import android.os.Handler;
import android.util.Log;
import io.github.libxposed.api.XposedInterface;
import java.lang.reflect.Method;
import java.util.List;
import java.util.Optional;
import java.util.concurrent.CopyOnWriteArrayList;

/**
 * What a codec feature gets in the Bluetooth process: the application context, a main-thread
 * handler, the A2DP service, and A2DP connection / codec callbacks.
 * <p>
 * The A2DP connection and codec broadcasts are not delivered to receivers registered in the
 * Bluetooth process itself, so A2dpService's own callbacks are hooked once here.
 */
public final class BluetoothHost {
    /** A2DP connection state from the native stack (BluetoothProfile.STATE_*), before it is handled. */
    public interface ConnectionListener {
        void onConnectionState(BluetoothDevice device, int state);
    }

    /** Codec configuration of a device, after the service handled it. */
    public interface CodecListener {
        void onCodecConfig(BluetoothDevice device, BluetoothCodecStatus status);
    }

    private static final String TAG = "CodecInjector";
    public final XposedInterface xposed;
    public final Context context;
    public final Handler handler;
    public final ClassLoader classLoader;
    private final List<ConnectionListener> connectionListeners = new CopyOnWriteArrayList<>();
    private final List<CodecListener> codecListeners = new CopyOnWriteArrayList<>();

    BluetoothHost(XposedInterface xposed, Context context, Handler handler, ClassLoader classLoader) {
        this.xposed = xposed;
        this.context = context;
        this.handler = handler;
        this.classLoader = classLoader;
        installCallbacks();
    }

    public void addConnectionListener(ConnectionListener listener) {
        connectionListeners.add(listener);
    }

    public void addCodecListener(CodecListener listener) {
        codecListeners.add(listener);
    }

    public Object a2dpService() throws ReflectiveOperationException {
        Class<?> adapterClass = Class.forName("com.android.bluetooth.btservice.AdapterService", false,
                classLoader);
        Object adapter = adapterClass.getMethod("deprecatedGetAdapterService").invoke(null);
        Optional<?> a2dp = (Optional<?>) adapterClass.getMethod("getA2dpService").invoke(adapter);
        return a2dp.orElseThrow();
    }

    public BluetoothDevice activeDevice() throws ReflectiveOperationException {
        Object service = a2dpService();
        Method method = service.getClass().getDeclaredMethod("getActiveDevice");
        method.setAccessible(true);
        return (BluetoothDevice) method.invoke(service);
    }

    public BluetoothCodecStatus codecStatus(BluetoothDevice device) throws ReflectiveOperationException {
        Object service = a2dpService();
        Method method = service.getClass().getDeclaredMethod("getCodecStatus", BluetoothDevice.class);
        method.setAccessible(true);
        return (BluetoothCodecStatus) method.invoke(service, device);
    }

    public void setCodecConfigPreference(BluetoothDevice device, BluetoothCodecConfig config)
            throws ReflectiveOperationException {
        Object service = a2dpService();
        Method method = service.getClass().getDeclaredMethod("setCodecConfigPreference",
                BluetoothDevice.class, BluetoothCodecConfig.class);
        method.setAccessible(true);
        method.invoke(service, device, config);
    }

    /** Selects the selectable codec with this stack index for the active device. */
    public void selectCodec(int index) throws ReflectiveOperationException {
        BluetoothDevice device = activeDevice();
        BluetoothCodecStatus status = codecStatus(device);
        BluetoothCodecConfig selected = null;
        for (BluetoothCodecConfig candidate : status.getCodecsSelectableCapabilities()) {
            if (candidate.getCodecType() == index) {
                selected = candidate;
                break;
            }
        }
        if (selected == null) throw new IllegalStateException("codec_not_selectable");
        setCodecConfigPreference(device, new BluetoothCodecConfig.Builder()
                .setExtendedCodecType(selected.getExtendedCodecType())
                .setCodecPriority(BluetoothCodecConfig.CODEC_PRIORITY_HIGHEST)
                .setSampleRate(selected.getSampleRate())
                .setBitsPerSample(selected.getBitsPerSample())
                .setChannelMode(selected.getChannelMode())
                .build());
        Log.w(TAG, "stage=codec_select requested=" + index);
    }

    private void installCallbacks() {
        try {
            Class<?> service = Class.forName("com.android.bluetooth.a2dp.A2dpService", false,
                    classLoader);
            Method connection = service.getDeclaredMethod("onConnectionStateChangedFromNative",
                    BluetoothDevice.class, int.class, int.class);
            Method codec = service.getDeclaredMethod("codecConfigUpdated",
                    BluetoothDevice.class, BluetoothCodecStatus.class, boolean.class);
            xposed.hook(connection).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                    .intercept(chain -> {
                        BluetoothDevice device = (BluetoothDevice) chain.getArg(0);
                        int state = (Integer) chain.getArg(1);
                        if (device != null) {
                            for (ConnectionListener listener : connectionListeners) {
                                listener.onConnectionState(device, state);
                            }
                        }
                        return chain.proceed();
                    });
            xposed.hook(codec).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                    .intercept(chain -> {
                        Object result = chain.proceed();
                        BluetoothDevice device = (BluetoothDevice) chain.getArg(0);
                        BluetoothCodecStatus status = (BluetoothCodecStatus) chain.getArg(1);
                        if (device != null && status != null) {
                            for (CodecListener listener : codecListeners) {
                                listener.onCodecConfig(device, status);
                            }
                        }
                        return result;
                    });
            Log.w(TAG, "stage=a2dp_callbacks installed=1");
        } catch (ReflectiveOperationException error) {
            Log.e(TAG, "stage=a2dp_callbacks error=" + error.getClass().getSimpleName());
        }
    }
}
