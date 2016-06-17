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
 */

#include "drmP.h"
#include <linux/module.h>

#if __REALLY_HAVE_AGP

#define DRM_AGP_GET (drm_agp_t *)inter_module_get("drm_agp")
#define DRM_AGP_PUT inter_module_put("drm_agp")

static const drm_agp_t *drm_agp = NULL;

int DRM(agp_info)(struct inode *inode, struct file *filp,
		  unsigned int cmd, unsigned long arg)
{
	drm_file_t	 *priv	 = filp->private_data;
	drm_device_t	 *dev	 = priv->dev;
	agp_kern_info    *kern;
	drm_agp_info_t   info;

	if (!dev->agp || !dev->agp->acquired || !drm_agp->copy_info)
		return -EINVAL;

	kern                   = &dev->agp->agp_info;
	info.agp_version_major = kern->version.major;
	info.agp_version_minor = kern->version.minor;
	info.mode              = kern->mode;
	info.aperture_base     = kern->aper_base;
	info.aperture_size     = kern->aper_size * 1024 * 1024;
	info.memory_allowed    = kern->max_memory << PAGE_SHIFT;
	info.memory_used       = kern->current_memory << PAGE_SHIFT;
	info.id_vendor         = kern->device->vendor;
	info.id_device         = kern->device->device;

	if (copy_to_user((drm_agp_info_t *)arg, &info, sizeof(info)))
		return -EFAULT;
	return 0;
}

int DRM(agp_acquire)(struct inode *inode, struct file *filp,
		     unsigned int cmd, unsigned long arg)
{
	drm_file_t	 *priv	 = filp->private_data;
	drm_device_t	 *dev	 = priv->dev;
	int              retcode;

	if (!dev->agp)
		return -ENODEV;
	if (dev->agp->acquired)
		return -EBUSY;
	if(!drm_agp->acquire)
		return -EINVAL;
	if ((retcode = drm_agp->acquire()))
		return retcode;
	dev->agp->acquired = 1;
	return 0;
}

int DRM(agp_release)(struct inode *inode, struct file *filp,
		     unsigned int cmd, unsigned long arg)
{
	drm_file_t	 *priv	 = filp->private_data;
	drm_device_t	 *dev	 = priv->dev;

	if (!dev->agp || !dev->agp->acquired || !drm_agp->release)
		return -EINVAL;
	drm_agp->release();
	dev->agp->acquired = 0;
	return 0;

}

void DRM(agp_do_release)(void)
{
	if (drm_agp->release) drm_agp->release();
}

int DRM(agp_enable)(struct inode *inode, struct file *filp,
		    unsigned int cmd, unsigned long arg)
{
	drm_file_t	 *priv	 = filp->private_data;
	drm_device_t	 *dev	 = priv->dev;
	drm_agp_mode_t   mode;

	if (!dev->agp || !dev->agp->acquired || !drm_agp->enable)
		return -EINVAL;

	if (copy_from_user(&mode, (drm_agp_mode_t *)arg, sizeof(mode)))
		return -EFAULT;

	dev->agp->mode    = mode.mode;
	drm_agp->enable(mode.mode);
	dev->agp->base    = dev->agp->agp_info.aper_base;
	dev->agp->enabled = 1;
	return 0;
}

int DRM(agp_alloc)(struct inode *inode, struct file *filp,
		   unsigned int cmd, unsigned long arg)
{
	drm_file_t	 *priv	 = filp->private_data;
	drm_device_t	 *dev	 = priv->dev;
	drm_agp_buffer_t request;
	drm_agp_mem_t    *entry;
	agp_memory       *memory;
	unsigned long    pages;
	u32 		 type;

	if (!dev->agp || !dev->agp->acquired) return -EINVAL;
	if (copy_from_user(&request, (drm_agp_buffer_t *)arg, sizeof(request)))
		return -EFAULT;
	if (!(entry = DRM(alloc)(sizeof(*entry), DRM_MEM_AGPLISTS)))
		return -ENOMEM;

   	memset(entry, 0, sizeof(*entry));

	pages = (request.size + PAGE_SIZE - 1) / PAGE_SIZE;
	type = (u32) request.type;

	if (!(memory = DRM(alloc_agp)(pages, type))) {
		DRM(free)(entry, sizeof(*entry), DRM_MEM_AGPLISTS);
		return -ENOMEM;
	}

	entry->handle    = (unsigned long)memory->memory;
	entry->memory    = memory;
	entry->bound     = 0;
	entry->pages     = pages;
	entry->prev      = NULL;
	entry->next      = dev->agp->memory;
	if (dev->agp->memory) dev->agp->memory->prev = entry;
	dev->agp->memory = entry;

	request.handle   = entry->handle;
        request.physical = memory->physical;

	if (copy_to_user((drm_agp_buffer_t *)arg, &request, sizeof(request))) {
		dev->agp->memory       = entry->next;
		dev->agp->memory->prev = NULL;
		DRM(free_agp)(memory, pages);
		DRM(free)(entry, sizeof(*entry), DRM_MEM_AGPLISTS);
		return -EFAULT;
	}
	return 0;
}

static drm_agp_mem_t *DRM(agp_lookup_entry)(drm_device_t *dev,
					    unsigned long handle)
{
	drm_agp_mem_t *entry;

	for (entry = dev->agp->memory; entry; entry = entry->next) {
		if (entry->handle == handle) return entry;
	}
	return NULL;
}

int DRM(agp_unbind)(struct inode *inode, struct file *filp,
		    unsigned int cmd, unsigned long arg)
{
	drm_file_t	  *priv	 = filp->private_data;
	drm_device_t	  *dev	 = priv->dev;
	drm_agp_binding_t request;
	drm_agp_mem_t     *entry;

	if (!dev->agp || !dev->agp->acquired) return -EINVAL;
	if (copy_from_user(&request, (drm_agp_binding_t *)arg, sizeof(request)))
		return -EFAULT;
	if (!(entry = DRM(agp_lookup_entry)(dev, request.handle)))
		return -EINVAL;
	if (!entry->bound) return -EINVAL;
	return DRM(unbind_agp)(entry->memory);
}

int DRM(agp_bind)(struct inode *inode, struct file *filp,
		  unsigned int cmd, unsigned long arg)
{
	drm_file_t	  *priv	 = filp->private_data;
	drm_device_t	  *dev	 = priv->dev;
	drm_agp_binding_t request;
	drm_agp_mem_t     *entry;
	int               retcode;
	int               page;

	if (!dev->agp || !dev->agp->acquired || !drm_agp->bind_memory)
		return -EINVAL;
	if (copy_from_user(&request, (drm_agp_binding_t *)arg, sizeof(request)))
		return -EFAULT;
	if (!(entry = DRM(agp_lookup_entry)(dev, request.handle)))
		return -EINVAL;
	if (entry->bound) return -EINVAL;
	page = (request.offset + PAGE_SIZE - 1) / PAGE_SIZE;
	if ((retcode = DRM(bind_agp)(entry->memory, page))) return retcode;
	entry->bound = dev->agp->base + (page << PAGE_SHIFT);
	DRM_DEBUG("base = 0x%lx entry->bound = 0x%lx\n",
		  dev->agp->base, entry->bound);
	return 0;
}

int DRM(agp_free)(struct inode *inode, struct file *filp,
		  unsigned int cmd, unsigned long arg)
{
	drm_file_t	 *priv	 = filp->private_data;
	drm_device_t	 *dev	 = priv->dev;
	drm_agp_buffer_t request;
	drm_agp_mem_t    *entry;

	if (!dev->agp || !dev->agp->acquired) return -EINVAL;
	if (copy_from_user(&request, (drm_agp_buffer_t *)arg, sizeof(request)))
		return -EFAULT;
	if (!(entry = DRM(agp_lookup_entry)(dev, request.handle)))
		return -EINVAL;
	if (entry->bound) DRM(unbind_agp)(entry->memory);

	if (entry->prev) entry->prev->next = entry->next;
	else             dev->agp->memory  = entry->next;
	if (entry->next) entry->next->prev = entry->prev;
	DRM(free_agp)(entry->memory, entry->pages);
	DRM(free)(entry, sizeof(*entry), DRM_MEM_AGPLISTS);
	return 0;
}

drm_agp_head_t *DRM(agp_init)(void)
{
	drm_agp_head_t *head         = NULL;

	drm_agp = DRM_AGP_GET;
	if (drm_agp) {
		if (!(head = DRM(alloc)(sizeof(*head), DRM_MEM_AGPLISTS)))
			return NULL;
		memset((void *)head, 0, sizeof(*head));
		drm_agp->copy_info(&head->agp_info);
		if (head->agp_info.chipset == NOT_SUPPORTED) {
			DRM(free)(head, sizeof(*head), DRM_MEM_AGPLISTS);
			return NULL;
		}
		head->memory = NULL;

		head->cant_use_aperture = head->agp_info.cant_use_aperture;
		head->page_mask = head->agp_info.page_mask;

		DRM_INFO("AGP %d.%d Aperture @ 0x%08lx %ZuMB\n",
			 head->agp_info.version.major,
			 head->agp_info.version.minor,
			 head->agp_info.aper_base,
			 head->agp_info.aper_size);
	}
	return head;
}

void DRM(agp_uninit)(void)
{
	DRM_AGP_PUT;
	drm_agp = NULL;
}

agp_memory *DRM(agp_allocate_memory)(size_t pages, u32 type)
{
	if (!drm_agp->allocate_memory) return NULL;
	return drm_agp->allocate_memory(pages, type);
}

int DRM(agp_free_memory)(agp_memory *handle)
{
	if (!handle || !drm_agp->free_memory) return 0;
	drm_agp->free_memory(handle);
	return 1;
}

int DRM(agp_bind_memory)(agp_memory *handle, off_t start)
{
	if (!handle || !drm_agp->bind_memory) return -EINVAL;
	return drm_agp->bind_memory(handle, start);
}

int DRM(agp_unbind_memory)(agp_memory *handle)
{
	if (!handle || !drm_agp->unbind_memory) return -EINVAL;
	return drm_agp->unbind_memory(handle);
}

#endif /* __REALLY_HAVE_AGP */
