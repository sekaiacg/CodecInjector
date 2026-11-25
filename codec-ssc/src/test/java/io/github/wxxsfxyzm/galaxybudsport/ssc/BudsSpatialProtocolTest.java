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
}
