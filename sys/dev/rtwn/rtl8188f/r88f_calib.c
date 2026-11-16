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

#include <dev/rtwn/if_rtwn_debug.h>

#include <dev/rtwn/rtl8188e/r88e.h>
#include <dev/rtwn/rtl8188e/r88e_reg.h>

#include <dev/rtwn/rtl8192c/r92c_reg.h>

#include <dev/rtwn/rtl8723b/r23b_reg.h>

#include <dev/rtwn/rtl8188f/r88f.h>
#include <dev/rtwn/rtl8188f/r88f_reg.h>

#define OFDM_LSTF_MASK			0x70000000

void
r88f_lc_calib(struct rtwn_softc *sc)
{
	uint32_t reg;
	uint32_t rf_amode, lstf;
	int i;

	/* Check continuous TX and Packet TX */
	lstf = rtwn_bb_read(sc, R92C_OFDM1_LSTF);

	if (lstf & OFDM_LSTF_MASK) {
		/* Disable all continuous TX */
		reg = lstf & ~OFDM_LSTF_MASK;
		rtwn_bb_write(sc, R92C_OFDM1_LSTF, reg);
	} else {
		/* Deal with Packet TX case */
		/* block all queues */
		rtwn_write_1(sc, R92C_TXPAUSE, 0xff);
	}

	/* Read original RF mode Path A */
	rf_amode = rtwn_rf_read(sc, 0, R92C_RF_CHNLBW);

	/* Start LC calibration */
	rtwn_rf_write(sc, 0, R92C_RF_CHNLBW, rf_amode | 0x08000);

	for (i = 0; i < 100; i++) {
		if ((rtwn_rf_read(sc, 0, R92C_RF_CHNLBW) & 0x08000) == 0)
			break;
		rtwn_delay(sc, 10);
	}

	if (i == 100)
		device_printf(sc->sc_dev, "LC calibration timed out.\n");

	rtwn_rf_write(sc, 0, R92C_RF_CHNLBW, rf_amode);

	/* Restore original parameters */
	if (lstf & OFDM_LSTF_MASK)
		rtwn_bb_write(sc, R92C_OFDM1_LSTF, lstf);
	else /*  Deal with Packet TX case */
		rtwn_write_1(sc, R92C_TXPAUSE, 0x00);
}

#define R88F_ADDA_REGS			16
#define R88F_MAC_REGS			4
#define R88F_BB_REGS			9

struct r88f_iqk_calib_regs {
	uint32_t	adda_regs[R88F_ADDA_REGS];
	uint32_t	mac_regs[R88F_MAC_REGS];
	uint32_t	bb_regs[R88F_BB_REGS];
	uint32_t	hssi_param1;
};

struct r88f_iqk_result {
	uint32_t	before;
	uint32_t	after;
};

static const uint32_t iqk_adda_regs[R88F_ADDA_REGS] = {
	R88F_FPGA0_XCD_SWITCH_CTRL, R88F_BLUETOOTH,
	R88F_RX_WAIT_CCA, R88F_TX_CCK_RFON,
	R88F_TX_CCK_BBON, R88F_TX_OFDM_RFON,
	R88F_TX_OFDM_BBON, R88F_TX_TO_RX,
	R88F_TX_TO_TX, R88F_RX_CCK,
	R88F_RX_OFDM, R88F_RX_WAIT_RIFS,
	R88F_RX_TO_RX, R88F_STANDBY,
	R88F_SLEEP, R88F_PMPD_ANAEN
};
static const uint32_t iqk_mac_regs[R88F_MAC_REGS] = {
	R92C_TXPAUSE, R92C_BCN_CTRL(0),
	R92C_BCN_CTRL(1), R92C_GPIO_MUXCFG
};
static const uint32_t iqk_bb_regs[R88F_BB_REGS] = {
	R92C_OFDM0_TRXPATHENA, R92C_OFDM0_TRMUXPAR,
	R88F_FPGA0_XCD_RF_SW_CTRL, R92C_CONFIG_ANT(0), R92C_CONFIG_ANT(1),
	R92C_FPGA0_RFIFACESW(0), R92C_FPGA0_RFIFACEOE(0),
	R92C_FPGA0_RFIFACEOE(1), R92C_FPGA0_RFMOD
};

static void
r88f_iqk_save_regs(struct rtwn_softc *sc, struct r88f_iqk_calib_regs *backup)
{
	int i;

	for (i = 0; i < R88F_ADDA_REGS; i++)
		backup->adda_regs[i] = rtwn_bb_read(sc, iqk_adda_regs[i]);

	for (i = 0; i < R88F_MAC_REGS - 1; i++)
		backup->mac_regs[i] = rtwn_read_1(sc, iqk_mac_regs[i]);

	backup->mac_regs[i] = rtwn_bb_read(sc, iqk_mac_regs[i]);

	for (i = 0; i < R88F_BB_REGS; i++)
		backup->bb_regs[i] = rtwn_bb_read(sc, iqk_bb_regs[i]);
}

static void
r88f_iqk_restore_regs(struct rtwn_softc *sc,
	const struct r88f_iqk_calib_regs *backup)
{
	int i;

	for (i = 0; i < R88F_ADDA_REGS; i++)
		rtwn_bb_write(sc, iqk_adda_regs[i], backup->adda_regs[i]);

	for (i = 0; i < R88F_MAC_REGS - 1; i++)
		rtwn_write_1(sc, iqk_mac_regs[i], backup->mac_regs[i]);

	rtwn_bb_write(sc, iqk_mac_regs[i], backup->mac_regs[i]);

	for (i = 0; i < R88F_BB_REGS; i++)
		rtwn_bb_write(sc, iqk_bb_regs[i], backup->bb_regs[i]);
}

static void
r88f_path_adda_on(struct rtwn_softc *sc)
{
	int i;

	rtwn_bb_write(sc, iqk_adda_regs[0], 0x03c00014);

	for (i = 1; i < R88F_ADDA_REGS; i++)
		rtwn_bb_write(sc, iqk_adda_regs[i], 0x03c00014);
}

static void
r88f_fill_iqk_matrix_a(struct rtwn_softc *sc,
	const struct r88f_iqk_result *tx_result,
	const struct r88f_iqk_result *rx_result,
	bool tx_only)
{
	uint32_t oldval, x, tx0_a, result_reg;
	int y, tx0_c;
	uint32_t reg;

	reg = rtwn_bb_read(sc, R92C_OFDM0_TXIQIMBALANCE(0));
	oldval = reg >> 22;

	x = tx_result->before;
	if ((x & 0x00000200) != 0)
		x = x | 0xfffffc00;
	tx0_a = (x * oldval) >> 8;

	reg = rtwn_bb_read(sc, R92C_OFDM0_TXIQIMBALANCE(0));
	reg &= ~0x3ff;
	reg |= tx0_a;
	rtwn_bb_write(sc, R92C_OFDM0_TXIQIMBALANCE(0), reg);

	reg = rtwn_bb_read(sc, R92C_OFDM0_ECCATHRESHOLD);
	reg &= ~0x80000000;
	if ((x * oldval >> 7) & 0x1)
		reg |= 0x80000000;
	rtwn_bb_write(sc, R92C_OFDM0_ECCATHRESHOLD, reg);

	y = tx_result->after;
	if ((y & 0x00000200) != 0)
		y |= 0xfffffc00;
	tx0_c = (y * oldval) >> 8;

	rtwn_bb_setbits(sc, R92C_OFDM0_TXAFE(0), 0xf0000000,
		 ((tx0_c & 0x3c0) >> 6) << 28);

	reg = rtwn_bb_read(sc, R92C_OFDM0_TXIQIMBALANCE(0));
	reg &= ~0x003f0000;
	reg |= ((tx0_a & 0x3f) << 16);
	rtwn_bb_write(sc, R92C_OFDM0_TXIQIMBALANCE(0), reg);

	reg = rtwn_bb_read(sc, R92C_OFDM0_ECCATHRESHOLD);
	reg &= ~0x20000000;
	if ((y * oldval >> 7) & 0x1)
		reg |= 0x20000000;
	rtwn_bb_write(sc, R92C_OFDM0_ECCATHRESHOLD, reg);

	if (tx_only) {
		RTWN_DPRINTF(sc, RTWN_DEBUG_CALIB, "%s: only TX\n", __func__);
		return;
	}

	result_reg = rx_result->before;
	rtwn_bb_setbits(sc, R92C_OFDM0_RXIQIMBALANCE(0), 0x3ff,
		 result_reg & 0x3ff);

	result_reg = rx_result->after & 0x3f;
	rtwn_bb_setbits(sc, R92C_OFDM0_RXIQIMBALANCE(0), 0xfc00,
		 (result_reg << 10) & 0xfc00);

	result_reg = (rx_result->after >> 6) & 0xf;
	rtwn_bb_setbits(sc, R92C_OFDM0_RXIQEXTANTA, 0xf0000000,
		 result_reg << 28);
}

static bool
r88f_iq_calib_get_tx_result(struct rtwn_softc *sc,
    struct r88f_iqk_result *tx_result)
{
	uint32_t reg_eac, reg_e94, reg_e9c;

	reg_eac = rtwn_bb_read(sc, R92C_RX_POWER_IQK_AFTER(0));

	reg_e94 = rtwn_bb_read(sc, R92C_TX_POWER_IQK_BEFORE(0));
	tx_result->before = (reg_e94 >> 16) & 0x3ff;

	reg_e9c = rtwn_bb_read(sc, R92C_TX_POWER_IQK_AFTER(0));
	tx_result->after = (reg_e9c >> 16) & 0x3ff;

	return (!(reg_eac & 0x10000000) &&
		(tx_result->before != 0x142) &&
		(tx_result->after != 0x042));
}

static bool
r88f_iq_calib_get_rx_result(struct rtwn_softc *sc, uint32_t lok_result,
    struct r88f_iqk_result *rx_result)
{

	uint32_t reg_eac, reg_ea4;

	/* Reload LOK value from TX calibration */
	rtwn_rf_write(sc, 0, R92C_RF_TXM_IDAC, lok_result);

	reg_eac = rtwn_bb_read(sc, R92C_RX_POWER_IQK_AFTER(0));
	reg_ea4 = rtwn_bb_read(sc, R92C_RX_POWER_IQK_BEFORE(0));
	rx_result->before = (reg_ea4 >> 16) & 0x3ff;
	rx_result->after = (reg_eac >> 16) & 0x3ff;

	return (!(reg_eac & 0x08000000) &&
		(rx_result->before != 0x132) &&
		(rx_result->after != 0x036));
}

static bool
r88f_iqk_path_a_tx(struct rtwn_softc *sc, uint32_t *lok_result,
    struct r88f_iqk_result *tx_result)
{
	uint32_t reg;

	/* Leave IQK mode */
	reg = rtwn_bb_read(sc, R92C_FPGA0_IQK);
	reg &= 0x000000ff;
	rtwn_bb_write(sc, R92C_FPGA0_IQK, reg);

	/* Enable path A PA in TX IQK mode */
	rtwn_rf_setbits(sc, 0, R88E_RF_WE_LUT, 0, 0x80000);
	rtwn_rf_write(sc, 0, R92C_RF_RCK_OS, 0x20000);
	rtwn_rf_write(sc, 0, R92C_RF_TXPA_G(0), 0x0000f);
	rtwn_rf_write(sc, 0, R92C_RF_TXPA_G(1), 0x07ff7);

	/* PA, PAD gain adjust */
	rtwn_rf_write(sc, 0, R88F_RF_GAIN_CCA, 0x980);
	rtwn_rf_write(sc, 0, R88F_RF_PAD_TXG, 0x5102a);

	/* Enter IQK mode */
	reg = rtwn_bb_read(sc, R92C_FPGA0_IQK);
	reg &= 0x000000ff;
	reg |= 0x80800000;
	rtwn_bb_write(sc, R92C_FPGA0_IQK, reg);

	/* path-A IQK settings */
	rtwn_bb_write(sc, R92C_TX_IQK_TONE(0), 0x18008c1c);
	rtwn_bb_write(sc, R92C_RX_IQK_TONE(0), 0x38008c1c);

	rtwn_bb_write(sc, R92C_TX_IQK_PI(0), 0x821403ff);
	rtwn_bb_write(sc, R92C_RX_IQK_PI(0), 0x28160000);

	/* LO calibration setting */
	rtwn_bb_write(sc, R92C_IQK_AGC_RSP, 0x00462911);

	/* One shot, path A LOK & IQK */
	rtwn_bb_write(sc, R92C_IQK_AGC_PTS, 0xf9000000);
	rtwn_bb_write(sc, R92C_IQK_AGC_PTS, 0xf8000000);

	rtwn_delay(sc, 25);

	/* Leave IQK mode */
	reg = rtwn_bb_read(sc, R92C_FPGA0_IQK);
	reg &= 0x000000ff;
	rtwn_bb_write(sc, R92C_FPGA0_IQK, reg);

	rtwn_rf_write(sc, 0, R88F_RF_GAIN_CCA, 0x180);

	*lok_result = rtwn_rf_read(sc, 0, R92C_RF_TXM_IDAC);

	return (r88f_iq_calib_get_tx_result(sc, tx_result));
}

static bool
r88f_iqk_path_a_tx_rx(struct rtwn_softc *sc, uint32_t lok_result,
    struct r88f_iqk_result *rx_result)
{
	uint32_t reg;
	struct r88f_iqk_result tx_verify;

	/*
	 * Leave IQK mode
	 */
	reg = rtwn_bb_read(sc, R92C_FPGA0_IQK);
	reg &= 0x000000ff;
	rtwn_bb_write(sc, R92C_FPGA0_IQK, reg);

	/*
	 * Enable path A PA in TX IQK mode
	 */
	rtwn_rf_setbits(sc, 0, R88E_RF_WE_LUT, 0, 0x80000);
	rtwn_rf_write(sc, 0, R92C_RF_RCK_OS, 0x30000);
	rtwn_rf_write(sc, 0, R92C_RF_TXPA_G(0), 0x0000f);
	rtwn_rf_write(sc, 0, R92C_RF_TXPA_G(1), 0xf1173);

	/* PA,PAD gain adjust */
	rtwn_rf_write(sc, 0, R88F_RF_GAIN_CCA, 0x980);
	rtwn_rf_write(sc, 0, R88F_RF_PAD_TXG, 0x5102a);

	/*
	 * Enter IQK mode
	 */
	reg = rtwn_bb_read(sc, R92C_FPGA0_IQK);
	reg &= 0x000000ff;
	reg |= 0x80800000;
	rtwn_bb_write(sc, R92C_FPGA0_IQK, reg);

	/*
	 * TX IQK settings
	 */
	rtwn_bb_write(sc, R92C_TX_IQK, 0x01007c00);
	rtwn_bb_write(sc, R92C_RX_IQK, 0x01004800);

	/* path-A IQK setting */
	rtwn_bb_write(sc, R92C_TX_IQK_TONE(0), 0x10008c1c);
	rtwn_bb_write(sc, R92C_RX_IQK_TONE(0), 0x30008c1c);

	rtwn_bb_write(sc, R92C_TX_IQK_PI(0), 0x82160fff);
	rtwn_bb_write(sc, R92C_RX_IQK_PI(0), 0x28160000);

	/* LO calibration setting */
	rtwn_bb_write(sc, R92C_IQK_AGC_RSP, 0x00462911);

	/* One shot, path A LOK & IQK */
	rtwn_bb_write(sc, R92C_IQK_AGC_PTS, 0xf9000000);
	rtwn_bb_write(sc, R92C_IQK_AGC_PTS, 0xf8000000);

	rtwn_delay(sc, 25);

	/*
	 * Leave IQK mode
	 */
	reg = rtwn_bb_read(sc, R92C_FPGA0_IQK);
	reg &= 0x000000ff;
	rtwn_bb_write(sc, R92C_FPGA0_IQK, reg);

	rtwn_rf_write(sc, 0, R88F_RF_GAIN_CCA, 0x180);

	if (!r88f_iq_calib_get_tx_result(sc, &tx_verify)) {
		/* TX verification failed */
		RTWN_DPRINTF(sc, RTWN_DEBUG_CALIB,
			"%s: TX verification failed in RX path\n", __func__);
		return (false);
	}

	rtwn_bb_write(sc, R92C_TX_IQK, 0x80007c00 |
	       (tx_verify.before << 16) | tx_verify.after);

	/*
	 * Modify RX IQK mode table
	 */
	reg = rtwn_bb_read(sc, R92C_FPGA0_IQK);
	reg &= 0x000000ff;
	rtwn_bb_write(sc, R92C_FPGA0_IQK, reg);

	rtwn_rf_setbits(sc, 0, R88E_RF_WE_LUT, 0, 0x80000);
	rtwn_rf_write(sc, 0, R92C_RF_RCK_OS, 0x30000);
	rtwn_rf_write(sc, 0, R92C_RF_TXPA_G(0), 0x0000f);
	rtwn_rf_write(sc, 0, R92C_RF_TXPA_G(1), 0xf7ff2);

	/*
	 * PA, PAD setting
	 */
	rtwn_rf_write(sc, 0, R88F_RF_GAIN_CCA, 0x980);
	rtwn_rf_write(sc, 0, R88F_RF_PAD_TXG, 0x51000);
	/*
	 * Enter IQK mode
	 */
	reg = rtwn_bb_read(sc, R92C_FPGA0_IQK);
	reg &= 0x000000ff;
	reg |= 0x80800000;
	rtwn_bb_write(sc, R92C_FPGA0_IQK, reg);

	/*
	 * RX IQK setting
	 */
	rtwn_bb_write(sc, R92C_RX_IQK, 0x01004800);

	/* path-A IQK setting */
	rtwn_bb_write(sc, R92C_TX_IQK_TONE(0), 0x30008c1c);
	rtwn_bb_write(sc, R92C_RX_IQK_TONE(0), 0x10008c1c);

	rtwn_bb_write(sc, R92C_TX_IQK_PI(0), 0x82160000);
	rtwn_bb_write(sc, R92C_RX_IQK_PI(0), 0x281613ff);

	/* LO calibration setting */
	rtwn_bb_write(sc, R92C_IQK_AGC_RSP, 0x0046a911);

	/* One shot, path A LOK & IQK */
	rtwn_bb_write(sc, R92C_IQK_AGC_PTS, 0xf9000000);
	rtwn_bb_write(sc, R92C_IQK_AGC_PTS, 0xf8000000);

	rtwn_delay(sc, 25);

	/*
	 * Leave IQK Mode
	 */
	reg = rtwn_bb_read(sc, R92C_FPGA0_IQK);
	reg &= 0x000000ff;
	rtwn_bb_write(sc, R92C_FPGA0_IQK, reg);

	rtwn_rf_write(sc, 0, R88F_RF_GAIN_CCA, 0x180);

	return (r88f_iq_calib_get_rx_result(sc, lok_result, rx_result));

}

#define R88F_MAX_TOLERANCE 5

static bool
r88f_iq_calib_compare_results(struct r88f_iqk_result *tx_result1,
    struct r88f_iqk_result *rx_result1,
    struct r88f_iqk_result *tx_result2,
    struct r88f_iqk_result *rx_result2,
    struct r88f_iqk_result **tx_candidate,
    struct r88f_iqk_result **rx_candidate)
{
	uint32_t diff;
	bool tx_good = false;
	int32_t tmp1, tmp2;

	diff = abs((int32_t)tx_result1->before - (int32_t)tx_result2->before);

	if (diff <= R88F_MAX_TOLERANCE) {
		tmp1 = tx_result1->after & 0x00000200 ?
			(tx_result1->after | 0xfffffc00) : tx_result1->after;
		tmp2 = tx_result2->after & 0x00000200 ?
			(tx_result2->after | 0xfffffc00) : tx_result2->after;
		diff = abs(tmp1 - tmp2);

		if (diff <= R88F_MAX_TOLERANCE) {
			*tx_candidate = tx_result1;
			tx_good = true;
		}
	}

	diff = abs((int32_t)rx_result1->before - (int32_t)rx_result2->before);

	if (diff > R88F_MAX_TOLERANCE) {
		if (tx_good && (rx_result1->before + rx_result1->after) == 0)
			*tx_candidate = tx_result2;

		return (false);
	}

	tmp1 = (rx_result1->after & 0x200) ?
		(0xfffffc00 | rx_result1->after) : rx_result1->after;
	tmp2 = (rx_result2->after & 0x200) ?
		(0xfffffc00 | rx_result2->after) : rx_result2->after;
	diff = abs(tmp1 - tmp2);

	if (diff > R88F_MAX_TOLERANCE)
		return (false);

	*rx_candidate = rx_result1;

	return (tx_good);
}

static void
r88f_iq_calib_run(struct rtwn_softc *sc, struct r88f_iqk_calib_regs *regs,
    struct r88f_iqk_result *tx_result, struct r88f_iqk_result *rx_result, int t)
{
	uint32_t rx_initial_gain, lok_result;
	uint32_t path_sel_rf, path_sel_bb;
	uint32_t reg;
	int retry = 2;
	int i;
	bool path_a_ok;

	/*
	 * Note: IQ calibration must be performed after loading
	 *       PHY_REG.txt , and radio_a, radio_b.txt
	 */

	rx_initial_gain = rtwn_bb_read(sc, R92C_OFDM0_AGCCORE1(0));

	if (t == 0) {
		r88f_iqk_save_regs(sc, regs);
	}

	r88f_path_adda_on(sc);

	if (t == 0) {
		/* Save ADDA parameters, turn Path A ADDA on */
		regs->hssi_param1 = rtwn_bb_read(sc, R92C_HSSI_PARAM1(0));
	}

	/* save RF path */
	path_sel_bb = rtwn_bb_read(sc, R23B_ANT_SEL);
	path_sel_rf = rtwn_rf_read(sc, 0, R23B_RF_S0S1);

	/* BB setting */
	rtwn_bb_write(sc, R92C_OFDM0_TRXPATHENA, 0x03a05600);
	rtwn_bb_write(sc, R92C_OFDM0_TRMUXPAR, 0x000800e4);
	rtwn_bb_write(sc, R88F_FPGA0_XCD_RF_SW_CTRL, 0x25204000);

	rtwn_bb_setbits(sc, R92C_TX_PTCL_CTRL, 0, 0x00ff0000);

	reg = rtwn_bb_read(sc, R92C_FPGA0_IQK);
	reg &= 0xff;
	reg |= 0x80800000;
	rtwn_bb_write(sc, R92C_FPGA0_IQK, reg);
	rtwn_bb_write(sc, R92C_TX_IQK, 0x01007c00);
	rtwn_bb_write(sc, R92C_RX_IQK, 0x01004800);

	for (i = 0; i < retry; i++) {
		path_a_ok = r88f_iqk_path_a_tx(sc, &lok_result, tx_result);
		if (path_a_ok) {
			rtwn_bb_write(sc, R92C_FPGA0_IQK, 0x00);
			break;
		}
	}

	for (i = 0; i < retry; i++) {
		path_a_ok = r88f_iqk_path_a_tx_rx(sc, lok_result, rx_result);
		if (path_a_ok) {
			rtwn_bb_write(sc, R92C_FPGA0_IQK, 0x00);
			break;
		}
	}

	if (!path_a_ok)
		RTWN_DPRINTF(sc, RTWN_DEBUG_CALIB,
			"%s: Path A IQK failed!\n", __func__);

	/* Back to BB mode, load original value */
	reg = rtwn_bb_read(sc, R92C_FPGA0_IQK);
	reg &= 0xff;
	rtwn_bb_write(sc, R92C_FPGA0_IQK, reg);

	if (t == 0)
		return;

	if (!(regs->hssi_param1 & R92C_HSSI_PARAM1_PI)) {
		/*
		 * Switch back BB to SI mode after finishing
		 * IQ Calibration
		 */
		rtwn_bb_write(sc, R92C_HSSI_PARAM1(0), 0x01000000);
		rtwn_bb_write(sc, R92C_HSSI_PARAM1(1), 0x01000000);
	}

	/* Reload ADDA power saving, MAC and BB paramters */
	r88f_iqk_restore_regs(sc, regs);

	/* Reload RF path */
	rtwn_bb_write(sc, R23B_ANT_SEL, path_sel_bb);
	rtwn_rf_write(sc, 0, R23B_RF_S0S1, path_sel_rf);

	/* Restore RX initial gain */
	reg = rtwn_bb_read(sc, R92C_OFDM0_AGCCORE1(0));
	reg &= 0xffffff00;
	reg |= 0x50;
	rtwn_bb_write(sc, R92C_OFDM0_AGCCORE1(0), reg);
	reg = rtwn_bb_read(sc, R92C_OFDM0_AGCCORE1(0));
	reg &= 0xffffff00;
	reg |= rx_initial_gain & 0xff;
	rtwn_bb_write(sc, R92C_OFDM0_AGCCORE1(0), reg);

	/* Load 0xe30 IQC default value */
	rtwn_bb_write(sc, R92C_TX_IQK_TONE(0), 0x01008c00);
	rtwn_bb_write(sc, R92C_RX_IQK_TONE(0), 0x01008c00);
}

void
r88f_iq_calib(struct rtwn_softc *sc)
{
	struct r88f_iqk_calib_regs regs;
	struct r88f_iqk_result tx_result[3] = { {0, 0} };
	struct r88f_iqk_result rx_result[3] = { {0, 0} };
	/* Final results */
	struct r88f_iqk_result *tx_candidate = NULL;
	struct r88f_iqk_result *rx_candidate = NULL;
	uint32_t path_sel_bb, path_sel_rf;
	int i;

	/* Save RF path */
	path_sel_bb = rtwn_bb_read(sc, R23B_ANT_SEL);
	path_sel_rf = rtwn_rf_read(sc, 0, R23B_RF_S0S1);

	for (i = 0; i < 3; i++) {
		r88f_iq_calib_run(sc, &regs, &tx_result[i], &rx_result[i], i);

		if (i == 1) {
			/* Compare 0 vs 1 */
			if (r88f_iq_calib_compare_results(&tx_result[0],
				    &rx_result[0],
				    &tx_result[1], &rx_result[1],
				    &tx_candidate, &rx_candidate)) {
				RTWN_DPRINTF(sc, RTWN_DEBUG_CALIB,
				    "%s: Run 0-1 passed\n", __func__);
				goto write;
			}
		}

		if (i == 2) {
			/* Compare 0 vs 2 */
			if (r88f_iq_calib_compare_results(&tx_result[0],
				    &rx_result[0], &tx_result[2],
				    &rx_result[2], &tx_candidate,
				    &rx_candidate)) {
				RTWN_DPRINTF(sc, RTWN_DEBUG_CALIB,
				    "%s: Run 0-2 passed\n", __func__);
				goto write;
			}

			/* Compare 1 vs 2 */
			if (r88f_iq_calib_compare_results(&tx_result[1],
				    &rx_result[1], &tx_result[2],
				    &rx_result[2], &tx_candidate,
				    &rx_candidate)) {
				RTWN_DPRINTF(sc, RTWN_DEBUG_CALIB,
				    "%s: Run 1-2 passed\n", __func__);
			} else {
				if (tx_candidate == NULL)
					tx_candidate = &tx_result[0];
				if (rx_candidate == NULL)
					rx_candidate = &rx_result[0];

				/* check complete failure */
				if (tx_candidate->before == 0 &&
					tx_candidate->after == 0 &&
					rx_candidate->before == 0 &&
					rx_candidate->after == 0) {
				RTWN_DPRINTF(sc, RTWN_DEBUG_CALIB,
				    "%s: IQK failed completely\n", __func__);
					goto done;
				}

				RTWN_DPRINTF(sc, RTWN_DEBUG_CALIB,
				    "%s: using fallback candidate\n", __func__);
			}
		}

		RTWN_DPRINTF(sc, RTWN_DEBUG_CALIB,
		    "IQK run %d: TX b=%#x a=%#x RX b=%#x a=%#x\n", i,
		    tx_result[i].before, tx_result[i].after,
		    rx_result[i].before, rx_result[i].after);
	}

write:

	r88f_fill_iqk_matrix_a(sc, tx_candidate, rx_candidate ?
	    rx_candidate : NULL,
	    (rx_candidate == NULL || rx_candidate->before == 0));

	if (tx_candidate == NULL || (tx_candidate->before == 0 &&
		tx_candidate->after == 0)) {
		RTWN_DPRINTF(sc, RTWN_DEBUG_CALIB,
		    "%s: TX IQK failed completely\n", __func__);
		goto done;
	}

	RTWN_DPRINTF(sc, RTWN_DEBUG_CALIB,
	       "%s: IQK candidate - TX: before=%#x after=%#x RX: before=%#x after=%#x\n",
	    __func__, tx_candidate->before, tx_candidate->after,
	    rx_candidate ? rx_candidate->before : 0,
	    rx_candidate ? rx_candidate->after : 0);

done:
	r88f_iqk_save_regs(sc, &regs);

	/* Restore RF path */
	rtwn_bb_write(sc, R23B_ANT_SEL, path_sel_bb);
	rtwn_rf_write(sc, 0, R23B_RF_S0S1, path_sel_rf);
}
