/*
 * NV-RAM memory access on autcpu12 
 * (C) 2002 Thomas Gleixner (gleixner@autronix.de)
 *
 * $Id: autcpu12-nvram.c,v 1.1 2002/02/22 09:30:24 gleixner Exp $ 
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
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <asm/io.h>
#include <asm/sizes.h>
#include <asm/hardware.h>
#include <asm/arch/autcpu12.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

__u8 autcpu12_read8(struct map_info *map, unsigned long ofs)
{
	return __raw_readb(map->map_priv_1 + ofs);
}

__u16 autcpu12_read16(struct map_info *map, unsigned long ofs)
{
	return __raw_readw(map->map_priv_1 + ofs);
}

__u32 autcpu12_read32(struct map_info *map, unsigned long ofs)
{
	return __raw_readl(map->map_priv_1 + ofs);
}

void autcpu12_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	__raw_writeb(d, map->map_priv_1 + adr);
	mb();
}

void autcpu12_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	__raw_writew(d, map->map_priv_1 + adr);
	mb();
}

void autcpu12_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	__raw_writel(d, map->map_priv_1 + adr);
	mb();
}

void autcpu12_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to, map->map_priv_1 + from, len);
}

void autcpu12_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	while(len) {
		__raw_writeb(*(unsigned char *) from, map->map_priv_1 + to);
		from++;
		to++;
		len--;
	}
}

static struct mtd_info *sram_mtd;

struct map_info autcpu12_sram_map = {
	name: "SRAM",
	size: 32768,
	buswidth: 8,
	read8: autcpu12_read8,
	read16: autcpu12_read16,
	read32: autcpu12_read32,
	copy_from: autcpu12_copy_from,
	write8: autcpu12_write8,
	write16: autcpu12_write16,
	write32: autcpu12_write32,
	copy_to: autcpu12_copy_to
};

static int __init init_autcpu12_sram (void)
{
	int err, save0, save1;

	autcpu12_sram_map.map_priv_1 = (unsigned long)ioremap(0x12000000, SZ_128K);
	if (!autcpu12_sram_map.map_priv_1) {
		printk("Failed to ioremap autcpu12 NV-RAM space\n");
		err = -EIO;
		goto out;
	}
	
	/* 
	 * Check for 32K/128K 
	 * read ofs 0 
	 * read ofs 0x10000 
	 * Write complement to ofs 0x100000
	 * Read	and check result on ofs 0x0
	 * Restore contents
	 */
	save0 = autcpu12_read32(&autcpu12_sram_map,0);
	save1 = autcpu12_read32(&autcpu12_sram_map,0x10000);
	autcpu12_write32(&autcpu12_sram_map,~save0,0x10000);
	/* if we find this pattern on 0x0, we have 32K size 
	 * restore contents and exit
	 */
	if ( autcpu12_read32(&autcpu12_sram_map,0) != save0) {
		autcpu12_write32(&autcpu12_sram_map,save0,0x0);
		goto map;
	}
	/* We have a 128K found, restore 0x10000 and set size
	 * to 128K
	 */
	autcpu12_write32(&autcpu12_sram_map,save1,0x10000);
	autcpu12_sram_map.size = SZ_128K;

map:
	sram_mtd = do_map_probe("map_ram", &autcpu12_sram_map);
	if (!sram_mtd) {
		printk("NV-RAM probe failed\n");
		err = -ENXIO;
		goto out_ioremap;
	}

	sram_mtd->module = THIS_MODULE;
	sram_mtd->erasesize = 16;
	
	if (add_mtd_device(sram_mtd)) {
		printk("NV-RAM device addition failed\n");
		err = -ENOMEM;
		goto out_probe;
	}

	printk("NV-RAM device size %ldK registered on AUTCPU12\n",autcpu12_sram_map.size/SZ_1K);
		
	return 0;

out_probe:
	map_destroy(sram_mtd);
	sram_mtd = 0;

out_ioremap:
	iounmap((void *)autcpu12_sram_map.map_priv_1);
out:
	return err;
}

static void __exit cleanup_autcpu12_maps(void)
{
	if (sram_mtd) {
		del_mtd_device(sram_mtd);
		map_destroy(sram_mtd);
		iounmap((void *)autcpu12_sram_map.map_priv_1);
	}
}

module_init(init_autcpu12_sram);
module_exit(cleanup_autcpu12_maps);

MODULE_AUTHOR("Thomas Gleixner");
MODULE_DESCRIPTION("autcpu12 NV-RAM map driver");
MODULE_LICENSE("GPL");
