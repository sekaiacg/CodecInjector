package io.github.wxxsfxyzm.codecinjector.ssc;

import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.os.Handler;
import android.os.HandlerThread;
import io.github.libxposed.api.XposedInterface;
import io.github.wxxsfxyzm.codecinjector.core.HeadTracker;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.reflect.Method;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.UUID;
import java.util.WeakHashMap;

/**
 * Runs inside the Galaxy Buds manager. Shares the manager's own SPP session instead of opening a
 * second socket: received bytes are passed through unchanged and decoded on the side, and our
 * spatial-sensor control frames are written through the same (serialized) output stream.
 */
final class BudsPoseTap {
    static final UUID BUDS_SPP = UUID.fromString("2e73a4ad-332d-41fc-90e2-16bef06523f2");
    private static final String TAG = "CodecInjectorBuds";
    private static final long ATTACH_DELAY_MS = 3000;
    private static final long KEEP_ALIVE_MS = 2000;

    private static final Map<BluetoothSocket, Session> SESSIONS =
            Collections.synchronizedMap(new WeakHashMap<>());
    private static Handler handler;
    private static volatile boolean headTracking;
    // Managers whose switches are read from the protocol (BudsSettingsBridge): the manager's own
    // SET_SPATIAL_AUDIO frames carry them, so the tap does not send its own.
    private static volatile boolean switchesFromProtocol;
    private static volatile BluetoothDevice device;

    /** Reads the 360 audio / head tracking switches from the manager's SET_SPATIAL_AUDIO frames. */
    static void readSwitchesFromProtocol() {
        switchesFromProtocol = true;
    }

    /** The buds of the most recently tracked SPP session, or null. */
    static BluetoothDevice device() {
        return device;
    }

    /** Head tracking switch from the manager's settings page (see BudsSettingsBridge). */
    static void setHeadTracking(boolean enabled) {
        headTracking = enabled;
        Session[] sessions;
        synchronized (SESSIONS) {
            sessions = SESSIONS.values().toArray(new Session[0]);
        }
        for (Session session : sessions) handler().post(session::update);
        BudsLog.w("stage=buds_tap event=head_tracking enabled=" + enabled);
    }

    private BudsPoseTap() {
    }

    /** Tracks the manager's SPP socket to the buds and taps its streams. */
    static void install(XposedInterface xposed) {
        BudsLog.attach(xposed);
        try {
            for (String name : new String[] {"createInsecureRfcommSocketToServiceRecord",
                    "createRfcommSocketToServiceRecord"}) {
                Method create = BluetoothDevice.class.getDeclaredMethod(name, UUID.class);
                xposed.hook(create).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                        .intercept(chain -> {
                            Object socket = chain.proceed();
                            if (socket instanceof BluetoothSocket && BUDS_SPP.equals(chain.getArg(0))) {
                                track((BluetoothSocket) socket);
                            }
                            return socket;
                        });
            }
            Method input = BluetoothSocket.class.getDeclaredMethod("getInputStream");
            xposed.hook(input).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                    .intercept(chain -> wrapInput((BluetoothSocket) chain.getThisObject(),
                            (InputStream) chain.proceed()));
            Method output = BluetoothSocket.class.getDeclaredMethod("getOutputStream");
            xposed.hook(output).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                    .intercept(chain -> wrapOutput((BluetoothSocket) chain.getThisObject(),
                            (OutputStream) chain.proceed()));
            BudsLog.w("stage=buds_tap event=installed");
        } catch (ReflectiveOperationException error) {
            BudsLog.e("stage=buds_tap error=" + error.getClass().getSimpleName());
        }
    }

    static synchronized Handler handler() {
        if (handler == null) {
            HandlerThread thread = new HandlerThread("CodecInjectorBudsTap");
            thread.start();
            handler = new Handler(thread.getLooper());
        }
        return handler;
    }

    static void track(BluetoothSocket socket) {
        SESSIONS.put(socket, new Session());
        try {
            if (socket.getRemoteDevice() != null) device = socket.getRemoteDevice();
        } catch (RuntimeException ignored) {
            // no device
        }
        BudsLog.w("stage=buds_tap event=socket_tracked");
    }

    static InputStream wrapInput(BluetoothSocket socket, InputStream stream) {
        Session session = SESSIONS.get(socket);
        if (session == null || stream == null) return stream;
        return session.input(stream);
    }

    static OutputStream wrapOutput(BluetoothSocket socket, OutputStream stream) {
        Session session = SESSIONS.get(socket);
        if (session == null || stream == null) return stream;
        return session.output(stream);
    }

    /** SET_SPATIAL_AUDIO payload: [360 audio, head tracking], the second byte optional. */
    static boolean[] spatialSwitches(BudsSpatialProtocol.Frame frame) {
        if (frame == null || frame.messageId != BudsSpatialProtocol.SET_SPATIAL_AUDIO
                || frame.payload.length < 1) {
            return null;
        }
        boolean spatial = frame.payload[0] == 1;
        if (frame.payload.length < 2) return new boolean[] {spatial};
        return new boolean[] {spatial, frame.payload[1] == 1};
    }

    static final class Session {
        private BudsSpatialProtocol.StreamDecoder decoder = new BudsSpatialProtocol.StreamDecoder();
        private BudsSpatialProtocol.StreamDecoder sentDecoder = new BudsSpatialProtocol.StreamDecoder();
        private TappedOutput output;
        private boolean attachScheduled;
        private boolean attached;
        // Role == true: world frame gravity aligned; head forward in the sensor frame from a capture
        // of turns, nods and tilts (absolute pitch).
        static final double[] FORWARD_ROLE_TRUE = {0.30, -0.88, -0.37};
        // Role == false: its world frame is tilted (nods are 20 degrees off the horizontal and leak
        // into the world-Z twist), so it uses the measured head axes in its sensor frame: turn left
        // (-0.93, -0.35, 0) and look up (0.03, -0.06, 1.0).
        static final double[] UP_ROLE_FALSE = {-0.93, -0.35, 0.0};
        static final double[] RIGHT_ROLE_FALSE = {0.03, -0.06, 1.0};
        private final HeadTracker tracker = new HeadTracker(-1.0, null, FORWARD_ROLE_TRUE)
                .useBodyAxes(false, UP_ROLE_FALSE, RIGHT_ROLE_FALSE);
        private long lastYawLogMs;
        private long lastYawModuleLogMs;
        private long samples;
        private long lastLogMs;
        private long windowStartMs;
        private long windowSamples;

        synchronized InputStream input(InputStream stream) {
            return new TappedInput(stream, this);
        }

        synchronized OutputStream output(OutputStream stream) {
            if (output == null || output.delegate != stream) output = new TappedOutput(stream, this);
            if (!attachScheduled) {
                attachScheduled = true;
                handler().postDelayed(this::update, ATTACH_DELAY_MS);
            }
            return output;
        }

        /** Runs on the tap handler: brings the sensor stream in line with the switch. */
        private void update() {
            synchronized (this) {
                if (output == null) return;
            }
            // A restarted manager does not report the switches again; use the stored ones.
            BudsSettingsBridge.restore();
            if (headTracking && !attached) {
                if ((!switchesFromProtocol && !send(BudsSpatialProtocol.encodeRequest(
                                BudsSpatialProtocol.SET_SPATIAL_AUDIO, 1)))
                        || !send(BudsSpatialProtocol.encodeRequest(
                                BudsSpatialProtocol.SPATIAL_AUDIO_CONTROL,
                                BudsSpatialProtocol.CONTROL_ATTACH))) {
                    return;
                }
                attached = true;
                tracker.reset();
                BudsLog.w("stage=buds_tap event=attach_sent");
                handler().postDelayed(this::keepAlive, KEEP_ALIVE_MS);
            } else if (!headTracking && attached) {
                attached = false;
                send(BudsSpatialProtocol.encodeRequest(BudsSpatialProtocol.SPATIAL_AUDIO_CONTROL,
                        BudsSpatialProtocol.CONTROL_DETACH));
                if (!switchesFromProtocol) {
                    send(BudsSpatialProtocol.encodeRequest(BudsSpatialProtocol.SET_SPATIAL_AUDIO, 0));
                }
                tracker.reset();
                BudsLog.w("stage=buds_tap event=detach_sent");
            }
        }

        private void keepAlive() {
            if (!attached) return;
            if (!send(BudsSpatialProtocol.encodeRequest(BudsSpatialProtocol.SPATIAL_AUDIO_CONTROL,
                    BudsSpatialProtocol.CONTROL_KEEP_ALIVE))) {
                attached = false;
                return;
            }
            handler().postDelayed(this::keepAlive, KEEP_ALIVE_MS);
        }

        private boolean send(byte[] frame) {
            TappedOutput out;
            synchronized (this) {
                out = output;
            }
            if (out == null) return false;
            try {
                out.writeOwn(frame);
                return true;
            } catch (IOException error) {
                BudsLog.w("stage=buds_tap event=send_failed error=" + error.getClass().getSimpleName());
                return false;
            }
        }

        /** Frames the manager writes to the buds. */
        void sent(byte[] data, int offset, int length) {
            if (!switchesFromProtocol) return;
            List<BudsSpatialProtocol.Frame> frames;
            try {
                frames = sentDecoder.append(data, offset, length);
            } catch (RuntimeException error) {
                sentDecoder = new BudsSpatialProtocol.StreamDecoder();
                return;
            }
            for (BudsSpatialProtocol.Frame frame : frames) {
                boolean[] switches = spatialSwitches(frame);
                if (switches != null) BudsSettingsBridge.fromProtocol(switches);
            }
        }

        void received(byte[] data, int offset, int length) {
            List<BudsSpatialProtocol.Frame> frames;
            try {
                frames = decoder.append(data, offset, length);
            } catch (RuntimeException error) {
                // Never let a decoding problem reach the manager's read path; start over.
                decoder = new BudsSpatialProtocol.StreamDecoder();
                return;
            }
            for (BudsSpatialProtocol.Frame frame : frames) {
                if (BudsSpatialProtocol.isAttachSuccess(frame)) {
                    BudsLog.w("stage=buds_tap event=attach_success");
                    continue;
                }
                BudsSpatialProtocol.QuaternionSample q = BudsSpatialProtocol.decodeQuaternion(frame);
                if (q != null) pose(q);
            }
        }

        private void pose(BudsSpatialProtocol.QuaternionSample q) {
            if (!headTracking) return;
            double yaw = tracker.sample(q.x, q.y, q.z, q.w, q.auxiliaryFlag,
                    android.os.SystemClock.elapsedRealtimeNanos());
            long nowMs = System.currentTimeMillis();
            if (nowMs - lastYawLogMs >= 500) {
                String line = String.format(java.util.Locale.ROOT,
                        "stage=buds_tap event=yaw deg=%.1f pitch=%.1f role=%b", Math.toDegrees(yaw),
                        Math.toDegrees(tracker.pitch()), q.auxiliaryFlag);
                // Every 5 s into the LSPosed log, every 0.5 s into logcat.
                if (nowMs - lastYawModuleLogMs >= 5000) {
                    lastYawModuleLogMs = nowMs;
                    BudsLog.w(line);
                } else {
                    BudsLog.verbose(line);
                }
                lastYawLogMs = nowMs;
            }
            samples++;
            windowSamples++;
            long now = System.currentTimeMillis();
            if (windowStartMs == 0) windowStartMs = now;
            if (now - lastLogMs < 1000) return;
            double rate = windowSamples * 1000.0 / Math.max(1, now - windowStartMs);
            windowStartMs = now;
            windowSamples = 0;
            lastLogMs = now;
            // Euler candidates (degrees) for each axis; which one tracks head yaw is determined on
            // the device by turning the head.
            double rx = Math.toDegrees(Math.atan2(2 * (q.w * q.x + q.y * q.z),
                    1 - 2 * (q.x * q.x + q.y * q.y)));
            double ry = Math.toDegrees(Math.asin(Math.max(-1, Math.min(1,
                    2 * (q.w * q.y - q.z * q.x)))));
            double rz = Math.toDegrees(Math.atan2(2 * (q.w * q.z + q.x * q.y),
                    1 - 2 * (q.y * q.y + q.z * q.z)));
            BudsLog.verbose(String.format(java.util.Locale.ROOT,
                    "stage=buds_tap event=pose n=%d rate=%.1f q=%.4f,%.4f,%.4f,%.4f flag=%b "
                            + "euler_x=%.1f euler_y=%.1f euler_z=%.1f",
                    samples, rate, q.x, q.y, q.z, q.w, q.auxiliaryFlag, rx, ry, rz));
        }
    }

    private static final class TappedInput extends InputStream {
        private final InputStream delegate;
        private final Session session;

        TappedInput(InputStream delegate, Session session) {
            this.delegate = delegate;
            this.session = session;
        }

        @Override public int read() throws IOException {
            int value = delegate.read();
            if (value >= 0) session.received(new byte[] {(byte) value}, 0, 1);
            return value;
        }

        @Override public int read(byte[] buffer, int offset, int length) throws IOException {
            int count = delegate.read(buffer, offset, length);
            if (count > 0) session.received(buffer, offset, count);
            return count;
        }

        @Override public int available() throws IOException {
            return delegate.available();
        }

        @Override public void close() throws IOException {
            delegate.close();
        }
    }

    private static final class TappedOutput extends OutputStream {
        final OutputStream delegate;
        private final Session session;

        TappedOutput(OutputStream delegate, Session session) {
            this.delegate = delegate;
            this.session = session;
        }

        /** Writes one of our own frames, without reading it as the manager's. */
        synchronized void writeOwn(byte[] frame) throws IOException {
            delegate.write(frame, 0, frame.length);
            delegate.flush();
        }

        @Override public synchronized void write(int value) throws IOException {
            delegate.write(value);
            session.sent(new byte[] {(byte) value}, 0, 1);
        }

        @Override public synchronized void write(byte[] buffer, int offset, int length)
                throws IOException {
            delegate.write(buffer, offset, length);
            session.sent(buffer, offset, length);
        }

        @Override public synchronized void flush() throws IOException {
            delegate.flush();
        }

        @Override public void close() throws IOException {
            delegate.close();
        }
    }
}
