#ifndef __ASM_PARISC_PCI_H
#define __ASM_PARISC_PCI_H

#include <asm/scatterlist.h>

/*
** HP PCI platforms generally support multiple bus adapters.
**    (workstations 1-~4, servers 2-~32)
**
** Newer platforms number the busses across PCI bus adapters *sparsely*.
** E.g. 0, 8, 16, ...
**
** Under a PCI bus, most HP platforms support PPBs up to two or three
** levels deep. See "Bit3" product line. 
*/
#define PCI_MAX_BUSSES	256

/* [soapbox on]
** Who the hell can develop stuff without ASSERT or VASSERT?
** No one understands all the modules across all platforms.
** For linux add another dimension - processor architectures.
**
** This should be a standard/global macro used liberally
** in all code. Every respectable engineer I know in HP
** would support this argument. - grant
** [soapbox off]
*/
#ifdef PCI_DEBUG
#define ASSERT(expr) \
	if(!(expr)) { \
		printk( "\n" __FILE__ ":%d: Assertion " #expr " failed!\n",__LINE__); \
		panic(#expr); \
	}
#else
#define ASSERT(expr)
#endif


/*
** pci_hba_data (aka H2P_OBJECT in HP/UX)
**
** This is the "common" or "base" data structure which HBA drivers
** (eg Dino or LBA) are required to place at the top of their own
** dev->sysdata structure.  I've heard this called "C inheritance" too.
**
** Data needed by pcibios layer belongs here.
*/
struct pci_hba_data {
	unsigned long	base_addr;	/* aka Host Physical Address */
	const struct parisc_device *dev; /* device from PA bus walk */
	struct pci_bus *hba_bus;	/* primary PCI bus below HBA */
	int		hba_num;	/* I/O port space access "key" */
	struct resource bus_num;	/* PCI bus numbers */
	struct resource io_space;	/* PIOP */
	struct resource lmmio_space;	/* bus addresses < 4Gb */
	struct resource elmmio_space;	/* additional bus addresses < 4Gb */
	unsigned long   lmmio_space_offset;  /* CPU view - PCI view */
	void *          iommu;          /* IOMMU this device is under */
	/* REVISIT - spinlock to protect resources? */
};

#define HBA_DATA(d)		((struct pci_hba_data *) (d))

/* 
** We support 2^16 I/O ports per HBA.  These are set up in the form
** 0xbbxxxx, where bb is the bus number and xxxx is the I/O port
** space address.
*/
#define HBA_PORT_SPACE_BITS	16

#define HBA_PORT_BASE(h)	((h) << HBA_PORT_SPACE_BITS)
#define HBA_PORT_SPACE_SIZE	(1UL << HBA_PORT_SPACE_BITS)

#define PCI_PORT_HBA(a)		((a) >> HBA_PORT_SPACE_BITS)
#define PCI_PORT_ADDR(a)	((a) & (HBA_PORT_SPACE_SIZE - 1))

/*
** Convert between PCI (IO_VIEW) addresses and processor (PA_VIEW) addresses.
** Note that we currently support only LMMIO.
*/
#define PCI_BUS_ADDR(hba,a)	((a) - hba->lmmio_space_offset)
#define PCI_HOST_ADDR(hba,a)	((a) + hba->lmmio_space_offset)

/* The PCI address space equals the physical memory address space.
   The networking and block device layers use this boolean for bounce buffer
   decisions.  */
#define PCI_DMA_BUS_IS_PHYS  1

/*
** KLUGE: linux/pci.h include asm/pci.h BEFORE declaring struct pci_bus
** (This eliminates some of the warnings).
*/
struct pci_bus;
struct pci_dev;

/*
** Most PCI devices (eg Tulip, NCR720) also export the same registers
** to both MMIO and I/O port space.  Due to poor performance of I/O Port
** access under HP PCI bus adapters, strongly reccomend use of MMIO
** address space.
**
** While I'm at it more PA programming notes:
**
** 1) MMIO stores (writes) are posted operations. This means the processor
**    gets an "ACK" before the write actually gets to the device. A read
**    to the same device (or typically the bus adapter above it) will
**    force in-flight write transaction(s) out to the targeted device
**    before the read can complete.
**
** 2) The Programmed I/O (PIO) data may not always be strongly ordered with
**    respect to DMA on all platforms. Ie PIO data can reach the processor
**    before in-flight DMA reaches memory. Since most SMP PA platforms
**    are I/O coherent, it generally doesn't matter...but sometimes
**    it does.
**
** I've helped device driver writers debug both types of problems.
*/
struct pci_port_ops {
	  u8 (*inb)  (struct pci_hba_data *hba, u16 port);
	 u16 (*inw)  (struct pci_hba_data *hba, u16 port);
	 u32 (*inl)  (struct pci_hba_data *hba, u16 port);
	void (*outb) (struct pci_hba_data *hba, u16 port,  u8 data);
	void (*outw) (struct pci_hba_data *hba, u16 port, u16 data);
	void (*outl) (struct pci_hba_data *hba, u16 port, u32 data);
};


struct pci_bios_ops {
	void (*init)(void);
	void (*fixup_bus)(struct pci_bus *bus);
};

/*
** See Documentation/DMA-mapping.txt
*/
struct pci_dma_ops {
	int  (*dma_supported)(struct pci_dev *dev, u64 mask);
	void *(*alloc_consistent)(struct pci_dev *dev, size_t size, dma_addr_t *iova);
	void (*free_consistent)(struct pci_dev *dev, size_t size, void *vaddr, dma_addr_t iova);
	dma_addr_t (*map_single)(struct pci_dev *dev, void *addr, size_t size, int direction);
	void (*unmap_single)(struct pci_dev *dev, dma_addr_t iova, size_t size, int direction);
	int  (*map_sg)(struct pci_dev *dev, struct scatterlist *sg, int nents, int direction);
	void (*unmap_sg)(struct pci_dev *dev, struct scatterlist *sg, int nhwents, int direction);
	void (*dma_sync_single)(struct pci_dev *dev, dma_addr_t iova, size_t size, int direction);
	void (*dma_sync_sg)(struct pci_dev *dev, struct scatterlist *sg, int nelems, int direction);
};


/*
** We could live without the hppa_dma_ops indirection if we didn't want
** to support 4 different coherent dma models with one binary (they will
** someday be loadable modules):
**     I/O MMU        consistent method           dma_sync behavior
**  =============   ======================       =======================
**  a) PA-7x00LC    uncachable host memory          flush/purge
**  b) U2/Uturn      cachable host memory              NOP
**  c) Ike/Astro     cachable host memory              NOP
**  d) EPIC/SAGA     memory on EPIC/SAGA         flush/reset DMA channel
**
** PA-7[13]00LC processors have a GSC bus interface and no I/O MMU.
**
** Systems (eg PCX-T workstations) that don't fall into the above
** categories will need to modify the needed drivers to perform
** flush/purge and allocate "regular" cacheable pages for everything.
*/

extern struct pci_dma_ops *hppa_dma_ops;

#ifdef CONFIG_PA11
extern struct pci_dma_ops pcxl_dma_ops;
extern struct pci_dma_ops pcx_dma_ops;
#endif

/*
** Oops hard if we haven't setup hppa_dma_ops by the time the first driver
** attempts to initialize.
** Since panic() is a (void)(), pci_dma_panic() is needed to satisfy
** the (int)() required by pci_dma_supported() interface.
*/
static inline int pci_dma_panic(char *msg)
{
	extern void panic(const char *, ...);	/* linux/kernel.h */
	panic(msg);
	/* NOTREACHED */
	return -1;
}

#define pci_dma_supported(p, m)	( \
	(NULL == hppa_dma_ops) \
	?  pci_dma_panic("Dynamic DMA support missing...OOPS!\n(Hint: was Astro/Ike/U2/Uturn not claimed?)\n") \
	: hppa_dma_ops->dma_supported(p,m) \
)

#define pci_alloc_consistent(p, s, a)	hppa_dma_ops->alloc_consistent(p,s,a)
#define pci_free_consistent(p, s, v, a)	hppa_dma_ops->free_consistent(p,s,v,a)
#define pci_map_single(p, v, s, d)	hppa_dma_ops->map_single(p, v, s, d)
#define pci_unmap_single(p, a, s, d)	hppa_dma_ops->unmap_single(p, a, s, d)
#define pci_map_sg(p, sg, n, d)		hppa_dma_ops->map_sg(p, sg, n, d)
#define pci_unmap_sg(p, sg, n, d)	hppa_dma_ops->unmap_sg(p, sg, n, d)

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

/* For U2/Astro/Ike based platforms (which are fully I/O coherent)
** dma_sync is a NOP. Let's keep the performance path short here.
*/
#define pci_dma_sync_single(p, a, s, d)	{ if (hppa_dma_ops->dma_sync_single) \
	hppa_dma_ops->dma_sync_single(p, a, s, d); \
	}
#define pci_dma_sync_sg(p, sg, n, d)	{ if (hppa_dma_ops->dma_sync_sg) \
	hppa_dma_ops->dma_sync_sg(p, sg, n, d); \
	}

/* No highmem on parisc, plus we have an IOMMU, so mapping pages is easy. */
#define pci_map_page(dev, page, off, size, dir) \
	pci_map_single(dev, (page_address(page) + (off)), size, dir)
#define pci_unmap_page(dev,addr,sz,dir) pci_unmap_single(dev,addr,sz,dir)

/* Don't support DAC yet. */
#define pci_dac_dma_supported(pci_dev, mask)	(0)

/*
** Stuff declared in arch/parisc/kernel/pci.c
*/
extern struct pci_port_ops *pci_port;
extern struct pci_bios_ops *pci_bios;
extern int pci_post_reset_delay;	/* delay after de-asserting #RESET */
extern int pci_hba_count;
extern struct pci_hba_data *parisc_pci_hba[];

#ifdef CONFIG_PCI
extern void pcibios_register_hba(struct pci_hba_data *);
extern void pcibios_set_master(struct pci_dev *);
extern void pcibios_assign_unassigned_resources(struct pci_bus *);
#else
extern inline void pcibios_register_hba(struct pci_hba_data *x)
{
}
#endif

/*
** used by drivers/pci/pci.c:pci_do_scan_bus()
**   0 == check if bridge is numbered before re-numbering.
**   1 == pci_do_scan_bus() should automatically number all PCI-PCI bridges.
**
** REVISIT:
**   To date, only alpha sets this to one. We'll need to set this
**   to zero for legacy platforms and one for PAT platforms.
*/
#define pcibios_assign_all_busses()     (pdc_type == PDC_TYPE_PAT)
#define pcibios_scan_all_fns()		0

#define PCIBIOS_MIN_IO          0x10
#define PCIBIOS_MIN_MEM         0x1000 /* NBPG - but pci/setup-res.c dies */

/* Return the index of the PCI controller for device PDEV. */
#define pci_controller_num(PDEV)	(0)

#define GET_IOC(dev) ((struct ioc *)(HBA_DATA(dev->sysdata)->iommu))

#ifdef CONFIG_IOMMU_CCIO
struct parisc_device;
struct ioc;
void * ccio_get_iommu(const struct parisc_device *dev);
struct pci_dev * ccio_get_fake(const struct parisc_device *dev);
int ccio_request_resource(const struct parisc_device *dev,
		struct resource *res);
int ccio_allocate_resource(const struct parisc_device *dev,
		struct resource *res, unsigned long size,
		unsigned long min, unsigned long max, unsigned long align,
		void (*alignf)(void *, struct resource *, unsigned long, unsigned long),
		void *alignf_data);
#else /* !CONFIG_IOMMU_CCIO */
#define ccio_get_iommu(dev) NULL
#define ccio_get_fake(dev) NULL
#define ccio_request_resource(dev, res) request_resource(&iomem_resource, res)
#define ccio_allocate_resource(dev, res, size, min, max, align, alignf, data) \
		allocate_resource(&iomem_resource, res, size, min, max, \
				align, alignf, data)
#endif /* !CONFIG_IOMMU_CCIO */

#ifdef CONFIG_IOMMU_SBA
struct parisc_device;
void * sba_get_iommu(struct parisc_device *dev);
#endif

#endif /* __ASM_PARISC_PCI_H */
