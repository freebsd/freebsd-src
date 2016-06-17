#ifndef VIA82CXXX_H
#define VIA82CXXX_H

#include <linux/config.h>
#include <linux/pci.h>
#include <linux/ide.h>

#define DISPLAY_VIA_TIMINGS

#if defined(DISPLAY_VIA_TIMINGS) && defined(CONFIG_PROC_FS)
#include <linux/stat.h>
#include <linux/proc_fs.h>

static u8 via_proc;

static int via_get_info(char *, char **, off_t, int);

static ide_pci_host_proc_t via_procs[] __initdata = {
	{
		.name		= "via",
		.set		= 1,
		.get_info	= via_get_info,
		.parent		= NULL,
	},
};
#endif /* DISPLAY_VIA_TIMINGS && CONFIG_PROC_FS */

static unsigned int init_chipset_via82cxxx(struct pci_dev *, const char *);
static void init_hwif_via82cxxx(ide_hwif_t *);
static void init_dma_via82cxxx(ide_hwif_t *, unsigned long);

static ide_pci_device_t via82cxxx_chipsets[] __devinitdata = {
	{	/* 0 */
		.vendor		= PCI_VENDOR_ID_VIA,
		.device		= PCI_DEVICE_ID_VIA_82C576_1,
		.name		= "VP_IDE",
		.init_chipset	= init_chipset_via82cxxx,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_via82cxxx,
		.init_dma	= init_dma_via82cxxx,
		.channels	= 2,
		.autodma	= NOAUTODMA,
		.enablebits	= {{0x40,0x02,0x02}, {0x40,0x01,0x01}},
		.bootable	= ON_BOARD,
		.extra		= 0,
	},{	/* 1 */
		.vendor		= PCI_VENDOR_ID_VIA,
		.device		= PCI_DEVICE_ID_VIA_82C586_1,
		.name		= "VP_IDE",
		.init_chipset	= init_chipset_via82cxxx,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_via82cxxx,
		.init_dma	= init_dma_via82cxxx,
		.channels	= 2,
		.autodma	= NOAUTODMA,
		.enablebits	= {{0x40,0x02,0x02}, {0x40,0x01,0x01}},
		.bootable	= ON_BOARD,
		.extra		= 0,
	},{
		.vendor		= 0,
		.device		= 0,
		.channels	= 0,
		.bootable	= EOL,
	}
};

#endif /* VIA82CXXX_H */
