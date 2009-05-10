/*-
 * Copyright 1999, 2000 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/** @file drm_context.c
 * Implementation of the context management ioctls.
 */

#include "dev/drm/drmP.h"

/* ================================================================
 * Context bitmap support
 */

void drm_ctxbitmap_free(struct drm_device *dev, int ctx_handle)
{
	if (ctx_handle < 0 || ctx_handle >= DRM_MAX_CTXBITMAP || 
	    dev->ctx_bitmap == NULL) {
		DRM_ERROR("Attempt to free invalid context handle: %d\n",
		   ctx_handle);
		return;
	}

	DRM_LOCK();
	clear_bit(ctx_handle, dev->ctx_bitmap);
	dev->context_sareas[ctx_handle] = NULL;
	DRM_UNLOCK();
	return;
}

int drm_ctxbitmap_next(struct drm_device *dev)
{
	int bit;

	if (dev->ctx_bitmap == NULL)
		return -1;

	DRM_LOCK();
	bit = find_first_zero_bit(dev->ctx_bitmap, DRM_MAX_CTXBITMAP);
	if (bit >= DRM_MAX_CTXBITMAP) {
		DRM_UNLOCK();
		return -1;
	}

	set_bit(bit, dev->ctx_bitmap);
	DRM_DEBUG("drm_ctxbitmap_next bit : %d\n", bit);
	if ((bit+1) > dev->max_context) {
		dev->max_context = (bit+1);
		if (dev->context_sareas != NULL) {
			drm_local_map_t **ctx_sareas;

			ctx_sareas = realloc(dev->context_sareas,
			    dev->max_context * sizeof(*dev->context_sareas),
			    DRM_MEM_SAREA, M_NOWAIT);
			if (ctx_sareas == NULL) {
				clear_bit(bit, dev->ctx_bitmap);
				DRM_UNLOCK();
				return -1;
			}
			dev->context_sareas = ctx_sareas;
			dev->context_sareas[bit] = NULL;
		} else {
			/* max_context == 1 at this point */
			dev->context_sareas = malloc(dev->max_context * 
			    sizeof(*dev->context_sareas), DRM_MEM_SAREA,
			    M_NOWAIT);
			if (dev->context_sareas == NULL) {
				clear_bit(bit, dev->ctx_bitmap);
				DRM_UNLOCK();
				return -1;
			}
			dev->context_sareas[bit] = NULL;
		}
	}
	DRM_UNLOCK();
	return bit;
}

int drm_ctxbitmap_init(struct drm_device *dev)
{
	int i;
   	int temp;

	DRM_LOCK();
	dev->ctx_bitmap = malloc(PAGE_SIZE, DRM_MEM_CTXBITMAP,
	    M_NOWAIT | M_ZERO);
	if (dev->ctx_bitmap == NULL) {
		DRM_UNLOCK();
		return ENOMEM;
	}
	dev->context_sareas = NULL;
	dev->max_context = -1;
	DRM_UNLOCK();

	for (i = 0; i < DRM_RESERVED_CONTEXTS; i++) {
		temp = drm_ctxbitmap_next(dev);
		DRM_DEBUG("drm_ctxbitmap_init : %d\n", temp);
	}

	return 0;
}

void drm_ctxbitmap_cleanup(struct drm_device *dev)
{
	DRM_LOCK();
	if (dev->context_sareas != NULL)
		free(dev->context_sareas, DRM_MEM_SAREA);
	free(dev->ctx_bitmap, DRM_MEM_CTXBITMAP);
	DRM_UNLOCK();
}

/* ================================================================
 * Per Context SAREA Support
 */

int drm_getsareactx(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	struct drm_ctx_priv_map *request = data;
	drm_local_map_t *map;

	DRM_LOCK();
	if (dev->max_context < 0 ||
	    request->ctx_id >= (unsigned) dev->max_context) {
		DRM_UNLOCK();
		return EINVAL;
	}

	map = dev->context_sareas[request->ctx_id];
	DRM_UNLOCK();

	request->handle = map->handle;

	return 0;
}

int drm_setsareactx(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	struct drm_ctx_priv_map *request = data;
	drm_local_map_t *map = NULL;

	DRM_LOCK();
	TAILQ_FOREACH(map, &dev->maplist, link) {
		if (map->handle == request->handle) {
			if (dev->max_context < 0)
				goto bad;
			if (request->ctx_id >= (unsigned) dev->max_context)
				goto bad;
			dev->context_sareas[request->ctx_id] = map;
			DRM_UNLOCK();
			return 0;
		}
	}

bad:
	DRM_UNLOCK();
	return EINVAL;
}

/* ================================================================
 * The actual DRM context handling routines
 */

int drm_context_switch(struct drm_device *dev, int old, int new)
{
	if (test_and_set_bit(0, &dev->context_flag)) {
		DRM_ERROR("Reentering -- FIXME\n");
		return EBUSY;
	}

	DRM_DEBUG("Context switch from %d to %d\n", old, new);

	if (new == dev->last_context) {
		clear_bit(0, &dev->context_flag);
		return 0;
	}

	return 0;
}

int drm_context_switch_complete(struct drm_device *dev, int new)
{
	dev->last_context = new;  /* PRE/POST: This is the _only_ writer. */

	if (!_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)) {
		DRM_ERROR("Lock isn't held after context switch\n");
	}

	/* If a context switch is ever initiated
	   when the kernel holds the lock, release
	   that lock here. */
	clear_bit(0, &dev->context_flag);

	return 0;
}

int drm_resctx(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_ctx_res *res = data;
	struct drm_ctx ctx;
	int i;

	if (res->count >= DRM_RESERVED_CONTEXTS) {
		bzero(&ctx, sizeof(ctx));
		for (i = 0; i < DRM_RESERVED_CONTEXTS; i++) {
			ctx.handle = i;
			if (DRM_COPY_TO_USER(&res->contexts[i],
			    &ctx, sizeof(ctx)))
				return EFAULT;
		}
	}
	res->count = DRM_RESERVED_CONTEXTS;

	return 0;
}

int drm_addctx(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_ctx *ctx = data;

	ctx->handle = drm_ctxbitmap_next(dev);
	if (ctx->handle == DRM_KERNEL_CONTEXT) {
		/* Skip kernel's context and get a new one. */
		ctx->handle = drm_ctxbitmap_next(dev);
	}
	DRM_DEBUG("%d\n", ctx->handle);
	if (ctx->handle == -1) {
		DRM_DEBUG("Not enough free contexts.\n");
		/* Should this return -EBUSY instead? */
		return ENOMEM;
	}

	if (dev->driver->context_ctor && ctx->handle != DRM_KERNEL_CONTEXT) {
		DRM_LOCK();
		dev->driver->context_ctor(dev, ctx->handle);
		DRM_UNLOCK();
	}

	return 0;
}

int drm_modctx(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	/* This does nothing */
	return 0;
}

int drm_getctx(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_ctx *ctx = data;

	/* This is 0, because we don't handle any context flags */
	ctx->flags = 0;

	return 0;
}

int drm_switchctx(struct drm_device *dev, void *data,
		  struct drm_file *file_priv)
{
	struct drm_ctx *ctx = data;

	DRM_DEBUG("%d\n", ctx->handle);
	return drm_context_switch(dev, dev->last_context, ctx->handle);
}

int drm_newctx(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_ctx *ctx = data;

	DRM_DEBUG("%d\n", ctx->handle);
	drm_context_switch_complete(dev, ctx->handle);

	return 0;
}

int drm_rmctx(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_ctx *ctx = data;

	DRM_DEBUG("%d\n", ctx->handle);
	if (ctx->handle != DRM_KERNEL_CONTEXT) {
		if (dev->driver->context_dtor) {
			DRM_LOCK();
			dev->driver->context_dtor(dev, ctx->handle);
			DRM_UNLOCK();
		}

		drm_ctxbitmap_free(dev, ctx->handle);
	}

	return 0;
}
