/*
 *	pnc2000.c - mapper for Photron PNC-2000 board.
 *
 * Copyright (C) 2000 Crossnet Co. <info@crossnet.co.jp>
 *
 * This code is GPL
 *
 * $Id: pnc2000.c,v 1.10 2001/10/02 15:05:14 dwmw2 Exp $
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>


#define WINDOW_ADDR 0xbf000000
#define WINDOW_SIZE 0x00400000

/* 
 * MAP DRIVER STUFF
 */

__u8 pnc_read8(struct map_info *map, unsigned long ofs)
{
  return *(__u8 *)(WINDOW_ADDR + ofs);
}

__u16 pnc_read16(struct map_info *map, unsigned long ofs)
{
  return *(__u16 *)(WINDOW_ADDR + ofs);
}

__u32 pnc_read32(struct map_info *map, unsigned long ofs)
{
  return *(volatile unsigned int *)(WINDOW_ADDR + ofs);
}

void pnc_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
  memcpy(to, (void *)(WINDOW_ADDR + from), len);
}

void pnc_write8(struct map_info *map, __u8 d, unsigned long adr)
{
  *(__u8 *)(WINDOW_ADDR + adr) = d;
}

void pnc_write16(struct map_info *map, __u16 d, unsigned long adr)
{
  *(__u16 *)(WINDOW_ADDR + adr) = d;
}

void pnc_write32(struct map_info *map, __u32 d, unsigned long adr)
{
  *(__u32 *)(WINDOW_ADDR + adr) = d;
}

void pnc_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
  memcpy((void *)(WINDOW_ADDR + to), from, len);
}

struct map_info pnc_map = {
	name: "PNC-2000",
	size: WINDOW_SIZE,
	buswidth: 4,
	read8: pnc_read8,
	read16: pnc_read16,
	read32: pnc_read32,
	copy_from: pnc_copy_from,
	write8: pnc_write8,
	write16: pnc_write16,
	write32: pnc_write32,
	copy_to: pnc_copy_to
};


/*
 * MTD 'PARTITIONING' STUFF 
 */
static struct mtd_partition pnc_partitions[3] = {
	{
		name: "PNC-2000 boot firmware",
		size: 0x20000,
		offset: 0
	},
	{
		name: "PNC-2000 kernel",
		size: 0x1a0000,
		offset: 0x20000
	},
	{
		name: "PNC-2000 filesystem",
		size: 0x240000,
		offset: 0x1c0000
	}
};

/* 
 * This is the master MTD device for which all the others are just
 * auto-relocating aliases.
 */
static struct mtd_info *mymtd;

int __init init_pnc2000(void)
{
	printk(KERN_NOTICE "Photron PNC-2000 flash mapping: %x at %x\n", WINDOW_SIZE, WINDOW_ADDR);

	mymtd = do_map_probe("cfi_probe", &pnc_map);
	if (mymtd) {
		mymtd->module = THIS_MODULE;
		return add_mtd_partitions(mymtd, pnc_partitions, 3);
	}

	return -ENXIO;
}

static void __exit cleanup_pnc2000(void)
{
	if (mymtd) {
		del_mtd_partitions(mymtd);
		map_destroy(mymtd);
	}
}

module_init(init_pnc2000);
module_exit(cleanup_pnc2000);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Crossnet Co. <info@crossnet.co.jp>");
MODULE_DESCRIPTION("MTD map driver for Photron PNC-2000 board");
