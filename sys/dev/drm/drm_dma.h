/* drm_dma.c -- DMA IOCTL and function support -*- linux-c -*-
 * Created: Fri Mar 19 14:30:16 1999 by faith@valinux.com
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

#include "dev/drm/drmP.h"

#ifndef __HAVE_DMA_WAITQUEUE
#define __HAVE_DMA_WAITQUEUE	0
#endif
#ifndef __HAVE_DMA_RECLAIM
#define __HAVE_DMA_RECLAIM	0
#endif
#ifndef __HAVE_SHARED_IRQ
#define __HAVE_SHARED_IRQ	0
#endif

#if __HAVE_DMA

int DRM(dma_setup)( drm_device_t *dev )
{
	int i;

	dev->dma = DRM(alloc)( sizeof(*dev->dma), DRM_MEM_DRIVER );
	if ( !dev->dma )
		return DRM_ERR(ENOMEM);

	memset( dev->dma, 0, sizeof(*dev->dma) );

	for ( i = 0 ; i <= DRM_MAX_ORDER ; i++ )
		memset(&dev->dma->bufs[i], 0, sizeof(dev->dma->bufs[0]));

	return 0;
}

void DRM(dma_takedown)(drm_device_t *dev)
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
				DRM(free)((void *)dma->bufs[i].seglist[j],
						dma->bufs[i].buf_size,
						DRM_MEM_DMA);
			}
			DRM(free)(dma->bufs[i].seglist,
				  dma->bufs[i].seg_count
				  * sizeof(*dma->bufs[0].seglist),
				  DRM_MEM_SEGS);
		}
	   	if(dma->bufs[i].buf_count) {
		   	for(j = 0; j < dma->bufs[i].buf_count; j++) {
			   if(dma->bufs[i].buflist[j].dev_private) {
			      DRM(free)(dma->bufs[i].buflist[j].dev_private,
					dma->bufs[i].buflist[j].dev_priv_size,
					DRM_MEM_BUFS);
			   }
			}
		   	DRM(free)(dma->bufs[i].buflist,
				  dma->bufs[i].buf_count *
				  sizeof(*dma->bufs[0].buflist),
				  DRM_MEM_BUFS);
#if __HAVE_DMA_FREELIST
		   	DRM(freelist_destroy)(&dma->bufs[i].freelist);
#endif
		}
	}

	if (dma->buflist) {
		DRM(free)(dma->buflist,
			  dma->buf_count * sizeof(*dma->buflist),
			  DRM_MEM_BUFS);
	}

	if (dma->pagelist) {
		DRM(free)(dma->pagelist,
			  dma->page_count * sizeof(*dma->pagelist),
			  DRM_MEM_PAGES);
	}
	DRM(free)(dev->dma, sizeof(*dev->dma), DRM_MEM_DRIVER);
	dev->dma = NULL;
}


#if __HAVE_DMA_HISTOGRAM
/* This is slow, but is useful for debugging. */
int DRM(histogram_slot)(unsigned long count)
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

void DRM(histogram_compute)(drm_device_t *dev, drm_buf_t *buf)
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

		q2d = DRM(histogram_slot)(queued_to_dispatched);
		d2c = DRM(histogram_slot)(dispatched_to_completed);
		c2f = DRM(histogram_slot)(completed_to_freed);

		q2c = DRM(histogram_slot)(queued_to_dispatched
					  + dispatched_to_completed);
		q2f = DRM(histogram_slot)(queued_to_dispatched
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

void DRM(free_buffer)(drm_device_t *dev, drm_buf_t *buf)
{
	if (!buf) return;

	buf->waiting  = 0;
	buf->pending  = 0;
	buf->pid      = 0;
	buf->used     = 0;
#if __HAVE_DMA_HISTOGRAM
	buf->time_completed = get_cycles();
#endif

	if ( buf->dma_wait ) {
		wakeup( (void *)&buf->dma_wait );
		buf->dma_wait = 0;
	}
#if __HAVE_DMA_FREELIST
	else {
		drm_device_dma_t *dma = dev->dma;
				/* If processes are waiting, the last one
				   to wake will put the buffer on the free
				   list.  If no processes are waiting, we
				   put the buffer on the freelist here. */
		DRM(freelist_put)(dev, &dma->bufs[buf->order].freelist, buf);
	}
#endif
}

#if !__HAVE_DMA_RECLAIM
void DRM(reclaim_buffers)(drm_device_t *dev, pid_t pid)
{
	drm_device_dma_t *dma = dev->dma;
	int		 i;

	if (!dma) return;
	for (i = 0; i < dma->buf_count; i++) {
		if (dma->buflist[i]->pid == pid) {
			switch (dma->buflist[i]->list) {
			case DRM_LIST_NONE:
				DRM(free_buffer)(dev, dma->buflist[i]);
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
#endif


/* GH: This is a big hack for now...
 */
#if __HAVE_OLD_DMA

void DRM(clear_next_buffer)(drm_device_t *dev)
{
	drm_device_dma_t *dma = dev->dma;

	dma->next_buffer = NULL;
	if (dma->next_queue && !DRM_BUFCOUNT(&dma->next_queue->waitlist)) {
		DRM_WAKEUP_INT(&dma->next_queue->flush_queue);
	}
	dma->next_queue	 = NULL;
}

int DRM(select_queue)(drm_device_t *dev, void (*wrapper)(unsigned long))
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
		int s = splclock();
		if (dev->timer.c_time != dev->last_switch + DRM_TIME_SLICE) {
			callout_reset(&dev->timer,
				      dev->last_switch + DRM_TIME_SLICE - j,
				      (void (*)(void *))wrapper,
				      dev);
		}
		splx(s);
		return -1;
	}

	return candidate;
}


int DRM(dma_enqueue)(drm_device_t *dev, drm_dma_t *d)
{
	int		  i;
	drm_queue_t	  *q;
	drm_buf_t	  *buf;
	int		  idx;
	int		  while_locked = 0;
	drm_device_dma_t  *dma = dev->dma;
	int		  error;

	DRM_DEBUG("%d\n", d->send_count);

	if (d->flags & _DRM_DMA_WHILE_LOCKED) {
		int context = dev->lock.hw_lock->lock;

		if (!_DRM_LOCK_IS_HELD(context)) {
			DRM_ERROR("No lock held during \"while locked\""
				  " request\n");
			return DRM_ERR(EINVAL);
		}
		if (d->context != _DRM_LOCKING_CONTEXT(context)
		    && _DRM_LOCKING_CONTEXT(context) != DRM_KERNEL_CONTEXT) {
			DRM_ERROR("Lock held by %d while %d makes"
				  " \"while locked\" request\n",
				  _DRM_LOCKING_CONTEXT(context),
				  d->context);
			return DRM_ERR(EINVAL);
		}
		q = dev->queuelist[DRM_KERNEL_CONTEXT];
		while_locked = 1;
	} else {
		q = dev->queuelist[d->context];
	}


	atomic_inc(&q->use_count);
	if (atomic_read(&q->block_write)) {
		atomic_inc(&q->block_count);
		for (;;) {
			if (!atomic_read(&q->block_write)) break;
			error = tsleep(&q->block_write, PZERO|PCATCH,
				       "dmawr", 0);
			if (error) {
				atomic_dec(&q->use_count);
				return error;
			}
		}
		atomic_dec(&q->block_count);
	}

	for (i = 0; i < d->send_count; i++) {
		idx = d->send_indices[i];
		if (idx < 0 || idx >= dma->buf_count) {
			atomic_dec(&q->use_count);
			DRM_ERROR("Index %d (of %d max)\n",
				  d->send_indices[i], dma->buf_count - 1);
			return DRM_ERR(EINVAL);
		}
		buf = dma->buflist[ idx ];
		if (buf->pid != DRM_CURRENTPID) {
			atomic_dec(&q->use_count);
			DRM_ERROR("Process %d using buffer owned by %d\n",
				  DRM_CURRENTPID, buf->pid);
			return DRM_ERR(EINVAL);
		}
		if (buf->list != DRM_LIST_NONE) {
			atomic_dec(&q->use_count);
			DRM_ERROR("Process %d using buffer %d on list %d\n",
				  DRM_CURRENTPID, buf->idx, buf->list);
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
			return DRM_ERR(EINVAL);
		}
		if (buf->waiting) {
			atomic_dec(&q->use_count);
			DRM_ERROR("Queueing waiting buffer:"
				  " buffer %d, offset %d\n",
				  d->send_indices[i], i);
			return DRM_ERR(EINVAL);
		}
		buf->waiting = 1;
		if (atomic_read(&q->use_count) == 1
		    || atomic_read(&q->finalization)) {
			DRM(free_buffer)(dev, buf);
		} else {
			DRM(waitlist_put)(&q->waitlist, buf);
			atomic_inc(&q->total_queued);
		}
	}
	atomic_dec(&q->use_count);

	return 0;
}

static int DRM(dma_get_buffers_of_order)(drm_device_t *dev, drm_dma_t *d,
					 int order)
{
	int		  i;
	drm_buf_t	  *buf;
	drm_device_dma_t  *dma = dev->dma;

	for (i = d->granted_count; i < d->request_count; i++) {
		buf = DRM(freelist_get)(&dma->bufs[order].freelist,
					d->flags & _DRM_DMA_WAIT);
		if (!buf) break;
		if (buf->pending || buf->waiting) {
			DRM_ERROR("Free buffer %d in use by %d (w%d, p%d)\n",
				  buf->idx,
				  buf->pid,
				  buf->waiting,
				  buf->pending);
		}
		buf->pid     = DRM_CURRENTPID;
		if (DRM_COPY_TO_USER(&d->request_indices[i],
				 &buf->idx,
				 sizeof(buf->idx)))
			return DRM_ERR(EFAULT);

		if (DRM_COPY_TO_USER(&d->request_sizes[i],
				 &buf->total,
				 sizeof(buf->total)))
			return DRM_ERR(EFAULT);

		++d->granted_count;
	}
	return 0;
}


int DRM(dma_get_buffers)(drm_device_t *dev, drm_dma_t *dma)
{
	int		  order;
	int		  retcode = 0;
	int		  tmp_order;

	order = DRM(order)(dma->request_size);

	dma->granted_count = 0;
	retcode		   = DRM(dma_get_buffers_of_order)(dev, dma, order);

	if (dma->granted_count < dma->request_count
	    && (dma->flags & _DRM_DMA_SMALLER_OK)) {
		for (tmp_order = order - 1;
		     !retcode
			     && dma->granted_count < dma->request_count
			     && tmp_order >= DRM_MIN_ORDER;
		     --tmp_order) {

			retcode = DRM(dma_get_buffers_of_order)(dev, dma,
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

			retcode = DRM(dma_get_buffers_of_order)(dev, dma,
								tmp_order);
		}
	}
	return 0;
}

#endif /* __HAVE_OLD_DMA */


#if __HAVE_DMA_IRQ

int DRM(irq_install)( drm_device_t *dev, int irq )
{
	int retcode;

	if ( !irq )
		return DRM_ERR(EINVAL);

	DRM_LOCK;
	if ( dev->irq ) {
		DRM_UNLOCK;
		return DRM_ERR(EBUSY);
	}
	dev->irq = irq;
	DRM_UNLOCK;

	DRM_DEBUG( "%s: irq=%d\n", __FUNCTION__, irq );

	dev->context_flag = 0;
	dev->interrupt_flag = 0;
	dev->dma_flag = 0;

	dev->dma->next_buffer = NULL;
	dev->dma->next_queue = NULL;
	dev->dma->this_buffer = NULL;

#if __HAVE_DMA_IRQ_BH
	TASK_INIT(&dev->task, 0, DRM(dma_immediate_bh), dev);
#endif

#if __HAVE_VBL_IRQ && 0 /* disabled */
	DRM_SPININIT( dev->vbl_lock, "vblsig" );
	TAILQ_INIT( &dev->vbl_sig_list );
#endif

				/* Before installing handler */
	DRM(driver_irq_preinstall)( dev );

				/* Install handler */
	dev->irqrid = 0;
#ifdef __FreeBSD__
	dev->irqr = bus_alloc_resource(dev->device, SYS_RES_IRQ, &dev->irqrid,
				      0, ~0, 1, RF_SHAREABLE);
	if (!dev->irqr) {
#elif defined(__NetBSD__)
	if (pci_intr_map(&dev->pa, &dev->ih) != 0) {
#endif
		DRM_LOCK;
		dev->irq = 0;
		dev->irqrid = 0;
		DRM_UNLOCK;
		return ENOENT;
	}
	
#ifdef __FreeBSD__
#if __FreeBSD_version < 500000
	retcode = bus_setup_intr(dev->device, dev->irqr, INTR_TYPE_TTY,
				 DRM(dma_service), dev, &dev->irqh);
#else
	retcode = bus_setup_intr(dev->device, dev->irqr, INTR_TYPE_TTY | INTR_MPSAFE,
				 DRM(dma_service), dev, &dev->irqh);
#endif
	if ( retcode ) {
#elif defined(__NetBSD__)
	dev->irqh = pci_intr_establish(&dev->pa.pa_pc, dev->ih, IPL_TTY,
				      (int (*)(DRM_IRQ_ARGS))DRM(dma_service), dev);
	if ( !dev->irqh ) {
#endif
		DRM_LOCK;
#ifdef __FreeBSD__
		bus_release_resource(dev->device, SYS_RES_IRQ, dev->irqrid, dev->irqr);
#endif
		dev->irq = 0;
		dev->irqrid = 0;
		DRM_UNLOCK;
		return retcode;
	}

				/* After installing handler */
	DRM(driver_irq_postinstall)( dev );

	return 0;
}

int DRM(irq_uninstall)( drm_device_t *dev )
{
	int irq;
	int irqrid;
	
	DRM_LOCK;
	irq = dev->irq;
	irqrid = dev->irqrid;
	dev->irq = 0;
	dev->irqrid = 0;
	DRM_UNLOCK;

	if ( !irq )
		return DRM_ERR(EINVAL);

	DRM_DEBUG( "%s: irq=%d\n", __FUNCTION__, irq );

	DRM(driver_irq_uninstall)( dev );

#ifdef __FreeBSD__
	bus_teardown_intr(dev->device, dev->irqr, dev->irqh);
	bus_release_resource(dev->device, SYS_RES_IRQ, irqrid, dev->irqr);
#elif defined(__NetBSD__)
	pci_intr_disestablish(&dev->pa.pa_pc, dev->irqh);
#endif

	return 0;
}

int DRM(control)( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_control_t ctl;

	DRM_COPY_FROM_USER_IOCTL( ctl, (drm_control_t *) data, sizeof(ctl) );

	switch ( ctl.func ) {
	case DRM_INST_HANDLER:
		return DRM(irq_install)( dev, ctl.irq );
	case DRM_UNINST_HANDLER:
		return DRM(irq_uninstall)( dev );
	default:
		return DRM_ERR(EINVAL);
	}
}

#if __HAVE_VBL_IRQ
int DRM(wait_vblank)( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_wait_vblank_t vblwait;
	struct timeval now;
	int ret;

	if (!dev->irq)
		return DRM_ERR(EINVAL);

	DRM_COPY_FROM_USER_IOCTL( vblwait, (drm_wait_vblank_t *)data,
				  sizeof(vblwait) );

	if (vblwait.request.type & _DRM_VBLANK_RELATIVE) {
		vblwait.request.sequence += atomic_read(&dev->vbl_received);
		vblwait.request.type &= ~_DRM_VBLANK_RELATIVE;
	}

	flags = vblwait.request.type & _DRM_VBLANK_FLAGS_MASK;
	if (flags & _DRM_VBLANK_SIGNAL) {
#if 0 /* disabled */
		drm_vbl_sig_t *vbl_sig = DRM_MALLOC(sizeof(drm_vbl_sig_t));
		if (vbl_sig == NULL)
			return ENOMEM;
		bzero(vbl_sig, sizeof(*vbl_sig));
		
		vbl_sig->sequence = vblwait.request.sequence;
		vbl_sig->signo = vblwait.request.signal;
		vbl_sig->pid = DRM_CURRENTPID;

		vblwait.reply.sequence = atomic_read(&dev->vbl_received);
		
		DRM_SPINLOCK(&dev->vbl_lock);
		TAILQ_INSERT_HEAD(&dev->vbl_sig_list, vbl_sig, link);
		DRM_SPINUNLOCK(&dev->vbl_lock);
		ret = 0;
#endif
		ret = EINVAL;
	} else {
		ret = DRM(vblank_wait)(dev, &vblwait.request.sequence);
		
		microtime(&now);
		vblwait.reply.tval_sec = now.tv_sec;
		vblwait.reply.tval_usec = now.tv_usec;
	}

	DRM_COPY_TO_USER_IOCTL( (drm_wait_vblank_t *)data, vblwait,
				sizeof(vblwait) );

	return ret;
}

void DRM(vbl_send_signals)(drm_device_t *dev)
{
}

#if 0 /* disabled */
void DRM(vbl_send_signals)( drm_device_t *dev )
{
	drm_vbl_sig_t *vbl_sig;
	unsigned int vbl_seq = atomic_read( &dev->vbl_received );
	struct proc *p;

	DRM_SPINLOCK(&dev->vbl_lock);

	vbl_sig = TAILQ_FIRST(&dev->vbl_sig_list);
	while (vbl_sig != NULL) {
		drm_vbl_sig_t *next = TAILQ_NEXT(vbl_sig, link);

		if ( ( vbl_seq - vbl_sig->sequence ) <= (1<<23) ) {
			p = pfind(vbl_sig->pid);
			if (p != NULL)
				psignal(p, vbl_sig->signo);

			TAILQ_REMOVE(&dev->vbl_sig_list, vbl_sig, link);
			DRM_FREE(vbl_sig,sizeof(*vbl_sig));
		}
		vbl_sig = next;
	}

	DRM_SPINUNLOCK(&dev->vbl_lock);
}
#endif

#endif /*  __HAVE_VBL_IRQ */

#else

int DRM(control)( DRM_IOCTL_ARGS )
{
	drm_control_t ctl;

	DRM_COPY_FROM_USER_IOCTL( ctl, (drm_control_t *) data, sizeof(ctl) );

	switch ( ctl.func ) {
	case DRM_INST_HANDLER:
	case DRM_UNINST_HANDLER:
		return 0;
	default:
		return DRM_ERR(EINVAL);
	}
}

#endif /* __HAVE_DMA_IRQ */

#endif /* __HAVE_DMA */

