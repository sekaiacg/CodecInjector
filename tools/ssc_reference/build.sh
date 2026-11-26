#!/usr/bin/env bash
# Builds ssc_reference (and ssc_trace, its range-coder symbol trace) for arm64 Android (a device or an arm64 emulator):
#   adb push build/ssc_reference <libScalable_Encoder.so> /data/local/tmp/
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
ndk="${ANDROID_NDK_HOME:-$HOME/Library/Android/sdk/ndk/30.0.16138531}"
bin="$(ls -d "$ndk"/toolchains/llvm/prebuilt/*/bin | head -1)"
mkdir -p "$here/build"
"$bin/aarch64-linux-android31-clang" -O2 -Wall "$here/ssc_reference.c" -ldl \
    -o "$here/build/ssc_reference"
"$bin/aarch64-linux-android31-clang" -O2 -Wall -rdynamic "$here/ssc_trace.c" -ldl \
    -o "$here/build/ssc_trace"
echo "$here/build/ssc_reference $here/build/ssc_trace"
