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

#ifndef VNIC_MAIN_H_INCLUDED
#define VNIC_MAIN_H_INCLUDED

#include <linux/timex.h>
#include <linux/netdevice.h>
#include <linux/kthread.h>
#include <linux/fs.h>

#include "vnic_config.h"
#include "vnic_netpath.h"

extern u16 vnic_max_mtu;
extern struct list_head vnic_list;
extern struct attribute_group vnic_stats_attr_group;
extern cycles_t vnic_recv_ref;

enum vnic_npevent_type {
	VNIC_PRINP_CONNECTED	= 0,
	VNIC_PRINP_DISCONNECTED	= 1,
	VNIC_PRINP_LINKUP	= 2,
	VNIC_PRINP_LINKDOWN	= 3,
	VNIC_PRINP_TIMEREXPIRED	= 4,
	VNIC_PRINP_SETLINK	= 5,

	/* used to figure out PRI vs SEC types for dbg msg*/
	VNIC_PRINP_LASTTYPE     = VNIC_PRINP_SETLINK,

	VNIC_SECNP_CONNECTED	= 6,
	VNIC_SECNP_DISCONNECTED	= 7,
	VNIC_SECNP_LINKUP	= 8,
	VNIC_SECNP_LINKDOWN	= 9,
	VNIC_SECNP_TIMEREXPIRED	= 10,
	VNIC_SECNP_SETLINK	= 11,

	/* used to figure out PRI vs SEC types for dbg msg*/
	VNIC_SECNP_LASTTYPE     = VNIC_SECNP_SETLINK,

	VNIC_FORCE_FAILOVER	= 12,
	VNIC_UNFAILOVER		= 13,
	VNIC_NP_FREEVNIC	= 14,
	/*
	 * NOTE : If any new netpath event is being added, don't forget to
	 * add corresponding netpath event string into vnic_main.c.
	 */
};

struct vnic_npevent {
	struct list_head	list_ptrs;
	struct vnic		*vnic;
	enum vnic_npevent_type	event_type;
};

void vnic_npevent_queue_evt(struct netpath *netpath,
			    enum vnic_npevent_type evt);
void vnic_npevent_dequeue_evt(struct netpath *netpath,
			      enum vnic_npevent_type evt);

enum vnic_state {
	VNIC_UNINITIALIZED	= 0,
	VNIC_REGISTERED		= 1
};

struct vnic {
	struct list_head		list_ptrs;
	enum vnic_state			state;
	struct vnic_config		*config;
	struct netpath			*current_path;
	struct netpath			primary_path;
	struct netpath			secondary_path;
	int				carrier;
	int				forced_failover;
	int				failed_over;
	int				mac_set;
	struct net_device		*netdevice;
	struct dev_info			dev_info;
	struct dev_mc_list		*mc_list;
	int				mc_list_len;
	int				mc_count;
	spinlock_t			lock;
	spinlock_t			current_path_lock;
#ifdef CONFIG_INFINIBAND_QLGC_VNIC_STATS
	struct {
		cycles_t	start_time;
		cycles_t	conn_time;
		cycles_t	disconn_ref;	/* intermediate time */
		cycles_t	disconn_time;
		u32		disconn_num;
		cycles_t	xmit_time;
		u32		xmit_num;
		u32		xmit_fail;
		cycles_t	recv_time;
		u32		recv_num;
		u32		multicast_recv_num;
		cycles_t	xmit_ref;	/* intermediate time */
		cycles_t	xmit_off_time;
		u32		xmit_off_num;
		cycles_t	carrier_ref;	/* intermediate time */
		cycles_t	carrier_off_time;
		u32		carrier_off_num;
	} statistics;
	struct dev_info		stat_info;
#endif	/* CONFIG_INFINIBAND_QLGC_VNIC_STATS */
};

struct vnic *vnic_allocate(struct vnic_config *config);

void vnic_free(struct vnic *vnic);

void vnic_force_failover(struct vnic *vnic);
void vnic_unfailover(struct vnic *vnic);

void vnic_connected(struct vnic *vnic, struct netpath *netpath);
void vnic_disconnected(struct vnic *vnic, struct netpath *netpath);

void vnic_link_up(struct vnic *vnic, struct netpath *netpath);
void vnic_link_down(struct vnic *vnic, struct netpath *netpath);

void vnic_stop_xmit(struct vnic *vnic, struct netpath *netpath);
void vnic_restart_xmit(struct vnic *vnic, struct netpath *netpath);

void vnic_recv_packet(struct vnic *vnic, struct netpath *netpath,
		      struct sk_buff *skb);
void vnic_npevent_cleanup(void);
#endif	/* VNIC_MAIN_H_INCLUDED */
