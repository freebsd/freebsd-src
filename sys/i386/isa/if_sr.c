/*
 * Copyright (c) 1996 John Hay.  All rights reserved.
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
 *	This product includes software developed by John Hay.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY John Hay ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL John Hay BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id$
 */

/*
 * Programming assumptions and other issues.
 *
 * Only a 16K window will be used.
 *
 * The descriptors of a DMA channel will fit in a 16K memory window.
 *
 * The buffers of a transmit DMA channel will fit in a 16K memory window.
 *
 * When interface is going up, handshaking is set and it is only cleared
 * when the interface is down'ed.
 *
 * There should be a way to set/reset Raw HDLC/PPP, Loopback, DCE/DTE,
 * internal/external clock, etc.....
 *
 */

#include "sr.h"
#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/malloc.h>

#include <sys/syslog.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/if_sppp.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <sys/devconf.h>
#include <machine/clock.h>
#include <machine/md_var.h>

#include <i386/isa/if_srregs.h>
#include <i386/isa/ic/hd64570.h>

#ifdef TRACE
#define TRC(x)               x
#else
#define TRC(x)
#endif

#define TRCL(x)              x

#define PPP_HEADER_LEN       4

#define SRC_REG(iobase,y)	((y & 0xf) + ((y & 0xf0) << 6) + \
					iobase | 0x8000)

#define SRC_GET8(iobase,off)	inb(SRC_REG(iobase,(u_int)&off))
#define SRC_GET16(iobase,off)	inw(SRC_REG(iobase,(u_int)&off))
#define SRC_PUT8(iobase,off,d)	outb(SRC_REG(iobase,(u_int)&off),d)
#define SRC_PUT16(iobase,off,d)	outw(SRC_REG(iobase,(u_int)&off),d)

#define SRC_GET_WIN(addr)	((addr >> SRC_WIN_SHFT) & SR_PG_MSK)

#define SRC_SET_ON(iobase)	outb(iobase+SR_PCR, \
					SR_PCR_MEM_WIN | inb(iobase+SR_PCR))
#define SRC_SET_MEM(iobase,win)	outb(iobase+SR_PSR, SRC_GET_WIN(win) | \
					inb(iobase+SR_PSR) & ~SR_PG_MSK)
#define SRC_SET_OFF(iobase)	outb(iobase+SR_PCR, \
					~SR_PCR_MEM_WIN & inb(iobase+SR_PCR))

static struct sr_hardc {
	int cunit;
	struct sr_softc *sc;
	u_short iobase;
	int startunit;
	int numports;
	caddr_t mem_start;
	caddr_t mem_end;

	u_int memsize;		/* in bytes */

	sca_regs *sca;

	struct kern_devconf kdc;
}sr_hardc[NSR];

struct sr_softc {
	struct sppp ifsppp;
	int unit;            /* With regards to all sr devices */
	int subunit;         /* With regards to this card */
	struct sr_hardc *hc;

	struct buf_block {
		u_int txdesc;        /* On card address */
		u_int txstart;       /* On card address */
		u_int txend;         /* On card address */
		u_int txtail;        /* Index of first unused buffer */
		u_int txmax;         /* number of usable buffers/descriptors */
		u_int txeda;         /* Error descriptor addresses */
	}block[SR_TX_BLOCKS];

	char  xmit_busy;     /* Transmitter is busy */
	char  txb_inuse;     /* Number of tx blocks currently in use */
	char  txb_new;       /* Index to where new buffer will be added */
	char  txb_next_tx;    /* Index to next block ready to tx */

	u_int rxdesc;        /* On card address */
	u_int rxstart;       /* On card address */
	u_int rxend;         /* On card address */
	u_int rxhind;        /* Index to the head of the rx buffers. */
	u_int rxmax;         /* number of usable buffers/descriptors */

	u_int clk_cfg;       /* Clock configuration */

	int scachan;

	struct kern_devconf kdc;
};

static int srprobe(struct isa_device *id);
static int srattach(struct isa_device *id);

/*
 * List of valid interrupt numbers.
 */
static int sr_irqtable[16] = {
	0,	/*  0 */
	0,	/*  1 */
	0,	/*  2 */
	1,	/*  3 */
	1,	/*  4 */
	1,	/*  5 */
	0,	/*  6 */
	1,	/*  7 */
	0,	/*  8 */
	0,	/*  9 */
	1,	/* 10 */
	1,	/* 11 */
	1,	/* 12 */
	0,	/* 13 */
	0,	/* 14 */
	1	/* 15 */
};

struct isa_driver srdriver = {srprobe, srattach, "src"};

static struct kern_devconf kdc_sr_template = { 
	0, 0, 0, 
	"sr", 0, { MDDT_ISA, 0, "net" },
	isa_generic_externalize, 0, 0, ISA_EXTERNALLEN, 
	&kdc_isa0, 
	0,
	DC_UNCONFIGURED, 
	"SDL Riscom/N2 Port",
	DC_CLS_NETIF
};

static struct kern_devconf kdc_src_template = { 
	0, 0, 0, 
	"src", 0, { MDDT_ISA, 0, "net" },
	isa_generic_externalize, 0, 0, ISA_EXTERNALLEN, 
	&kdc_isa0, 
	0,
	DC_UNCONFIGURED, 
	"SDL Riscom/N2 Adapter",
	DC_CLS_NETIF
};

static void sr_xmit(struct sr_softc *sc);
static void srstart(struct ifnet *ifp);
static int srioctl(struct ifnet *ifp, int cmd, caddr_t data);
static void srwatchdog(struct ifnet *ifp);
static int sr_packet_avail(struct sr_softc *sc, int *len, u_char *rxstat);
static void sr_copy_rxbuf(struct mbuf *m, struct sr_softc *sc, int len);
static void sr_eat_packet(struct sr_softc *sc, int single);
static void sr_get_packets(struct sr_softc *sc);

static void sr_up(struct sr_softc *sc);
static void sr_down(struct sr_softc *sc);
static void src_init(struct isa_device *id);
static void sr_init_sca(struct sr_hardc *hc);
static void sr_init_msci(struct sr_softc *sc);
static void sr_init_rx_dmac(struct sr_softc *sc);
static void sr_init_tx_dmac(struct sr_softc *sc);
static void sr_dmac_intr(struct sr_hardc *hc, u_char isr);
static void sr_msci_intr(struct sr_hardc *hc, u_char isr);
static void sr_timer_intr(struct sr_hardc *hc, u_char isr);

static inline void
sr_registerdev(int ctlr, int unit)
{
	struct sr_softc *sc;
	
	sc = &sr_hardc[ctlr].sc[unit];
	sc->kdc = kdc_sr_template;

	sc->kdc.kdc_unit = sr_hardc[ctlr].startunit + unit;
	sc->kdc.kdc_parentdata = &sr_hardc[ctlr].kdc;
	dev_attach(&sc->kdc);
}

static inline void
src_registerdev(struct isa_device *dvp)
{
        int unit = dvp->id_unit;
	struct sr_hardc *hc = &sr_hardc[dvp->id_unit];

	hc->kdc = kdc_src_template;

        hc->kdc.kdc_unit = unit;
        hc->kdc.kdc_parentdata = dvp;
        dev_attach(&hc->kdc);
}


/*
 * Register the Adapter.
 * Probe to see if it is there.
 * Get its information and fill it in.
 */
static int
srprobe(struct isa_device *id)
{
	struct sr_hardc *hc = &sr_hardc[id->id_unit];
	u_int pgs, i, tmp;
	u_short port;
	u_short *smem;
	u_char mar;
	sca_regs *sca = 0;

	/*
	 * Register the card.
	 */
	src_registerdev(id);

	/*
	 * Now see if the card is realy there.
	 *
	 * If it is there, size its memory.
	 */
	hc->sca = 0;
	port = id->id_iobase;
	hc->memsize = 64 * 1024;
	hc->numports = NCHAN;
	if(id->id_flags & SR_FLAGS_NCHAN_MSK)
		hc->numports = id->id_flags & SR_FLAGS_NCHAN_MSK;

	outb(port + SR_PCR, 0);
	for(i=0;i<0x100;i++) {
		outb(port + SR_BAR, i);
		inb(port + SR_PCR);
		tmp = inb(port + SR_BAR);
		if(tmp != i) {
			printf("sr%d: probe failed BAR %x, %x.\n",
				id->id_unit,
				i,
				tmp);
			return 0;
		}
	}

	/*
	 * Now see if its memory also works.
	 */
	outb(port + SR_PCR, SR_PCR_EN_VPM |
				  SR_PCR_ISA16);
	outb(port + SR_PSR, SR_PSR_WIN_16K);

	mar = (kvtop(id->id_maddr) >> 16) & SR_PCR_16M_SEL;
	outb(port + SR_PCR, mar | inb(port + SR_PCR));
	mar = kvtop(id->id_maddr) >> 12;
	outb(port + SR_BAR, mar);

	outb(port + SR_PCR, inb(port + SR_PCR) | SR_PCR_MEM_WIN);

	smem = (u_short *)id->id_maddr;

	for(i=0;i<=SR_PSR_PG_SEL;i++) {
		outb(port + SR_PSR,
			(inb(port + SR_PSR) & ~SR_PSR_PG_SEL) | i);
		*smem = 0xAA55;
	}

	for(i=0;i<=SR_PSR_PG_SEL;i++) {
		outb(port + SR_PSR,
			(inb(port + SR_PSR) & ~SR_PSR_PG_SEL) | i);
		if(*smem != 0xAA55) {
			/*
			 * If we have less than 64k of memory, give up.
			 * That is 4 x 16k pages.
			 */
			if(i<4) {
				printf("sr%d: Bad mem page %d, mem %x, %x.\n",
					id->id_unit,
					i,
					0xAA55,
					*smem);
				return 0;
			}
			break;
		}
		*smem = i;
	}

	hc->memsize = i * SRC_WIN_SIZ;
	pgs = i;
	for(i=0;i<=pgs;i++) {
		outb(port + SR_PSR,
			(inb(port + SR_PSR) & ~SR_PSR_PG_SEL) | i);
		bzero(smem, SRC_WIN_SIZ);
	}

	/*
	 * Now see if we can see the SCA.
	 */
	outb(port + SR_PCR, SR_PCR_SCARUN | inb(port + SR_PCR));
	SRC_PUT8(port, sca->wcrl, 0);
	SRC_PUT8(port, sca->wcrm, 0);
	SRC_PUT8(port, sca->wcrh, 0);
	SRC_PUT8(port, sca->pcr, 0);

	SRC_PUT8(port, sca->msci[0].tmc, 0);
	inb(port);
	tmp = SRC_GET8(port, sca->msci[0].tmc);
	if(tmp != 0) {
		printf("sr%d: Error reading SCA 0, %x\n", id->id_unit, tmp);
		return 0;
	}
	SRC_PUT8(port, sca->msci[0].tmc, 0x5A);
	inb(port);
	tmp = SRC_GET8(port, sca->msci[0].tmc);
	if(tmp != 0x5A) {
		printf("sr%d: Error reading SCA 0x5A, %x\n", id->id_unit, tmp);
		return 0;
	}

	SRC_PUT16(port, sca->dmac[0].cda, 0);
	inb(port);
	tmp = SRC_GET16(port, sca->dmac[0].cda);
	if(tmp != 0) {
		printf("sr%d: Error reading SCA 0, %x\n", id->id_unit, tmp);
		return 0;
	}
	SRC_PUT16(port, sca->dmac[0].cda, 0x55AA);
	inb(port);
	tmp = SRC_GET16(port, sca->dmac[0].cda);
	if(tmp != 0x55AA) {
		printf("sr%d: Error reading SCA 0x55AA, %x\n", id->id_unit, tmp);
		return 0;
	}

	SRC_SET_OFF(port);

	/*
	 * We have a card here, fill in what we can.
	 */

	id->id_msize = SRC_WIN_SIZ;

	hc->iobase = id->id_iobase;
	hc->mem_start = id->id_maddr;
	hc->mem_end = id->id_maddr + id->id_msize;
	hc->cunit = id->id_unit;

	if(id->id_unit == 0)
		hc->startunit = 0;
	else
		hc->startunit = sr_hardc[id->id_unit - 1].startunit +
				sr_hardc[id->id_unit - 1].numports;

	/*
	 * Do a little sanity check.
	 */
	if(sr_irqtable[ffs(id->id_irq) - 1] == 0)
		printf("sr%d: Warning illegal interrupt %d\n",
			id->id_unit, ffs(id->id_irq) - 1);
	if((hc->numports > NCHAN) || (hc->memsize > (512*1024)))
		return 0;

	return SRC_IO_SIZ;      /* return the amount of IO addresses used. */
}


/*
 * Malloc memory for the softc structures.
 * Reset the card to put it in a known state.
 * Register the ports on the adapter.
 * Fill in the info for each port.
 * Attach each port to sppp and bpf.
 */
static int
srattach(struct isa_device *id)
{
	struct sr_hardc *hc = &sr_hardc[id->id_unit];
	struct sr_softc *sc;
	struct ifnet *ifp;
	int unit;

	printf("src%d: %uK RAM, %u ports.\n",
		id->id_unit,
		hc->memsize/1024,
		hc->numports);
	
	hc->kdc.kdc_state = DC_BUSY;

	src_init(id);

	sc = hc->sc;

	sr_init_sca(hc);

	/*
	 * Now configure each port on the card.
	 */
	for(unit=0;unit<hc->numports;sc++,unit++) {
		sc->hc = hc;
		sc->subunit = unit;
		sc->unit = hc->startunit + unit;
		sc->scachan = unit%NCHAN;

		sr_registerdev(id->id_unit, unit);

		sr_init_rx_dmac(sc);
		sr_init_tx_dmac(sc);
		sr_init_msci(sc);

		ifp = &sc->ifsppp.pp_if;

		ifp->if_softc = sc;
		ifp->if_unit = hc->startunit + unit;
		ifp->if_name = "sr";
		ifp->if_mtu = PP_MTU;
		ifp->if_flags = IFF_POINTOPOINT | IFF_MULTICAST;
		ifp->if_ioctl = srioctl;
		ifp->if_start = srstart;
		ifp->if_watchdog = srwatchdog;

		sc->ifsppp.pp_flags = PP_KEEPALIVE;

		sc->kdc.kdc_state = DC_IDLE;

		printf("sr%d: Adapter %d, port %d.\n",
			sc->unit,
			hc->cunit,
			sc->subunit);

		sppp_attach((struct ifnet *)&sc->ifsppp);
		if_attach(ifp);

#if NBPFILTER > 0
		bpfattach(ifp, DLT_PPP, PPP_HEADER_LEN);
#endif
	}

	SRC_SET_OFF(hc->iobase);

	return 1;
}

/*
 * First figure out which SCA gave the interrupt.
 * Process it.
 * See if there is other interrupts pending.
 * Repeat until there is no more interrupts.
 */
void
srintr(int unit)
{
	struct sr_hardc *hc = &sr_hardc[unit];
	sca_regs *sca = hc->sca;
	u_char isr0, isr1, isr2;

	/*
	 * Loop until no outstanding interrupts.
	 */
	isr0 = SRC_GET8(hc->iobase, sca->isr0);
	isr1 = SRC_GET8(hc->iobase, sca->isr1);
	isr2 = SRC_GET8(hc->iobase, sca->isr2);

	do {
		/*
		 * Acknoledge all the interrupts pending.
		 *
		 * XXX Does this really work?
		 */
		SRC_PUT8(hc->iobase, sca->isr0, isr0);
		SRC_PUT8(hc->iobase, sca->isr1, isr1);
		SRC_PUT8(hc->iobase, sca->isr2, isr2);

		TRC(printf("src%d: SRINTR isr0 %x, isr1 %x, isr2 %x\n",
			unit,
			isr0,
			isr1,
			isr2));

		if(isr0)
			sr_msci_intr(hc, isr0);

		if(isr1)
			sr_dmac_intr(hc, isr1);

		if(isr2)
			sr_timer_intr(hc, isr2);

		isr0 = SRC_GET8(hc->iobase, sca->isr0);
		isr1 = SRC_GET8(hc->iobase, sca->isr1);
		isr2 = SRC_GET8(hc->iobase, sca->isr2);
	} while (isr0 | isr1 | isr2);
}


/*
 * This will only start the transmitter. It is assumed that the data
 * is already there. It is normally called from srstart() or sr_dmac_intr().
 *
 */
static void
sr_xmit(struct sr_softc *sc)
{
	struct ifnet *ifp = &sc->ifsppp.pp_if;
	dmac_channel *dmac = &sc->hc->sca->dmac[DMAC_TXCH(sc->scachan)];

	SRC_PUT16(sc->hc->iobase, dmac->cda,
		  (u_short)(sc->block[sc->txb_next_tx].txdesc & 0xffff));

	SRC_PUT16(sc->hc->iobase, dmac->eda,
		  (u_short)(sc->block[sc->txb_next_tx].txeda & 0xffff));
	SRC_PUT8(sc->hc->iobase, dmac->dsr, SCA_DSR_DE);

	sc->xmit_busy = 1;

	sc->txb_next_tx++;
	if(sc->txb_next_tx == SR_TX_BLOCKS)
		sc->txb_next_tx = 0;

	ifp->if_timer = 2; /* Value in seconds. */
}

/*
 * This function will be called from the upper level when a user add a
 * packet to be send, and from the interrupt handler after a finished
 * transmit.
 *
 * NOTE: it should run at spl_imp().
 *
 * This function only place the data in the oncard buffers. It does not
 * start the transmition. sr_xmit() does that.
 *
 * Transmitter idle state is indicated by the IFF_OACTIVE flag. The function
 * that clears that should ensure that the transmitter and it's DMA is
 * in a "good" idle state.
 */
static void
srstart(struct ifnet *ifp)
{
	struct sr_softc *sc = ifp->if_softc;
	int i, len, tlen;
	struct mbuf *mtx;
	u_char *txdata;
	sca_descriptor *txdesc;
	struct buf_block *blkp;

	if(!(ifp->if_flags & IFF_RUNNING))
		return;
  
	/*
	 * It is OK to set the memory window outside the loop because
	 * all tx buffers and descriptors are assumed to be in the same
	 * 16K window.
	 */
	SRC_SET_ON(sc->hc->iobase);
	SRC_SET_MEM(sc->hc->iobase, sc->block[0].txdesc);

top_srstart:

	/*
	 * See if we have space for more packets.
	 */
	if(sc->txb_inuse == SR_TX_BLOCKS) {
		ifp->if_flags |= IFF_OACTIVE;
		SRC_SET_OFF(sc->hc->iobase);
		return;
	}

	mtx = sppp_dequeue(ifp);
	if(!mtx) {
		SRC_SET_OFF(sc->hc->iobase);
		return;
	}

	/*
	 * We stay in this loop until there is nothing in the
	 * TX queue left or the tx buffers are full.
	 */
	i = 0;
	blkp = &sc->block[sc->txb_new];
	txdesc = (sca_descriptor *)
		(sc->hc->mem_start + (blkp->txdesc & SRC_WIN_MSK));
	txdata = (u_char *)(sc->hc->mem_start + (blkp->txstart & SRC_WIN_MSK));
	for(;;) {
		len = mtx->m_pkthdr.len;

		TRC(printf("sr%d: SRstart len %u\n", sc->unit, len));

		/*
		 * We can do this because the tx buffers don't wrap.
		 */
		m_copydata(mtx, 0, len, txdata);
		tlen = len;
		while(tlen > SR_BUF_SIZ) {
			txdesc->stat = 0;
			txdesc->len = SR_BUF_SIZ;
			tlen -= SR_BUF_SIZ;
			txdesc++;
			txdata += SR_BUF_SIZ;
			i++;
		}
		/* XXX Move into the loop? */
		txdesc->stat = SCA_DESC_EOM;
		txdesc->len = tlen;
		txdesc++;
		txdata += SR_BUF_SIZ;
		i++;

#if NBPFILTER > 0
		if(ifp->if_bpf)
			bpf_mtap(ifp, mtx);
#endif
		m_freem(mtx);
		++sc->ifsppp.pp_if.if_opackets;

		/*
		 * Check if we have space for another mbuf.
		 * XXX This is hardcoded. A packet won't be larger
		 * than 3 buffers (3 x 512).
		 */
		if((i + 3) >= blkp->txmax)
			break;

		mtx = sppp_dequeue(ifp);
		if(!mtx)
			break;
	}

	blkp->txtail = i;

	/*
	 * Mark the last descriptor, so that the SCA know where
	 * to stop.
	 */
	txdesc--;
	txdesc->stat |= SCA_DESC_EOT;

	txdesc = (sca_descriptor *)blkp->txdesc;
	blkp->txeda = (u_short)((u_int)&txdesc[i]);

	sc->txb_inuse++;
	sc->txb_new++;
	if(sc->txb_new == SR_TX_BLOCKS)
		sc->txb_new = 0;

	if(sc->xmit_busy == 0)
		sr_xmit(sc);

	goto top_srstart;
}

static int
srioctl(struct ifnet *ifp, int cmd, caddr_t data)
{
	int s, error;
	int was_up, should_be_up;
	struct sppp *sp = (struct sppp *)ifp;
	struct sr_softc *sc = ifp->if_softc;

	TRC(printf("sr%d: srioctl.\n", ifp->if_unit);)

	if(cmd == SIOCSIFFLAGS) {
		if(ifp->if_flags & IFF_LINK2)
			sp->pp_flags |= PP_CISCO;
		else
			sp->pp_flags &= ~PP_CISCO;
	}

	was_up = ifp->if_flags & IFF_RUNNING;

	error = sppp_ioctl(ifp, cmd, data);
	TRC(printf("sr%d: ioctl: ifsppp.pp_flags = %x, if_flags %x.\n", 
		ifp->if_unit, ((struct sppp *)ifp)->pp_flags, ifp->if_flags);)
	if(error)
		return error;

	if((cmd != SIOCSIFFLAGS) && cmd != (SIOCSIFADDR))
		return 0;

	TRC(printf("sr%d: srioctl %s.\n", ifp->if_unit, 
		(cmd == SIOCSIFFLAGS) ? "SIOCSIFFLAGS" : "SIOCSIFADDR");)

	s = splimp();
	should_be_up = ifp->if_flags & IFF_RUNNING;

	if(!was_up && should_be_up) {
		/* Interface should be up -- start it. */
		sr_up(sc);
		srstart(ifp);
		/* XXX Clear the IFF_UP flag so that the link
		 * will only go up after sppp lcp and ipcp negotiation.
		 */
		ifp->if_flags &= ~IFF_UP;
	} else if(was_up && !should_be_up) {
		/* Interface should be down -- stop it. */
		sr_down(sc);
		sppp_flush(ifp);
	}
	splx(s);
	return 0;
}

/*
 * This is to catch lost tx interrupts.
 */
static void
srwatchdog(struct ifnet *ifp)
{
	struct sr_softc *sc = ifp->if_softc;
	msci_channel *msci = &sc->hc->sca->msci[sc->scachan];

	if(!(ifp->if_flags & IFF_RUNNING))
		return;

	/* XXX if(sc->ifsppp.pp_if.if_flags & IFF_DEBUG) */
		printf("sr%d: transmit failed, "
			"ST0 %x, ST1 %x, ST3 %x, DSR %x.\n",
			ifp->if_unit,
			SRC_GET8(sc->hc->iobase, msci->st0),
			SRC_GET8(sc->hc->iobase, msci->st1),
			SRC_GET8(sc->hc->iobase, msci->st3),
			SRC_GET8(sc->hc->iobase, 
				sc->hc->sca->dmac[DMAC_TXCH(sc->scachan)].dsr));

	if(SRC_GET8(sc->hc->iobase, msci->st1) & SCA_ST1_UDRN) {
		SRC_PUT8(sc->hc->iobase, msci->cmd, SCA_CMD_TXABORT);
		SRC_PUT8(sc->hc->iobase, msci->cmd, SCA_CMD_TXENABLE);
		SRC_PUT8(sc->hc->iobase, msci->st1, SCA_ST1_UDRN);
	}

	sc->xmit_busy = 0;
	ifp->if_flags &= ~IFF_OACTIVE;

	if(sc->txb_inuse && --sc->txb_inuse)
		sr_xmit(sc);

	srstart(ifp);
}

static void
sr_up(struct sr_softc *sc)
{
	sca_regs *sca = sc->hc->sca;
	msci_channel *msci = &sca->msci[sc->scachan];

	sc->kdc.kdc_state = DC_BUSY;

	/*
	 * Enable transmitter and receiver.
	 * Raise DTR and RTS.
	 * Enable interrupts.
	 */

	/* XXX
	 * What about using AUTO mode in msci->md0 ???
	 */
	SRC_PUT8(sc->hc->iobase, msci->ctl,
		SRC_GET8(sc->hc->iobase, msci->ctl) & ~SCA_CTL_RTS);

	if(sc->scachan == 0)
		outb(sc->hc->iobase + SR_MCR,
			inb(sc->hc->iobase + SR_MCR) & ~SR_MCR_DTR0);
	else
		outb(sc->hc->iobase + SR_MCR,
			inb(sc->hc->iobase + SR_MCR) & ~SR_MCR_DTR1);

	if(sc->scachan == 0) {
		SRC_PUT8(sc->hc->iobase, sca->ier0,
			SRC_GET8(sc->hc->iobase, sca->ier0) | 0x0F);
		SRC_PUT8(sc->hc->iobase, sca->ier1,
			SRC_GET8(sc->hc->iobase, sca->ier1) | 0x0F);
	} else {
		SRC_PUT8(sc->hc->iobase, sca->ier0,
			SRC_GET8(sc->hc->iobase, sca->ier0) | 0xF0);
		SRC_PUT8(sc->hc->iobase, sca->ier1,
			SRC_GET8(sc->hc->iobase, sca->ier1) | 0xF0);
	}

	SRC_PUT8(sc->hc->iobase, msci->cmd, SCA_CMD_RXENABLE);
	inb(sc->hc->iobase); /* XXX slow it down a bit. */
	SRC_PUT8(sc->hc->iobase, msci->cmd, SCA_CMD_TXENABLE);
}

static void
sr_down(struct sr_softc *sc)
{
	sca_regs *sca = sc->hc->sca;
	msci_channel *msci = &sca->msci[sc->scachan];

	sc->kdc.kdc_state = DC_IDLE;

	/*
	 * Disable transmitter and receiver.
	 * Lower DTR and RTS.
	 * Disable interrupts.
	 */
	SRC_PUT8(sc->hc->iobase, msci->cmd, SCA_CMD_RXDISABLE);
	inb(sc->hc->iobase); /* XXX slow it down a bit. */
	SRC_PUT8(sc->hc->iobase, msci->cmd, SCA_CMD_TXDISABLE);

	SRC_PUT8(sc->hc->iobase, msci->ctl,
		SRC_GET8(sc->hc->iobase, msci->ctl) | SCA_CTL_RTS);

	if(sc->scachan == 0)
		outb(sc->hc->iobase + SR_MCR,
			inb(sc->hc->iobase + SR_MCR) | SR_MCR_DTR0);
	else
		outb(sc->hc->iobase + SR_MCR,
			inb(sc->hc->iobase + SR_MCR) | SR_MCR_DTR1);

	if(sc->scachan == 0) {
		SRC_PUT8(sc->hc->iobase, sca->ier0,
			SRC_GET8(sc->hc->iobase, sca->ier0) & ~0x0F);
		SRC_PUT8(sc->hc->iobase, sca->ier1,
			SRC_GET8(sc->hc->iobase, sca->ier1) & ~0x0F);
	} else {
		SRC_PUT8(sc->hc->iobase, sca->ier0,
			SRC_GET8(sc->hc->iobase, sca->ier0) & ~0xF0);
		SRC_PUT8(sc->hc->iobase, sca->ier1,
			SRC_GET8(sc->hc->iobase, sca->ier1) & ~0xF0);
	}
}

/*
 * Initialize the card, allocate memory for the sr_softc structures
 * and fill in the pointers.
 */
static void
src_init(struct isa_device *id)
{
	struct sr_hardc *hc = &sr_hardc[id->id_unit];
	struct sr_softc *sc;
	int x;
	u_int chanmem;
	u_int bufmem;
	u_int next;
	u_int descneeded;
	u_char mar;

	sc = hc->sc = malloc(hc->numports * sizeof(struct sr_softc),
				M_DEVBUF, M_WAITOK);
	bzero(sc, hc->numports * sizeof(struct sr_softc));

	outb(hc->iobase + SR_PCR, inb(hc->iobase + SR_PCR) | SR_PCR_SCARUN);
	outb(hc->iobase + SR_PSR, inb(hc->iobase + SR_PSR) | SR_PSR_EN_SCA_DMA);
	outb(hc->iobase + SR_MCR, SR_MCR_DTR0 |
				  SR_MCR_DTR1 |
				  SR_MCR_TE0  |
				  SR_MCR_TE1);
	SRC_SET_ON(hc->iobase);

	/*
	 * Configure the card.
	 * Mem address, irq, 
	 */
	mar = (kvtop(id->id_maddr) >> 16) & SR_PCR_16M_SEL;
	outb(hc->iobase + SR_PCR,
		mar | (inb(hc->iobase + SR_PCR) & ~SR_PCR_16M_SEL));
	mar = kvtop(id->id_maddr) >> 12;
	outb(hc->iobase + SR_BAR, mar);

	/*
	 * Get the TX clock direction and configuration.
	 * The default is a single external clock which is
	 * used by RX and TX.
	 */
	if(id->id_flags & SR_FLAGS_0_CLK_MSK)
		sc[0].clk_cfg = (id->id_flags & SR_FLAGS_0_CLK_MSK) >>
				   SR_FLAGS_CLK_SHFT;
	if((hc->numports == 2) && (id->id_flags & SR_FLAGS_1_CLK_MSK))
		sc[1].clk_cfg = (id->id_flags & SR_FLAGS_1_CLK_MSK) >>
				  (SR_FLAGS_CLK_SHFT + SR_FLAGS_CLK_CHAN_SHFT);

	chanmem = hc->memsize / hc->numports;
	next = 0;

	for(x=0;x<hc->numports;x++, sc++) {
		int blk;

		for(blk = 0; blk < SR_TX_BLOCKS; blk++) {
			sc->block[blk].txdesc = next;
			bufmem = (16 * 1024) / SR_TX_BLOCKS;
			descneeded = bufmem / SR_BUF_SIZ;
			sc->block[blk].txstart = sc->block[blk].txdesc +
				((((descneeded * sizeof(sca_descriptor)) /
					SR_BUF_SIZ) + 1) * SR_BUF_SIZ);
			sc->block[blk].txend = next + bufmem;
			sc->block[blk].txmax =
				(sc->block[blk].txend - sc->block[blk].txstart)
				/ SR_BUF_SIZ;
			next += bufmem;

			TRC(printf("sr%d: blk %d: txdesc %x, txstart %x, "
				   "txend %x, txmax %d\n",
				   x,
				   blk,
				   sc->block[blk].txdesc,
				   sc->block[blk].txstart,
				   sc->block[blk].txend,
				   sc->block[blk].txmax));
		}

		sc->rxdesc = next;
		bufmem = chanmem - (bufmem * SR_TX_BLOCKS);
		descneeded = bufmem / SR_BUF_SIZ;
		sc->rxstart = sc->rxdesc +
				((((descneeded * sizeof(sca_descriptor)) /
					SR_BUF_SIZ) + 1) * SR_BUF_SIZ);
		sc->rxend = next + bufmem;
		sc->rxmax = (sc->rxend - sc->rxstart) / SR_BUF_SIZ;
		next += bufmem;
	}
}


/*
 * The things done here are channel independent.
 *
 *   Configure the sca waitstates.
 *   Configure the global interrupt registers.
 *   Enable master dma enable.
 */
static void
sr_init_sca(struct sr_hardc *hc)
{
	sca_regs *sca = hc->sca;

	/*
	 * Do the wait registers.
	 * Set everything to 0 wait states.
	 */
	SRC_PUT8(hc->iobase, sca->pabr0, 0);
	SRC_PUT8(hc->iobase, sca->pabr1, 0);
	SRC_PUT8(hc->iobase, sca->wcrl, 0);
	SRC_PUT8(hc->iobase, sca->wcrm, 0);
	SRC_PUT8(hc->iobase, sca->wcrh, 0);

	/*
	 * Configure the interrupt registers.
	 * Most are cleared until the interface is configured.
	 */
	SRC_PUT8(hc->iobase, sca->ier0, 0x00); /* MSCI interrupts. */
	SRC_PUT8(hc->iobase, sca->ier1, 0x00); /* DMAC interrupts */
	SRC_PUT8(hc->iobase, sca->ier2, 0x00); /* TIMER interrupts. */
	SRC_PUT8(hc->iobase, sca->itcr, 0x00); /* Use ivr and no intr ack */
	SRC_PUT8(hc->iobase, sca->ivr, 0x40); /* Interrupt vector. */
	SRC_PUT8(hc->iobase, sca->imvr, 0x40);

	/*
	 * Configure the timers.
	 * XXX Later
	 */


	/*
	 * Set the DMA channel priority to rotate between
	 * all four channels.
	 *
	 * Enable all dma channels.
	 */
	SRC_PUT8(hc->iobase, sca->pcr, SCA_PCR_PR2);
	SRC_PUT8(hc->iobase, sca->dmer, SCA_DMER_EN);
}


/*
 * Configure the msci
 *
 * NOTE: The serial port configuration is hardcoded at the moment.
 */
static void
sr_init_msci(struct sr_softc *sc)
{
	msci_channel *msci = &sc->hc->sca->msci[sc->scachan];
	u_short iobase = sc->hc->iobase;

	SRC_PUT8(iobase, msci->cmd, SCA_CMD_RESET);

	SRC_PUT8(iobase, msci->md0, SCA_MD0_CRC_1 |
				    SCA_MD0_CRC_CCITT |
				    SCA_MD0_CRC_ENABLE |
				    SCA_MD0_MODE_HDLC);
	SRC_PUT8(iobase, msci->md1, SCA_MD1_NOADDRCHK);
	SRC_PUT8(iobase, msci->md2, SCA_MD2_DUPLEX | SCA_MD2_NRZ);

	/*
	 * According to the manual I should give a reset after changing the
	 * mode registers.
	 */
	SRC_PUT8(iobase, msci->cmd, SCA_CMD_RXRESET);
	SRC_PUT8(iobase, msci->ctl, SCA_CTL_IDLPAT |
				    SCA_CTL_UDRNC |
				    SCA_CTL_RTS);

	/*
	 * XXX Later we will have to support different clock settings.
	 */
	switch(sc->clk_cfg) {
	default:
	case SR_FLAGS_EXT_CLK:
		/*
		 * For now all interfaces are programmed to use the
		 * RX clock for the TX clock.
		 */
		SRC_PUT8(iobase, msci->rxs, SCA_RXS_CLK_RXC0 | SCA_RXS_DIV1);
		SRC_PUT8(iobase, msci->txs, SCA_TXS_CLK_RX | SCA_TXS_DIV1);
		break;
	case SR_FLAGS_EXT_SEP_CLK:
		SRC_PUT8(iobase, msci->rxs, SCA_RXS_CLK_RXC0 | SCA_RXS_DIV1);
		SRC_PUT8(iobase, msci->txs, SCA_TXS_CLK_TXC | SCA_TXS_DIV1);
		break;
	case SR_FLAGS_INT_CLK:
		/*
		 * XXX I do need some code to set the baud rate here!
		 */
		SRC_PUT8(iobase, msci->rxs, SCA_RXS_CLK_INT | SCA_RXS_DIV1);
		SRC_PUT8(iobase, msci->txs, SCA_TXS_CLK_INT | SCA_TXS_DIV1);

		SRC_PUT8(iobase, msci->tmc, 153);

		if(sc->scachan == 0)
			outb(iobase + SR_MCR, inb(iobase + SR_MCR) |
						  SR_MCR_ETC0);
		else
			outb(iobase + SR_MCR, inb(iobase + SR_MCR) |
						  SR_MCR_ETC1);
	}

	/* XXX
	 * Disable all interrupts for now. I think if you are using
	 * the dmac you don't use these interrupts.
	 */
	SRC_PUT8(iobase, msci->ie0, 0);
	SRC_PUT8(iobase, msci->ie1, 0x0C); /* XXX CTS and DCD level change. */
	SRC_PUT8(iobase, msci->ie2, 0);
	SRC_PUT8(iobase, msci->fie, 0);

	SRC_PUT8(iobase, msci->sa0, 0);
	SRC_PUT8(iobase, msci->sa1, 0);

	SRC_PUT8(iobase, msci->idl, 0x7E); /* XXX This is what cisco does. */

	SRC_PUT8(iobase, msci->rrc, 0x0E);
	SRC_PUT8(iobase, msci->trc0, 0x10);
	SRC_PUT8(iobase, msci->trc1, 0x1F);
}

/*
 * Configure the rx dma controller.
 */
static void
sr_init_rx_dmac(struct sr_softc *sc)
{
	dmac_channel *dmac = &sc->hc->sca->dmac[DMAC_RXCH(sc->scachan)];
	sca_descriptor *rxd;
	u_int rxbuf;
	u_int rxda;
	u_int rxda_d;
	u_short iobase = sc->hc->iobase;

	SRC_SET_MEM(iobase, sc->rxdesc);

	rxd = (sca_descriptor *)(sc->hc->mem_start + (sc->rxdesc&SRC_WIN_MSK));
	rxda_d = (u_int)sc->hc->mem_start - (sc->rxdesc & ~SRC_WIN_MSK);

	for(rxbuf=sc->rxstart;rxbuf<sc->rxend;rxbuf += SR_BUF_SIZ, rxd++) {
		rxda = (u_int)&rxd[1] - rxda_d;
		rxd->cp = (u_short)(rxda & 0xfffful);

		TRC(printf("Descrp %p, data pt %p, data long %lx, ",
			&sc->rxdesc[x], rxinuse->buf, rxbuf));

		rxd->bp = (u_short)(rxbuf & 0xfffful);
		rxd->bpb = (u_char)((rxbuf >> 16) & 0xff);
		rxd->len = 0;
		rxd->stat = 0xff; /* The sca write here when it is finished. */

		TRC(printf("bpb %x, bp %x.\n", rxd->bpb, rxd->bp));
	}
	rxd--;
	rxd->cp = (u_short)(sc->rxdesc & 0xfffful);

	sc->rxhind = 0;

	SRC_PUT8(iobase, dmac->dsr, 0);    /* Disable DMA transfer */
	SRC_PUT8(iobase, dmac->dcr, SCA_DCR_ABRT);

	/* XXX maybe also SCA_DMR_CNTE */
	SRC_PUT8(iobase, dmac->dmr, SCA_DMR_TMOD | SCA_DMR_NF);
	SRC_PUT16(iobase, dmac->bfl, SR_BUF_SIZ);

	SRC_PUT16(iobase, dmac->cda, (u_short)(sc->rxdesc & 0xffff));
	SRC_PUT8(iobase, dmac->sarb, (u_char)((sc->rxdesc >> 16) & 0xff));

	rxd = (sca_descriptor *)sc->rxstart;
	SRC_PUT16(iobase, dmac->eda,
			(u_short)((u_int)&rxd[sc->rxmax - 1] & 0xffff));

	SRC_PUT8(iobase, dmac->dir, 0xF0);

	SRC_PUT8(iobase, dmac->dsr, SCA_DSR_DE);
}

/*
 * Configure the TX DMA descriptors.
 * Initialize the needed values and chain the descriptors.
 */
static void
sr_init_tx_dmac(struct sr_softc *sc)
{
	dmac_channel *dmac = &sc->hc->sca->dmac[DMAC_TXCH(sc->scachan)];
	struct buf_block *blkp;
	int blk;
	sca_descriptor *txd;
	u_int txbuf;
	u_int txda;
	u_int txda_d;
	u_short iobase = sc->hc->iobase;

	SRC_SET_MEM(iobase, sc->block[0].txdesc);

	for(blk = 0; blk < SR_TX_BLOCKS; blk++) {
		blkp = &sc->block[blk];
		txd = (sca_descriptor *)(sc->hc->mem_start +
					(blkp->txdesc & SRC_WIN_MSK));
		txda_d = (u_int)sc->hc->mem_start -
				(blkp->txdesc & ~SRC_WIN_MSK);

		txbuf=blkp->txstart;
		for(;txbuf<blkp->txend;txbuf += SR_BUF_SIZ, txd++) {
			txda = (u_int)&txd[1] - txda_d;
			txd->cp = (u_short)(txda & 0xfffful);

			txd->bp = (u_short)(txbuf & 0xfffful);
			txd->bpb = (u_char)((txbuf >> 16) & 0xff);
			TRC(printf("sr%d: txbuf %x, bpb %x, bp %x\n",
				sc->unit, txbuf, txd->bpb, txd->bp));
			txd->len = 0;
			txd->stat = 0;
		}
		txd--;
		txd->cp = (u_short)(blkp->txdesc & 0xfffful);

		blkp->txtail = (u_int)txd - (u_int)sc->hc->mem_start;
		TRC(printf("TX Descriptors start %x, end %x.\n",
			blkp->txhead,
			blkp->txtail));
	}

	SRC_PUT8(iobase, dmac->dsr, 0); /* Disable DMA */
	SRC_PUT8(iobase, dmac->dcr, SCA_DCR_ABRT);
	SRC_PUT8(iobase, dmac->dmr, SCA_DMR_TMOD | SCA_DMR_NF);
	SRC_PUT8(iobase, dmac->dir, SCA_DIR_EOT | SCA_DIR_BOF | SCA_DIR_COF);

	SRC_PUT8(iobase, dmac->sarb,
			(u_char)((sc->block[0].txdesc >> 16) & 0xff));
}


/*
 * Look through the descriptors to see if there is a complete packet
 * available. Stop if we get to where the sca is busy.
 *
 * Return the length and status of the packet.
 * Return nonzero if there is a packet available.
 *
 * NOTE:
 * It seems that we get the interrupt a bit early. The updateing of
 * descriptor values is not always completed when this is called.
 */
static int
sr_packet_avail(struct sr_softc *sc,
		    int *len,
		    u_char *rxstat)
{
	sca_descriptor *rxdesc;
	sca_descriptor *endp;
	sca_descriptor *cda;

	cda = (sca_descriptor *)(sc->hc->mem_start +
	      (SRC_GET16(sc->hc->iobase, 
			sc->hc->sca->dmac[DMAC_RXCH(sc->scachan)].cda) &
				SRC_WIN_MSK));

	SRC_SET_MEM(sc->hc->iobase, sc->rxdesc);
	rxdesc = (sca_descriptor *)
			(sc->hc->mem_start + (sc->rxdesc & SRC_WIN_MSK));
	endp = rxdesc;
	rxdesc = &rxdesc[sc->rxhind];
	endp = &endp[sc->rxmax];

	*len = 0;

	while(rxdesc != cda) {
		*len += rxdesc->len;

		if(rxdesc->stat & SCA_DESC_EOM) {
			*rxstat = rxdesc->stat;
			TRC(printf("sr%d: PKT AVAIL len %d, %x, bufs %u.\n",
				sc->unit, *len, *rxstat, x));
			return 1;
		}

		rxdesc++;
		if(rxdesc == endp)
			rxdesc = (sca_descriptor *)
			       (sc->hc->mem_start + (sc->rxdesc & SRC_WIN_MSK));
	}

	*len = 0;
	*rxstat = 0;
	return 0;
}


/*
 * Copy a packet from the on card memory into a provided mbuf.
 * Take into account that buffers wrap and that a packet may
 * be larger than a buffer.
 */
static void 
sr_copy_rxbuf(struct mbuf *m,
		   struct sr_softc *sc,
		   int len)
{
	sca_descriptor *rxdesc;
	u_int rxdata;
	u_int rxmax;
	u_int off = 0;
	u_int tlen;

	rxdata = sc->rxstart + (sc->rxhind * SR_BUF_SIZ);
	rxmax = sc->rxstart + (sc->rxmax * SR_BUF_SIZ);

	rxdesc = (sca_descriptor *)
			(sc->hc->mem_start + (sc->rxdesc & SRC_WIN_MSK));
	rxdesc = &rxdesc[sc->rxhind];

	while(len) {
		tlen = (len < SR_BUF_SIZ) ? len : SR_BUF_SIZ;
		SRC_SET_MEM(sc->hc->iobase, rxdata);
		bcopy(sc->hc->mem_start + (rxdata & SRC_WIN_MSK), 
			mtod(m, caddr_t) + off,
			tlen);

		off += tlen;
		len -= tlen;

		SRC_SET_MEM(sc->hc->iobase, sc->rxdesc);
		rxdesc->len = 0;
		rxdesc->stat = 0xff;

		rxdata += SR_BUF_SIZ;
		rxdesc++;
		if(rxdata == rxmax) {
			rxdata = sc->rxstart;
			rxdesc = (sca_descriptor *)
				(sc->hc->mem_start + (sc->rxdesc & SRC_WIN_MSK));
		}
	}
}

/*
 * If single is set, just eat a packet. Otherwise eat everything up to
 * where cda points. Update pointers to point to the next packet.
 */
static void
sr_eat_packet(struct sr_softc *sc, int single)
{
	sca_descriptor *rxdesc;
	sca_descriptor *endp;
	sca_descriptor *cda;
	int loopcnt = 0;
	u_char stat;

	cda = (sca_descriptor *)(sc->hc->mem_start +
	      (SRC_GET16(sc->hc->iobase,
			sc->hc->sca->dmac[DMAC_RXCH(sc->scachan)].cda) &
				SRC_WIN_MSK));

	/*
	 * Loop until desc->stat == (0xff || EOM)
	 * Clear the status and length in the descriptor.
	 * Increment the descriptor.
	 */
	SRC_SET_MEM(sc->hc->iobase, sc->rxdesc);
	rxdesc = (sca_descriptor *)
		(sc->hc->mem_start + (sc->rxdesc & SRC_WIN_MSK));
	endp = rxdesc;
	rxdesc = &rxdesc[sc->rxhind];
	endp = &endp[sc->rxmax];

	while(rxdesc != cda) {
		loopcnt++;
		if(loopcnt > sc->rxmax) {
			printf("sr%d: eat pkt %d loop, cda %x, "
			       "rxdesc %x, stat %x.\n",
			       sc->unit,
			       loopcnt,
			       cda,
			       rxdesc,
			       rxdesc->stat);
			break;
		}

		stat = rxdesc->stat;

		rxdesc->len = 0;
		rxdesc->stat = 0xff;

		rxdesc++;
		sc->rxhind++;
		if(rxdesc == endp) {
			rxdesc = (sca_descriptor *)
			       (sc->hc->mem_start + (sc->rxdesc & SRC_WIN_MSK));
			sc->rxhind = 0;
		}

		if(single && (stat == SCA_DESC_EOM))
			break;
	}

	/*
	 * Update the eda to the previous descriptor.
	 */
	rxdesc = (sca_descriptor *)sc->rxdesc;
	rxdesc = &rxdesc[(sc->rxhind + sc->rxmax - 2 ) % sc->rxmax];

	SRC_PUT16(sc->hc->iobase,
		  sc->hc->sca->dmac[DMAC_RXCH(sc->scachan)].eda,
		  (u_short)((u_int)rxdesc & 0xffff));
}


/*
 * While there is packets available in the rx buffer, read them out
 * into mbufs and ship them off.
 */
static void
sr_get_packets(struct sr_softc *sc)
{
	sca_descriptor *rxdesc;
	struct mbuf *m = NULL;
	int i;
	int len;
	u_char rxstat;

	SRC_SET_ON(sc->hc->iobase);

	while(sr_packet_avail(sc, &len, &rxstat)) {
		if((rxstat & SCA_DESC_ERRORS) == 0) {
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if(m == NULL) {
				/* eat packet if get mbuf fail!! */
				sr_eat_packet(sc, 1);
				continue;
			}
			m->m_pkthdr.rcvif = &sc->ifsppp.pp_if;
			m->m_pkthdr.len = m->m_len = len;
			if(len > MHLEN) {
				MCLGET(m, M_DONTWAIT);
				if((m->m_flags & M_EXT) == 0) {
					m_freem(m);
					sr_eat_packet(sc, 1);
					continue;
				}
			}
			sr_copy_rxbuf(m, sc, len);
#if NBPFILTER > 0
			if(sc->ifsppp.pp_if.if_bpf)
				bpf_mtap(&sc->ifsppp.pp_if, m);
#endif
			sppp_input(&sc->ifsppp.pp_if, m);
			sc->ifsppp.pp_if.if_ipackets++;

			/*
			 * Update the eda to the previous descriptor.
			 */
			i = (len + SR_BUF_SIZ - 1) / SR_BUF_SIZ;
			sc->rxhind = (sc->rxhind + i) % sc->rxmax;

			rxdesc = (sca_descriptor *)sc->rxdesc;
			rxdesc =
			     &rxdesc[(sc->rxhind + sc->rxmax - 2 ) % sc->rxmax];

			SRC_PUT16(sc->hc->iobase,
				  sc->hc->sca->dmac[DMAC_RXCH(sc->scachan)].eda,
				  (u_short)((u_int)rxdesc & 0xffff));
		} else {
			msci_channel *msci = &sc->hc->sca->msci[sc->scachan];

			sr_eat_packet(sc, 1);

			sc->ifsppp.pp_if.if_ierrors++;

			TRCL(printf("sr%d: Receive error chan %d, "
				    "stat %x, msci st3 %x,"
				    "rxhind %d, cda %x, eda %x.\n",
				    sc->unit,
				    sc->scachan, 
				    rxstat,
				    SRC_GET8(sc->hc->iobase, msci->st3),
					     sc->rxhind,
				    SRC_GET16(sc->hc->iobase,
					      sc->hc->sca->dmac[
					      DMAC_RXCH(sc->scachan)].cda),
				    SRC_GET16(sc->hc->iobase,
					      sc->hc->sca->dmac[
					      DMAC_RXCH(sc->scachan)].eda)));
		}
	}

	SRC_SET_OFF(sc->hc->iobase);
}


/*
 * All DMA interrupts come here.
 *
 * Each channel has two interrupts.
 * Interrupt A for errors and Interrupt B for normal stuff like end
 * of transmit or receive dmas.
 */
static void
sr_dmac_intr(struct sr_hardc *hc, u_char isr1)
{
	u_char dsr;
	u_char dotxstart = isr1;
	int mch;
	struct sr_softc *sc;
	sca_regs *sca = hc->sca;
	dmac_channel *dmac;

	mch = 0;
	/*
	 * Shortcut if there is no interrupts for dma channel 0 or 1
	 */
	if((isr1 & 0x0F) == 0) {
		mch = 1;
		isr1 >>= 4;
	}

	do {
		sc = &hc->sc[mch];

		/*
		 * Transmit channel
		 */
		if(isr1 & 0x0C) {
			dmac = &sca->dmac[DMAC_TXCH(mch)];

			dsr = SRC_GET8(hc->iobase, dmac->dsr);
			SRC_PUT8(hc->iobase, dmac->dsr, dsr);

			/* Counter overflow */
			if(dsr & SCA_DSR_COF) {
				printf("sr%d: TX DMA Counter overflow, "
					"txpacket no %lu.\n",
					sc->unit,
					sc->ifsppp.pp_if.if_opackets);
				sc->ifsppp.pp_if.if_oerrors++;
			}

			/* Buffer overflow */
			if(dsr & SCA_DSR_BOF) {
				printf("sr%d: TX DMA Buffer overflow, "
					"txpacket no %lu, dsr %02x, "
					"cda %04x, eda %04x.\n",
					sc->unit,
					sc->ifsppp.pp_if.if_opackets,
					dsr,
					SRC_GET16(hc->iobase, dmac->cda),
					SRC_GET16(hc->iobase, dmac->eda));
				sc->ifsppp.pp_if.if_oerrors++;
			}

			/* End of Transfer */
			if(dsr & SCA_DSR_EOT) {
				/*
				 * This should be the most common case.
				 *
				 * Clear the IFF_OACTIVE flag.
				 *
				 * Call srstart to start a new transmit if
				 * there is data to transmit.
				 */
				sc->xmit_busy = 0;
				sc->ifsppp.pp_if.if_flags &= ~IFF_OACTIVE;
				sc->ifsppp.pp_if.if_timer = 0;

				if(sc->txb_inuse && --sc->txb_inuse)
					sr_xmit(sc);
			}
		}

		/*
		 * Receive channel
		 */
		if(isr1 & 0x03) {
			dmac = &sca->dmac[DMAC_RXCH(mch)];

			dsr = SRC_GET8(hc->iobase, dmac->dsr);
			SRC_PUT8(hc->iobase, dmac->dsr, dsr);

			TRC(printf("SR: RX DSR %x\n", dsr));

			/* End of frame */
			if(dsr & SCA_DSR_EOM) {
				TRC(int tt = sc->ifsppp.pp_if.if_ipackets;)
				TRC(int ind = sc->rxhind;)

				sr_get_packets(sc);
				TRC(
				if(tt == sc->ifsppp.pp_if.if_ipackets) {
					sca_descriptor *rxdesc;
					int i;

					printf("SR: RXINTR isr1 %x, dsr %x, "
					       "no data %d pkts, orxhind %d.\n",
					       dotxstart,
					       dsr,
					       tt,
					       ind);
					printf("SR: rxdesc %x, rxstart %x, "
					       "rxend %x, rxhind %d, "
					       "rxmax %d.\n",
					       sc->rxdesc,
					       sc->rxstart,
					       sc->rxend,
					       sc->rxhind,
					       sc->rxmax);
					printf("SR: cda %x, eda %x.\n",
					       SRC_GET16(hc->iobase, dmac->cda),
					       SRC_GET16(hc->iobase, dmac->eda));

					SRC_SET_ON(hc->iobase);
					SRC_SET_MEM(hc->iobase, sc->rxdesc);
					rxdesc = (sca_descriptor *)
						 (hc->mem_start +
						  (sc->rxdesc & SRC_WIN_MSK));
					rxdesc = &rxdesc[sc->rxhind];
					for(i=0;i<3;i++,rxdesc++)
						printf("SR: rxdesc->stat %x, "
							"len %d.\n",
							rxdesc->stat,
							rxdesc->len);
					SRC_SET_OFF(hc->iobase);
				})
			}

			/* Counter overflow */
			if(dsr & SCA_DSR_COF) {
				printf("sr%d: RX DMA Counter overflow, "
					"rxpkts %lu.\n",
					sc->unit,
					sc->ifsppp.pp_if.if_ipackets);
				sc->ifsppp.pp_if.if_ierrors++;
			}

			/* Buffer overflow */
			if(dsr & SCA_DSR_BOF) {
				printf("sr%d: RX DMA Buffer overflow, "
					"rxpkts %lu, rxind %d, "
					"cda %x, eda %x, dsr %x.\n",
					sc->unit,
					sc->ifsppp.pp_if.if_ipackets,
					sc->rxhind,
					SRC_GET16(hc->iobase, dmac->cda),
					SRC_GET16(hc->iobase, dmac->eda),
					dsr);
				/*
				 * Make sure we eat as many as possible.
				 * Then get the system running again.
				 */
				SRC_SET_ON(sc->hc->iobase);
				sr_eat_packet(sc, 0);
				sc->ifsppp.pp_if.if_ierrors++;
				SRC_PUT8(hc->iobase,
					 sca->msci[mch].cmd,
					 SCA_CMD_RXMSGREJ);
				SRC_PUT8(hc->iobase, dmac->dsr, SCA_DSR_DE);

				TRC(printf("sr%d: RX DMA Buffer overflow, "
					"rxpkts %lu, rxind %d, "
					"cda %x, eda %x, dsr %x. After\n",
					sc->unit,
					sc->ifsppp.pp_if.if_ipackets,
					sc->rxhind,
					SRC_GET16(hc->iobase, dmac->cda),
					SRC_GET16(hc->iobase, dmac->eda),
					SRC_GET8(hc->iobase, dmac->dsr));)
				SRC_SET_OFF(sc->hc->iobase);
			}

			/* End of Transfer */
			if(dsr & SCA_DSR_EOT) {
				/*
				 * If this happen, it means that we are
				 * receiving faster than what the processor
				 * can handle.
				 *
				 * XXX We should enable the dma again.
				 */
				printf("sr%d: RX End of transfer, rxpkts %lu.\n",
					sc->unit,
					sc->ifsppp.pp_if.if_ipackets);
				sc->ifsppp.pp_if.if_ierrors++;
			}
		}

		isr1 >>= 4;

		mch++;
	}while((mch<NCHAN) && isr1);

	/*
	 * Now that we have done all the urgent things, see if we
	 * can fill the transmit buffers.
	 */
	for(mch = 0; mch < NCHAN; mch++) {
		if(dotxstart & 0x0C) {
			sc = &hc->sc[mch];
			srstart(&sc->ifsppp.pp_if);
		}
		dotxstart >>= 4;
	}
}

static void
sr_msci_intr(struct sr_hardc *hc, u_char isr0)
{
	printf("src%d: SRINTR: MSCI\n", hc->cunit);
}

static void
sr_timer_intr(struct sr_hardc *hc, u_char isr2)
{
	printf("src%d: SRINTR: TIMER\n", hc->cunit);
}

/*
 ********************************* END ************************************
 */

