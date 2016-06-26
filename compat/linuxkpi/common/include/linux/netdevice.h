/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2016 Mellanox Technologies, Ltd.
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

#include <linux/completion.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/net.h>
#include <linux/notifier.h>

struct net {
};

extern struct net init_net;

#define	MAX_ADDR_LEN		20

#define	net_device	ifnet

#define	dev_get_by_index(n, idx)	ifnet_byindex_ref((idx))
#define	dev_hold(d)	if_ref((d))
#define	dev_put(d)	if_rele((d))
#define	dev_net(d)	(&init_net)

#define	net_eq(a,b)	((a) == (b))

#define	netif_running(dev)	!!((dev)->if_drv_flags & IFF_DRV_RUNNING)
#define	netif_oper_up(dev)	!!((dev)->if_flags & IFF_UP)
#define	netif_carrier_ok(dev)	netif_running(dev)

static inline void *
netdev_priv(const struct net_device *dev)
{
	return (dev->if_softc);
}

static inline struct net_device *
netdev_notifier_info_to_dev(void *ifp)
{
	return (ifp);
}

int	register_netdevice_notifier(struct notifier_block *);
int	register_inetaddr_notifier(struct notifier_block *);
int	unregister_netdevice_notifier(struct notifier_block *);
int	unregister_inetaddr_notifier(struct notifier_block *);

#define	rtnl_lock()
#define	rtnl_unlock()

static inline int
dev_mc_delete(struct net_device *dev, void *addr, int alen, int all)
{
	struct sockaddr_dl sdl;

	if (alen > sizeof(sdl.sdl_data))
		return (-EINVAL);
	memset(&sdl, 0, sizeof(sdl));
	sdl.sdl_len = sizeof(sdl);
	sdl.sdl_family = AF_LINK;
	sdl.sdl_alen = alen;
	memcpy(&sdl.sdl_data, addr, alen);

	return -if_delmulti(dev, (struct sockaddr *)&sdl);
}

static inline int
dev_mc_del(struct net_device *dev, void *addr)
{
	return (dev_mc_delete(dev, addr, 6, 0));
}

static inline int
dev_mc_add(struct net_device *dev, void *addr, int alen, int newonly)
{
	struct sockaddr_dl sdl;

	if (alen > sizeof(sdl.sdl_data))
		return (-EINVAL);
	memset(&sdl, 0, sizeof(sdl));
	sdl.sdl_len = sizeof(sdl);
	sdl.sdl_family = AF_LINK;
	sdl.sdl_alen = alen;
	memcpy(&sdl.sdl_data, addr, alen);

	return -if_addmulti(dev, (struct sockaddr *)&sdl, NULL);
}

#endif	/* _LINUX_NETDEVICE_H_ */
