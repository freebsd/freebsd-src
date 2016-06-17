/* gamma_dma.c -- DMA support for GMX 2000 -*- linux-c -*-
 * Created: Fri Mar 19 14:30:16 1999 by faith@precisioninsight.com
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

#include "gamma.h"
#include "drmP.h"
#include "drm.h"
#include "gamma_drm.h"
#include "gamma_drv.h"

#include <linux/interrupt.h>	/* For task queue support */
#include <linux/delay.h>

static inline void gamma_dma_dispatch(drm_device_t *dev, unsigned long address,
				      unsigned long length)
{
	drm_gamma_private_t *dev_priv =
				(drm_gamma_private_t *)dev->dev_private;
	mb();
	while ( GAMMA_READ(GAMMA_INFIFOSPACE) < 2) cpu_relax();
	GAMMA_WRITE(GAMMA_DMAADDRESS, address);
	while (GAMMA_READ(GAMMA_GCOMMANDSTATUS) != 4) cpu_relax();
	GAMMA_WRITE(GAMMA_DMACOUNT, length / 4);
}

void gamma_dma_quiescent_single(drm_device_t *dev)
{
	drm_gamma_private_t *dev_priv =
				(drm_gamma_private_t *)dev->dev_private;
	while (GAMMA_READ(GAMMA_DMACOUNT)) cpu_relax();

	while (GAMMA_READ(GAMMA_INFIFOSPACE) < 2) cpu_relax();

	GAMMA_WRITE(GAMMA_FILTERMODE, 1 << 10);
	GAMMA_WRITE(GAMMA_SYNC, 0);

	do {
		while (!GAMMA_READ(GAMMA_OUTFIFOWORDS))
			;
	} while (GAMMA_READ(GAMMA_OUTPUTFIFO) != GAMMA_SYNC_TAG);
}

void gamma_dma_quiescent_dual(drm_device_t *dev)
{
	drm_gamma_private_t *dev_priv =
				(drm_gamma_private_t *)dev->dev_private;
	while (GAMMA_READ(GAMMA_DMACOUNT)) cpu_relax();

	while (GAMMA_READ(GAMMA_INFIFOSPACE) < 3) cpu_relax();

	GAMMA_WRITE(GAMMA_BROADCASTMASK, 3);
	GAMMA_WRITE(GAMMA_FILTERMODE, 1 << 10);
	GAMMA_WRITE(GAMMA_SYNC, 0);

	/* Read from first MX */
	do {
		while (!GAMMA_READ(GAMMA_OUTFIFOWORDS)) cpu_relax();
	} while (GAMMA_READ(GAMMA_OUTPUTFIFO) != GAMMA_SYNC_TAG);

	/* Read from second MX */
	do {
		while (!GAMMA_READ(GAMMA_OUTFIFOWORDS + 0x10000)) cpu_relax();
	} while (GAMMA_READ(GAMMA_OUTPUTFIFO + 0x10000) != GAMMA_SYNC_TAG);
}

void gamma_dma_ready(drm_device_t *dev)
{
	drm_gamma_private_t *dev_priv =
				(drm_gamma_private_t *)dev->dev_private;
	while (GAMMA_READ(GAMMA_DMACOUNT)) cpu_relax();
}

static inline int gamma_dma_is_ready(drm_device_t *dev)
{
	drm_gamma_private_t *dev_priv =
				(drm_gamma_private_t *)dev->dev_private;
	return(!GAMMA_READ(GAMMA_DMACOUNT));
}

void gamma_dma_service(int irq, void *device, struct pt_regs *regs)
{
	drm_device_t	 *dev = (drm_device_t *)device;
	drm_device_dma_t *dma = dev->dma;
	drm_gamma_private_t *dev_priv =
				(drm_gamma_private_t *)dev->dev_private;

	atomic_inc(&dev->counts[6]); /* _DRM_STAT_IRQ */

	while (GAMMA_READ(GAMMA_INFIFOSPACE) < 3) cpu_relax();
	GAMMA_WRITE(GAMMA_GDELAYTIMER, 0xc350/2); /* 0x05S */
	GAMMA_WRITE(GAMMA_GCOMMANDINTFLAGS, 8);
	GAMMA_WRITE(GAMMA_GINTFLAGS, 0x2001);
	if (gamma_dma_is_ready(dev)) {
				/* Free previous buffer */
		if (test_and_set_bit(0, &dev->dma_flag)) return;
		if (dma->this_buffer) {
			gamma_free_buffer(dev, dma->this_buffer);
			dma->this_buffer = NULL;
		}
		clear_bit(0, &dev->dma_flag);

				/* Dispatch new buffer */
		queue_task(&dev->tq, &tq_immediate);
		mark_bh(IMMEDIATE_BH);
	}
}

/* Only called by gamma_dma_schedule. */
static int gamma_do_dma(drm_device_t *dev, int locked)
{
	unsigned long	 address;
	unsigned long	 length;
	drm_buf_t	 *buf;
	int		 retcode = 0;
	drm_device_dma_t *dma = dev->dma;
#if DRM_DMA_HISTOGRAM
	cycles_t	 dma_start, dma_stop;
#endif

	if (test_and_set_bit(0, &dev->dma_flag)) return -EBUSY;

#if DRM_DMA_HISTOGRAM
	dma_start = get_cycles();
#endif

	if (!dma->next_buffer) {
		DRM_ERROR("No next_buffer\n");
		clear_bit(0, &dev->dma_flag);
		return -EINVAL;
	}

	buf	= dma->next_buffer;
	/* WE NOW ARE ON LOGICAL PAGES!! - using page table setup in dma_init */
	/* So we pass the buffer index value into the physical page offset */
	address = buf->idx << 12;
	length	= buf->used;

	DRM_DEBUG("context %d, buffer %d (%ld bytes)\n",
		  buf->context, buf->idx, length);

	if (buf->list == DRM_LIST_RECLAIM) {
		gamma_clear_next_buffer(dev);
		gamma_free_buffer(dev, buf);
		clear_bit(0, &dev->dma_flag);
		return -EINVAL;
	}

	if (!length) {
		DRM_ERROR("0 length buffer\n");
		gamma_clear_next_buffer(dev);
		gamma_free_buffer(dev, buf);
		clear_bit(0, &dev->dma_flag);
		return 0;
	}

	if (!gamma_dma_is_ready(dev)) {
		clear_bit(0, &dev->dma_flag);
		return -EBUSY;
	}

	if (buf->while_locked) {
		if (!_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)) {
			DRM_ERROR("Dispatching buffer %d from pid %d"
				  " \"while locked\", but no lock held\n",
				  buf->idx, buf->pid);
		}
	} else {
		if (!locked && !gamma_lock_take(&dev->lock.hw_lock->lock,
					      DRM_KERNEL_CONTEXT)) {
			clear_bit(0, &dev->dma_flag);
			return -EBUSY;
		}
	}

	if (dev->last_context != buf->context
	    && !(dev->queuelist[buf->context]->flags
		 & _DRM_CONTEXT_PRESERVED)) {
				/* PRE: dev->last_context != buf->context */
		if (DRM(context_switch)(dev, dev->last_context,
					buf->context)) {
			DRM(clear_next_buffer)(dev);
			DRM(free_buffer)(dev, buf);
		}
		retcode = -EBUSY;
		goto cleanup;

				/* POST: we will wait for the context
				   switch and will dispatch on a later call
				   when dev->last_context == buf->context.
				   NOTE WE HOLD THE LOCK THROUGHOUT THIS
				   TIME! */
	}

	gamma_clear_next_buffer(dev);
	buf->pending	 = 1;
	buf->waiting	 = 0;
	buf->list	 = DRM_LIST_PEND;
#if DRM_DMA_HISTOGRAM
	buf->time_dispatched = get_cycles();
#endif

	/* WE NOW ARE ON LOGICAL PAGES!!! - overriding address */
	address = buf->idx << 12;

	gamma_dma_dispatch(dev, address, length);
	gamma_free_buffer(dev, dma->this_buffer);
	dma->this_buffer = buf;

	atomic_inc(&dev->counts[7]); /* _DRM_STAT_DMA */
	atomic_add(length, &dev->counts[8]); /* _DRM_STAT_PRIMARY */

	if (!buf->while_locked && !dev->context_flag && !locked) {
		if (gamma_lock_free(dev, &dev->lock.hw_lock->lock,
				  DRM_KERNEL_CONTEXT)) {
			DRM_ERROR("\n");
		}
	}
cleanup:

	clear_bit(0, &dev->dma_flag);

#if DRM_DMA_HISTOGRAM
	dma_stop = get_cycles();
	atomic_inc(&dev->histo.dma[gamma_histogram_slot(dma_stop - dma_start)]);
#endif

	return retcode;
}

static void gamma_dma_timer_bh(unsigned long dev)
{
	gamma_dma_schedule((drm_device_t *)dev, 0);
}

void gamma_dma_immediate_bh(void *dev)
{
	gamma_dma_schedule(dev, 0);
}

int gamma_dma_schedule(drm_device_t *dev, int locked)
{
	int		 next;
	drm_queue_t	 *q;
	drm_buf_t	 *buf;
	int		 retcode   = 0;
	int		 processed = 0;
	int		 missed;
	int		 expire	   = 20;
	drm_device_dma_t *dma	   = dev->dma;
#if DRM_DMA_HISTOGRAM
	cycles_t	 schedule_start;
#endif

	if (test_and_set_bit(0, &dev->interrupt_flag)) {
				/* Not reentrant */
		atomic_inc(&dev->counts[10]); /* _DRM_STAT_MISSED */
		return -EBUSY;
	}
	missed = atomic_read(&dev->counts[10]);

#if DRM_DMA_HISTOGRAM
	schedule_start = get_cycles();
#endif

again:
	if (dev->context_flag) {
		clear_bit(0, &dev->interrupt_flag);
		return -EBUSY;
	}
	if (dma->next_buffer) {
				/* Unsent buffer that was previously
				   selected, but that couldn't be sent
				   because the lock could not be obtained
				   or the DMA engine wasn't ready.  Try
				   again. */
		if (!(retcode = gamma_do_dma(dev, locked))) ++processed;
	} else {
		do {
			next = gamma_select_queue(dev, gamma_dma_timer_bh);
			if (next >= 0) {
				q   = dev->queuelist[next];
				buf = gamma_waitlist_get(&q->waitlist);
				dma->next_buffer = buf;
				dma->next_queue	 = q;
				if (buf && buf->list == DRM_LIST_RECLAIM) {
					gamma_clear_next_buffer(dev);
					gamma_free_buffer(dev, buf);
				}
			}
		} while (next >= 0 && !dma->next_buffer);
		if (dma->next_buffer) {
			if (!(retcode = gamma_do_dma(dev, locked))) {
				++processed;
			}
		}
	}

	if (--expire) {
		if (missed != atomic_read(&dev->counts[10])) {
			if (gamma_dma_is_ready(dev)) goto again;
		}
		if (processed && gamma_dma_is_ready(dev)) {
			processed = 0;
			goto again;
		}
	}

	clear_bit(0, &dev->interrupt_flag);

#if DRM_DMA_HISTOGRAM
	atomic_inc(&dev->histo.schedule[gamma_histogram_slot(get_cycles()
							   - schedule_start)]);
#endif
	return retcode;
}

static int gamma_dma_priority(drm_device_t *dev, drm_dma_t *d)
{
	unsigned long	  address;
	unsigned long	  length;
	int		  must_free = 0;
	int		  retcode   = 0;
	int		  i;
	int		  idx;
	drm_buf_t	  *buf;
	drm_buf_t	  *last_buf = NULL;
	drm_device_dma_t  *dma	    = dev->dma;
	DECLARE_WAITQUEUE(entry, current);

				/* Turn off interrupt handling */
	while (test_and_set_bit(0, &dev->interrupt_flag)) {
		schedule();
		if (signal_pending(current)) return -EINTR;
	}
	if (!(d->flags & _DRM_DMA_WHILE_LOCKED)) {
		while (!gamma_lock_take(&dev->lock.hw_lock->lock,
				      DRM_KERNEL_CONTEXT)) {
			schedule();
			if (signal_pending(current)) {
				clear_bit(0, &dev->interrupt_flag);
				return -EINTR;
			}
		}
		++must_free;
	}

	for (i = 0; i < d->send_count; i++) {
		idx = d->send_indices[i];
		if (idx < 0 || idx >= dma->buf_count) {
			DRM_ERROR("Index %d (of %d max)\n",
				  d->send_indices[i], dma->buf_count - 1);
			continue;
		}
		buf = dma->buflist[ idx ];
		if (buf->pid != current->pid) {
			DRM_ERROR("Process %d using buffer owned by %d\n",
				  current->pid, buf->pid);
			retcode = -EINVAL;
			goto cleanup;
		}
		if (buf->list != DRM_LIST_NONE) {
			DRM_ERROR("Process %d using %d's buffer on list %d\n",
				  current->pid, buf->pid, buf->list);
			retcode = -EINVAL;
			goto cleanup;
		}
				/* This isn't a race condition on
				   buf->list, since our concern is the
				   buffer reclaim during the time the
				   process closes the /dev/drm? handle, so
				   it can't also be doing DMA. */
		buf->list	  = DRM_LIST_PRIO;
		buf->used	  = d->send_sizes[i];
		buf->context	  = d->context;
		buf->while_locked = d->flags & _DRM_DMA_WHILE_LOCKED;
		address		  = (unsigned long)buf->address;
		length		  = buf->used;
		if (!length) {
			DRM_ERROR("0 length buffer\n");
		}
		if (buf->pending) {
			DRM_ERROR("Sending pending buffer:"
				  " buffer %d, offset %d\n",
				  d->send_indices[i], i);
			retcode = -EINVAL;
			goto cleanup;
		}
		if (buf->waiting) {
			DRM_ERROR("Sending waiting buffer:"
				  " buffer %d, offset %d\n",
				  d->send_indices[i], i);
			retcode = -EINVAL;
			goto cleanup;
		}
		buf->pending = 1;

		if (dev->last_context != buf->context
		    && !(dev->queuelist[buf->context]->flags
			 & _DRM_CONTEXT_PRESERVED)) {
			add_wait_queue(&dev->context_wait, &entry);
			current->state = TASK_INTERRUPTIBLE;
				/* PRE: dev->last_context != buf->context */
			DRM(context_switch)(dev, dev->last_context,
					    buf->context);
				/* POST: we will wait for the context
				   switch and will dispatch on a later call
				   when dev->last_context == buf->context.
				   NOTE WE HOLD THE LOCK THROUGHOUT THIS
				   TIME! */
			schedule();
			current->state = TASK_RUNNING;
			remove_wait_queue(&dev->context_wait, &entry);
			if (signal_pending(current)) {
				retcode = -EINTR;
				goto cleanup;
			}
			if (dev->last_context != buf->context) {
				DRM_ERROR("Context mismatch: %d %d\n",
					  dev->last_context,
					  buf->context);
			}
		}

#if DRM_DMA_HISTOGRAM
		buf->time_queued     = get_cycles();
		buf->time_dispatched = buf->time_queued;
#endif
		gamma_dma_dispatch(dev, address, length);
		atomic_inc(&dev->counts[9]); /* _DRM_STAT_SPECIAL */
		atomic_add(length, &dev->counts[8]); /* _DRM_STAT_PRIMARY */

		if (last_buf) {
			gamma_free_buffer(dev, last_buf);
		}
		last_buf = buf;
	}


cleanup:
	if (last_buf) {
		gamma_dma_ready(dev);
		gamma_free_buffer(dev, last_buf);
	}

	if (must_free && !dev->context_flag) {
		if (gamma_lock_free(dev, &dev->lock.hw_lock->lock,
				  DRM_KERNEL_CONTEXT)) {
			DRM_ERROR("\n");
		}
	}
	clear_bit(0, &dev->interrupt_flag);
	return retcode;
}

static int gamma_dma_send_buffers(drm_device_t *dev, drm_dma_t *d)
{
	DECLARE_WAITQUEUE(entry, current);
	drm_buf_t	  *last_buf = NULL;
	int		  retcode   = 0;
	drm_device_dma_t  *dma	    = dev->dma;

	if (d->flags & _DRM_DMA_BLOCK) {
		last_buf = dma->buflist[d->send_indices[d->send_count-1]];
		add_wait_queue(&last_buf->dma_wait, &entry);
	}

	if ((retcode = gamma_dma_enqueue(dev, d))) {
		if (d->flags & _DRM_DMA_BLOCK)
			remove_wait_queue(&last_buf->dma_wait, &entry);
		return retcode;
	}

	gamma_dma_schedule(dev, 0);

	if (d->flags & _DRM_DMA_BLOCK) {
		DRM_DEBUG("%d waiting\n", current->pid);
		for (;;) {
			current->state = TASK_INTERRUPTIBLE;
			if (!last_buf->waiting && !last_buf->pending)
				break; /* finished */
			schedule();
			if (signal_pending(current)) {
				retcode = -EINTR; /* Can't restart */
				break;
			}
		}
		current->state = TASK_RUNNING;
		DRM_DEBUG("%d running\n", current->pid);
		remove_wait_queue(&last_buf->dma_wait, &entry);
		if (!retcode
		    || (last_buf->list==DRM_LIST_PEND && !last_buf->pending)) {
			if (!waitqueue_active(&last_buf->dma_wait)) {
				gamma_free_buffer(dev, last_buf);
			}
		}
		if (retcode) {
			DRM_ERROR("ctx%d w%d p%d c%ld i%d l%d %d/%d\n",
				  d->context,
				  last_buf->waiting,
				  last_buf->pending,
				  (long)DRM_WAITCOUNT(dev, d->context),
				  last_buf->idx,
				  last_buf->list,
				  last_buf->pid,
				  current->pid);
		}
	}
	return retcode;
}

int gamma_dma(struct inode *inode, struct file *filp, unsigned int cmd,
	      unsigned long arg)
{
	drm_file_t	  *priv	    = filp->private_data;
	drm_device_t	  *dev	    = priv->dev;
	drm_device_dma_t  *dma	    = dev->dma;
	int		  retcode   = 0;
	drm_dma_t	  d;

	if (copy_from_user(&d, (drm_dma_t *)arg, sizeof(d)))
		return -EFAULT;

	if (d.send_count < 0 || d.send_count > dma->buf_count) {
		DRM_ERROR("Process %d trying to send %d buffers (of %d max)\n",
			  current->pid, d.send_count, dma->buf_count);
		return -EINVAL;
	}

	if (d.request_count < 0 || d.request_count > dma->buf_count) {
		DRM_ERROR("Process %d trying to get %d buffers (of %d max)\n",
			  current->pid, d.request_count, dma->buf_count);
		return -EINVAL;
	}

	if (d.send_count) {
		if (d.flags & _DRM_DMA_PRIORITY)
			retcode = gamma_dma_priority(dev, &d);
		else
			retcode = gamma_dma_send_buffers(dev, &d);
	}

	d.granted_count = 0;

	if (!retcode && d.request_count) {
		retcode = gamma_dma_get_buffers(dev, &d);
	}

	DRM_DEBUG("%d returning, granted = %d\n",
		  current->pid, d.granted_count);
	if (copy_to_user((drm_dma_t *)arg, &d, sizeof(d)))
		return -EFAULT;

	return retcode;
}

/* =============================================================
 * DMA initialization, cleanup
 */

static int gamma_do_init_dma( drm_device_t *dev, drm_gamma_init_t *init )
{
	drm_gamma_private_t *dev_priv;
	drm_device_dma_t    *dma = dev->dma;
	drm_buf_t	    *buf;
	int i;
	struct list_head    *list;
	unsigned long	    *pgt;

	DRM_DEBUG( "%s\n", __FUNCTION__ );

	dev_priv = DRM(alloc)( sizeof(drm_gamma_private_t),
							DRM_MEM_DRIVER );
	if ( !dev_priv )
		return -ENOMEM;

	dev->dev_private = (void *)dev_priv;

	memset( dev_priv, 0, sizeof(drm_gamma_private_t) );

	list_for_each(list, &dev->maplist->head) {
		#warning list_entry() is needed here
		drm_map_list_t *r_list = (drm_map_list_t *)list;
		if( r_list->map &&
		    r_list->map->type == _DRM_SHM &&
		    r_list->map->flags & _DRM_CONTAINS_LOCK ) {
			dev_priv->sarea = r_list->map;
 			break;
 		}
 	}

	DRM_FIND_MAP( dev_priv->mmio0, init->mmio0 );
	DRM_FIND_MAP( dev_priv->mmio1, init->mmio1 );
	DRM_FIND_MAP( dev_priv->mmio2, init->mmio2 );
	DRM_FIND_MAP( dev_priv->mmio3, init->mmio3 );

	dev_priv->sarea_priv = (drm_gamma_sarea_t *)
		((u8 *)dev_priv->sarea->handle +
		 init->sarea_priv_offset);

	if (init->pcimode) {
		buf = dma->buflist[GLINT_DRI_BUF_COUNT];
		pgt = buf->address;

 		for (i = 0; i < GLINT_DRI_BUF_COUNT; i++) {
			buf = dma->buflist[i];
			*pgt = virt_to_phys((void*)buf->address) | 0x07;
			pgt++;
		}

		buf = dma->buflist[GLINT_DRI_BUF_COUNT];
	} else {
		DRM_FIND_MAP( dev_priv->buffers, init->buffers_offset );

		DRM_IOREMAP( dev_priv->buffers, dev );

		buf = dma->buflist[GLINT_DRI_BUF_COUNT];
		pgt = buf->address;

 		for (i = 0; i < GLINT_DRI_BUF_COUNT; i++) {
			buf = dma->buflist[i];
			*pgt = (unsigned long)buf->address + 0x07;
			pgt++;
		}

		buf = dma->buflist[GLINT_DRI_BUF_COUNT];

		while (GAMMA_READ(GAMMA_INFIFOSPACE) < 1) cpu_relax();
		GAMMA_WRITE( GAMMA_GDMACONTROL, 0xe) ;
	}
	while (GAMMA_READ(GAMMA_INFIFOSPACE) < 2); cpu_relax();
	GAMMA_WRITE( GAMMA_PAGETABLEADDR, virt_to_phys((void*)buf->address) );
	GAMMA_WRITE( GAMMA_PAGETABLELENGTH, 2 );

	return 0;
}

int gamma_do_cleanup_dma( drm_device_t *dev )
{
	DRM_DEBUG( "%s\n", __FUNCTION__ );

	if ( dev->dev_private ) {
		drm_gamma_private_t *dev_priv = dev->dev_private;

		DRM_IOREMAPFREE( dev_priv->buffers, dev );

		DRM(free)( dev->dev_private, sizeof(drm_gamma_private_t),
			   DRM_MEM_DRIVER );
		dev->dev_private = NULL;
	}

	return 0;
}

int gamma_dma_init( struct inode *inode, struct file *filp,
		  unsigned int cmd, unsigned long arg )
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_gamma_init_t init;

	if ( copy_from_user( &init, (drm_gamma_init_t *)arg, sizeof(init) ) )
		return -EFAULT;

	switch ( init.func ) {
	case GAMMA_INIT_DMA:
		return gamma_do_init_dma( dev, &init );
	case GAMMA_CLEANUP_DMA:
		return gamma_do_cleanup_dma( dev );
	}

	return -EINVAL;
}

static int gamma_do_copy_dma( drm_device_t *dev, drm_gamma_copy_t *copy )
{
	drm_device_dma_t    *dma = dev->dma;
	unsigned int        *screenbuf;

	DRM_DEBUG( "%s\n", __FUNCTION__ );

	/* We've DRM_RESTRICTED this DMA buffer */

	screenbuf = dma->buflist[ GLINT_DRI_BUF_COUNT + 1 ]->address;

#if 0
	*buffer++ = 0x180;	/* Tag (FilterMode) */
	*buffer++ = 0x200;	/* Allow FBColor through */
	*buffer++ = 0x53B;	/* Tag */
	*buffer++ = copy->Pitch;
	*buffer++ = 0x53A;	/* Tag */
	*buffer++ = copy->SrcAddress;
	*buffer++ = 0x539;	/* Tag */
	*buffer++ = copy->WidthHeight; /* Initiates transfer */
	*buffer++ = 0x53C;	/* Tag - DMAOutputAddress */
	*buffer++ = virt_to_phys((void*)screenbuf);
	*buffer++ = 0x53D;	/* Tag - DMAOutputCount */
	*buffer++ = copy->Count; /* Reads HostOutFifo BLOCKS until ..*/

	/* Data now sitting in dma->buflist[ GLINT_DRI_BUF_COUNT + 1 ] */
	/* Now put it back to the screen */

	*buffer++ = 0x180;	/* Tag (FilterMode) */
	*buffer++ = 0x400;	/* Allow Sync through */
	*buffer++ = 0x538;	/* Tag - DMARectangleReadTarget */
	*buffer++ = 0x155;	/* FBSourceData | count */
	*buffer++ = 0x537;	/* Tag */
	*buffer++ = copy->Pitch;
	*buffer++ = 0x536;	/* Tag */
	*buffer++ = copy->DstAddress;
	*buffer++ = 0x535;	/* Tag */
	*buffer++ = copy->WidthHeight; /* Initiates transfer */
	*buffer++ = 0x530;	/* Tag - DMAAddr */
	*buffer++ = virt_to_phys((void*)screenbuf);
	*buffer++ = 0x531;
	*buffer++ = copy->Count; /* initiates DMA transfer of color data */
#endif

	/* need to dispatch it now */

	return 0;
}

int gamma_dma_copy( struct inode *inode, struct file *filp,
		  unsigned int cmd, unsigned long arg )
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_gamma_copy_t copy;

	if ( copy_from_user( &copy, (drm_gamma_copy_t *)arg, sizeof(copy) ) )
		return -EFAULT;

	return gamma_do_copy_dma( dev, &copy );
}

/* =============================================================
 * Per Context SAREA Support
 */

int gamma_getsareactx(struct inode *inode, struct file *filp,
		     unsigned int cmd, unsigned long arg)
{
	drm_file_t	*priv	= filp->private_data;
	drm_device_t	*dev	= priv->dev;
	drm_ctx_priv_map_t request;
	drm_map_t *map;

	if (copy_from_user(&request,
			   (drm_ctx_priv_map_t *)arg,
			   sizeof(request)))
		return -EFAULT;

	down(&dev->struct_sem);
	if ((int)request.ctx_id >= dev->max_context) {
		up(&dev->struct_sem);
		return -EINVAL;
	}

	map = dev->context_sareas[request.ctx_id];
	up(&dev->struct_sem);

	request.handle = map->handle;
	if (copy_to_user((drm_ctx_priv_map_t *)arg, &request, sizeof(request)))
		return -EFAULT;
	return 0;
}

int gamma_setsareactx(struct inode *inode, struct file *filp,
		     unsigned int cmd, unsigned long arg)
{
	drm_file_t	*priv	= filp->private_data;
	drm_device_t	*dev	= priv->dev;
	drm_ctx_priv_map_t request;
	drm_map_t *map = NULL;
	drm_map_list_t *r_list;
	struct list_head *list;

	if (copy_from_user(&request,
			   (drm_ctx_priv_map_t *)arg,
			   sizeof(request)))
		return -EFAULT;

	down(&dev->struct_sem);
	r_list = NULL;
	list_for_each(list, &dev->maplist->head) {
		r_list = (drm_map_list_t *)list;
		if(r_list->map &&
		   r_list->map->handle == request.handle) break;
	}
	if (list == &(dev->maplist->head)) {
		up(&dev->struct_sem);
		return -EINVAL;
	}
	map = r_list->map;
	up(&dev->struct_sem);

	if (!map) return -EINVAL;

	down(&dev->struct_sem);
	if ((int)request.ctx_id >= dev->max_context) {
		up(&dev->struct_sem);
		return -EINVAL;
	}
	dev->context_sareas[request.ctx_id] = map;
	up(&dev->struct_sem);
	return 0;
}

/* drm_dma.h hooks
*/
void DRM(driver_irq_preinstall)( drm_device_t *dev ) {
}

void DRM(driver_irq_postinstall)( drm_device_t *dev ) {
}

void DRM(driver_irq_uninstall)( drm_device_t *dev ) {
}
