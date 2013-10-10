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

/** @file drm_auth.c
 * Implementation of the get/authmagic ioctls implementing the authentication
 * scheme between the master and clients.
 */

#include <dev/drm2/drmP.h>

static int drm_hash_magic(drm_magic_t magic)
{
	return magic & (DRM_HASH_SIZE-1);
}

/**
 * Returns the file private associated with the given magic number.
 */
static struct drm_file *drm_find_file(struct drm_device *dev, drm_magic_t magic)
{
	drm_magic_entry_t *pt;
	int hash = drm_hash_magic(magic);

	DRM_LOCK_ASSERT(dev);

	for (pt = dev->magiclist[hash].head; pt; pt = pt->next) {
		if (pt->magic == magic) {
			return pt->priv;
		}
	}

	return NULL;
}

/**
 * Inserts the given magic number into the hash table of used magic number
 * lists.
 */
static int drm_add_magic(struct drm_device *dev, struct drm_file *priv,
			 drm_magic_t magic)
{
	int		  hash;
	drm_magic_entry_t *entry;

	DRM_DEBUG("%d\n", magic);

	DRM_LOCK_ASSERT(dev);

	hash = drm_hash_magic(magic);
	entry = malloc(sizeof(*entry), DRM_MEM_MAGIC, M_ZERO | M_NOWAIT);
	if (!entry)
		return ENOMEM;
	entry->magic = magic;
	entry->priv  = priv;
	entry->next  = NULL;

	if (dev->magiclist[hash].tail) {
		dev->magiclist[hash].tail->next = entry;
		dev->magiclist[hash].tail	= entry;
	} else {
		dev->magiclist[hash].head	= entry;
		dev->magiclist[hash].tail	= entry;
	}

	return 0;
}

/**
 * Removes the given magic number from the hash table of used magic number
 * lists.
 */
static int drm_remove_magic(struct drm_device *dev, drm_magic_t magic)
{
	drm_magic_entry_t *prev = NULL;
	drm_magic_entry_t *pt;
	int		  hash;

	DRM_LOCK_ASSERT(dev);

	DRM_DEBUG("%d\n", magic);
	hash = drm_hash_magic(magic);

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
			free(pt, DRM_MEM_MAGIC);
			return 0;
		}
	}

	return EINVAL;
}

/**
 * Called by the client, this returns a unique magic number to be authorized
 * by the master.
 *
 * The master may use its own knowledge of the client (such as the X
 * connection that the magic is passed over) to determine if the magic number
 * should be authenticated.
 */
int drm_getmagic(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	static drm_magic_t sequence = 0;
	struct drm_auth *auth = data;

	/* Find unique magic */
	if (file_priv->magic) {
		auth->magic = file_priv->magic;
	} else {
		DRM_LOCK(dev);
		do {
			int old = sequence;

			auth->magic = old+1;

			if (!atomic_cmpset_int(&sequence, old, auth->magic))
				continue;
		} while (drm_find_file(dev, auth->magic));
		file_priv->magic = auth->magic;
		drm_add_magic(dev, file_priv, auth->magic);
		DRM_UNLOCK(dev);
	}

	DRM_DEBUG("%u\n", auth->magic);

	return 0;
}

/**
 * Marks the client associated with the given magic number as authenticated.
 */
int drm_authmagic(struct drm_device *dev, void *data,
		  struct drm_file *file_priv)
{
	struct drm_auth *auth = data;
	struct drm_file *priv;

	DRM_DEBUG("%u\n", auth->magic);

	DRM_LOCK(dev);
	priv = drm_find_file(dev, auth->magic);
	if (priv != NULL) {
		priv->authenticated = 1;
		drm_remove_magic(dev, auth->magic);
		DRM_UNLOCK(dev);
		return 0;
	} else {
		DRM_UNLOCK(dev);
		return EINVAL;
	}
}
