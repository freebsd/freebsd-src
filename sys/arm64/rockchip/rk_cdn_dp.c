/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Kyle Crenshaw
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Rockchip RK3399 Cadence DisplayPort controller driver.
 *
 * Goal: USB-C DisplayPort Alt Mode on RockPro64 — single USB-C cable for
 * both power delivery and video output, replacing the HDMI cable.
 *
 * The RK3399 routes DisplayPort through the Cadence CDN-DP IP block, which
 * sits behind the TYPE-C PHY (rk_typec_phy).  The fusb302 USB-C PD controller
 * negotiates the cable orientation and Alt Mode entry; this driver owns the
 * CDN-DP controller itself: clocks, resets, PHY lane assignment, AUX channel,
 * and eventually DRM connector plumbing.
 *
 * Current state — scaffold phase:
 *   - attach the CDN-DP controller node and bring up its power/clock/reset tree
 *   - switch the TYPE-C PHY lanes into DP mode and enable them
 *   - implement the raw AUX channel so DPCD capability reads are possible
 *   - expose an attach-time debug probe (tunable-gated) to exercise AUX/HPD
 *
 * Not yet implemented:
 *   - link training (required before pixels flow)
 *   - HPD interrupt handling and hot-plug event delivery
 *   - Alt Mode negotiation integration with fusb302
 *   - DRM connector registration so Xorg sees a DP output
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/firmware.h>
#include <sys/linker.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <sys/rman.h>

#include <machine/bus.h>

#include <dev/clk/clk.h>
#include <dev/hwreset/hwreset.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>
#include <dev/phy/phy.h>
#include <dev/syscon/syscon.h>
#include <dev/iicbus/usb/fusb302_var.h>
#include <dev/drm2/drm_dp_helper.h>
#include "syscon_if.h"
#include "rk3399_power.h"
#include "rk3399_typec_altmode_var.h"
#include "rk_cdn_dp_var.h"
#include "rk_dp_forced_mode.h"
#include "rk_typec_phy_var.h"

#define RK_CDN_DP_MODE_FLAG_PHSYNC	(1u << 0)
#define RK_CDN_DP_MODE_FLAG_PVSYNC	(1u << 2)

#define	MODE_I2C_START	1
#define	MODE_I2C_WRITE	2
#define	MODE_I2C_READ	4
#define	MODE_I2C_STOP	8

#define	AUX_I2C_WRITE	0x0
#define	AUX_I2C_READ	0x1
#define	AUX_I2C_MOT	0x4

struct iic_dp_aux_data {
	bool running;
	uint16_t address;
	void *priv;
	int (*aux_ch)(device_t idev, int mode, uint8_t write_byte,
	    uint8_t *read_byte);
	device_t port;
};

/* iic_dp_aux_add_bus comes from <dev/drm2/drm_dp_helper.h>. */

/* RK3399 supports up to two TYPE-C ports, each providing one DP PHY. */
#define	RK_CDN_DP_MAX_PHYS	2

/* CDN-DP requires four clocks and four resets; counts kept here so loops
 * and array sizes stay in sync with the name tables below. */
#define	RK_CDN_DP_NCLKS		4
#define	RK_CDN_DP_NRSTS		4

/* AUX channel payload limit per the DisplayPort 1.4 spec (16 bytes). */
#define	RK_CDN_DP_AUX_MAX_XFER	16
#define	DP_LINK_STATUS_SIZE		6
#define	DP_LINK_BW_SET			0x100
#define	DP_LANE_COUNT_SET		0x101
#define	 DP_LANE_COUNT_ENHANCED_FRAME_EN	(1 << 7)
#define	DP_TRAINING_PATTERN_SET		0x102
#define	 DP_TRAINING_PATTERN_1		1
#define	 DP_LINK_SCRAMBLING_DISABLE	(1 << 5)
#define	DP_DOWNSPREAD_CTRL		0x107
#define	 DP_SPREAD_AMP_0_5		(1 << 4)
#define	DP_MAIN_LINK_CHANNEL_CODING_SET	0x108
#define	 DP_SET_ANSI_8B10B		(1 << 0)
#define	DP_SINK_COUNT			0x200
#define	DP_LANE0_1_STATUS		0x202
#define	DP_MAX_DOWNSPREAD		0x003
#define	 DP_MAX_DOWNSPREAD_0_5		(1 << 0)
#define	DP_MAIN_LINK_CHANNEL_CODING	0x006
#define	DP_MAX_LANE_COUNT		0x002

/* Number of DPCD registers read in one shot during the capability probe.
 * Bytes 0x000–0x00E cover revision, link rate, lane count, and key caps. */
#define	RK_CDN_DP_DPCD_CAP_SIZE	15
#define	RK_CDN_DP_DPCD_SINK_COUNT	0x200

/* DP training-related DPCD addresses and bits (DisplayPort 1.2 spec). */
#define	DP_TRAINING_LANE0_SET		0x103
#define	 DP_TRAIN_VOLTAGE_SWING_MASK		0x3
#define	 DP_TRAIN_VOLTAGE_SWING_SHIFT		0
#define	 DP_TRAIN_MAX_SWING_REACHED		(1 << 2)
#define	 DP_TRAIN_PRE_EMPHASIS_MASK		(0x3 << 3)
#define	 DP_TRAIN_PRE_EMPHASIS_SHIFT		3
#define	 DP_TRAIN_MAX_PRE_EMPHASIS_REACHED	(1 << 5)
#define	 DP_TRAIN_VOLTAGE_SWING_LEVEL_0		0
#define	 DP_TRAIN_VOLTAGE_SWING_LEVEL_1		1
#define	 DP_TRAIN_VOLTAGE_SWING_LEVEL_2		2
#define	 DP_TRAIN_PRE_EMPH_LEVEL_0		(0 << 3)
#define	 DP_TRAIN_PRE_EMPH_LEVEL_1		(1 << 3)
#define	 DP_TRAIN_PRE_EMPH_LEVEL_2		(2 << 3)
#define	 DP_TRAIN_PRE_EMPH_LEVEL_3		(3 << 3)
#define	DP_LANE_ALIGN_STATUS_UPDATED	0x204
#define	 DP_INTERLANE_ALIGN_DONE		(1 << 0)
#define	DP_ADJUST_REQUEST_LANE0_1	0x206
#define	DP_ADJUST_REQUEST_LANE2_3	0x207
#define	 DP_LANE_CR_DONE			(1 << 0)
#define	 DP_LANE_CHANNEL_EQ_DONE		(1 << 1)
#define	 DP_LANE_SYMBOL_LOCKED			(1 << 2)
#define	 DP_TRAINING_PATTERN_2			2
#define	 DP_TRAINING_PATTERN_3			3
#define	 DP_TRAINING_PATTERN_DISABLE		0
#define	 DP_TRAINING_PATTERN_MASK		0x3

/* Cadence DPTX framer/PHY config registers (mailbox WRITE_REGISTER, opcode 0x06). */
#define	RK_CDN_DP_DP_TX_PHY_CONFIG_REG	0x2000
#define	RK_CDN_DP_DP_FRAMER_GLOBAL_CONFIG	0x2200
#define	RK_CDN_DP_DPTX_LANE_EN		0x2300
#define	RK_CDN_DP_DPTX_ENHNCD		0x2304
/* DP_FRAMER_GLOBAL_CONFIG bits */
#define	 RK_CDN_DP_FRAMER_NUM_LANES(x)	((x) & 0x3)
#define	 RK_CDN_DP_FRAMER_SST_MODE	(0U << 2)
#define	 RK_CDN_DP_FRAMER_GLOBAL_EN	(1U << 3)
#define	 RK_CDN_DP_FRAMER_RG_EN		(0U << 4)
#define	 RK_CDN_DP_FRAMER_NO_VIDEO	(1U << 5)
#define	 RK_CDN_DP_FRAMER_ENC_RST_DIS	(1U << 6)
#define	 RK_CDN_DP_FRAMER_WR_VHSYNC_FALL	(1U << 7)
/* DP_TX_PHY_CONFIG_REG bits */
#define	 RK_CDN_DP_TX_PHY_TRAINING_ENABLE(x)	((x) & 1)
#define	 RK_CDN_DP_TX_PHY_TRAINING_PATTERN(x)	((uint32_t)((x) & 0xf) << 1)
#define	 RK_CDN_DP_TX_PHY_SCRAMBLER_BYPASS(x)	(((x) & 1) << 5)
#define	 RK_CDN_DP_TX_PHY_ENCODER_BYPASS(x)	(((x) & 1) << 6)
#define	 RK_CDN_DP_TX_PHY_SKEW_BYPASS(x)	(((x) & 1) << 7)
#define	 RK_CDN_DP_TX_PHY_DISPARITY_RST(x)	(((x) & 1) << 8)
#define	 RK_CDN_DP_TX_PHY_LANE0_SKEW(x)	(((uint32_t)(x) & 7) << 9)
#define	 RK_CDN_DP_TX_PHY_LANE1_SKEW(x)	(((uint32_t)(x) & 7) << 12)
#define	 RK_CDN_DP_TX_PHY_LANE2_SKEW(x)	(((uint32_t)(x) & 7) << 15)
#define	 RK_CDN_DP_TX_PHY_LANE3_SKEW(x)	(((uint32_t)(x) & 7) << 18)
#define	 RK_CDN_DP_TX_PHY_10BIT_ENABLE(x)	(((uint32_t)(x) & 1) << 21)

/*
 * CDN-DP APB register offsets.
 * All offsets are relative to the MMIO base at 0xfec00000 (RK3399 TRM Part2).
 */

/* DP_INT_STA — interrupt status; written to clear reply/error sticky bits
 * before starting an AUX transaction so stale flags don't confuse the wait. */
#define	RK_CDN_DP_DP_INT_STA		0x03DC
#define	 RK_CDN_DP_INT_RPLY_RECEIV	(1U << 1)
#define	 RK_CDN_DP_INT_AUX_ERR		(1U << 0)

/* SYS_CTL_3 — system control register 3; holds the live HPD status bit and
 * the software HPD force bits used for Type-C debug without a physical sink. */
#define	RK_CDN_DP_SYS_CTL_3		0x0608
#define	 RK_CDN_DP_SYS_CTL_3_HPD_STATUS	(1U << 6)
#define	 RK_CDN_DP_SYS_CTL_3_F_HPD	(1U << 5)
#define	 RK_CDN_DP_SYS_CTL_3_HPD_CTRL	(1U << 4)

/* AUX_CH_STA — AUX channel status; polled to detect transaction completion
 * and to read the result code after the CDN-DP engine finishes an exchange. */
#define	RK_CDN_DP_AUX_CH_STA		0x0780
#define	 RK_CDN_DP_AUX_BUSY		(1U << 4)
#define	 RK_CDN_DP_AUX_STATUS_MASK	0x0f

/* AUX_ERR_NUM — extended error count; printed alongside the status name to
 * give more context when AUX transactions fail during bring-up. */
#define	RK_CDN_DP_AUX_ERR_NUM		0x0784

/* BUFFER_DATA_CTL — AUX data buffer control; BUF_CLR flushes stale payload
 * bytes before a new transaction, BUF_HAVE_DATA signals that read bytes are
 * present in BUF_DATA_0..N after a completed native read. */
#define	RK_CDN_DP_BUFFER_DATA_CTL	0x0790
#define	 RK_CDN_DP_BUF_CLR		(1U << 7)
#define	 RK_CDN_DP_BUF_HAVE_DATA	(1U << 4)
#define	 RK_CDN_DP_BUF_DATA_COUNT_MASK	0x0f

/* AUX channel control and address registers; split into three byte-wide
 * address registers because the DPCD address space is 20 bits wide. */
#define	RK_CDN_DP_AUX_CH_CTL_1		0x0794
#define	RK_CDN_DP_AUX_ADDR_7_0		0x0798
#define	RK_CDN_DP_AUX_ADDR_15_8	0x079C
#define	RK_CDN_DP_AUX_ADDR_19_16	0x07A0
#define	RK_CDN_DP_AUX_CH_CTL_2		0x07A4
#define	 RK_CDN_DP_ADDR_ONLY		(1U << 1)	/* address-only (0-byte) transfer */
#define	 RK_CDN_DP_AUX_EN		(1U << 0)	/* self-clearing start bit */

/* BUF_DATA_0 — base of the 16-entry AUX payload FIFO; each slot is a full
 * 32-bit register even though only the low byte carries data. */
#define	RK_CDN_DP_BUF_DATA_0		0x07C0

/*
 * Cadence APB/mailbox register block.  The production path uses the
 * firmware-backed mailbox
 * path for DPCD/HPD/register access on RK3399 instead of programming the raw
 * AUX engine directly.
 */
#define	RK_CDN_DP_APB_CTRL		0x0000
#define	RK_CDN_DP_MAILBOX_FULL_ADDR	0x0008
#define	RK_CDN_DP_MAILBOX_EMPTY_ADDR	0x000c
#define	RK_CDN_DP_MAILBOX0_WR_DATA	0x0010
#define	RK_CDN_DP_MAILBOX0_RD_DATA	0x0014
#define	RK_CDN_DP_KEEP_ALIVE		0x0018
#define	RK_CDN_DP_VER_L		0x001c
#define	RK_CDN_DP_VER_H		0x0020
#define	RK_CDN_DP_VER_LIB_L_ADDR	0x0024
#define	RK_CDN_DP_VER_LIB_H_ADDR	0x0028
#define	RK_CDN_DP_SW_CLK_H		0x0040
#define	RK_CDN_DP_SW_EVENTS0		0x0044
#define	RK_CDN_DP_APB_INT_MASK		0x006c

#define	RK_CDN_DP_ADDR_IMEM		0x10000
#define	RK_CDN_DP_ADDR_DMEM		0x20000

#define	RK_CDN_DP_SOURCE_DPTX_CAR	0x0904
#define	RK_CDN_DP_SOURCE_PHY_CAR	0x0908
#define	RK_CDN_DP_SOURCE_PKT_CAR	0x0918
#define	RK_CDN_DP_SOURCE_AIF_CAR	0x091c
#define	RK_CDN_DP_SOURCE_CIPHER_CAR	0x0920
#define	RK_CDN_DP_SOURCE_CRYPTO_CAR	0x0924

#define	RK_CDN_DP_DP_AUX_SWAP_INVERSION_CONTROL	0x280c

#define	RK_CDN_DP_GRF_SOC_CON26		0x6268
#define	RK_CDN_DP_DPTX_HPD_SEL		(3U << 12)
#define	RK_CDN_DP_DPTX_HPD_DEL		(2U << 12)
#define	RK_CDN_DP_DPTX_HPD_SEL_MASK	(3U << 28)

#define	RK_CDN_DP_APB_IRAM_PATH		(1U << 2)
#define	RK_CDN_DP_APB_DRAM_PATH		(1U << 1)
#define	RK_CDN_DP_APB_XT_RESET		(1U << 0)

#define	RK_CDN_DP_DPTX_FRMR_DATA_CLK_RSTN_EN	(1U << 11)
#define	RK_CDN_DP_DPTX_FRMR_DATA_CLK_EN	(1U << 10)
#define	RK_CDN_DP_DPTX_PHY_DATA_RSTN_EN	(1U << 9)
#define	RK_CDN_DP_DPTX_PHY_DATA_CLK_EN	(1U << 8)
#define	RK_CDN_DP_DPTX_PHY_CHAR_RSTN_EN	(1U << 7)
#define	RK_CDN_DP_DPTX_PHY_CHAR_CLK_EN	(1U << 6)
#define	RK_CDN_DP_SOURCE_AUX_SYS_CLK_RSTN_EN	(1U << 5)
#define	RK_CDN_DP_SOURCE_AUX_SYS_CLK_EN	(1U << 4)
#define	RK_CDN_DP_DPTX_SYS_CLK_RSTN_EN	(1U << 3)
#define	RK_CDN_DP_DPTX_SYS_CLK_EN	(1U << 2)
#define	RK_CDN_DP_CFG_DPTX_VIF_CLK_RSTN_EN	(1U << 1)
#define	RK_CDN_DP_CFG_DPTX_VIF_CLK_EN	(1U << 0)
#define	RK_CDN_DP_SOURCE_PHY_RSTN_EN	(1U << 1)
#define	RK_CDN_DP_SOURCE_PHY_CLK_EN	(1U << 0)
#define	RK_CDN_DP_SOURCE_PKT_SYS_RSTN_EN	(1U << 3)
#define	RK_CDN_DP_SOURCE_PKT_SYS_CLK_EN	(1U << 2)
#define	RK_CDN_DP_SOURCE_PKT_DATA_RSTN_EN	(1U << 1)
#define	RK_CDN_DP_SOURCE_PKT_DATA_CLK_EN	(1U << 0)
#define	RK_CDN_DP_SPDIF_CDR_CLK_RSTN_EN	(1U << 5)
#define	RK_CDN_DP_SPDIF_CDR_CLK_EN	(1U << 4)
#define	RK_CDN_DP_SOURCE_AIF_SYS_RSTN_EN	(1U << 3)
#define	RK_CDN_DP_SOURCE_AIF_SYS_CLK_EN	(1U << 2)
#define	RK_CDN_DP_SOURCE_AIF_CLK_RSTN_EN	(1U << 1)
#define	RK_CDN_DP_SOURCE_AIF_CLK_EN	(1U << 0)
#define	RK_CDN_DP_SOURCE_CIPHER_SYSTEM_CLK_RSTN_EN	(1U << 3)
#define	RK_CDN_DP_SOURCE_CIPHER_SYS_CLK_EN	(1U << 2)
#define	RK_CDN_DP_SOURCE_CIPHER_CHAR_CLK_RSTN_EN	(1U << 1)
#define	RK_CDN_DP_SOURCE_CIPHER_CHAR_CLK_EN	(1U << 0)
#define	RK_CDN_DP_SOURCE_CRYPTO_SYS_CLK_RSTN_EN	(1U << 1)
#define	RK_CDN_DP_SOURCE_CRYPTO_SYS_CLK_EN	(1U << 0)

#define	RK_CDN_DP_MB_MODULE_ID_DP_TX	0x01
#define	RK_CDN_DP_MB_MODULE_ID_GENERAL	0x0a
#define	RK_CDN_DP_GENERAL_MAIN_CONTROL	0x01
#define	RK_CDN_DP_DPTX_SET_HOST_CAPABILITIES	0x01
#define	RK_CDN_DP_DPTX_GET_EDID		0x02
#define	RK_CDN_DP_DPTX_READ_DPCD	0x03
#define	RK_CDN_DP_DPTX_SET_VIDEO	0x0c
#define	RK_CDN_DP_DPTX_SET_AUDIO	0x0d

/*
 * Audio sub-block MMIO registers (within the cdn-dp 0xfec00000..fed00000
 * range mapped via RK_CDN_DP_RES_MEM).  Offsets from RK3399 Linux 4.4
 * BSP cdn-dp-reg.h.
 */
#define	RK_CDN_DP_CM_CTRL		0x0a00	/* mailbox-accessed */
#define	RK_CDN_DP_CM_LANE_CTRL		0x0a10	/* mailbox-accessed */
#define	RK_CDN_DP_LANE_REF_CYC		0xf000
#define	RK_CDN_DP_AUDIO_SRC_CNTL	0x30000
#define	RK_CDN_DP_AUDIO_SRC_CNFG	0x30004
#define	RK_CDN_DP_COM_CH_STTS_BITS	0x30008
#define	RK_CDN_DP_STTS_BIT_CH(x)	(0x3000c + ((x) << 2))
#define	RK_CDN_DP_SPDIF_CTRL_ADDR	0x3004c
#define	RK_CDN_DP_SMPL2PKT_CNTL		0x30080
#define	RK_CDN_DP_SMPL2PKT_CNFG		0x30084
#define	RK_CDN_DP_FIFO_CNTL		0x30088
#define	RK_CDN_DP_AUDIO_PACK_CONTROL	0x2214	/* mailbox write target */
#define	RK_CDN_DP_AUDIO_PACK_STATUS	0x2298
#define	RK_CDN_DP_PCK_STUFF_STATUS_0	0x22a0
#define	RK_CDN_DP_PCK_STUFF_STATUS_1	0x22a4
#define	RK_CDN_DP_VIF_STATUS		0x229c

/* SDP (Secondary-Data Packet) infoframe write path, direct MMIO. */
#define	RK_CDN_DP_SOURCE_PIF_WR_ADDR		0x30800
#define	RK_CDN_DP_SOURCE_PIF_WR_REQ		0x30804
#define	RK_CDN_DP_SOURCE_PIF_DATA_WR		0x30810
#define	RK_CDN_DP_SOURCE_PIF_PKT_ALLOC_REG	0x3082c
#define	RK_CDN_DP_SOURCE_PIF_PKT_ALLOC_WR_EN	0x30830
#define	RK_CDN_DP_HOST_WR			(1U << 0)
#define	RK_CDN_DP_PKT_ALLOC_WR_EN		(1U << 0)
#define	RK_CDN_DP_TYPE_VALID			(1U << 16)
#define	RK_CDN_DP_ACTIVE_IDLE_TYPE(x)		(((x) & 0x1) << 17)
#define	RK_CDN_DP_PACKET_TYPE(x)		(((x) & 0xff) << 8)
#define	RK_CDN_DP_PKT_ALLOC_ADDRESS(x)		((x) & 0xf)
#define	RK_CDN_DP_HDMI_INFOFRAME_TYPE_AUDIO	0x84

/* Audio config bit fields. */
#define	RK_CDN_DP_AUDIO_PACK_EN		(1U << 8)
#define	RK_CDN_DP_AUDIO_SW_RST		(1U << 0)
#define	RK_CDN_DP_SMPL2PKT_EN		(1U << 1)
#define	RK_CDN_DP_I2S_DEC_START		(1U << 1)
#define	RK_CDN_DP_SYNC_WR_TO_CH_ZERO	(1U << 1)
#define	RK_CDN_DP_AUDIO_TYPE_LPCM	(2U << 7)
#define	RK_CDN_DP_TRANS_SMPL_WIDTH_32	(2U << 11)
#define	RK_CDN_DP_MAX_NUM_CH(x)		((((x) & 0x1f) - 1) & 0x1f)
#define	RK_CDN_DP_NUM_OF_I2S_PORTS(x)	((((x) / 2 - 1) & 0x3) << 5)
#define	RK_CDN_DP_CFG_SUB_PCKT_NUM(x)	((((x) - 1) & 0x7) << 11)
#define	RK_CDN_DP_AUDIO_CH_NUM(x)	((((x) - 1) & 0x1f) << 2)
#define	RK_CDN_DP_I2S_DEC_PORT_EN(x)	(((x) & 0xf) << 17)
#define	RK_CDN_DP_SAMPLING_FREQ(x)	(((x) & 0xf) << 16)
#define	RK_CDN_DP_ORIGINAL_SAMP_FREQ(x)	(((x) & 0xf) << 24)
#define	RK_CDN_DP_DPTX_WRITE_FIELD	0x08
#define	RK_CDN_DP_MAX_EDID_BLOCKS	2

/* DP framer / MSA register offsets (Cadence DPTX, mailbox WRITE_REGISTER). */
#define	RK_CDN_DP_BND_HSYNC2VSYNC	0x0b00
#define	 RK_CDN_DP_VIF_BYPASS_INTERLACE	(1U << 13)
#define	RK_CDN_DP_HSYNC2VSYNC_POL_CTRL	0x0b10
#define	RK_CDN_DP_DP_FRAMER_TU		0x2208
#define	 RK_CDN_DP_TU_CNT_RST_EN	(1U << 15)
#define	 RK_CDN_DP_TU_SIZE_INIT		30
#define	RK_CDN_DP_DP_FRAMER_PXL_REPR	0x220c
#define	RK_CDN_DP_DP_FRAMER_SP		0x2210
#define	 RK_CDN_DP_FRAMER_SP_INTERLACE	(1U << 2)
#define	 RK_CDN_DP_FRAMER_SP_HSP	(1U << 0)
#define	 RK_CDN_DP_FRAMER_SP_VSP	(1U << 1)
#define	RK_CDN_DP_DP_VC_TABLE(x)	(0x2218 + ((x) << 2))
#define	RK_CDN_DP_DP_VB_ID		0x2258
#define	RK_CDN_DP_DP_FRONT_BACK_PORCH	0x2278
#define	RK_CDN_DP_DP_BYTE_COUNT		0x227c
#define	RK_CDN_DP_MSA_HORIZONTAL_0	0x2280
#define	RK_CDN_DP_MSA_HORIZONTAL_1	0x2284
#define	RK_CDN_DP_MSA_VERTICAL_0	0x2288
#define	RK_CDN_DP_MSA_VERTICAL_1	0x228c
#define	RK_CDN_DP_MSA_MISC		0x2290
#define	RK_CDN_DP_STREAM_CONFIG		0x2294
#define	RK_CDN_DP_DP_HORIZONTAL		0x22b0
#define	RK_CDN_DP_DP_VERTICAL_0		0x22b4
#define	RK_CDN_DP_DP_VERTICAL_1		0x22b8

/* Color depth encoding for DP_FRAMER_PXL_REPR (low byte). */
#define	RK_CDN_DP_BCS_8			0x02
/* Color format encoding for DP_FRAMER_PXL_REPR (high byte). */
#define	RK_CDN_DP_PXL_RGB		0x01
#define	RK_CDN_DP_DPTX_WRITE_DPCD	0x04
#define	RK_CDN_DP_DPTX_ENABLE_EVENT	0x05
#define	RK_CDN_DP_DPTX_WRITE_REGISTER	0x06
#define	RK_CDN_DP_DPTX_READ_REGISTER	0x07
#define	RK_CDN_DP_DPTX_READ_EVENT	0x0a
#define	RK_CDN_DP_DPTX_GET_LAST_AUX_STATUS	0x0e
#define	RK_CDN_DP_DPTX_HPD_STATE	0x11
/* Firmware-driven link training (reference vendor BSP cdn-dp-reg.h:368-370). */
#define	RK_CDN_DP_DPTX_TRAINING_CONTROL	0x09
#define	RK_CDN_DP_DPTX_READ_LINK_STAT	0x0b
#define	RK_CDN_DP_LINK_TRAINING_RUN	1
/* DPTX_READ_EVENT response byte 1 bits (cdn-dp-reg.h:412,419). */
#define	RK_CDN_DP_TRAINING_EVENT	(1U << 1)
#define	RK_CDN_DP_EQ_PHASE_FINISHED	(1U << 3)
/* the reference driver fixed timing constants (cdn-dp-reg.c:33-34). */
#define	RK_CDN_DP_LINK_TRAINING_RETRY_MS	20
#define	RK_CDN_DP_LINK_TRAINING_TIMEOUT_MS	500

#define	RK_CDN_DP_FW_STANDBY		0
#define	RK_CDN_DP_FW_ACTIVE		1

#define	RK_CDN_DP_DPTX_EVENT_ENABLE_HPD	(1U << 0)
#define	RK_CDN_DP_DPTX_EVENT_ENABLE_TRAINING	(1U << 1)

#define	RK_CDN_DP_AUX_HOST_INVERT	3
#define	RK_CDN_DP_FAST_LT_NOT_SUPPORT	0
#define	RK_CDN_DP_LANE_MAPPING_NORMAL	0x1b
#define	RK_CDN_DP_LANE_MAPPING_FLIPPED	0xe4
#define	RK_CDN_DP_ENHANCED		1
#define	RK_CDN_DP_SCRAMBLER_EN		(1U << 4)
#define	RK_CDN_DP_VOLTAGE_LEVEL_2	2
#define	RK_CDN_DP_PRE_EMPHASIS_LEVEL_3	3
#define	RK_CDN_DP_PTS1			(1U << 0)
#define	RK_CDN_DP_PTS2			(1U << 1)
#define	RK_CDN_DP_PTS3			(1U << 2)
#define	RK_CDN_DP_PTS4			(1U << 3)
#define	RK_CDN_DP_MAX_LINK_RATE_CODE	0x14

#define	RK_CDN_DP_FW_ALIVE_TIMEOUT_US	1000000
#define	RK_CDN_DP_MAILBOX_RETRY_US	1000
/*
 * Cap mailbox_write FULL-clear wait at 500 ms.  The dptx firmware
 * normally clears FULL in well under 1 ms; 5 s here meant a wedged
 * firmware would burn 5 s per write × hundreds of write-then-poll
 * retries upstack, locking shutdown out for tens of minutes.
 */
#define	RK_CDN_DP_MAILBOX_TIMEOUT_US	500000
#define	RK_CDN_DP_MAILBOX_READ_TIMEOUT_US	10000000
#define	RK_CDN_DP_DPCD_READ_RETRIES	16
#define	RK_CDN_DP_DPCD_DEFER_DELAY_US	20000

#define	RK_CDN_DP_FIRMWARE_NAME		"rockchip/dptx.bin"
#define	RK_CDN_DP_FIRMWARE_PATH		"/boot/firmware/rockchip/dptx.bin"
#define	RK_CDN_DP_FIRMWARE_BASENAME	"dptx.bin"
#define	RK_CDN_DP_FIRMWARE_OLDNAME	"rk3399_dptx_fw"

/* DisplayPort native AUX command codes (4-bit field in AUX_CH_CTL_1). */
#define	RK_CDN_DP_AUX_CMD_NATIVE_WRITE	0x8
#define	RK_CDN_DP_AUX_CMD_NATIVE_READ	0x9

enum rk_cdn_dp_res_id {
	RK_CDN_DP_RES_MEM = 0,
	RK_CDN_DP_RES_IRQ,
	RK_CDN_DP_RES_COUNT
};

enum rk_cdn_dp_clk_id {
	RK_CDN_DP_CLK_CORE = 0,
	RK_CDN_DP_CLK_PCLK,
	RK_CDN_DP_CLK_SPDIF,
	RK_CDN_DP_CLK_GRF
};

enum rk_cdn_dp_rst_id {
	RK_CDN_DP_RST_SPDIF = 0,
	RK_CDN_DP_RST_DPTX,
	RK_CDN_DP_RST_APB,
	RK_CDN_DP_RST_CORE
};

/* Clock names match the DTS clock-names property in rk3399-rockpro64.dtb. */
static const char *rk_cdn_dp_clk_names[RK_CDN_DP_NCLKS] = {
	"core-clk",
	"pclk",
	"spdif",
	"grf",
};

/*
 * Reset names match the DTS reset-names property.
 * Non-const because FreeBSD's hwreset_get_by_ofw_name takes a mutable char *
 * even though the string is never modified by the callee.
 */
static char *rk_cdn_dp_rst_names[RK_CDN_DP_NRSTS] = {
	"spdif",
	"dptx",
	"apb",
	"core",
};

static struct ofw_compat_data rk_cdn_dp_compat_data[] = {
	{ "rockchip,rk3399-cdn-dp", 1 },
	{ NULL, 0 }
};

static struct resource_spec rk_cdn_dp_spec[] = {
	{ SYS_RES_MEMORY, RK_CDN_DP_RES_MEM, RF_ACTIVE },
	{ SYS_RES_IRQ, 0, RF_ACTIVE | RF_OPTIONAL },	/* IRQ absent on some boards */
	{ -1, 0 }
};

#define	RK_CDN_DP_PMU_PWRDN_ST		0x0018
#define	RK_CDN_DP_PMU_BUS_IDLE_ST	0x0064
#define	RK_CDN_DP_PMU_BUS_IDLE_ACK	0x0068
#define	RK_CDN_DP_HDCP_BUS_BIT		11U

/*
 * DPRINTF — chatty trace output gated on dev.rk_cdn_dp.0.debug.
 *   debug = 0  (default): only true errors / one-shot bring-up
 *                         milestones go to dmesg
 *   debug >= 1:           verbose trace, mailbox / AUX / stage walk
 */
#define	RK_CDN_DP_DPRINTF(sc, ...)					\
	do {								\
		if ((sc)->debug > 0)					\
			device_printf((sc)->dev, __VA_ARGS__);		\
	} while (0)

struct rk_cdn_dp_softc {
	device_t		dev;
	device_t		aux_iicbus;
	device_t		aux_iic_adapter;
	phandle_t		node;
	int			debug;		/* sysctl-controlled, gates DPRINTF */
	struct resource		*res[RK_CDN_DP_RES_COUNT];
	clk_t			clks[RK_CDN_DP_NCLKS];
	hwreset_t		rsts[RK_CDN_DP_NRSTS];
	phy_t			phys[RK_CDN_DP_MAX_PHYS];
	int			nphys;
	int			active_port;
	bool			rockpro64_typec0_only;
	bool			clks_enabled;
	bool			rsts_deasserted;
	bool			phys_enabled;
	bool			detached;
	/*
	 * Detach drain lock: sysctl handlers that touch MMIO take a shared
	 * lock; detach takes it exclusive after setting `detached=true`.
	 * That gives sysctl callers a checkpoint to bail (return ENXIO) and
	 * makes detach block until any in-flight sysctl handler completes,
	 * preventing the use-after-free panic where a sysctl call lands in
	 * `mailbox_write` after the softc has already been freed.
	 */
	struct sx		detach_sx;
	bool			has_extcon;	/* cable orientation via extcon rather than fusb302 */
	device_t		extcon_dev;
	bool			has_power_domain;
	device_t		power_dev;	/* handle to rk3399_power provider */
	uint32_t		power_domain_id;
	struct syscon		*pmu_syscon;
	struct syscon		*grf;
	const struct firmware	*fw;
	bool			fw_active;
	uint32_t		fw_version;
	int			hpd_status;
	int			active_port_override;
	int			hostcap_lanes_override;
	int			hostcap_flip_override;
	int			hostcap_usb_ss_override;
	int			skip_aux_swap;
	uint32_t		aux_swap_value;
	int			skip_dpcd;	/* synthesize sink_caps, skip AUX read */
	uint32_t		debug_reg_addr;
	uint32_t		debug_reg_value;
	int			dp_altmode_valid;
	int			dp_altmode_ready;
	int			dp_altmode_usb_ss;
	uint32_t		dp_altmode_pin_assignment;
	uint32_t		dp_altmode_status;
	/* Display / backlight on-off, last value pushed via DPCD writes. */
	uint8_t			display_power_state;	/* 1=D0 awake, 0=D3 sleep */
	uint8_t			backlight_state;	/* 1=on, 0=off */

	/* Bring-up is staged so the module can be loaded safely. */
	int			stage;
	int			last_error;
	bool			allow_phys;
	bool			allow_aux;
	bool			aux_trace_reads;
	bus_size_t		aux_last_read_off;
	uint32_t		aux_last_read_val;
	bus_size_t		aux_last_write_off;
	uint32_t		aux_last_write_val;
	uint32_t		mbox_bad_header_count;
	uint32_t		mbox_last_header;
	uint32_t		mbox_last_expect;
	uint32_t		mbox_last_body0_3;
	uint32_t		mbox_last_body4;
	uint32_t		mbox_last_empty;
	uint32_t		mbox_last_full;
	uint32_t		mbox_last_empty_after_send;
	uint32_t		mbox_last_events0;
	uint32_t		mbox_last_keep_alive;
	uint32_t		mbox_last_apb_int_mask;
	uint32_t		mbox_last_send_header;
	uint32_t		mbox_last_send_size;
	uint32_t		mbox_last_send_written;
	uint32_t		mbox_last_write_full_first;
	uint32_t		mbox_last_write_full_last;
	uint32_t		mbox_last_write_full_polls;
	bool			aux_prepared;
	uint8_t			aux_pending_cmd;
	uint32_t		aux_pending_addr;
	int			aux_pending_len;
	uint8_t			aux_dpcd[RK_CDN_DP_DPCD_CAP_SIZE];
	bool			sink_caps_valid;
	uint8_t			sink_dpcd_rev;
	uint8_t			sink_max_link_rate_code;
	uint8_t			sink_max_lane_count;
	uint32_t		sink_max_link_rate_khz;
	uint8_t			link_plan_rate_code;
	uint8_t			link_plan_lanes;
	uint8_t			expected_link_bw_set;
	uint8_t			expected_lane_count_set;
	uint8_t			link_status[DP_LINK_STATUS_SIZE];
	uint8_t			train_set[4];
	bool			link_trained;
	bool			fw_link_training_used;
	uint8_t			edid[128 * RK_CDN_DP_MAX_EDID_BLOCKS];
	uint16_t		edid_len;
	bool			edid_valid;
	/*
	 * Video info parsed from EDID's first detailed timing block.
	 * pixel_clock_khz is what the display expects on its DP receiver.
	 * Polarities are 1 = positive sync, 0 = negative.
	 */
	uint32_t		pixel_clock_khz;
	uint16_t		hdisplay;
	uint16_t		hblank;
	uint16_t		hsync_start;
	uint16_t		hsync_end;
	uint16_t		htotal;
	uint16_t		vdisplay;
	uint16_t		vblank;
	uint16_t		vsync_start;
	uint16_t		vsync_end;
	uint16_t		vtotal;
	uint8_t			h_sync_polarity;
	uint8_t			v_sync_polarity;
	bool			video_configured;
};

struct rk_cdn_dp_fw_header {
	uint32_t		size_bytes;
	uint32_t		header_size;
	uint32_t		iram_size;
	uint32_t		dram_size;
};

static void	rk_cdn_dp_release(device_t dev);
static int	rk_cdn_dp_aux_prepare(struct rk_cdn_dp_softc *sc,
		    uint8_t cmd, uint32_t addr, uint8_t *buf, int len);
static int	rk_cdn_dp_aux_finish(struct rk_cdn_dp_softc *sc,
		    uint8_t *buf, int len);
static int	rk_cdn_dp_aux_i2c_xfer(device_t idev, int mode,
		    uint8_t write_byte, uint8_t *read_byte);
static int	rk_cdn_dp_probe_dpcd_caps(struct rk_cdn_dp_softc *sc);
static int	rk_cdn_dp_complete_dpcd_caps(struct rk_cdn_dp_softc *sc);
static uint32_t	rk_cdn_dp_link_rate_khz(uint8_t code);
static void	rk_cdn_dp_record_sink_caps(struct rk_cdn_dp_softc *sc);
static int	rk_cdn_dp_plan_link(struct rk_cdn_dp_softc *sc);
static int	rk_cdn_dp_mailbox_dpcd_write(struct rk_cdn_dp_softc *sc,
		    uint32_t addr, const uint8_t *buf, uint16_t len);
static int	rk_cdn_dp_write_link_config(struct rk_cdn_dp_softc *sc,
		    const char *tag);
static void	rk_cdn_dp_verify_link_config(struct rk_cdn_dp_softc *sc,
		    const char *tag);
static void	rk_cdn_dp_log_phy_config(struct rk_cdn_dp_softc *sc,
		    const char *tag, uint32_t phy_config);
static int	rk_cdn_dp_start_link_training(struct rk_cdn_dp_softc *sc);
static int	rk_cdn_dp_mailbox_get_firmware(struct rk_cdn_dp_softc *sc);
static int	rk_cdn_dp_mailbox_prepare_ucpu(struct rk_cdn_dp_softc *sc);
static int	rk_cdn_dp_mailbox_load_fw(struct rk_cdn_dp_softc *sc);
static int	rk_cdn_dp_mailbox_enable_events(struct rk_cdn_dp_softc *sc);
static int	rk_cdn_dp_select_hpd(struct rk_cdn_dp_softc *sc);
static int	rk_cdn_dp_mailbox_get_hpd_state(struct rk_cdn_dp_softc *sc);
static int	rk_cdn_dp_mailbox_set_host_cap(struct rk_cdn_dp_softc *sc);
static int	rk_cdn_dp_mailbox_dpcd_read_retry(struct rk_cdn_dp_softc *sc,
		    uint32_t addr, uint8_t *buf, uint16_t len, int retries);
static int	rk_cdn_dp_mailbox_get_last_aux_status(struct rk_cdn_dp_softc *sc,
		    uint8_t *status_out);
static int	rk_cdn_dp_read_edid(struct rk_cdn_dp_softc *sc);
/* rk_cdn_dp_* public entry points are declared in rk_cdn_dp_var.h. */
static void	rk_cdn_dp_fill_forced_mode(struct rk_cdn_dp_softc *sc);
static void	rk_cdn_dp_get_hostcap_config(struct rk_cdn_dp_softc *sc,
		    uint8_t *lanes, bool *flip, int *usb_ss);
static void	rk_cdn_dp_mailbox_drain(struct rk_cdn_dp_softc *sc, int limit);
static int	rk_cdn_dp_mailbox_drain_events(struct rk_cdn_dp_softc *sc);
static int	rk_cdn_dp_mailbox_settle_post_hostcap(
		    struct rk_cdn_dp_softc *sc);
static void	rk_cdn_dp_mailbox_capture_state(struct rk_cdn_dp_softc *sc);
static void	rk_cdn_dp_mailbox_log_state(struct rk_cdn_dp_softc *sc,
		    const char *tag);
static int	rk_cdn_dp_wait_sink_ready(struct rk_cdn_dp_softc *sc);
static int	rk_cdn_dp_mailbox_probe_dpcd_caps(struct rk_cdn_dp_softc *sc);
static bool	rk_cdn_dp_altmode_signature_ok(
		    const struct rk3399_typec_dp_altmode_status *status);
static void	rk_cdn_dp_log_typec_state(struct rk_cdn_dp_softc *sc,
		    const char *tag);
static int	rk_cdn_dp_get_power_domain(struct rk_cdn_dp_softc *sc);
static int	rk_cdn_dp_get_extcon(struct rk_cdn_dp_softc *sc);
static void	rk_cdn_dp_refresh_typec_provider(struct rk_cdn_dp_softc *sc);
static void	rk_cdn_dp_snapshot_typec_state(struct rk_cdn_dp_softc *sc,
		    const char *tag);
static int	rk_cdn_dp_select_active_port(struct rk_cdn_dp_softc *sc);
static int	rk_cdn_dp_do_enable_phys(struct rk_cdn_dp_softc *sc);
static bool	rk_cdn_dp_is_rockpro64(device_t dev);
static device_t	rk_cdn_dp_resolve_typec_dev(struct rk_cdn_dp_softc *sc);
static bool	rk_cdn_dp_get_typec_status(struct rk_cdn_dp_softc *sc,
		    struct fusb302_typec_status *status);
static bool	rk_cdn_dp_get_altmode_status(struct rk_cdn_dp_softc *sc,
		    struct rk3399_typec_dp_altmode_status *status);
static int	rk_cdn_dp_lookup_typec_status_cb(linker_file_t lf, void *arg);
static int	(*rk_cdn_dp_lookup_typec_status(void))
		    (device_t, struct fusb302_typec_status *);
static int	rk_cdn_dp_lookup_altmode_fusb302_cb(linker_file_t lf, void *arg);
static int	rk_cdn_dp_lookup_altmode_helper_cb(linker_file_t lf, void *arg);
static int	(*rk_cdn_dp_lookup_altmode_status(void))
		    (device_t, struct rk3399_typec_dp_altmode_status *);
static bool	rk_cdn_dp_power_domain_ready(struct rk_cdn_dp_softc *sc);
static bool	rk_cdn_dp_tunable_flag(const char *name, int defval);
static int	rk_cdn_dp_sysctl_flag(SYSCTL_HANDLER_ARGS);
static int	rk_cdn_dp_sysctl_hostcap_lanes(SYSCTL_HANDLER_ARGS);
static int	rk_cdn_dp_sysctl_hostcap_flip(SYSCTL_HANDLER_ARGS);
static int	rk_cdn_dp_sysctl_hostcap_usb_ss(SYSCTL_HANDLER_ARGS);
static int	rk_cdn_dp_sysctl_active_port(SYSCTL_HANDLER_ARGS);
static int	rk_cdn_dp_sysctl_reprobe(SYSCTL_HANDLER_ARGS);
static int	rk_cdn_dp_sysctl_refresh_typec(SYSCTL_HANDLER_ARGS);
static int	rk_cdn_dp_sysctl_framer_dump(SYSCTL_HANDLER_ARGS);
static int	rk_cdn_dp_sysctl_probe_warm(SYSCTL_HANDLER_ARGS);
static int	rk_cdn_dp_sysctl_retrain_now(SYSCTL_HANDLER_ARGS);
static int	rk_cdn_dp_sysctl_display_power(SYSCTL_HANDLER_ARGS);
static int	rk_cdn_dp_sysctl_backlight_power(SYSCTL_HANDLER_ARGS);
static int	rk_cdn_dp_sysctl_dpcd_write_now(SYSCTL_HANDLER_ARGS);
static int	rk_cdn_dp_sysctl_dpcd_read_now(SYSCTL_HANDLER_ARGS);
static int	rk_cdn_dp_get_clocks(struct rk_cdn_dp_softc *sc);
static int	rk_cdn_dp_enable_clocks(struct rk_cdn_dp_softc *sc);
static int	rk_cdn_dp_sysctl_reg_addr(SYSCTL_HANDLER_ARGS);
static int	rk_cdn_dp_sysctl_reg_value(SYSCTL_HANDLER_ARGS);
static int	rk_cdn_dp_sysctl_reg_read(SYSCTL_HANDLER_ARGS);
static int	rk_cdn_dp_sysctl_reg_write(SYSCTL_HANDLER_ARGS);
static int	rk_cdn_dp_rebind_child(bool reprobe);
static void	rk_cdn_dp_reset_runtime_state(struct rk_cdn_dp_softc *sc);
static int	rk_cdn_dp_set_stage(struct rk_cdn_dp_softc *sc, int target);
static void	rk_cdn_dp_rebind_taskfn(void *context, int pending);
static int	rk_cdn_dp_module_event(module_t mod, int what, void *arg);

static int	rk_cdn_dp_rebind_attempts;
static int	rk_cdn_dp_rebind_matches;
static int	rk_cdn_dp_rebind_last_error;

SYSCTL_NODE(_hw, OID_AUTO, rk_cdn_dp, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "RK3399 CDN-DP module controls");
SYSCTL_INT(_hw_rk_cdn_dp, OID_AUTO, rebind_attempts, CTLFLAG_RD,
    &rk_cdn_dp_rebind_attempts, 0, "Number of explicit child reprobe attempts");
SYSCTL_INT(_hw_rk_cdn_dp, OID_AUTO, rebind_matches, CTLFLAG_RD,
    &rk_cdn_dp_rebind_matches, 0, "Number of matching CDN-DP OFW children found");
SYSCTL_INT(_hw_rk_cdn_dp, OID_AUTO, rebind_last_error, CTLFLAG_RD,
    &rk_cdn_dp_rebind_last_error, 0, "Last error returned by the explicit child reprobe");
SYSCTL_PROC(_hw_rk_cdn_dp, OID_AUTO, reprobe,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, NULL, 0,
    rk_cdn_dp_sysctl_reprobe, "I",
    "Write 1 to reprobe the existing RK3399 CDN-DP child");

enum rk_cdn_dp_stage {
	RK_CDN_DP_STAGE_ATTACHED	= 0,
	RK_CDN_DP_STAGE_POWER		= 1,
	RK_CDN_DP_STAGE_HANDLES		= 2,
	RK_CDN_DP_STAGE_CLOCKS		= 3,
	RK_CDN_DP_STAGE_RESETS		= 4,
	RK_CDN_DP_STAGE_PHYS		= 5,
	RK_CDN_DP_STAGE_FW_GET		= 6,
	RK_CDN_DP_STAGE_FW_PREP		= 7,
	RK_CDN_DP_STAGE_FW_LOAD		= 8,
	RK_CDN_DP_STAGE_FW_ACTIVE	= 9,
	RK_CDN_DP_STAGE_HPD_SEL		= 10,
	RK_CDN_DP_STAGE_HPD_STATE	= 11,
	RK_CDN_DP_STAGE_HOSTCAP		= 12,
	RK_CDN_DP_STAGE_DPCD_READ	= 13,
	RK_CDN_DP_STAGE_LINK_PLAN	= 14,
	RK_CDN_DP_STAGE_LINK_TRAIN_START	= 15,
	RK_CDN_DP_STAGE_LINK_TRAIN_FULL	= 16,
	RK_CDN_DP_STAGE_EDID		= 17,
	RK_CDN_DP_STAGE_CONFIG_VIDEO	= 18,
	RK_CDN_DP_STAGE_VIDEO_ON	= 19,
};

static const char *
rk_cdn_dp_stage_name(int stage)
{
	switch (stage) {
	case RK_CDN_DP_STAGE_ATTACHED:
		return ("attached");
	case RK_CDN_DP_STAGE_POWER:
		return ("power-domain");
	case RK_CDN_DP_STAGE_HANDLES:
		return ("handles");
	case RK_CDN_DP_STAGE_CLOCKS:
		return ("clocks");
	case RK_CDN_DP_STAGE_RESETS:
		return ("resets");
	case RK_CDN_DP_STAGE_PHYS:
		return ("phys");
	case RK_CDN_DP_STAGE_FW_GET:
		return ("fw-get");
	case RK_CDN_DP_STAGE_FW_PREP:
		return ("fw-prep");
	case RK_CDN_DP_STAGE_FW_LOAD:
		return ("fw-load");
	case RK_CDN_DP_STAGE_FW_ACTIVE:
		return ("fw-active");
	case RK_CDN_DP_STAGE_HPD_SEL:
		return ("hpd-sel");
	case RK_CDN_DP_STAGE_HPD_STATE:
		return ("hpd-state");
	case RK_CDN_DP_STAGE_HOSTCAP:
		return ("host-cap");
	case RK_CDN_DP_STAGE_DPCD_READ:
		return ("dpcd-read");
	case RK_CDN_DP_STAGE_LINK_PLAN:
		return ("link-plan");
	case RK_CDN_DP_STAGE_LINK_TRAIN_START:
		return ("link-train-start");
	case RK_CDN_DP_STAGE_LINK_TRAIN_FULL:
		return ("link-train-full");
	case RK_CDN_DP_STAGE_EDID:
		return ("edid");
	case RK_CDN_DP_STAGE_CONFIG_VIDEO:
		return ("config-video");
	case RK_CDN_DP_STAGE_VIDEO_ON:
		return ("video-on");
	default:
		return ("unknown");
	}
}

static const char *
rk_cdn_dp_reg_name(bus_size_t off)
{
	switch (off) {
	case RK_CDN_DP_SYS_CTL_3:
		return ("SYS_CTL_3");
	case RK_CDN_DP_DP_INT_STA:
		return ("DP_INT_STA");
	case RK_CDN_DP_AUX_CH_STA:
		return ("AUX_CH_STA");
	case RK_CDN_DP_AUX_ERR_NUM:
		return ("AUX_ERR_NUM");
	case RK_CDN_DP_BUFFER_DATA_CTL:
		return ("BUFFER_DATA_CTL");
	case RK_CDN_DP_AUX_CH_CTL_1:
		return ("AUX_CH_CTL_1");
	case RK_CDN_DP_AUX_ADDR_7_0:
		return ("AUX_ADDR_7_0");
	case RK_CDN_DP_AUX_ADDR_15_8:
		return ("AUX_ADDR_15_8");
	case RK_CDN_DP_AUX_ADDR_19_16:
		return ("AUX_ADDR_19_16");
	case RK_CDN_DP_AUX_CH_CTL_2:
		return ("AUX_CH_CTL_2");
	case RK_CDN_DP_BUF_DATA_0:
		return ("BUF_DATA_0");
	default:
		return ("unknown");
	}
}

/*
 * rk_cdn_dp_tunable_flag
 *
 * Reads a boolean-style loader tunable and returns it as a C bool.  Keeping
 * the parsing in one helper ensures every stage gate uses the same defaulting
 * rules and keeps attach compact.
 */
static bool
rk_cdn_dp_tunable_flag(const char *name, int defval)
{
	int value;

	value = defval;
	(void)TUNABLE_INT_FETCH(name, &value);
	return (value != 0);
}

/* FUSB302 integration is not wired up yet; keep this module self-contained. */

/*
 * rk_cdn_dp_read_4 / rk_cdn_dp_write_4
 *
 * Thin wrappers around bus_read_4/bus_write_4 so call sites name the
 * controller rather than the resource slot.  Keeping MMIO access centralised
 * here makes it easy to add barrier or debug instrumentation in one place.
 */
static inline uint32_t
rk_cdn_dp_read_4(struct rk_cdn_dp_softc *sc, bus_size_t off)
{
	if (sc->aux_trace_reads)
		sc->aux_last_read_off = off;
	sc->aux_last_read_val = bus_read_4(sc->res[RK_CDN_DP_RES_MEM], off);
	return (sc->aux_last_read_val);
}

static inline void
rk_cdn_dp_write_4(struct rk_cdn_dp_softc *sc, bus_size_t off, uint32_t val)
{
	if (sc->aux_trace_reads) {
		sc->aux_last_write_off = off;
		sc->aux_last_write_val = val;
	}
	bus_write_4(sc->res[RK_CDN_DP_RES_MEM], off, val);
}

static int
rk_cdn_dp_mailbox_mmio_ready(struct rk_cdn_dp_softc *sc)
{
	if (sc == NULL)
		return (ENXIO);
	if (sc->detached)
		return (ENXIO);
	if (sc->res[RK_CDN_DP_RES_MEM] == NULL)
		return (ENXIO);

	return (0);
}

static void
rk_cdn_dp_set_fw_clk(struct rk_cdn_dp_softc *sc, uint64_t hz)
{
	rk_cdn_dp_write_4(sc, RK_CDN_DP_SW_CLK_H, hz / 1000000);
}

static void
rk_cdn_dp_clock_reset(struct rk_cdn_dp_softc *sc)
{
	uint32_t val;

	val = RK_CDN_DP_DPTX_FRMR_DATA_CLK_RSTN_EN |
	    RK_CDN_DP_DPTX_FRMR_DATA_CLK_EN |
	    RK_CDN_DP_DPTX_PHY_DATA_RSTN_EN |
	    RK_CDN_DP_DPTX_PHY_DATA_CLK_EN |
	    RK_CDN_DP_DPTX_PHY_CHAR_RSTN_EN |
	    RK_CDN_DP_DPTX_PHY_CHAR_CLK_EN |
	    RK_CDN_DP_SOURCE_AUX_SYS_CLK_RSTN_EN |
	    RK_CDN_DP_SOURCE_AUX_SYS_CLK_EN |
	    RK_CDN_DP_DPTX_SYS_CLK_RSTN_EN |
	    RK_CDN_DP_DPTX_SYS_CLK_EN |
	    RK_CDN_DP_CFG_DPTX_VIF_CLK_RSTN_EN |
	    RK_CDN_DP_CFG_DPTX_VIF_CLK_EN;
	rk_cdn_dp_write_4(sc, RK_CDN_DP_SOURCE_DPTX_CAR, val);

	val = RK_CDN_DP_SOURCE_PHY_RSTN_EN | RK_CDN_DP_SOURCE_PHY_CLK_EN;
	rk_cdn_dp_write_4(sc, RK_CDN_DP_SOURCE_PHY_CAR, val);

	val = RK_CDN_DP_SOURCE_PKT_SYS_RSTN_EN |
	    RK_CDN_DP_SOURCE_PKT_SYS_CLK_EN |
	    RK_CDN_DP_SOURCE_PKT_DATA_RSTN_EN |
	    RK_CDN_DP_SOURCE_PKT_DATA_CLK_EN;
	rk_cdn_dp_write_4(sc, RK_CDN_DP_SOURCE_PKT_CAR, val);

	val = RK_CDN_DP_SPDIF_CDR_CLK_RSTN_EN |
	    RK_CDN_DP_SPDIF_CDR_CLK_EN |
	    RK_CDN_DP_SOURCE_AIF_SYS_RSTN_EN |
	    RK_CDN_DP_SOURCE_AIF_SYS_CLK_EN |
	    RK_CDN_DP_SOURCE_AIF_CLK_RSTN_EN |
	    RK_CDN_DP_SOURCE_AIF_CLK_EN;
	rk_cdn_dp_write_4(sc, RK_CDN_DP_SOURCE_AIF_CAR, val);

	val = RK_CDN_DP_SOURCE_CIPHER_SYSTEM_CLK_RSTN_EN |
	    RK_CDN_DP_SOURCE_CIPHER_SYS_CLK_EN |
	    RK_CDN_DP_SOURCE_CIPHER_CHAR_CLK_RSTN_EN |
	    RK_CDN_DP_SOURCE_CIPHER_CHAR_CLK_EN;
	rk_cdn_dp_write_4(sc, RK_CDN_DP_SOURCE_CIPHER_CAR, val);

	val = RK_CDN_DP_SOURCE_CRYPTO_SYS_CLK_RSTN_EN |
	    RK_CDN_DP_SOURCE_CRYPTO_SYS_CLK_EN;
	rk_cdn_dp_write_4(sc, RK_CDN_DP_SOURCE_CRYPTO_CAR, val);

	rk_cdn_dp_write_4(sc, RK_CDN_DP_APB_INT_MASK, 0);
}

static int
rk_cdn_dp_mailbox_read(struct rk_cdn_dp_softc *sc, uint8_t *val)
{
	int error;
	int i;

	error = rk_cdn_dp_mailbox_mmio_ready(sc);
	if (error != 0)
		return (error);

	for (i = 0; i < RK_CDN_DP_MAILBOX_READ_TIMEOUT_US / RK_CDN_DP_MAILBOX_RETRY_US;
	    i++) {
		if (rk_cdn_dp_read_4(sc, RK_CDN_DP_MAILBOX_EMPTY_ADDR) == 0) {
			*val = rk_cdn_dp_read_4(sc, RK_CDN_DP_MAILBOX0_RD_DATA) &
			    0xff;
			return (0);
		}
		DELAY(RK_CDN_DP_MAILBOX_RETRY_US);
	}

	return (ETIMEDOUT);
}

static void
rk_cdn_dp_mailbox_drain(struct rk_cdn_dp_softc *sc, int limit)
{
	uint8_t trash;
	int i;

	if (rk_cdn_dp_mailbox_mmio_ready(sc) != 0)
		return;

	for (i = 0; i < limit; i++) {
		if (rk_cdn_dp_read_4(sc, RK_CDN_DP_MAILBOX_EMPTY_ADDR) != 0)
			break;
		trash = rk_cdn_dp_read_4(sc, RK_CDN_DP_MAILBOX0_RD_DATA) & 0xff;
		(void)trash;
	}
}

static void
rk_cdn_dp_mailbox_capture_state(struct rk_cdn_dp_softc *sc)
{

	if (rk_cdn_dp_mailbox_mmio_ready(sc) != 0)
		return;

	sc->mbox_last_empty = rk_cdn_dp_read_4(sc, RK_CDN_DP_MAILBOX_EMPTY_ADDR);
	sc->mbox_last_full = rk_cdn_dp_read_4(sc, RK_CDN_DP_MAILBOX_FULL_ADDR);
	sc->mbox_last_events0 = rk_cdn_dp_read_4(sc, RK_CDN_DP_SW_EVENTS0);
	sc->mbox_last_keep_alive = rk_cdn_dp_read_4(sc, RK_CDN_DP_KEEP_ALIVE);
	sc->mbox_last_apb_int_mask = rk_cdn_dp_read_4(sc, RK_CDN_DP_APB_INT_MASK);
}

static void
rk_cdn_dp_mailbox_log_state(struct rk_cdn_dp_softc *sc, const char *tag)
{

	rk_cdn_dp_mailbox_capture_state(sc);
	if (!bootverbose)
		return;
	device_printf(sc->dev,
	    "%s: FULL=0x%x EMPTY=0x%x SW_EVENTS0=0x%x KEEP_ALIVE=0x%08x APB_INT_MASK=0x%08x HPD=%d last_hdr=0x%08x expect=0x%08x send_hdr=0x%08x send_size=%u send_written=%u full_first=0x%x full_last=0x%x full_polls=%u bad_hdr=%u\n",
	    tag,
	    sc->mbox_last_full,
	    sc->mbox_last_empty,
	    sc->mbox_last_events0,
	    sc->mbox_last_keep_alive,
	    sc->mbox_last_apb_int_mask,
	    sc->hpd_status,
	    sc->mbox_last_header,
	    sc->mbox_last_expect,
	    sc->mbox_last_send_header,
	    sc->mbox_last_send_size,
	    sc->mbox_last_send_written,
	    sc->mbox_last_write_full_first,
	    sc->mbox_last_write_full_last,
	    sc->mbox_last_write_full_polls,
	    sc->mbox_bad_header_count);
}

static int
rk_cdn_dp_mailbox_write(struct rk_cdn_dp_softc *sc, uint8_t val)
{
	uint32_t full, ka_start, ka_end;
	int error;
	int i;

	error = rk_cdn_dp_mailbox_mmio_ready(sc);
	if (error != 0)
		return (error);

	sc->mbox_last_write_full_first = 0xffffffff;
	sc->mbox_last_write_full_last = 0xffffffff;
	sc->mbox_last_write_full_polls = 0;
	ka_start = rk_cdn_dp_read_4(sc, RK_CDN_DP_KEEP_ALIVE);

	for (i = 0; i < RK_CDN_DP_MAILBOX_TIMEOUT_US / RK_CDN_DP_MAILBOX_RETRY_US;
	    i++) {
		full = rk_cdn_dp_read_4(sc, RK_CDN_DP_MAILBOX_FULL_ADDR);
		if (i == 0)
			sc->mbox_last_write_full_first = full;
		sc->mbox_last_write_full_last = full;
		sc->mbox_last_write_full_polls = i + 1;
		if (full == 0) {
			rk_cdn_dp_write_4(sc, RK_CDN_DP_MAILBOX0_WR_DATA, val);
			return (0);
		}
		/*
		 * Drain FW→HOST FIFO while waiting for FULL to clear.
		 * After AUX_SWAP_INVERSION_CONTROL the firmware sends
		 * unsolicited AUX-engine events; if we don't consume them the
		 * firmware's output FIFO fills, hardware flow-control sets
		 * FULL=1 on the HOST→FW side, and the firmware hangs.
		 */
		rk_cdn_dp_mailbox_drain(sc, 64);
		DELAY(RK_CDN_DP_MAILBOX_RETRY_US);
	}

	ka_end = rk_cdn_dp_read_4(sc, RK_CDN_DP_KEEP_ALIVE);
	device_printf(sc->dev,
	    "mailbox_write timeout: FULL never cleared, "
	    "keep_alive 0x%08x->0x%08x EMPTY=0x%x\n",
	    ka_start, ka_end,
	    rk_cdn_dp_read_4(sc, RK_CDN_DP_MAILBOX_EMPTY_ADDR));
	return (ETIMEDOUT);
}

static int
rk_cdn_dp_mailbox_read_receive(struct rk_cdn_dp_softc *sc, uint8_t *buf,
    uint16_t len)
{
	int error;
	uint16_t i;

	if (buf == NULL && len != 0)
		return (EINVAL);

	for (i = 0; i < len; i++) {
		error = rk_cdn_dp_mailbox_read(sc, &buf[i]);
		if (error != 0)
			return (error);
	}

	return (0);
}

static int
rk_cdn_dp_mailbox_validate_receive(struct rk_cdn_dp_softc *sc, uint8_t module_id,
    uint8_t opcode, uint16_t req_size)
{
	uint8_t header[4], trash;
	uint16_t mbox_size, i;
	int error;

	error = rk_cdn_dp_mailbox_read_receive(sc, header, sizeof(header));
	if (error != 0) {
		rk_cdn_dp_mailbox_drain(sc, 64);
		return (error);
	}

	mbox_size = ((uint16_t)header[2] << 8) | header[3];
	if (header[0] == opcode && header[1] == module_id &&
	    mbox_size == req_size)
		return (0);

	sc->mbox_bad_header_count++;
	sc->mbox_last_header = ((uint32_t)header[0] << 24) |
	    ((uint32_t)header[1] << 16) |
	    ((uint32_t)header[2] << 8) |
	    header[3];
	sc->mbox_last_expect = ((uint32_t)opcode << 24) |
	    ((uint32_t)module_id << 16) |
	    req_size;
	sc->mbox_last_body0_3 = 0;
	sc->mbox_last_body4 = 0;

	for (i = 0; i < mbox_size; i++) {
		error = rk_cdn_dp_mailbox_read(sc, &trash);
		if (error == 0) {
			if (i < 4) {
				sc->mbox_last_body0_3 |=
				    (uint32_t)trash << ((3 - i) * 8);
			} else if (i == 4) {
				sc->mbox_last_body4 = trash;
			}
		}
		if (error != 0)
			break;
	}

	return (EINVAL);
}

static int
rk_cdn_dp_mailbox_send(struct rk_cdn_dp_softc *sc, uint8_t module_id,
    uint8_t opcode, uint16_t size, const uint8_t *msg)
{
	uint8_t header[4];
	int error;
	uint16_t i;

	error = rk_cdn_dp_mailbox_mmio_ready(sc);
	if (error != 0) {
		if (sc != NULL && sc->dev != NULL)
			device_printf(sc->dev,
			    "mailbox send with missing MMIO resource\n");
		return (error);
	}
	if (size != 0 && msg == NULL)
		return (EINVAL);

	header[0] = opcode;
	header[1] = module_id;
	header[2] = (size >> 8) & 0xff;
	header[3] = size & 0xff;
	sc->mbox_last_send_header = ((uint32_t)header[0] << 24) |
	    ((uint32_t)header[1] << 16) |
	    ((uint32_t)header[2] << 8) | header[3];
	sc->mbox_last_send_size = sizeof(header) + size;
	sc->mbox_last_send_written = 0;

	for (i = 0; i < sizeof(header); i++) {
		error = rk_cdn_dp_mailbox_write(sc, header[i]);
		if (error != 0)
			return (error);
		sc->mbox_last_send_written++;
	}
	for (i = 0; i < size; i++) {
		error = rk_cdn_dp_mailbox_write(sc, msg[i]);
		if (error != 0)
			return (error);
		sc->mbox_last_send_written++;
	}
	if (rk_cdn_dp_mailbox_mmio_ready(sc) == 0)
		sc->mbox_last_empty_after_send =
		    rk_cdn_dp_read_4(sc, RK_CDN_DP_MAILBOX_EMPTY_ADDR);

	return (0);
}

static int
rk_cdn_dp_mailbox_reg_write(struct rk_cdn_dp_softc *sc, uint16_t addr, uint32_t val)
{
	uint8_t msg[6];

	msg[0] = (addr >> 8) & 0xff;
	msg[1] = addr & 0xff;
	msg[2] = (val >> 24) & 0xff;
	msg[3] = (val >> 16) & 0xff;
	msg[4] = (val >> 8) & 0xff;
	msg[5] = val & 0xff;

	return (rk_cdn_dp_mailbox_send(sc, RK_CDN_DP_MB_MODULE_ID_DP_TX,
	    RK_CDN_DP_DPTX_WRITE_REGISTER, sizeof(msg), msg));
}

static int
rk_cdn_dp_mailbox_reg_write_bit(struct rk_cdn_dp_softc *sc, uint16_t addr,
    uint8_t start_bit, uint8_t bits_no, uint32_t val)
{
	uint8_t msg[8];

	msg[0] = (addr >> 8) & 0xff;
	msg[1] = addr & 0xff;
	msg[2] = start_bit;
	msg[3] = bits_no;
	msg[4] = (val >> 24) & 0xff;
	msg[5] = (val >> 16) & 0xff;
	msg[6] = (val >> 8) & 0xff;
	msg[7] = val & 0xff;

	return (rk_cdn_dp_mailbox_send(sc, RK_CDN_DP_MB_MODULE_ID_DP_TX,
	    RK_CDN_DP_DPTX_WRITE_FIELD, sizeof(msg), msg));
}

/*
 * Mailbox READ_REGISTER (opcode 0x07): firmware-side proxy for reading
 * cdn_dp internal registers (including the AUX engine MMIO at 0x0780+
 * which the CPU cannot map via /dev/mem).  Body is 2-byte address; reply
 * is 6 bytes: addr_h, addr_l, val[31:24], val[23:16], val[15:8], val[7:0].
 */
static int
rk_cdn_dp_mailbox_reg_read(struct rk_cdn_dp_softc *sc, uint16_t addr, uint32_t *val_out)
{
	uint8_t msg[2];
	uint8_t reply[6];
	int error;

	msg[0] = (addr >> 8) & 0xff;
	msg[1] = addr & 0xff;

	error = rk_cdn_dp_mailbox_send(sc, RK_CDN_DP_MB_MODULE_ID_DP_TX,
	    RK_CDN_DP_DPTX_READ_REGISTER, sizeof(msg), msg);
	if (error != 0)
		return (error);
	error = rk_cdn_dp_mailbox_validate_receive(sc,
	    RK_CDN_DP_MB_MODULE_ID_DP_TX, RK_CDN_DP_DPTX_READ_REGISTER,
	    sizeof(reply));
	if (error != 0)
		return (error);
	error = rk_cdn_dp_mailbox_read_receive(sc, reply, sizeof(reply));
	if (error != 0)
		return (error);
	*val_out = ((uint32_t)reply[2] << 24) | ((uint32_t)reply[3] << 16) |
	    ((uint32_t)reply[4] << 8) | (uint32_t)reply[5];
	return (0);
}

/*
 * Sysctl handler: dump the AUX engine MMIO (TRM 6.3.4 register map at
 * 0x0780–0x07A4) via the firmware-side READ_REGISTER mailbox so we can
 * see what state the AUX engine is in right before/after a DPCD attempt.
 */
static int
rk_cdn_dp_sysctl_aux_dump(SYSCTL_HANDLER_ARGS)
{
	struct rk_cdn_dp_softc *sc;
	int error;
	static const struct { uint16_t addr; const char *name; } regs[] = {
		{0x0780, "AUX_CH_STA"},
		{0x0784, "AUX_ERR_NUM"},
		{0x0788, "AUX_CH_DEFER_CTL"},
		{0x078c, "AUX_RX_COMM"},
		{0x0790, "BUFFER_DATA_CTL"},
		{0x0794, "AUX_CH_CTL_1"},
		{0x0798, "AUX_ADDR_7_0"},
		{0x079c, "AUX_ADDR_15_8"},
		{0x07a0, "AUX_ADDR_19_16"},
		{0x07a4, "AUX_CH_CTL_2"},
		{0x280c, "AUX_SWAP_INVERSION_CONTROL"},
	};
	int trigger = 0;

	sc = arg1;
	error = sysctl_handle_int(oidp, &trigger, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (trigger == 0)
		return (0);
	sx_slock(&sc->detach_sx);
	if (sc->detached) {
		sx_sunlock(&sc->detach_sx);
		return (ENXIO);
	}
	{
		/*
		 * Sanity check READ_REGISTER: write a known sentinel to
		 * AUX_SWAP_INVERSION_CONTROL and read it back.  If readback
		 * matches the sentinel, the opcode-7 plumbing is correct
		 * and the AUX engine zeroes elsewhere are real chip state.
		 * If readback is 0, the firmware's READ_REGISTER reply
		 * format differs from what we assume.
		 */
		uint32_t v0 = 0xdeadbeef, v1 = 0xdeadbeef;
		int e0 = rk_cdn_dp_mailbox_reg_read(sc, 0x280c, &v0);
		device_printf(sc->dev,
		    "  AUX_SWAP_INVERSION_CONTROL pre-write [0x280c] = 0x%08x%s\n",
		    v0, e0 == 0 ? "" : " (mailbox error)");
		(void)rk_cdn_dp_mailbox_reg_write(sc, 0x280c, 0x3);
		DELAY(1000);
		int e1 = rk_cdn_dp_mailbox_reg_read(sc, 0x280c, &v1);
		device_printf(sc->dev,
		    "  AUX_SWAP_INVERSION_CONTROL post-write=3 [0x280c] = 0x%08x%s\n",
		    v1, e1 == 0 ? "" : " (mailbox error)");
	}
	for (size_t i = 0; i < nitems(regs); i++) {
		uint32_t v = 0xdeadbeef;
		int e = rk_cdn_dp_mailbox_reg_read(sc, regs[i].addr, &v);
		device_printf(sc->dev,
		    "  %-28s [0x%04x] = 0x%08x%s\n",
		    regs[i].name, regs[i].addr, v,
		    e == 0 ? "" : " (mailbox error)");
		if (e != 0)
			break;
	}
	sx_sunlock(&sc->detach_sx);
	return (0);
}

static int
rk_cdn_dp_load_firmware(struct rk_cdn_dp_softc *sc)
{
	const struct rk_cdn_dp_fw_header *hdr;
	const uint8_t *data, *iram_data, *dram_data;
	uint32_t size_bytes, header_size, iram_size, dram_size, reg, val;
	uint32_t i;

	if (sc->fw == NULL)
		return (ENOENT);
	if (sc->fw->datasize < sizeof(*hdr))
		return (EINVAL);

	data = sc->fw->data;
	hdr = (const struct rk_cdn_dp_fw_header *)data;
	size_bytes = hdr->size_bytes;
	header_size = hdr->header_size;
	iram_size = hdr->iram_size;
	dram_size = hdr->dram_size;
	if (size_bytes != sc->fw->datasize || header_size > size_bytes ||
	    header_size + iram_size + dram_size > size_bytes)
		return (EINVAL);

	iram_data = data + header_size;
	dram_data = iram_data + iram_size;

	rk_cdn_dp_write_4(sc, RK_CDN_DP_APB_CTRL,
	    RK_CDN_DP_APB_IRAM_PATH |
	    RK_CDN_DP_APB_DRAM_PATH |
	    RK_CDN_DP_APB_XT_RESET);

	reg = rk_cdn_dp_read_4(sc, RK_CDN_DP_APB_CTRL);

	for (i = 0; i < iram_size; i += 4) {
		val = ((const uint32_t *)iram_data)[i / 4];
		rk_cdn_dp_write_4(sc, RK_CDN_DP_ADDR_IMEM + i, val);
	}
	for (i = 0; i < dram_size; i += 4) {
		val = ((const uint32_t *)dram_data)[i / 4];
		rk_cdn_dp_write_4(sc, RK_CDN_DP_ADDR_DMEM + i, val);
	}

	/* verify first IMEM word was retained */
	val = rk_cdn_dp_read_4(sc, RK_CDN_DP_ADDR_IMEM);

	reg = rk_cdn_dp_read_4(sc, RK_CDN_DP_KEEP_ALIVE);

	rk_cdn_dp_write_4(sc, RK_CDN_DP_APB_CTRL, 0);

	reg = rk_cdn_dp_read_4(sc, RK_CDN_DP_APB_CTRL);

	for (i = 0; i < RK_CDN_DP_FW_ALIVE_TIMEOUT_US / 2000; i++) {
		reg = rk_cdn_dp_read_4(sc, RK_CDN_DP_KEEP_ALIVE);
		if (reg != 0)
			break;
		if (i % 50 == 49)
			RK_CDN_DP_DPRINTF(sc,
			    "fw-load: waiting for KEEP_ALIVE (%u ms elapsed)\n",
			    (i + 1) * 2);
		DELAY(2000);
	}
	if (reg == 0)
		return (ETIMEDOUT);

	sc->fw_version = rk_cdn_dp_read_4(sc, RK_CDN_DP_VER_L) & 0xff;
	sc->fw_version |= (rk_cdn_dp_read_4(sc, RK_CDN_DP_VER_H) & 0xff) << 8;
	sc->fw_version |= (rk_cdn_dp_read_4(sc, RK_CDN_DP_VER_LIB_L_ADDR) & 0xff) << 16;
	sc->fw_version |= (rk_cdn_dp_read_4(sc, RK_CDN_DP_VER_LIB_H_ADDR) & 0xff) << 24;

	return (0);
}

static int
rk_cdn_dp_set_firmware_active(struct rk_cdn_dp_softc *sc, bool enable)
{
	uint8_t msg[5], val;
	int error;
	size_t i;

	msg[0] = RK_CDN_DP_GENERAL_MAIN_CONTROL;
	msg[1] = RK_CDN_DP_MB_MODULE_ID_GENERAL;
	msg[2] = 0;
	msg[3] = 1;
	msg[4] = enable ? RK_CDN_DP_FW_ACTIVE : RK_CDN_DP_FW_STANDBY;

	error = rk_cdn_dp_mailbox_mmio_ready(sc);
	if (error != 0)
		return (error);

	rk_cdn_dp_mailbox_drain(sc, 64);

	for (i = 0; i < nitems(msg); i++) {
		error = rk_cdn_dp_mailbox_write(sc, msg[i]);
		if (error != 0) {
			rk_cdn_dp_mailbox_drain(sc, 64);
			return (error);
		}
	}

	for (i = 0; i < nitems(msg); i++) {
		error = rk_cdn_dp_mailbox_read(sc, &val);
		if (error != 0) {
			rk_cdn_dp_mailbox_drain(sc, 64);
			return (error);
		}
		msg[i] = val;
	}

	sc->fw_active = enable;
	return (0);
}

static int
rk_cdn_dp_wait_keep_alive(struct rk_cdn_dp_softc *sc, uint32_t *lastp)
{
	uint32_t prev, cur;
	int i;

	prev = rk_cdn_dp_read_4(sc, RK_CDN_DP_KEEP_ALIVE);
	for (i = 0; i < RK_CDN_DP_FW_ALIVE_TIMEOUT_US / 2000; i++) {
		DELAY(2000);
		cur = rk_cdn_dp_read_4(sc, RK_CDN_DP_KEEP_ALIVE);
		if (cur != 0 && cur != prev) {
			if (lastp != NULL)
				*lastp = cur;
			return (0);
		}
		prev = cur;
	}
	if (lastp != NULL)
		*lastp = prev;
	return (ETIMEDOUT);
}

static int
rk_cdn_dp_event_config(struct rk_cdn_dp_softc *sc)
{
	uint8_t msg[5];

	memset(msg, 0, sizeof(msg));
	msg[0] = RK_CDN_DP_DPTX_EVENT_ENABLE_HPD |
	    RK_CDN_DP_DPTX_EVENT_ENABLE_TRAINING;

	return (rk_cdn_dp_mailbox_send(sc, RK_CDN_DP_MB_MODULE_ID_DP_TX,
	    RK_CDN_DP_DPTX_ENABLE_EVENT, sizeof(msg), msg));
}

static int
rk_cdn_dp_set_host_cap(struct rk_cdn_dp_softc *sc, uint8_t lanes, bool flip)
{
	uint8_t msg[8];
	int error;

	msg[0] = RK_CDN_DP_MAX_LINK_RATE_CODE;
	msg[1] = lanes | RK_CDN_DP_SCRAMBLER_EN;
	msg[2] = RK_CDN_DP_VOLTAGE_LEVEL_2;
	msg[3] = RK_CDN_DP_PRE_EMPHASIS_LEVEL_3;
	msg[4] = RK_CDN_DP_PTS1 | RK_CDN_DP_PTS2 |
	    RK_CDN_DP_PTS3 | RK_CDN_DP_PTS4;
	msg[5] = RK_CDN_DP_FAST_LT_NOT_SUPPORT;
	msg[6] = flip ? RK_CDN_DP_LANE_MAPPING_FLIPPED :
	    RK_CDN_DP_LANE_MAPPING_NORMAL;
	msg[7] = RK_CDN_DP_ENHANCED;

	error = rk_cdn_dp_mailbox_send(sc, RK_CDN_DP_MB_MODULE_ID_DP_TX,
	    RK_CDN_DP_DPTX_SET_HOST_CAPABILITIES, sizeof(msg), msg);
	if (error != 0)
		return (error);

	if (!sc->skip_aux_swap) {
		uint32_t aux_swap;

		/*
		 * the reference driver ALWAYS writes AUX_HOST_INVERT (0x3) regardless of
		 * cable orientation — see cdn_dp_set_host_cap in
		 * drivers/gpu/drm/rockchip/cdn-dp-reg.c, which calls
		 * cdn_dp_reg_write(DP_AUX_SWAP_INVERSION_CONTROL,
		 * AUX_HOST_INVERT).  The flip orientation is handled by the
		 * lane-mapping byte in the host-cap message body
		 * (LANE_MAPPING_NORMAL=0x1b vs LANE_MAPPING_FLIPPED=0xe4),
		 * NOT by AUX_SWAP_INVERSION_CONTROL.  Earlier flip→AUX_SWAP
		 * splits were fighting the lane mapping.
		 */
		aux_swap = sc->aux_swap_value;
		RK_CDN_DP_DPRINTF(sc,
		    "host_cap: AUX_SWAP=0x%x (flip=%u, ctl_override=0x%x)\n",
		    aux_swap, flip ? 1 : 0, sc->aux_swap_value);
		error = rk_cdn_dp_mailbox_reg_write(sc,
		    RK_CDN_DP_DP_AUX_SWAP_INVERSION_CONTROL, aux_swap);
		if (error != 0)
			return (error);
	}

	return (0);
}

static int
rk_cdn_dp_mailbox_dpcd_read(struct rk_cdn_dp_softc *sc, uint32_t addr,
    uint8_t *buf, uint16_t len)
{
	return (rk_cdn_dp_mailbox_dpcd_read_retry(sc, addr, buf, len, 1));
}

static int
rk_cdn_dp_mailbox_dpcd_write(struct rk_cdn_dp_softc *sc, uint32_t addr,
    const uint8_t *buf, uint16_t len)
{
	uint8_t msg[5 + RK_CDN_DP_AUX_MAX_XFER];
	uint8_t reg[5];
	int error;

	if (len == 0 || len > RK_CDN_DP_AUX_MAX_XFER || buf == NULL)
		return (EINVAL);

	msg[0] = (len >> 8) & 0xff;
	msg[1] = len & 0xff;
	msg[2] = (addr >> 16) & 0xff;
	msg[3] = (addr >> 8) & 0xff;
	msg[4] = addr & 0xff;
	memcpy(msg + 5, buf, len);

	error = rk_cdn_dp_mailbox_send(sc, RK_CDN_DP_MB_MODULE_ID_DP_TX,
	    RK_CDN_DP_DPTX_WRITE_DPCD, 5 + len, msg);
	if (error != 0)
		return (error);
	error = rk_cdn_dp_mailbox_validate_receive(sc,
	    RK_CDN_DP_MB_MODULE_ID_DP_TX, RK_CDN_DP_DPTX_WRITE_DPCD,
	    sizeof(reg));
	if (error != 0)
		return (error);
	error = rk_cdn_dp_mailbox_read_receive(sc, reg, sizeof(reg));
	if (error != 0)
		return (error);
	if ((((uint32_t)reg[2] << 16) | ((uint32_t)reg[3] << 8) | reg[4]) != addr)
		return (EINVAL);

	return (0);
}

static int
rk_cdn_dp_write_link_config(struct rk_cdn_dp_softc *sc, const char *tag)
{
	uint8_t buf[2];
	int error;

	buf[0] = sc->link_plan_rate_code;
	buf[1] = sc->link_plan_lanes;
	if ((sc->aux_dpcd[DP_MAX_LANE_COUNT] & DP_LANE_COUNT_ENHANCED_FRAME_EN) != 0)
		buf[1] |= DP_LANE_COUNT_ENHANCED_FRAME_EN;

	sc->expected_link_bw_set = buf[0];
	sc->expected_lane_count_set = buf[1];
	error = rk_cdn_dp_mailbox_dpcd_write(sc, DP_LINK_BW_SET, buf, 2);
	if (error != 0)
		return (error);

	device_printf(sc->dev,
	    "%s: wrote DPCD 0x100=0x%02x 0x101=0x%02x\n",
	    tag, sc->expected_link_bw_set, sc->expected_lane_count_set);
	rk_cdn_dp_verify_link_config(sc, tag);
	return (0);
}

static void
rk_cdn_dp_verify_link_config(struct rk_cdn_dp_softc *sc, const char *tag)
{
	uint8_t buf[2];
	int error;

	error = rk_cdn_dp_mailbox_dpcd_read(sc, DP_LINK_BW_SET, buf, sizeof(buf));
	if (error != 0) {
		device_printf(sc->dev,
		    "%s: verify DPCD 0x100/0x101 read failed (%d)\n",
		    tag, error);
		return;
	}
	device_printf(sc->dev,
	    "%s: verify DPCD 0x100=0x%02x 0x101=0x%02x expected=0x%02x/0x%02x%s\n",
	    tag, buf[0], buf[1], sc->expected_link_bw_set,
	    sc->expected_lane_count_set,
	    (buf[0] == sc->expected_link_bw_set &&
	    buf[1] == sc->expected_lane_count_set) ? "" : " MISMATCH");
}

static int
rk_cdn_dp_mailbox_dpcd_read_retry(struct rk_cdn_dp_softc *sc, uint32_t addr,
    uint8_t *buf, uint16_t len, int retries)
{
	uint8_t msg[5], reg[5];
	uint32_t short_expect;
	uint32_t reply_addr;
	uint8_t short_status;
	int error, attempt;

	if (retries <= 0)
		retries = 1;
	error = 0;

	msg[0] = (len >> 8) & 0xff;
	msg[1] = len & 0xff;
	msg[2] = (addr >> 16) & 0xff;
	msg[3] = (addr >> 8) & 0xff;
	msg[4] = addr & 0xff;

	short_expect = ((uint32_t)RK_CDN_DP_DPTX_READ_DPCD << 24) |
	    ((uint32_t)RK_CDN_DP_MB_MODULE_ID_DP_TX << 16) |
	    sizeof(reg);

	for (attempt = 0; attempt < retries; attempt++) {
		sc->mbox_last_header = 0;
		sc->mbox_last_expect = 0;
		sc->mbox_last_body0_3 = 0;
		sc->mbox_last_body4 = 0;

		error = rk_cdn_dp_mailbox_send(sc, RK_CDN_DP_MB_MODULE_ID_DP_TX,
		    RK_CDN_DP_DPTX_READ_DPCD, sizeof(msg), msg);
		if (error != 0)
			return (error);

		error = rk_cdn_dp_mailbox_validate_receive(sc,
		    RK_CDN_DP_MB_MODULE_ID_DP_TX, RK_CDN_DP_DPTX_READ_DPCD,
		    sizeof(reg) + len);
		if (error == 0)
			break;

		if (error == EINVAL && len > 0 &&
		    sc->mbox_last_header == short_expect) {
			uint8_t wire_status = 0xff;
			int aux_err;

			reg[0] = (sc->mbox_last_body0_3 >> 24) & 0xff;
			reg[1] = (sc->mbox_last_body0_3 >> 16) & 0xff;
			reg[2] = (sc->mbox_last_body0_3 >> 8) & 0xff;
			reg[3] = sc->mbox_last_body0_3 & 0xff;
			reg[4] = sc->mbox_last_body4 & 0xff;
			reply_addr = ((uint32_t)reg[0] << 16) |
			    ((uint32_t)reg[1] << 8) | reg[2];
			short_status = reg[3];

			aux_err = rk_cdn_dp_mailbox_get_last_aux_status(sc,
			    &wire_status);
			if (aux_err == 0) {
				if (wire_status == 0x02 || wire_status == 0x08)
					return (EAGAIN);
				if (wire_status == 0x01 || wire_status == 0x04)
					return (EIO);
				if (wire_status == 0x00)
					return (EAGAIN);
				device_printf(sc->dev,
				    "dpcd-read 0x%05x: AUX wire status=0x%02x "
				    "(short reply addr=0x%05x byte3=0x%02x)\n",
				    addr, wire_status, reply_addr,
				    short_status);
				return (EIO);
			}
			if (short_status == 0x02 || short_status == 0x08)
				return (EAGAIN);
			if (short_status == 0x01 || short_status == 0x04)
				return (EIO);
			device_printf(sc->dev,
			    "dpcd-read 0x%05x: aux_status query failed (%d), "
			    "short reply addr=0x%05x status=0x%02x body=%08x/%02x\n",
			    addr, aux_err, reply_addr, short_status,
			    sc->mbox_last_body0_3, sc->mbox_last_body4);
			return (EAGAIN);
		}

		if (error == ETIMEDOUT) {
			device_printf(sc->dev,
			    "dpcd-read 0x%05x: timeout waiting for "
			    "response from firmware (attempt %d)\n",
			    addr, attempt);
			rk_cdn_dp_mailbox_log_state(sc,
			    "dpcd-read timeout state");
		} else {
			device_printf(sc->dev,
			    "dpcd-read 0x%05x hdr mismatch (err=%d): "
			    "got=0x%08x expect=0x%08x\n",
			    addr, error, sc->mbox_last_header,
			    sc->mbox_last_expect);
			rk_cdn_dp_mailbox_log_state(sc,
			    "dpcd-read hdr-mismatch state");
		}
		return (error);
	}

	if (error != 0)
		return (error);

	error = rk_cdn_dp_mailbox_read_receive(sc, reg, sizeof(reg));
	if (error != 0)
		return (error);

	error = rk_cdn_dp_mailbox_read_receive(sc, buf, len);
	return (error);
}

static int
rk_cdn_dp_mailbox_get_firmware(struct rk_cdn_dp_softc *sc)
{
	static const char * const fw_names[] = {
		RK_CDN_DP_FIRMWARE_NAME,
		RK_CDN_DP_FIRMWARE_PATH,
		RK_CDN_DP_FIRMWARE_BASENAME,
		RK_CDN_DP_FIRMWARE_OLDNAME,
	};
	size_t i;

	if (sc->fw != NULL)
		return (0);
	for (i = 0; i < nitems(fw_names); i++) {
		sc->fw = firmware_get_flags(fw_names[i], FIRMWARE_GET_NOWARN);
		if (sc->fw != NULL)
			break;
	}
	if (sc->fw == NULL) {
		device_printf(sc->dev,
		    "missing firmware (%s, %s, %s, %s)\n",
		    RK_CDN_DP_FIRMWARE_NAME, RK_CDN_DP_FIRMWARE_PATH,
		    RK_CDN_DP_FIRMWARE_BASENAME, RK_CDN_DP_FIRMWARE_OLDNAME);
		return (ENOENT);
	}

	return (0);
}

static int
rk_cdn_dp_mailbox_prepare_ucpu(struct rk_cdn_dp_softc *sc)
{
	uint64_t rate;
	int error;

	error = clk_get_freq(sc->clks[RK_CDN_DP_CLK_CORE], &rate);
	if (error != 0)
		return (error);

	rk_cdn_dp_set_fw_clk(sc, rate);
	rk_cdn_dp_clock_reset(sc);

	return (0);
}

static int
rk_cdn_dp_mailbox_load_fw(struct rk_cdn_dp_softc *sc)
{
	return (rk_cdn_dp_load_firmware(sc));
}

static int
rk_cdn_dp_mailbox_enable_events(struct rk_cdn_dp_softc *sc)
{
	return (rk_cdn_dp_event_config(sc));
}

static int
rk_cdn_dp_select_hpd(struct rk_cdn_dp_softc *sc)
{
	int error;

	if (sc->grf == NULL)
		return (ENXIO);

	error = clk_enable(sc->clks[RK_CDN_DP_CLK_GRF]);
	if (error != 0)
		return (error);
	error = SYSCON_WRITE_4(sc->grf, RK_CDN_DP_GRF_SOC_CON26,
	    RK_CDN_DP_DPTX_HPD_SEL_MASK | RK_CDN_DP_DPTX_HPD_SEL);
	(void)clk_disable(sc->clks[RK_CDN_DP_CLK_GRF]);
	return (error);
}

static int
rk_cdn_dp_mailbox_get_hpd_state(struct rk_cdn_dp_softc *sc)
{
	uint8_t status;
	int error;

	error = rk_cdn_dp_mailbox_send(sc, RK_CDN_DP_MB_MODULE_ID_DP_TX,
	    RK_CDN_DP_DPTX_HPD_STATE, 0, NULL);
	if (error != 0)
		return (error);
	error = rk_cdn_dp_mailbox_validate_receive(sc,
	    RK_CDN_DP_MB_MODULE_ID_DP_TX, RK_CDN_DP_DPTX_HPD_STATE,
	    sizeof(status));
	if (error != 0)
		return (error);
	error = rk_cdn_dp_mailbox_read_receive(sc, &status, sizeof(status));
	if (error != 0)
		return (error);

	sc->hpd_status = status;
	RK_CDN_DP_DPRINTF(sc, "mailbox HPD status=%d\n", status);
	if (status <= 0)
		return (ENODEV);
	return (0);
}

/*
 * Query the firmware for the most recent AUX transaction's status byte
 * via DPTX_GET_LAST_AUX_STATUS (opcode 0x0e).
 *
 * Status byte values per DP spec / Cadence firmware:
 *   0x00 = AUX_OK (ACK)
 *   0x01 = AUX_NACK
 *   0x02 = AUX_DEFER
 *   0x04 = I2C_NACK
 *   0x08 = I2C_DEFER
 * If the firmware never even attempted an AUX transaction since boot, the
 * status reflects whatever default it carries (usually 0x00). Most useful
 * called *after* a failed DPCD read to find out what the engine actually saw.
 */
static int
rk_cdn_dp_mailbox_get_last_aux_status(struct rk_cdn_dp_softc *sc,
    uint8_t *status_out)
{
	uint8_t status = 0xff;
	int error;

	error = rk_cdn_dp_mailbox_send(sc, RK_CDN_DP_MB_MODULE_ID_DP_TX,
	    RK_CDN_DP_DPTX_GET_LAST_AUX_STATUS, 0, NULL);
	if (error != 0)
		return (error);
	error = rk_cdn_dp_mailbox_validate_receive(sc,
	    RK_CDN_DP_MB_MODULE_ID_DP_TX,
	    RK_CDN_DP_DPTX_GET_LAST_AUX_STATUS, sizeof(status));
	if (error != 0)
		return (error);
	error = rk_cdn_dp_mailbox_read_receive(sc, &status, sizeof(status));
	if (error != 0)
		return (error);
	if (status_out != NULL)
		*status_out = status;
	RK_CDN_DP_DPRINTF(sc, "last AUX status=0x%02x (%s)\n", status,
	    status == 0x00 ? "ACK" :
	    status == 0x01 ? "NACK" :
	    status == 0x02 ? "DEFER" :
	    status == 0x04 ? "I2C_NACK" :
	    status == 0x08 ? "I2C_DEFER" : "unknown");
	return (0);
}

static int
rk_cdn_dp_sysctl_aux_status(SYSCTL_HANDLER_ARGS)
{
	struct rk_cdn_dp_softc *sc;
	uint8_t status;
	int error, val;

	sc = arg1;
	sx_slock(&sc->detach_sx);
	if (sc->detached) {
		sx_sunlock(&sc->detach_sx);
		return (ENXIO);
	}
	error = rk_cdn_dp_mailbox_get_last_aux_status(sc, &status);
	sx_sunlock(&sc->detach_sx);
	if (error != 0)
		return (error);
	val = status;
	return (sysctl_handle_int(oidp, &val, 0, req));
}

/*
 * Pulse HPD low for 500ms then back high, forcing the sink's DP RX to
 * re-detect us.  Useful when the display's AUX channel has gone deaf —
 * a fresh HPD edge often resets its receiver state.
 *
 * Uses mailbox WRITE_REGISTER instead of direct MMIO: when this sysctl
 * runs out of band (after stages 1..12 have completed and idle time
 * has elapsed), the APB clock that backs SYS_CTL_3 is gated by the
 * firmware and a direct bus_read_4 panics with an external data abort.
 * Routing through the mailbox lets the firmware ungate the APB itself.
 */
static int
rk_cdn_dp_hpd_pulse(struct rk_cdn_dp_softc *sc)
{
	uint32_t base = RK_CDN_DP_SYS_CTL_3_HPD_CTRL;	/* SW-controlled HPD */
	int error;

	error = rk_cdn_dp_mailbox_reg_write(sc, RK_CDN_DP_SYS_CTL_3, base);
	if (error != 0) {
		RK_CDN_DP_DPRINTF(sc,
		    "hpd_pulse: mailbox write SYS_CTL_3 (HPD low) failed (%d)\n",
		    error);
		return (error);
	}
	RK_CDN_DP_DPRINTF(sc, "hpd_pulse: HPD low (SYS_CTL_3=0x%08x)\n",
	    base);

	DELAY(500000);	/* 500ms */

	error = rk_cdn_dp_mailbox_reg_write(sc, RK_CDN_DP_SYS_CTL_3,
	    base | RK_CDN_DP_SYS_CTL_3_F_HPD);
	if (error != 0) {
		RK_CDN_DP_DPRINTF(sc,
		    "hpd_pulse: mailbox write SYS_CTL_3 (HPD high) failed (%d)\n",
		    error);
		return (error);
	}
	RK_CDN_DP_DPRINTF(sc, "hpd_pulse: HPD high (SYS_CTL_3=0x%08x)\n",
	    base | RK_CDN_DP_SYS_CTL_3_F_HPD);

	DELAY(100000);	/* 100ms settle before any AUX */
	return (0);
}

static int
rk_cdn_dp_sysctl_hpd_pulse(SYSCTL_HANDLER_ARGS)
{
	struct rk_cdn_dp_softc *sc;
	int error, val = 0;

	sc = arg1;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (val != 1)
		return (EINVAL);

	sx_slock(&sc->detach_sx);
	if (sc->detached) {
		sx_sunlock(&sc->detach_sx);
		return (ENXIO);
	}
	error = rk_cdn_dp_hpd_pulse(sc);
	sx_sunlock(&sc->detach_sx);
	return (error);
}

/*
 * Try to wake the sink via DPCD SET_POWER (0x600): write D3 (sleep),
 * brief delay, write D0 (active).  Uses the AUX channel itself, so it
 * only helps in the (uncommon) case where the sink ACKs writes but
 * NACKs reads.
 */
static int
rk_cdn_dp_aux_wake(struct rk_cdn_dp_softc *sc)
{
	uint8_t d3 = 0x02, d0 = 0x01;
	int error;

	error = rk_cdn_dp_mailbox_dpcd_write(sc, 0x600, &d3, 1);
	RK_CDN_DP_DPRINTF(sc, "aux_wake: DPCD 0x600 <- D3 result=%d\n",
	    error);
	DELAY(50000);	/* 50ms */
	error = rk_cdn_dp_mailbox_dpcd_write(sc, 0x600, &d0, 1);
	RK_CDN_DP_DPRINTF(sc, "aux_wake: DPCD 0x600 <- D0 result=%d\n",
	    error);
	DELAY(50000);
	return (error);
}

static int
rk_cdn_dp_sysctl_aux_wake(SYSCTL_HANDLER_ARGS)
{
	struct rk_cdn_dp_softc *sc;
	int error, val = 0;

	sc = arg1;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (val != 1)
		return (EINVAL);

	sx_slock(&sc->detach_sx);
	if (sc->detached) {
		sx_sunlock(&sc->detach_sx);
		return (ENXIO);
	}
	error = rk_cdn_dp_aux_wake(sc);
	sx_sunlock(&sc->detach_sx);
	return (error);
}

/*
 * aux_probe: split-diagnostic for the stage-13 timeout.  Issues one
 * WRITE_DPCD (SET_POWER 0x600 <- D0), queries the firmware's last
 * AUX status, then issues one READ_DPCD (DPCD_REV 0x000).  Logs all
 * three results so the failure mode falls out:
 *
 *   write OK  + read OK   -> AUX wholly alive (likely just needed wake)
 *   write OK  + read FAIL -> firmware read-response path broken/misconfigured
 *   write FAIL+ read FAIL -> AUX engine never drives wire (PHY/SBU level)
 *
 * Cheaper to run than a full stage walk and decisive about which layer
 * to attack next.
 */
static const char *rk_cdn_dp_aux_status_name(uint32_t status);

static int
rk_cdn_dp_aux_probe(struct rk_cdn_dp_softc *sc)
{
	uint8_t d0 = 0x01;
	uint8_t dpcd[16];
	uint8_t status;
	int werr, rerr, serr;

	werr = rk_cdn_dp_mailbox_dpcd_write(sc, 0x600, &d0, 1);
	device_printf(sc->dev,
	    "aux_probe: WRITE 0x600 <- D0 result=%d\n", werr);

	DELAY(20000);	/* 20ms — give firmware time to drive wire */

	serr = rk_cdn_dp_mailbox_get_last_aux_status(sc, &status);
	if (serr == 0)
		device_printf(sc->dev,
		    "aux_probe: aux_status=0x%02x (%s)\n", status,
		    rk_cdn_dp_aux_status_name(status));
	else
		device_printf(sc->dev,
		    "aux_probe: aux_status query failed (%d)\n", serr);

	rerr = rk_cdn_dp_mailbox_dpcd_read(sc, 0x000, dpcd, sizeof(dpcd));
	device_printf(sc->dev,
	    "aux_probe: READ 0x000 (DPCD_REV) result=%d\n", rerr);
	if (rerr == 0)
		device_printf(sc->dev,
		    "aux_probe: DPCD[0..7]= %02x %02x %02x %02x %02x %02x %02x %02x\n",
		    dpcd[0], dpcd[1], dpcd[2], dpcd[3],
		    dpcd[4], dpcd[5], dpcd[6], dpcd[7]);

	/* Try a 1-byte read of the same address we just wrote (0x600).
	 * Many displays make SET_POWER readable as well as writable -- if
	 * 0x600 reads back D0 that proves general DPCD reads work.  */
	{
		uint8_t one;
		int err;

		err = rk_cdn_dp_mailbox_dpcd_read(sc, 0x600, &one, 1);
		device_printf(sc->dev,
		    "aux_probe: READ 0x600 (SET_POWER) result=%d val=0x%02x\n",
		    err, err == 0 ? one : 0xff);
	}

	/* Try DPCD 0x68000 (SINK_OUI / branch-device-ID, alternate
	 * receiver-cap region used by some receivers).  */
	{
		uint8_t one;
		int err;

		err = rk_cdn_dp_mailbox_dpcd_read(sc, 0x68000, &one, 1);
		device_printf(sc->dev,
		    "aux_probe: READ 0x68000 result=%d val=0x%02x\n",
		    err, err == 0 ? one : 0xff);
	}

	/*
	 * Post-train sink status block: tells us whether the panel
	 * actually thinks it's receiving valid video on the trained link.
	 *   0x101 LINK_BW_SET (readback) — sink confirms link rate
	 *   0x102 LANE_COUNT_SET (readback) — sink confirms lane count
	 *   0x200 SINK_COUNT
	 *   0x202 LANE0_1_STATUS
	 *   0x203 LANE2_3_STATUS
	 *   0x204 LANE_ALIGN_STATUS_UPDATED
	 *   0x205 SINK_STATUS (bit 0 = SINK_PORT0 sync-locked,
	 *                      bit 1 = RX_PORT1 sync-locked)
	 */
	{
		uint8_t buf[8];
		int err;

		err = rk_cdn_dp_mailbox_dpcd_read(sc, 0x100, buf, 2);
		if (err == 0)
			device_printf(sc->dev,
			    "aux_probe: LINK_BW_SET=0x%02x LANE_COUNT_SET=0x%02x\n",
			    buf[0], buf[1]);
		if (err == 0)
			device_printf(sc->dev,
			    "aux_probe: expected LINK_BW_SET=0x%02x LANE_COUNT_SET=0x%02x%s\n",
			    sc->expected_link_bw_set, sc->expected_lane_count_set,
			    (buf[0] == sc->expected_link_bw_set &&
			    buf[1] == sc->expected_lane_count_set) ? "" : " MISMATCH");

		err = rk_cdn_dp_mailbox_dpcd_read(sc, 0x200, buf, 6);
		if (err == 0)
			device_printf(sc->dev,
			    "aux_probe: SINK_COUNT=0x%02x DEVICE_SVC_IRQ=0x%02x "
			    "LANE0_1_STATUS=0x%02x LANE2_3_STATUS=0x%02x "
			    "ALIGN_STATUS=0x%02x SINK_STATUS=0x%02x\n",
			    buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
	}

	if (werr != 0)
		return (werr);
	return (rerr);
}

static int
rk_cdn_dp_sysctl_aux_probe(SYSCTL_HANDLER_ARGS)
{
	struct rk_cdn_dp_softc *sc;
	int error, val = 0;

	sc = arg1;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (val != 1)
		return (EINVAL);

	sx_slock(&sc->detach_sx);
	if (sc->detached) {
		sx_sunlock(&sc->detach_sx);
		return (ENXIO);
	}
	error = rk_cdn_dp_aux_probe(sc);
	sx_sunlock(&sc->detach_sx);
	return (error);
}

static int
rk_cdn_dp_sysctl_retrain_now(SYSCTL_HANDLER_ARGS)
{
	struct rk_cdn_dp_softc *sc;
	int error, val = 0;

	sc = arg1;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (val != 1)
		return (EINVAL);
	(void)sc;	/* retrain_default looks up softc itself */
	return (rk_cdn_dp_retrain_default());
}

static int
rk_cdn_dp_sysctl_audio_start_now(SYSCTL_HANDLER_ARGS)
{
	int error, val = 0;

	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (val != 1)
		return (EINVAL);
	(void)arg1;
	return (rk_cdn_dp_audio_start(2, 48000, 16));
}

static int
rk_cdn_dp_sysctl_audio_stop_now(SYSCTL_HANDLER_ARGS)
{
	int error, val = 0;

	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (val != 1)
		return (EINVAL);
	(void)arg1;
	return (rk_cdn_dp_audio_stop());
}

/*
 * Walk the cached EDID looking for a CTA-861 extension block and,
 * within it, any Short Audio Descriptors.  This is the source of
 * truth for whether the sink advertises DP audio support — a sink
 * with no SADs in its EDID will silently discard our audio packets
 * regardless of how perfectly we packetize them.
 */
static int
rk_cdn_dp_sysctl_edid_audio_dump(SYSCTL_HANDLER_ARGS)
{
	static const char *fmt_name[16] = {
		"reserved", "LPCM", "AC-3", "MPEG1", "MP3", "MPEG2",
		"AAC LC", "DTS", "ATRAC", "DSD", "E-AC-3", "DTS-HD",
		"MLP/TrueHD", "DST", "WMA Pro", "extended"
	};
	struct rk_cdn_dp_softc *sc;
	int error, val = 0;
	const uint8_t *cta;
	uint8_t dtd_offset, dbc_end;
	unsigned int idx, dbc_off;
	int sad_count = 0;

	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (val != 1)
		return (EINVAL);
	sc = arg1;
	if (sc == NULL)
		return (ENXIO);
	if (!sc->edid_valid || sc->edid_len < 256) {
		RK_CDN_DP_DPRINTF(sc,
		    "edid_audio_dump: EDID not cached or no extension "
		    "block (valid=%d len=%u)\n",
		    (int)sc->edid_valid, (unsigned int)sc->edid_len);
		return (ENXIO);
	}

	cta = &sc->edid[128];
	if (cta[0] != 0x02) {
		RK_CDN_DP_DPRINTF(sc,
		    "edid_audio_dump: ext block tag=0x%02x is not CTA-861 "
		    "(0x02) — no audio info\n", cta[0]);
		return (0);
	}
	dtd_offset = cta[2];
	RK_CDN_DP_DPRINTF(sc,
	    "edid_audio_dump: CTA-861 rev=%u dtd_offset=0x%02x flags=0x%02x "
	    "(YCbCr444=%d YCbCr422=%d basic_audio=%d underscan=%d, ndtd=%d)\n",
	    cta[1], dtd_offset, cta[3],
	    (cta[3] >> 5) & 1, (cta[3] >> 4) & 1,
	    (cta[3] >> 6) & 1, (cta[3] >> 7) & 1,
	    cta[3] & 0x0f);
	if (dtd_offset < 4 || dtd_offset > 128) {
		RK_CDN_DP_DPRINTF(sc,
		    "edid_audio_dump: invalid dtd_offset, bailing\n");
		return (0);
	}
	dbc_end = dtd_offset;
	dbc_off = 4;
	while (dbc_off < dbc_end) {
		uint8_t hdr = cta[dbc_off];
		uint8_t tag = (hdr >> 5) & 0x07;
		uint8_t blen = hdr & 0x1f;

		if (tag == 1) { /* Audio Data Block */
			for (idx = 0; idx + 3 <= blen; idx += 3) {
				const uint8_t *s =
				    &cta[dbc_off + 1 + idx];
				uint8_t fmt = (s[0] >> 3) & 0x0f;
				uint8_t ch  = (s[0] & 0x07) + 1;
				RK_CDN_DP_DPRINTF(sc,
				    "edid_audio_dump:  SAD%d fmt=%u(%s) "
				    "ch=%u rates=0x%02x byte2=0x%02x\n",
				    sad_count, fmt, fmt_name[fmt], ch,
				    s[1], s[2]);
				sad_count++;
			}
		}
		dbc_off += 1 + blen;
		if (blen == 0)
			break;
	}
	if (sad_count == 0) {
		RK_CDN_DP_DPRINTF(sc,
		    "edid_audio_dump: NO Short Audio Descriptors found — "
		    "sink does NOT advertise DP audio support\n");
	} else {
		RK_CDN_DP_DPRINTF(sc,
		    "edid_audio_dump: %d SAD(s) found — sink claims audio "
		    "support (basic_audio=%d for stereo LPCM 32/44.1/48kHz)\n",
		    sad_count, (cta[3] >> 6) & 1);
	}
	return (0);
}

/*
 * Read back the audio packetizer + mute state via the mailbox plus
 * the direct-MMIO audio sub-block + SDP PIF slot 0 status.  Lets us
 * confirm that audio_start_now actually armed the cadence side
 * (without trusting the no-status-feedback mailbox write path).
 */
static int
rk_cdn_dp_sysctl_audio_dump(SYSCTL_HANDLER_ARGS)
{
	struct rk_cdn_dp_softc *sc;
	int error, val = 0;
	uint32_t pack_ctrl = 0, vb_id = 0;
	uint32_t pack_status = 0, vif_status = 0;
	uint32_t stuff_0 = 0, stuff_1 = 0;
	uint32_t src_cntl, src_cnfg, smpl_cntl, smpl_cnfg;
	uint32_t fifo_cntl, com_ch, spdif_ctrl;
	uint32_t pif_alloc, pif_wr_en, pif_wr_addr, pif_wr_req;
	int e_pack, e_vb, e_pkst, e_vif, e_s0, e_s1;

	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (val != 1)
		return (EINVAL);
	sc = arg1;
	if (sc == NULL)
		return (ENXIO);

	e_pack = rk_cdn_dp_mailbox_reg_read(sc, RK_CDN_DP_AUDIO_PACK_CONTROL,
	    &pack_ctrl);
	e_vb   = rk_cdn_dp_mailbox_reg_read(sc, RK_CDN_DP_DP_VB_ID, &vb_id);
	e_pkst = rk_cdn_dp_mailbox_reg_read(sc, RK_CDN_DP_AUDIO_PACK_STATUS,
	    &pack_status);
	e_vif  = rk_cdn_dp_mailbox_reg_read(sc, RK_CDN_DP_VIF_STATUS,
	    &vif_status);
	e_s0   = rk_cdn_dp_mailbox_reg_read(sc, RK_CDN_DP_PCK_STUFF_STATUS_0,
	    &stuff_0);
	e_s1   = rk_cdn_dp_mailbox_reg_read(sc, RK_CDN_DP_PCK_STUFF_STATUS_1,
	    &stuff_1);

	src_cntl    = rk_cdn_dp_read_4(sc, RK_CDN_DP_AUDIO_SRC_CNTL);
	src_cnfg    = rk_cdn_dp_read_4(sc, RK_CDN_DP_AUDIO_SRC_CNFG);
	smpl_cntl   = rk_cdn_dp_read_4(sc, RK_CDN_DP_SMPL2PKT_CNTL);
	smpl_cnfg   = rk_cdn_dp_read_4(sc, RK_CDN_DP_SMPL2PKT_CNFG);
	fifo_cntl   = rk_cdn_dp_read_4(sc, RK_CDN_DP_FIFO_CNTL);
	com_ch      = rk_cdn_dp_read_4(sc, RK_CDN_DP_COM_CH_STTS_BITS);
	spdif_ctrl  = rk_cdn_dp_read_4(sc, RK_CDN_DP_SPDIF_CTRL_ADDR);
	pif_alloc   = rk_cdn_dp_read_4(sc, RK_CDN_DP_SOURCE_PIF_PKT_ALLOC_REG);
	pif_wr_en   = rk_cdn_dp_read_4(sc, RK_CDN_DP_SOURCE_PIF_PKT_ALLOC_WR_EN);
	pif_wr_addr = rk_cdn_dp_read_4(sc, RK_CDN_DP_SOURCE_PIF_WR_ADDR);
	pif_wr_req  = rk_cdn_dp_read_4(sc, RK_CDN_DP_SOURCE_PIF_WR_REQ);

	RK_CDN_DP_DPRINTF(sc,
	    "audio_dump: mb AUDIO_PACK_CONTROL=0x%08x (e=%d, want bit8=1)\n",
	    pack_ctrl, e_pack);
	RK_CDN_DP_DPRINTF(sc,
	    "audio_dump: mb DP_VB_ID=0x%08x (e=%d, want bit4=0 unmuted)\n",
	    vb_id, e_vb);
	RK_CDN_DP_DPRINTF(sc,
	    "audio_dump: AUDIO_SRC_CNTL=0x%08x (want bit1=I2S_DEC_START)\n",
	    src_cntl);
	RK_CDN_DP_DPRINTF(sc,
	    "audio_dump: AUDIO_SRC_CNFG=0x%08x\n", src_cnfg);
	RK_CDN_DP_DPRINTF(sc,
	    "audio_dump: SMPL2PKT_CNTL=0x%08x (want bit1=SMPL2PKT_EN)\n",
	    smpl_cntl);
	RK_CDN_DP_DPRINTF(sc,
	    "audio_dump: SMPL2PKT_CNFG=0x%08x\n", smpl_cnfg);
	RK_CDN_DP_DPRINTF(sc,
	    "audio_dump: FIFO_CNTL=0x%08x  COM_CH_STTS=0x%08x  SPDIF_CTRL=0x%08x\n",
	    fifo_cntl, com_ch, spdif_ctrl);
	RK_CDN_DP_DPRINTF(sc,
	    "audio_dump: PIF_PKT_ALLOC=0x%08x (want TYPE_VALID+PACKET_TYPE=0x84)\n",
	    pif_alloc);
	RK_CDN_DP_DPRINTF(sc,
	    "audio_dump: PIF_WR_EN=0x%08x  WR_ADDR=0x%08x  WR_REQ=0x%08x\n",
	    pif_wr_en, pif_wr_addr, pif_wr_req);
	RK_CDN_DP_DPRINTF(sc,
	    "audio_dump: mb AUDIO_PACK_STATUS=0x%08x (e=%d) VIF_STATUS=0x%08x (e=%d)\n",
	    pack_status, e_pkst, vif_status, e_vif);
	RK_CDN_DP_DPRINTF(sc,
	    "audio_dump: mb PCK_STUFF_STATUS_0=0x%08x (e=%d) STATUS_1=0x%08x (e=%d)\n",
	    stuff_0, e_s0, stuff_1, e_s1);

	return (0);
}

/*
 * display_power: DPMS-style on/off via DPCD SET_POWER (DPCD addr 0x600).
 *   write 0 -> D3 (sleep) — tells sink to power down
 *   write 1 -> D0 (active) + auto retrain — wakes sink + re-establishes link
 *
 * Reading returns the last value we set (sc->display_power_state).  We don't
 * query the sink because some DP sinks NAK reads of 0x600 while asleep.
 */
static int
rk_cdn_dp_sysctl_display_power(SYSCTL_HANDLER_ARGS)
{
	struct rk_cdn_dp_softc *sc;
	uint8_t dpcd_val;
	int error, val;

	sc = arg1;
	val = sc->display_power_state;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (val != 0 && val != 1)
		return (EINVAL);

	sx_slock(&sc->detach_sx);
	if (sc->detached) {
		sx_sunlock(&sc->detach_sx);
		return (ENXIO);
	}
	dpcd_val = (val == 1) ? 0x01 /* D0 */ : 0x02 /* D3 */;
	error = rk_cdn_dp_mailbox_dpcd_write(sc, 0x600, &dpcd_val, 1);
	device_printf(sc->dev,
	    "display_power: DPCD 0x600 <- 0x%02x (%s) result=%d\n",
	    dpcd_val, (val == 1) ? "D0" : "D3", error);
	sx_sunlock(&sc->detach_sx);
	if (error != 0)
		return (error);
	sc->display_power_state = (uint8_t)val;
	if (val == 1) {
		/* Wake: re-train the link in case sink dropped CR/EQ in D3. */
		DELAY(10000);
		error = rk_cdn_dp_retrain_default();
		if (error != 0)
			device_printf(sc->dev,
			    "display_power: wake retrain failed (%d)\n",
			    error);
	}
	return (error);
}

/*
 * backlight_power: AUX-channel backlight on/off via DPCD 0x720
 * (EDP_DISPLAY_CONTROL_REGISTER, bit 0 = BL_ENABLE).  Defined in eDP 1.4+
 * spec; many USB-C DP panels support the same register range.  If the
 * sink doesn't, the write will NAK and we return the error untranslated
 * so userspace can see it.
 */
static int
rk_cdn_dp_sysctl_backlight_power(SYSCTL_HANDLER_ARGS)
{
	struct rk_cdn_dp_softc *sc;
	uint8_t dpcd_val;
	int error, val;

	sc = arg1;
	val = sc->backlight_state;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (val != 0 && val != 1)
		return (EINVAL);

	sx_slock(&sc->detach_sx);
	if (sc->detached) {
		sx_sunlock(&sc->detach_sx);
		return (ENXIO);
	}
	dpcd_val = (val == 1) ? 0x01 : 0x00;
	error = rk_cdn_dp_mailbox_dpcd_write(sc, 0x720, &dpcd_val, 1);
	device_printf(sc->dev,
	    "backlight_power: DPCD 0x720 <- 0x%02x (%s) result=%d\n",
	    dpcd_val, (val == 1) ? "on" : "off", error);
	sx_sunlock(&sc->detach_sx);
	if (error != 0)
		return (error);
	sc->backlight_state = (uint8_t)val;
	return (0);
}

/*
 * Generic DPCD AUX write/read sysctls.  Pair an address (uint16_t —
 * full DPCD address space, including eDP backlight 0x720+ and vendor
 * ranges 0x80000+) with an 8-bit value, then trigger via dpcd_write_now.
 *
 * Backed by sc->debug_reg_addr / sc->debug_reg_value (same storage as
 * the cdn-dp mailbox-register pokers — distinct sysctls, separate
 * trigger).  Userspace flow:
 *
 *   sysctl dev.rk_cdn_dp.0.debug_reg_addr=0x720      # DPCD addr
 *   sysctl dev.rk_cdn_dp.0.debug_reg_value=0x00      # byte to write
 *   sysctl dev.rk_cdn_dp.0.dpcd_write_now=1
 *
 * For reads:
 *   sysctl dev.rk_cdn_dp.0.debug_reg_addr=0x720
 *   sysctl dev.rk_cdn_dp.0.dpcd_read_now=1
 *   sysctl -n dev.rk_cdn_dp.0.debug_reg_value      # result here
 */
static int
rk_cdn_dp_sysctl_dpcd_write_now(SYSCTL_HANDLER_ARGS)
{
	struct rk_cdn_dp_softc *sc;
	uint8_t byte;
	uint32_t addr;
	int error, val = 0;

	sc = arg1;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (val != 1)
		return (EINVAL);

	sx_slock(&sc->detach_sx);
	if (sc->detached) {
		sx_sunlock(&sc->detach_sx);
		return (ENXIO);
	}
	addr = sc->debug_reg_addr;
	byte = (uint8_t)(sc->debug_reg_value & 0xff);
	error = rk_cdn_dp_mailbox_dpcd_write(sc, addr, &byte, 1);
	device_printf(sc->dev,
	    "dpcd_write: DPCD 0x%05x <- 0x%02x result=%d\n",
	    addr, byte, error);
	sx_sunlock(&sc->detach_sx);
	return (error);
}

static int
rk_cdn_dp_sysctl_dpcd_read_now(SYSCTL_HANDLER_ARGS)
{
	struct rk_cdn_dp_softc *sc;
	uint8_t byte;
	uint32_t addr;
	int error, val = 0;
	ssize_t got;

	sc = arg1;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (val != 1)
		return (EINVAL);

	sx_slock(&sc->detach_sx);
	if (sc->detached) {
		sx_sunlock(&sc->detach_sx);
		return (ENXIO);
	}
	addr = sc->debug_reg_addr;
	byte = 0;
	got = rk_cdn_dp_mailbox_dpcd_read(sc, addr, &byte, 1);
	if (got != 1) {
		device_printf(sc->dev,
		    "dpcd_read: DPCD 0x%05x failed (got=%zd)\n", addr, got);
		sx_sunlock(&sc->detach_sx);
		return (EIO);
	}
	sc->debug_reg_value = byte;
	device_printf(sc->dev,
	    "dpcd_read: DPCD 0x%05x = 0x%02x\n", addr, byte);
	sx_sunlock(&sc->detach_sx);
	return (0);
}

/*
 * probe_warm: minimal non-destructive bring-up so userspace can read
 * cdn-dp APB / mailbox MMIO without (a) advancing the stage machine,
 * (b) resetting the µCPU, or (c) cycling cdn-dp resets.  Used to capture
 * U-Boot leftover firmware/framer state via memread32 while the panel
 * is still being driven by U-Boot's setup.
 *
 *   - clk_enable() is reference-counted in FreeBSD's clk framework, so
 *     calling it on an already-enabled clk is a no-op increment.
 *   - We skip rk_cdn_dp_deassert_resets() because it asserts then
 *     deasserts — that's a hard reset cycle that would clobber state.
 *   - power domain is already on at boot (verified in dmesg).
 */
static int
rk_cdn_dp_sysctl_probe_warm(SYSCTL_HANDLER_ARGS)
{
	struct rk_cdn_dp_softc *sc;
	int error, val = 0;

	sc = arg1;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (val != 1)
		return (EINVAL);

	sx_slock(&sc->detach_sx);
	if (sc->detached) {
		sx_sunlock(&sc->detach_sx);
		return (ENXIO);
	}
	if (sc->clks[0] == NULL) {
		error = rk_cdn_dp_get_clocks(sc);
		if (error != 0) {
			device_printf(sc->dev,
			    "probe_warm: get_clocks failed (%d)\n", error);
			goto out;
		}
	}
	if (!sc->clks_enabled) {
		error = rk_cdn_dp_enable_clocks(sc);
		if (error != 0) {
			device_printf(sc->dev,
			    "probe_warm: enable_clocks failed (%d)\n", error);
			goto out;
		}
	}
	RK_CDN_DP_DPRINTF(sc,
	    "probe_warm: clocks ready (skip resets/fw); APB MMIO at 0xfec02000+ now readable\n");
out:
	sx_sunlock(&sc->detach_sx);
	return (error);
}

static int
rk_cdn_dp_sysctl_framer_dump(SYSCTL_HANDLER_ARGS)
{
	struct rk_cdn_dp_softc *sc;
	int error, val = 0;
	static const struct { uint16_t addr; const char *name; } regs[] = {
		{ RK_CDN_DP_DP_FRAMER_GLOBAL_CONFIG, "DP_FRAMER_GLOBAL_CONFIG" },
		{ RK_CDN_DP_DP_TX_PHY_CONFIG_REG, "DP_TX_PHY_CONFIG_REG" },
		{ RK_CDN_DP_DPTX_LANE_EN, "DPTX_LANE_EN" },
		{ RK_CDN_DP_DPTX_ENHNCD, "DPTX_ENHNCD" },
		{ RK_CDN_DP_DP_FRAMER_TU, "DP_FRAMER_TU" },
		{ RK_CDN_DP_DP_FRAMER_PXL_REPR, "DP_FRAMER_PXL_REPR" },
		{ RK_CDN_DP_DP_FRAMER_SP, "DP_FRAMER_SP" },
		{ RK_CDN_DP_DP_FRONT_BACK_PORCH, "DP_FRONT_BACK_PORCH" },
		{ RK_CDN_DP_DP_BYTE_COUNT, "DP_BYTE_COUNT" },
		{ RK_CDN_DP_MSA_HORIZONTAL_0, "MSA_HORIZONTAL_0" },
		{ RK_CDN_DP_MSA_HORIZONTAL_1, "MSA_HORIZONTAL_1" },
		{ RK_CDN_DP_MSA_VERTICAL_0, "MSA_VERTICAL_0" },
		{ RK_CDN_DP_MSA_VERTICAL_1, "MSA_VERTICAL_1" },
		{ RK_CDN_DP_MSA_MISC, "MSA_MISC" },
		{ RK_CDN_DP_STREAM_CONFIG, "STREAM_CONFIG" },
		{ RK_CDN_DP_DP_HORIZONTAL, "DP_HORIZONTAL" },
		{ RK_CDN_DP_DP_VERTICAL_0, "DP_VERTICAL_0" },
		{ RK_CDN_DP_DP_VERTICAL_1, "DP_VERTICAL_1" },
	};

	sc = arg1;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (val != 1)
		return (EINVAL);

	sx_slock(&sc->detach_sx);
	if (sc->detached) {
		sx_sunlock(&sc->detach_sx);
		return (ENXIO);
	}
	RK_CDN_DP_DPRINTF(sc,
	    "framer_dump: trained=%d video_configured=%d rate_code=0x%x lanes=%u pixel_clock=%u\n",
	    sc->link_trained ? 1 : 0, sc->video_configured ? 1 : 0,
	    sc->link_plan_rate_code, sc->link_plan_lanes, sc->pixel_clock_khz);
	for (size_t i = 0; i < nitems(regs); i++) {
		uint32_t v = 0xdeadbeef;
		int e = rk_cdn_dp_mailbox_reg_read(sc, regs[i].addr, &v);
		device_printf(sc->dev,
		    "  %-24s [0x%04x] = 0x%08x%s\n",
		    regs[i].name, regs[i].addr, v,
		    e == 0 ? "" : " (mailbox error)");
		if (e != 0)
			break;
	}
	sx_sunlock(&sc->detach_sx);
	return (0);
}

static int
rk_cdn_dp_sysctl_reg_addr(SYSCTL_HANDLER_ARGS)
{
	struct rk_cdn_dp_softc *sc;
	int error;
	uint32_t val;

	sc = arg1;
	val = sc->debug_reg_addr;
	error = sysctl_handle_32(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	sc->debug_reg_addr = val & 0xffff;
	return (0);
}

static void
rk_cdn_dp_log_phy_config(struct rk_cdn_dp_softc *sc, const char *tag,
    uint32_t phy_config)
{
	device_printf(sc->dev,
	    "%s: phy_config=0x%08x train_en=%u pattern=%u scrambler_bypass=%u "
	    "encoder_bypass=%u skew_bypass=%u disparity_rst=%u "
	    "lane_skew=%u/%u/%u/%u ten_bit=%u\n",
	    tag, phy_config,
	    phy_config & 0x1,
	    (phy_config >> 1) & 0xf,
	    (phy_config >> 5) & 0x1,
	    (phy_config >> 6) & 0x1,
	    (phy_config >> 7) & 0x1,
	    (phy_config >> 8) & 0x1,
	    (phy_config >> 9) & 0x7,
	    (phy_config >> 12) & 0x7,
	    (phy_config >> 15) & 0x7,
	    (phy_config >> 18) & 0x7,
	    (phy_config >> 21) & 0x1);
}

static int
rk_cdn_dp_sysctl_reg_value(SYSCTL_HANDLER_ARGS)
{
	struct rk_cdn_dp_softc *sc;
	int error;
	uint32_t val;

	sc = arg1;
	val = sc->debug_reg_value;
	error = sysctl_handle_32(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	sc->debug_reg_value = val;
	return (0);
}

static int
rk_cdn_dp_sysctl_reg_read(SYSCTL_HANDLER_ARGS)
{
	struct rk_cdn_dp_softc *sc;
	int error, val;
	uint32_t regv;

	sc = arg1;
	val = 0;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (val != 1)
		return (EINVAL);

	sx_slock(&sc->detach_sx);
	if (sc->detached) {
		sx_sunlock(&sc->detach_sx);
		return (ENXIO);
	}
	error = rk_cdn_dp_mailbox_reg_read(sc, (uint16_t)sc->debug_reg_addr, &regv);
	if (error == 0) {
		sc->debug_reg_value = regv;
		RK_CDN_DP_DPRINTF(sc,
		    "debug_reg: read [0x%04x] = 0x%08x\n",
		    (uint16_t)sc->debug_reg_addr, regv);
	}
	sx_sunlock(&sc->detach_sx);
	return (error);
}

static int
rk_cdn_dp_sysctl_reg_write(SYSCTL_HANDLER_ARGS)
{
	struct rk_cdn_dp_softc *sc;
	int error, val;

	sc = arg1;
	val = 0;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (val != 1)
		return (EINVAL);

	sx_slock(&sc->detach_sx);
	if (sc->detached) {
		sx_sunlock(&sc->detach_sx);
		return (ENXIO);
	}
	error = rk_cdn_dp_mailbox_reg_write(sc, (uint16_t)sc->debug_reg_addr,
	    sc->debug_reg_value);
	if (error == 0)
		RK_CDN_DP_DPRINTF(sc,
		    "debug_reg: write [0x%04x] = 0x%08x\n",
		    (uint16_t)sc->debug_reg_addr, sc->debug_reg_value);
	sx_sunlock(&sc->detach_sx);
	return (error);
}

static int
rk_cdn_dp_sysctl_edid_now(SYSCTL_HANDLER_ARGS)
{
	struct rk_cdn_dp_softc *sc;
	int error, val = 0;

	sc = arg1;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (val != 1)
		return (EINVAL);

	sx_slock(&sc->detach_sx);
	if (sc->detached) {
		sx_sunlock(&sc->detach_sx);
		return (ENXIO);
	}
	error = rk_cdn_dp_read_edid(sc);
	sx_sunlock(&sc->detach_sx);
	return (error);
}

static void
rk_cdn_dp_get_hostcap_config(struct rk_cdn_dp_softc *sc, uint8_t *lanes,
    bool *flip, int *usb_ss)
{
	struct fusb302_typec_status typec;
	struct rk3399_typec_dp_altmode_status altmode;

	/*
	 * USB_SS encoding:
	 *   USB_SS = 1 => USB3 + DP => 2 lanes
	 *   USB_SS = 0 => DP-only    => 4 lanes
	 *
	 * Exposed explicitly through dev.rk_cdn_dp.0.hostcap_usb_ss until
	 * a native FreeBSD connector-property provider exists.
	 *
	 * Prefer DP-only / 4-lane by default. That matches the passive-cable
	 * fallback exported by fusb302 and is the safer baseline until the
	 * Type-C provider supplies an explicit USB_SS value.
	 */
	*lanes = 4;
	*flip = false;
	if (usb_ss != NULL)
		*usb_ss = 0;

	if (rk_cdn_dp_get_typec_status(sc, &typec)) {
		if (typec.orientation == FUSB302_TYPEC_ORIENT_CC2)
			*flip = true;
	}
	if (rk_cdn_dp_get_altmode_status(sc, &altmode)) {
		if (altmode.usb_ss >= 0) {
			*lanes = altmode.usb_ss != 0 ? 2 : 4;
			if (usb_ss != NULL)
				*usb_ss = altmode.usb_ss;
		}
	}

	if (sc->hostcap_usb_ss_override >= 0) {
		*lanes = sc->hostcap_usb_ss_override != 0 ? 2 : 4;
		if (usb_ss != NULL)
			*usb_ss = sc->hostcap_usb_ss_override != 0 ? 1 : 0;
	}
	if (sc->hostcap_lanes_override != 0) {
		*lanes = sc->hostcap_lanes_override;
		if (usb_ss != NULL)
			*usb_ss = sc->hostcap_lanes_override == 2 ? 1 : 0;
	}
	if (sc->hostcap_flip_override >= 0)
		*flip = sc->hostcap_flip_override != 0;
}

static int
rk_cdn_dp_mailbox_set_host_cap(struct rk_cdn_dp_softc *sc)
{
	uint8_t lanes;
	bool flip;
	int usb_ss;

	rk_cdn_dp_get_hostcap_config(sc, &lanes, &flip, &usb_ss);

	RK_CDN_DP_DPRINTF(sc,
	    "host-cap using lanes=%u flip=%u usb_ss=%d%s%s\n",
	    lanes, flip ? 1 : 0, usb_ss,
	    sc->hostcap_flip_override >= 0 ? " flip-override" : "",
	    sc->hostcap_lanes_override != 0 ? " lanes-override" : "");
	rk_cdn_dp_mailbox_log_state(sc, "before set_host_cap");
	return (rk_cdn_dp_set_host_cap(sc, lanes, flip));
}

static int
rk_cdn_dp_mailbox_drain_events(struct rk_cdn_dp_softc *sc)
{
	uint8_t event[2];
	uint32_t pending;
	int error, i;

	for (i = 0; i < 16; i++) {
		pending = rk_cdn_dp_read_4(sc, RK_CDN_DP_SW_EVENTS0);
		if (pending == 0)
			return (0);

		error = rk_cdn_dp_mailbox_send(sc, RK_CDN_DP_MB_MODULE_ID_DP_TX,
		    RK_CDN_DP_DPTX_READ_EVENT, 0, NULL);
		if (error != 0)
			return (error);
		error = rk_cdn_dp_mailbox_validate_receive(sc,
		    RK_CDN_DP_MB_MODULE_ID_DP_TX, RK_CDN_DP_DPTX_READ_EVENT,
		    sizeof(event));
		if (error != 0)
			return (error);
		error = rk_cdn_dp_mailbox_read_receive(sc, event, sizeof(event));
		if (error != 0)
			return (error);
	}

	return (EAGAIN);
}

static int
rk_cdn_dp_mailbox_settle_post_hostcap(struct rk_cdn_dp_softc *sc)
{
	uint32_t ka_prev, ka_cur, pending, full;
	int error, i;

	ka_prev = rk_cdn_dp_read_4(sc, RK_CDN_DP_KEEP_ALIVE);
	for (i = 0; i < 50; i++) {
		rk_cdn_dp_mailbox_drain(sc, 64);
		pending = rk_cdn_dp_read_4(sc, RK_CDN_DP_SW_EVENTS0);
		full = rk_cdn_dp_read_4(sc, RK_CDN_DP_MAILBOX_FULL_ADDR);
		if (pending != 0) {
			error = rk_cdn_dp_mailbox_drain_events(sc);
			if (error != 0)
				return (error);
			rk_cdn_dp_mailbox_drain(sc, 64);
			pending = rk_cdn_dp_read_4(sc, RK_CDN_DP_SW_EVENTS0);
			full = rk_cdn_dp_read_4(sc, RK_CDN_DP_MAILBOX_FULL_ADDR);
		}
		ka_cur = rk_cdn_dp_read_4(sc, RK_CDN_DP_KEEP_ALIVE);
		if (pending == 0 && full == 0 && ka_cur != 0 &&
		    (ka_cur != ka_prev || i >= 2))
			return (0);
		ka_prev = ka_cur;
		DELAY(2000);
	}

	device_printf(sc->dev,
	    "post-hostcap settle timed out: SW_EVENTS0=0x%x FULL=0x%x KEEP_ALIVE=0x%08x\n",
	    rk_cdn_dp_read_4(sc, RK_CDN_DP_SW_EVENTS0),
	    rk_cdn_dp_read_4(sc, RK_CDN_DP_MAILBOX_FULL_ADDR),
	    rk_cdn_dp_read_4(sc, RK_CDN_DP_KEEP_ALIVE));
	return (ETIMEDOUT);
}

static int
rk_cdn_dp_wait_sink_ready(struct rk_cdn_dp_softc *sc)
{
	uint8_t sink_count;
	int error, i;

	/*
	 * Poll DPCD 0x200 (DP_SINK_COUNT) every ~10ms for up to ~5s.
	 * Tolerate any kind of DPCD failure (NACK/DEFER/timeout/short-reply)
	 * and just retry — sinks should come up within 1ms but some docks
	 * need more time. Some displays NACK DPCD 0x000 reads until their
	 * DP RX is fully up; this poll gives them time to settle.
	 *
	 * 5s with a 10ms cadence (500 iterations).
	 *
	 * Early-bail: if the mailbox times out repeatedly with KEEP_ALIVE
	 * frozen, the dptx firmware has wedged and no further polling will
	 * help — return ETIMEDOUT immediately rather than burning seconds
	 * per iteration (which blocks shutdown for tens of minutes).
	 */
	uint32_t fw_dead_ka = 0;
	int fw_dead_streak = 0;
	for (i = 0; i < 500; i++) {
		error = rk_cdn_dp_mailbox_dpcd_read_retry(sc,
		    RK_CDN_DP_DPCD_SINK_COUNT, &sink_count, 1, 1);
		if (error == 0) {
			if ((sink_count & 0x3f) != 0)
				return (0);
			fw_dead_streak = 0;
		} else {
			uint32_t ka = rk_cdn_dp_read_4(sc, RK_CDN_DP_KEEP_ALIVE);
			if (error == ETIMEDOUT && ka == fw_dead_ka)
				fw_dead_streak++;
			else
				fw_dead_streak = (error == ETIMEDOUT) ? 1 : 0;
			fw_dead_ka = ka;
			if (i == 0 || i % 50 == 49) {
				RK_CDN_DP_DPRINTF(sc,
				    "sink_count poll: error=%d (try %d) "
				    "ka=0x%08x streak=%d\n",
				    error, i, ka, fw_dead_streak);
				if (i == 0)
					rk_cdn_dp_log_typec_state(sc,
					    "sink_count poll typec");
				rk_cdn_dp_mailbox_log_state(sc,
				    "sink_count poll state");
			}
			if (fw_dead_streak >= 3) {
				RK_CDN_DP_DPRINTF(sc,
				    "sink_count poll: firmware wedged "
				    "(ka frozen at 0x%08x for %d straight "
				    "timeouts) — bailing\n",
				    fw_dead_ka, fw_dead_streak);
				return (ETIMEDOUT);
			}
		}
		DELAY(10000);	/* 10ms */
	}

	return (ETIMEDOUT);
}

static int
rk_cdn_dp_mailbox_probe_dpcd_caps(struct rk_cdn_dp_softc *sc)
{
	struct rk3399_typec_dp_altmode_status altmode;
	int error;

	/*
	 * Do not wait for KEEP_ALIVE to change here.  The CDN-DP firmware
	 * only increments KEEP_ALIVE while actively processing mailbox
	 * commands; it stops when idle (waiting for the next command).
	 * Requiring KEEP_ALIVE to change before sending READ_DPCD creates a
	 * deadlock: we wait for the firmware to prove it's alive, but the
	 * firmware is waiting for us to give it work.  Just send the command
	 * and wait for the response mailbox to fill.
	 */
	if (rk_cdn_dp_get_altmode_status(sc, &altmode)) {
		if (!rk_cdn_dp_altmode_signature_ok(&altmode))
			return (EAGAIN);
	}
	rk_cdn_dp_log_typec_state(sc, "before wait_sink_ready");

	/*
	 * Poll DPCD 0x200 (SINK_COUNT) until the display reports a non-zero
	 * sink count, THEN read DP_DPCD_REV (0x000) for the receiver caps.
	 * Some displays NACK the 0x000 read until their DP RX is fully up;
	 * the SINK_COUNT poll is what gives them time to settle.
	 */
	error = rk_cdn_dp_wait_sink_ready(sc);
	if (error != 0) {
		device_printf(sc->dev,
		    "wait_sink_ready failed (%d) — proceeding to DPCD 0 anyway\n",
		    error);
		rk_cdn_dp_mailbox_log_state(sc, "after wait_sink_ready failure");
		/* Fall through to the DPCD 0x000 read so we still get a
		 * diagnostic, but don't fail stage 13 outright. */
	}

	/*
	 * Retry the DPCD 0x000 (DP_DPCD_REV) read with the same long-poll
	 * tolerance.  Don't filter on error type; firmware returns
	 * ETIMEDOUT (warm-up), EPROTO (short reply), EAGAIN (DEFER), or
	 * success.
	 */
	for (int dpcd_try = 0; dpcd_try < 100; dpcd_try++) {
		if (dpcd_try == 0)
			rk_cdn_dp_mailbox_log_state(sc, "before dpcd-read 0x000");
		error = rk_cdn_dp_mailbox_dpcd_read(sc, 0x000, sc->aux_dpcd,
		    sizeof(sc->aux_dpcd));
		if (error == 0)
			break;
		if (dpcd_try == 0 || dpcd_try % 10 == 9) {
			device_printf(sc->dev,
			    "mailbox dpcd-read 0x000 failed (%d) try %d\n",
			    error, dpcd_try);
			rk_cdn_dp_mailbox_log_state(sc,
			    "after dpcd-read 0x000 failure");
		}
		DELAY(100000); /* 100ms between retries */
	}
	if (error != 0) {
		device_printf(sc->dev,
		    "mailbox dpcd-read 0x000 still failing (%d) after retries\n",
		    error);
		return (error);
	}
	rk_cdn_dp_mailbox_log_state(sc, "after dpcd-read 0x000 success");
	rk_cdn_dp_record_sink_caps(sc);

	return (0);
}

static bool
rk_cdn_dp_altmode_signature_ok(
    const struct rk3399_typec_dp_altmode_status *status)
{

	if (status == NULL || !status->valid || !status->dp_ready)
		return (false);

	/*
	 * Accept any DisplayPort-capable pin assignment (C/D/E/F = 0x04/0x08/
	 * 0x10/0x20) with HPD asserted in dp_status (bit 7).
	 *
	 * Real partners on this board report pin_assignment=0x4 (PIN_C,
	 * 2-lane DP+USB3) with dp_status=0x8a→0x18a; the legacy RockPro64
	 * manual-test rig reported 0x8/0x9a.  An earlier check hardcoded the
	 * manual-test values and rejected real partners; gate on capability
	 * bits instead so any DP-capable partner can advance.
	 */
	if ((status->pin_assignment & 0x3c) == 0)
		return (false);
	if ((status->dp_status & (1u << 7)) == 0)
		return (false);

	return (true);
}

static void
rk_cdn_dp_log_typec_state(struct rk_cdn_dp_softc *sc, const char *tag)
{
	struct fusb302_typec_status typec;
	struct rk3399_typec_dp_altmode_status altmode;
	uint8_t lanes;
	bool flip;
	int usb_ss;
	bool have_typec;
	bool have_altmode;

	have_typec = rk_cdn_dp_get_typec_status(sc, &typec);
	have_altmode = rk_cdn_dp_get_altmode_status(sc, &altmode);
	rk_cdn_dp_get_hostcap_config(sc, &lanes, &flip, &usb_ss);

	if (!bootverbose)
		return;
	device_printf(sc->dev,
	    "%s: extcon=%s typec=%s altmode=%s attached=%d role=%d orient=%d state_valid=%d dp_ready=%d pin=0x%x usb_ss=%d dp_status=0x%x host_lanes=%u host_flip=%u\n",
	    tag,
	    sc->extcon_dev != NULL ? device_get_nameunit(sc->extcon_dev) : "none",
	    have_typec ? "yes" : "no",
	    have_altmode ? "yes" : "no",
	    have_typec ? (int)typec.attached : -1,
	    have_typec ? (int)typec.role : -1,
	    have_typec ? (int)typec.orientation : -1,
	    have_typec ? (int)typec.state_valid : 0,
	    have_altmode ? (int)altmode.dp_ready : -1,
	    have_altmode ? altmode.pin_assignment : 0,
	    usb_ss,
	    have_altmode ? altmode.dp_status : 0,
	    lanes, flip ? 1 : 0);
}

/*
 * rk_cdn_dp_force_typec_dp
 *
 * Returns true when the loader tunable hw.rk3399_typec_dp_force is set.
 * Used during bring-up to assert HPD in software when no physical USB-C sink
 * is connected, bypassing the normal fusb302 Alt Mode negotiation path.
 * Without this escape hatch, AUX transactions cannot be tested until the full
 * Type-C policy stack is wired up.
 */
static bool
rk_cdn_dp_force_typec_dp(void)
{
	return (rk_cdn_dp_tunable_flag("hw.rk3399_typec_dp_force", 0));
}

/*
 * rk_cdn_dp_hpd_status
 *
 * Reads the live HPD pin state from SYS_CTL_3.HPD_STATUS.  This is the
 * hardware-reported presence of a downstream DP sink (monitor or dock).
 * Checked before attempting AUX transactions because the AUX engine will
 * time out immediately if no sink is asserting HPD.
 */
static bool
rk_cdn_dp_hpd_status(struct rk_cdn_dp_softc *sc)
{
	return ((rk_cdn_dp_read_4(sc, RK_CDN_DP_SYS_CTL_3) &
	    RK_CDN_DP_SYS_CTL_3_HPD_STATUS) != 0);
}

/*
 * rk_cdn_dp_force_hpd
 *
 * Asserts HPD in software via SYS_CTL_3.F_HPD + HPD_CTRL.  Needed during
 * Type-C bring-up before fusb302 Alt Mode negotiation is implemented: the
 * CDN-DP AUX engine will not respond to transactions unless it believes a
 * sink is present, so this unlocks AUX access for DPCD probing without
 * requiring a physically negotiated Alt Mode cable.
 */
static void
rk_cdn_dp_force_hpd(struct rk_cdn_dp_softc *sc)
{
	rk_cdn_dp_write_4(sc, RK_CDN_DP_SYS_CTL_3,
	    RK_CDN_DP_SYS_CTL_3_HPD_CTRL | RK_CDN_DP_SYS_CTL_3_F_HPD);
	DELAY(1000);
	device_printf(sc->dev, "forcing HPD for Type-C DP debug\n");
}

/*
 * rk_cdn_dp_aux_status_name
 *
 * Maps the 4-bit AUX_CH_STA status field to a human-readable string.
 * The CDN-DP controller reports these codes after every AUX transaction;
 * having names in dmesg rather than raw numbers makes bring-up failures
 * much faster to diagnose on serial console without a debugger.
 */
static const char *
rk_cdn_dp_aux_status_name(uint32_t status)
{
	switch (status & RK_CDN_DP_AUX_STATUS_MASK) {
	case 0:
		return ("ok");
	case 1:
		return ("nack");
	case 2:
		return ("timeout");
	case 3:
		return ("unknown");
	case 4:
		return ("much-defer");
	case 5:
		return ("tx-short");
	case 6:
		return ("rx-short");
	case 7:
		return ("nack-without-m");
	case 8:
		return ("i2c-nack");
	default:
		return ("reserved");
	}
}

/*
 * rk_cdn_dp_aux_wait
 *
 * Polls until the CDN-DP AUX engine has finished a transaction.  The engine
 * signals completion by clearing AUX_EN in AUX_CH_CTL_2 and AUX_BUSY in
 * AUX_CH_STA.  Both must be clear to rule out a race where AUX_EN clears
 * before the internal state machine has fully settled.
 *
 * Timeout is 200ms (2000 iterations × 100µs) — sufficient for the worst-case
 * DEFER-retry loop defined by the DisplayPort spec while keeping attach fast
 * on a healthy link.
 */
static int
rk_cdn_dp_aux_wait(struct rk_cdn_dp_softc *sc)
{
	uint32_t ctl2, sta;
	int i;

	for (i = 0; i < 2000; i++) {
		ctl2 = rk_cdn_dp_read_4(sc, RK_CDN_DP_AUX_CH_CTL_2);
		sta = rk_cdn_dp_read_4(sc, RK_CDN_DP_AUX_CH_STA);
		if ((ctl2 & RK_CDN_DP_AUX_EN) == 0 &&
		    (sta & RK_CDN_DP_AUX_BUSY) == 0)
			return (0);
		DELAY(100);
	}

	return (ETIMEDOUT);
}

/*
 * rk_cdn_dp_aux_xfer
 *
 * Executes a single native AUX read or write against a DPCD register address.
 * This is the lowest-level AUX primitive; everything that needs to talk to the
 * downstream sink (DPCD capability reads, link training, EDID via I2C-over-AUX)
 * goes through here.
 *
 * Sequence per RK3399 TRM Part2, CDN-DP AUX section:
 *   1. Clear interrupt status and flush the payload buffer.
 *   2. For writes, pre-load the payload FIFO before arming the engine.
 *   3. Program the 20-bit DPCD address across three byte registers.
 *   4. Set length and command in CTL_1, then assert AUX_EN in CTL_2.
 *   5. Wait for completion, check status, harvest read data if applicable.
 */
static int
rk_cdn_dp_aux_prepare(struct rk_cdn_dp_softc *sc, uint8_t cmd, uint32_t addr,
    uint8_t *buf, int len)
{
	int i;

	if (len < 0 || len > RK_CDN_DP_AUX_MAX_XFER)
		return (EINVAL);

	rk_cdn_dp_write_4(sc, RK_CDN_DP_DP_INT_STA,
	    RK_CDN_DP_INT_RPLY_RECEIV | RK_CDN_DP_INT_AUX_ERR);
	rk_cdn_dp_write_4(sc, RK_CDN_DP_BUFFER_DATA_CTL, RK_CDN_DP_BUF_CLR);

	if (cmd == RK_CDN_DP_AUX_CMD_NATIVE_WRITE) {
		for (i = 0; i < len; i++)
			rk_cdn_dp_write_4(sc, RK_CDN_DP_BUF_DATA_0 + (i * 4),
			    buf[i]);
	}

	rk_cdn_dp_write_4(sc, RK_CDN_DP_AUX_ADDR_7_0, addr & 0xff);
	rk_cdn_dp_write_4(sc, RK_CDN_DP_AUX_ADDR_15_8, (addr >> 8) & 0xff);
	rk_cdn_dp_write_4(sc, RK_CDN_DP_AUX_ADDR_19_16, (addr >> 16) & 0x0f);
	rk_cdn_dp_write_4(sc, RK_CDN_DP_AUX_CH_CTL_1,
	    (((len > 0 ? len - 1 : 0) & 0xf) << 4) | (cmd & 0xf));
	sc->aux_pending_cmd = cmd;
	sc->aux_pending_addr = addr;
	sc->aux_pending_len = len;
	sc->aux_prepared = true;

	return (0);
}

static int
rk_cdn_dp_aux_i2c_xfer(device_t idev, int mode, uint8_t write_byte,
    uint8_t *read_byte)
{
	struct iic_dp_aux_data *aux;
	struct rk_cdn_dp_softc *sc;
	uint8_t cmd, data = 0;
	int error, len;

	aux = device_get_softc(idev);
	sc = aux != NULL ? aux->priv : NULL;
	if (sc == NULL)
		return (-ENXIO);

	sx_slock(&sc->detach_sx);
	if (sc->detached) {
		sx_sunlock(&sc->detach_sx);
		return (-ENXIO);
	}

	cmd = ((mode & MODE_I2C_READ) != 0) ? AUX_I2C_READ : AUX_I2C_WRITE;
	if ((mode & MODE_I2C_STOP) == 0)
		cmd |= AUX_I2C_MOT;

	if ((mode & MODE_I2C_START) != 0) {
		len = 0;
		error = rk_cdn_dp_aux_prepare(sc, cmd, aux->address, NULL, len);
	} else if ((mode & MODE_I2C_READ) != 0) {
		len = 1;
		error = rk_cdn_dp_aux_prepare(sc, cmd, aux->address, NULL, len);
	} else if ((mode & MODE_I2C_WRITE) != 0) {
		data = write_byte;
		len = 1;
		error = rk_cdn_dp_aux_prepare(sc, cmd, aux->address, &data, len);
	} else {
		len = 0;
		error = rk_cdn_dp_aux_prepare(sc, cmd, aux->address, NULL, len);
	}
	if (error == 0) {
		if ((mode & MODE_I2C_READ) != 0 && (mode & MODE_I2C_START) == 0)
			error = rk_cdn_dp_aux_finish(sc, &data, 1);
		else
			error = rk_cdn_dp_aux_finish(sc, NULL, 0);
	}
	if (error == 0 && (mode & MODE_I2C_READ) != 0 &&
	    (mode & MODE_I2C_START) == 0 && read_byte != NULL)
		*read_byte = data;

	sx_sunlock(&sc->detach_sx);
	return (-error);
}

static int
rk_cdn_dp_aux_finish(struct rk_cdn_dp_softc *sc, uint8_t *buf, int len)
{
	uint32_t buf_ctl, sta;
	int count, error, i;

	if (!sc->aux_prepared)
		return (EINVAL);

	rk_cdn_dp_write_4(sc, RK_CDN_DP_AUX_CH_CTL_2,
	    (sc->aux_pending_len == 0 ? RK_CDN_DP_ADDR_ONLY : 0) |
	    RK_CDN_DP_AUX_EN);

	error = rk_cdn_dp_aux_wait(sc);
	if (error != 0) {
		device_printf(sc->dev, "AUX wait timeout cmd=0x%x addr=0x%x\n",
		    sc->aux_pending_cmd, sc->aux_pending_addr);
		sc->aux_prepared = false;
		return (error);
	}

	sta = rk_cdn_dp_read_4(sc, RK_CDN_DP_AUX_CH_STA);
	if ((sta & RK_CDN_DP_AUX_STATUS_MASK) != 0) {
		device_printf(sc->dev,
		    "AUX error cmd=0x%x addr=0x%x status=%s(%u) errnum=%u\n",
		    sc->aux_pending_cmd, sc->aux_pending_addr,
		    rk_cdn_dp_aux_status_name(sta),
		    sta & RK_CDN_DP_AUX_STATUS_MASK,
		    rk_cdn_dp_read_4(sc, RK_CDN_DP_AUX_ERR_NUM) & 0xff);
		sc->aux_prepared = false;
		return (EIO);
	}

	if (sc->aux_pending_cmd == RK_CDN_DP_AUX_CMD_NATIVE_READ) {
		buf_ctl = rk_cdn_dp_read_4(sc, RK_CDN_DP_BUFFER_DATA_CTL);
		if ((buf_ctl & RK_CDN_DP_BUF_HAVE_DATA) == 0) {
			sc->aux_prepared = false;
			return (ENXIO);
		}
		count = buf_ctl & RK_CDN_DP_BUF_DATA_COUNT_MASK;
		if (count > len)
			count = len;
		for (i = 0; i < count; i++)
			buf[i] = rk_cdn_dp_read_4(sc,
			    RK_CDN_DP_BUF_DATA_0 + (i * 4)) & 0xff;
		if (count < len) {
			sc->aux_prepared = false;
			return (EMSGSIZE);
		}
	}

	sc->aux_prepared = false;

	return (0);
}

/*
 * rk_cdn_dp_probe_dpcd_caps
 *
 * Reads the first 15 bytes of the sink's DPCD register space (0x000–0x00E).
 * This range covers the DPCD revision, maximum link rate, maximum lane count,
 * and key capability flags — the minimum information needed to plan link
 * training.  Called from the attach-time debug probe to verify that the AUX
 * channel is alive end-to-end before link training is implemented.
 */
static int
rk_cdn_dp_probe_dpcd_caps(struct rk_cdn_dp_softc *sc)
{
	int error;

	error = rk_cdn_dp_aux_prepare(sc, RK_CDN_DP_AUX_CMD_NATIVE_READ, 0x000,
	    sc->aux_dpcd, sizeof(sc->aux_dpcd));
	if (error != 0)
		return (error);

	return (0);
}

static int
rk_cdn_dp_complete_dpcd_caps(struct rk_cdn_dp_softc *sc)
{
	int error;

	error = rk_cdn_dp_aux_finish(sc, sc->aux_dpcd, sizeof(sc->aux_dpcd));
	if (error != 0)
		return (error);

	device_printf(sc->dev,
	    "DPCD rev=%#x max_link_rate=%#x max_lane_count=%#x\n",
	    sc->aux_dpcd[0], sc->aux_dpcd[1], sc->aux_dpcd[2]);
	RK_CDN_DP_DPRINTF(sc,
	    "DPCD caps raw: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
	    sc->aux_dpcd[0], sc->aux_dpcd[1], sc->aux_dpcd[2],
	    sc->aux_dpcd[3], sc->aux_dpcd[4], sc->aux_dpcd[5],
	    sc->aux_dpcd[6], sc->aux_dpcd[7], sc->aux_dpcd[8],
	    sc->aux_dpcd[9], sc->aux_dpcd[10], sc->aux_dpcd[11],
	    sc->aux_dpcd[12], sc->aux_dpcd[13], sc->aux_dpcd[14]);

	return (0);
}

static uint32_t
rk_cdn_dp_link_rate_khz(uint8_t code)
{
	switch (code) {
	case 0x06:
		return (162000);
	case 0x0a:
		return (270000);
	case 0x14:
		return (540000);
	case 0x1e:
		return (810000);
	default:
		return (0);
	}
}

static void
rk_cdn_dp_record_sink_caps(struct rk_cdn_dp_softc *sc)
{
	sc->sink_caps_valid = true;
	sc->sink_dpcd_rev = sc->aux_dpcd[0];
	sc->sink_max_link_rate_code = sc->aux_dpcd[1];
	sc->sink_max_lane_count = sc->aux_dpcd[2] & 0x1f;
	sc->sink_max_link_rate_khz =
	    rk_cdn_dp_link_rate_khz(sc->sink_max_link_rate_code);

	device_printf(sc->dev,
	    "DPCD rev=%#x max_link_rate=%#x (%u kHz) max_lane_count=%#x enhanced_frame=%d downspread=%d\n",
	    sc->sink_dpcd_rev,
	    sc->sink_max_link_rate_code,
	    sc->sink_max_link_rate_khz,
	    sc->aux_dpcd[2],
	    (sc->aux_dpcd[2] & 0x80) != 0 ? 1 : 0,
	    (sc->aux_dpcd[3] & 0x01) != 0 ? 1 : 0);
	RK_CDN_DP_DPRINTF(sc,
	    "DPCD caps raw: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
	    sc->aux_dpcd[0], sc->aux_dpcd[1], sc->aux_dpcd[2],
	    sc->aux_dpcd[3], sc->aux_dpcd[4], sc->aux_dpcd[5],
	    sc->aux_dpcd[6], sc->aux_dpcd[7], sc->aux_dpcd[8],
	    sc->aux_dpcd[9], sc->aux_dpcd[10], sc->aux_dpcd[11],
	    sc->aux_dpcd[12], sc->aux_dpcd[13], sc->aux_dpcd[14]);
}

static int
rk_cdn_dp_plan_link(struct rk_cdn_dp_softc *sc)
{
	uint8_t host_lanes;
	bool flip;
	int usb_ss;
	uint8_t sink_lanes;

	if (!sc->sink_caps_valid)
		return (ENOENT);

	rk_cdn_dp_get_hostcap_config(sc, &host_lanes, &flip, &usb_ss);
	sink_lanes = sc->sink_max_lane_count;
	if (sink_lanes == 0)
		return (EINVAL);

	sc->link_plan_lanes = MIN(host_lanes, sink_lanes);
	sc->link_plan_rate_code = sc->sink_max_link_rate_code;

	RK_CDN_DP_DPRINTF(sc,
	    "link-plan: sink_rate=%#x (%u kHz) sink_lanes=%u host_lanes=%u usb_ss=%d flip=%u -> train_rate=%#x train_lanes=%u\n",
	    sc->sink_max_link_rate_code,
	    sc->sink_max_link_rate_khz,
	    sink_lanes, host_lanes, usb_ss, flip ? 1 : 0,
	    sc->link_plan_rate_code, sc->link_plan_lanes);

	return (0);
}

/*
 * DP link-training helpers (CR + EQ phases per DP spec).
 */

/* rk_typec_phy_dp_* helpers are declared in rk_typec_phy_var.h. */

static uint8_t
rk_cdn_dp_get_lane_status(const uint8_t link_status[DP_LINK_STATUS_SIZE],
    int lane)
{
	uint8_t shift = (lane & 1) ? 4 : 0;
	uint8_t v = link_status[lane >> 1];

	return ((v >> shift) & 0xf);
}

static bool
rk_cdn_dp_clock_recovery_ok(const uint8_t link_status[DP_LINK_STATUS_SIZE],
    int num_lanes)
{
	int i;
	uint8_t s;

	for (i = 0; i < num_lanes; i++) {
		s = rk_cdn_dp_get_lane_status(link_status, i);
		if ((s & DP_LANE_CR_DONE) == 0)
			return (false);
	}
	return (true);
}

static bool
rk_cdn_dp_channel_eq_ok(const uint8_t link_status[DP_LINK_STATUS_SIZE],
    int num_lanes)
{
	int i;
	uint8_t s;
	uint8_t need = DP_LANE_CR_DONE | DP_LANE_CHANNEL_EQ_DONE |
	    DP_LANE_SYMBOL_LOCKED;

	if ((link_status[2] & DP_INTERLANE_ALIGN_DONE) == 0)
		return (false);
	for (i = 0; i < num_lanes; i++) {
		s = rk_cdn_dp_get_lane_status(link_status, i);
		if ((s & need) != need)
			return (false);
	}
	return (true);
}

static uint8_t
rk_cdn_dp_get_adjust_request_voltage(
    const uint8_t link_status[DP_LINK_STATUS_SIZE], int lane)
{
	int idx = (lane >> 1) + (DP_ADJUST_REQUEST_LANE0_1 - DP_LANE0_1_STATUS);
	uint8_t shift = (lane & 1) ? 4 : 0;

	return ((link_status[idx] >> shift) & 0x3);
}

static uint8_t
rk_cdn_dp_get_adjust_request_pre_emphasis(
    const uint8_t link_status[DP_LINK_STATUS_SIZE], int lane)
{
	int idx = (lane >> 1) + (DP_ADJUST_REQUEST_LANE0_1 - DP_LANE0_1_STATUS);
	uint8_t shift = (lane & 1) ? 6 : 2;

	return ((link_status[idx] >> shift) & 0x3);
}

static uint8_t
rk_cdn_dp_pre_emphasis_max(uint8_t voltage_swing)
{
	switch (voltage_swing & DP_TRAIN_VOLTAGE_SWING_MASK) {
	case DP_TRAIN_VOLTAGE_SWING_LEVEL_0:
		return (DP_TRAIN_PRE_EMPH_LEVEL_3);
	case DP_TRAIN_VOLTAGE_SWING_LEVEL_1:
		return (DP_TRAIN_PRE_EMPH_LEVEL_2);
	case DP_TRAIN_VOLTAGE_SWING_LEVEL_2:
		return (DP_TRAIN_PRE_EMPH_LEVEL_1);
	default:
		return (DP_TRAIN_PRE_EMPH_LEVEL_0);
	}
}

static void
rk_cdn_dp_get_adjust_train(struct rk_cdn_dp_softc *sc)
{
	int i, n;
	uint8_t v = 0, p = 0, preemph_max;

	n = sc->link_plan_lanes;
	for (i = 0; i < n; i++) {
		uint8_t lv =
		    rk_cdn_dp_get_adjust_request_voltage(sc->link_status, i);
		uint8_t lp = rk_cdn_dp_get_adjust_request_pre_emphasis(
		    sc->link_status, i);
		if (lv > v)
			v = lv;
		if (lp > p)
			p = lp;
	}

	if (v >= DP_TRAIN_VOLTAGE_SWING_LEVEL_2)
		v = DP_TRAIN_VOLTAGE_SWING_LEVEL_2 |
		    DP_TRAIN_MAX_SWING_REACHED;

	preemph_max = rk_cdn_dp_pre_emphasis_max(v);
	p = (p << DP_TRAIN_PRE_EMPHASIS_SHIFT);
	if (p >= preemph_max)
		p = preemph_max | DP_TRAIN_MAX_PRE_EMPHASIS_REACHED;

	for (i = 0; i < n; i++)
		sc->train_set[i] = v | p;
}

static bool
rk_cdn_dp_link_max_vswing_reached(struct rk_cdn_dp_softc *sc)
{
	int i;

	for (i = 0; i < sc->link_plan_lanes; i++)
		if ((sc->train_set[i] & DP_TRAIN_MAX_SWING_REACHED) == 0)
			return (false);
	return (true);
}

static int
rk_cdn_dp_apply_phy_signal_levels(struct rk_cdn_dp_softc *sc)
{
	uint32_t rate_khz;
	uint8_t swing, pre_emp;
	int rc;

	rate_khz = rk_cdn_dp_link_rate_khz(sc->link_plan_rate_code);
	swing = (sc->train_set[0] & DP_TRAIN_VOLTAGE_SWING_MASK) >>
	    DP_TRAIN_VOLTAGE_SWING_SHIFT;
	pre_emp = (sc->train_set[0] & DP_TRAIN_PRE_EMPHASIS_MASK) >>
	    DP_TRAIN_PRE_EMPHASIS_SHIFT;

	rc = rk_typec_phy_dp_set_signal_levels_first((int)rate_khz,
	    sc->link_plan_lanes, swing, pre_emp);
	if (rc != 0) {
		device_printf(sc->dev,
		    "phy signal-levels failed: rate=%u lanes=%u swing=%u pe=%u rc=%d\n",
		    rate_khz, sc->link_plan_lanes, swing, pre_emp, rc);
	}
	return (rc);
}

static int
rk_cdn_dp_set_pattern(struct rk_cdn_dp_softc *sc, uint8_t dp_train_pat)
{
	uint32_t global_config, phy_config;
	uint8_t pattern = dp_train_pat & DP_TRAINING_PATTERN_MASK;
	int error;

	global_config = RK_CDN_DP_FRAMER_NUM_LANES(sc->link_plan_lanes - 1) |
	    RK_CDN_DP_FRAMER_SST_MODE | RK_CDN_DP_FRAMER_GLOBAL_EN |
	    RK_CDN_DP_FRAMER_RG_EN | RK_CDN_DP_FRAMER_ENC_RST_DIS |
	    RK_CDN_DP_FRAMER_WR_VHSYNC_FALL;

	phy_config = RK_CDN_DP_TX_PHY_ENCODER_BYPASS(0) |
	    RK_CDN_DP_TX_PHY_SKEW_BYPASS(0) |
	    RK_CDN_DP_TX_PHY_DISPARITY_RST(0) |
	    RK_CDN_DP_TX_PHY_LANE0_SKEW(0) |
	    RK_CDN_DP_TX_PHY_LANE1_SKEW(1) |
	    RK_CDN_DP_TX_PHY_LANE2_SKEW(2) |
	    RK_CDN_DP_TX_PHY_LANE3_SKEW(3) |
	    RK_CDN_DP_TX_PHY_10BIT_ENABLE(0);

	if (pattern != DP_TRAINING_PATTERN_DISABLE) {
		global_config |= RK_CDN_DP_FRAMER_NO_VIDEO;
		phy_config |= RK_CDN_DP_TX_PHY_TRAINING_ENABLE(1) |
		    RK_CDN_DP_TX_PHY_SCRAMBLER_BYPASS(1) |
		    RK_CDN_DP_TX_PHY_TRAINING_PATTERN(pattern);
	}
	rk_cdn_dp_log_phy_config(sc,
	    (pattern == DP_TRAINING_PATTERN_DISABLE) ?
	    "set_pattern(live-video)" : "set_pattern(training)",
	    phy_config);

	error = rk_cdn_dp_mailbox_reg_write(sc,
	    RK_CDN_DP_DP_FRAMER_GLOBAL_CONFIG, global_config);
	if (error != 0)
		return (error);
	error = rk_cdn_dp_mailbox_reg_write(sc,
	    RK_CDN_DP_DP_TX_PHY_CONFIG_REG, phy_config);
	if (error != 0)
		return (error);
	error = rk_cdn_dp_mailbox_reg_write(sc, RK_CDN_DP_DPTX_LANE_EN,
	    (1U << sc->link_plan_lanes) - 1U);
	if (error != 0)
		return (error);
	error = rk_cdn_dp_mailbox_reg_write(sc, RK_CDN_DP_DPTX_ENHNCD,
	    ((sc->aux_dpcd[DP_MAX_LANE_COUNT] &
	    DP_LANE_COUNT_ENHANCED_FRAME_EN) != 0) ? 1 : 0);
	return (error);
}

static int
rk_cdn_dp_set_link_train(struct rk_cdn_dp_softc *sc, uint8_t dp_train_pat)
{
	uint8_t buf[5];
	int len;

	buf[0] = dp_train_pat;
	if ((dp_train_pat & DP_TRAINING_PATTERN_MASK) ==
	    DP_TRAINING_PATTERN_DISABLE) {
		len = 1;
	} else {
		memcpy(buf + 1, sc->train_set, sc->link_plan_lanes);
		len = sc->link_plan_lanes + 1;
	}

	return (rk_cdn_dp_mailbox_dpcd_write(sc, DP_TRAINING_PATTERN_SET,
	    buf, len));
}

static int
rk_cdn_dp_update_link_train(struct rk_cdn_dp_softc *sc)
{
	int error;

	error = rk_cdn_dp_apply_phy_signal_levels(sc);
	if (error != 0)
		return (error);
	return (rk_cdn_dp_mailbox_dpcd_write(sc, DP_TRAINING_LANE0_SET,
	    sc->train_set, sc->link_plan_lanes));
}

static int
rk_cdn_dp_reset_link_train(struct rk_cdn_dp_softc *sc, uint8_t pat)
{
	int error;

	memset(sc->train_set, 0, sizeof(sc->train_set));
	error = rk_cdn_dp_apply_phy_signal_levels(sc);
	if (error != 0)
		return (error);
	error = rk_cdn_dp_set_pattern(sc, pat);
	if (error != 0)
		return (error);
	return (rk_cdn_dp_set_link_train(sc, pat));
}

static int
rk_cdn_dp_read_link_status(struct rk_cdn_dp_softc *sc)
{
	return (rk_cdn_dp_mailbox_dpcd_read(sc, DP_LANE0_1_STATUS,
	    sc->link_status, DP_LINK_STATUS_SIZE));
}

static int
rk_cdn_dp_link_train_clock_recovery(struct rk_cdn_dp_softc *sc)
{
	int error;
	uint32_t voltage_tries = 1, max_vswing_tries = 0;
	uint8_t prev_voltage;

	error = rk_cdn_dp_reset_link_train(sc,
	    DP_TRAINING_PATTERN_1 | DP_LINK_SCRAMBLING_DISABLE);
	if (error != 0)
		return (error);

	for (;;) {
		DELAY(100);	/* drm_dp_link_train_clock_recovery_delay */
		error = rk_cdn_dp_read_link_status(sc);
		if (error != 0)
			return (error);

		RK_CDN_DP_DPRINTF(sc,
		    "CR iter v_tries=%u max_vs=%u status=%02x %02x %02x %02x %02x %02x train[0]=%02x\n",
		    voltage_tries, max_vswing_tries,
		    sc->link_status[0], sc->link_status[1], sc->link_status[2],
		    sc->link_status[3], sc->link_status[4], sc->link_status[5],
		    sc->train_set[0]);

		if (rk_cdn_dp_clock_recovery_ok(sc->link_status,
		    sc->link_plan_lanes)) {
			RK_CDN_DP_DPRINTF(sc,
			    "CR done after %u vswing tries (max_vs=%u)\n",
			    voltage_tries, max_vswing_tries);
			return (0);
		}

		if (voltage_tries >= 5) {
			device_printf(sc->dev,
			    "CR failed: same voltage tried 5 times\n");
			return (EIO);
		}
		if (max_vswing_tries >= 1) {
			device_printf(sc->dev,
			    "CR failed: max voltage swing reached\n");
			return (EIO);
		}

		prev_voltage = sc->train_set[0] & DP_TRAIN_VOLTAGE_SWING_MASK;
		rk_cdn_dp_get_adjust_train(sc);
		error = rk_cdn_dp_update_link_train(sc);
		if (error != 0)
			return (error);

		if ((sc->train_set[0] & DP_TRAIN_VOLTAGE_SWING_MASK) ==
		    prev_voltage)
			voltage_tries++;
		else
			voltage_tries = 1;

		if (rk_cdn_dp_link_max_vswing_reached(sc))
			max_vswing_tries++;
	}
}

static uint8_t
rk_cdn_dp_select_chaneq_pattern(struct rk_cdn_dp_softc *sc)
{
	/*
	 * Mirror the reference driver's Cadence training helper: use TPS3 whenever the sink
	 * advertises support, not only at HBR2.  Some sinks complete EQ at HBR
	 * only when we present TPS3 during channel equalization.
	 */
	if ((sc->aux_dpcd[DP_MAX_LANE_COUNT] & 0x40) != 0)
		return (DP_TRAINING_PATTERN_3);
	return (DP_TRAINING_PATTERN_2);
}

static int
rk_cdn_dp_link_train_channel_eq(struct rk_cdn_dp_softc *sc)
{
	uint8_t pat;
	int tries, error;

	pat = rk_cdn_dp_select_chaneq_pattern(sc) | DP_LINK_SCRAMBLING_DISABLE;
	error = rk_cdn_dp_set_pattern(sc, pat);
	if (error != 0)
		return (error);
	error = rk_cdn_dp_set_link_train(sc, pat);
	if (error != 0)
		return (error);

	for (tries = 0; tries < 5; tries++) {
		DELAY(400);	/* drm_dp_link_train_channel_eq_delay */
		error = rk_cdn_dp_read_link_status(sc);
		if (error != 0)
			return (error);

		RK_CDN_DP_DPRINTF(sc,
		    "EQ iter try=%d status=%02x %02x %02x %02x %02x %02x train[0]=%02x\n",
		    tries, sc->link_status[0], sc->link_status[1],
		    sc->link_status[2], sc->link_status[3], sc->link_status[4],
		    sc->link_status[5], sc->train_set[0]);

		if (!rk_cdn_dp_clock_recovery_ok(sc->link_status,
		    sc->link_plan_lanes)) {
			device_printf(sc->dev,
			    "EQ aborted: CR slipped (try %d)\n", tries);
			return (EIO);
		}
		if (rk_cdn_dp_channel_eq_ok(sc->link_status,
		    sc->link_plan_lanes)) {
			RK_CDN_DP_DPRINTF(sc,
			    "EQ done after %d tries\n", tries);
			return (0);
		}

		rk_cdn_dp_get_adjust_train(sc);
		error = rk_cdn_dp_update_link_train(sc);
		if (error != 0)
			return (error);
	}
	device_printf(sc->dev, "EQ failed after 5 tries\n");
	return (EIO);
}

static int
rk_cdn_dp_stop_link_train(struct rk_cdn_dp_softc *sc)
{
	int error;

	error = rk_cdn_dp_set_pattern(sc, DP_TRAINING_PATTERN_DISABLE);
	if (error != 0)
		return (error);
	return (rk_cdn_dp_set_link_train(sc, DP_TRAINING_PATTERN_DISABLE));
}

/*
 * Drop to a lower DP link rate.  Returns 0 on success, ENOENT if already at
 * minimum (RBR).
 */
static int
rk_cdn_dp_lower_rate(struct rk_cdn_dp_softc *sc)
{
	switch (sc->link_plan_rate_code) {
	case 0x06:	/* RBR — already the lowest */
		return (ENOENT);
	case 0x0a:	/* HBR  -> RBR */
		sc->link_plan_rate_code = 0x06;
		break;
	case 0x14:	/* HBR2 -> HBR */
		sc->link_plan_rate_code = 0x0a;
		break;
	default:	/* unknown -> HBR2 (highest supported) */
		sc->link_plan_rate_code = 0x14;
		break;
	}
	device_printf(sc->dev, "downgrade link rate to 0x%x (%u kHz)\n",
	    sc->link_plan_rate_code,
	    rk_cdn_dp_link_rate_khz(sc->link_plan_rate_code));
	return (0);
}

/*
 * Firmware-driven link training fallback (reference vendor BSP cdn-dp-reg.c:564-639).
 *
 * The Cadence dptx.bin firmware has its own internal CR+EQ state machine
 * that uses fixed PHY config values it ships with.  the reference driver uses this as a
 * fallback when host-side DPCD-driven (software) training fails — the
 * firmware's analog tuning may succeed where ours doesn't, especially on
 * "boards with unique hardware design" (per the reference driver's comment).
 *
 * Protocol: send DPTX_TRAINING_CONTROL=LINK_TRAINING_RUN; poll READ_EVENT
 * until event[1] & EQ_PHASE_FINISHED; then DPTX_READ_LINK_STAT to learn
 * the firmware's negotiated rate/lanes.
 */
static int
rk_cdn_dp_fw_train_link(struct rk_cdn_dp_softc *sc)
{
	uint8_t msg, event[2], status[10];
	int error, elapsed_ms;

	msg = RK_CDN_DP_LINK_TRAINING_RUN;
	error = rk_cdn_dp_mailbox_send(sc, RK_CDN_DP_MB_MODULE_ID_DP_TX,
	    RK_CDN_DP_DPTX_TRAINING_CONTROL, sizeof(msg), &msg);
	if (error != 0) {
		RK_CDN_DP_DPRINTF(sc,
		    "fw-train: TRAINING_CONTROL send failed (%d)\n", error);
		return (error);
	}

	for (elapsed_ms = 0;
	    elapsed_ms < RK_CDN_DP_LINK_TRAINING_TIMEOUT_MS;
	    elapsed_ms += RK_CDN_DP_LINK_TRAINING_RETRY_MS) {
		DELAY(RK_CDN_DP_LINK_TRAINING_RETRY_MS * 1000);

		error = rk_cdn_dp_mailbox_send(sc,
		    RK_CDN_DP_MB_MODULE_ID_DP_TX,
		    RK_CDN_DP_DPTX_READ_EVENT, 0, NULL);
		if (error != 0) {
			device_printf(sc->dev,
			    "fw-train: READ_EVENT send failed (%d)\n", error);
			return (error);
		}
		error = rk_cdn_dp_mailbox_validate_receive(sc,
		    RK_CDN_DP_MB_MODULE_ID_DP_TX,
		    RK_CDN_DP_DPTX_READ_EVENT, sizeof(event));
		if (error != 0) {
			device_printf(sc->dev,
			    "fw-train: READ_EVENT recv invalid (%d)\n", error);
			return (error);
		}
		error = rk_cdn_dp_mailbox_read_receive(sc, event, sizeof(event));
		if (error != 0)
			return (error);

		if (event[1] & RK_CDN_DP_EQ_PHASE_FINISHED) {
			RK_CDN_DP_DPRINTF(sc,
			    "fw-train: EQ_PHASE_FINISHED after %d ms "
			    "(event=%02x %02x)\n",
			    elapsed_ms, event[0], event[1]);
			break;
		}
	}
	if (elapsed_ms >= RK_CDN_DP_LINK_TRAINING_TIMEOUT_MS) {
		device_printf(sc->dev,
		    "fw-train: timeout (%d ms) waiting for EQ_PHASE_FINISHED\n",
		    elapsed_ms);
		return (ETIMEDOUT);
	}

	/*
	 * Firmware finished training; read back its negotiated rate/lanes.
	 * status[0] = rate_code, status[1] = lane_count, [2..9] reserved.
	 */
	error = rk_cdn_dp_mailbox_send(sc, RK_CDN_DP_MB_MODULE_ID_DP_TX,
	    RK_CDN_DP_DPTX_READ_LINK_STAT, 0, NULL);
	if (error != 0) {
		device_printf(sc->dev,
		    "fw-train: READ_LINK_STAT send failed (%d)\n", error);
		return (error);
	}
	error = rk_cdn_dp_mailbox_validate_receive(sc,
	    RK_CDN_DP_MB_MODULE_ID_DP_TX,
	    RK_CDN_DP_DPTX_READ_LINK_STAT, sizeof(status));
	if (error != 0) {
		device_printf(sc->dev,
		    "fw-train: READ_LINK_STAT recv invalid (%d)\n", error);
		return (error);
	}
	error = rk_cdn_dp_mailbox_read_receive(sc, status, sizeof(status));
	if (error != 0)
		return (error);

	RK_CDN_DP_DPRINTF(sc,
	    "fw-train: firmware negotiated rate_code=0x%02x lanes=%u\n",
	    status[0], status[1]);
	sc->link_plan_rate_code = status[0];
	sc->link_plan_lanes = status[1] & 0x1f;	/* lane count, low 5 bits */
	return (0);
}

static int
rk_cdn_dp_link_train(struct rk_cdn_dp_softc *sc)
{
	uint8_t buf[2];
	bool ssc_on;
	int error;

	if (!sc->sink_caps_valid || sc->link_plan_lanes == 0 ||
	    sc->link_plan_rate_code == 0)
		return (ENOENT);

	/* Sink-side prep: downspread + 8b10b. (Run once.) */
	ssc_on = (sc->aux_dpcd[DP_MAX_DOWNSPREAD] &
	    DP_MAX_DOWNSPREAD_0_5) != 0;
	buf[0] = ssc_on ? DP_SPREAD_AMP_0_5 : 0;
	buf[1] = (sc->aux_dpcd[DP_MAIN_LINK_CHANNEL_CODING] & 0x01) ?
	    DP_SET_ANSI_8B10B : 0;
	error = rk_cdn_dp_mailbox_dpcd_write(sc, DP_DOWNSPREAD_CTRL, buf, 2);
	if (error != 0)
		return (error);

	for (;;) {
		/*
		 * Match the reference driver ordering on each attempt: re-rate the PHY first,
		 * then tell the sink the active link configuration.  That keeps
		 * both ends in sync when we retry at a lower bandwidth.
		 */
		error = rk_typec_phy_dp_set_link_rate_first(
		    (int)rk_cdn_dp_link_rate_khz(sc->link_plan_rate_code),
		    ssc_on);
		if (error != 0) {
			device_printf(sc->dev,
			    "phy set_link_rate failed (%d)\n", error);
			return (error);
		}
		error = rk_typec_phy_dp_set_lane_count_first(
		    sc->link_plan_lanes);
		if (error != 0) {
			device_printf(sc->dev,
			    "phy set_lane_count failed (%d)\n", error);
			return (error);
		}

		/* Tell sink the trained rate + lane count + enhanced framing. */
		error = rk_cdn_dp_write_link_config(sc, "link_train");
		if (error != 0)
			return (error);

		error = rk_cdn_dp_link_train_clock_recovery(sc);
		if (error != 0) {
			if (rk_cdn_dp_lower_rate(sc) == 0)
				continue;	/* retry at lower rate */
			break;
		}
		error = rk_cdn_dp_link_train_channel_eq(sc);
		if (error != 0) {
			if (rk_cdn_dp_lower_rate(sc) == 0)
				continue;	/* retry at lower rate */
			break;
		}
		break;	/* both phases succeeded */
	}

	{
		int stop_error;

		stop_error = rk_cdn_dp_stop_link_train(sc);
		if (stop_error != 0)
			return (stop_error);
	}

	/*
	 * Software training failed — fall back to firmware-driven training
	 * (reference vendor BSP cdn_dp_train_link does the same).  The dptx.bin firmware
	 * has its own internal CR+EQ FSM with fixed PHY tuning that may
	 * succeed where our DPCD-driven max-swing/pre-emp doesn't.  Only log
	 * if firmware training also fails.
	 */
	if (error != 0) {
		int fw_error;

		RK_CDN_DP_DPRINTF(sc,
		    "software link training failed (%d); trying firmware-driven\n",
		    error);
		fw_error = rk_cdn_dp_fw_train_link(sc);
		if (fw_error != 0) {
			device_printf(sc->dev,
			    "firmware link training also failed (%d)\n", fw_error);
			return (error);	/* return original sw error */
		}
		sc->link_trained = true;
		sc->fw_link_training_used = true;
		device_printf(sc->dev,
		    "link trained (firmware): rate_code=0x%x (%u kHz) lanes=%u\n",
		    sc->link_plan_rate_code,
		    rk_cdn_dp_link_rate_khz(sc->link_plan_rate_code),
		    sc->link_plan_lanes);
		return (0);
	}

	sc->link_trained = true;
	sc->fw_link_training_used = false;
	device_printf(sc->dev,
	    "link trained: rate_code=0x%x (%u kHz) lanes=%u train_set[0]=0x%02x\n",
	    sc->link_plan_rate_code,
	    rk_cdn_dp_link_rate_khz(sc->link_plan_rate_code),
	    sc->link_plan_lanes, sc->train_set[0]);
	return (0);
}

/*
 * Read one 128-byte EDID block via the Cadence firmware mailbox.
 * Mailbox sequence: send (block/2, block%2), validate receive
 * (sizeof(reg) + length), read back reg + data.
 *  block == 0  => base EDID (header at offset 0..7 should be 00 ff ff ff ff ff ff 00)
 *  block == 1  => first extension (CTA-861) if present
 */
static int
rk_cdn_dp_mailbox_get_edid_block(struct rk_cdn_dp_softc *sc, uint8_t block,
    uint8_t *out, size_t length)
{
	uint8_t msg[2], reg[2];
	int error, attempt;

	if (out == NULL || length == 0 || length > 256)
		return (EINVAL);

	for (attempt = 0; attempt < 4; attempt++) {
		msg[0] = block / 2;
		msg[1] = block % 2;
		error = rk_cdn_dp_mailbox_send(sc, RK_CDN_DP_MB_MODULE_ID_DP_TX,
		    RK_CDN_DP_DPTX_GET_EDID, sizeof(msg), msg);
		if (error != 0)
			continue;
		error = rk_cdn_dp_mailbox_validate_receive(sc,
		    RK_CDN_DP_MB_MODULE_ID_DP_TX, RK_CDN_DP_DPTX_GET_EDID,
		    sizeof(reg) + length);
		if (error != 0)
			continue;
		error = rk_cdn_dp_mailbox_read_receive(sc, reg, sizeof(reg));
		if (error != 0)
			continue;
		error = rk_cdn_dp_mailbox_read_receive(sc, out, length);
		if (error != 0)
			continue;
		if (reg[0] == length && reg[1] == (block / 2))
			return (0);
		device_printf(sc->dev,
		    "EDID block %u attempt %d: got reg=[%02x %02x] expected=[%02zx %02x]\n",
		    block, attempt, reg[0], reg[1], length, block / 2);
	}
	return (EIO);
}

static int
rk_cdn_dp_read_edid(struct rk_cdn_dp_softc *sc)
{
	int error;
	const uint8_t header[8] = {
	    0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00 };
	int i;
	const uint8_t *e;
	uint8_t ext_blocks, cached_blocks, block;

	/*
	 * EDID is read via firmware-side AUX over SBU, independent of the
	 * main-link DP data lane training state.  Don't gate on
	 * link_trained — AUX is alive as soon as HPD is high, which we
	 * already confirmed at stage 13.
	 */
	if (!sc->sink_caps_valid)
		return (ENOENT);

	error = rk_cdn_dp_mailbox_get_edid_block(sc, 0, sc->edid, 128);
	if (error != 0) {
		device_printf(sc->dev, "EDID read failed: %d\n", error);
		return (error);
	}

	e = sc->edid;
	for (i = 0; i < 8; i++) {
		if (e[i] != header[i]) {
			device_printf(sc->dev,
			    "EDID header mismatch at byte %d: got 0x%02x\n",
			    i, e[i]);
			return (EIO);
		}
	}

	ext_blocks = sc->edid[126];
	cached_blocks = ext_blocks;
	if (cached_blocks > (RK_CDN_DP_MAX_EDID_BLOCKS - 1)) {
		device_printf(sc->dev,
		    "EDID advertises %u extension block(s), caching first %u only\n",
		    ext_blocks, RK_CDN_DP_MAX_EDID_BLOCKS - 1);
		cached_blocks = RK_CDN_DP_MAX_EDID_BLOCKS - 1;
	}
	for (block = 1; block <= cached_blocks; block++) {
		error = rk_cdn_dp_mailbox_get_edid_block(sc, block,
		    &sc->edid[block * 128], 128);
		if (error != 0) {
			device_printf(sc->dev,
			    "EDID extension block %u read failed: %d\n",
			    block, error);
			return (error);
		}
	}

	sc->edid_len = 128 * (1 + cached_blocks);
	sc->edid_valid = true;
	RK_CDN_DP_DPRINTF(sc,
	    "EDID OK: vendor=%c%c%c product=0x%02x%02x%02x%02x version=%u.%u ext=%u cached_len=%u\n",
	    ((e[8] >> 2) & 0x1f) + 'A' - 1,
	    (((e[8] & 0x3) << 3) | (e[9] >> 5)) + 'A' - 1,
	    (e[9] & 0x1f) + 'A' - 1,
	    e[10], e[11], e[12], e[13], e[18], e[19], ext_blocks,
	    sc->edid_len);
	RK_CDN_DP_DPRINTF(sc,
	    "EDID first 32B: %02x %02x %02x %02x %02x %02x %02x %02x "
	    "%02x %02x %02x %02x %02x %02x %02x %02x "
	    "%02x %02x %02x %02x %02x %02x %02x %02x "
	    "%02x %02x %02x %02x %02x %02x %02x %02x\n",
	    e[0], e[1], e[2], e[3], e[4], e[5], e[6], e[7],
	    e[8], e[9], e[10], e[11], e[12], e[13], e[14], e[15],
	    e[16], e[17], e[18], e[19], e[20], e[21], e[22], e[23],
	    e[24], e[25], e[26], e[27], e[28], e[29], e[30], e[31]);
	return (0);
}

int
rk_cdn_dp_get_cached_edid(device_t dev, uint8_t *buf, size_t len)
{
	struct rk_cdn_dp_softc *sc;
	size_t copy_len;

	if (dev == NULL || buf == NULL || len < 128)
		return (EINVAL);
	sc = device_get_softc(dev);
	if (sc == NULL)
		return (EINVAL);
	if (!sc->edid_valid)
		return (ENXIO);
	copy_len = sc->edid_len >= 128 ? sc->edid_len : 128;
	if (len < copy_len)
		return (ENOSPC);
	memcpy(buf, sc->edid, copy_len);
	return (0);
}

/*
 * Parse the first 18-byte detailed timing descriptor from EDID block 0
 * (offset 0x36).  Stores the timings in sc-> fields.  Returns 0 on success
 * or EINVAL if the descriptor isn't a valid detailed-timing entry.
 *
 * Detailed-timing layout (DP/EDID 1.4 §3.10.2):
 *   00..01  pixel clock / 10kHz (LE)
 *   02      hactive lo
 *   03      hblank lo
 *   04      hactive_hi[7:4] | hblank_hi[3:0]
 *   05      vactive lo
 *   06      vblank lo
 *   07      vactive_hi[7:4] | vblank_hi[3:0]
 *   08      hsync_off lo
 *   09      hsync_pulse lo
 *   10      vsync_off_hi[7:4] | vsync_pulse_hi[3:0]
 *           ...wait, actually byte 10 = vsync_off_lo[7:4] | vsync_pulse_lo[3:0]
 *   11      hsync_off_hi[7:6] | hsync_pulse_hi[5:4] |
 *           vsync_off_hi[3:2]  | vsync_pulse_hi[1:0]
 *   12..16  image-size etc., not used here
 *   17      features bitmap, bits [2:1] set vs polarity, bit 1 hs polarity
 */
static int
rk_cdn_dp_parse_edid_dtd(struct rk_cdn_dp_softc *sc)
{
	const uint8_t *d;
	uint32_t pixclk;
	uint16_t hact, hblnk, vact, vblnk;
	uint16_t hso, hsp_w, vso, vsp_w;

	if (!sc->edid_valid)
		return (ENOENT);
	d = &sc->edid[0x36];

	pixclk = ((uint32_t)d[1] << 8) | d[0];
	if (pixclk == 0)
		return (EINVAL);
	pixclk *= 10;	/* 10 kHz units -> kHz */

	hact = ((uint16_t)(d[4] & 0xf0) << 4) | d[2];
	hblnk = ((uint16_t)(d[4] & 0x0f) << 8) | d[3];
	vact = ((uint16_t)(d[7] & 0xf0) << 4) | d[5];
	vblnk = ((uint16_t)(d[7] & 0x0f) << 8) | d[6];

	hso = ((uint16_t)(d[11] & 0xc0) << 2) | d[8];
	hsp_w = ((uint16_t)(d[11] & 0x30) << 4) | d[9];
	vso = ((uint16_t)(d[11] & 0x0c) << 2) | (d[10] >> 4);
	vsp_w = ((uint16_t)(d[11] & 0x03) << 4) | (d[10] & 0x0f);

	sc->pixel_clock_khz = pixclk;
	sc->hdisplay = hact;
	sc->hblank = hblnk;
	sc->htotal = hact + hblnk;
	sc->hsync_start = hact + hso;
	sc->hsync_end = hact + hso + hsp_w;
	sc->vdisplay = vact;
	sc->vblank = vblnk;
	sc->vtotal = vact + vblnk;
	sc->vsync_start = vact + vso;
	sc->vsync_end = vact + vso + vsp_w;
	sc->h_sync_polarity = (d[17] & (1U << 1)) ? 1 : 0;
	sc->v_sync_polarity = (d[17] & (1U << 2)) ? 1 : 0;

	device_printf(sc->dev,
	    "EDID DTD: %ux%u @ pixclk=%u kHz htotal=%u hsync=%u..%u (pol=%u) vtotal=%u vsync=%u..%u (pol=%u)\n",
	    hact, vact, pixclk, sc->htotal, sc->hsync_start, sc->hsync_end,
	    sc->h_sync_polarity, sc->vtotal, sc->vsync_start, sc->vsync_end,
	    sc->v_sync_polarity);
	return (0);
}

static void
rk_cdn_dp_fill_forced_mode(struct rk_cdn_dp_softc *sc)
{
	/*
	 * Values come from rk_dp_forced_mode.h; edit there to experiment.
	 * Both this function and rk_drm_fill_forced_dp_mode() consume the
	 * same header and MUST stay in sync.
	 */
	sc->pixel_clock_khz = RK_DP_FORCED_CLOCK_KHZ;
	sc->hdisplay = RK_DP_FORCED_HDISPLAY;
	sc->hblank = RK_DP_FORCED_HBLANK;
	sc->htotal = RK_DP_FORCED_HTOTAL;
	sc->hsync_start = RK_DP_FORCED_HSYNC_START;
	sc->hsync_end = RK_DP_FORCED_HSYNC_END;
	sc->vdisplay = RK_DP_FORCED_VDISPLAY;
	sc->vblank = RK_DP_FORCED_VBLANK;
	sc->vtotal = RK_DP_FORCED_VTOTAL;
	sc->vsync_start = RK_DP_FORCED_VSYNC_START;
	sc->vsync_end = RK_DP_FORCED_VSYNC_END;
	sc->h_sync_polarity = RK_DP_FORCED_H_POLARITY;
	sc->v_sync_polarity = RK_DP_FORCED_V_POLARITY;
	RK_CDN_DP_DPRINTF(sc,
	    "forced video mode: %ux%u @ %u kHz htotal=%u hsync=%u..%u (pol=%u) vtotal=%u vsync=%u..%u (pol=%u)\n",
	    sc->hdisplay, sc->vdisplay, sc->pixel_clock_khz, sc->htotal,
	    sc->hsync_start, sc->hsync_end, sc->h_sync_polarity, sc->vtotal,
	    sc->vsync_start, sc->vsync_end, sc->v_sync_polarity);
}

static void
rk_cdn_dp_fill_mode(struct rk_cdn_dp_softc *sc, uint32_t clock,
    uint16_t hdisplay, uint16_t hsync_start, uint16_t hsync_end,
    uint16_t htotal, uint16_t vdisplay, uint16_t vsync_start,
    uint16_t vsync_end, uint16_t vtotal, uint32_t flags)
{
	sc->pixel_clock_khz = clock;
	sc->hdisplay = hdisplay;
	sc->hblank = htotal - hdisplay;
	sc->hsync_start = hsync_start;
	sc->hsync_end = hsync_end;
	sc->htotal = htotal;
	sc->vdisplay = vdisplay;
	sc->vblank = vtotal - vdisplay;
	sc->vsync_start = vsync_start;
	sc->vsync_end = vsync_end;
	sc->vtotal = vtotal;
	sc->h_sync_polarity =
	    ((flags & RK_CDN_DP_MODE_FLAG_PHSYNC) != 0) ? 1 : 0;
	sc->v_sync_polarity =
	    ((flags & RK_CDN_DP_MODE_FLAG_PVSYNC) != 0) ? 1 : 0;
	RK_CDN_DP_DPRINTF(sc,
	    "selected video mode: %ux%u @ %u kHz htotal=%u hsync=%u..%u (pol=%u) vtotal=%u vsync=%u..%u (pol=%u)\n",
	    sc->hdisplay, sc->vdisplay, sc->pixel_clock_khz, sc->htotal,
	    sc->hsync_start, sc->hsync_end, sc->h_sync_polarity, sc->vtotal,
	    sc->vsync_start, sc->vsync_end, sc->v_sync_polarity);
}

/*
 * Calculate transfer-unit (TU) size + valid-symbol count and program the
 * Cadence DP framer + MSA registers.
 *
 * Hardcoded for now: RGB 8bpc (24 bpp), single-stream (SST_MODE), no audio.
 * TU search loop: pick the smallest TU >= 32 with VS in range 0.1..0.85
 * of TU, VS >= 2, TU - VS >= 4.
 */
static int
rk_cdn_dp_config_video(struct rk_cdn_dp_softc *sc)
{
	int error;
	uint64_t symbol;
	uint32_t link_rate, val, rem;
	uint8_t bpp = 24;	/* RGB 8bpc */
	uint8_t tu = RK_CDN_DP_TU_SIZE_INIT;

	if (!sc->link_trained)
		return (ENOENT);
	if (sc->pixel_clock_khz == 0) {
		rk_cdn_dp_fill_forced_mode(sc);
	}

	link_rate = rk_cdn_dp_link_rate_khz(sc->link_plan_rate_code) / 1000;
	if (link_rate == 0)
		return (EINVAL);

	error = rk_cdn_dp_mailbox_reg_write(sc, RK_CDN_DP_BND_HSYNC2VSYNC,
	    RK_CDN_DP_VIF_BYPASS_INTERLACE);
	if (error != 0)
		return (error);
	error = rk_cdn_dp_mailbox_reg_write(sc, RK_CDN_DP_HSYNC2VSYNC_POL_CTRL,
	    0);
	if (error != 0)
		return (error);

	/*
	 * TU/VS search loop.  symbol is the valid-symbol count per TU
	 * (kept in tu*1000 units to do fractional arithmetic with integer
	 * math; rem is the fractional part *1000).
	 */
	for (;;) {
		tu += 2;
		symbol = (uint64_t)tu * sc->pixel_clock_khz * bpp;
		symbol /= ((uint64_t)sc->link_plan_lanes * link_rate * 8);
		rem = (uint32_t)(symbol % 1000);
		symbol /= 1000;
		if (tu > 64) {
			device_printf(sc->dev,
			    "config_video: tu>64 (clk=%u lanes=%u rate=%u bpp=%u)\n",
			    sc->pixel_clock_khz, sc->link_plan_lanes, link_rate,
			    bpp);
			return (EINVAL);
		}
		if (symbol > 1 && (tu - symbol) >= 4 && rem >= 100 && rem <= 850)
			break;
	}

	val = (uint32_t)symbol | ((uint32_t)tu << 8) | RK_CDN_DP_TU_CNT_RST_EN;
	error = rk_cdn_dp_mailbox_reg_write(sc, RK_CDN_DP_DP_FRAMER_TU, val);
	if (error != 0)
		return (error);

	/* FIFO buffer size table entry — only DP_VC_TABLE(15) is needed. */
	val = (uint32_t)((sc->pixel_clock_khz * (symbol + 1) + 999) / 1000) +
	    link_rate;
	val /= ((uint32_t)sc->link_plan_lanes * link_rate);
	val = (uint32_t)((8 * (symbol + 1)) / bpp) - val;
	val += 2;
	(void)rk_cdn_dp_mailbox_reg_write(sc, RK_CDN_DP_DP_VC_TABLE(15), val);

	/* Pixel representation: BCS_8 (0x02) low, PXL_RGB (0x01) << 8. */
	val = RK_CDN_DP_BCS_8 | ((uint32_t)RK_CDN_DP_PXL_RGB << 8);
	error = rk_cdn_dp_mailbox_reg_write(sc, RK_CDN_DP_DP_FRAMER_PXL_REPR,
	    val);
	if (error != 0)
		return (error);

	val = (sc->h_sync_polarity ? RK_CDN_DP_FRAMER_SP_HSP : 0) |
	    (sc->v_sync_polarity ? RK_CDN_DP_FRAMER_SP_VSP : 0);
	error = rk_cdn_dp_mailbox_reg_write(sc, RK_CDN_DP_DP_FRAMER_SP, val);
	if (error != 0)
		return (error);

	val = ((uint32_t)(sc->hsync_start - sc->hdisplay) << 16) |
	    (uint32_t)(sc->htotal - sc->hsync_end);
	error = rk_cdn_dp_mailbox_reg_write(sc,
	    RK_CDN_DP_DP_FRONT_BACK_PORCH, val);
	if (error != 0)
		return (error);

	val = (uint32_t)sc->hdisplay * bpp / 8;
	error = rk_cdn_dp_mailbox_reg_write(sc, RK_CDN_DP_DP_BYTE_COUNT, val);
	if (error != 0)
		return (error);

	val = (uint32_t)sc->htotal |
	    ((uint32_t)(sc->htotal - sc->hsync_start) << 16);
	error = rk_cdn_dp_mailbox_reg_write(sc, RK_CDN_DP_MSA_HORIZONTAL_0,
	    val);
	if (error != 0)
		return (error);

	val = (uint32_t)(sc->hsync_end - sc->hsync_start) |
	    ((uint32_t)sc->hdisplay << 16) |
	    ((uint32_t)sc->h_sync_polarity << 15);
	error = rk_cdn_dp_mailbox_reg_write(sc, RK_CDN_DP_MSA_HORIZONTAL_1,
	    val);
	if (error != 0)
		return (error);

	val = (uint32_t)sc->vtotal |
	    ((uint32_t)(sc->vtotal - sc->vsync_start) << 16);
	error = rk_cdn_dp_mailbox_reg_write(sc, RK_CDN_DP_MSA_VERTICAL_0, val);
	if (error != 0)
		return (error);

	val = (uint32_t)(sc->vsync_end - sc->vsync_start) |
	    ((uint32_t)sc->vdisplay << 16) |
	    ((uint32_t)sc->v_sync_polarity << 15);
	error = rk_cdn_dp_mailbox_reg_write(sc, RK_CDN_DP_MSA_VERTICAL_1, val);
	if (error != 0)
		return (error);

	/*
	 * MSA_MISC: RGB 8bpc, no YUV.
	 *   misc = 2 * misc0 + 32 * misc1 (misc0=0 for RGB, misc1=1 for 8bpc)
	 *        = 0 + 32 = 0x20
	 */
	error = rk_cdn_dp_mailbox_reg_write(sc, RK_CDN_DP_MSA_MISC, 0x20);
	if (error != 0)
		return (error);

	error = rk_cdn_dp_mailbox_reg_write(sc, RK_CDN_DP_STREAM_CONFIG, 1);
	if (error != 0)
		return (error);
	RK_CDN_DP_DPRINTF(sc, "config_video: STREAM_CONFIG <- 0x00000001\n");

	val = (uint32_t)(sc->hsync_end - sc->hsync_start) |
	    ((uint32_t)sc->hdisplay << 16);
	error = rk_cdn_dp_mailbox_reg_write(sc, RK_CDN_DP_DP_HORIZONTAL, val);
	if (error != 0)
		return (error);

	val = (uint32_t)sc->vdisplay |
	    ((uint32_t)(sc->vtotal - sc->vsync_start) << 16);
	error = rk_cdn_dp_mailbox_reg_write(sc, RK_CDN_DP_DP_VERTICAL_0, val);
	if (error != 0)
		return (error);

	val = (uint32_t)sc->vtotal;
	error = rk_cdn_dp_mailbox_reg_write(sc, RK_CDN_DP_DP_VERTICAL_1, val);
	if (error != 0)
		return (error);

	/*
	 * Final commit: clear DP_VB_ID[2] (NO_VIDEO).  the reference driver's cdn_dp_config_video
	 * does this as the last write in the function (cdn-dp-reg.c:896) — it
	 * tells the framer that the upcoming frames are real video, not blanking.
	 * Without it the sink sees a valid link but never sees pixels.
	 */
	error = rk_cdn_dp_mailbox_reg_write_bit(sc, RK_CDN_DP_DP_VB_ID, 2, 1, 0);
	if (error != 0) {
		RK_CDN_DP_DPRINTF(sc,
		    "config_video: DP_VB_ID NO_VIDEO clear failed (%d)\n",
		    error);
		return (error);
	}

	sc->video_configured = true;
	RK_CDN_DP_DPRINTF(sc,
	    "config_video: TU=%u VS=%llu (rem=%u) %ux%u @ %u kHz, %u-lane HBR\n",
	    tu, (unsigned long long)symbol, rem, sc->hdisplay, sc->vdisplay,
	    sc->pixel_clock_khz, sc->link_plan_lanes);
	return (0);
}

/*
 * Send the DPTX_SET_VIDEO mailbox to enable/disable the video output.
 * Without VOP feeding pixels into CDN-DP this just signals the firmware
 * to start/stop driving the framer; useful as a smoke-test.
 */
static int
rk_cdn_dp_set_video_status(struct rk_cdn_dp_softc *sc, bool active)
{
	uint8_t msg = active ? 1 : 0;
	int error;

	if (!sc->fw_link_training_used)
		return (0);

	error = rk_cdn_dp_mailbox_send(sc, RK_CDN_DP_MB_MODULE_ID_DP_TX,
	    RK_CDN_DP_DPTX_SET_VIDEO, sizeof(msg), &msg);
	if (error == 0)
		rk_cdn_dp_verify_link_config(sc,
		    active ? "set_video_status(active)" :
		    "set_video_status(idle)");
	return (error);
}

/*
 * Exported entry point: lets rk_drm transition the dptx firmware framer
 * to VIDEO_VALID after the VOP modeset has actually programmed the VOP
 * to produce pixels. Mirrors reference vendor BSP's cdn-dp-core.c flow where
 * cdn_dp_encoder_enable sends VIDEO_IDLE before config_video and
 * VIDEO_VALID after.  Stage 19 now leaves the framer in IDLE so that
 * the CRTC side performs the single final VALID edge once pixels are live.
 */
int
rk_cdn_dp_set_video_active_first(bool active)
{
	devclass_t dc;
	device_t dev;
	struct rk_cdn_dp_softc *sc;

	dc = devclass_find("rk_cdn_dp");
	if (dc == NULL)
		return (ENXIO);
	dev = devclass_get_device(dc, 0);
	if (dev == NULL)
		return (ENXIO);
	sc = device_get_softc(dev);
	if (sc == NULL)
		return (ENXIO);
	return (rk_cdn_dp_set_video_status(sc, active));
}

int
rk_cdn_dp_auto_bringup_default(void)
{
	devclass_t dc;
	device_t dev;
	struct rk_cdn_dp_softc *sc;
	int error;

	dc = devclass_find("rk_cdn_dp");
	if (dc == NULL)
		return (ENXIO);
	dev = devclass_get_device(dc, 0);
	if (dev == NULL)
		return (ENXIO);
	sc = device_get_softc(dev);
	if (sc == NULL)
		return (ENXIO);

	rk_cdn_dp_reset_runtime_state(sc);
	sc->skip_aux_swap = rk_cdn_dp_is_rockpro64(sc->dev) ? 1 : 0;
	sc->hostcap_lanes_override = 2;
	sc->allow_phys = true;
	sc->allow_aux = true;

	error = rk_cdn_dp_set_stage(sc, RK_CDN_DP_STAGE_VIDEO_ON);
	if (error != 0) {
		RK_CDN_DP_DPRINTF(sc,
		    "auto_bringup_default: stage19 failed: %d\n", error);
		return (error);
	}

	RK_CDN_DP_DPRINTF(sc,
	    "auto_bringup_default: stage19 complete (skip_aux_swap=%d lanes=%d)\n",
	    sc->skip_aux_swap, sc->hostcap_lanes_override);
	return (0);
}

/*
 * Re-run DP link training against the already-configured Cadence framer
 * without touching the µCPU, firmware, or framer state.  Intended to be
 * invoked by an HPD_IRQ-driven loop (e.g. rk_drm_hpd_task) when the sink
 * signals it needs retraining via the LINK_STATUS_UPDATED bit / VDM
 * ATTENTION with HPD_IRQ.
 *
 * Mirrors the reference driver's cdn_dp_pd_event_work → cdn_dp_link_train path: the
 * framer (DP_FRAMER_GLOBAL_CONFIG, MSA, STREAM_CONFIG, DP_VB_ID) stays
 * exactly as auto_bringup_default left it.  We only re-do CR+EQ.
 *
 * Returns ENOTCONN if the link has never been trained yet (caller should
 * not invoke retrain before initial bring-up completes).
 */
int
rk_cdn_dp_retrain_default(void)
{
	devclass_t dc;
	device_t dev;
	struct rk_cdn_dp_softc *sc;
	int error;

	dc = devclass_find("rk_cdn_dp");
	if (dc == NULL)
		return (ENXIO);
	dev = devclass_get_device(dc, 0);
	if (dev == NULL)
		return (ENXIO);
	sc = device_get_softc(dev);
	if (sc == NULL)
		return (ENXIO);
	if (!sc->sink_caps_valid || sc->link_plan_lanes == 0 ||
	    sc->link_plan_rate_code == 0)
		return (ENOTCONN);

	sx_slock(&sc->detach_sx);
	if (sc->detached) {
		sx_sunlock(&sc->detach_sx);
		return (ENXIO);
	}
	error = rk_cdn_dp_link_train(sc);
	sx_sunlock(&sc->detach_sx);

	if (error == 0)
		device_printf(sc->dev,
		    "retrain_default: link retrained (rate=0x%x lanes=%u)\n",
		    sc->link_plan_rate_code, sc->link_plan_lanes);
	else
		device_printf(sc->dev,
		    "retrain_default: failed (%d)\n", error);
	return (error);
}

/*
 * USB-C DP audio bring-up — I2S input path (HDMI audio also runs via
 * I2S2 today, so reusing the same data wires).  Ported from the
 * Rockchip 4.4 BSP cdn-dp-reg.c (cdn_dp_audio_config_i2s + helpers).
 * Caller fixes 2-channel LPCM, 48 kHz, 16-bit for the first cut;
 * tunables for other rates can come later.
 */
static int
rk_cdn_dp_audio_config_i2s_locked(struct rk_cdn_dp_softc *sc,
    int channels, int sample_rate, int sample_width, int lanes)
{
	uint32_t val;
	int sub_pckt_num = 1;
	int i2s_port_en_val = 0xf;
	int i, error;

	/*
	 * Bring the cdn-dp clock manager into the state the audio
	 * sub-block needs.  Without these two mailbox writes, MMIO
	 * accesses to AUDIO_SRC_CNTL / FIFO_CNTL / SMPL2PKT_CNTL
	 * (offsets 0x30000+) hit an unpowered region and fault the
	 * kernel.  Mirrors cdn_dp_audio_config in the Rockchip 4.4
	 * BSP.
	 */
	error = rk_cdn_dp_mailbox_reg_write(sc, RK_CDN_DP_CM_LANE_CTRL,
	    RK_CDN_DP_LANE_REF_CYC);
	if (error != 0) {
		device_printf(sc->dev,
		    "audio_config: CM_LANE_CTRL write failed %d\n", error);
		return (error);
	}
	error = rk_cdn_dp_mailbox_reg_write(sc, RK_CDN_DP_CM_CTRL, 0);
	if (error != 0) {
		device_printf(sc->dev,
		    "audio_config: CM_CTRL write failed %d\n", error);
		return (error);
	}

	if (channels == 2) {
		sub_pckt_num = (lanes == 1) ? 2 : 4;
		i2s_port_en_val = 1;
	} else if (channels == 4) {
		i2s_port_en_val = 3;
	}

	rk_cdn_dp_write_4(sc, RK_CDN_DP_SPDIF_CTRL_ADDR, 0);
	rk_cdn_dp_write_4(sc, RK_CDN_DP_FIFO_CNTL,
	    RK_CDN_DP_SYNC_WR_TO_CH_ZERO);

	val = RK_CDN_DP_MAX_NUM_CH(channels) |
	    RK_CDN_DP_NUM_OF_I2S_PORTS(channels) |
	    RK_CDN_DP_AUDIO_TYPE_LPCM |
	    RK_CDN_DP_CFG_SUB_PCKT_NUM(sub_pckt_num);
	rk_cdn_dp_write_4(sc, RK_CDN_DP_SMPL2PKT_CNFG, val);

	if (sample_width == 16)
		val = 0;
	else if (sample_width == 24)
		val = (1U << 9);
	else
		val = (2U << 9);
	val |= RK_CDN_DP_AUDIO_CH_NUM(channels) |
	    RK_CDN_DP_I2S_DEC_PORT_EN(i2s_port_en_val) |
	    RK_CDN_DP_TRANS_SMPL_WIDTH_32;
	rk_cdn_dp_write_4(sc, RK_CDN_DP_AUDIO_SRC_CNFG, val);

	/* Per-channel IEC 60958 status bits. */
	for (i = 0; i < (channels + 1) / 2; i++) {
		if (sample_width == 16)
			val = (0x02 << 8) | (0x02 << 20);
		else
			val = (0x0b << 8) | (0x0b << 20);
		val |= ((2 * i) << 4) | ((2 * i + 1) << 16);
		rk_cdn_dp_write_4(sc, RK_CDN_DP_STTS_BIT_CH(i), val);
	}

	/* Sample-rate / original-frequency channel status. */
	switch (sample_rate) {
	case 32000:
		val = RK_CDN_DP_SAMPLING_FREQ(3) |
		    RK_CDN_DP_ORIGINAL_SAMP_FREQ(0xc);
		break;
	case 44100:
		val = RK_CDN_DP_SAMPLING_FREQ(0) |
		    RK_CDN_DP_ORIGINAL_SAMP_FREQ(0xf);
		break;
	case 48000:
		val = RK_CDN_DP_SAMPLING_FREQ(2) |
		    RK_CDN_DP_ORIGINAL_SAMP_FREQ(0xd);
		break;
	case 88200:
		val = RK_CDN_DP_SAMPLING_FREQ(8) |
		    RK_CDN_DP_ORIGINAL_SAMP_FREQ(7);
		break;
	case 96000:
		val = RK_CDN_DP_SAMPLING_FREQ(0xa) |
		    RK_CDN_DP_ORIGINAL_SAMP_FREQ(5);
		break;
	case 176400:
		val = RK_CDN_DP_SAMPLING_FREQ(0xc) |
		    RK_CDN_DP_ORIGINAL_SAMP_FREQ(3);
		break;
	case 192000:
		val = RK_CDN_DP_SAMPLING_FREQ(0xe) |
		    RK_CDN_DP_ORIGINAL_SAMP_FREQ(1);
		break;
	default:
		device_printf(sc->dev,
		    "audio_config: unsupported sample rate %d\n", sample_rate);
		return (EINVAL);
	}
	val |= 4;
	rk_cdn_dp_write_4(sc, RK_CDN_DP_COM_CH_STTS_BITS, val);

	rk_cdn_dp_write_4(sc, RK_CDN_DP_SMPL2PKT_CNTL,
	    RK_CDN_DP_SMPL2PKT_EN);
	rk_cdn_dp_write_4(sc, RK_CDN_DP_AUDIO_SRC_CNTL,
	    RK_CDN_DP_I2S_DEC_START);

	return (rk_cdn_dp_mailbox_reg_write(sc,
	    RK_CDN_DP_AUDIO_PACK_CONTROL, RK_CDN_DP_AUDIO_PACK_EN));
}

/*
 * Write a 32-byte DP Secondary Data Packet (SDP) into one of the
 * cdn-dp packet allocation slots.  Direct MMIO path: stream the data
 * words through SOURCE_PIF_DATA_WR, then poke the packet-type/slot
 * registers to arm it.  Matches Rockchip 4.4 BSP cdn_dp_infoframe_set.
 */
static void
rk_cdn_dp_infoframe_set_locked(struct rk_cdn_dp_softc *sc, int entry_id,
    const uint8_t *buf, uint32_t len, int type)
{
	uint32_t words;
	uint32_t v;
	unsigned int i;

	words = len / 4;
	for (i = 0; i < words; i++) {
		v = (uint32_t)buf[i * 4] |
		    ((uint32_t)buf[i * 4 + 1] << 8) |
		    ((uint32_t)buf[i * 4 + 2] << 16) |
		    ((uint32_t)buf[i * 4 + 3] << 24);
		rk_cdn_dp_write_4(sc, RK_CDN_DP_SOURCE_PIF_DATA_WR, v);
	}

	rk_cdn_dp_write_4(sc, RK_CDN_DP_SOURCE_PIF_WR_ADDR,
	    (uint32_t)entry_id);
	rk_cdn_dp_write_4(sc, RK_CDN_DP_SOURCE_PIF_WR_REQ,
	    RK_CDN_DP_HOST_WR);
	rk_cdn_dp_write_4(sc, RK_CDN_DP_SOURCE_PIF_PKT_ALLOC_REG,
	    RK_CDN_DP_ACTIVE_IDLE_TYPE(1) | RK_CDN_DP_TYPE_VALID |
	    RK_CDN_DP_PACKET_TYPE(type) |
	    RK_CDN_DP_PKT_ALLOC_ADDRESS(entry_id));
	rk_cdn_dp_write_4(sc, RK_CDN_DP_SOURCE_PIF_PKT_ALLOC_WR_EN,
	    RK_CDN_DP_PKT_ALLOC_WR_EN);
}

/*
 * Build a minimal HDMI audio infoframe SDP and push it to slot 0,
 * type 0x84.  For stream-encoded fields (coding_type=STREAM,
 * sample_frequency=STREAM, sample_size=STREAM, channels=0) every
 * body byte is zero, so the packed buffer collapses to a 32-byte
 * payload of (HB0=0, HB1=0x84, HB2=0x1b, HB3=0x48) followed by 28
 * zero bytes.  Ported from BSP cdn_dp_setup_audio_infoframe.
 */
static void
rk_cdn_dp_setup_audio_infoframe_locked(struct rk_cdn_dp_softc *sc)
{
	uint8_t sdp[32];

	memset(sdp, 0, sizeof(sdp));
	sdp[0] = 0;
	sdp[1] = RK_CDN_DP_HDMI_INFOFRAME_TYPE_AUDIO;
	sdp[2] = 0x1b;
	sdp[3] = 0x48;

	rk_cdn_dp_infoframe_set_locked(sc, 0, sdp, sizeof(sdp),
	    RK_CDN_DP_HDMI_INFOFRAME_TYPE_AUDIO);
}

int
rk_cdn_dp_audio_mute(bool mute)
{
	devclass_t dc;
	device_t dev;
	struct rk_cdn_dp_softc *sc;

	dc = devclass_find("rk_cdn_dp");
	dev = dc != NULL ? devclass_get_device(dc, 0) : NULL;
	sc = dev != NULL ? device_get_softc(dev) : NULL;
	if (sc == NULL)
		return (ENXIO);
	return (rk_cdn_dp_mailbox_reg_write_bit(sc,
	    RK_CDN_DP_DP_VB_ID, 4, 1, mute ? 1 : 0));
}

int
rk_cdn_dp_audio_start(int channels, int sample_rate, int sample_width)
{
	devclass_t dc;
	device_t dev;
	struct rk_cdn_dp_softc *sc;
	int error;

	dc = devclass_find("rk_cdn_dp");
	dev = dc != NULL ? devclass_get_device(dc, 0) : NULL;
	sc = dev != NULL ? device_get_softc(dev) : NULL;
	if (sc == NULL)
		return (ENXIO);

	(void)rk_cdn_dp_audio_mute(true);

	/*
	 * Idempotent: tear down any prior audio config before
	 * re-arming.  Without this, a second audio_start_now after a
	 * working first run hits half-configured state and faults.
	 */
	(void)rk_cdn_dp_mailbox_reg_write(sc,
	    RK_CDN_DP_AUDIO_PACK_CONTROL, 0);
	rk_cdn_dp_write_4(sc, RK_CDN_DP_SPDIF_CTRL_ADDR, 0);
	rk_cdn_dp_write_4(sc, RK_CDN_DP_AUDIO_SRC_CNTL, 0);
	rk_cdn_dp_write_4(sc, RK_CDN_DP_AUDIO_SRC_CNFG, 0);
	rk_cdn_dp_write_4(sc, RK_CDN_DP_AUDIO_SRC_CNTL,
	    RK_CDN_DP_AUDIO_SW_RST);
	rk_cdn_dp_write_4(sc, RK_CDN_DP_AUDIO_SRC_CNTL, 0);
	rk_cdn_dp_write_4(sc, RK_CDN_DP_SMPL2PKT_CNTL, 0);
	rk_cdn_dp_write_4(sc, RK_CDN_DP_SMPL2PKT_CNTL,
	    RK_CDN_DP_AUDIO_SW_RST);
	rk_cdn_dp_write_4(sc, RK_CDN_DP_SMPL2PKT_CNTL, 0);
	rk_cdn_dp_write_4(sc, RK_CDN_DP_FIFO_CNTL,
	    RK_CDN_DP_AUDIO_SW_RST);
	rk_cdn_dp_write_4(sc, RK_CDN_DP_FIFO_CNTL, 0);

	/*
	 * Order matches BSP cdn_dp_audio_hw_params: infoframe SDP must
	 * be armed before the audio packetizer starts emitting samples,
	 * otherwise the sink discards every packet.
	 */
	rk_cdn_dp_setup_audio_infoframe_locked(sc);

	error = rk_cdn_dp_audio_config_i2s_locked(sc, channels,
	    sample_rate, sample_width, sc->link_plan_lanes);
	if (error != 0) {
		device_printf(sc->dev,
		    "audio_start: config failed %d\n", error);
		return (error);
	}
	(void)rk_cdn_dp_audio_mute(false);
	device_printf(sc->dev,
	    "audio_start: I2S %dch %dHz %dbit infoframe+unmuted\n",
	    channels, sample_rate, sample_width);
	return (0);
}

int
rk_cdn_dp_audio_stop(void)
{
	devclass_t dc;
	device_t dev;
	struct rk_cdn_dp_softc *sc;

	dc = devclass_find("rk_cdn_dp");
	dev = dc != NULL ? devclass_get_device(dc, 0) : NULL;
	sc = dev != NULL ? device_get_softc(dev) : NULL;
	if (sc == NULL)
		return (ENXIO);

	(void)rk_cdn_dp_audio_mute(true);
	(void)rk_cdn_dp_mailbox_reg_write(sc,
	    RK_CDN_DP_AUDIO_PACK_CONTROL, 0);
	rk_cdn_dp_write_4(sc, RK_CDN_DP_SPDIF_CTRL_ADDR, 0);
	rk_cdn_dp_write_4(sc, RK_CDN_DP_AUDIO_SRC_CNTL, 0);
	rk_cdn_dp_write_4(sc, RK_CDN_DP_AUDIO_SRC_CNFG, 0);
	rk_cdn_dp_write_4(sc, RK_CDN_DP_AUDIO_SRC_CNTL,
	    RK_CDN_DP_AUDIO_SW_RST);
	rk_cdn_dp_write_4(sc, RK_CDN_DP_AUDIO_SRC_CNTL, 0);
	rk_cdn_dp_write_4(sc, RK_CDN_DP_SMPL2PKT_CNTL, 0);
	rk_cdn_dp_write_4(sc, RK_CDN_DP_SMPL2PKT_CNTL,
	    RK_CDN_DP_AUDIO_SW_RST);
	rk_cdn_dp_write_4(sc, RK_CDN_DP_SMPL2PKT_CNTL, 0);
	rk_cdn_dp_write_4(sc, RK_CDN_DP_FIFO_CNTL,
	    RK_CDN_DP_AUDIO_SW_RST);
	rk_cdn_dp_write_4(sc, RK_CDN_DP_FIFO_CNTL, 0);
	device_printf(sc->dev, "audio_stop: muted + reset\n");
	return (0);
}

int
rk_cdn_dp_enable_mode(uint32_t clock, uint16_t hdisplay,
    uint16_t hsync_start, uint16_t hsync_end, uint16_t htotal,
    uint16_t vdisplay, uint16_t vsync_start, uint16_t vsync_end,
    uint16_t vtotal, uint32_t flags)
{
	devclass_t dc;
	device_t dev;
	struct rk_cdn_dp_softc *sc;
	int error;

	dc = devclass_find("rk_cdn_dp");
	if (dc == NULL)
		return (ENXIO);
	dev = devclass_get_device(dc, 0);
	if (dev == NULL)
		return (ENXIO);
	sc = device_get_softc(dev);
	if (sc == NULL)
		return (ENXIO);

	/*
	 * Fast path: if we're already trained at VIDEO_ON and DRM is asking
	 * for the same mode we last configured, no-op.  Saves the second
	 * fw-load + link train on the post-EDID re-modeset.
	 *
	 * Slower fast path: if we're already at VIDEO_ON but DRM asks for a
	 * different mode, re-derive the mode and re-run only stages 18-19
	 * (config_video + video on).  Skips the destructive reset and
	 * fw-reload — the link stays up, only MSA / DP_HORIZONTAL etc. change.
	 */
	if (sc->video_configured && sc->link_trained) {
		if (sc->pixel_clock_khz == clock && sc->hdisplay == hdisplay &&
		    sc->hsync_start == hsync_start &&
		    sc->hsync_end == hsync_end && sc->htotal == htotal &&
		    sc->vdisplay == vdisplay &&
		    sc->vsync_start == vsync_start &&
		    sc->vsync_end == vsync_end && sc->vtotal == vtotal) {
			RK_CDN_DP_DPRINTF(sc,
			    "enable_mode: %ux%u already active, no-op\n",
			    hdisplay, vdisplay);
			return (0);
		}
		RK_CDN_DP_DPRINTF(sc,
		    "enable_mode: fast-path %ux%u@%u (skipping fw-reload, "
		    "re-running config_video only)\n",
		    hdisplay, vdisplay, clock);
		rk_cdn_dp_fill_mode(sc, clock, hdisplay, hsync_start, hsync_end,
		    htotal, vdisplay, vsync_start, vsync_end, vtotal, flags);
		sc->video_configured = false;
		/* Rewind so set_stage re-runs CONFIG_VIDEO then VIDEO_ON. */
		sc->stage = RK_CDN_DP_STAGE_EDID;
		error = rk_cdn_dp_set_stage(sc, RK_CDN_DP_STAGE_VIDEO_ON);
		if (error != 0) {
			RK_CDN_DP_DPRINTF(sc,
			    "enable_mode: fast-path config-video failed: %d\n",
			    error);
			return (error);
		}
		RK_CDN_DP_DPRINTF(sc,
		    "enable_mode: ready for %ux%u@%u kHz (fast-path)\n",
		    sc->hdisplay, sc->vdisplay, sc->pixel_clock_khz);
		return (0);
	}

	/*
	 * Slow path (first bring-up): mirror the the reference driver encoder path closely:
	 * - reset runtime/link state
	 * - derive mode from the DRM-selected mode
	 * - enable/train/configure the Cadence block for that exact mode
	 * - leave the framer idle only if firmware training was used
	 */
	rk_cdn_dp_reset_runtime_state(sc);
	sc->skip_aux_swap = rk_cdn_dp_is_rockpro64(sc->dev) ? 1 : 0;
	sc->hostcap_lanes_override = 2;
	sc->allow_phys = true;
	sc->allow_aux = true;
	rk_cdn_dp_fill_mode(sc, clock, hdisplay, hsync_start, hsync_end, htotal,
	    vdisplay, vsync_start, vsync_end, vtotal, flags);

	error = rk_cdn_dp_set_stage(sc, RK_CDN_DP_STAGE_CONFIG_VIDEO);
	if (error != 0) {
		device_printf(sc->dev,
		    "enable_mode: config-video path failed: %d\n", error);
		return (error);
	}
	if (sc->fw_link_training_used) {
		error = rk_cdn_dp_set_stage(sc, RK_CDN_DP_STAGE_VIDEO_ON);
		if (error != 0) {
			device_printf(sc->dev,
			    "enable_mode: video-idle arm failed: %d\n", error);
			return (error);
		}
	}

	RK_CDN_DP_DPRINTF(sc,
	    "enable_mode: ready for %ux%u@%u kHz (fw_training=%d stage=%d)\n",
	    sc->hdisplay, sc->vdisplay, sc->pixel_clock_khz,
	    sc->fw_link_training_used ? 1 : 0, sc->stage);
	return (0);
}

static int
rk_cdn_dp_start_link_training(struct rk_cdn_dp_softc *sc)
{
	uint8_t buf[4];
	uint8_t n;
	int error;

	if (!sc->sink_caps_valid || sc->link_plan_lanes == 0 ||
	    sc->link_plan_rate_code == 0)
		return (ENOENT);

	buf[0] = (sc->aux_dpcd[DP_MAX_DOWNSPREAD] & DP_MAX_DOWNSPREAD_0_5) ?
	    DP_SPREAD_AMP_0_5 : 0;
	buf[1] = (sc->aux_dpcd[DP_MAIN_LINK_CHANNEL_CODING] & 0x01) ?
	    DP_SET_ANSI_8B10B : 0;
	error = rk_cdn_dp_mailbox_dpcd_write(sc, DP_DOWNSPREAD_CTRL, buf, 2);
	if (error != 0)
		return (error);

	error = rk_cdn_dp_write_link_config(sc, "start_link_training");
	if (error != 0)
		return (error);

	n = sc->link_plan_lanes;
	if (n > 4)
		n = 4;
	memset(buf, 0, sizeof(buf));
	error = rk_cdn_dp_mailbox_dpcd_write(sc, DP_TRAINING_PATTERN_SET,
	    (uint8_t[]){ DP_TRAINING_PATTERN_1 | DP_LINK_SCRAMBLING_DISABLE, 0, 0, 0, 0 },
	    1 + n);
	if (error != 0)
		return (error);

	error = rk_cdn_dp_mailbox_dpcd_read(sc, DP_LANE0_1_STATUS,
	    sc->link_status, DP_LINK_STATUS_SIZE);
	if (error != 0)
		return (error);

	RK_CDN_DP_DPRINTF(sc,
	    "link-train-start: status raw: %02x %02x %02x %02x %02x %02x\n",
	    sc->link_status[0], sc->link_status[1], sc->link_status[2],
	    sc->link_status[3], sc->link_status[4], sc->link_status[5]);

	return (0);
}

/*
 * rk_cdn_dp_probe
 *
 * Standard FreeBSD device probe.  Rejects devices marked disabled in the DTB
 * so the driver is only active when the DTB overlay sets status = "okay" —
 * keeping the module safe to load on boards where the CDN-DP node is present
 * but intentionally not in use.
 */
static int
rk_cdn_dp_probe(device_t dev)
{
	if (ofw_bus_search_compatible(dev, rk_cdn_dp_compat_data)->ocd_data == 0)
		return (ENXIO);
	device_set_desc(dev, "Rockchip RK3399 Cadence DisplayPort scaffold");
	return (BUS_PROBE_DEFAULT);
}

/*
 * rk_cdn_dp_get_clocks
 *
 * Acquires handles for the four CDN-DP clocks from the DTS clock-names list.
 * Handles are stored in sc->clks[] for later enable/disable in rk_cdn_dp_enable
 * and rk_cdn_dp_release.  Acquiring separately from enabling allows the release
 * path to safely release only the handles that were successfully obtained.
 */
static int
rk_cdn_dp_get_clocks(struct rk_cdn_dp_softc *sc)
{
	int i, error;

	for (i = 0; i < RK_CDN_DP_NCLKS; i++) {
		error = clk_get_by_ofw_name(sc->dev, 0, rk_cdn_dp_clk_names[i],
		    &sc->clks[i]);
		if (error != 0) {
			device_printf(sc->dev, "cannot get clock %s\n",
			    rk_cdn_dp_clk_names[i]);
			return (error);
		}
	}

	return (0);
}

/*
 * rk_cdn_dp_get_resets
 *
 * Acquires handles for the four CDN-DP reset lines from the DTS reset-names
 * list.  Kept separate from the enable step for the same reason as clocks:
 * the release path needs to know exactly which handles exist before freeing.
 */
static int
rk_cdn_dp_get_resets(struct rk_cdn_dp_softc *sc)
{
	int i, error;

	for (i = 0; i < RK_CDN_DP_NRSTS; i++) {
		error = hwreset_get_by_ofw_name(sc->dev, 0, rk_cdn_dp_rst_names[i],
		    &sc->rsts[i]);
		if (error != 0) {
			device_printf(sc->dev, "cannot get reset %s\n",
			    rk_cdn_dp_rst_names[i]);
			return (error);
		}
	}

	return (0);
}

/*
 * rk_cdn_dp_get_phys
 *
 * Discovers and acquires the TYPE-C PHY lanes assigned to the CDN-DP
 * controller.  The RK3399 has two TYPE-C ports; the DTS phys property lists
 * whichever ports are wired to CDN-DP on the board (RockPro64 exposes both).
 * ENOENT from phy_get_by_ofw_idx means the list is exhausted — not an error.
 * At least one PHY must be present for DP to be possible at all.
 */
static int
rk_cdn_dp_get_phys(struct rk_cdn_dp_softc *sc)
{
	phy_t phy;
	int error, i;

	sc->nphys = 0;
	for (i = 0; i < RK_CDN_DP_MAX_PHYS; i++) {
		error = phy_get_by_ofw_idx(sc->dev, sc->node, i, &phy);
		if (error != 0) {
			if (error == ENOENT)
				break;
			device_printf(sc->dev, "cannot get phy index %d\n", i);
			return (error);
		}
		sc->phys[sc->nphys++] = phy;
	}
	if (sc->nphys == 0) {
		device_printf(sc->dev, "no DP phys available\n");
		return (ENXIO);
	}
	if (sc->rockpro64_typec0_only && sc->nphys > 1)
		sc->nphys = 1;

	return (0);
}

/*
 * rk_cdn_dp_get_power_domain
 *
 * Resolves the power-domain phandle from the DTS into a device handle and
 * domain ID that can be passed to rk3399_power_enable_domain_for_node.  The CDN-DP
 * controller sits in the HDCP power domain (domain 21 in PMU_PWRDN_CON),
 * which must be ungated before any CDN-DP MMIO access; failure to do so
 * causes an ARM SError (asynchronous external abort) because the APB bus
 * returns a fault for accesses to unpowered blocks.
 *
 * If the DTS has no power-domains property the controller is assumed always
 * powered and sc->power_dev is left NULL.
 */
static int
rk_cdn_dp_get_power_domain(struct rk_cdn_dp_softc *sc)
{
	pcell_t *cells;
	phandle_t pnode;
	phandle_t xref;
	int error, ncells;

	cells = NULL;
	error = ofw_bus_parse_xref_list_alloc(sc->node, "power-domains",
	    "#power-domain-cells", 0, &xref, &ncells, &cells);
	if (error == ENOENT)
		return (0);
	if (error != 0)
		return (error);
	sc->has_power_domain = true;
	if (ncells != 1) {
		OF_prop_free(cells);
		device_printf(sc->dev, "invalid power-domains specifier\n");
		return (EINVAL);
	}

	sc->power_dev = OF_device_from_xref(xref);
	sc->power_domain_id = cells[0];
	OF_prop_free(cells);

	if (sc->power_dev == NULL) {
		RK_CDN_DP_DPRINTF(sc,
		    "power-domain provider is not attached yet\n");
		return (ENXIO);
	}

	pnode = ofw_bus_get_node(sc->power_dev);
	if (pnode > 0 &&
	    syscon_get_by_ofw_node(sc->dev, OF_parent(pnode),
	    &sc->pmu_syscon) != 0)
		sc->pmu_syscon = NULL;

	return (0);
}

/*
 * rk_cdn_dp_power_domain_ready
 *
 * Checks the RK3399 PMU state directly.  If the HDCP domain is already
 * powered and both bus-idle bits are clear, there is nothing left for the
 * rk3399_power provider to do and stage 1 can safely skip it.
 */
static bool
rk_cdn_dp_power_domain_ready(struct rk_cdn_dp_softc *sc)
{
	uint32_t bus_mask, domain_mask;
	uint32_t bus_ack, bus_st, pwrdn_st;

	if (!sc->has_power_domain || sc->pmu_syscon == NULL)
		return (false);

	domain_mask = (1U << sc->power_domain_id);
	bus_mask = (1U << RK_CDN_DP_HDCP_BUS_BIT);
	pwrdn_st = SYSCON_READ_4(sc->pmu_syscon, RK_CDN_DP_PMU_PWRDN_ST);
	bus_st = SYSCON_READ_4(sc->pmu_syscon, RK_CDN_DP_PMU_BUS_IDLE_ST);
	bus_ack = SYSCON_READ_4(sc->pmu_syscon, RK_CDN_DP_PMU_BUS_IDLE_ACK);

	return ((pwrdn_st & domain_mask) == 0 &&
	    (bus_st & bus_mask) == 0 &&
	    (bus_ack & bus_mask) == 0);
}

/*
 * rk_cdn_dp_get_extcon
 *
 * Resolves the optional extcon provider from the DT extcon property into a
 * device handle.  On RockPro64 this is the FUSB302 Type-C controller.  The
 * current bridge is intentionally minimal: it lets the DP side query attach,
 * role, and orientation state without pretending that a full USB-PD policy
 * manager already exists in FreeBSD.
 */
static int
rk_cdn_dp_get_extcon(struct rk_cdn_dp_softc *sc)
{
	devclass_t dc;
	pcell_t xref;
	ssize_t len;

	if (!sc->has_extcon)
		goto fallback;

	len = OF_getencprop(sc->node, "extcon", &xref, sizeof(xref));
	if (len <= 0)
		goto fallback;
	if (len < (ssize_t)sizeof(xref))
		return (EINVAL);

	sc->extcon_dev = OF_device_from_xref(xref);
	if (sc->extcon_dev == NULL) {
		device_printf(sc->dev,
		    "extcon provider is not attached yet\n");
		goto fallback;
	}

	return (0);

fallback:
	dc = devclass_find("fusb302");
	if (dc != NULL)
		sc->extcon_dev = devclass_get_device(dc, 0);
	if (sc->extcon_dev != NULL) {
		RK_CDN_DP_DPRINTF(sc,
		    "using fusb302 fallback extcon provider %s%d\n",
		    device_get_name(sc->extcon_dev),
		    device_get_unit(sc->extcon_dev));
		return (0);
	}
	return (ENOENT);
}

static void
rk_cdn_dp_refresh_typec_provider(struct rk_cdn_dp_softc *sc)
{
	devclass_t dc;
	device_t dev;
	pcell_t xref;
	ssize_t len;

	if (sc == NULL)
		return;
	if (sc->extcon_dev != NULL && device_is_attached(sc->extcon_dev))
		return;

	sc->extcon_dev = NULL;
	if (sc->has_extcon) {
		len = OF_getencprop(sc->node, "extcon", &xref, sizeof(xref));
		if (len == (ssize_t)sizeof(xref)) {
			dev = OF_device_from_xref(xref);
			if (dev != NULL && device_is_attached(dev)) {
				sc->extcon_dev = dev;
				return;
			}
		}
	}

	dc = devclass_find("fusb302");
	if (dc == NULL)
		return;
	dev = devclass_get_device(dc, 0);
	if (dev != NULL && device_is_attached(dev))
		sc->extcon_dev = dev;
}

static void
rk_cdn_dp_snapshot_typec_state(struct rk_cdn_dp_softc *sc, const char *tag)
{
	struct rk3399_typec_dp_altmode_status altmode;
	bool have_altmode;

	if (sc == NULL)
		return;

	rk_cdn_dp_refresh_typec_provider(sc);
	have_altmode = rk_cdn_dp_get_altmode_status(sc, &altmode);
	if (!have_altmode) {
		sc->dp_altmode_valid = 0;
		sc->dp_altmode_ready = 0;
		sc->dp_altmode_usb_ss = -1;
		sc->dp_altmode_pin_assignment = 0;
		sc->dp_altmode_status = 0;
	}
	if (tag != NULL)
		rk_cdn_dp_log_typec_state(sc, tag);
}

static bool
rk_cdn_dp_is_rockpro64(device_t dev)
{
	phandle_t root;

	root = OF_finddevice("/");
	if (root <= 0)
		return (false);

	return (ofw_bus_node_is_compatible(root, "pine64,rockpro64") ||
	    ofw_bus_node_is_compatible(root, "pine64,rockpro64-v2.0") ||
	    ofw_bus_node_is_compatible(root, "pine64,rockpro64-v2.1"));
}

static device_t
rk_cdn_dp_resolve_typec_dev(struct rk_cdn_dp_softc *sc)
{
	devclass_t dc;

	if (sc == NULL)
		return (NULL);
	if (sc->extcon_dev != NULL)
		return (sc->extcon_dev);

	dc = devclass_find("fusb302");
	if (dc != NULL)
		sc->extcon_dev = devclass_get_device(dc, 0);

	return (sc->extcon_dev);
}

static bool
rk_cdn_dp_get_typec_status(struct rk_cdn_dp_softc *sc,
    struct fusb302_typec_status *status)
{
	int (*get_status)(device_t, struct fusb302_typec_status *);
	device_t dev;

	if (status == NULL)
		return (false);
	dev = rk_cdn_dp_resolve_typec_dev(sc);
	if (dev == NULL)
		return (false);

	get_status = rk_cdn_dp_lookup_typec_status();
	if (get_status == NULL)
		return (false);

	return (get_status(dev, status) == 0 &&
	    status->state_valid);
}

static bool
rk_cdn_dp_get_altmode_status(struct rk_cdn_dp_softc *sc,
    struct rk3399_typec_dp_altmode_status *status)
{
	int (*get_status)(device_t, struct rk3399_typec_dp_altmode_status *);
	device_t dev;

	if (status == NULL)
		return (false);
	dev = rk_cdn_dp_resolve_typec_dev(sc);
	if (dev == NULL || !device_is_attached(dev))
		return (false);

	get_status = rk_cdn_dp_lookup_altmode_status();
	if (get_status == NULL)
		return (false);
	if (get_status(dev, status) != 0 || !status->valid)
		return (false);

	sc->dp_altmode_valid = status->valid ? 1 : 0;
	sc->dp_altmode_ready = status->dp_ready ? 1 : 0;
	sc->dp_altmode_usb_ss = status->usb_ss;
	sc->dp_altmode_pin_assignment = status->pin_assignment;
	sc->dp_altmode_status = status->dp_status;
	return (true);
}

static int
rk_cdn_dp_lookup_typec_status_cb(linker_file_t lf, void *arg)
{
	caddr_t sym;

	sym = linker_file_lookup_symbol(lf, "fusb302_get_typec_status", 0);
	if (sym == 0)
		return (0);

	*(caddr_t *)arg = sym;
	return (1);
}

static int
rk_cdn_dp_lookup_altmode_fusb302_cb(linker_file_t lf, void *arg)
{
	caddr_t sym;

	sym = linker_file_lookup_symbol(lf, "fusb302_get_dp_altmode_state", 0);
	if (sym == 0)
		return (0);
	*(caddr_t *)arg = sym;
	return (1);
}

static int
rk_cdn_dp_lookup_altmode_helper_cb(linker_file_t lf, void *arg)
{
	caddr_t sym;

	sym = linker_file_lookup_symbol(lf, "rk3399_typec_dp_altmode_get_state",
	    0);
	if (sym == 0)
		return (0);
	*(caddr_t *)arg = sym;
	return (1);
}

static int
(*rk_cdn_dp_lookup_typec_status(void))(device_t, struct fusb302_typec_status *)
{
	caddr_t sym;

	sym = 0;
	(void)linker_file_foreach(rk_cdn_dp_lookup_typec_status_cb, &sym);
	return ((int (*)(device_t, struct fusb302_typec_status *))sym);
}

static int
(*rk_cdn_dp_lookup_altmode_status(void))(device_t,
    struct rk3399_typec_dp_altmode_status *)
{
	caddr_t sym;

	/*
	 * Two-pass scan: prefer fusb302's live exporter over the static
	 * rk3399_typec_altmode_helper module's hardcoded defaults. Per-file
	 * "try fusb302 first then helper" doesn't work because whichever
	 * module is iterated first wins, and the helper is loaded earlier.
	 */
	sym = 0;
	(void)linker_file_foreach(rk_cdn_dp_lookup_altmode_fusb302_cb, &sym);
	if (sym == 0)
		(void)linker_file_foreach(
		    rk_cdn_dp_lookup_altmode_helper_cb, &sym);
	return ((int (*)(device_t, struct rk3399_typec_dp_altmode_status *))sym);
}

/*
 * Split enable sequence into discrete phases so we can bisect crashes/hangs
 * during USB-C DP bring-up.
 */
static int
rk_cdn_dp_enable_clocks(struct rk_cdn_dp_softc *sc)
{
	int error, i;

	for (i = 0; i < RK_CDN_DP_NCLKS; i++) {
		error = clk_enable(sc->clks[i]);
		if (error != 0) {
			device_printf(sc->dev, "cannot enable clock %s\n",
			    rk_cdn_dp_clk_names[i]);
			return (error);
		}
	}
	sc->clks_enabled = true;

	return (0);
}

static int
rk_cdn_dp_deassert_resets(struct rk_cdn_dp_softc *sc)
{
	int error;
	/*
	 * Assert then deassert
	 * CORE → DPTX → APB in that order.  SPDIF is not cycled.
	 */
	(void)hwreset_assert(sc->rsts[RK_CDN_DP_RST_CORE]);
	(void)hwreset_assert(sc->rsts[RK_CDN_DP_RST_DPTX]);
	(void)hwreset_assert(sc->rsts[RK_CDN_DP_RST_APB]);

	error = hwreset_deassert(sc->rsts[RK_CDN_DP_RST_CORE]);
	if (error != 0) {
		device_printf(sc->dev, "cannot deassert reset core\n");
		return (error);
	}
	error = hwreset_deassert(sc->rsts[RK_CDN_DP_RST_DPTX]);
	if (error != 0) {
		device_printf(sc->dev, "cannot deassert reset dptx\n");
		return (error);
	}
	error = hwreset_deassert(sc->rsts[RK_CDN_DP_RST_APB]);
	if (error != 0) {
		device_printf(sc->dev, "cannot deassert reset apb\n");
		return (error);
	}
	sc->rsts_deasserted = true;

	DELAY(20000);
	return (0);
}

static int
rk_cdn_dp_select_active_port(struct rk_cdn_dp_softc *sc)
{
	struct fusb302_typec_status typec;

	if (sc->nphys <= 0)
		return (ENXIO);

	/*
	 * Mirror the connected-port choice as closely as current FreeBSD
	 * scaffolding allows.
	 *
	 * RockPro64 is special: board docs and schematic show that USB-C video
	 * is always on TYPEC0/PHY0, while the other RK3399 Type-C PHY is wired
	 * out as the fixed USB3 A-path. Cable orientation therefore changes AUX
	 * polarity/lane flip on the same TYPEC0 port; it does not select PHY1.
	 */
	sc->active_port = 0;
	if (sc->active_port_override >= 0 &&
	    sc->active_port_override < sc->nphys) {
		sc->active_port = sc->active_port_override;
		device_printf(sc->dev,
		    "active-port override=%d nphys=%d extcon=%s\n",
		    sc->active_port, sc->nphys,
		    sc->extcon_dev != NULL ? "yes" : "no");
		return (0);
	}
	if (sc->rockpro64_typec0_only)
		return (0);

	if (rk_cdn_dp_get_typec_status(sc, &typec)) {
		if (!typec.attached)
			return (ENODEV);
		if (typec.orientation == FUSB302_TYPEC_ORIENT_CC2 &&
		    sc->nphys > 1)
			sc->active_port = 1;
	}

	return (0);
}

static int
rk_cdn_dp_do_enable_phys(struct rk_cdn_dp_softc *sc)
{
	int error, i;

	for (i = 0; i < sc->nphys; i++) {
		if (i != sc->active_port)
			continue;
		error = phy_set_mode(sc->phys[i], PHY_MODE_DP, PHY_SUBMODE_NA);
		if (error != 0) {
			device_printf(sc->dev,
			    "cannot set phy %d to DP mode\n", i);
			return (error);
		}
		error = phy_enable(sc->phys[i]);
		if (error != 0) {
			device_printf(sc->dev, "cannot enable phy %d\n", i);
			return (error);
		}
	}
	sc->phys_enabled = true;

	return (0);
}

/*
 * rk_cdn_dp_defer_enable
 *
 * Preserves the existing loader knob in case operators already set it, but
 * attach is now inert regardless.  The return value is only used for status
 * reporting so dmesg reflects the boot environment that the operator chose.
 */
static bool
rk_cdn_dp_defer_enable(void)
{
	return (rk_cdn_dp_tunable_flag("hw.rk_cdn_dp_defer_enable", 1));
}

/*
 * rk_cdn_dp_allow_phys
 *
 * Returns true only when the operator explicitly allows stage 5.  PHY lane
 * switching is one of the first places where board-specific instability can
 * wedge the machine, so keep it behind an opt-in gate even after the driver
 * has attached safely.
 */
static bool
rk_cdn_dp_allow_phys(struct rk_cdn_dp_softc *sc)
{
	return (sc->allow_phys);
}

/*
 * rk_cdn_dp_allow_aux
 *
 * Returns true only when the operator explicitly allows the firmware/mailbox
 * stages.  This remains a risky area, so keep it behind a second gate instead
 * of letting any later stage write reach the Cadence block automatically.
 */
static bool
rk_cdn_dp_allow_aux(struct rk_cdn_dp_softc *sc)
{
	return (sc->allow_aux);
}

/*
 * rk_cdn_dp_sysctl_flag
 *
 * Handles read/write boolean sysctls stored directly in the softc.  The same
 * helper backs allow_phys and allow_aux so operators can open those gates at
 * runtime without rebuilding the module.
 */
static int
rk_cdn_dp_sysctl_flag(SYSCTL_HANDLER_ARGS)
{
	bool *flag;
	int error, value;

	flag = arg1;
	value = *flag ? 1 : 0;
	error = sysctl_handle_int(oidp, &value, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	*flag = (value != 0);
	return (0);
}

/*
 * rk_cdn_dp_sysctl_reprobe
 *
 * Exposes the single-child reprobe path through a module-global sysctl so it
 * can be driven explicitly from userspace on a stable kernel.  This avoids
 * depending on boot-time bus hooks while still letting us retry binding the
 * existing `dp@fec00000` OFW child after the module is loaded.
 */
static int
rk_cdn_dp_sysctl_reprobe(SYSCTL_HANDLER_ARGS)
{
	int error, value;

	value = 0;
	error = sysctl_handle_int(oidp, &value, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (value != 1)
		return (EINVAL);

	error = rk_cdn_dp_rebind_child(true);
	rk_cdn_dp_rebind_last_error = error;
	return (error);
}

static int
rk_cdn_dp_sysctl_refresh_typec(SYSCTL_HANDLER_ARGS)
{
	struct rk_cdn_dp_softc *sc;
	int error, value;

	sc = arg1;
	value = 0;
	error = sysctl_handle_int(oidp, &value, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (value != 1)
		return (EINVAL);

	sx_slock(&sc->detach_sx);
	if (sc->detached) {
		sx_sunlock(&sc->detach_sx);
		return (ENXIO);
	}
	rk_cdn_dp_snapshot_typec_state(sc, "refresh_typec_now");
	sx_sunlock(&sc->detach_sx);
	return (0);
}

static int
rk_cdn_dp_sysctl_hostcap_lanes(SYSCTL_HANDLER_ARGS)
{
	int error, value;

	value = *(int *)arg1;
	error = sysctl_handle_int(oidp, &value, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (value != 0 && value != 2 && value != 4)
		return (EINVAL);
	*(int *)arg1 = value;
	return (0);
}

static int
rk_cdn_dp_sysctl_hostcap_flip(SYSCTL_HANDLER_ARGS)
{
	int error, value;

	value = *(int *)arg1;
	error = sysctl_handle_int(oidp, &value, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (value < -1 || value > 1)
		return (EINVAL);
	*(int *)arg1 = value;
	return (0);
}

static int
rk_cdn_dp_sysctl_hostcap_usb_ss(SYSCTL_HANDLER_ARGS)
{
	int error, value;

	value = *(int *)arg1;
	error = sysctl_handle_int(oidp, &value, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (value < -1 || value > 1)
		return (EINVAL);
	*(int *)arg1 = value;
	return (0);
}

static int
rk_cdn_dp_sysctl_active_port(SYSCTL_HANDLER_ARGS)
{
	int error, value, max_port;
	struct rk_cdn_dp_softc *sc;

	sc = arg1;
	value = sc->active_port_override;
	error = sysctl_handle_int(oidp, &value, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	max_port = sc->nphys > 0 ? sc->nphys - 1 : RK_CDN_DP_MAX_PHYS - 1;
	if (value < -1 || value > max_port)
		return (EINVAL);
	sc->active_port_override = value;
	return (0);
}

/*
 * rk_cdn_dp_rebind_child
 *
 * Finds the existing `dp@fec00000` OFW child beneath `ofwbus0` and optionally
 * reprobes just that child.  This is the safest currently available binding
 * path: it avoids broad bus rescans and only touches the single unattached
 * RK3399 CDN-DP node that already exists in the device tree.
 */
static int
rk_cdn_dp_rebind_child(bool reprobe)
{
	device_t *children, bus, child;
	devclass_t bus_dc, child_dc;
	phandle_t node;
	char path[128], status[32];
	ssize_t slen;
	int count, error, i;

	rk_cdn_dp_rebind_matches = 0;
	rk_cdn_dp_rebind_last_error = 0;
	children = NULL;
	error = ENXIO;

	bus_topo_lock();
	bus_dc = devclass_find("ofwbus");
	if (bus_dc == NULL) {
		error = ENXIO;
		goto out;
	}
	bus = devclass_get_device(bus_dc, 0);
	if (bus == NULL) {
		error = ENXIO;
		goto out;
	}
	if (device_get_children(bus, &children, &count) != 0) {
		error = ENXIO;
		goto out;
	}

	for (i = 0; i < count; i++) {
		child = children[i];
		node = ofw_bus_get_node(child);
		if (node <= 0)
			continue;
		if (!ofw_bus_node_is_compatible(node, "rockchip,rk3399-cdn-dp"))
			continue;
		path[0] = '\0';
		status[0] = '\0';
		OF_package_to_path(node, path, sizeof(path));
		slen = OF_getprop(node, "status", status, sizeof(status) - 1);
		if (slen > 0)
			status[slen] = '\0';
		else
			strcpy(status, "<absent>");
		printf("rk_cdn_dp: rebind match child=%s node=%s status=%s state=%d\n",
		    device_get_nameunit(child), path[0] != '\0' ? path : "<path?>",
		    status, device_get_state(child));
		rk_cdn_dp_rebind_matches++;
		if (!reprobe) {
			error = 0;
			continue;
		}
		if (device_get_state(child) != DS_NOTPRESENT) {
			error = EBUSY;
			continue;
		}
		if (!device_is_devclass_fixed(child)) {
			child_dc = device_get_devclass(child);
			if (child_dc != NULL &&
			    strcmp(devclass_get_name(child_dc), "unknown") == 0)
				(void)device_set_devclass(child, NULL);
		}
		error = device_probe_and_attach(child);
		if (error == 0) {
			rk_cdn_dp_rebind_last_error = 0;
			goto out;
		}
		rk_cdn_dp_rebind_last_error = error;
	}

	if (rk_cdn_dp_rebind_matches == 0)
		error = ENOENT;
out:
	if (children != NULL)
		free(children, M_TEMP);
	bus_topo_unlock();
	return (error);
}

/*
 * rk_cdn_dp_rebind_taskfn
 *
 * Runs one tick after module load so the standard DRIVER_MODULE() path has
 * already registered the driver with ofwbus.  Deferring the reprobe this way
 * avoids custom module registration glue while still keeping the rebinding
 * logic local to the DP module.
 */
static void
rk_cdn_dp_rebind_taskfn(void *context, int pending)
{
	rk_cdn_dp_rebind_attempts++;
	rk_cdn_dp_rebind_last_error = EOPNOTSUPP;
	printf("rk_cdn_dp: rebind task disabled during staged bring-up (pending=%d)\n",
	    pending);
}

/*
 * rk_cdn_dp_module_event
 *
 * Schedules or cancels the deferred single-child reprobe around normal module
 * lifecycle events.  The actual driver registration remains on the standard
 * DRIVER_MODULE() path so kldload registers a normal module entry and the
 * delayed callback only runs after registration has completed.
 */
static int
rk_cdn_dp_module_event(module_t mod, int what, void *arg)
{

	printf("rk_cdn_dp: module event what=%d\n", what);
	switch (what) {
	case MOD_LOAD:
		break;
	case MOD_QUIESCE:
	case MOD_UNLOAD:
		break;
	default:
		break;
	}

	return (0);
}

static int
rk_cdn_dp_set_stage(struct rk_cdn_dp_softc *sc, int target)
{
	int error, next;

	sc->last_error = 0;
	if (target < sc->stage)
		return (EINVAL);
	if (target > RK_CDN_DP_STAGE_VIDEO_ON)
		return (EINVAL);

	rk_cdn_dp_snapshot_typec_state(sc, NULL);

	for (next = sc->stage + 1; next <= target; next++) {
		if (next == RK_CDN_DP_STAGE_PHYS && !rk_cdn_dp_allow_phys(sc)) {
			device_printf(sc->dev,
			    "stage %d (%s) blocked; set dev.rk_cdn_dp.%d.allow_phys=1 first\n",
			    next, rk_cdn_dp_stage_name(next), device_get_unit(sc->dev));
			sc->last_error = EPERM;
			return (EPERM);
		}
		if (next >= RK_CDN_DP_STAGE_FW_GET && !rk_cdn_dp_allow_aux(sc)) {
			device_printf(sc->dev,
			    "stage %d (%s) blocked; set dev.rk_cdn_dp.%d.allow_aux=1 first\n",
			    next, rk_cdn_dp_stage_name(next), device_get_unit(sc->dev));
			sc->last_error = EPERM;
			return (EPERM);
		}
		switch (next) {
		case RK_CDN_DP_STAGE_POWER:
			if (sc->has_power_domain) {
				if (sc->power_dev == NULL) {
					RK_CDN_DP_DPRINTF(sc,
					    "power-domain provider not ready; load/enable rk3399_power first\n");
					sc->last_error = ENXIO;
					return (ENXIO);
				}
				if (rk_cdn_dp_power_domain_ready(sc)) {
					RK_CDN_DP_DPRINTF(sc,
					    "power-domain %u already on and bus-open; skipping provider enable\n",
					    sc->power_domain_id);
				} else {
					error = rk3399_power_enable_domain(
					    sc->power_dev, sc->power_domain_id);
					if (error != 0) {
						device_printf(sc->dev,
						    "cannot enable power-domain %u\n",
						    sc->power_domain_id);
						sc->last_error = error;
						return (error);
					}
				}
				DELAY(1000);
			}
			break;
		case RK_CDN_DP_STAGE_HANDLES:
			error = rk_cdn_dp_get_clocks(sc);
			if (error != 0)
				sc->last_error = error;
			if (error != 0)
				return (error);
			error = rk_cdn_dp_get_resets(sc);
			if (error != 0)
				sc->last_error = error;
			if (error != 0)
				return (error);
			error = rk_cdn_dp_get_phys(sc);
			if (error != 0)
				sc->last_error = error;
			if (error != 0)
				return (error);
			break;
		case RK_CDN_DP_STAGE_CLOCKS:
			error = rk_cdn_dp_enable_clocks(sc);
			if (error != 0)
				sc->last_error = error;
			if (error != 0)
				return (error);
			break;
		case RK_CDN_DP_STAGE_RESETS:
			error = rk_cdn_dp_deassert_resets(sc);
			if (error != 0)
				sc->last_error = error;
			if (error != 0)
				return (error);
			break;
		case RK_CDN_DP_STAGE_PHYS:
			/*
			 * Stage 5 just enables the CDN-DP SOURCE_PHY_CAR (which
			 * provides the TC-PHY external PSM clock source). Actual
			 * phy_enable happens at end of stage 9, after CDN-DP
			 * firmware is active and driving PIPE PowerDown correctly.
			 * This matches the cdn_dp_clk_enable → firmware_init
			 * → enable_phy ordering.
			 */
			rk_cdn_dp_clock_reset(sc);
			RK_CDN_DP_DPRINTF(sc,
			    "stage5: SOURCE_PHY_CAR=0x%08x SOURCE_DPTX_CAR=0x%08x\n",
			    rk_cdn_dp_read_4(sc, RK_CDN_DP_SOURCE_PHY_CAR),
			    rk_cdn_dp_read_4(sc, RK_CDN_DP_SOURCE_DPTX_CAR));
			error = rk_cdn_dp_select_active_port(sc);
			if (error != 0) {
				sc->last_error = error;
				return (error);
			}
			break;
		case RK_CDN_DP_STAGE_FW_GET:
			error = rk_cdn_dp_mailbox_get_firmware(sc);
			if (error != 0) {
				sc->last_error = error;
				device_printf(sc->dev, "firmware get failed (%d)\n",
				    error);
				return (error);
			}
			break;
		case RK_CDN_DP_STAGE_FW_PREP:
			error = rk_cdn_dp_mailbox_prepare_ucpu(sc);
			if (error != 0) {
				sc->last_error = error;
				device_printf(sc->dev, "firmware prep failed (%d)\n",
				    error);
				return (error);
			}
			break;
		case RK_CDN_DP_STAGE_FW_LOAD:
			error = rk_cdn_dp_mailbox_load_fw(sc);
			if (error != 0) {
				sc->last_error = error;
				device_printf(sc->dev, "firmware load failed (%d)\n",
				    error);
				return (error);
			}
			break;
		case RK_CDN_DP_STAGE_FW_ACTIVE:
			/* firmware_init = set_firmware_active + event_config */
			error = rk_cdn_dp_set_firmware_active(sc, true);
			if (error != 0) {
				sc->last_error = error;
				device_printf(sc->dev, "firmware activate failed (%d)\n",
				    error);
				return (error);
			}
			error = rk_cdn_dp_wait_keep_alive(sc,
			    &sc->mbox_last_keep_alive);
			if (error != 0) {
				sc->last_error = error;
				device_printf(sc->dev,
				    "firmware keep_alive failed (0x%08x, %d)\n",
				    sc->mbox_last_keep_alive, error);
				return (error);
			}
			error = rk_cdn_dp_event_config(sc);
			if (error != 0) {
				sc->last_error = error;
				device_printf(sc->dev,
				    "event_config failed (%d)\n", error);
				return (error);
			}
			/* enable_phy = phy_power_on then HPD_SEL (stage 10) */
			error = rk_cdn_dp_do_enable_phys(sc);
			if (error != 0) {
				sc->last_error = error;
				return (error);
			}
			break;
		case RK_CDN_DP_STAGE_HPD_SEL:
			error = rk_cdn_dp_select_hpd(sc);
			if (error != 0) {
				sc->last_error = error;
				RK_CDN_DP_DPRINTF(sc, "HPD select failed (%d)\n",
				    error);
				return (error);
			}
			break;
		case RK_CDN_DP_STAGE_HPD_STATE:
			error = rk_cdn_dp_mailbox_get_hpd_state(sc);
			if (error != 0) {
				sc->last_error = error;
				RK_CDN_DP_DPRINTF(sc, "HPD state failed (%d)\n",
				    error);
				return (error);
			}
			break;
		case RK_CDN_DP_STAGE_HOSTCAP:
			/* No-op: set_host_cap moved to stage 13. */
			break;
		case RK_CDN_DP_STAGE_DPCD_READ:
			/*
			 * set_host_cap is the last step of phy enable, immediately
			 * followed by cdn_dp_get_sink_capability (poll sink_count +
			 * read DPCD), all in one call chain.  Autonomous training
			 * starts when the firmware receives SET_HOST_CAPABILITIES
			 * with HPD already asserted.  Sending the DPCD read in the
			 * same sysctl call (no sleep) means it arrives before the
			 * firmware can lock the APB for training.
			 */
			error = rk_cdn_dp_mailbox_set_host_cap(sc);
			if (error != 0) {
				sc->last_error = error;
				device_printf(sc->dev, "set_host_cap failed (%d)\n",
				    error);
				return (error);
			}
			rk_cdn_dp_mailbox_log_state(sc, "after set_host_cap");
			error = rk_cdn_dp_mailbox_settle_post_hostcap(sc);
			if (error != 0) {
				sc->last_error = error;
				device_printf(sc->dev,
				    "post-hostcap settle failed (%d)\n", error);
				return (error);
			}
			rk_cdn_dp_mailbox_log_state(sc, "after post-hostcap settle");
			if (sc->skip_dpcd) {
				/*
				 * Bypass the AUX-channel DPCD read entirely.
				 * Synthesize a baseline sink_caps that any DP
				 * 1.1 sink supports: 2 lanes at 1.62 Gbps, no
				 * enhanced framing, no spread spectrum. Lets
				 * stage 14+ (link-plan / training / EDID) run
				 * so we can isolate whether AUX is fundamentally
				 * dead or just our DPCD-read mailbox demux is
				 * desynced.
				 */
				memset(sc->aux_dpcd, 0, sizeof(sc->aux_dpcd));
				sc->aux_dpcd[0] = 0x11;	/* DPCD rev 1.1 */
				sc->aux_dpcd[1] = 0x06;	/* MAX_LINK_RATE 1.62 Gbps */
				sc->aux_dpcd[2] = 0x02;	/* MAX_LANE_COUNT=2 */
				rk_cdn_dp_record_sink_caps(sc);
				RK_CDN_DP_DPRINTF(sc,
				    "stage13: skip_dpcd=1, synthesized "
				    "DPCD 1.1 / 1.62Gbps / 2-lane\n");
				break;
			}
			error = rk_cdn_dp_mailbox_probe_dpcd_caps(sc);
			if (error != 0) {
				device_printf(sc->dev,
				    "mailbox DPCD read failed (%d); falling back to synthesized sink caps\n",
				    error);
				memset(sc->aux_dpcd, 0, sizeof(sc->aux_dpcd));
				sc->aux_dpcd[0] = 0x11;	/* DPCD rev 1.1 */
				sc->aux_dpcd[1] = 0x06;	/* MAX_LINK_RATE 1.62 Gbps */
				sc->aux_dpcd[2] = 0x02;	/* MAX_LANE_COUNT=2 */
				rk_cdn_dp_record_sink_caps(sc);
				RK_CDN_DP_DPRINTF(sc,
				    "stage13: synthesized fallback "
				    "DPCD 1.1 / 1.62Gbps / 2-lane\n");
				break;
			}
			break;
		case RK_CDN_DP_STAGE_LINK_PLAN:
			error = rk_cdn_dp_plan_link(sc);
			if (error != 0) {
				sc->last_error = error;
				device_printf(sc->dev, "link-plan failed (%d)\n",
				    error);
				return (error);
			}
			break;
		case RK_CDN_DP_STAGE_LINK_TRAIN_START:
			RK_CDN_DP_DPRINTF(sc,
			    "stage15: entering link-train-start\n");
			error = rk_cdn_dp_start_link_training(sc);
			if (error != 0) {
				sc->last_error = error;
				RK_CDN_DP_DPRINTF(sc,
				    "link-train-start failed (%d)\n", error);
				return (error);
			}
			RK_CDN_DP_DPRINTF(sc,
			    "stage15: link-train-start done\n");
			break;
		case RK_CDN_DP_STAGE_LINK_TRAIN_FULL:
			RK_CDN_DP_DPRINTF(sc,
			    "stage16: entering link-train-full\n");
			error = rk_cdn_dp_link_train(sc);
			if (error != 0) {
				sc->last_error = error;
				RK_CDN_DP_DPRINTF(sc,
				    "link-train-full failed (%d)\n", error);
				return (error);
			}
			RK_CDN_DP_DPRINTF(sc,
			    "stage16: link-train-full done\n");
			break;
		case RK_CDN_DP_STAGE_EDID:
			error = rk_cdn_dp_read_edid(sc);
			if (error != 0) {
				sc->last_error = error;
				device_printf(sc->dev,
				    "edid failed (%d)\n", error);
				return (error);
			}
			break;
		case RK_CDN_DP_STAGE_CONFIG_VIDEO:
			RK_CDN_DP_DPRINTF(sc,
			    "stage18: entering config-video\n");
			error = rk_cdn_dp_config_video(sc);
			if (error != 0) {
				sc->last_error = error;
				device_printf(sc->dev,
				    "config-video failed (%d)\n", error);
				return (error);
			}
			RK_CDN_DP_DPRINTF(sc,
			    "stage18: config-video done\n");
			break;
		case RK_CDN_DP_STAGE_VIDEO_ON:
			if (!sc->fw_link_training_used) {
				RK_CDN_DP_DPRINTF(sc,
				    "stage19: software-trained link; no firmware video arm needed\n");
				break;
			}
			RK_CDN_DP_DPRINTF(sc,
			    "stage19: entering video-idle arm\n");
			error = rk_cdn_dp_set_video_status(sc, false);
			if (error != 0) {
				sc->last_error = error;
				RK_CDN_DP_DPRINTF(sc,
				    "video-idle arm failed (%d)\n", error);
				return (error);
			}
			RK_CDN_DP_DPRINTF(sc,
			    "stage19: video-idle armed; awaiting VOP live pixels\n");
			break;
		default:
			return (EINVAL);
		}

		sc->stage = next;
	}

	return (0);
}

/*
 * rk_cdn_dp_sysctl_stage
 *
 * Exposes the monotonic stage controller through sysctl.  Reads report the
 * current stage, and writes advance the driver one step at a time subject to
 * the allow_phys/allow_aux gates.
 */
static int
rk_cdn_dp_sysctl_stage(SYSCTL_HANDLER_ARGS)
{
	struct rk_cdn_dp_softc *sc;
	int error, stage;

	sc = arg1;

	/*
	 * Hold detach_sx shared across the whole handler. If detach starts
	 * in another thread it'll block on the exclusive acquire until we
	 * unlock. If detach already finished setting `detached=true`, bail
	 * with ENXIO before touching MMIO — sysctl_root may still call us
	 * briefly even after sysctl_ctx_free has been called from detach.
	 */
	sx_slock(&sc->detach_sx);
	if (sc->detached) {
		sx_sunlock(&sc->detach_sx);
		return (ENXIO);
	}

	stage = sc->stage;
	error = sysctl_handle_int(oidp, &stage, 0, req);
	if (error == 0 && req->newptr != NULL)
		error = rk_cdn_dp_set_stage(sc, stage);

	sx_sunlock(&sc->detach_sx);
	return (error);
}


/*
 * Sysctl write-1 handler that rewinds stage→ATTACHED and clears runtime
 * state, so the operator can re-walk stages 1..14 with new tunables
 * without kldunload (which is unsafe per project memory — leaves PHY
 * and firmware in inconsistent state).  Caller must re-set
 * skip_aux_swap / hostcap_* / allow_phys / allow_aux after reset.
 */
static int
rk_cdn_dp_sysctl_reset_state(SYSCTL_HANDLER_ARGS)
{
	struct rk_cdn_dp_softc *sc;
	int error, val;

	sc = arg1;
	val = 0;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL || val != 1)
		return (error);

	sx_slock(&sc->detach_sx);
	if (sc->detached) {
		sx_sunlock(&sc->detach_sx);
		return (ENXIO);
	}
	rk_cdn_dp_reset_runtime_state(sc);
	RK_CDN_DP_DPRINTF(sc,
	    "reset_state: stage rewound to ATTACHED, tunables cleared\n");
	sx_sunlock(&sc->detach_sx);
	return (0);
}

static void
rk_cdn_dp_reset_runtime_state(struct rk_cdn_dp_softc *sc)
{
	sc->stage = RK_CDN_DP_STAGE_ATTACHED;
	sc->last_error = 0;
	sc->active_port = -1;
	sc->allow_phys = rk_cdn_dp_tunable_flag("hw.rk_cdn_dp_allow_phys", 0);
	sc->allow_aux = rk_cdn_dp_tunable_flag("hw.rk_cdn_dp_allow_aux", 0);
	sc->hpd_status = -1;
	sc->active_port_override = -1;
	sc->hostcap_lanes_override = 0;
	sc->hostcap_flip_override = -1;
	sc->hostcap_usb_ss_override = -1;
	/*
	 * Default: skip the AUX_SWAP_INVERSION_CONTROL write on RockPro64
	 * boards where the orientation-aware path is operator-controlled
	 * via skip_aux_swap=0 + the flip-derived value computed in
	 * rk_cdn_dp_set_host_cap.  Without the write, AUX engine stays at
	 * firmware default; on RockPro64 that's the working CC1 state.
	 */
	sc->skip_aux_swap = rk_cdn_dp_is_rockpro64(sc->dev) ? 1 : 0;
	sc->aux_swap_value = RK_CDN_DP_AUX_HOST_INVERT;
	sc->debug_reg_addr = 0;
	sc->debug_reg_value = 0;
	sc->dp_altmode_valid = 0;
	sc->dp_altmode_ready = 0;
	sc->dp_altmode_usb_ss = -1;
	sc->dp_altmode_pin_assignment = 0;
	sc->dp_altmode_status = 0;
	sc->display_power_state = 1;	/* boot leaves sink in D0 */
	sc->backlight_state = 1;	/* boot leaves backlight on */
	sc->aux_trace_reads = false;
	sc->aux_last_read_off = 0;
	sc->aux_last_read_val = 0;
	sc->aux_last_write_off = 0;
	sc->aux_last_write_val = 0;
	sc->mbox_bad_header_count = 0;
	sc->mbox_last_header = 0;
	sc->mbox_last_expect = 0;
	sc->mbox_last_body0_3 = 0;
	sc->mbox_last_body4 = 0;
	sc->mbox_last_empty = 0;
	sc->mbox_last_full = 0;
	sc->mbox_last_empty_after_send = 0;
	sc->mbox_last_events0 = 0;
	sc->mbox_last_keep_alive = 0;
	sc->mbox_last_apb_int_mask = 0;
	sc->mbox_last_send_header = 0;
	sc->mbox_last_send_size = 0;
	sc->mbox_last_send_written = 0;
	sc->mbox_last_write_full_first = 0;
	sc->mbox_last_write_full_last = 0;
	sc->mbox_last_write_full_polls = 0;
	sc->aux_prepared = false;
	sc->aux_pending_cmd = 0;
	sc->aux_pending_addr = 0;
	sc->aux_pending_len = 0;
	sc->sink_caps_valid = false;
	sc->sink_dpcd_rev = 0;
	sc->sink_max_link_rate_code = 0;
	sc->sink_max_lane_count = 0;
	sc->sink_max_link_rate_khz = 0;
	sc->link_plan_rate_code = 0;
	sc->link_plan_lanes = 0;
	sc->expected_link_bw_set = 0;
	sc->expected_lane_count_set = 0;
	memset(sc->link_status, 0, sizeof(sc->link_status));
	sc->link_trained = false;
	sc->fw_link_training_used = false;
	sc->video_configured = false;
	sc->fw_active = false;
	sc->fw_version = 0;
}

/*
 * rk_cdn_dp_attach
 *
 * Top-level attach.  The safe-mode attach path is intentionally inert:
 *
 *   1. Allocate MMIO and IRQ resources (no dangerous register programming).
 *   2. Resolve optional provider handles (power-domain and extcon).
 *   3. Register sysctls that let an operator advance one stage at a time.
 *   4. Stop at stage 0 until the operator explicitly opts into later stages.
 *
 * The driver is currently loaded as a module (not built into the kernel) so
 * that a panic during bring-up does not prevent the board from booting.
 */
static int
rk_cdn_dp_attach(device_t dev)
{
	struct rk_cdn_dp_softc *sc;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree;
	int error;
	bool defer;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->aux_iicbus = NULL;
	sc->aux_iic_adapter = NULL;
	sc->node = ofw_bus_get_node(dev);
	sc->rockpro64_typec0_only = rk_cdn_dp_is_rockpro64(dev);
	sc->has_extcon = OF_hasprop(sc->node, "extcon");
	sc->extcon_dev = NULL;
	sc->detached = false;
	sx_init(&sc->detach_sx, "rk_cdn_dp detach");
	rk_cdn_dp_reset_runtime_state(sc);
	sc->nphys = 0;
	sc->clks_enabled = false;
	sc->rsts_deasserted = false;
	sc->phys_enabled = false;
	sc->has_power_domain = false;
	sc->power_dev = NULL;
	sc->pmu_syscon = NULL;
	sc->grf = NULL;
	{
		int i;
		for (i = 0; i < RK_CDN_DP_NCLKS; i++)
			sc->clks[i] = NULL;
		for (i = 0; i < RK_CDN_DP_NRSTS; i++)
			sc->rsts[i] = NULL;
		for (i = 0; i < RK_CDN_DP_MAX_PHYS; i++)
			sc->phys[i] = NULL;
	}

	error = bus_alloc_resources(dev, rk_cdn_dp_spec, sc->res);
	if (error != 0) {
		device_printf(dev, "cannot allocate resources\n");
		return (ENXIO);
	}
	RK_CDN_DP_DPRINTF(sc, "attach: resources allocated\n");

	error = rk_cdn_dp_get_power_domain(sc);
	if (error != 0 && error != ENXIO) {
		RK_CDN_DP_DPRINTF(sc, "attach: power-domain lookup failed (%d)\n",
		    error);
		goto fail;
	}
	if (error == ENXIO) {
		/* Provider may attach later; staging keeps this safe. */
		RK_CDN_DP_DPRINTF(sc,
		    "attach: power-domain provider not ready yet; deferring provider use\n");
		sc->power_dev = NULL;
		error = 0;
	}
	RK_CDN_DP_DPRINTF(sc,
	    "attach: power-domain state has_prop=%s provider=%s id=%u\n",
	    sc->has_power_domain ? "yes" : "no",
	    sc->power_dev != NULL ? "yes" : "no", sc->power_domain_id);
	error = rk_cdn_dp_get_extcon(sc);
	if (error != 0 && error != ENOENT) {
		device_printf(dev, "extcon provider state unavailable (%d)\n",
		    error);
		error = 0;
	}
	RK_CDN_DP_DPRINTF(sc, "attach: extcon dt=%s provider=%s\n",
	    sc->has_extcon ? "yes" : "no",
	    sc->extcon_dev != NULL ? device_get_nameunit(sc->extcon_dev) : "none");
	if (OF_hasprop(sc->node, "rockchip,grf") &&
	    syscon_get_by_ofw_property(dev, sc->node, "rockchip,grf",
	    &sc->grf) != 0) {
		device_printf(dev, "cannot get rockchip,grf handle\n");
		error = ENXIO;
		goto fail;
	}
	RK_CDN_DP_DPRINTF(sc, "attach: grf=%s\n", sc->grf != NULL ? "yes" : "no");

	ctx = device_get_sysctl_ctx(dev);
	tree = device_get_sysctl_tree(dev);
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "debug", CTLFLAG_RW, &sc->debug, 0,
	    "Enable verbose trace prints (mailbox / AUX / stage walk / "
	    "audio path). 0=off (default), 1=on");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "stage", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    sc, 0, rk_cdn_dp_sysctl_stage, "I",
	    "Bring-up stage (monotonic). 0=attached 1=power 2=handles 3=clocks 4=resets 5=phys 6=fw-get 7=fw-prep 8=fw-load 9=fw-active 10=hpd-sel 11=hpd-state 12=host-cap 13=dpcd-read 14=link-plan 15=link-train-start");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "reset_state", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    sc, 0, rk_cdn_dp_sysctl_reset_state, "I",
	    "Write 1 to rewind stage to ATTACHED + clear runtime state (lets you re-walk stages with new tunables; safer than kldunload)");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "allow_phys", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    &sc->allow_phys, 0, rk_cdn_dp_sysctl_flag, "I",
	    "Allow manual entry into stage 5 (PHY mode switch and enable)");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "allow_aux", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    &sc->allow_aux, 0, rk_cdn_dp_sysctl_flag, "I",
	    "Allow manual entry into stages 6-13 (firmware/mailbox bring-up)");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "last_error", CTLFLAG_RD, &sc->last_error, 0,
	    "Last non-zero error returned by a staged bring-up step");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "hpd_status", CTLFLAG_RD, &sc->hpd_status, 0,
	    "Last mailbox HPD status");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "active_port", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    sc, 0, rk_cdn_dp_sysctl_active_port, "I",
	    "Active TYPE-C DP port override: -1=auto, otherwise 0..nphys-1");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "refresh_typec_now", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    sc, 0, rk_cdn_dp_sysctl_refresh_typec, "I",
	    "Write 1 to resnapshot Type-C / DP Alt Mode state from fusb302");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "hostcap_lanes", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    &sc->hostcap_lanes_override, 0, rk_cdn_dp_sysctl_hostcap_lanes,
	    "I", "Host-cap lanes override: 0=auto, 2 or 4 explicit");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "hostcap_flip", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    &sc->hostcap_flip_override, 0, rk_cdn_dp_sysctl_hostcap_flip,
	    "I", "Host-cap flip override: -1=auto, 0=normal, 1=flipped");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "hostcap_usb_ss", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    &sc->hostcap_usb_ss_override, 0, rk_cdn_dp_sysctl_hostcap_usb_ss,
	    "I", "USB_SS state: -1=auto, 0=DP-only(4 lanes), 1=USB3+DP(2 lanes)");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "skip_aux_swap", CTLFLAG_RW, &sc->skip_aux_swap, 0,
	    "Skip WRITE_REGISTER(AUX_SWAP_INVERSION_CONTROL) in host-cap (debug)");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "skip_dpcd", CTLFLAG_RW, &sc->skip_dpcd, 0,
	    "1=bypass stage 13 DPCD read; synthesize 2-lane / 1.62Gbps DP 1.1 sink_caps");
	SYSCTL_ADD_U32(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "aux_swap_value", CTLFLAG_RW, &sc->aux_swap_value, 0,
	    "Value for AUX_SWAP_INVERSION_CONTROL (0=none,1=invert,3=invert+init)");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "aux_status", CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    sc, 0, rk_cdn_dp_sysctl_aux_status, "I",
	    "Read DPTX_GET_LAST_AUX_STATUS via mailbox (0=ACK 1=NACK 2=DEFER 4=I2C_NACK 8=I2C_DEFER)");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "aux_dump_now", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    sc, 0, rk_cdn_dp_sysctl_aux_dump, "I",
	    "Write 1 to dump AUX engine MMIO (0x780-0x7A4 + 0x280c) via firmware READ_REGISTER mailbox");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "edid_now", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    sc, 0, rk_cdn_dp_sysctl_edid_now, "I",
	    "Write 1 to read EDID block 0 via DPTX_GET_EDID mailbox without going through the stage walker");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "hpd_pulse_now", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    sc, 0, rk_cdn_dp_sysctl_hpd_pulse, "I",
	    "Write 1 to drive HPD low for 500ms then high, forcing sink DP RX re-detect");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "aux_wake_now", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    sc, 0, rk_cdn_dp_sysctl_aux_wake, "I",
	    "Write 1 to attempt DPCD SET_POWER D3 -> D0 to wake the sink");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "aux_probe_now", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    sc, 0, rk_cdn_dp_sysctl_aux_probe, "I",
	    "Write 1: WRITE_DPCD 0x600<-D0 + aux_status + READ_DPCD 0x000 (split diagnostic)");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "framer_dump_now", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    sc, 0, rk_cdn_dp_sysctl_framer_dump, "I",
	    "Write 1 to dump Cadence framer/MSA registers via READ_REGISTER mailbox");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "probe_warm", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    sc, 0, rk_cdn_dp_sysctl_probe_warm, "I",
	    "Write 1 to non-destructively enable cdn-dp clocks for MMIO readability "
	    "(skips resets/fw — preserves U-Boot firmware state)");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "retrain_now", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    sc, 0, rk_cdn_dp_sysctl_retrain_now, "I",
	    "Write 1 to re-run DP link training (CR+EQ) without resetting fw/framer; "
	    "use when sink signals LINK_STATUS_UPDATED / HPD_IRQ");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "audio_start_now", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    sc, 0, rk_cdn_dp_sysctl_audio_start_now, "I",
	    "Write 1 to configure + unmute DP audio (I2S2, 2ch 48kHz 16bit)");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "audio_stop_now", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    sc, 0, rk_cdn_dp_sysctl_audio_stop_now, "I",
	    "Write 1 to mute + reset DP audio packetizer");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "audio_dump", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    sc, 0, rk_cdn_dp_sysctl_audio_dump, "I",
	    "Write 1 to print AUDIO_PACK_CONTROL / DP_VB_ID / audio sub-block / SDP PIF state");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "edid_audio_dump", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    sc, 0, rk_cdn_dp_sysctl_edid_audio_dump, "I",
	    "Write 1 to dump CTA-861 audio capabilities from cached EDID");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "display_power", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    sc, 0, rk_cdn_dp_sysctl_display_power, "I",
	    "DPMS-style on/off: write 1 = DPCD SET_POWER D0 + auto retrain, "
	    "write 0 = DPCD SET_POWER D3 (sleep)");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "backlight_power", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    sc, 0, rk_cdn_dp_sysctl_backlight_power, "I",
	    "AUX-channel backlight on/off via DPCD 0x720 BL_ENABLE bit "
	    "(eDP 1.4+ spec; sink may NAK if unsupported)");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "dpcd_write_now", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    sc, 0, rk_cdn_dp_sysctl_dpcd_write_now, "I",
	    "Write debug_reg_value (low byte) to DPCD addr debug_reg_addr "
	    "via AUX.  For poking arbitrary DPCD addresses including "
	    "eDP backlight ranges (0x701, 0x720-0x72f) and vendor ranges.");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "dpcd_read_now", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    sc, 0, rk_cdn_dp_sysctl_dpcd_read_now, "I",
	    "Read DPCD addr debug_reg_addr via AUX, store result in "
	    "debug_reg_value (low byte).");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "debug_reg_addr", CTLTYPE_U32 | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    sc, 0, rk_cdn_dp_sysctl_reg_addr, "IU",
	    "Cadence mailbox register address for debug read/write");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "debug_reg_value", CTLTYPE_U32 | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    sc, 0, rk_cdn_dp_sysctl_reg_value, "IU",
	    "Cadence mailbox register value for debug read/write");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "debug_reg_read_now", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    sc, 0, rk_cdn_dp_sysctl_reg_read, "I",
	    "Write 1 to read debug_reg_addr via READ_REGISTER mailbox into debug_reg_value");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "debug_reg_write_now", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    sc, 0, rk_cdn_dp_sysctl_reg_write, "I",
	    "Write 1 to write debug_reg_value to debug_reg_addr via WRITE_REGISTER mailbox");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "dp_altmode_valid", CTLFLAG_RD, &sc->dp_altmode_valid, 0,
	    "Last observed DP Alt Mode helper presence");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "dp_altmode_ready", CTLFLAG_RD, &sc->dp_altmode_ready, 0,
	    "Last observed DP Alt Mode ready state");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "dp_altmode_usb_ss", CTLFLAG_RD, &sc->dp_altmode_usb_ss, 0,
	    "Last observed DP Alt Mode USB_SS state");
	SYSCTL_ADD_U32(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "dp_altmode_pin_assignment", CTLFLAG_RD,
	    &sc->dp_altmode_pin_assignment, 0,
	    "Last observed DP Alt Mode pin assignment");
	SYSCTL_ADD_U32(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "dp_altmode_status", CTLFLAG_RD, &sc->dp_altmode_status, 0,
	    "Last observed DP Alt Mode status value");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "sink_caps_valid", CTLFLAG_RD, (int *)&sc->sink_caps_valid, 0,
	    "1 if DPCD receiver caps were captured successfully");
	SYSCTL_ADD_U32(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "sink_max_link_rate_khz", CTLFLAG_RD, &sc->sink_max_link_rate_khz, 0,
	    "Parsed sink maximum link rate in kHz from DPCD");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "sink_max_lane_count", CTLFLAG_RD, (int *)&sc->sink_max_lane_count, 0,
	    "Parsed sink maximum lane count from DPCD");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "link_plan_lanes", CTLFLAG_RD, (int *)&sc->link_plan_lanes, 0,
	    "Planned lane count for the first link-training attempt");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "link_plan_rate_code", CTLFLAG_RD, (int *)&sc->link_plan_rate_code, 0,
	    "Planned link-rate code for the first link-training attempt");
	SYSCTL_ADD_ULONG(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "aux_last_read_off", CTLFLAG_RD, &sc->aux_last_read_off,
	    "Last stage-6 MMIO read offset");
	SYSCTL_ADD_U32(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "aux_last_read_val", CTLFLAG_RD, &sc->aux_last_read_val, 0,
	    "Last stage-6 MMIO read value");
	SYSCTL_ADD_ULONG(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "aux_last_write_off", CTLFLAG_RD, &sc->aux_last_write_off,
	    "Last stage-6 MMIO write offset");
	SYSCTL_ADD_U32(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "aux_last_write_val", CTLFLAG_RD, &sc->aux_last_write_val, 0,
	    "Last stage-6 MMIO write value");
	SYSCTL_ADD_U32(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "mbox_bad_header_count", CTLFLAG_RD, &sc->mbox_bad_header_count, 0,
	    "Count of unexpected Cadence mailbox headers");
	SYSCTL_ADD_U32(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "mbox_last_header", CTLFLAG_RD, &sc->mbox_last_header, 0,
	    "Last unexpected mailbox header bytes as 0xAABBCCDD");
	SYSCTL_ADD_U32(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "mbox_last_expect", CTLFLAG_RD, &sc->mbox_last_expect, 0,
	    "Expected mailbox header as 0xAABBCCCC (opcode,module,size)");
	SYSCTL_ADD_U32(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "mbox_last_body0_3", CTLFLAG_RD, &sc->mbox_last_body0_3, 0,
	    "First four bytes drained from the last unexpected mailbox payload");
	SYSCTL_ADD_U32(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "mbox_last_body4", CTLFLAG_RD, &sc->mbox_last_body4, 0,
	    "Fifth byte drained from the last unexpected mailbox payload");
	SYSCTL_ADD_U32(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "mbox_last_empty", CTLFLAG_RD, &sc->mbox_last_empty, 0,
	    "MAILBOX_EMPTY_ADDR sampled before READ_DPCD");
	SYSCTL_ADD_U32(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "mbox_last_full", CTLFLAG_RD, &sc->mbox_last_full, 0,
	    "MAILBOX_FULL_ADDR sampled before READ_DPCD");
	SYSCTL_ADD_U32(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "mbox_last_empty_after_send", CTLFLAG_RD,
	    &sc->mbox_last_empty_after_send, 0,
	    "MAILBOX_EMPTY_ADDR sampled immediately after mailbox_send");
	SYSCTL_ADD_U32(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "mbox_last_events0", CTLFLAG_RD, &sc->mbox_last_events0, 0,
	    "SW_EVENTS0 sampled before READ_DPCD");
	SYSCTL_ADD_U32(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "mbox_last_keep_alive", CTLFLAG_RD, &sc->mbox_last_keep_alive, 0,
	    "KEEP_ALIVE sampled before READ_DPCD");
	SYSCTL_ADD_U32(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "mbox_last_apb_int_mask", CTLFLAG_RD, &sc->mbox_last_apb_int_mask, 0,
	    "APB_INT_MASK sampled before READ_DPCD");
	SYSCTL_ADD_U32(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "mbox_last_send_header", CTLFLAG_RD, &sc->mbox_last_send_header, 0,
	    "Last mailbox send header as 0xAABBCCDD");
	SYSCTL_ADD_U32(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "mbox_last_send_size", CTLFLAG_RD, &sc->mbox_last_send_size, 0,
	    "Last mailbox send total byte count");
	SYSCTL_ADD_U32(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "mbox_last_send_written", CTLFLAG_RD, &sc->mbox_last_send_written, 0,
	    "Last mailbox send successfully written byte count");
	SYSCTL_ADD_U32(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "mbox_last_write_full_first", CTLFLAG_RD,
	    &sc->mbox_last_write_full_first, 0,
	    "First MAILBOX_FULL_ADDR sample seen by mailbox_write");
	SYSCTL_ADD_U32(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "mbox_last_write_full_last", CTLFLAG_RD,
	    &sc->mbox_last_write_full_last, 0,
	    "Last MAILBOX_FULL_ADDR sample seen by mailbox_write");
	SYSCTL_ADD_U32(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "mbox_last_write_full_polls", CTLFLAG_RD,
	    &sc->mbox_last_write_full_polls, 0,
	    "MAILBOX_FULL_ADDR poll count in last mailbox_write");

	device_printf(dev, "Cadence DP scaffold attached: extcon=%s irq=%s\n",
	    sc->has_extcon ? "yes" : "no",
	    sc->res[RK_CDN_DP_RES_IRQ] != NULL ? "present" : "absent");
	if (sc->rockpro64_typec0_only)
		RK_CDN_DP_DPRINTF(sc,
		    "board recipe: RockPro64 USB-C DP is fixed to TYPEC0/PHY0\n");
	if (sc->extcon_dev != NULL)
		device_printf(dev, "Type-C extcon provider: %s\n",
		    device_get_nameunit(sc->extcon_dev));
	rk_cdn_dp_snapshot_typec_state(sc, "attach typec snapshot");

	defer = rk_cdn_dp_defer_enable();
	RK_CDN_DP_DPRINTF(sc,
	    "bring-up deferred=%s allow_phys=%s allow_aux=%s\n",
	    defer ? "yes" : "no",
	    sc->allow_phys ? "yes" : "no",
	    sc->allow_aux ? "yes" : "no");
	if (!defer)
		device_printf(dev,
		    "automatic bring-up is intentionally disabled; use dev.rk_cdn_dp.%d.stage manually\n",
		    device_get_unit(dev));

	error = iic_dp_aux_add_bus(dev, "rk_cdn_dp_aux",
	    rk_cdn_dp_aux_i2c_xfer, sc, &sc->aux_iicbus, &sc->aux_iic_adapter);
	if (error != 0) {
		RK_CDN_DP_DPRINTF(sc,
		    "AUX-backed iicbus unavailable (%d); continuing without it\n",
		    -error);
		sc->aux_iicbus = NULL;
		sc->aux_iic_adapter = NULL;
	} else {
		RK_CDN_DP_DPRINTF(sc, "AUX-backed iicbus registered: %s\n",
		    sc->aux_iic_adapter != NULL ?
		    device_get_nameunit(sc->aux_iic_adapter) : "unknown");
	}

	return (0);

fail:
	rk_cdn_dp_release(dev);
	return (ENXIO);
}

/*
 * rk_cdn_dp_release
 *
 * Tears down all resources acquired during attach in strict reverse order:
 * PHYs disabled and released before resets are re-asserted, resets before
 * clocks are disabled, so that no block loses its clock while still active.
 * Boolean flags (phys_enabled, rsts_deasserted, clks_enabled) ensure each
 * step is only attempted if the corresponding acquire succeeded, making this
 * safe to call from any point in a partial attach failure.
 */
static void
rk_cdn_dp_release(device_t dev)
{
	struct rk_cdn_dp_softc *sc;
	int i;

	sc = device_get_softc(dev);
	sc->detached = true;

	if (sc->aux_iicbus != NULL) {
		(void)device_delete_child(dev, sc->aux_iicbus);
		sc->aux_iicbus = NULL;
		sc->aux_iic_adapter = NULL;
	}

	if (sc->phys_enabled) {
		for (i = 0; i < sc->nphys; i++)
			(void)phy_disable(sc->phys[i]);
		sc->phys_enabled = false;
	}

	for (i = 0; i < sc->nphys; i++) {
		if (sc->phys[i] != NULL) {
			phy_release(sc->phys[i]);
			sc->phys[i] = NULL;
		}
	}
	sc->nphys = 0;

	if (sc->rsts_deasserted) {
		for (i = RK_CDN_DP_NRSTS - 1; i >= 0; i--)
			(void)hwreset_assert(sc->rsts[i]);
		sc->rsts_deasserted = false;
	}
	for (i = 0; i < RK_CDN_DP_NRSTS; i++) {
		if (sc->rsts[i] != NULL) {
			hwreset_release(sc->rsts[i]);
			sc->rsts[i] = NULL;
		}
	}

	if (sc->clks_enabled) {
		for (i = RK_CDN_DP_NCLKS - 1; i >= 0; i--)
			(void)clk_disable(sc->clks[i]);
		sc->clks_enabled = false;
	}
	for (i = 0; i < RK_CDN_DP_NCLKS; i++) {
		if (sc->clks[i] != NULL) {
			clk_release(sc->clks[i]);
			sc->clks[i] = NULL;
		}
	}

	if (sc->fw != NULL) {
		firmware_put(sc->fw, FIRMWARE_UNLOAD);
		sc->fw = NULL;
	}
	sc->fw_active = false;
	sc->fw_version = 0;

	bus_release_resources(dev, rk_cdn_dp_spec, sc->res);
}

/*
 * rk_cdn_dp_detach
 *
 * Module detach entry point.  Delegates entirely to rk_cdn_dp_release so
 * that the teardown logic lives in one place and detach cannot diverge from
 * the partial-fail cleanup path in attach.
 */
static int
rk_cdn_dp_detach(device_t dev)
{
	struct rk_cdn_dp_softc *sc;

	sc = device_get_softc(dev);

	/*
	 * Take detach_sx exclusive: blocks until every sysctl handler that
	 * holds the shared lock has dropped it, then prevents any new
	 * handler from entering the work path (they see `detached=true`
	 * after acquiring the slock and bail with ENXIO). After this point
	 * it is safe to release MMIO resources without races.
	 */
	sx_xlock(&sc->detach_sx);
	sc->detached = true;
	sx_xunlock(&sc->detach_sx);

	rk_cdn_dp_release(dev);
	sx_destroy(&sc->detach_sx);
	return (0);
}

static device_method_t rk_cdn_dp_methods[] = {
	DEVMETHOD(device_probe,		rk_cdn_dp_probe),
	DEVMETHOD(device_attach,	rk_cdn_dp_attach),
	DEVMETHOD(device_detach,	rk_cdn_dp_detach),

	DEVMETHOD_END
};

static driver_t rk_cdn_dp_driver = {
	"rk_cdn_dp",
	rk_cdn_dp_methods,
	sizeof(struct rk_cdn_dp_softc),
};

DRIVER_MODULE(rk_cdn_dp, ofwbus, rk_cdn_dp_driver, rk_cdn_dp_module_event,
    NULL);
MODULE_VERSION(rk_cdn_dp, 1);
/*
 * Avoid MODULE_DEPEND() on in-kernel subsystems (clk/hwreset/phy/ofwbus) here.
 * On our target images these are built into the kernel (no clk.ko/hwreset.ko/
 * phy.ko), and declaring a dependency prevents kldload from working.
 */
