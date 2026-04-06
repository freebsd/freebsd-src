/*
 * Mobile Window Manager
 * uOS(m) - User OS Mobile
 */

#ifndef _WINDOW_MANAGER_H_
#define _WINDOW_MANAGER_H_

#include <stdint.h>
#include "ui_widget.h"

#define MAX_WINDOWS 32

/* Window structure */
typedef struct window {
    uint32_t id;
    char title[64];

    int x, y;
    int width, height;
    int z_order;

    widget_t *root_widget;
    uint32_t bg_color;

    /* Window state */
    int visible;
    int focused;
    int minimized;

    /* Callbacks */
    void (*on_close)(struct window *window);
    void (*on_minimize)(struct window *window);
    void (*on_focus)(struct window *window);

    /* Linked list */
    struct window *next;
} window_t;

/* Initialize window manager */
int wm_init(void);

/* Window management */
window_t *wm_create_window(const char *title, int x, int y, int width, int height);
void wm_destroy_window(window_t *window);
void wm_show_window(window_t *window);
void wm_hide_window(window_t *window);
void wm_focus_window(window_t *window);
void wm_minimize_window(window_t *window);

/* Window operations */
void wm_move_window(window_t *window, int x, int y);
void wm_resize_window(window_t *window, int width, int height);
void wm_set_window_title(window_t *window, const char *title);

/* Widget management in windows */
void wm_add_widget_to_window(window_t *window, widget_t *widget);
void wm_remove_widget_from_window(window_t *window, widget_t *widget);

/* Event handling */
void wm_handle_touch(int x, int y, touch_action_t action);

/* Rendering */
void wm_render_all(void);

/* Window utilities */
window_t *wm_get_window_at(int x, int y);
window_t *wm_get_focused_window(void);
int wm_get_window_count(void);

#endif /* _WINDOW_MANAGER_H_ */