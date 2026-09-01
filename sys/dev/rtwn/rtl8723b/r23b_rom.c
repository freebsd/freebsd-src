/*
 * Copyright (c) 2026 Ahmad Khalifa <vexeduxr@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/linker.h>

#include <net/if.h>
#include <net/ethernet.h>
#include <net/if_media.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/rtwn/if_rtwnreg.h>
#include <dev/rtwn/if_rtwnvar.h>

#include <dev/rtwn/rtl8192c/r92c_reg.h>
#include <dev/rtwn/rtl8192c/r92c_rom_defs.h>

#include <dev/rtwn/rtl8192e/r92e_var.h>

#include <dev/rtwn/rtl8723b/r23b.h>
#include <dev/rtwn/rtl8723b/r23b_reg.h>
#include <dev/rtwn/rtl8723b/r23b_var.h>
#include <dev/rtwn/rtl8723b/r23b_rom_image.h>

#include <dev/rtwn/rtl8812a/r12a_rom_image.h>

int
r23b_efuse_preread(struct rtwn_softc *sc)
{
	int error;

	/* Switch to the WIFI bank. */
	error = rtwn_setbits_4(sc, R92C_EFUSE_TEST, R23B_EFUSE_TEST_SEL_M,
	    SM(R23B_EFUSE_TEST_SEL, R23B_EFUSE_TEST_SEL_WIFI));
	if (error != 0)
		return (error);

	/* NB: Vendor driver does this on every read. */
	error = rtwn_setbits_2(sc, R92C_EFUSE_TEST, R23B_EFUSE_TEST_PGMEN, 0);
	return (error);
}

void
r23b_parse_rom(struct rtwn_softc *sc, uint8_t *buf)
{
	struct r23b_rom *rom = (struct r23b_rom *)buf;
	struct r23b_softc *rs = sc->sc_priv;

	for (int i = 0; i < sc->ntxchains; i++) {
		struct r23b_tx_pwr_2g *pwr_2g = &rom->tx_pwr[i].pwr_2g;
		struct r23b_tx_pwr_diff_2g *pwr_diff_2g =
		    &rom->tx_pwr[i].pwr_diff_2g;
		int j;

		for (j = 0; j < R23B_GROUP_2G - 1; j++) {
			rs->super.cck_tx_pwr[i][j] = RTWN_GET_ROM_VAR(
			    pwr_2g->cck[j], R23B_DEF_TX_PWR_2G);
			rs->super.ht40_tx_pwr_2g[i][j] =
			    RTWN_GET_ROM_VAR(pwr_2g->ht40[j],
			    R23B_DEF_TX_PWR_2G);
		}
		rs->super.cck_tx_pwr[i][j] = RTWN_GET_ROM_VAR(pwr_2g->cck[j],
		    R23B_DEF_TX_PWR_2G);

		rs->super.cck_tx_pwr_diff_2g[i][0] = 0;
		rs->super.bw40_tx_pwr_diff_2g[i][0] = 0;
		if (pwr_diff_2g->ht20_ofdm == 0xff) {
			rs->super.ofdm_tx_pwr_diff_2g[i][0] =
			    R23B_DEF_TX_OFDM_DIFF;
			rs->super.bw20_tx_pwr_diff_2g[i][0] =
			    R23B_DEF_TX_HT20_DIFF;
		} else {
			rs->super.ofdm_tx_pwr_diff_2g[i][0] = RTWN_SIGN4TO8(
			    MS(pwr_diff_2g->ht20_ofdm, LOW_PART));
			rs->super.bw20_tx_pwr_diff_2g[i][0] = RTWN_SIGN4TO8(
			    MS(pwr_diff_2g->ht20_ofdm, HIGH_PART));
		}

		for (j = 1; j < R23B_MAX_TX_COUNT; j++) {
			struct r23b_tx_pwr_diff123_2g *diff =
			    &pwr_diff_2g->diff123[j - 1];

			if (diff->ofdm_cck == 0xff) {
				rs->super.ofdm_tx_pwr_diff_2g[i][j] =
				rs->super.cck_tx_pwr_diff_2g[i][j] =
				    R23B_DEF_TX_DIFF;
			} else {
				rs->super.cck_tx_pwr_diff_2g[i][j] =
				    RTWN_SIGN4TO8(MS(diff->ofdm_cck, LOW_PART));
				rs->super.ofdm_tx_pwr_diff_2g[i][j] =
				    RTWN_SIGN4TO8(
				    MS(diff->ofdm_cck, HIGH_PART));
			}

			if (diff->ht40_ht20 == 0xff) {
				rs->super.bw40_tx_pwr_diff_2g[i][j] =
				rs->super.bw20_tx_pwr_diff_2g[i][j] =
				    R23B_DEF_TX_DIFF;
			} else {
				rs->super.bw40_tx_pwr_diff_2g[i][j] =
				    RTWN_SIGN4TO8(
				    MS(diff->ht40_ht20, HIGH_PART));
				rs->super.bw20_tx_pwr_diff_2g[i][j] =
				    RTWN_SIGN4TO8(
				    MS(diff->ht40_ht20, LOW_PART));
			}
		}

		rs->bt_antnum = rom->rf_bt_opt & R12A_RF_BT_OPT_ANT_NUM;
	}

	rs->super.crystalcap = RTWN_GET_ROM_VAR(rom->crystal_cap,
	    R23B_ROM_CRYSTALCAP_DEF);

	sc->thermal_meter = RTWN_GET_ROM_VAR(rom->thermal_meter,
	    R23B_ROM_THERMALMETER_DEF);
}
