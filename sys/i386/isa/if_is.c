/*
 * Isolan AT 4141-0 Ethernet driver
 * Isolink 4110 
 *
 * By Paul Richards 
 *
 * Copyright (C) 1993, Paul Richards. This software may be used, modified,
 *   copied, distributed, and sold, in both source and binary form provided
 *   that the above copyright and these terms are retained. Under no
 *   circumstances is the author responsible for the proper functioning
 *   of this software, nor does the author assume any responsibility
 *   for damages incurred with its use.
 *
*/

/* TODO

1) Add working multicast support
2) Use better allocation of memory to card
3) Advertise for more packets until all transmit buffers are full
4) Add more of the timers/counters e.g. arpcom.opackets etc.
*/

#include "is.h"
#if NIS > 0

#include "bpfilter.h"

#include "param.h"
#include "systm.h"
#include "errno.h"
#include "ioctl.h"
#include "mbuf.h"
#include "socket.h"
#include "syslog.h"

#include "net/if.h"
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

#include "i386/isa/isa_device.h"
#include "i386/isa/if_isreg.h"
#include "i386/isa/icu.h"

#include "vm/vm.h"



#define ETHER_MIN_LEN 64
#define ETHER_MAX_LEN   1518
#define ETHER_ADDR_LEN  6


/*
 * Ethernet software status per interface.
 *
 * Each interface is referenced by a network interface structure,
 * arpcom.ac_if, which the routing code uses to locate the interface.
 * This structure contains the output queue for the interface, its address, ...
 */
struct	is_softc {
	struct arpcom arpcom;             /* Ethernet common part */
	int iobase;                       /* IO base address of card */
	void *lance_mem;             /* Base of memory allocated to card */
	struct mds 	*rd;
	struct mds	*td;
	unsigned char	*rbuf;
	unsigned char	*tbuf;
	int	last_rd;
	int	last_td;
	int	no_td;
	caddr_t bpf;                      /* BPF "magic cookie" */

} is_softc[NIS] ;

struct init_block init_block[NIS];

/* Function prototypes */
int is_probe(),is_attach(),is_watchdog();
int is_ioctl(),is_init(),is_start();

static inline void is_rint(int unit);
static inline void isread(struct is_softc*, unsigned char*, int);

struct	mbuf *isget();

struct	isa_driver isdriver = {
	is_probe,
	is_attach,
	"is"
};

iswrcsr(unit,port,val)
	int unit;
	u_short port;
	u_short val;
{
	int iobase;

	iobase = is_softc[unit].iobase;
	outw(iobase+RAP,port);
	outw(iobase+RDP,val);
}

u_short isrdcsr(unit,port)
	int unit;
	u_short port;
{
	int iobase;
	
	iobase = is_softc[unit].iobase;
	outw(iobase+RAP,port);
	return(inw(iobase+RDP));
} 

is_probe(isa_dev)
	struct isa_device *isa_dev;
{
	int val,i,s;
	int unit = isa_dev->id_unit ;
	register struct is_softc *is = &is_softc[unit];

	is->iobase = isa_dev->id_iobase;

	/* Stop the lance chip, put it known state */	
	iswrcsr(unit,0,STOP);
	DELAY(100);

	/* is there a lance? */
	iswrcsr(unit,3, 0xffff);
	if (isrdcsr(unit,3) != 7) {
		is->iobase = 0;
		return (0);
	}
	iswrcsr(unit,3, 0);

	/* Extract board address */
	for(i=0;i<ETHER_ADDR_LEN;i++)
		is->arpcom.ac_enaddr[i]=inb(is->iobase+(i*2));

	return (1);
}



/*
 * Reset of interface.
 */
int
is_reset(int unit)
{
	int s;
	struct is_softc *is = &is_softc[unit];

	if (unit >= NIS)
		return;
	s = splnet();
	printf("is%d: reset\n", unit);
	is_init(unit);
	(void) splx(s);
}
 
/*
 * Interface exists: make available by filling in network interface
 * record.  System will initialize the interface when it is ready
 * to accept packets.  We get the ethernet address here.
 */
int
is_attach(isa_dev)
	struct isa_device *isa_dev;
{
	int unit = isa_dev->id_unit;
	struct is_softc *is = &is_softc[unit];
	struct ifnet *ifp = &is->arpcom.ac_if;
	struct ifaddr *ifa;
	struct sockaddr_dl *sdl;

	ifp->if_unit = unit;
	ifp->if_name = isdriver.name ;
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS;
	ifp->if_init = is_init;
	ifp->if_output = ether_output;
	ifp->if_start = is_start;
	ifp->if_ioctl = is_ioctl;
	ifp->if_reset = is_reset;
	ifp->if_watchdog = is_watchdog;

	/* 
	 * XXX - Set is->lance_mem to NULL so first pass 
	 * through init_mem it won't try and free memory
	 * This is getting messy and needs redoing.
	 * Yes, I know NULL != 0 but it does what I want :-) 
	 */

	is->lance_mem = NULL; 

	/* Set up DMA */
	isa_dmacascade(isa_dev->id_drq);

	if_attach(ifp);

	/*
	 * Search down the ifa address list looking 
	 * for the AF_LINK type entry
	 */

	ifa = ifp->if_addrlist;
	while ((ifa != 0) && (ifa->ifa_addr != 0) &&
	  (ifa->ifa_addr->sa_family != AF_LINK))
		ifa = ifa->ifa_next;

	/*
	 * If we find an AF_LINK type entry, we will fill
	 * in the hardware address for this interface.
	 */

	if ((ifa != 0) && (ifa->ifa_addr != 0)) {

		/*
		 * Fill in the link level address for this interface
		 */

		sdl = (struct sockaddr_dl *)ifa->ifa_addr;
		sdl->sdl_type = IFT_ETHER;
		sdl->sdl_alen = ETHER_ADDR_LEN;
		sdl->sdl_slen = 0;
		bcopy(is->arpcom.ac_enaddr, LLADDR(sdl), ETHER_ADDR_LEN);
	}

	printf ("is%d: address %s\n", unit,
		ether_sprintf(is->arpcom.ac_enaddr)) ;

#if NBPFILTER > 0
	bpfattach(&is->bpf, ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif
}

int
is_watchdog(unit)
        int unit;
{
        log(LOG_ERR, "is%d: device timeout\n", unit);
        is_reset(unit);
}


/* Lance initialisation block set up */
init_mem(unit)
	int unit;
{
	int i;
	u_long temp;
	struct is_softc *is = &is_softc[unit];

	/* Allocate memory */

	/*
	 * XXX - hopefully have better way to get dma'able memory later,
	 * this code assumes that the physical memory address returned
	 * from malloc will be below 16Mb. The Lance's address registers
	 * are only 16 bits wide!
	 */

#define MAXMEM ((NRBUF+NTBUF)*(BUFSIZE) + (NRBUF+NTBUF)*sizeof(struct mds) + 8)

	/* 
	 * XXX - If we've been here before then free 
	 * the previously allocated memory
	 */
	if (is->lance_mem)
		free(is->lance_mem,M_TEMP);

	is->lance_mem = malloc(MAXMEM,M_TEMP,M_NOWAIT);
	if (!is->lance_mem) {
		printf("is%d : Couldn't allocate memory for card\n",unit);
		return;
	}
	temp = (u_long) is->lance_mem;

	/* Align message descriptors on quad word boundary 
		(this is essential) */

	temp = (temp+8) - (temp%8);
	is->rd = (struct mds *) temp;
	is->td = (struct mds *) (temp + (NRBUF*sizeof(struct mds)));
	temp += (NRBUF+NTBUF) * sizeof(struct mds);

	init_block[unit].mode = 0;
	init_block[unit].rdra = kvtop(is->rd);
	init_block[unit].rlen = ((kvtop(is->rd) >> 16) & 0xff) | (RLEN<<13);
	init_block[unit].tdra = kvtop(is->td);
	init_block[unit].tlen = ((kvtop(is->td) >> 16) & 0xff) | (TLEN<<13);

	/* Set up receive ring descriptors */

	is->rbuf = (unsigned char *)temp;
	for (i=0; i<NRBUF; i++) {
		(is->rd+i)->addr = kvtop(temp);
		(is->rd+i)->flags= ((kvtop(temp) >> 16) & 0xff) | OWN;
		(is->rd+i)->bcnt = -BUFSIZE;
		(is->rd+i)->mcnt = 0;
		temp += BUFSIZE;
	}

	/* Set up transmit ring descriptors */

	is->tbuf = (unsigned char *)temp;
	for (i=0; i<NTBUF; i++) {
		(is->td+i)->addr = kvtop(temp);
		(is->td+i)->flags= ((kvtop(temp) >> 16) & 0xff);
		(is->td+i)->bcnt = 0;
		(is->td+i)->mcnt = 0;
		temp += BUFSIZE;
	}

}

/*
 * Initialization of interface; set up initialization block
 * and transmit/receive descriptor rings.
 */
is_init(unit)
	int unit;
{
	register struct is_softc *is = &is_softc[unit];
	struct ifnet *ifp = &is->arpcom.ac_if;
	int s;
	register i;

	/* Address not known */
 	if (ifp->if_addrlist == (struct ifaddr *)0) return;

	s = splnet();
	is->last_rd = is->last_td = is->no_td = 0;

	/* Set up lance's memory area */
	init_mem(unit);

	/* Stop Lance to get access to other registers */
	iswrcsr(unit,0,STOP);

	/* Get ethernet address */
	for (i=0; i<ETHER_ADDR_LEN; i++) 
		init_block[unit].padr[i] = is->arpcom.ac_enaddr[i];

#if NBPFILTER > 0
        /*
         * Initialize multicast address hashing registers to accept
         *       all multicasts (only used when in promiscuous mode)
         */
        for (i = 0; i < 8; ++i)
		init_block[unit].ladrf[i] = 0xff;
#endif


	/* No byte swapping etc */
	iswrcsr(unit,3,0);

	/* Give lance the physical address of its memory area */
	iswrcsr(unit,1,kvtop(&init_block[unit]));
	iswrcsr(unit,2,(kvtop(&init_block[unit]) >> 16) & 0xff);

	/* OK, let's try and initialise the Lance */
	iswrcsr(unit,0,INIT);

	/* Wait for initialisation to finish */
	for(i=0; i<1000; i++){
		if (isrdcsr(unit,0)&IDON)
			break;
	}
	if (isrdcsr(unit,0)&IDON) {
		/* Start lance */
		iswrcsr(unit,0,STRT|IDON|INEA);
		ifp->if_flags |= IFF_RUNNING;
        	ifp->if_flags &= ~IFF_OACTIVE;

		is_start(ifp);
	}
	else 
		printf("is%d: card failed to initialise\n", unit);
	
	 (void) splx(s);
}

/*
 * Setup output on interface.
 * Get another datagram to send off of the interface queue,
 * and map it to the interface before starting the output.
 * called only at splimp or interrupt level.
 */
is_start(ifp)
	struct ifnet *ifp;
{
	int unit = ifp->if_unit;
	register struct is_softc *is = &is_softc[unit];
	struct mbuf *m0, *m;
	unsigned char *buffer;
	u_short len;
	int i;
	struct mds *cdm;


	if ((is->arpcom.ac_if.if_flags & IFF_RUNNING) == 0)
		return;

	do {
		cdm = (is->td + is->last_td);
		if (cdm->flags&OWN)
			return;
	
		IF_DEQUEUE(&is->arpcom.ac_if.if_snd, m);

		if (m == 0)
			return;

		/*
	 	* Copy the mbuf chain into the transmit buffer
	 	*/

		buffer = is->tbuf+(BUFSIZE*is->last_td);
		len=0;
		for (m0=m; m != 0; m=m->m_next) {
			bcopy(mtod(m,caddr_t),buffer,m->m_len);
			buffer += m->m_len;
			len += m->m_len;
		}
#if NBPFILTER > 0
        if (is->bpf) {
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
                m_copydata(m0, 0, sizeof(struct ether_header), ep);
                eh = (struct ether_header *) ep;
                ep += sizeof(struct ether_header);
                etype = ntohs(eh->ether_type);
                if (etype >= ETHERTYPE_TRAIL &&
                    etype < ETHERTYPE_TRAIL+ETHERTYPE_NTRAILER) {
                        datasize = ((etype - ETHERTYPE_TRAIL) << 9);
                        off = datasize + sizeof(struct ether_header);

                        /* copy trailer_header into a data structure */
                        m_copydata(m0, off, sizeof(struct trailer_header),
                                &trailer_header.ether_type);

                        /* copy residual data */
			resid = trailer_header.ether_residual -
				sizeof(struct trailer_header);
			resid = ntohs(resid);
                        m_copydata(m0, off+sizeof(struct trailer_header),
                                resid, ep);
                        ep += resid;

                        /* copy data */
                        m_copydata(m0, sizeof(struct ether_header),
                                datasize, ep);
                        ep += datasize;

                        /* restore original ether packet type */
                        eh->ether_type = trailer_header.ether_type;

                        bpf_tap(is->bpf, ether_packet, ep - ether_packet);
                } else
                        bpf_mtap(is->bpf, m0);
        }
#endif

		
		m_freem(m0);
		len = MAX(len,ETHER_MIN_LEN);

		/*
	 	* Init transmit registers, and set transmit start flag.
	 	*/

		cdm->flags |= (OWN|STP|ENP);
		cdm->bcnt = -len;
		cdm->mcnt = 0;
#if ISDEBUG > 3
		xmit_print(unit,is->last_td);
#endif
		
		iswrcsr(unit,0,TDMD|INEA);
		if (++is->last_td >= NTBUF)
			is->last_td=0;
		}while(++is->no_td < NTBUF);
		is->no_td = NTBUF;
		is->arpcom.ac_if.if_flags |= IFF_OACTIVE;	
#if ISDEBUG >4
	printf("no_td = %x, last_td = %x\n",is->no_td, is->last_td);
#endif
		return(0);	
}


/*
 * Controller interrupt.
 */
isintr(unit)
{
	register struct is_softc *is = &is_softc[unit];
	u_short isr;

	while((isr=isrdcsr(unit,0))&INTR) {
		if (isr&ERR) {
			if (isr&BABL){
				printf("is%d: BABL\n",unit);
				is->arpcom.ac_if.if_oerrors++;
			}
			if (isr&CERR) {
				printf("is%d: CERR\n",unit);
				is->arpcom.ac_if.if_collisions++;
			}
			if (isr&MISS) {
				printf("is%d: MISS\n",unit);
				is->arpcom.ac_if.if_ierrors++;
			}
			if (isr&MERR)
				printf("is%d: MERR\n",unit);
			iswrcsr(unit,0,BABL|CERR|MISS|MERR|INEA);
		}
		if (!(isr&RXON)) {
			printf("is%d: !(isr&RXON)\n", unit);
			is->arpcom.ac_if.if_ierrors++;
			is_reset(unit);
			return(1);
		}
		if (!(isr&TXON)) {
			printf("is%d: !(isr&TXON)\n", unit);
			is->arpcom.ac_if.if_oerrors++;
			is_reset(unit);
			return(1);
		}

		if (isr&RINT) {
			/* reset watchdog timer */
			is->arpcom.ac_if.if_timer = 0;
			is_rint(unit);
		}
		if (isr&TINT) {
			/* reset watchdog timer */
			is->arpcom.ac_if.if_timer = 0;
			iswrcsr(unit,0,TINT|INEA);
			istint(unit);
		}
	}
}

istint(unit) 
	int unit;
{
	struct is_softc *is = &is_softc[unit];
	register struct ifnet *ifp = &is->arpcom.ac_if;
	int i,loopcount=0;
	struct mds *cdm;

	is->arpcom.ac_if.if_opackets++;
	do {
		if ((i=is->last_td - is->no_td) < 0)
			i+=NTBUF;
		cdm = (is->td+i);
#if ISDEBUG >4
	printf("Trans cdm = %x\n",cdm);
#endif
		if (cdm->flags&OWN) {
			if (loopcount)
				break;
			return;
		}
		loopcount++;	
		is->arpcom.ac_if.if_flags &= ~IFF_OACTIVE;
	}while(--is->no_td > 0);
	is_start(ifp);	
	
}

#define NEXTRDS \
	if (++rmd == NRBUF) rmd=0, cdm=is->rd; else ++cdm
	
/* only called from one place, so may as well integrate */
static inline void is_rint(int unit)
{
	register struct is_softc *is=&is_softc[unit];
	register int rmd = is->last_rd;
	struct mds *cdm = (is->rd + rmd);

	/* Out of sync with hardware, should never happen */
	
	if (cdm->flags & OWN) {
		printf("is%d: error: out of sync\n",unit);
		iswrcsr(unit,0,RINT|INEA);
		return;
	}

	/* Process all buffers with valid data */
	while (!(cdm->flags&OWN)) {
		/* Clear interrupt to avoid race condition */
		iswrcsr(unit,0,RINT|INEA);
		if (cdm->flags&ERR) {
			if (cdm->flags&FRAM)
				printf("is%d: FRAM\n",unit);
			if (cdm->flags&OFLO)
				printf("is%d: OFLO\n",unit);
			if (cdm->flags&CRC)
				printf("is%d: CRC\n",unit);
			if (cdm->flags&RBUFF)
				printf("is%d: RBUFF\n",unit);
		}else 
		if (cdm->flags&(STP|ENP) != (STP|ENP)) {
			do {
				iswrcsr(unit,0,RINT|INEA);
				cdm->mcnt = 0;
				cdm->flags |= OWN;	
				NEXTRDS;
			}while (!(cdm->flags&(OWN|ERR|STP|ENP)));
			is->last_rd = rmd;
			printf("is%d: Chained buffer\n",unit);
			if ((cdm->flags & (OWN|ERR|STP|ENP)) != ENP) {
				is_reset(unit);
				return;
			}
		}else
			{
#if ISDEBUG >2
	recv_print(unit,is->last_rd);
#endif
			isread(is,is->rbuf+(BUFSIZE*rmd),(int)cdm->mcnt);
			is->arpcom.ac_if.if_ipackets++;
			}
			
		cdm->flags |= OWN;
		cdm->mcnt = 0;
		NEXTRDS;
#if ISDEBUG >4
	printf("is->last_rd = %x, cdm = %x\n",is->last_rd,cdm);
#endif
	} /* while */
	is->last_rd = rmd;
} /* is_rint */


/*
 * Pass a packet to the higher levels.
 * We deal with the trailer protocol here.
 */
static inline void 
isread(struct is_softc *is, unsigned char *buf, int len)
{
        register struct ether_header *eh;
        struct mbuf *m;
        int off, resid;
        register struct ifqueue *inq;

        /*
         * Deal with trailer protocol: if type is trailer type
         * get true type from first 16-bit word past data.
         * Remember that type was trailer by setting off.
         */
        eh = (struct ether_header *)buf;
        eh->ether_type = ntohs((u_short)eh->ether_type);
        len = len - sizeof(struct ether_header) - 4;
#define nedataaddr(eh, off, type)       ((type)(((caddr_t)((eh)+1)+(off))))
        if (eh->ether_type >= ETHERTYPE_TRAIL &&
            eh->ether_type < ETHERTYPE_TRAIL+ETHERTYPE_NTRAILER) {
                off = (eh->ether_type - ETHERTYPE_TRAIL) * 512;
                if (off >= ETHERMTU) return;            /* sanity */
                eh->ether_type = ntohs(*nedataaddr(eh, off, u_short *));
                resid = ntohs(*(nedataaddr(eh, off+2, u_short *)));
                if (off + resid > len) return;          /* sanity */
                len = off + resid;
        } else  off = 0;

        if (len == 0) return;

        /*
         * Pull packet off interface.  Off is nonzero if packet
         * has trailing header; neget will then force this header
         * information to be at the front, but we still have to drop
         * the type and length which are at the front of any trailer data.
         */
        is->arpcom.ac_if.if_ipackets++;
        m = isget(buf, len, off, &is->arpcom.ac_if);
        if (m == 0) return;
#if NBPFILTER > 0
        /*
         * Check if there's a BPF listener on this interface.
         * If so, hand off the raw packet to bpf. 
         */
        if (is->bpf) {
                bpf_mtap(is->bpf, m);

                /*
                 * Note that the interface cannot be in promiscuous mode if
                 * there are no BPF listeners.  And if we are in promiscuous
                 * mode, we have to check if this packet is really ours.
                 *
                 * XXX This test does not support multicasts.
                 */
                if ((is->arpcom.ac_if.if_flags & IFF_PROMISC) &&
                        bcmp(eh->ether_dhost, is->arpcom.ac_enaddr,
                                sizeof(eh->ether_dhost)) != 0 &&
                        bcmp(eh->ether_dhost, etherbroadcastaddr,
                                sizeof(eh->ether_dhost)) != 0) {

                        m_freem(m);
                        return;
                }
        }
#endif


        ether_input(&is->arpcom.ac_if, eh, m);
}

/*
 * Supporting routines
 */

/*
 * Pull read data off a interface.
 * Len is length of data, with local net header stripped.
 * Off is non-zero if a trailer protocol was used, and
 * gives the offset of the trailer information.
 * We copy the trailer information and then all the normal
 * data into mbufs.  When full cluster sized units are present
 * we copy into clusters.
 */
struct mbuf *
isget(buf, totlen, off0, ifp)
        caddr_t buf;
        int totlen, off0;
        struct ifnet *ifp;
{
        struct mbuf *top, **mp, *m, *p;
        int off = off0, len;
        register caddr_t cp = buf;
        char *epkt;

        buf += sizeof(struct ether_header);
        cp = buf;
        epkt = cp + totlen;


        if (off) {
                cp += off + 2 * sizeof(u_short);
                totlen -= 2 * sizeof(u_short);
        }

        MGETHDR(m, M_DONTWAIT, MT_DATA);
        if (m == 0)
                return (0);
        m->m_pkthdr.rcvif = ifp;
        m->m_pkthdr.len = totlen;
        m->m_len = MHLEN;
        top = 0;
        mp = &top;
        while (totlen > 0) {
                if (top) {
                        MGET(m, M_DONTWAIT, MT_DATA);
                        if (m == 0) {
                                m_freem(top);
                                return (0);
                        }
                        m->m_len = MLEN;
                }
                len = min(totlen, epkt - cp);
                if (len >= MINCLSIZE) {
                        MCLGET(m, M_DONTWAIT);
                        if (m->m_flags & M_EXT)
                                m->m_len = len = min(len, MCLBYTES);
                        else
                                len = m->m_len;
                } else {
                        /*
                         * Place initial small packet/header at end of mbuf.
                         */
                        if (len < m->m_len) {
                                if (top == 0 && len + max_linkhdr <= m->m_len)
                                        m->m_data += max_linkhdr;
                                m->m_len = len;
                        } else
                                len = m->m_len;
                }
                bcopy(cp, mtod(m, caddr_t), (unsigned)len);
                cp += len;
                *mp = m;
                mp = &m->m_next;
                totlen -= len;
                if (cp == epkt)
                        cp = buf;
        }
        return (top);
}


/*
 * Process an ioctl request.
 */
is_ioctl(ifp, cmd, data)
	register struct ifnet *ifp;
	int cmd;
	caddr_t data;
{
	register struct ifaddr *ifa = (struct ifaddr *)data;
	int unit = ifp->if_unit;
	struct is_softc *is = &is_softc[unit];
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	s = splnet();

	switch (cmd) {

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			is_init(ifp->if_unit);	/* before arpwhohas */
                        /*
                         * See if another station has *our* IP address.
                         * i.e.: There is an address conflict! If a
                         * conflict exists, a message is sent to the
                         * console.
                         */
			((struct arpcom *)ifp)->ac_ipaddr =
				IA_SIN(ifa)->sin_addr;
			arpwhohas((struct arpcom *)ifp, &IA_SIN(ifa)->sin_addr);
			break;
#endif
#ifdef NS
                /*
                 * XXX - This code is probably wrong
                 */
		case AF_NS:
		    {
			register struct ns_addr *ina = &(IA_SNS(ifa)->sns_addr);

			if (ns_nullhost(*ina))
				ina->x_host =
					*(union ns_host *)(is->arpcom.ac_enaddr);
			else {
				/* 
				 *
				 */
				bcopy((caddr_t)ina->x_host.c_host,
				    (caddr_t)is->arpcom.ac_enaddr,
					sizeof(is->arpcom.ac_enaddr));
			}
                        /*
                         * Set new address
                         */
			is_init(ifp->if_unit); 
			break;
		    }
#endif
		default:
			is_init(ifp->if_unit);
			break;
		}
		break;

	case SIOCSIFFLAGS:
                /*
                 * If interface is marked down and it is running, then stop it
                 */
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    ifp->if_flags & IFF_RUNNING) {
			iswrcsr(unit,0,STOP);
			ifp->if_flags &= ~IFF_RUNNING;
		} else {
                /*
                 * If interface is marked up and it is stopped, then start it
                 */
			if ((ifp->if_flags & IFF_UP) &&
		    		(ifp->if_flags & IFF_RUNNING) == 0)
			is_init(ifp->if_unit);
                }
#if NBPFILTER > 0
                if (ifp->if_flags & IFF_PROMISC) {
                        /*
                         * Set promiscuous mode on interface.
                         *      XXX - for multicasts to work, we would need to
                         *              write 1's in all bits of multicast
                         *              hashing array. For now we assume that
                         *              this was done in is_init().
                         */
			 init_block[unit].mode = PROM;	
                } else
                        /*
                         * XXX - for multicasts to work, we would need to
                         *      rewrite the multicast hashing array with the
                         *      proper hash (would have been destroyed above).
                         */
			{ /* Don't know about this */};
#endif
		break;

#ifdef notdef
	case SIOCGHWADDR:
		bcopy((caddr_t)is->arpcom.ac_enaddr, (caddr_t) &ifr->ifr_data,
			sizeof(is->arpcom.ac_enaddr));
		break;
#endif

	default:
		error = EINVAL;
	}
	(void) splx(s);
	return (error);
}

#ifdef ISDEBUG
recv_print(unit,no)
	int unit,no;
{
	register struct is_softc *is=&is_softc[unit];
	struct mds *rmd;
	int len,i,printed=0;
	
	rmd = (is->rd+no);
	len = rmd->mcnt;
	printf("is%d: Receive buffer %d, len = %d\n",unit,no,len);
	printf("is%d: Status %x\n",unit,isrdcsr(unit,0));
	for (i=0; i<len; i++) {
		if (!printed) {
			printed=1;
			printf("is%d: data: ", unit);
		}
		printf("%x ",*(is->rbuf+(BUFSIZE*no)+i));
	}
	if (printed)
		printf("\n");
}
		
xmit_print(unit,no)
	int unit,no;
{
	register struct is_softc *is=&is_softc[unit];
	struct mds *rmd;
	int i, printed=0;
	u_short len;
	
	rmd = (is->td+no);
	len = -(rmd->bcnt);
	printf("is%d: Transmit buffer %d, len = %d\n",unit,no,len);
	printf("is%d: Status %x\n",unit,isrdcsr(unit,0));
	printf("is%d: addr %x, flags %x, bcnt %x, mcnt %x\n",
		unit,rmd->addr,rmd->flags,rmd->bcnt,rmd->mcnt);
	for (i=0; i<len; i++)  {
		if (!printed) {
			printed = 1;
			printf("is%d: data: ", unit);
		}
		printf("%x ",*(is->tbuf+(BUFSIZE*no)+i));
	}
	if (printed)
		printf("\n");
}
#endif /* ISDEBUG */
		
#endif /* NIS > 0 */
