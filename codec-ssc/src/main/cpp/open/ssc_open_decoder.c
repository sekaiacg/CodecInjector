#include "opus_custom.h"
#include "ssc_format_rules.h"

typedef struct {
    OpusCustomDecoder *celt;
    int uhq;
} SscOpenDecoder;

#include <stdlib.h>

void *ssc_open_decoder_create(int32_t rate) {
    int err;
    SscOpenDecoder *d = calloc(1, sizeof(*d));
    if (!d) return NULL;
    d->uhq = rate == 96000;
    CELTMode *mode = ssc_mode(d->uhq);
    d->celt = mode ? opus_custom_decoder_create(mode, 2, &err) : NULL;
    if (!d->celt) { free(d); return NULL; }
    opus_custom_decoder_ctl(d->celt, CELT_SET_SIGNALLING(0));
    return d;
}

void ssc_open_decoder_destroy(void *decoder) {
    SscOpenDecoder *d = decoder;
    if (!d) return;
    opus_custom_decoder_destroy(d->celt);
    free(d);
}

int32_t ssc_open_decode(void *decoder, const uint8_t *frame, int32_t size, float *pcm) {
    SscOpenDecoder *d = decoder;
    SscRate rate;
    if (!d || size <= SSC_OPEN_HEADER || !ssc_rate_from_header(frame, &rate)) return -1;
    const int end = ssc_frame_setup(ssc_mode(d->uhq), d->uhq, &rate, frame[2] >> 2);
    opus_custom_decoder_ctl(d->celt, CELT_SET_END_BAND(end));
    return opus_custom_decode_float(d->celt, frame + SSC_OPEN_HEADER, size - SSC_OPEN_HEADER, pcm,
                                    SSC_OPEN_FRAME);
}
