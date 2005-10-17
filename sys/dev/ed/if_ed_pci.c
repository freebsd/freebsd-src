/*-
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/kernel.h>

#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_media.h>
#include <net/if_mib.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/ed/if_edvar.h>
#include <dev/ed/rtl80x9reg.h>

static struct _pcsid
{
	uint32_t	type;
	const char	*desc;
} pci_ids[] =
{
	{ ED_RTL8029_PCI_ID, "RealTek 8029" },
	{ 0x50004a14, "NetVin 5000" },
	{ 0x09401050, "ProLAN" },
	{ 0x140111f6, "Compex" },
	{ 0x30008e2e, "KTI" },
	{ 0x19808c4a, "Winbond W89C940" },
	{ 0x0e3410bd, "Surecom NE-34" },
	{ 0x09261106, "VIA VT86C926" },
	{ 0x00000000, NULL }
};

static int	ed_pci_probe(device_t);
static int	ed_pci_attach(device_t);

static int
ed_pci_probe(device_t dev)
{
	uint32_t	type = pci_get_devid(dev);
	struct _pcsid	*ep =pci_ids;

	while (ep->type && ep->type != type)
		++ep;
	if (ep->desc == NULL)
		return (ENXIO);
	device_set_desc(dev, ep->desc);
	return (BUS_PROBE_DEFAULT);
}

static int
ed_pci_attach(device_t dev)
{
	struct	ed_softc *sc = device_get_softc(dev);
	int	flags = 0;
	int	error = ENXIO;

	/*
	 * If this card claims to be a RTL8029, probe it as such.
	 * However, allow that probe to fail.  Some versions of qemu
	 * claim to be a 8029 in the PCI register, but it doesn't
	 * implement the 8029 specific registers.  In that case, fall
	 * back to a normal NE2000.
	 */
	if (pci_get_devid(dev) == ED_RTL8029_PCI_ID)
		error = ed_probe_RTL80x9(dev, PCIR_BAR(0), flags);
	if (error)
		error = ed_probe_Novell(dev, PCIR_BAR(0), flags);
	if (error) {
		ed_release_resources(dev);
		return (error);
	}
	ed_Novell_read_mac(sc);

	error = ed_alloc_irq(dev, 0, RF_SHAREABLE);
	if (error) {
		ed_release_resources(dev);
		return (error);
	}
	error = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_NET | INTR_MPSAFE,
	    edintr, sc, &sc->irq_handle);
	if (error) {
		ed_release_resources(dev);
		return (error);
	}

	error = ed_attach(dev);
	if (error)
		ed_release_resources(dev);
	return (error);
}

static device_method_t ed_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ed_pci_probe),
	DEVMETHOD(device_attach,	ed_pci_attach),
	DEVMETHOD(device_detach,	ed_detach),

	{ 0, 0 }
};

static driver_t ed_pci_driver = {
	"ed",
	ed_pci_methods,
	sizeof(struct ed_softc),
};

DRIVER_MODULE(ed, pci, ed_pci_driver, ed_devclass, 0, 0);
MODULE_DEPEND(ed, pci, 1, 1, 1);
MODULE_DEPEND(ed, ether, 1, 1, 1);
