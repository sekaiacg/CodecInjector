/* SSC = CELT (libopus, SSC_FORMAT build) with:
   - its own modes: 22 bands, 864-sample frames (LM 3), overlap 108, no pre-emphasis; static band,
     allocation, pulse cache and caps tables (ssc_tables.h) for 44.1/48 kHz and 96 kHz;
   - a 24-bit range coder and no postfilter tapset symbol (in third_party/opus);
   - per frame: no TF symbols on the high-rate path, the per-mode allocation table above
     192 kbps, the coded end band from the header;
   - a 4-byte frame header in front of the CBR payload. */
#ifndef SSC_FORMAT_RULES_H
#define SSC_FORMAT_RULES_H

#include <stdint.h>
#include "modes.h"
#include "ssc_open.h"

typedef struct {
    int32_t bitrate;
    int uhq_family; /* rate codes e0..e6, else f0..f6 */
    int code;       /* rate code, e.g. 0xe3 */
} SscRate;

CELTMode *ssc_mode(int uhq);
int ssc_rate_from_bitrate(int32_t bitrate, SscRate *rate);
int ssc_rate_from_header(const uint8_t *header, SscRate *rate);
/* Payload bytes after the header. */
int ssc_payload_bytes(int uhq, const SscRate *rate);
/* Applies the per-frame rules; returns the coded end band. lastband_row: UHQ header byte 2 >> 2. */
int ssc_frame_setup(CELTMode *mode, int uhq, const SscRate *rate, int lastband_row);
void ssc_write_header(uint8_t *header, const SscRate *rate, unsigned counter);
#endif
