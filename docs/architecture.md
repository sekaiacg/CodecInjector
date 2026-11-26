# Architecture

```
 audio HAL (A2DP software session, PCM 48/96 kHz, packed 24-bit)
        │ read callback
        ▼
 Bluetooth process ── libcodecinjector.so ─────────────────────────────────────────┐
 │  SSC encoder interface (ssc_interface)                                          │
 │    Source/Frame: 864 samples per channel → Q8.23                                │
 │    DolbyHalStage ──FMQ──► audio HAL process: Dolby Spatializer (BINAURAL, A2DP) │
 │    ssc_encode (libScalable_Encoder.so) at BitrateLadder bitrate                 │
 │    Packetizer → stock enqueue → AVDTP                                           │
 │  Java (PortRuntime + SscFeature): codec types, UHQ per device, quality, pose   │
 └─────────────────────────────────────────────────────────────────────────────────┘
        ▲ yaw (Binder, oneway)                ▲ broadcasts (sender-checked)
 Galaxy Buds manager process                  Settings process
   BudsPoseTap: SMEP frames on the            SettingsCodecBridge: SSC in the codec list,
   manager's own SPP socket → HeadYaw         device label
   BudsSettingsBridge (DexKit): 360 audio,    SettingsQualityBridge: SSC playback quality
   head tracking, UHQ switches
```

## Bluetooth stack (native)

The project is split into `:core` (stack hooks, codec plugin registry, Dolby stage, Java runtime),
`:codec-ssc` (SSC and Galaxy Buds) and `:codec-aeld` (AAC-ELD for AirPods); `:app` links them into
one LSPosed module. Each codec is a `port::CodecPlugin` (`core/src/main/cpp/codec_plugin.h`): its
index, codec ID, codec table, capability, matching and encoder; the app lists them in `codecs.cpp`.

`core/src/main/cpp/stack.cpp` is loaded by LSPosed in `com.android.bluetooth`. When `libbluetooth_jni.so` is
loaded, it resolves the following symbols. It searches the dynamic and static symbol tables, then
the xz-compressed `.gnu_debugdata` section (`elf_symbols.cpp`, using xz-embedded):

| Symbol | Purpose |
|---|---|
| `A2dpCodecs::init`, `A2dpCodecs::isSupportedCodec`, `provider::supports_codec` | register the plugin codecs (SSC 18, AAC-ELD 17) and keep them on the software path |
| `A2dpCodecConfigExt::A2dpCodecConfigExt`, `A2dpCodecConfig` C2/D2, `operator delete(void*, size_t)` | construct the SSC codec object |
| `A2DP_VendorSourceCodecIndex`, `A2DP_IsVendorSourceCodecValid`, `A2DP_VendorInitCodecConfig`, `A2DP_VendorGetEncoderInterface`, `A2DP_BuildCodecHeader`, `A2DP_GetPacketTimestamp` | vendor codec dispatch (`vendor_dispatch`, routed to the plugin whose codec information matches) |
| `osi_malloc` | packet buffers |

A missing symbol only skips its hook.

### Codec information (`ssc_negotiation`, `ssc_codec_state`)

The codec information element is `09 00 ff 75 00 00 00 03 01 <mode>`. Byte 9, following Samsung's
`A2DP_ParseInfoSsc`:

| Bits | Meaning |
|---|---|
| `0xf0` | sample-rate bits in capabilities (`0x10` 48 k, `0x20` 44.1 k) |
| `0x08` | bitrate limitation |
| `0x04` | ordinary SSC: 48 kHz, 24-bit |
| `0x02` | SSC UHQ: 96 kHz |
| `0x01` | reserved; UHQ is not selected when it is set |

- Galaxy Buds advertise `0x3e`. The local SEP capability is `0x0c`.
- Selection: 96 kHz when the framework explicitly requests 96 kHz, or when nothing is requested and
  the device's UHQ switch is on. 48 kHz otherwise (Samsung's rule). The selected mode is
  `0x02 | (peer & 0x08)` or `0x04 | (peer & 0x08)`.
- A peer-chosen configuration is accepted as it is.

### Encoder (`ssc_interface`, `ssc_frame`, `ssc_bitrate`, `ssc_packet`)

- Samsung's parameters: `ssc_encoder_create(96000 | 44100, 2, 24)` (44100 is what Samsung passes
  for 48 kHz SSC). Each frame is 864 samples per channel, so 18 ms at 48 kHz and 9 ms at 96 kHz.
- PCM arrives as packed 24-bit (5184 bytes per frame) and is converted to Q8.23.
- One frame per packet.
- Bitrate levels 0/2/3 are UHQ 584/291/152 kbps and SSC 192/128/88 kbps.
  - Samsung phones get the busy level from Bluetooth controller events. Here it follows the
    transmit queue length that the stack reports every tick.
  - A queue of 2 or more for 3 reports → level 2. A queue of 4 or more for 3 reports → level 3.
  - A queue under 2 for 100 reports → one level back.
  - Fixed modes from developer options pin a level. The mode resets to adaptive on every A2DP
    connection.

### Spatial audio (`dolby_hal_stage`)

The Bluetooth process is a client of `android.hardware.audio.effect.IFactory/default`. It creates
the device's Dolby Spatializer (Xiaomi implementation UUID
`ccd4cf09-a79d-46c2-9aae-06a1698d6c8f`), which runs and passes its license check in the audio HAL
process. It opens it as stereo float at the session rate, with 864 frames.

- **Parameters:**
  - device description `OUT_DEVICE` / `bt-a2dp` (without it the effect renders for the speaker);
  - level `MULTICHANNEL`, mode `BINAURAL`;
  - head tracking `FRAMEWORK_PROCESSED`, `RELATIVE_WORLD`.
- **Data path:** status/input/output FMQs. The wake bits are `0x1 | 1 << 11`, as V2 effects wait on
  `kEventFlagDataMqNotEmpty`.
- **Pipelined:** each encoder frame submits the current block and outputs the previous one, which
  adds one frame of latency. The HAL round trip (3–4 ms) is never on the critical path.
- **Output:** scaled by 0.5, since Dolby's output reaches full scale.
- **Head pose:** a separate thread sends `headTrackingSensorData` `headToStage {0,0,0,0,0,yaw}`
  every 20 ms when the yaw has changed.
- **Verified rates:** 48 kHz and 96 kHz (frequency response matches at the same absolute
  frequencies, and content above 24 kHz is kept).

## Galaxy Buds manager (`com.samsung.accessory.budsunitemgr`)

- **`BudsPoseTap`**
  - It hooks `BluetoothDevice.create*RfcommSocketToServiceRecord` for the Buds SPP UUID
    `2e73a4ad-332d-41fc-90e2-16bef06523f2`, plus the socket's streams. This shares the manager's
    session instead of competing for the port.
  - It sends `SET_SPATIAL_AUDIO` (0x7c), then `SPATIAL_AUDIO_CONTROL` (0xc3) Attach, with KeepAlive
    every 2 s.
  - It decodes `SPATIAL_AUDIO_DATA` (0xc2) BudGrv (0x20) quaternions at about 32 Hz.
- **`HeadYaw`**
  - The head heading is the twist about world Z (gravity) of `q · conj(ref)`, so pitch and roll do
    not leak into it. The output is the head yaw relative to the stage's forward, positive when
    the head is turned left (AOSP).
  - Recentering (Apple style): when the head has rested in one direction (within ±8°) for 6 s, at
    least 15° away from forward, forward glides to it like a critically damped spring (95% in about
    4 s, at most 30°/s), starting and ending slowly. Moving out of the resting window aborts it and
    the stage slows to a stop. A brief look away does not move the stage.
  - Drift: only while the head is within 15° of forward, a 120 s low-pass takes the remaining
    offset as gyro heading drift (no magnetometer).
  - The two buds' world frames are vertically opposite, so the sign follows the reporting bud's
    role and a role switch restarts the reference.
  - The role byte is at payload offset 36 in the 37-byte form, or right after the quaternion in the
    short form.
- **`PoseLink`:** the Bluetooth process passes a Binder sink in a broadcast. Yaw is sent oneway, and
  the caller UID is checked.
- **`BudsSettingsBridge`:** DexKit finds the methods by their strings and signatures.

  | Manager method (found by) | Our behaviour |
  |---|---|
  | `SpatialSensorManager.isSupported` | returns true |
  | creation of Samsung's sensor object | skipped (its lifecycle throws on non-Samsung phones) |
  | spatial / head-tracking values | broadcast `BUDS_SPATIAL`, and attach/detach the pose stream |
  | `semIsCodecSupported` / `semIsCodecEnabled` / `semSetCodecEnabled(dev, on, 8)` reflection helpers | UHQ supported, the per-device preference, and broadcasting `BUDS_UHQ` with the address |

## Settings (`com.android.settings`)

- HyperOS 4 developer options show the legacy `bluetooth_select_a2dp_codec` (MIUI
  `DropDownPreference`, `BluetoothAudioCodecPreferenceController`). Selecting a codec builds the
  config through `Builder.setCodecType(int)`, that is `BluetoothCodecType.createFromType(int)`. So
  `createFromType(18)` returns SSC in both the Settings and Bluetooth processes.
- **`SettingsCodecBridge`:**
  - appends "SSC" to the list values, summaries, entries and entry values;
  - maps an SSC config to that entry;
  - writes type 18 with the highest priority when SSC is chosen;
  - labels the codec on the device details page (`BluetoothDevicePreference`).
- **`SettingsQualityBridge`:** a "SSC playback quality" drop-down after LDAC playback quality. It
  uses broadcasts `SSC_QUALITY_SET` / `SSC_QUALITY_QUERY` → `SSC_QUALITY_STATE` and never
  renegotiates the codec.

## Cross-process messages

| Action | From → to | Check |
|---|---|---|
| `BUDS_SPATIAL` (`enabled`) | Buds manager → Bluetooth | sender package |
| `BUDS_UHQ` (`address`, `enabled`) | Buds manager → Bluetooth | sender package |
| `REQUEST_POSE_SINK` / `POSE_SINK` (Binder `sink`) | Buds manager ↔ Bluetooth | caller UID on each transaction |
| `SSC_QUALITY_SET` / `SSC_QUALITY_QUERY` (`mode`) | Settings → Bluetooth | sender package |
| `SSC_QUALITY_STATE` (`mode`) | Bluetooth → Settings | sender package |
| `SELECT_CODEC --ei index <n>`, `SPATIAL_ON`, `SPATIAL_OFF` | adb → Bluetooth | `android.permission.DUMP` |

All actions are prefixed `io.github.wxxsfxyzm.codecinjector.action.`. Identity comes from
`BroadcastOptions.setShareIdentityEnabled` and `BroadcastReceiver.getSentFromPackage`.

## Per-device UHQ

- Native selection does not know the peer. So on A2DP `CONNECTING` the Bluetooth process hands that
  device's preference to native code.
- The first SSC codec config of each connection is then checked once and renegotiated if it does
  not match.
- Flipping the switch renegotiates the active SSC stream at 96 or 48 kHz through
  `setCodecConfigPreference`.

## AAC-ELD for AirPods (`codec-aeld`)

- **Index and identity.** Index 17, one of the free source indexes on this stack (10, 11 and 15–17
  are `UNKNOWN` in `A2DP_CodecIndexStr`; indexes above 20 are sink codecs). Codec ID
  `0x0000008001004cff` (`0xff | vendor << 8 | codec << 24`), name "AAC-ELD".
- **Codec information** (`aeld_codec_info`). `10 00 ff 4c 00 00 00 01 80`, then object type
  `0x0080` (AAC-ELD), sample rate / channels (`0x008` = 48 kHz in the high 12 bits, `0x4` stereo,
  `0x8` mono), a reserved byte and VBR flag + bitrate (24 bits, big endian). The local capability
  matches Apple's source: 48 kHz, mono and stereo, VBR, 256 kbps. The configuration is always
  48 kHz stereo; other peer configurations are refused.
- **Encoder** (`aeld_fdk`). The FDK AAC encoder inside `libbluetooth_jni.so` (`aacEncOpen`,
  `aacEncoder_SetParam`, `aacEncEncode`, `aacEncClose`, resolved from MiniDebugInfo): AOT 39, 48 kHz
  stereo, 480-sample frames, CBR 265 kbps, no SBR, no afterburner, raw AUs. PCM is 16-bit. 512-sample
  frames make AirPods stop after about 0.1 s.
- **Pacing** (`aeld_source`). A 10 ms timer; each tick encodes the 10 ms frames that elapsed (at most
  eight), and a gap over 200 ms restarts with one frame rather than a burst. Before encoding, the
  frame goes through the shared Dolby stage (16-bit to Q8.23 and back).
- **Packets** (`aeld_packet`). Up to three AUs per RTP packet, each behind a 4-byte header
  (`0xb` + 12-bit sequence, `0x1` + 12-bit length; AUs over `0x7ff` bytes are refused); the
  sequence starts at 1, the RTP timestamp advances 480 per AU and the marker bit is used. With an
  AirPods MTU of 1011 (999 payload bytes) a packet holds two AUs. A packet the stack refuses (its
  queue flush on suspend) is dropped.
- **Recovery.** `feeding_reset` (stream start after suspend) reopens the encoder and restarts the
  sequence: AAC-ELD predicts across frames, so the decoder only recovers from a fresh encoder.
- **AirPods page** (`AirPodsSettingsBridge`). HyperOS' AirPods page is
  `plugin.settings.java.airpods.MiuiAirpodsFragment` in a Qigsaw split; its classes are hooked once
  the split's `SplitDexClassLoader` is created. `initSpatialAudio` hides the system `SpatialAudio`
  switch and adds a `miuix.preference.DropDownPreference` (off / fixed / head tracking), stored per
  device in Settings and sent to the Bluetooth process (`AIRPODS_SPATIAL`, sender-checked), which
  applies it when that device's AAC-ELD stream is configured. `updateSpatialAudio` and
  `AirPodsCodec.addPreferencesForSpecialCodec` (the AAC switch) are no-ops. In head tracking mode the
  Bluetooth process asks the AirPods tap for motion (`AIRPODS_TRACKING`). The Buds manager's 360
  audio switch is reapplied when an SSC stream is configured, since the spatial stage is shared.
- **AirPods motion** (`AirPodsChannelTap`, `AacpProtocol`, in `com.xiaomi.bluetooth`). AirPods serve
  one AACP channel (L2CAP PSM 0x1001), the most recently opened, and HyperOS' AirCore opens its own
  about 2 s after HFP connects; a second channel of ours would take the AirPods from HyperOS. So
  HyperOS' channel is tapped: its socket streams are wrapped (a BR/EDR L2CAP socket counts as AACP
  once AACP traffic is read; the PSM getter is a blocked hidden API), and 3 s after it opens the
  sensor session runs over its output: sensor interface setup, service query, the motion service
  found by name (`devmotion6`; service 16 on AirPods Pro 3), control session ownership only if the
  subscription gets no reports, then a 20 ms subscription. Sensor frames (opcode 0x17) and the
  replies to the setup are taken out of what HyperOS reads; its own features keep working.

## Head tracking (`core`: `HeadYaw`, `HeadTracker`, `PoseLink`)

The process that reads the earbuds (Buds manager, `com.xiaomi.bluetooth`) turns each orientation
sample into a head yaw and pitch and sends them to the Bluetooth process over a Binder sink
(`PoseLink`, sender-checked by package and calling UID), which sets the Dolby `headToStage` rotation
(Rz(yaw) * Rx(pitch), AOSP head frame: x right, y forward, z up).

- **Yaw**: the twist about world Z of the rotation since the reference. The sign depends on the
  earbud frame: Galaxy Buds -1 (and the opposite for the other role), AirPods +1.
- **Pitch**: the elevation against gravity of the head's forward axis in the sensor frame, measured
  from captures of turns, nods and tilts (the axis is perpendicular to the turn and nod axes, so it
  is horizontal for a level head): AirPods Pro 3 (-0.82, 0.54, 0.04), Galaxy Buds role true
  (0.30, -0.88, -0.37). Absolute, not recentered: the stage stays level in the room.
- **Galaxy Buds role false**: its world frame is not gravity aligned (its nod axis is 20 degrees off
  the horizontal, so nods leaked into the world-Z twist), so yaw and pitch are twists of the
  sensor-frame rotation about the measured turn (-0.93, -0.35, 0) and nod (0.03, -0.06, 1.0) axes;
  its pitch is relative and recentered.
- **Recentering** ports AOSP's head tracking processor (libheadtracking): the head is still when
  every orientation of the last 6 s is within 10.5 degrees of the latest (full 3D); then the current
  heading becomes forward and the output glides there at up to 0.8 rad/s, advanced every 20 ms by
  `HeadTracker` between slow samples (the Buds report about once a second while still).
