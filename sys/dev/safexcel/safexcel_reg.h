/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Rubicon Communications, LLC (Netgate)
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SAFEXCEL_REGS_H_
#define	_SAFEXCEL_REGS_H_

#define	SAFEXCEL_HIA_VERSION_LE			0x35ca
#define	SAFEXCEL_HIA_VERSION_BE			0xca35
#define	EIP201_VERSION_LE			0x36c9
#define	SAFEXCEL_REG_LO16(_reg)			((_reg) & 0xffff)
#define	SAFEXCEL_REG_HI16(_reg)			(((_reg) >> 16) & 0xffff)

/* HIA, Command Descriptor Ring Manager */
#define	CDR_BASE_ADDR_LO(x)			(0x0 + ((x) << 12))
#define	CDR_BASE_ADDR_HI(x)			(0x4 + ((x) << 12))
#define	CDR_DATA_BASE_ADDR_LO(x)		(0x8 + ((x) << 12))
#define	CDR_DATA_BASE_ADDR_HI(x)		(0xC + ((x) << 12))
#define	CDR_ACD_BASE_ADDR_LO(x)			(0x10 + ((x) << 12))
#define	CDR_ACD_BASE_ADDR_HI(x)			(0x14 + ((x) << 12))
#define	CDR_RING_SIZE(x)			(0x18 + ((x) << 12))
#define	CDR_DESC_SIZE(x)			(0x1C + ((x) << 12))
#define	CDR_CFG(x)				(0x20 + ((x) << 12))
#define	CDR_DMA_CFG(x)				(0x24 + ((x) << 12))
#define	CDR_THR(x)				(0x28 + ((x) << 12))
#define	CDR_PREP_COUNT(x)			(0x2C + ((x) << 12))
#define	CDR_PROC_COUNT(x)			(0x30 + ((x) << 12))
#define	CDR_PREP_PNTR(x)			(0x34 + ((x) << 12))
#define	CDR_PROC_PNTR(x)			(0x38 + ((x) << 12))
#define	CDR_STAT(x)				(0x3C + ((x) << 12))

/* HIA, Result Descriptor Ring Manager */
#define	RDR_BASE_ADDR_LO(x)			(0x800 + ((x) << 12))
#define	RDR_BASE_ADDR_HI(x)			(0x804 + ((x) << 12))
#define	RDR_DATA_BASE_ADDR_LO(x)		(0x808 + ((x) << 12))
#define	RDR_DATA_BASE_ADDR_HI(x)		(0x80C + ((x) << 12))
#define	RDR_ACD_BASE_ADDR_LO(x)			(0x810 + ((x) << 12))
#define	RDR_ACD_BASE_ADDR_HI(x)			(0x814 + ((x) << 12))
#define	RDR_RING_SIZE(x)			(0x818 + ((x) << 12))
#define	RDR_DESC_SIZE(x)			(0x81C + ((x) << 12))
#define	RDR_CFG(x)				(0x820 + ((x) << 12))
#define	RDR_DMA_CFG(x)				(0x824 + ((x) << 12))
#define	RDR_THR(x)				(0x828 + ((x) << 12))
#define	RDR_PREP_COUNT(x)			(0x82C + ((x) << 12))
#define	RDR_PROC_COUNT(x)			(0x830 + ((x) << 12))
#define	RDR_PREP_PNTR(x)			(0x834 + ((x) << 12))
#define	RDR_PROC_PNTR(x)			(0x838 + ((x) << 12))
#define	RDR_STAT(x)				(0x83C + ((x) << 12))

/* HIA, Ring AIC */
#define	AIC_POL_CTRL(x)				(0xE000 - ((x) << 12))
#define	AIC_TYPE_CTRL(x)			(0xE004 - ((x) << 12))
#define	AIC_ENABLE_CTRL(x)			(0xE008 - ((x) << 12))
#define	AIC_RAW_STAL(x)				(0xE00C - ((x) << 12))
#define	AIC_ENABLE_SET(x)			(0xE00C - ((x) << 12))
#define	AIC_ENABLED_STAT(x)			(0xE010 - ((x) << 12))
#define	AIC_ACK(x)				(0xE010 - ((x) << 12))
#define	AIC_ENABLE_CLR(x)			(0xE014 - ((x) << 12))
#define	AIC_OPTIONS(x)				(0xE018 - ((x) << 12))
#define	AIC_VERSION(x)				(0xE01C - ((x) << 12))

/* HIA, Global AIC */
#define	AIC_G_POL_CTRL				0xF800
#define	AIC_G_TYPE_CTRL				0xF804
#define	AIC_G_ENABLE_CTRL			0xF808
#define	AIC_G_RAW_STAT				0xF80C
#define	AIC_G_ENABLE_SET			0xF80C
#define	AIC_G_ENABLED_STAT			0xF810
#define	AIC_G_ACK				0xF810
#define	AIC_G_ENABLE_CLR			0xF814
#define	AIC_G_OPTIONS				0xF818
#define	AIC_G_VERSION				0xF81C

/* HIA, Data Fetch Engine */
#define	DFE_CFG					0xF000
#define	DFE_PRIO_0				0xF010
#define	DFE_PRIO_1				0xF014
#define	DFE_PRIO_2				0xF018
#define	DFE_PRIO_3				0xF01C

/* HIA, Data Fetch Engine access monitoring for CDR */
#define	DFE_RING_REGION_LO(x)			(0xF080 + ((x) << 3))
#define	DFE_RING_REGION_HI(x)			(0xF084 + ((x) << 3))

/* HIA, Data Fetch Engine thread control and status for thread */
#define	DFE_THR_CTRL				0xF200
#define	DFE_THR_STAT				0xF204
#define	DFE_THR_DESC_CTRL			0xF208
#define	DFE_THR_DESC_DPTR_LO			0xF210
#define	DFE_THR_DESC_DPTR_HI			0xF214
#define	DFE_THR_DESC_ACDPTR_LO			0xF218
#define	DFE_THR_DESC_ACDPTR_HI			0xF21C

/* HIA, Data Store Engine */
#define	DSE_CFG					0xF400
#define	DSE_PRIO_0				0xF410
#define	DSE_PRIO_1				0xF414
#define	DSE_PRIO_2				0xF418
#define	DSE_PRIO_3				0xF41C

/* HIA, Data Store Engine access monitoring for RDR */
#define	DSE_RING_REGION_LO(x)			(0xF480 + ((x) << 3))
#define	DSE_RING_REGION_HI(x)			(0xF484 + ((x) << 3))

/* HIA, Data Store Engine thread control and status for thread */
#define	DSE_THR_CTRL				0xF600
#define	DSE_THR_STAT				0xF604
#define	DSE_THR_DESC_CTRL			0xF608
#define	DSE_THR_DESC_DPTR_LO			0xF610
#define	DSE_THR_DESC_DPTR_HI			0xF614
#define	DSE_THR_DESC_S_DPTR_LO			0xF618
#define	DSE_THR_DESC_S_DPTR_HI			0xF61C
#define	DSE_THR_ERROR_STAT			0xF620

/* HIA Global */
#define	HIA_MST_CTRL				0xFFF4
#define	HIA_OPTIONS				0xFFF8
#define	HIA_VERSION				0xFFFC

/* Processing Engine Input Side, Processing Engine */
#define	PE_IN_DBUF_THRESH			0x10000
#define	PE_IN_TBUF_THRESH			0x10100

/* Packet Engine Configuration / Status Registers */
#define	PE_TOKEN_CTRL_STAT			0x11000
#define	PE_FUNCTION_EN				0x11004
#define	PE_CONTEXT_CTRL				0x11008
#define	PE_INTERRUPT_CTRL_STAT			0x11010
#define	PE_CONTEXT_STAT				0x1100C
#define	PE_OUT_TRANS_CTRL_STAT			0x11018
#define	PE_OUT_BUF_CTRL				0x1101C

/* Packet Engine AIC */
#define	PE_EIP96_AIC_POL_CTRL			0x113C0
#define	PE_EIP96_AIC_TYPE_CTRL			0x113C4
#define	PE_EIP96_AIC_ENABLE_CTRL		0x113C8
#define	PE_EIP96_AIC_RAW_STAT			0x113CC
#define	PE_EIP96_AIC_ENABLE_SET			0x113CC
#define	PE_EIP96_AIC_ENABLED_STAT		0x113D0
#define	PE_EIP96_AIC_ACK			0x113D0
#define	PE_EIP96_AIC_ENABLE_CLR			0x113D4
#define	PE_EIP96_AIC_OPTIONS			0x113D8
#define	PE_EIP96_AIC_VERSION			0x113DC

/* Packet Engine Options & Version Registers */
#define	PE_EIP96_OPTIONS			0x113F8
#define	PE_EIP96_VERSION			0x113FC

#define	SAFEXCEL_OPT

/* Processing Engine Output Side */
#define	PE_OUT_DBUF_THRESH			0x11C00
#define	PE_OUT_TBUF_THRESH			0x11D00

/* Processing Engine Local AIC */
#define	PE_AIC_POL_CTRL				0x11F00
#define	PE_AIC_TYPE_CTRL			0x11F04
#define	PE_AIC_ENABLE_CTRL			0x11F08
#define	PE_AIC_RAW_STAT				0x11F0C
#define	PE_AIC_ENABLE_SET			0x11F0C
#define	PE_AIC_ENABLED_STAT			0x11F10
#define	PE_AIC_ENABLE_CLR			0x11F14
#define	PE_AIC_OPTIONS				0x11F18
#define	PE_AIC_VERSION				0x11F1C

/* Processing Engine General Configuration and Version */
#define	PE_IN_FLIGHT				0x11FF0
#define	PE_OPTIONS				0x11FF8
#define	PE_VERSION				0x11FFC

/* EIP-97 - Global */
#define	EIP97_CLOCK_STATE			0x1FFE4
#define	EIP97_FORCE_CLOCK_ON			0x1FFE8
#define	EIP97_FORCE_CLOCK_OFF			0x1FFEC
#define	EIP97_MST_CTRL				0x1FFF4
#define	EIP97_OPTIONS				0x1FFF8
#define	EIP97_VERSION				0x1FFFC

/* Register base offsets */
#define	SAFEXCEL_HIA_AIC(_sc)			((_sc)->sc_offsets.hia_aic)
#define	SAFEXCEL_HIA_AIC_G(_sc)			((_sc)->sc_offsets.hia_aic_g)
#define	SAFEXCEL_HIA_AIC_R(_sc)			((_sc)->sc_offsets.hia_aic_r)
#define	SAFEXCEL_HIA_AIC_xDR(_sc)		((_sc)->sc_offsets.hia_aic_xdr)
#define	SAFEXCEL_HIA_DFE(_sc)			((_sc)->sc_offsets.hia_dfe)
#define	SAFEXCEL_HIA_DFE_THR(_sc)		((_sc)->sc_offsets.hia_dfe_thr)
#define	SAFEXCEL_HIA_DSE(_sc)			((_sc)->sc_offsets.hia_dse)
#define	SAFEXCEL_HIA_DSE_THR(_sc)		((_sc)->sc_offsets.hia_dse_thr)
#define	SAFEXCEL_HIA_GEN_CFG(_sc)		((_sc)->sc_offsets.hia_gen_cfg)
#define	SAFEXCEL_PE(_sc)			((_sc)->sc_offsets.pe)

/* EIP197 base offsets */
#define	SAFEXCEL_EIP197_HIA_AIC_BASE		0x90000
#define	SAFEXCEL_EIP197_HIA_AIC_G_BASE		0x90000
#define	SAFEXCEL_EIP197_HIA_AIC_R_BASE		0x90800
#define	SAFEXCEL_EIP197_HIA_AIC_xDR_BASE	0x80000
#define	SAFEXCEL_EIP197_HIA_DFE_BASE		0x8c000
#define	SAFEXCEL_EIP197_HIA_DFE_THR_BASE	0x8c040
#define	SAFEXCEL_EIP197_HIA_DSE_BASE		0x8d000
#define	SAFEXCEL_EIP197_HIA_DSE_THR_BASE	0x8d040
#define	SAFEXCEL_EIP197_HIA_GEN_CFG_BASE	0xf0000
#define	SAFEXCEL_EIP197_PE_BASE			0xa0000

/* EIP97 base offsets */
#define	SAFEXCEL_EIP97_HIA_AIC_BASE		0x0
#define	SAFEXCEL_EIP97_HIA_AIC_G_BASE		0x0
#define	SAFEXCEL_EIP97_HIA_AIC_R_BASE		0x0
#define	SAFEXCEL_EIP97_HIA_AIC_xDR_BASE		0x0
#define	SAFEXCEL_EIP97_HIA_DFE_BASE		0xf000
#define	SAFEXCEL_EIP97_HIA_DFE_THR_BASE		0xf200
#define	SAFEXCEL_EIP97_HIA_DSE_BASE		0xf400
#define	SAFEXCEL_EIP97_HIA_DSE_THR_BASE		0xf600
#define	SAFEXCEL_EIP97_HIA_GEN_CFG_BASE		0x10000
#define	SAFEXCEL_EIP97_PE_BASE			0x10000

/* CDR/RDR register offsets */
#define	SAFEXCEL_HIA_xDR_OFF(priv, r)		(SAFEXCEL_HIA_AIC_xDR(priv) + (r) * 0x1000)
#define	SAFEXCEL_HIA_CDR(priv, r)		(SAFEXCEL_HIA_xDR_OFF(priv, r))
#define	SAFEXCEL_HIA_RDR(priv, r)		(SAFEXCEL_HIA_xDR_OFF(priv, r) + 0x800)
#define	SAFEXCEL_HIA_xDR_RING_BASE_ADDR_LO	0x0000
#define	SAFEXCEL_HIA_xDR_RING_BASE_ADDR_HI	0x0004
#define	SAFEXCEL_HIA_xDR_RING_SIZE		0x0018
#define	SAFEXCEL_HIA_xDR_DESC_SIZE		0x001c
#define	SAFEXCEL_HIA_xDR_CFG			0x0020
#define	SAFEXCEL_HIA_xDR_DMA_CFG		0x0024
#define	SAFEXCEL_HIA_xDR_THRESH			0x0028
#define	SAFEXCEL_HIA_xDR_PREP_COUNT		0x002c
#define	SAFEXCEL_HIA_xDR_PROC_COUNT		0x0030
#define	SAFEXCEL_HIA_xDR_PREP_PNTR		0x0034
#define	SAFEXCEL_HIA_xDR_PROC_PNTR		0x0038
#define	SAFEXCEL_HIA_xDR_STAT			0x003c

/* register offsets */
#define	SAFEXCEL_HIA_DFE_CFG(n)			(0x000 + (128 * (n)))
#define	SAFEXCEL_HIA_DFE_THR_CTRL(n)		(0x000 + (128 * (n)))
#define	SAFEXCEL_HIA_DFE_THR_STAT(n)		(0x004 + (128 * (n)))
#define	SAFEXCEL_HIA_DSE_CFG(n)			(0x000 + (128 * (n)))
#define	SAFEXCEL_HIA_DSE_THR_CTRL(n)		(0x000 + (128 * (n)))
#define	SAFEXCEL_HIA_DSE_THR_STAT(n)		(0x004 + (128 * (n)))
#define	SAFEXCEL_HIA_RA_PE_CTRL(n)		(0x010 + (8 * (n)))
#define	SAFEXCEL_HIA_RA_PE_STAT			0x0014
#define	SAFEXCEL_HIA_AIC_R_OFF(r)		((r) * 0x1000)
#define	SAFEXCEL_HIA_AIC_R_ENABLE_CTRL(r)	(0xe008 - SAFEXCEL_HIA_AIC_R_OFF(r))
#define	SAFEXCEL_HIA_AIC_R_ENABLED_STAT(r)	(0xe010 - SAFEXCEL_HIA_AIC_R_OFF(r))
#define	SAFEXCEL_HIA_AIC_R_ACK(r)		(0xe010 - SAFEXCEL_HIA_AIC_R_OFF(r))
#define	SAFEXCEL_HIA_AIC_R_ENABLE_CLR(r)	(0xe014 - SAFEXCEL_HIA_AIC_R_OFF(r))
#define	SAFEXCEL_HIA_AIC_R_VERSION(r)		(0xe01c - SAFEXCEL_HIA_AIC_R_OFF(r))
#define	SAFEXCEL_HIA_AIC_G_ENABLE_CTRL		0xf808
#define	SAFEXCEL_HIA_AIC_G_ENABLED_STAT		0xf810
#define	SAFEXCEL_HIA_AIC_G_ACK			0xf810
#define	SAFEXCEL_HIA_MST_CTRL			0xfff4
#define	SAFEXCEL_HIA_OPTIONS			0xfff8
#define	SAFEXCEL_HIA_VERSION			0xfffc
#define	SAFEXCEL_PE_IN_DBUF_THRES(n)		(0x0000 + (0x2000 * (n)))
#define	SAFEXCEL_PE_IN_TBUF_THRES(n)		(0x0100 + (0x2000 * (n)))
#define	SAFEXCEL_PE_ICE_SCRATCH_RAM(x, n)	((0x800 + (x) * 4) + 0x2000 * (n))
#define	SAFEXCEL_PE_ICE_PUE_CTRL(n)		(0xc80 + (0x2000 * (n)))
#define	SAFEXCEL_PE_ICE_SCRATCH_CTRL		0x0d04
#define	SAFEXCEL_PE_ICE_FPP_CTRL(n)		(0xd80 + (0x2000 * (n)))
#define	SAFEXCEL_PE_ICE_RAM_CTRL(n)		(0xff0 + (0x2000 * (n)))
#define	SAFEXCEL_PE_EIP96_FUNCTION_EN(n)	(0x1004 + (0x2000 * (n)))
#define	SAFEXCEL_PE_EIP96_CONTEXT_CTRL(n)	(0x1008 + (0x2000 * (n)))
#define	SAFEXCEL_PE_EIP96_CONTEXT_STAT(n)	(0x100c + (0x2000 * (n)))
#define	SAFEXCEL_PE_EIP96_FUNCTION2_EN(n)	(0x1030 + (0x2000 * (n)))
#define	SAFEXCEL_PE_OUT_DBUF_THRES(n)		(0x1c00 + (0x2000 * (n)))
#define	SAFEXCEL_PE_OUT_TBUF_THRES(n)		(0x1d00 + (0x2000 * (n)))

/* EIP-197 Classification Engine */

/* Classification regs */
#define	SAFEXCEL_CS_RAM_CTRL			0xf7ff0

/* SAFEXCEL_HIA_xDR_DESC_SIZE */
#define	SAFEXCEL_xDR_DESC_MODE_64BIT		(1U << 31)
#define	SAFEXCEL_CDR_DESC_MODE_ADCP		(1 << 30)
#define	SAFEXCEL_xDR_DESC_xD_OFFSET		16

/* SAFEXCEL_DIA_xDR_CFG */
#define	SAFEXCEL_xDR_xD_FETCH_THRESH		16

/* SAFEXCEL_HIA_xDR_DMA_CFG */
#define	SAFEXCEL_HIA_xDR_WR_RES_BUF		(1 << 22)
#define	SAFEXCEL_HIA_xDR_WR_CTRL_BUF		(1 << 23)
#define	SAFEXCEL_HIA_xDR_WR_OWN_BUF		(1 << 24)
#define	SAFEXCEL_HIA_xDR_CFG_xD_PROT(n)		(((n) & 0xf) << 4)
#define	SAFEXCEL_HIA_xDR_CFG_DATA_PROT(n)	(((n) & 0xf) << 12)
#define	SAFEXCEL_HIA_xDR_CFG_ACD_PROT(n)	(((n) & 0xf) << 20)
#define	SAFEXCEL_HIA_xDR_CFG_WR_CACHE(n)	(((n) & 0x7) << 25)
#define	SAFEXCEL_HIA_xDR_CFG_RD_CACHE(n)	(((n) & 0x7) << 29)

/* SAFEXCEL_HIA_CDR_THRESH */
#define	SAFEXCEL_HIA_CDR_THRESH_PROC_PKT(n)	((n) & 0xffff)
#define	SAFEXCEL_HIA_CDR_THRESH_PROC_MODE	(1 << 22)
#define	SAFEXCEL_HIA_CDR_THRESH_PKT_MODE	(1 << 23)
						/* x256 clk cycles */
#define	SAFEXCEL_HIA_CDR_THRESH_TIMEOUT(n)	(((n) & 0xff) << 24)

/* SAFEXCEL_HIA_RDR_THRESH */
#define	SAFEXCEL_HIA_RDR_THRESH_PROC_PKT(n)	((n) & 0xffff)
#define	SAFEXCEL_HIA_RDR_THRESH_PKT_MODE	(1 << 23)
						/* x256 clk cycles */
#define	SAFEXCEL_HIA_RDR_THRESH_TIMEOUT(n)	(((n) & 0xff) << 24)

/* SAFEXCEL_HIA_xDR_PREP_COUNT */
#define	SAFEXCEL_xDR_PREP_CLR_COUNT		(1U << 31)
#define	SAFEXCEL_xDR_PREP_xD_COUNT_INCR_OFFSET	2
#define	SAFEXCEL_xDR_PREP_RD_COUNT_INCR_MASK	0x3fff

/* SAFEXCEL_HIA_xDR_PROC_COUNT */
#define	SAFEXCEL_xDR_PROC_xD_PKT_OFFSET		24
#define	SAFEXCEL_xDR_PROC_xD_PKT_MASK		0x7f
#define	SAFEXCEL_xDR_PROC_xD_COUNT(n)		((n) << 2)
#define	SAFEXCEL_xDR_PROC_xD_PKT(n)		\
    (((n) & SAFEXCEL_xDR_PROC_xD_PKT_MASK) << SAFEXCEL_xDR_PROC_xD_PKT_OFFSET)
#define	SAFEXCEL_xDR_PROC_CLR_COUNT		(1U << 31)

/* SAFEXCEL_HIA_xDR_STAT */
#define	SAFEXCEL_xDR_DMA_ERR			(1 << 0)
#define	SAFEXCEL_xDR_PREP_CMD_THRES		(1 << 1)
#define	SAFEXCEL_xDR_ERR			(1 << 2)
#define	SAFEXCEL_xDR_THRESH			(1 << 4)
#define	SAFEXCEL_xDR_TIMEOUT			(1 << 5)
#define	SAFEXCEL_CDR_INTR_MASK			0x3f
#define	SAFEXCEL_RDR_INTR_MASK			0xff

#define	SAFEXCEL_HIA_RA_PE_CTRL_RESET		(1U << 31)
#define	SAFEXCEL_HIA_RA_PE_CTRL_EN		(1 << 30)

/* Register offsets */

/* SAFEXCEL_HIA_DSE_THR_STAT */
#define	SAFEXCEL_DSE_THR_RDR_ID_MASK		0xf000

/* SAFEXCEL_HIA_OPTIONS */
#define	SAFEXCEL_OPT_ADDR_64			(1U << 31)
#define	SAFEXCEL_OPT_TGT_ALIGN_OFFSET		28
#define	SAFEXCEL_OPT_TGT_ALIGN_MASK		0x70000000
#define	SAFEXCEL_xDR_HDW_OFFSET			25
#define	SAFEXCEL_xDR_HDW_MASK			0x6000000
#define	SAFEXCEL_N_RINGS_MASK			0xf
#define	SAFEXCEL_N_PES_OFFSET			4
#define	SAFEXCEL_N_PES_MASK			0x1f0
#define	EIP97_N_PES_MASK			0x70

/* SAFEXCEL_HIA_AIC_R_ENABLE_CTRL */
#define	SAFEXCEL_CDR_IRQ(n)			(1 << ((n) * 2))
#define	SAFEXCEL_RDR_IRQ(n)			(1 << ((n) * 2 + 1))

/* SAFEXCEL_HIA_DFE/DSE_CFG */
#define	SAFEXCEL_HIA_DxE_CFG_MIN_DATA_SIZE(n)	((n) << 0)
#define	SAFEXCEL_HIA_DxE_CFG_DATA_CACHE_CTRL(n)	(((n) & 0x7) << 4)
#define	SAFEXCEL_HIA_DxE_CFG_MAX_DATA_SIZE(n)	((n) << 8)
#define	SAFEXCEL_HIA_DSE_CFG_ALLWAYS_BUFFERABLE	0xc000
#define	SAFEXCEL_HIA_DxE_CFG_MIN_CTRL_SIZE(n)	((n) << 16)
#define	SAFEXCEL_HIA_DxE_CFG_CTRL_CACHE_CTRL(n)	(((n) & 0x7) << 20)
#define	SAFEXCEL_HIA_DxE_CFG_MAX_CTRL_SIZE(n)	((n) << 24)
#define	SAFEXCEL_HIA_DFE_CFG_DIS_DEBUG		0xe0000000
#define	SAFEXCEL_HIA_DSE_CFG_EN_SINGLE_WR	(1 << 29)
#define	SAFEXCEL_HIA_DSE_CFG_DIS_DEBUG		0xc0000000

/* SAFEXCEL_HIA_DFE/DSE_THR_CTRL */
#define	SAFEXCEL_DxE_THR_CTRL_EN		(1 << 30)
#define	SAFEXCEL_DxE_THR_CTRL_RESET_PE		(1U << 31)

/* SAFEXCEL_HIA_AIC_G_ENABLED_STAT */
#define	SAFEXCEL_G_IRQ_DFE(n)			(1 << ((n) << 1))
#define	SAFEXCEL_G_IRQ_DSE(n)			(1 << (((n) << 1) + 1))
#define	SAFEXCEL_G_IRQ_RING			(1 << 16)
#define	SAFEXCEL_G_IRQ_PE(n)			(1 << ((n) + 20))

/* SAFEXCEL_HIA_MST_CTRL */
#define	RD_CACHE_3BITS				0x5U
#define	WR_CACHE_3BITS				0x3U
#define	RD_CACHE_4BITS				(RD_CACHE_3BITS << 1 | (1 << 0))
#define	WR_CACHE_4BITS				(WR_CACHE_3BITS << 1 | (1 << 0))
#define	SAFEXCEL_MST_CTRL_RD_CACHE(n)		(((n) & 0xf) << 0)
#define	SAFEXCEL_MST_CTRL_WD_CACHE(n)		(((n) & 0xf) << 4)
#define	MST_CTRL_SUPPORT_PROT(n)		(((n) & 0xf) << 12)
#define	SAFEXCEL_MST_CTRL_BYTE_SWAP		(1 << 24)
#define	SAFEXCEL_MST_CTRL_NO_BYTE_SWAP		(1 << 25)

/* SAFEXCEL_PE_IN_DBUF/TBUF_THRES */
#define	SAFEXCEL_PE_IN_xBUF_THRES_MIN(n)	((n) << 8)
#define	SAFEXCEL_PE_IN_xBUF_THRES_MAX(n)	((n) << 12)

/* SAFEXCEL_PE_OUT_DBUF_THRES */
#define	SAFEXCEL_PE_OUT_DBUF_THRES_MIN(n)	((n) << 0)
#define	SAFEXCEL_PE_OUT_DBUF_THRES_MAX(n)	((n) << 4)

/* SAFEXCEL_HIA_AIC_G_ACK */
#define	SAFEXCEL_AIC_G_ACK_ALL_MASK		0xffffffff
#define	SAFEXCEL_AIC_G_ACK_HIA_MASK		0x7ff00000

/* SAFEXCEL_HIA_AIC_R_ENABLE_CLR */
#define	SAFEXCEL_HIA_AIC_R_ENABLE_CLR_ALL_MASK	0xffffffff

/* SAFEXCEL_PE_EIP96_CONTEXT_CTRL */
#define	SAFEXCEL_CONTEXT_SIZE(n)		(n)
#define	SAFEXCEL_ADDRESS_MODE			(1 << 8)
#define	SAFEXCEL_CONTROL_MODE			(1 << 9)

/* SAFEXCEL_PE_EIP96_FUNCTION_EN */
#define	SAFEXCEL_FUNCTION_RSVD			((1U << 6) | (1U << 15) | (1U << 20) | (1U << 23))
#define	SAFEXCEL_PROTOCOL_HASH_ONLY		(1U << 0)
#define	SAFEXCEL_PROTOCOL_ENCRYPT_ONLY		(1U << 1)
#define	SAFEXCEL_PROTOCOL_HASH_ENCRYPT		(1U << 2)
#define	SAFEXCEL_PROTOCOL_HASH_DECRYPT		(1U << 3)
#define	SAFEXCEL_PROTOCOL_ENCRYPT_HASH		(1U << 4)
#define	SAFEXCEL_PROTOCOL_DECRYPT_HASH		(1U << 5)
#define	SAFEXCEL_ALG_ARC4			(1U << 7)
#define	SAFEXCEL_ALG_AES_ECB			(1U << 8)
#define	SAFEXCEL_ALG_AES_CBC			(1U << 9)
#define	SAFEXCEL_ALG_AES_CTR_ICM		(1U << 10)
#define	SAFEXCEL_ALG_AES_OFB			(1U << 11)
#define	SAFEXCEL_ALG_AES_CFB			(1U << 12)
#define	SAFEXCEL_ALG_DES_ECB			(1U << 13)
#define	SAFEXCEL_ALG_DES_CBC			(1U << 14)
#define	SAFEXCEL_ALG_DES_OFB			(1U << 16)
#define	SAFEXCEL_ALG_DES_CFB			(1U << 17)
#define	SAFEXCEL_ALG_3DES_ECB			(1U << 18)
#define	SAFEXCEL_ALG_3DES_CBC			(1U << 19)
#define	SAFEXCEL_ALG_3DES_OFB			(1U << 21)
#define	SAFEXCEL_ALG_3DES_CFB			(1U << 22)
#define	SAFEXCEL_ALG_MD5			(1U << 24)
#define	SAFEXCEL_ALG_HMAC_MD5			(1U << 25)
#define	SAFEXCEL_ALG_SHA1			(1U << 26)
#define	SAFEXCEL_ALG_HMAC_SHA1			(1U << 27)
#define	SAFEXCEL_ALG_SHA2			(1U << 28)
#define	SAFEXCEL_ALG_HMAC_SHA2			(1U << 29)
#define	SAFEXCEL_ALG_AES_XCBC_MAC		(1U << 30)
#define	SAFEXCEL_ALG_GCM_HASH			(1U << 31)

#endif /* _SAFEXCEL_REGS_H_ */
