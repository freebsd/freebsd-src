/*
 * Copyright (c) 2015-2024, Broadcom. All rights reserved.  The term
 * Broadcom refers to Broadcom Limited and/or its subsidiaries.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Description: Slow Path Operators
 */

#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/if_ether.h>
#include <linux/printk.h>

#include "hsi_struct_def.h"
#include "qplib_tlv.h"
#include "qplib_res.h"
#include "qplib_rcfw.h"
#include "qplib_sp.h"

const struct bnxt_qplib_gid bnxt_qplib_gid_zero = {{ 0, 0, 0, 0, 0, 0, 0, 0,
						     0, 0, 0, 0, 0, 0, 0, 0 }};

/* Device */
static u8 bnxt_qplib_is_atomic_cap(struct bnxt_qplib_rcfw *rcfw)
{
	u16 pcie_ctl2 = 0;

	if (!_is_chip_gen_p5_p7(rcfw->res->cctx))
		return false;
	pcie_capability_read_word(rcfw->pdev, PCI_EXP_DEVCTL2, &pcie_ctl2);
	return (pcie_ctl2 & PCI_EXP_DEVCTL2_ATOMIC_REQ);
}

static void bnxt_qplib_query_version(struct bnxt_qplib_rcfw *rcfw, char *fw_ver)
{
	struct creq_query_version_resp resp = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct cmdq_query_version req = {};
	int rc = 0;

	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_QUERY_VERSION,
				 sizeof(req));
	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, NULL, sizeof(req),
				sizeof(resp), 0);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	if (rc) {
		dev_err(&rcfw->pdev->dev, "QPLIB: Failed to query version\n");
		return;
	}
	fw_ver[0] = resp.fw_maj;
	fw_ver[1] = resp.fw_minor;
	fw_ver[2] = resp.fw_bld;
	fw_ver[3] = resp.fw_rsvd;
}

int bnxt_qplib_get_dev_attr(struct bnxt_qplib_rcfw *rcfw)
{
	struct creq_query_func_resp resp = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct creq_query_func_resp_sb *sb;
	struct bnxt_qplib_rcfw_sbuf sbuf;
	struct bnxt_qplib_dev_attr *attr;
	struct bnxt_qplib_chip_ctx *cctx;
	struct cmdq_query_func req = {};
	u8 *tqm_alloc;
	int i, rc = 0;
	u32 temp;
	u8 chip_gen = BNXT_RE_DEFAULT;

	cctx = rcfw->res->cctx;
	attr = rcfw->res->dattr;

	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_QUERY_FUNC,
				 sizeof(req));

	sbuf.size = sizeof(*sb);
	sbuf.sb = dma_zalloc_coherent(&rcfw->pdev->dev, sbuf.size,
				       &sbuf.dma_addr, GFP_KERNEL);
	if (!sbuf.sb)
		return -ENOMEM;

	sb = sbuf.sb;
	req.resp_size = sbuf.size / BNXT_QPLIB_CMDQE_UNITS;
	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, &sbuf, sizeof(req),
				sizeof(resp), 0);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	if (rc)
		goto bail;
	/* Extract the context from the side buffer */
	chip_gen = _get_chip_gen_p5_type(cctx);
	attr->max_qp = le32_to_cpu(sb->max_qp);
	attr->max_qp = min_t(u32, attr->max_qp, BNXT_RE_MAX_QP_SUPPORTED(chip_gen));
	/* max_qp value reported by FW does not include the QP1 */
	attr->max_qp += 1;
	attr->max_qp_rd_atom =
		sb->max_qp_rd_atom > BNXT_QPLIB_MAX_OUT_RD_ATOM ?
		BNXT_QPLIB_MAX_OUT_RD_ATOM : sb->max_qp_rd_atom;
	attr->max_qp_init_rd_atom =
		sb->max_qp_init_rd_atom > BNXT_QPLIB_MAX_OUT_RD_ATOM ?
		BNXT_QPLIB_MAX_OUT_RD_ATOM : sb->max_qp_init_rd_atom;
	/* Report 1 less than the max_qp_wqes reported by FW as driver adds
	 * one extra entry while creating the qp
	 */
	attr->max_qp_wqes = le16_to_cpu(sb->max_qp_wr) - 1;
	/* Adjust for max_qp_wqes for variable wqe */
	if (cctx->modes.wqe_mode == BNXT_QPLIB_WQE_MODE_VARIABLE) {
		attr->max_qp_wqes = (BNXT_MAX_SQ_SIZE) /
			(BNXT_MAX_VAR_WQE_SIZE / BNXT_SGE_SIZE) - 1;
	}
	if (!_is_chip_gen_p5_p7(cctx)) {
		/*
		 * 128 WQEs needs to be reserved for the HW (8916). Prevent
		 * reporting the max number for gen-p4 only.
		 */
		attr->max_qp_wqes -= BNXT_QPLIB_RESERVED_QP_WRS;
	}
	attr->max_qp_sges = sb->max_sge;
	if (_is_chip_gen_p5_p7(cctx) &&
	    cctx->modes.wqe_mode == BNXT_QPLIB_WQE_MODE_VARIABLE)
		attr->max_qp_sges = sb->max_sge_var_wqe;
	attr->max_cq = le32_to_cpu(sb->max_cq);
	attr->max_cq = min_t(u32, attr->max_cq, BNXT_RE_MAX_CQ_SUPPORTED(chip_gen));

	attr->max_cq_wqes = le32_to_cpu(sb->max_cqe);
	attr->max_cq_wqes = min_t(u32, BNXT_QPLIB_MAX_CQ_WQES, attr->max_cq_wqes);

	attr->max_cq_sges = attr->max_qp_sges;
	attr->max_mr = le32_to_cpu(sb->max_mr);
	attr->max_mr = min_t(u32, attr->max_mr, BNXT_RE_MAX_MRW_SUPPORTED(chip_gen));
	attr->max_mw = le32_to_cpu(sb->max_mw);
	attr->max_mw = min_t(u32, attr->max_mw, BNXT_RE_MAX_MRW_SUPPORTED(chip_gen));

	attr->max_mr_size = le64_to_cpu(sb->max_mr_size);
	attr->max_pd = BNXT_QPLIB_MAX_PD;
	attr->max_raw_ethy_qp = le32_to_cpu(sb->max_raw_eth_qp);
	attr->max_ah = le32_to_cpu(sb->max_ah);
	attr->max_ah = min_t(u32, attr->max_ah, BNXT_RE_MAX_AH_SUPPORTED(chip_gen));

	attr->max_fmr = le32_to_cpu(sb->max_fmr);
	attr->max_map_per_fmr = sb->max_map_per_fmr;

	attr->max_srq = le16_to_cpu(sb->max_srq);
	attr->max_srq = min_t(u32, attr->max_srq, BNXT_RE_MAX_SRQ_SUPPORTED(chip_gen));
	attr->max_srq_wqes = le32_to_cpu(sb->max_srq_wr) - 1;
	attr->max_srq_sges = sb->max_srq_sge;
	attr->max_pkey = 1;

	attr->max_inline_data = !cctx->modes.wqe_mode ?
				le32_to_cpu(sb->max_inline_data) :
				le16_to_cpu(sb->max_inline_data_var_wqe);
	if (!_is_chip_p7(cctx)) {
		attr->l2_db_size = (sb->l2_db_space_size + 1) *
				    (0x01 << RCFW_DBR_BASE_PAGE_SHIFT);
	}
	attr->max_sgid = le32_to_cpu(sb->max_gid);

	/* TODO: remove this hack for statically allocated gid_map */
	bnxt_re_set_max_gid(&attr->max_sgid);

	attr->dev_cap_flags = le16_to_cpu(sb->dev_cap_flags);
	attr->page_size_cap = BIT_ULL(28) | BIT_ULL(21) | BIT_ULL(12);

	bnxt_qplib_query_version(rcfw, attr->fw_ver);

	for (i = 0; i < MAX_TQM_ALLOC_REQ / 4; i++) {
		temp = le32_to_cpu(sb->tqm_alloc_reqs[i]);
		tqm_alloc = (u8 *)&temp;
		attr->tqm_alloc_reqs[i * 4] = *tqm_alloc;
		attr->tqm_alloc_reqs[i * 4 + 1] = *(++tqm_alloc);
		attr->tqm_alloc_reqs[i * 4 + 2] = *(++tqm_alloc);
		attr->tqm_alloc_reqs[i * 4 + 3] = *(++tqm_alloc);
	}

	if (rcfw->res->cctx->hwrm_intf_ver >= HWRM_VERSION_DEV_ATTR_MAX_DPI)
		attr->max_dpi = le32_to_cpu(sb->max_dpi);

	attr->is_atomic = bnxt_qplib_is_atomic_cap(rcfw);
bail:
	dma_free_coherent(&rcfw->pdev->dev, sbuf.size,
				  sbuf.sb, sbuf.dma_addr);
	return rc;
}

int bnxt_qplib_set_func_resources(struct bnxt_qplib_res *res)
{
	struct creq_set_func_resources_resp resp = {};
	struct cmdq_set_func_resources req = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct bnxt_qplib_rcfw *rcfw;
	struct bnxt_qplib_ctx *hctx;
	int rc = 0;

	rcfw = res->rcfw;
	hctx = res->hctx;
	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_SET_FUNC_RESOURCES,
				 sizeof(req));

	req.number_of_qp = cpu_to_le32(hctx->qp_ctx.max);
	req.number_of_mrw = cpu_to_le32(hctx->mrw_ctx.max);
	req.number_of_srq =  cpu_to_le32(hctx->srq_ctx.max);
	req.number_of_cq = cpu_to_le32(hctx->cq_ctx.max);

	req.max_qp_per_vf = cpu_to_le32(hctx->vf_res.max_qp);
	req.max_mrw_per_vf = cpu_to_le32(hctx->vf_res.max_mrw);
	req.max_srq_per_vf = cpu_to_le32(hctx->vf_res.max_srq);
	req.max_cq_per_vf = cpu_to_le32(hctx->vf_res.max_cq);
	req.max_gid_per_vf = cpu_to_le32(hctx->vf_res.max_gid);

	/* Keep the old stats context id of PF */
	req.stat_ctx_id = cpu_to_le32(hctx->stats.fw_id);

	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, NULL, sizeof(req),
				sizeof(resp), 0);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	if (rc)
		dev_err(&res->pdev->dev,
			"QPLIB: Failed to set function resources\n");

	return rc;
}

int bnxt_qplib_update_sgid(struct bnxt_qplib_sgid_tbl *sgid_tbl,
			   struct bnxt_qplib_gid *gid, u16 gid_idx, const u8 *smac)
{
	struct bnxt_qplib_res *res = to_bnxt_qplib(sgid_tbl,
						   struct bnxt_qplib_res,
						   sgid_tbl);
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct creq_modify_gid_resp resp = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct cmdq_modify_gid req = {};
	int rc;

	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_MODIFY_GID,
				 sizeof(req));

	req.gid[0] = cpu_to_be32(((u32 *)gid->data)[3]);
	req.gid[1] = cpu_to_be32(((u32 *)gid->data)[2]);
	req.gid[2] = cpu_to_be32(((u32 *)gid->data)[1]);
	req.gid[3] = cpu_to_be32(((u32 *)gid->data)[0]);
	if (res->prio) {
		req.vlan |= cpu_to_le16(CMDQ_ADD_GID_VLAN_TPID_TPID_8100 |
			CMDQ_ADD_GID_VLAN_VLAN_EN);
	}

	/* MAC in network format */
	req.src_mac[0] = cpu_to_be16(((u16 *)smac)[0]);
	req.src_mac[1] = cpu_to_be16(((u16 *)smac)[1]);
	req.src_mac[2] = cpu_to_be16(((u16 *)smac)[2]);
	req.gid_index = cpu_to_le16(gid_idx);

	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, NULL, sizeof(req),
				sizeof(resp), 0);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	if (rc) {
		dev_err(&res->pdev->dev,
			"QPLIB: update SGID table failed\n");
		return rc;
	}
	return 0;
}

/* SGID */
int bnxt_qplib_get_sgid(struct bnxt_qplib_res *res,
			struct bnxt_qplib_sgid_tbl *sgid_tbl, int index,
			struct bnxt_qplib_gid *gid)
{
	if (index > sgid_tbl->max) {
		dev_err(&res->pdev->dev,
			"QPLIB: Index %d exceeded SGID table max (%d)\n",
			index, sgid_tbl->max);
		return -EINVAL;
	}
	memcpy(gid, &sgid_tbl->tbl[index].gid, sizeof(*gid));
	return 0;
}

int bnxt_qplib_del_sgid(struct bnxt_qplib_sgid_tbl *sgid_tbl,
			struct bnxt_qplib_gid *gid,
			u16 vlan_id, bool update)
{
	struct bnxt_qplib_res *res = to_bnxt_qplib(sgid_tbl,
						   struct bnxt_qplib_res,
						   sgid_tbl);
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	int index;

	if (sgid_tbl == NULL) {
		dev_err(&res->pdev->dev, "QPLIB: SGID table not allocated\n");
		return -EINVAL;
	}
	/* Do we need a sgid_lock here? */
	if (!sgid_tbl->active) {
		dev_err(&res->pdev->dev,
			"QPLIB: SGID table has no active entries\n");
		return -ENOMEM;
	}
	for (index = 0; index < sgid_tbl->max; index++) {
		if (!memcmp(&sgid_tbl->tbl[index].gid, gid, sizeof(*gid)) &&
		    vlan_id == sgid_tbl->tbl[index].vlan_id)
			break;
	}
	if (index == sgid_tbl->max) {
		dev_warn(&res->pdev->dev, "GID not found in the SGID table\n");
		return 0;
	}

	if (update) {
		struct creq_delete_gid_resp resp = {};
		struct bnxt_qplib_cmdqmsg msg = {};
		struct cmdq_delete_gid req = {};
		int rc;

		bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_DELETE_GID,
					 sizeof(req));
		if (sgid_tbl->hw_id[index] == 0xFFFF) {
			dev_err(&res->pdev->dev,
				"QPLIB: GID entry contains an invalid HW id");
			return -EINVAL;
		}
		req.gid_index = cpu_to_le16(sgid_tbl->hw_id[index]);
		bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, NULL, sizeof(req),
					sizeof(resp), 0);
		rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
		if (rc)
			return rc;
	}
	memcpy(&sgid_tbl->tbl[index].gid, &bnxt_qplib_gid_zero,
	       sizeof(bnxt_qplib_gid_zero));
	sgid_tbl->tbl[index].vlan_id = 0xFFFF;
	sgid_tbl->vlan[index] = false;
	sgid_tbl->active--;
	dev_dbg(&res->pdev->dev,
		 "QPLIB: SGID deleted hw_id[0x%x] = 0x%x active = 0x%x\n",
		 index, sgid_tbl->hw_id[index], sgid_tbl->active);
	sgid_tbl->hw_id[index] = (u16)-1;

	return 0;
}

int bnxt_qplib_add_sgid(struct bnxt_qplib_sgid_tbl *sgid_tbl,
			const union ib_gid *gid, const u8 *smac, u16 vlan_id,
			bool update, u32 *index)
{
	struct bnxt_qplib_res *res = to_bnxt_qplib(sgid_tbl,
						   struct bnxt_qplib_res,
						   sgid_tbl);
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	int i, free_idx;

	if (sgid_tbl == NULL) {
		dev_err(&res->pdev->dev, "QPLIB: SGID table not allocated\n");
		return -EINVAL;
	}
	/* Do we need a sgid_lock here? */
	if (sgid_tbl->active == sgid_tbl->max) {
		dev_err(&res->pdev->dev, "QPLIB: SGID table is full\n");
		return -ENOMEM;
	}
	free_idx = sgid_tbl->max;
	for (i = 0; i < sgid_tbl->max; i++) {
		if (!memcmp(&sgid_tbl->tbl[i], gid, sizeof(*gid)) &&
		    sgid_tbl->tbl[i].vlan_id == vlan_id) {
			dev_dbg(&res->pdev->dev,
				"QPLIB: SGID entry already exist in entry %d!\n",
				i);
			*index = i;
			return -EALREADY;
		} else if (!memcmp(&sgid_tbl->tbl[i], &bnxt_qplib_gid_zero,
				   sizeof(bnxt_qplib_gid_zero)) &&
			   free_idx == sgid_tbl->max) {
			free_idx = i;
		}
	}
	if (free_idx == sgid_tbl->max) {
		dev_err(&res->pdev->dev,
			"QPLIB: SGID table is FULL but count is not MAX??\n");
		return -ENOMEM;
	}
	if (update) {
		struct creq_add_gid_resp resp = {};
		struct bnxt_qplib_cmdqmsg msg = {};
		struct cmdq_add_gid req = {};
		int rc;

		bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_ADD_GID,
					 sizeof(req));

		req.gid[0] = cpu_to_be32(((u32 *)gid->raw)[3]);
		req.gid[1] = cpu_to_be32(((u32 *)gid->raw)[2]);
		req.gid[2] = cpu_to_be32(((u32 *)gid->raw)[1]);
		req.gid[3] = cpu_to_be32(((u32 *)gid->raw)[0]);
		/* driver should ensure that all RoCE traffic is always VLAN tagged
		 * if RoCE traffic is running on non-zero VLAN ID or
		 * RoCE traffic is running on non-zero Priority.
		 */
		if ((vlan_id != 0xFFFF) || res->prio) {
			if (vlan_id != 0xFFFF)
				req.vlan = cpu_to_le16(vlan_id &
						CMDQ_ADD_GID_VLAN_VLAN_ID_MASK);
			req.vlan |=
				cpu_to_le16(CMDQ_ADD_GID_VLAN_TPID_TPID_8100 |
					    CMDQ_ADD_GID_VLAN_VLAN_EN);
		}

		/* MAC in network format */
		req.src_mac[0] = cpu_to_be16(((u16 *)smac)[0]);
		req.src_mac[1] = cpu_to_be16(((u16 *)smac)[1]);
		req.src_mac[2] = cpu_to_be16(((u16 *)smac)[2]);

		bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, NULL, sizeof(req),
					sizeof(resp), 0);
		rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
		if (rc)
			return rc;
		sgid_tbl->hw_id[free_idx] = le32_to_cpu(resp.xid);
	}

	if (vlan_id != 0xFFFF)
		sgid_tbl->vlan[free_idx] = true;

	memcpy(&sgid_tbl->tbl[free_idx], gid, sizeof(*gid));
	sgid_tbl->tbl[free_idx].vlan_id = vlan_id;
	sgid_tbl->active++;
	dev_dbg(&res->pdev->dev,
		 "QPLIB: SGID added hw_id[0x%x] = 0x%x active = 0x%x\n",
		 free_idx, sgid_tbl->hw_id[free_idx], sgid_tbl->active);

	*index = free_idx;
	/* unlock */
	return 0;
}

/* AH */
int bnxt_qplib_create_ah(struct bnxt_qplib_res *res, struct bnxt_qplib_ah *ah,
			 bool block)
{
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct creq_create_ah_resp resp = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct cmdq_create_ah req = {};
	u32 temp32[4];
	u16 temp16[3];
	int rc;

	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_CREATE_AH,
				 sizeof(req));

	memcpy(temp32, ah->dgid.data, sizeof(struct bnxt_qplib_gid));
	req.dgid[0] = cpu_to_le32(temp32[0]);
	req.dgid[1] = cpu_to_le32(temp32[1]);
	req.dgid[2] = cpu_to_le32(temp32[2]);
	req.dgid[3] = cpu_to_le32(temp32[3]);

	req.type = ah->nw_type;
	req.hop_limit = ah->hop_limit;
	req.sgid_index = cpu_to_le16(res->sgid_tbl.hw_id[ah->sgid_index]);
	req.dest_vlan_id_flow_label = cpu_to_le32((ah->flow_label &
					CMDQ_CREATE_AH_FLOW_LABEL_MASK) |
					CMDQ_CREATE_AH_DEST_VLAN_ID_MASK);
	req.pd_id = cpu_to_le32(ah->pd->id);
	req.traffic_class = ah->traffic_class;

	/* MAC in network format */
	memcpy(temp16, ah->dmac, ETH_ALEN);
	req.dest_mac[0] = cpu_to_le16(temp16[0]);
	req.dest_mac[1] = cpu_to_le16(temp16[1]);
	req.dest_mac[2] = cpu_to_le16(temp16[2]);

	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, NULL, sizeof(req),
				sizeof(resp), block);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	if (rc)
		return rc;

	ah->id = le32_to_cpu(resp.xid);
	/* for Cu/Wh AHID 0 is not valid */
	if (!_is_chip_gen_p5_p7(res->cctx) && !ah->id)
		rc = -EINVAL;

	return rc;
}

int bnxt_qplib_destroy_ah(struct bnxt_qplib_res *res, struct bnxt_qplib_ah *ah,
			  bool block)
{
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct creq_destroy_ah_resp resp = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct cmdq_destroy_ah req = {};
	int rc;

	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_DESTROY_AH,
				 sizeof(req));

	req.ah_cid = cpu_to_le32(ah->id);

	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, NULL, sizeof(req),
				sizeof(resp), block);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	return rc;
}

/* MRW */
int bnxt_qplib_free_mrw(struct bnxt_qplib_res *res, struct bnxt_qplib_mrw *mrw)
{
	struct creq_deallocate_key_resp resp = {};
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct cmdq_deallocate_key req = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	int rc;

	if (mrw->lkey == 0xFFFFFFFF) {
		dev_info(&res->pdev->dev,
			 "QPLIB: SP: Free a reserved lkey MRW\n");
		return 0;
	}

	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_DEALLOCATE_KEY,
				 sizeof(req));

	req.mrw_flags = mrw->type;

	if ((mrw->type == CMDQ_ALLOCATE_MRW_MRW_FLAGS_MW_TYPE1)  ||
	    (mrw->type == CMDQ_ALLOCATE_MRW_MRW_FLAGS_MW_TYPE2A) ||
	    (mrw->type == CMDQ_ALLOCATE_MRW_MRW_FLAGS_MW_TYPE2B))
		req.key = cpu_to_le32(mrw->rkey);
	else
		req.key = cpu_to_le32(mrw->lkey);

	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, NULL, sizeof(req),
				sizeof(resp), 0);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	if (rc)
		return rc;

	if (mrw->hwq.max_elements)
		bnxt_qplib_free_hwq(res, &mrw->hwq);

	return 0;
}

int bnxt_qplib_alloc_mrw(struct bnxt_qplib_res *res, struct bnxt_qplib_mrw *mrw)
{
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct creq_allocate_mrw_resp resp = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct cmdq_allocate_mrw req = {};
	int rc;

	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_ALLOCATE_MRW,
				 sizeof(req));

	req.pd_id = cpu_to_le32(mrw->pd->id);
	req.mrw_flags = mrw->type;
	if ((mrw->type == CMDQ_ALLOCATE_MRW_MRW_FLAGS_PMR &&
	     mrw->flags & BNXT_QPLIB_FR_PMR) ||
	    mrw->type == CMDQ_ALLOCATE_MRW_MRW_FLAGS_MW_TYPE2A ||
	    mrw->type == CMDQ_ALLOCATE_MRW_MRW_FLAGS_MW_TYPE2B)
		req.access = CMDQ_ALLOCATE_MRW_ACCESS_CONSUMER_OWNED_KEY;
	req.mrw_handle = cpu_to_le64((uintptr_t)mrw);

	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, NULL, sizeof(req),
				sizeof(resp), 0);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	if (rc)
		return rc;
	if ((mrw->type == CMDQ_ALLOCATE_MRW_MRW_FLAGS_MW_TYPE1)  ||
	    (mrw->type == CMDQ_ALLOCATE_MRW_MRW_FLAGS_MW_TYPE2A) ||
	    (mrw->type == CMDQ_ALLOCATE_MRW_MRW_FLAGS_MW_TYPE2B))
		mrw->rkey = le32_to_cpu(resp.xid);
	else
		mrw->lkey = le32_to_cpu(resp.xid);

	return 0;
}

int bnxt_qplib_dereg_mrw(struct bnxt_qplib_res *res, struct bnxt_qplib_mrw *mrw,
			 bool block)
{
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct creq_deregister_mr_resp resp = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct cmdq_deregister_mr req = {};
	int rc;

	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_DEREGISTER_MR,
				 sizeof(req));

	req.lkey = cpu_to_le32(mrw->lkey);
	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, NULL, sizeof(req),
				sizeof(resp), block);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	if (rc)
		return rc;

	if (mrw->hwq.max_elements) {
		mrw->va = 0;
		mrw->total_size = 0;
		bnxt_qplib_free_hwq(res, &mrw->hwq);
	}

	return 0;
}

int bnxt_qplib_reg_mr(struct bnxt_qplib_res *res,
		      struct bnxt_qplib_mrinfo *mrinfo,
		      bool block)
{
	struct bnxt_qplib_hwq_attr hwq_attr = {};
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct creq_register_mr_resp resp = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct cmdq_register_mr req = {};
	struct bnxt_qplib_mrw *mr;
	u32 buf_pg_size;
	u32 pg_size;
	u16 level;
	u16 flags;
	int rc;

	mr = mrinfo->mrw;
	buf_pg_size = 0x01ULL << mrinfo->sg.pgshft;
	if (mrinfo->sg.npages) {
		/* Free the hwq if it already exist, must be a rereg */
		if (mr->hwq.max_elements)
			bnxt_qplib_free_hwq(res, &mr->hwq);
		/* Use system PAGE_SIZE */
		hwq_attr.res = res;
		hwq_attr.depth = mrinfo->sg.npages;
		hwq_attr.stride = PAGE_SIZE;
		hwq_attr.type = HWQ_TYPE_MR;
		hwq_attr.sginfo = &mrinfo->sg;
		rc = bnxt_qplib_alloc_init_hwq(&mr->hwq, &hwq_attr);
		if (rc) {
			dev_err(&res->pdev->dev,
				"SP: Reg MR memory allocation failed\n");
			return -ENOMEM;
		}
	}

	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_REGISTER_MR,
				 sizeof(req));
	/* Configure the request */
	if (mrinfo->is_dma) {
		/* No PBL provided, just use system PAGE_SIZE */
		level = 0;
		req.pbl = 0;
		pg_size = PAGE_SIZE;
	} else {
		level = mr->hwq.level;
		req.pbl = cpu_to_le64(mr->hwq.pbl[PBL_LVL_0].pg_map_arr[0]);
	}

	pg_size = buf_pg_size ? buf_pg_size : PAGE_SIZE;
	req.log2_pg_size_lvl = (level << CMDQ_REGISTER_MR_LVL_SFT) |
			       ((ilog2(pg_size) <<
				 CMDQ_REGISTER_MR_LOG2_PG_SIZE_SFT) &
				CMDQ_REGISTER_MR_LOG2_PG_SIZE_MASK);
	req.log2_pbl_pg_size = cpu_to_le16(((ilog2(PAGE_SIZE) <<
				 CMDQ_REGISTER_MR_LOG2_PBL_PG_SIZE_SFT) &
				CMDQ_REGISTER_MR_LOG2_PBL_PG_SIZE_MASK));
	req.access = (mr->flags & 0xFFFF);
	req.va = cpu_to_le64(mr->va);
	req.key = cpu_to_le32(mr->lkey);
	if (_is_alloc_mr_unified(res->dattr)) {
		flags = 0;
		req.key = cpu_to_le32(mr->pd->id);
		flags |= CMDQ_REGISTER_MR_FLAGS_ALLOC_MR;
		req.flags = cpu_to_le16(flags);
	}
	req.mr_size = cpu_to_le64(mr->total_size);

	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, NULL, sizeof(req),
				sizeof(resp), block);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	if (rc)
		goto fail;

	if (_is_alloc_mr_unified(res->dattr)) {
		mr->lkey = le32_to_cpu(resp.xid);
		mr->rkey = mr->lkey;
	}

	return 0;
fail:
	if (mr->hwq.max_elements)
		bnxt_qplib_free_hwq(res, &mr->hwq);
	return rc;
}

int bnxt_qplib_alloc_fast_reg_page_list(struct bnxt_qplib_res *res,
					struct bnxt_qplib_frpl *frpl,
					int max_pg_ptrs)
{
	struct bnxt_qplib_hwq_attr hwq_attr = {};
	struct bnxt_qplib_sg_info sginfo = {};
	int pg_ptrs, rc;

	/* Re-calculate the max to fit the HWQ allocation model */
	pg_ptrs = roundup_pow_of_two(max_pg_ptrs);

	sginfo.pgsize = PAGE_SIZE;
	sginfo.nopte = true;

	hwq_attr.res = res;
	hwq_attr.depth = pg_ptrs;
	hwq_attr.stride = PAGE_SIZE;
	hwq_attr.sginfo = &sginfo;
	hwq_attr.type = HWQ_TYPE_CTX;
	rc = bnxt_qplib_alloc_init_hwq(&frpl->hwq, &hwq_attr);
	if (!rc)
		frpl->max_pg_ptrs = pg_ptrs;

	return rc;
}

void bnxt_qplib_free_fast_reg_page_list(struct bnxt_qplib_res *res,
					struct bnxt_qplib_frpl *frpl)
{
	bnxt_qplib_free_hwq(res, &frpl->hwq);
}

int bnxt_qplib_map_tc2cos(struct bnxt_qplib_res *res, u16 *cids)
{
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct creq_map_tc_to_cos_resp resp = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct cmdq_map_tc_to_cos req = {};
	int rc;

	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_MAP_TC_TO_COS,
				 sizeof(req));
	req.cos0 = cpu_to_le16(cids[0]);
	req.cos1 = cpu_to_le16(cids[1]);

	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, NULL, sizeof(req),
				sizeof(resp), 0);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	return rc;
}

static void bnxt_qplib_fill_cc_gen1(struct cmdq_modify_roce_cc_gen1_tlv *ext_req,
			     struct bnxt_qplib_cc_param_ext *cc_ext)
{
	ext_req->modify_mask = cpu_to_le64(cc_ext->ext_mask);
	cc_ext->ext_mask = 0;
	ext_req->inactivity_th_hi = cpu_to_le16(cc_ext->inact_th_hi);
	ext_req->min_time_between_cnps = cpu_to_le16(cc_ext->min_delta_cnp);
	ext_req->init_cp = cpu_to_le16(cc_ext->init_cp);
	ext_req->tr_update_mode = cc_ext->tr_update_mode;
	ext_req->tr_update_cycles = cc_ext->tr_update_cyls;
	ext_req->fr_num_rtts = cc_ext->fr_rtt;
	ext_req->ai_rate_increase = cc_ext->ai_rate_incr;
	ext_req->reduction_relax_rtts_th = cpu_to_le16(cc_ext->rr_rtt_th);
	ext_req->additional_relax_cr_th = cpu_to_le16(cc_ext->ar_cr_th);
	ext_req->cr_min_th = cpu_to_le16(cc_ext->cr_min_th);
	ext_req->bw_avg_weight = cc_ext->bw_avg_weight;
	ext_req->actual_cr_factor = cc_ext->cr_factor;
	ext_req->max_cp_cr_th = cpu_to_le16(cc_ext->cr_th_max_cp);
	ext_req->cp_bias_en = cc_ext->cp_bias_en;
	ext_req->cp_bias = cc_ext->cp_bias;
	ext_req->cnp_ecn = cc_ext->cnp_ecn;
	ext_req->rtt_jitter_en = cc_ext->rtt_jitter_en;
	ext_req->link_bytes_per_usec = cpu_to_le16(cc_ext->bytes_per_usec);
	ext_req->reset_cc_cr_th = cpu_to_le16(cc_ext->cc_cr_reset_th);
	ext_req->cr_width = cc_ext->cr_width;
	ext_req->quota_period_min = cc_ext->min_quota;
	ext_req->quota_period_max = cc_ext->max_quota;
	ext_req->quota_period_abs_max = cc_ext->abs_max_quota;
	ext_req->tr_lower_bound = cpu_to_le16(cc_ext->tr_lb);
	ext_req->cr_prob_factor = cc_ext->cr_prob_fac;
	ext_req->tr_prob_factor = cc_ext->tr_prob_fac;
	ext_req->fairness_cr_th = cpu_to_le16(cc_ext->fair_cr_th);
	ext_req->red_div = cc_ext->red_div;
	ext_req->cnp_ratio_th = cc_ext->cnp_ratio_th;
	ext_req->exp_ai_rtts = cpu_to_le16(cc_ext->ai_ext_rtt);
	ext_req->exp_ai_cr_cp_ratio = cc_ext->exp_crcp_ratio;
	ext_req->use_rate_table = cc_ext->low_rate_en;
	ext_req->cp_exp_update_th = cpu_to_le16(cc_ext->cpcr_update_th);
	ext_req->high_exp_ai_rtts_th1 = cpu_to_le16(cc_ext->ai_rtt_th1);
	ext_req->high_exp_ai_rtts_th2 = cpu_to_le16(cc_ext->ai_rtt_th2);
	ext_req->actual_cr_cong_free_rtts_th = cpu_to_le16(cc_ext->cf_rtt_th);
	ext_req->severe_cong_cr_th1 = cpu_to_le16(cc_ext->sc_cr_th1);
	ext_req->severe_cong_cr_th2 = cpu_to_le16(cc_ext->sc_cr_th2);
	ext_req->link64B_per_rtt = cpu_to_le32(cc_ext->l64B_per_rtt);
	ext_req->cc_ack_bytes = cc_ext->cc_ack_bytes;
	ext_req->reduce_init_cong_free_rtts_th = cpu_to_le16(cc_ext->reduce_cf_rtt_th);
}

int bnxt_qplib_modify_cc(struct bnxt_qplib_res *res,
			 struct bnxt_qplib_cc_param *cc_param)
{
	struct bnxt_qplib_tlv_modify_cc_req tlv_req = {};
	struct creq_modify_roce_cc_resp resp = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct cmdq_modify_roce_cc *req;
	int req_size;
	void *cmd;
	int rc;

	/* Prepare the older base command */
	req = &tlv_req.base_req;
	cmd = req;
	req_size = sizeof(*req);
	bnxt_qplib_rcfw_cmd_prep(req, CMDQ_BASE_OPCODE_MODIFY_ROCE_CC,
				 sizeof(*req));
	req->modify_mask = cpu_to_le32(cc_param->mask);
	req->enable_cc = cc_param->enable;
	req->g = cc_param->g;
	req->num_phases_per_state = cc_param->nph_per_state;
	req->time_per_phase = cc_param->time_pph;
	req->pkts_per_phase = cc_param->pkts_pph;
	req->init_cr = cpu_to_le16(cc_param->init_cr);
	req->init_tr = cpu_to_le16(cc_param->init_tr);
	req->tos_dscp_tos_ecn = (cc_param->tos_dscp <<
				 CMDQ_MODIFY_ROCE_CC_TOS_DSCP_SFT) |
				 (cc_param->tos_ecn &
				 CMDQ_MODIFY_ROCE_CC_TOS_ECN_MASK);
	req->alt_vlan_pcp = cc_param->alt_vlan_pcp;
	req->alt_tos_dscp = cpu_to_le16(cc_param->alt_tos_dscp);
	req->rtt = cpu_to_le16(cc_param->rtt);
	req->tcp_cp = cpu_to_le16(cc_param->tcp_cp);
	req->cc_mode = cc_param->cc_mode;
	req->inactivity_th = cpu_to_le16(cc_param->inact_th);

	/* For chip gen P5 onwards fill extended cmd and header */
	if (_is_chip_gen_p5_p7(res->cctx)) {
		struct roce_tlv *hdr;
		u32 payload;
		u32 chunks;

		cmd = &tlv_req;
		req_size = sizeof(tlv_req);
		/* Prepare primary tlv header */
		hdr = &tlv_req.tlv_hdr;
		chunks = CHUNKS(sizeof(struct bnxt_qplib_tlv_modify_cc_req));
		payload = sizeof(struct cmdq_modify_roce_cc);
		ROCE_1ST_TLV_PREP(hdr, chunks, payload, true);
		/* Prepare secondary tlv header */
		hdr = (struct roce_tlv *)&tlv_req.ext_req;
		payload = sizeof(struct cmdq_modify_roce_cc_gen1_tlv) -
			  sizeof(struct roce_tlv);
		ROCE_EXT_TLV_PREP(hdr, TLV_TYPE_MODIFY_ROCE_CC_GEN1, payload,
				  false, true);
		bnxt_qplib_fill_cc_gen1(&tlv_req.ext_req, &cc_param->cc_ext);
	}

	bnxt_qplib_fill_cmdqmsg(&msg, cmd, &resp, NULL, req_size,
				sizeof(resp), 0);
	rc = bnxt_qplib_rcfw_send_message(res->rcfw, &msg);
	return rc;
}

static void bnxt_qplib_read_cc_gen1(struct bnxt_qplib_cc_param_ext *cc_ext,
			     struct creq_query_roce_cc_gen1_resp_sb_tlv *sb)
{
	cc_ext->inact_th_hi = le16_to_cpu(sb->inactivity_th_hi);
	cc_ext->min_delta_cnp = le16_to_cpu(sb->min_time_between_cnps);
	cc_ext->init_cp = le16_to_cpu(sb->init_cp);
	cc_ext->tr_update_mode = sb->tr_update_mode;
	cc_ext->tr_update_cyls = sb->tr_update_cycles;
	cc_ext->fr_rtt = sb->fr_num_rtts;
	cc_ext->ai_rate_incr = sb->ai_rate_increase;
	cc_ext->rr_rtt_th = le16_to_cpu(sb->reduction_relax_rtts_th);
	cc_ext->ar_cr_th = le16_to_cpu(sb->additional_relax_cr_th);
	cc_ext->cr_min_th = le16_to_cpu(sb->cr_min_th);
	cc_ext->bw_avg_weight = sb->bw_avg_weight;
	cc_ext->cr_factor = sb->actual_cr_factor;
	cc_ext->cr_th_max_cp = le16_to_cpu(sb->max_cp_cr_th);
	cc_ext->cp_bias_en = sb->cp_bias_en;
	cc_ext->cp_bias = sb->cp_bias;
	cc_ext->cnp_ecn = sb->cnp_ecn;
	cc_ext->rtt_jitter_en = sb->rtt_jitter_en;
	cc_ext->bytes_per_usec = le16_to_cpu(sb->link_bytes_per_usec);
	cc_ext->cc_cr_reset_th = le16_to_cpu(sb->reset_cc_cr_th);
	cc_ext->cr_width = sb->cr_width;
	cc_ext->min_quota = sb->quota_period_min;
	cc_ext->max_quota = sb->quota_period_max;
	cc_ext->abs_max_quota = sb->quota_period_abs_max;
	cc_ext->tr_lb = le16_to_cpu(sb->tr_lower_bound);
	cc_ext->cr_prob_fac = sb->cr_prob_factor;
	cc_ext->tr_prob_fac = sb->tr_prob_factor;
	cc_ext->fair_cr_th = le16_to_cpu(sb->fairness_cr_th);
	cc_ext->red_div = sb->red_div;
	cc_ext->cnp_ratio_th = sb->cnp_ratio_th;
	cc_ext->ai_ext_rtt = le16_to_cpu(sb->exp_ai_rtts);
	cc_ext->exp_crcp_ratio = sb->exp_ai_cr_cp_ratio;
	cc_ext->low_rate_en = sb->use_rate_table;
	cc_ext->cpcr_update_th = le16_to_cpu(sb->cp_exp_update_th);
	cc_ext->ai_rtt_th1 = le16_to_cpu(sb->high_exp_ai_rtts_th1);
	cc_ext->ai_rtt_th2 = le16_to_cpu(sb->high_exp_ai_rtts_th2);
	cc_ext->cf_rtt_th = le16_to_cpu(sb->actual_cr_cong_free_rtts_th);
	cc_ext->sc_cr_th1 = le16_to_cpu(sb->severe_cong_cr_th1);
	cc_ext->sc_cr_th2 = le16_to_cpu(sb->severe_cong_cr_th2);
	cc_ext->l64B_per_rtt = le32_to_cpu(sb->link64B_per_rtt);
	cc_ext->cc_ack_bytes = sb->cc_ack_bytes;
	cc_ext->reduce_cf_rtt_th = le16_to_cpu(sb->reduce_init_cong_free_rtts_th);
}

int bnxt_qplib_query_cc_param(struct bnxt_qplib_res *res,
			      struct bnxt_qplib_cc_param *cc_param)
{
	struct creq_query_roce_cc_gen1_resp_sb_tlv *gen1_sb;
	struct bnxt_qplib_tlv_query_rcc_sb *ext_sb;
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct creq_query_roce_cc_resp resp = {};
	struct creq_query_roce_cc_resp_sb *sb;
	struct bnxt_qplib_cmdqmsg msg = {};
	struct cmdq_query_roce_cc req = {};
	struct bnxt_qplib_rcfw_sbuf sbuf;
	size_t resp_size;
	int rc;

	/* Query the parameters from chip */
	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_QUERY_ROCE_CC,
				 sizeof(req));
	if (_is_chip_gen_p5_p7(res->cctx))
		resp_size = sizeof(*ext_sb);
	else
		resp_size = sizeof(*sb);
	sbuf.size = ALIGN(resp_size, BNXT_QPLIB_CMDQE_UNITS);
	sbuf.sb = dma_zalloc_coherent(&rcfw->pdev->dev, sbuf.size,
				       &sbuf.dma_addr, GFP_KERNEL);
	if (!sbuf.sb)
		return -ENOMEM;

	req.resp_size = sbuf.size / BNXT_QPLIB_CMDQE_UNITS;
	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, &sbuf, sizeof(req),
				sizeof(resp), 0);
	rc = bnxt_qplib_rcfw_send_message(res->rcfw, &msg);
	if (rc) {
		dev_dbg(&res->pdev->dev, "%s:Query CC param failed:0x%x\n",
			__func__, rc);
		goto out;
	}

	ext_sb = sbuf.sb;
	gen1_sb = &ext_sb->gen1_sb;
	sb = _is_chip_gen_p5_p7(res->cctx) ? &ext_sb->base_sb :
		(struct creq_query_roce_cc_resp_sb *)ext_sb;

	cc_param->enable = sb->enable_cc & CREQ_QUERY_ROCE_CC_RESP_SB_ENABLE_CC;
	cc_param->tos_ecn = (sb->tos_dscp_tos_ecn &
			     CREQ_QUERY_ROCE_CC_RESP_SB_TOS_ECN_MASK) >>
			     CREQ_QUERY_ROCE_CC_RESP_SB_TOS_ECN_SFT;
	cc_param->tos_dscp = (sb->tos_dscp_tos_ecn &
			      CREQ_QUERY_ROCE_CC_RESP_SB_TOS_DSCP_MASK) >>
			      CREQ_QUERY_ROCE_CC_RESP_SB_TOS_DSCP_SFT;
	cc_param->alt_tos_dscp = sb->alt_tos_dscp;
	cc_param->alt_vlan_pcp = sb->alt_vlan_pcp;

	cc_param->g = sb->g;
	cc_param->nph_per_state = sb->num_phases_per_state;
	cc_param->init_cr = le16_to_cpu(sb->init_cr);
	cc_param->init_tr = le16_to_cpu(sb->init_tr);
	cc_param->cc_mode = sb->cc_mode;
	cc_param->inact_th = le16_to_cpu(sb->inactivity_th);
	cc_param->rtt = le16_to_cpu(sb->rtt);
	cc_param->tcp_cp = le16_to_cpu(sb->tcp_cp);
	cc_param->time_pph = sb->time_per_phase;
	cc_param->pkts_pph = sb->pkts_per_phase;
	if (_is_chip_gen_p5_p7(res->cctx))
		bnxt_qplib_read_cc_gen1(&cc_param->cc_ext, gen1_sb);
out:
	dma_free_coherent(&rcfw->pdev->dev, sbuf.size,
				  sbuf.sb, sbuf.dma_addr);
	return rc;
}


int bnxt_qplib_get_roce_error_stats(struct bnxt_qplib_rcfw *rcfw,
				    struct bnxt_qplib_roce_stats *stats,
				    struct bnxt_qplib_query_stats_info *sinfo)
{
	struct creq_query_roce_stats_resp resp = {};
	struct creq_query_roce_stats_resp_sb *sb;
	struct cmdq_query_roce_stats req = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct bnxt_qplib_rcfw_sbuf sbuf;
	u16 cmd_flags = 0;
	u32 fn_id = 0;
	int rc = 0;

	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_QUERY_ROCE_STATS,
				 sizeof(req));

	sbuf.size = sizeof(*sb);
	sbuf.sb = dma_zalloc_coherent(&rcfw->pdev->dev, sbuf.size,
				       &sbuf.dma_addr, GFP_KERNEL);
	if (!sbuf.sb)
		return -ENOMEM;
	sb = sbuf.sb;

	if (rcfw->res->cctx->hwrm_intf_ver >= HWRM_VERSION_ROCE_STATS_FN_ID) {
		if (sinfo->function_id != 0xFFFFFFFF) {
			cmd_flags = CMDQ_QUERY_ROCE_STATS_FLAGS_FUNCTION_ID;
			if (sinfo->vf_valid) {
				fn_id = CMDQ_QUERY_ROCE_STATS_VF_VALID;
				fn_id |= (sinfo->function_id <<
					  CMDQ_QUERY_ROCE_STATS_VF_NUM_SFT) &
					  CMDQ_QUERY_ROCE_STATS_VF_NUM_MASK;
			} else {
				fn_id = sinfo->function_id &
					CMDQ_QUERY_ROCE_STATS_PF_NUM_MASK;
			}
		}

		req.flags = cpu_to_le16(cmd_flags);
		req.function_id = cpu_to_le32(fn_id);

		if (sinfo->collection_id != 0xFF) {
			cmd_flags |= CMDQ_QUERY_ROCE_STATS_FLAGS_COLLECTION_ID;
			req.collection_id = sinfo->collection_id;
		}
	} else {
		/* For older HWRM version, the command length has to be
		 * adjusted. 8 bytes are more in the newer command.
		 * So subtract these 8 bytes for older HWRM version.
		 * command units are adjusted inside
		 * bnxt_qplib_rcfw_send_message.
		 */
		req.cmd_size -= 8;
	}

	req.resp_size = sbuf.size / BNXT_QPLIB_CMDQE_UNITS;
	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, &sbuf, sizeof(req),
				sizeof(resp), 0);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	if (rc)
		goto bail;
	/* Extract the context from the side buffer */
	stats->to_retransmits = le64_to_cpu(sb->to_retransmits);
	stats->seq_err_naks_rcvd = le64_to_cpu(sb->seq_err_naks_rcvd);
	stats->max_retry_exceeded = le64_to_cpu(sb->max_retry_exceeded);
	stats->rnr_naks_rcvd = le64_to_cpu(sb->rnr_naks_rcvd);
	stats->missing_resp = le64_to_cpu(sb->missing_resp);
	stats->unrecoverable_err = le64_to_cpu(sb->unrecoverable_err);
	stats->bad_resp_err = le64_to_cpu(sb->bad_resp_err);
	stats->local_qp_op_err = le64_to_cpu(sb->local_qp_op_err);
	stats->local_protection_err = le64_to_cpu(sb->local_protection_err);
	stats->mem_mgmt_op_err = le64_to_cpu(sb->mem_mgmt_op_err);
	stats->remote_invalid_req_err = le64_to_cpu(sb->remote_invalid_req_err);
	stats->remote_access_err = le64_to_cpu(sb->remote_access_err);
	stats->remote_op_err = le64_to_cpu(sb->remote_op_err);
	stats->dup_req = le64_to_cpu(sb->dup_req);
	stats->res_exceed_max = le64_to_cpu(sb->res_exceed_max);
	stats->res_length_mismatch = le64_to_cpu(sb->res_length_mismatch);
	stats->res_exceeds_wqe = le64_to_cpu(sb->res_exceeds_wqe);
	stats->res_opcode_err = le64_to_cpu(sb->res_opcode_err);
	stats->res_rx_invalid_rkey = le64_to_cpu(sb->res_rx_invalid_rkey);
	stats->res_rx_domain_err = le64_to_cpu(sb->res_rx_domain_err);
	stats->res_rx_no_perm = le64_to_cpu(sb->res_rx_no_perm);
	stats->res_rx_range_err = le64_to_cpu(sb->res_rx_range_err);
	stats->res_tx_invalid_rkey = le64_to_cpu(sb->res_tx_invalid_rkey);
	stats->res_tx_domain_err = le64_to_cpu(sb->res_tx_domain_err);
	stats->res_tx_no_perm = le64_to_cpu(sb->res_tx_no_perm);
	stats->res_tx_range_err = le64_to_cpu(sb->res_tx_range_err);
	stats->res_irrq_oflow = le64_to_cpu(sb->res_irrq_oflow);
	stats->res_unsup_opcode = le64_to_cpu(sb->res_unsup_opcode);
	stats->res_unaligned_atomic = le64_to_cpu(sb->res_unaligned_atomic);
	stats->res_rem_inv_err = le64_to_cpu(sb->res_rem_inv_err);
	stats->res_mem_error = le64_to_cpu(sb->res_mem_error);
	stats->res_srq_err = le64_to_cpu(sb->res_srq_err);
	stats->res_cmp_err = le64_to_cpu(sb->res_cmp_err);
	stats->res_invalid_dup_rkey = le64_to_cpu(sb->res_invalid_dup_rkey);
	stats->res_wqe_format_err = le64_to_cpu(sb->res_wqe_format_err);
	stats->res_cq_load_err = le64_to_cpu(sb->res_cq_load_err);
	stats->res_srq_load_err = le64_to_cpu(sb->res_srq_load_err);
	stats->res_tx_pci_err = le64_to_cpu(sb->res_tx_pci_err);
	stats->res_rx_pci_err = le64_to_cpu(sb->res_rx_pci_err);

	if (!rcfw->init_oos_stats) {
		rcfw->oos_prev = le64_to_cpu(sb->res_oos_drop_count);
		rcfw->init_oos_stats = true;
	} else {
		stats->res_oos_drop_count += (le64_to_cpu(sb->res_oos_drop_count) -
					      rcfw->oos_prev) &
					     BNXT_QPLIB_OOS_COUNT_MASK;
		rcfw->oos_prev = le64_to_cpu(sb->res_oos_drop_count);
	}

	stats->active_qp_count_p0 = le64_to_cpu(sb->active_qp_count_p0);
	stats->active_qp_count_p1 = le64_to_cpu(sb->active_qp_count_p1);
	stats->active_qp_count_p2 = le64_to_cpu(sb->active_qp_count_p2);
	stats->active_qp_count_p3 = le64_to_cpu(sb->active_qp_count_p3);
bail:
	dma_free_coherent(&rcfw->pdev->dev, sbuf.size,
				  sbuf.sb, sbuf.dma_addr);
	return rc;
}

int bnxt_qplib_set_link_aggr_mode(struct bnxt_qplib_res *res,
				  u8 aggr_mode, u8 member_port_map,
				  u8 active_port_map, bool aggr_en,
				  u32 stats_fw_id)
{
	struct creq_set_link_aggr_mode_resources_resp resp = {};
	struct cmdq_set_link_aggr_mode_cc req = {};
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct bnxt_qplib_cmdqmsg msg = {};
	int rc = 0;

	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_SET_LINK_AGGR_MODE,
				 sizeof(req));

	req.aggr_enable = aggr_en;
	req.active_port_map = active_port_map;
	req.member_port_map = member_port_map;
	req.link_aggr_mode = aggr_mode;

	/* need to specify only second port stats ctx id for now */
	req.stat_ctx_id[1] = cpu_to_le16((u16)(stats_fw_id));

	req.modify_mask =
		cpu_to_le32(CMDQ_SET_LINK_AGGR_MODE_MODIFY_MASK_AGGR_EN |
			    CMDQ_SET_LINK_AGGR_MODE_MODIFY_MASK_ACTIVE_PORT_MAP |
			    CMDQ_SET_LINK_AGGR_MODE_MODIFY_MASK_MEMBER_PORT_MAP |
			    CMDQ_SET_LINK_AGGR_MODE_MODIFY_MASK_AGGR_MODE |
			    CMDQ_SET_LINK_AGGR_MODE_MODIFY_MASK_STAT_CTX_ID);

	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, NULL, sizeof(req),
				sizeof(resp), 0);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	if (rc)
		dev_err(&res->pdev->dev,
			"QPLIB: Failed to set link aggr mode, %#x\n", rc);

	return rc;
}

int bnxt_qplib_qext_stat(struct bnxt_qplib_rcfw *rcfw, u32 fid,
			 struct bnxt_qplib_ext_stat *estat,
			 struct bnxt_qplib_query_stats_info *sinfo)
{
	struct creq_query_roce_stats_ext_resp resp = {};
	struct creq_query_roce_stats_ext_resp_sb *sb;
	struct cmdq_query_roce_stats_ext req = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct bnxt_qplib_rcfw_sbuf sbuf;
	int rc;

	sbuf.size = sizeof(*sb);
	sbuf.sb = dma_zalloc_coherent(&rcfw->pdev->dev, sbuf.size,
				       &sbuf.dma_addr, GFP_KERNEL);
	if (!sbuf.sb) {
		dev_err(&rcfw->pdev->dev,
			"QPLIB: SP: QUERY_ROCE_STATS_EXT alloc sb failed\n");
		return -ENOMEM;
	}
	sb = sbuf.sb;

	bnxt_qplib_rcfw_cmd_prep(&req,
			CMDQ_QUERY_ROCE_STATS_EXT_OPCODE_QUERY_ROCE_STATS,
			sizeof(req));
	req.resp_size = sbuf.size;
	req.resp_addr = cpu_to_le64(sbuf.dma_addr);
	req.flags = cpu_to_le16(CMDQ_QUERY_ROCE_STATS_EXT_FLAGS_FUNCTION_ID);
	if (_is_chip_p7(rcfw->res->cctx) && rcfw->res->is_vf) {
		if (sinfo->vf_valid)
			req.function_id =
				cpu_to_le32(CMDQ_QUERY_ROCE_STATS_EXT_VF_VALID |
					    (fid << CMDQ_QUERY_ROCE_STATS_EXT_VF_NUM_SFT));
		else
			req.flags = cpu_to_le16(0);
	} else {
		req.function_id = cpu_to_le32(fid);
	}

	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, &sbuf, sizeof(req),
				sizeof(resp), 0);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	if (rc)
		goto bail;

	/* dump when dyndbg is enabled */
	print_hex_dump_bytes("", DUMP_PREFIX_OFFSET, sb, sizeof(*sb));
	estat->tx_atomic_req = le64_to_cpu(sb->tx_atomic_req_pkts);
	estat->tx_read_req = le64_to_cpu(sb->tx_read_req_pkts);
	estat->tx_read_res = le64_to_cpu(sb->tx_read_res_pkts);
	estat->tx_write_req = le64_to_cpu(sb->tx_write_req_pkts);
	estat->tx_send_req = le64_to_cpu(sb->tx_send_req_pkts);
	estat->tx_roce_pkts = le64_to_cpu(sb->tx_roce_pkts);
	estat->tx_roce_bytes = le64_to_cpu(sb->tx_roce_bytes);
	estat->rx_atomic_req = le64_to_cpu(sb->rx_atomic_req_pkts);
	estat->rx_read_req = le64_to_cpu(sb->rx_read_req_pkts);
	estat->rx_read_res = le64_to_cpu(sb->rx_read_res_pkts);
	estat->rx_write_req = le64_to_cpu(sb->rx_write_req_pkts);
	estat->rx_send_req = le64_to_cpu(sb->rx_send_req_pkts);
	estat->rx_roce_pkts = le64_to_cpu(sb->rx_roce_pkts);
	estat->rx_roce_bytes = le64_to_cpu(sb->rx_roce_bytes);
	estat->rx_roce_good_pkts = le64_to_cpu(sb->rx_roce_good_pkts);
	estat->rx_roce_good_bytes = le64_to_cpu(sb->rx_roce_good_bytes);
	estat->rx_out_of_buffer = le64_to_cpu(sb->rx_out_of_buffer_pkts);
	estat->rx_out_of_sequence = le64_to_cpu(sb->rx_out_of_sequence_pkts);
	estat->tx_cnp = le64_to_cpu(sb->tx_cnp_pkts);
	estat->rx_cnp = le64_to_cpu(sb->rx_cnp_pkts);
	estat->rx_ecn_marked = le64_to_cpu(sb->rx_ecn_marked_pkts);
	estat->seq_err_naks_rcvd = le64_to_cpu(sb->seq_err_naks_rcvd);
	estat->rnr_naks_rcvd = le64_to_cpu(sb->rnr_naks_rcvd);
	estat->missing_resp = le64_to_cpu(sb->missing_resp);
	estat->to_retransmits = le64_to_cpu(sb->to_retransmit);
	estat->dup_req = le64_to_cpu(sb->dup_req);
	estat->rx_dcn_payload_cut = le64_to_cpu(sb->rx_dcn_payload_cut);
	estat->te_bypassed = le64_to_cpu(sb->te_bypassed);
bail:
	dma_free_coherent(&rcfw->pdev->dev, sbuf.size,
				  sbuf.sb, sbuf.dma_addr);
	return rc;
}
