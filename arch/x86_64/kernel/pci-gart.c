/*
 * Dynamic DMA mapping support for AMD Hammer.
 * 
 * Use the integrated AGP GART in the Hammer northbridge as an IOMMU for PCI.
 * This allows to use PCI devices that only support 32bit addresses on systems
 * with more than 4GB. 
 *
 * See Documentation/DMA-mapping.txt for the interface specification.
 * 
 * Copyright 2002 Andi Kleen, SuSE Labs.
 * $Id: pci-gart.c,v 1.32 2004/02/27 18:30:19 ak Exp $
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/agp_backend.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>
#include <linux/module.h>
#include <asm/io.h>
#include <asm/mtrr.h>
#include <asm/bitops.h>
#include <asm/pgtable.h>
#include <asm/proto.h>
#include "pci-x86_64.h"

unsigned long iommu_bus_base;	/* GART remapping area (physical) */
static unsigned long iommu_size; 	/* size of remapping area bytes */
static unsigned long iommu_pages;	/* .. and in pages */

u32 *iommu_gatt_base; 		/* Remapping table */

int no_iommu; 
static int no_agp; 
#ifdef CONFIG_IOMMU_DEBUG
int force_mmu = 1;
#else
int force_mmu = 0;
#endif
int iommu_fullflush = 1;

extern int fallback_aper_order;
extern int fallback_aper_force;

/* Allocation bitmap for the remapping area */ 
static spinlock_t iommu_bitmap_lock = SPIN_LOCK_UNLOCKED;
static unsigned long *iommu_gart_bitmap; /* guarded by iommu_bitmap_lock */

#define GPTE_VALID    1
#define GPTE_COHERENT 2
#define GPTE_ENCODE(x) (((x) & 0xfffff000) | (((x) >> 32) << 4) | GPTE_VALID | GPTE_COHERENT)
#define GPTE_DECODE(x) (((x) & 0xfffff000) | (((u64)(x) & 0xff0) << 28))

#define for_all_nb(dev) \
	pci_for_each_dev(dev) \
		if (dev->vendor == PCI_VENDOR_ID_AMD && dev->device==0x1103 &&\
		    dev->bus->number == 0 && PCI_FUNC(dev->devfn) == 3 && \
		    (PCI_SLOT(dev->devfn) >= 24) && (PCI_SLOT(dev->devfn) <= 31))

#define EMERGENCY_PAGES 32 /* = 128KB */ 

#ifdef CONFIG_AGP
extern int agp_init(void);
#define AGPEXTERN extern
#else
#define AGPEXTERN
#endif

/* backdoor interface to AGP driver */
AGPEXTERN int agp_memory_reserved;
AGPEXTERN __u32 *agp_gatt_table;

static unsigned long next_bit;  /* protected by iommu_bitmap_lock */

static struct pci_dev *northbridges[NR_CPUS + 1];
static u32 northbridge_flush_word[NR_CPUS + 1];
static int need_flush; 	/* global flush state. set for each gart wrap */
static unsigned long alloc_iommu(int size) 
{ 	
	unsigned long offset, flags;

	spin_lock_irqsave(&iommu_bitmap_lock, flags);	
	offset = find_next_zero_string(iommu_gart_bitmap,next_bit,iommu_pages,size);
	if (offset == -1) {
		need_flush = 1;
	       	offset = find_next_zero_string(iommu_gart_bitmap,0,next_bit,size);
	}
	if (offset != -1) { 
		set_bit_string(iommu_gart_bitmap, offset, size); 
		next_bit = offset+size; 
		if (next_bit >= iommu_pages) { 
			need_flush = 1;
			next_bit = 0;
	} 
	} 
	if (iommu_fullflush)
		need_flush = 1;
	spin_unlock_irqrestore(&iommu_bitmap_lock, flags);      
	return offset;
} 

static void free_iommu(unsigned long offset, int size)
{ 
	unsigned long flags;
	spin_lock_irqsave(&iommu_bitmap_lock, flags);
	clear_bit_string(iommu_gart_bitmap, offset, size);
	spin_unlock_irqrestore(&iommu_bitmap_lock, flags);
} 


/* 
 * Use global flush state to avoid races with multiple flushers.
 */
static void __flush_gart(void)
{ 
	unsigned long flags;
	int flushed = 0;
	int i;

	spin_lock_irqsave(&iommu_bitmap_lock, flags);
	/* recheck flush count inside lock */
	if (need_flush) { 
		for (i = 0; northbridges[i]; i++) { 
			u32 w;
			pci_write_config_dword(northbridges[i], 0x9c, 
					       northbridge_flush_word[i] | 1); 
			do { 
				pci_read_config_dword(northbridges[i], 0x9c, &w);
			} while (w & 1);
			flushed++;
		} 
		if (!flushed) 
			printk("nothing to flush?\n");
		need_flush = 0;
	} 
	spin_unlock_irqrestore(&iommu_bitmap_lock, flags);
} 

static inline void flush_gart(void)
{ 
	if (need_flush)
		__flush_gart();
} 

void *pci_alloc_consistent(struct pci_dev *hwdev, size_t size,
			   dma_addr_t *dma_handle)
{
	void *memory;
	int gfp = GFP_ATOMIC;
	int i;
	unsigned long iommu_page;

	if (hwdev == NULL || hwdev->dma_mask < 0xffffffff || no_iommu)
		gfp |= GFP_DMA;

	/* 
	 * First try to allocate continuous and use directly if already 
	 * in lowmem. 
	 */ 
	size = round_up(size, PAGE_SIZE); 
	memory = (void *)__get_free_pages(gfp, get_order(size));
	if (memory == NULL) {
		return NULL; 
	} else {
		int high = 0, mmu;
		if (((unsigned long)virt_to_bus(memory) + size) > 0xffffffffUL)
			high = 1;
		mmu = high;
		if (force_mmu && !(gfp & GFP_DMA)) 
			mmu = 1;
		if (no_iommu) { 
			if (high) goto error;
			mmu = 0; 
		} 	
		memset(memory, 0, size); 
		if (!mmu) { 
			*dma_handle = virt_to_bus(memory);
			return memory;
		}
	} 

	size >>= PAGE_SHIFT;

	iommu_page = alloc_iommu(size);
	if (iommu_page == -1)
		goto error; 

   	/* Fill in the GATT, allocating pages as needed. */
	for (i = 0; i < size; i++) { 
		unsigned long phys_mem; 
		void *mem = memory + i*PAGE_SIZE;
		if (i > 0) 
			atomic_inc(&virt_to_page(mem)->count); 
		phys_mem = virt_to_phys(mem); 
		BUG_ON(phys_mem & ~PHYSICAL_PAGE_MASK); 
		iommu_gatt_base[iommu_page + i] = GPTE_ENCODE(phys_mem); 
	} 

	flush_gart();
	*dma_handle = iommu_bus_base + (iommu_page << PAGE_SHIFT);
	return memory; 
	
 error:
	free_pages((unsigned long)memory, get_order(size)); 
	return NULL; 
}

/* 
 * Unmap consistent memory.
 * The caller must ensure that the device has finished accessing the mapping.
 */
void pci_free_consistent(struct pci_dev *hwdev, size_t size,
			 void *vaddr, dma_addr_t bus)
{
	unsigned long iommu_page;

	size = round_up(size, PAGE_SIZE); 
	if (bus >= iommu_bus_base && bus < iommu_bus_base + iommu_size) { 
		unsigned pages = size >> PAGE_SHIFT;
		iommu_page = (bus - iommu_bus_base) >> PAGE_SHIFT;
		vaddr = __va(GPTE_DECODE(iommu_gatt_base[iommu_page]));
		int i;
		for (i = 0; i < pages; i++) {
			u64 pte = iommu_gatt_base[iommu_page + i];
		BUG_ON((pte & GPTE_VALID) == 0); 
		iommu_gatt_base[iommu_page + i] = 0; 		
	} 
		free_iommu(iommu_page, pages);
	}
	free_pages((unsigned long)vaddr, get_order(size)); 		
}

#ifdef CONFIG_IOMMU_LEAK
/* Debugging aid for drivers that don't free their IOMMU tables */
static void **iommu_leak_tab; 
static int leak_trace;
int iommu_leak_pages = 20; 
extern unsigned long printk_address(unsigned long);
void dump_leak(void)
{
	int i;
	static int dump; 
	if (dump || !iommu_leak_tab) return;
	dump = 1;
	show_stack(NULL);
	/* Very crude. dump some from the end of the table too */ 
	printk("Dumping %d pages from end of IOMMU:\n", iommu_leak_pages); 
	for (i = 0; i < iommu_leak_pages; i+=2) {
		printk("%lu: ", iommu_pages-i);
		printk_address((unsigned long) iommu_leak_tab[iommu_pages-i]);
		printk("%c", (i+1)%2 == 0 ? '\n' : ' '); 
	} 
	printk("\n");
}
#endif

static void iommu_full(struct pci_dev *dev, void *addr, size_t size, int dir)
{
	/* 
	 * Ran out of IOMMU space for this operation. This is very bad.
	 * Unfortunately the drivers cannot handle this operation properly.
	 * Return some non mapped prereserved space in the aperture and 
	 * let the Northbridge deal with it. This will result in garbage
	 * in the IO operation. When the size exceeds the prereserved spa	 * memory corruption will occur or random memory will be DMAed 
	 * out. Hopefully no network devices use single mappings that big.
	 */ 
	
	printk(KERN_ERR 
  "PCI-DMA: Error: ran out out IOMMU space for %p size %lu at device %s[%s]\n",
	       addr,size, dev ? dev->name : "?", dev ? dev->slot_name : "?");

	if (size > PAGE_SIZE*EMERGENCY_PAGES) {
		if (dir == PCI_DMA_FROMDEVICE || dir == PCI_DMA_BIDIRECTIONAL)
			panic("PCI-DMA: Memory will be corrupted\n");
		if (dir == PCI_DMA_TODEVICE || dir == PCI_DMA_BIDIRECTIONAL) 
			panic("PCI-DMA: Random memory will be DMAed\n"); 
	} 

#ifdef CONFIG_IOMMU_LEAK
	dump_leak(); 
#endif
} 

static inline int need_iommu(struct pci_dev *dev, unsigned long addr, size_t size)
{ 
	u64 mask = dev ? dev->dma_mask : 0xffffffff;
	int high = (~mask & (unsigned long)(addr + size)) != 0;
	int mmu = high;
	if (force_mmu) 
		mmu = 1; 
	if (no_iommu) { 
		if (high) 
			panic("pci_map_single: high address but no IOMMU.\n"); 
		mmu = 0; 
	} 	
	return mmu; 
}

dma_addr_t pci_map_single(struct pci_dev *dev, void *addr, size_t size,
			    int dir)
{ 
	unsigned long iommu_page;
	unsigned long phys_mem, bus;
	int i, npages;

	BUG_ON(dir == PCI_DMA_NONE);

#ifdef CONFIG_SWIOTLB
       if (swiotlb)
               return swiotlb_map_single(dev,addr,size,dir);
#endif


	phys_mem = virt_to_phys(addr); 
	if (!need_iommu(dev, phys_mem, size))
		return phys_mem; 

	npages = round_up(size + ((u64)addr & ~PAGE_MASK), PAGE_SIZE) >> PAGE_SHIFT;

	iommu_page = alloc_iommu(npages); 
	if (iommu_page == -1) {
		iommu_full(dev, addr, size, dir); 
		return iommu_bus_base; 
	} 

	phys_mem &= PAGE_MASK;
	for (i = 0; i < npages; i++, phys_mem += PAGE_SIZE) {
		BUG_ON(phys_mem & ~PHYSICAL_PAGE_MASK); 
		
		/* 
		 * Set coherent mapping here to avoid needing to flush
		 * the caches on mapping.
		 */
		iommu_gatt_base[iommu_page + i] = GPTE_ENCODE(phys_mem);

#ifdef CONFIG_IOMMU_LEAK
		/* XXX need eventually caller of pci_map_sg */
		if (iommu_leak_tab) 
			iommu_leak_tab[iommu_page + i] = __builtin_return_address(0); 
#endif
	}
	flush_gart(); 

	bus = iommu_bus_base + iommu_page*PAGE_SIZE; 
	return bus + ((unsigned long)addr & ~PAGE_MASK); 	
} 

/*
 * Free a temporary PCI mapping.
 */ 
void pci_unmap_single(struct pci_dev *hwdev, dma_addr_t dma_addr,
		      size_t size, int direction)
{
	unsigned long iommu_page; 
	int npages;

#ifdef CONFIG_SWIOTLB
       if (swiotlb) {
               swiotlb_unmap_single(hwdev,dma_addr,size,direction);
              return;
       }
#endif


	if (dma_addr < iommu_bus_base + EMERGENCY_PAGES*PAGE_SIZE || 
	    dma_addr >= iommu_bus_base + iommu_size)
		return;
	iommu_page = (dma_addr - iommu_bus_base)>>PAGE_SHIFT;	
	npages = round_up(size + (dma_addr & ~PAGE_MASK), PAGE_SIZE) >> PAGE_SHIFT;
	int i;
	for (i = 0; i < npages; i++) { 
		iommu_gatt_base[iommu_page + i] = 0; 
#ifdef CONFIG_IOMMU_LEAK
		if (iommu_leak_tab)
			iommu_leak_tab[iommu_page + i] = 0; 
#endif
	}
	free_iommu(iommu_page, npages);
}

EXPORT_SYMBOL(pci_map_single);
EXPORT_SYMBOL(pci_unmap_single);

static __init unsigned long check_iommu_size(unsigned long aper, u64 aper_size)
{ 
	unsigned long a; 
	if (!iommu_size) { 
		iommu_size = aper_size; 
		if (!no_agp) 
			iommu_size /= 2; 
	} 

	a = aper + iommu_size; 
	iommu_size -= round_up(a, LARGE_PAGE_SIZE) - a;

	if (iommu_size < 64*1024*1024) 
		printk(KERN_WARNING
  "PCI-DMA: Warning: Small IOMMU %luMB. Consider increasing the AGP aperture in BIOS\n",iommu_size>>20); 
	
	return iommu_size;
} 

static __init unsigned read_aperture(struct pci_dev *dev, u32 *size) 
{ 
	unsigned aper_size = 0, aper_base_32;
	u64 aper_base;
	unsigned aper_order;

	pci_read_config_dword(dev, 0x94, &aper_base_32); 
	pci_read_config_dword(dev, 0x90, &aper_order);
	aper_order = (aper_order >> 1) & 7;	

	aper_base = aper_base_32 & 0x7fff; 
	aper_base <<= 25;

	aper_size = (32 * 1024 * 1024) << aper_order; 
	if (aper_base + aper_size >= 0xffffffff || !aper_size)
		aper_base = 0;

	*size = aper_size;
	return aper_base;
} 

/* 
 * Private Northbridge GATT initialization in case we cannot use the
 * AGP driver for some reason.  
 */
static __init int init_k8_gatt(agp_kern_info *info)
{ 
	struct pci_dev *dev;
	void *gatt;
	unsigned aper_base, new_aper_base;
	unsigned aper_size, gatt_size, new_aper_size;

	aper_size = aper_base = info->aper_size = 0;
	for_all_nb(dev) { 
		new_aper_base = read_aperture(dev, &new_aper_size); 
		if (!new_aper_base) 
	goto nommu; 

		if (!aper_base) { 
			aper_size = new_aper_size;
			aper_base = new_aper_base;
	}
		if (aper_size != new_aper_size || aper_base != new_aper_base) 
		goto nommu;
	} 
	if (!aper_base)
		goto nommu; 
	info->aper_base = aper_base;
	info->aper_size = aper_size>>20; 

	gatt_size = (aper_size >> PAGE_SHIFT) * sizeof(u32); 
	gatt = (void *)__get_free_pages(GFP_KERNEL, get_order(gatt_size)); 
	if (!gatt) 
		panic("Cannot allocate GATT table"); 
	memset(gatt, 0, gatt_size); 
	change_page_attr(virt_to_page(gatt), gatt_size/PAGE_SIZE, PAGE_KERNEL_NOCACHE);
	agp_gatt_table = gatt;

	for_all_nb(dev) { 
		u32 ctl; 
		u32 gatt_reg; 

		gatt_reg = __pa(gatt) >> 12; 
		gatt_reg <<= 4; 
		pci_write_config_dword(dev, 0x98, gatt_reg);
		pci_read_config_dword(dev, 0x90, &ctl); 

		ctl |= 1;
		ctl &= ~((1<<4) | (1<<5));

		pci_write_config_dword(dev, 0x90, ctl); 
	}
	flush_gart(); 
	
		
	printk("PCI-DMA: aperture base @ %x size %u KB\n", aper_base, aper_size>>10); 
		return 0;

 nommu:
	/* XXX: reject 0xffffffff mask now in pci mapping functions */
	if (end_pfn >= 0xffffffff>>PAGE_SHIFT)
		printk(KERN_ERR "PCI-DMA: More than 4GB of RAM and no IOMMU\n"
	       KERN_ERR "PCI-DMA: 32bit PCI IO may malfunction."); 
	return -1; 
} 

void __init pci_iommu_init(void)
{ 
	agp_kern_info info;
	unsigned long aper_size;
	unsigned long iommu_start;

#ifndef CONFIG_AGP
	no_agp = 1; 
#else
	no_agp = no_agp || (agp_init() < 0) || (agp_copy_info(&info) < 0); 
#endif	

	if (swiotlb) { 
		no_iommu = 1;
		printk(KERN_INFO "PCI-DMA: Using SWIOTLB\n"); 
		return; 
	} 


	if (no_iommu || (!force_mmu && end_pfn < 0xffffffff>>PAGE_SHIFT) || !iommu_aperture) { 
		printk(KERN_INFO "PCI-DMA: Disabling IOMMU.\n"); 
		no_iommu = 1;
		return;
	}

	if (no_agp) { 
		int err = -1;
		printk(KERN_INFO "PCI-DMA: Disabling AGP.\n");
		no_agp = 1;
		if (force_mmu || end_pfn >= 0xffffffff>>PAGE_SHIFT)
			err = init_k8_gatt(&info);
		if (err < 0) { 
			printk(KERN_INFO "PCI-DMA: Disabling IOMMU.\n"); 
			no_iommu = 1;
			return; 
		}
	} 
	
	aper_size = info.aper_size * 1024 * 1024;
	iommu_size = check_iommu_size(info.aper_base, aper_size); 
	iommu_pages = iommu_size >> PAGE_SHIFT; 

	iommu_gart_bitmap = (void*)__get_free_pages(GFP_KERNEL, 
						    get_order(iommu_pages/8)); 
	if (!iommu_gart_bitmap) 
		panic("Cannot allocate iommu bitmap\n"); 
	memset(iommu_gart_bitmap, 0, iommu_pages/8);

#ifdef CONFIG_IOMMU_LEAK
	if (leak_trace) { 
		iommu_leak_tab = (void *)__get_free_pages(GFP_KERNEL, 
				  get_order(iommu_pages*sizeof(void *)));
		if (iommu_leak_tab) 
			memset(iommu_leak_tab, 0, iommu_pages * 8); 
		else
			printk("PCI-DMA: Cannot allocate leak trace area\n"); 
	} 
#endif

	/* 
	 * Out of IOMMU space handling.
	 * Reserve some invalid pages at the beginning of the GART. 
	 */ 
	set_bit_string(iommu_gart_bitmap, 0, EMERGENCY_PAGES); 

	agp_memory_reserved = iommu_size;	
	printk(KERN_INFO"PCI-DMA: Reserving %luMB of IOMMU area in the AGP aperture\n",
	       iommu_size>>20); 

	iommu_start = aper_size - iommu_size;	
	iommu_bus_base = info.aper_base + iommu_start; 
	iommu_gatt_base = agp_gatt_table + (iommu_start>>PAGE_SHIFT);
	bad_dma_address = iommu_bus_base;

	/* 
         * Unmap the IOMMU part of the GART. The alias of the page is always mapped
	 * with cache enabled and there is no full cache coherency across the GART
	 * remapping. The unmapping avoids automatic prefetches from the CPU 
	 * allocating cache lines in there. All CPU accesses are done via the 
	 * direct mapping to the backing memory. The GART address is only used by PCI 
	 * devices. 
	 */
	clear_kernel_mapping((unsigned long)__va(iommu_bus_base), iommu_size);

	struct pci_dev *dev;
	for_all_nb(dev) {
		u32 flag; 
		int cpu = PCI_SLOT(dev->devfn) - 24;
		if (cpu >= NR_CPUS)
			continue;
		northbridges[cpu] = dev;

		pci_read_config_dword(dev, 0x9c, &flag); /* cache flush word */
		northbridge_flush_word[cpu] = flag; 
	}

	asm volatile("wbinvd" ::: "memory");

	flush_gart();
} 

/* iommu=[size][,noagp][,off][,force][,noforce][,leak][,memaper[=order]]
   size  set size of iommu (in bytes) 
   noagp don't initialize the AGP driver and use full aperture.
   off   don't use the IOMMU
   leak  turn on simple iommu leak tracing (only when CONFIG_IOMMU_LEAK is on)
   memaper[=order] allocate an own aperture over RAM with size 32MB^order.
   noforce don't force IOMMU usage. Default
   force  Force IOMMU for all devices.
   nofullflush use optimized IOMMU flushing (may break on some devices).
               default off.
*/
__init int iommu_setup(char *opt) 
{ 
    int arg;
    char *p = opt;
    
    for (;;) { 
	    if (!memcmp(p,"noagp", 5))
		    no_agp = 1; 
	    if (!memcmp(p,"off", 3))
		    no_iommu = 1;
	    if (!memcmp(p,"force", 5))
		    force_mmu = 1;
	    if (!memcmp(p,"noforce", 7))
		    force_mmu = 0;
	    if (!memcmp(p,"nofullflush", 11))
		    iommu_fullflush = 0;
	    if (!memcmp(p, "memaper", 7)) { 
		    fallback_aper_force = 1; 
		    p += 7; 
		    if (*p == '=') { 
			    ++p;
			    if (get_option(&p, &arg))
			    fallback_aper_order = arg;
	    } 
	    } 
#ifdef CONFIG_IOMMU_LEAK
	    if (!memcmp(p,"leak", 4)) { 
		    leak_trace = 1;
		    p += 4; 
		    if (*p == '=') ++p;
		    if (isdigit(*p) && get_option(&p, &arg))
			    iommu_leak_pages = arg;
	    } else
#endif
	    if (isdigit(*p) && get_option(&p, &arg)) 
		    iommu_size = arg;
	    do {
		    if (*p == ' ' || *p == 0) 
			    return 0; 
	    } while (*p++ != ','); 
    }	
    return 1;
} 

