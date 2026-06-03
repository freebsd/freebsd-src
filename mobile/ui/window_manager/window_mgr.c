/*
 * Window Manager Implementation
 * BSD-licensed
 */

#include "window_mgr.h"
#include <string.h>
#include <stdlib.h>

static window_manager_t g_wm;
static uint32_t g_next_window_id = 1;

int wm_init(void) {
    memset(&g_wm, 0, sizeof(g_wm));
    g_wm.stack = NULL;
    g_wm.focused = NULL;
    g_wm.initialized = 1;
    return 0;
}

void wm_shutdown(void) {
    while (g_wm.stack) {
        window_t *w = g_wm.stack;
        g_wm.stack = w->next;
        free(w);
    }
    g_wm.count = 0;
    g_wm.initialized = 0;
}

window_t *wm_create_window(const char *title, int x, int y, int width, int height) {
    if (g_wm.count >= MAX_WINDOWS) return NULL;
    
    window_t *win = calloc(1, sizeof(window_t));
    if (!win) return NULL;
    
    win->id = g_next_window_id++;
    strncpy(win->title, title ? title : "Untitled", sizeof(win->title) - 1);
    win->rect.x = x;
    win->rect.y = y;
    win->rect.width = width;
    win->rect.height = height;
    win->state = WIN_STATE_NORMAL;
    win->visible = 1;
    win->focused = 0;
    win->decorated = 1;
    win->bg_color = 0xFF1a1a2e;
    
    win->min_size.width = 100;
    win->min_size.height = 100;
    win->max_size.width = FB_WIDTH;
    win->max_size.height = FB_HEIGHT;
    
    win->next = g_wm.stack;
    g_wm.stack = win;
    g_wm.count++;
    
    return win;
}

void wm_close_window(window_t *window) {
    if (!window || !g_wm.initialized) return;
    
    if (window->on_close) window->on_close(window);
    
    if (g_wm.stack == window) {
        g_wm.stack = window->next;
    } else {
        window_t *w = g_wm.stack;
        while (w && w->next != window) {
            w = w->next;
        }
        if (w) w->next = window->next;
    }
    
    if (g_wm.focused == window) {
        g_wm.focused = NULL;
    }
    
    free(window);
    g_wm.count--;
}

void wm_focus(window_t *window) {
    if (!window || !g_wm.initialized) return;
    
    if (g_wm.focused == window) return;
    
    if (g_wm.focused) {
        g_wm.focused->focused = 0;
        if (g_wm.focused->on_focus) {
            g_wm.focused->on_focus(g_wm.focused);
        }
    }
    
    g_wm.focused = window;
    window->focused = 1;
    
    if (window->on_focus) window->on_focus(window);
    wm_raise(window);
}

void wm_raise(window_t *window) {
    if (!window || !g_wm.initialized) return;
    
    if (g_wm.stack == window) return;
    
    window_t *w = g_wm.stack;
    while (w && w->next != window) {
        w = w->next;
    }
    if (!w) return;
    
    w->next = window->next;
    window->next = g_wm.stack;
    g_wm.stack = window;
}

void wm_lower(window_t *window) {
    if (!window || !g_wm.initialized) return;
}

void wm_set_state(window_t *window, int state) {
    if (!window) return;
    
    window->state = state;
    switch (state) {
    case WIN_STATE_MAXIMIZED:
        window->rect.x = 0;
        window->rect.y = 0;
        window->rect.width = FB_WIDTH;
        window->rect.height = FB_HEIGHT;
        break;
    case WIN_STATE_MINIMIZED:
        window->visible = 0;
        break;
    case WIN_STATE_NORMAL:
        window->visible = 1;
        break;
    }
}

int wm_get_state(window_t *window) {
    return window ? window->state : -1;
}

void wm_render_focus_ring(window_t *window) {
    if (!window) return;
    
    int x = window->rect.x;
    int y = window->rect.y;
    int w = window->rect.width;
    int h = window->rect.height;
    
    fb_draw_rect(x - 2, y - 2, w + 4, h + 4, 0xFF0f3460);
}

void wm_render_titlebar(window_t *window) {
    if (!window || !window->decorated) return;
    
    int x = window->rect.x;
    int y = window->rect.y;
    int w = window->rect.width;
    
    fb_fill_rect(x, y, w, 40, 0xFF16213e);
    
    fb_draw_text(x + 10, y + 25, window->title, 0xFFe0e0e0, 0);
    
    int btn_x = x + w - 120;
    fb_fill_rect(btn_x, y + 10, 25, 25, 0xFFff0000);
    
    btn_x += 35;
    fb_fill_rect(btn_x, y + 10, 25, 25, 0xFF808080);
    
    btn_x += 35;
    fb_fill_rect(btn_x, y + 10, 25, 25, 0xFF00ff00);
}

void wm_render_decorations(window_t *window) {
    if (!window || !window->decorated) return;
    
    wm_render_titlebar(window);
}

void wm_set_min_size(window_t *window, int min_w, int min_h) {
    if (window) {
        window->min_size.width = min_w;
        window->min_size.height = min_h;
    }
}

void wm_set_max_size(window_t *window, int max_w, int max_h) {
    if (window) {
        window->max_size.width = max_w;
        window->max_size.height = max_h;
    }
}

window_t *wm_get_focused_window(void) {
    return g_wm.focused;
}

window_t *wm_get_window_at(int x, int y) {
    for (window_t *w = g_wm.stack; w; w = w->next) {
        if (!w->visible) continue;
        if (x >= w->rect.x && x < w->rect.x + w->rect.width &&
            y >= w->rect.y && y < w->rect.y + w->rect.height) {
            return w;
        }
    }
    return NULL;
}

int wm_get_window_count(void) {
    return g_wm.count;
}