#ifndef _AUDIO_ALSA_COMPAT_H_
#define _AUDIO_ALSA_COMPAT_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct alsa_card alsa_card_t;
typedef struct alsa_pcm alsa_pcm_t;
typedef struct alsa_mixer alsa_mixer_t;

/**
 * Open ALSA card
 * @param card_id card identifier (usually integer index)
 * @return pointer to card handle or NULL on failure
 */
alsa_card_t* alsa_open_card(int card_id);

/**
 * Close ALSA card
 * @param card card handle to close
 */
void alsa_close_card(alsa_card_t *card);

/**
 * Open PCM device on card
 * @param card card handle
 * @param pcm_id PCM identifier (e.g., "default", "hw:0,0")
 * @param stream stream direction (0 = playback, 1 = capture)
 * @return pointer to PCM handle or NULL on failure
 */
alsa_pcm_t* alsa_open_pcm(alsa_card_t *card, const char *pcm_id, int stream);

/**
 * Close PCM device
 * @param pcm PCM handle to close
 */
void alsa_close_pcm(alsa_pcm_t *pcm);

/**
 * Write audio data to PCM device
 * @param pcm PCM handle
 * @param buffer buffer containing audio data
 * @param frames number of frames to write
 * @return number of frames written on success, negative error code on failure
 */
int alsa_pcm_write(alsa_pcm_t *pcm, const void *buffer, uint32_t frames);

/**
 * Read audio data from PCM device
 * @param pcm PCM handle
 * @param buffer buffer to store audio data
 * @param frames number of frames to read
 * @return number of frames read on success, negative error code on failure
 */
int alsa_pcm_read(alsa_pcm_t *pcm, void *buffer, uint32_t frames);

/**
 * Prepare PCM device for I/O
 * @param pcm PCM handle
 * @return 0 on success, negative error code on failure
 */
int alsa_pcm_prepare(alsa_pcm_t *pcm);

/**
 * Drop PCM device (abort pending I/O)
 * @param pcm PCM handle
 * @return 0 on success, negative error code on failure
 */
int alsa_pcm_drop(alsa_pcm_t *pcm);

/**
 * Drain PCM device (wait for pending output to complete)
 * @param pcm PCM handle
 * @return 0 on success, negative error code on failure
 */
int alsa_pcm_drain(alsa_pcm_t *pcm);

/**
 * Open mixer device on card
 * @param card card handle
 * @return pointer to mixer handle or NULL on failure
 */
alsa_mixer_t* alsa_mixer_open(alsa_card_t *card);

/**
 * Close mixer device
 * @param mixer mixer handle to close
 */
void alsa_mixer_close(alsa_mixer_t *mixer);

/**
 * Set mixer control value
 * @param mixer mixer handle
 * @param elem name of mixer element (e.g., "Master", "PCM")
 * @param value value to set (0-100 for volume)
 * @return 0 on success, negative error code on failure
 */
int alsa_mixer_set(alsa_mixer_t *mixer, const char *elem, int value);

/**
 * Get mixer control value
 * @param mixer mixer handle
 * @param elem name of mixer element
 * @param value pointer to store value
 * @return 0 on success, negative error code on failure
 */
int alsa_mixer_get(alsa_mixer_t *mixer, const char *elem, int *value);

/**
 * Set hardware parameters for PCM device
 * @param pcm PCM handle
 * @param rate sample rate in Hz
 * @param format audio format (see below)
 * @param channels number of channels
 * @param buffer_size buffer size in frames
 * @param period_size period size in frames
 * @return 0 on success, negative error code on failure
 */
int alsa_pcm_set_hw_params(alsa_pcm_t *pcm, uint32_t rate, uint32_t format,
                          uint32_t channels, uint32_t buffer_size, uint32_t period_size);

/**
 * Audio format definitions (matching ALSA)
 */
typedef enum {
    FORMAT_S16_LE = 1,   /* Signed 16 bit Little Endian */
    FORMAT_S24_LE,       /* Signed 24 bit Little Endian */
    FORMAT_S32_LE,       /* Signed 32 bit Little Endian */
    FORMAT_FLOAT_LE      /* Float 32 bit Little Endian */
} alsa_format_t;

#endif // _AUDIO_ALSA_COMPAT_H_