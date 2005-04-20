/* drm_fops.h -- File operations for DRM -*- linux-c -*-
 * Created: Mon Jan  4 08:58:31 1999 by faith@valinux.com */
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
 *    Daryll Strauss <daryll@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 *
 * $FreeBSD$
 */

#include "dev/drm/drmP.h"

drm_file_t *DRM(find_file_by_proc)(drm_device_t *dev, DRM_STRUCTPROC *p)
{
#if __FreeBSD_version >= 500021
	uid_t uid = p->td_ucred->cr_svuid;
	pid_t pid = p->td_proc->p_pid;
#else
	uid_t uid = p->p_cred->p_svuid;
	pid_t pid = p->p_pid;
#endif
	drm_file_t *priv;

	DRM_SPINLOCK_ASSERT(&dev->dev_lock);

	TAILQ_FOREACH(priv, &dev->files, link)
		if (priv->pid == pid && priv->uid == uid)
			return priv;
	return NULL;
}

/* DRM(open_helper) is called whenever a process opens /dev/drm. */
int DRM(open_helper)(struct cdev *kdev, int flags, int fmt, DRM_STRUCTPROC *p,
		    drm_device_t *dev)
{
	int	     m = minor(kdev);
	drm_file_t   *priv;

	if (flags & O_EXCL)
		return EBUSY; /* No exclusive opens */
	dev->flags = flags;

	DRM_DEBUG("pid = %d, minor = %d\n", DRM_CURRENTPID, m);

	DRM_LOCK();
	priv = DRM(find_file_by_proc)(dev, p);
	if (priv) {
		priv->refs++;
	} else {
		priv = (drm_file_t *) DRM(alloc)(sizeof(*priv), DRM_MEM_FILES);
		if (priv == NULL) {
			DRM_UNLOCK();
			return DRM_ERR(ENOMEM);
		}
		bzero(priv, sizeof(*priv));
#if __FreeBSD_version >= 500000
		priv->uid		= p->td_ucred->cr_svuid;
		priv->pid		= p->td_proc->p_pid;
#else
		priv->uid		= p->p_cred->p_svuid;
		priv->pid		= p->p_pid;
#endif

		priv->refs		= 1;
		priv->minor		= m;
		priv->devXX		= dev;
		priv->ioctl_count 	= 0;
		priv->authenticated	= !DRM_SUSER(p);

		DRIVER_OPEN_HELPER( priv, dev );

		TAILQ_INSERT_TAIL(&dev->files, priv, link);
	}
	DRM_UNLOCK();
#ifdef __FreeBSD__
	kdev->si_drv1 = dev;
#endif
	return 0;
}


/* The DRM(read) and DRM(poll) are stubs to prevent spurious errors
 * on older X Servers (4.3.0 and earlier) */

int DRM(read)(struct cdev *kdev, struct uio *uio, int ioflag)
{
	return 0;
}

int DRM(poll)(struct cdev *kdev, int events, DRM_STRUCTPROC *p)
{
	return 0;
}
