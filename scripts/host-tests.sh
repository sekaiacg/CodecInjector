#!/usr/bin/env bash
# Builds and runs the native unit tests on the host (clang, ASan/UBSan): the core (codec registry,
# factory, registration, vendor dispatch) and each codec module.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
core="$root/core/src/main/cpp"
ssc="$root/codec-ssc/src/main/cpp"
aeld="$root/codec-aeld/src/main/cpp"
out="${TMPDIR:-/tmp}/codecinjector-host-tests"
mkdir -p "$out"
flags=(-std=c++20 -g -Wall -Wextra -Werror -fno-exceptions -fno-rtti -fsanitize=address,undefined)
core_sources=("$core"/codec_registry.cpp "$core"/factory.cpp "$core"/registration.cpp
              "$core"/vendor_dispatch.cpp)
ssc_sources=()
for name in ssc_frame ssc_bitrate ssc_packet ssc_interface ssc_negotiation ssc_codec_state ssc_codec_table; do
    ssc_sources+=("$ssc/$name.cpp")
done
aeld_sources=()
for name in aeld_codec_info aeld_codec_state aeld_packet aeld_source; do
    aeld_sources+=("$aeld/$name.cpp")
done
failed=0
run() {
    local test="$1"; shift
    local name; name="$(basename "$test" .cpp)"
    clang++ "${flags[@]}" "$@" "$test" -o "$out/$name"
    if "$out/$name"; then echo "PASS $name"; else echo "FAIL $name"; failed=1; fi
}
for test in "$root"/core/src/test/cpp/*_test.cpp; do
    run "$test" -I"$core" "${core_sources[@]}"
done
for test in "$root"/codec-ssc/src/test/cpp/*_test.cpp; do
    run "$test" -I"$core" -I"$ssc" "${core_sources[@]}" "${ssc_sources[@]}"
done
for test in "$root"/codec-aeld/src/test/cpp/*_test.cpp; do
    run "$test" -I"$core" -I"$aeld" "${core_sources[@]}" "${aeld_sources[@]}"
done
exit "$failed"
