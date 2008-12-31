/* drm_auth.h -- IOCTLs for authentication -*- linux-c -*-
 * Created: Tue Feb  2 08:37:54 1999 by faith@valinux.com
 */
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
__FBSDID("$FreeBSD: src/sys/dev/drm/drm_auth.c,v 1.2.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include "dev/drm/drmP.h"

static int drm_hash_magic(drm_magic_t magic)
{
	return magic & (DRM_HASH_SIZE-1);
}

static drm_file_t *drm_find_file(drm_device_t *dev, drm_magic_t magic)
{
	drm_file_t	  *retval = NULL;
	drm_magic_entry_t *pt;
	int		  hash;

	hash = drm_hash_magic(magic);

	DRM_LOCK();
	for (pt = dev->magiclist[hash].head; pt; pt = pt->next) {
		if (pt->magic == magic) {
			retval = pt->priv;
			break;
		}
	}
	DRM_UNLOCK();
	return retval;
}

static int drm_add_magic(drm_device_t *dev, drm_file_t *priv, drm_magic_t magic)
{
	int		  hash;
	drm_magic_entry_t *entry;

	DRM_DEBUG("%d\n", magic);

	hash = drm_hash_magic(magic);
	entry = malloc(sizeof(*entry), M_DRM, M_ZERO | M_NOWAIT);
	if (!entry) return DRM_ERR(ENOMEM);
	entry->magic = magic;
	entry->priv  = priv;
	entry->next  = NULL;

	DRM_LOCK();
	if (dev->magiclist[hash].tail) {
		dev->magiclist[hash].tail->next = entry;
		dev->magiclist[hash].tail	= entry;
	} else {
		dev->magiclist[hash].head	= entry;
		dev->magiclist[hash].tail	= entry;
	}
	DRM_UNLOCK();

	return 0;
}

static int drm_remove_magic(drm_device_t *dev, drm_magic_t magic)
{
	drm_magic_entry_t *prev = NULL;
	drm_magic_entry_t *pt;
	int		  hash;

	DRM_DEBUG("%d\n", magic);
	hash = drm_hash_magic(magic);

	DRM_LOCK();
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
			DRM_UNLOCK();
			return 0;
		}
	}
	DRM_UNLOCK();

	free(pt, M_DRM);
	return DRM_ERR(EINVAL);
}

int drm_getmagic(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	static drm_magic_t sequence = 0;
	drm_auth_t auth;
	drm_file_t *priv;

	DRM_LOCK();
	priv = drm_find_file_by_proc(dev, p);
	DRM_UNLOCK();
	if (priv == NULL) {
		DRM_ERROR("can't find authenticator\n");
		return EINVAL;
	}

				/* Find unique magic */
	if (priv->magic) {
		auth.magic = priv->magic;
	} else {
		do {
			int old = sequence;
			
			auth.magic = old+1;
			
			if (!atomic_cmpset_int(&sequence, old, auth.magic))
				continue;
		} while (drm_find_file(dev, auth.magic));
		priv->magic = auth.magic;
		drm_add_magic(dev, priv, auth.magic);
	}

	DRM_DEBUG("%u\n", auth.magic);

	DRM_COPY_TO_USER_IOCTL((drm_auth_t *)data, auth, sizeof(auth));

	return 0;
}

int drm_authmagic(DRM_IOCTL_ARGS)
{
	drm_auth_t	   auth;
	drm_file_t	   *file;
	DRM_DEVICE;

	DRM_COPY_FROM_USER_IOCTL(auth, (drm_auth_t *)data, sizeof(auth));

	DRM_DEBUG("%u\n", auth.magic);

	if ((file = drm_find_file(dev, auth.magic))) {
		file->authenticated = 1;
		drm_remove_magic(dev, auth.magic);
		return 0;
	}
	return DRM_ERR(EINVAL);
}
