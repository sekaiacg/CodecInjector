package io.github.wxxsfxyzm.galaxybudsport.core;

import java.util.ArrayDeque;

/**
 * Head yaw from earbud orientation quaternions (sensor to world, world Z along gravity): Galaxy
 * Buds game rotation vectors, AirPods motion reports. The world-frame rotation since the reference
 * pose is q * conj(ref); its twist about world Z is the head heading, independent of how the sensor
 * is mounted, without pitch/roll leaking into it.
 * <p>
 * Recentering follows AOSP's head tracking processor in world-relative mode
 * (frameworks/av/media/libheadtracking: StillnessDetector, PoseBias, PoseRateLimiter, with the
 * parameters of SpatializerPoseController), for the yaw:
 * <ul>
 *   <li>The head is still when every orientation of the last {@link #STILL_WINDOW_NS} is within
 *       {@link #STILL_THRESHOLD} of the latest one (full 3D: nodding counts as movement); after a
 *       movement it cannot be still before a whole window has passed.</li>
 *   <li>When it is still, the current heading becomes forward (the bias), and stillness detection
 *       starts over.</li>
 *   <li>The output then moves to the new target at most {@link #MAX_SPEED}, and follows the head
 *       directly again once it has caught up. While it is moving, {@link #tick} advances it between
 *       samples.</li>
 * </ul>
 * Output: positive yaw = head turned left (AOSP convention).
 * <p>
 * Pitch, when the head's forward axis in the sensor frame is known (per role: the Galaxy Buds'
 * two buds are mounted mirrored): the elevation of that axis
 * against gravity (positive = looking up). The axis is perpendicular to the measured turn axis (the
 * neck), so it is horizontal when the head is level: pitch is absolute, 0 for a level head (facing a
 * phone in the hand measured about -22 to -25 degrees on both AirPods and Galaxy Buds). It is not
 * recentered, so the stage stays level in the room; only a recentering glide rate limits it
 * together with the yaw.
 * <p>
 * A role whose world frame is not gravity aligned (measured: one Galaxy Buds role, whose nod axis is
 * 20 degrees off the horizontal) can use body axes instead ({@link #useBodyAxes}): the rotation since
 * the reference, in the sensor frame, is split into twists about the measured turn (neck) and nod
 * axes, which are fixed to the head. Its pitch is then relative and recentered with the yaw.
 * <p>
 * The sign of the world-Z twist depends on the earbud's frame ({@code yawSign}, set by a listening
 * check). The two Galaxy Buds' world frames are vertically opposite: the same head turn has opposite
 * twists depending on which bud reports (its role flag; the Buds manager also treats a role switch
 * as a reference change). The sign therefore also follows the role, and a role switch restarts the
 * reference.
 */
public final class HeadYaw {
    /** AOSP kAutoRecenterWindowDuration. */
    static final long STILL_WINDOW_NS = 6_000_000_000L;
    /** AOSP kAutoRecenterRotationThreshold. */
    static final double STILL_THRESHOLD = Math.toRadians(10.5);
    /** AOSP kMaxRotationalVelocity (rad/s). */
    static final double MAX_SPEED = 0.8;

    private static final double COS_HALF_THRESHOLD = Math.cos(STILL_THRESHOLD / 2);

    private static final class Sample {
        final long nanos;
        final double[] q;

        Sample(long nanos, double[] q) {
            this.nanos = nanos;
            this.q = q;
        }
    }

    private final double yawSign;
    private final double[][] forward;
    // Per role: {up (turn left positive), right (look up positive)} in the sensor frame, or null.
    private final double[][][] bodyAxes = new double[2][][];
    private double pitchBias;
    private Boolean role;
    private double[] reference;
    // Stillness detector.
    private final ArrayDeque<Sample> window = new ArrayDeque<>();
    private boolean windowFull;
    private long suppressUntil;
    private boolean suppressed;
    // Pose bias: the heading (and elevation) taken as forward.
    private double bias;
    // Rate limiter.
    private double target;
    private double output;
    private double pitchTarget;
    private double pitchOutput;
    private long outputNanos;
    private boolean limiting;
    private boolean started;

    /**
     * Galaxy Buds: -1. AirPods: +1. {@code forwardRoleFalse} / {@code forwardRoleTrue}: head forward
     * in the sensor frame of the bud with that role, or null for yaw only.
     */
    public HeadYaw(double yawSign, double[] forwardRoleFalse, double[] forwardRoleTrue) {
        this.yawSign = yawSign;
        this.forward = new double[][] {unit(forwardRoleFalse), unit(forwardRoleTrue)};
    }

    /** Yaw only. */
    public HeadYaw(double yawSign) {
        this(yawSign, null, null);
    }

    /** Body axes for the role (unit-normalized): turn axis (left positive), nod axis (up positive). */
    public HeadYaw useBodyAxes(boolean role, double[] up, double[] right) {
        bodyAxes[role ? 1 : 0] = new double[][] {unit(up), unit(right)};
        return this;
    }

    private static double[] unit(double[] v) {
        if (v == null) return null;
        double n = Math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        return new double[] {v[0] / n, v[1] / n, v[2] / n};
    }

    public void reset() {
        role = null;
        reference = null;
        resetStillness();
        bias = 0;
        pitchBias = 0;
        target = 0;
        output = 0;
        pitchTarget = 0;
        pitchOutput = 0;
        outputNanos = 0;
        limiting = false;
        started = false;
    }

    /** Returns the head yaw in radians for one sample from the bud with the given role. */
    public double update(double x, double y, double z, double w, boolean role, long nanos) {
        if (this.role == null || this.role != role) {
            reset();
            this.role = role;
        }
        double n = Math.sqrt(x * x + y * y + z * z + w * w);
        if (!(n > 0.5 && n < 1.5)) return output;
        double[] q = {x / n, y / n, z / n, w / n};
        double elevation = elevation(q);
        if (reference == null) {
            reference = q;
            outputNanos = nanos;
            started = true;
        }
        // R = q * conj(ref): rotation since the reference, expressed in the world frame.
        double bx = -reference[0], by = -reference[1], bz = -reference[2], bw = reference[3];
        double rz = q[3] * bz + q[0] * by - q[1] * bx + q[2] * bw;
        double rw = q[3] * bw - q[0] * bx - q[1] * by - q[2] * bz;
        double heading = wrap(2 * Math.atan2(rz, rw));
        double[][] body = bodyAxes[role ? 1 : 0];
        double sign = role ? -yawSign : yawSign;
        if (body != null) {
            // conj(ref) * q: rotation since the reference, in the sensor frame; twists about the
            // head-fixed axes.
            double[] b = multiply(new double[] {-reference[0], -reference[1], -reference[2],
                    reference[3]}, q);
            heading = twist(b, body[0]);
            elevation = twist(b, body[1]);
            sign = 1;
        }

        window.addLast(new Sample(nanos, q));
        if (still(nanos)) {
            // Recenter: this heading is forward now; glide there.
            bias = heading;
            if (body != null) pitchBias = elevation;
            resetStillness();
            limiting = true;
            // The glide starts now, not at the previous sample (which can be a second ago).
            outputNanos = nanos;
        }
        target = sign * wrap(heading - bias);
        pitchTarget = body != null ? wrap(elevation - pitchBias) : elevation;
        return advance(nanos);
    }

    /** Advances a glide in progress to {@code nanos} without a new sample. */
    public double tick(long nanos) {
        return started ? advance(nanos) : output;
    }

    /** Whether the output is still gliding to its target (call {@link #tick} until it is not). */
    public boolean gliding() {
        return limiting;
    }

    public double yaw() {
        return output;
    }

    /** Head pitch in radians (0 without a forward axis). */
    public double pitch() {
        return pitchOutput;
    }

    private double advance(long nanos) {
        double dt = Math.max(0, (nanos - outputNanos) / 1e9);
        outputNanos = Math.max(outputNanos, nanos);
        if (!limiting) {
            output = target;
            pitchOutput = pitchTarget;
            return output;
        }
        // One angular speed limit for the whole rotation, as AOSP's PoseRateLimiter.
        double step = MAX_SPEED * dt;
        double diff = wrap(target - output);
        double pitchDiff = pitchTarget - pitchOutput;
        double distance = Math.sqrt(diff * diff + pitchDiff * pitchDiff);
        if (distance <= step) {
            output = target;
            pitchOutput = pitchTarget;
            limiting = false;
        } else {
            output = wrap(output + diff * step / distance);
            pitchOutput += pitchDiff * step / distance;
        }
        return output;
    }

    private static double[] multiply(double[] a, double[] b) {
        return new double[] {
            a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
            a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
            a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
            a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]};
    }

    /** Twist angle of rotation {@code r} about the unit axis {@code a} (swing-twist split). */
    private static double twist(double[] r, double[] a) {
        double s = r[3] < 0 ? -1 : 1;
        return wrap(2 * Math.atan2(s * (r[0] * a[0] + r[1] * a[1] + r[2] * a[2]), s * r[3]));
    }

    /** Elevation of the forward axis rotated into the world frame (z up). */
    private double elevation(double[] q) {
        double[] forward = this.forward[role != null && role ? 1 : 0];
        if (forward == null) return 0;
        // z component of q * v * conj(q).
        double x = q[0], y = q[1], z = q[2], w = q[3];
        double vz = 2 * (x * z - w * y) * forward[0] + 2 * (y * z + w * x) * forward[1]
                + (1 - 2 * (x * x + y * y)) * forward[2];
        return Math.asin(Math.max(-1, Math.min(1, vz)));
    }

    // ---- AOSP StillnessDetector (default value false)

    private void resetStillness() {
        window.clear();
        windowFull = false;
        suppressed = false;
    }

    private boolean still(long nanos) {
        discardOld(nanos);
        boolean moved = false;
        Sample latest = window.peekLast();
        if (latest != null) {
            // Newest to oldest: the most recent sample away from the latest one sets the deadline.
            java.util.Iterator<Sample> it = window.descendingIterator();
            it.next();
            while (it.hasNext()) {
                Sample sample = it.next();
                if (!near(sample.q, latest.q)) {
                    long deadline = sample.nanos + STILL_WINDOW_NS;
                    if (!suppressed || suppressUntil < deadline) {
                        suppressUntil = deadline;
                        suppressed = true;
                    }
                    moved = true;
                    break;
                }
            }
        }
        if (!windowFull) return false;
        if (suppressed) return false;
        return !moved;
    }

    private void discardOld(long nanos) {
        long start = nanos - STILL_WINDOW_NS;
        while (!window.isEmpty() && window.peekFirst().nanos <= start) {
            windowFull = true;
            window.removeFirst();
        }
        if (suppressed && suppressUntil <= nanos) suppressed = false;
    }

    private static boolean near(double[] a, double[] b) {
        double dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
        return Math.abs(dot) >= COS_HALF_THRESHOLD;
    }

    private static double wrap(double a) {
        while (a > Math.PI) a -= 2 * Math.PI;
        while (a < -Math.PI) a += 2 * Math.PI;
        return a;
    }
}
