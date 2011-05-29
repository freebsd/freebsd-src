/*
 * Copyright (c) 2006-2008 Voltaire, Inc. All rights reserved.
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
/*
 * Abstract:
 *    Implementation of InformInfo testing flow..
 *    Top level is osmt_run_inform_info_flow:
 *     osmt_bind_inform_qp
 *     osmt_reg_unreg_inform_info
 *     osmt_send_trap_wait_for_forward
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <complib/cl_debug.h>
#include <vendor/osm_vendor_mlx_hca.h>
#include "osmtest.h"
#include "osmt_inform.h"

/*
 * Prepare an asynchronous QP (rcv) for sending inform info and
 * handling the incoming reports.
 *
 */
ib_api_status_t
osmt_bind_inform_qp(IN osmtest_t * const p_osmt, OUT osmt_qp_ctx_t * p_qp_ctx)
{
	ib_net64_t port_guid;
	VAPI_hca_hndl_t hca_hndl;
	VAPI_hca_id_t hca_id;
	uint32_t port_num;
	VAPI_ret_t vapi_ret;
	IB_MGT_ret_t mgt_ret;
	uint8_t hca_index;
	osm_log_t *p_log = &p_osmt->log;
	ib_api_status_t status = IB_SUCCESS;

	OSM_LOG_ENTER(p_log);

	port_guid = p_osmt->local_port.port_guid;

	OSM_LOG(p_log, OSM_LOG_DEBUG, "Binding to port 0x%" PRIx64 "\n",
		cl_ntoh64(port_guid));

	/* obtain the hca name and port num from the guid */
	OSM_LOG(p_log, OSM_LOG_DEBUG,
		"Finding CA and Port that owns port guid 0x%" PRIx64 "\n",
		port_guid);

	mgt_ret =
	    osm_vendor_get_guid_ca_and_port(p_osmt->p_vendor,
					    port_guid,
					    &hca_hndl,
					    &hca_id[0], &hca_index, &port_num);
	if (mgt_ret != IB_MGT_OK) {
		OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 0109: "
			"Unable to obtain CA and port (%d).\n");
		status = IB_ERROR;
		goto Exit;
	}
#define OSMT_MTL_REVERSE_QP1_WELL_KNOWN_Q_KEY 0x80010000

	strncpy(p_qp_ctx->qp_bind_hndl.hca_id, hca_id, sizeof(hca_id));
	p_qp_ctx->qp_bind_hndl.hca_hndl = hca_hndl;
	p_qp_ctx->qp_bind_hndl.port_num = port_num;
	p_qp_ctx->qp_bind_hndl.max_outs_sq = 10;
	p_qp_ctx->qp_bind_hndl.max_outs_rq = 10;
	p_qp_ctx->qp_bind_hndl.qkey = OSMT_MTL_REVERSE_QP1_WELL_KNOWN_Q_KEY;

	vapi_ret = osmt_mtl_init_opened_hca(&p_qp_ctx->qp_bind_hndl);
	if (vapi_ret != VAPI_OK) {
		OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 0114: "
			"Error initializing QP.\n");
		status = IB_ERROR;
		goto Exit;
	}

	/* we use the pre-allocated buffers for send and receive :
	   send from buf[0]
	   receive from buf[2]
	 */
	p_qp_ctx->p_send_buf =
	    (uint8_t *) p_qp_ctx->qp_bind_hndl.buf_ptr + GRH_LEN;
	p_qp_ctx->p_recv_buf =
	    (uint8_t *) p_qp_ctx->qp_bind_hndl.buf_ptr + 2 * (GRH_LEN +
							      MAD_BLOCK_SIZE);

	/* Need to clear assigned memory of p_send_buf - before using it to send any data */
	memset(p_qp_ctx->p_send_buf, 0, MAD_BLOCK_SIZE);

	status = IB_SUCCESS;
	OSM_LOG(p_log, OSM_LOG_DEBUG, "Initialized QP:0x%X in VAPI Mode\n",
		p_qp_ctx->qp_bind_hndl.qp_id);

	OSM_LOG(p_log, OSM_LOG_DEBUG, "Binding to IB_MGT SMI\n");

	/* we also need a QP0 handle for sending packets */
	mgt_ret = IB_MGT_get_handle(hca_id, port_num, IB_MGT_SMI,
				    &(p_qp_ctx->ib_mgt_qp0_handle));
	if (IB_MGT_OK != mgt_ret) {
		OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 0115: "
			"Error obtaining IB_MGT handle to SMI\n");
		status = IB_ERROR;
		goto Exit;
	}

Exit:
	OSM_LOG_EXIT(p_log);
	return status;
}

/*
 * Close the QP
 */
void
osmt_unbind_inform_qp(IN osmtest_t * const p_osmt, IN osmt_qp_ctx_t * p_qp_ctx)
{
	osm_log_t *p_log = &p_osmt->log;

	OSM_LOG_ENTER(p_log);

	osmt_mtl_mad_cleanup(&p_qp_ctx->qp_bind_hndl);

	IB_MGT_release_handle(p_qp_ctx->ib_mgt_qp0_handle);

	OSM_LOG(p_log, OSM_LOG_DEBUG, "Unbind QP handles\n");
	OSM_LOG_EXIT(&p_osmt->log);
}

/*
 * Register/Unregister to receive the given InformInfo
 *
 * Uses the qp context to send the inform info mad.
 * Wait for GetResp(InformInfoResp)
 *
 */
ib_api_status_t
osmt_reg_unreg_inform_info(IN osmtest_t * p_osmt,
			   IN osmt_qp_ctx_t * p_qp_ctx,
			   IN ib_inform_info_t * p_inform_info,
			   IN uint8_t reg_flag)
{
	ib_sa_mad_t *p_sa_mad = (ib_sa_mad_t *) (p_qp_ctx->p_send_buf);
	ib_inform_info_t *p_ii = ib_sa_mad_get_payload_ptr(p_sa_mad);	/*  SA Payload */
	VAPI_ret_t vapi_ret;
	VAPI_wc_desc_t wc_desc;
	VAPI_ud_av_hndl_t avh;
	static VAPI_wr_id_t wrid = 16198;
	osm_log_t *p_log = &p_osmt->log;
	ib_api_status_t status = IB_SUCCESS;

	OSM_LOG_ENTER(&p_osmt->log);

	/* init the MAD */
	ib_mad_init_new((ib_mad_t *) p_sa_mad,
			IB_MCLASS_SUBN_ADM,
			(uint8_t) 2,
			IB_MAD_METHOD_SET, cl_hton64(wrid), (ib_net16_t) 0, 0);
	wrid++;
	p_sa_mad->attr_id = IB_MAD_ATTR_INFORM_INFO;

	/* copy the reference inform info */
	memcpy(p_ii, p_inform_info, sizeof(ib_inform_info_t));

	if (reg_flag) {
		OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
			"Subscribing InformInfo: Traps from lid:0x%X to 0x%X, trap num :0x%X\n",
			p_ii->lid_range_begin, p_ii->lid_range_end,
			p_ii->g_or_v.generic.trap_num);
	} else {
		OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
			"UnSubscribing InformInfo: Traps from lid:0x%X to 0x%X\n",
			p_ii->lid_range_begin, p_ii->lid_range_end);
	}

	/* set the subscribe bit */
	if (reg_flag) {
		p_ii->subscribe = 1;
	} else {
		p_ii->subscribe = 0;
		/*
		 * we need to set the QPN on the mad if we unsubscribe:
		 * o13-2.1.1 - QPN Field need to be set when unsubscribing.
		 */
		ib_inform_info_set_qpn(p_ii,
				       cl_hton32(p_qp_ctx->qp_bind_hndl.qp_id.
						 qp_num));
	}

	osm_dump_inform_info(&p_osmt->log, p_ii, OSM_LOG_DEBUG);

	/* --------------------- PREP ------------------------- */
	if (osmt_mtl_mad_post_recv_bufs(&p_qp_ctx->qp_bind_hndl, p_qp_ctx->p_recv_buf, 1,	/*  but we need only one mad at a time */
					GRH_LEN + MAD_BLOCK_SIZE, wrid) != 1) {
		OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 0120: "
			"Error posting recv bufs\n");
		status = IB_ERROR;
		goto Exit;
	}
	OSM_LOG(p_log, OSM_LOG_DEBUG, "Posted recv bufs\n");

	vapi_ret =
	    osmt_mtl_create_av(&p_qp_ctx->qp_bind_hndl,
			       p_osmt->local_port.sm_lid, &avh);
	if (vapi_ret != VAPI_OK) {
		OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 0121: "
			"Error Preparing AVH (%s)\n",
			VAPI_strerror_sym(vapi_ret));
		status = IB_ERROR;
		goto Exit;
	}
	OSM_LOG(p_log, OSM_LOG_DEBUG, "Prepared AVH\n");

	if (osm_log_is_active(p_log, OSM_LOG_DEBUG)) {
		osm_dump_sa_mad(p_log, (ib_sa_mad_t *) (p_qp_ctx->p_send_buf),
				OSM_LOG_DEBUG);
#if 0
		for (i = 56; i < 253; i++) {
			if (i % 8 == 0) {
				printf("\n %d : ", i);
			}
			printf("0x%02X ", p_qp_ctx->p_send_buf[i]);
		}
#endif
		printf("\n");
	}

	/* --------------------- SEND ------------------------- */
	vapi_ret = osmt_mtl_mad_send(&p_qp_ctx->qp_bind_hndl, wrid, p_qp_ctx->p_send_buf, 1,	/*  SA is QP1 */
				     0,	/*  SL is 0 */
				     OSMT_MTL_REVERSE_QP1_WELL_KNOWN_Q_KEY,
				     avh);
	if (vapi_ret != VAPI_OK) {
		OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 0122: "
			"Error sending mad (%s)\n",
			VAPI_strerror_sym(vapi_ret));
		status = IB_ERROR;
		goto Exit;
	}

	vapi_ret = osmt_mtl_mad_poll4cqe(p_qp_ctx->qp_bind_hndl.hca_hndl,
					 p_qp_ctx->qp_bind_hndl.sq_cq_hndl,
					 &wc_desc, 20, 10000, NULL);
	if (vapi_ret != VAPI_OK) {
		OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 0123: "
			"Error getting send completion (%s)\n",
			VAPI_strerror_sym(vapi_ret));
		status = IB_ERROR;
		goto Exit;
	}

	if (wc_desc.status != VAPI_SUCCESS) {
		OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 0124: "
			"Error on send completion (%s) (%d)\n",
			VAPI_strerror_sym(wc_desc.status), wc_desc.status);
		status = IB_ERROR;
		goto Exit;
	}
	OSM_LOG(p_log, OSM_LOG_DEBUG, "Sent MAD\n");

	/* --------------------- RECV ------------------------- */
	vapi_ret = osmt_mtl_mad_poll4cqe(p_qp_ctx->qp_bind_hndl.hca_hndl,
					 p_qp_ctx->qp_bind_hndl.rq_cq_hndl,
					 &wc_desc, 20, 10000, &avh);
	if (vapi_ret != VAPI_SUCCESS) {
		if (vapi_ret == VAPI_CQ_EMPTY) {
			status = IB_TIMEOUT;
		} else {
			OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 0125: "
				"Error receiving mad (%s)\n",
				VAPI_strerror_sym(vapi_ret));
			status = IB_ERROR;
		}
		goto Exit;
	}

	/* check to see if successful - by examination of the subscribe bit */
	p_sa_mad = (ib_sa_mad_t *) (p_qp_ctx->p_recv_buf + GRH_LEN);

	if (p_sa_mad->status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "Remote error = %s\n",
			ib_get_mad_status_str((ib_mad_t *) p_sa_mad));
		status = IB_REMOTE_ERROR;
		goto Exit;
	}

	if (p_sa_mad->method != IB_MAD_METHOD_GET_RESP) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
			"Expected IB_MAD_METHOD_GET_RESP but got:(%X)\n",
			p_sa_mad->method);
		status = IB_REMOTE_ERROR;
		goto Exit;
	}

	if (p_sa_mad->attr_id != IB_MAD_ATTR_INFORM_INFO) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
			"Expected IB_MAD_ATTR_INFORM_INFO but got:(%X)\n",
			cl_ntoh16(p_sa_mad->attr_id));
		status = IB_REMOTE_ERROR;
		goto Exit;
	}

	p_ii = ib_sa_mad_get_payload_ptr(p_sa_mad);
	if (!p_ii->subscribe) {
		OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 0126: "
			"Subscribe/Unsubscribe Failed\n");
		status = IB_REMOTE_ERROR;
	}

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return status;
}

/*
 * Send a trap (Subn LID Route) Trap(Notice) through the regular
 * connection QP connection (targeted at QP0)
 *
 * Wait for the trap repress
 */
ib_api_status_t
osmt_send_trap_wait_for_forward(IN osmtest_t * const p_osmt,
				IN osmt_qp_ctx_t * p_qp_ctx)
{
	ib_smp_t *p_smp = (ib_smp_t *) (p_qp_ctx->p_send_buf);
	ib_mad_notice_attr_t *p_ntc = ib_smp_get_payload_ptr(p_smp);
	ib_sa_mad_t *p_sa_mad;
	IB_MGT_ret_t mgt_res;
	VAPI_ret_t vapi_ret;
	VAPI_wc_desc_t wc_desc;
	VAPI_ud_av_hndl_t avh;
	IB_ud_av_t av;
	static VAPI_wr_id_t wrid = 2222;
	osm_log_t *p_log = &p_osmt->log;
	ib_api_status_t status = IB_SUCCESS;

	OSM_LOG_ENTER(p_log);

	OSM_LOG(p_log, OSM_LOG_INFO,
		"Sending Traps to QP0 of SA LID:0x%X\n",
		p_osmt->local_port.sm_lid);

	/* init the MAD */
	memset(p_smp, 0, sizeof(ib_smp_t));
	ib_mad_init_new((ib_mad_t *) p_smp,
			IB_MCLASS_SUBN_LID,
			(uint8_t) 2,
			IB_MAD_METHOD_TRAP, cl_hton64(wrid), (ib_net16_t) 0, 0);

	wrid++;
	p_smp->attr_id = IB_MAD_ATTR_NOTICE;

	/* prepare the notice */
	p_ntc->generic_type = 0x82;	/*  generic, type = 2 */
	ib_notice_set_prod_type_ho(p_ntc, 1);
	p_ntc->g_or_v.generic.trap_num = cl_hton16(0x26);
	p_ntc->issuer_lid = cl_hton16(2);

	/* --------------------- PREP ------------------------- */
	if (osmt_mtl_mad_post_recv_bufs(&p_qp_ctx->qp_bind_hndl, p_qp_ctx->p_recv_buf, 1,	/*  we need to receive both trap repress and report */
					GRH_LEN + MAD_BLOCK_SIZE, wrid) != 1) {
		OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 0127: "
			"Error posting recv bufs\n");
		status = IB_ERROR;
		goto Exit;
	}
	OSM_LOG(p_log, OSM_LOG_DEBUG, "Posted recv bufs\n");

	av.dlid = p_osmt->local_port.sm_lid;
	av.grh_flag = FALSE;

	/*  EZ: returned in HACK: use constants */
	av.static_rate = 0;	/*  p_mad_addr->static_rate; */
	av.src_path_bits = 1;	/*  p_mad_addr->path_bits; */
	av.sl = 0;		/*  p_mad_addr->addr_type.gsi.service_level; */

	OSM_LOG(p_log, OSM_LOG_DEBUG,
		"av.dlid 0x%X, av.static_rate %d, av.path_bits %d\n",
		cl_ntoh16(av.dlid), av.static_rate, av.src_path_bits);

	/* send it */
	mgt_res = IB_MGT_send_mad(p_qp_ctx->ib_mgt_qp0_handle, p_smp,	/*  actual payload */
				  &av,	/*  address vector */
				  wrid,	/*  casting the mad wrapper pointer for err cb */
				  p_osmt->opt.transaction_timeout);
	if (mgt_res != IB_MGT_OK) {
		OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 0128: "
			"Error sending mad (%d)\n", mgt_res);
		status = IB_ERROR;
		goto Exit;
	}

	vapi_ret =
	    osmt_mtl_create_av(&p_qp_ctx->qp_bind_hndl,
			       p_osmt->local_port.sm_lid, &avh);
	if (vapi_ret != VAPI_OK) {
		OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 0129: "
			"Error Preparing AVH (%s)\n",
			VAPI_strerror_sym(vapi_ret));
		status = IB_ERROR;
		goto Exit;
	}
	OSM_LOG(p_log, OSM_LOG_DEBUG, "Prepared AVH\n");

	OSM_LOG(p_log, OSM_LOG_DEBUG, "Trap MAD Sent\n");

	/* --------------------- RECV ------------------------- */
	vapi_ret = osmt_mtl_mad_poll4cqe(p_qp_ctx->qp_bind_hndl.hca_hndl,
					 p_qp_ctx->qp_bind_hndl.rq_cq_hndl,
					 &wc_desc, 200, 10000, &avh);
	if (vapi_ret != VAPI_SUCCESS) {
		if (vapi_ret == VAPI_CQ_EMPTY) {
			OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 0130: "
				"Timeout receiving mad (%s)\n",
				VAPI_strerror_sym(vapi_ret));
			status = IB_TIMEOUT;
		} else {
			OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 0131: "
				"Error receiving mad (%s)\n",
				VAPI_strerror_sym(vapi_ret));
			status = IB_ERROR;
		}
		goto Exit;
	}

	/* check to see if successful - by examination of the subscribe bit */
	p_sa_mad = (ib_sa_mad_t *) (p_qp_ctx->p_recv_buf + GRH_LEN);

	if (p_sa_mad->method == IB_MAD_METHOD_REPORT) {
		if (p_sa_mad->attr_id == IB_MAD_ATTR_NOTICE) {
			OSM_LOG(p_log, OSM_LOG_INFO, "Received the Report!\n");
			status = IB_SUCCESS;
		} else {
			OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 1020"
				"Did not receive a Report(Notice) but attr:%d\n",
				cl_ntoh16(p_sa_mad->attr_id));
			status = IB_ERROR;
		}
	} else {
		OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 1020"
			"Received an Unexpected Method:%d\n", p_smp->method);
		status = IB_ERROR;
	}

Exit:
	OSM_LOG_EXIT(p_log);
	return status;
}

/*
 * Wait for a trap on QPn
 *
 */
ib_api_status_t
osmt_trap_wait(IN osmtest_t * const p_osmt, IN osmt_qp_ctx_t * p_qp_ctx)
{
	ib_smp_t *p_smp = (ib_smp_t *) (p_qp_ctx->p_send_buf);
	ib_sa_mad_t *p_sa_mad;
	VAPI_ret_t vapi_ret;
	VAPI_wc_desc_t wc_desc;
	osm_log_t *p_log = &p_osmt->log;
	ib_api_status_t status = IB_SUCCESS;

	OSM_LOG_ENTER(p_log);

	OSM_LOG(p_log, OSM_LOG_INFO,
		"Waiting for Traps under QP:0x%X of SA LID:0x%X\n",
		cl_ntoh16(p_osmt->local_port.sm_lid));

	/* --------------------- RECV ------------------------- */
	vapi_ret = osmt_mtl_mad_poll4cqe(p_qp_ctx->qp_bind_hndl.hca_hndl,
					 p_qp_ctx->qp_bind_hndl.rq_cq_hndl,
					 &wc_desc,
					 // 200,
					 p_osmt->opt.wait_time * 100,
					 10000, NULL);
	if (vapi_ret != VAPI_SUCCESS) {
		if (vapi_ret == VAPI_CQ_EMPTY) {
			OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 0130: "
				"Timeout receiving mad (%s)\n",
				VAPI_strerror_sym(vapi_ret));
			status = IB_TIMEOUT;
		} else {
			OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 0131: "
				"Error receiving mad (%s)\n",
				VAPI_strerror_sym(vapi_ret));
			status = IB_ERROR;
		}
		goto Exit;
	}

	/* check to see if successful - by examination of the subscribe bit */
	p_sa_mad = (ib_sa_mad_t *) (p_qp_ctx->p_recv_buf + GRH_LEN);

	if (p_sa_mad->method == IB_MAD_METHOD_REPORT) {
		if (p_sa_mad->attr_id == IB_MAD_ATTR_NOTICE) {
			OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
				"Received the Report!\n");
			status = IB_SUCCESS;
		} else {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 1020"
				"Did not receive a Report(Notice) but attr:%d\n",
				cl_ntoh16(p_sa_mad->attr_id));
			status = IB_ERROR;
		}
	} else {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 1020"
			"Received an Unexpected Method:%d\n", p_smp->method);
		status = IB_ERROR;
	}

Exit:
	OSM_LOG_EXIT(p_log);
	return status;
}

/*
 * Initialize an inform info attribute:
 * Catch all traps in the lid range of the p_osmt
 *
 */
ib_api_status_t
osmt_init_inform_info(IN osmtest_t * const p_osmt, OUT ib_inform_info_t * p_ii)
{

	memset(p_ii, 0, sizeof(ib_inform_info_t));
	/*  p_ii->lid_range_begin = cl_hton16(1); */
	p_ii->lid_range_begin = 0xFFFF;
	p_ii->lid_range_end = cl_hton16(p_osmt->max_lid);
	p_ii->is_generic = 1;	/*  have to choose */
	p_ii->trap_type = 0xFFFF;	/*  ALL */
	p_ii->g_or_v.generic.trap_num = 0xFFFF;	/*  ALL */
	p_ii->g_or_v.generic.node_type_lsb = 0xFFFF;	/*  ALL */
	p_ii->g_or_v.generic.node_type_msb = 0xFF;	/*  ALL */
	return IB_SUCCESS;
}

ib_api_status_t
osmt_init_inform_info_by_trap(IN osmtest_t * const p_osmt,
			      IN ib_net16_t trap_num,
			      OUT ib_inform_info_t * p_ii)
{

	memset(p_ii, 0, sizeof(ib_inform_info_t));
	/*  p_ii->lid_range_begin = cl_hton16(1); */
	p_ii->lid_range_begin = 0xFFFF;
	p_ii->lid_range_end = cl_hton16(p_osmt->max_lid);
	p_ii->is_generic = 1;	/*  have to choose */
	p_ii->trap_type = 0xFFFF;	/*  ALL */
	p_ii->g_or_v.generic.trap_num = trap_num;	/*  ALL */
	p_ii->g_or_v.generic.node_type_lsb = 0xFFFF;	/*  ALL */
	p_ii->g_or_v.generic.node_type_msb = 0xFF;	/*  ALL */
	return IB_SUCCESS;
}

/*
 * Run a complete inform info test flow:
 * - try to unregister inform info (should fail)
 * - register an inform info
 * - try to unregister inform info (should succeed)
 * - register an inform info
 * - send a trap - sleep
 * - check that a Report(Notice) arrived that match the sent one
 *
 */
ib_api_status_t osmt_run_inform_info_flow(IN osmtest_t * const p_osmt)
{
	ib_inform_info_t inform_info;
	ib_api_status_t status;
	osmt_qp_ctx_t qp_ctx;

	OSM_LOG_ENTER(&p_osmt->log);

	/* bind the QP */
	status = osmt_bind_inform_qp(p_osmt, &qp_ctx);
	if (status != IB_SUCCESS) {
		goto Exit;
	}

	/* init the inform info */
	osmt_init_inform_info(p_osmt, &inform_info);

	/* first try to unsubscribe */
	status = osmt_reg_unreg_inform_info(p_osmt, &qp_ctx, &inform_info, 0);
	/* WAS IB_REMOTE_ERROR */
	if (status != IB_REMOTE_ERROR) {
		if (status != IB_SUCCESS) {
			OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
				"Error during UnSubscribe: (%s)\n",
				ib_get_err_str(status));
			goto Exit;
		} else {
			OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
				"Expected Failure to UnSubscribe non existing InformInfo\n");
			status = IB_ERROR;
			goto Exit;
		}
	}

	/* send the inform info registration */
	status = osmt_reg_unreg_inform_info(p_osmt, &qp_ctx, &inform_info, 1);
	if (status != IB_SUCCESS) {
		goto Exit;
	}

	/* send a trap through QP0 and wait on QPN */
	status = osmt_send_trap_wait_for_forward(p_osmt, &qp_ctx);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
			"Error during Send Trap and Wait For Report: (%s)\n",
			ib_get_err_str(status));
		goto Exit;
	}

	/* try to unsubscribe for cleanup */
	status = osmt_reg_unreg_inform_info(p_osmt, &qp_ctx, &inform_info, 0);

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
			"Error during UnSubscribe: (%s)\n",
			ib_get_err_str(status));
		goto Exit;
	} else {
		if (status == IB_REMOTE_ERROR) {
			OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
				"Remote Error during UnSubscribe\n");
			status = IB_ERROR;
			goto Exit;
		}
	}

Exit:
	osmt_unbind_inform_qp(p_osmt, &qp_ctx);
	OSM_LOG_EXIT(&p_osmt->log);
	return status;
}

/*
 * Run a complete inform info test flow:
 * - try to unregister inform info (should fail)
 * - register an inform info
 * - try to unregister inform info (should succeed)
 * - register an inform info
 * - send a trap - sleep
 * - check that a Report(Notice) arrived that match the sent one
 *
 */
ib_api_status_t osmt_run_trap64_65_flow(IN osmtest_t * const p_osmt)
{
	ib_inform_info_t inform_info;
	ib_api_status_t status;
	osmt_qp_ctx_t qp_ctx;

	OSM_LOG_ENTER(&p_osmt->log);

	/* bind the QP */
	status = osmt_bind_inform_qp(p_osmt, &qp_ctx);
	if (status != IB_SUCCESS) {
		goto Exit;
	}

	/* init the inform info */
	osmt_init_inform_info_by_trap(p_osmt, cl_hton16(64), &inform_info);

	/* send the inform info registration */
	status = osmt_reg_unreg_inform_info(p_osmt, &qp_ctx, &inform_info, 1);
	if (status != IB_SUCCESS) {
		goto Exit;
	}

  /*--------------------- PREP -------------------------*/
	if (osmt_mtl_mad_post_recv_bufs(&qp_ctx.qp_bind_hndl, qp_ctx.p_recv_buf, 1,	/* we need to receive the report */
					GRH_LEN + MAD_BLOCK_SIZE, 1) != 1) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0127: "
			"Error posting recv bufs for trap 64\n");
		status = IB_ERROR;
		goto Exit;
	}

	OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG, "Posted recv bufs for trap 64\n");

	/* init the inform info */
	osmt_init_inform_info_by_trap(p_osmt, cl_hton16(65), &inform_info);

	/* send the inform info registration */
	status = osmt_reg_unreg_inform_info(p_osmt, &qp_ctx, &inform_info, 1);
	if (status != IB_SUCCESS) {
		goto Exit;
	}

  /*--------------------- PREP -------------------------*/
	if (osmt_mtl_mad_post_recv_bufs(&qp_ctx.qp_bind_hndl, qp_ctx.p_recv_buf, 1,	/* we need to reveive the report */
					GRH_LEN + MAD_BLOCK_SIZE, 1) != 1) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0127: "
			"Error posting recv bufs for trap 65\n");
		status = IB_ERROR;
		goto Exit;
	}
	OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG, "Posted recv bufs for trap 65\n");

	/* Sleep for x seconds in order to allow external script trap generation */
#if 0
	sleep(p_osmt->opt.wait_time);
#endif

	/* wait for a trap on QPN */
	status = osmt_trap_wait(p_osmt, &qp_ctx);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
			"Error during Send Trap and Wait For Report: (%s)\n",
			ib_get_err_str(status));
		goto Exit;
	}

	/* try to unsubscribe for cleanup */
	status = osmt_reg_unreg_inform_info(p_osmt, &qp_ctx, &inform_info, 0);

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
			"Error during UnSubscribe: (%s)\n",
			ib_get_err_str(status));
		goto Exit;
	}

Exit:
	osmt_unbind_inform_qp(p_osmt, &qp_ctx);
	OSM_LOG_EXIT(&p_osmt->log);
	return status;
}

#endif				/*  OSM_VENDOR_INTF_MTL */
