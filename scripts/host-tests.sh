#!/usr/bin/env bash
# Builds and runs the native unit tests on the host (clang, ASan/UBSan).
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
src="$root/app/src/main/cpp"
tests="$root/app/src/test/cpp"
out="${TMPDIR:-/tmp}/sscport-host-tests"
mkdir -p "$out"
lib=(ssc_frame ssc_bitrate ssc_packet ssc_interface ssc_negotiation ssc_codec_state
     ssc_codec_table ssc_vendor_dispatch ssc_factory ssc_registration)
sources=()
for name in "${lib[@]}"; do sources+=("$src/$name.cpp"); done
failed=0
for test in "$tests"/ssc_*_test.cpp; do
    name="$(basename "$test" .cpp)"
    clang++ -std=c++20 -g -Wall -Wextra -Werror -fno-exceptions -fno-rtti \
        -fsanitize=address,undefined -I"$src" "$test" "${sources[@]}" -o "$out/$name"
    if "$out/$name"; then echo "PASS $name"; else echo "FAIL $name"; failed=1; fi
done
exit "$failed"
