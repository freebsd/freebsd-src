#ifndef CMD640_H
#define CMD640_H

#include <linux/config.h>
#include <linux/pci.h>
#include <linux/ide.h>

#define IDE_IGNORE      ((void *)-1)

static ide_pci_device_t cmd640_chipsets[] __initdata = {
	{
		.vendor		= PCI_VENDOR_ID_CMD,
		.device		= PCI_DEVICE_ID_CMD_640,
		.name		= "CMD640",
		.init_setup	= NULL,
		.init_chipset	= NULL,
		.init_iops	= NULL,
		.init_hwif	= IDE_IGNORE,
		.init_dma	= NULL,
		.channels	= 2,
		.autodma	= NODMA,
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
		.bootable	= ON_BOARD,
		.extra		= 0
	},{
		.vendor		= 0,
		.device		= 0,
		.bootable	= EOL,
	}
}

#endif /* CMD640_H */
