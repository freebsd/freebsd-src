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

#include <dev/rtwn/if_rtwn_debug.h>

#include <dev/rtwn/rtl8188e/r88e.h>
#include <dev/rtwn/rtl8188e/r88e_reg.h>

#include <dev/rtwn/rtl8192c/r92c_reg.h>

#include <dev/rtwn/rtl8192e/r92e_var.h>

#include <dev/rtwn/rtl8723b/r23b.h>
#include <dev/rtwn/rtl8723b/r23b_var.h>
#include <dev/rtwn/rtl8723b/r23b_reg.h>
#include <dev/rtwn/rtl8723b/r23b_fw_cmd.h>

struct r23b_iqk_reg_vals {
	uint32_t adaa[16];
	uint32_t iqk_bb[9];
	uint8_t txpause;
	uint8_t bcn_ctrl[2];
	uint8_t gpio_muxcfg;
};

struct r23b_iqk_result {
	int32_t iqk_before;
	int32_t iqk_after;
};

enum r23b_iqk_stage {
	R23B_IQK_TX,
	R23B_IQK_TX_RX,
	R23B_IQK_RX
};

static const uint16_t r23b_iq_calib_reg_adaa[16] = {
	0x85c, 0xe6c, 0xe70, 0xe74,
	0xe78, 0xe7c, 0xe80, 0xe84,
	0xe88, 0xe8c, 0xed0, 0xed4,
	0xed8, 0xedc, 0xee0, 0xeec
};

static const uint16_t r23b_iq_calib_reg_iqk_bb[9] = {
	0xc04, 0xc08, 0x874, 0xb68,
	0xb6c, 0x870, 0x860, 0x864,
	0xa04
};

#define R23B_IQK_MAX_TOLERANCE	5
#define R23B_IQK_SIGN(r)	(((r) & 0x200) ? (0xfffffc00 | (r)) : (r))

/* XXX
 * Same as r92c_lc_calib except for LDO on and LDO off
 */
void
r23b_lc_calib(struct rtwn_softc *sc)
{
	uint32_t rf_ac[2];
	uint8_t txmode;
	int i;

	txmode = rtwn_read_1(sc, R92C_OFDM1_LSTF + 3);
	if ((txmode & 0x70) != 0) {
		/* Disable all continuous Tx. */
		rtwn_write_1(sc, R92C_OFDM1_LSTF + 3, txmode & ~0x70);

		/* Set RF mode to standby mode. */
		for (i = 0; i < sc->nrxchains; i++) {
			rf_ac[i] = rtwn_rf_read(sc, i, R92C_RF_AC);
			rtwn_rf_write(sc, i, R92C_RF_AC,
			    RW(rf_ac[i], R92C_RF_AC_MODE,
				R92C_RF_AC_MODE_STANDBY));
		}
	} else {
		/* Block all Tx queues. */
		rtwn_write_1(sc, R92C_TXPAUSE, R92C_TX_QUEUE_ALL);
	}

	/* LDO on */
	rtwn_rf_write(sc, 0, R23B_RF_S0S1, 0xdfbe0);

	/* Start calibration. */
	rtwn_rf_setbits(sc, 0, R92C_RF_CHNLBW, 0, R92C_RF_CHNLBW_LCSTART);

	/* Give calibration the time to complete. */
	rtwn_delay(sc, 100000);	/* 100ms */

	/* LDO off */
	rtwn_rf_write(sc, 0, R23B_RF_S0S1, 0xdffe0);

	/* Restore configuration. */
	if ((txmode & 0x70) != 0) {
		/* Restore Tx mode. */
		rtwn_write_1(sc, R92C_OFDM1_LSTF + 3, txmode);
		/* Restore RF mode. */
		for (i = 0; i < sc->nrxchains; i++)
			rtwn_rf_write(sc, i, R92C_RF_AC, rf_ac[i]);
	} else {
		/* Unblock all Tx queues. */
		rtwn_write_1(sc, R92C_TXPAUSE, 0x00);
	}
}

static void
r23b_iq_calib_chain(struct rtwn_softc *sc, enum r23b_iqk_stage stage)
{
	struct r23b_softc *rs = sc->sc_priv;
	uint32_t rf_path;

	RTWN_DPRINTF(sc, RTWN_DEBUG_CALIB, "%s: starting stage: %s\n",
	    __func__, stage == R23B_IQK_TX ? "R23B_IQK_TX" :
	        stage == R23B_IQK_TX_RX ? "R23B_IQK_TX_RX" : "R23B_IQK_RX");

	/* Save RF path. */
	rf_path = rtwn_read_4(sc, R23B_ANT_SEL);

	/* Leave IQK mode. */
	rtwn_setbits_4(sc, R92C_FPGA0_IQK, ~0xff, 0);

	rtwn_rf_setbits(sc, 0, R88E_RF_WE_LUT, 0, 0x80000);
	switch (stage) {
	case R23B_IQK_TX:
		rtwn_rf_write(sc, 0, R92C_RF_RCK_OS, 0x20000);
		rtwn_rf_write(sc, 0, R92C_RF_TXPA_G(0), 0x0003f);
		rtwn_rf_write(sc, 0, R92C_RF_TXPA_G(1), 0xc7f87);

		rtwn_write_4(sc, R92C_TX_IQK, 0x01007c00);
		break;
	case R23B_IQK_TX_RX:
		rtwn_rf_write(sc, 0, R92C_RF_RCK_OS, 0x30000);
		rtwn_rf_write(sc, 0, R92C_RF_TXPA_G(0), 0x0001f);
		rtwn_rf_write(sc, 0, R92C_RF_TXPA_G(1), 0xf7fb7);

		rtwn_write_4(sc, R92C_TX_IQK, 0x01007c00);
		break;
	case R23B_IQK_RX:
		rtwn_rf_write(sc, 0, R92C_RF_RCK_OS, 0x30000);
		rtwn_rf_write(sc, 0, R92C_RF_TXPA_G(0), 0x0001f);
		rtwn_rf_write(sc, 0, R92C_RF_TXPA_G(1), 0xf7d77);

		rtwn_rf_write(sc, 0, 0xdf, 0xf80);
		rtwn_rf_write(sc, 0, 0x55, 0x4021f);
		break;
	default:
		KASSERT(0, ("%s: Invalid stage %d", __func__, stage));
	}

	rtwn_write_4(sc, R92C_RX_IQK, 0x01004800);

	switch (stage) {
	case R23B_IQK_TX:
	case R23B_IQK_TX_RX:
		rtwn_write_4(sc, R92C_TX_IQK_TONE(0), 0x18008c1c);
		rtwn_write_4(sc, R92C_RX_IQK_TONE(0), 0x38008c1c);
		break;
	case R23B_IQK_RX:
		rtwn_write_4(sc, R92C_TX_IQK_TONE(0), 0x38008c1c);
		rtwn_write_4(sc, R92C_RX_IQK_TONE(0), 0x18008c1c);
		break;
	}
	rtwn_write_4(sc, R92C_TX_IQK_TONE(1), 0x38008c1c);
	rtwn_write_4(sc, R92C_RX_IQK_TONE(1), 0x38008c1c);

	switch (stage) {
	case R23B_IQK_TX:
		rtwn_write_4(sc, R92C_TX_IQK_PI(0), 0x821403ea);
		rtwn_write_4(sc, R92C_RX_IQK_PI(0), 0x28110000);
		break;
	case R23B_IQK_TX_RX:
		rtwn_write_4(sc, R92C_TX_IQK_PI(0), 0x82160ff0);
		rtwn_write_4(sc, R92C_RX_IQK_PI(0), 0x28110000);
		break;
	case R23B_IQK_RX:
		rtwn_write_4(sc, R92C_TX_IQK_PI(0), 0x82110000);
		rtwn_write_4(sc, R92C_RX_IQK_PI(0), 0x2816001f);
		break;
	}

	rtwn_write_4(sc, R92C_TX_IQK_PI(1), 0x82110000);
	rtwn_write_4(sc, R92C_RX_IQK_PI(1), 0x28110000);

	switch (stage) {
	case R23B_IQK_TX:
		rtwn_write_4(sc, R92C_IQK_AGC_RSP, 0x00462911);
		break;
	case R23B_IQK_TX_RX:
		rtwn_write_4(sc, R92C_IQK_AGC_RSP, 0x0046a911);
		break;
	case R23B_IQK_RX:
		rtwn_write_4(sc, R92C_IQK_AGC_RSP, 0x0046a8d1);
		break;
	}

	/* Enter IQK mode. */
	rtwn_setbits_4(sc, R92C_FPGA0_IQK, ~0xff, 0x80800000);

	rtwn_write_4(sc, R23B_ANT_SEL, rs->bt_antnum == 0 ?
	    R23B_ANT_SEL_S0 : R23B_ANT_SEL_S1);

	/* GNT_BT = 0 */
	rtwn_write_4(sc, R23B_GNT_BT, 0x00000800);

	/* We're doing LO and IQ calibration in one shot. */
	rtwn_write_4(sc, R92C_IQK_AGC_PTS, 0xf9000000);
	rtwn_write_4(sc, R92C_IQK_AGC_PTS, 0xf8000000);

	rtwn_delay(sc, 20000); /* 20ms */

	/* Restore RF path. */
	rtwn_write_4(sc, R23B_ANT_SEL, rf_path);

	/* GNT_BT = 1 */
	rtwn_write_4(sc, R23B_GNT_BT, 0x00001800);

	/* Leave IQK mode. */
	rtwn_setbits_4(sc, R92C_FPGA0_IQK, ~0xff, 0);
}

static bool
r23b_iq_calib_get_tx_result(struct rtwn_softc *sc,
    struct r23b_iqk_result *tx_result)
{
	uint32_t val;

	val = rtwn_read_4(sc, R92C_RX_POWER_IQK_AFTER(0));
	RTWN_DPRINTF(sc, RTWN_DEBUG_CALIB, "%s: rx_after (%#x): %#x\n",
	    __func__, R92C_RX_POWER_IQK_AFTER(0), val);
	if (val & 0x10000000)
		return (false);

	val = rtwn_read_4(sc, R92C_TX_POWER_IQK_BEFORE(0));
	RTWN_DPRINTF(sc, RTWN_DEBUG_CALIB, "%s: tx_before (%#x): %#x\n",
	    __func__, R92C_TX_POWER_IQK_BEFORE(0), val);
	val = MS(val, R92C_POWER_IQK_RESULT);
	if (val == 0x142 || val >= 0x110 || val <= 0xf0)
		return (false);
	tx_result->iqk_before = val;

	val = rtwn_read_4(sc, R92C_TX_POWER_IQK_AFTER(0));
	RTWN_DPRINTF(sc, RTWN_DEBUG_CALIB, "%s: tx_after (%#x): %#x\n",
	    __func__, R92C_TX_POWER_IQK_AFTER(0), val);

	val = MS(val, R92C_POWER_IQK_RESULT);
	if (val == 0x42)
		return (false);
	tx_result->iqk_after = val;

	if ((val & 0x200) != 0)
		val = 0x400 - val;
	return (val < 0xf);
}

static bool
r23b_iq_calib_get_rx_result(struct rtwn_softc *sc,
    struct r23b_iqk_result *rx_result)
{
	uint32_t iqk_before, iqk_after, val;

	iqk_after = rtwn_read_4(sc, R92C_RX_POWER_IQK_AFTER(0));
	RTWN_DPRINTF(sc, RTWN_DEBUG_CALIB, "%s: rx_after (%#x): %#x\n",
	    __func__, R92C_RX_POWER_IQK_AFTER(0), iqk_after);

	iqk_after = MS(iqk_after, R92C_POWER_IQK_RESULT);

	iqk_before = rtwn_read_4(sc, R92C_RX_POWER_IQK_BEFORE(0));
	RTWN_DPRINTF(sc, RTWN_DEBUG_CALIB, "%s: rx_before (%#x): %#x\n",
	    __func__, R92C_RX_POWER_IQK_BEFORE(0), iqk_before);

	iqk_before = MS(iqk_before, R92C_POWER_IQK_RESULT);

	/* Leave IQK mode. */
	rtwn_setbits_4(sc, R92C_FPGA0_IQK, ~0xff, 0);
	rtwn_rf_write(sc, 0, 0xdf, 0x780);

	val = (iqk_after & 0x200) != 0 ? 0x400 - iqk_after : iqk_after;
	if (iqk_after & 0x8000000)
		return (false);

	if (iqk_before == 0x132 || iqk_before >= 0x110 || iqk_before <= 0xf0)
		return (false);

	if (iqk_after == 0x36)
		return (false);

	if (val >= 0xf)
		return (false);

	rx_result->iqk_before = iqk_before;
	rx_result->iqk_after = iqk_after;
	return (true);
}

static inline void
r23b_iq_calib_save_regs(struct rtwn_softc *sc,
    struct r23b_iqk_reg_vals *vals)
{
	for (int i = 0; i < nitems(r23b_iq_calib_reg_adaa); i++)
		vals->adaa[i] = rtwn_read_4(sc, r23b_iq_calib_reg_adaa[i]);

	vals->txpause = rtwn_read_1(sc, R92C_TXPAUSE);

	for (int i = 0; i < sizeof(vals->bcn_ctrl); i++)
		vals->bcn_ctrl[i] = rtwn_read_1(sc, R92C_BCN_CTRL(i));

	vals->gpio_muxcfg = rtwn_read_4(sc, R92C_GPIO_MUXCFG);

	for (int i = 0; i < nitems(r23b_iq_calib_reg_iqk_bb); i++)
		vals->iqk_bb[i] = rtwn_read_4(sc, r23b_iq_calib_reg_iqk_bb[i]);
}

static inline void
r23b_iq_calib_restore_regs(struct rtwn_softc *sc,
    const struct r23b_iqk_reg_vals *vals)
{
	for (int i = 0; i < nitems(r23b_iq_calib_reg_adaa); i++)
		rtwn_write_4(sc, r23b_iq_calib_reg_adaa[i], vals->adaa[i]);

	rtwn_write_1(sc, R92C_TXPAUSE, vals->txpause);

	for (int i = 0; i < sizeof(vals->bcn_ctrl); i++)
		rtwn_write_1(sc, R92C_BCN_CTRL(i), vals->bcn_ctrl[i]);

	rtwn_write_4(sc, R92C_GPIO_MUXCFG, vals->gpio_muxcfg);

	for (int i = 0; i < nitems(r23b_iq_calib_reg_iqk_bb); i++)
		rtwn_write_4(sc, r23b_iq_calib_reg_iqk_bb[i], vals->iqk_bb[i]);
}

static void
r23b_iq_calib_run(struct rtwn_softc *sc, struct r23b_iqk_result *tx_result,
    struct r23b_iqk_result *rx_result, const struct r23b_iqk_reg_vals *vals)
{
	struct r23b_softc *rs = sc->sc_priv;

	for (int i = 0; i < nitems(r23b_iq_calib_reg_adaa); i++)
		rtwn_write_4(sc, r23b_iq_calib_reg_adaa[i], 0x01c00014);

	/* MAC settings */

	rtwn_write_1(sc, R92C_TXPAUSE, R92C_TX_QUEUE_AC |
	    R92C_TX_QUEUE_MGT | R92C_TX_QUEUE_HIGH);

	for (int i = 0; i < sizeof(vals->bcn_ctrl); i++) {
		rtwn_write_1(sc, R92C_BCN_CTRL(i),
		    vals->bcn_ctrl[i] & ~R92C_BCN_CTRL_EN_BCN);
	}

	rtwn_write_1(sc, R92C_GPIO_MUXCFG,
	    vals->gpio_muxcfg & ~R92C_GPIO_MUXCFG_ENBT);

	/* BB settings */

	rtwn_setbits_4(sc, R92C_CCK0_AFESETTING, 0, 0x0f000000);
	rtwn_write_4(sc, R92C_OFDM0_TRXPATHENA, 0x03a05600);
	rtwn_write_4(sc, R92C_OFDM0_TRMUXPAR, 0x000800e4);
	rtwn_write_4(sc, R92C_FPGA0_RFIFACESW(1), 0x22204000);

	/* IQ calibration settings */

	rtwn_setbits_4(sc, R92C_FPGA0_IQK, ~0xff, 0);
	rtwn_rf_setbits(sc, 0, R88E_RF_WE_LUT, 0, 0x80000);
	rtwn_rf_write(sc, 0, R92C_RF_RCK_OS, 0x30000);
	rtwn_rf_write(sc, 0, R92C_RF_TXPA_G(0), 0x0001f);
	rtwn_rf_write(sc, 0, R92C_RF_TXPA_G(1), 0xf7fb7);
	rtwn_rf_setbits(sc, 0, 0xed, 0, 0x20);
	rtwn_rf_write(sc, 0, 0x43, 0x60fbd);

	/* Path A Tx IQ calibration. */
	for (int retry = 0; retry < 2; retry++) {
		r23b_iq_calib_chain(sc, R23B_IQK_TX);

		if (r23b_iq_calib_get_tx_result(sc, tx_result)) {
			rtwn_setbits_4(sc, R92C_FPGA0_IQK, ~0xff, 0);
			break;
		}

		RTWN_DPRINTF(sc, RTWN_DEBUG_CALIB,
		    "%s: Tx failed, retry=%d\n", __func__, retry);
	}

	/* Path A Rx IQ calibration. */
	for (int retry = 0; retry < 2; retry++) {
		struct r23b_iqk_result result;

		r23b_iq_calib_chain(sc, R23B_IQK_TX_RX);
		if (!r23b_iq_calib_get_tx_result(sc, &result)) {
			RTWN_DPRINTF(sc, RTWN_DEBUG_CALIB,
			    "%s: Rx (Tx) failed, retry=%d\n", __func__, retry);
			continue;
		}

		RTWN_DPRINTF(sc, RTWN_DEBUG_CALIB,
		    "%s: writing %#x to TX_IQK\n", __func__,
		    0x80007c00 | (result.iqk_before << 16) | result.iqk_after);

		rtwn_write_4(sc, R92C_TX_IQK,
		    0x80007c00 | (result.iqk_before << 16) | result.iqk_after);

		r23b_iq_calib_chain(sc, R23B_IQK_RX);
		if (!r23b_iq_calib_get_rx_result(sc, rx_result)) {
			RTWN_DPRINTF(sc, RTWN_DEBUG_CALIB,
			    "%s: Rx failed, retry=%d\n", __func__, retry);
			continue;
		}

		break;
	}

	if (rs->bt_antnum == 0) {
		RTWN_DPRINTF(sc, RTWN_DEBUG_CALIB,
		    "%s: path B IQK not implemented!\n", __func__);
	}

	rtwn_setbits_4(sc, R92C_FPGA0_IQK, ~0xff, 0);
}

static bool
r23b_iq_calib_compare_results(
    struct r23b_iqk_result *tx_result1, struct r23b_iqk_result *rx_result1,
    struct r23b_iqk_result *tx_result2, struct r23b_iqk_result *rx_result2,
    struct r23b_iqk_result **tx_candidate,
    struct r23b_iqk_result **rx_candidate) {
	uint32_t diff;
	bool tx_good = false;

	diff = abs(tx_result1->iqk_before - tx_result2->iqk_before);
	if (diff > R23B_IQK_MAX_TOLERANCE)
		goto rx_compare;

	diff = abs(R23B_IQK_SIGN(tx_result1->iqk_after) -
	    R23B_IQK_SIGN(tx_result2->iqk_after));
	if (diff <= R23B_IQK_MAX_TOLERANCE) {
		*tx_candidate = tx_result1;
		tx_good = true;
	}

rx_compare:
	diff = abs(rx_result1->iqk_before - rx_result2->iqk_before);
	if (diff > R23B_IQK_MAX_TOLERANCE) {
		if (tx_good &&
		    rx_result1->iqk_before + rx_result1->iqk_after == 0)
			*tx_candidate = tx_result2;

		return (false);
	}

	diff = abs(R23B_IQK_SIGN(rx_result1->iqk_after) -
	    R23B_IQK_SIGN(rx_result2->iqk_after));
	if (diff > R23B_IQK_MAX_TOLERANCE)
		return (false);

	*rx_candidate = rx_result1;
	return (tx_good);
}

void
r23b_iq_calib(struct rtwn_softc *sc)
{
	struct r23b_iqk_reg_vals vals;
	struct r23b_iqk_result tx_result[3] = { 0 };
	struct r23b_iqk_result rx_result[3] = { 0 };
	struct r23b_iqk_result *tx_candidate = NULL, *rx_candidate = NULL;
	uint32_t oldval, val;
	uint32_t gnt_bt;

#ifndef RTWN_WITHOUT_UCODE
	/* Inform firmware that we're begininng IQK. */
	r88e_fw_cmd(sc, R23B_CMD_WLAN_CALIB, R23B_CMD_WLAN_CALIB_BEGIN,
	    sizeof(R23B_CMD_WLAN_CALIB_BEGIN));
#endif

	/* Save GNT_BT. */
	gnt_bt = rtwn_read_4(sc, R23B_GNT_BT);

	r23b_iq_calib_save_regs(sc, &vals);
	r23b_iq_calib_run(sc, &tx_result[0], &rx_result[0], &vals);
	r23b_iq_calib_run(sc, &tx_result[1], &rx_result[1], &vals);
	r23b_iq_calib_restore_regs(sc, &vals);

	if (r23b_iq_calib_compare_results(&tx_result[0], &rx_result[0],
	    &tx_result[1], &rx_result[1], &tx_candidate, &rx_candidate)) {
		RTWN_DPRINTF(sc, RTWN_DEBUG_CALIB,
		    "%s: comparing 0 and 1 succeeded\n", __func__);
		goto write;
	}

	r23b_iq_calib_run(sc, &tx_result[2], &rx_result[2], &vals);
	r23b_iq_calib_restore_regs(sc, &vals);

	if (r23b_iq_calib_compare_results(&tx_result[0], &rx_result[0],
	    &tx_result[2], &rx_result[2], &tx_candidate, &rx_candidate)) {
		RTWN_DPRINTF(sc, RTWN_DEBUG_CALIB,
		    "%s: comparing 0 and 2 succeeded\n", __func__);
		goto write;
	}

	if (!r23b_iq_calib_compare_results(&tx_result[1], &rx_result[1],
	    &tx_result[2], &rx_result[2], &tx_candidate, &rx_candidate)) {
		/* If no candidate was assigned, set the result to zero. */
		if (tx_candidate == NULL) {
			tx_result[0] = (struct r23b_iqk_result){ 0 };
			tx_candidate = &tx_result[0];
		}

		if (rx_candidate == NULL) {
			rx_result[0] = (struct r23b_iqk_result){ 0 };
			rx_candidate = &rx_result[0];
		}

		if (tx_candidate->iqk_before == 0 &&
		    tx_candidate->iqk_after == 0 &&
		    rx_candidate->iqk_before == 0 &&
		    rx_candidate->iqk_after == 0) {
			RTWN_DPRINTF(sc, RTWN_DEBUG_CALIB,
			    "%s: IQK failed\n", __func__);
			goto done;
		}

		RTWN_DPRINTF(sc, RTWN_DEBUG_CALIB,
		    "%s: comparison failed but a candidate was assigned\n",
		    __func__);
	} else {
		RTWN_DPRINTF(sc, RTWN_DEBUG_CALIB,
		    "%s: comparing 1 and 2 succeeded\n", __func__);
	}

write:
	RTWN_DPRINTF(sc, RTWN_DEBUG_CALIB, "%s: IQK candidate dump:\n"
	    "\ttx_candidate->iqk_before 0x%x\ttx_candidate->iqk_after 0x%x\n"
	    "\trx_candidate->iqk_before 0x%x\trx_candidate->iqk_after 0x%x\n",
	    __func__, tx_candidate->iqk_before, tx_candidate->iqk_after,
	    rx_candidate->iqk_before, rx_candidate->iqk_after);

	/*
	 * Don't write both the tx and rx configuration
	 * values if tx IQK failed.
	 */
	if (tx_candidate->iqk_before == 0)
		goto done;

	/* Write path A values */

	oldval = rtwn_read_4(sc, R92C_OFDM0_TXIQIMBALANCE(0));
	oldval = (oldval >> 22) & 0x3ff;

	val = (R23B_IQK_SIGN(tx_candidate->iqk_before) * oldval) >> 7;
	rtwn_setbits_4(sc, R92C_OFDM0_TXIQIMBALANCE(0), 0x3ff,
	    (val >> 1) & 0x3ff);
	rtwn_setbits_4(sc, R92C_OFDM0_ECCATHRESHOLD, 0x80000000, val << 31);

	val = (R23B_IQK_SIGN(tx_candidate->iqk_after) * oldval) >> 7;
	rtwn_setbits_4(sc, R92C_TX_POWER_IQK_BEFORE(0),
	    0xf0000000, (((val >> 1) & 0x3c0) >> 6) << 28);
	rtwn_setbits_4(sc, R92C_OFDM0_TXIQIMBALANCE(0),
	    0x3f0000, ((val >> 1) & 0x3f) << 16);
	rtwn_setbits_4(sc, R92C_OFDM0_ECCATHRESHOLD,
	    0x10000000, (val << 29) & 0x1);

	/* Skip the rx configuration values if rx IQK failed. */
	if (rx_candidate->iqk_before == 0)
		goto done;

	val = rx_candidate->iqk_after & 0x3f;
	rtwn_setbits_4(sc, R92C_OFDM0_RXIQIMBALANCE(0), 0x3ff,
	    rx_candidate->iqk_before);
	rtwn_setbits_4(sc, R92C_OFDM0_RXIQIMBALANCE(0), 0xfc00, val << 10);

	val = (rx_candidate->iqk_after >> 6) & 0xf;
	rtwn_setbits_4(sc, R92C_OFDM0_RXIQEXTANTA, 0xf0000000, val << 28);
done:
	/* Restore GNT_BT. */
	rtwn_write_4(sc, R23B_GNT_BT, gnt_bt);

	rtwn_rf_setbits(sc, 0, R88E_RF_WE_LUT, 0, 0x80000);
	rtwn_rf_write(sc, 0, R92C_RF_RCK_OS, 0x18000);
	rtwn_rf_write(sc, 0, R92C_RF_TXPA_G(0), 0x0001f);
	rtwn_rf_write(sc, 0, R92C_RF_TXPA_G(0), 0xe6177);
	rtwn_rf_setbits(sc, 0, 0xed, 0, 0x20);
	rtwn_rf_write(sc, 0, 0x43, 0x300bd);

#ifndef RTWN_WITHOUT_UCODE
	/* Inform firmware that we're done. */
	r88e_fw_cmd(sc, R23B_CMD_WLAN_CALIB, R23B_CMD_WLAN_CALIB_END,
	    sizeof(R23B_CMD_WLAN_CALIB_END));
#endif
}

void
r23b_temp_measure(struct rtwn_softc *sc)
{
	rtwn_rf_setbits(sc, 0, R88E_RF_T_METER, 0, R88E_RF_T_METER_START);
}
