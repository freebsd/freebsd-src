#ifndef __ALPHA_PCI_H
#define __ALPHA_PCI_H

#ifdef __KERNEL__

#include <linux/spinlock.h>
#include <asm/scatterlist.h>
#include <asm/machvec.h>

/*
 * The following structure is used to manage multiple PCI busses.
 */

struct pci_dev;
struct pci_bus;
struct resource;
struct pci_iommu_arena;
struct page;

/* A controller.  Used to manage multiple PCI busses.  */

struct pci_controller {
	struct pci_controller *next;
        struct pci_bus *bus;
	struct resource *io_space;
	struct resource *mem_space;

	/* The following are for reporting to userland.  The invariant is
	   that if we report a BWX-capable dense memory, we do not report
	   a sparse memory at all, even if it exists.  */
	unsigned long sparse_mem_base;
	unsigned long dense_mem_base;
	unsigned long sparse_io_base;
	unsigned long dense_io_base;

	/* This one's for the kernel only.  It's in KSEG somewhere.  */
	unsigned long config_space_base;

	unsigned int index;
	unsigned int first_busno;
	unsigned int last_busno;

	struct pci_iommu_arena *sg_pci;
	struct pci_iommu_arena *sg_isa;

	void *sysdata;
};

/* Override the logic in pci_scan_bus for skipping already-configured
   bus numbers.  */

#define pcibios_assign_all_busses()	1
#define pcibios_scan_all_fns()		0

#define PCIBIOS_MIN_IO		alpha_mv.min_io_address
#define PCIBIOS_MIN_MEM		alpha_mv.min_mem_address

extern void pcibios_set_master(struct pci_dev *dev);

extern inline void pcibios_penalize_isa_irq(int irq)
{
	/* We don't do dynamic PCI IRQ allocation */
}

/* IOMMU controls.  */

/* The PCI address space does not equal the physical memory address space.
   The networking and block device layers use this boolean for bounce buffer
   decisions.  */
#define PCI_DMA_BUS_IS_PHYS  0

/* Allocate and map kernel buffer using consistant mode DMA for PCI
   device.  Returns non-NULL cpu-view pointer to the buffer if
   successful and sets *DMA_ADDRP to the pci side dma address as well,
   else DMA_ADDRP is undefined.  */

extern void *pci_alloc_consistent(struct pci_dev *, size_t, dma_addr_t *);

/* Free and unmap a consistant DMA buffer.  CPU_ADDR and DMA_ADDR must
   be values that were returned from pci_alloc_consistant.  SIZE must
   be the same as what as passed into pci_alloc_consistant.
   References to the memory and mappings assosciated with CPU_ADDR or
   DMA_ADDR past this call are illegal.  */

extern void pci_free_consistent(struct pci_dev *, size_t, void *, dma_addr_t);

/* Map a single buffer of the indicate size for PCI DMA in streaming
   mode.  The 32-bit PCI bus mastering address to use is returned.
   Once the device is given the dma address, the device owns this memory
   until either pci_unmap_single or pci_dma_sync_single is performed.  */

extern dma_addr_t pci_map_single(struct pci_dev *, void *, size_t, int);

/* Likewise, but for a page instead of an address.  */
extern dma_addr_t pci_map_page(struct pci_dev *, struct page *,
			       unsigned long, size_t, int);

/* Unmap a single streaming mode DMA translation.  The DMA_ADDR and
   SIZE must match what was provided for in a previous pci_map_single
   call.  All other usages are undefined.  After this call, reads by
   the cpu to the buffer are guarenteed to see whatever the device
   wrote there.  */

extern void pci_unmap_single(struct pci_dev *, dma_addr_t, size_t, int);
extern void pci_unmap_page(struct pci_dev *, dma_addr_t, size_t, int);

/* pci_unmap_{single,page} is not a nop, thus... */
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

/* Map a set of buffers described by scatterlist in streaming mode for
   PCI DMA.  This is the scather-gather version of the above
   pci_map_single interface.  Here the scatter gather list elements
   are each tagged with the appropriate PCI dma address and length.
   They are obtained via sg_dma_{address,length}(SG).

   NOTE: An implementation may be able to use a smaller number of DMA
   address/length pairs than there are SG table elements.  (for
   example via virtual mapping capabilities) The routine returns the
   number of addr/length pairs actually used, at most nents.

   Device ownership issues as mentioned above for pci_map_single are
   the same here.  */

extern int pci_map_sg(struct pci_dev *, struct scatterlist *, int, int);

/* Unmap a set of streaming mode DMA translations.  Again, cpu read
   rules concerning calls here are the same as for pci_unmap_single()
   above.  */

extern void pci_unmap_sg(struct pci_dev *, struct scatterlist *, int, int);

/* Make physical memory consistant for a single streaming mode DMA
   translation after a transfer.

   If you perform a pci_map_single() but wish to interrogate the
   buffer using the cpu, yet do not wish to teardown the PCI dma
   mapping, you must call this function before doing so.  At the next
   point you give the PCI dma address back to the card, the device
   again owns the buffer.  */

static inline void
pci_dma_sync_single(struct pci_dev *dev, dma_addr_t dma_addr, long size,
		    int direction)
{
	/* Nothing to do.  */
}

/* Make physical memory consistant for a set of streaming mode DMA
   translations after a transfer.  The same as pci_dma_sync_single but
   for a scatter-gather list, same rules and usage.  */

static inline void
pci_dma_sync_sg(struct pci_dev *dev, struct scatterlist *sg, int nents,
	        int direction)
{
	/* Nothing to do.  */
}

/* Return whether the given PCI device DMA address mask can
   be supported properly.  For example, if your device can
   only drive the low 24-bits during PCI bus mastering, then
   you would pass 0x00ffffff as the mask to this function.  */

extern int pci_dma_supported(struct pci_dev *hwdev, u64 mask);

/* True if the machine supports DAC addressing, and DEV can
   make use of it given MASK.  */
extern int pci_dac_dma_supported(struct pci_dev *hwdev, u64 mask);

/* Convert to/from DAC dma address and struct page.  */
extern dma64_addr_t pci_dac_page_to_dma(struct pci_dev *, struct page *, unsigned long, int);
extern struct page *pci_dac_dma_to_page(struct pci_dev *, dma64_addr_t);
extern unsigned long pci_dac_dma_to_offset(struct pci_dev *, dma64_addr_t);

static __inline__ void
pci_dac_dma_sync_single(struct pci_dev *pdev, dma64_addr_t dma_addr, size_t len, int direction)
{
	/* Nothing to do. */
}

/* Return the index of the PCI controller for device PDEV. */
extern int pci_controller_num(struct pci_dev *pdev);
#endif /* __KERNEL__ */

/* Values for the `which' argument to sys_pciconfig_iobase.  */
#define IOBASE_HOSE		0
#define IOBASE_SPARSE_MEM	1
#define IOBASE_DENSE_MEM	2
#define IOBASE_SPARSE_IO	3
#define IOBASE_DENSE_IO		4
#define IOBASE_ROOT_BUS		5
#define IOBASE_FROM_HOSE	0x10000

#endif /* __ALPHA_PCI_H */
