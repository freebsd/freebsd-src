/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Routing netlink socket interface: protocol independent part.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 *	Fixes:
 *	Vitaly E. Lavrov		RTA_OK arithmetics was wrong.
 */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/sockios.h>
#include <linux/net.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/capability.h>
#include <linux/skbuff.h>
#include <linux/init.h>

#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/string.h>

#include <linux/inet.h>
#include <linux/netdevice.h>
#include <net/ip.h>
#include <net/protocol.h>
#include <net/arp.h>
#include <net/route.h>
#include <net/udp.h>
#include <net/sock.h>
#include <net/pkt_sched.h>

DECLARE_MUTEX(rtnl_sem);

void rtnl_lock(void)
{
	rtnl_shlock();
	rtnl_exlock();
}
 
void rtnl_unlock(void)
{
	rtnl_exunlock();
	rtnl_shunlock();
}

int rtattr_parse(struct rtattr *tb[], int maxattr, struct rtattr *rta, int len)
{
	memset(tb, 0, sizeof(struct rtattr*)*maxattr);

	while (RTA_OK(rta, len)) {
		unsigned flavor = rta->rta_type;
		if (flavor && flavor <= maxattr)
			tb[flavor-1] = rta;
		rta = RTA_NEXT(rta, len);
	}
	return 0;
}

struct sock *rtnl;

struct rtnetlink_link * rtnetlink_links[NPROTO];

static const int rtm_min[(RTM_MAX+1-RTM_BASE)/4] =
{
	NLMSG_LENGTH(sizeof(struct ifinfomsg)),
	NLMSG_LENGTH(sizeof(struct ifaddrmsg)),
	NLMSG_LENGTH(sizeof(struct rtmsg)),
	NLMSG_LENGTH(sizeof(struct ndmsg)),
	NLMSG_LENGTH(sizeof(struct rtmsg)),
	NLMSG_LENGTH(sizeof(struct tcmsg)),
	NLMSG_LENGTH(sizeof(struct tcmsg)),
	NLMSG_LENGTH(sizeof(struct tcmsg))
};

static const int rta_max[(RTM_MAX+1-RTM_BASE)/4] =
{
	IFLA_MAX,
	IFA_MAX,
	RTA_MAX,
	NDA_MAX,
	RTA_MAX,
	TCA_MAX,
	TCA_MAX,
	TCA_MAX
};

void __rta_fill(struct sk_buff *skb, int attrtype, int attrlen, const void *data)
{
	struct rtattr *rta;
	int size = RTA_LENGTH(attrlen);

	rta = (struct rtattr*)skb_put(skb, RTA_ALIGN(size));
	rta->rta_type = attrtype;
	rta->rta_len = size;
	memcpy(RTA_DATA(rta), data, attrlen);
}

int rtnetlink_send(struct sk_buff *skb, u32 pid, unsigned group, int echo)
{
	int err = 0;

	NETLINK_CB(skb).dst_groups = group;
	if (echo)
		atomic_inc(&skb->users);
	netlink_broadcast(rtnl, skb, pid, group, GFP_KERNEL);
	if (echo)
		err = netlink_unicast(rtnl, skb, pid, MSG_DONTWAIT);
	return err;
}

int rtnetlink_put_metrics(struct sk_buff *skb, unsigned *metrics)
{
	struct rtattr *mx = (struct rtattr*)skb->tail;
	int i;

	RTA_PUT(skb, RTA_METRICS, 0, NULL);
	for (i=0; i<RTAX_MAX; i++) {
		if (metrics[i])
			RTA_PUT(skb, i+1, sizeof(unsigned), metrics+i);
	}
	mx->rta_len = skb->tail - (u8*)mx;
	if (mx->rta_len == RTA_LENGTH(0))
		skb_trim(skb, (u8*)mx - skb->data);
	return 0;

rtattr_failure:
	skb_trim(skb, (u8*)mx - skb->data);
	return -1;
}


static int rtnetlink_fill_ifinfo(struct sk_buff *skb, struct net_device *dev,
				 int type, u32 pid, u32 seq, u32 change)
{
	struct ifinfomsg *r;
	struct nlmsghdr  *nlh;
	unsigned char	 *b = skb->tail;

	nlh = NLMSG_PUT(skb, pid, seq, type, sizeof(*r));
	if (pid) nlh->nlmsg_flags |= NLM_F_MULTI;
	r = NLMSG_DATA(nlh);
	r->ifi_family = AF_UNSPEC;
	r->ifi_type = dev->type;
	r->ifi_index = dev->ifindex;
	r->ifi_flags = dev->flags;
	r->ifi_change = change;

	if (!netif_running(dev) || !netif_carrier_ok(dev))
		r->ifi_flags &= ~IFF_RUNNING;
	else
		r->ifi_flags |= IFF_RUNNING;

	RTA_PUT(skb, IFLA_IFNAME, strlen(dev->name)+1, dev->name);
	if (dev->addr_len) {
		RTA_PUT(skb, IFLA_ADDRESS, dev->addr_len, dev->dev_addr);
		RTA_PUT(skb, IFLA_BROADCAST, dev->addr_len, dev->broadcast);
	}
	if (1) {
		unsigned mtu = dev->mtu;
		RTA_PUT(skb, IFLA_MTU, sizeof(mtu), &mtu);
	}
	if (dev->ifindex != dev->iflink)
		RTA_PUT(skb, IFLA_LINK, sizeof(int), &dev->iflink);
	if (dev->qdisc_sleeping)
		RTA_PUT(skb, IFLA_QDISC,
			strlen(dev->qdisc_sleeping->ops->id) + 1,
			dev->qdisc_sleeping->ops->id);
	if (dev->master)
		RTA_PUT(skb, IFLA_MASTER, sizeof(int), &dev->master->ifindex);
	if (dev->get_stats) {
		struct net_device_stats *stats = dev->get_stats(dev);
		if (stats)
			RTA_PUT(skb, IFLA_STATS, sizeof(*stats), stats);
	}
	nlh->nlmsg_len = skb->tail - b;
	return skb->len;

nlmsg_failure:
rtattr_failure:
	skb_trim(skb, b - skb->data);
	return -1;
}

int rtnetlink_dump_ifinfo(struct sk_buff *skb, struct netlink_callback *cb)
{
	int idx;
	int s_idx = cb->args[0];
	struct net_device *dev;

	read_lock(&dev_base_lock);
	for (dev=dev_base, idx=0; dev; dev = dev->next, idx++) {
		if (idx < s_idx)
			continue;
		if (rtnetlink_fill_ifinfo(skb, dev, RTM_NEWLINK, NETLINK_CB(cb->skb).pid, cb->nlh->nlmsg_seq, 0) <= 0)
			break;
	}
	read_unlock(&dev_base_lock);
	cb->args[0] = idx;

	return skb->len;
}

int rtnetlink_dump_all(struct sk_buff *skb, struct netlink_callback *cb)
{
	int idx;
	int s_idx = cb->family;

	if (s_idx == 0)
		s_idx = 1;
	for (idx=1; idx<NPROTO; idx++) {
		int type = cb->nlh->nlmsg_type-RTM_BASE;
		if (idx < s_idx || idx == PF_PACKET)
			continue;
		if (rtnetlink_links[idx] == NULL ||
		    rtnetlink_links[idx][type].dumpit == NULL)
			continue;
		if (idx > s_idx)
			memset(&cb->args[0], 0, sizeof(cb->args));
		if (rtnetlink_links[idx][type].dumpit(skb, cb))
			break;
	}
	cb->family = idx;

	return skb->len;
}

void rtmsg_ifinfo(int type, struct net_device *dev, unsigned change)
{
	struct sk_buff *skb;
	int size = NLMSG_GOODSIZE;

	skb = alloc_skb(size, GFP_KERNEL);
	if (!skb)
		return;

	if (rtnetlink_fill_ifinfo(skb, dev, type, 0, 0, change) < 0) {
		kfree_skb(skb);
		return;
	}
	NETLINK_CB(skb).dst_groups = RTMGRP_LINK;
	netlink_broadcast(rtnl, skb, 0, RTMGRP_LINK, GFP_KERNEL);
}

static int rtnetlink_done(struct netlink_callback *cb)
{
	return 0;
}

/* Process one rtnetlink message. */

static __inline__ int
rtnetlink_rcv_msg(struct sk_buff *skb, struct nlmsghdr *nlh, int *errp)
{
	struct rtnetlink_link *link;
	struct rtnetlink_link *link_tab;
	struct rtattr	*rta[RTATTR_MAX];

	int exclusive = 0;
	int sz_idx, kind;
	int min_len;
	int family;
	int type;
	int err;

	/* Only requests are handled by kernel now */
	if (!(nlh->nlmsg_flags&NLM_F_REQUEST))
		return 0;

	type = nlh->nlmsg_type;

	/* A control message: ignore them */
	if (type < RTM_BASE)
		return 0;

	/* Unknown message: reply with EINVAL */
	if (type > RTM_MAX)
		goto err_inval;

	type -= RTM_BASE;

	/* All the messages must have at least 1 byte length */
	if (nlh->nlmsg_len < NLMSG_LENGTH(sizeof(struct rtgenmsg)))
		return 0;

	family = ((struct rtgenmsg*)NLMSG_DATA(nlh))->rtgen_family;
	if (family > NPROTO) {
		*errp = -EAFNOSUPPORT;
		return -1;
	}

	link_tab = rtnetlink_links[family];
	if (link_tab == NULL)
		link_tab = rtnetlink_links[PF_UNSPEC];
	link = &link_tab[type];

	sz_idx = type>>2;
	kind = type&3;

	if (kind != 2 && !cap_raised(NETLINK_CB(skb).eff_cap, CAP_NET_ADMIN)) {
		*errp = -EPERM;
		return -1;
	}

	if (kind == 2 && nlh->nlmsg_flags&NLM_F_DUMP) {
		u32 rlen;

		if (link->dumpit == NULL)
			link = &(rtnetlink_links[PF_UNSPEC][type]);

		if (link->dumpit == NULL)
			goto err_inval;

		if ((*errp = netlink_dump_start(rtnl, skb, nlh,
						link->dumpit,
						rtnetlink_done)) != 0) {
			return -1;
		}
		rlen = NLMSG_ALIGN(nlh->nlmsg_len);
		if (rlen > skb->len)
			rlen = skb->len;
		skb_pull(skb, rlen);
		return -1;
	}

	if (kind != 2) {
		if (rtnl_exlock_nowait()) {
			*errp = 0;
			return -1;
		}
		exclusive = 1;
	}

	memset(&rta, 0, sizeof(rta));

	min_len = rtm_min[sz_idx];
	if (nlh->nlmsg_len < min_len)
		goto err_inval;

	if (nlh->nlmsg_len > min_len) {
		int attrlen = nlh->nlmsg_len - NLMSG_ALIGN(min_len);
		struct rtattr *attr = (void*)nlh + NLMSG_ALIGN(min_len);

		while (RTA_OK(attr, attrlen)) {
			unsigned flavor = attr->rta_type;
			if (flavor) {
				if (flavor > rta_max[sz_idx])
					goto err_inval;
				rta[flavor-1] = attr;
			}
			attr = RTA_NEXT(attr, attrlen);
		}
	}

	if (link->doit == NULL)
		link = &(rtnetlink_links[PF_UNSPEC][type]);
	if (link->doit == NULL)
		goto err_inval;
	err = link->doit(skb, nlh, (void *)&rta);

	if (exclusive)
		rtnl_exunlock();
	*errp = err;
	return err;

err_inval:
	if (exclusive)
		rtnl_exunlock();
	*errp = -EINVAL;
	return -1;
}

/* 
 * Process one packet of messages.
 * Malformed skbs with wrong lengths of messages are discarded silently.
 */

static inline int rtnetlink_rcv_skb(struct sk_buff *skb)
{
	int err;
	struct nlmsghdr * nlh;

	while (skb->len >= NLMSG_SPACE(0)) {
		u32 rlen;

		nlh = (struct nlmsghdr *)skb->data;
		if (nlh->nlmsg_len < sizeof(*nlh) || skb->len < nlh->nlmsg_len)
			return 0;
		rlen = NLMSG_ALIGN(nlh->nlmsg_len);
		if (rlen > skb->len)
			rlen = skb->len;
		if (rtnetlink_rcv_msg(skb, nlh, &err)) {
			/* Not error, but we must interrupt processing here:
			 *   Note, that in this case we do not pull message
			 *   from skb, it will be processed later.
			 */
			if (err == 0)
				return -1;
			netlink_ack(skb, nlh, err);
		} else if (nlh->nlmsg_flags&NLM_F_ACK)
			netlink_ack(skb, nlh, 0);
		skb_pull(skb, rlen);
	}

	return 0;
}

/*
 *  rtnetlink input queue processing routine:
 *	- try to acquire shared lock. If it is failed, defer processing.
 *	- feed skbs to rtnetlink_rcv_skb, until it refuse a message,
 *	  that will occur, when a dump started and/or acquisition of
 *	  exclusive lock failed.
 */

static void rtnetlink_rcv(struct sock *sk, int len)
{
	do {
		struct sk_buff *skb;

		if (rtnl_shlock_nowait())
			return;

		while ((skb = skb_dequeue(&sk->receive_queue)) != NULL) {
			if (rtnetlink_rcv_skb(skb)) {
				if (skb->len)
					skb_queue_head(&sk->receive_queue, skb);
				else
					kfree_skb(skb);
				break;
			}
			kfree_skb(skb);
		}

		up(&rtnl_sem);
	} while (rtnl && rtnl->receive_queue.qlen);
}

static struct rtnetlink_link link_rtnetlink_table[RTM_MAX-RTM_BASE+1] =
{
	{ NULL,			NULL,			},
	{ NULL,			NULL,			},
	{ NULL,			rtnetlink_dump_ifinfo,	},
	{ NULL,			NULL,			},

	{ NULL,			NULL,			},
	{ NULL,			NULL,			},
	{ NULL,			rtnetlink_dump_all,	},
	{ NULL,			NULL,			},

	{ NULL,			NULL,			},
	{ NULL,			NULL,			},
	{ NULL,			rtnetlink_dump_all,	},
	{ NULL,			NULL,			},

	{ neigh_add,		NULL,			},
	{ neigh_delete,		NULL,			},
	{ NULL,			neigh_dump_info,	},
	{ NULL,			NULL,			},

	{ NULL,			NULL,			},
	{ NULL,			NULL,			},
	{ NULL,			NULL,			},
	{ NULL,			NULL,			},
};


static int rtnetlink_event(struct notifier_block *this, unsigned long event, void *ptr)
{
	struct net_device *dev = ptr;
	switch (event) {
	case NETDEV_UNREGISTER:
		rtmsg_ifinfo(RTM_DELLINK, dev, ~0U);
		break;
	case NETDEV_REGISTER:
		rtmsg_ifinfo(RTM_NEWLINK, dev, ~0U);
		break;
	case NETDEV_UP:
	case NETDEV_DOWN:
		rtmsg_ifinfo(RTM_NEWLINK, dev, IFF_UP|IFF_RUNNING);
		break;
	case NETDEV_CHANGE:
	case NETDEV_GOING_DOWN:
		break;
	default:
		rtmsg_ifinfo(RTM_NEWLINK, dev, 0);
		break;
	}
	return NOTIFY_DONE;
}

struct notifier_block rtnetlink_dev_notifier = {
	rtnetlink_event,
	NULL,
	0
};


void __init rtnetlink_init(void)
{
#ifdef RTNL_DEBUG
	printk("Initializing RT netlink socket\n");
#endif
	rtnl = netlink_kernel_create(NETLINK_ROUTE, rtnetlink_rcv);
	if (rtnl == NULL)
		panic("rtnetlink_init: cannot initialize rtnetlink\n");
	netlink_set_nonroot(NETLINK_ROUTE, NL_NONROOT_RECV);
	register_netdevice_notifier(&rtnetlink_dev_notifier);
	rtnetlink_links[PF_UNSPEC] = link_rtnetlink_table;
	rtnetlink_links[PF_PACKET] = link_rtnetlink_table;
}
