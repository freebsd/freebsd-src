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
 *	$Id: if_lnc_p.c,v 1.4 1997/04/04 16:44:52 kato Exp $
 */

#include "pci.h"
#if NPCI > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include "lnc.h"

#define PCI_DEVICE_ID_PCNet_PCI	0x20001022

extern void *lnc_attach_ne2100_pci __P((int unit, unsigned iobase));

static char* lnc_pci_probe __P((pcici_t tag, pcidi_t type));
static void lnc_pci_attach __P((pcici_t config_id, int unit));

static u_long lnc_pci_count = NLNC;

static struct pci_device lnc_pci_driver = {
	"lnc",
	lnc_pci_probe,
	lnc_pci_attach,
	&lnc_pci_count,
	NULL
};

DATA_SET (pcidevice_set, lnc_pci_driver);

static char*
lnc_pci_probe (pcici_t tag, pcidi_t type)
{
	switch(type) {
	case PCI_DEVICE_ID_PCNet_PCI:
		return ("PCNet/PCI Ethernet adapter");
		break;
	default:
		break;
	}
	return (0);
}

void lncintr_sc (void*);

static void
lnc_pci_attach(config_id, unit)
	pcici_t config_id;
	int	unit;
{
	unsigned iobase;
	void *lnc; /* device specific data for interrupt handler ... */

	iobase = pci_conf_read(config_id, PCI_MAP_REG_START) & ~PCI_MAP_IO;

	lnc = lnc_attach_ne2100_pci(unit, iobase);
	if (!lnc)
		return;

	if(!(pci_map_int(config_id, lncintr_sc, (void *)lnc, &net_imask))) {
		free (lnc, M_DEVBUF);
		return;
	}

	return;
}

#endif /* NPCI > 0 */

