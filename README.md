# SSC Port

An LSPosed module that brings Samsung Seamless Codec (SSC, formerly Samsung Scalable Codec) to Xiaomi
phones running HyperOS 4 with Galaxy Buds, as a software encoder in the Bluetooth stack:

- **SSC 48 kHz and SSC UHQ 96 kHz.** The codec is registered as vendor codec index 18 with codec ID
  `0x00000001030075ff`. It is negotiated the way Samsung's stack does it and encoded on the CPU with
  Samsung's encoder library. Bitrate adapts to the transmit queue, or is fixed from developer options.
- **Spatial audio.** The PCM goes through the phone's own Dolby Spatializer, via the AIDL audio
  effect HAL, before encoding.
- **Head tracking.** Galaxy Buds pose data is read from the Galaxy Buds manager's own SPP session
  and turned into a head yaw for the spatializer.
- **Settings integration:**
  - The Galaxy Buds manager's 360 audio, head tracking and UHQ ("advanced sound quality") switches
    drive our implementation. UHQ is stored per device.
  - Android Settings lists SSC in developer options, adds an SSC playback quality setting, and labels
    the codec on the device details page.

## Supported devices

**Only Xiaomi HyperOS 4 (Android 17, API 37) on arm64 is supported**, with LSPosed (libxposed API 102).
Other ROMs, including other Android 17 builds, are not supported: the module relies on HyperOS
specifics, such as the Dolby Spatializer in Xiaomi's audio HAL, the audio HAL's handling of A2DP
software sessions, HyperOS Settings' developer options and device pages, and the MiniDebugInfo in
Xiaomi's Bluetooth stack.

Within HyperOS 4, nothing is tied to one build: Bluetooth stack targets are resolved by symbol name at
runtime from MiniDebugInfo, and the Galaxy Buds manager's obfuscated methods are located by DexKit,
so system and app updates do not need new offsets.

## Requirements

- `libScalable_Encoder.so` (Samsung's SSC encoder, arm64) in `app/src/main/jniLibs/arm64-v8a/`.
  It is not distributed with this repository and is excluded by `.gitignore`.
- Android SDK platform 37, NDK `30.0.16138531`, CMake 4.1.2.

## Build and install

```sh
./gradlew assembleDebug                 # app/build/outputs/apk/debug/app-debug.apk
./gradlew testDebugUnitTest             # Java unit tests
scripts/host-tests.sh                   # native unit tests on the host (clang, ASan/UBSan)
```

Enable the module in LSPosed with the scope `com.android.bluetooth`, `com.android.settings` and
`com.samsung.accessory.budsunitemgr`. Then restart Bluetooth, and restart the Buds manager.

## Keep Galaxy Wearable and the Galaxy Buds manager running

Head tracking, the 360 audio switch and the UHQ switch run inside the Galaxy Buds manager
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

## Layout

```
app/src/main/cpp/         native module (libsscport.so): Bluetooth stack hooks, SSC codec, encoder
                          pipeline, Dolby HAL stage, runtime symbol resolution
app/src/main/java/        LSPosed entry (SscPortModule) and the Buds / Settings bridges
app/src/test/             Java unit tests (head yaw, Buds protocol) and native host tests
hidden-api/               compile-only stubs of hidden framework APIs (not packaged)
tools/dolby_hal_smoke/    standalone Dolby Spatializer HAL client for on-device checks
third_party/              xz-embedded, AOSP audio effect AIDL NDK code (see third_party/README.md)
docs/                     architecture and verification
```

See [docs/architecture.md](docs/architecture.md) and [docs/verification.md](docs/verification.md).

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
supported by Samsung, Xiaomi, Dolby or Google. Samsung, Galaxy Buds, Xiaomi, HyperOS, Dolby and
Android are trademarks of their respective owners. Use it at your own risk.
