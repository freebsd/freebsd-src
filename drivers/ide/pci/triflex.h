/* 
 * triflex.h
 *
 * Copyright (C) 2002 Hewlett-Packard Development Group, L.P.
 * Author: Torben Mathiasen <torben.mathiasen@hp.com>
 *
 */
#ifndef TRIFLEX_H
#define TRIFLEX_H

#include <linux/config.h>
#include <linux/pci.h>
#include <linux/ide.h>

static unsigned int __devinit init_chipset_triflex(struct pci_dev *, const char *);
static void init_hwif_triflex(ide_hwif_t *);
static int triflex_get_info(char *, char **, off_t, int);

static ide_pci_device_t triflex_devices[] __devinitdata = {
	{
		.vendor 	= PCI_VENDOR_ID_COMPAQ,
		.device		= PCI_DEVICE_ID_COMPAQ_TRIFLEX_IDE,
		.name		= "TRIFLEX",
		.init_chipset	= init_chipset_triflex,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_triflex,
		.channels	= 2,
		.autodma	= AUTODMA,
		.enablebits	= {{0x80, 0x01, 0x01}, {0x80, 0x02, 0x02}},
		.bootable	= ON_BOARD,
	},{	
		.bootable	= EOL,
	}
};

#ifdef CONFIG_PROC_FS
static ide_pci_host_proc_t triflex_proc __initdata = {
	.name		= "triflex",
	.set		= 1,
	.get_info 	= triflex_get_info,
};
#endif

static struct pci_device_id triflex_pci_tbl[] __devinitdata = {
	{ PCI_VENDOR_ID_COMPAQ, PCI_DEVICE_ID_COMPAQ_TRIFLEX_IDE, PCI_ANY_ID, 
		PCI_ANY_ID, 0, 0, 0 },
	{ 0, },
};

#endif /* TRIFLEX_H */
