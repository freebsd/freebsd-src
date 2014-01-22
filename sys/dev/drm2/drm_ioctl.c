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

/** @file drm_ioctl.c
 * Varios minor DRM ioctls not applicable to other files, such as versioning
 * information and reporting DRM information to userland.
 */

#include <dev/drm2/drmP.h>
#include <dev/drm2/drm_core.h>

/*
 * Beginning in revision 1.1 of the DRM interface, getunique will return
 * a unique in the form pci:oooo:bb:dd.f (o=domain, b=bus, d=device, f=function)
 * before setunique has been called.  The format for the bus-specific part of
 * the unique is not defined for any other bus.
 */
int drm_getunique(struct drm_device *dev, void *data,
		  struct drm_file *file_priv)
{
	struct drm_unique *u = data;

	if (u->unique_len >= dev->unique_len) {
		if (DRM_COPY_TO_USER(u->unique, dev->unique, dev->unique_len))
			return EFAULT;
	}
	u->unique_len = dev->unique_len;

	return 0;
}

/* Deprecated in DRM version 1.1, and will return EBUSY when setversion has
 * requested version 1.1 or greater.
 */
int drm_setunique(struct drm_device *dev, void *data,
		  struct drm_file *file_priv)
{
	struct drm_unique *u = data;
	int domain, bus, slot, func, ret;
	char *busid;

	/* Check and copy in the submitted Bus ID */
	if (!u->unique_len || u->unique_len > 1024)
		return EINVAL;

	busid = malloc(u->unique_len + 1, DRM_MEM_DRIVER, M_WAITOK);
	if (busid == NULL)
		return ENOMEM;

	if (DRM_COPY_FROM_USER(busid, u->unique, u->unique_len)) {
		free(busid, DRM_MEM_DRIVER);
		return EFAULT;
	}
	busid[u->unique_len] = '\0';

	/* Return error if the busid submitted doesn't match the device's actual
	 * busid.
	 */
	ret = sscanf(busid, "PCI:%d:%d:%d", &bus, &slot, &func);
	if (ret != 3) {
		free(busid, DRM_MEM_DRIVER);
		return EINVAL;
	}
	domain = bus >> 8;
	bus &= 0xff;
	
	if ((domain != dev->pci_domain) ||
	    (bus != dev->pci_bus) ||
	    (slot != dev->pci_slot) ||
	    (func != dev->pci_func)) {
		free(busid, DRM_MEM_DRIVER);
		return EINVAL;
	}

	/* Actually set the device's busid now. */
	DRM_LOCK(dev);
	if (dev->unique_len || dev->unique) {
		DRM_UNLOCK(dev);
		return EBUSY;
	}

	dev->unique_len = u->unique_len;
	dev->unique = busid;
	DRM_UNLOCK(dev);

	return 0;
}


static int
drm_set_busid(struct drm_device *dev)
{

	DRM_LOCK(dev);

	if (dev->unique != NULL) {
		DRM_UNLOCK(dev);
		return EBUSY;
	}

	dev->unique_len = 20;
	dev->unique = malloc(dev->unique_len + 1, DRM_MEM_DRIVER, M_NOWAIT);
	if (dev->unique == NULL) {
		DRM_UNLOCK(dev);
		return ENOMEM;
	}

	snprintf(dev->unique, dev->unique_len, "pci:%04x:%02x:%02x.%1x",
	    dev->pci_domain, dev->pci_bus, dev->pci_slot, dev->pci_func);

	DRM_UNLOCK(dev);

	return 0;
}

int drm_getmap(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_map     *map = data;
	drm_local_map_t    *mapinlist;
	int          idx;
	int	     i = 0;

	idx = map->offset;

	DRM_LOCK(dev);
	if (idx < 0) {
		DRM_UNLOCK(dev);
		return EINVAL;
	}

	TAILQ_FOREACH(mapinlist, &dev->maplist, link) {
		if (i == idx) {
			map->offset = mapinlist->offset;
			map->size   = mapinlist->size;
			map->type   = mapinlist->type;
			map->flags  = mapinlist->flags;
			map->handle = mapinlist->handle;
			map->mtrr   = mapinlist->mtrr;
			break;
		}
		i++;
	}

	DRM_UNLOCK(dev);

 	if (mapinlist == NULL)
		return EINVAL;

	return 0;
}

int drm_getclient(struct drm_device *dev, void *data,
		  struct drm_file *file_priv)
{
	struct drm_client *client = data;
	struct drm_file *pt;
	int idx;
	int i = 0;

	idx = client->idx;
	DRM_LOCK(dev);
	TAILQ_FOREACH(pt, &dev->files, link) {
		if (i == idx) {
			client->auth  = pt->authenticated;
			client->pid   = pt->pid;
			client->uid   = pt->uid;
			client->magic = pt->magic;
			client->iocs  = pt->ioctl_count;
			DRM_UNLOCK(dev);
			return 0;
		}
		i++;
	}
	DRM_UNLOCK(dev);

	return EINVAL;
}

int drm_getstats(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_stats *stats = data;
	int          i;

	memset(stats, 0, sizeof(struct drm_stats));
	
	DRM_LOCK(dev);

	for (i = 0; i < dev->counters; i++) {
		if (dev->types[i] == _DRM_STAT_LOCK)
			stats->data[i].value =
			    (dev->lock.hw_lock ? dev->lock.hw_lock->lock : 0);
		else 
			stats->data[i].value = atomic_read(&dev->counts[i]);
		stats->data[i].type = dev->types[i];
	}
	
	stats->count = dev->counters;

	DRM_UNLOCK(dev);

	return 0;
}

int drm_getcap(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_get_cap *req = data;

	req->value = 0;
	switch (req->capability) {
	case DRM_CAP_DUMB_BUFFER:
		if (dev->driver->dumb_create)
			req->value = 1;
		break;
	case DRM_CAP_VBLANK_HIGH_CRTC:
		req->value = 1;
		break;
	case DRM_CAP_DUMB_PREFERRED_DEPTH:
		req->value = dev->mode_config.preferred_depth;
		break;
	case DRM_CAP_DUMB_PREFER_SHADOW:
		req->value = dev->mode_config.prefer_shadow;
		break;
	case DRM_CAP_TIMESTAMP_MONOTONIC:
		req->value = drm_timestamp_monotonic;
		break;
	default:
		return EINVAL;
	}
	return 0;
}

int drm_setversion(struct drm_device *dev, void *data,
		   struct drm_file *file_priv)
{
	struct drm_set_version *sv = data;
	struct drm_set_version ver;
	int if_version;

	/* Save the incoming data, and set the response before continuing
	 * any further.
	 */
	ver = *sv;
	sv->drm_di_major = DRM_IF_MAJOR;
	sv->drm_di_minor = DRM_IF_MINOR;
	sv->drm_dd_major = dev->driver->major;
	sv->drm_dd_minor = dev->driver->minor;

	DRM_DEBUG("ver.drm_di_major %d ver.drm_di_minor %d "
	    "ver.drm_dd_major %d ver.drm_dd_minor %d\n",
	    ver.drm_di_major, ver.drm_di_minor, ver.drm_dd_major,
	    ver.drm_dd_minor);
	DRM_DEBUG("sv->drm_di_major %d sv->drm_di_minor %d "
	    "sv->drm_dd_major %d sv->drm_dd_minor %d\n",
	    sv->drm_di_major, sv->drm_di_minor, sv->drm_dd_major,
	    sv->drm_dd_minor);

	if (ver.drm_di_major != -1) {
		if (ver.drm_di_major != DRM_IF_MAJOR ||
		    ver.drm_di_minor < 0 || ver.drm_di_minor > DRM_IF_MINOR) {
			return EINVAL;
		}
		if_version = DRM_IF_VERSION(ver.drm_di_major,
		    ver.drm_dd_minor);
		dev->if_version = DRM_MAX(if_version, dev->if_version);
		if (ver.drm_di_minor >= 1) {
			/*
			 * Version 1.1 includes tying of DRM to specific device
			 */
			drm_set_busid(dev);
		}
	}

	if (ver.drm_dd_major != -1) {
		if (ver.drm_dd_major != dev->driver->major ||
		    ver.drm_dd_minor < 0 ||
		    ver.drm_dd_minor > dev->driver->minor)
		{
			return EINVAL;
		}
	}

	return 0;
}


int drm_noop(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	DRM_DEBUG("\n");
	return 0;
}
