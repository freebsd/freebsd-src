/*
 * Flash on Cirrus CDB89712
 *
 * $Id: cdb89712.c,v 1.3 2001/10/02 15:14:43 rmk Exp $
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <asm/io.h>
#include <asm/arch/hardware.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>



__u8 cdb89712_read8(struct map_info *map, unsigned long ofs)
{
	return __raw_readb(map->map_priv_1 + ofs);
}

__u16 cdb89712_read16(struct map_info *map, unsigned long ofs)
{
	return __raw_readw(map->map_priv_1 + ofs);
}

__u32 cdb89712_read32(struct map_info *map, unsigned long ofs)
{
	return __raw_readl(map->map_priv_1 + ofs);
}

void cdb89712_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	__raw_writeb(d, map->map_priv_1 + adr);
	mb();
}

void cdb89712_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	__raw_writew(d, map->map_priv_1 + adr);
	mb();
}

void cdb89712_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	__raw_writel(d, map->map_priv_1 + adr);
	mb();
}

void cdb89712_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	// printk ("cdb89712_copy_from: 0x%x@0x%x -> 0x%x\n", len, from, to);
	memcpy_fromio(to, map->map_priv_1 + from, len);
}

void cdb89712_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	while(len) {
		__raw_writeb(*(unsigned char *) from, map->map_priv_1 + to);
		from++;
		to++;
		len--;
	}
}


static struct mtd_info *flash_mtd;

struct map_info cdb89712_flash_map = {
	name: "flash",
	size: FLASH_SIZE,
	buswidth: FLASH_WIDTH,
	read8: cdb89712_read8,
	read16: cdb89712_read16,
	read32: cdb89712_read32,
	copy_from: cdb89712_copy_from,
	write8: cdb89712_write8,
	write16: cdb89712_write16,
	write32: cdb89712_write32,
	copy_to: cdb89712_copy_to
};

struct resource cdb89712_flash_resource = {
	name:   "Flash",
	start:  FLASH_START,
	end:    FLASH_START + FLASH_SIZE - 1,
	flags:  IORESOURCE_IO | IORESOURCE_BUSY,
};

static int __init init_cdb89712_flash (void)
{
	int err;
	
	if (request_resource (&ioport_resource, &cdb89712_flash_resource)) {
		printk(KERN_NOTICE "Failed to reserve Cdb89712 FLASH space\n");
		err = -EBUSY;
		goto out;
	}
	
	cdb89712_flash_map.map_priv_1 = (unsigned long)ioremap(FLASH_START, FLASH_SIZE);
	if (!cdb89712_flash_map.map_priv_1) {
		printk(KERN_NOTICE "Failed to ioremap Cdb89712 FLASH space\n");
		err = -EIO;
		goto out_resource;
	}

	flash_mtd = do_map_probe("cfi_probe", &cdb89712_flash_map);
	if (!flash_mtd) {
		flash_mtd = do_map_probe("map_rom", &cdb89712_flash_map);
		if (flash_mtd)
			flash_mtd->erasesize = 0x10000;
	}
	if (!flash_mtd) {
		printk("FLASH probe failed\n");
		err = -ENXIO;
		goto out_ioremap;
	}

	flash_mtd->module = THIS_MODULE;
	
	if (add_mtd_device(flash_mtd)) {
		printk("FLASH device addition failed\n");
		err = -ENOMEM;
		goto out_probe;
	}
		
	return 0;

out_probe:
	map_destroy(flash_mtd);
	flash_mtd = 0;
out_ioremap:
	iounmap((void *)cdb89712_flash_map.map_priv_1);
out_resource:
	release_resource (&cdb89712_flash_resource);
out:
	return err;
}





static struct mtd_info *sram_mtd;

struct map_info cdb89712_sram_map = {
	name: "SRAM",
	size: SRAM_SIZE,
	buswidth: SRAM_WIDTH,
	read8: cdb89712_read8,
	read16: cdb89712_read16,
	read32: cdb89712_read32,
	copy_from: cdb89712_copy_from,
	write8: cdb89712_write8,
	write16: cdb89712_write16,
	write32: cdb89712_write32,
	copy_to: cdb89712_copy_to
};

struct resource cdb89712_sram_resource = {
	name:   "SRAM",
	start:  SRAM_START,
	end:    SRAM_START + SRAM_SIZE - 1,
	flags:  IORESOURCE_IO | IORESOURCE_BUSY,
};

static int __init init_cdb89712_sram (void)
{
	int err;
	
	if (request_resource (&ioport_resource, &cdb89712_sram_resource)) {
		printk(KERN_NOTICE "Failed to reserve Cdb89712 SRAM space\n");
		err = -EBUSY;
		goto out;
	}
	
	cdb89712_sram_map.map_priv_1 = (unsigned long)ioremap(SRAM_START, SRAM_SIZE);
	if (!cdb89712_sram_map.map_priv_1) {
		printk(KERN_NOTICE "Failed to ioremap Cdb89712 SRAM space\n");
		err = -EIO;
		goto out_resource;
	}

	sram_mtd = do_map_probe("map_ram", &cdb89712_sram_map);
	if (!sram_mtd) {
		printk("SRAM probe failed\n");
		err = -ENXIO;
		goto out_ioremap;
	}

	sram_mtd->module = THIS_MODULE;
	sram_mtd->erasesize = 16;
	
	if (add_mtd_device(sram_mtd)) {
		printk("SRAM device addition failed\n");
		err = -ENOMEM;
		goto out_probe;
	}
		
	return 0;

out_probe:
	map_destroy(sram_mtd);
	sram_mtd = 0;
out_ioremap:
	iounmap((void *)cdb89712_sram_map.map_priv_1);
out_resource:
	release_resource (&cdb89712_sram_resource);
out:
	return err;
}







static struct mtd_info *bootrom_mtd;

struct map_info cdb89712_bootrom_map = {
	name: "BootROM",
	size: BOOTROM_SIZE,
	buswidth: BOOTROM_WIDTH,
	read8: cdb89712_read8,
	read16: cdb89712_read16,
	read32: cdb89712_read32,
	copy_from: cdb89712_copy_from,
};

struct resource cdb89712_bootrom_resource = {
	name:   "BootROM",
	start:  BOOTROM_START,
	end:    BOOTROM_START + BOOTROM_SIZE - 1,
	flags:  IORESOURCE_IO | IORESOURCE_BUSY,
};

static int __init init_cdb89712_bootrom (void)
{
	int err;
	
	if (request_resource (&ioport_resource, &cdb89712_bootrom_resource)) {
		printk(KERN_NOTICE "Failed to reserve Cdb89712 BOOTROM space\n");
		err = -EBUSY;
		goto out;
	}
	
	cdb89712_bootrom_map.map_priv_1 = (unsigned long)ioremap(BOOTROM_START, BOOTROM_SIZE);
	if (!cdb89712_bootrom_map.map_priv_1) {
		printk(KERN_NOTICE "Failed to ioremap Cdb89712 BootROM space\n");
		err = -EIO;
		goto out_resource;
	}

	bootrom_mtd = do_map_probe("map_rom", &cdb89712_bootrom_map);
	if (!bootrom_mtd) {
		printk("BootROM probe failed\n");
		err = -ENXIO;
		goto out_ioremap;
	}

	bootrom_mtd->module = THIS_MODULE;
	bootrom_mtd->erasesize = 0x10000;
	
	if (add_mtd_device(bootrom_mtd)) {
		printk("BootROM device addition failed\n");
		err = -ENOMEM;
		goto out_probe;
	}
		
	return 0;

out_probe:
	map_destroy(bootrom_mtd);
	bootrom_mtd = 0;
out_ioremap:
	iounmap((void *)cdb89712_bootrom_map.map_priv_1);
out_resource:
	release_resource (&cdb89712_bootrom_resource);
out:
	return err;
}





static int __init init_cdb89712_maps(void)
{

       	printk(KERN_INFO "Cirrus CDB89712 MTD mappings:\n  Flash 0x%x at 0x%x\n  SRAM 0x%x at 0x%x\n  BootROM 0x%x at 0x%x\n", 
	       FLASH_SIZE, FLASH_START, SRAM_SIZE, SRAM_START, BOOTROM_SIZE, BOOTROM_START);

	init_cdb89712_flash();
	init_cdb89712_sram();
	init_cdb89712_bootrom();
	
	return 0;
}
	

static void __exit cleanup_cdb89712_maps(void)
{
	if (sram_mtd) {
		del_mtd_device(sram_mtd);
		map_destroy(sram_mtd);
		iounmap((void *)cdb89712_sram_map.map_priv_1);
		release_resource (&cdb89712_sram_resource);
	}
	
	if (flash_mtd) {
		del_mtd_device(flash_mtd);
		map_destroy(flash_mtd);
		iounmap((void *)cdb89712_flash_map.map_priv_1);
		release_resource (&cdb89712_flash_resource);
	}

	if (bootrom_mtd) {
		del_mtd_device(bootrom_mtd);
		map_destroy(bootrom_mtd);
		iounmap((void *)cdb89712_bootrom_map.map_priv_1);
		release_resource (&cdb89712_bootrom_resource);
	}
}

module_init(init_cdb89712_maps);
module_exit(cleanup_cdb89712_maps);

MODULE_AUTHOR("Ray L");
MODULE_DESCRIPTION("ARM CDB89712 map driver");
MODULE_LICENSE("GPL");
