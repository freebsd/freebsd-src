/*
 * Copyright (c) 1995 John Hay.  All rights reserved.
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
 * $Id: if_ar.c,v 1.7 1995/11/16 20:16:34 jhay Exp $
 */

/*
 * Programming assumptions and other issues.
 *
 * The descriptors of a DMA channel will fit in a 16K memory window.
 *
 * The buffers of a transmit DMA channel will fit in a 16K memory window.
 *
 * Only the ISA bus and V.35 is tested.
 *
 * When interface is going up, handshaking is set and it is only cleared
 * when the interface is down'ed.
 *
 * There should be a way to set/reset Raw HDLC/PPP, Loopback, DCE/DTE,
 * internal/external clock, etc.....
 *
 */

#include "ar.h"
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

#include <i386/isa/if_arregs.h>
#include <i386/isa/ic/hd64570.h>

#ifdef TRACE
#define TRC(x)               x
#else
#define TRC(x)
#endif

#define TRCL(x)              x

#define PPP_HEADER_LEN       4

#define ARC_GET_WIN(addr)	((addr >> ARC_WIN_SHFT) & AR_WIN_MSK)

#define ARC_SET_MEM(iobase,win)	outb(iobase+AR_MSCA_EN, AR_ENA_MEM | \
				ARC_GET_WIN(win))
#define ARC_SET_SCA(iobase,ch)	outb(iobase+AR_MSCA_EN, AR_ENA_MEM | \
				AR_ENA_SCA | (ch ? AR_SEL_SCA_1:AR_SEL_SCA_0))
#define ARC_SET_OFF(iobase)	outb(iobase+AR_MSCA_EN, 0)

#define ARUNIT2SC(unit)		ar_sc_ind[unit]

static struct ar_hardc {
	int cunit;
	struct ar_softc *sc;
	u_short iobase;
	int startunit;
	int numports;
	caddr_t mem_start;
	caddr_t mem_end;

	u_int memsize;		/* in bytes */
	u_char bustype;		/* ISA, MCA, PCI.... */
	u_char interface;	/* X21, V.35, EIA-530.... */
	u_char revision;
	u_char handshake;	/* handshake lines supported by card. */

	u_char txc_dtr[NPORT/NCHAN]; /* the register is write only */
	u_int txc_dtr_off[NPORT/NCHAN];

	sca_regs *sca;

	struct kern_devconf kdc;
}ar_hardc[NAR];

struct ar_softc {
	struct sppp ifsppp;
	int unit;            /* With regards to all ar devices */
	int subunit;         /* With regards to this card */
	struct ar_hardc *hc;
	caddr_t bpf;

	u_int txdesc;        /* On card address */
	u_int txstart;       /* On card address */
	u_int txend;         /* On card address */
	u_int txtail;        /* Index of first unused buffer */
	u_int txmax;         /* number of usable buffers/descriptors */

	u_int rxdesc;        /* On card address */
	u_int rxstart;       /* On card address */
	u_int rxend;         /* On card address */
	u_int rxhind;        /* Index to the head of the rx buffers. */
	u_int rxmax;         /* number of usable buffers/descriptors */

	int scano;
	int scachan;

	struct kern_devconf kdc;
};

struct ar_softc *ar_sc_ind[NAR*NPORT];

int arprobe(struct isa_device *id);
int arattach(struct isa_device *id);

/*
 * This translate from irq numbers to
 * the value that the arnet card needs
 * in the lower part of the AR_INT_SEL
 * register.
 */
static int irqtable[16] = {
	0,	/*  0 */
	0,	/*  1 */
	0,	/*  2 */
	1,	/*  3 */
	0,	/*  4 */
	2,	/*  5 */
	0,	/*  6 */
	3,	/*  7 */
	0,	/*  8 */
	0,	/*  9 */
	4,	/* 10 */
	5,	/* 11 */
	6,	/* 12 */
	0,	/* 13 */
	0,	/* 14 */
	7	/* 15 */
};

struct isa_driver ardriver = {arprobe, arattach, "arc"};

static struct kern_devconf kdc_ar_template = { 
	0, 0, 0, 
	"ar", 0, { MDDT_ISA, 0, "net" },
	isa_generic_externalize, 0, 0, ISA_EXTERNALLEN, 
	&kdc_isa0, 
	0,
	DC_UNCONFIGURED, 
	"Arnet SYNC/570i Port",
	DC_CLS_NETIF
};

static struct kern_devconf kdc_arc_template = { 
	0, 0, 0, 
	"arc", 0, { MDDT_ISA, 0, "net" },
	isa_generic_externalize, 0, 0, ISA_EXTERNALLEN, 
	&kdc_isa0, 
	0,
	DC_UNCONFIGURED, 
	"Arnet SYNC/570i Adapter",
	DC_CLS_NETIF
};

void arstart(struct ifnet *ifp);
int arioctl(struct ifnet *ifp, int cmd, caddr_t data);
void arwatchdog(int port_number);
void arinit(int port_number);

static void ar_up(struct ar_softc *sc);
static void ar_down(struct ar_softc *sc);
static void arc_init(struct isa_device *id);
static void ar_init_sca(struct ar_hardc *hc, int scano);
static void ar_init_msci(struct ar_softc *sc);
static void ar_init_rx_dmac(struct ar_softc *sc);
static void ar_init_tx_dmac(struct ar_softc *sc);
static void ar_dmac_intr(struct ar_hardc *hc, int scano, u_char isr);
static void ar_msci_intr(struct ar_hardc *hc, int scano, u_char isr);
static void ar_timer_intr(struct ar_hardc *hc, int scano, u_char isr);

static inline void ar_registerdev(int ctlr, int unit)
{
	struct ar_softc *sc;
	
	sc = &ar_hardc[ctlr].sc[unit];
	sc->kdc = kdc_ar_template;

	sc->kdc.kdc_unit = ar_hardc[ctlr].startunit + unit;
	sc->kdc.kdc_parentdata = &ar_hardc[ctlr].kdc;
	dev_attach(&sc->kdc);
}

static inline void arc_registerdev(struct isa_device *dvp)
{
        int unit = dvp->id_unit;
	struct ar_hardc *hc = &ar_hardc[dvp->id_unit];

	hc->kdc = kdc_arc_template;

        hc->kdc.kdc_unit = unit;
        hc->kdc.kdc_parentdata = dvp;
        dev_attach(&hc->kdc);
}


/*
 * Register the Adapter.
 * Probe to see if it is there.
 * Get its information and fill it in.
 */
int arprobe(struct isa_device *id)
{
	struct ar_hardc *hc = &ar_hardc[id->id_unit];
	u_int tmp;
	u_short port;

	/*
	 * Register the card.
	 */
	arc_registerdev(id);

	/*
	 * Now see if the card is realy there.
	 *
	 * XXX For now I just check the undocumented ports
	 * for "570". We will probably have to do more checking.
	 */
	port = id->id_iobase;

	if((inb(port+AR_ID_5) != '5') || (inb(port+AR_ID_7) != '7') ||
	   (inb(port+AR_ID_0) != '0'))
		return 0;
	/*
	 * We have a card here, fill in what we can.
	 */
	tmp = inb(port + AR_BMI);
	hc->bustype = tmp & AR_BUS_MSK;
	hc->memsize = (tmp & AR_MEM_MSK) >> AR_MEM_SHFT;
	hc->memsize = 1 << hc->memsize;
	hc->memsize <<= 16;
	hc->interface = (tmp & AR_IFACE_MSK);
	hc->revision = inb(port + AR_REV);
	hc->numports = inb(port + AR_PNUM);
	hc->handshake = inb(port + AR_HNDSH);

	id->id_msize = ARC_WIN_SIZ;

	hc->iobase = id->id_iobase;
	hc->mem_start = id->id_maddr;
	hc->mem_end = id->id_maddr + id->id_msize;
	hc->cunit = id->id_unit;

	switch(hc->interface) {
	case AR_IFACE_EIA_232:
		printf("ar%d: The EIA 232 interface is not supported.\n",
			id->id_unit);
		return 0;
	case AR_IFACE_V_35:
		break;
	case AR_IFACE_EIA_530:
		printf("ar%d: WARNING: The EIA 530 interface is untested.\n",
			id->id_unit);
		break;
	case AR_IFACE_X_21:
		break;
	case AR_IFACE_COMBO:
		printf("ar%d: WARNING: The COMBO interface is untested.\n",
			id->id_unit);
		break;
	}

	switch(hc->numports) {
	case 2:
		hc->kdc.kdc_description = "Arnet SYNC/570i 2 Port Adapter";
		break;
	case 4:
		hc->kdc.kdc_description = "Arnet SYNC/570i 4 Port Adapter";
		break;
	}

	if(id->id_unit == 0)
		hc->startunit = 0;
	else
		hc->startunit = ar_hardc[id->id_unit - 1].startunit +
				ar_hardc[id->id_unit - 1].numports;

	/*
	 * Do a little sanity check.
	 */
	if((hc->numports > NPORT) || (hc->memsize > (512*1024)))
		return 0;

	return ARC_IO_SIZ;      /* return the amount of IO addresses used. */
}


/*
 * Malloc memory for the softc structures.
 * Reset the card to put it in a known state.
 * Register the ports on the adapter.
 * Fill in the info for each port.
 * Attach each port to sppp and bpf.
 */
int arattach(struct isa_device *id)
{
	struct ar_hardc *hc = &ar_hardc[id->id_unit];
	struct ar_softc *sc;
	struct ifnet *ifp;
	int unit;
	char *iface;

	switch(hc->interface) {
	default: iface = "UNKNOWN"; break;
	case AR_IFACE_EIA_232: iface = "EIA-232"; break;
	case AR_IFACE_V_35: iface = "EIA-232 or V.35"; break;
	case AR_IFACE_EIA_530: iface = "EIA-530"; break;
	case AR_IFACE_X_21: iface = "X.21"; break;
	case AR_IFACE_COMBO: iface = "COMBO X.21 / EIA-530"; break;
	}

	printf("arc%d: %uK RAM, %u ports, rev %u, "
		"%s interface.\n",
		id->id_unit,
		hc->memsize/1024,
		hc->numports,
		hc->revision,
		iface);
	
	hc->kdc.kdc_state = DC_BUSY;

	arc_init(id);

	sc = hc->sc;

	for(unit=0;unit<hc->numports;unit+=NCHAN)
		ar_init_sca(hc, unit / NCHAN);

	/*
	 * Now configure each port on the card.
	 */
	for(unit=0;unit<hc->numports;sc++,unit++) {
		sc->hc = hc;
		sc->subunit = unit;
		sc->unit = hc->startunit + unit;
		sc->scano = unit / NCHAN;
		sc->scachan = unit%NCHAN;

		ar_registerdev(id->id_unit, unit);

		ar_init_rx_dmac(sc);
		ar_init_tx_dmac(sc);
		ar_init_msci(sc);

		ifp = &sc->ifsppp.pp_if;

		ifp->if_unit = hc->startunit + unit;
		ifp->if_name = "ar";
		ifp->if_mtu = PP_MTU;
		ifp->if_flags = IFF_POINTOPOINT | IFF_MULTICAST;
		ifp->if_ioctl = arioctl;
		ifp->if_start = arstart;
		ifp->if_watchdog = arwatchdog;
		ifp->if_init = arinit;

		sc->ifsppp.pp_flags = PP_KEEPALIVE;

		sc->kdc.kdc_state = DC_IDLE;

		printf("ar%d: Adapter %d, port %d.\n",
			sc->unit,
			hc->cunit,
			sc->subunit);

		sppp_attach((struct ifnet *)&sc->ifsppp);
		if_attach(ifp);

#if NBPFILTER > 0
		bpfattach(&sc->bpf, ifp, DLT_PPP, PPP_HEADER_LEN);
#endif
	}

	ARC_SET_OFF(hc->iobase);

	return 1;
}

/*
 * First figure out which SCA gave the interrupt.
 * Process it.
 * See if there is other interrupts pending.
 * Repeat until there is no more interrupts.
 */
void arintr(int unit)
{
	struct ar_hardc *hc = &ar_hardc[unit];
	sca_regs *sca = hc->sca;
	u_char isr0, isr1, isr2, arisr;
	int scano;
	static int intno = 0;

	arisr = inb(hc->iobase + AR_ISTAT);

	while(arisr & AR_BD_INT) {
		if(arisr & AR_INT_0)
			scano = 0;
		else if(arisr & AR_INT_1)
			scano = 1;
		else {
			/* XXX Oops this shouldn't happen. */
			printf("arc%d: Interrupted with no interrupt.\n", unit);
			return;
		}

		ARC_SET_SCA(hc->iobase, scano);

		isr0 = sca->isr0;
		isr1 = sca->isr1;
		isr2 = sca->isr2;

		/*
		 * Acknoledge all the interrupts pending.
		 */
		sca->isr0 = isr0;
		sca->isr1 = isr1;
		sca->isr2 = isr2;

		TRC(printf("ARINTR %d, isr0 %x, isr1 %x, isr2 %x\n", intno++, 
			isr0,
			isr1,
			isr2));
		if(isr0)
			ar_msci_intr(hc, scano, isr0);

		if(isr1)
			ar_dmac_intr(hc, scano, isr1);

		if(isr2)
			ar_timer_intr(hc, scano, isr2);

		/*
		 * Proccess the second sca's interrupt if available.
		 * Else see if there are any new interrupts.
		 */
		if((arisr & AR_INT_0) && (arisr & AR_INT_1))
			arisr &= ~AR_INT_0;
		else
			arisr = inb(hc->iobase + AR_ISTAT);
	}

	ARC_SET_OFF(hc->iobase);
}


/*
 * This function will be called from the upper level when a user add a
 * packet to be send, and from the interrupt handler after a finished
 * transmit.
 *
 * NOTE: it should run at spl_imp().
 *
 * Transmitter idle state is indicated by the IFF_OACTIVE flag. The function
 * that clears that should ensure that the transmitter and it's DMA is
 * in a "good" idle state.
 */
void arstart(struct ifnet *ifp)
{
	struct ar_softc *sc = ARUNIT2SC(ifp->if_unit);
	int i, len, tlen;
	struct mbuf *mtx;
	u_char *txdata;
	sca_descriptor *txdesc;
	static int intno = 0;

	if(!(ifp->if_flags & IFF_RUNNING))
		return;
  
	mtx = sppp_dequeue(ifp);
	if(!mtx)
		return;

	intno++;

	/*
	 * It is OK to set the memory window outside the loop because
	 * all tx buffers and descriptors are assumed to be in the same
	 * 16K window.
	 */
	ARC_SET_MEM(sc->hc->iobase, sc->txdesc);

	/*
	 * We stay in this loop until there is nothing in the
	 * TX queue left or the tx buffer is full.
	 */
	i = 0;
	txdesc = (sca_descriptor *)
		(sc->hc->mem_start + (sc->txdesc & ARC_WIN_MSK));
	txdata = (u_char *)(sc->hc->mem_start + (sc->txstart & ARC_WIN_MSK));
	for(;;) {
		len = mtx->m_pkthdr.len;

		TRC(printf("ar%d: ARstart %d, len %u\n", sc->unit, intno, len));

		/*
		 * We can do this because the tx buffers don't wrap.
		 */
		m_copydata(mtx, 0, len, txdata);
		tlen = len;
		while(tlen > AR_BUF_SIZ) {
			txdesc->stat = 0;
			txdesc->len = AR_BUF_SIZ;
			tlen -= AR_BUF_SIZ;
			txdesc++;
			txdata += AR_BUF_SIZ;
			i++;
		}
		/* XXX Move into the loop? */
		txdesc->stat = SCA_DESC_EOM;
		txdesc->len = tlen;
		txdesc++;
		txdata += AR_BUF_SIZ;
		i++;

#if NBPFILTER > 0
		if(sc->bpf)
			bpf_mtap(sc->bpf, mtx);
#endif
		m_freem(mtx);
		++sc->ifsppp.pp_if.if_opackets;

		/*
		 * Check if we have space for another mbuf.
		 * XXX This is hardcoded. A packet won't be larger
		 * than 3 buffers (3 x 512).
		 */
		if((i + 3) >= sc->txmax)
			break;

		mtx = sppp_dequeue(ifp);
		if(!mtx)
			break;
	}

	sc->txtail = i;

	/*
	 * Mark the last descriptor, so that the SCA know where
	 * to stop.
	 */
	txdesc--;
	txdesc->stat |= SCA_DESC_EOT;

#if 0
	printf("ARstart: %p desc->cp %x\n", &txdesc->cp, txdesc->cp);
	printf("ARstart: %p desc->bp %x\n", &txdesc->bp, txdesc->bp);
	printf("ARstart: %p desc->bpb %x\n", &txdesc->bpb, txdesc->bpb);
	printf("ARstart: %p desc->len %x\n", &txdesc->len, txdesc->len);
	printf("ARstart: %p desc->stat %x\n", &txdesc->stat, txdesc->stat);
#endif

	/*
	 * Start DMA.
	 * XXX For now we always start from the start.
	 */
	{
		dmac_channel *dmac = &sc->hc->sca->dmac[DMAC_TXCH(sc->scachan)];

		ARC_SET_SCA(sc->hc->iobase, sc->scano);
		dmac->cda = (u_short)(sc->txdesc & 0xffff);

		txdesc = (sca_descriptor *)sc->txdesc;
		dmac->eda = (u_short)((u_int)&txdesc[i]);
		dmac->dsr = SCA_DSR_DE;
	}

	ifp->if_flags |= IFF_OACTIVE;
	ifp->if_timer = 2; /* Value in seconds. */
	ARC_SET_OFF(sc->hc->iobase);
}

int arioctl(struct ifnet *ifp, int cmd, caddr_t data)
{
	int s, error;
	int was_up, should_be_up;
	struct sppp *sp = (struct sppp *)ifp;
	struct ar_softc *sc = ARUNIT2SC(ifp->if_unit);

	TRC(printf("ar%d: arioctl.\n", ifp->if_unit);)

	if(cmd == SIOCSIFFLAGS) {
		if(ifp->if_flags & IFF_LINK2)
			sp->pp_flags |= PP_CISCO;
		else
			sp->pp_flags &= ~PP_CISCO;
	}

	was_up = ifp->if_flags & IFF_RUNNING;

	error = sppp_ioctl(ifp, cmd, data);
	TRC(printf("ar%d: ioctl: ifsppp.pp_flags = %x, if_flags %x.\n", 
		ifp->if_unit, ((struct sppp *)ifp)->pp_flags, ifp->if_flags);)
	if(error)
		return error;

	if((cmd != SIOCSIFFLAGS) && cmd != (SIOCSIFADDR))
		return 0;

	TRC(printf("ar%d: arioctl %s.\n", ifp->if_unit, 
		(cmd == SIOCSIFFLAGS) ? "SIOCSIFFLAGS" : "SIOCSIFADDR");)

	s = splimp();
	should_be_up = ifp->if_flags & IFF_RUNNING;

	if(!was_up && should_be_up) {
		/* Interface should be up -- start it. */
		ar_up(sc);
		arstart(ifp);
		/* XXX Maybe clear the IFF_UP flag so that the link
		 * will only go up after sppp lcp and ipcp negotiation.
		 */
	} else if(was_up && !should_be_up) {
		/* Interface should be down -- stop it. */
		ar_down(sc);
		sppp_flush(ifp);
	}
	splx(s);
	return 0;
}

/*
 * This is to catch lost tx interrupts.
 */
void arwatchdog(int unit)
{
	struct ar_softc *sc = ARUNIT2SC(unit);

	if(!(sc->ifsppp.pp_if.if_flags & IFF_RUNNING))
		return;

	/* XXX if(sc->ifsppp.pp_if.if_flags & IFF_DEBUG) */
		printf("ar%d: transmit failed.\n", unit);

	ARC_SET_SCA(sc->hc->iobase, sc->scano);
	sc->hc->sca->msci[sc->scachan].cmd = SCA_CMD_TXABORT;

	ar_down(sc);
	ar_up(sc);

	sc->ifsppp.pp_if.if_flags &= ~IFF_OACTIVE;

	arstart(&sc->ifsppp.pp_if);
}

static void ar_up(struct ar_softc *sc)
{
	sca_regs *sca = sc->hc->sca;
	msci_channel *msci = &sca->msci[sc->scachan];

	TRC(printf("ar%d: sca %p, msci %p, ch %d\n",
		sc->unit, sca, msci, sc->scachan));
	sc->kdc.kdc_state = DC_BUSY;

	/*
	 * Enable transmitter and receiver.
	 * Raise DTR and RTS.
	 * Enable interrupts.
	 */
	ARC_SET_SCA(sc->hc->iobase, sc->scano);

	/* XXX
	 * What about using AUTO mode in msci->md0 ???
	 * And what about CTS/DCD etc... ?
	 */
	if(sc->hc->handshake & AR_SHSK_RTS)
		msci->ctl |= SCA_CTL_RTS;
	if(sc->hc->handshake & AR_SHSK_DTR) {
		sc->hc->txc_dtr[sc->scano] |= sc->scachan ? 
			AR_TXC_DTR_DTR1 : AR_TXC_DTR_DTR0;
		outb(sc->hc->iobase + sc->hc->txc_dtr_off[sc->scano],
			sc->hc->txc_dtr[sc->scano]);
	}

	if(sc->scachan == 0) {
		sca->ier0 |= 0x0F;
		sca->ier1 |= 0x0F;
	} else {
		sca->ier0 |= 0xF0;
		sca->ier1 |= 0xF0;
	}

	msci->cmd = SCA_CMD_RXENABLE;
	inb(sc->hc->iobase + AR_ID_5); /* XXX slow it down a bit. */
	msci->cmd = SCA_CMD_TXENABLE;

	ARC_SET_OFF(sc->hc->iobase);
}

static void ar_down(struct ar_softc *sc)
{
	sca_regs *sca = sc->hc->sca;
	msci_channel *msci = &sca->msci[sc->scachan];

	sc->kdc.kdc_state = DC_IDLE;

	/*
	 * Disable transmitter and receiver.
	 * Lower DTR and RTS.
	 * Disable interrupts.
	 */
	ARC_SET_SCA(sc->hc->iobase, sc->scano);
	msci->cmd = SCA_CMD_RXDISABLE;
	inb(sc->hc->iobase + AR_ID_5); /* XXX slow it down a bit. */
	msci->cmd = SCA_CMD_TXDISABLE;

	if(sc->hc->handshake & AR_SHSK_RTS)
		msci->ctl &= ~SCA_CTL_RTS;
	if(sc->hc->handshake & AR_SHSK_DTR) {
		sc->hc->txc_dtr[sc->scano] |= sc->scachan ? 
			AR_TXC_DTR_DTR1 : AR_TXC_DTR_DTR0;
		outb(sc->hc->iobase + sc->hc->txc_dtr_off[sc->scano],
			sc->hc->txc_dtr[sc->scano]);
	}

	if(sc->scachan == 0) {
		sca->ier0 &= ~0x0F;
		sca->ier1 &= ~0x0F;
	} else {
		sca->ier0 &= ~0xF0;
		sca->ier1 &= ~0xF0;
	}

	ARC_SET_OFF(sc->hc->iobase);
}

/*
 * I don't think anything ever calls this function.
 */
void arinit(int unit)
  {
  int s;
  struct ar_softc *sc = ARUNIT2SC(unit);

  printf("ar%d: OOPS, so somebody do call arinit!\n", unit);

  s = splimp();
  ar_down(sc);
  ar_up(sc);
  splx(s);
  }

/*
 * Initialize the card, allocate memory for the ar_softc structures
 * and fill in the pointers.
 */
void arc_init(struct isa_device *id)
{
	struct ar_hardc *hc = &ar_hardc[id->id_unit];
	struct ar_softc *sc;
	int x;
	u_int chanmem;
	u_int bufmem;
	u_int next;
	u_int descneeded;
	u_char isr, mar;

	sc = hc->sc = malloc(hc->numports * sizeof(struct ar_softc),
				M_DEVBUF, M_WAITOK);
	bzero(sc, hc->numports * sizeof(struct ar_softc));

	/*
	 * reset the card and wait at least 1uS.
	 */
	outb(hc->iobase + AR_TXC_DTR0, AR_TXC_DTR_RESET);
	DELAY(2);
	outb(hc->iobase + AR_TXC_DTR0, AR_TXC_DTR_NOTRESET);

	hc->txc_dtr[0] = AR_TXC_DTR_NOTRESET;
	hc->txc_dtr[1] = 0;
	hc->txc_dtr_off[0] = AR_TXC_DTR0;
	hc->txc_dtr_off[1] = AR_TXC_DTR2;

	/*
	 * Configure the card.
	 * Mem address, irq, 
	 */
	mar = kvtop(id->id_maddr) >> 16;
	isr = irqtable[ffs(id->id_irq) - 1] << 1;
	if(isr == 0)
		printf("ar%d: Warning illegal interrupt %d\n",
			id->id_unit, ffs(id->id_irq) - 1);
	isr = isr | ((kvtop(id->id_maddr) & 0xc000) >> 10);

	hc->sca = (sca_regs *)hc->mem_start;

	outb(hc->iobase + AR_MEM_SEL, mar);
	outb(hc->iobase + AR_INT_SEL, isr | AR_INTS_CEN);

	/*
	 * Make TX clock output and enable TX.
	 */
	hc->txc_dtr[0] |= AR_TXC_DTR_TX0 | AR_TXC_DTR_TX1 |
			  AR_TXC_DTR_TXCS0 | AR_TXC_DTR_TXCS1;
	outb(hc->iobase + AR_TXC_DTR0, hc->txc_dtr[0]);
	if(hc->numports > NCHAN) {
		hc->txc_dtr[1] |= AR_TXC_DTR_TX0 | AR_TXC_DTR_TX1 |
				  AR_TXC_DTR_TXCS0 | AR_TXC_DTR_TXCS1;
		outb(hc->iobase + AR_TXC_DTR2, hc->txc_dtr[1]);
	}

	chanmem = hc->memsize / hc->numports;
	next = 0;

	for(x=0;x<hc->numports;x++, sc++) {
		sc->txdesc = next;
		bufmem = 16 * 1024;
		descneeded = bufmem / AR_BUF_SIZ;
		sc->txstart = sc->txdesc +
				((((descneeded * sizeof(sca_descriptor)) /
					AR_BUF_SIZ) + 1) * AR_BUF_SIZ);
		sc->txend = next + bufmem;
		sc->txmax = (sc->txend - sc->txstart) / AR_BUF_SIZ;
		next += bufmem;

		TRC(printf("ar%d: txdesc %x, txstart %x, txend %x, txmax %d\n",
			x,
			sc->txdesc, sc->txstart, sc->txend, sc->txmax));

		sc->rxdesc = next;
		bufmem = chanmem - bufmem;
		descneeded = bufmem / AR_BUF_SIZ;
		sc->rxstart = sc->rxdesc +
				((((descneeded * sizeof(sca_descriptor)) /
					AR_BUF_SIZ) + 1) * AR_BUF_SIZ);
		sc->rxend = next + bufmem;
		sc->rxmax = (sc->rxend - sc->rxstart) / AR_BUF_SIZ;
		next += bufmem;

		/*
		 * This is by ARUNIT2SC().
		 */
		ar_sc_ind[x] = sc;
	}
}


/*
 * The things done here are channel independent.
 *
 *   Configure the sca waitstates.
 *   Configure the global interrupt registers.
 *   Enable master dma enable.
 */
static void ar_init_sca(struct ar_hardc *hc, int scano)
{
	sca_regs *sca = hc->sca;

	ARC_SET_SCA(hc->iobase, scano);

	/*
	 * Do the wait registers.
	 * Set everything to 0 wait states.
	 */
	sca->pabr0 = 0;
	sca->pabr1 = 0;
	sca->wcrl  = 0;
	sca->wcrm  = 0;
	sca->wcrh  = 0;

	/*
	 * Configure the interrupt registers.
	 * Most are cleared until the interface is configured.
	 */
	sca->ier0 = 0x00; /* MSCI interrupts... Not used with dma. */
	sca->ier1 = 0x00; /* DMAC interrupts */
	sca->ier2 = 0x00; /* TIMER interrupts... Not used yet. */
	sca->itcr = 0x00; /* Use ivr and no intr ack */
	sca->ivr  = 0x40; /* Fill in the interrupt vector. */
	sca->imvr = 0x40;

	/*
	 * Configure the timers.
	 * XXX Later
	 */


	sca->dmer = SCA_DMER_EN; /* Enable all dma channels. */
}


/*
 * Configure the msci
 *
 * NOTE: The serial port configuration is hardcoded at the moment.
 */
void ar_init_msci(struct ar_softc *sc)
{
	msci_channel *msci = &sc->hc->sca->msci[sc->scachan];

	ARC_SET_SCA(sc->hc->iobase, sc->scano);

	msci->cmd = SCA_CMD_RESET;

	msci->md0 = SCA_MD0_CRC_1 |
		    SCA_MD0_CRC_CCITT |
		    SCA_MD0_CRC_ENABLE |
		    SCA_MD0_MODE_HDLC;
	msci->md1 = SCA_MD1_NOADDRCHK;
	msci->md2 = SCA_MD2_DUPLEX | SCA_MD2_NRZ;

	/*
	 * Acording to the manual I should give a reset after changing the
	 * mode registers.
	 */
	msci->cmd = SCA_CMD_RXRESET;
	msci->ctl = SCA_CTL_IDLPAT | SCA_CTL_UDRNC;

	/*
	 * For now all interfaces are programmed to use the RX clock for
	 * the TX clock.
	 */
	switch(sc->hc->interface) {
	case AR_IFACE_V_35:
	case AR_IFACE_X_21:
	case AR_IFACE_EIA_530:
	case AR_IFACE_COMBO:
		msci->rxs = SCA_RXS_CLK_RXC0 | SCA_RXS_DIV1;
		msci->txs = SCA_TXS_CLK_RX | SCA_TXS_DIV1;
	}

	msci->tmc = 153;   /* This give 64k for loopback */

	/* XXX
	 * Disable all interrupts for now. I think if you are using
	 * the dmac you don't use these interrupts.
	 */
	msci->ie0 = 0;
	msci->ie1 = 0x0C; /* XXX CTS and DCD (DSR on 570I) level change. */
	msci->ie2 = 0;
	msci->fie = 0;

	msci->sa0 = 0;
	msci->sa1 = 0;

	msci->idl = 0x7E; /* XXX This is what cisco does. */

	/*
	 * This is what the ARNET diags use.
	 */
	msci->rrc  = 0x0E;
	msci->trc0 = 0x12;
	msci->trc1 = 0x1F;
}

/*
 * Configure the rx dma controller.
 */
void ar_init_rx_dmac(struct ar_softc *sc)
{
	dmac_channel *dmac = &sc->hc->sca->dmac[DMAC_RXCH(sc->scachan)];
	sca_descriptor *rxd;
	u_int rxbuf;
	u_int rxda;
	u_int rxda_d;

	ARC_SET_MEM(sc->hc->iobase, sc->rxdesc);

	rxd = (sca_descriptor *)(sc->hc->mem_start + (sc->rxdesc&ARC_WIN_MSK));
	rxda_d = (u_int)sc->hc->mem_start - (sc->rxdesc & ~ARC_WIN_MSK);

	for(rxbuf=sc->rxstart;rxbuf<sc->rxend;rxbuf += AR_BUF_SIZ, rxd++) {
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

	ARC_SET_SCA(sc->hc->iobase, sc->scano);

	dmac->dsr = 0;    /* Disable DMA transfer */
	dmac->dcr = SCA_DCR_ABRT;

	/* XXX maybe also SCA_DMR_CNTE */
	dmac->dmr = SCA_DMR_TMOD | SCA_DMR_NF;
	dmac->bfl = AR_BUF_SIZ;

	dmac->cda = (u_short)(sc->rxdesc & 0xffff);
	dmac->sarb = (u_char)((sc->rxdesc >> 16) & 0xff);

	rxd = (sca_descriptor *)sc->rxstart;
	dmac->eda = (u_short)((u_int)&rxd[sc->rxmax - 1] & 0xffff);

	dmac->dir = 0xF0;

	dmac->dsr = SCA_DSR_DE;
}

/*
 * Configure the TX DMA descriptors.
 * Initialize the needed values and chain the descriptors.
 */
void ar_init_tx_dmac(struct ar_softc *sc)
{
	dmac_channel *dmac = &sc->hc->sca->dmac[DMAC_TXCH(sc->scachan)];
	sca_descriptor *txd;
	u_int txbuf;
	u_int txda;
	u_int txda_d;

	ARC_SET_MEM(sc->hc->iobase, sc->txdesc);

	txd = (sca_descriptor *)(sc->hc->mem_start + (sc->txdesc&ARC_WIN_MSK));
	txda_d = (u_int)sc->hc->mem_start - (sc->txdesc & ~ARC_WIN_MSK);

	for(txbuf=sc->txstart;txbuf<sc->txend;txbuf += AR_BUF_SIZ, txd++) {
		txda = (u_int)&txd[1] - txda_d;
		txd->cp = (u_short)(txda & 0xfffful);

		txd->bp = (u_short)(txbuf & 0xfffful);
		txd->bpb = (u_char)((txbuf >> 16) & 0xff);
		TRC(printf("ar%d: txbuf %x, bpb %x, bp %x\n",
			sc->unit, txbuf, txd->bpb, txd->bp));
		txd->len = 0;
		txd->stat = 0;
	}
	txd--;
	txd->cp = (u_short)(sc->txdesc & 0xfffful);

	sc->txtail = (u_int)txd - (u_int)sc->hc->mem_start;
	TRC(printf("TX Descriptors start %x, end %x.\n",
		sc->txhead,
		sc->txtail));

	ARC_SET_SCA(sc->hc->iobase, sc->scano);

	dmac->dsr = 0; /* Disable DMA */
	dmac->dcr = SCA_DCR_ABRT;
	dmac->dmr = SCA_DMR_TMOD | SCA_DMR_NF;
	dmac->dir = SCA_DIR_EOT | SCA_DIR_BOF | SCA_DIR_COF;

	dmac->sarb = (u_char)((sc->txdesc >> 16) & 0xff);
}


/*
 * Look through the descriptors to see if there is a complete packet
 * available. Stop if we get to where the sca is busy.
 *
 * Return the length and status of the packet.
 * Return nonzero if there is a packet available.
 */
int ar_packet_avail(struct ar_softc *sc,
		    int *len,
		    u_char *rxstat)
{
	sca_descriptor *rxdesc;
	sca_descriptor *endp;

	ARC_SET_MEM(sc->hc->iobase, sc->rxdesc);
	rxdesc = (sca_descriptor *)
			(sc->hc->mem_start + (sc->rxdesc & ARC_WIN_MSK));
	endp = rxdesc;
	rxdesc = &rxdesc[sc->rxhind];
	endp = &endp[sc->rxmax];

	*len = 0;

	while(rxdesc->stat != 0xff) {
		*len += rxdesc->len;

		if(rxdesc->stat & SCA_DESC_EOM) {
			*rxstat = rxdesc->stat;
			TRC(printf("ar%d: PKT AVAIL len %d, %x, bufs %u.\n",
				sc->unit, *len, *rxstat, x));
			return 1;
		}
		rxdesc++;
		if(rxdesc == endp)
			rxdesc = (sca_descriptor *)
				(sc->hc->mem_start + (sc->rxdesc & ARC_WIN_MSK));
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
void ar_copy_rxbuf(struct mbuf *m,
		   struct ar_softc *sc,
		   int len)
{
	sca_descriptor *rxdesc;
	u_int rxdata;
	u_int rxmax;
	u_int off = 0;
	u_int tlen;

	rxdata = sc->rxstart + (sc->rxhind * AR_BUF_SIZ);
	rxmax = sc->rxstart + (sc->rxmax * AR_BUF_SIZ);

	rxdesc = (sca_descriptor *)
			(sc->hc->mem_start + (sc->rxdesc & ARC_WIN_MSK));
	rxdesc = &rxdesc[sc->rxhind];

	while(len) {
		tlen = (len < AR_BUF_SIZ) ? len : AR_BUF_SIZ;
		ARC_SET_MEM(sc->hc->iobase, rxdata);
		bcopy(sc->hc->mem_start + (rxdata & ARC_WIN_MSK), 
			mtod(m, caddr_t) + off,
			tlen);

		off += tlen;
		len -= tlen;

		ARC_SET_MEM(sc->hc->iobase, sc->rxdesc);
		rxdesc->len = 0;
		rxdesc->stat = 0xff;

		rxdata += AR_BUF_SIZ;
		rxdesc++;
		if(rxdata == rxmax) {
			rxdata = sc->rxstart;
			rxdesc = (sca_descriptor *)
				(sc->hc->mem_start + (sc->rxdesc & ARC_WIN_MSK));
		}
	}
}

/*
 * Just eat a packet. Update pointers to point to the next packet.
 */
void ar_eat_packet(struct ar_softc *sc)
{
	sca_descriptor *rxdesc;
	sca_descriptor *endp;

	/*
	 * Loop until desc->stat == (0xff || EOM)
	 * Clear the status and length in the descriptor.
	 * Increment the descriptor.
	 */
	ARC_SET_MEM(sc->hc->iobase, sc->rxdesc);
	rxdesc = (sca_descriptor *)
		(sc->hc->mem_start + (sc->rxdesc & ARC_WIN_MSK));
	endp = rxdesc;
	rxdesc = &rxdesc[sc->rxhind];
	endp = &endp[sc->rxmax];

	while(rxdesc->stat != 0xff) {
		if((rxdesc->stat & ~SCA_DESC_EOM) == 0)
			break;

		rxdesc->len = 0;
		rxdesc->stat = 0xff;

		rxdesc++;
		sc->rxhind++;
		if(rxdesc == endp) {
			rxdesc = (sca_descriptor *)
			       (sc->hc->mem_start + (sc->rxdesc & ARC_WIN_MSK));
			sc->rxhind = 0;
		}
	}
}


/*
 * While there is packets available in the rx buffer, read them out
 * into mbufs and ship them off.
 */
void ar_get_packets(struct ar_softc *sc)
{
	sca_descriptor *rxdesc;
	struct mbuf *m = NULL;
	int i;
	int len;
	u_char rxstat;

	while(ar_packet_avail(sc, &len, &rxstat)) {
		if((rxstat & SCA_DESC_ERRORS) == 0) {
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if(m != NULL) {
				MCLGET(m, M_DONTWAIT);
				if((m->m_flags & M_EXT) != 0) {
					m->m_pkthdr.rcvif = &sc->ifsppp.pp_if;
					m->m_pkthdr.len = m->m_len = len;
					ar_copy_rxbuf(m, sc, len);
#if NBPFILTER > 0
					if(sc->bpf)
						bpf_mtap(sc->bpf, m);
#endif
					sppp_input(&sc->ifsppp.pp_if, m);
					sc->ifsppp.pp_if.if_ipackets++;
				} else {
					m_freem(m);
					/* eat packet if get mbuf fail!! */
					ar_eat_packet(sc);
				}
			} else {
				/* eat packet if get mbuf fail!! */
				ar_eat_packet(sc);
			}
		} else {
			msci_channel *msci = &sc->hc->sca->msci[sc->scachan];

			ar_eat_packet(sc);

			sc->ifsppp.pp_if.if_ierrors++;

			ARC_SET_SCA(sc->hc->iobase, sc->scano);

			TRCL(printf("RX%d So this does happen :), stat %x, "
					"ST2 %x, cda %x.\n",
					sc->scachan, 
					rxstat, 
					msci->st2,
					sc->hc->sca->dmac[DMAC_RXCH(sc->scachan)].cda));

			/*
			 * Reset the rx unit.
			 *
			 * XXX Maybe it should only be done when certain
			 * errors occured. ie not for CRC errors?
			 */
			msci->cmd = SCA_CMD_RXRESET;
			msci->cmd = SCA_CMD_RXENABLE;

			TRCL(printf("RX%d After reset: ST2 %x.\n",
					sc->scachan, msci->st2));
		} /* else */

		i = (len + AR_BUF_SIZ - 1) / AR_BUF_SIZ;
		sc->rxhind = (sc->rxhind + i) % sc->rxmax;

		/*
		 * Update the eda to the previous descriptor.
		 */
		ARC_SET_SCA(sc->hc->iobase, sc->scano);

		rxdesc = (sca_descriptor *)sc->rxdesc;
		rxdesc = &rxdesc[(sc->rxhind + sc->rxmax -1 ) % sc->rxmax];

		sc->hc->sca->dmac[DMAC_RXCH(sc->scachan)].eda = 
			(u_short)((u_int)&rxdesc & 0xffff);

	} /* while */
}


/*
 * All DMA interrupts come here.
 *
 * Each channel has two interrupts.
 * Interrupt A for errors and Interrupt B for normal stuff like end
 * of transmit or receive dmas.
 */
static void ar_dmac_intr(struct ar_hardc *hc, int scano, u_char isr1)
{
	u_char dsr;
	int mch;
	struct ar_softc *sc;
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
		sc = &hc->sc[mch + (NCHAN * scano)];
		dmac = &sca->dmac[DMAC_RXCH(mch)];

		/*
		 * Receive channel
		 */
		if(isr1 & 0x03) {
			ARC_SET_SCA(hc->iobase, scano);

			dsr = dmac->dsr;
			dmac->dsr = dsr;

			TRC(printf("AR: RX DSR %x\n", dsr));

			/* End of frame */
			if(dsr & SCA_DSR_EOM) {
				ar_get_packets(sc);
			}

			/* Counter overflow */
			if(dsr & SCA_DSR_COF) {
				printf("ar%d: RX DMA Counter overflow, "
					"rxpkts %lu.\n",
					sc->unit,
					sc->ifsppp.pp_if.if_ipackets);
				sc->ifsppp.pp_if.if_ierrors++;
			}

			/* Buffer overflow */
			if(dsr & SCA_DSR_BOF) {
				printf("ar%d: RX DMA Buffer overflow, "
					"rxpkts %lu.\n",
					sc->unit,
					sc->ifsppp.pp_if.if_ipackets);
				sc->ifsppp.pp_if.if_ierrors++;
				dmac->dsr = SCA_DSR_DE;
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
				printf("ar%d: RX End of transfer, rxpkts %lu.\n",
					sc->unit,
					sc->ifsppp.pp_if.if_ipackets);
				sc->ifsppp.pp_if.if_ierrors++;
			}
		}

		/*
		 * We are finished with the 2 DMA status bits for RX now do TX.
		 */
		isr1 >>= 2;

		/*
		 * Transmit channel
		 */
		if(isr1 & 0x03) {
			dmac = &sca->dmac[DMAC_TXCH(mch)];

			ARC_SET_SCA(hc->iobase, scano);

			dsr = dmac->dsr;
			dmac->dsr = dsr;

			/* Counter overflow */
			if(dsr & SCA_DSR_COF) {
				printf("ar%d: TX DMA Counter overflow, "
					"txpacket no %lu.\n",
					sc->unit,
					sc->ifsppp.pp_if.if_opackets);
				sc->ifsppp.pp_if.if_oerrors++;
			}

			/* Buffer overflow */
			if(dsr & SCA_DSR_BOF) {
				printf("ar%d: TX DMA Buffer overflow, "
					"txpacket no %lu, dsr %02x, "
					"cda %04x, eda %04x.\n",
					sc->unit,
					sc->ifsppp.pp_if.if_opackets,
					dsr,
					dmac->cda,
					dmac->eda);
				sc->ifsppp.pp_if.if_oerrors++;
			}

			/* End of Transfer */
			if(dsr & SCA_DSR_EOT) {
				/*
				 * This should be the most common case.
				 *
				 * Clear the IFF_OACTIVE flag.
				 *
				 * Call arstart to start a new transmit if
				 * there is data to transmit.
				 */
				sc->ifsppp.pp_if.if_flags &= ~IFF_OACTIVE;
				sc->ifsppp.pp_if.if_timer = 0;

				arstart(&sc->ifsppp.pp_if);
			}
		}
		isr1 >>= 2;

		mch++;
	}while((mch<NCHAN) && isr1);
}

static void ar_msci_intr(struct ar_hardc *hc, int scano, u_char isr0)
{
	printf("arc%d: ARINTR: MSCI\n", hc->cunit);
}

static void ar_timer_intr(struct ar_hardc *hc, int scano, u_char isr2)
{
	printf("arc%d: ARINTR: TIMER\n", hc->cunit);
}

/*
 ********************************* END ************************************
 */

