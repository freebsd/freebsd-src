/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Microsoft Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

#ifndef _MANA_H
#define _MANA_H

#include <sys/types.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <sys/counter.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_media.h>
#include <netinet/tcp_lro.h>

#include "gdma.h"
#include "hw_channel.h"


/* Microsoft Azure Network Adapter (MANA)'s definitions
 *
 * Structures labeled with "HW DATA" are exchanged with the hardware. All of
 * them are naturally aligned and hence don't need __packed.
 */
/* MANA protocol version */
#define MANA_MAJOR_VERSION	0
#define MANA_MINOR_VERSION	1
#define MANA_MICRO_VERSION	1

#define DRV_MODULE_NAME		"mana"

#ifndef DRV_MODULE_VERSION
#define DRV_MODULE_VERSION				\
	__XSTRING(MANA_MAJOR_VERSION) "."		\
	__XSTRING(MANA_MINOR_VERSION) "."		\
	__XSTRING(MANA_MICRO_VERSION)
#endif
#define DEVICE_NAME	"Microsoft Azure Network Adapter (MANA)"
#define DEVICE_DESC	"MANA adapter"

/*
 * Supported PCI vendor and devices IDs
 */
#ifndef PCI_VENDOR_ID_MICROSOFT
#define PCI_VENDOR_ID_MICROSOFT	0x1414
#endif

#define PCI_DEV_ID_MANA_VF	0x00ba

typedef struct _mana_vendor_id_t {
	uint16_t vendor_id;
	uint16_t device_id;
} mana_vendor_id_t;

typedef uint64_t mana_handle_t;
#define INVALID_MANA_HANDLE	((mana_handle_t)-1)

enum TRI_STATE {
	TRI_STATE_UNKNOWN = -1,
	TRI_STATE_FALSE = 0,
	TRI_STATE_TRUE = 1
};

/* Number of entries for hardware indirection table must be in power of 2 */
#define MANA_INDIRECT_TABLE_SIZE	64
#define MANA_INDIRECT_TABLE_MASK	(MANA_INDIRECT_TABLE_SIZE - 1)

/* The Toeplitz hash key's length in bytes: should be multiple of 8 */
#define MANA_HASH_KEY_SIZE		40

#define COMP_ENTRY_SIZE			64

#define MIN_FRAME_SIZE			146
#define ADAPTER_MTU_SIZE		1500
#define DEFAULT_FRAME_SIZE		(ADAPTER_MTU_SIZE + 14)
#define MAX_FRAME_SIZE			4096

#define RX_BUFFERS_PER_QUEUE		512

#define MAX_SEND_BUFFERS_PER_QUEUE	256

#define EQ_SIZE				(8 * PAGE_SIZE)
#define LOG2_EQ_THROTTLE		3

#define MAX_PORTS_IN_MANA_DEV		8

struct mana_send_buf_info {
	struct mbuf			*mbuf;
	bus_dmamap_t			dma_map;

	/* Required to store the result of mana_gd_post_work_request.
	 * gdma_posted_wqe_info.wqe_size_in_bu is required for progressing the
	 * work queue when the WQE is consumed.
	 */
	struct gdma_posted_wqe_info	wqe_inf;
};

struct mana_stats {
	counter_u64_t			packets;		/* rx, tx */
	counter_u64_t			bytes;			/* rx, tx */
	counter_u64_t			stop;			/* tx */
	counter_u64_t			wakeup;			/* tx */
	counter_u64_t			collapse;		/* tx */
	counter_u64_t			collapse_err;		/* tx */
	counter_u64_t			dma_mapping_err;	/* rx, tx */
	counter_u64_t			mbuf_alloc_fail;	/* rx */
	counter_u64_t			alt_chg;		/* tx */
	counter_u64_t			alt_reset;		/* tx */
};

struct mana_txq {
	struct gdma_queue	*gdma_sq;

	union {
		uint32_t	gdma_txq_id;
		struct {
			uint32_t	reserved1	:10;
			uint32_t	vsq_frame	:14;
			uint32_t	reserved2	:8;
		};
	};

	uint16_t		vp_offset;

	struct ifnet		*ndev;
	/* Store index to the array of tx_qp in port structure */
	int			idx;
	/* The alternative txq idx when this txq is under heavy load */
	int			alt_txq_idx;

	/* The mbufs are sent to the HW and we are waiting for the CQEs. */
	struct mana_send_buf_info	*tx_buf_info;
	uint16_t		next_to_use;
	uint16_t		next_to_complete;

	atomic_t		pending_sends;

	struct buf_ring		*txq_br;
	struct mtx		txq_mtx;
	char			txq_mtx_name[16];

	struct task		enqueue_task;
	struct taskqueue	*enqueue_tq;

	struct mana_stats	stats;
};


/*
 * Max WQE size is 512B. The first 8B is for GDMA Out of Band (OOB),
 * next is the Client OOB can be either 8B or 24B. Thus, the max
 * space for SGL entries in a singel WQE is 512 - 8 - 8 = 496B. Since each
 * SGL is 16B in size, the max number of SGLs in a WQE is 496/16 = 31.
 * Save one for emergency use, set the MAX_MBUF_FRAGS allowed to 30.
 */
#define	MAX_MBUF_FRAGS		30
#define MANA_TSO_MAXSEG_SZ	PAGE_SIZE

/* mbuf data and frags dma mappings */
struct mana_mbuf_head {
	bus_addr_t dma_handle[MAX_MBUF_FRAGS + 1];

	uint32_t size[MAX_MBUF_FRAGS + 1];
};

#define MANA_HEADROOM		sizeof(struct mana_mbuf_head)

enum mana_tx_pkt_format {
	MANA_SHORT_PKT_FMT	= 0,
	MANA_LONG_PKT_FMT	= 1,
};

struct mana_tx_short_oob {
	uint32_t pkt_fmt		:2;
	uint32_t is_outer_ipv4		:1;
	uint32_t is_outer_ipv6		:1;
	uint32_t comp_iphdr_csum	:1;
	uint32_t comp_tcp_csum		:1;
	uint32_t comp_udp_csum		:1;
	uint32_t supress_txcqe_gen	:1;
	uint32_t vcq_num		:24;

	uint32_t trans_off		:10; /* Transport header offset */
	uint32_t vsq_frame		:14;
	uint32_t short_vp_offset	:8;
}; /* HW DATA */

struct mana_tx_long_oob {
	uint32_t is_encap		:1;
	uint32_t inner_is_ipv6		:1;
	uint32_t inner_tcp_opt		:1;
	uint32_t inject_vlan_pri_tag	:1;
	uint32_t reserved1		:12;
	uint32_t pcp			:3;  /* 802.1Q */
	uint32_t dei			:1;  /* 802.1Q */
	uint32_t vlan_id		:12; /* 802.1Q */

	uint32_t inner_frame_offset	:10;
	uint32_t inner_ip_rel_offset	:6;
	uint32_t long_vp_offset		:12;
	uint32_t reserved2		:4;

	uint32_t reserved3;
	uint32_t reserved4;
}; /* HW DATA */

struct mana_tx_oob {
	struct mana_tx_short_oob	s_oob;
	struct mana_tx_long_oob		l_oob;
}; /* HW DATA */

enum mana_cq_type {
	MANA_CQ_TYPE_RX,
	MANA_CQ_TYPE_TX,
};

enum mana_cqe_type {
	CQE_INVALID			= 0,
	CQE_RX_OKAY			= 1,
	CQE_RX_COALESCED_4		= 2,
	CQE_RX_OBJECT_FENCE		= 3,
	CQE_RX_TRUNCATED		= 4,

	CQE_TX_OKAY			= 32,
	CQE_TX_SA_DROP			= 33,
	CQE_TX_MTU_DROP			= 34,
	CQE_TX_INVALID_OOB		= 35,
	CQE_TX_INVALID_ETH_TYPE		= 36,
	CQE_TX_HDR_PROCESSING_ERROR	= 37,
	CQE_TX_VF_DISABLED		= 38,
	CQE_TX_VPORT_IDX_OUT_OF_RANGE	= 39,
	CQE_TX_VPORT_DISABLED		= 40,
	CQE_TX_VLAN_TAGGING_VIOLATION	= 41,
};

#define MANA_CQE_COMPLETION	1

struct mana_cqe_header {
	uint32_t cqe_type	:6;
	uint32_t client_type	:2;
	uint32_t vendor_err	:24;
}; /* HW DATA */

/* NDIS HASH Types */
#define NDIS_HASH_IPV4		BIT(0)
#define NDIS_HASH_TCP_IPV4	BIT(1)
#define NDIS_HASH_UDP_IPV4	BIT(2)
#define NDIS_HASH_IPV6		BIT(3)
#define NDIS_HASH_TCP_IPV6	BIT(4)
#define NDIS_HASH_UDP_IPV6	BIT(5)
#define NDIS_HASH_IPV6_EX	BIT(6)
#define NDIS_HASH_TCP_IPV6_EX	BIT(7)
#define NDIS_HASH_UDP_IPV6_EX	BIT(8)

#define MANA_HASH_L3 (NDIS_HASH_IPV4 | NDIS_HASH_IPV6 | NDIS_HASH_IPV6_EX)
#define MANA_HASH_L4                                                         \
	(NDIS_HASH_TCP_IPV4 | NDIS_HASH_UDP_IPV4 | NDIS_HASH_TCP_IPV6 |      \
	 NDIS_HASH_UDP_IPV6 | NDIS_HASH_TCP_IPV6_EX | NDIS_HASH_UDP_IPV6_EX)

#define NDIS_HASH_IPV4_L3_MASK	(NDIS_HASH_IPV4)
#define NDIS_HASH_IPV4_L4_MASK	(NDIS_HASH_TCP_IPV4 | NDIS_HASH_UDP_IPV4)
#define NDIS_HASH_IPV6_L3_MASK	(NDIS_HASH_IPV6 | NDIS_HASH_IPV6_EX)
#define NDIS_HASH_IPV6_L4_MASK						\
    (NDIS_HASH_TCP_IPV6 | NDIS_HASH_UDP_IPV6 |				\
    NDIS_HASH_TCP_IPV6_EX | NDIS_HASH_UDP_IPV6_EX)
#define NDIS_HASH_IPV4_MASK						\
    (NDIS_HASH_IPV4_L3_MASK | NDIS_HASH_IPV4_L4_MASK)
#define NDIS_HASH_IPV6_MASK						\
    (NDIS_HASH_IPV6_L3_MASK | NDIS_HASH_IPV6_L4_MASK)


struct mana_rxcomp_perpkt_info {
	uint32_t pkt_len	:16;
	uint32_t reserved1	:16;
	uint32_t reserved2;
	uint32_t pkt_hash;
}; /* HW DATA */

#define MANA_RXCOMP_OOB_NUM_PPI 4

/* Receive completion OOB */
struct mana_rxcomp_oob {
	struct mana_cqe_header cqe_hdr;

	uint32_t rx_vlan_id			:12;
	uint32_t rx_vlantag_present		:1;
	uint32_t rx_outer_iphdr_csum_succeed	:1;
	uint32_t rx_outer_iphdr_csum_fail	:1;
	uint32_t reserved1			:1;
	uint32_t rx_hashtype			:9;
	uint32_t rx_iphdr_csum_succeed		:1;
	uint32_t rx_iphdr_csum_fail		:1;
	uint32_t rx_tcp_csum_succeed		:1;
	uint32_t rx_tcp_csum_fail		:1;
	uint32_t rx_udp_csum_succeed		:1;
	uint32_t rx_udp_csum_fail		:1;
	uint32_t reserved2			:1;

	struct mana_rxcomp_perpkt_info ppi[MANA_RXCOMP_OOB_NUM_PPI];

	uint32_t rx_wqe_offset;
}; /* HW DATA */

struct mana_tx_comp_oob {
	struct mana_cqe_header	cqe_hdr;

	uint32_t tx_data_offset;

	uint32_t tx_sgl_offset		:5;
	uint32_t tx_wqe_offset		:27;

	uint32_t reserved[12];
}; /* HW DATA */

struct mana_rxq;

#define CQE_POLLING_BUFFER	512

struct mana_cq {
	struct gdma_queue	*gdma_cq;

	/* Cache the CQ id (used to verify if each CQE comes to the right CQ. */
	uint32_t		gdma_id;

	/* Type of the CQ: TX or RX */
	enum mana_cq_type	type;

	/* Pointer to the mana_rxq that is pushing RX CQEs to the queue.
	 * Only and must be non-NULL if type is MANA_CQ_TYPE_RX.
	 */
	struct mana_rxq		*rxq;

	/* Pointer to the mana_txq that is pushing TX CQEs to the queue.
	 * Only and must be non-NULL if type is MANA_CQ_TYPE_TX.
	 */
	struct mana_txq		*txq;

	/* Taskqueue and related structs */
	struct task		cleanup_task;
	struct taskqueue	*cleanup_tq;
	int			cpu;
	bool			do_not_ring_db;

	/* Budget for one cleanup task */
	int			work_done;
	int			budget;

	/* Buffer which the CQ handler can copy the CQE's into. */
	struct gdma_comp	gdma_comp_buf[CQE_POLLING_BUFFER];
};

struct mana_recv_buf_oob {
	/* A valid GDMA work request representing the data buffer. */
	struct gdma_wqe_request		wqe_req;

	struct mbuf			*mbuf;
	bus_dmamap_t			dma_map;

	/* SGL of the buffer going to be sent as part of the work request. */
	uint32_t			num_sge;
	struct gdma_sge			sgl[MAX_RX_WQE_SGL_ENTRIES];

	/* Required to store the result of mana_gd_post_work_request.
	 * gdma_posted_wqe_info.wqe_size_in_bu is required for progressing the
	 * work queue when the WQE is consumed.
	 */
	struct gdma_posted_wqe_info	wqe_inf;
};

struct mana_rxq {
	struct gdma_queue		*gdma_rq;
	/* Cache the gdma receive queue id */
	uint32_t			gdma_id;

	/* Index of RQ in the vPort, not gdma receive queue id */
	uint32_t			rxq_idx;

	uint32_t			datasize;

	mana_handle_t			rxobj;

	struct completion		fence_event;

	struct mana_cq			rx_cq;

	struct ifnet			*ndev;
	struct lro_ctrl			lro;

	/* Total number of receive buffers to be allocated */
	uint32_t			num_rx_buf;

	uint32_t			buf_index;

	struct mana_stats		stats;

	/* MUST BE THE LAST MEMBER:
	 * Each receive buffer has an associated mana_recv_buf_oob.
	 */
	struct mana_recv_buf_oob	rx_oobs[];
};

struct mana_tx_qp {
	struct mana_txq			txq;

	struct mana_cq			tx_cq;

	mana_handle_t			tx_object;
};

struct mana_port_stats {
	counter_u64_t		rx_packets;
	counter_u64_t		tx_packets;

	counter_u64_t		rx_bytes;
	counter_u64_t		tx_bytes;

	counter_u64_t		rx_drops;
	counter_u64_t		tx_drops;

	counter_u64_t		stop_queue;
	counter_u64_t		wake_queue;
};

struct mana_context {
	struct gdma_dev		*gdma_dev;

	uint16_t		num_ports;

	struct mana_eq		*eqs;

	struct ifnet		*ports[MAX_PORTS_IN_MANA_DEV];
};

struct mana_port_context {
	struct mana_context	*ac;
	struct ifnet		*ndev;
	struct ifmedia		media;

	struct sx		apc_lock;

	/* DMA tag used for queue bufs of the entire port */
	bus_dma_tag_t		rx_buf_tag;
	bus_dma_tag_t		tx_buf_tag;

	uint8_t			mac_addr[ETHER_ADDR_LEN];

	enum TRI_STATE		rss_state;

	mana_handle_t		default_rxobj;
	bool			tx_shortform_allowed;
	uint16_t		tx_vp_offset;

	struct mana_tx_qp	*tx_qp;

	/* Indirection Table for RX & TX. The values are queue indexes */
	uint32_t		indir_table[MANA_INDIRECT_TABLE_SIZE];

	/* Indirection table containing RxObject Handles */
	mana_handle_t		rxobj_table[MANA_INDIRECT_TABLE_SIZE];

	/*  Hash key used by the NIC */
	uint8_t			hashkey[MANA_HASH_KEY_SIZE];

	/* This points to an array of num_queues of RQ pointers. */
	struct mana_rxq		**rxqs;

	/* Create num_queues EQs, SQs, SQ-CQs, RQs and RQ-CQs, respectively. */
	unsigned int		max_queues;
	unsigned int		num_queues;

	mana_handle_t		port_handle;

	int			vport_use_count;

	uint16_t		port_idx;

	uint16_t		frame_size;

	bool			port_is_up;
	bool			port_st_save; /* Saved port state */

	bool			enable_tx_altq;

	bool			bind_cleanup_thread_cpu;
	int			last_tx_cq_bind_cpu;
	int			last_rx_cq_bind_cpu;

	struct mana_port_stats	port_stats;

	struct sysctl_oid_list	*port_list;
	struct sysctl_ctx_list	que_sysctl_ctx;
};

#define MANA_APC_LOCK_INIT(apc)			\
	sx_init(&(apc)->apc_lock, "MANA port lock")
#define MANA_APC_LOCK_DESTROY(apc)		sx_destroy(&(apc)->apc_lock)
#define MANA_APC_LOCK_LOCK(apc)			sx_xlock(&(apc)->apc_lock)
#define MANA_APC_LOCK_UNLOCK(apc)		sx_unlock(&(apc)->apc_lock)

int mana_config_rss(struct mana_port_context *ac, enum TRI_STATE rx,
    bool update_hash, bool update_tab);

int mana_alloc_queues(struct ifnet *ndev);
int mana_attach(struct ifnet *ndev);
int mana_detach(struct ifnet *ndev);

int mana_probe(struct gdma_dev *gd);
void mana_remove(struct gdma_dev *gd);

struct mana_obj_spec {
	uint32_t	queue_index;
	uint64_t	gdma_region;
	uint32_t	queue_size;
	uint32_t	attached_eq;
	uint32_t	modr_ctx_id;
};

enum mana_command_code {
	MANA_QUERY_DEV_CONFIG	= 0x20001,
	MANA_QUERY_GF_STAT	= 0x20002,
	MANA_CONFIG_VPORT_TX	= 0x20003,
	MANA_CREATE_WQ_OBJ	= 0x20004,
	MANA_DESTROY_WQ_OBJ	= 0x20005,
	MANA_FENCE_RQ		= 0x20006,
	MANA_CONFIG_VPORT_RX	= 0x20007,
	MANA_QUERY_VPORT_CONFIG	= 0x20008,
};

/* Query Device Configuration */
struct mana_query_device_cfg_req {
	struct gdma_req_hdr	hdr;

	/* Driver Capability flags */
	uint64_t		drv_cap_flags1;
	uint64_t		drv_cap_flags2;
	uint64_t		drv_cap_flags3;
	uint64_t		drv_cap_flags4;

	uint32_t		proto_major_ver;
	uint32_t		proto_minor_ver;
	uint32_t		proto_micro_ver;

	uint32_t		reserved;
}; /* HW DATA */

struct mana_query_device_cfg_resp {
	struct gdma_resp_hdr	hdr;

	uint64_t		pf_cap_flags1;
	uint64_t		pf_cap_flags2;
	uint64_t		pf_cap_flags3;
	uint64_t		pf_cap_flags4;

	uint16_t		max_num_vports;
	uint16_t		reserved;
	uint32_t		max_num_eqs;
}; /* HW DATA */

/* Query vPort Configuration */
struct mana_query_vport_cfg_req {
	struct gdma_req_hdr	hdr;
	uint32_t		vport_index;
}; /* HW DATA */

struct mana_query_vport_cfg_resp {
	struct gdma_resp_hdr	hdr;
	uint32_t		max_num_sq;
	uint32_t		max_num_rq;
	uint32_t		num_indirection_ent;
	uint32_t		reserved1;
	uint8_t			mac_addr[6];
	uint8_t			reserved2[2];
	mana_handle_t		vport;
}; /* HW DATA */

/* Configure vPort */
struct mana_config_vport_req {
	struct gdma_req_hdr	hdr;
	mana_handle_t		vport;
	uint32_t		pdid;
	uint32_t		doorbell_pageid;
}; /* HW DATA */

struct mana_config_vport_resp {
	struct gdma_resp_hdr	hdr;
	uint16_t		tx_vport_offset;
	uint8_t			short_form_allowed;
	uint8_t			reserved;
}; /* HW DATA */

/* Create WQ Object */
struct mana_create_wqobj_req {
	struct gdma_req_hdr	hdr;
	mana_handle_t		vport;
	uint32_t		wq_type;
	uint32_t		reserved;
	uint64_t		wq_gdma_region;
	uint64_t		cq_gdma_region;
	uint32_t		wq_size;
	uint32_t		cq_size;
	uint32_t		cq_moderation_ctx_id;
	uint32_t		cq_parent_qid;
}; /* HW DATA */

struct mana_create_wqobj_resp {
	struct gdma_resp_hdr	hdr;
	uint32_t		wq_id;
	uint32_t		cq_id;
	mana_handle_t		wq_obj;
}; /* HW DATA */

/* Destroy WQ Object */
struct mana_destroy_wqobj_req {
	struct gdma_req_hdr	hdr;
	uint32_t		wq_type;
	uint32_t		reserved;
	mana_handle_t		wq_obj_handle;
}; /* HW DATA */

struct mana_destroy_wqobj_resp {
	struct gdma_resp_hdr	hdr;
}; /* HW DATA */

/* Fence RQ */
struct mana_fence_rq_req {
	struct gdma_req_hdr	hdr;
	mana_handle_t		wq_obj_handle;
}; /* HW DATA */

struct mana_fence_rq_resp {
	struct gdma_resp_hdr	hdr;
}; /* HW DATA */

/* Configure vPort Rx Steering */
struct mana_cfg_rx_steer_req {
	struct gdma_req_hdr	hdr;
	mana_handle_t		vport;
	uint16_t		num_indir_entries;
	uint16_t		indir_tab_offset;
	uint32_t		rx_enable;
	uint32_t		rss_enable;
	uint8_t			update_default_rxobj;
	uint8_t			update_hashkey;
	uint8_t			update_indir_tab;
	uint8_t			reserved;
	mana_handle_t		default_rxobj;
	uint8_t			hashkey[MANA_HASH_KEY_SIZE];
}; /* HW DATA */

struct mana_cfg_rx_steer_resp {
	struct gdma_resp_hdr	hdr;
}; /* HW DATA */

#define MANA_MAX_NUM_QUEUES		16

#define MANA_SHORT_VPORT_OFFSET_MAX	((1U << 8) - 1)

struct mana_tx_package {
	struct gdma_wqe_request		wqe_req;
	struct gdma_sge			sgl_array[MAX_MBUF_FRAGS];

	struct mana_tx_oob		tx_oob;

	struct gdma_posted_wqe_info	wqe_info;
};

int mana_restart(struct mana_port_context *apc);

int mana_create_wq_obj(struct mana_port_context *apc,
    mana_handle_t vport,
    uint32_t wq_type, struct mana_obj_spec *wq_spec,
    struct mana_obj_spec *cq_spec,
    mana_handle_t *wq_obj);

void mana_destroy_wq_obj(struct mana_port_context *apc, uint32_t wq_type,
    mana_handle_t wq_obj);

int mana_cfg_vport(struct mana_port_context *apc, uint32_t protection_dom_id,
    uint32_t doorbell_pg_id);

void mana_uncfg_vport(struct mana_port_context *apc);
#endif /* _MANA_H */
