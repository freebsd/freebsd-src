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

#ifndef VNIC_DATA_H_INCLUDED
#define VNIC_DATA_H_INCLUDED

#include <linux/if_vlan.h>

#ifdef CONFIG_INFINIBAND_QLGC_VNIC_STATS
#include <linux/timex.h>
#endif	/* CONFIG_INFINIBAND_QLGC_VNIC_STATS */

#include "vnic_ib.h"
#include "vnic_control_pkt.h"
#include "vnic_trailer.h"

struct rdma_dest {
	struct list_head	list_ptrs;
	struct sk_buff		*skb;
	u8			*data;
	struct viport_trailer	*trailer __attribute__((aligned(32)));
};

struct buff_pool_entry {
	__be64	remote_addr;
	__be32	rkey;
	u32	valid;
};

struct recv_pool {
	u32			buffer_sz;
	u32			pool_sz;
	u32			eioc_pool_sz;
	u32	 		eioc_rdma_rkey;
	u64 			eioc_rdma_addr;
	u32 			next_full_buf;
	u32 			next_free_buf;
	u32 			num_free_bufs;
	u32 			num_posted_bufs;
	u32 			sz_free_bundle;
	int			kick_on_free;
	struct buff_pool_entry	*buf_pool;
	dma_addr_t		buf_pool_dma;
	int			buf_pool_len;
	struct rdma_dest	*recv_bufs;
	struct list_head	avail_recv_bufs;
};

struct xmit_pool {
	u32			buffer_sz;
	u32 			pool_sz;
	u32 			notify_count;
	u32 			notify_bundle;
	u32 			next_xmit_buf;
	u32 			last_comp_buf;
	u32 			num_xmit_bufs;
	u32 			next_xmit_pool;
	u32 			kick_count;
	u32 			kick_byte_count;
	u32 			kick_bundle;
	u32 			kick_byte_bundle;
	int			need_buffers;
	int			send_kicks;
	uint32_t 		rdma_rkey;
	u64 			rdma_addr;
	struct buff_pool_entry	*buf_pool;
	dma_addr_t		buf_pool_dma;
	int			buf_pool_len;
	struct rdma_io		*xmit_bufs;
	u8			*xmit_data;
	dma_addr_t		xmitdata_dma;
	int			xmitdata_len;
};

struct data {
	struct viport			*parent;
	struct data_config		*config;
	struct ib_mr			*mr;
	struct vnic_ib_conn		ib_conn;
	u8				*local_storage;
	struct vnic_recv_pool_config	host_pool_parms;
	struct vnic_recv_pool_config	eioc_pool_parms;
	struct recv_pool		recv_pool;
	struct xmit_pool		xmit_pool;
	u8				*region_data;
	dma_addr_t			region_data_dma;
	struct rdma_io			free_bufs_io;
	struct send_io			kick_io;
	struct list_head		recv_ios;
	spinlock_t			recv_ios_lock;
	spinlock_t			xmit_buf_lock;
	int				kick_timer_on;
	int				connected;
	u16				max_mtu;
	struct timer_list		kick_timer;
	struct completion		done;
#ifdef CONFIG_INFINIBAND_QLGC_VNIC_STATS
	struct {
		u32		xmit_num;
		u32		recv_num;
		u32		free_buf_sends;
		u32		free_buf_num;
		u32		free_buf_min;
		u32		kick_recvs;
		u32		kick_reqs;
		u32		no_xmit_bufs;
		cycles_t	no_xmit_buf_time;
	} statistics;
#endif	/* CONFIG_INFINIBAND_QLGC_VNIC_STATS */
};

struct mc_data {
    struct viport           *parent;
    struct data_config      *config;
    struct ib_mr            *mr;
    struct vnic_ib_conn     ib_conn;

    u32                     num_recvs;
    u32                     skb_len;
    spinlock_t              recv_lock;
    int                     recv_len;
    struct ud_recv_io      *recv_ios;
    struct list_head        avail_recv_ios_list;
};

int data_init(struct data *data, struct viport *viport,
	      struct data_config *config, struct ib_pd *pd);

int  data_connect(struct data *data);
void data_connected(struct data *data);
void data_disconnect(struct data *data);

int data_xmit_packet(struct data *data, struct sk_buff *skb);

void data_cleanup(struct data *data);

#define data_is_connected(data)		\
	(vnic_ib_conn_connected(&((data)->ib_conn)))
#define data_path_id(data)		((data)->config->path_id)
#define data_eioc_pool(data)		(&(data)->eioc_pool_parms)
#define data_host_pool(data)		(&(data)->host_pool_parms)
#define data_eioc_pool_min(data)	(&(data)->config->eioc_min)
#define data_host_pool_min(data)	(&(data)->config->host_min)
#define data_eioc_pool_max(data)	(&(data)->config->eioc_max)
#define data_host_pool_max(data)	(&(data)->config->host_max)
#define data_local_pool_addr(data)	((data)->xmit_pool.rdma_addr)
#define data_local_pool_rkey(data)	((data)->xmit_pool.rdma_rkey)
#define data_remote_pool_addr(data)	(&(data)->recv_pool.eioc_rdma_addr)
#define data_remote_pool_rkey(data)	(&(data)->recv_pool.eioc_rdma_rkey)

#define data_max_mtu(data)		((data)->max_mtu)


#define data_len(data, trailer)		be16_to_cpu(trailer->data_length)
#define data_offset(data, trailer)					\
	((data)->recv_pool.buffer_sz - sizeof(struct viport_trailer)	\
	- ALIGN(data_len((data), (trailer)), VIPORT_TRAILER_ALIGNMENT)	\
	+ (trailer->data_alignment_offset))

/* the following macros manipulate ring buffer indexes.
 * the ring buffer size must be a power of 2.
 */
#define ADD(index, increment, size)	(((index) + (increment))&((size) - 1))
#define NEXT(index, size)		ADD(index, 1, size)
#define INC(index, increment, size)	(index) = ADD(index, increment, size)

/* this is max multicast msg embedded will send */
#define MCAST_MSG_SIZE \
		(2048 - sizeof(struct ib_grh) - sizeof(struct viport_trailer))

int mc_data_init(struct mc_data *mc_data, struct viport *viport,
	struct data_config *config,
	struct ib_pd *pd);

void vnic_mc_data_cleanup(struct mc_data *mc_data);

#endif	/* VNIC_DATA_H_INCLUDED */
