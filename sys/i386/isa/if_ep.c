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
 *  $Id: if_ep.c,v 1.65 1997/10/27 00:02:33 fenner Exp $
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

#include "ep.h"
#if NEP > 0

#include "bpfilter.h"

#include <sys/param.h>
#if defined(__FreeBSD__)
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/conf.h>
#endif
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#if defined(__NetBSD__)
#include <sys/select.h>
#endif

#include <net/if.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#ifdef IPX
#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#if defined(__FreeBSD__)
#include <machine/clock.h>
#endif

#include <i386/isa/isa_device.h>
#include <i386/isa/if_epreg.h>
#include <i386/isa/elink.h>

/* Exported variables */
u_long	ep_unit;
int	ep_boards;
struct	ep_board ep_board[EP_MAX_BOARDS + 1];

static	int eeprom_rdy __P((struct ep_softc *sc));

static	int ep_isa_probe __P((struct isa_device *));
static struct ep_board * ep_look_for_board_at __P((struct isa_device *is));
static	int ep_isa_attach __P((struct isa_device *));
static	int epioctl __P((struct ifnet * ifp, int, caddr_t));

static	void epinit __P((struct ep_softc *));
static	void epread __P((struct ep_softc *));
void	epreset __P((int));
static	void epstart __P((struct ifnet *));
static	void epstop __P((struct ep_softc *));
static	void epwatchdog __P((struct ifnet *));

#if 0
static	int send_ID_sequence __P((int));
#endif
static	int get_eeprom_data __P((int, int));

static	struct ep_softc* ep_softc[NEP];
static	int ep_current_tag = EP_LAST_TAG + 1;
static	char *ep_conn_type[] = {"UTP", "AUI", "???", "BNC"};

#define ep_ftst(f) (sc->stat&(f))
#define ep_fset(f) (sc->stat|=(f))
#define ep_frst(f) (sc->stat&=~(f))

struct isa_driver epdriver = {
    ep_isa_probe,
    ep_isa_attach,
    "ep",
    0
};

#include "card.h"

#if NCARD > 0
#include <sys/select.h>
#include <pccard/card.h>
#include <pccard/driver.h>
#include <pccard/slot.h>

/*
 * PC-Card (PCMCIA) specific code.
 */
static int ep_pccard_init __P((struct pccard_devinfo *));
static int ep_pccard_attach  __P((struct pccard_devinfo *));
static void ep_unload __P((struct pccard_devinfo *));
static int card_intr __P((struct pccard_devinfo *));

static struct pccard_device ep_info = {
    "ep",
    ep_pccard_init,
    ep_unload,
    card_intr,
    0,                      /* Attributes - presently unused */
    &net_imask
};

DATA_SET(pccarddrv_set, ep_info);

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
    epb->prod_id = get_e(sc, EEPROM_PROD_ID);

    /* 3C589's product id? */
    if (epb->prod_id != 0x9058) {
	printf("ep%d: failed to come ready.\n", is->id_unit);
	return (ENXIO);
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
	printf("no connectors!");
    sc->ep_connector = inw(BASE + EP_W0_ADDRESS_CFG) >> ACF_CONNECTOR_BITS;

    /* ROM size = 0, ROM base = 0 */
    /* For now, ignore AUTO SELECT feature of 3C589B and later. */
    outw(BASE + EP_W0_ADDRESS_CFG, get_e(sc, EEPROM_ADDR_CFG) & 0xc000);

    /* Fake IRQ must be 3 */
    outw(BASE + EP_W0_RESOURCE_CFG, (sc->epb->res_cfg & 0x0fff) | 0x3000);

    outw(BASE + EP_W0_PRODUCT_ID, sc->epb->prod_id);

    ep_attach(sc);

    return 1;
}

static void
ep_unload(devi)
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
card_intr(devi)
    struct pccard_devinfo *devi;
{
    epintr(devi->isahd.id_unit);
    return(1);
}
#endif /* NCARD > 0 */

static int
eeprom_rdy(sc)
    struct ep_softc *sc;
{
    int i;

    for (i = 0; is_eeprom_busy(BASE) && i < MAX_EEPROMBUSY; i++)
	continue;
    if (i >= MAX_EEPROMBUSY) {
	printf("ep%d: eeprom failed to come ready.\n", sc->unit);
	return (0);
    }
    return (1);
}

static struct ep_board *
ep_look_for_board_at(is)
    struct isa_device *is;
{
    int data, i, j, id_port = ELINK_ID_PORT;
    int count = 0;

    if (ep_current_tag == (EP_LAST_TAG + 1)) {
	/* Come here just one time */

	ep_current_tag--;

        /* Look for the ISA boards. Init and leave them actived */
	outb(id_port, 0);
	outb(id_port, 0);

	elink_idseq(0xCF);

	elink_reset();
	DELAY(10000);
	for (i = 0; i < EP_MAX_BOARDS; i++) {
	    outb(id_port, 0);
	    outb(id_port, 0);
	    elink_idseq(0xCF);

	    data = get_eeprom_data(id_port, EEPROM_MFG_ID);
	    if (data != MFG_ID)
		break;

	    /* resolve contention using the Ethernet address */

	    for (j = 0; j < 3; j++)
		 get_eeprom_data(id_port, j);

	    /* and save this address for later use */

	    for (j = 0; j < 3; j++)
		 ep_board[ep_boards].eth_addr[j] = get_eeprom_data(id_port, j);

	    ep_board[ep_boards].res_cfg =
		get_eeprom_data(id_port, EEPROM_RESOURCE_CFG);

	    ep_board[ep_boards].prod_id =
		get_eeprom_data(id_port, EEPROM_PROD_ID);

	    ep_board[ep_boards].epb_used = 0;
#ifdef PC98
	    ep_board[ep_boards].epb_addr =
			(get_eeprom_data(id_port, EEPROM_ADDR_CFG) & 0x1f) * 0x100 + 0x40d0;
#else
	    ep_board[ep_boards].epb_addr =
			(get_eeprom_data(id_port, EEPROM_ADDR_CFG) & 0x1f) * 0x10 + 0x200;

	    if (ep_board[ep_boards].epb_addr > 0x3E0)
		/* Board in EISA configuration mode */
		continue;
#endif /* PC98 */

	    outb(id_port, ep_current_tag);	/* tags board */
	    outb(id_port, ACTIVATE_ADAPTER_TO_CONFIG);
	    ep_boards++;
	    count++;
	    ep_current_tag--;
	}

	ep_board[ep_boards].epb_addr = 0;
	if (count) {
	    printf("%d 3C5x9 board(s) on ISA found at", count);
	    for (j = 0; ep_board[j].epb_addr; j++)
		if (ep_board[j].epb_addr <= 0x3E0)
		    printf(" 0x%x", ep_board[j].epb_addr);
	    printf("\n");
	}
    }

    /* we have two cases:
     *
     *  1. Device was configured with 'port ?'
     *      In this case we search for the first unused card in list
     *
     *  2. Device was configured with 'port xxx'
     *      In this case we search for the unused card with that address
     *
     */

    if (IS_BASE == -1) { /* port? */
	for (i = 0; ep_board[i].epb_addr && ep_board[i].epb_used; i++)
	    ;
	if (ep_board[i].epb_addr == 0)
	    return 0;

	IS_BASE = ep_board[i].epb_addr;
	ep_board[i].epb_used = 1;

	return &ep_board[i];
    } else {
	for (i = 0;
	     ep_board[i].epb_addr && ep_board[i].epb_addr != IS_BASE;
	     i++)
	    ;

	if (ep_board[i].epb_used || ep_board[i].epb_addr != IS_BASE)
	    return 0;

	if (inw(IS_BASE + EP_W0_EEPROM_COMMAND) & EEPROM_TST_MODE) {
	    printf("ep%d: 3c5x9 at 0x%x in PnP mode. Disable PnP mode!\n",
		   is->id_unit, IS_BASE);
	}
	ep_board[i].epb_used = 1;

	return &ep_board[i];
    }
}

/*
 * get_e: gets a 16 bits word from the EEPROM. we must have set the window
 * before
 */
u_int16_t
get_e(sc, offset)
    struct ep_softc *sc;
    int offset;
{
    if (!eeprom_rdy(sc))
	return (0xffff);
    outw(BASE + EP_W0_EEPROM_COMMAND, EEPROM_CMD_RD | offset);
    if (!eeprom_rdy(sc))
	return (0xffff);
    return (inw(BASE + EP_W0_EEPROM_DATA));
}

struct ep_softc *
ep_alloc(unit, epb)
    int	unit;
    struct	ep_board *epb;
{
    struct	ep_softc *sc;

    if (unit >= NEP) {
	printf("ep: unit number (%d) too high\n", unit);
	return NULL;
    }
 
    /*
     * Allocate a storage area for us
     */
    if (ep_softc[unit]) {
	printf("ep%d: unit number already allocated to another "
	       "adaptor\n", unit);
	return NULL;
    }

    sc = malloc(sizeof(struct ep_softc), M_DEVBUF, M_NOWAIT);
    if (!sc) {
	printf("ep%d: cannot malloc!\n", unit);
	return NULL;
    }
    bzero(sc, sizeof(struct ep_softc));
    ep_softc[unit] = sc;
    sc->unit = unit;
    sc->ep_io_addr = epb->epb_addr;
    sc->epb = epb;

    return(sc);
}

void
ep_free(sc)
    struct ep_softc *sc;
{
    ep_softc[sc->unit] = NULL;
    free(sc, M_DEVBUF);
    return;
}

int
ep_isa_probe(is)
    struct isa_device *is;
{
    struct ep_softc *sc;
    struct ep_board *epb;
    u_short k;

    if ((epb = ep_look_for_board_at(is)) == 0)
	return (0);

    /*
     * Allocate a storage area for us
     */
    sc = ep_alloc(ep_unit, epb); 
    if (!sc)
	return (0);

    is->id_unit = ep_unit++;

    /*
     * The iobase was found and MFG_ID was 0x6d50. PROD_ID should be
     * 0x9[0-f]50
     */
    GO_WINDOW(0);
    k = sc->epb->prod_id;
    if ((k & 0xf0ff) != (PROD_ID & 0xf0ff)) {
	printf("ep_isa_probe: ignoring model %04x\n", k);
	ep_free(sc);
	return (0);
    }

    k = sc->epb->res_cfg;

    k >>= 12;

    /* Now we have two cases again:
     *
     *  1. Device was configured with 'irq?'
     *      In this case we use irq read from the board
     *
     *  2. Device was configured with 'irq xxx'
     *      In this case we set up the board to use specified interrupt
     *
     */

    if (is->id_irq == 0) { /* irq? */
	is->id_irq = 1 << ((k == 2) ? 9 : k);
    }

    sc->stat = 0;	/* 16 bit access */

    /* By now, the adapter is already activated */

    return (EP_IOSIZE);		/* 16 bytes of I/O space used. */
}

static int
ep_isa_attach(is)
    struct isa_device *is;
{
    struct ep_softc *sc = ep_softc[is->id_unit];
    u_short config;
    int irq;

    sc->ep_connectors = 0;
    config = inw(IS_BASE + EP_W0_CONFIG_CTRL);
    if (config & IS_AUI) {
	sc->ep_connectors |= AUI;
    }
    if (config & IS_BNC) {
	sc->ep_connectors |= BNC;
    }
    if (config & IS_UTP) {
	sc->ep_connectors |= UTP;
    }
    if (!(sc->ep_connectors & 7))
	printf("no connectors!");
    sc->ep_connector = inw(BASE + EP_W0_ADDRESS_CFG) >> ACF_CONNECTOR_BITS;
    /*
     * Write IRQ value to board
     */

    irq = ffs(is->id_irq) - 1;
    if (irq == -1) {
	printf(" invalid irq... cannot attach\n");
	return 0;
    }

    GO_WINDOW(0);
    SET_IRQ(BASE, irq);

    ep_attach(sc);
    return 1;
}

int
ep_attach(sc)
    struct ep_softc *sc;
{
    struct ifnet *ifp = &sc->arpcom.ac_if;
    u_short *p;
    int i;
    int attached;

    sc->gone = 0;
    attached = (ifp->if_softc != 0);

    printf("ep%d: ", sc->unit);
    /*
     * Current media type
     */
    if (sc->ep_connectors & AUI) {
	printf("aui");
	if (sc->ep_connectors & ~AUI)
		printf("/");
    }
    if (sc->ep_connectors & UTP) {
	printf("utp");
	if (sc->ep_connectors & BNC)
		printf("/");
    }
    if (sc->ep_connectors & BNC) {
	printf("bnc");
    }

    printf("[*%s*]", ep_conn_type[sc->ep_connector]);

    /*
     * Setup the station address
     */
    p = (u_short *) & sc->arpcom.ac_enaddr;
    GO_WINDOW(2);
    for (i = 0; i < 3; i++) {
	p[i] = htons(sc->epb->eth_addr[i]);
	outw(BASE + EP_W2_ADDR_0 + (i * 2), ntohs(p[i]));
    }
    printf(" address %6D\n", sc->arpcom.ac_enaddr, ":");

    ifp->if_softc = sc;
    ifp->if_unit = sc->unit;
    ifp->if_name = "ep";
    ifp->if_mtu = ETHERMTU;
    ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
    ifp->if_output = ether_output;
    ifp->if_start = epstart;
    ifp->if_ioctl = epioctl;
    ifp->if_watchdog = epwatchdog;

    if (!attached) {
	if_attach(ifp);
	ether_ifattach(ifp);
    }

#ifdef EP_LOCAL_STATS
    sc->rx_no_first = sc->rx_no_mbuf =
	sc->rx_bpf_disc = sc->rx_overrunf = sc->rx_overrunl =
	sc->tx_underrun = 0;
#endif
    ep_fset(F_RX_FIRST);
    sc->top = sc->mcur = 0;

#if NBPFILTER > 0
    if (!attached) {
	bpfattach(ifp, DLT_EN10MB, sizeof(struct ether_header));
    }
#endif
    return 0;
}


/*
 * The order in here seems important. Otherwise we may not receive
 * interrupts. ?!
 */
static void
epinit(sc)
    struct ep_softc *sc;
{
    register struct ifnet *ifp = &sc->arpcom.ac_if;
    int s, i, j;

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

     /*
      * S.B.
      *
      * Now behavior was slightly changed:
      *
      * if any of flags link[0-2] is used and its connector is
      * physically present the following connectors are used:
      *
      *   link0 - AUI * highest precedence
      *   link1 - BNC
      *   link2 - UTP * lowest precedence
      *
      * If none of them is specified then
      * connector specified in the EEPROM is used
      * (if present on card or AUI if not).
      *
      */

    /* Set the xcvr. */
    if (ifp->if_flags & IFF_LINK0 && sc->ep_connectors & AUI) {
	i = ACF_CONNECTOR_AUI;
    } else if (ifp->if_flags & IFF_LINK1 && sc->ep_connectors & BNC) {
	i = ACF_CONNECTOR_BNC;
    } else if (ifp->if_flags & IFF_LINK2 && sc->ep_connectors & UTP) {
	i = ACF_CONNECTOR_UTP;
    } else {
	i = sc->ep_connector;
    }
    GO_WINDOW(0);
    j = inw(BASE + EP_W0_ADDRESS_CFG) & 0x3fff;
    outw(BASE + EP_W0_ADDRESS_CFG, j | (i << ACF_CONNECTOR_BITS));

    switch(i) {
      case ACF_CONNECTOR_UTP:
	if (sc->ep_connectors & UTP) {
	    GO_WINDOW(4);
	    outw(BASE + EP_W4_MEDIA_TYPE, ENABLE_UTP);
	}
	break;
      case ACF_CONNECTOR_BNC:
	if (sc->ep_connectors & BNC) {
	    outw(BASE + EP_COMMAND, START_TRANSCEIVER);
	    DELAY(1000);
	}
	break;
      case ACF_CONNECTOR_AUI:
	/* nothing to do */
	break;
      default:
	printf("ep%d: strange connector type in EEPROM: assuming AUI\n",
	       sc->unit);
	break;
    }

    outw(BASE + EP_COMMAND, RX_ENABLE);
    outw(BASE + EP_COMMAND, TX_ENABLE);

    ifp->if_flags |= IFF_RUNNING;
    ifp->if_flags &= ~IFF_OACTIVE;	/* just in case */

#ifdef EP_LOCAL_STATS
    sc->rx_no_first = sc->rx_no_mbuf =
	sc->rx_bpf_disc = sc->rx_overrunf = sc->rx_overrunl =
	sc->tx_underrun = 0;
#endif
    ep_fset(F_RX_FIRST);
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
    epstart(ifp);

    splx(s);
}

static const char padmap[] = {0, 3, 2, 1};

static void
epstart(ifp)
    struct ifnet *ifp;
{
    register struct ep_softc *sc = ifp->if_softc;
    register u_int len;
    register struct mbuf *m;
    struct mbuf *top;
    int s, pad;

    if (sc->gone) {
	return;
    }

    s = splimp();
    while (inw(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS);
    if (ifp->if_flags & IFF_OACTIVE) {
	splx(s);
	return;
    }
startagain:
    /* Sneak a peek at the next packet */
    m = ifp->if_snd.ifq_head;
    if (m == 0) {
	splx(s);
	return;
    }
    for (len = 0, top = m; m; m = m->m_next)
	len += m->m_len;

    pad = padmap[len & 3];

    /*
     * The 3c509 automatically pads short packets to minimum ethernet length,
     * but we drop packets that are too large. Perhaps we should truncate
     * them instead?
     */
    if (len + pad > ETHER_MAX_LEN) {
	/* packet is obviously too large: toss it */
	++ifp->if_oerrors;
	IF_DEQUEUE(&ifp->if_snd, m);
	m_freem(m);
	goto readcheck;
    }
    if (inw(BASE + EP_W1_FREE_TX) < len + pad + 4) {
	/* no room in FIFO */
	outw(BASE + EP_COMMAND, SET_TX_AVAIL_THRESH | (len + pad + 4));
	/* make sure */
	if (inw(BASE + EP_W1_FREE_TX) < len + pad + 4) {
	    ifp->if_flags |= IFF_OACTIVE;
	    splx(s);
	    return;
	}
    }
    IF_DEQUEUE(&ifp->if_snd, m);

    outw(BASE + EP_W1_TX_PIO_WR_1, len); 
    outw(BASE + EP_W1_TX_PIO_WR_1, 0x0);	/* Second dword meaningless */

    for (top = m; m != 0; m = m->m_next)
	if (ep_ftst(F_ACCESS_32_BITS)) {
	    outsl(BASE + EP_W1_TX_PIO_WR_1, mtod(m, caddr_t),
		  m->m_len / 4);
	    if (m->m_len & 3)
		outsb(BASE + EP_W1_TX_PIO_WR_1,
		      mtod(m, caddr_t) + (m->m_len & (~3)),
		      m->m_len & 3);
	} else {
	    outsw(BASE + EP_W1_TX_PIO_WR_1, mtod(m, caddr_t), m->m_len / 2);
	    if (m->m_len & 1)
		outb(BASE + EP_W1_TX_PIO_WR_1,
		     *(mtod(m, caddr_t) + m->m_len - 1));
	}

    while (pad--)
	outb(BASE + EP_W1_TX_PIO_WR_1, 0);	/* Padding */

#if NBPFILTER > 0
    if (ifp->if_bpf) {
	bpf_mtap(ifp, top);
    }
#endif

    ifp->if_timer = 2;
    ifp->if_opackets++;
    m_freem(top);

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
	if (ifp->if_snd.ifq_head) {
	    outw(BASE + EP_COMMAND, SET_TX_AVAIL_THRESH | 8);
	}
	splx(s);
	return;
    }
    goto startagain;
}

void
epintr(unit)
    int unit;
{
    register struct ep_softc *sc = ep_softc[unit];

    if (sc->gone) {
	return;
    }

    ep_intr(sc);
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

    ifp = &sc->arpcom.ac_if;

    outw(BASE + EP_COMMAND, SET_INTR_MASK); /* disable all Ints */

rescan:

    while ((status = inw(BASE + EP_STATUS)) & S_5_INTS) {

	/* first acknowledge all interrupt sources */
	outw(BASE + EP_COMMAND, ACK_INTR | (status & S_MASK));

	if (status & (S_RX_COMPLETE | S_RX_EARLY)) {
	    epread(sc);
	    continue;
	}
	if (status & S_TX_AVAIL) {
	    /* we need ACK */
	    ifp->if_timer = 0;
	    ifp->if_flags &= ~IFF_OACTIVE;
	    GO_WINDOW(1);
	    inw(BASE + EP_W1_FREE_TX);
	    epstart(ifp);
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
	    printf("\tNOF=%d, NOMB=%d, BPFD=%d, RXOF=%d, RXOL=%d, TXU=%d\n",
		   sc->rx_no_first, sc->rx_no_mbuf, sc->rx_bpf_disc, sc->rx_overrunf,
		   sc->rx_overrunl, sc->tx_underrun);
#else

#ifdef DIAGNOSTIC
	    printf("ep%d: Status: %x (input buffer overflow)\n", sc->unit, status);
#else
	    ++ifp->if_ierrors;
#endif

#endif
	    epinit(sc);
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
	    epstart(ifp);
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
    struct ether_header *eh;
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
	    if (ep_ftst(F_RX_FIRST))
		sc->rx_overrunf++;
	    else
		sc->rx_overrunl++;
#endif
	}
	goto out;
    }
    rx_fifo = rx_fifo2 = status & RX_BYTES_MASK;

    if (ep_ftst(F_RX_FIRST)) {
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
	if (ep_ftst(F_ACCESS_32_BITS)) { /* default for EISA configured cards*/
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
	ep_frst(F_RX_FIRST);
	if (!((status = inw(BASE + EP_W1_RX_STATUS)) & ERR_RX_INCOMPLETE)) {
	    /* we see if by now, the packet has completly arrived */
	    goto read_again;
	}
	outw(BASE + EP_COMMAND, SET_RX_EARLY_THRESH | RX_NEXT_EARLY_THRESH);
	return;
    }
    outw(BASE + EP_COMMAND, RX_DISCARD_TOP_PACK);
    ++ifp->if_ipackets;
    ep_fset(F_RX_FIRST);
    top->m_pkthdr.rcvif = &sc->arpcom.ac_if;
    top->m_pkthdr.len = sc->cur_len;

#if NBPFILTER > 0
    if (ifp->if_bpf) {
	bpf_mtap(ifp, top);

	/*
	 * Note that the interface cannot be in promiscuous mode if there are
	 * no BPF listeners.  And if we are in promiscuous mode, we have to
	 * check if this packet is really ours.
	 */
	eh = mtod(top, struct ether_header *);
	if ((ifp->if_flags & IFF_PROMISC) &&
	    (eh->ether_dhost[0] & 1) == 0 &&
	    bcmp(eh->ether_dhost, sc->arpcom.ac_enaddr,
		 sizeof(eh->ether_dhost)) != 0 &&
	    bcmp(eh->ether_dhost, etherbroadcastaddr,
		 sizeof(eh->ether_dhost)) != 0) {
	    if (sc->top) {
		m_freem(sc->top);
		sc->top = 0;
	    }
	    ep_fset(F_RX_FIRST);
#ifdef EP_LOCAL_STATS
	    sc->rx_bpf_disc++;
#endif
	    while (inw(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS);
	    outw(BASE + EP_COMMAND, SET_RX_EARLY_THRESH | RX_INIT_EARLY_THRESH);
	    return;
	}
    }
#endif

    eh = mtod(top, struct ether_header *);
    m_adj(top, sizeof(struct ether_header));
    ether_input(ifp, eh, top);
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
    ep_fset(F_RX_FIRST);
    while (inw(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS);
    outw(BASE + EP_COMMAND, SET_RX_EARLY_THRESH | RX_INIT_EARLY_THRESH);
}

/*
 * Look familiar?
 */
static int
epioctl(ifp, cmd, data)
    register struct ifnet *ifp;
    int cmd;
    caddr_t data;
{
    register struct ifaddr *ifa = (struct ifaddr *) data;
    struct ep_softc *sc = ifp->if_softc;
    struct ifreq *ifr = (struct ifreq *) data;
    int s, error = 0;

    s = splimp();

    switch (cmd) {
      case SIOCSIFADDR:
	ifp->if_flags |= IFF_UP;

	/* netifs are BUSY when UP */

	switch (ifa->ifa_addr->sa_family) {
#ifdef INET
	  case AF_INET:
	    epinit(sc);	/* before arpwhohas */
	    arp_ifinit((struct arpcom *)ifp, ifa);
	    break;
#endif
#ifdef IPX
	  case AF_IPX:
	    {
		register struct ipx_addr *ina = &(IA_SIPX(ifa)->sipx_addr);

		if (ipx_nullhost(*ina))
		    ina->x_host =
			*(union ipx_host *) (sc->arpcom.ac_enaddr);
		else {
		    ifp->if_flags &= ~IFF_RUNNING;
		    bcopy((caddr_t) ina->x_host.c_host,
			  (caddr_t) sc->arpcom.ac_enaddr,
			  sizeof(sc->arpcom.ac_enaddr));
		}
		epinit(sc);
		break;
	    }
#endif
#ifdef NS
	  case AF_NS:
	    {
		register struct ns_addr *ina = &(IA_SNS(ifa)->sns_addr);

		if (ns_nullhost(*ina))
		    ina->x_host =
			*(union ns_host *) (sc->arpcom.ac_enaddr);
		else {
		    ifp->if_flags &= ~IFF_RUNNING;
		    bcopy((caddr_t) ina->x_host.c_host,
			  (caddr_t) sc->arpcom.ac_enaddr,
			  sizeof(sc->arpcom.ac_enaddr));
		}
		epinit(sc);
		break;
	    }
#endif
	  default:
	    epinit(sc);
	    break;
	}
	break;
      case SIOCGIFADDR:
	{ 
	  struct sockaddr *sa; 
 
	  sa = (struct sockaddr *) & ifr->ifr_data;
	  bcopy((caddr_t) sc->arpcom.ac_enaddr, 
		(caddr_t) sa->sa_data, ETHER_ADDR_LEN);
	}
	break;
      case SIOCSIFFLAGS:

	if ((ifp->if_flags & IFF_UP) == 0 && ifp->if_flags & IFF_RUNNING) {
	    ifp->if_flags &= ~IFF_RUNNING;
	    epstop(sc);
	    break;
	} else {
	    /* reinitialize card on any parameter change */
	    epinit(sc);
	    break;
	}

	/* NOTREACHED */
	break;
#ifdef notdef
      case SIOCGHWADDR:
	bcopy((caddr_t) sc->sc_addr, (caddr_t) & ifr->ifr_data,
	      sizeof(sc->sc_addr));
	break;
#endif
	case SIOCSIFMTU:

		/*
		 * Set the interface MTU.
		 */
		if (ifr->ifr_mtu > ETHERMTU) {
			error = EINVAL;
		} else {
			ifp->if_mtu = ifr->ifr_mtu;
		}
		break; 
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
      default:
		error = EINVAL;
    }

    splx(s);

    return (error);
}

static void
epwatchdog(ifp)
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
    epstart(ifp);
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
    outw(BASE + EP_COMMAND, RX_RESET);
    outw(BASE + EP_COMMAND, TX_RESET);
    while (inw(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS);
    outw(BASE + EP_COMMAND, C_INTR_LATCH);
    outw(BASE + EP_COMMAND, SET_RD_0_MASK);
    outw(BASE + EP_COMMAND, SET_INTR_MASK);
    outw(BASE + EP_COMMAND, SET_RX_FILTER);
}


#if 0
static int
send_ID_sequence(port)
    int port;
{
    int cx, al;

    for (al = 0xff, cx = 0; cx < 255; cx++) {
	outb(port, al);
	al <<= 1;
	if (al & 0x100)
	    al ^= 0xcf;
    }
    return (1);
}
#endif


/*
 * We get eeprom data from the id_port given an offset into the eeprom.
 * Basically; after the ID_sequence is sent to all of the cards; they enter
 * the ID_CMD state where they will accept command requests. 0x80-0xbf loads
 * the eeprom data.  We then read the port 16 times and with every read; the
 * cards check for contention (ie: if one card writes a 0 bit and another
 * writes a 1 bit then the host sees a 0. At the end of the cycle; each card
 * compares the data on the bus; if there is a difference then that card goes
 * into ID_WAIT state again). In the meantime; one bit of data is returned in
 * the AX register which is conveniently returned to us by inb().  Hence; we
 * read 16 times getting one bit of data with each read.
 */
static int
get_eeprom_data(id_port, offset)
    int id_port;
    int offset;
{
    int i, data = 0;
    outb(id_port, 0x80 + offset);
    DELAY(1000);
    for (i = 0; i < 16; i++)
	data = (data << 1) | (inw(id_port) & 1);
    return (data);
}

#endif				/* NEP > 0 */
