package io.github.wxxsfxyzm.galaxybudsport.ssc;

import android.app.ActivityThread;
import android.app.BroadcastOptions;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Binder;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Parcel;
import android.os.RemoteException;
import android.util.Log;

/**
 * Head yaw from the Buds manager process to the Bluetooth process. The Bluetooth side owns a
 * Binder sink and hands it out on request; yaw then flows as one-way transactions. Both
 * directions check the peer: broadcasts by sender package, transactions by calling UID.
 */
final class PoseLink {
    static final String ACTION_REQUEST = "io.github.wxxsfxyzm.galaxybudsport.action.REQUEST_POSE_SINK";
    static final String ACTION_SINK = "io.github.wxxsfxyzm.galaxybudsport.action.POSE_SINK";
    static final String BLUETOOTH = "com.android.bluetooth";
    static final String BUDS = "com.samsung.accessory.budsunitemgr";
    private static final String EXTRA_SINK = "sink";
    private static final int TRANSACTION_YAW = IBinder.FIRST_CALL_TRANSACTION;
    private static final String TAG = "SscPortBuds";

    // ---- Buds manager side ----
    private static volatile IBinder sink;
    private static boolean registered;
    private static long lastRequestMs;

    static void send(float yaw) {
        IBinder target = sink;
        if (target == null) {
            request();
            return;
        }
        Parcel data = Parcel.obtain();
        try {
            data.writeFloat(yaw);
            target.transact(TRANSACTION_YAW, data, null, IBinder.FLAG_ONEWAY);
        } catch (RemoteException error) {
            sink = null;
            request();
        } finally {
            data.recycle();
        }
    }

    static synchronized void request() {
        Context context = ActivityThread.currentApplication();
        if (context == null) return;
        if (!registered) {
            registered = true;
            context.registerReceiver(new BroadcastReceiver() {
                @Override public void onReceive(Context ctx, Intent intent) {
                    if (!BLUETOOTH.equals(getSentFromPackage())) return;
                    Bundle extras = intent.getExtras();
                    IBinder binder = extras == null ? null : extras.getBinder(EXTRA_SINK);
                    if (binder == null) return;
                    sink = binder;
                    Log.w(TAG, "stage=pose_link event=sink_received");
                }
            }, new IntentFilter(ACTION_SINK), null, BudsPoseTap.handler(),
                    Context.RECEIVER_EXPORTED);
        }
        long now = System.currentTimeMillis();
        if (now - lastRequestMs < 2000) return;
        lastRequestMs = now;
        context.sendBroadcast(new Intent(ACTION_REQUEST).setPackage(BLUETOOTH), null,
                identity());
    }

    static Bundle identity() {
        BroadcastOptions options = BroadcastOptions.makeBasic();
        options.setShareIdentityEnabled(true);
        return options.toBundle();
    }

    // ---- Bluetooth side ----
    interface YawConsumer {
        void accept(float yaw);
    }

    static void serve(Context context, android.os.Handler handler, YawConsumer consumer) {
        final int budsUid;
        try {
            budsUid = context.getPackageManager().getPackageUid(BUDS, 0);
        } catch (android.content.pm.PackageManager.NameNotFoundException error) {
            return;
        }
        Binder binder = new Binder() {
            @Override protected boolean onTransact(int code, Parcel data, Parcel reply, int flags) {
                if (code != TRANSACTION_YAW || Binder.getCallingUid() != budsUid) return false;
                consumer.accept(data.readFloat());
                return true;
            }
        };
        context.registerReceiver(new BroadcastReceiver() {
            @Override public void onReceive(Context ctx, Intent intent) {
                if (!BUDS.equals(getSentFromPackage())) return;
                Bundle extras = new Bundle();
                extras.putBinder(EXTRA_SINK, binder);
                ctx.sendBroadcast(new Intent(ACTION_SINK).setPackage(BUDS).putExtras(extras),
                        null, identity());
                Log.w("SscPort", "stage=pose_link event=sink_sent");
            }
        }, new IntentFilter(ACTION_REQUEST), null, handler, Context.RECEIVER_EXPORTED);
    }
}
