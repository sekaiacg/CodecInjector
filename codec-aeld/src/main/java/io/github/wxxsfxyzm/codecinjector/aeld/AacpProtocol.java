package io.github.wxxsfxyzm.codecinjector.aeld;

import java.io.ByteArrayOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

/**
 * The parts of Apple's accessory control protocol (AACP, BR/EDR L2CAP PSM 0x1001) used for AirPods
 * head tracking. Every packet starts with {@code 04 00 04 00} and an opcode; the 16-byte
 * {@code 00 00 04 00 ...} packets are the connection handshake.
 * <p>
 * Motion comes from the AirPods' sensor interface ("RTBuddy", opcode 0x17): a 12-byte frame header
 * ({@code 04 00 04 00 17 00}, descriptor {@code 00 00 10 00}, little-endian body length) and a
 * protobuf body. The firmware lists its sensor services (field 5: id, metadata naming the service),
 * a subscription names a service and a report interval, replies acknowledge it (field 9), and the
 * reports (field 3, or field 7 with the service id) carry orientation blobs with an int16
 * quaternion vector part.
 * <p>
 * Protocol facts from the AirPods research of LibrePods, HyperPods-X and the aap-head-tracker /
 * HeadTracker projects; this is an independent implementation.
 */
final class AacpProtocol {
    static final int PSM = 0x1001;
    static final int OPCODE_CONTROL = 0x09;
    static final int OPCODE_SENSOR = 0x17;
    /** Control id 6: which host owns the AirPods' control session (1 = the sender). */
    static final int CONTROL_OWNER = 6;
    /** The head motion service's name in the firmware's descriptors. */
    static final String MOTION_SERVICE = "devmotion6";
    /** Report interval in microseconds (50 Hz). */
    static final int MOTION_INTERVAL_US = 20000;

    private static final byte[] HEADER = {4, 0, 4, 0};

    private AacpProtocol() {
    }

    static byte[] handshake() {
        return new byte[] {0, 0, 4, 0, 1, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    }

    static byte[] features() {
        return packet(new byte[] {0x4d, 0, (byte) 0xd7, 0, 0, 0, 0, 0, 0, 0});
    }

    static byte[] notifications() {
        return packet(new byte[] {0x0f, 0, -1, -1, -1, -1});
    }

    /** Opens the sensor interface's channels before its services are queried. */
    static List<byte[]> sensorSetup() {
        List<byte[]> setup = new ArrayList<>();
        setup.add(new byte[] {0, 0, 0, 0, 1, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0});
        setup.add(new byte[] {4, 0, 0, 0, 1, 0, 0});
        setup.add(new byte[] {0, 0, 4, 0, 1, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0});
        setup.add(new byte[] {4, 0, 4, 0, 1, 0, 0});
        return setup;
    }

    static byte[] control(int id, int value) {
        return packet(new byte[] {OPCODE_CONTROL, 0, (byte) id, (byte) value, 0, 0, 0});
    }

    /** Asks the firmware for its sensor service descriptors. */
    static byte[] serviceQuery(int sequence) {
        ByteArrayOutputStream body = new ByteArrayOutputStream();
        body.write(0x08);
        varint(body, sequence);
        body.write(0x22);
        body.write(0);
        return sensor(body.toByteArray());
    }

    /** Subscribes to a sensor service; an interval of 0 ends the subscription. */
    static byte[] subscription(int service, int intervalMicros, int sequence) {
        ByteArrayOutputStream setting = new ByteArrayOutputStream();
        setting.write(0x08);
        varint(setting, service);
        setting.write(0x10);
        setting.write(2);
        setting.write(0x1a);
        setting.write(5);
        setting.write(1);
        for (int i = 0; i < 4; ++i) setting.write((intervalMicros >>> (8 * i)) & 0xff);
        ByteArrayOutputStream body = new ByteArrayOutputStream();
        body.write(0x08);
        varint(body, sequence);
        body.write(0x42);
        varint(body, setting.size());
        body.write(setting.toByteArray(), 0, setting.size());
        return sensor(body.toByteArray());
    }

    /** Splits one read into packets; unknown packets run to the next packet header. */
    static List<byte[]> split(byte[] data, int length) {
        List<byte[]> packets = new ArrayList<>();
        int pos = 0;
        while (length - pos >= 4) {
            if (data[pos] == 0 && data[pos + 1] == 0 && data[pos + 2] == 4 && data[pos + 3] == 0) {
                pos += Math.min(16, length - pos);
                continue;
            }
            if (!startsWithHeader(data, pos, length)) {
                ++pos;
                continue;
            }
            int size = -1;
            if (length - pos >= 6) {
                int opcode = data[pos + 4] & 0xff;
                if (opcode == OPCODE_SENSOR && length - pos >= 12) {
                    size = 12 + (u8(data, pos + 10) | (u8(data, pos + 11) << 8));
                } else if (opcode == OPCODE_CONTROL) {
                    size = 11;
                }
            }
            if (size < 6) {
                size = length - pos;
                for (int next = pos + 4; next + 4 <= length; ++next) {
                    if (startsWithHeader(data, next, length)) {
                        size = next - pos;
                        break;
                    }
                }
            }
            if (size > length - pos) break;
            byte[] packet = new byte[size];
            System.arraycopy(data, pos, packet, 0, size);
            packets.add(packet);
            pos += size;
        }
        return packets;
    }

    static int opcode(byte[] packet) {
        return packet.length >= 6 && startsWithHeader(packet, 0, packet.length) ? packet[4] & 0xff : -1;
    }

    /** A control report: {id, value}, or null. */
    static int[] controlReport(byte[] packet) {
        if (opcode(packet) != OPCODE_CONTROL || packet.length < 8) return null;
        return new int[] {u8(packet, 6), u8(packet, 7)};
    }

    /** Service ids whose descriptor names {@code name}, from a descriptor report. */
    static List<Integer> servicesNamed(byte[] packet, String name) {
        List<Integer> ids = new ArrayList<>();
        List<Field> top = sensorFields(packet);
        if (top == null) return ids;
        byte[] needle = name.getBytes(StandardCharsets.US_ASCII);
        for (Field field : top) {
            if (field.id != 5 || field.bytes == null) continue;
            List<Field> descriptor = fields(field.bytes, 0, field.bytes.length);
            if (descriptor == null) continue;
            Long id = number(descriptor, 1);
            byte[] metadata = bytes(descriptor, 2);
            if (id == null || id < 1 || id > 255 || metadata == null) continue;
            if (indexOf(metadata, needle) >= 0) ids.add(id.intValue());
        }
        return ids;
    }

    /** All descriptor ids with the printable names in their metadata, for diagnostics. */
    static String describeServices(byte[] packet) {
        List<Field> top = sensorFields(packet);
        if (top == null) return "";
        StringBuilder out = new StringBuilder();
        for (Field field : top) {
            if (field.id != 5 || field.bytes == null) continue;
            List<Field> descriptor = fields(field.bytes, 0, field.bytes.length);
            if (descriptor == null) continue;
            Long id = number(descriptor, 1);
            byte[] metadata = bytes(descriptor, 2);
            out.append(id).append(':').append(metadata == null ? "" : printable(metadata)).append(' ');
        }
        return out.toString().trim();
    }

    /** Subscription acknowledgement: {service, error}, or null. */
    static long[] acknowledgement(byte[] packet) {
        List<Field> top = sensorFields(packet);
        if (top == null) return null;
        for (Field field : top) {
            if (field.id != 9 || field.bytes == null) continue;
            List<Field> nested = fields(field.bytes, 0, field.bytes.length);
            if (nested == null) continue;
            Long service = number(nested, 1);
            Long error = number(nested, 2);
            if (service != null) return new long[] {service, error == null ? 0 : error};
        }
        return null;
    }

    /** The orientation quaternion {x, y, z, w} of a motion report from {@code service}, or null. */
    static double[] motion(byte[] packet, int service) {
        List<Field> top = sensorFields(packet);
        if (top == null) return null;
        for (Field field : top) {
            if ((field.id != 3 && field.id != 7) || field.bytes == null) continue;
            List<Field> nested = fields(field.bytes, 0, field.bytes.length);
            if (nested == null) continue;
            if (field.id == 7) {
                Long id = number(nested, 1);
                if (id == null || id != service) continue;
            }
            for (Field blobField : nested) {
                byte[] blob = blobField.bytes;
                if (blobField.id != 3 || blob == null || blob.length < 58) continue;
                if (u8(blob, 0) != 1 || u8(blob, 9) != 3) continue;
                double x = s16(blob, 20) / 32768.0;
                double y = s16(blob, 22) / 32768.0;
                double z = s16(blob, 24) / 32768.0;
                double n = x * x + y * y + z * z;
                if (n < 0.0001 || n > 1.01) continue;
                return new double[] {x, y, z, Math.sqrt(Math.max(0, 1 - n))};
            }
        }
        return null;
    }

    // ---- framing and protobuf

    private static byte[] packet(byte[] payload) {
        byte[] out = new byte[HEADER.length + payload.length];
        System.arraycopy(HEADER, 0, out, 0, HEADER.length);
        System.arraycopy(payload, 0, out, HEADER.length, payload.length);
        return out;
    }

    private static byte[] sensor(byte[] body) {
        byte[] out = new byte[12 + body.length];
        System.arraycopy(HEADER, 0, out, 0, 4);
        out[4] = OPCODE_SENSOR;
        out[8] = 0x10;
        out[10] = (byte) body.length;
        out[11] = (byte) (body.length >>> 8);
        System.arraycopy(body, 0, out, 12, body.length);
        return out;
    }

    /** The protobuf fields of a sensor frame body, or null when it is not one. */
    private static List<Field> sensorFields(byte[] p) {
        if (p.length < 12 || opcode(p) != OPCODE_SENSOR || p[5] != 0 || p[6] != 0 || p[7] != 0
                || p[8] != 0x10 || p[9] != 0) {
            return null;
        }
        int length = u8(p, 10) | (u8(p, 11) << 8);
        if (12 + length != p.length) return null;
        List<Field> fields = fields(p, 12, p.length);
        if (fields != null) return fields;
        // Some report types carry a two-byte trailer after the protobuf.
        return length >= 3 ? fields(p, 12, p.length - 2) : null;
    }

    static final class Field {
        final int id;
        final long number;
        final byte[] bytes;

        Field(int id, long number, byte[] bytes) {
            this.id = id;
            this.number = number;
            this.bytes = bytes;
        }
    }

    /** Parses protobuf fields of p[from, to); null on malformed input. */
    static List<Field> fields(byte[] p, int from, int to) {
        List<Field> out = new ArrayList<>();
        int[] pos = {from};
        while (pos[0] < to) {
            if (out.size() >= 128) return null;
            long key = varint(p, pos, to);
            if (key <= 0 || key > Integer.MAX_VALUE) return null;
            int id = (int) (key >>> 3);
            if (id == 0) return null;
            switch ((int) (key & 7)) {
                case 0: {
                    long value = varint(p, pos, to);
                    if (value < 0) return null;
                    out.add(new Field(id, value, null));
                    break;
                }
                case 2: {
                    long n = varint(p, pos, to);
                    if (n < 0 || n > to - pos[0]) return null;
                    byte[] bytes = new byte[(int) n];
                    System.arraycopy(p, pos[0], bytes, 0, (int) n);
                    pos[0] += (int) n;
                    out.add(new Field(id, 0, bytes));
                    break;
                }
                case 1:
                case 5: {
                    int n = (key & 7) == 1 ? 8 : 4;
                    if (pos[0] + n > to) return null;
                    pos[0] += n;
                    break;
                }
                default:
                    return null;
            }
        }
        return out;
    }

    /** Reads a varint at pos[0]; -1 when truncated or too long. */
    private static long varint(byte[] p, int[] pos, int to) {
        long result = 0;
        for (int i = 0; i < 10; ++i) {
            if (pos[0] >= to) return -1;
            int b = p[pos[0]++] & 0xff;
            if (i == 9 && b > 1) return -1;
            result |= (long) (b & 0x7f) << (7 * i);
            if (b < 0x80) return result;
        }
        return -1;
    }

    private static void varint(ByteArrayOutputStream out, int value) {
        int next = value;
        do {
            int low = next & 0x7f;
            next >>>= 7;
            out.write(low | (next != 0 ? 0x80 : 0));
        } while (next != 0);
    }

    private static Long number(List<Field> fields, int id) {
        for (Field f : fields) if (f.id == id && f.bytes == null) return f.number;
        return null;
    }

    private static byte[] bytes(List<Field> fields, int id) {
        for (Field f : fields) if (f.id == id && f.bytes != null) return f.bytes;
        return null;
    }

    private static boolean startsWithHeader(byte[] p, int pos, int length) {
        return length - pos >= 4 && p[pos] == 4 && p[pos + 1] == 0 && p[pos + 2] == 4 && p[pos + 3] == 0;
    }

    private static int indexOf(byte[] haystack, byte[] needle) {
        outer:
        for (int i = 0; i + needle.length <= haystack.length; ++i) {
            for (int j = 0; j < needle.length; ++j) {
                if (Character.toLowerCase(haystack[i + j]) != Character.toLowerCase(needle[j])) {
                    continue outer;
                }
            }
            return i;
        }
        return -1;
    }

    private static String printable(byte[] p) {
        StringBuilder out = new StringBuilder();
        StringBuilder run = new StringBuilder();
        for (byte b : p) {
            if (b >= 0x20 && b < 0x7f) {
                run.append((char) b);
            } else {
                if (run.length() >= 4) out.append(out.length() > 0 ? "," : "").append(run);
                run.setLength(0);
            }
        }
        if (run.length() >= 4) out.append(out.length() > 0 ? "," : "").append(run);
        return out.toString();
    }

    private static int u8(byte[] p, int i) {
        return p[i] & 0xff;
    }

    private static int s16(byte[] p, int i) {
        return (short) ((p[i] & 0xff) | ((p[i + 1] & 0xff) << 8));
    }
}
