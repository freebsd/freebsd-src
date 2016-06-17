/*
 *	Neighbour Discovery for IPv6
 *	Linux INET6 implementation 
 *
 *	Authors:
 *	Pedro Roque		<roque@di.fc.ul.pt>	
 *	Mike Shaver		<shaver@ingenia.com>
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

/*
 *	Changes:
 *
 *	Lars Fenneberg			:	fixed MTU setting on receipt
 *						of an RA.
 *
 *	Janos Farkas			:	kmalloc failure checks
 *	Alexey Kuznetsov		:	state machine reworked
 *						and moved to net/core.
 *	Pekka Savola			:	RFC2461 validation
 *	YOSHIFUJI Hideaki @USAGI	:	Verify ND options properly
 */

/* Set to 3 to get tracing... */
#define ND_DEBUG 1

#define ND_PRINTK(x...) printk(KERN_DEBUG x)
#define ND_NOPRINTK(x...) do { ; } while(0)
#define ND_PRINTK0 ND_PRINTK
#define ND_PRINTK1 ND_NOPRINTK
#define ND_PRINTK2 ND_NOPRINTK
#if ND_DEBUG >= 1
#undef ND_PRINTK1
#define ND_PRINTK1 ND_PRINTK
#endif
#if ND_DEBUG >= 2
#undef ND_PRINTK2
#define ND_PRINTK2 ND_PRINTK
#endif

#include <linux/module.h>
#include <linux/config.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/sched.h>
#include <linux/net.h>
#include <linux/in6.h>
#include <linux/route.h>
#include <linux/init.h>
#ifdef CONFIG_SYSCTL
#include <linux/sysctl.h>
#endif

#include <linux/if_arp.h>
#include <linux/ipv6.h>
#include <linux/icmpv6.h>

#include <net/sock.h>
#include <net/snmp.h>

#include <net/ipv6.h>
#include <net/protocol.h>
#include <net/ndisc.h>
#include <net/ip6_route.h>
#include <net/addrconf.h>
#include <net/icmp.h>

#include <net/checksum.h>
#include <linux/proc_fs.h>

static struct socket *ndisc_socket;

static u32 ndisc_hash(const void *pkey, const struct net_device *dev);
static int ndisc_constructor(struct neighbour *neigh);
static void ndisc_solicit(struct neighbour *neigh, struct sk_buff *skb);
static void ndisc_error_report(struct neighbour *neigh, struct sk_buff *skb);
static int pndisc_constructor(struct pneigh_entry *n);
static void pndisc_destructor(struct pneigh_entry *n);
static void pndisc_redo(struct sk_buff *skb);

static struct neigh_ops ndisc_generic_ops =
{
	AF_INET6,
	NULL,
	ndisc_solicit,
	ndisc_error_report,
	neigh_resolve_output,
	neigh_connected_output,
	dev_queue_xmit,
	dev_queue_xmit
};

static struct neigh_ops ndisc_hh_ops =
{
	AF_INET6,
	NULL,
	ndisc_solicit,
	ndisc_error_report,
	neigh_resolve_output,
	neigh_resolve_output,
	dev_queue_xmit,
	dev_queue_xmit
};


static struct neigh_ops ndisc_direct_ops =
{
	AF_INET6,
	NULL,
	NULL,
	NULL,
	dev_queue_xmit,
	dev_queue_xmit,
	dev_queue_xmit,
	dev_queue_xmit
};

struct neigh_table nd_tbl =
{
	NULL,
	AF_INET6,
	sizeof(struct neighbour) + sizeof(struct in6_addr),
	sizeof(struct in6_addr),
	ndisc_hash,
	ndisc_constructor,
	pndisc_constructor,
	pndisc_destructor,
	pndisc_redo,
	"ndisc_cache",
        { NULL, NULL, &nd_tbl, 0, NULL, NULL,
		  30*HZ, 1*HZ, 60*HZ, 30*HZ, 5*HZ, 3, 3, 0, 3, 1*HZ, (8*HZ)/10, 64, 0 },
	30*HZ, 128, 512, 1024,
};

#define NDISC_OPT_SPACE(len) (((len)+2+7)&~7)

static u8 *ndisc_fill_option(u8 *opt, int type, void *data, int data_len)
{
	int space = NDISC_OPT_SPACE(data_len);

	opt[0] = type;
	opt[1] = space>>3;
	memcpy(opt+2, data, data_len);
	data_len += 2;
	opt += data_len;
	if ((space -= data_len) > 0)
		memset(opt, 0, space);
	return opt + space;
}

struct nd_opt_hdr *ndisc_next_option(struct nd_opt_hdr *cur,
				     struct nd_opt_hdr *end)
{
	int type;
	if (!cur || !end || cur >= end)
		return NULL;
	type = cur->nd_opt_type;
	do {
		cur = ((void *)cur) + (cur->nd_opt_len << 3);
	} while(cur < end && cur->nd_opt_type != type);
	return (cur <= end && cur->nd_opt_type == type ? cur : NULL);
}

struct ndisc_options *ndisc_parse_options(u8 *opt, int opt_len,
					  struct ndisc_options *ndopts)
{
	struct nd_opt_hdr *nd_opt = (struct nd_opt_hdr *)opt;

	if (!nd_opt || opt_len < 0 || !ndopts)
		return NULL;
	memset(ndopts, 0, sizeof(*ndopts));
	while (opt_len) {
		int l;
		if (opt_len < sizeof(struct nd_opt_hdr))
			return NULL;
		l = nd_opt->nd_opt_len << 3;
		if (opt_len < l || l == 0)
			return NULL;
		switch (nd_opt->nd_opt_type) {
		case ND_OPT_SOURCE_LL_ADDR:
		case ND_OPT_TARGET_LL_ADDR:
		case ND_OPT_MTU:
		case ND_OPT_REDIRECT_HDR:
			if (ndopts->nd_opt_array[nd_opt->nd_opt_type]) {
				ND_PRINTK2((KERN_WARNING
					    "ndisc_parse_options(): duplicated ND6 option found: type=%d\n",
					    nd_opt->nd_opt_type));
			} else {
				ndopts->nd_opt_array[nd_opt->nd_opt_type] = nd_opt;
			}
			break;
		case ND_OPT_PREFIX_INFO:
			ndopts->nd_opts_pi_end = nd_opt;
			if (ndopts->nd_opt_array[nd_opt->nd_opt_type] == 0)
				ndopts->nd_opt_array[nd_opt->nd_opt_type] = nd_opt;
			break;
		default:
			/*
			 * Unknown options must be silently ignored,
			 * to accomodate future extension to the protocol.
			 */
			ND_PRINTK2(KERN_WARNING
				   "ndisc_parse_options(): ignored unsupported option; type=%d, len=%d\n",
				   nd_opt->nd_opt_type, nd_opt->nd_opt_len);
		}
		opt_len -= l;
		nd_opt = ((void *)nd_opt) + l;
	}
	return ndopts;
}

int ndisc_mc_map(struct in6_addr *addr, char *buf, struct net_device *dev, int dir)
{
	switch (dev->type) {
	case ARPHRD_ETHER:
	case ARPHRD_IEEE802:	/* Not sure. Check it later. --ANK */
	case ARPHRD_FDDI:
		ipv6_eth_mc_map(addr, buf);
		return 0;
	case ARPHRD_IEEE802_TR:
		ipv6_tr_mc_map(addr,buf);
		return 0;
	case ARPHRD_ARCNET:
		ipv6_arcnet_mc_map(addr, buf);
		return 0;
	default:
		if (dir) {
			memcpy(buf, dev->broadcast, dev->addr_len);
			return 0;
		}
	}
	return -EINVAL;
}

static u32 ndisc_hash(const void *pkey, const struct net_device *dev)
{
	u32 hash_val;

	hash_val = *(u32*)(pkey + sizeof(struct in6_addr) - 4);
	hash_val ^= (hash_val>>16);
	hash_val ^= hash_val>>8;
	hash_val ^= hash_val>>3;
	hash_val = (hash_val^dev->ifindex)&NEIGH_HASHMASK;

	return hash_val;
}

static int ndisc_constructor(struct neighbour *neigh)
{
	struct in6_addr *addr = (struct in6_addr*)&neigh->primary_key;
	struct net_device *dev = neigh->dev;
	struct inet6_dev *in6_dev = in6_dev_get(dev);
	int addr_type;

	if (in6_dev == NULL)
		return -EINVAL;

	addr_type = ipv6_addr_type(addr);
	if (in6_dev->nd_parms)
		neigh->parms = in6_dev->nd_parms;

	if (addr_type&IPV6_ADDR_MULTICAST)
		neigh->type = RTN_MULTICAST;
	else
		neigh->type = RTN_UNICAST;
	if (dev->hard_header == NULL) {
		neigh->nud_state = NUD_NOARP;
		neigh->ops = &ndisc_direct_ops;
		neigh->output = neigh->ops->queue_xmit;
	} else {
		if (addr_type&IPV6_ADDR_MULTICAST) {
			neigh->nud_state = NUD_NOARP;
			ndisc_mc_map(addr, neigh->ha, dev, 1);
		} else if (dev->flags&(IFF_NOARP|IFF_LOOPBACK)) {
			neigh->nud_state = NUD_NOARP;
			memcpy(neigh->ha, dev->dev_addr, dev->addr_len);
			if (dev->flags&IFF_LOOPBACK)
				neigh->type = RTN_LOCAL;
		} else if (dev->flags&IFF_POINTOPOINT) {
			neigh->nud_state = NUD_NOARP;
			memcpy(neigh->ha, dev->broadcast, dev->addr_len);
		}
		if (dev->hard_header_cache)
			neigh->ops = &ndisc_hh_ops;
		else
			neigh->ops = &ndisc_generic_ops;
		if (neigh->nud_state&NUD_VALID)
			neigh->output = neigh->ops->connected_output;
		else
			neigh->output = neigh->ops->output;
	}
	in6_dev_put(in6_dev);
	return 0;
}

static int pndisc_constructor(struct pneigh_entry *n)
{
	struct in6_addr *addr = (struct in6_addr*)&n->key;
	struct in6_addr maddr;
	struct net_device *dev = n->dev;

	if (dev == NULL || __in6_dev_get(dev) == NULL)
		return -EINVAL;
	addrconf_addr_solict_mult(addr, &maddr);
	ipv6_dev_mc_inc(dev, &maddr);
	return 0;
}

static void pndisc_destructor(struct pneigh_entry *n)
{
	struct in6_addr *addr = (struct in6_addr*)&n->key;
	struct in6_addr maddr;
	struct net_device *dev = n->dev;

	if (dev == NULL || __in6_dev_get(dev) == NULL)
		return;
	addrconf_addr_solict_mult(addr, &maddr);
	ipv6_dev_mc_dec(dev, &maddr);
}



static int
ndisc_build_ll_hdr(struct sk_buff *skb, struct net_device *dev,
		   struct in6_addr *daddr, struct neighbour *neigh, int len)
{
	unsigned char ha[MAX_ADDR_LEN];
	unsigned char *h_dest = NULL;

	skb_reserve(skb, (dev->hard_header_len + 15) & ~15);

	if (dev->hard_header) {
		if (ipv6_addr_type(daddr) & IPV6_ADDR_MULTICAST) {
			ndisc_mc_map(daddr, ha, dev, 1);
			h_dest = ha;
		} else if (neigh) {
			read_lock_bh(&neigh->lock);
			if (neigh->nud_state&NUD_VALID) {
				memcpy(ha, neigh->ha, dev->addr_len);
				h_dest = ha;
			}
			read_unlock_bh(&neigh->lock);
		} else {
			neigh = neigh_lookup(&nd_tbl, daddr, dev);
			if (neigh) {
				read_lock_bh(&neigh->lock);
				if (neigh->nud_state&NUD_VALID) {
					memcpy(ha, neigh->ha, dev->addr_len);
					h_dest = ha;
				}
				read_unlock_bh(&neigh->lock);
				neigh_release(neigh);
			}
		}

		if (dev->hard_header(skb, dev, ETH_P_IPV6, h_dest, NULL, len) < 0)
			return 0;
	}

	return 1;
}


/*
 *	Send a Neighbour Advertisement
 */

void ndisc_send_na(struct net_device *dev, struct neighbour *neigh,
		   struct in6_addr *daddr, struct in6_addr *solicited_addr,
		   int router, int solicited, int override, int inc_opt) 
{
	static struct in6_addr tmpaddr;
	struct inet6_ifaddr *ifp;
        struct sock *sk = ndisc_socket->sk;
	struct in6_addr *src_addr;
        struct nd_msg *msg;
        int len;
        struct sk_buff *skb;
	int err;

	len = sizeof(struct icmp6hdr) + sizeof(struct in6_addr);

	if (inc_opt) {
		if (dev->addr_len)
			len += NDISC_OPT_SPACE(dev->addr_len);
		else
			inc_opt = 0;
	}

	skb = sock_alloc_send_skb(sk, MAX_HEADER + len + dev->hard_header_len + 15,
				  1, &err);

	if (skb == NULL) {
		ND_PRINTK1("send_na: alloc skb failed\n");
		return;
	}
	/* for anycast or proxy, solicited_addr != src_addr */
	ifp = ipv6_get_ifaddr(solicited_addr, dev);
	if (ifp) {
		src_addr = solicited_addr;
		in6_ifa_put(ifp);
	} else {
		if (ipv6_dev_get_saddr(dev, daddr, &tmpaddr, 0))
			return;
		src_addr = &tmpaddr;
	}

	if (ndisc_build_ll_hdr(skb, dev, daddr, neigh, len) == 0) {
		kfree_skb(skb);
		return;
	}

	ip6_nd_hdr(sk, skb, dev, src_addr, daddr, IPPROTO_ICMPV6, len);

	msg = (struct nd_msg *) skb_put(skb, len);

        msg->icmph.icmp6_type = NDISC_NEIGHBOUR_ADVERTISEMENT;
        msg->icmph.icmp6_code = 0;
        msg->icmph.icmp6_cksum = 0;

        msg->icmph.icmp6_unused = 0;
        msg->icmph.icmp6_router    = router;
        msg->icmph.icmp6_solicited = solicited;
        msg->icmph.icmp6_override  = !!override;

        /* Set the target address. */
	ipv6_addr_copy(&msg->target, solicited_addr);

	if (inc_opt)
		ndisc_fill_option(msg->opt, ND_OPT_TARGET_LL_ADDR, dev->dev_addr, dev->addr_len);

	/* checksum */
	msg->icmph.icmp6_cksum = csum_ipv6_magic(src_addr, daddr, len, 
						 IPPROTO_ICMPV6,
						 csum_partial((__u8 *) msg, 
							      len, 0));

	dev_queue_xmit(skb);

	ICMP6_INC_STATS(Icmp6OutNeighborAdvertisements);
	ICMP6_INC_STATS(Icmp6OutMsgs);
}        

void ndisc_send_ns(struct net_device *dev, struct neighbour *neigh,
		   struct in6_addr *solicit,
		   struct in6_addr *daddr, struct in6_addr *saddr) 
{
        struct sock *sk = ndisc_socket->sk;
        struct sk_buff *skb;
        struct nd_msg *msg;
	struct in6_addr addr_buf;
        int len;
	int err;
	int send_llinfo;

	if (saddr == NULL) {
		if (ipv6_get_lladdr(dev, &addr_buf))
			return;
		saddr = &addr_buf;
	}

	len = sizeof(struct icmp6hdr) + sizeof(struct in6_addr);
	send_llinfo = dev->addr_len && ipv6_addr_type(saddr) != IPV6_ADDR_ANY;
	if (send_llinfo)
		len += NDISC_OPT_SPACE(dev->addr_len);

	skb = sock_alloc_send_skb(sk, MAX_HEADER + len + dev->hard_header_len + 15,
				  1, &err);
	if (skb == NULL) {
		ND_PRINTK1("send_ns: alloc skb failed\n");
		return;
	}

	if (ndisc_build_ll_hdr(skb, dev, daddr, neigh, len) == 0) {
		kfree_skb(skb);
		return;
	}

	ip6_nd_hdr(sk, skb, dev, saddr, daddr, IPPROTO_ICMPV6, len);

	msg = (struct nd_msg *)skb_put(skb, len);
	msg->icmph.icmp6_type = NDISC_NEIGHBOUR_SOLICITATION;
	msg->icmph.icmp6_code = 0;
	msg->icmph.icmp6_cksum = 0;
	msg->icmph.icmp6_unused = 0;

	/* Set the target address. */
	ipv6_addr_copy(&msg->target, solicit);

	if (send_llinfo)
		ndisc_fill_option(msg->opt, ND_OPT_SOURCE_LL_ADDR, dev->dev_addr, dev->addr_len);

	/* checksum */
	msg->icmph.icmp6_cksum = csum_ipv6_magic(&skb->nh.ipv6h->saddr,
						 daddr, len, 
						 IPPROTO_ICMPV6,
						 csum_partial((__u8 *) msg, 
							      len, 0));
	/* send it! */
	dev_queue_xmit(skb);

	ICMP6_INC_STATS(Icmp6OutNeighborSolicits);
	ICMP6_INC_STATS(Icmp6OutMsgs);
}

void ndisc_send_rs(struct net_device *dev, struct in6_addr *saddr,
		   struct in6_addr *daddr)
{
	struct sock *sk = ndisc_socket->sk;
        struct sk_buff *skb;
        struct icmp6hdr *hdr;
	__u8 * opt;
        int len;
	int err;

	len = sizeof(struct icmp6hdr);
	if (dev->addr_len)
		len += NDISC_OPT_SPACE(dev->addr_len);

        skb = sock_alloc_send_skb(sk, MAX_HEADER + len + dev->hard_header_len + 15,
				  1, &err);
	if (skb == NULL) {
		ND_PRINTK1("send_ns: alloc skb failed\n");
		return;
	}

	if (ndisc_build_ll_hdr(skb, dev, daddr, NULL, len) == 0) {
		kfree_skb(skb);
		return;
	}

	ip6_nd_hdr(sk, skb, dev, saddr, daddr, IPPROTO_ICMPV6, len);

        hdr = (struct icmp6hdr *) skb_put(skb, len);
        hdr->icmp6_type = NDISC_ROUTER_SOLICITATION;
        hdr->icmp6_code = 0;
        hdr->icmp6_cksum = 0;
        hdr->icmp6_unused = 0;

	opt = (u8*) (hdr + 1);

	if (dev->addr_len)
		ndisc_fill_option(opt, ND_OPT_SOURCE_LL_ADDR, dev->dev_addr, dev->addr_len);

	/* checksum */
	hdr->icmp6_cksum = csum_ipv6_magic(&skb->nh.ipv6h->saddr, daddr, len,
					   IPPROTO_ICMPV6,
					   csum_partial((__u8 *) hdr, len, 0));

	/* send it! */
	dev_queue_xmit(skb);

	ICMP6_INC_STATS(Icmp6OutRouterSolicits);
	ICMP6_INC_STATS(Icmp6OutMsgs);
}
		   

static void ndisc_error_report(struct neighbour *neigh, struct sk_buff *skb)
{
	/*
	 *	"The sender MUST return an ICMP
	 *	 destination unreachable"
	 */
	dst_link_failure(skb);
	kfree_skb(skb);
}

/* Called with locked neigh: either read or both */

static void ndisc_solicit(struct neighbour *neigh, struct sk_buff *skb)
{
	struct in6_addr *saddr = NULL;
	struct in6_addr mcaddr;
	struct net_device *dev = neigh->dev;
	struct in6_addr *target = (struct in6_addr *)&neigh->primary_key;
	int probes = atomic_read(&neigh->probes);

	if (skb && ipv6_chk_addr(&skb->nh.ipv6h->saddr, dev))
		saddr = &skb->nh.ipv6h->saddr;

	if ((probes -= neigh->parms->ucast_probes) < 0) {
		if (!(neigh->nud_state&NUD_VALID))
			ND_PRINTK1("trying to ucast probe in NUD_INVALID\n");
		ndisc_send_ns(dev, neigh, target, target, saddr);
	} else if ((probes -= neigh->parms->app_probes) < 0) {
#ifdef CONFIG_ARPD
		neigh_app_ns(neigh);
#endif
	} else {
		addrconf_addr_solict_mult(target, &mcaddr);
		ndisc_send_ns(dev, NULL, target, &mcaddr, saddr);
	}
}

void ndisc_recv_ns(struct sk_buff *skb)
{
	struct nd_msg *msg = (struct nd_msg *)skb->h.raw;
	struct in6_addr *saddr = &skb->nh.ipv6h->saddr;
	struct in6_addr *daddr = &skb->nh.ipv6h->daddr;
	u8 *lladdr = NULL;
	int lladdrlen = 0;
	u32 ndoptlen = skb->tail - msg->opt;
	struct ndisc_options ndopts;
	struct net_device *dev = skb->dev;
	struct inet6_ifaddr *ifp;
	struct neighbour *neigh;

	if (skb->len < sizeof(struct nd_msg)) {
		if (net_ratelimit())
			printk(KERN_WARNING "ICMP NS: packet too short\n");
		return;
	}

	if (ipv6_addr_type(&msg->target)&IPV6_ADDR_MULTICAST) {
		if (net_ratelimit())
			printk(KERN_WARNING "ICMP NS: target address is multicast\n");
		return;
	}

	if (!ndisc_parse_options(msg->opt, ndoptlen, &ndopts)) {
		if (net_ratelimit())
			printk(KERN_WARNING "ICMP NS: invalid ND option, ignored.\n");
		return;
	}

	if (ndopts.nd_opts_src_lladdr) {
		lladdr = (u8*)(ndopts.nd_opts_src_lladdr + 1);
		lladdrlen = ndopts.nd_opts_src_lladdr->nd_opt_len << 3;
		if (lladdrlen != NDISC_OPT_SPACE(dev->addr_len)) {
			if (net_ratelimit())
				printk(KERN_WARNING "ICMP NS: bad lladdr length.\n");
			return;
		}
	}

	/* XXX: RFC2461 7.1.1:
	 * 	If the IP source address is the unspecified address, there
	 *	MUST NOT be source link-layer address option in the message.
	 *
	 *	NOTE! Linux kernel < 2.4.4 broke this rule.
	 */
		 	
	/* XXX: RFC2461 7.1.1:
	 *	If the IP source address is the unspecified address, the IP
      	 *	destination address MUST be a solicited-node multicast address.
	 */

	if ((ifp = ipv6_get_ifaddr(&msg->target, dev)) != NULL) {
		int addr_type = ipv6_addr_type(saddr);

		if (ifp->flags & IFA_F_TENTATIVE) {
			/* Address is tentative. If the source
			   is unspecified address, it is someone
			   does DAD, otherwise we ignore solicitations
			   until DAD timer expires.
			 */
			if (addr_type == IPV6_ADDR_ANY) {
				if (dev->type == ARPHRD_IEEE802_TR) { 
					unsigned char *sadr = skb->mac.raw ;
					if (((sadr[8] &0x7f) != (dev->dev_addr[0] & 0x7f)) ||
					(sadr[9] != dev->dev_addr[1]) ||
					(sadr[10] != dev->dev_addr[2]) ||
					(sadr[11] != dev->dev_addr[3]) ||
					(sadr[12] != dev->dev_addr[4]) ||
					(sadr[13] != dev->dev_addr[5])) 
					{
						addrconf_dad_failure(ifp) ; 
					}
				} else {
					addrconf_dad_failure(ifp);
				}
			} else
				in6_ifa_put(ifp);
			return;
		}
	
		if (addr_type == IPV6_ADDR_ANY) {
			struct in6_addr maddr;

			ipv6_addr_all_nodes(&maddr);
			ndisc_send_na(dev, NULL, &maddr, &ifp->addr, 
				      ifp->idev->cnf.forwarding, 0, 
				      ipv6_addr_type(&ifp->addr)&IPV6_ADDR_ANYCAST ? 0 : 1, 
				      1);
			in6_ifa_put(ifp);
			return;
		}

		if (addr_type & IPV6_ADDR_UNICAST) {
			int inc = ipv6_addr_type(daddr)&IPV6_ADDR_MULTICAST;

			if (inc)
				nd_tbl.stats.rcv_probes_mcast++;
			else
				nd_tbl.stats.rcv_probes_ucast++;

			/* 
			 *	update / create cache entry
			 *	for the source adddress
			 */

			neigh = neigh_event_ns(&nd_tbl, lladdr, saddr, dev);

			if (neigh || !dev->hard_header) {
				ndisc_send_na(dev, neigh, saddr, &ifp->addr, 
					      ifp->idev->cnf.forwarding, 1, 
					      ipv6_addr_type(&ifp->addr)&IPV6_ADDR_ANYCAST ? 0 : 1, 
					      1);
				if (neigh)
					neigh_release(neigh);
			}
		}
		in6_ifa_put(ifp);
	} else if (ipv6_chk_acast_addr(dev, &msg->target)) {
		struct inet6_dev *idev = in6_dev_get(dev);
		int addr_type = ipv6_addr_type(saddr);

		/* anycast */

		if (!idev) {
			/* XXX: count this drop? */
			return;
		}

		if (addr_type == IPV6_ADDR_ANY) {
			struct in6_addr maddr;

			ipv6_addr_all_nodes(&maddr);
			ndisc_send_na(dev, NULL, &maddr, &msg->target,
				      idev->cnf.forwarding, 0, 0, 1);
			in6_dev_put(idev);
			return;
		}

		if (addr_type & IPV6_ADDR_UNICAST) {
			int inc = ipv6_addr_type(daddr)&IPV6_ADDR_MULTICAST;
			if (inc)  
				nd_tbl.stats.rcv_probes_mcast++;
 			else
				nd_tbl.stats.rcv_probes_ucast++;

			/*
			 *   update / create cache entry
			 *   for the source adddress
			 */

			neigh = neigh_event_ns(&nd_tbl, lladdr, saddr, skb->dev);

			if (neigh || !dev->hard_header) {
				ndisc_send_na(dev, neigh, saddr,
					&msg->target, 
				        idev->cnf.forwarding, 1, 0, inc);
				if (neigh)
					neigh_release(neigh);
			}
		}
		in6_dev_put(idev);
	} else {
		struct inet6_dev *in6_dev = in6_dev_get(dev);
		int addr_type = ipv6_addr_type(saddr);

		if (in6_dev && in6_dev->cnf.forwarding &&
		    (addr_type & IPV6_ADDR_UNICAST) &&
		    pneigh_lookup(&nd_tbl, &msg->target, dev, 0)) {
			int inc = ipv6_addr_type(daddr)&IPV6_ADDR_MULTICAST;

			if (skb->stamp.tv_sec == 0 ||
			    skb->pkt_type == PACKET_HOST ||
			    inc == 0 ||
			    in6_dev->nd_parms->proxy_delay == 0) {
				if (inc)
					nd_tbl.stats.rcv_probes_mcast++;
				else
					nd_tbl.stats.rcv_probes_ucast++;
					
				neigh = neigh_event_ns(&nd_tbl, lladdr, saddr, dev);

				if (neigh) {
					ndisc_send_na(dev, neigh, saddr, &msg->target,
						      0, 1, 0, 1);
					neigh_release(neigh);
				}
			} else {
				struct sk_buff *n = skb_clone(skb, GFP_ATOMIC);
				if (n)
					pneigh_enqueue(&nd_tbl, in6_dev->nd_parms, n);
				in6_dev_put(in6_dev);
				return;
			}
		}
		if (in6_dev)
			in6_dev_put(in6_dev);
	}
	return;
}

void ndisc_recv_na(struct sk_buff *skb)
{
	struct nd_msg *msg = (struct nd_msg *)skb->h.raw;
	struct in6_addr *saddr = &skb->nh.ipv6h->saddr;
	struct in6_addr *daddr = &skb->nh.ipv6h->daddr;
	u8 *lladdr = NULL;
	int lladdrlen = 0;
	u32 ndoptlen = skb->tail - msg->opt;
	struct ndisc_options ndopts;
	struct net_device *dev = skb->dev;
	struct inet6_ifaddr *ifp;
	struct neighbour *neigh;

	if (skb->len < sizeof(struct nd_msg)) {
		if (net_ratelimit())
			printk(KERN_WARNING "ICMP NA: packet too short\n");
		return;
	}

	if (ipv6_addr_type(&msg->target)&IPV6_ADDR_MULTICAST) {
		if (net_ratelimit())
			printk(KERN_WARNING "NDISC NA: target address is multicast\n");
		return;
	}

	if ((ipv6_addr_type(daddr)&IPV6_ADDR_MULTICAST) &&
	    msg->icmph.icmp6_solicited) {
		ND_PRINTK0("NDISC: solicited NA is multicasted\n");
		return;
	}
		
	if (!ndisc_parse_options(msg->opt, ndoptlen, &ndopts)) {
		if (net_ratelimit())
			printk(KERN_WARNING "ICMP NS: invalid ND option, ignored.\n");
		return;
	}
	if (ndopts.nd_opts_tgt_lladdr) {
		lladdr = (u8*)(ndopts.nd_opts_tgt_lladdr + 1);
		lladdrlen = ndopts.nd_opts_tgt_lladdr->nd_opt_len << 3;
		if (lladdrlen != NDISC_OPT_SPACE(dev->addr_len)) {
			if (net_ratelimit())
				printk(KERN_WARNING "NDISC NA: invalid lladdr length.\n");
			return;
		}
	}
	if ((ifp = ipv6_get_ifaddr(&msg->target, dev))) {
		if (ifp->flags & IFA_F_TENTATIVE) {
			addrconf_dad_failure(ifp);
			return;
		}
		/* What should we make now? The advertisement
		   is invalid, but ndisc specs say nothing
		   about it. It could be misconfiguration, or
		   an smart proxy agent tries to help us :-)
		 */
		ND_PRINTK0("%s: someone advertises our address!\n",
			   ifp->idev->dev->name);
		in6_ifa_put(ifp);
		return;
	}
	neigh = neigh_lookup(&nd_tbl, &msg->target, dev);

	if (neigh) {
		if (neigh->flags & NTF_ROUTER) {
			if (msg->icmph.icmp6_router == 0) {
				/*
				 *	Change: router to host
				 */
				struct rt6_info *rt;
				rt = rt6_get_dflt_router(saddr, dev);
				if (rt) {
					/* It is safe only because
					   we aer in BH */
					dst_release(&rt->u.dst);
					ip6_del_rt(rt, NULL);
				}
			}
		} else {
			if (msg->icmph.icmp6_router)
				neigh->flags |= NTF_ROUTER;
		}

		neigh_update(neigh, lladdr,
			     msg->icmph.icmp6_solicited ? NUD_REACHABLE : NUD_STALE,
			     msg->icmph.icmp6_override, 1);
		neigh_release(neigh);
	}
}

static void ndisc_router_discovery(struct sk_buff *skb)
{
        struct ra_msg *ra_msg = (struct ra_msg *) skb->h.raw;
	struct neighbour *neigh;
	struct inet6_dev *in6_dev;
	struct rt6_info *rt;
	int lifetime;
	struct ndisc_options ndopts;
	int optlen;

	__u8 * opt = (__u8 *)(ra_msg + 1);

	optlen = (skb->tail - skb->h.raw) - sizeof(struct ra_msg);

	if (!(ipv6_addr_type(&skb->nh.ipv6h->saddr) & IPV6_ADDR_LINKLOCAL)) {
		if (net_ratelimit())
			printk(KERN_WARNING "ICMP RA: source address is not linklocal\n");
		return;
	}
	if (optlen < 0) {
		if (net_ratelimit())
			printk(KERN_WARNING "ICMP RA: packet too short\n");
		return;
	}

	/*
	 *	set the RA_RECV flag in the interface
	 */

	in6_dev = in6_dev_get(skb->dev);
	if (in6_dev == NULL) {
		ND_PRINTK1("RA: can't find in6 device\n");
		return;
	}
	if (in6_dev->cnf.forwarding || !in6_dev->cnf.accept_ra) {
		in6_dev_put(in6_dev);
		return;
	}

	if (!ndisc_parse_options(opt, optlen, &ndopts)) {
		in6_dev_put(in6_dev);
		if (net_ratelimit())
			ND_PRINTK2(KERN_WARNING
				   "ICMP6 RA: invalid ND option, ignored.\n");
		return;
	}

	if (in6_dev->if_flags & IF_RS_SENT) {
		/*
		 *	flag that an RA was received after an RS was sent
		 *	out on this interface.
		 */
		in6_dev->if_flags |= IF_RA_RCVD;
	}

	/*
	 * Remember the managed/otherconf flags from most recently
	 * received RA message (RFC 2462) -- yoshfuji
	 */
	in6_dev->if_flags = (in6_dev->if_flags & ~(IF_RA_MANAGED |
				IF_RA_OTHERCONF)) |
				(ra_msg->icmph.icmp6_addrconf_managed ?
					IF_RA_MANAGED : 0) |
				(ra_msg->icmph.icmp6_addrconf_other ?
					IF_RA_OTHERCONF : 0);

	lifetime = ntohs(ra_msg->icmph.icmp6_rt_lifetime);

	rt = rt6_get_dflt_router(&skb->nh.ipv6h->saddr, skb->dev);

	if (rt && lifetime == 0) {
		ip6_del_rt(rt, NULL);
		rt = NULL;
	}

	if (rt == NULL && lifetime) {
		ND_PRINTK2("ndisc_rdisc: adding default router\n");

		rt = rt6_add_dflt_router(&skb->nh.ipv6h->saddr, skb->dev);
		if (rt == NULL) {
			ND_PRINTK1("route_add failed\n");
			in6_dev_put(in6_dev);
			return;
		}

		neigh = rt->rt6i_nexthop;
		if (neigh == NULL) {
			ND_PRINTK1("nd: add default router: null neighbour\n");
			dst_release(&rt->u.dst);
			in6_dev_put(in6_dev);
			return;
		}
		neigh->flags |= NTF_ROUTER;

		/*
		 *	If we where using an "all destinations on link" route
		 *	delete it
		 */

		rt6_purge_dflt_routers(RTF_ALLONLINK);
	}

	if (rt)
		rt->rt6i_expires = jiffies + (HZ * lifetime);

	if (ra_msg->icmph.icmp6_hop_limit)
		in6_dev->cnf.hop_limit = ra_msg->icmph.icmp6_hop_limit;

	/*
	 *	Update Reachable Time and Retrans Timer
	 */

	if (in6_dev->nd_parms) {
		unsigned long rtime = ntohl(ra_msg->retrans_timer);

		if (rtime && rtime/1000 < MAX_SCHEDULE_TIMEOUT/HZ) {
			rtime = (rtime*HZ)/1000;
			if (rtime < HZ/10)
				rtime = HZ/10;
			in6_dev->nd_parms->retrans_time = rtime;
		}

		rtime = ntohl(ra_msg->reachable_time);
		if (rtime && rtime/1000 < MAX_SCHEDULE_TIMEOUT/(3*HZ)) {
			rtime = (rtime*HZ)/1000;

			if (rtime < HZ/10)
				rtime = HZ/10;

			if (rtime != in6_dev->nd_parms->base_reachable_time) {
				in6_dev->nd_parms->base_reachable_time = rtime;
				in6_dev->nd_parms->gc_staletime = 3 * rtime;
				in6_dev->nd_parms->reachable_time = neigh_rand_reach_time(rtime);
			}
		}
	}

	/*
	 *	Process options.
	 */

	if (rt && (neigh = rt->rt6i_nexthop) != NULL) {
		u8 *lladdr = NULL;
		int lladdrlen;
		if (ndopts.nd_opts_src_lladdr) {
			lladdr = (u8*)((ndopts.nd_opts_src_lladdr)+1);
			lladdrlen = ndopts.nd_opts_src_lladdr->nd_opt_len << 3;
			if (lladdrlen != NDISC_OPT_SPACE(skb->dev->addr_len)) {
				if (net_ratelimit())
					ND_PRINTK2(KERN_WARNING
						   "ICMP6 RA: Invalid lladdr length.\n");
				goto out;
			}
		}
		neigh_update(neigh, lladdr, NUD_STALE, 1, 1);
	}

	if (ndopts.nd_opts_pi) {
		struct nd_opt_hdr *p;
		for (p = ndopts.nd_opts_pi;
		     p;
		     p = ndisc_next_option(p, ndopts.nd_opts_pi_end)) {
			addrconf_prefix_rcv(skb->dev, (u8*)p, (p->nd_opt_len) << 3);
		}
	}

	if (ndopts.nd_opts_mtu) {
		u32 mtu;

		memcpy(&mtu, ((u8*)(ndopts.nd_opts_mtu+1))+2, sizeof(mtu));
		mtu = ntohl(mtu);

		if (mtu < IPV6_MIN_MTU || mtu > skb->dev->mtu) {
			if (net_ratelimit()) {
				ND_PRINTK0("NDISC: router announcement with mtu = %d\n",
					   mtu);
			}
		}

		if (in6_dev->cnf.mtu6 != mtu) {
			in6_dev->cnf.mtu6 = mtu;

			if (rt)
				rt->u.dst.pmtu = mtu;

			rt6_mtu_change(skb->dev, mtu);
		}
	}
			
	if (ndopts.nd_opts_tgt_lladdr || ndopts.nd_opts_rh) {
		if (net_ratelimit())
			ND_PRINTK0(KERN_WARNING
				   "ICMP6 RA: got illegal option with RA");
	}
out:
	if (rt)
		dst_release(&rt->u.dst);
	in6_dev_put(in6_dev);
}

static void ndisc_redirect_rcv(struct sk_buff *skb)
{
	struct inet6_dev *in6_dev;
	struct icmp6hdr *icmph;
	struct in6_addr *dest;
	struct in6_addr *target;	/* new first hop to destination */
	struct neighbour *neigh;
	int on_link = 0;
	struct ndisc_options ndopts;
	int optlen;
	u8 *lladdr = NULL;
	int lladdrlen;

	if (!(ipv6_addr_type(&skb->nh.ipv6h->saddr) & IPV6_ADDR_LINKLOCAL)) {
		if (net_ratelimit())
			printk(KERN_WARNING "ICMP redirect: source address is not linklocal\n");
		return;
	}

	optlen = skb->tail - skb->h.raw;
	optlen -= sizeof(struct icmp6hdr) + 2 * sizeof(struct in6_addr);

	if (optlen < 0) {
		if (net_ratelimit())
			printk(KERN_WARNING "ICMP redirect: packet too small\n");
		return;
	}

	icmph = (struct icmp6hdr *) skb->h.raw;
	target = (struct in6_addr *) (icmph + 1);
	dest = target + 1;

	if (ipv6_addr_type(dest) & IPV6_ADDR_MULTICAST) {
		if (net_ratelimit())
			printk(KERN_WARNING "ICMP redirect for multicast addr\n");
		return;
	}

	if (ipv6_addr_cmp(dest, target) == 0) {
		on_link = 1;
	} else if (!(ipv6_addr_type(target) & IPV6_ADDR_LINKLOCAL)) {
		if (net_ratelimit())
			printk(KERN_WARNING "ICMP redirect: target address is not linklocal\n");
		return;
	}

	in6_dev = in6_dev_get(skb->dev);
	if (!in6_dev)
		return;
	if (in6_dev->cnf.forwarding || !in6_dev->cnf.accept_redirects) {
		in6_dev_put(in6_dev);
		return;
	}

	/* XXX: RFC2461 8.1: 
	 *	The IP source address of the Redirect MUST be the same as the current
	 *	first-hop router for the specified ICMP Destination Address.
	 */
		
	if (!ndisc_parse_options((u8*)(dest + 1), optlen, &ndopts)) {
		if (net_ratelimit())
			ND_PRINTK2(KERN_WARNING
				   "ICMP6 Redirect: invalid ND options, rejected.\n");
		in6_dev_put(in6_dev);
		return;
	}
	if (ndopts.nd_opts_tgt_lladdr) {
		lladdr = (u8*)(ndopts.nd_opts_tgt_lladdr + 1);
		lladdrlen = ndopts.nd_opts_tgt_lladdr->nd_opt_len << 3;
		if (lladdrlen != NDISC_OPT_SPACE(skb->dev->addr_len)) {
			if (net_ratelimit())
				ND_PRINTK2(KERN_WARNING
					   "ICMP6 Redirect: invalid lladdr length.\n");
			in6_dev_put(in6_dev);
			return;
		}
	}
	/* passed validation tests */

	/*
	   We install redirect only if nexthop state is valid.
	 */

	neigh = __neigh_lookup(&nd_tbl, target, skb->dev, 1);
	if (neigh) {
		neigh_update(neigh, lladdr, NUD_STALE, 1, 1);
		if (neigh->nud_state&NUD_VALID)
			rt6_redirect(dest, &skb->nh.ipv6h->saddr, neigh, on_link);
		else
			__neigh_event_send(neigh, NULL);
		neigh_release(neigh);
	}
	in6_dev_put(in6_dev);
}

void ndisc_send_redirect(struct sk_buff *skb, struct neighbour *neigh,
			 struct in6_addr *target)
{
	struct sock *sk = ndisc_socket->sk;
	int len = sizeof(struct icmp6hdr) + 2 * sizeof(struct in6_addr);
	struct sk_buff *buff;
	struct icmp6hdr *icmph;
	struct in6_addr saddr_buf;
	struct in6_addr *addrp;
	struct net_device *dev;
	struct rt6_info *rt;
	u8 *opt;
	int rd_len;
	int err;
	int hlen;

	dev = skb->dev;
	rt = rt6_lookup(&skb->nh.ipv6h->saddr, NULL, dev->ifindex, 1);

	if (rt == NULL)
		return;

	if (rt->rt6i_flags & RTF_GATEWAY) {
		ND_PRINTK1("ndisc_send_redirect: not a neighbour\n");
		dst_release(&rt->u.dst);
		return;
	}
	if (!xrlim_allow(&rt->u.dst, 1*HZ)) {
		dst_release(&rt->u.dst);
		return;
	}
	dst_release(&rt->u.dst);

	if (dev->addr_len) {
		if (neigh->nud_state&NUD_VALID) {
			len  += NDISC_OPT_SPACE(dev->addr_len);
		} else {
			/* If nexthop is not valid, do not redirect!
			   We will make it later, when will be sure,
			   that it is alive.
			 */
			return;
		}
	}

	rd_len = min_t(unsigned int,
		     IPV6_MIN_MTU-sizeof(struct ipv6hdr)-len, skb->len + 8);
	rd_len &= ~0x7;
	len += rd_len;

	if (ipv6_get_lladdr(dev, &saddr_buf)) {
 		ND_PRINTK1("redirect: no link_local addr for dev\n");
 		return;
 	}

	buff = sock_alloc_send_skb(sk, MAX_HEADER + len + dev->hard_header_len + 15,
				   1, &err);
	if (buff == NULL) {
		ND_PRINTK1("ndisc_send_redirect: alloc_skb failed\n");
		return;
	}

	hlen = 0;

	if (ndisc_build_ll_hdr(buff, dev, &skb->nh.ipv6h->saddr, NULL, len) == 0) {
		kfree_skb(buff);
		return;
	}

	ip6_nd_hdr(sk, buff, dev, &saddr_buf, &skb->nh.ipv6h->saddr,
		   IPPROTO_ICMPV6, len);

	icmph = (struct icmp6hdr *) skb_put(buff, len);

	memset(icmph, 0, sizeof(struct icmp6hdr));
	icmph->icmp6_type = NDISC_REDIRECT;

	/*
	 *	copy target and destination addresses
	 */

	addrp = (struct in6_addr *)(icmph + 1);
	ipv6_addr_copy(addrp, target);
	addrp++;
	ipv6_addr_copy(addrp, &skb->nh.ipv6h->daddr);

	opt = (u8*) (addrp + 1);

	/*
	 *	include target_address option
	 */

	if (dev->addr_len)
		opt = ndisc_fill_option(opt, ND_OPT_TARGET_LL_ADDR, neigh->ha, dev->addr_len);

	/*
	 *	build redirect option and copy skb over to the new packet.
	 */

	memset(opt, 0, 8);	
	*(opt++) = ND_OPT_REDIRECT_HDR;
	*(opt++) = (rd_len >> 3);
	opt += 6;

	memcpy(opt, skb->nh.ipv6h, rd_len - 8);

	icmph->icmp6_cksum = csum_ipv6_magic(&saddr_buf, &skb->nh.ipv6h->saddr,
					     len, IPPROTO_ICMPV6,
					     csum_partial((u8 *) icmph, len, 0));

	dev_queue_xmit(buff);

	ICMP6_INC_STATS(Icmp6OutRedirects);
	ICMP6_INC_STATS(Icmp6OutMsgs);
}

static void pndisc_redo(struct sk_buff *skb)
{
	ndisc_rcv(skb);
	kfree_skb(skb);
}

int ndisc_rcv(struct sk_buff *skb)
{
	struct nd_msg *msg = (struct nd_msg *) skb->h.raw;

	__skb_push(skb, skb->data-skb->h.raw);

	if (skb->nh.ipv6h->hop_limit != 255) {
		if (net_ratelimit())
			printk(KERN_WARNING
			       "ICMP NDISC: fake message with non-255 Hop Limit received: %d\n",
			       		skb->nh.ipv6h->hop_limit);
		return 0;
	}

	if (msg->icmph.icmp6_code != 0) {
		if (net_ratelimit())
			printk(KERN_WARNING "ICMP NDISC: code is not zero\n");
		return 0;
	}

	switch (msg->icmph.icmp6_type) {
	case NDISC_NEIGHBOUR_SOLICITATION:
		ndisc_recv_ns(skb);
		break;

	case NDISC_NEIGHBOUR_ADVERTISEMENT:
		ndisc_recv_na(skb);
		break;

	case NDISC_ROUTER_ADVERTISEMENT:
		ndisc_router_discovery(skb);
		break;

	case NDISC_REDIRECT:
		ndisc_redirect_rcv(skb);
		break;
	};

	return 0;
}

static int ndisc_netdev_event(struct notifier_block *this, unsigned long event, void *ptr)
{
	struct net_device *dev = ptr;

	switch (event) {
	case NETDEV_CHANGEADDR:
		neigh_changeaddr(&nd_tbl, dev);
		fib6_run_gc(0);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

struct notifier_block ndisc_netdev_notifier = {
	.notifier_call = ndisc_netdev_event,
};

int __init ndisc_init(struct net_proto_family *ops)
{
	struct sock *sk;
        int err;

	ndisc_socket = sock_alloc();
	if (ndisc_socket == NULL) {
		printk(KERN_ERR
		       "Failed to create the NDISC control socket.\n");
		return -1;
	}
	ndisc_socket->inode->i_uid = 0;
	ndisc_socket->inode->i_gid = 0;
	ndisc_socket->type = SOCK_RAW;

	if((err = ops->create(ndisc_socket, IPPROTO_ICMPV6)) < 0) {
		printk(KERN_DEBUG 
		       "Failed to initialize the NDISC control socket (err %d).\n",
		       err);
		sock_release(ndisc_socket);
		ndisc_socket = NULL; /* For safety. */
		return err;
	}

	sk = ndisc_socket->sk;
	sk->allocation = GFP_ATOMIC;
	sk->net_pinfo.af_inet6.hop_limit = 255;
	/* Do not loopback ndisc messages */
	sk->net_pinfo.af_inet6.mc_loop = 0;
	sk->prot->unhash(sk);

        /*
         * Initialize the neighbour table
         */
	
	neigh_table_init(&nd_tbl);

#ifdef CONFIG_SYSCTL
	neigh_sysctl_register(NULL, &nd_tbl.parms, NET_IPV6, NET_IPV6_NEIGH, "ipv6");
#endif

	register_netdevice_notifier(&ndisc_netdev_notifier);
	return 0;
}

void ndisc_cleanup(void)
{
	neigh_table_clear(&nd_tbl);
	sock_release(ndisc_socket);
	ndisc_socket = NULL; /* For safety. */
}
