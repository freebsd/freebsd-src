/*
 * Copyright (c) 1994 Herb Peyerl <hpeyerl@novatel.ca>
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Herb Peyerl.
 * 4. The name of Herb Peyerl may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *
 * $FreeBSD$
 */

/*
 * Pccard support for 3C589 by:
 *		HAMADA Naoki
 *		nao@tom-yam.or.jp
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>
 
#include <net/ethernet.h>
#include <net/if.h> 
#include <net/if_arp.h>
#include <net/if_media.h>


#include <dev/ep/if_epreg.h>
#include <dev/ep/if_epvar.h>

#include <dev/pccard/pccardvar.h>
#include <dev/pccard/pccarddevs.h>
#include "card_if.h"

static const char *ep_pccard_identify(u_short id);

/*
 * Initialize the device - called from Slot manager.
 */
static int
ep_pccard_probe(device_t dev)
{
	struct ep_softc *	sc = device_get_softc(dev);
	struct ep_board *	epb = &sc->epb;
	const char *		desc;
	int			error;

	error = ep_alloc(dev);
	if (error)
		return error;

	/*
	 * XXX - Certain (newer?) 3Com cards need epb->cmd_off ==
	 * 2. Sadly, you need to have a correct cmd_off in order to
	 * identify the card.  So we have to hit it with both and
	 * cross our virtual fingers.  There's got to be a better way
	 * to do this.  jyoung@accessus.net 09/11/1999 
	 */

	epb->cmd_off = 0;
	epb->prod_id = get_e(sc, EEPROM_PROD_ID);
	if ((desc = ep_pccard_identify(epb->prod_id)) == NULL) {
		if (bootverbose) 
			device_printf(dev, "Pass 1 of 2 detection "
			    "failed (nonfatal) id 0x%x\n", epb->prod_id);
		epb->cmd_off = 2;
		epb->prod_id = get_e(sc, EEPROM_PROD_ID);
		if ((desc = ep_pccard_identify(epb->prod_id)) == NULL) {
			device_printf(dev, "Unit failed to come ready or "
			    "product ID unknown! (id 0x%x)\n", epb->prod_id);
			ep_free(dev);
			return (ENXIO);
		}
	}
	device_set_desc(dev, desc);

	/*
	 * For some reason the 3c574 needs this.
	 */
	ep_get_macaddr(sc, (u_char *)&sc->arpcom.ac_enaddr);

	ep_free(dev);
	return (0);
}

static const char *
ep_pccard_identify(u_short id)
{
	/* Determine device type and associated MII capabilities  */
	switch (id) {
	case 0x6055: /* 3C556 */
		return ("3Com 3C556");
	case 0x4057: /* 3C574 */
		return ("3Com 3C574");
	case 0x4b57: /* 3C574B */
		return ("3Com 3C574B, Megahertz 3CCFE574BT or "
		    "Fast Etherlink 3C574-TX");
	case 0x2b57: /* 3CXSH572BT */
		return ("3Com OfficeConnect 572BT");
	case 0x9058: /* 3C589 */
		return ("3Com Etherlink III 3C589");
	case 0x2056: /* 3C562/3C563 */
		return ("3Com 3C562D/3C563D");
	case 0x0010:	/* 3C1 */
		return ("3Com Megahertz C1");
	}
	return (NULL);
}

static int
ep_pccard_card_attach(struct ep_board *epb)
{
	/* Determine device type and associated MII capabilities  */
	switch (epb->prod_id) {
	case 0x6055: /* 3C556 */
	case 0x2b57: /* 3C572BT */
	case 0x4057: /* 3C574 */
	case 0x4b57: /* 3C574B */
	case 0x0010: /* 3C1 */
		epb->mii_trans = 1;
		return (1);
	case 0x2056: /* 3C562D/3C563D */
	case 0x9058: /* 3C589 */
		epb->mii_trans = 0;
		return (1);
	}
	return (0);
}

static int
ep_pccard_attach(device_t dev)
{
	struct ep_softc *	sc = device_get_softc(dev);
	int			error = 0;

	if ((error = ep_alloc(dev))) {
		device_printf(dev, "ep_alloc() failed! (%d)\n", error);
		goto bad;
	}

	sc->epb.cmd_off = 0;
	sc->epb.prod_id = get_e(sc, EEPROM_PROD_ID);
	if (!ep_pccard_card_attach(&sc->epb)) {
		sc->epb.cmd_off = 2;
		sc->epb.prod_id = get_e(sc, EEPROM_PROD_ID);
		sc->epb.res_cfg = get_e(sc, EEPROM_RESOURCE_CFG);
		if (!ep_pccard_card_attach(&sc->epb)) {
			device_printf(dev,
			    "Probe found ID, attach failed so ignore card!\n");
			error = ENXIO;
			goto bad;
		}
	}

	/* ROM size = 0, ROM base = 0 */
	/* For now, ignore AUTO SELECT feature of 3C589B and later. */
	outw(BASE + EP_W0_ADDRESS_CFG, get_e(sc, EEPROM_ADDR_CFG) & 0xc000);

	/* Fake IRQ must be 3 */
	outw(BASE + EP_W0_RESOURCE_CFG, (sc->epb.res_cfg & 0x0fff) | 0x3000);

	outw(BASE + EP_W0_PRODUCT_ID, sc->epb.prod_id);

	if (sc->epb.mii_trans) {
		/*
		 * turn on the MII transciever
		 */
		GO_WINDOW(3);
		outw(BASE + EP_W3_OPTIONS, 0x8040);
		DELAY(1000);
		outw(BASE + EP_W3_OPTIONS, 0xc040);
		outw(BASE + EP_COMMAND, RX_RESET);
		outw(BASE + EP_COMMAND, TX_RESET);
		while (inw(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS);
		DELAY(1000);
		outw(BASE + EP_W3_OPTIONS, 0x8040);
	} else {
		ep_get_media(sc);
	}

	if ((error = ep_attach(sc))) {
		device_printf(dev, "ep_attach() failed! (%d)\n", error);
		goto bad;
	}

	if ((error = bus_setup_intr(dev, sc->irq, INTR_TYPE_NET, ep_intr,
				    sc, &sc->ep_intrhand))) {
		device_printf(dev, "bus_setup_intr() failed! (%d)\n", error);
		goto bad;
	}

	return (0);
bad:
	ep_free(dev);
	return (error);
}

static int
ep_pccard_detach(device_t dev)
{
	struct ep_softc *sc = device_get_softc(dev);

	if (sc->gone) {
		device_printf(dev, "already unloaded\n");
		return (0);
	}
	sc->arpcom.ac_if.if_flags &= ~IFF_RUNNING; 
	ether_ifdetach(&sc->arpcom.ac_if);
	sc->gone = 1;
	bus_teardown_intr(dev, sc->irq, sc->ep_intrhand);
	ep_free(dev);
	return (0);
}

static const struct pccard_product ep_pccard_products[] = {
	PCMCIA_CARD(3COM, 3C1, 0),
	PCMCIA_CARD(3COM, 3C562, 0),
	PCMCIA_CARD(3COM, 3C574, 0),		/* ROADRUNNER */
	PCMCIA_CARD(3COM, 3C589, 0),
	PCMCIA_CARD(3COM, 3CCFEM556BI, 0),	/* ROADRUNNER */
	PCMCIA_CARD(3COM, 3CXEM556, 0),
	PCMCIA_CARD(3COM, 3CXEM556INT, 0),
	{ NULL }
};

static int
ep_pccard_match(device_t dev)
{
	const struct pccard_product *pp;

	if ((pp = pccard_product_lookup(dev, ep_pccard_products,
	    sizeof(ep_pccard_products[0]), NULL)) != NULL) {
		device_set_desc(dev, pp->pp_name);
		return 0;
	}
	return EIO;
}

static device_method_t ep_pccard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pccard_compat_probe),
	DEVMETHOD(device_attach,	pccard_compat_attach),
	DEVMETHOD(device_detach,	ep_pccard_detach),

	/* Card interface */
	DEVMETHOD(card_compat_match,	ep_pccard_match),
	DEVMETHOD(card_compat_probe,	ep_pccard_probe),
	DEVMETHOD(card_compat_attach,	ep_pccard_attach),

	{ 0, 0 }
};

static driver_t ep_pccard_driver = {
	"ep",
	ep_pccard_methods,
	sizeof(struct ep_softc),
};

extern devclass_t ep_devclass;

DRIVER_MODULE(ep, pccard, ep_pccard_driver, ep_devclass, 0, 0);
