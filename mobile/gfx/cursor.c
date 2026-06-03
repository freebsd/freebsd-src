/*
 * Copyright (c) 2026 The FreeBSD Mobile Project
 * All rightserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "cursor.h"
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

/* Global state */
static int g_drm_fd = -1;
static uint32_t g_cursor_bo_handle = 0;
static bool g_cursor_visible = false;
static int g_cursor_x = 0, g_cursor_y = 0;

/* Initialize cursor */
int
cursor_init(int drm_fd)
{
    if (drm_fd < 0)
        return -EINVAL;

    g_drm_fd = drm_fd;

    /* Allocate a buffer object for the cursor */
    /* We'll use a dumb buffer for simplicity */
    struct drm_mode_create_dumb create = {
        .width = CURSOR_WIDTH,
        .height = CURSOR_HEIGHT,
        .bpp = 32 /* ARGB8888 */
    };
    int ret = drmIoctl(g_drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create);
    if (ret < 0)
        return -errno;

    g_cursor_bo_handle = create.handle;

    /* Hide cursor initially */
    cursor_hide(0); /* Assume CRTC 0 for now */

    return 0;
}

/* Cleanup cursor */
void
cursor_fini(void)
{
    if (g_drm_fd >= 0) {
        if (g_cursor_bo_handle) {
            struct drm_mode_destroy_dumb destroy = {
                .handle = g_cursor_bo_handle
            };
            drmIoctl(g_drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
        }
        close(g_drm_fd);
        g_drm_fd = -1;
    }
    g_cursor_bo_handle = 0;
    g_cursor_visible = false;
}

/* Set cursor position and image */
int
cursor_set(uint32_t crtc_id, int x, int y, uint32_t bo_handle)
{
    if (g_drm_fd < 0)
        return -ENODEV;

    /* Use CURSOR2 ioctl if available */
    struct drm_mode_cursor2 cursor = {
        .flags = DRM_MODE_CURSOR_BO,
        .crtc_id = crtc_id,
        .width = CURSOR_WIDTH,
        .height = CURSOR_HEIGHT,
        .handle = bo_handle ? bo_handle : g_cursor_bo_handle,
        .hot_x = CURSOR_WIDTH / 2,
        .hot_y = CURSOR_HEIGHT / 2,
    };

    if (x >= 0 && y >= 0) {
        cursor.flags |= DRM_MODE_CURSOR_MOVE;
        cursor.x = x;
        cursor.y = y;
    } else {
        cursor.flags |= DRM_MODE_CURSOR_HIDE;
    }

    int ret = drmIoctl(g_drm_fd, DRM_IOCTL_MODE_CURSOR2, &cursor);
    if (ret < 0) {
        /* Fallback to CURSOR ioctl */
        struct drm_mode_cursor old_cursor = {
            .crtc_id = crtc_id,
            .width = CURSOR_WIDTH,
            .height = CURSOR_HEIGHT,
            .handle = bo_handle ? bo_handle : g_cursor_bo_handle,
        };
        if (x >= 0 && y >= 0) {
            old_cursor.flags = DRM_MODE_CURSOR_BO | DRM_MODE_CURSOR_MOVE;
            old_cursor.x = x;
            old_cursor.y = y;
        } else {
            old_cursor.flags = DRM_MODE_CURSOR_BO | DRM_MODE_CURSOR_HIDE;
        }
        ret = drmIoctl(g_drm_fd, DRM_IOCTL_MODE_CURSOR, &old_cursor);
        if (ret < 0)
            return -errno;
    }

    if (x >= 0 && y >= 0) {
        g_cursor_x = x;
        g_cursor_y = y;
        g_cursor_visible = true;
    } else {
        g_cursor_visible = false;
    }

    return 0;
}

/* Move cursor without changing image */
int
cursor_move(uint32_t crtc_id, int x, int y)
{
    if (g_drm_fd < 0)
        return -ENODEV;

    struct drm_mode_cursor cursor = {
        .crtc_id = crtc_id,
        .flags = DRM_MODE_CURSOR_MOVE,
        .x = x,
        .y = y,
    };

    int ret = drmIoctl(g_drm_fd, DRM_IOCTL_MODE_CURSOR, &cursor);
    if (ret < 0)
        return -errno;

    g_cursor_x = x;
    g_cursor_y = y;
    return 0;
}

/* Hide cursor */
int
cursor_hide(uint32_t crtc_id)
{
    if (g_drm_fd < 0)
        return -ENODEV;

    struct drm_mode_cursor cursor = {
        .crtc_id = crtc_id,
        .flags = DRM_MODE_CURSOR_HIDE,
    };

    int ret = drmIoctl(g_drm_fd, DRM_IOCTL_MODE_CURSOR, &cursor);
    if (ret < 0)
        return -errno;

    g_cursor_visible = false;
    return 0;
}

/* Show cursor */
int
cursor_show(uint32_t crtc_id)
{
    if (g_drm_fd < 0)
        return -ENODEV;

    struct drm_mode_cursor cursor = {
        .crtc_id = crtc_id,
        .flags = DRM_MODE_CURSOR_BO | DRM_MODE_CURSOR_MOVE,
        .width = CURSOR_WIDTH,
        .height = CURSOR_HEIGHT,
        .handle = g_cursor_bo_handle,
        .x = g_cursor_x,
        .y = g_cursor_y,
    };

    int ret = drmIoctl(g_drm_fd, DRM_IOCTL_MODE_CURSOR, &cursor);
    if (ret < 0)
        return -errno;

    g_cursor_visible = true;
    return 0;
}

/* Software fallback: render cursor into a buffer */
void
cursor_render_software(uint8_t *buffer, int width, int height, int pitch,
                       int x, int y, uint32_t fg, uint32_t bg)
{
    int i, j;
    int cursor_x = x - CURSOR_WIDTH / 2;
    int cursor_y = y - CURSOR_HEIGHT / 2;

    /* Simple cursor: a black border with white interior */
    uint32_t border = 0xFF000000; /* ARGB */
    uint32_t fill = 0xFFFFFFFF;   /* ARGB */

    for (j = 0; j < CURSOR_HEIGHT; j++) {
        int screen_y = cursor_y + j;
        if (screen_y < 0 || screen_y >= height)
            continue;
        uint8_t *row = buffer + screen_y * pitch;
        for (i = 0; i < CURSOR_WIDTH; i++) {
            int screen_x = cursor_x + i;
            if (screen_x < 0 || screen_x >= width)
                continue;
            bool is_border = (i == 0 || i == CURSOR_WIDTH-1 || j == 0 || j == CURSOR_HEIGHT-1);
            uint32_t color = is_border ? border : fill;
            *(uint32_t *)(row + screen_x * 4) = color;
        }
    }
}