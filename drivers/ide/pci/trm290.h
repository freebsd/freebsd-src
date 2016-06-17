#ifndef TRM290_H
#define TRM290_H

#include <linux/config.h>
#include <linux/pci.h>
#include <linux/ide.h>

extern void init_hwif_trm290(ide_hwif_t *);

static ide_pci_device_t trm290_chipsets[] __devinitdata = {
	{	/* 0 */
		vendor:		PCI_VENDOR_ID_TEKRAM,
		device:		PCI_DEVICE_ID_TEKRAM_DC290,
		name:		"TRM290",
		init_chipset:	NULL,
		init_iops:	NULL,
		init_hwif:	init_hwif_trm290,
		init_dma:	NULL,
		channels:	2,
		autodma:	NOAUTODMA,
		enablebits:	{{0x00,0x00,0x00}, {0x00,0x00,0x00}},
		bootable:	ON_BOARD,
		extra:		0,
	},{
		vendor:		0,
		device:		0,
		channels:	0,
		bootable:	EOL,
	}
};

#endif /* TRM290_H */
