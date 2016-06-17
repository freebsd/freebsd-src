/*
 * MTD map driver for flash on the DC21285 (the StrongARM-110 companion chip)
 *
 * (C) 2000  Nicolas Pitre <nico@cam.org>
 *
 * This code is GPL
 * 
 * $Id: dc21285.c,v 1.9 2002/10/14 12:22:10 rmk Exp $
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#include <asm/io.h>
#include <asm/hardware/dec21285.h>


static struct mtd_info *mymtd;

__u8 dc21285_read8(struct map_info *map, unsigned long ofs)
{
	return *(__u8*)(map->map_priv_1 + ofs);
}

__u16 dc21285_read16(struct map_info *map, unsigned long ofs)
{
	return *(__u16*)(map->map_priv_1 + ofs);
}

__u32 dc21285_read32(struct map_info *map, unsigned long ofs)
{
	return *(__u32*)(map->map_priv_1 + ofs);
}

void dc21285_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy(to, (void*)(map->map_priv_1 + from), len);
}

void dc21285_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	*CSR_ROMWRITEREG = adr & 3;
	adr &= ~3;
	*(__u8*)(map->map_priv_1 + adr) = d;
}

void dc21285_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	*CSR_ROMWRITEREG = adr & 3;
	adr &= ~3;
	*(__u16*)(map->map_priv_1 + adr) = d;
}

void dc21285_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	*(__u32*)(map->map_priv_1 + adr) = d;
}

void dc21285_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	switch (map->buswidth) {
		case 4:
			while (len > 0) {
				__u32 d = *((__u32*)from)++;
				dc21285_write32(map, d, to);
				to += 4;
				len -= 4;
			}
			break;
		case 2:
			while (len > 0) {
				__u16 d = *((__u16*)from)++;
				dc21285_write16(map, d, to);
				to += 2;
				len -= 2;
			}
			break;
		case 1:
			while (len > 0) {
				__u8 d = *((__u8*)from)++;
				dc21285_write8(map, d, to);
				to++;
				len--;
			}
			break;
	}
}

struct map_info dc21285_map = {
	name: "DC21285 flash",
	size: 16*1024*1024,
	read8: dc21285_read8,
	read16: dc21285_read16,
	read32: dc21285_read32,
	copy_from: dc21285_copy_from,
	write8: dc21285_write8,
	write16: dc21285_write16,
	write32: dc21285_write32,
	copy_to: dc21285_copy_to
};


/* Partition stuff */
static struct mtd_partition *dc21285_parts;
		      
extern int parse_redboot_partitions(struct mtd_info *, struct mtd_partition **);

int __init init_dc21285(void)
{
	/* Determine buswidth */
	switch (*CSR_SA110_CNTL & (3<<14)) {
		case SA110_CNTL_ROMWIDTH_8: 
			dc21285_map.buswidth = 1;
			break;
		case SA110_CNTL_ROMWIDTH_16: 
			dc21285_map.buswidth = 2; 
			break;
		case SA110_CNTL_ROMWIDTH_32: 
			dc21285_map.buswidth = 4; 
			break;
		default:
			printk (KERN_ERR "DC21285 flash: undefined buswidth\n");
			return -ENXIO;
	}
	printk (KERN_NOTICE "DC21285 flash support (%d-bit buswidth)\n",
		dc21285_map.buswidth*8);

	/* Let's map the flash area */
	dc21285_map.map_priv_1 = (unsigned long)ioremap(DC21285_FLASH, 16*1024*1024);
	if (!dc21285_map.map_priv_1) {
		printk("Failed to ioremap\n");
		return -EIO;
	}

	mymtd = do_map_probe("cfi_probe", &dc21285_map);
	if (mymtd) {
		int nrparts = 0;

		mymtd->module = THIS_MODULE;
			
		/* partition fixup */

#ifdef CONFIG_MTD_REDBOOT_PARTS
		nrparts = parse_redboot_partitions(mymtd, &dc21285_parts);
#endif
		if (nrparts > 0) {
			add_mtd_partitions(mymtd, dc21285_parts, nrparts);
		} else if (nrparts == 0) {
			printk(KERN_NOTICE "RedBoot partition table failed\n");
			add_mtd_device(mymtd);
		}

		/* 
		 * Flash timing is determined with bits 19-16 of the
		 * CSR_SA110_CNTL.  The value is the number of wait cycles, or
		 * 0 for 16 cycles (the default).  Cycles are 20 ns.
		 * Here we use 7 for 140 ns flash chips.
		 */
		/* access time */
		*CSR_SA110_CNTL = ((*CSR_SA110_CNTL & ~0x000f0000) | (7 << 16));
		/* burst time */
		*CSR_SA110_CNTL = ((*CSR_SA110_CNTL & ~0x00f00000) | (7 << 20));
		/* tristate time */
		*CSR_SA110_CNTL = ((*CSR_SA110_CNTL & ~0x0f000000) | (7 << 24));

		return 0;
	}

	iounmap((void *)dc21285_map.map_priv_1);
	return -ENXIO;
}

static void __exit cleanup_dc21285(void)
{
	if (mymtd) {
		del_mtd_device(mymtd);
		map_destroy(mymtd);
		mymtd = NULL;
	}
	if (dc21285_map.map_priv_1) {
		iounmap((void *)dc21285_map.map_priv_1);
		dc21285_map.map_priv_1 = 0;
	}
	if(dc21285_parts)
		kfree(dc21285_parts);
}

module_init(init_dc21285);
module_exit(cleanup_dc21285);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nicolas Pitre <nico@cam.org>");
MODULE_DESCRIPTION("MTD map driver for DC21285 boards");
