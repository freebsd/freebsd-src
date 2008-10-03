/*-
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
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/** @file drm_lock.c
 * Implementation of the ioctls and other support code for dealing with the
 * hardware lock.
 *
 * The DRM hardware lock is a shared structure between the kernel and userland.
 *
 * On uncontended access where the new context was the last context, the
 * client may take the lock without dropping down into the kernel, using atomic
 * compare-and-set.
 *
 * If the client finds during compare-and-set that it was not the last owner
 * of the lock, it calls the DRM lock ioctl, which may sleep waiting for the
 * lock, and may have side-effects of kernel-managed context switching.
 *
 * When the client releases the lock, if the lock is marked as being contended
 * by another client, then the DRM unlock ioctl is called so that the
 * contending client may be woken up.
 */

#include "dev/drm/drmP.h"

int drm_lock(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_lock *lock = data;
	int ret = 0;

	if (lock->context == DRM_KERNEL_CONTEXT) {
		DRM_ERROR("Process %d using kernel context %d\n",
		    DRM_CURRENTPID, lock->context);
		return EINVAL;
	}

	DRM_DEBUG("%d (pid %d) requests lock (0x%08x), flags = 0x%08x\n",
	    lock->context, DRM_CURRENTPID, dev->lock.hw_lock->lock,
	    lock->flags);

	if (drm_core_check_feature(dev, DRIVER_DMA_QUEUE) &&
	    lock->context < 0)
		return EINVAL;

	DRM_LOCK();
	for (;;) {
		if (drm_lock_take(&dev->lock, lock->context)) {
			dev->lock.file_priv = file_priv;
			dev->lock.lock_time = jiffies;
			atomic_inc(&dev->counts[_DRM_STAT_LOCKS]);
			break;  /* Got lock */
		}

		/* Contention */
		ret = mtx_sleep((void *)&dev->lock.lock_queue, &dev->dev_lock,
		    PZERO | PCATCH, "drmlk2", 0);
		if (ret != 0)
			break;
	}
	DRM_UNLOCK();
	DRM_DEBUG("%d %s\n", lock->context, ret ? "interrupted" : "has lock");

	if (ret != 0)
		return ret;

	/* XXX: Add signal blocking here */

	if (dev->driver->dma_quiescent != NULL &&
	    (lock->flags & _DRM_LOCK_QUIESCENT))
		dev->driver->dma_quiescent(dev);

	return 0;
}

int drm_unlock(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_lock *lock = data;

	if (lock->context == DRM_KERNEL_CONTEXT) {
		DRM_ERROR("Process %d using kernel context %d\n",
		    DRM_CURRENTPID, lock->context);
		return EINVAL;
	}
	/* Check that the context unlock being requested actually matches
	 * who currently holds the lock.
	 */
	if (!_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock) ||
	    _DRM_LOCKING_CONTEXT(dev->lock.hw_lock->lock) != lock->context)
		return EINVAL;

	DRM_SPINLOCK(&dev->tsk_lock);
	if (dev->locked_task_call != NULL) {
		dev->locked_task_call(dev);
		dev->locked_task_call = NULL;
	}
	DRM_SPINUNLOCK(&dev->tsk_lock);

	atomic_inc(&dev->counts[_DRM_STAT_UNLOCKS]);

	DRM_LOCK();
	drm_lock_transfer(&dev->lock, DRM_KERNEL_CONTEXT);

	if (drm_lock_free(&dev->lock, DRM_KERNEL_CONTEXT)) {
		DRM_ERROR("\n");
	}
	DRM_UNLOCK();

	return 0;
}

int drm_lock_take(struct drm_lock_data *lock_data, unsigned int context)
{
	volatile unsigned int *lock = &lock_data->hw_lock->lock;
	unsigned int old, new;

	do {
		old = *lock;
		if (old & _DRM_LOCK_HELD)
			new = old | _DRM_LOCK_CONT;
		else
			new = context | _DRM_LOCK_HELD;
	} while (!atomic_cmpset_int(lock, old, new));

	if (_DRM_LOCKING_CONTEXT(old) == context) {
		if (old & _DRM_LOCK_HELD) {
			if (context != DRM_KERNEL_CONTEXT) {
				DRM_ERROR("%d holds heavyweight lock\n",
				    context);
			}
			return 0;
		}
	}
	if (new == (context | _DRM_LOCK_HELD)) {
		/* Have lock */
		return 1;
	}
	return 0;
}

/* This takes a lock forcibly and hands it to context.	Should ONLY be used
   inside *_unlock to give lock to kernel before calling *_dma_schedule. */
int drm_lock_transfer(struct drm_lock_data *lock_data, unsigned int context)
{
	volatile unsigned int *lock = &lock_data->hw_lock->lock;
	unsigned int old, new;

	lock_data->file_priv = NULL;
	do {
		old = *lock;
		new = context | _DRM_LOCK_HELD;
	} while (!atomic_cmpset_int(lock, old, new));

	return 1;
}

int drm_lock_free(struct drm_lock_data *lock_data, unsigned int context)
{
	volatile unsigned int *lock = &lock_data->hw_lock->lock;
	unsigned int old, new;

	lock_data->file_priv = NULL;
	do {
		old = *lock;
		new = 0;
	} while (!atomic_cmpset_int(lock, old, new));

	if (_DRM_LOCK_IS_HELD(old) && _DRM_LOCKING_CONTEXT(old) != context) {
		DRM_ERROR("%d freed heavyweight lock held by %d\n",
		    context, _DRM_LOCKING_CONTEXT(old));
		return 1;
	}
	DRM_WAKEUP_INT((void *)&lock_data->lock_queue);
	return 0;
}
