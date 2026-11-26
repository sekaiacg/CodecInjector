package io.github.wxxsfxyzm.codecinjector.aeld;

import android.app.ActivityThread;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.SystemClock;
import android.util.Log;
import io.github.libxposed.api.XposedInterface;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.WeakHashMap;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Head tracking over HyperOS' own AACP channel (com.xiaomi.bluetooth's AirCore). AirPods serve one
 * AACP channel, the most recently opened, so a second channel of ours would take the AirPods from
 * HyperOS (and lose them again when HyperOS reconnects). Instead the channel HyperOS opens is
 * tapped: its socket streams are wrapped, the sensor session is run over its output, and the
 * sensor frames (and the replies to the sensor setup) are taken out of what HyperOS reads.
 * <p>
 * The Bluetooth process asks for tracking per device ({@link #ACTION_TRACKING}).
 */
final class AirPodsChannelTap {
    static final String PROCESS = "com.xiaomi.bluetooth";
    static final String ACTION_TRACKING = "io.github.wxxsfxyzm.codecinjector.action.AIRPODS_TRACKING";
    static final String EXTRA_ADDRESS = "address";
    static final String EXTRA_ENABLED = "enabled";

    private static final String TAG = "CodecInjectorAirPods";
    private static final long SETUP_WINDOW_MS = 4000;
    private static final long SETTLE_MS = 3000;
    private static final long STALL_MS = 2500;
    private static final int[] FALLBACK_SERVICES = {15, 16, 14, 6};

    private static final Map<BluetoothSocket, Channel> channels =
            Collections.synchronizedMap(new WeakHashMap<>());
    private static final Set<String> wanted = ConcurrentHashMap.newKeySet();
    private static Handler handler;
    private static AirPodsPose.Listener poses;

    private AirPodsChannelTap() {
    }

    static void install(XposedInterface xposed, AirPodsPose.Listener listener) {
        poses = listener;
        HandlerThread thread = new HandlerThread("AirPodsTap");
        thread.start();
        handler = new Handler(thread.getLooper());
        try {
            Method input = BluetoothSocket.class.getDeclaredMethod("getInputStream");
            xposed.hook(input).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                    .intercept(chain -> {
                        InputStream stream = (InputStream) chain.proceed();
                        Channel channel = channel((BluetoothSocket) chain.getThisObject());
                        return channel == null ? stream : channel.input(stream);
                    });
            Method output = BluetoothSocket.class.getDeclaredMethod("getOutputStream");
            xposed.hook(output).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                    .intercept(chain -> {
                        OutputStream stream = (OutputStream) chain.proceed();
                        Channel channel = channel((BluetoothSocket) chain.getThisObject());
                        if (channel != null) channel.output(stream);
                        return stream;
                    });
            Log.w(TAG, "stage=airpods_tap event=installed");
        } catch (ReflectiveOperationException error) {
            Log.e(TAG, "stage=airpods_tap error=" + error);
        }
        handler.post(new Runnable() {
            int attempts;

            @Override public void run() {
                Context context = ActivityThread.currentApplication();
                if (context == null) {
                    if (++attempts < 120) handler.postDelayed(this, 250);
                    return;
                }
                registerControl(context);
            }
        });
    }

    private static void registerControl(Context context) {
        context.registerReceiver(new BroadcastReceiver() {
            @Override public void onReceive(Context ctx, Intent intent) {
                if (!"com.android.bluetooth".equals(getSentFromPackage())) {
                    Log.w(TAG, "stage=airpods_tap rejected_sender=" + getSentFromPackage());
                    return;
                }
                String address = intent.getStringExtra(EXTRA_ADDRESS);
                if (address == null) return;
                address = address.toUpperCase(Locale.ROOT);
                boolean enabled = intent.getBooleanExtra(EXTRA_ENABLED, false);
                Log.w(TAG, "stage=airpods_tap event=request enabled=" + enabled);
                if (enabled) wanted.add(address); else wanted.remove(address);
                synchronized (channels) {
                    for (Channel channel : channels.values()) channel.update();
                }
            }
        }, new IntentFilter(ACTION_TRACKING), null, handler, Context.RECEIVER_EXPORTED);
        Log.w(TAG, "stage=airpods_tap event=control_registered");
    }

    private static Channel channel(BluetoothSocket socket) {
        // The PSM getter is hidden; a BR/EDR L2CAP socket to a device counts as AACP once AACP
        // traffic is read from it.
        try {
            if (socket.getConnectionType() != BluetoothSocket.TYPE_L2CAP
                    || socket.getRemoteDevice() == null) {
                return null;
            }
        } catch (RuntimeException error) {
            return null;
        }
        synchronized (channels) {
            Channel channel = channels.get(socket);
            if (channel == null) {
                channel = new Channel(socket.getRemoteDevice());
                channels.put(socket, channel);
            }
            return channel;
        }
    }

    /** One HyperOS AACP channel and the sensor session we run over it. */
    private static final class Channel {
        final BluetoothDevice device;
        private volatile OutputStream output;
        private volatile boolean open = true;
        private volatile boolean aacp;
        private volatile long trackedAt;
        private volatile long setupUntil;
        private volatile boolean sensorsUsed;
        private Thread session;
        // Session state, read by the input wrapper.
        private final Set<Integer> named = new LinkedHashSet<>();
        private volatile int owner = -1;
        private volatile int service = -1;
        private volatile int poseCount;
        private volatile long lastPose;
        private int sequence = 1024;

        Channel(BluetoothDevice device) {
            this.device = device;
        }

        void output(OutputStream stream) {
            if (output == null) {
                output = stream;
                handler.post(this::update);
            }
        }

        InputStream input(InputStream stream) {
            return new InputStream() {
                @Override public int read() throws IOException {
                    return stream.read();
                }

                @Override public int read(byte[] b, int off, int len) throws IOException {
                    while (true) {
                        int n;
                        try {
                            n = stream.read(b, off, len);
                        } catch (IOException error) {
                            closed();
                            throw error;
                        }
                        if (n < 0) closed();
                        if (n <= 0 || !consume(b, off, n)) return n;
                    }
                }

                @Override public int available() throws IOException {
                    return stream.available();
                }

                @Override public void close() throws IOException {
                    closed();
                    stream.close();
                }
            };
        }

        private void closed() {
            if (!open) return;
            open = false;
            Log.w(TAG, "stage=airpods_tap event=channel_closed poses=" + poseCount);
            handler.post(this::update);
        }

        /** Starts or stops the sensor session as requested. */
        synchronized void update() {
            if (aacp && SystemClock.elapsedRealtime() - trackedAt < SETTLE_MS) return;
            boolean want = open && aacp && output != null
                    && wanted.contains(device.getAddress().toUpperCase(Locale.ROOT));
            if (want && session == null) {
                session = new Thread(this::run, "AirPodsMotion");
                session.start();
            } else if (!want && session != null) {
                session.interrupt();
                session = null;
            }
        }

        /** Whether a packet HyperOS read belongs to our sensor session (and is hidden from it). */
        private boolean consume(byte[] b, int off, int n) {
            if (!aacp) {
                boolean header = n >= 4 && b[off + 1] == 0 && b[off + 2] == 4 && b[off + 3] == 0
                        && (b[off] == 4 || b[off] == 1);
                if (!header) return false;
                aacp = true;
                trackedAt = SystemClock.elapsedRealtime();
                Log.w(TAG, "stage=airpods_tap event=channel_tracked");
                // Let HyperOS finish its handshake and initial reads before the sensor setup.
                handler.postDelayed(this::update, SETTLE_MS);
            }
            byte[] chunk = new byte[n];
            System.arraycopy(b, off, chunk, 0, n);
            boolean ours = false;
            for (byte[] packet : AacpProtocol.split(chunk, n)) {
                int opcode = AacpProtocol.opcode(packet);
                if (opcode == AacpProtocol.OPCODE_CONTROL) {
                    int[] control = AacpProtocol.controlReport(packet);
                    if (control != null && control[0] == AacpProtocol.CONTROL_OWNER) owner = control[1];
                } else if (opcode == AacpProtocol.OPCODE_SENSOR && sensorsUsed) {
                    ours = true;
                    sensor(packet);
                }
            }
            // Replies to the sensor setup (connection-level packets outside 04 00 04 00).
            if (!ours && SystemClock.elapsedRealtime() < setupUntil && n >= 4
                    && !(b[off] == 4 && b[off + 1] == 0 && b[off + 2] == 4 && b[off + 3] == 0)) {
                ours = true;
                Log.w(TAG, "stage=airpods_tap event=setup_reply " + hex(chunk, n));
            }
            return ours;
        }

        private void sensor(byte[] packet) {
            List<Integer> ids = AacpProtocol.servicesNamed(packet, AacpProtocol.MOTION_SERVICE);
            String services = AacpProtocol.describeServices(packet);
            if (!services.isEmpty()) {
                named.addAll(ids);
                Log.w(TAG, "stage=airpods_tap event=services " + services);
            }
            long[] ack = AacpProtocol.acknowledgement(packet);
            if (ack != null) Log.w(TAG, "stage=airpods_tap event=ack service=" + ack[0] + " error=" + ack[1]);
            double[] q = AacpProtocol.motion(packet, service);
            if (q == null) return;
            ++poseCount;
            lastPose = SystemClock.elapsedRealtime();
            poses.onPose(q[0], q[1], q[2], q[3], SystemClock.elapsedRealtimeNanos());
        }

        private void run() {
            boolean tookOwnership = false;
            int before = owner;
            try {
                sensorsUsed = true;
                setupUntil = SystemClock.elapsedRealtime() + SETUP_WINDOW_MS;
                for (byte[] setup : AacpProtocol.sensorSetup()) {
                    write(setup);
                    Thread.sleep(150);
                }
                Thread.sleep(800);
                write(AacpProtocol.serviceQuery(++sequence));
                Thread.sleep(2000);
                List<Integer> candidates = new ArrayList<>(named);
                if (candidates.isEmpty()) for (int id : FALLBACK_SERVICES) candidates.add(id);
                Log.w(TAG, "stage=airpods_tap event=candidates " + candidates + " owner=" + owner);
                if (!subscribe(candidates)) {
                    // Protected services may need this host to own the control session.
                    tookOwnership = before != 1;
                    write(AacpProtocol.control(AacpProtocol.CONTROL_OWNER, 1));
                    Thread.sleep(1200);
                    Log.w(TAG, "stage=airpods_tap event=ownership before=" + before + " now=" + owner);
                    if (!subscribe(candidates)) {
                        Log.w(TAG, "stage=airpods_tap event=no_motion");
                        return;
                    }
                }
                Log.w(TAG, "stage=airpods_tap event=tracking service=" + service);
                while (!Thread.currentThread().isInterrupted()) {
                    Thread.sleep(1000);
                    if (SystemClock.elapsedRealtime() - lastPose > STALL_MS) {
                        Log.w(TAG, "stage=airpods_tap event=stalled");
                        return;
                    }
                }
            } catch (InterruptedException ignored) {
                // stopped
            } catch (IOException error) {
                Log.w(TAG, "stage=airpods_tap error=" + error);
            } finally {
                try {
                    if (open && service > 0) write(AacpProtocol.subscription(service, 0, ++sequence));
                    if (open && tookOwnership) {
                        write(AacpProtocol.control(AacpProtocol.CONTROL_OWNER, Math.max(0, before)));
                    }
                } catch (IOException ignored) {
                    // channel gone
                }
                Log.w(TAG, "stage=airpods_tap event=session_end poses=" + poseCount);
                poses.onEnd();
                synchronized (this) {
                    if (session == Thread.currentThread()) session = null;
                }
            }
        }

        private boolean subscribe(List<Integer> candidates) throws IOException, InterruptedException {
            for (int candidate : candidates) {
                service = candidate;
                int start = poseCount;
                write(AacpProtocol.subscription(candidate, AacpProtocol.MOTION_INTERVAL_US, ++sequence));
                Thread.sleep(3000);
                if (poseCount > start) return true;
                write(AacpProtocol.subscription(candidate, 0, ++sequence));
            }
            service = -1;
            return false;
        }

        private void write(byte[] packet) throws IOException {
            OutputStream out = output;
            if (out == null || !open) throw new IOException("channel closed");
            // One write is one L2CAP packet; HyperOS' writes interleave whole packets.
            out.write(packet);
            out.flush();
        }
    }

    private static String hex(byte[] p, int count) {
        StringBuilder out = new StringBuilder();
        for (int i = 0; i < Math.min(count, 48); ++i) out.append(String.format(Locale.ROOT, "%02x", p[i]));
        return out.toString();
    }
}
