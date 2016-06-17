/*
 *  linux/drivers/ide/pci/generic.c	Version 0.11	December 30, 2002
 *
 *  Copyright (C) 2001-2002	Andre Hedrick <andre@linux-ide.org>
 *  Portions (C) Copyright 2002  Red Hat Inc <alan@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * For the avoidance of doubt the "preferred form" of this code is one which
 * is in an open non patent encumbered format. Where cryptographic key signing
 * forms part of the process of creating an executable the information
 * including keys needed to generate an equivalently functional executable
 * are deemed to be part of the source code.
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

#include "generic.h"

static unsigned int __init init_chipset_generic (struct pci_dev *dev, const char *name)
{
	return 0;
}

static void __init init_hwif_generic (ide_hwif_t *hwif)
{
	switch(hwif->pci_dev->device) {
		case PCI_DEVICE_ID_UMC_UM8673F:
		case PCI_DEVICE_ID_UMC_UM8886A:
		case PCI_DEVICE_ID_UMC_UM8886BF:
			hwif->irq = hwif->channel ? 15 : 14;
			break;
		default:
			break;
	}

	if (!(hwif->dma_base))
		return;

	hwif->atapi_dma = 1;
	hwif->ultra_mask = 0x7f;
	hwif->mwdma_mask = 0x07;
	hwif->swdma_mask = 0x07;

	if (!noautodma)
		hwif->autodma = 1;
	hwif->drives[0].autodma = hwif->autodma;
	hwif->drives[1].autodma = hwif->autodma;
}

static void init_dma_generic (ide_hwif_t *hwif, unsigned long dmabase)
{
	ide_setup_dma(hwif, dmabase, 8);
}

extern void ide_setup_pci_device(struct pci_dev *, ide_pci_device_t *);

#if 0

	/* Logic to add back later on */
	
	if ((dev->class >> 8) == PCI_CLASS_STORAGE_IDE) {
		ide_pci_device_t *unknown = unknown_chipset;
//		unknown->vendor = dev->vendor;
//		unknown->device = dev->device;
		init_setup_unknown(dev, unknown);
		return 1;
	}
	return 0;
#endif	

/**
 *	generic_init_one	-	called when a PIIX is found
 *	@dev: the generic device
 *	@id: the matching pci id
 *
 *	Called when the PCI registration layer (or the IDE initialization)
 *	finds a device matching our IDE device tables.
 */
 
static int __devinit generic_init_one(struct pci_dev *dev, const struct pci_device_id *id)
{
	ide_pci_device_t *d = &generic_chipsets[id->driver_data];
	u16 command;

	if (dev->device != d->device)
		BUG();
	if ((d->vendor == PCI_VENDOR_ID_UMC) &&
	    (d->device == PCI_DEVICE_ID_UMC_UM8886A) &&
	    (!(PCI_FUNC(dev->devfn) & 1)))
		return 1; /* UM8886A/BF pair */

	if ((d->vendor == PCI_VENDOR_ID_OPTI) &&
	    (d->device == PCI_DEVICE_ID_OPTI_82C558) &&
	    (!(PCI_FUNC(dev->devfn) & 1)))
		return 1;

	pci_read_config_word(dev, PCI_COMMAND, &command);
	if(!(command & PCI_COMMAND_IO))
	{
		printk(KERN_INFO "Skipping disabled %s IDE controller.\n", d->name);
		return 1; 
	}
	ide_setup_pci_device(dev, d);
	MOD_INC_USE_COUNT;
	return 0;
}

static struct pci_device_id generic_pci_tbl[] __devinitdata = {
	{ PCI_VENDOR_ID_NS,     PCI_DEVICE_ID_NS_87410,            PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{ PCI_VENDOR_ID_PCTECH, PCI_DEVICE_ID_PCTECH_SAMURAI_IDE,  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 1},
	{ PCI_VENDOR_ID_HOLTEK, PCI_DEVICE_ID_HOLTEK_6565,         PCI_ANY_ID, PCI_ANY_ID, 0, 0, 2},
	{ PCI_VENDOR_ID_UMC,    PCI_DEVICE_ID_UMC_UM8673F,         PCI_ANY_ID, PCI_ANY_ID, 0, 0, 3},
	{ PCI_VENDOR_ID_UMC,    PCI_DEVICE_ID_UMC_UM8886A,         PCI_ANY_ID, PCI_ANY_ID, 0, 0, 4},
	{ PCI_VENDOR_ID_UMC,    PCI_DEVICE_ID_UMC_UM8886BF,        PCI_ANY_ID, PCI_ANY_ID, 0, 0, 5},
	{ PCI_VENDOR_ID_HINT,   PCI_DEVICE_ID_HINT_VXPROII_IDE,    PCI_ANY_ID, PCI_ANY_ID, 0, 0, 6},
	{ PCI_VENDOR_ID_VIA,    PCI_DEVICE_ID_VIA_82C561,          PCI_ANY_ID, PCI_ANY_ID, 0, 0, 7},
	{ PCI_VENDOR_ID_OPTI,   PCI_DEVICE_ID_OPTI_82C558,         PCI_ANY_ID, PCI_ANY_ID, 0, 0, 8},
	{ PCI_VENDOR_ID_TOSHIBA, PCI_DEVICE_ID_TOSHIBA_PICCOLO,	   PCI_ANY_ID, PCI_ANY_ID, 0, 0, 9},
	{ 0, },
};

static struct pci_driver driver = {
	.name		= "PCI IDE",
	.id_table	= generic_pci_tbl,
	.probe		= generic_init_one,
};

static int generic_ide_init(void)
{
	return ide_pci_register_driver(&driver);
}

static void generic_ide_exit(void)
{
	ide_pci_unregister_driver(&driver);
}

module_init(generic_ide_init);
module_exit(generic_ide_exit);

MODULE_AUTHOR("Andre Hedrick");
MODULE_DESCRIPTION("PCI driver module for generic PCI IDE");
MODULE_LICENSE("GPL");

EXPORT_NO_SYMBOLS;
