#ifndef SIS5513_H
#define SIS5513_H

#include <linux/config.h>
#include <linux/pci.h>
#include <linux/ide.h>

#define DISPLAY_SIS_TIMINGS

#if defined(DISPLAY_SIS_TIMINGS) && defined(CONFIG_PROC_FS)
#include <linux/stat.h>
#include <linux/proc_fs.h>

static u8 sis_proc;

static int sis_get_info(char *, char **, off_t, int);

static ide_pci_host_proc_t sis_procs[] __initdata = {
{
		.name		= "sis",
		.set		= 1,
		.get_info	= sis_get_info,
		.parent		= NULL,
	},
};
#endif /* defined(DISPLAY_SIS_TIMINGS) && defined(CONFIG_PROC_FS) */

static unsigned int init_chipset_sis5513(struct pci_dev *, const char *);
static void init_hwif_sis5513(ide_hwif_t *);
static void init_dma_sis5513(ide_hwif_t *, unsigned long);

static ide_pci_device_t sis5513_chipsets[] __devinitdata = {
	{	/* 0 */
		.vendor		= PCI_VENDOR_ID_SI,
		.device		= PCI_DEVICE_ID_SI_5513,
		.name		= "SIS5513",
		.init_chipset	= init_chipset_sis5513,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_sis5513,
		.init_dma	= init_dma_sis5513,
		.channels	= 2,
		.autodma	= NOAUTODMA,
		.enablebits	= {{0x4a,0x02,0x02}, {0x4a,0x04,0x04}},
		.bootable	= ON_BOARD,
		.extra		= 0
	},{
		.vendor		= 0,
		.device		= 0,
		.channels	= 0,
		.bootable	= EOL,
	}
};

#endif /* SIS5513_H */
