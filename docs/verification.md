# Verification

## Before the device

```sh
scripts/host-tests.sh                   # SSC negotiation, codec state, frames, bitrate, packets, dispatch
./gradlew testDebugUnitTest assembleDebug
```

## Install

```sh
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb shell cmd bluetooth_manager disable && adb shell cmd bluetooth_manager enable
adb shell am force-stop com.samsung.accessory.budsunitemgr
```

## Logs

```sh
adb logcat -s SscPort SscPortBuds SscPortSettings
```

| Tag / stage | What it shows |
|---|---|
| `SscPort stage=module event=installed` | native hooks installed in the Bluetooth process |
| `stage=module event=symbol_missing` / `hook_failed` | a symbol was not found in this Bluetooth build |
| `event=ssc_encoder_init a=<mtu> b=<48000\|96000>`, `event=dolby_started` | an SSC session started, and its rate |
| `event=aeld_encoder_init a=<mtu> b=<payload capacity>`, `event=aeld_enqueue`, `event=aeld_fault` | an AAC-ELD session started, its first packets, a stopped stream |
| `event=ssc_bitrate a=<bps>` | the encoder bitrate changed (adaptive level or fixed mode) |
| `event=uhq_preferred`, `event=quality_mode` | the per-connection UHQ preference and the bitrate mode |
| `stage=dolby event=stats` | per 500 frames: `timeouts`, `write_full` and `bad` should be 0; `max_wait_us` stays well below a frame |
| `SscPortBuds stage=buds_settings event=installed / uhq_installed` | DexKit resolved the Buds manager's targets |
| `stage=buds_tap event=attach_sent / yaw` | the head tracking stream and its yaw |
| `SscPortSettings stage=settings_codec / settings_quality` | the Settings hooks and the inserted quality preference |

## Checklist

1. **SSC 48 kHz.** Connect the Buds and play. The Bluetooth device details page shows "SSC", and the
   developer options codec list shows SSC as selected.
2. **Spatial audio and head tracking.** In the Buds manager, turn 360 audio and head tracking on.
   Turn the head about 90° left: the sound stays in front, so it moves to the right ear.
   Turn Bluetooth off and on, reconnect, and check the direction again; this covers both buds' roles.
3. **UHQ.** Turn on "advanced sound quality" in the Buds manager. The stream renegotiates to 96 kHz
   (`ssc_encoder_init b=96000`), with sound, spatial audio and head tracking working.
   Reconnect: it returns at 96 kHz for this device only. Turn it off: back to 48 kHz.
4. **Playback quality.** In developer options, open "SSC playback quality". The entries follow the
   profile (584/291/152 or 192/128/88 kbps). A fixed mode changes `ssc_bitrate` at once, without a
   reconnect. After reconnecting, the mode is adaptive again.
5. **Stability.** Listen for clicks or dropouts in long playback, and check `stage=dolby event=stats`.

## Dolby HAL smoke test

`tools/dolby_hal_smoke` drives the Dolby Spatializer directly, without Bluetooth:

```sh
tools/dolby_hal_smoke/build.sh
adb shell mkdir -p /data/local/tmp/dhal
adb push tools/dolby_hal_smoke/build/dolby_hal_smoke /data/local/tmp/dhal/
adb shell su -c '/data/local/tmp/dhal/dolby_hal_smoke list'
adb shell su -c 'cd /data/local/tmp/dhal && ./dolby_hal_smoke run ccd4cf09-a79d-46c2-9aae-06a1698d6c8f in.wav out.wav rate=96000 yaw=60'
```

The input is a 44-byte-header float32 stereo WAV. The options are `rate=`, `yaw=<deg>`, `sweep`,
`ht=0|1`, `level=`, `in714` and `src=<ch>`.
