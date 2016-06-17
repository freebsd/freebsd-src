#ifndef _ASM_IA64_PCI_H
#define _ASM_IA64_PCI_H

#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/spinlock.h>

#include <asm/io.h>
#include <asm/scatterlist.h>

/*
 * Can be used to override the logic in pci_scan_bus for skipping already-configured bus
 * numbers - to be used for buggy BIOSes or architectures with incomplete PCI setup by the
 * loader.
 */
#define pcibios_assign_all_busses()     0
#define pcibios_scan_all_fns()		0

#define PCIBIOS_MIN_IO		0x1000
#define PCIBIOS_MIN_MEM		0x10000000

void pcibios_config_init(void);
struct pci_bus *pcibios_scan_root(void *acpi_handle, int segment, int bus);
extern int (*pci_config_read)(int seg, int bus, int dev, int fn, int reg, int len, u32 *value);
extern int (*pci_config_write)(int seg, int bus, int dev, int fn, int reg, int len, u32 value);

struct pci_dev;

/*
 * The PCI address space does equal the physical memory address space.
 * The networking and block device layers use this boolean for bounce
 * buffer decisions.
 */
#define PCI_DMA_BUS_IS_PHYS	(1)

static inline void
pcibios_set_master (struct pci_dev *dev)
{
	/* No special bus mastering setup handling */
}

static inline void
pcibios_penalize_isa_irq (int irq)
{
	/* We don't do dynamic PCI IRQ allocation */
}

#define HAVE_ARCH_PCI_MWI 1
extern int pcibios_set_mwi (struct pci_dev *);

/*
 * Dynamic DMA mapping API.  See Documentation/DMA-mapping.txt for details.
 */
#define pci_alloc_consistent		platform_pci_alloc_consistent
#define pci_free_consistent		platform_pci_free_consistent
#define pci_map_single			platform_pci_map_single
#define pci_unmap_single		platform_pci_unmap_single
#define pci_map_sg			platform_pci_map_sg
#define pci_unmap_sg			platform_pci_unmap_sg
#define pci_dma_sync_single		platform_pci_dma_sync_single
#define pci_dma_sync_sg			platform_pci_dma_sync_sg
#define pci_dma_supported		platform_pci_dma_supported

/* pci_unmap_{single,page} is not a nop, thus... */
#define DECLARE_PCI_UNMAP_ADDR(addr_name)	dma_addr_t addr_name;
#define DECLARE_PCI_UNMAP_LEN(len_name)		__u32 len_name;
#define pci_unmap_addr(ptr, addr_name)		((ptr)->addr_name)
#define pci_unmap_addr_set(ptr, addr_name, val)	(((ptr)->addr_name) = (val))
#define pci_unmap_len(ptr, len_name)		((ptr)->len_name)
#define pci_unmap_len_set(ptr, len_name, val)	(((ptr)->len_name) = (val))

#define pci_map_page(dev,pg,off,size,dir)				\
	pci_map_single((dev), page_address(pg) + (off), (size), (dir))
#define pci_unmap_page(dev,dma_addr,size,dir)				\
	pci_unmap_single((dev), (dma_addr), (size), (dir))

/* The ia64 platform always supports 64-bit addressing. */
#define pci_dac_dma_supported(pci_dev, mask)	(1)

#define pci_dac_page_to_dma(dev,pg,off,dir)	((dma_addr_t) page_to_bus(pg) + (off))
#define pci_dac_dma_to_page(dev,dma_addr)	(virt_to_page(bus_to_virt(dma_addr)))
#define pci_dac_dma_to_offset(dev,dma_addr)	((dma_addr) & ~PAGE_MASK)
#define pci_dac_dma_sync_single(dev,dma_addr,len,dir)	do { /* nothing */ } while (0)

/* Return the PCI domain number */
#define pci_controller_num(pdev)	(PCI_SEGMENT(pdev))

#define sg_dma_address(sg)	((sg)->dma_address)
#define sg_dma_len(sg)		((sg)->dma_length)

#define HAVE_PCI_MMAP
extern int pci_mmap_page_range (struct pci_dev *dev, struct vm_area_struct *vma,
				enum pci_mmap_state mmap_state, int write_combine);

struct pci_window {
	struct resource resource;
	u64 offset;
};

struct pci_controller {
	void *acpi_handle;
	void *iommu;
	int segment;

	unsigned int windows;
	struct pci_window *window;

	void *platform_data;
};

#define PCI_CONTROLLER(dev) ((struct pci_controller *) dev->sysdata)
#define PCI_SEGMENT(dev)    (PCI_CONTROLLER(dev)->segment)

#endif /* _ASM_IA64_PCI_H */
