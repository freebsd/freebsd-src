/*
 * UOS Compositor - Window Management Implementation
 * BSD-licensed
 */

#include "window.h"
#include "compositor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uos_window_t *window_create(const char *title, int32_t w, int32_t h, const char *app_id)
{
    uos_window_t *window;

    window = calloc(1, sizeof(*window));
    if (!window) return NULL;

    if (title) {
        strncpy(window->title, title, sizeof(window->title) - 1);
    }
    if (app_id) {
        strncpy(window->app_id, app_id, sizeof(window->app_id) - 1);
    }

    window->width = w > 0 ? w : 800;
    window->height = h > 0 ? h : 600;
    window->decorated = true;
    window->visible = true;
    window->state = WINDOW_STATE_NORMAL;

    wl_list_init(&window->link);
    wl_list_init(&window->layer_link);

    return window;
}

void window_destroy(uos_window_t *window)
{
    if (!window) return;

    wl_list_remove(&window->link);
    wl_list_remove(&window->layer_link);
    free(window);
}

void window_set_state(uos_window_t *window, uint32_t state)
{
    uint32_t old_state, changes;

    if (!window) return;

    old_state = window->state;
    window->state = state;

    changes = old_state ^ state;

    if (changes & WINDOW_STATE_MAXIMIZED) {
        if (state & WINDOW_STATE_MAXIMIZED) {
            window->saved_width = window->width;
            window->saved_height = window->height;
            window->width = g_compositor->output.width;
            window->height = g_compositor->output.height;
        } else {
            window->width = window->saved_width;
            window->height = window->saved_height;
        }
    }

    if (changes & WINDOW_STATE_FULLSCREEN) {
        if (state & WINDOW_STATE_FULLSCREEN) {
            window->saved_x = window->x;
            window->saved_y = window->y;
            window->x = 0;
            window->y = 0;
            window->width = g_compositor->output.width;
            window->height = g_compositor->output.height;
        } else {
            window->x = window->saved_x;
            window->y = window->saved_y;
            window->width = window->saved_width;
            window->height = window->saved_height;
        }
    }

    if (changes & WINDOW_STATE_MINIMIZED) {
        window->visible = !(state & WINDOW_STATE_MINIMIZED);
    }
}

void window_set_title(uos_window_t *window, const char *title)
{
    if (!window || !title) return;
    strncpy(window->title, title, sizeof(window->title) - 1);
    /* Send title change event */
}

void window_set_app_id(uos_window_t *window, const char *app_id)
{
    if (!window || !app_id) return;
    strncpy(window->app_id, app_id, sizeof(window->app_id) - 1);
}

void window_set_geometry(uos_window_t *window, int32_t x, int32_t y, int32_t w, int32_t h)
{
    if (!window) return;
    window->x = x;
    window->y = y;
    window->width = w;
    window->height = h;
}

void window_raise(uos_window_t *window)
{
    uos_window_t *w;
    int max_z = 0;

    if (!window) return;

    wl_list_for_each(w, &g_compositor->window_list, link) {
        if (w != window && w->z_order > max_z) {
            max_z = w->z_order;
        }
    }

    window->z_order = max_z + 1;
    wl_list_remove(&window->link);
    wl_list_insert(&g_compositor->window_list, &window->link);
}

void window_lower(uos_window_t *window)
{
    if (!window) return;
    wl_list_remove(&window->link);
    wl_list_insert(&g_compositor->window_list.prev, &window->link);
}

void window_stack_fullscreen(uos_window_t *window)
{
    if (!window) return;
    window_set_state(window, WINDOW_STATE_FULLSCREEN);
    window_raise(window);
}

void window_stack_remove(uos_window_t *window)
{
    if (!window) return;
    /* Remove from fullscreen stack */
    if (window->state & WINDOW_STATE_FULLSCREEN) {
        window_set_state(window, window->state & ~WINDOW_STATE_FULLSCREEN);
    }
}

void window_show(uos_window_t *window)
{
    if (!window) return;
    window->visible = true;
}

void window_hide(uos_window_t *window)
{
    if (!window) return;
    window->visible = false;
}

bool window_is_visible(uos_window_t *window)
{
    return window ? window->visible : false;
}