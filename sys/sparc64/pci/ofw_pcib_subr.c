/*-
 * Copyright (c) 2003 by Thomas Moestl <tmm@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/sparc64/pci/ofw_pcib_subr.c,v 1.8 2007/06/18 21:49:42 marius Exp $");

#include "opt_ofw_pci.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_pci.h>
#include <dev/ofw/openfirm.h>

#include <machine/bus.h>
#include <machine/ofw_bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcib_private.h>

#include "pcib_if.h"

#include <sparc64/pci/ofw_pci.h>
#include <sparc64/pci/ofw_pcib_subr.h>

void
ofw_pcib_gen_setup(device_t bridge)
{
	struct ofw_pcib_gen_softc *sc;
#ifndef SUN4V
	int secbus;

#endif
	sc = device_get_softc(bridge);
	sc->ops_pcib_sc.dev = bridge;
	sc->ops_node = ofw_bus_get_node(bridge);
	KASSERT(sc->ops_node != 0,
	    ("ofw_pcib_gen_setup: no ofw pci parent bus!"));

	/*
	 * Setup the secondary bus number register, if supported, by
	 * allocating a new unique bus number for it; the firmware
	 * preset does not always seem to be correct in that case.
	 */
#ifndef SUN4V
	secbus = OFW_PCI_ALLOC_BUSNO(bridge);
	if (secbus != -1) {
		pci_write_config(bridge, PCIR_PRIBUS_1, pci_get_bus(bridge), 1);
		pci_write_config(bridge, PCIR_SECBUS_1, secbus, 1);
		pci_write_config(bridge, PCIR_SUBBUS_1, secbus, 1);
		sc->ops_pcib_sc.subbus = sc->ops_pcib_sc.secbus = secbus;
		/* Notify parent bridges. */
		OFW_PCI_ADJUST_BUSRANGE(device_get_parent(bridge), secbus);
	}

#endif
	ofw_bus_setup_iinfo(sc->ops_node, &sc->ops_iinfo,
	    sizeof(ofw_pci_intr_t));
}

int
ofw_pcib_gen_route_interrupt(device_t bridge, device_t dev, int intpin)
{
	struct ofw_pcib_gen_softc *sc;
	struct ofw_bus_iinfo *ii;
	struct ofw_pci_register reg;
	ofw_pci_intr_t pintr, mintr;
	uint8_t maskbuf[sizeof(reg) + sizeof(pintr)];

	sc = device_get_softc(bridge);
	ii = &sc->ops_iinfo;
	if (ii->opi_imapsz > 0) {
		pintr = intpin;
		if (ofw_bus_lookup_imap(ofw_bus_get_node(dev), ii, &reg,
		    sizeof(reg), &pintr, sizeof(pintr), &mintr, sizeof(mintr),
		    maskbuf)) {
			/*
			 * If we've found a mapping, return it and don't map
			 * it again on higher levels - that causes problems
			 * in some cases, and never seems to be required.
			 */
			return (mintr);
		}
	} else if (intpin >= 1 && intpin <= 4) {
		/*
		 * When an interrupt map is missing, we need to do the
		 * standard PCI swizzle and continue mapping at the parent.
		 */
		return (pcib_route_interrupt(bridge, dev, intpin));
	}
	/* Try at the parent. */
	return (PCIB_ROUTE_INTERRUPT(device_get_parent(device_get_parent(
	    bridge)), bridge, intpin));
}

phandle_t
ofw_pcib_gen_get_node(device_t bridge, device_t dev)
{
	struct ofw_pcib_gen_softc *sc;

	sc = device_get_softc(bridge);
	return (sc->ops_node);
}

void
ofw_pcib_gen_adjust_busrange(device_t bridge, u_int subbus)
{
	struct ofw_pcib_gen_softc *sc;

	sc = device_get_softc(bridge);
	if (subbus > sc->ops_pcib_sc.subbus) {
#ifdef OFW_PCI_DEBUG
		device_printf(bridge,
		    "adjusting subordinate bus number from %d to %d\n",
		    sc->ops_pcib_sc.subbus, subbus);
#endif
		pci_write_config(bridge, PCIR_SUBBUS_1, subbus, 1);
		sc->ops_pcib_sc.subbus = subbus;
		/* Notify parent bridges. */
		OFW_PCI_ADJUST_BUSRANGE(device_get_parent(bridge), subbus);
	}
}
