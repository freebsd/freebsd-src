/*
 * UOS Compositor - Shell Integration
 * BSD-licensed
 */

#ifndef _UOS_SHELL_H_
#define _UOS_SHELL_H_

#include <stdint.h>
#include <stdbool.h>
#include "compositor.h"

/* Shell surface types */
#define SHELL_SURFACE_LOCK    0x01
#define SHELL_SURFACE_OVERLAY 0x02
#define SHELL_SURFACE_POPUP   0x04

/* Shell surface */
typedef struct uos_shell_surface {
    struct wl_resource *resource;
    struct wl_surface *surface;
    int32_t type;
    bool active;
    int32_t x, y;
    int32_t width, height;
    struct wl_list link;
} uos_shell_surface_t;

/* Shell popup */
typedef struct uos_shell_popup {
    struct wl_resource *resource;
    struct wl_surface *surface;
    int32_t x, y;
    int32_t width, height;
} uos_shell_popup_t;

/* Shell public API */
int shell_init(void);
void shell_deinit(void);

uos_shell_surface_t *shell_lock_surface(void);
void shell_unlock_surface(void);

uos_shell_surface_t *shell_overlay_surface(int32_t x, int32_t y, int32_t w, int32_t h);
void shell_remove_overlay(uos_shell_surface_t *shell_surface);

uos_shell_popup_t *shell_popup_create(struct wl_surface *parent, int32_t x, int32_t y, int32_t w, int32_t h);
void shell_popup_destroy(uos_shell_popup_t *popup);

#endif /* _UOS_SHELL_H_ */