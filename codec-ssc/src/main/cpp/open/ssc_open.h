/* Open SSC encoder: libopus CELT (third_party/opus, built with SSC_FORMAT) with the SSC format
   rules. Same entry points as Samsung's encoder library, so the module can use either. */
#ifndef SSC_OPEN_H
#define SSC_OPEN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { SSC_OPEN_FRAME = 864, SSC_OPEN_HEADER = 4 };

/* rate: 96000 for SSC UHQ, otherwise ordinary SSC (Samsung's stack passes 44100). Stereo only;
   bits must be 24. */
void *ssc_open_create(int32_t rate, int32_t channels, int32_t bits, int32_t *error);
void ssc_open_destroy(void *encoder);
/* pcm: SSC_OPEN_FRAME interleaved stereo int32 samples holding 24-bit values. Returns the frame
   size (4-byte header + payload), or -1 for an unknown bitrate or an encoder error. */
int32_t ssc_open_encode(void *encoder, const void *pcm, int32_t samples, uint8_t *out,
                        int32_t bitrate);

/* Decoder, for tests: returns SSC_OPEN_FRAME, or -1. Output: interleaved stereo float, +-1.0. */
void *ssc_open_decoder_create(int32_t rate);
void ssc_open_decoder_destroy(void *decoder);
int32_t ssc_open_decode(void *decoder, const uint8_t *frame, int32_t size, float *pcm);

#ifdef __cplusplus
}
#endif
#endif
