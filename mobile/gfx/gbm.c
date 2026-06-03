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

#include "gbm.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <gbm.h>

struct gbm_device {
    struct gbm_backend *backend;
    int fd;
};

struct gbm_surface {
    struct gbm_device *gbm;
    struct gbm_backend_surface *backend surface;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t flags;
};

struct gbm_bo {
    struct gbm_device *gbm;
    struct gbm_backend_bo *backend bo;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t flags;
};

/* Create GBM device from DRM fd */
struct gbm_device *
gbm_device_create(int fd)
{
    struct gbm_device *gbm;

    if (fd < 0)
        return NULL;

    gbm = calloc(1, sizeof(struct gbm_device));
    if (!gbm)
        return NULL;

    gbm->fd = fd;

    /* In a real implementation, we would initialize the GBM backend */
    /* For now, we use the real GBM library */
    gbm->backend = gbm_create_device(fd);
    if (!gbm->backend) {
        free(gbm);
        return NULL;
    }

    return gbm;
}

/* Destroy GBM device */
void
gbm_device_destroy(struct gbm_device *gbm)
{
    if (!gbm)
        return;

    if (gbm->backend)
        gbm_device_destroy(gbm->backend);
    free(gbm);
}

/* Create GBM surface */
struct gbm_surface *
gbm_surface_create(struct gbm_device *gbm, uint32_t width, uint32_t height,
                   uint32_t format, uint32_t flags)
{
    struct gbm_surface *surface;

    if (!gbm)
        return NULL;

    surface = calloc(1, sizeof(struct gbm_surface));
    if (!surface)
        return NULL;

    surface->gbm = gbm;
    surface->width = width;
    surface->height = height;
    surface->format = format;
    surface->flags = flags;

    /* In a real implementation, we would call gbm_surface_create */
    surface->backend surface = gbm_surface_create(gbm->backend, width, height, format, flags);
    if (!surface->backend surface) {
        free(surface);
        return NULL;
    }

    return surface;
}

/* Destroy GBM surface */
void
gbm_surface_destroy(struct gbm_surface *surface)
{
    if (!surface)
        return;

    if (surface->backend surface)
        gbm_surface_destroy(surface->backend surface);
    free(surface);
}

/* Lock front buffer */
struct gbm_bo *
gbm_surface_lock_front_buffer(struct gbm_surface *surface)
{
    if (!surface || !surface->backend surface)
        return NULL;

    return gbm_surface_lock_front_buffer(surface->backend surface);
}

/* Release buffer */
void
gbm_surface_release_buffer(struct gbm_surface *surface, struct gbm_bo *bo)
{
    if (!surface || !bo || !surface->backend surface)
        return;

    gbm_surface_release_buffer(surface->backend surface, bo->backend bo);
}

/* Create GBM BO */
struct gbm_bo *
gbm_bo_create(struct gbm_device *gbm, uint32_t width, uint32_t height,
              uint32_t format, uint32_t flags)
{
    struct gbm_bo *bo;

    if (!gbm)
        return NULL;

    bo = calloc(1, sizeof(struct gbm_bo));
    if (!bo)
        return NULL;

    bo->gbm = gbm;
    bo->width = width;
    bo->height = height;
    bo->format = format;
    bo->flags = flags;

    /* In a real implementation, we would call gbm_bo_create */
    bo->backend bo = gbm_bo_create(gbm->backend, width, height, format, flags);
    if (!bo->backend bo) {
        free(bo);
        return NULL;
    }

    return bo;
}

/* Destroy GBM BO */
void
gbm_bo_destroy(struct gbm_bo *bo)
{
    if (!bo)
        return;

    if (bo->backend bo)
        gbm_bo_destroy(bo->backend bo);
    free(bo);
}

/* Write to BO */
int
gbm_bo_write(struct gbm_bo *bo, const void *buf, size_t count)
{
    if (!bo || !buf)
        return -EINVAL;

    /* In a real implementation, we would map the BO, copy, and unmap */
    void *map = gbm_bo_map(bo, NULL, NULL, NULL, NULL, NULL);
    if (!map)
        return -errno;

    size_t max = gbm_bo_get_stride(bo) * gbm_bo_get_height(bo);
    if (count > max)
        count = max;

    memcpy(map, buf, count);
    gbm_bo_unmap(bo);
    return 0;
}

/* Map BO for CPU access */
void *
gbm_bo_map(struct gbm_bo *bo, uint32_t *stride, uint32_t *x, uint32_t *y,
           uint32_t *width, uint32_t *height)
{
    if (!bo)
        return NULL;

    /* In a real implementation, we would call gbm_bo_map */
    return gbm_bo_map(bo->backend bo, stride, x, y, width, height);
}

/* Unmap BO */
void
gbm_bo_unmap(struct gbm_bo *bo)
{
    if (!bo)
        return;

    gbm_bo_unmap(bo->backend bo);
}

/* Format helpers */
uint32_t
gbm_bo_get_format(struct gbm_bo *bo)
{
    return bo ? bo->format : 0;
}

uint32_t
gbm_bo_get_stride(struct gbm_bo *bo)
{
    return bo ? gbm_bo_get_stride(bo->backend bo) : 0;
}

uint32_t
gbm_bo_get_width(struct gbm_bo *bo)
{
    return bo ? bo->width : 0;
}

uint32_t
gbm_bo_get_height(struct gbm_bo *bo)
{
    return bo ? bo->height : 0;
}

uint32_t
gbm_bo_get_handle(struct gbm_bo *bo)
{
    return bo ? gbm_bo_get_handle(bo->backend bo).u32 : 0;
}

/* Surface helpers */
uint32_t
gbm_surface_get_width(struct gbm_surface *surface)
{
    return surface ? surface->width : 0;
}

uint32_t
gbm_surface_get_height(struct gbm_surface *surface)
{
    return surface ? surface->height : 0;
}

uint32_t
gbm_surface_get_format(struct gbm_surface *surface)
{
    return surface ? surface->format : 0;
}