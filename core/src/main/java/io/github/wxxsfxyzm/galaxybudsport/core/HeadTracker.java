package io.github.wxxsfxyzm.galaxybudsport.core;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.SystemClock;

/**
 * Head yaw from earbud samples to the Bluetooth process. A recentering glide is advanced every
 * {@link #TICK_MS} between samples, since earbuds report slowly while the head is still (Galaxy Buds
 * about once a second) and the glide would otherwise move in jumps.
 */
public final class HeadTracker {
    private static final long TICK_MS = 20;

    private final HeadYaw headYaw;
    private final Handler handler;
    private boolean ticking;

    /** Forward axes per role for pitch (see {@link HeadYaw}); null for yaw only. */
    public HeadTracker(double yawSign, double[] forwardRoleFalse, double[] forwardRoleTrue) {
        headYaw = new HeadYaw(yawSign, forwardRoleFalse, forwardRoleTrue);
        HandlerThread thread = new HandlerThread("HeadTracker");
        thread.start();
        handler = new Handler(thread.getLooper());
    }

    /** One orientation sample (sensor to world), its time in elapsedRealtimeNanos. Returns the yaw. */
    public synchronized double sample(double x, double y, double z, double w, boolean role, long nanos) {
        double yaw = headYaw.update(x, y, z, w, role, nanos);
        PoseLink.send((float) yaw, (float) headYaw.pitch());
        if (headYaw.gliding() && !ticking) {
            ticking = true;
            handler.postDelayed(this::tick, TICK_MS);
        }
        return yaw;
    }

    /** See {@link HeadYaw#useBodyAxes}. */
    public synchronized HeadTracker useBodyAxes(boolean role, double[] up, double[] right) {
        headYaw.useBodyAxes(role, up, right);
        return this;
    }

    public synchronized double pitch() {
        return headYaw.pitch();
    }

    /** Back to forward: the stage faces ahead until the next sample. */
    public synchronized void reset() {
        handler.removeCallbacksAndMessages(null);
        ticking = false;
        headYaw.reset();
        PoseLink.send(0f, 0f);
    }

    private synchronized void tick() {
        if (!ticking) return;
        double yaw = headYaw.tick(SystemClock.elapsedRealtimeNanos());
        PoseLink.send((float) yaw, (float) headYaw.pitch());
        if (headYaw.gliding()) {
            handler.postDelayed(this::tick, TICK_MS);
        } else {
            ticking = false;
        }
    }
}
