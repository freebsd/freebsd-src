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

/** @file drm_bufs.c
 * Implementation of the ioctls for setup of DRM mappings and DMA buffers.
 */

#include "dev/pci/pcireg.h"

#include "dev/drm/drmP.h"

/* Allocation of PCI memory resources (framebuffer, registers, etc.) for
 * drm_get_resource_*.  Note that they are not RF_ACTIVE, so there's no virtual
 * address for accessing them.  Cleaned up at unload.
 */
static int drm_alloc_resource(struct drm_device *dev, int resource)
{
	struct resource *res;
	int rid;

	DRM_SPINLOCK_ASSERT(&dev->dev_lock);

	if (resource >= DRM_MAX_PCI_RESOURCE) {
		DRM_ERROR("Resource %d too large\n", resource);
		return 1;
	}

	if (dev->pcir[resource] != NULL) {
		return 0;
	}

	DRM_UNLOCK();
	rid = PCIR_BAR(resource);
	res = bus_alloc_resource_any(dev->device, SYS_RES_MEMORY, &rid,
	    RF_SHAREABLE);
	DRM_LOCK();
	if (res == NULL) {
		DRM_ERROR("Couldn't find resource 0x%x\n", resource);
		return 1;
	}

	if (dev->pcir[resource] == NULL) {
		dev->pcirid[resource] = rid;
		dev->pcir[resource] = res;
	}

	return 0;
}

unsigned long drm_get_resource_start(struct drm_device *dev,
				     unsigned int resource)
{
	if (drm_alloc_resource(dev, resource) != 0)
		return 0;

	return rman_get_start(dev->pcir[resource]);
}

unsigned long drm_get_resource_len(struct drm_device *dev,
				   unsigned int resource)
{
	if (drm_alloc_resource(dev, resource) != 0)
		return 0;

	return rman_get_size(dev->pcir[resource]);
}

int drm_addmap(struct drm_device * dev, unsigned long offset,
	       unsigned long size,
    enum drm_map_type type, enum drm_map_flags flags, drm_local_map_t **map_ptr)
{
	drm_local_map_t *map;
	int align;
	/*drm_agp_mem_t *entry;
	int valid;*/

	/* Only allow shared memory to be removable since we only keep enough
	 * book keeping information about shared memory to allow for removal
	 * when processes fork.
	 */
	if ((flags & _DRM_REMOVABLE) && type != _DRM_SHM) {
		DRM_ERROR("Requested removable map for non-DRM_SHM\n");
		return EINVAL;
	}
	if ((offset & PAGE_MASK) || (size & PAGE_MASK)) {
		DRM_ERROR("offset/size not page aligned: 0x%lx/0x%lx\n",
		    offset, size);
		return EINVAL;
	}
	if (offset + size < offset) {
		DRM_ERROR("offset and size wrap around: 0x%lx/0x%lx\n",
		    offset, size);
		return EINVAL;
	}

	DRM_DEBUG("offset = 0x%08lx, size = 0x%08lx, type = %d\n", offset,
	    size, type);

	/* Check if this is just another version of a kernel-allocated map, and
	 * just hand that back if so.
	 */
	if (type == _DRM_REGISTERS || type == _DRM_FRAME_BUFFER ||
	    type == _DRM_SHM) {
		TAILQ_FOREACH(map, &dev->maplist, link) {
			if (map->type == type && (map->offset == offset ||
			    (map->type == _DRM_SHM &&
			    map->flags == _DRM_CONTAINS_LOCK))) {
				map->size = size;
				DRM_DEBUG("Found kernel map %d\n", type);
				goto done;
			}
		}
	}
	DRM_UNLOCK();

	/* Allocate a new map structure, fill it in, and do any type-specific
	 * initialization necessary.
	 */
	map = malloc(sizeof(*map), DRM_MEM_MAPS, M_ZERO | M_NOWAIT);
	if (!map) {
		DRM_LOCK();
		return ENOMEM;
	}

	map->offset = offset;
	map->size = size;
	map->type = type;
	map->flags = flags;
	map->handle = (void *)((unsigned long)alloc_unr(dev->map_unrhdr) <<
	    DRM_MAP_HANDLE_SHIFT);

	switch (map->type) {
	case _DRM_REGISTERS:
		map->virtual = drm_ioremap(dev, map);
		if (!(map->flags & _DRM_WRITE_COMBINING))
			break;
		/* FALLTHROUGH */
	case _DRM_FRAME_BUFFER:
		if (drm_mtrr_add(map->offset, map->size, DRM_MTRR_WC) == 0)
			map->mtrr = 1;
		break;
	case _DRM_SHM:
		map->virtual = malloc(map->size, DRM_MEM_MAPS, M_NOWAIT);
		DRM_DEBUG("%lu %d %p\n",
		    map->size, drm_order(map->size), map->virtual);
		if (!map->virtual) {
			free(map, DRM_MEM_MAPS);
			DRM_LOCK();
			return ENOMEM;
		}
		map->offset = (unsigned long)map->virtual;
		if (map->flags & _DRM_CONTAINS_LOCK) {
			/* Prevent a 2nd X Server from creating a 2nd lock */
			DRM_LOCK();
			if (dev->lock.hw_lock != NULL) {
				DRM_UNLOCK();
				free(map->virtual, DRM_MEM_MAPS);
				free(map, DRM_MEM_MAPS);
				return EBUSY;
			}
			dev->lock.hw_lock = map->virtual; /* Pointer to lock */
			DRM_UNLOCK();
		}
		break;
	case _DRM_AGP:
		/*valid = 0;*/
		/* In some cases (i810 driver), user space may have already
		 * added the AGP base itself, because dev->agp->base previously
		 * only got set during AGP enable.  So, only add the base
		 * address if the map's offset isn't already within the
		 * aperture.
		 */
		if (map->offset < dev->agp->base ||
		    map->offset > dev->agp->base +
		    dev->agp->info.ai_aperture_size - 1) {
			map->offset += dev->agp->base;
		}
		map->mtrr   = dev->agp->mtrr; /* for getmap */
		/*for (entry = dev->agp->memory; entry; entry = entry->next) {
			if ((map->offset >= entry->bound) &&
			    (map->offset + map->size <=
			    entry->bound + entry->pages * PAGE_SIZE)) {
				valid = 1;
				break;
			}
		}
		if (!valid) {
			free(map, DRM_MEM_MAPS);
			DRM_LOCK();
			return EACCES;
		}*/
		break;
	case _DRM_SCATTER_GATHER:
		if (!dev->sg) {
			free(map, DRM_MEM_MAPS);
			DRM_LOCK();
			return EINVAL;
		}
		map->virtual = (void *)(dev->sg->vaddr + offset);
		map->offset = dev->sg->vaddr + offset;
		break;
	case _DRM_CONSISTENT:
		/* Unfortunately, we don't get any alignment specification from
		 * the caller, so we have to guess.  drm_pci_alloc requires
		 * a power-of-two alignment, so try to align the bus address of
		 * the map to it size if possible, otherwise just assume
		 * PAGE_SIZE alignment.
		 */
		align = map->size;
		if ((align & (align - 1)) != 0)
			align = PAGE_SIZE;
		map->dmah = drm_pci_alloc(dev, map->size, align, 0xfffffffful);
		if (map->dmah == NULL) {
			free(map, DRM_MEM_MAPS);
			DRM_LOCK();
			return ENOMEM;
		}
		map->virtual = map->dmah->vaddr;
		map->offset = map->dmah->busaddr;
		break;
	default:
		DRM_ERROR("Bad map type %d\n", map->type);
		free(map, DRM_MEM_MAPS);
		DRM_LOCK();
		return EINVAL;
	}

	DRM_LOCK();
	TAILQ_INSERT_TAIL(&dev->maplist, map, link);

done:
	/* Jumped to, with lock held, when a kernel map is found. */

	DRM_DEBUG("Added map %d 0x%lx/0x%lx\n", map->type, map->offset,
	    map->size);

	*map_ptr = map;

	return 0;
}

int drm_addmap_ioctl(struct drm_device *dev, void *data,
		     struct drm_file *file_priv)
{
	struct drm_map *request = data;
	drm_local_map_t *map;
	int err;

	if (!(dev->flags & (FREAD|FWRITE)))
		return EACCES; /* Require read/write */

	if (!DRM_SUSER(DRM_CURPROC) && request->type != _DRM_AGP)
		return EACCES;

	DRM_LOCK();
	err = drm_addmap(dev, request->offset, request->size, request->type,
	    request->flags, &map);
	DRM_UNLOCK();
	if (err != 0)
		return err;

	request->offset = map->offset;
	request->size = map->size;
	request->type = map->type;
	request->flags = map->flags;
	request->mtrr   = map->mtrr;
	request->handle = (void *)map->handle;

	return 0;
}

void drm_rmmap(struct drm_device *dev, drm_local_map_t *map)
{
	DRM_SPINLOCK_ASSERT(&dev->dev_lock);

	if (map == NULL)
		return;

	TAILQ_REMOVE(&dev->maplist, map, link);

	switch (map->type) {
	case _DRM_REGISTERS:
		if (map->bsr == NULL)
			drm_ioremapfree(map);
		/* FALLTHROUGH */
	case _DRM_FRAME_BUFFER:
		if (map->mtrr) {
			int __unused retcode;
			
			retcode = drm_mtrr_del(0, map->offset, map->size,
			    DRM_MTRR_WC);
			DRM_DEBUG("mtrr_del = %d\n", retcode);
		}
		break;
	case _DRM_SHM:
		free(map->virtual, DRM_MEM_MAPS);
		break;
	case _DRM_AGP:
	case _DRM_SCATTER_GATHER:
		break;
	case _DRM_CONSISTENT:
		drm_pci_free(dev, map->dmah);
		break;
	default:
		DRM_ERROR("Bad map type %d\n", map->type);
		break;
	}

	if (map->bsr != NULL) {
		bus_release_resource(dev->device, SYS_RES_MEMORY, map->rid,
		    map->bsr);
	}

	DRM_UNLOCK();
	if (map->handle)
		free_unr(dev->map_unrhdr, (unsigned long)map->handle >>
		    DRM_MAP_HANDLE_SHIFT);
	DRM_LOCK();

	free(map, DRM_MEM_MAPS);
}

/* Remove a map private from list and deallocate resources if the mapping
 * isn't in use.
 */

int drm_rmmap_ioctl(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	drm_local_map_t *map;
	struct drm_map *request = data;

	DRM_LOCK();
	TAILQ_FOREACH(map, &dev->maplist, link) {
		if (map->handle == request->handle &&
		    map->flags & _DRM_REMOVABLE)
			break;
	}

	/* No match found. */
	if (map == NULL) {
		DRM_UNLOCK();
		return EINVAL;
	}

	drm_rmmap(dev, map);

	DRM_UNLOCK();

	return 0;
}


static void drm_cleanup_buf_error(struct drm_device *dev,
				  drm_buf_entry_t *entry)
{
	int i;

	if (entry->seg_count) {
		for (i = 0; i < entry->seg_count; i++) {
			drm_pci_free(dev, entry->seglist[i]);
		}
		free(entry->seglist, DRM_MEM_SEGS);

		entry->seg_count = 0;
	}

   	if (entry->buf_count) {
	   	for (i = 0; i < entry->buf_count; i++) {
			free(entry->buflist[i].dev_private, DRM_MEM_BUFS);
		}
		free(entry->buflist, DRM_MEM_BUFS);

		entry->buf_count = 0;
	}
}

static int drm_do_addbufs_agp(struct drm_device *dev, struct drm_buf_desc *request)
{
	drm_device_dma_t *dma = dev->dma;
	drm_buf_entry_t *entry;
	/*drm_agp_mem_t *agp_entry;
	int valid*/
	drm_buf_t *buf;
	unsigned long offset;
	unsigned long agp_offset;
	int count;
	int order;
	int size;
	int alignment;
	int page_order;
	int total;
	int byte_count;
	int i;
	drm_buf_t **temp_buflist;

	count = request->count;
	order = drm_order(request->size);
	size = 1 << order;

	alignment  = (request->flags & _DRM_PAGE_ALIGN)
	    ? round_page(size) : size;
	page_order = order - PAGE_SHIFT > 0 ? order - PAGE_SHIFT : 0;
	total = PAGE_SIZE << page_order;

	byte_count = 0;
	agp_offset = dev->agp->base + request->agp_start;

	DRM_DEBUG("count:      %d\n",  count);
	DRM_DEBUG("order:      %d\n",  order);
	DRM_DEBUG("size:       %d\n",  size);
	DRM_DEBUG("agp_offset: 0x%lx\n", agp_offset);
	DRM_DEBUG("alignment:  %d\n",  alignment);
	DRM_DEBUG("page_order: %d\n",  page_order);
	DRM_DEBUG("total:      %d\n",  total);

	/* Make sure buffers are located in AGP memory that we own */
	/* Breaks MGA due to drm_alloc_agp not setting up entries for the
	 * memory.  Safe to ignore for now because these ioctls are still
	 * root-only.
	 */
	/*valid = 0;
	for (agp_entry = dev->agp->memory; agp_entry;
	    agp_entry = agp_entry->next) {
		if ((agp_offset >= agp_entry->bound) &&
		    (agp_offset + total * count <=
		    agp_entry->bound + agp_entry->pages * PAGE_SIZE)) {
			valid = 1;
			break;
		}
	}
	if (!valid) {
		DRM_DEBUG("zone invalid\n");
		return EINVAL;
	}*/

	entry = &dma->bufs[order];

	entry->buflist = malloc(count * sizeof(*entry->buflist), DRM_MEM_BUFS,
	    M_NOWAIT | M_ZERO);
	if (!entry->buflist) {
		return ENOMEM;
	}

	entry->buf_size = size;
	entry->page_order = page_order;

	offset = 0;

	while (entry->buf_count < count) {
		buf          = &entry->buflist[entry->buf_count];
		buf->idx     = dma->buf_count + entry->buf_count;
		buf->total   = alignment;
		buf->order   = order;
		buf->used    = 0;

		buf->offset  = (dma->byte_count + offset);
		buf->bus_address = agp_offset + offset;
		buf->address = (void *)(agp_offset + offset);
		buf->next    = NULL;
		buf->pending = 0;
		buf->file_priv = NULL;

		buf->dev_priv_size = dev->driver->buf_priv_size;
		buf->dev_private = malloc(buf->dev_priv_size, DRM_MEM_BUFS,
		    M_NOWAIT | M_ZERO);
		if (buf->dev_private == NULL) {
			/* Set count correctly so we free the proper amount. */
			entry->buf_count = count;
			drm_cleanup_buf_error(dev, entry);
			return ENOMEM;
		}

		offset += alignment;
		entry->buf_count++;
		byte_count += PAGE_SIZE << page_order;
	}

	DRM_DEBUG("byte_count: %d\n", byte_count);

	temp_buflist = realloc(dma->buflist,
	    (dma->buf_count + entry->buf_count) * sizeof(*dma->buflist),
	    DRM_MEM_BUFS, M_NOWAIT);
	if (temp_buflist == NULL) {
		/* Free the entry because it isn't valid */
		drm_cleanup_buf_error(dev, entry);
		return ENOMEM;
	}
	dma->buflist = temp_buflist;

	for (i = 0; i < entry->buf_count; i++) {
		dma->buflist[i + dma->buf_count] = &entry->buflist[i];
	}

	dma->buf_count += entry->buf_count;
	dma->byte_count += byte_count;

	DRM_DEBUG("dma->buf_count : %d\n", dma->buf_count);
	DRM_DEBUG("entry->buf_count : %d\n", entry->buf_count);

	request->count = entry->buf_count;
	request->size = size;

	dma->flags = _DRM_DMA_USE_AGP;

	return 0;
}

static int drm_do_addbufs_pci(struct drm_device *dev, struct drm_buf_desc *request)
{
	drm_device_dma_t *dma = dev->dma;
	int count;
	int order;
	int size;
	int total;
	int page_order;
	drm_buf_entry_t *entry;
	drm_buf_t *buf;
	int alignment;
	unsigned long offset;
	int i;
	int byte_count;
	int page_count;
	unsigned long *temp_pagelist;
	drm_buf_t **temp_buflist;

	count = request->count;
	order = drm_order(request->size);
	size = 1 << order;

	DRM_DEBUG("count=%d, size=%d (%d), order=%d\n",
	    request->count, request->size, size, order);

	alignment = (request->flags & _DRM_PAGE_ALIGN)
	    ? round_page(size) : size;
	page_order = order - PAGE_SHIFT > 0 ? order - PAGE_SHIFT : 0;
	total = PAGE_SIZE << page_order;

	entry = &dma->bufs[order];

	entry->buflist = malloc(count * sizeof(*entry->buflist), DRM_MEM_BUFS,
	    M_NOWAIT | M_ZERO);
	entry->seglist = malloc(count * sizeof(*entry->seglist), DRM_MEM_SEGS,
	    M_NOWAIT | M_ZERO);

	/* Keep the original pagelist until we know all the allocations
	 * have succeeded
	 */
	temp_pagelist = malloc((dma->page_count + (count << page_order)) *
	    sizeof(*dma->pagelist), DRM_MEM_PAGES, M_NOWAIT);

	if (entry->buflist == NULL || entry->seglist == NULL || 
	    temp_pagelist == NULL) {
		free(temp_pagelist, DRM_MEM_PAGES);
		free(entry->seglist, DRM_MEM_SEGS);
		free(entry->buflist, DRM_MEM_BUFS);
		return ENOMEM;
	}

	memcpy(temp_pagelist, dma->pagelist, dma->page_count * 
	    sizeof(*dma->pagelist));

	DRM_DEBUG("pagelist: %d entries\n",
	    dma->page_count + (count << page_order));

	entry->buf_size	= size;
	entry->page_order = page_order;
	byte_count = 0;
	page_count = 0;

	while (entry->buf_count < count) {
		DRM_SPINUNLOCK(&dev->dma_lock);
		drm_dma_handle_t *dmah = drm_pci_alloc(dev, size, alignment,
		    0xfffffffful);
		DRM_SPINLOCK(&dev->dma_lock);
		if (dmah == NULL) {
			/* Set count correctly so we free the proper amount. */
			entry->buf_count = count;
			entry->seg_count = count;
			drm_cleanup_buf_error(dev, entry);
			free(temp_pagelist, DRM_MEM_PAGES);
			return ENOMEM;
		}

		entry->seglist[entry->seg_count++] = dmah;
		for (i = 0; i < (1 << page_order); i++) {
			DRM_DEBUG("page %d @ %p\n",
			    dma->page_count + page_count,
			    (char *)dmah->vaddr + PAGE_SIZE * i);
			temp_pagelist[dma->page_count + page_count++] = 
			    (long)dmah->vaddr + PAGE_SIZE * i;
		}
		for (offset = 0;
		    offset + size <= total && entry->buf_count < count;
		    offset += alignment, ++entry->buf_count) {
			buf	     = &entry->buflist[entry->buf_count];
			buf->idx     = dma->buf_count + entry->buf_count;
			buf->total   = alignment;
			buf->order   = order;
			buf->used    = 0;
			buf->offset  = (dma->byte_count + byte_count + offset);
			buf->address = ((char *)dmah->vaddr + offset);
			buf->bus_address = dmah->busaddr + offset;
			buf->next    = NULL;
			buf->pending = 0;
			buf->file_priv = NULL;

			buf->dev_priv_size = dev->driver->buf_priv_size;
			buf->dev_private = malloc(buf->dev_priv_size,
			    DRM_MEM_BUFS, M_NOWAIT | M_ZERO);
			if (buf->dev_private == NULL) {
				/* Set count correctly so we free the proper amount. */
				entry->buf_count = count;
				entry->seg_count = count;
				drm_cleanup_buf_error(dev, entry);
				free(temp_pagelist, DRM_MEM_PAGES);
				return ENOMEM;
			}

			DRM_DEBUG("buffer %d @ %p\n",
			    entry->buf_count, buf->address);
		}
		byte_count += PAGE_SIZE << page_order;
	}

	temp_buflist = realloc(dma->buflist,
	    (dma->buf_count + entry->buf_count) * sizeof(*dma->buflist),
	    DRM_MEM_BUFS, M_NOWAIT);
	if (temp_buflist == NULL) {
		/* Free the entry because it isn't valid */
		drm_cleanup_buf_error(dev, entry);
		free(temp_pagelist, DRM_MEM_PAGES);
		return ENOMEM;
	}
	dma->buflist = temp_buflist;

	for (i = 0; i < entry->buf_count; i++) {
		dma->buflist[i + dma->buf_count] = &entry->buflist[i];
	}

	/* No allocations failed, so now we can replace the orginal pagelist
	 * with the new one.
	 */
	free(dma->pagelist, DRM_MEM_PAGES);
	dma->pagelist = temp_pagelist;

	dma->buf_count += entry->buf_count;
	dma->seg_count += entry->seg_count;
	dma->page_count += entry->seg_count << page_order;
	dma->byte_count += PAGE_SIZE * (entry->seg_count << page_order);

	request->count = entry->buf_count;
	request->size = size;

	return 0;

}

static int drm_do_addbufs_sg(struct drm_device *dev, struct drm_buf_desc *request)
{
	drm_device_dma_t *dma = dev->dma;
	drm_buf_entry_t *entry;
	drm_buf_t *buf;
	unsigned long offset;
	unsigned long agp_offset;
	int count;
	int order;
	int size;
	int alignment;
	int page_order;
	int total;
	int byte_count;
	int i;
	drm_buf_t **temp_buflist;

	count = request->count;
	order = drm_order(request->size);
	size = 1 << order;

	alignment  = (request->flags & _DRM_PAGE_ALIGN)
	    ? round_page(size) : size;
	page_order = order - PAGE_SHIFT > 0 ? order - PAGE_SHIFT : 0;
	total = PAGE_SIZE << page_order;

	byte_count = 0;
	agp_offset = request->agp_start;

	DRM_DEBUG("count:      %d\n",  count);
	DRM_DEBUG("order:      %d\n",  order);
	DRM_DEBUG("size:       %d\n",  size);
	DRM_DEBUG("agp_offset: %ld\n", agp_offset);
	DRM_DEBUG("alignment:  %d\n",  alignment);
	DRM_DEBUG("page_order: %d\n",  page_order);
	DRM_DEBUG("total:      %d\n",  total);

	entry = &dma->bufs[order];

	entry->buflist = malloc(count * sizeof(*entry->buflist), DRM_MEM_BUFS,
	    M_NOWAIT | M_ZERO);
	if (entry->buflist == NULL)
		return ENOMEM;

	entry->buf_size = size;
	entry->page_order = page_order;

	offset = 0;

	while (entry->buf_count < count) {
		buf          = &entry->buflist[entry->buf_count];
		buf->idx     = dma->buf_count + entry->buf_count;
		buf->total   = alignment;
		buf->order   = order;
		buf->used    = 0;

		buf->offset  = (dma->byte_count + offset);
		buf->bus_address = agp_offset + offset;
		buf->address = (void *)(agp_offset + offset + dev->sg->vaddr);
		buf->next    = NULL;
		buf->pending = 0;
		buf->file_priv = NULL;

		buf->dev_priv_size = dev->driver->buf_priv_size;
		buf->dev_private = malloc(buf->dev_priv_size, DRM_MEM_BUFS,
		    M_NOWAIT | M_ZERO);
		if (buf->dev_private == NULL) {
			/* Set count correctly so we free the proper amount. */
			entry->buf_count = count;
			drm_cleanup_buf_error(dev, entry);
			return ENOMEM;
		}

		DRM_DEBUG("buffer %d @ %p\n",
		    entry->buf_count, buf->address);

		offset += alignment;
		entry->buf_count++;
		byte_count += PAGE_SIZE << page_order;
	}

	DRM_DEBUG("byte_count: %d\n", byte_count);

	temp_buflist = realloc(dma->buflist,
	    (dma->buf_count + entry->buf_count) * sizeof(*dma->buflist),
	    DRM_MEM_BUFS, M_NOWAIT);
	if (temp_buflist == NULL) {
		/* Free the entry because it isn't valid */
		drm_cleanup_buf_error(dev, entry);
		return ENOMEM;
	}
	dma->buflist = temp_buflist;

	for (i = 0; i < entry->buf_count; i++) {
		dma->buflist[i + dma->buf_count] = &entry->buflist[i];
	}

	dma->buf_count += entry->buf_count;
	dma->byte_count += byte_count;

	DRM_DEBUG("dma->buf_count : %d\n", dma->buf_count);
	DRM_DEBUG("entry->buf_count : %d\n", entry->buf_count);

	request->count = entry->buf_count;
	request->size = size;

	dma->flags = _DRM_DMA_USE_SG;

	return 0;
}

int drm_addbufs_agp(struct drm_device *dev, struct drm_buf_desc *request)
{
	int order, ret;

	if (request->count < 0 || request->count > 4096)
		return EINVAL;
	
	order = drm_order(request->size);
	if (order < DRM_MIN_ORDER || order > DRM_MAX_ORDER)
		return EINVAL;

	DRM_SPINLOCK(&dev->dma_lock);

	/* No more allocations after first buffer-using ioctl. */
	if (dev->buf_use != 0) {
		DRM_SPINUNLOCK(&dev->dma_lock);
		return EBUSY;
	}
	/* No more than one allocation per order */
	if (dev->dma->bufs[order].buf_count != 0) {
		DRM_SPINUNLOCK(&dev->dma_lock);
		return ENOMEM;
	}

	ret = drm_do_addbufs_agp(dev, request);

	DRM_SPINUNLOCK(&dev->dma_lock);

	return ret;
}

int drm_addbufs_sg(struct drm_device *dev, struct drm_buf_desc *request)
{
	int order, ret;

	if (!DRM_SUSER(DRM_CURPROC))
		return EACCES;

	if (request->count < 0 || request->count > 4096)
		return EINVAL;

	order = drm_order(request->size);
	if (order < DRM_MIN_ORDER || order > DRM_MAX_ORDER)
		return EINVAL;

	DRM_SPINLOCK(&dev->dma_lock);

	/* No more allocations after first buffer-using ioctl. */
	if (dev->buf_use != 0) {
		DRM_SPINUNLOCK(&dev->dma_lock);
		return EBUSY;
	}
	/* No more than one allocation per order */
	if (dev->dma->bufs[order].buf_count != 0) {
		DRM_SPINUNLOCK(&dev->dma_lock);
		return ENOMEM;
	}

	ret = drm_do_addbufs_sg(dev, request);

	DRM_SPINUNLOCK(&dev->dma_lock);

	return ret;
}

int drm_addbufs_pci(struct drm_device *dev, struct drm_buf_desc *request)
{
	int order, ret;

	if (!DRM_SUSER(DRM_CURPROC))
		return EACCES;

	if (request->count < 0 || request->count > 4096)
		return EINVAL;

	order = drm_order(request->size);
	if (order < DRM_MIN_ORDER || order > DRM_MAX_ORDER)
		return EINVAL;

	DRM_SPINLOCK(&dev->dma_lock);

	/* No more allocations after first buffer-using ioctl. */
	if (dev->buf_use != 0) {
		DRM_SPINUNLOCK(&dev->dma_lock);
		return EBUSY;
	}
	/* No more than one allocation per order */
	if (dev->dma->bufs[order].buf_count != 0) {
		DRM_SPINUNLOCK(&dev->dma_lock);
		return ENOMEM;
	}

	ret = drm_do_addbufs_pci(dev, request);

	DRM_SPINUNLOCK(&dev->dma_lock);

	return ret;
}

int drm_addbufs(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_buf_desc *request = data;
	int err;

	if (request->flags & _DRM_AGP_BUFFER)
		err = drm_addbufs_agp(dev, request);
	else if (request->flags & _DRM_SG_BUFFER)
		err = drm_addbufs_sg(dev, request);
	else
		err = drm_addbufs_pci(dev, request);

	return err;
}

int drm_infobufs(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_device_dma_t *dma = dev->dma;
	struct drm_buf_info *request = data;
	int i;
	int count;
	int retcode = 0;

	DRM_SPINLOCK(&dev->dma_lock);
	++dev->buf_use;		/* Can't allocate more after this call */
	DRM_SPINUNLOCK(&dev->dma_lock);

	for (i = 0, count = 0; i < DRM_MAX_ORDER + 1; i++) {
		if (dma->bufs[i].buf_count)
			++count;
	}

	DRM_DEBUG("count = %d\n", count);

	if (request->count >= count) {
		for (i = 0, count = 0; i < DRM_MAX_ORDER + 1; i++) {
			if (dma->bufs[i].buf_count) {
				struct drm_buf_desc from;

				from.count = dma->bufs[i].buf_count;
				from.size = dma->bufs[i].buf_size;
				from.low_mark = dma->bufs[i].freelist.low_mark;
				from.high_mark = dma->bufs[i].freelist.high_mark;

				if (DRM_COPY_TO_USER(&request->list[count], &from,
				    sizeof(struct drm_buf_desc)) != 0) {
					retcode = EFAULT;
					break;
				}

				DRM_DEBUG("%d %d %d %d %d\n",
				    i, dma->bufs[i].buf_count,
				    dma->bufs[i].buf_size,
				    dma->bufs[i].freelist.low_mark,
				    dma->bufs[i].freelist.high_mark);
				++count;
			}
		}
	}
	request->count = count;

	return retcode;
}

int drm_markbufs(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_device_dma_t *dma = dev->dma;
	struct drm_buf_desc *request = data;
	int order;

	DRM_DEBUG("%d, %d, %d\n",
		  request->size, request->low_mark, request->high_mark);
	

	order = drm_order(request->size);	
	if (order < DRM_MIN_ORDER || order > DRM_MAX_ORDER ||
	    request->low_mark < 0 || request->high_mark < 0) {
		return EINVAL;
	}

	DRM_SPINLOCK(&dev->dma_lock);
	if (request->low_mark > dma->bufs[order].buf_count ||
	    request->high_mark > dma->bufs[order].buf_count) {
		DRM_SPINUNLOCK(&dev->dma_lock);
		return EINVAL;
	}

	dma->bufs[order].freelist.low_mark  = request->low_mark;
	dma->bufs[order].freelist.high_mark = request->high_mark;
	DRM_SPINUNLOCK(&dev->dma_lock);

	return 0;
}

int drm_freebufs(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_device_dma_t *dma = dev->dma;
	struct drm_buf_free *request = data;
	int i;
	int idx;
	drm_buf_t *buf;
	int retcode = 0;

	DRM_DEBUG("%d\n", request->count);
	
	DRM_SPINLOCK(&dev->dma_lock);
	for (i = 0; i < request->count; i++) {
		if (DRM_COPY_FROM_USER(&idx, &request->list[i], sizeof(idx))) {
			retcode = EFAULT;
			break;
		}
		if (idx < 0 || idx >= dma->buf_count) {
			DRM_ERROR("Index %d (of %d max)\n",
			    idx, dma->buf_count - 1);
			retcode = EINVAL;
			break;
		}
		buf = dma->buflist[idx];
		if (buf->file_priv != file_priv) {
			DRM_ERROR("Process %d freeing buffer not owned\n",
			    DRM_CURRENTPID);
			retcode = EINVAL;
			break;
		}
		drm_free_buffer(dev, buf);
	}
	DRM_SPINUNLOCK(&dev->dma_lock);

	return retcode;
}

int drm_mapbufs(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_device_dma_t *dma = dev->dma;
	int retcode = 0;
	const int zero = 0;
	vm_offset_t address;
	struct vmspace *vms;
	vm_ooffset_t foff;
	vm_size_t size;
	vm_offset_t vaddr;
	struct drm_buf_map *request = data;
	int i;

	vms = DRM_CURPROC->td_proc->p_vmspace;

	DRM_SPINLOCK(&dev->dma_lock);
	dev->buf_use++;		/* Can't allocate more after this call */
	DRM_SPINUNLOCK(&dev->dma_lock);

	if (request->count < dma->buf_count)
		goto done;

	if ((drm_core_has_AGP(dev) && (dma->flags & _DRM_DMA_USE_AGP)) ||
	    (drm_core_check_feature(dev, DRIVER_SG) &&
	    (dma->flags & _DRM_DMA_USE_SG))) {
		drm_local_map_t *map = dev->agp_buffer_map;

		if (map == NULL) {
			retcode = EINVAL;
			goto done;
		}
		size = round_page(map->size);
		foff = (unsigned long)map->handle;
	} else {
		size = round_page(dma->byte_count),
		foff = 0;
	}

	vaddr = round_page((vm_offset_t)vms->vm_daddr + MAXDSIZ);
#if __FreeBSD_version >= 600023
	retcode = vm_mmap(&vms->vm_map, &vaddr, size, PROT_READ | PROT_WRITE,
	    VM_PROT_ALL, MAP_SHARED | MAP_NOSYNC, OBJT_DEVICE,
	    dev->devnode, foff);
#else
	retcode = vm_mmap(&vms->vm_map, &vaddr, size, PROT_READ | PROT_WRITE,
	    VM_PROT_ALL, MAP_SHARED | MAP_NOSYNC,
	    SLIST_FIRST(&dev->devnode->si_hlist), foff);
#endif
	if (retcode)
		goto done;

	request->virtual = (void *)vaddr;

	for (i = 0; i < dma->buf_count; i++) {
		if (DRM_COPY_TO_USER(&request->list[i].idx,
		    &dma->buflist[i]->idx, sizeof(request->list[0].idx))) {
			retcode = EFAULT;
			goto done;
		}
		if (DRM_COPY_TO_USER(&request->list[i].total,
		    &dma->buflist[i]->total, sizeof(request->list[0].total))) {
			retcode = EFAULT;
			goto done;
		}
		if (DRM_COPY_TO_USER(&request->list[i].used, &zero,
		    sizeof(zero))) {
			retcode = EFAULT;
			goto done;
		}
		address = vaddr + dma->buflist[i]->offset; /* *** */
		if (DRM_COPY_TO_USER(&request->list[i].address, &address,
		    sizeof(address))) {
			retcode = EFAULT;
			goto done;
		}
	}

 done:
	request->count = dma->buf_count;

	DRM_DEBUG("%d buffers, retcode = %d\n", request->count, retcode);

	return retcode;
}

/*
 * Compute order.  Can be made faster.
 */
int drm_order(unsigned long size)
{
	int order;

	if (size == 0)
		return 0;

	order = flsl(size) - 1;
	if (size & ~(1ul << order))
		++order;

	return order;
}
