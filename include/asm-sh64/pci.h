
/*
**
**                      P C I  -  C O M M O N  Internal Interface
**
** Module:    This module includes all functions that can be used for all included
**            bridge.
**
** Copyright: This file is subject to the terms and conditions of the GNU General Public
**            License.  See the file "COPYING" in the main directory of this archive
**            for more details.
**
**            Copyright (C) 2001   Roberto Giai Meniet (giai@while1.com)  
**                                 Franco  Ometti      (ometti@while1.com) 
**
** File:      include/asm-sh64/pci.h
**
** Note:      For a good view of this file use TABSTOP=8
**
*/


/*
** System includes
*/
#include <linux/types.h>
#include <linux/slab.h>
#include <asm/scatterlist.h>
#include <linux/string.h>
#include <asm/io.h>

struct pci_dev;

#ifndef __ASM_SH64_PCI_H
#define __ASM_SH64_PCI_H

#ifdef __KERNEL__

/* 
** Can be used to override the logic in pci_scan_bus for skipping
** already-configured bus numbers - to be used for buggy BIOSes
** or architectures with incomplete PCI setup by the loader 
*/
#define pcibios_assign_all_busses()     1
#define pcibios_scan_all_fns()		0

/* 
** These are currently the correct values for the STM overdrive board.
** We need some way of setting this on a board specific way, it will
** not be the same on other boards I think
*/
#if defined(CONFIG_CPU_SUBTYPE_SH5_101) || defined(CONFIG_CPU_SUBTYPE_SH5_103)
#define PCIBIOS_MIN_IO          0x2000
#define PCIBIOS_MIN_MEM         0x40000000
#endif


/*
** Set penalize isa irq function
*/
static inline void pcibios_penalize_isa_irq(int irq)
{
	/* We don't do dynamic PCI IRQ allocation */
}


/* 
** Dynamic DMA mapping stuff.
** SuperH has everything mapped statically like x86.
*/

/* 
** Allocate and map kernel buffer using consistent mode DMA for a device.
** hwdev should be valid struct pci_dev pointer for PCI devices,
** NULL for PCI-like buses (ISA, EISA).
** Returns non-NULL cpu-view pointer to the buffer if successful and
** sets *dma_addrp to the pci side dma address as well, else *dma_addrp
** is undefined.
*/
extern void *pci_alloc_consistent(struct pci_dev *hwdev, size_t size,
				  dma_addr_t *dma_handle);

/* 
** Free and unmap a consistent DMA buffer.
** cpu_addr is what was returned from pci_alloc_consistent,
** size must be the same as what as passed into pci_alloc_consistent,
** and likewise dma_addr must be the same as what *dma_addrp was set to.
**
** References to the memory and mappings associated with cpu_addr/dma_addr
** past this call are illegal.
*/
extern void pci_free_consistent(struct pci_dev *hwdev, size_t size,
				void *vaddr, dma_addr_t dma_handle);

/* 
** Map a single buffer of the indicated size for DMA in streaming mode.
** The 32-bit bus address to use is returned.
**
** Once the device is given the dma address, the device owns this memory
** until either pci_unmap_single or pci_dma_sync_single is performed.
*/
static inline dma_addr_t pci_map_single(struct pci_dev *hwdev, void *ptr,
					size_t size,int direction)
{
	dma_cache_wback_inv((unsigned long)ptr, size);
	return virt_to_bus(ptr);
}


/*
** Unmap a single streaming mode DMA translation.  The dma_addr and size
** must match what was provided for in a previous pci_map_single call.  All
** other usages are undefined.
**
** After this call, reads by the cpu to the buffer are guarenteed to see
** whatever the device wrote there.
*/
static inline void pci_unmap_single(struct pci_dev *hwdev, dma_addr_t dma_addr,
				    size_t size,int direction)
{
	/* Nothing to do */
}

/*
 * pci_{map,unmap}_single_page maps a kernel page to a dma_addr_t. identical
 * to pci_map_single, but takes a struct page instead of a virtual address
 */
static inline dma_addr_t pci_map_page(struct pci_dev *hwdev, struct page *page,
				      unsigned long offset, size_t size, int direction)
{
	return ((dma_addr_t)(page - mem_map) *
		(dma_addr_t) PAGE_SIZE +
		(dma_addr_t) offset);
}

static inline void pci_unmap_page(struct pci_dev *hwdev, dma_addr_t dma_address,
				  size_t size, int direction)
{
	/* Nothing to do */
}

/* 
** Map a set of buffers described by scatterlist in streaming
** mode for DMA.  This is the scather-gather version of the
** above pci_map_single interface.  Here the scatter gather list
** elements are each tagged with the appropriate dma address
** and length.  They are obtained via sg_dma_{address,length}(SG).
**
** NOTE: An implementation may be able to use a smaller number of
**       DMA address/length pairs than there are SG table elements.
**       (for example via virtual mapping capabilities)
**       The routine returns the number of addr/length pairs actually
**       used, at most nents.
**
** Device ownership issues as mentioned above for pci_map_single are
** the same here.
*/
static inline int pci_map_sg(struct pci_dev *hwdev, struct scatterlist *sg,
			     int nents,int direction)
{
	int i;

	for (i = 0; i < nents; i++) {
		if (sg[i].address) {
			dma_cache_wback_inv((unsigned long)sg[i].address,
					    sg[i].length);
			sg[i].dma_address = virt_to_bus(sg[i].address);
		} else {
			sg[i].dma_address = page_to_bus(sg[i].page) +
					    sg[i].offset;
			dma_cache_wback_inv((unsigned long) bus_to_virt(sg[i].dma_address), sg[i].length);
		}
	}

	return nents;
}


/* 
** Unmap a set of streaming mode DMA translations.
** Again, cpu read rules concerning calls here are the same as for
** pci_unmap_single() above.
*/
static inline void pci_unmap_sg(struct pci_dev *hwdev, struct scatterlist *sg,
				int nents,int direction)
{
	int i;

	if (direction == PCI_DMA_TODEVICE)
		return;
	
	for (i = 0; i < nents; i++) {
		if (!sg[i].address)
			continue;

		dma_cache_wback_inv((unsigned long)sg[i].address, sg[i].length);
	}
}


/* 
** Make physical memory consistent for a single
** streaming mode DMA translation after a transfer.
**
** If you perform a pci_map_single() but wish to interrogate the
** buffer using the cpu, yet do not wish to teardown the PCI dma
** mapping, you must call this function before doing so.  At the
** next point you give the PCI dma address back to the card, the
** device again owns the buffer.
*/
static inline void pci_dma_sync_single(struct pci_dev *hwdev,
				       dma_addr_t dma_handle,
				       size_t size,int direction)
{
	if (direction == PCI_DMA_NONE)
		BUG();
	
	dma_cache_wback_inv((unsigned long) bus_to_virt(dma_handle), size);
}


/* 
** Make physical memory consistent for a set of streaming
** mode DMA translations after a transfer.
**
** The same as pci_dma_sync_single but for a scatter-gather list,
** same rules and usage.
*/
static inline void pci_dma_sync_sg(struct pci_dev *hwdev,
				   struct scatterlist *sg,
				   int nelems,int direction)
{
	int i;

	for (i = 0; i < nelems; i++)
		dma_cache_wback_inv((unsigned long)sg[i].address, sg[i].length);
}


/* 
** Return whether the given PCI device DMA address mask can
** be supported properly.  For example, if your device can
** only drive the low 24-bits during PCI bus mastering, then
** you would pass 0x00ffffff as the mask to this function.
*/
extern inline int pci_dma_supported(struct pci_dev *hwdev, dma_addr_t mask)
{
	return 1;
}


/* Not supporting more than 32-bit PCI bus addresses now, but
 * must satisfy references to this function.  Change if needed.
 */
#define pci_dac_dma_supported(pci_dev, mask) (0)
 
/* Return the index of the PCI controller for device PDEV. */
#define pci_controller_num(PDEV)        (0)

/* 
** These macros should be used after a pci_map_sg call has been done
** to get bus addresses of each of the SG entries and their lengths.
** You should only work with the number of sg entries pci_map_sg
** returns, or alternatively stop on the first sg_dma_len(sg) which
** is 0.
*/
#define sg_dma_address(sg)      ((sg)->dma_address)
#define sg_dma_len(sg)          ((sg)->length)

#define PCI_DMA_BUS_IS_PHYS	(1)

#endif /* __KERNEL__ */

#endif /* __ASM_SH64_PCI_H */

