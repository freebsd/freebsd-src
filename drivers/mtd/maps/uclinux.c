/****************************************************************************/

/*
 *	uclinux.c -- generic memory mapped MTD driver for uclinux
 *
 *	(C) Copyright 2002, Greg Ungerer (gerg@snapgear.com)
 *
 * 	$Id: uclinux.c,v 1.2 2002/08/07 00:43:45 gerg Exp $
 */

/****************************************************************************/

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/major.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <asm/io.h>

/****************************************************************************/

__u8 uclinux_read8(struct map_info *map, unsigned long ofs)
{
	return(*((__u8 *) (map->map_priv_1 + ofs)));
}

__u16 uclinux_read16(struct map_info *map, unsigned long ofs)
{
	return(*((__u16 *) (map->map_priv_1 + ofs)));
}

__u32 uclinux_read32(struct map_info *map, unsigned long ofs)
{
	return(*((__u32 *) (map->map_priv_1 + ofs)));
}

void uclinux_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy(to, (void *)(map->map_priv_1 + from), len);
}

void uclinux_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	*((__u8 *) (map->map_priv_1 + adr)) = d;
}

void uclinux_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	*((__u16 *) (map->map_priv_1 + adr)) = d;
}

void uclinux_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	*((__u32 *) (map->map_priv_1 + adr)) = d;
}

void uclinux_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy((void *) (map->map_priv_1 + to), from, len);
}

/****************************************************************************/

struct map_info uclinux_ram_map = {
	name:		"RAM",
	read8:		uclinux_read8,
	read16:		uclinux_read16,
	read32:		uclinux_read32,
	copy_from:	uclinux_copy_from,
	write8:		uclinux_write8,
	write16:	uclinux_write16,
	write32:	uclinux_write32,
	copy_to:	uclinux_copy_to,
};

struct mtd_info *uclinux_ram_mtdinfo;

/****************************************************************************/

struct mtd_partition uclinux_romfs[] = {
	{ name: "ROMfs", offset: 0 }
};

#define	NUM_PARTITIONS	(sizeof(uclinux_romfs) / sizeof(uclinux_romfs[0]))

/****************************************************************************/

int uclinux_point(struct mtd_info *mtd, loff_t from, size_t len,
	size_t *retlen, u_char **mtdbuf)
{
	struct map_info *map = (struct map_info *) mtd->priv;
	*mtdbuf = (u_char *) (map->map_priv_1 + ((int) from));
	*retlen = len;
	return(0);
}

/****************************************************************************/

int __init uclinux_mtd_init(void)
{
	struct mtd_info *mtd;
	struct map_info *mapp;
	extern char _ebss;

	mapp = &uclinux_ram_map;
	mapp->map_priv_2 = (unsigned long) &_ebss;
	mapp->size = PAGE_ALIGN(*((unsigned long *)((&_ebss) + 8)));
	mapp->buswidth = 4;

	printk("uclinux[mtd]: RAM probe address=0x%x size=0x%x\n",
	       	(int) mapp->map_priv_2, (int) mapp->size);

	mapp->map_priv_1 = (unsigned long)
		ioremap_nocache(mapp->map_priv_2, mapp->size);

	if (mapp->map_priv_1 == 0) {
		printk("uclinux[mtd]: ioremap_nocache() failed\n");
		return(-EIO);
	}

	mtd = do_map_probe("map_ram", mapp);
	if (!mtd) {
		printk("uclinux[mtd]: failed to find a mapping?\n");
		iounmap((void *) mapp->map_priv_1);
		return(-ENXIO);
	}
		
	mtd->module = THIS_MODULE;
	mtd->point = uclinux_point;
	mtd->priv = mapp;

	uclinux_ram_mtdinfo = mtd;
	add_mtd_partitions(mtd, uclinux_romfs, NUM_PARTITIONS);

	printk("uclinux[mtd]: set %s to be root filesystem\n",
	     	uclinux_romfs[0].name);
	ROOT_DEV = MKDEV(MTD_BLOCK_MAJOR, 0);
	put_mtd_device(mtd);

	return(0);
}

/****************************************************************************/

void __exit uclinux_mtd_cleanup(void)
{
	if (uclinux_ram_mtdinfo) {
		del_mtd_partitions(uclinux_ram_mtdinfo);
		map_destroy(uclinux_ram_mtdinfo);
		uclinux_ram_mtdinfo = NULL;
	}
	if (uclinux_ram_map.map_priv_1) {
		iounmap((void *) uclinux_ram_map.map_priv_1);
		uclinux_ram_map.map_priv_1 = 0;
	}
}

/****************************************************************************/

module_init(uclinux_mtd_init);
module_exit(uclinux_mtd_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Greg Ungerer <gerg@snapgear.com>");
MODULE_DESCRIPTION("Generic RAM based MTD for uClinux");

/****************************************************************************/
