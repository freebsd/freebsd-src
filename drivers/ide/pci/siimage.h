#ifndef SIIMAGE_H
#define SIIMAGE_H

#include <linux/config.h>
#include <linux/pci.h>
#include <linux/ide.h>

#include <asm/io.h>

#define DISPLAY_SIIMAGE_TIMINGS

#undef SIIMAGE_VIRTUAL_DMAPIO
#undef SIIMAGE_BUFFERED_TASKFILE
#undef SIIMAGE_LARGE_DMA

#define SII_DEBUG 0

#if SII_DEBUG
#define siiprintk(x...)	printk(x)
#else
#define siiprintk(x...)
#endif


#if defined(DISPLAY_SIIMAGE_TIMINGS) && defined(CONFIG_PROC_FS)
#include <linux/stat.h>
#include <linux/proc_fs.h>

static char * print_siimage_get_info(char *, struct pci_dev *, int);
static int siimage_get_info(char *, char **, off_t, int);

static u8 siimage_proc;

static ide_pci_host_proc_t siimage_procs[] __initdata = {
	{
		.name		= "siimage",
		.set		= 1,
		.get_info	= siimage_get_info,
		.parent		= NULL,
	},
};
#endif /* DISPLAY_SIIMAGE_TIMINGS && CONFIG_PROC_FS */	

static unsigned int init_chipset_siimage(struct pci_dev *, const char *);
static void init_iops_siimage(ide_hwif_t *);
static void init_hwif_siimage(ide_hwif_t *);
static void init_dma_siimage(ide_hwif_t *, unsigned long);

static ide_pci_device_t siimage_chipsets[] __devinitdata = {
	{	/* 0 */
		.vendor		= PCI_VENDOR_ID_CMD,
		.device		= PCI_DEVICE_ID_SII_680,
		.name		= "SiI680",
		.init_chipset	= init_chipset_siimage,
		.init_iops	= init_iops_siimage,
		.init_hwif	= init_hwif_siimage,
		.init_dma	= init_dma_siimage,
		.channels	= 2,
		.autodma	= AUTODMA,
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
		.bootable	= ON_BOARD,
		.extra		= 0,
	},{	/* 1 */
		.vendor		= PCI_VENDOR_ID_CMD,
		.device		= PCI_DEVICE_ID_SII_3112,
		.name		= "SiI3112 Serial ATA",
		.init_chipset	= init_chipset_siimage,
		.init_iops	= init_iops_siimage,
		.init_hwif	= init_hwif_siimage,
		.init_dma	= init_dma_siimage,
		.channels	= 2,
		.autodma	= AUTODMA,
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
		.bootable	= ON_BOARD,
		.extra		= 0,
	},{	/* 2 */
		.vendor		= PCI_VENDOR_ID_CMD,
		.device		= PCI_DEVICE_ID_SII_1210SA,
		.name		= "Adaptec AAR-1210SA",
		.init_chipset	= init_chipset_siimage,
		.init_iops	= init_iops_siimage,
		.init_hwif	= init_hwif_siimage,
		.init_dma	= init_dma_siimage,
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

#endif /* SIIMAGE_H */
