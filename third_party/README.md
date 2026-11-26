# Third-party source

| Directory | Source | License | Used for |
|---|---|---|---|
| `xz-embedded/` | xz-embedded v2024-12-30 (tukaani.org), unmodified | 0BSD | decompressing `.gnu_debugdata` (MiniDebugInfo) to resolve Bluetooth stack symbols at runtime |
| `opus/` | libopus v1.3.1 (opus-codec.org): `celt/` and the public headers; SSC changes marked `SSC_FORMAT` (24-bit range coder, no tapset symbol, per-frame TF/caps hooks, no time-domain transient detector), see `celt/ssc_format.h` | BSD-3-Clause (`opus/COPYING`) | the open SSC encoder (`codec-ssc/src/main/cpp/open`) |
| `aosp-effect-aidl-ndk/` | AOSP audio effect AIDL API snapshots + `aidl --lang=ndk` output + libbinder_ndk C++ helpers; see its README | Apache-2.0 | NDK client of the AIDL audio effect HAL (Dolby Spatializer) |

The Gradle build compiles them from here (`app/src/main/cpp/CMakeLists.txt`).
