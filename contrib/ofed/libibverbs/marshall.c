/*
 * Copyright (c) 2005 Intel Corporation.  All rights reserved.
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
 */

#include <config.h>

#include <string.h>

#include <infiniband/marshall.h>

void ibv_copy_ah_attr_from_kern(struct ibv_ah_attr *dst,
				struct ibv_kern_ah_attr *src)
{
	memcpy(dst->grh.dgid.raw, src->grh.dgid, sizeof dst->grh.dgid);
	dst->grh.flow_label = src->grh.flow_label;
	dst->grh.sgid_index = src->grh.sgid_index;
	dst->grh.hop_limit = src->grh.hop_limit;
	dst->grh.traffic_class = src->grh.traffic_class;

	dst->dlid = src->dlid;
	dst->sl = src->sl;
	dst->src_path_bits = src->src_path_bits;
	dst->static_rate = src->static_rate;
	dst->is_global = src->is_global;
	dst->port_num = src->port_num;
}

void ibv_copy_qp_attr_from_kern(struct ibv_qp_attr *dst,
				struct ibv_kern_qp_attr *src)
{
	dst->cur_qp_state = src->cur_qp_state;
	dst->path_mtu = src->path_mtu;
	dst->path_mig_state = src->path_mig_state;
	dst->qkey = src->qkey;
	dst->rq_psn = src->rq_psn;
	dst->sq_psn = src->sq_psn;
	dst->dest_qp_num = src->dest_qp_num;
	dst->qp_access_flags = src->qp_access_flags;

	dst->cap.max_send_wr = src->max_send_wr;
	dst->cap.max_recv_wr = src->max_recv_wr;
	dst->cap.max_send_sge = src->max_send_sge;
	dst->cap.max_recv_sge = src->max_recv_sge;
	dst->cap.max_inline_data = src->max_inline_data;

	ibv_copy_ah_attr_from_kern(&dst->ah_attr, &src->ah_attr);
	ibv_copy_ah_attr_from_kern(&dst->alt_ah_attr, &src->alt_ah_attr);

	dst->pkey_index = src->pkey_index;
	dst->alt_pkey_index = src->alt_pkey_index;
	dst->en_sqd_async_notify = src->en_sqd_async_notify;
	dst->sq_draining = src->sq_draining;
	dst->max_rd_atomic = src->max_rd_atomic;
	dst->max_dest_rd_atomic = src->max_dest_rd_atomic;
	dst->min_rnr_timer = src->min_rnr_timer;
	dst->port_num = src->port_num;
	dst->timeout = src->timeout;
	dst->retry_cnt = src->retry_cnt;
	dst->rnr_retry = src->rnr_retry;
	dst->alt_port_num = src->alt_port_num;
	dst->alt_timeout = src->alt_timeout;
}

void ibv_copy_path_rec_from_kern(struct ibv_sa_path_rec *dst,
				 struct ib_user_path_rec *src)
{
	memcpy(dst->dgid.raw, src->dgid, sizeof dst->dgid);
	memcpy(dst->sgid.raw, src->sgid, sizeof dst->sgid);

	dst->dlid		= src->dlid;
	dst->slid		= src->slid;
	dst->raw_traffic	= src->raw_traffic;
	dst->flow_label		= src->flow_label;
	dst->hop_limit		= src->hop_limit;
	dst->traffic_class	= src->traffic_class;
	dst->reversible		= src->reversible;
	dst->numb_path		= src->numb_path;
	dst->pkey		= src->pkey;
	dst->sl			= src->sl;
	dst->mtu_selector	= src->mtu_selector;
	dst->mtu		= src->mtu;
	dst->rate_selector	= src->rate_selector;
	dst->rate		= src->rate;
	dst->packet_life_time	= src->packet_life_time;
	dst->preference		= src->preference;
	dst->packet_life_time_selector = src->packet_life_time_selector;
}

void ibv_copy_path_rec_to_kern(struct ib_user_path_rec *dst,
			       struct ibv_sa_path_rec *src)
{
	memcpy(dst->dgid, src->dgid.raw, sizeof src->dgid);
	memcpy(dst->sgid, src->sgid.raw, sizeof src->sgid);

	dst->dlid		= src->dlid;
	dst->slid		= src->slid;
	dst->raw_traffic	= src->raw_traffic;
	dst->flow_label		= src->flow_label;
	dst->hop_limit		= src->hop_limit;
	dst->traffic_class	= src->traffic_class;
	dst->reversible		= src->reversible;
	dst->numb_path		= src->numb_path;
	dst->pkey		= src->pkey;
	dst->sl			= src->sl;
	dst->mtu_selector	= src->mtu_selector;
	dst->mtu		= src->mtu;
	dst->rate_selector	= src->rate_selector;
	dst->rate		= src->rate;
	dst->packet_life_time	= src->packet_life_time;
	dst->preference		= src->preference;
	dst->packet_life_time_selector = src->packet_life_time_selector;
}
