/* drm_memory.h -- Memory management wrappers for DRM -*- linux-c -*-
 * Created: Thu Feb  4 14:00:34 1999 by faith@valinux.com */
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

#if defined(__FreeBSD__) || defined(__NetBSD__)
#define malloctype DRM(M_DRM)
/* The macros conflicted in the MALLOC_DEFINE */
MALLOC_DEFINE(malloctype, "drm", "DRM Data Structures");
#undef malloctype
#endif

#ifdef DEBUG_MEMORY
#include "drm_memory_debug.h"
#else
void DRM(mem_init)(void)
{
#ifdef __NetBSD__
	malloc_type_attach(DRM(M_DRM));
#endif
}

void DRM(mem_uninit)(void)
{
}

void *DRM(alloc)(size_t size, int area)
{
	return malloc(size, DRM(M_DRM), M_NOWAIT);
}

void *DRM(calloc)(size_t nmemb, size_t size, int area)
{
	return malloc(size * nmemb, DRM(M_DRM), M_NOWAIT | M_ZERO);
}

void *DRM(realloc)(void *oldpt, size_t oldsize, size_t size, int area)
{
	void *pt;

	pt = malloc(size, DRM(M_DRM), M_NOWAIT);
	if (pt == NULL)
		return NULL;
	if (oldpt && oldsize) {
		memcpy(pt, oldpt, oldsize);
		free(oldpt, DRM(M_DRM));
	}
	return pt;
}

void DRM(free)(void *pt, size_t size, int area)
{
	free(pt, DRM(M_DRM));
}

void *DRM(ioremap)( drm_device_t *dev, drm_local_map_t *map )
{
#ifdef __FreeBSD__
	return pmap_mapdev(map->offset, map->size);
#elif defined(__NetBSD__)
	map->iot = dev->pa.pa_memt;
	if (bus_space_map(map->iot, map->offset, map->size, 
	    BUS_SPACE_MAP_LINEAR, &map->ioh))
		return NULL;
	return bus_space_vaddr(map->iot, map->ioh);
#endif
}

void DRM(ioremapfree)(drm_local_map_t *map)
{
#ifdef __FreeBSD__
	pmap_unmapdev((vm_offset_t) map->handle, map->size);
#elif defined(__NetBSD__)
	bus_space_unmap(map->iot, map->ioh, map->size);
#endif
}

#if __REALLY_HAVE_AGP
agp_memory *DRM(alloc_agp)(int pages, u32 type)
{
	return DRM(agp_allocate_memory)(pages, type);
}

int DRM(free_agp)(agp_memory *handle, int pages)
{
	return DRM(agp_free_memory)(handle);
}

int DRM(bind_agp)(agp_memory *handle, unsigned int start)
{
	return DRM(agp_bind_memory)(handle, start);
}

int DRM(unbind_agp)(agp_memory *handle)
{
	return DRM(agp_unbind_memory)(handle);
}
#endif /* __REALLY_HAVE_AGP */

#ifdef __FreeBSD__
int
DRM(mtrr_add)(unsigned long offset, size_t size, int flags)
{
	int act;
	struct mem_range_desc mrdesc;

	mrdesc.mr_base = offset;
	mrdesc.mr_len = size;
	mrdesc.mr_flags = flags;
	act = MEMRANGE_SET_UPDATE;
	bcopy(DRIVER_NAME, &mrdesc.mr_owner, strlen(DRIVER_NAME));
	return mem_range_attr_set(&mrdesc, &act);
}

int
DRM(mtrr_del)(unsigned long offset, size_t size, int flags)
{
	int act;
	struct mem_range_desc mrdesc;

	mrdesc.mr_base = offset;
	mrdesc.mr_len = size;
	mrdesc.mr_flags = flags;
	act = MEMRANGE_SET_REMOVE;
	bcopy(DRIVER_NAME, &mrdesc.mr_owner, strlen(DRIVER_NAME));
	return mem_range_attr_set(&mrdesc, &act);
}
#elif defined(__NetBSD__)
int
DRM(mtrr_add)(unsigned long offset, size_t size, int flags)
{
	struct mtrr mtrrmap;
	int one = 1;

	mtrrmap.base = offset;
	mtrrmap.len = size;
	mtrrmap.type = flags;
	mtrrmap.flags = MTRR_VALID;
	return mtrr_set(&mtrrmap, &one, NULL, MTRR_GETSET_KERNEL);
}

int
DRM(mtrr_del)(unsigned long offset, size_t size, int flags)
{
	struct mtrr mtrrmap;
	int one = 1;

	mtrrmap.base = offset;
	mtrrmap.len = size;
	mtrrmap.type = flags;
	mtrrmap.flags = 0;
	return mtrr_set(&mtrrmap, &one, NULL, MTRR_GETSET_KERNEL);
}
#endif

#endif /* DEBUG_MEMORY */
