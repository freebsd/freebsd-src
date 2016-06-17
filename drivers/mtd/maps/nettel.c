/****************************************************************************/

/*
 *      nettel.c -- mappings for NETtel/SecureEdge/SnapGear (x86) boards.
 *
 *      (C) Copyright 2000-2001, Greg Ungerer (gerg@snapgear.com)
 *      (C) Copyright 2001-2002, SnapGear (www.snapgear.com)
 *
 *	$Id: nettel.c,v 1.1 2002/08/08 06:30:13 gerg Exp $
 */

/****************************************************************************/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/cfi.h>
#include <linux/reboot.h>
#include <asm/io.h>

/****************************************************************************/

#define INTEL_BUSWIDTH		1
#define AMD_WINDOW_MAXSIZE	0x00200000
#define AMD_BUSWIDTH	 	1

/*
 *	PAR masks and shifts, assuming 64K pages.
 */
#define SC520_PAR_ADDR_MASK	0x00003fff
#define SC520_PAR_ADDR_SHIFT	16
#define SC520_PAR_TO_ADDR(par) \
	(((par)&SC520_PAR_ADDR_MASK) << SC520_PAR_ADDR_SHIFT)

#define SC520_PAR_SIZE_MASK	0x01ffc000
#define SC520_PAR_SIZE_SHIFT	2
#define SC520_PAR_TO_SIZE(par) \
	((((par)&SC520_PAR_SIZE_MASK) << SC520_PAR_SIZE_SHIFT) + (64*1024))

#define SC520_PAR(cs, addr, size) \
	((cs) | \
	((((size)-(64*1024)) >> SC520_PAR_SIZE_SHIFT) & SC520_PAR_SIZE_MASK) | \
	(((addr) >> SC520_PAR_ADDR_SHIFT) & SC520_PAR_ADDR_MASK))

#define SC520_PAR_BOOTCS	0x8a000000
#define	SC520_PAR_ROMCS1	0xaa000000
#define SC520_PAR_ROMCS2	0xca000000	/* Cache disabled, 64K page */

static void *nettel_mmcrp = NULL;

#ifdef CONFIG_MTD_CFI_INTELEXT
static struct mtd_info *intel_mtd;
#endif
static struct mtd_info *amd_mtd;

/****************************************************************************/

static __u8 nettel_read8(struct map_info *map, unsigned long ofs)
{
	return(readb(map->map_priv_1 + ofs));
}

static __u16 nettel_read16(struct map_info *map, unsigned long ofs)
{
	return(readw(map->map_priv_1 + ofs));
}

static __u32 nettel_read32(struct map_info *map, unsigned long ofs)
{
	return(readl(map->map_priv_1 + ofs));
}

static void nettel_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to, map->map_priv_1 + from, len);
}

static void nettel_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	writeb(d, map->map_priv_1 + adr);
}

static void nettel_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	writew(d, map->map_priv_1 + adr);
}

static void nettel_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	writel(d, map->map_priv_1 + adr);
}

static void nettel_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy_toio(map->map_priv_1 + to, from, len);
}

/****************************************************************************/

#ifdef CONFIG_MTD_CFI_INTELEXT
static struct map_info nettel_intel_map = {
	name: "SnapGear Intel",
	size: 0,
	buswidth: INTEL_BUSWIDTH,
	read8: nettel_read8,
	read16: nettel_read16,
	read32: nettel_read32,
	copy_from: nettel_copy_from,
	write8: nettel_write8,
	write16: nettel_write16,
	write32: nettel_write32,
	copy_to: nettel_copy_to
};

static struct mtd_partition nettel_intel_partitions[] = {
	{
		name: "SnapGear kernel",
		offset: 0,
		size: 0x000e0000
	},
	{
		name: "SnapGear filesystem",
		offset: 0x00100000,
	},
	{
		name: "SnapGear config",
		offset: 0x000e0000,
		size: 0x00020000
	},
	{
		name: "SnapGear Intel",
		offset: 0
	},
	{
		name: "SnapGear BIOS Config",
		offset: 0x007e0000,
		size: 0x00020000
	},
	{
		name: "SnapGear BIOS",
		offset: 0x007e0000,
		size: 0x00020000
	},
};
#endif

static struct map_info nettel_amd_map = {
	name: "SnapGear AMD",
	size: AMD_WINDOW_MAXSIZE,
	buswidth: AMD_BUSWIDTH,
	read8: nettel_read8,
	read16: nettel_read16,
	read32: nettel_read32,
	copy_from: nettel_copy_from,
	write8: nettel_write8,
	write16: nettel_write16,
	write32: nettel_write32,
	copy_to: nettel_copy_to
};

static struct mtd_partition nettel_amd_partitions[] = {
	{
		name: "SnapGear BIOS config",
		offset: 0x000e0000,
		size: 0x00010000
	},
	{
		name: "SnapGear BIOS",
		offset: 0x000f0000,
		size: 0x00010000
	},
	{
		name: "SnapGear AMD",
		offset: 0
	},
	{
		name: "SnapGear high BIOS",
		offset: 0x001f0000,
		size: 0x00010000
	}
};

#define NUM_AMD_PARTITIONS \
	(sizeof(nettel_amd_partitions)/sizeof(nettel_amd_partitions[0]))

/****************************************************************************/

#ifdef CONFIG_MTD_CFI_INTELEXT

/*
 *	Set the Intel flash back to read mode since some old boot
 *	loaders don't.
 */
static int nettel_reboot_notifier(struct notifier_block *nb, unsigned long val, void *v)
{
	struct cfi_private *cfi = nettel_intel_map.fldrv_priv;
	unsigned long b;
	
	/* Make sure all FLASH chips are put back into read mode */
	for (b = 0; (b < nettel_intel_partitions[3].size); b += 0x100000) {
		cfi_send_gen_cmd(0xff, 0x55, b, &nettel_intel_map, cfi,
			cfi->device_type, NULL);
	}
	return(NOTIFY_OK);
}

static struct notifier_block nettel_notifier_block = {
	nettel_reboot_notifier, NULL, 0
};

/*
 *	Erase the configuration file system.
 *	Used to support the software reset button.
 */
static void nettel_erasecallback(struct erase_info *done)
{
	wait_queue_head_t *wait_q = (wait_queue_head_t *)done->priv;
	wake_up(wait_q);
}

static struct erase_info nettel_erase;

int nettel_eraseconfig(void)
{
	struct mtd_info *mtd;
	DECLARE_WAITQUEUE(wait, current);
	wait_queue_head_t wait_q;
	int ret;

	init_waitqueue_head(&wait_q);
	mtd = get_mtd_device(NULL, 2);
	if (mtd) {
		nettel_erase.mtd = mtd;
		nettel_erase.callback = nettel_erasecallback;
		nettel_erase.callback = 0;
		nettel_erase.addr = 0;
		nettel_erase.len = mtd->size;
		nettel_erase.priv = (u_long) &wait_q;
		nettel_erase.priv = 0;

		set_current_state(TASK_INTERRUPTIBLE);
		add_wait_queue(&wait_q, &wait);

		ret = MTD_ERASE(mtd, &nettel_erase);
		if (ret) {
			set_current_state(TASK_RUNNING);
			remove_wait_queue(&wait_q, &wait);
			put_mtd_device(mtd);
			return(ret);
		}

		schedule();  /* Wait for erase to finish. */
		remove_wait_queue(&wait_q, &wait);
		
		put_mtd_device(mtd);
	}

	return(0);
}

#else

int nettel_eraseconfig(void)
{
	return(0);
}

#endif

/****************************************************************************/

int __init nettel_init(void)
{
	volatile unsigned long *amdpar;
	unsigned long amdaddr, maxsize;
	int num_amd_partitions=0;
#ifdef CONFIG_MTD_CFI_INTELEXT
	volatile unsigned long *intel0par, *intel1par;
	unsigned long orig_bootcspar, orig_romcs1par;
	unsigned long intel0addr, intel0size;
	unsigned long intel1addr, intel1size;
	int intelboot, intel0cs, intel1cs;
	int num_intel_partitions;
#endif
	int rc = 0;

	nettel_mmcrp = (void *) ioremap_nocache(0xfffef000, 4096);
	if (nettel_mmcrp == NULL) {
		printk("SNAPGEAR: failed to disable MMCR cache??\n");
		return(-EIO);
	}

	/* Set CPU clock to be 33.000MHz */
	*((unsigned char *) (nettel_mmcrp + 0xc64)) = 0x01;

	amdpar = (volatile unsigned long *) (nettel_mmcrp + 0xc4);

#ifdef CONFIG_MTD_CFI_INTELEXT
	intelboot = 0;
	intel0cs = SC520_PAR_ROMCS1;
	intel0par = (volatile unsigned long *) (nettel_mmcrp + 0xc0);
	intel1cs = SC520_PAR_ROMCS2;
	intel1par = (volatile unsigned long *) (nettel_mmcrp + 0xbc);

	/*
	 *	Save the CS settings then ensure ROMCS1 and ROMCS2 are off,
	 *	otherwise they might clash with where we try to map BOOTCS.
	 */
	orig_bootcspar = *amdpar;
	orig_romcs1par = *intel0par;
	*intel0par = 0;
	*intel1par = 0;
#endif

	/*
	 *	The first thing to do is determine if we have a separate
	 *	boot FLASH device. Typically this is a small (1 to 2MB)
	 *	AMD FLASH part. It seems that device size is about the
	 *	only way to tell if this is the case...
	 */
	amdaddr = 0x20000000;
	maxsize = AMD_WINDOW_MAXSIZE;

	*amdpar = SC520_PAR(SC520_PAR_BOOTCS, amdaddr, maxsize);
	__asm__ ("wbinvd");

	nettel_amd_map.map_priv_1 = (unsigned long)
		ioremap_nocache(amdaddr, maxsize);
	if (!nettel_amd_map.map_priv_1) {
		printk("SNAPGEAR: failed to ioremap() BOOTCS\n");
		return(-EIO);
	}

	if ((amd_mtd = do_map_probe("jedec_probe", &nettel_amd_map))) {
		printk(KERN_NOTICE "SNAPGEAR: AMD flash device size = %dK\n",
			amd_mtd->size>>10);

		amd_mtd->module = THIS_MODULE;

		/* The high BIOS partition is only present for 2MB units */
		num_amd_partitions = NUM_AMD_PARTITIONS;
		if (amd_mtd->size < AMD_WINDOW_MAXSIZE)
			num_amd_partitions--;
		/* Don't add the partition until after the primary INTEL's */

#ifdef CONFIG_MTD_CFI_INTELEXT
		/*
		 *	Map the Intel flash into memory after the AMD
		 *	It has to start on a multiple of maxsize.
		 */
		maxsize = SC520_PAR_TO_SIZE(orig_romcs1par);
		if (maxsize < (32 * 1024 * 1024))
			maxsize = (32 * 1024 * 1024);
		intel0addr = amdaddr + maxsize;
#endif
	} else {
#ifdef CONFIG_MTD_CFI_INTELEXT
		/* INTEL boot FLASH */
		intelboot++;

		if (!orig_romcs1par) {
			intel0cs = SC520_PAR_BOOTCS;
			intel0par = (volatile unsigned long *)
				(nettel_mmcrp + 0xc4);
			intel1cs = SC520_PAR_ROMCS1;
			intel1par = (volatile unsigned long *)
				(nettel_mmcrp + 0xc0);

			intel0addr = SC520_PAR_TO_ADDR(orig_bootcspar);
			maxsize = SC520_PAR_TO_SIZE(orig_bootcspar);
		} else {
			/* Kernel base is on ROMCS1, not BOOTCS */
			intel0cs = SC520_PAR_ROMCS1;
			intel0par = (volatile unsigned long *)
				(nettel_mmcrp + 0xc0);
			intel1cs = SC520_PAR_BOOTCS;
			intel1par = (volatile unsigned long *)
				(nettel_mmcrp + 0xc4);

			intel0addr = SC520_PAR_TO_ADDR(orig_romcs1par);
			maxsize = SC520_PAR_TO_SIZE(orig_romcs1par);
		}

		/* Destroy useless AMD MTD mapping */
		amd_mtd = NULL;
		iounmap((void *) nettel_amd_map.map_priv_1);
		nettel_amd_map.map_priv_1 = (unsigned long) NULL;
#else
		/* Only AMD flash supported */
		return(-ENXIO);
#endif
	}

#ifdef CONFIG_MTD_CFI_INTELEXT
	/*
	 *	We have determined the INTEL FLASH configuration, so lets
	 *	go ahead and probe for them now.
	 */

	/* Set PAR to the maximum size */
	if (maxsize < (32 * 1024 * 1024))
		maxsize = (32 * 1024 * 1024);
	*intel0par = SC520_PAR(intel0cs, intel0addr, maxsize);

	/* Turn other PAR off so the first probe doesn't find it */
	*intel1par = 0;

	/* Probe for the the size of the first Intel flash */
	nettel_intel_map.size = maxsize;
	nettel_intel_map.map_priv_1 = (unsigned long)
		ioremap_nocache(intel0addr, maxsize);
	if (!nettel_intel_map.map_priv_1) {
		printk("SNAPGEAR: failed to ioremap() ROMCS1\n");
		return(-EIO);
	}

	intel_mtd = do_map_probe("cfi_probe", &nettel_intel_map);
	if (! intel_mtd) {
		iounmap((void *) nettel_intel_map.map_priv_1);
		return(-ENXIO);
	}

	/* Set PAR to the detected size */
	intel0size = intel_mtd->size;
	*intel0par = SC520_PAR(intel0cs, intel0addr, intel0size);

	/*
	 *	Map second Intel FLASH right after first. Set its size to the
	 *	same maxsize used for the first Intel FLASH.
	 */
	intel1addr = intel0addr + intel0size;
	*intel1par = SC520_PAR(intel1cs, intel1addr, maxsize);
	__asm__ ("wbinvd");

	maxsize += intel0size;

	/* Delete the old map and probe again to do both chips */
	map_destroy(intel_mtd);
	intel_mtd = NULL;
	iounmap((void *) nettel_intel_map.map_priv_1);

	nettel_intel_map.size = maxsize;
	nettel_intel_map.map_priv_1 = (unsigned long)
		ioremap_nocache(intel0addr, maxsize);
	if (!nettel_intel_map.map_priv_1) {
		printk("SNAPGEAR: failed to ioremap() ROMCS1/2\n");
		return(-EIO);
	}

	intel_mtd = do_map_probe("cfi_probe", &nettel_intel_map);
	if (! intel_mtd) {
		iounmap((void *) nettel_intel_map.map_priv_1);
		return(-ENXIO);
	}

	intel1size = intel_mtd->size - intel0size;
	if (intel1size > 0) {
		*intel1par = SC520_PAR(intel1cs, intel1addr, intel1size);
		__asm__ ("wbinvd");
	} else {
		*intel1par = 0;
	}

	printk(KERN_NOTICE "SNAPGEAR: Intel flash device size = %dK\n",
		(intel_mtd->size >> 10));

	intel_mtd->module = THIS_MODULE;

#ifndef CONFIG_BLK_DEV_INITRD
	ROOT_DEV = MKDEV(MTD_BLOCK_MAJOR, 1);
#endif

	num_intel_partitions = sizeof(nettel_intel_partitions) /
		sizeof(nettel_intel_partitions[0]);

	if (intelboot) {
		/*
		 *	Adjust offset and size of last boot partition.
		 *	Must allow for BIOS region at end of FLASH.
		 */
		nettel_intel_partitions[1].size = (intel0size + intel1size) -
			(1024*1024 + intel_mtd->erasesize);
		nettel_intel_partitions[3].size = intel0size + intel1size;
		nettel_intel_partitions[4].offset = 
			(intel0size + intel1size) - intel_mtd->erasesize;
		nettel_intel_partitions[4].size = intel_mtd->erasesize;
		nettel_intel_partitions[5].offset =
			nettel_intel_partitions[4].offset;
		nettel_intel_partitions[5].size =
			nettel_intel_partitions[4].size;
	} else {
		/* No BIOS regions when AMD boot */
		num_intel_partitions -= 2;
	}
	rc = add_mtd_partitions(intel_mtd, nettel_intel_partitions,
		num_intel_partitions);
#endif

	if (amd_mtd) {
		rc = add_mtd_partitions(amd_mtd, nettel_amd_partitions,
			num_amd_partitions);
	}

#ifdef CONFIG_MTD_CFI_INTELEXT
	register_reboot_notifier(&nettel_notifier_block);
#endif

	return(rc);
}

/****************************************************************************/

void __exit nettel_cleanup(void)
{
#ifdef CONFIG_MTD_CFI_INTELEXT
	unregister_reboot_notifier(&nettel_notifier_block);
#endif
	if (amd_mtd) {
		del_mtd_partitions(amd_mtd);
		map_destroy(amd_mtd);
	}
	if (nettel_amd_map.map_priv_1) {
		iounmap((void *)nettel_amd_map.map_priv_1);
		nettel_amd_map.map_priv_1 = 0;
	}
#ifdef CONFIG_MTD_CFI_INTELEXT
	if (intel_mtd) {
		del_mtd_partitions(intel_mtd);
		map_destroy(intel_mtd);
	}
	if (nettel_intel_map.map_priv_1) {
		iounmap((void *)nettel_intel_map.map_priv_1);
		nettel_intel_map.map_priv_1 = 0;
	}
#endif
}

/****************************************************************************/

module_init(nettel_init);
module_exit(nettel_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Greg Ungerer <gerg@snapgear.com>");
MODULE_DESCRIPTION("SnapGear/SecureEdge FLASH support");

/****************************************************************************/
