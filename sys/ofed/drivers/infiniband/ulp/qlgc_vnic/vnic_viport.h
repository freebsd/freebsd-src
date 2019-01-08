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

#ifndef VNIC_VIPORT_H_INCLUDED
#define VNIC_VIPORT_H_INCLUDED

#include "vnic_control.h"
#include "vnic_data.h"
#include "vnic_multicast.h"

enum viport_state {
	VIPORT_DISCONNECTED	= 0,
	VIPORT_CONNECTED	= 1
};

enum link_state {
	LINK_UNINITIALIZED	= 0,
	LINK_INITIALIZE		= 1,
	LINK_INITIALIZECONTROL	= 2,
	LINK_INITIALIZEDATA	= 3,
	LINK_CONTROLCONNECT	= 4,
	LINK_CONTROLCONNECTWAIT	= 5,
	LINK_INITVNICREQ	= 6,
	LINK_INITVNICRSP	= 7,
	LINK_BEGINDATAPATH	= 8,
	LINK_CONFIGDATAPATHREQ	= 9,
	LINK_CONFIGDATAPATHRSP	= 10,
	LINK_DATACONNECT	= 11,
	LINK_DATACONNECTWAIT	= 12,
	LINK_XCHGPOOLREQ	= 13,
	LINK_XCHGPOOLRSP	= 14,
	LINK_INITIALIZED	= 15,
	LINK_IDLE		= 16,
	LINK_IDLING		= 17,
	LINK_CONFIGLINKREQ	= 18,
	LINK_CONFIGLINKRSP	= 19,
	LINK_CONFIGADDRSREQ	= 20,
	LINK_CONFIGADDRSRSP	= 21,
	LINK_REPORTSTATREQ	= 22,
	LINK_REPORTSTATRSP	= 23,
	LINK_HEARTBEATREQ	= 24,
	LINK_HEARTBEATRSP	= 25,
	LINK_RESET		= 26,
	LINK_RESETRSP		= 27,
	LINK_RESETCONTROL	= 28,
	LINK_RESETCONTROLRSP	= 29,
	LINK_DATADISCONNECT	= 30,
	LINK_CONTROLDISCONNECT	= 31,
	LINK_CLEANUPDATA	= 32,
	LINK_CLEANUPCONTROL	= 33,
	LINK_DISCONNECTED	= 34,
	LINK_RETRYWAIT		= 35,
	LINK_FIRSTCONNECT	= 36
};

enum {
	BROADCAST_ADDR		= 0,
	UNICAST_ADDR		= 1,
	MCAST_ADDR_START	= 2
};

#define current_mac_address	mac_addresses[UNICAST_ADDR].address

enum {
	NEED_STATS           	= 0x00000001,
	NEED_ADDRESS_CONFIG  	= 0x00000002,
	NEED_LINK_CONFIG     	= 0x00000004,
	MCAST_OVERFLOW       	= 0x00000008,
	NEED_MCAST_COMPLETION	= 0x00000010,
	NEED_MCAST_JOIN      	= 0x00000020
};

struct viport {
	struct list_head		list_ptrs;
	struct netpath			*parent;
	struct vnic			*vnic;
	struct viport_config		*config;
	struct control			control;
	struct data			data;
	spinlock_t			lock;
	struct ib_pd			*pd;
	enum viport_state		state;
	enum link_state			link_state;
	struct vnic_cmd_report_stats_rsp stats;
	wait_queue_head_t		stats_queue;
	unsigned long			last_stats_time;
	u32				features_supported;
	u8				hw_mac_address[ETH_ALEN];
	u16				default_vlan;
	u16				num_mac_addresses;
	struct vnic_address_op2		*mac_addresses;
	u32				updates;
	u16				flags;
	u16				new_flags;
	u16				mtu;
	u16				new_mtu;
	u32				errored;
	enum { WAIT, DELAY, NOW }	connect;
	u32				disconnect;
	u32 				retry;
	wait_queue_head_t		disconnect_queue;
	int				timer_active;
	struct timer_list		timer;
	u32 				retry_duration;
	u32 				total_retry_duration;
	atomic_t			reference_count;
	wait_queue_head_t		reference_queue;
	struct mc_info	mc_info;
	struct mc_data	mc_data;
};

int  viport_start(void);
void viport_cleanup(void);

struct viport *viport_allocate(struct viport_config *config);
void viport_free(struct viport *viport);

void viport_connect(struct viport *viport, int delay);

void viport_set_link(struct viport *viport, u16 flags, u16 mtu);
void viport_get_stats(struct viport *viport,
		      struct net_device_stats *stats);
int  viport_xmit_packet(struct viport *viport, struct sk_buff *skb);
void viport_kick(struct viport *viport);

void viport_failure(struct viport *viport);

int viport_set_unicast(struct viport *viport, u8 *address);
int viport_set_multicast(struct viport *viport,
			 struct dev_mc_list *mc_list,
			 int mc_count);

#define viport_max_mtu(viport)		data_max_mtu(&(viport)->data)

#define viport_get_hw_addr(viport, address)			\
	memcpy(address, (viport)->hw_mac_address, ETH_ALEN)

#define viport_features(viport) ((viport)->features_supported)

#define viport_can_tx_csum(viport)				\
	(((viport)->features_supported & 			\
	(VNIC_FEAT_IPV4_CSUM_TX | VNIC_FEAT_TCP_CSUM_TX |	\
	VNIC_FEAT_UDP_CSUM_TX)) == (VNIC_FEAT_IPV4_CSUM_TX |	\
	VNIC_FEAT_TCP_CSUM_TX | VNIC_FEAT_UDP_CSUM_TX))

#endif /* VNIC_VIPORT_H_INCLUDED */
