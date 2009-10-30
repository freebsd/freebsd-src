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

static void drm_sg_alloc_cb(void *arg, bus_dma_segment_t *segs,
			    int nsegs, int error);

int
drm_sg_alloc(struct drm_device *dev, struct drm_scatter_gather *request)
{
	struct drm_sg_mem *entry;
	struct drm_dma_handle *dmah;
	int ret;

	if (dev->sg)
		return EINVAL;

	entry = malloc(sizeof(*entry), DRM_MEM_SGLISTS, M_WAITOK | M_ZERO);
	entry->pages = round_page(request->size) / PAGE_SIZE;
	DRM_DEBUG("sg size=%ld pages=%d\n", request->size, entry->pages);

	entry->busaddr = malloc(entry->pages * sizeof(*entry->busaddr),
	    DRM_MEM_PAGES, M_WAITOK | M_ZERO);
	dmah = malloc(sizeof(struct drm_dma_handle), DRM_MEM_DMA,
	    M_WAITOK | M_ZERO);
	entry->dmah = dmah;

	ret = bus_dma_tag_create(NULL, PAGE_SIZE, 0, /* tag, align, boundary */
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, /* lowaddr, highaddr */
	    NULL, NULL, /* filtfunc, filtfuncargs */
	    request->size, pages, /* maxsize, nsegs */
	    PAGE_SIZE, 0, /* maxsegsize, flags */
	    NULL, NULL, /* lockfunc, lockfuncargs */
	    &dmah->tag);
	if (ret != 0) {
		drm_sg_cleanup(entry);
		return ENOMEM;
	}

	ret = bus_dmamem_alloc(dmah->tag, &dmah->vaddr,
	    BUS_DMA_WAITOK | BUS_DMA_ZERO | BUS_DMA_NOCACHE, &dmah->map);
	if (ret != 0) {
		drm_sg_cleanup(entry);
		return ENOMEM;
	}

	entry->handle = (unsigned long)dmah->vaddr;
	entry->virtual = dmah->vaddr;

	ret = bus_dmamap_load(dmah->tag, dmah->map, dmah->vaddr,
	    request->size, drm_sg_alloc_cb, entry, BUS_DMA_NOWAIT);
	if (ret != 0) {
		drm_sg_cleanup(entry);
		return ENOMEM;
	}

	DRM_LOCK();
	if (dev->sg) {
		DRM_UNLOCK();
		drm_sg_cleanup(entry);
		return EINVAL;
	}
	dev->sg = entry;
	DRM_UNLOCK();

	DRM_DEBUG("handle=%08lx, kva=%p, contents=%08lx\n", entry->handle,
	    entry->virtual, *(unsigned long *)entry->virtual);

	request->handle = entry->handle;

	return 0;
}

static void
drm_sg_alloc_cb(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	struct drm_sg_mem *entry = arg;
	int i;

	if (error != 0)
	    return;

	for(i = 0 ; i < nsegs ; i++) {
		entry->busaddr[i] = segs[i].ds_addr;
		DRM_DEBUG("segment %d @ 0x%016lx\n", i,
		    (unsigned long)segs[i].ds_addr);
	}
}

int
drm_sg_alloc_ioctl(struct drm_device *dev, void *data,
		   struct drm_file *file_priv)
{
	struct drm_scatter_gather *request = data;

	DRM_DEBUG("\n");

	return drm_sg_alloc(dev, request);
}

void
drm_sg_cleanup(struct drm_sg_mem *entry)
{
	struct drm_dma_handle *dmah = entry->dmah;

	if (dmah->map != NULL)
		bus_dmamap_unload(dmah->tag, dmah->map);
	if (dmah->vaddr != NULL)
		bus_dmamem_free(dmah->tag, dmah->vaddr, dmah->map);
	if (dmah->tag != NULL)
		bus_dma_tag_destroy(dmah->tag);
	free(dmah, DRM_MEM_DMA);
	free(entry->busaddr, DRM_MEM_PAGES);
	free(entry, DRM_MEM_SGLISTS);
}

int
drm_sg_free(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_scatter_gather *request = data;
	struct drm_sg_mem *entry;

	DRM_LOCK();
	entry = dev->sg;
	dev->sg = NULL;
	DRM_UNLOCK();

	if (!entry || entry->handle != request->handle)
		return EINVAL;

	DRM_DEBUG("sg free virtual = 0x%lx\n", entry->handle);

	drm_sg_cleanup(entry);

	return 0;
}
