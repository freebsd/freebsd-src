/* drm_fops.h -- File operations for DRM -*- linux-c -*-
 * Created: Mon Jan  4 08:58:31 1999 by faith@valinux.com
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
 *    Daryll Strauss <daryll@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 *
 * $FreeBSD$
 */

#include "dev/drm/drmP.h"

drm_file_t *DRM(find_file_by_proc)(drm_device_t *dev, DRM_STRUCTPROC *p)
{
#if __FreeBSD_version >= 500021
	uid_t uid = p->td_proc->p_ucred->cr_svuid;
	pid_t pid = p->td_proc->p_pid;
#else
	uid_t uid = p->p_cred->p_svuid;
	pid_t pid = p->p_pid;
#endif
	drm_file_t *priv;

	TAILQ_FOREACH(priv, &dev->files, link)
		if (priv->pid == pid && priv->uid == uid)
			return priv;
	return NULL;
}

/* DRM(open) is called whenever a process opens /dev/drm. */

int DRM(open_helper)(dev_t kdev, int flags, int fmt, DRM_STRUCTPROC *p,
		    drm_device_t *dev)
{
	int	     m = minor(kdev);
	drm_file_t   *priv;

	if (flags & O_EXCL)
		return EBUSY; /* No exclusive opens */
	dev->flags = flags;
	if (!DRM(cpu_valid)())
		return DRM_ERR(EINVAL);

	DRM_DEBUG("pid = %d, minor = %d\n", DRM_CURRENTPID, m);

	/* FIXME: linux mallocs and bzeros here */
	priv = (drm_file_t *) DRM(find_file_by_proc)(dev, p);
	if (priv) {
		priv->refs++;
	} else {
		priv = (drm_file_t *) DRM(alloc)(sizeof(*priv), DRM_MEM_FILES);
		bzero(priv, sizeof(*priv));
#if __FreeBSD_version >= 500000
		priv->uid		= p->td_proc->p_ucred->cr_svuid;
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
		DRM_LOCK;
		TAILQ_INSERT_TAIL(&dev->files, priv, link);
		DRM_UNLOCK;
	}
#ifdef __FreeBSD__
	kdev->si_drv1 = dev;
#endif
	return 0;
}


/* The drm_read and drm_write_string code (especially that which manages
   the circular buffer), is based on Alessandro Rubini's LINUX DEVICE
   DRIVERS (Cambridge: O'Reilly, 1998), pages 111-113. */

int DRM(read)(dev_t kdev, struct uio *uio, int ioflag)
{
	DRM_DEVICE;
	int	      left;
	int	      avail;
	int	      send;
	int	      cur;
	int           error = 0;

	DRM_DEBUG("%p, %p\n", dev->buf_rp, dev->buf_wp);

	while (dev->buf_rp == dev->buf_wp) {
		DRM_DEBUG("  sleeping\n");
		if (dev->flags & FASYNC)
			return EWOULDBLOCK;
		error = tsleep(&dev->buf_rp, PZERO|PCATCH, "drmrd", 0);
		if (error) {
			DRM_DEBUG("  interrupted\n");
			return error;
		}
		DRM_DEBUG("  awake\n");
	}

	left  = (dev->buf_rp + DRM_BSZ - dev->buf_wp) % DRM_BSZ;
	avail = DRM_BSZ - left;
	send  = DRM_MIN(avail, uio->uio_resid);

	while (send) {
		if (dev->buf_wp > dev->buf_rp) {
			cur = DRM_MIN(send, dev->buf_wp - dev->buf_rp);
		} else {
			cur = DRM_MIN(send, dev->buf_end - dev->buf_rp);
		}
		error = uiomove(dev->buf_rp, cur, uio);
		if (error)
			break;
		dev->buf_rp += cur;
		if (dev->buf_rp == dev->buf_end) dev->buf_rp = dev->buf;
		send -= cur;
	}

	wakeup(&dev->buf_wp);
	return error;
}

int DRM(write_string)(drm_device_t *dev, const char *s)
{
	int left   = (dev->buf_rp + DRM_BSZ - dev->buf_wp) % DRM_BSZ;
	int send   = strlen(s);
	int count;
#ifdef __NetBSD__
	struct proc *p;
#endif /* __NetBSD__ */

	DRM_DEBUG("%d left, %d to send (%p, %p)\n",
		  left, send, dev->buf_rp, dev->buf_wp);

	if (left == 1 || dev->buf_wp != dev->buf_rp) {
		DRM_ERROR("Buffer not empty (%d left, wp = %p, rp = %p)\n",
			  left,
			  dev->buf_wp,
			  dev->buf_rp);
	}

	while (send) {
		if (dev->buf_wp >= dev->buf_rp) {
			count = DRM_MIN(send, dev->buf_end - dev->buf_wp);
			if (count == left) --count; /* Leave a hole */
		} else {
			count = DRM_MIN(send, dev->buf_rp - dev->buf_wp - 1);
		}
		strncpy(dev->buf_wp, s, count);
		dev->buf_wp += count;
		if (dev->buf_wp == dev->buf_end) dev->buf_wp = dev->buf;
		send -= count;
	}

	if (dev->buf_selecting) {
		dev->buf_selecting = 0;
		selwakeup(&dev->buf_sel);
	}
		
#ifdef __FreeBSD__
	DRM_DEBUG("dev->buf_sigio=%p\n", dev->buf_sigio);
	if (dev->buf_sigio) {
		DRM_DEBUG("dev->buf_sigio->sio_pgid=%d\n", dev->buf_sigio->sio_pgid);
#if __FreeBSD_version >= 500000
		pgsigio(&dev->buf_sigio, SIGIO, 0);
#else
		pgsigio(dev->buf_sigio, SIGIO, 0);
#endif /* __FreeBSD_version */
	}
#endif /* __FreeBSD__ */
#ifdef __NetBSD__
	if (dev->buf_pgid) {
		DRM_DEBUG("dev->buf_pgid=%d\n", dev->buf_pgid);
		if(dev->buf_pgid > 0)
			gsignal(dev->buf_pgid, SIGIO);
		else if(dev->buf_pgid && (p = pfind(-dev->buf_pgid)) != NULL)
			psignal(p, SIGIO);
	}
#endif /* __NetBSD__ */
	DRM_DEBUG("waking\n");
	wakeup(&dev->buf_rp);

	return 0;
}

int DRM(poll)(dev_t kdev, int events, DRM_STRUCTPROC *p)
{
	DRM_DEVICE;
	int           s;
	int	      revents = 0;

	s = spldrm();
	if (events & (POLLIN | POLLRDNORM)) {
		int left  = (dev->buf_rp + DRM_BSZ - dev->buf_wp) % DRM_BSZ;
		if (left > 0)
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(p, &dev->buf_sel);
	}
	splx(s);

	return revents;
}

int DRM(write)(dev_t kdev, struct uio *uio, int ioflag)
{
#if DRM_DEBUG_CODE
	DRM_DEVICE;
#endif
#ifdef __FreeBSD__
	DRM_DEBUG("pid = %d, device = %p, open_count = %d\n",
		  curproc->p_pid, dev->device, dev->open_count);
#elif defined(__NetBSD__)
	DRM_DEBUG("pid = %d, device = %p, open_count = %d\n",
		  curproc->p_pid, &dev->device, dev->open_count);
#endif
	return 0;
}
