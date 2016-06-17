/*
 * linux/drivers/ide/ide-pnp.c
 *
 * This file provides autodetection for ISA PnP IDE interfaces.
 * It was tested with "ESS ES1868 Plug and Play AudioDrive" IDE interface.
 *
 * Copyright (C) 2000 Andrey Panin <pazke@orbita.don.sitek.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example /usr/src/linux/COPYING); if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  
 */

#include <linux/module.h>
#include <linux/ide.h>
#include <linux/init.h>

#include <linux/isapnp.h>

#define DEV_IO(dev, index) (dev->resource[index].start)
#define DEV_IRQ(dev, index) (dev->irq_resource[index].start)

#define DEV_NAME(dev) (dev->bus->name ? dev->bus->name : "ISA PnP")

#define GENERIC_HD_DATA		0
#define GENERIC_HD_ERROR	1
#define GENERIC_HD_NSECTOR	2
#define GENERIC_HD_SECTOR	3
#define GENERIC_HD_LCYL		4
#define GENERIC_HD_HCYL		5
#define GENERIC_HD_SELECT	6
#define GENERIC_HD_STATUS	7

static int generic_ide_offsets[IDE_NR_PORTS] __initdata = {
	GENERIC_HD_DATA, GENERIC_HD_ERROR, GENERIC_HD_NSECTOR, 
	GENERIC_HD_SECTOR, GENERIC_HD_LCYL, GENERIC_HD_HCYL,
	GENERIC_HD_SELECT, GENERIC_HD_STATUS, -1, -1
};

/* ISA PnP device table entry */
struct pnp_dev_t {
	unsigned short card_vendor, card_device, vendor, device;
	int (*init_fn)(struct pci_dev *dev, int enable);
};

/* Generic initialisation function for ISA PnP IDE interface */

static int __init pnpide_generic_init(struct pci_dev *dev, int enable)
{
	hw_regs_t hw;
	int index;

	if (!enable)
		return 0;

	if (!(DEV_IO(dev, 0) && DEV_IO(dev, 1) && DEV_IRQ(dev, 0)))
		return 1;

	ide_setup_ports(&hw, (ide_ioreg_t) DEV_IO(dev, 0),
			generic_ide_offsets,
			(ide_ioreg_t) DEV_IO(dev, 1),
			0, NULL,
//			generic_pnp_ide_iops,
			DEV_IRQ(dev, 0));

	index = ide_register_hw(&hw, NULL);

	if (index != -1) {
	    	printk(KERN_INFO "ide%d: %s IDE interface\n", index, DEV_NAME(dev));
		return 0;
	}

	return 1;
}

/* Add your devices here :)) */
struct pnp_dev_t idepnp_devices[] __initdata = {
  	/* Generic ESDI/IDE/ATA compatible hard disk controller */
	{	ISAPNP_ANY_ID, ISAPNP_ANY_ID,
		ISAPNP_VENDOR('P', 'N', 'P'), ISAPNP_DEVICE(0x0600),
		pnpide_generic_init },
	{	0 }
};

#define NR_PNP_DEVICES 8
struct pnp_dev_inst {
	struct pci_dev *dev;
	struct pnp_dev_t *dev_type;
};
static struct pnp_dev_inst devices[NR_PNP_DEVICES];
static int pnp_ide_dev_idx = 0;

/*
 * Probe for ISA PnP IDE interfaces.
 */

static void pnpide_init(int enable)
{
	struct pci_dev *dev = NULL;
	struct pnp_dev_t *dev_type;

	if (!isapnp_present())
		return;

	/* Module unload, deactivate all registered devices. */
	if (!enable) {
		int i;
		for (i = 0; i < pnp_ide_dev_idx; i++) {
			dev = devices[i].dev;
			devices[i].dev_type->init_fn(dev, 0);
			if (dev->deactivate)
				dev->deactivate(dev);
		}
		return;
	}

	for (dev_type = idepnp_devices; dev_type->vendor; dev_type++) {
		while ((dev = isapnp_find_dev(NULL, dev_type->vendor,
			dev_type->device, dev))) {

			if (dev->active)
				continue;

       			if (dev->prepare && dev->prepare(dev) < 0) {
				printk(KERN_ERR"ide-pnp: %s prepare failed\n", DEV_NAME(dev));
				continue;
			}

			if (dev->activate && dev->activate(dev) < 0) {
				printk(KERN_ERR"ide: %s activate failed\n", DEV_NAME(dev));
				continue;
			}

			/* Call device initialization function */
			if (dev_type->init_fn(dev, 1)) {
				if (dev->deactivate(dev))
					dev->deactivate(dev);
			} else {
#ifdef MODULE
				/*
				 * Register device in the array to
				 * deactivate it on a module unload.
				 */
				if (pnp_ide_dev_idx >= NR_PNP_DEVICES)
					return;
				devices[pnp_ide_dev_idx].dev = dev;
				devices[pnp_ide_dev_idx].dev_type = dev_type;
				pnp_ide_dev_idx++;
#endif
			}
		}
	}
}

static void __init pnpide_begin(void)
{
	pnpide_init(1);
}

static int pnpide_init_module(void)
{
	ide_register_driver(pnpide_begin);
	return 0;
}

static void pnpide_unload(void)
{
	pnpide_init(0);
}

module_init(pnpide_init_module);
module_exit(pnpide_unload);

MODULE_AUTHOR("Andrey Panin");
MODULE_DESCRIPTION("Enabler for ISAPNP IDE devices");
MODULE_LICENSE("GPL");
