/* Open SSC encoder: frame sizes and headers per bitrate, and an encode/decode round trip. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ssc_open.h"

static double round_trip(int rate, int bitrate, int expected_size) {
    int32_t err = 1;
    void *enc = ssc_open_create(rate, 2, 24, &err);
    void *dec = ssc_open_decoder_create(rate);
    assert(enc && err == 0 && dec);
    static int32_t pcm[SSC_OPEN_FRAME * 2];
    static float out[SSC_OPEN_FRAME * 2];
    uint8_t frame[SSC_OPEN_HEADER + 1275];
    const int frames = 40, delay = 108; /* CELT's overlap */
    static float in_f[40 * SSC_OPEN_FRAME * 2], out_f[40 * SSC_OPEN_FRAME * 2];
    long t = 0;
    for (int f = 0; f < frames; f++) {
        for (int i = 0; i < SSC_OPEN_FRAME; i++, t++) {
            const double l = 0.4 * sin(2 * M_PI * 440.0 * t / rate) + 0.1 * sin(2 * M_PI * 3100.0 * t / rate);
            const double r = 0.3 * sin(2 * M_PI * 660.0 * t / rate);
            pcm[2 * i] = (int32_t)(l * 8388607);
            pcm[2 * i + 1] = (int32_t)(r * 8388607);
            in_f[(f * SSC_OPEN_FRAME + i) * 2] = (float)l;
            in_f[(f * SSC_OPEN_FRAME + i) * 2 + 1] = (float)r;
        }
        const int32_t size = ssc_open_encode(enc, pcm, SSC_OPEN_FRAME, frame, bitrate);
        assert(size == expected_size);
        assert(frame[0] == 0xff);
        assert(ssc_open_decode(dec, frame, size, out) == SSC_OPEN_FRAME);
        memcpy(out_f + f * SSC_OPEN_FRAME * 2, out, sizeof(out));
    }
    /* SNR after the first frames, at the codec delay. */
    double sig = 0, err2 = 0;
    for (long i = 4 * SSC_OPEN_FRAME; i < (long)(frames - 1) * SSC_OPEN_FRAME; i++)
        for (int c = 0; c < 2; c++) {
            const double a = in_f[i * 2 + c], b = out_f[(i + delay) * 2 + c];
            sig += a * a;
            err2 += (b - a) * (b - a);
        }
    ssc_open_destroy(enc);
    ssc_open_decoder_destroy(dec);
    return 10 * log10(sig / err2);
}

int main(void) {
    int32_t err = 0;
    assert(ssc_open_create(96000, 1, 24, &err) == NULL && err != 0);
    void *enc = ssc_open_create(96000, 2, 24, &err);
    static int32_t silence[SSC_OPEN_FRAME * 2];
    uint8_t frame[SSC_OPEN_HEADER + 1275];
    assert(ssc_open_encode(enc, silence, SSC_OPEN_FRAME, frame, 123456) == -1); /* not a step */
    /* Header: FF, E0|counter>>4, counter<<4|flags, rate code. */
    assert(ssc_open_encode(enc, silence, SSC_OPEN_FRAME, frame, 584000) == 660);
    assert(frame[0] == 0xff && frame[1] == 0xe0 && frame[2] == 0x01 && frame[3] == 0xe3);
    assert(ssc_open_encode(enc, silence, SSC_OPEN_FRAME, frame, 584000) == 660);
    assert(frame[2] == 0x11);
    /* Counter 224..239 puts 0xEE in byte 1 of a UHQ frame, as in a 48 kHz frame: the decoder must
       take the family from the rate code. */
    void *dec = ssc_open_decoder_create(96000);
    static float out[SSC_OPEN_FRAME * 2];
    for (int i = 2; i < 240; i++) {
        assert(ssc_open_encode(enc, silence, SSC_OPEN_FRAME, frame, 584000) == 660);
        assert(ssc_open_decode(dec, frame, 660, out) == SSC_OPEN_FRAME);
    }
    assert(frame[1] == 0xee && frame[3] == 0xe3);
    ssc_open_decoder_destroy(dec);
    ssc_open_destroy(enc);
    /* Frame sizes match Samsung's encoder at each step. */
    const struct { int rate, bitrate, size; double min_snr; } cases[] = {
        {44100, 88000, 224, 20}, {44100, 128000, 324, 25}, {44100, 192000, 484, 28},
        {96000, 152000, 174, 20}, {96000, 291000, 330, 28}, {96000, 584000, 660, 30},
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        const double snr = round_trip(cases[i].rate, cases[i].bitrate, cases[i].size);
        printf("  %d Hz %d bps: %d bytes, %.1f dB\n", cases[i].rate, cases[i].bitrate, cases[i].size, snr);
        assert(snr > cases[i].min_snr);
    }
    return 0;
}
