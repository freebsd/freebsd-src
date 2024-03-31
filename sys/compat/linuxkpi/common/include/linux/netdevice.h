/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2019 Mellanox Technologies, Ltd.
 * All rights reserved.
 * Copyright (c) 2020-2021 The FreeBSD Foundation
 * Copyright (c) 2020-2022 Bjoern A. Zeeb
 *
 * Portions of this software were developed by Björn Zeeb
 * under sponsorship from the FreeBSD Foundation.
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
#ifndef	_LINUXKPI_LINUX_NETDEVICE_H
#define	_LINUXKPI_LINUX_NETDEVICE_H

#include <linux/types.h>
#include <linux/netdev_features.h>

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/taskqueue.h>

#include <net/if_types.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>

#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/net.h>
#include <linux/if_ether.h>
#include <linux/notifier.h>
#include <linux/random.h>
#include <linux/rcupdate.h>

#ifdef VIMAGE
#define	init_net *vnet0
#else
#define	init_net *((struct vnet *)0)
#endif

struct sk_buff;
struct net_device;
struct wireless_dev;		/* net/cfg80211.h */

#define	MAX_ADDR_LEN		20

#define	NET_NAME_UNKNOWN	0

enum netdev_tx {
	NETDEV_TX_OK		= 0,
};
typedef	enum netdev_tx		netdev_tx_t;

struct netdev_hw_addr {
	struct list_head	addr_list;
	uint8_t			addr[MAX_ADDR_LEN];
};

struct netdev_hw_addr_list {
	struct list_head	addr_list;
	int			count;
};

enum net_device_reg_state {
	NETREG_DUMMY		= 1,
	NETREG_REGISTERED,
};

struct net_device_ops {
	int (*ndo_open)(struct net_device *);
	int (*ndo_stop)(struct net_device *);
	int (*ndo_set_mac_address)(struct net_device *,  void *);
	netdev_tx_t (*ndo_start_xmit)(struct sk_buff *, struct net_device *);
	void (*ndo_set_rx_mode)(struct net_device *);
};

struct net_device {
	/* net_device fields seen publicly. */
	/* XXX can we later make some aliases to ifnet? */
	char				name[IFNAMSIZ];
	struct wireless_dev		*ieee80211_ptr;
	uint8_t				dev_addr[ETH_ALEN];
	struct netdev_hw_addr_list	mc;
	netdev_features_t		features;
	struct {
		unsigned long		multicast;

		unsigned long		rx_bytes;
		unsigned long		rx_errors;
		unsigned long		rx_packets;
		unsigned long		tx_bytes;
		unsigned long		tx_dropped;
		unsigned long		tx_errors;
		unsigned long		tx_packets;
	} stats;
	enum net_device_reg_state	reg_state;
	const struct ethtool_ops	*ethtool_ops;
	const struct net_device_ops	*netdev_ops;

	bool				needs_free_netdev;
	/* Not properly typed as-of now. */
	int	flags, type;
	int	name_assign_type, needed_headroom;
	int	threaded;

	void (*priv_destructor)(struct net_device *);

	/* net_device internal. */
	struct device			dev;

	/*
	 * In case we delete the net_device we need to be able to clear all
	 * NAPI consumers.
	 */
	struct mtx			napi_mtx;
	TAILQ_HEAD(, napi_struct)	napi_head;
	struct taskqueue		*napi_tq;

	/* Must stay last. */
	uint8_t				drv_priv[0] __aligned(CACHE_LINE_SIZE);
};

#define	SET_NETDEV_DEV(_ndev, _dev)	(_ndev)->dev.parent = _dev;

/* -------------------------------------------------------------------------- */
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

/* -------------------------------------------------------------------------- */

#define	NAPI_POLL_WEIGHT			64	/* budget */

/*
 * There are drivers directly testing napi state bits, so we need to publicly
 * expose them.  If you ask me, those accesses should be hid behind an
 * inline function and the bit flags not be directly exposed.
 */
enum napi_state_bits {
	/*
	 * Official Linux flags encountered.
	 */
	NAPI_STATE_SCHED = 1,

	/*
	 * Our internal versions (for now).
	 */
	/* Do not schedule new things while we are waiting to clear things. */
	LKPI_NAPI_FLAG_DISABLE_PENDING = 0,
	/* To synchronise that only one poll is ever running. */
	LKPI_NAPI_FLAG_IS_SCHEDULED = 1,
	/* If trying to schedule while poll is running. Need to re-schedule. */
	LKPI_NAPI_FLAG_LOST_RACE_TRY_AGAIN = 2,
	/* When shutting down forcefully prevent anything from running task/poll. */
	LKPI_NAPI_FLAG_SHUTDOWN = 3,
};

struct napi_struct {
	TAILQ_ENTRY(napi_struct)	entry;

	struct list_head	rx_list;
	struct net_device	*dev;
	int			(*poll)(struct napi_struct *, int);
	int			budget;
	int			rx_count;


	/*
	 * These flags mostly need to be checked/changed atomically
	 * (multiple together in some cases).
	 */
	volatile unsigned long	state;

	/* FreeBSD internal. */
	/* Use task for now, so we can easily switch between direct and task. */
	struct task		napi_task;
};

void linuxkpi_init_dummy_netdev(struct net_device *);
void linuxkpi_netif_napi_add(struct net_device *, struct napi_struct *,
    int(*napi_poll)(struct napi_struct *, int));
void linuxkpi_netif_napi_del(struct napi_struct *);
bool linuxkpi_napi_schedule_prep(struct napi_struct *);
void linuxkpi___napi_schedule(struct napi_struct *);
bool linuxkpi_napi_schedule(struct napi_struct *);
void linuxkpi_napi_reschedule(struct napi_struct *);
bool linuxkpi_napi_complete_done(struct napi_struct *, int);
bool linuxkpi_napi_complete(struct napi_struct *);
void linuxkpi_napi_disable(struct napi_struct *);
void linuxkpi_napi_enable(struct napi_struct *);
void linuxkpi_napi_synchronize(struct napi_struct *);

#define	init_dummy_netdev(_n)						\
	linuxkpi_init_dummy_netdev(_n)
#define	netif_napi_add(_nd, _ns, _p)					\
	linuxkpi_netif_napi_add(_nd, _ns, _p)
#define	netif_napi_del(_n)						\
	linuxkpi_netif_napi_del(_n)
#define	napi_schedule_prep(_n)						\
	linuxkpi_napi_schedule_prep(_n)
#define	__napi_schedule(_n)						\
	linuxkpi___napi_schedule(_n)
#define	napi_schedule(_n)						\
	linuxkpi_napi_schedule(_n)
#define	napi_reschedule(_n)						\
	linuxkpi_napi_reschedule(_n)
#define	napi_complete_done(_n, _r)					\
	linuxkpi_napi_complete_done(_n, _r)
#define	napi_complete(_n)						\
	linuxkpi_napi_complete(_n)
#define	napi_disable(_n)						\
	linuxkpi_napi_disable(_n)
#define	napi_enable(_n)							\
	linuxkpi_napi_enable(_n)
#define	napi_synchronize(_n)						\
	linuxkpi_napi_synchronize(_n)


static inline void
netif_napi_add_tx(struct net_device *dev, struct napi_struct *napi,
    int(*napi_poll)(struct napi_struct *, int))
{

	netif_napi_add(dev, napi, napi_poll);
}

static inline bool
napi_is_scheduled(struct napi_struct *napi)
{

	return (test_bit(LKPI_NAPI_FLAG_IS_SCHEDULED, &napi->state));
}

/* -------------------------------------------------------------------------- */

static inline void
netdev_rss_key_fill(uint32_t *buf, size_t len)
{

	/*
	 * Remembering from a previous life there was discussions on what is
	 * a good RSS hash key.  See end of rss_init() in net/rss_config.c.
	 * iwlwifi is looking for a 10byte "secret" so stay with random for now.
	 */
	get_random_bytes(buf, len);
}

static inline int
netdev_hw_addr_list_count(struct netdev_hw_addr_list *list)
{

	return (list->count);
}

static inline int
netdev_mc_count(struct net_device *ndev)
{

	return (netdev_hw_addr_list_count(&ndev->mc));
}

#define	netdev_hw_addr_list_for_each(_addr, _list)			\
	list_for_each_entry((_addr), &(_list)->addr_list, addr_list)

#define	netdev_for_each_mc_addr(na, ndev)				\
	netdev_hw_addr_list_for_each(na, &(ndev)->mc)

static __inline void
synchronize_net(void)
{

	/* We probably cannot do that unconditionally at some point anymore. */
	synchronize_rcu();
}

static __inline void
netif_receive_skb_list(struct list_head *head)
{

	pr_debug("%s: TODO\n", __func__);
}

static __inline int
napi_gro_receive(struct napi_struct *napi, struct sk_buff *skb)
{

	pr_debug("%s: TODO\n", __func__);
	return (-1);
}

static __inline void
ether_setup(struct net_device *ndev)
{

	pr_debug("%s: TODO\n", __func__);
}

static __inline void
dev_net_set(struct net_device *ndev, void *p)
{

	pr_debug("%s: TODO\n", __func__);
}

static __inline int
dev_set_threaded(struct net_device *ndev, bool threaded)
{

	pr_debug("%s: TODO\n", __func__);
	return (-ENODEV);
}

/* -------------------------------------------------------------------------- */

static __inline bool
netif_carrier_ok(struct net_device *ndev)
{
	pr_debug("%s: TODO\n", __func__);
	return (false);
}

static __inline void
netif_carrier_off(struct net_device *ndev)
{
	pr_debug("%s: TODO\n", __func__);
}

static __inline void
netif_carrier_on(struct net_device *ndev)
{
	pr_debug("%s: TODO\n", __func__);
}

/* -------------------------------------------------------------------------- */

static __inline bool
netif_queue_stopped(struct net_device *ndev)
{
	pr_debug("%s: TODO\n", __func__);
	return (false);
}

static __inline void
netif_stop_queue(struct net_device *ndev)
{
	pr_debug("%s: TODO\n", __func__);
}

static __inline void
netif_wake_queue(struct net_device *ndev)
{
	pr_debug("%s: TODO\n", __func__);
}

/* -------------------------------------------------------------------------- */

static __inline int
register_netdevice(struct net_device *ndev)
{

	/* assert rtnl_locked? */
	pr_debug("%s: TODO\n", __func__);
	return (0);
}

static __inline int
register_netdev(struct net_device *ndev)
{
	int error;

	/* lock */
	error = register_netdevice(ndev);
	/* unlock */
	pr_debug("%s: TODO\n", __func__);
	return (error);
}

static __inline void
unregister_netdev(struct net_device *ndev)
{
	pr_debug("%s: TODO\n", __func__);
}

static __inline void
unregister_netdevice(struct net_device *ndev)
{
	pr_debug("%s: TODO\n", __func__);
}

/* -------------------------------------------------------------------------- */

static __inline void
netif_rx(struct sk_buff *skb)
{
	pr_debug("%s: TODO\n", __func__);
}

static __inline void
netif_rx_ni(struct sk_buff *skb)
{
	pr_debug("%s: TODO\n", __func__);
}

/* -------------------------------------------------------------------------- */

struct net_device *linuxkpi_alloc_netdev(size_t, const char *, uint32_t,
    void(*)(struct net_device *));
void linuxkpi_free_netdev(struct net_device *);

#define	alloc_netdev(_l, _n, _f, _func)						\
	linuxkpi_alloc_netdev(_l, _n, _f, _func)
#define	free_netdev(_n)								\
	linuxkpi_free_netdev(_n)

static inline void *
netdev_priv(const struct net_device *ndev)
{

	return (__DECONST(void *, ndev->drv_priv));
}

/* -------------------------------------------------------------------------- */
/* This is really rtnetlink and probably belongs elsewhere. */

#define	rtnl_lock()		do { } while(0)
#define	rtnl_unlock()		do { } while(0)
#define	rcu_dereference_rtnl(x)	READ_ONCE(x)

#endif	/* _LINUXKPI_LINUX_NETDEVICE_H */
