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

#include <net/ethernet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <machine/clock.h>

#include <sys/select.h>
#include <sys/module.h>
#include <pccard/cardinfo.h>
#include <pccard/slot.h>

#include <dev/ep/if_epreg.h>
#include <dev/ep/if_epvar.h>

/*
 * PC-Card (PCMCIA) specific code.
 */
static int	ep_pccard_init		(struct pccard_devinfo *);
static int	ep_pccard_attach	(struct pccard_devinfo *);
static void	ep_pccard_unload	(struct pccard_devinfo *);
static int	ep_pccard_intr		(struct pccard_devinfo *);
static int	ep_pccard_identify	(struct ep_board *epb, int unit); 

PCCARD_MODULE(ep, ep_pccard_init, ep_pccard_unload, ep_pccard_intr, 0, net_imask);

/*
 * Initialize the device - called from Slot manager.
 */
static int
ep_pccard_init(devi)
    struct pccard_devinfo *devi;
{
    struct isa_device *is = &devi->isahd;
    struct ep_softc *sc = ep_softc[is->id_unit];
    struct ep_board *epb;
    int i;

    epb = &ep_board[is->id_unit];

    if (sc == 0) {
	if ((sc = ep_alloc(is->id_unit, epb)) == 0) {
	    return (ENXIO);
	}
	ep_unit++;
    }

    /* get_e() requires these. */
    sc->ep_io_addr = is->id_iobase;
    sc->unit = is->id_unit;
    epb->epb_addr = is->id_iobase;
    epb->epb_used = 1;

    /*
     * XXX - Certain (newer?) 3Com cards need epb->cmd_off == 2. Sadly,
     * you need to have a correct cmd_off in order to identify the card. 
     * So we have to hit it with both and cross our virtual fingers. There's
     * got to be a better way to do this. jyoung@accessus.net 09/11/1999
     */

    epb->cmd_off = 0;
    epb->prod_id = get_e(sc, EEPROM_PROD_ID);
    if (!ep_pccard_identify(epb, is->id_unit)) {
	if (bootverbose) printf("ep%d: Pass 1 of 2 detection failed (nonfatal)\n", is->id_unit);
	epb->cmd_off = 2;
	epb->prod_id = get_e(sc, EEPROM_PROD_ID);
	if (!ep_pccard_identify(epb, is->id_unit)) {
	    if (bootverbose) printf("ep%d: Pass 2 of 2 detection failed (fatal!)\n", is->id_unit);
	    printf("ep%d: Unit failed to come ready or product ID unknown! (id 0x%x)\n", is->id_unit, epb->prod_id);
	    return (ENXIO);
	}
    }

    epb->res_cfg = get_e(sc, EEPROM_RESOURCE_CFG);
    for (i = 0; i < 3; i++)
	sc->epb->eth_addr[i] = get_e(sc, EEPROM_NODE_ADDR_0 + i);

    if (ep_pccard_attach(devi) == 0)
	return (ENXIO);

    sc->arpcom.ac_if.if_snd.ifq_maxlen = ifqmaxlen;
    return (0);
}

static int
ep_pccard_identify(epb, unit)
    struct ep_board *epb;
    int unit;
{
    /* Determine device type and associated MII capabilities  */
    switch (epb->prod_id) {
	case 0x6055: /* 3C556 */
	    if (bootverbose) printf("ep%d: 3Com 3C556\n", unit);
	    epb->mii_trans = 1;
	    return (1);
	    break; /* NOTREACHED */
	case 0x4057: /* 3C574 */
	    if (bootverbose) printf("ep%d: 3Com 3C574\n", unit);
	    epb->mii_trans = 1;
	    return (1);
	    break; /* NOTREACHED */
	case 0x4b57: /* 3C574B */
	    if (bootverbose) printf("ep%d: 3Com 3C574B, Megahertz 3CCFE574BT or Fast Etherlink 3C574-TX\n", unit);
	    epb->mii_trans = 1;
	    return (1);
	    break; /* NOTREACHED */
	case 0x9058: /* 3C589 */
	    if (bootverbose) printf("ep%d: 3Com Etherlink III 3C589[B/C/D]\n", unit);
	    epb->mii_trans = 0;
	    return (1);
	    break; /* NOTREACHED */
    }
    return (0);
}

static int
ep_pccard_attach(devi)
    struct pccard_devinfo *devi;
{
    struct isa_device *is = &devi->isahd;
    struct ep_softc *sc = ep_softc[is->id_unit];
    u_short config;

    sc->ep_connectors = 0;
    config = inw(IS_BASE + EP_W0_CONFIG_CTRL);
    if (config & IS_BNC) {
	sc->ep_connectors |= BNC;
    }
    if (config & IS_UTP) {
	sc->ep_connectors |= UTP;
    }
    if (!(sc->ep_connectors & 7))
	/* (Apparently) non-fatal */
	if(bootverbose) printf("ep%d: No connectors or MII.\n", is->id_unit);

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

    ep_attach(sc);

    return 1;
}

static void
ep_pccard_unload(devi)
    struct pccard_devinfo *devi;
{
    struct ep_softc *sc = ep_softc[devi->isahd.id_unit];

    if (sc->gone) {
        printf("ep%d: already unloaded\n", devi->isahd.id_unit);
	return;
    }
    sc->arpcom.ac_if.if_flags &= ~IFF_RUNNING;
    sc->gone = 1;
    printf("ep%d: unload\n", devi->isahd.id_unit);
}

/*
 * card_intr - Shared interrupt called from
 * front end of PC-Card handler.
 */
static int
ep_pccard_intr(devi)
    struct pccard_devinfo *devi;
{
    struct ep_softc *sc = ep_softc[devi->isahd.id_unit];

    if (sc->gone) {
        return;
    }

    ep_intr((void *)sc);

    return(1);
}
