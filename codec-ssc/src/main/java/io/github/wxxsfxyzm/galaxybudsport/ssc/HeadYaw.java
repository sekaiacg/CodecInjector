package io.github.wxxsfxyzm.galaxybudsport.ssc;

/**
 * Head yaw from Buds game-rotation-vector quaternions (sensor to world, world Z along gravity).
 * The world-frame rotation since the reference pose is q * conj(ref); its twist about world Z is
 * the head heading, independent of how the sensor is mounted, without pitch/roll leaking into it.
 * (conj(q) * ref was tried and gives a body-frame twist: on device it only reached about +-20
 * degrees for full head turns.)
 * <p>
 * The output is the head yaw relative to the sound stage's "forward" ({@code center}), which moves
 * in two ways:
 * <ul>
 *   <li>Recentering (Apple style): when the head has rested in one direction, at least
 *       {@link #RECENTER_MIN_OFFSET} away from forward, for {@link #RECENTER_HOLD_S}, forward
 *       glides to it: it speeds up and slows down smoothly, over about {@link #RECENTER_TIME_S}.
 *       Looking away briefly does not move the stage. Moving the head out of the resting window
 *       aborts a recentering in progress, and the stage then slows to a stop.</li>
 *   <li>Drift: while the head is near forward, a slow low-pass ({@link #DRIFT_TAU_S}) cancels the
 *       gyro's heading drift (there is no magnetometer). While it is turned away, the stage stays
 *       put.</li>
 * </ul>
 * Output: positive yaw = head turned left (AOSP convention).
 * <p>
 * The two buds' world frames are vertically opposite: the same head turn has opposite twists
 * depending on which bud reports (its role flag; the Buds manager also treats a role switch as a
 * reference change). The sign therefore follows the role, and a role switch restarts the reference.
 */
final class HeadYaw {
    /** Minimum offset between head and stage forward for recentering. */
    static final double RECENTER_MIN_OFFSET = Math.toRadians(15);
    /** The head counts as resting while it stays within this angle of where it settled. */
    static final double REST_WINDOW = Math.toRadians(8);
    /** How long the head must rest before the stage follows it. */
    static final double RECENTER_HOLD_S = 6.0;
    /**
     * The stage turns to the new direction like a critically damped spring: it starts and ends
     * slowly, reaching 95% in about {@link #RECENTER_TIME_S}.
     */
    static final double RECENTER_TIME_S = 4.0;
    private static final double RECENTER_OMEGA = 4.74 / RECENTER_TIME_S;
    /** Upper bound of the stage's turning speed. */
    static final double RECENTER_MAX_SPEED = Math.toRadians(30);
    /** Recentering ends within this angle. */
    static final double RECENTER_DONE = Math.toRadians(1);
    /** Slow heading-drift correction. */
    static final double DRIFT_TAU_S = 120.0;
    // Sign of the world-Z twist relative to the AOSP convention for role == false; set by a
    // listening check. Role == true uses the opposite sign.
    private static final double YAW_SIGN = -1.0;

    private Boolean role;
    private double[] reference;
    private long lastNanos;
    // Stage forward, as a heading in the reference frame.
    private double center;
    // Where the head settled, and since when (seconds of accumulated sample time).
    private double rest;
    private double restSeconds;
    private boolean recentering;
    // Turning speed of the stage forward (rad/s).
    private double centerSpeed;
    private double last;

    void reset() {
        role = null;
        reference = null;
        lastNanos = 0;
        center = 0;
        rest = 0;
        restSeconds = 0;
        recentering = false;
        centerSpeed = 0;
        last = 0;
    }

    /** Returns the head yaw in radians for one sample from the bud with the given role. */
    double update(double x, double y, double z, double w, boolean role, long nanos) {
        if (this.role == null || this.role != role) {
            reset();
            this.role = role;
        }
        double n = Math.sqrt(x * x + y * y + z * z + w * w);
        if (!(n > 0.5 && n < 1.5)) return yaw();
        x /= n; y /= n; z /= n; w /= n;
        if (reference == null) {
            reference = new double[] {x, y, z, w};
            lastNanos = nanos;
            return 0;
        }
        // R = q * conj(ref): rotation since the reference, expressed in the world frame.
        double ax = x, ay = y, az = z, aw = w;
        double bx = -reference[0], by = -reference[1], bz = -reference[2], bw = reference[3];
        double rx = aw * bx + ax * bw + ay * bz - az * by;
        double ry = aw * by - ax * bz + ay * bw + az * bx;
        double rz = aw * bz + ax * by - ay * bx + az * bw;
        double rw = aw * bw - ax * bx - ay * by - az * bz;
        double heading = wrap(2 * Math.atan2(rz, rw));
        double dt = Math.max(0, Math.min(1.0, (nanos - lastNanos) / 1e9));
        lastNanos = nanos;
        moveCenter(heading, dt);
        last = (role ? -YAW_SIGN : YAW_SIGN) * wrap(heading - center);
        return last;
    }

    private void moveCenter(double heading, double dt) {
        // Resting: the head stays within REST_WINDOW of where it settled.
        if (Math.abs(wrap(heading - rest)) > REST_WINDOW) {
            rest = heading;
            restSeconds = 0;
            recentering = false;
        } else {
            restSeconds += dt;
        }
        if (!recentering && restSeconds >= RECENTER_HOLD_S
                && Math.abs(wrap(heading - center)) >= RECENTER_MIN_OFFSET) {
            recentering = true;
        }
        // The buds lower their report rate to about 1 Hz while the head is still; the stage motion
        // is integrated in short steps so it stays stable for any sample interval.
        for (double left = dt; left > 0; left -= MAX_STEP_S) {
            stepCenter(heading, Math.min(left, MAX_STEP_S));
        }
    }

    private static final double MAX_STEP_S = 0.01;

    private void stepCenter(double heading, double dt) {
        double offset = wrap(heading - center);
        if (recentering) {
            // Critically damped spring toward the head heading.
            double accel = RECENTER_OMEGA * RECENTER_OMEGA * offset
                    - 2 * RECENTER_OMEGA * centerSpeed;
            centerSpeed += accel * dt;
            centerSpeed = Math.max(-RECENTER_MAX_SPEED, Math.min(RECENTER_MAX_SPEED, centerSpeed));
            center = wrap(center + centerSpeed * dt);
            if (Math.abs(wrap(heading - center)) < RECENTER_DONE
                    && Math.abs(centerSpeed) < Math.toRadians(2)) {
                recentering = false;
                centerSpeed = 0;
            }
            return;
        }
        if (centerSpeed != 0) {
            // Recentering was aborted: the stage slows down instead of stopping dead.
            centerSpeed *= Math.max(0, 1 - 2 * RECENTER_OMEGA * dt);
            if (Math.abs(centerSpeed) < Math.toRadians(0.5)) centerSpeed = 0;
            center = wrap(center + centerSpeed * dt);
        } else if (Math.abs(offset) < RECENTER_MIN_OFFSET) {
            // Near forward the remaining offset is taken as gyro drift. While the head is turned
            // away the stage stays put until it has rested long enough to recenter.
            center = wrap(center + offset * (dt / DRIFT_TAU_S));
        }
    }

    double yaw() {
        return last;
    }

    private static double wrap(double a) {
        while (a > Math.PI) a -= 2 * Math.PI;
        while (a < -Math.PI) a += 2 * Math.PI;
        return a;
    }
}
