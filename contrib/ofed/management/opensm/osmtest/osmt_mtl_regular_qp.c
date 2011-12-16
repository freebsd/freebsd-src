/*
 * Copyright (c) 2002-2005 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifdef OSM_VENDOR_INTF_MTL

/*                  - Mellanox Confidential and Proprietary -
 *
 *  Copyright (C) Jul. 2001, Mellanox Technologies Ltd.  ALL RIGHTS RESERVED.
 *
 *  Except as specifically permitted herein, no portion of the information,
 *  including but not limited to object code and source code, may be reproduced,
 *  modified, distributed, republished or otherwise exploited in any form or by
 *  any means for any purpose without the prior written permission of Mellanox
 *  Technologies Ltd. Use of software subject to the terms and conditions
 *  detailed in the file "LICENSE.txt".
 *
 *  End of legal section ......................................................
 *
 *  osmt_mtl_regular_qp.c -
 *    Provide Simple Interface for Sending and Receiving MADS through a regular QP
 *
 *  Creation date:
 *
 *  Version: $Id$
 *
 *  Authors:
 *    Eitan Zahavi
 *
 *  Changes:
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <mtl_common.h>
#include <vapi.h>
#include <evapi.h>
#include <vapi_common.h>
#include <ib_defs.h>
#include <osmt_mtl_regular_qp.h>
#include <complib/cl_types.h>
/*
 * Initialize the QP etc.
 * Given in res: port_num, max_outs_sq, max_outs_rq
 */
VAPI_ret_t osmt_mtl_get_qp_resources(IN OUT osmt_mtl_mad_res_t * res)
{
	VAPI_ret_t ret;
	VAPI_hca_port_t hca_port_info;
	VAPI_qp_init_attr_t qp_init_attr;
	VAPI_qp_prop_t qp_prop;
	VAPI_cqe_num_t act_num;

	/* Get HCA LID */
	ret =
	    VAPI_query_hca_port_prop(res->hca_hndl, res->port_num,
				     &hca_port_info);
	VAPI_CHECK_RET;
	res->slid = hca_port_info.lid;

	/* Get a PD */
	ret = VAPI_alloc_pd(res->hca_hndl, &(res->pd_hndl));
	VAPI_CHECK_RET;

	/* Create CQ for RQ and SQ *//* TBD - Check we have enough act nums */
	ret =
	    VAPI_create_cq(res->hca_hndl, res->max_outs_sq + 1,
			   &(res->sq_cq_hndl), &act_num);
	VAPI_CHECK_RET;
	ret =
	    VAPI_create_cq(res->hca_hndl, res->max_outs_rq + 1,
			   &(res->rq_cq_hndl), &act_num);
	VAPI_CHECK_RET;

	/* register event handlers for polling(block mode) internal use */
	/* ret= EVAPI_set_comp_eventh(res->hca_hndl,res->rq_cq_hndl, */
	/*                            EVAPI_POLL_CQ_UNBLOCK_HANDLER,NULL,&(res->rq_cq_eventh)); */
	/* VAPI_CHECK_RET; */
	/* ret= EVAPI_set_comp_eventh(res->hca_hndl,res->sq_cq_hndl, */
	/*                            EVAPI_POLL_CQ_UNBLOCK_HANDLER,NULL,&(res->sq_cq_eventh)); */
	/* VAPI_CHECK_RET; */

	/* Create QP */
	qp_init_attr.cap.max_oust_wr_sq = res->max_outs_sq + 1;
	qp_init_attr.cap.max_oust_wr_rq = res->max_outs_rq + 1;
	qp_init_attr.cap.max_sg_size_sq = 4;
	qp_init_attr.cap.max_sg_size_rq = 4;

	qp_init_attr.pd_hndl = res->pd_hndl;
	qp_init_attr.rdd_hndl = 0;
	qp_init_attr.rq_cq_hndl = res->rq_cq_hndl;
	qp_init_attr.rq_sig_type = VAPI_SIGNAL_ALL_WR;	/* That's default for IB */
	qp_init_attr.sq_cq_hndl = res->sq_cq_hndl;
	qp_init_attr.sq_sig_type = VAPI_SIGNAL_REQ_WR;
	qp_init_attr.ts_type = VAPI_TS_UD;

	ret =
	    VAPI_create_qp(res->hca_hndl, &qp_init_attr, &(res->qp_hndl),
			   &qp_prop);
	VAPI_CHECK_RET;
	res->qp_id.qp_num = qp_prop.qp_num;

	return (VAPI_OK);
}

VAPI_ret_t osmt_mtl_qp_init(osmt_mtl_mad_res_t * res)
{
	VAPI_ret_t ret;

	VAPI_qp_attr_t qp_attr;
	VAPI_qp_attr_mask_t qp_attr_mask;
	VAPI_qp_cap_t qp_cap;

	/*
	 * Change QP to INIT
	 *
	 */
	QP_ATTR_MASK_CLR_ALL(qp_attr_mask);
	qp_attr.qp_state = VAPI_INIT;
	QP_ATTR_MASK_SET(qp_attr_mask, QP_ATTR_QP_STATE);
	qp_attr.pkey_ix = 0;
	QP_ATTR_MASK_SET(qp_attr_mask, QP_ATTR_PKEY_IX);
	qp_attr.port = res->port_num;
	QP_ATTR_MASK_SET(qp_attr_mask, QP_ATTR_PORT);
	qp_attr.qkey = res->qkey;
	QP_ATTR_MASK_SET(qp_attr_mask, QP_ATTR_QKEY);

	/* If I do not set this mask, I get an error from HH. QPM should catch it */
	ret =
	    VAPI_modify_qp(res->hca_hndl, res->qp_hndl, &qp_attr, &qp_attr_mask,
			   &qp_cap);
	VAPI_CHECK_RET;

	return (ret);

}

VAPI_ret_t osmt_mtl_qp_2_rtr_rts(osmt_mtl_mad_res_t * res)
{
	VAPI_ret_t ret;

	VAPI_qp_attr_t qp_attr;
	VAPI_qp_attr_mask_t qp_attr_mask;
	VAPI_qp_cap_t qp_cap;

	/*
	 *  Change QP to RTR
	 *
	 */
	QP_ATTR_MASK_CLR_ALL(qp_attr_mask);
	qp_attr.qp_state = VAPI_RTR;
	QP_ATTR_MASK_SET(qp_attr_mask, QP_ATTR_QP_STATE);
	/*   qp_attr.rq_psn   = 0;                */
	/*   QP_ATTR_MASK_SET(qp_attr_mask,QP_ATTR_RQ_PSN); */

	ret =
	    VAPI_modify_qp(res->hca_hndl, res->qp_hndl, &qp_attr, &qp_attr_mask,
			   &qp_cap);
	VAPI_CHECK_RET;

	/*
	 * Change QP to RTS
	 *
	 */
	QP_ATTR_MASK_CLR_ALL(qp_attr_mask);
	qp_attr.qp_state = VAPI_RTS;
	QP_ATTR_MASK_SET(qp_attr_mask, QP_ATTR_QP_STATE);
	qp_attr.sq_psn = 0;
	QP_ATTR_MASK_SET(qp_attr_mask, QP_ATTR_SQ_PSN);

	ret =
	    VAPI_modify_qp(res->hca_hndl, res->qp_hndl, &qp_attr, &qp_attr_mask,
			   &qp_cap);
	VAPI_CHECK_RET;

	return (ret);
}

VAPI_ret_t osmt_mtl_mad_create_mr(osmt_mtl_mad_res_t * res)
{

	VAPI_ret_t ret;

	VAPI_mrw_t mr_in, mr_out;

	res->buf_size =
	    (MAD_SIZE + GRH_LEN) * (res->max_outs_sq + res->max_outs_rq + 1);

	/* Register single memory address region for all buffers */
	res->buf_ptr = VMALLOC(res->buf_size);

	if (res->buf_ptr == ((VAPI_virt_addr_t) NULL)) {
		ret = VAPI_EAGAIN;
		VAPI_CHECK_RET;
	}

	/* Enable local and remote access to memory region */
	mr_in.acl = VAPI_EN_LOCAL_WRITE | VAPI_EN_REMOTE_WRITE;
	mr_in.l_key = 0;
	mr_in.pd_hndl = res->pd_hndl;
	mr_in.r_key = 0;
	mr_in.size = res->buf_size;
	ASSERT_VOIDP2UINTN(res->buf_ptr);
	mr_in.start = (VAPI_virt_addr_t) (uintn_t) (res->buf_ptr);
	mr_in.type = VAPI_MR;

	ret = VAPI_register_mr(res->hca_hndl, &mr_in, &(res->mr_hndl), &mr_out);
	VAPI_CHECK_RET;

	res->l_key = mr_out.l_key;

	return (ret);
}

VAPI_ret_t osmt_mtl_init_opened_hca(osmt_mtl_mad_res_t * res)
{
	VAPI_ret_t ret;

	res->pd_hndl = VAPI_INVAL_HNDL;
	res->rq_cq_hndl = VAPI_INVAL_HNDL;
	res->sq_cq_hndl = VAPI_INVAL_HNDL;
	res->sq_cq_eventh = VAPI_INVAL_HNDL;
	res->rq_cq_eventh = VAPI_INVAL_HNDL;
	res->qp_hndl = VAPI_INVAL_HNDL;
	res->mr_hndl = VAPI_INVAL_HNDL;

	/*
	 * Create QP
	 *
	 */
	ret = osmt_mtl_get_qp_resources(res);
	if (ret != VAPI_OK) {
		return ret;
	}

	/*
	 * Move to init
	 *
	 */
	ret = osmt_mtl_qp_init(res);
	if (ret != VAPI_OK) {
		return ret;
	}

	/*
	 * Initialize memory regions
	 *
	 */
	ret = osmt_mtl_mad_create_mr(res);
	if (ret != VAPI_OK) {
		return ret;
	}

	/* only now move to RTR and RTS */
	ret = osmt_mtl_qp_2_rtr_rts(res);
	if (ret != VAPI_OK) {
		return ret;
	}

	return VAPI_OK;
}

VAPI_ret_t osmt_mtl_mad_cleanup(osmt_mtl_mad_res_t * res)
{
	if (res->qp_hndl != VAPI_INVAL_HNDL) {
		VAPI_destroy_qp(res->hca_hndl, res->qp_hndl);
	}
	if (res->sq_cq_eventh != VAPI_INVAL_HNDL) {
		EVAPI_clear_comp_eventh(res->hca_hndl, res->sq_cq_eventh);
	}
	if (res->rq_cq_eventh != VAPI_INVAL_HNDL) {
		EVAPI_clear_comp_eventh(res->hca_hndl, res->rq_cq_eventh);
	}
	if (res->rq_cq_hndl != VAPI_INVAL_HNDL) {
		VAPI_destroy_cq(res->hca_hndl, res->rq_cq_hndl);
	}
	if (res->sq_cq_hndl != VAPI_INVAL_HNDL) {
		VAPI_destroy_cq(res->hca_hndl, res->sq_cq_hndl);
	}
	if (res->mr_hndl != VAPI_INVAL_HNDL) {
		VAPI_deregister_mr(res->hca_hndl, res->mr_hndl);
	}
	if (res->pd_hndl != VAPI_INVAL_HNDL) {
		VAPI_dealloc_pd(res->hca_hndl, res->pd_hndl);
	}
#if 0
	/* open/close of HCA should be done system wide - not per application */
	if (res->hca_hndl != VAPI_INVAL_HNDL) {
		VAPI_close_hca(res->hca_hndl);	/* TBD: HCA_open/close should be done on a system wide basis */
	}
#endif
	return VAPI_OK;
}

VAPI_ret_t osmt_mtl_create_av(osmt_mtl_mad_res_t * res, int16_t dlid,
			      VAPI_ud_av_hndl_t * avh_p)
{
	VAPI_ud_av_t av;
	VAPI_ret_t ret;

	av.dlid = dlid;
	av.port = res->port_num;
	av.sl = 0;		/* dest->sl; */
	av.src_path_bits = 0;	/*  dest->ee_dlid.dst_path_bits; */
	av.static_rate = 0;
	/* GRH ? */
	av.grh_flag = 0;

	ret = VAPI_create_addr_hndl(res->hca_hndl, res->pd_hndl, &av, avh_p);
	if (ret != VAPI_OK) {
		MTL_ERROR1("%s: failed VAPI_create_addr_hndl (%s)\n", __func__,
			   VAPI_strerror_sym(ret));
		return ret;
	}
	return VAPI_OK;
}

VAPI_ret_t osmt_mtl_mad_send(osmt_mtl_mad_res_t * res, VAPI_wr_id_t id,
			     void *mad, VAPI_qp_num_t dest_qp, IB_sl_t sl,
			     u_int32_t dest_qkey, VAPI_ud_av_hndl_t avh)
{
	VAPI_sr_desc_t sr;
	VAPI_sg_lst_entry_t sg_entry;
	VAPI_ret_t ret;

	/* building SEND request */
	sr.opcode = VAPI_SEND;
	sr.remote_ah = avh;
	sr.remote_qp = dest_qp;
	sr.remote_qkey = dest_qkey;

	sr.id = id;
	sr.set_se = FALSE;
	sr.fence = FALSE;
	sr.comp_type = VAPI_SIGNALED;
	sr.sg_lst_len = 1;
	sr.sg_lst_p = &sg_entry;
	ASSERT_VOIDP2UINTN(mad);
	sg_entry.addr = (VAPI_virt_addr_t) (uintn_t) (mad);
	sg_entry.len = MAD_SIZE;
	sg_entry.lkey = res->l_key;

	ret = VAPI_post_sr(res->hca_hndl, res->qp_hndl, &sr);
	if (ret != VAPI_OK) {
		MTL_ERROR1(__FUNCTION__ ": failed VAPI_post_sr (%s)\n",
			   VAPI_strerror_sym(ret));
		return ret;
	}

	return VAPI_OK;
}

int osmt_mtl_mad_post_recv_bufs(osmt_mtl_mad_res_t * res, void *buf_array,
				u_int32_t num_o_bufs, u_int32_t size,
				VAPI_wr_id_t start_id)
{
	uint32_t i;
	void *cur_buf;
	VAPI_rr_desc_t rr;
	VAPI_sg_lst_entry_t sg_entry;
	VAPI_ret_t ret;

	rr.opcode = VAPI_RECEIVE;
	rr.comp_type = VAPI_SIGNALED;	/* All with CQE (IB compliant) */
	rr.sg_lst_len = 1;	/* single buffers */
	rr.sg_lst_p = &sg_entry;
	sg_entry.lkey = res->l_key;
	cur_buf = buf_array;
	for (i = 0; i < num_o_bufs; i++) {
		rr.id = start_id + i;	/* WQE id used is the index to buffers ptr array */
		ASSERT_VOIDP2UINTN(cur_buf);
		sg_entry.addr = (VAPI_virt_addr_t) (uintn_t) cur_buf;
		sg_entry.len = size;
		memset(cur_buf, 0x00, size);	/* fill with 0 */
		ret = VAPI_post_rr(res->hca_hndl, res->qp_hndl, &rr);
		if (ret != VAPI_OK) {
			MTL_ERROR1(__FUNCTION__
				   ": failed posting RQ WQE (%s)\n",
				   VAPI_strerror_sym(ret));
			return i;
		}
		MTL_DEBUG4(__FUNCTION__ ": posted buf at %p\n", cur_buf);
		cur_buf += size;
	}

	return i;		/* num of buffers posted */
}

VAPI_ret_t osmt_mtl_mad_poll4cqe(VAPI_hca_hndl_t hca, VAPI_cq_hndl_t cq,
				 VAPI_wc_desc_t * wc_desc_p,
				 u_int32_t max_poll, u_int32_t poll_sleep,
				 VAPI_ud_av_hndl_t * avh_p)
{
	VAPI_ret_t ret = VAPI_CQ_EMPTY;
	u_int32_t poll_cnt = 0;

	/* wait for something to arrive */
	while ((ret == VAPI_CQ_EMPTY) && (poll_cnt < max_poll)) {
		ret = VAPI_poll_cq(hca, cq, wc_desc_p);
		/* don't sleep if we already succeeded) */
		if (ret != VAPI_CQ_EMPTY) {
			break;
		}
		usleep(poll_sleep);
		poll_cnt++;
	}

	/* if passed an AVH to destory - do it */
	if (avh_p != NULL) {
		VAPI_destroy_addr_hndl(hca, *avh_p);
	}

	if ((poll_cnt == max_poll) && (ret == VAPI_CQ_EMPTY)) {
		MTL_DEBUG1(__FUNCTION__
			   ": Failed to get completion on wq after %d polls.\n",
			   max_poll);
		return VAPI_CQ_EMPTY;
	}

	if (ret != VAPI_OK) {
		MTL_DEBUG1(__FUNCTION__
			   ": VAPI_poll_cq failed with ret=%s on sq_cq\n",
			   mtl_strerror_sym(ret));
		return ret;
	}

	if (wc_desc_p->status != VAPI_SUCCESS) {
		MTL_DEBUG1(__FUNCTION__ ": completion error (%d) detected\n",
			   wc_desc_p->status);
	}

	return VAPI_OK;
}

#endif				/*  OSM_VENDOR_INTF_MTL */
