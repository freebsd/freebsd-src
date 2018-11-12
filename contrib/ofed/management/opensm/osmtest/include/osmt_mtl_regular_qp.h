/*
 * Copyright (c) 2001-2005 Mellanox Technologies LTD. All rights reserved.
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

/*
 *    mad.h -
 *      Header file for common special QP resources creation code.
 *
 *  Creation date:
 *
 *  Version: osmt_mtl_regular_qp.h,v 1.2 2003/03/20 16:05:10 eitan
 *
 *  Authors:
 *    Elazar Raab
 *
 *  Changes:
 */

#ifndef H_MAD_H
#define H_MAD_H

#include <vapi.h>
#include <evapi.h>
#include <vapi_common.h>
#include <ib_defs.h>

#if defined(MAD_IN) || defined(MAD_OUT)
#error MACROS MAD_IN and MAD_OUT are in use, do not override
#endif
#define MAD_IN
#define MAD_OUT

/* HCA Constants */
#define HCA_ID     "mt21108_pci0"
#define GRH_LEN 40
#define KNOWN_QP1_QKEY 0x80010000

#define MAX_OUTS_SQ   2		/* Max. buffers posted for requests in SQ */
#define MAX_OUTS_RQ   5		/* Max. buffers posted for responses in RQ */

#define MAX_POLL_CNT    300
#define POLL_SLEEP      1	/* for usleep */

#define MAD_SIZE  		256	/* MADs are always 256B */
#define MAD_ATTR_OFFSET 16
#define MAD_TID_OFFSET  8

/* Verbs SQP resources handles */
typedef struct {
	VAPI_hca_id_t hca_id;	/*id of HCA */
	u_int8_t port_num;	/* the port num to use */
	VAPI_hca_hndl_t hca_hndl;	/*handle of HCA */
	VAPI_qp_hndl_t qp_hndl;	/*handle of QP I use */
	VAPI_mr_hndl_t mr_hndl;	/*handle of memory region */
	VAPI_cq_hndl_t rq_cq_hndl, sq_cq_hndl;	/*handle of send & receive completion Queues */
	VAPI_pd_hndl_t pd_hndl;	/*handle of Partition Domain */
	/* VAPI_ud_av_hndl_t   av_hndl; */
	IB_lid_t slid;
	 /*LID*/ void *buf_ptr;	/*mem buffer for outstanding pkts */
	MT_size_t buf_size;	/*size of mem buffer for outstanding pkts */

	u_int32_t max_outs_sq;	/*max # of outstanding pkts in send queue */
	u_int32_t max_outs_rq;	/*max # of outstanding pkts in receive queue */

	IB_rkey_t l_key;	/*my l_key for memory regions */
	VAPI_qkey_t qkey;	/*my qkey */

	EVAPI_compl_handler_hndl_t rq_cq_eventh, sq_cq_eventh;	/* event handlers for polling */

	bool is_sqp;		/* relate to union below - my QP */
	union {
		VAPI_special_qp_t sqp_type;
		VAPI_qp_num_t qp_num;
	} qp_id;
	void *wait_q;
} osmt_mtl_mad_res_t;

/* init an osmt_mtl_mad_res_t with all resources initialized (use functions below) */
VAPI_ret_t osmt_mtl_init(osmt_mtl_mad_res_t * res	/*pointer to res (resources) struct */
    );
VAPI_ret_t osmt_mtl_init_opened_hca(osmt_mtl_mad_res_t * res	/*pointer to res (resources) struct */
    );

/* Cleanup all resources of (which are valid) in res */
VAPI_ret_t osmt_mtl_mad_cleanup(osmt_mtl_mad_res_t * res	/*pointer to res (resources) struct */
    );

/* create CQs and QP as given in res->is_sqp (if TRUE, get special QP) */
VAPI_ret_t osmt_mtl_get_qp_resources(osmt_mtl_mad_res_t * res	/*pointer to res (resources) struct */
    );

/* move QP to RTS state */
VAPI_ret_t osmt_mtl_mad_qp_init(osmt_mtl_mad_res_t * res	/*max number of outstanding packets allowed in send queue */
    );

/* create and register res->buf_ptr */
VAPI_ret_t osmt_mtl_mad_create_mr(osmt_mtl_mad_res_t * res	/*pointer to res (resources) struct */
    );

VAPI_ret_t osmt_mtl_create_av(osmt_mtl_mad_res_t * res,	/* pointer to res (resources) struct */
			      int16_t dlid,	/*destination lid */
			      VAPI_ud_av_hndl_t * avh_p	/* address vectr handle to update */
    );

/* Send MAD to given dest QP*/
VAPI_ret_t osmt_mtl_mad_send(osmt_mtl_mad_res_t * res,	/*pointer to res (resources) struct */
			     VAPI_wr_id_t id,	/*wqe ID */
			     void *mad,	/*mad buffer to send */
			     VAPI_qp_num_t dest_qp,	/*destination QP */
			     IB_sl_t sl,	/*Service Level */
			     u_int32_t dest_qkey,	/*Destination QP KEY */
			     VAPI_ud_av_hndl_t avh	/* address vectr handle to use */
    );

/* post buffers to RQ. returns num of buffers actually posted */
int osmt_mtl_mad_post_recv_bufs(osmt_mtl_mad_res_t * res,	/*pointer to res (resources) struct */
				void *buf_array,	/*array of receive buffers */
				u_int32_t num_o_bufs,	/*number of receive buffers */
				u_int32_t size,	/* size of expected receive packet - MAD */
				VAPI_wr_id_t start_id	/* start id for receive buffers */
    );

/* Poll given CQ for completion max_poll times (POLL_SLEEP [usec] delays). result in wc_desc_p. */
VAPI_ret_t osmt_mtl_mad_poll4cqe(VAPI_hca_hndl_t hca,	/*handle for HCA */
				 VAPI_cq_hndl_t cq,	/*handle for Completion Queue - Rcv/Send  */
				 VAPI_wc_desc_t * wc_desc_p,	/*handle of cqe */
				 u_int32_t max_poll,	/*number of polling iterations */
				 u_int32_t poll_sleep,	/*timeout for each polling    */
				 VAPI_ud_av_hndl_t * avh_p	/* address vectopr handle to cleanup */
    );

#endif
