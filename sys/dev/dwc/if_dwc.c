/*-
 * Copyright (c) 2014 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
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
__FBSDID("$FreeBSD$");

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

#include <dev/dwc/if_dwc.h>
#include <dev/dwc/if_dwcvar.h>
#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#ifdef EXT_RESOURCES
#include <dev/extres/clk/clk.h>
#include <dev/extres/hwreset/hwreset.h>
#endif

#include "if_dwc_if.h"
#include "gpio_if.h"
#include "miibus_if.h"

#define	READ4(_sc, _reg) \
	bus_read_4((_sc)->res[0], _reg)
#define	WRITE4(_sc, _reg, _val) \
	bus_write_4((_sc)->res[0], _reg, _val)

#define	MAC_RESET_TIMEOUT	100
#define	WATCHDOG_TIMEOUT_SECS	5
#define	STATS_HARVEST_INTERVAL	2

#define	DWC_LOCK(sc)			mtx_lock(&(sc)->mtx)
#define	DWC_UNLOCK(sc)			mtx_unlock(&(sc)->mtx)
#define	DWC_ASSERT_LOCKED(sc)		mtx_assert(&(sc)->mtx, MA_OWNED)
#define	DWC_ASSERT_UNLOCKED(sc)		mtx_assert(&(sc)->mtx, MA_NOTOWNED)

/* TX descriptors - TDESC0 is almost unified */
#define	TDESC0_OWN		(1U << 31)
#define	TDESC0_IHE		(1U << 16)	/* IP Header Error */
#define	TDESC0_ES		(1U << 15)	/* Error Summary */
#define	TDESC0_JT		(1U << 14)	/* Jabber Timeout */
#define	TDESC0_FF		(1U << 13)	/* Frame Flushed */
#define	TDESC0_PCE		(1U << 12)	/* Payload Checksum Error */
#define	TDESC0_LOC		(1U << 11)	/* Loss of Carrier */
#define	TDESC0_NC		(1U << 10)	/* No Carrier */
#define	TDESC0_LC		(1U <<  9)	/* Late Collision */
#define	TDESC0_EC		(1U <<  8)	/* Excessive Collision */
#define	TDESC0_VF		(1U <<  7)	/* VLAN Frame */
#define	TDESC0_CC_MASK		0xf
#define	TDESC0_CC_SHIFT		3		/* Collision Count */
#define	TDESC0_ED		(1U <<  2)	/* Excessive Deferral */
#define	TDESC0_UF		(1U <<  1)	/* Underflow Error */
#define	TDESC0_DB		(1U <<  0)	/* Deferred Bit */
/* TX descriptors - TDESC0 extended format only */
#define	ETDESC0_IC		(1U << 30)	/* Interrupt on Completion */
#define	ETDESC0_LS		(1U << 29)	/* Last Segment */
#define	ETDESC0_FS		(1U << 28)	/* First Segment */
#define	ETDESC0_DC		(1U << 27)	/* Disable CRC */
#define	ETDESC0_DP		(1U << 26)	/* Disable Padding */
#define	ETDESC0_CIC_NONE	(0U << 22)	/* Checksum Insertion Control */
#define	ETDESC0_CIC_HDR		(1U << 22)
#define	ETDESC0_CIC_SEG 	(2U << 22)
#define	ETDESC0_CIC_FULL	(3U << 22)
#define	ETDESC0_TER		(1U << 21)	/* Transmit End of Ring */
#define	ETDESC0_TCH		(1U << 20)	/* Second Address Chained */

/* TX descriptors - TDESC1 normal format */
#define	NTDESC1_IC		(1U << 31)	/* Interrupt on Completion */
#define	NTDESC1_LS		(1U << 30)	/* Last Segment */
#define	NTDESC1_FS		(1U << 29)	/* First Segment */
#define	NTDESC1_CIC_NONE	(0U << 27)	/* Checksum Insertion Control */
#define	NTDESC1_CIC_HDR		(1U << 27)
#define	NTDESC1_CIC_SEG 	(2U << 27)
#define	NTDESC1_CIC_FULL	(3U << 27)
#define	NTDESC1_DC		(1U << 26)	/* Disable CRC */
#define	NTDESC1_TER		(1U << 25)	/* Transmit End of Ring */
#define	NTDESC1_TCH		(1U << 24)	/* Second Address Chained */
/* TX descriptors - TDESC1 extended format */
#define	ETDESC1_DP		(1U << 23)	/* Disable Padding */
#define	ETDESC1_TBS2_MASK	0x7ff
#define	ETDESC1_TBS2_SHIFT	11		/* Receive Buffer 2 Size */
#define	ETDESC1_TBS1_MASK	0x7ff
#define	ETDESC1_TBS1_SHIFT	0		/* Receive Buffer 1 Size */

/* RX descriptor - RDESC0 is unified */
#define	RDESC0_OWN		(1U << 31)
#define	RDESC0_AFM		(1U << 30)	/* Dest. Address Filter Fail */
#define	RDESC0_FL_MASK		0x3fff
#define	RDESC0_FL_SHIFT		16		/* Frame Length */
#define	RDESC0_ES		(1U << 15)	/* Error Summary */
#define	RDESC0_DE		(1U << 14)	/* Descriptor Error */
#define	RDESC0_SAF		(1U << 13)	/* Source Address Filter Fail */
#define	RDESC0_LE		(1U << 12)	/* Length Error */
#define	RDESC0_OE		(1U << 11)	/* Overflow Error */
#define	RDESC0_VLAN		(1U << 10)	/* VLAN Tag */
#define	RDESC0_FS		(1U <<  9)	/* First Descriptor */
#define	RDESC0_LS		(1U <<  8)	/* Last Descriptor */
#define	RDESC0_ICE		(1U <<  7)	/* IPC Checksum Error */
#define	RDESC0_LC		(1U <<  6)	/* Late Collision */
#define	RDESC0_FT		(1U <<  5)	/* Frame Type */
#define	RDESC0_RWT		(1U <<  4)	/* Receive Watchdog Timeout */
#define	RDESC0_RE		(1U <<  3)	/* Receive Error */
#define	RDESC0_DBE		(1U <<  2)	/* Dribble Bit Error */
#define	RDESC0_CE		(1U <<  1)	/* CRC Error */
#define	RDESC0_PCE		(1U <<  0)	/* Payload Checksum Error */
#define	RDESC0_RXMA		(1U <<  0)	/* Rx MAC Address */

/* RX descriptors - RDESC1 normal format */
#define	NRDESC1_DIC		(1U << 31)	/* Disable Intr on Completion */
#define	NRDESC1_RER		(1U << 25)	/* Receive End of Ring */
#define	NRDESC1_RCH		(1U << 24)	/* Second Address Chained */
#define	NRDESC1_RBS2_MASK	0x7ff
#define	NRDESC1_RBS2_SHIFT	11		/* Receive Buffer 2 Size */
#define	NRDESC1_RBS1_MASK	0x7ff
#define	NRDESC1_RBS1_SHIFT	0		/* Receive Buffer 1 Size */

/* RX descriptors - RDESC1 enhanced format */
#define	ERDESC1_DIC		(1U << 31)	/* Disable Intr on Completion */
#define	ERDESC1_RBS2_MASK	0x7ffff
#define	ERDESC1_RBS2_SHIFT	16		/* Receive Buffer 2 Size */
#define	ERDESC1_RER		(1U << 15)	/* Receive End of Ring */
#define	ERDESC1_RCH		(1U << 14)	/* Second Address Chained */
#define	ERDESC1_RBS1_MASK	0x7ffff
#define	ERDESC1_RBS1_SHIFT	0		/* Receive Buffer 1 Size */

/*
 * A hardware buffer descriptor.  Rx and Tx buffers have the same descriptor
 * layout, but the bits in the fields have different meanings.
 */
struct dwc_hwdesc
{
	uint32_t desc0;
	uint32_t desc1;
	uint32_t addr1;		/* ptr to first buffer data */
	uint32_t addr2;		/* ptr to next descriptor / second buffer data*/
};


struct dwc_hash_maddr_ctx {
	struct dwc_softc *sc;
	uint32_t hash[8];
};

/*
 * The hardware imposes alignment restrictions on various objects involved in
 * DMA transfers.  These values are expressed in bytes (not bits).
 */
#define	DWC_DESC_RING_ALIGN	2048

static struct resource_spec dwc_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ -1, 0 }
};

static void dwc_txfinish_locked(struct dwc_softc *sc);
static void dwc_rxfinish_locked(struct dwc_softc *sc);
static void dwc_stop_locked(struct dwc_softc *sc);
static void dwc_setup_rxfilter(struct dwc_softc *sc);
static void dwc_setup_core(struct dwc_softc *sc);
static void dwc_enable_mac(struct dwc_softc *sc, bool enable);
static void dwc_init_dma(struct dwc_softc *sc);
static void dwc_stop_dma(struct dwc_softc *sc);

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
dwc_media_status(struct ifnet * ifp, struct ifmediareq *ifmr)
{
	struct dwc_softc *sc;
	struct mii_data *mii;

	sc = ifp->if_softc;
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
dwc_media_change(struct ifnet * ifp)
{
	struct dwc_softc *sc;
	int error;

	sc = ifp->if_softc;

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
	struct ifnet *ifp;
	uint8_t *eaddr;
	uint32_t ffval, hi, lo;
	int nhash, i;

	DWC_ASSERT_LOCKED(sc);

	ifp = sc->ifp;
	nhash = sc->mactype != DWC_GMAC_EXT_DESC ? 2 : 8;

	/*
	 * Set the multicast (group) filter hash.
	 */
	if ((ifp->if_flags & IFF_ALLMULTI) != 0) {
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
	if (ifp->if_flags & IFF_PROMISC)
		ffval |= (FRAME_FILTER_PR);

	/*
	 * Set the primary address.
	 */
	eaddr = IF_LLADDR(ifp);
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
 * DMA functions
 */

static void
dwc_init_dma(struct dwc_softc *sc)
{
	uint32_t reg;

	DWC_ASSERT_LOCKED(sc);

	/* Initializa DMA and enable transmitters */
	reg = READ4(sc, OPERATION_MODE);
	reg |= (MODE_TSF | MODE_OSF | MODE_FUF);
	reg &= ~(MODE_RSF);
	reg |= (MODE_RTC_LEV32 << MODE_RTC_SHIFT);
	WRITE4(sc, OPERATION_MODE, reg);

	WRITE4(sc, INTERRUPT_ENABLE, INT_EN_DEFAULT);

	/* Start DMA */
	reg = READ4(sc, OPERATION_MODE);
	reg |= (MODE_ST | MODE_SR);
	WRITE4(sc, OPERATION_MODE, reg);
}

static void
dwc_stop_dma(struct dwc_softc *sc)
{
	uint32_t reg;

	DWC_ASSERT_LOCKED(sc);

	/* Stop DMA TX */
	reg = READ4(sc, OPERATION_MODE);
	reg &= ~(MODE_ST);
	WRITE4(sc, OPERATION_MODE, reg);

	/* Flush TX */
	reg = READ4(sc, OPERATION_MODE);
	reg |= (MODE_FTF);
	WRITE4(sc, OPERATION_MODE, reg);

	/* Stop DMA RX */
	reg = READ4(sc, OPERATION_MODE);
	reg &= ~(MODE_SR);
	WRITE4(sc, OPERATION_MODE, reg);
}

static inline uint32_t
next_rxidx(struct dwc_softc *sc, uint32_t curidx)
{

	return ((curidx + 1) % RX_DESC_COUNT);
}

static inline uint32_t
next_txidx(struct dwc_softc *sc, uint32_t curidx)
{

	return ((curidx + 1) % TX_DESC_COUNT);
}

static void
dwc_get1paddr(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{

	if (error != 0)
		return;
	*(bus_addr_t *)arg = segs[0].ds_addr;
}

inline static void
dwc_setup_txdesc(struct dwc_softc *sc, int idx, bus_addr_t paddr,
  uint32_t len, uint32_t flags)
{
	uint32_t desc0, desc1;

	/* Addr/len 0 means we're clearing the descriptor after xmit done. */
	if (paddr == 0 || len == 0) {
		desc0 = 0;
		desc1 = 0;
		--sc->txcount;
	} else {
		if (sc->mactype != DWC_GMAC_EXT_DESC) {
			desc0 = 0;
			desc1 = NTDESC1_TCH | NTDESC1_FS | NTDESC1_LS |
			    NTDESC1_IC | len | flags;
		} else {
			desc0 = ETDESC0_TCH | ETDESC0_FS | ETDESC0_LS |
			    ETDESC0_IC | flags;
			desc1 = len;
		}
		++sc->txcount;
	}

	sc->txdesc_ring[idx].addr1 = (uint32_t)(paddr);
	sc->txdesc_ring[idx].desc0 = desc0;
	sc->txdesc_ring[idx].desc1 = desc1;

	if (paddr && len) {
		wmb();
		sc->txdesc_ring[idx].desc0 |= TDESC0_OWN;
		wmb();
	}
}

static int
dwc_setup_txbuf(struct dwc_softc *sc, int idx, struct mbuf **mp)
{
	struct bus_dma_segment seg;
	int error, nsegs;
	struct mbuf * m;
	uint32_t flags = 0;

	if ((m = m_defrag(*mp, M_NOWAIT)) == NULL)
		return (ENOMEM);
	*mp = m;

	error = bus_dmamap_load_mbuf_sg(sc->txbuf_tag, sc->txbuf_map[idx].map,
	    m, &seg, &nsegs, 0);
	if (error != 0) {
		return (ENOMEM);
	}

	KASSERT(nsegs == 1, ("%s: %d segments returned!", __func__, nsegs));

	bus_dmamap_sync(sc->txbuf_tag, sc->txbuf_map[idx].map,
	    BUS_DMASYNC_PREWRITE);

	sc->txbuf_map[idx].mbuf = m;

	if ((m->m_pkthdr.csum_flags & CSUM_IP) != 0) {
		if ((m->m_pkthdr.csum_flags & (CSUM_TCP|CSUM_UDP)) != 0) {
			if (sc->mactype != DWC_GMAC_EXT_DESC)
				flags = NTDESC1_CIC_FULL;
			else
				flags = ETDESC0_CIC_FULL;
		} else {
			if (sc->mactype != DWC_GMAC_EXT_DESC)
				flags = NTDESC1_CIC_HDR;
			else
				flags = ETDESC0_CIC_HDR;
		}
	}

	dwc_setup_txdesc(sc, idx, seg.ds_addr, seg.ds_len, flags);

	return (0);
}

inline static uint32_t
dwc_setup_rxdesc(struct dwc_softc *sc, int idx, bus_addr_t paddr)
{
	uint32_t nidx;

	sc->rxdesc_ring[idx].addr1 = (uint32_t)paddr;
	nidx = next_rxidx(sc, idx);
	sc->rxdesc_ring[idx].addr2 = sc->rxdesc_ring_paddr +
	    (nidx * sizeof(struct dwc_hwdesc));
	if (sc->mactype != DWC_GMAC_EXT_DESC)
		sc->rxdesc_ring[idx].desc1 = NRDESC1_RCH |
		    MIN(MCLBYTES, NRDESC1_RBS1_MASK);
	else
		sc->rxdesc_ring[idx].desc1 = ERDESC1_RCH |
		    MIN(MCLBYTES, ERDESC1_RBS1_MASK);

	wmb();
	sc->rxdesc_ring[idx].desc0 = RDESC0_OWN;
	wmb();
	return (nidx);
}

static int
dwc_setup_rxbuf(struct dwc_softc *sc, int idx, struct mbuf *m)
{
	struct bus_dma_segment seg;
	int error, nsegs;

	m_adj(m, ETHER_ALIGN);

	error = bus_dmamap_load_mbuf_sg(sc->rxbuf_tag, sc->rxbuf_map[idx].map,
	    m, &seg, &nsegs, 0);
	if (error != 0)
		return (error);

	KASSERT(nsegs == 1, ("%s: %d segments returned!", __func__, nsegs));

	bus_dmamap_sync(sc->rxbuf_tag, sc->rxbuf_map[idx].map,
	    BUS_DMASYNC_PREREAD);

	sc->rxbuf_map[idx].mbuf = m;
	dwc_setup_rxdesc(sc, idx, seg.ds_addr);

	return (0);
}

static struct mbuf *
dwc_alloc_mbufcl(struct dwc_softc *sc)
{
	struct mbuf *m;

	m = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
	if (m != NULL)
		m->m_pkthdr.len = m->m_len = m->m_ext.ext_size;

	return (m);
}

static struct mbuf *
dwc_rxfinish_one(struct dwc_softc *sc, struct dwc_hwdesc *desc,
    struct dwc_bufmap *map)
{
	struct ifnet *ifp;
	struct mbuf *m, *m0;
	int len;
	uint32_t rdesc0;

	m = map->mbuf;
	ifp = sc->ifp;
	rdesc0 = desc ->desc0;
	/* Validate descriptor. */
	if (rdesc0 & RDESC0_ES) {
		/*
		 * Errored packet. Statistic counters are updated
		 * globally, so do nothing
		 */
		return (NULL);
	}

	if ((rdesc0 & (RDESC0_FS | RDESC0_LS)) !=
		    (RDESC0_FS | RDESC0_LS)) {
		/*
		 * Something very wrong happens. The whole packet should be
		 * recevied in one descriptr. Report problem.
		 */
		device_printf(sc->dev,
		    "%s: RX descriptor without FIRST and LAST bit set: 0x%08X",
		    __func__, rdesc0);
		return (NULL);
	}

	len = (rdesc0 >> RDESC0_FL_SHIFT) & RDESC0_FL_MASK;
	if (len < 64) {
		/*
		 * Lenght is invalid, recycle old mbuf
		 * Probably impossible case
		 */
		return (NULL);
	}

	/* Allocate new buffer */
	m0 = dwc_alloc_mbufcl(sc);
	if (m0 == NULL) {
		/* no new mbuf available, recycle old */
		if_inc_counter(sc->ifp, IFCOUNTER_IQDROPS, 1);
		return (NULL);
	}
	/* Do dmasync for newly received packet */
	bus_dmamap_sync(sc->rxbuf_tag, map->map, BUS_DMASYNC_POSTREAD);
	bus_dmamap_unload(sc->rxbuf_tag, map->map);

	/* Received packet is valid, process it */
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = len;
	m->m_len = len;
	if_inc_counter(ifp, IFCOUNTER_IPACKETS, 1);

	if ((if_getcapenable(ifp) & IFCAP_RXCSUM) != 0 &&
	  (rdesc0 & RDESC0_FT) != 0) {
		m->m_pkthdr.csum_flags = CSUM_IP_CHECKED;
		if ((rdesc0 & RDESC0_ICE) == 0)
			m->m_pkthdr.csum_flags |= CSUM_IP_VALID;
		if ((rdesc0 & RDESC0_PCE) == 0) {
			m->m_pkthdr.csum_flags |=
				CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
			m->m_pkthdr.csum_data = 0xffff;
		}
	}

	/* Remove trailing FCS */
	m_adj(m, -ETHER_CRC_LEN);

	DWC_UNLOCK(sc);
	(*ifp->if_input)(ifp, m);
	DWC_LOCK(sc);
	return (m0);
}

static int
setup_dma(struct dwc_softc *sc)
{
	struct mbuf *m;
	int error;
	int nidx;
	int idx;

	/*
	 * Set up TX descriptor ring, descriptors, and dma maps.
	 */
	error = bus_dma_tag_create(
	    bus_get_dma_tag(sc->dev),	/* Parent tag. */
	    DWC_DESC_RING_ALIGN, 0,	/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    TX_DESC_SIZE, 1, 		/* maxsize, nsegments */
	    TX_DESC_SIZE,		/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->txdesc_tag);
	if (error != 0) {
		device_printf(sc->dev,
		    "could not create TX ring DMA tag.\n");
		goto out;
	}

	error = bus_dmamem_alloc(sc->txdesc_tag, (void**)&sc->txdesc_ring,
	    BUS_DMA_COHERENT | BUS_DMA_WAITOK | BUS_DMA_ZERO,
	    &sc->txdesc_map);
	if (error != 0) {
		device_printf(sc->dev,
		    "could not allocate TX descriptor ring.\n");
		goto out;
	}

	error = bus_dmamap_load(sc->txdesc_tag, sc->txdesc_map,
	    sc->txdesc_ring, TX_DESC_SIZE, dwc_get1paddr,
	    &sc->txdesc_ring_paddr, 0);
	if (error != 0) {
		device_printf(sc->dev,
		    "could not load TX descriptor ring map.\n");
		goto out;
	}

	for (idx = 0; idx < TX_DESC_COUNT; idx++) {
		nidx = next_txidx(sc, idx);
		sc->txdesc_ring[idx].addr2 = sc->txdesc_ring_paddr +
		    (nidx * sizeof(struct dwc_hwdesc));
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
	    &sc->txbuf_tag);
	if (error != 0) {
		device_printf(sc->dev,
		    "could not create TX ring DMA tag.\n");
		goto out;
	}

	for (idx = 0; idx < TX_DESC_COUNT; idx++) {
		error = bus_dmamap_create(sc->txbuf_tag, BUS_DMA_COHERENT,
		    &sc->txbuf_map[idx].map);
		if (error != 0) {
			device_printf(sc->dev,
			    "could not create TX buffer DMA map.\n");
			goto out;
		}
		dwc_setup_txdesc(sc, idx, 0, 0, 0);
	}

	/*
	 * Set up RX descriptor ring, descriptors, dma maps, and mbufs.
	 */
	error = bus_dma_tag_create(
	    bus_get_dma_tag(sc->dev),	/* Parent tag. */
	    DWC_DESC_RING_ALIGN, 0,	/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    RX_DESC_SIZE, 1, 		/* maxsize, nsegments */
	    RX_DESC_SIZE,		/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->rxdesc_tag);
	if (error != 0) {
		device_printf(sc->dev,
		    "could not create RX ring DMA tag.\n");
		goto out;
	}

	error = bus_dmamem_alloc(sc->rxdesc_tag, (void **)&sc->rxdesc_ring,
	    BUS_DMA_COHERENT | BUS_DMA_WAITOK | BUS_DMA_ZERO,
	    &sc->rxdesc_map);
	if (error != 0) {
		device_printf(sc->dev,
		    "could not allocate RX descriptor ring.\n");
		goto out;
	}

	error = bus_dmamap_load(sc->rxdesc_tag, sc->rxdesc_map,
	    sc->rxdesc_ring, RX_DESC_SIZE, dwc_get1paddr,
	    &sc->rxdesc_ring_paddr, 0);
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
		device_printf(sc->dev,
		    "could not create RX buf DMA tag.\n");
		goto out;
	}

	for (idx = 0; idx < RX_DESC_COUNT; idx++) {
		error = bus_dmamap_create(sc->rxbuf_tag, BUS_DMA_COHERENT,
		    &sc->rxbuf_map[idx].map);
		if (error != 0) {
			device_printf(sc->dev,
			    "could not create RX buffer DMA map.\n");
			goto out;
		}
		if ((m = dwc_alloc_mbufcl(sc)) == NULL) {
			device_printf(sc->dev, "Could not alloc mbuf\n");
			error = ENOMEM;
			goto out;
		}
		if ((error = dwc_setup_rxbuf(sc, idx, m)) != 0) {
			device_printf(sc->dev,
			    "could not create new RX buffer.\n");
			goto out;
		}
	}

out:
	if (error != 0)
		return (ENXIO);

	return (0);
}

/*
 * if_ functions
 */

static void
dwc_txstart_locked(struct dwc_softc *sc)
{
	struct ifnet *ifp;
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
		if (sc->txcount == (TX_DESC_COUNT - 1)) {
			if_setdrvflagbits(ifp, IFF_DRV_OACTIVE, 0);
			break;
		}

		m = if_dequeue(ifp);
		if (m == NULL)
			break;
		if (dwc_setup_txbuf(sc, sc->tx_idx_head, &m) != 0) {
			if_sendq_prepend(ifp, m);
			break;
		}
		if_bpfmtap(ifp, m);
		sc->tx_idx_head = next_txidx(sc, sc->tx_idx_head);
		++enqueued;
	}

	if (enqueued != 0) {
		WRITE4(sc, TRANSMIT_POLL_DEMAND, 0x1);
		sc->tx_watchdog_count = WATCHDOG_TIMEOUT_SECS;
	}
}

static void
dwc_txstart(struct ifnet *ifp)
{
	struct dwc_softc *sc = ifp->if_softc;

	DWC_LOCK(sc);
	dwc_txstart_locked(sc);
	DWC_UNLOCK(sc);
}

static void
dwc_init_locked(struct dwc_softc *sc)
{
	struct ifnet *ifp = sc->ifp;

	DWC_ASSERT_LOCKED(sc);

	if (if_getdrvflags(ifp) & IFF_DRV_RUNNING)
		return;

	dwc_setup_rxfilter(sc);
	dwc_setup_core(sc);
	dwc_enable_mac(sc, true);
	dwc_init_dma(sc);

	if_setdrvflagbits(ifp, IFF_DRV_RUNNING, IFF_DRV_OACTIVE);

	/*
	 * Call mii_mediachg() which will call back into dwc_miibus_statchg()
	 * to set up the remaining config registers based on current media.
	 */
	mii_mediachg(sc->mii_softc);
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
	struct ifnet *ifp;

	DWC_ASSERT_LOCKED(sc);

	ifp = sc->ifp;
	if_setdrvflagbits(ifp, 0, IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
	sc->tx_watchdog_count = 0;
	sc->stats_harvest_count = 0;

	callout_stop(&sc->dwc_callout);

	dwc_stop_dma(sc);
	dwc_enable_mac(sc, false);
}

static int
dwc_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct dwc_softc *sc;
	struct mii_data *mii;
	struct ifreq *ifr;
	int flags, mask, error;

	sc = ifp->if_softc;
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
dwc_txfinish_locked(struct dwc_softc *sc)
{
	struct dwc_bufmap *bmap;
	struct dwc_hwdesc *desc;
	struct ifnet *ifp;

	DWC_ASSERT_LOCKED(sc);

	ifp = sc->ifp;
	while (sc->tx_idx_tail != sc->tx_idx_head) {
		desc = &sc->txdesc_ring[sc->tx_idx_tail];
		if ((desc->desc0 & TDESC0_OWN) != 0)
			break;
		bmap = &sc->txbuf_map[sc->tx_idx_tail];
		bus_dmamap_sync(sc->txbuf_tag, bmap->map,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->txbuf_tag, bmap->map);
		m_freem(bmap->mbuf);
		bmap->mbuf = NULL;
		dwc_setup_txdesc(sc, sc->tx_idx_tail, 0, 0, 0);
		sc->tx_idx_tail = next_txidx(sc, sc->tx_idx_tail);
		if_setdrvflagbits(ifp, 0, IFF_DRV_OACTIVE);
		if_inc_counter(ifp, IFCOUNTER_OPACKETS, 1);
	}

	/* If there are no buffers outstanding, muzzle the watchdog. */
	if (sc->tx_idx_tail == sc->tx_idx_head) {
		sc->tx_watchdog_count = 0;
	}
}

static void
dwc_rxfinish_locked(struct dwc_softc *sc)
{
	struct ifnet *ifp;
	struct mbuf *m;
	int error, idx;
	struct dwc_hwdesc *desc;

	DWC_ASSERT_LOCKED(sc);
	ifp = sc->ifp;
	for (;;) {
		idx = sc->rx_idx;
		desc = sc->rxdesc_ring + idx;
		if ((desc->desc0 & RDESC0_OWN) != 0)
			break;

		m = dwc_rxfinish_one(sc, desc, sc->rxbuf_map + idx);
		if (m == NULL) {
			wmb();
			desc->desc0 = RDESC0_OWN;
			wmb();
		} else {
			/* We cannot create hole in RX ring */
			error = dwc_setup_rxbuf(sc, idx, m);
			if (error != 0)
				panic("dwc_setup_rxbuf failed:  error %d\n",
				    error);

		}
		sc->rx_idx = next_rxidx(sc, sc->rx_idx);
	}
}

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
			dwc_rxfinish_locked(sc);

		if (reg & DMA_STATUS_TI) {
			dwc_txfinish_locked(sc);
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
	struct ifnet *ifp;

	/* We don't need to harvest too often. */
	if (++sc->stats_harvest_count < STATS_HARVEST_INTERVAL)
		return;

	sc->stats_harvest_count = 0;
	ifp = sc->ifp;

	if_inc_counter(ifp, IFCOUNTER_IPACKETS, READ4(sc, RXFRAMECOUNT_GB));
	if_inc_counter(ifp, IFCOUNTER_IMCASTS, READ4(sc, RXMULTICASTFRAMES_G));
	if_inc_counter(ifp, IFCOUNTER_IERRORS,
	    READ4(sc, RXOVERSIZE_G) + READ4(sc, RXUNDERSIZE_G) +
	    READ4(sc, RXCRCERROR) + READ4(sc, RXALIGNMENTERROR) +
	    READ4(sc, RXRUNTERROR) + READ4(sc, RXJABBERERROR) +
	    READ4(sc, RXLENGTHERROR));

	if_inc_counter(ifp, IFCOUNTER_OPACKETS, READ4(sc, TXFRAMECOUNT_G));
	if_inc_counter(ifp, IFCOUNTER_OMCASTS, READ4(sc, TXMULTICASTFRAMES_G));
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
	struct ifnet *ifp;
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
			dwc_txfinish_locked(sc);
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

/*
 * Probe/Attach functions
 */

#define	GPIO_ACTIVE_LOW 1

static int
dwc_reset(device_t dev)
{
	pcell_t gpio_prop[4];
	pcell_t delay_prop[3];
	phandle_t node, gpio_node;
	device_t gpio;
	uint32_t pin, flags;
	uint32_t pin_value;

	node = ofw_bus_get_node(dev);
	if (OF_getencprop(node, "snps,reset-gpio",
	    gpio_prop, sizeof(gpio_prop)) <= 0)
		return (0);

	if (OF_getencprop(node, "snps,reset-delays-us",
	    delay_prop, sizeof(delay_prop)) <= 0) {
		device_printf(dev,
		    "Wrong property for snps,reset-delays-us");
		return (ENXIO);
	}

	gpio_node = OF_node_from_xref(gpio_prop[0]);
	if ((gpio = OF_device_from_xref(gpio_prop[0])) == NULL) {
		device_printf(dev,
		    "Can't find gpio controller for phy reset\n");
		return (ENXIO);
	}

	if (GPIO_MAP_GPIOS(gpio, node, gpio_node,
	    nitems(gpio_prop) - 1,
	    gpio_prop + 1, &pin, &flags) != 0) {
		device_printf(dev, "Can't map gpio for phy reset\n");
		return (ENXIO);
	}

	pin_value = GPIO_PIN_LOW;
	if (OF_hasprop(node, "snps,reset-active-low"))
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

#ifdef EXT_RESOURCES
static int
dwc_clock_init(device_t dev)
{
	hwreset_t rst;
	clk_t clk;
	int error;
	int64_t freq;

	/* Enable clocks */
	if (clk_get_by_ofw_name(dev, 0, "stmmaceth", &clk) == 0) {
		error = clk_enable(clk);
		if (error != 0) {
			device_printf(dev, "could not enable main clock\n");
			return (error);
		}
		if (bootverbose) {
			clk_get_freq(clk, &freq);
			device_printf(dev, "MAC clock(%s) freq: %jd\n",
					clk_get_name(clk), (intmax_t)freq);
		}
	}
	else {
		device_printf(dev, "could not find clock stmmaceth\n");
	}

	/* De-assert reset */
	if (hwreset_get_by_ofw_name(dev, 0, "stmmaceth", &rst) == 0) {
		error = hwreset_deassert(rst);
		if (error != 0) {
			device_printf(dev, "could not de-assert reset\n");
			return (error);
		}
	}

	return (0);
}
#endif

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
	struct ifnet *ifp;
	int error, i;
	uint32_t reg;
	char *phy_mode;
	phandle_t node;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->rx_idx = 0;
	sc->txcount = TX_DESC_COUNT;
	sc->mii_clk = IF_DWC_MII_CLK(dev);
	sc->mactype = IF_DWC_MAC_TYPE(dev);

	node = ofw_bus_get_node(dev);
	if (OF_getprop_alloc(node, "phy-mode", (void **)&phy_mode)) {
		if (strcmp(phy_mode, "rgmii") == 0)
			sc->phy_mode = PHY_MODE_RGMII;
		if (strcmp(phy_mode, "rmii") == 0)
			sc->phy_mode = PHY_MODE_RMII;
		OF_prop_free(phy_mode);
	}

	if (IF_DWC_INIT(dev) != 0)
		return (ENXIO);

#ifdef EXT_RESOURCES
	if (dwc_clock_init(dev) != 0)
		return (ENXIO);
#endif

	if (bus_alloc_resources(dev, dwc_spec, sc->res)) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	/* Read MAC before reset */
	dwc_get_hwaddr(sc, macaddr);

	/* Reset the PHY if needed */
	if (dwc_reset(dev) != 0) {
		device_printf(dev, "Can't reset the PHY\n");
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
		return (ENXIO);
	}

	if (sc->mactype != DWC_GMAC_EXT_DESC) {
		reg = BUS_MODE_FIXEDBURST;
		reg |= (BUS_MODE_PRIORXTX_41 << BUS_MODE_PRIORXTX_SHIFT);
	} else
		reg = (BUS_MODE_EIGHTXPBL);
	reg |= (BUS_MODE_PBL_BEATS_8 << BUS_MODE_PBL_SHIFT);
	WRITE4(sc, BUS_MODE, reg);

	/*
	 * DMA must be stop while changing descriptor list addresses.
	 */
	reg = READ4(sc, OPERATION_MODE);
	reg &= ~(MODE_ST | MODE_SR);
	WRITE4(sc, OPERATION_MODE, reg);

	if (setup_dma(sc))
	        return (ENXIO);

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
		return (ENXIO);
	}

	/* Set up the ethernet interface. */
	sc->ifp = ifp = if_alloc(IFT_ETHER);

	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	if_setflags(sc->ifp, IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST);
	if_setstartfn(ifp, dwc_txstart);
	if_setioctlfn(ifp, dwc_ioctl);
	if_setinitfn(ifp, dwc_init);
	if_setsendqlen(ifp, TX_DESC_COUNT - 1);
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
		return (ENXIO);
	}
	sc->mii_softc = device_get_softc(sc->miibus);

	/* All ready to run, attach the ethernet interface. */
	ether_ifattach(ifp, macaddr);
	sc->is_attached = true;

	return (0);
}

static device_method_t dwc_methods[] = {
	DEVMETHOD(device_probe,		dwc_probe),
	DEVMETHOD(device_attach,	dwc_attach),

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

static devclass_t dwc_devclass;

DRIVER_MODULE(dwc, simplebus, dwc_driver, dwc_devclass, 0, 0);
DRIVER_MODULE(miibus, dwc, miibus_driver, miibus_devclass, 0, 0);

MODULE_DEPEND(dwc, ether, 1, 1, 1);
MODULE_DEPEND(dwc, miibus, 1, 1, 1);
