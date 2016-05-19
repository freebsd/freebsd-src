/*

  Broadcom B43 wireless driver
  IEEE 802.11n PHY data tables

  Copyright (c) 2008 Michael Buesch <m@bues.ch>
  Copyright (c) 2010 Rafał Miłecki <zajec5@gmail.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
  Boston, MA 02110-1301, USA.

*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * The Broadcom Wireless LAN controller driver.
 */
#include "opt_wlan.h"
#include "opt_bwn.h"

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
#include <dev/bwn/if_bwn_debug.h>
#include <dev/bwn/if_bwn_util.h>
#include <dev/bwn/if_bwn_phy_common.h>

#include <gnu/dev/bwn/phy_n/if_bwn_phy_n_regs.h>
#include <gnu/dev/bwn/phy_n/if_bwn_phy_n_ppr.h>

#define ppr_for_each_entry(ppr, i, entry)				\
	for (i = 0, entry = &(ppr)->__all_rates[i];			\
	     i < BWN_PPR_RATES_NUM;					\
	     i++, entry++)

void bwn_ppr_clear(struct bwn_mac *mac, struct bwn_ppr *ppr)
{
	memset(ppr, 0, sizeof(*ppr));

	/* Compile-time PPR check */
	CTASSERT(sizeof(struct bwn_ppr) == BWN_PPR_RATES_NUM * sizeof(uint8_t));
}

void bwn_ppr_add(struct bwn_mac *mac, struct bwn_ppr *ppr, int diff)
{
	int i;
	uint8_t *rate;

	ppr_for_each_entry(ppr, i, rate) {
		*rate = bwn_clamp_val(*rate + diff, 0, 127);
	}
}

void bwn_ppr_apply_max(struct bwn_mac *mac, struct bwn_ppr *ppr, uint8_t max)
{
	int i;
	uint8_t *rate;

	ppr_for_each_entry(ppr, i, rate) {
		*rate = min(*rate, max);
	}
}

void bwn_ppr_apply_min(struct bwn_mac *mac, struct bwn_ppr *ppr, uint8_t min)
{
	int i;
	uint8_t *rate;

	ppr_for_each_entry(ppr, i, rate) {
		*rate = max(*rate, min);
	}
}

uint8_t bwn_ppr_get_max(struct bwn_mac *mac, struct bwn_ppr *ppr)
{
	uint8_t res = 0;
	int i;
	uint8_t *rate;

	ppr_for_each_entry(ppr, i, rate) {
		res = max(*rate, res);
	}

	return res;
}

bool bwn_ppr_load_max_from_sprom(struct bwn_mac *mac, struct bwn_ppr *ppr,
				 bwn_phy_band_t band)
{
	struct bwn_softc *sc = mac->mac_sc;
	struct siba_sprom_core_pwr_info core_pwr_info[4];
	struct bwn_ppr_rates *rates = &ppr->rates;
	struct bwn_phy *phy = &mac->mac_phy;
	uint8_t maxpwr, off;
	uint32_t sprom_ofdm_po;
	uint16_t sprom_mcs_po[8];
	uint8_t extra_cdd_po, extra_stbc_po;
	int i;

	for (i = 0; i < 4; i++) {
		bzero(&core_pwr_info[i], sizeof(core_pwr_info[i]));
		if (siba_sprom_get_core_power_info(sc->sc_dev, i,
		    &core_pwr_info[i]) != 0) {
			BWN_ERRPRINTF(mac->mac_sc,
			    "%s: failed to get core_pwr_info for core %d\n",
			    __func__,
			    i);
		}
	}

	switch (band) {
	case BWN_PHY_BAND_2G:
		maxpwr = min(core_pwr_info[0].maxpwr_2g,
			     core_pwr_info[1].maxpwr_2g);
		sprom_ofdm_po = siba_sprom_get_ofdm2gpo(sc->sc_dev);
		siba_sprom_get_mcs2gpo(sc->sc_dev, sprom_mcs_po);
		extra_cdd_po = (siba_sprom_get_cddpo(sc->sc_dev) >> 0) & 0xf;
		extra_stbc_po = (siba_sprom_get_stbcpo(sc->sc_dev) >> 0) & 0xf;
		break;
	case BWN_PHY_BAND_5G_LO:
		maxpwr = min(core_pwr_info[0].maxpwr_5gl,
			     core_pwr_info[1].maxpwr_5gl);
		sprom_ofdm_po = siba_sprom_get_ofdm5glpo(sc->sc_dev);
		siba_sprom_get_mcs5glpo(sc->sc_dev, sprom_mcs_po);
		extra_cdd_po = (siba_sprom_get_cddpo(sc->sc_dev) >> 8) & 0xf;
		extra_stbc_po = (siba_sprom_get_stbcpo(sc->sc_dev) >> 8) & 0xf;
		break;
	case BWN_PHY_BAND_5G_MI:
		maxpwr = min(core_pwr_info[0].maxpwr_5g,
			     core_pwr_info[1].maxpwr_5g);
		sprom_ofdm_po = siba_sprom_get_ofdm5gpo(sc->sc_dev);
		siba_sprom_get_mcs5gpo(sc->sc_dev, sprom_mcs_po);
		extra_cdd_po = (siba_sprom_get_cddpo(sc->sc_dev) >> 4) & 0xf;
		extra_stbc_po = (siba_sprom_get_stbcpo(sc->sc_dev) >> 4) & 0xf;
		break;
	case BWN_PHY_BAND_5G_HI:
		maxpwr = min(core_pwr_info[0].maxpwr_5gh,
			     core_pwr_info[1].maxpwr_5gh);
		sprom_ofdm_po = siba_sprom_get_ofdm5ghpo(sc->sc_dev);
		siba_sprom_get_mcs5ghpo(sc->sc_dev, sprom_mcs_po);
		extra_cdd_po = (siba_sprom_get_cddpo(sc->sc_dev) >> 12) & 0xf;
		extra_stbc_po = (siba_sprom_get_stbcpo(sc->sc_dev) >> 12) & 0xf;
		break;
	default:
		device_printf(mac->mac_sc->sc_dev, "%s: invalid band (%d)\n",
		    __func__,
		    band);
		return false;
	}

	if (band == BWN_BAND_2G) {
		for (i = 0; i < 4; i++) {
			off = ((siba_sprom_get_cck2gpo(sc->sc_dev) >> (i * 4)) & 0xf) * 2;
			rates->cck[i] = maxpwr - off;
		}
	}

	/* OFDM */
	for (i = 0; i < 8; i++) {
		off = ((sprom_ofdm_po >> (i * 4)) & 0xf) * 2;
		rates->ofdm[i] = maxpwr - off;
	}

	/* MCS 20 SISO */
	rates->mcs_20[0] = rates->ofdm[0];
	rates->mcs_20[1] = rates->ofdm[2];
	rates->mcs_20[2] = rates->ofdm[3];
	rates->mcs_20[3] = rates->ofdm[4];
	rates->mcs_20[4] = rates->ofdm[5];
	rates->mcs_20[5] = rates->ofdm[6];
	rates->mcs_20[6] = rates->ofdm[7];
	rates->mcs_20[7] = rates->ofdm[7];

	/* MCS 20 CDD */
	for (i = 0; i < 4; i++) {
		off = ((sprom_mcs_po[0] >> (i * 4)) & 0xf) * 2;
		rates->mcs_20_cdd[i] = maxpwr - off;
		if (phy->type == BWN_PHYTYPE_N && phy->rev >= 3)
			rates->mcs_20_cdd[i] -= extra_cdd_po;
	}
	for (i = 0; i < 4; i++) {
		off = ((sprom_mcs_po[1] >> (i * 4)) & 0xf) * 2;
		rates->mcs_20_cdd[4 + i] = maxpwr - off;
		if (phy->type == BWN_PHYTYPE_N && phy->rev >= 3)
			rates->mcs_20_cdd[4 + i] -= extra_cdd_po;
	}

	/* OFDM 20 CDD */
	rates->ofdm_20_cdd[0] = rates->mcs_20_cdd[0];
	rates->ofdm_20_cdd[1] = rates->mcs_20_cdd[0];
	rates->ofdm_20_cdd[2] = rates->mcs_20_cdd[1];
	rates->ofdm_20_cdd[3] = rates->mcs_20_cdd[2];
	rates->ofdm_20_cdd[4] = rates->mcs_20_cdd[3];
	rates->ofdm_20_cdd[5] = rates->mcs_20_cdd[4];
	rates->ofdm_20_cdd[6] = rates->mcs_20_cdd[5];
	rates->ofdm_20_cdd[7] = rates->mcs_20_cdd[6];

	/* MCS 20 STBC */
	for (i = 0; i < 4; i++) {
		off = ((sprom_mcs_po[0] >> (i * 4)) & 0xf) * 2;
		rates->mcs_20_stbc[i] = maxpwr - off;
		if (phy->type == BWN_PHYTYPE_N && phy->rev >= 3)
			rates->mcs_20_stbc[i] -= extra_stbc_po;
	}
	for (i = 0; i < 4; i++) {
		off = ((sprom_mcs_po[1] >> (i * 4)) & 0xf) * 2;
		rates->mcs_20_stbc[4 + i] = maxpwr - off;
		if (phy->type == BWN_PHYTYPE_N && phy->rev >= 3)
			rates->mcs_20_stbc[4 + i] -= extra_stbc_po;
	}

	/* MCS 20 SDM */
	for (i = 0; i < 4; i++) {
		off = ((sprom_mcs_po[2] >> (i * 4)) & 0xf) * 2;
		rates->mcs_20_sdm[i] = maxpwr - off;
	}
	for (i = 0; i < 4; i++) {
		off = ((sprom_mcs_po[3] >> (i * 4)) & 0xf) * 2;
		rates->mcs_20_sdm[4 + i] = maxpwr - off;
	}

	return true;
}
