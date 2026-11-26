# CodecInjector

An LSPosed module for Xiaomi phones running HyperOS 4 that injects vendor Bluetooth codecs into the
system Bluetooth stack as software encoders, with spatial audio rendered by the phone's own Dolby
Spatializer and head tracking driven by the earbuds:

| Earbuds | Codec | Spatial audio | Head tracking |
|---|---|---|---|
| Samsung Galaxy Buds | SSC 48 kHz, SSC UHQ 96 kHz | Dolby Spatializer | turn and nod, from the Galaxy Buds manager's session |
| Apple AirPods | AAC-ELD 48 kHz | Dolby Spatializer | turn and nod, over HyperOS' own AirPods channel |

Tested with Galaxy Buds4 Pro and AirPods Pro 3.

## Features

### Samsung Seamless Codec (Galaxy Buds)

- **SSC 48 kHz and SSC UHQ 96 kHz.** Registered as vendor codec index 18 with codec ID
  `0x00000001030075ff`, negotiated the way Samsung's stack does it and encoded on the CPU. The
  bitrate adapts to the transmit queue, or is fixed from developer options.
- **Open SSC encoder.** SSC is CELT (the music codec of Opus) with a few changes: its own 22-band
  modes, a 24-bit range coder, no post-filter tapset, per-frame TF and allocation rules and a 4-byte
  frame header. The module carries an open implementation of it, built on libopus
  (`third_party/opus`, `codec-ssc/src/main/cpp/open`), and uses it unless Samsung's encoder library is
  packaged. Its streams decode with an open decoder that reads Samsung's streams symbol for symbol;
  it is not yet verified with the earbuds.
- **Galaxy Buds manager integration.** Its 360 audio, head tracking and UHQ ("advanced sound
  quality") switches drive this implementation; UHQ is stored per device.

### Apple AAC-ELD (AirPods)

- **AAC-ELD.** Apple's AAC-ELD (vendor `0x004c`, codec `0x8001`) is registered as vendor codec index
  17 with codec ID `0x0000008001004cff`, with the profile Apple sources send (48 kHz stereo,
  480-sample frames, 265 kbps CBR, Apple's AU framing). It is encoded with the FDK AAC encoder that
  the Bluetooth stack already contains.
- **HyperOS' AirPods page.** The system spatial audio switch is replaced by a drop-down for ours
  (off, fixed, head tracking; per device), and the AAC switch is removed (switching it with AAC-ELD
  active leaves the stream silent).

### Spatial audio and head tracking

- **Rendering.** Before encoding, the PCM goes through the phone's Dolby Spatializer, used through
  the AIDL audio effect HAL as the system Spatializer uses it.
- **Head pose.** The earbuds' orientation becomes a head yaw (turning) and pitch (nodding):
  - Galaxy Buds: read from the Galaxy Buds manager's own SPP session.
  - AirPods: the motion service is subscribed over the AACP channel HyperOS' Bluetooth service
    (`com.xiaomi.bluetooth`) already holds, so its own AirPods features keep working.
- **Recentering** follows AOSP's head tracking processor: once the head has been still for 6 s
  (within 10.5 degrees), that direction becomes forward and the stage glides there at up to
  0.8 rad/s. Pitch is measured against gravity where the earbud's frame allows it, so the stage stays
  level in the room.

### Android Settings

- Developer options list SSC and AAC-ELD as Bluetooth codecs, with an SSC playback quality setting.
- The device details page labels the codec in use.

## Supported devices

**Only Xiaomi HyperOS 4 (Android 17, API 37) on arm64 is supported**, with LSPosed (libxposed API 102).
Other ROMs, including other Android 17 builds, are not supported: the module relies on HyperOS
specifics, such as the Dolby Spatializer in Xiaomi's audio HAL, the audio HAL's handling of A2DP
software sessions, HyperOS Settings' developer options, device and AirPods pages, HyperOS' AirPods
service, and the MiniDebugInfo in Xiaomi's Bluetooth stack.

Within HyperOS 4, nothing is tied to one build: Bluetooth stack targets (including the FDK encoder)
are resolved by symbol name at runtime from MiniDebugInfo, the Galaxy Buds manager's obfuscated
methods are located by DexKit, and the AirPods motion service is found by name, so system and app
updates do not need new offsets.

## Requirements

- Nothing extra for SSC or AAC-ELD. Optionally, Samsung's SSC encoder (`libScalable_Encoder.so`,
  arm64) in `app/src/main/jniLibs/arm64-v8a/` is used instead of the open encoder; it is not
  distributed with this repository and is excluded by `.gitignore`.
- Android SDK platform 37, NDK `30.0.16138531`, CMake 4.1.2.

## Build and install

```sh
./gradlew assembleDebug                 # app/build/outputs/apk/debug/app-debug.apk
./gradlew testDebugUnitTest             # Java unit tests
scripts/host-tests.sh                   # native unit tests on the host (clang, ASan/UBSan)
```

Enable the module in LSPosed with the scope:

| Package | For |
|---|---|
| `com.android.bluetooth` | the codecs, Dolby spatial audio |
| `com.android.settings` | codec list, quality setting, device label, AirPods page |
| `com.samsung.accessory.budsunitemgr` | Galaxy Buds4 / Buds4 Pro manager: head tracking and switches |
| `com.samsung.accessory.paranmgr` | Galaxy Buds3 Pro manager: head tracking and switches (not yet verified with the earbuds) |
| `com.xiaomi.bluetooth` | AirPods head tracking |

Then restart Bluetooth, the Galaxy Buds manager and `com.xiaomi.bluetooth` (or reboot).

## Keep Galaxy Wearable and the Galaxy Buds manager running

Galaxy Buds head tracking, the 360 audio switch and the UHQ switch run inside the Galaxy Buds manager
(`com.samsung.accessory.budsunitemgr`), which needs Galaxy Wearable to stay alive. One UI keeps these
apps alive in the background; HyperOS does not, and kills them soon after they leave the foreground.
When the manager is killed, head tracking stops until it starts again. SSC audio itself, Dolby
spatial audio and the UHQ setting already applied keep working, since they run in the Bluetooth
process.

So, for **both Galaxy Wearable and Galaxy Buds**:

1. Settings → Apps → Manage apps → the app → turn **Autostart** on.
2. In the same page → Battery saver → **No restrictions**.
3. Optionally lock the app in the recent apps screen.

Or keep them alive by any other means you prefer (for example a background-keeping module). When
the manager does restart, the module restores the last 360 audio and head tracking switches and
re-attaches head tracking by itself.

AirPods need nothing like this: HyperOS' own AirPods service is a system app that stays running.

## Layout

```
core/                     codec-independent core
  src/main/cpp/           Bluetooth stack hooks, codec plugin registry, Dolby HAL stage, HAL
                          session restart, runtime symbol resolution
  src/main/java/          PortRuntime, BluetoothHost, framework codec types, Settings codec list,
                          head tracking (HeadYaw, HeadTracker, PoseLink)
codec-ssc/                SSC with Galaxy Buds: codec plugin, encoder pipeline, Buds manager and
                          Settings quality bridges, Buds pose tap
codec-aeld/               AAC-ELD for AirPods: codec plugin, FDK encoder, AU/RTP packetizer, AACP
                          motion session, AirPods settings page
app/                      LSPosed entry and the native library linking the core and the codecs
scripts/host-tests.sh     native host tests of all modules (src/test/cpp)
hidden-api/               compile-only stubs of hidden framework APIs (not packaged)
tools/                    on-device probes (Dolby HAL client, FDK AAC-ELD encoder)
third_party/              xz-embedded, AOSP audio effect AIDL NDK code (see third_party/README.md)
docs/                     architecture and verification
```

See [docs/architecture.md](docs/architecture.md) and [docs/verification.md](docs/verification.md).

## Acknowledgements

- The AirPods AACP and motion protocol facts come from the research of
  [LibrePods](https://github.com/librepods-org/librepods),
  [HyperPods-X](https://github.com/YeFeng233/HyperPods-X), aap-head-tracker and HeadTracker.
  This project's implementation is its own.
- The recentering logic follows AOSP's `libheadtracking` (Apache License 2.0); see
  [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## License

Copyright (C) 2026 wxxsfxyzm

This program is free software: you can redistribute it and/or modify it under the terms of the GNU
General Public License as published by the Free Software Foundation, either version 3 of the License,
or (at your option) any later version. It is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See [LICENSE](LICENSE) for details, and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for
the third-party components.

Samsung's SSC encoder library is not included and is not covered by this license.

### Additional permission under GNU GPL version 3 section 7

If you modify this Program, or any covered work, by linking or combining it with Samsung's SSC
encoder library (`libScalable_Encoder.so`), or with libraries, services and applications that are
part of the Android system or the device vendor's system image or that the Program runs inside of
(including the Bluetooth stack, the audio HAL and its audio effects such as the Dolby Spatializer,
Android Settings and the Galaxy Buds manager application), or with a modified version of any of
those, containing parts covered by the terms of their own licenses, the licensors of this Program
grant you additional permission to convey the resulting work. Corresponding Source for a non-source
form of such a combination need not include the source code of those libraries, services and
applications; it shall include the source code for the parts of this Program used as well as that of
the covered work.

## Disclaimer

This is an independent project for interoperability. It is not affiliated with, endorsed by or
supported by Samsung, Apple, Xiaomi, Dolby or Google. Samsung, Galaxy Buds, Apple, AirPods, Xiaomi,
HyperOS, Dolby and Android are trademarks of their respective owners. Use it at your own risk.
