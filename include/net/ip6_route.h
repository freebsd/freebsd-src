#ifndef _NET_IP6_ROUTE_H
#define _NET_IP6_ROUTE_H

#define IP6_RT_PRIO_FW		16
#define IP6_RT_PRIO_USER	1024
#define IP6_RT_PRIO_ADDRCONF	256
#define IP6_RT_PRIO_KERN	512
#define IP6_RT_FLOW_MASK	0x00ff

#ifdef __KERNEL__

#include <net/flow.h>
#include <net/ip6_fib.h>

struct pol_chain {
	int			type;
	int			priority;
	struct fib6_node	*rules;
	struct pol_chain	*next;
};

extern struct rt6_info	ip6_null_entry;

extern int ip6_rt_max_size;
extern int ip6_rt_gc_min;
extern int ip6_rt_gc_timeout;
extern int ip6_rt_gc_interval;

extern void			ip6_route_input(struct sk_buff *skb);

extern struct dst_entry *	ip6_route_output(struct sock *sk,
						 struct flowi *fl);

extern int			ip6_route_me_harder(struct sk_buff *skb);

extern void			ip6_route_init(void);
extern void			ip6_route_cleanup(void);

extern int			ipv6_route_ioctl(unsigned int cmd, void *arg);

extern int			ip6_route_add(struct in6_rtmsg *rtmsg,
					      struct nlmsghdr *);
extern int			ip6_del_rt(struct rt6_info *,
					   struct nlmsghdr *);

extern int			ip6_rt_addr_add(struct in6_addr *addr,
						struct net_device *dev);

extern int			ip6_rt_addr_del(struct in6_addr *addr,
						struct net_device *dev);

extern void			rt6_sndmsg(int type, struct in6_addr *dst,
					   struct in6_addr *src,
					   struct in6_addr *gw,
					   struct net_device *dev, 
					   int dstlen, int srclen,
					   int metric, __u32 flags);

extern struct rt6_info		*rt6_lookup(struct in6_addr *daddr,
					    struct in6_addr *saddr,
					    int oif, int flags);

/*
 *	support functions for ND
 *
 */
extern struct rt6_info *	rt6_get_dflt_router(struct in6_addr *addr,
						    struct net_device *dev);
extern struct rt6_info *	rt6_add_dflt_router(struct in6_addr *gwaddr,
						    struct net_device *dev);

extern void			rt6_purge_dflt_routers(int lst_resort);

extern void			rt6_redirect(struct in6_addr *dest,
					     struct in6_addr *saddr,
					     struct neighbour *neigh,
					     int on_link);

extern void			rt6_pmtu_discovery(struct in6_addr *daddr,
						   struct in6_addr *saddr,
						   struct net_device *dev,
						   u32 pmtu);

struct nlmsghdr;
struct netlink_callback;
extern int inet6_dump_fib(struct sk_buff *skb, struct netlink_callback *cb);
extern int inet6_rtm_newroute(struct sk_buff *skb, struct nlmsghdr* nlh, void *arg);
extern int inet6_rtm_delroute(struct sk_buff *skb, struct nlmsghdr* nlh, void *arg);
extern int inet6_rtm_getroute(struct sk_buff *skb, struct nlmsghdr* nlh, void *arg);

extern void rt6_ifdown(struct net_device *dev);
extern void rt6_mtu_change(struct net_device *dev, unsigned mtu);

extern rwlock_t rt6_lock;

/*
 *	Store a destination cache entry in a socket
 *	For UDP/RAW sockets this is done on udp_connect.
 */

static inline void ip6_dst_store(struct sock *sk, struct dst_entry *dst,
				     struct in6_addr *daddr)
{
	struct ipv6_pinfo *np = &sk->net_pinfo.af_inet6;
	struct rt6_info *rt = (struct rt6_info *) dst;

	write_lock(&sk->dst_lock);
	__sk_dst_set(sk, dst);
	np->daddr_cache = daddr;
	np->dst_cookie = rt->rt6i_node ? rt->rt6i_node->fn_sernum : 0;
	write_unlock(&sk->dst_lock);
}

#endif
#endif
