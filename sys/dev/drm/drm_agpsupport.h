/* drm_agpsupport.h -- DRM support for AGP/GART backend -*- linux-c -*-
 * Created: Mon Dec 13 09:56:45 1999 by faith@precisioninsight.com
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
 * Author:
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 *
 * $FreeBSD$
 */

#include "dev/drm/drmP.h"

int DRM(agp_info)(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	struct agp_info *kern;
	drm_agp_info_t   info;

	if (!dev->agp || !dev->agp->acquired)
		return EINVAL;

	kern                   = &dev->agp->info;
	agp_get_info(dev->agp->agpdev, kern);
	info.agp_version_major = 1;
	info.agp_version_minor = 0;
	info.mode              = kern->ai_mode;
	info.aperture_base     = kern->ai_aperture_base;
	info.aperture_size     = kern->ai_aperture_size;
	info.memory_allowed    = kern->ai_memory_allowed;
	info.memory_used       = kern->ai_memory_used;
	info.id_vendor         = kern->ai_devid & 0xffff;
	info.id_device         = kern->ai_devid >> 16;

	*(drm_agp_info_t *) data = info;
	return 0;
}

int DRM(agp_acquire)(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	int          retcode;

	if (!dev->agp || dev->agp->acquired)
		return EINVAL;
	retcode = agp_acquire(dev->agp->agpdev);
	if (retcode)
		return retcode;
	dev->agp->acquired = 1;
	return 0;
}

int DRM(agp_release)(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;

	if (!dev->agp || !dev->agp->acquired)
		return EINVAL;
	agp_release(dev->agp->agpdev);
	dev->agp->acquired = 0;
	return 0;
	
}

void DRM(agp_do_release)(void)
{
	device_t agpdev;

	agpdev = DRM_AGP_FIND_DEVICE();
	if (agpdev)
		agp_release(agpdev);
}

int DRM(agp_enable)(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_agp_mode_t mode;

	if (!dev->agp || !dev->agp->acquired)
		return EINVAL;

	mode = *(drm_agp_mode_t *) data;
	
	dev->agp->mode    = mode.mode;
	agp_enable(dev->agp->agpdev, mode.mode);
	dev->agp->base    = dev->agp->info.ai_aperture_base;
	dev->agp->enabled = 1;
	return 0;
}

int DRM(agp_alloc)(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_agp_buffer_t request;
	drm_agp_mem_t    *entry;
	void	         *handle;
	unsigned long    pages;
	u_int32_t	 type;
	struct agp_memory_info info;

	if (!dev->agp || !dev->agp->acquired)
		return EINVAL;

	request = *(drm_agp_buffer_t *) data;

	if (!(entry = DRM(alloc)(sizeof(*entry), DRM_MEM_AGPLISTS)))
		return ENOMEM;
   
   	bzero(entry, sizeof(*entry));

	pages = (request.size + PAGE_SIZE - 1) / PAGE_SIZE;
	type = (u_int32_t) request.type;

	if (!(handle = DRM(alloc_agp)(pages, type))) {
		DRM(free)(entry, sizeof(*entry), DRM_MEM_AGPLISTS);
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

	request.handle   = (unsigned long) entry->handle;
        request.physical = info.ami_physical;

	*(drm_agp_buffer_t *) data = request;

	return 0;
}

static drm_agp_mem_t * DRM(agp_lookup_entry)(drm_device_t *dev, void *handle)
{
	drm_agp_mem_t *entry;

	for (entry = dev->agp->memory; entry; entry = entry->next) {
		if (entry->handle == handle) return entry;
	}
	return NULL;
}

int DRM(agp_unbind)(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_agp_binding_t request;
	drm_agp_mem_t     *entry;
	int retcode;

	if (!dev->agp || !dev->agp->acquired)
		return EINVAL;
	request = *(drm_agp_binding_t *) data;
	if (!(entry = DRM(agp_lookup_entry)(dev, (void *) request.handle)))
		return EINVAL;
	if (!entry->bound) return EINVAL;
	retcode=DRM(unbind_agp)(entry->handle);
	if (!retcode)
	{
		entry->bound=0;
		return 0;
	}
	else
		return retcode;
}

int DRM(agp_bind)(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_agp_binding_t request;
	drm_agp_mem_t     *entry;
	int               retcode;
	int               page;
	
	DRM_DEBUG("agp_bind, page_size=%x\n", PAGE_SIZE);
	if (!dev->agp || !dev->agp->acquired)
		return EINVAL;
	request = *(drm_agp_binding_t *) data;
	if (!(entry = DRM(agp_lookup_entry)(dev, (void *) request.handle)))
		return EINVAL;
	if (entry->bound) return EINVAL;
	page = (request.offset + PAGE_SIZE - 1) / PAGE_SIZE;
	if ((retcode = DRM(bind_agp)(entry->handle, page)))
		return retcode;
	entry->bound = dev->agp->base + (page << PAGE_SHIFT);
	return 0;
}

int DRM(agp_free)(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_agp_buffer_t request;
	drm_agp_mem_t    *entry;
	
	if (!dev->agp || !dev->agp->acquired)
		return EINVAL;
	request = *(drm_agp_buffer_t *) data;
	if (!(entry = DRM(agp_lookup_entry)(dev, (void*) request.handle)))
		return EINVAL;
	if (entry->bound)
		DRM(unbind_agp)(entry->handle);
   
	if (entry->prev)
		entry->prev->next = entry->next;
	else
		dev->agp->memory  = entry->next;
	if (entry->next)
		entry->next->prev = entry->prev;
	DRM(free_agp)(entry->handle, entry->pages);
	DRM(free)(entry, sizeof(*entry), DRM_MEM_AGPLISTS);
	return 0;
}

drm_agp_head_t *DRM(agp_init)(void)
{
	device_t agpdev;
	drm_agp_head_t *head   = NULL;
	int      agp_available = 1;
   
	agpdev = DRM_AGP_FIND_DEVICE();
	if (!agpdev)
		agp_available = 0;

	DRM_DEBUG("agp_available = %d\n", agp_available);

	if (agp_available) {
		if (!(head = DRM(alloc)(sizeof(*head), DRM_MEM_AGPLISTS)))
			return NULL;
		bzero((void *)head, sizeof(*head));
		head->agpdev = agpdev;
		agp_get_info(agpdev, &head->info);
		head->memory = NULL;
		DRM_INFO("AGP at 0x%08lx %dMB\n",
			 (long)head->info.ai_aperture_base,
			 (int)(head->info.ai_aperture_size >> 20));
	}
	return head;
}

void DRM(agp_uninit)(void)
{
/* FIXME: What goes here */
}


agp_memory *DRM(agp_allocate_memory)(size_t pages, u32 type)
{
	device_t agpdev;

	agpdev = DRM_AGP_FIND_DEVICE();
	if (!agpdev)
		return NULL;

	return agp_alloc_memory(agpdev, type, pages << AGP_PAGE_SHIFT);
}

int DRM(agp_free_memory)(agp_memory *handle)
{
	device_t agpdev;

	agpdev = DRM_AGP_FIND_DEVICE();
	if (!agpdev || !handle)
		return 0;

	agp_free_memory(agpdev, handle);
	return 1;
}

int DRM(agp_bind_memory)(agp_memory *handle, off_t start)
{
	device_t agpdev;

	agpdev = DRM_AGP_FIND_DEVICE();
	if (!agpdev || !handle)
		return EINVAL;

	return agp_bind_memory(agpdev, handle, start * PAGE_SIZE);
}

int DRM(agp_unbind_memory)(agp_memory *handle)
{
	device_t agpdev;

	agpdev = DRM_AGP_FIND_DEVICE();
	if (!agpdev || !handle)
		return EINVAL;

	return agp_unbind_memory(agpdev, handle);
}
