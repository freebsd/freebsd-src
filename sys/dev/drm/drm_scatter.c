/*-
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
 * Authors:
 *   Gareth Hughes <gareth@valinux.com>
 *   Eric Anholt <anholt@FreeBSD.org>
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/** @file drm_scatter.c
 * Allocation of memory for scatter-gather mappings by the graphics chip.
 *
 * The memory allocated here is then made into an aperture in the card
 * by drm_ati_pcigart_init().
 */

#include "dev/drm/drmP.h"

#define DEBUG_SCATTER 0

void drm_sg_cleanup(drm_sg_mem_t *entry)
{
	free((void *)entry->handle, M_DRM);
	free(entry->busaddr, M_DRM);
	free(entry, M_DRM);
}

int drm_sg_alloc(struct drm_device * dev, drm_scatter_gather_t * request)
{
	drm_sg_mem_t *entry;
	unsigned long pages;
	int i;

	if ( dev->sg )
		return EINVAL;

	entry = malloc(sizeof(*entry), M_DRM, M_WAITOK | M_ZERO);
	if ( !entry )
		return ENOMEM;

	pages = round_page(request->size) / PAGE_SIZE;
	DRM_DEBUG( "sg size=%ld pages=%ld\n", request->size, pages );

	entry->pages = pages;

	entry->busaddr = malloc(pages * sizeof(*entry->busaddr), M_DRM,
	    M_WAITOK | M_ZERO);
	if ( !entry->busaddr ) {
		drm_sg_cleanup(entry);
		return ENOMEM;
	}

	entry->handle = (long)malloc(pages << PAGE_SHIFT, M_DRM,
	    M_WAITOK | M_ZERO);
	if (entry->handle == 0) {
		drm_sg_cleanup(entry);
		return ENOMEM;
	}

	for (i = 0; i < pages; i++) {
		entry->busaddr[i] = vtophys(entry->handle + i * PAGE_SIZE);
	}

	DRM_DEBUG( "sg alloc handle  = %08lx\n", entry->handle );

	entry->virtual = (void *)entry->handle;
	request->handle = entry->handle;

	DRM_LOCK();
	if (dev->sg) {
		DRM_UNLOCK();
		drm_sg_cleanup(entry);
		return EINVAL;
	}
	dev->sg = entry;
	DRM_UNLOCK();

	return 0;
}

int drm_sg_alloc_ioctl(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	drm_scatter_gather_t *request = data;
	int ret;

	DRM_DEBUG( "%s\n", __FUNCTION__ );

	ret = drm_sg_alloc(dev, request);
	return ret;
}

int drm_sg_free(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_scatter_gather_t *request = data;
	drm_sg_mem_t *entry;

	DRM_LOCK();
	entry = dev->sg;
	dev->sg = NULL;
	DRM_UNLOCK();

	if ( !entry || entry->handle != request->handle )
		return EINVAL;

	DRM_DEBUG( "sg free virtual  = 0x%lx\n", entry->handle );

	drm_sg_cleanup(entry);

	return 0;
}
