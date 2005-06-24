/*-
 * Copyright (c) 1999 M. Warner Losh <imp@village.org> 
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
/*
 * Modifications for Megahertz X-Jack Ethernet Card (XJ-10BT)
 * 
 * Copyright (c) 1996 by Tatsumi Hosokawa <hosokawa@jp.FreeBSD.org>
 *                       BSD-nomads, Tokyo, Japan.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/systm.h>

#include <net/ethernet.h> 
#include <net/if.h> 
#include <net/if_arp.h>

#include <machine/bus.h>

#include <dev/pccard/pccardvar.h>
#include <dev/pccard/pccard_cis.h>
#include <dev/sn/if_snvar.h>

#include "card_if.h"
#include "pccarddevs.h"

static const struct pccard_product sn_pccard_products[] = {
	PCMCIA_CARD(DSPSI, XJACK),
	PCMCIA_CARD(NEWMEDIA, BASICS),
#if 0
	PCMCIA_CARD(SMC, 8020BT),
#endif
	{ NULL }
};

static int
sn_pccard_match(device_t dev)
{
	const struct pccard_product *pp;
	int		error;
	uint32_t	fcn = PCCARD_FUNCTION_UNSPEC;

	/* Make sure we're a network function */
	error = pccard_get_function(dev, &fcn);
	if (error != 0)
		return (error);
	if (fcn != PCCARD_FUNCTION_NETWORK)
		return (ENXIO);

	if ((pp = pccard_product_lookup(dev, sn_pccard_products,
	    sizeof(sn_pccard_products[0]), NULL)) != NULL) {
		if (pp->pp_name != NULL)
			device_set_desc(dev, pp->pp_name);
		return 0;
	}
	return EIO;
}

static int
sn_pccard_probe(device_t dev)
{
	int err;

	err = sn_probe(dev, 1);
	return (err);
}

static int
sn_pccard_ascii_enaddr(const char *str, u_char *enet)
{
        uint8_t digit;
	int i;
                
	memset(enet, 0, ETHER_ADDR_LEN);
         
	for (i = 0, digit = 0; i < (ETHER_ADDR_LEN * 2); i++) {
		if (str[i] >= '0' && str[i] <= '9')
			digit |= str[i] - '0';
		else if (str[i] >= 'a' && str[i] <= 'f')
			digit |= (str[i] - 'a') + 10;
		else if (str[i] >= 'A' && str[i] <= 'F')
			digit |= (str[i] - 'A') + 10;
		else {
			/* Bogus digit!! */
			return (0);
		}

		/* Compensate for ordering of digits. */
		if (i & 1) {
			enet[i >> 1] = digit;
			digit = 0;
		} else
			digit <<= 4;
	}

	return (1);
}

static int
sn_pccard_attach(device_t dev)
{
	struct sn_softc *sc = device_get_softc(dev);
	int i;
	u_char sum;
	u_char ether_addr[ETHER_ADDR_LEN];
	const char *cisstr;

	sc->pccard_enaddr = 0;
	pccard_get_ether(dev, ether_addr);
	for (i = 0, sum = 0; i < ETHER_ADDR_LEN; i++)
		sum |= ether_addr[i];
	if (sum == 0) {
		pccard_get_cis3_str(dev, &cisstr);
		if (strlen(cisstr) == ETHER_ADDR_LEN * 2)
		    sum = sn_pccard_ascii_enaddr(cisstr, ether_addr);
	}
	if (sum == 0) {
		pccard_get_cis4_str(dev, &cisstr);
		if (strlen(cisstr) == ETHER_ADDR_LEN * 2)
		    sum = sn_pccard_ascii_enaddr(cisstr, ether_addr);
	}
	if (sum) {
		sc->pccard_enaddr = 1;
		bcopy(ether_addr, IFP2ENADDR(sc->ifp), ETHER_ADDR_LEN);
	}
	return (sn_attach(dev));
}

static device_method_t sn_pccard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pccard_compat_probe),
	DEVMETHOD(device_attach,	pccard_compat_attach),
	DEVMETHOD(device_detach,	sn_detach),

	/* Card interface */
	DEVMETHOD(card_compat_match,	sn_pccard_match),
	DEVMETHOD(card_compat_probe,	sn_pccard_probe),
	DEVMETHOD(card_compat_attach,	sn_pccard_attach),

	{ 0, 0 }
};

static driver_t sn_pccard_driver = {
	"sn",
	sn_pccard_methods,
	sizeof(struct sn_softc),
};

extern devclass_t sn_devclass;

DRIVER_MODULE(sn, pccard, sn_pccard_driver, sn_devclass, 0, 0);
MODULE_DEPEND(sn, ether, 1, 1, 1);
