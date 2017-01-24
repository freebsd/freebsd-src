/*-
 * Copyright (c) 2017 Kevin Lo <kevlo@FreeBSD.org>
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

#include "opt_wlan.h"

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

#include <dev/rtwn/if_rtwn_debug.h>
#include <dev/rtwn/if_rtwn_ridx.h>
#include <dev/rtwn/if_rtwn_rx.h>

#include <dev/rtwn/rtl8192e/r92e.h>
#include <dev/rtwn/rtl8192e/r92e_reg.h>
#include <dev/rtwn/rtl8192e/r92e_var.h>

static int
r92e_get_power_group(struct rtwn_softc *sc, struct ieee80211_channel *c)
{
	uint8_t chan;
	int group;

	chan = rtwn_chan2centieee(c);
	if (IEEE80211_IS_CHAN_2GHZ(c)) {
		if (chan <= 2)			group = 0;
		else if (chan <= 5)		group = 1;
		else if (chan <= 8)		group = 2;
		else if (chan <= 11)		group = 3;
		else if (chan <= 14)		group = 4;
		else {
			KASSERT(0, ("wrong 2GHz channel %d!\n", chan));
			return (-1);
		}
	} else {
		KASSERT(0, ("wrong channel band (flags %08X)\n", c->ic_flags));
		return (-1);
	}

	return (group);
}

static void
r92e_get_txpower(struct rtwn_softc *sc, int chain, struct ieee80211_channel *c,
    uint8_t power[RTWN_RIDX_COUNT])
{
	struct r92e_softc *rs = sc->sc_priv;
	int i, ridx, group, max_mcs;

	/* Determine channel group. */
	group = r92e_get_power_group(sc, c);
	if (group == -1) {	/* shouldn't happen */
		device_printf(sc->sc_dev, "%s: incorrect channel\n", __func__);
		return;
	}

	max_mcs = RTWN_RIDX_MCS(sc->ntxchains * 8 - 1);

	/* XXX regulatory */
	/* XXX net80211 regulatory */

	for (ridx = RTWN_RIDX_CCK1; ridx <= RTWN_RIDX_CCK11; ridx++)
		power[ridx] = rs->cck_tx_pwr[chain][group];
	for (ridx = RTWN_RIDX_OFDM6; ridx <= max_mcs; ridx++)
		power[ridx] = rs->ht40_tx_pwr_2g[chain][group];

	for (ridx = RTWN_RIDX_OFDM6; ridx <= RTWN_RIDX_OFDM54; ridx++)
		power[ridx] += rs->ofdm_tx_pwr_diff_2g[chain][0];

	for (i = 0; i < sc->ntxchains; i++) {
		uint8_t min_mcs;
		uint8_t pwr_diff;

		if (IEEE80211_IS_CHAN_HT40(c))
			pwr_diff = rs->bw40_tx_pwr_diff_2g[chain][i];
		else
			pwr_diff = rs->bw20_tx_pwr_diff_2g[chain][i];

		min_mcs = RTWN_RIDX_MCS(i * 8);
		for (ridx = min_mcs; ridx <= max_mcs; ridx++)
			power[ridx] += pwr_diff;

	}

	/* Apply max limit. */
	for (ridx = RTWN_RIDX_CCK1; ridx <= max_mcs; ridx++) {
		if (power[ridx] > R92C_MAX_TX_PWR)
			power[ridx] = R92C_MAX_TX_PWR;
	}

#ifdef RTWN_DEBUG
	if (sc->sc_debug & RTWN_DEBUG_TXPWR) {
		/* Dump per-rate Tx power values. */
		printf("Tx power for chain %d:\n", chain);
		for (ridx = RTWN_RIDX_CCK1; ridx < RTWN_RIDX_COUNT; ridx++)
			printf("Rate %d = %u\n", ridx, power[ridx]);
	}
#endif
}


static void
r92e_write_txpower(struct rtwn_softc *sc, int chain,
    uint8_t power[RTWN_RIDX_COUNT])
{
	uint32_t reg;

	/* Write per-CCK rate Tx power. */
	if (chain == 0) {
		reg = rtwn_bb_read(sc, R92C_TXAGC_A_CCK1_MCS32);
		reg = RW(reg, R92C_TXAGC_A_CCK1,  power[RTWN_RIDX_CCK1]);
		rtwn_bb_write(sc, R92C_TXAGC_A_CCK1_MCS32, reg);
		reg = rtwn_bb_read(sc, R92C_TXAGC_B_CCK11_A_CCK2_11);
		reg = RW(reg, R92C_TXAGC_A_CCK2,  power[RTWN_RIDX_CCK2]);
		reg = RW(reg, R92C_TXAGC_A_CCK55, power[RTWN_RIDX_CCK55]);
		reg = RW(reg, R92C_TXAGC_A_CCK11, power[RTWN_RIDX_CCK11]);
		rtwn_bb_write(sc, R92C_TXAGC_B_CCK11_A_CCK2_11, reg);
	} else {
		reg = rtwn_bb_read(sc, R92C_TXAGC_B_CCK1_55_MCS32);
		reg = RW(reg, R92C_TXAGC_B_CCK1,  power[RTWN_RIDX_CCK1]);
		reg = RW(reg, R92C_TXAGC_B_CCK2,  power[RTWN_RIDX_CCK2]);
		reg = RW(reg, R92C_TXAGC_B_CCK55, power[RTWN_RIDX_CCK55]);
		rtwn_bb_write(sc, R92C_TXAGC_B_CCK1_55_MCS32, reg);
		reg = rtwn_bb_read(sc, R92C_TXAGC_B_CCK11_A_CCK2_11);
		reg = RW(reg, R92C_TXAGC_B_CCK11, power[RTWN_RIDX_CCK11]);
		rtwn_bb_write(sc, R92C_TXAGC_B_CCK11_A_CCK2_11, reg);
	}
	/* Write per-OFDM rate Tx power. */
	rtwn_bb_write(sc, R92C_TXAGC_RATE18_06(chain),
	    SM(R92C_TXAGC_RATE06, power[RTWN_RIDX_OFDM6]) |
	    SM(R92C_TXAGC_RATE09, power[RTWN_RIDX_OFDM9]) |
	    SM(R92C_TXAGC_RATE12, power[RTWN_RIDX_OFDM12]) |
	    SM(R92C_TXAGC_RATE18, power[RTWN_RIDX_OFDM18]));
	rtwn_bb_write(sc, R92C_TXAGC_RATE54_24(chain),
	    SM(R92C_TXAGC_RATE24, power[RTWN_RIDX_OFDM24]) |
	    SM(R92C_TXAGC_RATE36, power[RTWN_RIDX_OFDM36]) |
	    SM(R92C_TXAGC_RATE48, power[RTWN_RIDX_OFDM48]) |
	    SM(R92C_TXAGC_RATE54, power[RTWN_RIDX_OFDM54]));
	/* Write per-MCS Tx power. */
	rtwn_bb_write(sc, R92C_TXAGC_MCS03_MCS00(chain),
	    SM(R92C_TXAGC_MCS00,  power[RTWN_RIDX_MCS(0)]) |
	    SM(R92C_TXAGC_MCS01,  power[RTWN_RIDX_MCS(1)]) |
	    SM(R92C_TXAGC_MCS02,  power[RTWN_RIDX_MCS(2)]) |
	    SM(R92C_TXAGC_MCS03,  power[RTWN_RIDX_MCS(3)]));
	rtwn_bb_write(sc, R92C_TXAGC_MCS07_MCS04(chain),
	    SM(R92C_TXAGC_MCS04,  power[RTWN_RIDX_MCS(4)]) |
	    SM(R92C_TXAGC_MCS05,  power[RTWN_RIDX_MCS(5)]) |
	    SM(R92C_TXAGC_MCS06,  power[RTWN_RIDX_MCS(6)]) |
	    SM(R92C_TXAGC_MCS07,  power[RTWN_RIDX_MCS(7)]));
	if (sc->ntxchains >= 2) {
		rtwn_bb_write(sc, R92C_TXAGC_MCS11_MCS08(chain),
		    SM(R92C_TXAGC_MCS08,  power[RTWN_RIDX_MCS(8)]) |
		    SM(R92C_TXAGC_MCS09,  power[RTWN_RIDX_MCS(9)]) |
		    SM(R92C_TXAGC_MCS10,  power[RTWN_RIDX_MCS(10)]) |
		    SM(R92C_TXAGC_MCS11,  power[RTWN_RIDX_MCS(11)]));
		rtwn_bb_write(sc, R92C_TXAGC_MCS15_MCS12(chain),
		    SM(R92C_TXAGC_MCS12,  power[RTWN_RIDX_MCS(12)]) |
		    SM(R92C_TXAGC_MCS13,  power[RTWN_RIDX_MCS(13)]) |
		    SM(R92C_TXAGC_MCS14,  power[RTWN_RIDX_MCS(14)]) |
		    SM(R92C_TXAGC_MCS15,  power[RTWN_RIDX_MCS(15)]));
	}
}

static void
r92e_set_txpower(struct rtwn_softc *sc, struct ieee80211_channel *c)
{
	uint8_t power[RTWN_RIDX_COUNT];
	int i;

	for (i = 0; i < sc->ntxchains; i++) {
		memset(power, 0, sizeof(power));
		/* Compute per-rate Tx power values. */
		r92e_get_txpower(sc, i, c, power);
		/* Write per-rate Tx power values to hardware. */
		r92e_write_txpower(sc, i, power);
	}
}

static void
r92e_set_bw40(struct rtwn_softc *sc, uint8_t chan, int prichlo)
{
	int i;

	rtwn_setbits_2(sc, R92C_WMAC_TRXPTCL_CTL, 0x100, 0x80);
	rtwn_write_1(sc, R12A_DATA_SEC,
	    prichlo ? R12A_DATA_SEC_PRIM_DOWN_20 : R12A_DATA_SEC_PRIM_UP_20);

	rtwn_bb_setbits(sc, R92C_FPGA0_RFMOD, 0, R92C_RFMOD_40MHZ);
	rtwn_bb_setbits(sc, R92C_FPGA1_RFMOD, 0, R92C_RFMOD_40MHZ);

	/* Select 40MHz bandwidth. */
	for (i = 0; i < sc->nrxchains; i++)
		rtwn_rf_setbits(sc, i, R92C_RF_CHNLBW,
		    R88E_RF_CHNLBW_BW20, 0x400);

	/* Set CCK side band. */
	rtwn_bb_setbits(sc, R92C_CCK0_SYSTEM,
	    R92C_CCK0_SYSTEM_CCK_SIDEBAND, (prichlo ? 0 : 1) << 4);
		
	rtwn_bb_setbits(sc, R92C_OFDM1_LSTF, 0x0c00, (prichlo ? 1 : 2) << 10);

	rtwn_bb_setbits(sc, R92C_FPGA0_ANAPARAM2,
	    R92C_FPGA0_ANAPARAM2_CBW20, 0);

	rtwn_bb_setbits(sc, 0x818, 0x0c000000, (prichlo ? 2 : 1) << 26);
}

static void
r92e_set_bw20(struct rtwn_softc *sc, uint8_t chan)
{
	int i;

	rtwn_setbits_2(sc, R92C_WMAC_TRXPTCL_CTL, 0x180, 0);
	rtwn_write_1(sc, R12A_DATA_SEC, R12A_DATA_SEC_NO_EXT);

	rtwn_bb_setbits(sc, R92C_FPGA0_RFMOD, R92C_RFMOD_40MHZ, 0);
	rtwn_bb_setbits(sc, R92C_FPGA1_RFMOD, R92C_RFMOD_40MHZ, 0);

	/* Select 20MHz bandwidth. */
	for (i = 0; i < sc->nrxchains; i++)
		rtwn_rf_setbits(sc, i, R92C_RF_CHNLBW,
		    R88E_RF_CHNLBW_BW20, 0xc00);

	rtwn_bb_setbits(sc, R92C_OFDM0_TXPSEUDONOISEWGT, 0xc0000000, 0);
}

void
r92e_set_chan(struct rtwn_softc *sc, struct ieee80211_channel *c)
{
	struct r92e_softc *rs = sc->sc_priv;
	u_int chan;
	int i;

	chan = rtwn_chan2centieee(c);

	for (i = 0; i < sc->nrxchains; i++) {
		rtwn_rf_write(sc, i, R92C_RF_CHNLBW,
		    RW(rs->rf_chnlbw[0], R92C_RF_CHNLBW_CHNL, chan));
	}

	if (IEEE80211_IS_CHAN_HT40(c))
		r92e_set_bw40(sc, chan, IEEE80211_IS_CHAN_HT40U(c));
	else
		r92e_set_bw20(sc, chan);

	/* Set Tx power for this new channel. */
	r92e_set_txpower(sc, c);
}
