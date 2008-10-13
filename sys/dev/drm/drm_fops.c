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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/** @file drm_fops.c
 * Support code for dealing with the file privates associated with each
 * open of the DRM device.
 */

#include "dev/drm/drmP.h"

/* drm_open_helper is called whenever a process opens /dev/drm. */
int drm_open_helper(struct cdev *kdev, int flags, int fmt, DRM_STRUCTPROC *p,
		    struct drm_device *dev)
{
	struct drm_file *priv;
	int m = minor(kdev);
	int retcode;

	if (flags & O_EXCL)
		return EBUSY; /* No exclusive opens */
	dev->flags = flags;

	DRM_DEBUG("pid = %d, minor = %d\n", DRM_CURRENTPID, m);

	priv = malloc(sizeof(*priv), DRM_MEM_FILES, M_NOWAIT | M_ZERO);
	if (priv == NULL) {
		return ENOMEM;
	}

	retcode = devfs_set_cdevpriv(priv, drm_close);
	if (retcode != 0) {
		free(priv, DRM_MEM_FILES);
		return retcode;
	}

	DRM_LOCK();
	priv->dev		= dev;
	priv->uid		= p->td_ucred->cr_svuid;
	priv->pid		= p->td_proc->p_pid;
	priv->minor		= m;
	priv->ioctl_count 	= 0;

	/* for compatibility root is always authenticated */
	priv->authenticated	= DRM_SUSER(p);

	if (dev->driver->open) {
		/* shared code returns -errno */
		retcode = -dev->driver->open(dev, priv);
		if (retcode != 0) {
			devfs_clear_cdevpriv();
			free(priv, DRM_MEM_FILES);
			DRM_UNLOCK();
			return retcode;
		}
	}

	/* first opener automatically becomes master */
	priv->master = TAILQ_EMPTY(&dev->files);

	TAILQ_INSERT_TAIL(&dev->files, priv, link);
	DRM_UNLOCK();
	kdev->si_drv1 = dev;
	return 0;
}


/* The drm_read and drm_poll are stubs to prevent spurious errors
 * on older X Servers (4.3.0 and earlier) */

int drm_read(struct cdev *kdev, struct uio *uio, int ioflag)
{
	return 0;
}

int drm_poll(struct cdev *kdev, int events, DRM_STRUCTPROC *p)
{
	return 0;
}
