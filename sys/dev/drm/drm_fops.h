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

#define __NO_VERSION__
#include "dev/drm/drmP.h"

#ifdef __linux__
#include <linux/poll.h>
#endif /* __linux__ */

#ifdef __FreeBSD__
#include <sys/signalvar.h>
#include <sys/poll.h>

drm_file_t *DRM(find_file_by_proc)(drm_device_t *dev, DRM_OS_STRUCTPROC *p)
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
#endif /* __FreeBSD__ */

/* DRM(open) is called whenever a process opens /dev/drm. */

#ifdef __linux__
int DRM(open_helper)(struct inode *inode, struct file *filp, drm_device_t *dev)
{
	kdev_t	     m = MINOR(inode->i_rdev);
#endif /* __linux__ */
#ifdef __FreeBSD__
int DRM(open_helper)(dev_t kdev, int flags, int fmt, DRM_OS_STRUCTPROC *p,
		    drm_device_t *dev)
{
	int	     m = minor(kdev);
#endif /* __FreeBSD__ */
	drm_file_t   *priv;

#ifdef __linux__
	if (filp->f_flags & O_EXCL) return -EBUSY; /* No exclusive opens */
#endif /* __linux__ */
#ifdef __FreeBSD__
	if (flags & O_EXCL)
		return EBUSY; /* No exclusive opens */
	dev->flags = flags;
#endif /* __FreeBSD__ */
	if (!DRM(cpu_valid)())
		return DRM_OS_ERR(EINVAL);

	DRM_DEBUG("pid = %d, minor = %d\n", DRM_OS_CURRENTPID, m);

#ifdef __linux__
	priv = (drm_file_t *) DRM(alloc)(sizeof(*priv), DRM_MEM_FILES);
	if(!priv) return DRM_OS_ERR(ENOMEM);

	memset(priv, 0, sizeof(*priv));
	filp->private_data  = priv;
	priv->uid	    = current->euid;
	priv->pid	    = current->pid;
	priv->minor	    = m;
	priv->dev	    = dev;
	priv->ioctl_count   = 0;
	priv->authenticated = capable(CAP_SYS_ADMIN);

	down(&dev->struct_sem);
	if (!dev->file_last) {
		priv->next	= NULL;
		priv->prev	= NULL;
		dev->file_first = priv;
		dev->file_last	= priv;
	} else {
		priv->next	     = NULL;
		priv->prev	     = dev->file_last;
		dev->file_last->next = priv;
		dev->file_last	     = priv;
	}
	up(&dev->struct_sem);
#endif /* __linux__ */
#ifdef __FreeBSD__
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
		priv->authenticated	= !DRM_OS_CHECKSUSER;
		lockmgr(&dev->dev_lock, LK_EXCLUSIVE, 0, p);
		TAILQ_INSERT_TAIL(&dev->files, priv, link);
		lockmgr(&dev->dev_lock, LK_RELEASE, 0, p);
	}

	kdev->si_drv1 = dev;
#endif /* __FreeBSD__ */

#ifdef __linux__
#ifdef __alpha__
	/*
	 * Default the hose
	 */
	if (!dev->hose) {
		struct pci_dev *pci_dev;
		pci_dev = pci_find_class(PCI_CLASS_DISPLAY_VGA << 8, NULL);
		if (pci_dev) dev->hose = pci_dev->sysdata;
		if (!dev->hose) {
			struct pci_bus *b = pci_bus_b(pci_root_buses.next);
			if (b) dev->hose = b->sysdata;
		}
	}
#endif
#endif /* __linux__ */

	return 0;
}

#ifdef __linux__
int DRM(flush)(struct file *filp)
{
	drm_file_t	*priv	= filp->private_data;
	drm_device_t	*dev	= priv->dev;

	DRM_DEBUG("pid = %d, device = 0x%x, open_count = %d\n",
		  current->pid, dev->device, dev->open_count);
	return 0;
}

int DRM(fasync)(int fd, struct file *filp, int on)
{
	drm_file_t	*priv	= filp->private_data;
	drm_device_t	*dev	= priv->dev;
	int	      retcode;

	DRM_DEBUG("fd = %d, device = 0x%x\n", fd, dev->device);
	retcode = fasync_helper(fd, filp, on, &dev->buf_async);
	if (retcode < 0) return retcode;
	return 0;
}
#endif /* __linux__ */

/* The drm_read and drm_write_string code (especially that which manages
   the circular buffer), is based on Alessandro Rubini's LINUX DEVICE
   DRIVERS (Cambridge: O'Reilly, 1998), pages 111-113. */

#ifdef __linux__
ssize_t DRM(read)(struct file *filp, char *buf, size_t count, loff_t *off)
#endif /* __linux__ */
#ifdef __FreeBSD__
ssize_t DRM(read)(dev_t kdev, struct uio *uio, int ioflag)
#endif /* __FreeBSD__ */
{
	DRM_OS_DEVICE;
	int	      left;
	int	      avail;
	int	      send;
	int	      cur;
#ifdef __FreeBSD__
	int           error = 0;
#endif /* __FreeBSD__ */

	DRM_DEBUG("%p, %p\n", dev->buf_rp, dev->buf_wp);

	while (dev->buf_rp == dev->buf_wp) {
		DRM_DEBUG("  sleeping\n");
#ifdef __linux__
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		interruptible_sleep_on(&dev->buf_readers);
		if (signal_pending(current)) {
			DRM_DEBUG("  interrupted\n");
			return -ERESTARTSYS;
		}
#endif /* __linux__ */
#ifdef __FreeBSD__
		if (dev->flags & FASYNC)
			return EWOULDBLOCK;
		error = tsleep(&dev->buf_rp, PZERO|PCATCH, "drmrd", 0);
		if (error) {
			DRM_DEBUG("  interrupted\n");
			return error;
		}
#endif /* __FreeBSD__ */
		DRM_DEBUG("  awake\n");
	}

	left  = (dev->buf_rp + DRM_BSZ - dev->buf_wp) % DRM_BSZ;
	avail = DRM_BSZ - left;
#ifdef __linux__
	send  = DRM_MIN(avail, count);
#endif /* __linux__ */
#ifdef __FreeBSD__
	send  = DRM_MIN(avail, uio->uio_resid);
#endif /* __FreeBSD__ */

	while (send) {
		if (dev->buf_wp > dev->buf_rp) {
			cur = DRM_MIN(send, dev->buf_wp - dev->buf_rp);
		} else {
			cur = DRM_MIN(send, dev->buf_end - dev->buf_rp);
		}
#ifdef __linux__
		if (copy_to_user(buf, dev->buf_rp, cur))
			return -EFAULT;
#endif /* __linux__ */
#ifdef __FreeBSD__
		error = uiomove(dev->buf_rp, cur, uio);
		if (error)
			break;
#endif /* __FreeBSD__ */
		dev->buf_rp += cur;
		if (dev->buf_rp == dev->buf_end) dev->buf_rp = dev->buf;
		send -= cur;
	}

#ifdef __linux__
	wake_up_interruptible(&dev->buf_writers);
	return DRM_MIN(avail, count);
#endif /* __linux__ */
#ifdef __FreeBSD__
	wakeup(&dev->buf_wp);
	return error;
#endif /* __FreeBSD__ */
}

int DRM(write_string)(drm_device_t *dev, const char *s)
{
	int left   = (dev->buf_rp + DRM_BSZ - dev->buf_wp) % DRM_BSZ;
	int send   = strlen(s);
	int count;

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

#ifdef __linux__
	if (dev->buf_async) kill_fasync(&dev->buf_async, SIGIO, POLL_IN);
	DRM_DEBUG("waking\n");
	wake_up_interruptible(&dev->buf_readers);
#endif /* __linux__ */
#ifdef __FreeBSD__
	if (dev->buf_selecting) {
		dev->buf_selecting = 0;
		selwakeup(&dev->buf_sel);
	}
		
	DRM_DEBUG("dev->buf_sigio=%p\n", dev->buf_sigio);
	if (dev->buf_sigio) {
		DRM_DEBUG("dev->buf_sigio->sio_pgid=%d\n", dev->buf_sigio->sio_pgid);
		pgsigio(&dev->buf_sigio, SIGIO, 0);
	}
	DRM_DEBUG("waking\n");
	wakeup(&dev->buf_rp);
#endif /* __FreeBSD__ */

	return 0;
}

#ifdef __linux__
unsigned int DRM(poll)(struct file *filp, struct poll_table_struct *wait)
{
	DRM_OS_DEVICE;

	poll_wait(filp, &dev->buf_readers, wait);
	if (dev->buf_wp != dev->buf_rp) return POLLIN | POLLRDNORM;
	return 0;
}
#endif /* __linux__ */
#ifdef __FreeBSD__
int DRM(poll)(dev_t kdev, int events, DRM_OS_STRUCTPROC *p)
{
	drm_device_t  *dev    = kdev->si_drv1;
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
        DRM_DEBUG("pid = %d, device = %p, open_count = %d\n",
                  curproc->p_pid, ((drm_device_t *)kdev->si_drv1)->device, ((drm_device_t *)kdev->si_drv1)->open_count);
        return 0;
}
#endif /* __FreeBSD__ */
