/*-
 * Copyright (c) 1994, 1995 Matt Thomas (matt@lkg.dec.com)
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
 * $Id: if_de.c,v 1.21 1995/04/13 04:46:40 davidg Exp $
 *
 */

/*
 * DEC DC21040 PCI Ethernet Controller
 *
 * Written by Matt Thomas
 * BPF support code stolen directly from if_ec.c
 *
 *   This driver supports the DEC DE435 or any other PCI
 *   board which support DC21040.
 */

#define __IF_DE_C__  "pl2 95/03/21"
#include "de.h"
#if NDE > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/proc.h>	/* only for declaration of wakeup() used by vm.h */
#include <sys/devconf.h>
#include <machine/clock.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

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

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_param.h>


#include <pci.h>
#if NPCI > 0
#include <pci/pcivar.h>
#endif

#include <pci/dc21040.h>

/*
 * This module supports
 *	the DEC DC21040 PCI Ethernet Controller.
 *	the DEC DC21140 PCI Fast Ethernet Controller.
 */

typedef struct {
    unsigned long addr;
    unsigned long length;
} tulip_addrvec_t;

typedef struct {
    tulip_desc_t *ri_first;
    tulip_desc_t *ri_last;
    tulip_desc_t *ri_nextin;
    tulip_desc_t *ri_nextout;
    int ri_max;
    int ri_free;
} tulip_ringinfo_t;

typedef struct {
    volatile tulip_uint32_t *csr_busmode;		/* CSR0 */
    volatile tulip_uint32_t *csr_txpoll;		/* CSR1 */
    volatile tulip_uint32_t *csr_rxpoll;		/* CSR2 */
    volatile tulip_uint32_t *csr_rxlist;		/* CSR3 */
    volatile tulip_uint32_t *csr_txlist;		/* CSR4 */
    volatile tulip_uint32_t *csr_status;		/* CSR5 */
    volatile tulip_uint32_t *csr_command;		/* CSR6 */
    volatile tulip_uint32_t *csr_intr;			/* CSR7 */
    volatile tulip_uint32_t *csr_missed_frame;		/* CSR8 */

    /* DC21040 specific registers */

    volatile tulip_sint32_t *csr_enetrom;		/* CSR9 */
    volatile tulip_uint32_t *csr_reserved;		/* CSR10 */
    volatile tulip_uint32_t *csr_full_duplex;		/* CSR11 */
    volatile tulip_uint32_t *csr_sia_status;		/* CSR12 */
    volatile tulip_uint32_t *csr_sia_connectivity;	/* CSR13 */
    volatile tulip_uint32_t *csr_sia_tx_rx;		/* CSR14 */
    volatile tulip_uint32_t *csr_sia_general;		/* CSR15 */

    /* DC21140/DC21041 specific registers */

    volatile tulip_uint32_t *csr_srom_mii;		/* CSR9 */
    volatile tulip_uint32_t *csr_gp_timer;		/* CSR11 */
    volatile tulip_uint32_t *csr_gp;			/* CSR12 */
    volatile tulip_uint32_t *csr_watchdog;		/* CSR15 */
} tulip_regfile_t;

/*
 * The DC21040 has a stupid restriction in that the receive
 * buffers must be longword aligned.  But since Ethernet
 * headers are not a multiple of longwords in size this forces
 * the data to non-longword aligned.  Since IP requires the
 * data to be longword aligned, we need to copy it after it has
 * been DMA'ed in our memory.
 *
 * Since we have to copy it anyways, we might as well as allocate
 * dedicated receive space for the input.  This allows to use a
 * small receive buffer size and more ring entries to be able to
 * better keep with a flood of tiny Ethernet packets.
 *
 * The receive space MUST ALWAYS be a multiple of the page size.
 * And the number of receive descriptors multiplied by the size
 * of the receive buffers must equal the recevive space.  This
 * is so that we can manipulate the page tables so that even if a
 * packet wraps around the end of the receive space, we can 
 * treat it as virtually contiguous.
 *
 * The above used to be true (the stupid restriction is still true)
 * but we gone to directly DMA'ing into MBUFs because with 100Mb
 * cards the copying is just too much of a hit.
 */
#define	TULIP_RXDESCS		16
#define	TULIP_TXDESCS		128
#define	TULIP_RXQ_TARGET	8

typedef enum {
    TULIP_DC21040_GENERIC,
    TULIP_DC21140_DEC_EB,
    TULIP_DC21140_DEC_DE500
} tulip_board_t;

typedef struct _tulip_softc_t tulip_softc_t;

typedef struct {
    tulip_board_t bd_type;
    const char *bd_description;
    int (*bd_media_probe)(tulip_softc_t *sc);
    void (*bd_media_select)(tulip_softc_t *sc);
} tulip_boardsw_t;


struct _tulip_softc_t {
    struct arpcom tulip_ac;
    tulip_regfile_t tulip_csrs;
    unsigned tulip_flags;
#define	TULIP_WANTSETUP		0x01
#define	TULIP_WANTHASH		0x02
#define	TULIP_DOINGSETUP	0x04
#define	TULIP_ALTPHYS		0x08	/* use AUI */
    unsigned char tulip_rombuf[128];
    tulip_uint32_t tulip_setupbuf[192/sizeof(tulip_uint32_t)];
    tulip_uint32_t tulip_setupdata[192/sizeof(tulip_uint32_t)];
    tulip_uint32_t tulip_intrmask;
    tulip_uint32_t tulip_cmdmode;
    tulip_uint32_t tulip_revinfo;
    const tulip_boardsw_t *tulip_boardsw;
#if NBPFILTER > 0
    caddr_t tulip_bpf;			/* BPF context */
#endif
    struct ifqueue tulip_txq;
    struct ifqueue tulip_rxq;
    tulip_ringinfo_t tulip_rxinfo;
    tulip_ringinfo_t tulip_txinfo;
};

#ifndef IFF_ALTPHYS
#define	IFF_ALTPHYS	IFF_LINK2		/* In case it isn't defined */
#endif
typedef enum { TULIP_DC21040, TULIP_DC21140, TULIP_DC21041 } tulip_chipid_t;
const char *tulip_chipdescs[] = { 
    "DC21040 [10Mb/s]",
    "DC21140 [10-100Mb/s]",
    "DC21041 [10Mb/s]"
};

tulip_softc_t *tulips[NDE];
tulip_chipid_t tulip_chipids[NDE];

#define	tulip_if	tulip_ac.ac_if
#define	tulip_unit	tulip_ac.ac_if.if_unit
#define	tulip_name	tulip_ac.ac_if.if_name
#define	tulip_hwaddr	tulip_ac.ac_enaddr

#define	TULIP_CRC32_POLY	0xEDB88320UL	/* CRC-32 Poly -- Little Endian */
#define	TULIP_CHECK_RXCRC	0
#define	TULIP_MAX_TXSEG		30

#define	TULIP_ADDREQUAL(a1, a2) \
	(((u_short *)a1)[0] == ((u_short *)a2)[0] \
	 && ((u_short *)a1)[1] == ((u_short *)a2)[1] \
	 && ((u_short *)a1)[2] == ((u_short *)a2)[2])
#define	TULIP_ADDRBRDCST(a1) \
	(((u_short *)a1)[0] == 0xFFFFU \
	 && ((u_short *)a1)[1] == 0xFFFFU \
	 && ((u_short *)a1)[2] == 0xFFFFU)

static void tulip_start(struct ifnet *ifp);
static void tulip_rx_intr(tulip_softc_t *sc);
static void tulip_addr_filter(tulip_softc_t *sc);

#if __FreeBSD__ > 1
#define	TULIP_IFRESET_ARGS	int unit
#define	TULIP_RESET(sc)		tulip_reset((sc)->tulip_unit)
#else
#define	TULIP_IFRESET_ARGS	int unit, int uban
#define	TULIP_RESET(sc)		tulip_reset((sc)->tulip_unit, 0)
#endif


static int
tulip_dc21040_media_probe(
    tulip_softc_t *sc)
{
    int cnt;

    *sc->tulip_csrs.csr_sia_connectivity = 0;
    *sc->tulip_csrs.csr_sia_connectivity = TULIP_SIACONN_10BASET;
    for (cnt = 0; cnt < 2400; cnt++) {
	if ((*sc->tulip_csrs.csr_sia_status & TULIP_SIASTS_LINKFAIL) == 0)
	    break;
	DELAY(1000);
    }
    return (*sc->tulip_csrs.csr_sia_status & TULIP_SIASTS_LINKFAIL) != 0;
}

static void
tulip_dc21040_media_select(
    tulip_softc_t *sc)
{
    sc->tulip_cmdmode |= TULIP_CMD_CAPTREFFCT;
    *sc->tulip_csrs.csr_sia_connectivity = TULIP_SIACONN_RESET;
    if (sc->tulip_if.if_flags & IFF_ALTPHYS) {
	if ((sc->tulip_flags & TULIP_ALTPHYS) == 0)
	    printf("%s%d: enabling Thinwire/AUI port\n",
		   sc->tulip_if.if_name, sc->tulip_if.if_unit);
	*sc->tulip_csrs.csr_sia_connectivity = TULIP_SIACONN_AUI;
	sc->tulip_flags |= TULIP_ALTPHYS;
    } else {
	if (sc->tulip_flags & TULIP_ALTPHYS)
	    printf("%s%d: enabling 10baseT/UTP port\n",
		   sc->tulip_if.if_name, sc->tulip_if.if_unit);
	*sc->tulip_csrs.csr_sia_connectivity = TULIP_SIACONN_10BASET;
	sc->tulip_flags &= ~TULIP_ALTPHYS;
    }
}

static const tulip_boardsw_t tulip_dc21040_boardsw = {
    TULIP_DC21040_GENERIC,
    "",
    tulip_dc21040_media_probe,
    tulip_dc21040_media_select
};

static int
tulip_dc21140_evalboard_media_probe(
    tulip_softc_t *sc)
{
    *sc->tulip_csrs.csr_gp = TULIP_GP_EB_PINS;
    *sc->tulip_csrs.csr_gp = TULIP_GP_EB_INIT;
    *sc->tulip_csrs.csr_command |= TULIP_CMD_PORTSELECT
	|TULIP_CMD_PCSFUNCTION|TULIP_CMD_SCRAMBLER|TULIP_CMD_MUSTBEONE;
    *sc->tulip_csrs.csr_command &= ~TULIP_CMD_TXTHRSHLDCTL;
    DELAY(1000000);
    return (*sc->tulip_csrs.csr_gp & TULIP_GP_EB_OK100) != 0;
}

static void
tulip_dc21140_evalboard_media_select(
    tulip_softc_t *sc)
{
    sc->tulip_cmdmode |= TULIP_CMD_STOREFWD|TULIP_CMD_MUSTBEONE;
    *sc->tulip_csrs.csr_gp = TULIP_GP_EB_PINS;
    *sc->tulip_csrs.csr_gp = TULIP_GP_EB_INIT;
    if (sc->tulip_if.if_flags & IFF_ALTPHYS) {
	if ((sc->tulip_flags & TULIP_ALTPHYS) == 0)
	    printf("%s%d: enabling 100baseTX UTP port\n",
		   sc->tulip_if.if_name, sc->tulip_if.if_unit);
	sc->tulip_cmdmode |= TULIP_CMD_PORTSELECT
	    |TULIP_CMD_PCSFUNCTION|TULIP_CMD_SCRAMBLER;
	sc->tulip_cmdmode &= ~TULIP_CMD_TXTHRSHLDCTL;
	sc->tulip_flags |= TULIP_ALTPHYS;
    } else {
	if (sc->tulip_flags & TULIP_ALTPHYS)
	    printf("%s%d: enabling 10baseT UTP port\n",
		   sc->tulip_if.if_name, sc->tulip_if.if_unit);
	sc->tulip_cmdmode &= ~(TULIP_CMD_PORTSELECT
			       |TULIP_CMD_PCSFUNCTION|TULIP_CMD_SCRAMBLER);
	sc->tulip_cmdmode |= TULIP_CMD_TXTHRSHLDCTL;
	sc->tulip_flags &= ~TULIP_ALTPHYS;
    }
}

static const tulip_boardsw_t tulip_dc21140_eb_boardsw = {
    TULIP_DC21140_DEC_EB,
    "",
    tulip_dc21140_evalboard_media_probe,
    tulip_dc21140_evalboard_media_select
};

static int
tulip_dc21140_de500_media_probe(
    tulip_softc_t *sc)
{
    *sc->tulip_csrs.csr_gp = TULIP_GP_DE500_PINS;
    *sc->tulip_csrs.csr_gp = TULIP_GP_DE500_HALFDUPLEX;
    if ((*sc->tulip_csrs.csr_gp & (TULIP_GP_DE500_NOTOK_100|TULIP_GP_DE500_NOTOK_10)) != (TULIP_GP_DE500_NOTOK_100|TULIP_GP_DE500_NOTOK_10))
	return (*sc->tulip_csrs.csr_gp & TULIP_GP_DE500_NOTOK_100) == 0;
    *sc->tulip_csrs.csr_gp = TULIP_GP_DE500_HALFDUPLEX|TULIP_GP_DE500_FORCE_100;
    *sc->tulip_csrs.csr_command |= TULIP_CMD_PORTSELECT
	|TULIP_CMD_PCSFUNCTION|TULIP_CMD_SCRAMBLER|TULIP_CMD_MUSTBEONE;
    *sc->tulip_csrs.csr_command &= ~TULIP_CMD_TXTHRSHLDCTL;
    DELAY(1000000);
    return (*sc->tulip_csrs.csr_gp & TULIP_GP_DE500_NOTOK_100) == 0;
}

static void
tulip_dc21140_de500_media_select(
    tulip_softc_t *sc)
{
    sc->tulip_cmdmode |= TULIP_CMD_STOREFWD|TULIP_CMD_MUSTBEONE;
    *sc->tulip_csrs.csr_gp = TULIP_GP_DE500_PINS;
    if (sc->tulip_if.if_flags & IFF_ALTPHYS) {
	if ((sc->tulip_flags & TULIP_ALTPHYS) == 0)
	    printf("%s%d: enabling 100baseTX UTP port\n",
		   sc->tulip_if.if_name, sc->tulip_if.if_unit);
	sc->tulip_cmdmode |= TULIP_CMD_PORTSELECT
	    |TULIP_CMD_PCSFUNCTION|TULIP_CMD_SCRAMBLER;
	sc->tulip_cmdmode &= ~TULIP_CMD_TXTHRSHLDCTL;
	sc->tulip_flags |= TULIP_ALTPHYS;
	*sc->tulip_csrs.csr_gp = TULIP_GP_DE500_HALFDUPLEX
	    |TULIP_GP_DE500_FORCE_100;
    } else {
	if (sc->tulip_flags & TULIP_ALTPHYS)
	    printf("%s%d: enabling 10baseT UTP port\n",
		   sc->tulip_if.if_name, sc->tulip_if.if_unit);
	sc->tulip_cmdmode &= ~(TULIP_CMD_PORTSELECT
			       |TULIP_CMD_PCSFUNCTION|TULIP_CMD_SCRAMBLER);
	sc->tulip_cmdmode |= TULIP_CMD_TXTHRSHLDCTL;
	sc->tulip_flags &= ~TULIP_ALTPHYS;
	*sc->tulip_csrs.csr_gp = TULIP_GP_DE500_HALFDUPLEX;
    }
}

static const tulip_boardsw_t tulip_dc21140_de500_boardsw = {
    TULIP_DC21140_DEC_DE500, "Digital DE500 ",
    tulip_dc21140_de500_media_probe,
    tulip_dc21140_de500_media_select
};

static void
tulip_reset(
    TULIP_IFRESET_ARGS)
{
    tulip_softc_t *sc = tulips[unit];
    tulip_ringinfo_t *ri;
    tulip_desc_t *di;

    *sc->tulip_csrs.csr_busmode = TULIP_BUSMODE_SWRESET;
    DELAY(10);	/* Wait 10 microsends (actually 50 PCI cycles but at 
		   33MHz that comes to two microseconds but wait a
		   bit longer anyways) */

    (*sc->tulip_boardsw->bd_media_select)(sc);

    *sc->tulip_csrs.csr_txlist = vtophys(&sc->tulip_txinfo.ri_first[0]);
    *sc->tulip_csrs.csr_rxlist = vtophys(&sc->tulip_rxinfo.ri_first[0]);
    *sc->tulip_csrs.csr_intr = 0;
    *sc->tulip_csrs.csr_busmode = TULIP_BUSMODE_BURSTLEN_8LW
	|TULIP_BUSMODE_CACHE_ALIGN8
	|(BYTE_ORDER != LITTLE_ENDIAN
	  ? TULIP_BUSMODE_BIGENDIAN
	  : 0);

    sc->tulip_txq.ifq_maxlen = TULIP_TXDESCS;
    /*
     * Free all the mbufs that were on the transmit ring.
     */
    for (;;) {
	struct mbuf *m;
	IF_DEQUEUE(&sc->tulip_txq, m);
	if (m == NULL)
	    break;
	m_freem(m);
    }

    ri = &sc->tulip_txinfo;
    ri->ri_nextin = ri->ri_nextout = ri->ri_first;
    ri->ri_free = ri->ri_max;
    for (di = ri->ri_first; di < ri->ri_last; di++)
	di->d_status = 0;

    /*
     * We need to collect all the mbufs were on the 
     * receive ring before we reinit it either to put
     * them back on or to know if we have to allocate
     * more.
     */
    ri = &sc->tulip_rxinfo;
    ri->ri_nextin = ri->ri_nextout = ri->ri_first;
    ri->ri_free = ri->ri_max;
    for (di = ri->ri_first; di < ri->ri_last; di++) {
	di->d_status = 0;
	di->d_length1 = 0; di->d_addr1 = 0;
	di->d_length2 = 0; di->d_addr2 = 0;
    }
    for (;;) {
	struct mbuf *m;
	IF_DEQUEUE(&sc->tulip_rxq, m);
	if (m == NULL)
	    break;
	m_freem(m);
    }

    sc->tulip_intrmask = TULIP_STS_NORMALINTR|TULIP_STS_RXINTR|TULIP_STS_TXINTR
	|TULIP_STS_ABNRMLINTR|TULIP_STS_SYSERROR|TULIP_STS_TXSTOPPED
	    |TULIP_STS_TXBABBLE|TULIP_STS_LINKFAIL|TULIP_STS_RXSTOPPED;
    sc->tulip_flags &= ~(TULIP_DOINGSETUP|TULIP_WANTSETUP);
    tulip_addr_filter(sc);
}

static void
tulip_init(
    int unit)
{
    tulip_softc_t *sc = tulips[unit];

    if (sc->tulip_if.if_flags & IFF_UP) {
	sc->tulip_if.if_flags |= IFF_RUNNING;
	if (sc->tulip_if.if_flags & IFF_PROMISC) {
	    sc->tulip_cmdmode |= TULIP_CMD_PROMISCUOUS;
	} else {
	    sc->tulip_cmdmode &= ~TULIP_CMD_PROMISCUOUS;
	    if (sc->tulip_if.if_flags & IFF_ALLMULTI) {
		sc->tulip_cmdmode |= TULIP_CMD_ALLMULTI;
	    } else {
		sc->tulip_cmdmode &= ~TULIP_CMD_ALLMULTI;
	    }
	}
	sc->tulip_cmdmode |= TULIP_CMD_TXRUN;
	if ((sc->tulip_flags & TULIP_WANTSETUP) == 0) {
	    tulip_rx_intr(sc);
	    sc->tulip_cmdmode |= TULIP_CMD_RXRUN;
	    sc->tulip_intrmask |= TULIP_STS_RXSTOPPED;
	} else {
	    sc->tulip_intrmask &= ~TULIP_STS_RXSTOPPED;
	    tulip_start(&sc->tulip_if);
	}
	sc->tulip_cmdmode |= TULIP_CMD_THRSHLD160;
	*sc->tulip_csrs.csr_intr = sc->tulip_intrmask;
	*sc->tulip_csrs.csr_command = sc->tulip_cmdmode;
    } else {
	TULIP_RESET(sc);
	sc->tulip_if.if_flags &= ~IFF_RUNNING;
    }
}


#if TULIP_CHECK_RXCRC
static unsigned
tulip_crc32(
    u_char *addr,
    int len)
{
    unsigned int crc = 0xFFFFFFFF;
    static unsigned int crctbl[256];
    int idx;
    static int done;
    /*
     * initialize the multicast address CRC table
     */
    for (idx = 0; !done && idx < 256; idx++) {
	unsigned int tmp = idx;
	tmp = (tmp >> 1) ^ (tmp & 1 ? TULIP_CRC32_POLY : 0);	/* XOR */
	tmp = (tmp >> 1) ^ (tmp & 1 ? TULIP_CRC32_POLY : 0);	/* XOR */
	tmp = (tmp >> 1) ^ (tmp & 1 ? TULIP_CRC32_POLY : 0);	/* XOR */
	tmp = (tmp >> 1) ^ (tmp & 1 ? TULIP_CRC32_POLY : 0);	/* XOR */
	tmp = (tmp >> 1) ^ (tmp & 1 ? TULIP_CRC32_POLY : 0);	/* XOR */
	tmp = (tmp >> 1) ^ (tmp & 1 ? TULIP_CRC32_POLY : 0);	/* XOR */
	tmp = (tmp >> 1) ^ (tmp & 1 ? TULIP_CRC32_POLY : 0);	/* XOR */
	tmp = (tmp >> 1) ^ (tmp & 1 ? TULIP_CRC32_POLY : 0);	/* XOR */
	crctbl[idx] = tmp;
    }
    done = 1;

    while (len-- > 0)
	crc = (crc >> 8) ^ crctbl[*addr++] ^ crctbl[crc & 0xFF];

    return crc;
}
#endif

static void
tulip_rx_intr(
    tulip_softc_t *sc)
{
    tulip_ringinfo_t *ri = &sc->tulip_rxinfo;
    struct ifnet *ifp = &sc->tulip_if;

    for (;;) {
	struct ether_header eh;
	tulip_desc_t *eop = ri->ri_nextin;
	int total_len = 0;
	struct mbuf *m = NULL;
	int accept = 0;

	if (sc->tulip_rxq.ifq_len < TULIP_RXQ_TARGET)
	     goto queue_mbuf;

	if (((volatile tulip_desc_t *) eop)->d_status & TULIP_DSTS_OWNER)
	    break;
	
	total_len = ((eop->d_status >> 16) & 0x7FF) - 4;
	IF_DEQUEUE(&sc->tulip_rxq, m);
	if ((eop->d_status & TULIP_DSTS_ERRSUM) == 0) {

#if TULIP_CHECK_RXCRC
	    unsigned crc = tulip_crc32(mtod(m, unsigned char *), total_len);
	    if (~crc != *((unsigned *) &bufaddr[total_len])) {
		printf("de0: bad rx crc: %08x [rx] != %08x\n",
		       *((unsigned *) &bufaddr[total_len]), ~crc);
		goto next;
	    }
#endif
	    eh = *mtod(m, struct ether_header *);
#if NBPFILTER > 0
	    if (sc->tulip_bpf != NULL) {
		bpf_tap(sc->tulip_bpf, mtod(m, caddr_t), total_len);
		if ((eh.ether_dhost[0] & 1) == 0 &&
		    !TULIP_ADDREQUAL(eh.ether_dhost, sc->tulip_ac.ac_enaddr))
		    goto next;
	    } else if (!TULIP_ADDREQUAL(eh.ether_dhost, sc->tulip_ac.ac_enaddr)
		    && !TULIP_ADDRBRDCST(eh.ether_dhost)) {
		    goto next;
	    }
#endif
	    accept = 1;
	} else {
	    ifp->if_ierrors++;
	}
      next:
	ifp->if_ipackets++;
	if (++ri->ri_nextin == ri->ri_last)
	    ri->ri_nextin = ri->ri_first;
      queue_mbuf:
	/*
	 * Either we are priming the TULIP with mbufs (m == NULL)
	 * or we are about to accept an mbuf for the upper layers
	 * so we need to allocate an mbuf to replace it.  If we
	 * can't replace, then count it as an input error and reuse
	 * the mbuf.
	 */
	if (accept || m == NULL) {
	    struct mbuf *m0;
	    MGETHDR(m0, M_DONTWAIT, MT_DATA);
	    if (m0 != NULL) {
		MCLGET(m0, M_DONTWAIT);
		if ((m0->m_flags & M_EXT) == 0) {
		    m_freem(m0);
		    m0 = NULL;
		}
	    }
	    if (accept) {
		if (m0 != NULL) {
		    m->m_pkthdr.rcvif = ifp;
		    m->m_data += sizeof(struct ether_header);
		    m->m_len = m->m_pkthdr.len = total_len;
		    ether_input(ifp, &eh, m);
		    m = m0;
		} else {
		    ifp->if_ierrors++;
		}
	    } else {
		m = m0;
	    }
	}
	if (m == NULL)
	    break;
	/*
	 * Now give the buffer to the TULIP and save in our
	 * receive queue.
	 */
	ri->ri_nextout->d_length1 = MCLBYTES - 4;
	ri->ri_nextout->d_addr1 = vtophys(mtod(m, caddr_t));
	ri->ri_nextout->d_status = TULIP_DSTS_OWNER;
	if (++ri->ri_nextout == ri->ri_last)
	    ri->ri_nextout = ri->ri_first;
	IF_ENQUEUE(&sc->tulip_rxq, m);
    }
}

static int
tulip_tx_intr(
    tulip_softc_t *sc)
{
    tulip_ringinfo_t *ri = &sc->tulip_txinfo;
    struct mbuf *m;
    int xmits = 0;

    while (ri->ri_free < ri->ri_max) {
	if (((volatile tulip_desc_t *) ri->ri_nextin)->d_status & TULIP_DSTS_OWNER)
	    break;

	if (ri->ri_nextin->d_flag & TULIP_DFLAG_TxLASTSEG) {
	    if (ri->ri_nextin->d_flag & TULIP_DFLAG_TxSETUPPKT) {
		/*
		 * We've just finished processing a setup packet.
		 * Mark that we can finished it.  If there's not
		 * another pending, startup the TULIP receiver.
		 * Make sure we ack the RXSTOPPED so we won't get
		 * an abormal interrupt indication.
		 */
		sc->tulip_flags &= ~TULIP_DOINGSETUP;
		if ((sc->tulip_flags & TULIP_WANTSETUP) == 0) {
		    tulip_rx_intr(sc);
		    sc->tulip_cmdmode |= TULIP_CMD_RXRUN;
		    sc->tulip_intrmask |= TULIP_STS_RXSTOPPED;
		    *sc->tulip_csrs.csr_status = TULIP_STS_RXSTOPPED;
		    *sc->tulip_csrs.csr_command = sc->tulip_cmdmode;
		    *sc->tulip_csrs.csr_intr = sc->tulip_intrmask;
		}
	   } else {
		IF_DEQUEUE(&sc->tulip_txq, m);
		m_freem(m);
		sc->tulip_if.if_collisions +=
		    (ri->ri_nextin->d_status & TULIP_DSTS_TxCOLLMASK)
			>> TULIP_DSTS_V_TxCOLLCNT;
		if (ri->ri_nextin->d_status & TULIP_DSTS_ERRSUM)
		    sc->tulip_if.if_oerrors++;
		xmits++;
	    }
	}

	if (++ri->ri_nextin == ri->ri_last)
	    ri->ri_nextin = ri->ri_first;
	ri->ri_free++;
	sc->tulip_if.if_flags &= ~IFF_OACTIVE;
    }
    sc->tulip_if.if_opackets += xmits;
    return xmits;
}

static int
tulip_txsegment(
    tulip_softc_t *sc,
    struct mbuf *m,
    tulip_addrvec_t *avp,
    size_t maxseg)
{
    int segcnt;

    for (segcnt = 0; m; m = m->m_next) {
	int len = m->m_len;
	caddr_t addr = mtod(m, caddr_t);
	unsigned clsize = CLBYTES - (((u_long) addr) & (CLBYTES-1));

	while (len > 0) {
	    unsigned slen = min(len, clsize);
	    if (segcnt < maxseg) {
		avp->addr = vtophys(addr);
		avp->length = slen;
	    }
	    len -= slen;
	    addr += slen;
	    clsize = CLBYTES;
	    avp++;
	    segcnt++;
	}
    }
    if (segcnt >= maxseg) {
	printf("%s%d: tulip_txsegment: extremely fragmented packet encountered (%d segments)\n",
	       sc->tulip_name, sc->tulip_unit, segcnt);
	return -1;
    }
    avp->addr = 0;
    avp->length = 0;
    return segcnt;
}

static void
tulip_start(
    struct ifnet *ifp)
{
    tulip_softc_t *sc = (tulip_softc_t *) ifp;
    struct ifqueue *ifq = &ifp->if_snd;
    tulip_ringinfo_t *ri = &sc->tulip_txinfo;
    tulip_desc_t *sop, *eop;
    struct mbuf *m;
    tulip_addrvec_t addrvec[TULIP_MAX_TXSEG+1], *avp;
    int segcnt;
    tulip_uint32_t d_status;

    if ((ifp->if_flags & IFF_RUNNING) == 0)
	return;

    for (;;) {
	if (sc->tulip_flags & TULIP_WANTSETUP) {
	    if ((sc->tulip_flags & TULIP_DOINGSETUP) || ri->ri_free == 1) {
		ifp->if_flags |= IFF_OACTIVE;
		return;
	    }
	    bcopy(sc->tulip_setupdata, sc->tulip_setupbuf,
		   sizeof(sc->tulip_setupbuf));
	    sc->tulip_flags &= ~TULIP_WANTSETUP;
	    sc->tulip_flags |= TULIP_DOINGSETUP;
	    ri->ri_free--;
	    ri->ri_nextout->d_flag &= TULIP_DFLAG_ENDRING|TULIP_DFLAG_CHAIN;
	    ri->ri_nextout->d_flag |= TULIP_DFLAG_TxFIRSTSEG|TULIP_DFLAG_TxLASTSEG
		    |TULIP_DFLAG_TxSETUPPKT|TULIP_DFLAG_TxWANTINTR;
	    if (sc->tulip_flags & TULIP_WANTHASH)
		ri->ri_nextout->d_flag |= TULIP_DFLAG_TxHASHFILT;
	    ri->ri_nextout->d_length1 = sizeof(sc->tulip_setupbuf);
	    ri->ri_nextout->d_addr1 = vtophys(sc->tulip_setupbuf);
	    ri->ri_nextout->d_length2 = 0;
	    ri->ri_nextout->d_addr2 = 0;
	    ri->ri_nextout->d_status = TULIP_DSTS_OWNER;
	    *sc->tulip_csrs.csr_txpoll = 1;
	    /*
	     * Advance the ring for the next transmit packet.
	     */
	    if (++ri->ri_nextout == ri->ri_last)
		ri->ri_nextout = ri->ri_first;
	}
	    
	IF_DEQUEUE(ifq, m);
	if (m == NULL)
	    break;

	/*
	 * First find out how many and which different pages
	 * the mbuf data occupies.  Then check to see if we
	 * have enough descriptor space in our transmit ring
	 * to actually send it.
	 */
	segcnt = tulip_txsegment(sc, m, addrvec,
				 min(ri->ri_max - 1, TULIP_MAX_TXSEG));
	if (segcnt < 0) {
	    struct mbuf *m0;
	    MGETHDR(m0, M_DONTWAIT, MT_DATA);
	    if (m0 != NULL) {
		if (m->m_pkthdr.len > MHLEN) {
		    MCLGET(m0, M_DONTWAIT);
		    if ((m0->m_flags & M_EXT) == 0) {
			m_freem(m);
			continue;
		    }
		}
		m_copydata(m, 0, m0->m_pkthdr.len, mtod(m0, caddr_t));
		m0->m_pkthdr.len = m0->m_len = m->m_pkthdr.len;
		m_freem(m);
		IF_PREPEND(ifq, m0);
		continue;
	    } else {
		m_freem(m);
		continue;
	    }
	}
	if (ri->ri_free - 2 <= (segcnt + 1) >> 1)
	    break;

	ri->ri_free -= (segcnt + 1) >> 1;
	/*
	 * Now we fill in our transmit descriptors.  This is
	 * a bit reminiscent of going on the Ark two by two
	 * since each descriptor for the TULIP can describe
	 * two buffers.  So we advance through the address
	 * vector two entries at a time to to fill each
	 * descriptor.  Clear the first and last segment bits
	 * in each descriptor (actually just clear everything
	 * but the end-of-ring or chain bits) to make sure
	 * we don't get messed up by previously sent packets.
	 */
	sop = ri->ri_nextout;
	d_status = 0;
	avp = addrvec;
	do {
	    eop = ri->ri_nextout;
	    eop->d_flag &= TULIP_DFLAG_ENDRING|TULIP_DFLAG_CHAIN;
	    eop->d_status = d_status;
	    eop->d_addr1 = avp->addr; eop->d_length1 = avp->length; avp++;
	    eop->d_addr2 = avp->addr; eop->d_length2 = avp->length; avp++;
	    d_status = TULIP_DSTS_OWNER;
	    if (++ri->ri_nextout == ri->ri_last)
		ri->ri_nextout = ri->ri_first;
	} while ((segcnt -= 2) > 0);
#if NBPFILTER > 0
	    if (sc->tulip_bpf != NULL)
		bpf_mtap(sc->tulip_bpf, m);
#endif
	/*
	 * The descriptors have been filled in.  Mark the first
	 * and last segments, indicate we want a transmit complete
	 * interrupt, give the descriptors to the TULIP, and tell
	 * it to transmit!
	 */

	IF_ENQUEUE(&sc->tulip_txq, m);
	eop->d_flag |= TULIP_DFLAG_TxLASTSEG|TULIP_DFLAG_TxWANTINTR;
	sop->d_flag |= TULIP_DFLAG_TxFIRSTSEG;
	sop->d_status = TULIP_DSTS_OWNER;

	*sc->tulip_csrs.csr_txpoll = 1;
    }
    if (m != NULL) {
	ifp->if_flags |= IFF_OACTIVE;
	IF_PREPEND(ifq, m);
    }
}

static int
tulip_intr(
    tulip_softc_t *sc)
{
    tulip_uint32_t csr;
    int progress=0;

    while ((csr = *sc->tulip_csrs.csr_status) & (TULIP_STS_NORMALINTR|TULIP_STS_ABNRMLINTR)) {
	progress = 1;
	*sc->tulip_csrs.csr_status = csr & sc->tulip_intrmask;

	if (csr & TULIP_STS_SYSERROR) {
	    if ((csr & TULIP_STS_ERRORMASK) == TULIP_STS_ERR_PARITY) {
		TULIP_RESET(sc);
		tulip_init(sc->tulip_unit);
		break;
	    }
	}
	if (csr & TULIP_STS_ABNRMLINTR) {
	    printf("%s%d: abnormal interrupt: 0x%05x [0x%05x]\n",
		   sc->tulip_name, sc->tulip_unit, csr, csr & sc->tulip_intrmask);
	    *sc->tulip_csrs.csr_command = sc->tulip_cmdmode;
	}
	if (csr & TULIP_STS_RXINTR)
	    tulip_rx_intr(sc);
	if (sc->tulip_txinfo.ri_free < sc->tulip_txinfo.ri_max) {
	    tulip_tx_intr(sc);
	    tulip_start(&sc->tulip_if);
	}
    }
    return (progress);
}

/*
 *
 */

void
tulip_delay_300ns(
    tulip_softc_t *sc)
{
    *sc->tulip_csrs.csr_busmode; *sc->tulip_csrs.csr_busmode;
    *sc->tulip_csrs.csr_busmode; *sc->tulip_csrs.csr_busmode;

    *sc->tulip_csrs.csr_busmode; *sc->tulip_csrs.csr_busmode;
    *sc->tulip_csrs.csr_busmode; *sc->tulip_csrs.csr_busmode;

    *sc->tulip_csrs.csr_busmode; *sc->tulip_csrs.csr_busmode;
    *sc->tulip_csrs.csr_busmode; *sc->tulip_csrs.csr_busmode;
}

#define EMIT    do { *sc->tulip_csrs.csr_srom_mii = csr; tulip_delay_300ns(sc); } while (0)


void
tulip_idle_srom(
    tulip_softc_t *sc)
{
    unsigned bit, csr;
    
    csr  = SROMSEL | SROMRD; EMIT;  
    csr ^= SROMCS; EMIT;
    csr ^= SROMCLKON; EMIT;

    /*
     * Write 25 cycles of 0 which will force the SROM to be idle.
     */
    for (bit = 3 + SROM_BITWIDTH + 16; bit > 0; bit--) {
        csr ^= SROMCLKOFF; EMIT;    /* clock low; data not valid */
        csr ^= SROMCLKON; EMIT;     /* clock high; data valid */
    }
    csr ^= SROMCLKOFF; EMIT;
    csr ^= SROMCS; EMIT; EMIT;
    csr  = 0; EMIT;
}

     
void
tulip_read_srom(
    tulip_softc_t *sc)
{   
    int idx; 
    const unsigned bitwidth = SROM_BITWIDTH;
    const unsigned cmdmask = (SROMCMD_RD << bitwidth);
    const unsigned msb = 1 << (bitwidth + 3 - 1);
    unsigned lastidx = (1 << bitwidth) - 1;
    int lowbit = 0; 

    tulip_idle_srom(sc);

    for (idx = 0; idx <= lastidx; idx++) {
        unsigned lastbit, data, bits, bit, csr;
        csr  = SROMSEL | SROMRD;        EMIT;
        csr ^= SROMCSON;                EMIT;
        csr ^=            SROMCLKON;    EMIT;
    
        lastbit = 0;
        for (bits = idx|cmdmask, bit = bitwidth + 3; bit > 0; bit--, bits <<= 1) {
            const unsigned thisbit = bits & msb;
            csr ^= SROMCLKOFF; EMIT;    /* clock low; data not valid */
            if (thisbit != lastbit) {
                csr ^= SROMDOUT; EMIT;  /* clock low; invert data */
            }
            csr ^= SROMCLKON; EMIT;     /* clock high; data valid */
            lastbit = thisbit;
        }
        csr ^= SROMCLKOFF; EMIT;

        for (data = 0, bits = 0; bits < 16; bits++) {
            data <<= 1;
            csr ^= SROMCLKON; EMIT;     /* clock high; data valid */ 
            data |= *sc->tulip_csrs.csr_srom_mii & SROMDIN ? 1 : 0;
            csr ^= SROMCLKOFF; EMIT;    /* clock low; data not valid */
        }
	sc->tulip_rombuf[idx*2] = data & 0xFF;
	sc->tulip_rombuf[idx*2+1] = data >> 8;
        csr  = SROMSEL | SROMRD; EMIT;
        csr  = 0; EMIT;
    }
}

#define	tulip_mchash(mca)	(tulip_crc32(mca, 6) & 0x1FF)
#define	tulip_srom_crcok(databuf)	( \
    (tulip_crc32(databuf, 126) & 0xFFFF) == \
     ((databuf)[126] | ((databuf)[127] << 8)))

static unsigned
tulip_crc32(
    const unsigned char *databuf,
    size_t datalen)
{
    u_int idx, bit, data, crc = 0xFFFFFFFFUL;

    for (idx = 0; idx < datalen; idx++)
        for (data = *databuf++, bit = 0; bit < 8; bit++, data >>= 1)
            crc = (crc >> 1) ^ (((crc ^ data) & 1) ? TULIP_CRC32_POLY : 0);
    return crc;
}


/*
 *  This is the standard method of reading the DEC Address ROMS.
 */
static int
tulip_read_macaddr(
    tulip_softc_t *sc)
{
    int cksum, rom_cksum, idx;
    tulip_sint32_t csr;
    unsigned char tmpbuf[8];
    static u_char testpat[] = { 0xFF, 0, 0x55, 0xAA, 0xFF, 0, 0x55, 0xAA };

    if (tulip_chipids[sc->tulip_unit] == TULIP_DC21040) {
	*sc->tulip_csrs.csr_enetrom = 1;
	sc->tulip_boardsw = &tulip_dc21040_boardsw;
	for (idx = 0; idx < 32; idx++) {
	    int cnt = 0;
	    while ((csr = *sc->tulip_csrs.csr_enetrom) < 0 && cnt < 10000)
		cnt++;
	    sc->tulip_rombuf[idx] = csr & 0xFF;
	}
    } else {
	/*
	 * Assume all DC21140 board are compatible with the
	 * DEC 10/100 evaluation board.  Not really valid but ...
	 */
	if (tulip_chipids[sc->tulip_unit] == TULIP_DC21140)
	    sc->tulip_boardsw = &tulip_dc21140_eb_boardsw;
	tulip_read_srom(sc);
	if (tulip_srom_crcok(sc->tulip_rombuf)) {
	    /*
	     * New SROM format.  Copy out the Ethernet address.
	     * If it contains a DE500-XA string, then it must be
	     * a DE500-XA.
	     */
	    bcopy(sc->tulip_rombuf + 20, sc->tulip_hwaddr, 6);
	    if (bcmp(sc->tulip_rombuf + 29, "DE500-XA", 8) == 0)
		sc->tulip_boardsw = &tulip_dc21140_de500_boardsw;
	    if (sc->tulip_boardsw == NULL)
		return -6;
	    return 0;
	}
    }


    if (bcmp(&sc->tulip_rombuf[0], &sc->tulip_rombuf[16], 8) != 0) {
	/*
	 * Some folks don't use the standard ethernet rom format
	 * but instead just put the address in the first 6 bytes
	 * of the rom and let the rest be all 0xffs.  (Can we say
	 * ZNYX???)
	 */
	for (idx = 6; idx < 32; idx++) {
	    if (sc->tulip_rombuf[idx] != 0xFF)
		return -4;
	}
	/*
	 * Make sure the address is not multicast or locally assigned
	 * that the OUI is not 00-00-00.
	 */
	if ((sc->tulip_rombuf[0] & 3) != 0)
	    return -4;
	if (sc->tulip_rombuf[0] == 0 && sc->tulip_rombuf[1] == 0
		&& sc->tulip_rombuf[2] == 0)
	    return -4;
	bcopy(sc->tulip_rombuf, sc->tulip_hwaddr, 6);
	return 0;
    }
    if (bcmp(&sc->tulip_rombuf[24], testpat, 8) != 0)
	return -3;

    tmpbuf[0] = sc->tulip_rombuf[15]; tmpbuf[1] = sc->tulip_rombuf[14];
    tmpbuf[2] = sc->tulip_rombuf[13]; tmpbuf[3] = sc->tulip_rombuf[12];
    tmpbuf[4] = sc->tulip_rombuf[11]; tmpbuf[5] = sc->tulip_rombuf[10];
    tmpbuf[6] = sc->tulip_rombuf[9];  tmpbuf[7] = sc->tulip_rombuf[8];
    if (bcmp(&sc->tulip_rombuf[0], tmpbuf, 8) != 0)
	return -2;

    bcopy(sc->tulip_rombuf, sc->tulip_hwaddr, 6);

    cksum = *(u_short *) &sc->tulip_hwaddr[0];
    cksum *= 2;
    if (cksum > 65535) cksum -= 65535;
    cksum += *(u_short *) &sc->tulip_hwaddr[2];
    if (cksum > 65535) cksum -= 65535;
    cksum *= 2;
    if (cksum > 65535) cksum -= 65535;
    cksum += *(u_short *) &sc->tulip_hwaddr[4];
    if (cksum >= 65535) cksum -= 65535;

    rom_cksum = *(u_short *) &sc->tulip_rombuf[6];
	
    if (cksum != rom_cksum)
	return -1;
    return 0;
}

static void
tulip_addr_filter(
    tulip_softc_t *sc)
{
    tulip_uint32_t *sp = sc->tulip_setupdata;
    struct ether_multistep step;
    struct ether_multi *enm;
    int i;

    sc->tulip_flags &= ~TULIP_WANTHASH;
    sc->tulip_flags |= TULIP_WANTSETUP;
    sc->tulip_cmdmode &= ~TULIP_CMD_RXRUN;
    sc->tulip_intrmask &= ~TULIP_STS_RXSTOPPED;
    if (sc->tulip_ac.ac_multicnt > 14) {
	unsigned hash;
	/*
	 * If we have more than 14 multicasts, we have
	 * go into hash perfect mode (512 bit multicast
	 * hash and one perfect hardware).
	 */

	bzero(sc->tulip_setupdata, sizeof(sc->tulip_setupdata));
	hash = tulip_mchash(etherbroadcastaddr);
	sp[hash >> 4] |= 1 << (hash & 0xF);
	ETHER_FIRST_MULTI(step, &sc->tulip_ac, enm);
	while (enm != NULL) {
	    hash = tulip_mchash(enm->enm_addrlo);
	    sp[hash >> 4] |= 1 << (hash & 0xF);
	    ETHER_NEXT_MULTI(step, enm);
	}
	sc->tulip_cmdmode |= TULIP_WANTHASH;
	sp[39] = ((u_short *) sc->tulip_ac.ac_enaddr)[0]; 
	sp[40] = ((u_short *) sc->tulip_ac.ac_enaddr)[1]; 
	sp[41] = ((u_short *) sc->tulip_ac.ac_enaddr)[2];
    } else {
	/*
	 * Else can get perfect filtering for 16 addresses.
	 */
	i = 0;
	ETHER_FIRST_MULTI(step, &sc->tulip_ac, enm);
	for (; enm != NULL; i++) {
	    *sp++ = ((u_short *) enm->enm_addrlo)[0]; 
	    *sp++ = ((u_short *) enm->enm_addrlo)[1]; 
	    *sp++ = ((u_short *) enm->enm_addrlo)[2];
	    ETHER_NEXT_MULTI(step, enm);
	}
	/*
	 * If an IP address is enabled, turn on broadcast
	 */
	if (sc->tulip_ac.ac_ipaddr.s_addr != 0) {
	    i++;
	    *sp++ = 0xFFFF;
	    *sp++ = 0xFFFF;
	    *sp++ = 0xFFFF;
	}
	/*
	 * Pad the rest with our hardware address
	 */
	for (; i < 16; i++) {
	    *sp++ = ((u_short *) sc->tulip_ac.ac_enaddr)[0]; 
	    *sp++ = ((u_short *) sc->tulip_ac.ac_enaddr)[1]; 
	    *sp++ = ((u_short *) sc->tulip_ac.ac_enaddr)[2];
	}
    }
}

/*extern void arp_ifinit(struct arpcom *, struct ifaddr*);*/

static int
tulip_ioctl(
    struct ifnet *ifp,
    int cmd,
    caddr_t data)
{
    tulip_softc_t *sc = tulips[ifp->if_unit];
    struct ifaddr *ifa = (struct ifaddr *)data;
    struct ifreq *ifr = (struct ifreq *) data;
    int s, error = 0;

    s = splimp();

    switch (cmd) {
	case SIOCSIFADDR: {

	    ifp->if_flags |= IFF_UP;
	    switch(ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET: {
		    ((struct arpcom *)ifp)->ac_ipaddr = IA_SIN(ifa)->sin_addr;
		    tulip_addr_filter(sc);	/* reset multicast filtering */
		    (*ifp->if_init)(ifp->if_unit);
		    arp_ifinit((struct arpcom *)ifp, ifa);
		    break;
		}
#endif /* INET */

#ifdef NS
		/* This magic copied from if_is.c; I don't use XNS,
		 * so I have no way of telling if this actually
		 * works or not.
		 */
		case AF_NS: {
		    struct ns_addr *ina = &(IA_SNS(ifa)->sns_addr);
		    if (ns_nullhost(*ina)) {
			ina->x_host = *(union ns_host *)(sc->tulip_ac.ac_enaddr);
		    } else {
			ifp->if_flags &= ~IFF_RUNNING;
			bcopy((caddr_t)ina->x_host.c_host,
			      (caddr_t)sc->tulip_ac.ac_enaddr,
			      sizeof sc->tulip_ac.ac_enaddr);
		    }

		    (*ifp->if_init)(ifp->if_unit);
		    break;
		}
#endif /* NS */

		default: {
		    (*ifp->if_init)(ifp->if_unit);
		    break;
		}
	    }
	    break;
	}

	case SIOCSIFFLAGS: {
	    /*
	     * Changing the connection forces a reset.
	     */
	    if (sc->tulip_flags & TULIP_ALTPHYS) {
		if ((ifp->if_flags & IFF_ALTPHYS) == 0)
		    TULIP_RESET(sc);
	    } else {
		if (ifp->if_flags & IFF_ALTPHYS)
		    TULIP_RESET(sc);
	    }
	    (*ifp->if_init)(ifp->if_unit);
	    break;
	}

	case SIOCADDMULTI:
	case SIOCDELMULTI: {
	    /*
	     * Update multicast listeners
	     */
	    if (cmd == SIOCADDMULTI)
		error = ether_addmulti(ifr, &sc->tulip_ac);
	    else
		error = ether_delmulti(ifr, &sc->tulip_ac);

	    if (error == ENETRESET) {
		tulip_addr_filter(sc);		/* reset multicast filtering */
		(*ifp->if_init)(ifp->if_unit);
		error = 0;
	    }
	    break;
	}
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

	default: {
	    error = EINVAL;
	    break;
	}
    }

    splx(s);
    return error;
}

static void
tulip_attach(
    tulip_softc_t *sc)
{
    struct ifnet *ifp = &sc->tulip_if;

    if ((*sc->tulip_boardsw->bd_media_probe)(sc)) {
	ifp->if_flags |= IFF_ALTPHYS;
    } else {
	sc->tulip_flags |= TULIP_ALTPHYS;
    }

    TULIP_RESET(sc);

    ifp->if_flags = IFF_BROADCAST|IFF_SIMPLEX|IFF_NOTRAILERS|IFF_MULTICAST;
    ifp->if_init = tulip_init;
    ifp->if_ioctl = tulip_ioctl;
    ifp->if_output = ether_output;
    ifp->if_reset = tulip_reset;
    ifp->if_start = tulip_start;
  
    printf("%s%d: %s%s pass %d.%d Ethernet address %s\n", 
	   sc->tulip_name, sc->tulip_unit,
	   sc->tulip_boardsw->bd_description,
	   tulip_chipdescs[tulip_chipids[sc->tulip_unit]],
	   (sc->tulip_revinfo & 0xF0) >> 4,
	   sc->tulip_revinfo & 0x0F,
	   ether_sprintf(sc->tulip_hwaddr));

    if_attach(ifp);

#if NBPFILTER > 0
    bpfattach(&sc->tulip_bpf, ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif
}

static void
tulip_initcsrs(
    tulip_softc_t *sc,
    volatile tulip_uint32_t *va_csrs,
    size_t csr_size)
{
    sc->tulip_csrs.csr_busmode		= va_csrs +  0 * csr_size;
    sc->tulip_csrs.csr_txpoll		= va_csrs +  1 * csr_size;
    sc->tulip_csrs.csr_rxpoll		= va_csrs +  2 * csr_size;
    sc->tulip_csrs.csr_rxlist		= va_csrs +  3 * csr_size;
    sc->tulip_csrs.csr_txlist		= va_csrs +  4 * csr_size;
    sc->tulip_csrs.csr_status		= va_csrs +  5 * csr_size;
    sc->tulip_csrs.csr_command		= va_csrs +  6 * csr_size;
    sc->tulip_csrs.csr_intr		= va_csrs +  7 * csr_size;
    sc->tulip_csrs.csr_missed_frame	= va_csrs +  8 * csr_size;
    if (tulip_chipids[sc->tulip_unit] == TULIP_DC21040) {
	sc->tulip_csrs.csr_enetrom		= va_csrs +  9 * csr_size;
	sc->tulip_csrs.csr_reserved		= va_csrs + 10 * csr_size;
	sc->tulip_csrs.csr_full_duplex		= va_csrs + 11 * csr_size;
	sc->tulip_csrs.csr_sia_status		= va_csrs + 12 * csr_size;
	sc->tulip_csrs.csr_sia_connectivity	= va_csrs + 13 * csr_size;
	sc->tulip_csrs.csr_sia_tx_rx 		= va_csrs + 14 * csr_size;
	sc->tulip_csrs.csr_sia_general		= va_csrs + 15 * csr_size;
    } else if (tulip_chipids[sc->tulip_unit] == TULIP_DC21140) {
	if (sc->tulip_revinfo < 0x10)
	    sc->tulip_csrs.csr_enetrom	= va_csrs +  9 * csr_size;
	else
	    sc->tulip_csrs.csr_srom_mii = va_csrs +  9 * csr_size;

	sc->tulip_csrs.csr_gp_timer	= va_csrs + 11 * csr_size;
	sc->tulip_csrs.csr_gp		= va_csrs + 12 * csr_size;
	sc->tulip_csrs.csr_watchdog	= va_csrs + 15 * csr_size;
    }
}

static void
tulip_initring(
    tulip_softc_t *sc,
    tulip_ringinfo_t *ri,
    tulip_desc_t *descs,
    int ndescs)
{
    ri->ri_max = ndescs;
    ri->ri_first = descs;
    ri->ri_last = ri->ri_first + ri->ri_max;
    bzero((caddr_t) ri->ri_first, sizeof(ri->ri_first[0]) * ri->ri_max);
    ri->ri_last[-1].d_flag = TULIP_DFLAG_ENDRING;
}

#if NPCI > 0
/*
 * This is the PCI configuration support.  Since the DC21040 is available
 * on both EISA and PCI boards, one must be careful in how defines the
 * DC21040 in the config file.
 */
static char* tulip_pci_probe (pcici_t config_id, pcidi_t device_id);
static void  tulip_pci_attach(pcici_t config_id, int unit);
static u_long tulip_pci_count;
static int tulip_pci_shutdown(struct kern_devconf *, int);

struct pci_device dedevice = {
    "de",
    tulip_pci_probe,
    tulip_pci_attach,
   &tulip_pci_count,
    tulip_pci_shutdown,
};

DATA_SET (pcidevice_set, dedevice);

#define	PCI_CFID	0x00	/* Configuration ID */
#define	PCI_CFCS	0x04	/* Configurtion Command/Status */
#define	PCI_CFRV	0x08	/* Configuration Revision */
#define	PCI_CFLT	0x0c	/* Configuration Latency Timer */
#define	PCI_CBIO	0x10	/* Configuration Base IO Address */
#define	PCI_CBMA	0x14	/* Configuration Base Memory Address */
#define	PCI_CFIT	0x3c	/* Configuration Interrupt */
#define	PCI_CFDA	0x40	/* Configuration Driver Area */

#define	TULIP_PCI_CSRSIZE	(8 / sizeof(tulip_uint32_t))
static char*
tulip_pci_probe(
    pcici_t config_id,
    pcidi_t device_id)
{
    if (device_id == 0x00021011ul) {
	if (tulip_pci_count < NDE)
	    tulip_chipids[tulip_pci_count] = TULIP_DC21040;
	return "Digital DC21040 Ethernet";
    }
    if (device_id == 0x00141011ul) {
	if (tulip_pci_count < NDE)
	    tulip_chipids[tulip_pci_count] = TULIP_DC21041;
	return "Digital DC21041 Ethernet";
    }
    if (device_id == 0x00091011ul) {
	if (tulip_pci_count < NDE)
	    tulip_chipids[tulip_pci_count] = TULIP_DC21140;
	return "Digital DC21140 Fast Ethernet";
    }
    return NULL;
}

static void
tulip_pci_attach(
    pcici_t config_id,
    int unit)
{
    tulip_softc_t *sc;
    int retval, idx, revinfo;
    vm_offset_t va_csrs, pa_csrs;
    tulip_desc_t *rxdescs, *txdescs;

    if (unit >= NDE) {
	printf("de%d: not configured; kernel is built for only %d device%s.\n",
	       unit, NDE, NDE == 1 ? "" : "s");
	return;
    }

    revinfo = pci_conf_read(config_id, PCI_CFRV) & 0xFF;
    if (tulip_chipids[unit] == TULIP_DC21040 && revinfo < 0x22) {
	printf("de%d: not configured; DC21040 pass 2.2 required (%d.%d found)\n",
	       unit, revinfo >> 4, revinfo & 0x0f);
	return;
    } else if (tulip_chipids[unit] == TULIP_DC21140 && revinfo < 0x11) {
	printf("de%d: not configured; DC21140 pass 1.1 required (%d.%d found)\n",
	       unit, revinfo >> 4, revinfo & 0x0f);
	return;
    }


    sc = (tulip_softc_t *) malloc(sizeof(*sc), M_DEVBUF, M_NOWAIT);
    if (sc == NULL)
	return;

    rxdescs = (tulip_desc_t *)
	malloc(sizeof(tulip_desc_t) * TULIP_RXDESCS, M_DEVBUF, M_NOWAIT);
    if (rxdescs == NULL) {
	free((caddr_t) sc, M_DEVBUF);
	return;
    }

    txdescs = (tulip_desc_t *)
	malloc(sizeof(tulip_desc_t) * TULIP_TXDESCS, M_DEVBUF, M_NOWAIT);
    if (txdescs == NULL) {
	free((caddr_t) rxdescs, M_DEVBUF);
	free((caddr_t) sc, M_DEVBUF);
	return;
    }

    bzero(sc, sizeof(*sc));				/* Zero out the softc*/

    sc->tulip_unit = unit;
    sc->tulip_name = "de";
    retval = pci_map_mem(config_id, PCI_CBMA, &va_csrs, &pa_csrs);
    if (!retval) {
	free((caddr_t) txdescs, M_DEVBUF);
	free((caddr_t) rxdescs, M_DEVBUF);
	free((caddr_t) sc, M_DEVBUF);
	return;
    }
    tulips[unit] = sc;
    sc->tulip_revinfo = pci_conf_read(config_id, PCI_CFRV) & 0xFF;
    tulip_initcsrs(sc, (volatile tulip_uint32_t *) va_csrs, TULIP_PCI_CSRSIZE);
    tulip_initring(sc, &sc->tulip_rxinfo, rxdescs, TULIP_RXDESCS);
    tulip_initring(sc, &sc->tulip_txinfo, txdescs, TULIP_TXDESCS);
    if ((retval = tulip_read_macaddr(sc)) < 0) {
	printf("de%d: can't read ENET ROM (why=%d) (", sc->tulip_unit, retval);
	for (idx = 0; idx < 32; idx++)
	    printf("%02x", sc->tulip_rombuf[idx]);
	printf("\n");
	printf("%s%d: %s%s pass %d.%d Ethernet address %s\n",
	       sc->tulip_name, sc->tulip_unit,
	       (sc->tulip_boardsw != NULL ? sc->tulip_boardsw->bd_description : ""),
	       tulip_chipdescs[tulip_chipids[sc->tulip_unit]],
	       (sc->tulip_revinfo & 0xF0) >> 4, sc->tulip_revinfo & 0x0F,
	       "unknown");
    } else {
	TULIP_RESET(sc);
	tulip_attach(sc);
	pci_map_int (config_id, tulip_intr, (void*) sc, &net_imask);
    }
}

static int
tulip_pci_shutdown(
    struct kern_devconf *kdc,
    int force)
{
    if (kdc->kdc_unit < NDE) {
	tulip_softc_t *sc = tulips[kdc->kdc_unit];
	*sc->tulip_csrs.csr_busmode = TULIP_BUSMODE_SWRESET;
	DELAY(10);	/* Wait 10 microsends (actually 50 PCI cycles but at 
			   33MHz that comes to two microseconds but wait a
			   bit longer anyways) */
    }
    (void) dev_detach(kdc);
    return 0;
}

#endif /* NPCI > 0 */
#endif /* NDE > 0 */
