# Open SSC encoder: coding loss benchmark

How much the open SSC encoder (`codec-ssc/src/main/cpp/open`) changes the signal, measured by
encoding PCM and decoding it again with the open decoder. The decoder reads Samsung's own SSC
streams symbol for symbol, so it stands in for the decoder in the earbuds.

## Method

- **Test signals.** Full-band test signals generated with ffmpeg, 10 s, stereo. The 48 kHz versions
  are resampled from the 96 kHz ones.

  ```sh
  ffmpeg -f lavfi -i "anoisesrc=color=white:sample_rate=96000:amplitude=0.25:duration=10:seed=1" -ac 2 -f s32le white96.s32
  ffmpeg -f lavfi -i "anoisesrc=color=pink:sample_rate=96000:amplitude=0.5:duration=10:seed=2" -ac 2 -f s32le pink96.s32
  ffmpeg -f lavfi -i "aevalsrc='0.3*sin(2*PI*20*10/log(47000/20)*(exp(t/10*log(47000/20))-1))':s=96000:d=10" -ac 2 -f s32le sweep96.s32
  ffmpeg -f s32le -ar 96000 -ac 2 -i white96.s32 -ar 48000 -f s32le white48.s32   # likewise pink, sweep
  ```

  - White noise has equal energy at every frequency.
  - Pink noise has equal energy per octave, closer to the spectrum of music.
  - The sweep goes logarithmically from 20 Hz to 47 kHz.
- **Encode and decode.** `tools/ssc_quality` encodes every 864-sample frame with the open encoder
  at a fixed bitrate (no adaptive ladder), then decodes it. The input passes through the 24-bit path
  the module uses. Each bitrate is run as `ssc_quality <96000|44100> <bitrate> <pcm>`; 44100 selects
  the ordinary (48 kHz) profile, as Samsung's stack does.
- **Comparison.** The output is compared with the input at the codec delay of 108 samples, and
  the first 4 frames are skipped. The tool reports:
  - SNR over the whole file, and segmental SNR in 20 ms windows;
  - the 5 worst windows;
  - the loudness change;
  - per 1/3 octave: input and output level, and the signal-to-error ratio. These use Hann-windowed
    16384-point spectra.

## Results

| Mode | Bitrate | Loudness | Band level deviation | Upper edge | SNR white | SNR pink | SNR sweep |
|---|---|---|---|---|---|---|---|
| UHQ 96 kHz | 584 kbps | 0.00 dB | ±0.01 dB | 48 kHz (full band) | 23.2 dB | 30.1 dB | 31.3 dB |
| UHQ 96 kHz | 442 kbps | 0.00 dB | +0.05 dB | 48 kHz | 12.9 dB | 21.1 dB | 30.5 dB |
| UHQ 96 kHz | 291 kbps | ±0.05 dB | +0.3 dB | 48 kHz | 2.4 dB | 10.4 dB | 30.1 dB |
| UHQ 96 kHz | 152 kbps | -0.5 to -4.8 dB | none above 16 kHz | 16 kHz | 1.7 dB | 9.2 dB | 8.6 dB |
| 48 kHz | 192 kbps | -0.1 dB | -0.4 dB (top band) | 22.7 kHz | 4.7 dB | 13.7 dB | 25.8 dB |
| 48 kHz | 128 kbps | -0.2 dB | -1.0 dB (top band) | 22.7 kHz | 4.4 dB | 13.0 dB | 25.4 dB |
| 48 kHz | 88 kbps | -0.2 dB | -1.0 dB (top band) | 22.7 kHz | 2.5 dB | 10.5 dB | 24.8 dB |

Column notes:
- **Band level deviation:** output minus input, over the 1/3 octaves where the input has signal.
- **Upper edge:** the highest frequency the stream carries.
- **SNR:** over the whole file. Segmental SNR matches it within 0.2 dB for the noise signals, so
  the error is spread evenly over time.
- **Loudness at 152 kbps:** the change depends on the signal: -0.5 dB pink, -0.6 dB sweep and
  -4.8 dB white, because the cut-off above 16 kHz removes a different share of each one's energy.

Pink noise, per 1/3 octave, at UHQ 584 kbps and 152 kbps:

| 1/3 octave | 584 kbps: output - input | 584 kbps: signal/error | 152 kbps: output - input | 152 kbps: signal/error |
|---|---|---|---|---|
| 25 Hz | -0.01 dB | 38.1 dB | +0.04 dB | 26.1 dB |
| 101 Hz | +0.00 dB | 36.4 dB | +0.02 dB | 24.3 dB |
| 1.0 kHz | +0.01 dB | 37.2 dB | -0.00 dB | 24.1 dB |
| 5.1 kHz | +0.00 dB | 42.8 dB | +0.01 dB | 21.7 dB |
| 10.2 kHz | +0.00 dB | 44.3 dB | +0.01 dB | 21.5 dB |
| 12.9 kHz | -0.00 dB | 43.0 dB | +0.01 dB | 20.9 dB |
| 16.3 kHz | +0.00 dB | 23.9 dB | -3.69 dB | 2.4 dB |
| 20.5 kHz | +0.00 dB | 21.5 dB | -71.2 dB | 0.0 dB |
| 32.5 kHz | +0.00 dB | 21.5 dB | -98.5 dB | 0.0 dB |
| 41.0 kHz | +0.00 dB | 21.4 dB | -103.3 dB | 0.0 dB |

## Findings

- **Loudness and frequency response are kept.**
  - At 291 to 584 kbps, every 1/3 octave from 25 Hz to 48 kHz stays within 0.3 dB of the input.
    There are no missing bands, ultrasonic content included.
  - The 48 kHz profile covers 22.7 kHz, with its top band 0.4 to 1 dB low.
- **152 kbps is band-limited to 16 kHz.** It codes 18 of the 22 bands; this is part of the format,
  and Samsung's encoder codes this rate the same way. It is the lowest step of the adaptive ladder,
  used only when the link is congested.
- **Accuracy rises with bitrate, and falls toward high frequencies and low bitrates.**
  - SSC, like Opus CELT, keeps each band's energy exactly but codes the fine structure with a
    limited number of pulses. Where pulses run out, it fills the band with noise of the right level.
    So the level stays right while the waveform does not. An example is the 20 kHz band at 48 kHz /
    192 kbps: its level is within 0.4 dB, but its signal/error is about 0 dB.
  - Waveform SNR is therefore a pessimistic measure for this codec, and white noise is its worst
    case. For comparison, stock Opus CELT (48 kHz, 20 ms frames) on a synthetic music signal reaches
    17.9 / 22.0 / 23.3 dB at 128 / 256 / 510 kbps.
- **Listening.** On Galaxy Buds4 Pro all six bitrates sounded normal.

## Bug found by this benchmark

The first run showed 16 broken frames out of every 256 in UHQ streams.
- **Cause:** UHQ frames carry the high nibble of the frame counter in byte 1, so counter values
  224 to 239 make byte 1 `0xEE`. That is the same value as an ordinary-SSC frame, and the open
  decoder took the profile from byte 1.
- **Fix:** it now takes it from the rate code in byte 3 (`f0..f6` vs `e0..e6`). `ssc_open_test`
  now covers the counter wrap.
- **What was affected:** only the open decoder, which the module does not ship. The encoder writes
  the header exactly as Samsung's encoder does, so the frames sent to the earbuds were always
  correct.

## Reproducing

```sh
tools/ssc_quality/build.sh
tools/ssc_quality/build/ssc_quality 96000 584000 pink96.s32            # UHQ
tools/ssc_quality/build/ssc_quality 44100 192000 pink48.s32            # ordinary SSC
SSC_DUMP=out.ssc tools/ssc_quality/build/ssc_quality 96000 291000 white96.s32 decoded.f32
```

Any audio file works as input once it is converted to `-f s32le -ac 2 -ar 96000` (or 48000).
