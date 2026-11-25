#!/bin/bash
# Regenerates include/ and src/ from the frozen AIDL in aidl/ (Android SDK build-tools aidl).
set -e
cd "$(dirname "$0")"
A=${AIDL:-~/Library/Android/sdk/build-tools/37.0.0/aidl}
rm -rf include src; mkdir -p include src
INC=(-I aidl/effect3 -I aidl/types4 -I aidl/fmq1 -I aidl/common2 -I aidl/acommon4 -I aidl/eraser1)
for spec in effect3:3 types4:4 fmq1:1 common2:2 acommon4:4 eraser1:1; do
  dir=aidl/${spec%%:*}; ver=${spec##*:}; hash=$(tail -1 $dir/.hash)
  for f in $(find $dir -name '*.aidl'); do
    "$A" --lang=ndk --structured --stability=vintf --min_sdk_version=31 --version=$ver --hash=$hash \
      "${INC[@]}" -o src -h include "$f"
  done
done
echo "generated $(find src -name '*.cpp' | wc -l | tr -d ' ') sources"
