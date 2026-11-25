# Third-party source

| Directory | Source | License | Used for |
|---|---|---|---|
| `xz-embedded/` | xz-embedded v2024-12-30 (tukaani.org), unmodified | 0BSD | decompressing `.gnu_debugdata` (MiniDebugInfo) to resolve Bluetooth stack symbols at runtime |
| `aosp-effect-aidl-ndk/` | AOSP audio effect AIDL API snapshots + `aidl --lang=ndk` output + libbinder_ndk C++ helpers; see its README | Apache-2.0 | NDK client of the AIDL audio effect HAL (Dolby Spatializer) |

The Gradle build compiles both from here (`app/src/main/cpp/CMakeLists.txt`).
