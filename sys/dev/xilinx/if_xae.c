/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Ruslan Bukin <br@bsdpad.com>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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
#include <dev/xilinx/if_xaereg.h>
#include <dev/xilinx/if_xaevar.h>

#include <dev/xilinx/axidma.h>

#include "miibus_if.h"

#define	READ4(_sc, _reg) \
	bus_read_4((_sc)->res[0], _reg)
#define	WRITE4(_sc, _reg, _val) \
	bus_write_4((_sc)->res[0], _reg, _val)

#define	READ8(_sc, _reg) \
	bus_read_8((_sc)->res[0], _reg)
#define	WRITE8(_sc, _reg, _val) \
	bus_write_8((_sc)->res[0], _reg, _val)

#define	XAE_LOCK(sc)			mtx_lock(&(sc)->mtx)
#define	XAE_UNLOCK(sc)			mtx_unlock(&(sc)->mtx)
#define	XAE_ASSERT_LOCKED(sc)		mtx_assert(&(sc)->mtx, MA_OWNED)
#define	XAE_ASSERT_UNLOCKED(sc)		mtx_assert(&(sc)->mtx, MA_NOTOWNED)

#define XAE_DEBUG
#undef XAE_DEBUG

#ifdef XAE_DEBUG
#define dprintf(fmt, ...)  printf(fmt, ##__VA_ARGS__)
#else
#define dprintf(fmt, ...)
#endif

#define	RX_QUEUE_SIZE		64
#define	TX_QUEUE_SIZE		64
#define	NUM_RX_MBUF		16
#define	BUFRING_SIZE		8192
#define	MDIO_CLK_DIV_DEFAULT	29

#define	PHY1_RD(sc, _r)		\
	xae_miibus_read_reg(sc->dev, 1, _r)
#define	PHY1_WR(sc, _r, _v)	\
	xae_miibus_write_reg(sc->dev, 1, _r, _v)

#define	PHY_RD(sc, _r)		\
	xae_miibus_read_reg(sc->dev, sc->phy_addr, _r)
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

static struct resource_spec xae_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ -1, 0 }
};

static void xae_stop_locked(struct xae_softc *sc);
static void xae_setup_rxfilter(struct xae_softc *sc);

static int
xae_rx_enqueue(struct xae_softc *sc, uint32_t n)
{
	struct mbuf *m;
	int i;

	for (i = 0; i < n; i++) {
		m = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
		if (m == NULL) {
			device_printf(sc->dev,
			    "%s: Can't alloc rx mbuf\n", __func__);
			return (-1);
		}

		m->m_pkthdr.len = m->m_len = m->m_ext.ext_size;
		xdma_enqueue_mbuf(sc->xchan_rx, &m, 0, 4, 4, XDMA_DEV_TO_MEM);
	}

	return (0);
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

static int
xae_xdma_tx_intr(void *arg, xdma_transfer_status_t *status)
{
	xdma_transfer_status_t st;
	struct xae_softc *sc;
	struct ifnet *ifp;
	struct mbuf *m;
	int err;

	sc = arg;

	XAE_LOCK(sc);

	ifp = sc->ifp;

	for (;;) {
		err = xdma_dequeue_mbuf(sc->xchan_tx, &m, &st);
		if (err != 0) {
			break;
		}

		if (st.error != 0) {
			if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
		}

		m_freem(m);
	}

	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	XAE_UNLOCK(sc);

	return (0);
}

static int
xae_xdma_rx_intr(void *arg, xdma_transfer_status_t *status)
{
	xdma_transfer_status_t st;
	struct xae_softc *sc;
	struct ifnet *ifp;
	struct mbuf *m;
	int err;
	uint32_t cnt_processed;

	sc = arg;

	dprintf("%s\n", __func__);

	XAE_LOCK(sc);

	ifp = sc->ifp;

	cnt_processed = 0;
	for (;;) {
		err = xdma_dequeue_mbuf(sc->xchan_rx, &m, &st);
		if (err != 0) {
			break;
		}
		cnt_processed++;

		if (st.error != 0) {
			if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);
			m_freem(m);
			continue;
		}

		m->m_pkthdr.len = m->m_len = st.transferred;
		m->m_pkthdr.rcvif = ifp;
		XAE_UNLOCK(sc);
		(*ifp->if_input)(ifp, m);
		XAE_LOCK(sc);
	}

	xae_rx_enqueue(sc, cnt_processed);

	XAE_UNLOCK(sc);

	return (0);
}

static void
xae_qflush(struct ifnet *ifp)
{
	struct xae_softc *sc;

	sc = ifp->if_softc;
}

static int
xae_transmit_locked(struct ifnet *ifp)
{
	struct xae_softc *sc;
	struct mbuf *m;
	struct buf_ring *br;
	int error;
	int enq;

	dprintf("%s\n", __func__);

	sc = ifp->if_softc;
	br = sc->br;

	enq = 0;

	while ((m = drbr_peek(ifp, br)) != NULL) {
		error = xdma_enqueue_mbuf(sc->xchan_tx,
		    &m, 0, 4, 4, XDMA_MEM_TO_DEV);
		if (error != 0) {
			/* No space in request queue available yet. */
			drbr_putback(ifp, br, m);
			break;
		}

		drbr_advance(ifp, br);

		enq++;

		/* If anyone is interested give them a copy. */
		ETHER_BPF_MTAP(ifp, m);
        }

	if (enq > 0)
		xdma_queue_submit(sc->xchan_tx);

	return (0);
}

static int
xae_transmit(struct ifnet *ifp, struct mbuf *m)
{
	struct xae_softc *sc;
	int error;

	dprintf("%s\n", __func__);

	sc = ifp->if_softc;

	XAE_LOCK(sc);

	error = drbr_enqueue(ifp, sc->br, m);
	if (error) {
		XAE_UNLOCK(sc);
		return (error);
	}

	if ((ifp->if_drv_flags & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING) {
		XAE_UNLOCK(sc);
		return (0);
	}

	if (!sc->link_is_up) {
		XAE_UNLOCK(sc);
		return (0);
	}

	error = xae_transmit_locked(ifp);

	XAE_UNLOCK(sc);

	return (error);
}

static void
xae_stop_locked(struct xae_softc *sc)
{
	struct ifnet *ifp;
	uint32_t reg;

	XAE_ASSERT_LOCKED(sc);

	ifp = sc->ifp;
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	callout_stop(&sc->xae_callout);

	/* Stop the transmitter */
	reg = READ4(sc, XAE_TC);
	reg &= ~TC_TX;
	WRITE4(sc, XAE_TC, reg);

	/* Stop the receiver. */
	reg = READ4(sc, XAE_RCW1);
	reg &= ~RCW1_RX;
	WRITE4(sc, XAE_RCW1, reg);
}

static uint64_t
xae_stat(struct xae_softc *sc, int counter_id)
{
	uint64_t new, old;
	uint64_t delta;

	KASSERT(counter_id < XAE_MAX_COUNTERS,
		("counter %d is out of range", counter_id));

	new = READ8(sc, XAE_STATCNT(counter_id));
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
	struct ifnet *ifp;

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
	struct ifnet *ifp;
	int link_was_up;

	sc = arg;

	XAE_ASSERT_LOCKED(sc);

	ifp = sc->ifp;

	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
		return;

	/* Gather stats from hardware counters. */
	xae_harvest_stats(sc);

	/* Check the media status. */
	link_was_up = sc->link_is_up;
	mii_tick(sc->mii_softc);
	if (sc->link_is_up && !link_was_up)
		xae_transmit_locked(sc->ifp);

	/* Schedule another check one second from now. */
	callout_reset(&sc->xae_callout, hz, xae_tick, sc);
}

static void
xae_init_locked(struct xae_softc *sc)
{
	struct ifnet *ifp;

	XAE_ASSERT_LOCKED(sc);

	ifp = sc->ifp;
	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		return;

	ifp->if_drv_flags |= IFF_DRV_RUNNING;

	xae_setup_rxfilter(sc);

	/* Enable the transmitter */
	WRITE4(sc, XAE_TC, TC_TX);

	/* Enable the receiver. */
	WRITE4(sc, XAE_RCW1, RCW1_RX);

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
xae_media_status(struct ifnet * ifp, struct ifmediareq *ifmr)
{
	struct xae_softc *sc;
	struct mii_data *mii;

	sc = ifp->if_softc;
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
xae_media_change(struct ifnet * ifp)
{
	struct xae_softc *sc;
	int error;

	sc = ifp->if_softc;

	XAE_LOCK(sc);
	error = xae_media_change_locked(sc);
	XAE_UNLOCK(sc);

	return (error);
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

	reg = READ4(sc, XAE_FFC) & 0xffffff00;
	reg |= cnt;
	WRITE4(sc, XAE_FFC, reg);

	reg = (ma[0]);
	reg |= (ma[1] << 8);
	reg |= (ma[2] << 16);
	reg |= (ma[3] << 24);
	WRITE4(sc, XAE_FFV(0), reg);

	reg = ma[4];
	reg |= ma[5] << 8;
	WRITE4(sc, XAE_FFV(1), reg);

	return (1);
}

static void
xae_setup_rxfilter(struct xae_softc *sc)
{
	struct ifnet *ifp;
	uint32_t reg;

	XAE_ASSERT_LOCKED(sc);

	ifp = sc->ifp;

	/*
	 * Set the multicast (group) filter hash.
	 */
	if ((ifp->if_flags & (IFF_ALLMULTI | IFF_PROMISC)) != 0) {
		reg = READ4(sc, XAE_FFC);
		reg |= FFC_PM;
		WRITE4(sc, XAE_FFC, reg);
	} else {
		reg = READ4(sc, XAE_FFC);
		reg &= ~FFC_PM;
		WRITE4(sc, XAE_FFC, reg);

		if_foreach_llmaddr(ifp, xae_write_maddr, sc);
	}

	/*
	 * Set the primary address.
	 */
	reg = sc->macaddr[0];
	reg |= (sc->macaddr[1] << 8);
	reg |= (sc->macaddr[2] << 16);
	reg |= (sc->macaddr[3] << 24);
	WRITE4(sc, XAE_UAW0, reg);

	reg = sc->macaddr[4];
	reg |= (sc->macaddr[5] << 8);
	WRITE4(sc, XAE_UAW1, reg);
}

static int
xae_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct xae_softc *sc;
	struct mii_data *mii;
	struct ifreq *ifr;
	int mask, error;

	sc = ifp->if_softc;
	ifr = (struct ifreq *)data;

	error = 0;
	switch (cmd) {
	case SIOCSIFFLAGS:
		XAE_LOCK(sc);
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				if ((ifp->if_flags ^ sc->if_flags) &
				    (IFF_PROMISC | IFF_ALLMULTI))
					xae_setup_rxfilter(sc);
			} else {
				if (!sc->is_detaching)
					xae_init_locked(sc);
			}
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				xae_stop_locked(sc);
		}
		sc->if_flags = ifp->if_flags;
		XAE_UNLOCK(sc);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
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
		mask = ifp->if_capenable ^ ifr->ifr_reqcap;
		if (mask & IFCAP_VLAN_MTU) {
			/* No work to do except acknowledge the change took */
			ifp->if_capenable ^= IFCAP_VLAN_MTU;
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

	OF_getprop(node, "local-mac-address", hwaddr,
	    ETHER_ADDR_LEN);

	return (0);
}

static int
mdio_wait(struct xae_softc *sc)
{
	uint32_t reg;
	int timeout;

	timeout = 200;

	do {
		reg = READ4(sc, XAE_MDIO_CTRL);
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

	WRITE4(sc, XAE_MDIO_CTRL, mii);

	if (mdio_wait(sc))
		return (0);

	rv = READ4(sc, XAE_MDIO_READ);

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

	WRITE4(sc, XAE_MDIO_WRITE, val);
	WRITE4(sc, XAE_MDIO_CTRL, mii);

	if (mdio_wait(sc))
		return (1);

	return (0);
}

static void
xae_phy_fixup(struct xae_softc *sc)
{
	uint32_t reg;
	device_t dev;

	dev = sc->dev;

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
get_xdma_std(struct xae_softc *sc)
{

	sc->xdma_tx = xdma_ofw_get(sc->dev, "tx");
	if (sc->xdma_tx == NULL)
		return (ENXIO);

	sc->xdma_rx = xdma_ofw_get(sc->dev, "rx");
	if (sc->xdma_rx == NULL) {
		xdma_put(sc->xdma_tx);
		return (ENXIO);
	}

	return (0);
}

static int
get_xdma_axistream(struct xae_softc *sc)
{
	struct axidma_fdt_data *data;
	device_t dma_dev;
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
	dma_dev = OF_device_from_xref(prop);
	if (dma_dev == NULL) {
		device_printf(sc->dev, "Could not get DMA device by xref.\n");
		return (ENXIO);
	}

	sc->xdma_tx = xdma_get(sc->dev, dma_dev);
	if (sc->xdma_tx == NULL) {
		device_printf(sc->dev, "Could not find DMA controller.\n");
		return (ENXIO);
	}
	data = malloc(sizeof(struct axidma_fdt_data),
	    M_DEVBUF, (M_WAITOK | M_ZERO));
	data->id = AXIDMA_TX_CHAN;
	sc->xdma_tx->data = data;

	sc->xdma_rx = xdma_get(sc->dev, dma_dev);
	if (sc->xdma_rx == NULL) {
		device_printf(sc->dev, "Could not find DMA controller.\n");
		return (ENXIO);
	}
	data = malloc(sizeof(struct axidma_fdt_data),
	    M_DEVBUF, (M_WAITOK | M_ZERO));
	data->id = AXIDMA_RX_CHAN;
	sc->xdma_rx->data = data;

	return (0);
}

static int
setup_xdma(struct xae_softc *sc)
{
	device_t dev;
	vmem_t *vmem;
	int error;

	dev = sc->dev;

	/* Get xDMA controller */   
	error = get_xdma_std(sc);

	if (error) {
		device_printf(sc->dev,
		    "Fallback to axistream-connected property\n");
		error = get_xdma_axistream(sc);
	}

	if (error) {
		device_printf(dev, "Could not find xDMA controllers.\n");
		return (ENXIO);
	}

	/* Alloc xDMA TX virtual channel. */
	sc->xchan_tx = xdma_channel_alloc(sc->xdma_tx, 0);
	if (sc->xchan_tx == NULL) {
		device_printf(dev, "Can't alloc virtual DMA TX channel.\n");
		return (ENXIO);
	}

	/* Setup interrupt handler. */
	error = xdma_setup_intr(sc->xchan_tx, 0,
	    xae_xdma_tx_intr, sc, &sc->ih_tx);
	if (error) {
		device_printf(sc->dev,
		    "Can't setup xDMA TX interrupt handler.\n");
		return (ENXIO);
	}

	/* Alloc xDMA RX virtual channel. */
	sc->xchan_rx = xdma_channel_alloc(sc->xdma_rx, 0);
	if (sc->xchan_rx == NULL) {
		device_printf(dev, "Can't alloc virtual DMA RX channel.\n");
		return (ENXIO);
	}

	/* Setup interrupt handler. */
	error = xdma_setup_intr(sc->xchan_rx, XDMA_INTR_NET,
	    xae_xdma_rx_intr, sc, &sc->ih_rx);
	if (error) {
		device_printf(sc->dev,
		    "Can't setup xDMA RX interrupt handler.\n");
		return (ENXIO);
	}

	/* Setup bounce buffer */
	vmem = xdma_get_memory(dev);
	if (vmem) {
		xchan_set_memory(sc->xchan_tx, vmem);
		xchan_set_memory(sc->xchan_rx, vmem);
	}

	xdma_prep_sg(sc->xchan_tx,
	    TX_QUEUE_SIZE,	/* xchan requests queue size */
	    MCLBYTES,	/* maxsegsize */
	    8,		/* maxnsegs */
	    16,		/* alignment */
	    0,		/* boundary */
	    BUS_SPACE_MAXADDR_32BIT,
	    BUS_SPACE_MAXADDR);

	xdma_prep_sg(sc->xchan_rx,
	    RX_QUEUE_SIZE,	/* xchan requests queue size */
	    MCLBYTES,	/* maxsegsize */
	    1,		/* maxnsegs */
	    16,		/* alignment */
	    0,		/* boundary */
	    BUS_SPACE_MAXADDR_32BIT,
	    BUS_SPACE_MAXADDR);

	return (0);
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
	struct ifnet *ifp;
	phandle_t node;
	uint32_t reg;
	int error;

	sc = device_get_softc(dev);
	sc->dev = dev;
	node = ofw_bus_get_node(dev);

	if (setup_xdma(sc) != 0) {
		device_printf(dev, "Could not setup xDMA.\n");
		return (ENXIO);
	}

	mtx_init(&sc->mtx, device_get_nameunit(sc->dev),
	    MTX_NETWORK_LOCK, MTX_DEF);

	sc->br = buf_ring_alloc(BUFRING_SIZE, M_DEVBUF,
	    M_NOWAIT, &sc->mtx);
	if (sc->br == NULL)
		return (ENOMEM);

	if (bus_alloc_resources(dev, xae_spec, sc->res)) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	/* Memory interface */
	sc->bst = rman_get_bustag(sc->res[0]);
	sc->bsh = rman_get_bushandle(sc->res[0]);

	device_printf(sc->dev, "Identification: %x\n",
	    READ4(sc, XAE_IDENT));

	/* Get MAC addr */
	if (xae_get_hwaddr(sc, sc->macaddr)) {
		device_printf(sc->dev, "can't get mac\n");
		return (ENXIO);
	}

	/* Enable MII clock */
	reg = (MDIO_CLK_DIV_DEFAULT << MDIO_SETUP_CLK_DIV_S);
	reg |= MDIO_SETUP_ENABLE;
	WRITE4(sc, XAE_MDIO_SETUP, reg);
	if (mdio_wait(sc))
		return (ENXIO);

	callout_init_mtx(&sc->xae_callout, &sc->mtx, 0);

	/* Setup interrupt handler. */
	error = bus_setup_intr(dev, sc->res[1], INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, xae_intr, sc, &sc->intr_cookie);
	if (error != 0) {
		device_printf(dev, "could not setup interrupt handler.\n");
		return (ENXIO);
	}

	/* Set up the ethernet interface. */
	sc->ifp = ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "could not allocate ifp.\n");
		return (ENXIO);
	}

	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_capabilities = IFCAP_VLAN_MTU;
	ifp->if_capenable = ifp->if_capabilities;
	ifp->if_transmit = xae_transmit;
	ifp->if_qflush = xae_qflush;
	ifp->if_ioctl = xae_ioctl;
	ifp->if_init = xae_init;
	IFQ_SET_MAXLEN(&ifp->if_snd, TX_DESC_COUNT - 1);
	ifp->if_snd.ifq_drv_maxlen = TX_DESC_COUNT - 1;
	IFQ_SET_READY(&ifp->if_snd);

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

	xae_rx_enqueue(sc, NUM_RX_MBUF);
	xdma_queue_submit(sc->xchan_rx);

	return (0);
}

static int
xae_detach(device_t dev)
{
	struct xae_softc *sc;
	struct ifnet *ifp;

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

	if (sc->miibus != NULL)
		device_delete_child(dev, sc->miibus);

	if (ifp != NULL)
		if_free(ifp);

	mtx_destroy(&sc->mtx);

	bus_teardown_intr(dev, sc->res[1], sc->intr_cookie);

	bus_release_resources(dev, xae_spec, sc->res);

	xdma_channel_free(sc->xchan_tx);
	xdma_channel_free(sc->xchan_rx);
	xdma_put(sc->xdma_tx);
	xdma_put(sc->xdma_rx);

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

	WRITE4(sc, XAE_SPEED, reg);
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

static devclass_t xae_devclass;

DRIVER_MODULE(xae, simplebus, xae_driver, xae_devclass, 0, 0);
DRIVER_MODULE(miibus, xae, miibus_driver, miibus_devclass, 0, 0);

MODULE_DEPEND(xae, ether, 1, 1, 1);
MODULE_DEPEND(xae, miibus, 1, 1, 1);
