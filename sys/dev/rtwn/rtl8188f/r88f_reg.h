/*
 * Copyright (c) 2026 Mir Saheb Ali <mirsahebali@pm.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef R88F_REG_H
#define R88F_REG_H

/*
 * Baseband registers.
 */

/* FPGA0. */
#define R88F_FPGA0_PSD_FUNC		0x808
#define R88F_FPGA0_XB_RF_INT_OE		0x864
#define R88F_FPGA0_XCD_RF_SW_CTRL	0x874
#define R88F_FPGA0_ANALOG4		0x88c
#define R88F_FPGA0_PSD_REPORT		0x8b4

/* Noise Histogram. */
#define R88F_NHM_TH9_TH10_8723B		0x890
#define R88F_NHM_TIMER_8723B		0x894
#define R88F_NHM_TH3_TO_TH0_8723B	0x898
#define R88F_NHM_TH7_TO_TH4_8723B	0x89c

/* RX/CCK. */
#define R88F_RX_DFIR		0x954
#define R88F_CCK_PD_THRESH	0xa0a

/* OFDM0. */
#define R88F_OFDM0_FA_RSTC		0xc0c
#define R88F_OFDM0_XA_RX_AFE		0xc10
#define R88F_OFDM0_RX_D_SYNC_PATH	0xc40

/* OFDM1. */
#define R88F_OFDM1_CFO_TRACKING		0xd2c
#define R88F_OFDM1_CSI_FIX_MASK1	0xd40
#define R88F_OFDM1_CSI_FIX_MASK2	0xd44

/* ADDA registers. */
#define R88F_SLEEP			0xee0
#define R88F_RX_OFDM			0xed0
#define R88F_STANDBY			0xedc
#define R88F_RX_WAIT_RIFS		0xed4
#define R88F_RX_TO_RX			0xed8
#define R88F_PMPD_ANAEN			0xeec
#define R88F_RX_CCK			0xe8c
#define R88F_TX_OFDM_RFON		0xe7c
#define R88F_TX_TO_TX			0xe88
#define R88F_FPGA0_XCD_SWITCH_CTRL	0x85c
#define R88F_BLUETOOTH			0xe6c
#define R88F_TX_OFDM_BBON		0xe80
#define R88F_TX_CCK_RFON		0xe74
#define R88F_TX_TO_RX			0xe84
#define R88F_TX_CCK_BBON		0xe78
#define R88F_RX_WAIT_CCA		0xe70

/*
 * RF (6052) registers.
 */
#define R88F_RF_GAIN_CCA	0xdf
#define R88F_RF_UNKNOWN_55	0x55
#define R88F_RF_PAD_TXG		0x56
#define R88F_RF_RXG_GAIN	0x87

/* Bits for R92C_RF_CHNLBW. */
#define R88F_RF_CHNLBW_BW20	0xc00
#define R88F_RF_CHNLBW_BW40	0x400

/* Bits for R88F_RF_RXG_GAIN. */
#define R88F_RF_TXA_PREPAD	0x65

/* Bits for R88F_RF_GAIN_CCA. */
#define R88F_RF_PKTBUF_DBG_CTRL	0x140

/* Bits for R88F_CCK_PD_THRESH. */
#define R88F_CCK_PD_TYPE1_LV1_TH	0x83

#endif /* R88F_REG_H */
