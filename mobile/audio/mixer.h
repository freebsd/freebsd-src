#ifndef _AUDIO_MIXER_H_
#define _AUDIO_MIXER_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    CHANNEL_MASTER,
    CHANNEL_MUSIC,
    CHANNEL_VOICE_CALL,
    CHANNEL_NOTIFICATION,
    CHANNEL_ALARM,
    CHANNEL_SYSTEM,
    CHANNEL_MAX
} mixer_channel_t;

typedef enum {
    VOLUME_CURVE_LINEAR,
    VOLUME_CURVE_LOGARITHMIC,
    VOLUME_CURVE_EQUAL_POWER
} volume_curve_t;

typedef struct mixer mixer_t;

/**
 * Initialize mixer with N input channels (one per mixer_channel_t)
 * @return pointer to initialized mixer or NULL on failure
 */
mixer_t* mixer_init(void);

/**
 * Set volume for a specific channel (0-100)
 * @param mix mixer instance
 * @param channel channel to set
 * @param volume volume level (0-100)
 * @return 0 on success, -1 on failure
 */
int mixer_set_volume(mixer_t *mix, mixer_channel_t channel, int volume);

/**
 * Get volume for a specific channel
 * @param mix mixer instance
 * @param channel channel to get
 * @return volume level (0-100) or -1 on failure
 */
int mixer_get_volume(const mixer_t *mix, mixer_channel_t channel);

/**
 * Set mute state for a channel
 * @param mix mixer instance
 * @param channel channel to mute/unmute
 * @param muted true to mute, false to unmute
 * @return 0 on success, -1 on failure
 */
int mixer_set_mute(mixer_t *mix, mixer_channel_t channel, bool muted);

/**
 * Get mute state for a channel
 * @param mix mixer instance
 * @param channel channel to check
 * @return true if muted, false if not muted or -1 on failure
 */
bool mixer_get_mute(const mixer_t *mix, mixer_channel_t channel);

/**
 * Set volume curve type
 * @param mix mixer instance
 * @param curve curve type to use
 * @return 0 on success, -1 on failure
 */
int mixer_set_volume_curve(mixer_t *mix, volume_curve_t curve);

/**
 * Enable/disable audio ducking (lower music during voice call)
 * @param mix mixer instance
 * @param enable true to enable ducking
 * @return 0 on success, -1 on failure
 */
int mixer_set_ducking(mixer_t *mix, bool enable);

/**
 * Free mixer resources
 * @param mix mixer instance to free
 */
void mixer_free(mixer_t *mix);

#endif // _AUDIO_MIXER_H_