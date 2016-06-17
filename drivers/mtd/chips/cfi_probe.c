/* 
   Common Flash Interface probe code.
   (C) 2000 Red Hat. GPL'd.
   $Id: cfi_probe.c,v 1.69 2002/05/11 22:13:03 dwmw2 Exp $
*/

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <asm/byteorder.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/interrupt.h>

#include <linux/mtd/map.h>
#include <linux/mtd/cfi.h>
#include <linux/mtd/gen_probe.h>

//#define DEBUG_CFI 

#ifdef DEBUG_CFI
static void print_cfi_ident(struct cfi_ident *);
#endif

static int cfi_probe_chip(struct map_info *map, __u32 base,
			  struct flchip *chips, struct cfi_private *cfi);
static int cfi_chip_setup(struct map_info *map, struct cfi_private *cfi);

struct mtd_info *cfi_probe(struct map_info *map);

/* check for QRY.
   in: interleave,type,mode
   ret: table index, <0 for error
 */
static inline int qry_present(struct map_info *map, __u32 base,
				struct cfi_private *cfi)
{
	int osf = cfi->interleave * cfi->device_type;	// scale factor

	if (cfi_read(map,base+osf*0x10)==cfi_build_cmd('Q',map,cfi) &&
	    cfi_read(map,base+osf*0x11)==cfi_build_cmd('R',map,cfi) &&
	    cfi_read(map,base+osf*0x12)==cfi_build_cmd('Y',map,cfi))
		return 1;	// ok !

	return 0; 	// nothing found
}

static int cfi_probe_chip(struct map_info *map, __u32 base,
			  struct flchip *chips, struct cfi_private *cfi)
{
	int i;
	
	if ((base + 0) >= map->size) {
		printk(KERN_NOTICE
			"Probe at base[0x00](0x%08lx) past the end of the map(0x%08lx)\n",
			(unsigned long)base, map->size -1);
		return 0;
	}
	if ((base + 0xff) >= map->size) {
		printk(KERN_NOTICE
			"Probe at base[0x55](0x%08lx) past the end of the map(0x%08lx)\n",
			(unsigned long)base + 0x55, map->size -1);
		return 0;
	}
	cfi_send_gen_cmd(0xF0, 0, base, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0x98, 0x55, base, map, cfi, cfi->device_type, NULL);

	if (!qry_present(map,base,cfi))
		return 0;

	if (!cfi->numchips) {
		/* This is the first time we're called. Set up the CFI 
		   stuff accordingly and return */
		return cfi_chip_setup(map, cfi);
	}

	/* Check each previous chip to see if it's an alias */
	for (i=0; i<cfi->numchips; i++) {
		/* This chip should be in read mode if it's one
		   we've already touched. */
		if (qry_present(map,chips[i].start,cfi)) {
			/* Eep. This chip also had the QRY marker. 
			 * Is it an alias for the new one? */
			cfi_send_gen_cmd(0xF0, 0, chips[i].start, map, cfi, cfi->device_type, NULL);

			/* If the QRY marker goes away, it's an alias */
			if (!qry_present(map, chips[i].start, cfi)) {
				printk(KERN_DEBUG "%s: Found an alias at 0x%x for the chip at 0x%lx\n",
				       map->name, base, chips[i].start);
				return 0;
			}
			/* Yes, it's actually got QRY for data. Most 
			 * unfortunate. Stick the new chip in read mode
			 * too and if it's the same, assume it's an alias. */
			/* FIXME: Use other modes to do a proper check */
			cfi_send_gen_cmd(0xF0, 0, base, map, cfi, cfi->device_type, NULL);
			
			if (qry_present(map, base, cfi)) {
				printk(KERN_DEBUG "%s: Found an alias at 0x%x for the chip at 0x%lx\n",
				       map->name, base, chips[i].start);
				return 0;
			}
		}
	}
	
	/* OK, if we got to here, then none of the previous chips appear to
	   be aliases for the current one. */
	if (cfi->numchips == MAX_CFI_CHIPS) {
		printk(KERN_WARNING"%s: Too many flash chips detected. Increase MAX_CFI_CHIPS from %d.\n", map->name, MAX_CFI_CHIPS);
		/* Doesn't matter about resetting it to Read Mode - we're not going to talk to it anyway */
		return -1;
	}
	chips[cfi->numchips].start = base;
	chips[cfi->numchips].state = FL_READY;
	cfi->numchips++;
	
	/* Put it back into Read Mode */
	cfi_send_gen_cmd(0xF0, 0, base, map, cfi, cfi->device_type, NULL);

	printk(KERN_INFO "%s: Found %d x%d devices at 0x%x in %d-bit mode\n",
	       map->name, cfi->interleave, cfi->device_type*8, base,
	       map->buswidth*8);
	
	return 1;
}

static int cfi_chip_setup(struct map_info *map, 
		   struct cfi_private *cfi)
{
	int ofs_factor = cfi->interleave*cfi->device_type;
	__u32 base = 0;
	int num_erase_regions = cfi_read_query(map, base + (0x10 + 28)*ofs_factor);
	int i;

#ifdef DEBUG_CFI
	printk("Number of erase regions: %d\n", num_erase_regions);
#endif
	if (!num_erase_regions)
		return 0;

	cfi->cfiq = kmalloc(sizeof(struct cfi_ident) + num_erase_regions * 4, GFP_KERNEL);
	if (!cfi->cfiq) {
		printk(KERN_WARNING "%s: kmalloc failed for CFI ident structure\n", map->name);
		return 0;
	}
	
	memset(cfi->cfiq,0,sizeof(struct cfi_ident));	
	
	cfi->cfi_mode = CFI_MODE_CFI;
	cfi->fast_prog=1;		/* CFI supports fast programming */
	
	/* Read the CFI info structure */
	for (i=0; i<(sizeof(struct cfi_ident) + num_erase_regions * 4); i++) {
		((unsigned char *)cfi->cfiq)[i] = cfi_read_query(map,base + (0x10 + i)*ofs_factor);
	}
	
	/* Do any necessary byteswapping */
	cfi->cfiq->P_ID = le16_to_cpu(cfi->cfiq->P_ID);
	
	cfi->cfiq->P_ADR = le16_to_cpu(cfi->cfiq->P_ADR);
	cfi->cfiq->A_ID = le16_to_cpu(cfi->cfiq->A_ID);
	cfi->cfiq->A_ADR = le16_to_cpu(cfi->cfiq->A_ADR);
	cfi->cfiq->InterfaceDesc = le16_to_cpu(cfi->cfiq->InterfaceDesc);
	cfi->cfiq->MaxBufWriteSize = le16_to_cpu(cfi->cfiq->MaxBufWriteSize);

#ifdef DEBUG_CFI
	/* Dump the information therein */
	print_cfi_ident(cfi->cfiq);
#endif

	for (i=0; i<cfi->cfiq->NumEraseRegions; i++) {
		cfi->cfiq->EraseRegionInfo[i] = le32_to_cpu(cfi->cfiq->EraseRegionInfo[i]);
		
#ifdef DEBUG_CFI		
		printk("  Erase Region #%d: BlockSize 0x%4.4X bytes, %d blocks\n",
		       i, (cfi->cfiq->EraseRegionInfo[i] >> 8) & ~0xff, 
		       (cfi->cfiq->EraseRegionInfo[i] & 0xffff) + 1);
#endif
	}
	/* Put it back into Read Mode */
	cfi_send_gen_cmd(0xF0, 0, base, map, cfi, cfi->device_type, NULL);

	return 1;
}

#ifdef DEBUG_CFI
static char *vendorname(__u16 vendor) 
{
	switch (vendor) {
	case P_ID_NONE:
		return "None";
		
	case P_ID_INTEL_EXT:
		return "Intel/Sharp Extended";
		
	case P_ID_AMD_STD:
		return "AMD/Fujitsu Standard";
		
	case P_ID_INTEL_STD:
		return "Intel/Sharp Standard";
		
	case P_ID_AMD_EXT:
		return "AMD/Fujitsu Extended";
		
	case P_ID_MITSUBISHI_STD:
		return "Mitsubishi Standard";
		
	case P_ID_MITSUBISHI_EXT:
		return "Mitsubishi Extended";
		
	case P_ID_RESERVED:
		return "Not Allowed / Reserved for Future Use";
		
	default:
		return "Unknown";
	}
}


static void print_cfi_ident(struct cfi_ident *cfip)
{
#if 0
	if (cfip->qry[0] != 'Q' || cfip->qry[1] != 'R' || cfip->qry[2] != 'Y') {
		printk("Invalid CFI ident structure.\n");
		return;
	}	
#endif		
	printk("Primary Vendor Command Set: %4.4X (%s)\n", cfip->P_ID, vendorname(cfip->P_ID));
	if (cfip->P_ADR)
		printk("Primary Algorithm Table at %4.4X\n", cfip->P_ADR);
	else
		printk("No Primary Algorithm Table\n");
	
	printk("Alternative Vendor Command Set: %4.4X (%s)\n", cfip->A_ID, vendorname(cfip->A_ID));
	if (cfip->A_ADR)
		printk("Alternate Algorithm Table at %4.4X\n", cfip->A_ADR);
	else
		printk("No Alternate Algorithm Table\n");
		
		
	printk("Vcc Minimum: %x.%x V\n", cfip->VccMin >> 4, cfip->VccMin & 0xf);
	printk("Vcc Maximum: %x.%x V\n", cfip->VccMax >> 4, cfip->VccMax & 0xf);
	if (cfip->VppMin) {
		printk("Vpp Minimum: %x.%x V\n", cfip->VppMin >> 4, cfip->VppMin & 0xf);
		printk("Vpp Maximum: %x.%x V\n", cfip->VppMax >> 4, cfip->VppMax & 0xf);
	}
	else
		printk("No Vpp line\n");
	
	printk("Typical byte/word write timeout: %d 탎\n", 1<<cfip->WordWriteTimeoutTyp);
	printk("Maximum byte/word write timeout: %d 탎\n", (1<<cfip->WordWriteTimeoutMax) * (1<<cfip->WordWriteTimeoutTyp));
	
	if (cfip->BufWriteTimeoutTyp || cfip->BufWriteTimeoutMax) {
		printk("Typical full buffer write timeout: %d 탎\n", 1<<cfip->BufWriteTimeoutTyp);
		printk("Maximum full buffer write timeout: %d 탎\n", (1<<cfip->BufWriteTimeoutMax) * (1<<cfip->BufWriteTimeoutTyp));
	}
	else
		printk("Full buffer write not supported\n");
	
	printk("Typical block erase timeout: %d ms\n", 1<<cfip->BlockEraseTimeoutTyp);
	printk("Maximum block erase timeout: %d ms\n", (1<<cfip->BlockEraseTimeoutMax) * (1<<cfip->BlockEraseTimeoutTyp));
	if (cfip->ChipEraseTimeoutTyp || cfip->ChipEraseTimeoutMax) {
		printk("Typical chip erase timeout: %d ms\n", 1<<cfip->ChipEraseTimeoutTyp); 
		printk("Maximum chip erase timeout: %d ms\n", (1<<cfip->ChipEraseTimeoutMax) * (1<<cfip->ChipEraseTimeoutTyp));
	}
	else
		printk("Chip erase not supported\n");
	
	printk("Device size: 0x%X bytes (%d MiB)\n", 1 << cfip->DevSize, 1<< (cfip->DevSize - 20));
	printk("Flash Device Interface description: 0x%4.4X\n", cfip->InterfaceDesc);
	switch(cfip->InterfaceDesc) {
	case 0:
		printk("  - x8-only asynchronous interface\n");
		break;
		
	case 1:
		printk("  - x16-only asynchronous interface\n");
		break;
		
	case 2:
		printk("  - supports x8 and x16 via BYTE# with asynchronous interface\n");
		break;
		
	case 3:
		printk("  - x32-only asynchronous interface\n");
		break;
		
	case 65535:
		printk("  - Not Allowed / Reserved\n");
		break;
		
	default:
		printk("  - Unknown\n");
		break;
	}
	
	printk("Max. bytes in buffer write: 0x%x\n", 1<< cfip->MaxBufWriteSize);
	printk("Number of Erase Block Regions: %d\n", cfip->NumEraseRegions);
	
}
#endif /* DEBUG_CFI */

static struct chip_probe cfi_chip_probe = {
	name: "CFI",
	probe_chip: cfi_probe_chip
};

struct mtd_info *cfi_probe(struct map_info *map)
{
	/*
	 * Just use the generic probe stuff to call our CFI-specific
	 * chip_probe routine in all the possible permutations, etc.
	 */
	return mtd_do_chip_probe(map, &cfi_chip_probe);
}

static struct mtd_chip_driver cfi_chipdrv = {
	probe: cfi_probe,
	name: "cfi_probe",
	module: THIS_MODULE
};

int __init cfi_probe_init(void)
{
	register_mtd_chip_driver(&cfi_chipdrv);
	return 0;
}

static void __exit cfi_probe_exit(void)
{
	unregister_mtd_chip_driver(&cfi_chipdrv);
}

module_init(cfi_probe_init);
module_exit(cfi_probe_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Woodhouse <dwmw2@infradead.org> et al.");
MODULE_DESCRIPTION("Probe code for CFI-compliant flash chips");
