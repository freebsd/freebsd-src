/*-
 * Copyright (c) 2026 Ruslan Bukin <br@bsdpad.com>
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

#include <net/if.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <machine/bus.h>

#include <dev/clk/clk.h>
#include <dev/hwreset/hwreset.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/mii/mii_fdt.h>

#include <dev/dwc/if_dwcvar.h>
#include <dev/dwc/dwc4_reg.h>
#include <dev/dwc/dwc4_dma.h>

#include "if_dwc_if.h"

struct dwc_hash_maddr_ctx {
	struct dwc_softc *sc;
	uint32_t hash[8];
};

#define	STATS_HARVEST_INTERVAL	2

/* Pause time field in the transmitted control frame */
static int dwc_pause_time = 0xffff;
TUNABLE_INT("hw.dwc.pause_time", &dwc_pause_time);

/*
 * MIIBUS functions
 */

int
dwc4_miibus_read_reg(device_t dev, int phy, int reg)
{
	struct dwc_softc *sc;
	uint32_t mii;
	size_t cnt;
	int rv = 0;

	sc = device_get_softc(dev);

	mii = ((phy << MACMDIOAR_PA_S) & MACMDIOAR_PA_M)
	    | ((reg << MACMDIOAR_RDA_S) & MACMDIOAR_RDA_M)
	    | (sc->mii_clk << MACMDIOAR_CR_S)
	    | MACMDIOAR_READ
	    | MACMDIOAR_GB; /* Busy flag */

	WRITE4(sc, ETH_MACMDIOAR, mii);

	for (cnt = 0; cnt < 1000; cnt++) {
		if (!(READ4(sc, ETH_MACMDIOAR) & MACMDIOAR_GB)) {
			rv = READ4(sc, ETH_MACMDIODR);
			break;
		}
		DELAY(10);
	}

	return rv;
}

int
dwc4_miibus_write_reg(device_t dev, int phy, int reg, int val)
{
	struct dwc_softc *sc;
	uint32_t mii;
	size_t cnt;

	sc = device_get_softc(dev);

	mii = ((phy << MACMDIOAR_PA_S) & MACMDIOAR_PA_M)
	    | ((reg << MACMDIOAR_RDA_S) & MACMDIOAR_RDA_M)
	    | (sc->mii_clk << MACMDIOAR_CR_S)
	    | MACMDIOAR_WRITE
	    | MACMDIOAR_GB; /* Busy flag */

	WRITE4(sc, ETH_MACMDIODR, val);
	WRITE4(sc, ETH_MACMDIOAR, mii);

	for (cnt = 0; cnt < 1000; cnt++) {
		if (!(READ4(sc, ETH_MACMDIOAR) & MACMDIOAR_GB)) {
			break;
                }
		DELAY(10);
	}

	return (0);
}

void
dwc4_miibus_statchg(device_t dev)
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

	reg = READ4(sc, ETH_MACCR);
	switch (IFM_SUBTYPE(mii->mii_media_active)) {
	case IFM_1000_T:
	case IFM_1000_SX:
		reg &= ~(MACCR_FES | MACCR_PS);
		break;
	case IFM_100_TX:
		reg |= (MACCR_FES | MACCR_PS);
		break;
	case IFM_10_T:
		reg &= ~(MACCR_FES);
		reg |= (MACCR_PS);
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
		reg |= (MACCR_DM);
	else
		reg &= ~(MACCR_DM);
	WRITE4(sc, ETH_MACCR, reg);

	IF_DWC_SET_SPEED(dev, IFM_SUBTYPE(mii->mii_media_active));

}

void
dwc4_core_setup(struct dwc_softc *sc)
{
	uint32_t reg;

	DWC_ASSERT_LOCKED(sc);

	reg = READ4(sc, ETH_MACVR);
	device_printf(sc->dev, "MAC Version: %x\n", reg);

	/* Enable core */
	reg = READ4(sc, ETH_MACCR);
	reg |= (MACCR_JD | MACCR_ACS | MACCR_BE);
	WRITE4(sc, ETH_MACCR, reg);
}

void
dwc4_enable_mac(struct dwc_softc *sc, bool enable)
{
	uint32_t reg;

	DWC_ASSERT_LOCKED(sc);

	reg = READ4(sc, ETH_MACCR);
	if (enable)
		reg |= MACCR_TE | MACCR_RE;
	else
		reg &= ~(MACCR_TE | MACCR_RE);
	WRITE4(sc, ETH_MACCR, reg);
}

void
dwc4_enable_csum_offload(struct dwc_softc *sc)
{
	uint32_t reg;

	DWC_ASSERT_LOCKED(sc);
	reg = READ4(sc, ETH_MACCR);
	if ((if_getcapenable(sc->ifp) & (IFCAP_RXCSUM | IFCAP_RXCSUM_IPV6)) !=
	    0)
		reg |= MACCR_IPC;
	else
		reg &= ~MACCR_IPC;
	WRITE4(sc, ETH_MACCR, reg);
}

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
	/*
	 * TODO: There is probably a HW_FEATURES bit which isn't
	 * related to the extended descriptors that describe this
	 */
	if (!ctx->sc->dma_ext_desc)
		val >>= 2; /* Only need lower 6 bits */
	hashreg = (val >> 5);
	hashbit = (val & 31);
	ctx->hash[hashreg] |= (1 << hashbit);

	return (1);
}

void
dwc4_setup_rxfilter(struct dwc_softc *sc)
{
	struct dwc_hash_maddr_ctx ctx;
	if_t ifp;
	uint8_t *eaddr;
	uint32_t ffval, hi, lo;
	int nhash, i;

	DWC_ASSERT_LOCKED(sc);

	ifp = sc->ifp;
	/*
	 * TODO: There is probably a HW_FEATURES bit which isn't
	 * related to the extended descriptors that describe this
	 */
	nhash = sc->dma_ext_desc == false ? 2 : 8;

	/*
	 * Set the multicast (group) filter hash.
	 */
	if ((if_getflags(ifp) & IFF_ALLMULTI) != 0) {
		ffval = (MACPFR_PM);
		for (i = 0; i < nhash; i++)
			ctx.hash[i] = ~0;
	} else {
		ffval = (MACPFR_HMC);
		for (i = 0; i < nhash; i++)
			ctx.hash[i] = 0;
		ctx.sc = sc;
		if_foreach_llmaddr(ifp, dwc_hash_maddr, &ctx);
	}

	/*
	 * Set the individual address filter hash.
	 */
	if ((if_getflags(ifp) & IFF_PROMISC) != 0)
		ffval |= MACPFR_PR;

	/*
	 * Set the primary address.
	 */
	eaddr = if_getlladdr(ifp);
	lo = eaddr[0] | (eaddr[1] << 8) | (eaddr[2] << 16) |
	    (eaddr[3] << 24);
	hi = eaddr[4] | (eaddr[5] << 8);
	WRITE4(sc, ETH_MACA0LR, lo);
	WRITE4(sc, ETH_MACA0HR, hi);

	WRITE4(sc, ETH_MACPFR, ffval);
	if (!sc->dma_ext_desc) {
		WRITE4(sc, ETH_MACHT0R, ctx.hash[0]);
		WRITE4(sc, ETH_MACHT1R, ctx.hash[1]);
	} else
		panic("implement me: ext_desc");
}

void
dwc4_get_hwaddr(struct dwc_softc *sc, uint8_t *hwaddr)
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
	lo = READ4(sc, ETH_MACA0LR);
	hi = READ4(sc, ETH_MACA0HR) & 0xffff;
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
 * Stats
 */

static void
dwc4_clear_stats(struct dwc_softc *sc)
{
	uint32_t reg;

	reg = READ4(sc, ETH_MMC_CONTROL);
	reg |= MMC_CONTROL_CNTRST;
	WRITE4(sc, ETH_MMC_CONTROL, reg);
}

void
dwc4_harvest_stats(struct dwc_softc *sc)
{
	if_t ifp __unused;
	int val;

	/* We don't need to harvest too often. */
	if (++sc->stats_harvest_count < STATS_HARVEST_INTERVAL)
		return;

	sc->stats_harvest_count = 0;
	ifp = sc->ifp;

	val = READ4(sc, ETH_RX_RECEIVE_ERROR);
	if_inc_counter(ifp, IFCOUNTER_IERRORS, val);

	val = READ4(sc, ETH_TX_PACKET_COUNT_GOOD_BAD);
	val -= READ4(sc, ETH_TX_PACKET_COUNT_GOOD);
	if_inc_counter(ifp, IFCOUNTER_OERRORS, val);

	val = READ4(sc, ETH_TX_SINGLE_COLLISION_GOOD_PACKETS);
	val += READ4(sc, ETH_TX_MULTIPLE_COLLISION_GOOD_PACKETS);
	val += READ4(sc, ETH_TX_LATE_COLLISION_PACKETS);
	val += READ4(sc, ETH_TX_EXCESSIVE_COLLISION_PACKETS);
	if_inc_counter(ifp, IFCOUNTER_COLLISIONS, val);

	dwc4_clear_stats(sc);
}

void
dwc4_intr(struct dwc_softc *sc)
{
	uint32_t reg;

	DWC_ASSERT_LOCKED(sc);

	reg = READ4(sc, ETH_MACISR);
	if (reg)
		READ4(sc, ETH_MACPHYCSR);
}

void
dwc4_intr_disable(struct dwc_softc *sc)
{

	WRITE4(sc, ETH_MACIER, 0);
}
