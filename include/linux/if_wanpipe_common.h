/*****************************************************************************
* if_wanipe_common.h   Sangoma Driver/Socket common area definitions.
*
* Author:       Nenad Corbic <ncorbic@sangoma.com>
*
* Copyright:    (c) 2000 Sangoma Technologies Inc.
*
*               This program is free software; you can redistribute it and/or
*               modify it under the terms of the GNU General Public License
*               as published by the Free Software Foundation; either version
*               2 of the License, or (at your option) any later version.
* ============================================================================
* Jan 13, 2000  Nenad Corbic      Initial version
*****************************************************************************/


#ifndef _WANPIPE_SOCK_DRIVER_COMMON_H
#define _WANPIPE_SOCK_DRIVER_COMMON_H

#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,0)
 #define netdevice_t struct net_device
#else
 #define netdevice_t struct device
#endif


typedef struct {
	netdevice_t *slave;
	atomic_t packet_sent;
	atomic_t receive_block;
	atomic_t command;
	atomic_t disconnect;
	atomic_t driver_busy;
	unsigned char common_critical;
	struct timer_list *tx_timer;
	struct sock *sk;		/* Wanpipe Sock bind's here */ 
	int   (*func) (struct sk_buff *, netdevice_t *, 
                       struct sock *);

	struct tq_struct wanpipe_task;    /* Immediate BH handler task */
	unsigned char rw_bind;			  /* Sock bind state */
	unsigned char usedby;
	unsigned char state;
	unsigned char svc;
	unsigned short lcn;
	void *mbox;
} wanpipe_common_t;


enum {
	WANSOCK_UNCONFIGURED,	/* link/channel is not configured */
	WANSOCK_DISCONNECTED,	/* link/channel is disconnected */
	WANSOCK_CONNECTING,		/* connection is in progress */
	WANSOCK_CONNECTED,		/* link/channel is operational */
	WANSOCK_LIMIT,		/* for verification only */
	WANSOCK_DUALPORT,		/* for Dual Port cards */
	WANSOCK_DISCONNECTING,
	WANSOCK_BINDED,
	WANSOCK_BIND_LISTEN,
	WANSOCK_LISTEN
};

#endif


