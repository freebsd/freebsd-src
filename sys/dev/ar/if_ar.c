/*
 * Copyright (c) 1995 - 2001 John Hay.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
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
 * $FreeBSD$
 */

/*
 * Programming assumptions and other issues.
 *
 * The descriptors of a DMA channel will fit in a 16K memory window.
 *
 * The buffers of a transmit DMA channel will fit in a 16K memory window.
 *
 * Only the ISA bus cards with X.21 and V.35 is tested.
 *
 * When interface is going up, handshaking is set and it is only cleared
 * when the interface is down'ed.
 *
 * There should be a way to set/reset Raw HDLC/PPP, Loopback, DCE/DTE,
 * internal/external clock, etc.....
 *
 */

#include "opt_netgraph.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/bus_pio.h>
#include <machine/bus_memio.h>
#include <sys/rman.h>

#include <net/if.h>
#ifdef NETGRAPH
#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <sys/syslog.h>
#include <dev/ar/if_ar.h>
#else /* NETGRAPH */
#include <net/if_sppp.h>
#include <net/bpf.h>
#endif /* NETGRAPH */

#include <machine/md_var.h>

#include <dev/ic/hd64570.h>
#include <dev/ar/if_arregs.h>

#ifdef TRACE
#define TRC(x)               x
#else
#define TRC(x)
#endif

#define TRCL(x)              x

#define PPP_HEADER_LEN       4

struct ar_softc {
#ifndef	NETGRAPH
	struct sppp ifsppp;
#endif /* NETGRAPH */
	int unit;            /* With regards to all ar devices */
	int subunit;         /* With regards to this card */
	struct ar_hardc *hc;

	struct buf_block {
		u_int txdesc;        /* On card address */
		u_int txstart;       /* On card address */
		u_int txend;         /* On card address */
		u_int txtail;        /* Index of first unused buffer */
		u_int txmax;         /* number of usable buffers/descriptors */
		u_int txeda;         /* Error descriptor addresses */
	}block[AR_TX_BLOCKS];

	char  xmit_busy;     /* Transmitter is busy */
	char  txb_inuse;     /* Number of tx blocks currently in use */
	u_char txb_new;      /* Index to where new buffer will be added */
	u_char txb_next_tx;  /* Index to next block ready to tx */

	u_int rxdesc;        /* On card address */
	u_int rxstart;       /* On card address */
	u_int rxend;         /* On card address */
	u_int rxhind;        /* Index to the head of the rx buffers. */
	u_int rxmax;         /* number of usable buffers/descriptors */

	int scano;
	int scachan;
	sca_regs *sca;
#ifdef NETGRAPH
	int	running;	/* something is attached so we are running */
	int	dcd;		/* do we have dcd? */
	/* ---netgraph bits --- */
	char		nodename[NG_NODELEN + 1]; /* store our node name */
	int		datahooks;	/* number of data hooks attached */
	node_p		node;		/* netgraph node */
	hook_p		hook;		/* data hook */
	hook_p		debug_hook;
	struct ifqueue	xmitq_hipri;	/* hi-priority transmit queue */
	struct ifqueue	xmitq;		/* transmit queue */
	int		flags;		/* state */
#define	SCF_RUNNING	0x01		/* board is active */
#define	SCF_OACTIVE	0x02		/* output is active */
	int		out_dog;	/* watchdog cycles output count-down */
	struct callout_handle handle;	/* timeout(9) handle */
	u_long		inbytes, outbytes;	/* stats */
	u_long		lastinbytes, lastoutbytes; /* a second ago */
	u_long		inrate, outrate;	/* highest rate seen */
	u_long		inlast;		/* last input N secs ago */
	u_long		out_deficit;	/* output since last input */
	u_long		oerrors, ierrors[6];
	u_long		opackets, ipackets;
#endif /* NETGRAPH */
};

static int	next_ar_unit = 0;

#ifdef NETGRAPH
#define DOG_HOLDOFF	6	/* dog holds off for 6 secs */
#define QUITE_A_WHILE	300	/* 5 MINUTES */
#define LOTS_OF_PACKETS	100
#endif /* NETGRAPH */

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

#ifndef NETGRAPH
MODULE_DEPEND(if_ar, sppp, 1, 1, 1);
#else
MODULE_DEPEND(ng_sync_ar, netgraph, 1, 1, 1);
#endif

static void arintr(void *arg);
static void ar_xmit(struct ar_softc *sc);
#ifndef NETGRAPH
static void arstart(struct ifnet *ifp);
static int arioctl(struct ifnet *ifp, u_long cmd, caddr_t data);
static void arwatchdog(struct ifnet *ifp);
#else	/* NETGRAPH */
static void arstart(struct ar_softc *sc);
static void arwatchdog(struct ar_softc *sc);
#endif	/* NETGRAPH */
static int ar_packet_avail(struct ar_softc *sc, int *len, u_char *rxstat);
static void ar_copy_rxbuf(struct mbuf *m, struct ar_softc *sc, int len);
static void ar_eat_packet(struct ar_softc *sc, int single);
static void ar_get_packets(struct ar_softc *sc);

static int ar_read_pim_iface(volatile struct ar_hardc *hc, int channel);
static void ar_up(struct ar_softc *sc);
static void ar_down(struct ar_softc *sc);
static void arc_init(struct ar_hardc *hc);
static void ar_init_sca(struct ar_hardc *hc, int scano);
static void ar_init_msci(struct ar_softc *sc);
static void ar_init_rx_dmac(struct ar_softc *sc);
static void ar_init_tx_dmac(struct ar_softc *sc);
static void ar_dmac_intr(struct ar_hardc *hc, int scano, u_char isr);
static void ar_msci_intr(struct ar_hardc *hc, int scano, u_char isr);
static void ar_timer_intr(struct ar_hardc *hc, int scano, u_char isr);

#ifdef	NETGRAPH
static	void	ngar_watchdog_frame(void * arg);
static	void	ngar_init(void* ignored);

static ng_constructor_t	ngar_constructor;
static ng_rcvmsg_t	ngar_rcvmsg;
static ng_shutdown_t	ngar_shutdown;
static ng_newhook_t	ngar_newhook;
/*static ng_findhook_t	ngar_findhook; */
static ng_connect_t	ngar_connect;
static ng_rcvdata_t	ngar_rcvdata;
static ng_disconnect_t	ngar_disconnect;
	
static struct ng_type typestruct = {
	NG_ABI_VERSION,
	NG_AR_NODE_TYPE,
	NULL,
	ngar_constructor,
	ngar_rcvmsg,
	ngar_shutdown,
	ngar_newhook,
	NULL,
	ngar_connect,
	ngar_rcvdata,
	ngar_disconnect,
	NULL
};

static int	ngar_done_init = 0;
#endif /* NETGRAPH */

int
ar_attach(device_t device)
{
	struct ar_hardc *hc;
	struct ar_softc *sc;
#ifndef	NETGRAPH
	struct ifnet *ifp;
	char *iface;
#endif	/* NETGRAPH */
	int unit;

	hc = (struct ar_hardc *)device_get_softc(device);

	printf("arc%d: %uK RAM, %u ports, rev %u.\n",
		hc->cunit,
		hc->memsize/1024,
		hc->numports,
		hc->revision);
	
	arc_init(hc);

	if(BUS_SETUP_INTR(device_get_parent(device), device, hc->res_irq,
	    INTR_TYPE_NET, arintr, hc, &hc->intr_cookie) != 0)
		return (1);

	sc = hc->sc;

	for(unit=0;unit<hc->numports;unit+=NCHAN)
		ar_init_sca(hc, unit / NCHAN);

	/*
	 * Now configure each port on the card.
	 */
	for(unit=0;unit<hc->numports;sc++,unit++) {
		sc->hc = hc;
		sc->subunit = unit;
		sc->unit = next_ar_unit;
		next_ar_unit++;
		sc->scano = unit / NCHAN;
		sc->scachan = unit%NCHAN;

		ar_init_rx_dmac(sc);
		ar_init_tx_dmac(sc);
		ar_init_msci(sc);

#ifndef	NETGRAPH
		ifp = &sc->ifsppp.pp_if;

		ifp->if_softc = sc;
		ifp->if_unit = sc->unit;
		ifp->if_name = "ar";
		ifp->if_mtu = PP_MTU;
		ifp->if_flags = IFF_POINTOPOINT | IFF_MULTICAST;
		ifp->if_ioctl = arioctl;
		ifp->if_start = arstart;
		ifp->if_watchdog = arwatchdog;

		sc->ifsppp.pp_flags = PP_KEEPALIVE;

		switch(hc->interface[unit]) {
		default: iface = "UNKNOWN"; break;
		case AR_IFACE_EIA_232: iface = "EIA-232"; break;
		case AR_IFACE_V_35: iface = "EIA-232 or V.35"; break;
		case AR_IFACE_EIA_530: iface = "EIA-530"; break;
		case AR_IFACE_X_21: iface = "X.21"; break;
		case AR_IFACE_COMBO: iface = "COMBO X.21 / EIA-530"; break;
		}

		printf("ar%d: Adapter %d, port %d, interface %s.\n",
			sc->unit,
			hc->cunit,
			sc->subunit,
			iface);

		sppp_attach((struct ifnet *)&sc->ifsppp);
		if_attach(ifp);

		bpfattach(ifp, DLT_PPP, PPP_HEADER_LEN);
#else	/* NETGRAPH */
		/*
		 * we have found a node, make sure our 'type' is availabe.
		 */
		if (ngar_done_init == 0) ngar_init(NULL);
		if (ng_make_node_common(&typestruct, &sc->node) != 0)
			return (1);
		sprintf(sc->nodename, "%s%d", NG_AR_NODE_TYPE, sc->unit);
		if (ng_name_node(sc->node, sc->nodename)) {
			NG_NODE_UNREF(sc->node); /* drop it again */
			return (1);
		}
		NG_NODE_SET_PRIVATE(sc->node, sc);
		callout_handle_init(&sc->handle);
		sc->xmitq.ifq_maxlen = IFQ_MAXLEN;
		sc->xmitq_hipri.ifq_maxlen = IFQ_MAXLEN;
		mtx_init(&sc->xmitq.ifq_mtx, "ar_xmitq", NULL, MTX_DEF);
		mtx_init(&sc->xmitq_hipri.ifq_mtx, "ar_xmitq_hipri", NULL,
		    MTX_DEF);
		sc->running = 0;
#endif	/* NETGRAPH */
	}

	if(hc->bustype == AR_BUS_ISA)
		ARC_SET_OFF(hc->iobase);

	return (0);
}

int
ar_detach(device_t device)
{
	device_t parent = device_get_parent(device);
	struct ar_hardc *hc = device_get_softc(device);

	if (hc->intr_cookie != NULL) {
		if (BUS_TEARDOWN_INTR(parent, device,
			hc->res_irq, hc->intr_cookie) != 0) {
				printf("intr teardown failed.. continuing\n");
		}
		hc->intr_cookie = NULL;
	}

	/*
	 * deallocate any system resources we may have
	 * allocated on behalf of this driver.
	 */
	FREE(hc->sc, M_DEVBUF);
	hc->sc = NULL;
	hc->mem_start = NULL;
	return (ar_deallocate_resources(device));
}

int
ar_allocate_ioport(device_t device, int rid, u_long size)
{
	struct ar_hardc *hc = device_get_softc(device);

	hc->rid_ioport = rid;
	hc->res_ioport = bus_alloc_resource(device, SYS_RES_IOPORT,
			&hc->rid_ioport, 0ul, ~0ul, size, RF_ACTIVE);
	if (hc->res_ioport == NULL) {
		goto errexit;
	}
	return (0);

errexit:
	ar_deallocate_resources(device);
	return (ENXIO);
}

int
ar_allocate_irq(device_t device, int rid, u_long size)
{
	struct ar_hardc *hc = device_get_softc(device);

	hc->rid_irq = rid;
	hc->res_irq = bus_alloc_resource(device, SYS_RES_IRQ,
			&hc->rid_irq, 0ul, ~0ul, 1, RF_SHAREABLE|RF_ACTIVE);
	if (hc->res_irq == NULL) {
		goto errexit;
	}
	return (0);

errexit:
	ar_deallocate_resources(device);
	return (ENXIO);
}

int
ar_allocate_memory(device_t device, int rid, u_long size)
{
	struct ar_hardc *hc = device_get_softc(device);

	hc->rid_memory = rid;
	hc->res_memory = bus_alloc_resource(device, SYS_RES_MEMORY,
			&hc->rid_memory, 0ul, ~0ul, size, RF_ACTIVE);
	if (hc->res_memory == NULL) {
		goto errexit;
	}
	return (0);

errexit:
	ar_deallocate_resources(device);
	return (ENXIO);
}

int
ar_allocate_plx_memory(device_t device, int rid, u_long size)
{
	struct ar_hardc *hc = device_get_softc(device);

	hc->rid_plx_memory = rid;
	hc->res_plx_memory = bus_alloc_resource(device, SYS_RES_MEMORY,
			&hc->rid_plx_memory, 0ul, ~0ul, size, RF_ACTIVE);
	if (hc->res_plx_memory == NULL) {
		goto errexit;
	}
	return (0);

errexit:
	ar_deallocate_resources(device);
	return (ENXIO);
}

int
ar_deallocate_resources(device_t device)
{
	struct ar_hardc *hc = device_get_softc(device);

	if (hc->res_irq != 0) {
		bus_deactivate_resource(device, SYS_RES_IRQ,
			hc->rid_irq, hc->res_irq);
		bus_release_resource(device, SYS_RES_IRQ,
			hc->rid_irq, hc->res_irq);
		hc->res_irq = 0;
	}
	if (hc->res_ioport != 0) {
		bus_deactivate_resource(device, SYS_RES_IOPORT,
			hc->rid_ioport, hc->res_ioport);
		bus_release_resource(device, SYS_RES_IOPORT,
			hc->rid_ioport, hc->res_ioport);
		hc->res_ioport = 0;
	}
	if (hc->res_memory != 0) {
		bus_deactivate_resource(device, SYS_RES_MEMORY,
			hc->rid_memory, hc->res_memory);
		bus_release_resource(device, SYS_RES_MEMORY,
			hc->rid_memory, hc->res_memory);
		hc->res_memory = 0;
	}
	if (hc->res_plx_memory != 0) {
		bus_deactivate_resource(device, SYS_RES_MEMORY,
			hc->rid_plx_memory, hc->res_plx_memory);
		bus_release_resource(device, SYS_RES_MEMORY,
			hc->rid_plx_memory, hc->res_plx_memory);
		hc->res_plx_memory = 0;
	}
	return (0);
}

/*
 * First figure out which SCA gave the interrupt.
 * Process it.
 * See if there is other interrupts pending.
 * Repeat until there is no more interrupts.
 */
static void
arintr(void *arg)
{
	struct ar_hardc *hc = (struct ar_hardc *)arg;
	sca_regs *sca;
	u_char isr0, isr1, isr2, arisr;
	int scano;

	/* XXX Use the PCI interrupt score board register later */
	if(hc->bustype == AR_BUS_PCI)
		arisr = hc->orbase[AR_ISTAT * 4];
	else
		arisr = inb(hc->iobase + AR_ISTAT);

	while(arisr & AR_BD_INT) {
		TRC(printf("arisr = %x\n", arisr));
		if(arisr & AR_INT_0)
			scano = 0;
		else if(arisr & AR_INT_1)
			scano = 1;
		else {
			/* XXX Oops this shouldn't happen. */
			printf("arc%d: Interrupted with no interrupt.\n",
				hc->cunit);
			return;
		}
		sca = hc->sca[scano];

		if(hc->bustype == AR_BUS_ISA)
			ARC_SET_SCA(hc->iobase, scano);

		isr0 = sca->isr0;
		isr1 = sca->isr1;
		isr2 = sca->isr2;

		TRC(printf("arc%d: ARINTR isr0 %x, isr1 %x, isr2 %x\n",
			hc->cunit,
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
		else {
			if(hc->bustype == AR_BUS_PCI)
				arisr = hc->orbase[AR_ISTAT * 4];
			else
				arisr = inb(hc->iobase + AR_ISTAT);
		}
	}

	if(hc->bustype == AR_BUS_ISA)
		ARC_SET_OFF(hc->iobase);
}


/*
 * This will only start the transmitter. It is assumed that the data
 * is already there. It is normally called from arstart() or ar_dmac_intr().
 *
 */
static void
ar_xmit(struct ar_softc *sc)
{
#ifndef NETGRAPH
	struct ifnet *ifp;
#endif /* NETGRAPH */
	dmac_channel *dmac;

#ifndef NETGRAPH
	ifp = &sc->ifsppp.pp_if;
#endif /* NETGRAPH */
	dmac = &sc->sca->dmac[DMAC_TXCH(sc->scachan)];

	if(sc->hc->bustype == AR_BUS_ISA)
		ARC_SET_SCA(sc->hc->iobase, sc->scano);
	dmac->cda = (u_short)(sc->block[sc->txb_next_tx].txdesc & 0xffff);

	dmac->eda = (u_short)(sc->block[sc->txb_next_tx].txeda & 0xffff);
	dmac->dsr = SCA_DSR_DE;

	sc->xmit_busy = 1;

	sc->txb_next_tx++;
	if(sc->txb_next_tx == AR_TX_BLOCKS)
		sc->txb_next_tx = 0;

#ifndef NETGRAPH
	ifp->if_timer = 2; /* Value in seconds. */
#else	/* NETGRAPH */
	sc->out_dog = DOG_HOLDOFF;	/* give ourself some breathing space*/
#endif	/* NETGRAPH */
	if(sc->hc->bustype == AR_BUS_ISA)
		ARC_SET_OFF(sc->hc->iobase);
}

/*
 * This function will be called from the upper level when a user add a
 * packet to be send, and from the interrupt handler after a finished
 * transmit.
 *
 * NOTE: it should run at spl_imp().
 *
 * This function only place the data in the oncard buffers. It does not
 * start the transmition. ar_xmit() does that.
 *
 * Transmitter idle state is indicated by the IFF_OACTIVE flag. The function
 * that clears that should ensure that the transmitter and its DMA is
 * in a "good" idle state.
 */
#ifndef NETGRAPH
static void
arstart(struct ifnet *ifp)
{
	struct ar_softc *sc = ifp->if_softc;
#else	/* NETGRAPH */
static void
arstart(struct ar_softc *sc)
{
#endif	/* NETGRAPH */
	int i, len, tlen;
	struct mbuf *mtx;
	u_char *txdata;
	sca_descriptor *txdesc;
	struct buf_block *blkp;

#ifndef NETGRAPH
	if(!(ifp->if_flags & IFF_RUNNING))
		return;
#else	/* NETGRAPH */
/* XXX */
#endif	/* NETGRAPH */
  
top_arstart:

	/*
	 * See if we have space for more packets.
	 */
	if(sc->txb_inuse == AR_TX_BLOCKS) {
#ifndef NETGRAPH
		ifp->if_flags |= IFF_OACTIVE;	/* yes, mark active */
#else	/* NETGRAPH */
/*XXX*/		/*ifp->if_flags |= IFF_OACTIVE;*/	/* yes, mark active */
#endif /* NETGRAPH */
		return;
	}

#ifndef NETGRAPH
	mtx = sppp_dequeue(ifp);
#else	/* NETGRAPH */
	IF_DEQUEUE(&sc->xmitq_hipri, mtx);
	if (mtx == NULL) {
		IF_DEQUEUE(&sc->xmitq, mtx);
	}
#endif /* NETGRAPH */
	if(!mtx)
		return;

	/*
	 * It is OK to set the memory window outside the loop because
	 * all tx buffers and descriptors are assumed to be in the same
	 * 16K window.
	 */
	if(sc->hc->bustype == AR_BUS_ISA)
		ARC_SET_MEM(sc->hc->iobase, sc->block[0].txdesc);

	/*
	 * We stay in this loop until there is nothing in the
	 * TX queue left or the tx buffer is full.
	 */
	i = 0;
	blkp = &sc->block[sc->txb_new];
	txdesc = (sca_descriptor *)
		(sc->hc->mem_start + (blkp->txdesc & sc->hc->winmsk));
	txdata = (u_char *)(sc->hc->mem_start + (blkp->txstart & sc->hc->winmsk));
	for(;;) {
		len = mtx->m_pkthdr.len;

		TRC(printf("ar%d: ARstart len %u\n", sc->unit, len));

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

#ifndef NETGRAPH
		BPF_MTAP(ifp, mtx);
		m_freem(mtx);
		++sc->ifsppp.pp_if.if_opackets;
#else	/* NETGRAPH */
		m_freem(mtx);
		sc->outbytes += len;
		++sc->opackets;
#endif	/* NETGRAPH */

		/*
		 * Check if we have space for another mbuf.
		 * XXX This is hardcoded. A packet won't be larger
		 * than 3 buffers (3 x 512).
		 */
		if((i + 3) >= blkp->txmax)
			break;

#ifndef NETGRAPH
		mtx = sppp_dequeue(ifp);
#else	/* NETGRAPH */
		IF_DEQUEUE(&sc->xmitq_hipri, mtx);
		if (mtx == NULL) {
			IF_DEQUEUE(&sc->xmitq, mtx);
		}
#endif /* NETGRAPH */
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

#if 0
	printf("ARstart: %p desc->cp %x\n", &txdesc->cp, txdesc->cp);
	printf("ARstart: %p desc->bp %x\n", &txdesc->bp, txdesc->bp);
	printf("ARstart: %p desc->bpb %x\n", &txdesc->bpb, txdesc->bpb);
	printf("ARstart: %p desc->len %x\n", &txdesc->len, txdesc->len);
	printf("ARstart: %p desc->stat %x\n", &txdesc->stat, txdesc->stat);
#endif

	sc->txb_inuse++;
	sc->txb_new++;
	if(sc->txb_new == AR_TX_BLOCKS)
		sc->txb_new = 0;

	if(sc->xmit_busy == 0)
		ar_xmit(sc);

	if(sc->hc->bustype == AR_BUS_ISA)
		ARC_SET_OFF(sc->hc->iobase);

	goto top_arstart;
}

#ifndef	NETGRAPH
static int
arioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	int s, error;
	int was_up, should_be_up;
	struct ar_softc *sc = ifp->if_softc;

	TRC(if_printf(ifp, "arioctl.\n");)

	was_up = ifp->if_flags & IFF_RUNNING;

	error = sppp_ioctl(ifp, cmd, data);
	TRC(if_printf(ifp, "ioctl: ifsppp.pp_flags = %x, if_flags %x.\n", 
		((struct sppp *)ifp)->pp_flags, ifp->if_flags);)
	if(error)
		return (error);

	if((cmd != SIOCSIFFLAGS) && cmd != (SIOCSIFADDR))
		return (0);

	TRC(if_printf(ifp, "arioctl %s.\n",
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
	return (0);
}
#endif	/* NETGRAPH */

/*
 * This is to catch lost tx interrupts.
 */
static void
#ifndef	NETGRAPH
arwatchdog(struct ifnet *ifp)
{
	struct ar_softc *sc = ifp->if_softc;
#else	/* NETGRAPH */
arwatchdog(struct ar_softc *sc)
{
#endif	/* NETGRAPH */
	msci_channel *msci = &sc->sca->msci[sc->scachan];

#ifndef	NETGRAPH
	if(!(ifp->if_flags & IFF_RUNNING))
		return;
#endif	/* NETGRAPH */

	if(sc->hc->bustype == AR_BUS_ISA)
		ARC_SET_SCA(sc->hc->iobase, sc->scano);

	/* XXX if(sc->ifsppp.pp_if.if_flags & IFF_DEBUG) */
		printf("ar%d: transmit failed, "
			"ST0 %x, ST1 %x, ST3 %x, DSR %x.\n",
			sc->unit,
			msci->st0,
			msci->st1,
			msci->st3,
			sc->sca->dmac[DMAC_TXCH(sc->scachan)].dsr);

	if(msci->st1 & SCA_ST1_UDRN) {
		msci->cmd = SCA_CMD_TXABORT;
		msci->cmd = SCA_CMD_TXENABLE;
		msci->st1 = SCA_ST1_UDRN;
	}

	sc->xmit_busy = 0;
#ifndef	NETGRAPH
	ifp->if_flags &= ~IFF_OACTIVE;
#else	/* NETGRAPH */
	/* XXX ifp->if_flags &= ~IFF_OACTIVE; */
#endif	/* NETGRAPH */

	if(sc->txb_inuse && --sc->txb_inuse)
		ar_xmit(sc);

#ifndef	NETGRAPH
	arstart(ifp);
#else	/* NETGRAPH */
	arstart(sc);
#endif	/* NETGRAPH */
}

static void
ar_up(struct ar_softc *sc)
{
	sca_regs *sca;
	msci_channel *msci;

	sca = sc->sca;
	msci = &sca->msci[sc->scachan];

	TRC(printf("ar%d: sca %p, msci %p, ch %d\n",
		sc->unit, sca, msci, sc->scachan));

	/*
	 * Enable transmitter and receiver.
	 * Raise DTR and RTS.
	 * Enable interrupts.
	 */
	if(sc->hc->bustype == AR_BUS_ISA)
		ARC_SET_SCA(sc->hc->iobase, sc->scano);

	/* XXX
	 * What about using AUTO mode in msci->md0 ???
	 * And what about CTS/DCD etc... ?
	 */
	if(sc->hc->handshake & AR_SHSK_RTS)
		msci->ctl &= ~SCA_CTL_RTS;
	if(sc->hc->handshake & AR_SHSK_DTR) {
		sc->hc->txc_dtr[sc->scano] &= sc->scachan ? 
			~AR_TXC_DTR_DTR1 : ~AR_TXC_DTR_DTR0;
		if(sc->hc->bustype == AR_BUS_PCI)
			sc->hc->orbase[sc->hc->txc_dtr_off[sc->scano]] =
				sc->hc->txc_dtr[sc->scano];
		else
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
	if(sc->hc->bustype == AR_BUS_ISA)
		inb(sc->hc->iobase + AR_ID_5); /* XXX slow it down a bit. */
	msci->cmd = SCA_CMD_TXENABLE;

	if(sc->hc->bustype == AR_BUS_ISA)
		ARC_SET_OFF(sc->hc->iobase);
#ifdef	NETGRAPH
	untimeout(ngar_watchdog_frame, sc, sc->handle);
	sc->handle = timeout(ngar_watchdog_frame, sc, hz);
	sc->running = 1;
#endif	/* NETGRAPH */
}

static void
ar_down(struct ar_softc *sc)
{
	sca_regs *sca;
	msci_channel *msci;

	sca = sc->sca;
	msci = &sca->msci[sc->scachan];

#ifdef	NETGRAPH
	untimeout(ngar_watchdog_frame, sc, sc->handle);
	sc->running = 0;
#endif	/* NETGRAPH */
	/*
	 * Disable transmitter and receiver.
	 * Lower DTR and RTS.
	 * Disable interrupts.
	 */
	if(sc->hc->bustype == AR_BUS_ISA)
		ARC_SET_SCA(sc->hc->iobase, sc->scano);
	msci->cmd = SCA_CMD_RXDISABLE;
	if(sc->hc->bustype == AR_BUS_ISA)
		inb(sc->hc->iobase + AR_ID_5); /* XXX slow it down a bit. */
	msci->cmd = SCA_CMD_TXDISABLE;

	if(sc->hc->handshake & AR_SHSK_RTS)
		msci->ctl |= SCA_CTL_RTS;
	if(sc->hc->handshake & AR_SHSK_DTR) {
		sc->hc->txc_dtr[sc->scano] |= sc->scachan ? 
			AR_TXC_DTR_DTR1 : AR_TXC_DTR_DTR0;
		if(sc->hc->bustype == AR_BUS_PCI)
			sc->hc->orbase[sc->hc->txc_dtr_off[sc->scano]] =
				sc->hc->txc_dtr[sc->scano];
		else
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

	if(sc->hc->bustype == AR_BUS_ISA)
		ARC_SET_OFF(sc->hc->iobase);
}

static int
ar_read_pim_iface(volatile struct ar_hardc *hc, int channel)
{
	int ctype, i, val, x;
	volatile u_char *pimctrl;

	ctype = 0;
	val = 0;

	pimctrl = hc->orbase + AR_PIMCTRL;

	/* Reset the PIM */
	*pimctrl = 0x00;
	*pimctrl = AR_PIM_STROBE;

	/* Check if there is a PIM */
	*pimctrl = 0x00;
	*pimctrl = AR_PIM_READ;
	x = *pimctrl;
	TRC(printf("x = %x", x));
	if(x & AR_PIM_DATA) {
		printf("No PIM installed\n");
		return (AR_IFACE_UNKNOWN);
	}

	x = (x >> 1) & 0x01;
	val |= x << 0;

	/* Now read the next 15 bits */
	for(i = 1; i < 16; i++) {
		*pimctrl = AR_PIM_READ;
		*pimctrl = AR_PIM_READ | AR_PIM_STROBE;
		x = *pimctrl;
		TRC(printf(" %x ", x));
		x = (x >> 1) & 0x01;
		val |= x << i;
		if(i == 8 && (val & 0x000f) == 0x0004) {
			int ii;
			
			/* Start bit */
			*pimctrl = AR_PIM_A2D_DOUT | AR_PIM_A2D_STROBE;
			*pimctrl = AR_PIM_A2D_DOUT;

			/* Mode bit */
			*pimctrl = AR_PIM_A2D_DOUT | AR_PIM_A2D_STROBE;
			*pimctrl = AR_PIM_A2D_DOUT;

			/* Sign bit */
			*pimctrl = AR_PIM_A2D_DOUT | AR_PIM_A2D_STROBE;
			*pimctrl = AR_PIM_A2D_DOUT;

			/* Select channel */
			*pimctrl = AR_PIM_A2D_STROBE | ((channel & 2) << 2);
			*pimctrl = ((channel & 2) << 2);
			*pimctrl = AR_PIM_A2D_STROBE | ((channel & 1) << 3);
			*pimctrl = ((channel & 1) << 3);

			*pimctrl = AR_PIM_A2D_STROBE;

			x = *pimctrl;
			if(x & AR_PIM_DATA)
				printf("\nOops A2D start bit not zero (%X)\n", x);

			for(ii = 7; ii >= 0; ii--) {
				*pimctrl = 0x00;
				*pimctrl = AR_PIM_A2D_STROBE;
				x = *pimctrl;
				if(x & AR_PIM_DATA)
					ctype |= 1 << ii;
			}
		}
	}
	TRC(printf("\nPIM val %x, ctype %x, %d\n", val, ctype, ctype));
	*pimctrl = AR_PIM_MODEG;
	*pimctrl = AR_PIM_MODEG | AR_PIM_AUTO_LED;
	if(ctype > 255)
		return (AR_IFACE_UNKNOWN);
	if(ctype > 239)
		return (AR_IFACE_V_35);
	if(ctype > 207)
		return (AR_IFACE_EIA_232);
	if(ctype > 178)
		return (AR_IFACE_X_21);
	if(ctype > 150)
		return (AR_IFACE_EIA_530);
	if(ctype > 25)
		return (AR_IFACE_UNKNOWN);
	if(ctype > 7)
		return (AR_IFACE_LOOPBACK);
	return (AR_IFACE_UNKNOWN);
}

/*
 * Initialize the card, allocate memory for the ar_softc structures
 * and fill in the pointers.
 */
static void
arc_init(struct ar_hardc *hc)
{
	struct ar_softc *sc;
	int x;
	u_int chanmem;
	u_int bufmem;
	u_int next;
	u_int descneeded;
	u_char isr, mar;

	MALLOC(sc, struct ar_softc *, hc->numports * sizeof(struct ar_softc),
		M_DEVBUF, M_WAITOK | M_ZERO);
	if (sc == NULL)
		return;
	hc->sc = sc;

	hc->txc_dtr[0] = AR_TXC_DTR_NOTRESET |
			 AR_TXC_DTR_DTR0 | AR_TXC_DTR_DTR1;
	hc->txc_dtr[1] = AR_TXC_DTR_DTR0 | AR_TXC_DTR_DTR1;
	hc->txc_dtr_off[0] = AR_TXC_DTR0;
	hc->txc_dtr_off[1] = AR_TXC_DTR2;
	if(hc->bustype == AR_BUS_PCI) {
		hc->txc_dtr_off[0] *= 4;
		hc->txc_dtr_off[1] *= 4;
	}

	/*
	 * reset the card and wait at least 1uS.
	 */
	if(hc->bustype == AR_BUS_PCI)
		hc->orbase[AR_TXC_DTR0 * 4] = ~AR_TXC_DTR_NOTRESET &
			hc->txc_dtr[0];
	else
		outb(hc->iobase + AR_TXC_DTR0, ~AR_TXC_DTR_NOTRESET &
			hc->txc_dtr[0]);
	DELAY(2);
	if(hc->bustype == AR_BUS_PCI)
		hc->orbase[AR_TXC_DTR0 * 4] = hc->txc_dtr[0];
	else
		outb(hc->iobase + AR_TXC_DTR0, hc->txc_dtr[0]);

	if(hc->bustype == AR_BUS_ISA) {
		/*
		 * Configure the card.
		 * Mem address, irq, 
		 */
		mar = kvtop(hc->mem_start) >> 16;
		isr = irqtable[hc->isa_irq] << 1;
		if(isr == 0)
			printf("ar%d: Warning illegal interrupt %d\n",
				hc->cunit, hc->isa_irq);
		isr = isr | ((kvtop(hc->mem_start) & 0xc000) >> 10);

		hc->sca[0] = (sca_regs *)hc->mem_start;
		hc->sca[1] = (sca_regs *)hc->mem_start;

		outb(hc->iobase + AR_MEM_SEL, mar);
		outb(hc->iobase + AR_INT_SEL, isr | AR_INTS_CEN);
	}

	if(hc->bustype == AR_BUS_PCI && hc->interface[0] == AR_IFACE_PIM)
		for(x = 0; x < hc->numports; x++)
			hc->interface[x] = ar_read_pim_iface(hc, x);

	/*
	 * Set the TX clock direction and enable TX.
	 */
	for(x=0;x<hc->numports;x++) {
		switch(hc->interface[x]) {
		case AR_IFACE_V_35:
			hc->txc_dtr[x / NCHAN] |= (x % NCHAN == 0) ?
			    AR_TXC_DTR_TX0 : AR_TXC_DTR_TX1;
			hc->txc_dtr[x / NCHAN] |= (x % NCHAN == 0) ?
			    AR_TXC_DTR_TXCS0 : AR_TXC_DTR_TXCS1;
			break;
		case AR_IFACE_EIA_530:
		case AR_IFACE_COMBO:
		case AR_IFACE_X_21:
			hc->txc_dtr[x / NCHAN] |= (x % NCHAN == 0) ?
			    AR_TXC_DTR_TX0 : AR_TXC_DTR_TX1;
			break;
		}
	}

	if(hc->bustype == AR_BUS_PCI)
		hc->orbase[AR_TXC_DTR0 * 4] = hc->txc_dtr[0];
	else
		outb(hc->iobase + AR_TXC_DTR0, hc->txc_dtr[0]);
	if(hc->numports > NCHAN) {
		if(hc->bustype == AR_BUS_PCI)
			hc->orbase[AR_TXC_DTR2 * 4] = hc->txc_dtr[1];
		else
			outb(hc->iobase + AR_TXC_DTR2, hc->txc_dtr[1]);
	}

	chanmem = hc->memsize / hc->numports;
	next = 0;

	for(x=0;x<hc->numports;x++, sc++) {
		int blk;

		sc->sca = hc->sca[x / NCHAN];

		for(blk = 0; blk < AR_TX_BLOCKS; blk++) {
			sc->block[blk].txdesc = next;
			bufmem = (16 * 1024) / AR_TX_BLOCKS;
			descneeded = bufmem / AR_BUF_SIZ;
			sc->block[blk].txstart = sc->block[blk].txdesc +
				((((descneeded * sizeof(sca_descriptor)) /
					AR_BUF_SIZ) + 1) * AR_BUF_SIZ);
			sc->block[blk].txend = next + bufmem;
			sc->block[blk].txmax =
				(sc->block[blk].txend - sc->block[blk].txstart)
				/ AR_BUF_SIZ;
			next += bufmem;

			TRC(printf("ar%d: blk %d: txdesc %x, txstart %x, "
				   "txend %x, txmax %d\n",
				   x,
				   blk,
				   sc->block[blk].txdesc,
				   sc->block[blk].txstart,
				   sc->block[blk].txend,
				   sc->block[blk].txmax));
		}

		sc->rxdesc = next;
		bufmem = chanmem - (bufmem * AR_TX_BLOCKS);
		descneeded = bufmem / AR_BUF_SIZ;
		sc->rxstart = sc->rxdesc +
				((((descneeded * sizeof(sca_descriptor)) /
					AR_BUF_SIZ) + 1) * AR_BUF_SIZ);
		sc->rxend = next + bufmem;
		sc->rxmax = (sc->rxend - sc->rxstart) / AR_BUF_SIZ;
		next += bufmem;
		TRC(printf("ar%d: rxdesc %x, rxstart %x, "
			   "rxend %x, rxmax %d\n",
			   x, sc->rxdesc, sc->rxstart, sc->rxend, sc->rxmax));
	}

	if(hc->bustype == AR_BUS_PCI)
		hc->orbase[AR_PIMCTRL] = AR_PIM_MODEG | AR_PIM_AUTO_LED;
}


/*
 * The things done here are channel independent.
 *
 *   Configure the sca waitstates.
 *   Configure the global interrupt registers.
 *   Enable master dma enable.
 */
static void
ar_init_sca(struct ar_hardc *hc, int scano)
{
	sca_regs *sca;

	sca = hc->sca[scano];
	if(hc->bustype == AR_BUS_ISA)
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


	/*
	 * Set the DMA channel priority to rotate between
	 * all four channels.
	 *
	 * Enable all dma channels.
	 */
	if(hc->bustype == AR_BUS_PCI) {
		u_char *t;

		/*
		 * Stupid problem with the PCI interface chip that break
		 * things.
		 * XXX
		 */
		t = (u_char *)sca;
		t[AR_PCI_SCA_PCR] = SCA_PCR_PR2;
		t[AR_PCI_SCA_DMER] = SCA_DMER_EN;
	} else {
		sca->pcr = SCA_PCR_PR2;
		sca->dmer = SCA_DMER_EN;
	}
}


/*
 * Configure the msci
 *
 * NOTE: The serial port configuration is hardcoded at the moment.
 */
static void
ar_init_msci(struct ar_softc *sc)
{
	msci_channel *msci;

	msci = &sc->sca->msci[sc->scachan];

	if(sc->hc->bustype == AR_BUS_ISA)
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
	msci->ctl = SCA_CTL_IDLPAT | SCA_CTL_UDRNC | SCA_CTL_RTS;

	/*
	 * For now all interfaces are programmed to use the RX clock for
	 * the TX clock.
	 */
	switch(sc->hc->interface[sc->subunit]) {
	case AR_IFACE_V_35:
		msci->rxs = SCA_RXS_CLK_RXC0 | SCA_RXS_DIV1;
		msci->txs = SCA_TXS_CLK_TXC | SCA_TXS_DIV1;
		break;
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
static void
ar_init_rx_dmac(struct ar_softc *sc)
{
	dmac_channel *dmac;
	sca_descriptor *rxd;
	u_int rxbuf;
	u_int rxda;
	u_int rxda_d;
	int x = 0;

	dmac = &sc->sca->dmac[DMAC_RXCH(sc->scachan)];

	if(sc->hc->bustype == AR_BUS_ISA)
		ARC_SET_MEM(sc->hc->iobase, sc->rxdesc);

	rxd = (sca_descriptor *)(sc->hc->mem_start + (sc->rxdesc&sc->hc->winmsk));
	rxda_d = (u_int)sc->hc->mem_start - (sc->rxdesc & ~sc->hc->winmsk);

	for(rxbuf=sc->rxstart;rxbuf<sc->rxend;rxbuf += AR_BUF_SIZ, rxd++) {
		rxda = (u_int)&rxd[1] - rxda_d;
		rxd->cp = (u_short)(rxda & 0xfffful);

		x++;
		if(x < 6)
		TRC(printf("Descrp %p, data pt %x, data %x, ",
			rxd, rxda, rxbuf));

		rxd->bp = (u_short)(rxbuf & 0xfffful);
		rxd->bpb = (u_char)((rxbuf >> 16) & 0xff);
		rxd->len = 0;
		rxd->stat = 0xff; /* The sca write here when it is finished. */

		if(x < 6)
		TRC(printf("bpb %x, bp %x.\n", rxd->bpb, rxd->bp));
	}
	rxd--;
	rxd->cp = (u_short)(sc->rxdesc & 0xfffful);

	sc->rxhind = 0;

	if(sc->hc->bustype == AR_BUS_ISA)
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
static void
ar_init_tx_dmac(struct ar_softc *sc)
{
	dmac_channel *dmac;
	struct buf_block *blkp;
	int blk;
	sca_descriptor *txd;
	u_int txbuf;
	u_int txda;
	u_int txda_d;

	dmac = &sc->sca->dmac[DMAC_TXCH(sc->scachan)];

	if(sc->hc->bustype == AR_BUS_ISA)
		ARC_SET_MEM(sc->hc->iobase, sc->block[0].txdesc);

	for(blk = 0; blk < AR_TX_BLOCKS; blk++) {
		blkp = &sc->block[blk];
		txd = (sca_descriptor *)(sc->hc->mem_start +
					(blkp->txdesc&sc->hc->winmsk));
		txda_d = (u_int)sc->hc->mem_start -
				(blkp->txdesc & ~sc->hc->winmsk);

		txbuf=blkp->txstart;
		for(;txbuf<blkp->txend;txbuf += AR_BUF_SIZ, txd++) {
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
		txd->cp = (u_short)(blkp->txdesc & 0xfffful);

		blkp->txtail = (u_int)txd - (u_int)sc->hc->mem_start;
		TRC(printf("TX Descriptors start %x, end %x.\n",
			blkp->txdesc,
			blkp->txtail));
	}

	if(sc->hc->bustype == AR_BUS_ISA)
		ARC_SET_SCA(sc->hc->iobase, sc->scano);

	dmac->dsr = 0; /* Disable DMA */
	dmac->dcr = SCA_DCR_ABRT;
	dmac->dmr = SCA_DMR_TMOD | SCA_DMR_NF;
	dmac->dir = SCA_DIR_EOT | SCA_DIR_BOF | SCA_DIR_COF;

	dmac->sarb = (u_char)((sc->block[0].txdesc >> 16) & 0xff);
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
ar_packet_avail(struct ar_softc *sc,
		    int *len,
		    u_char *rxstat)
{
	dmac_channel *dmac;
	sca_descriptor *rxdesc;
	sca_descriptor *endp;
	sca_descriptor *cda;

	if(sc->hc->bustype == AR_BUS_ISA)
		ARC_SET_SCA(sc->hc->iobase, sc->scano);
	dmac = &sc->sca->dmac[DMAC_RXCH(sc->scachan)];
	cda = (sca_descriptor *)(sc->hc->mem_start +
	      ((((u_int)dmac->sarb << 16) + dmac->cda) & sc->hc->winmsk));

	if(sc->hc->bustype == AR_BUS_ISA)
		ARC_SET_MEM(sc->hc->iobase, sc->rxdesc);
	rxdesc = (sca_descriptor *)
			(sc->hc->mem_start + (sc->rxdesc & sc->hc->winmsk));
	endp = rxdesc;
	rxdesc = &rxdesc[sc->rxhind];
	endp = &endp[sc->rxmax];

	*len = 0;

	while(rxdesc != cda) {
		*len += rxdesc->len;

		if(rxdesc->stat & SCA_DESC_EOM) {
			*rxstat = rxdesc->stat;
			TRC(printf("ar%d: PKT AVAIL len %d, %x.\n",
				sc->unit, *len, *rxstat));
			return (1);
		}

		rxdesc++;
		if(rxdesc == endp)
			rxdesc = (sca_descriptor *)
			       (sc->hc->mem_start + (sc->rxdesc & sc->hc->winmsk));
	}

	*len = 0;
	*rxstat = 0;
	return (0);
}


/*
 * Copy a packet from the on card memory into a provided mbuf.
 * Take into account that buffers wrap and that a packet may
 * be larger than a buffer.
 */
static void 
ar_copy_rxbuf(struct mbuf *m,
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
			(sc->hc->mem_start + (sc->rxdesc & sc->hc->winmsk));
	rxdesc = &rxdesc[sc->rxhind];

	while(len) {
		tlen = (len < AR_BUF_SIZ) ? len : AR_BUF_SIZ;
		if(sc->hc->bustype == AR_BUS_ISA)
			ARC_SET_MEM(sc->hc->iobase, rxdata);
		bcopy(sc->hc->mem_start + (rxdata & sc->hc->winmsk), 
			mtod(m, caddr_t) + off,
			tlen);

		off += tlen;
		len -= tlen;

		if(sc->hc->bustype == AR_BUS_ISA)
			ARC_SET_MEM(sc->hc->iobase, sc->rxdesc);
		rxdesc->len = 0;
		rxdesc->stat = 0xff;

		rxdata += AR_BUF_SIZ;
		rxdesc++;
		if(rxdata == rxmax) {
			rxdata = sc->rxstart;
			rxdesc = (sca_descriptor *)
				(sc->hc->mem_start + (sc->rxdesc & sc->hc->winmsk));
		}
	}
}

/*
 * If single is set, just eat a packet. Otherwise eat everything up to
 * where cda points. Update pointers to point to the next packet.
 */
static void
ar_eat_packet(struct ar_softc *sc, int single)
{
	dmac_channel *dmac;
	sca_descriptor *rxdesc;
	sca_descriptor *endp;
	sca_descriptor *cda;
	int loopcnt = 0;
	u_char stat;

	if(sc->hc->bustype == AR_BUS_ISA)
		ARC_SET_SCA(sc->hc->iobase, sc->scano);
	dmac = &sc->sca->dmac[DMAC_RXCH(sc->scachan)];
	cda = (sca_descriptor *)(sc->hc->mem_start +
	      ((((u_int)dmac->sarb << 16) + dmac->cda) & sc->hc->winmsk));

	/*
	 * Loop until desc->stat == (0xff || EOM)
	 * Clear the status and length in the descriptor.
	 * Increment the descriptor.
	 */
	if(sc->hc->bustype == AR_BUS_ISA)
		ARC_SET_MEM(sc->hc->iobase, sc->rxdesc);
	rxdesc = (sca_descriptor *)
		(sc->hc->mem_start + (sc->rxdesc & sc->hc->winmsk));
	endp = rxdesc;
	rxdesc = &rxdesc[sc->rxhind];
	endp = &endp[sc->rxmax];

	while(rxdesc != cda) {
		loopcnt++;
		if(loopcnt > sc->rxmax) {
			printf("ar%d: eat pkt %d loop, cda %p, "
			       "rxdesc %p, stat %x.\n",
			       sc->unit,
			       loopcnt,
			       (void *)cda,
			       (void *)rxdesc,
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
			       (sc->hc->mem_start + (sc->rxdesc & sc->hc->winmsk));
			sc->rxhind = 0;
		}

		if(single && (stat == SCA_DESC_EOM))
			break;
	}

	/*
	 * Update the eda to the previous descriptor.
	 */
	if(sc->hc->bustype == AR_BUS_ISA)
		ARC_SET_SCA(sc->hc->iobase, sc->scano);

	rxdesc = (sca_descriptor *)sc->rxdesc;
	rxdesc = &rxdesc[(sc->rxhind + sc->rxmax - 2 ) % sc->rxmax];

	sc->sca->dmac[DMAC_RXCH(sc->scachan)].eda = 
			(u_short)((u_int)rxdesc & 0xffff);
}


/*
 * While there is packets available in the rx buffer, read them out
 * into mbufs and ship them off.
 */
static void
ar_get_packets(struct ar_softc *sc)
{
	sca_descriptor *rxdesc;
	struct mbuf *m = NULL;
	int i;
	int len;
	u_char rxstat;
#ifdef NETGRAPH
	int error;
#endif

	while(ar_packet_avail(sc, &len, &rxstat)) {
		TRC(printf("apa: len %d, rxstat %x\n", len, rxstat));
		if(((rxstat & SCA_DESC_ERRORS) == 0) && (len < MCLBYTES)) {
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if(m == NULL) {
				/* eat packet if get mbuf fail!! */
				ar_eat_packet(sc, 1);
				continue;
			}
#ifndef NETGRAPH
			m->m_pkthdr.rcvif = &sc->ifsppp.pp_if;
#else	/* NETGRAPH */
			m->m_pkthdr.rcvif = NULL;
			sc->inbytes += len;
			sc->inlast = 0;
#endif	/* NETGRAPH */
			m->m_pkthdr.len = m->m_len = len;
			if(len > MHLEN) {
				MCLGET(m, M_DONTWAIT);
				if((m->m_flags & M_EXT) == 0) {
					m_freem(m);
					ar_eat_packet(sc, 1);
					continue;
				}
			}
			ar_copy_rxbuf(m, sc, len);
#ifndef NETGRAPH
			BPF_MTAP(&sc->ifsppp.pp_if, m);
			sppp_input(&sc->ifsppp.pp_if, m);
			sc->ifsppp.pp_if.if_ipackets++;
#else	/* NETGRAPH */
			NG_SEND_DATA_ONLY(error, sc->hook, m);
			sc->ipackets++;
#endif	/* NETGRAPH */

			/*
			 * Update the eda to the previous descriptor.
			 */
			i = (len + AR_BUF_SIZ - 1) / AR_BUF_SIZ;
			sc->rxhind = (sc->rxhind + i) % sc->rxmax;

			if(sc->hc->bustype == AR_BUS_ISA)
				ARC_SET_SCA(sc->hc->iobase, sc->scano);

			rxdesc = (sca_descriptor *)sc->rxdesc;
			rxdesc =
			     &rxdesc[(sc->rxhind + sc->rxmax - 2 ) % sc->rxmax];

			sc->sca->dmac[DMAC_RXCH(sc->scachan)].eda = 
				(u_short)((u_int)rxdesc & 0xffff);
		} else {
			int tries = 5;

			while((rxstat == 0xff) && --tries)
				ar_packet_avail(sc, &len, &rxstat);

			/*
			 * It look like we get an interrupt early
			 * sometimes and then the status is not
			 * filled in yet.
			 */
			if(tries && (tries != 5))
				continue;

			ar_eat_packet(sc, 1);

#ifndef	NETGRAPH
			sc->ifsppp.pp_if.if_ierrors++;
#else	/* NETGRAPH */
			sc->ierrors[0]++;
#endif	/* NETGRAPH */

			if(sc->hc->bustype == AR_BUS_ISA)
				ARC_SET_SCA(sc->hc->iobase, sc->scano);

			TRCL(printf("ar%d: Receive error chan %d, "
					"stat %x, msci st3 %x,"
					"rxhind %d, cda %x, eda %x.\n",
					sc->unit,
					sc->scachan, 
					rxstat,
					sc->sca->msci[sc->scachan].st3,
					sc->rxhind,
					sc->sca->dmac[
						DMAC_RXCH(sc->scachan)].cda,
					sc->sca->dmac[
						DMAC_RXCH(sc->scachan)].eda));
		}
	}
}


/*
 * All DMA interrupts come here.
 *
 * Each channel has two interrupts.
 * Interrupt A for errors and Interrupt B for normal stuff like end
 * of transmit or receive dmas.
 */
static void
ar_dmac_intr(struct ar_hardc *hc, int scano, u_char isr1)
{
	u_char dsr;
	u_char dotxstart = isr1;
	int mch;
	struct ar_softc *sc;
	sca_regs *sca;
	dmac_channel *dmac;

	sca = hc->sca[scano];
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

		/*
		 * Transmit channel
		 */
		if(isr1 & 0x0C) {
			dmac = &sca->dmac[DMAC_TXCH(mch)];

			if(hc->bustype == AR_BUS_ISA)
				ARC_SET_SCA(hc->iobase, scano);

			dsr = dmac->dsr;
			dmac->dsr = dsr;

			/* Counter overflow */
			if(dsr & SCA_DSR_COF) {
				printf("ar%d: TX DMA Counter overflow, "
					"txpacket no %lu.\n",
					sc->unit,
#ifndef	NETGRAPH
					sc->ifsppp.pp_if.if_opackets);
				sc->ifsppp.pp_if.if_oerrors++;
#else	/* NETGRAPH */
					sc->opackets);
				sc->oerrors++;
#endif	/* NETGRAPH */
			}

			/* Buffer overflow */
			if(dsr & SCA_DSR_BOF) {
				printf("ar%d: TX DMA Buffer overflow, "
					"txpacket no %lu, dsr %02x, "
					"cda %04x, eda %04x.\n",
					sc->unit,
#ifndef	NETGRAPH
					sc->ifsppp.pp_if.if_opackets,
#else	/* NETGRAPH */
					sc->opackets,
#endif	/* NETGRAPH */
					dsr,
					dmac->cda,
					dmac->eda);
#ifndef	NETGRAPH
				sc->ifsppp.pp_if.if_oerrors++;
#else	/* NETGRAPH */
				sc->oerrors++;
#endif	/* NETGRAPH */
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
				sc->xmit_busy = 0;
#ifndef	NETGRAPH
				sc->ifsppp.pp_if.if_flags &= ~IFF_OACTIVE;
				sc->ifsppp.pp_if.if_timer = 0;
#else	/* NETGRAPH */
			/* XXX 	c->ifsppp.pp_if.if_flags &= ~IFF_OACTIVE; */
				sc->out_dog = 0; /* XXX */
#endif	/* NETGRAPH */

				if(sc->txb_inuse && --sc->txb_inuse)
					ar_xmit(sc);
			}
		}

		/*
		 * Receive channel
		 */
		if(isr1 & 0x03) {
			dmac = &sca->dmac[DMAC_RXCH(mch)];

			if(hc->bustype == AR_BUS_ISA)
				ARC_SET_SCA(hc->iobase, scano);

			dsr = dmac->dsr;
			dmac->dsr = dsr;

			TRC(printf("AR: RX DSR %x\n", dsr));

			/* End of frame */
			if(dsr & SCA_DSR_EOM) {
				TRC(int tt = sc->ifsppp.pp_if.if_ipackets;)
				TRC(int ind = sc->rxhind;)

				ar_get_packets(sc);
#ifndef	NETGRAPH
#define	IPACKETS sc->ifsppp.pp_if.if_ipackets
#else	/* NETGRAPH */
#define	IPACKETS sc->ipackets
#endif	/* NETGRAPH */
				TRC(if(tt == IPACKETS) {
					sca_descriptor *rxdesc;
					int i;

					if(hc->bustype == AR_BUS_ISA)
						ARC_SET_SCA(hc->iobase, scano);
					printf("AR: RXINTR isr1 %x, dsr %x, "
					       "no data %d pkts, orxhind %d.\n",
					       dotxstart,
					       dsr,
					       tt,
					       ind);
					printf("AR: rxdesc %x, rxstart %x, "
					       "rxend %x, rxhind %d, "
					       "rxmax %d.\n",
					       sc->rxdesc,
					       sc->rxstart,
					       sc->rxend,
					       sc->rxhind,
					       sc->rxmax);
					printf("AR: cda %x, eda %x.\n",
					       dmac->cda,
					       dmac->eda);

					if(sc->hc->bustype == AR_BUS_ISA)
						ARC_SET_MEM(sc->hc->iobase,
						    sc->rxdesc);
					rxdesc = (sca_descriptor *)
						 (sc->hc->mem_start +
						  (sc->rxdesc & sc->hc->winmsk));
					rxdesc = &rxdesc[sc->rxhind];
					for(i=0;i<3;i++,rxdesc++)
						printf("AR: rxdesc->stat %x, "
							"len %d.\n",
							rxdesc->stat,
							rxdesc->len);
				})
			}

			/* Counter overflow */
			if(dsr & SCA_DSR_COF) {
				printf("ar%d: RX DMA Counter overflow, "
					"rxpkts %lu.\n",
					sc->unit,
#ifndef	NETGRAPH
					sc->ifsppp.pp_if.if_ipackets);
				sc->ifsppp.pp_if.if_ierrors++;
#else	/* NETGRAPH */
					sc->ipackets);
				sc->ierrors[1]++;
#endif	/* NETGRAPH */
			}

			/* Buffer overflow */
			if(dsr & SCA_DSR_BOF) {
				if(hc->bustype == AR_BUS_ISA)
					ARC_SET_SCA(hc->iobase, scano);
				printf("ar%d: RX DMA Buffer overflow, "
					"rxpkts %lu, rxind %d, "
					"cda %x, eda %x, dsr %x.\n",
					sc->unit,
#ifndef	NETGRAPH
					sc->ifsppp.pp_if.if_ipackets,
#else	/* NETGRAPH */
					sc->ipackets,
#endif	/* NETGRAPH */
					sc->rxhind,
					dmac->cda,
					dmac->eda,
					dsr);
				/*
				 * Make sure we eat as many as possible.
				 * Then get the system running again.
				 */
				ar_eat_packet(sc, 0);
#ifndef	NETGRAPH
				sc->ifsppp.pp_if.if_ierrors++;
#else	/* NETGRAPH */
				sc->ierrors[2]++;
#endif	/* NETGRAPH */
				if(hc->bustype == AR_BUS_ISA)
					ARC_SET_SCA(hc->iobase, scano);
				sca->msci[mch].cmd = SCA_CMD_RXMSGREJ;
				dmac->dsr = SCA_DSR_DE;

				TRC(printf("ar%d: RX DMA Buffer overflow, "
					"rxpkts %lu, rxind %d, "
					"cda %x, eda %x, dsr %x. After\n",
					sc->unit,
					sc->ifsppp.pp_if.if_ipackets,
					sc->rxhind,
					dmac->cda,
					dmac->eda,
					dmac->dsr);)
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
#ifndef	NETGRAPH
					sc->ifsppp.pp_if.if_ipackets);
				sc->ifsppp.pp_if.if_ierrors++;
#else	/* NETGRAPH */
					sc->ipackets);
				sc->ierrors[3]++;
#endif	/* NETGRAPH */
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
			sc = &hc->sc[mch + (NCHAN * scano)];
#ifndef	NETGRAPH
			arstart(&sc->ifsppp.pp_if);
#else	/* NETGRAPH */
			arstart(sc);
#endif	/* NETGRAPH */
		}
		dotxstart >>= 4;
	}
}

static void
ar_msci_intr(struct ar_hardc *hc, int scano, u_char isr0)
{
	printf("arc%d: ARINTR: MSCI\n", hc->cunit);
}

static void
ar_timer_intr(struct ar_hardc *hc, int scano, u_char isr2)
{
	printf("arc%d: ARINTR: TIMER\n", hc->cunit);
}


#ifdef	NETGRAPH
/*****************************************
 * Device timeout/watchdog routine.
 * called once per second.
 * checks to see that if activity was expected, that it hapenned.
 * At present we only look to see if expected output was completed.
 */
static void
ngar_watchdog_frame(void * arg)
{
	struct ar_softc * sc = arg;
	int s;
	int	speed;

	if(sc->running == 0)
		return; /* if we are not running let timeouts die */
	/*
	 * calculate the apparent throughputs 
	 *  XXX a real hack
	 */
	s = splimp();
	speed = sc->inbytes - sc->lastinbytes;
	sc->lastinbytes = sc->inbytes;
	if ( sc->inrate < speed )
		sc->inrate = speed;
	speed = sc->outbytes - sc->lastoutbytes;
	sc->lastoutbytes = sc->outbytes;
	if ( sc->outrate < speed )
		sc->outrate = speed;
	sc->inlast++;
	splx(s);

	if ((sc->inlast > QUITE_A_WHILE)
	&& (sc->out_deficit > LOTS_OF_PACKETS)) {
		log(LOG_ERR, "ar%d: No response from remote end\n", sc->unit);
		s = splimp();
		ar_down(sc);
		ar_up(sc);
		sc->inlast = sc->out_deficit = 0;
		splx(s);
	} else if ( sc->xmit_busy ) { /* no TX -> no TX timeouts */
		if (sc->out_dog == 0) { 
			log(LOG_ERR, "ar%d: Transmit failure.. no clock?\n",
					sc->unit);
			s = splimp();
			arwatchdog(sc);
#if 0
			ar_down(sc);
			ar_up(sc);
#endif
			splx(s);
			sc->inlast = sc->out_deficit = 0;
		} else {
			sc->out_dog--;
		}
	}
	sc->handle = timeout(ngar_watchdog_frame, sc, hz);
}

/***********************************************************************
 * This section contains the methods for the Netgraph interface
 ***********************************************************************/
/*
 * It is not possible or allowable to create a node of this type.
 * If the hardware exists, it will already have created it.
 */
static	int
ngar_constructor(node_p node)
{
	return (EINVAL);
}

/*
 * give our ok for a hook to be added...
 * If we are not running this should kick the device into life.
 * The hook's private info points to our stash of info about that
 * channel.
 */
static int
ngar_newhook(node_p node, hook_p hook, const char *name)
{
	struct ar_softc *	sc = NG_NODE_PRIVATE(node);

	/*
	 * check if it's our friend the debug hook
	 */
	if (strcmp(name, NG_AR_HOOK_DEBUG) == 0) {
		NG_HOOK_SET_PRIVATE(hook, NULL); /* paranoid */
		sc->debug_hook = hook;
		return (0);
	}

	/*
	 * Check for raw mode hook.
	 */
	if (strcmp(name, NG_AR_HOOK_RAW) != 0) {
		return (EINVAL);
	}
	NG_HOOK_SET_PRIVATE(hook, sc);
	sc->hook = hook;
	sc->datahooks++;
	ar_up(sc);
	return (0);
}

/*
 * incoming messages.
 * Just respond to the generic TEXT_STATUS message
 */
static	int
ngar_rcvmsg(node_p node, item_p item, hook_p lasthook)
{
	struct ar_softc *	sc;
	struct ng_mesg *resp = NULL;
	int error = 0;
	struct ng_mesg *msg;

	NGI_GET_MSG(item, msg);
	sc = NG_NODE_PRIVATE(node);
	switch (msg->header.typecookie) {
	case	NG_AR_COOKIE: 
		error = EINVAL;
		break;
	case	NGM_GENERIC_COOKIE: 
		switch(msg->header.cmd) {
		case NGM_TEXT_STATUS: {
			char        *arg;
			int pos = 0;

			int resplen = sizeof(struct ng_mesg) + 512;
			NG_MKRESPONSE(resp, msg, resplen, M_NOWAIT);
			if (resp == NULL) {
				error = ENOMEM;
				break;
			}
			arg = (resp)->data;
			pos = sprintf(arg, "%ld bytes in, %ld bytes out\n"
			    "highest rate seen: %ld B/S in, %ld B/S out\n",
			sc->inbytes, sc->outbytes,
			sc->inrate, sc->outrate);
			pos += sprintf(arg + pos,
				"%ld output errors\n",
			    	sc->oerrors);
			pos += sprintf(arg + pos,
				"ierrors = %ld, %ld, %ld, %ld\n",
			    	sc->ierrors[0],
			    	sc->ierrors[1],
			    	sc->ierrors[2],
			    	sc->ierrors[3]);

			resp->header.arglen = pos + 1;
			break;
		      }
		default:
		 	error = EINVAL;
		 	break;
		    }
		break;
	default:
		error = EINVAL;
		break;
	}
	/* Take care of synchronous response, if any */
	NG_RESPOND_MSG(error, node, item, resp);
	NG_FREE_MSG(msg);
	return (error);
}

/*
 * get data from another node and transmit it to the correct channel
 */
static int
ngar_rcvdata(hook_p hook, item_p item)
{
	int s;
	int error = 0;
	struct ar_softc * sc = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));
	struct ifqueue	*xmitq_p;
	struct mbuf *m;
	meta_p meta;
	
	NGI_GET_M(item, m);
	NGI_GET_META(item, meta);
	NG_FREE_ITEM(item);
	/*
	 * data doesn't come in from just anywhere (e.g control hook)
	 */
	if ( NG_HOOK_PRIVATE(hook) == NULL) {
		error = ENETDOWN;
		goto bad;
	}

	/* 
	 * Now queue the data for when it can be sent
	 */
	if (meta && meta->priority > 0) {
		xmitq_p = (&sc->xmitq_hipri);
	} else {
		xmitq_p = (&sc->xmitq);
	}
	s = splimp();
	IF_LOCK(xmitq_p);
	if (_IF_QFULL(xmitq_p)) {
		_IF_DROP(xmitq_p);
		IF_UNLOCK(xmitq_p);
		splx(s);
		error = ENOBUFS;
		goto bad;
	}
	_IF_ENQUEUE(xmitq_p, m);
	IF_UNLOCK(xmitq_p);
	arstart(sc);
	splx(s);
	return (0);

bad:
	/* 
	 * It was an error case.
	 * check if we need to free the mbuf, and then return the error
	 */
	NG_FREE_M(m);
	NG_FREE_META(meta);
	return (error);
}

/*
 * do local shutdown processing..
 * this node will refuse to go away, unless the hardware says to..
 * don't unref the node, or remove our name. just clear our links up.
 */
static	int
ngar_shutdown(node_p node)
{
	struct ar_softc * sc = NG_NODE_PRIVATE(node);

	ar_down(sc);
	NG_NODE_UNREF(node);
	/* XXX need to drain the output queues! */

	/* The node is dead, long live the node! */
	/* stolen from the attach routine */
	if (ng_make_node_common(&typestruct, &sc->node) != 0)
		return (0);
	sprintf(sc->nodename, "%s%d", NG_AR_NODE_TYPE, sc->unit);
	if (ng_name_node(sc->node, sc->nodename)) {
		sc->node = NULL;
		printf("node naming failed\n");
		NG_NODE_UNREF(sc->node); /* node dissappears */
		return (0);
	}
	NG_NODE_SET_PRIVATE(sc->node, sc);
	sc->running = 0;
	return (0);
}

/* already linked */
static	int
ngar_connect(hook_p hook)
{
	/* probably not at splnet, force outward queueing */
	NG_HOOK_FORCE_QUEUE(NG_HOOK_PEER(hook));
	/* be really amiable and just say "YUP that's OK by me! " */
	return (0);
}

/*
 * notify on hook disconnection (destruction)
 *
 * Invalidate the private data associated with this dlci.
 * For this type, removal of the last link resets tries to destroy the node.
 * As the device still exists, the shutdown method will not actually
 * destroy the node, but reset the device and leave it 'fresh' :)
 *
 * The node removal code will remove all references except that owned by the
 * driver. 
 */
static	int
ngar_disconnect(hook_p hook)
{
	struct ar_softc * sc = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));
	int	s;
	/*
	 * If it's the data hook, then free resources etc.
	 */
	if (NG_HOOK_PRIVATE(hook)) {
		s = splimp();
		sc->datahooks--;
		if (sc->datahooks == 0)
			ar_down(sc);
		splx(s);
	} else {
		sc->debug_hook = NULL;
	}
	return (0);
}

/*
 * called during bootup
 * or LKM loading to put this type into the list of known modules
 */
static void
ngar_init(void *ignored)
{
	if (ng_newtype(&typestruct))
		printf("ngar install failed\n");
	ngar_done_init = 1;
}
#endif /* NETGRAPH */

/*
 ********************************* END ************************************
 */
