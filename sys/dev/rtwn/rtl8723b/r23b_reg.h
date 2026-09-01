/*
 * Copyright (c) 2026 Ahmad Khalifa <vexeduxr@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef R23B_REG_H
#define R23B_REG_H

#define R23B_BTCOEX_TABLE(i)	(0x6c0 + (i) * 4)
#define R23B_GNT_BT		0x764
#define R23B_ANT_SEL		0x948

/* Bits for R92C_EFUSE_TEST. */
#define R23B_EFUSE_TEST_SEL_M		0x300
#define R23B_EFUSE_TEST_SEL_S		8
#define R23B_EFUSE_TEST_SEL_WIFI	0
#define R23B_EFUSE_TEST_SEL_BT0		1
#define R23B_EFUSE_TEST_SEL_BT1		2
#define R23B_EFUSE_TEST_SEL_BT2		3
#define R23B_EFUSE_TEST_PGMEN		0x800

/* Bits for R92C_PWR_DATA. */
#define R23B_PWR_DATA_RFE_CTRL_EN	0x800

/* Bits for R92C_LEDCFG0. */
#define R23B_LEDCFG0_DPDT_SEL_EN	0x800000

/* Bits for R92C_LEDCFG2. */
#define R23B_LEDCFG2_LED_SW_CTRL	0x20
#define R23B_LEDCFG2_LED_DISABLE	0x04
#define R23B_LEDCFG2_LED_M		0x7f

/* Bits for R88E_BB_PAD_CTRL. */
#define R23B_BB_PAD_CTRL_DPDT_SEL_DATA	0x1

/* Bits for R92C_RXDMA_AGG_PG_TH. */
#define R23B_RXDMA_AGG_PG_TH_EN		0x80000000
#define R23B_RXDMA_AGG_PG_TH_SIZE_M	0xf
#define R23B_RXDMA_AGG_PG_TH_SIZE_S	0
#define R23B_RXDMA_AGG_PG_TH_TIME_M	0xff00
#define R23B_RXDMA_AGG_PG_TH_TIME_S	8

/* Bits for R92C_FWHW_TXQ_CTRL. */
#define R23B_FWHW_TXQ_CTRL_ACK_MGNT	0x1000

/* Bits for R92C_WMAC_TRXPTCL_CTL. */
#define R23B_WMAC_TRXPTCL_CTL_HT40	0x80
#define R23B_WMAC_TRXPTCL_CTL_VHT80	0x100

/* Bits for R23B_ANT_SEL. */
#define R23B_ANT_SEL_S0	0x000
#define R23B_ANT_SEL_S1	0x280

/*
 * RF (6052) registers.
 */
#define R23B_RF_S0S1	0xb0

/* Bits for R92C_RF_CHNLBW. */
#define R23B_RF_CHNLBW_BW40	0x00400

#endif
