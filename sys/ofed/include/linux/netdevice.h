/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013, 2014 Mellanox Technologies, Ltd.
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
#include <linux/ethtool.h>
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

#define	netif_running(dev)	!!((dev)->if_drv_flags & IFF_DRV_RUNNING)
#define	netif_oper_up(dev)	!!((dev)->if_flags & IFF_UP)
#define	netif_carrier_ok(dev)	netif_running(dev)

static inline void *
netdev_priv(const struct net_device *dev)
{
	return (dev->if_softc);
}

static inline void
_handle_ifnet_link_event(void *arg, struct ifnet *ifp, int linkstate)
{
	struct notifier_block *nb;

	nb = arg;
	if (linkstate == LINK_STATE_UP)
		nb->notifier_call(nb, NETDEV_UP, ifp);
	else
		nb->notifier_call(nb, NETDEV_DOWN, ifp);
}

static inline void
_handle_ifnet_arrival_event(void *arg, struct ifnet *ifp)
{
	struct notifier_block *nb;

	nb = arg;
	nb->notifier_call(nb, NETDEV_REGISTER, ifp);
}

static inline void
_handle_ifnet_departure_event(void *arg, struct ifnet *ifp)
{
	struct notifier_block *nb;

	nb = arg;
	nb->notifier_call(nb, NETDEV_UNREGISTER, ifp);
}

static inline void
_handle_iflladdr_event(void *arg, struct ifnet *ifp)
{
	struct notifier_block *nb;

	nb = arg;
	nb->notifier_call(nb, NETDEV_CHANGEADDR, ifp);
}

static inline void
_handle_ifaddr_event(void *arg, struct ifnet *ifp)
{
	struct notifier_block *nb;

	nb = arg;
	nb->notifier_call(nb, NETDEV_CHANGEIFADDR, ifp);
}

static inline int
register_netdevice_notifier(struct notifier_block *nb)
{

	nb->tags[NETDEV_UP] = EVENTHANDLER_REGISTER(
	    ifnet_link_event, _handle_ifnet_link_event, nb, 0);
	nb->tags[NETDEV_REGISTER] = EVENTHANDLER_REGISTER(
	    ifnet_arrival_event, _handle_ifnet_arrival_event, nb, 0);
	nb->tags[NETDEV_UNREGISTER] = EVENTHANDLER_REGISTER(
	    ifnet_departure_event, _handle_ifnet_departure_event, nb, 0);
	nb->tags[NETDEV_CHANGEADDR] = EVENTHANDLER_REGISTER(
	    iflladdr_event, _handle_iflladdr_event, nb, 0);

	return (0);
}

static inline int
register_inetaddr_notifier(struct notifier_block *nb)
{

        nb->tags[NETDEV_CHANGEIFADDR] = EVENTHANDLER_REGISTER(
            ifaddr_event, _handle_ifaddr_event, nb, 0);
        return (0);
}

static inline int
unregister_netdevice_notifier(struct notifier_block *nb)
{

        EVENTHANDLER_DEREGISTER(ifnet_link_event, nb->tags[NETDEV_UP]);
        EVENTHANDLER_DEREGISTER(ifnet_arrival_event, nb->tags[NETDEV_REGISTER]);
        EVENTHANDLER_DEREGISTER(ifnet_departure_event,
	    nb->tags[NETDEV_UNREGISTER]);
        EVENTHANDLER_DEREGISTER(iflladdr_event,
            nb->tags[NETDEV_CHANGEADDR]);

	return (0);
}

static inline int
unregister_inetaddr_notifier(struct notifier_block *nb)
{

        EVENTHANDLER_DEREGISTER(ifaddr_event,
            nb->tags[NETDEV_CHANGEIFADDR]);

        return (0);
}


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
