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
 *	$Id: if_ep.c,v 1.20 1995/03/23 06:53:38 davidg Exp $
 */

/*
 *	Modified from the FreeBSD 1.1.5.1 version by:
 *		 	Andres Vega Garcia 
 *			INRIA - Sophia Antipolis, France
 *			avega@sophia.inria.fr
 */

#include "ep.h"
#if NEP > 0

#include "bpfilter.h"

#include <sys/param.h>
#if defined(__FreeBSD__)
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/devconf.h>
#endif
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#if defined(__NetBSD__)
#include <sys/select.h>
#endif

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/icu.h>
#include <i386/isa/if_epreg.h>

static int epprobe __P((struct isa_device *));
static int epattach __P((struct isa_device *));
static int epioctl __P((struct ifnet * ifp, int, caddr_t));
static void epmbuffill __P((caddr_t, int));
static void epmbufempty __P((struct ep_softc *));

void epinit __P((int));
void epintr __P((int));
void epread __P((struct ep_softc *));
void epreset __P((int));
void epstart __P((struct ifnet *));
void epstop __P((int));
void epwatchdog __P((int));

static int send_ID_sequence __P((int));
static int get_eeprom_data __P((int, int));

struct ep_softc ep_softc[NEP];

#define ep_ftst(f) (sc->stat&(f))
#define ep_fset(f) (sc->stat|=(f))
#define ep_frst(f) (sc->stat&=~(f))

struct isa_driver epdriver = {
    epprobe,
    epattach,
    "ep"
};

static struct kern_devconf kdc_ep[NEP] = { {
      0, 0, 0,                /* filled in by dev_attach */
      "ep", 0, { MDDT_ISA, 0, "net" },
      isa_generic_externalize, 0, 0, ISA_EXTERNALLEN,
      &kdc_isa0,              /* parent */
      0,                      /* parentdata */
      DC_BUSY,                /* network interfaces are always ``open'' */
      "3Com 3C509 Ethernet adapter"
} };

static inline void
ep_registerdev(struct isa_device *id)
{
      if(id->id_unit)
              kdc_ep[id->id_unit] = kdc_ep[0];
      kdc_ep[id->id_unit].kdc_unit = id->id_unit;
      kdc_ep[id->id_unit].kdc_parentdata = id;
      dev_attach(&kdc_ep[id->id_unit]);
}

int ep_current_tag = EP_LAST_TAG + 1;

int ep_board[EP_MAX_BOARDS + 1];

static int
eeprom_rdy(is)
    struct isa_device *is;
{
    int i;

    for (i = 0; is_eeprom_busy(IS_BASE) && i < MAX_EEPROMBUSY; i++);
    if (i >= MAX_EEPROMBUSY) {
	printf("ep%d: eeprom failed to come ready.\n", is->id_unit);
	return (0);
    }
    return (1);
}

static int
ep_look_for_board_at(is)
    struct isa_device *is;
{
    int data, i, j, io_base, id_port = EP_ID_PORT;
    int nisa = 0, neisa = 0;

    if (ep_current_tag == (EP_LAST_TAG + 1)) {
	/* Come here just one time */
   
	/* Look for the EISA boards, leave them activated */
	for(j = 1; j < 16; j++) {
	    io_base = (j * EP_EISA_START) | EP_EISA_W0;
	    if (inw(io_base + EP_W0_MFG_ID) != MFG_ID)
		continue;

	    /* we must found 0x1f if the board is EISA configurated */
	    if ((inw(io_base + EP_W0_ADDRESS_CFG) & 0x1f) != 0x1f) 
		continue;

	    /* Reset and Enable the card */
	    outb(io_base + EP_W0_CONFIG_CTRL, W0_P4_CMD_RESET_ADAPTER);
	    DELAY(1000); /* we must wait at least 1 ms */
	    outb(io_base + EP_W0_CONFIG_CTRL, W0_P4_CMD_ENABLE_ADAPTER);

	    /*
	     * Once activated, all the registers are mapped in the range
	     * x000 - x00F, where x is the slot number.
             */
	    ep_board[neisa++] = j * EP_EISA_START;
	}
	ep_current_tag--;

        /* Look for the ISA boards. Init and leave them actived */
	outb(id_port, 0xc0);	/* Global reset */
	DELAY(1000);
	for (i = 0; i < EP_MAX_BOARDS; i++) {
	    outb(id_port, 0);
	    outb(id_port, 0);
	    send_ID_sequence(id_port);

	    data = get_eeprom_data(id_port, EEPROM_MFG_ID);
	    if (data != MFG_ID)
		break;

	    /* resolve contention using the Ethernet address */
	    for (j = 0; j < 3; j++)
		data = get_eeprom_data(id_port, j);

	    ep_board[neisa+nisa++] =
		(get_eeprom_data(id_port, EEPROM_ADDR_CFG) & 0x1f) * 0x10 + 0x200;
	    outb(id_port, ep_current_tag);	/* tags board */
	    outb(id_port, ACTIVATE_ADAPTER_TO_CONFIG);
	    ep_current_tag--;
	}

	ep_board[neisa+nisa] = 0;
	if (neisa) {
	    printf("%d 3C5x9 board(s) on EISA found at", neisa);
	    for (j = 0; ep_board[j]; j++)
		if (ep_board[j] >= EP_EISA_START)
		    printf(" 0x%x", ep_board[j]);
	    printf("\n");
	}
	if (nisa) {
	    printf("%d 3C5x9 board(s) on ISA found at", nisa);
	    for (j = 0; ep_board[j]; j++)
		if (ep_board[j] < EP_EISA_START)
		    printf(" 0x%x", ep_board[j]);
	    printf("\n");
	}
    }

    for (i = 0; ep_board[i] && ep_board[i] != IS_BASE; i++);
    if (ep_board[i] == IS_BASE) {
	if (inw(IS_BASE + EP_W0_EEPROM_COMMAND) & EEPROM_TST_MODE)
	    printf("ep%d: 3c5x9 at 0x%x in test mode. Erase pencil mark!\n",
		   is->id_unit, IS_BASE);
	return (1);
    }
    return (0);
}

/*
 * get_e: gets a 16 bits word from the EEPROM. we must have set the window
 * before
 */
static int
get_e(is, offset)
    struct isa_device *is;
    int offset;
{
    if (!eeprom_rdy(is))
	return (0xffff);
    outw(IS_BASE + EP_W0_EEPROM_COMMAND, EEPROM_CMD_RD | offset);
    if (!eeprom_rdy(is))
	return (0xffff);
    return (inw(IS_BASE + EP_W0_EEPROM_DATA));
}

int
epprobe(is)
    struct isa_device *is;
{
    struct ep_softc *sc = &ep_softc[is->id_unit];
    u_short k;
    int i;

    if (!ep_look_for_board_at(is))
	return (0);
    /*
     * The iobase was found and MFG_ID was 0x6d50. PROD_ID should be
     * 0x9[0-f]50
     */
    GO_WINDOW(0);
    k = get_e(is, EEPROM_PROD_ID);
    if ((k & 0xf0ff) != (PROD_ID & 0xf0ff)) {
	printf("epprobe: ignoring model %04x\n", k);
	return (0);
    }

    k = get_e(is, EEPROM_RESOURCE_CFG);
    k >>= 12;
    if (is->id_irq != (1 << ((k == 2) ? 9 : k))) {
	printf("epprobe: interrupt number %d doesn't match\n",is->id_irq);
	return (0);
    }

    if (BASE >= EP_EISA_START) /* we have an EISA board, we allow 32 bits access */
	sc->stat = F_ACCESS_32_BITS;
    else
	sc->stat = 0;

    /* By now, the adapter is already activated */

    return (0x10);		/* 16 bytes of I/O space used. */
}

static char *ep_conn_type[] = {"UTP", "AUI", "???", "BNC"};

static int
epattach(is)
    struct isa_device *is;
{
    struct ep_softc *sc = &ep_softc[is->id_unit];
    struct ifnet *ifp = &sc->arpcom.ac_if;
    u_short i, j, *p;
    struct ifaddr *ifa;
    struct sockaddr_dl *sdl;

    /* BASE = IS_BASE; */
    sc->ep_io_addr = is->id_iobase;

    printf("ep%d: ", is->id_unit);

    sc->ep_connectors = 0;
    i = inw(IS_BASE + EP_W0_CONFIG_CTRL);
    j = inw(IS_BASE + EP_W0_ADDRESS_CFG) >> 14;
    if (i & IS_AUI) {
	printf("aui");
	sc->ep_connectors |= AUI;
    }
    if (i & IS_BNC) {
	if (sc->ep_connectors)
	    printf("/");
	printf("bnc");
	sc->ep_connectors |= BNC;
    }
    if (i & IS_UTP) {
	if (sc->ep_connectors)
	    printf("/");
	printf("utp");
	sc->ep_connectors |= UTP;
    }
    if (!(sc->ep_connectors & 7))
	printf("no connectors!");
    else
	printf("[*%s*]", ep_conn_type[j]);

    /*
     * Read the station address from the eeprom
     */
    p = (u_short *) & sc->arpcom.ac_enaddr;
    for (i = 0; i < 3; i++) {
	GO_WINDOW(0);
	p[i] = htons(get_e(is, i));
	GO_WINDOW(2);
	outw(BASE + EP_W2_ADDR_0 + (i * 2), ntohs(p[i]));
    }
    printf(" address %s\n", ether_sprintf(sc->arpcom.ac_enaddr));

    ifp->if_unit = is->id_unit;
    ifp->if_name = "ep";
    ifp->if_mtu = ETHERMTU;
    ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS;
    ifp->if_init = epinit;
    ifp->if_output = ether_output;
    ifp->if_start = epstart;
    ifp->if_ioctl = epioctl;
    ifp->if_watchdog = epwatchdog;

    if_attach(ifp);
    ep_registerdev(is);

    /*
     * Fill the hardware address into ifa_addr if we find an AF_LINK entry.
     * We need to do this so bpf's can get the hardware addr of this card.
     * netstat likes this too!
     */
    ifa = ifp->if_addrlist;
    while ((ifa != 0) && (ifa->ifa_addr != 0) &&
	   (ifa->ifa_addr->sa_family != AF_LINK))
	ifa = ifa->ifa_next;

    if ((ifa != 0) && (ifa->ifa_addr != 0)) {
	sdl = (struct sockaddr_dl *) ifa->ifa_addr;
	sdl->sdl_type = IFT_ETHER;
	sdl->sdl_alen = ETHER_ADDR_LEN;
	sdl->sdl_slen = 0;
	bcopy(sc->arpcom.ac_enaddr, LLADDR(sdl), ETHER_ADDR_LEN);
    }
    /* we give some initial parameters */
    sc->rx_avg_pkt = 128;

    /*
     * NOTE: In all this I multiply everything by 64. 
     * W_s = the speed the CPU is able to write to the TX FIFO. 
     * T_s = the speed the board sends the info to the Ether.
     * W_s/T_s = 16   (represents 16/64) =>    W_s = 25 % of T_s. 
     * This will give us for a packet of 1500 bytes
     * tx_start_thresh=1125 and for a pkt of 64 bytes tx_start_threshold=48.
     * We prefer to start thinking the CPU is much slower than the Ethernet
     * transmission.
     */
    sc->tx_rate = TX_INIT_RATE;
    sc->tx_counter = 0;
    sc->rx_latency = RX_INIT_LATENCY;
    sc->rx_early_thresh = RX_INIT_EARLY_THRESH;
#ifdef EP_LOCAL_STATS
    sc->rx_no_first = sc->rx_no_mbuf =
	sc->rx_bpf_disc = sc->rx_overrunf = sc->rx_overrunl =
	sc->tx_underrun = 0;
#endif
    ep_fset(F_RX_FIRST);
    sc->top = sc->mcur = 0;

#if NBPFILTER > 0
    bpfattach(&sc->bpf, ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif
    return 1;
}


/*
 * The order in here seems important. Otherwise we may not receive
 * interrupts. ?!
 */
void
epinit(unit)
    int unit;
{
    register struct ep_softc *sc = &ep_softc[unit];
    register struct ifnet *ifp = &sc->arpcom.ac_if;
    int s, i;

    if (ifp->if_addrlist == (struct ifaddr *) 0)
	return;

    s = splimp();
    while (inw(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS);

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

    /* Window 1 is operating window */
    GO_WINDOW(1);
    for (i = 0; i < 31; i++)
	inb(BASE + EP_W1_TX_STATUS);

    /* get rid of stray intr's */
    outw(BASE + EP_COMMAND, ACK_INTR | 0xff);

    outw(BASE + EP_COMMAND, SET_RD_0_MASK | S_5_INTS);

    outw(BASE + EP_COMMAND, SET_INTR_MASK | S_5_INTS);

	if(ep_ftst(F_PROMISC))
		outw(BASE + EP_COMMAND, SET_RX_FILTER | FIL_INDIVIDUAL |
		 FIL_GROUP | FIL_BRDCST | FIL_ALL);
	else
		outw(BASE + EP_COMMAND, SET_RX_FILTER | FIL_INDIVIDUAL |
		 FIL_GROUP | FIL_BRDCST);

	/*
	 * you can `ifconfig ep0 (bnc|aui)' to get the following
	 * behaviour:
	 *	bnc	disable AUI/UTP. enable BNC.
	 *	aui	disable BNC. enable AUI. if the card has a UTP
	 *		connector, that is enabled too. not sure, but it
	 * 		seems you have to be careful to not plug things
	 *		into both AUI & UTP.
	 */
#if defined(__NetBSD__)
    if (!(ifp->if_flags & IFF_LINK0) && (sc->ep_connectors & BNC)) {
#else
    if (!(ifp->if_flags & IFF_ALTPHYS) && (sc->ep_connectors & BNC)) {
#endif
	outw(BASE + EP_COMMAND, START_TRANSCEIVER);
	DELAY(1000);
    }
#if defined(__NetBSD__)
    if ((ifp->if_flags & IFF_LINK0) && (sc->ep_connectors & UTP)) {
#else
    if ((ifp->if_flags & IFF_ALTPHYS) && (sc->ep_connectors & UTP)) {
#endif
	GO_WINDOW(4);
	outw(BASE + EP_W4_MEDIA_TYPE, ENABLE_UTP);
	GO_WINDOW(1);
    }
    outw(BASE + EP_COMMAND, RX_ENABLE);
    outw(BASE + EP_COMMAND, TX_ENABLE);

    ifp->if_flags |= IFF_RUNNING;
    ifp->if_flags &= ~IFF_OACTIVE;	/* just in case */

    sc->tx_rate = TX_INIT_RATE;
    sc->tx_counter = 0;
    sc->rx_latency = RX_INIT_LATENCY;
    sc->rx_early_thresh = RX_INIT_EARLY_THRESH;
#ifdef EP_LOCAL_STATS
    sc->rx_no_first = sc->rx_no_mbuf =
	sc->rx_bpf_disc = sc->rx_overrunf = sc->rx_overrunl =
	sc->tx_underrun = 0;
#endif
    ep_fset(F_RX_FIRST);
    ep_frst(F_RX_TRAILER);
    if (sc->top) {
	m_freem(sc->top);
	sc->top = sc->mcur = 0;
    }
    outw(BASE + EP_COMMAND, SET_RX_EARLY_THRESH | sc->rx_early_thresh);

    /*
     * Store up a bunch of mbuf's for use later. (MAX_MBS). First we free up
     * any that we had in case we're being called from intr or somewhere
     * else.
     */
    sc->last_mb = 0;
    sc->next_mb = 0;
    epmbuffill((caddr_t) sc, 0);

    epstart(ifp);

    splx(s);
}

static const char padmap[] = {0, 3, 2, 1};

void
epstart(ifp)
    struct ifnet *ifp;
{
    register struct ep_softc *sc = &ep_softc[ifp->if_unit];
    register u_int len;
    register struct mbuf *m;
    struct mbuf *top;
    int s, pad;

    s = splimp();
    if (sc->arpcom.ac_if.if_flags & IFF_OACTIVE) {
	splx(s);
	return;
    }
startagain:
    /* Sneak a peek at the next packet */
    m = sc->arpcom.ac_if.if_snd.ifq_head;
    if (m == 0) {
	splx(s);
	return;
    }
#if 0
    len = m->m_pkthdr.len;
#else
    for (len = 0, top = m; m; m = m->m_next)
	len += m->m_len;
#endif

    pad = padmap[len & 3];

    /*
     * The 3c509 automatically pads short packets to minimum ethernet length,
     * but we drop packets that are too large. Perhaps we should truncate
     * them instead?
     */
    if (len + pad > ETHER_MAX_LEN) {
	/* packet is obviously too large: toss it */
	++sc->arpcom.ac_if.if_oerrors;
	IF_DEQUEUE(&sc->arpcom.ac_if.if_snd, m);
	m_freem(m);
	goto readcheck;
    }
    if (inw(BASE + EP_W1_FREE_TX) < len + pad + 4) {
	/* no room in FIFO */
	outw(BASE + EP_COMMAND, SET_TX_AVAIL_THRESH | (len + pad + 4));
	sc->arpcom.ac_if.if_flags |= IFF_OACTIVE;
	splx(s);
	return;
    }
    IF_DEQUEUE(&sc->arpcom.ac_if.if_snd, m);

    outw(BASE + EP_W1_TX_PIO_WR_1, len);
    outw(BASE + EP_W1_TX_PIO_WR_1, 0x0);	/* Second dword meaningless */

    /* compute the Tx start threshold for this packet */
    sc->tx_start_thresh = len =
	(((len * (64 - sc->tx_rate)) >> 6) & ~3) + 16;
    outw(BASE + EP_COMMAND, SET_TX_START_THRESH | len);

    for (top = m; m != 0; m = m->m_next)
	if(ep_ftst(F_ACCESS_32_BITS)) {
	    outsl(BASE + EP_W1_TX_PIO_WR_1, mtod(m, caddr_t),
		  m->m_len / 4);
	    if (m->m_len & 3)
		outsb(BASE + EP_W1_TX_PIO_WR_1,
		      mtod(m, caddr_t) + m->m_len / 4,
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
    if (sc->bpf) {
	bpf_mtap(sc->bpf, top);
    }
#endif

    sc->arpcom.ac_if.if_opackets++;
    m_freem(top);
    /*
     * Every 1024*4 packets we increment the tx_rate if we haven't had
     * errors, that in the case it has abnormaly goten too low
     */
    if (!(++sc->tx_counter & (1024 * 4 - 1)) &&
	sc->tx_rate < TX_INIT_MAX_RATE)
	sc->tx_rate++;

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
	if (sc->arpcom.ac_if.if_snd.ifq_head) {
	    outw(BASE + EP_COMMAND, SET_TX_AVAIL_THRESH |
		 sc->tx_start_thresh);
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
    int i;
    register int status;
    register struct ep_softc *sc = &ep_softc[unit];
    struct ifnet *ifp = &sc->arpcom.ac_if;
    struct mbuf *m;

rescan:

    while ((status = inw(BASE + EP_STATUS)) & S_5_INTS) {
	if (status & (S_RX_COMPLETE | S_RX_EARLY)) {
	    /* we just need ACK for RX_EARLY */
	    if (status & S_RX_EARLY)
		outw(BASE + EP_COMMAND, C_RX_EARLY);
	    epread(sc);
	    continue;
	}
	if (status & S_TX_AVAIL) {
	    /* we need ACK */
	    outw(BASE + EP_COMMAND, C_TX_AVAIL);
	    sc->arpcom.ac_if.if_flags &= ~IFF_OACTIVE;
	    epstart(&sc->arpcom.ac_if);
	}
	if (status & S_CARD_FAILURE) {
#ifdef EP_LOCAL_STATS
	    printf("\nep%d:\n\tStatus: %x\n", unit, status);
	    GO_WINDOW(4);
	    printf("\tFIFO Diagnostic: %x\n", inw(BASE + EP_W4_FIFO_DIAG));
	    printf("\tStat: %x\n", sc->stat);
	    printf("\tIpackets=%d, Opackets=%d\n",
		sc->arpcom.ac_if.if_ipackets, sc->arpcom.ac_if.if_opackets);
	    printf("\tNOF=%d, NOMB=%d, BPFD=%d, RXOF=%d, RXOL=%d, TXU=%d\n",
		   sc->rx_no_first, sc->rx_no_mbuf, sc->rx_bpf_disc, sc->rx_overrunf,
		   sc->rx_overrunl, sc->tx_underrun);
#else
	    printf("ep%d: Status: %x\n", unit, status); 
#endif
	    epinit(unit);
	    return;
	}
	if (status & S_TX_COMPLETE) {
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
			if (sc->tx_rate > 1) {
			    sc->tx_rate--;	/* Actually in steps of 1/64 */
			    sc->tx_counter = 0;	/* We reset it */
			}
#ifdef EP_LOCAL_STATS
			sc->tx_underrun++;
#endif
		    } else {
			if (status & TXS_JABBER);
			else	/* TXS_MAX_COLLISION - we shouldn't get here */
			    ++sc->arpcom.ac_if.if_collisions;
		    }
		    ++sc->arpcom.ac_if.if_oerrors;
		    outw(BASE + EP_COMMAND, TX_ENABLE);
		    /*
		     * To have a tx_avail_int but giving the chance to the
		     * Reception
		     */
		    if (sc->arpcom.ac_if.if_snd.ifq_head) {
			outw(BASE + EP_COMMAND, SET_TX_AVAIL_THRESH | 8);
		    }
		}
		outb(BASE + EP_W1_TX_STATUS, 0x0);	/* pops up the next
							 * status */
	    }			/* while */
	}			/* end TX_COMPLETE */
    }
    /* re-enable ints */
    /* outw(BASE + EP_COMMAND, SET_INTR_MASK | S_5_INTS); */

    outw(BASE + EP_COMMAND, C_INTR_LATCH);	/* ACK int Latch */

    if ((status = inw(BASE + EP_STATUS)) & S_5_INTS) 
	goto rescan;
}

void
epread(sc)
    register struct ep_softc *sc;
{
    struct ether_header *eh;
    struct mbuf *top, *mcur, *m;
    int lenthisone;

    short rx_fifo2, status;
    register short delta;
    register short rx_fifo;

    status = inw(BASE + EP_W1_RX_STATUS);

read_again:

    if (status & ERR_RX) {
	++sc->arpcom.ac_if.if_ierrors;
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
	    if (sc->rx_latency < ETHERMTU)
		sc->rx_latency += 16;
	}
	goto out;
    }
    rx_fifo = rx_fifo2 = status & RX_BYTES_MASK;

    if (ep_ftst(F_RX_FIRST)) {
	if (m = sc->mb[sc->next_mb]) {
	    sc->mb[sc->next_mb] = 0;
	    sc->next_mb = (sc->next_mb + 1) % MAX_MBS;
	    m->m_data = m->m_pktdat;
	    m->m_flags = M_PKTHDR;
	} else {
	    MGETHDR(m, M_DONTWAIT, MT_DATA);
	    if (!m)
		goto out;
	}
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
	if (ep_ftst(F_RX_TRAILER))
	    /* We don't read the trailer */
	    rx_fifo -= sizeof(struct ether_header);
    }

    /* Reads what is left in the RX FIFO */
    while (rx_fifo > 0) {
	lenthisone = min(rx_fifo, M_TRAILINGSPACE(m));
	if (lenthisone == 0) {	/* no room in this one */
	    mcur = m;
	    if (m = sc->mb[sc->next_mb]) {
		sc->mb[sc->next_mb] = 0;
		sc->next_mb = (sc->next_mb + 1) % MAX_MBS;
	    } else {
		MGET(m, M_DONTWAIT, MT_DATA);
		if (!m)
		    goto out;
	    }

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

    if (ep_ftst(F_RX_TRAILER)) {/* reads the trailer */
	if (m = sc->mb[sc->next_mb]) {
	    sc->mb[sc->next_mb] = 0;
	    sc->next_mb = (sc->next_mb + 1) % MAX_MBS;
	    m->m_data = m->m_pktdat;
	    m->m_flags = M_PKTHDR;
	} else {
	    MGETHDR(m, M_DONTWAIT, MT_DATA);
	    if (!m)
		goto out;
	}
	insw(BASE + EP_W1_RX_PIO_RD_1, mtod(m, caddr_t),
	     sizeof(struct ether_header));
	m->m_len = sizeof(struct ether_header);
	m->m_next = top;
	sc->top = top = m;
	/* XXX Accomodate for type and len from beginning of trailer */
	sc->cur_len -= (2 * sizeof(u_short));
	ep_frst(F_RX_TRAILER);
	goto all_pkt;
    }

    if (status & ERR_RX_INCOMPLETE) {	/* we haven't received the complete
					 * packet */
	sc->mcur = m;
#ifdef EP_LOCAL_STATS
	sc->rx_no_first++;	/* to know how often we come here */
#endif
	/*
	 * Re-compute rx_latency, the factor used is 1/4 to go up and 1/32 to
	 * go down
	 */
	delta = rx_fifo2 - sc->rx_early_thresh;	/* last latency seen LLS */
	delta -= sc->rx_latency;/* LLS - estimated_latency */
	if (delta >= 0)
	    sc->rx_latency += (delta / 4);
	else
	    sc->rx_latency += (delta / 32);
	ep_frst(F_RX_FIRST);
	if (!((status = inw(BASE + EP_W1_RX_STATUS)) & ERR_RX_INCOMPLETE)) {
	    /* we see if by now, the packet has completly arrived */
	    goto read_again;
	}
	/* compute rx_early_threshold */
	delta = (sc->rx_avg_pkt - sc->cur_len - sc->rx_latency - 16) & ~3;
	if (delta < MIN_RX_EARLY_THRESHL)
	    delta = MIN_RX_EARLY_THRESHL;

	outw(BASE + EP_COMMAND, SET_RX_EARLY_THRESH |
	     (sc->rx_early_thresh = delta));
	return;
    }
all_pkt:
    outw(BASE + EP_COMMAND, RX_DISCARD_TOP_PACK);
    /*
     * recompute average packet's length, the factor used is 1/8 to go down
     * and 1/32 to go up
     */
    delta = sc->cur_len - sc->rx_avg_pkt;
    if (delta > 0)
	sc->rx_avg_pkt += (delta / 32);
    else
	sc->rx_avg_pkt += (delta / 8);
    delta = (sc->rx_avg_pkt - sc->rx_latency - 16) & ~3;
    if (delta < MIN_RX_EARLY_THRESHF)
	delta = MIN_RX_EARLY_THRESHF;
    sc->rx_early_thresh = delta;
    ++sc->arpcom.ac_if.if_ipackets;
    ep_fset(F_RX_FIRST);
    ep_frst(F_RX_TRAILER);
    top->m_pkthdr.rcvif = &sc->arpcom.ac_if;
    top->m_pkthdr.len = sc->cur_len;

#if NBPFILTER > 0
    if (sc->bpf) {
	bpf_mtap(sc->bpf, top);

	/*
	 * Note that the interface cannot be in promiscuous mode if there are
	 * no BPF listeners.  And if we are in promiscuous mode, we have to
	 * check if this packet is really ours.
	 */
	eh = mtod(top, struct ether_header *);
	if ((sc->arpcom.ac_if.if_flags & IFF_PROMISC) &&
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
	    ep_frst(F_RX_TRAILER);
#ifdef EP_LOCAL_STATS
	    sc->rx_bpf_disc++;
#endif
	    while (inw(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS);
	    outw(BASE + EP_COMMAND, SET_RX_EARLY_THRESH | delta);
	    return;
	}
    }
#endif

    eh = mtod(top, struct ether_header *);
    m_adj(top, sizeof(struct ether_header));
    ether_input(&sc->arpcom.ac_if, eh, top);
    if (!sc->mb[sc->next_mb])
	epmbuffill((caddr_t) sc, 0);
    sc->top = 0;
    while (inw(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS);
    outw(BASE + EP_COMMAND, SET_RX_EARLY_THRESH | delta);
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
    delta = (sc->rx_avg_pkt - sc->rx_latency - 16) & ~3;
    if (delta < MIN_RX_EARLY_THRESHF)
	delta = MIN_RX_EARLY_THRESHF;
    ep_fset(F_RX_FIRST);
    ep_frst(F_RX_TRAILER);
    while (inw(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS);
    outw(BASE + EP_COMMAND, SET_RX_EARLY_THRESH |
	 (sc->rx_early_thresh = delta));
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
    struct ep_softc *sc = &ep_softc[ifp->if_unit];
    struct ifreq *ifr = (struct ifreq *) data;
    int s, error = 0;

    switch (cmd) {
      case SIOCSIFADDR:
	ifp->if_flags |= IFF_UP;
	switch (ifa->ifa_addr->sa_family) {
#ifdef INET
	  case AF_INET:
	    epinit(ifp->if_unit);	/* before arpwhohas */
	    arp_ifinit((struct arpcom *)ifp, ifa);
	    break;
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
		epinit(ifp->if_unit);
		break;
	    }
#endif
	  default:
	    epinit(ifp->if_unit);
	    break;
	}
	break;
      case SIOCSIFFLAGS:
	if ((ifp->if_flags & IFF_UP) == 0 && ifp->if_flags & IFF_RUNNING) {
	    ifp->if_flags &= ~IFF_RUNNING;
	    epstop(ifp->if_unit);
	    epmbufempty(sc);
	    break;
	}
	if (ifp->if_flags & IFF_UP && (ifp->if_flags & IFF_RUNNING) == 0)
	    epinit(ifp->if_unit);

	if ( (ifp->if_flags & IFF_PROMISC) &&  !ep_ftst(F_PROMISC) ) {
	    ep_fset(F_PROMISC);
	    epinit(ifp->if_unit);
	    }
	else if( !(ifp->if_flags & IFF_PROMISC) && ep_ftst(F_PROMISC) ) {
	    ep_frst(F_PROMISC);
	    epinit(ifp->if_unit);
	    }

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

      default:
		error = EINVAL;
    }
    return (error);
}

void
epreset(unit)
    int unit;
{
    int s = splimp();

    epstop(unit);
    epinit(unit);
    splx(s);
}

void
epwatchdog(unit)
    int unit;
{
    struct ep_softc *sc = &ep_softc[unit];

    log(LOG_ERR, "ep%d: watchdog\n", unit);
    ++sc->arpcom.ac_if.if_oerrors;

    epreset(unit);
}

void
epstop(unit)
    int unit;
{
    struct ep_softc *sc = &ep_softc[unit];

    outw(BASE + EP_COMMAND, RX_DISABLE);
    outw(BASE + EP_COMMAND, RX_DISCARD_TOP_PACK);
    while (inw(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS);
    outw(BASE + EP_COMMAND, TX_DISABLE);
    outw(BASE + EP_COMMAND, STOP_TRANSCEIVER);
    outw(BASE + EP_COMMAND, RX_RESET);
    outw(BASE + EP_COMMAND, TX_RESET);
    outw(BASE + EP_COMMAND, C_INTR_LATCH);
    outw(BASE + EP_COMMAND, SET_RD_0_MASK);
    outw(BASE + EP_COMMAND, SET_INTR_MASK);
    outw(BASE + EP_COMMAND, SET_RX_FILTER);
}


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

/*
 * We suppose this is always called inside a splimp(){...}splx() region
 */
static void
epmbuffill(sp, dummy_arg)
    caddr_t sp;
    int dummy_arg;
{
    struct ep_softc *sc = (struct ep_softc *) sp;
    int i;

    i = sc->last_mb;
    do {
	if (sc->mb[i] == NULL)
	    MGET(sc->mb[i], M_DONTWAIT, MT_DATA);
	if (sc->mb[i] == NULL)
	    break;
	i = (i + 1) % MAX_MBS;
    } while (i != sc->next_mb);
    sc->last_mb = i;
}

static void
epmbufempty(sc)
    struct ep_softc *sc;
{
    int s, i;

    s = splimp();
    for (i = 0; i < MAX_MBS; i++) {
	if (sc->mb[i]) {
	    m_freem(sc->mb[i]);
	    sc->mb[i] = NULL;
	}
    }
    sc->last_mb = sc->next_mb = 0;
    splx(s);
}

#endif				/* NEP > 0 */
