/* drm_ioctl.h -- IOCTL processing for DRM -*- linux-c -*-
 * Created: Fri Jan  8 09:01:26 1999 by faith@valinux.com
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
 * $FreeBSD$
 */

#include "dev/drm/drmP.h"

/*
 * Beginning in revision 1.1 of the DRM interface, getunique will return
 * a unique in the form pci:oooo:bb:dd.f (o=domain, b=bus, d=device, f=function)
 * before setunique has been called.  The format for the bus-specific part of
 * the unique is not defined for any other bus.
 */
int drm_getunique(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_unique_t	 u;

	DRM_COPY_FROM_USER_IOCTL( u, (drm_unique_t *)data, sizeof(u) );

	if (u.unique_len >= dev->unique_len) {
		if (DRM_COPY_TO_USER(u.unique, dev->unique, dev->unique_len))
			return DRM_ERR(EFAULT);
	}
	u.unique_len = dev->unique_len;

	DRM_COPY_TO_USER_IOCTL( (drm_unique_t *)data, u, sizeof(u) );

	return 0;
}

/* Deprecated in DRM version 1.1, and will return EBUSY when setversion has
 * requested version 1.1 or greater.
 */
int drm_setunique(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_unique_t u;
	int domain, bus, slot, func, ret;
	char *busid;

	DRM_COPY_FROM_USER_IOCTL( u, (drm_unique_t *)data, sizeof(u) );

	/* Check and copy in the submitted Bus ID */
	if (!u.unique_len || u.unique_len > 1024)
		return DRM_ERR(EINVAL);

	busid = malloc(u.unique_len + 1, M_DRM, M_WAITOK);
	if (busid == NULL)
		return DRM_ERR(ENOMEM);

	if (DRM_COPY_FROM_USER(busid, u.unique, u.unique_len)) {
		free(busid, M_DRM);
		return DRM_ERR(EFAULT);
	}
	busid[u.unique_len] = '\0';

	/* Return error if the busid submitted doesn't match the device's actual
	 * busid.
	 */
	ret = sscanf(busid, "PCI:%d:%d:%d", &bus, &slot, &func);
	if (ret != 3) {
		free(busid, M_DRM);
		return DRM_ERR(EINVAL);
	}
	domain = bus >> 8;
	bus &= 0xff;
	
	if ((domain != dev->pci_domain) ||
	    (bus != dev->pci_bus) ||
	    (slot != dev->pci_slot) ||
	    (func != dev->pci_func)) {
		free(busid, M_DRM);
		return DRM_ERR(EINVAL);
	}

	/* Actually set the device's busid now. */
	DRM_LOCK();
	if (dev->unique_len || dev->unique) {
		DRM_UNLOCK();
		return DRM_ERR(EBUSY);
	}

	dev->unique_len = u.unique_len;
	dev->unique = busid;
	DRM_UNLOCK();

	return 0;
}


static int
drm_set_busid(drm_device_t *dev)
{

	DRM_LOCK();

	if (dev->unique != NULL) {
		DRM_UNLOCK();
		return EBUSY;
	}

	dev->unique_len = 20;
	dev->unique = malloc(dev->unique_len + 1, M_DRM, M_NOWAIT);
	if (dev->unique == NULL) {
		DRM_UNLOCK();
		return ENOMEM;
	}

	snprintf(dev->unique, dev->unique_len, "pci:%04x:%02x:%02x.%1x",
	    dev->pci_domain, dev->pci_bus, dev->pci_slot, dev->pci_func);

	DRM_UNLOCK();

	return 0;
}

int drm_getmap(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_map_t    map;
	drm_local_map_t    *mapinlist;
	int          idx;
	int	     i = 0;

	DRM_COPY_FROM_USER_IOCTL( map, (drm_map_t *)data, sizeof(map) );

	idx = map.offset;

	DRM_LOCK();
	if (idx < 0) {
		DRM_UNLOCK();
		return DRM_ERR(EINVAL);
	}

	TAILQ_FOREACH(mapinlist, &dev->maplist, link) {
		if (i==idx) {
			map.offset = mapinlist->offset;
			map.size   = mapinlist->size;
			map.type   = mapinlist->type;
			map.flags  = mapinlist->flags;
			map.handle = mapinlist->handle;
			map.mtrr   = mapinlist->mtrr;
			break;
		}
		i++;
	}

	DRM_UNLOCK();

 	if (mapinlist == NULL)
		return EINVAL;

	DRM_COPY_TO_USER_IOCTL( (drm_map_t *)data, map, sizeof(map) );

	return 0;
}

int drm_getclient(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_client_t client;
	drm_file_t   *pt;
	int          idx;
	int          i = 0;

	DRM_COPY_FROM_USER_IOCTL( client, (drm_client_t *)data, sizeof(client) );

	idx = client.idx;
	DRM_LOCK();
	TAILQ_FOREACH(pt, &dev->files, link) {
		if (i==idx)
		{
			client.auth  = pt->authenticated;
			client.pid   = pt->pid;
			client.uid   = pt->uid;
			client.magic = pt->magic;
			client.iocs  = pt->ioctl_count;
			DRM_UNLOCK();

			*(drm_client_t *)data = client;
			return 0;
		}
		i++;
	}
	DRM_UNLOCK();

	DRM_COPY_TO_USER_IOCTL( (drm_client_t *)data, client, sizeof(client) );

	return 0;
}

int drm_getstats(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_stats_t  stats;
	int          i;

	memset(&stats, 0, sizeof(stats));
	
	DRM_LOCK();

	for (i = 0; i < dev->counters; i++) {
		if (dev->types[i] == _DRM_STAT_LOCK)
			stats.data[i].value
				= (dev->lock.hw_lock
				   ? dev->lock.hw_lock->lock : 0);
		else 
			stats.data[i].value = atomic_read(&dev->counts[i]);
		stats.data[i].type  = dev->types[i];
	}
	
	stats.count = dev->counters;

	DRM_UNLOCK();

	DRM_COPY_TO_USER_IOCTL( (drm_stats_t *)data, stats, sizeof(stats) );

	return 0;
}

#define DRM_IF_MAJOR	1
#define DRM_IF_MINOR	2

int drm_setversion(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_set_version_t sv;
	drm_set_version_t retv;
	int if_version;

	DRM_COPY_FROM_USER_IOCTL(sv, (drm_set_version_t *)data, sizeof(sv));

	retv.drm_di_major = DRM_IF_MAJOR;
	retv.drm_di_minor = DRM_IF_MINOR;
	retv.drm_dd_major = dev->driver_major;
	retv.drm_dd_minor = dev->driver_minor;

	DRM_COPY_TO_USER_IOCTL((drm_set_version_t *)data, retv, sizeof(sv));

	if (sv.drm_di_major != -1) {
		if (sv.drm_di_major != DRM_IF_MAJOR ||
		    sv.drm_di_minor < 0 || sv.drm_di_minor > DRM_IF_MINOR)
			return EINVAL;
		if_version = DRM_IF_VERSION(sv.drm_di_major, sv.drm_dd_minor);
		dev->if_version = DRM_MAX(if_version, dev->if_version);
		if (sv.drm_di_minor >= 1) {
			/*
			 * Version 1.1 includes tying of DRM to specific device
			 */
			drm_set_busid(dev);
		}
	}

	if (sv.drm_dd_major != -1) {
		if (sv.drm_dd_major != dev->driver_major ||
		    sv.drm_dd_minor < 0 || sv.drm_dd_minor > dev->driver_minor)
			return EINVAL;
	}
	return 0;
}


int drm_noop(DRM_IOCTL_ARGS)
{
	DRM_DEBUG("\n");
	return 0;
}
