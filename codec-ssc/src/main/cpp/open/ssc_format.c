/* SSC format rules on top of CELT; see ssc_format_rules.h. */
#include "ssc_format_rules.h"

#include "ssc_tables.h"

void compute_pulse_cache(CELTMode *m, int LM);

int ssc_tf_off;
const int *ssc_caps;

static const int32_t kNormal[7] = {88000, 96000, 128000, 192000, 229000, 328000, 256000};
static const int32_t kUhq[7] = {291000, 308000, 442000, 584000, 886000, 250000, 152000};

static CELTMode *create_mode(int uhq) {
    int err;
    CELTMode *m = opus_custom_mode_create(48000, SSC_OPEN_FRAME, &err);
    if (!m) return NULL;
    /* No pre-emphasis in SSC. */
    m->preemph[0] = 0;
    m->preemph[1] = 0;
    m->preemph[2] = 1;
    m->preemph[3] = 1;
    m->nbEBands = 22;
    m->effEBands = 22;
    m->eBands = uhq ? ssc_eBands96 : ssc_eBands48;
    m->allocVectors = ssc_alloc;
    m->logN = uhq ? ssc_logN96 : ssc_logN48;
    compute_pulse_cache(m, 3);
    m->cache.index = uhq ? ssc_cache_index96 : ssc_cache_index48;
    m->cache.bits = uhq ? ssc_cache_bits96 : ssc_cache_bits48;
    m->cache.size = uhq ? SSC_CACHE_SIZE96 : SSC_CACHE_SIZE48;
    return m;
}

CELTMode *ssc_mode(int uhq) {
    /* One mode per profile for the life of the process: their tables are static. */
    static CELTMode *modes[2];
    if (!modes[uhq]) modes[uhq] = create_mode(uhq);
    return modes[uhq];
}

int ssc_rate_from_bitrate(int32_t bitrate, SscRate *rate) {
    for (int i = 0; i < 7; i++) {
        if (kNormal[i] == bitrate) {
            rate->bitrate = bitrate; rate->uhq_family = 0; rate->code = 0xf0 | i;
            return 1;
        }
        if (kUhq[i] == bitrate) {
            rate->bitrate = bitrate; rate->uhq_family = 1; rate->code = 0xe0 | i;
            return 1;
        }
    }
    return 0;
}

int ssc_rate_from_header(const uint8_t *h, SscRate *rate) {
    const int i = h[3] & 0x0f;
    if (h[0] != 0xff || i > 6) return 0;
    /* The family is in the rate code (bit 4: f0..f6 set, e0..e6 clear; UHQ frames keep a level in
       bits 5..7). Byte 1 cannot tell: UHQ frames put the counter's high nibble there, which is
       0xEE for counter values 224..239. */
    rate->uhq_family = !(h[3] & 0x10);
    rate->code = (rate->uhq_family ? 0xe0 : 0xf0) | i;
    rate->bitrate = rate->uhq_family ? kUhq[i] : kNormal[i];
    return 1;
}

/* The high-rate path of Samsung's encoder: all UHQ codes and 256/328 kbps (no TF symbols). */
static int high_rate_path(const SscRate *rate) {
    return rate->uhq_family || rate->code == 0xf5 || rate->code == 0xf6;
}

int ssc_payload_bytes(int uhq, const SscRate *rate) {
    /* Samsung's formula; the ordinary profile counts at 44.1 kHz. */
    const int64_t fs = uhq ? 96000 : 44100;
    int n = (int)((int64_t)rate->bitrate * SSC_OPEN_FRAME / (fs * 8));
    if (!high_rate_path(rate)) n += n * 3 >> 7;
    n += ~n & 1;
    n -= 1;
    return n > 1275 ? 1275 : n;
}

int ssc_frame_setup(CELTMode *m, int uhq, const SscRate *rate, int lastband_row) {
    ssc_tf_off = high_rate_path(rate);
    ssc_caps = uhq ? ssc_caps96 : ssc_caps48;
    m->allocVectors = rate->bitrate > 192000 ? (uhq ? ssc_alloc2_96 : ssc_alloc2_48) : ssc_alloc;
    if (!rate->uhq_family) return 22;
    if (rate->code == 0xe6) return 18;
    return 22 - (lastband_row & 3);
}

void ssc_write_header(uint8_t *h, const SscRate *rate, unsigned counter) {
    /* Full bandwidth: UHQ region 0 at level 7 (the rate code byte as is), ordinary SSC all four
       sub-bands of the last band (0x0c); bit 0 of byte 2 is always set. */
    h[0] = 0xff;
    h[1] = rate->uhq_family ? (uint8_t)(0xe0 | ((counter >> 4) & 0x0f)) : 0xee;
    h[2] = (uint8_t)(((counter & 0x0f) << 4) | 0x01 | (rate->uhq_family ? 0 : 0x0c));
    h[3] = (uint8_t)rate->code;
}
