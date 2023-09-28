/*-
 * Copyright (c) 2014 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

/*
 * Ethernet media access controller (EMAC)
 * Chapter 17, Altera Cyclone V Device Handbook (CV-5V2 2014.07.22)
 *
 * EMAC is an instance of the Synopsys DesignWare 3504-0
 * Universal 10/100/1000 Ethernet MAC (DWC_gmac).
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/gpio.h>
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

#include <dev/extres/clk/clk.h>
#include <dev/extres/hwreset/hwreset.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/mii/mii_fdt.h>

#include <dev/dwc/if_dwcvar.h>
#include <dev/dwc/dwc1000_reg.h>
#include <dev/dwc/dwc1000_dma.h>

#include "if_dwc_if.h"
#include "gpio_if.h"
#include "miibus_if.h"

#define	MAC_RESET_TIMEOUT	100
#define	WATCHDOG_TIMEOUT_SECS	5
#define	STATS_HARVEST_INTERVAL	2

static struct resource_spec dwc_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ -1, 0 }
};

static void dwc_stop_locked(struct dwc_softc *sc);
static void dwc_setup_rxfilter(struct dwc_softc *sc);
static void dwc_setup_core(struct dwc_softc *sc);
static void dwc_enable_mac(struct dwc_softc *sc, bool enable);

static void dwc_tick(void *arg);

/* Pause time field in the transmitted control frame */
static int dwc_pause_time = 0xffff;
TUNABLE_INT("hw.dwc.pause_time", &dwc_pause_time);

/*
 * MIIBUS functions
 */

static int
dwc_miibus_read_reg(device_t dev, int phy, int reg)
{
	struct dwc_softc *sc;
	uint16_t mii;
	size_t cnt;
	int rv = 0;

	sc = device_get_softc(dev);

	mii = ((phy & GMII_ADDRESS_PA_MASK) << GMII_ADDRESS_PA_SHIFT)
	    | ((reg & GMII_ADDRESS_GR_MASK) << GMII_ADDRESS_GR_SHIFT)
	    | (sc->mii_clk << GMII_ADDRESS_CR_SHIFT)
	    | GMII_ADDRESS_GB; /* Busy flag */

	WRITE4(sc, GMII_ADDRESS, mii);

	for (cnt = 0; cnt < 1000; cnt++) {
		if (!(READ4(sc, GMII_ADDRESS) & GMII_ADDRESS_GB)) {
			rv = READ4(sc, GMII_DATA);
			break;
		}
		DELAY(10);
	}

	return rv;
}

static int
dwc_miibus_write_reg(device_t dev, int phy, int reg, int val)
{
	struct dwc_softc *sc;
	uint16_t mii;
	size_t cnt;

	sc = device_get_softc(dev);

	mii = ((phy & GMII_ADDRESS_PA_MASK) << GMII_ADDRESS_PA_SHIFT)
	    | ((reg & GMII_ADDRESS_GR_MASK) << GMII_ADDRESS_GR_SHIFT)
	    | (sc->mii_clk << GMII_ADDRESS_CR_SHIFT)
	    | GMII_ADDRESS_GB | GMII_ADDRESS_GW;

	WRITE4(sc, GMII_DATA, val);
	WRITE4(sc, GMII_ADDRESS, mii);

	for (cnt = 0; cnt < 1000; cnt++) {
		if (!(READ4(sc, GMII_ADDRESS) & GMII_ADDRESS_GB)) {
			break;
                }
		DELAY(10);
	}

	return (0);
}

static void
dwc_miibus_statchg(device_t dev)
{
	struct dwc_softc *sc;
	struct mii_data *mii;
	uint32_t reg;

	/*
	 * Called by the MII bus driver when the PHY establishes
	 * link to set the MAC interface registers.
	 */

	sc = device_get_softc(dev);

	DWC_ASSERT_LOCKED(sc);

	mii = sc->mii_softc;

	if (mii->mii_media_status & IFM_ACTIVE)
		sc->link_is_up = true;
	else
		sc->link_is_up = false;

	reg = READ4(sc, MAC_CONFIGURATION);
	switch (IFM_SUBTYPE(mii->mii_media_active)) {
	case IFM_1000_T:
	case IFM_1000_SX:
		reg &= ~(CONF_FES | CONF_PS);
		break;
	case IFM_100_TX:
		reg |= (CONF_FES | CONF_PS);
		break;
	case IFM_10_T:
		reg &= ~(CONF_FES);
		reg |= (CONF_PS);
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
	if ((IFM_OPTIONS(mii->mii_media_active) & IFM_FDX) != 0)
		reg |= (CONF_DM);
	else
		reg &= ~(CONF_DM);
	WRITE4(sc, MAC_CONFIGURATION, reg);

	reg = FLOW_CONTROL_UP;
	if ((IFM_OPTIONS(mii->mii_media_active) & IFM_ETH_TXPAUSE) != 0)
		reg |= FLOW_CONTROL_TX;
	if ((IFM_OPTIONS(mii->mii_media_active) & IFM_ETH_RXPAUSE) != 0)
		reg |= FLOW_CONTROL_RX;
	if ((IFM_OPTIONS(mii->mii_media_active) & IFM_FDX) != 0)
		reg |= dwc_pause_time << FLOW_CONTROL_PT_SHIFT;
	WRITE4(sc, FLOW_CONTROL, reg);

	IF_DWC_SET_SPEED(dev, IFM_SUBTYPE(mii->mii_media_active));

}

/*
 * Media functions
 */

static void
dwc_media_status(if_t ifp, struct ifmediareq *ifmr)
{
	struct dwc_softc *sc;
	struct mii_data *mii;

	sc = if_getsoftc(ifp);
	mii = sc->mii_softc;
	DWC_LOCK(sc);
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
	DWC_UNLOCK(sc);
}

static int
dwc_media_change_locked(struct dwc_softc *sc)
{

	return (mii_mediachg(sc->mii_softc));
}

static int
dwc_media_change(if_t ifp)
{
	struct dwc_softc *sc;
	int error;

	sc = if_getsoftc(ifp);

	DWC_LOCK(sc);
	error = dwc_media_change_locked(sc);
	DWC_UNLOCK(sc);
	return (error);
}

/*
 * Core functions
 */

static const uint8_t nibbletab[] = {
	/* 0x0 0000 -> 0000 */  0x0,
	/* 0x1 0001 -> 1000 */  0x8,
	/* 0x2 0010 -> 0100 */  0x4,
	/* 0x3 0011 -> 1100 */  0xc,
	/* 0x4 0100 -> 0010 */  0x2,
	/* 0x5 0101 -> 1010 */  0xa,
	/* 0x6 0110 -> 0110 */  0x6,
	/* 0x7 0111 -> 1110 */  0xe,
	/* 0x8 1000 -> 0001 */  0x1,
	/* 0x9 1001 -> 1001 */  0x9,
	/* 0xa 1010 -> 0101 */  0x5,
	/* 0xb 1011 -> 1101 */  0xd,
	/* 0xc 1100 -> 0011 */  0x3,
	/* 0xd 1101 -> 1011 */  0xb,
	/* 0xe 1110 -> 0111 */  0x7,
	/* 0xf 1111 -> 1111 */  0xf, };

static uint8_t
bitreverse(uint8_t x)
{

	return (nibbletab[x & 0xf] << 4) | nibbletab[x >> 4];
}

static u_int
dwc_hash_maddr(void *arg, struct sockaddr_dl *sdl, u_int cnt)
{
	struct dwc_hash_maddr_ctx *ctx = arg;
	uint32_t crc, hashbit, hashreg;
	uint8_t val;

	crc = ether_crc32_le(LLADDR(sdl), ETHER_ADDR_LEN);
	/* Take lower 8 bits and reverse it */
	val = bitreverse(~crc & 0xff);
	if (ctx->sc->mactype != DWC_GMAC_EXT_DESC)
		val >>= 2; /* Only need lower 6 bits */
	hashreg = (val >> 5);
	hashbit = (val & 31);
	ctx->hash[hashreg] |= (1 << hashbit);

	return (1);
}

static void
dwc_setup_rxfilter(struct dwc_softc *sc)
{
	struct dwc_hash_maddr_ctx ctx;
	if_t ifp;
	uint8_t *eaddr;
	uint32_t ffval, hi, lo;
	int nhash, i;

	DWC_ASSERT_LOCKED(sc);

	ifp = sc->ifp;
	nhash = sc->mactype != DWC_GMAC_EXT_DESC ? 2 : 8;

	/*
	 * Set the multicast (group) filter hash.
	 */
	if ((if_getflags(ifp) & IFF_ALLMULTI) != 0) {
		ffval = (FRAME_FILTER_PM);
		for (i = 0; i < nhash; i++)
			ctx.hash[i] = ~0;
	} else {
		ffval = (FRAME_FILTER_HMC);
		for (i = 0; i < nhash; i++)
			ctx.hash[i] = 0;
		ctx.sc = sc;
		if_foreach_llmaddr(ifp, dwc_hash_maddr, &ctx);
	}

	/*
	 * Set the individual address filter hash.
	 */
	if ((if_getflags(ifp) & IFF_PROMISC) != 0)
		ffval |= (FRAME_FILTER_PR);

	/*
	 * Set the primary address.
	 */
	eaddr = if_getlladdr(ifp);
	lo = eaddr[0] | (eaddr[1] << 8) | (eaddr[2] << 16) |
	    (eaddr[3] << 24);
	hi = eaddr[4] | (eaddr[5] << 8);
	WRITE4(sc, MAC_ADDRESS_LOW(0), lo);
	WRITE4(sc, MAC_ADDRESS_HIGH(0), hi);
	WRITE4(sc, MAC_FRAME_FILTER, ffval);
	if (sc->mactype != DWC_GMAC_EXT_DESC) {
		WRITE4(sc, GMAC_MAC_HTLOW, ctx.hash[0]);
		WRITE4(sc, GMAC_MAC_HTHIGH, ctx.hash[1]);
	} else {
		for (i = 0; i < nhash; i++)
			WRITE4(sc, HASH_TABLE_REG(i), ctx.hash[i]);
	}
}

static void
dwc_setup_core(struct dwc_softc *sc)
{
	uint32_t reg;

	DWC_ASSERT_LOCKED(sc);

	/* Enable core */
	reg = READ4(sc, MAC_CONFIGURATION);
	reg |= (CONF_JD | CONF_ACS | CONF_BE);
	WRITE4(sc, MAC_CONFIGURATION, reg);
}

static void
dwc_enable_mac(struct dwc_softc *sc, bool enable)
{
	uint32_t reg;

	DWC_ASSERT_LOCKED(sc);
	reg = READ4(sc, MAC_CONFIGURATION);
	if (enable)
		reg |= CONF_TE | CONF_RE;
	else
		reg &= ~(CONF_TE | CONF_RE);
	WRITE4(sc, MAC_CONFIGURATION, reg);
}

static void
dwc_enable_csum_offload(struct dwc_softc *sc)
{
	uint32_t reg;

	DWC_ASSERT_LOCKED(sc);
	reg = READ4(sc, MAC_CONFIGURATION);
	if ((if_getcapenable(sc->ifp) & IFCAP_RXCSUM) != 0)
		reg |= CONF_IPC;
	else
		reg &= ~CONF_IPC;
	WRITE4(sc, MAC_CONFIGURATION, reg);
}

static void
dwc_get_hwaddr(struct dwc_softc *sc, uint8_t *hwaddr)
{
	uint32_t hi, lo, rnd;

	/*
	 * Try to recover a MAC address from the running hardware. If there's
	 * something non-zero there, assume the bootloader did the right thing
	 * and just use it.
	 *
	 * Otherwise, set the address to a convenient locally assigned address,
	 * 'bsd' + random 24 low-order bits.  'b' is 0x62, which has the locally
	 * assigned bit set, and the broadcast/multicast bit clear.
	 */
	lo = READ4(sc, MAC_ADDRESS_LOW(0));
	hi = READ4(sc, MAC_ADDRESS_HIGH(0)) & 0xffff;
	if ((lo != 0xffffffff) || (hi != 0xffff)) {
		hwaddr[0] = (lo >>  0) & 0xff;
		hwaddr[1] = (lo >>  8) & 0xff;
		hwaddr[2] = (lo >> 16) & 0xff;
		hwaddr[3] = (lo >> 24) & 0xff;
		hwaddr[4] = (hi >>  0) & 0xff;
		hwaddr[5] = (hi >>  8) & 0xff;
	} else {
		rnd = arc4random() & 0x00ffffff;
		hwaddr[0] = 'b';
		hwaddr[1] = 's';
		hwaddr[2] = 'd';
		hwaddr[3] = rnd >> 16;
		hwaddr[4] = rnd >>  8;
		hwaddr[5] = rnd >>  0;
	}
}

/*
 * if_ functions
 */

static void
dwc_txstart_locked(struct dwc_softc *sc)
{
	if_t ifp;
	struct mbuf *m;
	int enqueued;

	DWC_ASSERT_LOCKED(sc);

	if (!sc->link_is_up)
		return;

	ifp = sc->ifp;

	if ((if_getdrvflags(ifp) & (IFF_DRV_RUNNING|IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING)
		return;

	enqueued = 0;

	for (;;) {
		if (sc->tx_desccount > (TX_DESC_COUNT - TX_MAP_MAX_SEGS  + 1)) {
			if_setdrvflagbits(ifp, IFF_DRV_OACTIVE, 0);
			break;
		}

		if (sc->tx_mapcount == (TX_MAP_COUNT - 1)) {
			if_setdrvflagbits(ifp, IFF_DRV_OACTIVE, 0);
			break;
		}

		m = if_dequeue(ifp);
		if (m == NULL)
			break;
		if (dma1000_setup_txbuf(sc, sc->tx_map_head, &m) != 0) {
			if_sendq_prepend(ifp, m);
			if_setdrvflagbits(ifp, IFF_DRV_OACTIVE, 0);
			break;
		}
		bpf_mtap_if(ifp, m);
		sc->tx_map_head = next_txidx(sc, sc->tx_map_head);
		sc->tx_mapcount++;
		++enqueued;
	}

	if (enqueued != 0) {
		WRITE4(sc, TRANSMIT_POLL_DEMAND, 0x1);
		sc->tx_watchdog_count = WATCHDOG_TIMEOUT_SECS;
	}
}

static void
dwc_txstart(if_t ifp)
{
	struct dwc_softc *sc = if_getsoftc(ifp);

	DWC_LOCK(sc);
	dwc_txstart_locked(sc);
	DWC_UNLOCK(sc);
}

static void
dwc_init_locked(struct dwc_softc *sc)
{
	if_t ifp = sc->ifp;

	DWC_ASSERT_LOCKED(sc);

	if (if_getdrvflags(ifp) & IFF_DRV_RUNNING)
		return;

	/*
	 * Call mii_mediachg() which will call back into dwc_miibus_statchg()
	 * to set up the remaining config registers based on current media.
	 */
	mii_mediachg(sc->mii_softc);

	dwc_setup_rxfilter(sc);
	dwc_setup_core(sc);
	dwc_enable_mac(sc, true);
	dwc_enable_csum_offload(sc);
	dma1000_start(sc);

	if_setdrvflagbits(ifp, IFF_DRV_RUNNING, IFF_DRV_OACTIVE);

	callout_reset(&sc->dwc_callout, hz, dwc_tick, sc);
}

static void
dwc_init(void *if_softc)
{
	struct dwc_softc *sc = if_softc;

	DWC_LOCK(sc);
	dwc_init_locked(sc);
	DWC_UNLOCK(sc);
}

static void
dwc_stop_locked(struct dwc_softc *sc)
{
	if_t ifp;

	DWC_ASSERT_LOCKED(sc);

	ifp = sc->ifp;
	if_setdrvflagbits(ifp, 0, IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
	sc->tx_watchdog_count = 0;
	sc->stats_harvest_count = 0;

	callout_stop(&sc->dwc_callout);

	dma1000_stop(sc);
	dwc_enable_mac(sc, false);
}

static int
dwc_ioctl(if_t ifp, u_long cmd, caddr_t data)
{
	struct dwc_softc *sc;
	struct mii_data *mii;
	struct ifreq *ifr;
	int flags, mask, error;

	sc = if_getsoftc(ifp);
	ifr = (struct ifreq *)data;

	error = 0;
	switch (cmd) {
	case SIOCSIFFLAGS:
		DWC_LOCK(sc);
		if (if_getflags(ifp) & IFF_UP) {
			if (if_getdrvflags(ifp) & IFF_DRV_RUNNING) {
				flags = if_getflags(ifp) ^ sc->if_flags;
				if ((flags & (IFF_PROMISC|IFF_ALLMULTI)) != 0)
					dwc_setup_rxfilter(sc);
			} else {
				if (!sc->is_detaching)
					dwc_init_locked(sc);
			}
		} else {
			if (if_getdrvflags(ifp) & IFF_DRV_RUNNING)
				dwc_stop_locked(sc);
		}
		sc->if_flags = if_getflags(ifp);
		DWC_UNLOCK(sc);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (if_getdrvflags(ifp) & IFF_DRV_RUNNING) {
			DWC_LOCK(sc);
			dwc_setup_rxfilter(sc);
			DWC_UNLOCK(sc);
		}
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		mii = sc->mii_softc;
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, cmd);
		break;
	case SIOCSIFCAP:
		mask = ifr->ifr_reqcap ^ if_getcapenable(ifp);
		if (mask & IFCAP_VLAN_MTU) {
			/* No work to do except acknowledge the change took */
			if_togglecapenable(ifp, IFCAP_VLAN_MTU);
		}
		if (mask & IFCAP_RXCSUM)
			if_togglecapenable(ifp, IFCAP_RXCSUM);
		if (mask & IFCAP_TXCSUM)
			if_togglecapenable(ifp, IFCAP_TXCSUM);
		if ((if_getcapenable(ifp) & IFCAP_TXCSUM) != 0)
			if_sethwassistbits(ifp, CSUM_IP | CSUM_UDP | CSUM_TCP, 0);
		else
			if_sethwassistbits(ifp, 0, CSUM_IP | CSUM_UDP | CSUM_TCP);

		if (if_getdrvflags(ifp) & IFF_DRV_RUNNING) {
			DWC_LOCK(sc);
			dwc_enable_csum_offload(sc);
			DWC_UNLOCK(sc);
		}
		break;

	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}

	return (error);
}

/*
 * Interrupts functions
 */


static void
dwc_intr(void *arg)
{
	struct dwc_softc *sc;
	uint32_t reg;

	sc = arg;

	DWC_LOCK(sc);

	reg = READ4(sc, INTERRUPT_STATUS);
	if (reg)
		READ4(sc, SGMII_RGMII_SMII_CTRL_STATUS);

	reg = READ4(sc, DMA_STATUS);
	if (reg & DMA_STATUS_NIS) {
		if (reg & DMA_STATUS_RI)
			dma1000_rxfinish_locked(sc);

		if (reg & DMA_STATUS_TI) {
			dma1000_txfinish_locked(sc);
			dwc_txstart_locked(sc);
		}
	}

	if (reg & DMA_STATUS_AIS) {
		if (reg & DMA_STATUS_FBI) {
			/* Fatal bus error */
			device_printf(sc->dev,
			    "Ethernet DMA error, restarting controller.\n");
			dwc_stop_locked(sc);
			dwc_init_locked(sc);
		}
	}

	WRITE4(sc, DMA_STATUS, reg & DMA_STATUS_INTR_MASK);
	DWC_UNLOCK(sc);
}

/*
 * Stats
 */

static void dwc_clear_stats(struct dwc_softc *sc)
{
	uint32_t reg;

	reg = READ4(sc, MMC_CONTROL);
	reg |= (MMC_CONTROL_CNTRST);
	WRITE4(sc, MMC_CONTROL, reg);
}

static void
dwc_harvest_stats(struct dwc_softc *sc)
{
	if_t ifp;

	/* We don't need to harvest too often. */
	if (++sc->stats_harvest_count < STATS_HARVEST_INTERVAL)
		return;

	sc->stats_harvest_count = 0;
	ifp = sc->ifp;

	if_inc_counter(ifp, IFCOUNTER_IERRORS,
	    READ4(sc, RXOVERSIZE_G) + READ4(sc, RXUNDERSIZE_G) +
	    READ4(sc, RXCRCERROR) + READ4(sc, RXALIGNMENTERROR) +
	    READ4(sc, RXRUNTERROR) + READ4(sc, RXJABBERERROR) +
	    READ4(sc, RXLENGTHERROR));

	if_inc_counter(ifp, IFCOUNTER_OERRORS,
	    READ4(sc, TXOVERSIZE_G) + READ4(sc, TXEXCESSDEF) +
	    READ4(sc, TXCARRIERERR) + READ4(sc, TXUNDERFLOWERROR));

	if_inc_counter(ifp, IFCOUNTER_COLLISIONS,
	    READ4(sc, TXEXESSCOL) + READ4(sc, TXLATECOL));

	dwc_clear_stats(sc);
}

static void
dwc_tick(void *arg)
{
	struct dwc_softc *sc;
	if_t ifp;
	int link_was_up;

	sc = arg;

	DWC_ASSERT_LOCKED(sc);

	ifp = sc->ifp;

	if ((if_getdrvflags(ifp) & IFF_DRV_RUNNING) == 0)
	    return;

	/*
	 * Typical tx watchdog.  If this fires it indicates that we enqueued
	 * packets for output and never got a txdone interrupt for them.  Maybe
	 * it's a missed interrupt somehow, just pretend we got one.
	 */
	if (sc->tx_watchdog_count > 0) {
		if (--sc->tx_watchdog_count == 0) {
			dma1000_txfinish_locked(sc);
		}
	}

	/* Gather stats from hardware counters. */
	dwc_harvest_stats(sc);

	/* Check the media status. */
	link_was_up = sc->link_is_up;
	mii_tick(sc->mii_softc);
	if (sc->link_is_up && !link_was_up)
		dwc_txstart_locked(sc);

	/* Schedule another check one second from now. */
	callout_reset(&sc->dwc_callout, hz, dwc_tick, sc);
}

static int
dwc_reset_phy(struct dwc_softc *sc)
{
	pcell_t gpio_prop[4];
	pcell_t delay_prop[3];
	phandle_t gpio_node;
	device_t gpio;
	uint32_t pin, flags;
	uint32_t pin_value;

	/*
	 * All those properties are deprecated but still used in some DTS.
	 * The new way to deal with this is to use the generic bindings
	 * present in the ethernet-phy node.
	 */
	if (OF_getencprop(sc->node, "snps,reset-gpio",
	    gpio_prop, sizeof(gpio_prop)) <= 0)
		return (0);

	if (OF_getencprop(sc->node, "snps,reset-delays-us",
	    delay_prop, sizeof(delay_prop)) <= 0) {
		device_printf(sc->dev,
		    "Wrong property for snps,reset-delays-us");
		return (ENXIO);
	}

	gpio_node = OF_node_from_xref(gpio_prop[0]);
	if ((gpio = OF_device_from_xref(gpio_prop[0])) == NULL) {
		device_printf(sc->dev,
		    "Can't find gpio controller for phy reset\n");
		return (ENXIO);
	}

	if (GPIO_MAP_GPIOS(gpio, sc->node, gpio_node,
	    nitems(gpio_prop) - 1,
	    gpio_prop + 1, &pin, &flags) != 0) {
		device_printf(sc->dev, "Can't map gpio for phy reset\n");
		return (ENXIO);
	}

	pin_value = GPIO_PIN_LOW;
	if (OF_hasprop(sc->node, "snps,reset-active-low"))
		pin_value = GPIO_PIN_HIGH;

	GPIO_PIN_SETFLAGS(gpio, pin, GPIO_PIN_OUTPUT);
	GPIO_PIN_SET(gpio, pin, pin_value);
	DELAY(delay_prop[0] * 5);
	GPIO_PIN_SET(gpio, pin, !pin_value);
	DELAY(delay_prop[1] * 5);
	GPIO_PIN_SET(gpio, pin, pin_value);
	DELAY(delay_prop[2] * 5);

	return (0);
}

static int
dwc_clock_init(struct dwc_softc *sc)
{
	int rv;
	int64_t freq;

	/* Required clock */
	rv = clk_get_by_ofw_name(sc->dev, 0, "stmmaceth", &sc->clk_stmmaceth);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get GMAC main clock\n");
		return (ENXIO);
	}
	if ((rv = clk_enable(sc->clk_stmmaceth)) != 0) {
		device_printf(sc->dev, "could not enable main clock\n");
		return (rv);
	}

	/* Optional clock */
	rv = clk_get_by_ofw_name(sc->dev, 0, "pclk", &sc->clk_pclk);
	if (rv != 0)
		return (0);
	if ((rv = clk_enable(sc->clk_pclk)) != 0) {
		device_printf(sc->dev, "could not enable peripheral clock\n");
		return (rv);
	}

	if (bootverbose) {
		clk_get_freq(sc->clk_stmmaceth, &freq);
		device_printf(sc->dev, "MAC clock(%s) freq: %jd\n",
		    clk_get_name(sc->clk_stmmaceth), (intmax_t)freq);
	}

	return (0);
}

static int
dwc_reset_deassert(struct dwc_softc *sc)
{
	int rv;

	/* Required reset */
	rv = hwreset_get_by_ofw_name(sc->dev, 0, "stmmaceth", &sc->rst_stmmaceth);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get GMAC reset\n");
		return (ENXIO);
	}
	rv = hwreset_deassert(sc->rst_stmmaceth);
	if (rv != 0) {
		device_printf(sc->dev, "could not de-assert GMAC reset\n");
		return (rv);
	}

	/* Optional reset */
	rv = hwreset_get_by_ofw_name(sc->dev, 0, "ahb", &sc->rst_ahb);
	if (rv != 0)
		return (0);
	rv = hwreset_deassert(sc->rst_ahb);
	if (rv != 0) {
		device_printf(sc->dev, "could not de-assert AHB reset\n");
		return (rv);
	}

	return (0);
}

/*
 * Probe/Attach functions
 */

static int
dwc_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "snps,dwmac"))
		return (ENXIO);

	device_set_desc(dev, "Gigabit Ethernet Controller");
	return (BUS_PROBE_DEFAULT);
}

static int
dwc_attach(device_t dev)
{
	uint8_t macaddr[ETHER_ADDR_LEN];
	struct dwc_softc *sc;
	if_t ifp;
	int error, i;
	uint32_t reg;
	uint32_t txpbl, rxpbl, pbl;
	bool nopblx8 = false;
	bool fixed_burst = false;
	bool mixed_burst = false;
	bool aal = false;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->rx_idx = 0;
	sc->tx_desccount = TX_DESC_COUNT;
	sc->tx_mapcount = 0;
	sc->mii_clk = IF_DWC_MII_CLK(dev);
	sc->mactype = IF_DWC_MAC_TYPE(dev);

	sc->node = ofw_bus_get_node(dev);
	switch (mii_fdt_get_contype(sc->node)) {
	case MII_CONTYPE_RGMII:
	case MII_CONTYPE_RGMII_ID:
	case MII_CONTYPE_RGMII_RXID:
	case MII_CONTYPE_RGMII_TXID:
		sc->phy_mode = PHY_MODE_RGMII;
		break;
	case MII_CONTYPE_RMII:
		sc->phy_mode = PHY_MODE_RMII;
		break;
	case MII_CONTYPE_MII:
		sc->phy_mode = PHY_MODE_MII;
		break;
	default:
		device_printf(dev, "Unsupported MII type\n");
		return (ENXIO);
	}

	if (OF_getencprop(sc->node, "snps,pbl", &pbl, sizeof(uint32_t)) <= 0)
		pbl = BUS_MODE_DEFAULT_PBL;
	if (OF_getencprop(sc->node, "snps,txpbl", &txpbl, sizeof(uint32_t)) <= 0)
		txpbl = pbl;
	if (OF_getencprop(sc->node, "snps,rxpbl", &rxpbl, sizeof(uint32_t)) <= 0)
		rxpbl = pbl;
	if (OF_hasprop(sc->node, "snps,no-pbl-x8") == 1)
		nopblx8 = true;
	if (OF_hasprop(sc->node, "snps,fixed-burst") == 1)
		fixed_burst = true;
	if (OF_hasprop(sc->node, "snps,mixed-burst") == 1)
		mixed_burst = true;
	if (OF_hasprop(sc->node, "snps,aal") == 1)
		aal = true;

	error = clk_set_assigned(dev, ofw_bus_get_node(dev));
	if (error != 0) {
		device_printf(dev, "clk_set_assigned failed\n");
		return (error);
	}

	/* Enable main clock */
	if ((error = dwc_clock_init(sc)) != 0)
		return (error);
	/* De-assert main reset */
	if ((error = dwc_reset_deassert(sc)) != 0)
		return (error);

	if (IF_DWC_INIT(dev) != 0)
		return (ENXIO);

	if (bus_alloc_resources(dev, dwc_spec, sc->res)) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	/* Read MAC before reset */
	dwc_get_hwaddr(sc, macaddr);

	/* Reset the PHY if needed */
	if (dwc_reset_phy(sc) != 0) {
		device_printf(dev, "Can't reset the PHY\n");
		bus_release_resources(dev, dwc_spec, sc->res);
		return (ENXIO);
	}

	/* Reset */
	reg = READ4(sc, BUS_MODE);
	reg |= (BUS_MODE_SWR);
	WRITE4(sc, BUS_MODE, reg);

	for (i = 0; i < MAC_RESET_TIMEOUT; i++) {
		if ((READ4(sc, BUS_MODE) & BUS_MODE_SWR) == 0)
			break;
		DELAY(10);
	}
	if (i >= MAC_RESET_TIMEOUT) {
		device_printf(sc->dev, "Can't reset DWC.\n");
		bus_release_resources(dev, dwc_spec, sc->res);
		return (ENXIO);
	}

	reg = BUS_MODE_USP;
	if (!nopblx8)
		reg |= BUS_MODE_EIGHTXPBL;
	reg |= (txpbl << BUS_MODE_PBL_SHIFT);
	reg |= (rxpbl << BUS_MODE_RPBL_SHIFT);
	if (fixed_burst)
		reg |= BUS_MODE_FIXEDBURST;
	if (mixed_burst)
		reg |= BUS_MODE_MIXEDBURST;
	if (aal)
		reg |= BUS_MODE_AAL;

	WRITE4(sc, BUS_MODE, reg);

	/*
	 * DMA must be stop while changing descriptor list addresses.
	 */
	reg = READ4(sc, OPERATION_MODE);
	reg &= ~(MODE_ST | MODE_SR);
	WRITE4(sc, OPERATION_MODE, reg);

	if (dma1000_init(sc)) {
		bus_release_resources(dev, dwc_spec, sc->res);
		return (ENXIO);
	}

	/* Setup addresses */
	WRITE4(sc, RX_DESCR_LIST_ADDR, sc->rxdesc_ring_paddr);
	WRITE4(sc, TX_DESCR_LIST_ADDR, sc->txdesc_ring_paddr);

	mtx_init(&sc->mtx, device_get_nameunit(sc->dev),
	    MTX_NETWORK_LOCK, MTX_DEF);

	callout_init_mtx(&sc->dwc_callout, &sc->mtx, 0);

	/* Setup interrupt handler. */
	error = bus_setup_intr(dev, sc->res[1], INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, dwc_intr, sc, &sc->intr_cookie);
	if (error != 0) {
		device_printf(dev, "could not setup interrupt handler.\n");
		bus_release_resources(dev, dwc_spec, sc->res);
		return (ENXIO);
	}

	/* Set up the ethernet interface. */
	sc->ifp = ifp = if_alloc(IFT_ETHER);

	if_setsoftc(ifp, sc);
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	if_setflags(sc->ifp, IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST);
	if_setstartfn(ifp, dwc_txstart);
	if_setioctlfn(ifp, dwc_ioctl);
	if_setinitfn(ifp, dwc_init);
	if_setsendqlen(ifp, TX_MAP_COUNT - 1);
	if_setsendqready(sc->ifp);
	if_sethwassist(sc->ifp, CSUM_IP | CSUM_UDP | CSUM_TCP);
	if_setcapabilities(sc->ifp, IFCAP_VLAN_MTU | IFCAP_HWCSUM);
	if_setcapenable(sc->ifp, if_getcapabilities(sc->ifp));

	/* Attach the mii driver. */
	error = mii_attach(dev, &sc->miibus, ifp, dwc_media_change,
	    dwc_media_status, BMSR_DEFCAPMASK, MII_PHY_ANY,
	    MII_OFFSET_ANY, 0);

	if (error != 0) {
		device_printf(dev, "PHY attach failed\n");
		bus_teardown_intr(dev, sc->res[1], sc->intr_cookie);
		bus_release_resources(dev, dwc_spec, sc->res);
		return (ENXIO);
	}
	sc->mii_softc = device_get_softc(sc->miibus);

	/* All ready to run, attach the ethernet interface. */
	ether_ifattach(ifp, macaddr);
	sc->is_attached = true;

	return (0);
}

static int
dwc_detach(device_t dev)
{
	struct dwc_softc *sc;

	sc = device_get_softc(dev);

	/*
	 * Disable and tear down interrupts before anything else, so we don't
	 * race with the handler.
	 */
	WRITE4(sc, INTERRUPT_ENABLE, 0);
	if (sc->intr_cookie != NULL) {
		bus_teardown_intr(dev, sc->res[1], sc->intr_cookie);
	}

	if (sc->is_attached) {
		DWC_LOCK(sc);
		sc->is_detaching = true;
		dwc_stop_locked(sc);
		DWC_UNLOCK(sc);
		callout_drain(&sc->dwc_callout);
		ether_ifdetach(sc->ifp);
	}

	if (sc->miibus != NULL) {
		device_delete_child(dev, sc->miibus);
		sc->miibus = NULL;
	}
	bus_generic_detach(dev);

	/* Free DMA descriptors */
	dma1000_free(sc);

	if (sc->ifp != NULL) {
		if_free(sc->ifp);
		sc->ifp = NULL;
	}

	bus_release_resources(dev, dwc_spec, sc->res);

	mtx_destroy(&sc->mtx);
	return (0);
}

static device_method_t dwc_methods[] = {
	DEVMETHOD(device_probe,		dwc_probe),
	DEVMETHOD(device_attach,	dwc_attach),
	DEVMETHOD(device_detach,	dwc_detach),

	/* MII Interface */
	DEVMETHOD(miibus_readreg,	dwc_miibus_read_reg),
	DEVMETHOD(miibus_writereg,	dwc_miibus_write_reg),
	DEVMETHOD(miibus_statchg,	dwc_miibus_statchg),

	{ 0, 0 }
};

driver_t dwc_driver = {
	"dwc",
	dwc_methods,
	sizeof(struct dwc_softc),
};

DRIVER_MODULE(dwc, simplebus, dwc_driver, 0, 0);
DRIVER_MODULE(miibus, dwc, miibus_driver, 0, 0);

MODULE_DEPEND(dwc, ether, 1, 1, 1);
MODULE_DEPEND(dwc, miibus, 1, 1, 1);
