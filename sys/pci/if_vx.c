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
 */

/*
 * Created from if_ep.c driver by Fred Gray (fgray@rice.edu) to support
 * the 3c590 family.
 */

/*
 *	Modified from the FreeBSD 1.1.5.1 version by:
 *		 	Andres Vega Garcia
 *			INRIA - Sophia Antipolis, France
 *			avega@sophia.inria.fr
 */

/*
 *  Promiscuous mode added and interrupt logic slightly changed
 *  to reduce the number of adapter failures. Transceiver select
 *  logic changed to use value from EEPROM. Autoconfiguration
 *  features added.
 *  Done by:
 *          Serge Babkin
 *          Chelindbank (Chelyabinsk, Russia)
 *          babkin@hq.icb.chel.su
 */

#include "vx.h"
#if NVX > 0

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

#if defined(__FreeBSD__)
#include <machine/clock.h>
#endif

#include <pci/pcivar.h>
#include <pci/if_vxreg.h>

static int eeprom_rdy __P((int unit));
static int get_e __P((int unit, int offset));
static int vxioctl __P((struct ifnet * ifp, int, caddr_t));
static void vxmbuffill __P((caddr_t, int));
static void vxmbufempty __P((struct vx_softc *));

static void vxinit __P((int));
static void vxintr __P((int));
static void vxread __P((struct vx_softc *));
static void vxreset __P((int));
static void vxstart __P((struct ifnet *));
static void vxstop __P((int));
static void vxwatchdog __P((struct ifnet *));

static struct vx_softc vx_softc[NVX];

#define vx_ftst(f) (sc->stat&(f))
#define vx_fset(f) (sc->stat|=(f))
#define vx_frst(f) (sc->stat&=~(f))

static int
eeprom_rdy(unit)
	int unit;
{
    struct vx_softc *sc = &vx_softc[unit];
    int i;

    for (i = 0; is_eeprom_busy(BASE) && i < MAX_EEPROMBUSY; i++);
    if (i >= MAX_EEPROMBUSY) {
	printf("vx%d: eeprom failed to come ready.\n", unit);
	return (0);
    }
    return (1);
}

/*
 * get_e: gets a 16 bits word from the EEPROM. we must have set the window
 * before
 */
static int
get_e(unit, offset)
    int unit;
    int offset;
{
    struct vx_softc *sc = &vx_softc[unit];

    if (!eeprom_rdy(unit))
	return (0xffff);
    outw(BASE + VX_W0_EEPROM_COMMAND, EEPROM_CMD_RD | offset);
    if (!eeprom_rdy(unit))
	return (0xffff);
    return (inw(BASE + VX_W0_EEPROM_DATA));
}

static int
vx_pci_shutdown(
	struct kern_devconf * const kdc,
	int force)
{
   vxreset(kdc->kdc_unit); 
   dev_detach(kdc);
   return 0;
}

static char*
vx_pci_probe(
	pcici_t config_id,
	pcidi_t device_id)
{
   if(device_id == 0x590010b7ul)
      return "3Com 3c590 EtherLink III PCI";
   return NULL;
}

static char *vx_conn_type[] = {"UTP", "AUI", "???", "BNC"};

static void
vx_pci_attach(
	pcici_t config_id,
	int unit)
{
    struct vx_softc *sc;
    struct ifnet *ifp;
    u_short i, j, *p;
    struct ifaddr *ifa;
    struct sockaddr_dl *sdl;

    if (unit >= NVX) {
       printf("vx%d: not configured; kernel is built for only %d device%s.\n",
          unit, NVX, NVX == 1 ? "" : "s"); 
       return;
    }

    sc = &vx_softc[unit];
    ifp = &sc->arpcom.ac_if;

    sc->vx_io_addr = pci_conf_read(config_id, 0x10) & 0xfffffff0;

    outw(VX_COMMAND, GLOBAL_RESET);
    DELAY(1000);

    sc->vx_connectors = 0;
    i = pci_conf_read(config_id, 0x48);
    j = inw(BASE + VX_W3_INTERNAL_CFG) >> INTERNAL_CONNECTOR_BITS;
    if (i & RS_AUI) {
	printf("aui");
	sc->vx_connectors |= AUI;
    }
    if (i & RS_BNC) {
	if (sc->vx_connectors)
	    printf("/");
	printf("bnc");
	sc->vx_connectors |= BNC;
    }
    if (i & RS_UTP) {
	if (sc->vx_connectors)
	    printf("/");
	printf("utp");
	sc->vx_connectors |= UTP;
    }
    if (!(sc->vx_connectors & 7))
	printf("no connectors!");
    else
	printf("[*%s*]", vx_conn_type[j]);


    /*
     * Read the station address from the eeprom
     */
    p = (u_short *) & sc->arpcom.ac_enaddr;
    for (i = 0; i < 3; i++) {
	GO_WINDOW(0);
	p[i] = htons(get_e(unit, i));
	GO_WINDOW(2);
	outw(BASE + VX_W2_ADDR_0 + (i * 2), ntohs(p[i]));
    }
    printf(" address %s\n", ether_sprintf(sc->arpcom.ac_enaddr));

    /*
     * Check for receive overrun anomaly in the first revision of the
     * adapters.
     */
    if(!(get_e(unit, EEPROM_SOFT_INFO_2) & NO_RX_OVN_ANOMALY)) {
 	printf("Warning! Defective early revision adapter!\n");
    }

    ifp->if_unit = unit;
    ifp->if_name = "vx";
    ifp->if_mtu = ETHERMTU;
    ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX /*| IFF_NOTRAILERS*/;
    ifp->if_output = ether_output;
    ifp->if_start = vxstart;
    ifp->if_ioctl = vxioctl;
    ifp->if_watchdog = vxwatchdog;

    if_attach(ifp);

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
#ifdef VX_LOCAL_STATS
    sc->rx_no_first = sc->rx_no_mbuf =
    sc->rx_bpf_disc = sc->rx_overrunf = sc->rx_overrunl =
    sc->tx_underrun = 0;
#endif
    vx_fset(F_RX_FIRST);
    sc->top = sc->mcur = 0;

#if NBPFILTER > 0
    bpfattach(&sc->bpf, ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif

   pci_map_int(config_id, (void *) vxintr, (void *) unit, &net_imask);
}

static u_long vx_pci_count;

static struct pci_device vxdevice = {
    "vx",
    vx_pci_probe,
    vx_pci_attach,
    &vx_pci_count,
    vx_pci_shutdown,
};

DATA_SET (pcidevice_set, vxdevice);

/*
 * The order in here seems important. Otherwise we may not receive
 * interrupts. ?!
 */
static void
vxinit(unit)
    int unit;
{
    register struct vx_softc *sc = &vx_softc[unit];
    register struct ifnet *ifp = &sc->arpcom.ac_if;
    int s, i, j;

	/*
    if (ifp->if_addrlist == (struct ifaddr *) 0)
	return;
	*/

    s = splimp();
    while (inw(BASE + VX_STATUS) & S_COMMAND_IN_PROGRESS);

    GO_WINDOW(0);
    outw(BASE + VX_COMMAND, STOP_TRANSCEIVER);
    GO_WINDOW(4);
    outw(BASE + VX_W4_MEDIA_TYPE, DISABLE_UTP);

    GO_WINDOW(2);

    /* Reload the ether_addr. */
    for (i = 0; i < 6; i++)
	outb(BASE + VX_W2_ADDR_0 + i, sc->arpcom.ac_enaddr[i]);

    outw(BASE + VX_COMMAND, RX_RESET);
    outw(BASE + VX_COMMAND, TX_RESET);

    /* Window 1 is operating window */
    GO_WINDOW(1);
    for (i = 0; i < 31; i++)
	inb(BASE + VX_W1_TX_STATUS);

    /* get rid of stray intr's */
    outw(BASE + VX_COMMAND, ACK_INTR | 0xff);

    outw(BASE + VX_COMMAND, SET_RD_0_MASK | S_5_INTS);

    outw(BASE + VX_COMMAND, SET_INTR_MASK | S_5_INTS);

	if(ifp->if_flags & IFF_PROMISC)
		outw(BASE + VX_COMMAND, SET_RX_FILTER | FIL_INDIVIDUAL |
		 FIL_GROUP | FIL_BRDCST | FIL_ALL);
	else
		outw(BASE + VX_COMMAND, SET_RX_FILTER | FIL_INDIVIDUAL |
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

    if(ifp->if_flags & IFF_LINK0 && sc->vx_connectors & AUI) {
	/* nothing */
    } else if(ifp->if_flags & IFF_LINK1 && sc->vx_connectors & BNC) {
	outw(BASE + VX_COMMAND, START_TRANSCEIVER);
	DELAY(1000);
    } else if(ifp->if_flags & IFF_LINK2 && sc->vx_connectors & UTP) {
	GO_WINDOW(4);
	outw(BASE + VX_W4_MEDIA_TYPE, ENABLE_UTP);
	GO_WINDOW(1);
    } else {
	GO_WINDOW(0);
        j = inw(BASE + VX_W3_INTERNAL_CFG) >> INTERNAL_CONNECTOR_BITS;
	GO_WINDOW(1);
	switch(j) {
	    case ACF_CONNECTOR_UTP:
		if(sc->vx_connectors & UTP) {
		    GO_WINDOW(4);
		    outw(BASE + VX_W4_MEDIA_TYPE, ENABLE_UTP);
		    GO_WINDOW(1);
		}
		break;
	    case ACF_CONNECTOR_BNC:
		if(sc->vx_connectors & BNC) {
		    outw(BASE + VX_COMMAND, START_TRANSCEIVER);
		    DELAY(1000);
		}
		break;
	    case ACF_CONNECTOR_AUI:
		/* nothing to do */
		break;
	    default:
		printf("vx%d: strange connector type in EEPROM: assuming AUI\n",
		    unit);
		break;
	}
    }

    outw(BASE + VX_COMMAND, RX_ENABLE);
    outw(BASE + VX_COMMAND, TX_ENABLE);

    ifp->if_flags |= IFF_RUNNING;
    ifp->if_flags &= ~IFF_OACTIVE;	/* just in case */

    sc->tx_rate = TX_INIT_RATE;
    sc->tx_counter = 0;
    sc->rx_latency = RX_INIT_LATENCY;
    sc->rx_early_thresh = RX_INIT_EARLY_THRESH;
#ifdef VX_LOCAL_STATS
    sc->rx_no_first = sc->rx_no_mbuf =
	sc->rx_bpf_disc = sc->rx_overrunf = sc->rx_overrunl =
	sc->tx_underrun = 0;
#endif
    vx_fset(F_RX_FIRST);
    vx_frst(F_RX_TRAILER);
    if (sc->top) {
	m_freem(sc->top);
	sc->top = sc->mcur = 0;
    }
    outw(BASE + VX_COMMAND, SET_RX_EARLY_THRESH | sc->rx_early_thresh);

    /*
     * These clever computations look very interesting
     * but the fixed threshold gives near no output errors
     * and if it as low as 16 bytes it gives the max. throughput.
     * We think that processor is anyway quicker than Ethernet
	 * (and this should be true for any 386 and higher)
     */

    outw(BASE + VX_COMMAND, SET_TX_START_THRESH | 16);

    /*
     * Store up a bunch of mbuf's for use later. (MAX_MBS). First we free up
     * any that we had in case we're being called from intr or somewhere
     * else.
     */
    sc->last_mb = 0;
    sc->next_mb = 0;
    vxmbuffill((caddr_t) sc, 0);

    vxstart(ifp);

    splx(s);
}

static const char padmap[] = {0, 3, 2, 1};

static void
vxstart(ifp)
    struct ifnet *ifp;
{
    register struct vx_softc *sc = &vx_softc[ifp->if_unit];
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
    if (inw(BASE + VX_W1_FREE_TX) < len + pad + 4) {
	/* no room in FIFO */
	outw(BASE + VX_COMMAND, SET_TX_AVAIL_THRESH | (len + pad + 4));
	sc->arpcom.ac_if.if_flags |= IFF_OACTIVE;
	splx(s);
	return;
    }
    IF_DEQUEUE(&sc->arpcom.ac_if.if_snd, m);

    outw(BASE + VX_W1_TX_PIO_WR_1, len);
    outw(BASE + VX_W1_TX_PIO_WR_1, 0x0);	/* Second dword meaningless */

    /* compute the Tx start threshold for this packet */
    sc->tx_start_thresh = len =
	(((len * (64 - sc->tx_rate)) >> 6) & ~3) + 16;
#if 0
    /*
     * The following string does something strange with the card and
     * we get a lot of output errors due to it so it's commented out
     * and we use fixed threshold (see above)
     */

    outw(BASE + VX_COMMAND, SET_TX_START_THRESH | len);
#endif

    for (top = m; m != 0; m = m->m_next)
	if(vx_ftst(F_ACCESS_32_BITS)) {
	    outsl(BASE + VX_W1_TX_PIO_WR_1, mtod(m, caddr_t),
		  m->m_len / 4);
	    if (m->m_len & 3)
		outsb(BASE + VX_W1_TX_PIO_WR_1,
		      mtod(m, caddr_t) + m->m_len / 4,
		      m->m_len & 3);
	} else {
	    outsw(BASE + VX_W1_TX_PIO_WR_1, mtod(m, caddr_t), m->m_len / 2);
	    if (m->m_len & 1)
		outb(BASE + VX_W1_TX_PIO_WR_1,
		     *(mtod(m, caddr_t) + m->m_len - 1));
	}

    while (pad--)
	outb(BASE + VX_W1_TX_PIO_WR_1, 0);	/* Padding */

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
    if (inw(BASE + VX_W1_RX_STATUS) & RX_BYTES_MASK) {
	/*
	 * we check if we have packets left, in that case we prepare to come
	 * back later
	 */
	if (sc->arpcom.ac_if.if_snd.ifq_head) {
	    outw(BASE + VX_COMMAND, SET_TX_AVAIL_THRESH |
		 sc->tx_start_thresh);
	}
	splx(s);
	return;
    }
    goto startagain;
}

static void
vxintr(unit)
    int unit;
{
    register int status;
    register struct vx_softc *sc = &vx_softc[unit];
    int x;

    x=splbio();

    outw(BASE + VX_COMMAND, SET_INTR_MASK); /* disable all Ints */

rescan:

    while ((status = inw(BASE + VX_STATUS)) & S_5_INTS) {

	/* first acknowledge all interrupt sources */
	outw(BASE + VX_COMMAND, ACK_INTR | (status & S_MASK));

	if (status & (S_RX_COMPLETE | S_RX_EARLY)) {
	    vxread(sc);
	    continue;
	}
	if (status & S_TX_AVAIL) {
	    /* we need ACK */
	    sc->arpcom.ac_if.if_flags &= ~IFF_OACTIVE;
	    GO_WINDOW(1);
	    inw(BASE + VX_W1_FREE_TX);
	    vxstart(&sc->arpcom.ac_if);
	}
	if (status & S_CARD_FAILURE) {
#ifdef VX_LOCAL_STATS
	    printf("\nvx%d:\n\tStatus: %x\n", unit, status);
	    GO_WINDOW(4);
	    printf("\tFIFO Diagnostic: %x\n", inw(BASE + VX_W4_FIFO_DIAG));
	    printf("\tStat: %x\n", sc->stat);
	    printf("\tIpackets=%d, Opackets=%d\n",
		sc->arpcom.ac_if.if_ipackets, sc->arpcom.ac_if.if_opackets);
	    printf("\tNOF=%d, NOMB=%d, BPFD=%d, RXOF=%d, RXOL=%d, TXU=%d\n",
		   sc->rx_no_first, sc->rx_no_mbuf, sc->rx_bpf_disc, sc->rx_overrunf,
		   sc->rx_overrunl, sc->tx_underrun);
#else
	    printf("vx%d: Status: %x\n", unit, status);
#endif
	    vxinit(unit);
	    splx(x);
	    return;
	}
	if (status & S_TX_COMPLETE) {
	    /* we  need ACK. we do it at the end */
	    /*
	     * We need to read TX_STATUS until we get a 0 status in order to
	     * turn off the interrupt flag.
	     */
	    while ((status = inb(BASE + VX_W1_TX_STATUS)) & TXS_COMPLETE) {
		if (status & TXS_SUCCES_INTR_REQ);
		else if (status & (TXS_UNDERRUN | TXS_JABBER | TXS_MAX_COLLISION)) {
		    outw(BASE + VX_COMMAND, TX_RESET);
		    if (status & TXS_UNDERRUN) {
			if (sc->tx_rate > 1) {
			    sc->tx_rate--;	/* Actually in steps of 1/64 */
			    sc->tx_counter = 0;	/* We reset it */
			}
#ifdef VX_LOCAL_STATS
			sc->tx_underrun++;
#endif
		    } else {
			if (status & TXS_JABBER);
			else	/* TXS_MAX_COLLISION - we shouldn't get here */
			    ++sc->arpcom.ac_if.if_collisions;
		    }
		    ++sc->arpcom.ac_if.if_oerrors;
		    outw(BASE + VX_COMMAND, TX_ENABLE);
		    /*
		     * To have a tx_avail_int but giving the chance to the
		     * Reception
		     */
		    if (sc->arpcom.ac_if.if_snd.ifq_head) {
			outw(BASE + VX_COMMAND, SET_TX_AVAIL_THRESH | 8);
		    }
		}
		outb(BASE + VX_W1_TX_STATUS, 0x0);	/* pops up the next
							 * status */
	    }			/* while */
	    sc->arpcom.ac_if.if_flags &= ~IFF_OACTIVE;
	    GO_WINDOW(1);
	    inw(BASE + VX_W1_FREE_TX);
	    vxstart(&sc->arpcom.ac_if);
	}			/* end TX_COMPLETE */
    }

    outw(BASE + VX_COMMAND, C_INTR_LATCH);	/* ACK int Latch */

    if ((status = inw(BASE + VX_STATUS)) & S_5_INTS)
	goto rescan;

    /* re-enable Ints */
    outw(BASE + VX_COMMAND, SET_INTR_MASK | S_5_INTS);

    splx(x);
}

static void
vxread(sc)
    register struct vx_softc *sc;
{
    struct ether_header *eh;
    struct mbuf *top, *mcur, *m;
    int lenthisone;

    short rx_fifo2, status;
    register short delta;
    register short rx_fifo;

    status = inw(BASE + VX_W1_RX_STATUS);

read_again:

    if (status & ERR_RX) {
	++sc->arpcom.ac_if.if_ierrors;
	if (status & ERR_RX_OVERRUN) {
	    /*
	     * we can think the rx latency is actually greather than we
	     * expect
	     */
#ifdef VX_LOCAL_STATS
	    if (vx_ftst(F_RX_FIRST))
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

    if (vx_ftst(F_RX_FIRST)) {
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
	insw(BASE + VX_W1_RX_PIO_RD_1,
	     mtod(top, caddr_t), sizeof(struct ether_header) / 2);
	top->m_len = sizeof(struct ether_header);
	rx_fifo -= sizeof(struct ether_header);
	sc->cur_len = rx_fifo2;
    } else {
	/* come here if we didn't have a complete packet last time */
	top = sc->top;
	m = sc->mcur;
	sc->cur_len += rx_fifo2;
	if (vx_ftst(F_RX_TRAILER))
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
	if (vx_ftst(F_ACCESS_32_BITS)) { /* default for EISA configured cards*/
	    insl(BASE + VX_W1_RX_PIO_RD_1, mtod(m, caddr_t) + m->m_len,
		 lenthisone / 4);
	    m->m_len += (lenthisone & ~3);
	    if (lenthisone & 3)
		insb(BASE + VX_W1_RX_PIO_RD_1,
		     mtod(m, caddr_t) + m->m_len,
		     lenthisone & 3);
	    m->m_len += (lenthisone & 3);
	} else {
	    insw(BASE + VX_W1_RX_PIO_RD_1, mtod(m, caddr_t) + m->m_len,
		 lenthisone / 2);
	    m->m_len += lenthisone;
	    if (lenthisone & 1)
		*(mtod(m, caddr_t) + m->m_len - 1) = inb(BASE + VX_W1_RX_PIO_RD_1);
	}
	rx_fifo -= lenthisone;
    }

    if (vx_ftst(F_RX_TRAILER)) {/* reads the trailer */
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
	insw(BASE + VX_W1_RX_PIO_RD_1, mtod(m, caddr_t),
	     sizeof(struct ether_header));
	m->m_len = sizeof(struct ether_header);
	m->m_next = top;
	sc->top = top = m;
	/* XXX Accomodate for type and len from beginning of trailer */
	sc->cur_len -= (2 * sizeof(u_short));
	vx_frst(F_RX_TRAILER);
	goto all_pkt;
    }

    if (status & ERR_RX_INCOMPLETE) {	/* we haven't received the complete
					 * packet */
	sc->mcur = m;
#ifdef VX_LOCAL_STATS
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
	vx_frst(F_RX_FIRST);
	if (!((status = inw(BASE + VX_W1_RX_STATUS)) & ERR_RX_INCOMPLETE)) {
	    /* we see if by now, the packet has completly arrived */
	    goto read_again;
	}
	/* compute rx_early_threshold */
	delta = (sc->rx_avg_pkt - sc->cur_len - sc->rx_latency - 16) & ~3;
	if (delta < MIN_RX_EARLY_THRESHL)
	    delta = MIN_RX_EARLY_THRESHL;

	outw(BASE + VX_COMMAND, SET_RX_EARLY_THRESH |
	     (sc->rx_early_thresh = delta));
	return;
    }
all_pkt:
    outw(BASE + VX_COMMAND, RX_DISCARD_TOP_PACK);
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
    vx_fset(F_RX_FIRST);
    vx_frst(F_RX_TRAILER);
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
	    vx_fset(F_RX_FIRST);
	    vx_frst(F_RX_TRAILER);
#ifdef VX_LOCAL_STATS
	    sc->rx_bpf_disc++;
#endif
	    while (inw(BASE + VX_STATUS) & S_COMMAND_IN_PROGRESS);
	    outw(BASE + VX_COMMAND, SET_RX_EARLY_THRESH | delta);
	    return;
	}
    }
#endif

    eh = mtod(top, struct ether_header *);
    m_adj(top, sizeof(struct ether_header));
    ether_input(&sc->arpcom.ac_if, eh, top);
    if (!sc->mb[sc->next_mb])
	vxmbuffill((caddr_t) sc, 0);
    sc->top = 0;
    while (inw(BASE + VX_STATUS) & S_COMMAND_IN_PROGRESS);
    outw(BASE + VX_COMMAND, SET_RX_EARLY_THRESH | delta);
    return;

out:
    outw(BASE + VX_COMMAND, RX_DISCARD_TOP_PACK);
    if (sc->top) {
	m_freem(sc->top);
	sc->top = 0;
#ifdef VX_LOCAL_STATS
	sc->rx_no_mbuf++;
#endif
    }
    delta = (sc->rx_avg_pkt - sc->rx_latency - 16) & ~3;
    if (delta < MIN_RX_EARLY_THRESHF)
	delta = MIN_RX_EARLY_THRESHF;
    vx_fset(F_RX_FIRST);
    vx_frst(F_RX_TRAILER);
    while (inw(BASE + VX_STATUS) & S_COMMAND_IN_PROGRESS);
    outw(BASE + VX_COMMAND, SET_RX_EARLY_THRESH |
	 (sc->rx_early_thresh = delta));
}

/*
 * Look familiar?
 */
static int
vxioctl(ifp, cmd, data)
    register struct ifnet *ifp;
    int cmd;
    caddr_t data;
{
    register struct ifaddr *ifa = (struct ifaddr *) data;
    struct vx_softc *sc = &vx_softc[ifp->if_unit];
    struct ifreq *ifr = (struct ifreq *) data;
    int s, error = 0;

    s = splimp();

    switch (cmd) {
      case SIOCSIFADDR:
	ifp->if_flags |= IFF_UP;
	switch (ifa->ifa_addr->sa_family) {
#ifdef INET
	  case AF_INET:
	    vxinit(ifp->if_unit);	/* before arpwhohas */
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
		vxinit(ifp->if_unit);
		break;
	    }
#endif
	  default:
	    vxinit(ifp->if_unit);
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
	    vxstop(ifp->if_unit);
	    vxmbufempty(sc);
	    break;
	} else {
	    /* reinitialize card on any parameter change */
	    vxinit(ifp->if_unit);
	    break;
	}

	/* NOTREACHED */

	if (ifp->if_flags & IFF_UP && (ifp->if_flags & IFF_RUNNING) == 0)
	    vxinit(ifp->if_unit);

	if ( (ifp->if_flags & IFF_PROMISC) &&  !vx_ftst(F_PROMISC) ) {
	    vx_fset(F_PROMISC);
	    vxinit(ifp->if_unit);
	    }
	else if( !(ifp->if_flags & IFF_PROMISC) && vx_ftst(F_PROMISC) ) {
	    vx_frst(F_PROMISC);
	    vxinit(ifp->if_unit);
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

    splx(s);

    return (error);
}

static void
vxreset(unit)
    int unit;
{
    int s = splimp();

    vxstop(unit);
    vxinit(unit);
    splx(s);
}

static void
vxwatchdog(ifp)
    struct ifnet *ifp;
{
    /*
    printf("vx: watchdog\n");

    log(LOG_ERR, "vx%d: watchdog\n", ifp->if_unit);
    ifp->if_oerrors++;
    */

    /* vxreset(ifp->if_unit); */
    ifp->if_flags &= ~IFF_OACTIVE;
    vxstart(ifp);
    vxintr(ifp->if_unit);
}

static void
vxstop(unit)
    int unit;
{
    struct vx_softc *sc = &vx_softc[unit];

    outw(BASE + VX_COMMAND, RX_DISABLE);
    outw(BASE + VX_COMMAND, RX_DISCARD_TOP_PACK);
    while (inw(BASE + VX_STATUS) & S_COMMAND_IN_PROGRESS);
    outw(BASE + VX_COMMAND, TX_DISABLE);
    outw(BASE + VX_COMMAND, STOP_TRANSCEIVER);
    outw(BASE + VX_COMMAND, RX_RESET);
    outw(BASE + VX_COMMAND, TX_RESET);
    outw(BASE + VX_COMMAND, C_INTR_LATCH);
    outw(BASE + VX_COMMAND, SET_RD_0_MASK);
    outw(BASE + VX_COMMAND, SET_INTR_MASK);
    outw(BASE + VX_COMMAND, SET_RX_FILTER);
}

/*
 * We suppose this is always called inside a splimp(){...}splx() region
 */
static void
vxmbuffill(sp, dummy_arg)
    caddr_t sp;
    int dummy_arg;
{
    struct vx_softc *sc = (struct vx_softc *) sp;
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
vxmbufempty(sc)
    struct vx_softc *sc;
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

#endif				/* NVX > 0 */
