/*
 * $Id: dbox2-flash.c,v 1.4 2001/10/02 15:05:14 dwmw2 Exp $
 *
 * Nokia / Sagem D-Box 2 flash driver
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/config.h>

/* partition_info gives details on the logical partitions that the split the
 * single flash device into. If the size if zero we use up to the end of the
 * device. */
static struct mtd_partition partition_info[]= {{name: "BR bootloader",		// raw
						      size: 128 * 1024, 
						      offset: 0,                  
						      mask_flags: MTD_WRITEABLE},
                                                     {name: "PPC bootloader",		// flfs
						      size: 128 * 1024, 
						      offset: MTDPART_OFS_APPEND, 
						      mask_flags: 0},
                                                     {name: "Kernel",			// idxfs
						      size: 768 * 1024, 
						      offset: MTDPART_OFS_APPEND, 
						      mask_flags: 0},
                                                     {name: "System",			// jffs
						      size: MTDPART_SIZ_FULL, 
						      offset: MTDPART_OFS_APPEND, 
						      mask_flags: 0}};

#define NUM_PARTITIONS (sizeof(partition_info) / sizeof(partition_info[0]))

#define WINDOW_ADDR 0x10000000
#define WINDOW_SIZE 0x800000

static struct mtd_info *mymtd;

__u8 dbox2_flash_read8(struct map_info *map, unsigned long ofs)
{
	return __raw_readb(map->map_priv_1 + ofs);
}

__u16 dbox2_flash_read16(struct map_info *map, unsigned long ofs)
{
	return __raw_readw(map->map_priv_1 + ofs);
}

__u32 dbox2_flash_read32(struct map_info *map, unsigned long ofs)
{
	return __raw_readl(map->map_priv_1 + ofs);
}

void dbox2_flash_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to, map->map_priv_1 + from, len);
}

void dbox2_flash_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	__raw_writeb(d, map->map_priv_1 + adr);
	mb();
}

void dbox2_flash_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	__raw_writew(d, map->map_priv_1 + adr);
	mb();
}

void dbox2_flash_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	__raw_writel(d, map->map_priv_1 + adr);
	mb();
}

void dbox2_flash_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy_toio(map->map_priv_1 + to, from, len);
}

struct map_info dbox2_flash_map = {
	name: "D-Box 2 flash memory",
	size: WINDOW_SIZE,
	buswidth: 4,
	read8: dbox2_flash_read8,
	read16: dbox2_flash_read16,
	read32: dbox2_flash_read32,
	copy_from: dbox2_flash_copy_from,
	write8: dbox2_flash_write8,
	write16: dbox2_flash_write16,
	write32: dbox2_flash_write32,
	copy_to: dbox2_flash_copy_to
};

int __init init_dbox2_flash(void)
{
       	printk(KERN_NOTICE "D-Box 2 flash driver (size->0x%X mem->0x%X)\n", WINDOW_SIZE, WINDOW_ADDR);
	dbox2_flash_map.map_priv_1 = (unsigned long)ioremap(WINDOW_ADDR, WINDOW_SIZE);

	if (!dbox2_flash_map.map_priv_1) {
		printk("Failed to ioremap\n");
		return -EIO;
	}

	// Probe for dual Intel 28F320 or dual AMD
	mymtd = do_map_probe("cfi_probe", &dbox2_flash_map);
	if (!mymtd) {
	    // Probe for single Intel 28F640
	    dbox2_flash_map.buswidth = 2;
	
	    mymtd = do_map_probe("cfi_probe", &dbox2_flash_map);
	}
	    
	if (mymtd) {
		mymtd->module = THIS_MODULE;

                /* Create MTD devices for each partition. */
	        add_mtd_partitions(mymtd, partition_info, NUM_PARTITIONS);
		
		return 0;
	}

	iounmap((void *)dbox2_flash_map.map_priv_1);
	return -ENXIO;
}

static void __exit cleanup_dbox2_flash(void)
{
	if (mymtd) {
		del_mtd_partitions(mymtd);
		map_destroy(mymtd);
	}
	if (dbox2_flash_map.map_priv_1) {
		iounmap((void *)dbox2_flash_map.map_priv_1);
		dbox2_flash_map.map_priv_1 = 0;
	}
}

module_init(init_dbox2_flash);
module_exit(cleanup_dbox2_flash);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kári Davíðsson <kd@flaga.is>");
MODULE_DESCRIPTION("MTD map driver for Nokia/Sagem D-Box 2 board");
