/* context.c -- IOCTLs for contexts and DMA queues -*- linux-c -*-
 * Created: Tue Feb  2 08:37:54 1999 by faith@precisioninsight.com
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
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
#include "drmP.h"

static int drm_init_queue(drm_device_t *dev, drm_queue_t *q, drm_ctx_t *ctx)
{
	DRM_DEBUG("\n");
	
	if (atomic_read(&q->use_count) != 1
	    || atomic_read(&q->finalization)
	    || atomic_read(&q->block_count)) {
		DRM_ERROR("New queue is already in use: u%d f%d b%d\n",
			  atomic_read(&q->use_count),
			  atomic_read(&q->finalization),
			  atomic_read(&q->block_count));
	}
		  
	atomic_set(&q->finalization,  0);
	atomic_set(&q->block_count,   0);
	atomic_set(&q->block_read,    0);
	atomic_set(&q->block_write,   0);
	atomic_set(&q->total_queued,  0);
	atomic_set(&q->total_flushed, 0);
	atomic_set(&q->total_locks,   0);

	init_waitqueue_head(&q->write_queue);
	init_waitqueue_head(&q->read_queue);
	init_waitqueue_head(&q->flush_queue);

	q->flags = ctx->flags;

	drm_waitlist_create(&q->waitlist, dev->dma->buf_count);

	return 0;
}


/* drm_alloc_queue:
PRE: 1) dev->queuelist[0..dev->queue_count] is allocated and will not
	disappear (so all deallocation must be done after IOCTLs are off)
     2) dev->queue_count < dev->queue_slots
     3) dev->queuelist[i].use_count == 0 and
	dev->queuelist[i].finalization == 0 if i not in use 
POST: 1) dev->queuelist[i].use_count == 1
      2) dev->queue_count < dev->queue_slots */
		
static int drm_alloc_queue(drm_device_t *dev)
{
	int	    i;
	drm_queue_t *queue;
	int	    oldslots;
	int	    newslots;
				/* Check for a free queue */
	for (i = 0; i < dev->queue_count; i++) {
		atomic_inc(&dev->queuelist[i]->use_count);
		if (atomic_read(&dev->queuelist[i]->use_count) == 1
		    && !atomic_read(&dev->queuelist[i]->finalization)) {
			DRM_DEBUG("%d (free)\n", i);
			return i;
		}
		atomic_dec(&dev->queuelist[i]->use_count);
	}
				/* Allocate a new queue */
	
	queue = drm_alloc(sizeof(*queue), DRM_MEM_QUEUES);
	if(queue == NULL)
		return -ENOMEM;	

	memset(queue, 0, sizeof(*queue));
	down(&dev->struct_sem);
	atomic_set(&queue->use_count, 1);
	
	++dev->queue_count;
	if (dev->queue_count >= dev->queue_slots) {
		oldslots = dev->queue_slots * sizeof(*dev->queuelist);
		if (!dev->queue_slots) dev->queue_slots = 1;
		dev->queue_slots *= 2;
		newslots = dev->queue_slots * sizeof(*dev->queuelist);

		dev->queuelist = drm_realloc(dev->queuelist,
					     oldslots,
					     newslots,
					     DRM_MEM_QUEUES);
		if (!dev->queuelist) {
			up(&dev->struct_sem);
			DRM_DEBUG("out of memory\n");
			return -ENOMEM;
		}
	}
	dev->queuelist[dev->queue_count-1] = queue;
	
	up(&dev->struct_sem);
	DRM_DEBUG("%d (new)\n", dev->queue_count - 1);
	return dev->queue_count - 1;
}

int drm_resctx(struct inode *inode, struct file *filp, unsigned int cmd,
	       unsigned long arg)
{
	drm_ctx_res_t	res;
	drm_ctx_t	ctx;
	int		i;

	DRM_DEBUG("%d\n", DRM_RESERVED_CONTEXTS);
	if (copy_from_user(&res, (drm_ctx_res_t *)arg, sizeof(res)))
		return -EFAULT;
	if (res.count >= DRM_RESERVED_CONTEXTS) {
		memset(&ctx, 0, sizeof(ctx));
		for (i = 0; i < DRM_RESERVED_CONTEXTS; i++) {
			ctx.handle = i;
			if (copy_to_user(&res.contexts[i],
					 &i,
					 sizeof(i)))
				return -EFAULT;
		}
	}
	res.count = DRM_RESERVED_CONTEXTS;
	if (copy_to_user((drm_ctx_res_t *)arg, &res, sizeof(res)))
		return -EFAULT;
	return 0;
}


int drm_addctx(struct inode *inode, struct file *filp, unsigned int cmd,
	       unsigned long arg)
{
	drm_file_t	*priv	= filp->private_data;
	drm_device_t	*dev	= priv->dev;
	drm_ctx_t	ctx;

	if (copy_from_user(&ctx, (drm_ctx_t *)arg, sizeof(ctx)))
		return -EFAULT;
	if ((ctx.handle = drm_alloc_queue(dev)) == DRM_KERNEL_CONTEXT) {
				/* Init kernel's context and get a new one. */
		drm_init_queue(dev, dev->queuelist[ctx.handle], &ctx);
		ctx.handle = drm_alloc_queue(dev);
	}
	drm_init_queue(dev, dev->queuelist[ctx.handle], &ctx);
	DRM_DEBUG("%d\n", ctx.handle);
	if (copy_to_user((drm_ctx_t *)arg, &ctx, sizeof(ctx)))
		return -EFAULT;
	return 0;
}

int drm_modctx(struct inode *inode, struct file *filp, unsigned int cmd,
	       unsigned long arg)
{
	drm_file_t	*priv	= filp->private_data;
	drm_device_t	*dev	= priv->dev;
	drm_ctx_t	ctx;
	drm_queue_t	*q;
		
	if (copy_from_user(&ctx, (drm_ctx_t *)arg, sizeof(ctx)))
		return -EFAULT;
	
	DRM_DEBUG("%d\n", ctx.handle);
	
	if (ctx.handle < 0 || ctx.handle >= dev->queue_count) return -EINVAL;
	q = dev->queuelist[ctx.handle];
	
	atomic_inc(&q->use_count);
	if (atomic_read(&q->use_count) == 1) {
				/* No longer in use */
		atomic_dec(&q->use_count);
		return -EINVAL;
	}

	if (DRM_BUFCOUNT(&q->waitlist)) {
		atomic_dec(&q->use_count);
		return -EBUSY;
	}
	
	q->flags = ctx.flags;
	
	atomic_dec(&q->use_count);
	return 0;
}

int drm_getctx(struct inode *inode, struct file *filp, unsigned int cmd,
	       unsigned long arg)
{
	drm_file_t	*priv	= filp->private_data;
	drm_device_t	*dev	= priv->dev;
	drm_ctx_t	ctx;
	drm_queue_t	*q;
		
	if (copy_from_user(&ctx, (drm_ctx_t *)arg, sizeof(ctx)))
		return -EFAULT;
	
	DRM_DEBUG("%d\n", ctx.handle);
	
	if (ctx.handle >= dev->queue_count) return -EINVAL;
	q = dev->queuelist[ctx.handle];
	
	atomic_inc(&q->use_count);
	if (atomic_read(&q->use_count) == 1) {
				/* No longer in use */
		atomic_dec(&q->use_count);
		return -EINVAL;
	}
	
	ctx.flags = q->flags;
	atomic_dec(&q->use_count);
	
	if (copy_to_user((drm_ctx_t *)arg, &ctx, sizeof(ctx)))
		return -EFAULT;
	
	return 0;
}

int drm_switchctx(struct inode *inode, struct file *filp, unsigned int cmd,
		  unsigned long arg)
{
	drm_file_t	*priv	= filp->private_data;
	drm_device_t	*dev	= priv->dev;
	drm_ctx_t	ctx;

	if (copy_from_user(&ctx, (drm_ctx_t *)arg, sizeof(ctx)))
		return -EFAULT;
	DRM_DEBUG("%d\n", ctx.handle);
	return drm_context_switch(dev, dev->last_context, ctx.handle);
}

int drm_newctx(struct inode *inode, struct file *filp, unsigned int cmd,
	       unsigned long arg)
{
	drm_file_t	*priv	= filp->private_data;
	drm_device_t	*dev	= priv->dev;
	drm_ctx_t	ctx;

	if (copy_from_user(&ctx, (drm_ctx_t *)arg, sizeof(ctx)))
		return -EFAULT;
	DRM_DEBUG("%d\n", ctx.handle);
	drm_context_switch_complete(dev, ctx.handle);

	return 0;
}

int drm_rmctx(struct inode *inode, struct file *filp, unsigned int cmd,
	      unsigned long arg)
{
	drm_file_t	*priv	= filp->private_data;
	drm_device_t	*dev	= priv->dev;
	drm_ctx_t	ctx;
	drm_queue_t	*q;
	drm_buf_t	*buf;

	if (copy_from_user(&ctx, (drm_ctx_t *)arg, sizeof(ctx)))
		return -EFAULT;
	DRM_DEBUG("%d\n", ctx.handle);
	
	if (ctx.handle >= dev->queue_count) return -EINVAL;
	q = dev->queuelist[ctx.handle];
	
	atomic_inc(&q->use_count);
	if (atomic_read(&q->use_count) == 1) {
				/* No longer in use */
		atomic_dec(&q->use_count);
		return -EINVAL;
	}
	
	atomic_inc(&q->finalization); /* Mark queue in finalization state */
	atomic_sub(2, &q->use_count); /* Mark queue as unused (pending
					 finalization) */

	while (test_and_set_bit(0, &dev->interrupt_flag)) {
		schedule();
		if (signal_pending(current)) {
			clear_bit(0, &dev->interrupt_flag);
			return -EINTR;
		}
	}
				/* Remove queued buffers */
	while ((buf = drm_waitlist_get(&q->waitlist))) {
		drm_free_buffer(dev, buf);
	}
	clear_bit(0, &dev->interrupt_flag);
	
				/* Wakeup blocked processes */
	wake_up_interruptible(&q->read_queue);
	wake_up_interruptible(&q->write_queue);
	wake_up_interruptible(&q->flush_queue);
	
				/* Finalization over.  Queue is made
				   available when both use_count and
				   finalization become 0, which won't
				   happen until all the waiting processes
				   stop waiting. */
	atomic_dec(&q->finalization);
	return 0;
}
