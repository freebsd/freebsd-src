/*
 * UOS Compositor - Shell Integration Implementation
 * BSD-licensed
 */

#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uos_shell_surface_t *g_lock_surface = NULL;
static struct wl_list g_overlay_list;

int shell_init(void)
{
    wl_list_init(&g_overlay_list);
    return 0;
}

void shell_deinit(void)
{
    if (g_lock_surface) {
        free(g_lock_surface);
        g_lock_surface = NULL;
    }
}

uos_shell_surface_t *shell_lock_surface(void)
{
    g_lock_surface = calloc(1, sizeof(*g_lock_surface));
    if (!g_lock_surface) return NULL;

    g_lock_surface->type = SHELL_SURFACE_LOCK;
    g_lock_surface->active = true;
    g_lock_surface->width = g_compositor->output.width;
    g_lock_surface->height = g_compositor->output.height;

    return g_lock_surface;
}

void shell_unlock_surface(void)
{
    if (!g_lock_surface) return;
    free(g_lock_surface);
    g_lock_surface = NULL;
}

uos_shell_surface_t *shell_overlay_surface(int32_t x, int32_t y, int32_t w, int32_t h)
{
    uos_shell_surface_t *shell_surface;

    shell_surface = calloc(1, sizeof(*shell_surface));
    if (!shell_surface) return NULL;

    shell_surface->type = SHELL_SURFACE_OVERLAY;
    shell_surface->active = true;
    shell_surface->x = x;
    shell_surface->y = y;
    shell_surface->width = w;
    shell_surface->height = h;

    wl_list_insert(&g_overlay_list, &shell_surface->link);

    return shell_surface;
}

void shell_remove_overlay(uos_shell_surface_t *shell_surface)
{
    if (!shell_surface) return;
    wl_list_remove(&shell_surface->link);
    free(shell_surface);
}

uos_shell_popup_t *shell_popup_create(struct wl_surface *parent, int32_t x, int32_t y, int32_t w, int32_t h)
{
    uos_shell_popup_t *popup;

    (void)parent;

    popup = calloc(1, sizeof(*popup));
    if (!popup) return NULL;

    popup->x = x;
    popup->y = y;
    popup->width = w;
    popup->height = h;

    return popup;
}

void shell_popup_destroy(uos_shell_popup_t *popup)
{
    if (!popup) return;
    free(popup);
}