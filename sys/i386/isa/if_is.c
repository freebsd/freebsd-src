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

char *card_type[] = {"Unknown",
                     "BICC Isolan",
                     "NE2100"};

char *ic_type[] = {"Unknown",
                   "Am7990 LANCE",
                   "Am79960 PCnet_ISA"};
                 

struct	is_softc {
	struct arpcom arpcom;             /* Ethernet common part */
	int iobase;
	int rap;
	int rdp;
	int ic_type;                         /* Am 7990 or Am79960 */
	int card_type;
	int is_debug;
	struct init_block  *init_block;   /* Lance initialisation block */
	struct mds 	*rd;
	struct mds	*td;
	unsigned char	*rbuf;
	unsigned char	*tbuf;
	int	last_rd;
	int	last_td;
	int	no_td;
	caddr_t bpf;                      /* BPF "magic cookie" */

} is_softc[NIS] ;


/* Function prototypes */
static int is_probe(struct isa_device *);
static int is_attach(struct isa_device *);
static void is_watchdog(int);
static int is_ioctl(struct ifnet *, int, caddr_t);
static void is_init(int);
static void is_start(struct ifnet *);
static void istint(int);
static void recv_print(int, int);
static void xmit_print(int, int);



static inline void is_rint(int unit);
static inline void isread(struct is_softc*, unsigned char*, int);

struct	mbuf *isget();

struct	isa_driver isdriver = {
	is_probe,
	is_attach,
	"is"
};

void
iswrcsr(unit,port,val)
	int unit;
	u_short port;
	u_short val;
{
	outw(is_softc[unit].rap,port);
	outw(is_softc[unit].rdp,val);
}

u_short isrdcsr(unit,port)
	int unit;
	u_short port;
{
	outw(is_softc[unit].rap,port);
	return(inw(is_softc[unit].rdp));
} 

int
is_probe(isa_dev)
	struct isa_device *isa_dev;
{
	int unit = isa_dev->id_unit ;
	int nports;

int i;
	is_softc[unit].iobase = isa_dev->id_iobase;

	/*
	 * It's impossible to do a non-invasive probe of the 
	 * LANCE and PCnet_ISA. The LANCE requires setting the
	 * STOP bit to access the registers and the PCnet_ISA
	 * address port resets to an unknown state!!
	 */

	/*
	 * Check for BICC cards first since for the NE2100 and
	 * PCnet-ISA cards this write will hit the Address PROM. 
	 */

printf("Dumping io space for is%d starting at %x\n",unit,is_softc[unit].iobase);
for (i=0; i< 32; i++)
	printf(" %x ",inb(is_softc[unit].iobase+i));
printf("\n");

	if (nports = bicc_probe(unit))
		return (nports);
	if (nports = ne2100_probe(unit))
		return (nports);


	return (0);
}

int
ne2100_probe(unit)
	int unit;
{
struct is_softc *is = &is_softc[unit];
int i;

	is->rap = is->iobase + NE2100_RAP;
	is->rdp = is->iobase + NE2100_RDP;

	if (is->ic_type = lance_probe(unit)) {
		is->card_type = NE2100;
		/* 
		 * Extract the physical MAC address from ROM
		 */
		for(i=0;i<ETHER_ADDR_LEN;i++)
			is->arpcom.ac_enaddr[i]=inb(is->iobase+i);

		/* 
		 * Return number of I/O ports used by card 
		 */
		return (24);
	}
	return (0);
}
			

int
bicc_probe(unit)
	int unit;
{
struct is_softc *is = &is_softc[unit];
int i;

	is->rap = is->iobase + BICC_RAP;
	is->rdp = is->iobase + BICC_RDP;

	if (is->ic_type = lance_probe(unit)) {
		is->card_type = BICC;

		/*
		 * Extract the physical ethernet address from ROM
		 */

		for(i=0;i<ETHER_ADDR_LEN;i++)
			is->arpcom.ac_enaddr[i]=inb(is->iobase+(i*2));

		/* 
		 * Return number of I/O ports used by card 
		 */
		return (16);
	}
	return (0);
}


/* 
 * Determine which, if any, of the LANCE or 
 * PCnet-ISA are present on the card.
 */

int
lance_probe(unit)
	int unit;
{
int type=0;

	/* 
	 * Have to reset the LANCE to get any 
	 * stable information from it.
	 */

	iswrcsr(unit,0,STOP);
	DELAY(100);

	if (isrdcsr(unit,0) != STOP)
		/* 
		 * This either isn't a LANCE 
		 * or there's a major problem.
		 */
		return(0);

	/* 
	 * Depending on which controller it is, CSR3 will have 
	 * different settable bits. Write to them all and see which ones
	 * get set.
	 */

	iswrcsr(unit,3, LANCE_MASK);

	if (isrdcsr(unit,3) == LANCE_MASK)
		type = LANCE;

	if (isrdcsr(unit,3) == PCnet_ISA_MASK)
		type = PCnet_ISA;

	return (type);
}

/*
 * Reset of interface.
 */
static void
is_reset(int unit, int uban)
{
	int s;
	struct is_softc *is = &is_softc[unit];

	if (unit >= NIS)
		return;
	printf("is%d: reset\n", unit);
	is_init(unit);
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
	 * XXX -- not sure this is right place to do this
	 * Allocate memory for use by Lance
	 * Memory allocated for:
	 * 	initialisation block,
	 * 	ring descriptors,
	 * 	transmit and receive buffers.
	 */

	/*
	 * XXX - hopefully have better way to get dma'able memory later,
	 * this code assumes that the physical memory address returned
	 * from malloc will be below 16Mb. The Lance's address registers
	 * are only 16 bits wide!
	 */

#define MAXMEM ((NRBUF+NTBUF)*(BUFSIZE) + (NRBUF+NTBUF)*sizeof(struct mds) \
                 + sizeof(struct init_block) + 8)
	is->init_block = (struct init_block *)malloc(MAXMEM,M_TEMP,M_NOWAIT);
	if (!is->init_block) {
		printf("is%d : Couldn't allocate memory for card\n",unit);
	}
	/* 
	 * XXX -- should take corrective action if not
	 * quadword alilgned, the 8 byte slew factor in MAXMEM
	 * allows for this.
	 */

	if ((u_long)is->init_block & 0x3) 
		printf("is%d: memory allocated not quadword aligned\n");

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
	printf("%s, %s\n",ic_type[is->ic_type],card_type[is->card_type]);

#if NBPFILTER > 0
	bpfattach(&is->bpf, ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif
	return 1;
}

static void
is_watchdog(unit)
        int unit;
{
        log(LOG_ERR, "is%d: device timeout\n", unit);
        is_reset(unit, 0);
}


/* Lance initialisation block set up */
void
init_mem(unit)
	int unit;
{
	int i;
	void *temp;
	struct is_softc *is = &is_softc[unit];

	/*
	 * At this point we assume that the
	 * memory allocated to the Lance is
	 * quadword aligned. If it isn't
	 * then the initialisation is going
	 * fail later on.
	 */


	/* 
	 * Set up lance initialisation block
	 */

	temp = (void *)is->init_block;
	temp += sizeof(struct init_block);
	is->rd = (struct mds *) temp;
	is->td = (struct mds *) (temp + (NRBUF*sizeof(struct mds)));
	temp += (NRBUF+NTBUF) * sizeof(struct mds);

	is->init_block->mode = 0;
	for (i=0; i<ETHER_ADDR_LEN; i++) 
		is->init_block->padr[i] = is->arpcom.ac_enaddr[i];
	for (i = 0; i < 8; ++i)
		is->init_block->ladrf[i] = MULTI_INIT_ADDR;
	is->init_block->rdra = kvtop(is->rd);
	is->init_block->rlen = ((kvtop(is->rd) >> 16) & 0xff) | (RLEN<<13);
	is->init_block->tdra = kvtop(is->td);
	is->init_block->tlen = ((kvtop(is->td) >> 16) & 0xff) | (TLEN<<13);


	/* 
	 * Set up receive ring descriptors
	 */

	is->rbuf = (unsigned char *)temp;
	for (i=0; i<NRBUF; i++) {
		(is->rd+i)->addr = kvtop(temp);
		(is->rd+i)->flags= ((kvtop(temp) >> 16) & 0xff) | OWN;
		(is->rd+i)->bcnt = -BUFSIZE;
		(is->rd+i)->mcnt = 0;
		temp += BUFSIZE;
	}

	/* 
	 * Set up transmit ring descriptors
	 */

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

static void
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

	/* 
	 * Lance must be stopped
	 * to access registers.
	 */
 
	iswrcsr(unit,0,STOP);

	is->last_rd = is->last_td = is->no_td = 0;

	/* Set up lance's memory area */
	init_mem(unit);

	/* No byte swapping etc */
	iswrcsr(unit,3,0);

	/* Give lance the physical address of its memory area */
	iswrcsr(unit,1,kvtop(is->init_block));
	iswrcsr(unit,2,(kvtop(is->init_block) >> 16) & 0xff);

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
static void
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
                                (caddr_t)&trailer_header.ether_type);

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
#ifdef ISDEBUG
		if (is->is_debug)
			xmit_print(unit,is->last_td);
#endif
		
		iswrcsr(unit,0,TDMD|INEA);
		if (++is->last_td >= NTBUF)
			is->last_td=0;
		}while(++is->no_td < NTBUF);
		is->no_td = NTBUF;
		is->arpcom.ac_if.if_flags |= IFF_OACTIVE;	
#ifdef ISDEBUG
		if (is->is_debug)	
			printf("no_td = %x, last_td = %x\n",is->no_td, is->last_td);
#endif
}


/*
 * Controller interrupt.
 */
void
isintr(unit)
	int unit;
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
			is_reset(unit, 0);
			return;
		}
		if (!(isr&TXON)) {
			printf("is%d: !(isr&TXON)\n", unit);
			is->arpcom.ac_if.if_oerrors++;
			is_reset(unit, 0);
			return;
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

static void
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
#ifdef ISDEBUG
	if (is->is_debug)
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
				is_reset(unit, 0);
				return;
			}
		}else
			{
#ifdef ISDEBUG
			if (is->is_debug)
				recv_print(unit,is->last_rd);
#endif
			isread(is,is->rbuf+(BUFSIZE*rmd),(int)cdm->mcnt);
			is->arpcom.ac_if.if_ipackets++;
			}
			
		cdm->flags |= OWN;
		cdm->mcnt = 0;
		NEXTRDS;
#ifdef ISDEBUG
		if (is->is_debug)
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
int
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
#ifdef ISDEBUG
		if (ifp->if_flags & IFF_DEBUG)
			is->is_debug = 1;
		else
			is->is_debug = 0;
#endif
#if NBPFILTER > 0
                if (ifp->if_flags & IFF_PROMISC) {
                        /*
                         * Set promiscuous mode on interface.
                         *      XXX - for multicasts to work, we would need to
                         *              write 1's in all bits of multicast
                         *              hashing array. For now we assume that
                         *              this was done in is_init().
                         */
			 is->init_block->mode = PROM;	
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
void
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

void
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
