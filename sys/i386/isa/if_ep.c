/*
 * Copyright (c) 1993 Herb Peyerl (hpeyerl@novatel.ca)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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
 *	$Id: if_ep.c,v 1.5 1993/12/20 09:05:55 mycroft Exp $
 */
/*
 * TODO: 
 *       Multi-509 configs.
 *       don't pass unit into epstop.
 *	 epintr returns an int for magnum. 0=not for me. 1=for me. -1=whoknows?
 */
#include "ep.h"
#if NEP > 0
#include "bpfilter.h"

#include "param.h"
#include "systm.h"
#include "kernel.h"
#include "mbuf.h"
#include "socket.h"
#include "ioctl.h"
#include "errno.h"
#include "syslog.h"

#include "net/if.h"
#include "net/netisr.h"
#include "net/if_dl.h"
#include "net/if_types.h"
#include "net/netisr.h"

#ifdef INET
#include "netinet/in.h"
#include "netinet/in_systm.h"
#include "netinet/in_var.h"
#include "netinet/ip.h"
#include "netinet/if_ether.h"
#endif

#ifdef NS
#include "netns/ns.h"
#include "netns/ns_if.h"
#endif

#if NBPFILTER > 0
#include "net/bpf.h"
#include "net/bpfdesc.h"
#endif

#include "i386/include/pio.h"

#include "i386/isa/isa.h"
#include "i386/isa/isa_device.h"
#include "i386/isa/icu.h"
#include "i386/isa/if_epreg.h"

#define ETHER_MIN_LEN 64
#define ETHER_MAX_LEN   1518
#define ETHER_ADDR_LEN  6
/*
 * Ethernet software status per interface.
 */
struct  ep_softc {
	struct  arpcom ep_ac;		/* Ethernet common part	        */
#define ep_if   ep_ac.ac_if		/* network-visible interface    */
#define ep_addr ep_ac.ac_enaddr		/* hardware Ethernet address    */
	short	ep_io_addr;		/* i/o bus address		*/
	char	ep_connectors;		/* Connectors on this card.	*/
#define MAX_MBS  8			/* # of mbufs we keep around	*/
	struct	mbuf *mb[MAX_MBS];	/* spare mbuf storage.		*/
	int	next_mb;		/* Which mbuf to use next. 	*/
	int 	last_mb;		/* Last mbuf.			*/
	int	tx_start_thresh;	/* Current TX_start_thresh.	*/
	caddr_t bpf;			/* BPF  "magic cookie"		*/
} ep_softc[NEP];

void epinit(int);
void epreset(int, int);
void epwatchdog(int);
void epstop(int);
void epintr(int);
void epstart(struct ifnet *);
void epread(struct ep_softc *);

int epprobe(struct isa_device *);
int ether_output(),
    epattach(),
    epioctl();

static void send_ID_sequence(u_short);
static u_short get_eeprom_data(int, int);
static int is_eeprom_busy(struct isa_device *);
static void fill_mbuf_queue(caddr_t, int);

struct isa_driver epdriver = {
	epprobe, 
	epattach,
	"ep"
};

/*
 * Rudimentary support for multiple cards is here but is not
 * currently handled.  In the future we will have to add code
 * for tagging the cards for later activation.  We wanna do something
 * about the id_port.  We're limited due to current config procedure.
 * Magnum config holds promise of a fix but we'll have to wait a bit. 
 */
int epprobe(is)
	struct isa_device *is;
{
	struct ep_softc *sc = &ep_softc[is->id_unit];
	u_short k;
	int id_port = 0x100;   /* XXX */

	outw(BASE+EP_COMMAND, GLOBAL_RESET);
	DELAY(1000);
	outb(id_port, 0xc0);   /* Global reset to id_port. */
	DELAY(1000);
	send_ID_sequence(id_port);
	DELAY(1000);
/*
 * MFG_ID should have 0x6d50.
 * PROD_ID should be 0x9[0-f]50
 */
	k = get_eeprom_data(id_port, EEPROM_MFG_ID);
	if (k != MFG_ID)
		return(0);
	k = get_eeprom_data(id_port, EEPROM_PROD_ID);
	if ((k & 0xf0ff) != (PROD_ID & 0xf0ff))
		return(0);

	k = get_eeprom_data(id_port, EEPROM_ADDR_CFG); /* get addr cfg */
	k = (k & 0x1f)*0x10+0x200;	/* decode base addr. */
	if (k != (u_short) is->id_iobase)
		return(0);

	k = get_eeprom_data(id_port, EEPROM_RESOURCE_CFG);
	k >>= 12;
	if (is->id_irq != (1<<((k==2) ? 9 : k)))
		return(0);

	outb(id_port, ACTIVATE_ADAPTER_TO_CONFIG);

	return(0x10); /* 16 bytes of I/O space used. */
}

int epattach(is)
	struct isa_device *is;
{
	struct ep_softc *sc = &ep_softc[is->id_unit];
	struct ifnet *ifp = &sc->ep_if;
	u_short i;
	struct ifaddr *ifa;
	struct sockaddr_dl *sdl;

	ifp->if_unit = is->id_unit;
	ifp->if_name = "ep" ;
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS;
	ifp->if_init = epinit;
	ifp->if_output = ether_output;
	ifp->if_start = epstart;
	ifp->if_ioctl = epioctl;
	ifp->if_watchdog = epwatchdog;

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
		sdl = (struct sockaddr_dl *)ifa->ifa_addr;
		sdl->sdl_type = IFT_ETHER;
		sdl->sdl_alen = ETHER_ADDR_LEN;
		sdl->sdl_slen = 0;
		bcopy(sc->ep_addr, LLADDR(sdl), ETHER_ADDR_LEN);
	}

	sc->ep_io_addr = is->id_iobase;

	sc->ep_connectors =  0;
	i=inw(is->id_iobase+EP_W0_CONFIG_CTRL);

	printf("ep%d: ", is->id_unit);

	if (i & IS_AUI) {
		if (sc->ep_connectors) 
			printf("/");
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
	if (!sc->ep_connectors) 
		printf("!no connectors!");

/*
 * Read the station address from the eeprom
 */
	for (i=0; i<3; i++) {
		u_short *p;
		GO_WINDOW(0);
		if (is_eeprom_busy(is)) 
			return 0;
		outw(BASE+EP_W0_EEPROM_COMMAND, READ_EEPROM | i);
		if (is_eeprom_busy(is)) 
			return 0;
		p =(u_short *)&sc->ep_addr[i*2];
		*p=htons(inw(BASE+EP_W0_EEPROM_DATA));
		GO_WINDOW(2);
		outw(BASE+EP_W2_ADDR_0+(i*2), ntohs(*p));
	}
	printf(" address %s\n", ether_sprintf(sc->ep_addr));

#if NBPFILTER > 0
	bpfattach(&sc->bpf, ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif
	return 1;
}

/* The order in here seems important. Otherwise we may not receive interrupts. ?! */
void epinit(unit)
	int unit;
{
	register struct ep_softc *sc = &ep_softc[unit];
	register struct ifnet *ifp = &sc->ep_if;
	int s,i;


	if (ifp->if_addrlist == (struct ifaddr *) 0) {
		printf("ep: address not known...\n");
		return;
	}

	s=splimp();

	while (inb(BASE+EP_STATUS) & S_COMMAND_IN_PROGRESS) 
		;


	GO_WINDOW(0);
	outw(BASE+EP_W0_CONFIG_CTRL, 0);   /* Disable the card */

	outw(BASE+EP_W0_CONFIG_CTRL, ENABLE_DRQ_IRQ);   /* Enable the card */

	GO_WINDOW(2);
	for(i=0;i<6;i++)	/* Reload the ether_addr. */
		outb(BASE+EP_W2_ADDR_0+i, sc->ep_addr[i]);

	outw(BASE+EP_COMMAND, RX_RESET);
	outw(BASE+EP_COMMAND, TX_RESET);

	GO_WINDOW(1);			/* Window 1 is operating window */
	for(i=0;i<31;i++)
		inb(BASE+EP_W1_TX_STATUS);

	outw(BASE+EP_COMMAND, ACK_INTR | 0xff);	/* get rid of stray intr's */

	outw(BASE+EP_COMMAND, SET_RD_0_MASK | S_CARD_FAILURE | S_RX_COMPLETE |
		S_TX_COMPLETE | S_TX_AVAIL);
	outw(BASE+EP_COMMAND, SET_INTR_MASK | S_CARD_FAILURE | S_RX_COMPLETE |
		S_TX_COMPLETE | S_TX_AVAIL);

	outw(BASE+EP_COMMAND, SET_RX_FILTER | FIL_INDIVIDUAL | 
		FIL_GROUP | FIL_BRDCST);

	if (!(ifp->if_flags & IFF_ALTPHYS) && (sc->ep_connectors & BNC)) {	/* Want BNC? */
		outw(BASE+EP_COMMAND, START_TRANSCEIVER);
		DELAY(1000);
#ifdef EP_DEBUG
printf("ed0: selected BNC\n");
#endif
	}
	if (!(ifp->if_flags & IFF_ALTPHYS) && (sc->ep_connectors & UTP)) {    /* Want UTP? */
		GO_WINDOW(4);
		outw(BASE+EP_W4_MEDIA_TYPE, ENABLE_UTP);
		GO_WINDOW(1);
#ifdef EP_DEBUG
printf("ed0: selected UTP\n");
#endif
	}
		
	outw(BASE+EP_COMMAND, RX_ENABLE);
	outw(BASE+EP_COMMAND, TX_ENABLE);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;   /* just in case */
	sc->tx_start_thresh = 20;	/* probably a good starting point. */
	/*
	 * Store up a bunch of mbuf's for use later. (MAX_MBS). First we
	 * free up any that we had in case we're being called from intr or
	 * somewhere else.
	 */
	sc->last_mb = 0;
	sc->next_mb = 0;
	fill_mbuf_queue((caddr_t) sc, 0);

	epstart(ifp);

	(void) splx(s);
}

void epstart(ifp)
	struct ifnet *ifp;
{
	register struct ep_softc *sc = &ep_softc[ifp->if_unit];
	struct mbuf *m, *top;
	int s, len, pad;

	s=splimp();
	if (sc->ep_if.if_flags & IFF_OACTIVE) {
		splx(s);
		return;
	}

startagain:
	m = sc->ep_if.if_snd.ifq_head;    /* Sneak a peek at the next packet */
	if (m == 0) {
		splx(s);
		return;
	}
	pad = (4-(m->m_pkthdr.len%4)+4)%4; /* icky pooh!! */

	if ((inw(BASE+EP_W1_FREE_TX)) < (m->m_pkthdr.len)+pad+4) {   /* no room in FIFO */
		outw(BASE+EP_COMMAND, SET_TX_AVAIL_THRESH | (m->m_pkthdr.len)+pad+4);
		sc->ep_if.if_flags |= IFF_OACTIVE;
		splx(s);
		return;
	}

	IF_DEQUEUE(&sc->ep_if.if_snd, m);
	if (m == 0)  {   /* Could make this go away. */
		splx(s);
		return;
	}

	outw(BASE+EP_COMMAND, SET_TX_START_THRESH | 
		(m->m_pkthdr.len/4 + sc->tx_start_thresh));

	outw(BASE+EP_W1_TX_PIO_WR_1, m->m_pkthdr.len);
	outw(BASE+EP_W1_TX_PIO_WR_1, 0xffff); /* Second dword meaningless */

	for(top = m; m != 0; m = m->m_next) {
		outsw(BASE+EP_W1_TX_PIO_WR_1, mtod(m, caddr_t), m->m_len/2);
		if(m->m_len & 1)
			outb(BASE+EP_W1_TX_PIO_WR_1,
				*(mtod(m, caddr_t)+m->m_len-1));
	}

	while (pad--)
		outb(BASE+EP_W1_TX_PIO_WR_1, 0); /* Padding */
#if NBPFILTER > 0
	if (sc->bpf) {
	u_short etype;
		int off, datasize, resid;
		struct ether_header *eh;
		struct trailer_header {
			u_short ether_type;
			u_short ether_residual;
		} trailer_header;
		char ether_packet[ETHER_MAX_LEN];
		char *ep;

		ep = ether_packet;

		/*
		 * We handle trailers below:
		 * Copy ether header first, then residual data,
		 * then data. Put all this in a temporary buffer
		 * 'ether_packet' and send off to bpf. Since the
		 * system has generated this packet, we assume
		 * that all of the offsets in the packet are
		 * correct; if they're not, the system will almost
		 * certainly crash in m_copydata.
		 * We make no assumptions about how the data is
		 * arranged in the mbuf chain (i.e. how much
		 * data is in each mbuf, if mbuf clusters are
		 * used, etc.), which is why we use m_copydata
		 * to get the ether header rather than assume
		 * that this is located in the first mbuf.
		 */
		/* copy ether header */
                m_copydata(top, 0, sizeof(struct ether_header), ep);
                eh = (struct ether_header *) ep;
                ep += sizeof(struct ether_header);
                etype = ntohs(eh->ether_type);
                if (etype >= ETHERTYPE_TRAIL &&
                    etype < ETHERTYPE_TRAIL+ETHERTYPE_NTRAILER) {
                        datasize = ((etype - ETHERTYPE_TRAIL) << 9);
                        off = datasize + sizeof(struct ether_header);

                        /* copy trailer_header into a data structure */
                        m_copydata(top, off, sizeof(struct trailer_header),
                                &trailer_header.ether_type);

                        /* copy residual data */
			resid = trailer_header.ether_residual -
				sizeof(struct trailer_header);
			resid = ntohs(resid);
                        m_copydata(top, off+sizeof(struct trailer_header),
                                resid, ep);
                        ep += resid;

                        /* copy data */
                        m_copydata(top, sizeof(struct ether_header),
                                datasize, ep);
                        ep += datasize;

                        /* restore original ether packet type */
                        eh->ether_type = trailer_header.ether_type;

                        bpf_tap(sc->bpf, ether_packet, ep - ether_packet);
                } else
                        bpf_mtap(sc->bpf, top);
        }
#endif

	m_freem(top);
	++sc->ep_if.if_opackets;
	/*
	 * Is another packet coming in? We don't want to overflow the
	 * tiny RX fifo.
	 */
	if (inw(BASE+EP_W1_RX_STATUS) & RX_BYTES_MASK) {
		splx(s);
		return;
	}
	goto startagain;
}

void epintr(unit)
	int unit;
{
	int status, i;
	register struct ep_softc *sc = &ep_softc[unit];
	struct ifnet *ifp = &sc->ep_if;
	struct mbuf *m;

	status=0;
checkintr:
	status = inw(BASE + EP_STATUS) & (S_TX_COMPLETE|S_TX_AVAIL|S_RX_COMPLETE|S_CARD_FAILURE);
	if (status == 0) {                     /* No interrupts. */
		outw(BASE+EP_COMMAND, C_INTR_LATCH);
		return;
	}
	outw(BASE+EP_COMMAND, ACK_INTR | status);  /* important that we do this first. */

	if (status & S_TX_AVAIL) {
		status &= ~S_TX_AVAIL;
		inw(BASE+EP_W1_FREE_TX);
		sc->ep_if.if_flags &= ~IFF_OACTIVE;
		epstart(&sc->ep_if);
	}
	if (status & S_RX_COMPLETE) {
		status &= ~S_RX_COMPLETE;
		epread(sc);
	}
	if (status & S_CARD_FAILURE) {
		printf("ep%d: reset (status: %x)\n", unit, status);
		outw(BASE+EP_COMMAND, C_INTR_LATCH);
		epinit(unit);
		return;
	}
	if (status & S_TX_COMPLETE) {
		status &= ~S_TX_COMPLETE;
		/*
		 * We need to read TX_STATUS until we get a 0 status in
		 * order to turn off the interrupt flag.
		 */
		while ((i=inb(BASE+EP_W1_TX_STATUS)) & TXS_COMPLETE) {
			outw(BASE+EP_W1_TX_STATUS, 0x0);
			if (i & (TXS_MAX_COLLISION|TXS_JABBER|TXS_UNDERRUN)) {
				if (i & TXS_MAX_COLLISION)
					++sc->ep_if.if_collisions;
				if (i & (TXS_JABBER|TXS_UNDERRUN)) {
					outw(BASE+EP_COMMAND, TX_RESET);
					if(i & TXS_UNDERRUN) {
						if (sc->tx_start_thresh < ETHER_MAX_LEN) {
							sc->tx_start_thresh += 20;
							outw(BASE+EP_COMMAND, 
								SET_TX_START_THRESH |
								sc->tx_start_thresh);
						}
					}
				}
				outw(BASE+EP_COMMAND, TX_ENABLE);
				++sc->ep_if.if_oerrors;
			}
		}
		epstart(ifp);
	}
	goto checkintr;
}

void epread(sc)
	register struct ep_softc *sc;
{
	struct ether_header *eh;
	struct mbuf *mcur, *m, *m0, *top;
	int totlen, lenthisone;
	int save_totlen;
	u_short etype;
	int off, resid;
	int count, spinwait;
	int i;

	totlen = inw(BASE + EP_W1_RX_STATUS);
	off = 0;
	top = 0;

	if (totlen & ERR_RX) {
		++sc->ep_if.if_ierrors;
		goto out;
	}	
	save_totlen = totlen &= RX_BYTES_MASK;	/* Lower 10 bits = RX bytes. */

	m = sc->mb[sc->next_mb];
	sc->mb[sc->next_mb] = 0;

	if (m == 0) {
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == 0)
			goto out;
	} else {	/* Convert one of our saved mbuf's */
		sc->next_mb = (sc->next_mb + 1) % MAX_MBS;
		m->m_data = m->m_pktdat;
		m->m_flags = M_PKTHDR;
	}

	top = m0 = m;    /* We assign top so we can "goto out" */
#	define EROUND  ((sizeof(struct ether_header) + 3) & ~3)
#	define EOFF    (EROUND - sizeof(struct ether_header))
	m0->m_data += EOFF;
	/* Read what should be the header. */
	insw(BASE+EP_W1_RX_PIO_RD_1, mtod(m0, caddr_t), sizeof(struct ether_header)/2);
	m->m_len = sizeof(struct ether_header);
	totlen -= sizeof(struct ether_header);
	/*
 	 * mostly deal with trailer here.  (untested)
	 * We do this in a couple of parts.  First we check for a trailer, if
	 * we have one we convert the mbuf back to a regular mbuf and set the offset and
	 * subtract sizeof(struct ether_header) from the pktlen.
	 * After we've read the packet off the interface (all except for the trailer
	 * header, we then get a header mbuf, read the trailer into it, and fix up
	 * the mbuf pointer chain.
	 */
	eh=mtod(m, struct ether_header *);
	eh->ether_type = ntohs((u_short)eh->ether_type);
	if (eh->ether_type >=  ETHERTYPE_TRAIL &&
	    eh->ether_type < ETHERTYPE_TRAIL+ETHERTYPE_NTRAILER) {
		m->m_data = m->m_dat;   /* Convert back to regular mbuf.  */
		m->m_flags = 0;		/* This sucks but non-trailers are the norm */
		off = (eh->ether_type - ETHERTYPE_TRAIL) * 512;
		if (off >= ETHERMTU) {
			m_freem(m);
			return;		/* sanity */
		}
		totlen -= sizeof(struct ether_header); /* We don't read the trailer */
		m->m_data += 2 * sizeof(u_short);      /* Get rid of type & len*/
	}
	while (totlen>0) {
		lenthisone=min(totlen, M_TRAILINGSPACE(m));
		if (lenthisone == 0) {	/* no room in this one */
			mcur = m;
			m = sc->mb[sc->next_mb];
			sc->mb[sc->next_mb] = 0;
			if (!m) {
				MGET(m, M_DONTWAIT, MT_DATA);
				if (m==0)
					goto out;
			} else {
				timeout(fill_mbuf_queue, (caddr_t) sc, 1);
				sc->next_mb = (sc->next_mb + 1) % MAX_MBS;
			}
			if (totlen >= MINCLSIZE)
				MCLGET(m, M_DONTWAIT);
			m->m_len = 0;
			mcur->m_next = m;
			lenthisone = min(totlen, M_TRAILINGSPACE(m));
		}
		insw(BASE+EP_W1_RX_PIO_RD_1, mtod(m, caddr_t)+m->m_len, lenthisone/2);
		m->m_len += lenthisone;
		if (lenthisone & 1)
			*(mtod(m, caddr_t)+m->m_len-1) = inb(BASE+EP_W1_RX_PIO_RD_1);
		totlen -= lenthisone;
	}
	if (off) {
		top = sc->mb[sc->next_mb];
		sc->mb[sc->next_mb] = 0;
		if (top == 0) {
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (top == 0)
				goto out;
		} else {	/* Convert one of our saved mbuf's */
			sc->next_mb = (sc->next_mb + 1) % MAX_MBS;
			top->m_data = top->m_pktdat;
			top->m_flags = M_PKTHDR;
		}
		insw(BASE+EP_W1_RX_PIO_RD_1, mtod(m, caddr_t)+m->m_len,
			sizeof(struct ether_header));
		eh->ether_type = ntohs(eh->ether_type);
		top->m_next = m0;
		top->m_len = sizeof(struct ether_header);
		/*  XXX Accomodate for type and len from beginning of trailer data */
		top->m_pkthdr.len = save_totlen - (2 * sizeof(u_short));
	} else {
		top = m0;
		top->m_pkthdr.len = save_totlen;
	}
		
        top->m_pkthdr.rcvif = &sc->ep_if;
	outw(BASE+EP_COMMAND, RX_DISCARD_TOP_PACK);
	while (inb(BASE+EP_STATUS) & S_COMMAND_IN_PROGRESS) 
		;
        ++sc->ep_if.if_ipackets;
	m_adj(top, sizeof(struct ether_header));
        ether_input(&sc->ep_if, eh, top);
	return;

out:	outw(BASE+EP_COMMAND, RX_DISCARD_TOP_PACK);
	while (inb(BASE+EP_STATUS) & S_COMMAND_IN_PROGRESS) 
		;
	if (top) 
		m_freem(top);
}

/*
 * Look familiar?
 */
int epioctl(ifp, cmd, data)
	register struct ifnet *ifp;
	int cmd;
	caddr_t data;
{
	register struct ifaddr *ifa = (struct ifaddr *)data;
	struct ep_softc *sc = &ep_softc[ifp->if_unit];
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error=0;
	
	switch(cmd){
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			epinit(ifp->if_unit);  /* before arpwhohas */
			((struct arpcom *)ifp)->ac_ipaddr = IA_SIN(ifa)->sin_addr;
			arpwhohas((struct arpcom *)ifp, &IA_SIN(ifa)->sin_addr);
			break;
#endif
#ifdef NS
		case AF_NS:
		{
			register struct ns_addr *ina = &(IA_SNS(ifa)->sns_addr);

			if (ns_nullhost(*ina))
				ina->x_host = *(union ns_host *)(sc->ep_ac.ac_enaddr);
			else {
				ifp->if_flags &= ~IFF_RUNNING;
				bcopy((caddr_t)ina->x_host.c_host,
					(caddr_t)sc->ns_addr,
					sizeof(sc->ep_ac.ac_enaddr));		   
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
#ifdef EP_DEBUG
printf("ep: ioctl SIOCSIFFLAGS = 0x%x\n", ifp->if_flags);
#endif
		if ((ifp->if_flags & IFF_UP) == 0 && ifp->if_flags & IFF_RUNNING) {
			epstop(ifp->if_unit);
			ifp->if_flags &= ~IFF_RUNNING;
		} else if ((ifp->if_flags & IFF_UP) && (ifp->if_flags & IFF_RUNNING) == 0)
			 epinit(ifp->if_unit);
		break;

#ifdef notdef
	case SIOCGHWADDR:
		bcopy((caddr_t)sc->sc_addr, (caddr_t) &ifr->ifr_data, sizeof(sc->sc_addr));
		break;
#endif

	default:
		error = EINVAL;
	}
	return (error);
}

void epreset(unit, uban)
	int unit;
	int uban;	/* XXXX */
{
	int s;

	s = splimp();
	epstop(unit);
	epinit(unit);
	(void) splx(s);
}

void epwatchdog(unit)
	int unit;
{
	struct ep_softc *sc = &ep_softc[unit];

	log(LOG_ERR, "ep%d: device timeout\n", unit);
	++sc->ep_ac.ac_if.if_oerrors;

	epreset(unit, 0);
}

void epstop(unit)
	int unit;
{
        struct ep_softc *sc = &ep_softc[unit];

	outw(BASE+EP_COMMAND, RX_DISABLE);
	outw(BASE+EP_COMMAND, RX_DISCARD_TOP_PACK);
	while (inb(BASE+EP_STATUS) & S_COMMAND_IN_PROGRESS) 
		;
	outw(BASE+EP_COMMAND, TX_DISABLE);
	outw(BASE+EP_COMMAND, STOP_TRANSCEIVER);
	outw(BASE+EP_COMMAND, RX_RESET);
	outw(BASE+EP_COMMAND, TX_RESET);
	outw(BASE+EP_COMMAND, C_INTR_LATCH);
	outw(BASE+EP_COMMAND, SET_RD_0_MASK);
	outw(BASE+EP_COMMAND, SET_INTR_MASK);
	outw(BASE+EP_COMMAND, SET_RX_FILTER);
}

/*
 * This is adapted straight from the book. There's probably a better way.
 */
void send_ID_sequence(port)
	u_short port;
{
	char cx, al;

	cx=0x0ff;
	al=0x0ff;

	outb(port, 0x0);
	DELAY(1000);
	outb(port, 0x0);
	DELAY(1000);
	
loop1:  cx--;
	outb(port, al);
	if (!(al & 0x80)) {
		al=al<<1;
		goto loop1;
	}
	al=al<<1;
	al^=0xcf;
	if (cx)
		goto loop1;
	
}

/*
 * We get eeprom data from the id_port given an offset into the
 * eeprom.  Basically; after the ID_sequence is sent to all of
 * the cards; they enter the ID_CMD state where they will accept
 * command requests. 0x80-0xbf loads the eeprom data.  We then
 * read the port 16 times and with every read; the cards check
 * for contention (ie: if one card writes a 0 bit and another
 * writes a 1 bit then the host sees a 0. At the end of the cycle;
 * each card compares the data on the bus; if there is a difference
 * then that card goes into ID_WAIT state again). In the meantime;
 * one bit of data is returned in the AX register which is conveniently
 * returned to us by inb().  Hence; we read 16 times getting one
 * bit of data with each read.  
 */
static u_short get_eeprom_data(id_port, offset)
	int id_port;
	int offset;
{
	int i, data=0;
	outb(id_port, 0x80+offset);
	DELAY(1000);
	for (i=0; i<16; i++)
		data = (data<<1) | (inw(id_port) & 1);
	return(data);
}

static int is_eeprom_busy(is)
	struct isa_device *is;
{
	int i=0, j;
	register struct ep_softc *sc = &ep_softc[is->id_unit];

	while (i++<100) {
		j=inw(BASE+EP_W0_EEPROM_COMMAND);
		if (j & EEPROM_BUSY)
			DELAY(100);
		else
			break;
	}
	if (i>=100) {
		printf("\nep%d: eeprom failed to come ready.\n", is->id_unit);
		return(1);
	}
	if (j & EEPROM_TST_MODE) {
		printf("\nep%d: 3c509 in test mode. Erase pencil mark!\n", is->id_unit);
		return(1);
	}
	return(0);
}

static void fill_mbuf_queue(sp, dummy)
	caddr_t sp;
	int dummy;
{
	struct ep_softc *sc = (struct ep_softc *) sp;
	int i=0;

	if (sc->mb[sc->last_mb])
		return;
	i=sc->last_mb;
	do {
		MGET(sc->mb[i], M_DONTWAIT, MT_DATA);
		if (!sc->mb[i])
			break;
		i = (i+1) % MAX_MBS;
	} while(i != sc->next_mb);
	sc->last_mb = i;
}
#endif /* NEP > 0 */
