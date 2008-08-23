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

/** @file ati_pcigart.c
 * Implementation of ATI's PCIGART, which provides an aperture in card virtual
 * address space with addresses remapped to system memory.
 */

#include "dev/drm/drmP.h"

#define ATI_PCIGART_PAGE_SIZE		4096	/* PCI GART page size */
#define ATI_PCIGART_PAGE_MASK		(~(ATI_PCIGART_PAGE_SIZE-1))

#define ATI_PCIE_WRITE 0x4
#define ATI_PCIE_READ 0x8

static int drm_ati_alloc_pcigart_table(struct drm_device *dev,
				       struct drm_ati_pcigart_info *gart_info)
{
	dev->sg->dmah = drm_pci_alloc(dev, gart_info->table_size,
						PAGE_SIZE,
						gart_info->table_mask);
	if (dev->sg->dmah == NULL)
		return ENOMEM;

	return 0;
}

static void drm_ati_free_pcigart_table(struct drm_device *dev,
				       struct drm_ati_pcigart_info *gart_info)
{
	drm_pci_free(dev, dev->sg->dmah);
	dev->sg->dmah = NULL;
}

int drm_ati_pcigart_cleanup(struct drm_device *dev, struct drm_ati_pcigart_info *gart_info)
{
	/* we need to support large memory configurations */
	if (dev->sg == NULL) {
		DRM_ERROR("no scatter/gather memory!\n");
		return 0;
	}

	if (gart_info->bus_addr) {
		if (gart_info->gart_table_location == DRM_ATI_GART_MAIN) {
			gart_info->bus_addr = 0;
			if (dev->sg->dmah)
				drm_ati_free_pcigart_table(dev, gart_info);
		}
	}

	return 1;
}

int drm_ati_pcigart_init(struct drm_device *dev,
			 struct drm_ati_pcigart_info *gart_info)
{

	void *address = NULL;
	unsigned long pages;
	u32 *pci_gart, page_base;
	dma_addr_t bus_address = 0;
	int i, j, ret = 0;
	int max_pages;
	dma_addr_t entry_addr;

	/* we need to support large memory configurations */
	if (dev->sg == NULL) {
		DRM_ERROR("no scatter/gather memory!\n");
		goto done;
	}

	if (gart_info->gart_table_location == DRM_ATI_GART_MAIN) {
		DRM_DEBUG("PCI: no table in VRAM: using normal RAM\n");

		ret = drm_ati_alloc_pcigart_table(dev, gart_info);
		if (ret) {
			DRM_ERROR("cannot allocate PCI GART page!\n");
			goto done;
		}

		address = (void *)dev->sg->dmah->vaddr;
		bus_address = dev->sg->dmah->busaddr;
	} else {
		address = gart_info->addr;
		bus_address = gart_info->bus_addr;
		DRM_DEBUG("PCI: Gart Table: VRAM %08X mapped at %08lX\n",
			  (unsigned int)bus_address, (unsigned long)address);
	}

	pci_gart = (u32 *) address;

	max_pages = (gart_info->table_size / sizeof(u32));
	pages = (dev->sg->pages <= max_pages)
	    ? dev->sg->pages : max_pages;

	memset(pci_gart, 0, max_pages * sizeof(u32));

	KASSERT(PAGE_SIZE >= ATI_PCIGART_PAGE_SIZE, ("page size too small"));

	for (i = 0; i < pages; i++) {
		entry_addr = dev->sg->busaddr[i];
		for (j = 0; j < (PAGE_SIZE / ATI_PCIGART_PAGE_SIZE); j++) {
			page_base = (u32) entry_addr & ATI_PCIGART_PAGE_MASK;
			switch(gart_info->gart_reg_if) {
			case DRM_ATI_GART_IGP:
				page_base |= (upper_32_bits(entry_addr) & 0xff) << 4;
				page_base |= 0xc;
				break;
			case DRM_ATI_GART_PCIE:
				page_base >>= 8;
				page_base |= (upper_32_bits(entry_addr) & 0xff) << 24;
				page_base |= ATI_PCIE_READ | ATI_PCIE_WRITE;
				break;
			default:
			case DRM_ATI_GART_PCI:
				break;
			}
			*pci_gart = cpu_to_le32(page_base);
			pci_gart++;
			entry_addr += ATI_PCIGART_PAGE_SIZE;
		}
	}

	DRM_MEMORYBARRIER();

	ret = 1;

    done:
	gart_info->addr = address;
	gart_info->bus_addr = bus_address;
	return ret;
}
