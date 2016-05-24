/*-
 * Copyright (c) 2009-2010 Weongyo Jeong <weongyo@freebsd.org>
 * Copyright (c) 2016 Adrian Chadd <adrian@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_bwn.h"
#include "opt_wlan.h"

/*
 * The Broadcom Wireless LAN controller driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/endian.h>
#include <sys/errno.h>
#include <sys/firmware.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_llc.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/siba/siba_ids.h>
#include <dev/siba/sibareg.h>
#include <dev/siba/sibavar.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>
#include <net80211/ieee80211_regdomain.h>
#include <net80211/ieee80211_phy.h>
#include <net80211/ieee80211_ratectl.h>

#include <dev/bwn/if_bwnreg.h>
#include <dev/bwn/if_bwnvar.h>

#include <dev/bwn/if_bwn_chipid.h>
#include <dev/bwn/if_bwn_debug.h>
#include <dev/bwn/if_bwn_misc.h>
#include <dev/bwn/if_bwn_phy_common.h>

void
bwn_mac_switch_freq(struct bwn_mac *mac, int spurmode)
{
	struct bwn_softc *sc = mac->mac_sc;
	uint16_t chip_id = siba_get_chipid(sc->sc_dev);

	if (chip_id == BCMA_CHIP_ID_BCM4331) {
		switch (spurmode) {
		case 2: /* 168 Mhz: 2^26/168 = 0x61862 */
			BWN_WRITE_2(mac, BWN_TSF_CLK_FRAC_LOW, 0x1862);
			BWN_WRITE_2(mac, BWN_TSF_CLK_FRAC_HIGH, 0x6);
			break;
		case 1: /* 164 Mhz: 2^26/164 = 0x63e70 */
			BWN_WRITE_2(mac, BWN_TSF_CLK_FRAC_LOW, 0x3e70);
			BWN_WRITE_2(mac, BWN_TSF_CLK_FRAC_HIGH, 0x6);
			break;
		default: /* 160 Mhz: 2^26/160 = 0x66666 */
			BWN_WRITE_2(mac, BWN_TSF_CLK_FRAC_LOW, 0x6666);
			BWN_WRITE_2(mac, BWN_TSF_CLK_FRAC_HIGH, 0x6);
			break;
		}
	} else if (chip_id == BCMA_CHIP_ID_BCM43131 ||
	    chip_id == BCMA_CHIP_ID_BCM43217 ||
	    chip_id == BCMA_CHIP_ID_BCM43222 ||
	    chip_id == BCMA_CHIP_ID_BCM43224 ||
	    chip_id == BCMA_CHIP_ID_BCM43225 ||
	    chip_id == BCMA_CHIP_ID_BCM43227 ||
	    chip_id == BCMA_CHIP_ID_BCM43228) {
		switch (spurmode) {
		case 2: /* 126 Mhz */
			BWN_WRITE_2(mac, BWN_TSF_CLK_FRAC_LOW, 0x2082);
			BWN_WRITE_2(mac, BWN_TSF_CLK_FRAC_HIGH, 0x8);
			break;
		case 1: /* 123 Mhz */
			BWN_WRITE_2(mac, BWN_TSF_CLK_FRAC_LOW, 0x5341);
			BWN_WRITE_2(mac, BWN_TSF_CLK_FRAC_HIGH, 0x8);
			break;
		default: /* 120 Mhz */
			BWN_WRITE_2(mac, BWN_TSF_CLK_FRAC_LOW, 0x8889);
			BWN_WRITE_2(mac, BWN_TSF_CLK_FRAC_HIGH, 0x8);
			break;
		}
	} else if (mac->mac_phy.type == BWN_PHYTYPE_LCN) {
		switch (spurmode) {
		case 1: /* 82 Mhz */
			BWN_WRITE_2(mac, BWN_TSF_CLK_FRAC_LOW, 0x7CE0);
			BWN_WRITE_2(mac, BWN_TSF_CLK_FRAC_HIGH, 0xC);
			break;
		default: /* 80 Mhz */
			BWN_WRITE_2(mac, BWN_TSF_CLK_FRAC_LOW, 0xCCCD);
			BWN_WRITE_2(mac, BWN_TSF_CLK_FRAC_HIGH, 0xC);
			break;
		}
	}
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/BmacPhyClkFgc */
void
bwn_phy_force_clock(struct bwn_mac *mac, int force)
{
	struct bwn_softc *sc = mac->mac_sc;
	uint32_t tmp;

	/* XXX Only for N, HT and AC PHYs */

	/* XXX bhnd bus */
	if (bwn_is_bus_siba(mac)) {
			tmp = siba_read_4(sc->sc_dev, SIBA_TGSLOW);
		if (force)
			tmp |= SIBA_TGSLOW_FGC;
		else
			tmp &= ~SIBA_TGSLOW_FGC;
		siba_write_4(sc->sc_dev, SIBA_TGSLOW, tmp);
	} else {
		BWN_ERRPRINTF(sc, "%s: unknown bus!\n", __func__);
	}
}

int
bwn_radio_wait_value(struct bwn_mac *mac, uint16_t offset, uint16_t mask,
    uint16_t value, int delay, int timeout)
{
	uint16_t val;
	int i;

	for (i = 0; i < timeout; i += delay) {
		val = BWN_RF_READ(mac, offset);
		if ((val & mask) == value)
			return (1);
		DELAY(delay);
	}
	return (0);
}

void
bwn_mac_phy_clock_set(struct bwn_mac *mac, int enabled)
{
	struct bwn_softc *sc = mac->mac_sc;
	uint32_t val;

	/* XXX bhnd bus */
	if (bwn_is_bus_siba(mac)) {
		val = siba_read_4(sc->sc_dev, SIBA_TGSLOW);
		if (enabled)
			    val |= BWN_TGSLOW_MACPHYCLKEN;
		else
			    val &= ~BWN_TGSLOW_MACPHYCLKEN;
		siba_write_4(sc->sc_dev, SIBA_TGSLOW, val);
	} else {
		BWN_ERRPRINTF(sc, "%s: unknown bus!\n", __func__);
	}
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/BmacCorePllReset */
void
bwn_wireless_core_phy_pll_reset(struct bwn_mac *mac)
{
	struct bwn_softc *sc = mac->mac_sc;

	/* XXX bhnd bus */
	if (bwn_is_bus_siba(mac)) {
		siba_cc_write32(sc->sc_dev, SIBA_CC_CHIPCTL_ADDR, 0);
		siba_cc_mask32(sc->sc_dev, SIBA_CC_CHIPCTL_DATA, ~0x4);
		siba_cc_set32(sc->sc_dev, SIBA_CC_CHIPCTL_DATA, 0x4);
		siba_cc_mask32(sc->sc_dev, SIBA_CC_CHIPCTL_DATA, ~0x4);
	} else {
		BWN_ERRPRINTF(sc, "%s: unknown bus!\n", __func__);
	}
}
