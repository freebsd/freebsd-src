/**
 * \file drm_pci.h
 * \brief PCI consistent, DMA-accessible memory functions.
 *
 * \author Eric Anholt <anholt@FreeBSD.org>
 */

/*-
 * Copyright 2003 Eric Anholt.
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * $FreeBSD$
 */

#include "dev/drm/drmP.h"

/**********************************************************************/
/** \name PCI memory */
/*@{*/

/**
 * \brief Allocate a physically contiguous DMA-accessible consistent 
 * memory block.
 */
void *
drm_pci_alloc(drm_device_t *dev, size_t size, size_t align, dma_addr_t maxaddr,
    dma_addr_t *busaddr)
{
	void *vaddr;

	vaddr = contigmalloc(size, M_DRM, M_NOWAIT, 0ul, maxaddr, align,
	    0);
	*busaddr = vtophys(vaddr);
	
	return vaddr;
}

/**
 * \brief Free a DMA-accessible consistent memory block.
 */
void
drm_pci_free(drm_device_t *dev, size_t size, void *vaddr, dma_addr_t busaddr)
{
#if __FreeBSD_version > 500000
	if (vaddr == NULL)
		return;
	contigfree(vaddr, size, M_DRM);	/* Not available on 4.x */
#endif
}

/*@}*/
