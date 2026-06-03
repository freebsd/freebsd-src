#ifndef _AUDIO_DSP_H_
#define _AUDIO_DSP_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct dsp dsp_t;

/**
 * Initialize DSP module
 * @return pointer to initialized dsp or NULL on failure
 */
dsp_t* dsp_init(void);

/**
 * Set parametric EQ band parameters
 * @param dsp dsp instance
 * @param band_idx band index (0-9 for 10 bands)
 * @param freq center frequency in Hz
 * @param gain gain in dB (-12 to +12)
 * @param Q quality factor (0.1 to 10.0)
 * @return 0 on success, -1 on failure
 */
int dsp_eq_band_freq(dsp_t *dsp, int band_idx, uint32_t freq, int gain, float Q);

/**
 * Enable/disable equalizer
 * @param dsp dsp instance
 * @param on true to enable, false to disable
 * @return 0 on success, -1 on failure
 */
int dsp_eq_enable(dsp_t *dsp, bool on);

/**
 * Set noise suppression strength
 * @param dsp dsp instance
 * @param strength 0-100 (0 = off, 100 = maximum suppression)
 * @return 0 on success, -1 on failure
 */
int dsp_noise_suppression(dsp_t *dsp, int strength);

/**
 * Enable/disable echo cancellation
 * @param dsp dsp instance
 * @param enable true to enable, false to disable
 * @return 0 on success, -1 on failure
 */
int dsp_echo_cancellation(dsp_t *dsp, bool enable);

/**
 * Configure compressor
 * @param dsp dsp instance
 * @param threshold threshold in dB (-60 to 0)
 * @param ratio compression ratio (e.g., 4 for 4:1)
 * @param attack attack time in ms
 * @param release release time in ms
 * @return 0 on success, -1 on failure
 */
int dsp_compressor(dsp_t *dsp, int threshold, int ratio, int attack, int release);

/**
 * Set bass boost gain
 * @param dsp dsp instance
 * @param gain gain in dB (0 to +12)
 * @return 0 on success, -1 on failure
 */
int dsp_bass_boost(dsp_t *dsp, int gain);

/**
 * Enable/disable surround virtualizer
 * @param dsp dsp instance
 * @param enable true to enable, false to disable
 * @return 0 on success, -1 on failure
 */
int dsp_surround_virtualizer(dsp_t *dsp, bool enable);

/**
 * Process audio frame through DSP
 * @param dsp dsp instance
 * @param buffer audio buffer (interleaved 16-bit for simplicity)
 * @param frames number of frames to process
 * @param channels number of channels (1,2,6,8)
 * @return 0 on success, -1 on failure
 */
int dsp_process(dsp_t *dsp, int16_t *buffer, uint32_t frames, int channels);

/**
 * Free DSP resources
 * @param dsp dsp instance to free
 */
void dsp_free(dsp_t *dsp);

#endif // _AUDIO_DSP_H_