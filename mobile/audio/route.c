#include "route.h"
#include <stdlib.h>
#include <string.h>

struct audio_route {
    audio_route_t active_route;
    bool auto_route_enabled;
    int a2dp_codec;
    int a2dp_bitrate;
    int hfp_codec;
};

audio_route_t* route_init(void) {
    audio_route_t *route = calloc(1, sizeof(audio_route_t));
    if (!route) return NULL;

    // Default route is speaker
    route->active_route = ROUTE_SPEAKER;
    route->auto_route_enabled = true;
    route->a2dp_codec = 0; // SBC
    route->a2dp_bitrate = 328; // default SBC bitrate
    route->hfp_codec = 0; // CVSD

    return route;
}

int route_set(audio_route_t *route, audio_route_t mode) {
    if (!route || mode >= ROUTE_MAX) return -1;
    route->active_route = mode;
    return 0;
}

audio_route_t route_get_active(const audio_route_t *route) {
    if (!route) return ROUTE_MAX;
    return route->active_route;
}

int route_set_auto(audio_route_t *route, bool enable) {
    if (!route) return -1;
    route->auto_route_enabled = enable;
    return 0;
}

int route_set_a2dp_codec(audio_route_t *route, int codec, int bitrate) {
    if (!route) return -1;
    if (codec < 0 || codec > 4) return -1; // SBC, AAC, LDAC, aptX, aptX HD
    if (bitrate <= 0) return -1;
    route->a2dp_codec = codec;
    route->a2dp_bitrate = bitrate;
    return 0;
}

int route_set_hfp_codec(audio_route_t *route, int codec) {
    if (!route) return -1;
    if (codec < 0 || codec > 1) return -1; // CVSD or mSBC
    route->hfp_codec = codec;
    return 0;
}

void route_free(audio_route_t *route) {
    if (route) free(route);
}