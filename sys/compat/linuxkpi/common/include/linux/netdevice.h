/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2019 Mellanox Technologies, Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#ifndef	_LINUX_NETDEVICE_H_
#define	_LINUX_NETDEVICE_H_

#include <linux/types.h>

#include <sys/socket.h>

#include <net/if_types.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>

#include <linux/net.h>
#include <linux/notifier.h>

#ifdef VIMAGE
#define	init_net *vnet0
#else
#define	init_net *((struct vnet *)0)
#endif

#define	MAX_ADDR_LEN		20

#define	net_device	ifnet

#define	rtnl_lock()
#define	rtnl_unlock()

/* According to linux::ipoib_main.c. */
struct netdev_notifier_info {
	struct net_device	*dev;
	struct ifnet		*ifp;
};

static inline struct net_device *
netdev_notifier_info_to_dev(struct netdev_notifier_info *ni)
{
	return (ni->dev);
}

static inline struct ifnet *
netdev_notifier_info_to_ifp(struct netdev_notifier_info *ni)
{
	return (ni->ifp);
}

int	register_netdevice_notifier(struct notifier_block *);
int	register_inetaddr_notifier(struct notifier_block *);
int	unregister_netdevice_notifier(struct notifier_block *);
int	unregister_inetaddr_notifier(struct notifier_block *);

#endif	/* _LINUX_NETDEVICE_H_ */
