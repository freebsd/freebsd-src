/* drm_bufs.h -- Generic buffer template -*- linux-c -*-
 * Created: Thu Nov 23 03:10:50 2000 by gareth@valinux.com */
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
 * $FreeBSD$
 */

#include "dev/drm/drmP.h"

#ifndef __HAVE_PCI_DMA
#define __HAVE_PCI_DMA		0
#endif

#ifndef __HAVE_SG
#define __HAVE_SG		0
#endif

#ifndef DRIVER_BUF_PRIV_T
#define DRIVER_BUF_PRIV_T		u32
#endif
#ifndef DRIVER_AGP_BUFFERS_MAP
#if __HAVE_AGP && __HAVE_DMA
#error "You must define DRIVER_AGP_BUFFERS_MAP()"
#else
#define DRIVER_AGP_BUFFERS_MAP( dev )	NULL
#endif
#endif

/*
 * Compute order.  Can be made faster.
 */
int DRM(order)( unsigned long size )
{
	int order;
	unsigned long tmp;

	for ( order = 0, tmp = size ; tmp >>= 1 ; ++order );

	if ( size & ~(1 << order) )
		++order;

	return order;
}

int DRM(addmap)( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_map_t request;
	drm_local_map_t *map;
	drm_map_list_entry_t *list;
	
	if (!(dev->flags & (FREAD|FWRITE)))
		return DRM_ERR(EACCES); /* Require read/write */

	DRM_COPY_FROM_USER_IOCTL( request, (drm_map_t *)data, sizeof(drm_map_t) );

	map = (drm_local_map_t *) DRM(alloc)( sizeof(*map), DRM_MEM_MAPS );
	if ( !map )
		return DRM_ERR(ENOMEM);

	map->offset = request.offset;
	map->size = request.size;
	map->type = request.type;
	map->flags = request.flags;
	map->mtrr = 0;
	map->handle = 0;
	
	/* Only allow shared memory to be removable since we only keep enough
	 * book keeping information about shared memory to allow for removal
	 * when processes fork.
	 */
	if ( (map->flags & _DRM_REMOVABLE) && map->type != _DRM_SHM ) {
		DRM(free)( map, sizeof(*map), DRM_MEM_MAPS );
		return DRM_ERR(EINVAL);
	}
	DRM_DEBUG( "offset = 0x%08lx, size = 0x%08lx, type = %d\n",
		   map->offset, map->size, map->type );
	if ( (map->offset & PAGE_MASK) || (map->size & PAGE_MASK) ) {
		DRM(free)( map, sizeof(*map), DRM_MEM_MAPS );
		return DRM_ERR(EINVAL);
	}
	if (map->offset + map->size < map->offset) {
		DRM(free)(map, sizeof(*map), DRM_MEM_MAPS);
		return DRM_ERR(EINVAL);
	}

	switch ( map->type ) {
	case _DRM_REGISTERS:
		DRM_IOREMAP(map, dev);
		if (!(map->flags & _DRM_WRITE_COMBINING))
			break;
		/* FALLTHROUGH */
	case _DRM_FRAME_BUFFER:
#if __REALLY_HAVE_MTRR
		if (DRM(mtrr_add)(map->offset, map->size, DRM_MTRR_WC) == 0)
			map->mtrr = 1;
#endif
		break;
	case _DRM_SHM:
		map->handle = (void *)DRM(alloc)(map->size, DRM_MEM_SAREA);
		DRM_DEBUG( "%lu %d %p\n",
			   map->size, DRM(order)( map->size ), map->handle );
		if ( !map->handle ) {
			DRM(free)( map, sizeof(*map), DRM_MEM_MAPS );
			return DRM_ERR(ENOMEM);
		}
		map->offset = (unsigned long)map->handle;
		if ( map->flags & _DRM_CONTAINS_LOCK ) {
			/* Prevent a 2nd X Server from creating a 2nd lock */
			DRM_LOCK();
			if (dev->lock.hw_lock != NULL) {
				DRM_UNLOCK();
				DRM(free)(map->handle, map->size,
				    DRM_MEM_SAREA);
				DRM(free)(map, sizeof(*map), DRM_MEM_MAPS);
				return DRM_ERR(EBUSY);
			}
			dev->lock.hw_lock = map->handle; /* Pointer to lock */
			DRM_UNLOCK();
		}
		break;
#if __REALLY_HAVE_AGP
	case _DRM_AGP:
		map->offset += dev->agp->base;
		map->mtrr   = dev->agp->mtrr; /* for getmap */
		break;
#endif
	case _DRM_SCATTER_GATHER:
		if (!dev->sg) {
			DRM(free)(map, sizeof(*map), DRM_MEM_MAPS);
			return DRM_ERR(EINVAL);
		}
		map->offset = map->offset + dev->sg->handle;
		break;

	default:
		DRM(free)( map, sizeof(*map), DRM_MEM_MAPS );
		return DRM_ERR(EINVAL);
	}

	list = DRM(calloc)(1, sizeof(*list), DRM_MEM_MAPS);
	if (list == NULL) {
		DRM(free)(map, sizeof(*map), DRM_MEM_MAPS);
		return DRM_ERR(EINVAL);
	}
	list->map = map;

	DRM_LOCK();
	TAILQ_INSERT_TAIL(dev->maplist, list, link);
	DRM_UNLOCK();

	request.offset = map->offset;
	request.size = map->size;
	request.type = map->type;
	request.flags = map->flags;
	request.mtrr   = map->mtrr;
	request.handle = map->handle;

	if ( request.type != _DRM_SHM ) {
		request.handle = (void *)request.offset;
	}

	DRM_COPY_TO_USER_IOCTL( (drm_map_t *)data, request, sizeof(drm_map_t) );

	return 0;
}


/* Remove a map private from list and deallocate resources if the mapping
 * isn't in use.
 */

int DRM(rmmap)( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_map_list_entry_t *list;
	drm_local_map_t *map;
	drm_map_t request;

	DRM_COPY_FROM_USER_IOCTL( request, (drm_map_t *)data, sizeof(request) );

	DRM_LOCK();
	TAILQ_FOREACH(list, dev->maplist, link) {
		map = list->map;
		if (map->handle == request.handle &&
		    map->flags & _DRM_REMOVABLE)
			break;
	}

	/* No match found. */
	if (list == NULL) {
		DRM_UNLOCK();
		return DRM_ERR(EINVAL);
	}
	TAILQ_REMOVE(dev->maplist, list, link);
	DRM_UNLOCK();

	DRM(free)(list, sizeof(*list), DRM_MEM_MAPS);

	switch (map->type) {
	case _DRM_REGISTERS:
	case _DRM_FRAME_BUFFER:
#if __REALLY_HAVE_MTRR
		if (map->mtrr) {
			int __unused retcode;
			
			retcode = DRM(mtrr_del)(map->offset, map->size,
			    DRM_MTRR_WC);
			DRM_DEBUG("mtrr_del = %d\n", retcode);
		}
#endif
		DRM(ioremapfree)(map);
		break;
	case _DRM_SHM:
		DRM(free)(map->handle, map->size, DRM_MEM_SAREA);
		break;
	case _DRM_AGP:
	case _DRM_SCATTER_GATHER:
		break;
	}
	DRM(free)(map, sizeof(*map), DRM_MEM_MAPS);
	return 0;
}

#if __HAVE_DMA


static void DRM(cleanup_buf_error)(drm_device_t *dev, drm_buf_entry_t *entry)
{
	int i;

#if __HAVE_PCI_DMA
	if (entry->seg_count) {
		for (i = 0; i < entry->seg_count; i++) {
			if (entry->seglist[i] != 0)
				DRM(pci_free)(dev, entry->buf_size,
				    (void *)entry->seglist[i],
				    entry->seglist_bus[i]);
		}
		DRM(free)(entry->seglist,
			  entry->seg_count *
			  sizeof(*entry->seglist),
			  DRM_MEM_SEGS);
		DRM(free)(entry->seglist_bus, entry->seg_count *
			  sizeof(*entry->seglist_bus), DRM_MEM_SEGS);

		entry->seg_count = 0;
	}
#endif /* __HAVE_PCI_DMA */

   	if (entry->buf_count) {
	   	for (i = 0; i < entry->buf_count; i++) {
			DRM(free)(entry->buflist[i].dev_private,
			    entry->buflist[i].dev_priv_size, DRM_MEM_BUFS);
		}
		DRM(free)(entry->buflist,
			  entry->buf_count *
			  sizeof(*entry->buflist),
			  DRM_MEM_BUFS);

		entry->buf_count = 0;
	}
}

#if __REALLY_HAVE_AGP
static int DRM(addbufs_agp)(drm_device_t *dev, drm_buf_desc_t *request)
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
	order = DRM(order)(request->size);
	size = 1 << order;

	alignment  = (request->flags & _DRM_PAGE_ALIGN)
		? round_page(size) : size;
	page_order = order - PAGE_SHIFT > 0 ? order - PAGE_SHIFT : 0;
	total = PAGE_SIZE << page_order;

	byte_count = 0;
	agp_offset = dev->agp->base + request->agp_start;

	DRM_DEBUG( "count:      %d\n",  count );
	DRM_DEBUG( "order:      %d\n",  order );
	DRM_DEBUG( "size:       %d\n",  size );
	DRM_DEBUG( "agp_offset: 0x%lx\n", agp_offset );
	DRM_DEBUG( "alignment:  %d\n",  alignment );
	DRM_DEBUG( "page_order: %d\n",  page_order );
	DRM_DEBUG( "total:      %d\n",  total );

	entry = &dma->bufs[order];

	entry->buflist = DRM(alloc)( count * sizeof(*entry->buflist),
				    DRM_MEM_BUFS );
	if ( !entry->buflist ) {
		return DRM_ERR(ENOMEM);
	}
	memset( entry->buflist, 0, count * sizeof(*entry->buflist) );

	entry->buf_size = size;
	entry->page_order = page_order;

	offset = 0;

	while ( entry->buf_count < count ) {
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
		buf->filp    = NULL;

		buf->dev_priv_size = sizeof(DRIVER_BUF_PRIV_T);
		buf->dev_private = DRM(calloc)(1, buf->dev_priv_size,
		    DRM_MEM_BUFS);
		if (buf->dev_private == NULL) {
			/* Set count correctly so we free the proper amount. */
			entry->buf_count = count;
			DRM(cleanup_buf_error)(dev, entry);
			return DRM_ERR(ENOMEM);
		}

		offset += alignment;
		entry->buf_count++;
		byte_count += PAGE_SIZE << page_order;
	}

	DRM_DEBUG( "byte_count: %d\n", byte_count );

	temp_buflist = DRM(realloc)( dma->buflist,
				     dma->buf_count * sizeof(*dma->buflist),
				     (dma->buf_count + entry->buf_count)
				     * sizeof(*dma->buflist),
				     DRM_MEM_BUFS );
	if (temp_buflist == NULL) {
		/* Free the entry because it isn't valid */
		DRM(cleanup_buf_error)(dev, entry);
		return DRM_ERR(ENOMEM);
	}
	dma->buflist = temp_buflist;

	for ( i = 0 ; i < entry->buf_count ; i++ ) {
		dma->buflist[i + dma->buf_count] = &entry->buflist[i];
	}

	dma->buf_count += entry->buf_count;
	dma->byte_count += byte_count;

	DRM_DEBUG( "dma->buf_count : %d\n", dma->buf_count );
	DRM_DEBUG( "entry->buf_count : %d\n", entry->buf_count );

	request->count = entry->buf_count;
	request->size = size;

	dma->flags = _DRM_DMA_USE_AGP;

	return 0;
}
#endif /* __REALLY_HAVE_AGP */

#if __HAVE_PCI_DMA
static int DRM(addbufs_pci)(drm_device_t *dev, drm_buf_desc_t *request)
{
	drm_device_dma_t *dma = dev->dma;
	int count;
	int order;
	int size;
	int total;
	int page_order;
	drm_buf_entry_t *entry;
	vm_offset_t vaddr;
	drm_buf_t *buf;
	int alignment;
	unsigned long offset;
	int i;
	int byte_count;
	int page_count;
	unsigned long *temp_pagelist;
	drm_buf_t **temp_buflist;
	dma_addr_t bus_addr;

	count = request->count;
	order = DRM(order)(request->size);
	size = 1 << order;

	DRM_DEBUG( "count=%d, size=%d (%d), order=%d\n",
		   request->count, request->size, size, order );

	alignment = (request->flags & _DRM_PAGE_ALIGN)
		? round_page(size) : size;
	page_order = order - PAGE_SHIFT > 0 ? order - PAGE_SHIFT : 0;
	total = PAGE_SIZE << page_order;

	entry = &dma->bufs[order];

	entry->buflist = DRM(alloc)(count * sizeof(*entry->buflist),
	    DRM_MEM_BUFS);
	entry->seglist = DRM(alloc)(count * sizeof(*entry->seglist),
	    DRM_MEM_SEGS);
	entry->seglist_bus = DRM(alloc)(count * sizeof(*entry->seglist_bus),
	    DRM_MEM_SEGS);

	/* Keep the original pagelist until we know all the allocations
	 * have succeeded
	 */
	temp_pagelist = DRM(alloc)((dma->page_count + (count << page_order)) *
	    sizeof(*dma->pagelist), DRM_MEM_PAGES);

	if (entry->buflist == NULL || entry->seglist == NULL || 
	    temp_pagelist == NULL) {
		DRM(free)(entry->buflist, count * sizeof(*entry->buflist),
		    DRM_MEM_BUFS);
		DRM(free)(entry->seglist, count * sizeof(*entry->seglist),
		    DRM_MEM_SEGS);
		DRM(free)(entry->seglist_bus, count *
		    sizeof(*entry->seglist_bus), DRM_MEM_SEGS);
		return DRM_ERR(ENOMEM);
	}

	bzero(entry->buflist, count * sizeof(*entry->buflist));
	bzero(entry->seglist, count * sizeof(*entry->seglist));
	
	memcpy(temp_pagelist, dma->pagelist, dma->page_count * 
	    sizeof(*dma->pagelist));

	DRM_DEBUG( "pagelist: %d entries\n",
		   dma->page_count + (count << page_order) );

	entry->buf_size	= size;
	entry->page_order = page_order;
	byte_count = 0;
	page_count = 0;

	while ( entry->buf_count < count ) {
		vaddr = (vm_offset_t) DRM(pci_alloc)(dev, size, alignment,
		    0xfffffffful, &bus_addr);
		if (vaddr == 0) {
			/* Set count correctly so we free the proper amount. */
			entry->buf_count = count;
			entry->seg_count = count;
			DRM(cleanup_buf_error)(dev, entry);
			DRM(free)(temp_pagelist, (dma->page_count +
			    (count << page_order)) * sizeof(*dma->pagelist),
			    DRM_MEM_PAGES);
			return DRM_ERR(ENOMEM);
		}
	
		entry->seglist_bus[entry->seg_count] = bus_addr;
		entry->seglist[entry->seg_count++] = vaddr;
		for ( i = 0 ; i < (1 << page_order) ; i++ ) {
			DRM_DEBUG( "page %d @ 0x%08lx\n",
				   dma->page_count + page_count,
				   (long)vaddr + PAGE_SIZE * i );
			temp_pagelist[dma->page_count + page_count++] = 
			    vaddr + PAGE_SIZE * i;
		}
		for ( offset = 0 ;
		      offset + size <= total && entry->buf_count < count ;
		      offset += alignment, ++entry->buf_count ) {
			buf	     = &entry->buflist[entry->buf_count];
			buf->idx     = dma->buf_count + entry->buf_count;
			buf->total   = alignment;
			buf->order   = order;
			buf->used    = 0;
			buf->offset  = (dma->byte_count + byte_count + offset);
			buf->address = (void *)(vaddr + offset);
			buf->bus_address = bus_addr + offset;
			buf->next    = NULL;
			buf->pending = 0;
			buf->filp    = NULL;

			buf->dev_priv_size = sizeof(DRIVER_BUF_PRIV_T);
			buf->dev_private = DRM(alloc)(sizeof(DRIVER_BUF_PRIV_T),
			    DRM_MEM_BUFS);
			if (buf->dev_private == NULL) {
				/* Set count correctly so we free the proper amount. */
				entry->buf_count = count;
				entry->seg_count = count;
				DRM(cleanup_buf_error)(dev, entry);
				DRM(free)(temp_pagelist, (dma->page_count + 
				    (count << page_order)) *
				    sizeof(*dma->pagelist), DRM_MEM_PAGES );
				return DRM_ERR(ENOMEM);
			}
			bzero(buf->dev_private, buf->dev_priv_size);

			DRM_DEBUG( "buffer %d @ %p\n",
				   entry->buf_count, buf->address );
		}
		byte_count += PAGE_SIZE << page_order;
	}

	temp_buflist = DRM(realloc)( dma->buflist,
				     dma->buf_count * sizeof(*dma->buflist),
				     (dma->buf_count + entry->buf_count)
				     * sizeof(*dma->buflist),
				     DRM_MEM_BUFS );
	if (temp_buflist == NULL) {
		/* Free the entry because it isn't valid */
		DRM(cleanup_buf_error)(dev, entry);
		DRM(free)(temp_pagelist, (dma->page_count + 
		    (count << page_order)) * sizeof(*dma->pagelist),
		    DRM_MEM_PAGES);
		return DRM_ERR(ENOMEM);
	}
	dma->buflist = temp_buflist;

	for ( i = 0 ; i < entry->buf_count ; i++ ) {
		dma->buflist[i + dma->buf_count] = &entry->buflist[i];
	}

	/* No allocations failed, so now we can replace the orginal pagelist
	 * with the new one.
	 */
	DRM(free)(dma->pagelist, dma->page_count * sizeof(*dma->pagelist),
	    DRM_MEM_PAGES);
	dma->pagelist = temp_pagelist;

	dma->buf_count += entry->buf_count;
	dma->seg_count += entry->seg_count;
	dma->page_count += entry->seg_count << page_order;
	dma->byte_count += PAGE_SIZE * (entry->seg_count << page_order);

	request->count = entry->buf_count;
	request->size = size;

	return 0;

}
#endif /* __HAVE_PCI_DMA */

#if __REALLY_HAVE_SG
static int DRM(addbufs_sg)(drm_device_t *dev, drm_buf_desc_t *request)
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
	order = DRM(order)(request->size);
	size = 1 << order;

	alignment  = (request->flags & _DRM_PAGE_ALIGN)
		? round_page(size) : size;
	page_order = order - PAGE_SHIFT > 0 ? order - PAGE_SHIFT : 0;
	total = PAGE_SIZE << page_order;

	byte_count = 0;
	agp_offset = request->agp_start;

	DRM_DEBUG( "count:      %d\n",  count );
	DRM_DEBUG( "order:      %d\n",  order );
	DRM_DEBUG( "size:       %d\n",  size );
	DRM_DEBUG( "agp_offset: %ld\n", agp_offset );
	DRM_DEBUG( "alignment:  %d\n",  alignment );
	DRM_DEBUG( "page_order: %d\n",  page_order );
	DRM_DEBUG( "total:      %d\n",  total );

	entry = &dma->bufs[order];

	entry->buflist = DRM(calloc)(1, count * sizeof(*entry->buflist),
	    DRM_MEM_BUFS);
	if (entry->buflist == NULL)
		return DRM_ERR(ENOMEM);

	entry->buf_size = size;
	entry->page_order = page_order;

	offset = 0;

	while ( entry->buf_count < count ) {
		buf          = &entry->buflist[entry->buf_count];
		buf->idx     = dma->buf_count + entry->buf_count;
		buf->total   = alignment;
		buf->order   = order;
		buf->used    = 0;

		buf->offset  = (dma->byte_count + offset);
		buf->bus_address = agp_offset + offset;
		buf->address = (void *)(agp_offset + offset + dev->sg->handle);
		buf->next    = NULL;
		buf->pending = 0;
		buf->filp    = NULL;

		buf->dev_priv_size = sizeof(DRIVER_BUF_PRIV_T);
		buf->dev_private = DRM(calloc)(1, buf->dev_priv_size,
		    DRM_MEM_BUFS);
		if (buf->dev_private == NULL) {
			/* Set count correctly so we free the proper amount. */
			entry->buf_count = count;
			DRM(cleanup_buf_error)(dev, entry);
			return DRM_ERR(ENOMEM);
		}

		DRM_DEBUG( "buffer %d @ %p\n",
			   entry->buf_count, buf->address );

		offset += alignment;
		entry->buf_count++;
		byte_count += PAGE_SIZE << page_order;
	}

	DRM_DEBUG( "byte_count: %d\n", byte_count );

	temp_buflist = DRM(realloc)( dma->buflist,
				     dma->buf_count * sizeof(*dma->buflist),
				     (dma->buf_count + entry->buf_count)
				     * sizeof(*dma->buflist),
				     DRM_MEM_BUFS );
	if (temp_buflist == NULL) {
		/* Free the entry because it isn't valid */
		DRM(cleanup_buf_error)(dev, entry);
		return DRM_ERR(ENOMEM);
	}
	dma->buflist = temp_buflist;

	for ( i = 0 ; i < entry->buf_count ; i++ ) {
		dma->buflist[i + dma->buf_count] = &entry->buflist[i];
	}

	dma->buf_count += entry->buf_count;
	dma->byte_count += byte_count;

	DRM_DEBUG( "dma->buf_count : %d\n", dma->buf_count );
	DRM_DEBUG( "entry->buf_count : %d\n", entry->buf_count );

	request->count = entry->buf_count;
	request->size = size;

	dma->flags = _DRM_DMA_USE_SG;

	return 0;
}
#endif /* __REALLY_HAVE_SG */

int DRM(addbufs)( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_buf_desc_t request;
	int err;
	int order;

	DRM_COPY_FROM_USER_IOCTL( request, (drm_buf_desc_t *)data, sizeof(request) );

	if (request.count < 0 || request.count > 4096)
		return DRM_ERR(EINVAL);

	order = DRM(order)(request.size);
	if (order < DRM_MIN_ORDER || order > DRM_MAX_ORDER)
		return DRM_ERR(EINVAL);

	DRM_SPINLOCK(&dev->dma_lock);
	/* No more allocations after first buffer-using ioctl. */
	if (dev->buf_use != 0) {
		DRM_SPINUNLOCK(&dev->dma_lock);
		return DRM_ERR(EBUSY);
	}
	/* No more than one allocation per order */
	if (dev->dma->bufs[order].buf_count != 0) {
		DRM_SPINUNLOCK(&dev->dma_lock);
		return DRM_ERR(ENOMEM);
	}

#if __REALLY_HAVE_AGP
	if ( request.flags & _DRM_AGP_BUFFER )
		err = DRM(addbufs_agp)(dev, &request);
	else
#endif
#if __REALLY_HAVE_SG
	if ( request.flags & _DRM_SG_BUFFER )
		err = DRM(addbufs_sg)(dev, &request);
	else
#endif
#if __HAVE_PCI_DMA
		err = DRM(addbufs_pci)(dev, &request);
#else
		err = DRM_ERR(EINVAL);
#endif
	DRM_SPINUNLOCK(&dev->dma_lock);

	DRM_COPY_TO_USER_IOCTL((drm_buf_desc_t *)data, request, sizeof(request));

	return err;
}

int DRM(infobufs)( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_info_t request;
	int i;
	int count;
	int retcode = 0;

	DRM_COPY_FROM_USER_IOCTL( request, (drm_buf_info_t *)data, sizeof(request) );

	DRM_SPINLOCK(&dev->dma_lock);
	++dev->buf_use;		/* Can't allocate more after this call */
	DRM_SPINUNLOCK(&dev->dma_lock);

	for ( i = 0, count = 0 ; i < DRM_MAX_ORDER + 1 ; i++ ) {
		if ( dma->bufs[i].buf_count ) ++count;
	}

	DRM_DEBUG( "count = %d\n", count );

	if ( request.count >= count ) {
		for ( i = 0, count = 0 ; i < DRM_MAX_ORDER + 1 ; i++ ) {
			if ( dma->bufs[i].buf_count ) {
				drm_buf_desc_t from;

				from.count = dma->bufs[i].buf_count;
				from.size = dma->bufs[i].buf_size;
				from.low_mark = dma->bufs[i].freelist.low_mark;
				from.high_mark = dma->bufs[i].freelist.high_mark;

				if (DRM_COPY_TO_USER(&request.list[count], &from,
				    sizeof(drm_buf_desc_t)) != 0) {
					retcode = DRM_ERR(EFAULT);
					break;
				}

				DRM_DEBUG( "%d %d %d %d %d\n",
					   i,
					   dma->bufs[i].buf_count,
					   dma->bufs[i].buf_size,
					   dma->bufs[i].freelist.low_mark,
					   dma->bufs[i].freelist.high_mark );
				++count;
			}
		}
	}
	request.count = count;

	DRM_COPY_TO_USER_IOCTL( (drm_buf_info_t *)data, request, sizeof(request) );

	return retcode;
}

int DRM(markbufs)( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_desc_t request;
	int order;

	DRM_COPY_FROM_USER_IOCTL( request, (drm_buf_desc_t *)data, sizeof(request) );

	DRM_DEBUG( "%d, %d, %d\n",
		   request.size, request.low_mark, request.high_mark );
	

	order = DRM(order)(request.size);	
	if (order < DRM_MIN_ORDER || order > DRM_MAX_ORDER ||
	    request.low_mark < 0 || request.high_mark < 0) {
		return DRM_ERR(EINVAL);
	}

	DRM_SPINLOCK(&dev->dma_lock);
	if (request.low_mark > dma->bufs[order].buf_count ||
	    request.high_mark > dma->bufs[order].buf_count) {
		return DRM_ERR(EINVAL);
	}

	dma->bufs[order].freelist.low_mark  = request.low_mark;
	dma->bufs[order].freelist.high_mark = request.high_mark;
	DRM_SPINUNLOCK(&dev->dma_lock);

	return 0;
}

int DRM(freebufs)( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_free_t request;
	int i;
	int idx;
	drm_buf_t *buf;
	int retcode = 0;

	DRM_COPY_FROM_USER_IOCTL( request, (drm_buf_free_t *)data, sizeof(request) );

	DRM_DEBUG( "%d\n", request.count );
	
	DRM_SPINLOCK(&dev->dma_lock);
	for ( i = 0 ; i < request.count ; i++ ) {
		if (DRM_COPY_FROM_USER(&idx, &request.list[i], sizeof(idx))) {
			retcode = DRM_ERR(EFAULT);
			break;
		}
		if ( idx < 0 || idx >= dma->buf_count ) {
			DRM_ERROR( "Index %d (of %d max)\n",
				   idx, dma->buf_count - 1 );
			retcode = DRM_ERR(EINVAL);
			break;
		}
		buf = dma->buflist[idx];
		if ( buf->filp != filp ) {
			DRM_ERROR("Process %d freeing buffer not owned\n",
				   DRM_CURRENTPID);
			retcode = DRM_ERR(EINVAL);
			break;
		}
		DRM(free_buffer)( dev, buf );
	}
	DRM_SPINUNLOCK(&dev->dma_lock);

	return retcode;
}

int DRM(mapbufs)( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_device_dma_t *dma = dev->dma;
	int retcode = 0;
	const int zero = 0;
	vm_offset_t address;
	struct vmspace *vms;
#ifdef __FreeBSD__
	vm_ooffset_t foff;
	vm_size_t size;
	vm_offset_t vaddr;
#endif /* __FreeBSD__ */
#ifdef __NetBSD__
	struct vnode *vn;
	vm_size_t size;
	vaddr_t vaddr;
#endif /* __NetBSD__ */

	drm_buf_map_t request;
	int i;

	DRM_COPY_FROM_USER_IOCTL( request, (drm_buf_map_t *)data, sizeof(request) );

#ifdef __NetBSD__
	if (!vfinddev(kdev, VCHR, &vn))
		return 0;	/* FIXME: Shouldn't this be EINVAL or something? */
#endif /* __NetBSD__ */

#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
	vms = p->td_proc->p_vmspace;
#else
	vms = p->p_vmspace;
#endif

	DRM_SPINLOCK(&dev->dma_lock);
	dev->buf_use++;		/* Can't allocate more after this call */
	DRM_SPINUNLOCK(&dev->dma_lock);

	if (request.count < dma->buf_count)
		goto done;

	if ((__HAVE_AGP && (dma->flags & _DRM_DMA_USE_AGP)) ||
	    (__HAVE_SG && (dma->flags & _DRM_DMA_USE_SG))) {
		drm_local_map_t *map = DRIVER_AGP_BUFFERS_MAP(dev);

		if (map == NULL) {
			retcode = EINVAL;
			goto done;
		}
		size = round_page(map->size);
		foff = map->offset;
	} else {
		size = round_page(dma->byte_count),
		foff = 0;
	}

#ifdef __FreeBSD__
	vaddr = round_page((vm_offset_t)vms->vm_daddr + MAXDSIZ);
	retcode = vm_mmap(&vms->vm_map, &vaddr, size, PROT_READ | PROT_WRITE,
	    VM_PROT_ALL, MAP_SHARED, SLIST_FIRST(&kdev->si_hlist), foff );
#elif defined(__NetBSD__)
	vaddr = round_page((vaddr_t)vms->vm_daddr + MAXDSIZ);
	retcode = uvm_mmap(&vms->vm_map, &vaddr, size,
	    UVM_PROT_READ | UVM_PROT_WRITE, UVM_PROT_ALL, MAP_SHARED,
	    &vn->v_uobj, foff, p->p_rlimit[RLIMIT_MEMLOCK].rlim_cur);
#endif /* __NetBSD__ */
	if (retcode)
		goto done;

	request.virtual = (void *)vaddr;

	for ( i = 0 ; i < dma->buf_count ; i++ ) {
		if (DRM_COPY_TO_USER(&request.list[i].idx,
		    &dma->buflist[i]->idx, sizeof(request.list[0].idx))) {
			retcode = EFAULT;
			goto done;
		}
		if (DRM_COPY_TO_USER(&request.list[i].total,
		    &dma->buflist[i]->total, sizeof(request.list[0].total))) {
			retcode = EFAULT;
			goto done;
		}
		if (DRM_COPY_TO_USER(&request.list[i].used, &zero,
		    sizeof(zero))) {
			retcode = EFAULT;
			goto done;
		}
		address = vaddr + dma->buflist[i]->offset; /* *** */
		if (DRM_COPY_TO_USER(&request.list[i].address, &address,
		    sizeof(address))) {
			retcode = EFAULT;
			goto done;
		}
	}

 done:
	request.count = dma->buf_count;

	DRM_DEBUG( "%d buffers, retcode = %d\n", request.count, retcode );

	DRM_COPY_TO_USER_IOCTL((drm_buf_map_t *)data, request, sizeof(request));

	return DRM_ERR(retcode);
}

#endif /* __HAVE_DMA */
