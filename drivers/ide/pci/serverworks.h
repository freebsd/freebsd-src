
#ifndef SERVERWORKS_H
#define SERVERWORKS_H

#include <linux/config.h>
#include <linux/pci.h>
#include <linux/ide.h>

#undef SVWKS_DEBUG_DRIVE_INFO

#define SVWKS_CSB5_REVISION_NEW	0x92 /* min PCI_REVISION_ID for UDMA5 (A2.0) */
#define SVWKS_CSB6_REVISION	0xa0 /* min PCI_REVISION_ID for UDMA4 (A1.0) */

/* Seagate Barracuda ATA IV Family drives in UDMA mode 5
 * can overrun their FIFOs when used with the CSB5 */
const char *svwks_bad_ata100[] = {
	"ST320011A",
	"ST340016A",
	"ST360021A",
	"ST380021A",
	NULL
};

#define DISPLAY_SVWKS_TIMINGS	1

#if defined(DISPLAY_SVWKS_TIMINGS) && defined(CONFIG_PROC_FS)
#include <linux/stat.h>
#include <linux/proc_fs.h>

static u8 svwks_proc;

static int svwks_get_info(char *, char **, off_t, int);

static ide_pci_host_proc_t svwks_procs[] __initdata = {
{
		.name		= "svwks",
		.set		= 1,
		.get_info	= svwks_get_info,
		.parent		= NULL,
	},
};
#endif  /* defined(DISPLAY_SVWKS_TIMINGS) && defined(CONFIG_PROC_FS) */

static void init_setup_svwks(struct pci_dev *, ide_pci_device_t *);
static void init_setup_csb6(struct pci_dev *, ide_pci_device_t *);
static unsigned int init_chipset_svwks(struct pci_dev *, const char *);
static void init_hwif_svwks(ide_hwif_t *);
static void init_dma_svwks(ide_hwif_t *, unsigned long);

static ide_pci_device_t serverworks_chipsets[] __devinitdata = {
	{	/* 0 */
		.vendor		= PCI_VENDOR_ID_SERVERWORKS,
		.device		= PCI_DEVICE_ID_SERVERWORKS_OSB4IDE,
		.name		= "SvrWks OSB4",
		.init_setup	= init_setup_svwks,
		.init_chipset	= init_chipset_svwks,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_svwks,
		.init_dma	= NULL,
		.channels	= 2,
		.autodma	= AUTODMA,
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
		.bootable	= ON_BOARD,
		.extra		= 0,
	},{	/* 1 */
		.vendor		= PCI_VENDOR_ID_SERVERWORKS,
		.device		= PCI_DEVICE_ID_SERVERWORKS_CSB5IDE,
		.name		= "SvrWks CSB5",
		.init_setup	= init_setup_svwks,
		.init_chipset	= init_chipset_svwks,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_svwks,
		.init_dma	= init_dma_svwks,
		.channels	= 2,
		.autodma	= AUTODMA,
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
		.bootable	= ON_BOARD,
		.extra		= 0,
	},{	/* 2 */
		.vendor		= PCI_VENDOR_ID_SERVERWORKS,
		.device		= PCI_DEVICE_ID_SERVERWORKS_CSB6IDE,
		.name		= "SvrWks CSB6",
		.init_setup	= init_setup_csb6,
		.init_chipset	= init_chipset_svwks,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_svwks,
		.init_dma	= init_dma_svwks,
		.channels	= 2,
		.autodma	= AUTODMA,
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
		.bootable	= ON_BOARD,
		.extra		= 0,
	},{	/* 3 */
		.vendor		= PCI_VENDOR_ID_SERVERWORKS,
		.device		= PCI_DEVICE_ID_SERVERWORKS_CSB6IDE2,
		.name		= "SvrWks CSB6",
		.init_setup	= init_setup_csb6,
		.init_chipset	= init_chipset_svwks,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_svwks,
		.init_dma	= init_dma_svwks,
		.channels	= 1,	/* 2 */
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

#endif /* SERVERWORKS_H */
