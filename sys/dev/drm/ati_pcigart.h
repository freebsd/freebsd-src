/* ati_pcigart.h -- ATI PCI GART support -*- linux-c -*-
 * Created: Wed Dec 13 21:52:19 2000 by gareth@valinux.com
 *
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
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *   Gareth Hughes <gareth@valinux.com>
 *
 * $FreeBSD$
 */

#include "dev/drm/drmP.h"

#if PAGE_SIZE == 8192
# define ATI_PCIGART_TABLE_ORDER 	2
# define ATI_PCIGART_TABLE_PAGES 	(1 << 2)
#elif PAGE_SIZE == 4096
# define ATI_PCIGART_TABLE_ORDER 	3
# define ATI_PCIGART_TABLE_PAGES 	(1 << 3)
#elif
# error - PAGE_SIZE not 8K or 4K
#endif

# define ATI_MAX_PCIGART_PAGES		8192	/* 32 MB aperture, 4K pages */
# define ATI_PCIGART_PAGE_SIZE		4096	/* PCI GART page size */

int DRM(ati_pcigart_init)( drm_device_t *dev,
			   unsigned long *addr,
			   dma_addr_t *bus_addr)
{
	drm_sg_mem_t *entry = dev->sg;
	unsigned long address = 0;
	unsigned long pages;
	u32 *pci_gart=0, page_base, bus_address = 0;
	int i, j, ret = 0;

	if ( !entry ) {
		DRM_ERROR( "no scatter/gather memory!\n" );
		goto done;
	}

	address = (long)contigmalloc((1 << ATI_PCIGART_TABLE_ORDER) * PAGE_SIZE, 
	    DRM(M_DRM), M_NOWAIT, 0ul, 0xfffffffful, PAGE_SIZE, 0);
	if ( !address ) {
		DRM_ERROR( "cannot allocate PCI GART page!\n" );
		goto done;
	}

	/* XXX: we need to busdma this */
	bus_address = vtophys( address );

	pci_gart = (u32 *)address;

	pages = ( entry->pages <= ATI_MAX_PCIGART_PAGES )
		? entry->pages : ATI_MAX_PCIGART_PAGES;

	bzero( pci_gart, ATI_MAX_PCIGART_PAGES * sizeof(u32) );

	for ( i = 0 ; i < pages ; i++ ) {
		entry->busaddr[i] = vtophys( entry->handle + (i*PAGE_SIZE) );
		page_base = (u32) entry->busaddr[i];

		for (j = 0; j < (PAGE_SIZE / ATI_PCIGART_PAGE_SIZE); j++) {
			*pci_gart++ = cpu_to_le32( page_base );
			page_base += ATI_PCIGART_PAGE_SIZE;
		}
	}

	DRM_MEMORYBARRIER();

	ret = 1;

done:
	*addr = address;
	*bus_addr = bus_address;
	return ret;
}

int DRM(ati_pcigart_cleanup)( drm_device_t *dev,
			      unsigned long addr,
			      dma_addr_t bus_addr)
{
	drm_sg_mem_t *entry = dev->sg;

	/* we need to support large memory configurations */
	if ( !entry ) {
		DRM_ERROR( "no scatter/gather memory!\n" );
		return 0;
	}

#if __FreeBSD_version > 500000
	contigfree( (void *)addr, (1 << ATI_PCIGART_TABLE_ORDER) * PAGE_SIZE, DRM(M_DRM));	/* Not available on 4.x */
#endif
	return 1;
}
