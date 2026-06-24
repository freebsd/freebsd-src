/*-
 * SPDX-License-Identifier: BSD-2-Clause OR GPL-2.0
 *
 * Copyright (c) 2004 Topspin Communications.  All rights reserved.
 * Copyright (c) 2005 Voltaire, Inc.  All rights reserved.
 * Copyright (c) 2006 Intel Corporation.  All rights reserved.
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

#include <sys/cdefs.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/random.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/kref.h>
#include <linux/idr.h>
#include <linux/workqueue.h>
#include <linux/etherdevice.h>
#include <rdma/ib_pack.h>
#include <rdma/ib_cache.h>
#include <rdma/ib_user_sa.h>
#include <rdma/ib_marshall.h>
#include <rdma/ib_addr.h>
#include <rdma/opa_addr.h>
#include "sa.h"
#include "core_priv.h"

#define IB_SA_LOCAL_SVC_TIMEOUT_MIN		100
#define IB_SA_LOCAL_SVC_TIMEOUT_DEFAULT		2000
#define IB_SA_LOCAL_SVC_TIMEOUT_MAX		200000
#define IB_SA_CPI_MAX_RETRY_CNT			3
#define IB_SA_CPI_RETRY_WAIT			1000 /*msecs */

struct ib_sa_sm_ah {
	struct ib_ah        *ah;
	struct kref          ref;
	u16		     pkey_index;
	u8		     src_path_mask;
};

enum rdma_class_port_info_type {
	RDMA_CLASS_PORT_INFO_IB,
	RDMA_CLASS_PORT_INFO_OPA
};

struct rdma_class_port_info {
	enum rdma_class_port_info_type type;
	union {
		struct ib_class_port_info ib;
		struct opa_class_port_info opa;
	};
};

struct ib_sa_classport_cache {
	bool valid;
	int retry_cnt;
	struct rdma_class_port_info data;
};

struct ib_sa_port {
	struct ib_mad_agent *agent;
	struct ib_sa_sm_ah  *sm_ah;
	struct work_struct   update_task;
	struct ib_sa_classport_cache classport_info;
	struct delayed_work ib_cpi_work;
	spinlock_t                   classport_lock; /* protects class port info set */
	spinlock_t           ah_lock;
	u8                   port_num;
};

struct ib_sa_device {
	int                     start_port, end_port;
	struct ib_event_handler event_handler;
	struct ib_sa_port port[0];
};

struct ib_sa_query {
	void (*callback)(struct ib_sa_query *, int, struct ib_sa_mad *);
	void (*release)(struct ib_sa_query *);
	struct ib_sa_client    *client;
	struct ib_sa_port      *port;
	struct ib_mad_send_buf *mad_buf;
	struct ib_sa_sm_ah     *sm_ah;
	int			id;
	u32			flags;
	struct list_head	list; /* Local svc request list */
	u32			seq; /* Local svc request sequence number */
	unsigned long		timeout; /* Local svc timeout */
	u8			path_use; /* How will the pathrecord be used */
};

#define IB_SA_ENABLE_LOCAL_SERVICE	0x00000001
#define IB_SA_CANCEL			0x00000002
#define IB_SA_QUERY_OPA			0x00000004

struct ib_sa_service_query {
	void (*callback)(int, struct ib_sa_service_rec *, void *);
	void *context;
	struct ib_sa_query sa_query;
};

struct ib_sa_path_query {
	void (*callback)(int, struct sa_path_rec *, void *);
	void *context;
	struct ib_sa_query sa_query;
	struct sa_path_rec *conv_pr;
};

struct ib_sa_guidinfo_query {
	void (*callback)(int, struct ib_sa_guidinfo_rec *, void *);
	void *context;
	struct ib_sa_query sa_query;
};

struct ib_sa_classport_info_query {
	void (*callback)(void *);
	void *context;
	struct ib_sa_query sa_query;
};

struct ib_sa_mcmember_query {
	void (*callback)(int, struct ib_sa_mcmember_rec *, void *);
	void *context;
	struct ib_sa_query sa_query;
};

static void ib_sa_add_one(struct ib_device *device);
static void ib_sa_remove_one(struct ib_device *device, void *client_data);

static struct ib_client sa_client = {
	.name   = "sa",
	.add    = ib_sa_add_one,
	.remove = ib_sa_remove_one
};

static DEFINE_SPINLOCK(idr_lock);
static DEFINE_IDR(query_idr);

static DEFINE_SPINLOCK(tid_lock);
static u32 tid;

#define PATH_REC_FIELD(field) \
	.struct_offset_bytes = offsetof(struct sa_path_rec, field),	\
	.struct_size_bytes   = sizeof((struct sa_path_rec *)0)->field,	\
	.field_name          = "sa_path_rec:" #field

static const struct ib_field path_rec_table[] = {
	{ PATH_REC_FIELD(service_id),
	  .offset_words = 0,
	  .offset_bits  = 0,
	  .size_bits    = 64 },
	{ PATH_REC_FIELD(dgid),
	  .offset_words = 2,
	  .offset_bits  = 0,
	  .size_bits    = 128 },
	{ PATH_REC_FIELD(sgid),
	  .offset_words = 6,
	  .offset_bits  = 0,
	  .size_bits    = 128 },
	{ PATH_REC_FIELD(ib.dlid),
	  .offset_words = 10,
	  .offset_bits  = 0,
	  .size_bits    = 16 },
	{ PATH_REC_FIELD(ib.slid),
	  .offset_words = 10,
	  .offset_bits  = 16,
	  .size_bits    = 16 },
	{ PATH_REC_FIELD(ib.raw_traffic),
	  .offset_words = 11,
	  .offset_bits  = 0,
	  .size_bits    = 1 },
	{ RESERVED,
	  .offset_words = 11,
	  .offset_bits  = 1,
	  .size_bits    = 3 },
	{ PATH_REC_FIELD(flow_label),
	  .offset_words = 11,
	  .offset_bits  = 4,
	  .size_bits    = 20 },
	{ PATH_REC_FIELD(hop_limit),
	  .offset_words = 11,
	  .offset_bits  = 24,
	  .size_bits    = 8 },
	{ PATH_REC_FIELD(traffic_class),
	  .offset_words = 12,
	  .offset_bits  = 0,
	  .size_bits    = 8 },
	{ PATH_REC_FIELD(reversible),
	  .offset_words = 12,
	  .offset_bits  = 8,
	  .size_bits    = 1 },
	{ PATH_REC_FIELD(numb_path),
	  .offset_words = 12,
	  .offset_bits  = 9,
	  .size_bits    = 7 },
	{ PATH_REC_FIELD(pkey),
	  .offset_words = 12,
	  .offset_bits  = 16,
	  .size_bits    = 16 },
	{ PATH_REC_FIELD(qos_class),
	  .offset_words = 13,
	  .offset_bits  = 0,
	  .size_bits    = 12 },
	{ PATH_REC_FIELD(sl),
	  .offset_words = 13,
	  .offset_bits  = 12,
	  .size_bits    = 4 },
	{ PATH_REC_FIELD(mtu_selector),
	  .offset_words = 13,
	  .offset_bits  = 16,
	  .size_bits    = 2 },
	{ PATH_REC_FIELD(mtu),
	  .offset_words = 13,
	  .offset_bits  = 18,
	  .size_bits    = 6 },
	{ PATH_REC_FIELD(rate_selector),
	  .offset_words = 13,
	  .offset_bits  = 24,
	  .size_bits    = 2 },
	{ PATH_REC_FIELD(rate),
	  .offset_words = 13,
	  .offset_bits  = 26,
	  .size_bits    = 6 },
	{ PATH_REC_FIELD(packet_life_time_selector),
	  .offset_words = 14,
	  .offset_bits  = 0,
	  .size_bits    = 2 },
	{ PATH_REC_FIELD(packet_life_time),
	  .offset_words = 14,
	  .offset_bits  = 2,
	  .size_bits    = 6 },
	{ PATH_REC_FIELD(preference),
	  .offset_words = 14,
	  .offset_bits  = 8,
	  .size_bits    = 8 },
	{ RESERVED,
	  .offset_words = 14,
	  .offset_bits  = 16,
	  .size_bits    = 48 },
};

#define OPA_PATH_REC_FIELD(field) \
	.struct_offset_bytes = \
		offsetof(struct sa_path_rec, field), \
	.struct_size_bytes   = \
		sizeof((struct sa_path_rec *)0)->field,	\
	.field_name          = "sa_path_rec:" #field

static const struct ib_field opa_path_rec_table[] = {
	{ OPA_PATH_REC_FIELD(service_id),
	  .offset_words = 0,
	  .offset_bits  = 0,
	  .size_bits    = 64 },
	{ OPA_PATH_REC_FIELD(dgid),
	  .offset_words = 2,
	  .offset_bits  = 0,
	  .size_bits    = 128 },
	{ OPA_PATH_REC_FIELD(sgid),
	  .offset_words = 6,
	  .offset_bits  = 0,
	  .size_bits    = 128 },
	{ OPA_PATH_REC_FIELD(opa.dlid),
	  .offset_words = 10,
	  .offset_bits  = 0,
	  .size_bits    = 32 },
	{ OPA_PATH_REC_FIELD(opa.slid),
	  .offset_words = 11,
	  .offset_bits  = 0,
	  .size_bits    = 32 },
	{ OPA_PATH_REC_FIELD(opa.raw_traffic),
	  .offset_words = 12,
	  .offset_bits  = 0,
	  .size_bits    = 1 },
	{ RESERVED,
	  .offset_words = 12,
	  .offset_bits  = 1,
	  .size_bits    = 3 },
	{ OPA_PATH_REC_FIELD(flow_label),
	  .offset_words = 12,
	  .offset_bits  = 4,
	  .size_bits    = 20 },
	{ OPA_PATH_REC_FIELD(hop_limit),
	  .offset_words = 12,
	  .offset_bits  = 24,
	  .size_bits    = 8 },
	{ OPA_PATH_REC_FIELD(traffic_class),
	  .offset_words = 13,
	  .offset_bits  = 0,
	  .size_bits    = 8 },
	{ OPA_PATH_REC_FIELD(reversible),
	  .offset_words = 13,
	  .offset_bits  = 8,
	  .size_bits    = 1 },
	{ OPA_PATH_REC_FIELD(numb_path),
	  .offset_words = 13,
	  .offset_bits  = 9,
	  .size_bits    = 7 },
	{ OPA_PATH_REC_FIELD(pkey),
	  .offset_words = 13,
	  .offset_bits  = 16,
	  .size_bits    = 16 },
	{ OPA_PATH_REC_FIELD(opa.l2_8B),
	  .offset_words = 14,
	  .offset_bits  = 0,
	  .size_bits    = 1 },
	{ OPA_PATH_REC_FIELD(opa.l2_10B),
	  .offset_words = 14,
	  .offset_bits  = 1,
	  .size_bits    = 1 },
	{ OPA_PATH_REC_FIELD(opa.l2_9B),
	  .offset_words = 14,
	  .offset_bits  = 2,
	  .size_bits    = 1 },
	{ OPA_PATH_REC_FIELD(opa.l2_16B),
	  .offset_words = 14,
	  .offset_bits  = 3,
	  .size_bits    = 1 },
	{ RESERVED,
	  .offset_words = 14,
	  .offset_bits  = 4,
	  .size_bits    = 2 },
	{ OPA_PATH_REC_FIELD(opa.qos_type),
	  .offset_words = 14,
	  .offset_bits  = 6,
	  .size_bits    = 2 },
	{ OPA_PATH_REC_FIELD(opa.qos_priority),
	  .offset_words = 14,
	  .offset_bits  = 8,
	  .size_bits    = 8 },
	{ RESERVED,
	  .offset_words = 14,
	  .offset_bits  = 16,
	  .size_bits    = 3 },
	{ OPA_PATH_REC_FIELD(sl),
	  .offset_words = 14,
	  .offset_bits  = 19,
	  .size_bits    = 5 },
	{ RESERVED,
	  .offset_words = 14,
	  .offset_bits  = 24,
	  .size_bits    = 8 },
	{ OPA_PATH_REC_FIELD(mtu_selector),
	  .offset_words = 15,
	  .offset_bits  = 0,
	  .size_bits    = 2 },
	{ OPA_PATH_REC_FIELD(mtu),
	  .offset_words = 15,
	  .offset_bits  = 2,
	  .size_bits    = 6 },
	{ OPA_PATH_REC_FIELD(rate_selector),
	  .offset_words = 15,
	  .offset_bits  = 8,
	  .size_bits    = 2 },
	{ OPA_PATH_REC_FIELD(rate),
	  .offset_words = 15,
	  .offset_bits  = 10,
	  .size_bits    = 6 },
	{ OPA_PATH_REC_FIELD(packet_life_time_selector),
	  .offset_words = 15,
	  .offset_bits  = 16,
	  .size_bits    = 2 },
	{ OPA_PATH_REC_FIELD(packet_life_time),
	  .offset_words = 15,
	  .offset_bits  = 18,
	  .size_bits    = 6 },
	{ OPA_PATH_REC_FIELD(preference),
	  .offset_words = 15,
	  .offset_bits  = 24,
	  .size_bits    = 8 },
};

#define MCMEMBER_REC_FIELD(field) \
	.struct_offset_bytes = offsetof(struct ib_sa_mcmember_rec, field),	\
	.struct_size_bytes   = sizeof ((struct ib_sa_mcmember_rec *) 0)->field,	\
	.field_name          = "sa_mcmember_rec:" #field

static const struct ib_field mcmember_rec_table[] = {
	{ MCMEMBER_REC_FIELD(mgid),
	  .offset_words = 0,
	  .offset_bits  = 0,
	  .size_bits    = 128 },
	{ MCMEMBER_REC_FIELD(port_gid),
	  .offset_words = 4,
	  .offset_bits  = 0,
	  .size_bits    = 128 },
	{ MCMEMBER_REC_FIELD(qkey),
	  .offset_words = 8,
	  .offset_bits  = 0,
	  .size_bits    = 32 },
	{ MCMEMBER_REC_FIELD(mlid),
	  .offset_words = 9,
	  .offset_bits  = 0,
	  .size_bits    = 16 },
	{ MCMEMBER_REC_FIELD(mtu_selector),
	  .offset_words = 9,
	  .offset_bits  = 16,
	  .size_bits    = 2 },
	{ MCMEMBER_REC_FIELD(mtu),
	  .offset_words = 9,
	  .offset_bits  = 18,
	  .size_bits    = 6 },
	{ MCMEMBER_REC_FIELD(traffic_class),
	  .offset_words = 9,
	  .offset_bits  = 24,
	  .size_bits    = 8 },
	{ MCMEMBER_REC_FIELD(pkey),
	  .offset_words = 10,
	  .offset_bits  = 0,
	  .size_bits    = 16 },
	{ MCMEMBER_REC_FIELD(rate_selector),
	  .offset_words = 10,
	  .offset_bits  = 16,
	  .size_bits    = 2 },
	{ MCMEMBER_REC_FIELD(rate),
	  .offset_words = 10,
	  .offset_bits  = 18,
	  .size_bits    = 6 },
	{ MCMEMBER_REC_FIELD(packet_life_time_selector),
	  .offset_words = 10,
	  .offset_bits  = 24,
	  .size_bits    = 2 },
	{ MCMEMBER_REC_FIELD(packet_life_time),
	  .offset_words = 10,
	  .offset_bits  = 26,
	  .size_bits    = 6 },
	{ MCMEMBER_REC_FIELD(sl),
	  .offset_words = 11,
	  .offset_bits  = 0,
	  .size_bits    = 4 },
	{ MCMEMBER_REC_FIELD(flow_label),
	  .offset_words = 11,
	  .offset_bits  = 4,
	  .size_bits    = 20 },
	{ MCMEMBER_REC_FIELD(hop_limit),
	  .offset_words = 11,
	  .offset_bits  = 24,
	  .size_bits    = 8 },
	{ MCMEMBER_REC_FIELD(scope),
	  .offset_words = 12,
	  .offset_bits  = 0,
	  .size_bits    = 4 },
	{ MCMEMBER_REC_FIELD(join_state),
	  .offset_words = 12,
	  .offset_bits  = 4,
	  .size_bits    = 4 },
	{ MCMEMBER_REC_FIELD(proxy_join),
	  .offset_words = 12,
	  .offset_bits  = 8,
	  .size_bits    = 1 },
	{ RESERVED,
	  .offset_words = 12,
	  .offset_bits  = 9,
	  .size_bits    = 23 },
};

#define SERVICE_REC_FIELD(field) \
	.struct_offset_bytes = offsetof(struct ib_sa_service_rec, field),	\
	.struct_size_bytes   = sizeof ((struct ib_sa_service_rec *) 0)->field,	\
	.field_name          = "sa_service_rec:" #field

static const struct ib_field service_rec_table[] = {
	{ SERVICE_REC_FIELD(id),
	  .offset_words = 0,
	  .offset_bits  = 0,
	  .size_bits    = 64 },
	{ SERVICE_REC_FIELD(gid),
	  .offset_words = 2,
	  .offset_bits  = 0,
	  .size_bits    = 128 },
	{ SERVICE_REC_FIELD(pkey),
	  .offset_words = 6,
	  .offset_bits  = 0,
	  .size_bits    = 16 },
	{ SERVICE_REC_FIELD(lease),
	  .offset_words = 7,
	  .offset_bits  = 0,
	  .size_bits    = 32 },
	{ SERVICE_REC_FIELD(key),
	  .offset_words = 8,
	  .offset_bits  = 0,
	  .size_bits    = 128 },
	{ SERVICE_REC_FIELD(name),
	  .offset_words = 12,
	  .offset_bits  = 0,
	  .size_bits    = 64*8 },
	{ SERVICE_REC_FIELD(data8),
	  .offset_words = 28,
	  .offset_bits  = 0,
	  .size_bits    = 16*8 },
	{ SERVICE_REC_FIELD(data16),
	  .offset_words = 32,
	  .offset_bits  = 0,
	  .size_bits    = 8*16 },
	{ SERVICE_REC_FIELD(data32),
	  .offset_words = 36,
	  .offset_bits  = 0,
	  .size_bits    = 4*32 },
	{ SERVICE_REC_FIELD(data64),
	  .offset_words = 40,
	  .offset_bits  = 0,
	  .size_bits    = 2*64 },
};

#define CLASSPORTINFO_REC_FIELD(field) \
	.struct_offset_bytes = offsetof(struct ib_class_port_info, field),	\
	.struct_size_bytes   = sizeof((struct ib_class_port_info *)0)->field,	\
	.field_name          = "ib_class_port_info:" #field

static const struct ib_field ib_classport_info_rec_table[] = {
	{ CLASSPORTINFO_REC_FIELD(base_version),
	  .offset_words = 0,
	  .offset_bits  = 0,
	  .size_bits    = 8 },
	{ CLASSPORTINFO_REC_FIELD(class_version),
	  .offset_words = 0,
	  .offset_bits  = 8,
	  .size_bits    = 8 },
	{ CLASSPORTINFO_REC_FIELD(capability_mask),
	  .offset_words = 0,
	  .offset_bits  = 16,
	  .size_bits    = 16 },
	{ CLASSPORTINFO_REC_FIELD(cap_mask2_resp_time),
	  .offset_words = 1,
	  .offset_bits  = 0,
	  .size_bits    = 32 },
	{ CLASSPORTINFO_REC_FIELD(redirect_gid),
	  .offset_words = 2,
	  .offset_bits  = 0,
	  .size_bits    = 128 },
	{ CLASSPORTINFO_REC_FIELD(redirect_tcslfl),
	  .offset_words = 6,
	  .offset_bits  = 0,
	  .size_bits    = 32 },
	{ CLASSPORTINFO_REC_FIELD(redirect_lid),
	  .offset_words = 7,
	  .offset_bits  = 0,
	  .size_bits    = 16 },
	{ CLASSPORTINFO_REC_FIELD(redirect_pkey),
	  .offset_words = 7,
	  .offset_bits  = 16,
	  .size_bits    = 16 },

	{ CLASSPORTINFO_REC_FIELD(redirect_qp),
	  .offset_words = 8,
	  .offset_bits  = 0,
	  .size_bits    = 32 },
	{ CLASSPORTINFO_REC_FIELD(redirect_qkey),
	  .offset_words = 9,
	  .offset_bits  = 0,
	  .size_bits    = 32 },

	{ CLASSPORTINFO_REC_FIELD(trap_gid),
	  .offset_words = 10,
	  .offset_bits  = 0,
	  .size_bits    = 128 },
	{ CLASSPORTINFO_REC_FIELD(trap_tcslfl),
	  .offset_words = 14,
	  .offset_bits  = 0,
	  .size_bits    = 32 },

	{ CLASSPORTINFO_REC_FIELD(trap_lid),
	  .offset_words = 15,
	  .offset_bits  = 0,
	  .size_bits    = 16 },
	{ CLASSPORTINFO_REC_FIELD(trap_pkey),
	  .offset_words = 15,
	  .offset_bits  = 16,
	  .size_bits    = 16 },

	{ CLASSPORTINFO_REC_FIELD(trap_hlqp),
	  .offset_words = 16,
	  .offset_bits  = 0,
	  .size_bits    = 32 },
	{ CLASSPORTINFO_REC_FIELD(trap_qkey),
	  .offset_words = 17,
	  .offset_bits  = 0,
	  .size_bits    = 32 },
};

#define OPA_CLASSPORTINFO_REC_FIELD(field) \
	.struct_offset_bytes =\
		offsetof(struct opa_class_port_info, field),	\
	.struct_size_bytes   = \
		sizeof((struct opa_class_port_info *)0)->field,	\
	.field_name          = "opa_class_port_info:" #field

static const struct ib_field opa_classport_info_rec_table[] = {
	{ OPA_CLASSPORTINFO_REC_FIELD(base_version),
	  .offset_words = 0,
	  .offset_bits  = 0,
	  .size_bits    = 8 },
	{ OPA_CLASSPORTINFO_REC_FIELD(class_version),
	  .offset_words = 0,
	  .offset_bits  = 8,
	  .size_bits    = 8 },
	{ OPA_CLASSPORTINFO_REC_FIELD(cap_mask),
	  .offset_words = 0,
	  .offset_bits  = 16,
	  .size_bits    = 16 },
	{ OPA_CLASSPORTINFO_REC_FIELD(cap_mask2_resp_time),
	  .offset_words = 1,
	  .offset_bits  = 0,
	  .size_bits    = 32 },
	{ OPA_CLASSPORTINFO_REC_FIELD(redirect_gid),
	  .offset_words = 2,
	  .offset_bits  = 0,
	  .size_bits    = 128 },
	{ OPA_CLASSPORTINFO_REC_FIELD(redirect_tc_fl),
	  .offset_words = 6,
	  .offset_bits  = 0,
	  .size_bits    = 32 },
	{ OPA_CLASSPORTINFO_REC_FIELD(redirect_lid),
	  .offset_words = 7,
	  .offset_bits  = 0,
	  .size_bits    = 32 },
	{ OPA_CLASSPORTINFO_REC_FIELD(redirect_sl_qp),
	  .offset_words = 8,
	  .offset_bits  = 0,
	  .size_bits    = 32 },
	{ OPA_CLASSPORTINFO_REC_FIELD(redirect_qkey),
	  .offset_words = 9,
	  .offset_bits  = 0,
	  .size_bits    = 32 },
	{ OPA_CLASSPORTINFO_REC_FIELD(trap_gid),
	  .offset_words = 10,
	  .offset_bits  = 0,
	  .size_bits    = 128 },
	{ OPA_CLASSPORTINFO_REC_FIELD(trap_tc_fl),
	  .offset_words = 14,
	  .offset_bits  = 0,
	  .size_bits    = 32 },
	{ OPA_CLASSPORTINFO_REC_FIELD(trap_lid),
	  .offset_words = 15,
	  .offset_bits  = 0,
	  .size_bits    = 32 },
	{ OPA_CLASSPORTINFO_REC_FIELD(trap_hl_qp),
	  .offset_words = 16,
	  .offset_bits  = 0,
	  .size_bits    = 32 },
	{ OPA_CLASSPORTINFO_REC_FIELD(trap_qkey),
	  .offset_words = 17,
	  .offset_bits  = 0,
	  .size_bits    = 32 },
	{ OPA_CLASSPORTINFO_REC_FIELD(trap_pkey),
	  .offset_words = 18,
	  .offset_bits  = 0,
	  .size_bits    = 16 },
	{ OPA_CLASSPORTINFO_REC_FIELD(redirect_pkey),
	  .offset_words = 18,
	  .offset_bits  = 16,
	  .size_bits    = 16 },
	{ OPA_CLASSPORTINFO_REC_FIELD(trap_sl_rsvd),
	  .offset_words = 19,
	  .offset_bits  = 0,
	  .size_bits    = 8 },
	{ RESERVED,
	  .offset_words = 19,
	  .offset_bits  = 8,
	  .size_bits    = 24 },
};

#define GUIDINFO_REC_FIELD(field) \
	.struct_offset_bytes = offsetof(struct ib_sa_guidinfo_rec, field),	\
	.struct_size_bytes   = sizeof((struct ib_sa_guidinfo_rec *) 0)->field,	\
	.field_name          = "sa_guidinfo_rec:" #field

static const struct ib_field guidinfo_rec_table[] = {
	{ GUIDINFO_REC_FIELD(lid),
	  .offset_words = 0,
	  .offset_bits  = 0,
	  .size_bits    = 16 },
	{ GUIDINFO_REC_FIELD(block_num),
	  .offset_words = 0,
	  .offset_bits  = 16,
	  .size_bits    = 8 },
	{ GUIDINFO_REC_FIELD(res1),
	  .offset_words = 0,
	  .offset_bits  = 24,
	  .size_bits    = 8 },
	{ GUIDINFO_REC_FIELD(res2),
	  .offset_words = 1,
	  .offset_bits  = 0,
	  .size_bits    = 32 },
	{ GUIDINFO_REC_FIELD(guid_info_list),
	  .offset_words = 2,
	  .offset_bits  = 0,
	  .size_bits    = 512 },
};

static inline void ib_sa_disable_local_svc(struct ib_sa_query *query)
{
	query->flags &= ~IB_SA_ENABLE_LOCAL_SERVICE;
}

static void free_sm_ah(struct kref *kref)
{
	struct ib_sa_sm_ah *sm_ah = container_of(kref, struct ib_sa_sm_ah, ref);

	rdma_destroy_ah(sm_ah->ah, 0);
	kfree(sm_ah);
}

void ib_sa_register_client(struct ib_sa_client *client)
{
	atomic_set(&client->users, 1);
	init_completion(&client->comp);
}
EXPORT_SYMBOL(ib_sa_register_client);

void ib_sa_unregister_client(struct ib_sa_client *client)
{
	ib_sa_client_put(client);
	wait_for_completion(&client->comp);
}
EXPORT_SYMBOL(ib_sa_unregister_client);

/**
 * ib_sa_cancel_query - try to cancel an SA query
 * @id:ID of query to cancel
 * @query:query pointer to cancel
 *
 * Try to cancel an SA query.  If the id and query don't match up or
 * the query has already completed, nothing is done.  Otherwise the
 * query is canceled and will complete with a status of -EINTR.
 */
void ib_sa_cancel_query(int id, struct ib_sa_query *query)
{
	unsigned long flags;
	struct ib_mad_agent *agent;
	struct ib_mad_send_buf *mad_buf;

	spin_lock_irqsave(&idr_lock, flags);
	if (idr_find(&query_idr, id) != query) {
		spin_unlock_irqrestore(&idr_lock, flags);
		return;
	}
	agent = query->port->agent;
	mad_buf = query->mad_buf;
	spin_unlock_irqrestore(&idr_lock, flags);

	ib_cancel_mad(agent, mad_buf);
}
EXPORT_SYMBOL(ib_sa_cancel_query);

static u8 get_src_path_mask(struct ib_device *device, u8 port_num)
{
	struct ib_sa_device *sa_dev;
	struct ib_sa_port   *port;
	unsigned long flags;
	u8 src_path_mask;

	sa_dev = ib_get_client_data(device, &sa_client);
	if (!sa_dev)
		return 0x7f;

	port  = &sa_dev->port[port_num - sa_dev->start_port];
	spin_lock_irqsave(&port->ah_lock, flags);
	src_path_mask = port->sm_ah ? port->sm_ah->src_path_mask : 0x7f;
	spin_unlock_irqrestore(&port->ah_lock, flags);

	return src_path_mask;
}

static int
roce_resolve_route_from_path(struct ib_device *device, u8 port_num,
			     struct sa_path_rec *rec)
{
	if_t resolved_dev;
	if_t ndev;
	if_t idev;
	struct rdma_dev_addr dev_addr = {
		.bound_dev_if = ((sa_path_get_ifindex(rec) >= 0) ?
				 sa_path_get_ifindex(rec) : 0),
		.net = sa_path_get_ndev(rec) ?
			sa_path_get_ndev(rec) :
			&init_net
	};
	union rdma_sockaddr sgid_addr, dgid_addr;
	int ret;

	if (rec->roce.route_resolved)
		return 0;

	if (!device->get_netdev)
		return -EOPNOTSUPP;

	rdma_gid2ip(&sgid_addr._sockaddr, &rec->sgid);
	rdma_gid2ip(&dgid_addr._sockaddr, &rec->dgid);

	/* validate the route */
	ret = rdma_resolve_ip_route(&sgid_addr._sockaddr,
				    &dgid_addr._sockaddr, &dev_addr);
	if (ret)
		return ret;

	if ((dev_addr.network == RDMA_NETWORK_IPV4 ||
	     dev_addr.network == RDMA_NETWORK_IPV6) &&
	    rec->rec_type != SA_PATH_REC_TYPE_ROCE_V2)
		return -EINVAL;

	idev = device->get_netdev(device, port_num);
	if (!idev)
		return -ENODEV;

	resolved_dev = dev_get_by_index(dev_addr.net,
					dev_addr.bound_dev_if);
	if (!resolved_dev) {
		ret = -ENODEV;
		goto done;
	}
	ndev = ib_get_ndev_from_path(rec);
	rcu_read_lock();
	if ((ndev && ndev != resolved_dev) ||
	    (resolved_dev != idev &&
	     rdma_vlan_dev_real_dev(resolved_dev) != idev))
		ret = -EHOSTUNREACH;
	rcu_read_unlock();
	dev_put(resolved_dev);
	if (ndev)
		dev_put(ndev);
done:
	dev_put(idev);
	if (!ret)
		rec->roce.route_resolved = true;
	return ret;
}

static int init_ah_attr_grh_fields(struct ib_device *device, u8 port_num,
				   struct sa_path_rec *rec,
				   struct rdma_ah_attr *ah_attr)
{
	enum ib_gid_type type = sa_conv_pathrec_to_gid_type(rec);
	if_t ndev;
	u16 gid_index;
	int ret;

	ndev = ib_get_ndev_from_path(rec);
	ret = ib_find_cached_gid_by_port(device, &rec->sgid, type,
					 port_num, ndev, &gid_index);
	if (ndev)
		dev_put(ndev);
	if (ret)
		return ret;

	rdma_ah_set_grh(ah_attr, &rec->dgid,
			be32_to_cpu(rec->flow_label),
			gid_index, rec->hop_limit,
			rec->traffic_class);
	return 0;
}

int ib_init_ah_attr_from_path(struct ib_device *device, u8 port_num,
			      struct sa_path_rec *rec,
			      struct rdma_ah_attr *ah_attr)
{
	int ret = 0;

	memset(ah_attr, 0, sizeof(*ah_attr));
	ah_attr->type = rdma_ah_find_type(device, port_num);
	rdma_ah_set_sl(ah_attr, rec->sl);
	rdma_ah_set_port_num(ah_attr, port_num);
	rdma_ah_set_static_rate(ah_attr, rec->rate);

	if (sa_path_is_roce(rec)) {
		ret = roce_resolve_route_from_path(device, port_num, rec);
		if (ret)
			return ret;

		memcpy(ah_attr->roce.dmac, sa_path_get_dmac(rec), ETH_ALEN);
	} else {
		rdma_ah_set_dlid(ah_attr, be32_to_cpu(sa_path_get_dlid(rec)));
		if (sa_path_is_opa(rec) &&
		    rdma_ah_get_dlid(ah_attr) == be16_to_cpu(IB_LID_PERMISSIVE))
			rdma_ah_set_make_grd(ah_attr, true);

		rdma_ah_set_path_bits(ah_attr,
				      be32_to_cpu(sa_path_get_slid(rec)) &
				      get_src_path_mask(device, port_num));
	}

	if (rec->hop_limit > 0 || sa_path_is_roce(rec))
		ret = init_ah_attr_grh_fields(device, port_num, rec, ah_attr);
	return ret;
}
EXPORT_SYMBOL(ib_init_ah_attr_from_path);

static int alloc_mad(struct ib_sa_query *query, gfp_t gfp_mask)
{
	struct rdma_ah_attr ah_attr;
	unsigned long flags;

	spin_lock_irqsave(&query->port->ah_lock, flags);
	if (!query->port->sm_ah) {
		spin_unlock_irqrestore(&query->port->ah_lock, flags);
		return -EAGAIN;
	}
	kref_get(&query->port->sm_ah->ref);
	query->sm_ah = query->port->sm_ah;
	spin_unlock_irqrestore(&query->port->ah_lock, flags);

	/*
	 * Always check if sm_ah has valid dlid assigned,
	 * before querying for class port info
	 */
	if ((rdma_query_ah(query->sm_ah->ah, &ah_attr) < 0) ||
	    !rdma_is_valid_unicast_lid(&ah_attr)) {
		kref_put(&query->sm_ah->ref, free_sm_ah);
		return -EAGAIN;
	}
	query->mad_buf = ib_create_send_mad(query->port->agent, 1,
					    query->sm_ah->pkey_index,
					    0, IB_MGMT_SA_HDR, IB_MGMT_SA_DATA,
					    gfp_mask,
					    ((query->flags & IB_SA_QUERY_OPA) ?
					     OPA_MGMT_BASE_VERSION :
					     IB_MGMT_BASE_VERSION));
	if (IS_ERR(query->mad_buf)) {
		kref_put(&query->sm_ah->ref, free_sm_ah);
		return -ENOMEM;
	}

	query->mad_buf->ah = query->sm_ah->ah;

	return 0;
}

static void free_mad(struct ib_sa_query *query)
{
	ib_free_send_mad(query->mad_buf);
	kref_put(&query->sm_ah->ref, free_sm_ah);
}

static void init_mad(struct ib_sa_query *query, struct ib_mad_agent *agent)
{
	struct ib_sa_mad *mad = query->mad_buf->mad;
	unsigned long flags;

	memset(mad, 0, sizeof *mad);

	if (query->flags & IB_SA_QUERY_OPA) {
		mad->mad_hdr.base_version  = OPA_MGMT_BASE_VERSION;
		mad->mad_hdr.class_version = OPA_SA_CLASS_VERSION;
	} else {
		mad->mad_hdr.base_version  = IB_MGMT_BASE_VERSION;
		mad->mad_hdr.class_version = IB_SA_CLASS_VERSION;
	}
	mad->mad_hdr.mgmt_class    = IB_MGMT_CLASS_SUBN_ADM;
	spin_lock_irqsave(&tid_lock, flags);
	mad->mad_hdr.tid           =
		cpu_to_be64(((u64) agent->hi_tid) << 32 | tid++);
	spin_unlock_irqrestore(&tid_lock, flags);
}

static int send_mad(struct ib_sa_query *query, int timeout_ms, gfp_t gfp_mask)
{
	bool preload = gfpflags_allow_blocking(gfp_mask);
	unsigned long flags;
	int ret, id;

	if (preload)
		idr_preload(gfp_mask);
	spin_lock_irqsave(&idr_lock, flags);

	id = idr_alloc(&query_idr, query, 0, 0, GFP_NOWAIT);

	spin_unlock_irqrestore(&idr_lock, flags);
	if (preload)
		idr_preload_end();
	if (id < 0)
		return id;

	query->mad_buf->timeout_ms  = timeout_ms;
	query->mad_buf->context[0] = query;
	query->id = id;

	if ((query->flags & IB_SA_ENABLE_LOCAL_SERVICE) &&
	    (!(query->flags & IB_SA_QUERY_OPA))) {
		ib_sa_disable_local_svc(query);
	}

	ret = ib_post_send_mad(query->mad_buf, NULL);
	if (ret) {
		spin_lock_irqsave(&idr_lock, flags);
		idr_remove(&query_idr, id);
		spin_unlock_irqrestore(&idr_lock, flags);
	}

	/*
	 * It's not safe to dereference query any more, because the
	 * send may already have completed and freed the query in
	 * another context.
	 */
	return ret ? ret : id;
}

void ib_sa_unpack_path(void *attribute, struct sa_path_rec *rec)
{
	ib_unpack(path_rec_table, ARRAY_SIZE(path_rec_table), attribute, rec);
}
EXPORT_SYMBOL(ib_sa_unpack_path);

void ib_sa_pack_path(struct sa_path_rec *rec, void *attribute)
{
	ib_pack(path_rec_table, ARRAY_SIZE(path_rec_table), rec, attribute);
}
EXPORT_SYMBOL(ib_sa_pack_path);

static bool ib_sa_opa_pathrecord_support(struct ib_sa_client *client,
					 struct ib_device *device,
					 u8 port_num)
{
	struct ib_sa_device *sa_dev = ib_get_client_data(device, &sa_client);
	struct ib_sa_port *port;
	unsigned long flags;
	bool ret = false;

	if (!sa_dev)
		return ret;

	port = &sa_dev->port[port_num - sa_dev->start_port];
	spin_lock_irqsave(&port->classport_lock, flags);
	if (!port->classport_info.valid)
		goto ret;

	if (port->classport_info.data.type == RDMA_CLASS_PORT_INFO_OPA)
		ret = opa_get_cpi_capmask2(&port->classport_info.data.opa) &
			OPA_CLASS_PORT_INFO_PR_SUPPORT;
ret:
	spin_unlock_irqrestore(&port->classport_lock, flags);
	return ret;
}

enum opa_pr_supported {
	PR_NOT_SUPPORTED,
	PR_OPA_SUPPORTED,
	PR_IB_SUPPORTED
};

/**
 * Check if current PR query can be an OPA query.
 * Retuns PR_NOT_SUPPORTED if a path record query is not
 * possible, PR_OPA_SUPPORTED if an OPA path record query
 * is possible and PR_IB_SUPPORTED if an IB path record
 * query is possible.
 */
static int opa_pr_query_possible(struct ib_sa_client *client,
				 struct ib_device *device,
				 u8 port_num,
				 struct sa_path_rec *rec)
{
	struct ib_port_attr port_attr;

	if (ib_query_port(device, port_num, &port_attr))
		return PR_NOT_SUPPORTED;

	if (ib_sa_opa_pathrecord_support(client, device, port_num))
		return PR_OPA_SUPPORTED;

	if (port_attr.lid >= be16_to_cpu(IB_MULTICAST_LID_BASE))
		return PR_NOT_SUPPORTED;
	else
		return PR_IB_SUPPORTED;
}

static void ib_sa_path_rec_callback(struct ib_sa_query *sa_query,
				    int status,
				    struct ib_sa_mad *mad)
{
	struct ib_sa_path_query *query =
		container_of(sa_query, struct ib_sa_path_query, sa_query);

	if (mad) {
		struct sa_path_rec rec;

		if (sa_query->flags & IB_SA_QUERY_OPA) {
			ib_unpack(opa_path_rec_table,
				  ARRAY_SIZE(opa_path_rec_table),
				  mad->data, &rec);
			rec.rec_type = SA_PATH_REC_TYPE_OPA;
			query->callback(status, &rec, query->context);
		} else {
			ib_unpack(path_rec_table,
				  ARRAY_SIZE(path_rec_table),
				  mad->data, &rec);
			rec.rec_type = SA_PATH_REC_TYPE_IB;
			sa_path_set_ndev(&rec, NULL);
			sa_path_set_ifindex(&rec, 0);
			sa_path_set_dmac_zero(&rec);

			if (query->conv_pr) {
				struct sa_path_rec opa;

				memset(&opa, 0, sizeof(struct sa_path_rec));
				sa_convert_path_ib_to_opa(&opa, &rec);
				query->callback(status, &opa, query->context);
			} else {
				query->callback(status, &rec, query->context);
			}
		}
	} else
		query->callback(status, NULL, query->context);
}

static void ib_sa_path_rec_release(struct ib_sa_query *sa_query)
{
	struct ib_sa_path_query *query =
		container_of(sa_query, struct ib_sa_path_query, sa_query);

	kfree(query->conv_pr);
	kfree(query);
}

/**
 * ib_sa_path_rec_get - Start a Path get query
 * @client:SA client
 * @device:device to send query on
 * @port_num: port number to send query on
 * @rec:Path Record to send in query
 * @comp_mask:component mask to send in query
 * @timeout_ms:time to wait for response
 * @gfp_mask:GFP mask to use for internal allocations
 * @callback:function called when query completes, times out or is
 * canceled
 * @context:opaque user context passed to callback
 * @sa_query:query context, used to cancel query
 *
 * Send a Path Record Get query to the SA to look up a path.  The
 * callback function will be called when the query completes (or
 * fails); status is 0 for a successful response, -EINTR if the query
 * is canceled, -ETIMEDOUT is the query timed out, or -EIO if an error
 * occurred sending the query.  The resp parameter of the callback is
 * only valid if status is 0.
 *
 * If the return value of ib_sa_path_rec_get() is negative, it is an
 * error code.  Otherwise it is a query ID that can be used to cancel
 * the query.
 */
int ib_sa_path_rec_get(struct ib_sa_client *client,
		       struct ib_device *device, u8 port_num,
		       struct sa_path_rec *rec,
		       ib_sa_comp_mask comp_mask,
		       int timeout_ms, gfp_t gfp_mask,
		       void (*callback)(int status,
					struct sa_path_rec *resp,
					void *context),
		       void *context,
		       struct ib_sa_query **sa_query)
{
	struct ib_sa_path_query *query;
	struct ib_sa_device *sa_dev = ib_get_client_data(device, &sa_client);
	struct ib_sa_port   *port;
	struct ib_mad_agent *agent;
	struct ib_sa_mad *mad;
	enum opa_pr_supported status;
	int ret;

	if (!sa_dev)
		return -ENODEV;

	if ((rec->rec_type != SA_PATH_REC_TYPE_IB) &&
	    (rec->rec_type != SA_PATH_REC_TYPE_OPA))
		return -EINVAL;

	port  = &sa_dev->port[port_num - sa_dev->start_port];
	agent = port->agent;

	query = kzalloc(sizeof(*query), gfp_mask);
	if (!query)
		return -ENOMEM;

	query->sa_query.port     = port;
	if (rec->rec_type == SA_PATH_REC_TYPE_OPA) {
		status = opa_pr_query_possible(client, device, port_num, rec);
		if (status == PR_NOT_SUPPORTED) {
			ret = -EINVAL;
			goto err1;
		} else if (status == PR_OPA_SUPPORTED) {
			query->sa_query.flags |= IB_SA_QUERY_OPA;
		} else {
			query->conv_pr =
				kmalloc(sizeof(*query->conv_pr), gfp_mask);
			if (!query->conv_pr) {
				ret = -ENOMEM;
				goto err1;
			}
		}
	}

	ret = alloc_mad(&query->sa_query, gfp_mask);
	if (ret)
		goto err2;

	ib_sa_client_get(client);
	query->sa_query.client = client;
	query->callback        = callback;
	query->context         = context;

	mad = query->sa_query.mad_buf->mad;
	init_mad(&query->sa_query, agent);

	query->sa_query.callback = callback ? ib_sa_path_rec_callback : NULL;
	query->sa_query.release  = ib_sa_path_rec_release;
	mad->mad_hdr.method	 = IB_MGMT_METHOD_GET;
	mad->mad_hdr.attr_id	 = cpu_to_be16(IB_SA_ATTR_PATH_REC);
	mad->sa_hdr.comp_mask	 = comp_mask;

	if (query->sa_query.flags & IB_SA_QUERY_OPA) {
		ib_pack(opa_path_rec_table, ARRAY_SIZE(opa_path_rec_table),
			rec, mad->data);
	} else if (query->conv_pr) {
		sa_convert_path_opa_to_ib(query->conv_pr, rec);
		ib_pack(path_rec_table, ARRAY_SIZE(path_rec_table),
			query->conv_pr, mad->data);
	} else {
		ib_pack(path_rec_table, ARRAY_SIZE(path_rec_table),
			rec, mad->data);
	}

	*sa_query = &query->sa_query;

	query->sa_query.flags |= IB_SA_ENABLE_LOCAL_SERVICE;
	query->sa_query.mad_buf->context[1] = (query->conv_pr) ?
						query->conv_pr : rec;

	ret = send_mad(&query->sa_query, timeout_ms, gfp_mask);
	if (ret < 0)
		goto err3;

	return ret;

err3:
	*sa_query = NULL;
	ib_sa_client_put(query->sa_query.client);
	free_mad(&query->sa_query);
err2:
	kfree(query->conv_pr);
err1:
	kfree(query);
	return ret;
}
EXPORT_SYMBOL(ib_sa_path_rec_get);

static void ib_sa_service_rec_callback(struct ib_sa_query *sa_query,
				    int status,
				    struct ib_sa_mad *mad)
{
	struct ib_sa_service_query *query =
		container_of(sa_query, struct ib_sa_service_query, sa_query);

	if (mad) {
		struct ib_sa_service_rec rec;

		ib_unpack(service_rec_table, ARRAY_SIZE(service_rec_table),
			  mad->data, &rec);
		query->callback(status, &rec, query->context);
	} else
		query->callback(status, NULL, query->context);
}

static void ib_sa_service_rec_release(struct ib_sa_query *sa_query)
{
	kfree(container_of(sa_query, struct ib_sa_service_query, sa_query));
}

/**
 * ib_sa_service_rec_query - Start Service Record operation
 * @client:SA client
 * @device:device to send request on
 * @port_num: port number to send request on
 * @method:SA method - should be get, set, or delete
 * @rec:Service Record to send in request
 * @comp_mask:component mask to send in request
 * @timeout_ms:time to wait for response
 * @gfp_mask:GFP mask to use for internal allocations
 * @callback:function called when request completes, times out or is
 * canceled
 * @context:opaque user context passed to callback
 * @sa_query:request context, used to cancel request
 *
 * Send a Service Record set/get/delete to the SA to register,
 * unregister or query a service record.
 * The callback function will be called when the request completes (or
 * fails); status is 0 for a successful response, -EINTR if the query
 * is canceled, -ETIMEDOUT is the query timed out, or -EIO if an error
 * occurred sending the query.  The resp parameter of the callback is
 * only valid if status is 0.
 *
 * If the return value of ib_sa_service_rec_query() is negative, it is an
 * error code.  Otherwise it is a request ID that can be used to cancel
 * the query.
 */
int ib_sa_service_rec_query(struct ib_sa_client *client,
			    struct ib_device *device, u8 port_num, u8 method,
			    struct ib_sa_service_rec *rec,
			    ib_sa_comp_mask comp_mask,
			    int timeout_ms, gfp_t gfp_mask,
			    void (*callback)(int status,
					     struct ib_sa_service_rec *resp,
					     void *context),
			    void *context,
			    struct ib_sa_query **sa_query)
{
	struct ib_sa_service_query *query;
	struct ib_sa_device *sa_dev = ib_get_client_data(device, &sa_client);
	struct ib_sa_port   *port;
	struct ib_mad_agent *agent;
	struct ib_sa_mad *mad;
	int ret;

	if (!sa_dev)
		return -ENODEV;

	port  = &sa_dev->port[port_num - sa_dev->start_port];
	agent = port->agent;

	if (method != IB_MGMT_METHOD_GET &&
	    method != IB_MGMT_METHOD_SET &&
	    method != IB_SA_METHOD_DELETE)
		return -EINVAL;

	query = kzalloc(sizeof(*query), gfp_mask);
	if (!query)
		return -ENOMEM;

	query->sa_query.port     = port;
	ret = alloc_mad(&query->sa_query, gfp_mask);
	if (ret)
		goto err1;

	ib_sa_client_get(client);
	query->sa_query.client = client;
	query->callback        = callback;
	query->context         = context;

	mad = query->sa_query.mad_buf->mad;
	init_mad(&query->sa_query, agent);

	query->sa_query.callback = callback ? ib_sa_service_rec_callback : NULL;
	query->sa_query.release  = ib_sa_service_rec_release;
	mad->mad_hdr.method	 = method;
	mad->mad_hdr.attr_id	 = cpu_to_be16(IB_SA_ATTR_SERVICE_REC);
	mad->sa_hdr.comp_mask	 = comp_mask;

	ib_pack(service_rec_table, ARRAY_SIZE(service_rec_table),
		rec, mad->data);

	*sa_query = &query->sa_query;

	ret = send_mad(&query->sa_query, timeout_ms, gfp_mask);
	if (ret < 0)
		goto err2;

	return ret;

err2:
	*sa_query = NULL;
	ib_sa_client_put(query->sa_query.client);
	free_mad(&query->sa_query);

err1:
	kfree(query);
	return ret;
}
EXPORT_SYMBOL(ib_sa_service_rec_query);

static void ib_sa_mcmember_rec_callback(struct ib_sa_query *sa_query,
					int status,
					struct ib_sa_mad *mad)
{
	struct ib_sa_mcmember_query *query =
		container_of(sa_query, struct ib_sa_mcmember_query, sa_query);

	if (mad) {
		struct ib_sa_mcmember_rec rec;

		ib_unpack(mcmember_rec_table, ARRAY_SIZE(mcmember_rec_table),
			  mad->data, &rec);
		query->callback(status, &rec, query->context);
	} else
		query->callback(status, NULL, query->context);
}

static void ib_sa_mcmember_rec_release(struct ib_sa_query *sa_query)
{
	kfree(container_of(sa_query, struct ib_sa_mcmember_query, sa_query));
}

int ib_sa_mcmember_rec_query(struct ib_sa_client *client,
			     struct ib_device *device, u8 port_num,
			     u8 method,
			     struct ib_sa_mcmember_rec *rec,
			     ib_sa_comp_mask comp_mask,
			     int timeout_ms, gfp_t gfp_mask,
			     void (*callback)(int status,
					      struct ib_sa_mcmember_rec *resp,
					      void *context),
			     void *context,
			     struct ib_sa_query **sa_query)
{
	struct ib_sa_mcmember_query *query;
	struct ib_sa_device *sa_dev = ib_get_client_data(device, &sa_client);
	struct ib_sa_port   *port;
	struct ib_mad_agent *agent;
	struct ib_sa_mad *mad;
	int ret;

	if (!sa_dev)
		return -ENODEV;

	port  = &sa_dev->port[port_num - sa_dev->start_port];
	agent = port->agent;

	query = kzalloc(sizeof(*query), gfp_mask);
	if (!query)
		return -ENOMEM;

	query->sa_query.port     = port;
	ret = alloc_mad(&query->sa_query, gfp_mask);
	if (ret)
		goto err1;

	ib_sa_client_get(client);
	query->sa_query.client = client;
	query->callback        = callback;
	query->context         = context;

	mad = query->sa_query.mad_buf->mad;
	init_mad(&query->sa_query, agent);

	query->sa_query.callback = callback ? ib_sa_mcmember_rec_callback : NULL;
	query->sa_query.release  = ib_sa_mcmember_rec_release;
	mad->mad_hdr.method	 = method;
	mad->mad_hdr.attr_id	 = cpu_to_be16(IB_SA_ATTR_MC_MEMBER_REC);
	mad->sa_hdr.comp_mask	 = comp_mask;

	ib_pack(mcmember_rec_table, ARRAY_SIZE(mcmember_rec_table),
		rec, mad->data);

	*sa_query = &query->sa_query;

	ret = send_mad(&query->sa_query, timeout_ms, gfp_mask);
	if (ret < 0)
		goto err2;

	return ret;

err2:
	*sa_query = NULL;
	ib_sa_client_put(query->sa_query.client);
	free_mad(&query->sa_query);

err1:
	kfree(query);
	return ret;
}

/* Support GuidInfoRecord */
static void ib_sa_guidinfo_rec_callback(struct ib_sa_query *sa_query,
					int status,
					struct ib_sa_mad *mad)
{
	struct ib_sa_guidinfo_query *query =
		container_of(sa_query, struct ib_sa_guidinfo_query, sa_query);

	if (mad) {
		struct ib_sa_guidinfo_rec rec;

		ib_unpack(guidinfo_rec_table, ARRAY_SIZE(guidinfo_rec_table),
			  mad->data, &rec);
		query->callback(status, &rec, query->context);
	} else
		query->callback(status, NULL, query->context);
}

static void ib_sa_guidinfo_rec_release(struct ib_sa_query *sa_query)
{
	kfree(container_of(sa_query, struct ib_sa_guidinfo_query, sa_query));
}

int ib_sa_guid_info_rec_query(struct ib_sa_client *client,
			      struct ib_device *device, u8 port_num,
			      struct ib_sa_guidinfo_rec *rec,
			      ib_sa_comp_mask comp_mask, u8 method,
			      int timeout_ms, gfp_t gfp_mask,
			      void (*callback)(int status,
					       struct ib_sa_guidinfo_rec *resp,
					       void *context),
			      void *context,
			      struct ib_sa_query **sa_query)
{
	struct ib_sa_guidinfo_query *query;
	struct ib_sa_device *sa_dev = ib_get_client_data(device, &sa_client);
	struct ib_sa_port *port;
	struct ib_mad_agent *agent;
	struct ib_sa_mad *mad;
	int ret;

	if (!sa_dev)
		return -ENODEV;

	if (method != IB_MGMT_METHOD_GET &&
	    method != IB_MGMT_METHOD_SET &&
	    method != IB_SA_METHOD_DELETE) {
		return -EINVAL;
	}

	port  = &sa_dev->port[port_num - sa_dev->start_port];
	agent = port->agent;

	query = kzalloc(sizeof(*query), gfp_mask);
	if (!query)
		return -ENOMEM;

	query->sa_query.port = port;
	ret = alloc_mad(&query->sa_query, gfp_mask);
	if (ret)
		goto err1;

	ib_sa_client_get(client);
	query->sa_query.client = client;
	query->callback        = callback;
	query->context         = context;

	mad = query->sa_query.mad_buf->mad;
	init_mad(&query->sa_query, agent);

	query->sa_query.callback = callback ? ib_sa_guidinfo_rec_callback : NULL;
	query->sa_query.release  = ib_sa_guidinfo_rec_release;

	mad->mad_hdr.method	 = method;
	mad->mad_hdr.attr_id	 = cpu_to_be16(IB_SA_ATTR_GUID_INFO_REC);
	mad->sa_hdr.comp_mask	 = comp_mask;

	ib_pack(guidinfo_rec_table, ARRAY_SIZE(guidinfo_rec_table), rec,
		mad->data);

	*sa_query = &query->sa_query;

	ret = send_mad(&query->sa_query, timeout_ms, gfp_mask);
	if (ret < 0)
		goto err2;

	return ret;

err2:
	*sa_query = NULL;
	ib_sa_client_put(query->sa_query.client);
	free_mad(&query->sa_query);

err1:
	kfree(query);
	return ret;
}
EXPORT_SYMBOL(ib_sa_guid_info_rec_query);

bool ib_sa_sendonly_fullmem_support(struct ib_sa_client *client,
				    struct ib_device *device,
				    u8 port_num)
{
	struct ib_sa_device *sa_dev = ib_get_client_data(device, &sa_client);
	struct ib_sa_port *port;
	bool ret = false;
	unsigned long flags;

	if (!sa_dev)
		return ret;

	port  = &sa_dev->port[port_num - sa_dev->start_port];

	spin_lock_irqsave(&port->classport_lock, flags);
	if ((port->classport_info.valid) &&
	    (port->classport_info.data.type == RDMA_CLASS_PORT_INFO_IB))
		ret = ib_get_cpi_capmask2(&port->classport_info.data.ib)
			& IB_SA_CAP_MASK2_SENDONLY_FULL_MEM_SUPPORT;
	spin_unlock_irqrestore(&port->classport_lock, flags);
	return ret;
}
EXPORT_SYMBOL(ib_sa_sendonly_fullmem_support);

struct ib_classport_info_context {
	struct completion	done;
	struct ib_sa_query	*sa_query;
};

static void ib_classportinfo_cb(void *context)
{
	struct ib_classport_info_context *cb_ctx = context;

	complete(&cb_ctx->done);
}

static void ib_sa_classport_info_rec_callback(struct ib_sa_query *sa_query,
					      int status,
					      struct ib_sa_mad *mad)
{
	unsigned long flags;
	struct ib_sa_classport_info_query *query =
		container_of(sa_query, struct ib_sa_classport_info_query, sa_query);
	struct ib_sa_classport_cache *info = &sa_query->port->classport_info;

	if (mad) {
		if (sa_query->flags & IB_SA_QUERY_OPA) {
			struct opa_class_port_info rec;

			ib_unpack(opa_classport_info_rec_table,
				  ARRAY_SIZE(opa_classport_info_rec_table),
				  mad->data, &rec);

			spin_lock_irqsave(&sa_query->port->classport_lock,
					  flags);
			if (!status && !info->valid) {
				memcpy(&info->data.opa, &rec,
				       sizeof(info->data.opa));

				info->valid = true;
				info->data.type = RDMA_CLASS_PORT_INFO_OPA;
			}
			spin_unlock_irqrestore(&sa_query->port->classport_lock,
					       flags);

		} else {
			struct ib_class_port_info rec;

			ib_unpack(ib_classport_info_rec_table,
				  ARRAY_SIZE(ib_classport_info_rec_table),
				  mad->data, &rec);

			spin_lock_irqsave(&sa_query->port->classport_lock,
					  flags);
			if (!status && !info->valid) {
				memcpy(&info->data.ib, &rec,
				       sizeof(info->data.ib));

				info->valid = true;
				info->data.type = RDMA_CLASS_PORT_INFO_IB;
			}
			spin_unlock_irqrestore(&sa_query->port->classport_lock,
					       flags);
		}
	}
	query->callback(query->context);
}

static void ib_sa_classport_info_rec_release(struct ib_sa_query *sa_query)
{
	kfree(container_of(sa_query, struct ib_sa_classport_info_query,
			   sa_query));
}

static int ib_sa_classport_info_rec_query(struct ib_sa_port *port,
					  int timeout_ms,
					  void (*callback)(void *context),
					  void *context,
					  struct ib_sa_query **sa_query)
{
	struct ib_mad_agent *agent;
	struct ib_sa_classport_info_query *query;
	struct ib_sa_mad *mad;
	gfp_t gfp_mask = GFP_KERNEL;
	int ret;

	agent = port->agent;

	query = kzalloc(sizeof(*query), gfp_mask);
	if (!query)
		return -ENOMEM;

	query->sa_query.port = port;
	query->sa_query.flags |= rdma_cap_opa_ah(port->agent->device,
						 port->port_num) ?
				 IB_SA_QUERY_OPA : 0;
	ret = alloc_mad(&query->sa_query, gfp_mask);
	if (ret)
		goto err_free;

	query->callback = callback;
	query->context = context;

	mad = query->sa_query.mad_buf->mad;
	init_mad(&query->sa_query, agent);

	query->sa_query.callback = ib_sa_classport_info_rec_callback;
	query->sa_query.release  = ib_sa_classport_info_rec_release;
	mad->mad_hdr.method	 = IB_MGMT_METHOD_GET;
	mad->mad_hdr.attr_id	 = cpu_to_be16(IB_SA_ATTR_CLASS_PORTINFO);
	mad->sa_hdr.comp_mask	 = 0;
	*sa_query = &query->sa_query;

	ret = send_mad(&query->sa_query, timeout_ms, gfp_mask);
	if (ret < 0)
		goto err_free_mad;

	return ret;

err_free_mad:
	*sa_query = NULL;
	free_mad(&query->sa_query);

err_free:
	kfree(query);
	return ret;
}

static void update_ib_cpi(struct work_struct *work)
{
	struct ib_sa_port *port =
		container_of(work, struct ib_sa_port, ib_cpi_work.work);
	struct ib_classport_info_context *cb_context;
	unsigned long flags;
	int ret;

	/* If the classport info is valid, nothing
	 * to do here.
	 */
	spin_lock_irqsave(&port->classport_lock, flags);
	if (port->classport_info.valid) {
		spin_unlock_irqrestore(&port->classport_lock, flags);
		return;
	}
	spin_unlock_irqrestore(&port->classport_lock, flags);

	cb_context = kmalloc(sizeof(*cb_context), GFP_KERNEL);
	if (!cb_context)
		goto err_nomem;

	init_completion(&cb_context->done);

	ret = ib_sa_classport_info_rec_query(port, 3000,
					     ib_classportinfo_cb, cb_context,
					     &cb_context->sa_query);
	if (ret < 0)
		goto free_cb_err;
	wait_for_completion(&cb_context->done);
free_cb_err:
	kfree(cb_context);
	spin_lock_irqsave(&port->classport_lock, flags);

	/* If the classport info is still not valid, the query should have
	 * failed for some reason. Retry issuing the query
	 */
	if (!port->classport_info.valid) {
		port->classport_info.retry_cnt++;
		if (port->classport_info.retry_cnt <=
		    IB_SA_CPI_MAX_RETRY_CNT) {
			unsigned long delay =
				msecs_to_jiffies(IB_SA_CPI_RETRY_WAIT);

			queue_delayed_work(ib_wq, &port->ib_cpi_work, delay);
		}
	}
	spin_unlock_irqrestore(&port->classport_lock, flags);

err_nomem:
	return;
}

static void send_handler(struct ib_mad_agent *agent,
			 struct ib_mad_send_wc *mad_send_wc)
{
	struct ib_sa_query *query = mad_send_wc->send_buf->context[0];
	unsigned long flags;

	if (query->callback)
		switch (mad_send_wc->status) {
		case IB_WC_SUCCESS:
			/* No callback -- already got recv */
			break;
		case IB_WC_RESP_TIMEOUT_ERR:
			query->callback(query, -ETIMEDOUT, NULL);
			break;
		case IB_WC_WR_FLUSH_ERR:
			query->callback(query, -EINTR, NULL);
			break;
		default:
			query->callback(query, -EIO, NULL);
			break;
		}

	spin_lock_irqsave(&idr_lock, flags);
	idr_remove(&query_idr, query->id);
	spin_unlock_irqrestore(&idr_lock, flags);

	free_mad(query);
	if (query->client)
		ib_sa_client_put(query->client);
	query->release(query);
}

static void recv_handler(struct ib_mad_agent *mad_agent,
			 struct ib_mad_send_buf *send_buf,
			 struct ib_mad_recv_wc *mad_recv_wc)
{
	struct ib_sa_query *query;

	if (!send_buf)
		return;

	query = send_buf->context[0];
	if (query->callback) {
		if (mad_recv_wc->wc->status == IB_WC_SUCCESS)
			query->callback(query,
					mad_recv_wc->recv_buf.mad->mad_hdr.status ?
					-EINVAL : 0,
					(struct ib_sa_mad *) mad_recv_wc->recv_buf.mad);
		else
			query->callback(query, -EIO, NULL);
	}

	ib_free_recv_mad(mad_recv_wc);
}

static void update_sm_ah(struct work_struct *work)
{
	struct ib_sa_port *port =
		container_of(work, struct ib_sa_port, update_task);
	struct ib_sa_sm_ah *new_ah;
	struct ib_port_attr port_attr;
	struct rdma_ah_attr   ah_attr;

	if (ib_query_port(port->agent->device, port->port_num, &port_attr)) {
		pr_warn("Couldn't query port\n");
		return;
	}

	new_ah = kmalloc(sizeof(*new_ah), GFP_KERNEL);
	if (!new_ah)
		return;

	kref_init(&new_ah->ref);
	new_ah->src_path_mask = (1 << port_attr.lmc) - 1;

	new_ah->pkey_index = 0;
	if (ib_find_pkey(port->agent->device, port->port_num,
			 IB_DEFAULT_PKEY_FULL, &new_ah->pkey_index))
		pr_err("Couldn't find index for default PKey\n");

	memset(&ah_attr, 0, sizeof(ah_attr));
	ah_attr.type = rdma_ah_find_type(port->agent->device,
					 port->port_num);
	rdma_ah_set_dlid(&ah_attr, port_attr.sm_lid);
	rdma_ah_set_sl(&ah_attr, port_attr.sm_sl);
	rdma_ah_set_port_num(&ah_attr, port->port_num);
	if (port_attr.grh_required) {
		if (ah_attr.type == RDMA_AH_ATTR_TYPE_OPA) {
			rdma_ah_set_make_grd(&ah_attr, true);
		} else {
			rdma_ah_set_ah_flags(&ah_attr, IB_AH_GRH);
			rdma_ah_set_subnet_prefix(&ah_attr,
						  cpu_to_be64(port_attr.subnet_prefix));
			rdma_ah_set_interface_id(&ah_attr,
						 cpu_to_be64(IB_SA_WELL_KNOWN_GUID));
		}
	}

	new_ah->ah = rdma_create_ah(port->agent->qp->pd, &ah_attr,
				    RDMA_CREATE_AH_SLEEPABLE);
	if (IS_ERR(new_ah->ah)) {
		pr_warn("Couldn't create new SM AH\n");
		kfree(new_ah);
		return;
	}

	spin_lock_irq(&port->ah_lock);
	if (port->sm_ah)
		kref_put(&port->sm_ah->ref, free_sm_ah);
	port->sm_ah = new_ah;
	spin_unlock_irq(&port->ah_lock);
}

static void ib_sa_event(struct ib_event_handler *handler,
			struct ib_event *event)
{
	if (event->event == IB_EVENT_PORT_ERR    ||
	    event->event == IB_EVENT_PORT_ACTIVE ||
	    event->event == IB_EVENT_LID_CHANGE  ||
	    event->event == IB_EVENT_PKEY_CHANGE ||
	    event->event == IB_EVENT_SM_CHANGE   ||
	    event->event == IB_EVENT_CLIENT_REREGISTER) {
		unsigned long flags;
		struct ib_sa_device *sa_dev =
			container_of(handler, typeof(*sa_dev), event_handler);
		u8 port_num = event->element.port_num - sa_dev->start_port;
		struct ib_sa_port *port = &sa_dev->port[port_num];

		if (!rdma_cap_ib_sa(handler->device, port->port_num))
			return;

		spin_lock_irqsave(&port->ah_lock, flags);
		if (port->sm_ah)
			kref_put(&port->sm_ah->ref, free_sm_ah);
		port->sm_ah = NULL;
		spin_unlock_irqrestore(&port->ah_lock, flags);

		if (event->event == IB_EVENT_SM_CHANGE ||
		    event->event == IB_EVENT_CLIENT_REREGISTER ||
		    event->event == IB_EVENT_LID_CHANGE ||
		    event->event == IB_EVENT_PORT_ACTIVE) {
			unsigned long delay =
				msecs_to_jiffies(IB_SA_CPI_RETRY_WAIT);

			spin_lock_irqsave(&port->classport_lock, flags);
			port->classport_info.valid = false;
			port->classport_info.retry_cnt = 0;
			spin_unlock_irqrestore(&port->classport_lock, flags);
			queue_delayed_work(ib_wq,
					   &port->ib_cpi_work, delay);
		}
		queue_work(ib_wq, &sa_dev->port[port_num].update_task);
	}
}

static void ib_sa_add_one(struct ib_device *device)
{
	struct ib_sa_device *sa_dev;
	int s, e, i;
	int count = 0;

	s = rdma_start_port(device);
	e = rdma_end_port(device);

	sa_dev = kzalloc(sizeof *sa_dev +
			 (e - s + 1) * sizeof (struct ib_sa_port),
			 GFP_KERNEL);
	if (!sa_dev)
		return;

	sa_dev->start_port = s;
	sa_dev->end_port   = e;

	for (i = 0; i <= e - s; ++i) {
		spin_lock_init(&sa_dev->port[i].ah_lock);
		if (!rdma_cap_ib_sa(device, i + 1))
			continue;

		sa_dev->port[i].sm_ah    = NULL;
		sa_dev->port[i].port_num = i + s;

		spin_lock_init(&sa_dev->port[i].classport_lock);
		sa_dev->port[i].classport_info.valid = false;

		sa_dev->port[i].agent =
			ib_register_mad_agent(device, i + s, IB_QPT_GSI,
					      NULL, 0, send_handler,
					      recv_handler, sa_dev, 0);
		if (IS_ERR(sa_dev->port[i].agent))
			goto err;

		INIT_WORK(&sa_dev->port[i].update_task, update_sm_ah);
		INIT_DELAYED_WORK(&sa_dev->port[i].ib_cpi_work,
				  update_ib_cpi);

		count++;
	}

	if (!count)
		goto free;

	ib_set_client_data(device, &sa_client, sa_dev);

	/*
	 * We register our event handler after everything is set up,
	 * and then update our cached info after the event handler is
	 * registered to avoid any problems if a port changes state
	 * during our initialization.
	 */

	INIT_IB_EVENT_HANDLER(&sa_dev->event_handler, device, ib_sa_event);
	ib_register_event_handler(&sa_dev->event_handler);

	for (i = 0; i <= e - s; ++i) {
		if (rdma_cap_ib_sa(device, i + 1))
			update_sm_ah(&sa_dev->port[i].update_task);
	}

	return;

err:
	while (--i >= 0) {
		if (rdma_cap_ib_sa(device, i + 1))
			ib_unregister_mad_agent(sa_dev->port[i].agent);
	}
free:
	kfree(sa_dev);
	return;
}

static void ib_sa_remove_one(struct ib_device *device, void *client_data)
{
	struct ib_sa_device *sa_dev = client_data;
	int i;

	if (!sa_dev)
		return;

	ib_unregister_event_handler(&sa_dev->event_handler);
	flush_workqueue(ib_wq);

	for (i = 0; i <= sa_dev->end_port - sa_dev->start_port; ++i) {
		if (rdma_cap_ib_sa(device, i + 1)) {
			cancel_delayed_work_sync(&sa_dev->port[i].ib_cpi_work);
			ib_unregister_mad_agent(sa_dev->port[i].agent);
			if (sa_dev->port[i].sm_ah)
				kref_put(&sa_dev->port[i].sm_ah->ref, free_sm_ah);
		}

	}

	kfree(sa_dev);
}

int ib_sa_init(void)
{
	int ret;

	get_random_bytes(&tid, sizeof tid);

	ret = ib_register_client(&sa_client);
	if (ret) {
		pr_err("Couldn't register ib_sa client\n");
		goto err1;
	}

	ret = mcast_init();
	if (ret) {
		pr_err("Couldn't initialize multicast handling\n");
		goto err2;
	}

	return 0;

err2:
	ib_unregister_client(&sa_client);
err1:
	return ret;
}

void ib_sa_cleanup(void)
{
	mcast_cleanup();
	ib_unregister_client(&sa_client);
	idr_destroy(&query_idr);
}
