#ifndef RZ100X_H
#define RZ100X_H

#include <linux/config.h>
#include <linux/pci.h>
#include <linux/ide.h>

static void init_hwif_rz1000(ide_hwif_t *);

static ide_pci_device_t rz1000_chipsets[] __devinitdata = {
{
		.vendor		= PCI_VENDOR_ID_PCTECH,
		.device		= PCI_DEVICE_ID_PCTECH_RZ1000,
		.name		= "RZ1000",
		.init_chipset	= NULL,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_rz1000,
		.init_dma	= NULL,
		.channels	= 2,
		.autodma	= NODMA,
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
		.bootable	= ON_BOARD,
		.extra		= 0,
	},{
		.vendor		= PCI_VENDOR_ID_PCTECH,
		.device		= PCI_DEVICE_ID_PCTECH_RZ1001,
		.name		= "RZ1001",
		.init_chipset	= NULL,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_rz1000,
		.init_dma	= NULL,
		.channels	= 2,
		.autodma	= NODMA,
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
		.bootable	= ON_BOARD,
		.extra		= 0,
	},{
		.vendor		= 0,
		.device		= 0,
		.channels	= 0,
		.bootable	= EOL,
	}
};

#endif /* RZ100X_H */
