#include "mixer.h"
#include <stdlib.h>
#include <string.h>

struct mixer {
    int volumes[CHANNEL_MAX];
    bool muted[CHANNEL_MAX];
    volume_curve_t curve;
    bool ducking_enabled;
    int ducking_volume; // volume to duck to when active (0-100)
};

mixer_t* mixer_init(void) {
    mixer_t *mix = calloc(1, sizeof(mixer_t));
    if (!mix) return NULL;

    // Initialize volumes to 100 (max) and unmuted
    for (int i = 0; i < CHANNEL_MAX; i++) {
        mix->volumes[i] = 100;
        mix->muted[i] = false;
    }
    mix->curve = VOLUME_CURVE_LINEAR;
    mix->ducking_enabled = false;
    mix->ducking_volume = 30; // duck to 30% volume

    return mix;
}

int mixer_set_volume(mixer_t *mix, mixer_channel_t channel, int volume) {
    if (!mix || channel < 0 || channel >= CHANNEL_MAX) return -1;
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    mix->volumes[channel] = volume;
    return 0;
}

int mixer_get_volume(const mixer_t *mix, mixer_channel_t channel) {
    if (!mix || channel < 0 || channel >= CHANNEL_MAX) return -1;
    return mix->volumes[channel];
}

int mixer_set_mute(mixer_t *mix, mixer_channel_t channel, bool muted) {
    if (!mix || channel < 0 || channel >= CHANNEL_MAX) return -1;
    mix->muted[channel] = muted;
    return 0;
}

bool mixer_get_mute(const mixer_t *mix, mixer_channel_t channel) {
    if (!mix || channel < 0 || channel >= CHANNEL_MAX) return false;
    return mix->muted[channel];
}

int mixer_set_volume_curve(mixer_t *mix, volume_curve_t curve) {
    if (!mix) return -1;
    mix->curve = curve;
    return 0;
}

int mixer_set_ducking(mixer_t *mix, bool enable) {
    if (!mix) return -1;
    mix->ducking_enabled = enable;
    return 0;
}

void mixer_free(mixer_t *mix) {
    if (mix) free(mix);
}