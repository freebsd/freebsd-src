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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <sys/socket.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <dev/lnc/if_lncvar.h>

#include "lnc.h"

#ifndef COMPAT_OLDPCI
#error "The lnc device requires the old pci compatibility shims"
#endif

#define PCI_DEVICE_ID_PCNet_PCI	0x20001022
#define PCI_DEVICE_ID_PCHome_PCI 0x20011022

extern int pcnet_probe __P((lnc_softc_t *sc));
extern int lnc_attach_sc __P((lnc_softc_t *sc, int unit));

static const char* lnc_pci_probe __P((pcici_t tag, pcidi_t type));
static void lnc_pci_attach __P((pcici_t config_id, int unit));

static u_long lnc_pci_count = NLNC;

static struct pci_device lnc_pci_driver = {
	"lnc",
	lnc_pci_probe,
	lnc_pci_attach,
	&lnc_pci_count,
	NULL
};

COMPAT_PCI_DRIVER (lnc_pci, lnc_pci_driver);

static const char*
lnc_pci_probe (pcici_t tag, pcidi_t type)
{
	switch(type) {
	case PCI_DEVICE_ID_PCNet_PCI:
		return ("PCNet/PCI Ethernet adapter");
		break;
	case PCI_DEVICE_ID_PCHome_PCI:
		return ("PCHome/PCI Ethernet adapter");
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
	lnc_softc_t *sc;
	unsigned iobase;
	unsigned data;	/* scratch to make this device a bus master*/
	int i;

	if ( !pci_map_port(config_id,PCI_MAP_REG_START,(u_short *)&iobase) )
	    printf("lnc%d: pci_port_map_attach failed?!\n",unit);


	/* Make this device a bus master.  This was implictly done by 
	   pci_map_port under 2.2.x -- tvf */

	data = pci_cfgread(config_id, PCIR_COMMAND, 4);
	data |= PCIM_CMD_PORTEN | PCIM_CMD_BUSMASTEREN;
	pci_cfgwrite(config_id, PCIR_COMMAND, data, 4);

	sc = malloc(sizeof *sc, M_DEVBUF, M_NOWAIT | M_ZERO);

	if (sc) {
		sc->rap = iobase + PCNET_RAP;
		sc->rdp = iobase + PCNET_RDP;
		sc->bdp = iobase + PCNET_BDP;

		sc->nic.ic = pcnet_probe(sc);
		if (sc->nic.ic >= PCnet_32) {
			sc->nic.ident = NE2100;
			sc->nic.mem_mode = DMA_FIXED;
  
			/* XXX - For now just use the defines */
			sc->nrdre = NRDRE;
			sc->ntdre = NTDRE;

			/* Extract MAC address from PROM */
			for (i = 0; i < ETHER_ADDR_LEN; i++)
				sc->arpcom.ac_enaddr[i] = inb(iobase + i);

			if (lnc_attach_sc(sc, unit) == 0) {
				free(sc, M_DEVBUF);
				sc = NULL;
			}

			if(!(pci_map_int(config_id, lncintr_sc, (void *)sc, &net_imask))) {
				free (sc, M_DEVBUF);
				return;
			}
		} else {
			free(sc, M_DEVBUF);
		}
	}

	return;
}
