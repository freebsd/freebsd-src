#ifndef IDE_GENERIC_H
#define IDE_GENERIC_H

#include <linux/config.h>
#include <linux/pci.h>
#include <linux/ide.h>

static unsigned int init_chipset_generic(struct pci_dev *, const char *);
static void init_hwif_generic(ide_hwif_t *);
static void init_dma_generic(ide_hwif_t *, unsigned long);

static ide_pci_device_t generic_chipsets[] __devinitdata = {
	{	/* 0 */
		.vendor		= PCI_VENDOR_ID_NS,
		.device		= PCI_DEVICE_ID_NS_87410,
		.name		= "NS87410",
		.init_chipset	= init_chipset_generic,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_generic,
		.init_dma	= init_dma_generic,
		.channels	= 2,
		.autodma	= AUTODMA,
		.enablebits	= {{0x43,0x08,0x08}, {0x47,0x08,0x08}},
		.bootable	= ON_BOARD,
		.extra		= 0,
        },{	/* 1 */
		.vendor		= PCI_VENDOR_ID_PCTECH,
		.device		= PCI_DEVICE_ID_PCTECH_SAMURAI_IDE,
		.name		= "SAMURAI",
		.init_chipset	= init_chipset_generic,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_generic,
		.init_dma	= init_dma_generic,
		.channels	= 2,
		.autodma	= AUTODMA,
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
		.bootable	= ON_BOARD,
		.extra		= 0,
	},{	/* 2 */
		.vendor		= PCI_VENDOR_ID_HOLTEK,
		.device		= PCI_DEVICE_ID_HOLTEK_6565,
		.name		= "HT6565",
		.init_chipset	= init_chipset_generic,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_generic,
		.init_dma	= init_dma_generic,
		.channels	= 2,
		.autodma	= AUTODMA,
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
		.bootable	= ON_BOARD,
		.extra		= 0,
	},{	/* 3 */
		.vendor		= PCI_VENDOR_ID_UMC,
		.device		= PCI_DEVICE_ID_UMC_UM8673F,
		.name		= "UM8673F",
		.init_chipset	= init_chipset_generic,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_generic,
		.init_dma	= init_dma_generic,
		.channels	= 2,
		.autodma	= NODMA,
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
		.bootable	= ON_BOARD,
		.extra		= 0,
	},{	/* 4 */
		.vendor		= PCI_VENDOR_ID_UMC,
		.device		= PCI_DEVICE_ID_UMC_UM8886A,
		.name		= "UM8886A",
		.init_chipset	= init_chipset_generic,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_generic,
		.init_dma	= init_dma_generic,
		.channels	= 2,
		.autodma	= NODMA,
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
		.bootable	= ON_BOARD,
		.extra		= 0,
	},{	/* 5 */
		.vendor		= PCI_VENDOR_ID_UMC,
		.device		= PCI_DEVICE_ID_UMC_UM8886BF,
		.name		= "UM8886BF",
		.init_chipset	= init_chipset_generic,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_generic,
		.init_dma	= init_dma_generic,
		.channels	= 2,
		.autodma	= NODMA,
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
		.bootable	= ON_BOARD,
		.extra		= 0,
	},{	/* 6 */
		.vendor		= PCI_VENDOR_ID_HINT,
		.device		= PCI_DEVICE_ID_HINT_VXPROII_IDE,
		.name		= "HINT_IDE",
		.init_chipset	= init_chipset_generic,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_generic,
		.init_dma	= init_dma_generic,
		.channels	= 2,
		.autodma	= AUTODMA,
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
		.bootable	= ON_BOARD,
		.extra		= 0,
	},{	/* 7 */
		.vendor		= PCI_VENDOR_ID_VIA,
		.device		= PCI_DEVICE_ID_VIA_82C561,
		.name		= "VIA_IDE",
		.init_chipset	= init_chipset_generic,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_generic,
		.init_dma	= init_dma_generic,
		.channels	= 2,
		.autodma	= NOAUTODMA,
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
		.bootable	= ON_BOARD,
		.extra		= 0,
	},{	/* 8 */
		.vendor		= PCI_VENDOR_ID_OPTI,
		.device		= PCI_DEVICE_ID_OPTI_82C558,
		.name		= "OPTI621V",
		.init_chipset	= init_chipset_generic,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_generic,
		.init_dma	= init_dma_generic,
		.channels	= 2,
		.autodma	= NOAUTODMA,
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
		.bootable	= ON_BOARD,
		.extra		= 0,
	},{	/* 9 */
		.vendor		= PCI_VENDOR_ID_TOSHIBA,
		.device		= PCI_DEVICE_ID_TOSHIBA_PICCOLO,
		.name		= "Piccolo",
		.init_chipset	= init_chipset_generic,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_generic,
		.init_dma	= init_dma_generic,
		.channels	= 2,
		.autodma	= NOAUTODMA,
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

#if 0
static ide_pci_device_t unknown_chipset[] __devinitdata = {
	{	/* 0 */
		.vendor		= 0,
		.device		= 0,
		.name		= "PCI_IDE",
		.init_chipset	= init_chipset_generic,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_generic,
		.init_dma	= init_dma_generic,
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
#endif

#endif /* IDE_GENERIC_H */
