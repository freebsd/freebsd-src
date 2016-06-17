/*
 * $Id: impa7.c,v 1.2 2002/09/05 05:11:24 acurtis Exp $
 *
 * Handle mapping of the NOR flash on implementa A7 boards
 *
 * Copyright 2002 SYSGO Real-Time Solutions GmbH
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
#include <linux/config.h>

#ifdef CONFIG_MTD_PARTITIONS
#include <linux/mtd/partitions.h>
#endif

#define WINDOW_ADDR0 0x00000000      /* physical properties of flash */
#define WINDOW_SIZE0 0x00800000
#define WINDOW_ADDR1 0x10000000      /* physical properties of flash */
#define WINDOW_SIZE1 0x00800000
#define NUM_FLASHBANKS 2
#define BUSWIDTH     4

/* can be { "cfi_probe", "jedec_probe", "map_rom", 0 }; */
#define PROBETYPES { "jedec_probe", 0 }

#define MSG_PREFIX "impA7:"   /* prefix for our printk()'s */
#define MTDID      "impa7-%d"  /* for mtdparts= partitioning */

static struct mtd_info *impa7_mtd[NUM_FLASHBANKS] = { 0 };

__u8 impa7_read8(struct map_info *map, unsigned long ofs)
{
	return __raw_readb(map->map_priv_1 + ofs);
}

__u16 impa7_read16(struct map_info *map, unsigned long ofs)
{
	return __raw_readw(map->map_priv_1 + ofs);
}

__u32 impa7_read32(struct map_info *map, unsigned long ofs)
{
	return __raw_readl(map->map_priv_1 + ofs);
}

void impa7_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to, map->map_priv_1 + from, len);
}

void impa7_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	__raw_writeb(d, map->map_priv_1 + adr);
	mb();
}

void impa7_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	__raw_writew(d, map->map_priv_1 + adr);
	mb();
}

void impa7_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	__raw_writel(d, map->map_priv_1 + adr);
	mb();
}

void impa7_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy_toio(map->map_priv_1 + to, from, len);
}

static struct map_info impa7_map[NUM_FLASHBANKS] = {
	{
	name: "impA7 NOR Flash Bank #0",
	size: WINDOW_SIZE0,
	buswidth: BUSWIDTH,
	read8: impa7_read8,
	read16: impa7_read16,
	read32: impa7_read32,
	copy_from: impa7_copy_from,
	write8: impa7_write8,
	write16: impa7_write16,
	write32: impa7_write32,
	copy_to: impa7_copy_to
	},
	{
	name: "impA7 NOR Flash Bank #1",
	size: WINDOW_SIZE1,
	buswidth: BUSWIDTH,
	read8: impa7_read8,
	read16: impa7_read16,
	read32: impa7_read32,
	copy_from: impa7_copy_from,
	write8: impa7_write8,
	write16: impa7_write16,
	write32: impa7_write32,
	copy_to: impa7_copy_to
	},
};

#ifdef CONFIG_MTD_PARTITIONS

/*
 * MTD partitioning stuff 
 */
static struct mtd_partition static_partitions[] =
{
    {
	name: "FileSystem",
	  size: 0x800000,
	  offset: 0x00000000
    },
};

#define NB_OF(x) (sizeof (x) / sizeof (x[0]))

#ifdef CONFIG_MTD_CMDLINE_PARTS
int parse_cmdline_partitions(struct mtd_info *master, 
			     struct mtd_partition **pparts,
			     const char *mtd_id);
#endif

#endif

static int                   mtd_parts_nb = 0;
static struct mtd_partition *mtd_parts    = 0;

int __init init_impa7(void)
{
	static const char *rom_probe_types[] = PROBETYPES;
	const char **type;
	const char *part_type = 0;
	int i;
	static struct { u_long addr; u_long size; } pt[NUM_FLASHBANKS] = {
	  { WINDOW_ADDR0, WINDOW_SIZE0 },
	  { WINDOW_ADDR1, WINDOW_SIZE1 },
        };
	char mtdid[10];
	int devicesfound = 0;

	for(i=0; i<NUM_FLASHBANKS; i++)
	{
		printk(KERN_NOTICE MSG_PREFIX "probing 0x%08lx at 0x%08lx\n",
		       pt[i].size, pt[i].addr);
		impa7_map[i].map_priv_1 = (unsigned long)
		  ioremap(pt[i].addr, pt[i].size);

		if (!impa7_map[i].map_priv_1) {
			printk(MSG_PREFIX "failed to ioremap\n");
			return -EIO;
		}

		impa7_mtd[i] = 0;
		type = rom_probe_types;
		for(; !impa7_mtd[i] && *type; type++) {
			impa7_mtd[i] = do_map_probe(*type, &impa7_map[i]);
		}

		if (impa7_mtd[i]) 
		{
			impa7_mtd[i]->module = THIS_MODULE;
			add_mtd_device(impa7_mtd[i]);
			devicesfound++;
#ifdef CONFIG_MTD_PARTITIONS
#ifdef CONFIG_MTD_CMDLINE_PARTS
			sprintf(mtdid, MTDID, i);
			mtd_parts_nb = parse_cmdline_partitions(impa7_mtd[i], 
								&mtd_parts, 
								mtdid);
			if (mtd_parts_nb > 0)
			  part_type = "command line";
#endif
			if (mtd_parts_nb <= 0)
			{
				mtd_parts = static_partitions;
				mtd_parts_nb = NB_OF(static_partitions);
				part_type = "static";
			}
			if (mtd_parts_nb <= 0)
			{
				printk(KERN_NOTICE MSG_PREFIX 
				       "no partition info available\n");
			}
			else
			{
				printk(KERN_NOTICE MSG_PREFIX
				       "using %s partition definition\n", 
				       part_type);
				add_mtd_partitions(impa7_mtd[i], 
						   mtd_parts, mtd_parts_nb);
			}
#endif
		}
		else 
		  iounmap((void *)impa7_map[i].map_priv_1);
	}
	return devicesfound == 0 ? -ENXIO : 0;
}

static void __exit cleanup_impa7(void)
{
	int i;
	for (i=0; i<NUM_FLASHBANKS; i++) 
	{
		if (impa7_mtd[i]) 
		{
			del_mtd_device(impa7_mtd[i]);
			map_destroy(impa7_mtd[i]);
		}
		if (impa7_map[i].map_priv_1)
		{
			iounmap((void *)impa7_map[i].map_priv_1);
			impa7_map[i].map_priv_1 = 0;
		}
	}
}

module_init(init_impa7);
module_exit(cleanup_impa7);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pavel Bartusek <pba@sysgo.de>");
MODULE_DESCRIPTION("MTD map driver for implementa impA7");
