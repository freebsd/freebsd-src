/*
 * Copyright (c) 2005 Topspin Communications.  All rights reserved.
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
 *
 * $Id$
 */

#ifndef CM_ABI_H
#define CM_ABI_H

#include <infiniband/types.h>
#include <infiniband/sa.h>
#include <infiniband/marshall.h>

/*
 * This file must be kept in sync with the kernel's version of ib_user_cm.h
 */

#define IB_USER_CM_MIN_ABI_VERSION	4
#define IB_USER_CM_MAX_ABI_VERSION	5

enum {
	IB_USER_CM_CMD_CREATE_ID,
	IB_USER_CM_CMD_DESTROY_ID,
	IB_USER_CM_CMD_ATTR_ID,

	IB_USER_CM_CMD_LISTEN,
	IB_USER_CM_CMD_NOTIFY,
	IB_USER_CM_CMD_ESTABLISH = IB_USER_CM_CMD_NOTIFY, /* ABI 4 support */
	
	IB_USER_CM_CMD_SEND_REQ,
	IB_USER_CM_CMD_SEND_REP,
	IB_USER_CM_CMD_SEND_RTU,
	IB_USER_CM_CMD_SEND_DREQ,
	IB_USER_CM_CMD_SEND_DREP,
	IB_USER_CM_CMD_SEND_REJ,
	IB_USER_CM_CMD_SEND_MRA,
	IB_USER_CM_CMD_SEND_LAP,
	IB_USER_CM_CMD_SEND_APR,
	IB_USER_CM_CMD_SEND_SIDR_REQ,
	IB_USER_CM_CMD_SEND_SIDR_REP,

	IB_USER_CM_CMD_EVENT,
	IB_USER_CM_CMD_INIT_QP_ATTR,
};
/*
 * command ABI structures.
 */
struct cm_abi_cmd_hdr {
	__u32 cmd;
	__u16 in;
	__u16 out;
};

struct cm_abi_create_id {
	__u64 uid;
	__u64 response;
};

struct cm_abi_create_id_resp {
	__u32 id;
};

struct cm_abi_destroy_id {
	__u64 response;
	__u32 id;
	__u32 reserved;
};

struct cm_abi_destroy_id_resp {
	__u32 events_reported;
};

struct cm_abi_attr_id {
	__u64 response;
	__u32 id;
	__u32 reserved;
};

struct cm_abi_attr_id_resp {
	__u64 service_id;
	__u64 service_mask;
	__u32 local_id;
	__u32 remote_id;
};

struct cm_abi_init_qp_attr {
	__u64 response;
	__u32 id;
	__u32 qp_state;
};

struct cm_abi_listen {
	__u64 service_id;
	__u64 service_mask;
	__u32 id;
	__u32 reserved;
};

struct cm_abi_establish {	/* ABI 4 support */
	__u32 id;
};

struct cm_abi_notify {
	__u32 id;
	__u32 event;
};

struct cm_abi_private_data {
	__u64 data;
	__u32 id;
	__u8  len;
	__u8  reserved[3];
};

struct cm_abi_req {
	__u32 id;
	__u32 qpn;
	__u32 qp_type;
	__u32 psn;
	__u64 sid;
	__u64 data;
	__u64 primary_path;
	__u64 alternate_path;
	__u8  len;
	__u8  peer_to_peer;
	__u8  responder_resources;
	__u8  initiator_depth;
	__u8  remote_cm_response_timeout;
	__u8  flow_control;
	__u8  local_cm_response_timeout;
	__u8  retry_count;
	__u8  rnr_retry_count;
	__u8  max_cm_retries;
	__u8  srq;
	__u8  reserved[5];
};

struct cm_abi_rep {
	__u64 uid;
	__u64 data;
	__u32 id;
	__u32 qpn;
	__u32 psn;
	__u8  len;
	__u8  responder_resources;
	__u8  initiator_depth;
	__u8  target_ack_delay;
	__u8  failover_accepted;
	__u8  flow_control;
	__u8  rnr_retry_count;
	__u8  srq;
	__u8  reserved[4];
};

struct cm_abi_info {
	__u32 id;
	__u32 status;
	__u64 info;
	__u64 data;
	__u8  info_len;
	__u8  data_len;
	__u8  reserved[6];
};

struct cm_abi_mra {
	__u64 data;
	__u32 id;
	__u8  len;
	__u8  timeout;
	__u8  reserved[2];
};

struct cm_abi_lap {
	__u64 path;
	__u64 data;
	__u32 id;
	__u8  len;
	__u8  reserved[3];
};

struct cm_abi_sidr_req {
	__u32 id;
	__u32 timeout;
	__u64 sid;
	__u64 data;
	__u64 path;
	__u16 pkey;
	__u8  len;
	__u8  max_cm_retries;
	__u8  reserved[4];
};

struct cm_abi_sidr_rep {
	__u32 id;
	__u32 qpn;
	__u32 qkey;
	__u32 status;
	__u64 info;
	__u64 data;
	__u8  info_len;
	__u8  data_len;
	__u8  reserved[6];
};
/*
 * event notification ABI structures.
 */
struct cm_abi_event_get {
	__u64 response;
	__u64 data;
	__u64 info;
	__u8  data_len;
	__u8  info_len;
	__u8  reserved[6];
};

struct cm_abi_req_event_resp {
	struct ibv_kern_path_rec primary_path;
	struct ibv_kern_path_rec alternate_path;
	__u64                  remote_ca_guid;
	__u32                  remote_qkey;
	__u32                  remote_qpn;
	__u32                  qp_type;
	__u32                  starting_psn;
	__u8  responder_resources;
	__u8  initiator_depth;
	__u8  local_cm_response_timeout;
	__u8  flow_control;
	__u8  remote_cm_response_timeout;
	__u8  retry_count;
	__u8  rnr_retry_count;
	__u8  srq;
	__u8  port;
	__u8  reserved[7];
};

struct cm_abi_rep_event_resp {
	__u64 remote_ca_guid;
	__u32 remote_qkey;
	__u32 remote_qpn;
	__u32 starting_psn;
	__u8  responder_resources;
	__u8  initiator_depth;
	__u8  target_ack_delay;
	__u8  failover_accepted;
	__u8  flow_control;
	__u8  rnr_retry_count;
	__u8  srq;
	__u8  reserved[5];
};

struct cm_abi_rej_event_resp {
	__u32 reason;
	/* ari in cm_abi_event_get info field. */
};

struct cm_abi_mra_event_resp {
	__u8  timeout;
	__u8  reserved[3];
};

struct cm_abi_lap_event_resp {
	struct ibv_kern_path_rec path;
};

struct cm_abi_apr_event_resp {
	__u32 status;
	/* apr info in cm_abi_event_get info field. */
};

struct cm_abi_sidr_req_event_resp {
	__u16 pkey;
	__u8  port;
	__u8  reserved;
};

struct cm_abi_sidr_rep_event_resp {
	__u32 status;
	__u32 qkey;
	__u32 qpn;
	/* info in cm_abi_event_get info field. */
};

#define CM_ABI_PRES_DATA      0x01
#define CM_ABI_PRES_INFO      0x02
#define CM_ABI_PRES_PRIMARY   0x04
#define CM_ABI_PRES_ALTERNATE 0x08

struct cm_abi_event_resp {
	__u64 uid;
	__u32 id;
	__u32 event;
	__u32 present;
	__u32 reserved;
	union {
		struct cm_abi_req_event_resp req_resp;
		struct cm_abi_rep_event_resp rep_resp;
		struct cm_abi_rej_event_resp rej_resp;
		struct cm_abi_mra_event_resp mra_resp;
		struct cm_abi_lap_event_resp lap_resp;
		struct cm_abi_apr_event_resp apr_resp;

		struct cm_abi_sidr_req_event_resp sidr_req_resp;
		struct cm_abi_sidr_rep_event_resp sidr_rep_resp;

		__u32                             send_status;
	} u;
};

#endif /* CM_ABI_H */
