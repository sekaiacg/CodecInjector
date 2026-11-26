#include <stdlib.h>

#include "opus_custom.h"
#include "ssc_format_rules.h"

typedef struct {
    OpusCustomEncoder *celt;
    int uhq;
    unsigned counter;
    float pcm[SSC_OPEN_FRAME * 2];
} SscOpenEncoder;

void *ssc_open_create(int32_t rate, int32_t channels, int32_t bits, int32_t *error) {
    int err = 0;
    SscOpenEncoder *e = NULL;
    CELTMode *mode = NULL;
    if (channels != 2 || bits != 24) { err = -1; goto done; }
    e = calloc(1, sizeof(*e));
    if (!e) { err = -7; goto done; }
    e->uhq = rate == 96000;
    mode = ssc_mode(e->uhq);
    if (mode) e->celt = opus_custom_encoder_create(mode, 2, &err);
    if (!e->celt) { free(e); e = NULL; if (!err) err = -7; goto done; }
    opus_custom_encoder_ctl(e->celt, CELT_SET_SIGNALLING(0));
    opus_custom_encoder_ctl(e->celt, OPUS_SET_VBR(0));
    opus_custom_encoder_ctl(e->celt, OPUS_SET_COMPLEXITY(10));
done:
    if (error) *error = err;
    return e;
}

void ssc_open_destroy(void *encoder) {
    SscOpenEncoder *e = encoder;
    if (!e) return;
    opus_custom_encoder_destroy(e->celt);
    free(e);
}

int32_t ssc_open_encode(void *encoder, const void *pcm, int32_t samples, uint8_t *out,
                        int32_t bitrate) {
    SscOpenEncoder *e = encoder;
    SscRate rate;
    if (!e || samples != SSC_OPEN_FRAME || !ssc_rate_from_bitrate(bitrate, &rate)) return -1;
    const int32_t *in = pcm;
    for (int i = 0; i < SSC_OPEN_FRAME * 2; i++) e->pcm[i] = (float)in[i] * (1.0f / 8388608.0f);
    CELTMode *mode = ssc_mode(e->uhq);
    const int end = ssc_frame_setup(mode, e->uhq, &rate, 0);
    opus_custom_encoder_ctl(e->celt, CELT_SET_END_BAND(end));
    const int payload = ssc_payload_bytes(e->uhq, &rate);
    if (opus_custom_encode_float(e->celt, e->pcm, SSC_OPEN_FRAME, out + SSC_OPEN_HEADER,
                                 payload) != payload)
        return -1;
    ssc_write_header(out, &rate, e->counter++);
    return SSC_OPEN_HEADER + payload;
}
