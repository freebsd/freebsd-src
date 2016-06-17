/*
 * $Id: mbx860.c,v 1.1 2001/11/18 19:43:09 dwmw2 Exp $
 *
 * Handle mapping of the flash on MBX860 boards
 *
 * Author:	Anton Todorov
 * Copyright:	(C) 2001 Emness Technology
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>


#define WINDOW_ADDR 0xfe000000
#define WINDOW_SIZE 0x00200000

/* Flash / Partition sizing */
#define MAX_SIZE_KiB              8192
#define BOOT_PARTITION_SIZE_KiB    512
#define KERNEL_PARTITION_SIZE_KiB 5632
#define APP_PARTITION_SIZE_KiB    2048

#define NUM_PARTITIONS 3

/* partition_info gives details on the logical partitions that the split the
 * single flash device into. If the size if zero we use up to the end of the
 * device. */
static struct mtd_partition partition_info[]={
	{ name: "MBX flash BOOT partition",
	offset: 0,
	size:   BOOT_PARTITION_SIZE_KiB*1024 },
	{ name: "MBX flash DATA partition",
	offset: BOOT_PARTITION_SIZE_KiB*1024,
	size: (KERNEL_PARTITION_SIZE_KiB)*1024 },
	{ name: "MBX flash APPLICATION partition",
	offset: (BOOT_PARTITION_SIZE_KiB+KERNEL_PARTITION_SIZE_KiB)*1024 }
};
				   

static struct mtd_info *mymtd;

__u8 mbx_read8(struct map_info *map, unsigned long ofs)
{
	return readb(map->map_priv_1 + ofs);
}

__u16 mbx_read16(struct map_info *map, unsigned long ofs)
{
	return readw(map->map_priv_1 + ofs);
}

__u32 mbx_read32(struct map_info *map, unsigned long ofs)
{
	return readl(map->map_priv_1 + ofs);
}

void mbx_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to, (void *)(map->map_priv_1 + from), len);
}

void mbx_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	writeb(d, map->map_priv_1 + adr);
}

void mbx_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	writew(d, map->map_priv_1 + adr);
}

void mbx_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	writel(d, map->map_priv_1 + adr);
}

void mbx_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy_toio((void *)(map->map_priv_1 + to), from, len);
}

struct map_info mbx_map = {
	name: "MBX flash",
	size: WINDOW_SIZE,
	buswidth: 4,
	read8: mbx_read8,
	read16: mbx_read16,
	read32: mbx_read32,
	copy_from: mbx_copy_from,
	write8: mbx_write8,
	write16: mbx_write16,
	write32: mbx_write32,
	copy_to: mbx_copy_to
};

int __init init_mbx(void)
{
	printk(KERN_NOTICE "Motorola MBX flash device: %x at %x\n", WINDOW_SIZE*4, WINDOW_ADDR);
	mbx_map.map_priv_1 = (unsigned long)ioremap(WINDOW_ADDR, WINDOW_SIZE * 4);

	if (!mbx_map.map_priv_1) {
		printk("Failed to ioremap\n");
		return -EIO;
	}
	mymtd = do_map_probe("jedec_probe", &mbx_map);
	if (mymtd) {
		mymtd->module = THIS_MODULE;
		add_mtd_device(mymtd);
                add_mtd_partitions(mymtd, partition_info, NUM_PARTITIONS);
		return 0;
	}

	iounmap((void *)mbx_map.map_priv_1);
	return -ENXIO;
}

static void __exit cleanup_mbx(void)
{
	if (mymtd) {
		del_mtd_device(mymtd);
		map_destroy(mymtd);
	}
	if (mbx_map.map_priv_1) {
		iounmap((void *)mbx_map.map_priv_1);
		mbx_map.map_priv_1 = 0;
	}
}

module_init(init_mbx);
module_exit(cleanup_mbx);

MODULE_AUTHOR("Anton Todorov <a.todorov@emness.com>");
MODULE_DESCRIPTION("MTD map driver for Motorola MBX860 board");
MODULE_LICENSE("GPL");
