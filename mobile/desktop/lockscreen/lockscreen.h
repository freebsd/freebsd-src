/*
 * Lock Screen for uOS(m) Desktop
 * Clock, swipe unlock, shortcuts, blur effect
 */

#ifndef _LOCKSCREEN_H_
#define _LOCKSCREEN_H_

#include <stdint.h>
#include "../ui/framebuffer.h"
#include "../ui/compositor/compositor_core.h"
#include "../ui/input.h"
#include "icon_theme.h"
#include "wallpaper.h"

#define LOCKSCREEN_HANDLE_HEIGHT 120
#define LOCKSCREEN_CLOCK_FONT_SIZE 72
#define LOCKSCREEN_DATE_FONT_SIZE 28

typedef enum {
    LOCK_METHOD_SWIPE,
    LOCK_METHOD_PIN,
    LOCK_METHOD_FINGERPRINT,
    LOCK_METHOD_NONE
} unlock_method_t;

typedef struct {
    int x, y;
    int width, height;
    int visible;
    int locked;
    int unlock_progress;
    unlock_method_t unlock_method;
    uint64_t unlock_start;
    int show_notifications;
    int notification_previews_hidden;
    int camera_shortcut;
    int flashlight_on;
    int emergency_mode;
    char emergency_number[16];
    surface_t *clock_surface;
    surface_t *date_surface;
    surface_t *handle_surface;
    surface_t *shortcut_surface;
    wallpaper_t *wallpaper;
} lockscreen_t;

int lockscreen_init(void);
void lockscreen_shutdown(void);
void lockscreen_show(void);
void lockscreen_hide(void);
void lockscreen_render(layer_t *ui_layer);
void lockscreen_update(void);
int lockscreen_handle_touch(int x, int y, int action);
void lockscreen_set_unlock_method(unlock_method_t method);
void lockscreen_set_camera_shortcut(int enabled);
void lockscreen_toggle_flashlight(void);
void lockscreen_set_emergency_number(const char *number);
lockscreen_t *lockscreen_get_instance(void);

#endif /* _LOCKSCREEN_H_ */
