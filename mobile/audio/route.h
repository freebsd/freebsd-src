#ifndef _AUDIO_ROUTE_H_
#define _AUDIO_ROUTE_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    ROUTE_SPEAKER,
    ROUTE_HEADPHONE,
    ROUTE_HEADSET,
    ROUTE_BLUETOOTH_A2DP,
    ROUTE_BLUETOOTH_HFP,
    ROUTE_HDMI,
    ROUTE_MAX
} audio_route_t;

typedef struct audio_route audio_route_t;

/**
 * Initialize audio routing subsystem
 * @return pointer to initialized route or NULL on failure
 */
audio_route_t* route_init(void);

/**
 * Set audio output route
 * @param route route instance
 * @param mode route to set
 * @return 0 on success, -1 on failure
 */
int route_set(audio_route_t *route, audio_route_t mode);

/**
 * Get currently active route
 * @param route route instance
 * @return current route or ROUTE_MAX on failure
 */
audio_route_t route_get_active(const audio_route_t *route);

/**
 * Enable/disable auto-routing (e.g., headphone insert -> switch to headphone)
 * @param route route instance
 * @param enable true to enable auto-routing
 * @return 0 on success, -1 on failure
 */
int route_set_auto(audio_route_t *route, bool enable);

/**
 * Configure A2DP codec
 * @param route route instance
 * @param codec codec to use (0=SBC,1=AAC,2=LDAC,3=aptX,4=aptX HD)
 * @param bitrate bitrate in kbps
 * @return 0 on success, -1 on failure
 */
int route_set_a2dp_codec(audio_route_t *route, int codec, int bitrate);

/**
 * Configure HFP/HSP codec
 * @param route route instance
 * @param codec codec to use (0=CVSD,1=mSBC)
 * @return 0 on success, -1 on failure
 */
int route_set_hfp_codec(audio_route_t *route, int codec);

/**
 * Free route resources
 * @param route route instance to free
 */
void route_free(audio_route_t *route);

#endif // _AUDIO_ROUTE_H_