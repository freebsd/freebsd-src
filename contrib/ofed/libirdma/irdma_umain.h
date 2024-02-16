/*-
 * SPDX-License-Identifier: GPL-2.0 or Linux-OpenIB
 *
 * Copyright (C) 2019 - 2022 Intel Corporation
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

#ifndef IRDMA_UMAIN_H
#define IRDMA_UMAIN_H

#include <sys/queue.h>
#include <infiniband/verbs.h>
#include <infiniband/driver.h>

#include "osdep.h"
#include "irdma.h"
#include "irdma_defs.h"
#include "i40iw_hw.h"
#include "irdma_user.h"

#define PFX	"libirdma-"

#define IRDMA_BASE_PUSH_PAGE		1
#define IRDMA_U_MINCQ_SIZE		4
#define IRDMA_DB_SHADOW_AREA_SIZE	64
#define IRDMA_DB_CQ_OFFSET		64

LIST_HEAD(list_head, irdma_cq_buf);
LIST_HEAD(list_head_cmpl, irdma_cmpl_gen);

enum irdma_supported_wc_flags_ex {
	IRDMA_STANDARD_WC_FLAGS_EX = IBV_WC_EX_WITH_BYTE_LEN
				    | IBV_WC_EX_WITH_IMM
				    | IBV_WC_EX_WITH_QP_NUM
				    | IBV_WC_EX_WITH_SRC_QP
				    | IBV_WC_EX_WITH_SL,
};

struct irdma_udevice {
	struct verbs_device ibv_dev;
};

struct irdma_uah {
	struct ibv_ah ibv_ah;
	uint32_t ah_id;
	struct ibv_global_route grh;
};

struct irdma_upd {
	struct ibv_pd ibv_pd;
	void *arm_cq_page;
	void *arm_cq;
	uint32_t pd_id;
};

struct irdma_uvcontext {
	struct ibv_context ibv_ctx;
	struct irdma_upd *iwupd;
	struct irdma_uk_attrs uk_attrs;
	void *db;
	int abi_ver;
	bool legacy_mode:1;
	bool use_raw_attrs:1;
};

struct irdma_uqp;

struct irdma_cq_buf {
	LIST_ENTRY(irdma_cq_buf) list;
	struct irdma_cq_uk cq;
	struct verbs_mr vmr;
};

extern pthread_mutex_t sigusr1_wait_mutex;

struct verbs_cq {
	union {
		struct ibv_cq cq;
		struct ibv_cq_ex cq_ex;
	};
};

struct irdma_ucq {
	struct verbs_cq verbs_cq;
	struct verbs_mr vmr;
	struct verbs_mr vmr_shadow_area;
	pthread_spinlock_t lock;
	size_t buf_size;
	bool is_armed;
	bool skip_arm;
	bool arm_sol;
	bool skip_sol;
	int comp_vector;
	struct irdma_uqp *uqp;
	struct irdma_cq_uk cq;
	struct list_head resize_list;
	/* for extended CQ completion fields */
	struct irdma_cq_poll_info cur_cqe;
};

struct irdma_uqp {
	struct ibv_qp ibv_qp;
	struct irdma_ucq *send_cq;
	struct irdma_ucq *recv_cq;
	struct verbs_mr vmr;
	size_t buf_size;
	uint32_t irdma_drv_opt;
	pthread_spinlock_t lock;
	uint16_t sq_sig_all;
	uint16_t qperr;
	uint16_t rsvd;
	uint32_t pending_rcvs;
	uint32_t wq_size;
	struct ibv_recv_wr *pend_rx_wr;
	struct irdma_qp_uk qp;
	enum ibv_qp_type qp_type;
};

/* irdma_uverbs.c */
int irdma_uquery_device_ex(struct ibv_context *context,
			   const struct ibv_query_device_ex_input *input,
			   struct ibv_device_attr_ex *attr, size_t attr_size);
int irdma_uquery_port(struct ibv_context *context, uint8_t port,
		      struct ibv_port_attr *attr);
struct ibv_pd *irdma_ualloc_pd(struct ibv_context *context);
int irdma_ufree_pd(struct ibv_pd *pd);
int irdma_uquery_device(struct ibv_context *, struct ibv_device_attr *);
struct ibv_mr *irdma_ureg_mr(struct ibv_pd *pd, void *addr, size_t length,
			     int access);
int irdma_udereg_mr(struct ibv_mr *mr);

int irdma_urereg_mr(struct verbs_mr *mr, int flags, struct ibv_pd *pd, void *addr,
		    size_t length, int access);

struct ibv_mw *irdma_ualloc_mw(struct ibv_pd *pd, enum ibv_mw_type type);
int irdma_ubind_mw(struct ibv_qp *qp, struct ibv_mw *mw,
		   struct ibv_mw_bind *mw_bind);
int irdma_udealloc_mw(struct ibv_mw *mw);
struct ibv_cq *irdma_ucreate_cq(struct ibv_context *context, int cqe,
				struct ibv_comp_channel *channel,
				int comp_vector);
struct ibv_cq_ex *irdma_ucreate_cq_ex(struct ibv_context *context,
				      struct ibv_cq_init_attr_ex *attr_ex);
void irdma_ibvcq_ex_fill_priv_funcs(struct irdma_ucq *iwucq,
				    struct ibv_cq_init_attr_ex *attr_ex);
int irdma_uresize_cq(struct ibv_cq *cq, int cqe);
int irdma_udestroy_cq(struct ibv_cq *cq);
int irdma_upoll_cq(struct ibv_cq *cq, int entries, struct ibv_wc *entry);
int irdma_uarm_cq(struct ibv_cq *cq, int solicited);
void irdma_cq_event(struct ibv_cq *cq);
struct ibv_qp *irdma_ucreate_qp(struct ibv_pd *pd,
				struct ibv_qp_init_attr *attr);
int irdma_uquery_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr, int attr_mask,
		    struct ibv_qp_init_attr *init_attr);
int irdma_umodify_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr,
		     int attr_mask);
int irdma_udestroy_qp(struct ibv_qp *qp);
int irdma_upost_send(struct ibv_qp *ib_qp, struct ibv_send_wr *ib_wr,
		     struct ibv_send_wr **bad_wr);
int irdma_upost_recv(struct ibv_qp *ib_qp, struct ibv_recv_wr *ib_wr,
		     struct ibv_recv_wr **bad_wr);
struct ibv_ah *irdma_ucreate_ah(struct ibv_pd *ibpd, struct ibv_ah_attr *attr);
int irdma_udestroy_ah(struct ibv_ah *ibah);
int irdma_uattach_mcast(struct ibv_qp *qp, const union ibv_gid *gid,
			uint16_t lid);
int irdma_udetach_mcast(struct ibv_qp *qp, const union ibv_gid *gid,
			uint16_t lid);
void irdma_async_event(struct ibv_context *context,
		       struct ibv_async_event *event);
void irdma_set_hw_attrs(struct irdma_hw_attrs *attrs);
void *irdma_mmap(int fd, off_t offset);
void irdma_munmap(void *map);
#endif /* IRDMA_UMAIN_H */
