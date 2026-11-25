package io.github.wxxsfxyzm.galaxybudsport;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
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
        HeadYaw yaw = new HeadYaw();
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
        HeadYaw yaw = new HeadYaw();
        double[] turned = mul(axis(0, 0, 1, 40), REF);
        yaw.update(REF[0], REF[1], REF[2], REF[3], false, 0);
        assertEquals(Math.toRadians(-40), yaw.update(turned[0], turned[1], turned[2], turned[3], false, 1), EPS);
        // The other bud starts reporting: its first sample becomes the new reference.
        assertEquals(0, yaw.update(turned[0], turned[1], turned[2], turned[3], true, 2), 0);
        // Back to REF is a world twist of -40 degrees; this role maps it with the opposite sign.
        assertEquals(Math.toRadians(-40), yaw.update(REF[0], REF[1], REF[2], REF[3], true, 3), EPS);
    }

    /** Feeds a world-yaw angle (degrees, twist about world Z from REF) at 32 Hz. */
    private static final class Head {
        final HeadYaw yaw = new HeadYaw();
        long t;
        double last;

        Head() {
            yaw.update(REF[0], REF[1], REF[2], REF[3], false, 0);
        }

        /** Holds the head at {@code deg} for {@code seconds}; returns the output in degrees. */
        double hold(double deg, double seconds) {
            return hold(deg, seconds, 32);
        }

        /** As {@link #hold(double, double)}, with the buds reporting at {@code hz}. */
        double hold(double deg, double seconds, int hz) {
            double[] q = mul(axis(0, 0, 1, deg), REF);
            for (int i = 0; i < Math.round(seconds * hz); i++) {
                t += 1_000_000_000L / hz;
                last = yaw.update(q[0], q[1], q[2], q[3], false, t);
            }
            return Math.toDegrees(last);
        }
    }

    @Test public void restingInANewDirectionRecentersTheStage() {
        Head head = new Head();
        // World twist +40 is -40 in the output convention for this role.
        assertEquals(-40, head.hold(40, 4), 1.5);      // still turned: stage stays put
        assertEquals(-40, head.hold(40, 1.9), 1.5);    // 5.9 s: not yet
        assertEquals(-40, head.hold(40, 0.5), 5);      // just started: it starts slowly
        double midway = head.hold(40, 1.5);            // 8 s: gliding
        assertTrue("gliding: " + midway, midway > -35 && midway < -5);
        double after = head.hold(40, 4);               // 12 s
        assertTrue("recentred: " + after, Math.abs(after) < 2);
        // The new direction is forward now: turning back reads as a turn the other way.
        assertEquals(40, head.hold(0, 1), 2);
    }

    @Test public void recenteringGlidesWithoutJumps() {
        Head head = new Head();
        double previous = head.hold(90, 6);
        double maxStep = 0;
        for (int i = 0; i < 32 * 10; i++) {
            double now = head.hold(90, 1.0 / 32);
            maxStep = Math.max(maxStep, Math.abs(now - previous));
            assertTrue("monotonic", Math.abs(now) <= Math.abs(previous) + 1e-6);
            previous = now;
        }
        // At most 30 degrees per second, i.e. under one degree per 32 Hz sample.
        assertTrue("max step " + maxStep, maxStep < 1.0);
        assertTrue("recentred: " + previous, Math.abs(previous) < 2);
    }

    @Test public void recenteringIsStableAtTheBudsIdleReportRate() {
        // The buds report at about 1 Hz while the head is still (seen on device): the stage must
        // still glide monotonically, not oscillate from sample to sample.
        Head head = new Head();
        head.hold(25, 1);
        double previous = head.hold(25, 6, 1);
        for (int i = 0; i < 15; i++) {
            double now = head.hold(25, 1, 1);
            assertTrue("monotonic at 1 Hz: " + previous + " -> " + now,
                    Math.abs(now) <= Math.abs(previous) + 1e-6 && now * previous >= -1e-6);
            previous = now;
        }
        assertTrue("recentred: " + previous, Math.abs(previous) < 2);
    }

    @Test public void briefGlanceDoesNotMoveTheStage() {
        Head head = new Head();
        assertEquals(-60, head.hold(60, 5), 1.5);
        assertEquals(0, head.hold(0, 1), 1);
        assertEquals(0, head.hold(0, 20), 1);
    }

    @Test public void smallOffsetsOnlyDriftSlowly() {
        Head head = new Head();
        // 10 degrees is below the recentering threshold: only the 120 s drift filter acts.
        double after = head.hold(10, 30);
        assertTrue("slow drift only: " + after, after < -7 && after > -9);
    }

    @Test public void continuousMovementNeverRecenters() {
        Head head = new Head();
        double deg = 0;
        for (int i = 0; i < 60; i++) {           // a minute of turning back and forth
            deg = (i % 2 == 0) ? 45 : -45;
            head.hold(deg, 1);
        }
        assertEquals(-deg, head.hold(deg, 0.1), 3);
    }

    @Test public void movingDuringRecenteringAbortsIt() {
        Head head = new Head();
        double partial = head.hold(40, 8);        // 2 s into recentering
        assertTrue("in progress: " + partial, partial < -10 && partial > -35);
        // The head moves away: recentering stops and the stage slows to a stop part-way, so the
        // head at -30 reads well beyond 30 degrees.
        double moved = head.hold(-30, 2);
        assertTrue("aborted: " + moved, moved > 35 && moved < 70);
        assertEquals(moved, head.hold(-30, 2), 1);  // no further movement before the next rest
    }

    @Test public void decodesRoleFromLongAndShortGrvForms() {
        byte[] longForm = new byte[1 + 37];
        longForm[0] = (byte) BudsSpatialProtocol.DATA_BUD_GRV;
        longForm[7] = (byte) 0x10; // w = 0x2710 = 10000
        longForm[8] = (byte) 0x27;
        longForm[1 + 8] = 1;       // accelerometer byte in the long form, not the role
        longForm[1 + 36] = 0;      // role byte
        BudsSpatialProtocol.QuaternionSample q = BudsSpatialProtocol.decodeQuaternion(
                new BudsSpatialProtocol.Frame(BudsSpatialProtocol.SPATIAL_AUDIO_DATA, longForm));
        assertNotNull(q);
        assertEquals(1.0, q.w, 1e-9);
        assertTrue(q.auxiliaryFlag);
        longForm[1 + 36] = 1;
        assertFalse(BudsSpatialProtocol.decodeQuaternion(new BudsSpatialProtocol.Frame(
                BudsSpatialProtocol.SPATIAL_AUDIO_DATA, longForm)).auxiliaryFlag);

        byte[] shortForm = new byte[1 + 9];
        shortForm[0] = (byte) BudsSpatialProtocol.DATA_BUD_GRV;
        shortForm[1 + 8] = 0;
        assertTrue(BudsSpatialProtocol.decodeQuaternion(new BudsSpatialProtocol.Frame(
                BudsSpatialProtocol.SPATIAL_AUDIO_DATA, shortForm)).auxiliaryFlag);
    }
}
