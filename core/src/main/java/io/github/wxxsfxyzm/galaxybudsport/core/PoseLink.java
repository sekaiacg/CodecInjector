package io.github.wxxsfxyzm.galaxybudsport.core;

import android.app.ActivityThread;
import android.app.BroadcastOptions;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.os.Binder;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.Parcel;
import android.os.RemoteException;
import android.util.Log;
import java.util.HashSet;
import java.util.Set;

/**
 * Head yaw from the process that reads the earbuds (the Galaxy Buds manager, HyperOS' AirPods
 * service) to the Bluetooth process. The Bluetooth side owns a Binder sink and hands it out on
 * request; yaw then flows as one-way transactions. Both directions check the peer: broadcasts by
 * sender package, transactions by calling UID.
 */
public final class PoseLink {
    static final String ACTION_REQUEST = "io.github.wxxsfxyzm.galaxybudsport.action.REQUEST_POSE_SINK";
    static final String ACTION_SINK = "io.github.wxxsfxyzm.galaxybudsport.action.POSE_SINK";
    static final String BLUETOOTH = "com.android.bluetooth";
    private static final String EXTRA_SINK = "sink";
    private static final int TRANSACTION_YAW = IBinder.FIRST_CALL_TRANSACTION;
    private static final String TAG = "SscPort";

    private PoseLink() {
    }

    // ---- earbud process side ----
    private static volatile IBinder sink;
    private static boolean registered;
    private static long lastRequestMs;
    private static Handler handler;

    /**
     * Sends the head yaw and pitch in radians (yaw positive = turned left, pitch positive = looking
     * up); asks for the sink first when there is none.
     */
    public static void send(float yaw, float pitch) {
        IBinder target = sink;
        if (target == null) {
            request();
            return;
        }
        Parcel data = Parcel.obtain();
        try {
            data.writeFloat(yaw);
            data.writeFloat(pitch);
            target.transact(TRANSACTION_YAW, data, null, IBinder.FLAG_ONEWAY);
        } catch (RemoteException error) {
            sink = null;
            request();
        } finally {
            data.recycle();
        }
    }

    public static synchronized void request() {
        Context context = ActivityThread.currentApplication();
        if (context == null) return;
        if (!registered) {
            registered = true;
            HandlerThread thread = new HandlerThread("PoseLink");
            thread.start();
            handler = new Handler(thread.getLooper());
            context.registerReceiver(new BroadcastReceiver() {
                @Override public void onReceive(Context ctx, Intent intent) {
                    if (!BLUETOOTH.equals(getSentFromPackage())) return;
                    Bundle extras = intent.getExtras();
                    IBinder binder = extras == null ? null : extras.getBinder(EXTRA_SINK);
                    if (binder == null) return;
                    sink = binder;
                    Log.w(TAG, "stage=pose_link event=sink_received");
                }
            }, new IntentFilter(ACTION_SINK), null, handler, Context.RECEIVER_EXPORTED);
        }
        long now = System.currentTimeMillis();
        if (now - lastRequestMs < 2000) return;
        lastRequestMs = now;
        context.sendBroadcast(new Intent(ACTION_REQUEST).setPackage(BLUETOOTH), null, identity());
    }

    static Bundle identity() {
        BroadcastOptions options = BroadcastOptions.makeBasic();
        options.setShareIdentityEnabled(true);
        return options.toBundle();
    }

    // ---- Bluetooth side ----
    public interface PoseConsumer {
        void accept(float yaw, float pitch);
    }

    /** Serves the sink to the given packages. */
    static void serve(Context context, Handler handler, PoseConsumer consumer, Set<String> senders) {
        Set<Integer> uids = new HashSet<>();
        for (String sender : senders) {
            try {
                uids.add(context.getPackageManager().getPackageUid(sender, 0));
            } catch (PackageManager.NameNotFoundException ignored) {
                // not installed
            }
        }
        if (uids.isEmpty()) return;
        Binder binder = new Binder() {
            @Override protected boolean onTransact(int code, Parcel data, Parcel reply, int flags) {
                if (code != TRANSACTION_YAW || !uids.contains(Binder.getCallingUid())) return false;
                float yaw = data.readFloat();
                float pitch = data.dataAvail() >= 4 ? data.readFloat() : 0f;
                consumer.accept(yaw, pitch);
                return true;
            }
        };
        context.registerReceiver(new BroadcastReceiver() {
            @Override public void onReceive(Context ctx, Intent intent) {
                String sender = getSentFromPackage();
                if (sender == null || !senders.contains(sender)) return;
                Bundle extras = new Bundle();
                extras.putBinder(EXTRA_SINK, binder);
                ctx.sendBroadcast(new Intent(ACTION_SINK).setPackage(sender).putExtras(extras),
                        null, identity());
                Log.w(TAG, "stage=pose_link event=sink_sent to=" + sender);
            }
        }, new IntentFilter(ACTION_REQUEST), null, handler, Context.RECEIVER_EXPORTED);
    }
}
