# Third-party notices

This project is licensed under the GNU General Public License v3.0 (see `LICENSE`). It includes or
uses the following third-party components, under their own licenses.

## Included in this repository

### xz-embedded

`third_party/xz-embedded/`: xz-embedded v2024-12-30 (<https://tukaani.org/xz/embedded.html>),
unmodified. Licensed under the BSD Zero Clause License (0BSD); see `third_party/xz-embedded/COPYING`.
Used to decompress the `.gnu_debugdata` (MiniDebugInfo) section for runtime symbol resolution.

### AOSP audio effect AIDL and libbinder_ndk C++ helpers

`third_party/aosp-effect-aidl-ndk/`: Android Open Source Project, Apache License 2.0.

- `aidl/`: frozen AIDL API snapshots from `hardware/interfaces` and `system/hardware/interfaces`;
- `include/`, `src/`: generated from those snapshots by the AOSP `aidl` compiler (`generate.sh`);
- `binder_ndk_cpp/`: header-only helpers from `frameworks/native/libs/binder/ndk`.

The Apache License 2.0 text: <https://www.apache.org/licenses/LICENSE-2.0>. Copyright notices are kept
in the files.

### libopus (CELT)

`third_party/opus/`: libopus v1.3.1 (<https://opus-codec.org>), `celt/` and the public headers, with
the SSC format changes marked `SSC_FORMAT`. Copyright 2001-2011 Xiph.Org, Skype Limited, Octasic,
Jean-Marc Valin, Timothy B. Terriberry, CSIRO, Gregory Maxwell, Mark Borgerding, Erik de Castro Lopo.
BSD 3-Clause License, text in `third_party/opus/COPYING`. Built into the module's native library as
the open SSC encoder.

### AOSP libheadtracking (ported logic)

`core/src/main/java/.../core/HeadYaw.java` ports the recentering logic of AOSP's head tracking
processor (`frameworks/av/media/libheadtracking`: StillnessDetector, PoseBias, PoseRateLimiter) and
the parameters of `frameworks/av/services/audiopolicy/service/SpatializerPoseController.cpp` to Java,
for the head yaw and pitch. Android Open Source Project, Apache License 2.0
(<https://www.apache.org/licenses/LICENSE-2.0>).

## Protocol research (no code included)

The AirPods AACP and motion protocol facts (packet framing, the sensor setup, service discovery and
subscription, the motion report layout) come from the published research of LibrePods
(<https://github.com/librepods-org/librepods>),
[HyperPods-X](https://github.com/YeFeng233/HyperPods-X), aap-head-tracker and HeadTracker. The
implementation in `codec-aeld` is independent; none of their code is included.

## Distributed in the built APK

### DexKit

`org.luckypray:dexkit:2.3.0`, unmodified (<https://github.com/LuckyPray/DexKit/tree/2.3.0>). Its
Java/Kotlin part is licensed under the Apache License 2.0 and its native `Core/` under the GNU LGPL
3.0 (<https://github.com/LuckyPray/DexKit#license>). Used to locate obfuscated methods of the Galaxy
Buds manager.

## Build-time only (not distributed)

- libxposed API `io.github.libxposed:api:102.0.0` (`compileOnly`), Apache License 2.0.
- JUnit 4.13.2 (unit tests), Eclipse Public License 1.0.

## System components

The module runs inside, and works with, components of the Android system and the device's vendor
image that are not part of this repository (the Bluetooth stack, the audio HAL and its Dolby
Spatializer, Android Settings, the Galaxy Buds manager). Combining the Program with them is allowed by
the additional permission under GNU GPL version 3 section 7 stated in `README.md`.

`tools/ssc_reference/` is a development probe that runs a separately obtained copy of Samsung's SSC
encoder to produce reference streams; that library is not part of this repository, and the module
neither loads nor packages it.
