package io.github.wxxsfxyzm.codecinjector.aeld;

import android.util.Log;
import io.github.wxxsfxyzm.codecinjector.core.HeadTracker;
import java.util.Locale;

/** AirPods orientation samples, as the motion service reports them. */
final class AirPodsPose {
    interface Listener {
        /** Orientation quaternion as the AirPods report it, and its arrival time. */
        void onPose(double x, double y, double z, double w, long nanos);

        /** The motion session ended. */
        default void onEnd() {
        }
    }

    /**
     * Head yaw for the spatial stage: the AirPods quaternion is in a gravity-aligned world frame
     * (z up) like the Buds' (checked on device: turns, nods and tilts fall on z, y and x), and a
     * left turn is a positive twist about z, the AOSP sign.
     */
    static final class Tracker implements Listener {
        // Head forward in the AirPods Pro 3 sensor frame, from a capture of turns, nods and tilts:
        // up x right for the turn (-0.65, -0.72, -0.23) and nod (-0.17, -0.19, 0.96) axes, averaged
        // with the tilt axis (-0.91, 0.41, 0.08).
        static final double[] FORWARD = {-0.82, 0.54, 0.04};
        private final HeadTracker tracker = new HeadTracker(+1.0, FORWARD, null);
        private long lastLog;

        @Override public void onPose(double x, double y, double z, double w, long nanos) {
            double yaw = tracker.sample(x, y, z, w, false, nanos);
            if (nanos - lastLog > 1_000_000_000L) {
                lastLog = nanos;
                Log.w(TAG, String.format(Locale.ROOT, "stage=airpods_pose event=yaw deg=%.1f pitch=%.1f",
                        Math.toDegrees(yaw), Math.toDegrees(tracker.pitch())));
            }
        }

        @Override public void onEnd() {
            tracker.reset();
        }
    }

    private static final String TAG = "CodecInjectorAirPods";

    private AirPodsPose() {
    }
}
