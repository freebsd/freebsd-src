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
#if __HAVE_PCI_DMA
		if (dma->bufs[i].seg_count) {
			DRM_DEBUG("order %d: buf_count = %d,"
				  " seg_count = %d\n",
				  i,
				  dma->bufs[i].buf_count,
				  dma->bufs[i].seg_count);
			for (j = 0; j < dma->bufs[i].seg_count; j++) {
				if (dma->bufs[i].seglist[j] != NULL)
					DRM(pci_free)(dev, dma->bufs[i].buf_size,
					    (void *)dma->bufs[i].seglist[j],
					    dma->bufs[i].seglist_bus[j]);
			}
			DRM(free)(dma->bufs[i].seglist,
				  dma->bufs[i].seg_count
				  * sizeof(*dma->bufs[0].seglist),
				  DRM_MEM_SEGS);
			DRM(free)(dma->bufs[i].seglist_bus,
				  dma->bufs[i].seg_count
				  * sizeof(*dma->bufs[0].seglist_bus),
				  DRM_MEM_SEGS);
		}
#endif /* __HAVE_PCI_DMA */

	   	if (dma->bufs[i].buf_count) {
		   	for (j = 0; j < dma->bufs[i].buf_count; j++) {
				DRM(free)(dma->bufs[i].buflist[j].dev_private,
					dma->bufs[i].buflist[j].dev_priv_size,
					DRM_MEM_BUFS);
			}
		   	DRM(free)(dma->bufs[i].buflist,
				  dma->bufs[i].buf_count *
				  sizeof(*dma->bufs[0].buflist),
				  DRM_MEM_BUFS);
		}
	}

	DRM(free)(dma->buflist, dma->buf_count * sizeof(*dma->buflist),
	    DRM_MEM_BUFS);
	DRM(free)(dma->pagelist, dma->page_count * sizeof(*dma->pagelist),
	    DRM_MEM_PAGES);
	DRM(free)(dev->dma, sizeof(*dev->dma), DRM_MEM_DRIVER);
	dev->dma = NULL;
}


void DRM(free_buffer)(drm_device_t *dev, drm_buf_t *buf)
{
	if (!buf) return;

	buf->pending  = 0;
	buf->filp     = NULL;
	buf->used     = 0;
}

#if !__HAVE_DMA_RECLAIM
void DRM(reclaim_buffers)(drm_device_t *dev, DRMFILE filp)
{
	drm_device_dma_t *dma = dev->dma;
	int		 i;

	if (!dma) return;
	for (i = 0; i < dma->buf_count; i++) {
		if (dma->buflist[i]->filp == filp) {
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


#if __HAVE_DMA_IRQ

int DRM(irq_install)( drm_device_t *dev, int irq )
{
	int retcode;

	if ( !irq )
		return DRM_ERR(EINVAL);

	if (dev->dev_private == NULL)
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

	dev->dma->next_buffer = NULL;
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
				      (irqreturn_t (*)(DRM_IRQ_ARGS))DRM(dma_service), dev);
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

