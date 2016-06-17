/*
 *  linux/drivers/ide/pci/rz1000.c	Version 0.06	January 12, 2003
 *
 *  Copyright (C) 1995-1998  Linus Torvalds & author (see below)
 */

/*
 *  Principal Author:  mlord@pobox.com (Mark Lord)
 *
 *  See linux/MAINTAINERS for address of current maintainer.
 *
 *  This file provides support for disabling the buggy read-ahead
 *  mode of the RZ1000 IDE chipset, commonly used on Intel motherboards.
 *
 *  Dunno if this fixes both ports, or only the primary port (?).
 */

#undef REALLY_SLOW_IO		/* most systems can safely undef this */

#include <linux/config.h> /* for CONFIG_BLK_DEV_IDEPCI */
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>
#include <linux/pci.h>
#include <linux/ide.h>
#include <linux/init.h>

#include <asm/io.h>

#include "rz1000.h"

static void __init init_hwif_rz1000 (ide_hwif_t *hwif)
{
	u16 reg;
	struct pci_dev *dev = hwif->pci_dev;

	hwif->chipset = ide_rz1000;
	if (!pci_read_config_word (dev, 0x40, &reg) &&
	    !pci_write_config_word(dev, 0x40, reg & 0xdfff)) {
		printk(KERN_INFO "%s: disabled chipset read-ahead "
			"(buggy RZ1000/RZ1001)\n", hwif->name);
	} else {
		hwif->serialized = 1;
		hwif->drives[0].no_unmask = 1;
		hwif->drives[1].no_unmask = 1;
		printk(KERN_INFO "%s: serialized, disabled unmasking "
			"(buggy RZ1000/RZ1001)\n", hwif->name);
	}
}

extern void ide_setup_pci_device(struct pci_dev *, ide_pci_device_t *);

static int __devinit rz1000_init_one(struct pci_dev *dev, const struct pci_device_id *id)
{
	ide_pci_device_t *d = &rz1000_chipsets[id->driver_data];
	if (dev->device != d->device)
		BUG();
	ide_setup_pci_device(dev, d);
	MOD_INC_USE_COUNT;
	return 0;
}

static struct pci_device_id rz1000_pci_tbl[] __devinitdata = {
	{ PCI_VENDOR_ID_PCTECH, PCI_DEVICE_ID_PCTECH_RZ1000, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{ PCI_VENDOR_ID_PCTECH, PCI_DEVICE_ID_PCTECH_RZ1001, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 1},
	{ 0, },
};

static struct pci_driver driver = {
	.name		= "RZ1000 IDE",
	.id_table	= rz1000_pci_tbl,
	.probe		= rz1000_init_one,
};

static int rz1000_ide_init(void)
{
	return ide_pci_register_driver(&driver);
}

static void rz1000_ide_exit(void)
{
	ide_pci_unregister_driver(&driver);
}

module_init(rz1000_ide_init);
module_exit(rz1000_ide_exit);

MODULE_AUTHOR("Andre Hedrick");
MODULE_DESCRIPTION("PCI driver module for RZ1000 IDE");
MODULE_LICENSE("GPL");

EXPORT_NO_SYMBOLS;

