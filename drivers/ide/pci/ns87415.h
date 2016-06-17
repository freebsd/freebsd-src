#ifndef NS87415_H
#define NS87415_H

#include <linux/config.h>
#include <linux/pci.h>
#include <linux/ide.h>

static void init_hwif_ns87415(ide_hwif_t *);
static void init_dma_ns87415(ide_hwif_t *, unsigned long);

static ide_pci_device_t ns87415_chipsets[] __devinitdata = {
	{	/* 0 */
		.vendor		= PCI_VENDOR_ID_NS,
		.device		= PCI_DEVICE_ID_NS_87415,
		.name		= "NS87415",
		.init_chipset	= NULL,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_ns87415,
                .init_dma	= init_dma_ns87415,
		.channels	= 2,
		.autodma	= AUTODMA,
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

#endif /* NS87415_H */
