#!/usr/bin/env bash
# Builds dolby_hal_smoke, a standalone client of the device's Dolby Spatializer through the AIDL
# audio effect HAL. Run it on the device as root (shell may not reach hal_audio_service):
#   adb push build/dolby_hal_smoke /data/local/tmp/dhal/
#   adb shell su -c '/data/local/tmp/dhal/dolby_hal_smoke list'
#   adb shell su -c 'cd /data/local/tmp/dhal && ./dolby_hal_smoke run <impl-uuid> in.wav out.wav [rate=96000] [yaw=60]'
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"
ndk="${ANDROID_NDK_HOME:-$HOME/Library/Android/sdk/ndk/30.0.16138531}"
cxx="$(ls -d "$ndk"/toolchains/llvm/prebuilt/*/bin | head -1)/aarch64-linux-android37-clang++"
aidl="$root/third_party/aosp-effect-aidl-ndk"
mkdir -p "$here/build"
sources=()
while IFS= read -r file; do sources+=("$file"); done < <(find "$aidl/src" -name '*.cpp' | sort)
"$cxx" -std=c++20 -O2 -Wall -isystem "$aidl/include" -isystem "$aidl/binder_ndk_cpp" \
    "$here/dolby_hal_smoke.cpp" "${sources[@]}" -lbinder_ndk -ldl -static-libstdc++ \
    -o "$here/build/dolby_hal_smoke"
echo "$here/build/dolby_hal_smoke"
