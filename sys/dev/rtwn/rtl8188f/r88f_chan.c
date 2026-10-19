/*
 * Copyright (c) 2026 Mir Saheb Ali <mirsahebali@pm.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/cdefs.h>
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

#include <dev/rtwn/if_rtwn_ridx.h>

#include <dev/rtwn/rtl8188e/r88e.h>
#include <dev/rtwn/rtl8188e/r88e_priv.h>
#include <dev/rtwn/rtl8188e/r88e_reg.h>

#include <dev/rtwn/rtl8192c/r92c.h>
#include <dev/rtwn/rtl8192c/r92c_var.h>

#include <dev/rtwn/rtl8723b/r23b_reg.h>

#include <dev/rtwn/rtl8812a/r12a_reg.h>

#include <dev/rtwn/rtl8188f/r88f.h>
#include <dev/rtwn/rtl8188f/r88f_reg.h>

static int
rtwn_chan2group(int chan)
{
	int group;

	if (chan <= 2)
		group = 0;
	else if (chan <= 5)
		group = 1;
	else if (chan <= 8)
		group = 2;
	else if (chan <= 11)
		group = 3;
	else
		group = 4;

	return (group);
}

void
r88f_get_txpower(struct rtwn_softc *sc, int chain,
    struct ieee80211_channel *c, uint8_t power[RTWN_RIDX_COUNT])
{
	struct r92c_softc *rs = sc->sc_priv;
	const struct rtwn_r88e_txpwr *rt = rs->rs_txpwr;
	uint32_t reg;
	uint8_t cckpow, htpow, ofdmpow;
	int8_t diff;
	int ridx, chan, group;

	/* Determine channel group. */
	chan = rtwn_chan2centieee(c);	/* XXX center freq! */
	group = rtwn_chan2group(chan);

	memset(power, 0, R88F_RTWN_POWER_COUNT * sizeof(power[0]));

	/* Compute per-CCK rate Tx power. */
	cckpow = rt->cck_tx_pwr[group];
	for (ridx = RTWN_RIDX_CCK1; ridx <= RTWN_RIDX_CCK11; ridx++) {
		power[ridx] = cckpow;
		if (power[ridx] > R92C_MAX_TX_PWR)
			power[ridx] = R92C_MAX_TX_PWR;
	}

	reg = rtwn_read_4(sc, R92C_TXAGC_A_CCK1_MCS32);
	reg &= 0xffff00ff;
	reg |= (cckpow << 8);
	rtwn_write_4(sc, R92C_TXAGC_A_CCK1_MCS32, reg);

	reg = rtwn_read_4(sc, R92C_TXAGC_B_CCK11_A_CCK2_11);
	reg &= 0xff;
	reg |= ((cckpow << 8) | (cckpow << 16) | (cckpow << 24));
	rtwn_write_4(sc, R92C_TXAGC_B_CCK11_A_CCK2_11, reg);

	htpow = rt->ht40_tx_pwr[group];

	/* Compute per-OFDM rate Tx power. */
	diff = rt->ofdm_tx_pwr_diff;
	ofdmpow = htpow + diff;
	for (ridx = RTWN_RIDX_OFDM6; ridx <= RTWN_RIDX_OFDM54; ridx++) {
		power[ridx] = ofdmpow;
		if (power[ridx] > R92C_MAX_TX_PWR)
			power[ridx] = R92C_MAX_TX_PWR;
	}

	/* Compute per-MCS Tx power. */
	diff = rt->bw20_tx_pwr_diff;
	htpow += diff;
	for (ridx = RTWN_RIDX_HT_MCS(0); ridx < RTWN_RIDX_HT_MCS(8); ridx++) {
		power[ridx] = htpow;
		if (power[ridx] > R92C_MAX_TX_PWR)
			power[ridx] = R92C_MAX_TX_PWR;
	}
}

static void
r88f_set_txpower(struct rtwn_softc *sc, struct ieee80211_channel *c)
{
	uint8_t power[RTWN_RIDX_COUNT];

	memset(power, 0, sizeof(power));
	/* Compute per-rate Tx power values. */
	r88f_get_txpower(sc, 0, c, power);
	/* Write per-rate Tx power values to hardware. */
	r92c_write_txpower(sc, 0, power);
}

static void
r88f_spur_calibration(struct rtwn_softc *sc, struct ieee80211_channel *c,
    int chan)
{
	static const uint32_t frequencies[14 + 1] = {
		[5] = 0xFCCD,
		[6] = 0xFC4D,
		[7] = 0xFFCD,
		[8] = 0xFF4D,
		[11] = 0xFDCD,
		[13] = 0xFCCD,
		[14] = 0xFF9A
	};

	static const uint32_t reg_d40[14 + 1] = {
		[5] = 0x06000000,
		[6] = 0x00000600,
		[13] = 0x06000000
	};

	static const uint32_t reg_d44[14 + 1] = {
		[11] = 0x04000000
	};

	static const uint32_t reg_d4c[14 + 1] = {
		[7] = 0x06000000,
		[8] = 0x00000380,
		[14] = 0x00180000
	};

	const uint8_t threshold = 0x16;
	bool do_notch, hw_ctrl, sw_ctrl, hw_ctrl_s1 = 0, sw_ctrl_s1 = 0;
	uint32_t reg, initial_gain, reg948;

	rtwn_bb_setbits(sc, R88F_OFDM0_RX_D_SYNC_PATH, 0, 0x1f000000);

	/* enable notch filter */
	rtwn_bb_setbits(sc, R88F_OFDM0_RX_D_SYNC_PATH, 0, 0x00000200);

	if (chan <= 14 && frequencies[chan] > 0) {
		reg948 = rtwn_read_4(sc, R23B_ANT_SEL);
		hw_ctrl = reg948 & 0x00000040;
		sw_ctrl = !hw_ctrl;
		if (hw_ctrl) {
			reg = rtwn_read_4(sc, R88F_FPGA0_XB_RF_INT_OE);
			reg &= 0x00000038;
			hw_ctrl_s1 = reg == 0x00000008;
		} else if (sw_ctrl) {
			sw_ctrl_s1 = !(reg948 & 0x00000200);
		}

		if (hw_ctrl_s1 || sw_ctrl_s1) {
			initial_gain = rtwn_read_4(sc, R92C_OFDM0_AGCCORE1(0));

			/* Disable CCK block */
			rtwn_bb_setbits(sc, R92C_FPGA0_RFMOD,
			    R92C_RFMOD_CCK_EN, 0);

			rtwn_bb_setbits(sc, R92C_OFDM0_AGCCORE1(0),
			    R92C_OFDM0_AGCCORE1_GAIN_M, 0x30);

			/* disable 3-wire */
			rtwn_bb_write(sc, R88F_FPGA0_ANALOG4, 0xccf000c0);

			/* Setup PSD */
			rtwn_bb_write(sc, R88F_FPGA0_PSD_FUNC,
			    frequencies[chan]);

			/* Start PSD */
			rtwn_bb_write(sc, R88F_FPGA0_PSD_FUNC,
			    0x400000 | frequencies[chan]);

			rtwn_delay(sc, 30);

			do_notch = rtwn_bb_read(sc, R88F_FPGA0_PSD_REPORT) >=
				threshold;

			/* turn off PSD */
			rtwn_bb_write(sc, R88F_FPGA0_PSD_FUNC,
			    frequencies[chan]);

			/* enable 3-wire */
			rtwn_bb_write(sc, R88F_FPGA0_ANALOG4, 0xccc000c0);

			/* Enable CCK block */
			rtwn_bb_setbits(sc, R92C_FPGA0_RFMOD, 0,
			    R92C_RFMOD_CCK_EN);

			rtwn_bb_write(sc, R92C_OFDM0_AGCCORE1(0), initial_gain);

			if (do_notch) {
				rtwn_bb_write(sc, R88F_OFDM1_CSI_FIX_MASK1,
				    reg_d40[chan]);
				rtwn_bb_write(sc, R88F_OFDM1_CSI_FIX_MASK2,
				    reg_d44[chan]);

				rtwn_bb_write(sc, 0xd48, 0x0);
				rtwn_bb_write(sc, 0xd4c, reg_d4c[chan]);

				/* enable CSI mask */
				rtwn_bb_setbits(sc, R88F_OFDM1_CFO_TRACKING, 0,
				    0x10000000);

				return;
			}
		}
	}

	/* disable CSI mask function */
	rtwn_bb_setbits(sc, R88F_OFDM1_CFO_TRACKING, 0x10000000, 0);
}

void
r88f_set_chan(struct rtwn_softc *sc, struct ieee80211_channel *c)
{
	struct r92c_softc *rs = sc->sc_priv;
	uint32_t reg;
	uint8_t chan, subchannel;
	bool sec_ch_above;

	chan = rtwn_chan2centieee(c);	/* XXX center freq! */

	/* Set Tx power for this new channel. */
	r88f_set_txpower(sc, c);

	/* Spur calibration */
	r88f_spur_calibration(sc, c, chan);

	/* Set bandwidth mode */
	rtwn_bb_setbits(sc, R92C_FPGA0_RFMOD, R92C_RFMOD_40MHZ,
	    IEEE80211_IS_CHAN_HT40(c));
	rtwn_bb_setbits(sc, R92C_FPGA1_RFMOD, R92C_RFMOD_40MHZ,
	    IEEE80211_IS_CHAN_HT40(c));

	/* RXADC CLK */
	rtwn_bb_setbits(sc, R92C_FPGA0_RFMOD, 0, 0x00000700);

	/* TXDAC CLK */
	rtwn_bb_setbits(sc, R92C_FPGA0_RFMOD, 0x00002000,
	    0x00004000 | 0x00001000);

	/* Small bandwidth */
	rtwn_bb_setbits(sc, R92C_OFDM0_TXPSEUDONOISEWGT, 0xc0000000, 0);

	/* ADC buffer clk */
	rtwn_bb_setbits(sc, R92C_OFDM0_TXPSEUDONOISEWGT, 0x20000000,
	    0x10000000);

	/* ADC buffer clk */
	rtwn_bb_setbits(sc, R88F_OFDM0_XA_RX_AFE, 0x20000000, 0x10000000);

	/* OFDM Rx DFIR */
	rtwn_bb_setbits(sc, R88F_RX_DFIR, 0x00080000, 0);

	reg = rtwn_bb_read(sc, R88F_RX_DFIR);
	reg &= ~0x00f00000;
	reg |= 0x00200000;
	if (IEEE80211_IS_CHAN_HT20(c) || IEEE80211_IS_CHAN_VHT20(c) ||
	    (!IEEE80211_IS_CHAN_HT(c) && !IEEE80211_IS_CHAN_VHT(c)))
		reg |= 0x00100000;
	else if (IEEE80211_IS_CHAN_HT40(c) || IEEE80211_IS_CHAN_VHT40(c))
		reg |= 0x00400000;
	rtwn_bb_write(sc, R88F_RX_DFIR, reg);

	if (IEEE80211_IS_CHAN_HT40(c) || IEEE80211_IS_CHAN_VHT40(c)) {
		if (IEEE80211_IS_CHAN_HT40U(c) ||
			IEEE80211_IS_CHAN_VHT40U(c)) {
			sec_ch_above = 1;
			chan += 2;
		} else {
			sec_ch_above = 0;
			chan -= 2;
		}

		/* Set Control channel to upper or lower. */
		reg = rtwn_bb_read(sc, R92C_CCK0_SYSTEM);
		reg &= ~R92C_CCK0_SYSTEM_CCK_SIDEBAND;
		if (!sec_ch_above)
			reg |= R92C_CCK0_SYSTEM_CCK_SIDEBAND;
		rtwn_bb_write(sc, R92C_CCK0_SYSTEM, reg);

		if (sec_ch_above)
			subchannel = 2;
		else
			subchannel = 1;
		rtwn_bb_setbits(sc, R12A_DATA_SEC, 0x0000000F, subchannel);

		rtwn_bb_setbits(sc, R92C_RRSR, R92C_RRSR_RSC_LOWSUBCHNL |
		    R92C_RRSR_RSC_UPSUBCHNL, 0);
	}

	/* RF TRX_BW */

	reg = chan;
	if (IEEE80211_IS_CHAN_HT20(c) || IEEE80211_IS_CHAN_VHT20(c) ||
	    (!IEEE80211_IS_CHAN_HT(c) && !IEEE80211_IS_CHAN_VHT(c)))
		reg |= R88F_RF_CHNLBW_BW20;
	else if (IEEE80211_IS_CHAN_HT40(c) || IEEE80211_IS_CHAN_VHT40(c))
		reg |= R88F_RF_CHNLBW_BW40;
	rtwn_rf_write(sc, 0, R92C_RF_CHNLBW, reg);

	/* FILTER BW&RC Corner (ACPR) */
	if (IEEE80211_IS_CHAN_HT20(c) || IEEE80211_IS_CHAN_VHT20(c) ||
	    (!IEEE80211_IS_CHAN_HT(c) && !IEEE80211_IS_CHAN_VHT(c)))
		reg = 0x00065;
	else if (IEEE80211_IS_CHAN_HT40(c) || IEEE80211_IS_CHAN_VHT40(c))
		reg = 0x00025;
	rtwn_rf_write(sc, 0, R88F_RF_RXG_GAIN, reg);

	if (IEEE80211_IS_CHAN_HT20(c) || IEEE80211_IS_CHAN_VHT20(c) ||
	    (!IEEE80211_IS_CHAN_HT(c) && !IEEE80211_IS_CHAN_VHT(c)))
		reg = 0x0;
	else if (IEEE80211_IS_CHAN_HT40(c) || IEEE80211_IS_CHAN_VHT40(c))
		reg = 0x01000;
	rtwn_rf_write(sc, 0, R92C_RF_RX_BB2, reg);

	rtwn_rf_write(sc, 0, R92C_RF_RX_G2, 0x1c6c);
	rtwn_rf_write(sc, 0, R88F_RF_GAIN_CCA,
	    R88F_RF_PKTBUF_DBG_CTRL);
	rtwn_rf_write(sc, 0, R92C_RF_CHNLBW,
		(rs->rf_chnlbw[0] & ~0xfff) | chan | R88E_RF_CHNLBW_BW20);
}
