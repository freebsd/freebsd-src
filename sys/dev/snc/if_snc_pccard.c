/*-
 * Copyright (c) 1995, David Greenman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 *	National Semiconductor  DP8393X SONIC Driver
 *
 *	This is the PC Card attachment on FreeBSD
 *		written by Motomichi Matsuzaki <mzaki@e-mail.ne.jp> and
 *			   Hiroshi Yamashita <bluemoon@msj.biglobe.ne.jp>
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_media.h>

#include <dev/snc/dp83932var.h>
#include <dev/snc/if_sncvar.h>
#include <dev/snc/if_sncreg.h>

#include <dev/pccard/pccardvar.h>
#include <dev/pccard/pccard_cis.h>
#include "pccarddevs.h"

static const struct pccard_product snc_pccard_products[] = {
	PCMCIA_CARD(NEC, PC9801N_J02),
	PCMCIA_CARD(NEC, PC9801N_J02R),
	{ NULL }
};

/*
 *      PC Card (PCMCIA) specific code.
 */
static int	snc_pccard_probe(device_t);
static int	snc_pccard_attach(device_t);
static int	snc_pccard_detach(device_t);


static device_method_t snc_pccard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		snc_pccard_probe),
	DEVMETHOD(device_attach,	snc_pccard_attach),
	DEVMETHOD(device_detach,	snc_pccard_detach),

	{ 0, 0 }
};

static driver_t snc_pccard_driver = {
	"snc",
	snc_pccard_methods,
	sizeof(struct snc_softc)
};

DRIVER_MODULE(snc, pccard, snc_pccard_driver, snc_devclass, 0, 0);
MODULE_DEPEND(snc, ether, 1, 1, 1);

/*
 *      snc_pccard_detach - detach this instance from the device.
 */
static int
snc_pccard_detach(device_t dev)
{
	struct snc_softc *sc = device_get_softc(dev);
	struct ifnet *ifp = sc->sc_ifp;

	if (sc->gone) {
		device_printf(dev, "already unloaded\n");
		return (0);
	}
	SNC_LOCK(sc);
	sncshutdown(sc);
	SNC_UNLOCK(sc);
	callout_drain(&sc->sc_timer);
	ether_ifdetach(ifp);
	sc->gone = 1;
	bus_teardown_intr(dev, sc->irq, sc->irq_handle);
	snc_release_resources(dev);
	mtx_destroy(&sc->sc_lock);
	return (0);
}

/* 
 * Probe the pccard.
 */
static int
snc_pccard_probe(device_t dev)
{
	const struct pccard_product *pp;

	if ((pp = pccard_product_lookup(dev, snc_pccard_products,
	    sizeof(snc_pccard_products[0]), NULL)) == NULL)
		return (EIO);
	if (pp->pp_name != NULL)
		device_set_desc(dev, pp->pp_name);
	return (0);
}

static int
snc_pccard_attach(device_t dev)
{
	struct snc_softc *sc = device_get_softc(dev);
	int error;
	
	/*
	 * Not sure that this belongs here or in snc_pccard_attach
	 */
	if ((error = snc_alloc_port(dev, 0)) != 0)
		goto err;
	if ((error = snc_alloc_memory(dev, 0)) != 0)
		goto err;
	if ((error = snc_alloc_irq(dev, 0, 0)) != 0)
		goto err;
	if ((error = snc_probe(dev, SNEC_TYPE_PNP)) != 0)
		goto err;
	/* This interface is always enabled. */
	sc->sc_enabled = 1;
	/* pccard_get_ether(dev, ether_addr); */
	if ((error = snc_attach(dev)) != 0)
		goto err;
	return 0;
err:;
	snc_release_resources(dev);
	return error;
} 
