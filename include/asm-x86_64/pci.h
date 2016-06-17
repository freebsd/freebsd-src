#ifndef __x8664_PCI_H
#define __x8664_PCI_H

#include <linux/config.h>
#include <asm/io.h>

#ifdef __KERNEL__

extern dma_addr_t bad_dma_address;

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

#include <linux/types.h>
#include <linux/slab.h>
#include <asm/scatterlist.h>
#include <linux/string.h>
#include <asm/io.h>
#include <asm/page.h>
#include <asm/mmzone.h>

struct pci_dev;
extern int force_mmu;

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

#ifdef CONFIG_SWIOTLB
extern int swiotlb; 
extern dma_addr_t swiotlb_map_single (struct pci_dev *hwdev, void *ptr, size_t size, 
                                     int dir);
extern void swiotlb_unmap_single (struct pci_dev *hwdev, dma_addr_t dev_addr,
                                 size_t size, int dir);
extern void swiotlb_sync_single (struct pci_dev *hwdev, dma_addr_t dev_addr, 
                                size_t size, int dir);
extern void swiotlb_sync_sg (struct pci_dev *hwdev, struct scatterlist *sg, int nelems, 
                            int dir);
#endif

#ifdef CONFIG_GART_IOMMU

/* Map a single buffer of the indicated size for DMA in streaming mode.
 * The 32-bit bus address to use is returned.
 *
 * Once the device is given the dma address, the device owns this memory
 * until either pci_unmap_single or pci_dma_sync_single is performed.
 */
extern dma_addr_t pci_map_single(struct pci_dev *hwdev, void *ptr,
				 size_t size, int direction);


void pci_unmap_single(struct pci_dev *hwdev, dma_addr_t addr,
				   size_t size, int direction);

/*
 * pci_{map,unmap}_single_page maps a kernel page to a dma_addr_t. identical
 * to pci_map_single, but takes a struct page instead of a virtual address
 */

#define pci_map_page(dev,page,offset,size,dir) \
	pci_map_single((dev), page_address(page)+(offset), (size), (dir)) 

#define DECLARE_PCI_UNMAP_ADDR(ADDR_NAME)	\
	dma_addr_t ADDR_NAME;
#define DECLARE_PCI_UNMAP_LEN(LEN_NAME)		\
	__u32 LEN_NAME;
#define pci_unmap_addr(PTR, ADDR_NAME)			\
	((PTR)->ADDR_NAME)
#define pci_unmap_addr_set(PTR, ADDR_NAME, VAL)		\
	(((PTR)->ADDR_NAME) = (VAL))
#define pci_unmap_len(PTR, LEN_NAME)			\
	((PTR)->LEN_NAME)
#define pci_unmap_len_set(PTR, LEN_NAME, VAL)		\
	(((PTR)->LEN_NAME) = (VAL))

static inline void pci_dma_sync_single(struct pci_dev *hwdev, 
				       dma_addr_t dma_handle,
				       size_t size, int direction)
{
#ifdef CONFIG_SWIOTLB
       if (swiotlb)
               return swiotlb_sync_single(hwdev,dma_handle,size,direction);
#endif
	BUG_ON(direction == PCI_DMA_NONE); 
} 

static inline void pci_dma_sync_sg(struct pci_dev *hwdev, 
				   struct scatterlist *sg,
				   int nelems, int direction)
{ 
	BUG_ON(direction == PCI_DMA_NONE); 
#ifdef CONFIG_SWIOTLB
       if (swiotlb)
               return swiotlb_sync_sg(hwdev,sg,nelems,direction);
#endif
} 

/* The PCI address space does equal the physical memory
 * address space.  The networking and block device layers use
 * this boolean for bounce buffer decisions.
 */
#define PCI_DMA_BUS_IS_PHYS	(0)


#else
static inline dma_addr_t pci_map_single(struct pci_dev *hwdev, void *ptr,
					size_t size, int direction)
{
	dma_addr_t addr; 

	if (direction == PCI_DMA_NONE)
		out_of_line_bug();	
	addr = virt_to_bus(ptr); 

	/* 
	 * This is gross, but what should I do.
	 * Unfortunately drivers do not test the return value of this.
	 */
	if ((addr+size) & ~hwdev->dma_mask) 
		out_of_line_bug(); 
	return addr;
}

static inline void pci_unmap_single(struct pci_dev *hwdev, dma_addr_t dma_addr,
				    size_t size, int direction)
{
	if (direction == PCI_DMA_NONE)
		out_of_line_bug();
	/* Nothing to do */
}

static inline dma_addr_t pci_map_page(struct pci_dev *hwdev, struct page *page,
				      unsigned long offset, size_t size, int direction)
{
	dma_addr_t addr;
	if (direction == PCI_DMA_NONE)
		out_of_line_bug();	
 	addr = page_to_pfn(page) * PAGE_SIZE + offset;
	if ((addr+size) & ~hwdev->dma_mask) 
		out_of_line_bug();
	return addr;
}

/* pci_unmap_{page,single} is a nop so... */
#define DECLARE_PCI_UNMAP_ADDR(ADDR_NAME)
#define DECLARE_PCI_UNMAP_LEN(LEN_NAME)
#define pci_unmap_addr(PTR, ADDR_NAME)		(0)
#define pci_unmap_addr_set(PTR, ADDR_NAME, VAL)	do { } while (0)
#define pci_unmap_len(PTR, LEN_NAME)		(0)
#define pci_unmap_len_set(PTR, LEN_NAME, VAL)	do { } while (0)
	
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

#define PCI_DMA_BUS_IS_PHYS	1

#endif

extern int pci_map_sg(struct pci_dev *hwdev, struct scatterlist *sg,
		      int nents, int direction);
extern void pci_unmap_sg(struct pci_dev *hwdev, struct scatterlist *sg,
			 int nents, int direction);

#define pci_unmap_page pci_unmap_single

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
	return pfn_to_page(phys_to_pfn(dma_addr)); 
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

#endif /* __x8664_PCI_H */
