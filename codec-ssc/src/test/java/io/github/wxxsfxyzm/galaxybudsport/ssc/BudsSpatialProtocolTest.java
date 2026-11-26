package io.github.wxxsfxyzm.galaxybudsport.ssc;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

public class BudsSpatialProtocolTest {
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

    @Test public void readsSpatialSwitchesFromSetSpatialAudio() {
        // The hearable managers send [360 audio, head tracking].
        boolean[] both = BudsPoseTap.spatialSwitches(new BudsSpatialProtocol.Frame(
                BudsSpatialProtocol.SET_SPATIAL_AUDIO, new byte[] {1, 0}));
        assertEquals(2, both.length);
        assertTrue(both[0]);
        assertFalse(both[1]);
        boolean[] tracking = BudsPoseTap.spatialSwitches(new BudsSpatialProtocol.Frame(
                BudsSpatialProtocol.SET_SPATIAL_AUDIO, new byte[] {1, 1}));
        assertTrue(tracking[0] && tracking[1]);
        // Older firmware: the 360 audio switch only.
        boolean[] spatial = BudsPoseTap.spatialSwitches(new BudsSpatialProtocol.Frame(
                BudsSpatialProtocol.SET_SPATIAL_AUDIO, new byte[] {0}));
        assertEquals(1, spatial.length);
        assertFalse(spatial[0]);
        assertEquals(null, BudsPoseTap.spatialSwitches(new BudsSpatialProtocol.Frame(
                BudsSpatialProtocol.SPATIAL_AUDIO_CONTROL, new byte[] {1, 1})));
    }
}
