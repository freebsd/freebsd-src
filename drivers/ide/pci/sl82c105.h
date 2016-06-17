#ifndef W82C105_H
#define W82C105_H

#include <linux/config.h>
#include <linux/pci.h>
#include <linux/ide.h>

static unsigned int init_chipset_sl82c105(struct pci_dev *, const char *);
static void init_hwif_sl82c105(ide_hwif_t *);
static void init_dma_sl82c105(ide_hwif_t *, unsigned long);

static ide_pci_device_t sl82c105_chipsets[] __devinitdata = {
	{	/* 0 */
		.vendor		= PCI_VENDOR_ID_WINBOND,
		.device		= PCI_DEVICE_ID_WINBOND_82C105,
		.name		= "W82C105",
		.init_chipset	= init_chipset_sl82c105,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_sl82c105,
		.init_dma	= init_dma_sl82c105,
		.channels	= 2,
		.autodma	= NOAUTODMA,
		.enablebits	= {{0x40,0x01,0x01}, {0x40,0x10,0x10}},
		.bootable	= ON_BOARD,
		.extra		= 0,
	},{
		.vendor		= 0,
		.device		= 0,
		.channels	= 0,
		.bootable	= EOL,
	}
};

#endif /* W82C105_H */
