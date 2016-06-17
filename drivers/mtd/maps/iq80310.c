/*
 * $Id: iq80310.c,v 1.9 2002/01/01 22:45:02 rmk Exp $
 *
 * Mapping for the Intel XScale IQ80310 evaluation board
 *
 * Author:	Nicolas Pitre
 * Copyright:	(C) 2001 MontaVista Software Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>


#define WINDOW_ADDR 	0
#define WINDOW_SIZE 	8*1024*1024
#define BUSWIDTH 	1

static struct mtd_info *mymtd;

static __u8 iq80310_read8(struct map_info *map, unsigned long ofs)
{
	return *(__u8 *)(map->map_priv_1 + ofs);
}

static __u16 iq80310_read16(struct map_info *map, unsigned long ofs)
{
	return *(__u16 *)(map->map_priv_1 + ofs);
}

static __u32 iq80310_read32(struct map_info *map, unsigned long ofs)
{
	return *(__u32 *)(map->map_priv_1 + ofs);
}

static void iq80310_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy(to, (void *)(map->map_priv_1 + from), len);
}

static void iq80310_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	*(__u8 *)(map->map_priv_1 + adr) = d;
}

static void iq80310_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	*(__u16 *)(map->map_priv_1 + adr) = d;
}

static void iq80310_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	*(__u32 *)(map->map_priv_1 + adr) = d;
}

static void iq80310_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy((void *)(map->map_priv_1 + to), from, len);
}

static struct map_info iq80310_map = {
	name: "IQ80310 flash",
	size: WINDOW_SIZE,
	buswidth: BUSWIDTH,
	read8:		iq80310_read8,
	read16:		iq80310_read16,
	read32:		iq80310_read32,
	copy_from:	iq80310_copy_from,
	write8:		iq80310_write8,
	write16:	iq80310_write16,
	write32:	iq80310_write32,
	copy_to:	iq80310_copy_to
};

static struct mtd_partition iq80310_partitions[4] = {
	{
		name:		"Firmware",
		size:		0x00080000,
		offset:		0,
		mask_flags:	MTD_WRITEABLE  /* force read-only */
	},{
		name:		"Kernel",
		size:		0x000a0000,
		offset:		0x00080000,
	},{
		name:		"Filesystem",
		size:		0x00600000,
		offset:		0x00120000
	},{
		name:		"RedBoot",
		size:		0x000e0000,
		offset:		0x00720000,
		mask_flags:	MTD_WRITEABLE
	}
};

#define NB_OF(x)  (sizeof(x)/sizeof(x[0]))

static struct mtd_info *mymtd;
static struct mtd_partition *parsed_parts;

extern int parse_redboot_partitions(struct mtd_info *master, struct mtd_partition **pparts);

static int __init init_iq80310(void)
{
	struct mtd_partition *parts;
	int nb_parts = 0;
	int parsed_nr_parts = 0;
	char *part_type = "static";

	iq80310_map.map_priv_1 = (unsigned long)ioremap(WINDOW_ADDR, WINDOW_SIZE);
	if (!iq80310_map.map_priv_1) {
		printk("Failed to ioremap\n");
		return -EIO;
	}
	mymtd = do_map_probe("cfi_probe", &iq80310_map);
	if (!mymtd) {
		iounmap((void *)iq80310_map.map_priv_1);
		return -ENXIO;
	}
	mymtd->module = THIS_MODULE;

#ifdef CONFIG_MTD_REDBOOT_PARTS
	if (parsed_nr_parts == 0) {
		int ret = parse_redboot_partitions(mymtd, &parsed_parts);

		if (ret > 0) {
			part_type = "RedBoot";
			parsed_nr_parts = ret;
		}
	}
#endif

	if (parsed_nr_parts > 0) {
		parts = parsed_parts;
		nb_parts = parsed_nr_parts;
	} else {
		parts = iq80310_partitions;
		nb_parts = NB_OF(iq80310_partitions);
	}
	printk(KERN_NOTICE "Using %s partition definition\n", part_type);
	add_mtd_partitions(mymtd, parts, nb_parts);
	return 0;
}

static void __exit cleanup_iq80310(void)
{
	if (mymtd) {
		del_mtd_partitions(mymtd);
		map_destroy(mymtd);
		if (parsed_parts)
			kfree(parsed_parts);
	}
	if (iq80310_map.map_priv_1)
		iounmap((void *)iq80310_map.map_priv_1);
}

module_init(init_iq80310);
module_exit(cleanup_iq80310);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nicolas Pitre <nico@cam.org>");
MODULE_DESCRIPTION("MTD map driver for Intel XScale IQ80310 evaluation board");
