/* drm_dma.c -- DMA IOCTL and function support
 * Created: Fri Oct 18 2003 by anholt@FreeBSD.org
 *
 * Copyright 2003 Eric Anholt
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
 *    Eric Anholt <anholt@FreeBSD.org>
 *
 * $FreeBSD$
 */

#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
static irqreturn_t
DRM(irq_handler_wrap)(DRM_IRQ_ARGS)
{
	drm_device_t *dev = (drm_device_t *)arg;

	DRM_SPINLOCK(&dev->irq_lock);
	DRM(irq_handler)(arg);
	DRM_SPINUNLOCK(&dev->irq_lock);
}
#endif

int DRM(irq_install)( drm_device_t *dev, int irq )
{
	int retcode;

	if ( irq == 0 || dev->dev_private == NULL)
		return DRM_ERR(EINVAL);

	DRM_LOCK();
	if ( dev->irq ) {
		DRM_UNLOCK();
		return DRM_ERR(EBUSY);
	}
	dev->irq = irq;
	DRM_UNLOCK();

	DRM_DEBUG( "%s: irq=%d\n", __FUNCTION__, irq );

	dev->context_flag = 0;

	dev->dma->next_buffer = NULL;
	dev->dma->this_buffer = NULL;

#if __HAVE_IRQ_BH
	TASK_INIT(&dev->task, 0, DRM(dma_immediate_bh), dev);
#endif

	DRM_SPININIT(dev->irq_lock, "DRM IRQ lock");

#if __HAVE_VBL_IRQ && 0 /* disabled */
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
		DRM_LOCK();
		DRM_SPINUNINIT(dev->irq_lock);
		dev->irq = 0;
		dev->irqrid = 0;
		DRM_UNLOCK();
		return ENOENT;
	}
	
#ifdef __FreeBSD__
#if __FreeBSD_version < 500000
	retcode = bus_setup_intr(dev->device, dev->irqr, INTR_TYPE_TTY,
				 DRM(irq_handler), dev, &dev->irqh);
#else
	retcode = bus_setup_intr(dev->device, dev->irqr, INTR_TYPE_TTY | INTR_MPSAFE,
				 DRM(irq_handler_wrap), dev, &dev->irqh);
#endif
	if ( retcode ) {
#elif defined(__NetBSD__)
	dev->irqh = pci_intr_establish(&dev->pa.pa_pc, dev->ih, IPL_TTY,
	    (irqreturn_t (*)(DRM_IRQ_ARGS))DRM(irq_handler), dev);
	if ( !dev->irqh ) {
#endif
		DRM_LOCK();
#ifdef __FreeBSD__
		bus_release_resource(dev->device, SYS_RES_IRQ, dev->irqrid, dev->irqr);
#endif
		DRM_SPINUNINIT(dev->irq_lock);
		dev->irq = 0;
		dev->irqrid = 0;
		DRM_UNLOCK();
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
	
	DRM_LOCK();
	irq = dev->irq;
	irqrid = dev->irqrid;
	dev->irq = 0;
	dev->irqrid = 0;
	DRM_UNLOCK();

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
	DRM_SPINUNINIT(dev->irq_lock);

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
		
		DRM_SPINLOCK(&dev->irq_lock);
		TAILQ_INSERT_HEAD(&dev->vbl_sig_list, vbl_sig, link);
		DRM_SPINUNLOCK(&dev->irq_lock);
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
}
#endif

#endif /*  __HAVE_VBL_IRQ */
