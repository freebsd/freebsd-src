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

#ifndef _MOBILE_GFX_GBM_H_
#define _MOBILE_GFX_GBM_H_

#include <stdint.h>
#include <stdbool.h>

/* GBM flags */
#define GBM_BO_USE_SCANOUT        0x00000001
#define GBM_BO_USE_RENDERING      0x00000002
#define GBM_BO_USE_CURSOR         0x00000004
#define GBM_BO_USE_LINEAR         0x00000008
#define GBM_BO_USE_WRITE          0x00000010

/* GBM surface flags */
#define GBM_BO_USE_SHARED         0x00000020

/* Forward declarations */
struct gbm_device;
struct gbm_surface;
struct gbm_bo;

/* Public API */
struct gbm_device *gbm_device_create(int fd);
void gbm_device_destroy(struct gbm_device *gbm);

struct gbm_surface *gbm_surface_create(struct gbm_device *gbm,
                                       uint32_t width, uint32_t height,
                                       uint32_t format, uint32_t flags);
void gbm_surface_destroy(struct gbm_surface *surface);

struct gbm_bo *gbm_surface_lock_front_buffer(struct gbm_surface *surface);
void gbm_surface_release_buffer(struct gbm_surface *surface, struct gbm_bo *bo);

struct gbm_bo *gbm_bo_create(struct gbm_device *gbm,
                             uint32_t width, uint32_t height,
                             uint32_t format, uint32_t flags);
void gbm_bo_destroy(struct gbm_bo *bo);

int gbm_bo_write(struct gbm_bo *bo, const void *buf, size_t count);
void *gbm_bo_map(struct gbm_bo *bo, uint32_t *stride, uint32_t *x, uint32_t *y,
                 uint32_t *width, uint32_t *height);
void gbm_bo_unmap(struct gbm_bo *bo);

/* Format helpers */
uint32_t gbm_bo_get_format(struct gbm_bo *bo);
uint32_t gbm_bo_get_stride(struct gbm_bo *bo);
uint32_t gbm_bo_get_width(struct gbm_bo *bo);
uint32_t gbm_bo_get_height(struct gbm_bo *bo);
uint32_t gbm_bo_get_handle(struct gbm_bo *bo);

/* Surface helpers */
uint32_t gbm_surface_get_width(struct gbm_surface *surface);
uint32_t gbm_surface_get_height(struct gbm_surface *surface);
uint32_t gbm_surface_get_format(struct gbm_surface *surface);

#endif /* _MOBILE_GFX_GBM_H_ */