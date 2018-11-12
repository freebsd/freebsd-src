/*-
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
 * ERIC ANHOLT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <anholt@FreeBSD.org>
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/** @file drm_irq.c
 * Support code for handling setup/teardown of interrupt handlers and
 * handing interrupt handlers off to the drivers.
 */

#include "dev/drm/drmP.h"
#include "dev/drm/drm.h"

int drm_irq_by_busid(struct drm_device *dev, void *data,
		     struct drm_file *file_priv)
{
	struct drm_irq_busid *irq = data;

	if ((irq->busnum >> 8) != dev->pci_domain ||
	    (irq->busnum & 0xff) != dev->pci_bus ||
	    irq->devnum != dev->pci_slot ||
	    irq->funcnum != dev->pci_func)
		return EINVAL;

	irq->irq = dev->irq;

	DRM_DEBUG("%d:%d:%d => IRQ %d\n",
	    irq->busnum, irq->devnum, irq->funcnum, irq->irq);

	return 0;
}

static irqreturn_t
drm_irq_handler_wrap(DRM_IRQ_ARGS)
{
	struct drm_device *dev = arg;

	DRM_SPINLOCK(&dev->irq_lock);
	dev->driver->irq_handler(arg);
	DRM_SPINUNLOCK(&dev->irq_lock);
}

static void vblank_disable_fn(void *arg)
{
	struct drm_device *dev = (struct drm_device *)arg;
	int i;

	/* Make sure that we are called with the lock held */
	mtx_assert(&dev->vbl_lock, MA_OWNED);

	if (callout_pending(&dev->vblank_disable_timer)) {
		/* callout was reset */
		return;
	}
	if (!callout_active(&dev->vblank_disable_timer)) {
		/* callout was stopped */
		return;
	}
	callout_deactivate(&dev->vblank_disable_timer);

	DRM_DEBUG("vblank_disable: %s\n", dev->vblank_disable_allowed ?
		"allowed" : "denied");
	if (!dev->vblank_disable_allowed)
		return;

	for (i = 0; i < dev->num_crtcs; i++) {
		if (dev->vblank[i].refcount == 0 &&
		    dev->vblank[i].enabled && !dev->vblank[i].inmodeset) {
			DRM_DEBUG("disabling vblank on crtc %d\n", i);
			dev->vblank[i].last =
			    dev->driver->get_vblank_counter(dev, i);
			dev->driver->disable_vblank(dev, i);
			dev->vblank[i].enabled = 0;
		}
	}
}

void drm_vblank_cleanup(struct drm_device *dev)
{
	/* Bail if the driver didn't call drm_vblank_init() */
	if (dev->num_crtcs == 0)
		return;

	DRM_SPINLOCK(&dev->vbl_lock);
	callout_stop(&dev->vblank_disable_timer);
	DRM_SPINUNLOCK(&dev->vbl_lock);

	callout_drain(&dev->vblank_disable_timer);

	DRM_SPINLOCK(&dev->vbl_lock);
	vblank_disable_fn((void *)dev);
	DRM_SPINUNLOCK(&dev->vbl_lock);

	free(dev->vblank, DRM_MEM_DRIVER);

	dev->num_crtcs = 0;
}

int drm_vblank_init(struct drm_device *dev, int num_crtcs)
{
	int i, ret = ENOMEM;

	callout_init_mtx(&dev->vblank_disable_timer, &dev->vbl_lock, 0);
	dev->num_crtcs = num_crtcs;

	dev->vblank = malloc(sizeof(struct drm_vblank_info) * num_crtcs,
	    DRM_MEM_DRIVER, M_NOWAIT | M_ZERO);
	if (!dev->vblank)
	    goto err;

	DRM_DEBUG("\n");

	/* Zero per-crtc vblank stuff */
	DRM_SPINLOCK(&dev->vbl_lock);
	for (i = 0; i < num_crtcs; i++) {
		DRM_INIT_WAITQUEUE(&dev->vblank[i].queue);
		dev->vblank[i].refcount = 0;
		atomic_store_rel_32(&dev->vblank[i].count, 0);
	}
	dev->vblank_disable_allowed = 0;
	DRM_SPINUNLOCK(&dev->vbl_lock);

	return 0;

err:
	drm_vblank_cleanup(dev);
	return ret;
}

int drm_irq_install(struct drm_device *dev)
{
	int crtc, retcode;

	if (dev->irq == 0 || dev->dev_private == NULL)
		return EINVAL;

	DRM_DEBUG("irq=%d\n", dev->irq);

	DRM_LOCK();
	if (dev->irq_enabled) {
		DRM_UNLOCK();
		return EBUSY;
	}
	dev->irq_enabled = 1;

	dev->context_flag = 0;

	/* Before installing handler */
	dev->driver->irq_preinstall(dev);
	DRM_UNLOCK();

	/* Install handler */
	retcode = bus_setup_intr(dev->device, dev->irqr,
				 INTR_TYPE_TTY | INTR_MPSAFE,
				 NULL, drm_irq_handler_wrap, dev, &dev->irqh);
	if (retcode != 0)
		goto err;

	/* After installing handler */
	DRM_LOCK();
	dev->driver->irq_postinstall(dev);
	DRM_UNLOCK();
	if (dev->driver->enable_vblank) {
		DRM_SPINLOCK(&dev->vbl_lock);
		for( crtc = 0 ; crtc < dev->num_crtcs ; crtc++) {
			if (dev->driver->enable_vblank(dev, crtc) == 0) {
				dev->vblank[crtc].enabled = 1;
			}
		}
		callout_reset(&dev->vblank_disable_timer, 5 * DRM_HZ,
		    (timeout_t *)vblank_disable_fn, (void *)dev);
		DRM_SPINUNLOCK(&dev->vbl_lock);
	}

	return 0;
err:
	DRM_LOCK();
	dev->irq_enabled = 0;
	DRM_UNLOCK();

	return retcode;
}

int drm_irq_uninstall(struct drm_device *dev)
{
	int crtc;

	if (!dev->irq_enabled)
		return EINVAL;

	dev->irq_enabled = 0;

	/*
	* Wake up any waiters so they don't hang.
	*/
	DRM_SPINLOCK(&dev->vbl_lock);
	for (crtc = 0; crtc < dev->num_crtcs; crtc++) {
		if (dev->vblank[crtc].enabled) {
			DRM_WAKEUP(&dev->vblank[crtc].queue);
			dev->vblank[crtc].last =
			    dev->driver->get_vblank_counter(dev, crtc);
			dev->vblank[crtc].enabled = 0;
		}
	}
	DRM_SPINUNLOCK(&dev->vbl_lock);

	DRM_DEBUG("irq=%d\n", dev->irq);

	dev->driver->irq_uninstall(dev);

	DRM_UNLOCK();
	bus_teardown_intr(dev->device, dev->irqr, dev->irqh);
	DRM_LOCK();

	return 0;
}

int drm_control(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_control *ctl = data;
	int err;

	switch (ctl->func) {
	case DRM_INST_HANDLER:
		/* Handle drivers whose DRM used to require IRQ setup but the
		 * no longer does.
		 */
		if (!drm_core_check_feature(dev, DRIVER_HAVE_IRQ))
			return 0;
		if (dev->if_version < DRM_IF_VERSION(1, 2) &&
		    ctl->irq != dev->irq)
			return EINVAL;
		return drm_irq_install(dev);
	case DRM_UNINST_HANDLER:
		if (!drm_core_check_feature(dev, DRIVER_HAVE_IRQ))
			return 0;
		DRM_LOCK();
		err = drm_irq_uninstall(dev);
		DRM_UNLOCK();
		return err;
	default:
		return EINVAL;
	}
}

u32 drm_vblank_count(struct drm_device *dev, int crtc)
{
	return atomic_load_acq_32(&dev->vblank[crtc].count);
}

static void drm_update_vblank_count(struct drm_device *dev, int crtc)
{
	u32 cur_vblank, diff;

	/*
	 * Interrupts were disabled prior to this call, so deal with counter
	 * wrap if needed.
	 * NOTE!  It's possible we lost a full dev->max_vblank_count events
	 * here if the register is small or we had vblank interrupts off for
	 * a long time.
	 */
	cur_vblank = dev->driver->get_vblank_counter(dev, crtc);
	diff = cur_vblank - dev->vblank[crtc].last;
	if (cur_vblank < dev->vblank[crtc].last) {
		diff += dev->max_vblank_count;

		DRM_DEBUG("vblank[%d].last=0x%x, cur_vblank=0x%x => diff=0x%x\n",
		    crtc, dev->vblank[crtc].last, cur_vblank, diff);
	}

	DRM_DEBUG("enabling vblank interrupts on crtc %d, missed %d\n",
	    crtc, diff);

	atomic_add_rel_32(&dev->vblank[crtc].count, diff);
}

int drm_vblank_get(struct drm_device *dev, int crtc)
{
	int ret = 0;

	/* Make sure that we are called with the lock held */
	mtx_assert(&dev->vbl_lock, MA_OWNED);

	/* Going from 0->1 means we have to enable interrupts again */
	if (++dev->vblank[crtc].refcount == 1 &&
	    !dev->vblank[crtc].enabled) {
		ret = dev->driver->enable_vblank(dev, crtc);
		DRM_DEBUG("enabling vblank on crtc %d, ret: %d\n", crtc, ret);
		if (ret)
			--dev->vblank[crtc].refcount;
		else {
			dev->vblank[crtc].enabled = 1;
			drm_update_vblank_count(dev, crtc);
		}
	}

	if (dev->vblank[crtc].enabled)
		dev->vblank[crtc].last =
		    dev->driver->get_vblank_counter(dev, crtc);

	return ret;
}

void drm_vblank_put(struct drm_device *dev, int crtc)
{
	/* Make sure that we are called with the lock held */
	mtx_assert(&dev->vbl_lock, MA_OWNED);

	KASSERT(dev->vblank[crtc].refcount > 0,
	    ("invalid refcount"));

	/* Last user schedules interrupt disable */
	if (--dev->vblank[crtc].refcount == 0)
	    callout_reset(&dev->vblank_disable_timer, 5 * DRM_HZ,
		(timeout_t *)vblank_disable_fn, (void *)dev);
}

int drm_modeset_ctl(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	struct drm_modeset_ctl *modeset = data;
	int crtc, ret = 0;

	/* If drm_vblank_init() hasn't been called yet, just no-op */
	if (!dev->num_crtcs)
		goto out;

	crtc = modeset->crtc;
	if (crtc >= dev->num_crtcs) {
		ret = EINVAL;
		goto out;
	}

	/*
	 * To avoid all the problems that might happen if interrupts
	 * were enabled/disabled around or between these calls, we just
	 * have the kernel take a reference on the CRTC (just once though
	 * to avoid corrupting the count if multiple, mismatch calls occur),
	 * so that interrupts remain enabled in the interim.
	 */
	switch (modeset->cmd) {
	case _DRM_PRE_MODESET:
		DRM_DEBUG("pre-modeset, crtc %d\n", crtc);
		DRM_SPINLOCK(&dev->vbl_lock);
		if (!dev->vblank[crtc].inmodeset) {
			dev->vblank[crtc].inmodeset = 0x1;
			if (drm_vblank_get(dev, crtc) == 0)
				dev->vblank[crtc].inmodeset |= 0x2;
		}
		DRM_SPINUNLOCK(&dev->vbl_lock);
		break;
	case _DRM_POST_MODESET:
		DRM_DEBUG("post-modeset, crtc %d\n", crtc);
		DRM_SPINLOCK(&dev->vbl_lock);
		if (dev->vblank[crtc].inmodeset) {
			if (dev->vblank[crtc].inmodeset & 0x2)
				drm_vblank_put(dev, crtc);
			dev->vblank[crtc].inmodeset = 0;
		}
		dev->vblank_disable_allowed = 1;
		DRM_SPINUNLOCK(&dev->vbl_lock);
		break;
	default:
		ret = EINVAL;
		break;
	}

out:
	return ret;
}

int drm_wait_vblank(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	union drm_wait_vblank *vblwait = data;
	unsigned int flags, seq, crtc;
	int ret = 0;

	if (!dev->irq_enabled)
		return EINVAL;

	if (vblwait->request.type &
	    ~(_DRM_VBLANK_TYPES_MASK | _DRM_VBLANK_FLAGS_MASK)) {
		DRM_ERROR("Unsupported type value 0x%x, supported mask 0x%x\n",
		    vblwait->request.type,
		    (_DRM_VBLANK_TYPES_MASK | _DRM_VBLANK_FLAGS_MASK));
		return EINVAL;
	}

	flags = vblwait->request.type & _DRM_VBLANK_FLAGS_MASK;
	crtc = flags & _DRM_VBLANK_SECONDARY ? 1 : 0;

	if (crtc >= dev->num_crtcs)
		return EINVAL;

	DRM_SPINLOCK(&dev->vbl_lock);
	ret = drm_vblank_get(dev, crtc);
	DRM_SPINUNLOCK(&dev->vbl_lock);
	if (ret) {
		DRM_ERROR("failed to acquire vblank counter, %d\n", ret);
		return ret;
	}
	seq = drm_vblank_count(dev, crtc);

	switch (vblwait->request.type & _DRM_VBLANK_TYPES_MASK) {
	case _DRM_VBLANK_RELATIVE:
		vblwait->request.sequence += seq;
		vblwait->request.type &= ~_DRM_VBLANK_RELATIVE;
	case _DRM_VBLANK_ABSOLUTE:
		break;
	default:
		ret = EINVAL;
		goto done;
	}

	if ((flags & _DRM_VBLANK_NEXTONMISS) &&
	    (seq - vblwait->request.sequence) <= (1<<23)) {
		vblwait->request.sequence = seq + 1;
	}

	if (flags & _DRM_VBLANK_SIGNAL) {
		/* There have never been any consumers */
		ret = EINVAL;
	} else {
		DRM_DEBUG("waiting on vblank count %d, crtc %d\n",
		    vblwait->request.sequence, crtc);
		for ( ret = 0 ; !ret && !(((drm_vblank_count(dev, crtc) -
		    vblwait->request.sequence) <= (1 << 23)) ||
		    !dev->irq_enabled) ; ) {
			mtx_lock(&dev->irq_lock);
			if (!(((drm_vblank_count(dev, crtc) -
			    vblwait->request.sequence) <= (1 << 23)) ||
			    !dev->irq_enabled))
				ret = mtx_sleep(&dev->vblank[crtc].queue,
				    &dev->irq_lock, PCATCH, "vblwtq",
				    DRM_HZ);
			mtx_unlock(&dev->irq_lock);
		}

		if (ret != EINTR && ret != ERESTART) {
			struct timeval now;

			microtime(&now);
			vblwait->reply.tval_sec = now.tv_sec;
			vblwait->reply.tval_usec = now.tv_usec;
			vblwait->reply.sequence = drm_vblank_count(dev, crtc);
			DRM_DEBUG("returning %d to client, irq_enabled %d\n",
			    vblwait->reply.sequence, dev->irq_enabled);
		} else {
			DRM_DEBUG("vblank wait interrupted by signal\n");
		}
	}

done:
	DRM_SPINLOCK(&dev->vbl_lock);
	drm_vblank_put(dev, crtc);
	DRM_SPINUNLOCK(&dev->vbl_lock);

	return ret;
}

void drm_handle_vblank(struct drm_device *dev, int crtc)
{
	atomic_add_rel_32(&dev->vblank[crtc].count, 1);
	DRM_WAKEUP(&dev->vblank[crtc].queue);
}

