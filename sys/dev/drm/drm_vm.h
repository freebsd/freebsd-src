/*-
 * Copyright 2003 Eric Anholt
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
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 *
 * $FreeBSD$
 */

#if defined(__FreeBSD__) && __FreeBSD_version >= 500102
static int DRM(dma_mmap)(struct cdev *kdev, vm_offset_t offset, vm_paddr_t *paddr, 
    int prot)
#elif defined(__FreeBSD__)
static int DRM(dma_mmap)(dev_t kdev, vm_offset_t offset, int prot)
#elif defined(__NetBSD__)
static paddr_t DRM(dma_mmap)(dev_t kdev, vm_offset_t offset, int prot)
#endif
{
	DRM_DEVICE;
	drm_device_dma_t *dma = dev->dma;
	unsigned long physical;
	unsigned long page;

	if (dma == NULL || dma->pagelist == NULL)
		return -1;

	page	 = offset >> PAGE_SHIFT;
	physical = dma->pagelist[page];

	DRM_DEBUG("0x%08lx (page %lu) => 0x%08lx\n", (long)offset, page, physical);
#if defined(__FreeBSD__) && __FreeBSD_version >= 500102
	*paddr = physical;
	return 0;
#else
	return atop(physical);
#endif
}

#if defined(__FreeBSD__) && __FreeBSD_version >= 500102
int DRM(mmap)(struct cdev *kdev, vm_offset_t offset, vm_paddr_t *paddr, 
    int prot)
#elif defined(__FreeBSD__)
int DRM(mmap)(dev_t kdev, vm_offset_t offset, int prot)
#elif defined(__NetBSD__)
paddr_t DRM(mmap)(dev_t kdev, off_t offset, int prot)
#endif
{
	DRM_DEVICE;
	drm_local_map_t *map = NULL;
	drm_map_list_entry_t *listentry = NULL;
	drm_file_t *priv;

	DRM_GET_PRIV_WITH_RETURN(priv, (DRMFILE)(uintptr_t)DRM_CURRENTPID);

	if (!priv->authenticated)
		return DRM_ERR(EACCES);

	if (dev->dma
	    && offset >= 0
	    && offset < ptoa(dev->dma->page_count))
#if defined(__FreeBSD__) && __FreeBSD_version >= 500102
		return DRM(dma_mmap)(kdev, offset, paddr, prot);
#else
		return DRM(dma_mmap)(kdev, offset, prot);
#endif

				/* A sequential search of a linked list is
				   fine here because: 1) there will only be
				   about 5-10 entries in the list and, 2) a
				   DRI client only has to do this mapping
				   once, so it doesn't have to be optimized
				   for performance, even if the list was a
				   bit longer. */
	TAILQ_FOREACH(listentry, dev->maplist, link) {
		map = listentry->map;
/*		DRM_DEBUG("considering 0x%x..0x%x\n", map->offset, map->offset + map->size - 1);*/
		if (offset >= map->offset
		    && offset < map->offset + map->size) break;
	}
	
	if (!listentry) {
		DRM_DEBUG("can't find map\n");
		return -1;
	}
	if (((map->flags&_DRM_RESTRICTED) && DRM_SUSER(DRM_CURPROC))) {
		DRM_DEBUG("restricted map\n");
		return -1;
	}

	switch (map->type) {
	case _DRM_FRAME_BUFFER:
	case _DRM_REGISTERS:
	case _DRM_AGP:
#if defined(__FreeBSD__) && __FreeBSD_version >= 500102
		*paddr = offset;
		return 0;
#else
		return atop(offset);
#endif
	case _DRM_SCATTER_GATHER:
	case _DRM_SHM:
#if defined(__FreeBSD__) && __FreeBSD_version >= 500102
		*paddr = vtophys(offset);
		return 0;
#else
		return atop(vtophys(offset));
#endif
	default:
		return -1;	/* This should never happen. */
	}
	DRM_DEBUG("bailing out\n");
	
	return -1;
}

