#ifndef __i386_PCI_H
#define __i386_PCI_H

#include <linux/config.h>

#ifdef __KERNEL__

/* Can be used to override the logic in pci_scan_bus for skipping
   already-configured bus numbers - to be used for buggy BIOSes
   or architectures with incomplete PCI setup by the loader */

#ifdef CONFIG_PCI
extern unsigned int pcibios_assign_all_busses(void);
#else
#define pcibios_assign_all_busses()	0
#endif
#define pcibios_scan_all_fns()		0

extern unsigned long pci_mem_start;
#define PCIBIOS_MIN_IO		0x1000
#define PCIBIOS_MIN_MEM		(pci_mem_start)

void pcibios_config_init(void);
struct pci_bus * pcibios_scan_root(int bus);
extern int (*pci_config_read)(int seg, int bus, int dev, int fn, int reg, int len, u32 *value);
extern int (*pci_config_write)(int seg, int bus, int dev, int fn, int reg, int len, u32 value);

void pcibios_set_master(struct pci_dev *dev);
void pcibios_penalize_isa_irq(int irq);
struct irq_routing_table *pcibios_get_irq_routing_table(void);
int pcibios_set_irq_routing(struct pci_dev *dev, int pin, int irq);

/* Dynamic DMA mapping stuff.
 * i386 has everything mapped statically.
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <asm/scatterlist.h>
#include <linux/string.h>
#include <asm/io.h>

struct pci_dev;

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
	if (direction == PCI_DMA_NONE)
		out_of_line_bug();
	flush_write_buffers();
	return virt_to_bus(ptr);
}

/* Unmap a single streaming mode DMA translation.  The dma_addr and size
 * must match what was provided for in a previous pci_map_single call.  All
 * other usages are undefined.
 *
 * After this call, reads by the cpu to the buffer are guarenteed to see
 * whatever the device wrote there.
 */
static inline void pci_unmap_single(struct pci_dev *hwdev, dma_addr_t dma_addr,
				    size_t size, int direction)
{
	if (direction == PCI_DMA_NONE)
		out_of_line_bug();
	/* Nothing to do */
}

/*
 * pci_{map,unmap}_single_page maps a kernel page to a dma_addr_t. identical
 * to pci_map_single, but takes a struct page instead of a virtual address
 */
static inline dma_addr_t pci_map_page(struct pci_dev *hwdev, struct page *page,
				      unsigned long offset, size_t size, int direction)
{
	if (direction == PCI_DMA_NONE)
		out_of_line_bug();

	return ((dma_addr_t)(page - mem_map) *
		(dma_addr_t) PAGE_SIZE +
		(dma_addr_t) offset);
}

static inline void pci_unmap_page(struct pci_dev *hwdev, dma_addr_t dma_address,
				  size_t size, int direction)
{
	if (direction == PCI_DMA_NONE)
		out_of_line_bug();
	/* Nothing to do */
}

/* pci_unmap_{page,single} is a nop so... */
#define DECLARE_PCI_UNMAP_ADDR(ADDR_NAME)
#define DECLARE_PCI_UNMAP_LEN(LEN_NAME)
#define pci_unmap_addr(PTR, ADDR_NAME)		(0)
#define pci_unmap_addr_set(PTR, ADDR_NAME, VAL)	do { } while (0)
#define pci_unmap_len(PTR, LEN_NAME)		(0)
#define pci_unmap_len_set(PTR, LEN_NAME, VAL)	do { } while (0)

/* Map a set of buffers described by scatterlist in streaming
 * mode for DMA.  This is the scather-gather version of the
 * above pci_map_single interface.  Here the scatter gather list
 * elements are each tagged with the appropriate dma address
 * and length.  They are obtained via sg_dma_{address,length}(SG).
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
		out_of_line_bug();
 
 	/*
 	 * temporary 2.4 hack
 	 */
 	for (i = 0; i < nents; i++ ) {
 		if (sg[i].address && sg[i].page)
 			out_of_line_bug();
 		else if (!sg[i].address && !sg[i].page)
 			out_of_line_bug();
 
 		if (sg[i].address)
 			sg[i].dma_address = virt_to_bus(sg[i].address);
 		else
 			sg[i].dma_address = page_to_bus(sg[i].page) + sg[i].offset;
 	}
 
	flush_write_buffers();
	return nents;
}

/* Unmap a set of streaming mode DMA translations.
 * Again, cpu read rules concerning calls here are the same as for
 * pci_unmap_single() above.
 */
static inline void pci_unmap_sg(struct pci_dev *hwdev, struct scatterlist *sg,
				int nents, int direction)
{
	if (direction == PCI_DMA_NONE)
		out_of_line_bug();
	/* Nothing to do */
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
	if (direction == PCI_DMA_NONE)
		out_of_line_bug();
	flush_write_buffers();
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
	if (direction == PCI_DMA_NONE)
		out_of_line_bug();
	flush_write_buffers();
}

/* Return whether the given PCI device DMA address mask can
 * be supported properly.  For example, if your device can
 * only drive the low 24-bits during PCI bus mastering, then
 * you would pass 0x00ffffff as the mask to this function.
 */
static inline int pci_dma_supported(struct pci_dev *hwdev, u64 mask)
{
        /*
         * we fall back to GFP_DMA when the mask isn't all 1s,
         * so we can't guarantee allocations that must be
         * within a tighter range than GFP_DMA..
         */
        if(mask < 0x00ffffff)
                return 0;

	return 1;
}

/* This is always fine. */
#define pci_dac_dma_supported(pci_dev, mask)	(1)

static __inline__ dma64_addr_t
pci_dac_page_to_dma(struct pci_dev *pdev, struct page *page, unsigned long offset, int direction)
{
	return ((dma64_addr_t) page_to_bus(page) +
		(dma64_addr_t) offset);
}

static __inline__ struct page *
pci_dac_dma_to_page(struct pci_dev *pdev, dma64_addr_t dma_addr)
{
	unsigned long poff = (dma_addr >> PAGE_SHIFT);

	return mem_map + poff;
}

static __inline__ unsigned long
pci_dac_dma_to_offset(struct pci_dev *pdev, dma64_addr_t dma_addr)
{
	return (dma_addr & ~PAGE_MASK);
}

static __inline__ void
pci_dac_dma_sync_single(struct pci_dev *pdev, dma64_addr_t dma_addr, size_t len, int direction)
{
	flush_write_buffers();
}

/* These macros should be used after a pci_map_sg call has been done
 * to get bus addresses of each of the SG entries and their lengths.
 * You should only work with the number of sg entries pci_map_sg
 * returns.
 */
#define sg_dma_address(sg)	((sg)->dma_address)
#define sg_dma_len(sg)		((sg)->length)

/* Return the index of the PCI controller for device. */
static inline int pci_controller_num(struct pci_dev *dev)
{
	return 0;
}

#define HAVE_PCI_MMAP
extern int pci_mmap_page_range(struct pci_dev *dev, struct vm_area_struct *vma,
			       enum pci_mmap_state mmap_state, int write_combine);

#endif /* __KERNEL__ */

#endif /* __i386_PCI_H */
