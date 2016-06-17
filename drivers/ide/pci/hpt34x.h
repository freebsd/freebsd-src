#ifndef HPT34X_H
#define HPT34X_H

#include <linux/config.h>
#include <linux/pci.h>
#include <linux/ide.h>

#define HPT343_DEBUG_DRIVE_INFO		0

#ifndef SPLIT_BYTE
#define SPLIT_BYTE(B,H,L)	((H)=(B>>4), (L)=(B-((B>>4)<<4)))
#endif

#undef DISPLAY_HPT34X_TIMINGS

#if defined(DISPLAY_HPT34X_TIMINGS) && defined(CONFIG_PROC_FS)
#include <linux/stat.h>
#include <linux/proc_fs.h>

static u8 hpt34x_proc;

static int hpt34x_get_info(char *, char **, off_t, int);

static ide_pci_host_proc_t hpt34x_procs[] __initdata = {
	{
		.name		= "hpt34x",
		.set		= 1,
		.get_info	= hpt34x_get_info,
		.parent		= NULL,
	},
};
#endif  /* defined(DISPLAY_HPT34X_TIMINGS) && defined(CONFIG_PROC_FS) */

static unsigned int init_chipset_hpt34x(struct pci_dev *, const char *);
static void init_hwif_hpt34x(ide_hwif_t *);
static void init_dma_hpt34x(ide_hwif_t *, unsigned long);

static ide_pci_device_t hpt34x_chipsets[] __devinitdata = {
	{	/* 0 */
		.vendor		= PCI_VENDOR_ID_TTI,
		.device		= PCI_DEVICE_ID_TTI_HPT343,
		.name		= "HPT34X",
		.init_chipset	= init_chipset_hpt34x,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_hpt34x,
		.init_dma	= init_dma_hpt34x,
		.channels	= 2,
		.autodma	= NOAUTODMA,
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
		.bootable	= NEVER_BOARD,
		.extra		= 16
	},{
		.vendor		= 0,
		.device		= 0,
		.channels	= 0,
		.bootable	= EOL,
	}
};

#endif /* HPT34X_H */
