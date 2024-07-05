/*
 * Copyright (c) 2024, Broadcom. All rights reserved.  The term
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
 */

#ifndef __BNXT_RE_VERBS_H__
#define __BNXT_RE_VERBS_H__

#include <sys/mman.h>

#include <netinet/in.h>
#include <infiniband/verbs.h>

#include <assert.h>
#include <errno.h>
#include <malloc.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int bnxt_re_query_device(struct ibv_context *ibvctx,
			 struct ibv_device_attr *dev_attr);

int bnxt_re_query_device_compat(struct ibv_context *ibvctx,
				struct ibv_device_attr *dev_attr);

int bnxt_re_query_port(struct ibv_context *, uint8_t, struct ibv_port_attr *);

struct ibv_pd *bnxt_re_alloc_pd(struct ibv_context *);
int bnxt_re_free_pd(struct ibv_pd *);

typedef struct ibv_mr VERBS_MR;

struct ibv_mr *bnxt_re_reg_mr(struct ibv_pd *, void *, size_t,
			      int ibv_access_flags);
int bnxt_re_dereg_mr(VERBS_MR*);

struct ibv_cq *bnxt_re_create_cq(struct ibv_context *, int,
				 struct ibv_comp_channel *, int);
int bnxt_re_resize_cq(struct ibv_cq *, int);
int bnxt_re_destroy_cq(struct ibv_cq *);
int bnxt_re_poll_cq(struct ibv_cq *, int, struct ibv_wc *);
void bnxt_re_cq_event(struct ibv_cq *);
int bnxt_re_arm_cq(struct ibv_cq *, int);

struct ibv_qp *bnxt_re_create_qp(struct ibv_pd *, struct ibv_qp_init_attr *);
int bnxt_re_modify_qp(struct ibv_qp *, struct ibv_qp_attr *,
		      int ibv_qp_attr_mask);
int bnxt_re_query_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr,
		     int attr_mask, struct ibv_qp_init_attr *init_attr);
int bnxt_re_destroy_qp(struct ibv_qp *);
int bnxt_re_post_send(struct ibv_qp *, struct ibv_send_wr *,
		      struct ibv_send_wr **);
int bnxt_re_post_recv(struct ibv_qp *, struct ibv_recv_wr *,
		      struct ibv_recv_wr **);

struct ibv_srq *bnxt_re_create_srq(struct ibv_pd *,
				   struct ibv_srq_init_attr *);
int bnxt_re_modify_srq(struct ibv_srq *, struct ibv_srq_attr *, int);
int bnxt_re_destroy_srq(struct ibv_srq *);
int bnxt_re_query_srq(struct ibv_srq *ibsrq, struct ibv_srq_attr *attr);
int bnxt_re_post_srq_recv(struct ibv_srq *, struct ibv_recv_wr *,
			  struct ibv_recv_wr **);

struct ibv_ah *bnxt_re_create_ah(struct ibv_pd *, struct ibv_ah_attr *);
int bnxt_re_destroy_ah(struct ibv_ah *);

int bnxt_re_attach_mcast(struct ibv_qp *, const union ibv_gid *, uint16_t);
int bnxt_re_detach_mcast(struct ibv_qp *, const union ibv_gid *, uint16_t);

void bnxt_re_async_event(struct ibv_async_event *event);

struct bnxt_re_work_compl {
	struct bnxt_re_list_node cnode;
	struct ibv_wc wc;
};

static inline uint8_t bnxt_re_get_psne_size(struct bnxt_re_context *cntx)
{
	return (BNXT_RE_HW_RETX(cntx)) ? sizeof(struct bnxt_re_msns) :
					      (cntx->cctx->chip_is_gen_p5_thor2) ?
					      sizeof(struct bnxt_re_psns_ext) :
					      sizeof(struct bnxt_re_psns);
}

static inline uint32_t bnxt_re_get_npsn(uint8_t mode, uint32_t nwr,
					uint32_t slots)
{
	return mode == BNXT_RE_WQE_MODE_VARIABLE ? slots : nwr;
}

static inline bool bnxt_re_is_mqp_ex_supported(struct bnxt_re_context *cntx)
{
	return cntx->comp_mask & BNXT_RE_COMP_MASK_UCNTX_MQP_EX_SUPPORTED;
}

static inline bool can_request_ppp(struct bnxt_re_qp *re_qp,
				   struct ibv_qp_attr *attr, int attr_mask)
{
	struct bnxt_re_context *cntx;
	struct bnxt_re_qp *qp;
	bool request = false;

	qp = re_qp;
	cntx = qp->cntx;
	if (!qp->push_st_en && cntx->udpi.wcdpi && (attr_mask & IBV_QP_STATE) &&
	    qp->qpst == IBV_QPS_RESET && attr->qp_state == IBV_QPS_INIT) {
		request = true;
	}
	return request;
}

static inline uint64_t bnxt_re_update_msn_tbl(uint32_t st_idx, uint32_t npsn, uint32_t start_psn)
{
	return htole64((((uint64_t)(st_idx) << BNXT_RE_SQ_MSN_SEARCH_START_IDX_SHIFT) &
		 BNXT_RE_SQ_MSN_SEARCH_START_IDX_MASK) |
		 (((uint64_t)(npsn) << BNXT_RE_SQ_MSN_SEARCH_NEXT_PSN_SHIFT) &
		 BNXT_RE_SQ_MSN_SEARCH_NEXT_PSN_MASK) |
		 (((start_psn) << BNXT_RE_SQ_MSN_SEARCH_START_PSN_SHIFT) &
		 BNXT_RE_SQ_MSN_SEARCH_START_PSN_MASK));
}

static inline int ibv_cmd_modify_qp_compat(struct ibv_qp *ibvqp,
					   struct ibv_qp_attr *attr,
					   int attr_mask, bool issue_mqp_ex,
					   struct bnxt_re_modify_ex_req *mreq,
					   struct bnxt_re_modify_ex_resp *mresp)
{
	int rc;

	if (issue_mqp_ex) {
		struct bnxt_re_modify_ex_resp *resp;
		struct bnxt_re_modify_ex_req *req;

		req = mreq;
		resp = mresp;
		rc = ibv_cmd_modify_qp_ex(ibvqp, attr, attr_mask, &req->cmd,
					  sizeof(req->cmd), sizeof(*req),
					  &resp->resp, sizeof(resp->resp),
					  sizeof(*resp));
	} else {
		struct ibv_modify_qp cmd = {};

		rc = ibv_cmd_modify_qp(ibvqp, attr, attr_mask,
				       &cmd, sizeof(cmd));
	}
	return rc;
}

#define bnxt_re_is_zero_len_pkt(len, opcd)	(len == 0)
#define BNXT_RE_MSN_IDX(m) (((m) & BNXT_RE_SQ_MSN_SEARCH_START_IDX_MASK) >> \
		BNXT_RE_SQ_MSN_SEARCH_START_IDX_SHIFT)
#define BNXT_RE_MSN_NPSN(m) (((m) & BNXT_RE_SQ_MSN_SEARCH_NEXT_PSN_MASK) >> \
		BNXT_RE_SQ_MSN_SEARCH_NEXT_PSN_SHIFT)
#define BNXT_RE_MSN_SPSN(m) (((m) & BNXT_RE_SQ_MSN_SEARCH_START_PSN_MASK) >> \
		BNXT_RE_SQ_MSN_SEARCH_START_PSN_SHIFT)

#endif /* __BNXT_RE_VERBS_H__ */
