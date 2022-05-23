/*-
 * SPDX-License-Identifier: GPL-2.0 or Linux-OpenIB
 *
 * Copyright (c) 2015 - 2022 Intel Corporation
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
#include "irdma_ws.h"
#include "irdma_protos.h"

/**
 * irdma_qp_from_entry - Given entry, get to the qp structure
 * @entry: Points to list of qp structure
 */
static struct irdma_sc_qp *
irdma_qp_from_entry(struct list_head *entry)
{
	if (!entry)
		return NULL;

	return (struct irdma_sc_qp *)((char *)entry -
				      offsetof(struct irdma_sc_qp, list));
}

/**
 * irdma_get_qp_from_list - get next qp from a list
 * @head: Listhead of qp's
 * @qp: current qp
 */
struct irdma_sc_qp *
irdma_get_qp_from_list(struct list_head *head,
		       struct irdma_sc_qp *qp)
{
	struct list_head *lastentry;
	struct list_head *entry = NULL;

	if (list_empty(head))
		return NULL;

	if (!qp) {
		entry = (head)->next;
	} else {
		lastentry = &qp->list;
		entry = (lastentry)->next;
		if (entry == head)
			return NULL;
	}

	return irdma_qp_from_entry(entry);
}

/**
 * irdma_sc_suspend_resume_qps - suspend/resume all qp's on VSI
 * @vsi: the VSI struct pointer
 * @op: Set to IRDMA_OP_RESUME or IRDMA_OP_SUSPEND
 */
void
irdma_sc_suspend_resume_qps(struct irdma_sc_vsi *vsi, u8 op)
{
	struct irdma_sc_qp *qp = NULL;
	u8 i;

	for (i = 0; i < IRDMA_MAX_USER_PRIORITY; i++) {
		mutex_lock(&vsi->qos[i].qos_mutex);
		qp = irdma_get_qp_from_list(&vsi->qos[i].qplist, qp);
		while (qp) {
			if (op == IRDMA_OP_RESUME) {
				if (!qp->dev->ws_add(vsi, i)) {
					qp->qs_handle =
					    vsi->qos[qp->user_pri].qs_handle;
					irdma_cqp_qp_suspend_resume(qp, op);
				} else {
					irdma_cqp_qp_suspend_resume(qp, op);
					irdma_modify_qp_to_err(qp);
				}
			} else if (op == IRDMA_OP_SUSPEND) {
				/* issue cqp suspend command */
				if (!irdma_cqp_qp_suspend_resume(qp, op))
					atomic_inc(&vsi->qp_suspend_reqs);
			}
			qp = irdma_get_qp_from_list(&vsi->qos[i].qplist, qp);
		}
		mutex_unlock(&vsi->qos[i].qos_mutex);
	}
}

static void
irdma_set_qos_info(struct irdma_sc_vsi *vsi, struct irdma_l2params *l2p)
{
	u8 i;

	vsi->qos_rel_bw = l2p->vsi_rel_bw;
	vsi->qos_prio_type = l2p->vsi_prio_type;
	vsi->dscp_mode = l2p->dscp_mode;
	if (l2p->dscp_mode) {
		irdma_memcpy(vsi->dscp_map, l2p->dscp_map, sizeof(vsi->dscp_map));
		for (i = 0; i < IRDMA_MAX_USER_PRIORITY; i++)
			l2p->up2tc[i] = i;
	}
	for (i = 0; i < IRDMA_MAX_TRAFFIC_CLASS; i++)
		vsi->tc_print_warning[i] = true;
	for (i = 0; i < IRDMA_MAX_USER_PRIORITY; i++) {
		if (vsi->dev->hw_attrs.uk_attrs.hw_rev == IRDMA_GEN_1)
			vsi->qos[i].qs_handle = l2p->qs_handle_list[i];
		if (vsi->dev->hw_attrs.uk_attrs.hw_rev == IRDMA_GEN_2)
			irdma_init_config_check(&vsi->cfg_check[i],
						l2p->up2tc[i],
						l2p->qs_handle_list[i]);
		vsi->qos[i].traffic_class = l2p->up2tc[i];
		vsi->qos[i].rel_bw =
		    l2p->tc_info[vsi->qos[i].traffic_class].rel_bw;
		vsi->qos[i].prio_type =
		    l2p->tc_info[vsi->qos[i].traffic_class].prio_type;
		vsi->qos[i].valid = false;
	}
}

/**
 * irdma_change_l2params - given the new l2 parameters, change all qp
 * @vsi: RDMA VSI pointer
 * @l2params: New parameters from l2
 */
void
irdma_change_l2params(struct irdma_sc_vsi *vsi,
		      struct irdma_l2params *l2params)
{
	if (l2params->mtu_changed) {
		vsi->mtu = l2params->mtu;
		if (vsi->ieq)
			irdma_reinitialize_ieq(vsi);
	}

	if (!l2params->tc_changed)
		return;

	vsi->tc_change_pending = false;
	irdma_set_qos_info(vsi, l2params);
	irdma_sc_suspend_resume_qps(vsi, IRDMA_OP_RESUME);
}

/**
 * irdma_qp_rem_qos - remove qp from qos lists during destroy qp
 * @qp: qp to be removed from qos
 */
void
irdma_qp_rem_qos(struct irdma_sc_qp *qp)
{
	struct irdma_sc_vsi *vsi = qp->vsi;

	irdma_debug(qp->dev, IRDMA_DEBUG_DCB,
		    "DCB: Remove qp[%d] UP[%d] qset[%d] on_qoslist[%d]\n",
		    qp->qp_uk.qp_id, qp->user_pri, qp->qs_handle, qp->on_qoslist);
	mutex_lock(&vsi->qos[qp->user_pri].qos_mutex);
	if (qp->on_qoslist) {
		qp->on_qoslist = false;
		list_del(&qp->list);
	}
	mutex_unlock(&vsi->qos[qp->user_pri].qos_mutex);
}

/**
 * irdma_qp_add_qos - called during setctx for qp to be added to qos
 * @qp: qp to be added to qos
 */
void
irdma_qp_add_qos(struct irdma_sc_qp *qp)
{
	struct irdma_sc_vsi *vsi = qp->vsi;

	irdma_debug(qp->dev, IRDMA_DEBUG_DCB,
		    "DCB: Add qp[%d] UP[%d] qset[%d] on_qoslist[%d]\n",
		    qp->qp_uk.qp_id, qp->user_pri, qp->qs_handle, qp->on_qoslist);
	mutex_lock(&vsi->qos[qp->user_pri].qos_mutex);
	if (!qp->on_qoslist) {
		list_add(&qp->list, &vsi->qos[qp->user_pri].qplist);
		qp->on_qoslist = true;
		qp->qs_handle = vsi->qos[qp->user_pri].qs_handle;
	}
	mutex_unlock(&vsi->qos[qp->user_pri].qos_mutex);
}

/**
 * irdma_sc_pd_init - initialize sc pd struct
 * @dev: sc device struct
 * @pd: sc pd ptr
 * @pd_id: pd_id for allocated pd
 * @abi_ver: User/Kernel ABI version
 */
void
irdma_sc_pd_init(struct irdma_sc_dev *dev, struct irdma_sc_pd *pd, u32 pd_id,
		 int abi_ver)
{
	pd->pd_id = pd_id;
	pd->abi_ver = abi_ver;
	pd->dev = dev;
}

/**
 * irdma_sc_add_arp_cache_entry - cqp wqe add arp cache entry
 * @cqp: struct for cqp hw
 * @info: arp entry information
 * @scratch: u64 saved to be used during cqp completion
 * @post_sq: flag for cqp db to ring
 */
static int
irdma_sc_add_arp_cache_entry(struct irdma_sc_cqp *cqp,
			     struct irdma_add_arp_cache_entry_info *info,
			     u64 scratch, bool post_sq)
{
	__le64 *wqe;
	u64 temp, hdr;

	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;
	set_64bit_val(wqe, IRDMA_BYTE_8, info->reach_max);

	temp = info->mac_addr[5] | LS_64_1(info->mac_addr[4], 8) |
	    LS_64_1(info->mac_addr[3], 16) | LS_64_1(info->mac_addr[2], 24) |
	    LS_64_1(info->mac_addr[1], 32) | LS_64_1(info->mac_addr[0], 40);
	set_64bit_val(wqe, IRDMA_BYTE_16, temp);

	hdr = info->arp_index |
	    LS_64(IRDMA_CQP_OP_MANAGE_ARP, IRDMA_CQPSQ_OPCODE) |
	    LS_64((info->permanent ? 1 : 0), IRDMA_CQPSQ_MAT_PERMANENT) |
	    LS_64(1, IRDMA_CQPSQ_MAT_ENTRYVALID) |
	    LS_64(cqp->polarity, IRDMA_CQPSQ_WQEVALID);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "ARP_CACHE_ENTRY WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);
	if (post_sq)
		irdma_sc_cqp_post_sq(cqp);

	return 0;
}

/**
 * irdma_sc_del_arp_cache_entry - dele arp cache entry
 * @cqp: struct for cqp hw
 * @scratch: u64 saved to be used during cqp completion
 * @arp_index: arp index to delete arp entry
 * @post_sq: flag for cqp db to ring
 */
static int
irdma_sc_del_arp_cache_entry(struct irdma_sc_cqp *cqp, u64 scratch,
			     u16 arp_index, bool post_sq)
{
	__le64 *wqe;
	u64 hdr;

	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;

	hdr = arp_index | LS_64(IRDMA_CQP_OP_MANAGE_ARP, IRDMA_CQPSQ_OPCODE) |
	    LS_64(cqp->polarity, IRDMA_CQPSQ_WQEVALID);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "ARP_CACHE_DEL_ENTRY WQE",
			wqe, IRDMA_CQP_WQE_SIZE * 8);
	if (post_sq)
		irdma_sc_cqp_post_sq(cqp);

	return 0;
}

/**
 * irdma_sc_manage_apbvt_entry - for adding and deleting apbvt entries
 * @cqp: struct for cqp hw
 * @info: info for apbvt entry to add or delete
 * @scratch: u64 saved to be used during cqp completion
 * @post_sq: flag for cqp db to ring
 */
static int
irdma_sc_manage_apbvt_entry(struct irdma_sc_cqp *cqp,
			    struct irdma_apbvt_info *info,
			    u64 scratch, bool post_sq)
{
	__le64 *wqe;
	u64 hdr;

	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;

	set_64bit_val(wqe, IRDMA_BYTE_16, info->port);

	hdr = LS_64(IRDMA_CQP_OP_MANAGE_APBVT, IRDMA_CQPSQ_OPCODE) |
	    LS_64(info->add, IRDMA_CQPSQ_MAPT_ADDPORT) |
	    LS_64(cqp->polarity, IRDMA_CQPSQ_WQEVALID);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "MANAGE_APBVT WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);
	if (post_sq)
		irdma_sc_cqp_post_sq(cqp);

	return 0;
}

/**
 * irdma_sc_manage_qhash_table_entry - manage quad hash entries
 * @cqp: struct for cqp hw
 * @info: info for quad hash to manage
 * @scratch: u64 saved to be used during cqp completion
 * @post_sq: flag for cqp db to ring
 *
 * This is called before connection establishment is started.
 * For passive connections, when listener is created, it will
 * call with entry type of  IRDMA_QHASH_TYPE_TCP_SYN with local
 * ip address and tcp port. When SYN is received (passive
 * connections) or sent (active connections), this routine is
 * called with entry type of IRDMA_QHASH_TYPE_TCP_ESTABLISHED
 * and quad is passed in info.
 *
 * When iwarp connection is done and its state moves to RTS, the
 * quad hash entry in the hardware will point to iwarp's qp
 * number and requires no calls from the driver.
 */
static int
irdma_sc_manage_qhash_table_entry(struct irdma_sc_cqp *cqp,
				  struct irdma_qhash_table_info *info,
				  u64 scratch, bool post_sq)
{
	__le64 *wqe;
	u64 qw1 = 0;
	u64 qw2 = 0;
	u64 temp;
	struct irdma_sc_vsi *vsi = info->vsi;

	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;
	temp = info->mac_addr[5] | LS_64_1(info->mac_addr[4], 8) |
	    LS_64_1(info->mac_addr[3], 16) | LS_64_1(info->mac_addr[2], 24) |
	    LS_64_1(info->mac_addr[1], 32) | LS_64_1(info->mac_addr[0], 40);
	set_64bit_val(wqe, IRDMA_BYTE_0, temp);

	qw1 = LS_64(info->qp_num, IRDMA_CQPSQ_QHASH_QPN) |
	    LS_64(info->dest_port, IRDMA_CQPSQ_QHASH_DEST_PORT);
	if (info->ipv4_valid) {
		set_64bit_val(wqe, IRDMA_BYTE_48,
			      LS_64(info->dest_ip[0], IRDMA_CQPSQ_QHASH_ADDR3));
	} else {
		set_64bit_val(wqe, IRDMA_BYTE_56,
			      LS_64(info->dest_ip[0], IRDMA_CQPSQ_QHASH_ADDR0) |
			      LS_64(info->dest_ip[1], IRDMA_CQPSQ_QHASH_ADDR1));

		set_64bit_val(wqe, IRDMA_BYTE_48,
			      LS_64(info->dest_ip[2], IRDMA_CQPSQ_QHASH_ADDR2) |
			      LS_64(info->dest_ip[3], IRDMA_CQPSQ_QHASH_ADDR3));
	}
	qw2 = LS_64(vsi->qos[info->user_pri].qs_handle,
		    IRDMA_CQPSQ_QHASH_QS_HANDLE);
	if (info->vlan_valid)
		qw2 |= LS_64(info->vlan_id, IRDMA_CQPSQ_QHASH_VLANID);
	set_64bit_val(wqe, IRDMA_BYTE_16, qw2);
	if (info->entry_type == IRDMA_QHASH_TYPE_TCP_ESTABLISHED) {
		qw1 |= LS_64(info->src_port, IRDMA_CQPSQ_QHASH_SRC_PORT);
		if (!info->ipv4_valid) {
			set_64bit_val(wqe, IRDMA_BYTE_40,
				      LS_64(info->src_ip[0], IRDMA_CQPSQ_QHASH_ADDR0) |
				      LS_64(info->src_ip[1], IRDMA_CQPSQ_QHASH_ADDR1));
			set_64bit_val(wqe, IRDMA_BYTE_32,
				      LS_64(info->src_ip[2], IRDMA_CQPSQ_QHASH_ADDR2) |
				      LS_64(info->src_ip[3], IRDMA_CQPSQ_QHASH_ADDR3));
		} else {
			set_64bit_val(wqe, IRDMA_BYTE_32,
				      LS_64(info->src_ip[0], IRDMA_CQPSQ_QHASH_ADDR3));
		}
	}

	set_64bit_val(wqe, IRDMA_BYTE_8, qw1);
	temp = LS_64(cqp->polarity, IRDMA_CQPSQ_QHASH_WQEVALID) |
	    LS_64(IRDMA_CQP_OP_MANAGE_QUAD_HASH_TABLE_ENTRY,
		  IRDMA_CQPSQ_QHASH_OPCODE) |
	    LS_64(info->manage, IRDMA_CQPSQ_QHASH_MANAGE) |
	    LS_64(info->ipv4_valid, IRDMA_CQPSQ_QHASH_IPV4VALID) |
	    LS_64(info->vlan_valid, IRDMA_CQPSQ_QHASH_VLANVALID) |
	    LS_64(info->entry_type, IRDMA_CQPSQ_QHASH_ENTRYTYPE);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, temp);

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "MANAGE_QHASH WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);
	if (post_sq)
		irdma_sc_cqp_post_sq(cqp);

	return 0;
}

/**
 * irdma_sc_qp_init - initialize qp
 * @qp: sc qp
 * @info: initialization qp info
 */
int
irdma_sc_qp_init(struct irdma_sc_qp *qp, struct irdma_qp_init_info *info)
{
	int ret_code;
	u32 pble_obj_cnt;
	u16 wqe_size;

	if (info->qp_uk_init_info.max_sq_frag_cnt >
	    info->pd->dev->hw_attrs.uk_attrs.max_hw_wq_frags ||
	    info->qp_uk_init_info.max_rq_frag_cnt >
	    info->pd->dev->hw_attrs.uk_attrs.max_hw_wq_frags)
		return -EINVAL;

	qp->dev = info->pd->dev;
	qp->vsi = info->vsi;
	qp->ieq_qp = info->vsi->exception_lan_q;
	qp->sq_pa = info->sq_pa;
	qp->rq_pa = info->rq_pa;
	qp->hw_host_ctx_pa = info->host_ctx_pa;
	qp->q2_pa = info->q2_pa;
	qp->shadow_area_pa = info->shadow_area_pa;
	qp->q2_buf = info->q2;
	qp->pd = info->pd;
	qp->hw_host_ctx = info->host_ctx;
	info->qp_uk_init_info.wqe_alloc_db = qp->pd->dev->wqe_alloc_db;
	ret_code = irdma_uk_qp_init(&qp->qp_uk, &info->qp_uk_init_info);
	if (ret_code)
		return ret_code;

	qp->virtual_map = info->virtual_map;
	pble_obj_cnt = info->pd->dev->hmc_info->hmc_obj[IRDMA_HMC_IW_PBLE].cnt;

	if ((info->virtual_map && info->sq_pa >= pble_obj_cnt) ||
	    (info->virtual_map && info->rq_pa >= pble_obj_cnt))
		return -EINVAL;

	qp->llp_stream_handle = (void *)(-1);
	qp->qp_uk.force_fence = true;
	qp->hw_sq_size = irdma_get_encoded_wqe_size(qp->qp_uk.sq_ring.size,
						    IRDMA_QUEUE_TYPE_SQ_RQ);
	irdma_debug(qp->dev, IRDMA_DEBUG_WQE,
		    "hw_sq_size[%04d] sq_ring.size[%04d]\n", qp->hw_sq_size,
		    qp->qp_uk.sq_ring.size);
	if (qp->qp_uk.uk_attrs->hw_rev == IRDMA_GEN_1)
		wqe_size = IRDMA_WQE_SIZE_128;
	else
		ret_code = irdma_fragcnt_to_wqesize_rq(qp->qp_uk.max_rq_frag_cnt,
						       &wqe_size);
	if (ret_code)
		return ret_code;

	qp->hw_rq_size =
	    irdma_get_encoded_wqe_size(qp->qp_uk.rq_size *
				       (wqe_size / IRDMA_QP_WQE_MIN_SIZE),
				       IRDMA_QUEUE_TYPE_SQ_RQ);
	irdma_debug(qp->dev, IRDMA_DEBUG_WQE,
		    "hw_rq_size[%04d] qp_uk.rq_size[%04d] wqe_size[%04d]\n",
		    qp->hw_rq_size, qp->qp_uk.rq_size, wqe_size);

	qp->sq_tph_val = info->sq_tph_val;
	qp->rq_tph_val = info->rq_tph_val;
	qp->sq_tph_en = info->sq_tph_en;
	qp->rq_tph_en = info->rq_tph_en;
	qp->rcv_tph_en = info->rcv_tph_en;
	qp->xmit_tph_en = info->xmit_tph_en;
	qp->qp_uk.first_sq_wq = info->qp_uk_init_info.first_sq_wq;
	qp->qs_handle = qp->vsi->qos[qp->user_pri].qs_handle;

	return 0;
}

/**
 * irdma_sc_qp_create - create qp
 * @qp: sc qp
 * @info: qp create info
 * @scratch: u64 saved to be used during cqp completion
 * @post_sq: flag for cqp db to ring
 */
int
irdma_sc_qp_create(struct irdma_sc_qp *qp, struct irdma_create_qp_info *info,
		   u64 scratch, bool post_sq)
{
	struct irdma_sc_cqp *cqp;
	__le64 *wqe;
	u64 hdr;

	cqp = qp->dev->cqp;
	if (qp->qp_uk.qp_id < cqp->dev->hw_attrs.min_hw_qp_id ||
	    qp->qp_uk.qp_id > (cqp->dev->hmc_info->hmc_obj[IRDMA_HMC_IW_QP].max_cnt - 1))
		return -EINVAL;

	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;

	set_64bit_val(wqe, IRDMA_BYTE_16, qp->hw_host_ctx_pa);
	set_64bit_val(wqe, IRDMA_BYTE_40, qp->shadow_area_pa);

	hdr = qp->qp_uk.qp_id |
	    LS_64(IRDMA_CQP_OP_CREATE_QP, IRDMA_CQPSQ_OPCODE) |
	    LS_64((info->ord_valid ? 1 : 0), IRDMA_CQPSQ_QP_ORDVALID) |
	    LS_64(info->tcp_ctx_valid, IRDMA_CQPSQ_QP_TOECTXVALID) |
	    LS_64(info->mac_valid, IRDMA_CQPSQ_QP_MACVALID) |
	    LS_64(qp->qp_uk.qp_type, IRDMA_CQPSQ_QP_QPTYPE) |
	    LS_64(qp->virtual_map, IRDMA_CQPSQ_QP_VQ) |
	    LS_64(info->force_lpb, IRDMA_CQPSQ_QP_FORCELOOPBACK) |
	    LS_64(info->cq_num_valid, IRDMA_CQPSQ_QP_CQNUMVALID) |
	    LS_64(info->arp_cache_idx_valid, IRDMA_CQPSQ_QP_ARPTABIDXVALID) |
	    LS_64(info->next_iwarp_state, IRDMA_CQPSQ_QP_NEXTIWSTATE) |
	    LS_64(cqp->polarity, IRDMA_CQPSQ_WQEVALID);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "QP_CREATE WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);
	if (post_sq)
		irdma_sc_cqp_post_sq(cqp);

	return 0;
}

/**
 * irdma_sc_qp_modify - modify qp cqp wqe
 * @qp: sc qp
 * @info: modify qp info
 * @scratch: u64 saved to be used during cqp completion
 * @post_sq: flag for cqp db to ring
 */
int
irdma_sc_qp_modify(struct irdma_sc_qp *qp, struct irdma_modify_qp_info *info,
		   u64 scratch, bool post_sq)
{
	__le64 *wqe;
	struct irdma_sc_cqp *cqp;
	u64 hdr;
	u8 term_actions = 0;
	u8 term_len = 0;

	cqp = qp->dev->cqp;
	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;

	if (info->next_iwarp_state == IRDMA_QP_STATE_TERMINATE) {
		if (info->dont_send_fin)
			term_actions += IRDMAQP_TERM_SEND_TERM_ONLY;
		if (info->dont_send_term)
			term_actions += IRDMAQP_TERM_SEND_FIN_ONLY;
		if (term_actions == IRDMAQP_TERM_SEND_TERM_AND_FIN ||
		    term_actions == IRDMAQP_TERM_SEND_TERM_ONLY)
			term_len = info->termlen;
	}

	set_64bit_val(wqe, IRDMA_BYTE_8,
		      LS_64(info->new_mss, IRDMA_CQPSQ_QP_NEWMSS) |
		      LS_64(term_len, IRDMA_CQPSQ_QP_TERMLEN));
	set_64bit_val(wqe, IRDMA_BYTE_16, qp->hw_host_ctx_pa);
	set_64bit_val(wqe, IRDMA_BYTE_40, qp->shadow_area_pa);

	hdr = qp->qp_uk.qp_id |
	    LS_64(IRDMA_CQP_OP_MODIFY_QP, IRDMA_CQPSQ_OPCODE) |
	    LS_64(info->ord_valid, IRDMA_CQPSQ_QP_ORDVALID) |
	    LS_64(info->tcp_ctx_valid, IRDMA_CQPSQ_QP_TOECTXVALID) |
	    LS_64(info->cached_var_valid, IRDMA_CQPSQ_QP_CACHEDVARVALID) |
	    LS_64(qp->virtual_map, IRDMA_CQPSQ_QP_VQ) |
	    LS_64(info->force_lpb, IRDMA_CQPSQ_QP_FORCELOOPBACK) |
	    LS_64(info->cq_num_valid, IRDMA_CQPSQ_QP_CQNUMVALID) |
	    LS_64(info->mac_valid, IRDMA_CQPSQ_QP_MACVALID) |
	    LS_64(qp->qp_uk.qp_type, IRDMA_CQPSQ_QP_QPTYPE) |
	    LS_64(info->mss_change, IRDMA_CQPSQ_QP_MSSCHANGE) |
	    LS_64(info->remove_hash_idx, IRDMA_CQPSQ_QP_REMOVEHASHENTRY) |
	    LS_64(term_actions, IRDMA_CQPSQ_QP_TERMACT) |
	    LS_64(info->reset_tcp_conn, IRDMA_CQPSQ_QP_RESETCON) |
	    LS_64(info->arp_cache_idx_valid, IRDMA_CQPSQ_QP_ARPTABIDXVALID) |
	    LS_64(info->next_iwarp_state, IRDMA_CQPSQ_QP_NEXTIWSTATE) |
	    LS_64(cqp->polarity, IRDMA_CQPSQ_WQEVALID);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "QP_MODIFY WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);
	if (post_sq)
		irdma_sc_cqp_post_sq(cqp);

	return 0;
}

/**
 * irdma_sc_qp_destroy - cqp destroy qp
 * @qp: sc qp
 * @scratch: u64 saved to be used during cqp completion
 * @remove_hash_idx: flag if to remove hash idx
 * @ignore_mw_bnd: memory window bind flag
 * @post_sq: flag for cqp db to ring
 */
int
irdma_sc_qp_destroy(struct irdma_sc_qp *qp, u64 scratch,
		    bool remove_hash_idx, bool ignore_mw_bnd, bool post_sq)
{
	__le64 *wqe;
	struct irdma_sc_cqp *cqp;
	u64 hdr;

	cqp = qp->dev->cqp;
	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;

	set_64bit_val(wqe, IRDMA_BYTE_16, qp->hw_host_ctx_pa);
	set_64bit_val(wqe, IRDMA_BYTE_40, qp->shadow_area_pa);

	hdr = qp->qp_uk.qp_id |
	    LS_64(IRDMA_CQP_OP_DESTROY_QP, IRDMA_CQPSQ_OPCODE) |
	    LS_64(qp->qp_uk.qp_type, IRDMA_CQPSQ_QP_QPTYPE) |
	    LS_64(ignore_mw_bnd, IRDMA_CQPSQ_QP_IGNOREMWBOUND) |
	    LS_64(remove_hash_idx, IRDMA_CQPSQ_QP_REMOVEHASHENTRY) |
	    LS_64(cqp->polarity, IRDMA_CQPSQ_WQEVALID);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "QP_DESTROY WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);
	if (post_sq)
		irdma_sc_cqp_post_sq(cqp);

	return 0;
}

/**
 * irdma_sc_get_encoded_ird_size -
 * @ird_size: IRD size
 * The ird from the connection is rounded to a supported HW setting and then encoded
 * for ird_size field of qp_ctx. Consumers are expected to provide valid ird size based
 * on hardware attributes. IRD size defaults to a value of 4 in case of invalid input
 */
static u8 irdma_sc_get_encoded_ird_size(u16 ird_size) {
	switch (ird_size ?
		roundup_pow_of_two(2 * ird_size) : 4) {
	case 256:
		return IRDMA_IRD_HW_SIZE_256;
	case 128:
		return IRDMA_IRD_HW_SIZE_128;
	case 64:
	case 32:
		return IRDMA_IRD_HW_SIZE_64;
	case 16:
	case 8:
		return IRDMA_IRD_HW_SIZE_16;
	case 4:
	default:
		break;
	}

	return IRDMA_IRD_HW_SIZE_4;
}

/**
 * irdma_sc_qp_setctx_roce - set qp's context
 * @qp: sc qp
 * @qp_ctx: context ptr
 * @info: ctx info
 */
void
irdma_sc_qp_setctx_roce(struct irdma_sc_qp *qp, __le64 * qp_ctx,
			struct irdma_qp_host_ctx_info *info)
{
	struct irdma_roce_offload_info *roce_info;
	struct irdma_udp_offload_info *udp;
	u8 push_mode_en;
	u32 push_idx;
	u64 mac;

	roce_info = info->roce_info;
	udp = info->udp_info;

	mac = LS_64_1(roce_info->mac_addr[5], 16) |
	    LS_64_1(roce_info->mac_addr[4], 24) |
	    LS_64_1(roce_info->mac_addr[3], 32) |
	    LS_64_1(roce_info->mac_addr[2], 40) |
	    LS_64_1(roce_info->mac_addr[1], 48) |
	    LS_64_1(roce_info->mac_addr[0], 56);

	qp->user_pri = info->user_pri;
	if (qp->push_idx == IRDMA_INVALID_PUSH_PAGE_INDEX) {
		push_mode_en = 0;
		push_idx = 0;
	} else {
		push_mode_en = 1;
		push_idx = qp->push_idx;
	}
	set_64bit_val(qp_ctx, IRDMA_BYTE_0,
		      LS_64(qp->qp_uk.rq_wqe_size, IRDMAQPC_RQWQESIZE) |
		      LS_64(qp->rcv_tph_en, IRDMAQPC_RCVTPHEN) |
		      LS_64(qp->xmit_tph_en, IRDMAQPC_XMITTPHEN) |
		      LS_64(qp->rq_tph_en, IRDMAQPC_RQTPHEN) |
		      LS_64(qp->sq_tph_en, IRDMAQPC_SQTPHEN) |
		      LS_64(push_idx, IRDMAQPC_PPIDX) |
		      LS_64(push_mode_en, IRDMAQPC_PMENA) |
		      LS_64(roce_info->pd_id >> 16, IRDMAQPC_PDIDXHI) |
		      LS_64(roce_info->dctcp_en, IRDMAQPC_DC_TCP_EN) |
		      LS_64(roce_info->err_rq_idx_valid, IRDMAQPC_ERR_RQ_IDX_VALID) |
		      LS_64(roce_info->is_qp1, IRDMAQPC_ISQP1) |
		      LS_64(roce_info->roce_tver, IRDMAQPC_ROCE_TVER) |
		      LS_64(udp->ipv4, IRDMAQPC_IPV4) |
		      LS_64(udp->insert_vlan_tag, IRDMAQPC_INSERTVLANTAG));
	set_64bit_val(qp_ctx, IRDMA_BYTE_8, qp->sq_pa);
	set_64bit_val(qp_ctx, IRDMA_BYTE_16, qp->rq_pa);
	if (roce_info->dcqcn_en || roce_info->dctcp_en) {
		udp->tos &= ~ECN_CODE_PT_MASK;
		udp->tos |= ECN_CODE_PT_VAL;
	}

	set_64bit_val(qp_ctx, IRDMA_BYTE_24,
		      LS_64(qp->hw_rq_size, IRDMAQPC_RQSIZE) |
		      LS_64(qp->hw_sq_size, IRDMAQPC_SQSIZE) |
		      LS_64(udp->ttl, IRDMAQPC_TTL) | LS_64(udp->tos, IRDMAQPC_TOS) |
		      LS_64(udp->src_port, IRDMAQPC_SRCPORTNUM) |
		      LS_64(udp->dst_port, IRDMAQPC_DESTPORTNUM));
	set_64bit_val(qp_ctx, IRDMA_BYTE_32,
		      LS_64(udp->dest_ip_addr[2], IRDMAQPC_DESTIPADDR2) |
		      LS_64(udp->dest_ip_addr[3], IRDMAQPC_DESTIPADDR3));
	set_64bit_val(qp_ctx, IRDMA_BYTE_40,
		      LS_64(udp->dest_ip_addr[0], IRDMAQPC_DESTIPADDR0) |
		      LS_64(udp->dest_ip_addr[1], IRDMAQPC_DESTIPADDR1));
	set_64bit_val(qp_ctx, IRDMA_BYTE_48,
		      LS_64(udp->snd_mss, IRDMAQPC_SNDMSS) |
		      LS_64(udp->vlan_tag, IRDMAQPC_VLANTAG) |
		      LS_64(udp->arp_idx, IRDMAQPC_ARPIDX));
	set_64bit_val(qp_ctx, IRDMA_BYTE_56,
		      LS_64(roce_info->p_key, IRDMAQPC_PKEY) |
		      LS_64(roce_info->pd_id, IRDMAQPC_PDIDX) |
		      LS_64(roce_info->ack_credits, IRDMAQPC_ACKCREDITS) |
		      LS_64(udp->flow_label, IRDMAQPC_FLOWLABEL));
	set_64bit_val(qp_ctx, IRDMA_BYTE_64,
		      LS_64(roce_info->qkey, IRDMAQPC_QKEY) |
		      LS_64(roce_info->dest_qp, IRDMAQPC_DESTQP));
	set_64bit_val(qp_ctx, IRDMA_BYTE_80,
		      LS_64(udp->psn_nxt, IRDMAQPC_PSNNXT) |
		      LS_64(udp->lsn, IRDMAQPC_LSN));
	set_64bit_val(qp_ctx, IRDMA_BYTE_88, LS_64(udp->epsn, IRDMAQPC_EPSN));
	set_64bit_val(qp_ctx, IRDMA_BYTE_96,
		      LS_64(udp->psn_max, IRDMAQPC_PSNMAX) |
		      LS_64(udp->psn_una, IRDMAQPC_PSNUNA));
	set_64bit_val(qp_ctx, IRDMA_BYTE_112,
		      LS_64(udp->cwnd, IRDMAQPC_CWNDROCE));
	set_64bit_val(qp_ctx, IRDMA_BYTE_128,
		      LS_64(roce_info->err_rq_idx, IRDMAQPC_ERR_RQ_IDX) |
		      LS_64(udp->rnr_nak_thresh, IRDMAQPC_RNRNAK_THRESH) |
		      LS_64(udp->rexmit_thresh, IRDMAQPC_REXMIT_THRESH) |
		      LS_64(roce_info->rtomin, IRDMAQPC_RTOMIN));
	set_64bit_val(qp_ctx, IRDMA_BYTE_136,
		      LS_64(info->send_cq_num, IRDMAQPC_TXCQNUM) |
		      LS_64(info->rcv_cq_num, IRDMAQPC_RXCQNUM));
	set_64bit_val(qp_ctx, IRDMA_BYTE_144,
		      LS_64(info->stats_idx, IRDMAQPC_STAT_INDEX));
	set_64bit_val(qp_ctx, IRDMA_BYTE_152, mac);
	set_64bit_val(qp_ctx, IRDMA_BYTE_160,
		      LS_64(roce_info->ord_size, IRDMAQPC_ORDSIZE) |
		      LS_64(irdma_sc_get_encoded_ird_size(roce_info->ird_size), IRDMAQPC_IRDSIZE) |
		      LS_64(roce_info->wr_rdresp_en, IRDMAQPC_WRRDRSPOK) |
		      LS_64(roce_info->rd_en, IRDMAQPC_RDOK) |
		      LS_64(info->stats_idx_valid, IRDMAQPC_USESTATSINSTANCE) |
		      LS_64(roce_info->bind_en, IRDMAQPC_BINDEN) |
		      LS_64(roce_info->fast_reg_en, IRDMAQPC_FASTREGEN) |
		      LS_64(roce_info->dcqcn_en, IRDMAQPC_DCQCNENABLE) |
		      LS_64(roce_info->rcv_no_icrc, IRDMAQPC_RCVNOICRC) |
		      LS_64(roce_info->fw_cc_enable, IRDMAQPC_FW_CC_ENABLE) |
		      LS_64(roce_info->udprivcq_en, IRDMAQPC_UDPRIVCQENABLE) |
		      LS_64(roce_info->priv_mode_en, IRDMAQPC_PRIVEN) |
		      LS_64(roce_info->timely_en, IRDMAQPC_TIMELYENABLE));
	set_64bit_val(qp_ctx, IRDMA_BYTE_168,
		      LS_64(info->qp_compl_ctx, IRDMAQPC_QPCOMPCTX));
	set_64bit_val(qp_ctx, IRDMA_BYTE_176,
		      LS_64(qp->sq_tph_val, IRDMAQPC_SQTPHVAL) |
		      LS_64(qp->rq_tph_val, IRDMAQPC_RQTPHVAL) |
		      LS_64(qp->qs_handle, IRDMAQPC_QSHANDLE));
	set_64bit_val(qp_ctx, IRDMA_BYTE_184,
		      LS_64(udp->local_ipaddr[3], IRDMAQPC_LOCAL_IPADDR3) |
		      LS_64(udp->local_ipaddr[2], IRDMAQPC_LOCAL_IPADDR2));
	set_64bit_val(qp_ctx, IRDMA_BYTE_192,
		      LS_64(udp->local_ipaddr[1], IRDMAQPC_LOCAL_IPADDR1) |
		      LS_64(udp->local_ipaddr[0], IRDMAQPC_LOCAL_IPADDR0));
	set_64bit_val(qp_ctx, IRDMA_BYTE_200,
		      LS_64(roce_info->t_high, IRDMAQPC_THIGH) |
		      LS_64(roce_info->t_low, IRDMAQPC_TLOW));
	set_64bit_val(qp_ctx, IRDMA_BYTE_208,
		      LS_64(info->rem_endpoint_idx, IRDMAQPC_REMENDPOINTIDX));

	irdma_debug_buf(qp->dev, IRDMA_DEBUG_WQE, "QP_HOST CTX WQE", qp_ctx,
			IRDMA_QP_CTX_SIZE);
}

/*
 * irdma_sc_alloc_local_mac_entry - allocate a mac entry @cqp: struct for cqp hw @scratch: u64 saved to be used during
 * cqp completion @post_sq: flag for cqp db to ring
 */
static int
irdma_sc_alloc_local_mac_entry(struct irdma_sc_cqp *cqp, u64 scratch,
			       bool post_sq)
{
	__le64 *wqe;
	u64 hdr;

	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;

	hdr = LS_64(IRDMA_CQP_OP_ALLOCATE_LOC_MAC_TABLE_ENTRY,
		    IRDMA_CQPSQ_OPCODE) |
	    LS_64(cqp->polarity, IRDMA_CQPSQ_WQEVALID);

	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "ALLOCATE_LOCAL_MAC WQE",
			wqe, IRDMA_CQP_WQE_SIZE * 8);

	if (post_sq)
		irdma_sc_cqp_post_sq(cqp);
	return 0;
}

/**
 * irdma_sc_add_local_mac_entry - add mac enry
 * @cqp: struct for cqp hw
 * @info:mac addr info
 * @scratch: u64 saved to be used during cqp completion
 * @post_sq: flag for cqp db to ring
 */
static int
irdma_sc_add_local_mac_entry(struct irdma_sc_cqp *cqp,
			     struct irdma_local_mac_entry_info *info,
			     u64 scratch, bool post_sq)
{
	__le64 *wqe;
	u64 temp, header;

	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;
	temp = info->mac_addr[5] | LS_64_1(info->mac_addr[4], 8) |
	    LS_64_1(info->mac_addr[3], 16) | LS_64_1(info->mac_addr[2], 24) |
	    LS_64_1(info->mac_addr[1], 32) | LS_64_1(info->mac_addr[0], 40);

	set_64bit_val(wqe, IRDMA_BYTE_32, temp);

	header = LS_64(info->entry_idx, IRDMA_CQPSQ_MLM_TABLEIDX) |
	    LS_64(IRDMA_CQP_OP_MANAGE_LOC_MAC_TABLE, IRDMA_CQPSQ_OPCODE) |
	    LS_64(cqp->polarity, IRDMA_CQPSQ_WQEVALID);

	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, header);

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "ADD_LOCAL_MAC WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);

	if (post_sq)
		irdma_sc_cqp_post_sq(cqp);
	return 0;
}

/**
 * irdma_sc_del_local_mac_entry - cqp wqe to dele local mac
 * @cqp: struct for cqp hw
 * @scratch: u64 saved to be used during cqp completion
 * @entry_idx: index of mac entry
 * @ignore_ref_count: to force mac adde delete
 * @post_sq: flag for cqp db to ring
 */
static int
irdma_sc_del_local_mac_entry(struct irdma_sc_cqp *cqp, u64 scratch,
			     u16 entry_idx, u8 ignore_ref_count,
			     bool post_sq)
{
	__le64 *wqe;
	u64 header;

	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;
	header = LS_64(entry_idx, IRDMA_CQPSQ_MLM_TABLEIDX) |
	    LS_64(IRDMA_CQP_OP_MANAGE_LOC_MAC_TABLE, IRDMA_CQPSQ_OPCODE) |
	    LS_64(1, IRDMA_CQPSQ_MLM_FREEENTRY) |
	    LS_64(cqp->polarity, IRDMA_CQPSQ_WQEVALID) |
	    LS_64(ignore_ref_count, IRDMA_CQPSQ_MLM_IGNORE_REF_CNT);

	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, header);

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "DEL_LOCAL_MAC_IPADDR WQE",
			wqe, IRDMA_CQP_WQE_SIZE * 8);

	if (post_sq)
		irdma_sc_cqp_post_sq(cqp);
	return 0;
}

/**
 * irdma_sc_qp_setctx - set qp's context
 * @qp: sc qp
 * @qp_ctx: context ptr
 * @info: ctx info
 */
void
irdma_sc_qp_setctx(struct irdma_sc_qp *qp, __le64 * qp_ctx,
		   struct irdma_qp_host_ctx_info *info)
{
	struct irdma_iwarp_offload_info *iw;
	struct irdma_tcp_offload_info *tcp;
	struct irdma_sc_dev *dev;
	u8 push_mode_en;
	u32 push_idx;
	u64 qw0, qw3, qw7 = 0, qw16 = 0;
	u64 mac = 0;

	iw = info->iwarp_info;
	tcp = info->tcp_info;
	dev = qp->dev;
	if (iw->rcv_mark_en) {
		qp->pfpdu.marker_len = 4;
		qp->pfpdu.rcv_start_seq = tcp->rcv_nxt;
	}
	qp->user_pri = info->user_pri;
	if (qp->push_idx == IRDMA_INVALID_PUSH_PAGE_INDEX) {
		push_mode_en = 0;
		push_idx = 0;
	} else {
		push_mode_en = 1;
		push_idx = qp->push_idx;
	}
	qw0 = LS_64(qp->qp_uk.rq_wqe_size, IRDMAQPC_RQWQESIZE) |
	    LS_64(qp->rcv_tph_en, IRDMAQPC_RCVTPHEN) |
	    LS_64(qp->xmit_tph_en, IRDMAQPC_XMITTPHEN) |
	    LS_64(qp->rq_tph_en, IRDMAQPC_RQTPHEN) |
	    LS_64(qp->sq_tph_en, IRDMAQPC_SQTPHEN) |
	    LS_64(push_idx, IRDMAQPC_PPIDX) |
	    LS_64(push_mode_en, IRDMAQPC_PMENA);

	set_64bit_val(qp_ctx, IRDMA_BYTE_8, qp->sq_pa);
	set_64bit_val(qp_ctx, IRDMA_BYTE_16, qp->rq_pa);

	qw3 = LS_64(qp->hw_rq_size, IRDMAQPC_RQSIZE) |
	    LS_64(qp->hw_sq_size, IRDMAQPC_SQSIZE);
	if (dev->hw_attrs.uk_attrs.hw_rev == IRDMA_GEN_1)
		qw3 |= LS_64(qp->src_mac_addr_idx, IRDMAQPC_GEN1_SRCMACADDRIDX);
	set_64bit_val(qp_ctx, IRDMA_BYTE_136,
		      LS_64(info->send_cq_num, IRDMAQPC_TXCQNUM) |
		      LS_64(info->rcv_cq_num, IRDMAQPC_RXCQNUM));
	set_64bit_val(qp_ctx, IRDMA_BYTE_168,
		      LS_64(info->qp_compl_ctx, IRDMAQPC_QPCOMPCTX));
	set_64bit_val(qp_ctx, IRDMA_BYTE_176,
		      LS_64(qp->sq_tph_val, IRDMAQPC_SQTPHVAL) |
		      LS_64(qp->rq_tph_val, IRDMAQPC_RQTPHVAL) |
		      LS_64(qp->qs_handle, IRDMAQPC_QSHANDLE) |
		      LS_64(qp->ieq_qp, IRDMAQPC_EXCEPTION_LAN_QUEUE));
	if (info->iwarp_info_valid) {
		qw0 |= LS_64(iw->ddp_ver, IRDMAQPC_DDP_VER) |
		    LS_64(iw->rdmap_ver, IRDMAQPC_RDMAP_VER) |
		    LS_64(iw->dctcp_en, IRDMAQPC_DC_TCP_EN) |
		    LS_64(iw->ecn_en, IRDMAQPC_ECN_EN) |
		    LS_64(iw->ib_rd_en, IRDMAQPC_IBRDENABLE) |
		    LS_64(iw->pd_id >> 16, IRDMAQPC_PDIDXHI) |
		    LS_64(iw->err_rq_idx_valid, IRDMAQPC_ERR_RQ_IDX_VALID);
		qw7 |= LS_64(iw->pd_id, IRDMAQPC_PDIDX);
		qw16 |= LS_64(iw->err_rq_idx, IRDMAQPC_ERR_RQ_IDX) |
		    LS_64(iw->rtomin, IRDMAQPC_RTOMIN);
		set_64bit_val(qp_ctx, IRDMA_BYTE_144,
			      LS_64(qp->q2_pa >> 8, IRDMAQPC_Q2ADDR) |
			      LS_64(info->stats_idx, IRDMAQPC_STAT_INDEX));

		if (dev->hw_attrs.uk_attrs.hw_rev >= IRDMA_GEN_2) {
			mac = LS_64_1(iw->mac_addr[5], 16) |
			    LS_64_1(iw->mac_addr[4], 24) |
			    LS_64_1(iw->mac_addr[3], 32) |
			    LS_64_1(iw->mac_addr[2], 40) |
			    LS_64_1(iw->mac_addr[1], 48) |
			    LS_64_1(iw->mac_addr[0], 56);
		}

		set_64bit_val(qp_ctx, IRDMA_BYTE_152,
			      mac | LS_64(iw->last_byte_sent, IRDMAQPC_LASTBYTESENT));
		set_64bit_val(qp_ctx, IRDMA_BYTE_160,
			      LS_64(iw->ord_size, IRDMAQPC_ORDSIZE) |
			      LS_64(irdma_sc_get_encoded_ird_size(iw->ird_size), IRDMAQPC_IRDSIZE) |
			      LS_64(iw->wr_rdresp_en, IRDMAQPC_WRRDRSPOK) |
			      LS_64(iw->rd_en, IRDMAQPC_RDOK) |
			      LS_64(iw->snd_mark_en, IRDMAQPC_SNDMARKERS) |
			      LS_64(iw->bind_en, IRDMAQPC_BINDEN) |
			      LS_64(iw->fast_reg_en, IRDMAQPC_FASTREGEN) |
			      LS_64(iw->priv_mode_en, IRDMAQPC_PRIVEN) |
			      LS_64(info->stats_idx_valid, IRDMAQPC_USESTATSINSTANCE) |
			      LS_64(1, IRDMAQPC_IWARPMODE) |
			      LS_64(iw->rcv_mark_en, IRDMAQPC_RCVMARKERS) |
			      LS_64(iw->align_hdrs, IRDMAQPC_ALIGNHDRS) |
			      LS_64(iw->rcv_no_mpa_crc, IRDMAQPC_RCVNOMPACRC) |
			      LS_64(iw->rcv_mark_offset, IRDMAQPC_RCVMARKOFFSET) |
			      LS_64(iw->snd_mark_offset, IRDMAQPC_SNDMARKOFFSET) |
			      LS_64(iw->timely_en, IRDMAQPC_TIMELYENABLE));
	}
	if (info->tcp_info_valid) {
		qw0 |= LS_64(tcp->ipv4, IRDMAQPC_IPV4) |
		    LS_64(tcp->no_nagle, IRDMAQPC_NONAGLE) |
		    LS_64(tcp->insert_vlan_tag, IRDMAQPC_INSERTVLANTAG) |
		    LS_64(tcp->time_stamp, IRDMAQPC_TIMESTAMP) |
		    LS_64(tcp->cwnd_inc_limit, IRDMAQPC_LIMIT) |
		    LS_64(tcp->drop_ooo_seg, IRDMAQPC_DROPOOOSEG) |
		    LS_64(tcp->dup_ack_thresh, IRDMAQPC_DUPACK_THRESH);

		if (iw->ecn_en || iw->dctcp_en) {
			tcp->tos &= ~ECN_CODE_PT_MASK;
			tcp->tos |= ECN_CODE_PT_VAL;
		}

		qw3 |= LS_64(tcp->ttl, IRDMAQPC_TTL) |
		    LS_64(tcp->avoid_stretch_ack, IRDMAQPC_AVOIDSTRETCHACK) |
		    LS_64(tcp->tos, IRDMAQPC_TOS) |
		    LS_64(tcp->src_port, IRDMAQPC_SRCPORTNUM) |
		    LS_64(tcp->dst_port, IRDMAQPC_DESTPORTNUM);
		if (dev->hw_attrs.uk_attrs.hw_rev == IRDMA_GEN_1) {
			qw3 |= LS_64(tcp->src_mac_addr_idx,
				     IRDMAQPC_GEN1_SRCMACADDRIDX);

			qp->src_mac_addr_idx = tcp->src_mac_addr_idx;
		}
		set_64bit_val(qp_ctx, IRDMA_BYTE_32,
			      LS_64(tcp->dest_ip_addr[2], IRDMAQPC_DESTIPADDR2) |
			      LS_64(tcp->dest_ip_addr[3], IRDMAQPC_DESTIPADDR3));
		set_64bit_val(qp_ctx, IRDMA_BYTE_40,
			      LS_64(tcp->dest_ip_addr[0], IRDMAQPC_DESTIPADDR0) |
			      LS_64(tcp->dest_ip_addr[1], IRDMAQPC_DESTIPADDR1));
		set_64bit_val(qp_ctx, IRDMA_BYTE_48,
			      LS_64(tcp->snd_mss, IRDMAQPC_SNDMSS) |
			      LS_64(tcp->syn_rst_handling, IRDMAQPC_SYN_RST_HANDLING) |
			      LS_64(tcp->vlan_tag, IRDMAQPC_VLANTAG) |
			      LS_64(tcp->arp_idx, IRDMAQPC_ARPIDX));
		qw7 |= LS_64(tcp->flow_label, IRDMAQPC_FLOWLABEL) |
		    LS_64(tcp->wscale, IRDMAQPC_WSCALE) |
		    LS_64(tcp->ignore_tcp_opt, IRDMAQPC_IGNORE_TCP_OPT) |
		    LS_64(tcp->ignore_tcp_uns_opt,
			  IRDMAQPC_IGNORE_TCP_UNS_OPT) |
		    LS_64(tcp->tcp_state, IRDMAQPC_TCPSTATE) |
		    LS_64(tcp->rcv_wscale, IRDMAQPC_RCVSCALE) |
		    LS_64(tcp->snd_wscale, IRDMAQPC_SNDSCALE);
		set_64bit_val(qp_ctx, IRDMA_BYTE_72,
			      LS_64(tcp->time_stamp_recent, IRDMAQPC_TIMESTAMP_RECENT) |
			      LS_64(tcp->time_stamp_age, IRDMAQPC_TIMESTAMP_AGE));
		set_64bit_val(qp_ctx, IRDMA_BYTE_80,
			      LS_64(tcp->snd_nxt, IRDMAQPC_SNDNXT) |
			      LS_64(tcp->snd_wnd, IRDMAQPC_SNDWND));
		set_64bit_val(qp_ctx, IRDMA_BYTE_88,
			      LS_64(tcp->rcv_nxt, IRDMAQPC_RCVNXT) |
			      LS_64(tcp->rcv_wnd, IRDMAQPC_RCVWND));
		set_64bit_val(qp_ctx, IRDMA_BYTE_96,
			      LS_64(tcp->snd_max, IRDMAQPC_SNDMAX) |
			      LS_64(tcp->snd_una, IRDMAQPC_SNDUNA));
		set_64bit_val(qp_ctx, IRDMA_BYTE_104,
			      LS_64(tcp->srtt, IRDMAQPC_SRTT) |
			      LS_64(tcp->rtt_var, IRDMAQPC_RTTVAR));
		set_64bit_val(qp_ctx, IRDMA_BYTE_112,
			      LS_64(tcp->ss_thresh, IRDMAQPC_SSTHRESH) |
			      LS_64(tcp->cwnd, IRDMAQPC_CWND));
		set_64bit_val(qp_ctx, IRDMA_BYTE_120,
			      LS_64(tcp->snd_wl1, IRDMAQPC_SNDWL1) |
			      LS_64(tcp->snd_wl2, IRDMAQPC_SNDWL2));
		qw16 |= LS_64(tcp->max_snd_window, IRDMAQPC_MAXSNDWND) |
		    LS_64(tcp->rexmit_thresh, IRDMAQPC_REXMIT_THRESH);
		set_64bit_val(qp_ctx, IRDMA_BYTE_184,
			      LS_64(tcp->local_ipaddr[3], IRDMAQPC_LOCAL_IPADDR3) |
			      LS_64(tcp->local_ipaddr[2], IRDMAQPC_LOCAL_IPADDR2));
		set_64bit_val(qp_ctx, IRDMA_BYTE_192,
			      LS_64(tcp->local_ipaddr[1], IRDMAQPC_LOCAL_IPADDR1) |
			      LS_64(tcp->local_ipaddr[0], IRDMAQPC_LOCAL_IPADDR0));
		set_64bit_val(qp_ctx, IRDMA_BYTE_200,
			      LS_64(iw->t_high, IRDMAQPC_THIGH) |
			      LS_64(iw->t_low, IRDMAQPC_TLOW));
		set_64bit_val(qp_ctx, IRDMA_BYTE_208,
			      LS_64(info->rem_endpoint_idx, IRDMAQPC_REMENDPOINTIDX));
	}

	set_64bit_val(qp_ctx, IRDMA_BYTE_0, qw0);
	set_64bit_val(qp_ctx, IRDMA_BYTE_24, qw3);
	set_64bit_val(qp_ctx, IRDMA_BYTE_56, qw7);
	set_64bit_val(qp_ctx, IRDMA_BYTE_128, qw16);

	irdma_debug_buf(qp->dev, IRDMA_DEBUG_WQE, "QP_HOST CTX", qp_ctx,
			IRDMA_QP_CTX_SIZE);
}

/**
 * irdma_sc_alloc_stag - mr stag alloc
 * @dev: sc device struct
 * @info: stag info
 * @scratch: u64 saved to be used during cqp completion
 * @post_sq: flag for cqp db to ring
 */
static int
irdma_sc_alloc_stag(struct irdma_sc_dev *dev,
		    struct irdma_allocate_stag_info *info,
		    u64 scratch, bool post_sq)
{
	__le64 *wqe;
	struct irdma_sc_cqp *cqp;
	u64 hdr;
	enum irdma_page_size page_size;

	if (info->page_size == 0x40000000)
		page_size = IRDMA_PAGE_SIZE_1G;
	else if (info->page_size == 0x200000)
		page_size = IRDMA_PAGE_SIZE_2M;
	else
		page_size = IRDMA_PAGE_SIZE_4K;

	cqp = dev->cqp;
	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;

	set_64bit_val(wqe, IRDMA_BYTE_8,
		      FLD_LS_64(dev, info->pd_id, IRDMA_CQPSQ_STAG_PDID) |
		      LS_64(info->total_len, IRDMA_CQPSQ_STAG_STAGLEN));
	set_64bit_val(wqe, IRDMA_BYTE_16,
		      LS_64(info->stag_idx, IRDMA_CQPSQ_STAG_IDX));
	set_64bit_val(wqe, IRDMA_BYTE_40,
		      LS_64(info->hmc_fcn_index, IRDMA_CQPSQ_STAG_HMCFNIDX));

	if (info->chunk_size)
		set_64bit_val(wqe, IRDMA_BYTE_48,
			      LS_64(info->first_pm_pbl_idx, IRDMA_CQPSQ_STAG_FIRSTPMPBLIDX));

	hdr = LS_64(IRDMA_CQP_OP_ALLOC_STAG, IRDMA_CQPSQ_OPCODE) |
	    LS_64(1, IRDMA_CQPSQ_STAG_MR) |
	    LS_64(info->access_rights, IRDMA_CQPSQ_STAG_ARIGHTS) |
	    LS_64(info->chunk_size, IRDMA_CQPSQ_STAG_LPBLSIZE) |
	    LS_64(page_size, IRDMA_CQPSQ_STAG_HPAGESIZE) |
	    LS_64(info->remote_access, IRDMA_CQPSQ_STAG_REMACCENABLED) |
	    LS_64(info->use_hmc_fcn_index, IRDMA_CQPSQ_STAG_USEHMCFNIDX) |
	    LS_64(info->use_pf_rid, IRDMA_CQPSQ_STAG_USEPFRID) |
	    LS_64(cqp->polarity, IRDMA_CQPSQ_WQEVALID);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	irdma_debug_buf(dev, IRDMA_DEBUG_WQE, "ALLOC_STAG WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);
	if (post_sq)
		irdma_sc_cqp_post_sq(cqp);

	return 0;
}

/**
 * irdma_sc_mr_reg_non_shared - non-shared mr registration
 * @dev: sc device struct
 * @info: mr info
 * @scratch: u64 saved to be used during cqp completion
 * @post_sq: flag for cqp db to ring
 */
static int
irdma_sc_mr_reg_non_shared(struct irdma_sc_dev *dev,
			   struct irdma_reg_ns_stag_info *info,
			   u64 scratch, bool post_sq)
{
	__le64 *wqe;
	u64 fbo;
	struct irdma_sc_cqp *cqp;
	u64 hdr;
	u32 pble_obj_cnt;
	bool remote_access;
	u8 addr_type;
	enum irdma_page_size page_size;

	if (info->page_size == 0x40000000)
		page_size = IRDMA_PAGE_SIZE_1G;
	else if (info->page_size == 0x200000)
		page_size = IRDMA_PAGE_SIZE_2M;
	else if (info->page_size == 0x1000)
		page_size = IRDMA_PAGE_SIZE_4K;
	else
		return -EINVAL;

	if (info->access_rights & (IRDMA_ACCESS_FLAGS_REMOTEREAD_ONLY |
				   IRDMA_ACCESS_FLAGS_REMOTEWRITE_ONLY))
		remote_access = true;
	else
		remote_access = false;

	pble_obj_cnt = dev->hmc_info->hmc_obj[IRDMA_HMC_IW_PBLE].cnt;
	if (info->chunk_size && info->first_pm_pbl_index >= pble_obj_cnt)
		return -EINVAL;

	cqp = dev->cqp;
	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;
	fbo = info->va & (info->page_size - 1);

	set_64bit_val(wqe, IRDMA_BYTE_0,
		      (info->addr_type == IRDMA_ADDR_TYPE_VA_BASED ?
		       info->va : fbo));
	set_64bit_val(wqe, IRDMA_BYTE_8,
		      LS_64(info->total_len, IRDMA_CQPSQ_STAG_STAGLEN) |
		      FLD_LS_64(dev, info->pd_id, IRDMA_CQPSQ_STAG_PDID));
	set_64bit_val(wqe, IRDMA_BYTE_16,
		      LS_64(info->stag_key, IRDMA_CQPSQ_STAG_KEY) |
		      LS_64(info->stag_idx, IRDMA_CQPSQ_STAG_IDX));
	if (!info->chunk_size)
		set_64bit_val(wqe, IRDMA_BYTE_32, info->reg_addr_pa);
	else
		set_64bit_val(wqe, IRDMA_BYTE_48,
			      LS_64(info->first_pm_pbl_index, IRDMA_CQPSQ_STAG_FIRSTPMPBLIDX));

	set_64bit_val(wqe, IRDMA_BYTE_40, info->hmc_fcn_index);

	addr_type = (info->addr_type == IRDMA_ADDR_TYPE_VA_BASED) ? 1 : 0;
	hdr = LS_64(IRDMA_CQP_OP_REG_MR, IRDMA_CQPSQ_OPCODE) |
	    LS_64(1, IRDMA_CQPSQ_STAG_MR) |
	    LS_64(info->chunk_size, IRDMA_CQPSQ_STAG_LPBLSIZE) |
	    LS_64(page_size, IRDMA_CQPSQ_STAG_HPAGESIZE) |
	    LS_64(info->access_rights, IRDMA_CQPSQ_STAG_ARIGHTS) |
	    LS_64(remote_access, IRDMA_CQPSQ_STAG_REMACCENABLED) |
	    LS_64(addr_type, IRDMA_CQPSQ_STAG_VABASEDTO) |
	    LS_64(info->use_hmc_fcn_index, IRDMA_CQPSQ_STAG_USEHMCFNIDX) |
	    LS_64(info->use_pf_rid, IRDMA_CQPSQ_STAG_USEPFRID) |
	    LS_64(cqp->polarity, IRDMA_CQPSQ_WQEVALID);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	irdma_debug_buf(dev, IRDMA_DEBUG_WQE, "MR_REG_NS WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);
	if (post_sq)
		irdma_sc_cqp_post_sq(cqp);

	return 0;
}

/**
 * irdma_sc_dealloc_stag - deallocate stag
 * @dev: sc device struct
 * @info: dealloc stag info
 * @scratch: u64 saved to be used during cqp completion
 * @post_sq: flag for cqp db to ring
 */
static int
irdma_sc_dealloc_stag(struct irdma_sc_dev *dev,
		      struct irdma_dealloc_stag_info *info,
		      u64 scratch, bool post_sq)
{
	u64 hdr;
	__le64 *wqe;
	struct irdma_sc_cqp *cqp;

	cqp = dev->cqp;
	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;

	set_64bit_val(wqe, IRDMA_BYTE_8,
		      FLD_LS_64(dev, info->pd_id, IRDMA_CQPSQ_STAG_PDID));
	set_64bit_val(wqe, IRDMA_BYTE_16,
		      LS_64(info->stag_idx, IRDMA_CQPSQ_STAG_IDX));

	hdr = LS_64(IRDMA_CQP_OP_DEALLOC_STAG, IRDMA_CQPSQ_OPCODE) |
	    LS_64(info->mr, IRDMA_CQPSQ_STAG_MR) |
	    LS_64(cqp->polarity, IRDMA_CQPSQ_WQEVALID);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	irdma_debug_buf(dev, IRDMA_DEBUG_WQE, "DEALLOC_STAG WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);
	if (post_sq)
		irdma_sc_cqp_post_sq(cqp);

	return 0;
}

/**
 * irdma_sc_mw_alloc - mw allocate
 * @dev: sc device struct
 * @info: memory window allocation information
 * @scratch: u64 saved to be used during cqp completion
 * @post_sq: flag for cqp db to ring
 */
static int
irdma_sc_mw_alloc(struct irdma_sc_dev *dev,
		  struct irdma_mw_alloc_info *info, u64 scratch,
		  bool post_sq)
{
	u64 hdr;
	struct irdma_sc_cqp *cqp;
	__le64 *wqe;

	cqp = dev->cqp;
	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;

	set_64bit_val(wqe, IRDMA_BYTE_8,
		      FLD_LS_64(dev, info->pd_id, IRDMA_CQPSQ_STAG_PDID));
	set_64bit_val(wqe, IRDMA_BYTE_16,
		      LS_64(info->mw_stag_index, IRDMA_CQPSQ_STAG_IDX));

	hdr = LS_64(IRDMA_CQP_OP_ALLOC_STAG, IRDMA_CQPSQ_OPCODE) |
	    LS_64(info->mw_wide, IRDMA_CQPSQ_STAG_MWTYPE) |
	    LS_64(info->mw1_bind_dont_vldt_key,
		  IRDMA_CQPSQ_STAG_MW1_BIND_DONT_VLDT_KEY) |
	    LS_64(cqp->polarity, IRDMA_CQPSQ_WQEVALID);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	irdma_debug_buf(dev, IRDMA_DEBUG_WQE, "MW_ALLOC WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);
	if (post_sq)
		irdma_sc_cqp_post_sq(cqp);

	return 0;
}

/**
 * irdma_sc_mr_fast_register - Posts RDMA fast register mr WR to iwarp qp
 * @qp: sc qp struct
 * @info: fast mr info
 * @post_sq: flag for cqp db to ring
 */
int
irdma_sc_mr_fast_register(struct irdma_sc_qp *qp,
			  struct irdma_fast_reg_stag_info *info,
			  bool post_sq)
{
	u64 temp, hdr;
	__le64 *wqe;
	u32 wqe_idx;
	enum irdma_page_size page_size;
	struct irdma_post_sq_info sq_info = {0};

	if (info->page_size == 0x40000000)
		page_size = IRDMA_PAGE_SIZE_1G;
	else if (info->page_size == 0x200000)
		page_size = IRDMA_PAGE_SIZE_2M;
	else
		page_size = IRDMA_PAGE_SIZE_4K;

	sq_info.wr_id = info->wr_id;
	sq_info.signaled = info->signaled;
	sq_info.push_wqe = info->push_wqe;

	wqe = irdma_qp_get_next_send_wqe(&qp->qp_uk, &wqe_idx,
					 IRDMA_QP_WQE_MIN_QUANTA, 0, &sq_info);
	if (!wqe)
		return -ENOSPC;

	irdma_clr_wqes(&qp->qp_uk, wqe_idx);

	qp->qp_uk.sq_wrtrk_array[wqe_idx].signaled = info->signaled;
	irdma_debug(qp->dev, IRDMA_DEBUG_MR,
		    "wr_id[%llxh] wqe_idx[%04d] location[%p]\n", (unsigned long long)info->wr_id,
		    wqe_idx, &qp->qp_uk.sq_wrtrk_array[wqe_idx].wrid);

	temp = (info->addr_type == IRDMA_ADDR_TYPE_VA_BASED) ?
	    (uintptr_t)info->va : info->fbo;
	set_64bit_val(wqe, IRDMA_BYTE_0, temp);

	temp = RS_64(info->first_pm_pbl_index >> 16, IRDMAQPSQ_FIRSTPMPBLIDXHI);
	set_64bit_val(wqe, IRDMA_BYTE_8,
		      LS_64(temp, IRDMAQPSQ_FIRSTPMPBLIDXHI) |
		      LS_64(info->reg_addr_pa >> IRDMAQPSQ_PBLADDR_S, IRDMAQPSQ_PBLADDR));
	set_64bit_val(wqe, IRDMA_BYTE_16,
		      info->total_len |
		      LS_64(info->first_pm_pbl_index, IRDMAQPSQ_FIRSTPMPBLIDXLO));

	hdr = LS_64(info->stag_key, IRDMAQPSQ_STAGKEY) |
	    LS_64(info->stag_idx, IRDMAQPSQ_STAGINDEX) |
	    LS_64(IRDMAQP_OP_FAST_REGISTER, IRDMAQPSQ_OPCODE) |
	    LS_64(info->chunk_size, IRDMAQPSQ_LPBLSIZE) |
	    LS_64(page_size, IRDMAQPSQ_HPAGESIZE) |
	    LS_64(info->access_rights, IRDMAQPSQ_STAGRIGHTS) |
	    LS_64(info->addr_type, IRDMAQPSQ_VABASEDTO) |
	    LS_64((sq_info.push_wqe ? 1 : 0), IRDMAQPSQ_PUSHWQE) |
	    LS_64(info->read_fence, IRDMAQPSQ_READFENCE) |
	    LS_64(info->local_fence, IRDMAQPSQ_LOCALFENCE) |
	    LS_64(info->signaled, IRDMAQPSQ_SIGCOMPL) |
	    LS_64(qp->qp_uk.swqe_polarity, IRDMAQPSQ_VALID);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	irdma_debug_buf(qp->dev, IRDMA_DEBUG_WQE, "FAST_REG WQE", wqe,
			IRDMA_QP_WQE_MIN_SIZE);
	if (sq_info.push_wqe) {
		irdma_qp_push_wqe(&qp->qp_uk, wqe, IRDMA_QP_WQE_MIN_QUANTA,
				  wqe_idx, post_sq);
	} else {
		if (post_sq)
			irdma_uk_qp_post_wr(&qp->qp_uk);
	}

	return 0;
}

/**
 * irdma_sc_gen_rts_ae - request AE generated after RTS
 * @qp: sc qp struct
 */
static void
irdma_sc_gen_rts_ae(struct irdma_sc_qp *qp)
{
	__le64 *wqe;
	u64 hdr;
	struct irdma_qp_uk *qp_uk;

	qp_uk = &qp->qp_uk;

	wqe = qp_uk->sq_base[1].elem;

	hdr = LS_64(IRDMAQP_OP_NOP, IRDMAQPSQ_OPCODE) |
	    LS_64(1, IRDMAQPSQ_LOCALFENCE) |
	    LS_64(qp->qp_uk.swqe_polarity, IRDMAQPSQ_VALID);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);
	irdma_debug_buf(qp->dev, IRDMA_DEBUG_QP, "NOP W/LOCAL FENCE WQE", wqe,
			IRDMA_QP_WQE_MIN_SIZE);

	wqe = qp_uk->sq_base[2].elem;
	hdr = LS_64(IRDMAQP_OP_GEN_RTS_AE, IRDMAQPSQ_OPCODE) |
	    LS_64(qp->qp_uk.swqe_polarity, IRDMAQPSQ_VALID);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);
	irdma_debug_buf(qp->dev, IRDMA_DEBUG_QP, "CONN EST WQE", wqe,
			IRDMA_QP_WQE_MIN_SIZE);
}

/**
 * irdma_sc_send_lsmm - send last streaming mode message
 * @qp: sc qp struct
 * @lsmm_buf: buffer with lsmm message
 * @size: size of lsmm buffer
 * @stag: stag of lsmm buffer
 */
int
irdma_sc_send_lsmm(struct irdma_sc_qp *qp, void *lsmm_buf, u32 size,
		   irdma_stag stag)
{
	__le64 *wqe;
	u64 hdr;
	struct irdma_qp_uk *qp_uk;

	qp_uk = &qp->qp_uk;
	wqe = qp_uk->sq_base->elem;

	set_64bit_val(wqe, IRDMA_BYTE_0, (uintptr_t)lsmm_buf);
	if (qp->qp_uk.uk_attrs->hw_rev == IRDMA_GEN_1) {
		set_64bit_val(wqe, IRDMA_BYTE_8,
			      LS_64(size, IRDMAQPSQ_GEN1_FRAG_LEN) |
			      LS_64(stag, IRDMAQPSQ_GEN1_FRAG_STAG));
	} else {
		set_64bit_val(wqe, IRDMA_BYTE_8,
			      LS_64(size, IRDMAQPSQ_FRAG_LEN) |
			      LS_64(stag, IRDMAQPSQ_FRAG_STAG) |
			      LS_64(qp->qp_uk.swqe_polarity, IRDMAQPSQ_VALID));
	}
	set_64bit_val(wqe, IRDMA_BYTE_16, 0);

	hdr = LS_64(IRDMAQP_OP_RDMA_SEND, IRDMAQPSQ_OPCODE) |
	    LS_64(1, IRDMAQPSQ_STREAMMODE) |
	    LS_64(1, IRDMAQPSQ_WAITFORRCVPDU) |
	    LS_64(qp->qp_uk.swqe_polarity, IRDMAQPSQ_VALID);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	irdma_debug_buf(qp->dev, IRDMA_DEBUG_WQE, "SEND_LSMM WQE", wqe,
			IRDMA_QP_WQE_MIN_SIZE);

	if (qp->dev->hw_attrs.uk_attrs.feature_flags & IRDMA_FEATURE_RTS_AE)
		irdma_sc_gen_rts_ae(qp);

	return 0;
}

/**
 * irdma_sc_send_lsmm_nostag - for privilege qp
 * @qp: sc qp struct
 * @lsmm_buf: buffer with lsmm message
 * @size: size of lsmm buffer
 */
int
irdma_sc_send_lsmm_nostag(struct irdma_sc_qp *qp, void *lsmm_buf, u32 size)
{
	__le64 *wqe;
	u64 hdr;
	struct irdma_qp_uk *qp_uk;

	qp_uk = &qp->qp_uk;
	wqe = qp_uk->sq_base->elem;

	set_64bit_val(wqe, IRDMA_BYTE_0, (uintptr_t)lsmm_buf);

	if (qp->qp_uk.uk_attrs->hw_rev == IRDMA_GEN_1)
		set_64bit_val(wqe, IRDMA_BYTE_8,
			      LS_64(size, IRDMAQPSQ_GEN1_FRAG_LEN));
	else
		set_64bit_val(wqe, IRDMA_BYTE_8,
			      LS_64(size, IRDMAQPSQ_FRAG_LEN) |
			      LS_64(qp->qp_uk.swqe_polarity, IRDMAQPSQ_VALID));
	set_64bit_val(wqe, IRDMA_BYTE_16, 0);

	hdr = LS_64(IRDMAQP_OP_RDMA_SEND, IRDMAQPSQ_OPCODE) |
	    LS_64(1, IRDMAQPSQ_STREAMMODE) |
	    LS_64(1, IRDMAQPSQ_WAITFORRCVPDU) |
	    LS_64(qp->qp_uk.swqe_polarity, IRDMAQPSQ_VALID);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	irdma_debug_buf(qp->dev, IRDMA_DEBUG_WQE, "SEND_LSMM_NOSTAG WQE", wqe,
			IRDMA_QP_WQE_MIN_SIZE);

	return 0;
}

/**
 * irdma_sc_send_rtt - send last read0 or write0
 * @qp: sc qp struct
 * @read: Do read0 or write0
 */
int
irdma_sc_send_rtt(struct irdma_sc_qp *qp, bool read)
{
	__le64 *wqe;
	u64 hdr;
	struct irdma_qp_uk *qp_uk;

	qp_uk = &qp->qp_uk;
	wqe = qp_uk->sq_base->elem;

	set_64bit_val(wqe, IRDMA_BYTE_0, 0);
	set_64bit_val(wqe, IRDMA_BYTE_16, 0);
	if (read) {
		if (qp->qp_uk.uk_attrs->hw_rev == IRDMA_GEN_1) {
			set_64bit_val(wqe, IRDMA_BYTE_8,
				      LS_64(0xabcd, IRDMAQPSQ_GEN1_FRAG_STAG));
		} else {
			set_64bit_val(wqe, IRDMA_BYTE_8,
				      (u64)0xabcd | LS_64(qp->qp_uk.swqe_polarity,
							   IRDMAQPSQ_VALID));
		}
		hdr = LS_64(0x1234, IRDMAQPSQ_REMSTAG) |
		    LS_64(IRDMAQP_OP_RDMA_READ, IRDMAQPSQ_OPCODE) |
		    LS_64(qp->qp_uk.swqe_polarity, IRDMAQPSQ_VALID);

	} else {
		if (qp->qp_uk.uk_attrs->hw_rev == IRDMA_GEN_1) {
			set_64bit_val(wqe, IRDMA_BYTE_8, 0);
		} else {
			set_64bit_val(wqe, IRDMA_BYTE_8,
				      LS_64(qp->qp_uk.swqe_polarity,
					    IRDMAQPSQ_VALID));
		}
		hdr = LS_64(IRDMAQP_OP_RDMA_WRITE, IRDMAQPSQ_OPCODE) |
		    LS_64(qp->qp_uk.swqe_polarity, IRDMAQPSQ_VALID);
	}

	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	irdma_debug_buf(qp->dev, IRDMA_DEBUG_WQE, "RTR WQE", wqe,
			IRDMA_QP_WQE_MIN_SIZE);

	if (qp->dev->hw_attrs.uk_attrs.feature_flags & IRDMA_FEATURE_RTS_AE)
		irdma_sc_gen_rts_ae(qp);

	return 0;
}

/**
 * irdma_iwarp_opcode - determine if incoming is rdma layer
 * @info: aeq info for the packet
 * @pkt: packet for error
 */
static u32 irdma_iwarp_opcode(struct irdma_aeqe_info *info, u8 *pkt){
	BE16 *mpa;
	u32 opcode = 0xffffffff;

	if (info->q2_data_written) {
		mpa = (BE16 *) pkt;
		opcode = IRDMA_NTOHS(mpa[1]) & 0xf;
	}

	return opcode;
}

/**
 * irdma_locate_mpa - return pointer to mpa in the pkt
 * @pkt: packet with data
 */
static u8 *irdma_locate_mpa(u8 *pkt) {
	/* skip over ethernet header */
	pkt += IRDMA_MAC_HLEN;

	/* Skip over IP and TCP headers */
	pkt += 4 * (pkt[0] & 0x0f);
	pkt += 4 * ((pkt[12] >> 4) & 0x0f);

	return pkt;
}

/**
 * irdma_bld_termhdr_ctrl - setup terminate hdr control fields
 * @qp: sc qp ptr for pkt
 * @hdr: term hdr
 * @opcode: flush opcode for termhdr
 * @layer_etype: error layer + error type
 * @err: error cod ein the header
 */
static void
irdma_bld_termhdr_ctrl(struct irdma_sc_qp *qp,
		       struct irdma_terminate_hdr *hdr,
		       enum irdma_flush_opcode opcode,
		       u8 layer_etype, u8 err)
{
	qp->flush_code = opcode;
	hdr->layer_etype = layer_etype;
	hdr->error_code = err;
}

/**
 * irdma_bld_termhdr_ddp_rdma - setup ddp and rdma hdrs in terminate hdr
 * @pkt: ptr to mpa in offending pkt
 * @hdr: term hdr
 * @copy_len: offending pkt length to be copied to term hdr
 * @is_tagged: DDP tagged or untagged
 */
static void
irdma_bld_termhdr_ddp_rdma(u8 *pkt, struct irdma_terminate_hdr *hdr,
			   int *copy_len, u8 *is_tagged)
{
	u16 ddp_seg_len;

	ddp_seg_len = IRDMA_NTOHS(*(BE16 *) pkt);
	if (ddp_seg_len) {
		*copy_len = 2;
		hdr->hdrct = DDP_LEN_FLAG;
		if (pkt[2] & 0x80) {
			*is_tagged = 1;
			if (ddp_seg_len >= TERM_DDP_LEN_TAGGED) {
				*copy_len += TERM_DDP_LEN_TAGGED;
				hdr->hdrct |= DDP_HDR_FLAG;
			}
		} else {
			if (ddp_seg_len >= TERM_DDP_LEN_UNTAGGED) {
				*copy_len += TERM_DDP_LEN_UNTAGGED;
				hdr->hdrct |= DDP_HDR_FLAG;
			}
			if (ddp_seg_len >= (TERM_DDP_LEN_UNTAGGED + TERM_RDMA_LEN) &&
			    ((pkt[3] & RDMA_OPCODE_M) == RDMA_READ_REQ_OPCODE)) {
				*copy_len += TERM_RDMA_LEN;
				hdr->hdrct |= RDMA_HDR_FLAG;
			}
		}
	}
}

/**
 * irdma_bld_terminate_hdr - build terminate message header
 * @qp: qp associated with received terminate AE
 * @info: the struct contiaing AE information
 */
static int
irdma_bld_terminate_hdr(struct irdma_sc_qp *qp,
			struct irdma_aeqe_info *info)
{
	u8 *pkt = qp->q2_buf + Q2_BAD_FRAME_OFFSET;
	int copy_len = 0;
	u8 is_tagged = 0;
	u32 opcode;
	struct irdma_terminate_hdr *termhdr;

	termhdr = (struct irdma_terminate_hdr *)qp->q2_buf;
	memset(termhdr, 0, Q2_BAD_FRAME_OFFSET);

	if (info->q2_data_written) {
		pkt = irdma_locate_mpa(pkt);
		irdma_bld_termhdr_ddp_rdma(pkt, termhdr, &copy_len, &is_tagged);
	}

	opcode = irdma_iwarp_opcode(info, pkt);
	qp->event_type = IRDMA_QP_EVENT_CATASTROPHIC;
	qp->sq_flush_code = info->sq;
	qp->rq_flush_code = info->rq;

	switch (info->ae_id) {
	case IRDMA_AE_AMP_UNALLOCATED_STAG:
		qp->event_type = IRDMA_QP_EVENT_ACCESS_ERR;
		if (opcode == IRDMA_OP_TYPE_RDMA_WRITE)
			irdma_bld_termhdr_ctrl(qp, termhdr, FLUSH_PROT_ERR,
					       (LAYER_DDP << 4) | DDP_TAGGED_BUF,
					       DDP_TAGGED_INV_STAG);
		else
			irdma_bld_termhdr_ctrl(qp, termhdr, FLUSH_REM_ACCESS_ERR,
					       (LAYER_RDMA << 4) | RDMAP_REMOTE_PROT,
					       RDMAP_INV_STAG);
		break;
	case IRDMA_AE_AMP_BOUNDS_VIOLATION:
		qp->event_type = IRDMA_QP_EVENT_ACCESS_ERR;
		if (info->q2_data_written)
			irdma_bld_termhdr_ctrl(qp, termhdr, FLUSH_PROT_ERR,
					       (LAYER_DDP << 4) | DDP_TAGGED_BUF,
					       DDP_TAGGED_BOUNDS);
		else
			irdma_bld_termhdr_ctrl(qp, termhdr, FLUSH_REM_ACCESS_ERR,
					       (LAYER_RDMA << 4) | RDMAP_REMOTE_PROT,
					       RDMAP_INV_BOUNDS);
		break;
	case IRDMA_AE_AMP_BAD_PD:
		switch (opcode) {
		case IRDMA_OP_TYPE_RDMA_WRITE:
			irdma_bld_termhdr_ctrl(qp, termhdr, FLUSH_PROT_ERR,
					       (LAYER_DDP << 4) | DDP_TAGGED_BUF,
					       DDP_TAGGED_UNASSOC_STAG);
			break;
		case IRDMA_OP_TYPE_SEND_INV:
		case IRDMA_OP_TYPE_SEND_SOL_INV:
			irdma_bld_termhdr_ctrl(qp, termhdr, FLUSH_REM_ACCESS_ERR,
					       (LAYER_RDMA << 4) | RDMAP_REMOTE_PROT,
					       RDMAP_CANT_INV_STAG);
			break;
		default:
			irdma_bld_termhdr_ctrl(qp, termhdr, FLUSH_REM_ACCESS_ERR,
					       (LAYER_RDMA << 4) | RDMAP_REMOTE_PROT,
					       RDMAP_UNASSOC_STAG);
		}
		break;
	case IRDMA_AE_AMP_INVALID_STAG:
		qp->event_type = IRDMA_QP_EVENT_ACCESS_ERR;
		irdma_bld_termhdr_ctrl(qp, termhdr, FLUSH_REM_ACCESS_ERR,
				       (LAYER_RDMA << 4) | RDMAP_REMOTE_PROT,
				       RDMAP_INV_STAG);
		break;
	case IRDMA_AE_AMP_BAD_QP:
		irdma_bld_termhdr_ctrl(qp, termhdr, FLUSH_LOC_QP_OP_ERR,
				       (LAYER_DDP << 4) | DDP_UNTAGGED_BUF,
				       DDP_UNTAGGED_INV_QN);
		break;
	case IRDMA_AE_AMP_BAD_STAG_KEY:
	case IRDMA_AE_AMP_BAD_STAG_INDEX:
		qp->event_type = IRDMA_QP_EVENT_ACCESS_ERR;
		switch (opcode) {
		case IRDMA_OP_TYPE_SEND_INV:
		case IRDMA_OP_TYPE_SEND_SOL_INV:
			irdma_bld_termhdr_ctrl(qp, termhdr, FLUSH_REM_OP_ERR,
					       (LAYER_RDMA << 4) | RDMAP_REMOTE_OP,
					       RDMAP_CANT_INV_STAG);
			break;
		default:
			irdma_bld_termhdr_ctrl(qp, termhdr, FLUSH_REM_ACCESS_ERR,
					       (LAYER_RDMA << 4) | RDMAP_REMOTE_OP,
					       RDMAP_INV_STAG);
		}
		break;
	case IRDMA_AE_AMP_RIGHTS_VIOLATION:
	case IRDMA_AE_AMP_INVALIDATE_NO_REMOTE_ACCESS_RIGHTS:
	case IRDMA_AE_PRIV_OPERATION_DENIED:
		qp->event_type = IRDMA_QP_EVENT_ACCESS_ERR;
		irdma_bld_termhdr_ctrl(qp, termhdr, FLUSH_REM_ACCESS_ERR,
				       (LAYER_RDMA << 4) | RDMAP_REMOTE_PROT,
				       RDMAP_ACCESS);
		break;
	case IRDMA_AE_AMP_TO_WRAP:
		qp->event_type = IRDMA_QP_EVENT_ACCESS_ERR;
		irdma_bld_termhdr_ctrl(qp, termhdr, FLUSH_REM_ACCESS_ERR,
				       (LAYER_RDMA << 4) | RDMAP_REMOTE_PROT,
				       RDMAP_TO_WRAP);
		break;
	case IRDMA_AE_LLP_RECEIVED_MPA_CRC_ERROR:
		irdma_bld_termhdr_ctrl(qp, termhdr, FLUSH_GENERAL_ERR,
				       (LAYER_MPA << 4) | DDP_LLP, MPA_CRC);
		break;
	case IRDMA_AE_LLP_SEGMENT_TOO_SMALL:
		irdma_bld_termhdr_ctrl(qp, termhdr, FLUSH_LOC_LEN_ERR,
				       (LAYER_DDP << 4) | DDP_CATASTROPHIC,
				       DDP_CATASTROPHIC_LOCAL);
		break;
	case IRDMA_AE_LCE_QP_CATASTROPHIC:
	case IRDMA_AE_DDP_NO_L_BIT:
		irdma_bld_termhdr_ctrl(qp, termhdr, FLUSH_FATAL_ERR,
				       (LAYER_DDP << 4) | DDP_CATASTROPHIC,
				       DDP_CATASTROPHIC_LOCAL);
		break;
	case IRDMA_AE_DDP_INVALID_MSN_GAP_IN_MSN:
		irdma_bld_termhdr_ctrl(qp, termhdr, FLUSH_GENERAL_ERR,
				       (LAYER_DDP << 4) | DDP_UNTAGGED_BUF,
				       DDP_UNTAGGED_INV_MSN_RANGE);
		break;
	case IRDMA_AE_DDP_UBE_DDP_MESSAGE_TOO_LONG_FOR_AVAILABLE_BUFFER:
		qp->event_type = IRDMA_QP_EVENT_ACCESS_ERR;
		irdma_bld_termhdr_ctrl(qp, termhdr, FLUSH_LOC_LEN_ERR,
				       (LAYER_DDP << 4) | DDP_UNTAGGED_BUF,
				       DDP_UNTAGGED_INV_TOO_LONG);
		break;
	case IRDMA_AE_DDP_UBE_INVALID_DDP_VERSION:
		if (is_tagged)
			irdma_bld_termhdr_ctrl(qp, termhdr, FLUSH_GENERAL_ERR,
					       (LAYER_DDP << 4) | DDP_TAGGED_BUF,
					       DDP_TAGGED_INV_DDP_VER);
		else
			irdma_bld_termhdr_ctrl(qp, termhdr, FLUSH_GENERAL_ERR,
					       (LAYER_DDP << 4) | DDP_UNTAGGED_BUF,
					       DDP_UNTAGGED_INV_DDP_VER);
		break;
	case IRDMA_AE_DDP_UBE_INVALID_MO:
		irdma_bld_termhdr_ctrl(qp, termhdr, FLUSH_GENERAL_ERR,
				       (LAYER_DDP << 4) | DDP_UNTAGGED_BUF,
				       DDP_UNTAGGED_INV_MO);
		break;
	case IRDMA_AE_DDP_UBE_INVALID_MSN_NO_BUFFER_AVAILABLE:
		irdma_bld_termhdr_ctrl(qp, termhdr, FLUSH_REM_OP_ERR,
				       (LAYER_DDP << 4) | DDP_UNTAGGED_BUF,
				       DDP_UNTAGGED_INV_MSN_NO_BUF);
		break;
	case IRDMA_AE_DDP_UBE_INVALID_QN:
		irdma_bld_termhdr_ctrl(qp, termhdr, FLUSH_GENERAL_ERR,
				       (LAYER_DDP << 4) | DDP_UNTAGGED_BUF,
				       DDP_UNTAGGED_INV_QN);
		break;
	case IRDMA_AE_RDMAP_ROE_INVALID_RDMAP_VERSION:
		irdma_bld_termhdr_ctrl(qp, termhdr, FLUSH_GENERAL_ERR,
				       (LAYER_RDMA << 4) | RDMAP_REMOTE_OP,
				       RDMAP_INV_RDMAP_VER);
		break;
	default:
		irdma_bld_termhdr_ctrl(qp, termhdr, FLUSH_FATAL_ERR,
				       (LAYER_RDMA << 4) | RDMAP_REMOTE_OP,
				       RDMAP_UNSPECIFIED);
		break;
	}

	if (copy_len)
		irdma_memcpy(termhdr + 1, pkt, copy_len);

	return sizeof(struct irdma_terminate_hdr) + copy_len;
}

/**
 * irdma_terminate_send_fin() - Send fin for terminate message
 * @qp: qp associated with received terminate AE
 */
void
irdma_terminate_send_fin(struct irdma_sc_qp *qp)
{
	irdma_term_modify_qp(qp, IRDMA_QP_STATE_TERMINATE,
			     IRDMAQP_TERM_SEND_FIN_ONLY, 0);
}

/**
 * irdma_terminate_connection() - Bad AE and send terminate to remote QP
 * @qp: qp associated with received terminate AE
 * @info: the struct contiaing AE information
 */
void
irdma_terminate_connection(struct irdma_sc_qp *qp,
			   struct irdma_aeqe_info *info)
{
	u8 termlen = 0;

	if (qp->term_flags & IRDMA_TERM_SENT)
		return;

	termlen = irdma_bld_terminate_hdr(qp, info);
	irdma_terminate_start_timer(qp);
	qp->term_flags |= IRDMA_TERM_SENT;
	irdma_term_modify_qp(qp, IRDMA_QP_STATE_TERMINATE,
			     IRDMAQP_TERM_SEND_TERM_ONLY, termlen);
}

/**
 * irdma_terminate_received - handle terminate received AE
 * @qp: qp associated with received terminate AE
 * @info: the struct contiaing AE information
 */
void
irdma_terminate_received(struct irdma_sc_qp *qp,
			 struct irdma_aeqe_info *info)
{
	u8 *pkt = qp->q2_buf + Q2_BAD_FRAME_OFFSET;
	BE32 *mpa;
	u8 ddp_ctl;
	u8 rdma_ctl;
	u16 aeq_id = 0;
	struct irdma_terminate_hdr *termhdr;

	mpa = (BE32 *) irdma_locate_mpa(pkt);
	if (info->q2_data_written) {
		/* did not validate the frame - do it now */
		ddp_ctl = (ntohl(mpa[0]) >> 8) & 0xff;
		rdma_ctl = ntohl(mpa[0]) & 0xff;
		if ((ddp_ctl & 0xc0) != 0x40)
			aeq_id = IRDMA_AE_LCE_QP_CATASTROPHIC;
		else if ((ddp_ctl & 0x03) != 1)
			aeq_id = IRDMA_AE_DDP_UBE_INVALID_DDP_VERSION;
		else if (ntohl(mpa[2]) != 2)
			aeq_id = IRDMA_AE_DDP_UBE_INVALID_QN;
		else if (ntohl(mpa[3]) != 1)
			aeq_id = IRDMA_AE_DDP_INVALID_MSN_GAP_IN_MSN;
		else if (ntohl(mpa[4]) != 0)
			aeq_id = IRDMA_AE_DDP_UBE_INVALID_MO;
		else if ((rdma_ctl & 0xc0) != 0x40)
			aeq_id = IRDMA_AE_RDMAP_ROE_INVALID_RDMAP_VERSION;

		info->ae_id = aeq_id;
		if (info->ae_id) {
			/* Bad terminate recvd - send back a terminate */
			irdma_terminate_connection(qp, info);
			return;
		}
	}

	qp->term_flags |= IRDMA_TERM_RCVD;
	qp->event_type = IRDMA_QP_EVENT_CATASTROPHIC;
	termhdr = (struct irdma_terminate_hdr *)&mpa[5];
	if (termhdr->layer_etype == RDMAP_REMOTE_PROT ||
	    termhdr->layer_etype == RDMAP_REMOTE_OP) {
		irdma_terminate_done(qp, 0);
	} else {
		irdma_terminate_start_timer(qp);
		irdma_terminate_send_fin(qp);
	}
}

static int
irdma_null_ws_add(struct irdma_sc_vsi *vsi, u8 user_pri)
{
	return 0;
}

static void
irdma_null_ws_remove(struct irdma_sc_vsi *vsi, u8 user_pri)
{
	/* do nothing */
}

static void
irdma_null_ws_reset(struct irdma_sc_vsi *vsi)
{
	/* do nothing */
}

/**
 * irdma_sc_vsi_init - Init the vsi structure
 * @vsi: pointer to vsi structure to initialize
 * @info: the info used to initialize the vsi struct
 */
void
irdma_sc_vsi_init(struct irdma_sc_vsi *vsi,
		  struct irdma_vsi_init_info *info)
{
	u8 i;

	vsi->dev = info->dev;
	vsi->back_vsi = info->back_vsi;
	vsi->register_qset = info->register_qset;
	vsi->unregister_qset = info->unregister_qset;
	vsi->mtu = info->params->mtu;
	vsi->exception_lan_q = info->exception_lan_q;
	vsi->vsi_idx = info->pf_data_vsi_num;
	if (vsi->dev->hw_attrs.uk_attrs.hw_rev == IRDMA_GEN_1)
		vsi->fcn_id = info->dev->hmc_fn_id;

	irdma_set_qos_info(vsi, info->params);
	for (i = 0; i < IRDMA_MAX_USER_PRIORITY; i++) {
		mutex_init(&vsi->qos[i].qos_mutex);
		INIT_LIST_HEAD(&vsi->qos[i].qplist);
	}
	if (vsi->register_qset) {
		vsi->dev->ws_add = irdma_ws_add;
		vsi->dev->ws_remove = irdma_ws_remove;
		vsi->dev->ws_reset = irdma_ws_reset;
	} else {
		vsi->dev->ws_add = irdma_null_ws_add;
		vsi->dev->ws_remove = irdma_null_ws_remove;
		vsi->dev->ws_reset = irdma_null_ws_reset;
	}
}

/**
 * irdma_get_fcn_id - Return the function id
 * @vsi: pointer to the vsi
 */
static u8 irdma_get_fcn_id(struct irdma_sc_vsi *vsi){
	struct irdma_stats_inst_info stats_info = {0};
	struct irdma_sc_dev *dev = vsi->dev;
	u8 fcn_id = IRDMA_INVALID_FCN_ID;
	u8 start_idx, max_stats, i;

	if (dev->hw_attrs.uk_attrs.hw_rev != IRDMA_GEN_1) {
		if (!irdma_cqp_stats_inst_cmd(vsi, IRDMA_OP_STATS_ALLOCATE,
					      &stats_info))
			return stats_info.stats_idx;
	}

	start_idx = 1;
	max_stats = 16;
	for (i = start_idx; i < max_stats; i++)
		if (!dev->fcn_id_array[i]) {
			fcn_id = i;
			dev->fcn_id_array[i] = true;
			break;
		}

	return fcn_id;
}

/**
 * irdma_vsi_stats_init - Initialize the vsi statistics
 * @vsi: pointer to the vsi structure
 * @info: The info structure used for initialization
 */
int
irdma_vsi_stats_init(struct irdma_sc_vsi *vsi,
		     struct irdma_vsi_stats_info *info)
{
	u8 fcn_id = info->fcn_id;
	struct irdma_dma_mem *stats_buff_mem;

	vsi->pestat = info->pestat;
	vsi->pestat->hw = vsi->dev->hw;
	vsi->pestat->vsi = vsi;
	stats_buff_mem = &vsi->pestat->gather_info.stats_buff_mem;
	stats_buff_mem->size = IRDMA_GATHER_STATS_BUF_SIZE * 2;
	stats_buff_mem->va = irdma_allocate_dma_mem(vsi->pestat->hw,
						    stats_buff_mem,
						    stats_buff_mem->size, 1);
	if (!stats_buff_mem->va)
		return -ENOMEM;

	vsi->pestat->gather_info.gather_stats_va = stats_buff_mem->va;
	vsi->pestat->gather_info.last_gather_stats_va =
	    (void *)((uintptr_t)stats_buff_mem->va +
		     IRDMA_GATHER_STATS_BUF_SIZE);

	irdma_hw_stats_start_timer(vsi);
	if (info->alloc_fcn_id)
		fcn_id = irdma_get_fcn_id(vsi);
	if (fcn_id == IRDMA_INVALID_FCN_ID)
		goto stats_error;

	vsi->stats_fcn_id_alloc = info->alloc_fcn_id;
	vsi->fcn_id = fcn_id;
	if (info->alloc_fcn_id) {
		vsi->pestat->gather_info.use_stats_inst = true;
		vsi->pestat->gather_info.stats_inst_index = fcn_id;
	}

	return 0;

stats_error:
	irdma_free_dma_mem(vsi->pestat->hw, stats_buff_mem);

	return -EIO;
}

/**
 * irdma_vsi_stats_free - Free the vsi stats
 * @vsi: pointer to the vsi structure
 */
void
irdma_vsi_stats_free(struct irdma_sc_vsi *vsi)
{
	struct irdma_stats_inst_info stats_info = {0};
	u8 fcn_id = vsi->fcn_id;
	struct irdma_sc_dev *dev = vsi->dev;

	if (dev->hw_attrs.uk_attrs.hw_rev != IRDMA_GEN_1) {
		if (vsi->stats_fcn_id_alloc) {
			stats_info.stats_idx = vsi->fcn_id;
			irdma_cqp_stats_inst_cmd(vsi, IRDMA_OP_STATS_FREE,
						 &stats_info);
		}
	} else {
		if (vsi->stats_fcn_id_alloc &&
		    fcn_id < vsi->dev->hw_attrs.max_stat_inst)
			vsi->dev->fcn_id_array[fcn_id] = false;
	}

	if (!vsi->pestat)
		return;
	irdma_hw_stats_stop_timer(vsi);
	irdma_free_dma_mem(vsi->pestat->hw,
			   &vsi->pestat->gather_info.stats_buff_mem);
}

/**
 * irdma_get_encoded_wqe_size - given wq size, returns hardware encoded size
 * @wqsize: size of the wq (sq, rq) to encoded_size
 * @queue_type: queue type selected for the calculation algorithm
 */
u8
irdma_get_encoded_wqe_size(u32 wqsize, enum irdma_queue_type queue_type)
{
	u8 encoded_size = 0;

	/*
	 * cqp sq's hw coded value starts from 1 for size of 4 while it starts from 0 for qp' wq's.
	 */
	if (queue_type == IRDMA_QUEUE_TYPE_CQP)
		encoded_size = 1;
	wqsize >>= 2;
	while (wqsize >>= 1)
		encoded_size++;

	return encoded_size;
}

/**
 * irdma_sc_gather_stats - collect the statistics
 * @cqp: struct for cqp hw
 * @info: gather stats info structure
 * @scratch: u64 saved to be used during cqp completion
 */
static int
irdma_sc_gather_stats(struct irdma_sc_cqp *cqp,
		      struct irdma_stats_gather_info *info,
		      u64 scratch)
{
	__le64 *wqe;
	u64 temp;

	if (info->stats_buff_mem.size < IRDMA_GATHER_STATS_BUF_SIZE)
		return -ENOSPC;

	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;

	set_64bit_val(wqe, IRDMA_BYTE_40,
		      LS_64(info->hmc_fcn_index, IRDMA_CQPSQ_STATS_HMC_FCN_INDEX));
	set_64bit_val(wqe, IRDMA_BYTE_32, info->stats_buff_mem.pa);

	temp = LS_64(cqp->polarity, IRDMA_CQPSQ_STATS_WQEVALID) |
	    LS_64(info->use_stats_inst, IRDMA_CQPSQ_STATS_USE_INST) |
	    LS_64(info->stats_inst_index, IRDMA_CQPSQ_STATS_INST_INDEX) |
	    LS_64(info->use_hmc_fcn_index,
		  IRDMA_CQPSQ_STATS_USE_HMC_FCN_INDEX) |
	    LS_64(IRDMA_CQP_OP_GATHER_STATS, IRDMA_CQPSQ_STATS_OP);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, temp);

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_STATS, "GATHER_STATS WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);

	irdma_sc_cqp_post_sq(cqp);
	irdma_debug(cqp->dev, IRDMA_DEBUG_STATS,
		    "CQP SQ head 0x%x tail 0x%x size 0x%x\n", cqp->sq_ring.head,
		    cqp->sq_ring.tail, cqp->sq_ring.size);

	return 0;
}

/**
 * irdma_sc_manage_stats_inst - allocate or free stats instance
 * @cqp: struct for cqp hw
 * @info: stats info structure
 * @alloc: alloc vs. delete flag
 * @scratch: u64 saved to be used during cqp completion
 */
static int
irdma_sc_manage_stats_inst(struct irdma_sc_cqp *cqp,
			   struct irdma_stats_inst_info *info,
			   bool alloc, u64 scratch)
{
	__le64 *wqe;
	u64 temp;

	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;

	set_64bit_val(wqe, IRDMA_BYTE_40,
		      LS_64(info->hmc_fn_id, IRDMA_CQPSQ_STATS_HMC_FCN_INDEX));
	temp = LS_64(cqp->polarity, IRDMA_CQPSQ_STATS_WQEVALID) |
	    LS_64(alloc, IRDMA_CQPSQ_STATS_ALLOC_INST) |
	    LS_64(info->use_hmc_fcn_index, IRDMA_CQPSQ_STATS_USE_HMC_FCN_INDEX) |
	    LS_64(info->stats_idx, IRDMA_CQPSQ_STATS_INST_INDEX) |
	    LS_64(IRDMA_CQP_OP_MANAGE_STATS, IRDMA_CQPSQ_STATS_OP);

	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, temp);

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "MANAGE_STATS WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);

	irdma_sc_cqp_post_sq(cqp);
	return 0;
}

/**
 * irdma_sc_set_up_map - set the up map table
 * @cqp: struct for cqp hw
 * @info: User priority map info
 * @scratch: u64 saved to be used during cqp completion
 */
static int
irdma_sc_set_up_map(struct irdma_sc_cqp *cqp,
		    struct irdma_up_info *info, u64 scratch)
{
	__le64 *wqe;
	u64 temp;

	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;

	temp = info->map[0] | LS_64_1(info->map[1], 8) |
	    LS_64_1(info->map[2], 16) | LS_64_1(info->map[3], 24) |
	    LS_64_1(info->map[4], 32) | LS_64_1(info->map[5], 40) |
	    LS_64_1(info->map[6], 48) | LS_64_1(info->map[7], 56);

	set_64bit_val(wqe, IRDMA_BYTE_0, temp);
	set_64bit_val(wqe, IRDMA_BYTE_40,
		      LS_64(info->cnp_up_override, IRDMA_CQPSQ_UP_CNPOVERRIDE) |
		      LS_64(info->hmc_fcn_idx, IRDMA_CQPSQ_UP_HMCFCNIDX));

	temp = LS_64(cqp->polarity, IRDMA_CQPSQ_UP_WQEVALID) |
	    LS_64(info->use_vlan, IRDMA_CQPSQ_UP_USEVLAN) |
	    LS_64(info->use_cnp_up_override, IRDMA_CQPSQ_UP_USEOVERRIDE) |
	    LS_64(IRDMA_CQP_OP_UP_MAP, IRDMA_CQPSQ_UP_OP);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, temp);

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "UPMAP WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);
	irdma_sc_cqp_post_sq(cqp);

	return 0;
}

/**
 * irdma_sc_manage_ws_node - create/modify/destroy WS node
 * @cqp: struct for cqp hw
 * @info: node info structure
 * @node_op: 0 for add 1 for modify, 2 for delete
 * @scratch: u64 saved to be used during cqp completion
 */
static int
irdma_sc_manage_ws_node(struct irdma_sc_cqp *cqp,
			struct irdma_ws_node_info *info,
			enum irdma_ws_node_op node_op, u64 scratch)
{
	__le64 *wqe;
	u64 temp = 0;

	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;

	set_64bit_val(wqe, IRDMA_BYTE_32,
		      LS_64(info->vsi, IRDMA_CQPSQ_WS_VSI) |
		      LS_64(info->weight, IRDMA_CQPSQ_WS_WEIGHT));

	temp = LS_64(cqp->polarity, IRDMA_CQPSQ_WS_WQEVALID) |
	    LS_64(node_op, IRDMA_CQPSQ_WS_NODEOP) |
	    LS_64(info->enable, IRDMA_CQPSQ_WS_ENABLENODE) |
	    LS_64(info->type_leaf, IRDMA_CQPSQ_WS_NODETYPE) |
	    LS_64(info->prio_type, IRDMA_CQPSQ_WS_PRIOTYPE) |
	    LS_64(info->tc, IRDMA_CQPSQ_WS_TC) |
	    LS_64(IRDMA_CQP_OP_WORK_SCHED_NODE, IRDMA_CQPSQ_WS_OP) |
	    LS_64(info->parent_id, IRDMA_CQPSQ_WS_PARENTID) |
	    LS_64(info->id, IRDMA_CQPSQ_WS_NODEID);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, temp);

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "MANAGE_WS WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);
	irdma_sc_cqp_post_sq(cqp);

	return 0;
}

/**
 * irdma_sc_qp_flush_wqes - flush qp's wqe
 * @qp: sc qp
 * @info: dlush information
 * @scratch: u64 saved to be used during cqp completion
 * @post_sq: flag for cqp db to ring
 */
int
irdma_sc_qp_flush_wqes(struct irdma_sc_qp *qp,
		       struct irdma_qp_flush_info *info, u64 scratch,
		       bool post_sq)
{
	u64 temp = 0;
	__le64 *wqe;
	struct irdma_sc_cqp *cqp;
	u64 hdr;
	bool flush_sq = false, flush_rq = false;

	if (info->rq && !qp->flush_rq)
		flush_rq = true;
	if (info->sq && !qp->flush_sq)
		flush_sq = true;
	qp->flush_sq |= flush_sq;
	qp->flush_rq |= flush_rq;

	if (!flush_sq && !flush_rq) {
		irdma_debug(qp->dev, IRDMA_DEBUG_CQP,
			    "Additional flush request ignored for qp %x\n", qp->qp_uk.qp_id);
		return -EALREADY;
	}

	cqp = qp->pd->dev->cqp;
	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;

	if (info->userflushcode) {
		if (flush_rq)
			temp |= LS_64(info->rq_minor_code, IRDMA_CQPSQ_FWQE_RQMNERR) |
			    LS_64(info->rq_major_code, IRDMA_CQPSQ_FWQE_RQMJERR);
		if (flush_sq)
			temp |= LS_64(info->sq_minor_code, IRDMA_CQPSQ_FWQE_SQMNERR) |
			    LS_64(info->sq_major_code, IRDMA_CQPSQ_FWQE_SQMJERR);
	}
	set_64bit_val(wqe, IRDMA_BYTE_16, temp);

	temp = (info->generate_ae) ?
	    info->ae_code | LS_64(info->ae_src, IRDMA_CQPSQ_FWQE_AESOURCE) : 0;
	set_64bit_val(wqe, IRDMA_BYTE_8, temp);

	hdr = qp->qp_uk.qp_id |
	    LS_64(IRDMA_CQP_OP_FLUSH_WQES, IRDMA_CQPSQ_OPCODE) |
	    LS_64(info->generate_ae, IRDMA_CQPSQ_FWQE_GENERATE_AE) |
	    LS_64(info->userflushcode, IRDMA_CQPSQ_FWQE_USERFLCODE) |
	    LS_64(flush_sq, IRDMA_CQPSQ_FWQE_FLUSHSQ) |
	    LS_64(flush_rq, IRDMA_CQPSQ_FWQE_FLUSHRQ) |
	    LS_64(cqp->polarity, IRDMA_CQPSQ_WQEVALID);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "QP_FLUSH WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);
	if (post_sq)
		irdma_sc_cqp_post_sq(cqp);

	return 0;
}

/**
 * irdma_sc_gen_ae - generate AE, uses flush WQE CQP OP
 * @qp: sc qp
 * @info: gen ae information
 * @scratch: u64 saved to be used during cqp completion
 * @post_sq: flag for cqp db to ring
 */
static int
irdma_sc_gen_ae(struct irdma_sc_qp *qp,
		struct irdma_gen_ae_info *info, u64 scratch,
		bool post_sq)
{
	u64 temp;
	__le64 *wqe;
	struct irdma_sc_cqp *cqp;
	u64 hdr;

	cqp = qp->pd->dev->cqp;
	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;

	temp = info->ae_code | LS_64(info->ae_src, IRDMA_CQPSQ_FWQE_AESOURCE);
	set_64bit_val(wqe, IRDMA_BYTE_8, temp);

	hdr = qp->qp_uk.qp_id | LS_64(IRDMA_CQP_OP_GEN_AE, IRDMA_CQPSQ_OPCODE) |
	    LS_64(1, IRDMA_CQPSQ_FWQE_GENERATE_AE) |
	    LS_64(cqp->polarity, IRDMA_CQPSQ_WQEVALID);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "GEN_AE WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);
	if (post_sq)
		irdma_sc_cqp_post_sq(cqp);

	return 0;
}

/*** irdma_sc_qp_upload_context - upload qp's context
 * @dev: sc device struct
 * @info: upload context info ptr for return
 * @scratch: u64 saved to be used during cqp completion
 * @post_sq: flag for cqp db to ring
 */
static int
irdma_sc_qp_upload_context(struct irdma_sc_dev *dev,
			   struct irdma_upload_context_info *info,
			   u64 scratch, bool post_sq)
{
	__le64 *wqe;
	struct irdma_sc_cqp *cqp;
	u64 hdr;

	cqp = dev->cqp;
	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;

	set_64bit_val(wqe, IRDMA_BYTE_16, info->buf_pa);

	hdr = LS_64(info->qp_id, IRDMA_CQPSQ_UCTX_QPID) |
	    LS_64(IRDMA_CQP_OP_UPLOAD_CONTEXT, IRDMA_CQPSQ_OPCODE) |
	    LS_64(info->qp_type, IRDMA_CQPSQ_UCTX_QPTYPE) |
	    LS_64(info->raw_format, IRDMA_CQPSQ_UCTX_RAWFORMAT) |
	    LS_64(info->freeze_qp, IRDMA_CQPSQ_UCTX_FREEZEQP) |
	    LS_64(cqp->polarity, IRDMA_CQPSQ_WQEVALID);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	irdma_debug_buf(dev, IRDMA_DEBUG_WQE, "QP_UPLOAD_CTX WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);
	if (post_sq)
		irdma_sc_cqp_post_sq(cqp);

	return 0;
}

/**
 * irdma_sc_manage_push_page - Handle push page
 * @cqp: struct for cqp hw
 * @info: push page info
 * @scratch: u64 saved to be used during cqp completion
 * @post_sq: flag for cqp db to ring
 */
static int
irdma_sc_manage_push_page(struct irdma_sc_cqp *cqp,
			  struct irdma_cqp_manage_push_page_info *info,
			  u64 scratch, bool post_sq)
{
	__le64 *wqe;
	u64 hdr;

	if (info->free_page &&
	    info->push_idx >= cqp->dev->hw_attrs.max_hw_device_pages)
		return -EINVAL;

	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;

	set_64bit_val(wqe, IRDMA_BYTE_16, info->qs_handle);
	hdr = LS_64(info->push_idx, IRDMA_CQPSQ_MPP_PPIDX) |
	    LS_64(info->push_page_type, IRDMA_CQPSQ_MPP_PPTYPE) |
	    LS_64(IRDMA_CQP_OP_MANAGE_PUSH_PAGES, IRDMA_CQPSQ_OPCODE) |
	    LS_64(cqp->polarity, IRDMA_CQPSQ_WQEVALID) |
	    LS_64(info->free_page, IRDMA_CQPSQ_MPP_FREE_PAGE);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "MANAGE_PUSH_PAGES WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);
	if (post_sq)
		irdma_sc_cqp_post_sq(cqp);

	return 0;
}

/**
 * irdma_sc_suspend_qp - suspend qp for param change
 * @cqp: struct for cqp hw
 * @qp: sc qp struct
 * @scratch: u64 saved to be used during cqp completion
 */
static int
irdma_sc_suspend_qp(struct irdma_sc_cqp *cqp, struct irdma_sc_qp *qp,
		    u64 scratch)
{
	u64 hdr;
	__le64 *wqe;

	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;

	hdr = LS_64(qp->qp_uk.qp_id, IRDMA_CQPSQ_SUSPENDQP_QPID) |
	    LS_64(IRDMA_CQP_OP_SUSPEND_QP, IRDMA_CQPSQ_OPCODE) |
	    LS_64(cqp->polarity, IRDMA_CQPSQ_WQEVALID);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "SUSPEND_QP WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);
	irdma_sc_cqp_post_sq(cqp);

	return 0;
}

/**
 * irdma_sc_resume_qp - resume qp after suspend
 * @cqp: struct for cqp hw
 * @qp: sc qp struct
 * @scratch: u64 saved to be used during cqp completion
 */
static int
irdma_sc_resume_qp(struct irdma_sc_cqp *cqp, struct irdma_sc_qp *qp,
		   u64 scratch)
{
	u64 hdr;
	__le64 *wqe;

	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;

	set_64bit_val(wqe, IRDMA_BYTE_16,
		      LS_64(qp->qs_handle, IRDMA_CQPSQ_RESUMEQP_QSHANDLE));

	hdr = LS_64(qp->qp_uk.qp_id, IRDMA_CQPSQ_RESUMEQP_QPID) |
	    LS_64(IRDMA_CQP_OP_RESUME_QP, IRDMA_CQPSQ_OPCODE) |
	    LS_64(cqp->polarity, IRDMA_CQPSQ_WQEVALID);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "RESUME_QP WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);
	irdma_sc_cqp_post_sq(cqp);

	return 0;
}

/**
 * irdma_sc_cq_ack - acknowledge completion q
 * @cq: cq struct
 */
static inline void
irdma_sc_cq_ack(struct irdma_sc_cq *cq)
{
	db_wr32(cq->cq_uk.cq_id, cq->cq_uk.cq_ack_db);
}

/**
 * irdma_sc_cq_init - initialize completion q
 * @cq: cq struct
 * @info: cq initialization info
 */
int
irdma_sc_cq_init(struct irdma_sc_cq *cq, struct irdma_cq_init_info *info)
{
	int ret_code;
	u32 pble_obj_cnt;

	pble_obj_cnt = info->dev->hmc_info->hmc_obj[IRDMA_HMC_IW_PBLE].cnt;
	if (info->virtual_map && info->first_pm_pbl_idx >= pble_obj_cnt)
		return -EINVAL;

	cq->cq_pa = info->cq_base_pa;
	cq->dev = info->dev;
	cq->ceq_id = info->ceq_id;
	info->cq_uk_init_info.cqe_alloc_db = cq->dev->cq_arm_db;
	info->cq_uk_init_info.cq_ack_db = cq->dev->cq_ack_db;
	ret_code = irdma_uk_cq_init(&cq->cq_uk, &info->cq_uk_init_info);
	if (ret_code)
		return ret_code;

	cq->virtual_map = info->virtual_map;
	cq->pbl_chunk_size = info->pbl_chunk_size;
	cq->ceqe_mask = info->ceqe_mask;
	cq->cq_type = (info->type) ? info->type : IRDMA_CQ_TYPE_IWARP;
	cq->shadow_area_pa = info->shadow_area_pa;
	cq->shadow_read_threshold = info->shadow_read_threshold;
	cq->ceq_id_valid = info->ceq_id_valid;
	cq->tph_en = info->tph_en;
	cq->tph_val = info->tph_val;
	cq->first_pm_pbl_idx = info->first_pm_pbl_idx;
	cq->vsi = info->vsi;

	return 0;
}

/**
 * irdma_sc_cq_create - create completion q
 * @cq: cq struct
 * @scratch: u64 saved to be used during cqp completion
 * @check_overflow: flag for overflow check
 * @post_sq: flag for cqp db to ring
 */
static int
irdma_sc_cq_create(struct irdma_sc_cq *cq, u64 scratch,
		   bool check_overflow, bool post_sq)
{
	__le64 *wqe;
	struct irdma_sc_cqp *cqp;
	u64 hdr;
	struct irdma_sc_ceq *ceq;
	int ret_code = 0;

	cqp = cq->dev->cqp;
	if (cq->cq_uk.cq_id > (cqp->dev->hmc_info->hmc_obj[IRDMA_HMC_IW_CQ].max_cnt - 1))
		return -EINVAL;

	if (cq->ceq_id > (cq->dev->hmc_fpm_misc.max_ceqs - 1))
		return -EINVAL;

	ceq = cq->dev->ceq[cq->ceq_id];
	if (ceq && ceq->reg_cq)
		ret_code = irdma_sc_add_cq_ctx(ceq, cq);

	if (ret_code)
		return ret_code;

	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe) {
		if (ceq && ceq->reg_cq)
			irdma_sc_remove_cq_ctx(ceq, cq);
		return -ENOSPC;
	}

	set_64bit_val(wqe, IRDMA_BYTE_0, cq->cq_uk.cq_size);
	set_64bit_val(wqe, IRDMA_BYTE_8, RS_64_1(cq, 1));
	set_64bit_val(wqe, IRDMA_BYTE_16,
		      LS_64(cq->shadow_read_threshold,
			    IRDMA_CQPSQ_CQ_SHADOW_READ_THRESHOLD));
	set_64bit_val(wqe, IRDMA_BYTE_32, (cq->virtual_map ? 0 : cq->cq_pa));
	set_64bit_val(wqe, IRDMA_BYTE_40, cq->shadow_area_pa);
	set_64bit_val(wqe, IRDMA_BYTE_48,
		      LS_64((cq->virtual_map ? cq->first_pm_pbl_idx : 0),
			    IRDMA_CQPSQ_CQ_FIRSTPMPBLIDX));
	set_64bit_val(wqe, IRDMA_BYTE_56,
		      LS_64(cq->tph_val, IRDMA_CQPSQ_TPHVAL) |
		      LS_64(cq->vsi->vsi_idx, IRDMA_CQPSQ_VSIIDX));

	hdr = FLD_LS_64(cq->dev, cq->cq_uk.cq_id, IRDMA_CQPSQ_CQ_CQID) |
	    FLD_LS_64(cq->dev, (cq->ceq_id_valid ? cq->ceq_id : 0),
		      IRDMA_CQPSQ_CQ_CEQID) |
	    LS_64(IRDMA_CQP_OP_CREATE_CQ, IRDMA_CQPSQ_OPCODE) |
	    LS_64(cq->pbl_chunk_size, IRDMA_CQPSQ_CQ_LPBLSIZE) |
	    LS_64(check_overflow, IRDMA_CQPSQ_CQ_CHKOVERFLOW) |
	    LS_64(cq->virtual_map, IRDMA_CQPSQ_CQ_VIRTMAP) |
	    LS_64(cq->ceqe_mask, IRDMA_CQPSQ_CQ_ENCEQEMASK) |
	    LS_64(cq->ceq_id_valid, IRDMA_CQPSQ_CQ_CEQIDVALID) |
	    LS_64(cq->tph_en, IRDMA_CQPSQ_TPHEN) |
	    LS_64(cq->cq_uk.avoid_mem_cflct, IRDMA_CQPSQ_CQ_AVOIDMEMCNFLCT) |
	    LS_64(cqp->polarity, IRDMA_CQPSQ_WQEVALID);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "CQ_CREATE WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);
	if (post_sq)
		irdma_sc_cqp_post_sq(cqp);

	return 0;
}

/**
 * irdma_sc_cq_destroy - destroy completion q
 * @cq: cq struct
 * @scratch: u64 saved to be used during cqp completion
 * @post_sq: flag for cqp db to ring
 */
int
irdma_sc_cq_destroy(struct irdma_sc_cq *cq, u64 scratch, bool post_sq)
{
	struct irdma_sc_cqp *cqp;
	__le64 *wqe;
	u64 hdr;
	struct irdma_sc_ceq *ceq;

	cqp = cq->dev->cqp;
	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;

	ceq = cq->dev->ceq[cq->ceq_id];
	if (ceq && ceq->reg_cq)
		irdma_sc_remove_cq_ctx(ceq, cq);

	set_64bit_val(wqe, IRDMA_BYTE_0, cq->cq_uk.cq_size);
	set_64bit_val(wqe, IRDMA_BYTE_8, RS_64_1(cq, 1));
	set_64bit_val(wqe, IRDMA_BYTE_40, cq->shadow_area_pa);
	set_64bit_val(wqe, IRDMA_BYTE_48,
		      (cq->virtual_map ? cq->first_pm_pbl_idx : 0));

	hdr = cq->cq_uk.cq_id |
	    FLD_LS_64(cq->dev, (cq->ceq_id_valid ? cq->ceq_id : 0),
		      IRDMA_CQPSQ_CQ_CEQID) |
	    LS_64(IRDMA_CQP_OP_DESTROY_CQ, IRDMA_CQPSQ_OPCODE) |
	    LS_64(cq->pbl_chunk_size, IRDMA_CQPSQ_CQ_LPBLSIZE) |
	    LS_64(cq->virtual_map, IRDMA_CQPSQ_CQ_VIRTMAP) |
	    LS_64(cq->ceqe_mask, IRDMA_CQPSQ_CQ_ENCEQEMASK) |
	    LS_64(cq->ceq_id_valid, IRDMA_CQPSQ_CQ_CEQIDVALID) |
	    LS_64(cq->tph_en, IRDMA_CQPSQ_TPHEN) |
	    LS_64(cq->cq_uk.avoid_mem_cflct, IRDMA_CQPSQ_CQ_AVOIDMEMCNFLCT) |
	    LS_64(cqp->polarity, IRDMA_CQPSQ_WQEVALID);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "CQ_DESTROY WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);
	if (post_sq)
		irdma_sc_cqp_post_sq(cqp);

	return 0;
}

/**
 * irdma_sc_cq_resize - set resized cq buffer info
 * @cq: resized cq
 * @info: resized cq buffer info
 */
void
irdma_sc_cq_resize(struct irdma_sc_cq *cq, struct irdma_modify_cq_info *info)
{
	cq->virtual_map = info->virtual_map;
	cq->cq_pa = info->cq_pa;
	cq->first_pm_pbl_idx = info->first_pm_pbl_idx;
	cq->pbl_chunk_size = info->pbl_chunk_size;
	irdma_uk_cq_resize(&cq->cq_uk, info->cq_base, info->cq_size);
}

/**
 * irdma_sc_cq_modify - modify a Completion Queue
 * @cq: cq struct
 * @info: modification info struct
 * @scratch: u64 saved to be used during cqp completion
 * @post_sq: flag to post to sq
 */
static int
irdma_sc_cq_modify(struct irdma_sc_cq *cq,
		   struct irdma_modify_cq_info *info, u64 scratch,
		   bool post_sq)
{
	struct irdma_sc_cqp *cqp;
	__le64 *wqe;
	u64 hdr;
	u32 pble_obj_cnt;

	pble_obj_cnt = cq->dev->hmc_info->hmc_obj[IRDMA_HMC_IW_PBLE].cnt;
	if (info->cq_resize && info->virtual_map &&
	    info->first_pm_pbl_idx >= pble_obj_cnt)
		return -EINVAL;

	cqp = cq->dev->cqp;
	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;

	set_64bit_val(wqe, IRDMA_BYTE_0, info->cq_size);
	set_64bit_val(wqe, IRDMA_BYTE_8, RS_64_1(cq, 1));
	set_64bit_val(wqe, IRDMA_BYTE_16,
		      LS_64(info->shadow_read_threshold,
			    IRDMA_CQPSQ_CQ_SHADOW_READ_THRESHOLD));
	set_64bit_val(wqe, IRDMA_BYTE_32, info->cq_pa);
	set_64bit_val(wqe, IRDMA_BYTE_40, cq->shadow_area_pa);
	set_64bit_val(wqe, IRDMA_BYTE_48, info->first_pm_pbl_idx);
	set_64bit_val(wqe, IRDMA_BYTE_56,
		      LS_64(cq->tph_val, IRDMA_CQPSQ_TPHVAL) |
		      LS_64(cq->vsi->vsi_idx, IRDMA_CQPSQ_VSIIDX));

	hdr = cq->cq_uk.cq_id |
	    LS_64(IRDMA_CQP_OP_MODIFY_CQ, IRDMA_CQPSQ_OPCODE) |
	    LS_64(info->cq_resize, IRDMA_CQPSQ_CQ_CQRESIZE) |
	    LS_64(info->pbl_chunk_size, IRDMA_CQPSQ_CQ_LPBLSIZE) |
	    LS_64(info->check_overflow, IRDMA_CQPSQ_CQ_CHKOVERFLOW) |
	    LS_64(info->virtual_map, IRDMA_CQPSQ_CQ_VIRTMAP) |
	    LS_64(cq->ceqe_mask, IRDMA_CQPSQ_CQ_ENCEQEMASK) |
	    LS_64(cq->tph_en, IRDMA_CQPSQ_TPHEN) |
	    LS_64(cq->cq_uk.avoid_mem_cflct, IRDMA_CQPSQ_CQ_AVOIDMEMCNFLCT) |
	    LS_64(cqp->polarity, IRDMA_CQPSQ_WQEVALID);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "CQ_MODIFY WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);
	if (post_sq)
		irdma_sc_cqp_post_sq(cqp);

	return 0;
}

/**
 * irdma_check_cqp_progress - check cqp processing progress
 * @timeout: timeout info struct
 * @dev: sc device struct
 */
void
irdma_check_cqp_progress(struct irdma_cqp_timeout *timeout, struct irdma_sc_dev *dev)
{
	if (timeout->compl_cqp_cmds != dev->cqp_cmd_stats[IRDMA_OP_CMPL_CMDS]) {
		timeout->compl_cqp_cmds = dev->cqp_cmd_stats[IRDMA_OP_CMPL_CMDS];
		timeout->count = 0;
	} else {
		if (dev->cqp_cmd_stats[IRDMA_OP_REQ_CMDS] !=
		    timeout->compl_cqp_cmds)
			timeout->count++;
	}
}

/**
 * irdma_get_cqp_reg_info - get head and tail for cqp using registers
 * @cqp: struct for cqp hw
 * @val: cqp tail register value
 * @tail: wqtail register value
 * @error: cqp processing err
 */
static inline void
irdma_get_cqp_reg_info(struct irdma_sc_cqp *cqp, u32 *val,
		       u32 *tail, u32 *error)
{
	*val = readl(cqp->dev->hw_regs[IRDMA_CQPTAIL]);
	*tail = RS_32(*val, IRDMA_CQPTAIL_WQTAIL);
	*error = RS_32(*val, IRDMA_CQPTAIL_CQP_OP_ERR);
}

/**
 * irdma_cqp_poll_registers - poll cqp registers
 * @cqp: struct for cqp hw
 * @tail: wqtail register value
 * @count: how many times to try for completion
 */
static int
irdma_cqp_poll_registers(struct irdma_sc_cqp *cqp, u32 tail,
			 u32 count)
{
	u32 i = 0;
	u32 newtail, error, val;

	while (i++ < count) {
		irdma_get_cqp_reg_info(cqp, &val, &newtail, &error);
		if (error) {
			error = readl(cqp->dev->hw_regs[IRDMA_CQPERRCODES]);
			irdma_debug(cqp->dev, IRDMA_DEBUG_CQP,
				    "CQPERRCODES error_code[x%08X]\n", error);
			return -EIO;
		}
		if (newtail != tail) {
			/* SUCCESS */
			IRDMA_RING_MOVE_TAIL(cqp->sq_ring);
			cqp->dev->cqp_cmd_stats[IRDMA_OP_CMPL_CMDS]++;
			return 0;
		}
		irdma_usec_delay(cqp->dev->hw_attrs.max_sleep_count);
	}

	return -ETIMEDOUT;
}

/**
 * irdma_sc_decode_fpm_commit - decode a 64 bit value into count and base
 * @dev: sc device struct
 * @buf: pointer to commit buffer
 * @buf_idx: buffer index
 * @obj_info: object info pointer
 * @rsrc_idx: indexs of memory resource
 */
static u64 irdma_sc_decode_fpm_commit(struct irdma_sc_dev *dev, __le64 * buf,
				      u32 buf_idx, struct irdma_hmc_obj_info *obj_info,
				      u32 rsrc_idx){
	u64 temp;

	get_64bit_val(buf, buf_idx, &temp);

	switch (rsrc_idx) {
	case IRDMA_HMC_IW_QP:
		obj_info[rsrc_idx].cnt = (u32)RS_64(temp, IRDMA_COMMIT_FPM_QPCNT);
		break;
	case IRDMA_HMC_IW_CQ:
		obj_info[rsrc_idx].cnt = (u32)FLD_RS_64(dev, temp, IRDMA_COMMIT_FPM_CQCNT);
		break;
	case IRDMA_HMC_IW_APBVT_ENTRY:
		obj_info[rsrc_idx].cnt = 1;
		break;
	default:
		obj_info[rsrc_idx].cnt = (u32)temp;
		break;
	}

	obj_info[rsrc_idx].base = (u64)RS_64_1(temp, IRDMA_COMMIT_FPM_BASE_S) * 512;

	return temp;
}

/**
 * irdma_sc_parse_fpm_commit_buf - parse fpm commit buffer
 * @dev: pointer to dev struct
 * @buf: ptr to fpm commit buffer
 * @info: ptr to irdma_hmc_obj_info struct
 * @sd: number of SDs for HMC objects
 *
 * parses fpm commit info and copy base value
 * of hmc objects in hmc_info
 */
static int
irdma_sc_parse_fpm_commit_buf(struct irdma_sc_dev *dev, __le64 * buf,
			      struct irdma_hmc_obj_info *info,
			      u32 *sd)
{
	u64 size;
	u32 i;
	u64 max_base = 0;
	u32 last_hmc_obj = 0;

	irdma_sc_decode_fpm_commit(dev, buf, IRDMA_BYTE_0, info,
				   IRDMA_HMC_IW_QP);
	irdma_sc_decode_fpm_commit(dev, buf, IRDMA_BYTE_8, info,
				   IRDMA_HMC_IW_CQ);
	/* skiping RSRVD */
	irdma_sc_decode_fpm_commit(dev, buf, IRDMA_BYTE_24, info,
				   IRDMA_HMC_IW_HTE);
	irdma_sc_decode_fpm_commit(dev, buf, IRDMA_BYTE_32, info,
				   IRDMA_HMC_IW_ARP);
	irdma_sc_decode_fpm_commit(dev, buf, IRDMA_BYTE_40, info,
				   IRDMA_HMC_IW_APBVT_ENTRY);
	irdma_sc_decode_fpm_commit(dev, buf, IRDMA_BYTE_48, info,
				   IRDMA_HMC_IW_MR);
	irdma_sc_decode_fpm_commit(dev, buf, IRDMA_BYTE_56, info,
				   IRDMA_HMC_IW_XF);
	irdma_sc_decode_fpm_commit(dev, buf, IRDMA_BYTE_64, info,
				   IRDMA_HMC_IW_XFFL);
	irdma_sc_decode_fpm_commit(dev, buf, IRDMA_BYTE_72, info,
				   IRDMA_HMC_IW_Q1);
	irdma_sc_decode_fpm_commit(dev, buf, IRDMA_BYTE_80, info,
				   IRDMA_HMC_IW_Q1FL);
	irdma_sc_decode_fpm_commit(dev, buf, IRDMA_BYTE_88, info,
				   IRDMA_HMC_IW_TIMER);
	irdma_sc_decode_fpm_commit(dev, buf, IRDMA_BYTE_112, info,
				   IRDMA_HMC_IW_PBLE);
	/* skipping RSVD. */
	if (dev->hw_attrs.uk_attrs.hw_rev != IRDMA_GEN_1) {
		irdma_sc_decode_fpm_commit(dev, buf, IRDMA_BYTE_96, info,
					   IRDMA_HMC_IW_FSIMC);
		irdma_sc_decode_fpm_commit(dev, buf, IRDMA_BYTE_104, info,
					   IRDMA_HMC_IW_FSIAV);
		irdma_sc_decode_fpm_commit(dev, buf, IRDMA_BYTE_128, info,
					   IRDMA_HMC_IW_RRF);
		irdma_sc_decode_fpm_commit(dev, buf, IRDMA_BYTE_136, info,
					   IRDMA_HMC_IW_RRFFL);
		irdma_sc_decode_fpm_commit(dev, buf, IRDMA_BYTE_144, info,
					   IRDMA_HMC_IW_HDR);
		irdma_sc_decode_fpm_commit(dev, buf, IRDMA_BYTE_152, info,
					   IRDMA_HMC_IW_MD);
		irdma_sc_decode_fpm_commit(dev, buf, IRDMA_BYTE_160, info,
					   IRDMA_HMC_IW_OOISC);
		irdma_sc_decode_fpm_commit(dev, buf, IRDMA_BYTE_168, info,
					   IRDMA_HMC_IW_OOISCFFL);
	}

	/* searching for the last object in HMC to find the size of the HMC area. */
	for (i = IRDMA_HMC_IW_QP; i < IRDMA_HMC_IW_MAX; i++) {
		if (info[i].base > max_base) {
			max_base = info[i].base;
			last_hmc_obj = i;
		}
	}

	size = info[last_hmc_obj].cnt * info[last_hmc_obj].size +
	    info[last_hmc_obj].base;

	if (size & 0x1FFFFF)
		*sd = (u32)((size >> 21) + 1);	/* add 1 for remainder */
	else
		*sd = (u32)(size >> 21);

	return 0;
}

/**
 * irdma_sc_decode_fpm_query() - Decode a 64 bit value into max count and size
 * @buf: ptr to fpm query buffer
 * @buf_idx: index into buf
 * @obj_info: ptr to irdma_hmc_obj_info struct
 * @rsrc_idx: resource index into info
 *
 * Decode a 64 bit value from fpm query buffer into max count and size
 */
static u64 irdma_sc_decode_fpm_query(__le64 * buf, u32 buf_idx,
				     struct irdma_hmc_obj_info *obj_info,
				     u32 rsrc_idx){
	u64 temp;
	u32 size;

	get_64bit_val(buf, buf_idx, &temp);
	obj_info[rsrc_idx].max_cnt = (u32)temp;
	size = (u32)RS_64_1(temp, 32);
	obj_info[rsrc_idx].size = LS_64_1(1, size);

	return temp;
}

/**
 * irdma_sc_parse_fpm_query_buf() - parses fpm query buffer
 * @dev: ptr to shared code device
 * @buf: ptr to fpm query buffer
 * @hmc_info: ptr to irdma_hmc_obj_info struct
 * @hmc_fpm_misc: ptr to fpm data
 *
 * parses fpm query buffer and copy max_cnt and
 * size value of hmc objects in hmc_info
 */
static int
irdma_sc_parse_fpm_query_buf(struct irdma_sc_dev *dev, __le64 * buf,
			     struct irdma_hmc_info *hmc_info,
			     struct irdma_hmc_fpm_misc *hmc_fpm_misc)
{
	struct irdma_hmc_obj_info *obj_info;
	u64 temp;
	u32 size;
	u16 max_pe_sds;

	obj_info = hmc_info->hmc_obj;

	get_64bit_val(buf, IRDMA_BYTE_0, &temp);
	hmc_info->first_sd_index = (u16)RS_64(temp, IRDMA_QUERY_FPM_FIRST_PE_SD_INDEX);
	max_pe_sds = (u16)RS_64(temp, IRDMA_QUERY_FPM_MAX_PE_SDS);

	hmc_fpm_misc->max_sds = max_pe_sds;
	hmc_info->sd_table.sd_cnt = max_pe_sds + hmc_info->first_sd_index;
	get_64bit_val(buf, 8, &temp);
	obj_info[IRDMA_HMC_IW_QP].max_cnt = (u32)RS_64(temp, IRDMA_QUERY_FPM_MAX_QPS);
	size = (u32)RS_64_1(temp, 32);
	obj_info[IRDMA_HMC_IW_QP].size = LS_64_1(1, size);

	get_64bit_val(buf, 16, &temp);
	obj_info[IRDMA_HMC_IW_CQ].max_cnt = (u32)RS_64(temp, IRDMA_QUERY_FPM_MAX_CQS);
	size = (u32)RS_64_1(temp, 32);
	obj_info[IRDMA_HMC_IW_CQ].size = LS_64_1(1, size);

	irdma_sc_decode_fpm_query(buf, 32, obj_info, IRDMA_HMC_IW_HTE);
	irdma_sc_decode_fpm_query(buf, 40, obj_info, IRDMA_HMC_IW_ARP);

	obj_info[IRDMA_HMC_IW_APBVT_ENTRY].size = 8192;
	obj_info[IRDMA_HMC_IW_APBVT_ENTRY].max_cnt = 1;

	irdma_sc_decode_fpm_query(buf, 48, obj_info, IRDMA_HMC_IW_MR);
	irdma_sc_decode_fpm_query(buf, 56, obj_info, IRDMA_HMC_IW_XF);

	get_64bit_val(buf, 64, &temp);
	obj_info[IRDMA_HMC_IW_XFFL].max_cnt = (u32)temp;
	obj_info[IRDMA_HMC_IW_XFFL].size = 4;
	hmc_fpm_misc->xf_block_size = RS_64(temp, IRDMA_QUERY_FPM_XFBLOCKSIZE);
	if (!hmc_fpm_misc->xf_block_size)
		return -EINVAL;

	irdma_sc_decode_fpm_query(buf, 72, obj_info, IRDMA_HMC_IW_Q1);
	get_64bit_val(buf, 80, &temp);
	obj_info[IRDMA_HMC_IW_Q1FL].max_cnt = (u32)temp;
	obj_info[IRDMA_HMC_IW_Q1FL].size = 4;

	hmc_fpm_misc->q1_block_size = RS_64(temp, IRDMA_QUERY_FPM_Q1BLOCKSIZE);
	if (!hmc_fpm_misc->q1_block_size)
		return -EINVAL;

	irdma_sc_decode_fpm_query(buf, 88, obj_info, IRDMA_HMC_IW_TIMER);

	get_64bit_val(buf, 112, &temp);
	obj_info[IRDMA_HMC_IW_PBLE].max_cnt = (u32)temp;
	obj_info[IRDMA_HMC_IW_PBLE].size = 8;

	get_64bit_val(buf, 120, &temp);
	hmc_fpm_misc->max_ceqs = RS_64(temp, IRDMA_QUERY_FPM_MAX_CEQS);
	hmc_fpm_misc->ht_multiplier = RS_64(temp, IRDMA_QUERY_FPM_HTMULTIPLIER);
	hmc_fpm_misc->timer_bucket = RS_64(temp, IRDMA_QUERY_FPM_TIMERBUCKET);
	if (dev->hw_attrs.uk_attrs.hw_rev == IRDMA_GEN_1)
		return 0;
	irdma_sc_decode_fpm_query(buf, 96, obj_info, IRDMA_HMC_IW_FSIMC);
	irdma_sc_decode_fpm_query(buf, 104, obj_info, IRDMA_HMC_IW_FSIAV);
	irdma_sc_decode_fpm_query(buf, 128, obj_info, IRDMA_HMC_IW_RRF);

	get_64bit_val(buf, IRDMA_BYTE_136, &temp);
	obj_info[IRDMA_HMC_IW_RRFFL].max_cnt = (u32)temp;
	obj_info[IRDMA_HMC_IW_RRFFL].size = 4;
	hmc_fpm_misc->rrf_block_size = RS_64(temp, IRDMA_QUERY_FPM_RRFBLOCKSIZE);
	if (!hmc_fpm_misc->rrf_block_size &&
	    obj_info[IRDMA_HMC_IW_RRFFL].max_cnt)
		return -EINVAL;

	irdma_sc_decode_fpm_query(buf, 144, obj_info, IRDMA_HMC_IW_HDR);
	irdma_sc_decode_fpm_query(buf, 152, obj_info, IRDMA_HMC_IW_MD);
	irdma_sc_decode_fpm_query(buf, 160, obj_info, IRDMA_HMC_IW_OOISC);

	get_64bit_val(buf, IRDMA_BYTE_168, &temp);
	obj_info[IRDMA_HMC_IW_OOISCFFL].max_cnt = (u32)temp;
	obj_info[IRDMA_HMC_IW_OOISCFFL].size = 4;
	hmc_fpm_misc->ooiscf_block_size = RS_64(temp, IRDMA_QUERY_FPM_OOISCFBLOCKSIZE);
	if (!hmc_fpm_misc->ooiscf_block_size &&
	    obj_info[IRDMA_HMC_IW_OOISCFFL].max_cnt)
		return -EINVAL;

	return 0;
}

/**
 * irdma_sc_find_reg_cq - find cq ctx index
 * @ceq: ceq sc structure
 * @cq: cq sc structure
 */
static u32 irdma_sc_find_reg_cq(struct irdma_sc_ceq *ceq,
				struct irdma_sc_cq *cq){
	u32 i;

	for (i = 0; i < ceq->reg_cq_size; i++) {
		if (cq == ceq->reg_cq[i])
			return i;
	}

	return IRDMA_INVALID_CQ_IDX;
}

/**
 * irdma_sc_add_cq_ctx - add cq ctx tracking for ceq
 * @ceq: ceq sc structure
 * @cq: cq sc structure
 */
int
irdma_sc_add_cq_ctx(struct irdma_sc_ceq *ceq, struct irdma_sc_cq *cq)
{
	unsigned long flags;

	spin_lock_irqsave(&ceq->req_cq_lock, flags);

	if (ceq->reg_cq_size == ceq->elem_cnt) {
		spin_unlock_irqrestore(&ceq->req_cq_lock, flags);
		return -ENOSPC;
	}

	ceq->reg_cq[ceq->reg_cq_size++] = cq;

	spin_unlock_irqrestore(&ceq->req_cq_lock, flags);

	return 0;
}

/**
 * irdma_sc_remove_cq_ctx - remove cq ctx tracking for ceq
 * @ceq: ceq sc structure
 * @cq: cq sc structure
 */
void
irdma_sc_remove_cq_ctx(struct irdma_sc_ceq *ceq, struct irdma_sc_cq *cq)
{
	unsigned long flags;
	u32 cq_ctx_idx;

	spin_lock_irqsave(&ceq->req_cq_lock, flags);
	cq_ctx_idx = irdma_sc_find_reg_cq(ceq, cq);
	if (cq_ctx_idx == IRDMA_INVALID_CQ_IDX)
		goto exit;

	ceq->reg_cq_size--;
	if (cq_ctx_idx != ceq->reg_cq_size)
		ceq->reg_cq[cq_ctx_idx] = ceq->reg_cq[ceq->reg_cq_size];
	ceq->reg_cq[ceq->reg_cq_size] = NULL;

exit:
	spin_unlock_irqrestore(&ceq->req_cq_lock, flags);
}

/**
 * irdma_sc_cqp_init - Initialize buffers for a control Queue Pair
 * @cqp: IWARP control queue pair pointer
 * @info: IWARP control queue pair init info pointer
 *
 * Initializes the object and context buffers for a control Queue Pair.
 */
int
irdma_sc_cqp_init(struct irdma_sc_cqp *cqp,
		  struct irdma_cqp_init_info *info)
{
	u8 hw_sq_size;

	if (info->sq_size > IRDMA_CQP_SW_SQSIZE_2048 ||
	    info->sq_size < IRDMA_CQP_SW_SQSIZE_4 ||
	    ((info->sq_size & (info->sq_size - 1))))
		return -EINVAL;

	hw_sq_size = irdma_get_encoded_wqe_size(info->sq_size,
						IRDMA_QUEUE_TYPE_CQP);
	cqp->size = sizeof(*cqp);
	cqp->sq_size = info->sq_size;
	cqp->hw_sq_size = hw_sq_size;
	cqp->sq_base = info->sq;
	cqp->host_ctx = info->host_ctx;
	cqp->sq_pa = info->sq_pa;
	cqp->host_ctx_pa = info->host_ctx_pa;
	cqp->dev = info->dev;
	cqp->struct_ver = info->struct_ver;
	cqp->hw_maj_ver = info->hw_maj_ver;
	cqp->hw_min_ver = info->hw_min_ver;
	cqp->scratch_array = info->scratch_array;
	cqp->polarity = 0;
	cqp->en_datacenter_tcp = info->en_datacenter_tcp;
	cqp->ena_vf_count = info->ena_vf_count;
	cqp->hmc_profile = info->hmc_profile;
	cqp->ceqs_per_vf = info->ceqs_per_vf;
	cqp->disable_packed = info->disable_packed;
	cqp->rocev2_rto_policy = info->rocev2_rto_policy;
	cqp->protocol_used = info->protocol_used;
	irdma_memcpy(&cqp->dcqcn_params, &info->dcqcn_params, sizeof(cqp->dcqcn_params));
	cqp->en_rem_endpoint_trk = info->en_rem_endpoint_trk;
	info->dev->cqp = cqp;

	IRDMA_RING_INIT(cqp->sq_ring, cqp->sq_size);
	cqp->dev->cqp_cmd_stats[IRDMA_OP_REQ_CMDS] = 0;
	cqp->dev->cqp_cmd_stats[IRDMA_OP_CMPL_CMDS] = 0;
	/* for the cqp commands backlog. */
	INIT_LIST_HEAD(&cqp->dev->cqp_cmd_head);

	writel(0, cqp->dev->hw_regs[IRDMA_CQPTAIL]);
	writel(0, cqp->dev->hw_regs[IRDMA_CQPDB]);
	writel(0, cqp->dev->hw_regs[IRDMA_CCQPSTATUS]);

	irdma_debug(cqp->dev, IRDMA_DEBUG_WQE,
		    "sq_size[%04d] hw_sq_size[%04d] sq_base[%p] sq_pa[%llxh] cqp[%p] polarity[x%04x]\n",
		    cqp->sq_size, cqp->hw_sq_size, cqp->sq_base, (unsigned long long)cqp->sq_pa, cqp,
		    cqp->polarity);
	return 0;
}

/**
 * irdma_sc_cqp_create - create cqp during bringup
 * @cqp: struct for cqp hw
 * @maj_err: If error, major err number
 * @min_err: If error, minor err number
 */
int
irdma_sc_cqp_create(struct irdma_sc_cqp *cqp, u16 *maj_err, u16 *min_err)
{
	u64 temp;
	u8 hw_rev;
	u32 cnt = 0, p1, p2, val = 0, err_code;
	int ret_code;

	hw_rev = cqp->dev->hw_attrs.uk_attrs.hw_rev;
	cqp->sdbuf.size = IRDMA_UPDATE_SD_BUFF_SIZE * cqp->sq_size;
	cqp->sdbuf.va = irdma_allocate_dma_mem(cqp->dev->hw, &cqp->sdbuf,
					       cqp->sdbuf.size,
					       IRDMA_SD_BUF_ALIGNMENT);
	if (!cqp->sdbuf.va)
		return -ENOMEM;

	spin_lock_init(&cqp->dev->cqp_lock);

	temp = LS_64(cqp->hw_sq_size, IRDMA_CQPHC_SQSIZE) |
	    LS_64(cqp->struct_ver, IRDMA_CQPHC_SVER) |
	    LS_64(cqp->disable_packed, IRDMA_CQPHC_DISABLE_PFPDUS) |
	    LS_64(cqp->ceqs_per_vf, IRDMA_CQPHC_CEQPERVF);
	if (hw_rev >= IRDMA_GEN_2) {
		temp |= LS_64(cqp->rocev2_rto_policy, IRDMA_CQPHC_ROCEV2_RTO_POLICY) |
		    LS_64(cqp->protocol_used, IRDMA_CQPHC_PROTOCOL_USED);
	}

	set_64bit_val(cqp->host_ctx, IRDMA_BYTE_0, temp);
	set_64bit_val(cqp->host_ctx, IRDMA_BYTE_8, cqp->sq_pa);

	temp = LS_64(cqp->ena_vf_count, IRDMA_CQPHC_ENABLED_VFS) |
	    LS_64(cqp->hmc_profile, IRDMA_CQPHC_HMC_PROFILE);
	if (hw_rev >= IRDMA_GEN_2)
		temp |= LS_64(cqp->en_rem_endpoint_trk, IRDMA_CQPHC_EN_REM_ENDPOINT_TRK);
	set_64bit_val(cqp->host_ctx, IRDMA_BYTE_16, temp);
	set_64bit_val(cqp->host_ctx, IRDMA_BYTE_24, (uintptr_t)cqp);
	temp = LS_64(cqp->hw_maj_ver, IRDMA_CQPHC_HW_MAJVER) |
	    LS_64(cqp->hw_min_ver, IRDMA_CQPHC_HW_MINVER);
	if (hw_rev >= IRDMA_GEN_2) {
		temp |= LS_64(cqp->dcqcn_params.min_rate, IRDMA_CQPHC_MIN_RATE) |
		    LS_64(cqp->dcqcn_params.min_dec_factor, IRDMA_CQPHC_MIN_DEC_FACTOR);
	}
	set_64bit_val(cqp->host_ctx, IRDMA_BYTE_32, temp);
	set_64bit_val(cqp->host_ctx, IRDMA_BYTE_40, 0);
	temp = 0;
	if (hw_rev >= IRDMA_GEN_2) {
		temp |= LS_64(cqp->dcqcn_params.dcqcn_t, IRDMA_CQPHC_DCQCN_T) |
		    LS_64(cqp->dcqcn_params.rai_factor, IRDMA_CQPHC_RAI_FACTOR) |
		    LS_64(cqp->dcqcn_params.hai_factor, IRDMA_CQPHC_HAI_FACTOR);
	}
	set_64bit_val(cqp->host_ctx, IRDMA_BYTE_48, temp);
	temp = 0;
	if (hw_rev >= IRDMA_GEN_2) {
		temp |= LS_64(cqp->dcqcn_params.dcqcn_b, IRDMA_CQPHC_DCQCN_B) |
		    LS_64(cqp->dcqcn_params.dcqcn_f, IRDMA_CQPHC_DCQCN_F) |
		    LS_64(cqp->dcqcn_params.cc_cfg_valid, IRDMA_CQPHC_CC_CFG_VALID) |
		    LS_64(cqp->dcqcn_params.rreduce_mperiod, IRDMA_CQPHC_RREDUCE_MPERIOD);
	}
	set_64bit_val(cqp->host_ctx, IRDMA_BYTE_56, temp);
	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "CQP_HOST_CTX WQE",
			cqp->host_ctx, IRDMA_CQP_CTX_SIZE * 8);
	p1 = RS_32_1(cqp->host_ctx_pa, 32);
	p2 = (u32)cqp->host_ctx_pa;

	writel(p1, cqp->dev->hw_regs[IRDMA_CCQPHIGH]);
	writel(p2, cqp->dev->hw_regs[IRDMA_CCQPLOW]);

	do {
		if (cnt++ > cqp->dev->hw_attrs.max_done_count) {
			ret_code = -ETIMEDOUT;
			goto err;
		}
		irdma_usec_delay(cqp->dev->hw_attrs.max_sleep_count);
		val = readl(cqp->dev->hw_regs[IRDMA_CCQPSTATUS]);
	} while (!val);

	if (FLD_RS_32(cqp->dev, val, IRDMA_CCQPSTATUS_CCQP_ERR)) {
		ret_code = -EOPNOTSUPP;
		goto err;
	}

	cqp->process_cqp_sds = irdma_update_sds_noccq;
	return 0;

err:
	spin_lock_destroy(&cqp->dev->cqp_lock);
	irdma_free_dma_mem(cqp->dev->hw, &cqp->sdbuf);
	err_code = readl(cqp->dev->hw_regs[IRDMA_CQPERRCODES]);
	*min_err = RS_32(err_code, IRDMA_CQPERRCODES_CQP_MINOR_CODE);
	*maj_err = RS_32(err_code, IRDMA_CQPERRCODES_CQP_MAJOR_CODE);
	return ret_code;
}

/**
 * irdma_sc_cqp_post_sq - post of cqp's sq
 * @cqp: struct for cqp hw
 */
void
irdma_sc_cqp_post_sq(struct irdma_sc_cqp *cqp)
{
	db_wr32(IRDMA_RING_CURRENT_HEAD(cqp->sq_ring), cqp->dev->cqp_db);

	irdma_debug(cqp->dev, IRDMA_DEBUG_WQE,
		    "CQP SQ head 0x%x tail 0x%x size 0x%x\n", cqp->sq_ring.head,
		    cqp->sq_ring.tail, cqp->sq_ring.size);
}

/**
 * irdma_sc_cqp_get_next_send_wqe_idx - get next wqe on cqp sq
 * and pass back index
 * @cqp: CQP HW structure
 * @scratch: private data for CQP WQE
 * @wqe_idx: WQE index of CQP SQ
 */
__le64 *
irdma_sc_cqp_get_next_send_wqe_idx(struct irdma_sc_cqp *cqp, u64 scratch,
				   u32 *wqe_idx)
{
	__le64 *wqe = NULL;
	int ret_code;

	if (IRDMA_RING_FULL_ERR(cqp->sq_ring)) {
		irdma_debug(cqp->dev, IRDMA_DEBUG_WQE,
			    "CQP SQ is full, head 0x%x tail 0x%x size 0x%x\n",
			    cqp->sq_ring.head, cqp->sq_ring.tail,
			    cqp->sq_ring.size);
		return NULL;
	}
	IRDMA_ATOMIC_RING_MOVE_HEAD(cqp->sq_ring, *wqe_idx, ret_code);
	if (ret_code)
		return NULL;

	cqp->dev->cqp_cmd_stats[IRDMA_OP_REQ_CMDS]++;
	if (!*wqe_idx)
		cqp->polarity = !cqp->polarity;
	wqe = cqp->sq_base[*wqe_idx].elem;
	cqp->scratch_array[*wqe_idx] = scratch;

	memset(&wqe[0], 0, 24);
	memset(&wqe[4], 0, 32);

	return wqe;
}

/**
 * irdma_sc_cqp_destroy - destroy cqp during close
 * @cqp: struct for cqp hw
 */
int
irdma_sc_cqp_destroy(struct irdma_sc_cqp *cqp)
{
	u32 cnt = 0, val;
	int ret_code = 0;

	writel(0, cqp->dev->hw_regs[IRDMA_CCQPHIGH]);
	writel(0, cqp->dev->hw_regs[IRDMA_CCQPLOW]);
	do {
		if (cnt++ > cqp->dev->hw_attrs.max_done_count) {
			ret_code = -ETIMEDOUT;
			break;
		}
		irdma_usec_delay(cqp->dev->hw_attrs.max_sleep_count);
		val = readl(cqp->dev->hw_regs[IRDMA_CCQPSTATUS]);
	} while (FLD_RS_32(cqp->dev, val, IRDMA_CCQPSTATUS_CCQP_DONE));

	irdma_free_dma_mem(cqp->dev->hw, &cqp->sdbuf);
	spin_lock_destroy(&cqp->dev->cqp_lock);
	return ret_code;
}

/**
 * irdma_sc_ccq_arm - enable intr for control cq
 * @ccq: ccq sc struct
 */
void
irdma_sc_ccq_arm(struct irdma_sc_cq *ccq)
{
	u64 temp_val;
	u16 sw_cq_sel;
	u8 arm_next_se;
	u8 arm_seq_num;

	get_64bit_val(ccq->cq_uk.shadow_area, IRDMA_BYTE_32, &temp_val);
	sw_cq_sel = (u16)RS_64(temp_val, IRDMA_CQ_DBSA_SW_CQ_SELECT);
	arm_next_se = (u8)RS_64(temp_val, IRDMA_CQ_DBSA_ARM_NEXT_SE);
	arm_seq_num = (u8)RS_64(temp_val, IRDMA_CQ_DBSA_ARM_SEQ_NUM);
	arm_seq_num++;
	temp_val = LS_64(arm_seq_num, IRDMA_CQ_DBSA_ARM_SEQ_NUM) |
	    LS_64(sw_cq_sel, IRDMA_CQ_DBSA_SW_CQ_SELECT) |
	    LS_64(arm_next_se, IRDMA_CQ_DBSA_ARM_NEXT_SE) |
	    LS_64(1, IRDMA_CQ_DBSA_ARM_NEXT);
	set_64bit_val(ccq->cq_uk.shadow_area, IRDMA_BYTE_32, temp_val);

	irdma_wmb();		/* make sure shadow area is updated before arming */

	db_wr32(ccq->cq_uk.cq_id, ccq->dev->cq_arm_db);
}

/**
 * irdma_sc_ccq_get_cqe_info - get ccq's cq entry
 * @ccq: ccq sc struct
 * @info: completion q entry to return
 */
int
irdma_sc_ccq_get_cqe_info(struct irdma_sc_cq *ccq,
			  struct irdma_ccq_cqe_info *info)
{
	u64 qp_ctx, temp, temp1;
	__le64 *cqe;
	struct irdma_sc_cqp *cqp;
	u32 wqe_idx;
	u32 error;
	u8 polarity;
	int ret_code = 0;

	if (ccq->cq_uk.avoid_mem_cflct)
		cqe = IRDMA_GET_CURRENT_EXTENDED_CQ_ELEM(&ccq->cq_uk);
	else
		cqe = IRDMA_GET_CURRENT_CQ_ELEM(&ccq->cq_uk);

	get_64bit_val(cqe, IRDMA_BYTE_24, &temp);
	polarity = (u8)RS_64(temp, IRDMA_CQ_VALID);
	if (polarity != ccq->cq_uk.polarity)
		return -ENOENT;

	get_64bit_val(cqe, IRDMA_BYTE_8, &qp_ctx);
	cqp = (struct irdma_sc_cqp *)(irdma_uintptr) qp_ctx;
	info->error = (bool)RS_64(temp, IRDMA_CQ_ERROR);
	info->maj_err_code = IRDMA_CQPSQ_MAJ_NO_ERROR;
	info->min_err_code = (u16)RS_64(temp, IRDMA_CQ_MINERR);
	if (info->error) {
		info->maj_err_code = (u16)RS_64(temp, IRDMA_CQ_MAJERR);
		error = readl(cqp->dev->hw_regs[IRDMA_CQPERRCODES]);
		irdma_debug(cqp->dev, IRDMA_DEBUG_CQP,
			    "CQPERRCODES error_code[x%08X]\n", error);
	}

	wqe_idx = (u32)RS_64(temp, IRDMA_CQ_WQEIDX);
	info->scratch = cqp->scratch_array[wqe_idx];

	get_64bit_val(cqe, IRDMA_BYTE_16, &temp1);
	info->op_ret_val = (u32)RS_64(temp1, IRDMA_CCQ_OPRETVAL);
	get_64bit_val(cqp->sq_base[wqe_idx].elem, IRDMA_BYTE_24, &temp1);
	info->op_code = (u8)RS_64(temp1, IRDMA_CQPSQ_OPCODE);
	info->cqp = cqp;

	/* move the head for cq */
	IRDMA_RING_MOVE_HEAD(ccq->cq_uk.cq_ring, ret_code);
	if (!IRDMA_RING_CURRENT_HEAD(ccq->cq_uk.cq_ring))
		ccq->cq_uk.polarity ^= 1;

	/* update cq tail in cq shadow memory also */
	IRDMA_RING_MOVE_TAIL(ccq->cq_uk.cq_ring);
	set_64bit_val(ccq->cq_uk.shadow_area, IRDMA_BYTE_0,
		      IRDMA_RING_CURRENT_HEAD(ccq->cq_uk.cq_ring));

	irdma_wmb();		/* make sure shadow area is updated before moving tail */

	IRDMA_RING_MOVE_TAIL(cqp->sq_ring);
	ccq->dev->cqp_cmd_stats[IRDMA_OP_CMPL_CMDS]++;

	return ret_code;
}

/**
 * irdma_sc_poll_for_cqp_op_done - Waits for last write to complete in CQP SQ
 * @cqp: struct for cqp hw
 * @op_code: cqp opcode for completion
 * @compl_info: completion q entry to return
 */
int
irdma_sc_poll_for_cqp_op_done(struct irdma_sc_cqp *cqp, u8 op_code,
			      struct irdma_ccq_cqe_info *compl_info)
{
	struct irdma_ccq_cqe_info info = {0};
	struct irdma_sc_cq *ccq;
	int ret_code = 0;
	u32 cnt = 0;

	ccq = cqp->dev->ccq;
	while (1) {
		if (cnt++ > 100 * cqp->dev->hw_attrs.max_done_count)
			return -ETIMEDOUT;

		if (cqp->dev->no_cqp)
			return -ETIMEDOUT;

		if (irdma_sc_ccq_get_cqe_info(ccq, &info)) {
			irdma_usec_delay(cqp->dev->hw_attrs.max_sleep_count);
			continue;
		}
		if (info.error && info.op_code != IRDMA_CQP_OP_QUERY_STAG) {
			ret_code = -EIO;
			break;
		}
		/* make sure op code matches */
		if (op_code == info.op_code)
			break;
		irdma_debug(cqp->dev, IRDMA_DEBUG_WQE,
			    "opcode mismatch for my op code 0x%x, returned opcode %x\n",
			    op_code, info.op_code);
	}

	if (compl_info)
		irdma_memcpy(compl_info, &info, sizeof(*compl_info));

	return ret_code;
}

/**
 * irdma_sc_manage_hmc_pm_func_table - manage of function table
 * @cqp: struct for cqp hw
 * @scratch: u64 saved to be used during cqp completion
 * @info: info for the manage function table operation
 * @post_sq: flag for cqp db to ring
 */
static int
irdma_sc_manage_hmc_pm_func_table(struct irdma_sc_cqp *cqp,
				  struct irdma_hmc_fcn_info *info,
				  u64 scratch, bool post_sq)
{
	__le64 *wqe;
	u64 hdr;

	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;

	hdr = LS_64(info->vf_id, IRDMA_CQPSQ_MHMC_VFIDX) |
	    LS_64(IRDMA_CQP_OP_MANAGE_HMC_PM_FUNC_TABLE,
		  IRDMA_CQPSQ_OPCODE) |
	    LS_64(info->free_fcn, IRDMA_CQPSQ_MHMC_FREEPMFN) |
	    LS_64(cqp->polarity, IRDMA_CQPSQ_WQEVALID);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE,
			"MANAGE_HMC_PM_FUNC_TABLE WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);
	if (post_sq)
		irdma_sc_cqp_post_sq(cqp);

	return 0;
}

/**
 * irdma_sc_commit_fpm_val_done - wait for cqp eqe completion
 * for fpm commit
 * @cqp: struct for cqp hw
 */
static int
irdma_sc_commit_fpm_val_done(struct irdma_sc_cqp *cqp)
{
	return irdma_sc_poll_for_cqp_op_done(cqp, IRDMA_CQP_OP_COMMIT_FPM_VAL,
					     NULL);
}

/**
 * irdma_sc_commit_fpm_val - cqp wqe for commit fpm values
 * @cqp: struct for cqp hw
 * @scratch: u64 saved to be used during cqp completion
 * @hmc_fn_id: hmc function id
 * @commit_fpm_mem: Memory for fpm values
 * @post_sq: flag for cqp db to ring
 * @wait_type: poll ccq or cqp registers for cqp completion
 */
static int
irdma_sc_commit_fpm_val(struct irdma_sc_cqp *cqp, u64 scratch,
			u8 hmc_fn_id,
			struct irdma_dma_mem *commit_fpm_mem,
			bool post_sq, u8 wait_type)
{
	__le64 *wqe;
	u64 hdr;
	u32 tail, val, error;
	int ret_code = 0;

	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;

	set_64bit_val(wqe, IRDMA_BYTE_16, hmc_fn_id);
	set_64bit_val(wqe, IRDMA_BYTE_32, commit_fpm_mem->pa);

	hdr = LS_64(IRDMA_COMMIT_FPM_BUF_SIZE, IRDMA_CQPSQ_BUFSIZE) |
	    LS_64(IRDMA_CQP_OP_COMMIT_FPM_VAL, IRDMA_CQPSQ_OPCODE) |
	    LS_64(cqp->polarity, IRDMA_CQPSQ_WQEVALID);

	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "COMMIT_FPM_VAL WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);
	irdma_get_cqp_reg_info(cqp, &val, &tail, &error);

	if (post_sq) {
		irdma_sc_cqp_post_sq(cqp);
		if (wait_type == IRDMA_CQP_WAIT_POLL_REGS)
			ret_code = irdma_cqp_poll_registers(cqp, tail,
							    cqp->dev->hw_attrs.max_done_count);
		else if (wait_type == IRDMA_CQP_WAIT_POLL_CQ)
			ret_code = irdma_sc_commit_fpm_val_done(cqp);
	}

	return ret_code;
}

/**
 * irdma_sc_query_fpm_val_done - poll for cqp wqe completion for
 * query fpm
 * @cqp: struct for cqp hw
 */
static int
irdma_sc_query_fpm_val_done(struct irdma_sc_cqp *cqp)
{
	return irdma_sc_poll_for_cqp_op_done(cqp, IRDMA_CQP_OP_QUERY_FPM_VAL,
					     NULL);
}

/**
 * irdma_sc_query_fpm_val - cqp wqe query fpm values
 * @cqp: struct for cqp hw
 * @scratch: u64 saved to be used during cqp completion
 * @hmc_fn_id: hmc function id
 * @query_fpm_mem: memory for return fpm values
 * @post_sq: flag for cqp db to ring
 * @wait_type: poll ccq or cqp registers for cqp completion
 */
static int
irdma_sc_query_fpm_val(struct irdma_sc_cqp *cqp, u64 scratch,
		       u8 hmc_fn_id,
		       struct irdma_dma_mem *query_fpm_mem,
		       bool post_sq, u8 wait_type)
{
	__le64 *wqe;
	u64 hdr;
	u32 tail, val, error;
	int ret_code = 0;

	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;

	set_64bit_val(wqe, IRDMA_BYTE_16, hmc_fn_id);
	set_64bit_val(wqe, IRDMA_BYTE_32, query_fpm_mem->pa);

	hdr = LS_64(IRDMA_CQP_OP_QUERY_FPM_VAL, IRDMA_CQPSQ_OPCODE) |
	    LS_64(cqp->polarity, IRDMA_CQPSQ_WQEVALID);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "QUERY_FPM WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);
	irdma_get_cqp_reg_info(cqp, &val, &tail, &error);

	if (post_sq) {
		irdma_sc_cqp_post_sq(cqp);
		if (wait_type == IRDMA_CQP_WAIT_POLL_REGS)
			ret_code = irdma_cqp_poll_registers(cqp, tail,
							    cqp->dev->hw_attrs.max_done_count);
		else if (wait_type == IRDMA_CQP_WAIT_POLL_CQ)
			ret_code = irdma_sc_query_fpm_val_done(cqp);
	}

	return ret_code;
}

/**
 * irdma_sc_ceq_init - initialize ceq
 * @ceq: ceq sc structure
 * @info: ceq initialization info
 */
int
irdma_sc_ceq_init(struct irdma_sc_ceq *ceq,
		  struct irdma_ceq_init_info *info)
{
	u32 pble_obj_cnt;

	if (info->elem_cnt < info->dev->hw_attrs.min_hw_ceq_size ||
	    info->elem_cnt > info->dev->hw_attrs.max_hw_ceq_size)
		return -EINVAL;

	if (info->ceq_id > (info->dev->hmc_fpm_misc.max_ceqs - 1))
		return -EINVAL;
	pble_obj_cnt = info->dev->hmc_info->hmc_obj[IRDMA_HMC_IW_PBLE].cnt;

	if (info->virtual_map && info->first_pm_pbl_idx >= pble_obj_cnt)
		return -EINVAL;

	ceq->size = sizeof(*ceq);
	ceq->ceqe_base = (struct irdma_ceqe *)info->ceqe_base;
	ceq->ceq_id = info->ceq_id;
	ceq->dev = info->dev;
	ceq->elem_cnt = info->elem_cnt;
	ceq->ceq_elem_pa = info->ceqe_pa;
	ceq->virtual_map = info->virtual_map;
	ceq->itr_no_expire = info->itr_no_expire;
	ceq->reg_cq = info->reg_cq;
	ceq->reg_cq_size = 0;
	spin_lock_init(&ceq->req_cq_lock);
	ceq->pbl_chunk_size = (ceq->virtual_map ? info->pbl_chunk_size : 0);
	ceq->first_pm_pbl_idx = (ceq->virtual_map ? info->first_pm_pbl_idx : 0);
	ceq->pbl_list = (ceq->virtual_map ? info->pbl_list : NULL);
	ceq->tph_en = info->tph_en;
	ceq->tph_val = info->tph_val;
	ceq->vsi = info->vsi;
	ceq->polarity = 1;
	IRDMA_RING_INIT(ceq->ceq_ring, ceq->elem_cnt);
	ceq->dev->ceq[info->ceq_id] = ceq;

	return 0;
}

/**
 * irdma_sc_ceq_create - create ceq wqe
 * @ceq: ceq sc structure
 * @scratch: u64 saved to be used during cqp completion
 * @post_sq: flag for cqp db to ring
 */

static int
irdma_sc_ceq_create(struct irdma_sc_ceq *ceq, u64 scratch,
		    bool post_sq)
{
	struct irdma_sc_cqp *cqp;
	__le64 *wqe;
	u64 hdr;

	cqp = ceq->dev->cqp;
	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;
	set_64bit_val(wqe, IRDMA_BYTE_16, ceq->elem_cnt);
	set_64bit_val(wqe, IRDMA_BYTE_32,
		      (ceq->virtual_map ? 0 : ceq->ceq_elem_pa));
	set_64bit_val(wqe, IRDMA_BYTE_48,
		      (ceq->virtual_map ? ceq->first_pm_pbl_idx : 0));
	set_64bit_val(wqe, IRDMA_BYTE_56,
		      LS_64(ceq->tph_val, IRDMA_CQPSQ_TPHVAL) |
		      LS_64(ceq->vsi->vsi_idx, IRDMA_CQPSQ_VSIIDX));
	hdr = LS_64(ceq->ceq_id, IRDMA_CQPSQ_CEQ_CEQID) |
	    LS_64(IRDMA_CQP_OP_CREATE_CEQ, IRDMA_CQPSQ_OPCODE) |
	    LS_64(ceq->pbl_chunk_size, IRDMA_CQPSQ_CEQ_LPBLSIZE) |
	    LS_64(ceq->virtual_map, IRDMA_CQPSQ_CEQ_VMAP) |
	    LS_64(ceq->itr_no_expire, IRDMA_CQPSQ_CEQ_ITRNOEXPIRE) |
	    LS_64(ceq->tph_en, IRDMA_CQPSQ_TPHEN) |
	    LS_64(cqp->polarity, IRDMA_CQPSQ_WQEVALID);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "CEQ_CREATE WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);
	if (post_sq)
		irdma_sc_cqp_post_sq(cqp);

	return 0;
}

/**
 * irdma_sc_cceq_create_done - poll for control ceq wqe to complete
 * @ceq: ceq sc structure
 */
static int
irdma_sc_cceq_create_done(struct irdma_sc_ceq *ceq)
{
	struct irdma_sc_cqp *cqp;

	cqp = ceq->dev->cqp;
	return irdma_sc_poll_for_cqp_op_done(cqp, IRDMA_CQP_OP_CREATE_CEQ,
					     NULL);
}

/**
 * irdma_sc_cceq_destroy_done - poll for destroy cceq to complete
 * @ceq: ceq sc structure
 */
int
irdma_sc_cceq_destroy_done(struct irdma_sc_ceq *ceq)
{
	struct irdma_sc_cqp *cqp;

	if (ceq->reg_cq)
		irdma_sc_remove_cq_ctx(ceq, ceq->dev->ccq);

	cqp = ceq->dev->cqp;
	cqp->process_cqp_sds = irdma_update_sds_noccq;

	return irdma_sc_poll_for_cqp_op_done(cqp, IRDMA_CQP_OP_DESTROY_CEQ,
					     NULL);
}

/**
 * irdma_sc_cceq_create - create cceq
 * @ceq: ceq sc structure
 * @scratch: u64 saved to be used during cqp completion
 */
int
irdma_sc_cceq_create(struct irdma_sc_ceq *ceq, u64 scratch)
{
	int ret_code;
	struct irdma_sc_dev *dev = ceq->dev;

	dev->ccq->vsi = ceq->vsi;
	if (ceq->reg_cq) {
		ret_code = irdma_sc_add_cq_ctx(ceq, ceq->dev->ccq);
		if (ret_code)
			return ret_code;
	}

	ret_code = irdma_sc_ceq_create(ceq, scratch, true);
	if (!ret_code)
		return irdma_sc_cceq_create_done(ceq);

	return ret_code;
}

/**
 * irdma_sc_ceq_destroy - destroy ceq
 * @ceq: ceq sc structure
 * @scratch: u64 saved to be used during cqp completion
 * @post_sq: flag for cqp db to ring
 */
int
irdma_sc_ceq_destroy(struct irdma_sc_ceq *ceq, u64 scratch, bool post_sq)
{
	struct irdma_sc_cqp *cqp;
	__le64 *wqe;
	u64 hdr;

	cqp = ceq->dev->cqp;
	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;

	set_64bit_val(wqe, IRDMA_BYTE_16, ceq->elem_cnt);
	set_64bit_val(wqe, IRDMA_BYTE_48, ceq->first_pm_pbl_idx);
	hdr = ceq->ceq_id |
	    LS_64(IRDMA_CQP_OP_DESTROY_CEQ, IRDMA_CQPSQ_OPCODE) |
	    LS_64(ceq->pbl_chunk_size, IRDMA_CQPSQ_CEQ_LPBLSIZE) |
	    LS_64(ceq->virtual_map, IRDMA_CQPSQ_CEQ_VMAP) |
	    LS_64(ceq->tph_en, IRDMA_CQPSQ_TPHEN) |
	    LS_64(cqp->polarity, IRDMA_CQPSQ_WQEVALID);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "CEQ_DESTROY WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);
	ceq->dev->ceq[ceq->ceq_id] = NULL;
	if (post_sq)
		irdma_sc_cqp_post_sq(cqp);

	return 0;
}

/**
 * irdma_sc_process_ceq - process ceq
 * @dev: sc device struct
 * @ceq: ceq sc structure
 *
 * It is expected caller serializes this function with cleanup_ceqes()
 * because these functions manipulate the same ceq
 */
void *
irdma_sc_process_ceq(struct irdma_sc_dev *dev, struct irdma_sc_ceq *ceq)
{
	u64 temp;
	__le64 *ceqe;
	struct irdma_sc_cq *cq = NULL;
	struct irdma_sc_cq *temp_cq;
	u8 polarity;
	u32 cq_idx;
	unsigned long flags;

	do {
		cq_idx = 0;
		ceqe = IRDMA_GET_CURRENT_CEQ_ELEM(ceq);
		get_64bit_val(ceqe, IRDMA_BYTE_0, &temp);
		polarity = (u8)RS_64(temp, IRDMA_CEQE_VALID);
		if (polarity != ceq->polarity)
			return NULL;

		temp_cq = (struct irdma_sc_cq *)(irdma_uintptr) LS_64_1(temp, 1);
		if (!temp_cq) {
			cq_idx = IRDMA_INVALID_CQ_IDX;
			IRDMA_RING_MOVE_TAIL(ceq->ceq_ring);

			if (!IRDMA_RING_CURRENT_TAIL(ceq->ceq_ring))
				ceq->polarity ^= 1;
			continue;
		}

		cq = temp_cq;
		if (ceq->reg_cq) {
			spin_lock_irqsave(&ceq->req_cq_lock, flags);
			cq_idx = irdma_sc_find_reg_cq(ceq, cq);
			spin_unlock_irqrestore(&ceq->req_cq_lock, flags);
		}

		IRDMA_RING_MOVE_TAIL(ceq->ceq_ring);
		if (!IRDMA_RING_CURRENT_TAIL(ceq->ceq_ring))
			ceq->polarity ^= 1;
	} while (cq_idx == IRDMA_INVALID_CQ_IDX);

	if (cq) {
		cq->cq_uk.armed = false;
		irdma_sc_cq_ack(cq);
	}
	return cq;
}

/**
 * irdma_sc_cleanup_ceqes - clear the valid ceqes ctx matching the cq
 * @cq: cq for which the ceqes need to be cleaned up
 * @ceq: ceq ptr
 *
 * The function is called after the cq is destroyed to cleanup
 * its pending ceqe entries. It is expected caller serializes this
 * function with process_ceq() in interrupt context.
 */
void
irdma_sc_cleanup_ceqes(struct irdma_sc_cq *cq, struct irdma_sc_ceq *ceq)
{
	struct irdma_sc_cq *next_cq;
	u8 ceq_polarity = ceq->polarity;
	__le64 *ceqe;
	u8 polarity;
	u64 temp;
	int next;
	u32 i;

	next = IRDMA_RING_GET_NEXT_TAIL(ceq->ceq_ring, 0);

	for (i = 1; i <= IRDMA_RING_SIZE(*ceq); i++) {
		ceqe = IRDMA_GET_CEQ_ELEM_AT_POS(ceq, next);

		get_64bit_val(ceqe, IRDMA_BYTE_0, &temp);
		polarity = (u8)RS_64(temp, IRDMA_CEQE_VALID);
		if (polarity != ceq_polarity)
			return;

		next_cq = (struct irdma_sc_cq *)(irdma_uintptr) LS_64_1(temp, 1);
		if (cq == next_cq)
			set_64bit_val(ceqe, IRDMA_BYTE_0, temp & IRDMA_CEQE_VALID_M);

		next = IRDMA_RING_GET_NEXT_TAIL(ceq->ceq_ring, i);
		if (!next)
			ceq_polarity ^= 1;
	}
}

/**
 * irdma_sc_aeq_init - initialize aeq
 * @aeq: aeq structure ptr
 * @info: aeq initialization info
 */
int
irdma_sc_aeq_init(struct irdma_sc_aeq *aeq,
		  struct irdma_aeq_init_info *info)
{
	u32 pble_obj_cnt;

	if (info->elem_cnt < info->dev->hw_attrs.min_hw_aeq_size ||
	    info->elem_cnt > info->dev->hw_attrs.max_hw_aeq_size)
		return -EINVAL;

	pble_obj_cnt = info->dev->hmc_info->hmc_obj[IRDMA_HMC_IW_PBLE].cnt;

	if (info->virtual_map && info->first_pm_pbl_idx >= pble_obj_cnt)
		return -EINVAL;

	aeq->size = sizeof(*aeq);
	aeq->polarity = 1;
	aeq->aeqe_base = (struct irdma_sc_aeqe *)info->aeqe_base;
	aeq->dev = info->dev;
	aeq->elem_cnt = info->elem_cnt;
	aeq->aeq_elem_pa = info->aeq_elem_pa;
	IRDMA_RING_INIT(aeq->aeq_ring, aeq->elem_cnt);
	aeq->virtual_map = info->virtual_map;
	aeq->pbl_list = (aeq->virtual_map ? info->pbl_list : NULL);
	aeq->pbl_chunk_size = (aeq->virtual_map ? info->pbl_chunk_size : 0);
	aeq->first_pm_pbl_idx = (aeq->virtual_map ? info->first_pm_pbl_idx : 0);
	aeq->msix_idx = info->msix_idx;
	info->dev->aeq = aeq;

	return 0;
}

/**
 * irdma_sc_aeq_create - create aeq
 * @aeq: aeq structure ptr
 * @scratch: u64 saved to be used during cqp completion
 * @post_sq: flag for cqp db to ring
 */
static int
irdma_sc_aeq_create(struct irdma_sc_aeq *aeq, u64 scratch,
		    bool post_sq)
{
	__le64 *wqe;
	struct irdma_sc_cqp *cqp;
	u64 hdr;

	cqp = aeq->dev->cqp;
	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;
	set_64bit_val(wqe, IRDMA_BYTE_16, aeq->elem_cnt);
	set_64bit_val(wqe, IRDMA_BYTE_32,
		      (aeq->virtual_map ? 0 : aeq->aeq_elem_pa));
	set_64bit_val(wqe, IRDMA_BYTE_48,
		      (aeq->virtual_map ? aeq->first_pm_pbl_idx : 0));

	hdr = LS_64(IRDMA_CQP_OP_CREATE_AEQ, IRDMA_CQPSQ_OPCODE) |
	    LS_64(aeq->pbl_chunk_size, IRDMA_CQPSQ_AEQ_LPBLSIZE) |
	    LS_64(aeq->virtual_map, IRDMA_CQPSQ_AEQ_VMAP) |
	    LS_64(cqp->polarity, IRDMA_CQPSQ_WQEVALID);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "AEQ_CREATE WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);
	if (post_sq)
		irdma_sc_cqp_post_sq(cqp);

	return 0;
}

/**
 * irdma_sc_aeq_destroy - destroy aeq during close
 * @aeq: aeq structure ptr
 * @scratch: u64 saved to be used during cqp completion
 * @post_sq: flag for cqp db to ring
 */
static int
irdma_sc_aeq_destroy(struct irdma_sc_aeq *aeq, u64 scratch,
		     bool post_sq)
{
	__le64 *wqe;
	struct irdma_sc_cqp *cqp;
	struct irdma_sc_dev *dev;
	u64 hdr;

	dev = aeq->dev;
	writel(0, dev->hw_regs[IRDMA_PFINT_AEQCTL]);

	cqp = dev->cqp;
	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;
	set_64bit_val(wqe, IRDMA_BYTE_16, aeq->elem_cnt);
	set_64bit_val(wqe, IRDMA_BYTE_48, aeq->first_pm_pbl_idx);
	hdr = LS_64(IRDMA_CQP_OP_DESTROY_AEQ, IRDMA_CQPSQ_OPCODE) |
	    LS_64(aeq->pbl_chunk_size, IRDMA_CQPSQ_AEQ_LPBLSIZE) |
	    LS_64(aeq->virtual_map, IRDMA_CQPSQ_AEQ_VMAP) |
	    LS_64(cqp->polarity, IRDMA_CQPSQ_WQEVALID);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	irdma_debug_buf(dev, IRDMA_DEBUG_WQE, "AEQ_DESTROY WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);
	if (post_sq)
		irdma_sc_cqp_post_sq(cqp);
	return 0;
}

/**
 * irdma_sc_get_next_aeqe - get next aeq entry
 * @aeq: aeq structure ptr
 * @info: aeqe info to be returned
 */
int
irdma_sc_get_next_aeqe(struct irdma_sc_aeq *aeq,
		       struct irdma_aeqe_info *info)
{
	u64 temp, compl_ctx;
	__le64 *aeqe;
	u16 wqe_idx;
	u8 ae_src;
	u8 polarity;

	aeqe = IRDMA_GET_CURRENT_AEQ_ELEM(aeq);
	get_64bit_val(aeqe, IRDMA_BYTE_0, &compl_ctx);
	get_64bit_val(aeqe, IRDMA_BYTE_8, &temp);
	polarity = (u8)RS_64(temp, IRDMA_AEQE_VALID);

	if (aeq->polarity != polarity)
		return -ENOENT;

	irdma_debug_buf(aeq->dev, IRDMA_DEBUG_WQE, "AEQ_ENTRY WQE", aeqe, 16);

	ae_src = (u8)RS_64(temp, IRDMA_AEQE_AESRC);
	wqe_idx = (u16)RS_64(temp, IRDMA_AEQE_WQDESCIDX);
	info->qp_cq_id = (u32)RS_64(temp, IRDMA_AEQE_QPCQID_LOW) |
	    ((u32)RS_64(temp, IRDMA_AEQE_QPCQID_HI) << 18);
	info->ae_id = (u16)RS_64(temp, IRDMA_AEQE_AECODE);
	info->tcp_state = (u8)RS_64(temp, IRDMA_AEQE_TCPSTATE);
	info->iwarp_state = (u8)RS_64(temp, IRDMA_AEQE_IWSTATE);
	info->q2_data_written = (u8)RS_64(temp, IRDMA_AEQE_Q2DATA);
	info->aeqe_overflow = (bool)RS_64(temp, IRDMA_AEQE_OVERFLOW);

	info->ae_src = ae_src;
	switch (info->ae_id) {
	case IRDMA_AE_PRIV_OPERATION_DENIED:
	case IRDMA_AE_AMP_INVALIDATE_TYPE1_MW:
	case IRDMA_AE_AMP_MWBIND_ZERO_BASED_TYPE1_MW:
	case IRDMA_AE_AMP_FASTREG_INVALID_PBL_HPS_CFG:
	case IRDMA_AE_AMP_FASTREG_PBLE_MISMATCH:
	case IRDMA_AE_UDA_XMIT_DGRAM_TOO_LONG:
	case IRDMA_AE_UDA_XMIT_BAD_PD:
	case IRDMA_AE_UDA_XMIT_DGRAM_TOO_SHORT:
	case IRDMA_AE_BAD_CLOSE:
	case IRDMA_AE_RDMA_READ_WHILE_ORD_ZERO:
	case IRDMA_AE_STAG_ZERO_INVALID:
	case IRDMA_AE_IB_RREQ_AND_Q1_FULL:
	case IRDMA_AE_IB_INVALID_REQUEST:
	case IRDMA_AE_WQE_UNEXPECTED_OPCODE:
	case IRDMA_AE_IB_REMOTE_ACCESS_ERROR:
	case IRDMA_AE_IB_REMOTE_OP_ERROR:
	case IRDMA_AE_DDP_UBE_INVALID_DDP_VERSION:
	case IRDMA_AE_DDP_UBE_INVALID_MO:
	case IRDMA_AE_DDP_UBE_INVALID_QN:
	case IRDMA_AE_DDP_NO_L_BIT:
	case IRDMA_AE_RDMAP_ROE_INVALID_RDMAP_VERSION:
	case IRDMA_AE_RDMAP_ROE_UNEXPECTED_OPCODE:
	case IRDMA_AE_ROE_INVALID_RDMA_READ_REQUEST:
	case IRDMA_AE_ROE_INVALID_RDMA_WRITE_OR_READ_RESP:
	case IRDMA_AE_ROCE_RSP_LENGTH_ERROR:
	case IRDMA_AE_INVALID_ARP_ENTRY:
	case IRDMA_AE_INVALID_TCP_OPTION_RCVD:
	case IRDMA_AE_STALE_ARP_ENTRY:
	case IRDMA_AE_INVALID_AH_ENTRY:
	case IRDMA_AE_LLP_RECEIVED_MPA_CRC_ERROR:
	case IRDMA_AE_LLP_SEGMENT_TOO_SMALL:
	case IRDMA_AE_LLP_TOO_MANY_RETRIES:
	case IRDMA_AE_LLP_DOUBT_REACHABILITY:
	case IRDMA_AE_LLP_CONNECTION_ESTABLISHED:
	case IRDMA_AE_RESET_SENT:
	case IRDMA_AE_TERMINATE_SENT:
	case IRDMA_AE_RESET_NOT_SENT:
	case IRDMA_AE_LCE_QP_CATASTROPHIC:
	case IRDMA_AE_QP_SUSPEND_COMPLETE:
	case IRDMA_AE_UDA_L4LEN_INVALID:
		info->qp = true;
		info->compl_ctx = compl_ctx;
		break;
	case IRDMA_AE_LCE_CQ_CATASTROPHIC:
		info->cq = true;
		info->compl_ctx = LS_64_1(compl_ctx, 1);
		ae_src = IRDMA_AE_SOURCE_RSVD;
		break;
	case IRDMA_AE_ROCE_EMPTY_MCG:
	case IRDMA_AE_ROCE_BAD_MC_IP_ADDR:
	case IRDMA_AE_ROCE_BAD_MC_QPID:
	case IRDMA_AE_MCG_QP_PROTOCOL_MISMATCH:
		/* fallthrough */
	case IRDMA_AE_LLP_CONNECTION_RESET:
	case IRDMA_AE_LLP_SYN_RECEIVED:
	case IRDMA_AE_LLP_FIN_RECEIVED:
	case IRDMA_AE_LLP_CLOSE_COMPLETE:
	case IRDMA_AE_LLP_TERMINATE_RECEIVED:
	case IRDMA_AE_RDMAP_ROE_BAD_LLP_CLOSE:
		ae_src = IRDMA_AE_SOURCE_RSVD;
		info->qp = true;
		info->compl_ctx = compl_ctx;
		break;
	default:
		break;
	}

	switch (ae_src) {
	case IRDMA_AE_SOURCE_RQ:
	case IRDMA_AE_SOURCE_RQ_0011:
		info->qp = true;
		info->rq = true;
		info->wqe_idx = wqe_idx;
		info->compl_ctx = compl_ctx;
		break;
	case IRDMA_AE_SOURCE_CQ:
	case IRDMA_AE_SOURCE_CQ_0110:
	case IRDMA_AE_SOURCE_CQ_1010:
	case IRDMA_AE_SOURCE_CQ_1110:
		info->cq = true;
		info->compl_ctx = LS_64_1(compl_ctx, 1);
		break;
	case IRDMA_AE_SOURCE_SQ:
	case IRDMA_AE_SOURCE_SQ_0111:
		info->qp = true;
		info->sq = true;
		info->wqe_idx = wqe_idx;
		info->compl_ctx = compl_ctx;
		break;
	case IRDMA_AE_SOURCE_IN_WR:
	case IRDMA_AE_SOURCE_IN_RR:
		info->qp = true;
		info->compl_ctx = compl_ctx;
		info->in_rdrsp_wr = true;
		break;
	case IRDMA_AE_SOURCE_OUT_RR:
	case IRDMA_AE_SOURCE_OUT_RR_1111:
		info->qp = true;
		info->compl_ctx = compl_ctx;
		info->out_rdrsp = true;
		break;
	case IRDMA_AE_SOURCE_RSVD:
	default:
		break;
	}

	IRDMA_RING_MOVE_TAIL(aeq->aeq_ring);
	if (!IRDMA_RING_CURRENT_TAIL(aeq->aeq_ring))
		aeq->polarity ^= 1;

	return 0;
}

/**
 * irdma_sc_repost_aeq_entries - repost completed aeq entries
 * @dev: sc device struct
 * @count: allocate count
 */
int
irdma_sc_repost_aeq_entries(struct irdma_sc_dev *dev, u32 count)
{
	writel(count, dev->hw_regs[IRDMA_AEQALLOC]);

	return 0;
}

/**
 * irdma_sc_ccq_init - initialize control cq
 * @cq: sc's cq ctruct
 * @info: info for control cq initialization
 */
int
irdma_sc_ccq_init(struct irdma_sc_cq *cq, struct irdma_ccq_init_info *info)
{
	u32 pble_obj_cnt;

	if (info->num_elem < info->dev->hw_attrs.uk_attrs.min_hw_cq_size ||
	    info->num_elem > info->dev->hw_attrs.uk_attrs.max_hw_cq_size)
		return -EINVAL;

	if (info->ceq_id > (info->dev->hmc_fpm_misc.max_ceqs - 1))
		return -EINVAL;

	pble_obj_cnt = info->dev->hmc_info->hmc_obj[IRDMA_HMC_IW_PBLE].cnt;

	if (info->virtual_map && info->first_pm_pbl_idx >= pble_obj_cnt)
		return -EINVAL;

	cq->cq_pa = info->cq_pa;
	cq->cq_uk.cq_base = info->cq_base;
	cq->shadow_area_pa = info->shadow_area_pa;
	cq->cq_uk.shadow_area = info->shadow_area;
	cq->shadow_read_threshold = info->shadow_read_threshold;
	cq->dev = info->dev;
	cq->ceq_id = info->ceq_id;
	cq->cq_uk.cq_size = info->num_elem;
	cq->cq_type = IRDMA_CQ_TYPE_CQP;
	cq->ceqe_mask = info->ceqe_mask;
	IRDMA_RING_INIT(cq->cq_uk.cq_ring, info->num_elem);
	cq->cq_uk.cq_id = 0;	/* control cq is id 0 always */
	cq->ceq_id_valid = info->ceq_id_valid;
	cq->tph_en = info->tph_en;
	cq->tph_val = info->tph_val;
	cq->cq_uk.avoid_mem_cflct = info->avoid_mem_cflct;
	cq->pbl_list = info->pbl_list;
	cq->virtual_map = info->virtual_map;
	cq->pbl_chunk_size = info->pbl_chunk_size;
	cq->first_pm_pbl_idx = info->first_pm_pbl_idx;
	cq->cq_uk.polarity = true;
	cq->vsi = info->vsi;
	cq->cq_uk.cq_ack_db = cq->dev->cq_ack_db;

	/* Only applicable to CQs other than CCQ so initialize to zero */
	cq->cq_uk.cqe_alloc_db = NULL;

	info->dev->ccq = cq;
	return 0;
}

/**
 * irdma_sc_ccq_create_done - poll cqp for ccq create
 * @ccq: ccq sc struct
 */
static inline int
irdma_sc_ccq_create_done(struct irdma_sc_cq *ccq)
{
	struct irdma_sc_cqp *cqp;

	cqp = ccq->dev->cqp;

	return irdma_sc_poll_for_cqp_op_done(cqp, IRDMA_CQP_OP_CREATE_CQ, NULL);
}

/**
 * irdma_sc_ccq_create - create control cq
 * @ccq: ccq sc struct
 * @scratch: u64 saved to be used during cqp completion
 * @check_overflow: overlow flag for ccq
 * @post_sq: flag for cqp db to ring
 */
int
irdma_sc_ccq_create(struct irdma_sc_cq *ccq, u64 scratch,
		    bool check_overflow, bool post_sq)
{
	int ret_code;

	ret_code = irdma_sc_cq_create(ccq, scratch, check_overflow, post_sq);
	if (ret_code)
		return ret_code;

	if (post_sq) {
		ret_code = irdma_sc_ccq_create_done(ccq);
		if (ret_code)
			return ret_code;
	}
	ccq->dev->cqp->process_cqp_sds = irdma_cqp_sds_cmd;

	return 0;
}

/**
 * irdma_sc_ccq_destroy - destroy ccq during close
 * @ccq: ccq sc struct
 * @scratch: u64 saved to be used during cqp completion
 * @post_sq: flag for cqp db to ring
 */
int
irdma_sc_ccq_destroy(struct irdma_sc_cq *ccq, u64 scratch, bool post_sq)
{
	struct irdma_sc_cqp *cqp;
	__le64 *wqe;
	u64 hdr;
	int ret_code = 0;
	u32 tail, val, error;

	cqp = ccq->dev->cqp;
	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;

	set_64bit_val(wqe, IRDMA_BYTE_0, ccq->cq_uk.cq_size);
	set_64bit_val(wqe, IRDMA_BYTE_8, RS_64_1(ccq, 1));
	set_64bit_val(wqe, IRDMA_BYTE_40, ccq->shadow_area_pa);

	hdr = ccq->cq_uk.cq_id |
	    FLD_LS_64(ccq->dev, (ccq->ceq_id_valid ? ccq->ceq_id : 0),
		      IRDMA_CQPSQ_CQ_CEQID) |
	    LS_64(IRDMA_CQP_OP_DESTROY_CQ, IRDMA_CQPSQ_OPCODE) |
	    LS_64(ccq->ceqe_mask, IRDMA_CQPSQ_CQ_ENCEQEMASK) |
	    LS_64(ccq->ceq_id_valid, IRDMA_CQPSQ_CQ_CEQIDVALID) |
	    LS_64(ccq->tph_en, IRDMA_CQPSQ_TPHEN) |
	    LS_64(ccq->cq_uk.avoid_mem_cflct, IRDMA_CQPSQ_CQ_AVOIDMEMCNFLCT) |
	    LS_64(cqp->polarity, IRDMA_CQPSQ_WQEVALID);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "CCQ_DESTROY WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);
	irdma_get_cqp_reg_info(cqp, &val, &tail, &error);

	if (post_sq) {
		irdma_sc_cqp_post_sq(cqp);
		ret_code = irdma_cqp_poll_registers(cqp, tail,
						    cqp->dev->hw_attrs.max_done_count);
	}

	cqp->process_cqp_sds = irdma_update_sds_noccq;

	return ret_code;
}

/**
 * irdma_sc_init_iw_hmc() - queries fpm values using cqp and populates hmc_info
 * @dev : ptr to irdma_dev struct
 * @hmc_fn_id: hmc function id
 */
int
irdma_sc_init_iw_hmc(struct irdma_sc_dev *dev, u8 hmc_fn_id)
{
	struct irdma_hmc_info *hmc_info;
	struct irdma_hmc_fpm_misc *hmc_fpm_misc;
	struct irdma_dma_mem query_fpm_mem;
	int ret_code = 0;
	u8 wait_type;

	hmc_info = dev->hmc_info;
	hmc_fpm_misc = &dev->hmc_fpm_misc;
	query_fpm_mem.pa = dev->fpm_query_buf_pa;
	query_fpm_mem.va = dev->fpm_query_buf;
	hmc_info->hmc_fn_id = hmc_fn_id;
	wait_type = (u8)IRDMA_CQP_WAIT_POLL_REGS;

	ret_code = irdma_sc_query_fpm_val(dev->cqp, 0, hmc_info->hmc_fn_id,
					  &query_fpm_mem, true, wait_type);
	if (ret_code)
		return ret_code;

	/* parse the fpm_query_buf and fill hmc obj info */
	ret_code = irdma_sc_parse_fpm_query_buf(dev, query_fpm_mem.va, hmc_info,
						hmc_fpm_misc);

	irdma_debug_buf(dev, IRDMA_DEBUG_HMC, "QUERY FPM BUFFER",
			query_fpm_mem.va, IRDMA_QUERY_FPM_BUF_SIZE);
	return ret_code;
}

/**
 * irdma_sc_cfg_iw_fpm() - commits hmc obj cnt values using cqp
 * command and populates fpm base address in hmc_info
 * @dev : ptr to irdma_dev struct
 * @hmc_fn_id: hmc function id
 */
static int
irdma_sc_cfg_iw_fpm(struct irdma_sc_dev *dev, u8 hmc_fn_id)
{
	struct irdma_hmc_obj_info *obj_info;
	__le64 *buf;
	struct irdma_hmc_info *hmc_info;
	struct irdma_dma_mem commit_fpm_mem;
	int ret_code = 0;
	u8 wait_type;

	hmc_info = dev->hmc_info;
	obj_info = hmc_info->hmc_obj;
	buf = dev->fpm_commit_buf;

	set_64bit_val(buf, IRDMA_BYTE_0, (u64)obj_info[IRDMA_HMC_IW_QP].cnt);
	set_64bit_val(buf, IRDMA_BYTE_8, (u64)obj_info[IRDMA_HMC_IW_CQ].cnt);
	set_64bit_val(buf, IRDMA_BYTE_16, (u64)0);	/* RSRVD */
	set_64bit_val(buf, IRDMA_BYTE_24, (u64)obj_info[IRDMA_HMC_IW_HTE].cnt);
	set_64bit_val(buf, IRDMA_BYTE_32, (u64)obj_info[IRDMA_HMC_IW_ARP].cnt);
	set_64bit_val(buf, IRDMA_BYTE_40, (u64)0);	/* RSVD */
	set_64bit_val(buf, IRDMA_BYTE_48, (u64)obj_info[IRDMA_HMC_IW_MR].cnt);
	set_64bit_val(buf, IRDMA_BYTE_56, (u64)obj_info[IRDMA_HMC_IW_XF].cnt);
	set_64bit_val(buf, IRDMA_BYTE_64, (u64)obj_info[IRDMA_HMC_IW_XFFL].cnt);
	set_64bit_val(buf, IRDMA_BYTE_72, (u64)obj_info[IRDMA_HMC_IW_Q1].cnt);
	set_64bit_val(buf, IRDMA_BYTE_80, (u64)obj_info[IRDMA_HMC_IW_Q1FL].cnt);
	set_64bit_val(buf, IRDMA_BYTE_88,
		      (u64)obj_info[IRDMA_HMC_IW_TIMER].cnt);
	set_64bit_val(buf, IRDMA_BYTE_96,
		      (u64)obj_info[IRDMA_HMC_IW_FSIMC].cnt);
	set_64bit_val(buf, IRDMA_BYTE_104,
		      (u64)obj_info[IRDMA_HMC_IW_FSIAV].cnt);
	set_64bit_val(buf, IRDMA_BYTE_112,
		      (u64)obj_info[IRDMA_HMC_IW_PBLE].cnt);
	set_64bit_val(buf, IRDMA_BYTE_120, (u64)0);	/* RSVD */
	set_64bit_val(buf, IRDMA_BYTE_128, (u64)obj_info[IRDMA_HMC_IW_RRF].cnt);
	set_64bit_val(buf, IRDMA_BYTE_136,
		      (u64)obj_info[IRDMA_HMC_IW_RRFFL].cnt);
	set_64bit_val(buf, IRDMA_BYTE_144, (u64)obj_info[IRDMA_HMC_IW_HDR].cnt);
	set_64bit_val(buf, IRDMA_BYTE_152, (u64)obj_info[IRDMA_HMC_IW_MD].cnt);
	set_64bit_val(buf, IRDMA_BYTE_160,
		      (u64)obj_info[IRDMA_HMC_IW_OOISC].cnt);
	set_64bit_val(buf, IRDMA_BYTE_168,
		      (u64)obj_info[IRDMA_HMC_IW_OOISCFFL].cnt);
	commit_fpm_mem.pa = dev->fpm_commit_buf_pa;
	commit_fpm_mem.va = dev->fpm_commit_buf;

	wait_type = (u8)IRDMA_CQP_WAIT_POLL_REGS;
	irdma_debug_buf(dev, IRDMA_DEBUG_HMC, "COMMIT FPM BUFFER",
			commit_fpm_mem.va, IRDMA_COMMIT_FPM_BUF_SIZE);
	ret_code = irdma_sc_commit_fpm_val(dev->cqp, 0, hmc_info->hmc_fn_id,
					   &commit_fpm_mem, true, wait_type);
	if (!ret_code)
		ret_code = irdma_sc_parse_fpm_commit_buf(dev, dev->fpm_commit_buf,
							 hmc_info->hmc_obj,
							 &hmc_info->sd_table.sd_cnt);
	irdma_debug_buf(dev, IRDMA_DEBUG_HMC, "COMMIT FPM BUFFER",
			commit_fpm_mem.va, IRDMA_COMMIT_FPM_BUF_SIZE);

	return ret_code;
}

/**
 * cqp_sds_wqe_fill - fill cqp wqe doe sd
 * @cqp: struct for cqp hw
 * @info: sd info for wqe
 * @scratch: u64 saved to be used during cqp completion
 */
static int
cqp_sds_wqe_fill(struct irdma_sc_cqp *cqp,
		 struct irdma_update_sds_info *info, u64 scratch)
{
	u64 data;
	u64 hdr;
	__le64 *wqe;
	int mem_entries, wqe_entries;
	struct irdma_dma_mem *sdbuf = &cqp->sdbuf;
	u64 offset = 0;
	u32 wqe_idx;

	wqe = irdma_sc_cqp_get_next_send_wqe_idx(cqp, scratch, &wqe_idx);
	if (!wqe)
		return -ENOSPC;

	wqe_entries = (info->cnt > 3) ? 3 : info->cnt;
	mem_entries = info->cnt - wqe_entries;

	if (mem_entries) {
		offset = wqe_idx * IRDMA_UPDATE_SD_BUFF_SIZE;
		irdma_memcpy(((char *)sdbuf->va + offset), &info->entry[3], mem_entries << 4);

		data = (u64)sdbuf->pa + offset;
	} else {
		data = 0;
	}
	data |= LS_64(info->hmc_fn_id, IRDMA_CQPSQ_UPESD_HMCFNID);
	set_64bit_val(wqe, IRDMA_BYTE_16, data);

	switch (wqe_entries) {
	case 3:
		set_64bit_val(wqe, IRDMA_BYTE_48,
			      (LS_64(info->entry[2].cmd, IRDMA_CQPSQ_UPESD_SDCMD) |
			       LS_64(1, IRDMA_CQPSQ_UPESD_ENTRY_VALID)));

		set_64bit_val(wqe, IRDMA_BYTE_56, info->entry[2].data);
		/* fallthrough */
	case 2:
		set_64bit_val(wqe, IRDMA_BYTE_32,
			      (LS_64(info->entry[1].cmd, IRDMA_CQPSQ_UPESD_SDCMD) |
			       LS_64(1, IRDMA_CQPSQ_UPESD_ENTRY_VALID)));

		set_64bit_val(wqe, IRDMA_BYTE_40, info->entry[1].data);
		/* fallthrough */
	case 1:
		set_64bit_val(wqe, IRDMA_BYTE_0,
			      LS_64(info->entry[0].cmd, IRDMA_CQPSQ_UPESD_SDCMD));

		set_64bit_val(wqe, IRDMA_BYTE_8, info->entry[0].data);
		break;
	default:
		break;
	}

	hdr = LS_64(IRDMA_CQP_OP_UPDATE_PE_SDS, IRDMA_CQPSQ_OPCODE) |
	    LS_64(cqp->polarity, IRDMA_CQPSQ_WQEVALID) |
	    LS_64(mem_entries, IRDMA_CQPSQ_UPESD_ENTRY_COUNT);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	if (mem_entries)
		irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "UPDATE_PE_SDS WQE Buffer",
				(char *)sdbuf->va + offset, mem_entries << 4);

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "UPDATE_PE_SDS WQE", wqe,
			IRDMA_CQP_WQE_SIZE * 8);

	return 0;
}

/**
 * irdma_update_pe_sds - cqp wqe for sd
 * @dev: ptr to irdma_dev struct
 * @info: sd info for sd's
 * @scratch: u64 saved to be used during cqp completion
 */
static int
irdma_update_pe_sds(struct irdma_sc_dev *dev,
		    struct irdma_update_sds_info *info, u64 scratch)
{
	struct irdma_sc_cqp *cqp = dev->cqp;
	int ret_code;

	ret_code = cqp_sds_wqe_fill(cqp, info, scratch);
	if (!ret_code)
		irdma_sc_cqp_post_sq(cqp);

	return ret_code;
}

/**
 * irdma_update_sds_noccq - update sd before ccq created
 * @dev: sc device struct
 * @info: sd info for sd's
 */
int
irdma_update_sds_noccq(struct irdma_sc_dev *dev,
		       struct irdma_update_sds_info *info)
{
	u32 error, val, tail;
	struct irdma_sc_cqp *cqp = dev->cqp;
	int ret_code;

	ret_code = cqp_sds_wqe_fill(cqp, info, 0);
	if (ret_code)
		return ret_code;

	irdma_get_cqp_reg_info(cqp, &val, &tail, &error);

	irdma_sc_cqp_post_sq(cqp);
	return irdma_cqp_poll_registers(cqp, tail,
					cqp->dev->hw_attrs.max_done_count);
}

/**
 * irdma_sc_static_hmc_pages_allocated - cqp wqe to allocate hmc pages
 * @cqp: struct for cqp hw
 * @scratch: u64 saved to be used during cqp completion
 * @hmc_fn_id: hmc function id
 * @post_sq: flag for cqp db to ring
 * @poll_registers: flag to poll register for cqp completion
 */
int
irdma_sc_static_hmc_pages_allocated(struct irdma_sc_cqp *cqp, u64 scratch,
				    u8 hmc_fn_id, bool post_sq,
				    bool poll_registers)
{
	u64 hdr;
	__le64 *wqe;
	u32 tail, val, error;

	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;

	set_64bit_val(wqe, IRDMA_BYTE_16,
		      LS_64(hmc_fn_id, IRDMA_SHMC_PAGE_ALLOCATED_HMC_FN_ID));

	hdr = LS_64(IRDMA_CQP_OP_SHMC_PAGES_ALLOCATED, IRDMA_CQPSQ_OPCODE) |
	    LS_64(cqp->polarity, IRDMA_CQPSQ_WQEVALID);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, hdr);

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "SHMC_PAGES_ALLOCATED WQE",
			wqe, IRDMA_CQP_WQE_SIZE * 8);
	irdma_get_cqp_reg_info(cqp, &val, &tail, &error);

	if (post_sq) {
		irdma_sc_cqp_post_sq(cqp);
		if (poll_registers)
			/* check for cqp sq tail update */
			return irdma_cqp_poll_registers(cqp, tail,
							cqp->dev->hw_attrs.max_done_count);
		else
			return irdma_sc_poll_for_cqp_op_done(cqp,
							     IRDMA_CQP_OP_SHMC_PAGES_ALLOCATED,
							     NULL);
	}

	return 0;
}

/**
 * irdma_cqp_ring_full - check if cqp ring is full
 * @cqp: struct for cqp hw
 */
static bool
irdma_cqp_ring_full(struct irdma_sc_cqp *cqp)
{
	return IRDMA_RING_FULL_ERR(cqp->sq_ring);
}

/**
 * irdma_est_sd - returns approximate number of SDs for HMC
 * @dev: sc device struct
 * @hmc_info: hmc structure, size and count for HMC objects
 */
static u32 irdma_est_sd(struct irdma_sc_dev *dev,
			struct irdma_hmc_info *hmc_info){
	int i;
	u64 size = 0;
	u64 sd;

	for (i = IRDMA_HMC_IW_QP; i < IRDMA_HMC_IW_MAX; i++)
		if (i != IRDMA_HMC_IW_PBLE)
			size += round_up(hmc_info->hmc_obj[i].cnt *
					 hmc_info->hmc_obj[i].size, 512);
	size += round_up(hmc_info->hmc_obj[IRDMA_HMC_IW_PBLE].cnt *
			 hmc_info->hmc_obj[IRDMA_HMC_IW_PBLE].size, 512);
	if (size & 0x1FFFFF)
		sd = (size >> 21) + 1;	/* add 1 for remainder */
	else
		sd = size >> 21;
	if (sd > 0xFFFFFFFF) {
		irdma_debug(dev, IRDMA_DEBUG_HMC, "sd overflow[%ld]\n", sd);
		sd = 0xFFFFFFFF - 1;
	}

	return (u32)sd;
}

/**
 * irdma_sc_query_rdma_features - query RDMA features and FW ver
 * @cqp: struct for cqp hw
 * @buf: buffer to hold query info
 * @scratch: u64 saved to be used during cqp completion
 */
static int
irdma_sc_query_rdma_features(struct irdma_sc_cqp *cqp,
			     struct irdma_dma_mem *buf, u64 scratch)
{
	__le64 *wqe;
	u64 temp;
	u32 tail, val, error;
	int status;

	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, scratch);
	if (!wqe)
		return -ENOSPC;

	temp = buf->pa;
	set_64bit_val(wqe, IRDMA_BYTE_32, temp);

	temp = LS_64(cqp->polarity, IRDMA_CQPSQ_QUERY_RDMA_FEATURES_WQEVALID) |
	    LS_64(buf->size, IRDMA_CQPSQ_QUERY_RDMA_FEATURES_BUF_LEN) |
	    LS_64(IRDMA_CQP_OP_QUERY_RDMA_FEATURES, IRDMA_CQPSQ_UP_OP);
	irdma_wmb();		/* make sure WQE is written before valid bit is set */

	set_64bit_val(wqe, IRDMA_BYTE_24, temp);

	irdma_debug_buf(cqp->dev, IRDMA_DEBUG_WQE, "QUERY RDMA FEATURES", wqe,
			IRDMA_CQP_WQE_SIZE * 8);
	irdma_get_cqp_reg_info(cqp, &val, &tail, &error);

	irdma_sc_cqp_post_sq(cqp);
	status = irdma_cqp_poll_registers(cqp, tail,
					  cqp->dev->hw_attrs.max_done_count);
	if (error || status)
		status = -EIO;

	return status;
}

/**
 * irdma_get_rdma_features - get RDMA features
 * @dev: sc device struct
 */
int
irdma_get_rdma_features(struct irdma_sc_dev *dev)
{
	int ret_code;
	struct irdma_dma_mem feat_buf;
	u64 temp;
	u16 byte_idx, feat_type, feat_cnt, feat_idx;

	feat_buf.size = IRDMA_FEATURE_BUF_SIZE;
	feat_buf.va = irdma_allocate_dma_mem(dev->hw, &feat_buf, feat_buf.size,
					     IRDMA_FEATURE_BUF_ALIGNMENT);
	if (!feat_buf.va)
		return -ENOMEM;

	ret_code = irdma_sc_query_rdma_features(dev->cqp, &feat_buf, 0);
	if (ret_code)
		goto exit;

	get_64bit_val(feat_buf.va, IRDMA_BYTE_0, &temp);
	feat_cnt = (u16)RS_64(temp, IRDMA_FEATURE_CNT);
	if (feat_cnt < IRDMA_MIN_FEATURES) {
		ret_code = -EINVAL;
		goto exit;
	} else if (feat_cnt > IRDMA_MAX_FEATURES) {
		irdma_debug(dev, IRDMA_DEBUG_DEV,
			    "feature buf size insufficient,"
			    "retrying with larger buffer\n");
		irdma_free_dma_mem(dev->hw, &feat_buf);
		feat_buf.size = 8 * feat_cnt;
		feat_buf.va = irdma_allocate_dma_mem(dev->hw, &feat_buf,
						     feat_buf.size,
						     IRDMA_FEATURE_BUF_ALIGNMENT);
		if (!feat_buf.va)
			return -ENOMEM;

		ret_code = irdma_sc_query_rdma_features(dev->cqp, &feat_buf, 0);
		if (ret_code)
			goto exit;

		get_64bit_val(feat_buf.va, IRDMA_BYTE_0, &temp);
		feat_cnt = (u16)RS_64(temp, IRDMA_FEATURE_CNT);
		if (feat_cnt < IRDMA_MIN_FEATURES) {
			ret_code = -EINVAL;
			goto exit;
		}
	}

	irdma_debug_buf(dev, IRDMA_DEBUG_WQE, "QUERY RDMA FEATURES", feat_buf.va,
			feat_cnt * 8);

	for (byte_idx = 0, feat_idx = 0; feat_idx < min(feat_cnt, (u16)IRDMA_MAX_FEATURES);
	     feat_idx++, byte_idx += 8) {
		get_64bit_val(feat_buf.va, byte_idx, &temp);
		feat_type = RS_64(temp, IRDMA_FEATURE_TYPE);
		dev->feature_info[feat_type] = temp;
	}
exit:
	irdma_free_dma_mem(dev->hw, &feat_buf);
	return ret_code;
}

static u32 irdma_q1_cnt(struct irdma_sc_dev *dev,
			struct irdma_hmc_info *hmc_info, u32 qpwanted){
	u32 q1_cnt;

	if (dev->hw_attrs.uk_attrs.hw_rev == IRDMA_GEN_1) {
		q1_cnt = roundup_pow_of_two(dev->hw_attrs.max_hw_ird * 2 * qpwanted);
	} else {
		if (dev->cqp->protocol_used != IRDMA_IWARP_PROTOCOL_ONLY)
			q1_cnt = roundup_pow_of_two(dev->hw_attrs.max_hw_ird * 2 * qpwanted + 512);
		else
			q1_cnt = dev->hw_attrs.max_hw_ird * 2 * qpwanted;
	}

	return q1_cnt;
}

static void
cfg_fpm_value_gen_1(struct irdma_sc_dev *dev,
		    struct irdma_hmc_info *hmc_info, u32 qpwanted)
{
	hmc_info->hmc_obj[IRDMA_HMC_IW_XF].cnt = roundup_pow_of_two(qpwanted * dev->hw_attrs.max_hw_wqes);
}

static void
cfg_fpm_value_gen_2(struct irdma_sc_dev *dev,
		    struct irdma_hmc_info *hmc_info, u32 qpwanted)
{
	struct irdma_hmc_fpm_misc *hmc_fpm_misc = &dev->hmc_fpm_misc;

	hmc_info->hmc_obj[IRDMA_HMC_IW_XF].cnt =
	    4 * hmc_fpm_misc->xf_block_size * qpwanted;

	hmc_info->hmc_obj[IRDMA_HMC_IW_HDR].cnt = qpwanted;

	if (hmc_info->hmc_obj[IRDMA_HMC_IW_RRF].max_cnt)
		hmc_info->hmc_obj[IRDMA_HMC_IW_RRF].cnt = 32 * qpwanted;
	if (hmc_info->hmc_obj[IRDMA_HMC_IW_RRFFL].max_cnt)
		hmc_info->hmc_obj[IRDMA_HMC_IW_RRFFL].cnt =
		    hmc_info->hmc_obj[IRDMA_HMC_IW_RRF].cnt /
		    hmc_fpm_misc->rrf_block_size;
	if (hmc_info->hmc_obj[IRDMA_HMC_IW_OOISC].max_cnt)
		hmc_info->hmc_obj[IRDMA_HMC_IW_OOISC].cnt = 32 * qpwanted;
	if (hmc_info->hmc_obj[IRDMA_HMC_IW_OOISCFFL].max_cnt)
		hmc_info->hmc_obj[IRDMA_HMC_IW_OOISCFFL].cnt =
		    hmc_info->hmc_obj[IRDMA_HMC_IW_OOISC].cnt /
		    hmc_fpm_misc->ooiscf_block_size;
}

/**
 * irdma_cfg_fpm_val - configure HMC objects
 * @dev: sc device struct
 * @qp_count: desired qp count
 */
int
irdma_cfg_fpm_val(struct irdma_sc_dev *dev, u32 qp_count)
{
	struct irdma_virt_mem virt_mem;
	u32 i, mem_size;
	u32 qpwanted, mrwanted, pblewanted;
	u32 powerof2, hte;
	u32 sd_needed;
	u32 sd_diff;
	u32 loop_count = 0;
	struct irdma_hmc_info *hmc_info;
	struct irdma_hmc_fpm_misc *hmc_fpm_misc;
	int ret_code = 0;
	u32 max_sds;

	hmc_info = dev->hmc_info;
	hmc_fpm_misc = &dev->hmc_fpm_misc;
	ret_code = irdma_sc_init_iw_hmc(dev, dev->hmc_fn_id);
	if (ret_code) {
		irdma_debug(dev, IRDMA_DEBUG_HMC,
			    "irdma_sc_init_iw_hmc returned error_code = %d\n",
			    ret_code);
		return ret_code;
	}

	max_sds = hmc_fpm_misc->max_sds;

	for (i = IRDMA_HMC_IW_QP; i < IRDMA_HMC_IW_MAX; i++)
		hmc_info->hmc_obj[i].cnt = hmc_info->hmc_obj[i].max_cnt;

	sd_needed = irdma_est_sd(dev, hmc_info);
	irdma_debug(dev, IRDMA_DEBUG_HMC, "sd count %d where max sd is %d\n",
		    hmc_info->sd_table.sd_cnt, max_sds);

	qpwanted = min(qp_count, hmc_info->hmc_obj[IRDMA_HMC_IW_QP].max_cnt);

	powerof2 = 1;
	while (powerof2 <= qpwanted)
		powerof2 *= 2;
	powerof2 /= 2;
	qpwanted = powerof2;

	mrwanted = hmc_info->hmc_obj[IRDMA_HMC_IW_MR].max_cnt;
	pblewanted = hmc_info->hmc_obj[IRDMA_HMC_IW_PBLE].max_cnt;

	irdma_debug(dev, IRDMA_DEBUG_HMC,
		    "req_qp=%d max_sd=%d, max_qp = %d, max_cq=%d, max_mr=%d, max_pble=%d, mc=%d, av=%d\n",
		    qp_count, max_sds,
		    hmc_info->hmc_obj[IRDMA_HMC_IW_QP].max_cnt,
		    hmc_info->hmc_obj[IRDMA_HMC_IW_CQ].max_cnt,
		    hmc_info->hmc_obj[IRDMA_HMC_IW_MR].max_cnt,
		    hmc_info->hmc_obj[IRDMA_HMC_IW_PBLE].max_cnt,
		    hmc_info->hmc_obj[IRDMA_HMC_IW_FSIMC].max_cnt,
		    hmc_info->hmc_obj[IRDMA_HMC_IW_FSIAV].max_cnt);
	hmc_info->hmc_obj[IRDMA_HMC_IW_FSIMC].cnt =
	    hmc_info->hmc_obj[IRDMA_HMC_IW_FSIMC].max_cnt;
	hmc_info->hmc_obj[IRDMA_HMC_IW_FSIAV].cnt =
	    hmc_info->hmc_obj[IRDMA_HMC_IW_FSIAV].max_cnt;
	hmc_info->hmc_obj[IRDMA_HMC_IW_ARP].cnt =
	    hmc_info->hmc_obj[IRDMA_HMC_IW_ARP].max_cnt;

	hmc_info->hmc_obj[IRDMA_HMC_IW_APBVT_ENTRY].cnt = 1;

	while (irdma_q1_cnt(dev, hmc_info, qpwanted) > hmc_info->hmc_obj[IRDMA_HMC_IW_Q1].max_cnt)
		qpwanted /= 2;

	if (dev->hw_attrs.uk_attrs.hw_rev == IRDMA_GEN_1) {
		cfg_fpm_value_gen_1(dev, hmc_info, qpwanted);
		while (hmc_info->hmc_obj[IRDMA_HMC_IW_XF].cnt > hmc_info->hmc_obj[IRDMA_HMC_IW_XF].max_cnt) {
			qpwanted /= 2;
			cfg_fpm_value_gen_1(dev, hmc_info, qpwanted);
		}
	}

	do {
		++loop_count;
		hmc_info->hmc_obj[IRDMA_HMC_IW_QP].cnt = qpwanted;
		hmc_info->hmc_obj[IRDMA_HMC_IW_CQ].cnt =
		    min(2 * qpwanted, hmc_info->hmc_obj[IRDMA_HMC_IW_CQ].cnt);
		hmc_info->hmc_obj[IRDMA_HMC_IW_RESERVED].cnt = 0;	/* Reserved */
		hmc_info->hmc_obj[IRDMA_HMC_IW_MR].cnt = mrwanted;

		hte = round_up(qpwanted + hmc_info->hmc_obj[IRDMA_HMC_IW_FSIMC].cnt, 512);
		powerof2 = 1;
		while (powerof2 < hte)
			powerof2 *= 2;
		hmc_info->hmc_obj[IRDMA_HMC_IW_HTE].cnt =
		    powerof2 * hmc_fpm_misc->ht_multiplier;
		if (dev->hw_attrs.uk_attrs.hw_rev == IRDMA_GEN_1)
			cfg_fpm_value_gen_1(dev, hmc_info, qpwanted);
		else
			cfg_fpm_value_gen_2(dev, hmc_info, qpwanted);

		hmc_info->hmc_obj[IRDMA_HMC_IW_Q1].cnt = irdma_q1_cnt(dev, hmc_info, qpwanted);
		hmc_info->hmc_obj[IRDMA_HMC_IW_XFFL].cnt =
		    hmc_info->hmc_obj[IRDMA_HMC_IW_XF].cnt / hmc_fpm_misc->xf_block_size;
		hmc_info->hmc_obj[IRDMA_HMC_IW_Q1FL].cnt =
		    hmc_info->hmc_obj[IRDMA_HMC_IW_Q1].cnt / hmc_fpm_misc->q1_block_size;
		hmc_info->hmc_obj[IRDMA_HMC_IW_TIMER].cnt =
		    (round_up(qpwanted, 512) / 512 + 1) * hmc_fpm_misc->timer_bucket;

		hmc_info->hmc_obj[IRDMA_HMC_IW_PBLE].cnt = pblewanted;
		sd_needed = irdma_est_sd(dev, hmc_info);
		irdma_debug(dev, IRDMA_DEBUG_HMC,
			    "sd_needed = %d, max_sds=%d, mrwanted=%d, pblewanted=%d qpwanted=%d\n",
			    sd_needed, max_sds, mrwanted, pblewanted, qpwanted);

		/* Do not reduce resources further. All objects fit with max SDs */
		if (sd_needed <= max_sds)
			break;

		sd_diff = sd_needed - max_sds;
		if (sd_diff > 128) {
			if (!(loop_count % 2) && qpwanted > 128) {
				qpwanted /= 2;
			} else {
				mrwanted /= 2;
				pblewanted /= 2;
			}
			continue;
		}
		if (dev->cqp->hmc_profile != IRDMA_HMC_PROFILE_FAVOR_VF &&
		    pblewanted > (512 * FPM_MULTIPLIER * sd_diff)) {
			pblewanted -= 256 * FPM_MULTIPLIER * sd_diff;
			continue;
		} else if (pblewanted > (100 * FPM_MULTIPLIER)) {
			pblewanted -= 10 * FPM_MULTIPLIER;
		} else if (pblewanted > FPM_MULTIPLIER) {
			pblewanted -= FPM_MULTIPLIER;
		} else if (qpwanted <= 128) {
			if (hmc_info->hmc_obj[IRDMA_HMC_IW_FSIMC].cnt > 256)
				hmc_info->hmc_obj[IRDMA_HMC_IW_FSIMC].cnt /= 2;
			if (hmc_info->hmc_obj[IRDMA_HMC_IW_FSIAV].cnt > 256)
				hmc_info->hmc_obj[IRDMA_HMC_IW_FSIAV].cnt /= 2;
		}
		if (mrwanted > FPM_MULTIPLIER)
			mrwanted -= FPM_MULTIPLIER;
		if (!(loop_count % 10) && qpwanted > 128) {
			qpwanted /= 2;
			if (hmc_info->hmc_obj[IRDMA_HMC_IW_FSIAV].cnt > 256)
				hmc_info->hmc_obj[IRDMA_HMC_IW_FSIAV].cnt /= 2;
		}
	} while (loop_count < 2000);

	if (sd_needed > max_sds) {
		irdma_debug(dev, IRDMA_DEBUG_HMC,
			    "cfg_fpm failed loop_cnt=%d, sd_needed=%d, max sd count %d\n",
			    loop_count, sd_needed, hmc_info->sd_table.sd_cnt);
		return -EINVAL;
	}

	if (loop_count > 1 && sd_needed < max_sds) {
		pblewanted += (max_sds - sd_needed) * 256 * FPM_MULTIPLIER;
		hmc_info->hmc_obj[IRDMA_HMC_IW_PBLE].cnt = pblewanted;
		sd_needed = irdma_est_sd(dev, hmc_info);
	}

	irdma_debug(dev, IRDMA_DEBUG_HMC,
		    "loop_cnt=%d, sd_needed=%d, qpcnt = %d, cqcnt=%d, mrcnt=%d, pblecnt=%d, mc=%d, ah=%d, max sd count %d, first sd index %d\n",
		    loop_count, sd_needed,
		    hmc_info->hmc_obj[IRDMA_HMC_IW_QP].cnt,
		    hmc_info->hmc_obj[IRDMA_HMC_IW_CQ].cnt,
		    hmc_info->hmc_obj[IRDMA_HMC_IW_MR].cnt,
		    hmc_info->hmc_obj[IRDMA_HMC_IW_PBLE].cnt,
		    hmc_info->hmc_obj[IRDMA_HMC_IW_FSIMC].cnt,
		    hmc_info->hmc_obj[IRDMA_HMC_IW_FSIAV].cnt,
		    hmc_info->sd_table.sd_cnt, hmc_info->first_sd_index);

	ret_code = irdma_sc_cfg_iw_fpm(dev, dev->hmc_fn_id);
	if (ret_code) {
		irdma_debug(dev, IRDMA_DEBUG_HMC,
			    "cfg_iw_fpm returned error_code[x%08X]\n",
			    readl(dev->hw_regs[IRDMA_CQPERRCODES]));
		return ret_code;
	}

	mem_size = sizeof(struct irdma_hmc_sd_entry) *
	    (hmc_info->sd_table.sd_cnt + hmc_info->first_sd_index + 1);
	virt_mem.size = mem_size;
	virt_mem.va = kzalloc(virt_mem.size, GFP_ATOMIC);
	if (!virt_mem.va) {
		irdma_debug(dev, IRDMA_DEBUG_HMC,
			    "failed to allocate memory for sd_entry buffer\n");
		return -ENOMEM;
	}
	hmc_info->sd_table.sd_entry = virt_mem.va;

	return ret_code;
}

/**
 * irdma_exec_cqp_cmd - execute cqp cmd when wqe are available
 * @dev: rdma device
 * @pcmdinfo: cqp command info
 */
static int
irdma_exec_cqp_cmd(struct irdma_sc_dev *dev,
		   struct cqp_cmds_info *pcmdinfo)
{
	int status;
	struct irdma_dma_mem val_mem;
	bool alloc = false;

	dev->cqp_cmd_stats[pcmdinfo->cqp_cmd]++;
	switch (pcmdinfo->cqp_cmd) {
	case IRDMA_OP_CEQ_DESTROY:
		status = irdma_sc_ceq_destroy(pcmdinfo->in.u.ceq_destroy.ceq,
					      pcmdinfo->in.u.ceq_destroy.scratch,
					      pcmdinfo->post_sq);
		break;
	case IRDMA_OP_AEQ_DESTROY:
		status = irdma_sc_aeq_destroy(pcmdinfo->in.u.aeq_destroy.aeq,
					      pcmdinfo->in.u.aeq_destroy.scratch,
					      pcmdinfo->post_sq);

		break;
	case IRDMA_OP_CEQ_CREATE:
		status = irdma_sc_ceq_create(pcmdinfo->in.u.ceq_create.ceq,
					     pcmdinfo->in.u.ceq_create.scratch,
					     pcmdinfo->post_sq);
		break;
	case IRDMA_OP_AEQ_CREATE:
		status = irdma_sc_aeq_create(pcmdinfo->in.u.aeq_create.aeq,
					     pcmdinfo->in.u.aeq_create.scratch,
					     pcmdinfo->post_sq);
		break;
	case IRDMA_OP_QP_UPLOAD_CONTEXT:
		status = irdma_sc_qp_upload_context(pcmdinfo->in.u.qp_upload_context.dev,
						    &pcmdinfo->in.u.qp_upload_context.info,
						    pcmdinfo->in.u.qp_upload_context.scratch,
						    pcmdinfo->post_sq);
		break;
	case IRDMA_OP_CQ_CREATE:
		status = irdma_sc_cq_create(pcmdinfo->in.u.cq_create.cq,
					    pcmdinfo->in.u.cq_create.scratch,
					    pcmdinfo->in.u.cq_create.check_overflow,
					    pcmdinfo->post_sq);
		break;
	case IRDMA_OP_CQ_MODIFY:
		status = irdma_sc_cq_modify(pcmdinfo->in.u.cq_modify.cq,
					    &pcmdinfo->in.u.cq_modify.info,
					    pcmdinfo->in.u.cq_modify.scratch,
					    pcmdinfo->post_sq);
		break;
	case IRDMA_OP_CQ_DESTROY:
		status = irdma_sc_cq_destroy(pcmdinfo->in.u.cq_destroy.cq,
					     pcmdinfo->in.u.cq_destroy.scratch,
					     pcmdinfo->post_sq);
		break;
	case IRDMA_OP_QP_FLUSH_WQES:
		status = irdma_sc_qp_flush_wqes(pcmdinfo->in.u.qp_flush_wqes.qp,
						&pcmdinfo->in.u.qp_flush_wqes.info,
						pcmdinfo->in.u.qp_flush_wqes.scratch,
						pcmdinfo->post_sq);
		break;
	case IRDMA_OP_GEN_AE:
		status = irdma_sc_gen_ae(pcmdinfo->in.u.gen_ae.qp,
					 &pcmdinfo->in.u.gen_ae.info,
					 pcmdinfo->in.u.gen_ae.scratch,
					 pcmdinfo->post_sq);
		break;
	case IRDMA_OP_MANAGE_PUSH_PAGE:
		status = irdma_sc_manage_push_page(pcmdinfo->in.u.manage_push_page.cqp,
						   &pcmdinfo->in.u.manage_push_page.info,
						   pcmdinfo->in.u.manage_push_page.scratch,
						   pcmdinfo->post_sq);
		break;
	case IRDMA_OP_UPDATE_PE_SDS:
		status = irdma_update_pe_sds(pcmdinfo->in.u.update_pe_sds.dev,
					     &pcmdinfo->in.u.update_pe_sds.info,
					     pcmdinfo->in.u.update_pe_sds.scratch);
		break;
	case IRDMA_OP_MANAGE_HMC_PM_FUNC_TABLE:
		/* switch to calling through the call table */
		status =
		    irdma_sc_manage_hmc_pm_func_table(pcmdinfo->in.u.manage_hmc_pm.dev->cqp,
						      &pcmdinfo->in.u.manage_hmc_pm.info,
						      pcmdinfo->in.u.manage_hmc_pm.scratch,
						      true);
		break;
	case IRDMA_OP_SUSPEND:
		status = irdma_sc_suspend_qp(pcmdinfo->in.u.suspend_resume.cqp,
					     pcmdinfo->in.u.suspend_resume.qp,
					     pcmdinfo->in.u.suspend_resume.scratch);
		break;
	case IRDMA_OP_RESUME:
		status = irdma_sc_resume_qp(pcmdinfo->in.u.suspend_resume.cqp,
					    pcmdinfo->in.u.suspend_resume.qp,
					    pcmdinfo->in.u.suspend_resume.scratch);
		break;
	case IRDMA_OP_QUERY_FPM_VAL:
		val_mem.pa = pcmdinfo->in.u.query_fpm_val.fpm_val_pa;
		val_mem.va = pcmdinfo->in.u.query_fpm_val.fpm_val_va;
		status = irdma_sc_query_fpm_val(pcmdinfo->in.u.query_fpm_val.cqp,
						pcmdinfo->in.u.query_fpm_val.scratch,
						pcmdinfo->in.u.query_fpm_val.hmc_fn_id,
						&val_mem, true, IRDMA_CQP_WAIT_EVENT);
		break;
	case IRDMA_OP_COMMIT_FPM_VAL:
		val_mem.pa = pcmdinfo->in.u.commit_fpm_val.fpm_val_pa;
		val_mem.va = pcmdinfo->in.u.commit_fpm_val.fpm_val_va;
		status = irdma_sc_commit_fpm_val(pcmdinfo->in.u.commit_fpm_val.cqp,
						 pcmdinfo->in.u.commit_fpm_val.scratch,
						 pcmdinfo->in.u.commit_fpm_val.hmc_fn_id,
						 &val_mem,
						 true,
						 IRDMA_CQP_WAIT_EVENT);
		break;
	case IRDMA_OP_STATS_ALLOCATE:
		alloc = true;
		/* fallthrough */
	case IRDMA_OP_STATS_FREE:
		status = irdma_sc_manage_stats_inst(pcmdinfo->in.u.stats_manage.cqp,
						    &pcmdinfo->in.u.stats_manage.info,
						    alloc,
						    pcmdinfo->in.u.stats_manage.scratch);
		break;
	case IRDMA_OP_STATS_GATHER:
		status = irdma_sc_gather_stats(pcmdinfo->in.u.stats_gather.cqp,
					       &pcmdinfo->in.u.stats_gather.info,
					       pcmdinfo->in.u.stats_gather.scratch);
		break;
	case IRDMA_OP_WS_MODIFY_NODE:
		status = irdma_sc_manage_ws_node(pcmdinfo->in.u.ws_node.cqp,
						 &pcmdinfo->in.u.ws_node.info,
						 IRDMA_MODIFY_NODE,
						 pcmdinfo->in.u.ws_node.scratch);
		break;
	case IRDMA_OP_WS_DELETE_NODE:
		status = irdma_sc_manage_ws_node(pcmdinfo->in.u.ws_node.cqp,
						 &pcmdinfo->in.u.ws_node.info,
						 IRDMA_DEL_NODE,
						 pcmdinfo->in.u.ws_node.scratch);
		break;
	case IRDMA_OP_WS_ADD_NODE:
		status = irdma_sc_manage_ws_node(pcmdinfo->in.u.ws_node.cqp,
						 &pcmdinfo->in.u.ws_node.info,
						 IRDMA_ADD_NODE,
						 pcmdinfo->in.u.ws_node.scratch);
		break;
	case IRDMA_OP_SET_UP_MAP:
		status = irdma_sc_set_up_map(pcmdinfo->in.u.up_map.cqp,
					     &pcmdinfo->in.u.up_map.info,
					     pcmdinfo->in.u.up_map.scratch);
		break;
	case IRDMA_OP_QUERY_RDMA_FEATURES:
		status = irdma_sc_query_rdma_features(pcmdinfo->in.u.query_rdma.cqp,
						      &pcmdinfo->in.u.query_rdma.query_buff_mem,
						      pcmdinfo->in.u.query_rdma.scratch);
		break;
	case IRDMA_OP_DELETE_ARP_CACHE_ENTRY:
		status = irdma_sc_del_arp_cache_entry(pcmdinfo->in.u.del_arp_cache_entry.cqp,
						      pcmdinfo->in.u.del_arp_cache_entry.scratch,
						      pcmdinfo->in.u.del_arp_cache_entry.arp_index,
						      pcmdinfo->post_sq);
		break;
	case IRDMA_OP_MANAGE_APBVT_ENTRY:
		status = irdma_sc_manage_apbvt_entry(pcmdinfo->in.u.manage_apbvt_entry.cqp,
						     &pcmdinfo->in.u.manage_apbvt_entry.info,
						     pcmdinfo->in.u.manage_apbvt_entry.scratch,
						     pcmdinfo->post_sq);
		break;
	case IRDMA_OP_MANAGE_QHASH_TABLE_ENTRY:
		status = irdma_sc_manage_qhash_table_entry(pcmdinfo->in.u.manage_qhash_table_entry.cqp,
							   &pcmdinfo->in.u.manage_qhash_table_entry.info,
							   pcmdinfo->in.u.manage_qhash_table_entry.scratch,
							   pcmdinfo->post_sq);
		break;
	case IRDMA_OP_QP_MODIFY:
		status = irdma_sc_qp_modify(pcmdinfo->in.u.qp_modify.qp,
					    &pcmdinfo->in.u.qp_modify.info,
					    pcmdinfo->in.u.qp_modify.scratch,
					    pcmdinfo->post_sq);
		break;
	case IRDMA_OP_QP_CREATE:
		status = irdma_sc_qp_create(pcmdinfo->in.u.qp_create.qp,
					    &pcmdinfo->in.u.qp_create.info,
					    pcmdinfo->in.u.qp_create.scratch,
					    pcmdinfo->post_sq);
		break;
	case IRDMA_OP_QP_DESTROY:
		status = irdma_sc_qp_destroy(pcmdinfo->in.u.qp_destroy.qp,
					     pcmdinfo->in.u.qp_destroy.scratch,
					     pcmdinfo->in.u.qp_destroy.remove_hash_idx,
					     pcmdinfo->in.u.qp_destroy.ignore_mw_bnd,
					     pcmdinfo->post_sq);
		break;
	case IRDMA_OP_ALLOC_STAG:
		status = irdma_sc_alloc_stag(pcmdinfo->in.u.alloc_stag.dev,
					     &pcmdinfo->in.u.alloc_stag.info,
					     pcmdinfo->in.u.alloc_stag.scratch,
					     pcmdinfo->post_sq);
		break;
	case IRDMA_OP_MR_REG_NON_SHARED:
		status = irdma_sc_mr_reg_non_shared(pcmdinfo->in.u.mr_reg_non_shared.dev,
						    &pcmdinfo->in.u.mr_reg_non_shared.info,
						    pcmdinfo->in.u.mr_reg_non_shared.scratch,
						    pcmdinfo->post_sq);
		break;
	case IRDMA_OP_DEALLOC_STAG:
		status = irdma_sc_dealloc_stag(pcmdinfo->in.u.dealloc_stag.dev,
					       &pcmdinfo->in.u.dealloc_stag.info,
					       pcmdinfo->in.u.dealloc_stag.scratch,
					       pcmdinfo->post_sq);
		break;
	case IRDMA_OP_MW_ALLOC:
		status = irdma_sc_mw_alloc(pcmdinfo->in.u.mw_alloc.dev,
					   &pcmdinfo->in.u.mw_alloc.info,
					   pcmdinfo->in.u.mw_alloc.scratch,
					   pcmdinfo->post_sq);
		break;
	case IRDMA_OP_ADD_ARP_CACHE_ENTRY:
		status = irdma_sc_add_arp_cache_entry(pcmdinfo->in.u.add_arp_cache_entry.cqp,
						      &pcmdinfo->in.u.add_arp_cache_entry.info,
						      pcmdinfo->in.u.add_arp_cache_entry.scratch,
						      pcmdinfo->post_sq);
		break;
	case IRDMA_OP_ALLOC_LOCAL_MAC_ENTRY:
		status = irdma_sc_alloc_local_mac_entry(pcmdinfo->in.u.alloc_local_mac_entry.cqp,
							pcmdinfo->in.u.alloc_local_mac_entry.scratch,
							pcmdinfo->post_sq);
		break;
	case IRDMA_OP_ADD_LOCAL_MAC_ENTRY:
		status = irdma_sc_add_local_mac_entry(pcmdinfo->in.u.add_local_mac_entry.cqp,
						      &pcmdinfo->in.u.add_local_mac_entry.info,
						      pcmdinfo->in.u.add_local_mac_entry.scratch,
						      pcmdinfo->post_sq);
		break;
	case IRDMA_OP_DELETE_LOCAL_MAC_ENTRY:
		status = irdma_sc_del_local_mac_entry(pcmdinfo->in.u.del_local_mac_entry.cqp,
						      pcmdinfo->in.u.del_local_mac_entry.scratch,
						      pcmdinfo->in.u.del_local_mac_entry.entry_idx,
						      pcmdinfo->in.u.del_local_mac_entry.ignore_ref_count,
						      pcmdinfo->post_sq);
		break;
	case IRDMA_OP_AH_CREATE:
		status = irdma_sc_create_ah(pcmdinfo->in.u.ah_create.cqp,
					    &pcmdinfo->in.u.ah_create.info,
					    pcmdinfo->in.u.ah_create.scratch);
		break;
	case IRDMA_OP_AH_DESTROY:
		status = irdma_sc_destroy_ah(pcmdinfo->in.u.ah_destroy.cqp,
					     &pcmdinfo->in.u.ah_destroy.info,
					     pcmdinfo->in.u.ah_destroy.scratch);
		break;
	case IRDMA_OP_MC_CREATE:
		status = irdma_sc_create_mcast_grp(pcmdinfo->in.u.mc_create.cqp,
						   &pcmdinfo->in.u.mc_create.info,
						   pcmdinfo->in.u.mc_create.scratch);
		break;
	case IRDMA_OP_MC_DESTROY:
		status = irdma_sc_destroy_mcast_grp(pcmdinfo->in.u.mc_destroy.cqp,
						    &pcmdinfo->in.u.mc_destroy.info,
						    pcmdinfo->in.u.mc_destroy.scratch);
		break;
	case IRDMA_OP_MC_MODIFY:
		status = irdma_sc_modify_mcast_grp(pcmdinfo->in.u.mc_modify.cqp,
						   &pcmdinfo->in.u.mc_modify.info,
						   pcmdinfo->in.u.mc_modify.scratch);
		break;
	default:
		status = -EOPNOTSUPP;
		break;
	}

	return status;
}

/**
 * irdma_process_cqp_cmd - process all cqp commands
 * @dev: sc device struct
 * @pcmdinfo: cqp command info
 */
int
irdma_process_cqp_cmd(struct irdma_sc_dev *dev,
		      struct cqp_cmds_info *pcmdinfo)
{
	int status = 0;
	unsigned long flags;

	if (dev->no_cqp)
		return -EFAULT;

	spin_lock_irqsave(&dev->cqp_lock, flags);
	if (list_empty(&dev->cqp_cmd_head) && !irdma_cqp_ring_full(dev->cqp))
		status = irdma_exec_cqp_cmd(dev, pcmdinfo);
	else
		list_add_tail(&pcmdinfo->cqp_cmd_entry, &dev->cqp_cmd_head);
	spin_unlock_irqrestore(&dev->cqp_lock, flags);
	return status;
}

/**
 * irdma_process_bh - called from tasklet for cqp list
 * @dev: sc device struct
 */
int
irdma_process_bh(struct irdma_sc_dev *dev)
{
	int status = 0;
	struct cqp_cmds_info *pcmdinfo;
	unsigned long flags;

	spin_lock_irqsave(&dev->cqp_lock, flags);
	while (!list_empty(&dev->cqp_cmd_head) &&
	       !irdma_cqp_ring_full(dev->cqp)) {
		pcmdinfo = (struct cqp_cmds_info *)irdma_remove_cqp_head(dev);
		status = irdma_exec_cqp_cmd(dev, pcmdinfo);
		if (status)
			break;
	}
	spin_unlock_irqrestore(&dev->cqp_lock, flags);
	return status;
}

/**
 * irdma_cfg_aeq- Configure AEQ interrupt
 * @dev: pointer to the device structure
 * @idx: vector index
 * @enable: True to enable, False disables
 */
void
irdma_cfg_aeq(struct irdma_sc_dev *dev, u32 idx, bool enable)
{
	u32 reg_val;
	reg_val = enable ? IRDMA_PFINT_AEQCTL_CAUSE_ENA_M : 0;
	reg_val |= (idx << IRDMA_PFINT_AEQCTL_MSIX_INDX_S) |
	    IRDMA_PFINT_AEQCTL_ITR_INDX_M;
	writel(reg_val, dev->hw_regs[IRDMA_PFINT_AEQCTL]);
}

/**
 * sc_vsi_update_stats - Update statistics
 * @vsi: sc_vsi instance to update
 */
void
sc_vsi_update_stats(struct irdma_sc_vsi *vsi)
{
	struct irdma_gather_stats *gather_stats;
	struct irdma_gather_stats *last_gather_stats;

	gather_stats = vsi->pestat->gather_info.gather_stats_va;
	last_gather_stats = vsi->pestat->gather_info.last_gather_stats_va;
	irdma_update_stats(&vsi->pestat->hw_stats, gather_stats,
			   last_gather_stats, vsi->dev->hw_stats_map,
			   vsi->dev->hw_attrs.max_stat_idx);
}

/**
 * irdma_wait_pe_ready - Check if firmware is ready
 * @dev: provides access to registers
 */
static int
irdma_wait_pe_ready(struct irdma_sc_dev *dev)
{
	u32 statuscpu0;
	u32 statuscpu1;
	u32 statuscpu2;
	u32 retrycount = 0;

	do {
		statuscpu0 = readl(dev->hw_regs[IRDMA_GLPE_CPUSTATUS0]);
		statuscpu1 = readl(dev->hw_regs[IRDMA_GLPE_CPUSTATUS1]);
		statuscpu2 = readl(dev->hw_regs[IRDMA_GLPE_CPUSTATUS2]);
		if (statuscpu0 == 0x80 && statuscpu1 == 0x80 &&
		    statuscpu2 == 0x80)
			return 0;
		mdelay(1000);
	} while (retrycount++ < dev->hw_attrs.max_pe_ready_count);
	return -1;
}

static inline void
irdma_sc_init_hw(struct irdma_sc_dev *dev)
{
	switch (dev->hw_attrs.uk_attrs.hw_rev) {
	case IRDMA_GEN_2:
		icrdma_init_hw(dev);
		break;
	}
}

/**
 * irdma_sc_dev_init - Initialize control part of device
 * @ver: version
 * @dev: Device pointer
 * @info: Device init info
 */
int
irdma_sc_dev_init(enum irdma_vers ver, struct irdma_sc_dev *dev,
		  struct irdma_device_init_info *info)
{
	u32 val;
	int ret_code = 0;
	u8 db_size;

	INIT_LIST_HEAD(&dev->cqp_cmd_head);	/* for CQP command backlog */
	mutex_init(&dev->ws_mutex);
	dev->debug_mask = info->debug_mask;
	dev->hmc_fn_id = info->hmc_fn_id;
	dev->fpm_query_buf_pa = info->fpm_query_buf_pa;
	dev->fpm_query_buf = info->fpm_query_buf;
	dev->fpm_commit_buf_pa = info->fpm_commit_buf_pa;
	dev->fpm_commit_buf = info->fpm_commit_buf;
	dev->hw = info->hw;
	dev->hw->hw_addr = info->bar0;
	/* Setup the hardware limits, hmc may limit further */
	dev->hw_attrs.min_hw_qp_id = IRDMA_MIN_IW_QP_ID;
	dev->hw_attrs.min_hw_aeq_size = IRDMA_MIN_AEQ_ENTRIES;
	dev->hw_attrs.max_hw_aeq_size = IRDMA_MAX_AEQ_ENTRIES;
	dev->hw_attrs.min_hw_ceq_size = IRDMA_MIN_CEQ_ENTRIES;
	dev->hw_attrs.max_hw_ceq_size = IRDMA_MAX_CEQ_ENTRIES;
	dev->hw_attrs.uk_attrs.min_hw_cq_size = IRDMA_MIN_CQ_SIZE;
	dev->hw_attrs.uk_attrs.max_hw_cq_size = IRDMA_MAX_CQ_SIZE;
	dev->hw_attrs.max_hw_outbound_msg_size = IRDMA_MAX_OUTBOUND_MSG_SIZE;
	dev->hw_attrs.max_mr_size = IRDMA_MAX_MR_SIZE;
	dev->hw_attrs.max_hw_inbound_msg_size = IRDMA_MAX_INBOUND_MSG_SIZE;
	dev->hw_attrs.max_hw_device_pages = IRDMA_MAX_PUSH_PAGE_COUNT;
	dev->hw_attrs.uk_attrs.max_hw_inline = IRDMA_MAX_INLINE_DATA_SIZE;
	dev->hw_attrs.max_hw_wqes = IRDMA_MAX_WQ_ENTRIES;
	dev->hw_attrs.max_qp_wr = IRDMA_MAX_QP_WRS(IRDMA_MAX_QUANTA_PER_WR);

	dev->hw_attrs.uk_attrs.max_hw_rq_quanta = IRDMA_QP_SW_MAX_RQ_QUANTA;
	dev->hw_attrs.uk_attrs.max_hw_wq_quanta = IRDMA_QP_SW_MAX_WQ_QUANTA;
	dev->hw_attrs.max_hw_pds = IRDMA_MAX_PDS;
	dev->hw_attrs.max_hw_ena_vf_count = IRDMA_MAX_PE_ENA_VF_COUNT;

	dev->hw_attrs.max_pe_ready_count = 14;
	dev->hw_attrs.max_done_count = IRDMA_DONE_COUNT;
	dev->hw_attrs.max_sleep_count = IRDMA_SLEEP_COUNT;
	dev->hw_attrs.max_cqp_compl_wait_time_ms = CQP_COMPL_WAIT_TIME_MS;

	dev->hw_attrs.uk_attrs.hw_rev = ver;
	irdma_sc_init_hw(dev);

	if (irdma_wait_pe_ready(dev))
		return -ETIMEDOUT;

	val = readl(dev->hw_regs[IRDMA_GLPCI_LBARCTRL]);
	db_size = (u8)RS_32(val, IRDMA_GLPCI_LBARCTRL_PE_DB_SIZE);
	if (db_size != IRDMA_PE_DB_SIZE_4M && db_size != IRDMA_PE_DB_SIZE_8M) {
		irdma_debug(dev, IRDMA_DEBUG_DEV,
			    "RDMA PE doorbell is not enabled in CSR val 0x%x db_size=%d\n",
			    val, db_size);
		return -ENODEV;
	}
	dev->db_addr = dev->hw->hw_addr + (uintptr_t)dev->hw_regs[IRDMA_DB_ADDR_OFFSET];

	return ret_code;
}

/**
 * irdma_stat_val - Extract HW counter value from statistics buffer
 * @stats_val: pointer to statistics buffer
 * @byteoff: byte offset of counter value in the buffer (8B-aligned)
 * @bitoff: bit offset of counter value within 8B entry
 * @bitmask: maximum counter value (e.g. 0xffffff for 24-bit counter)
 */
static inline u64 irdma_stat_val(const u64 *stats_val, u16 byteoff,
				 u8 bitoff, u64 bitmask){
	u16 idx = byteoff / sizeof(*stats_val);

	return (stats_val[idx] >> bitoff) & bitmask;
}

/**
 * irdma_stat_delta - Calculate counter delta
 * @new_val: updated counter value
 * @old_val: last counter value
 * @max_val: maximum counter value (e.g. 0xffffff for 24-bit counter)
 */
static inline u64 irdma_stat_delta(u64 new_val, u64 old_val, u64 max_val) {
	if (new_val >= old_val)
		return new_val - old_val;
	else
		/* roll-over case */
		return max_val - old_val + new_val + 1;
}

/**
 * irdma_update_stats - Update statistics
 * @hw_stats: hw_stats instance to update
 * @gather_stats: updated stat counters
 * @last_gather_stats: last stat counters
 * @map: HW stat map (hw_stats => gather_stats)
 * @max_stat_idx: number of HW stats
 */
void
irdma_update_stats(struct irdma_dev_hw_stats *hw_stats,
		   struct irdma_gather_stats *gather_stats,
		   struct irdma_gather_stats *last_gather_stats,
		   const struct irdma_hw_stat_map *map,
		   u16 max_stat_idx)
{
	u64 *stats_val = hw_stats->stats_val;
	u16 i;

	for (i = 0; i < max_stat_idx; i++) {
		u64 new_val = irdma_stat_val(gather_stats->val,
					     map[i].byteoff, map[i].bitoff,
					     map[i].bitmask);
		u64 last_val = irdma_stat_val(last_gather_stats->val,
					      map[i].byteoff, map[i].bitoff,
					      map[i].bitmask);

		stats_val[i] += irdma_stat_delta(new_val, last_val,
						 map[i].bitmask);
	}

	irdma_memcpy(last_gather_stats, gather_stats,
		     sizeof(*last_gather_stats));
}
