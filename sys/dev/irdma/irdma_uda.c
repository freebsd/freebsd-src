/*-
 * SPDX-License-Identifier: GPL-2.0 or Linux-OpenIB
 *
 * Copyright (c) 2016 - 2021 Intel Corporation
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
/*$FreeBSD$*/

#include "osdep.h"
#include "irdma_hmc.h"
#include "irdma_defs.h"
#include "irdma_type.h"
#include "irdma_protos.h"
#include "irdma_uda.h"
#include "irdma_uda_d.h"

/**
 * irdma_sc_access_ah() - Create, modify or delete AH
 * @cqp: struct for cqp hw
 * @info: ah information
 * @op: Operation
 * @scratch: u64 saved to be used during cqp completion
 */
int
irdma_sc_access_ah(struct irdma_sc_cqp *cqp, struct irdma_ah_info *info,
		   u32 op, u64 scratch)
{
	__le64 *wqe;
	u64 qw1, qw2;

	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;

	set_64bit_val(wqe, IRDMA_BYTE_0, LS_64_1(info->mac_addr[5], 16) |
		      LS_64_1(info->mac_addr[4], 24) |
		      LS_64_1(info->mac_addr[3], 32) |
		      LS_64_1(info->mac_addr[2], 40) |
		      LS_64_1(info->mac_addr[1], 48) |
		      LS_64_1(info->mac_addr[0], 56));

	qw1 = FIELD_PREP(IRDMA_UDA_CQPSQ_MAV_PDINDEXLO, info->pd_idx) |
	    FIELD_PREP(IRDMA_UDA_CQPSQ_MAV_TC, info->tc_tos) |
	    FIELD_PREP(IRDMA_UDAQPC_VLANTAG, info->vlan_tag);

	qw2 = FIELD_PREP(IRDMA_UDA_CQPSQ_MAV_ARPINDEX, info->dst_arpindex) |
	    FIELD_PREP(IRDMA_UDA_CQPSQ_MAV_FLOWLABEL, info->flow_label) |
	    FIELD_PREP(IRDMA_UDA_CQPSQ_MAV_HOPLIMIT, info->hop_ttl) |
	    FIELD_PREP(IRDMA_UDA_CQPSQ_MAV_PDINDEXHI, info->pd_idx >> 16);

	if (!info->ipv4_valid) {
		set_64bit_val(wqe, IRDMA_BYTE_40,
			      FIELD_PREP(IRDMA_UDA_CQPSQ_MAV_ADDR0, info->dest_ip_addr[0]) |
			      FIELD_PREP(IRDMA_UDA_CQPSQ_MAV_ADDR1, info->dest_ip_addr[1]));
		set_64bit_val(wqe, IRDMA_BYTE_32,
			      FIELD_PREP(IRDMA_UDA_CQPSQ_MAV_ADDR2, info->dest_ip_addr[2]) |
			      FIELD_PREP(IRDMA_UDA_CQPSQ_MAV_ADDR3, info->dest_ip_addr[3]));

		set_64bit_val(wqe, IRDMA_BYTE_56,
			      FIELD_PREP(IRDMA_UDA_CQPSQ_MAV_ADDR0, info->src_ip_addr[0]) |
			      FIELD_PREP(IRDMA_UDA_CQPSQ_MAV_ADDR1, info->src_ip_addr[1]));
		set_64bit_val(wqe, IRDMA_BYTE_48,
			      FIELD_PREP(IRDMA_UDA_CQPSQ_MAV_ADDR2, info->src_ip_addr[2]) |
			      FIELD_PREP(IRDMA_UDA_CQPSQ_MAV_ADDR3, info->src_ip_addr[3]));
	} else {
		set_64bit_val(wqe, IRDMA_BYTE_32,
			      FIELD_PREP(IRDMA_UDA_CQPSQ_MAV_ADDR3, info->dest_ip_addr[0]));

		set_64bit_val(wqe, IRDMA_BYTE_48,
			      FIELD_PREP(IRDMA_UDA_CQPSQ_MAV_ADDR3, info->src_ip_addr[0]));
	}

	set_64bit_val(wqe, IRDMA_BYTE_8, qw1);
	set_64bit_val(wqe, IRDMA_BYTE_16, qw2);

	irdma_wmb();		/* need write block before writing WQE header */

	set_64bit_val(
		      wqe, IRDMA_BYTE_24,
		      FIELD_PREP(IRDMA_UDA_CQPSQ_MAV_WQEVALID, cqp->polarity) |
		      FIELD_PREP(IRDMA_UDA_CQPSQ_MAV_OPCODE, op) |
		      FIELD_PREP(IRDMA_UDA_CQPSQ_MAV_DOLOOPBACKK, info->do_lpbk) |
		      FIELD_PREP(IRDMA_UDA_CQPSQ_MAV_IPV4VALID, info->ipv4_valid) |
		      FIELD_PREP(IRDMA_UDA_CQPSQ_MAV_AVIDX, info->ah_idx) |
		      FIELD_PREP(IRDMA_UDA_CQPSQ_MAV_INSERTVLANTAG, info->insert_vlan_tag));

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "MANAGE_AH WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);
	irdma_sc_cqp_post_sq(cqp);

	return 0;
}

/**
 * irdma_create_mg_ctx() - create a mcg context
 * @info: multicast group context info
 */
static int
irdma_create_mg_ctx(struct irdma_mcast_grp_info *info)
{
	struct irdma_mcast_grp_ctx_entry_info *entry_info = NULL;
	u8 idx = 0;		/* index in the array */
	u8 ctx_idx = 0;		/* index in the MG context */

	memset(info->dma_mem_mc.va, 0, IRDMA_MAX_MGS_PER_CTX * sizeof(u64));

	for (idx = 0; idx < IRDMA_MAX_MGS_PER_CTX; idx++) {
		entry_info = &info->mg_ctx_info[idx];
		if (entry_info->valid_entry) {
			set_64bit_val((__le64 *) info->dma_mem_mc.va,
				      ctx_idx * sizeof(u64),
				      FIELD_PREP(IRDMA_UDA_MGCTX_DESTPORT, entry_info->dest_port) |
				      FIELD_PREP(IRDMA_UDA_MGCTX_VALIDENT, entry_info->valid_entry) |
				      FIELD_PREP(IRDMA_UDA_MGCTX_QPID, entry_info->qp_id));
			ctx_idx++;
		}
	}

	return 0;
}

/**
 * irdma_access_mcast_grp() - Access mcast group based on op
 * @cqp: Control QP
 * @info: multicast group context info
 * @op: operation to perform
 * @scratch: u64 saved to be used during cqp completion
 */
int
irdma_access_mcast_grp(struct irdma_sc_cqp *cqp,
		       struct irdma_mcast_grp_info *info, u32 op,
		       u64 scratch)
{
	__le64 *wqe;
	int ret_code = 0;

	if (info->mg_id >= IRDMA_UDA_MAX_FSI_MGS) {
		irdma_debug(cqp->dev, IRDMA_DEBUG_WQE, "mg_id out of range\n");
		return -EINVAL;
	}

	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe) {
		irdma_debug(cqp->dev, IRDMA_DEBUG_WQE, "ring full\n");
		return -ENOSPC;
	}

	ret_code = irdma_create_mg_ctx(info);
	if (ret_code)
		return ret_code;

	set_64bit_val(wqe, IRDMA_BYTE_32, info->dma_mem_mc.pa);
	set_64bit_val(wqe, IRDMA_BYTE_16,
		      FIELD_PREP(IRDMA_UDA_CQPSQ_MG_VLANID, info->vlan_id) |
		      FIELD_PREP(IRDMA_UDA_CQPSQ_QS_HANDLE, info->qs_handle));
	set_64bit_val(wqe, IRDMA_BYTE_0, LS_64_1(info->dest_mac_addr[5], 0) |
		      LS_64_1(info->dest_mac_addr[4], 8) |
		      LS_64_1(info->dest_mac_addr[3], 16) |
		      LS_64_1(info->dest_mac_addr[2], 24) |
		      LS_64_1(info->dest_mac_addr[1], 32) |
		      LS_64_1(info->dest_mac_addr[0], 40));
	set_64bit_val(wqe, IRDMA_BYTE_8,
		      FIELD_PREP(IRDMA_UDA_CQPSQ_MG_HMC_FCN_ID, info->hmc_fcn_id));

	if (!info->ipv4_valid) {
		set_64bit_val(wqe, IRDMA_BYTE_56,
			      FIELD_PREP(IRDMA_UDA_CQPSQ_MAV_ADDR0, info->dest_ip_addr[0]) |
			      FIELD_PREP(IRDMA_UDA_CQPSQ_MAV_ADDR1, info->dest_ip_addr[1]));
		set_64bit_val(wqe, IRDMA_BYTE_48,
			      FIELD_PREP(IRDMA_UDA_CQPSQ_MAV_ADDR2, info->dest_ip_addr[2]) |
			      FIELD_PREP(IRDMA_UDA_CQPSQ_MAV_ADDR3, info->dest_ip_addr[3]));
	} else {
		set_64bit_val(wqe, IRDMA_BYTE_48,
			      FIELD_PREP(IRDMA_UDA_CQPSQ_MAV_ADDR3, info->dest_ip_addr[0]));
	}

	irdma_wmb();		/* need write memory block before writing the WQE header. */

	set_64bit_val(wqe, IRDMA_BYTE_24,
		      FIELD_PREP(IRDMA_UDA_CQPSQ_MG_WQEVALID, cqp->polarity) |
		      FIELD_PREP(IRDMA_UDA_CQPSQ_MG_OPCODE, op) |
		      FIELD_PREP(IRDMA_UDA_CQPSQ_MG_MGIDX, info->mg_id) |
		      FIELD_PREP(IRDMA_UDA_CQPSQ_MG_VLANVALID, info->vlan_valid) |
		      FIELD_PREP(IRDMA_UDA_CQPSQ_MG_IPV4VALID, info->ipv4_valid));

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "MANAGE_MCG WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);
	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "MCG_HOST CTX WQE",
			info->dma_mem_mc.va, IRDMA_MAX_MGS_PER_CTX * 8);
	irdma_sc_cqp_post_sq(cqp);

	return 0;
}

/**
 * irdma_compare_mgs - Compares two multicast group structures
 * @entry1: Multcast group info
 * @entry2: Multcast group info in context
 */
static bool
irdma_compare_mgs(struct irdma_mcast_grp_ctx_entry_info *entry1,
		  struct irdma_mcast_grp_ctx_entry_info *entry2)
{
	if (entry1->dest_port == entry2->dest_port &&
	    entry1->qp_id == entry2->qp_id)
		return true;

	return false;
}

/**
 * irdma_sc_add_mcast_grp - Allocates mcast group entry in ctx
 * @ctx: Multcast group context
 * @mg: Multcast group info
 */
int
irdma_sc_add_mcast_grp(struct irdma_mcast_grp_info *ctx,
		       struct irdma_mcast_grp_ctx_entry_info *mg)
{
	u32 idx;
	bool free_entry_found = false;
	u32 free_entry_idx = 0;

	/* find either an identical or a free entry for a multicast group */
	for (idx = 0; idx < IRDMA_MAX_MGS_PER_CTX; idx++) {
		if (ctx->mg_ctx_info[idx].valid_entry) {
			if (irdma_compare_mgs(&ctx->mg_ctx_info[idx], mg)) {
				ctx->mg_ctx_info[idx].use_cnt++;
				return 0;
			}
			continue;
		}
		if (!free_entry_found) {
			free_entry_found = true;
			free_entry_idx = idx;
		}
	}

	if (free_entry_found) {
		ctx->mg_ctx_info[free_entry_idx] = *mg;
		ctx->mg_ctx_info[free_entry_idx].valid_entry = true;
		ctx->mg_ctx_info[free_entry_idx].use_cnt = 1;
		ctx->no_of_mgs++;
		return 0;
	}

	return -ENOMEM;
}

/**
 * irdma_sc_del_mcast_grp - Delete mcast group
 * @ctx: Multcast group context
 * @mg: Multcast group info
 *
 * Finds and removes a specific mulicast group from context, all
 * parameters must match to remove a multicast group.
 */
int
irdma_sc_del_mcast_grp(struct irdma_mcast_grp_info *ctx,
		       struct irdma_mcast_grp_ctx_entry_info *mg)
{
	u32 idx;

	/* find an entry in multicast group context */
	for (idx = 0; idx < IRDMA_MAX_MGS_PER_CTX; idx++) {
		if (!ctx->mg_ctx_info[idx].valid_entry)
			continue;

		if (irdma_compare_mgs(mg, &ctx->mg_ctx_info[idx])) {
			ctx->mg_ctx_info[idx].use_cnt--;

			if (!ctx->mg_ctx_info[idx].use_cnt) {
				ctx->mg_ctx_info[idx].valid_entry = false;
				ctx->no_of_mgs--;
				/* Remove gap if element was not the last */
				if (idx != ctx->no_of_mgs &&
				    ctx->no_of_mgs > 0) {
					irdma_memcpy(&ctx->mg_ctx_info[idx],
						     &ctx->mg_ctx_info[ctx->no_of_mgs - 1],
						     sizeof(ctx->mg_ctx_info[idx]));
					ctx->mg_ctx_info[ctx->no_of_mgs - 1].valid_entry = false;
				}
			}

			return 0;
		}
	}

	return -EINVAL;
}
