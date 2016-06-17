/* drm_bufs.h -- Generic buffer template -*- linux-c -*-
 * Created: Thu Nov 23 03:10:50 2000 by gareth@valinux.com
 *
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
 */

#include <linux/vmalloc.h>
#include "drmP.h"

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

int DRM(addmap)( struct inode *inode, struct file *filp,
		 unsigned int cmd, unsigned long arg )
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_map_t *map;
	drm_map_list_t *list;

	if ( !(filp->f_mode & 3) ) return -EACCES; /* Require read/write */

	map = DRM(alloc)( sizeof(*map), DRM_MEM_MAPS );
	if ( !map )
		return -ENOMEM;

	if ( copy_from_user( map, (drm_map_t *)arg, sizeof(*map) ) ) {
		DRM(free)( map, sizeof(*map), DRM_MEM_MAPS );
		return -EFAULT;
	}

	/* Only allow shared memory to be removable since we only keep enough
	 * book keeping information about shared memory to allow for removal
	 * when processes fork.
	 */
	if ( (map->flags & _DRM_REMOVABLE) && map->type != _DRM_SHM ) {
		DRM(free)( map, sizeof(*map), DRM_MEM_MAPS );
		return -EINVAL;
	}
	DRM_DEBUG( "offset = 0x%08lx, size = 0x%08lx, type = %d\n",
		   map->offset, map->size, map->type );
	if ( (map->offset & (~PAGE_MASK)) || (map->size & (~PAGE_MASK)) ) {
		DRM(free)( map, sizeof(*map), DRM_MEM_MAPS );
		return -EINVAL;
	}
	map->mtrr   = -1;
	map->handle = 0;

	switch ( map->type ) {
	case _DRM_REGISTERS:
	case _DRM_FRAME_BUFFER:
#if !defined(__sparc__) && !defined(__alpha__)
		if ( map->offset + map->size < map->offset ||
		     map->offset < virt_to_phys(high_memory) ) {
			DRM(free)( map, sizeof(*map), DRM_MEM_MAPS );
			return -EINVAL;
		}
#endif
#ifdef __alpha__
		map->offset += dev->hose->mem_space->start;
#endif
#if __REALLY_HAVE_MTRR
		if ( map->type == _DRM_FRAME_BUFFER ||
		     (map->flags & _DRM_WRITE_COMBINING) ) {
			map->mtrr = mtrr_add( map->offset, map->size,
					      MTRR_TYPE_WRCOMB, 1 );
		}
#endif
		map->handle = DRM(ioremap)( map->offset, map->size, dev );
		break;

	case _DRM_SHM:
		map->handle = vmalloc_32(map->size);
		DRM_DEBUG( "%ld %d %p\n",
			   map->size, DRM(order)( map->size ), map->handle );
		if ( !map->handle ) {
			DRM(free)( map, sizeof(*map), DRM_MEM_MAPS );
			return -ENOMEM;
		}
		map->offset = (unsigned long)map->handle;
		if ( map->flags & _DRM_CONTAINS_LOCK ) {
			dev->sigdata.lock =
			dev->lock.hw_lock = map->handle; /* Pointer to lock */
		}
		break;
#if __REALLY_HAVE_AGP
	case _DRM_AGP:
#ifdef __alpha__
		map->offset += dev->hose->mem_space->start;
#endif
		map->offset = map->offset + dev->agp->base;
		map->mtrr   = dev->agp->agp_mtrr; /* for getmap */
		break;
#endif
	case _DRM_SCATTER_GATHER:
		if (!dev->sg) {
			DRM(free)(map, sizeof(*map), DRM_MEM_MAPS);
			return -EINVAL;
		}
		map->offset = map->offset + dev->sg->handle;
		break;

	default:
		DRM(free)( map, sizeof(*map), DRM_MEM_MAPS );
		return -EINVAL;
	}

	list = DRM(alloc)(sizeof(*list), DRM_MEM_MAPS);
	if(!list) {
		DRM(free)(map, sizeof(*map), DRM_MEM_MAPS);
		return -EINVAL;
	}
	memset(list, 0, sizeof(*list));
	list->map = map;

	down(&dev->struct_sem);
	list_add(&list->head, &dev->maplist->head);
 	up(&dev->struct_sem);

	if ( copy_to_user( (drm_map_t *)arg, map, sizeof(*map) ) )
		return -EFAULT;
	if ( map->type != _DRM_SHM ) {
		if ( copy_to_user( &((drm_map_t *)arg)->handle,
				   &map->offset,
				   sizeof(map->offset) ) )
			return -EFAULT;
	}
	return 0;
}


/* Remove a map private from list and deallocate resources if the mapping
 * isn't in use.
 */

int DRM(rmmap)(struct inode *inode, struct file *filp,
	       unsigned int cmd, unsigned long arg)
{
	drm_file_t	*priv	= filp->private_data;
	drm_device_t	*dev	= priv->dev;
	struct list_head *list;
	drm_map_list_t *r_list = NULL;
	drm_vma_entry_t *pt, *prev;
	drm_map_t *map;
	drm_map_t request;
	int found_maps = 0;

	if (copy_from_user(&request, (drm_map_t *)arg,
			   sizeof(request))) {
		return -EFAULT;
	}

	down(&dev->struct_sem);
	list = &dev->maplist->head;
	list_for_each(list, &dev->maplist->head) {
		r_list = (drm_map_list_t *) list;

		if(r_list->map &&
		   r_list->map->handle == request.handle &&
		   r_list->map->flags & _DRM_REMOVABLE) break;
	}

	/* List has wrapped around to the head pointer, or its empty we didn't
	 * find anything.
	 */
	if(list == (&dev->maplist->head)) {
		up(&dev->struct_sem);
		return -EINVAL;
	}
	map = r_list->map;
	list_del(list);
	DRM(free)(list, sizeof(*list), DRM_MEM_MAPS);

	for (pt = dev->vmalist, prev = NULL; pt; prev = pt, pt = pt->next) {
		if (pt->vma->vm_private_data == map) found_maps++;
	}

	if(!found_maps) {
		switch (map->type) {
		case _DRM_REGISTERS:
		case _DRM_FRAME_BUFFER:
#if __REALLY_HAVE_MTRR
			if (map->mtrr >= 0) {
				int retcode;
				retcode = mtrr_del(map->mtrr,
						   map->offset,
						   map->size);
				DRM_DEBUG("mtrr_del = %d\n", retcode);
			}
#endif
			DRM(ioremapfree)(map->handle, map->size, dev);
			break;
		case _DRM_SHM:
			vfree(map->handle);
			break;
		case _DRM_AGP:
		case _DRM_SCATTER_GATHER:
			break;
		}
		DRM(free)(map, sizeof(*map), DRM_MEM_MAPS);
	}
	up(&dev->struct_sem);
	return 0;
}

#if __HAVE_DMA


static void DRM(cleanup_buf_error)(drm_buf_entry_t *entry)
{
	int i;

	if (entry->seg_count) {
		for (i = 0; i < entry->seg_count; i++) {
			DRM(free_pages)(entry->seglist[i],
					entry->page_order,
					DRM_MEM_DMA);
		}
		DRM(free)(entry->seglist,
			  entry->seg_count *
			  sizeof(*entry->seglist),
			  DRM_MEM_SEGS);

		entry->seg_count = 0;
	}

   	if(entry->buf_count) {
	   	for(i = 0; i < entry->buf_count; i++) {
			if(entry->buflist[i].dev_private) {
				DRM(free)(entry->buflist[i].dev_private,
					  entry->buflist[i].dev_priv_size,
					  DRM_MEM_BUFS);
			}
		}
		DRM(free)(entry->buflist,
			  entry->buf_count *
			  sizeof(*entry->buflist),
			  DRM_MEM_BUFS);

#if __HAVE_DMA_FREELIST
	   	DRM(freelist_destroy)(&entry->freelist);
#endif

		entry->buf_count = 0;
	}
}

#if __REALLY_HAVE_AGP
int DRM(addbufs_agp)( struct inode *inode, struct file *filp,
		      unsigned int cmd, unsigned long arg )
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_desc_t request;
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

	if ( !dma ) return -EINVAL;

	if ( copy_from_user( &request, (drm_buf_desc_t *)arg,
			     sizeof(request) ) )
		return -EFAULT;

	count = request.count;
	order = DRM(order)( request.size );
	size = 1 << order;

	alignment  = (request.flags & _DRM_PAGE_ALIGN)
		? PAGE_ALIGN(size) : size;
	page_order = order - PAGE_SHIFT > 0 ? order - PAGE_SHIFT : 0;
	total = PAGE_SIZE << page_order;

	byte_count = 0;
	agp_offset = dev->agp->base + request.agp_start;

	DRM_DEBUG( "count:      %d\n",  count );
	DRM_DEBUG( "order:      %d\n",  order );
	DRM_DEBUG( "size:       %d\n",  size );
	DRM_DEBUG( "agp_offset: %ld\n", agp_offset );
	DRM_DEBUG( "alignment:  %d\n",  alignment );
	DRM_DEBUG( "page_order: %d\n",  page_order );
	DRM_DEBUG( "total:      %d\n",  total );

	if ( order < DRM_MIN_ORDER || order > DRM_MAX_ORDER ) return -EINVAL;
	if ( dev->queue_count ) return -EBUSY; /* Not while in use */

	spin_lock( &dev->count_lock );
	if ( dev->buf_use ) {
		spin_unlock( &dev->count_lock );
		return -EBUSY;
	}
	atomic_inc( &dev->buf_alloc );
	spin_unlock( &dev->count_lock );

	down( &dev->struct_sem );
	entry = &dma->bufs[order];
	if ( entry->buf_count ) {
		up( &dev->struct_sem );
		atomic_dec( &dev->buf_alloc );
		return -ENOMEM; /* May only call once for each order */
	}

	if (count < 0 || count > 4096) {
		up( &dev->struct_sem );
		atomic_dec( &dev->buf_alloc );
		return -EINVAL;
	}

	entry->buflist = DRM(alloc)( count * sizeof(*entry->buflist),
				    DRM_MEM_BUFS );
	if ( !entry->buflist ) {
		up( &dev->struct_sem );
		atomic_dec( &dev->buf_alloc );
		return -ENOMEM;
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
		buf->waiting = 0;
		buf->pending = 0;
		init_waitqueue_head( &buf->dma_wait );
		buf->pid     = 0;

		buf->dev_priv_size = sizeof(DRIVER_BUF_PRIV_T);
		buf->dev_private = DRM(alloc)( sizeof(DRIVER_BUF_PRIV_T),
					       DRM_MEM_BUFS );
		if(!buf->dev_private) {
			/* Set count correctly so we free the proper amount. */
			entry->buf_count = count;
			DRM(cleanup_buf_error)(entry);
		}
		memset( buf->dev_private, 0, buf->dev_priv_size );

#if __HAVE_DMA_HISTOGRAM
		buf->time_queued = 0;
		buf->time_dispatched = 0;
		buf->time_completed = 0;
		buf->time_freed = 0;
#endif
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
	if(!temp_buflist) {
		/* Free the entry because it isn't valid */
		DRM(cleanup_buf_error)(entry);
		up( &dev->struct_sem );
		atomic_dec( &dev->buf_alloc );
		return -ENOMEM;
	}
	dma->buflist = temp_buflist;

	for ( i = 0 ; i < entry->buf_count ; i++ ) {
		dma->buflist[i + dma->buf_count] = &entry->buflist[i];
	}

	dma->buf_count += entry->buf_count;
	dma->byte_count += byte_count;

	DRM_DEBUG( "dma->buf_count : %d\n", dma->buf_count );
	DRM_DEBUG( "entry->buf_count : %d\n", entry->buf_count );

#if __HAVE_DMA_FREELIST
	DRM(freelist_create)( &entry->freelist, entry->buf_count );
	for ( i = 0 ; i < entry->buf_count ; i++ ) {
		DRM(freelist_put)( dev, &entry->freelist, &entry->buflist[i] );
	}
#endif
	up( &dev->struct_sem );

	request.count = entry->buf_count;
	request.size = size;

	if ( copy_to_user( (drm_buf_desc_t *)arg, &request, sizeof(request) ) )
		return -EFAULT;

	dma->flags = _DRM_DMA_USE_AGP;

	atomic_dec( &dev->buf_alloc );
	return 0;
}
#endif /* __REALLY_HAVE_AGP */

#if __HAVE_PCI_DMA
int DRM(addbufs_pci)( struct inode *inode, struct file *filp,
		      unsigned int cmd, unsigned long arg )
{
   	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_desc_t request;
	int count;
	int order;
	int size;
	int total;
	int page_order;
	drm_buf_entry_t *entry;
	unsigned long page;
	drm_buf_t *buf;
	int alignment;
	unsigned long offset;
	int i;
	int byte_count;
	int page_count;
	unsigned long *temp_pagelist;
	drm_buf_t **temp_buflist;

	if ( !dma ) return -EINVAL;

	if ( copy_from_user( &request, (drm_buf_desc_t *)arg,
			     sizeof(request) ) )
		return -EFAULT;

	count = request.count;
	order = DRM(order)( request.size );
	size = 1 << order;

	DRM_DEBUG( "count=%d, size=%d (%d), order=%d, queue_count=%d\n",
		   request.count, request.size, size,
		   order, dev->queue_count );

	if ( order < DRM_MIN_ORDER || order > DRM_MAX_ORDER ) return -EINVAL;
	if ( dev->queue_count ) return -EBUSY; /* Not while in use */

	alignment = (request.flags & _DRM_PAGE_ALIGN)
		? PAGE_ALIGN(size) : size;
	page_order = order - PAGE_SHIFT > 0 ? order - PAGE_SHIFT : 0;
	total = PAGE_SIZE << page_order;

	spin_lock( &dev->count_lock );
	if ( dev->buf_use ) {
		spin_unlock( &dev->count_lock );
		return -EBUSY;
	}
	atomic_inc( &dev->buf_alloc );
	spin_unlock( &dev->count_lock );

	down( &dev->struct_sem );
	entry = &dma->bufs[order];
	if ( entry->buf_count ) {
		up( &dev->struct_sem );
		atomic_dec( &dev->buf_alloc );
		return -ENOMEM;	/* May only call once for each order */
	}

	if (count < 0 || count > 4096) {
		up( &dev->struct_sem );
		atomic_dec( &dev->buf_alloc );
		return -EINVAL;
	}

	entry->buflist = DRM(alloc)( count * sizeof(*entry->buflist),
				    DRM_MEM_BUFS );
	if ( !entry->buflist ) {
		up( &dev->struct_sem );
		atomic_dec( &dev->buf_alloc );
		return -ENOMEM;
	}
	memset( entry->buflist, 0, count * sizeof(*entry->buflist) );

	entry->seglist = DRM(alloc)( count * sizeof(*entry->seglist),
				    DRM_MEM_SEGS );
	if ( !entry->seglist ) {
		DRM(free)( entry->buflist,
			  count * sizeof(*entry->buflist),
			  DRM_MEM_BUFS );
		up( &dev->struct_sem );
		atomic_dec( &dev->buf_alloc );
		return -ENOMEM;
	}
	memset( entry->seglist, 0, count * sizeof(*entry->seglist) );

	temp_pagelist = DRM(realloc)( dma->pagelist,
				      dma->page_count * sizeof(*dma->pagelist),
				      (dma->page_count + (count << page_order))
				      * sizeof(*dma->pagelist),
				      DRM_MEM_PAGES );
	if(!temp_pagelist) {
		DRM(free)( entry->buflist,
			   count * sizeof(*entry->buflist),
			   DRM_MEM_BUFS );
		DRM(free)( entry->seglist,
			   count * sizeof(*entry->seglist),
			   DRM_MEM_SEGS );
		up( &dev->struct_sem );
		atomic_dec( &dev->buf_alloc );
		return -ENOMEM;
	}

	dma->pagelist = temp_pagelist;
	DRM_DEBUG( "pagelist: %d entries\n",
		   dma->page_count + (count << page_order) );

	entry->buf_size	= size;
	entry->page_order = page_order;
	byte_count = 0;
	page_count = 0;

	while ( entry->buf_count < count ) {
		page = DRM(alloc_pages)( page_order, DRM_MEM_DMA );
		if ( !page ) break;
		entry->seglist[entry->seg_count++] = page;
		for ( i = 0 ; i < (1 << page_order) ; i++ ) {
			DRM_DEBUG( "page %d @ 0x%08lx\n",
				   dma->page_count + page_count,
				   page + PAGE_SIZE * i );
			dma->pagelist[dma->page_count + page_count++]
				= page + PAGE_SIZE * i;
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
			buf->address = (void *)(page + offset);
			buf->next    = NULL;
			buf->waiting = 0;
			buf->pending = 0;
			init_waitqueue_head( &buf->dma_wait );
			buf->pid     = 0;
#if __HAVE_DMA_HISTOGRAM
			buf->time_queued     = 0;
			buf->time_dispatched = 0;
			buf->time_completed  = 0;
			buf->time_freed      = 0;
#endif
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
	if(!temp_buflist) {
		/* Free the entry because it isn't valid */
		DRM(cleanup_buf_error)(entry);
		up( &dev->struct_sem );
		atomic_dec( &dev->buf_alloc );
		return -ENOMEM;
	}
	dma->buflist = temp_buflist;

	for ( i = 0 ; i < entry->buf_count ; i++ ) {
		dma->buflist[i + dma->buf_count] = &entry->buflist[i];
	}

	dma->buf_count += entry->buf_count;
	dma->seg_count += entry->seg_count;
	dma->page_count += entry->seg_count << page_order;
	dma->byte_count += PAGE_SIZE * (entry->seg_count << page_order);

#if __HAVE_DMA_FREELIST
	DRM(freelist_create)( &entry->freelist, entry->buf_count );
	for ( i = 0 ; i < entry->buf_count ; i++ ) {
		DRM(freelist_put)( dev, &entry->freelist, &entry->buflist[i] );
	}
#endif
	up( &dev->struct_sem );

	request.count = entry->buf_count;
	request.size = size;

	if ( copy_to_user( (drm_buf_desc_t *)arg, &request, sizeof(request) ) )
		return -EFAULT;

	atomic_dec( &dev->buf_alloc );
	return 0;

}
#endif /* __HAVE_PCI_DMA */

#ifdef __HAVE_SG
int DRM(addbufs_sg)( struct inode *inode, struct file *filp,
                     unsigned int cmd, unsigned long arg )
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_desc_t request;
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

	if ( !dma ) return -EINVAL;

	if ( copy_from_user( &request, (drm_buf_desc_t *)arg,
                             sizeof(request) ) )
		return -EFAULT;

	count = request.count;
	order = DRM(order)( request.size );
	size = 1 << order;

	alignment  = (request.flags & _DRM_PAGE_ALIGN)
			? PAGE_ALIGN(size) : size;
	page_order = order - PAGE_SHIFT > 0 ? order - PAGE_SHIFT : 0;
	total = PAGE_SIZE << page_order;

	byte_count = 0;
	agp_offset = request.agp_start;

	DRM_DEBUG( "count:      %d\n",  count );
	DRM_DEBUG( "order:      %d\n",  order );
	DRM_DEBUG( "size:       %d\n",  size );
	DRM_DEBUG( "agp_offset: %ld\n", agp_offset );
	DRM_DEBUG( "alignment:  %d\n",  alignment );
	DRM_DEBUG( "page_order: %d\n",  page_order );
	DRM_DEBUG( "total:      %d\n",  total );

	if ( order < DRM_MIN_ORDER || order > DRM_MAX_ORDER ) return -EINVAL;
	if ( dev->queue_count ) return -EBUSY; /* Not while in use */

	spin_lock( &dev->count_lock );
	if ( dev->buf_use ) {
		spin_unlock( &dev->count_lock );
		return -EBUSY;
	}
	atomic_inc( &dev->buf_alloc );
	spin_unlock( &dev->count_lock );

	down( &dev->struct_sem );
	entry = &dma->bufs[order];
	if ( entry->buf_count ) {
		up( &dev->struct_sem );
		atomic_dec( &dev->buf_alloc );
		return -ENOMEM; /* May only call once for each order */
	}

	if (count < 0 || count > 4096) {
		up( &dev->struct_sem );
		atomic_dec( &dev->buf_alloc );
		return -EINVAL;
	}

	entry->buflist = DRM(alloc)( count * sizeof(*entry->buflist),
				     DRM_MEM_BUFS );
	if ( !entry->buflist ) {
		up( &dev->struct_sem );
		atomic_dec( &dev->buf_alloc );
		return -ENOMEM;
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
		buf->address = (void *)(agp_offset + offset + dev->sg->handle);
		buf->next    = NULL;
		buf->waiting = 0;
		buf->pending = 0;
		init_waitqueue_head( &buf->dma_wait );
		buf->pid     = 0;

		buf->dev_priv_size = sizeof(DRIVER_BUF_PRIV_T);
		buf->dev_private = DRM(alloc)( sizeof(DRIVER_BUF_PRIV_T),
					       DRM_MEM_BUFS );
		if(!buf->dev_private) {
			/* Set count correctly so we free the proper amount. */
			entry->buf_count = count;
			DRM(cleanup_buf_error)(entry);
			up( &dev->struct_sem );
			atomic_dec( &dev->buf_alloc );
			return -ENOMEM;
		}

		memset( buf->dev_private, 0, buf->dev_priv_size );

# if __HAVE_DMA_HISTOGRAM
		buf->time_queued = 0;
		buf->time_dispatched = 0;
		buf->time_completed = 0;
		buf->time_freed = 0;
# endif
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
	if(!temp_buflist) {
		/* Free the entry because it isn't valid */
		DRM(cleanup_buf_error)(entry);
		up( &dev->struct_sem );
		atomic_dec( &dev->buf_alloc );
		return -ENOMEM;
	}
	dma->buflist = temp_buflist;

	for ( i = 0 ; i < entry->buf_count ; i++ ) {
		dma->buflist[i + dma->buf_count] = &entry->buflist[i];
	}

	dma->buf_count += entry->buf_count;
	dma->byte_count += byte_count;

	DRM_DEBUG( "dma->buf_count : %d\n", dma->buf_count );
	DRM_DEBUG( "entry->buf_count : %d\n", entry->buf_count );

#if __HAVE_DMA_FREELIST
	DRM(freelist_create)( &entry->freelist, entry->buf_count );
	for ( i = 0 ; i < entry->buf_count ; i++ ) {
		DRM(freelist_put)( dev, &entry->freelist, &entry->buflist[i] );
	}
#endif
	up( &dev->struct_sem );

	request.count = entry->buf_count;
	request.size = size;

	if ( copy_to_user( (drm_buf_desc_t *)arg, &request, sizeof(request) ) )
		return -EFAULT;

	dma->flags = _DRM_DMA_USE_SG;

	atomic_dec( &dev->buf_alloc );
	return 0;
}
#endif /* __HAVE_SG */

int DRM(addbufs)( struct inode *inode, struct file *filp,
		  unsigned int cmd, unsigned long arg )
{
	drm_buf_desc_t request;

	if ( copy_from_user( &request, (drm_buf_desc_t *)arg,
			     sizeof(request) ) )
		return -EFAULT;

#if __REALLY_HAVE_AGP
	if ( request.flags & _DRM_AGP_BUFFER )
		return DRM(addbufs_agp)( inode, filp, cmd, arg );
	else
#endif
#if __HAVE_SG
	if ( request.flags & _DRM_SG_BUFFER )
		return DRM(addbufs_sg)( inode, filp, cmd, arg );
	else
#endif
#if __HAVE_PCI_DMA
		return DRM(addbufs_pci)( inode, filp, cmd, arg );
#else
		return -EINVAL;
#endif
}

int DRM(infobufs)( struct inode *inode, struct file *filp,
		   unsigned int cmd, unsigned long arg )
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_info_t request;
	int i;
	int count;

	if ( !dma ) return -EINVAL;

	spin_lock( &dev->count_lock );
	if ( atomic_read( &dev->buf_alloc ) ) {
		spin_unlock( &dev->count_lock );
		return -EBUSY;
	}
	++dev->buf_use;		/* Can't allocate more after this call */
	spin_unlock( &dev->count_lock );

	if ( copy_from_user( &request,
			     (drm_buf_info_t *)arg,
			     sizeof(request) ) )
		return -EFAULT;

	for ( i = 0, count = 0 ; i < DRM_MAX_ORDER + 1 ; i++ ) {
		if ( dma->bufs[i].buf_count ) ++count;
	}

	DRM_DEBUG( "count = %d\n", count );

	if ( request.count >= count ) {
		for ( i = 0, count = 0 ; i < DRM_MAX_ORDER + 1 ; i++ ) {
			if ( dma->bufs[i].buf_count ) {
				drm_buf_desc_t *to = &request.list[count];
				drm_buf_entry_t *from = &dma->bufs[i];
				drm_freelist_t *list = &dma->bufs[i].freelist;
				if ( copy_to_user( &to->count,
						   &from->buf_count,
						   sizeof(from->buf_count) ) ||
				     copy_to_user( &to->size,
						   &from->buf_size,
						   sizeof(from->buf_size) ) ||
				     copy_to_user( &to->low_mark,
						   &list->low_mark,
						   sizeof(list->low_mark) ) ||
				     copy_to_user( &to->high_mark,
						   &list->high_mark,
						   sizeof(list->high_mark) ) )
					return -EFAULT;

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

	if ( copy_to_user( (drm_buf_info_t *)arg,
			   &request,
			   sizeof(request) ) )
		return -EFAULT;

	return 0;
}

int DRM(markbufs)( struct inode *inode, struct file *filp,
		   unsigned int cmd, unsigned long arg )
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_desc_t request;
	int order;
	drm_buf_entry_t *entry;

	if ( !dma ) return -EINVAL;

	if ( copy_from_user( &request,
			     (drm_buf_desc_t *)arg,
			     sizeof(request) ) )
		return -EFAULT;

	DRM_DEBUG( "%d, %d, %d\n",
		   request.size, request.low_mark, request.high_mark );
	order = DRM(order)( request.size );
	if ( order < DRM_MIN_ORDER || order > DRM_MAX_ORDER ) return -EINVAL;
	entry = &dma->bufs[order];

	if ( request.low_mark < 0 || request.low_mark > entry->buf_count )
		return -EINVAL;
	if ( request.high_mark < 0 || request.high_mark > entry->buf_count )
		return -EINVAL;

	entry->freelist.low_mark  = request.low_mark;
	entry->freelist.high_mark = request.high_mark;

	return 0;
}

int DRM(freebufs)( struct inode *inode, struct file *filp,
		   unsigned int cmd, unsigned long arg )
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_free_t request;
	int i;
	int idx;
	drm_buf_t *buf;

	if ( !dma ) return -EINVAL;

	if ( copy_from_user( &request,
			     (drm_buf_free_t *)arg,
			     sizeof(request) ) )
		return -EFAULT;

	DRM_DEBUG( "%d\n", request.count );
	for ( i = 0 ; i < request.count ; i++ ) {
		if ( copy_from_user( &idx,
				     &request.list[i],
				     sizeof(idx) ) )
			return -EFAULT;
		if ( idx < 0 || idx >= dma->buf_count ) {
			DRM_ERROR( "Index %d (of %d max)\n",
				   idx, dma->buf_count - 1 );
			return -EINVAL;
		}
		buf = dma->buflist[idx];
		if ( buf->pid != current->pid ) {
			DRM_ERROR( "Process %d freeing buffer owned by %d\n",
				   current->pid, buf->pid );
			return -EINVAL;
		}
		DRM(free_buffer)( dev, buf );
	}

	return 0;
}

int DRM(mapbufs)( struct inode *inode, struct file *filp,
		  unsigned int cmd, unsigned long arg )
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_device_dma_t *dma = dev->dma;
	int retcode = 0;
	const int zero = 0;
	unsigned long virtual;
	unsigned long address;
	drm_buf_map_t request;
	int i;

	if ( !dma ) return -EINVAL;

	spin_lock( &dev->count_lock );
	if ( atomic_read( &dev->buf_alloc ) ) {
		spin_unlock( &dev->count_lock );
		return -EBUSY;
	}
	dev->buf_use++;		/* Can't allocate more after this call */
	spin_unlock( &dev->count_lock );

	if ( copy_from_user( &request, (drm_buf_map_t *)arg,
			     sizeof(request) ) )
		return -EFAULT;

	if ( request.count >= dma->buf_count ) {
		if ( (__HAVE_AGP && (dma->flags & _DRM_DMA_USE_AGP)) ||
		     (__HAVE_SG && (dma->flags & _DRM_DMA_USE_SG)) ) {
			drm_map_t *map = DRIVER_AGP_BUFFERS_MAP( dev );

			if ( !map ) {
				retcode = -EINVAL;
				goto done;
			}

			down_write( &current->mm->mmap_sem );
			virtual = do_mmap( filp, 0, map->size,
					   PROT_READ | PROT_WRITE,
					   MAP_SHARED,
					   (unsigned long)map->offset );
			up_write( &current->mm->mmap_sem );
		} else {
			down_write( &current->mm->mmap_sem );
			virtual = do_mmap( filp, 0, dma->byte_count,
					   PROT_READ | PROT_WRITE,
					   MAP_SHARED, 0 );
			up_write( &current->mm->mmap_sem );
		}
		if ( virtual > -1024UL ) {
			/* Real error */
			retcode = (signed long)virtual;
			goto done;
		}
		request.virtual = (void *)virtual;

		for ( i = 0 ; i < dma->buf_count ; i++ ) {
			if ( copy_to_user( &request.list[i].idx,
					   &dma->buflist[i]->idx,
					   sizeof(request.list[0].idx) ) ) {
				retcode = -EFAULT;
				goto done;
			}
			if ( copy_to_user( &request.list[i].total,
					   &dma->buflist[i]->total,
					   sizeof(request.list[0].total) ) ) {
				retcode = -EFAULT;
				goto done;
			}
			if ( copy_to_user( &request.list[i].used,
					   &zero,
					   sizeof(zero) ) ) {
				retcode = -EFAULT;
				goto done;
			}
			address = virtual + dma->buflist[i]->offset; /* *** */
			if ( copy_to_user( &request.list[i].address,
					   &address,
					   sizeof(address) ) ) {
				retcode = -EFAULT;
				goto done;
			}
		}
	}
 done:
	request.count = dma->buf_count;
	DRM_DEBUG( "%d buffers, retcode = %d\n", request.count, retcode );

	if ( copy_to_user( (drm_buf_map_t *)arg, &request, sizeof(request) ) )
		return -EFAULT;

	return retcode;
}

#endif /* __HAVE_DMA */
