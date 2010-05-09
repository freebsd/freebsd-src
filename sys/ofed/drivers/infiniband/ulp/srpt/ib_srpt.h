/*
 * Copyright (c) 2006 - 2009 Mellanox Technology Inc.  All rights reserved.
 * Copyright (C) 2009 - 2010 Bart Van Assche <bart.vanassche@gmail.com>
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

#ifndef IB_SRPT_H
#define IB_SRPT_H

#include <linux/version.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/mutex.h>

#include <rdma/ib_verbs.h>
#include <rdma/ib_sa.h>
#include <rdma/ib_cm.h>

#include <scsi/srp.h>

#include <scst.h>

#include "ib_dm_mad.h"

/*
 * The prefix the ServiceName field of the ServiceName/ServiceID pair in the
 * device management ServiceEntries attribute pair must start with, as specified
 * in the SRP r16a document.
 */
#define SRP_SERVICE_NAME_PREFIX		"SRP.T10:"

enum {
	/*
	 * SRP IOControllerProfile attributes for SRP target ports that have
	 * not been defined in <scsi/srp.h>. Source: section B.7, table B.7
	 * in the T10 SRP r16a document.
	 */
	SRP_PROTOCOL = 0x0108,
	SRP_PROTOCOL_VERSION = 0x0001,
	SRP_IO_SUBCLASS = 0x609e,
	SRP_SEND_TO_IOC = 0x01,
	SRP_SEND_FROM_IOC = 0x02,
	SRP_RDMA_READ_FROM_IOC = 0x08,
	SRP_RDMA_WRITE_FROM_IOC = 0x20,

	/*
	 * srp_login_cmd::req_flags bitmasks. See also table 9 in the T10 r16a
	 * document.
	 */
	SRP_MTCH_ACTION = 0x03, /* MULTI-CHANNEL ACTION */
	SRP_LOSOLNT = 0x10, /* logout solicited notification */
	SRP_CRSOLNT = 0x20, /* credit request solicited notification */
	SRP_AESOLNT = 0x40, /* asynchronous event solicited notification */

	/*
	 * srp_cmd::sol_nt / srp_tsk_mgmt::sol_not bitmasks. See also tables
	 * 18 and 20 in the T10 r16a document.
	 */
	SRP_SCSOLNT = 0x02, /* SCSOLNT = successful solicited notification */
	SRP_UCSOLNT = 0x04, /* UCSOLNT = unsuccessful solicited notification */

	/*
	 * srp_rsp::sol_not / srp_t_logout::sol_not bitmasks. See also tables
	 * 16 and 22 in the T10 r16a document.
	 */
	SRP_SOLNT = 0x01, /* SOLNT = solicited notification */

	/* See also table 24 in the T10 r16a document. */
	SRP_TSK_MGMT_SUCCESS = 0x00,
	SRP_TSK_MGMT_FUNC_NOT_SUPP = 0x04,
	SRP_TSK_MGMT_FAILED = 0x05,

	/* See also table 21 in the T10 r16a document. */
	SRP_CMD_SIMPLE_Q = 0x0,
	SRP_CMD_HEAD_OF_Q = 0x1,
	SRP_CMD_ORDERED_Q = 0x2,
	SRP_CMD_ACA = 0x4,

	SRP_LOGIN_RSP_MULTICHAN_NO_CHAN = 0x0,
	SRP_LOGIN_RSP_MULTICHAN_TERMINATED = 0x1,
	SRP_LOGIN_RSP_MULTICHAN_MAINTAINED = 0x2,

	SRPT_DEF_SG_TABLESIZE = 128,
	SRPT_DEF_SG_PER_WQE = 16,

	SRPT_SQ_SIZE = 128 * SRPT_DEF_SG_PER_WQE,
	SRPT_RQ_SIZE = 128,
	SRPT_SRQ_SIZE = 4095,

	MIN_MAX_MESSAGE_SIZE = 996,
	DEFAULT_MAX_MESSAGE_SIZE
		= sizeof(struct srp_cmd)/*48*/
		+ sizeof(struct srp_indirect_buf)/*20*/
		+ 128 * sizeof(struct srp_direct_buf)/*16*/,

	DEFAULT_MAX_RDMA_SIZE = 65536,
};

/* wr_id / wc_id flag for marking receive operations. */
#define SRPT_OP_RECV			(1 << 31)


struct rdma_iu {
	u64 raddr;
	u32 rkey;
	struct ib_sge *sge;
	u32 sge_cnt;
	int mem_id;
};

/**
 * enum srpt_command_state - SCSI command states managed by SRPT.
 * @SRPT_STATE_NEW: New command arrived and is being processed.
 * @SRPT_STATE_NEED_DATA: Processing a write or bidir command and waiting for
 *   data arrival.
 * @SRPT_STATE_DATA_IN: Data for the write or bidir command arrived and is
 *   being processed.
 * @SRPT_STATE_CMD_RSP_SENT: SRP_RSP for SRP_CMD has been sent.
 * @SRPT_STATE_MGMT_RSP_SENT: SRP_RSP for SRP_TSK_MGMT has been sent.
 * @SRPT_STATE_DONE: Command processing finished successfully, command
 *   processing has been aborted or command processing failed.
 */
enum srpt_command_state {
	SRPT_STATE_NEW = 0,
	SRPT_STATE_NEED_DATA = 1,
	SRPT_STATE_DATA_IN = 2,
	SRPT_STATE_CMD_RSP_SENT = 3,
	SRPT_STATE_MGMT_RSP_SENT = 4,
	SRPT_STATE_DONE = 5,
};

/* SRPT I/O context: SRPT-private data associated with a struct scst_cmd. */
struct srpt_ioctx {
	int index;
	void *buf;
	dma_addr_t dma;
	struct rdma_iu *rdma_ius;
	struct srp_direct_buf *rbufs;
	struct srp_direct_buf single_rbuf;
	/* Node for insertion in srpt_rdma_ch::cmd_wait_list. */
	struct list_head wait_list;
	int mapped_sg_count;
	u16 n_rdma_ius;
	u8 n_rdma;
	u8 n_rbuf;

	enum ib_wc_opcode op;
	/* Node for insertion in the srpt_thread::thread_ioctx_list. */
	struct list_head comp_list;
	struct srpt_rdma_ch *ch;
	struct scst_cmd *scmnd;
	atomic_t state; /*enum srpt_command_state*/
};

/* Additional context information for SCST management commands. */
struct srpt_mgmt_ioctx {
	struct srpt_ioctx *ioctx;
	struct srpt_rdma_ch *ch;
	u64 tag;
};

/* channel state */
enum rdma_ch_state {
	RDMA_CHANNEL_CONNECTING,
	RDMA_CHANNEL_LIVE,
	RDMA_CHANNEL_DISCONNECTING
};

struct srpt_rdma_ch {
	struct ib_cm_id *cm_id;
	/* IB queue pair. */
	struct ib_qp *qp;
	/* Number of WR's in the QP 'qp' that are not in use. */
	atomic_t qp_wr_avail;
	struct ib_cq *cq;
	struct srpt_port *sport;
	/* 128-bit initiator port identifier copied from SRP_LOGIN_REQ. */
	u8 i_port_id[16];
	/* 128-bit target port identifier copied from SRP_LOGIN_REQ. */
	u8 t_port_id[16];
	int max_ti_iu_len;
	/*
	 * Request limit: maximum number of request that may be sent by
	 * the initiator without having received a response or SRP_CRED_REQ.
	 */
	atomic_t req_lim;
	/*
	 * Value of req_lim the last time a response or SRP_CRED_REQ was sent.
	 */
	atomic_t last_response_req_lim;
	/* Channel state -- see also enum rdma_ch_state. */
	atomic_t state;
	/* Node for insertion in the srpt_device::rch_list list. */
	struct list_head list;
	/*
	 * List of SCST commands that arrived before the RTU event.
	 * This list contains struct srpt_ioctx elements and is protected
	 * against concurrent modification by the cm_id spinlock.
	 */
	struct list_head cmd_wait_list;

	struct scst_session *scst_sess;
	u8 sess_name[36];
};

struct srpt_port {
	struct srpt_device *sdev;
	struct ib_mad_agent *mad_agent;
	/* One-based port number. */
	u8 port;
	/* Cached values of the port's sm_lid, lid and gid. */
	u16 sm_lid;
	u16 lid;
	union ib_gid gid;
	/* Work structure for refreshing the aforementioned cached values. */
	struct work_struct work;
};

/*
 * Data stored by the ib_srpt kernel module per InfiniBand device
 * (struct ib_device).
 */
struct srpt_device {
	/* Backpointer to the struct ib_device managed by the IB core. */
	struct ib_device *device;
	/* Protection domain. */
	struct ib_pd *pd;
	/* L_Key (local key) with write access to all local memory. */
	struct ib_mr *mr;
	/* SRQ (shared receive queue). */
	struct ib_srq *srq;
	/* Connection identifier. */
	struct ib_cm_id *cm_id;
	/*
	 * Attributes of the InfiniBand device as obtained during the
	 * ib_client::add() callback.
	 */
	struct ib_device_attr dev_attr;
	/* I/O context ring. */
	struct srpt_ioctx *ioctx_ring[SRPT_SRQ_SIZE];
	/*
	 * List node for insertion in the srpt_rdma_ch::list list.
	 * This list is protected by srpt_device::spinlock.
	 */
	struct list_head rch_list;
	spinlock_t spinlock;
	struct srpt_port port[2];
	struct ib_event_handler event_handler;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 26)
	/* per-port srpt-<portname> device instance. */
	struct class_device class_dev;
#else
	struct device dev;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 30)
	char init_name[20];
#endif

	struct scst_tgt *scst_tgt;
};

#endif				/* IB_SRPT_H */
