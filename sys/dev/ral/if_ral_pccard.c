/*	$FreeBSD$	*/

/*-
 * Copyright (c) 2005
 *	Damien Bergamini <damien.bergamini@free.fr>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * CardBus front-end for the Ralink RT2500 driver.
 */

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/endian.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/pccard/pccardvar.h>
#include <dev/pccard/pccard_cis.h>

#include <dev/ral/if_ralrate.h>
#include <dev/ral/if_ralreg.h>
#include <dev/ral/if_ralvar.h>

#include "card_if.h"
#include "pccarddevs.h"

MODULE_DEPEND(ral, pccard, 1, 1, 1);
MODULE_DEPEND(ral, wlan, 1, 1, 1);

static const struct pccard_product ral_pccard_products[] = {
	PCMCIA_CARD(RALINK, RT2560, 0),

	{ NULL }
};

static int ral_pccard_match(device_t);
static int ral_pccard_probe(device_t);
static int ral_pccard_attach(device_t);

static device_method_t ral_pccard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pccard_compat_probe),
	DEVMETHOD(device_attach,	pccard_compat_attach),
	DEVMETHOD(device_detach,	ral_detach),
	DEVMETHOD(device_shutdown,	ral_shutdown),

	/* Card interface */
	DEVMETHOD(card_compat_match,	ral_pccard_match),
	DEVMETHOD(card_compat_probe,	ral_pccard_probe),
	DEVMETHOD(card_compat_attach,	ral_pccard_attach),

	{ 0, 0 }
};

static driver_t ral_pccard_driver = {
	"ral",
	ral_pccard_methods,
	sizeof (struct ral_softc)
};

DRIVER_MODULE(ral, pccard, ral_pccard_driver, ral_devclass, 0, 0);

static int
ral_pccard_match(device_t dev)
{
	const struct pccard_product *pp;

	if ((pp = pccard_product_lookup(dev, ral_pccard_products,
	    sizeof (struct pccard_product), NULL)) != NULL) {
		if (pp->pp_name != NULL)
			device_set_desc(dev, pp->pp_name);
		return 0;
	}
	return ENXIO;
}

static int
ral_pccard_probe(device_t dev)
{
	int error;

	error = ral_alloc(dev, 0);
	if (error != 0)
		return error;

	ral_free(dev);

	return 0;
}

static int
ral_pccard_attach(device_t dev)
{
	int error;

	error = ral_alloc(dev, 0);
	if (error != 0)
		return error;

	error = ral_attach(dev);
	if (error != 0)
		ral_free(dev);

	return error;
}
