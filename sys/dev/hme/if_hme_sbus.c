/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 *	from: NetBSD: if_hme_sbus.c,v 1.9 2001/11/13 06:58:17 lukem Exp
 *
 * $FreeBSD$
 */

/*
 * SBus front-end device driver for the HME ethernet device.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/resource.h>
#include <sys/socket.h>

#include <machine/bus.h>
#include <machine/ofw_machdep.h>
#include <machine/resource.h>

#include <sys/rman.h>

#include <ofw/openfirm.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <mii/mii.h>
#include <mii/miivar.h>

#include <sparc64/sbus/sbusvar.h>

#include <hme/if_hmereg.h>
#include <hme/if_hmevar.h>

#include "miibus_if.h"

struct hme_sbus_softc {
	struct	hme_softc	hsc_hme;	/* HME device */
	struct	resource	*hsc_seb_res;
	int			hsc_seb_rid;
	struct	resource	*hsc_etx_res;
	int			hsc_etx_rid;
	struct	resource	*hsc_erx_res;
	int			hsc_erx_rid;
	struct	resource	*hsc_mac_res;
	int			hsc_mac_rid;
	struct	resource	*hsc_mif_res;
	int			hsc_mif_rid;
	struct	resource	*hsc_ires;
	int			hsc_irid;
	void			*hsc_ih;
};

static int hme_sbus_probe(device_t);
static int hme_sbus_attach(device_t);
static int hme_sbus_detach(device_t);
static int hme_sbus_suspend(device_t);
static int hme_sbus_resume(device_t);

static device_method_t hme_sbus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		hme_sbus_probe),
	DEVMETHOD(device_attach,	hme_sbus_attach),
	DEVMETHOD(device_detach,	hme_sbus_detach),
	DEVMETHOD(device_suspend,	hme_sbus_suspend),
	DEVMETHOD(device_resume,	hme_sbus_resume),
	/* Can just use the suspend method here. */
	DEVMETHOD(device_shutdown,	hme_sbus_suspend),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	hme_mii_readreg),
	DEVMETHOD(miibus_writereg,	hme_mii_writereg),
	DEVMETHOD(miibus_statchg,	hme_mii_statchg),

	{ 0, 0 }
};

static driver_t hme_sbus_driver = {
	"hme",
	hme_sbus_methods,
	sizeof(struct hme_sbus_softc)
};

DRIVER_MODULE(if_hme, sbus, hme_sbus_driver, hme_devclass, 0, 0);

static int
hme_sbus_probe(device_t dev)
{
	char *name;

	name = sbus_get_name(dev);
	if (strcmp(name, "SUNW,qfe") == 0 ||
	    strcmp(name, "SUNW,hme") == 0) {
		device_set_desc(dev, "Sun HME 10/100 Ethernet");
		return (0);
	}
	return (ENXIO);
}

static int
hme_sbus_attach(device_t dev)
{
	struct hme_sbus_softc *hsc = device_get_softc(dev);
	struct hme_softc *sc = &hsc->hsc_hme;
	u_int32_t burst;
	u_long start, count;
	int error;

	/*
	 * Map five register banks:
	 *
	 *	bank 0: HME SEB registers
	 *	bank 1: HME ETX registers
	 *	bank 2: HME ERX registers
	 *	bank 3: HME MAC registers
	 *	bank 4: HME MIF registers
	 *
	 */
	sc->sc_sebo = sc->sc_etxo = sc->sc_erxo = sc->sc_maco = sc->sc_mifo = 0;
	hsc->hsc_seb_rid = 0;
	hsc->hsc_seb_res = bus_alloc_resource(dev, SYS_RES_MEMORY,
	    &hsc->hsc_seb_rid, 0, ~0, 1, RF_ACTIVE);
	if (hsc->hsc_seb_res == NULL) {
		device_printf(dev, "cannot map SEB registers\n");
		return (ENXIO);
	}
	sc->sc_sebt = rman_get_bustag(hsc->hsc_seb_res);
	sc->sc_sebh = rman_get_bushandle(hsc->hsc_seb_res);

	hsc->hsc_etx_rid = 1;
	hsc->hsc_etx_res = bus_alloc_resource(dev, SYS_RES_MEMORY,
	    &hsc->hsc_etx_rid, 0, ~0, 1, RF_ACTIVE);
	if (hsc->hsc_etx_res == NULL) {
		device_printf(dev, "cannot map ETX registers\n");
		goto fail_seb_res;
	}
	sc->sc_etxt = rman_get_bustag(hsc->hsc_etx_res);
	sc->sc_etxh = rman_get_bushandle(hsc->hsc_etx_res);

	hsc->hsc_erx_rid = 2;
	hsc->hsc_erx_res = bus_alloc_resource(dev, SYS_RES_MEMORY,
	    &hsc->hsc_erx_rid, 0, ~0, 1, RF_ACTIVE);
	if (hsc->hsc_erx_res == NULL) {
		device_printf(dev, "cannot map ERX registers\n");
		goto fail_etx_res;
	}
	sc->sc_erxt = rman_get_bustag(hsc->hsc_erx_res);
	sc->sc_erxh = rman_get_bushandle(hsc->hsc_erx_res);

	hsc->hsc_mac_rid = 3;
	hsc->hsc_mac_res = bus_alloc_resource(dev, SYS_RES_MEMORY,
	    &hsc->hsc_mac_rid, 0, ~0, 1, RF_ACTIVE);
	if (hsc->hsc_mac_res == NULL) {
		device_printf(dev, "cannot map MAC registers\n");
		goto fail_erx_res;
	}
	sc->sc_mact = rman_get_bustag(hsc->hsc_mac_res);
	sc->sc_mach = rman_get_bushandle(hsc->hsc_mac_res);

	/*
	 * At least on some HMEs, the MIF registers seem to be inside the MAC
	 * range, so map try to kluge around it.
	 */
	hsc->hsc_mif_rid = 4;
	hsc->hsc_mif_res = bus_alloc_resource(dev, SYS_RES_MEMORY,
	    &hsc->hsc_mif_rid, 0, ~0, 1, RF_ACTIVE);
	if (hsc->hsc_mif_res == NULL) {
		if (bus_get_resource(dev, SYS_RES_MEMORY, hsc->hsc_mif_rid,
		    &start, &count) != 0) {
			device_printf(dev, "cannot get MIF registers\n");
			goto fail_mac_res;
		}
		if (start < rman_get_start(hsc->hsc_mac_res) ||
		    start + count - 1 > rman_get_end(hsc->hsc_mac_res)) {
			device_printf(dev, "cannot move MIF registers to MAC "
			    "bank\n");
			goto fail_mac_res;
		}
		sc->sc_mift = sc->sc_mact;
		sc->sc_mifh = sc->sc_mach;
		sc->sc_mifo = sc->sc_maco + start -
		    rman_get_start(hsc->hsc_mac_res);
	} else {
		sc->sc_mift = rman_get_bustag(hsc->hsc_mif_res);
		sc->sc_mifh = rman_get_bushandle(hsc->hsc_mif_res);
	}

	hsc->hsc_irid = 0;
	hsc->hsc_ires = bus_alloc_resource(dev, SYS_RES_IRQ, &hsc->hsc_irid, 0,
	    ~0, 1, RF_SHAREABLE | RF_ACTIVE);
	if (hsc->hsc_ires == NULL) {
		device_printf(dev, "could not allocate interrupt\n");
		error = ENXIO;
		goto fail_mif_res;
	}


	OF_getetheraddr(dev, sc->sc_arpcom.ac_enaddr);

	burst = sbus_get_burstsz(dev);
	/* Translate into plain numerical format */
	sc->sc_burst =  (burst & SBUS_BURST_32) ? 32 :
	    (burst & SBUS_BURST_16) ? 16 : 0;

	sc->sc_pci = 0;	/* XXX: should all be done in bus_dma. */
	sc->sc_dev = dev;

	if ((error = hme_config(sc)) != 0) {
		device_printf(dev, "could not be configured\n");
		goto fail_ires;
	}


	if ((error = bus_setup_intr(dev, hsc->hsc_ires, INTR_TYPE_NET, hme_intr,
	     sc, &hsc->hsc_ih)) != 0) {
		device_printf(dev, "couldn't establish interrupt\n");
		hme_detach(sc);
		goto fail_ires;
	}
	return (0);

fail_ires:
	bus_release_resource(dev, SYS_RES_IRQ, hsc->hsc_irid, hsc->hsc_ires);
fail_mif_res:
	if (hsc->hsc_mif_res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, hsc->hsc_mif_rid,
		    hsc->hsc_mif_res);
	}
fail_mac_res:
	bus_release_resource(dev, SYS_RES_MEMORY, hsc->hsc_mac_rid,
	    hsc->hsc_mac_res);
fail_erx_res:
	bus_release_resource(dev, SYS_RES_MEMORY, hsc->hsc_erx_rid,
	    hsc->hsc_erx_res);
fail_etx_res:
	bus_release_resource(dev, SYS_RES_MEMORY, hsc->hsc_etx_rid,
	    hsc->hsc_etx_res);
fail_seb_res:
	bus_release_resource(dev, SYS_RES_MEMORY, hsc->hsc_seb_rid,
	    hsc->hsc_seb_res);
	return (ENXIO);
}

static int
hme_sbus_detach(device_t dev)
{
	struct hme_sbus_softc *hsc = device_get_softc(dev);
	struct hme_softc *sc = &hsc->hsc_hme;

	hme_detach(sc);

	bus_teardown_intr(dev, hsc->hsc_ires, hsc->hsc_ih);
	if (hsc->hsc_mif_res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, hsc->hsc_mif_rid,
		    hsc->hsc_mif_res);
	}
	bus_release_resource(dev, SYS_RES_MEMORY, hsc->hsc_mac_rid,
	    hsc->hsc_mac_res);
	bus_release_resource(dev, SYS_RES_MEMORY, hsc->hsc_erx_rid,
	    hsc->hsc_erx_res);
	bus_release_resource(dev, SYS_RES_MEMORY, hsc->hsc_etx_rid,
	    hsc->hsc_etx_res);
	bus_release_resource(dev, SYS_RES_MEMORY, hsc->hsc_seb_rid,
	    hsc->hsc_seb_res);
	return (0);
}

static int
hme_sbus_suspend(device_t dev)
{
	struct hme_sbus_softc *hsc = device_get_softc(dev);
	struct hme_softc *sc = &hsc->hsc_hme;

	hme_suspend(sc);
	return (0);
}

static int
hme_sbus_resume(device_t dev)
{
	struct hme_sbus_softc *hsc = device_get_softc(dev);
	struct hme_softc *sc = &hsc->hsc_hme;

	hme_resume(sc);
	return (0);
}
