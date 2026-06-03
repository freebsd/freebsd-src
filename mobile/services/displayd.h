/*
 * Display Daemon - Display management service
 * Handles framebuffer, compositor, input, and DPMS
 */

#ifndef _MOBILE_DISPLAYD_H_
#define _MOBILE_DISPLAYD_H_

#include <sys/types.h>

#define DISPLAYD_SOCKET_PATH "/var/run/displayd.sock"

#define DISPLAY_STATE_ON      1
#define DISPLAY_STATE_BLANK   2
#define DISPLAY_STATE_SUSPEND 3

#define BRIGHTNESS_MAX      255
#define BRIGHTNESS_DEFAULT  180

struct display_info {
    int width;
    int height;
    int bpp;
    int brightness;
    int dpms_state;
};

int displayd_main(int argc, char **argv);
int displayd_init(void);
void displayd_shutdown(void);
int displayd_set_brightness(int level);
int displayd_blank(void);
int displayd_unblank(void);
void displayd_hotplug_check(void);
void displayd_suspend(void);
void displayd_resume(void);

#endif /* _MOBILE_DISPLAYD_H_ */