package io.github.wxxsfxyzm.galaxybudsport.core;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

public class HeadYawTest {
    private static final double EPS = Math.toRadians(0.05);
    // A real BudGrv sample (x, y, z, w), normalized below.
    private static final double[] REF = normalize(new double[] {0.6308, -0.6791, -0.3751, 0.0});

    private static double[] normalize(double[] q) {
        double n = Math.sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
        return new double[] {q[0] / n, q[1] / n, q[2] / n, q[3] / n};
    }

    private static double[] mul(double[] a, double[] b) {
        return new double[] {
            a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
            a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
            a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
            a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]};
    }

    private static double[] axis(double x, double y, double z, double deg) {
        double h = Math.toRadians(deg) / 2, s = Math.sin(h);
        return new double[] {x * s, y * s, z * s, Math.cos(h)};
    }

    /** Yaw after turning from REF to q, 1 ns later (no drift correction). */
    private static double measure(double[] q, boolean role) {
        HeadYaw yaw = new HeadYaw(-1.0);
        yaw.update(REF[0], REF[1], REF[2], REF[3], role, 0);
        return yaw.update(q[0], q[1], q[2], q[3], role, 1);
    }

    @Test public void worldYawMapsToAospSign() {
        // Twist +30 about world Z = head turned right for role false (listening check), so -30.
        assertEquals(Math.toRadians(-30), measure(mul(axis(0, 0, 1, 30), REF), false), EPS);
        assertEquals(Math.toRadians(75), measure(mul(axis(0, 0, 1, -75), REF), false), EPS);
    }

    @Test public void otherRoleHasOppositeSign() {
        assertEquals(Math.toRadians(30), measure(mul(axis(0, 0, 1, 30), REF), true), EPS);
    }

    @Test public void pitchAndRollDoNotLeak() {
        assertEquals(Math.toRadians(-30),
                measure(mul(axis(1, 0, 0, 40), mul(axis(0, 0, 1, 30), REF)), false), EPS);
        assertEquals(0, measure(mul(axis(0, 1, 0, -25), REF), false), EPS);
    }

    @Test public void roleSwitchRestartsReference() {
        HeadYaw yaw = new HeadYaw(-1.0);
        double[] turned = mul(axis(0, 0, 1, 40), REF);
        yaw.update(REF[0], REF[1], REF[2], REF[3], false, 0);
        assertEquals(Math.toRadians(-40), yaw.update(turned[0], turned[1], turned[2], turned[3], false, 1), EPS);
        // The other bud starts reporting: its first sample becomes the new reference.
        assertEquals(0, yaw.update(turned[0], turned[1], turned[2], turned[3], true, 2), EPS);
        // Back to REF is a world twist of -40 degrees; this role maps it with the opposite sign.
        assertEquals(Math.toRadians(-40), yaw.update(REF[0], REF[1], REF[2], REF[3], true, 3), EPS);
    }

    /** Feeds a world-yaw angle (degrees, twist about world Z from REF) at {@code hz}. */
    private static final class Head {
        final HeadYaw yaw = new HeadYaw(-1.0);
        long t;
        double last;

        Head() {
            yaw.update(REF[0], REF[1], REF[2], REF[3], false, 0);
        }

        double hold(double deg, double seconds) {
            return hold(deg, seconds, 32);
        }

        double hold(double deg, double seconds, int hz) {
            double[] q = mul(axis(0, 0, 1, deg), REF);
            for (int i = 0; i < Math.round(seconds * hz); i++) {
                t += 1_000_000_000L / hz;
                last = yaw.update(q[0], q[1], q[2], q[3], false, t);
            }
            return Math.toDegrees(last);
        }

        /** Advances a glide between samples, as HeadTracker does. */
        double tick(double seconds) {
            t += (long) (seconds * 1e9);
            last = yaw.tick(t);
            return Math.toDegrees(last);
        }
    }

    @Test public void stillnessForTheWindowRecenters() {
        Head head = new Head();
        // World twist +40 is -40 in the output convention for this role.
        assertEquals(-40, head.hold(40, 5.9), 0.5);   // not yet still for a whole window
        double gliding = head.hold(40, 0.3);          // 6.2 s: recentered, gliding at 0.8 rad/s
        assertTrue("gliding: " + gliding, gliding > -40 && gliding < -20);
        assertEquals(0, head.hold(40, 1), 0.5);       // 40 degrees take about 0.9 s
        // The new direction is forward: turning back reads as a turn the other way.
        assertEquals(40, head.hold(0, 0.5), 0.5);
    }

    @Test public void glideIsRateLimited() {
        Head head = new Head();
        double previous = head.hold(90, 6);
        double maxStep = 0;
        for (int i = 0; i < 32 * 3; i++) {
            double now = head.hold(90, 1.0 / 32);
            maxStep = Math.max(maxStep, Math.abs(now - previous));
            assertTrue("monotonic", Math.abs(now) <= Math.abs(previous) + 1e-6);
            previous = now;
        }
        // 0.8 rad/s is 1.43 degrees per 32 Hz sample.
        assertTrue("max step " + maxStep, maxStep < 1.5);
        assertEquals(0, previous, 0.5);
    }

    @Test public void ticksGlideBetweenSlowSamples() {
        // The buds report at about 1 Hz while the head is still: ticks keep the glide smooth.
        Head head = new Head();
        head.hold(25, 1);
        head.hold(25, 5, 1);                      // the 6 s sample recenters
        assertTrue(head.yaw.gliding());
        assertEquals(-25, Math.toDegrees(head.yaw.yaw()), 0.5);  // no jump at the slow sample
        double previous = Math.toDegrees(head.yaw.yaw());
        for (int i = 0; i < 50 && head.yaw.gliding(); i++) {
            double now = head.tick(0.02);
            assertTrue("smooth: " + previous + " -> " + now, Math.abs(now - previous) < 1.0);
            previous = now;
        }
        assertEquals(0, previous, 0.5);
    }

    @Test public void smallOffsetsRecenterToo() {
        Head head = new Head();
        assertEquals(-5, head.hold(5, 5), 0.5);
        assertEquals(0, head.hold(5, 2), 0.5);
    }

    @Test public void movementRestartsTheWindow() {
        Head head = new Head();
        head.hold(60, 5);
        // A glance back and forth: the 6 s window starts again at the last movement.
        head.hold(0, 0.5);
        assertEquals(-60, head.hold(60, 5.4), 0.5);
        assertEquals(0, head.hold(60, 2.5), 0.5);
    }

    @Test public void continuousMovementNeverRecenters() {
        Head head = new Head();
        double deg = 0;
        for (int i = 0; i < 60; i++) {           // a minute of turning back and forth
            deg = (i % 2 == 0) ? 45 : -45;
            head.hold(deg, 1);
        }
        assertEquals(-deg, head.hold(deg, 0.1), 0.5);
    }

    @Test public void noddingCountsAsMovement() {
        Head head = new Head();
        double[] up = mul(axis(0, 1, 0, 30), mul(axis(0, 0, 1, 40), REF));
        double[] level = mul(axis(0, 0, 1, 40), REF);
        for (int i = 0; i < 10; i++) {
            double[] q = (i % 2 == 0) ? up : level;
            for (int k = 0; k < 32; k++) {
                head.t += 1_000_000_000L / 32;
                head.last = head.yaw.update(q[0], q[1], q[2], q[3], false, head.t);
            }
        }
        assertEquals(-40, Math.toDegrees(head.last), 0.5);
    }

    @Test public void headFollowsDuringAGlide() {
        Head head = new Head();
        head.hold(40, 6.1);                       // recentering starts
        // Turning on during the glide: the output still reaches the moving target.
        assertEquals(-20, head.hold(60, 1.5), 0.5);
    }

    // Pitch: a sensor whose forward axis is +x, starting level (identity orientation, world z up).
    private static final double[] FWD = {1, 0, 0};

    @Test public void pitchIsTheElevationOfForward() {
        HeadYaw yaw = new HeadYaw(+1.0, FWD, null);
        double[] level = {0, 0, 0, 1};
        yaw.update(level[0], level[1], level[2], level[3], false, 0);
        // Looking up 20 degrees: forward (+x) rotates toward +z, a rotation of -20 about y.
        double[] up = axis(0, 1, 0, -20);
        yaw.update(up[0], up[1], up[2], up[3], false, 1);
        assertEquals(Math.toRadians(20), yaw.pitch(), EPS);
        // Turning the head does not change the pitch.
        double[] turned = mul(axis(0, 0, 1, 50), up);
        yaw.update(turned[0], turned[1], turned[2], turned[3], false, 2);
        assertEquals(Math.toRadians(20), yaw.pitch(), EPS);
        assertEquals(Math.toRadians(50), yaw.yaw(), EPS);
    }

    @Test public void roleWithoutForwardAxisHasNoPitch() {
        HeadYaw yaw = new HeadYaw(-1.0, null, FWD);
        double[] up = axis(0, 1, 0, -20);
        yaw.update(0, 0, 0, 1, false, 0);
        yaw.update(up[0], up[1], up[2], up[3], false, 1);
        assertEquals(0, yaw.pitch(), 0);
        yaw.update(0, 0, 0, 1, true, 2);
        yaw.update(up[0], up[1], up[2], up[3], true, 3);
        assertEquals(Math.toRadians(20), yaw.pitch(), EPS);
    }

    @Test public void pitchIsAbsoluteAndNotRecentered() {
        HeadYaw yaw = new HeadYaw(+1.0, FWD, null);
        double[] down = axis(0, 1, 0, 30);       // looking down 30 degrees (at the phone)
        yaw.update(0, 0, 0, 1, false, 0);
        long t = 0;
        for (int i = 0; i < 32 * 5; i++) yaw.update(down[0], down[1], down[2], down[3], false, t += 31_250_000L);
        assertEquals(Math.toRadians(-30), yaw.pitch(), EPS);
        for (int i = 0; i < 32 * 3; i++) yaw.update(down[0], down[1], down[2], down[3], false, t += 31_250_000L);
        assertEquals(Math.toRadians(-30), yaw.pitch(), EPS);  // still for 6 s: still 30 below level
    }

    @Test public void bodyAxesSeparateTurnAndNod() {
        // Sensor axes: turn (neck) about +x, nod (look up) about +z; the world frame is tilted.
        HeadYaw yaw = new HeadYaw(-1.0).useBodyAxes(false, new double[] {1, 0, 0},
                new double[] {0, 0, 1});
        double[] tilt = axis(0, 1, 0, 25);       // the tilted world: any constant offset
        yaw.update(tilt[0], tilt[1], tilt[2], tilt[3], false, 0);
        double[] up = mul(tilt, axis(0, 0, 1, 40));  // body-frame nod of 40 degrees
        yaw.update(up[0], up[1], up[2], up[3], false, 1);
        assertEquals(0, yaw.yaw(), EPS);
        assertEquals(Math.toRadians(40), yaw.pitch(), EPS);
        double[] left = mul(tilt, axis(1, 0, 0, 30));
        yaw.update(left[0], left[1], left[2], left[3], false, 2);
        assertEquals(Math.toRadians(30), yaw.yaw(), EPS);
        assertEquals(0, yaw.pitch(), EPS);
    }
}
