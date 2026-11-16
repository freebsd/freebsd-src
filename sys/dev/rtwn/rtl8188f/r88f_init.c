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

#include <dev/rtwn/rtl8192c/r92c.h>
#include <dev/rtwn/rtl8192c/r92c_priv.h>
#include <dev/rtwn/rtl8192c/r92c_reg.h>
#include <dev/rtwn/rtl8192c/r92c_var.h>

#include <dev/rtwn/rtl8192e/r92e_reg.h>

#include <dev/rtwn/rtl8812a/r12a_reg.h>

#include <dev/rtwn/rtl8188f/r88f.h>
#include <dev/rtwn/rtl8188f/r88f_reg.h>

int
r88f_llt_init(struct rtwn_softc *sc)
{
	int ntries;

	rtwn_setbits_4(sc, R92E_AUTO_LLT, 0, R92E_AUTO_LLT_EN);
	for (ntries = 0; ntries < 1000; ntries++) {
		if (!(rtwn_read_4(sc, R92E_AUTO_LLT) & R92E_AUTO_LLT_EN))
			return (0);
		rtwn_delay(sc, 2);
	}
	return (ETIMEDOUT);
}

int
r88f_set_page_size(struct rtwn_softc *sc)
{
	return (rtwn_write_1(sc, R92C_PBP,
	    SM(R92C_PBP_PSRX, R92C_PBP_256) |
	    SM(R92C_PBP_PSTX, R92C_PBP_256)));
}

void
r88f_init_ampdu(struct rtwn_softc *sc)
{
	/* Setup AMPDU aggregation. */
	rtwn_setbits_1(sc, R12A_HT_SINGLE_AMPDU, 0,
	    R12A_HT_SINGLE_AMPDU_PKT_ENA);
	rtwn_write_2(sc, R92C_MAX_AGGR_NUM, 0x0c14);
	rtwn_write_1(sc, R12A_AMPDU_MAX_TIME, 0x70);
	rtwn_write_4(sc, R92C_AGGLEN_LMT, 0xffffffff);

	/* For VHT packet length 11K */
	rtwn_write_1(sc, R92C_RX_PKT_LIMIT, 0x18);

	rtwn_write_1(sc, R92C_PIFS, 0);
	rtwn_write_1(sc, R92C_FWHW_TXQ_CTRL, 0x80);
	rtwn_write_4(sc, R92C_FAST_EDCA_CTRL, 0x03086666);
	rtwn_write_1(sc, R92C_USTIME_TSF, 0x28);
	rtwn_write_1(sc, R92C_USTIME_EDCA, 0x28);

	/* To prevent bus resetting the mac. */
	rtwn_setbits_1(sc, R92C_RSV_CTRL, 0,
	    R92C_RSV_CTRL_R_DIS_PRST_0 | R92C_RSV_CTRL_R_DIS_PRST_1);
}

static int
r88f_read_efuse_1(struct rtwn_softc *sc, uint16_t offset, uint8_t *data)
{
	int i;
	uint8_t val8;
	uint32_t reg;

	/* Write Address */
	rtwn_write_1(sc, R92C_EFUSE_CTRL + 1, offset & 0xff);
	val8 = rtwn_read_1(sc, R92C_EFUSE_CTRL + 2);
	val8 &= 0xfc;
	val8 |= (offset >> 8) & 0x03;
	rtwn_write_1(sc, R92C_EFUSE_CTRL + 2, val8);

	val8 = rtwn_read_1(sc, R92C_EFUSE_CTRL + 3);
	rtwn_write_1(sc, R92C_EFUSE_CTRL + 3, val8 & 0x7f);

	/* Poll for data read */
	reg = rtwn_bb_read(sc, R92C_EFUSE_CTRL);
	for (i = 0; i < 500; i++) {
		reg = rtwn_bb_read(sc, R92C_EFUSE_CTRL);
		if (reg & 0x80000000)
			break;
	}

	if (i == 500)
		return (-EIO);

	rtwn_delay(sc, 50);
	reg = rtwn_bb_read(sc, R92C_EFUSE_CTRL);

	*data = reg & 0xff;
	return (0);
}

#define PPG_BB_GAIN_2G_TXA_OFFSET_8188F 0xee
#define PPG_BB_GAIN_2G_TX_OFFSET_MASK 0x0f
#define OFDM_RF_PATH_RX_A		0x00000001
#define OFDM_RF_PATH_TX_A		0x00000010
#define OFDM_RF_PATH_TX_MASK		0xf0
#define OFDM_RF_PATH_RX_MASK		0x0f

static void
r88f_enable_rf(struct rtwn_softc *sc)
{
	uint8_t pg_pwrtrim = 0xff, val8;
	int8_t bb_gain;

	/* Somehow this is not found in the efuse we read earlier. */
	r88f_read_efuse_1(sc, PPG_BB_GAIN_2G_TXA_OFFSET_8188F, &pg_pwrtrim);

	if (pg_pwrtrim != 0xff) {
		bb_gain = pg_pwrtrim & PPG_BB_GAIN_2G_TX_OFFSET_MASK;

		if (bb_gain == PPG_BB_GAIN_2G_TX_OFFSET_MASK)
			bb_gain = 0;
		else if (bb_gain & 1)
			bb_gain = bb_gain >> 1;
		else
			bb_gain = -(bb_gain >> 1);

		val8 = abs(bb_gain);
		if (bb_gain > 0)
			val8 |= 0x00000020;

		rtwn_rf_setbits(sc, 0, R88F_RF_UNKNOWN_55,
		    0xfc000, val8 << 14);
	}

	rtwn_write_1(sc, R92C_RF_CTRL, R92C_RF_CTRL_EN |
	    R92C_RF_CTRL_RSTB | R92C_RF_CTRL_SDMRSTB);
	rtwn_bb_setbits(sc, R92C_OFDM0_TRXPATHENA, OFDM_RF_PATH_RX_MASK |
	    OFDM_RF_PATH_TX_MASK, OFDM_RF_PATH_RX_A | OFDM_RF_PATH_TX_A);

	rtwn_write_1(sc, R92C_TXPAUSE, 0x00);
}

void
r88f_init_rf(struct rtwn_softc *sc)
{
	struct r92c_softc *rs = sc->sc_priv;
	uint32_t reg, type;
	int i = 0;

	r88f_enable_rf(sc);

	/* Save RF_ENV control type. */
	reg = rtwn_bb_read(sc, R92C_FPGA0_RFIFACESW(0));
	type = reg & 0x10;

	/* Set RF_ENV enable. */
	rtwn_bb_setbits(sc, R92C_FPGA0_RFIFACEOE(0), 0, 0x100000);
	rtwn_delay(sc, 1);

	/* Set RF_ENV output high. */
	rtwn_bb_setbits(sc, R92C_FPGA0_RFIFACEOE(0), 0, 0x10);
	rtwn_delay(sc, 1);

	/* Set address and data lengths of RF registers. */
	rtwn_bb_setbits(sc, R92C_HSSI_PARAM2(0),
	    R92C_HSSI_PARAM2_ADDR_LENGTH, 0);
	rtwn_delay(sc, 1);

	rtwn_bb_setbits(sc, R92C_HSSI_PARAM2(0),
	    R92C_HSSI_PARAM2_DATA_LENGTH, 0);
	rtwn_delay(sc, 1);

	/* Write RF initialization values for this chain. */
	i += r92c_init_rf_chain(sc, &sc->rf_prog[0], 0);

	/* Restore RF_ENV control type. */
	rtwn_bb_setbits(sc, R92C_FPGA0_RFIFACESW(0), 0x10, type);

	/* Cache RF register CHNLBW. */
	rs->rf_chnlbw[i] = rtwn_rf_read(sc, 0, R92C_RF_CHNLBW);

	/* Turn CCK and OFDM blocks on. */
	rtwn_bb_setbits(sc, R92C_FPGA0_RFMOD, 0, R92C_RFMOD_CCK_EN);
	rtwn_bb_setbits(sc, R92C_FPGA0_RFMOD, 0, R92C_RFMOD_OFDM_EN);
}
