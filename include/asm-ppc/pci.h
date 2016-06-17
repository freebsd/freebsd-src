#ifndef __PPC_PCI_H
#define __PPC_PCI_H
#ifdef __KERNEL__

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <asm/scatterlist.h>
#include <asm/io.h>

struct pci_dev;

/* Values for the `which' argument to sys_pciconfig_iobase syscall.  */
#define IOBASE_BRIDGE_NUMBER	0
#define IOBASE_MEMORY		1
#define IOBASE_IO		2
#define IOBASE_ISA_IO		3
#define IOBASE_ISA_MEM		4

/*
 * Set this to 1 if you want the kernel to re-assign all PCI
 * bus numbers
 */
extern int pci_assign_all_busses;

#define pcibios_assign_all_busses()	(pci_assign_all_busses)
#define pcibios_scan_all_fns()		0

#define PCIBIOS_MIN_IO		0x1000
#define PCIBIOS_MIN_MEM		0x10000000

extern inline void pcibios_set_master(struct pci_dev *dev)
{
	/* No special bus mastering setup handling */
}

extern inline void pcibios_penalize_isa_irq(int irq)
{
	/* We don't do dynamic PCI IRQ allocation */
}

extern unsigned long pci_resource_to_bus(struct pci_dev *pdev, struct resource *res);

/*
 * The PCI bus bridge can translate addresses issued by the processor(s)
 * into a different address on the PCI bus.  On 32-bit cpus, we assume
 * this mapping is 1-1, but on 64-bit systems it often isn't.
 *
 * Obsolete ! Drivers should now use pci_resource_to_bus
 */
extern unsigned long phys_to_bus(unsigned long pa);
extern unsigned long pci_phys_to_bus(unsigned long pa, int busnr);
extern unsigned long pci_bus_to_phys(unsigned int ba, int busnr);

/*
 * Dynamic DMA Mapping stuff
 * Originally stolen from i386 by ajoshi and updated by paulus
 * Non-consistent cache support by Dan Malek
 */

/* The PCI address space does equal the physical memory
 * address space.  The networking and block device layers use
 * this boolean for bounce buffer decisions.
 */
#define PCI_DMA_BUS_IS_PHYS	(1)

/* Allocate and map kernel buffer using consistent mode DMA for a device.
 * hwdev should be valid struct pci_dev pointer for PCI devices,
 * NULL for PCI-like buses (ISA, EISA).
 * Returns non-NULL cpu-view pointer to the buffer if successful and
 * sets *dma_addrp to the pci side dma address as well, else *dma_addrp
 * is undefined.
 */
extern void *pci_alloc_consistent(struct pci_dev *hwdev, size_t size,
				  dma_addr_t *dma_handle);

/* Free and unmap a consistent DMA buffer.
 * cpu_addr is what was returned from pci_alloc_consistent,
 * size must be the same as what as passed into pci_alloc_consistent,
 * and likewise dma_addr must be the same as what *dma_addrp was set to.
 *
 * References to the memory and mappings associated with cpu_addr/dma_addr
 * past this call are illegal.
 */
extern void pci_free_consistent(struct pci_dev *hwdev, size_t size,
				void *vaddr, dma_addr_t dma_handle);

/* Map a single buffer of the indicated size for DMA in streaming mode.
 * The 32-bit bus address to use is returned.
 *
 * Once the device is given the dma address, the device owns this memory
 * until either pci_unmap_single or pci_dma_sync_single is performed.
 */
static inline dma_addr_t pci_map_single(struct pci_dev *hwdev, void *ptr,
					size_t size, int direction)
{
	BUG_ON(direction == PCI_DMA_NONE);
	consistent_sync(ptr, size, direction);
	return virt_to_bus(ptr);
}

static inline void pci_unmap_single(struct pci_dev *hwdev, dma_addr_t dma_addr,
				    size_t size, int direction)
{
	if (direction == PCI_DMA_NONE)
		BUG();
	/* nothing to do */
}

/* pci_unmap_{page,single} is a nop so... */
#define DECLARE_PCI_UNMAP_ADDR(ADDR_NAME)
#define DECLARE_PCI_UNMAP_LEN(LEN_NAME)
#define pci_unmap_addr(PTR, ADDR_NAME)		(0)
#define pci_unmap_addr_set(PTR, ADDR_NAME, VAL)	do { } while (0)
#define pci_unmap_len(PTR, LEN_NAME)		(0)
#define pci_unmap_len_set(PTR, LEN_NAME, VAL)	do { } while (0)

/*
 * pci_{map,unmap}_single_page maps a kernel page to a dma_addr_t. identical
 * to pci_map_single, but takes a struct page instead of a virtual address
 */
static inline dma_addr_t pci_map_page(struct pci_dev *hwdev, struct page *page,
				      unsigned long offset, size_t size,
				      int direction)
{
	BUG_ON(direction == PCI_DMA_NONE);
	consistent_sync_page(page, offset, size, direction);
	return (page - mem_map) * PAGE_SIZE + PCI_DRAM_OFFSET + offset;
}

static inline void pci_unmap_page(struct pci_dev *hwdev, dma_addr_t dma_address,
				  size_t size, int direction)
{
	if (direction == PCI_DMA_NONE)
		BUG();
	/* Nothing to do */
}

/* Map a set of buffers described by scatterlist in streaming
 * mode for DMA.  This is the scather-gather version of the
 * above pci_map_single interface.  Here the scatter gather list
 * elements are each tagged with the appropriate dma address
 * and length.  They are obtained via sg_dma_{address,len}(SG),
 * defined in <asm/scatterlist.h>.
 *
 * NOTE: An implementation may be able to use a smaller number of
 *       DMA address/length pairs than there are SG table elements.
 *       (for example via virtual mapping capabilities)
 *       The routine returns the number of addr/length pairs actually
 *       used, at most nents.
 *
 * Device ownership issues as mentioned above for pci_map_single are
 * the same here.
 */
static inline int pci_map_sg(struct pci_dev *hwdev, struct scatterlist *sg,
			     int nents, int direction)
{
	int i;

	if (direction == PCI_DMA_NONE)
		BUG();

	/*
	 * temporary 2.4 hack
	 */
	for (i = 0; i < nents; i++) {
		if (sg[i].address && sg[i].page)
			BUG();
		else if (!sg[i].address && !sg[i].page)
			BUG();

		if (sg[i].address) {
			consistent_sync(sg[i].address, sg[i].length, direction);
			sg[i].dma_address = virt_to_bus(sg[i].address);
		} else {
			consistent_sync_page(sg[i].page, sg[i].offset,
					     sg[i].length, direction);
			sg[i].dma_address = page_to_bus(sg[i].page) + sg[i].offset;
		}
		sg[i].dma_length = sg[i].length;
	}

	return nents;
}

/* Unmap a set of streaming mode DMA translations.
 * Again, cpu read rules concerning calls here are the same as for
 * pci_unmap_single() above.
 */
static inline void pci_unmap_sg(struct pci_dev *hwdev, struct scatterlist *sg,
				int nents, int direction)
{
	BUG_ON(direction == PCI_DMA_NONE);
	/* nothing to do */
}

/* Make physical memory consistent for a single
 * streaming mode DMA translation after a transfer.
 *
 * If you perform a pci_map_single() but wish to interrogate the
 * buffer using the cpu, yet do not wish to teardown the PCI dma
 * mapping, you must call this function before doing so.  At the
 * next point you give the PCI dma address back to the card, the
 * device again owns the buffer.
 */
static inline void pci_dma_sync_single(struct pci_dev *hwdev,
				       dma_addr_t dma_handle,
				       size_t size, int direction)
{
	BUG_ON(direction == PCI_DMA_NONE);

	consistent_sync(bus_to_virt(dma_handle), size, direction);
}

/* Make physical memory consistent for a set of streaming
 * mode DMA translations after a transfer.
 *
 * The same as pci_dma_sync_single but for a scatter-gather list,
 * same rules and usage.
 */
static inline void pci_dma_sync_sg(struct pci_dev *hwdev,
				   struct scatterlist *sg,
				   int nelems, int direction)
{
	int i;

	BUG_ON(direction == PCI_DMA_NONE);

	for (i = 0; i < nelems; i++, sg++) {
		if (sg->address)
			consistent_sync(sg->address, sg->length, direction);
		else
			consistent_sync_page(sg->page, sg->offset,
					sg->length, direction);
	}
}

/* Return whether the given PCI device DMA address mask can
 * be supported properly.  For example, if your device can
 * only drive the low 24-bits during PCI bus mastering, then
 * you would pass 0x00ffffff as the mask to this function.
 */
static inline int pci_dma_supported(struct pci_dev *hwdev, u64 mask)
{
	return 1;
}

/*
 * At present there are very few 32-bit PPC machines that can have
 * memory above the 4GB point, and we don't support that.
 */
#define pci_dac_dma_supported(pci_dev, mask)	(0)

static __inline__ dma64_addr_t
pci_dac_page_to_dma(struct pci_dev *pdev, struct page *page, unsigned long offset, int direction)
{
	return (dma64_addr_t) page_to_bus(page) + offset;
}

static __inline__ struct page *
pci_dac_dma_to_page(struct pci_dev *pdev, dma64_addr_t dma_addr)
{
	return mem_map + (unsigned long)(dma_addr >> PAGE_SHIFT);
}

static __inline__ unsigned long
pci_dac_dma_to_offset(struct pci_dev *pdev, dma64_addr_t dma_addr)
{
	return (dma_addr & ~PAGE_MASK);
}

static __inline__ void
pci_dac_dma_sync_single(struct pci_dev *pdev, dma64_addr_t dma_addr, size_t len, int direction)
{
	/* Nothing to do. */
}

/* Return the index of the PCI controller for device PDEV. */
extern int pci_controller_num(struct pci_dev *pdev);

/* Map a range of PCI memory or I/O space for a device into user space */
int pci_mmap_page_range(struct pci_dev *pdev, struct vm_area_struct *vma,
			enum pci_mmap_state mmap_state, int write_combine);

/* Tell drivers/pci/proc.c that we have pci_mmap_page_range() */
#define HAVE_PCI_MMAP	1

#endif	/* __KERNEL__ */

#endif /* __PPC_PCI_H */
