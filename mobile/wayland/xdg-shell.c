/*
 * UOS Compositor - xdg-shell Protocol Implementation
 * BSD-licensed
 */

#include "xdg-shell.h"
#include "window.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static xdg_wm_base_t *g_wm_base = NULL;

void xdg_shell_init(struct wl_display *display)
{
    g_wm_base = calloc(1, sizeof(*g_wm_base));
    g_wm_base->version = XDG_WM_BASE_VERSION;
}

void xdg_shell_deinit(struct wl_display *display)
{
    if (g_wm_base) {
        free(g_wm_base);
        g_wm_base = NULL;
    }
}

xdg_surface_t *xdg_surface_create(xdg_wm_base_t *wm_base,
                                  struct wl_surface *surface,
                                  uint32_t id)
{
    xdg_surface_t *xdg_surface;

    xdg_surface = calloc(1, sizeof(*xdg_surface));
    if (!xdg_surface) return NULL;

    xdg_surface->surface = surface;
    xdg_surface->wm_base = wm_base;
    xdg_surface->width = 800;
    xdg_surface->height = 600;

    return xdg_surface;
}

void xdg_surface_destroy(xdg_surface_t *xdg_surface)
{
    if (!xdg_surface) return;
    free(xdg_surface);
}

void xdg_surface_get_toplevel(xdg_surface_t *xdg_surface, uint32_t id)
{
    xdg_toplevel_t *toplevel;

    toplevel = calloc(1, sizeof(*toplevel));
    if (!toplevel) return;

    toplevel->xdg_surface = xdg_surface;

    /* Create associated window */
    xdg_surface->window = comp_create_toplevel(NULL, id,
                                              toplevel->title,
                                              xdg_surface->width,
                                              xdg_surface->height,
                                              toplevel->app_id);
}

void xdg_surface_get_popup(xdg_surface_t *xdg_surface, uint32_t id,
                           struct wl_surface *parent, int32_t x, int32_t y)
{
    xdg_popup_t *popup;

    popup = calloc(1, sizeof(*popup));
    if (!popup) return;

    popup->xdg_surface = xdg_surface;
    popup->parent = parent;
    popup->x = x;
    popup->y = y;
}

void xdg_surface_set_window_geometry(xdg_surface_t *xdg_surface,
                                    int32_t x, int32_t y, int32_t w, int32_t h)
{
    xdg_surface->x = x;
    xdg_surface->y = y;
    xdg_surface->width = w;
    xdg_surface->height = h;
}

void xdg_surface_ack_configure(xdg_surface_t *xdg_surface, uint32_t serial)
{
    if (xdg_surface->configure_serial == serial) {
        /* Configuration acknowledged - apply state */
    }
}

void xdg_wm_base_pong(xdg_wm_base_t *wm_base, uint32_t serial)
{
    /* Ping response received - send frame if needed */
    (void)wm_base;
    (void)serial;
}

void xdg_toplevel_destroy(xdg_toplevel_t *toplevel)
{
    if (!toplevel) return;
    free(toplevel);
}

void xdg_toplevel_set_parent(xdg_toplevel_t *toplevel, xdg_toplevel_t *parent)
{
    /* Set parent window for modal dialogs */
    (void)parent;
}

void xdg_toplevel_set_title(xdg_toplevel_t *toplevel, const char *title)
{
    if (!toplevel || !title) return;
    strncpy(toplevel->title, title, sizeof(toplevel->title) - 1);
    /* Update window title */
}

void xdg_toplevel_set_app_id(xdg_toplevel_t *toplevel, const char *app_id)
{
    if (!toplevel || !app_id) return;
    strncpy(toplevel->app_id, app_id, sizeof(toplevel->app_id) - 1);
}

void xdg_toplevel_set_maximized(xdg_toplevel_t *toplevel)
{
    if (!toplevel) return;
    toplevel->state |= XDG_TOPLEVEL_STATE_MAXIMIZED;
    toplevel->xdg_surface->width = g_compositor->output.width;
    toplevel->xdg_surface->height = g_compositor->output.height;
}

void xdg_toplevel_unset_maximized(xdg_toplevel_t *toplevel)
{
    if (!toplevel) return;
    toplevel->state &= ~XDG_TOPLEVEL_STATE_MAXIMIZED;
}

void xdg_toplevel_set_fullscreen(xdg_toplevel_t *toplevel, int32_t output)
{
    if (!toplevel) return;
    toplevel->state |= XDG_TOPLEVEL_STATE_FULLSCREEN;
    (void)output;
}

void xdg_toplevel_unset_fullscreen(xdg_toplevel_t *toplevel)
{
    if (!toplevel) return;
    toplevel->state &= ~XDG_TOPLEVEL_STATE_FULLSCREEN;
}

void xdg_toplevel_set_minimized(xdg_toplevel_t *toplevel)
{
    if (!toplevel) return;
    toplevel->state |= XDG_TOPLEVEL_STATE_ACTIVE;
    /* Window minimized - hide from view */
}

void xdg_popup_destroy(xdg_popup_t *popup)
{
    if (!popup) return;
    free(popup);
}