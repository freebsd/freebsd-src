/* ioctl.c -- IOCTL processing for DRM -*- linux-c -*-
 * Created: Fri Jan  8 09:01:26 1999 by faith@precisioninsight.com
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
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 * 
 * Authors:
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 *
 */

#define __NO_VERSION__
#include "drmP.h"

int drm_irq_busid(struct inode *inode, struct file *filp, unsigned int cmd,
		  unsigned long arg)
{
	drm_irq_busid_t p;
	struct pci_dev	*dev;

	if (copy_from_user(&p, (drm_irq_busid_t *)arg, sizeof(p)))
		return -EFAULT;
	dev = pci_find_slot(p.busnum, PCI_DEVFN(p.devnum, p.funcnum));
	if (dev) p.irq = dev->irq;
	else	 p.irq = 0;
	DRM_DEBUG("%d:%d:%d => IRQ %d\n",
		  p.busnum, p.devnum, p.funcnum, p.irq);
	if (copy_to_user((drm_irq_busid_t *)arg, &p, sizeof(p)))
		return -EFAULT;
	return 0;
}

int drm_getunique(struct inode *inode, struct file *filp, unsigned int cmd,
		  unsigned long arg)
{
	drm_file_t	 *priv	 = filp->private_data;
	drm_device_t	 *dev	 = priv->dev;
	drm_unique_t	 u;

	if (copy_from_user(&u, (drm_unique_t *)arg, sizeof(u)))
		return -EFAULT;
	if (u.unique_len >= dev->unique_len) {
		if (copy_to_user(u.unique, dev->unique, dev->unique_len))
			return -EFAULT;
	}
	u.unique_len = dev->unique_len;
	if (copy_to_user((drm_unique_t *)arg, &u, sizeof(u)))
		return -EFAULT;
	return 0;
}

int drm_setunique(struct inode *inode, struct file *filp, unsigned int cmd,
		  unsigned long arg)
{
	drm_file_t	 *priv	 = filp->private_data;
	drm_device_t	 *dev	 = priv->dev;
	drm_unique_t	 u;

	if (dev->unique_len || dev->unique)
		return -EBUSY;

	if (copy_from_user(&u, (drm_unique_t *)arg, sizeof(u)))
		return -EFAULT;

	if (!u.unique_len || u.unique_len > 1024)
		return -EINVAL;
	
	dev->unique_len = u.unique_len;
	dev->unique	= drm_alloc(u.unique_len + 1, DRM_MEM_DRIVER);
	if (copy_from_user(dev->unique, u.unique, dev->unique_len))
		return -EFAULT;
	dev->unique[dev->unique_len] = '\0';

	dev->devname = drm_alloc(strlen(dev->name) + strlen(dev->unique) + 2,
				 DRM_MEM_DRIVER);
	sprintf(dev->devname, "%s@%s", dev->name, dev->unique);

	return 0;
}
