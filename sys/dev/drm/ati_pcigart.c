/* ati_pcigart.h -- ATI PCI GART support -*- linux-c -*-
 * Created: Wed Dec 13 21:52:19 2000 by gareth@valinux.com
 */
/*-
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "dev/drm/drmP.h"

#define ATI_PCIGART_PAGE_SIZE		4096	/* PCI GART page size */
#define ATI_MAX_PCIGART_PAGES		8192	/* 32 MB aperture, 4K pages */
#define ATI_PCIGART_TABLE_SIZE		32768

int drm_ati_pcigart_init(drm_device_t *dev, drm_ati_pcigart_info *gart_info)
{
	unsigned long pages;
	u32 *pci_gart = NULL, page_base;
	int i, j;

	if (dev->sg == NULL) {
		DRM_ERROR( "no scatter/gather memory!\n" );
		return 0;
	}

	if (gart_info->gart_table_location == DRM_ATI_GART_MAIN) {
		/* GART table in system memory */
		dev->sg->dmah = drm_pci_alloc(dev, ATI_PCIGART_TABLE_SIZE, 0,
		    0xfffffffful);
		if (dev->sg->dmah == NULL) {
			DRM_ERROR("cannot allocate PCI GART table!\n");
			return 0;
		}
	
		gart_info->addr = (void *)dev->sg->dmah->vaddr;
		gart_info->bus_addr = dev->sg->dmah->busaddr;
		pci_gart = (u32 *)dev->sg->dmah->vaddr;
	} else {
		/* GART table in framebuffer memory */
		pci_gart = gart_info->addr;
	}
	
	pages = DRM_MIN(dev->sg->pages, ATI_MAX_PCIGART_PAGES);

	bzero(pci_gart, ATI_PCIGART_TABLE_SIZE);

	KASSERT(PAGE_SIZE >= ATI_PCIGART_PAGE_SIZE, ("page size too small"));

	for ( i = 0 ; i < pages ; i++ ) {
		page_base = (u32) dev->sg->busaddr[i];

		for (j = 0; j < (PAGE_SIZE / ATI_PCIGART_PAGE_SIZE); j++) {
			if (gart_info->is_pcie)
				*pci_gart = (cpu_to_le32(page_base) >> 8) | 0xc;
			else
				*pci_gart = cpu_to_le32(page_base);
			pci_gart++;
			page_base += ATI_PCIGART_PAGE_SIZE;
		}
	}

	DRM_MEMORYBARRIER();

	return 1;
}

int drm_ati_pcigart_cleanup(drm_device_t *dev, drm_ati_pcigart_info *gart_info)
{
	if (dev->sg == NULL) {
		DRM_ERROR( "no scatter/gather memory!\n" );
		return 0;
	}

	drm_pci_free(dev, dev->sg->dmah);

	return 1;
}
