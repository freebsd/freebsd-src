#ifndef ALI15X3_H
#define ALI15X3_H

#include <linux/config.h>
#include <linux/pci.h>
#include <linux/ide.h>

#define DISPLAY_ALI_TIMINGS

#if defined(DISPLAY_ALI_TIMINGS) && defined(CONFIG_PROC_FS)
#include <linux/stat.h>
#include <linux/proc_fs.h>

static u8 ali_proc;

static int ali_get_info(char *, char **, off_t, int);

static ide_pci_host_proc_t ali_procs[] __initdata = {
	{
		.name		= "ali",
		.set		= 1,
		.get_info	= ali_get_info,
		.parent		= NULL,
	},
};
#endif /* DISPLAY_ALI_TIMINGS && CONFIG_PROC_FS */

static unsigned int init_chipset_ali15x3(struct pci_dev *, const char *);
static void init_hwif_common_ali15x3(ide_hwif_t *);
static void init_hwif_ali15x3(ide_hwif_t *);
static void init_dma_ali15x3(ide_hwif_t *, unsigned long);

static ide_pci_device_t ali15x3_chipsets[] __devinitdata = {
	{	/* 0 */
		.vendor		= PCI_VENDOR_ID_AL,
		.device		= PCI_DEVICE_ID_AL_M5229,
		.name		= "ALI15X3",
		.init_chipset	= init_chipset_ali15x3,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_ali15x3,
		.init_dma	= init_dma_ali15x3,
		.channels	= 2,
		.autodma	= AUTODMA,
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
		.bootable	= ON_BOARD,
		.extra		= 0
	},{
		.vendor		= 0,
		.device		= 0,
		.channels	= 0,
		.bootable	= EOL,
	}
};

#endif /* ALI15X3 */
