/*
 *  linux/drivers/ide/pci/adma100.c -- basic support for Pacific Digital ADMA-100 boards
 *
 *     Created 09 Apr 2002 by Mark Lord
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License.  See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/config.h> /* for CONFIG_BLK_DEV_IDEPCI */
#include <linux/types.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <asm/io.h>

#include "adma100.h"

void __init init_hwif_adma100 (ide_hwif_t *hwif)
{
	unsigned long  phy_admctl = pci_resource_start(hwif->pci_dev, 4) + 0x80 + (hwif->channel * 0x20);
	void *v_admctl;

	hwif->autodma  = 0;		// not compatible with normal IDE DMA transfers
	hwif->dma_base = 0;		// disable DMA completely
	hwif->io_ports[IDE_CONTROL_OFFSET] += 4;	// chip needs offset of 6 instead of 2
	v_admctl = ioremap_nocache(phy_admctl, 1024);	// map config regs, so we can turn on drive IRQs
	*((unsigned short *)v_admctl) &= 3;		// enable aIEN; preserve PIO mode
	iounmap(v_admctl);				// all done; unmap config regs
	printk("ADMA100: initialized %s for basic PIO-only operation\n", hwif->name);
}

extern void ide_setup_pci_device(struct pci_dev *, ide_pci_device_t *);

static int __devinit adma100_init_one (struct pci_dev *dev, const struct pci_device_id *id)
{
	ide_pci_device_t *d = &adma100_chipsets[id->driver_data];

	if (dev->device != d->device)
		BUG();
	ide_setup_pci_device(dev, d);
	MOD_INC_USE_COUNT;
	return 0;
}

static struct pci_device_id adma100_pci_tbl[] __devinitdata = {
	{ PCI_VENDOR_ID_PDC, PCI_DEVICE_ID_PDC_ADMA100, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{ 0, },
};

static struct pci_driver driver = {
	.name		= "ADMA100 IDE",
	.id_table	= adma100_pci_tbl,
	.probe		= adma100_init_one,
};

static int adma100_ide_init (void)
{
	return ide_pci_register_driver(&driver);
}

static void adma100_ide_exit (void)
{
	ide_pci_unregister_driver(&driver);
}

module_init(adma100_ide_init);
module_exit(adma100_ide_exit);

MODULE_AUTHOR("Mark Lord");
MODULE_DESCRIPTION("Basic PIO support for ADMA100 IDE");
MODULE_LICENSE("GPL");

EXPORT_NO_SYMBOLS;

