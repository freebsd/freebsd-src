/*	$FreeBSD$ */
/*      From NetBSD: if_de.c,v 1.56.2.1 1997/10/27 02:13:25 thorpej Exp    */
/*	$Id: if_lmc.c,v 1.9 1999/02/19 15:08:42 explorer Exp $	*/

/*-
 * Copyright (c) 1994-1997 Matt Thomas (matt@3am-software.com)
 * Copyright (c) LAN Media Corporation 1998, 1999.
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
 */

char lmc_version[] = "BSD 1.1";

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/proc.h>	/* only for declaration of wakeup() used by vm.h */
#if defined(__FreeBSD__)
#include <machine/clock.h>
#elif defined(__bsdi__) || defined(__NetBSD__)
#include <sys/device.h>
#endif

#if defined(__NetBSD__)
#include <dev/pci/pcidevs.h>
#include "rnd.h"
#if NRND > 0
#include <sys/rnd.h>
#endif
#endif

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/netisr.h>

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>

#if defined(__FreeBSD__) || defined(__NetBSD__)
#include <net/if_sppp.h>
#endif

#if defined(__bsdi__)
#if INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#endif

#include <net/netisr.h>
#include <net/if.h>
#include <net/netisr.h>
#include <net/if_types.h>
#include <net/if_p2p.h>
#include <net/if_c_hdlc.h>
#endif

#if defined(__FreeBSD__)
#include <vm/pmap.h>
#include <pci.h>
#if NPCI > 0
#include <pci/pcivar.h>
#include <pci/dc21040reg.h>
#define INCLUDE_PATH_PREFIX "pci/"
#endif
#endif /* __FreeBSD__ */

#if defined(__bsdi__)
#include <i386/pci/ic/dc21040.h>
#include <i386/isa/isa.h>
#include <i386/isa/icu.h>
#include <i386/isa/dma.h>
#include <i386/isa/isavar.h>
#include <i386/pci/pci.h>

#define	INCLUDE_PATH_PREFIX	"i386/pci/"
#endif /* __bsdi__ */

#if defined(__NetBSD__)
#include <machine/bus.h>
#if defined(__alpha__)
#include <machine/intr.h>
#endif
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/ic/dc21040reg.h>
#define	INCLUDE_PATH_PREFIX	"dev/pci/"
#endif /* __NetBSD__ */

/*
 * Intel CPUs should use I/O mapped access.  XXXMLG Is this true on NetBSD
 * too?
 */
#if defined(__i386__)
#define	LMC_IOMAPPED
#endif

/*
 * This turns on all sort of debugging stuff and make the
 * driver much larger.
 */
#if 0
#define LMC_DEBUG
#define DP(x)	printf x
#else
#define DP(x)
#endif

#define	LMC_HZ	10

#ifndef TULIP_GP_PINSET
#define TULIP_GP_PINSET			0x00000100L
#endif
#ifndef TULIP_BUSMODE_READMULTIPLE
#define TULIP_BUSMODE_READMULTIPLE	0x00200000L
#endif

/*
 * C sucks
 */
typedef struct lmc___softc lmc_softc_t;
typedef struct lmc___media lmc_media_t;
typedef struct lmc___ctl lmc_ctl_t;

/*
 * Sigh.  Every OS puts these in different places.  NetBSD and FreeBSD use
 * a C preprocessor that allows this hack, but BSDI does not.
 */
#if defined(__NetBSD__) || defined(__FreeBSD__)
#include INCLUDE_PATH_PREFIX "if_lmcioctl.h"
#include INCLUDE_PATH_PREFIX "if_lmcvar.h"
#include INCLUDE_PATH_PREFIX "if_lmc_common.c"
#include INCLUDE_PATH_PREFIX "if_lmc_media.c"
#else /* BSDI */
#include "i386/pci/if_lmcioctl.h"
#include "i386/pci/if_lmcvar.h"
#include "i386/pci/if_lmc_common.c"
#include "i386/pci/if_lmc_media.c"
#endif

/*
 * This module supports
 *	the DEC 21140A pass 2.2 PCI Fast Ethernet Controller.
 */
static lmc_intrfunc_t lmc_intr_normal(void *);
static ifnet_ret_t lmc_ifstart_one(struct ifnet *ifp);
static ifnet_ret_t lmc_ifstart(struct ifnet *ifp);
static struct mbuf *lmc_txput(lmc_softc_t * const sc, struct mbuf *m);
static void lmc_rx_intr(lmc_softc_t * const sc);

#if defined(__NetBSD__) || defined(__FreeBSD__)
static void lmc_watchdog(struct ifnet *ifp);
#endif
#if defined(__bsdi__)
static int lmc_watchdog(int);
#endif
static void lmc_ifup(lmc_softc_t * const sc);
static void lmc_ifdown(lmc_softc_t * const sc);


/*
 * Code the read the SROM and MII bit streams (I2C)
 */
static void
lmc_delay_300ns(lmc_softc_t * const sc)
{
	int idx;
	for (idx = (300 / 33) + 1; idx > 0; idx--)
		(void)LMC_CSR_READ(sc, csr_busmode);
}


#define EMIT    \
do { \
	LMC_CSR_WRITE(sc, csr_srom_mii, csr); \
	lmc_delay_300ns(sc); \
} while (0)

static void
lmc_srom_idle(lmc_softc_t * const sc)
{
	unsigned bit, csr;
    
	csr  = SROMSEL ; EMIT;
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
	csr ^= SROMCS; EMIT;
	csr  = 0; EMIT;
}

     
static void
lmc_srom_read(lmc_softc_t * const sc)
{   
	unsigned idx; 
	const unsigned bitwidth = SROM_BITWIDTH;
	const unsigned cmdmask = (SROMCMD_RD << bitwidth);
	const unsigned msb = 1 << (bitwidth + 3 - 1);
	unsigned lastidx = (1 << bitwidth) - 1;

	lmc_srom_idle(sc);

	for (idx = 0; idx <= lastidx; idx++) {
		unsigned lastbit, data, bits, bit, csr;
		csr  = SROMSEL ;	        EMIT;
		csr  = SROMSEL | SROMRD;        EMIT;
		csr ^= SROMCSON;                EMIT;
		csr ^=            SROMCLKON;    EMIT;
    
		lastbit = 0;
		for (bits = idx|cmdmask, bit = bitwidth + 3
			     ; bit > 0
			     ; bit--, bits <<= 1) {
			const unsigned thisbit = bits & msb;
			csr ^= SROMCLKOFF; EMIT;    /* clock L data invalid */
			if (thisbit != lastbit) {
				csr ^= SROMDOUT; EMIT;/* clock L invert data */
			} else {
				EMIT;
			}
			csr ^= SROMCLKON; EMIT;     /* clock H data valid */
			lastbit = thisbit;
		}
		csr ^= SROMCLKOFF; EMIT;

		for (data = 0, bits = 0; bits < 16; bits++) {
			data <<= 1;
			csr ^= SROMCLKON; EMIT;     /* clock H data valid */ 
			data |= LMC_CSR_READ(sc, csr_srom_mii) & SROMDIN ? 1 : 0;
			csr ^= SROMCLKOFF; EMIT;    /* clock L data invalid */
		}
		sc->lmc_rombuf[idx*2] = data & 0xFF;
		sc->lmc_rombuf[idx*2+1] = data >> 8;
		csr  = SROMSEL | SROMRD; EMIT;
		csr  = 0; EMIT;
	}
	lmc_srom_idle(sc);
}

#define MII_EMIT    do { LMC_CSR_WRITE(sc, csr_srom_mii, csr); lmc_delay_300ns(sc); } while (0)

static void
lmc_mii_writebits(lmc_softc_t * const sc, unsigned data, unsigned bits)
{
    unsigned msb = 1 << (bits - 1);
    unsigned csr = LMC_CSR_READ(sc, csr_srom_mii) & (MII_RD|MII_DOUT|MII_CLK);
    unsigned lastbit = (csr & MII_DOUT) ? msb : 0;

    csr |= MII_WR; MII_EMIT;  		/* clock low; assert write */

    for (; bits > 0; bits--, data <<= 1) {
	const unsigned thisbit = data & msb;
	if (thisbit != lastbit) {
	    csr ^= MII_DOUT; MII_EMIT;  /* clock low; invert data */
	}
	csr ^= MII_CLKON; MII_EMIT;     /* clock high; data valid */
	lastbit = thisbit;
	csr ^= MII_CLKOFF; MII_EMIT;    /* clock low; data not valid */
    }
}

static void
lmc_mii_turnaround(lmc_softc_t * const sc, unsigned cmd)
{
    unsigned csr = LMC_CSR_READ(sc, csr_srom_mii) & (MII_RD|MII_DOUT|MII_CLK);

    if (cmd == MII_WRCMD) {
	csr |= MII_DOUT; MII_EMIT;	/* clock low; change data */
	csr ^= MII_CLKON; MII_EMIT;	/* clock high; data valid */
	csr ^= MII_CLKOFF; MII_EMIT;	/* clock low; data not valid */
	csr ^= MII_DOUT; MII_EMIT;	/* clock low; change data */
    } else {
	csr |= MII_RD; MII_EMIT;	/* clock low; switch to read */
    }
    csr ^= MII_CLKON; MII_EMIT;		/* clock high; data valid */
    csr ^= MII_CLKOFF; MII_EMIT;	/* clock low; data not valid */
}

static unsigned
lmc_mii_readbits(lmc_softc_t * const sc)
{
    unsigned data;
    unsigned csr = LMC_CSR_READ(sc, csr_srom_mii) & (MII_RD|MII_DOUT|MII_CLK);
    int idx;

    for (idx = 0, data = 0; idx < 16; idx++) {
	data <<= 1;	/* this is NOOP on the first pass through */
	csr ^= MII_CLKON; MII_EMIT;	/* clock high; data valid */
	if (LMC_CSR_READ(sc, csr_srom_mii) & MII_DIN)
	    data |= 1;
	csr ^= MII_CLKOFF; MII_EMIT;	/* clock low; data not valid */
    }
    csr ^= MII_RD; MII_EMIT;		/* clock low; turn off read */

    return data;
}

static unsigned
lmc_mii_readreg(lmc_softc_t * const sc, unsigned devaddr, unsigned regno)
{
    unsigned csr = LMC_CSR_READ(sc, csr_srom_mii) & (MII_RD|MII_DOUT|MII_CLK);
    unsigned data;

    csr &= ~(MII_RD|MII_CLK); MII_EMIT;
    lmc_mii_writebits(sc, MII_PREAMBLE, 32);
    lmc_mii_writebits(sc, MII_RDCMD, 8);
    lmc_mii_writebits(sc, devaddr, 5);
    lmc_mii_writebits(sc, regno, 5);
    lmc_mii_turnaround(sc, MII_RDCMD);

    data = lmc_mii_readbits(sc);
    return data;
}

static void
lmc_mii_writereg(lmc_softc_t * const sc, unsigned devaddr,
		   unsigned regno, unsigned data)
{
    unsigned csr = LMC_CSR_READ(sc, csr_srom_mii) & (MII_RD|MII_DOUT|MII_CLK);
    csr &= ~(MII_RD|MII_CLK); MII_EMIT;
    lmc_mii_writebits(sc, MII_PREAMBLE, 32);
    lmc_mii_writebits(sc, MII_WRCMD, 8);
    lmc_mii_writebits(sc, devaddr, 5);
    lmc_mii_writebits(sc, regno, 5);
    lmc_mii_turnaround(sc, MII_WRCMD);
    lmc_mii_writebits(sc, data, 16);
}

static int
lmc_read_macaddr(lmc_softc_t * const sc)
{
	lmc_srom_read(sc);

	bcopy(sc->lmc_rombuf + 20, sc->lmc_enaddr, 6);

	return 0;
}

/*
 * Check to make certain there is a signal from the modem, and flicker
 * lights as needed.
 */
#if defined(__NetBSD__) || defined(__FreeBSD__)
static void
lmc_watchdog(struct ifnet *ifp)
#endif
#if defined(__bsdi__)
static int
lmc_watchdog(int unit)
#endif
{
#if defined(__NetBSD__) || defined(__FreeBSD__)
	lmc_softc_t * const sc = LMC_IFP_TO_SOFTC(ifp);
#endif
#if defined(__bsdi__)
	lmc_softc_t * const sc = LMC_UNIT_TO_SOFTC(unit);
	struct ifnet *ifp = &sc->lmc_if;
#endif
	int state;
	u_int32_t ostatus;
	u_int32_t link_status;
	u_int32_t ticks;

	state = 0;

	link_status = sc->lmc_media->get_link_status(sc);
	ostatus = ((sc->lmc_flags & LMC_MODEMOK) == LMC_MODEMOK);

	/*
	 * hardware level link lost, but the interface is marked as up.
	 * Mark it as down.
	 */
        if (link_status == 0 && ostatus) {
		printf(LMC_PRINTF_FMT ": physical link down\n",
		       LMC_PRINTF_ARGS);
		sc->lmc_flags &= ~LMC_MODEMOK;
		lmc_led_off(sc, LMC_MII16_LED1);
	}

	/*
	 * hardware link is up, but the interface is marked as down.
	 * Bring it back up again.
	 */
	if (link_status != 0 && !ostatus) {
		printf(LMC_PRINTF_FMT ": physical link up\n",
		       LMC_PRINTF_ARGS);
		if (sc->lmc_flags & LMC_IFUP)
			lmc_ifup(sc);
		sc->lmc_flags |= LMC_MODEMOK;
		lmc_led_on(sc, LMC_MII16_LED1); 
		return;
	}

	/*
	 * remember the timer value
	 */
	ticks = LMC_CSR_READ(sc, csr_gp_timer);
	LMC_CSR_WRITE(sc, csr_gp_timer, 0xffffffffUL);
	sc->ictl.ticks = 0x0000ffff - (ticks & 0x0000ffff);

	ifp->if_timer = 1;
}

/*
 * Mark the interface as "up" and enable TX/RX and TX/RX interrupts.
 * This also does a full software reset.
 */
static void
lmc_ifup(lmc_softc_t * const sc)
{
	sc->lmc_if.if_timer = 0;

	lmc_dec_reset(sc);
	lmc_reset(sc);

	sc->lmc_media->set_link_status(sc, 1);
	sc->lmc_media->set_status(sc, NULL);

	sc->lmc_flags |= LMC_IFUP;
	lmc_led_on(sc, LMC_MII16_LED1);

	/*
	 * select what interrupts we want to get
	 */
	sc->lmc_intrmask |= (TULIP_STS_NORMALINTR
			       | TULIP_STS_RXINTR
			       | TULIP_STS_TXINTR
			       | TULIP_STS_ABNRMLINTR
			       | TULIP_STS_SYSERROR
			       | TULIP_STS_TXSTOPPED
			       | TULIP_STS_TXUNDERFLOW
			       | TULIP_STS_RXSTOPPED
			       );
	LMC_CSR_WRITE(sc, csr_intr, sc->lmc_intrmask);

	sc->lmc_cmdmode |= TULIP_CMD_TXRUN;
	sc->lmc_cmdmode |= TULIP_CMD_RXRUN;
	LMC_CSR_WRITE(sc, csr_command, sc->lmc_cmdmode);

	sc->lmc_if.if_timer = 1;
}

/*
 * Mark the interface as "down" and disable TX/RX and TX/RX interrupts.
 * This is done by performing a full reset on the interface.
 */
static void
lmc_ifdown(lmc_softc_t * const sc)
{
	sc->lmc_if.if_timer = 0;
	sc->lmc_flags &= ~LMC_IFUP;

	sc->lmc_media->set_link_status(sc, 0);
	lmc_led_off(sc, LMC_MII16_LED1);

	lmc_dec_reset(sc);
	lmc_reset(sc);
	sc->lmc_media->set_status(sc, NULL);
}

static void
lmc_rx_intr(lmc_softc_t * const sc)
{
	lmc_ringinfo_t * const ri = &sc->lmc_rxinfo;
	struct ifnet * const ifp = &sc->lmc_if;
	int fillok = 1;

	sc->lmc_rxtick++;

	for (;;) {
		tulip_desc_t *eop = ri->ri_nextin;
		int total_len = 0, last_offset = 0;
		struct mbuf *ms = NULL, *me = NULL;
		int accept = 0;

		if (fillok && sc->lmc_rxq.ifq_len < LMC_RXQ_TARGET)
			goto queue_mbuf;

		/*
		 * If the TULIP has no descriptors, there can't be any receive
		 * descriptors to process.
		 */
		if (eop == ri->ri_nextout)
			break;
	    
		/*
		 * 90% of the packets will fit in one descriptor.  So we
		 * optimize for that case.
		 */
		if ((((volatile tulip_desc_t *) eop)->d_status & (TULIP_DSTS_OWNER|TULIP_DSTS_RxFIRSTDESC|TULIP_DSTS_RxLASTDESC)) == (TULIP_DSTS_RxFIRSTDESC|TULIP_DSTS_RxLASTDESC)) {
			IF_DEQUEUE(&sc->lmc_rxq, ms);
			me = ms;
		} else {
			/*
			 * If still owned by the TULIP, don't touch it.
			 */
			if (((volatile tulip_desc_t *)eop)->d_status & TULIP_DSTS_OWNER)
				break;

			/*
			 * It is possible (though improbable unless the
			 * BIG_PACKET support is enabled or MCLBYTES < 1518)
			 * for a received packet to cross more than one
			 * receive descriptor.
			 */
			while ((((volatile tulip_desc_t *) eop)->d_status & TULIP_DSTS_RxLASTDESC) == 0) {
				if (++eop == ri->ri_last)
					eop = ri->ri_first;
				if (eop == ri->ri_nextout || ((((volatile tulip_desc_t *) eop)->d_status & TULIP_DSTS_OWNER))) {
					return;
				}
				total_len++;
			}

			/*
			 * Dequeue the first buffer for the start of the
			 * packet.  Hopefully this will be the only one we
			 * need to dequeue.  However, if the packet consumed
			 * multiple descriptors, then we need to dequeue
			 * those buffers and chain to the starting mbuf.
			 * All buffers but the last buffer have the same
			 * length so we can set that now. (we add to
			 * last_offset instead of multiplying since we
			 * normally won't go into the loop and thereby
			 * saving a ourselves from doing a multiplication
			 * by 0 in the normal case).
			 */
			IF_DEQUEUE(&sc->lmc_rxq, ms);
			for (me = ms; total_len > 0; total_len--) {
				me->m_len = LMC_RX_BUFLEN;
				last_offset += LMC_RX_BUFLEN;
				IF_DEQUEUE(&sc->lmc_rxq, me->m_next);
				me = me->m_next;
			}
		}

		/*
		 *  Now get the size of received packet (minus the CRC).
		 */
		total_len = ((eop->d_status >> 16) & 0x7FFF);
		if (sc->ictl.crc_length == 16)
			total_len -= 2;
		else
			total_len -= 4;

		if ((sc->lmc_flags & LMC_RXIGNORE) == 0
		    && ((eop->d_status & LMC_DSTS_ERRSUM) == 0
#ifdef BIG_PACKET
			|| (total_len <= sc->lmc_if.if_mtu + PPP_HEADER_LEN
			    && (eop->d_status & TULIP_DSTS_RxOVERFLOW) == 0)
#endif
			)) {
			me->m_len = total_len - last_offset;
#if NBPFILTER > 0
			if (sc->lmc_bpf != NULL) {
				if (me == ms)
					LMC_BPF_TAP(sc, mtod(ms, caddr_t), total_len);
				else
					LMC_BPF_MTAP(sc, ms);
			}
#endif
			sc->lmc_flags |= LMC_RXACT;
			accept = 1;
		} else {
			ifp->if_ierrors++;
			if (eop->d_status & TULIP_DSTS_RxOVERFLOW) {
				sc->lmc_dot3stats.dot3StatsInternalMacReceiveErrors++;
			}
		}

		ifp->if_ipackets++;
		if (++eop == ri->ri_last)
			eop = ri->ri_first;
		ri->ri_nextin = eop;

	queue_mbuf:
		/*
		 * Either we are priming the TULIP with mbufs (m == NULL)
		 * or we are about to accept an mbuf for the upper layers
		 * so we need to allocate an mbuf to replace it.  If we
		 * can't replace it, send up it anyways.  This may cause
		 * us to drop packets in the future but that's better than
		 * being caught in livelock.
		 *
		 * Note that if this packet crossed multiple descriptors
		 * we don't even try to reallocate all the mbufs here.
		 * Instead we rely on the test of the beginning of
		 * the loop to refill for the extra consumed mbufs.
		 */
		if (accept || ms == NULL) {
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
				ms->m_pkthdr.len = total_len;
				ms->m_pkthdr.rcvif = ifp;
#if defined(__NetBSD__) || defined(__FreeBSD__)
				sppp_input(ifp, ms);
#endif
#if defined(__bsdi__)
				sc->lmc_p2pcom.p2p_input(&sc->lmc_p2pcom, ms);
#endif
			}
			ms = m0;
		}
		if (ms == NULL) {
			/*
			 * Couldn't allocate a new buffer.  Don't bother 
			 * trying to replenish the receive queue.
			 */
			fillok = 0;
			sc->lmc_flags |= LMC_RXBUFSLOW;
			continue;
		}
		/*
		 * Now give the buffer(s) to the TULIP and save in our
		 * receive queue.
		 */
		do {
			ri->ri_nextout->d_length1 = LMC_RX_BUFLEN;
			ri->ri_nextout->d_addr1 = LMC_KVATOPHYS(sc, mtod(ms, caddr_t));
			ri->ri_nextout->d_status = TULIP_DSTS_OWNER;
			if (++ri->ri_nextout == ri->ri_last)
				ri->ri_nextout = ri->ri_first;
			me = ms->m_next;
			ms->m_next = NULL;
			IF_ENQUEUE(&sc->lmc_rxq, ms);
		} while ((ms = me) != NULL);

		if (sc->lmc_rxq.ifq_len >= LMC_RXQ_TARGET)
			sc->lmc_flags &= ~LMC_RXBUFSLOW;
	}
}

static int
lmc_tx_intr(lmc_softc_t * const sc)
{
    lmc_ringinfo_t * const ri = &sc->lmc_txinfo;
    struct mbuf *m;
    int xmits = 0;
    int descs = 0;

    sc->lmc_txtick++;

    while (ri->ri_free < ri->ri_max) {
	u_int32_t d_flag;
	if (((volatile tulip_desc_t *) ri->ri_nextin)->d_status & TULIP_DSTS_OWNER)
	    break;

	d_flag = ri->ri_nextin->d_flag;
	if (d_flag & TULIP_DFLAG_TxLASTSEG) {
		const u_int32_t d_status = ri->ri_nextin->d_status;
		IF_DEQUEUE(&sc->lmc_txq, m);
		if (m != NULL) {
#if NBPFILTER > 0
		    if (sc->lmc_bpf != NULL)
			LMC_BPF_MTAP(sc, m);
#endif
		    m_freem(m);
#if defined(LMC_DEBUG)
		} else {
		    printf(LMC_PRINTF_FMT ": tx_intr: failed to dequeue mbuf?!?\n", LMC_PRINTF_ARGS);
#endif
		}
		    xmits++;
		    if (d_status & LMC_DSTS_ERRSUM) {
			sc->lmc_if.if_oerrors++;
			if (d_status & TULIP_DSTS_TxUNDERFLOW)
			    sc->lmc_dot3stats.dot3StatsInternalTransmitUnderflows++;
		    } else {
			if (d_status & TULIP_DSTS_TxDEFERRED)
			    sc->lmc_dot3stats.dot3StatsDeferredTransmissions++;
		    }
	}

	if (++ri->ri_nextin == ri->ri_last)
	    ri->ri_nextin = ri->ri_first;

	ri->ri_free++;
	descs++;
	sc->lmc_if.if_flags &= ~IFF_OACTIVE;
    }
    /*
     * If nothing left to transmit, disable the timer.
     * Else if progress, reset the timer back to 2 ticks.
     */
    sc->lmc_if.if_opackets += xmits;

    return descs;
}

static void
lmc_print_abnormal_interrupt (lmc_softc_t * const sc, u_int32_t csr)
{
	printf(LMC_PRINTF_FMT ": Abnormal interrupt\n", LMC_PRINTF_ARGS);
}

static void
lmc_intr_handler(lmc_softc_t * const sc, int *progress_p)
{
    u_int32_t csr;

    while ((csr = LMC_CSR_READ(sc, csr_status)) & sc->lmc_intrmask) {

#if defined(__NetBSD__)
#if NRND > 0
	    rnd_add_uint32(&sc->lmc_rndsource, csr);
#endif
#endif

	*progress_p = 1;
	LMC_CSR_WRITE(sc, csr_status, csr);

	if (csr & TULIP_STS_SYSERROR) {
	    sc->lmc_last_system_error = (csr & TULIP_STS_ERRORMASK) >> TULIP_STS_ERR_SHIFT;
	    if (sc->lmc_flags & LMC_NOMESSAGES) {
		sc->lmc_flags |= LMC_SYSTEMERROR;
	    } else {
		printf(LMC_PRINTF_FMT ": system error: %s\n",
		       LMC_PRINTF_ARGS,
		       lmc_system_errors[sc->lmc_last_system_error]);
	    }
	    sc->lmc_flags |= LMC_NEEDRESET;
	    sc->lmc_system_errors++;
	    break;
	}
	if (csr & (TULIP_STS_RXINTR | TULIP_STS_RXNOBUF)) {
	    u_int32_t misses = LMC_CSR_READ(sc, csr_missed_frames);
	    if (csr & TULIP_STS_RXNOBUF)
		sc->lmc_dot3stats.dot3StatsMissedFrames += misses & 0xFFFF;
	    /*
	     * Pass 2.[012] of the 21140A-A[CDE] may hang and/or corrupt data
	     * on receive overflows.
	     */
	   if ((misses & 0x0FFE0000) && (sc->lmc_features & LMC_HAVE_RXBADOVRFLW)) {
		sc->lmc_dot3stats.dot3StatsInternalMacReceiveErrors++;
		/*
		 * Stop the receiver process and spin until it's stopped.
		 * Tell rx_intr to drop the packets it dequeues.
		 */
		LMC_CSR_WRITE(sc, csr_command, sc->lmc_cmdmode & ~TULIP_CMD_RXRUN);
		while ((LMC_CSR_READ(sc, csr_status) & TULIP_STS_RXSTOPPED) == 0)
		    ;
		LMC_CSR_WRITE(sc, csr_status, TULIP_STS_RXSTOPPED);
		sc->lmc_flags |= LMC_RXIGNORE;
	    }
	    lmc_rx_intr(sc);
	    if (sc->lmc_flags & LMC_RXIGNORE) {
		/*
		 * Restart the receiver.
		 */
		sc->lmc_flags &= ~LMC_RXIGNORE;
		LMC_CSR_WRITE(sc, csr_command, sc->lmc_cmdmode);
	    }
	}
	if (csr & TULIP_STS_ABNRMLINTR) {
	    u_int32_t tmp = csr & sc->lmc_intrmask
		& ~(TULIP_STS_NORMALINTR|TULIP_STS_ABNRMLINTR);
	    if (csr & TULIP_STS_TXUNDERFLOW) {
		if ((sc->lmc_cmdmode & TULIP_CMD_THRESHOLDCTL) != TULIP_CMD_THRSHLD160) {
		    sc->lmc_cmdmode += TULIP_CMD_THRSHLD96;
		    sc->lmc_flags |= LMC_NEWTXTHRESH;
		} else if (sc->lmc_features & LMC_HAVE_STOREFWD) {
		    sc->lmc_cmdmode |= TULIP_CMD_STOREFWD;
		    sc->lmc_flags |= LMC_NEWTXTHRESH;
		}
	    }
	    if (sc->lmc_flags & LMC_NOMESSAGES) {
		sc->lmc_statusbits |= tmp;
	    } else {
		lmc_print_abnormal_interrupt(sc, tmp);
		sc->lmc_flags |= LMC_NOMESSAGES;
	    }
	    LMC_CSR_WRITE(sc, csr_command, sc->lmc_cmdmode);
	}

	if (csr & TULIP_STS_TXINTR)
		lmc_tx_intr(sc);

	if (sc->lmc_flags & LMC_WANTTXSTART)
	    lmc_ifstart(&sc->lmc_if);
    }
}

static lmc_intrfunc_t
lmc_intr_normal(void *arg)
{
	lmc_softc_t * sc = (lmc_softc_t *) arg;
	int progress = 0;

	lmc_intr_handler(sc, &progress);

#if !defined(LMC_VOID_INTRFUNC)
	return progress;
#endif
}

static struct mbuf *
lmc_mbuf_compress(struct mbuf *m)
{
	struct mbuf *m0;
#if MCLBYTES >= LMC_MTU + PPP_HEADER_LEN && !defined(BIG_PACKET)
	MGETHDR(m0, M_DONTWAIT, MT_DATA);
	if (m0 != NULL) {
		if (m->m_pkthdr.len > MHLEN) {
			MCLGET(m0, M_DONTWAIT);
			if ((m0->m_flags & M_EXT) == 0) {
				m_freem(m);
				m_freem(m0);
				return NULL;
			}
		}
		m_copydata(m, 0, m->m_pkthdr.len, mtod(m0, caddr_t));
		m0->m_pkthdr.len = m0->m_len = m->m_pkthdr.len;
	}
#else
	int mlen = MHLEN;
	int len = m->m_pkthdr.len;
	struct mbuf **mp = &m0;

	while (len > 0) {
		if (mlen == MHLEN) {
			MGETHDR(*mp, M_DONTWAIT, MT_DATA);
		} else {
			MGET(*mp, M_DONTWAIT, MT_DATA);
		}
		if (*mp == NULL) {
			m_freem(m0);
			m0 = NULL;
			break;
		}
		if (len > MLEN) {
			MCLGET(*mp, M_DONTWAIT);
			if (((*mp)->m_flags & M_EXT) == 0) {
				m_freem(m0);
				m0 = NULL;
				break;
			}
			(*mp)->m_len = (len <= MCLBYTES ? len : MCLBYTES);
		} else {
			(*mp)->m_len = (len <= mlen ? len : mlen);
		}
		m_copydata(m, m->m_pkthdr.len - len,
			   (*mp)->m_len, mtod((*mp), caddr_t));
		len -= (*mp)->m_len;
		mp = &(*mp)->m_next;
		mlen = MLEN;
	}
#endif
	m_freem(m);
	return m0;
}

/*
 * queue the mbuf handed to us for the interface.  If we cannot
 * queue it, return the mbuf.  Return NULL if the mbuf was queued.
 */
static struct mbuf *
lmc_txput(lmc_softc_t * const sc, struct mbuf *m)
{
	lmc_ringinfo_t * const ri = &sc->lmc_txinfo;
	tulip_desc_t *eop, *nextout;
	int segcnt, free;
	u_int32_t d_status;
	struct mbuf *m0;

#if defined(LMC_DEBUG)
	if ((sc->lmc_cmdmode & TULIP_CMD_TXRUN) == 0) {
		printf(LMC_PRINTF_FMT ": txput: tx not running\n",
		       LMC_PRINTF_ARGS);
		sc->lmc_flags |= LMC_WANTTXSTART;
		goto finish;
	}
#endif

	/*
	 * Now we try to fill in our transmit descriptors.  This is
	 * a bit reminiscent of going on the Ark two by two
	 * since each descriptor for the TULIP can describe
	 * two buffers.  So we advance through packet filling
	 * each of the two entries at a time to fill each
	 * descriptor.  Clear the first and last segment bits
	 * in each descriptor (actually just clear everything
	 * but the end-of-ring or chain bits) to make sure
	 * we don't get messed up by previously sent packets.
	 *
	 * We may fail to put the entire packet on the ring if
	 * there is either not enough ring entries free or if the
	 * packet has more than MAX_TXSEG segments.  In the former
	 * case we will just wait for the ring to empty.  In the
	 * latter case we have to recopy.
	 */
 again:
	d_status = 0;
	eop = nextout = ri->ri_nextout;
	m0 = m;
	segcnt = 0;
	free = ri->ri_free;
	do {
		int len = m0->m_len;
		caddr_t addr = mtod(m0, caddr_t);
		unsigned clsize = CLBYTES - (((u_long) addr) & (CLBYTES-1));

		while (len > 0) {
			unsigned slen = min(len, clsize);
#ifdef BIG_PACKET
			int partial = 0;
			if (slen >= 2048)
				slen = 2040, partial = 1;
#endif
			segcnt++;
			if (segcnt > LMC_MAX_TXSEG) {
				/*
				 * The packet exceeds the number of transmit
				 * buffer entries that we can use for one
				 * packet, so we have recopy it into one mbuf
				 * and then try again.
				 */
				m = lmc_mbuf_compress(m);
				if (m == NULL)
					goto finish;
				goto again;
			}
			if (segcnt & 1) {
				if (--free == 0) {
					/*
					 * See if there's any unclaimed space
					 * in the transmit ring.
					 */
					if ((free += lmc_tx_intr(sc)) == 0) {
						/*
						 * There's no more room but
						 * since nothing has been
						 * committed at this point,
						 * just show output is active,
						 * put back the mbuf and
						 * return.
						 */
						sc->lmc_flags |= LMC_WANTTXSTART;
						goto finish;
					}
				}
				eop = nextout;
				if (++nextout == ri->ri_last)
					nextout = ri->ri_first;
				eop->d_flag &= TULIP_DFLAG_ENDRING;
				eop->d_flag |= TULIP_DFLAG_TxNOPADDING;
				if (sc->ictl.crc_length == 16)
					eop->d_flag |= TULIP_DFLAG_TxHASCRC;
				eop->d_status = d_status;
				eop->d_addr1 = LMC_KVATOPHYS(sc, addr);
				eop->d_length1 = slen;
			} else {
				/*
				 *  Fill in second half of descriptor
				 */
				eop->d_addr2 = LMC_KVATOPHYS(sc, addr);
				eop->d_length2 = slen;
			}
			d_status = TULIP_DSTS_OWNER;
			len -= slen;
			addr += slen;
#ifdef BIG_PACKET
			if (partial)
				continue;
#endif
			clsize = CLBYTES;
		}
	} while ((m0 = m0->m_next) != NULL);


	/*
	 * The descriptors have been filled in.  Now get ready
	 * to transmit.
	 */
	IF_ENQUEUE(&sc->lmc_txq, m);
	m = NULL;

	/*
	 * Make sure the next descriptor after this packet is owned
	 * by us since it may have been set up above if we ran out
	 * of room in the ring.
	 */
	nextout->d_status = 0;

	/*
	 * If we only used the first segment of the last descriptor,
	 * make sure the second segment will not be used.
	 */
	if (segcnt & 1) {
		eop->d_addr2 = 0;
		eop->d_length2 = 0;
	}

	/*
	 * Mark the last and first segments, indicate we want a transmit
	 * complete interrupt, and tell it to transmit!
	 */
	eop->d_flag |= TULIP_DFLAG_TxLASTSEG | TULIP_DFLAG_TxWANTINTR;

	/*
	 * Note that ri->ri_nextout is still the start of the packet
	 * and until we set the OWNER bit, we can still back out of
	 * everything we have done.
	 */
	ri->ri_nextout->d_flag |= TULIP_DFLAG_TxFIRSTSEG;
	ri->ri_nextout->d_status = TULIP_DSTS_OWNER;

	LMC_CSR_WRITE(sc, csr_txpoll, 1);

	/*
	 * This advances the ring for us.
	 */
	ri->ri_nextout = nextout;
	ri->ri_free = free;

	/*
	 * switch back to the single queueing ifstart.
	 */
	sc->lmc_flags &= ~LMC_WANTTXSTART;
	sc->lmc_if.if_start = lmc_ifstart_one;

	/*
	 * If we want a txstart, there must be not enough space in the
	 * transmit ring.  So we want to enable transmit done interrupts
	 * so we can immediately reclaim some space.  When the transmit
	 * interrupt is posted, the interrupt handler will call tx_intr
	 * to reclaim space and then txstart (since WANTTXSTART is set).
	 * txstart will move the packet into the transmit ring and clear
	 * WANTTXSTART thereby causing TXINTR to be cleared.
	 */
 finish:
	if (sc->lmc_flags & LMC_WANTTXSTART) {
		sc->lmc_if.if_flags |= IFF_OACTIVE;
		sc->lmc_if.if_start = lmc_ifstart;
	}

	return m;
}


/*
 * This routine is entered at splnet() (splsoftnet() on NetBSD)
 */
static int
lmc_ifioctl(struct ifnet * ifp, ioctl_cmd_t cmd, caddr_t data)
{
	lmc_softc_t * const sc = LMC_IFP_TO_SOFTC(ifp);
#if defined(__NetBSD__) || defined(__FreeBSD__)
	lmc_spl_t s;
#endif
	int error = 0;
	struct ifreq *ifr = (struct ifreq *)data;
	u_int32_t new_state;
	u_int32_t old_state;
	lmc_ctl_t ctl;

#if defined(__NetBSD__) || defined(__FreeBSD__)
	s = LMC_RAISESPL();
#endif

	switch (cmd) {
	case LMCIOCGINFO:
		error = copyout(&sc->ictl, ifr->ifr_data, sizeof(lmc_ctl_t));

		goto out;
		break;

	case LMCIOCSINFO:
#if 0 /* XXX */
		error = suser(p->p_ucred, &p->p_acflag);
		if (error)
			goto out;
#endif

		error = copyin(ifr->ifr_data, &ctl, sizeof(lmc_ctl_t));
		if (error != 0)
			goto out;

		sc->lmc_media->set_status(sc, &ctl);

		goto out;
		break;

#if defined(__NetBSD__) || defined(__FreeBSD__)
	case SIOCSIFMTU:
		/*
		 * Don't allow the MTU to get larger than we can handle
		 */
		if (ifr->ifr_mtu > LMC_MTU) {
			error = EINVAL;
			goto out;
		}
#endif
	}

#if defined(__NetBSD__) || defined(__FreeBSD__)
	/*
	 * call the sppp ioctl layer
	 */
	error = sppp_ioctl(ifp, cmd, data);
	if (error != 0)
		goto out;
#endif

#if defined(__bsdi__)
	error = p2p_ioctl(ifp, cmd, data);
#endif

#if defined(__NetBSD__) || defined(__FreeBSD__)
	/*
	 * If we are transitioning from up to down or down to up, call
	 * our init routine.
	 */
	new_state = ifp->if_flags & IFF_UP;
	old_state = sc->lmc_flags & LMC_IFUP;

	if (new_state && !old_state)
		lmc_ifup(sc);
	else if (!new_state && old_state)
		lmc_ifdown(sc);
#endif

 out:
#if defined(__NetBSD__) || defined(__FreeBSD__)
	LMC_RESTORESPL(s);
#endif

	return error;
}

/*
 * These routines gets called at device spl (from sppp_output).
 */

#if defined(__NetBSD__) || defined(__FreeBSD__)
static ifnet_ret_t
lmc_ifstart(struct ifnet * const ifp)
{
	lmc_softc_t * const sc = LMC_IFP_TO_SOFTC(ifp);
	struct mbuf *m;

	if (sc->lmc_flags & LMC_IFUP) {
		while (sppp_isempty(ifp) == 0) {
			m = sppp_dequeue(ifp);
			if ((m = lmc_txput(sc, m)) != NULL) {
				IF_PREPEND(&((struct sppp *)ifp)->pp_fastq, m);
				break;
			}
		}
		LMC_CSR_WRITE(sc, csr_txpoll, 1);
	}
}

static ifnet_ret_t
lmc_ifstart_one(struct ifnet * const ifp)
{
	lmc_softc_t * const sc = LMC_IFP_TO_SOFTC(ifp);
	struct mbuf *m;

	if ((sc->lmc_flags & LMC_IFUP) && (sppp_isempty(ifp) == 0)) {
		m = sppp_dequeue(ifp);
		if ((m = lmc_txput(sc, m)) != NULL) {
			IF_PREPEND(&((struct sppp *)ifp)->pp_fastq, m);
		}
		LMC_CSR_WRITE(sc, csr_txpoll, 1);
	}
}
#endif

#if defined(__bsdi__)
static ifnet_ret_t
lmc_ifstart(struct ifnet * const ifp)
{
	lmc_softc_t * const sc = LMC_IFP_TO_SOFTC(ifp);
	struct mbuf *m;
	struct ifqueue *ifq;

	if ((sc->lmc_flags & LMC_IFUP) == 0)
		return;

	for (;;) {
		ifq = &sc->lmc_p2pcom.p2p_isnd;

		m = ifq->ifq_head;
		if (m == NULL) {
			ifq = &sc->lmc_if.if_snd;
			m = ifq->ifq_head;
		}
		if (m == NULL)
			break;
		IF_DEQUEUE(ifq, m);

		m = lmc_txput(sc, m);
		if (m != NULL) {
			IF_PREPEND(ifq, m);
			break;
		}
	}

	LMC_CSR_WRITE(sc, csr_txpoll, 1);
}

static ifnet_ret_t
lmc_ifstart_one(struct ifnet * const ifp)
{
	lmc_softc_t * const sc = LMC_IFP_TO_SOFTC(ifp);
	struct mbuf *m;
	struct ifqueue *ifq;

	if ((sc->lmc_flags & LMC_IFUP) == 0)
		return;

	ifq = &sc->lmc_p2pcom.p2p_isnd;

	m = ifq->ifq_head;
	if (m == NULL) {
		ifq = &sc->lmc_if.if_snd;
		m = ifq->ifq_head;
	}
	if (m == NULL)
		return 0;
	IF_DEQUEUE(ifq, m);

	m = lmc_txput(sc, m);
	if (m != NULL)
		IF_PREPEND(ifq, m);

	LMC_CSR_WRITE(sc, csr_txpoll, 1);
}
#endif

#if defined(__bsdi__)
int
lmc_getmdm(struct p2pcom *pp, caddr_t b)
{
	lmc_softc_t *sc = LMC_UNIT_TO_SOFTC(pp->p2p_if.if_unit);

	if (sc->lmc_media->get_link_status(sc)) {
		*(int *)b = TIOCM_CAR;
	} else {
		*(int *)b = 0;
	}

	return (0);
}

int
lmc_mdmctl(struct p2pcom *pp, int flag)
{
	lmc_softc_t *sc = LMC_UNIT_TO_SOFTC(pp->p2p_if.if_unit);

	sc->lmc_media->set_link_status(sc, flag);

	if (flag)
		if ((sc->lmc_flags & LMC_IFUP) == 0)
			lmc_ifup(sc);
	else
		if ((sc->lmc_flags & LMC_IFUP) == LMC_IFUP)
			lmc_ifdown(sc);

	return (0);
}
#endif

/*
 * Set up the OS interface magic and attach to the operating system
 * network services.
 */
static void
lmc_attach(lmc_softc_t * const sc)
{
	struct ifnet * const ifp = &sc->lmc_if;

	ifp->if_flags = IFF_POINTOPOINT | IFF_MULTICAST;
	ifp->if_ioctl = lmc_ifioctl;
	ifp->if_start = lmc_ifstart;
	ifp->if_watchdog = lmc_watchdog;
	ifp->if_timer = 1;
	ifp->if_mtu = LMC_MTU;

#if defined(__bsdi__)
	ifp->if_type = IFT_NONE;
	ifp->if_unit = (sc->lmc_dev.dv_unit);
#endif
  
	if_attach(ifp);

#if defined(__NetBSD__) || defined(__FreeBSD__)
	sppp_attach((struct ifnet *)&sc->lmc_sppp);
	sc->lmc_sppp.pp_flags = PP_CISCO | PP_KEEPALIVE;
#endif
#if defined(__bsdi__)
	sc->lmc_p2pcom.p2p_mdmctl = lmc_mdmctl;
	sc->lmc_p2pcom.p2p_getmdm = lmc_getmdm;
	p2p_attach(&sc->lmc_p2pcom);
#endif

#if NBPFILTER > 0
	LMC_BPF_ATTACH(sc);
#endif

#if defined(__NetBSD__) && NRND > 0
	rnd_attach_source(&sc->lmc_rndsource, sc->lmc_dev.dv_xname,
			  RND_TYPE_NET);
#endif

	/*
	 * turn off those LEDs...
	 */
	sc->lmc_miireg16 |= LMC_MII16_LED_ALL;
	lmc_led_on(sc, LMC_MII16_LED0);
}

static void
lmc_initring(lmc_softc_t * const sc, lmc_ringinfo_t * const ri,
	       tulip_desc_t *descs, int ndescs)
{
	ri->ri_max = ndescs;
	ri->ri_first = descs;
	ri->ri_last = ri->ri_first + ri->ri_max;
	bzero((caddr_t) ri->ri_first, sizeof(ri->ri_first[0]) * ri->ri_max);
	ri->ri_last[-1].d_flag = TULIP_DFLAG_ENDRING;
}

/*
 * This is the PCI configuration support.
 */
#define	PCI_CFID	0x00	/* Configuration ID */
#define	PCI_CFCS	0x04	/* Configurtion Command/Status */
#define	PCI_CFRV	0x08	/* Configuration Revision */
#define	PCI_CFLT	0x0c	/* Configuration Latency Timer */
#define	PCI_CBIO	0x10	/* Configuration Base IO Address */
#define	PCI_CBMA	0x14	/* Configuration Base Memory Address */
#define PCI_SSID	0x2c	/* subsystem config register */
#define	PCI_CFIT	0x3c	/* Configuration Interrupt */
#define	PCI_CFDA	0x40	/* Configuration Driver Area */



#if defined(__NetBSD__)
#include "dev/pci/if_lmc_nbsd.c"
#elif defined(__FreeBSD__)
#include "pci/if_lmc_fbsd.c"
#elif defined(__bsdi__)
#include "i386/pci/if_lmc_bsdi.c"
#else
#error "This driver only works on NetBSD, FreeBSD, or BSDi"
#endif
