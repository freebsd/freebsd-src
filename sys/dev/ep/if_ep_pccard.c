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
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <machine/clock.h>

#include <dev/ep/if_epreg.h>
#include <dev/ep/if_epvar.h>

/* XXX should die XXX */
#include <sys/select.h>
#include <sys/module.h>
#include <pccard/cardinfo.h>
#include <pccard/slot.h>

static const char *ep_pccard_identify(u_short id);

/*
 * Initialize the device - called from Slot manager.
 */
static int
ep_pccard_probe(device_t dev)
{
	struct ep_board ep;
	struct ep_board *epb;
	u_long	port_start, port_count;
	struct ep_softc fake_softc;
	struct ep_softc *sc = &fake_softc;
	const char *desc;
	int error;
	const char *name;

	name = pccard_get_name(dev);
	printf("ep_pccard_probe: Does %s match?\n", name);
	if (strcmp(name, "ep"))
		return ENXIO;
	  
	epb = &ep;
	error = bus_get_resource(dev, SYS_RES_IOPORT, 0, &port_start,
	    &port_count);
	if (error != 0)
		return error;
	/* get_e() requires these. */
	bzero(sc, sizeof(*sc));
	sc->ep_io_addr = port_start;
	sc->unit = device_get_unit(dev);
	sc->epb = epb;

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
			    "failed (nonfatal)\n");
		epb->cmd_off = 2;
		epb->prod_id = get_e(sc, EEPROM_PROD_ID);
		if ((desc = ep_pccard_identify(epb->prod_id))) {
			device_printf(dev, "Unit failed to come ready or "
			    "product ID unknown! (id 0x%x)\n", epb->prod_id);
			return (ENXIO);
		}
	}

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
		return("3Com 3C574");
	case 0x4b57: /* 3C574B */
		return("3Com 3C574B, Megahertz 3CCFE574BT or "
		    "Fast Etherlink 3C574-TX");
	case 0x9058: /* 3C589 */
		return("3Com Etherlink III 3C589[B/C/D]");
	}
	return (0);
}

static int
ep_pccard_card_attach(struct ep_board *epb)
{
	/* Determine device type and associated MII capabilities  */
	switch (epb->prod_id) {
	case 0x6055: /* 3C556 */
		epb->mii_trans = 1;
		return (1);
	case 0x4057: /* 3C574 */
		epb->mii_trans = 1;
		return (1);
	case 0x4b57: /* 3C574B */
		epb->mii_trans = 1;
		return (1);
	case 0x9058: /* 3C589 */
		epb->mii_trans = 0;
		return (1);
	}
	return (0);
}

static int
ep_pccard_attach(device_t dev)
{
	struct ep_softc *sc = 0;
	struct ep_board *epb;
	struct resource *io = 0;
	struct resource *irq = 0;
	int unit = device_get_unit(dev);
	int i, rid;
	u_short config;

	rid = 0;
	io = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
				0, ~0, 1, RF_ACTIVE);
	if (!io) {
		device_printf(dev, "No I/O space?!\n");
		goto bad;
	}

	epb = &ep_board[ep_boards];

	epb->epb_addr = rman_get_start(io);
	epb->epb_used = 1;

	if ((sc = ep_alloc(unit, epb)) == 0)
		goto bad;
	ep_boards++;

	epb->cmd_off = 0;
	epb->prod_id = get_e(sc, EEPROM_PROD_ID);
	if (!ep_pccard_card_attach(epb)) {
		epb->cmd_off = 2;
		epb->prod_id = get_e(sc, EEPROM_PROD_ID);
		if (!ep_pccard_card_attach(epb)) {
			device_printf(dev,
			    "Probe found ID, attach failed so ignore card!\n");
			ep_free(sc);
			return (ENXIO);
		}
	}

	sc->ep_connectors = 0;
	epb->res_cfg = get_e(sc, EEPROM_RESOURCE_CFG);
	for (i = 0; i < 3; i++)
		sc->epb->eth_addr[i] = get_e(sc, EEPROM_NODE_ADDR_0 + i);

	config = inw(rman_get_start(io) + EP_W0_CONFIG_CTRL);
	if (config & IS_BNC) {
		sc->ep_connectors |= BNC;
	}
	if (config & IS_UTP) {
		sc->ep_connectors |= UTP;
	}
	if (!(sc->ep_connectors & 7))
		/* (Apparently) non-fatal */
		if (bootverbose) 
			device_printf(dev, "No connectors or MII.\n");

	sc->ep_connector = inw(BASE + EP_W0_ADDRESS_CFG) >> ACF_CONNECTOR_BITS;

	/* ROM size = 0, ROM base = 0 */
	/* For now, ignore AUTO SELECT feature of 3C589B and later. */
	outw(BASE + EP_W0_ADDRESS_CFG, get_e(sc, EEPROM_ADDR_CFG) & 0xc000);

	/* Fake IRQ must be 3 */
	outw(BASE + EP_W0_RESOURCE_CFG, (sc->epb->res_cfg & 0x0fff) | 0x3000);

	outw(BASE + EP_W0_PRODUCT_ID, sc->epb->prod_id);

	if (sc->epb->mii_trans) {
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
	}

	sc->irq = irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid,
				 0, ~0, 1, RF_ACTIVE);
	if (!irq) {
		device_printf(dev, "No irq?!\n");
		goto bad;
	}

	ep_attach(sc);
	if (bus_setup_intr(dev, irq, INTR_TYPE_NET, ep_intr, sc, &sc->ih)) {
		device_printf(dev, "cannot setup intr\n");
		goto bad;
	}
	sc->arpcom.ac_if.if_snd.ifq_maxlen = ifqmaxlen;

	return 0;
bad:
	if (io)
		bus_release_resource(dev, SYS_RES_IOPORT, 0, io);
	if (irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, irq);
	if (sc)
		ep_free(sc);
	return ENXIO;
}

static void
ep_pccard_detach(device_t dev)
{
	struct ep_softc *sc = device_get_softc(dev);

	printf("detach\n");

	if (sc->gone) {
		device_printf(dev, "already unloaded\n");
		return;
	}
	sc->arpcom.ac_if.if_flags &= ~IFF_RUNNING; 
	if_down(&sc->arpcom.ac_if);
	sc->gone = 1;
	bus_teardown_intr(dev, sc->irq, sc->ih);
	device_printf(dev, "unload\n");
}

static device_method_t ep_pccard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ep_pccard_probe),
	DEVMETHOD(device_attach,	ep_pccard_attach),
	DEVMETHOD(device_detach,	ep_pccard_detach),

	{ 0, 0 }
};

static driver_t ep_pccard_driver = {
	"ep",
	ep_pccard_methods,
	1,			/* unused */
};

extern devclass_t ep_devclass;

DRIVER_MODULE(ep, pccard, ep_pccard_driver, ep_devclass, 0, 0);
