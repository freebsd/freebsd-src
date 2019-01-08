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

#ifndef VNIC_IB_H_INCLUDED
#define VNIC_IB_H_INCLUDED

#include <linux/timex.h>
#include <linux/completion.h>
#include <rdma/ib_verbs.h>
#include <rdma/ib_pack.h>
#include <rdma/ib_sa.h>
#include <rdma/ib_cm.h>

#include "vnic_sys.h"
#include "vnic_netpath.h"
#define PFX	"qlgc_vnic: "

struct io;
typedef void (comp_routine_t) (struct io *io);

enum vnic_ib_conn_state {
	IB_CONN_UNINITTED	= 0,
	IB_CONN_INITTED		= 1,
	IB_CONN_CONNECTING	= 2,
	IB_CONN_CONNECTED	= 3,
	IB_CONN_DISCONNECTED	= 4,
	IB_CONN_ERRORED		= 5
};

struct vnic_ib_conn {
	struct viport		*viport;
	struct vnic_ib_config	*ib_config;
	spinlock_t		conn_lock;
	enum vnic_ib_conn_state	state;
	struct ib_qp		*qp;
	struct ib_cq		*cq;
	struct ib_cm_id		*cm_id;
	int 			callback_thread_end;
	struct task_struct	*callback_thread;
	wait_queue_head_t	callback_wait_queue;
	u32 			in_thread;
	u32 			compl_received;
	struct completion 	callback_thread_exit;
	spinlock_t		compl_received_lock;
#ifdef CONFIG_INFINIBAND_QLGC_VNIC_STATS
	struct {
		cycles_t	connection_time;
		cycles_t	rdma_post_time;
		u32		rdma_post_ios;
		cycles_t	rdma_comp_time;
		u32		rdma_comp_ios;
		cycles_t	send_post_time;
		u32		send_post_ios;
		cycles_t	send_comp_time;
		u32		send_comp_ios;
		cycles_t	recv_post_time;
		u32		recv_post_ios;
		cycles_t	recv_comp_time;
		u32		recv_comp_ios;
		u32		num_ios;
		u32		num_callbacks;
		u32		max_ios;
	} statistics;
#endif	/* CONFIG_INFINIBAND_QLGC_VNIC_STATS */
};

struct vnic_ib_path_info {
	struct ib_sa_path_rec	path;
	struct ib_sa_query	*path_query;
	int			path_query_id;
	int			status;
	struct			completion done;
};

struct vnic_ib_device {
	struct ib_device	*dev;
	struct list_head	port_list;
};

struct vnic_ib_port {
	struct vnic_ib_device	*dev;
	u8			port_num;
	struct dev_info		pdev_info;
	struct list_head	list;
};

struct io {
	struct list_head	list_ptrs;
	struct viport		*viport;
	comp_routine_t		*routine;
	struct ib_recv_wr	rwr;
	struct ib_send_wr	swr;
#ifdef CONFIG_INFINIBAND_QLGC_VNIC_STATS
	cycles_t		time;
#endif	/* CONFIG_INFINIBAND_QLGC_VNIC_STATS */
	enum {RECV, RDMA, SEND, RECV_UD}	type;
};

struct rdma_io {
	struct io		io;
	struct ib_sge		list[2];
	u16			index;
	u16			len;
	u8			*data;
	dma_addr_t		data_dma;
	struct sk_buff		*skb;
	dma_addr_t		skb_data_dma;
	struct viport_trailer 	*trailer;
	dma_addr_t 		trailer_dma;
};

struct send_io {
	struct io	io;
	struct ib_sge	list;
	u8		*virtual_addr;
};

struct recv_io {
	struct io	io;
	struct ib_sge	list;
	u8		*virtual_addr;
};

struct ud_recv_io {
	struct io	io;
	u16 	len;
	dma_addr_t		skb_data_dma;
	struct ib_sge	list[2]; /* one for grh and other for rest of pkt. */
	struct sk_buff 	*skb;
};

int	vnic_ib_init(void);
void	vnic_ib_cleanup(void);
void vnic_completion_cleanup(struct vnic_ib_conn *ib_conn);

struct vnic;
int vnic_ib_get_path(struct netpath *netpath, struct vnic *vnic);
int vnic_ib_conn_init(struct vnic_ib_conn *ib_conn, struct viport *viport,
		      struct ib_pd *pd, struct vnic_ib_config *config);

int vnic_ib_post_recv(struct vnic_ib_conn *ib_conn, struct io *io);
int vnic_ib_post_send(struct vnic_ib_conn *ib_conn, struct io *io);
int vnic_ib_cm_connect(struct vnic_ib_conn *ib_conn);
int vnic_ib_cm_handler(struct ib_cm_id *cm_id, struct ib_cm_event *event);

#define	vnic_ib_conn_uninitted(ib_conn)			\
	((ib_conn)->state == IB_CONN_UNINITTED)
#define	vnic_ib_conn_initted(ib_conn)			\
	((ib_conn)->state == IB_CONN_INITTED)
#define	vnic_ib_conn_connecting(ib_conn)		\
	((ib_conn)->state == IB_CONN_CONNECTING)
#define	vnic_ib_conn_connected(ib_conn)			\
	((ib_conn)->state == IB_CONN_CONNECTED)
#define	vnic_ib_conn_disconnected(ib_conn)		\
	((ib_conn)->state == IB_CONN_DISCONNECTED)

#define MCAST_GROUP_INVALID 0x00 /* viport failed to join or left mc group */
#define MCAST_GROUP_JOINING 0x01 /* wait for completion */
#define MCAST_GROUP_JOINED  0x02 /* join process completed successfully */

/* vnic_sa_client is used to register with sa once. It is needed to join and
 * leave multicast groups.
 */
extern struct ib_sa_client vnic_sa_client;

/* The following functions are using initialize and handle multicast
 * components.
 */
struct mc_data; /* forward declaration */
/* Initialize all necessary mc components */
int vnic_ib_mc_init(struct mc_data *mc_data, struct viport *viport,
			struct ib_pd *pd, struct vnic_ib_config *config);
/* Put multicast qp in RTS */
int vnic_ib_mc_mod_qp_to_rts(struct ib_qp *qp);
/* Post multicast receive buffers */
int vnic_ib_mc_post_recv(struct mc_data *mc_data, struct io *io);

#endif	/* VNIC_IB_H_INCLUDED */
