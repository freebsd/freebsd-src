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

#include "drm.h"
#include "gbm.h"
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#define DRM_DISPLAY_MODE_LEN 32

/* Helper to find GPU vendor from PCI ID */
static const char *
gpu_vendor_name(uint16_t vendor)
{
    switch (vendor) {
    case GPU_VENDOR_INTEL: return "Intel";
    case GPU_VENDOR_AMD: return "AMD";
    case GPU_VENDOR_ARM: return "ARM";
    case GPU_VENDOR_QUALCOMM: return "Qualcomm";
    default: return "Unknown";
    }
}

/* Helper to detect GPU has 3D/GL based on driver name */
static bool
gpu_has_3d(const char *driver)
{
    if (!driver)
        return false;
    /* Simple heuristic: most drivers with 3D acceleration */
    return (strstr(driver, "i915") || strstr(driver, "amdgpu") ||
            strstr(driver, "lima") || strstr(driver, "panfrost") ||
            strstr(driver, "adreno") || strstr(driver, "freedreno"));
}

static bool
gpu_has_gl(const char *driver)
{
    return gpu_has_3d(driver); /* Assume GL if 3D */
}

/* Open DRM device */
int
drm_open(const char *path, struct drm_device **dev_out)
{
    struct drm_device *dev;
    int fd, ret;
    char driver[64];
    int major, minor;

    if (!path || !dev_out)
        return -EINVAL;

    fd = open(path, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        return -errno;
    }

    ret = drmGetVersion(fd, &(struct drm_version){
        .version = 0,
        .name = NULL,
        .date = NULL,
        .desc = NULL
    });
    if (ret < 0) {
        close(fd);
        return ret;
    }

    /* Allocate device structure */
    dev = calloc(1, sizeof(struct drm_device));
    if (!dev) {
        close(fd);
        return -ENOMEM;
    }
    dev->fd = fd;

    /* Get GPU information */
    ret = drmGetVersion(fd, &(struct drm_version){
        .version = 0,
        .name = driver,
        .date = NULL,
        .desc = NULL
    });
    if (ret >= 0) {
        dev->gpu.has_3d = gpu_has_3d(driver);
        dev->gpu.has_gl = gpu_has_gl(driver);
        dev->gpu.name = gpu_vendor_name(0); /* We don't have PCI info here */
        /* In a real implementation, we would scan PCI or sysfs for vendor/device */
        /* For now, we set unknown */
        dev->gpu.vendor = 0;
        dev->gpu.device = 0;
    } else {
        dev->gpu.has_3d = false;
        dev->gpu.has_gl = false;
        dev->gpu.name = "Unknown";
        dev->gpu.vendor = 0;
        dev->gpu.device = 0;
    }

    /* Get resources (connectors, encoders, CRTCs) */
    ret = drmGetResources(fd, &(struct drm_mode_card_res){
        .count_connectors = 0,
        .connectors = NULL,
        .count_encoders = 0,
        .encoders = NULL,
        .count_crtcs = 0,
        .crtcs = NULL,
        .count_fb = 0,
        .fb_id = NULL
    });
    if (ret < 0) {
        free(dev);
        close(fd);
        return ret;
    }

    /* In a real implementation, we would populate the connectors, encoders, crtcs arrays */
    /* For now, we leave them as NULL and set counts to 0 */
    dev->count_connectors = 0;
    dev->connectors = NULL;
    dev->count_encoders = 0;
    dev->encoders = NULL;
    dev->count_crtcs = 0;
    dev->crtcs = NULL;
    dev->count_framebuffers = 0;
    dev->framebuffers = NULL;

    *dev_out = dev;
    return 0;
}

/* Close DRM device */
void
drm_close(struct drm_device *dev)
{
    if (!dev)
        return;

    if (dev->fd >= 0)
        close(dev->fd);

    /* Free resources */
    if (dev->connectors)
        free(dev->connectors);
    if (dev->encoders)
        free(dev->encoders);
    if (dev->crtcs)
        free(dev->crtcs);
    if (dev->framebuffers)
        free(dev->framebuffers);

    free(dev);
}

/* Get resources (stub) */
int
drm_get_resources(struct drm_device *dev)
{
    if (!dev)
        return -EINVAL;

    /* In a real implementation, we would call drmModeGetResources and populate the arrays */
    /* For now, we return success but with empty arrays */
    return 0;
}

/* Find connector by type */
struct drm_connector *
drm_find_connector(struct drm_device *dev, uint32_t connector_type)
{
    size_t i;

    if (!dev)
        return NULL;

    for (i = 0; i < dev->count_connectors; i++) {
        if (dev->connectors[i]->connector_type == connector_type &&
            dev->connectors[i]->connected) {
            return dev->connectors[i];
        }
    }
    return NULL;
}

/* Find CRTC for connector */
struct drm_crtc *
drm_find_crtc(struct drm_device *dev, struct drm_connector *connector)
{
    /* In a real implementation, we would check the connector's encoder and then find a CRTC */
    /* For now, we return the first CRTC if available */
    if (!dev || !connector || dev->count_crtcs == 0)
        return NULL;
    return dev->crtcs[0];
}

/* Setup mode */
int
drm_setup_mode(struct drm_device *dev, struct drm_connector *connector, struct drm_mode *mode)
{
    if (!dev || !connector || !mode)
        return -EINVAL;

    /* In a real implementation, we would call drmModeSetCrtc */
    /* For now, we return success */
    return 0;
}

/* Page flip */
int
drm_page_flip(struct drm_device *dev, uint32_t crtc_id, uint32_t fb_id, uint32_t flags, void *user_data)
{
    if (!dev)
        return -EINVAL;

    /* In a real implementation, we would call drmModePageFlip */
    /* For now, we return success */
    return 0;
}

/* Wait for vblank */
int
drm_wait_vblank(struct drm_device *dev, uint32_t crtc_id)
{
    if (!dev)
        return -EINVAL;

    /* In a real implementation, we would call drmWaitVBlank */
    /* For now, we return success */
    return 0;
}

/* Create framebuffer (dumb buffer via GBM) */
uint32_t
drm_create_fb(struct drm_device *dev, uint32_t width, uint32_t height, uint32_t format)
{
    struct gbm_device *gbm;
    struct gbm_bo *bo;
    uint32_t handle, pitch;
    uint32_t fb_id = 0;
    int ret;

    if (!dev)
        return 0;

    /* Create GBM device from DRM fd */
    gbm = gbm_device_create(dev->fd);
    if (!gbm)
        return 0;

    /* Create GBM BO */
    bo = gbm_bo_create(gbm, width, height, format,
                       GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    if (!bo) {
        gbm_device_destroy(gbm);
        return 0;
    }

    /* Get handle and pitch */
    handle = gbm_bo_get_handle(bo).u32;
    pitch = gbm_bo_get_stride(bo);

    /* Create DRM framebuffer */
    ret = drmModeAddFB(dev->fd, width, height, 24, 32, pitch, handle, &fb_id);
    if (ret) {
        gbm_bo_destroy(bo);
        gbm_device_destroy(gbm);
        return 0;
    }

    /* We don't keep the BO around; the FB is backed by it */
    /* In a real implementation, we would associate the BO with the FB for cleanup */
    gbm_bo_destroy(bo);
    gbm_device_destroy(gbm);

    return fb_id;
}

/* Destroy framebuffer */
int
drm_destroy_fb(struct drm_device *dev, uint32_t fb_id)
{
    if (!dev)
        return -EINVAL;

    return drmModeRmFB(dev->fd, fb_id);
}