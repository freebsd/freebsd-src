/*-
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
 */

/*
 * Pccard support for 3C589 by:
 *		HAMADA Naoki
 *		nao@tom-yam.or.jp
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

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_media.h>

#include <dev/ep/if_epreg.h>
#include <dev/ep/if_epvar.h>

#include <dev/pccard/pccardvar.h>

#include "card_if.h"
#include "pccarddevs.h"

static const char *ep_pccard_identify(u_short id);

/*
 * Initialize the device - called from Slot manager.
 */
static int
ep_pccard_probe(device_t dev)
{
	struct ep_softc *sc = device_get_softc(dev);
	struct ep_board *epb = &sc->epb;
	const char *desc;
	u_int16_t result;
	int error;

	error = ep_alloc(dev);
	if (error)
		return (error);

	/*
	 * It appears that the eeprom comes in two sizes.  There's
	 * a 512 byte eeprom and a 2k eeprom.  Bit 13 of the eeprom
	 * command register is supposed to contain the size of the
	 * eeprom.
	 */
	/*
	 * XXX - Certain (newer?) 3Com cards need epb->cmd_off ==
	 * 2. Sadly, you need to have a correct cmd_off in order to
	 * identify the card.  So we have to hit it with both and
	 * cross our virtual fingers.  There's got to be a better way
	 * to do this.  jyoung@accessus.net 09/11/1999
	 */

	epb->cmd_off = 0;

	/* XXX check return */
	error = get_e(sc, EEPROM_PROD_ID, &result);
	epb->prod_id = result;

	if ((desc = ep_pccard_identify(epb->prod_id)) == NULL) {
		if (bootverbose)
			device_printf(dev, "Pass 1 of 2 detection "
			    "failed (nonfatal) id 0x%x\n", epb->prod_id);
		epb->cmd_off = 2;
		/* XXX check return */
		error = get_e(sc, EEPROM_PROD_ID, &result);
		epb->prod_id = result;
		if ((desc = ep_pccard_identify(epb->prod_id)) == NULL) {
			device_printf(dev, "Unit failed to come ready or "
			    "product ID unknown! (id 0x%x)\n", epb->prod_id);
			ep_free(dev);
			return (ENXIO);
		}
	}
	device_set_desc(dev, desc);

	/*
	 * Newer cards supported by this device need to have their
	 * MAC address set.
	 */
	error = ep_get_macaddr(sc, (u_char *)&sc->arpcom.ac_enaddr);

	ep_free(dev);
	return (0);
}

static const char *
ep_pccard_identify(u_short id)
{
	/* Determine device type and associated MII capabilities  */
	switch (id) {
	case 0x6055:	/* 3C556 */
		return ("3Com 3C556");
	case 0x4057:		/* 3C574 */
		return ("3Com 3C574");
	case 0x4b57:		/* 3C574B */
		return ("3Com 3C574B, Megahertz 3CCFE574BT or "
		    "Fast Etherlink 3C574-TX");
	case 0x2b57:		/* 3CXSH572BT */
		return ("3Com OfficeConnect 572BT");
	case 0x9058:		/* 3C589 */
		return ("3Com Etherlink III 3C589");
	case 0x2056:		/* 3C562/3C563 */
		return ("3Com 3C562D/3C563D");
	case 0x0010:		/* 3C1 */
		return ("3Com Megahertz C1");
	case 0x0035:
		return ("3Com 3CCEM556");
	default:
		printf("Unknown ID: 0x%x\n", id);
		return (NULL);
	}
}

static int
ep_pccard_card_attach(struct ep_board * epb)
{
	/* Determine device type and associated MII capabilities  */
	switch (epb->prod_id) {
	case 0x6055:		/* 3C556 */
	case 0x2b57:		/* 3C572BT */
	case 0x4057:		/* 3C574 */
	case 0x4b57:		/* 3C574B */
		epb->mii_trans = 1;
		return (1);
	case 0x2056:		/* 3C562D/3C563D */
	case 0x9058:		/* 3C589 */
	case 0x0010:		/* 3C1 */
	case 0x0035:		/* 3C[XC]EM556 */
		epb->mii_trans = 0;
		return (1);
	default:
		return (0);
	}
}

static int
ep_pccard_attach(device_t dev)
{
	struct ep_softc *sc = device_get_softc(dev);
	u_int16_t result;
	int error = 0;

	if ((error = ep_alloc(dev))) {
		device_printf(dev, "ep_alloc() failed! (%d)\n", error);
		goto bad;
	}
	sc->epb.cmd_off = 0;

	/* XXX check return */
	error = get_e(sc, EEPROM_PROD_ID, &result);
	sc->epb.prod_id = result;

	if (!ep_pccard_card_attach(&sc->epb)) {
		sc->epb.cmd_off = 2;
		error = get_e(sc, EEPROM_PROD_ID, &result);
		sc->epb.prod_id = result;
		error = get_e(sc, EEPROM_RESOURCE_CFG, &result);
		sc->epb.res_cfg = result;
		if (!ep_pccard_card_attach(&sc->epb)) {
			device_printf(dev,
			    "Probe found ID, attach failed so ignore card!\n");
			error = ENXIO;
			goto bad;
		}
	}
	error = get_e(sc, EEPROM_ADDR_CFG, &result);

	/* ROM size = 0, ROM base = 0 */
	/* For now, ignore AUTO SELECT feature of 3C589B and later. */
	CSR_WRITE_2(sc, EP_W0_ADDRESS_CFG, result & 0xc000);

	/* 
	 * Fake IRQ must be 3 for 3C589 and 3C589B.  3C589D and newer
	 * ignore this value.  3C589C is unknown, as are the other
	 * cards supported by this driver, but it appears to never hurt
	 * and always helps.
	 */
	SET_IRQ(sc, 3);
	CSR_WRITE_2(sc, EP_W0_PRODUCT_ID, sc->epb.prod_id);

	if (sc->epb.mii_trans) {
		/*
		 * turn on the MII transciever
		 */
		GO_WINDOW(sc, 3);
		CSR_WRITE_2(sc, EP_W3_OPTIONS, 0x8040);
		DELAY(1000);
		CSR_WRITE_2(sc, EP_W3_OPTIONS, 0xc040);
		CSR_WRITE_2(sc, EP_COMMAND, RX_RESET);
		CSR_WRITE_2(sc, EP_COMMAND, TX_RESET);
		EP_BUSY_WAIT(sc);
		DELAY(1000);
		CSR_WRITE_2(sc, EP_W3_OPTIONS, 0x8040);
	} else
		ep_get_media(sc);

	if ((error = ep_attach(sc))) {
		device_printf(dev, "ep_attach() failed! (%d)\n", error);
		goto bad;
	}
	if ((error = bus_setup_intr(dev, sc->irq, INTR_TYPE_NET | INTR_MPSAFE, ep_intr,
		    sc, &sc->ep_intrhand))) {
		device_printf(dev, "bus_setup_intr() failed! (%d)\n", error);
		goto bad;
	}
	return (0);
bad:
	ep_free(dev);
	return (error);
}

static const struct pccard_product ep_pccard_products[] = {
	PCMCIA_CARD(3COM, 3C1, 0),
	PCMCIA_CARD(3COM, 3C562, 0),
	PCMCIA_CARD(3COM, 3C574, 0),	/* ROADRUNNER */
	PCMCIA_CARD(3COM, 3C589, 0),
	PCMCIA_CARD(3COM, 3CCFEM556BI, 0),	/* ROADRUNNER */
	PCMCIA_CARD(3COM, 3CXEM556, 0),
	PCMCIA_CARD(3COM, 3CXEM556INT, 0),
	{NULL}
};

static int
ep_pccard_match(device_t dev)
{
	const struct pccard_product *pp;

	if ((pp = pccard_product_lookup(dev, ep_pccard_products,
		    sizeof(ep_pccard_products[0]), NULL)) != NULL) {
		if (pp->pp_name != NULL)
			device_set_desc(dev, pp->pp_name);
		return 0;
	}
	return EIO;
}

static device_method_t ep_pccard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, pccard_compat_probe),
	DEVMETHOD(device_attach, pccard_compat_attach),
	DEVMETHOD(device_detach, ep_detach),

	/* Card interface */
	DEVMETHOD(card_compat_match, ep_pccard_match),
	DEVMETHOD(card_compat_probe, ep_pccard_probe),
	DEVMETHOD(card_compat_attach, ep_pccard_attach),

	{0, 0}
};

static driver_t ep_pccard_driver = {
	"ep",
	ep_pccard_methods,
	sizeof(struct ep_softc),
};

extern devclass_t ep_devclass;

DRIVER_MODULE(ep, pccard, ep_pccard_driver, ep_devclass, 0, 0);
