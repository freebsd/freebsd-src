/* drm_auth.h -- IOCTLs for authentication -*- linux-c -*-
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
 */

#include "drmP.h"

static int DRM(hash_magic)(drm_magic_t magic)
{
	return magic & (DRM_HASH_SIZE-1);
}

static drm_file_t *DRM(find_file)(drm_device_t *dev, drm_magic_t magic)
{
	drm_file_t	  *retval = NULL;
	drm_magic_entry_t *pt;
	int		  hash	  = DRM(hash_magic)(magic);

	down(&dev->struct_sem);
	for (pt = dev->magiclist[hash].head; pt; pt = pt->next) {
		if (pt->magic == magic) {
			retval = pt->priv;
			break;
		}
	}
	up(&dev->struct_sem);
	return retval;
}

int DRM(add_magic)(drm_device_t *dev, drm_file_t *priv, drm_magic_t magic)
{
	int		  hash;
	drm_magic_entry_t *entry;

	DRM_DEBUG("%d\n", magic);

	hash	     = DRM(hash_magic)(magic);
	entry	     = DRM(alloc)(sizeof(*entry), DRM_MEM_MAGIC);
	if (!entry) return -ENOMEM;
	memset(entry, 0, sizeof(*entry));
	entry->magic = magic;
	entry->priv  = priv;
	entry->next  = NULL;

	down(&dev->struct_sem);
	if (dev->magiclist[hash].tail) {
		dev->magiclist[hash].tail->next = entry;
		dev->magiclist[hash].tail	= entry;
	} else {
		dev->magiclist[hash].head	= entry;
		dev->magiclist[hash].tail	= entry;
	}
	up(&dev->struct_sem);

	return 0;
}

int DRM(remove_magic)(drm_device_t *dev, drm_magic_t magic)
{
	drm_magic_entry_t *prev = NULL;
	drm_magic_entry_t *pt;
	int		  hash;

	DRM_DEBUG("%d\n", magic);
	hash = DRM(hash_magic)(magic);

	down(&dev->struct_sem);
	for (pt = dev->magiclist[hash].head; pt; prev = pt, pt = pt->next) {
		if (pt->magic == magic) {
			if (dev->magiclist[hash].head == pt) {
				dev->magiclist[hash].head = pt->next;
			}
			if (dev->magiclist[hash].tail == pt) {
				dev->magiclist[hash].tail = prev;
			}
			if (prev) {
				prev->next = pt->next;
			}
			up(&dev->struct_sem);
			return 0;
		}
	}
	up(&dev->struct_sem);

	DRM(free)(pt, sizeof(*pt), DRM_MEM_MAGIC);

	return -EINVAL;
}

int DRM(getmagic)(struct inode *inode, struct file *filp,
		  unsigned int cmd, unsigned long arg)
{
	static drm_magic_t sequence = 0;
	static spinlock_t  lock	    = SPIN_LOCK_UNLOCKED;
	drm_file_t	   *priv    = filp->private_data;
	drm_device_t	   *dev	    = priv->dev;
	drm_auth_t	   auth;

				/* Find unique magic */
	if (priv->magic) {
		auth.magic = priv->magic;
	} else {
		do {
			spin_lock(&lock);
			if (!sequence) ++sequence; /* reserve 0 */
			auth.magic = sequence++;
			spin_unlock(&lock);
		} while (DRM(find_file)(dev, auth.magic));
		priv->magic = auth.magic;
		DRM(add_magic)(dev, priv, auth.magic);
	}

	DRM_DEBUG("%u\n", auth.magic);
	if (copy_to_user((drm_auth_t *)arg, &auth, sizeof(auth)))
		return -EFAULT;
	return 0;
}

int DRM(authmagic)(struct inode *inode, struct file *filp,
		   unsigned int cmd, unsigned long arg)
{
	drm_file_t	   *priv    = filp->private_data;
	drm_device_t	   *dev	    = priv->dev;
	drm_auth_t	   auth;
	drm_file_t	   *file;

	if (copy_from_user(&auth, (drm_auth_t *)arg, sizeof(auth)))
		return -EFAULT;
	DRM_DEBUG("%u\n", auth.magic);
	if ((file = DRM(find_file)(dev, auth.magic))) {
		file->authenticated = 1;
		DRM(remove_magic)(dev, auth.magic);
		return 0;
	}
	return -EINVAL;
}
