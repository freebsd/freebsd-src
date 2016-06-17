/*
 * amd766rom.c
 *
 * Normal mappings of chips in physical memory
 * $Id: amd766rom.c,v 1.1 2002/01/10 22:59:13 eric Exp $
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/config.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>


struct amd766rom_map_info {
	struct map_info map;
	struct mtd_info *mtd;
	unsigned long window_addr;
	u32 window_start, window_size;
	struct pci_dev *pdev;
};

static __u8 amd766rom_read8(struct map_info *map, unsigned long ofs)
{
	return __raw_readb(map->map_priv_1 + ofs);
}

static __u16 amd766rom_read16(struct map_info *map, unsigned long ofs)
{
	return __raw_readw(map->map_priv_1 + ofs);
}

static __u32 amd766rom_read32(struct map_info *map, unsigned long ofs)
{
	return __raw_readl(map->map_priv_1 + ofs);
}

static void amd766rom_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to, map->map_priv_1 + from, len);
}

static void amd766rom_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	__raw_writeb(d, map->map_priv_1 + adr);
	mb();
}

static void amd766rom_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	__raw_writew(d, map->map_priv_1 + adr);
	mb();
}

static void amd766rom_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	__raw_writel(d, map->map_priv_1 + adr);
	mb();
}

static void amd766rom_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy_toio(map->map_priv_1 + to, from, len);
}

static struct amd766rom_map_info amd766rom_map = {
	map: {
		name: "AMD766 rom",
		size: 0,
		buswidth: 1,
		read8: amd766rom_read8,
		read16: amd766rom_read16,
		read32: amd766rom_read32,
		copy_from: amd766rom_copy_from,
		write8: amd766rom_write8,
		write16: amd766rom_write16,
		write32: amd766rom_write32,
		copy_to: amd766rom_copy_to,
		/* The standard rom socket is for single power supply chips
		 * that don't have an extra vpp.
		 */
	},
	mtd: 0,
	window_addr: 0,
};

static int __devinit amd766rom_init_one (struct pci_dev *pdev,
	const struct pci_device_id *ent)
{
	struct rom_window {
		u32 start;
		u32 size;
		u8 segen_bits;
	};
	static struct rom_window rom_window[] = {
		{ 0xffb00000, 5*1024*1024, (1<<7) | (1<<6), },
		{ 0xffc00000, 4*1024*1024, (1<<7), },
		{ 0xffff0000, 64*1024,     0 },
		{ 0         , 0,           0 },
	};
	static const u32 rom_probe_sizes[] = { 
		5*1024*1024, 4*1024*1024, 2*1024*1024, 1024*1024, 512*1024, 
		256*1024, 128*1024, 64*1024, 0};
	static char *rom_probe_types[] = { "cfi_probe", "jedec_probe", 0 };
	u8 byte;
	struct amd766rom_map_info *info = &amd766rom_map;
	struct rom_window *window;
	int i;
	u32 rom_size;

	window = &rom_window[0];
	while(window->size) {
		if (request_mem_region(window->start, window->size, "amd766rom")) {
			break;
		}
		window++;
	}
	if (!window->size) {
		printk(KERN_ERR "amd766rom: cannot reserve rom window");
		goto err_out_none;
	}

	/* Enable the selected rom window */
	pci_read_config_byte(pdev, 0x43, &byte);
	pci_write_config_byte(pdev, 0x43, byte | window->segen_bits);

	/* Enable writes through the rom window */
	pci_read_config_byte(pdev, 0x40, &byte);
	pci_write_config_byte(pdev, 0x40, byte | 1);

	/* FIXME handle registers 0x80 - 0x8C the bios region locks */

	printk(KERN_NOTICE "amd766rom window : %x at %x\n", 
		window->size, window->start);
	/* For write accesses caches are useless */
	info->window_addr = (unsigned long)ioremap_nocache(window->start, window->size);

	if (!info->window_addr) {
		printk(KERN_ERR "Failed to ioremap\n");
		goto err_out_free_mmio_region;
	}
	info->mtd = 0;
	for(i = 0; (rom_size = rom_probe_sizes[i]); i++) {
		char **chip_type;
		if (rom_size > window->size) {
			continue;
		}
		info->map.map_priv_1 = 
			info->window_addr + window->size - rom_size;
		info->map.size = rom_size;
		chip_type = rom_probe_types;
		for(; !info->mtd && *chip_type; chip_type++) {
			info->mtd = do_map_probe(*chip_type, &amd766rom_map.map);
		}
		if (info->mtd) {
			break;
		}
	}
	if (!info->mtd) {
		goto err_out_iounmap;
	}
	printk(KERN_NOTICE "amd766rom chip at offset: %x\n",
		window->size - rom_size);
		
	info->mtd->module = THIS_MODULE;
	add_mtd_device(info->mtd);
	info->window_start = window->start;
	info->window_size = window->size;
	return 0;

err_out_iounmap:
	iounmap((void *)(info->window_addr));
err_out_free_mmio_region:
	release_mem_region(window->start, window->size);
err_out_none:
	return -ENODEV;
}


static void __devexit amd766rom_remove_one (struct pci_dev *pdev)
{
	struct amd766rom_map_info *info = &amd766rom_map;
	u8 byte;

	del_mtd_device(info->mtd);
	map_destroy(info->mtd);
	info->mtd = 0;
	info->map.map_priv_1 = 0;

	iounmap((void *)(info->window_addr));
	info->window_addr = 0;

	/* Disable writes through the rom window */
	pci_read_config_byte(pdev, 0x40, &byte);
	pci_write_config_byte(pdev, 0x40, byte & ~1);
	
	release_mem_region(info->window_start, info->window_size);
}

static struct pci_device_id amd766rom_pci_tbl[] __devinitdata = {
	{ PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_VIPER_7410, 
	  PCI_ANY_ID, PCI_ANY_ID, },
};

MODULE_DEVICE_TABLE(pci, amd766rom_pci_tbl);

#if 0
static struct pci_driver amd766rom_driver = {
	name:	  "amd766rom",
	id_table: amd766rom_pci_tbl,
	probe:    amd766rom_init_one,
	remove:   amd766rom_remove_one,
};
#endif

int __init init_amd766rom(void)
{
	struct pci_dev *pdev;
	pdev = pci_find_device(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_VIPER_7410, 0);
	if (pdev) {
		amd766rom_map.pdev = pdev;
		return amd766rom_init_one(pdev, &amd766rom_pci_tbl[0]);
	}
	return -ENXIO;
#if 0
	return pci_module_init(&amd766rom_driver);
#endif
}

static void __exit cleanup_amd766rom(void)
{
	amd766rom_remove_one(amd766rom_map.pdev);
}

module_init(init_amd766rom);
module_exit(cleanup_amd766rom);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eric Biederman <ebiederman@lnxi.com>");
MODULE_DESCRIPTION("MTD map driver for BIOS chips on the AMD766 southbridge");

