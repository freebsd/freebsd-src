/*
 * Copyright (c) 2006 QLogic, Inc.  All rights reserved.
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

#ifndef VNIC_CONTROL_H_INCLUDED
#define VNIC_CONTROL_H_INCLUDED

#ifdef CONFIG_INFINIBAND_QLGC_VNIC_STATS
#include <linux/timex.h>
#include <linux/completion.h>
#endif	/* CONFIG_INFINIBAND_QLGC_VNIC_STATS */

#include "vnic_ib.h"
#include "vnic_control_pkt.h"

enum control_timer_state {
	TIMER_IDLE	= 0,
	TIMER_ACTIVE	= 1,
	TIMER_EXPIRED	= 2
};

enum control_request_state {
	REQ_INACTIVE,  /* quiet state, all previous operations done
			*      response is NULL
			*      last_cmd = CMD_INVALID
			*      timer_state = IDLE
			*/
	REQ_POSTED,    /* REQ put on send Q
			*      response is NULL
			*      last_cmd = command issued
			*      timer_state = ACTIVE
			*/
	REQ_SENT,      /* Send completed for REQ
			*      response is NULL
			*      last_cmd = command issued
			*      timer_state = ACTIVE
			*/
	RSP_RECEIVED,  /* Received Resp, but no Send completion yet
			*      response is response buffer received
			*      last_cmd = command issued
			*      timer_state = ACTIVE
			*/
	REQ_COMPLETED, /* all processing for REQ completed, ready to be gotten
			*      response is response buffer received
			*      last_cmd = command issued
			*      timer_state = ACTIVE
			*/
	REQ_FAILED,    /* processing of REQ/RSP failed.
			*      response is NULL
			*      last_cmd = CMD_INVALID
			*      timer_state = IDLE or EXPIRED
			*      viport has been moved to error state to force
			*      recovery
			*/
};

struct control {
	struct viport			*parent;
	struct control_config		*config;
	struct ib_mr			*mr;
	struct vnic_ib_conn		ib_conn;
	struct vnic_control_packet	*local_storage;
	int				send_len;
	int				recv_len;
	u16				maj_ver;
	u16				min_ver;
	struct vnic_lan_switch_attribs	lan_switch;
	struct send_io			send_io;
	struct recv_io			*recv_ios;
	dma_addr_t			send_dma;
	dma_addr_t			recv_dma;
	enum control_timer_state	timer_state;
	enum control_request_state      req_state;
	struct timer_list		timer;
	u8				seq_num;
	u8				last_cmd;
	struct recv_io			*response;
	struct recv_io			*info;
	struct list_head		failure_list;
	spinlock_t			io_lock;
	struct completion		done;
#ifdef CONFIG_INFINIBAND_QLGC_VNIC_STATS
	struct {
		cycles_t	request_time;	/* intermediate value */
		cycles_t	response_time;
		u32		response_num;
		cycles_t	response_max;
		cycles_t	response_min;
		u32		timeout_num;
	} statistics;
#endif	/* CONFIG_INFINIBAND_QLGC_VNIC_STATS */
};

int control_init(struct control *control, struct viport *viport,
		 struct control_config *config, struct ib_pd *pd);

void control_cleanup(struct control *control);

void control_process_async(struct control *control);

int control_init_vnic_req(struct control *control);
int control_init_vnic_rsp(struct control *control, u32 *features,
			  u8 *mac_address, u16 *num_addrs, u16 *vlan);

int control_config_data_path_req(struct control *control, u64 path_id,
				 struct vnic_recv_pool_config *host,
				 struct vnic_recv_pool_config *eioc);
int control_config_data_path_rsp(struct control *control,
				 struct vnic_recv_pool_config *host,
				 struct vnic_recv_pool_config *eioc,
				 struct vnic_recv_pool_config *max_host,
				 struct vnic_recv_pool_config *max_eioc,
				 struct vnic_recv_pool_config *min_host,
				 struct vnic_recv_pool_config *min_eioc);

int control_exchange_pools_req(struct control *control,
			       u64 addr, u32 rkey);
int control_exchange_pools_rsp(struct control *control,
			       u64 *addr, u32 *rkey);

int control_config_link_req(struct control *control,
			    u16 flags, u16 mtu);
int control_config_link_rsp(struct control *control,
			    u16 *flags, u16 *mtu);

int control_config_addrs_req(struct control *control,
			     struct vnic_address_op2 *addrs, u16 num);
int control_config_addrs_rsp(struct control *control);

int control_report_statistics_req(struct control *control);
int control_report_statistics_rsp(struct control *control,
				  struct vnic_cmd_report_stats_rsp *stats);

int control_heartbeat_req(struct control *control, u32 hb_interval);
int control_heartbeat_rsp(struct control *control);

int control_reset_req(struct control *control);
int control_reset_rsp(struct control *control);

#define control_packet(io)					\
			((struct vnic_control_packet *)(io)->virtual_addr)

#define control_is_connected(control) 				\
	(vnic_ib_conn_connected(&((control)->ib_conn)))

#define control_last_req(control)	control_packet(&(control)->send_io)
#define control_features(control)	((control)->features_supported)

#define control_get_mac_address(control,addr) 				\
	memcpy(addr, (control)->lan_switch.hw_mac_address, ETH_ALEN)

#endif	/* VNIC_CONTROL_H_INCLUDED */
