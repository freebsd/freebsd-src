#ifndef ADMA_100_H
#define ADMA_100_H

#include <linux/config.h>
#include <linux/pci.h>
#include <linux/ide.h>

extern void init_hwif_adma100(ide_hwif_t *);

static ide_pci_device_t adma100_chipsets[] __devinitdata = {
	{
		.vendor		= PCI_VENDOR_ID_PDC,
		.device		= PCI_DEVICE_ID_PDC_ADMA100,
		.name		= "ADMA100",
		.init_setup	= NULL,
		.init_chipset	= NULL,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_adma100,
		.init_dma	= NULL,
		.channels	= 2,
		.autodma	= NODMA,
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
		.bootable	= OFF_BOARD,
		.extra		= 0
	},{
		.vendor		= 0,
		.device		= 0,
		.channels	= 0,
		.bootable	= EOL,
	}
};

#endif /* ADMA_100_H */
