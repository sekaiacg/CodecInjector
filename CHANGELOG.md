# Changelog

## 1.0.4 (2026-11-26)

**Upgrade note: the package name changed, so LSPosed treats this as a new module.** After
installing, enable CodecInjector in LSPosed and tick its scope again. Also uninstall the old
`io.github.wxxsfxyzm.galaxybudsport`, or both modules will be active at once. Settings are kept.

- Renamed to CodecInjector throughout:
  - package: `io.github.wxxsfxyzm.codecinjector`;
  - entry class: `CodecInjectorModule`;
  - log tags: `CodecInjector`, `CodecInjectorBuds`, `CodecInjectorSettings`,
    `CodecInjectorAirPods`;
  - broadcast action prefixes and the native namespace renamed to match.
- Preference files renamed. The first time a renamed file is read, the old file's contents are
  moved over and the old file is deleted. This covers per-device UHQ, the Buds' 360 audio and head
  tracking switches, and the AirPods spatial audio mode.
- R8 now also shrinks DexKit's Java layer. The APK goes from 1.5 MB to 1.14 MB, and `classes.dex`
  from 515 KB to 133 KB.

## 1.0.3 (2026-11-26)

- Release builds are shrunk with R8, without obfuscation, so JNI names and log stack traces stay
  readable. The APK goes from 3.6 MB to 1.5 MB. Only the parts of the Kotlin standard library that
  DexKit actually uses are kept, and Kotlin metadata files are no longer packaged.
- The native library is renamed from `libgalaxybudsport.so` to `libcodecinjector.so`.

## 1.0.2 (2026-11-26)

- **Galaxy Buds3 Pro manager support** (`com.samsung.accessory.paranmgr`). Not yet verified with
  the earbuds.
  - The 360 audio, head tracking and UHQ switches all work.
  - The switch states are read from the protocol messages the manager sends to the earbuds, not
    from its obfuscated UI code.
  - Samsung's own sensor library stays off, so it doesn't conflict with this module's head
    tracking.
  - Head tracking data uses the same SPP protocol as the Buds4 Pro.
- Key events are also written to the LSPosed module log, so remote testers only need to export
  the LSPosed logs.
- Module description added (English and Chinese), shown in LSPosed's module list.

## 1.0.1 (2026-11-26)

The project is renamed from Galaxy Buds Port to CodecInjector and gains AirPods support.

**AirPods**

- **AAC-ELD encoding.** Uses the parameters Apple devices send to AirPods: 48 kHz stereo,
  480-sample frames, 265 kbps CBR, Apple's AU framing. Encoding uses the FDK AAC encoder already in
  the Bluetooth stack, so no extra library is needed.
- **Dolby spatial audio.** AirPods and Galaxy Buds share the same Dolby spatial audio processing.
- **AirPods head tracking.**
  - Motion data comes over HyperOS' own AirPods channel rather than a separate connection, so the
    system's battery and noise control features keep working.
  - The motion service is found by name. Verified on AirPods Pro 3.
- **HyperOS' AirPods settings page.**
  - The spatial audio switch is replaced by a drop-down: off, fixed, head tracking, saved per
    device.
  - The AAC switch is removed, because toggling it while AAC-ELD is active leaves the stream
    silent.

**Head tracking (Galaxy Buds and AirPods)**

- Recentering is ported from AOSP's head tracking processor: once the head has stayed within 10.5°
  for 6 seconds, that direction becomes the new forward, and the stage glides there at up to
  0.8 rad/s.
- Up/down (pitch) added. Pitch is measured against gravity: a level head is level, and the sound
  stage stays level in the room.
- One Galaxy Buds role reports a tilted world frame. That role now uses the measured turn and nod
  axes, so nodding no longer shifts the vocals sideways.

**Fixes and improvements**

- Fixed: the 96 kHz (UHQ) preference sometimes didn't apply when the earbuds started the
  connection.
- Fixed: the module stopped sending audio after the Bluetooth stack rejected AAC-ELD packets
  during a pause.
- AirPods and Galaxy Buds share the spatial audio processing: switching back to SSC re-applies the
  Buds manager's 360 audio switch.
- The code is split into `core` (shared parts), `codec-ssc` and `codec-aeld` modules, with `app`
  putting them together.
- Acknowledgements: protocol research from LibrePods, HyperPods-X, aap-head-tracker and HeadTracker
  (this project is an independent implementation); the recentering logic follows AOSP
  `libheadtracking` (Apache 2.0).

## 1.0.0 (2026-11-25)

First release, then called Galaxy Buds Port: Samsung SSC software encoding for Galaxy Buds on
HyperOS 4.

- **Encoding:** SSC 48 kHz and SSC UHQ 96 kHz, encoded on the CPU with Samsung's encoder library.
- **Bitrate:** adapts to the transmit queue, or can be fixed in developer options.
- **Dolby spatial audio:** the phone's own Dolby spatial audio processing (through the AIDL audio
  effect HAL), applied before encoding.
- **Galaxy Buds head tracking:** pose data is read from the Galaxy Buds manager's own SPP
  connection.
- **Galaxy Buds manager integration:** the manager's 360 audio, head tracking and UHQ ("advanced
  sound quality") switches control this module directly; UHQ is saved per device.
- **System Settings integration:** SSC appears in developer options' codec list, with an SSC
  playback quality setting, and the device details page shows the codec in use.
- **Compatibility:** Bluetooth stack functions are resolved by symbol name at runtime, and the
  manager's obfuscated methods are located with DexKit, so system and app updates usually need no
  re-adaptation.

## Known limitations

- Only Xiaomi HyperOS 4 (Android 17, arm64) is supported.
- Galaxy Buds3 Pro support is not yet verified with the earbuds. Head tracking axes for other
  models may need to be recaptured and calibrated.
- The release APK contains Samsung's closed-source encoder `libScalable_Encoder.so` and must not be
  published.
