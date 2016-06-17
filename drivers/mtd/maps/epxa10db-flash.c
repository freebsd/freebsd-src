/*
 * Flash memory access on EPXA based devices
 *
 * (C) 2000 Nicolas Pitre <nico@cam.org>
 *  Copyright (C) 2001 Altera Corporation
 *  Copyright (C) 2001 Red Hat, Inc.
 *
 * $Id: epxa10db-flash.c,v 1.4 2002/08/22 10:46:19 cdavies Exp $ 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#include <asm/hardware.h>
#ifdef CONFIG_EPXA10DB
#define BOARD_NAME "EPXA10DB"
#else
#define BOARD_NAME "EPXA1DB"
#endif

static int nr_parts = 0;
static struct mtd_partition *parts;

static struct mtd_info *mymtd;

extern int parse_redboot_partitions(struct mtd_info *, struct mtd_partition **);
static int epxa_default_partitions(struct mtd_info *master, struct mtd_partition **pparts);

static __u8 epxa_read8(struct map_info *map, unsigned long ofs)
{
	return __raw_readb(map->map_priv_1 + ofs);
}

static __u16 epxa_read16(struct map_info *map, unsigned long ofs)
{
	return __raw_readw(map->map_priv_1 + ofs);
}

static __u32 epxa_read32(struct map_info *map, unsigned long ofs)
{
	return __raw_readl(map->map_priv_1 + ofs);
}

static void epxa_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to, (void *)(map->map_priv_1 + from), len);
}

static void epxa_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	__raw_writeb(d, map->map_priv_1 + adr);
	mb();
}

static void epxa_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	__raw_writew(d, map->map_priv_1 + adr);
	mb();
}

static void epxa_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	__raw_writel(d, map->map_priv_1 + adr);
	mb();
}

static void epxa_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy_toio((void *)(map->map_priv_1 + to), from, len);
}



static struct map_info epxa_map = {
	name:		"EPXA flash",
	size:		FLASH_SIZE,
	buswidth:	2,
	read8:		epxa_read8,
	read16:		epxa_read16,
	read32:		epxa_read32,
	copy_from:	epxa_copy_from,
	write8:		epxa_write8,
	write16:	epxa_write16,
	write32:	epxa_write32,
	copy_to:	epxa_copy_to
};


static int __init epxa_mtd_init(void)
{
	int i;
	
	printk(KERN_NOTICE "%s flash device: %x at %x\n", BOARD_NAME, FLASH_SIZE, FLASH_START);
	epxa_map.map_priv_1 = (unsigned long)ioremap(FLASH_START, FLASH_SIZE);
	if (!epxa_map.map_priv_1) {
		printk("Failed to ioremap %s flash\n",BOARD_NAME);
		return -EIO;
	}

	mymtd = do_map_probe("cfi_probe", &epxa_map);
	if (!mymtd) {
		iounmap((void *)epxa_map.map_priv_1);
		return -ENXIO;
	}

	mymtd->module = THIS_MODULE;

	/* Unlock the flash device. */
	if(mymtd->unlock){
		for (i=0; i<mymtd->numeraseregions;i++){
			int j;
			for(j=0;j<mymtd->eraseregions[i].numblocks;j++){
				mymtd->unlock(mymtd,mymtd->eraseregions[i].offset + j * mymtd->eraseregions[i].erasesize,mymtd->eraseregions[i].erasesize);
			}
		}
	}

#ifdef CONFIG_MTD_REDBOOT_PARTS
	nr_parts = parse_redboot_partitions(mymtd, &parts);

	if (nr_parts > 0) {
		add_mtd_partitions(mymtd, parts, nr_parts);
		return 0;
	}
#endif
#ifdef CONFIG_MTD_AFS_PARTS
	nr_parts = parse_afs_partitions(mymtd, &parts);

	if (nr_parts > 0) {
		add_mtd_partitions(mymtd, parts, nr_parts);
		return 0;
	}
#endif

	/* No recognised partitioning schemes found - use defaults */
	nr_parts = epxa_default_partitions(mymtd, &parts);
	if (nr_parts > 0) {
		add_mtd_partitions(mymtd, parts, nr_parts);
		return 0;
	}

	/* If all else fails... */
	add_mtd_device(mymtd);
	return 0;
}

static void __exit epxa_mtd_cleanup(void)
{
	if (mymtd) {
		if (nr_parts)
			del_mtd_partitions(mymtd);
		else
			del_mtd_device(mymtd);
		map_destroy(mymtd);
	}
	if (epxa_map.map_priv_1) {
		iounmap((void *)epxa_map.map_priv_1);
		epxa_map.map_priv_1 = 0;
	}
}


/* 
 * This will do for now, once we decide which bootldr we're finally 
 * going to use then we'll remove this function and do it properly
 *
 * Partions are currently (as offsets from base of flash):
 * 0x00000000 - 0x003FFFFF - bootloader (!)
 * 0x00400000 - 0x00FFFFFF - Flashdisk
 */

static int __init epxa_default_partitions(struct mtd_info *master, struct mtd_partition **pparts)
{
	struct mtd_partition *parts;
	int ret, i;
	int npartitions = 0;
	char *names; 
	const char *name = "jffs";

	printk("Using default partitions for %s\n",BOARD_NAME);
	npartitions=1;
	parts = kmalloc(npartitions*sizeof(*parts)+strlen(name), GFP_KERNEL);
	memzero(parts,npartitions*sizeof(*parts)+strlen(name));
	if (!parts) {
		ret = -ENOMEM;
		goto out;
	}
	i=0;
	names = (char *)&parts[npartitions];	
	parts[i].name = names;
	names += strlen(name) + 1;
	strcpy(parts[i].name, name);

#ifdef CONFIG_EPXA10DB
	parts[i].size = FLASH_SIZE-0x00400000;
	parts[i].offset = 0x00400000;
#else
	parts[i].size = FLASH_SIZE-0x00180000;
	parts[i].offset = 0x00180000;
#endif

 out:
	*pparts = parts;
	return npartitions;
}


module_init(epxa_mtd_init);
module_exit(epxa_mtd_cleanup);

MODULE_AUTHOR("Clive Davies");
MODULE_DESCRIPTION("Altera epxa mtd flash map");
MODULE_LICENSE("GPL");
