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

static void drm_locked_task(void *context, int pending __unused);

int drm_irq_by_busid(struct drm_device *dev, void *data,
		     struct drm_file *file_priv)
{
	drm_irq_busid_t *irq = data;

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

#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
static irqreturn_t
drm_irq_handler_wrap(DRM_IRQ_ARGS)
{
	struct drm_device *dev = arg;

	DRM_SPINLOCK(&dev->irq_lock);
	dev->driver.irq_handler(arg);
	DRM_SPINUNLOCK(&dev->irq_lock);
}
#endif

static void vblank_disable_fn(void *arg)
{
	struct drm_device *dev = (struct drm_device *)arg;
	int i;

	if (callout_pending(&dev->vblank_disable_timer)) {
		/* callout was reset */
		return;
	}
	if (!callout_active(&dev->vblank_disable_timer)) {
		/* callout was stopped */
		return;
	}
	callout_deactivate(&dev->vblank_disable_timer);

	if (!dev->vblank_disable_allowed)
		return;

	for (i = 0; i < dev->num_crtcs; i++) {
		if (atomic_read(&dev->vblank[i].refcount) == 0 &&
		    dev->vblank[i].enabled) {
			DRM_DEBUG("disabling vblank on crtc %d\n", i);
			dev->vblank[i].last =
			    dev->driver.get_vblank_counter(dev, i);
			dev->driver.disable_vblank(dev, i);
			dev->vblank[i].enabled = 0;
		}
	}
}

static void drm_vblank_cleanup(struct drm_device *dev)
{
	unsigned long irqflags;

	/* Bail if the driver didn't call drm_vblank_init() */
	if (dev->num_crtcs == 0)
	    return;

	DRM_SPINLOCK_IRQSAVE(&dev->vbl_lock, irqflags);
	callout_stop(&dev->vblank_disable_timer);
	DRM_SPINUNLOCK_IRQRESTORE(&dev->vbl_lock, irqflags);

	callout_drain(&dev->vblank_disable_timer);

	vblank_disable_fn((void *)dev);

	drm_free(dev->vblank, sizeof(struct drm_vblank_info) * dev->num_crtcs,
	    DRM_MEM_DRIVER);

	dev->num_crtcs = 0;
}

int drm_vblank_init(struct drm_device *dev, int num_crtcs)
{
	int i, ret = ENOMEM;

	callout_init_mtx(&dev->vblank_disable_timer, &dev->vbl_lock, 0);
	atomic_set(&dev->vbl_signal_pending, 0);
	dev->num_crtcs = num_crtcs;

	dev->vblank = drm_calloc(num_crtcs, sizeof(struct drm_vblank_info),
	    DRM_MEM_DRIVER);
	if (!dev->vblank)
	    goto err;

	/* Zero per-crtc vblank stuff */
	for (i = 0; i < num_crtcs; i++) {
		DRM_INIT_WAITQUEUE(&dev->vblank[i].queue);
		TAILQ_INIT(&dev->vblank[i].sigs);
		atomic_set(&dev->vblank[i].count, 0);
		atomic_set(&dev->vblank[i].refcount, 0);
	}

	dev->vblank_disable_allowed = 0;

	return 0;

err:
	drm_vblank_cleanup(dev);
	return ret;
}

int drm_irq_install(struct drm_device *dev)
{
	int retcode;
#ifdef __NetBSD__
	pci_intr_handle_t ih;
#endif

	if (dev->irq == 0 || dev->dev_private == NULL)
		return EINVAL;

	DRM_DEBUG( "%s: irq=%d\n", __FUNCTION__, dev->irq );

	DRM_LOCK();
	if (dev->irq_enabled) {
		DRM_UNLOCK();
		return EBUSY;
	}
	dev->irq_enabled = 1;

	dev->context_flag = 0;

				/* Before installing handler */
	dev->driver.irq_preinstall(dev);
	DRM_UNLOCK();

				/* Install handler */
#ifdef __FreeBSD__
	dev->irqrid = 0;
	dev->irqr = bus_alloc_resource_any(dev->device, SYS_RES_IRQ, 
				      &dev->irqrid, RF_SHAREABLE);
	if (!dev->irqr) {
		retcode = ENOENT;
		goto err;
	}
#if __FreeBSD_version >= 700031
	retcode = bus_setup_intr(dev->device, dev->irqr,
				 INTR_TYPE_TTY | INTR_MPSAFE,
				 NULL, drm_irq_handler_wrap, dev, &dev->irqh);
#else
	retcode = bus_setup_intr(dev->device, dev->irqr,
				 INTR_TYPE_TTY | INTR_MPSAFE,
				 drm_irq_handler_wrap, dev, &dev->irqh);
#endif
	if (retcode != 0)
		goto err;
#elif defined(__NetBSD__) || defined(__OpenBSD__)
	if (pci_intr_map(&dev->pa, &ih) != 0) {
		retcode = ENOENT;
		goto err;
	}
	dev->irqh = pci_intr_establish(&dev->pa.pa_pc, ih, IPL_TTY,
	    (irqreturn_t (*)(void *))dev->irq_handler, dev);
	if (!dev->irqh) {
		retcode = ENOENT;
		goto err;
	}
#endif

				/* After installing handler */
	DRM_LOCK();
	dev->driver.irq_postinstall(dev);
	DRM_UNLOCK();

	TASK_INIT(&dev->locked_task, 0, drm_locked_task, dev);
	return 0;
err:
	DRM_LOCK();
	dev->irq_enabled = 0;
#ifdef ___FreeBSD__
	if (dev->irqrid != 0) {
		bus_release_resource(dev->device, SYS_RES_IRQ, dev->irqrid,
		    dev->irqr);
		dev->irqrid = 0;
	}
#endif
	DRM_UNLOCK();
	return retcode;
}

int drm_irq_uninstall(struct drm_device *dev)
{
#ifdef __FreeBSD__
	int irqrid;
#endif

	if (!dev->irq_enabled)
		return EINVAL;

	dev->irq_enabled = 0;
#ifdef __FreeBSD__
	irqrid = dev->irqrid;
	dev->irqrid = 0;
#endif

	DRM_DEBUG( "%s: irq=%d\n", __FUNCTION__, dev->irq );

	dev->driver.irq_uninstall(dev);

#ifdef __FreeBSD__
	DRM_UNLOCK();
	bus_teardown_intr(dev->device, dev->irqr, dev->irqh);
	bus_release_resource(dev->device, SYS_RES_IRQ, irqrid, dev->irqr);
	DRM_LOCK();
#elif defined(__NetBSD__) || defined(__OpenBSD__)
	pci_intr_disestablish(&dev->pa.pa_pc, dev->irqh);
#endif
	drm_vblank_cleanup(dev);

	return 0;
}

int drm_control(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_control_t *ctl = data;
	int err;

	switch ( ctl->func ) {
	case DRM_INST_HANDLER:
		/* Handle drivers whose DRM used to require IRQ setup but the
		 * no longer does.
		 */
		if (!dev->driver.use_irq)
			return 0;
		if (dev->if_version < DRM_IF_VERSION(1, 2) &&
		    ctl->irq != dev->irq)
			return EINVAL;
		return drm_irq_install(dev);
	case DRM_UNINST_HANDLER:
		if (!dev->driver.use_irq)
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
	return atomic_read(&dev->vblank[crtc].count);
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
	cur_vblank = dev->driver.get_vblank_counter(dev, crtc);
	diff = cur_vblank - dev->vblank[crtc].last;
	if (cur_vblank < dev->vblank[crtc].last) {
		diff += dev->max_vblank_count;

		DRM_DEBUG("last_vblank[%d]=0x%x, cur_vblank=0x%x => diff=0x%x\n",
		    crtc, dev->vblank[crtc].last, cur_vblank, diff);
	}

	DRM_DEBUG("enabling vblank interrupts on crtc %d, missed %d\n",
	    crtc, diff);

	atomic_add(diff, &dev->vblank[crtc].count);
}

int drm_vblank_get(struct drm_device *dev, int crtc)
{
	unsigned long irqflags;
	int ret = 0;

	DRM_SPINLOCK_IRQSAVE(&dev->vbl_lock, irqflags);
	/* Going from 0->1 means we have to enable interrupts again */
	atomic_add_acq_int(&dev->vblank[crtc].refcount, 1);
	if (dev->vblank[crtc].refcount == 1 &&
	    !dev->vblank[crtc].enabled) {
		ret = dev->driver.enable_vblank(dev, crtc);
		if (ret)
			atomic_dec(&dev->vblank[crtc].refcount);
		else {
			dev->vblank[crtc].enabled = 1;
			drm_update_vblank_count(dev, crtc);
		}
	}
	DRM_SPINUNLOCK_IRQRESTORE(&dev->vbl_lock, irqflags);

	return ret;
}

void drm_vblank_put(struct drm_device *dev, int crtc)
{
	unsigned long irqflags;

	DRM_SPINLOCK_IRQSAVE(&dev->vbl_lock, irqflags);
	/* Last user schedules interrupt disable */
	atomic_subtract_acq_int(&dev->vblank[crtc].refcount, 1);
	if (dev->vblank[crtc].refcount == 0)
	    callout_reset(&dev->vblank_disable_timer, 5 * DRM_HZ,
		(timeout_t *)vblank_disable_fn, (void *)dev);
	DRM_SPINUNLOCK_IRQRESTORE(&dev->vbl_lock, irqflags);
}

int drm_modeset_ctl(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	struct drm_modeset_ctl *modeset = data;
	unsigned long irqflags;
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
		if (!dev->vblank[crtc].inmodeset) {
			dev->vblank[crtc].inmodeset = 1;
			drm_vblank_get(dev, crtc);
		}
		break;
	case _DRM_POST_MODESET:
		if (dev->vblank[crtc].inmodeset) {
			DRM_SPINLOCK_IRQSAVE(&dev->vbl_lock, irqflags);
			dev->vblank_disable_allowed = 1;
			dev->vblank[crtc].inmodeset = 0;
			DRM_SPINUNLOCK_IRQRESTORE(&dev->vbl_lock, irqflags);
			drm_vblank_put(dev, crtc);
		}
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
	drm_wait_vblank_t *vblwait = data;
	int ret = 0;
	int flags, seq, crtc;

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

	ret = drm_vblank_get(dev, crtc);
	if (ret)
	    return ret;
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
#if 0 /* disabled */
		drm_vbl_sig_t *vbl_sig = malloc(sizeof(drm_vbl_sig_t), M_DRM,
		    M_NOWAIT | M_ZERO);
		if (vbl_sig == NULL)
			return ENOMEM;

		vbl_sig->sequence = vblwait->request.sequence;
		vbl_sig->signo = vblwait->request.signal;
		vbl_sig->pid = DRM_CURRENTPID;

		vblwait->reply.sequence = atomic_read(&dev->vbl_received);
		
		DRM_SPINLOCK(&dev->vbl_lock);
		TAILQ_INSERT_HEAD(&dev->vbl_sig_list, vbl_sig, link);
		DRM_SPINUNLOCK(&dev->vbl_lock);
		ret = 0;
#endif
		ret = EINVAL;
	} else {
		DRM_LOCK();
		/* shared code returns -errno */

		DRM_WAIT_ON(ret, dev->vblank[crtc].queue, 3 * DRM_HZ,
		    ((drm_vblank_count(dev, crtc)
		      - vblwait->request.sequence) <= (1 << 23)));
		DRM_UNLOCK();

		if (ret != EINTR) {
			struct timeval now;

			microtime(&now);
			vblwait->reply.tval_sec = now.tv_sec;
			vblwait->reply.tval_usec = now.tv_usec;
			vblwait->reply.sequence = drm_vblank_count(dev, crtc);
		}
	}

done:
	drm_vblank_put(dev, crtc);
	return ret;
}

void drm_vbl_send_signals(struct drm_device *dev, int crtc)
{
}

#if 0 /* disabled */
void drm_vbl_send_signals(struct drm_device *dev, int crtc )
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

void drm_handle_vblank(struct drm_device *dev, int crtc)
{
	atomic_inc(&dev->vblank[crtc].count);
	DRM_WAKEUP(&dev->vblank[crtc].queue);
	drm_vbl_send_signals(dev, crtc);
}

static void drm_locked_task(void *context, int pending __unused)
{
	struct drm_device *dev = context;

	DRM_SPINLOCK(&dev->tsk_lock);

	DRM_LOCK(); /* XXX drm_lock_take() should do it's own locking */
	if (dev->locked_task_call == NULL ||
	    drm_lock_take(&dev->lock.hw_lock->lock, DRM_KERNEL_CONTEXT) == 0) {
		DRM_UNLOCK();
		DRM_SPINUNLOCK(&dev->tsk_lock);
		return;
	}

	dev->lock.file_priv = NULL; /* kernel owned */
	dev->lock.lock_time = jiffies;
	atomic_inc(&dev->counts[_DRM_STAT_LOCKS]);

	DRM_UNLOCK();

	dev->locked_task_call(dev);

	drm_lock_free(dev, &dev->lock.hw_lock->lock, DRM_KERNEL_CONTEXT);

	dev->locked_task_call = NULL;

	DRM_SPINUNLOCK(&dev->tsk_lock);
}

void
drm_locked_tasklet(struct drm_device *dev,
		   void (*tasklet)(struct drm_device *dev))
{
	DRM_SPINLOCK(&dev->tsk_lock);
	if (dev->locked_task_call != NULL) {
		DRM_SPINUNLOCK(&dev->tsk_lock);
		return;
	}

	dev->locked_task_call = tasklet;
	DRM_SPINUNLOCK(&dev->tsk_lock);
	taskqueue_enqueue(taskqueue_swi, &dev->locked_task);
}
