/* 
 * Firmware replacement code.
 * 
 * Work around broken BIOSes that don't set an aperture or only set the
 * aperture in the AGP bridge. 
 * If all fails map the aperture over some low memory.  This is cheaper than 
 * doing bounce buffering. The memory is lost. This is done at early boot 
 * because only the bootmem allocator can allocate 32+MB. 
 * 
 * Copyright 2002 Andi Kleen, SuSE Labs.
 * $Id: aperture.c,v 1.8 2004/02/27 18:30:19 ak Exp $
 */
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/bootmem.h>
#include <linux/mmzone.h>
#include <linux/pci_ids.h>
#include <linux/pci.h>
#include <linux/bitops.h>
#include <asm/e820.h>
#include <asm/io.h>
#include <asm/proto.h>
#include <asm/pci-direct.h>

int fallback_aper_order __initdata = 1; /* 64MB */
int fallback_aper_force __initdata = 0; 

int iommu_aperture;

extern int no_iommu, force_mmu;

/* This code runs before the PCI subsystem is initialized, so just 
   access the northbridge directly. */

#define NB_ID_3 (PCI_VENDOR_ID_AMD | (0x1103<<16))

static u32 __init allocate_aperture(void) 
{
#ifdef CONFIG_DISCONTIGMEM
	pg_data_t *nd0 = NODE_DATA(0);
#else
	pg_data_t *nd0 = &contig_page_data;
#endif	
	u32 aper_size;
	void *p; 

	if (fallback_aper_order > 7) 
		fallback_aper_order = 7; 
	aper_size = (32 * 1024 * 1024) << fallback_aper_order; 

	/* 
         * Aperture has to be naturally aligned. This means an 2GB aperture won't 
         * have much chances to find a place in the lower 4GB of memory. Unfortunately 
         * we cannot move it up because that would make the IOMMU useless.
	 */
	p = __alloc_bootmem_node(nd0, aper_size, aper_size, 0); 
	if (!p || __pa(p)+aper_size > 0xffffffff) {
		printk("Cannot allocate aperture memory hole (%p,%uK)\n",
		       p, aper_size>>10);
		if (p)
			free_bootmem_generic((unsigned long)p, aper_size); 
		return 0;
	}
	printk("Mapping aperture over %d KB of RAM @ %lx\n",  
	       aper_size >> 10, __pa(p)); 
	return (u32)__pa(p); 
}

static int __init aperture_valid(char *name, u64 aper_base, u32 aper_size) 
{ 
	if (!aper_base) 
		return 0;
	if (aper_size < 64*1024*1024) { 
		printk("Aperture from %s too small (%d MB)\n", name, aper_size>>20); 
		return 0;
	}
	if (aper_base + aper_size >= 0xffffffff) { 
		printk("Aperture from %s beyond 4GB. Ignoring.\n",name);
		return 0; 
	}
	if (e820_mapped(aper_base, aper_base + aper_size, E820_RAM)) {  
		printk("Aperture from %s pointing to e820 RAM. Ignoring.\n",name);
		return 0; 
	} 
	return 1;
} 

/* Find a PCI capability */ 
static __u32 __init find_cap(int num, int slot, int func, int cap) 
{ 
	if (!(read_pci_config_16(num,slot,func,PCI_STATUS) & PCI_STATUS_CAP_LIST))
		return 0;
	u8 pos = read_pci_config_byte(num,slot,func,PCI_CAPABILITY_LIST);
	int bytes;
	for (bytes = 0; bytes < 48 && pos >= 0x40; bytes++) { 
		pos &= ~3; 
		u8 id = read_pci_config_byte(num,slot,func,pos+PCI_CAP_LIST_ID); 
		if (id == 0xff)
			break;
		if (id == cap) 
			return pos; 
		pos = read_pci_config_byte(num,slot,func,pos+PCI_CAP_LIST_NEXT); 
	} 
	return 0;
} 

/* Read a standard AGPv3 bridge header */
static __u32 __init read_agp(int num, int slot, int func, int cap, u32 *order)
{ 
	printk("AGP bridge at %02x:%02x:%02x\n", num, slot, func); 
	u32 apsizereg = read_pci_config_16(num,slot,func, cap + 0x14);

	if (apsizereg == 0xffffffff) {
		printk("APSIZE in AGP bridge unreadable\n");
		return 0;
	}

	u32 apsize = apsizereg & 0xfff;
	/* Some BIOS use weird encodings not in the AGPv3 table. */
	if (apsize & 0xff) 
		apsize |= 0xf00; 
	int nbits = hweight16(apsize);
	*order = 7 - nbits;
	if ((int)*order < 0) /* < 32MB */
		*order = 0;
	
	u32 aper_low = read_pci_config(num,slot,func, 0x10); 
	u32 aper_hi = read_pci_config(num,slot,func,0x14); 
	u64 aper = (aper_low & ~((1<<22)-1)) | ((u64)aper_hi << 32); 

	printk("Aperture from AGP @ %Lx size %u MB (APSIZE %x)\n", 
	       aper, 32 << *order, apsizereg);

	if (!aperture_valid("AGP bridge", aper, (32*1024*1024) << *order))
	    return 0;
	return (u32)aper; 
} 

/* Look for an AGP bridge. Windows only expects the aperture in the
   AGP bridge and some BIOS forget to initialize the Northbridge too.
   Work around this here. 

   Do an PCI bus scan by hand because we're running before the PCI
   subsystem. 

   All K8 AGP bridges are AGPv3 compliant, so we can do this scan
   generically. It's probably overkill to always scan all slots because
   the AGP bridges should be always an own bus on the HT hierarchy, 
   but do it here for future safety. */
static __u32 __init search_agp_bridge(u32 *order, int *valid_agp)
{
	int num, slot, func;

	/* Poor man's PCI discovery */
	for (num = 0; num < 32; num++) { 
		for (slot = 0; slot < 32; slot++) { 
			for (func = 0; func < 8; func++) { 
				u32 class, cap;
				class = read_pci_config(num,slot,func,
							PCI_CLASS_REVISION);
				if (class == 0xffffffff)
					break; 
				
				switch (class >> 16) { 
				case PCI_CLASS_BRIDGE_HOST:
				case PCI_CLASS_BRIDGE_OTHER: /* needed? */
					/* AGP bridge? */
					cap = find_cap(num,slot,func,PCI_CAP_ID_AGP);
					if (!cap)
						break;
					*valid_agp = 1; 
					return read_agp(num,slot,func,cap,order);
				} 
				
				/* No multi-function device? */
				u8 type = read_pci_config_byte(num,slot,func,
							       PCI_HEADER_TYPE);
				if (!(type & 0x80))
					break;
			} 
		} 
	}
	printk("No AGP bridge found\n"); 
	return 0;
}

void __init iommu_hole_init(void) 
{ 
	int fix, num; 
	u32 aper_size, aper_alloc = 0, aper_order;
	u64 aper_base; 
	int valid_agp = 0;

	printk("Checking aperture...\n"); 

	fix = 0;
	for (num = 24; num < 32; num++) {		
		if (read_pci_config(0, num, 3, 0x00) != NB_ID_3) 
			continue;	

		iommu_aperture = 1;

		aper_order = (read_pci_config(0, num, 3, 0x90) >> 1) & 7; 
		aper_size = (32 * 1024 * 1024) << aper_order; 
		aper_base = read_pci_config(0, num, 3, 0x94) & 0x7fff;
		aper_base <<= 25; 

		printk("CPU %d: aperture @ %Lx size %u MB\n", num-24, 
		       aper_base, aper_size>>20);
		
		char name[30];
		sprintf(name, "northbridge cpu %d", num-24); 

		if (!aperture_valid(name, aper_base, aper_size)) { 
			fix = 1; 
			break; 
		} 
	} 

	if (!fix && !fallback_aper_force) 
		return; 

	if (!fallback_aper_force)
		aper_alloc = search_agp_bridge(&aper_order, &valid_agp); 
		
	if (aper_alloc) { 
		/* Got the aperture from the AGP bridge */
	} else if ((!no_iommu && end_pfn >= 0xffffffff>>PAGE_SHIFT) ||
		   force_mmu ||
		   valid_agp || 
		   fallback_aper_force) { 
		/* When there is a AGP bridge in the system assume the
		   user wants to use the AGP driver too and needs an
		   aperture.  However this case (AGP but no good
		   aperture) should only happen with a more broken than
		   usual BIOS, because it would even break Windows. */

	printk("Your BIOS doesn't leave a aperture memory hole\n");
	printk("Please enable the IOMMU option in the BIOS setup\n"); 
		printk("This costs you %d MB of RAM\n", 32 << fallback_aper_order); 
 
		aper_order = fallback_aper_order;
	aper_alloc = allocate_aperture(); 
		if (!aper_alloc) { 
			/* Could disable AGP and IOMMU here, but it's probably
			   not worth it. But the later users cannot deal with
			   bad apertures and turning on the aperture over memory
			   causes very strange problems, so it's better to 
			   panic early. */
			panic("Not enough memory for aperture");
		}
	} else { 
		return; 
	} 

	/* Fix up the north bridges */
	for (num = 24; num < 32; num++) { 		
		if (read_pci_config(0, num, 3, 0x00) != NB_ID_3) 
			continue;	

		/* Don't enable translation yet. That is done later. 
		   Assume this BIOS didn't initialise the GART so 
		   just overwrite all previous bits */ 
		write_pci_config(0, num, 3, 0x90, aper_order<<1); 
		write_pci_config(0, num, 3, 0x94, aper_alloc>>25); 
	} 
} 
