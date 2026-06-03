/*
 * UOS Compositor - xdg-shell Protocol Implementation
 * BSD-licensed
 */

#ifndef _UOS_XDG_SHELL_H_
#define _UOS_XDG_SHELL_H_

#include <stdint.h>
#include <stdbool.h>
#include "protocols.h"
#include "compositor.h"

/* xdg-shell version */
#define XDG_WM_BASE_VERSION 1

/* Toplevel window states */
#define XDG_TOPLEVEL_STATE_ACTIVE      0x01
#define XDG_TOPLEVEL_STATE_MAXIMIZED   0x02
#define XDG_TOPLEVEL_STATE_FULLSCREEN  0x04
#define XDG_TOPLEVEL_STATE_RESIZING    0x08

/* xdg-shell interfaces */
typedef struct xdg_wm_base {
    struct wl_resource resource;
    uint32_t version;
} xdg_wm_base_t;

typedef struct xdg_surface {
    struct wl_resource resource;
    struct wl_surface *surface;
    uint32_t version;
    xdg_wm_base_t *wm_base;
    uos_window_t *window;
    int32_t x, y;
    int32_t width, height;
    uint32_t configure_serial;
} xdg_surface_t;

typedef struct xdg_toplevel {
    struct wl_resource resource;
    xdg_surface_t *xdg_surface;
    char title[256];
    char app_id[64];
    uint32_t state;
    uint32_t resize_edges;
    uint32_t parent_id;
} xdg_toplevel_t;

typedef struct xdg_popup {
    struct wl_resource resource;
    xdg_surface_t *xdg_surface;
    struct wl_surface *parent;
    int32_t x, y;
    int32_t width, height;
} xdg_popup_t;

/* xdg-shell API */
void xdg_shell_init(struct wl_display *display);
void xdg_shell_deinit(struct wl_display *display);

/* xdg_wm_base handlers */
void xdg_wm_base_destroy(xdg_wm_base_t *wm_base);
void xdg_wm_base_pong(xdg_wm_base_t *wm_base, uint32_t serial);

/* xdg_surface handlers */
xdg_surface_t *xdg_surface_create(xdg_wm_base_t *wm_base,
                                  struct wl_surface *surface,
                                  uint32_t id);
void xdg_surface_destroy(xdg_surface_t *xdg_surface);
void xdg_surface_get_toplevel(xdg_surface_t *xdg_surface, uint32_t id);
void xdg_surface_get_popup(xdg_surface_t *xdg_surface, uint32_t id,
                           struct wl_surface *parent, int32_t x, int32_t y);
void xdg_surface_set_window_geometry(xdg_surface_t *xdg_surface,
                                    int32_t x, int32_t y, int32_t w, int32_t h);
void xdg_surface_ack_configure(xdg_surface_t *xdg_surface, uint32_t serial);

/* xdg_toplevel handlers */
void xdg_toplevel_destroy(xdg_toplevel_t *toplevel);
void xdg_toplevel_set_parent(xdg_toplevel_t *toplevel, xdg_toplevel_t *parent);
void xdg_toplevel_set_title(xdg_toplevel_t *toplevel, const char *title);
void xdg_toplevel_set_app_id(xdg_toplevel_t *toplevel, const char *app_id);
void xdg_toplevel_show_window_menu(xdg_toplevel_t *toplevel, int32_t seat,
                                   int32_t x, int32_t y);
void xdg_toplevel_move(xdg_toplevel_t *toplevel, int32_t seat, int32_t serial);
void xdg_toplevel_resize(xdg_toplevel_t *toplevel, int32_t seat, int32_t serial,
                         int32_t edges);
void xdg_toplevel_set_max_size(xdg_toplevel_t *toplevel, int32_t w, int32_t h);
void xdg_toplevel_set_min_size(xdg_toplevel_t *toplevel, int32_t w, int32_t h);
void xdg_toplevel_set_maximized(xdg_toplevel_t *toplevel);
void xdg_toplevel_unset_maximized(xdg_toplevel_t *toplevel);
void xdg_toplevel_set_fullscreen(xdg_toplevel_t *toplevel, int32_t output);
void xdg_toplevel_unset_fullscreen(xdg_toplevel_t *toplevel);
void xdg_toplevel_set_minimized(xdg_toplevel_t *toplevel);

/* xdg_popup handlers */
void xdg_popup_destroy(xdg_popup_t *popup);
void xdg_popup_grab(xdg_popup_t *popup, int32_t seat, int32_t serial);
void xdg_popup_reposition(xdg_popup_t *popup, int32_t seat, int32_t serial);

#endif /* _UOS_XDG_SHELL_H_ */