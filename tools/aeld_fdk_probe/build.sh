#!/usr/bin/env bash
# Builds aeld_fdk_probe. FDK_INCLUDE: directories holding the FDK AAC headers (aacenc_lib.h and the
# libSYS headers), e.g. from an fdk-aac 2.0.x source tree; only the headers are used.
#   adb push build/aeld_fdk_probe /data/local/tmp/ && adb shell su -c /data/local/tmp/aeld_fdk_probe
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"
: "${FDK_INCLUDE:?set FDK_INCLUDE to the fdk-aac source root}"
ndk="${ANDROID_NDK_HOME:-$HOME/Library/Android/sdk/ndk/30.0.16138531}"
bin="$(ls -d "$ndk"/toolchains/llvm/prebuilt/*/bin | head -1)"
xz="$root/third_party/xz-embedded"
mkdir -p "$here/build"
"$bin/aarch64-linux-android37-clang" -O2 -c -DXZ_USE_CRC64 -DXZ_DEC_ANY_CHECK \
    -I"$xz/linux/include/linux" -I"$xz/userspace" \
    "$xz"/linux/lib/xz/xz_crc32.c "$xz"/linux/lib/xz/xz_crc64.c \
    "$xz"/linux/lib/xz/xz_dec_stream.c "$xz"/linux/lib/xz/xz_dec_lzma2.c
mv xz_*.o "$here/build/"
"$bin/aarch64-linux-android37-clang++" -std=c++20 -O2 -Wall -DXZ_USE_CRC64 -DXZ_DEC_ANY_CHECK \
    -I"$root/core/src/main/cpp" -I"$xz/linux/include/linux" \
    -I"$FDK_INCLUDE/libAACenc/include" -I"$FDK_INCLUDE/libSYS/include" \
    "$here/aeld_fdk_probe.cpp" "$root/core/src/main/cpp/elf_symbols.cpp" "$here"/build/xz_*.o \
    -ldl -static-libstdc++ -o "$here/build/aeld_fdk_probe"
echo "$here/build/aeld_fdk_probe"
