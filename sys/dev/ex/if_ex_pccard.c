/*-
 * Copyright (c) 2000 Mitsuru IWASAKI
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

#include <dev/ex/if_exreg.h>
#include <dev/ex/if_exvar.h>

#include <dev/pccard/pccardvar.h>
#include <dev/pccard/pccard_cis.h>

#include "pccarddevs.h"

static const struct pccard_product ex_pccard_products[] = {
	PCMCIA_CARD(OLICOM, OC2220),
	PCMCIA_CARD(OLICOM, OC2231),
	PCMCIA_CARD(OLICOM, OC2232),
	PCMCIA_CARD(INTEL, ETHEREXPPRO),
	{ NULL }
};

/* Bus Front End Functions */
static int	ex_pccard_probe(device_t);
static int	ex_pccard_attach(device_t);

static int
ex_pccard_enet_ok(u_char *enaddr)
{
	int			i;
	u_char			sum;

	if (enaddr[0] == 0xff)
		return (0);
	for (i = 0, sum = 0; i < ETHER_ADDR_LEN; i++)
		sum |= enaddr[i];
	return (sum != 0);
}

static int
ex_pccard_silicom_cb(const struct pccard_tuple *tuple, void *arg)
{
	u_char *enaddr = arg;
	int i;

	if (tuple->code != CISTPL_FUNCE)
		return (0);
	if (tuple->length != 15)
		return (0);
	if (pccard_tuple_read_1(tuple, 6) != 6)
		return (0);
	for (i = 0; i < 6; i++)
		enaddr[i] = pccard_tuple_read_1(tuple, 7 + i);
	return (1);
}

static void
ex_pccard_get_silicom_mac(device_t dev, u_char *ether_addr)
{
	pccard_cis_scan(dev, ex_pccard_silicom_cb, ether_addr);
}

static int
ex_pccard_probe(device_t dev)
{
	const struct pccard_product *pp;
	int error;
	uint32_t	fcn = PCCARD_FUNCTION_UNSPEC;

	/* Make sure we're a network function */
	error = pccard_get_function(dev, &fcn);
	if (error != 0)
		return (error);
	if (fcn != PCCARD_FUNCTION_NETWORK)
		return (ENXIO);

	if ((pp = pccard_product_lookup(dev, ex_pccard_products,
	    sizeof(ex_pccard_products[0]), NULL)) != NULL) {
		if (pp->pp_name != NULL)
			device_set_desc(dev, pp->pp_name);
		return 0;
	}
	return EIO;
}

static int
ex_pccard_attach(device_t dev)
{
	struct ex_softc *	sc = device_get_softc(dev);
	int			error = 0;
	u_char			ether_addr[ETHER_ADDR_LEN];

	sc->dev = dev;
	sc->ioport_rid = 0;
	sc->irq_rid = 0;

	if ((error = ex_alloc_resources(dev)) != 0) {
		device_printf(dev, "ex_alloc_resources() failed!\n");
		goto bad;
	}

	/*
	 * Fill in several fields of the softc structure:
	 *	- Hardware Ethernet address.
	 *	- IRQ number.
	 */
	sc->irq_no = rman_get_start(sc->irq);

	/* Try to get the ethernet address from the chip, then the CIS */
	ex_get_address(sc, ether_addr);
	if (!ex_pccard_enet_ok(ether_addr))
		pccard_get_ether(dev, ether_addr);
	if (!ex_pccard_enet_ok(ether_addr))
		ex_pccard_get_silicom_mac(dev, ether_addr);
	if (!ex_pccard_enet_ok(ether_addr)) {
		device_printf(dev, "No NIC address found.\n");
		error = ENXIO;
		goto bad;
	}
	bcopy(ether_addr, sc->enaddr, ETHER_ADDR_LEN);

	if ((error = ex_attach(dev)) != 0) {
		device_printf(dev, "ex_attach() failed!\n");
		goto bad;
	}

	error = bus_setup_intr(dev, sc->irq, INTR_TYPE_NET,
	    ex_intr, (void *)sc, &sc->ih);
	if (error) {
		device_printf(dev, "bus_setup_intr() failed!\n");
		goto bad;
	}

	return(0);
bad:
	ex_release_resources(dev);
	return (error);
}
static device_method_t ex_pccard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ex_pccard_probe),
	DEVMETHOD(device_attach,	ex_pccard_attach),
	DEVMETHOD(device_detach,	ex_detach),

	{ 0, 0 }
};

static driver_t ex_pccard_driver = {
	"ex",
	ex_pccard_methods,
	sizeof(struct ex_softc),
};

DRIVER_MODULE(ex, pccard, ex_pccard_driver, ex_devclass, 0, 0);
