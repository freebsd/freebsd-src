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
 *
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

#include <net/if.h>

#include <net/ethernet.h>
#include <net/if_arp.h>

#include <machine/bus_pio.h>
#include <machine/bus.h>

#include <sys/bus.h>

#include <net/bpf.h>

#include <dev/vx/if_vxreg.h>
#include <dev/vx/if_vxvar.h>

#define ETHER_MAX_LEN	1518
#define ETHER_ADDR_LEN	6
#define ETHER_ALIGN 	2

static struct connector_entry {
  int bit;
  char *name;
} conn_tab[VX_CONNECTORS] = {
#define CONNECTOR_UTP	0
  { 0x08, "utp"},
#define CONNECTOR_AUI	1
  { 0x20, "aui"},
/* dummy */
  { 0, "???"},
#define CONNECTOR_BNC	3
  { 0x10, "bnc"},
#define CONNECTOR_TX	4
  { 0x02, "tx"},
#define CONNECTOR_FX	5
  { 0x04, "fx"},
#define CONNECTOR_MII	6
  { 0x40, "mii"},
  { 0, "???"}
};

/* int vxattach(struct vx_softc *); */
static void vxtxstat(struct vx_softc *);
static int vxstatus(struct vx_softc *);
static void vxinit(void *);
static int vxioctl(struct ifnet *, u_long, caddr_t); 
static void vxstart(struct ifnet *ifp);
static void vxwatchdog(struct ifnet *);
static void vxreset(struct vx_softc *);
/* void vxstop(struct vx_softc *); */
static void vxread(struct vx_softc *);
static struct mbuf *vxget(struct vx_softc *, u_int);
static void vxmbuffill(void *);
static void vxmbufempty(struct vx_softc *);
static void vxsetfilter(struct vx_softc *);
static void vxgetlink(struct vx_softc *);
static void vxsetlink(struct vx_softc *);
/* int vxbusyeeprom(struct vx_softc *); */


int
vxattach(dev)
    device_t dev;
{
    struct vx_softc *sc = device_get_softc(dev);
    struct ifnet *ifp = &sc->arpcom.ac_if;
    int i;

    callout_handle_init(&sc->ch);
    GO_WINDOW(0);
    CSR_WRITE_2(sc, VX_COMMAND, GLOBAL_RESET);
    VX_BUSY_WAIT;

    vxgetlink(sc);

    /*
     * Read the station address from the eeprom
     */
    GO_WINDOW(0);
    for (i = 0; i < 3; i++) {
        int x;
        if (vxbusyeeprom(sc))
            return 0;
        CSR_WRITE_2(sc,  VX_W0_EEPROM_COMMAND, EEPROM_CMD_RD
	     | (EEPROM_OEM_ADDR0 + i));
        if (vxbusyeeprom(sc))
            return 0;
        x = CSR_READ_2(sc, VX_W0_EEPROM_DATA);
        sc->arpcom.ac_enaddr[(i << 1)] = x >> 8;
        sc->arpcom.ac_enaddr[(i << 1) + 1] = x;
    }

    if_initname(ifp, device_get_name(dev), device_get_unit(dev));
    ifp->if_mtu = ETHERMTU;
    ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;
    ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST |
	IFF_NEEDSGIANT;
    ifp->if_start = vxstart;
    ifp->if_ioctl = vxioctl;
    ifp->if_init = vxinit;
    ifp->if_watchdog = vxwatchdog;
    ifp->if_softc = sc;

    ether_ifattach(ifp, sc->arpcom.ac_enaddr);

    sc->tx_start_thresh = 20;	/* probably a good starting point. */

    vxstop(sc);

    return 1;
}



/*
 * The order in here seems important. Otherwise we may not receive
 * interrupts. ?!
 */
static void
vxinit(xsc)
	void *xsc;
{
    struct vx_softc *sc = (struct vx_softc *) xsc;
    struct ifnet *ifp = &sc->arpcom.ac_if;
    int i;

    VX_BUSY_WAIT;

    GO_WINDOW(2);

    for (i = 0; i < 6; i++) /* Reload the ether_addr. */
	CSR_WRITE_1(sc,  VX_W2_ADDR_0 + i, sc->arpcom.ac_enaddr[i]);

    CSR_WRITE_2(sc,  VX_COMMAND, RX_RESET);
    VX_BUSY_WAIT;
    CSR_WRITE_2(sc,  VX_COMMAND, TX_RESET);
    VX_BUSY_WAIT;

    GO_WINDOW(1);	/* Window 1 is operating window */
    for (i = 0; i < 31; i++)
	CSR_READ_1(sc,  VX_W1_TX_STATUS);

    CSR_WRITE_2(sc,  VX_COMMAND,SET_RD_0_MASK | S_CARD_FAILURE |
			S_RX_COMPLETE | S_TX_COMPLETE | S_TX_AVAIL);
    CSR_WRITE_2(sc,  VX_COMMAND,SET_INTR_MASK | S_CARD_FAILURE |
			S_RX_COMPLETE | S_TX_COMPLETE | S_TX_AVAIL);

    /*
     * Attempt to get rid of any stray interrupts that occured during
     * configuration.  On the i386 this isn't possible because one may
     * already be queued.  However, a single stray interrupt is
     * unimportant.
     */
    CSR_WRITE_2(sc,  VX_COMMAND, ACK_INTR | 0xff);

    vxsetfilter(sc);
    vxsetlink(sc);

    CSR_WRITE_2(sc,  VX_COMMAND, RX_ENABLE);
    CSR_WRITE_2(sc,  VX_COMMAND, TX_ENABLE);

    vxmbuffill((caddr_t) sc);

    /* Interface is now `running', with no output active. */
    ifp->if_flags |= IFF_RUNNING;
    ifp->if_flags &= ~IFF_OACTIVE;

    /* Attempt to start output, if any. */
    vxstart(ifp);
}

static void
vxsetfilter(sc)
    struct vx_softc *sc;
{
    register struct ifnet *ifp = &sc->arpcom.ac_if;  
    
    GO_WINDOW(1);           /* Window 1 is operating window */
    CSR_WRITE_2(sc,  VX_COMMAND, SET_RX_FILTER | FIL_INDIVIDUAL | FIL_BRDCST |
	 FIL_MULTICAST |
	 ((ifp->if_flags & IFF_PROMISC) ? FIL_PROMISC : 0 ));
}               

static void            
vxgetlink(sc)
    struct vx_softc *sc;
{
    int n, k;

    GO_WINDOW(3);
    sc->vx_connectors = CSR_READ_2(sc, VX_W3_RESET_OPT) & 0x7f;
    for (n = 0, k = 0; k < VX_CONNECTORS; k++) {
      if (sc->vx_connectors & conn_tab[k].bit) {
	if (n > 0) {
	  printf("/");
	}
	printf("%s", conn_tab[k].name);
	n++;
      }
    }
    if (sc->vx_connectors == 0) {
	printf("no connectors!");
	return;
    }
    GO_WINDOW(3);
    sc->vx_connector = (CSR_READ_4(sc,  VX_W3_INTERNAL_CFG) 
			& INTERNAL_CONNECTOR_MASK) 
			>> INTERNAL_CONNECTOR_BITS;
    if (sc->vx_connector & 0x10) {
	sc->vx_connector &= 0x0f;
	printf("[*%s*]", conn_tab[(int)sc->vx_connector].name);
	printf(": disable 'auto select' with DOS util!");
    } else {
	printf("[*%s*]", conn_tab[(int)sc->vx_connector].name);
    }
}

static void            
vxsetlink(sc)
    struct vx_softc *sc;
{       
    register struct ifnet *ifp = &sc->arpcom.ac_if;  
    int i, j, k;
    char *reason, *warning;
    static int prev_flags;
    static char prev_conn = -1;

    if (prev_conn == -1) {
	prev_conn = sc->vx_connector;
    }

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
     * (if present on card or UTP if not).
     */

    i = sc->vx_connector;	/* default in EEPROM */
    reason = "default";
    warning = 0;

    if (ifp->if_flags & IFF_LINK0) {
	if (sc->vx_connectors & conn_tab[CONNECTOR_AUI].bit) {
	    i = CONNECTOR_AUI;
	    reason = "link0";
	} else {
	    warning = "aui not present! (link0)";
	}
    } else if (ifp->if_flags & IFF_LINK1) {
	if (sc->vx_connectors & conn_tab[CONNECTOR_BNC].bit) {
	    i = CONNECTOR_BNC;
	    reason = "link1";
	} else {
	    warning = "bnc not present! (link1)";
	}
    } else if (ifp->if_flags & IFF_LINK2) {
	if (sc->vx_connectors & conn_tab[CONNECTOR_UTP].bit) {
	    i = CONNECTOR_UTP;
	    reason = "link2";
	} else {
	    warning = "utp not present! (link2)";
	}
    } else if ((sc->vx_connectors & conn_tab[(int)sc->vx_connector].bit) == 0) {
	warning = "strange connector type in EEPROM.";
	reason = "forced";
	i = CONNECTOR_UTP;
    }

    /* Avoid unnecessary message. */
    k = (prev_flags ^ ifp->if_flags) & (IFF_LINK0 | IFF_LINK1 | IFF_LINK2);
    if ((k != 0) || (prev_conn != i)) {
	if (warning != 0) {
	    printf("vx%d: warning: %s\n", sc->unit, warning);
	}
	printf("vx%d: selected %s. (%s)\n",
	       sc->unit, conn_tab[i].name, reason);
    }

    /* Set the selected connector. */
    GO_WINDOW(3);
    j = CSR_READ_4(sc,  VX_W3_INTERNAL_CFG) & ~INTERNAL_CONNECTOR_MASK;
    CSR_WRITE_4(sc,  VX_W3_INTERNAL_CFG, j | (i <<INTERNAL_CONNECTOR_BITS));

    /* First, disable all. */
    CSR_WRITE_2(sc,  VX_COMMAND, STOP_TRANSCEIVER);
    DELAY(800);
    GO_WINDOW(4);
    CSR_WRITE_2(sc,  VX_W4_MEDIA_TYPE, 0);

    /* Second, enable the selected one. */
    switch(i) {
      case CONNECTOR_UTP:
	GO_WINDOW(4);
	CSR_WRITE_2(sc,  VX_W4_MEDIA_TYPE, ENABLE_UTP);
	break;
      case CONNECTOR_BNC:
	CSR_WRITE_2(sc,  VX_COMMAND, START_TRANSCEIVER);
	DELAY(800);
	break;
      case CONNECTOR_TX:
      case CONNECTOR_FX:
	GO_WINDOW(4);
	CSR_WRITE_2(sc,  VX_W4_MEDIA_TYPE, LINKBEAT_ENABLE);
	break;
      default:	/* AUI and MII fall here */
	break;
    }
    GO_WINDOW(1); 

    prev_flags = ifp->if_flags;
    prev_conn = i;
}

static void
vxstart(ifp)
    struct ifnet *ifp;
{
    register struct vx_softc *sc = ifp->if_softc;
    register struct mbuf *m;
    int sh, len, pad;

    /* Don't transmit if interface is busy or not running */
    if ((sc->arpcom.ac_if.if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING)
	return;

startagain:
    /* Sneak a peek at the next packet */
    m = ifp->if_snd.ifq_head;
    if (m == NULL) {
	return;
    }
    
    /* We need to use m->m_pkthdr.len, so require the header */
    M_ASSERTPKTHDR(m);
    len = m->m_pkthdr.len;

    pad = (4 - len) & 3;

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
    VX_BUSY_WAIT;
    if (CSR_READ_2(sc, VX_W1_FREE_TX) < len + pad + 4) {
	CSR_WRITE_2(sc,  VX_COMMAND, SET_TX_AVAIL_THRESH | ((len + pad + 4) >> 2));
	/* not enough room in FIFO */
	if (CSR_READ_2(sc, VX_W1_FREE_TX) < len + pad + 4) { /* make sure */
	    ifp->if_flags |= IFF_OACTIVE;
	    ifp->if_timer = 1;
	    return;
	}
    }
    CSR_WRITE_2(sc,  VX_COMMAND, SET_TX_AVAIL_THRESH | (8188 >> 2));
    IF_DEQUEUE(&ifp->if_snd, m);
    if (m == NULL) 		/* not really needed */
	return;

    VX_BUSY_WAIT;
    CSR_WRITE_2(sc,  VX_COMMAND, SET_TX_START_THRESH |
	((len / 4 + sc->tx_start_thresh) >> 2));

    BPF_MTAP(&sc->arpcom.ac_if, m);

    /*
     * Do the output at splhigh() so that an interrupt from another device
     * won't cause a FIFO underrun.
     */
    sh = splhigh();

    CSR_WRITE_4(sc,  VX_W1_TX_PIO_WR_1, len | TX_INDICATE);

    while (m) {
        if (m->m_len > 3)
	    bus_space_write_multi_4(sc->bst, sc->bsh,
		VX_W1_TX_PIO_WR_1, (u_int32_t *)mtod(m, caddr_t), m->m_len / 4);
        if (m->m_len & 3)
	    bus_space_write_multi_1(sc->bst, sc->bsh,
		VX_W1_TX_PIO_WR_1,
		mtod(m, caddr_t) + (m->m_len & ~3) , m->m_len & 3);
	m = m_free(m);
    }
    while (pad--)
	CSR_WRITE_1(sc,  VX_W1_TX_PIO_WR_1, 0);	/* Padding */

    splx(sh);

    ++ifp->if_opackets;
    ifp->if_timer = 1;

readcheck:
    if ((CSR_READ_2(sc, VX_W1_RX_STATUS) & ERR_INCOMPLETE) == 0) {
	/* We received a complete packet. */
	
	if ((CSR_READ_2(sc, VX_STATUS) & S_INTR_LATCH) == 0) {
	    /*
	     * No interrupt, read the packet and continue
	     * Is  this supposed to happen? Is my motherboard
	     * completely busted?
	     */
	    vxread(sc);
	} else
	    /* Got an interrupt, return so that it gets serviced. */
	    return;
    } else {
	/* Check if we are stuck and reset [see XXX comment] */
	if (vxstatus(sc)) {
	    if (ifp->if_flags & IFF_DEBUG)
	       if_printf(ifp, "adapter reset\n");
	    vxreset(sc);
	}
    }

    goto startagain;
}

/*
 * XXX: The 3c509 card can get in a mode where both the fifo status bit
 *      FIFOS_RX_OVERRUN and the status bit ERR_INCOMPLETE are set
 *      We detect this situation and we reset the adapter.
 *      It happens at times when there is a lot of broadcast traffic
 *      on the cable (once in a blue moon).
 */
static int
vxstatus(sc)
    struct vx_softc *sc;
{
    int fifost;

    /*
     * Check the FIFO status and act accordingly
     */
    GO_WINDOW(4);
    fifost = CSR_READ_2(sc, VX_W4_FIFO_DIAG);
    GO_WINDOW(1);

    if (fifost & FIFOS_RX_UNDERRUN) {
	if (sc->arpcom.ac_if.if_flags & IFF_DEBUG)
	    printf("vx%d: RX underrun\n", sc->unit);
	vxreset(sc);
	return 0;
    }

    if (fifost & FIFOS_RX_STATUS_OVERRUN) {
	if (sc->arpcom.ac_if.if_flags & IFF_DEBUG)
	    printf("vx%d: RX Status overrun\n", sc->unit);
	return 1;
    }

    if (fifost & FIFOS_RX_OVERRUN) {
	if (sc->arpcom.ac_if.if_flags & IFF_DEBUG)
	    printf("vx%d: RX overrun\n", sc->unit);
	return 1;
    }

    if (fifost & FIFOS_TX_OVERRUN) {
	if (sc->arpcom.ac_if.if_flags & IFF_DEBUG)
	    printf("vx%d: TX overrun\n", sc->unit);
	vxreset(sc);
	return 0;
    }

    return 0;
}

static void     
vxtxstat(sc)
    struct vx_softc *sc;
{
    int i;

    /*
    * We need to read+write TX_STATUS until we get a 0 status
    * in order to turn off the interrupt flag.
    */
    while ((i = CSR_READ_1(sc,  VX_W1_TX_STATUS)) & TXS_COMPLETE) {
	CSR_WRITE_1(sc,  VX_W1_TX_STATUS, 0x0);

    if (i & TXS_JABBER) {
	++sc->arpcom.ac_if.if_oerrors;
	if (sc->arpcom.ac_if.if_flags & IFF_DEBUG)
	    printf("vx%d: jabber (%x)\n", sc->unit, i);
	vxreset(sc);
    } else if (i & TXS_UNDERRUN) {
	++sc->arpcom.ac_if.if_oerrors;
	if (sc->arpcom.ac_if.if_flags & IFF_DEBUG)
	    printf("vx%d: fifo underrun (%x) @%d\n",
		sc->unit, i, sc->tx_start_thresh);
	if (sc->tx_succ_ok < 100)
	    sc->tx_start_thresh = min(ETHER_MAX_LEN, sc->tx_start_thresh + 20);
	sc->tx_succ_ok = 0;
	vxreset(sc);
    } else if (i & TXS_MAX_COLLISION) {
	++sc->arpcom.ac_if.if_collisions;
	CSR_WRITE_2(sc,  VX_COMMAND, TX_ENABLE);
	sc->arpcom.ac_if.if_flags &= ~IFF_OACTIVE;
    } else
	sc->tx_succ_ok = (sc->tx_succ_ok+1) & 127;
    }
}

void
vxintr(voidsc)
    void *voidsc;
{
    register short status;
    struct vx_softc *sc = voidsc;
    struct ifnet *ifp = &sc->arpcom.ac_if;

    for (;;) {
	CSR_WRITE_2(sc,  VX_COMMAND, C_INTR_LATCH);

	status = CSR_READ_2(sc, VX_STATUS);

	if ((status & (S_TX_COMPLETE | S_TX_AVAIL |
		S_RX_COMPLETE | S_CARD_FAILURE)) == 0)
	    break;

	/*
	 * Acknowledge any interrupts.  It's important that we do this
	 * first, since there would otherwise be a race condition.
	 * Due to the i386 interrupt queueing, we may get spurious
	 * interrupts occasionally.
	 */
	CSR_WRITE_2(sc,  VX_COMMAND, ACK_INTR | status);

	if (status & S_RX_COMPLETE)
	    vxread(sc);
	if (status & S_TX_AVAIL) {
	    ifp->if_timer = 0;
	    sc->arpcom.ac_if.if_flags &= ~IFF_OACTIVE;
	    vxstart(&sc->arpcom.ac_if);
	}
	if (status & S_CARD_FAILURE) {
	    printf("vx%d: adapter failure (%x)\n", sc->unit, status);
	    ifp->if_timer = 0;
	    vxreset(sc);
	    return;
	}
	if (status & S_TX_COMPLETE) {
	    ifp->if_timer = 0;
	    vxtxstat(sc);
	    vxstart(ifp);
	}
    }

    /* no more interrupts */
    return;
}

static void
vxread(sc)
    struct vx_softc *sc;
{
    struct ifnet *ifp = &sc->arpcom.ac_if;
    struct mbuf *m;
    struct ether_header *eh;
    u_int len;

    len = CSR_READ_2(sc, VX_W1_RX_STATUS);

again:

    if (ifp->if_flags & IFF_DEBUG) {
	int err = len & ERR_MASK;
	char *s = NULL;

	if (len & ERR_INCOMPLETE)
	    s = "incomplete packet";
	else if (err == ERR_OVERRUN)
	    s = "packet overrun";
	else if (err == ERR_RUNT)
	    s = "runt packet";
	else if (err == ERR_ALIGNMENT)
	    s = "bad alignment";
	else if (err == ERR_CRC)
	    s = "bad crc";
	else if (err == ERR_OVERSIZE)
	    s = "oversized packet";
	else if (err == ERR_DRIBBLE)
	    s = "dribble bits";

	if (s)
	printf("vx%d: %s\n", sc->unit, s);
    }

    if (len & ERR_INCOMPLETE)
	return;

    if (len & ERR_RX) {
	++ifp->if_ierrors;
	goto abort;
    }

    len &= RX_BYTES_MASK;	/* Lower 11 bits = RX bytes. */

    /* Pull packet off interface. */
    m = vxget(sc, len);
    if (m == 0) {
	ifp->if_ierrors++;
	goto abort;
    }

    ++ifp->if_ipackets;

    {
	struct mbuf		*m0;

	m0 = m_devget(mtod(m, char *), m->m_pkthdr.len, ETHER_ALIGN, ifp, NULL);
	if (m0 == NULL) {
		ifp->if_ierrors++;
		goto abort;
	}

	m_freem(m);
	m = m0;
    }

    /* We assume the header fit entirely in one mbuf. */
    eh = mtod(m, struct ether_header *);

    /*
     * XXX: Some cards seem to be in promiscous mode all the time.
     * we need to make sure we only get our own stuff always.
     * bleah!
     */

    if ((eh->ether_dhost[0] & 1) == 0		/* !mcast and !bcast */
      && bcmp(eh->ether_dhost, sc->arpcom.ac_enaddr, ETHER_ADDR_LEN) != 0) {
	m_freem(m);
	return;
    }

    (*ifp->if_input)(ifp, m);

    /*
    * In periods of high traffic we can actually receive enough
    * packets so that the fifo overrun bit will be set at this point,
    * even though we just read a packet. In this case we
    * are not going to receive any more interrupts. We check for
    * this condition and read again until the fifo is not full.
    * We could simplify this test by not using vxstatus(), but
    * rechecking the RX_STATUS register directly. This test could
    * result in unnecessary looping in cases where there is a new
    * packet but the fifo is not full, but it will not fix the
    * stuck behavior.
    *
    * Even with this improvement, we still get packet overrun errors
    * which are hurting performance. Maybe when I get some more time
    * I'll modify vxread() so that it can handle RX_EARLY interrupts.
    */
    if (vxstatus(sc)) {
	len = CSR_READ_2(sc, VX_W1_RX_STATUS);
	/* Check if we are stuck and reset [see XXX comment] */
	if (len & ERR_INCOMPLETE) {
	    if (ifp->if_flags & IFF_DEBUG)
		printf("vx%d: adapter reset\n", sc->unit);
	    vxreset(sc);
	    return;
	}
	goto again;
    }

    return;

abort:
    CSR_WRITE_2(sc,  VX_COMMAND, RX_DISCARD_TOP_PACK);
}

static struct mbuf *
vxget(sc, totlen)
    struct vx_softc *sc;
    u_int totlen;
{
    struct ifnet *ifp = &sc->arpcom.ac_if;
    struct mbuf *top, **mp, *m;
    int len;
    int sh;

    m = sc->mb[sc->next_mb];
    sc->mb[sc->next_mb] = 0;
    if (m == 0) {
        MGETHDR(m, M_DONTWAIT, MT_DATA);
        if (m == 0)
            return 0;
    } else {
        /* If the queue is no longer full, refill. */
        if (sc->last_mb == sc->next_mb && sc->buffill_pending == 0) {
	    sc->ch = timeout(vxmbuffill, sc, 1);
	    sc->buffill_pending = 1;
	}
        /* Convert one of our saved mbuf's. */
        sc->next_mb = (sc->next_mb + 1) % MAX_MBS;
        m->m_data = m->m_pktdat;
        m->m_flags = M_PKTHDR;
	bzero(&m->m_pkthdr, sizeof(m->m_pkthdr));
    }
    m->m_pkthdr.rcvif = ifp;
    m->m_pkthdr.len = totlen;
    len = MHLEN;
    top = 0;
    mp = &top;

    /*
     * We read the packet at splhigh() so that an interrupt from another
     * device doesn't cause the card's buffer to overflow while we're
     * reading it.  We may still lose packets at other times.
     */
    sh = splhigh();

    /*
     * Since we don't set allowLargePackets bit in MacControl register,
     * we can assume that totlen <= 1500bytes.
     * The while loop will be performed iff we have a packet with
     * MLEN < m_len < MINCLSIZE.
     */
    while (totlen > 0) {
        if (top) {
            m = sc->mb[sc->next_mb];
            sc->mb[sc->next_mb] = 0;
            if (m == 0) {
                MGET(m, M_DONTWAIT, MT_DATA);
                if (m == 0) {
                    splx(sh);
                    m_freem(top);
                    return 0;
                }
            } else {
                sc->next_mb = (sc->next_mb + 1) % MAX_MBS;
            }
            len = MLEN;
        }
        if (totlen >= MINCLSIZE) {
	    MCLGET(m, M_DONTWAIT);
	    if (m->m_flags & M_EXT)
		len = MCLBYTES;
        }
        len = min(totlen, len);
        if (len > 3)
            bus_space_read_multi_4(sc->bst, sc->bsh,
		VX_W1_RX_PIO_RD_1, mtod(m, u_int32_t *), len / 4);
	if (len & 3) {
            bus_space_read_multi_1(sc->bst, sc->bsh,
		VX_W1_RX_PIO_RD_1, mtod(m, u_int8_t *) + (len & ~3),
		len & 3);
	}
        m->m_len = len;
        totlen -= len;
        *mp = m;
        mp = &m->m_next;
    }

    CSR_WRITE_2(sc, VX_COMMAND, RX_DISCARD_TOP_PACK);

    splx(sh);

    return top;
}


static int
vxioctl(ifp, cmd, data)
    register struct ifnet *ifp;
    u_long cmd;
    caddr_t data;
{
    struct vx_softc *sc = ifp->if_softc;
    struct ifreq *ifr = (struct ifreq *) data;
    int s, error = 0;

    s = splimp();

    switch (cmd) {
    case SIOCSIFFLAGS:
	if ((ifp->if_flags & IFF_UP) == 0 &&
	    (ifp->if_flags & IFF_RUNNING) != 0) {
	    /*
             * If interface is marked up and it is stopped, then
             * start it.
             */
	    vxstop(sc);
	    ifp->if_flags &= ~IFF_RUNNING;
        } else if ((ifp->if_flags & IFF_UP) != 0 &&
                   (ifp->if_flags & IFF_RUNNING) == 0) {
            /*
             * If interface is marked up and it is stopped, then
             * start it.
             */
            vxinit(sc);
        } else {
            /*
             * deal with flags changes:
             * IFF_MULTICAST, IFF_PROMISC,
             * IFF_LINK0, IFF_LINK1,
             */
            vxsetfilter(sc);
            vxsetlink(sc);
        }
        break;

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
	 * Multicast list has changed; set the hardware filter
	 * accordingly.
	 */
	vxreset(sc);
	error = 0;
        break;


    default:
	error = ether_ioctl(ifp, cmd, data);
	break;
    }

    splx(s);

    return (error);
}

static void
vxreset(sc)
    struct vx_softc *sc;
{
    int s;
    s = splimp();

    vxstop(sc);
    vxinit(sc);
    splx(s);
}

static void
vxwatchdog(ifp)
    struct ifnet *ifp;
{
    struct vx_softc *sc = ifp->if_softc;

    if (ifp->if_flags & IFF_DEBUG)
	if_printf(ifp, "device timeout\n");
    ifp->if_flags &= ~IFF_OACTIVE;
    vxstart(ifp);
    vxintr(sc);
}

void
vxstop(sc)
    struct vx_softc *sc;
{
    struct ifnet *ifp = &sc->arpcom.ac_if;

    ifp->if_timer = 0;

    CSR_WRITE_2(sc,  VX_COMMAND, RX_DISABLE);
    CSR_WRITE_2(sc,  VX_COMMAND, RX_DISCARD_TOP_PACK);
    VX_BUSY_WAIT;
    CSR_WRITE_2(sc,  VX_COMMAND, TX_DISABLE);
    CSR_WRITE_2(sc,  VX_COMMAND, STOP_TRANSCEIVER);
    DELAY(800);
    CSR_WRITE_2(sc,  VX_COMMAND, RX_RESET);
    VX_BUSY_WAIT;
    CSR_WRITE_2(sc,  VX_COMMAND, TX_RESET);
    VX_BUSY_WAIT;
    CSR_WRITE_2(sc,  VX_COMMAND, C_INTR_LATCH);
    CSR_WRITE_2(sc,  VX_COMMAND, SET_RD_0_MASK);
    CSR_WRITE_2(sc,  VX_COMMAND, SET_INTR_MASK);
    CSR_WRITE_2(sc,  VX_COMMAND, SET_RX_FILTER);

    vxmbufempty(sc);
}

int
vxbusyeeprom(sc)
    struct vx_softc *sc;
{
    int j, i = 100;

    while (i--) {
        j = CSR_READ_2(sc, VX_W0_EEPROM_COMMAND);
        if (j & EEPROM_BUSY)
            DELAY(100);
        else
            break;
    }
    if (!i) {
        printf("vx%d: eeprom failed to come ready\n", sc->unit);
        return (1);
    }
    return (0);
}

static void
vxmbuffill(sp)
    void *sp;
{
    struct vx_softc *sc = (struct vx_softc *) sp;
    int s, i;

    s = splimp();
    i = sc->last_mb;
    do {
	if (sc->mb[i] == NULL)
	    MGET(sc->mb[i], M_DONTWAIT, MT_DATA);
	if (sc->mb[i] == NULL)
	    break;
	i = (i + 1) % MAX_MBS;
    } while (i != sc->next_mb);
    sc->last_mb = i;
    /* If the queue was not filled, try again. */
    if (sc->last_mb != sc->next_mb) {
	sc->ch = timeout(vxmbuffill, sc, 1);
	sc->buffill_pending = 1;
    } else {
	sc->buffill_pending = 0;
    }
    splx(s);
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
    if (sc->buffill_pending != 0)
	untimeout(vxmbuffill, sc, sc->ch);
    splx(s);
}
