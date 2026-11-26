// Runs Samsung's SSC encoder (libScalable_Encoder.so) over raw PCM, to produce reference streams
// for the open SSC implementation. Development only; the library is not part of this repository.
//   ssc_reference <lib.so> <rate 48000|96000> <bitrate> <in.s32le> <out.ssc>
// Input: interleaved stereo signed 32-bit little-endian samples holding 24-bit values, as the
// module passes them. Output: per frame a 16-bit little-endian length, then the frame bytes.
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef void* (*Create)(int32_t, int32_t, int32_t, int32_t*);
typedef void (*Destroy)(void*);
typedef int32_t (*Encode)(void*, const void*, int32_t, uint8_t*, int32_t);

enum { kFrame = 864, kChannels = 2 };

int main(int argc, char** argv) {
    if (argc != 6) {
        fprintf(stderr, "usage: %s lib rate bitrate in out\n", argv[0]);
        return 2;
    }
    void* lib = dlopen(argv[1], RTLD_NOW);
    if (!lib) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
    Create create = (Create)dlsym(lib, "ssc_encoder_create");
    Destroy destroy = (Destroy)dlsym(lib, "ssc_encoder_destroy");
    Encode encode = (Encode)dlsym(lib, "ssc_encode");
    if (!create || !destroy || !encode) { fprintf(stderr, "symbols missing\n"); return 1; }
    const int rate = atoi(argv[2]);
    const int bitrate = atoi(argv[3]);
    int32_t error = 0;
    // The module (like Samsung's stack) creates the 48 kHz encoder with 44100.
    void* enc = create(rate == 96000 ? 96000 : 44100, kChannels, 24, &error);
    if (!enc || error) { fprintf(stderr, "create failed %d\n", error); return 1; }
    FILE* in = fopen(argv[4], "rb");
    FILE* out = fopen(argv[5], "wb");
    if (!in || !out) { perror("open"); return 1; }
    static int32_t pcm[kFrame * kChannels];
    static uint8_t frame[4096];
    int frames = 0;
    while (fread(pcm, sizeof(int32_t), kFrame * kChannels, in) == kFrame * kChannels) {
        const int32_t size = encode(enc, pcm, kFrame, frame, bitrate);
        if (size <= 0 || size > (int32_t)sizeof(frame)) {
            fprintf(stderr, "encode returned %d at frame %d\n", size, frames);
            return 1;
        }
        const uint16_t len = (uint16_t)size;
        fwrite(&len, sizeof(len), 1, out);
        fwrite(frame, 1, (size_t)size, out);
        ++frames;
    }
    fclose(out);
    fclose(in);
    destroy(enc);
    fprintf(stderr, "%d frames\n", frames);
    return 0;
}
