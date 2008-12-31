/*-
 * Copyright (c) 2000 Atsushi Onoe <onoe@sm.sony.co.jp>
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
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/awi/if_awi_pccard.c,v 1.25.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>
 
#include <net/if.h> 
#include <net/if_arp.h>
#include <net/if_media.h>
#include <net/ethernet.h>

#include <net80211/ieee80211_var.h>

#include <dev/awi/am79c930reg.h>
#include <dev/awi/am79c930var.h>
#include <dev/awi/awireg.h>
#include <dev/awi/awivar.h>

#include <dev/pccard/pccardvar.h>

#include "card_if.h"
#include "pccarddevs.h"

struct awi_pccard_softc {
	struct awi_softc	sc_awi;

	u_int8_t 	sc_version[AWI_BANNER_LEN];
	int		sc_intr_mask;
	void		*sc_intrhand;
	struct resource	*sc_irq_res;
	int		sc_irq_rid;
	struct resource	*sc_port_res;
	int		sc_port_rid;
	struct resource	*sc_mem_res;
	int		sc_mem_rid;
};

static const struct pccard_product awi_pccard_products[] = {
	PCMCIA_CARD(AMD, AM79C930),
	PCMCIA_CARD(BAY, STACK_650),
	PCMCIA_CARD(BAY, STACK_660),
	PCMCIA_CARD(BAY, SURFER_PRO),
	PCMCIA_CARD(ICOM, SL200),
	PCMCIA_CARD(NOKIA, C020_WLAN),
	PCMCIA_CARD(FARALLON, SKYLINE),
	PCMCIA_CARD(ZOOM, AIR_4000),
	{ NULL }
};

static int awi_pccard_probe(device_t);
static int awi_pccard_attach(device_t);
static int awi_pccard_detach(device_t);
static void awi_pccard_shutdown(device_t);
static int awi_pccard_enable(struct awi_softc *);
static void awi_pccard_disable(struct awi_softc *);

static int
awi_pccard_probe(device_t dev)
{
	const struct pccard_product *pp;

	if ((pp = pccard_product_lookup(dev, awi_pccard_products,
	    sizeof(awi_pccard_products[0]), NULL)) != NULL) {
		if (pp->pp_name != NULL)
			device_set_desc(dev, pp->pp_name);
		return 0;
	}
	return ENXIO;
}

/*
 * Initialize the device - called from Slot manager.
 */
static int
awi_pccard_attach(device_t dev)
{
	struct awi_pccard_softc *psc = device_get_softc(dev);
	struct awi_softc *sc = &psc->sc_awi;
	int error = 0;

	psc->sc_port_rid = 0;
	psc->sc_port_res = bus_alloc_resource(dev, SYS_RES_IOPORT,
	    &psc->sc_port_rid, 0, ~0, AM79C930_IO_SIZE,
	    rman_make_alignment_flags(AM79C930_IO_ALIGN) | RF_ACTIVE);
	if (!psc->sc_port_res)
		return ENOMEM;

	sc->sc_chip.sc_iot = rman_get_bustag(psc->sc_port_res);
	sc->sc_chip.sc_ioh = rman_get_bushandle(psc->sc_port_res);
	am79c930_chip_init(&sc->sc_chip, 0);
	tsleep(sc, PWAIT, "awiprb", 1);

	awi_read_bytes(sc, AWI_BANNER, psc->sc_version, AWI_BANNER_LEN);
	if (memcmp(psc->sc_version, "PCnetMobile:", 12) != 0)  {
		device_printf(dev, "awi_pccard_probe: bad banner: %12D\n",
		    psc->sc_version, " ");
		error = ENXIO;
	} else
		device_set_desc(dev, psc->sc_version);

	psc->sc_irq_res = 0;
	psc->sc_mem_res = 0;
	psc->sc_intrhand = 0;

	psc->sc_port_rid = 0;
	psc->sc_port_res = bus_alloc_resource(dev, SYS_RES_IOPORT,
	    &psc->sc_port_rid, 0, ~0, 16, RF_ACTIVE);
	if (!psc->sc_port_res) {
		device_printf(dev, "awi_pccard_attach: port alloc failed\n");
		goto fail;
	}
	sc->sc_chip.sc_iot = rman_get_bustag(psc->sc_port_res);
	sc->sc_chip.sc_ioh = rman_get_bushandle(psc->sc_port_res);

	psc->sc_irq_rid = 0;
	psc->sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ,
	    &psc->sc_irq_rid, RF_ACTIVE);
	if (!psc->sc_irq_res) {
		device_printf(dev, "awi_pccard_attach: irq alloc failed\n");
		goto fail;
	}

	psc->sc_mem_rid = 0;
#if 1
	/*
	 * XXX: awi needs to access memory with 8bit,
	 * but OLDCARD apparently maps memory with MDF_16BITS flag.
	 * So memory mapped access is disabled and use IO port instead.
	 * XXX: Should check to see if this is true of NEWCARD
	 */
	psc->sc_mem_res = 0;
#else
	psc->sc_mem_res = bus_alloc_resource(dev, SYS_RES_MEMORY,
	    &psc->sc_mem_rid, 0, ~0, 0x8000, RF_ACTIVE);
#endif
	if (psc->sc_mem_res) {
		sc->sc_chip.sc_memt = rman_get_bustag(psc->sc_mem_res);
		sc->sc_chip.sc_memh = rman_get_bushandle(psc->sc_mem_res);
		am79c930_chip_init(&sc->sc_chip, 1);
	} else
		am79c930_chip_init(&sc->sc_chip, 0);

	sc->sc_dev = dev;
	sc->sc_cansleep = 1;
	sc->sc_enable = awi_pccard_enable;
	sc->sc_disable = awi_pccard_disable;

	if (awi_pccard_enable(sc))
		goto fail;
	sc->sc_enabled = 1;
	error = awi_attach(sc);
	sc->sc_enabled = 0;	/*XXX*/
	awi_pccard_disable(sc);
	if (error == 0)
		return 0;
	device_printf(dev, "awi_pccard_attach: awi_attach failed\n");

  fail:
	awi_pccard_detach(dev);
	if (error == 0)
		error = ENXIO;
	return error;
}

static int
awi_pccard_detach(device_t dev)
{
	struct awi_pccard_softc *psc = device_get_softc(dev);
	struct awi_softc *sc = &psc->sc_awi;

	awi_detach(sc);
	if (psc->sc_mem_res) {
		bus_release_resource(dev, SYS_RES_MEMORY, psc->sc_mem_rid,
		    psc->sc_mem_res);
		psc->sc_mem_res = 0;
	}
	if (psc->sc_irq_res) {
		bus_release_resource(dev, SYS_RES_IRQ, psc->sc_irq_rid,
		    psc->sc_irq_res);
		psc->sc_irq_res = 0;
	}
	if (psc->sc_port_res) {
		bus_release_resource(dev, SYS_RES_IOPORT, psc->sc_port_rid,
		    psc->sc_port_res);
		psc->sc_port_res = 0;
	}
	return 0;
}

static void
awi_pccard_shutdown(device_t dev)
{
	struct awi_pccard_softc *psc = device_get_softc(dev);
	struct awi_softc *sc = &psc->sc_awi;

	awi_shutdown(sc);
}

static int
awi_pccard_enable(struct awi_softc *sc)
{
	device_t dev = sc->sc_dev;
	struct awi_pccard_softc *psc = device_get_softc(dev);
	int error;

	if (psc->sc_intrhand == 0) {
		error = bus_setup_intr(dev, psc->sc_irq_res, INTR_TYPE_NET,
		    NULL, (void (*)(void *))awi_intr, sc, &psc->sc_intrhand);
		if (error) {
			device_printf(dev,
			    "couldn't establish interrupt error=%d\n", error);
			return error;
		}
	}
	return 0;
}

static void
awi_pccard_disable(struct awi_softc *sc)
{
	device_t dev = sc->sc_dev;
	struct awi_pccard_softc *psc = device_get_softc(dev);

	if (psc->sc_intrhand) {
		bus_teardown_intr(dev, psc->sc_irq_res, psc->sc_intrhand);
		psc->sc_intrhand = 0;
	}
}

static device_method_t awi_pccard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		awi_pccard_probe),
	DEVMETHOD(device_attach,	awi_pccard_attach),
	DEVMETHOD(device_detach,	awi_pccard_detach),
	DEVMETHOD(device_shutdown,	awi_pccard_shutdown),

	{ 0, 0 }
};

static driver_t awi_pccard_driver = {
	"awi",
	awi_pccard_methods,
	sizeof(struct awi_pccard_softc),
};

extern devclass_t awi_devclass;

DRIVER_MODULE(awi, pccard, awi_pccard_driver, awi_devclass, 0, 0);
MODULE_DEPEND(awi, wlan, 1, 1, 1);
