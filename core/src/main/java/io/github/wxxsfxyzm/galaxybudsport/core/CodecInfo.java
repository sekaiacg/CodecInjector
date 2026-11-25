package io.github.wxxsfxyzm.galaxybudsport.core;

import android.bluetooth.BluetoothCodecConfig;
import android.bluetooth.BluetoothCodecType;

/** A vendor codec the module adds: the stack's codec index, 64-bit codec ID and display name. */
public final class CodecInfo {
    public final int index;
    public final long codecId;
    public final String name;

    public CodecInfo(int index, long codecId, String name) {
        this.index = index;
        this.codecId = codecId;
        this.name = name;
    }

    public boolean matches(BluetoothCodecConfig config) {
        BluetoothCodecType type = config == null ? null : config.getExtendedCodecType();
        return type != null && type.getCodecId() == codecId;
    }
}
