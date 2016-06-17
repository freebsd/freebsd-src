/*
 * Flash device on lasat 100 and 200 boards
 *
 * Presumably (C) 2002 Brian Murphy <brian@murphy.dk> or whoever he
 * works for.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * $Id: lasat.c,v 1.1 2003/01/24 14:26:38 dwmw2 Exp $
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/config.h>
#include <asm/lasat/lasat.h>
#include <asm/lasat/lasat_mtd.h>

static struct mtd_info *mymtd;

static __u8 sp_read8(struct map_info *map, unsigned long ofs)
{
	return __raw_readb(map->map_priv_1 + ofs);
}

static __u16 sp_read16(struct map_info *map, unsigned long ofs)
{
	return __raw_readw(map->map_priv_1 + ofs);
}

static __u32 sp_read32(struct map_info *map, unsigned long ofs)
{
	return __raw_readl(map->map_priv_1 + ofs);
}

static void sp_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to, map->map_priv_1 + from, len);
}

static void sp_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	__raw_writeb(d, map->map_priv_1 + adr);
	mb();
}

static void sp_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	__raw_writew(d, map->map_priv_1 + adr);
	mb();
}

static void sp_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	__raw_writel(d, map->map_priv_1 + adr);
	mb();
}

static void sp_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy_toio(map->map_priv_1 + to, from, len);
}

static struct map_info sp_map = {
	.name = "SP flash",
	.buswidth = 4,
	.read8 = sp_read8,
	.read16 = sp_read16,
	.read32 = sp_read32,
	.copy_from = sp_copy_from,
	.write8 = sp_write8,
	.write16 = sp_write16,
	.write32 = sp_write32,
	.copy_to = sp_copy_to
};

static struct mtd_partition partition_info[LASAT_MTD_LAST];
static char *lasat_mtd_partnames[] = {"Bootloader", "Service", "Normal", "Filesystem", "Config"};

static int __init init_sp(void)
{
	int i;
	/* this does not play well with the old flash code which 
	 * protects and uprotects the flash when necessary */
       	printk(KERN_NOTICE "Unprotecting flash\n");
	*lasat_misc->flash_wp_reg |= 1 << lasat_misc->flash_wp_bit;

	sp_map.map_priv_1 = lasat_flash_partition_start(LASAT_MTD_BOOTLOADER);
	sp_map.size = lasat_board_info.li_flash_size;

       	printk(KERN_NOTICE "sp flash device: %lx at %lx\n", 
			sp_map.size, sp_map.map_priv_1);

	for (i=0; i < LASAT_MTD_LAST; i++)
		partition_info[i].name = lasat_mtd_partnames[i];

	mymtd = do_map_probe("cfi_probe", &sp_map);
	if (mymtd) {
		u32 size, offset = 0;

		mymtd->module = THIS_MODULE;

		for (i=0; i < LASAT_MTD_LAST; i++) {
			size = lasat_flash_partition_size(i);
			partition_info[i].size = size;
			partition_info[i].offset = offset;
			offset += size;
		}

		add_mtd_partitions( mymtd, partition_info, LASAT_MTD_LAST );
		return 0;
	}

	return -ENXIO;
}

static void __exit cleanup_sp(void)
{
	if (mymtd) {
		del_mtd_partitions(mymtd);
		map_destroy(mymtd);
	}
	if (sp_map.map_priv_1) {
		sp_map.map_priv_1 = 0;
	}
}

module_init(init_sp);
module_exit(cleanup_sp);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Brian Murphy <brian@murphy.dk>");
MODULE_DESCRIPTION("Lasat Safepipe/Masquerade MTD map driver");
