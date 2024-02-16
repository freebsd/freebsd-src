/*-
 * SPDX-License-Identifier: GPL-2.0 or Linux-OpenIB
 *
 * Copyright (c) 2017 - 2023 Intel Corporation
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenFabrics.org BSD license below:
 *
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following
 *   conditions are met:
 *
 *    - Redistributions of source code must retain the above
 *	copyright notice, this list of conditions and the following
 *	disclaimer.
 *
 *    - Redistributions in binary form must reproduce the above
 *	copyright notice, this list of conditions and the following
 *	disclaimer in the documentation and/or other materials
 *	provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "osdep.h"
#include "irdma_type.h"
#include "icrdma_hw.h"

void disable_prefetch(struct irdma_hw *hw);

void disable_tx_spad(struct irdma_hw *hw);

void rdpu_ackreqpmthresh(struct irdma_hw *hw);

static u32 icrdma_regs[IRDMA_MAX_REGS] = {
	PFPE_CQPTAIL,
	    PFPE_CQPDB,
	    PFPE_CCQPSTATUS,
	    PFPE_CCQPHIGH,
	    PFPE_CCQPLOW,
	    PFPE_CQARM,
	    PFPE_CQACK,
	    PFPE_AEQALLOC,
	    PFPE_CQPERRCODES,
	    PFPE_WQEALLOC,
	    GLINT_DYN_CTL(0),
	    ICRDMA_DB_ADDR_OFFSET,

	    GLPCI_LBARCTRL,
	    GLPE_CPUSTATUS0,
	    GLPE_CPUSTATUS1,
	    GLPE_CPUSTATUS2,
	    PFINT_AEQCTL,
	    GLINT_CEQCTL(0),
	    VSIQF_PE_CTL1(0),
	    PFHMC_PDINV,
	    GLHMC_VFPDINV(0),
	    GLPE_CRITERR,
	    GLINT_RATE(0),
};

static u64 icrdma_masks[IRDMA_MAX_MASKS] = {
	ICRDMA_CCQPSTATUS_CCQP_DONE,
	    ICRDMA_CCQPSTATUS_CCQP_ERR,
	    ICRDMA_CQPSQ_STAG_PDID,
	    ICRDMA_CQPSQ_CQ_CEQID,
	    ICRDMA_CQPSQ_CQ_CQID,
	    ICRDMA_COMMIT_FPM_CQCNT,
	    ICRDMA_CQPSQ_UPESD_HMCFNID,
};

static u8 icrdma_shifts[IRDMA_MAX_SHIFTS] = {
	ICRDMA_CCQPSTATUS_CCQP_DONE_S,
	    ICRDMA_CCQPSTATUS_CCQP_ERR_S,
	    ICRDMA_CQPSQ_STAG_PDID_S,
	    ICRDMA_CQPSQ_CQ_CEQID_S,
	    ICRDMA_CQPSQ_CQ_CQID_S,
	    ICRDMA_COMMIT_FPM_CQCNT_S,
	    ICRDMA_CQPSQ_UPESD_HMCFNID_S,
};

/**
 * icrdma_ena_irq - Enable interrupt
 * @dev: pointer to the device structure
 * @idx: vector index
 */
static void
icrdma_ena_irq(struct irdma_sc_dev *dev, u32 idx)
{
	u32 val;
	u32 interval = 0;

	if (dev->ceq_itr && dev->aeq->msix_idx != idx)
		interval = dev->ceq_itr >> 1;	/* 2 usec units */
	val = FIELD_PREP(IRDMA_GLINT_DYN_CTL_ITR_INDX, IRDMA_IDX_ITR0) |
	    FIELD_PREP(IRDMA_GLINT_DYN_CTL_INTERVAL, interval) |
	    FIELD_PREP(IRDMA_GLINT_DYN_CTL_INTENA, true) |
	    FIELD_PREP(IRDMA_GLINT_DYN_CTL_CLEARPBA, true);
	writel(val, dev->hw_regs[IRDMA_GLINT_DYN_CTL] + idx);
}

/**
 * icrdma_disable_irq - Disable interrupt
 * @dev: pointer to the device structure
 * @idx: vector index
 */
static void
icrdma_disable_irq(struct irdma_sc_dev *dev, u32 idx)
{
	writel(0, dev->hw_regs[IRDMA_GLINT_DYN_CTL] + idx);
}

/**
 * icrdma_cfg_ceq- Configure CEQ interrupt
 * @dev: pointer to the device structure
 * @ceq_id: Completion Event Queue ID
 * @idx: vector index
 * @enable: True to enable, False disables
 */
static void
icrdma_cfg_ceq(struct irdma_sc_dev *dev, u32 ceq_id, u32 idx,
	       bool enable)
{
	u32 reg_val;

	reg_val = enable ? IRDMA_GLINT_CEQCTL_CAUSE_ENA : 0;
	reg_val |= (idx << IRDMA_GLINT_CEQCTL_MSIX_INDX_S) |
	    IRDMA_GLINT_CEQCTL_ITR_INDX;

	writel(reg_val, dev->hw_regs[IRDMA_GLINT_CEQCTL] + ceq_id);
}

static const struct irdma_irq_ops icrdma_irq_ops = {
	.irdma_cfg_aeq = irdma_cfg_aeq,
	.irdma_cfg_ceq = icrdma_cfg_ceq,
	.irdma_dis_irq = icrdma_disable_irq,
	.irdma_en_irq = icrdma_ena_irq,
};

static const struct irdma_hw_stat_map icrdma_hw_stat_map[] = {
	[IRDMA_HW_STAT_INDEX_RXVLANERR] = {0, 32, IRDMA_MAX_STATS_24},
	[IRDMA_HW_STAT_INDEX_IP4RXOCTS] = {8, 0, IRDMA_MAX_STATS_48},
	[IRDMA_HW_STAT_INDEX_IP4RXPKTS] = {16, 0, IRDMA_MAX_STATS_48},
	[IRDMA_HW_STAT_INDEX_IP4RXDISCARD] = {24, 32, IRDMA_MAX_STATS_32},
	[IRDMA_HW_STAT_INDEX_IP4RXTRUNC] = {24, 0, IRDMA_MAX_STATS_32},
	[IRDMA_HW_STAT_INDEX_IP4RXFRAGS] = {32, 0, IRDMA_MAX_STATS_48},
	[IRDMA_HW_STAT_INDEX_IP4RXMCOCTS] = {40, 0, IRDMA_MAX_STATS_48},
	[IRDMA_HW_STAT_INDEX_IP4RXMCPKTS] = {48, 0, IRDMA_MAX_STATS_48},
	[IRDMA_HW_STAT_INDEX_IP6RXOCTS] = {56, 0, IRDMA_MAX_STATS_48},
	[IRDMA_HW_STAT_INDEX_IP6RXPKTS] = {64, 0, IRDMA_MAX_STATS_48},
	[IRDMA_HW_STAT_INDEX_IP6RXDISCARD] = {72, 32, IRDMA_MAX_STATS_32},
	[IRDMA_HW_STAT_INDEX_IP6RXTRUNC] = {72, 0, IRDMA_MAX_STATS_32},
	[IRDMA_HW_STAT_INDEX_IP6RXFRAGS] = {80, 0, IRDMA_MAX_STATS_48},
	[IRDMA_HW_STAT_INDEX_IP6RXMCOCTS] = {88, 0, IRDMA_MAX_STATS_48},
	[IRDMA_HW_STAT_INDEX_IP6RXMCPKTS] = {96, 0, IRDMA_MAX_STATS_48},
	[IRDMA_HW_STAT_INDEX_IP4TXOCTS] = {104, 0, IRDMA_MAX_STATS_48},
	[IRDMA_HW_STAT_INDEX_IP4TXPKTS] = {112, 0, IRDMA_MAX_STATS_48},
	[IRDMA_HW_STAT_INDEX_IP4TXFRAGS] = {120, 0, IRDMA_MAX_STATS_48},
	[IRDMA_HW_STAT_INDEX_IP4TXMCOCTS] = {128, 0, IRDMA_MAX_STATS_48},
	[IRDMA_HW_STAT_INDEX_IP4TXMCPKTS] = {136, 0, IRDMA_MAX_STATS_48},
	[IRDMA_HW_STAT_INDEX_IP6TXOCTS] = {144, 0, IRDMA_MAX_STATS_48},
	[IRDMA_HW_STAT_INDEX_IP6TXPKTS] = {152, 0, IRDMA_MAX_STATS_48},
	[IRDMA_HW_STAT_INDEX_IP6TXFRAGS] = {160, 0, IRDMA_MAX_STATS_48},
	[IRDMA_HW_STAT_INDEX_IP6TXMCOCTS] = {168, 0, IRDMA_MAX_STATS_48},
	[IRDMA_HW_STAT_INDEX_IP6TXMCPKTS] = {176, 0, IRDMA_MAX_STATS_48},
	[IRDMA_HW_STAT_INDEX_IP4TXNOROUTE] = {184, 32, IRDMA_MAX_STATS_24},
	[IRDMA_HW_STAT_INDEX_IP6TXNOROUTE] = {184, 0, IRDMA_MAX_STATS_24},
	[IRDMA_HW_STAT_INDEX_TCPRXSEGS] = {192, 32, IRDMA_MAX_STATS_48},
	[IRDMA_HW_STAT_INDEX_TCPRXOPTERR] = {200, 32, IRDMA_MAX_STATS_24},
	[IRDMA_HW_STAT_INDEX_TCPRXPROTOERR] = {200, 0, IRDMA_MAX_STATS_24},
	[IRDMA_HW_STAT_INDEX_TCPTXSEG] = {208, 0, IRDMA_MAX_STATS_48},
	[IRDMA_HW_STAT_INDEX_TCPRTXSEG] = {216, 32, IRDMA_MAX_STATS_32},
	[IRDMA_HW_STAT_INDEX_UDPRXPKTS] = {224, 0, IRDMA_MAX_STATS_48},
	[IRDMA_HW_STAT_INDEX_UDPTXPKTS] = {232, 0, IRDMA_MAX_STATS_48},
	[IRDMA_HW_STAT_INDEX_RDMARXWRS] = {240, 0, IRDMA_MAX_STATS_48},
	[IRDMA_HW_STAT_INDEX_RDMARXRDS] = {248, 0, IRDMA_MAX_STATS_48},
	[IRDMA_HW_STAT_INDEX_RDMARXSNDS] = {256, 0, IRDMA_MAX_STATS_48},
	[IRDMA_HW_STAT_INDEX_RDMATXWRS] = {264, 0, IRDMA_MAX_STATS_48},
	[IRDMA_HW_STAT_INDEX_RDMATXRDS] = {272, 0, IRDMA_MAX_STATS_48},
	[IRDMA_HW_STAT_INDEX_RDMATXSNDS] = {280, 0, IRDMA_MAX_STATS_48},
	[IRDMA_HW_STAT_INDEX_RDMAVBND] = {288, 0, IRDMA_MAX_STATS_48},
	[IRDMA_HW_STAT_INDEX_RDMAVINV] = {296, 0, IRDMA_MAX_STATS_48},
	[IRDMA_HW_STAT_INDEX_RXNPECNMARKEDPKTS] = {304, 0, IRDMA_MAX_STATS_48},
	[IRDMA_HW_STAT_INDEX_RXRPCNPIGNORED] = {312, 32, IRDMA_MAX_STATS_16},
	[IRDMA_HW_STAT_INDEX_RXRPCNPHANDLED] = {312, 0, IRDMA_MAX_STATS_32},
	[IRDMA_HW_STAT_INDEX_TXNPCNPSENT] = {320, 0, IRDMA_MAX_STATS_32},
};

void
icrdma_init_hw(struct irdma_sc_dev *dev)
{
	int i;
	u8 IOMEM *hw_addr;

	for (i = 0; i < IRDMA_MAX_REGS; ++i) {
		hw_addr = dev->hw->hw_addr;

		if (i == IRDMA_DB_ADDR_OFFSET)
			hw_addr = NULL;

		dev->hw_regs[i] = (u32 IOMEM *) (hw_addr + icrdma_regs[i]);
	}

	for (i = 0; i < IRDMA_MAX_SHIFTS; ++i)
		dev->hw_shifts[i] = icrdma_shifts[i];

	for (i = 0; i < IRDMA_MAX_MASKS; ++i)
		dev->hw_masks[i] = icrdma_masks[i];

	dev->wqe_alloc_db = dev->hw_regs[IRDMA_WQEALLOC];
	dev->cq_arm_db = dev->hw_regs[IRDMA_CQARM];
	dev->aeq_alloc_db = dev->hw_regs[IRDMA_AEQALLOC];
	dev->cqp_db = dev->hw_regs[IRDMA_CQPDB];
	dev->cq_ack_db = dev->hw_regs[IRDMA_CQACK];
	dev->irq_ops = &icrdma_irq_ops;
	dev->hw_stats_map = icrdma_hw_stat_map;
	dev->hw_attrs.page_size_cap = SZ_4K | SZ_2M | SZ_1G;
	dev->hw_attrs.max_hw_ird = ICRDMA_MAX_IRD_SIZE;
	dev->hw_attrs.max_hw_ord = ICRDMA_MAX_ORD_SIZE;
	dev->hw_attrs.max_stat_inst = ICRDMA_MAX_STATS_COUNT;
	dev->hw_attrs.max_stat_idx = IRDMA_HW_STAT_INDEX_MAX_GEN_2;
	dev->hw_attrs.max_hw_device_pages = ICRDMA_MAX_PUSH_PAGE_COUNT;

	dev->hw_attrs.uk_attrs.max_hw_wq_frags = ICRDMA_MAX_WQ_FRAGMENT_COUNT;
	dev->hw_attrs.uk_attrs.max_hw_read_sges = ICRDMA_MAX_SGE_RD;
	dev->hw_attrs.uk_attrs.min_hw_wq_size = ICRDMA_MIN_WQ_SIZE;
	dev->hw_attrs.uk_attrs.max_hw_sq_chunk = IRDMA_MAX_QUANTA_PER_WR;
	disable_tx_spad(dev->hw);
	disable_prefetch(dev->hw);
	rdpu_ackreqpmthresh(dev->hw);
	dev->hw_attrs.uk_attrs.feature_flags |= IRDMA_FEATURE_RELAX_RQ_ORDER;
	dev->hw_attrs.uk_attrs.feature_flags |= IRDMA_FEATURE_RTS_AE |
	    IRDMA_FEATURE_CQ_RESIZE;
}

void
irdma_init_config_check(struct irdma_config_check *cc, u8 traffic_class, u16 qs_handle)
{
	cc->config_ok = false;
	cc->traffic_class = traffic_class;
	cc->qs_handle = qs_handle;
	cc->lfc_set = 0;
	cc->pfc_set = 0;
}

static bool
irdma_is_lfc_set(struct irdma_config_check *cc, struct irdma_sc_vsi *vsi)
{
	u32 lfc = 1;
	u8 fn_id = vsi->dev->hmc_fn_id;

	lfc &= (rd32(vsi->dev->hw,
		     PRTMAC_HSEC_CTL_RX_PAUSE_ENABLE_0 + 4 * fn_id) >> 8);
	lfc &= (rd32(vsi->dev->hw,
		     PRTMAC_HSEC_CTL_TX_PAUSE_ENABLE_0 + 4 * fn_id) >> 8);
	lfc &= rd32(vsi->dev->hw,
		    PRTMAC_HSEC_CTL_RX_ENABLE_GPP_0 + 4 * vsi->dev->hmc_fn_id);

	if (lfc)
		return true;
	return false;
}

static bool
irdma_check_tc_has_pfc(struct irdma_sc_vsi *vsi, u64 reg_offset, u16 traffic_class)
{
	u32 value, pfc = 0;
	u32 i;

	value = rd32(vsi->dev->hw, reg_offset);
	for (i = 0; i < 4; i++)
		pfc |= (value >> (8 * i + traffic_class)) & 0x1;

	if (pfc)
		return true;
	return false;
}

static bool
irdma_is_pfc_set(struct irdma_config_check *cc, struct irdma_sc_vsi *vsi)
{
	u32 pause;
	u8 fn_id = vsi->dev->hmc_fn_id;

	pause = (rd32(vsi->dev->hw,
		      PRTMAC_HSEC_CTL_RX_PAUSE_ENABLE_0 + 4 * fn_id) >>
		 cc->traffic_class) & BIT(0);
	pause &= (rd32(vsi->dev->hw,
		       PRTMAC_HSEC_CTL_TX_PAUSE_ENABLE_0 + 4 * fn_id) >>
		  cc->traffic_class) & BIT(0);

	return irdma_check_tc_has_pfc(vsi, GLDCB_TC2PFC, cc->traffic_class) &&
	    pause;
}

bool
irdma_is_config_ok(struct irdma_config_check *cc, struct irdma_sc_vsi *vsi)
{
	cc->lfc_set = irdma_is_lfc_set(cc, vsi);
	cc->pfc_set = irdma_is_pfc_set(cc, vsi);

	cc->config_ok = cc->lfc_set || cc->pfc_set;

	return cc->config_ok;
}

#define IRDMA_RCV_WND_NO_FC	65536
#define IRDMA_RCV_WND_FC	65536

#define IRDMA_CWND_NO_FC	0x1
#define IRDMA_CWND_FC		0x18

#define IRDMA_RTOMIN_NO_FC	0x5
#define IRDMA_RTOMIN_FC		0x32

#define IRDMA_ACKCREDS_NO_FC	0x02
#define IRDMA_ACKCREDS_FC	0x06

static void
irdma_check_flow_ctrl(struct irdma_sc_vsi *vsi, u8 user_prio, u8 traffic_class)
{
	struct irdma_config_check *cfg_chk = &vsi->cfg_check[user_prio];

	if (!irdma_is_config_ok(cfg_chk, vsi)) {
		if (vsi->tc_print_warning[traffic_class]) {
			irdma_pr_info("INFO: Flow control is disabled for this traffic class (%d) on this vsi.\n", traffic_class);
			vsi->tc_print_warning[traffic_class] = false;
		}
	} else {
		if (vsi->tc_print_warning[traffic_class]) {
			irdma_pr_info("INFO: Flow control is enabled for this traffic class (%d) on this vsi.\n", traffic_class);
			vsi->tc_print_warning[traffic_class] = false;
		}
	}
}

void
irdma_check_fc_for_tc_update(struct irdma_sc_vsi *vsi,
			     struct irdma_l2params *l2params)
{
	u8 i;

	for (i = 0; i < IRDMA_MAX_TRAFFIC_CLASS; i++)
		vsi->tc_print_warning[i] = true;

	for (i = 0; i < IRDMA_MAX_USER_PRIORITY; i++) {
		struct irdma_config_check *cfg_chk = &vsi->cfg_check[i];
		u8 tc = l2params->up2tc[i];

		cfg_chk->traffic_class = tc;
		cfg_chk->qs_handle = vsi->qos[i].qs_handle;
		irdma_check_flow_ctrl(vsi, i, tc);
	}
}

void
irdma_check_fc_for_qp(struct irdma_sc_vsi *vsi, struct irdma_sc_qp *sc_qp)
{
	u8 i;

	for (i = 0; i < IRDMA_MAX_USER_PRIORITY; i++) {
		struct irdma_config_check *cfg_chk = &vsi->cfg_check[i];

		irdma_init_config_check(cfg_chk,
					vsi->qos[i].traffic_class,
					vsi->qos[i].qs_handle);
		if (sc_qp->qs_handle == cfg_chk->qs_handle)
			irdma_check_flow_ctrl(vsi, i, cfg_chk->traffic_class);
	}
}

#define GLPE_WQMTXIDXADDR	0x50E000
#define GLPE_WQMTXIDXDATA	0x50E004

void
disable_prefetch(struct irdma_hw *hw)
{
	u32 wqm_data;

	wr32(hw, GLPE_WQMTXIDXADDR, 0x12);
	irdma_mb();

	wqm_data = rd32(hw, GLPE_WQMTXIDXDATA);
	wqm_data &= ~(1);
	wr32(hw, GLPE_WQMTXIDXDATA, wqm_data);
}

void
disable_tx_spad(struct irdma_hw *hw)
{
	u32 wqm_data;

	wr32(hw, GLPE_WQMTXIDXADDR, 0x12);
	irdma_mb();

	wqm_data = rd32(hw, GLPE_WQMTXIDXDATA);
	wqm_data &= ~(1 << 3);
	wr32(hw, GLPE_WQMTXIDXDATA, wqm_data);
}

#define GL_RDPU_CNTRL		0x52054
void
rdpu_ackreqpmthresh(struct irdma_hw *hw)
{
	u32 val;

	val = rd32(hw, GL_RDPU_CNTRL);
	val &= ~(0x3f << 10);
	val |= (3 << 10);
	wr32(hw, GL_RDPU_CNTRL, val);
}
