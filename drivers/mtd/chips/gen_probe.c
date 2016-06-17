/*
 * Routines common to all CFI-type probes.
 * (C) 2001, 2001 Red Hat, Inc.
 * GPL'd
 * $Id: gen_probe.c,v 1.9 2002/09/05 05:15:32 acurtis Exp $
 */

#include <linux/kernel.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/cfi.h>
#include <linux/mtd/gen_probe.h>

static struct mtd_info *check_cmd_set(struct map_info *, int);
static struct cfi_private *genprobe_ident_chips(struct map_info *map,
						struct chip_probe *cp);
static int genprobe_new_chip(struct map_info *map, struct chip_probe *cp,
			     struct cfi_private *cfi);

struct mtd_info *mtd_do_chip_probe(struct map_info *map, struct chip_probe *cp)
{
	struct mtd_info *mtd = NULL;
	struct cfi_private *cfi;

	/* First probe the map to see if we have CFI stuff there. */
	cfi = genprobe_ident_chips(map, cp);
	
	if (!cfi)
		return NULL;

	map->fldrv_priv = cfi;
	/* OK we liked it. Now find a driver for the command set it talks */

	mtd = check_cmd_set(map, 1); /* First the primary cmdset */
	if (!mtd)
		mtd = check_cmd_set(map, 0); /* Then the secondary */
	
	if (mtd)
		return mtd;

	printk(KERN_WARNING"gen_probe: No supported Vendor Command Set found\n");
	
	kfree(cfi->cfiq);
	kfree(cfi);
	map->fldrv_priv = NULL;
	return NULL;
}
EXPORT_SYMBOL(mtd_do_chip_probe);


struct cfi_private *genprobe_ident_chips(struct map_info *map, struct chip_probe *cp)
{
	unsigned long base=0;
	struct cfi_private cfi;
	struct cfi_private *retcfi;
	struct flchip chip[MAX_CFI_CHIPS];
	int i;

	memset(&cfi, 0, sizeof(cfi));

	/* Call the probetype-specific code with all permutations of 
	   interleave and device type, etc. */
	if (!genprobe_new_chip(map, cp, &cfi)) {
		/* The probe didn't like it */
		printk(KERN_WARNING "%s: Found no %s device at location zero\n",
		       cp->name, map->name);
		return NULL;
	}		

#if 0 /* Let the CFI probe routine do this sanity check. The Intel and AMD
	 probe routines won't ever return a broken CFI structure anyway,
	 because they make them up themselves.
      */
	if (cfi.cfiq->NumEraseRegions == 0) {
		printk(KERN_WARNING "Number of erase regions is zero\n");
		kfree(cfi.cfiq);
		return NULL;
	}
#endif
	chip[0].start = 0;
	chip[0].state = FL_READY;
	cfi.chipshift = cfi.cfiq->DevSize;

	switch(cfi.interleave) {
#ifdef CFIDEV_INTERLEAVE_1
	case 1:
		break;
#endif
#ifdef CFIDEV_INTERLEAVE_2
	case 2:
		cfi.chipshift++;
		break;
#endif
#ifdef CFIDEV_INTERLEAVE_4
	case 4:
		cfi.chipshift+=2;
		break;
#endif
	default:
		BUG();
	}
		
	cfi.numchips = 1;

	/*
	 * Now probe for other chips, checking sensibly for aliases while
	 * we're at it. The new_chip probe above should have let the first
	 * chip in read mode.
	 *
	 * NOTE: Here, we're checking if there is room for another chip
	 *       the same size within the mapping. Therefore, 
	 *       base + chipsize <= map->size is the correct thing to do, 
	 *       because, base + chipsize would be the  _first_ byte of the
	 *       next chip, not the one we're currently pondering.
	 */

	for (base = (1<<cfi.chipshift); base + (1<<cfi.chipshift) <= map->size;
	     base += (1<<cfi.chipshift))
		cp->probe_chip(map, base, &chip[0], &cfi);

	/*
	 * Now allocate the space for the structures we need to return to 
	 * our caller, and copy the appropriate data into them.
	 */

	retcfi = kmalloc(sizeof(struct cfi_private) + cfi.numchips * sizeof(struct flchip), GFP_KERNEL);

	if (!retcfi) {
		printk(KERN_WARNING "%s: kmalloc failed for CFI private structure\n", map->name);
		kfree(cfi.cfiq);
		return NULL;
	}

	memcpy(retcfi, &cfi, sizeof(cfi));
	memcpy(&retcfi->chips[0], chip, sizeof(struct flchip) * cfi.numchips);

	/* Fix up the stuff that breaks when you move it */
	for (i=0; i< retcfi->numchips; i++) {
		init_waitqueue_head(&retcfi->chips[i].wq);
		spin_lock_init(&retcfi->chips[i]._spinlock);
		retcfi->chips[i].mutex = &retcfi->chips[i]._spinlock;
	}

	return retcfi;
}

	
static int genprobe_new_chip(struct map_info *map, struct chip_probe *cp,
			     struct cfi_private *cfi)
{
	switch (map->buswidth) {
#ifdef CFIDEV_BUSWIDTH_1		
	case CFIDEV_BUSWIDTH_1:
		cfi->interleave = CFIDEV_INTERLEAVE_1;

		cfi->device_type = CFI_DEVICETYPE_X8;
		if (cp->probe_chip(map, 0, NULL, cfi))
			return 1;

		cfi->device_type = CFI_DEVICETYPE_X16;
		if (cp->probe_chip(map, 0, NULL, cfi))
			return 1;
		break;			
#endif /* CFIDEV_BUSWITDH_1 */

#ifdef CFIDEV_BUSWIDTH_2		
	case CFIDEV_BUSWIDTH_2:
#ifdef CFIDEV_INTERLEAVE_1
		cfi->interleave = CFIDEV_INTERLEAVE_1;

		cfi->device_type = CFI_DEVICETYPE_X16;
		if (cp->probe_chip(map, 0, NULL, cfi))
			return 1;
#endif /* CFIDEV_INTERLEAVE_1 */
#ifdef CFIDEV_INTERLEAVE_2
		cfi->interleave = CFIDEV_INTERLEAVE_2;

		cfi->device_type = CFI_DEVICETYPE_X8;
		if (cp->probe_chip(map, 0, NULL, cfi))
			return 1;

		cfi->device_type = CFI_DEVICETYPE_X16;
		if (cp->probe_chip(map, 0, NULL, cfi))
			return 1;
#endif /* CFIDEV_INTERLEAVE_2 */
		break;			
#endif /* CFIDEV_BUSWIDTH_2 */

#ifdef CFIDEV_BUSWIDTH_4
	case CFIDEV_BUSWIDTH_4:
#if defined(CFIDEV_INTERLEAVE_1) && defined(SOMEONE_ACTUALLY_MAKES_THESE)
                cfi->interleave = CFIDEV_INTERLEAVE_1;

                cfi->device_type = CFI_DEVICETYPE_X32;
		if (cp->probe_chip(map, 0, NULL, cfi))
			return 1;
#endif /* CFIDEV_INTERLEAVE_1 */
#ifdef CFIDEV_INTERLEAVE_2
		cfi->interleave = CFIDEV_INTERLEAVE_2;

#ifdef SOMEONE_ACTUALLY_MAKES_THESE
		cfi->device_type = CFI_DEVICETYPE_X32;
		if (cp->probe_chip(map, 0, NULL, cfi))
			return 1;
#endif
		cfi->device_type = CFI_DEVICETYPE_X16;
		if (cp->probe_chip(map, 0, NULL, cfi))
			return 1;

		cfi->device_type = CFI_DEVICETYPE_X8;
		if (cp->probe_chip(map, 0, NULL, cfi))
			return 1;
#endif /* CFIDEV_INTERLEAVE_2 */
#ifdef CFIDEV_INTERLEAVE_4
		cfi->interleave = CFIDEV_INTERLEAVE_4;

#ifdef SOMEONE_ACTUALLY_MAKES_THESE
		cfi->device_type = CFI_DEVICETYPE_X32;
		if (cp->probe_chip(map, 0, NULL, cfi))
			return 1;
#endif
		cfi->device_type = CFI_DEVICETYPE_X16;
		if (cp->probe_chip(map, 0, NULL, cfi))
			return 1;

		cfi->device_type = CFI_DEVICETYPE_X8;
		if (cp->probe_chip(map, 0, NULL, cfi))
			return 1;
#endif /* CFIDEV_INTERLEAVE_4 */
		break;
#endif /* CFIDEV_BUSWIDTH_4 */

#ifdef CFIDEV_BUSWIDTH_8
	case CFIDEV_BUSWIDTH_8:
#if defined(CFIDEV_INTERLEAVE_2) && defined(SOMEONE_ACTUALLY_MAKES_THESE)
                cfi->interleave = CFIDEV_INTERLEAVE_2;

                cfi->device_type = CFI_DEVICETYPE_X32;
		if (cp->probe_chip(map, 0, NULL, cfi))
			return 1;
#endif /* CFIDEV_INTERLEAVE_2 */
#ifdef CFIDEV_INTERLEAVE_4
		cfi->interleave = CFIDEV_INTERLEAVE_4;

#ifdef SOMEONE_ACTUALLY_MAKES_THESE
		cfi->device_type = CFI_DEVICETYPE_X32;
		if (cp->probe_chip(map, 0, NULL, cfi))
			return 1;
#endif
		cfi->device_type = CFI_DEVICETYPE_X16;
		if (cp->probe_chip(map, 0, NULL, cfi))
			return 1;
#endif /* CFIDEV_INTERLEAVE_4 */
#ifdef CFIDEV_INTERLEAVE_8
		cfi->interleave = CFIDEV_INTERLEAVE_8;

		cfi->device_type = CFI_DEVICETYPE_X16;
		if (cp->probe_chip(map, 0, NULL, cfi))
			return 1;

		cfi->device_type = CFI_DEVICETYPE_X8;
		if (cp->probe_chip(map, 0, NULL, cfi))
			return 1;
#endif /* CFIDEV_INTERLEAVE_8 */
		break;
#endif /* CFIDEV_BUSWIDTH_8 */

	default:
		printk(KERN_WARNING "genprobe_new_chip called with unsupported buswidth %d\n", map->buswidth);
		return 0;
	}
	return 0;
}


typedef struct mtd_info *cfi_cmdset_fn_t(struct map_info *, int);

extern cfi_cmdset_fn_t cfi_cmdset_0001;
extern cfi_cmdset_fn_t cfi_cmdset_0002;
extern cfi_cmdset_fn_t cfi_cmdset_0020;

static inline struct mtd_info *cfi_cmdset_unknown(struct map_info *map, 
						  int primary)
{
	struct cfi_private *cfi = map->fldrv_priv;
	__u16 type = primary?cfi->cfiq->P_ID:cfi->cfiq->A_ID;
#if defined(CONFIG_MODULES) && defined(HAVE_INTER_MODULE)
	char probename[32];
	cfi_cmdset_fn_t *probe_function;

	sprintf(probename, "cfi_cmdset_%4.4X", type);
		
	probe_function = inter_module_get_request(probename, probename);

	if (probe_function) {
		struct mtd_info *mtd;

		mtd = (*probe_function)(map, primary);
		/* If it was happy, it'll have increased its own use count */
		inter_module_put(probename);
		return mtd;
	}
#endif
	printk(KERN_NOTICE "Support for command set %04X not present\n",
	       type);

	return NULL;
}

static struct mtd_info *check_cmd_set(struct map_info *map, int primary)
{
	struct cfi_private *cfi = map->fldrv_priv;
	__u16 type = primary?cfi->cfiq->P_ID:cfi->cfiq->A_ID;
	
	if (type == P_ID_NONE || type == P_ID_RESERVED)
		return NULL;

	switch(type){
		/* Urgh. Ifdefs. The version with weak symbols was
		 * _much_ nicer. Shame it didn't seem to work on
		 * anything but x86, really.
		 * But we can't rely in inter_module_get() because
		 * that'd mean we depend on link order.
		 */
#ifdef CONFIG_MTD_CFI_INTELEXT
	case 0x0001:
	case 0x0003:
		return cfi_cmdset_0001(map, primary);
#endif
#ifdef CONFIG_MTD_CFI_AMDSTD
	case 0x0002:
		return cfi_cmdset_0002(map, primary);
#endif
#ifdef CONFIG_MTD_CFI_STAA
        case 0x0020:
		return cfi_cmdset_0020(map, primary);
#endif
	}

	return cfi_cmdset_unknown(map, primary);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Woodhouse <dwmw2@infradead.org>");
MODULE_DESCRIPTION("Helper routines for flash chip probe code");
