/*-
 * Copyright (c) 1994 Matt Thomas (thomas@lkg.dec.com)
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
 * $Id: if_le.c,v 1.40 1997/08/21 09:01:00 fsmp Exp $
 */

/*
 * DEC EtherWORKS 2 Ethernet Controllers
 * DEC EtherWORKS 3 Ethernet Controllers
 *
 * Written by Matt Thomas
 * BPF support code stolen directly from if_ec.c
 *
 *   This driver supports the DEPCA, DE100, DE101, DE200, DE201,
 *   DE2002, DE203, DE204, DE205, and DE422 cards.
 */

#include "le.h"
#if NLE > 0

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/malloc.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>

#include "bpfilter.h"

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

#include <machine/clock.h>

#include <i386/isa/isa_device.h>
#include <i386/isa/icu.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

/* Forward declarations */
typedef struct le_softc le_softc_t;
typedef struct le_board le_board_t;

typedef u_short le_mcbits_t;
#define	LE_MC_NBPW_LOG2		4
#define LE_MC_NBPW		(1 << LE_MC_NBPW_LOG2)
#if __FreeBSD__ > 1
#define	IF_RESET_ARGS	int unit
#define	LE_RESET(ifp)	(((sc)->if_reset)((sc)->le_if.if_unit))
#else
#define	IF_RESET_ARGS	int unit, int dummy
#define	LE_RESET(ifp)	(((sc)->if_reset)((sc)->le_if.if_unit, 0))
#endif

#if !defined(LE_NOLEMAC)
/*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * Start of DEC EtherWORKS III (LEMAC) dependent structures
 *
 */
#include <i386/isa/ic/lemac.h>		/* Include LEMAC definitions */

static int lemac_probe(le_softc_t *sc, const le_board_t *bd, int *msize);

struct le_lemac_info {
    u_int lemac__lastpage;		/* last 2K page */
    u_int lemac__memmode;		/* Are we in 2K, 32K, or 64K mode */
    u_int lemac__membase;		/* Physical address of start of RAM */
    u_int lemac__txctl;			/* Transmit Control Byte */
    u_int lemac__txmax;			/* Maximum # of outstanding transmits */
    le_mcbits_t lemac__mctbl[LEMAC_MCTBL_SIZE/sizeof(le_mcbits_t)];
					/* local copy of multicast table */
    u_char lemac__eeprom[LEMAC_EEP_SIZE]; /* local copy eeprom */
    char lemac__prodname[LEMAC_EEP_PRDNMSZ+1]; /* prodname name */
#define	lemac_lastpage		le_un.un_lemac.lemac__lastpage
#define	lemac_memmode		le_un.un_lemac.lemac__memmode
#define	lemac_membase		le_un.un_lemac.lemac__membase
#define	lemac_txctl		le_un.un_lemac.lemac__txctl
#define	lemac_txmax		le_un.un_lemac.lemac__txmax
#define	lemac_mctbl		le_un.un_lemac.lemac__mctbl
#define	lemac_eeprom		le_un.un_lemac.lemac__eeprom
#define	lemac_prodname		le_un.un_lemac.lemac__prodname
};
#endif /* !defined(LE_NOLEMAC) */

#if !defined(LE_NOLANCE)
/*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * Start of DEC EtherWORKS II (LANCE) dependent structures
 *
 */

#include <i386/isa/ic/am7990.h>

#ifndef LN_DOSTATS
#define	LN_DOSTATS	1
#endif

static int depca_probe(le_softc_t *sc, const le_board_t *bd, int *msize);

typedef struct lance_descinfo lance_descinfo_t;
typedef struct lance_ring lance_ring_t;

typedef unsigned lance_addr_t;

struct lance_descinfo {
    caddr_t di_addr;			/* address of descriptor */
    lance_addr_t di_bufaddr;		/* LANCE address of buffer owned by descriptor */
    unsigned di_buflen;			/* size of buffer owned by descriptor */
    struct mbuf *di_mbuf;		/* mbuf being transmitted/received */
};

struct lance_ring {
    lance_descinfo_t *ri_first;		/* Pointer to first descriptor in ring */
    lance_descinfo_t *ri_last;		/* Pointer to last + 1 descriptor in ring */
    lance_descinfo_t *ri_nextin;	/* Pointer to next one to be given to HOST */
    lance_descinfo_t *ri_nextout;	/* Pointer to next one to be given to LANCE */
    unsigned ri_max;			/* Size of Ring - 1 */
    unsigned ri_free;			/* Number of free rings entires (owned by HOST) */
    lance_addr_t ri_heap;			/* Start of RAM for this ring */
    lance_addr_t ri_heapend;		/* End + 1 of RAM for this ring */
    lance_addr_t ri_outptr;			/* Pointer to first output byte */
    unsigned ri_outsize;		/* Space remaining for output */
};

struct le_lance_info {
    unsigned lance__csr1;		/* LANCE Address of init block (low 16) */
    unsigned lance__csr2;		/* LANCE Address of init block (high 8) */
    unsigned lance__csr3;		/* Copy of CSR3 */
    unsigned lance__rap;		/* IO Port Offset of RAP */
    unsigned lance__rdp;		/* IO Port Offset of RDP */
    unsigned lance__ramoffset;		/* Offset to valid LANCE RAM */
    unsigned lance__ramsize;		/* Amount of RAM shared by LANCE */
    unsigned lance__rxbufsize;		/* Size of a receive buffer */
    ln_initb_t lance__initb;		/* local copy of LANCE initblock */
    ln_initb_t *lance__raminitb;	/* copy to board's LANCE initblock (debugging) */
    ln_desc_t *lance__ramdesc;		/* copy to board's LANCE descriptors (debugging) */
    lance_ring_t lance__rxinfo;		/* Receive ring information */
    lance_ring_t lance__txinfo;		/* Transmit ring information */
#define	lance_csr1		le_un.un_lance.lance__csr1
#define	lance_csr2		le_un.un_lance.lance__csr2
#define	lance_csr3		le_un.un_lance.lance__csr3
#define	lance_rap		le_un.un_lance.lance__rap
#define	lance_rdp		le_un.un_lance.lance__rdp
#define	lance_ramoffset		le_un.un_lance.lance__ramoffset
#define	lance_ramsize		le_un.un_lance.lance__ramsize
#define	lance_rxbufsize		le_un.un_lance.lance__rxbufsize
#define	lance_initb		le_un.un_lance.lance__initb
#define	lance_raminitb		le_un.un_lance.lance__raminitb
#define	lance_ramdesc		le_un.un_lance.lance__ramdesc
#define	lance_rxinfo		le_un.un_lance.lance__rxinfo
#define	lance_txinfo		le_un.un_lance.lance__txinfo
};
#endif /* !defined(LE_NOLANCE) */

/*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * Start of Common Code
 *
 */

static void (*le_intrvec[NLE])(le_softc_t *sc);

/*
 * Ethernet status, per interface.
 */
struct le_softc {
    struct arpcom le_ac;		/* Common Ethernet/ARP Structure */
    void (*if_init) __P((int));		/* Interface init routine */
    void (*if_reset) __P((int));	/* Interface reset routine */
    caddr_t le_membase;			/* Starting memory address (virtual) */
    unsigned le_iobase;			/* Starting I/O base address */
    unsigned le_irq;			/* Interrupt Request Value */
    unsigned le_flags;			/* local copy of if_flags */
#define	LE_BRDCSTONLY	0x01000000	/* If only broadcast is enabled */
    u_int le_mcmask;			/* bit mask for CRC-32 for multicast hash */
    le_mcbits_t *le_mctbl;		/* pointer to multicast table */
    const char *le_prodname;		/* product name DE20x-xx */
    u_char le_hwaddr[6];		/* local copy of hwaddr */
    unsigned le_scast_drops;		/* singlecast drops */
    unsigned le_mcast_drops;		/* multicast drops */
    unsigned le_bcast_drops;		/* broadcast drops */
    union {
#if !defined(LE_NOLEMAC)
	struct le_lemac_info un_lemac;	/* LEMAC specific information */
#endif
#if !defined(LE_NOLANCE)
	struct le_lance_info un_lance;	/* Am7990 specific information */
#endif
    } le_un;
};
#define	le_if		le_ac.ac_if


static int le_probe(struct isa_device *dvp);
static int le_attach(struct isa_device *dvp);
static int le_ioctl(struct ifnet *ifp, int command, caddr_t data);
static void le_input(le_softc_t *sc, caddr_t seg1, size_t total_len,
		     size_t len2, caddr_t seg2);
static void le_multi_filter(le_softc_t *sc);
static void le_multi_op(le_softc_t *sc, const u_char *mca, int oper_flg);
static int le_read_macaddr(le_softc_t *sc, int ioreg, int skippat);

#define	LE_CRC32_POLY		0xEDB88320UL	/* CRC-32 Poly -- Little Endian */

struct le_board {
    int (*bd_probe)(le_softc_t *sc, const le_board_t *bd, int *msize);
};


static le_softc_t le_softc[NLE];

static const le_board_t le_boards[] = {
#if !defined(LE_NOLEMAC)
    { lemac_probe },			/* DE20[345] */
#endif
#if !defined(LE_NOLANCE)
    { depca_probe },			/* DE{20[012],422} */
#endif
    { NULL }				/* Must Be Last! */
};

/*
 * This tells the autoconf code how to set us up.
 */
struct isa_driver ledriver = {
    le_probe, le_attach, "le",
};

static unsigned le_intrs[NLE];

#define	LE_ADDREQUAL(a1, a2) \
	(((u_short *)a1)[0] == ((u_short *)a2)[0] \
	 || ((u_short *)a1)[1] == ((u_short *)a2)[1] \
	 || ((u_short *)a1)[2] == ((u_short *)a2)[2])
#define	LE_ADDRBRDCST(a1) \
	(((u_short *)a1)[0] == 0xFFFFU \
	 || ((u_short *)a1)[1] == 0xFFFFU \
	 || ((u_short *)a1)[2] == 0xFFFFU)

#define LE_INL(sc, reg) \
({ u_long data; \
        __asm __volatile("inl %1, %0": "=a" (data): "d" ((u_short)((sc)->le_iobase + (reg)))); \
        data; })


#define LE_OUTL(sc, reg, data) \
	({__asm __volatile("outl %0, %1"::"a" ((u_long)(data)), "d" ((u_short)((sc)->le_iobase + (reg))));})

#define LE_INW(sc, reg) \
({ u_short data; \
        __asm __volatile("inw %1, %0": "=a" (data): "d" ((u_short)((sc)->le_iobase + (reg)))); \
        data; })


#define LE_OUTW(sc, reg, data) \
	({__asm __volatile("outw %0, %1"::"a" ((u_short)(data)), "d" ((u_short)((sc)->le_iobase + (reg))));})

#define LE_INB(sc, reg) \
({ u_char data; \
        __asm __volatile("inb %1, %0": "=a" (data): "d" ((u_short)((sc)->le_iobase + (reg)))); \
        data; })


#define LE_OUTB(sc, reg, data) \
	({__asm __volatile("outb %0, %1"::"a" ((u_char)(data)), "d" ((u_short)((sc)->le_iobase + (reg))));})

#define	MEMCPY(to, from, len)		bcopy(from, to, len)
#define	MEMSET(where, what, howmuch)	bzero(where, howmuch)
#define	MEMCMP(l, r, len)		bcmp(l, r, len)


static int
le_probe(
    struct isa_device *dvp)
{
    le_softc_t *sc = &le_softc[dvp->id_unit];
    const le_board_t *bd;
    int iospace;

    if (dvp->id_unit >= NLE) {
	printf("%s%d not configured -- too many devices\n",
	       ledriver.name, dvp->id_unit);
	return 0;
    }

    sc->le_iobase = dvp->id_iobase;
    sc->le_membase = (u_char *) dvp->id_maddr;
    sc->le_irq = dvp->id_irq;
    sc->le_if.if_name = ledriver.name;
    sc->le_if.if_unit = dvp->id_unit;

    /*
     * Find and Initialize board..
     */

    sc->le_flags &= ~(IFF_UP|IFF_ALLMULTI);

    for (bd = le_boards; bd->bd_probe != NULL; bd++) {
	if ((iospace = (*bd->bd_probe)(sc, bd, &dvp->id_msize)) != 0) {
	    return iospace;
	}
    }

    return 0;
}

static int
le_attach(
    struct isa_device *dvp)
{
    le_softc_t *sc = &le_softc[dvp->id_unit];
    struct ifnet *ifp = &sc->le_if;

    ifp->if_softc = sc;
    ifp->if_mtu = ETHERMTU;
    printf("%s%d: %s ethernet address %6D\n",
	   ifp->if_name, ifp->if_unit,
	   sc->le_prodname,
	   sc->le_ac.ac_enaddr, ":");

    ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
    ifp->if_output = ether_output;
    ifp->if_ioctl = le_ioctl;
    ifp->if_type = IFT_ETHER;
    ifp->if_addrlen = 6;
    ifp->if_hdrlen = 14;

#if NBPFILTER > 0
    bpfattach(ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif

    if_attach(ifp);
    ether_ifattach(ifp);

    return 1;
}

void
le_intr(
    int unit)
{
    int s = splimp();

    le_intrs[unit]++;
    (*le_intrvec[unit])(&le_softc[unit]);

    splx(s);
}

#define	LE_XTRA		0

static void
le_input(
    le_softc_t *sc,
    caddr_t seg1,
    size_t total_len,
    size_t len1,
    caddr_t seg2)
{
    struct ether_header eh;
    struct mbuf *m;

    if (total_len - sizeof(eh) > ETHERMTU
	    || total_len - sizeof(eh) < ETHERMIN) {
	sc->le_if.if_ierrors++;
	return;
    }
    MEMCPY(&eh, seg1, sizeof(eh));

#if NBPFILTER > 0
    if (sc->le_if.if_bpf != NULL && seg2 == NULL) {
	bpf_tap(&sc->le_if, seg1, total_len);
	/*
	 * If this is single cast but not to us
	 * drop it!
	 */
	if ((eh.ether_dhost[0] & 1) == 0) {
	    if (!LE_ADDREQUAL(eh.ether_dhost, sc->le_ac.ac_enaddr)) {
		sc->le_scast_drops++;
		return;
	    }
	} else if ((sc->le_flags & IFF_MULTICAST) == 0) {
	    sc->le_mcast_drops++;
	    return;
	} else if (sc->le_flags & LE_BRDCSTONLY) {
	    if (!LE_ADDRBRDCST(eh.ether_dhost)) {
		sc->le_bcast_drops++;
		return;
	    }
	}
    }
#endif
    seg1 += sizeof(eh); total_len -= sizeof(eh); len1 -= sizeof(eh);

    MGETHDR(m, M_DONTWAIT, MT_DATA);
    if (m == NULL) {
	sc->le_if.if_ierrors++;
	return;
    }
    m->m_pkthdr.len = total_len;
    m->m_pkthdr.rcvif = &sc->le_if;
    if (total_len + LE_XTRA > MHLEN /* >= MINCLSIZE */) {
	MCLGET(m, M_DONTWAIT);
	if ((m->m_flags & M_EXT) == 0) {
	    m_free(m);
	    sc->le_if.if_ierrors++;
	    return;
	}
    } else if (total_len + LE_XTRA > MHLEN && MINCLSIZE == (MHLEN+MLEN)) {
	MGET(m->m_next, M_DONTWAIT, MT_DATA);
	if (m->m_next == NULL) {
	    m_free(m);
	    sc->le_if.if_ierrors++;
	    return;
	}
	m->m_next->m_len = total_len - MHLEN - LE_XTRA;
	len1 = total_len = MHLEN - LE_XTRA;
	MEMCPY(mtod(m->m_next, caddr_t), &seg1[MHLEN-LE_XTRA], m->m_next->m_len);
    } else if (total_len + LE_XTRA > MHLEN) {
	panic("le_input: pkt of unknown length");
    }
    m->m_data += LE_XTRA;
    m->m_len = total_len;
    MEMCPY(mtod(m, caddr_t), seg1, len1);
    if (seg2 != NULL)
	MEMCPY(mtod(m, caddr_t) + len1, seg2, total_len - len1);
#if NBPFILTER > 0
    if (sc->le_if.if_bpf != NULL && seg2 != NULL) {
	bpf_mtap(&sc->le_if, m);
	/*
	 * If this is single cast but not to us
	 * drop it!
	 */
	if ((eh.ether_dhost[0] & 1) == 0) {
	    if (!LE_ADDREQUAL(eh.ether_dhost, sc->le_ac.ac_enaddr)) {
		sc->le_scast_drops++;
		m_freem(m);
		return;
	    }
	} else if ((sc->le_flags & IFF_MULTICAST) == 0) {
	    sc->le_mcast_drops++;
	    m_freem(m);
	    return;
	} else if (sc->le_flags & LE_BRDCSTONLY) {
	    if (!LE_ADDRBRDCST(eh.ether_dhost)) {
		sc->le_bcast_drops++;
		m_freem(m);
		return;
	    }
	}
    }
#endif
    ether_input(&sc->le_if, &eh, m);
}

static int
le_ioctl(
    struct ifnet *ifp,
    int cmd,
    caddr_t data)
{
    le_softc_t *sc = ifp->if_softc;
    int s, error = 0;

    if ((sc->le_flags & IFF_UP) == 0)
	return EIO;

    s = splimp();

    switch (cmd) {
	case SIOCSIFADDR: {
	    struct ifaddr *ifa = (struct ifaddr *)data;

	    ifp->if_flags |= IFF_UP;
	    switch(ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET: {
		    (*sc->if_init)(ifp->if_unit);
		    arp_ifinit((struct arpcom *)ifp, ifa);
		    break;
		}
#endif /* INET */
#ifdef IPX
		/* This magic copied from if_is.c; I don't use XNS,
		 * so I have no way of telling if this actually
		 * works or not.
		 */
		case AF_IPX: {
		    struct ipx_addr *ina = &(IA_SIPX(ifa)->sipx_addr);
		    if (ipx_nullhost(*ina)) {
			ina->x_host = *(union ipx_host *)(sc->le_ac.ac_enaddr);
		    } else {
			ifp->if_flags &= ~IFF_RUNNING;
			bcopy((caddr_t)ina->x_host.c_host,
			      (caddr_t)sc->le_ac.ac_enaddr,
			      sizeof sc->le_ac.ac_enaddr);
		    }

		    (*sc->if_init)(ifp->if_unit);
		    break;
		}
#endif /* IPX */
#ifdef NS
		/* This magic copied from if_is.c; I don't use XNS,
		 * so I have no way of telling if this actually
		 * works or not.
		 */
		case AF_NS: {
		    struct ns_addr *ina = &(IA_SNS(ifa)->sns_addr);
		    if (ns_nullhost(*ina)) {
			ina->x_host = *(union ns_host *)(sc->le_ac.ac_enaddr);
		    } else {
			ifp->if_flags &= ~IFF_RUNNING;
			bcopy((caddr_t)ina->x_host.c_host,
			      (caddr_t)sc->le_ac.ac_enaddr,
			      sizeof sc->le_ac.ac_enaddr);
		    }

		    (*sc->if_init)(ifp->if_unit);
		    break;
		}
#endif /* NS */
		default: {
		    (*sc->if_init)(ifp->if_unit);
		    break;
		}
	    }
	    break;
	}

	case SIOCSIFFLAGS: {
	    (*sc->if_init)(ifp->if_unit);
	    break;
	}

	case SIOCADDMULTI:
	case SIOCDELMULTI:
	    /*
	     * Update multicast listeners
	     */
		(*sc->if_init)(ifp->if_unit);
		error = 0;
		break;

	default: {
	    error = EINVAL;
	}
    }

    splx(s);
    return error;
}

/*
 *  This is the standard method of reading the DEC Address ROMS.
 *  I don't understand it but it does work.
 */
static int
le_read_macaddr(
    le_softc_t *sc,
    int ioreg,
    int skippat)
{
    int cksum, rom_cksum;

    if (!skippat) {
	int idx, idx2, found, octet;
	static u_char testpat[] = { 0xFF, 0, 0x55, 0xAA, 0xFF, 0, 0x55, 0xAA };
	idx2 = found = 0;

	for (idx = 0; idx < 32; idx++) {
	    octet = LE_INB(sc, ioreg);

	    if (octet == testpat[idx2]) {
		if (++idx2 == sizeof testpat) {
		    ++found;
		    break;
		}
	    } else {
		idx2 = 0;
	    }
	}

	if (!found)
	    return -1;
    }

    cksum = 0;
    sc->le_hwaddr[0] = LE_INB(sc, ioreg);
    sc->le_hwaddr[1] = LE_INB(sc, ioreg);

    cksum = *(u_short *) &sc->le_hwaddr[0];

    sc->le_hwaddr[2] = LE_INB(sc, ioreg);
    sc->le_hwaddr[3] = LE_INB(sc, ioreg);
    cksum *= 2;
    if (cksum > 65535) cksum -= 65535;
    cksum += *(u_short *) &sc->le_hwaddr[2];
    if (cksum > 65535) cksum -= 65535;

    sc->le_hwaddr[4] = LE_INB(sc, ioreg);
    sc->le_hwaddr[5] = LE_INB(sc, ioreg);
    cksum *= 2;
    if (cksum > 65535) cksum -= 65535;
    cksum += *(u_short *) &sc->le_hwaddr[4];
    if (cksum >= 65535) cksum -= 65535;

    rom_cksum = LE_INB(sc, ioreg);
    rom_cksum |= LE_INB(sc, ioreg) << 8;

    if (cksum != rom_cksum)
	return -1;
    return 0;
}

static void
le_multi_filter(
    le_softc_t *sc)
{
    struct ifmultiaddr *ifma;

    MEMSET(sc->le_mctbl, 0, (sc->le_mcmask + 1) / 8);

    if (sc->le_if.if_flags & IFF_ALLMULTI) {
	sc->le_flags |= IFF_MULTICAST|IFF_ALLMULTI;
	return;
    }
    sc->le_flags &= ~IFF_MULTICAST;
    /* if (interface has had an address assigned) { */
	le_multi_op(sc, etherbroadcastaddr, TRUE);
	sc->le_flags |= LE_BRDCSTONLY|IFF_MULTICAST;
    /* } */

    sc->le_flags |= IFF_MULTICAST;

    for (ifma = sc->le_ac.ac_if.if_multiaddrs.lh_first; ifma;
	 ifma = ifma->ifma_link.le_next) {
	    if (ifma->ifma_addr->sa_family != AF_LINK)
		    continue;

	    le_multi_op(sc, LLADDR((struct sockaddr_dl *)ifma->ifma_addr), 1);
	    sc->le_flags &= ~LE_BRDCSTONLY;
    }
}

static void
le_multi_op(
    le_softc_t *sc,
    const u_char *mca,
    int enable)
{
    u_int idx, bit, data, crc = 0xFFFFFFFFUL;

#ifdef __alpha
    for (data = *(__unaligned u_long *) mca, bit = 0; bit < 48; bit++, data >>=
1)
        crc = (crc >> 1) ^ (((crc ^ data) & 1) ? LE_CRC32_POLY : 0);
#else
    for (idx = 0; idx < 6; idx++)
        for (data = *mca++, bit = 0; bit < 8; bit++, data >>= 1)
            crc = (crc >> 1) ^ (((crc ^ data) & 1) ? LE_CRC32_POLY : 0);
#endif
    /*
     * The following two line convert the N bit index into a longword index
     * and a longword mask.
     */
    crc &= sc->le_mcmask;
    bit = 1 << (crc & (LE_MC_NBPW -1));
    idx = crc >> (LE_MC_NBPW_LOG2);

    /*
     * Set or clear hash filter bit in our table.
     */
    if (enable) {
	sc->le_mctbl[idx] |= bit;		/* Set Bit */
    } else {
	sc->le_mctbl[idx] &= ~bit;		/* Clear Bit */
    }
}

#if !defined(LE_NOLEMAC)
/*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * Start of DEC EtherWORKS III (LEMAC) dependent code
 *
 */

#define	LEMAC_INTR_ENABLE(sc) \
	LE_OUTB(sc, LEMAC_REG_IC, LE_INB(sc, LEMAC_REG_IC) | LEMAC_IC_ALL)

#define	LEMAC_INTR_DISABLE(sc) \
	LE_OUTB(sc, LEMAC_REG_IC, LE_INB(sc, LEMAC_REG_IC) & ~LEMAC_IC_ALL)

#define LEMAC_64K_MODE(mbase)	(((mbase) >= 0x0A) && ((mbase) <= 0x0F))
#define LEMAC_32K_MODE(mbase)	(((mbase) >= 0x14) && ((mbase) <= 0x1F))
#define LEMAC_2K_MODE(mbase)	( (mbase) >= 0x40)

static void lemac_init(int unit);
static void lemac_start(struct ifnet *ifp);
static void lemac_reset(IF_RESET_ARGS);
static void lemac_intr(le_softc_t *sc);
static void lemac_rne_intr(le_softc_t *sc);
static void lemac_tne_intr(le_softc_t *sc);
static void lemac_txd_intr(le_softc_t *sc, unsigned cs_value);
static void lemac_rxd_intr(le_softc_t *sc, unsigned cs_value);
static int  lemac_read_eeprom(le_softc_t *sc);
static void lemac_init_adapmem(le_softc_t *sc);

#define	LE_MCBITS_ALL_1S	((le_mcbits_t)~(le_mcbits_t)0)

static const le_mcbits_t lemac_allmulti_mctbl[16] =  {
    LE_MCBITS_ALL_1S, LE_MCBITS_ALL_1S, LE_MCBITS_ALL_1S, LE_MCBITS_ALL_1S,
    LE_MCBITS_ALL_1S, LE_MCBITS_ALL_1S, LE_MCBITS_ALL_1S, LE_MCBITS_ALL_1S,
    LE_MCBITS_ALL_1S, LE_MCBITS_ALL_1S, LE_MCBITS_ALL_1S, LE_MCBITS_ALL_1S,
    LE_MCBITS_ALL_1S, LE_MCBITS_ALL_1S, LE_MCBITS_ALL_1S, LE_MCBITS_ALL_1S,
};
/*
 * An IRQ mapping table.  Less space than switch statement.
 */
static const int lemac_irqs[] = { IRQ5, IRQ10, IRQ11, IRQ15 };

/*
 * Some tuning/monitoring variables.
 */
static unsigned lemac_deftxmax = 16;	/* see lemac_max above */
static unsigned lemac_txnospc = 0;	/* total # of tranmit starvations */

static unsigned lemac_tne_intrs = 0;	/* total # of tranmit done intrs */
static unsigned lemac_rne_intrs = 0;	/* total # of receive done intrs */
static unsigned lemac_txd_intrs = 0;	/* total # of tranmit error intrs */
static unsigned lemac_rxd_intrs = 0;	/* total # of receive error intrs */


static int
lemac_probe(
    le_softc_t *sc,
    const le_board_t *bd,
    int *msize)
{
    int irq, portval;

    LE_OUTB(sc, LEMAC_REG_IOP, LEMAC_IOP_EEINIT);
    DELAY(LEMAC_EEP_DELAY);

    /*
     *  Read Ethernet address if card is present.
     */
    if (le_read_macaddr(sc, LEMAC_REG_APD, 0) < 0)
	return 0;

    MEMCPY(sc->le_ac.ac_enaddr, sc->le_hwaddr, 6);
    /*
     *  Clear interrupts and set IRQ.
     */

    portval = LE_INB(sc, LEMAC_REG_IC) & LEMAC_IC_IRQMSK;
    irq = lemac_irqs[portval >> 5];
    LE_OUTB(sc, LEMAC_REG_IC, portval);

    /*
     *  Make sure settings match.
     */

    if (irq != sc->le_irq) {
	printf("%s%d: lemac configuration error: expected IRQ 0x%x actual 0x%x\n",
	       sc->le_if.if_name, sc->le_if.if_unit, sc->le_irq, irq);
	return 0;
    }

    /*
     * Try to reset the unit
     */
    sc->if_init = lemac_init;
    sc->le_if.if_start = lemac_start;
    sc->if_reset = lemac_reset;
    sc->lemac_memmode = 2;
    LE_RESET(sc);
    if ((sc->le_flags & IFF_UP) == 0)
	return 0;

    /*
     *  Check for correct memory base configuration.
     */
    if (vtophys(sc->le_membase) != sc->lemac_membase) {
	printf("%s%d: lemac configuration error: expected iomem 0x%x actual 0x%x\n",
	       sc->le_if.if_name, sc->le_if.if_unit,
	       vtophys(sc->le_membase), sc->lemac_membase);
	return 0;
    }

    sc->le_prodname = sc->lemac_prodname;
    sc->le_mctbl = sc->lemac_mctbl;
    sc->le_mcmask = (1 << LEMAC_MCTBL_BITS) - 1;
    sc->lemac_txmax = lemac_deftxmax;
    *msize = 2048;
    le_intrvec[sc->le_if.if_unit] = lemac_intr;

    return LEMAC_IOSPACE;
}

/*
 * Do a hard reset of the board;
 */
static void
lemac_reset(
    IF_RESET_ARGS)
{
    le_softc_t *sc = &le_softc[unit];
    int portval, cksum;

    /*
     * Initialize board..
     */

    sc->le_flags &= IFF_UP;
    sc->le_if.if_flags &= ~IFF_OACTIVE;
    LEMAC_INTR_DISABLE(sc);

    LE_OUTB(sc, LEMAC_REG_IOP, LEMAC_IOP_EEINIT);
    DELAY(LEMAC_EEP_DELAY);

    /* Disable Interrupts */
    /* LE_OUTB(sc, LEMAC_REG_IC, LE_INB(sc, LEMAC_REG_IC) & ICR_IRQ_SEL); */

    /*
     * Read EEPROM information.  NOTE - the placement of this function
     * is important because functions hereafter may rely on information
     * read from the EEPROM.
     */
    if ((cksum = lemac_read_eeprom(sc)) != LEMAC_EEP_CKSUM) {
	printf("%s%d: reset: EEPROM checksum failed (0x%x)\n",
	       sc->le_if.if_name, sc->le_if.if_unit, cksum);
	return;
    }

    /*
     *  Force to 2K mode if not already configured.
     */

    portval = LE_INB(sc, LEMAC_REG_MBR);
    if (!LEMAC_2K_MODE(portval)) {
	if (LEMAC_64K_MODE(portval)) {
	    portval = (((portval * 2) & 0xF) << 4);
	    sc->lemac_memmode = 64;
	} else if (LEMAC_32K_MODE(portval)) {
	    portval = ((portval & 0xF) << 4);
	    sc->lemac_memmode = 32;
	}
	LE_OUTB(sc, LEMAC_REG_MBR, portval);
    }
    sc->lemac_membase = portval * (2 * 1024) + (512 * 1024);

    /*
     *  Initialize Free Memory Queue, Init mcast table with broadcast.
     */

    lemac_init_adapmem(sc);
    sc->le_flags |= IFF_UP;
    return;
}

static void
lemac_init(
    int unit)
{
    le_softc_t *sc = &le_softc[unit];
    int s;

    if ((sc->le_flags & IFF_UP) == 0)
	return;

    s = splimp();

    /*
     * If the interface has the up flag
     */
    if (sc->le_if.if_flags & IFF_UP) {
	int saved_cs = LE_INB(sc, LEMAC_REG_CS);
	LE_OUTB(sc, LEMAC_REG_CS, saved_cs | (LEMAC_CS_TXD | LEMAC_CS_RXD));
	LE_OUTB(sc, LEMAC_REG_PA0, sc->le_ac.ac_enaddr[0]);
	LE_OUTB(sc, LEMAC_REG_PA1, sc->le_ac.ac_enaddr[1]);
	LE_OUTB(sc, LEMAC_REG_PA2, sc->le_ac.ac_enaddr[2]);
	LE_OUTB(sc, LEMAC_REG_PA3, sc->le_ac.ac_enaddr[3]);
	LE_OUTB(sc, LEMAC_REG_PA4, sc->le_ac.ac_enaddr[4]);
	LE_OUTB(sc, LEMAC_REG_PA5, sc->le_ac.ac_enaddr[5]);

	LE_OUTB(sc, LEMAC_REG_IC, LE_INB(sc, LEMAC_REG_IC) | LEMAC_IC_IE);

	if (sc->le_if.if_flags & IFF_PROMISC) {
	    LE_OUTB(sc, LEMAC_REG_CS, LEMAC_CS_MCE | LEMAC_CS_PME);
	} else {
	    LEMAC_INTR_DISABLE(sc);
	    le_multi_filter(sc);
	    LE_OUTB(sc, LEMAC_REG_MPN, 0);
	    if ((sc->le_flags | sc->le_if.if_flags) & IFF_ALLMULTI) {
		MEMCPY(&sc->le_membase[LEMAC_MCTBL_OFF], lemac_allmulti_mctbl, sizeof(lemac_allmulti_mctbl));
	    } else {
		MEMCPY(&sc->le_membase[LEMAC_MCTBL_OFF], sc->lemac_mctbl, sizeof(sc->lemac_mctbl));
	    }
	    LE_OUTB(sc, LEMAC_REG_CS, LEMAC_CS_MCE);
	}

	LE_OUTB(sc, LEMAC_REG_CTL, LE_INB(sc, LEMAC_REG_CTL) ^ LEMAC_CTL_LED);

	LEMAC_INTR_ENABLE(sc);
	sc->le_if.if_flags |= IFF_RUNNING;
    } else {
	LE_OUTB(sc, LEMAC_REG_CS, LEMAC_CS_RXD|LEMAC_CS_TXD);

	LEMAC_INTR_DISABLE(sc);
	sc->le_if.if_flags &= ~IFF_RUNNING;
    }
    splx(s);
}

/*
 * What to do upon receipt of an interrupt.
 */
static void
lemac_intr(
    le_softc_t *sc)
{
    int cs_value;

    LEMAC_INTR_DISABLE(sc);	/* Mask interrupts */

    /*
     * Determine cause of interrupt.  Receive events take
     * priority over Transmit.
     */

    cs_value = LE_INB(sc, LEMAC_REG_CS);

    /*
     * Check for Receive Queue not being empty.
     * Check for Transmit Done Queue not being empty.
     */

    if (cs_value & LEMAC_CS_RNE)
	lemac_rne_intr(sc);
    if (cs_value & LEMAC_CS_TNE)
	lemac_tne_intr(sc);

    /*
     * Check for Transmitter Disabled.
     * Check for Receiver Disabled.
     */

    if (cs_value & LEMAC_CS_TXD)
	lemac_txd_intr(sc, cs_value);
    if (cs_value & LEMAC_CS_RXD)
	lemac_rxd_intr(sc, cs_value);

    /*
     * Toggle LED and unmask interrupts.
     */

    LE_OUTB(sc, LEMAC_REG_CTL, LE_INB(sc, LEMAC_REG_CTL) ^ LEMAC_CTL_LED);
    LEMAC_INTR_ENABLE(sc);		/* Unmask interrupts */
}

static void
lemac_rne_intr(
    le_softc_t *sc)
{
    int rxcount, rxlen, rxpg;
    u_char *rxptr;

    lemac_rne_intrs++;
    rxcount = LE_INB(sc, LEMAC_REG_RQC);
    while (rxcount--) {
	rxpg = LE_INB(sc, LEMAC_REG_RQ);
	LE_OUTB(sc, LEMAC_REG_MPN, rxpg);

	rxptr = sc->le_membase;
	sc->le_if.if_ipackets++;
	if (*rxptr & LEMAC_RX_OK) {

	    /*
	     * Get receive length - subtract out checksum.
	     */

	    rxlen = ((*(u_int *)rxptr >> 8) & 0x7FF) - 4;
	    le_input(sc, rxptr + sizeof(u_int), rxlen, rxlen, NULL);
	} else { /* end if (*rxptr & LEMAC_RX_OK) */
	    sc->le_if.if_ierrors++;
	}
	LE_OUTB(sc, LEMAC_REG_FMQ, rxpg);  /* Return this page to Free Memory Queue */
    }  /* end while (recv_count--) */

    return;
}

static void
lemac_rxd_intr(
    le_softc_t *sc,
    unsigned cs_value)
{
    /*
     * Handle CS_RXD (Receiver disabled) here.
     *
     * Check Free Memory Queue Count. If not equal to zero
     * then just turn Receiver back on. If it is equal to
     * zero then check to see if transmitter is disabled.
     * Process transmit TXD loop once more.  If all else
     * fails then do software init (0xC0 to EEPROM Init)
     * and rebuild Free Memory Queue.
     */

    lemac_rxd_intrs++;

    /*
     *  Re-enable Receiver.
     */

    cs_value &= ~LEMAC_CS_RXD;
    LE_OUTB(sc, LEMAC_REG_CS, cs_value);

    if (LE_INB(sc, LEMAC_REG_FMC) > 0)
	return;

    if (cs_value & LEMAC_CS_TXD)
	lemac_txd_intr(sc, cs_value);

    if ((LE_INB(sc, LEMAC_REG_CS) & LEMAC_CS_RXD) == 0)
	return;

    printf("%s%d: fatal RXD error, attempting recovery\n",
	   sc->le_if.if_name, sc->le_if.if_unit);

    LE_RESET(sc);
    if (sc->le_flags & IFF_UP) {
	lemac_init(sc->le_if.if_unit);
	return;
    }

    /*
     *  Error during initializion.  Mark card as disabled.
     */
    printf("%s%d: recovery failed -- board disabled\n",
	   sc->le_if.if_name, sc->le_if.if_unit);
    return;
}

static void
lemac_start(
    struct ifnet *ifp)
{
    le_softc_t *sc = (le_softc_t *) ifp;
    struct ifqueue *ifq = &ifp->if_snd;

    if ((ifp->if_flags & IFF_RUNNING) == 0)
	return;

    LEMAC_INTR_DISABLE(sc);

    while (ifq->ifq_head != NULL) {
	struct mbuf  *m;
	int tx_pg;
	u_int txhdr, txoff;

	if (LE_INB(sc, LEMAC_REG_TQC) >= sc->lemac_txmax) {
	    ifp->if_flags |= IFF_OACTIVE;
	    break;
	}

	tx_pg = LE_INB(sc, LEMAC_REG_FMQ);	/* get free memory page */
	/*
	 * Check for good transmit page.
	 */
	if (tx_pg == 0 || tx_pg > sc->lemac_lastpage) {
	    lemac_txnospc++;
	    ifp->if_flags |= IFF_OACTIVE;
	    break;
	}

	IF_DEQUEUE(ifq, m);
	LE_OUTB(sc, LEMAC_REG_MPN, tx_pg);	/* Shift 2K window. */

	/*
	 * The first four bytes of each transmit buffer are for
	 * control information.  The first byte is the control
	 * byte, then the length (why not word aligned??), then
	 * the off to the buffer.
	 */

	txoff = (mtod(m, u_int) & (sizeof(u_long) - 1)) + LEMAC_TX_HDRSZ;
	txhdr = sc->lemac_txctl | (m->m_pkthdr.len << 8) | (txoff << 24);
	*(u_int *) sc->le_membase = txhdr;

	/*
	 * Copy the packet to the board
	 */

	m_copydata(m, 0, m->m_pkthdr.len, sc->le_membase + txoff);

	LE_OUTB(sc, LEMAC_REG_TQ, tx_pg);	/* tell chip to transmit this packet */

#if NBPFILTER > 0
	if (sc->le_if.if_bpf)
		bpf_mtap(&sc->le_if, m);
#endif

	m_freem(m);			/* free the mbuf */
    }
    LEMAC_INTR_ENABLE(sc);
}

static void
lemac_tne_intr(
    le_softc_t *sc)
{
    int txsts, txcount = LE_INB(sc, LEMAC_REG_TDC);

    lemac_tne_intrs++;
    while (txcount--) {
	txsts = LE_INB(sc, LEMAC_REG_TDQ);
	sc->le_if.if_opackets++;		/* another one done */
	if ((txsts & LEMAC_TDQ_COL) != LEMAC_TDQ_NOCOL)
	    sc->le_if.if_collisions++;
    }
    sc->le_if.if_flags &= ~IFF_OACTIVE;
    lemac_start(&sc->le_if);
}

static void
lemac_txd_intr(
    le_softc_t *sc,
    unsigned cs_value)
{
    /*
     * Read transmit status, remove transmit buffer from
     * transmit queue and place on free memory queue,
     * then reset transmitter.
     * Increment appropriate counters.
     */

    lemac_txd_intrs++;
    sc->le_if.if_oerrors++;
    if (LE_INB(sc, LEMAC_REG_TS) & LEMAC_TS_ECL)
	sc->le_if.if_collisions++;
    sc->le_if.if_flags &= ~IFF_OACTIVE;

    LE_OUTB(sc, LEMAC_REG_FMQ, LE_INB(sc, LEMAC_REG_TQ));
				/* Get Page number and write it back out */

    LE_OUTB(sc, LEMAC_REG_CS, cs_value & ~LEMAC_CS_TXD);
				/* Turn back on transmitter */
    return;
}

static int
lemac_read_eeprom(
    le_softc_t *sc)
{
    int	word_off, cksum;

    u_char *ep;

    cksum = 0;
    ep = sc->lemac_eeprom;
    for (word_off = 0; word_off < LEMAC_EEP_SIZE / 2; word_off++) {
	LE_OUTB(sc, LEMAC_REG_PI1, word_off);
	LE_OUTB(sc, LEMAC_REG_IOP, LEMAC_IOP_EEREAD);

	DELAY(LEMAC_EEP_DELAY);

	*ep = LE_INB(sc, LEMAC_REG_EE1);	cksum += *ep++;
	*ep = LE_INB(sc, LEMAC_REG_EE2);	cksum += *ep++;
    }

    /*
     *  Set up Transmit Control Byte for use later during transmit.
     */

    sc->lemac_txctl |= LEMAC_TX_FLAGS;

    if ((sc->lemac_eeprom[LEMAC_EEP_SWFLAGS] & LEMAC_EEP_SW_SQE) == 0)
	sc->lemac_txctl &= ~LEMAC_TX_SQE;

    if (sc->lemac_eeprom[LEMAC_EEP_SWFLAGS] & LEMAC_EEP_SW_LAB)
	sc->lemac_txctl |= LEMAC_TX_LAB;

    MEMCPY(sc->lemac_prodname, &sc->lemac_eeprom[LEMAC_EEP_PRDNM], LEMAC_EEP_PRDNMSZ);
    sc->lemac_prodname[LEMAC_EEP_PRDNMSZ] = '\0';

    return cksum % 256;
}

static void
lemac_init_adapmem(
    le_softc_t *sc)
{
    int pg, conf;

    conf = LE_INB(sc, LEMAC_REG_CNF);

    if ((sc->lemac_eeprom[LEMAC_EEP_SETUP] & LEMAC_EEP_ST_DRAM) == 0) {
	sc->lemac_lastpage = 63;
	conf &= ~LEMAC_CNF_DRAM;
    } else {
	sc->lemac_lastpage = 127;
	conf |= LEMAC_CNF_DRAM;
    }

    LE_OUTB(sc, LEMAC_REG_CNF, conf);

    for (pg = 1; pg <= sc->lemac_lastpage; pg++)
	LE_OUTB(sc, LEMAC_REG_FMQ, pg);

    return;
}
#endif /* !defined(LE_NOLEMAC) */

#if !defined(LE_NOLANCE)
/*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * Start of DEPCA (DE200/DE201/DE202/DE422 etal) support.
 *
 */
static void depca_intr(le_softc_t *sc);
static int  lance_init_adapmem(le_softc_t *sc);
static int  lance_init_ring(le_softc_t *sc, ln_ring_t *rp, lance_ring_t *ri,
			    unsigned ndescs, unsigned bufoffset,
			    unsigned descoffset);
static void lance_init(int unit);
static void lance_reset(IF_RESET_ARGS);
static void lance_intr(le_softc_t *sc);
static int  lance_rx_intr(le_softc_t *sc);
static void lance_start(struct ifnet *ifp);
static int  lance_tx_intr(le_softc_t *sc);

#define	LN_BUFSIZE		/* 380 */ 304	/* 1520 / 4 */
#define	LN_TXDESC_RATIO		2048
#define	LN_DESC_MAX		128

#if LN_DOSTATS
static struct {
    unsigned lance_rx_misses;
    unsigned lance_rx_badcrc;
    unsigned lance_rx_badalign;
    unsigned lance_rx_badframe;
    unsigned lance_rx_buferror;
    unsigned lance_tx_deferred;
    unsigned lance_tx_single_collisions;
    unsigned lance_tx_multiple_collisions;
    unsigned lance_tx_excessive_collisions;
    unsigned lance_tx_late_collisions;

    unsigned lance_memory_errors;
    unsigned lance_inits;
    unsigned lance_tx_intrs;
    unsigned lance_tx_nospc[2];
    unsigned lance_tx_drains[2];
    unsigned lance_tx_orphaned;
    unsigned lance_tx_adoptions;
    unsigned lance_tx_emptied;
    unsigned lance_tx_deftxint;
    unsigned lance_tx_buferror;
    unsigned lance_high_txoutptr;
    unsigned lance_low_txheapsize;
    unsigned lance_low_txfree;
    unsigned lance_tx_intr_hidescs;
    /* unsigned lance_tx_intr_descs[LN_DESC_MAX]; */

    unsigned lance_rx_intrs;
    unsigned lance_rx_badsop;
    unsigned lance_rx_contig;
    unsigned lance_rx_noncontig;
    unsigned lance_rx_intr_hidescs;
    unsigned lance_rx_ndescs[4096 / LN_BUFSIZE];
    /* unsigned lance_rx_intr_descs[LN_DESC_MAX]; */
} lance_stats;

#define	LN_STAT(stat)	(lance_stats.lance_ ## stat)
#define	LN_MINSTAT(stat, val)	(LN_STAT(stat > (val)) ? LN_STAT(stat = (val)) : 0)
#define	LN_MAXSTAT(stat, val)	(LN_STAT(stat < (val)) ? LN_STAT(stat = (val)) : 0)

#else
#define	LN_STAT(stat)	0
#define	LN_MINSTAT(stat, val)	0
#define	LN_MAXSTAT(stat, val)	0
#endif

#define	LN_SELCSR(sc, csrno)		(LE_OUTW(sc, sc->lance_rap, csrno))
#define	LN_INQCSR(sc)			(LE_INW(sc, sc->lance_rap))

#define	LN_WRCSR(sc, val)		(LE_OUTW(sc, sc->lance_rdp, val))
#define	LN_RDCSR(sc)			(LE_INW(sc, sc->lance_rdp))


#define	LN_ZERO(sc, vaddr, len)		bzero(vaddr, len)
#define	LN_COPYTO(sc, from, to, len)	bcopy(from, to, len)

#define	LN_SETFLAG(sc, vaddr, val) \
	(((volatile u_char *) vaddr)[3] = (val))

#define	LN_PUTDESC(sc, desc, vaddr) \
	(((volatile u_short *) vaddr)[0] = ((u_short *) desc)[0], \
	 ((volatile u_short *) vaddr)[2] = ((u_short *) desc)[2], \
	 ((volatile u_short *) vaddr)[1] = ((u_short *) desc)[1])

/*
 * Only get the descriptor flags and length/status.  All else
 * read-only.
 */
#define	LN_GETDESC(sc, desc, vaddr) \
	(((u_short *) desc)[1] = ((volatile u_short *) vaddr)[1], \
	 ((u_short *) desc)[3] = ((volatile u_short *) vaddr)[3])


/*
 *  These definitions are specific to the DEC "DEPCA-style" NICs.
 *	(DEPCA, DE10x, DE20[012], DE422)
 *
 */
#define	DEPCA_REG_NICSR		0		/* (RW;16) NI Control / Status */
#define	DEPCA_REG_RDP		4		/* (RW:16) LANCE RDP (data) register */
#define	DEPCA_REG_RAP		6		/* (RW:16) LANCE RAP (address) register */
#define	DEPCA_REG_ADDRROM	12		/* (R : 8) DEPCA Ethernet Address ROM */
#define	DEPCA_IOSPACE		16		/* DEPCAs use 16 bytes of IO space */

#define	DEPCA_NICSR_LED		0x0001		/* Light the LED on the back of the DEPCA */
#define	DEPCA_NICSR_ENABINTR	0x0002		/* Enable Interrupts */
#define	DEPCA_NICSR_MASKINTR	0x0004		/* Mask Interrupts */
#define	DEPCA_NICSR_AAC		0x0008		/* Address Counter Clear */
#define	DEPCA_NICSR_REMOTEBOOT	0x0010		/* Remote Boot Enabled (ignored) */
#define	DEPCA_NICSR_32KRAM	0x0020		/* DEPCA LANCE RAM size 64K (C) / 32K (S) */
#define	DEPCA_NICSR_LOW32K	0x0040		/* Bank Select (A15 = !This Bit) */
#define	DEPCA_NICSR_SHE		0x0080		/* Shared RAM Enabled (ie hide ROM) */
#define	DEPCA_NICSR_BOOTTMO	0x0100		/* Remote Boot Timeout (ignored) */

#define	DEPCA_RDNICSR(sc)	(LE_INW(sc, DEPCA_REG_NICSR))
#define	DEPCA_WRNICSR(sc, val)	(LE_OUTW(sc, DEPCA_REG_NICSR, val))

#define	DEPCA_IDSTR_OFFSET	0xC006		/* ID String Offset */

#define DEPCA_REG_EISAID	0x80
#define	DEPCA_EISAID_MASK	0xf0ffffff
#define	DEPCA_EISAID_DE422	0x2042A310

typedef enum {
    DEPCA_CLASSIC,
    DEPCA_DE100, DEPCA_DE101,
    DEPCA_EE100,
    DEPCA_DE200, DEPCA_DE201, DEPCA_DE202,
    DEPCA_DE422,
    DEPCA_UNKNOWN
} depca_t;

static const char *depca_signatures[] = {
    "DEPCA",
    "DE100", "DE101",
    "EE100",
    "DE200", "DE201", "DE202",
    "DE422",
    NULL
};

static int
depca_probe(
    le_softc_t *sc,
    const le_board_t *bd,
    int *msize)
{
    unsigned nicsr, idx, idstr_offset = DEPCA_IDSTR_OFFSET;

    /*
     *  Find out how memory we are dealing with.  Adjust
     *  the ID string offset approriately if we are at
     *  32K.  Make sure the ROM is enabled.
     */
    nicsr = DEPCA_RDNICSR(sc);
    nicsr &= ~(DEPCA_NICSR_SHE|DEPCA_NICSR_LED|DEPCA_NICSR_ENABINTR);

    if (nicsr & DEPCA_NICSR_32KRAM) {
	/*
	 * Make we are going to read the upper
	 * 32K so we do read the ROM.
	 */
	sc->lance_ramsize = 32 * 1024;
	nicsr &= ~DEPCA_NICSR_LOW32K;
	sc->lance_ramoffset = 32 * 1024;
	idstr_offset -= sc->lance_ramsize;
    } else {
	sc->lance_ramsize = 64 * 1024;
	sc->lance_ramoffset = 0;
    }
    DEPCA_WRNICSR(sc, nicsr);

    sc->le_prodname = NULL;
    for (idx = 0; depca_signatures[idx] != NULL; idx++) {
	if (bcmp(depca_signatures[idx], sc->le_membase + idstr_offset, 5) == 0) {
	    sc->le_prodname = depca_signatures[idx];
	    break;
	}
    }

    if (sc->le_prodname == NULL) {
	/*
	 * Try to get the EISA device if it's a DE422.
	 */
	if (sc->le_iobase > 0x1000 && (sc->le_iobase & 0x0F00) == 0x0C00
	    && (LE_INL(sc, DEPCA_REG_EISAID) & DEPCA_EISAID_MASK)
	     == DEPCA_EISAID_DE422) {
	    sc->le_prodname = "DE422";
	} else {
	    return 0;
	}
    }
    if (idx == DEPCA_CLASSIC)
	sc->lance_ramsize -= 16384;	/* Can't use the ROM area on a DEPCA */

    /*
     * Try to read the address ROM.
     *   Stop the LANCE, reset the Address ROM Counter (AAC),
     *	 read the NICSR to "clock" in the reset, and then
     *	 re-enable the Address ROM Counter.  Now read the
     *   address ROM.
     */
    sc->lance_rdp = DEPCA_REG_RDP;
    sc->lance_rap = DEPCA_REG_RAP;
    sc->lance_csr3 = LN_CSR3_ALE;
    sc->le_mctbl = sc->lance_initb.ln_multi_mask;
    sc->le_mcmask = LN_MC_MASK;
    LN_SELCSR(sc, LN_CSR0);
    LN_WRCSR(sc, LN_CSR0_STOP);

    if (idx < DEPCA_DE200) {
	DEPCA_WRNICSR(sc, DEPCA_RDNICSR(sc) & ~DEPCA_NICSR_AAC);
	DEPCA_WRNICSR(sc, DEPCA_RDNICSR(sc) | DEPCA_NICSR_AAC);
    }

    if (le_read_macaddr(sc, DEPCA_REG_ADDRROM, idx == DEPCA_CLASSIC) < 0)
	return 0;

    MEMCPY(sc->le_ac.ac_enaddr, sc->le_hwaddr, 6);
    /*
     * Renable shared RAM.
     */
    DEPCA_WRNICSR(sc, DEPCA_RDNICSR(sc) | DEPCA_NICSR_SHE);

    le_intrvec[sc->le_if.if_unit] = depca_intr;
    if (!lance_init_adapmem(sc))
	return 0;

    sc->if_reset = lance_reset;
    sc->if_init = lance_init;
    sc->le_if.if_start = lance_start;
    DEPCA_WRNICSR(sc, DEPCA_NICSR_SHE | DEPCA_NICSR_ENABINTR);
    LE_RESET(sc);

    LN_STAT(low_txfree = sc->lance_txinfo.ri_max);
    LN_STAT(low_txheapsize = 0xFFFFFFFF);
    *msize = sc->lance_ramsize;
    return DEPCA_IOSPACE;
}

static void
depca_intr(
    le_softc_t *sc)
{
    DEPCA_WRNICSR(sc, DEPCA_RDNICSR(sc) ^ DEPCA_NICSR_LED);
    lance_intr(sc);
}

/*
 * Here's as good a place to describe our paritioning of the
 * LANCE shared RAM space.  (NOTE: this driver does not yet support
 * the concept of a LANCE being able to DMA).
 *
 * First is the 24 (00:23) bytes for LANCE Initialization Block
 * Next are the recieve descriptors.  The number is calculated from
 * how many LN_BUFSIZE buffers we can allocate (this number must
 * be a power of 2).  Next are the transmit descriptors.  The amount
 * of transmit descriptors is derived from the size of the RAM
 * divided by 1K.  Now come the receive buffers (one for each receive
 * descriptor).  Finally is the transmit heap.  (no fixed buffers are
 * allocated so as to make the most use of the limited space).
 */
static int
lance_init_adapmem(
    le_softc_t *sc)
{
    lance_addr_t rxbufoffset;
    lance_addr_t rxdescoffset, txdescoffset;
    unsigned rxdescs, txdescs;

    /*
     * First calculate how many descriptors we heap.
     * Note this assumes the ramsize is a power of two.
     */
    sc->lance_rxbufsize = LN_BUFSIZE;
    rxdescs = 1;
    while (rxdescs * sc->lance_rxbufsize < sc->lance_ramsize)
	rxdescs *= 2;
    rxdescs /= 2;
    if (rxdescs > LN_DESC_MAX) {
	sc->lance_rxbufsize *= rxdescs / LN_DESC_MAX;
	rxdescs = LN_DESC_MAX;
    }
    txdescs = sc->lance_ramsize / LN_TXDESC_RATIO;
    if (txdescs > LN_DESC_MAX)
	txdescs = LN_DESC_MAX;

    /*
     * Now calculate where everything goes in memory
     */
    rxdescoffset = sizeof(ln_initb_t);
    txdescoffset = rxdescoffset + sizeof(ln_desc_t) * rxdescs;
    rxbufoffset  = txdescoffset + sizeof(ln_desc_t) * txdescs;

    sc->le_mctbl = (le_mcbits_t *) sc->lance_initb.ln_multi_mask;
    /*
     * Remember these for debugging.
     */
    sc->lance_raminitb = (ln_initb_t *) sc->le_membase;
    sc->lance_ramdesc = (ln_desc_t *) (sc->le_membase + rxdescoffset);

    /*
     * Initialize the rings.
     */
    if (!lance_init_ring(sc, &sc->lance_initb.ln_rxring, &sc->lance_rxinfo,
		   rxdescs, rxbufoffset, rxdescoffset))
	return 0;
    sc->lance_rxinfo.ri_heap = rxbufoffset;
    sc->lance_rxinfo.ri_heapend = rxbufoffset + sc->lance_rxbufsize * rxdescs;

    if (!lance_init_ring(sc, &sc->lance_initb.ln_txring, &sc->lance_txinfo,
		   txdescs, 0, txdescoffset))
	return 0;
    sc->lance_txinfo.ri_heap = sc->lance_rxinfo.ri_heapend;
    sc->lance_txinfo.ri_heapend = sc->lance_ramsize;

    /*
     * Set CSR1 and CSR2 to the address of the init block (which
     * for us is always 0.
     */
    sc->lance_csr1 = LN_ADDR_LO(0 + sc->lance_ramoffset);
    sc->lance_csr2 = LN_ADDR_HI(0 + sc->lance_ramoffset);
    return 1;
}

static int
lance_init_ring(
    le_softc_t *sc,
    ln_ring_t *rp,
    lance_ring_t *ri,
    unsigned ndescs,
    lance_addr_t bufoffset,
    lance_addr_t descoffset)
{
    lance_descinfo_t *di;

    /*
     * Initialize the ring pointer in the LANCE InitBlock
     */
    rp->r_addr_lo = LN_ADDR_LO(descoffset + sc->lance_ramoffset);
    rp->r_addr_hi = LN_ADDR_HI(descoffset + sc->lance_ramoffset);
    rp->r_log2_size = ffs(ndescs) - 1;

    /*
     * Allocate the ring entry descriptors and initialize
     * our ring information data structure.  All these are
     * our copies and do not live in the LANCE RAM.
     */
    ri->ri_first = (lance_descinfo_t *) malloc(ndescs * sizeof(*di), M_DEVBUF, M_NOWAIT);
    if (ri->ri_first == NULL) {
	printf("lance_init_ring: malloc(%d) failed\n", ndescs * sizeof(*di));
	return 0;
    }
    ri->ri_free = ri->ri_max = ndescs;
    ri->ri_last = ri->ri_first + ri->ri_max;
    for (di = ri->ri_first; di < ri->ri_last; di++) {
	di->di_addr = sc->le_membase + descoffset;
	di->di_mbuf = NULL;
	if (bufoffset) {
	    di->di_bufaddr = bufoffset;
	    di->di_buflen = sc->lance_rxbufsize;
	    bufoffset += sc->lance_rxbufsize;
	}
	descoffset += sizeof(ln_desc_t);
    }
    return 1;
}

static void
lance_dumpcsrs(
    le_softc_t *sc,
    const char *id)
{
    printf("%s%d: %s: nicsr=%04x",
	   sc->le_if.if_name, sc->le_if.if_unit,
	   id, DEPCA_RDNICSR(sc));
    LN_SELCSR(sc, LN_CSR0); printf(" csr0=%04x", LN_RDCSR(sc));
    LN_SELCSR(sc, LN_CSR1); printf(" csr1=%04x", LN_RDCSR(sc));
    LN_SELCSR(sc, LN_CSR2); printf(" csr2=%04x", LN_RDCSR(sc));
    LN_SELCSR(sc, LN_CSR3); printf(" csr3=%04x\n", LN_RDCSR(sc));
    LN_SELCSR(sc, LN_CSR0);
}

static void
lance_reset(
    IF_RESET_ARGS)
{
    le_softc_t *sc = &le_softc[unit];
    register int cnt, csr;

    /* lance_dumpcsrs(sc, "lance_reset: start"); */

    LN_WRCSR(sc, LN_RDCSR(sc) & ~LN_CSR0_ENABINTR);
    LN_WRCSR(sc, LN_CSR0_STOP);
    DELAY(100);

    sc->le_flags &= ~IFF_UP;
    sc->le_if.if_flags &= ~(IFF_UP|IFF_RUNNING);

    le_multi_filter(sc);		/* initialize the multicast table */
    if ((sc->le_flags | sc->le_if.if_flags) & IFF_ALLMULTI) {
	sc->lance_initb.ln_multi_mask[0] = 0xFFFFU;
	sc->lance_initb.ln_multi_mask[1] = 0xFFFFU;
	sc->lance_initb.ln_multi_mask[2] = 0xFFFFU;
	sc->lance_initb.ln_multi_mask[3] = 0xFFFFU;
    }
    sc->lance_initb.ln_physaddr[0] = ((u_short *) sc->le_ac.ac_enaddr)[0];
    sc->lance_initb.ln_physaddr[1] = ((u_short *) sc->le_ac.ac_enaddr)[1];
    sc->lance_initb.ln_physaddr[2] = ((u_short *) sc->le_ac.ac_enaddr)[2];
    if (sc->le_if.if_flags & IFF_PROMISC) {
	sc->lance_initb.ln_mode |= LN_MODE_PROMISC;
    } else {
	sc->lance_initb.ln_mode &= ~LN_MODE_PROMISC;
    }
    /*
     * We force the init block to be at the start
     * of the LANCE's RAM buffer.
     */
    LN_COPYTO(sc, &sc->lance_initb, sc->le_membase, sizeof(sc->lance_initb));
    LN_SELCSR(sc, LN_CSR1); LN_WRCSR(sc, sc->lance_csr1);
    LN_SELCSR(sc, LN_CSR2); LN_WRCSR(sc, sc->lance_csr2);
    LN_SELCSR(sc, LN_CSR3); LN_WRCSR(sc, sc->lance_csr3);

    /* lance_dumpcsrs(sc, "lance_reset: preinit"); */

    /*
     * clear INITDONE and INIT the chip
     */
    LN_SELCSR(sc, LN_CSR0);
    LN_WRCSR(sc, LN_CSR0_INIT|LN_CSR0_INITDONE);

    csr = 0;
    cnt = 100;
    while (cnt-- > 0) {
        if (((csr = LN_RDCSR(sc)) & LN_CSR0_INITDONE) != 0)
            break;
        DELAY(10000);
    }

    if ((csr & LN_CSR0_INITDONE) == 0) {    /* make sure we got out okay */
	lance_dumpcsrs(sc, "lance_reset: reset failure");
    } else {
	/* lance_dumpcsrs(sc, "lance_reset: end"); */
	sc->le_if.if_flags |= IFF_UP;
	sc->le_flags |= IFF_UP;
    }
}

static void
lance_init(
    int unit)
{
    le_softc_t *sc = &le_softc[unit];
    lance_ring_t *ri;
    lance_descinfo_t *di;
    ln_desc_t desc;

    LN_STAT(inits++);
    if (sc->le_if.if_flags & IFF_RUNNING) {
	LE_RESET(sc);
	lance_tx_intr(sc);
	/*
	 * If we were running, requeue any pending transmits.
	 */
	ri = &sc->lance_txinfo;
	di = ri->ri_nextout;
	while (ri->ri_free < ri->ri_max) {
	    if (--di == ri->ri_first)
		di = ri->ri_nextout - 1;
	    if (di->di_mbuf == NULL)
		break;
	    IF_PREPEND(&sc->le_if.if_snd, di->di_mbuf);
	    di->di_mbuf = NULL;
	    ri->ri_free++;
	}
    } else {
	LE_RESET(sc);
    }

    /*
     * Reset the transmit ring.  Make sure we own all the buffers.
     * Also reset the transmit heap.
     */
    sc->le_if.if_flags &= ~IFF_OACTIVE;
    ri = &sc->lance_txinfo;
    for (di = ri->ri_first; di < ri->ri_last; di++) {
	if (di->di_mbuf != NULL) {
	    m_freem(di->di_mbuf);
	    di->di_mbuf = NULL;
	}
	desc.d_flag = 0;
	desc.d_addr_lo = LN_ADDR_LO(ri->ri_heap + sc->lance_ramoffset);
	desc.d_addr_hi = LN_ADDR_HI(ri->ri_heap + sc->lance_ramoffset);
	desc.d_buflen = 0;
	LN_PUTDESC(sc, &desc, di->di_addr);
    }
    ri->ri_nextin = ri->ri_nextout = ri->ri_first;
    ri->ri_free = ri->ri_max;
    ri->ri_outptr = ri->ri_heap;
    ri->ri_outsize = ri->ri_heapend - ri->ri_heap;

    ri = &sc->lance_rxinfo;
    desc.d_flag = LN_DFLAG_OWNER;
    desc.d_buflen = 0 - sc->lance_rxbufsize;
    for (di = ri->ri_first; di < ri->ri_last; di++) {
	desc.d_addr_lo = LN_ADDR_LO(di->di_bufaddr + sc->lance_ramoffset);
	desc.d_addr_hi = LN_ADDR_HI(di->di_bufaddr + sc->lance_ramoffset);
	LN_PUTDESC(sc, &desc, di->di_addr);
    }
    ri->ri_nextin = ri->ri_nextout = ri->ri_first;
    ri->ri_outptr = ri->ri_heap;
    ri->ri_outsize = ri->ri_heapend - ri->ri_heap;
    ri->ri_free = 0;

    if (sc->le_if.if_flags & IFF_UP) {
	sc->le_if.if_flags |= IFF_RUNNING;
	LN_WRCSR(sc, LN_CSR0_START|LN_CSR0_INITDONE|LN_CSR0_ENABINTR);
	/* lance_dumpcsrs(sc, "lance_init: up"); */
	lance_start(&sc->le_if);
    } else {
	/* lance_dumpcsrs(sc, "lance_init: down"); */
	sc->le_if.if_flags &= ~IFF_RUNNING;
    }
}

static void
lance_intr(
    le_softc_t *sc)
{
    unsigned oldcsr;

    oldcsr = LN_RDCSR(sc);
    oldcsr &= ~LN_CSR0_ENABINTR;
    LN_WRCSR(sc, oldcsr);
    LN_WRCSR(sc, LN_CSR0_ENABINTR);

    if (oldcsr & LN_CSR0_ERRSUM) {
	if (oldcsr & LN_CSR0_MISS) {
	    /*
             *  LN_CSR0_MISS is signaled when the LANCE receiver
             *  loses a packet because it doesn't own a receive
	     *  descriptor. Rev. D LANCE chips, which are no
	     *  longer used, require a chip reset as described
	     *  below.
	     */
	    LN_STAT(rx_misses++);
	}
	if (oldcsr & LN_CSR0_MEMERROR) {
	    LN_STAT(memory_errors++);
	    if (oldcsr & (LN_CSR0_RXON|LN_CSR0_TXON)) {
		lance_init(sc->le_if.if_unit);
		return;
	    }
	}
    }

    if ((oldcsr & LN_CSR0_RXINT) && lance_rx_intr(sc)) {
	lance_init(sc->le_if.if_unit);
	return;
    }

    if (oldcsr & LN_CSR0_TXINT) {
	if (lance_tx_intr(sc))
	    lance_start(&sc->le_if);
    }

    if (oldcsr == (LN_CSR0_PENDINTR|LN_CSR0_RXON|LN_CSR0_TXON))
        printf("%s%d: lance_intr: stray interrupt\n",
	       sc->le_if.if_name, sc->le_if.if_unit);
}

static int
lance_rx_intr(
    le_softc_t *sc)
{
    lance_ring_t *ri = &sc->lance_rxinfo;
    lance_descinfo_t *eop;
    ln_desc_t desc;
    int ndescs, total_len, rxdescs;

    LN_STAT(rx_intrs++);

    for (rxdescs = 0;;) {
	/*
	 * Now to try to find the end of this packet chain.
	 */
	for (ndescs = 1, eop = ri->ri_nextin;; ndescs++) {
	    /*
	     * If we don't own this descriptor, the packet ain't
	     * all here so return because we are done.
	     */
	    LN_GETDESC(sc, &desc, eop->di_addr);
	    if (desc.d_flag & LN_DFLAG_OWNER)
		return 0;
	    /*
	     * In case we have missed a packet and gotten the
	     * LANCE confused, make sure we are pointing at the
	     * start of a packet. If we aren't, something is really
	     * strange so reinit the LANCE.
	     */
	    if (desc.d_flag & LN_DFLAG_RxBUFERROR) {
		LN_STAT(rx_buferror++);
		return 1;
	    }
	    if ((desc.d_flag & LN_DFLAG_SOP) && eop != ri->ri_nextin) {
		LN_STAT(rx_badsop++);
		return 1;
	    }
	    if (desc.d_flag & LN_DFLAG_EOP)
		break;
	    if (++eop == ri->ri_last)
		eop = ri->ri_first;
	}

	total_len = (desc.d_status & LN_DSTS_RxLENMASK) - 4;
	if ((desc.d_flag & LN_DFLAG_RxERRSUM) == 0) {
	    /*
	     * Valid Packet -- If the SOP is less than or equal to the EOP
	     * or the length is less than the receive buffer size, then the
	     * packet is contiguous in memory and can be copied in one shot.
	     * Otherwise we need to copy two segments to get the entire
	     * packet.
	     */
	    if (ri->ri_nextin <= eop || total_len <= ri->ri_heapend - ri->ri_nextin->di_bufaddr) {
		le_input(sc, sc->le_membase + ri->ri_nextin->di_bufaddr,
			 total_len, total_len, NULL);
		LN_STAT(rx_contig++);
	    } else {
		le_input(sc, sc->le_membase + ri->ri_nextin->di_bufaddr,
			 total_len,
			 ri->ri_heapend - ri->ri_nextin->di_bufaddr,
			 sc->le_membase + ri->ri_first->di_bufaddr);
		LN_STAT(rx_noncontig++);
	    }
	} else {
	    /*
	     * If the packet is bad, increment the
	     * counters.
	     */
	    sc->le_if.if_ierrors++;
	    if (desc.d_flag & LN_DFLAG_RxBADCRC)
		LN_STAT(rx_badcrc++);
	    if (desc.d_flag & LN_DFLAG_RxOVERFLOW)
		LN_STAT(rx_badalign++);
	    if (desc.d_flag & LN_DFLAG_RxFRAMING)
		LN_STAT(rx_badframe++);
	}
	sc->le_if.if_ipackets++;
	LN_STAT(rx_ndescs[ndescs-1]++);
	rxdescs += ndescs;
	while (ndescs-- > 0) {
	    LN_SETFLAG(sc, ri->ri_nextin->di_addr, LN_DFLAG_OWNER);
	    if (++ri->ri_nextin == ri->ri_last)
		ri->ri_nextin = ri->ri_first;
	}
    }
    /* LN_STAT(rx_intr_descs[rxdescs]++); */
    LN_MAXSTAT(rx_intr_hidescs, rxdescs);

    return 0;
}

static void
lance_start(
    struct ifnet *ifp)
{
    le_softc_t *sc = (le_softc_t *) ifp;
    struct ifqueue *ifq = &ifp->if_snd;
    lance_ring_t *ri = &sc->lance_txinfo;
    lance_descinfo_t *di;
    ln_desc_t desc;
    unsigned len, slop;
    struct mbuf *m, *m0;
    caddr_t bp;

    if ((ifp->if_flags & IFF_RUNNING) == 0)
	return;

    for (;;) {
	IF_DEQUEUE(ifq, m);
	if (m == NULL)
	    break;

	/*
	 * Make the packet meets the minimum size for Ethernet.
	 * The slop is so that we also use an even number of longwards.
	 */
	len = ETHERMIN + sizeof(struct ether_header);
	if (m->m_pkthdr.len > len)
	    len = m->m_pkthdr.len;

	slop = (8 - len) & 3;
	/*
	 * If there are no free ring entries (there must be always
	 * one owned by the host), or there's not enough space for
	 * this packet, or this packet would wrap around the end
	 * of LANCE RAM then wait for the transmits to empty for
	 * space and ring entries to become available.
	 */
	if (ri->ri_free == 1 || len + slop > ri->ri_outsize) {
	    /*
	     * Try to see if we can free up anything off the transit ring.
	     */
	    if (lance_tx_intr(sc) > 0) {
		LN_STAT(tx_drains[0]++);
		IF_PREPEND(ifq, m);
		continue;
	    }
	    LN_STAT(tx_nospc[0]++);
	    break;
	}

	if (len + slop > ri->ri_heapend - ri->ri_outptr) {
	    /*
	     * Since the packet won't fit in the end of the transmit
	     * heap, see if there is space at the beginning of the transmit
	     * heap.  If not, try again when there is space.
	     */
	    LN_STAT(tx_orphaned++);
	    slop += ri->ri_heapend - ri->ri_outptr;
	    if (len + slop > ri->ri_outsize) {
		LN_STAT(tx_nospc[1]++);
		break;
	    }
	    /*
	     * Point to the beginning of the heap
	     */
	    ri->ri_outptr = ri->ri_heap;
	    LN_STAT(tx_adoptions++);
	}

	/*
	 * Initialize the descriptor (saving the buffer address,
	 * buffer length, and mbuf) and write the packet out
	 * to the board.
	 */
	di = ri->ri_nextout;
	di->di_bufaddr = ri->ri_outptr;
	di->di_buflen = len + slop;
	di->di_mbuf = m;
	bp = sc->le_membase + di->di_bufaddr;
	for (m0 = m; m0 != NULL; m0 = m0->m_next) {
	    LN_COPYTO(sc, mtod(m0, caddr_t), bp, m0->m_len);
	    bp += m0->m_len;
	}
	/*
	 * Zero out the remainder if needed (< ETHERMIN).
	 */
	if (m->m_pkthdr.len < len)
	    LN_ZERO(sc, bp, len - m->m_pkthdr.len);

	/*
	 * Finally, copy out the descriptor and tell the
	 * LANCE to transmit!.
	 */
	desc.d_buflen = 0 - len;
	desc.d_addr_lo = LN_ADDR_LO(di->di_bufaddr + sc->lance_ramoffset);
	desc.d_addr_hi = LN_ADDR_HI(di->di_bufaddr + sc->lance_ramoffset);
	desc.d_flag = LN_DFLAG_SOP|LN_DFLAG_EOP|LN_DFLAG_OWNER;
	LN_PUTDESC(sc, &desc, di->di_addr);
	LN_WRCSR(sc, LN_CSR0_TXDEMAND|LN_CSR0_ENABINTR);

	/*
	 * Do our bookkeeping with our transmit heap.
	 * (if we wrap, point back to the beginning).
	 */
	ri->ri_outptr += di->di_buflen;
	ri->ri_outsize -= di->di_buflen;
	LN_MAXSTAT(high_txoutptr, ri->ri_outptr);
	LN_MINSTAT(low_txheapsize, ri->ri_outsize);

	if (ri->ri_outptr == ri->ri_heapend)
	    ri->ri_outptr = ri->ri_heap;

	ri->ri_free--;
	if (++ri->ri_nextout == ri->ri_last)
	    ri->ri_nextout = ri->ri_first;
	LN_MINSTAT(low_txfree, ri->ri_free);
    }
    if (m != NULL) {
	ifp->if_flags |= IFF_OACTIVE;
	IF_PREPEND(ifq, m);
    }
}

static int
lance_tx_intr(
    le_softc_t *sc)
{
    lance_ring_t *ri = &sc->lance_txinfo;
    unsigned xmits;

    LN_STAT(tx_intrs++);
    for (xmits = 0; ri->ri_free < ri->ri_max; ) {
	ln_desc_t desc;

	LN_GETDESC(sc, &desc, ri->ri_nextin->di_addr);
	if (desc.d_flag & LN_DFLAG_OWNER)
	    break;

	if (desc.d_flag & (LN_DFLAG_TxONECOLL|LN_DFLAG_TxMULTCOLL))
	    sc->le_if.if_collisions++;
	if (desc.d_flag & LN_DFLAG_TxDEFERRED)
	    LN_STAT(tx_deferred++);
	if (desc.d_flag & LN_DFLAG_TxONECOLL)
	    LN_STAT(tx_single_collisions++);
	if (desc.d_flag & LN_DFLAG_TxMULTCOLL)
	    LN_STAT(tx_multiple_collisions++);

	if (desc.d_flag & LN_DFLAG_TxERRSUM) {
	    if (desc.d_status & (LN_DSTS_TxUNDERFLOW|LN_DSTS_TxBUFERROR|
				 LN_DSTS_TxEXCCOLL|LN_DSTS_TxLATECOLL)) {
		if (desc.d_status & LN_DSTS_TxEXCCOLL) {
		    unsigned tdr;
		    LN_STAT(tx_excessive_collisions++);
		    if ((tdr = (desc.d_status & LN_DSTS_TxTDRMASK)) > 0) {
			tdr *= 100;
			printf("%s%d: lance: warning: excessive collisions: TDR %dns (%d-%dm)\n",
			       sc->le_if.if_name, sc->le_if.if_unit,
			       tdr, (tdr*99)/1000, (tdr*117)/1000);
		    }
		}
		if (desc.d_status & LN_DSTS_TxBUFERROR)
		    LN_STAT(tx_buferror++);
		sc->le_if.if_oerrors++;
		if ((desc.d_status & LN_DSTS_TxLATECOLL) == 0) {
		    lance_init(sc->le_if.if_unit);
		    return 0;
		} else {
		    LN_STAT(tx_late_collisions++);
		}
	    }
	}
	m_freem(ri->ri_nextin->di_mbuf);
	ri->ri_nextin->di_mbuf = NULL;
	sc->le_if.if_opackets++;
	ri->ri_free++;
	ri->ri_outsize += ri->ri_nextin->di_buflen;
	if (++ri->ri_nextin == ri->ri_last)
	    ri->ri_nextin = ri->ri_first;
	sc->le_if.if_flags &= ~IFF_OACTIVE;
	xmits++;
    }
    if (ri->ri_free == ri->ri_max)
	LN_STAT(tx_emptied++);
    /* LN_STAT(tx_intr_descs[xmits]++); */
    LN_MAXSTAT(tx_intr_hidescs, xmits);
    return xmits;
}
#endif /* !defined(LE_NOLANCE) */
#endif /* NLE > 0 */
