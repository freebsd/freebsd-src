/*
 * Copyright (c) 1996 - 2001 John Hay.
 * Copyright (c) 1996 SDL Communications, Inc.
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include "opt_netgraph.h"
#ifdef NETGRAPH
#include <dev/sr/if_sr.h>
#endif	/* NETGRAPH */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/sockio.h>
#include <sys/socket.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/bus_pio.h>
#include <machine/bus_memio.h>
#include <sys/rman.h>

#include <net/if.h>
#ifdef NETGRAPH
#include <sys/syslog.h>
#else /* NETGRAPH */
#include <net/if_sppp.h>

#include <net/bpf.h>
#endif	/* NETGRAPH */

#include <machine/md_var.h>

#include <dev/ic/hd64570.h>
#include <dev/sr/if_srregs.h>

#ifdef NETGRAPH
#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#endif /* NETGRAPH */
/* #define USE_MODEMCK */

#ifndef BUGGY
#define BUGGY		0
#endif

#ifndef NETGRAPH
#define PPP_HEADER_LEN	4
#endif /* NETGRAPH */

static int	next_sc_unit = 0;
#ifndef NETGRAPH
#ifdef USE_MODEMCK
static int	sr_watcher = 0;
#endif
#endif /* NETGRAPH */

/*
 * Define the software interface for the card... There is one for
 * every channel (port).
 */
struct sr_softc {
#ifndef NETGRAPH
	struct	sppp ifsppp;	/* PPP service w/in system */
#endif /* NETGRAPH */
	struct	sr_hardc *hc;	/* card-level information */

	int	unit;		/* With regard to all sr devices */
	int	subunit;	/* With regard to this card */

	struct	buf_block {
		u_int	txdesc;	/* DPRAM offset */
		u_int	txstart;/* DPRAM offset */
		u_int	txend;	/* DPRAM offset */
		u_int	txtail;	/* # of 1st free gran */
		u_int	txmax;	/* # of free grans */
		u_int	txeda;	/* err descr addr */
	} block[SR_TX_BLOCKS];

	char	xmit_busy;	/* Transmitter is busy */
	char	txb_inuse;	/* # of tx grans in use */
	u_int	txb_new;	/* ndx to new buffer */
	u_int	txb_next_tx;	/* ndx to next gran rdy tx */

	u_int	rxdesc;		/* DPRAM offset */
	u_int	rxstart;	/* DPRAM offset */
	u_int	rxend;		/* DPRAM offset */
	u_int	rxhind;		/* ndx to the hd of rx bufrs */
	u_int	rxmax;		/* # of avail grans */

	u_int	clk_cfg;	/* Clock configuration */

	int	scachan;	/* channel # on card */
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

#ifdef NETGRAPH
#define	DOG_HOLDOFF	6	/* dog holds off for 6 secs */
#define	QUITE_A_WHILE	300	/* 5 MINUTES */
#define	LOTS_OF_PACKETS	100	
#endif /* NETGRAPH */

/*
 * Baud Rate table for Sync Mode.
 * Each entry consists of 3 elements:
 * Baud Rate (x100) , TMC, BR
 *
 * Baud Rate = FCLK / TMC / 2^BR
 * Baud table for Crystal freq. of 9.8304 Mhz
 */
#ifdef N2_TEST_SPEED
struct rate_line {
	int	target;		/* target rate/100 */
	int	tmc_reg;	/* TMC register value */
	int	br_reg;		/* BR (BaudRateClk) selector */
} n2_rates[] = {
	/* Baudx100	TMC		BR */
	{ 3,		128,		8 },
	{ 6,		128,		7 },
	{ 12,		128,		6 },
	{ 24,		128,		5 },
	{ 48,		128,		4 },
	{ 96,		128,		3 },
	{ 192,		128,		2 },
	{ 384,		128,		1 },
	{ 560,		88,		1 },
	{ 640,		77,		1 },
	{ 1280,		38,		1 },
	{ 2560,		19,		1 },
	{ 5120,		10,		1 },
	{ 10000,	5,		1 },
	{ 15000,	3,		1 },
	{ 25000,	2,		1 },
	{ 50000,	1,		1 },
	{ 0,		0,		0 }
};

int	sr_test_speed[] = {
	N2_TEST_SPEED,
	N2_TEST_SPEED
};

int	etc0vals[] = {
	SR_MCR_ETC0,		/* ISA channel 0 */
	SR_MCR_ETC1,		/* ISA channel 1 */
	SR_FECR_ETC0,		/* PCI channel 0 */
	SR_FECR_ETC1		/* PCI channel 1 */
};
#endif

devclass_t sr_devclass;
#ifndef NETGRAPH
MODULE_DEPEND(if_sr, sppp, 1, 1, 1);
#else
MODULE_DEPEND(ng_sync_sr, netgraph, 1, 1, 1);
#endif

static void	srintr(void *arg);
static void	sr_xmit(struct sr_softc *sc);
#ifndef NETGRAPH
static void	srstart(struct ifnet *ifp);
static int	srioctl(struct ifnet *ifp, u_long cmd, caddr_t data);
static void	srwatchdog(struct ifnet *ifp);
#else
static void	srstart(struct sr_softc *sc);
static void	srwatchdog(struct sr_softc *sc);
#endif /* NETGRAPH */
static int	sr_packet_avail(struct sr_softc *sc, int *len, u_char *rxstat);
static void	sr_copy_rxbuf(struct mbuf *m, struct sr_softc *sc, int len);
static void	sr_eat_packet(struct sr_softc *sc, int single);
static void	sr_get_packets(struct sr_softc *sc);

static void	sr_up(struct sr_softc *sc);
static void	sr_down(struct sr_softc *sc);
static void	src_init(struct sr_hardc *hc);
static void	sr_init_sca(struct sr_hardc *hc);
static void	sr_init_msci(struct sr_softc *sc);
static void	sr_init_rx_dmac(struct sr_softc *sc);
static void	sr_init_tx_dmac(struct sr_softc *sc);
static void	sr_dmac_intr(struct sr_hardc *hc, u_char isr);
static void	sr_msci_intr(struct sr_hardc *hc, u_char isr);
static void	sr_timer_intr(struct sr_hardc *hc, u_char isr);
#ifndef NETGRAPH
#ifdef USE_MODEMCK
static void	sr_modemck(void *x);
#endif
#else
static void	sr_modemck(struct sr_softc *x);
#endif /* NETGRAPH */

#ifdef NETGRAPH
static	void	ngsr_watchdog_frame(void * arg);
static	void	ngsr_init(void* ignored);

static ng_constructor_t	ngsr_constructor;
static ng_rcvmsg_t	ngsr_rcvmsg;
static ng_shutdown_t	ngsr_shutdown;
static ng_newhook_t	ngsr_newhook;
/*static ng_findhook_t	ngsr_findhook; */
static ng_connect_t	ngsr_connect;
static ng_rcvdata_t	ngsr_rcvdata;
static ng_disconnect_t	ngsr_disconnect;

static struct ng_type typestruct = {
	NG_ABI_VERSION,
	NG_SR_NODE_TYPE,
	NULL,
	ngsr_constructor,
	ngsr_rcvmsg,
	ngsr_shutdown,
	ngsr_newhook,
	NULL,
	ngsr_connect,
	ngsr_rcvdata,
	ngsr_disconnect,
	NULL
};

static int	ngsr_done_init = 0;
#endif /* NETGRAPH */

/*
 * Register the ports on the adapter.
 * Fill in the info for each port.
#ifndef NETGRAPH
 * Attach each port to sppp and bpf.
#endif
 */
int
sr_attach(device_t device)
{
	int intf_sw, pndx;
	u_int32_t flags;
	u_int fecr, *fecrp;
	struct sr_hardc *hc;
	struct sr_softc *sc;
#ifndef NETGRAPH
	struct ifnet *ifp;
#endif /* NETGRAPH */
	int unit;		/* index: channel w/in card */

	hc = (struct sr_hardc *)device_get_softc(device);
	MALLOC(sc, struct sr_softc *,
		hc->numports * sizeof(struct sr_softc),
		M_DEVBUF, M_WAITOK | M_ZERO);
	if (sc == NULL)
		goto errexit;
	hc->sc = sc;

	/*
	 * Get the TX clock direction and configuration. The default is a
	 * single external clock which is used by RX and TX.
	 */
	switch(hc->cardtype) {
	case SR_CRD_N2:
		flags = device_get_flags(device);
#ifdef N2_TEST_SPEED
		if (sr_test_speed[0] > 0)
			hc->sc[0].clk_cfg = SR_FLAGS_INT_CLK;
		else
#endif
		if (flags & SR_FLAGS_0_CLK_MSK)
			hc->sc[0].clk_cfg =
			    (flags & SR_FLAGS_0_CLK_MSK)
			    >> SR_FLAGS_CLK_SHFT;

		if (hc->numports == 2)
#ifdef N2_TEST_SPEED
			if (sr_test_speed[1] > 0)
				hc->sc[0].clk_cfg = SR_FLAGS_INT_CLK;
			else
#endif
			if (flags & SR_FLAGS_1_CLK_MSK)
				hc->sc[1].clk_cfg = (flags & SR_FLAGS_1_CLK_MSK)
				    >> (SR_FLAGS_CLK_SHFT +
				    SR_FLAGS_CLK_CHAN_SHFT);
		break;
	case SR_CRD_N2PCI:
		fecrp = (u_int *)(hc->sca_base + SR_FECR);
		fecr = *fecrp;
		for (pndx = 0; pndx < hc->numports; pndx++, sc++) {
			switch (pndx) {
			case 1:
				intf_sw = fecr & SR_FECR_ID1 >> SR_FE_ID1_SHFT;
				break;
			case 0:
			default:
				intf_sw = fecr & SR_FECR_ID0 >> SR_FE_ID0_SHFT;
			}

#ifdef N2_TEST_SPEED
			if (sr_test_speed[pndx] > 0)
				sc->clk_cfg = SR_FLAGS_INT_CLK;
			else
#endif
				switch (intf_sw) {
				default:
				case SR_FE_ID_RS232:
				case SR_FE_ID_HSSI:
				case SR_FE_ID_RS422:
				case SR_FE_ID_TEST:
					break;

				case SR_FE_ID_V35:
					sc->clk_cfg = SR_FLAGS_EXT_SEP_CLK;
					break;

				case SR_FE_ID_X21:
					sc->clk_cfg = SR_FLAGS_EXT_CLK;
					break;
				}
		}
		sc = hc->sc;
		break;
	}

	/*
	 * Report Card configuration information before we start configuring
	 * each channel on the card...
	 */
	printf("src%d: %uK RAM (%d mempages) @ %p-%p, %u ports.\n",
	       hc->cunit, hc->memsize / 1024, hc->mempages,
	       hc->mem_start, hc->mem_end, hc->numports);

	src_init(hc);
	sr_init_sca(hc);

	if (BUS_SETUP_INTR(device_get_parent(device), device, hc->res_irq,
	    INTR_TYPE_NET, srintr, hc, &hc->intr_cookie) != 0)
		goto errexit;

	/*
	 * Now configure each port on the card.
	 */
	for (unit = 0; unit < hc->numports; sc++, unit++) {
		sc->hc = hc;
		sc->subunit = unit;
		sc->unit = next_sc_unit;
		next_sc_unit++;
		sc->scachan = unit % NCHAN;

		sr_init_rx_dmac(sc);
		sr_init_tx_dmac(sc);
		sr_init_msci(sc);

		printf("sr%d: Adapter %d, port %d.\n",
		       sc->unit, hc->cunit, sc->subunit);

#ifndef NETGRAPH
		ifp = &sc->ifsppp.pp_if;
		ifp->if_softc = sc;
		ifp->if_unit = sc->unit;
		ifp->if_name = "sr";
		ifp->if_mtu = PP_MTU;
		ifp->if_flags = IFF_POINTOPOINT | IFF_MULTICAST;
		ifp->if_ioctl = srioctl;
		ifp->if_start = srstart;
		ifp->if_watchdog = srwatchdog;

		sc->ifsppp.pp_flags = PP_KEEPALIVE;
		sppp_attach((struct ifnet *)&sc->ifsppp);
		if_attach(ifp);

		bpfattach(ifp, DLT_PPP, PPP_HEADER_LEN);
#else	/* NETGRAPH */
		/*
		 * we have found a node, make sure our 'type' is availabe.
		 */
		if (ngsr_done_init == 0) ngsr_init(NULL);
		if (ng_make_node_common(&typestruct, &sc->node) != 0)
			goto errexit;
		sprintf(sc->nodename, "%s%d", NG_SR_NODE_TYPE, sc->unit);
		if (ng_name_node(sc->node, sc->nodename)) {
			NG_NODE_UNREF(sc->node); /* make it go away again */
			goto errexit;
		}
		NG_NODE_SET_PRIVATE(sc->node, sc);
		callout_handle_init(&sc->handle);
		sc->xmitq.ifq_maxlen = IFQ_MAXLEN;
		sc->xmitq_hipri.ifq_maxlen = IFQ_MAXLEN;
		mtx_init(&sc->xmitq.ifq_mtx, "sr_xmitq", NULL, MTX_DEF);
		mtx_init(&sc->xmitq_hipri.ifq_mtx, "sr_xmitq_hipri", NULL,
		    MTX_DEF);
		sc->running = 0;
#endif	/* NETGRAPH */
	}

	if (hc->mempages)
		SRC_SET_OFF(hc->iobase);

	return (0);

errexit:
	sr_deallocate_resources(device);
	return (ENXIO);
}

int
sr_detach(device_t device)
{
	device_t parent = device_get_parent(device);
	struct sr_hardc *hc = device_get_softc(device);

	if (hc->intr_cookie != NULL) {
		if (BUS_TEARDOWN_INTR(parent, device,
			hc->res_irq, hc->intr_cookie) != 0) {
				printf("intr teardown failed.. continuing\n");
		}
		hc->intr_cookie = NULL;
	}

	/* XXX Stop the DMA. */

	/*
	 * deallocate any system resources we may have
	 * allocated on behalf of this driver.
	 */
	FREE(hc->sc, M_DEVBUF);
	hc->sc = NULL;
	hc->mem_start = NULL;
	return (sr_deallocate_resources(device));
}

int
sr_allocate_ioport(device_t device, int rid, u_long size)
{
	struct sr_hardc *hc = device_get_softc(device);

	hc->rid_ioport = rid;
	hc->res_ioport = bus_alloc_resource(device, SYS_RES_IOPORT,
			&hc->rid_ioport, 0ul, ~0ul, size, RF_ACTIVE);
	if (hc->res_ioport == NULL) {
		goto errexit;
	}
	return (0);

errexit:
	sr_deallocate_resources(device);
	return (ENXIO);
}

int
sr_allocate_irq(device_t device, int rid, u_long size)
{
	struct sr_hardc *hc = device_get_softc(device);

	hc->rid_irq = rid;
	hc->res_irq = bus_alloc_resource(device, SYS_RES_IRQ,
			&hc->rid_irq, 0ul, ~0ul, 1, RF_SHAREABLE|RF_ACTIVE);
	if (hc->res_irq == NULL) {
		goto errexit;
	}
	return (0);

errexit:
	sr_deallocate_resources(device);
	return (ENXIO);
}

int
sr_allocate_memory(device_t device, int rid, u_long size)
{
	struct sr_hardc *hc = device_get_softc(device);

	hc->rid_memory = rid;
	hc->res_memory = bus_alloc_resource(device, SYS_RES_MEMORY,
			&hc->rid_memory, 0ul, ~0ul, size, RF_ACTIVE);
	if (hc->res_memory == NULL) {
		goto errexit;
	}
	return (0);

errexit:
	sr_deallocate_resources(device);
	return (ENXIO);
}

int
sr_allocate_plx_memory(device_t device, int rid, u_long size)
{
	struct sr_hardc *hc = device_get_softc(device);

	hc->rid_plx_memory = rid;
	hc->res_plx_memory = bus_alloc_resource(device, SYS_RES_MEMORY,
			&hc->rid_plx_memory, 0ul, ~0ul, size, RF_ACTIVE);
	if (hc->res_plx_memory == NULL) {
		goto errexit;
	}
	return (0);

errexit:
	sr_deallocate_resources(device);
	return (ENXIO);
}

int
sr_deallocate_resources(device_t device)
{
	struct sr_hardc *hc = device_get_softc(device);

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
 * N2 Interrupt Service Routine
 *
 * First figure out which SCA gave the interrupt.
 * Process it.
 * See if there is other interrupts pending.
 * Repeat until there no interrupts remain.
 */
static void
srintr(void *arg)
{
	struct sr_hardc *hc = (struct sr_hardc *)arg;
	sca_regs *sca = hc->sca;	/* MSCI register tree */
	u_char  isr0, isr1, isr2;	/* interrupt statii captured */

#if BUGGY > 1
	printf("sr: srintr_hc(hc=%08x)\n", hc);
#endif

	/*
	 * Since multiple interfaces may share this interrupt, we must loop
	 * until no interrupts are still pending service.
	 */
	while (1) {
		/*
		 * Read all three interrupt status registers from the N2
		 * card...
		 */
		isr0 = SRC_GET8(hc->sca_base, sca->isr0);
		isr1 = SRC_GET8(hc->sca_base, sca->isr1);
		isr2 = SRC_GET8(hc->sca_base, sca->isr2);

		/*
		 * If all three registers returned 0, we've finished
		 * processing interrupts from this device, so we can quit
		 * this loop...
		 */
		if ((isr0 | isr1 | isr2) == 0)
			break;

#if BUGGY > 2
		printf("src%d: srintr_hc isr0 %x, isr1 %x, isr2 %x\n",
#ifndef NETGRAPH
			unit, isr0, isr1, isr2);
#else
			hc->cunit, isr0, isr1, isr2);
#endif /* NETGRAPH */
#endif

		/*
		 * Now we can dispatch the interrupts. Since we don't expect
		 * either MSCI or timer interrupts, we'll test for DMA
		 * interrupts first...
		 */
		if (isr1)	/* DMA-initiated interrupt */
			sr_dmac_intr(hc, isr1);

		if (isr0)	/* serial part IRQ? */
			sr_msci_intr(hc, isr0);

		if (isr2)	/* timer-initiated interrupt */
			sr_timer_intr(hc, isr2);
	}
}

/*
 * This will only start the transmitter. It is assumed that the data
 * is already there.
 * It is normally called from srstart() or sr_dmac_intr().
 */
static void
sr_xmit(struct sr_softc *sc)
{
	u_short cda_value;	/* starting descriptor */
	u_short eda_value;	/* ending descriptor */
	struct sr_hardc *hc;
#ifndef NETGRAPH
	struct ifnet *ifp;	/* O/S Network Services */
#endif /* NETGRAPH */
	dmac_channel *dmac;	/* DMA channel registers */

#if BUGGY > 0
	printf("sr: sr_xmit( sc=%08x)\n", sc);
#endif

	hc = sc->hc;
#ifndef NETGRAPH
	ifp = &sc->ifsppp.pp_if;
#endif /* NETGRAPH */
	dmac = &hc->sca->dmac[DMAC_TXCH(sc->scachan)];

	/*
	 * Get the starting and ending addresses of the chain to be
	 * transmitted and pass these on to the DMA engine on-chip.
	 */
	cda_value = sc->block[sc->txb_next_tx].txdesc + hc->mem_pstart;
	cda_value &= 0x00ffff;
	eda_value = sc->block[sc->txb_next_tx].txeda + hc->mem_pstart;
	eda_value &= 0x00ffff;

	SRC_PUT16(hc->sca_base, dmac->cda, cda_value);
	SRC_PUT16(hc->sca_base, dmac->eda, eda_value);

	/*
	 * Now we'll let the DMA status register know about this change
	 */
	SRC_PUT8(hc->sca_base, dmac->dsr, SCA_DSR_DE);

	sc->xmit_busy = 1;	/* mark transmitter busy */

#if BUGGY > 2
	printf("sr%d: XMIT  cda=%04x, eda=%4x, rcda=%08lx\n",
	       sc->unit, cda_value, eda_value,
	       sc->block[sc->txb_next_tx].txdesc + hc->mem_pstart);
#endif

	sc->txb_next_tx++;	/* update next transmit seq# */

	if (sc->txb_next_tx == SR_TX_BLOCKS)	/* handle wrap... */
		sc->txb_next_tx = 0;

#ifndef NETGRAPH
	/*
	 * Finally, we'll set a timout (which will start srwatchdog())
	 * within the O/S network services layer...
	 */
	ifp->if_timer = 2;	/* Value in seconds. */
#else
	/*
	 * Don't time out for a while.
	 */
	sc->out_dog = DOG_HOLDOFF;	/* give ourself some breathing space*/
#endif /* NETGRAPH */
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
 * Transmitter idle state is indicated by the IFF_OACTIVE flag.
 * The function that clears that should ensure that the transmitter
 * and its DMA is in a "good" idle state.
 */
#ifndef NETGRAPH
static void
srstart(struct ifnet *ifp)
{
	struct sr_softc *sc;	/* channel control structure */
#else
static void
srstart(struct sr_softc *sc)
{
#endif /* NETGRAPH */
	struct sr_hardc *hc;	/* card control/config block */
	int len;		/* total length of a packet */
	int pkts;		/* packets placed in DPRAM */
	int tlen;		/* working length of pkt */
	u_int i;
	struct mbuf *mtx;	/* message buffer from O/S */
	u_char *txdata;		/* buffer address in DPRAM */
	sca_descriptor *txdesc;	/* working descriptor pointr */
	struct buf_block *blkp;

#ifndef NETGRAPH
#if BUGGY > 0
	printf("sr: srstart( ifp=%08x)\n", ifp);
#endif
	sc = ifp->if_softc;
	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return;
#endif /* NETGRAPH */
	hc = sc->hc;
	/*
	 * It is OK to set the memory window outside the loop because all tx
	 * buffers and descriptors are assumed to be in the same 16K window.
	 */
	if (hc->mempages) {
		SRC_SET_ON(hc->iobase);
		SRC_SET_MEM(hc->iobase, sc->block[0].txdesc);
	}

	/*
	 * Loop to place packets into DPRAM.
	 *
	 * We stay in this loop until there is nothing in
	 * the TX queue left or the tx buffers are full.
	 */
top_srstart:

	/*
	 * See if we have space for more packets.
	 */
	if (sc->txb_inuse == SR_TX_BLOCKS) {	/* out of space? */
#ifndef NETGRAPH
		ifp->if_flags |= IFF_OACTIVE;	/* yes, mark active */
#else
		/*ifp->if_flags |= IFF_OACTIVE;*/	/* yes, mark active */
#endif /* NETGRAPH */

		if (hc->mempages)
			SRC_SET_OFF(hc->iobase);

#if BUGGY > 9
		printf("sr%d.srstart: sc->txb_inuse=%d; DPRAM full...\n",
		       sc->unit, sc->txb_inuse);
#endif
		return;
	}
	/*
	 * OK, the card can take more traffic.  Let's see if there's any
	 * pending from the system...
	 *
	 * NOTE:
	 * The architecture of the networking interface doesn't
	 * actually call us like 'write()', providing an address.  We get
	 * started, a lot like a disk strategy routine, and we actually call
	 * back out to the system to get traffic to send...
	 *
	 * NOTE:
	 * If we were gonna run through another layer, we would use a
	 * dispatch table to select the service we're getting a packet
	 * from...
	 */
#ifndef NETGRAPH
	mtx = sppp_dequeue(ifp);
#else /* NETGRAPH */
	IF_DEQUEUE(&sc->xmitq_hipri, mtx);
	if (mtx == NULL) {
		IF_DEQUEUE(&sc->xmitq, mtx);
	}
#endif /* NETGRAPH */
	if (!mtx) {
		if (hc->mempages)
			SRC_SET_OFF(hc->iobase);
		return;
	}
	/*
	 * OK, we got a packet from the network services of the O/S. Now we
	 * can move it into the DPRAM (under control of the descriptors) and
	 * fire it off...
	 */
	pkts = 0;
	i = 0;			/* counts # of granules used */

	blkp = &sc->block[sc->txb_new];	/* address of free granule */
	txdesc = (sca_descriptor *)
	    (hc->mem_start + (blkp->txdesc & hc->winmsk));

	txdata = (u_char *)(hc->mem_start
			    + (blkp->txstart & hc->winmsk));

	/*
	 * Now we'll try to install as many packets as possible into the
	 * card's DP RAM buffers.
	 */
	for (;;) {		/* perform actual copy of packet */
		len = mtx->m_pkthdr.len;	/* length of message */

#if BUGGY > 1
		printf("sr%d.srstart: mbuf @ %08lx, %d bytes\n",
			   sc->unit, mtx, len);
#endif

#ifndef NETGRAPH
		BPF_MTAP(ifp, mtx);
#else	/* NETGRAPH */
		sc->outbytes += len;
#endif	/* NETGRAPH */

		/*
		 * We can perform a straight copy because the tranmit
		 * buffers won't wrap.
		 */
		m_copydata(mtx, 0, len, txdata);

		/*
		 * Now we know how big the message is gonna be.  We must now
		 * construct the descriptors to drive this message out...
		 */
		tlen = len;
		while (tlen > SR_BUF_SIZ) {	/* loop for full granules */
			txdesc->stat = 0;	/* reset bits */
			txdesc->len = SR_BUF_SIZ;	/* size of granule */
			tlen -= SR_BUF_SIZ;

			txdesc++;	/* move to next dscr */
			txdata += SR_BUF_SIZ;	/* adjust data addr */
			i++;
		}

		/*
		 * This section handles the setting of the final piece of a
		 * message.
		 */
		txdesc->stat = SCA_DESC_EOM;
		txdesc->len = tlen;
		pkts++;

		/*
		 * prepare for subsequent packets (if any)
		 */
		txdesc++;
		txdata += SR_BUF_SIZ;	/* next mem granule */
		i++;		/* count of granules */

		/*
		 * OK, we've now placed the message into the DPRAM where it
		 * can be transmitted.  We'll now release the message memory
		 * and update the statistics...
		 */
		m_freem(mtx);
#ifndef NETGRAPH
		++sc->ifsppp.pp_if.if_opackets;
#else	/* NETGRAPH */
		sc->opackets++;
#endif /* NETGRAPH */

		/*
		 * Check if we have space for another packet. XXX This is
		 * hardcoded.  A packet can't be larger than 3 buffers (3 x
		 * 512).
		 */
		if ((i + 3) >= blkp->txmax) {	/* enough remains? */
#if BUGGY > 9
			printf("sr%d.srstart: i=%d (%d pkts); card full.\n",
			       sc->unit, i, pkts);
#endif
			break;
		}
		/*
		 * We'll pull the next message to be sent (if any)
		 */
#ifndef NETGRAPH
		mtx = sppp_dequeue(ifp);
#else /* NETGRAPH */
		IF_DEQUEUE(&sc->xmitq_hipri, mtx);
		if (mtx == NULL) {
			IF_DEQUEUE(&sc->xmitq, mtx);
		}
#endif /* NETGRAPH */
		if (!mtx) {	/* no message?  We're done! */
#if BUGGY > 9
			printf("sr%d.srstart: pending=0, pkts=%d\n",
			       sc->unit, pkts);
#endif
			break;
		}
	}

	blkp->txtail = i;	/* record next free granule */

	/*
	 * Mark the last descriptor, so that the SCA know where to stop.
	 */
	txdesc--;		/* back up to last descriptor in list */
	txdesc->stat |= SCA_DESC_EOT;	/* mark as end of list */

	/*
	 * Now we'll reset the transmit granule's descriptor address so we
	 * can record this in the structure and fire it off w/ the DMA
	 * processor of the serial chip...
	 */
	txdesc = (sca_descriptor *)(uintptr_t)blkp->txdesc;
	blkp->txeda = (u_short)((uintptr_t)&txdesc[i]);

	sc->txb_inuse++;	/* update inuse status */
	sc->txb_new++;		/* new traffic wuz added */

	if (sc->txb_new == SR_TX_BLOCKS)
		sc->txb_new = 0;

	/*
	 * If the tranmitter wasn't marked as "busy" we will force it to be
	 * started...
	 */
	if (sc->xmit_busy == 0) {
		sr_xmit(sc);
#if BUGGY > 9
		printf("sr%d.srstart: called sr_xmit()\n", sc->unit);
#endif
	}
	goto top_srstart;
}

#ifndef NETGRAPH
/*
 * Handle ioctl's at the device level, though we *will* call up
 * a layer...
 */
#if BUGGY > 2
static int bug_splats[] = {0, 0, 0, 0, 0, 0, 0, 0};
#endif

static int
srioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	int s, error, was_up, should_be_up;
	struct sr_softc *sc = ifp->if_softc;

#if BUGGY > 0
	if_printf(ifp, "srioctl(ifp=%08x, cmd=%08x, data=%08x)\n",
	       ifp, cmd, data);
#endif

	was_up = ifp->if_flags & IFF_RUNNING;

	error = sppp_ioctl(ifp, cmd, data);

#if BUGGY > 1
	if_printf(ifp, "ioctl: ifsppp.pp_flags = %08x, if_flags %08x.\n",
	      ((struct sppp *)ifp)->pp_flags, ifp->if_flags);
#endif

	if (error)
		return error;

	if ((cmd != SIOCSIFFLAGS) && (cmd != SIOCSIFADDR)) {
#if BUGGY > 2
		if (bug_splats[sc->unit]++ < 2) {
			printf("sr(%d).if_addrlist = %08x\n",
			       sc->unit, ifp->if_addrlist);
			printf("sr(%d).if_bpf = %08x\n",
			       sc->unit, ifp->if_bpf);
			printf("sr(%d).if_init = %08x\n",
			       sc->unit, ifp->if_init);
			printf("sr(%d).if_output = %08x\n",
			       sc->unit, ifp->if_output);
			printf("sr(%d).if_start = %08x\n",
			       sc->unit, ifp->if_start);
			printf("sr(%d).if_done = %08x\n",
			       sc->unit, ifp->if_done);
			printf("sr(%d).if_ioctl = %08x\n",
			       sc->unit, ifp->if_ioctl);
			printf("sr(%d).if_reset = %08x\n",
			       sc->unit, ifp->if_reset);
			printf("sr(%d).if_watchdog = %08x\n",
			       sc->unit, ifp->if_watchdog);
		}
#endif
		return 0;
	}

	s = splimp();
	should_be_up = ifp->if_flags & IFF_RUNNING;

	if (!was_up && should_be_up) {
		/*
		 * Interface should be up -- start it.
		 */
		sr_up(sc);
		srstart(ifp);

		/*
		 * XXX Clear the IFF_UP flag so that the link will only go
		 * up after sppp lcp and ipcp negotiation.
		 */
		/* ifp->if_flags &= ~IFF_UP; */
	} else if (was_up && !should_be_up) {
		/*
		 * Interface should be down -- stop it.
		 */
		sr_down(sc);
		sppp_flush(ifp);
	}
	splx(s);
	return 0;
}
#endif /* NETGRAPH */

/*
 * This is to catch lost tx interrupts.
 */
static void
#ifndef NETGRAPH
srwatchdog(struct ifnet *ifp)
#else
srwatchdog(struct sr_softc *sc)
#endif /* NETGRAPH */
{
	int     got_st0, got_st1, got_st3, got_dsr;
#ifndef NETGRAPH
	struct sr_softc *sc = ifp->if_softc;
#endif /* NETGRAPH */
	struct sr_hardc *hc = sc->hc;
	msci_channel *msci = &hc->sca->msci[sc->scachan];
	dmac_channel *dmac = &sc->hc->sca->dmac[sc->scachan];

#if BUGGY > 0
#ifndef NETGRAPH
	printf("srwatchdog(unit=%d)\n", unit);
#else
	printf("srwatchdog(unit=%d)\n", sc->unit);
#endif /* NETGRAPH */
#endif

#ifndef NETGRAPH
	if (!(ifp->if_flags & IFF_RUNNING))
		return;

	ifp->if_oerrors++;	/* update output error count */
#else	/* NETGRAPH */
	sc->oerrors++;	/* update output error count */
#endif /* NETGRAPH */

	got_st0 = SRC_GET8(hc->sca_base, msci->st0);
	got_st1 = SRC_GET8(hc->sca_base, msci->st1);
	got_st3 = SRC_GET8(hc->sca_base, msci->st3);
	got_dsr = SRC_GET8(hc->sca_base, dmac->dsr);

#ifndef NETGRAPH
#if	0
	if (ifp->if_flags & IFF_DEBUG)
#endif
		printf("sr%d: transmit failed, "
#else	/* NETGRAPH */
	printf("sr%d: transmit failed, "
#endif /* NETGRAPH */
		       "ST0 %02x, ST1 %02x, ST3 %02x, DSR %02x.\n",
		       sc->unit,
		       got_st0, got_st1, got_st3, got_dsr);

	if (SRC_GET8(hc->sca_base, msci->st1) & SCA_ST1_UDRN) {
		SRC_PUT8(hc->sca_base, msci->cmd, SCA_CMD_TXABORT);
		SRC_PUT8(hc->sca_base, msci->cmd, SCA_CMD_TXENABLE);
		SRC_PUT8(hc->sca_base, msci->st1, SCA_ST1_UDRN);
	}
	sc->xmit_busy = 0;
#ifndef NETGRAPH
	ifp->if_flags &= ~IFF_OACTIVE;
#else
	/*ifp->if_flags &= ~IFF_OACTIVE; */
#endif /* NETGRAPH */

	if (sc->txb_inuse && --sc->txb_inuse)
		sr_xmit(sc);

#ifndef NETGRAPH
	srstart(ifp);	/* restart transmitter */
#else
	srstart(sc);	/* restart transmitter */
#endif /* NETGRAPH */
}

static void
sr_up(struct sr_softc *sc)
{
	u_int *fecrp;
	struct sr_hardc *hc = sc->hc;
	sca_regs *sca = hc->sca;
	msci_channel *msci = &sca->msci[sc->scachan];

#if BUGGY > 0
	printf("sr_up(sc=%08x)\n", sc);
#endif

	/*
	 * Enable transmitter and receiver. Raise DTR and RTS. Enable
	 * interrupts.
	 *
	 * XXX What about using AUTO mode in msci->md0 ???
	 */
	SRC_PUT8(hc->sca_base, msci->ctl,
		 SRC_GET8(hc->sca_base, msci->ctl) & ~SCA_CTL_RTS);

	if (sc->scachan == 0)
		switch (hc->cardtype) {
		case SR_CRD_N2:
			outb(hc->iobase + SR_MCR,
			     (inb(hc->iobase + SR_MCR) & ~SR_MCR_DTR0));
			break;
		case SR_CRD_N2PCI:
			fecrp = (u_int *)(hc->sca_base + SR_FECR);
			*fecrp &= ~SR_FECR_DTR0;
			break;
		}
	else
		switch (hc->cardtype) {
		case SR_CRD_N2:
			outb(hc->iobase + SR_MCR,
			     (inb(hc->iobase + SR_MCR) & ~SR_MCR_DTR1));
			break;
		case SR_CRD_N2PCI:
			fecrp = (u_int *)(hc->sca_base + SR_FECR);
			*fecrp &= ~SR_FECR_DTR1;
			break;
		}

	if (sc->scachan == 0) {
		SRC_PUT8(hc->sca_base, sca->ier0,
			 SRC_GET8(hc->sca_base, sca->ier0) | 0x000F);
		SRC_PUT8(hc->sca_base, sca->ier1,
			 SRC_GET8(hc->sca_base, sca->ier1) | 0x000F);
	} else {
		SRC_PUT8(hc->sca_base, sca->ier0,
			 SRC_GET8(hc->sca_base, sca->ier0) | 0x00F0);
		SRC_PUT8(hc->sca_base, sca->ier1,
			 SRC_GET8(hc->sca_base, sca->ier1) | 0x00F0);
	}

	SRC_PUT8(hc->sca_base, msci->cmd, SCA_CMD_RXENABLE);
	inb(hc->iobase);	/* XXX slow it down a bit. */
	SRC_PUT8(hc->sca_base, msci->cmd, SCA_CMD_TXENABLE);

#ifndef NETGRAPH
#ifdef USE_MODEMCK
	if (sr_watcher == 0)
		sr_modemck(NULL);
#endif
#else	/* NETGRAPH */
	untimeout(ngsr_watchdog_frame, sc, sc->handle);
	sc->handle = timeout(ngsr_watchdog_frame, sc, hz);
	sc->running = 1;
#endif /* NETGRAPH */
}

static void
sr_down(struct sr_softc *sc)
{
	u_int *fecrp;
	struct sr_hardc *hc = sc->hc;
	sca_regs *sca = hc->sca;
	msci_channel *msci = &sca->msci[sc->scachan];

#if BUGGY > 0
	printf("sr_down(sc=%08x)\n", sc);
#endif
#ifdef NETGRAPH
	untimeout(ngsr_watchdog_frame, sc, sc->handle);
	sc->running = 0;
#endif /* NETGRAPH */

	/*
	 * Disable transmitter and receiver. Lower DTR and RTS. Disable
	 * interrupts.
	 */
	SRC_PUT8(hc->sca_base, msci->cmd, SCA_CMD_RXDISABLE);
	inb(hc->iobase);	/* XXX slow it down a bit. */
	SRC_PUT8(hc->sca_base, msci->cmd, SCA_CMD_TXDISABLE);

	SRC_PUT8(hc->sca_base, msci->ctl,
		 SRC_GET8(hc->sca_base, msci->ctl) | SCA_CTL_RTS);

	if (sc->scachan == 0)
		switch (hc->cardtype) {
		case SR_CRD_N2:
			outb(hc->iobase + SR_MCR,
			     (inb(hc->iobase + SR_MCR) | SR_MCR_DTR0));
			break;
		case SR_CRD_N2PCI:
			fecrp = (u_int *)(hc->sca_base + SR_FECR);
			*fecrp |= SR_FECR_DTR0;
			break;
		}
	else
		switch (hc->cardtype) {
		case SR_CRD_N2:
			outb(hc->iobase + SR_MCR,
			     (inb(hc->iobase + SR_MCR) | SR_MCR_DTR1));
			break;
		case SR_CRD_N2PCI:
			fecrp = (u_int *)(hc->sca_base + SR_FECR);
			*fecrp |= SR_FECR_DTR1;
			break;
		}

	if (sc->scachan == 0) {
		SRC_PUT8(hc->sca_base, sca->ier0,
			 SRC_GET8(hc->sca_base, sca->ier0) & ~0x0F);
		SRC_PUT8(hc->sca_base, sca->ier1,
			 SRC_GET8(hc->sca_base, sca->ier1) & ~0x0F);
	} else {
		SRC_PUT8(hc->sca_base, sca->ier0,
			 SRC_GET8(hc->sca_base, sca->ier0) & ~0xF0);
		SRC_PUT8(hc->sca_base, sca->ier1,
			 SRC_GET8(hc->sca_base, sca->ier1) & ~0xF0);
	}
}

/*
 * Initialize the card, allocate memory for the sr_softc structures
 * and fill in the pointers.
 */
static void
src_init(struct sr_hardc *hc)
{
	struct sr_softc *sc = hc->sc;
	int x;
	u_int chanmem;
	u_int bufmem;
	u_int next;
	u_int descneeded;

#if BUGGY > 0
	printf("src_init(hc=%08x)\n", hc);
#endif

	chanmem = hc->memsize / hc->numports;
	next = 0;

	for (x = 0; x < hc->numports; x++, sc++) {
		int blk;

		for (blk = 0; blk < SR_TX_BLOCKS; blk++) {
			sc->block[blk].txdesc = next;
			bufmem = (16 * 1024) / SR_TX_BLOCKS;
			descneeded = bufmem / SR_BUF_SIZ;

			sc->block[blk].txstart = sc->block[blk].txdesc
			    + ((((descneeded * sizeof(sca_descriptor))
				 / SR_BUF_SIZ) + 1)
			       * SR_BUF_SIZ);

			sc->block[blk].txend = next + bufmem;
			sc->block[blk].txmax =
			    (sc->block[blk].txend - sc->block[blk].txstart)
			    / SR_BUF_SIZ;
			next += bufmem;

#if BUGGY > 2
			printf("sr%d: blk %d: txdesc %08x, txstart %08x\n",
			       sc->unit, blk,
			       sc->block[blk].txdesc, sc->block[blk].txstart);
#endif
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
 * Configure the sca waitstates.
 * Configure the global interrupt registers.
 * Enable master dma enable.
 */
static void
sr_init_sca(struct sr_hardc *hc)
{
	sca_regs *sca = hc->sca;

#if BUGGY > 0
	printf("sr_init_sca(hc=%08x)\n", hc);
#endif

	/*
	 * Do the wait registers. Set everything to 0 wait states.
	 */
	SRC_PUT8(hc->sca_base, sca->pabr0, 0);
	SRC_PUT8(hc->sca_base, sca->pabr1, 0);
	SRC_PUT8(hc->sca_base, sca->wcrl, 0);
	SRC_PUT8(hc->sca_base, sca->wcrm, 0);
	SRC_PUT8(hc->sca_base, sca->wcrh, 0);

	/*
	 * Configure the interrupt registers. Most are cleared until the
	 * interface is configured.
	 */
	SRC_PUT8(hc->sca_base, sca->ier0, 0x00);	/* MSCI interrupts. */
	SRC_PUT8(hc->sca_base, sca->ier1, 0x00);	/* DMAC interrupts */
	SRC_PUT8(hc->sca_base, sca->ier2, 0x00);	/* TIMER interrupts. */
	SRC_PUT8(hc->sca_base, sca->itcr, 0x00);	/* Use ivr and no intr
							 * ack */
	SRC_PUT8(hc->sca_base, sca->ivr, 0x40);	/* Interrupt vector. */
	SRC_PUT8(hc->sca_base, sca->imvr, 0x40);

	/*
	 * Configure the timers. XXX Later
	 */

	/*
	 * Set the DMA channel priority to rotate between all four channels.
	 *
	 * Enable all dma channels.
	 */
	SRC_PUT8(hc->sca_base, sca->pcr, SCA_PCR_PR2);
	SRC_PUT8(hc->sca_base, sca->dmer, SCA_DMER_EN);
}

/*
 * Configure the msci
 *
 * NOTE: The serial port configuration is hardcoded at the moment.
 */
static void
sr_init_msci(struct sr_softc *sc)
{
	int portndx;		/* on-board port number */
	u_int mcr_v;		/* contents of modem control */
	u_int *fecrp;		/* pointer for PCI's MCR i/o */
	struct sr_hardc *hc = sc->hc;
	msci_channel *msci = &hc->sca->msci[sc->scachan];
#ifdef N2_TEST_SPEED
	int br_v;		/* contents for BR divisor */
	int etcndx;		/* index into ETC table */
	int fifo_v, gotspeed;	/* final tabled speed found */
	int tmc_v;		/* timer control register */
	int wanted;		/* speed (bitrate) wanted... */
	struct rate_line *rtp;
#endif

	portndx = sc->scachan;

#if BUGGY > 0
	printf("sr: sr_init_msci( sc=%08x)\n", sc);
#endif

	SRC_PUT8(hc->sca_base, msci->cmd, SCA_CMD_RESET);
	SRC_PUT8(hc->sca_base, msci->md0, SCA_MD0_CRC_1 |
		 SCA_MD0_CRC_CCITT |
		 SCA_MD0_CRC_ENABLE |
		 SCA_MD0_MODE_HDLC);
	SRC_PUT8(hc->sca_base, msci->md1, SCA_MD1_NOADDRCHK);
	SRC_PUT8(hc->sca_base, msci->md2, SCA_MD2_DUPLEX | SCA_MD2_NRZ);

	/*
	 * According to the manual I should give a reset after changing the
	 * mode registers.
	 */
	SRC_PUT8(hc->sca_base, msci->cmd, SCA_CMD_RXRESET);
	SRC_PUT8(hc->sca_base, msci->ctl, SCA_CTL_IDLPAT |
		 SCA_CTL_UDRNC |
		 SCA_CTL_RTS);

	/*
	 * XXX Later we will have to support different clock settings.
	 */
	switch (sc->clk_cfg) {
	default:
#if BUGGY > 0
		printf("sr%: clk_cfg=%08x, selected default clock.\n",
		       portndx, sc->clk_cfg);
#endif
		/* FALLTHROUGH */
	case SR_FLAGS_EXT_CLK:
		/*
		 * For now all interfaces are programmed to use the RX clock
		 * for the TX clock.
		 */

#if BUGGY > 0
		printf("sr%d: External Clock Selected.\n", portndx);
#endif

		SRC_PUT8(hc->sca_base, msci->rxs,
			 SCA_RXS_CLK_RXC0 | SCA_RXS_DIV1);
		SRC_PUT8(hc->sca_base, msci->txs,
			 SCA_TXS_CLK_RX | SCA_TXS_DIV1);
		break;

	case SR_FLAGS_EXT_SEP_CLK:
#if BUGGY > 0
		printf("sr%d: Split Clocking Selected.\n", portndx);
#endif

		SRC_PUT8(hc->sca_base, msci->rxs,
			 SCA_RXS_CLK_RXC0 | SCA_RXS_DIV1);
		SRC_PUT8(hc->sca_base, msci->txs,
			 SCA_TXS_CLK_TXC | SCA_TXS_DIV1);
		break;

	case SR_FLAGS_INT_CLK:
#if BUGGY > 0
		printf("sr%d: Internal Clocking selected.\n", portndx);
#endif

		/*
		 * XXX I do need some code to set the baud rate here!
		 */
#ifdef N2_TEST_SPEED
		switch (hc->cardtype) {
		case SR_CRD_N2PCI:
			fecrp = (u_int *)(hc->sca_base + SR_FECR);
			mcr_v = *fecrp;
			etcndx = 2;
			break;
		case SR_CRD_N2:
		default:
			mcr_v = inb(hc->iobase + SR_MCR);
			etcndx = 0;
		}

		fifo_v = 0x10;	/* stolen from Linux version */

		/*
		 * search for appropriate speed in table, don't calc it:
		 */
		wanted = sr_test_speed[portndx];
		rtp = &n2_rates[0];	/* point to first table item */

		while ((rtp->target > 0)	/* search table for speed */
		       &&(rtp->target != wanted))
			rtp++;

		/*
		 * We've searched the table for a matching speed.  If we've
		 * found the correct rate line, we'll get the pre-calc'd
		 * values for the TMC and baud rate divisor for subsequent
		 * use...
		 */
		if (rtp->target > 0) {	/* use table-provided values */
			gotspeed = wanted;
			tmc_v = rtp->tmc_reg;
			br_v = rtp->br_reg;
		} else {	/* otherwise assume 1MBit comm rate */
			gotspeed = 10000;
			tmc_v = 5;
			br_v = 1;
		}

		/*
		 * Now we mask in the enable clock output for the MCR:
		 */
		mcr_v |= etc0vals[etcndx + portndx];

		/*
		 * Now we'll program the registers with these speed- related
		 * contents...
		 */
		SRC_PUT8(hc->sca_base, msci->tmc, tmc_v);
		SRC_PUT8(hc->sca_base, msci->trc0, fifo_v);
		SRC_PUT8(hc->sca_base, msci->rxs, SCA_RXS_CLK_INT + br_v);
		SRC_PUT8(hc->sca_base, msci->txs, SCA_TXS_CLK_INT + br_v);

		switch (hc->cardtype) {
		case SR_CRD_N2PCI:
			*fecrp = mcr_v;
			break;
		case SR_CRD_N2:
		default:
			outb(hc->iobase + SR_MCR, mcr_v);
		}

#if BUGGY > 0
		if (wanted != gotspeed)
			printf("sr%d: Speed wanted=%d, found=%d\n",
			       wanted, gotspeed);

		printf("sr%d: Internal Clock %dx100 BPS, tmc=%d, div=%d\n",
		       portndx, gotspeed, tmc_v, br_v);
#endif
#else
		SRC_PUT8(hc->sca_base, msci->rxs,
			 SCA_RXS_CLK_INT | SCA_RXS_DIV1);
		SRC_PUT8(hc->sca_base, msci->txs,
			 SCA_TXS_CLK_INT | SCA_TXS_DIV1);

		SRC_PUT8(hc->sca_base, msci->tmc, 5);

		if (portndx == 0)
			switch (hc->cardtype) {
			case SR_CRD_N2PCI:
				fecrp = (u_int *)(hc->sca_base + SR_FECR);
				*fecrp |= SR_FECR_ETC0;
				break;
			case SR_CRD_N2:
			default:
				mcr_v = inb(hc->iobase + SR_MCR);
				mcr_v |= SR_MCR_ETC0;
				outb(hc->iobase + SR_MCR, mcr_v);
			}
		else
			switch (hc->cardtype) {
			case SR_CRD_N2:
				mcr_v = inb(hc->iobase + SR_MCR);
				mcr_v |= SR_MCR_ETC1;
				outb(hc->iobase + SR_MCR, mcr_v);
				break;
			case SR_CRD_N2PCI:
				fecrp = (u_int *)(hc->sca_base + SR_FECR);
				*fecrp |= SR_FECR_ETC1;
				break;
			}
#endif
	}

	/*
	 * XXX Disable all interrupts for now. I think if you are using the
	 * dmac you don't use these interrupts.
	 */
	SRC_PUT8(hc->sca_base, msci->ie0, 0);
	SRC_PUT8(hc->sca_base, msci->ie1, 0x0C);
	SRC_PUT8(hc->sca_base, msci->ie2, 0);
	SRC_PUT8(hc->sca_base, msci->fie, 0);

	SRC_PUT8(hc->sca_base, msci->sa0, 0);
	SRC_PUT8(hc->sca_base, msci->sa1, 0);

	SRC_PUT8(hc->sca_base, msci->idl, 0x7E);	/* set flags value */

	SRC_PUT8(hc->sca_base, msci->rrc, 0x0E);
	SRC_PUT8(hc->sca_base, msci->trc0, 0x10);
	SRC_PUT8(hc->sca_base, msci->trc1, 0x1F);
}

/*
 * Configure the rx dma controller.
 */
static void
sr_init_rx_dmac(struct sr_softc *sc)
{
	struct sr_hardc *hc;
	dmac_channel *dmac;
	sca_descriptor *rxd;
	u_int cda_v, sarb_v, rxbuf, rxda, rxda_d;

#if BUGGY > 0
	printf("sr_init_rx_dmac(sc=%08x)\n", sc);
#endif

	hc = sc->hc;
	dmac = &hc->sca->dmac[DMAC_RXCH(sc->scachan)];

	if (hc->mempages)
		SRC_SET_MEM(hc->iobase, sc->rxdesc);

	/*
	 * This phase initializes the contents of the descriptor table
	 * needed to construct a circular buffer...
	 */
	rxd = (sca_descriptor *)(hc->mem_start + (sc->rxdesc & hc->winmsk));
	rxda_d = (uintptr_t) hc->mem_start - (sc->rxdesc & ~hc->winmsk);

	for (rxbuf = sc->rxstart;
	     rxbuf < sc->rxend;
	     rxbuf += SR_BUF_SIZ, rxd++) {
		/*
		 * construct the circular chain...
		 */
		rxda = (uintptr_t) &rxd[1] - rxda_d + hc->mem_pstart;
		rxd->cp = (u_short)(rxda & 0xffff);

		/*
		 * set the on-card buffer address...
		 */
		rxd->bp = (u_short)((rxbuf + hc->mem_pstart) & 0xffff);
		rxd->bpb = (u_char)(((rxbuf + hc->mem_pstart) >> 16) & 0xff);

		rxd->len = 0;	/* bytes resident w/in granule */
		rxd->stat = 0xff;	/* The sca write here when finished */
	}

	/*
	 * heal the chain so that the last entry points to the first...
	 */
	rxd--;
	rxd->cp = (u_short)((sc->rxdesc + hc->mem_pstart) & 0xffff);

	/*
	 * reset the reception handler's index...
	 */
	sc->rxhind = 0;

	/*
	 * We'll now configure the receiver's DMA logic...
	 */
	SRC_PUT8(hc->sca_base, dmac->dsr, 0);	/* Disable DMA transfer */
	SRC_PUT8(hc->sca_base, dmac->dcr, SCA_DCR_ABRT);

	/* XXX maybe also SCA_DMR_CNTE */
	SRC_PUT8(hc->sca_base, dmac->dmr, SCA_DMR_TMOD | SCA_DMR_NF);
	SRC_PUT16(hc->sca_base, dmac->bfl, SR_BUF_SIZ);

	cda_v = (u_short)((sc->rxdesc + hc->mem_pstart) & 0xffff);
	sarb_v = (u_char)(((sc->rxdesc + hc->mem_pstart) >> 16) & 0xff);

	SRC_PUT16(hc->sca_base, dmac->cda, cda_v);
	SRC_PUT8(hc->sca_base, dmac->sarb, sarb_v);

	rxd = (sca_descriptor *)(uintptr_t)sc->rxstart;

	SRC_PUT16(hc->sca_base, dmac->eda,
		  (u_short)((uintptr_t) &rxd[sc->rxmax - 1] & 0xffff));

	SRC_PUT8(hc->sca_base, dmac->dir, 0xF0);


	SRC_PUT8(hc->sca_base, dmac->dsr, SCA_DSR_DE);	/* Enable DMA */
}

/*
 * Configure the TX DMA descriptors.
 * Initialize the needed values and chain the descriptors.
 */
static void
sr_init_tx_dmac(struct sr_softc *sc)
{
	int blk;
	u_int txbuf, txda, txda_d;
	struct sr_hardc *hc;
	sca_descriptor *txd;
	dmac_channel *dmac;
	struct buf_block *blkp;
	u_int x;
	u_int sarb_v;

#if BUGGY > 0
	printf("sr_init_tx_dmac(sc=%08x)\n", sc);
#endif

	hc = sc->hc;
	dmac = &hc->sca->dmac[DMAC_TXCH(sc->scachan)];

	if (hc->mempages)
		SRC_SET_MEM(hc->iobase, sc->block[0].txdesc);

	/*
	 * Initialize the array of descriptors for transmission
	 */
	for (blk = 0; blk < SR_TX_BLOCKS; blk++) {
		blkp = &sc->block[blk];
		txd = (sca_descriptor *)(hc->mem_start
					 + (blkp->txdesc & hc->winmsk));
		txda_d = (uintptr_t) hc->mem_start
		    - (blkp->txdesc & ~hc->winmsk);

		x = 0;
		txbuf = blkp->txstart;
		for (; txbuf < blkp->txend; txbuf += SR_BUF_SIZ, txd++) {
			txda = (uintptr_t) &txd[1] - txda_d + hc->mem_pstart;
			txd->cp = (u_short)(txda & 0xffff);

			txd->bp = (u_short)((txbuf + hc->mem_pstart)
					    & 0xffff);
			txd->bpb = (u_char)(((txbuf + hc->mem_pstart) >> 16)
					    & 0xff);
			txd->len = 0;
			txd->stat = 0;
			x++;
		}

		txd--;
		txd->cp = (u_short)((blkp->txdesc + hc->mem_pstart)
				    & 0xffff);

		blkp->txtail = (uintptr_t)txd - (uintptr_t)hc->mem_start;
	}

	SRC_PUT8(hc->sca_base, dmac->dsr, 0);	/* Disable DMA */
	SRC_PUT8(hc->sca_base, dmac->dcr, SCA_DCR_ABRT);
	SRC_PUT8(hc->sca_base, dmac->dmr, SCA_DMR_TMOD | SCA_DMR_NF);
	SRC_PUT8(hc->sca_base, dmac->dir,
		 SCA_DIR_EOT | SCA_DIR_BOF | SCA_DIR_COF);

	sarb_v = (sc->block[0].txdesc + hc->mem_pstart) >> 16;
	sarb_v &= 0x00ff;

	SRC_PUT8(hc->sca_base, dmac->sarb, (u_char) sarb_v);
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
sr_packet_avail(struct sr_softc *sc, int *len, u_char *rxstat)
{
	int granules;	/* count of granules in pkt */
	int wki, wko;
	struct sr_hardc *hc;
	sca_descriptor *rxdesc;	/* current descriptor */
	sca_descriptor *endp;	/* ending descriptor */
	sca_descriptor *cda;	/* starting descriptor */

	hc = sc->hc;		/* get card's information */

	/*
	 * set up starting descriptor by pulling that info from the DMA half
	 * of the HD chip...
	 */
	wki = DMAC_RXCH(sc->scachan);
	wko = SRC_GET16(hc->sca_base, hc->sca->dmac[wki].cda);

	cda = (sca_descriptor *)(hc->mem_start + (wko & hc->winmsk));

#if BUGGY > 1
	printf("sr_packet_avail(): wki=%d, wko=%04x, cda=%08x\n",
	       wki, wko, cda);
#endif

	/*
	 * open the appropriate memory window and set our expectations...
	 */
	if (hc->mempages) {
		SRC_SET_MEM(hc->iobase, sc->rxdesc);
		SRC_SET_ON(hc->iobase);
	}
	rxdesc = (sca_descriptor *)
	    (hc->mem_start + (sc->rxdesc & hc->winmsk));
	endp = rxdesc;
	rxdesc = &rxdesc[sc->rxhind];
	endp = &endp[sc->rxmax];

	*len = 0;		/* reset result total length */
	granules = 0;		/* reset count of granules */

	/*
	 * This loop will scan descriptors, but it *will* puke up if we wrap
	 * around to our starting point...
	 */
	while (rxdesc != cda) {
		*len += rxdesc->len;	/* increment result length */
		granules++;

		/*
		 * If we hit a valid packet's completion we'll know we've
		 * got a live one, and that we can deliver the packet.
		 * Since we're only allowed to report a packet available,
		 * somebody else does that...
		 */
		if (rxdesc->stat & SCA_DESC_EOM) {	/* End Of Message */
			*rxstat = rxdesc->stat;	/* return closing */
#if BUGGY > 0
			printf("sr%d: PKT AVAIL len %d, %x, bufs %u.\n",
			       sc->unit, *len, *rxstat, granules);
#endif
			return 1;	/* indicate success */
		}
		/*
		 * OK, this packet take up multiple granules.  Move on to
		 * the next descriptor so we can consider it...
		 */
		rxdesc++;

		if (rxdesc == endp)	/* recognize & act on wrap point */
			rxdesc = (sca_descriptor *)
			    (hc->mem_start + (sc->rxdesc & hc->winmsk));
	}

	/*
	 * Nothing found in the DPRAM.  Let the caller know...
	 */
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
sr_copy_rxbuf(struct mbuf *m, struct sr_softc *sc, int len)
{
	struct sr_hardc *hc;
	sca_descriptor *rxdesc;
	u_int rxdata;
	u_int rxmax;
	u_int off = 0;
	u_int tlen;

#if BUGGY > 0
	printf("sr_copy_rxbuf(m=%08x,sc=%08x,len=%d)\n",
	       m, sc, len);
#endif

	hc = sc->hc;

	rxdata = sc->rxstart + (sc->rxhind * SR_BUF_SIZ);
	rxmax = sc->rxstart + (sc->rxmax * SR_BUF_SIZ);

	rxdesc = (sca_descriptor *)
	    (hc->mem_start + (sc->rxdesc & hc->winmsk));
	rxdesc = &rxdesc[sc->rxhind];

	/*
	 * Using the count of bytes in the received packet, we decrement it
	 * for each granule (controller by an SCA descriptor) to control the
	 * looping...
	 */
	while (len) {
		/*
		 * tlen gets the length of *this* granule... ...which is
		 * then copied to the target buffer.
		 */
		tlen = (len < SR_BUF_SIZ) ? len : SR_BUF_SIZ;

		if (hc->mempages)
			SRC_SET_MEM(hc->iobase, rxdata);

		bcopy(hc->mem_start + (rxdata & hc->winmsk),
		      mtod(m, caddr_t) +off,
		      tlen);

		off += tlen;
		len -= tlen;

		/*
		 * now, return to the descriptor's window in DPRAM and reset
		 * the descriptor we've just suctioned...
		 */
		if (hc->mempages)
			SRC_SET_MEM(hc->iobase, sc->rxdesc);

		rxdesc->len = 0;
		rxdesc->stat = 0xff;

		/*
		 * Move on to the next granule.  If we've any remaining
		 * bytes to process we'll just continue in our loop...
		 */
		rxdata += SR_BUF_SIZ;
		rxdesc++;

		if (rxdata == rxmax) {	/* handle the wrap point */
			rxdata = sc->rxstart;
			rxdesc = (sca_descriptor *)
			    (hc->mem_start + (sc->rxdesc & hc->winmsk));
		}
	}
}

/*
 * If single is set, just eat a packet. Otherwise eat everything up to
 * where cda points. Update pointers to point to the next packet.
 *
 * This handles "flushing" of a packet as received...
 *
 * If the "single" parameter is zero, all pending reeceive traffic will
 * be flushed out of existence.  A non-zero value will only drop the
 * *next* (currently) pending packet...
 */
static void
sr_eat_packet(struct sr_softc *sc, int single)
{
	struct sr_hardc *hc;
	sca_descriptor *rxdesc;	/* current descriptor being eval'd */
	sca_descriptor *endp;	/* last descriptor in chain */
	sca_descriptor *cda;	/* current start point */
	u_int loopcnt = 0;	/* count of packets flushed ??? */
	u_char stat;		/* captured status byte from descr */

	hc = sc->hc;
	cda = (sca_descriptor *)(hc->mem_start +
				 (SRC_GET16(hc->sca_base,
				  hc->sca->dmac[DMAC_RXCH(sc->scachan)].cda) &
				  hc->winmsk));

	/*
	 * loop until desc->stat == (0xff || EOM) Clear the status and
	 * length in the descriptor. Increment the descriptor.
	 */
	if (hc->mempages)
		SRC_SET_MEM(hc->iobase, sc->rxdesc);

	rxdesc = (sca_descriptor *)
	    (hc->mem_start + (sc->rxdesc & hc->winmsk));
	endp = rxdesc;
	rxdesc = &rxdesc[sc->rxhind];
	endp = &endp[sc->rxmax];

	/*
	 * allow loop, but abort it if we wrap completely...
	 */
	while (rxdesc != cda) {
		loopcnt++;

		if (loopcnt > sc->rxmax) {
			printf("sr%d: eat pkt %d loop, cda %p, "
			       "rxdesc %p, stat %x.\n",
			       sc->unit, loopcnt, cda, rxdesc,
			       rxdesc->stat);
			break;
		}
		stat = rxdesc->stat;

		rxdesc->len = 0;
		rxdesc->stat = 0xff;

		rxdesc++;
		sc->rxhind++;

		if (rxdesc == endp) {
			rxdesc = (sca_descriptor *)
			    (hc->mem_start + (sc->rxdesc & hc->winmsk));
			sc->rxhind = 0;
		}
		if (single && (stat == SCA_DESC_EOM))
			break;
	}

	/*
	 * Update the eda to the previous descriptor.
	 */
	rxdesc = (sca_descriptor *)(uintptr_t)sc->rxdesc;
	rxdesc = &rxdesc[(sc->rxhind + sc->rxmax - 2) % sc->rxmax];

	SRC_PUT16(hc->sca_base,
		  hc->sca->dmac[DMAC_RXCH(sc->scachan)].eda,
		  (u_short)(((uintptr_t)rxdesc + hc->mem_pstart) & 0xffff));
}

/*
 * While there is packets available in the rx buffer, read them out
 * into mbufs and ship them off.
 */
static void
sr_get_packets(struct sr_softc *sc)
{
	u_char rxstat;		/* acquired status byte */
	int i;
	int pkts;		/* count of packets found */
	int rxndx;		/* rcv buffer index */
	int tries;		/* settling time counter */
	u_int len;		/* length of pending packet */
	struct sr_hardc *hc;	/* card-level information */
	sca_descriptor *rxdesc;	/* descriptor in memory */
#ifndef NETGRAPH
	struct ifnet *ifp;	/* network intf ctl table */
#else
	int error;
#endif /* NETGRAPH */
	struct mbuf *m = NULL;	/* message buffer */

#if BUGGY > 0
	printf("sr_get_packets(sc=%08x)\n", sc);
#endif

	hc = sc->hc;
#ifndef NETGRAPH
	ifp = &sc->ifsppp.pp_if;
#endif /* NETGRAPH */

	if (hc->mempages) {
		SRC_SET_MEM(hc->iobase, sc->rxdesc);
		SRC_SET_ON(hc->iobase);	/* enable shared memory */
	}
	pkts = 0;		/* reset count of found packets */

	/*
	 * for each complete packet in the receiving pool, process each
	 * packet...
	 */
	while (sr_packet_avail(sc, &len, &rxstat)) {	/* packet pending? */
		/*
		 * I have seen situations where we got the interrupt but the
		 * status value wasn't deposited.  This code should allow
		 * the status byte's value to settle...
		 */

		tries = 5;

		while ((rxstat == 0x00ff)
		       && --tries)
			sr_packet_avail(sc, &len, &rxstat);

#if BUGGY > 1
		printf("sr_packet_avail() returned len=%d, rxstat=%02ux\n",
		       len, rxstat);
#endif

		pkts++;
#ifdef NETGRAPH
		sc->inbytes += len;
		sc->inlast = 0;
#endif /* NETGRAPH */

		/*
		 * OK, we've settled the incoming message status. We can now
		 * process it...
		 */
		if (((rxstat & SCA_DESC_ERRORS) == 0) && (len < MCLBYTES)) {
#if BUGGY > 1
			printf("sr%d: sr_get_packet() rxstat=%02x, len=%d\n",
			       sc->unit, rxstat, len);
#endif

			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				/*
				 * eat (flush) packet if get mbuf fail!!
				 */
				sr_eat_packet(sc, 1);
				continue;
			}
			/*
			 * construct control information for pass-off
			 */
#ifndef NETGRAPH
			m->m_pkthdr.rcvif = ifp;
#else
			m->m_pkthdr.rcvif = NULL;
#endif /* NETGRAPH */
			m->m_pkthdr.len = m->m_len = len;
			if (len > MHLEN) {
				MCLGET(m, M_DONTWAIT);
				if ((m->m_flags & M_EXT) == 0) {
					/*
					 * We couldn't get a big enough
					 * message packet, so we'll send the
					 * packet to /dev/null...
					 */
					m_freem(m);
					sr_eat_packet(sc, 1);
					continue;
				}
			}
			/*
			 * OK, we've got a good message buffer.  Now we can
			 * copy the received message into it
			 */
			sr_copy_rxbuf(m, sc, len);	/* copy from DPRAM */

#ifndef NETGRAPH
			BPF_MTAP(ifp, m);

#if BUGGY > 3
			{
				u_char *bp;

				bp = (u_char *)m;
				printf("sr%d: rcvd=%02x%02x%02x%02x%02x%02x\n",
				       sc->unit,
				       bp[0], bp[1], bp[2],
				       bp[4], bp[5], bp[6]);
			}
#endif
			sppp_input(ifp, m);
			ifp->if_ipackets++;

#else	/* NETGRAPH */
#if BUGGY > 3
			{
				u_char *bp;

				bp = mtod(m,u_char *);
				printf("sr%d: rd=%02x:%02x:%02x:%02x:%02x:%02x",
				       sc->unit,
				       bp[0], bp[1], bp[2],
				       bp[4], bp[5], bp[6]);
				printf(":%02x:%02x:%02x:%02x:%02x:%02x\n",
				       bp[6], bp[7], bp[8],
				       bp[9], bp[10], bp[11]);
			}
#endif
			NG_SEND_DATA_ONLY(error, sc->hook, m);
			sc->ipackets++;
#endif /* NETGRAPH */
			/*
			 * Update the eda to the previous descriptor.
			 */
			i = (len + SR_BUF_SIZ - 1) / SR_BUF_SIZ;
			sc->rxhind = (sc->rxhind + i) % sc->rxmax;

			rxdesc = (sca_descriptor *)(uintptr_t)sc->rxdesc;
			rxndx = (sc->rxhind + sc->rxmax - 2) % sc->rxmax;
			rxdesc = &rxdesc[rxndx];

			SRC_PUT16(hc->sca_base,
				  hc->sca->dmac[DMAC_RXCH(sc->scachan)].eda,
				  (u_short)(((uintptr_t)rxdesc + hc->mem_pstart)
					     & 0xffff));

		} else {
			int got_st3, got_cda, got_eda;
			int tries = 5;

			while ((rxstat == 0xff) && --tries)
				sr_packet_avail(sc, &len, &rxstat);

			/*
			 * It look like we get an interrupt early
			 * sometimes and then the status is not
			 * filled in yet.
			 */
			if (tries && (tries != 5))
				continue;

			/*
			 * This chunk of code handles the error packets.
			 * We'll log them for posterity...
			 */
			sr_eat_packet(sc, 1);

#ifndef NETGRAPH
			ifp->if_ierrors++;
#else
			sc->ierrors[0]++;
#endif /* NETGRAPH */

			got_st3 = SRC_GET8(hc->sca_base,
				  hc->sca->msci[sc->scachan].st3);
			got_cda = SRC_GET16(hc->sca_base,
				  hc->sca->dmac[DMAC_RXCH(sc->scachan)].cda);
			got_eda = SRC_GET16(hc->sca_base,
				  hc->sca->dmac[DMAC_RXCH(sc->scachan)].eda);

#if BUGGY > 0
			printf("sr%d: Receive error chan %d, "
			       "stat %02x, msci st3 %02x,"
			       "rxhind %d, cda %04x, eda %04x.\n",
			       sc->unit, sc->scachan, rxstat,
			       got_st3, sc->rxhind, got_cda, got_eda);
#endif
		}
	}

#if BUGGY > 0
	printf("sr%d: sr_get_packets() found %d packet(s)\n",
	       sc->unit, pkts);
#endif

	if (hc->mempages)
		SRC_SET_OFF(hc->iobase);
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
	u_char dsr;		/* contents of DMA Stat Reg */
	u_char dotxstart;	/* enables for tranmit part */
	int mch;		/* channel being processed */
	struct sr_softc *sc;	/* channel's softc structure */
	sca_regs *sca = hc->sca;
	dmac_channel *dmac;	/* dma structure of chip */

#if BUGGY > 0
	printf("sr_dmac_intr(hc=%08x,isr1=%04x)\n", hc, isr1);
#endif

	mch = 0;		/* assume chan0 on card */
	dotxstart = isr1;	/* copy for xmitter starts */

	/*
	 * Shortcut if there is no interrupts for dma channel 0 or 1.
	 * Skip processing for channel 0 if no incoming hit
	 */
	if ((isr1 & 0x0F) == 0) {
		mch = 1;
		isr1 >>= 4;
	}
	do {
		sc = &hc->sc[mch];

		/*
		 * Transmit channel - DMA Status Register Evaluation
		 */
		if (isr1 & 0x0C) {
			dmac = &sca->dmac[DMAC_TXCH(mch)];

			/*
			 * get the DMA Status Register contents and write
			 * back to reset interrupt...
			 */
			dsr = SRC_GET8(hc->sca_base, dmac->dsr);
			SRC_PUT8(hc->sca_base, dmac->dsr, dsr);

			/*
			 * Check for (& process) a Counter overflow
			 */
			if (dsr & SCA_DSR_COF) {
				printf("sr%d: TX DMA Counter overflow, "
				       "txpacket no %lu.\n",
#ifndef NETGRAPH
				       sc->unit, sc->ifsppp.pp_if.if_opackets);
				sc->ifsppp.pp_if.if_oerrors++;
#else
				       sc->unit, sc->opackets);
				sc->oerrors++;
#endif /* NETGRAPH */
			}
			/*
			 * Check for (& process) a Buffer overflow
			 */
			if (dsr & SCA_DSR_BOF) {
				printf("sr%d: TX DMA Buffer overflow, "
				       "txpacket no %lu, dsr %02x, "
				       "cda %04x, eda %04x.\n",
#ifndef NETGRAPH
				       sc->unit, sc->ifsppp.pp_if.if_opackets,
#else
				       sc->unit, sc->opackets,
#endif /* NETGRAPH */
				       dsr,
				       SRC_GET16(hc->sca_base, dmac->cda),
				       SRC_GET16(hc->sca_base, dmac->eda));
#ifndef NETGRAPH
				sc->ifsppp.pp_if.if_oerrors++;
#else
				sc->oerrors++;
#endif /* NETGRAPH */
			}
			/*
			 * Check for (& process) an End of Transfer (OK)
			 */
			if (dsr & SCA_DSR_EOT) {
				/*
				 * This should be the most common case.
				 *
				 * Clear the IFF_OACTIVE flag.
				 *
				 * Call srstart to start a new transmit if
				 * there is data to transmit.
				 */
#if BUGGY > 0
				printf("sr%d: TX Completed OK\n", sc->unit);
#endif
				sc->xmit_busy = 0;
#ifndef NETGRAPH
				sc->ifsppp.pp_if.if_flags &= ~IFF_OACTIVE;
				sc->ifsppp.pp_if.if_timer = 0;
#else
				/* XXX may need to mark tx inactive? */
				sc->out_deficit++;
				sc->out_dog = DOG_HOLDOFF;
#endif /* NETGRAPH */

				if (sc->txb_inuse && --sc->txb_inuse)
					sr_xmit(sc);
			}
		}
		/*
		 * Receive channel processing of DMA Status Register
		 */
		if (isr1 & 0x03) {
			dmac = &sca->dmac[DMAC_RXCH(mch)];

			dsr = SRC_GET8(hc->sca_base, dmac->dsr);
			SRC_PUT8(hc->sca_base, dmac->dsr, dsr);

			/*
			 * End of frame processing (MSG OK?)
			 */
			if (dsr & SCA_DSR_EOM) {
#if BUGGY > 0
				int tt, ind;

#ifndef NETGRAPH
				tt = sc->ifsppp.pp_if.if_ipackets;
#else	/* NETGRAPH */
				tt = sc->ipackets;
#endif /* NETGRAPH */
				ind = sc->rxhind;
#endif

				sr_get_packets(sc);
#if BUGGY > 0
#ifndef NETGRAPH
				if (tt == sc->ifsppp.pp_if.if_ipackets)
#else	/* NETGRAPH */
				if (tt == sc->ipackets)
#endif /* NETGRAPH */
				{
					sca_descriptor *rxdesc;
					int i;

					printf("SR: RXINTR isr1 %x, dsr %x, "
					       "no data %d pkts, orxind %d.\n",
					       dotxstart, dsr, tt, ind);
					printf("SR: rxdesc %x, rxstart %x, "
					       "rxend %x, rxhind %d, "
					       "rxmax %d.\n",
					       sc->rxdesc, sc->rxstart,
					       sc->rxend, sc->rxhind,
					       sc->rxmax);
					printf("SR: cda %x, eda %x.\n",
					    SRC_GET16(hc->sca_base, dmac->cda),
					    SRC_GET16(hc->sca_base, dmac->eda));

					if (hc->mempages) {
						SRC_SET_ON(hc->iobase);
						SRC_SET_MEM(hc->iobase, sc->rxdesc);
					}
					rxdesc = (sca_descriptor *)
					         (hc->mem_start +
					          (sc->rxdesc & hc->winmsk));
					rxdesc = &rxdesc[sc->rxhind];

					for (i = 0; i < 3; i++, rxdesc++)
						printf("SR: rxdesc->stat %x, "
						       "len %d.\n",
						       rxdesc->stat,
						       rxdesc->len);

					if (hc->mempages)
						SRC_SET_OFF(hc->iobase);
				}
#endif /* BUGGY */
			}
			/*
			 * Check for Counter overflow
			 */
			if (dsr & SCA_DSR_COF) {
				printf("sr%d: RX DMA Counter overflow, "
				       "rxpkts %lu.\n",
#ifndef NETGRAPH
				       sc->unit, sc->ifsppp.pp_if.if_ipackets);
				sc->ifsppp.pp_if.if_ierrors++;
#else	/* NETGRAPH */
				       sc->unit, sc->ipackets);
				sc->ierrors[1]++;
#endif /* NETGRAPH */
			}
			/*
			 * Check for Buffer overflow
			 */
			if (dsr & SCA_DSR_BOF) {
				printf("sr%d: RX DMA Buffer overflow, "
				       "rxpkts %lu, rxind %d, "
				       "cda %x, eda %x, dsr %x.\n",
#ifndef NETGRAPH
				       sc->unit, sc->ifsppp.pp_if.if_ipackets,
#else	/* NETGRAPH */
				       sc->unit, sc->ipackets,
#endif /* NETGRAPH */
				       sc->rxhind,
				       SRC_GET16(hc->sca_base, dmac->cda),
				       SRC_GET16(hc->sca_base, dmac->eda),
				       dsr);

				/*
				 * Make sure we eat as many as possible.
				 * Then get the system running again.
				 */
				if (hc->mempages)
					SRC_SET_ON(hc->iobase);

				sr_eat_packet(sc, 0);
#ifndef NETGRAPH
				sc->ifsppp.pp_if.if_ierrors++;
#else	/* NETGRAPH */
				sc->ierrors[2]++;
#endif /* NETGRAPH */

				SRC_PUT8(hc->sca_base,
					 sca->msci[mch].cmd,
					 SCA_CMD_RXMSGREJ);

				SRC_PUT8(hc->sca_base, dmac->dsr, SCA_DSR_DE);

#if BUGGY > 0
				printf("sr%d: RX DMA Buffer overflow, "
				       "rxpkts %lu, rxind %d, "
				       "cda %x, eda %x, dsr %x. After\n",
				       sc->unit,
#ifndef NETGRAPH
				       sc->ipackets,
#else	/* NETGRAPH */
				       sc->ifsppp.pp_if.if_ipackets,
#endif /* NETGRAPH */
				       sc->rxhind,
				       SRC_GET16(hc->sca_base, dmac->cda),
				       SRC_GET16(hc->sca_base, dmac->eda),
				       SRC_GET8(hc->sca_base, dmac->dsr));
#endif

				if (hc->mempages)
					SRC_SET_OFF(hc->iobase);
			}
			/*
			 * End of Transfer
			 */
			if (dsr & SCA_DSR_EOT) {
				/*
				 * If this happen, it means that we are
				 * receiving faster than what the processor
				 * can handle.
				 * 
				 * XXX We should enable the dma again.
				 */
				printf("sr%d: RX End of xfer, rxpkts %lu.\n",
				       sc->unit,
#ifndef NETGRAPH
				       sc->ifsppp.pp_if.if_ipackets);
				sc->ifsppp.pp_if.if_ierrors++;
#else
				       sc->ipackets);
				sc->ierrors[3]++;
#endif /* NETGRAPH */
			}
		}
		isr1 >>= 4;	/* process next half of ISR */
		mch++;		/* and move to next channel */
	} while ((mch < NCHAN) && isr1);	/* loop for each chn */

	/*
	 * Now that we have done all the urgent things, see if we can fill
	 * the transmit buffers.
	 */
	for (mch = 0; mch < NCHAN; mch++) {
		if (dotxstart & 0x0C) {	/* TX initiation enabled? */
			sc = &hc->sc[mch];
#ifndef NETGRAPH
			srstart(&sc->ifsppp.pp_if);
#else
			srstart(sc);
#endif /* NETGRAPH */
		}
		dotxstart >>= 4;/* shift for next channel */
	}
}
#ifndef NETGRAPH
#ifdef USE_MODEMCK
/*
 * Perform timeout on an FR channel 
 *
 * Establish a periodic check of open N2 ports;  If
 * a port is open/active, its DCD state is checked
 * and a loss of DCD is recognized (and eventually
 * processed).
 */
static void
sr_modemck(void *arg)
{
	u_int s;
	int card;		/* card index in table */
	int cards;		/* card list index */
	int mch;		/* channel on card */
	u_char dcd_v;		/* Data Carrier Detect */
	u_char got_st0;		/* contents of ST0 */
	u_char got_st1;		/* contents of ST1 */
	u_char got_st2;		/* contents of ST2 */
	u_char got_st3;		/* contents of ST3 */
	struct sr_hardc *hc;	/* card's configuration */
	struct sr_hardc *Card[16];/* up to 16 cards in system */
	struct sr_softc *sc;	/* channel's softc structure */
	struct ifnet *ifp;	/* interface control table */
	msci_channel *msci;	/* regs specific to channel */

	s = splimp();

#if	0
	if (sr_opens == 0) {	/* count of "up" channels */
		sr_watcher = 0;	/* indicate no watcher */
		splx(s);
		return;
	}
#endif

	sr_watcher = 1;		/* mark that we're online */

	/*
	 * Now we'll need a list of cards to process.  Since we can handle
	 * both ISA and PCI cards (and I didn't think of making this logic
	 * global YET) we'll generate a single table of card table
	 * addresses.
	 */
	cards = 0;

	for (card = 0; card < NSR; card++) {
		hc = &sr_hardc[card];

		if (hc->sc == (void *)0)
			continue;

		Card[cards++] = hc;
	}

	hc = sr_hardc_pci;

	while (hc) {
		Card[cards++] = hc;
		hc = hc->next;
	}

	/*
	 * OK, we've got work we can do.  Let's do it... (Please note that
	 * this code _only_ deals w/ ISA cards)
	 */
	for (card = 0; card < cards; card++) {
		hc = Card[card];/* get card table */

		for (mch = 0; mch < hc->numports; mch++) {
			sc = &hc->sc[mch];

			ifp = &sc->ifsppp.pp_if;

			/*
			 * if this channel isn't "up", skip it
			 */
			if ((ifp->if_flags & IFF_UP) == 0)
				continue;

			/*
			 * OK, now we can go looking at this channel's
			 * actual register contents...
			 */
			msci = &hc->sca->msci[sc->scachan];

			/*
			 * OK, now we'll look into the actual status of this
			 * channel...
			 * 
			 * I suck in more registers than strictly needed
			 */
			got_st0 = SRC_GET8(hc->sca_base, msci->st0);
			got_st1 = SRC_GET8(hc->sca_base, msci->st1);
			got_st2 = SRC_GET8(hc->sca_base, msci->st2);
			got_st3 = SRC_GET8(hc->sca_base, msci->st3);

			/*
			 * We want to see if the DCD signal is up (DCD is
			 * true if zero)
			 */
			dcd_v = (got_st3 & SCA_ST3_DCD) == 0;

			if (dcd_v == 0)
				printf("sr%d: DCD lost\n", sc->unit);
		}
	}

	/*
	 * OK, now set up for the next modem signal checking pass...
	 */
	timeout(sr_modemck, NULL, hz);

	splx(s);
}
#endif
#else	/* NETGRAPH */
/*
 * If a port is open/active, it's DCD state is checked
 * and a loss of DCD is recognized (and eventually processed?).
 */
static void
sr_modemck(struct sr_softc *sc )
{
	u_int s;
	u_char got_st3;			/* contents of ST3 */
	struct sr_hardc *hc = sc->hc;	/* card's configuration */
	msci_channel *msci;		/* regs specific to channel */

	s = splimp();


	if (sc->running == 0)
		return;
	/*
	 * OK, now we can go looking at this channel's register contents...
	 */
	msci = &hc->sca->msci[sc->scachan];
	got_st3 = SRC_GET8(hc->sca_base, msci->st3);

	/*
	 * We want to see if the DCD signal is up (DCD is true if zero)
	 */
	sc->dcd = (got_st3 & SCA_ST3_DCD) == 0;
	splx(s);
}

#endif	/* NETGRAPH */
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

#ifdef	NETGRAPH
/*****************************************
 * Device timeout/watchdog routine.
 * called once per second.
 * checks to see that if activity was expected, that it hapenned.
 * At present we only look to see if expected output was completed.
 */
static void
ngsr_watchdog_frame(void * arg)
{
	struct sr_softc * sc = arg;
	int s;
	int	speed;

	if (sc->running == 0)
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
		log(LOG_ERR, "sr%d: No response from remote end\n", sc->unit);
		s = splimp();
		sr_down(sc);
		sr_up(sc);
		sc->inlast = sc->out_deficit = 0;
		splx(s);
	} else if ( sc->xmit_busy ) { /* no TX -> no TX timeouts */
		if (sc->out_dog == 0) { 
			log(LOG_ERR, "sr%d: Transmit failure.. no clock?\n",
					sc->unit);
			s = splimp();
			srwatchdog(sc);
#if 0
			sr_down(sc);
			sr_up(sc);
#endif
			splx(s);
			sc->inlast = sc->out_deficit = 0;
		} else {
			sc->out_dog--;
		}
	}
	sr_modemck(sc); 	/* update the DCD status */
	sc->handle = timeout(ngsr_watchdog_frame, sc, hz);
}

/***********************************************************************
 * This section contains the methods for the Netgraph interface
 ***********************************************************************/
/*
 * It is not possible or allowable to create a node of this type.
 * If the hardware exists, it will already have created it.
 */
static	int
ngsr_constructor(node_p node)
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
ngsr_newhook(node_p node, hook_p hook, const char *name)
{
	struct sr_softc *	sc = NG_NODE_PRIVATE(node);

	/*
	 * check if it's our friend the debug hook
	 */
	if (strcmp(name, NG_SR_HOOK_DEBUG) == 0) {
		NG_HOOK_SET_PRIVATE(hook, NULL); /* paranoid */
		sc->debug_hook = hook;
		return (0);
	}

	/*
	 * Check for raw mode hook.
	 */
	if (strcmp(name, NG_SR_HOOK_RAW) != 0) {
		return (EINVAL);
	}
	NG_HOOK_SET_PRIVATE(hook, sc);
	sc->hook = hook;
	sc->datahooks++;
	sr_up(sc);
	return (0);
}

/*
 * incoming messages.
 * Just respond to the generic TEXT_STATUS message
 */
static	int
ngsr_rcvmsg(node_p node, item_p item, hook_p lasthook)
{
	struct sr_softc *	sc;
	struct ng_mesg *resp = NULL;
	int error = 0;
	struct ng_mesg *msg;

	NGI_GET_MSG(item,msg);
	sc = NG_NODE_PRIVATE(node);
	switch (msg->header.typecookie) {
	case	NG_SR_COOKIE: 
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
				"ierrors = %ld, %ld, %ld, %ld, %ld, %ld\n",
			    	sc->ierrors[0],
			    	sc->ierrors[1],
			    	sc->ierrors[2],
			    	sc->ierrors[3],
			    	sc->ierrors[4],
			    	sc->ierrors[5]);

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
static	int
ngsr_rcvdata(hook_p hook, item_p item)
{
	int s;
	int error = 0;
	struct sr_softc * sc = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));
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
	srstart(sc);
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
ngsr_shutdown(node_p node)
{
	struct sr_softc * sc = NG_NODE_PRIVATE(node);

	sr_down(sc);
	NG_NODE_UNREF(node);
/* XXX should drain queues! */
	if (ng_make_node_common(&typestruct, &sc->node) != 0)
		return (0);
	sprintf(sc->nodename, "%s%d", NG_SR_NODE_TYPE, sc->unit);
	if (ng_name_node(sc->node, sc->nodename)) {
		printf("node naming failed\n");
		sc->node = NULL;
		NG_NODE_UNREF(sc->node); /* drop it again */
		return (0);
	}
	NG_NODE_SET_PRIVATE(sc->node, sc);
	callout_handle_init(&sc->handle); /* should kill timeout */
	sc->running = 0;
	return (0);
}

/* already linked */
static	int
ngsr_connect(hook_p hook)
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
ngsr_disconnect(hook_p hook)
{
	struct sr_softc * sc = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));
	int	s;
	/*
	 * If it's the data hook, then free resources etc.
	 */
	if (NG_HOOK_PRIVATE(hook)) {
		s = splimp();
		sc->datahooks--;
		if (sc->datahooks == 0)
			sr_down(sc);
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
ngsr_init(void *ignored)
{
	if (ng_newtype(&typestruct))
		printf("ngsr install failed\n");
	ngsr_done_init = 1;
}
#endif /* NETGRAPH */

/*
 ********************************* END ************************************
 */
