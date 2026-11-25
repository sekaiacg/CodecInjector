package io.github.wxxsfxyzm.galaxybudsport;

import android.bluetooth.BluetoothSocket;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
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
    private static final String TAG = "SscPortBuds";
    private static final long ATTACH_DELAY_MS = 3000;
    private static final long KEEP_ALIVE_MS = 2000;

    private static final Map<BluetoothSocket, Session> SESSIONS =
            Collections.synchronizedMap(new WeakHashMap<>());
    private static Handler handler;
    private static volatile boolean headTracking;

    /** Head tracking switch from the manager's settings page (see BudsSettingsBridge). */
    static void setHeadTracking(boolean enabled) {
        headTracking = enabled;
        Session[] sessions;
        synchronized (SESSIONS) {
            sessions = SESSIONS.values().toArray(new Session[0]);
        }
        for (Session session : sessions) handler().post(session::update);
        Log.w(TAG, "stage=buds_tap event=head_tracking enabled=" + enabled);
    }

    private BudsPoseTap() {
    }

    static synchronized Handler handler() {
        if (handler == null) {
            HandlerThread thread = new HandlerThread("SscPortBudsTap");
            thread.start();
            handler = new Handler(thread.getLooper());
        }
        return handler;
    }

    static void track(BluetoothSocket socket) {
        SESSIONS.put(socket, new Session());
        Log.w(TAG, "stage=buds_tap event=socket_tracked");
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

    static final class Session {
        private BudsSpatialProtocol.StreamDecoder decoder = new BudsSpatialProtocol.StreamDecoder();
        private TappedOutput output;
        private boolean attachScheduled;
        private boolean attached;
        private final HeadYaw headYaw = new HeadYaw();
        private long lastYawLogMs;
        private long samples;
        private long lastLogMs;
        private long windowStartMs;
        private long windowSamples;

        synchronized InputStream input(InputStream stream) {
            return new TappedInput(stream, this);
        }

        synchronized OutputStream output(OutputStream stream) {
            if (output == null || output.delegate != stream) output = new TappedOutput(stream);
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
                if (!send(BudsSpatialProtocol.encodeRequest(BudsSpatialProtocol.SET_SPATIAL_AUDIO, 1))
                        || !send(BudsSpatialProtocol.encodeRequest(
                                BudsSpatialProtocol.SPATIAL_AUDIO_CONTROL,
                                BudsSpatialProtocol.CONTROL_ATTACH))) {
                    return;
                }
                attached = true;
                headYaw.reset();
                PoseLink.request();
                Log.w(TAG, "stage=buds_tap event=attach_sent");
                handler().postDelayed(this::keepAlive, KEEP_ALIVE_MS);
            } else if (!headTracking && attached) {
                attached = false;
                send(BudsSpatialProtocol.encodeRequest(BudsSpatialProtocol.SPATIAL_AUDIO_CONTROL,
                        BudsSpatialProtocol.CONTROL_DETACH));
                send(BudsSpatialProtocol.encodeRequest(BudsSpatialProtocol.SET_SPATIAL_AUDIO, 0));
                PoseLink.send(0f);
                Log.w(TAG, "stage=buds_tap event=detach_sent");
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
                out.write(frame);
                out.flush();
                return true;
            } catch (IOException error) {
                Log.w(TAG, "stage=buds_tap event=send_failed error=" + error.getClass().getSimpleName());
                return false;
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
                    Log.w(TAG, "stage=buds_tap event=attach_success");
                    continue;
                }
                BudsSpatialProtocol.QuaternionSample q = BudsSpatialProtocol.decodeQuaternion(frame);
                if (q != null) pose(q);
            }
        }

        private void pose(BudsSpatialProtocol.QuaternionSample q) {
            if (!headTracking) return;
            double yaw = headYaw.update(q.x, q.y, q.z, q.w, q.auxiliaryFlag, System.nanoTime());
            PoseLink.send((float) yaw);
            long nowMs = System.currentTimeMillis();
            if (nowMs - lastYawLogMs >= 500) {
                lastYawLogMs = nowMs;
                Log.w(TAG, String.format(java.util.Locale.ROOT,
                        "stage=buds_tap event=yaw deg=%.1f role=%b", Math.toDegrees(yaw),
                        q.auxiliaryFlag));
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
            Log.w(TAG, String.format(java.util.Locale.ROOT,
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

        TappedOutput(OutputStream delegate) {
            this.delegate = delegate;
        }

        @Override public synchronized void write(int value) throws IOException {
            delegate.write(value);
        }

        @Override public synchronized void write(byte[] buffer, int offset, int length)
                throws IOException {
            delegate.write(buffer, offset, length);
        }

        @Override public synchronized void flush() throws IOException {
            delegate.flush();
        }

        @Override public void close() throws IOException {
            delegate.close();
        }
    }
}
