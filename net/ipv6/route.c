/*
 *	Linux INET6 implementation
 *	FIB front-end.
 *
 *	Authors:
 *	Pedro Roque		<roque@di.fc.ul.pt>	
 *
 *	$Id: route.c,v 1.56 2001/10/31 21:55:55 davem Exp $
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

/*	Changes:
 *
 *	YOSHIFUJI Hideaki @USAGI
 *		reworked default router selection.
 *		- respect outgoing interface
 *		- select from (probably) reachable routers (i.e.
 *		routers in REACHABLE, STALE, DELAY or PROBE states).
 *		- always select the same router if it is (probably)
 *		reachable.  otherwise, round-robin the list.
 */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/net.h>
#include <linux/route.h>
#include <linux/netdevice.h>
#include <linux/in6.h>
#include <linux/init.h>
#include <linux/netlink.h>
#include <linux/if_arp.h>

#ifdef 	CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif

#include <net/snmp.h>
#include <net/ipv6.h>
#include <net/ip6_fib.h>
#include <net/ip6_route.h>
#include <net/ndisc.h>
#include <net/addrconf.h>
#include <net/tcp.h>
#include <linux/rtnetlink.h>

#include <asm/uaccess.h>

#ifdef CONFIG_SYSCTL
#include <linux/sysctl.h>
#endif

#undef CONFIG_RT6_POLICY

/* Set to 3 to get tracing. */
#define RT6_DEBUG 2

#if RT6_DEBUG >= 3
#define RDBG(x) printk x
#define RT6_TRACE(x...) printk(KERN_DEBUG x)
#else
#define RDBG(x)
#define RT6_TRACE(x...) do { ; } while (0)
#endif


int ip6_rt_max_size = 4096;
int ip6_rt_gc_min_interval = HZ / 2;
int ip6_rt_gc_timeout = 60*HZ;
int ip6_rt_gc_interval = 30*HZ;
int ip6_rt_gc_elasticity = 9;
int ip6_rt_mtu_expires = 10*60*HZ;
int ip6_rt_min_advmss = IPV6_MIN_MTU - 20 - 40;

static struct rt6_info * ip6_rt_copy(struct rt6_info *ort);
static struct dst_entry	*ip6_dst_check(struct dst_entry *dst, u32 cookie);
static struct dst_entry	*ip6_dst_reroute(struct dst_entry *dst,
					 struct sk_buff *skb);
static struct dst_entry *ip6_negative_advice(struct dst_entry *);
static int		 ip6_dst_gc(void);

static int		ip6_pkt_discard(struct sk_buff *skb);
static void		ip6_link_failure(struct sk_buff *skb);

struct dst_ops ip6_dst_ops = {
	AF_INET6,
	__constant_htons(ETH_P_IPV6),
	1024,

        ip6_dst_gc,
	ip6_dst_check,
	ip6_dst_reroute,
	NULL,
	ip6_negative_advice,
	ip6_link_failure,
	sizeof(struct rt6_info),
};

struct rt6_info ip6_null_entry = {
	{{NULL, ATOMIC_INIT(1), 1, &loopback_dev,
	  -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	  -ENETUNREACH, NULL, NULL,
	  ip6_pkt_discard, ip6_pkt_discard,
#ifdef CONFIG_NET_CLS_ROUTE
	  0,
#endif
	  &ip6_dst_ops}},
	NULL, {{{0}}}, RTF_REJECT|RTF_NONEXTHOP, ~0U,
	255, ATOMIC_INIT(1), {NULL}, {{{{0}}}, 0}, {{{{0}}}, 0}
};

struct fib6_node ip6_routing_table = {
	NULL, NULL, NULL, NULL,
	&ip6_null_entry,
	0, RTN_ROOT|RTN_TL_ROOT|RTN_RTINFO, 0
};

#ifdef CONFIG_RT6_POLICY
int	ip6_rt_policy = 0;

struct pol_chain *rt6_pol_list = NULL;


static int rt6_flow_match_in(struct rt6_info *rt, struct sk_buff *skb);
static int rt6_flow_match_out(struct rt6_info *rt, struct sock *sk);

static struct rt6_info	*rt6_flow_lookup(struct rt6_info *rt,
					 struct in6_addr *daddr,
					 struct in6_addr *saddr,
					 struct fl_acc_args *args);

#else
#define ip6_rt_policy (0)
#endif

/* Protects all the ip6 fib */

rwlock_t rt6_lock = RW_LOCK_UNLOCKED;


/*
 *	Route lookup. Any rt6_lock is implied.
 */

static __inline__ struct rt6_info *rt6_device_match(struct rt6_info *rt,
						    int oif,
						    int strict)
{
	struct rt6_info *local = NULL;
	struct rt6_info *sprt;

	if (oif) {
		for (sprt = rt; sprt; sprt = sprt->u.next) {
			struct net_device *dev = sprt->rt6i_dev;
			if (dev->ifindex == oif)
				return sprt;
			if (dev->flags&IFF_LOOPBACK)
				local = sprt;
		}

		if (local)
			return local;

		if (strict)
			return &ip6_null_entry;
	}
	return rt;
}

/*
 *	pointer to the last default router chosen. BH is disabled locally.
 */
static struct rt6_info *rt6_dflt_pointer = NULL;
static spinlock_t rt6_dflt_lock = SPIN_LOCK_UNLOCKED;

/* Default Router Selection (RFC 2461 6.3.6) */
static struct rt6_info *rt6_best_dflt(struct rt6_info *rt, int oif)
{
	struct rt6_info *match = NULL;
	struct rt6_info *sprt;
	int mpri = 0;

	for (sprt = rt; sprt; sprt = sprt->u.next) {
		struct neighbour *neigh;
		int m = 0;

		if (!oif ||
		    (sprt->rt6i_dev &&
		     sprt->rt6i_dev->ifindex == oif))
			m += 8;

		if (sprt == rt6_dflt_pointer)
			m += 4;

		if ((neigh = sprt->rt6i_nexthop) != NULL) {
			read_lock_bh(&neigh->lock);
			switch (neigh->nud_state) {
			case NUD_REACHABLE:
				m += 3;
				break;

			case NUD_STALE:
			case NUD_DELAY:
			case NUD_PROBE:
				m += 2;
				break;

			case NUD_NOARP:
			case NUD_PERMANENT:
				m += 1;
				break;

			case NUD_INCOMPLETE:
			default:
				read_unlock_bh(&neigh->lock);
				continue;
			}
			read_unlock_bh(&neigh->lock);
		} else {
			continue;
		}

		if (m > mpri || m >= 12) {
			match = sprt;
			mpri = m;
			if (m >= 12) {
				/* we choose the lastest default router if it
				 * is in (probably) reachable state.
				 * If route changed, we should do pmtu
				 * discovery. --yoshfuji
				 */
				break;
			}
		}
	}

	spin_lock(&rt6_dflt_lock);
	if (!match) {
		/*
		 *	No default routers are known to be reachable.
		 *	SHOULD round robin
		 */
		if (rt6_dflt_pointer) {
			for (sprt = rt6_dflt_pointer->u.next;
			     sprt; sprt = sprt->u.next) {
				if (sprt->u.dst.obsolete <= 0 &&
				    sprt->u.dst.error == 0) {
					match = sprt;
					break;
				}
			}
			for (sprt = rt;
			     !match && sprt;
			     sprt = sprt->u.next) {
				if (sprt->u.dst.obsolete <= 0 &&
				    sprt->u.dst.error == 0) {
					match = sprt;
					break;
				}
				if (sprt == rt6_dflt_pointer)
					break;
			}
		}
	}

	if (match)
		rt6_dflt_pointer = match;

	spin_unlock(&rt6_dflt_lock);

	if (!match) {
		/*
		 * Last Resort: if no default routers found, 
		 * use addrconf default route.
		 * We don't record this route.
		 */
		for (sprt = ip6_routing_table.leaf;
		     sprt; sprt = sprt->u.next) {
			if ((sprt->rt6i_flags & RTF_DEFAULT) &&
			    (!oif ||
			     (sprt->rt6i_dev &&
			      sprt->rt6i_dev->ifindex == oif))) {
				match = sprt;
				break;
			}
		}
		if (!match) {
			/* no default route.  give up. */
			match = &ip6_null_entry;
		}
	}

	return match;
}

struct rt6_info *rt6_lookup(struct in6_addr *daddr, struct in6_addr *saddr,
			    int oif, int strict)
{
	struct fib6_node *fn;
	struct rt6_info *rt;

	read_lock_bh(&rt6_lock);
	fn = fib6_lookup(&ip6_routing_table, daddr, saddr);
	rt = rt6_device_match(fn->leaf, oif, strict);
	dst_hold(&rt->u.dst);
	rt->u.dst.__use++;
	read_unlock_bh(&rt6_lock);

	rt->u.dst.lastuse = jiffies;
	if (rt->u.dst.error == 0)
		return rt;
	dst_release(&rt->u.dst);
	return NULL;
}

/* rt6_ins is called with FREE rt6_lock.
   It takes new route entry, the addition fails by any reason the
   route is freed. In any case, if caller does not hold it, it may
   be destroyed.
 */

static int rt6_ins(struct rt6_info *rt, struct nlmsghdr *nlh)
{
	int err;

	write_lock_bh(&rt6_lock);
	err = fib6_add(&ip6_routing_table, rt, nlh);
	write_unlock_bh(&rt6_lock);

	return err;
}

/* No rt6_lock! If COW failed, the function returns dead route entry
   with dst->error set to errno value.
 */

static struct rt6_info *rt6_cow(struct rt6_info *ort, struct in6_addr *daddr,
				struct in6_addr *saddr)
{
	int err;
	struct rt6_info *rt;

	/*
	 *	Clone the route.
	 */

	rt = ip6_rt_copy(ort);

	if (rt) {
		ipv6_addr_copy(&rt->rt6i_dst.addr, daddr);

		if (!(rt->rt6i_flags&RTF_GATEWAY))
			ipv6_addr_copy(&rt->rt6i_gateway, daddr);

		rt->rt6i_dst.plen = 128;
		rt->rt6i_flags |= RTF_CACHE;
		rt->u.dst.flags |= DST_HOST;

#ifdef CONFIG_IPV6_SUBTREES
		if (rt->rt6i_src.plen && saddr) {
			ipv6_addr_copy(&rt->rt6i_src.addr, saddr);
			rt->rt6i_src.plen = 128;
		}
#endif

		rt->rt6i_nexthop = ndisc_get_neigh(rt->rt6i_dev, &rt->rt6i_gateway);

		dst_hold(&rt->u.dst);

		err = rt6_ins(rt, NULL);
		if (err == 0)
			return rt;

		rt->u.dst.error = err;

		return rt;
	}
	dst_hold(&ip6_null_entry.u.dst);
	return &ip6_null_entry;
}

#ifdef CONFIG_RT6_POLICY
static __inline__ struct rt6_info *rt6_flow_lookup_in(struct rt6_info *rt,
						      struct sk_buff *skb)
{
	struct in6_addr *daddr, *saddr;
	struct fl_acc_args arg;

	arg.type = FL_ARG_FORWARD;
	arg.fl_u.skb = skb;

	saddr = &skb->nh.ipv6h->saddr;
	daddr = &skb->nh.ipv6h->daddr;

	return rt6_flow_lookup(rt, daddr, saddr, &arg);
}

static __inline__ struct rt6_info *rt6_flow_lookup_out(struct rt6_info *rt,
						       struct sock *sk,
						       struct flowi *fl)
{
	struct fl_acc_args arg;

	arg.type = FL_ARG_ORIGIN;
	arg.fl_u.fl_o.sk = sk;
	arg.fl_u.fl_o.flow = fl;

	return rt6_flow_lookup(rt, fl->nl_u.ip6_u.daddr, fl->nl_u.ip6_u.saddr,
			       &arg);
}

#endif

#define BACKTRACK() \
if (rt == &ip6_null_entry && strict) { \
       while ((fn = fn->parent) != NULL) { \
		if (fn->fn_flags & RTN_ROOT) { \
			dst_hold(&rt->u.dst); \
			goto out; \
		} \
		if (fn->fn_flags & RTN_RTINFO) \
			goto restart; \
	} \
}


void ip6_route_input(struct sk_buff *skb)
{
	struct fib6_node *fn;
	struct rt6_info *rt;
	int strict;
	int attempts = 3;

	strict = ipv6_addr_type(&skb->nh.ipv6h->daddr) & (IPV6_ADDR_MULTICAST|IPV6_ADDR_LINKLOCAL);

relookup:
	read_lock_bh(&rt6_lock);

	fn = fib6_lookup(&ip6_routing_table, &skb->nh.ipv6h->daddr,
			 &skb->nh.ipv6h->saddr);

restart:
	rt = fn->leaf;

	if ((rt->rt6i_flags & RTF_CACHE)) {
		if (ip6_rt_policy == 0) {
			rt = rt6_device_match(rt, skb->dev->ifindex, strict);
			BACKTRACK();
			dst_hold(&rt->u.dst);
			goto out;
		}

#ifdef CONFIG_RT6_POLICY
		if ((rt->rt6i_flags & RTF_FLOW)) {
			struct rt6_info *sprt;

			for (sprt = rt; sprt; sprt = sprt->u.next) {
				if (rt6_flow_match_in(sprt, skb)) {
					rt = sprt;
					dst_hold(&rt->u.dst);
					goto out;
				}
			}
		}
#endif
	}

	rt = rt6_device_match(rt, skb->dev->ifindex, 0);
	BACKTRACK();

	if (ip6_rt_policy == 0) {
		if (!rt->rt6i_nexthop && !(rt->rt6i_flags & RTF_NONEXTHOP)) {
			read_unlock_bh(&rt6_lock);

			rt = rt6_cow(rt, &skb->nh.ipv6h->daddr,
				     &skb->nh.ipv6h->saddr);
			
			if (rt->u.dst.error != -EEXIST || --attempts <= 0)
				goto out2;
			/* Race condition! In the gap, when rt6_lock was
			   released someone could insert this route.  Relookup.
			 */
			goto relookup;
		}
		dst_hold(&rt->u.dst);
	} else {
#ifdef CONFIG_RT6_POLICY
		rt = rt6_flow_lookup_in(rt, skb);
#else
		/* NEVER REACHED */
#endif
	}

out:
	read_unlock_bh(&rt6_lock);
out2:
	rt->u.dst.lastuse = jiffies;
	rt->u.dst.__use++;
	skb->dst = (struct dst_entry *) rt;
}

struct dst_entry * ip6_route_output(struct sock *sk, struct flowi *fl)
{
	struct fib6_node *fn;
	struct rt6_info *rt;
	int strict;
	int attempts = 3;

	strict = ipv6_addr_type(fl->nl_u.ip6_u.daddr) & (IPV6_ADDR_MULTICAST|IPV6_ADDR_LINKLOCAL);

relookup:
	read_lock_bh(&rt6_lock);

	fn = fib6_lookup(&ip6_routing_table, fl->nl_u.ip6_u.daddr,
			 fl->nl_u.ip6_u.saddr);

restart:
	rt = fn->leaf;

	if ((rt->rt6i_flags & RTF_CACHE)) {
		if (ip6_rt_policy == 0) {
			rt = rt6_device_match(rt, fl->oif, strict);
			BACKTRACK();
			dst_hold(&rt->u.dst);
			goto out;
		}

#ifdef CONFIG_RT6_POLICY
		if ((rt->rt6i_flags & RTF_FLOW)) {
			struct rt6_info *sprt;

			for (sprt = rt; sprt; sprt = sprt->u.next) {
				if (rt6_flow_match_out(sprt, sk)) {
					rt = sprt;
					dst_hold(&rt->u.dst);
					goto out;
				}
			}
		}
#endif
	}
	if (rt->rt6i_flags & RTF_DEFAULT) {
		if (rt->rt6i_metric >= IP6_RT_PRIO_ADDRCONF)
			rt = rt6_best_dflt(rt, fl->oif);
	} else {
		rt = rt6_device_match(rt, fl->oif, strict);
		BACKTRACK();
	}

	if (ip6_rt_policy == 0) {
		if (!rt->rt6i_nexthop && !(rt->rt6i_flags & RTF_NONEXTHOP)) {
			read_unlock_bh(&rt6_lock);

			rt = rt6_cow(rt, fl->nl_u.ip6_u.daddr,
				     fl->nl_u.ip6_u.saddr);
			
			if (rt->u.dst.error != -EEXIST || --attempts <= 0)
				goto out2;

			/* Race condition! In the gap, when rt6_lock was
			   released someone could insert this route.  Relookup.
			 */
			goto relookup;
		}
		dst_hold(&rt->u.dst);
	} else {
#ifdef CONFIG_RT6_POLICY
		rt = rt6_flow_lookup_out(rt, sk, fl);
#else
		/* NEVER REACHED */
#endif
	}

out:
	read_unlock_bh(&rt6_lock);
out2:
	rt->u.dst.lastuse = jiffies;
	rt->u.dst.__use++;
	return &rt->u.dst;
}


/*
 *	Destination cache support functions
 */

static struct dst_entry *ip6_dst_check(struct dst_entry *dst, u32 cookie)
{
	struct rt6_info *rt;

	rt = (struct rt6_info *) dst;

	if (rt && rt->rt6i_node && (rt->rt6i_node->fn_sernum == cookie))
		return dst;

	dst_release(dst);
	return NULL;
}

static struct dst_entry *ip6_dst_reroute(struct dst_entry *dst, struct sk_buff *skb)
{
	/*
	 *	FIXME
	 */
	RDBG(("ip6_dst_reroute(%p,%p)[%p] (AIEEE)\n", dst, skb,
	      __builtin_return_address(0)));
	return NULL;
}

static struct dst_entry *ip6_negative_advice(struct dst_entry *dst)
{
	struct rt6_info *rt = (struct rt6_info *) dst;

	if (rt) {
		if (rt->rt6i_flags & RTF_CACHE)
			ip6_del_rt(rt, NULL);
		else
			dst_release(dst);
	}
	return NULL;
}

static void ip6_link_failure(struct sk_buff *skb)
{
	struct rt6_info *rt;

	icmpv6_send(skb, ICMPV6_DEST_UNREACH, ICMPV6_ADDR_UNREACH, 0, skb->dev);

	rt = (struct rt6_info *) skb->dst;
	if (rt) {
		if (rt->rt6i_flags&RTF_CACHE) {
			dst_set_expires(&rt->u.dst, 0);
			rt->rt6i_flags |= RTF_EXPIRES;
		} else if (rt->rt6i_node && (rt->rt6i_flags & RTF_DEFAULT))
			rt->rt6i_node->fn_sernum = -1;
	}
}

static int ip6_dst_gc()
{
	static unsigned expire = 30*HZ;
	static unsigned long last_gc;
	unsigned long now = jiffies;

	if (time_after(last_gc + ip6_rt_gc_min_interval, now) &&
	    atomic_read(&ip6_dst_ops.entries) <= ip6_rt_max_size)
		goto out;

	expire++;
	fib6_run_gc(expire);
	last_gc = now;
	if (atomic_read(&ip6_dst_ops.entries) < ip6_dst_ops.gc_thresh)
		expire = ip6_rt_gc_timeout>>1;

out:
	expire -= expire>>ip6_rt_gc_elasticity;
	return (atomic_read(&ip6_dst_ops.entries) > ip6_rt_max_size);
}

/* Clean host part of a prefix. Not necessary in radix tree,
   but results in cleaner routing tables.

   Remove it only when all the things will work!
 */

static void ipv6_addr_prefix(struct in6_addr *pfx,
			     const struct in6_addr *addr, int plen)
{
	int b = plen&0x7;
	int o = plen>>3;

	memcpy(pfx->s6_addr, addr, o);
	if (o < 16)
		memset(pfx->s6_addr + o, 0, 16 - o);
	if (b != 0)
		pfx->s6_addr[o] = addr->s6_addr[o]&(0xff00 >> b);
}

static int ipv6_get_mtu(struct net_device *dev)
{
	int mtu = IPV6_MIN_MTU;
	struct inet6_dev *idev;

	idev = in6_dev_get(dev);
	if (idev) {
		mtu = idev->cnf.mtu6;
		in6_dev_put(idev);
	}
	return mtu;
}

static int ipv6_get_hoplimit(struct net_device *dev)
{
	int hoplimit = ipv6_devconf.hop_limit;
	struct inet6_dev *idev;

	idev = in6_dev_get(dev);
	if (idev) {
		hoplimit = idev->cnf.hop_limit;
		in6_dev_put(idev);
	}
	return hoplimit;
}

/*
 *
 */

int ip6_route_add(struct in6_rtmsg *rtmsg, struct nlmsghdr *nlh)
{
	int err;
	struct rtmsg *r;
	struct rt6_info *rt;
	struct net_device *dev = NULL;
	int addr_type;

	if (rtmsg->rtmsg_dst_len > 128 || rtmsg->rtmsg_src_len > 128)
		return -EINVAL;
#ifndef CONFIG_IPV6_SUBTREES
	if (rtmsg->rtmsg_src_len)
		return -EINVAL;
#endif
	if (rtmsg->rtmsg_metric == 0)
		rtmsg->rtmsg_metric = IP6_RT_PRIO_USER;

	rt = dst_alloc(&ip6_dst_ops);

	if (rt == NULL)
		return -ENOMEM;

	rt->u.dst.obsolete = -1;
	rt->rt6i_expires = rtmsg->rtmsg_info;
	if (nlh && (r = NLMSG_DATA(nlh))) {
		rt->rt6i_protocol = r->rtm_protocol;
	} else {
		rt->rt6i_protocol = RTPROT_BOOT;
	}

	addr_type = ipv6_addr_type(&rtmsg->rtmsg_dst);

	if (addr_type & IPV6_ADDR_MULTICAST)
		rt->u.dst.input = ip6_mc_input;
	else
		rt->u.dst.input = ip6_forward;

	rt->u.dst.output = ip6_output;

	if (rtmsg->rtmsg_ifindex) {
		dev = dev_get_by_index(rtmsg->rtmsg_ifindex);
		err = -ENODEV;
		if (dev == NULL)
			goto out;
	}

	ipv6_addr_prefix(&rt->rt6i_dst.addr, 
			 &rtmsg->rtmsg_dst, rtmsg->rtmsg_dst_len);
	rt->rt6i_dst.plen = rtmsg->rtmsg_dst_len;
	if (rt->rt6i_dst.plen == 128)
	       rt->u.dst.flags = DST_HOST;

#ifdef CONFIG_IPV6_SUBTREES
	ipv6_addr_prefix(&rt->rt6i_src.addr, 
			 &rtmsg->rtmsg_src, rtmsg->rtmsg_src_len);
	rt->rt6i_src.plen = rtmsg->rtmsg_src_len;
#endif

	rt->rt6i_metric = rtmsg->rtmsg_metric;

	/* We cannot add true routes via loopback here,
	   they would result in kernel looping; promote them to reject routes
	 */
	if ((rtmsg->rtmsg_flags&RTF_REJECT) ||
	    (dev && (dev->flags&IFF_LOOPBACK) && !(addr_type&IPV6_ADDR_LOOPBACK))) {
		if (dev)
			dev_put(dev);
		dev = &loopback_dev;
		dev_hold(dev);
		rt->u.dst.output = ip6_pkt_discard;
		rt->u.dst.input = ip6_pkt_discard;
		rt->u.dst.error = -ENETUNREACH;
		rt->rt6i_flags = RTF_REJECT|RTF_NONEXTHOP;
		goto install_route;
	}

	if (rtmsg->rtmsg_flags & RTF_GATEWAY) {
		struct in6_addr *gw_addr;
		int gwa_type;

		gw_addr = &rtmsg->rtmsg_gateway;
		ipv6_addr_copy(&rt->rt6i_gateway, &rtmsg->rtmsg_gateway);
		gwa_type = ipv6_addr_type(gw_addr);

		if (gwa_type != (IPV6_ADDR_LINKLOCAL|IPV6_ADDR_UNICAST)) {
			struct rt6_info *grt;

			/* IPv6 strictly inhibits using not link-local
			   addresses as nexthop address.
			   Otherwise, router will not able to send redirects.
			   It is very good, but in some (rare!) curcumstances
			   (SIT, PtP, NBMA NOARP links) it is handy to allow
			   some exceptions. --ANK
			 */
			err = -EINVAL;
			if (!(gwa_type&IPV6_ADDR_UNICAST))
				goto out;

			grt = rt6_lookup(gw_addr, NULL, rtmsg->rtmsg_ifindex, 1);

			err = -EHOSTUNREACH;
			if (grt == NULL)
				goto out;
			if (dev) {
				if (dev != grt->rt6i_dev) {
					dst_release(&grt->u.dst);
					goto out;
				}
			} else {
				dev = grt->rt6i_dev;
				dev_hold(dev);
			}
			if (!(grt->rt6i_flags&RTF_GATEWAY))
				err = 0;
			dst_release(&grt->u.dst);

			if (err)
				goto out;
		}
		err = -EINVAL;
		if (dev == NULL || (dev->flags&IFF_LOOPBACK))
			goto out;
	}

	err = -ENODEV;
	if (dev == NULL)
		goto out;

	if (rtmsg->rtmsg_flags & (RTF_GATEWAY|RTF_NONEXTHOP)) {
		rt->rt6i_nexthop = __neigh_lookup_errno(&nd_tbl, &rt->rt6i_gateway, dev);
		if (IS_ERR(rt->rt6i_nexthop)) {
			err = PTR_ERR(rt->rt6i_nexthop);
			rt->rt6i_nexthop = NULL;
			goto out;
		}
	}

	if (ipv6_addr_is_multicast(&rt->rt6i_dst.addr))
		rt->rt6i_hoplimit = IPV6_DEFAULT_MCASTHOPS;
	else
		rt->rt6i_hoplimit = ipv6_get_hoplimit(dev);
	rt->rt6i_flags = rtmsg->rtmsg_flags;

install_route:
	rt->u.dst.pmtu = ipv6_get_mtu(dev);
	rt->u.dst.advmss = max_t(unsigned int, rt->u.dst.pmtu - 60, ip6_rt_min_advmss);
	/* Maximal non-jumbo IPv6 payload is 65535 and corresponding
	   MSS is 65535 - tcp_header_size. 65535 is also valid and
	   means: "any MSS, rely only on pmtu discovery"
	 */
	if (rt->u.dst.advmss > 65535-20)
		rt->u.dst.advmss = 65535;
	rt->u.dst.dev = dev;
	return rt6_ins(rt, nlh);

out:
	if (dev)
		dev_put(dev);
	dst_free((struct dst_entry *) rt);
	return err;
}

int ip6_del_rt(struct rt6_info *rt, struct nlmsghdr *nlh)
{
	int err;

	write_lock_bh(&rt6_lock);

	spin_lock_bh(&rt6_dflt_lock);
	rt6_dflt_pointer = NULL;
	spin_unlock_bh(&rt6_dflt_lock);

	dst_release(&rt->u.dst);

	err = fib6_del(rt, nlh);
	write_unlock_bh(&rt6_lock);

	return err;
}

int ip6_route_del(struct in6_rtmsg *rtmsg, struct nlmsghdr *nlh)
{
	struct fib6_node *fn;
	struct rt6_info *rt;
	int err = -ESRCH;

	read_lock_bh(&rt6_lock);

	fn = fib6_locate(&ip6_routing_table,
			 &rtmsg->rtmsg_dst, rtmsg->rtmsg_dst_len,
			 &rtmsg->rtmsg_src, rtmsg->rtmsg_src_len);
	
	if (fn) {
		for (rt = fn->leaf; rt; rt = rt->u.next) {
			if (rtmsg->rtmsg_ifindex &&
			    (rt->rt6i_dev == NULL ||
			     rt->rt6i_dev->ifindex != rtmsg->rtmsg_ifindex))
				continue;
			if (rtmsg->rtmsg_flags&RTF_GATEWAY &&
			    ipv6_addr_cmp(&rtmsg->rtmsg_gateway, &rt->rt6i_gateway))
				continue;
			if (rtmsg->rtmsg_metric &&
			    rtmsg->rtmsg_metric != rt->rt6i_metric)
				continue;
			dst_hold(&rt->u.dst);
			read_unlock_bh(&rt6_lock);

			return ip6_del_rt(rt, nlh);
		}
	}
	read_unlock_bh(&rt6_lock);

	return err;
}

/*
 *	Handle redirects
 */
void rt6_redirect(struct in6_addr *dest, struct in6_addr *saddr,
		  struct neighbour *neigh, int on_link)
{
	struct rt6_info *rt, *nrt;

	/* Locate old route to this destination. */
	rt = rt6_lookup(dest, NULL, neigh->dev->ifindex, 1);

	if (rt == NULL)
		return;

	if (neigh->dev != rt->rt6i_dev)
		goto out;

	/* Redirect received -> path was valid.
	   Look, redirects are sent only in response to data packets,
	   so that this nexthop apparently is reachable. --ANK
	 */
	dst_confirm(&rt->u.dst);

	/* Duplicate redirect: silently ignore. */
	if (neigh == rt->u.dst.neighbour)
		goto out;

	/* Current route is on-link; redirect is always invalid.
	   
	   Seems, previous statement is not true. It could
	   be node, which looks for us as on-link (f.e. proxy ndisc)
	   But then router serving it might decide, that we should
	   know truth 8)8) --ANK (980726).
	 */
	if (!(rt->rt6i_flags&RTF_GATEWAY))
		goto out;

	/*
	 *	RFC 1970 specifies that redirects should only be
	 *	accepted if they come from the nexthop to the target.
	 *	Due to the way default routers are chosen, this notion
	 *	is a bit fuzzy and one might need to check all default
	 *	routers.
	 */

	if (ipv6_addr_cmp(saddr, &rt->rt6i_gateway)) {
		if (rt->rt6i_flags & RTF_DEFAULT) {
			struct rt6_info *rt1;

			read_lock(&rt6_lock);
			for (rt1 = ip6_routing_table.leaf; rt1; rt1 = rt1->u.next) {
				if (!ipv6_addr_cmp(saddr, &rt1->rt6i_gateway)) {
					dst_hold(&rt1->u.dst);
					dst_release(&rt->u.dst);
					read_unlock(&rt6_lock);
					rt = rt1;
					goto source_ok;
				}
			}
			read_unlock(&rt6_lock);
		}
		if (net_ratelimit())
			printk(KERN_DEBUG "rt6_redirect: source isn't a valid nexthop "
			       "for redirect target\n");
		goto out;
	}

source_ok:

	/*
	 *	We have finally decided to accept it.
	 */

	nrt = ip6_rt_copy(rt);
	if (nrt == NULL)
		goto out;

	nrt->rt6i_flags = RTF_GATEWAY|RTF_UP|RTF_DYNAMIC|RTF_CACHE;
	if (on_link)
		nrt->rt6i_flags &= ~RTF_GATEWAY;

	ipv6_addr_copy(&nrt->rt6i_dst.addr, dest);
	nrt->rt6i_dst.plen = 128;
	nrt->u.dst.flags |= DST_HOST;

	ipv6_addr_copy(&nrt->rt6i_gateway, (struct in6_addr*)neigh->primary_key);
	nrt->rt6i_nexthop = neigh_clone(neigh);
	/* Reset pmtu, it may be better */
	nrt->u.dst.pmtu = ipv6_get_mtu(neigh->dev);
	nrt->u.dst.advmss = max_t(unsigned int, nrt->u.dst.pmtu - 60, ip6_rt_min_advmss);
	if (rt->u.dst.advmss > 65535-20)
		rt->u.dst.advmss = 65535;
	nrt->rt6i_hoplimit = ipv6_get_hoplimit(neigh->dev);

	if (rt6_ins(nrt, NULL))
		goto out;

	if (rt->rt6i_flags&RTF_CACHE) {
		ip6_del_rt(rt, NULL);
		return;
	}

out:
        dst_release(&rt->u.dst);
	return;
}

/*
 *	Handle ICMP "packet too big" messages
 *	i.e. Path MTU discovery
 */

void rt6_pmtu_discovery(struct in6_addr *daddr, struct in6_addr *saddr,
			struct net_device *dev, u32 pmtu)
{
	struct rt6_info *rt, *nrt;

	if (pmtu < IPV6_MIN_MTU) {
		if (net_ratelimit())
			printk(KERN_DEBUG "rt6_pmtu_discovery: invalid MTU value %d\n",
			       pmtu);
		/* According to RFC1981, the PMTU is set to the IPv6 minimum
		   link MTU if the node receives a Packet Too Big message
		   reporting next-hop MTU that is less than the IPv6 minimum MTU.
		 */	
		pmtu = IPV6_MIN_MTU;
	}

	rt = rt6_lookup(daddr, saddr, dev->ifindex, 0);

	if (rt == NULL)
		return;

	if (pmtu >= rt->u.dst.pmtu)
		goto out;

	/* New mtu received -> path was valid.
	   They are sent only in response to data packets,
	   so that this nexthop apparently is reachable. --ANK
	 */
	dst_confirm(&rt->u.dst);

	/* Host route. If it is static, it would be better
	   not to override it, but add new one, so that
	   when cache entry will expire old pmtu
	   would return automatically.
	 */
	if (rt->rt6i_flags & RTF_CACHE) {
		rt->u.dst.pmtu = pmtu;
		dst_set_expires(&rt->u.dst, ip6_rt_mtu_expires);
		rt->rt6i_flags |= RTF_MODIFIED|RTF_EXPIRES;
		goto out;
	}

	/* Network route.
	   Two cases are possible:
	   1. It is connected route. Action: COW
	   2. It is gatewayed route or NONEXTHOP route. Action: clone it.
	 */
	if (!rt->rt6i_nexthop && !(rt->rt6i_flags & RTF_NONEXTHOP)) {
		nrt = rt6_cow(rt, daddr, saddr);
		if (!nrt->u.dst.error) {
			nrt->u.dst.pmtu = pmtu;
			/* According to RFC 1981, detecting PMTU increase shouldn't be
			   happened within 5 mins, the recommended timer is 10 mins.
			   Here this route expiration time is set to ip6_rt_mtu_expires 
			   which is 10 mins. After 10 mins the decreased pmtu is expired
			   and detecting PMTU increase will be automatically happened.
			 */
			dst_set_expires(&nrt->u.dst, ip6_rt_mtu_expires);
			nrt->rt6i_flags |= RTF_DYNAMIC|RTF_EXPIRES;
			dst_release(&nrt->u.dst);
		}
	} else {
		nrt = ip6_rt_copy(rt);
		if (nrt == NULL)
			goto out;
		ipv6_addr_copy(&nrt->rt6i_dst.addr, daddr);
		nrt->rt6i_dst.plen = 128;
		nrt->u.dst.flags |= DST_HOST;
		nrt->rt6i_nexthop = neigh_clone(rt->rt6i_nexthop);
		dst_set_expires(&nrt->u.dst, ip6_rt_mtu_expires);
		nrt->rt6i_flags |= RTF_DYNAMIC|RTF_CACHE|RTF_EXPIRES;
		nrt->u.dst.pmtu = pmtu;
		rt6_ins(nrt, NULL);
	}

out:
	dst_release(&rt->u.dst);
}

/*
 *	Misc support functions
 */

static struct rt6_info * ip6_rt_copy(struct rt6_info *ort)
{
	struct rt6_info *rt;

	rt = dst_alloc(&ip6_dst_ops);

	if (rt) {
		rt->u.dst.input = ort->u.dst.input;
		rt->u.dst.output = ort->u.dst.output;

		memcpy(&rt->u.dst.mxlock, &ort->u.dst.mxlock, RTAX_MAX*sizeof(unsigned));
		rt->u.dst.dev = ort->u.dst.dev;
		if (rt->u.dst.dev)
			dev_hold(rt->u.dst.dev);
		rt->u.dst.lastuse = jiffies;
		rt->rt6i_hoplimit = ort->rt6i_hoplimit;
		rt->rt6i_expires = 0;

		ipv6_addr_copy(&rt->rt6i_gateway, &ort->rt6i_gateway);
		rt->rt6i_flags = ort->rt6i_flags & ~RTF_EXPIRES;
		rt->rt6i_metric = 0;

		memcpy(&rt->rt6i_dst, &ort->rt6i_dst, sizeof(struct rt6key));
#ifdef CONFIG_IPV6_SUBTREES
		memcpy(&rt->rt6i_src, &ort->rt6i_src, sizeof(struct rt6key));
#endif
	}
	return rt;
}

struct rt6_info *rt6_get_dflt_router(struct in6_addr *addr, struct net_device *dev)
{	
	struct rt6_info *rt;
	struct fib6_node *fn;

	fn = &ip6_routing_table;

	write_lock_bh(&rt6_lock);
	for (rt = fn->leaf; rt; rt=rt->u.next) {
		if (dev == rt->rt6i_dev &&
		    ipv6_addr_cmp(&rt->rt6i_gateway, addr) == 0)
			break;
	}
	if (rt)
		dst_hold(&rt->u.dst);
	write_unlock_bh(&rt6_lock);
	return rt;
}

struct rt6_info *rt6_add_dflt_router(struct in6_addr *gwaddr,
				     struct net_device *dev)
{
	struct in6_rtmsg rtmsg;

	memset(&rtmsg, 0, sizeof(struct in6_rtmsg));
	rtmsg.rtmsg_type = RTMSG_NEWROUTE;
	ipv6_addr_copy(&rtmsg.rtmsg_gateway, gwaddr);
	rtmsg.rtmsg_metric = 1024;
	rtmsg.rtmsg_flags = RTF_GATEWAY | RTF_ADDRCONF | RTF_DEFAULT | RTF_UP;

	rtmsg.rtmsg_ifindex = dev->ifindex;

	ip6_route_add(&rtmsg, NULL);
	return rt6_get_dflt_router(gwaddr, dev);
}

void rt6_purge_dflt_routers(int last_resort)
{
	struct rt6_info *rt;
	u32 flags;

	if (last_resort)
		flags = RTF_ALLONLINK;
	else
		flags = RTF_DEFAULT | RTF_ADDRCONF;	

restart:
	read_lock_bh(&rt6_lock);
	for (rt = ip6_routing_table.leaf; rt; rt = rt->u.next) {
		if (rt->rt6i_flags & flags) {
			dst_hold(&rt->u.dst);

			spin_lock_bh(&rt6_dflt_lock);
			rt6_dflt_pointer = NULL;
			spin_unlock_bh(&rt6_dflt_lock);

			read_unlock_bh(&rt6_lock);

			ip6_del_rt(rt, NULL);

			goto restart;
		}
	}
	read_unlock_bh(&rt6_lock);
}

int ipv6_route_ioctl(unsigned int cmd, void *arg)
{
	struct in6_rtmsg rtmsg;
	int err;

	switch(cmd) {
	case SIOCADDRT:		/* Add a route */
	case SIOCDELRT:		/* Delete a route */
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		err = copy_from_user(&rtmsg, arg,
				     sizeof(struct in6_rtmsg));
		if (err)
			return -EFAULT;
			
		rtnl_lock();
		switch (cmd) {
		case SIOCADDRT:
			err = ip6_route_add(&rtmsg, NULL);
			break;
		case SIOCDELRT:
			err = ip6_route_del(&rtmsg, NULL);
			break;
		default:
			err = -EINVAL;
		}
		rtnl_unlock();

		return err;
	};

	return -EINVAL;
}

/*
 *	Drop the packet on the floor
 */

int ip6_pkt_discard(struct sk_buff *skb)
{
	IP6_INC_STATS(Ip6OutNoRoutes);
	icmpv6_send(skb, ICMPV6_DEST_UNREACH, ICMPV6_NOROUTE, 0, skb->dev);
	kfree_skb(skb);
	return 0;
}

/*
 *	Add address
 */

int ip6_rt_addr_add(struct in6_addr *addr, struct net_device *dev)
{
	struct rt6_info *rt;

	rt = dst_alloc(&ip6_dst_ops);
	if (rt == NULL)
		return -ENOMEM;

	rt->u.dst.flags = DST_HOST;
	rt->u.dst.input = ip6_input;
	rt->u.dst.output = ip6_output;
	rt->rt6i_dev = dev_get_by_name("lo");
	rt->u.dst.pmtu = ipv6_get_mtu(rt->rt6i_dev);
	rt->u.dst.advmss = max_t(unsigned int, rt->u.dst.pmtu - 60, ip6_rt_min_advmss);
	if (rt->u.dst.advmss > 65535-20)
		rt->u.dst.advmss = 65535;
	rt->rt6i_hoplimit = ipv6_get_hoplimit(rt->rt6i_dev);
	rt->u.dst.obsolete = -1;

	rt->rt6i_flags = RTF_UP | RTF_NONEXTHOP;
	rt->rt6i_nexthop = ndisc_get_neigh(rt->rt6i_dev, &rt->rt6i_gateway);
	if (rt->rt6i_nexthop == NULL) {
		dst_free((struct dst_entry *) rt);
		return -ENOMEM;
	}

	ipv6_addr_copy(&rt->rt6i_dst.addr, addr);
	rt->rt6i_dst.plen = 128;
	rt6_ins(rt, NULL);

	return 0;
}

/* Delete address. Warning: you should check that this address
   disappeared before calling this function.
 */

int ip6_rt_addr_del(struct in6_addr *addr, struct net_device *dev)
{
	struct rt6_info *rt;
	int err = -ENOENT;

	rt = rt6_lookup(addr, NULL, loopback_dev.ifindex, 1);
	if (rt) {
		if (rt->rt6i_dst.plen == 128)
			err = ip6_del_rt(rt, NULL);
		else
			dst_release(&rt->u.dst);
	}

	return err;
}

#ifdef CONFIG_RT6_POLICY

static int rt6_flow_match_in(struct rt6_info *rt, struct sk_buff *skb)
{
	struct flow_filter *frule;
	struct pkt_filter *filter;
	int res = 1;

	if ((frule = rt->rt6i_filter) == NULL)
		goto out;

	if (frule->type != FLR_INPUT) {
		res = 0;
		goto out;
	}

	for (filter = frule->u.filter; filter; filter = filter->next) {
		__u32 *word;

		word = (__u32 *) skb->h.raw;
		word += filter->offset;

		if ((*word ^ filter->value) & filter->mask) {
			res = 0;
			break;
		}
	}

out:
	return res;
}

static int rt6_flow_match_out(struct rt6_info *rt, struct sock *sk)
{
	struct flow_filter *frule;
	int res = 1;

	if ((frule = rt->rt6i_filter) == NULL)
		goto out;

	if (frule->type != FLR_INPUT) {
		res = 0;
		goto out;
	}

	if (frule->u.sk != sk)
		res = 0;
out:
	return res;
}

static struct rt6_info *rt6_flow_lookup(struct rt6_info *rt,
					struct in6_addr *daddr,
					struct in6_addr *saddr,
					struct fl_acc_args *args)
{
	struct flow_rule *frule;
	struct rt6_info *nrt = NULL;
	struct pol_chain *pol;

	for (pol = rt6_pol_list; pol; pol = pol->next) {
		struct fib6_node *fn;
		struct rt6_info *sprt;

		fn = fib6_lookup(pol->rules, daddr, saddr);

		do {
			for (sprt = fn->leaf; sprt; sprt=sprt->u.next) {
				int res;

				frule = sprt->rt6i_flowr;
#if RT6_DEBUG >= 2
				if (frule == NULL) {
					printk(KERN_DEBUG "NULL flowr\n");
					goto error;
				}
#endif
				res = frule->ops->accept(rt, sprt, args, &nrt);

				switch (res) {
				case FLOWR_SELECT:
					goto found;
				case FLOWR_CLEAR:
					goto next_policy;
				case FLOWR_NODECISION:
					break;
				default:
					goto error;
				};
			}

			fn = fn->parent;

		} while ((fn->fn_flags & RTN_TL_ROOT) == 0);

	next_policy:
	}

error:
	dst_hold(&ip6_null_entry.u.dst);
	return &ip6_null_entry;

found:
	if (nrt == NULL)
		goto error;

	nrt->rt6i_flags |= RTF_CACHE;
	dst_hold(&nrt->u.dst);
	err = rt6_ins(nrt, NULL);
	if (err)
		nrt->u.dst.error = err;
	return nrt;
}
#endif

static int fib6_ifdown(struct rt6_info *rt, void *arg)
{
	if (((void*)rt->rt6i_dev == arg || arg == NULL) &&
	    rt != &ip6_null_entry) {
		RT6_TRACE("deleted by ifdown %p\n", rt);
		return -1;
	}
	return 0;
}

void rt6_ifdown(struct net_device *dev)
{
	write_lock_bh(&rt6_lock);
	fib6_clean_tree(&ip6_routing_table, fib6_ifdown, 0, dev);
	write_unlock_bh(&rt6_lock);
}

struct rt6_mtu_change_arg
{
	struct net_device *dev;
	unsigned mtu;
};

static int rt6_mtu_change_route(struct rt6_info *rt, void *p_arg)
{
	struct rt6_mtu_change_arg *arg = (struct rt6_mtu_change_arg *) p_arg;
	struct inet6_dev *idev;
	/* In IPv6 pmtu discovery is not optional,
	   so that RTAX_MTU lock cannot disable it.
	   We still use this lock to block changes
	   caused by addrconf/ndisc.
	*/
	idev = __in6_dev_get(arg->dev);
	if (idev == NULL)
		return 0;

	/* For administrative MTU increase, there is no way to discover 
	   IPv6 PMTU increase, so PMTU increase should be updated here.
	   Since RFC 1981 doesn't include administrative MTU increase
	   update PMTU increase is a MUST. (i.e. jumbo frame)
	 */
	/*
	   If new MTU is less than route PMTU, this new MTU will be the 
	   lowest MTU in the path, update the route PMTU to refect PMTU 
	   decreases; if new MTU is greater than route PMTU, and the 
	   old MTU is the lowest MTU in the path, update the route PMTU 
	   to refect the increase. In this case if the other nodes' MTU
	   also have the lowest MTU, TOO BIG MESSAGE will be lead to 
	   PMTU discouvery. 
	 */
	if (rt->rt6i_dev == arg->dev &&
	    !(rt->u.dst.mxlock&(1<<RTAX_MTU)) &&
	      (rt->u.dst.pmtu > arg->mtu ||
	       (rt->u.dst.pmtu < arg->mtu &&
		rt->u.dst.pmtu == idev->cnf.mtu6)))
		rt->u.dst.pmtu = arg->mtu;
	rt->u.dst.advmss = max_t(unsigned int, arg->mtu - 60, ip6_rt_min_advmss);
	if (rt->u.dst.advmss > 65535-20)
		rt->u.dst.advmss = 65535;
	return 0;
}

void rt6_mtu_change(struct net_device *dev, unsigned mtu)
{
	struct rt6_mtu_change_arg arg;

	arg.dev = dev;
	arg.mtu = mtu;
	read_lock_bh(&rt6_lock);
	fib6_clean_tree(&ip6_routing_table, rt6_mtu_change_route, 0, &arg);
	read_unlock_bh(&rt6_lock);
}

static int inet6_rtm_to_rtmsg(struct rtmsg *r, struct rtattr **rta,
			      struct in6_rtmsg *rtmsg)
{
	memset(rtmsg, 0, sizeof(*rtmsg));

	rtmsg->rtmsg_dst_len = r->rtm_dst_len;
	rtmsg->rtmsg_src_len = r->rtm_src_len;
	rtmsg->rtmsg_flags = RTF_UP;
	if (r->rtm_type == RTN_UNREACHABLE)
		rtmsg->rtmsg_flags |= RTF_REJECT;

	if (rta[RTA_GATEWAY-1]) {
		if (rta[RTA_GATEWAY-1]->rta_len != RTA_LENGTH(16))
			return -EINVAL;
		memcpy(&rtmsg->rtmsg_gateway, RTA_DATA(rta[RTA_GATEWAY-1]), 16);
		rtmsg->rtmsg_flags |= RTF_GATEWAY;
	}
	if (rta[RTA_DST-1]) {
		if (RTA_PAYLOAD(rta[RTA_DST-1]) < ((r->rtm_dst_len+7)>>3))
			return -EINVAL;
		memcpy(&rtmsg->rtmsg_dst, RTA_DATA(rta[RTA_DST-1]), ((r->rtm_dst_len+7)>>3));
	}
	if (rta[RTA_SRC-1]) {
		if (RTA_PAYLOAD(rta[RTA_SRC-1]) < ((r->rtm_src_len+7)>>3))
			return -EINVAL;
		memcpy(&rtmsg->rtmsg_src, RTA_DATA(rta[RTA_SRC-1]), ((r->rtm_src_len+7)>>3));
	}
	if (rta[RTA_OIF-1]) {
		if (rta[RTA_OIF-1]->rta_len != RTA_LENGTH(sizeof(int)))
			return -EINVAL;
		memcpy(&rtmsg->rtmsg_ifindex, RTA_DATA(rta[RTA_OIF-1]), sizeof(int));
	}
	if (rta[RTA_PRIORITY-1]) {
		if (rta[RTA_PRIORITY-1]->rta_len != RTA_LENGTH(4))
			return -EINVAL;
		memcpy(&rtmsg->rtmsg_metric, RTA_DATA(rta[RTA_PRIORITY-1]), 4);
	}
	return 0;
}

int inet6_rtm_delroute(struct sk_buff *skb, struct nlmsghdr* nlh, void *arg)
{
	struct rtmsg *r = NLMSG_DATA(nlh);
	struct in6_rtmsg rtmsg;

	if (inet6_rtm_to_rtmsg(r, arg, &rtmsg))
		return -EINVAL;
	return ip6_route_del(&rtmsg, nlh);
}

int inet6_rtm_newroute(struct sk_buff *skb, struct nlmsghdr* nlh, void *arg)
{
	struct rtmsg *r = NLMSG_DATA(nlh);
	struct in6_rtmsg rtmsg;

	if (inet6_rtm_to_rtmsg(r, arg, &rtmsg))
		return -EINVAL;
	return ip6_route_add(&rtmsg, nlh);
}

struct rt6_rtnl_dump_arg
{
	struct sk_buff *skb;
	struct netlink_callback *cb;
};

static int rt6_fill_node(struct sk_buff *skb, struct rt6_info *rt,
			 struct in6_addr *dst,
			 struct in6_addr *src,
			 int iif,
			 int type, u32 pid, u32 seq,
			 struct nlmsghdr *in_nlh, int prefix)
{
	struct rtmsg *rtm;
	struct nlmsghdr  *nlh;
	unsigned char	 *b = skb->tail;
	struct rta_cacheinfo ci;

	if (prefix) {	/* user wants prefix routes only */
		if (!(rt->rt6i_flags & RTF_PREFIX_RT)) {
			/* success since this is not a prefix route */
			return 1;
		}
	}
	if (!pid && in_nlh) {
		pid = in_nlh->nlmsg_pid;
	}

	nlh = NLMSG_PUT(skb, pid, seq, type, sizeof(*rtm));
	rtm = NLMSG_DATA(nlh);
	rtm->rtm_family = AF_INET6;
	rtm->rtm_dst_len = rt->rt6i_dst.plen;
	rtm->rtm_src_len = rt->rt6i_src.plen;
	rtm->rtm_tos = 0;
	rtm->rtm_table = RT_TABLE_MAIN;
	if (rt->rt6i_flags&RTF_REJECT)
		rtm->rtm_type = RTN_UNREACHABLE;
	else if (rt->rt6i_dev && (rt->rt6i_dev->flags&IFF_LOOPBACK))
		rtm->rtm_type = RTN_LOCAL;
	else
		rtm->rtm_type = RTN_UNICAST;
	rtm->rtm_flags = 0;
	rtm->rtm_scope = RT_SCOPE_UNIVERSE;
	rtm->rtm_protocol = rt->rt6i_protocol;
	if (rt->rt6i_flags&RTF_DYNAMIC)
		rtm->rtm_protocol = RTPROT_REDIRECT;
	else if (rt->rt6i_flags&(RTF_ADDRCONF|RTF_ALLONLINK))
		rtm->rtm_protocol = RTPROT_KERNEL;
	else if (rt->rt6i_flags&RTF_DEFAULT)
		rtm->rtm_protocol = RTPROT_RA;

	if (rt->rt6i_flags&RTF_CACHE)
		rtm->rtm_flags |= RTM_F_CLONED;

	if (dst) {
		RTA_PUT(skb, RTA_DST, 16, dst);
	        rtm->rtm_dst_len = 128;
	} else if (rtm->rtm_dst_len)
		RTA_PUT(skb, RTA_DST, 16, &rt->rt6i_dst.addr);
#ifdef CONFIG_IPV6_SUBTREES
	if (src) {
		RTA_PUT(skb, RTA_SRC, 16, src);
	        rtm->rtm_src_len = 128;
	} else if (rtm->rtm_src_len)
		RTA_PUT(skb, RTA_SRC, 16, &rt->rt6i_src.addr);
#endif
	if (iif)
		RTA_PUT(skb, RTA_IIF, 4, &iif);
	else if (dst) {
		struct in6_addr saddr_buf;
		if (ipv6_get_saddr(&rt->u.dst, dst, &saddr_buf) == 0)
			RTA_PUT(skb, RTA_PREFSRC, 16, &saddr_buf);
	}
	if (rtnetlink_put_metrics(skb, &rt->u.dst.mxlock) < 0)
		goto rtattr_failure;
	if (rt->u.dst.neighbour)
		RTA_PUT(skb, RTA_GATEWAY, 16, &rt->u.dst.neighbour->primary_key);
	if (rt->u.dst.dev)
		RTA_PUT(skb, RTA_OIF, sizeof(int), &rt->rt6i_dev->ifindex);
	RTA_PUT(skb, RTA_PRIORITY, 4, &rt->rt6i_metric);
	ci.rta_lastuse = jiffies - rt->u.dst.lastuse;
	if (rt->rt6i_expires)
		ci.rta_expires = rt->rt6i_expires - jiffies;
	else
		ci.rta_expires = 0;
	ci.rta_used = rt->u.dst.__use;
	ci.rta_clntref = atomic_read(&rt->u.dst.__refcnt);
	ci.rta_error = rt->u.dst.error;
	ci.rta_id = 0;
	ci.rta_ts = 0;
	ci.rta_tsage = 0;
	RTA_PUT(skb, RTA_CACHEINFO, sizeof(ci), &ci);
	nlh->nlmsg_len = skb->tail - b;
	return skb->len;

nlmsg_failure:
rtattr_failure:
	skb_trim(skb, b - skb->data);
	return -1;
}

static int rt6_dump_route(struct rt6_info *rt, void *p_arg)
{
	struct rt6_rtnl_dump_arg *arg = (struct rt6_rtnl_dump_arg *) p_arg;
	int prefix;

	if (arg->cb->nlh->nlmsg_len >= NLMSG_LENGTH(sizeof(struct rtmsg))) {
		struct rtmsg *rtm = NLMSG_DATA(arg->cb->nlh);
		prefix = (rtm->rtm_flags & RTM_F_PREFIX) != 0;
	} else
		prefix = 0;

	return rt6_fill_node(arg->skb, rt, NULL, NULL, 0, RTM_NEWROUTE,
		     NETLINK_CB(arg->cb->skb).pid, arg->cb->nlh->nlmsg_seq,
		     NULL, prefix);
}

static int fib6_dump_node(struct fib6_walker_t *w)
{
	int res;
	struct rt6_info *rt;

	for (rt = w->leaf; rt; rt = rt->u.next) {
		res = rt6_dump_route(rt, w->args);
		if (res < 0) {
			/* Frame is full, suspend walking */
			w->leaf = rt;
			return 1;
		}
		BUG_TRAP(res!=0);
	}
	w->leaf = NULL;
	return 0;
}

static void fib6_dump_end(struct netlink_callback *cb)
{
	struct fib6_walker_t *w = (void*)cb->args[0];

	if (w) {
		cb->args[0] = 0;
		fib6_walker_unlink(w);
		kfree(w);
	}
	if (cb->args[1]) {
		cb->done = (void*)cb->args[1];
		cb->args[1] = 0;
	}
}

static int fib6_dump_done(struct netlink_callback *cb)
{
	fib6_dump_end(cb);
	return cb->done(cb);
}

int inet6_dump_fib(struct sk_buff *skb, struct netlink_callback *cb)
{
	struct rt6_rtnl_dump_arg arg;
	struct fib6_walker_t *w;
	int res;

	arg.skb = skb;
	arg.cb = cb;

	w = (void*)cb->args[0];
	if (w == NULL) {
		/* New dump:
		 * 
		 * 1. hook callback destructor.
		 */
		cb->args[1] = (long)cb->done;
		cb->done = fib6_dump_done;

		/*
		 * 2. allocate and initialize walker.
		 */
		w = kmalloc(sizeof(*w), GFP_ATOMIC);
		if (w == NULL)
			return -ENOMEM;
		RT6_TRACE("dump<%p", w);
		memset(w, 0, sizeof(*w));
		w->root = &ip6_routing_table;
		w->func = fib6_dump_node;
		w->args = &arg;
		cb->args[0] = (long)w;
		read_lock_bh(&rt6_lock);
		res = fib6_walk(w);
		read_unlock_bh(&rt6_lock);
	} else {
		w->args = &arg;
		read_lock_bh(&rt6_lock);
		res = fib6_walk_continue(w);
		read_unlock_bh(&rt6_lock);
	}
#if RT6_DEBUG >= 3
	if (res <= 0 && skb->len == 0)
		RT6_TRACE("%p>dump end\n", w);
#endif
	res = res < 0 ? res : skb->len;
	/* res < 0 is an error. (really, impossible)
	   res == 0 means that dump is complete, but skb still can contain data.
	   res > 0 dump is not complete, but frame is full.
	 */
	/* Destroy walker, if dump of this table is complete. */
	if (res <= 0)
		fib6_dump_end(cb);
	return res;
}

int inet6_rtm_getroute(struct sk_buff *in_skb, struct nlmsghdr* nlh, void *arg)
{
	struct rtattr **rta = arg;
	int iif = 0;
	int err = -ENOBUFS;
	struct sk_buff *skb;
	struct flowi fl;
	struct rt6_info *rt;

	skb = alloc_skb(NLMSG_GOODSIZE, GFP_KERNEL);
	if (skb == NULL)
		goto out;

	/* Reserve room for dummy headers, this skb can pass
	   through good chunk of routing engine.
	 */
	skb->mac.raw = skb->data;
	skb_reserve(skb, MAX_HEADER + sizeof(struct ipv6hdr));

	fl.proto = 0;
	fl.nl_u.ip6_u.daddr = NULL;
	fl.nl_u.ip6_u.saddr = NULL;
	fl.uli_u.icmpt.type = 0;
	fl.uli_u.icmpt.code = 0;
	if (rta[RTA_SRC-1])
		fl.nl_u.ip6_u.saddr = (struct in6_addr*)RTA_DATA(rta[RTA_SRC-1]);
	if (rta[RTA_DST-1])
		fl.nl_u.ip6_u.daddr = (struct in6_addr*)RTA_DATA(rta[RTA_DST-1]);

	if (rta[RTA_IIF-1])
		memcpy(&iif, RTA_DATA(rta[RTA_IIF-1]), sizeof(int));

	if (iif) {
		struct net_device *dev;
		dev = __dev_get_by_index(iif);
		if (!dev) {
			err = -ENODEV;
			goto out_free;
		}
	}

	fl.oif = 0;
	if (rta[RTA_OIF-1])
		memcpy(&fl.oif, RTA_DATA(rta[RTA_OIF-1]), sizeof(int));

	rt = (struct rt6_info*)ip6_route_output(NULL, &fl);

	skb->dst = &rt->u.dst;

	NETLINK_CB(skb).dst_pid = NETLINK_CB(in_skb).pid;
	err = rt6_fill_node(skb, rt, 
			    fl.nl_u.ip6_u.daddr,
			    fl.nl_u.ip6_u.saddr,
			    iif,
			    RTM_NEWROUTE, NETLINK_CB(in_skb).pid,
			    nlh->nlmsg_seq, nlh, 0);
	if (err < 0) {
		err = -EMSGSIZE;
		goto out_free;
	}

	err = netlink_unicast(rtnl, skb, NETLINK_CB(in_skb).pid, MSG_DONTWAIT);
	if (err > 0)
		err = 0;
out:
	return err;
out_free:
	kfree_skb(skb);
	goto out;	
}

void inet6_rt_notify(int event, struct rt6_info *rt, struct nlmsghdr *nlh)
{
	struct sk_buff *skb;
	int size = NLMSG_SPACE(sizeof(struct rtmsg)+256);

	skb = alloc_skb(size, gfp_any());
	if (!skb) {
		netlink_set_err(rtnl, 0, RTMGRP_IPV6_ROUTE, ENOBUFS);
		return;
	}
	if (rt6_fill_node(skb, rt, NULL, NULL, 0, event, 0, 0, nlh, 0) < 0) {
		kfree_skb(skb);
		netlink_set_err(rtnl, 0, RTMGRP_IPV6_ROUTE, EINVAL);
		return;
	}
	NETLINK_CB(skb).dst_groups = RTMGRP_IPV6_ROUTE;
	netlink_broadcast(rtnl, skb, 0, RTMGRP_IPV6_ROUTE, gfp_any());
}

/*
 *	/proc
 */

#ifdef CONFIG_PROC_FS

#define RT6_INFO_LEN (32 + 4 + 32 + 4 + 32 + 40 + 5 + 1)

struct rt6_proc_arg
{
	char *buffer;
	int offset;
	int length;
	int skip;
	int len;
};

static int rt6_info_route(struct rt6_info *rt, void *p_arg)
{
	struct rt6_proc_arg *arg = (struct rt6_proc_arg *) p_arg;
	int i;

	if (arg->skip < arg->offset / RT6_INFO_LEN) {
		arg->skip++;
		return 0;
	}

	if (arg->len >= arg->length)
		return 0;

	for (i=0; i<16; i++) {
		sprintf(arg->buffer + arg->len, "%02x",
			rt->rt6i_dst.addr.s6_addr[i]);
		arg->len += 2;
	}
	arg->len += sprintf(arg->buffer + arg->len, " %02x ",
			    rt->rt6i_dst.plen);

#ifdef CONFIG_IPV6_SUBTREES
	for (i=0; i<16; i++) {
		sprintf(arg->buffer + arg->len, "%02x",
			rt->rt6i_src.addr.s6_addr[i]);
		arg->len += 2;
	}
	arg->len += sprintf(arg->buffer + arg->len, " %02x ",
			    rt->rt6i_src.plen);
#else
	sprintf(arg->buffer + arg->len,
		"00000000000000000000000000000000 00 ");
	arg->len += 36;
#endif

	if (rt->rt6i_nexthop) {
		for (i=0; i<16; i++) {
			sprintf(arg->buffer + arg->len, "%02x",
				rt->rt6i_nexthop->primary_key[i]);
			arg->len += 2;
		}
	} else {
		sprintf(arg->buffer + arg->len,
			"00000000000000000000000000000000");
		arg->len += 32;
	}
	arg->len += sprintf(arg->buffer + arg->len,
			    " %08x %08x %08x %08x %8s\n",
			    rt->rt6i_metric, atomic_read(&rt->u.dst.__refcnt),
			    rt->u.dst.__use, rt->rt6i_flags, 
			    rt->rt6i_dev ? rt->rt6i_dev->name : "");
	return 0;
}

static int rt6_proc_info(char *buffer, char **start, off_t offset, int length)
{
	struct rt6_proc_arg arg;
	arg.buffer = buffer;
	arg.offset = offset;
	arg.length = length;
	arg.skip = 0;
	arg.len = 0;

	read_lock_bh(&rt6_lock);
	fib6_clean_tree(&ip6_routing_table, rt6_info_route, 0, &arg);
	read_unlock_bh(&rt6_lock);

	*start = buffer;
	if (offset)
		*start += offset % RT6_INFO_LEN;

	arg.len -= offset % RT6_INFO_LEN;

	if (arg.len > length)
		arg.len = length;
	if (arg.len < 0)
		arg.len = 0;

	return arg.len;
}

extern struct rt6_statistics rt6_stats;

static int rt6_proc_stats(char *buffer, char **start, off_t offset, int length)
{
	int len;

	len = sprintf(buffer, "%04x %04x %04x %04x %04x %04x\n",
		      rt6_stats.fib_nodes, rt6_stats.fib_route_nodes,
		      rt6_stats.fib_rt_alloc, rt6_stats.fib_rt_entries,
		      rt6_stats.fib_rt_cache,
		      atomic_read(&ip6_dst_ops.entries));

	len -= offset;

	if (len > length)
		len = length;
	if(len < 0)
		len = 0;

	*start = buffer + offset;

	return len;
}
#endif	/* CONFIG_PROC_FS */

#ifdef CONFIG_SYSCTL

static int flush_delay;

static
int ipv6_sysctl_rtcache_flush(ctl_table *ctl, int write, struct file * filp,
			      void *buffer, size_t *lenp)
{
	if (write) {
		proc_dointvec(ctl, write, filp, buffer, lenp);
		if (flush_delay < 0)
			flush_delay = 0;
		fib6_run_gc((unsigned long)flush_delay);
		return 0;
	} else
		return -EINVAL;
}

ctl_table ipv6_route_table[] = {
        {NET_IPV6_ROUTE_FLUSH, "flush",
         &flush_delay, sizeof(int), 0644, NULL,
         &ipv6_sysctl_rtcache_flush},
	{NET_IPV6_ROUTE_GC_THRESH, "gc_thresh",
         &ip6_dst_ops.gc_thresh, sizeof(int), 0644, NULL,
         &proc_dointvec},
	{NET_IPV6_ROUTE_MAX_SIZE, "max_size",
         &ip6_rt_max_size, sizeof(int), 0644, NULL,
         &proc_dointvec},
	{NET_IPV6_ROUTE_GC_MIN_INTERVAL, "gc_min_interval",
         &ip6_rt_gc_min_interval, sizeof(int), 0644, NULL,
         &proc_dointvec_jiffies, &sysctl_jiffies},
	{NET_IPV6_ROUTE_GC_TIMEOUT, "gc_timeout",
         &ip6_rt_gc_timeout, sizeof(int), 0644, NULL,
         &proc_dointvec_jiffies, &sysctl_jiffies},
	{NET_IPV6_ROUTE_GC_INTERVAL, "gc_interval",
         &ip6_rt_gc_interval, sizeof(int), 0644, NULL,
         &proc_dointvec_jiffies, &sysctl_jiffies},
	{NET_IPV6_ROUTE_GC_ELASTICITY, "gc_elasticity",
         &ip6_rt_gc_elasticity, sizeof(int), 0644, NULL,
         &proc_dointvec_jiffies, &sysctl_jiffies},
	{NET_IPV6_ROUTE_MTU_EXPIRES, "mtu_expires",
         &ip6_rt_mtu_expires, sizeof(int), 0644, NULL,
         &proc_dointvec_jiffies, &sysctl_jiffies},
	{NET_IPV6_ROUTE_MIN_ADVMSS, "min_adv_mss",
         &ip6_rt_min_advmss, sizeof(int), 0644, NULL,
         &proc_dointvec_jiffies, &sysctl_jiffies},
	 {0}
};

#endif


void __init ip6_route_init(void)
{
	ip6_dst_ops.kmem_cachep = kmem_cache_create("ip6_dst_cache",
						     sizeof(struct rt6_info),
						     0, SLAB_HWCACHE_ALIGN,
						     NULL, NULL);
	fib6_init();
#ifdef 	CONFIG_PROC_FS
	proc_net_create("ipv6_route", 0, rt6_proc_info);
	proc_net_create("rt6_stats", 0, rt6_proc_stats);
#endif
}

#ifdef MODULE
void ip6_route_cleanup(void)
{
#ifdef CONFIG_PROC_FS
	proc_net_remove("ipv6_route");
	proc_net_remove("rt6_stats");
#endif

	rt6_ifdown(NULL);
	fib6_gc_cleanup();
}
#endif	/* MODULE */
