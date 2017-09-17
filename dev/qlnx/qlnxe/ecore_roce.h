/*
 * Copyright (c) 2017-2018 Cavium, Inc. 
 * All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

#ifndef __ECORE_RDMA_H__
#define __ECORE_RDMA_H__

#include "ecore_status.h"
#include "ecore.h"
#include "ecore_hsi_common.h"
#include "ecore_proto_if.h"
#include "ecore_roce_api.h"
#include "ecore_dev_api.h"

/* Constants */

/* HW/FW RoCE Limitations (internal. For external see ecore_rdma_api.h) */
#define ECORE_RDMA_MAX_FMR                    (RDMA_MAX_TIDS) /* 2^17 - 1 */
#define ECORE_RDMA_MAX_P_KEY                  (1)
#define ECORE_RDMA_MAX_WQE                    (0x7FFF) /* 2^15 -1 */
#define ECORE_RDMA_MAX_SRQ_WQE_ELEM           (0x7FFF) /* 2^15 -1 */
#define ECORE_RDMA_PAGE_SIZE_CAPS             (0xFFFFF000) /* TODO: > 4k?! */
#define ECORE_RDMA_ACK_DELAY                  (15) /* 131 milliseconds */
#define ECORE_RDMA_MAX_MR_SIZE                (0x10000000000ULL) /* 2^40 */
#define ECORE_RDMA_MAX_CQS                    (RDMA_MAX_CQS) /* 64k */
#define ECORE_RDMA_MAX_MRS                    (RDMA_MAX_TIDS) /* 2^17 - 1 */
/* Add 1 for header element */
#define ECORE_RDMA_MAX_SRQ_ELEM_PER_WQE	      (RDMA_MAX_SGE_PER_RQ_WQE + 1)
#define ECORE_RDMA_MAX_SGE_PER_SRQ_WQE	      (RDMA_MAX_SGE_PER_RQ_WQE)
#define ECORE_RDMA_SRQ_WQE_ELEM_SIZE          (16)
#define ECORE_RDMA_MAX_SRQS		      (32 * 1024) /* 32k */

/* Configurable */
/* Max CQE is derived from u16/32 size, halved and decremented by 1 to handle
 * wrap properly and then decremented by 1 again. The latter decrement comes
 * from a requirement to create a chain that is bigger than what the user
 * requested by one:
 * The CQE size is 32 bytes but the FW writes in chunks of 64
 * bytes, for performance purposes. Allocating an extra entry and telling the
 * FW we have less prevents overwriting the first entry in case of a wrap i.e.
 * when the FW writes the last entry and the application hasn't read the first
 * one.
 */
#define ECORE_RDMA_MAX_CQE_32_BIT             (0x7FFFFFFF - 1)
#define ECORE_RDMA_MAX_CQE_16_BIT             (0x7FFF - 1)

enum ecore_rdma_toggle_bit {
	ECORE_RDMA_TOGGLE_BIT_CLEAR = 0,
	ECORE_RDMA_TOGGLE_BIT_SET   = 1
};

/* @@@TBD Currently we support only affilited events
   * enum ecore_rdma_unaffiliated_event_code {
   * ECORE_RDMA_PORT_ACTIVE, // Link Up
   * ECORE_RDMA_PORT_CHANGED, // SGID table has changed
   * ECORE_RDMA_LOCAL_CATASTROPHIC_ERR, // Fatal device error
   * ECORE_RDMA_PORT_ERR, // Link down
   * };
   */

#define QEDR_MAX_BMAP_NAME	(10)
struct ecore_bmap {
	u32           max_count;
	unsigned long *bitmap;
	char name[QEDR_MAX_BMAP_NAME];
};

/* functions for enabling/disabling edpm in rdma PFs according to existence of
 * qps during DCBx update or bar size
 */
void ecore_roce_dpm_dcbx(struct ecore_hwfn *p_hwfn, struct ecore_ptt *p_ptt);
void ecore_rdma_dpm_bar(struct ecore_hwfn *p_hwfn, struct ecore_ptt *p_ptt);

#ifdef CONFIG_ECORE_IWARP

#define ECORE_IWARP_PREALLOC_CNT	(256)

#define ECORE_IWARP_LL2_SYN_TX_SIZE	(128)
#define ECORE_IWARP_LL2_SYN_RX_SIZE	(256)

#define ECORE_IWARP_LL2_OOO_DEF_TX_SIZE	(256)
#define ECORE_IWARP_LL2_OOO_DEF_RX_SIZE	(4096)
#define ECORE_IWARP_LL2_OOO_MAX_RX_SIZE	(16384)

#define ECORE_IWARP_MAX_SYN_PKT_SIZE	(128)
#define ECORE_IWARP_HANDLE_INVAL	(0xff)

struct ecore_iwarp_ll2_buff {
	struct ecore_iwarp_ll2_buff	*piggy_buf;
	void 				*data;
	dma_addr_t			data_phys_addr;
	u32				buff_size;
};

struct ecore_iwarp_ll2_mpa_buf {
	osal_list_entry_t		list_entry;
	struct ecore_iwarp_ll2_buff	*ll2_buf;
	struct unaligned_opaque_data	data;
	u16				tcp_payload_len;
	u8				placement_offset;
};

/* In some cases a fpdu will arrive with only one byte of the header, in this
 * case the fpdu_length will be partial ( contain only higher byte and
 * incomplete bytes will contain the invalid value */
#define ECORE_IWARP_INVALID_INCOMPLETE_BYTES 0xffff

struct ecore_iwarp_fpdu {
	struct ecore_iwarp_ll2_buff 	*mpa_buf;
	dma_addr_t			pkt_hdr;
	u8				pkt_hdr_size;
	dma_addr_t			mpa_frag;
	void				*mpa_frag_virt;
	u16				mpa_frag_len;
	u16				fpdu_length;
	u16				incomplete_bytes;
};

struct ecore_iwarp_info {
	osal_list_t			listen_list; /* ecore_iwarp_listener */
	osal_list_t			ep_list;     /* ecore_iwarp_ep */
	osal_list_t			ep_free_list;/* pre-allocated ep's */
	osal_list_t			mpa_buf_list;/* list of mpa_bufs */
	osal_list_t			mpa_buf_pending_list;
	osal_spinlock_t			iw_lock;
	osal_spinlock_t			qp_lock; /* for teardown races */
	struct iwarp_rxmit_stats_drv	stats;
	u32				rcv_wnd_scale;
	u16				max_mtu;
	u16				num_ooo_rx_bufs;
	u8				mac_addr[ETH_ALEN];
	u8				crc_needed;
	u8				tcp_flags;
	u8				ll2_syn_handle;
	u8				ll2_ooo_handle;
	u8				ll2_mpa_handle;
	u8				peer2peer;
	u8				_pad;
	enum mpa_negotiation_mode	mpa_rev;
	enum mpa_rtr_type		rtr_type;
	struct ecore_iwarp_fpdu		*partial_fpdus;
	struct ecore_iwarp_ll2_mpa_buf  *mpa_bufs;
	u8				*mpa_intermediate_buf;
	u16				max_num_partial_fpdus;

	/* MPA statistics */
	u64				unalign_rx_comp;
};
#endif

#define IS_ECORE_DCQCN(p_hwfn)	\
	(!!(p_hwfn->pf_params.rdma_pf_params.enable_dcqcn))

struct ecore_roce_info {
	struct roce_events_stats	event_stats;

	u8				dcqcn_enabled;
	u8				dcqcn_reaction_point;
};

struct ecore_rdma_info {
	osal_spinlock_t			lock;

	struct ecore_bmap		cq_map;
	struct ecore_bmap		pd_map;
	struct ecore_bmap		tid_map;
	struct ecore_bmap		srq_map;
	struct ecore_bmap		cid_map;
	struct ecore_bmap		tcp_cid_map;
	struct ecore_bmap		real_cid_map;
	struct ecore_bmap		dpi_map;
	struct ecore_bmap		toggle_bits;
	struct ecore_rdma_events	events;
	struct ecore_rdma_device	*dev;
	struct ecore_rdma_port		*port;
	u32				last_tid;
	u8				num_cnqs;
	struct rdma_sent_stats          rdma_sent_pstats;
	struct rdma_rcv_stats           rdma_rcv_tstats;
	u32				num_qps;
	u32				num_mrs;
	u32				num_srqs;
	u16				queue_zone_base;
	u16				max_queue_zones;
	enum protocol_type		proto;
	struct ecore_roce_info		roce;
#ifdef CONFIG_ECORE_IWARP
	struct ecore_iwarp_info		iwarp;
#endif
};

#ifdef CONFIG_ECORE_IWARP
enum ecore_iwarp_qp_state {
	ECORE_IWARP_QP_STATE_IDLE,
	ECORE_IWARP_QP_STATE_RTS,
	ECORE_IWARP_QP_STATE_TERMINATE,
	ECORE_IWARP_QP_STATE_CLOSING,
	ECORE_IWARP_QP_STATE_ERROR,
};
#endif

struct ecore_rdma_qp {
	struct regpair qp_handle;
	struct regpair qp_handle_async;
	u32	qpid; /* iwarp: may differ from icid */
	u16	icid;
	enum ecore_roce_qp_state cur_state;
#ifdef CONFIG_ECORE_IWARP
	enum ecore_iwarp_qp_state iwarp_state;
#endif
	bool	use_srq;
	bool	signal_all;
	bool	fmr_and_reserved_lkey;

	bool	incoming_rdma_read_en;
	bool	incoming_rdma_write_en;
	bool	incoming_atomic_en;
	bool	e2e_flow_control_en;

	u16	pd;			/* Protection domain */
	u16	pkey;			/* Primary P_key index */
	u32	dest_qp;
	u16	mtu;
	u16	srq_id;
	u8	traffic_class_tos;	/* IPv6/GRH traffic class; IPv4 TOS */
	u8	hop_limit_ttl;		/* IPv6/GRH hop limit; IPv4 TTL */
	u16	dpi;
	u32	flow_label;		/* ignored in IPv4 */
	u16	vlan_id;
	u32	ack_timeout;
	u8	retry_cnt;
	u8	rnr_retry_cnt;
	u8	min_rnr_nak_timer;
	bool	sqd_async;
	union ecore_gid	sgid;		/* GRH SGID; IPv4/6 Source IP */
	union ecore_gid	dgid;		/* GRH DGID; IPv4/6 Destination IP */
	enum roce_mode roce_mode;
	u16	udp_src_port;		/* RoCEv2 only */
	u8	stats_queue;

	/* requeseter */
	u8	max_rd_atomic_req;
	u32     sq_psn;
	u16	sq_cq_id; /* The cq to be associated with the send queue*/
	u16	sq_num_pages;
	dma_addr_t sq_pbl_ptr;
	void	*orq;
	dma_addr_t orq_phys_addr;
	u8	orq_num_pages;
	bool	req_offloaded;

	/* responder */
	u8	max_rd_atomic_resp;
	u32     rq_psn;
	u16	rq_cq_id; /* The cq to be associated with the receive queue */
	u16	rq_num_pages;
	dma_addr_t rq_pbl_ptr;
	void	*irq;
	dma_addr_t irq_phys_addr;
	u8	irq_num_pages;
	bool	resp_offloaded;
	u32	cq_prod;

	u8	remote_mac_addr[6];
	u8	local_mac_addr[6];

	void	*shared_queue;
	dma_addr_t shared_queue_phys_addr;
#ifdef CONFIG_ECORE_IWARP
	struct ecore_iwarp_ep *ep;
#endif
};

#ifdef CONFIG_ECORE_IWARP

enum ecore_iwarp_ep_state {
	ECORE_IWARP_EP_INIT,
	ECORE_IWARP_EP_MPA_REQ_RCVD,
	ECORE_IWARP_EP_ESTABLISHED,
	ECORE_IWARP_EP_CLOSED
};

union async_output {
	struct iwarp_eqe_data_mpa_async_completion mpa_response;
	struct iwarp_eqe_data_tcp_async_completion mpa_request;
};

#define ECORE_MAX_PRIV_DATA_LEN (512)
struct ecore_iwarp_ep_memory {
	u8			in_pdata[ECORE_MAX_PRIV_DATA_LEN];
	u8			out_pdata[ECORE_MAX_PRIV_DATA_LEN];
	union async_output	async_output;
};

/* Endpoint structure represents a TCP connection. This connection can be
 * associated with a QP or not (in which case QP==NULL)
 */
struct ecore_iwarp_ep {
	osal_list_entry_t		list_entry;
	int				sig;
	struct ecore_rdma_qp		*qp;
	enum ecore_iwarp_ep_state	state;

	/* This contains entire buffer required for ep memories. This is the
	 * only one actually allocated and freed. The rest are pointers into
	 * this buffer
	 */
	struct ecore_iwarp_ep_memory    *ep_buffer_virt;
	dma_addr_t			ep_buffer_phys;

	struct ecore_iwarp_cm_info	cm_info;
	enum tcp_connect_mode		connect_mode;
	enum mpa_rtr_type		rtr_type;
	enum mpa_negotiation_mode	mpa_rev;
	u32				tcp_cid;
	u32				cid;
	u8				remote_mac_addr[6];
	u8				local_mac_addr[6];
	u16				mss;
	bool				mpa_reply_processed;

	/* The event_cb function is called for asynchrounous events associated
	 * with the ep. It is initialized at different entry points depending
	 * on whether the ep is the tcp connection active side or passive side
	 * The cb_context is passed to the event_cb function.
	 */
	iwarp_event_handler		event_cb;
	void				*cb_context;

	/* For Passive side - syn packet related data */
	struct ecore_iwarp_ll2_buff	*syn;
	u16				syn_ip_payload_length;
	dma_addr_t			syn_phy_addr;
};

struct ecore_iwarp_listener {
	osal_list_entry_t	list_entry;

	/* The event_cb function is called for connection requests.
	 * The cb_context is passed to the event_cb function.
	 */
	iwarp_event_handler	event_cb;
	void			*cb_context;
	u32			max_backlog;
	u8			ip_version;
	u32			ip_addr[4];
	u16			port;
	u16			vlan;

};

void ecore_iwarp_async_event(struct ecore_hwfn *p_hwfn,
			     u8 fw_event_code,
			     struct regpair *fw_handle,
			     u8 fw_return_code);

#endif /* CONFIG_ECORE_IWARP */

void ecore_roce_async_event(struct ecore_hwfn *p_hwfn,
			    u8 fw_event_code,
			    union rdma_eqe_data *rdma_data);

#endif /*__ECORE_RDMA_H__*/
