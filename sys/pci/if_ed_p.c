/*
 *
 * Copyright (c) 1996 Stefan Esser <se@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Absolutely no warranty of function or purpose is made by the author
 *    Stefan Esser.
 * 4. Modifications may be freely made to this file if the above conditions
 *    are met.
 *
 *	$FreeBSD$
 */

#include "pci.h"
#if NPCI > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include "ed.h"

#define PCI_DEVICE_ID_RealTek_8029	0x802910ec
#define PCI_DEVICE_ID_ProLAN_NE2000	0x09401050
#define PCI_DEVICE_ID_Compex_NE2000	0x140111f6

extern void *ed_attach_NE2000_pci __P((int, int));

static char* ed_pci_probe __P((pcici_t tag, pcidi_t type));
static void ed_pci_attach __P((pcici_t config_id, int unit));

static u_long ed_pci_count = NED;

static struct pci_device ed_pci_driver = {
	"ed",
	ed_pci_probe,
	ed_pci_attach,
	&ed_pci_count,
	NULL
};

DATA_SET (pcidevice_set, ed_pci_driver);

static char*
ed_pci_probe (pcici_t tag, pcidi_t type)
{
	switch(type) {
	case PCI_DEVICE_ID_RealTek_8029:
		return ("NE2000 PCI Ethernet (RealTek 8029)");
	case PCI_DEVICE_ID_ProLAN_NE2000:
		return ("NE2000 PCI Ethernet (ProLAN)");
	case PCI_DEVICE_ID_Compex_NE2000:
		return ("NE2000 PCI Ethernet (Compex)");
	}
	return (0);
}

void edintr_sc (void*);

static void
ed_pci_attach(config_id, unit)
	pcici_t config_id;
	int	unit;
{
	int io_port;
	void *ed; /* device specific data ... */

	io_port = pci_conf_read(config_id, PCI_MAP_REG_START) & ~PCI_MAP_IO;

	ed = ed_attach_NE2000_pci(unit, io_port);
	if (!ed)
		return;

	if(!(pci_map_int(config_id, edintr_sc, (void *)ed, &net_imask))) {
		free (ed, M_DEVBUF);
		return;
	}

	return;
}

#endif /* NPCI > 0 */

