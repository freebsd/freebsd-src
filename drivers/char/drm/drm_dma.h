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
 */

#include "drmP.h"
#include "drm_os_linux.h"
#include <linux/interrupt.h>	/* For task queue support */

#ifndef __HAVE_DMA_WAITQUEUE
#define __HAVE_DMA_WAITQUEUE	0
#endif
#ifndef __HAVE_DMA_RECLAIM
#define __HAVE_DMA_RECLAIM	0
#endif
#ifndef __HAVE_SHARED_IRQ
#define __HAVE_SHARED_IRQ	0
#endif

#if __HAVE_SHARED_IRQ
#define DRM_IRQ_TYPE		SA_SHIRQ
#else
#define DRM_IRQ_TYPE		0
#endif

#if __HAVE_DMA

int DRM(dma_setup)( drm_device_t *dev )
{
	int i;

	dev->dma = DRM(alloc)( sizeof(*dev->dma), DRM_MEM_DRIVER );
	if ( !dev->dma )
		return -ENOMEM;

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
				DRM(free_pages)(dma->bufs[i].seglist[j],
						dma->bufs[i].page_order,
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

	if ( __HAVE_DMA_WAITQUEUE && waitqueue_active(&buf->dma_wait)) {
		wake_up_interruptible(&buf->dma_wait);
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
		wake_up_interruptible(&dma->next_queue->flush_queue);
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


int DRM(dma_enqueue)(drm_device_t *dev, drm_dma_t *d)
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
	int ret;

	if ( !irq )
		return -EINVAL;

	down( &dev->struct_sem );
	if ( dev->irq ) {
		up( &dev->struct_sem );
		return -EBUSY;
	}
	dev->irq = irq;
	up( &dev->struct_sem );

	DRM_DEBUG( "%s: irq=%d\n", __FUNCTION__, irq );

	dev->context_flag = 0;
	dev->interrupt_flag = 0;
	dev->dma_flag = 0;

	dev->dma->next_buffer = NULL;
	dev->dma->next_queue = NULL;
	dev->dma->this_buffer = NULL;

#if __HAVE_DMA_IRQ_BH
	INIT_LIST_HEAD( &dev->tq.list );
	dev->tq.sync = 0;
	dev->tq.routine = DRM(dma_immediate_bh);
	dev->tq.data = dev;
#endif

#if __HAVE_VBL_IRQ
	init_waitqueue_head(&dev->vbl_queue);

	spin_lock_init( &dev->vbl_lock );

	INIT_LIST_HEAD( &dev->vbl_sigs.head );

	dev->vbl_pending = 0;
#endif

				/* Before installing handler */
	DRM(driver_irq_preinstall)(dev);

				/* Install handler */
	ret = request_irq( dev->irq, DRM(dma_service),
			   DRM_IRQ_TYPE, dev->devname, dev );
	if ( ret < 0 ) {
		down( &dev->struct_sem );
		dev->irq = 0;
		up( &dev->struct_sem );
		return ret;
	}

				/* After installing handler */
	DRM(driver_irq_postinstall)(dev);

	return 0;
}

int DRM(irq_uninstall)( drm_device_t *dev )
{
	int irq;

	down( &dev->struct_sem );
	irq = dev->irq;
	dev->irq = 0;
	up( &dev->struct_sem );

	if ( !irq )
		return -EINVAL;

	DRM_DEBUG( "%s: irq=%d\n", __FUNCTION__, irq );

	DRM(driver_irq_uninstall)( dev );

	free_irq( irq, dev );

	return 0;
}

int DRM(control)( struct inode *inode, struct file *filp,
		  unsigned int cmd, unsigned long arg )
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_control_t ctl;

	if ( copy_from_user( &ctl, (drm_control_t *)arg, sizeof(ctl) ) )
		return -EFAULT;

	switch ( ctl.func ) {
	case DRM_INST_HANDLER:
		return DRM(irq_install)( dev, ctl.irq );
	case DRM_UNINST_HANDLER:
		return DRM(irq_uninstall)( dev );
	default:
		return -EINVAL;
	}
}

#if __HAVE_VBL_IRQ

int DRM(wait_vblank)(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long data )
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_wait_vblank_t vblwait;
	struct timeval now;
	int ret = 0;
	unsigned int flags;

	if (!dev->irq)
		return -EINVAL;

	DRM_COPY_FROM_USER_IOCTL( vblwait, (drm_wait_vblank_t *)data,
				  sizeof(vblwait) );

	switch ( vblwait.request.type & ~_DRM_VBLANK_FLAGS_MASK ) {
	case _DRM_VBLANK_RELATIVE:
		vblwait.request.sequence += atomic_read( &dev->vbl_received );
		vblwait.request.type &= ~_DRM_VBLANK_RELATIVE;
	case _DRM_VBLANK_ABSOLUTE:
		break;
	default:
		return -EINVAL;
	}

	flags = vblwait.request.type & _DRM_VBLANK_FLAGS_MASK;
	
	if ( flags & _DRM_VBLANK_SIGNAL ) {
		unsigned long irqflags;
		drm_vbl_sig_t *vbl_sig;
		
		vblwait.reply.sequence = atomic_read( &dev->vbl_received );

		spin_lock_irqsave( &dev->vbl_lock, irqflags );

		/* Check if this task has already scheduled the same signal
		 * for the same vblank sequence number; nothing to be done in
		 * that case
		 */
		list_for_each( ( (struct list_head *) vbl_sig ), &dev->vbl_sigs.head ) {
			if (vbl_sig->sequence == vblwait.request.sequence
			    && vbl_sig->info.si_signo == vblwait.request.signal
			    && vbl_sig->task == current)
			{
				spin_unlock_irqrestore( &dev->vbl_lock, irqflags );
				goto done;
			}
		}

		if ( dev->vbl_pending >= 100 ) {
			spin_unlock_irqrestore( &dev->vbl_lock, irqflags );
			return -EBUSY;
		}

		dev->vbl_pending++;

		spin_unlock_irqrestore( &dev->vbl_lock, irqflags );

		if ( !( vbl_sig = kmalloc(sizeof(drm_vbl_sig_t), GFP_KERNEL) ) ) 
			return -ENOMEM;
		

		memset( (void *)vbl_sig, 0, sizeof(*vbl_sig) );

		vbl_sig->sequence = vblwait.request.sequence;
		vbl_sig->info.si_signo = vblwait.request.signal;
		vbl_sig->task = current;

		spin_lock_irqsave( &dev->vbl_lock, irqflags );

		list_add_tail( (struct list_head *) vbl_sig, &dev->vbl_sigs.head );

		spin_unlock_irqrestore( &dev->vbl_lock, irqflags );
	} else {
		ret = DRM(vblank_wait)( dev, &vblwait.request.sequence );

		do_gettimeofday( &now );
		vblwait.reply.tval_sec = now.tv_sec;
		vblwait.reply.tval_usec = now.tv_usec;
	}

done:
	DRM_COPY_TO_USER_IOCTL( (drm_wait_vblank_t *)data, vblwait,
				sizeof(vblwait) );

	return ret;
}

void DRM(vbl_send_signals)( drm_device_t *dev )
{
	struct list_head *tmp;
	drm_vbl_sig_t *vbl_sig;
	unsigned int vbl_seq = atomic_read( &dev->vbl_received );
	unsigned long flags;

	spin_lock_irqsave( &dev->vbl_lock, flags );

	list_for_each_safe( ( (struct list_head *) vbl_sig ), tmp, &dev->vbl_sigs.head ) {
		if ( ( vbl_seq - vbl_sig->sequence ) <= (1<<23) ) {
			vbl_sig->info.si_code = vbl_seq;
			send_sig_info( vbl_sig->info.si_signo, &vbl_sig->info, vbl_sig->task );

			list_del( (struct list_head *) vbl_sig );


			kfree( vbl_sig );
			dev->vbl_pending--;
		}
	}

	spin_unlock_irqrestore( &dev->vbl_lock, flags );
}

#endif	/* __HAVE_VBL_IRQ */

#else

int DRM(control)( struct inode *inode, struct file *filp,
		  unsigned int cmd, unsigned long arg )
{
	drm_control_t ctl;

	if ( copy_from_user( &ctl, (drm_control_t *)arg, sizeof(ctl) ) )
		return -EFAULT;

	switch ( ctl.func ) {
	case DRM_INST_HANDLER:
	case DRM_UNINST_HANDLER:
		return 0;
	default:
		return -EINVAL;
	}
}

#endif /* __HAVE_DMA_IRQ */

#endif /* __HAVE_DMA */
