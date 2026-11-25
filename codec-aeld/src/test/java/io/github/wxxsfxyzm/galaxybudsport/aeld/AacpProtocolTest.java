package io.github.wxxsfxyzm.galaxybudsport.aeld;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import java.io.ByteArrayOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.List;
import org.junit.Test;

public class AacpProtocolTest {
    private static byte[] bytes(int... values) {
        byte[] out = new byte[values.length];
        for (int i = 0; i < values.length; ++i) out[i] = (byte) values[i];
        return out;
    }

    /** A sensor frame around a protobuf body. */
    private static byte[] frame(byte[] body) {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        out.write(bytes(4, 0, 4, 0, 0x17, 0, 0, 0, 0x10, 0, body.length, body.length >> 8), 0, 12);
        out.write(body, 0, body.length);
        return out.toByteArray();
    }

    private static byte[] field(int id, byte[] value) {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        out.write((id << 3) | 2);
        out.write(value.length);
        out.write(value, 0, value.length);
        return out.toByteArray();
    }

    private static byte[] concat(byte[]... parts) {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        for (byte[] p : parts) out.write(p, 0, p.length);
        return out.toByteArray();
    }

    @Test public void subscriptionEncoding() {
        // service 16, 20 ms, sequence 1025 (varint 81 08)
        assertArrayEquals(bytes(4, 0, 4, 0, 0x17, 0, 0, 0, 0x10, 0, 16, 0,
                        0x08, 0x81, 0x08, 0x42, 11,
                        0x08, 16, 0x10, 2, 0x1a, 5, 1, 0x20, 0x4e, 0, 0),
                AacpProtocol.subscription(16, 20000, 1025));
        assertArrayEquals(bytes(4, 0, 4, 0, 0x17, 0, 0, 0, 0x10, 0, 5, 0, 0x08, 0x81, 0x08, 0x22, 0),
                AacpProtocol.serviceQuery(1025));
        assertArrayEquals(bytes(4, 0, 4, 0, 9, 0, 6, 1, 0, 0, 0), AacpProtocol.control(6, 1));
    }

    @Test public void motionReport() {
        byte[] blob = new byte[58];
        blob[0] = 1;
        blob[9] = 3;
        // x = 0.5, y = -0.25, z = 0 (int16 / 32768)
        blob[20] = 0x00; blob[21] = 0x40;
        blob[22] = 0x00; blob[23] = (byte) 0xe0;
        byte[] report = concat(bytes(0x08, 16), field(3, blob));
        byte[] packet = frame(field(7, report));
        double[] q = AacpProtocol.motion(packet, 16);
        assertEquals(0.5, q[0], 1e-9);
        assertEquals(-0.25, q[1], 1e-9);
        assertEquals(0, q[2], 1e-9);
        assertEquals(Math.sqrt(1 - 0.3125), q[3], 1e-9);
        assertNull(AacpProtocol.motion(packet, 15));
        // A two-byte trailer after the protobuf is tolerated.
        byte[] trailed = frame(concat(field(7, report), bytes(0xff, 0xff)));
        assertEquals(0.5, AacpProtocol.motion(trailed, 16)[0], 1e-9);
    }

    @Test public void serviceDescriptors() {
        byte[] metadata = "\u0001AccessoryService\u0002devmotion6\u0003".getBytes(StandardCharsets.US_ASCII);
        byte[] other = "\u0001AccessoryService\u0002activity\u0003".getBytes(StandardCharsets.US_ASCII);
        byte[] packet = frame(concat(field(5, concat(bytes(0x08, 16), field(2, metadata))),
                field(5, concat(bytes(0x08, 14), field(2, other)))));
        assertEquals(List.of(16), AacpProtocol.servicesNamed(packet, AacpProtocol.MOTION_SERVICE));
        assertEquals("16:AccessoryService,devmotion6 14:AccessoryService,activity",
                AacpProtocol.describeServices(packet));
        long[] ack = AacpProtocol.acknowledgement(frame(field(9, bytes(0x08, 16, 0x10, 0))));
        assertArrayEquals(new long[] {16, 0}, ack);
    }

    @Test public void splitsReads() {
        byte[] control = bytes(4, 0, 4, 0, 9, 0, 6, 1, 0, 0, 0);
        byte[] sensor = frame(field(9, bytes(0x08, 16)));
        byte[] handshake = bytes(0, 0, 4, 0, 1, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        byte[] other = bytes(4, 0, 4, 0, 6, 0, 1, 1);
        byte[] read = concat(handshake, control, sensor, other);
        List<byte[]> packets = AacpProtocol.split(read, read.length);
        assertEquals(3, packets.size());
        assertArrayEquals(control, packets.get(0));
        assertArrayEquals(sensor, packets.get(1));
        assertArrayEquals(other, packets.get(2));
        assertArrayEquals(new int[] {6, 1}, AacpProtocol.controlReport(packets.get(0)));
    }
}
