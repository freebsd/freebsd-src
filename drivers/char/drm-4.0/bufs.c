/* bufs.c -- IOCTLs to manage buffers -*- linux-c -*-
 * Created: Tue Feb  2 08:37:54 1999 by faith@precisioninsight.com
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
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 * 
 * Authors:
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 *
 */

#define __NO_VERSION__
#include <linux/config.h>
#include "drmP.h"
#include "linux/un.h"

				/* Compute order.  Can be made faster. */
int drm_order(unsigned long size)
{
	int	      order;
	unsigned long tmp;

	for (order = 0, tmp = size; tmp >>= 1; ++order);
	if (size & ~(1 << order)) ++order;
	return order;
}

int drm_addmap(struct inode *inode, struct file *filp, unsigned int cmd,
	       unsigned long arg)
{
	drm_file_t	*priv	= filp->private_data;
	drm_device_t	*dev	= priv->dev;
	drm_map_t	*map;
	
	if (!(filp->f_mode & 3)) return -EACCES; /* Require read/write */

	map	     = drm_alloc(sizeof(*map), DRM_MEM_MAPS);
	if (!map) return -ENOMEM;
	if (copy_from_user(map, (drm_map_t *)arg, sizeof(*map))) {
		drm_free(map, sizeof(*map), DRM_MEM_MAPS);
		return -EFAULT;
	}

	DRM_DEBUG("offset = 0x%08lx, size = 0x%08lx, type = %d\n",
		  map->offset, map->size, map->type);
	if ((map->offset & (~PAGE_MASK)) || (map->size & (~PAGE_MASK))) {
		drm_free(map, sizeof(*map), DRM_MEM_MAPS);
		return -EINVAL;
	}
	map->mtrr   = -1;
	map->handle = 0;

	switch (map->type) {
	case _DRM_REGISTERS:
	case _DRM_FRAME_BUFFER:
#ifndef __sparc__
		if (map->offset + map->size < map->offset
		    || map->offset < virt_to_phys(high_memory)) {
			drm_free(map, sizeof(*map), DRM_MEM_MAPS);
			return -EINVAL;
		}
#endif
#ifdef CONFIG_MTRR
		if (map->type == _DRM_FRAME_BUFFER
		    || (map->flags & _DRM_WRITE_COMBINING)) {
			map->mtrr = mtrr_add(map->offset, map->size,
					     MTRR_TYPE_WRCOMB, 1);
		}
#endif
		map->handle = drm_ioremap(map->offset, map->size, dev);
		break;
			

	case _DRM_SHM:
		map->handle = (void *)drm_alloc_pages(drm_order(map->size)
						      - PAGE_SHIFT,
						      DRM_MEM_SAREA);
		DRM_DEBUG("%ld %d %p\n", map->size, drm_order(map->size),
			  map->handle);
		if (!map->handle) {
			drm_free(map, sizeof(*map), DRM_MEM_MAPS);
			return -ENOMEM;
		}
		map->offset = (unsigned long)map->handle;
		if (map->flags & _DRM_CONTAINS_LOCK) {
			dev->lock.hw_lock = map->handle; /* Pointer to lock */
		}
		break;
#if defined(CONFIG_AGP) || defined(CONFIG_AGP_MODULE)
	case _DRM_AGP:
		map->offset = map->offset + dev->agp->base;
		break;
#endif
	default:
		drm_free(map, sizeof(*map), DRM_MEM_MAPS);
		return -EINVAL;
	}

	down(&dev->struct_sem);
	if (dev->maplist) {
		++dev->map_count;
		dev->maplist = drm_realloc(dev->maplist,
					   (dev->map_count-1)
					   * sizeof(*dev->maplist),
					   dev->map_count
					   * sizeof(*dev->maplist),
					   DRM_MEM_MAPS);
	} else {
		dev->map_count = 1;
		dev->maplist = drm_alloc(dev->map_count*sizeof(*dev->maplist),
					 DRM_MEM_MAPS);
	}
	dev->maplist[dev->map_count-1] = map;
	up(&dev->struct_sem);

	if (copy_to_user((drm_map_t *)arg, map, sizeof(*map)))
		return -EFAULT;
	if (map->type != _DRM_SHM) {
		if (copy_to_user(&((drm_map_t *)arg)->handle,
				 &map->offset,
				 sizeof(map->offset)))
			return -EFAULT;
	}		
	return 0;
}

int drm_addbufs(struct inode *inode, struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	drm_file_t	 *priv	 = filp->private_data;
	drm_device_t	 *dev	 = priv->dev;
	drm_device_dma_t *dma	 = dev->dma;
	drm_buf_desc_t	 request;
	int		 count;
	int		 order;
	int		 size;
	int		 total;
	int		 page_order;
	drm_buf_entry_t	 *entry;
	unsigned long	 page;
	drm_buf_t	 *buf;
	int		 alignment;
	unsigned long	 offset;
	int		 i;
	int		 byte_count;
	int		 page_count;

	if (!dma) return -EINVAL;

	if (copy_from_user(&request,
			   (drm_buf_desc_t *)arg,
			   sizeof(request)))
		return -EFAULT;

	count	   = request.count;
	order	   = drm_order(request.size);
	size	   = 1 << order;
	
	DRM_DEBUG("count = %d, size = %d (%d), order = %d, queue_count = %d\n",
		  request.count, request.size, size, order, dev->queue_count);

	if (order < DRM_MIN_ORDER || order > DRM_MAX_ORDER) return -EINVAL;
	if (dev->queue_count) return -EBUSY; /* Not while in use */

	alignment  = (request.flags & _DRM_PAGE_ALIGN) ? PAGE_ALIGN(size):size;
	page_order = order - PAGE_SHIFT > 0 ? order - PAGE_SHIFT : 0;
	total	   = PAGE_SIZE << page_order;

	spin_lock(&dev->count_lock);
	if (dev->buf_use) {
		spin_unlock(&dev->count_lock);
		return -EBUSY;
	}
	atomic_inc(&dev->buf_alloc);
	spin_unlock(&dev->count_lock);
	
	down(&dev->struct_sem);
	entry = &dma->bufs[order];
	if (entry->buf_count) {
		up(&dev->struct_sem);
		atomic_dec(&dev->buf_alloc);
		return -ENOMEM;	/* May only call once for each order */
	}
	
	if(count < 0 || count > 4096)
	{
		up(&dev->struct_sem);
		return -EINVAL;
	}
	
	entry->buflist = drm_alloc(count * sizeof(*entry->buflist),
				   DRM_MEM_BUFS);
	if (!entry->buflist) {
		up(&dev->struct_sem);
		atomic_dec(&dev->buf_alloc);
		return -ENOMEM;
	}
	memset(entry->buflist, 0, count * sizeof(*entry->buflist));

	entry->seglist = drm_alloc(count * sizeof(*entry->seglist),
				   DRM_MEM_SEGS);
	if (!entry->seglist) {
		drm_free(entry->buflist,
			 count * sizeof(*entry->buflist),
			 DRM_MEM_BUFS);
		up(&dev->struct_sem);
		atomic_dec(&dev->buf_alloc);
		return -ENOMEM;
	}
	memset(entry->seglist, 0, count * sizeof(*entry->seglist));

	dma->pagelist = drm_realloc(dma->pagelist,
				    dma->page_count * sizeof(*dma->pagelist),
				    (dma->page_count + (count << page_order))
				    * sizeof(*dma->pagelist),
				    DRM_MEM_PAGES);
	DRM_DEBUG("pagelist: %d entries\n",
		  dma->page_count + (count << page_order));


	entry->buf_size	  = size;
	entry->page_order = page_order;
	byte_count	  = 0;
	page_count	  = 0;
	while (entry->buf_count < count) {
		if (!(page = drm_alloc_pages(page_order, DRM_MEM_DMA))) break;
		entry->seglist[entry->seg_count++] = page;
		for (i = 0; i < (1 << page_order); i++) {
			DRM_DEBUG("page %d @ 0x%08lx\n",
				  dma->page_count + page_count,
				  page + PAGE_SIZE * i);
			dma->pagelist[dma->page_count + page_count++]
				= page + PAGE_SIZE * i;
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
			buf->address = (void *)(page + offset);
			buf->next    = NULL;
			buf->waiting = 0;
			buf->pending = 0;
			init_waitqueue_head(&buf->dma_wait);
			buf->pid     = 0;
#if DRM_DMA_HISTOGRAM
			buf->time_queued     = 0;
			buf->time_dispatched = 0;
			buf->time_completed  = 0;
			buf->time_freed	     = 0;
#endif
			DRM_DEBUG("buffer %d @ %p\n",
				  entry->buf_count, buf->address);
		}
		byte_count += PAGE_SIZE << page_order;
	}

	dma->buflist = drm_realloc(dma->buflist,
				   dma->buf_count * sizeof(*dma->buflist),
				   (dma->buf_count + entry->buf_count)
				   * sizeof(*dma->buflist),
				   DRM_MEM_BUFS);
	for (i = dma->buf_count; i < dma->buf_count + entry->buf_count; i++)
		dma->buflist[i] = &entry->buflist[i - dma->buf_count];

	dma->buf_count	+= entry->buf_count;
	dma->seg_count	+= entry->seg_count;
	dma->page_count += entry->seg_count << page_order;
	dma->byte_count += PAGE_SIZE * (entry->seg_count << page_order);
	
	drm_freelist_create(&entry->freelist, entry->buf_count);
	for (i = 0; i < entry->buf_count; i++) {
		drm_freelist_put(dev, &entry->freelist, &entry->buflist[i]);
	}
	
	up(&dev->struct_sem);

	request.count = entry->buf_count;
	request.size  = size;

	if (copy_to_user((drm_buf_desc_t *)arg,
			 &request,
			 sizeof(request)))
		return -EFAULT;
	
	atomic_dec(&dev->buf_alloc);
	return 0;
}

int drm_infobufs(struct inode *inode, struct file *filp, unsigned int cmd,
		 unsigned long arg)
{
	drm_file_t	 *priv	 = filp->private_data;
	drm_device_t	 *dev	 = priv->dev;
	drm_device_dma_t *dma	 = dev->dma;
	drm_buf_info_t	 request;
	int		 i;
	int		 count;

	if (!dma) return -EINVAL;

	spin_lock(&dev->count_lock);
	if (atomic_read(&dev->buf_alloc)) {
		spin_unlock(&dev->count_lock);
		return -EBUSY;
	}
	++dev->buf_use;		/* Can't allocate more after this call */
	spin_unlock(&dev->count_lock);

	if (copy_from_user(&request,
			   (drm_buf_info_t *)arg,
			   sizeof(request)))
		return -EFAULT;

	for (i = 0, count = 0; i < DRM_MAX_ORDER+1; i++) {
		if (dma->bufs[i].buf_count) ++count;
	}
	
	DRM_DEBUG("count = %d\n", count);
	
	if (request.count >= count) {
		for (i = 0, count = 0; i < DRM_MAX_ORDER+1; i++) {
			if (dma->bufs[i].buf_count) {
				if (copy_to_user(&request.list[count].count,
						 &dma->bufs[i].buf_count,
						 sizeof(dma->bufs[0]
							.buf_count)) ||
				    copy_to_user(&request.list[count].size,
						 &dma->bufs[i].buf_size,
						 sizeof(dma->bufs[0].buf_size)) ||
				    copy_to_user(&request.list[count].low_mark,
						 &dma->bufs[i]
						 .freelist.low_mark,
						 sizeof(dma->bufs[0]
							.freelist.low_mark)) ||
				    copy_to_user(&request.list[count]
						 .high_mark,
						 &dma->bufs[i]
						 .freelist.high_mark,
						 sizeof(dma->bufs[0]
							.freelist.high_mark)))
					return -EFAULT;

				DRM_DEBUG("%d %d %d %d %d\n",
					  i,
					  dma->bufs[i].buf_count,
					  dma->bufs[i].buf_size,
					  dma->bufs[i].freelist.low_mark,
					  dma->bufs[i].freelist.high_mark);
				++count;
			}
		}
	}
	request.count = count;

	if (copy_to_user((drm_buf_info_t *)arg,
			 &request,
			 sizeof(request)))
		return -EFAULT;
	
	return 0;
}

int drm_markbufs(struct inode *inode, struct file *filp, unsigned int cmd,
		 unsigned long arg)
{
	drm_file_t	 *priv	 = filp->private_data;
	drm_device_t	 *dev	 = priv->dev;
	drm_device_dma_t *dma	 = dev->dma;
	drm_buf_desc_t	 request;
	int		 order;
	drm_buf_entry_t	 *entry;

	if (!dma) return -EINVAL;

	if (copy_from_user(&request,
			   (drm_buf_desc_t *)arg,
			   sizeof(request)))
		return -EFAULT;

	DRM_DEBUG("%d, %d, %d\n",
		  request.size, request.low_mark, request.high_mark);
	order = drm_order(request.size);
	if (order < DRM_MIN_ORDER || order > DRM_MAX_ORDER) return -EINVAL;
	entry = &dma->bufs[order];

	if (request.low_mark < 0 || request.low_mark > entry->buf_count)
		return -EINVAL;
	if (request.high_mark < 0 || request.high_mark > entry->buf_count)
		return -EINVAL;

	entry->freelist.low_mark  = request.low_mark;
	entry->freelist.high_mark = request.high_mark;
	
	return 0;
}

int drm_freebufs(struct inode *inode, struct file *filp, unsigned int cmd,
		 unsigned long arg)
{
	drm_file_t	 *priv	 = filp->private_data;
	drm_device_t	 *dev	 = priv->dev;
	drm_device_dma_t *dma	 = dev->dma;
	drm_buf_free_t	 request;
	int		 i;
	int		 idx;
	drm_buf_t	 *buf;

	if (!dma) return -EINVAL;

	if (copy_from_user(&request,
			   (drm_buf_free_t *)arg,
			   sizeof(request)))
		return -EFAULT;

	DRM_DEBUG("%d\n", request.count);
	for (i = 0; i < request.count; i++) {
		if (copy_from_user(&idx,
				   &request.list[i],
				   sizeof(idx)))
			return -EFAULT;
		if (idx < 0 || idx >= dma->buf_count) {
			DRM_ERROR("Index %d (of %d max)\n",
				  idx, dma->buf_count - 1);
			return -EINVAL;
		}
		buf = dma->buflist[idx];
		if (buf->pid != current->pid) {
			DRM_ERROR("Process %d freeing buffer owned by %d\n",
				  current->pid, buf->pid);
			return -EINVAL;
		}
		drm_free_buffer(dev, buf);
	}
	
	return 0;
}

int drm_mapbufs(struct inode *inode, struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	drm_file_t	 *priv	 = filp->private_data;
	drm_device_t	 *dev	 = priv->dev;
	drm_device_dma_t *dma	 = dev->dma;
	int		 retcode = 0;
	const int	 zero	 = 0;
	unsigned long	 virtual;
	unsigned long	 address;
	drm_buf_map_t	 request;
	int		 i;

	if (!dma) return -EINVAL;
	
	DRM_DEBUG("\n");

	spin_lock(&dev->count_lock);
	if (atomic_read(&dev->buf_alloc)) {
		spin_unlock(&dev->count_lock);
		return -EBUSY;
	}
	++dev->buf_use;		/* Can't allocate more after this call */
	spin_unlock(&dev->count_lock);

	if (copy_from_user(&request,
			   (drm_buf_map_t *)arg,
			   sizeof(request)))
		return -EFAULT;

	if (request.count >= dma->buf_count) {
		down_write(&current->mm->mmap_sem);
		virtual = do_mmap(filp, 0, dma->byte_count,
				  PROT_READ|PROT_WRITE, MAP_SHARED, 0);
		up_write(&current->mm->mmap_sem);
		if (virtual > -1024UL) {
				/* Real error */
			retcode = (signed long)virtual;
			goto done;
		}
		request.virtual = (void *)virtual;

		for (i = 0; i < dma->buf_count; i++) {
			if (copy_to_user(&request.list[i].idx,
					 &dma->buflist[i]->idx,
					 sizeof(request.list[0].idx))) {
				retcode = -EFAULT;
				goto done;
			}
			if (copy_to_user(&request.list[i].total,
					 &dma->buflist[i]->total,
					 sizeof(request.list[0].total))) {
				retcode = -EFAULT;
				goto done;
			}
			if (copy_to_user(&request.list[i].used,
					 &zero,
					 sizeof(zero))) {
				retcode = -EFAULT;
				goto done;
			}
			address = virtual + dma->buflist[i]->offset;
			if (copy_to_user(&request.list[i].address,
					 &address,
					 sizeof(address))) {
				retcode = -EFAULT;
				goto done;
			}
		}
	}
done:
	request.count = dma->buf_count;
	DRM_DEBUG("%d buffers, retcode = %d\n", request.count, retcode);

	if (copy_to_user((drm_buf_map_t *)arg,
			 &request,
			 sizeof(request)))
		return -EFAULT;

	return retcode;
}
