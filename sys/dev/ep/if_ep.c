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
 *	if_ep.c,v 1.19 1995/01/24 20:53:45 davidg Exp
 */

/*
 *	Modified from the FreeBSD 1.1.5.1 version by:
 *		 	Andres Vega Garcia
 *			INRIA - Sophia Antipolis, France
 *			avega@sophia.inria.fr
 */

/*
 * $FreeBSD$
 *
 *  Promiscuous mode added and interrupt logic slightly changed
 *  to reduce the number of adapter failures. Transceiver select
 *  logic changed to use value from EEPROM. Autoconfiguration
 *  features added.
 *  Done by:
 *          Serge Babkin
 *          Chelindbank (Chelyabinsk, Russia)
 *          babkin@hq.icb.chel.su
 */

/*
 * Pccard support for 3C589 by:
 *		HAMADA Naoki
 *		nao@tom-yam.or.jp
 */

/*
 * MAINTAINER: Matthew N. Dodd <winter@jurai.net>
 *                             <mdodd@FreeBSD.org>
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h> 

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_media.h> 
#include <net/ethernet.h>
#include <net/bpf.h>


#include <dev/ep/if_epreg.h>
#include <dev/ep/if_epvar.h>

/* Exported variables */
devclass_t ep_devclass;

#if 0
static char *	ep_conn_type[] = {"UTP", "AUI", "???", "BNC"};
static int	if_media2ep_media[] = { 0, 0, 0, UTP, BNC, AUI };
#endif

static int	ep_media2if_media[] =
	{ IFM_10_T, IFM_10_5, IFM_NONE, IFM_10_2, IFM_NONE };

/* if functions */
static void	ep_if_init	(void *);
static int	ep_if_ioctl	(struct ifnet *, u_long, caddr_t);
static void	ep_if_start	(struct ifnet *);
static void	ep_if_watchdog	(struct ifnet *);

/* if_media functions */
static int	ep_ifmedia_upd	(struct ifnet *);
static void	ep_ifmedia_sts	(struct ifnet *, struct ifmediareq *);

static void	epstop		(struct ep_softc *);
static void	epread		(struct ep_softc *);
static int	eeprom_rdy	(struct ep_softc *);

#define EP_FTST(sc, f)	(sc->stat &   (f))
#define EP_FSET(sc, f)	(sc->stat |=  (f))
#define EP_FRST(sc, f)	(sc->stat &= ~(f))

static int
eeprom_rdy(sc)
    struct ep_softc *sc;
{
    int i;

    for (i = 0; is_eeprom_busy(BASE) && i < MAX_EEPROMBUSY; i++) {
	DELAY(100);
    }
    if (i >= MAX_EEPROMBUSY) {
	printf("ep%d: eeprom failed to come ready.\n", sc->unit);
	return (ENXIO);
    }
    return (0);
}

/*
 * get_e: gets a 16 bits word from the EEPROM. we must have set the window
 * before
 */
int
get_e(sc, offset, result)
	struct ep_softc *sc;
	u_int16_t offset;
	u_int16_t *result;
{

	if (eeprom_rdy(sc))
		return (ENXIO);
	outw(BASE + EP_W0_EEPROM_COMMAND,
	     (EEPROM_CMD_RD << sc->epb.cmd_off) | offset);
	if (eeprom_rdy(sc))
		return (ENXIO);
	(*result) = inw(BASE + EP_W0_EEPROM_DATA);

	return (0);
}

int
ep_get_macaddr(sc, addr)
	struct ep_softc	*	sc;
	u_char *		addr;
{
	int			i;
	u_int16_t		result;
	int			error;
	u_int16_t * 		macaddr;

	macaddr = (u_int16_t *)addr;

	GO_WINDOW(0);
        for(i = EEPROM_NODE_ADDR_0; i <= EEPROM_NODE_ADDR_2; i++) {
		error = get_e(sc, i, &result);
		if (error)
			return (error);
		macaddr[i] = htons(result);
        }

	return (0);
}

int
ep_alloc(device_t dev)
{
	struct ep_softc	*	sc = device_get_softc(dev);
	int			rid;
	int			error = 0;
	u_int16_t		result;

        rid = 0;
        sc->iobase = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
                                        0, ~0, 1, RF_ACTIVE);
        if (!sc->iobase) {
                device_printf(dev, "No I/O space?!\n");
		error = ENXIO;
                goto bad;
        }

        rid = 0;
        sc->irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid,
                                     0, ~0, 1, RF_ACTIVE);
        if (!sc->irq) {
                device_printf(dev, "No irq?!\n");
		error = ENXIO;
                goto bad;
        }

        sc->dev = dev;
        sc->unit = device_get_unit(dev);
        sc->stat = 0;   /* 16 bit access */

        sc->ep_io_addr = rman_get_start(sc->iobase);

        sc->ep_btag = rman_get_bustag(sc->iobase);
        sc->ep_bhandle = rman_get_bushandle(sc->iobase);

	sc->ep_connectors = 0;
	sc->ep_connector = 0;

        GO_WINDOW(0);
	sc->epb.cmd_off = 0;

	error = get_e(sc, EEPROM_PROD_ID, &result);
	if (error)
		goto bad;
	sc->epb.prod_id = result;

	error = get_e(sc, EEPROM_RESOURCE_CFG, &result);
	if (error)
		goto bad;
	sc->epb.res_cfg = result;

bad:
	return (error);
}

void
ep_get_media(sc)
	struct ep_softc	*	sc;
{
	u_int16_t		config;
	
        GO_WINDOW(0);
        config = inw(BASE + EP_W0_CONFIG_CTRL);
        if (config & IS_AUI)
                sc->ep_connectors |= AUI;
        if (config & IS_BNC)
                sc->ep_connectors |= BNC;
        if (config & IS_UTP)
                sc->ep_connectors |= UTP;

        if (!(sc->ep_connectors & 7)) {
		if (bootverbose)
                	device_printf(sc->dev, "no connectors!\n");
        }

	/*
	 * This works for most of the cards so we'll do it here.
	 * The cards that require something different can override
	 * this later on.
	 */
	sc->ep_connector = inw(BASE + EP_W0_ADDRESS_CFG) >> ACF_CONNECTOR_BITS;

	return;
}

void
ep_free(device_t dev)
{
	struct ep_softc	*	sc = device_get_softc(dev);

	if (sc->ep_intrhand)
		bus_teardown_intr(dev, sc->irq, sc->ep_intrhand);
	if (sc->iobase)
		bus_release_resource(dev, SYS_RES_IOPORT, 0, sc->iobase);
	if (sc->irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irq);

	return;
}
	
int
ep_attach(sc)
	struct ep_softc *	sc;
{
	struct ifnet *		ifp = NULL;
	struct ifmedia *	ifm = NULL;
	u_short *		p;
	int			i;
	int			attached;
	int			error;

	sc->gone = 0;

	error = ep_get_macaddr(sc, (u_char *)&sc->arpcom.ac_enaddr);
	if (error) {
		device_printf(sc->dev, "Unable to retrieve Ethernet address!\n");
		return (ENXIO);
	}

	/*
	 * Setup the station address
	 */
	p = (u_short *)&sc->arpcom.ac_enaddr;
	GO_WINDOW(2);
	for (i = 0; i < 3; i++) {
		outw(BASE + EP_W2_ADDR_0 + (i * 2), ntohs(p[i]));
	}

	device_printf(sc->dev, "Ethernet address %6D\n",
			sc->arpcom.ac_enaddr, ":");
  
	ifp = &sc->arpcom.ac_if;
	attached = (ifp->if_softc != 0);

	ifp->if_softc = sc;
	ifp->if_unit = sc->unit;
	ifp->if_name = "ep";
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_output = ether_output;
	ifp->if_start = ep_if_start;
	ifp->if_ioctl = ep_if_ioctl;
	ifp->if_watchdog = ep_if_watchdog;
	ifp->if_init = ep_if_init;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;

	if (!sc->epb.mii_trans) {
		ifmedia_init(&sc->ifmedia, 0, ep_ifmedia_upd, ep_ifmedia_sts);

		if (sc->ep_connectors & AUI)
			ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_5, 0, NULL);
		if (sc->ep_connectors & UTP)
			ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_T, 0, NULL);
		if (sc->ep_connectors & BNC)
			ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_2, 0, NULL);
		if (!sc->ep_connectors)
			ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_NONE, 0, NULL);
	
		ifmedia_set(&sc->ifmedia, IFM_ETHER|ep_media2if_media[sc->ep_connector]);
	
		ifm = &sc->ifmedia;
		ifm->ifm_media = ifm->ifm_cur->ifm_media;
		ep_ifmedia_upd(ifp);
	}

	if (!attached)
		ether_ifattach(ifp, sc->arpcom.ac_enaddr);

#ifdef EP_LOCAL_STATS
	sc->rx_no_first = sc->rx_no_mbuf = sc->rx_bpf_disc =
		sc->rx_overrunf = sc->rx_overrunl = sc->tx_underrun = 0;
#endif
	EP_FSET(sc, F_RX_FIRST);
	sc->top = sc->mcur = 0;

	epstop(sc);

	return 0;
}

int
ep_detach(device_t dev)
{
	struct ep_softc *sc;
	struct ifnet *ifp;

	sc = device_get_softc(dev);
	ifp = &sc->arpcom.ac_if;

	if (sc->gone) {
		device_printf(dev, "already unloaded\n");
		return (0);
	}

	epstop(sc);

	ifp->if_flags &= ~IFF_RUNNING;
	ether_ifdetach(ifp);

	sc->gone = 1;
	ep_free(dev);

	return (0);
}

/*
 * The order in here seems important. Otherwise we may not receive
 * interrupts. ?!
 */
static void
ep_if_init(xsc)
    void *xsc;
{
    struct ep_softc *sc = xsc;
    register struct ifnet *ifp = &sc->arpcom.ac_if;
    int s, i;

    if (sc->gone)
	return;

	/*
    if (ifp->if_addrlist == (struct ifaddr *) 0)
	return;
	*/

    s = splimp();
    while (inw(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS);

    GO_WINDOW(0);
    outw(BASE + EP_COMMAND, STOP_TRANSCEIVER);
    GO_WINDOW(4);
    outw(BASE + EP_W4_MEDIA_TYPE, DISABLE_UTP);
    GO_WINDOW(0);

    /* Disable the card */
    outw(BASE + EP_W0_CONFIG_CTRL, 0);

    /* Enable the card */
    outw(BASE + EP_W0_CONFIG_CTRL, ENABLE_DRQ_IRQ);

    GO_WINDOW(2);

    /* Reload the ether_addr. */
    for (i = 0; i < 6; i++)
	outb(BASE + EP_W2_ADDR_0 + i, sc->arpcom.ac_enaddr[i]);

    outw(BASE + EP_COMMAND, RX_RESET);
    outw(BASE + EP_COMMAND, TX_RESET);
    while (inw(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS);

    /* Window 1 is operating window */
    GO_WINDOW(1);
    for (i = 0; i < 31; i++)
	inb(BASE + EP_W1_TX_STATUS);

    /* get rid of stray intr's */
    outw(BASE + EP_COMMAND, ACK_INTR | 0xff);

    outw(BASE + EP_COMMAND, SET_RD_0_MASK | S_5_INTS);

    outw(BASE + EP_COMMAND, SET_INTR_MASK | S_5_INTS);

    if (ifp->if_flags & IFF_PROMISC)
	outw(BASE + EP_COMMAND, SET_RX_FILTER | FIL_INDIVIDUAL |
	 FIL_GROUP | FIL_BRDCST | FIL_ALL);
    else
	outw(BASE + EP_COMMAND, SET_RX_FILTER | FIL_INDIVIDUAL |
	 FIL_GROUP | FIL_BRDCST);

    if (!sc->epb.mii_trans) {
	ep_ifmedia_upd(ifp);
    }

    outw(BASE + EP_COMMAND, RX_ENABLE);
    outw(BASE + EP_COMMAND, TX_ENABLE);

    ifp->if_flags |= IFF_RUNNING;
    ifp->if_flags &= ~IFF_OACTIVE;	/* just in case */

#ifdef EP_LOCAL_STATS
    sc->rx_no_first = sc->rx_no_mbuf =
	sc->rx_overrunf = sc->rx_overrunl = sc->tx_underrun = 0;
#endif
    EP_FSET(sc, F_RX_FIRST);
    if (sc->top) {
	m_freem(sc->top);
	sc->top = sc->mcur = 0;
    }
    outw(BASE + EP_COMMAND, SET_RX_EARLY_THRESH | RX_INIT_EARLY_THRESH);
    outw(BASE + EP_COMMAND, SET_TX_START_THRESH | 16);

    /*
     * Store up a bunch of mbuf's for use later. (MAX_MBS). First we free up
     * any that we had in case we're being called from intr or somewhere
     * else.
     */

    GO_WINDOW(1);
    ep_if_start(ifp);

    splx(s);
}

static void
ep_if_start(ifp)
    struct ifnet *ifp;
{
    struct ep_softc *sc;
    u_int len;
    struct mbuf *m, *m0;
    int s, pad;

    sc = ifp->if_softc;
    if (sc->gone)
	return;

    while (inw(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS);
    if (ifp->if_flags & IFF_OACTIVE)
	return;

startagain:
    /* Sneak a peek at the next packet */
    IF_DEQUEUE(&ifp->if_snd, m0);
    if (m0 == NULL)
	return;
    for (len = 0, m = m0; m != NULL; m = m->m_next)
	len += m->m_len;

    pad = (4 - len) & 3;

    /*
     * The 3c509 automatically pads short packets to minimum ethernet length,
     * but we drop packets that are too large. Perhaps we should truncate
     * them instead?
     */
    if (len + pad > ETHER_MAX_LEN) {
	/* packet is obviously too large: toss it */
	ifp->if_oerrors++;
	m_freem(m0);
	goto readcheck;
    }
    if (inw(BASE + EP_W1_FREE_TX) < len + pad + 4) {
	/* no room in FIFO */
	outw(BASE + EP_COMMAND, SET_TX_AVAIL_THRESH | (len + pad + 4));
	/* make sure */
	if (inw(BASE + EP_W1_FREE_TX) < len + pad + 4) {
	    ifp->if_flags |= IFF_OACTIVE;
	    IF_PREPEND(&ifp->if_snd, m0);
	    return;
	}
    } else {
	outw(BASE + EP_COMMAND, SET_TX_AVAIL_THRESH | EP_THRESH_DISABLE);
    }

    s = splhigh();

    outw(BASE + EP_W1_TX_PIO_WR_1, len); 
    outw(BASE + EP_W1_TX_PIO_WR_1, 0x0);	/* Second dword meaningless */

    if (EP_FTST(sc, F_ACCESS_32_BITS)) {
        for (m = m0; m != NULL; m = m->m_next) {
	    if (m->m_len > 3)
	    	outsl(BASE + EP_W1_TX_PIO_WR_1,
			mtod(m, caddr_t), m->m_len / 4);
	    if (m->m_len & 3)
		outsb(BASE + EP_W1_TX_PIO_WR_1,
			mtod(m, caddr_t) + (m->m_len & (~3)), m->m_len & 3);
	}
    } else {
        for (m = m0; m != NULL; m = m->m_next) {
	    if (m->m_len > 1)
	    	outsw(BASE + EP_W1_TX_PIO_WR_1,
			mtod(m, caddr_t), m->m_len / 2);
	    if (m->m_len & 1)
		outb(BASE + EP_W1_TX_PIO_WR_1,
			*(mtod(m, caddr_t) + m->m_len - 1));
	}
    }

    while (pad--)
	outb(BASE + EP_W1_TX_PIO_WR_1, 0);	/* Padding */

    splx(s);

    BPF_MTAP(ifp, m0);

    ifp->if_timer = 2;
    ifp->if_opackets++;
    m_freem(m0);

    /*
     * Is another packet coming in? We don't want to overflow the tiny RX
     * fifo.
     */
readcheck:
    if (inw(BASE + EP_W1_RX_STATUS) & RX_BYTES_MASK) {
	/*
	 * we check if we have packets left, in that case we prepare to come
	 * back later
	 */
	if (ifp->if_snd.ifq_head)
	    outw(BASE + EP_COMMAND, SET_TX_AVAIL_THRESH | 8);
	return;
    }
    goto startagain;
}

void
ep_intr(arg)
    void *arg;
{
    struct ep_softc *sc;
    register int status;
    struct ifnet *ifp;
    int x;

    x = splbio();

    sc = (struct ep_softc *)arg;

    /*
     * quick fix: Try to detect an interrupt when the card goes away.
     */
    if (sc->gone || inw(BASE + EP_STATUS) == 0xffff) {
	    splx(x);
	    return;
    }

    ifp = &sc->arpcom.ac_if;

    outw(BASE + EP_COMMAND, SET_INTR_MASK); /* disable all Ints */

rescan:

    while ((status = inw(BASE + EP_STATUS)) & S_5_INTS) {

	/* first acknowledge all interrupt sources */
	outw(BASE + EP_COMMAND, ACK_INTR | (status & S_MASK));

	if (status & (S_RX_COMPLETE | S_RX_EARLY))
	    epread(sc);
	if (status & S_TX_AVAIL) {
	    /* we need ACK */
	    ifp->if_timer = 0;
	    ifp->if_flags &= ~IFF_OACTIVE;
	    GO_WINDOW(1);
	    inw(BASE + EP_W1_FREE_TX);
	    ep_if_start(ifp);
	}
	if (status & S_CARD_FAILURE) {
	    ifp->if_timer = 0;
#ifdef EP_LOCAL_STATS
	    printf("\nep%d:\n\tStatus: %x\n", sc->unit, status);
	    GO_WINDOW(4);
	    printf("\tFIFO Diagnostic: %x\n", inw(BASE + EP_W4_FIFO_DIAG));
	    printf("\tStat: %x\n", sc->stat);
	    printf("\tIpackets=%d, Opackets=%d\n",
		ifp->if_ipackets, ifp->if_opackets);
	    printf("\tNOF=%d, NOMB=%d, RXOF=%d, RXOL=%d, TXU=%d\n",
		   sc->rx_no_first, sc->rx_no_mbuf, sc->rx_overrunf,
		   sc->rx_overrunl, sc->tx_underrun);
#else

#ifdef DIAGNOSTIC
	    printf("ep%d: Status: %x (input buffer overflow)\n", sc->unit, status);
#else
	    ++ifp->if_ierrors;
#endif

#endif
	    ep_if_init(sc);
	    splx(x);
	    return;
	}
	if (status & S_TX_COMPLETE) {
	    ifp->if_timer = 0;
	    /* we  need ACK. we do it at the end */
	    /*
	     * We need to read TX_STATUS until we get a 0 status in order to
	     * turn off the interrupt flag.
	     */
	    while ((status = inb(BASE + EP_W1_TX_STATUS)) & TXS_COMPLETE) {
		if (status & TXS_SUCCES_INTR_REQ);
		else if (status & (TXS_UNDERRUN | TXS_JABBER | TXS_MAX_COLLISION)) {
		    outw(BASE + EP_COMMAND, TX_RESET);
		    if (status & TXS_UNDERRUN) {
#ifdef EP_LOCAL_STATS
			sc->tx_underrun++;
#endif
		    } else {
			if (status & TXS_JABBER);
			else	/* TXS_MAX_COLLISION - we shouldn't get here */
			    ++ifp->if_collisions;
		    }
		    ++ifp->if_oerrors;
		    outw(BASE + EP_COMMAND, TX_ENABLE);
		    /*
		     * To have a tx_avail_int but giving the chance to the
		     * Reception
		     */
		    if (ifp->if_snd.ifq_head) {
			outw(BASE + EP_COMMAND, SET_TX_AVAIL_THRESH | 8);
		    }
		}
		outb(BASE + EP_W1_TX_STATUS, 0x0);	/* pops up the next
							 * status */
	    }			/* while */
	    ifp->if_flags &= ~IFF_OACTIVE;
	    GO_WINDOW(1);
	    inw(BASE + EP_W1_FREE_TX);
	    ep_if_start(ifp);
	}			/* end TX_COMPLETE */
    }

    outw(BASE + EP_COMMAND, C_INTR_LATCH);	/* ACK int Latch */

    if ((status = inw(BASE + EP_STATUS)) & S_5_INTS)
	goto rescan;

    /* re-enable Ints */
    outw(BASE + EP_COMMAND, SET_INTR_MASK | S_5_INTS);

    splx(x);
}

static void
epread(sc)
    register struct ep_softc *sc;
{
    struct mbuf *top, *mcur, *m;
    struct ifnet *ifp;
    int lenthisone;

    short rx_fifo2, status;
    register short rx_fifo;

    ifp = &sc->arpcom.ac_if;
    status = inw(BASE + EP_W1_RX_STATUS);

read_again:

    if (status & ERR_RX) {
	++ifp->if_ierrors;
	if (status & ERR_RX_OVERRUN) {
	    /*
	     * we can think the rx latency is actually greather than we
	     * expect
	     */
#ifdef EP_LOCAL_STATS
	    if (EP_FTST(sc, F_RX_FIRST))
		sc->rx_overrunf++;
	    else
		sc->rx_overrunl++;
#endif
	}
	goto out;
    }
    rx_fifo = rx_fifo2 = status & RX_BYTES_MASK;

    if (EP_FTST(sc, F_RX_FIRST)) {
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (!m)
	    goto out;
	if (rx_fifo >= MINCLSIZE)
	    MCLGET(m, M_DONTWAIT);
	sc->top = sc->mcur = top = m;
#define EROUND  ((sizeof(struct ether_header) + 3) & ~3)
#define EOFF    (EROUND - sizeof(struct ether_header))
	top->m_data += EOFF;

	/* Read what should be the header. */
	insw(BASE + EP_W1_RX_PIO_RD_1,
	     mtod(top, caddr_t), sizeof(struct ether_header) / 2);
	top->m_len = sizeof(struct ether_header);
	rx_fifo -= sizeof(struct ether_header);
	sc->cur_len = rx_fifo2;
    } else {
	/* come here if we didn't have a complete packet last time */
	top = sc->top;
	m = sc->mcur;
	sc->cur_len += rx_fifo2;
    }

    /* Reads what is left in the RX FIFO */
    while (rx_fifo > 0) {
	lenthisone = min(rx_fifo, M_TRAILINGSPACE(m));
	if (lenthisone == 0) {	/* no room in this one */
	    mcur = m;
	    MGET(m, M_DONTWAIT, MT_DATA);
	    if (!m)
		goto out;
	    if (rx_fifo >= MINCLSIZE)
		MCLGET(m, M_DONTWAIT);
	    m->m_len = 0;
	    mcur->m_next = m;
	    lenthisone = min(rx_fifo, M_TRAILINGSPACE(m));
	}
	if (EP_FTST(sc, F_ACCESS_32_BITS)) { /* default for EISA configured cards*/
	    insl(BASE + EP_W1_RX_PIO_RD_1, mtod(m, caddr_t) + m->m_len,
		 lenthisone / 4);
	    m->m_len += (lenthisone & ~3);
	    if (lenthisone & 3)
		insb(BASE + EP_W1_RX_PIO_RD_1,
		     mtod(m, caddr_t) + m->m_len,
		     lenthisone & 3);
	    m->m_len += (lenthisone & 3);
	} else {
	    insw(BASE + EP_W1_RX_PIO_RD_1, mtod(m, caddr_t) + m->m_len,
		 lenthisone / 2);
	    m->m_len += lenthisone;
	    if (lenthisone & 1)
		*(mtod(m, caddr_t) + m->m_len - 1) = inb(BASE + EP_W1_RX_PIO_RD_1);
	}
	rx_fifo -= lenthisone;
    }

    if (status & ERR_RX_INCOMPLETE) {	/* we haven't received the complete
					 * packet */
	sc->mcur = m;
#ifdef EP_LOCAL_STATS
	sc->rx_no_first++;	/* to know how often we come here */
#endif
	EP_FRST(sc, F_RX_FIRST);
	if (!((status = inw(BASE + EP_W1_RX_STATUS)) & ERR_RX_INCOMPLETE)) {
	    /* we see if by now, the packet has completly arrived */
	    goto read_again;
	}
	outw(BASE + EP_COMMAND, SET_RX_EARLY_THRESH | RX_NEXT_EARLY_THRESH);
	return;
    }
    outw(BASE + EP_COMMAND, RX_DISCARD_TOP_PACK);
    ++ifp->if_ipackets;
    EP_FSET(sc, F_RX_FIRST);
    top->m_pkthdr.rcvif = &sc->arpcom.ac_if;
    top->m_pkthdr.len = sc->cur_len;

    (*ifp->if_input)(ifp, top);
    sc->top = 0;
    while (inw(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS);
    outw(BASE + EP_COMMAND, SET_RX_EARLY_THRESH | RX_INIT_EARLY_THRESH);
    return;

out:
    outw(BASE + EP_COMMAND, RX_DISCARD_TOP_PACK);
    if (sc->top) {
	m_freem(sc->top);
	sc->top = 0;
#ifdef EP_LOCAL_STATS
	sc->rx_no_mbuf++;
#endif
    }
    EP_FSET(sc, F_RX_FIRST);
    while (inw(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS);
    outw(BASE + EP_COMMAND, SET_RX_EARLY_THRESH | RX_INIT_EARLY_THRESH);
}

static int 
ep_ifmedia_upd(ifp)
	struct ifnet *		ifp;
{
	struct ep_softc *	sc = ifp->if_softc;
	int			i = 0, j;

	GO_WINDOW(0);
	outw(BASE + EP_COMMAND, STOP_TRANSCEIVER);
	GO_WINDOW(4);
	outw(BASE + EP_W4_MEDIA_TYPE, DISABLE_UTP);
	GO_WINDOW(0);

	switch (IFM_SUBTYPE(sc->ifmedia.ifm_media)) {
		case IFM_10_T:
			if (sc->ep_connectors & UTP) {
				i = ACF_CONNECTOR_UTP;
				GO_WINDOW(4);
				outw(BASE + EP_W4_MEDIA_TYPE, ENABLE_UTP);
			}
			break;
		case IFM_10_2:
			if (sc->ep_connectors & BNC) {
				i = ACF_CONNECTOR_BNC;
				outw(BASE + EP_COMMAND, START_TRANSCEIVER);
				DELAY(DELAY_MULTIPLE * 1000);
			}
			break;
		case IFM_10_5:
			if (sc->ep_connectors & AUI)
				i = ACF_CONNECTOR_AUI;
			break;
		default:
			i = sc->ep_connector;
			device_printf(sc->dev,
				"strange connector type in EEPROM: assuming AUI\n");
	}

	GO_WINDOW(0);
	j = inw(BASE + EP_W0_ADDRESS_CFG) & 0x3fff;
	outw(BASE + EP_W0_ADDRESS_CFG, j | (i << ACF_CONNECTOR_BITS));

	return (0);
}

static void
ep_ifmedia_sts(ifp, ifmr)
	struct ifnet *		ifp;
	struct ifmediareq *	ifmr;
{
	struct ep_softc *	sc = ifp->if_softc;

	ifmr->ifm_active = sc->ifmedia.ifm_media;

	return;
}

static int
ep_if_ioctl(ifp, cmd, data)
	struct ifnet *		ifp;
	u_long			cmd;
	caddr_t			data;
{
	struct ep_softc *	sc = ifp->if_softc;
	struct ifreq *		ifr = (struct ifreq *)data;
	int s, error = 0;

	s = splimp();

	switch (cmd) {
	case SIOCSIFFLAGS:
		if (((ifp->if_flags & IFF_UP) == 0) &&
		    (ifp->if_flags & IFF_RUNNING)) {
			ifp->if_flags &= ~IFF_RUNNING;
			epstop(sc);
		} else {
			/* reinitialize card on any parameter change */
			ep_if_init(sc);
		}
		break;
#ifdef notdef
	case SIOCGHWADDR:
		bcopy((caddr_t) sc->sc_addr, (caddr_t) & ifr->ifr_data,
		      sizeof(sc->sc_addr));
		break;
#endif
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		/*
		 * The Etherlink III has no programmable multicast
		 * filter.  We always initialize the card to be
		 * promiscuous to multicast, since we're always a
		 * member of the ALL-SYSTEMS group, so there's no
		 * need to process SIOC*MULTI requests.
		 */
		error = 0;
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		if (!sc->epb.mii_trans) {
			error = ifmedia_ioctl(ifp, ifr, &sc->ifmedia, cmd);
		} else {
			error = EINVAL;
		}
		break;
	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}

	(void)splx(s);

	return (error);
}

static void
ep_if_watchdog(ifp)
    struct ifnet *ifp;
{
    struct ep_softc *sc = ifp->if_softc;

    /*
    printf("ep: watchdog\n");

    log(LOG_ERR, "ep%d: watchdog\n", ifp->if_unit);
    ifp->if_oerrors++;
    */

    if (sc->gone) {
	return;
    }

    ifp->if_flags &= ~IFF_OACTIVE;
    ep_if_start(ifp);
    ep_intr(ifp->if_softc);
}

static void
epstop(sc)
    struct ep_softc *sc;
{
    if (sc->gone) {
	return;
    }

    outw(BASE + EP_COMMAND, RX_DISABLE);
    outw(BASE + EP_COMMAND, RX_DISCARD_TOP_PACK);
    while (inw(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS);

    outw(BASE + EP_COMMAND, TX_DISABLE);
    outw(BASE + EP_COMMAND, STOP_TRANSCEIVER);
    DELAY(800);

    outw(BASE + EP_COMMAND, RX_RESET);
    while (inw(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS);
    outw(BASE + EP_COMMAND, TX_RESET);
    while (inw(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS);

    outw(BASE + EP_COMMAND, C_INTR_LATCH);
    outw(BASE + EP_COMMAND, SET_RD_0_MASK);
    outw(BASE + EP_COMMAND, SET_INTR_MASK);
    outw(BASE + EP_COMMAND, SET_RX_FILTER);
}
