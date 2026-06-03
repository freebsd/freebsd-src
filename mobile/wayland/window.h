/*
 * UOS Compositor - Window Management
 * BSD-licensed
 */

#ifndef _UOS_WINDOW_H_
#define _UOS_WINDOW_H_

#include <stdint.h>
#include <stdbool.h>
#include "compositor.h"

/* Window states */
#define WINDOW_STATE_NORMAL      0
#define WINDOW_STATE_MAXIMIZED   1
#define WINDOW_STATE_MINIMIZED   2
#define WINDOW_STATE_FULLSCREEN  4
#define WINDOW_STATE_RESIZING    8

/* Window */
typedef struct uos_window {
    struct wl_resource *xdg_toplevel;
    struct wl_resource *surface;
    int32_t x, y;
    int32_t width, height;
    int32_t saved_width, saved_height;
    int32_t saved_x, saved_y;
    uint32_t state;
    bool decorated;
    bool visible;
    bool activated;
    int32_t z_order;
    char title[256];
    char app_id[64];
    struct wl_list link;
    struct wl_list layer_link;
} uos_window_t;

/* Window public API */
uos_window_t *window_create(const char *title, int32_t w, int32_t h, const char *app_id);
void window_destroy(uos_window_t *window);
void window_set_state(uos_window_t *window, uint32_t state);
void window_set_title(uos_window_t *window, const char *title);
void window_set_app_id(uos_window_t *window, const char *app_id);
void window_set_geometry(uos_window_t *window, int32_t x, int32_t y, int32_t w, int32_t h);

/* Z-order management */
void window_raise(uos_window_t *window);
void window_lower(uos_window_t *window);
void window_stack_fullscreen(uos_window_t *window);
void window_stack_remove(uos_window_t *window);

/* Visibility */
void window_show(uos_window_t *window);
void window_hide(uos_window_t *window);
bool window_is_visible(uos_window_t *window);

#endif /* _UOS_WINDOW_H_ */