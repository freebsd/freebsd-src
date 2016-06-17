#ifndef SC1200_H
#define SC1200_H

#include <linux/config.h>
#include <linux/pci.h>
#include <linux/ide.h>

#define DISPLAY_SC1200_TIMINGS

#if defined(DISPLAY_SC1200_TIMINGS) && defined(CONFIG_PROC_FS)
#include <linux/stat.h>
#include <linux/proc_fs.h>

static u8 sc1200_proc;

static int sc1200_get_info(char *, char **, off_t, int);

static ide_pci_host_proc_t sc1200_procs[] __initdata = {
	{
		name:		"sc1200",
		set:		1,
		get_info:	sc1200_get_info,
		parent:		NULL,
	},
};
#endif /* DISPLAY_SC1200_TIMINGS && CONFIG_PROC_FS */

static unsigned int init_chipset_sc1200(struct pci_dev *, const char *);
static void init_hwif_sc1200(ide_hwif_t *);
static void init_dma_sc1200(ide_hwif_t *, unsigned long);

static ide_pci_device_t sc1200_chipsets[] __devinitdata = {
	{	/* 0 */
		vendor:		PCI_VENDOR_ID_NS,
		device:		PCI_DEVICE_ID_NS_SCx200_IDE,
		name:		"SC1200",
		init_chipset:	init_chipset_sc1200,
		init_iops:	NULL,
		init_hwif:	init_hwif_sc1200,
		init_dma:	init_dma_sc1200,
		channels:	2,
		autodma:	AUTODMA,
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

#endif /* SC1200_H */
