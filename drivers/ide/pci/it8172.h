#ifndef ITE8172G_H
#define ITE8172G_H

#include <linux/config.h>
#include <linux/pci.h>
#include <linux/ide.h>

static u8 it8172_ratemask(ide_drive_t *drive);
static u8 it8172_ratefilter(ide_drive_t *drive, u8 speed);
static void it8172_tune_drive(ide_drive_t *drive, u8 pio);
static u8 it8172_dma_2_pio(u8 xfer_rate);
static int it8172_tune_chipset(ide_drive_t *drive, u8 xferspeed);
#ifdef CONFIG_BLK_DEV_IDEDMA
static int it8172_config_chipset_for_dma(ide_drive_t *drive);
#endif

static void init_setup_it8172(struct pci_dev *, ide_pci_device_t *);
static unsigned int init_chipset_it8172(struct pci_dev *, const char *);
static void init_hwif_it8172(ide_hwif_t *);
static void init_dma_it8172(ide_hwif_t *, unsigned long);

static ide_pci_device_t it8172_chipsets[] __devinitdata = {
	{	/* 0 */
		.vendor		= PCI_VENDOR_ID_ITE,
		.device		= PCI_DEVICE_ID_ITE_IT8172G,
		.name		= "IT8172G",
		.init_setup	= init_setup_it8172,
		.init_chipset	= init_chipset_it8172,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_it8172,
                .init_dma	= init_dma_it8172,
		.channels	= 2,
		.autodma	= AUTODMA,
		.enablebits	= {{0x00,0x00,0x00}, {0x40,0x00,0x01}},
		.bootable	= ON_BOARD,
		.extra		= 0,
	},{
		.vendor		= 0,
		.device		= 0,
		.channels	= 0,
		.bootable	= EOL,
	}
};

#endif /* ITE8172G_H */
