/* lock.c -- IOCTLs for locking -*- linux-c -*-
 * Created: Tue Feb  2 08:37:54 1999 by faith@valinux.com
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

int DRM(block)( DRM_IOCTL_ARGS )
{
	DRM_DEBUG("\n");
	return 0;
}

int DRM(unblock)( DRM_IOCTL_ARGS )
{
	DRM_DEBUG("\n");
	return 0;
}

int DRM(lock_take)(__volatile__ unsigned int *lock, unsigned int context)
{
	unsigned int old, new;

	do {
		old = *lock;
		if (old & _DRM_LOCK_HELD) new = old | _DRM_LOCK_CONT;
		else			  new = context | _DRM_LOCK_HELD;
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
int DRM(lock_transfer)(drm_device_t *dev,
		       __volatile__ unsigned int *lock, unsigned int context)
{
	unsigned int old, new;

	dev->lock.pid = 0;
	do {
		old  = *lock;
		new  = context | _DRM_LOCK_HELD;
	} while (!atomic_cmpset_int(lock, old, new));

	return 1;
}

int DRM(lock_free)(drm_device_t *dev,
		   __volatile__ unsigned int *lock, unsigned int context)
{
	unsigned int old, new;
	pid_t        pid = dev->lock.pid;

	dev->lock.pid = 0;
	do {
		old  = *lock;
		new  = 0;
	} while (!atomic_cmpset_int(lock, old, new));

	if (_DRM_LOCK_IS_HELD(old) && _DRM_LOCKING_CONTEXT(old) != context) {
		DRM_ERROR("%d freed heavyweight lock held by %d (pid %d)\n",
			  context,
			  _DRM_LOCKING_CONTEXT(old),
			  pid);
		return 1;
	}
	DRM_WAKEUP_INT((void *)&dev->lock.lock_queue);
	return 0;
}

static int DRM(flush_queue)(drm_device_t *dev, int context)
{
	int               error;
	int		  ret	= 0;
	drm_queue_t	  *q	= dev->queuelist[context];

	DRM_DEBUG("\n");

	atomic_inc(&q->use_count);
	if (atomic_read(&q->use_count) > 1) {
		atomic_inc(&q->block_write);
		atomic_inc(&q->block_count);
		error = tsleep((void *)&q->flush_queue, PZERO|PCATCH, "drmfq", 0);
		if (error)
			return error;
		atomic_dec(&q->block_count);
	}
	atomic_dec(&q->use_count);

				/* NOTE: block_write is still incremented!
				   Use drm_flush_unlock_queue to decrement. */
	return ret;
}

static int DRM(flush_unblock_queue)(drm_device_t *dev, int context)
{
	drm_queue_t	  *q	= dev->queuelist[context];

	DRM_DEBUG("\n");

	atomic_inc(&q->use_count);
	if (atomic_read(&q->use_count) > 1) {
		if (atomic_read(&q->block_write)) {
			atomic_dec(&q->block_write);
			DRM_WAKEUP_INT((void *)&q->write_queue);
		}
	}
	atomic_dec(&q->use_count);
	return 0;
}

int DRM(flush_block_and_flush)(drm_device_t *dev, int context,
			       drm_lock_flags_t flags)
{
	int ret = 0;
	int i;

	DRM_DEBUG("\n");

	if (flags & _DRM_LOCK_FLUSH) {
		ret = DRM(flush_queue)(dev, DRM_KERNEL_CONTEXT);
		if (!ret) ret = DRM(flush_queue)(dev, context);
	}
	if (flags & _DRM_LOCK_FLUSH_ALL) {
		for (i = 0; !ret && i < dev->queue_count; i++) {
			ret = DRM(flush_queue)(dev, i);
		}
	}
	return ret;
}

int DRM(flush_unblock)(drm_device_t *dev, int context, drm_lock_flags_t flags)
{
	int ret = 0;
	int i;

	DRM_DEBUG("\n");

	if (flags & _DRM_LOCK_FLUSH) {
		ret = DRM(flush_unblock_queue)(dev, DRM_KERNEL_CONTEXT);
		if (!ret) ret = DRM(flush_unblock_queue)(dev, context);
	}
	if (flags & _DRM_LOCK_FLUSH_ALL) {
		for (i = 0; !ret && i < dev->queue_count; i++) {
			ret = DRM(flush_unblock_queue)(dev, i);
		}
	}

	return ret;
}

int DRM(finish)( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	int		  ret	  = 0;
	drm_lock_t	  lock;

	DRM_DEBUG("\n");

	DRM_COPY_FROM_USER_IOCTL( lock, (drm_lock_t *)data, sizeof(lock) );

	ret = DRM(flush_block_and_flush)(dev, lock.context, lock.flags);
	DRM(flush_unblock)(dev, lock.context, lock.flags);
	return ret;
}

/* If we get here, it means that the process has called DRM_IOCTL_LOCK
   without calling DRM_IOCTL_UNLOCK.

   If the lock is not held, then let the signal proceed as usual.

   If the lock is held, then set the contended flag and keep the signal
   blocked.


   Return 1 if the signal should be delivered normally.
   Return 0 if the signal should be blocked.  */

int DRM(notifier)(void *priv)
{
	drm_sigdata_t *s = (drm_sigdata_t *)priv;
	unsigned int  old, new;

				/* Allow signal delivery if lock isn't held */
	if (!_DRM_LOCK_IS_HELD(s->lock->lock)
	    || _DRM_LOCKING_CONTEXT(s->lock->lock) != s->context) return 1;

				/* Otherwise, set flag to force call to
                                   drmUnlock */
	do {
		old  = s->lock->lock;
		new  = old | _DRM_LOCK_CONT;
	} while (!atomic_cmpset_int(&s->lock->lock, old, new));

	return 0;
}

