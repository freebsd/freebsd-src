/*
 * $Id: l440gx.c,v 1.8 2002/01/10 20:27:40 eric Exp $
 *
 * BIOS Flash chip on Intel 440GX board.
 *
 * Bugs this currently does not work under linuxBIOS.
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/config.h>

#define PIIXE_IOBASE_RESOURCE	11

#define WINDOW_ADDR 0xfff00000
#define WINDOW_SIZE 0x00100000
#define BUSWIDTH 1

static u32 iobase;
#define IOBASE iobase
#define TRIBUF_PORT (IOBASE+0x37)
#define VPP_PORT (IOBASE+0x28)

static struct mtd_info *mymtd;

__u8 l440gx_read8(struct map_info *map, unsigned long ofs)
{
	return __raw_readb(map->map_priv_1 + ofs);
}

__u16 l440gx_read16(struct map_info *map, unsigned long ofs)
{
	return __raw_readw(map->map_priv_1 + ofs);
}

__u32 l440gx_read32(struct map_info *map, unsigned long ofs)
{
	return __raw_readl(map->map_priv_1 + ofs);
}

void l440gx_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to, map->map_priv_1 + from, len);
}

void l440gx_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	__raw_writeb(d, map->map_priv_1 + adr);
	mb();
}

void l440gx_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	__raw_writew(d, map->map_priv_1 + adr);
	mb();
}

void l440gx_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	__raw_writel(d, map->map_priv_1 + adr);
	mb();
}

void l440gx_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy_toio(map->map_priv_1 + to, from, len);
}

/* Is this really the vpp port? */
void l440gx_set_vpp(struct map_info *map, int vpp)
{
	unsigned long l;

	l = inl(VPP_PORT);
	if (vpp) {
		l |= 1;
	} else {
		l &= ~1;
	}
	outl(l, VPP_PORT);
}

struct map_info l440gx_map = {
	name: "L440GX BIOS",
	size: WINDOW_SIZE,
	buswidth: BUSWIDTH,
	read8: l440gx_read8,
	read16: l440gx_read16,
	read32: l440gx_read32,
	copy_from: l440gx_copy_from,
	write8: l440gx_write8,
	write16: l440gx_write16,
	write32: l440gx_write32,
	copy_to: l440gx_copy_to,
#if 0
	/* FIXME verify that this is the 
	 * appripriate code for vpp enable/disable
	 */
	set_vpp: l440gx_set_vpp
#endif
};

static int __init init_l440gx(void)
{
	struct pci_dev *dev, *pm_dev;
	struct resource *pm_iobase;
	__u16 word;

	dev = pci_find_device(PCI_VENDOR_ID_INTEL, 
		PCI_DEVICE_ID_INTEL_82371AB_0, NULL);


	pm_dev = pci_find_device(PCI_VENDOR_ID_INTEL, 
		PCI_DEVICE_ID_INTEL_82371AB_3, NULL);

	if (!dev || !pm_dev) {
		printk(KERN_NOTICE "L440GX flash mapping: failed to find PIIX4 ISA bridge, cannot continue\n");
		return -ENODEV;
	}


	l440gx_map.map_priv_1 = (unsigned long)ioremap_nocache(WINDOW_ADDR, WINDOW_SIZE);

	if (!l440gx_map.map_priv_1) {
		printk(KERN_WARNING "Failed to ioremap L440GX flash region\n");
		return -ENOMEM;
	}

	printk(KERN_NOTICE "window_addr = 0x%08lx\n", (unsigned long)l440gx_map.map_priv_1);

	/* Setup the pm iobase resource 
	 * This code should move into some kind of generic bridge
	 * driver but for the moment I'm content with getting the
	 * allocation correct. 
	 */
	pm_iobase = &pm_dev->resource[PIIXE_IOBASE_RESOURCE];
	if (!(pm_iobase->flags & IORESOURCE_IO)) {
		pm_iobase->name = "pm iobase";
		pm_iobase->start = 0;
		pm_iobase->end = 63;
		pm_iobase->flags = IORESOURCE_IO;

		/* Put the current value in the resource */
		pci_read_config_dword(pm_dev, 0x40, &iobase);
		iobase &= ~1;
		pm_iobase->start += iobase & ~1;
		pm_iobase->end += iobase & ~1;

		/* Allocate the resource region */
		if (pci_assign_resource(pm_dev, PIIXE_IOBASE_RESOURCE) != 0) {
			printk(KERN_WARNING "Could not allocate pm iobase resource\n");
			iounmap((void *)l440gx_map.map_priv_1);
			return -ENXIO;
		}
	}
	/* Set the iobase */
	iobase = pm_iobase->start;
	pci_write_config_dword(pm_dev, 0x40, iobase | 1);
	

	/* Set XBCS# */
	pci_read_config_word(dev, 0x4e, &word);
	word |= 0x4;
        pci_write_config_word(dev, 0x4e, word);

	/* Supply write voltage to the chip */
	l440gx_set_vpp(&l440gx_map, 1);

	/* Enable the gate on the WE line */
	outb(inb(TRIBUF_PORT) & ~1, TRIBUF_PORT);
	
       	printk(KERN_NOTICE "Enabled WE line to L440GX BIOS flash chip.\n");

	mymtd = do_map_probe("jedec_probe", &l440gx_map);
	if (!mymtd) {
		printk(KERN_NOTICE "JEDEC probe on BIOS chip failed. Using ROM\n");
		mymtd = do_map_probe("map_rom", &l440gx_map);
	}
	if (mymtd) {
		mymtd->module = THIS_MODULE;

		add_mtd_device(mymtd);
		return 0;
	}

	iounmap((void *)l440gx_map.map_priv_1);
	return -ENXIO;
}

static void __exit cleanup_l440gx(void)
{
	del_mtd_device(mymtd);
	map_destroy(mymtd);
	
	iounmap((void *)l440gx_map.map_priv_1);
}

module_init(init_l440gx);
module_exit(cleanup_l440gx);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Woodhouse <dwmw2@infradead.org>");
MODULE_DESCRIPTION("MTD map driver for BIOS chips on Intel L440GX motherboards");
