/* drm_agpsupport.h -- DRM support for AGP/GART backend -*- linux-c -*-
 * Created: Mon Dec 13 09:56:45 1999 by faith@precisioninsight.com
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
 * Author:
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/drm/drm_agpsupport.c,v 1.6.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include "dev/drm/drmP.h"

#ifdef __FreeBSD__
#include <pci/agpreg.h>
#include <dev/pci/pcireg.h>
#endif

/* Returns 1 if AGP or 0 if not. */
static int
drm_device_find_capability(drm_device_t *dev, int cap)
{
#ifdef __FreeBSD__
#if __FreeBSD_version >= 602102

	return (pci_find_extcap(dev->device, cap, NULL) == 0);
#else
	/* Code taken from agp.c.  IWBNI that was a public interface. */
	u_int32_t status;
	u_int8_t ptr, next;

	/*
	 * Check the CAP_LIST bit of the PCI status register first.
	 */
	status = pci_read_config(dev->device, PCIR_STATUS, 2);
	if (!(status & 0x10))
		return 0;

	/*
	 * Traverse the capabilities list.
	 */
	for (ptr = pci_read_config(dev->device, AGP_CAPPTR, 1);
	     ptr != 0;
	     ptr = next) {
		u_int32_t capid = pci_read_config(dev->device, ptr, 4);
		next = AGP_CAPID_GET_NEXT_PTR(capid);

		/*
		 * If this capability entry ID is cap, then we are done.
		 */
		if (AGP_CAPID_GET_CAP_ID(capid) == cap)
			return 1;
	}

	return 0;
#endif
#else
	/* XXX: fill me in for non-FreeBSD */
	return 1;
#endif
}

int drm_device_is_agp(drm_device_t *dev)
{
	if (dev->driver.device_is_agp != NULL) {
		int ret;

		/* device_is_agp returns a tristate, 0 = not AGP, 1 = definitely
		 * AGP, 2 = fall back to PCI capability
		 */
		ret = (*dev->driver.device_is_agp)(dev);
		if (ret != DRM_MIGHT_BE_AGP)
			return ret;
	}

	return (drm_device_find_capability(dev, PCIY_AGP));
}

int drm_device_is_pcie(drm_device_t *dev)
{
	return (drm_device_find_capability(dev, PCIY_EXPRESS));
}

int drm_agp_info(drm_device_t * dev, drm_agp_info_t *info)
{
	struct agp_info *kern;

	if (!dev->agp || !dev->agp->acquired)
		return EINVAL;

	kern                   = &dev->agp->info;
	agp_get_info(dev->agp->agpdev, kern);
	info->agp_version_major = 1;
	info->agp_version_minor = 0;
	info->mode              = kern->ai_mode;
	info->aperture_base     = kern->ai_aperture_base;
	info->aperture_size     = kern->ai_aperture_size;
	info->memory_allowed    = kern->ai_memory_allowed;
	info->memory_used       = kern->ai_memory_used;
	info->id_vendor         = kern->ai_devid & 0xffff;
	info->id_device         = kern->ai_devid >> 16;

	return 0;
}

int drm_agp_info_ioctl(DRM_IOCTL_ARGS)
{
	int err;
	drm_agp_info_t info;
	DRM_DEVICE;

	err = drm_agp_info(dev, &info);
	if (err != 0)
		return err;

	*(drm_agp_info_t *) data = info;
	return 0;
}

int drm_agp_acquire_ioctl(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;

	return drm_agp_acquire(dev);
}

int drm_agp_acquire(drm_device_t *dev)
{
	int retcode;

	if (!dev->agp || dev->agp->acquired)
		return EINVAL;

	retcode = agp_acquire(dev->agp->agpdev);
	if (retcode)
		return retcode;

	dev->agp->acquired = 1;
	return 0;
}

int drm_agp_release_ioctl(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;

	return drm_agp_release(dev);
}

int drm_agp_release(drm_device_t * dev)
{
	if (!dev->agp || !dev->agp->acquired)
		return EINVAL;
	agp_release(dev->agp->agpdev);
	dev->agp->acquired = 0;
	return 0;
}

int drm_agp_enable(drm_device_t *dev, drm_agp_mode_t mode)
{

	if (!dev->agp || !dev->agp->acquired)
		return EINVAL;
	
	dev->agp->mode    = mode.mode;
	agp_enable(dev->agp->agpdev, mode.mode);
	dev->agp->base    = dev->agp->info.ai_aperture_base;
	dev->agp->enabled = 1;
	return 0;
}

int drm_agp_enable_ioctl(DRM_IOCTL_ARGS)
{
	drm_agp_mode_t mode;
	DRM_DEVICE;

	mode = *(drm_agp_mode_t *) data;

	return drm_agp_enable(dev, mode);
}

int drm_agp_alloc(drm_device_t *dev, drm_agp_buffer_t *request)
{
	drm_agp_mem_t    *entry;
	void	         *handle;
	unsigned long    pages;
	u_int32_t	 type;
	struct agp_memory_info info;

	if (!dev->agp || !dev->agp->acquired)
		return EINVAL;

	entry = malloc(sizeof(*entry), M_DRM, M_NOWAIT | M_ZERO);
	if (entry == NULL)
		return ENOMEM;

	pages = (request->size + PAGE_SIZE - 1) / PAGE_SIZE;
	type = (u_int32_t) request->type;

	DRM_UNLOCK();
	handle = drm_agp_allocate_memory(pages, type);
	DRM_LOCK();
	if (handle == NULL) {
		free(entry, M_DRM);
		return ENOMEM;
	}
	
	entry->handle    = handle;
	entry->bound     = 0;
	entry->pages     = pages;
	entry->prev      = NULL;
	entry->next      = dev->agp->memory;
	if (dev->agp->memory)
		dev->agp->memory->prev = entry;
	dev->agp->memory = entry;

	agp_memory_info(dev->agp->agpdev, entry->handle, &info);

	request->handle   = (unsigned long) entry->handle;
        request->physical = info.ami_physical;

	return 0;
}

int drm_agp_alloc_ioctl(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_agp_buffer_t request;
	int retcode;

	request = *(drm_agp_buffer_t *) data;

	DRM_LOCK();
	retcode = drm_agp_alloc(dev, &request);
	DRM_UNLOCK();

	*(drm_agp_buffer_t *) data = request;

	return retcode;
}

static drm_agp_mem_t * drm_agp_lookup_entry(drm_device_t *dev, void *handle)
{
	drm_agp_mem_t *entry;

	for (entry = dev->agp->memory; entry; entry = entry->next) {
		if (entry->handle == handle) return entry;
	}
	return NULL;
}

int drm_agp_unbind(drm_device_t *dev, drm_agp_binding_t *request)
{
	drm_agp_mem_t     *entry;
	int retcode;

	if (!dev->agp || !dev->agp->acquired)
		return EINVAL;

	entry = drm_agp_lookup_entry(dev, (void *)request->handle);
	if (entry == NULL || !entry->bound)
		return EINVAL;

	DRM_UNLOCK();
	retcode = drm_agp_unbind_memory(entry->handle);
	DRM_LOCK();

	if (retcode == 0)
		entry->bound = 0;

	return retcode;
}

int drm_agp_unbind_ioctl(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_agp_binding_t request;
	int retcode;

	request = *(drm_agp_binding_t *) data;

	DRM_LOCK();
	retcode = drm_agp_unbind(dev, &request);
	DRM_UNLOCK();

	return retcode;
}

int drm_agp_bind(drm_device_t *dev, drm_agp_binding_t *request)
{
	drm_agp_mem_t     *entry;
	int               retcode;
	int               page;
	
	if (!dev->agp || !dev->agp->acquired)
		return EINVAL;

	DRM_DEBUG("agp_bind, page_size=%x\n", PAGE_SIZE);

	entry = drm_agp_lookup_entry(dev, (void *)request->handle);
	if (entry == NULL || entry->bound)
		return EINVAL;

	page = (request->offset + PAGE_SIZE - 1) / PAGE_SIZE;

	DRM_UNLOCK();
	retcode = drm_agp_bind_memory(entry->handle, page);
	DRM_LOCK();
	if (retcode == 0)
		entry->bound = dev->agp->base + (page << PAGE_SHIFT);

	return retcode;
}

int drm_agp_bind_ioctl(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_agp_binding_t request;
	int retcode;

	request = *(drm_agp_binding_t *) data;

	DRM_LOCK();
	retcode = drm_agp_bind(dev, &request);
	DRM_UNLOCK();

	return retcode;
}

int drm_agp_free(drm_device_t *dev, drm_agp_buffer_t *request)
{
	drm_agp_mem_t    *entry;
	
	if (!dev->agp || !dev->agp->acquired)
		return EINVAL;

	entry = drm_agp_lookup_entry(dev, (void*)request->handle);
	if (entry == NULL)
		return EINVAL;
   
	if (entry->prev)
		entry->prev->next = entry->next;
	else
		dev->agp->memory  = entry->next;
	if (entry->next)
		entry->next->prev = entry->prev;

	DRM_UNLOCK();
	if (entry->bound)
		drm_agp_unbind_memory(entry->handle);
	drm_agp_free_memory(entry->handle);
	DRM_LOCK();

	free(entry, M_DRM);

	return 0;

}

int drm_agp_free_ioctl(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_agp_buffer_t request;
	int retcode;

	request = *(drm_agp_buffer_t *) data;

	DRM_LOCK();
	retcode = drm_agp_free(dev, &request);
	DRM_UNLOCK();

	return retcode;
}

drm_agp_head_t *drm_agp_init(void)
{
	device_t agpdev;
	drm_agp_head_t *head   = NULL;
	int      agp_available = 1;
   
	agpdev = DRM_AGP_FIND_DEVICE();
	if (!agpdev)
		agp_available = 0;

	DRM_DEBUG("agp_available = %d\n", agp_available);

	if (agp_available) {
		head = malloc(sizeof(*head), M_DRM, M_NOWAIT | M_ZERO);
		if (head == NULL)
			return NULL;
		head->agpdev = agpdev;
		agp_get_info(agpdev, &head->info);
		head->memory = NULL;
		DRM_INFO("AGP at 0x%08lx %dMB\n",
			 (long)head->info.ai_aperture_base,
			 (int)(head->info.ai_aperture_size >> 20));
	}
	return head;
}

void *drm_agp_allocate_memory(size_t pages, u32 type)
{
	device_t agpdev;

	agpdev = DRM_AGP_FIND_DEVICE();
	if (!agpdev)
		return NULL;

	return agp_alloc_memory(agpdev, type, pages << AGP_PAGE_SHIFT);
}

int drm_agp_free_memory(void *handle)
{
	device_t agpdev;

	agpdev = DRM_AGP_FIND_DEVICE();
	if (!agpdev || !handle)
		return 0;

	agp_free_memory(agpdev, handle);
	return 1;
}

int drm_agp_bind_memory(void *handle, off_t start)
{
	device_t agpdev;

	agpdev = DRM_AGP_FIND_DEVICE();
	if (!agpdev || !handle)
		return EINVAL;

	return agp_bind_memory(agpdev, handle, start * PAGE_SIZE);
}

int drm_agp_unbind_memory(void *handle)
{
	device_t agpdev;

	agpdev = DRM_AGP_FIND_DEVICE();
	if (!agpdev || !handle)
		return EINVAL;

	return agp_unbind_memory(agpdev, handle);
}
