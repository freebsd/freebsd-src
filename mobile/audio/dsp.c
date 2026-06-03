#include "dsp.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define EQ_BANDS 10

struct dsp {
    bool eq_enabled;
    struct {
        uint32_t freq;
        int gain;   // dB
        float Q;
    } eq_bands[EQ_BANDS];
    int noise_suppression; // 0-100
    bool echo_cancellation;
    int compressor_threshold; // dB
    int compressor_ratio;     // e.g., 4 for 4:1
    int compressor_attack;    // ms
    int compressor_release;   // ms
    int bass_boost_gain;      // dB
    bool surround_enabled;
};

dsp_t* dsp_init(void) {
    dsp_t *dsp = calloc(1, sizeof(dsp_t));
    if (!dsp) return NULL;

    // Initialize EQ bands to default frequencies (Hz): 32, 64, 125, 250, 500, 1k, 2k, 4k, 8k, 16k
    int freq_steps[EQ_BANDS] = {32, 64, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
    for (int i = 0; i < EQ_BANDS; i++) {
        dsp->eq_bands[i].freq = freq_steps[i];
        dsp->eq_bands[i].gain = 0;
        dsp->eq_bands[i].Q = 1.0f;
    }
    dsp->eq_enabled = false;
    dsp->noise_suppression = 0;
    dsp->echo_cancellation = false;
    dsp->compressor_threshold = -20; // dB
    dsp->compressor_ratio = 4;
    dsp->compressor_attack = 10; // ms
    dsp->compressor_release = 100; // ms
    dsp->bass_boost_gain = 0;
    dsp->surround_enabled = false;

    return dsp;
}

int dsp_eq_band_freq(dsp_t *dsp, int band_idx, uint32_t freq, int gain, float Q) {
    if (!dsp || band_idx < 0 || band_idx >= EQ_BANDS) return -1;
    if (gain < -12) gain = -12;
    if (gain > 12) gain = 12;
    if (Q < 0.1f) Q = 0.1f;
    if (Q > 10.0f) Q = 10.0f;
    dsp->eq_bands[band_idx].freq = freq;
    dsp->eq_bands[band_idx].gain = gain;
    dsp->eq_bands[band_idx].Q = Q;
    return 0;
}

int dsp_eq_enable(dsp_t *dsp, bool on) {
    if (!dsp) return -1;
    dsp->eq_enabled = on;
    return 0;
}

int dsp_noise_suppression(dsp_t *dsp, int strength) {
    if (!dsp) return -1;
    if (strength < 0) strength = 0;
    if (strength > 100) strength = 100;
    dsp->noise_suppression = strength;
    return 0;
}

int dsp_echo_cancellation(dsp_t *dsp, bool enable) {
    if (!dsp) return -1;
    dsp->echo_cancellation = enable;
    return 0;
}

int dsp_compressor(dsp_t *dsp, int threshold, int ratio, int attack, int release) {
    if (!dsp) return -1;
    if (threshold < -60) threshold = -60;
    if (threshold > 0) threshold = 0;
    if (ratio < 1) ratio = 1;
    if (attack < 0) attack = 0;
    if (release < 0) release = 0;
    dsp->compressor_threshold = threshold;
    dsp->compressor_ratio = ratio;
    dsp->compressor_attack = attack;
    dsp->compressor_release = release;
    return 0;
}

int dsp_bass_boost(dsp_t *dsp, int gain) {
    if (!dsp) return -1;
    if (gain < 0) gain = 0;
    if (gain > 12) gain = 12;
    dsp->bass_boost_gain = gain;
    return 0;
}

int dsp_surround_virtualizer(dsp_t *dsp, bool enable) {
    if (!dsp) return -1;
    dsp->surround_enabled = enable;
    return 0;
}

/**
 * Simple processing function - in reality this would be much more complex
 * For now, we just apply volume changes based on parameters
 */
int dsp_process(dsp_t *dsp, int16_t *buffer, uint32_t frames, int channels) {
    if (!dsp || !buffer || frames == 0) return -1;

    // Apply simple gain for demonstration
    float gain = 1.0f;
    if (dsp->eq_enabled) {
        // Very simplified: just add all gains together (not realistic)
        float eq_gain_db = 0;
        for (int i = 0; i < EQ_BANDS; i++) {
            eq_gain_db += dsp->eq_bands[i].gain;
        }
        gain *= powf(10.0f, eq_gain_db / 20.0f);
    }
    if (dsp->bass_boost_gain > 0) {
        gain *= powf(10.0f, dsp->bass_boost_gain / 20.0f);
    }
    // Noise suppression, echo cancellation, compressor, surround would be implemented here

    // Apply gain to each sample
    for (uint32_t i = 0; i < frames * channels; i++) {
        int32_t sample = buffer[i];
        int32_t processed = (int32_t)((float)sample * gain);
        // Clamp to int16 range
        if (processed > 32767) processed = 32767;
        if (processed < -32768) processed = -32768;
        buffer[i] = (int16_t)processed;
    }

    return 0;
}

void dsp_free(dsp_t *dsp) {
    if (dsp) free(dsp);
}