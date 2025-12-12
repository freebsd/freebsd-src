/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019-2025 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <vm/vm.h>
#include <vm/vm_page.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_var.h>

#include <machine/bus.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/tiphy.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/xilinx/axidma.h>
#include <dev/xilinx/if_xaereg.h>
#include <dev/xilinx/if_xaevar.h>

#include "miibus_if.h"
#include "axidma_if.h"

#define	XAE_RD4(_sc, _reg)		bus_read_4((_sc)->res[0], _reg)
#define	XAE_RD8(_sc, _reg)		bus_read_8((_sc)->res[0], _reg)
#define	XAE_WR4(_sc, _reg, _val)	bus_write_4((_sc)->res[0], _reg, _val)
#define	XAE_WR8(_sc, _reg, _val)	bus_write_8((_sc)->res[0], _reg, _val)

#define	AXIDMA_RD4(_sc, _reg)		bus_read_4((_sc)->dma_res, _reg)
#define	AXIDMA_RD8(_sc, _reg)		bus_read_8((_sc)->dma_res, _reg)
#define	AXIDMA_WR4(_sc, _reg, _val)	bus_write_4((_sc)->dma_res, _reg, _val)
#define	AXIDMA_WR8(_sc, _reg, _val)	bus_write_8((_sc)->dma_res, _reg, _val)

#define	XAE_LOCK(sc)			mtx_lock(&(sc)->mtx)
#define	XAE_UNLOCK(sc)			mtx_unlock(&(sc)->mtx)
#define	XAE_ASSERT_LOCKED(sc)		mtx_assert(&(sc)->mtx, MA_OWNED)
#define	XAE_ASSERT_UNLOCKED(sc)		mtx_assert(&(sc)->mtx, MA_NOTOWNED)

#define	dprintf(fmt, ...)

#define	MDIO_CLK_DIV_DEFAULT	29

#define	PHY1_RD(sc, _r)		xae_miibus_read_reg(sc->dev, 1, _r)
#define	PHY1_WR(sc, _r, _v)	xae_miibus_write_reg(sc->dev, 1, _r, _v)
#define	PHY_RD(sc, _r)		xae_miibus_read_reg(sc->dev, sc->phy_addr, _r)
#define	PHY_WR(sc, _r, _v)	\
    xae_miibus_write_reg(sc->dev, sc->phy_addr, _r, _v)

/* Use this macro to access regs > 0x1f */
#define WRITE_TI_EREG(sc, reg, data) {					\
	PHY_WR(sc, MII_MMDACR, MMDACR_DADDRMASK);			\
	PHY_WR(sc, MII_MMDAADR, reg);					\
	PHY_WR(sc, MII_MMDACR, MMDACR_DADDRMASK | MMDACR_FN_DATANPI);	\
	PHY_WR(sc, MII_MMDAADR, data);					\
}

/* Not documented, Xilinx VCU118 workaround */
#define	 CFG4_SGMII_TMR			0x160 /* bits 8:7 MUST be '10' */
#define	DP83867_SGMIICTL1		0xD3 /* not documented register */
#define	 SGMIICTL1_SGMII_6W		(1 << 14) /* no idea what it is */

#define	AXI_DESC_RING_ALIGN		64

/*
 * Driver data and defines.
 */

static struct resource_spec xae_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ -1, 0 }
};

static inline uint32_t
next_rxidx(struct xae_softc *sc, uint32_t curidx)
{

	return ((curidx == RX_DESC_COUNT - 1) ? 0 : curidx + 1);
}

static inline uint32_t
next_txidx(struct xae_softc *sc, uint32_t curidx)
{

	return ((curidx == TX_DESC_COUNT - 1) ? 0 : curidx + 1);
}

static void
xae_get1paddr(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{

	if (error != 0)
		return;
	*(bus_addr_t *)arg = segs[0].ds_addr;
}

inline static uint32_t
xae_setup_txdesc(struct xae_softc *sc, int idx, bus_addr_t paddr, 
    uint32_t len)
{
	struct axidma_desc *desc;
	uint32_t nidx;
	uint32_t flags;

	nidx = next_txidx(sc, idx);

	desc = &sc->txdesc_ring[idx];

	/* Addr/len 0 means we're clearing the descriptor after xmit done. */
	if (paddr == 0 || len == 0) {
		flags = 0;
		--sc->txcount;
	} else {
		flags = BD_CONTROL_TXSOF | BD_CONTROL_TXEOF;
		++sc->txcount;
	}

	desc->next = sc->txdesc_ring_paddr + sizeof(struct axidma_desc) * nidx;
	desc->phys = paddr;
	desc->status = 0;
	desc->control = len | flags;

	return (nidx);
}

static int
xae_setup_txbuf(struct xae_softc *sc, int idx, struct mbuf **mp)
{
	struct bus_dma_segment seg;
	struct mbuf *m;
	int error;
	int nsegs;

	dprintf("%s\n", __func__);

	if ((m = m_defrag(*mp, M_NOWAIT)) == NULL)
		return (ENOMEM);

	*mp = m;

	error = bus_dmamap_load_mbuf_sg(sc->txbuf_tag, sc->txbuf_map[idx].map,
	    m, &seg, &nsegs, 0);
	if (error != 0)
		return (ENOMEM);

	bus_dmamap_sync(sc->txbuf_tag, sc->txbuf_map[idx].map,
	    BUS_DMASYNC_PREWRITE);

	sc->txbuf_map[idx].mbuf = m;
	xae_setup_txdesc(sc, idx, seg.ds_addr, seg.ds_len);

	return (0);
}

static void
xae_txstart_locked(struct xae_softc *sc)
{
	struct mbuf *m;
	int enqueued;
	uint32_t addr;
	int tmp;
	if_t ifp;

	dprintf("%s\n", __func__);

	XAE_ASSERT_LOCKED(sc);

	if (!sc->link_is_up)
		return;

	ifp = sc->ifp;

	if (if_getdrvflags(ifp) & IFF_DRV_OACTIVE)
		return;

	enqueued = 0;

	for (;;) {
		if (sc->txcount == (TX_DESC_COUNT - 1)) {
			if_setdrvflagbits(ifp, IFF_DRV_OACTIVE, 0);
			break;
		}
		m = if_dequeue(ifp);
		if (m == NULL)
			break;
		if (xae_setup_txbuf(sc, sc->tx_idx_head, &m) != 0) {
			if_sendq_prepend(ifp, m);
			break;
		}
		BPF_MTAP(ifp, m);
		tmp = sc->tx_idx_head;
		sc->tx_idx_head = next_txidx(sc, sc->tx_idx_head);
		++enqueued;
	}

	if (enqueued != 0) {
		bus_dmamap_sync(sc->txdesc_tag, sc->txdesc_map,
		    BUS_DMASYNC_PREWRITE);

		addr = sc->txdesc_ring_paddr + tmp * sizeof(struct axidma_desc);
		dprintf("%s: new tail desc %x\n", __func__, addr);
		AXIDMA_WR8(sc, AXI_TAILDESC(AXIDMA_TX_CHAN), addr);
	}
}

static void
xae_txfinish_locked(struct xae_softc *sc)
{
	struct axidma_desc *desc;
	struct xae_bufmap *bmap;
	boolean_t retired_buffer;

	XAE_ASSERT_LOCKED(sc);

	bus_dmamap_sync(sc->txdesc_tag, sc->txdesc_map, BUS_DMASYNC_PREREAD);
	bus_dmamap_sync(sc->txdesc_tag, sc->txdesc_map, BUS_DMASYNC_POSTREAD);
	retired_buffer = false;
	while (sc->tx_idx_tail != sc->tx_idx_head) {
		desc = &sc->txdesc_ring[sc->tx_idx_tail];
		if ((desc->status & BD_STATUS_CMPLT) == 0)
			break;
		retired_buffer = true;
		bmap = &sc->txbuf_map[sc->tx_idx_tail];
		bus_dmamap_sync(sc->txbuf_tag, bmap->map,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->txbuf_tag, bmap->map);
		m_freem(bmap->mbuf);
		bmap->mbuf = NULL;
		xae_setup_txdesc(sc, sc->tx_idx_tail, 0, 0);
		sc->tx_idx_tail = next_txidx(sc, sc->tx_idx_tail);
	}

	/*
	 * If we retired any buffers, there will be open tx slots available in
	 * the descriptor ring, go try to start some new output.
	 */
	if (retired_buffer) {
		if_setdrvflagbits(sc->ifp, 0, IFF_DRV_OACTIVE);
		xae_txstart_locked(sc);
	}
}

inline static uint32_t
xae_setup_rxdesc(struct xae_softc *sc, int idx, bus_addr_t paddr)
{
	struct axidma_desc *desc;
	uint32_t nidx;

	/*
	 * The hardware requires 32-bit physical addresses.  We set up the dma
	 * tag to indicate that, so the cast to uint32_t should never lose
	 * significant bits.
	 */
	nidx = next_rxidx(sc, idx);

	desc = &sc->rxdesc_ring[idx];
	desc->next = sc->rxdesc_ring_paddr + sizeof(struct axidma_desc) * nidx;
	desc->phys = paddr;
	desc->status = 0;
	desc->control = MCLBYTES | BD_CONTROL_TXSOF | BD_CONTROL_TXEOF;

	return (nidx);
}

static struct mbuf *
xae_alloc_mbufcl(struct xae_softc *sc)
{
	struct mbuf *m;

	m = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
	if (m != NULL)
		m->m_pkthdr.len = m->m_len = m->m_ext.ext_size;

	return (m);
}

static int
xae_setup_rxbuf(struct xae_softc *sc, int idx, struct mbuf * m)
{
	int error, nsegs;
	struct bus_dma_segment seg;

	error = bus_dmamap_load_mbuf_sg(sc->rxbuf_tag, sc->rxbuf_map[idx].map,
	   m, &seg, &nsegs, 0);
	if (error != 0)
		return (error);

	bus_dmamap_sync(sc->rxbuf_tag, sc->rxbuf_map[idx].map,
	   BUS_DMASYNC_PREREAD);

	sc->rxbuf_map[idx].mbuf = m;
	xae_setup_rxdesc(sc, idx, seg.ds_addr);

	return (0);
}

static void
xae_rxfinish_onebuf(struct xae_softc *sc, int len)
{
	struct mbuf *m, *newmbuf;
	struct xae_bufmap *bmap;
	int error;

	dprintf("%s\n", __func__);

	/*
	 * First try to get a new mbuf to plug into this slot in the rx ring.
	 * If that fails, drop the current packet and recycle the current
	 * mbuf, which is still mapped and loaded.
	 */
	if ((newmbuf = xae_alloc_mbufcl(sc)) == NULL) {
		if_inc_counter(sc->ifp, IFCOUNTER_IQDROPS, 1);
		xae_setup_rxdesc(sc, sc->rx_idx,
		    sc->rxdesc_ring[sc->rx_idx].phys);
		return;
	}

	XAE_UNLOCK(sc);

	bmap = &sc->rxbuf_map[sc->rx_idx];
	bus_dmamap_sync(sc->rxbuf_tag, bmap->map, BUS_DMASYNC_POSTREAD);
	bus_dmamap_unload(sc->rxbuf_tag, bmap->map);
	m = bmap->mbuf;
	bmap->mbuf = NULL;
	m->m_len = len;
	m->m_pkthdr.len = len;
	m->m_pkthdr.rcvif = sc->ifp;

	if_input(sc->ifp, m);

	XAE_LOCK(sc);

	if ((error = xae_setup_rxbuf(sc, sc->rx_idx, newmbuf)) != 0) {
		device_printf(sc->dev, "xae_setup_rxbuf error %d\n", error);
		/* XXX Now what?  We've got a hole in the rx ring. */
	}
}

static void
xae_rxfinish_locked(struct xae_softc *sc)
{
	boolean_t desc_completed;
	struct axidma_desc *desc;
	uint32_t addr;
	int len;
	int tmp;

	dprintf("%s\n", __func__);

	XAE_ASSERT_LOCKED(sc);

	bus_dmamap_sync(sc->rxdesc_tag, sc->rxdesc_map, BUS_DMASYNC_PREREAD);
	bus_dmamap_sync(sc->rxdesc_tag, sc->rxdesc_map, BUS_DMASYNC_POSTREAD);
	desc_completed = false;
	for (;;) {
		desc = &sc->rxdesc_ring[sc->rx_idx];
		if ((desc->status & BD_STATUS_CMPLT) == 0)
			break;
		desc_completed = true;
		len = desc->status & BD_CONTROL_LEN_M;
		xae_rxfinish_onebuf(sc, len);
		tmp = sc->rx_idx;
		sc->rx_idx = next_rxidx(sc, sc->rx_idx);
	}

	if (desc_completed) {
		bus_dmamap_sync(sc->rxdesc_tag, sc->rxdesc_map,
		    BUS_DMASYNC_PREWRITE);

		addr = sc->rxdesc_ring_paddr + tmp * sizeof(struct axidma_desc);
		dprintf("%s: new tail desc %x\n", __func__, addr);
		AXIDMA_WR8(sc, AXI_TAILDESC(AXIDMA_RX_CHAN), addr);
	}
}

static void
xae_intr_rx(void *arg)
{
	struct xae_softc *sc;
	uint32_t pending;

	sc = arg;

	XAE_LOCK(sc);
	pending = AXIDMA_RD4(sc, AXI_DMASR(AXIDMA_RX_CHAN));
	dprintf("%s: pending %x\n", __func__, pending);
	AXIDMA_WR4(sc, AXI_DMASR(AXIDMA_RX_CHAN), pending);
	xae_rxfinish_locked(sc);
	XAE_UNLOCK(sc);
}

static void
xae_intr_tx(void *arg)
{
	struct xae_softc *sc;
	uint32_t pending;

	sc = arg;

	XAE_LOCK(sc);
	pending = AXIDMA_RD4(sc, AXI_DMASR(AXIDMA_TX_CHAN));
	dprintf("%s: pending %x\n", __func__, pending);
	AXIDMA_WR4(sc, AXI_DMASR(AXIDMA_TX_CHAN), pending);
	xae_txfinish_locked(sc);
	XAE_UNLOCK(sc);
}


static u_int
xae_write_maddr(void *arg, struct sockaddr_dl *sdl, u_int cnt)
{
	struct xae_softc *sc = arg;
	uint32_t reg;
	uint8_t *ma;

	if (cnt >= XAE_MULTICAST_TABLE_SIZE)
		return (1);

	ma = LLADDR(sdl);

	reg = XAE_RD4(sc, XAE_FFC) & 0xffffff00;
	reg |= cnt;
	XAE_WR4(sc, XAE_FFC, reg);

	reg = (ma[0]);
	reg |= (ma[1] << 8);
	reg |= (ma[2] << 16);
	reg |= (ma[3] << 24);
	XAE_WR4(sc, XAE_FFV(0), reg);

	reg = ma[4];
	reg |= ma[5] << 8;
	XAE_WR4(sc, XAE_FFV(1), reg);

	return (1);
}

static void
xae_setup_rxfilter(struct xae_softc *sc)
{
	if_t ifp;
	uint32_t reg;

	XAE_ASSERT_LOCKED(sc);

	ifp = sc->ifp;

	/*
	 * Set the multicast (group) filter hash.
	 */
	if ((if_getflags(ifp) & (IFF_ALLMULTI | IFF_PROMISC)) != 0) {
		reg = XAE_RD4(sc, XAE_FFC);
		reg |= FFC_PM;
		XAE_WR4(sc, XAE_FFC, reg);
	} else {
		reg = XAE_RD4(sc, XAE_FFC);
		reg &= ~FFC_PM;
		XAE_WR4(sc, XAE_FFC, reg);

		if_foreach_llmaddr(ifp, xae_write_maddr, sc);
	}

	/*
	 * Set the primary address.
	 */
	reg = sc->macaddr[0];
	reg |= (sc->macaddr[1] << 8);
	reg |= (sc->macaddr[2] << 16);
	reg |= (sc->macaddr[3] << 24);
	XAE_WR4(sc, XAE_UAW0, reg);

	reg = sc->macaddr[4];
	reg |= (sc->macaddr[5] << 8);
	XAE_WR4(sc, XAE_UAW1, reg);
}

static int
xae_get_phyaddr(phandle_t node, int *phy_addr)
{
	phandle_t phy_node;
	pcell_t phy_handle, phy_reg;

	if (OF_getencprop(node, "phy-handle", (void *)&phy_handle,
	    sizeof(phy_handle)) <= 0)
		return (ENXIO);

	phy_node = OF_node_from_xref(phy_handle);

	if (OF_getencprop(phy_node, "reg", (void *)&phy_reg,
	    sizeof(phy_reg)) <= 0)
		return (ENXIO);

	*phy_addr = phy_reg;

	return (0);
}

static void
xae_stop_locked(struct xae_softc *sc)
{
	uint32_t reg;

	XAE_ASSERT_LOCKED(sc);

	if_setdrvflagbits(sc->ifp, 0, (IFF_DRV_RUNNING | IFF_DRV_OACTIVE));

	callout_stop(&sc->xae_callout);

	/* Stop the transmitter */
	reg = XAE_RD4(sc, XAE_TC);
	reg &= ~TC_TX;
	XAE_WR4(sc, XAE_TC, reg);

	/* Stop the receiver. */
	reg = XAE_RD4(sc, XAE_RCW1);
	reg &= ~RCW1_RX;
	XAE_WR4(sc, XAE_RCW1, reg);
}

static uint64_t
xae_stat(struct xae_softc *sc, int counter_id)
{
	uint64_t new, old;
	uint64_t delta;

	KASSERT(counter_id < XAE_MAX_COUNTERS,
	    ("counter %d is out of range", counter_id));

	new = XAE_RD8(sc, XAE_STATCNT(counter_id));
	old = sc->counters[counter_id];

	if (new >= old)
		delta = new - old;
	else
		delta = UINT64_MAX - old + new;
	sc->counters[counter_id] = new;

	return (delta);
}

static void
xae_harvest_stats(struct xae_softc *sc)
{
	if_t ifp;

	ifp = sc->ifp;

	if_inc_counter(ifp, IFCOUNTER_IPACKETS, xae_stat(sc, RX_GOOD_FRAMES));
	if_inc_counter(ifp, IFCOUNTER_IMCASTS, xae_stat(sc, RX_GOOD_MCASTS));
	if_inc_counter(ifp, IFCOUNTER_IERRORS,
	    xae_stat(sc, RX_FRAME_CHECK_SEQ_ERROR) +
	    xae_stat(sc, RX_LEN_OUT_OF_RANGE) +
	    xae_stat(sc, RX_ALIGNMENT_ERRORS));

	if_inc_counter(ifp, IFCOUNTER_OBYTES, xae_stat(sc, TX_BYTES));
	if_inc_counter(ifp, IFCOUNTER_OPACKETS, xae_stat(sc, TX_GOOD_FRAMES));
	if_inc_counter(ifp, IFCOUNTER_OMCASTS, xae_stat(sc, TX_GOOD_MCASTS));
	if_inc_counter(ifp, IFCOUNTER_OERRORS,
	    xae_stat(sc, TX_GOOD_UNDERRUN_ERRORS));

	if_inc_counter(ifp, IFCOUNTER_COLLISIONS,
	    xae_stat(sc, TX_SINGLE_COLLISION_FRAMES) +
	    xae_stat(sc, TX_MULTI_COLLISION_FRAMES) +
	    xae_stat(sc, TX_LATE_COLLISIONS) +
	    xae_stat(sc, TX_EXCESS_COLLISIONS));
}

static void
xae_tick(void *arg)
{
	struct xae_softc *sc;
	if_t ifp;
	int link_was_up;

	sc = arg;

	XAE_ASSERT_LOCKED(sc);

	ifp = sc->ifp;

	if (!(if_getdrvflags(ifp) & IFF_DRV_RUNNING))
		return;

	/* Gather stats from hardware counters. */
	xae_harvest_stats(sc);

	/* Check the media status. */
	link_was_up = sc->link_is_up;
	mii_tick(sc->mii_softc);
	if (sc->link_is_up && !link_was_up)
		xae_txstart_locked(sc);

	/* Schedule another check one second from now. */
	callout_reset(&sc->xae_callout, hz, xae_tick, sc);
}

static void
xae_init_locked(struct xae_softc *sc)
{
	if_t ifp;

	XAE_ASSERT_LOCKED(sc);

	ifp = sc->ifp;
	if (if_getdrvflags(ifp) & IFF_DRV_RUNNING)
		return;

	if_setdrvflagbits(ifp, IFF_DRV_RUNNING, 0);

	xae_setup_rxfilter(sc);

	/* Enable the transmitter */
	XAE_WR4(sc, XAE_TC, TC_TX);

	/* Enable the receiver. */
	XAE_WR4(sc, XAE_RCW1, RCW1_RX);

	/*
	 * Call mii_mediachg() which will call back into xae_miibus_statchg()
	 * to set up the remaining config registers based on current media.
	 */
	mii_mediachg(sc->mii_softc);
	callout_reset(&sc->xae_callout, hz, xae_tick, sc);
}

static void
xae_init(void *arg)
{
	struct xae_softc *sc;

	sc = arg;

	XAE_LOCK(sc);
	xae_init_locked(sc);
	XAE_UNLOCK(sc);
}

static void
xae_media_status(if_t  ifp, struct ifmediareq *ifmr)
{
	struct xae_softc *sc;
	struct mii_data *mii;

	sc = if_getsoftc(ifp);
	mii = sc->mii_softc;

	XAE_LOCK(sc);
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
	XAE_UNLOCK(sc);
}

static int
xae_media_change_locked(struct xae_softc *sc)
{

	return (mii_mediachg(sc->mii_softc));
}

static int
xae_media_change(if_t  ifp)
{
	struct xae_softc *sc;
	int error;

	sc = if_getsoftc(ifp);

	XAE_LOCK(sc);
	error = xae_media_change_locked(sc);
	XAE_UNLOCK(sc);

	return (error);
}

static int
xae_ioctl(if_t ifp, u_long cmd, caddr_t data)
{
	struct xae_softc *sc;
	struct mii_data *mii;
	struct ifreq *ifr;
	int mask, error;

	sc = if_getsoftc(ifp);
	ifr = (struct ifreq *)data;

	error = 0;
	switch (cmd) {
	case SIOCSIFFLAGS:
		XAE_LOCK(sc);
		if (if_getflags(ifp) & IFF_UP) {
			if (if_getdrvflags(ifp) & IFF_DRV_RUNNING) {
				if ((if_getflags(ifp) ^ sc->if_flags) &
				    (IFF_PROMISC | IFF_ALLMULTI))
					xae_setup_rxfilter(sc);
			} else {
				if (!sc->is_detaching)
					xae_init_locked(sc);
			}
		} else {
			if (if_getdrvflags(ifp) & IFF_DRV_RUNNING)
				xae_stop_locked(sc);
		}
		sc->if_flags = if_getflags(ifp);
		XAE_UNLOCK(sc);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (if_getdrvflags(ifp) & IFF_DRV_RUNNING) {
			XAE_LOCK(sc);
			xae_setup_rxfilter(sc);
			XAE_UNLOCK(sc);
		}
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		mii = sc->mii_softc;
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, cmd);
		break;
	case SIOCSIFCAP:
		mask = if_getcapenable(ifp) ^ ifr->ifr_reqcap;
		if (mask & IFCAP_VLAN_MTU) {
			/* No work to do except acknowledge the change took */
			if_togglecapenable(ifp, IFCAP_VLAN_MTU);
		}
		break;

	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}

	return (error);
}

static void
xae_intr(void *arg)
{

}

static int
xae_get_hwaddr(struct xae_softc *sc, uint8_t *hwaddr)
{
	phandle_t node;
	int len;

	node = ofw_bus_get_node(sc->dev);

	/* Check if there is property */
	if ((len = OF_getproplen(node, "local-mac-address")) <= 0)
		return (EINVAL);

	if (len != ETHER_ADDR_LEN)
		return (EINVAL);

	OF_getprop(node, "local-mac-address", hwaddr, ETHER_ADDR_LEN);

	return (0);
}

static int
mdio_wait(struct xae_softc *sc)
{
	uint32_t reg;
	int timeout;

	timeout = 200;

	do {
		reg = XAE_RD4(sc, XAE_MDIO_CTRL);
		if (reg & MDIO_CTRL_READY)
			break;
		DELAY(1);
	} while (timeout--);

	if (timeout <= 0) {
		printf("Failed to get MDIO ready\n");
		return (1);
	}

	return (0);
}

static int
xae_miibus_read_reg(device_t dev, int phy, int reg)
{
	struct xae_softc *sc;
	uint32_t mii;
	int rv;

	sc = device_get_softc(dev);

	if (mdio_wait(sc))
		return (0);

	mii = MDIO_CTRL_TX_OP_READ | MDIO_CTRL_INITIATE;
	mii |= (reg << MDIO_TX_REGAD_S);
	mii |= (phy << MDIO_TX_PHYAD_S);

	XAE_WR4(sc, XAE_MDIO_CTRL, mii);

	if (mdio_wait(sc))
		return (0);

	rv = XAE_RD4(sc, XAE_MDIO_READ);

	return (rv);
}

static int
xae_miibus_write_reg(device_t dev, int phy, int reg, int val)
{
	struct xae_softc *sc;
	uint32_t mii;

	sc = device_get_softc(dev);

	if (mdio_wait(sc))
		return (1);

	mii = MDIO_CTRL_TX_OP_WRITE | MDIO_CTRL_INITIATE;
	mii |= (reg << MDIO_TX_REGAD_S);
	mii |= (phy << MDIO_TX_PHYAD_S);

	XAE_WR4(sc, XAE_MDIO_WRITE, val);
	XAE_WR4(sc, XAE_MDIO_CTRL, mii);

	if (mdio_wait(sc))
		return (1);

	return (0);
}

static void
xae_phy_fixup(struct xae_softc *sc)
{
	uint32_t reg;

	do {
		WRITE_TI_EREG(sc, DP83867_SGMIICTL1, SGMIICTL1_SGMII_6W);
		PHY_WR(sc, DP83867_PHYCR, PHYCR_SGMII_EN);

		reg = PHY_RD(sc, DP83867_CFG2);
		reg &= ~CFG2_SPEED_OPT_ATTEMPT_CNT_M;
		reg |= (CFG2_SPEED_OPT_ATTEMPT_CNT_4);
		reg |= CFG2_INTERRUPT_POLARITY;
		reg |= CFG2_SPEED_OPT_ENHANCED_EN;
		reg |= CFG2_SPEED_OPT_10M_EN;
		PHY_WR(sc, DP83867_CFG2, reg);

		WRITE_TI_EREG(sc, DP83867_CFG4, CFG4_SGMII_TMR);
		PHY_WR(sc, MII_BMCR,
		    BMCR_AUTOEN | BMCR_FDX | BMCR_SPEED1 | BMCR_RESET);
	} while (PHY1_RD(sc, MII_BMCR) == 0x0ffff);

	do {
		PHY1_WR(sc, MII_BMCR,
		    BMCR_AUTOEN | BMCR_FDX | BMCR_SPEED1 | BMCR_STARTNEG);
		DELAY(40000);
	} while ((PHY1_RD(sc, MII_BMSR) & BMSR_ACOMP) == 0);
}

static int
get_axistream(struct xae_softc *sc)
{
	phandle_t node;
	pcell_t prop;
	size_t len;

	node = ofw_bus_get_node(sc->dev);
	len = OF_getencprop(node, "axistream-connected", &prop, sizeof(prop));
	if (len != sizeof(prop)) {
		device_printf(sc->dev,
		    "%s: Couldn't get axistream-connected prop.\n", __func__);
		return (ENXIO);
	}
	sc->dma_dev = OF_device_from_xref(prop);
	if (sc->dma_dev == NULL) {
		device_printf(sc->dev, "Could not get DMA device by xref.\n");
		return (ENXIO);
	}
	sc->dma_res = AXIDMA_MEMRES(sc->dma_dev);

	return (0);
}

static void
xae_txstart(if_t ifp)
{
	struct xae_softc *sc;

	sc = if_getsoftc(ifp);

	dprintf("%s\n", __func__);

	XAE_LOCK(sc);
	xae_txstart_locked(sc);
	XAE_UNLOCK(sc);
}

static int
xae_setup_dma(struct xae_softc *sc)
{
	struct axidma_desc *desc;
	uint32_t addr;
	uint32_t reg;
	struct mbuf *m;
	int error;
	int idx;

	sc->rxbuf_align = PAGE_SIZE;
	sc->txbuf_align = PAGE_SIZE;

	/*
	 * Set up TX descriptor ring, descriptors, and dma maps.
	 */
	error = bus_dma_tag_create(
	    bus_get_dma_tag(sc->dev),	/* Parent tag. */
	    AXI_DESC_RING_ALIGN, 0,	/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    TX_DESC_SIZE, 1, 		/* maxsize, nsegments */
	    TX_DESC_SIZE,		/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->txdesc_tag);
	if (error != 0) {
		device_printf(sc->dev, "could not create TX ring DMA tag.\n");
		goto out;
	}

	error = bus_dmamem_alloc(sc->txdesc_tag, (void**)&sc->txdesc_ring,
	    BUS_DMA_COHERENT | BUS_DMA_WAITOK | BUS_DMA_ZERO, &sc->txdesc_map);
	if (error != 0) {
		device_printf(sc->dev,
		    "could not allocate TX descriptor ring.\n");
		goto out;
	}

	error = bus_dmamap_load(sc->txdesc_tag, sc->txdesc_map, sc->txdesc_ring,
	    TX_DESC_SIZE, xae_get1paddr, &sc->txdesc_ring_paddr, 0);
	if (error != 0) {
		device_printf(sc->dev,
		    "could not load TX descriptor ring map.\n");
		goto out;
	}

	error = bus_dma_tag_create(
	    bus_get_dma_tag(sc->dev),	/* Parent tag. */
	    sc->txbuf_align, 0,		/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    MCLBYTES, 1, 		/* maxsize, nsegments */
	    MCLBYTES,			/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->txbuf_tag);
	if (error != 0) {
		device_printf(sc->dev,
		    "could not create TX ring DMA tag.\n");
		goto out;
	}

	for (idx = 0; idx < TX_DESC_COUNT; ++idx) {
		desc = &sc->txdesc_ring[idx];
		bzero(desc, sizeof(struct axidma_desc));
	}

	for (idx = 0; idx < TX_DESC_COUNT; ++idx) {
		error = bus_dmamap_create(sc->txbuf_tag, 0,
		   &sc->txbuf_map[idx].map);
		if (error != 0) {
			device_printf(sc->dev,
			   "could not create TX buffer DMA map.\n");
			goto out;
		}
		xae_setup_txdesc(sc, idx, 0, 0);
	}

	/*
	* Set up RX descriptor ring, descriptors, dma maps, and mbufs.
	*/
	error = bus_dma_tag_create(
	    bus_get_dma_tag(sc->dev),	/* Parent tag. */
	    AXI_DESC_RING_ALIGN, 0,	/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    RX_DESC_SIZE, 1, 		/* maxsize, nsegments */
	    RX_DESC_SIZE,		/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->rxdesc_tag);
	if (error != 0) {
		device_printf(sc->dev, "could not create RX ring DMA tag.\n");
		goto out;
	}

	error = bus_dmamem_alloc(sc->rxdesc_tag, (void **)&sc->rxdesc_ring, 
	    BUS_DMA_COHERENT | BUS_DMA_WAITOK | BUS_DMA_ZERO, &sc->rxdesc_map);
	if (error != 0) {
		device_printf(sc->dev,
		    "could not allocate RX descriptor ring.\n");
		goto out;
	}

	error = bus_dmamap_load(sc->rxdesc_tag, sc->rxdesc_map, sc->rxdesc_ring,
	    RX_DESC_SIZE, xae_get1paddr, &sc->rxdesc_ring_paddr, 0);
	if (error != 0) {
		device_printf(sc->dev,
		   "could not load RX descriptor ring map.\n");
		goto out;
	}

	error = bus_dma_tag_create(
	    bus_get_dma_tag(sc->dev),	/* Parent tag. */
	    1, 0,			/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    MCLBYTES, 1, 		/* maxsize, nsegments */
	    MCLBYTES,			/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->rxbuf_tag);
	if (error != 0) {
		device_printf(sc->dev, "could not create RX buf DMA tag.\n");
		goto out;
	}

	for (idx = 0; idx < RX_DESC_COUNT; ++idx) {
		desc = &sc->rxdesc_ring[idx];
		bzero(desc, sizeof(struct axidma_desc));
	}

	for (idx = 0; idx < RX_DESC_COUNT; ++idx) {
		error = bus_dmamap_create(sc->rxbuf_tag, 0,
		    &sc->rxbuf_map[idx].map);
		if (error != 0) {
			device_printf(sc->dev,
			    "could not create RX buffer DMA map.\n");
			goto out;
		}
		if ((m = xae_alloc_mbufcl(sc)) == NULL) {
			device_printf(sc->dev, "Could not alloc mbuf\n");
			error = ENOMEM;
			goto out;
		}
		if ((error = xae_setup_rxbuf(sc, idx, m)) != 0) {
			device_printf(sc->dev,
			    "could not create new RX buffer.\n");
			goto out;
		}
	}

	if (AXIDMA_RESET(sc->dma_dev, AXIDMA_TX_CHAN) != 0) {
		device_printf(sc->dev, "Could not reset TX channel.\n");
		goto out;
	}
	if (AXIDMA_RESET(sc->dma_dev, AXIDMA_RX_CHAN) != 0) {
		device_printf(sc->dev, "Could not reset TX channel.\n");
		goto out;
	}
	if (AXIDMA_SETUP_CB(sc->dma_dev, AXIDMA_TX_CHAN, xae_intr_tx, sc)) {
		device_printf(sc->dev, "Could not setup TX intr callback.\n");
		goto out;
	}
	if (AXIDMA_SETUP_CB(sc->dma_dev, AXIDMA_RX_CHAN, xae_intr_rx, sc)) {
		device_printf(sc->dev, "Could not setup RX intr callback.\n");
		goto out;
	}

	dprintf("%s: tx desc base %lx\n", __func__, sc->txdesc_ring_paddr);
	AXIDMA_WR8(sc, AXI_CURDESC(AXIDMA_TX_CHAN), sc->txdesc_ring_paddr);
	reg = AXIDMA_RD4(sc, AXI_DMACR(AXIDMA_TX_CHAN));
	reg |= DMACR_IOC_IRQEN | DMACR_DLY_IRQEN | DMACR_ERR_IRQEN;
	AXIDMA_WR4(sc, AXI_DMACR(AXIDMA_TX_CHAN), reg);
	reg |= DMACR_RS;
	AXIDMA_WR4(sc, AXI_DMACR(AXIDMA_TX_CHAN), reg);

	AXIDMA_WR8(sc, AXI_CURDESC(AXIDMA_RX_CHAN), sc->rxdesc_ring_paddr);
	reg = AXIDMA_RD4(sc, AXI_DMACR(AXIDMA_RX_CHAN));
	reg |= DMACR_IOC_IRQEN | DMACR_DLY_IRQEN | DMACR_ERR_IRQEN;
	AXIDMA_WR4(sc, AXI_DMACR(AXIDMA_RX_CHAN), reg);
	reg |= DMACR_RS;
	AXIDMA_WR4(sc, AXI_DMACR(AXIDMA_RX_CHAN), reg);

	addr = sc->rxdesc_ring_paddr +
	    (RX_DESC_COUNT - 1) * sizeof(struct axidma_desc);
	dprintf("%s: new RX tail desc %x\n", __func__, addr);
	AXIDMA_WR8(sc, AXI_TAILDESC(AXIDMA_RX_CHAN), addr);

	return (0);

out:
	/* TODO: release resources. */
	return (-1);
}


static int
xae_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "xlnx,axi-ethernet-1.00.a"))
		return (ENXIO);

	device_set_desc(dev, "Xilinx AXI Ethernet");

	return (BUS_PROBE_DEFAULT);
}

static int
xae_attach(device_t dev)
{
	struct xae_softc *sc;
	if_t ifp;
	phandle_t node;
	uint32_t reg;
	int error;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->rx_idx = 0;
	sc->tx_idx_head = sc->tx_idx_tail = 0;
	sc->txcount = 0;

	mtx_init(&sc->mtx, device_get_nameunit(sc->dev), MTX_NETWORK_LOCK,
	    MTX_DEF);

	node = ofw_bus_get_node(dev);

	/* Get out MAC addr. */
	if (xae_get_hwaddr(sc, sc->macaddr)) {
		device_printf(sc->dev, "can't get mac\n");
		return (ENXIO);
	}

	/* DMA */
	error = get_axistream(sc);
	if (error != 0)
		return (error);

	if (bus_alloc_resources(dev, xae_spec, sc->res)) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	/* Setup interrupt handler. */
	error = bus_setup_intr(dev, sc->res[1], INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, xae_intr, sc, &sc->intr_cookie);
	if (error != 0) {
		device_printf(dev, "could not setup interrupt handler.\n");
		return (ENXIO);
	}

	/* Memory interface */
	sc->bst = rman_get_bustag(sc->res[0]);
	sc->bsh = rman_get_bushandle(sc->res[0]);

	device_printf(sc->dev, "Identification: %x\n", XAE_RD4(sc, XAE_IDENT));

	error = xae_setup_dma(sc);
	if (error != 0)
		return (error);

	/* Enable MII clock */
	reg = (MDIO_CLK_DIV_DEFAULT << MDIO_SETUP_CLK_DIV_S);
	reg |= MDIO_SETUP_ENABLE;
	XAE_WR4(sc, XAE_MDIO_SETUP, reg);
	if (mdio_wait(sc))
		return (ENXIO);

	callout_init_mtx(&sc->xae_callout, &sc->mtx, 0);

	/* Set up the ethernet interface. */
	sc->ifp = ifp = if_alloc(IFT_ETHER);
	if_setsoftc(ifp, sc);
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	if_setflags(ifp, IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST);
	if_setcapabilities(ifp, IFCAP_VLAN_MTU);
	if_setcapenable(ifp, if_getcapabilities(ifp));
	if_setioctlfn(ifp, xae_ioctl);
	if_setstartfn(ifp, xae_txstart);
	if_setinitfn(ifp, xae_init);
	if_setsendqlen(ifp, TX_DESC_COUNT - 1);
	if_setsendqready(ifp);

	if (xae_get_phyaddr(node, &sc->phy_addr) != 0)
		return (ENXIO);

	/* Attach the mii driver. */
	error = mii_attach(dev, &sc->miibus, ifp, xae_media_change,
	    xae_media_status, BMSR_DEFCAPMASK, sc->phy_addr,
	    MII_OFFSET_ANY, 0);
	if (error != 0) {
		device_printf(dev, "PHY attach failed\n");
		return (ENXIO);
	}

	sc->mii_softc = device_get_softc(sc->miibus);

	/* Apply vcu118 workaround. */
	if (OF_getproplen(node, "xlnx,vcu118") >= 0)
		xae_phy_fixup(sc);

	/* All ready to run, attach the ethernet interface. */
	ether_ifattach(ifp, sc->macaddr);
	sc->is_attached = true;

	return (0);
}

static int
xae_detach(device_t dev)
{
	struct xae_softc *sc;
	if_t ifp;

	sc = device_get_softc(dev);

	KASSERT(mtx_initialized(&sc->mtx), ("%s: mutex not initialized",
	    device_get_nameunit(dev)));

	ifp = sc->ifp;

	/* Only cleanup if attach succeeded. */
	if (device_is_attached(dev)) {
		XAE_LOCK(sc);
		xae_stop_locked(sc);
		XAE_UNLOCK(sc);
		callout_drain(&sc->xae_callout);
		ether_ifdetach(ifp);
	}

	bus_generic_detach(dev);

	if (ifp != NULL)
		if_free(ifp);

	mtx_destroy(&sc->mtx);

	bus_teardown_intr(dev, sc->res[1], sc->intr_cookie);

	bus_release_resources(dev, xae_spec, sc->res);

	return (0);
}

static void
xae_miibus_statchg(device_t dev)
{
	struct xae_softc *sc;
	struct mii_data *mii;
	uint32_t reg;

	/*
	 * Called by the MII bus driver when the PHY establishes
	 * link to set the MAC interface registers.
	 */

	sc = device_get_softc(dev);

	XAE_ASSERT_LOCKED(sc);

	mii = sc->mii_softc;

	if (mii->mii_media_status & IFM_ACTIVE)
		sc->link_is_up = true;
	else
		sc->link_is_up = false;

	switch (IFM_SUBTYPE(mii->mii_media_active)) {
	case IFM_1000_T:
	case IFM_1000_SX:
		reg = SPEED_1000;
		break;
	case IFM_100_TX:
		reg = SPEED_100;
		break;
	case IFM_10_T:
		reg = SPEED_10;
		break;
	case IFM_NONE:
		sc->link_is_up = false;
		return;
	default:
		sc->link_is_up = false;
		device_printf(dev, "Unsupported media %u\n",
		    IFM_SUBTYPE(mii->mii_media_active));
		return;
	}

	XAE_WR4(sc, XAE_SPEED, reg);
}

static device_method_t xae_methods[] = {
	DEVMETHOD(device_probe,		xae_probe),
	DEVMETHOD(device_attach,	xae_attach),
	DEVMETHOD(device_detach,	xae_detach),

	/* MII Interface */
	DEVMETHOD(miibus_readreg,	xae_miibus_read_reg),
	DEVMETHOD(miibus_writereg,	xae_miibus_write_reg),
	DEVMETHOD(miibus_statchg,	xae_miibus_statchg),
	{ 0, 0 }
};

driver_t xae_driver = {
	"xae",
	xae_methods,
	sizeof(struct xae_softc),
};

DRIVER_MODULE(xae, simplebus, xae_driver, 0, 0);
DRIVER_MODULE(miibus, xae, miibus_driver, 0, 0);

MODULE_DEPEND(xae, axidma, 1, 1, 1);
MODULE_DEPEND(xae, ether, 1, 1, 1);
MODULE_DEPEND(xae, miibus, 1, 1, 1);
