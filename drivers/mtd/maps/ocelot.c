/*
 * $Id: ocelot.c,v 1.6 2001/10/02 15:05:14 dwmw2 Exp $
 *
 * Flash on Momenco Ocelot
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#define OCELOT_PLD 0x2c000000
#define FLASH_WINDOW_ADDR 0x2fc00000
#define FLASH_WINDOW_SIZE 0x00080000
#define FLASH_BUSWIDTH 1
#define NVRAM_WINDOW_ADDR 0x2c800000
#define NVRAM_WINDOW_SIZE 0x00007FF0
#define NVRAM_BUSWIDTH 1

extern int parse_redboot_partitions(struct mtd_info *master, struct mtd_partition **pparts);

static unsigned int cacheflush = 0;

static struct mtd_info *flash_mtd;
static struct mtd_info *nvram_mtd;

__u8 ocelot_read8(struct map_info *map, unsigned long ofs)
{
	return __raw_readb(map->map_priv_1 + ofs);
}

void ocelot_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	cacheflush = 1;
	__raw_writeb(d, map->map_priv_1 + adr);
	mb();
}

void ocelot_copy_from_cache(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	if (cacheflush) {
		dma_cache_inv(map->map_priv_2, map->size);
		cacheflush = 0;
	}
	memcpy_fromio(to, map->map_priv_1 + from, len);
}

void ocelot_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to, map->map_priv_1 + from, len);
}

void ocelot_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	/* If we use memcpy, it does word-wide writes. Even though we told the 
	   GT64120A that it's an 8-bit wide region, word-wide writes don't work.
	   We end up just writing the first byte of the four to all four bytes.
	   So we have this loop instead */
	while(len) {
		__raw_writeb(*(unsigned char *) from, map->map_priv_1 + to);
		from++;
		to++;
		len--;
	}
}

static struct mtd_partition *parsed_parts;

struct map_info ocelot_flash_map = {
	name: "Ocelot boot flash",
	size: FLASH_WINDOW_SIZE,
	buswidth: FLASH_BUSWIDTH,
	read8: ocelot_read8,
	copy_from: ocelot_copy_from_cache,
	write8: ocelot_write8,
};

struct map_info ocelot_nvram_map = {
	name: "Ocelot NVRAM",
	size: NVRAM_WINDOW_SIZE,
	buswidth: NVRAM_BUSWIDTH,
	read8: ocelot_read8,
	copy_from: ocelot_copy_from,
	write8: ocelot_write8,
	copy_to: ocelot_copy_to
};

static int __init init_ocelot_maps(void)
{
	void *pld;
	int nr_parts;
	unsigned char brd_status;

       	printk(KERN_INFO "Momenco Ocelot MTD mappings: Flash 0x%x at 0x%x, NVRAM 0x%x at 0x%x\n", 
	       FLASH_WINDOW_SIZE, FLASH_WINDOW_ADDR, NVRAM_WINDOW_SIZE, NVRAM_WINDOW_ADDR);

	/* First check whether the flash jumper is present */
	pld = ioremap(OCELOT_PLD, 0x10);
	if (!pld) {
		printk(KERN_NOTICE "Failed to ioremap Ocelot PLD\n");
		return -EIO;
	}
	brd_status = readb(pld+4);
	iounmap(pld);

	/* Now ioremap the NVRAM space */
	ocelot_nvram_map.map_priv_1 = (unsigned long)ioremap_nocache(NVRAM_WINDOW_ADDR, NVRAM_WINDOW_SIZE);
	if (!ocelot_nvram_map.map_priv_1) {
		printk(KERN_NOTICE "Failed to ioremap Ocelot NVRAM space\n");
		return -EIO;
	}
	//	ocelot_nvram_map.map_priv_2 = ocelot_nvram_map.map_priv_1;

	/* And do the RAM probe on it to get an MTD device */
	nvram_mtd = do_map_probe("map_ram", &ocelot_nvram_map);
	if (!nvram_mtd) {
		printk("NVRAM probe failed\n");
		goto fail_1;
	}
	nvram_mtd->module = THIS_MODULE;
	nvram_mtd->erasesize = 16;

	/* Now map the flash space */
	ocelot_flash_map.map_priv_1 = (unsigned long)ioremap_nocache(FLASH_WINDOW_ADDR, FLASH_WINDOW_SIZE);
	if (!ocelot_flash_map.map_priv_1) {
		printk(KERN_NOTICE "Failed to ioremap Ocelot flash space\n");
		goto fail_2;
	}
	/* Now the cached version */
	ocelot_flash_map.map_priv_2 = (unsigned long)__ioremap(FLASH_WINDOW_ADDR, FLASH_WINDOW_SIZE, 0);

	if (!ocelot_flash_map.map_priv_2) {
		/* Doesn't matter if it failed. Just use the uncached version */
		ocelot_flash_map.map_priv_2 = ocelot_flash_map.map_priv_1;
	}

	/* Only probe for flash if the write jumper is present */
	if (brd_status & 0x40) {
		flash_mtd = do_map_probe("jedec", &ocelot_flash_map);
	} else {
		printk(KERN_NOTICE "Ocelot flash write jumper not present. Treating as ROM\n");
	}
	/* If that failed or the jumper's absent, pretend it's ROM */
	if (!flash_mtd) {
		flash_mtd = do_map_probe("map_rom", &ocelot_flash_map);
		/* If we're treating it as ROM, set the erase size */
		if (flash_mtd)
			flash_mtd->erasesize = 0x10000;
	}
	if (!flash_mtd)
		goto fail3;

	add_mtd_device(nvram_mtd);

	flash_mtd->module = THIS_MODULE;
	nr_parts = parse_redboot_partitions(flash_mtd, &parsed_parts);

	if (nr_parts)
		add_mtd_partitions(flash_mtd, parsed_parts, nr_parts);
	else
		add_mtd_device(flash_mtd);

	return 0;
	
 fail3:	
	iounmap((void *)ocelot_flash_map.map_priv_1);
	if (ocelot_flash_map.map_priv_2 &&
	    ocelot_flash_map.map_priv_2 != ocelot_flash_map.map_priv_1)
			iounmap((void *)ocelot_flash_map.map_priv_2);
 fail_2:
	map_destroy(nvram_mtd);
 fail_1:
	iounmap((void *)ocelot_nvram_map.map_priv_1);

	return -ENXIO;
}

static void __exit cleanup_ocelot_maps(void)
{
	del_mtd_device(nvram_mtd);
	map_destroy(nvram_mtd);
	iounmap((void *)ocelot_nvram_map.map_priv_1);

	if (parsed_parts)
		del_mtd_partitions(flash_mtd);
	else
		del_mtd_device(flash_mtd);
	map_destroy(flash_mtd);
	iounmap((void *)ocelot_flash_map.map_priv_1);
	if (ocelot_flash_map.map_priv_2 != ocelot_flash_map.map_priv_1)
		iounmap((void *)ocelot_flash_map.map_priv_2);
}

module_init(init_ocelot_maps);
module_exit(cleanup_ocelot_maps);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Red Hat, Inc. - David Woodhouse <dwmw2@cambridge.redhat.com>");
MODULE_DESCRIPTION("MTD map driver for Momenco Ocelot board");
