/*
 * Window Manager - Enhanced window management
 * BSD-licensed
 */

#ifndef _WINDOW_MGR_H_
#define _WINDOW_MGR_H_

#include <stdint.h>
#include "../framebuffer.h"
#include "../input.h"

#define MAX_WINDOWS 32

#define WIN_STATE_NORMAL     0
#define WIN_STATE_MAXIMIZED  1
#define WIN_STATE_MINIMIZED  2
#define WIN_STATE_FULLSCREEN 3

typedef struct {
    int x, y;
    int width, height;
} rect_t;

typedef struct window {
    uint32_t id;
    char title[64];
    rect_t rect;
    rect_t min_size;
    rect_t max_size;
    
    int state;
    int visible;
    int focused;
    int decorated;
    
    uint32_t bg_color;
    
    void (*on_close)(struct window *window);
    void (*on_minimize)(struct window *window);
    void (*on_focus)(struct window *window);
    void (*on_draw)(struct window *window);
    
    struct window *next;
} window_t;

typedef struct {
    window_t *stack;
    window_t *focused;
    int count;
    int initialized;
} window_manager_t;

int wm_init(void);
void wm_shutdown(void);

window_t *wm_create_window(const char *title, int x, int y, int width, int height);
void wm_close_window(window_t *window);
void wm_focus(window_t *window);
void wm_raise(window_t *window);
void wm_lower(window_t *window);

void wm_set_state(window_t *window, int state);
int wm_get_state(window_t *window);

void wm_render_focus_ring(window_t *window);
void wm_render_titlebar(window_t *window);
void wm_render_decorations(window_t *window);

void wm_set_min_size(window_t *window, int min_w, int min_h);
void wm_set_max_size(window_t *window, int max_w, int max_h);

window_t *wm_get_focused_window(void);
window_t *wm_get_window_at(int x, int y);
int wm_get_window_count(void);

#endif /* _WINDOW_MGR_H_ */