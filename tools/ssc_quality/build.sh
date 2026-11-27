#!/usr/bin/env bash
# Builds ssc_quality for the host from the module's open SSC encoder/decoder and third_party/opus.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"
opus="$root/third_party/opus"
open="$root/codec-ssc/src/main/cpp/open"
mkdir -p "$here/build"
clang -O2 -w -DOPUS_BUILD -DCUSTOM_MODES -DVAR_ARRAYS -DHAVE_LRINTF -DSSC_FORMAT \
    -I"$opus/include" -I"$opus/celt" -I"$open" "$opus"/celt/*.c "$open"/*.c "$here/ssc_quality.c" \
    -lm -o "$here/build/ssc_quality"
echo "$here/build/ssc_quality"
