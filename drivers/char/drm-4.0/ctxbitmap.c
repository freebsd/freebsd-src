/* ctxbitmap.c -- Context bitmap management -*- linux-c -*-
 * Created: Thu Jan 6 03:56:42 2000 by jhartmann@precisioninsight.com
 * 
 * Copyright 2000 Precision Insight, Inc., Cedar Park, Texas.
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
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Author: Jeff Hartmann <jhartmann@valinux.com>
 *
 */

#define __NO_VERSION__
#include "drmP.h"

void drm_ctxbitmap_free(drm_device_t *dev, int ctx_handle)
{
	if (ctx_handle < 0) goto failed;

	if (ctx_handle < DRM_MAX_CTXBITMAP) {
		clear_bit(ctx_handle, dev->ctx_bitmap);
		return;
	}
failed:
       	DRM_ERROR("Attempt to free invalid context handle: %d\n",
		  ctx_handle);
       	return;
}

int drm_ctxbitmap_next(drm_device_t *dev)
{
	int bit;

	bit = find_first_zero_bit(dev->ctx_bitmap, DRM_MAX_CTXBITMAP);
	if (bit < DRM_MAX_CTXBITMAP) {
		set_bit(bit, dev->ctx_bitmap);
	   	DRM_DEBUG("drm_ctxbitmap_next bit : %d\n", bit);
		return bit;
	}
	return -1;
}

int drm_ctxbitmap_init(drm_device_t *dev)
{
	int i;
   	int temp;

	dev->ctx_bitmap = (unsigned long *) drm_alloc(PAGE_SIZE, 
						      DRM_MEM_CTXBITMAP);
	if(dev->ctx_bitmap == NULL) {
		return -ENOMEM;
	}
	memset((void *) dev->ctx_bitmap, 0, PAGE_SIZE);
	for(i = 0; i < DRM_RESERVED_CONTEXTS; i++) {
		temp = drm_ctxbitmap_next(dev);
	   	DRM_DEBUG("drm_ctxbitmap_init : %d\n", temp);
	}

	return 0;
}

void drm_ctxbitmap_cleanup(drm_device_t *dev)
{
	drm_free((void *)dev->ctx_bitmap, PAGE_SIZE,
		 DRM_MEM_CTXBITMAP);
}

