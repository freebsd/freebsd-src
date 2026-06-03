/*
 * Audio Daemon - Audio management service
 * Handles mixer, PCM, volume, routing
 */

#ifndef _MOBILE_AUDIOD_H_
#define _MOBILE_AUDIOD_H_

#include <sys/types.h>

#define AUDIOD_SOCKET_PATH "/var/run/audiod.sock"

#define AUDIO_ROUTE_SPEAKER    0
#define AUDIO_ROUTE_HEADPHONE  1
#define AUDIO_ROUTE_BLUETOOTH 2

#define AUDIO_TYPE_MASTER     0
#define AUDIO_TYPE_MEDIA      1
#define AUDIO_TYPE_CALL       2
#define AUDIO_TYPE_NOTIFICATION 3

struct audio_device {
    int card;
    int device;
    char name[64];
    int active;
};

struct audio_volume {
    int type;
    int level; /* 0-100 */
    int muted;
};

int audiod_main(int argc, char **argv);
int audiod_init(void);
void audiod_shutdown(void);
int audiod_set_volume(int type, int level);
int audiod_get_volume(int type);
int audiod_mute(int type);
int audiod_unmute(int type);
int audiod_set_route(int route);
int audiod_pcm_open(int card, int device);
int audiod_pcm_close(int card, int device);
int audiod_pcm_write(int card, int device, const void *data, size_t len);

#endif /* _MOBILE_AUDIOD_H_ */