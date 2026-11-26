/* SSC format hooks (CodecInjector): SSC is CELT with a few changes, selected per frame by the
   SSC layer (codec-ssc/src/main/cpp/open). Only used when SSC_FORMAT is defined. */
#ifndef SSC_FORMAT_H
#define SSC_FORMAT_H
#ifdef SSC_FORMAT
/* Set per frame: no TF symbols, tf_res = 0 (the encoder's high-rate path). */
extern int ssc_tf_off;
/* Final per-band caps (stereo, LM 3) of the SSC mode in use, or NULL for CELT's. */
extern const int *ssc_caps;
#endif
#endif
