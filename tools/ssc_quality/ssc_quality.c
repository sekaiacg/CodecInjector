// Quantifies the SSC coding loss: encodes PCM with the open SSC encoder, decodes it with the open
// decoder and compares the result with the input.
//   ssc_quality <96000|48000> <bitrate> <in.s32le> [out.f32]
// Input: interleaved stereo int32 holding 24-bit samples (ffmpeg -f s32le, scaled by 256 below).
// Reports SNR (whole file and segmental, 20 ms windows) and the error per frequency band.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ssc_open.h"

enum { FFT = 16384 };

static void fft(double *re, double *im, int n) {
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { double t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (int len = 2; len <= n; len <<= 1) {
        double a = -2 * M_PI / len;
        for (int i = 0; i < n; i += len)
            for (int k = 0; k < len / 2; k++) {
                double wr = cos(a * k), wi = sin(a * k);
                double ur = re[i + k], ui = im[i + k];
                double vr = re[i + k + len / 2] * wr - im[i + k + len / 2] * wi;
                double vi = re[i + k + len / 2] * wi + im[i + k + len / 2] * wr;
                re[i + k] = ur + vr; im[i + k] = ui + vi;
                re[i + k + len / 2] = ur - vr; im[i + k + len / 2] = ui - vi;
            }
    }
}

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: ssc_quality rate bitrate in.s32le [out.f32]\n"); return 2; }
    /* 44100 selects the ordinary SSC encoder (as Samsung's stack does); the PCM is 48 kHz. */
    const int codec_rate = atoi(argv[1]), bitrate = atoi(argv[2]);
    const int rate = codec_rate == 96000 ? 96000 : 48000;
    FILE *in = fopen(argv[3], "rb");
    if (!in) { perror("in"); return 1; }
    fseek(in, 0, SEEK_END);
    long frames = ftell(in) / (8 * SSC_OPEN_FRAME);
    fseek(in, 0, SEEK_SET);
    const long n = frames * SSC_OPEN_FRAME;
    int32_t *pcm = malloc(sizeof(int32_t) * 2 * n);
    float *dec = malloc(sizeof(float) * 2 * n);
    double *ref = malloc(sizeof(double) * 2 * n);
    if (fread(pcm, 8, n, in) != (size_t)n) { fprintf(stderr, "short read\n"); return 1; }
    int32_t err;
    void *e = ssc_open_create(codec_rate, 2, 24, &err), *d = ssc_open_decoder_create(codec_rate);
    uint8_t frame[SSC_OPEN_HEADER + 1275];
    int frame_bytes = 0;
    FILE *dump = getenv("SSC_DUMP") ? fopen(getenv("SSC_DUMP"), "wb") : NULL;
    for (long f = 0; f < frames; f++) {
        int32_t *p = pcm + f * SSC_OPEN_FRAME * 2;
        for (int i = 0; i < SSC_OPEN_FRAME * 2; i++) {
            p[i] >>= 8; /* s32 -> 24-bit */
            ref[f * SSC_OPEN_FRAME * 2 + i] = p[i] / 8388608.0;
        }
        frame_bytes = ssc_open_encode(e, p, SSC_OPEN_FRAME, frame, bitrate);
        if (frame_bytes <= 0 || ssc_open_decode(d, frame, frame_bytes, dec + f * SSC_OPEN_FRAME * 2) != SSC_OPEN_FRAME) {
            fprintf(stderr, "codec error at frame %ld\n", f); return 1;
        }
        if (dump) { uint16_t len = (uint16_t)frame_bytes; fwrite(&len, 2, 1, dump); fwrite(frame, 1, len, dump); }
    }
    if (argc > 4) { FILE *o = fopen(argv[4], "wb"); fwrite(dec, sizeof(float), 2 * n, o); fclose(o); }
    const int delay = 108; /* CELT overlap */
    const long start = 4 * SSC_OPEN_FRAME, stop = n - delay - SSC_OPEN_FRAME;
    double sig = 0, noise = 0, peak = 0;
    double seg = 0; int segs = 0;
    const int win = rate / 50;
    double ws = 0, wn = 0; int wc = 0;
    double worst[5] = {1e9, 1e9, 1e9, 1e9, 1e9}; long worst_at[5] = {0};
    for (long i = start; i < stop; i++) {
        for (int c = 0; c < 2; c++) {
            double a = ref[2 * i + c], b = dec[2 * (i + delay) + c];
            sig += a * a; noise += (b - a) * (b - a); ws += a * a; wn += (b - a) * (b - a);
            if (fabs(a) > peak) peak = fabs(a);
        }
        if (++wc == win) {
            if (ws > 1e-9 * win) {
                const double v = 10 * log10(ws / (wn + 1e-20));
                seg += v; segs++;
                for (int k = 0; k < 5; k++)
                    if (v < worst[k]) {
                        for (int j = 4; j > k; j--) { worst[j] = worst[j - 1]; worst_at[j] = worst_at[j - 1]; }
                        worst[k] = v; worst_at[k] = i - win + 1;
                        break;
                    }
            }
            ws = wn = 0; wc = 0;
        }
    }
    printf("rate %d bitrate %d: %ld frames (%.1f s), %d bytes/frame\n", rate, bitrate, frames,
           (double)n / rate, frame_bytes);
    printf("  SNR %.2f dB, segmental SNR %.2f dB, error RMS %.1f dBFS (signal %.1f dBFS)\n",
           10 * log10(sig / noise), seg / (segs ? segs : 1), 10 * log10(noise / (2.0 * (stop - start))),
           10 * log10(sig / (2.0 * (stop - start))));
    /* Per 1/3 octave: input and output level (Hann-windowed power spectra, averaged), and the
       signal-to-error ratio. Levels are relative to the input's loudest band. */
    double *xr = malloc(sizeof(double) * FFT), *xi = malloc(sizeof(double) * FFT);
    double *yr = malloc(sizeof(double) * FFT), *yi = malloc(sizeof(double) * FFT);
    double *er = malloc(sizeof(double) * FFT), *ei = malloc(sizeof(double) * FFT);
    enum { BANDS = 40 };
    double lo[BANDS], hi[BANDS], px[BANDS] = {0}, py[BANDS] = {0}, pe[BANDS] = {0};
    int nb = 0;
    for (double c = 20.0; nb < BANDS && c * pow(2, 1.0 / 6) <= rate / 2.0 + 1; c *= pow(2, 1.0 / 3), nb++) {
        lo[nb] = c / pow(2, 1.0 / 6);
        hi[nb] = c * pow(2, 1.0 / 6);
    }
    double tx = 0, ty = 0;
    for (long pos = start; pos + FFT < stop; pos += FFT / 2)
        for (int c = 0; c < 2; c++) {
            for (int k = 0; k < FFT; k++) {
                double w = 0.5 - 0.5 * cos(2 * M_PI * k / FFT);
                double a = ref[2 * (pos + k) + c], b = dec[2 * (pos + k + delay) + c];
                xr[k] = a * w; yr[k] = b * w; er[k] = (b - a) * w; xi[k] = yi[k] = ei[k] = 0;
            }
            fft(xr, xi, FFT); fft(yr, yi, FFT); fft(er, ei, FFT);
            for (int k = 1; k < FFT / 2; k++) {
                const double f = (double)k * rate / FFT;
                const double X = xr[k] * xr[k] + xi[k] * xi[k], Y = yr[k] * yr[k] + yi[k] * yi[k];
                tx += X; ty += Y;
                for (int b = 0; b < nb; b++)
                    if (f >= lo[b] && f < hi[b]) { px[b] += X; py[b] += Y; pe[b] += er[k] * er[k] + ei[k] * ei[k]; }
            }
        }
    printf("  worst 20 ms windows:");
    for (int k = 0; k < 5; k++) printf(" %.3fs %.1fdB", (double)worst_at[k] / rate, worst[k]);
    printf("\n");
    printf("  loudness: output %+.2f dB vs input\n", 10 * log10(ty / tx));
    double mx = 0;
    for (int b = 0; b < nb; b++) if (px[b] > mx) mx = px[b];
    printf("  1/3 octave     input    output   output-input   signal/error\n");
    for (int b = 0; b < nb; b++) {
        const double c = sqrt(lo[b] * hi[b]);
        if (c >= 1000) printf("  %6.1f kHz", c / 1000); else printf("  %6.0f Hz ", c);
        printf("  %6.1f dB %6.1f dB   %+7.2f dB    %6.1f dB\n", 10 * log10(px[b] / mx + 1e-30),
               10 * log10(py[b] / mx + 1e-30), 10 * log10((py[b] + 1e-30) / (px[b] + 1e-30)),
               10 * log10((px[b] + 1e-30) / (pe[b] + 1e-30)));
    }
    return 0;
}
