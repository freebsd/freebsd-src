/*
 * $Id: physmap64.c,v 1.1 2002/09/05 05:12:32 acurtis Exp $
 *
 * Normal mappings of chips in physical memory
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/config.h>
#include <linux/mtd/cfi.h>

#ifdef CONFIG_MTD_PARTITIONS
#include <linux/mtd/partitions.h>
#endif

#define WINDOW_ADDR CONFIG_MTD_PHYSMAP_START
#define WINDOW_SIZE CONFIG_MTD_PHYSMAP_LEN
#define BUSWIDTH CONFIG_MTD_PHYSMAP_BUSWIDTH

static struct mtd_info *mymtd;

__u8 physmap_read8(struct map_info *map, unsigned long ofs)
{
	return __raw_readb(map->map_priv_1 + ofs);
}

__u16 physmap_read16(struct map_info *map, unsigned long ofs)
{
	return __raw_readw(map->map_priv_1 + ofs);
}

__u32 physmap_read32(struct map_info *map, unsigned long ofs)
{
	return __raw_readl(map->map_priv_1 + ofs);
}

__u64 physmap_read64(struct map_info *map, unsigned long ofs)
{
	return __raw_readll(map->map_priv_1 + ofs);
}

void physmap_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to, map->map_priv_1 + from, len);
}

void physmap_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	__raw_writeb(d, map->map_priv_1 + adr);
	mb();
}

void physmap_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	__raw_writew(d, map->map_priv_1 + adr);
	mb();
}

void physmap_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	__raw_writel(d, map->map_priv_1 + adr);
	mb();
}

void physmap_write64(struct map_info *map, __u64 d, unsigned long adr)
{
	__raw_writell(d, map->map_priv_1 + adr);
	mb();
}

void physmap_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy_toio(map->map_priv_1 + to, from, len);
}

struct map_info physmap_map = {
	name: "Physically mapped flash",
	size: WINDOW_SIZE,
	buswidth: BUSWIDTH,
	read8: physmap_read8,
	read16: physmap_read16,
	read32: physmap_read32,
	read64: physmap_read64,
	copy_from: physmap_copy_from,
	write8: physmap_write8,
	write16: physmap_write16,
	write32: physmap_write32,
	write64: physmap_write64,
	copy_to: physmap_copy_to
};

#ifdef CONFIG_MTD_PARTITIONS
#ifdef CONFIG_MTD_CMDLINE_PARTS
static struct mtd_partition *mtd_parts = 0;
static int                   mtd_parts_nb = 0;
#else
static struct mtd_partition physmap_partitions[] = {
/* Put your own partition definitions here */
#if 0
	{
		name:		"bootROM",
		size:		0x80000,
		offset:		0,
		mask_flags:	MTD_WRITEABLE,  /* force read-only */
	}, {
		name:		"zImage",
		size:		0x100000,
		offset:		MTDPART_OFS_APPEND,
		mask_flags:	MTD_WRITEABLE,  /* force read-only */
	}, {
		name:		"ramdisk.gz",
		size:		0x300000,
		offset:		MTDPART_OFS_APPEND,
		mask_flags:	MTD_WRITEABLE,  /* force read-only */
	}, {
		name:		"User FS",
		size:		MTDPART_SIZ_FULL,
		offset:		MTDPART_OFS_APPEND,
	}
#endif
};

#define NUM_PARTITIONS	(sizeof(physmap_partitions)/sizeof(struct mtd_partition))

#endif
#endif

int __init init_physmap(void)
{
	static const char *rom_probe_types[] = { "cfi_probe", "jedec_probe", "map_rom", 0 };
	const char **type;

       	printk(KERN_NOTICE "physmap flash device: %x at %x\n", WINDOW_SIZE, WINDOW_ADDR);
	physmap_map.map_priv_1 = (unsigned long)ioremap(WINDOW_ADDR, WINDOW_SIZE);

	if (!physmap_map.map_priv_1) {
		printk("Failed to ioremap\n");
		return -EIO;
	}
	
	mymtd = 0;
	type = rom_probe_types;
	for(; !mymtd && *type; type++) {
		mymtd = do_map_probe(*type, &physmap_map);
	}
	if (mymtd) {
		mymtd->module = THIS_MODULE;

		add_mtd_device(mymtd);
#ifdef CONFIG_MTD_PARTITIONS
#ifdef CONFIG_MTD_CMDLINE_PARTS
		mtd_parts_nb = parse_cmdline_partitions(mymtd, &mtd_parts, 
							"phys");
		if (mtd_parts_nb > 0)
		{
			printk(KERN_NOTICE 
			       "Using command line partition definition\n");
			add_mtd_partitions (mymtd, mtd_parts, mtd_parts_nb);
		}
#else
		if (NUM_PARTITIONS != 0) 
		{
			printk(KERN_NOTICE 
			       "Using physmap partition definition\n");
			add_mtd_partitions (mymtd, physmap_partitions, NUM_PARTITIONS);
		}

#endif
#endif
		return 0;
	}

	iounmap((void *)physmap_map.map_priv_1);
	return -ENXIO;
}

static void __exit cleanup_physmap(void)
{
	if (mymtd) {
		del_mtd_device(mymtd);
		map_destroy(mymtd);
	}
	if (physmap_map.map_priv_1) {
		iounmap((void *)physmap_map.map_priv_1);
		physmap_map.map_priv_1 = 0;
	}
}

module_init(init_physmap);
module_exit(cleanup_physmap);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Woodhouse <dwmw2@infradead.org>");
MODULE_DESCRIPTION("Generic configurable MTD map driver");
