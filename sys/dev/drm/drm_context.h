/* drm_context.h -- IOCTLs for generic contexts -*- linux-c -*-
 * Created: Fri Nov 24 18:31:37 2000 by gareth@valinux.com
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
 *
 * $FreeBSD$
 */

#define __NO_VERSION__
#include "dev/drm/drmP.h"

#if __HAVE_CTX_BITMAP

/* ================================================================
 * Context bitmap support
 */

void DRM(ctxbitmap_free)( drm_device_t *dev, int ctx_handle )
{
	if ( ctx_handle < 0 ) goto failed;
	if ( !dev->ctx_bitmap ) goto failed;

	if ( ctx_handle < DRM_MAX_CTXBITMAP ) {
		DRM_OS_LOCK;
		clear_bit( ctx_handle, dev->ctx_bitmap );
		dev->context_sareas[ctx_handle] = NULL;
		DRM_OS_UNLOCK;
		return;
	}
failed:
       	DRM_ERROR( "Attempt to free invalid context handle: %d\n",
		   ctx_handle );
       	return;
}

int DRM(ctxbitmap_next)( drm_device_t *dev )
{
	int bit;

	if(!dev->ctx_bitmap) return -1;

	DRM_OS_LOCK;
	bit = find_first_zero_bit( dev->ctx_bitmap, DRM_MAX_CTXBITMAP );
	if ( bit < DRM_MAX_CTXBITMAP ) {
		set_bit( bit, dev->ctx_bitmap );
	   	DRM_DEBUG( "drm_ctxbitmap_next bit : %d\n", bit );
		if((bit+1) > dev->max_context) {
			dev->max_context = (bit+1);
			if(dev->context_sareas) {
				drm_map_t **ctx_sareas;

				ctx_sareas = DRM(realloc)(dev->context_sareas,
						(dev->max_context - 1) * 
						sizeof(*dev->context_sareas),
						dev->max_context * 
						sizeof(*dev->context_sareas),
						DRM_MEM_MAPS);
				if(!ctx_sareas) {
					clear_bit(bit, dev->ctx_bitmap);
					DRM_OS_UNLOCK;
					return -1;
				}
				dev->context_sareas = ctx_sareas;
				dev->context_sareas[bit] = NULL;
			} else {
				/* max_context == 1 at this point */
				dev->context_sareas = DRM(alloc)(
						dev->max_context * 
						sizeof(*dev->context_sareas),
						DRM_MEM_MAPS);
				if(!dev->context_sareas) {
					clear_bit(bit, dev->ctx_bitmap);
					DRM_OS_UNLOCK;
					return -1;
				}
				dev->context_sareas[bit] = NULL;
			}
		}
		DRM_OS_UNLOCK;
		return bit;
	}
	DRM_OS_UNLOCK;
	return -1;
}

int DRM(ctxbitmap_init)( drm_device_t *dev )
{
	int i;
   	int temp;

	DRM_OS_LOCK;
	dev->ctx_bitmap = (unsigned long *) DRM(alloc)( PAGE_SIZE,
							DRM_MEM_CTXBITMAP );
	if ( dev->ctx_bitmap == NULL ) {
		DRM_OS_UNLOCK;
		return DRM_OS_ERR(ENOMEM);
	}
	memset( (void *)dev->ctx_bitmap, 0, PAGE_SIZE );
	dev->context_sareas = NULL;
	dev->max_context = -1;
	DRM_OS_UNLOCK;

	for ( i = 0 ; i < DRM_RESERVED_CONTEXTS ; i++ ) {
		temp = DRM(ctxbitmap_next)( dev );
	   	DRM_DEBUG( "drm_ctxbitmap_init : %d\n", temp );
	}

	return 0;
}

void DRM(ctxbitmap_cleanup)( drm_device_t *dev )
{
	DRM_OS_LOCK;
	if( dev->context_sareas ) DRM(free)( dev->context_sareas,
					     sizeof(*dev->context_sareas) * 
					     dev->max_context,
					     DRM_MEM_MAPS );
	DRM(free)( (void *)dev->ctx_bitmap, PAGE_SIZE, DRM_MEM_CTXBITMAP );
	DRM_OS_UNLOCK;
}

/* ================================================================
 * Per Context SAREA Support
 */

int DRM(getsareactx)( DRM_OS_IOCTL )
{
	DRM_OS_DEVICE;
	drm_ctx_priv_map_t request;
	drm_map_t *map;

	DRM_OS_KRNFROMUSR( request, (drm_ctx_priv_map_t *)data, 
			   sizeof(request) );

	DRM_OS_LOCK;
	if (dev->max_context < 0 || request.ctx_id >= (unsigned) dev->max_context) {
		DRM_OS_UNLOCK;
		return DRM_OS_ERR(EINVAL);
	}

	map = dev->context_sareas[request.ctx_id];
	DRM_OS_UNLOCK;

	request.handle = map->handle;

	DRM_OS_KRNTOUSR( (drm_ctx_priv_map_t *)data, request, sizeof(request) );

	return 0;
}

int DRM(setsareactx)( DRM_OS_IOCTL )
{
	DRM_OS_DEVICE;
	drm_ctx_priv_map_t request;
	drm_map_t *map = NULL;
#ifdef __linux__
	drm_map_list_t *r_list = NULL;
	struct list_head *list;
#endif /* __linux__ */
#ifdef __FreeBSD__
	drm_map_list_entry_t *list;
#endif /* __FreeBSD__ */

	DRM_OS_KRNFROMUSR( request, (drm_ctx_priv_map_t *)data,
			   sizeof(request) );

	DRM_OS_LOCK;
#ifdef __linux__
	list_for_each(list, &dev->maplist->head) {
		r_list = (drm_map_list_t *)list;
		if(r_list->map &&
		   r_list->map->handle == request.handle) 
			goto found;
	}
#endif /* __linux__ */
#ifdef __FreeBSD__
	TAILQ_FOREACH(list, dev->maplist, link) {
		map=list->map;
		if(map->handle == request.handle) 
			goto found;
	}
#endif /* __FreeBSD__ */

bad:
	DRM_OS_UNLOCK;
	return DRM_OS_ERR(EINVAL);

found:
#ifdef __linux__
	map = r_list->map;
#endif /* __linux__ */
#ifdef __FreeBSD__
	map = list->map;
#endif /* __FreeBSD__ */
	if (!map) goto bad;
	if (dev->max_context < 0)
		goto bad;
	if (request.ctx_id >= (unsigned) dev->max_context)
		goto bad;
	dev->context_sareas[request.ctx_id] = map;
	DRM_OS_UNLOCK;
	return 0;
}

/* ================================================================
 * The actual DRM context handling routines
 */

int DRM(context_switch)( drm_device_t *dev, int old, int new )
{
        char buf[64];

        if ( test_and_set_bit( 0, &dev->context_flag ) ) {
                DRM_ERROR( "Reentering -- FIXME\n" );
                return DRM_OS_ERR(EBUSY);
        }

#if __HAVE_DMA_HISTOGRAM
        dev->ctx_start = get_cycles();
#endif

        DRM_DEBUG( "Context switch from %d to %d\n", old, new );

        if ( new == dev->last_context ) {
                clear_bit( 0, &dev->context_flag );
                return 0;
        }

        if ( DRM(flags) & DRM_FLAG_NOCTX ) {
                DRM(context_switch_complete)( dev, new );
        } else {
                sprintf( buf, "C %d %d\n", old, new );
                DRM(write_string)( dev, buf );
        }

        return 0;
}

int DRM(context_switch_complete)( drm_device_t *dev, int new )
{
        dev->last_context = new;  /* PRE/POST: This is the _only_ writer. */
        dev->last_switch  = jiffies;

        if ( !_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock) ) {
                DRM_ERROR( "Lock isn't held after context switch\n" );
        }

				/* If a context switch is ever initiated
                                   when the kernel holds the lock, release
                                   that lock here. */
#if __HAVE_DMA_HISTOGRAM
        atomic_inc( &dev->histo.ctx[DRM(histogram_slot)(get_cycles()
							- dev->ctx_start)] );

#endif
        clear_bit( 0, &dev->context_flag );
        DRM_OS_WAKEUP( &dev->context_wait );

        return 0;
}

int DRM(resctx)( DRM_OS_IOCTL )
{
	drm_ctx_res_t res;
	drm_ctx_t ctx;
	int i;

	DRM_OS_KRNFROMUSR( res, (drm_ctx_res_t *)data, sizeof(res) );

	if ( res.count >= DRM_RESERVED_CONTEXTS ) {
		memset( &ctx, 0, sizeof(ctx) );
		for ( i = 0 ; i < DRM_RESERVED_CONTEXTS ; i++ ) {
			ctx.handle = i;
			if ( DRM_OS_COPYTOUSR( &res.contexts[i],
					   &i, sizeof(i) ) )
				return DRM_OS_ERR(EFAULT);
		}
	}
	res.count = DRM_RESERVED_CONTEXTS;

	DRM_OS_KRNTOUSR( (drm_ctx_res_t *)data, res, sizeof(res) );

	return 0;
}

int DRM(addctx)( DRM_OS_IOCTL )
{
	DRM_OS_DEVICE;
	drm_ctx_t ctx;

	DRM_OS_KRNFROMUSR( ctx, (drm_ctx_t *)data, sizeof(ctx) );

	ctx.handle = DRM(ctxbitmap_next)( dev );
	if ( ctx.handle == DRM_KERNEL_CONTEXT ) {
				/* Skip kernel's context and get a new one. */
		ctx.handle = DRM(ctxbitmap_next)( dev );
	}
	DRM_DEBUG( "%d\n", ctx.handle );
	if ( ctx.handle == -1 ) {
		DRM_DEBUG( "Not enough free contexts.\n" );
				/* Should this return -EBUSY instead? */
		return DRM_OS_ERR(ENOMEM);
	}

	DRM_OS_KRNTOUSR( (drm_ctx_t *)data, ctx, sizeof(ctx) );

	return 0;
}

int DRM(modctx)( DRM_OS_IOCTL )
{
	/* This does nothing */
	return 0;
}

int DRM(getctx)( DRM_OS_IOCTL )
{
	drm_ctx_t ctx;

	DRM_OS_KRNFROMUSR( ctx, (drm_ctx_t *)data, sizeof(ctx) );

	/* This is 0, because we don't handle any context flags */
	ctx.flags = 0;

	DRM_OS_KRNTOUSR( (drm_ctx_t *)data, ctx, sizeof(ctx) );

	return 0;
}

int DRM(switchctx)( DRM_OS_IOCTL )
{
	DRM_OS_DEVICE;
	drm_ctx_t ctx;

	DRM_OS_KRNFROMUSR( ctx, (drm_ctx_t *)data, sizeof(ctx) );

	DRM_DEBUG( "%d\n", ctx.handle );
	return DRM(context_switch)( dev, dev->last_context, ctx.handle );
}

int DRM(newctx)( DRM_OS_IOCTL )
{
	DRM_OS_DEVICE;
	drm_ctx_t ctx;

	DRM_OS_KRNFROMUSR( ctx, (drm_ctx_t *)data, sizeof(ctx) );

	DRM_DEBUG( "%d\n", ctx.handle );
	DRM(context_switch_complete)( dev, ctx.handle );

	return 0;
}

int DRM(rmctx)( DRM_OS_IOCTL )
{
	DRM_OS_DEVICE;
	drm_ctx_t ctx;

	DRM_OS_KRNFROMUSR( ctx, (drm_ctx_t *)data, sizeof(ctx) );

	DRM_DEBUG( "%d\n", ctx.handle );
#ifdef __linux__
	if ( ctx.handle == DRM_KERNEL_CONTEXT + 1 ) {
		priv->remove_auth_on_close = 1;
	}
#endif /* __linux__ */
	if ( ctx.handle != DRM_KERNEL_CONTEXT ) {
		DRM(ctxbitmap_free)( dev, ctx.handle );
	}

	return 0;
}


#else /* __HAVE_CTX_BITMAP */

/* ================================================================
 * Old-style context support
 */


int DRM(context_switch)(drm_device_t *dev, int old, int new)
{
	char	    buf[64];
	drm_queue_t *q;

#if 0
	atomic_inc(&dev->total_ctx);
#endif

	if (test_and_set_bit(0, &dev->context_flag)) {
		DRM_ERROR("Reentering -- FIXME\n");
		return DRM_OS_ERR(EBUSY);
	}

#if __HAVE_DMA_HISTOGRAM
	dev->ctx_start = get_cycles();
#endif

	DRM_DEBUG("Context switch from %d to %d\n", old, new);

	if (new >= dev->queue_count) {
		clear_bit(0, &dev->context_flag);
		return DRM_OS_ERR(EINVAL);
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
		return DRM_OS_ERR(EINVAL);
	}

	if (DRM(flags) & DRM_FLAG_NOCTX) {
		DRM(context_switch_complete)(dev, new);
	} else {
		sprintf(buf, "C %d %d\n", old, new);
		DRM(write_string)(dev, buf);
	}

	atomic_dec(&q->use_count);

	return 0;
}

int DRM(context_switch_complete)(drm_device_t *dev, int new)
{
	drm_device_dma_t *dma = dev->dma;

	dev->last_context = new;  /* PRE/POST: This is the _only_ writer. */
	dev->last_switch  = jiffies;

	if (!_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)) {
		DRM_ERROR("Lock isn't held after context switch\n");
	}

	if (!dma || !(dma->next_buffer && dma->next_buffer->while_locked)) {
		if (DRM(lock_free)(dev, &dev->lock.hw_lock->lock,
				  DRM_KERNEL_CONTEXT)) {
			DRM_ERROR("Cannot free lock\n");
		}
	}

#if __HAVE_DMA_HISTOGRAM
	atomic_inc(&dev->histo.ctx[DRM(histogram_slot)(get_cycles()
						      - dev->ctx_start)]);

#endif
	clear_bit(0, &dev->context_flag);
	DRM_OS_WAKEUP_INT(&dev->context_wait);

	return 0;
}

static int DRM(init_queue)(drm_device_t *dev, drm_queue_t *q, drm_ctx_t *ctx)
{
	DRM_DEBUG("\n");

	if (atomic_read(&q->use_count) != 1
	    || atomic_read(&q->finalization)
	    || atomic_read(&q->block_count)) {
		DRM_ERROR("New queue is already in use: u%ld f%ld b%ld\n",
			  (unsigned long)atomic_read(&q->use_count),
			  (unsigned long)atomic_read(&q->finalization),
			  (unsigned long)atomic_read(&q->block_count));
	}

	atomic_set(&q->finalization,  0);
	atomic_set(&q->block_count,   0);
	atomic_set(&q->block_read,    0);
	atomic_set(&q->block_write,   0);
	atomic_set(&q->total_queued,  0);
	atomic_set(&q->total_flushed, 0);
	atomic_set(&q->total_locks,   0);

#ifdef __linux__
	init_waitqueue_head(&q->write_queue);
	init_waitqueue_head(&q->read_queue);
	init_waitqueue_head(&q->flush_queue);
#endif /* __linux__ */
#ifdef __FreeBSD__
	q->write_queue = 0;
	q->read_queue = 0;
	q->flush_queue = 0;
#endif /* __FreeBSD__ */

	q->flags = ctx->flags;

	DRM(waitlist_create)(&q->waitlist, dev->dma->buf_count);

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

static int DRM(alloc_queue)(drm_device_t *dev)
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
	DRM_OS_LOCK;

	queue = gamma_alloc(sizeof(*queue), DRM_MEM_QUEUES);
	memset(queue, 0, sizeof(*queue));
	atomic_set(&queue->use_count, 1);

	++dev->queue_count;
	if (dev->queue_count >= dev->queue_slots) {
		oldslots = dev->queue_slots * sizeof(*dev->queuelist);
		if (!dev->queue_slots) dev->queue_slots = 1;
		dev->queue_slots *= 2;
		newslots = dev->queue_slots * sizeof(*dev->queuelist);

		dev->queuelist = DRM(realloc)(dev->queuelist,
					      oldslots,
					      newslots,
					      DRM_MEM_QUEUES);
		if (!dev->queuelist) {
			DRM_OS_UNLOCK;
			DRM_DEBUG("out of memory\n");
			return DRM_OS_ERR(ENOMEM);
		}
	}
	dev->queuelist[dev->queue_count-1] = queue;

	DRM_OS_UNLOCK;
	DRM_DEBUG("%d (new)\n", dev->queue_count - 1);
	return dev->queue_count - 1;
}

int DRM(resctx)( DRM_OS_IOCTL )
{
	drm_ctx_res_t	res;
	drm_ctx_t	ctx;
	int		i;

	DRM_DEBUG("%d\n", DRM_RESERVED_CONTEXTS);
	
	DRM_OS_KRNFROMUSR( res, (drm_ctx_res_t *)data, sizeof(res) );

	if (res.count >= DRM_RESERVED_CONTEXTS) {
		memset(&ctx, 0, sizeof(ctx));
		for (i = 0; i < DRM_RESERVED_CONTEXTS; i++) {
			ctx.handle = i;
			if (DRM_OS_COPYTOUSR(&res.contexts[i],
					 &i,
					 sizeof(i)))
				return DRM_OS_ERR(EFAULT);
		}
	}
	res.count = DRM_RESERVED_CONTEXTS;

	DRM_OS_KRNTOUSR( (drm_ctx_res_t *)data, res, sizeof(res) );

	return 0;
}

int DRM(addctx)( DRM_OS_IOCTL )
{
	DRM_OS_DEVICE;
	drm_ctx_t	ctx;

	DRM_OS_KRNFROMUSR( ctx, (drm_ctx_t *)data, sizeof(ctx) );

	if ((ctx.handle = DRM(alloc_queue)(dev)) == DRM_KERNEL_CONTEXT) {
				/* Init kernel's context and get a new one. */
		DRM(init_queue)(dev, dev->queuelist[ctx.handle], &ctx);
		ctx.handle = DRM(alloc_queue)(dev);
	}
	DRM(init_queue)(dev, dev->queuelist[ctx.handle], &ctx);
	DRM_DEBUG("%d\n", ctx.handle);
	
	DRM_OS_KRNTOUSR( (drm_ctx_t *)data, ctx, sizeof(ctx) );

	return 0;
}

int DRM(modctx)( DRM_OS_IOCTL )
{
	DRM_OS_DEVICE;
	drm_ctx_t	ctx;
	drm_queue_t	*q;

	DRM_OS_KRNFROMUSR( ctx, (drm_ctx_t *)data, sizeof(ctx) );

	DRM_DEBUG("%d\n", ctx.handle);

	if (ctx.handle < 0 || ctx.handle >= dev->queue_count) 
		return DRM_OS_ERR(EINVAL);
	q = dev->queuelist[ctx.handle];

	atomic_inc(&q->use_count);
	if (atomic_read(&q->use_count) == 1) {
				/* No longer in use */
		atomic_dec(&q->use_count);
		return DRM_OS_ERR(EINVAL);
	}

	if (DRM_BUFCOUNT(&q->waitlist)) {
		atomic_dec(&q->use_count);
		return DRM_OS_ERR(EBUSY);
	}

	q->flags = ctx.flags;

	atomic_dec(&q->use_count);
	return 0;
}

int DRM(getctx)( DRM_OS_IOCTL )
{
	DRM_OS_DEVICE;
	drm_ctx_t	ctx;
	drm_queue_t	*q;

	DRM_OS_KRNFROMUSR( ctx, (drm_ctx_t *)data, sizeof(ctx) );

	DRM_DEBUG("%d\n", ctx.handle);

	if (ctx.handle >= dev->queue_count) 
		return DRM_OS_ERR(EINVAL);
	q = dev->queuelist[ctx.handle];

	atomic_inc(&q->use_count);
	if (atomic_read(&q->use_count) == 1) {
				/* No longer in use */
		atomic_dec(&q->use_count);
		return DRM_OS_ERR(EINVAL);
	}

	ctx.flags = q->flags;
	atomic_dec(&q->use_count);

	DRM_OS_KRNTOUSR( (drm_ctx_t *)data, ctx, sizeof(ctx) );

	return 0;
}

int DRM(switchctx)( DRM_OS_IOCTL )
{
	DRM_OS_DEVICE;
	drm_ctx_t	ctx;

	DRM_OS_KRNFROMUSR( ctx, (drm_ctx_t *)data, sizeof(ctx) );

	DRM_DEBUG("%d\n", ctx.handle);
	return DRM(context_switch)(dev, dev->last_context, ctx.handle);
}

int DRM(newctx)( DRM_OS_IOCTL )
{
	DRM_OS_DEVICE;
	drm_ctx_t	ctx;

	DRM_OS_KRNFROMUSR( ctx, (drm_ctx_t *)data, sizeof(ctx) );

	DRM_DEBUG("%d\n", ctx.handle);
	DRM(context_switch_complete)(dev, ctx.handle);

	return 0;
}

int DRM(rmctx)( DRM_OS_IOCTL )
{
	DRM_OS_DEVICE;
	drm_ctx_t	ctx;
	drm_queue_t	*q;
	drm_buf_t	*buf;

	DRM_OS_KRNFROMUSR( ctx, (drm_ctx_t *)data, sizeof(ctx) );

	DRM_DEBUG("%d\n", ctx.handle);

	if (ctx.handle >= dev->queue_count) return DRM_OS_ERR(EINVAL);
	q = dev->queuelist[ctx.handle];

	atomic_inc(&q->use_count);
	if (atomic_read(&q->use_count) == 1) {
				/* No longer in use */
		atomic_dec(&q->use_count);
		return DRM_OS_ERR(EINVAL);
	}

	atomic_inc(&q->finalization); /* Mark queue in finalization state */
	atomic_sub(2, &q->use_count); /* Mark queue as unused (pending
					 finalization) */

	while (test_and_set_bit(0, &dev->interrupt_flag)) {
#ifdef __linux__
		schedule();
		if (signal_pending(current)) {
			clear_bit(0, &dev->interrupt_flag);
			return DRM_OS_ERR(EINTR);
		}
#endif /* __linux__ */
#ifdef __FreeBSD__
		static int never;
		int retcode;
		retcode = tsleep(&never, PZERO|PCATCH, "never", 1);
		if (retcode)
			return retcode;
#endif /* __FreeBSD__ */
	}
				/* Remove queued buffers */
	while ((buf = DRM(waitlist_get)(&q->waitlist))) {
		DRM(free_buffer)(dev, buf);
	}
	clear_bit(0, &dev->interrupt_flag);

				/* Wakeup blocked processes */
#ifdef __linux__
	wake_up_interruptible(&q->read_queue);
	wake_up_interruptible(&q->write_queue);
#endif /* __linux__ */
#ifdef __FreeBSD__
	wakeup( &q->block_read );
	wakeup( &q->block_write );
#endif /* __FreeBSD__ */
	DRM_OS_WAKEUP_INT( &q->flush_queue );
				/* Finalization over.  Queue is made
				   available when both use_count and
				   finalization become 0, which won't
				   happen until all the waiting processes
				   stop waiting. */
	atomic_dec(&q->finalization);
	return 0;
}

#endif /* __HAVE_CTX_BITMAP */
