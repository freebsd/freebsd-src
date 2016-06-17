/* dma.c -- DMA IOCTL and function support -*- linux-c -*-
 * Created: Fri Mar 19 14:30:16 1999 by faith@precisioninsight.com
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
 *    Rickard E. (Rik) Faith <faith@valinuxa.com>
 *
 */

#define __NO_VERSION__
#include "drmP.h"

#include <linux/interrupt.h>	/* For task queue support */

void drm_dma_setup(drm_device_t *dev)
{
	int i;
	
	if (!(dev->dma = drm_alloc(sizeof(*dev->dma), DRM_MEM_DRIVER))) {
                printk(KERN_ERR "drm_dma_setup: can't drm_alloc dev->dma");
                return;
        }       
	memset(dev->dma, 0, sizeof(*dev->dma));
	for (i = 0; i <= DRM_MAX_ORDER; i++)
		memset(&dev->dma->bufs[i], 0, sizeof(dev->dma->bufs[0]));
}

void drm_dma_takedown(drm_device_t *dev)
{
	drm_device_dma_t  *dma = dev->dma;
	int		  i, j;

	if (!dma) return;
	
				/* Clear dma buffers */
	for (i = 0; i <= DRM_MAX_ORDER; i++) {
		if (dma->bufs[i].seg_count) {
			DRM_DEBUG("order %d: buf_count = %d,"
				  " seg_count = %d\n",
				  i,
				  dma->bufs[i].buf_count,
				  dma->bufs[i].seg_count);
			for (j = 0; j < dma->bufs[i].seg_count; j++) {
				drm_free_pages(dma->bufs[i].seglist[j],
					       dma->bufs[i].page_order,
					       DRM_MEM_DMA);
			}
			drm_free(dma->bufs[i].seglist,
				 dma->bufs[i].seg_count
				 * sizeof(*dma->bufs[0].seglist),
				 DRM_MEM_SEGS);
		}
	   	if(dma->bufs[i].buf_count) {
		   	for(j = 0; j < dma->bufs[i].buf_count; j++) {
			   if(dma->bufs[i].buflist[j].dev_private) {
			      drm_free(dma->bufs[i].buflist[j].dev_private,
				       dma->bufs[i].buflist[j].dev_priv_size,
				       DRM_MEM_BUFS);
			   }
			}
		   	drm_free(dma->bufs[i].buflist,
				 dma->bufs[i].buf_count *
				 sizeof(*dma->bufs[0].buflist),
				 DRM_MEM_BUFS);
		   	drm_freelist_destroy(&dma->bufs[i].freelist);
		}
	}
	
	if (dma->buflist) {
		drm_free(dma->buflist,
			 dma->buf_count * sizeof(*dma->buflist),
			 DRM_MEM_BUFS);
	}

	if (dma->pagelist) {
		drm_free(dma->pagelist,
			 dma->page_count * sizeof(*dma->pagelist),
			 DRM_MEM_PAGES);
	}
	drm_free(dev->dma, sizeof(*dev->dma), DRM_MEM_DRIVER);
	dev->dma = NULL;
}

#if DRM_DMA_HISTOGRAM
/* This is slow, but is useful for debugging. */
int drm_histogram_slot(unsigned long count)
{
	int value = DRM_DMA_HISTOGRAM_INITIAL;
	int slot;

	for (slot = 0;
	     slot < DRM_DMA_HISTOGRAM_SLOTS;
	     ++slot, value = DRM_DMA_HISTOGRAM_NEXT(value)) {
		if (count < value) return slot;
	}
	return DRM_DMA_HISTOGRAM_SLOTS - 1;
}

void drm_histogram_compute(drm_device_t *dev, drm_buf_t *buf)
{
	cycles_t queued_to_dispatched;
	cycles_t dispatched_to_completed;
	cycles_t completed_to_freed;
	int	 q2d, d2c, c2f, q2c, q2f;
	
	if (buf->time_queued) {
		queued_to_dispatched	= (buf->time_dispatched
					   - buf->time_queued);
		dispatched_to_completed = (buf->time_completed
					   - buf->time_dispatched);
		completed_to_freed	= (buf->time_freed
					   - buf->time_completed);

		q2d = drm_histogram_slot(queued_to_dispatched);
		d2c = drm_histogram_slot(dispatched_to_completed);
		c2f = drm_histogram_slot(completed_to_freed);

		q2c = drm_histogram_slot(queued_to_dispatched
					 + dispatched_to_completed);
		q2f = drm_histogram_slot(queued_to_dispatched
					 + dispatched_to_completed
					 + completed_to_freed);
		
		atomic_inc(&dev->histo.total);
		atomic_inc(&dev->histo.queued_to_dispatched[q2d]);
		atomic_inc(&dev->histo.dispatched_to_completed[d2c]);
		atomic_inc(&dev->histo.completed_to_freed[c2f]);
		
		atomic_inc(&dev->histo.queued_to_completed[q2c]);
		atomic_inc(&dev->histo.queued_to_freed[q2f]);

	}
	buf->time_queued     = 0;
	buf->time_dispatched = 0;
	buf->time_completed  = 0;
	buf->time_freed	     = 0;
}
#endif

void drm_free_buffer(drm_device_t *dev, drm_buf_t *buf)
{
	drm_device_dma_t *dma = dev->dma;

	if (!buf) return;
	
	buf->waiting  = 0;
	buf->pending  = 0;
	buf->pid      = 0;
	buf->used     = 0;
#if DRM_DMA_HISTOGRAM
	buf->time_completed = get_cycles();
#endif
	if (waitqueue_active(&buf->dma_wait)) {
		wake_up_interruptible(&buf->dma_wait);
	} else {
				/* If processes are waiting, the last one
				   to wake will put the buffer on the free
				   list.  If no processes are waiting, we
				   put the buffer on the freelist here. */
		drm_freelist_put(dev, &dma->bufs[buf->order].freelist, buf);
	}
}

void drm_reclaim_buffers(drm_device_t *dev, pid_t pid)
{
	drm_device_dma_t *dma = dev->dma;
	int		 i;

	if (!dma) return;
	for (i = 0; i < dma->buf_count; i++) {
		if (dma->buflist[i]->pid == pid) {
			switch (dma->buflist[i]->list) {
			case DRM_LIST_NONE:
				drm_free_buffer(dev, dma->buflist[i]);
				break;
			case DRM_LIST_WAIT:
				dma->buflist[i]->list = DRM_LIST_RECLAIM;
				break;
			default:
				/* Buffer already on hardware. */
				break;
			}
		}
	}
}

int drm_context_switch(drm_device_t *dev, int old, int new)
{
	char	    buf[64];
	drm_queue_t *q;

	atomic_inc(&dev->total_ctx);

	if (test_and_set_bit(0, &dev->context_flag)) {
		DRM_ERROR("Reentering -- FIXME\n");
		return -EBUSY;
	}

#if DRM_DMA_HISTOGRAM
	dev->ctx_start = get_cycles();
#endif
	
	DRM_DEBUG("Context switch from %d to %d\n", old, new);

	if (new >= dev->queue_count) {
		clear_bit(0, &dev->context_flag);
		return -EINVAL;
	}

	if (new == dev->last_context) {
		clear_bit(0, &dev->context_flag);
		return 0;
	}
	
	q = dev->queuelist[new];
	atomic_inc(&q->use_count);
	if (atomic_read(&q->use_count) == 1) {
		atomic_dec(&q->use_count);
		clear_bit(0, &dev->context_flag);
		return -EINVAL;
	}

	if (drm_flags & DRM_FLAG_NOCTX) {
		drm_context_switch_complete(dev, new);
	} else {
		sprintf(buf, "C %d %d\n", old, new);
		drm_write_string(dev, buf);
	}
	
	atomic_dec(&q->use_count);
	
	return 0;
}

int drm_context_switch_complete(drm_device_t *dev, int new)
{
	drm_device_dma_t *dma = dev->dma;
	
	dev->last_context = new;  /* PRE/POST: This is the _only_ writer. */
	dev->last_switch  = jiffies;
	
	if (!_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)) {
		DRM_ERROR("Lock isn't held after context switch\n");
	}

	if (!dma || !(dma->next_buffer && dma->next_buffer->while_locked)) {
		if (drm_lock_free(dev, &dev->lock.hw_lock->lock,
				  DRM_KERNEL_CONTEXT)) {
			DRM_ERROR("Cannot free lock\n");
		}
	}
	
#if DRM_DMA_HISTOGRAM
	atomic_inc(&dev->histo.ctx[drm_histogram_slot(get_cycles()
						      - dev->ctx_start)]);
		   
#endif
	clear_bit(0, &dev->context_flag);
	wake_up_interruptible(&dev->context_wait);
	
	return 0;
}

void drm_clear_next_buffer(drm_device_t *dev)
{
	drm_device_dma_t *dma = dev->dma;
	
	dma->next_buffer = NULL;
	if (dma->next_queue && !DRM_BUFCOUNT(&dma->next_queue->waitlist)) {
		wake_up_interruptible(&dma->next_queue->flush_queue);
	}
	dma->next_queue	 = NULL;
}


int drm_select_queue(drm_device_t *dev, void (*wrapper)(unsigned long))
{
	int	   i;
	int	   candidate = -1;
	int	   j	     = jiffies;

	if (!dev) {
		DRM_ERROR("No device\n");
		return -1;
	}
	if (!dev->queuelist || !dev->queuelist[DRM_KERNEL_CONTEXT]) {
				/* This only happens between the time the
				   interrupt is initialized and the time
				   the queues are initialized. */
		return -1;
	}

				/* Doing "while locked" DMA? */
	if (DRM_WAITCOUNT(dev, DRM_KERNEL_CONTEXT)) {
		return DRM_KERNEL_CONTEXT;
	}

				/* If there are buffers on the last_context
				   queue, and we have not been executing
				   this context very long, continue to
				   execute this context. */
	if (dev->last_switch <= j
	    && dev->last_switch + DRM_TIME_SLICE > j
	    && DRM_WAITCOUNT(dev, dev->last_context)) {
		return dev->last_context;
	}

				/* Otherwise, find a candidate */
	for (i = dev->last_checked + 1; i < dev->queue_count; i++) {
		if (DRM_WAITCOUNT(dev, i)) {
			candidate = dev->last_checked = i;
			break;
		}
	}

	if (candidate < 0) {
		for (i = 0; i < dev->queue_count; i++) {
			if (DRM_WAITCOUNT(dev, i)) {
				candidate = dev->last_checked = i;
				break;
			}
		}
	}

	if (wrapper
	    && candidate >= 0
	    && candidate != dev->last_context
	    && dev->last_switch <= j
	    && dev->last_switch + DRM_TIME_SLICE > j) {
		if (dev->timer.expires != dev->last_switch + DRM_TIME_SLICE) {
			del_timer(&dev->timer);
			dev->timer.function = wrapper;
			dev->timer.data	    = (unsigned long)dev;
			dev->timer.expires  = dev->last_switch+DRM_TIME_SLICE;
			add_timer(&dev->timer);
		}
		return -1;
	}

	return candidate;
}


int drm_dma_enqueue(drm_device_t *dev, drm_dma_t *d)
{
	int		  i;
	drm_queue_t	  *q;
	drm_buf_t	  *buf;
	int		  idx;
	int		  while_locked = 0;
	drm_device_dma_t  *dma = dev->dma;
	DECLARE_WAITQUEUE(entry, current);

	DRM_DEBUG("%d\n", d->send_count);

	if (d->flags & _DRM_DMA_WHILE_LOCKED) {
		int context = dev->lock.hw_lock->lock;
		
		if (!_DRM_LOCK_IS_HELD(context)) {
			DRM_ERROR("No lock held during \"while locked\""
				  " request\n");
			return -EINVAL;
		}
		if (d->context != _DRM_LOCKING_CONTEXT(context)
		    && _DRM_LOCKING_CONTEXT(context) != DRM_KERNEL_CONTEXT) {
			DRM_ERROR("Lock held by %d while %d makes"
				  " \"while locked\" request\n",
				  _DRM_LOCKING_CONTEXT(context),
				  d->context);
			return -EINVAL;
		}
		q = dev->queuelist[DRM_KERNEL_CONTEXT];
		while_locked = 1;
	} else {
		q = dev->queuelist[d->context];
	}


	atomic_inc(&q->use_count);
	if (atomic_read(&q->block_write)) {
		add_wait_queue(&q->write_queue, &entry);
		atomic_inc(&q->block_count);
		for (;;) {
			current->state = TASK_INTERRUPTIBLE;
			if (!atomic_read(&q->block_write)) break;
			schedule();
			if (signal_pending(current)) {
				atomic_dec(&q->use_count);
				remove_wait_queue(&q->write_queue, &entry);
				return -EINTR;
			}
		}
		atomic_dec(&q->block_count);
		current->state = TASK_RUNNING;
		remove_wait_queue(&q->write_queue, &entry);
	}
	
	for (i = 0; i < d->send_count; i++) {
		idx = d->send_indices[i];
		if (idx < 0 || idx >= dma->buf_count) {
			atomic_dec(&q->use_count);
			DRM_ERROR("Index %d (of %d max)\n",
				  d->send_indices[i], dma->buf_count - 1);
			return -EINVAL;
		}
		buf = dma->buflist[ idx ];
		if (buf->pid != current->pid) {
			atomic_dec(&q->use_count);
			DRM_ERROR("Process %d using buffer owned by %d\n",
				  current->pid, buf->pid);
			return -EINVAL;
		}
		if (buf->list != DRM_LIST_NONE) {
			atomic_dec(&q->use_count);
			DRM_ERROR("Process %d using buffer %d on list %d\n",
				  current->pid, buf->idx, buf->list);
		}
		buf->used	  = d->send_sizes[i];
		buf->while_locked = while_locked;
		buf->context	  = d->context;
		if (!buf->used) {
			DRM_ERROR("Queueing 0 length buffer\n");
		}
		if (buf->pending) {
			atomic_dec(&q->use_count);
			DRM_ERROR("Queueing pending buffer:"
				  " buffer %d, offset %d\n",
				  d->send_indices[i], i);
			return -EINVAL;
		}
		if (buf->waiting) {
			atomic_dec(&q->use_count);
			DRM_ERROR("Queueing waiting buffer:"
				  " buffer %d, offset %d\n",
				  d->send_indices[i], i);
			return -EINVAL;
		}
		buf->waiting = 1;
		if (atomic_read(&q->use_count) == 1
		    || atomic_read(&q->finalization)) {
			drm_free_buffer(dev, buf);
		} else {
			drm_waitlist_put(&q->waitlist, buf);
			atomic_inc(&q->total_queued);
		}
	}
	atomic_dec(&q->use_count);
	
	return 0;
}

static int drm_dma_get_buffers_of_order(drm_device_t *dev, drm_dma_t *d,
					int order)
{
	int		  i;
	drm_buf_t	  *buf;
	drm_device_dma_t  *dma = dev->dma;
	
	for (i = d->granted_count; i < d->request_count; i++) {
		buf = drm_freelist_get(&dma->bufs[order].freelist,
				       d->flags & _DRM_DMA_WAIT);
		if (!buf) break;
		if (buf->pending || buf->waiting) {
			DRM_ERROR("Free buffer %d in use by %d (w%d, p%d)\n",
				  buf->idx,
				  buf->pid,
				  buf->waiting,
				  buf->pending);
		}
		buf->pid     = current->pid;
		if (copy_to_user(&d->request_indices[i],
				 &buf->idx,
				 sizeof(buf->idx)))
			return -EFAULT;

		if (copy_to_user(&d->request_sizes[i],
				 &buf->total,
				 sizeof(buf->total)))
			return -EFAULT;

		++d->granted_count;
	}
	return 0;
}


int drm_dma_get_buffers(drm_device_t *dev, drm_dma_t *dma)
{
	int		  order;
	int		  retcode = 0;
	int		  tmp_order;
	
	order = drm_order(dma->request_size);

	dma->granted_count = 0;
	retcode		   = drm_dma_get_buffers_of_order(dev, dma, order);

	if (dma->granted_count < dma->request_count
	    && (dma->flags & _DRM_DMA_SMALLER_OK)) {
		for (tmp_order = order - 1;
		     !retcode
			     && dma->granted_count < dma->request_count
			     && tmp_order >= DRM_MIN_ORDER;
		     --tmp_order) {
			
			retcode = drm_dma_get_buffers_of_order(dev, dma,
							       tmp_order);
		}
	}

	if (dma->granted_count < dma->request_count
	    && (dma->flags & _DRM_DMA_LARGER_OK)) {
		for (tmp_order = order + 1;
		     !retcode
			     && dma->granted_count < dma->request_count
			     && tmp_order <= DRM_MAX_ORDER;
		     ++tmp_order) {
			
			retcode = drm_dma_get_buffers_of_order(dev, dma,
							       tmp_order);
		}
	}
	return 0;
}
