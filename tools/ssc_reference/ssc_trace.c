// Symbol trace of Samsung's SSC encoder: this executable exports the range-coder entry points the
// library calls through its PLT, logs each call to stderr, and forwards it to the library.
//   ssc_trace <lib.so> <rate> <bitrate> <in.s32le> <frames>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef void* (*Create)(int32_t, int32_t, int32_t, int32_t*);
typedef int32_t (*Encode)(void*, const void*, int32_t, uint8_t*, int32_t);
static void* lib;
// ec_enc layout (libopus): nbits_total at 0x18, rng at 0x20.
static void state(void* e) {
    if (getenv("SSC_STATE")) fprintf(stderr, "   rng=%08x val=%08x offs=%u\n", *(unsigned*)((char*)e + 0x20),
            *(unsigned*)((char*)e + 0x24), *(unsigned*)((char*)e + 0x1c));
}
static int tell(void* e) {
    int nbits = *(int*)((char*)e + 0x18);
    unsigned rng = *(unsigned*)((char*)e + 0x20);
    return nbits - (32 - __builtin_clz(rng));
}
static int depth;
#define REAL(name) static void* r_##name; if (!r_##name) r_##name = dlsym(lib, #name)

void ssc_ec_encode(void* e, unsigned fl, unsigned fh, unsigned ft) {
    REAL(ssc_ec_encode);
    if (!depth) fprintf(stderr, "[%d] encode %u %u %u\n", tell(e), fl, fh, ft);
    ((void (*)(void*, unsigned, unsigned, unsigned))r_ssc_ec_encode)(e, fl, fh, ft);
    if (!depth) state(e);
}
void ssc_ec_encode_bin(void* e, unsigned fl, unsigned fh, unsigned bits) {
    REAL(ssc_ec_encode_bin);
    if (!depth) fprintf(stderr, "[%d] encode_bin %u %u %u\n", tell(e), fl, fh, bits);
    ((void (*)(void*, unsigned, unsigned, unsigned))r_ssc_ec_encode_bin)(e, fl, fh, bits);
    if (!depth) state(e);
}
void ssc_ec_enc_bit_logp(void* e, int val, unsigned logp) {
    REAL(ssc_ec_enc_bit_logp);
    if (!depth) fprintf(stderr, "[%d] bit_logp %d %u\n", tell(e), val, logp);
    ((void (*)(void*, int, unsigned))r_ssc_ec_enc_bit_logp)(e, val, logp);
    if (!depth) state(e);
}
void ssc_ec_enc_icdf(void* e, int s, const unsigned char* icdf, unsigned ftb) {
    REAL(ssc_ec_enc_icdf);
    if (!depth) fprintf(stderr, "[%d] icdf %d %u [%u %u %u]\n", tell(e), s, ftb, icdf[0], icdf[1], icdf[2]);
    ((void (*)(void*, int, const unsigned char*, unsigned))r_ssc_ec_enc_icdf)(e, s, icdf, ftb);
    if (!depth) state(e);
}
void ssc_ec_enc_uint(void* e, uint32_t fl, uint32_t ft) {
    REAL(ssc_ec_enc_uint);
    if (!depth) fprintf(stderr, "[%d] uint %u %u\n", tell(e), fl, ft);
    ++depth;
    ((void (*)(void*, uint32_t, uint32_t))r_ssc_ec_enc_uint)(e, fl, ft);
    --depth;
    state(e);
}
void ssc_ec_enc_bits(void* e, uint32_t fl, unsigned bits) {
    REAL(ssc_ec_enc_bits);
    if (!depth) fprintf(stderr, "[%d] bits %u %u\n", tell(e), fl, bits);
    ((void (*)(void*, uint32_t, unsigned))r_ssc_ec_enc_bits)(e, fl, bits);
    if (!depth) state(e);
}
void ssc_ec_laplace_encode(void* e, int* value, unsigned fs, int decay) {
    REAL(ssc_ec_laplace_encode);
    int before = *value;
    ++depth;
    ((void (*)(void*, int*, unsigned, int))r_ssc_ec_laplace_encode)(e, value, fs, decay);
    --depth;
    fprintf(stderr, "[%d] laplace %d->%d %u %d\n", tell(e), before, *value, fs, decay);
    state(e);
}
void ssc_encode_pulses(const int* y, int n, int k, void* e) {
    REAL(ssc_encode_pulses);
    fprintf(stderr, "[%d] pulses n=%d k=%d y=", tell(e), n, k);
    for (int i = 0; i < n; i++) fprintf(stderr, "%d ", y[i]);
    fprintf(stderr, "\n");
    ++depth;
    ((void (*)(const int*, int, int, void*))r_ssc_encode_pulses)(y, n, k, e);
    --depth;
}

void ssc_quant_all_bands(void* m, unsigned end, void* X, void* masks, void* bandE, int* pulses,
                         int shortBlocks, int spread, int dual, unsigned intensity, int* tf,
                         int total, int balance, void* e, int coded, void* seed) {
    REAL(ssc_quant_all_bands);
    fprintf(stderr, "qab end=%u dual=%d intensity=%u total=%d balance=%d coded=%d pulses=", end, dual,
            intensity, total, balance, coded);
    for (unsigned i = 0; i < end; i++) fprintf(stderr, "%d ", pulses[i]);
    fprintf(stderr, "\n");
    ((void (*)(void*, unsigned, void*, void*, void*, int*, int, int, int, unsigned, int*, int, int,
               void*, int, void*))r_ssc_quant_all_bands)(m, end, X, masks, bandE, pulses, shortBlocks,
            spread, dual, intensity, tf, total, balance, e, coded, seed);
}

int main(int argc, char** argv) {
    if (argc != 6) { fprintf(stderr, "usage: %s lib rate bitrate in frames\n", argv[0]); return 2; }
    lib = dlopen(argv[1], RTLD_NOW);
    if (!lib) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
    Create create = (Create)dlsym(lib, "ssc_encoder_create");
    Encode encode = (Encode)dlsym(lib, "ssc_encode");
    const int rate = atoi(argv[2]), bitrate = atoi(argv[3]), frames = atoi(argv[5]);
    int32_t error = 0;
    void* enc = create(rate == 96000 ? 96000 : 44100, 2, 24, &error);
    FILE* in = fopen(argv[4], "rb");
    static int32_t pcm[864 * 2];
    static uint8_t frame[4096];
    for (int f = 0; f < frames && fread(pcm, 4, 864 * 2, in) == 864 * 2; f++) {
        fprintf(stderr, "== frame %d\n", f);
        int32_t size = encode(enc, pcm, 864, frame, bitrate);
        fprintf(stderr, "== size %d header %02x %02x %02x %02x\n", size, frame[0], frame[1], frame[2], frame[3]);
    }
    return 0;
}
