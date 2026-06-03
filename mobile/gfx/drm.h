/*
 * Copyright (c) 2026 The FreeBSD Mobile Project
 * All rights reserved.
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

#ifndef _MOBILE_GFX_DRM_H_
#define _MOBILE_GFX_DRM_H_

#include <stdint.h>
#include <stdbool.h>

/* DRM connector types */
#define DRM_MODE_CONNECTOR_HDMI   0x00000006
#define DRM_MODE_CONNECTOR_DP     0x0000000A
#define DRM_MODE_CONNECTOR_EDP    0x0000000B
#define DRM_MODE_CONNECTOR_LVDS   0x00000002
#define DRM_MODE_CONNECTOR_DSI    0x0000000E

/* DRM pixel formats */
#define DRM_FORMAT_XRGB8888       0x34325258 /* XR24 */
#define DRM_FORMAT_ARGB8888       0x34325241 /* AR24 */
#define DRM_FORMAT_RGB565         0x36314752 /* RGB565 */

/* DRM flags */
#define DRM_MODE_PAGE_FLIP_EVENT  0x00000001
#define DRM_MODE_PAGE_FLIP_ASYNC  0x00000002

/* GPU vendor IDs */
#define GPU_VENDOR_INTEL          0x8086
#define GPU_VENDOR_AMD            0x1022
#define GPU_VENDOR_ARM            0x13B5
#define GPU_VENDOR_QUALCOMM       0x5143

/* Forward declarations */
struct drm_device;
struct drm_connector;
struct drm_encoder;
struct drm_crtc;
struct drm_mode;
struct drm_framebuffer;

/* GPU information */
struct gpu_info {
    uint16_t vendor;
    uint16_t device;
    const char *name;
    bool has_3d;
    bool has_gl;
    int max_width;
    int max_height;
};

/* DRM device handle */
struct drm_device {
    int fd;
    struct gpu_info gpu;
    /* Resources */
    size_t count_connectors;
    struct drm_connector **connectors;
    size_t count_encoders;
    struct drm_encoder **encoders;
    size_t count_crtcs;
    struct drm_crtc **crtcs;
    size_t count_framebuffers;
    struct drm_framebuffer **framebuffers;
};

/* DRM connector */
struct drm_connector {
    uint32_t connector_id;
    uint32_t connector_type;
    uint32_t connector_type_id;
    bool connected;
    struct drm_mode *modes;
    size_t count_modes;
};

/* DRM encoder */
struct drm_encoder {
    uint32_t encoder_id;
    uint32_t encoder_type;
};

/* DRM CRTC */
struct drm_crtc {
    uint32_t crtc_id;
};

/* DRM display mode */
struct drm_mode {
    uint32_t clock;
    uint16_t hdisplay;
    uint16_t hsync_start;
    uint16_t hsync_end;
    uint16_t htotal;
    uint16_t vdisplay;
    uint16_t vsync_start;
    uint16_t vsync_end;
    uint16_t vtotal;
    uint32_t vrefresh;
    uint32_t flags;
    uint32_t type;
    char name[DRM_DISPLAY_MODE_LEN];
};

/* DRM framebuffer (dumb buffer) */
struct drm_framebuffer {
    uint32_t fb_id;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t handle;
    uint8_t *map;
    size_t size;
};

/* Public API */
int drm_open(const char *path, struct drm_device **dev);
void drm_close(struct drm_device *dev);
int drm_get_resources(struct drm_device *dev);
struct drm_connector *drm_find_connector(struct drm_device *dev, uint32_t connector_type);
struct drm_crtc *drm_find_crtc(struct drm_device *dev, struct drm_connector *connector);
int drm_setup_mode(struct drm_device *dev, struct drm_connector *connector, struct drm_mode *mode);
int drm_page_flip(struct drm_device *dev, uint32_t crtc_id, uint32_t fb_id, uint32_t flags, void *user_data);
int drm_wait_vblank(struct drm_device *dev, uint32_t crtc_id);
uint32_t drm_create_fb(struct drm_device *dev, uint32_t width, uint32_t height, uint32_t format);
int drm_destroy_fb(struct drm_device *dev, uint32_t fb_id);

#endif /* _MOBILE_GFX_DRM_H_ */