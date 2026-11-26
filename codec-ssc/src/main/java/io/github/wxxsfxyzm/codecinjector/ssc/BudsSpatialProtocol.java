package io.github.wxxsfxyzm.codecinjector.ssc;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

final class BudsSpatialProtocol {
    static final int STATUS_UPDATED = 0x60;
    static final int EXTENDED_STATUS_UPDATED = 0x61;
    static final int SET_SPATIAL_AUDIO = 0x7c;
    static final int MANAGER_INFO = 0x88;
    static final int SPATIAL_AUDIO_DATA = 0xc2;
    static final int SPATIAL_AUDIO_CONTROL = 0xc3;

    static final int CONTROL_ATTACH = 0x00;
    static final int CONTROL_DETACH = 0x01;
    static final int CONTROL_ATTACH_SUCCESS = 0x02;
    static final int CONTROL_KEEP_ALIVE = 0x04;
    static final int DATA_BUD_GRV = 0x20;

    private static final byte MANAGER_INFO_VERSION = 0x01;
    private static final byte MANAGER_INFO_CLIENT_SAMSUNG = 0x01;
    private static final byte MANAGER_INFO_ANDROID_SDK = 0x22;

    private static final int START_OF_MESSAGE = 0xfd;
    private static final int END_OF_MESSAGE = 0xdd;
    private static final int MAX_DECLARED_SIZE = 0x3ff;
    private static final int MAX_BUFFER_SIZE = 8192;

    private BudsSpatialProtocol() {
    }

    static byte[] encodeRequest(int messageId, int payloadByte) {
        return encodeRequest(messageId, new byte[]{(byte) payloadByte});
    }

    static byte[] encodeRequest(int messageId, byte[] payload) {
        if (messageId < 0 || messageId > 0xff || payload == null) {
            throw new IllegalArgumentException("invalid message");
        }
        int declaredSize = 1 + payload.length + 2;
        if (declaredSize > MAX_DECLARED_SIZE) {
            throw new IllegalArgumentException("payload too large");
        }

        byte[] packet = new byte[declaredSize + 4];
        packet[0] = (byte) START_OF_MESSAGE;
        packet[1] = (byte) declaredSize;
        packet[2] = (byte) (declaredSize >>> 8);
        packet[3] = (byte) messageId;
        System.arraycopy(payload, 0, packet, 4, payload.length);
        int crc = crc16Ccitt(packet, 3, 1 + payload.length);
        packet[4 + payload.length] = (byte) crc;
        packet[5 + payload.length] = (byte) (crc >>> 8);
        packet[6 + payload.length] = (byte) END_OF_MESSAGE;
        return packet;
    }

    static byte[] encodeManagerInfo() {
        return encodeRequest(MANAGER_INFO, new byte[]{
                MANAGER_INFO_VERSION,
                MANAGER_INFO_CLIENT_SAMSUNG,
                MANAGER_INFO_ANDROID_SDK
        });
    }

    static int crc16Ccitt(byte[] data, int offset, int length) {
        if (data == null || offset < 0 || length < 0 || offset + length > data.length) {
            throw new IllegalArgumentException("invalid CRC range");
        }
        int crc = 0;
        for (int index = offset; index < offset + length; index++) {
            crc ^= (data[index] & 0xff) << 8;
            for (int bit = 0; bit < 8; bit++) {
                crc = (crc & 0x8000) != 0 ? (crc << 1) ^ 0x1021 : crc << 1;
                crc &= 0xffff;
            }
        }
        return crc;
    }

    static boolean isAttachSuccess(Frame frame) {
        return frame != null && frame.messageId == SPATIAL_AUDIO_CONTROL
                && frame.payload.length >= 1
                && (frame.payload[0] & 0xff) == CONTROL_ATTACH_SUCCESS;
    }

    static boolean isSessionStatus(Frame frame) {
        return frame != null && (frame.messageId == STATUS_UPDATED
                || frame.messageId == EXTENDED_STATUS_UPDATED);
    }

    static QuaternionSample decodeQuaternion(Frame frame) {
        if (frame == null || frame.messageId != SPATIAL_AUDIO_DATA
                || frame.payload.length < 10
                || (frame.payload[0] & 0xff) != DATA_BUD_GRV) {
            return null;
        }
        // After the event id: four int16 quaternion components (x, y, z, w) / 10000, then the
        // reporting bud's role. The 37-byte form (with accel/gyro/throughput) carries the role at
        // offset 36, the short form right after the quaternion (as the Buds manager parses it).
        int body = frame.payload.length - 1;
        int roleOffset = 1 + (body == 37 ? 36 : 8);
        if (frame.payload.length <= roleOffset) return null;
        return new QuaternionSample(
                readInt16Le(frame.payload, 1) / 10000.0,
                readInt16Le(frame.payload, 3) / 10000.0,
                readInt16Le(frame.payload, 5) / 10000.0,
                readInt16Le(frame.payload, 7) / 10000.0,
                frame.payload[roleOffset] == 0);
    }

    private static short readInt16Le(byte[] data, int offset) {
        return (short) ((data[offset] & 0xff) | ((data[offset + 1] & 0xff) << 8));
    }

    static final class Frame {
        final int messageId;
        final byte[] payload;

        Frame(int messageId, byte[] payload) {
            this.messageId = messageId;
            this.payload = payload;
        }
    }

    static final class QuaternionSample {
        final double x;
        final double y;
        final double z;
        final double w;
        final boolean auxiliaryFlag;

        QuaternionSample(double x, double y, double z, double w, boolean auxiliaryFlag) {
            this.x = x;
            this.y = y;
            this.z = z;
            this.w = w;
            this.auxiliaryFlag = auxiliaryFlag;
        }
    }

    static final class StreamDecoder {
        private final byte[] buffer = new byte[MAX_BUFFER_SIZE];
        private int buffered;
        private long framingErrors;
        private long crcErrors;

        List<Frame> append(byte[] data, int offset, int length) {
            if (data == null || offset < 0 || length < 0 || offset + length > data.length) {
                throw new IllegalArgumentException("invalid stream range");
            }
            if (length == 0) {
                return Collections.emptyList();
            }
            if (buffered + length > buffer.length) {
                throw new IllegalStateException("SPP stream buffer overflow");
            }
            System.arraycopy(data, offset, buffer, buffered, length);
            buffered += length;

            List<Frame> frames = new ArrayList<>();
            while (buffered >= 4) {
                int start = findStart();
                if (start < 0) {
                    framingErrors++;
                    buffered = 0;
                    break;
                }
                if (start > 0) {
                    framingErrors++;
                    removePrefix(start);
                }
                if (buffered < 4) {
                    break;
                }

                int header = (buffer[1] & 0xff) | ((buffer[2] & 0xff) << 8);
                int declaredSize = header & MAX_DECLARED_SIZE;
                if (declaredSize < 3) {
                    framingErrors++;
                    removePrefix(1);
                    continue;
                }
                int totalSize = declaredSize + 4;
                if (buffered < totalSize) {
                    break;
                }
                if ((buffer[totalSize - 1] & 0xff) != END_OF_MESSAGE) {
                    framingErrors++;
                    removePrefix(1);
                    continue;
                }

                int payloadLength = declaredSize - 3;
                int expectedCrc = crc16Ccitt(buffer, 3, 1 + payloadLength);
                int receivedCrc = (buffer[4 + payloadLength] & 0xff)
                        | ((buffer[5 + payloadLength] & 0xff) << 8);
                if (expectedCrc != receivedCrc) {
                    crcErrors++;
                    removePrefix(1);
                    continue;
                }

                int messageId = buffer[3] & 0xff;
                byte[] payload = Arrays.copyOfRange(buffer, 4, 4 + payloadLength);
                frames.add(new Frame(messageId, payload));
                removePrefix(totalSize);
            }
            return frames;
        }

        long getFramingErrors() {
            return framingErrors;
        }

        long getCrcErrors() {
            return crcErrors;
        }

        private int findStart() {
            for (int index = 0; index < buffered; index++) {
                if ((buffer[index] & 0xff) == START_OF_MESSAGE) {
                    return index;
                }
            }
            return -1;
        }

        private void removePrefix(int count) {
            if (count <= 0) {
                return;
            }
            int remaining = buffered - count;
            if (remaining > 0) {
                System.arraycopy(buffer, count, buffer, 0, remaining);
            }
            buffered = Math.max(remaining, 0);
        }
    }
}
