/* drm_memory.h -- Memory management wrappers for DRM -*- linux-c -*-
 * Created: Thu Feb  4 14:00:34 1999 by faith@valinux.com
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
 *
 * $FreeBSD$
 */

#define __NO_VERSION__
#ifdef __linux__
#include <linux/config.h>
#endif /* __linux__ */
#include "dev/drm/drmP.h"
#ifdef __linux__
#include <linux/wrapper.h>
#endif /* __linux__ */
#ifdef __FreeBSD__
#include <vm/vm.h>
#include <vm/pmap.h>
#if __REALLY_HAVE_AGP
#include <sys/agpio.h>
#endif

#define malloctype DRM(M_DRM)
/* The macros confliced in the MALLOC_DEFINE */

MALLOC_DEFINE(malloctype, "drm", "DRM Data Structures");

#undef malloctype
#endif /* __FreeBSD__ */

typedef struct drm_mem_stats {
	const char	  *name;
	int		  succeed_count;
	int		  free_count;
	int		  fail_count;
	unsigned long	  bytes_allocated;
	unsigned long	  bytes_freed;
} drm_mem_stats_t;

#ifdef __linux__
static spinlock_t	  DRM(mem_lock)	     = SPIN_LOCK_UNLOCKED;
#endif /* __linux__ */
#ifdef __FreeBSD__
static DRM_OS_SPINTYPE	  DRM(mem_lock);
#endif /* __FreeBSD__ */
static unsigned long	  DRM(ram_available) = 0; /* In pages */
static unsigned long	  DRM(ram_used)      = 0;
static drm_mem_stats_t	  DRM(mem_stats)[]   = {
	[DRM_MEM_DMA]	    = { "dmabufs"  },
	[DRM_MEM_SAREA]	    = { "sareas"   },
	[DRM_MEM_DRIVER]    = { "driver"   },
	[DRM_MEM_MAGIC]	    = { "magic"	   },
	[DRM_MEM_IOCTLS]    = { "ioctltab" },
	[DRM_MEM_MAPS]	    = { "maplist"  },
	[DRM_MEM_VMAS]	    = { "vmalist"  },
	[DRM_MEM_BUFS]	    = { "buflist"  },
	[DRM_MEM_SEGS]	    = { "seglist"  },
	[DRM_MEM_PAGES]	    = { "pagelist" },
	[DRM_MEM_FILES]	    = { "files"	   },
	[DRM_MEM_QUEUES]    = { "queues"   },
	[DRM_MEM_CMDS]	    = { "commands" },
	[DRM_MEM_MAPPINGS]  = { "mappings" },
	[DRM_MEM_BUFLISTS]  = { "buflists" },
	[DRM_MEM_AGPLISTS]  = { "agplist"  },
	[DRM_MEM_SGLISTS]   = { "sglist"   },
	[DRM_MEM_TOTALAGP]  = { "totalagp" },
	[DRM_MEM_BOUNDAGP]  = { "boundagp" },
	[DRM_MEM_CTXBITMAP] = { "ctxbitmap"},
	[DRM_MEM_STUB]      = { "stub"     },
	{ NULL, 0, }		/* Last entry must be null */
};

void DRM(mem_init)(void)
{
	drm_mem_stats_t *mem;
#ifdef __linux__
	struct sysinfo	si;
#endif /* __linux__ */

#ifdef __FreeBSD__
	DRM_OS_SPININIT(DRM(mem_lock), "drm memory");
#endif /* __FreeBSD__ */

	for (mem = DRM(mem_stats); mem->name; ++mem) {
		mem->succeed_count   = 0;
		mem->free_count	     = 0;
		mem->fail_count	     = 0;
		mem->bytes_allocated = 0;
		mem->bytes_freed     = 0;
	}

#ifdef __linux__
	si_meminfo(&si);
	DRM(ram_available) = si.totalram;
#endif /* __linux__ */
#ifdef __FreeBSD__
	DRM(ram_available) = 0; /* si.totalram */
#endif /* __FreeBSD__ */
	DRM(ram_used)	   = 0;
}

#ifdef __FreeBSD__
static int 
DRM(_mem_info)(drm_mem_stats_t *stats, struct sysctl_oid *oidp, void *arg1, 
    int arg2, struct sysctl_req *req)
{
	drm_mem_stats_t *pt;
	char buf[128];
	int error;

	DRM_SYSCTL_PRINT("		  total counts			"
		       " |    outstanding  \n");
	DRM_SYSCTL_PRINT("type	   alloc freed fail	bytes	   freed"
		       " | allocs      bytes\n\n");
	DRM_SYSCTL_PRINT("%-9.9s %5d %5d %4d %10lu	    |\n",
		       "system", 0, 0, 0, DRM(ram_available));
	DRM_SYSCTL_PRINT("%-9.9s %5d %5d %4d %10lu	    |\n",
		       "locked", 0, 0, 0, DRM(ram_used));
	DRM_SYSCTL_PRINT("\n");
	for (pt = stats; pt->name; pt++) {
		DRM_SYSCTL_PRINT("%-9.9s %5d %5d %4d %10lu %10lu | %6d %10ld\n",
			       pt->name,
			       pt->succeed_count,
			       pt->free_count,
			       pt->fail_count,
			       pt->bytes_allocated,
			       pt->bytes_freed,
			       pt->succeed_count - pt->free_count,
			       (long)pt->bytes_allocated
			       - (long)pt->bytes_freed);
	}
	SYSCTL_OUT(req, "", 1);
	
	return 0;
}

int DRM(mem_info) DRM_SYSCTL_HANDLER_ARGS
{
	int ret;
	drm_mem_stats_t *stats;
	
	stats = malloc(sizeof(DRM(mem_stats)), DRM(M_DRM), M_NOWAIT);
	if (stats == NULL)
		return ENOMEM;
	
	DRM_OS_SPINLOCK(&DRM(mem_lock));
	bcopy(DRM(mem_stats), stats, sizeof(DRM(mem_stats)));
	DRM_OS_SPINUNLOCK(&DRM(mem_lock));
	
	ret = DRM(_mem_info)(stats, oidp, arg1, arg2, req);
	
	free(stats, DRM(M_DRM));
	return ret;
}
#endif /* __FreeBSD__ */

void *DRM(alloc)(size_t size, int area)
{
	void *pt;

	if (!size) {
		DRM_MEM_ERROR(area, "Allocating 0 bytes\n");
		return NULL;
	}

#ifdef __linux__
	if (!(pt = kmalloc(size, GFP_KERNEL))) {
#endif /* __linux__ */
#ifdef __FreeBSD__
	if (!(pt = malloc(size, DRM(M_DRM), M_NOWAIT))) {
#endif /* __FreeBSD__ */
		DRM_OS_SPINLOCK(&DRM(mem_lock));
		++DRM(mem_stats)[area].fail_count;
		DRM_OS_SPINUNLOCK(&DRM(mem_lock));
		return NULL;
	}
	DRM_OS_SPINLOCK(&DRM(mem_lock));
	++DRM(mem_stats)[area].succeed_count;
	DRM(mem_stats)[area].bytes_allocated += size;
	DRM_OS_SPINUNLOCK(&DRM(mem_lock));
	return pt;
}

void *DRM(realloc)(void *oldpt, size_t oldsize, size_t size, int area)
{
	void *pt;

	if (!(pt = DRM(alloc)(size, area))) return NULL;
	if (oldpt && oldsize) {
		memcpy(pt, oldpt, oldsize);
		DRM(free)(oldpt, oldsize, area);
	}
	return pt;
}

char *DRM(strdup)(const char *s, int area)
{
	char *pt;
	int	 length = s ? strlen(s) : 0;

	if (!(pt = DRM(alloc)(length+1, area))) return NULL;
	strcpy(pt, s);
	return pt;
}

void DRM(strfree)(char *s, int area)
{
	unsigned int size;

	if (!s) return;

	size = 1 + strlen(s);
	DRM(free)((void *)s, size, area);
}

void DRM(free)(void *pt, size_t size, int area)
{
	int alloc_count;
	int free_count;

	if (!pt) DRM_MEM_ERROR(area, "Attempt to free NULL pointer\n");
#ifdef __linux__
	else	 kfree(pt);
#endif /* __linux__ */
#ifdef __FreeBSD__
	else     free(pt, DRM(M_DRM));
#endif /* __FreeBSD__ */
	DRM_OS_SPINLOCK(&DRM(mem_lock));
	DRM(mem_stats)[area].bytes_freed += size;
	free_count  = ++DRM(mem_stats)[area].free_count;
	alloc_count =	DRM(mem_stats)[area].succeed_count;
	DRM_OS_SPINUNLOCK(&DRM(mem_lock));
	if (free_count > alloc_count) {
		DRM_MEM_ERROR(area, "Excess frees: %d frees, %d allocs\n",
			      free_count, alloc_count);
	}
}

unsigned long DRM(alloc_pages)(int order, int area)
{
#ifdef __linux__
	unsigned long address;
	unsigned long addr;
	unsigned int  sz;
#endif /* __linux__ */
#ifdef __FreeBSD__
	vm_offset_t address;
#endif /* __FreeBSD__ */
	unsigned long bytes	  = PAGE_SIZE << order;

#ifdef __linux__
	DRM_OS_SPINLOCK(&DRM(mem_lock));
	if ((DRM(ram_used) >> PAGE_SHIFT)
	    > (DRM_RAM_PERCENT * DRM(ram_available)) / 100) {
		DRM_OS_SPINUNLOCK(&DRM(mem_lock));
		return 0;
	}
	DRM_OS_SPINUNLOCK(&DRM(mem_lock));
#endif /* __linux__ */

#ifdef __linux__
	address = __get_free_pages(GFP_KERNEL, order);
#endif /* __linux__ */
#ifdef __FreeBSD__
	address = (vm_offset_t) contigmalloc(bytes, DRM(M_DRM), M_WAITOK, 0, ~0, 1, 0);
#endif /* __FreeBSD__ */
	if (!address) {
		DRM_OS_SPINLOCK(&DRM(mem_lock));
		++DRM(mem_stats)[area].fail_count;
		DRM_OS_SPINUNLOCK(&DRM(mem_lock));
		return 0;
	}
	DRM_OS_SPINLOCK(&DRM(mem_lock));
	++DRM(mem_stats)[area].succeed_count;
	DRM(mem_stats)[area].bytes_allocated += bytes;
	DRM(ram_used)		             += bytes;
	DRM_OS_SPINUNLOCK(&DRM(mem_lock));


				/* Zero outside the lock */
	memset((void *)address, 0, bytes);

#ifdef __linux__
				/* Reserve */
	for (addr = address, sz = bytes;
	     sz > 0;
	     addr += PAGE_SIZE, sz -= PAGE_SIZE) {
		mem_map_reserve(virt_to_page(addr));
	}
#endif /* __linux__ */

	return address;
}

void DRM(free_pages)(unsigned long address, int order, int area)
{
	unsigned long bytes = PAGE_SIZE << order;
	int		  alloc_count;
	int		  free_count;

	if (!address) {
		DRM_MEM_ERROR(area, "Attempt to free address 0\n");
	} else {
#ifdef __linux__
		unsigned long addr;
		unsigned int  sz;
				/* Unreserve */
		for (addr = address, sz = bytes;
		     sz > 0;
		     addr += PAGE_SIZE, sz -= PAGE_SIZE) {
			mem_map_unreserve(virt_to_page(addr));
		}
		free_pages(address, order);
#endif /* __linux__ */
#ifdef __FreeBSD__
		contigfree((void *) address, bytes, DRM(M_DRM));
#endif /* __FreeBSD__ */
	}

	DRM_OS_SPINLOCK(&DRM(mem_lock));
	free_count  = ++DRM(mem_stats)[area].free_count;
	alloc_count =	DRM(mem_stats)[area].succeed_count;
	DRM(mem_stats)[area].bytes_freed += bytes;
	DRM(ram_used)			 -= bytes;
	DRM_OS_SPINUNLOCK(&DRM(mem_lock));
	if (free_count > alloc_count) {
		DRM_MEM_ERROR(area,
			      "Excess frees: %d frees, %d allocs\n",
			      free_count, alloc_count);
	}
}

void *DRM(ioremap)(unsigned long offset, unsigned long size)
{
	void *pt;

	if (!size) {
		DRM_MEM_ERROR(DRM_MEM_MAPPINGS,
			      "Mapping 0 bytes at 0x%08lx\n", offset);
		return NULL;
	}

#ifdef __linux__
	if (!(pt = ioremap(offset, size))) {
#endif /* __linux__ */
#ifdef __FreeBSD__
	if (!(pt = pmap_mapdev(offset, size))) {
#endif /* __FreeBSD__ */
		DRM_OS_SPINLOCK(&DRM(mem_lock));
		++DRM(mem_stats)[DRM_MEM_MAPPINGS].fail_count;
		DRM_OS_SPINUNLOCK(&DRM(mem_lock));
		return NULL;
	}
	DRM_OS_SPINLOCK(&DRM(mem_lock));
	++DRM(mem_stats)[DRM_MEM_MAPPINGS].succeed_count;
	DRM(mem_stats)[DRM_MEM_MAPPINGS].bytes_allocated += size;
	DRM_OS_SPINUNLOCK(&DRM(mem_lock));
	return pt;
}

void DRM(ioremapfree)(void *pt, unsigned long size)
{
	int alloc_count;
	int free_count;

	if (!pt)
		DRM_MEM_ERROR(DRM_MEM_MAPPINGS,
			      "Attempt to free NULL pointer\n");
	else
#ifdef __linux__
		iounmap(pt);
#endif /* __linux__ */
#ifdef __FreeBSD__
		pmap_unmapdev((vm_offset_t) pt, size);
#endif /* __FreeBSD__ */

	DRM_OS_SPINLOCK(&DRM(mem_lock));
	DRM(mem_stats)[DRM_MEM_MAPPINGS].bytes_freed += size;
	free_count  = ++DRM(mem_stats)[DRM_MEM_MAPPINGS].free_count;
	alloc_count =	DRM(mem_stats)[DRM_MEM_MAPPINGS].succeed_count;
	DRM_OS_SPINUNLOCK(&DRM(mem_lock));
	if (free_count > alloc_count) {
		DRM_MEM_ERROR(DRM_MEM_MAPPINGS,
			      "Excess frees: %d frees, %d allocs\n",
			      free_count, alloc_count);
	}
}

#if __REALLY_HAVE_AGP
agp_memory *DRM(alloc_agp)(int pages, u32 type)
{
	agp_memory *handle;

	if (!pages) {
		DRM_MEM_ERROR(DRM_MEM_TOTALAGP, "Allocating 0 pages\n");
		return NULL;
	}

	if ((handle = DRM(agp_allocate_memory)(pages, type))) {
		DRM_OS_SPINLOCK(&DRM(mem_lock));
		++DRM(mem_stats)[DRM_MEM_TOTALAGP].succeed_count;
		DRM(mem_stats)[DRM_MEM_TOTALAGP].bytes_allocated
			+= pages << PAGE_SHIFT;
		DRM_OS_SPINUNLOCK(&DRM(mem_lock));
		return handle;
	}
	DRM_OS_SPINLOCK(&DRM(mem_lock));
	++DRM(mem_stats)[DRM_MEM_TOTALAGP].fail_count;
	DRM_OS_SPINUNLOCK(&DRM(mem_lock));
	return NULL;
}

int DRM(free_agp)(agp_memory *handle, int pages)
{
	int           alloc_count;
	int           free_count;

	if (!handle) {
		DRM_MEM_ERROR(DRM_MEM_TOTALAGP,
			      "Attempt to free NULL AGP handle\n");
		return DRM_OS_ERR(EINVAL);
	}

	if (DRM(agp_free_memory)(handle)) {
		DRM_OS_SPINLOCK(&DRM(mem_lock));
		free_count  = ++DRM(mem_stats)[DRM_MEM_TOTALAGP].free_count;
		alloc_count =   DRM(mem_stats)[DRM_MEM_TOTALAGP].succeed_count;
		DRM(mem_stats)[DRM_MEM_TOTALAGP].bytes_freed
			+= pages << PAGE_SHIFT;
		DRM_OS_SPINUNLOCK(&DRM(mem_lock));
		if (free_count > alloc_count) {
			DRM_MEM_ERROR(DRM_MEM_TOTALAGP,
				      "Excess frees: %d frees, %d allocs\n",
				      free_count, alloc_count);
		}
		return 0;
	}
	return DRM_OS_ERR(EINVAL);
}

int DRM(bind_agp)(agp_memory *handle, unsigned int start)
{
	int retcode;
#ifdef __FreeBSD__
	device_t dev = agp_find_device();
	struct agp_memory_info info;

	if (!dev)
		return DRM_OS_ERR(EINVAL);
#endif /* __FreeBSD__ */

	if (!handle) {
		DRM_MEM_ERROR(DRM_MEM_BOUNDAGP,
			      "Attempt to bind NULL AGP handle\n");
		return DRM_OS_ERR(EINVAL);
	}

	if (!(retcode = DRM(agp_bind_memory)(handle, start))) {
		DRM_OS_SPINLOCK(&DRM(mem_lock));
		++DRM(mem_stats)[DRM_MEM_BOUNDAGP].succeed_count;
#ifdef __linux__
		DRM(mem_stats)[DRM_MEM_BOUNDAGP].bytes_allocated
			+= handle->page_count << PAGE_SHIFT;
#endif /* __linux__ */
#ifdef __FreeBSD__
		agp_memory_info(dev, handle, &info);
		DRM(mem_stats)[DRM_MEM_BOUNDAGP].bytes_allocated
			+= info.ami_size;
#endif /* __FreeBSD__ */
		DRM_OS_SPINUNLOCK(&DRM(mem_lock));
		return 0;
	}
	DRM_OS_SPINLOCK(&DRM(mem_lock));
	++DRM(mem_stats)[DRM_MEM_BOUNDAGP].fail_count;
	DRM_OS_SPINUNLOCK(&DRM(mem_lock));
	return retcode;
}

int DRM(unbind_agp)(agp_memory *handle)
{
	int alloc_count;
	int free_count;
	int retcode = DRM_OS_ERR(EINVAL);
#ifdef __FreeBSD__
	device_t dev = agp_find_device();
	struct agp_memory_info info;

	if (!dev)
		return DRM_OS_ERR(EINVAL);
#endif /* __FreeBSD__ */

	if (!handle) {
		DRM_MEM_ERROR(DRM_MEM_BOUNDAGP,
			      "Attempt to unbind NULL AGP handle\n");
		return retcode;
	}

#ifdef __FreeBSD__
	agp_memory_info(dev, handle, &info);
#endif /* __FreeBSD__ */

	if ((retcode = DRM(agp_unbind_memory)(handle))) 
		return retcode;

	DRM_OS_SPINLOCK(&DRM(mem_lock));
	free_count  = ++DRM(mem_stats)[DRM_MEM_BOUNDAGP].free_count;
	alloc_count = DRM(mem_stats)[DRM_MEM_BOUNDAGP].succeed_count;
#ifdef __linux__
	DRM(mem_stats)[DRM_MEM_BOUNDAGP].bytes_freed
		+= handle->page_count << PAGE_SHIFT;
#endif /* __linux__ */
#ifdef __FreeBSD__
	DRM(mem_stats)[DRM_MEM_BOUNDAGP].bytes_freed
		+= info.ami_size;
#endif /* __FreeBSD__ */
	DRM_OS_SPINUNLOCK(&DRM(mem_lock));
	if (free_count > alloc_count) {
		DRM_MEM_ERROR(DRM_MEM_BOUNDAGP,
			      "Excess frees: %d frees, %d allocs\n",
			      free_count, alloc_count);
	}
	return retcode;
}
#endif
