/*
 *	IPv6 over IPv4 tunnel device - Simple Internet Transition (SIT)
 *	Linux INET6 implementation
 *
 *	Authors:
 *	Pedro Roque		<roque@di.fc.ul.pt>	
 *	Alexey Kuznetsov	<kuznet@ms2.inr.ac.ru>
 *
 *	$Id: sit.c,v 1.53 2001/09/25 05:09:53 davem Exp $
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *	Changes:
 * Roger Venning <r.venning@telstra.com>:	6to4 support
 * Nate Thompson <nate@thebog.net>:		6to4 support
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/sched.h>
#include <linux/net.h>
#include <linux/in6.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/icmp.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/netfilter_ipv4.h>

#include <net/sock.h>
#include <net/snmp.h>

#include <net/ipv6.h>
#include <net/protocol.h>
#include <net/transp_v6.h>
#include <net/ip6_fib.h>
#include <net/ip6_route.h>
#include <net/ndisc.h>
#include <net/addrconf.h>
#include <net/ip.h>
#include <net/udp.h>
#include <net/icmp.h>
#include <net/ipip.h>
#include <net/inet_ecn.h>

/*
   This version of net/ipv6/sit.c is cloned of net/ipv4/ip_gre.c

   For comments look at net/ipv4/ip_gre.c --ANK
 */

#define HASH_SIZE  16
#define HASH(addr) ((addr^(addr>>4))&0xF)

static int ipip6_fb_tunnel_init(struct net_device *dev);
static int ipip6_tunnel_init(struct net_device *dev);

static struct net_device ipip6_fb_tunnel_dev = {
	name: 		"sit0", 
	init:		ipip6_fb_tunnel_init,
};

static struct ip_tunnel ipip6_fb_tunnel = {
	NULL, &ipip6_fb_tunnel_dev, {0, }, 0, 0, 0, 0, 0, 0, 0, {"sit0", }
};

static struct ip_tunnel *tunnels_r_l[HASH_SIZE];
static struct ip_tunnel *tunnels_r[HASH_SIZE];
static struct ip_tunnel *tunnels_l[HASH_SIZE];
static struct ip_tunnel *tunnels_wc[1];
static struct ip_tunnel **tunnels[4] = { tunnels_wc, tunnels_l, tunnels_r, tunnels_r_l };

static rwlock_t ipip6_lock = RW_LOCK_UNLOCKED;

static struct ip_tunnel * ipip6_tunnel_lookup(u32 remote, u32 local)
{
	unsigned h0 = HASH(remote);
	unsigned h1 = HASH(local);
	struct ip_tunnel *t;

	for (t = tunnels_r_l[h0^h1]; t; t = t->next) {
		if (local == t->parms.iph.saddr &&
		    remote == t->parms.iph.daddr && (t->dev->flags&IFF_UP))
			return t;
	}
	for (t = tunnels_r[h0]; t; t = t->next) {
		if (remote == t->parms.iph.daddr && (t->dev->flags&IFF_UP))
			return t;
	}
	for (t = tunnels_l[h1]; t; t = t->next) {
		if (local == t->parms.iph.saddr && (t->dev->flags&IFF_UP))
			return t;
	}
	if ((t = tunnels_wc[0]) != NULL && (t->dev->flags&IFF_UP))
		return t;
	return NULL;
}

static struct ip_tunnel ** ipip6_bucket(struct ip_tunnel *t)
{
	u32 remote = t->parms.iph.daddr;
	u32 local = t->parms.iph.saddr;
	unsigned h = 0;
	int prio = 0;

	if (remote) {
		prio |= 2;
		h ^= HASH(remote);
	}
	if (local) {
		prio |= 1;
		h ^= HASH(local);
	}
	return &tunnels[prio][h];
}

static void ipip6_tunnel_unlink(struct ip_tunnel *t)
{
	struct ip_tunnel **tp;

	for (tp = ipip6_bucket(t); *tp; tp = &(*tp)->next) {
		if (t == *tp) {
			write_lock_bh(&ipip6_lock);
			*tp = t->next;
			write_unlock_bh(&ipip6_lock);
			break;
		}
	}
}

static void ipip6_tunnel_link(struct ip_tunnel *t)
{
	struct ip_tunnel **tp = ipip6_bucket(t);

	write_lock_bh(&ipip6_lock);
	t->next = *tp;
	write_unlock_bh(&ipip6_lock);
	*tp = t;
}

struct ip_tunnel * ipip6_tunnel_locate(struct ip_tunnel_parm *parms, int create)
{
	u32 remote = parms->iph.daddr;
	u32 local = parms->iph.saddr;
	struct ip_tunnel *t, **tp, *nt;
	struct net_device *dev;
	unsigned h = 0;
	int prio = 0;

	if (remote) {
		prio |= 2;
		h ^= HASH(remote);
	}
	if (local) {
		prio |= 1;
		h ^= HASH(local);
	}
	for (tp = &tunnels[prio][h]; (t = *tp) != NULL; tp = &t->next) {
		if (local == t->parms.iph.saddr && remote == t->parms.iph.daddr)
			return t;
	}
	if (!create)
		return NULL;

	MOD_INC_USE_COUNT;
	dev = kmalloc(sizeof(*dev) + sizeof(*t), GFP_KERNEL);
	if (dev == NULL) {
		MOD_DEC_USE_COUNT;
		return NULL;
	}
	memset(dev, 0, sizeof(*dev) + sizeof(*t));
	dev->priv = (void*)(dev+1);
	nt = (struct ip_tunnel*)dev->priv;
	nt->dev = dev;
	dev->init = ipip6_tunnel_init;
	dev->features |= NETIF_F_DYNALLOC;
	memcpy(&nt->parms, parms, sizeof(*parms));
	nt->parms.name[IFNAMSIZ-1] = '\0';
	strcpy(dev->name, nt->parms.name);
	if (dev->name[0] == 0) {
		int i;
		for (i=1; i<100; i++) {
			sprintf(dev->name, "sit%d", i);
			if (__dev_get_by_name(dev->name) == NULL)
				break;
		}
		if (i==100)
			goto failed;
		memcpy(nt->parms.name, dev->name, IFNAMSIZ);
	}
	if (register_netdevice(dev) < 0)
		goto failed;

	dev_hold(dev);
	ipip6_tunnel_link(nt);
	/* Do not decrement MOD_USE_COUNT here. */
	return nt;

failed:
	kfree(dev);
	MOD_DEC_USE_COUNT;
	return NULL;
}

static void ipip6_tunnel_destructor(struct net_device *dev)
{
	if (dev != &ipip6_fb_tunnel_dev) {
		MOD_DEC_USE_COUNT;
	}
}

static void ipip6_tunnel_uninit(struct net_device *dev)
{
	if (dev == &ipip6_fb_tunnel_dev) {
		write_lock_bh(&ipip6_lock);
		tunnels_wc[0] = NULL;
		write_unlock_bh(&ipip6_lock);
		dev_put(dev);
	} else {
		ipip6_tunnel_unlink((struct ip_tunnel*)dev->priv);
		dev_put(dev);
	}
}


void ipip6_err(struct sk_buff *skb, u32 info)
{
#ifndef I_WISH_WORLD_WERE_PERFECT

/* It is not :-( All the routers (except for Linux) return only
   8 bytes of packet payload. It means, that precise relaying of
   ICMP in the real Internet is absolutely infeasible.
 */
	struct iphdr *iph = (struct iphdr*)skb->data;
	int type = skb->h.icmph->type;
	int code = skb->h.icmph->code;
	struct ip_tunnel *t;

	switch (type) {
	default:
	case ICMP_PARAMETERPROB:
		return;

	case ICMP_DEST_UNREACH:
		switch (code) {
		case ICMP_SR_FAILED:
		case ICMP_PORT_UNREACH:
			/* Impossible event. */
			return;
		case ICMP_FRAG_NEEDED:
			/* Soft state for pmtu is maintained by IP core. */
			return;
		default:
			/* All others are translated to HOST_UNREACH.
			   rfc2003 contains "deep thoughts" about NET_UNREACH,
			   I believe they are just ether pollution. --ANK
			 */
			break;
		}
		break;
	case ICMP_TIME_EXCEEDED:
		if (code != ICMP_EXC_TTL)
			return;
		break;
	}

	read_lock(&ipip6_lock);
	t = ipip6_tunnel_lookup(iph->daddr, iph->saddr);
	if (t == NULL || t->parms.iph.daddr == 0)
		goto out;
	if (t->parms.iph.ttl == 0 && type == ICMP_TIME_EXCEEDED)
		goto out;

	if (jiffies - t->err_time < IPTUNNEL_ERR_TIMEO)
		t->err_count++;
	else
		t->err_count = 1;
	t->err_time = jiffies;
out:
	read_unlock(&ipip6_lock);
	return;
#else
	struct iphdr *iph = (struct iphdr*)dp;
	int hlen = iph->ihl<<2;
	struct ipv6hdr *iph6;
	int type = skb->h.icmph->type;
	int code = skb->h.icmph->code;
	int rel_type = 0;
	int rel_code = 0;
	int rel_info = 0;
	struct sk_buff *skb2;
	struct rt6_info *rt6i;

	if (len < hlen + sizeof(struct ipv6hdr))
		return;
	iph6 = (struct ipv6hdr*)(dp + hlen);

	switch (type) {
	default:
		return;
	case ICMP_PARAMETERPROB:
		if (skb->h.icmph->un.gateway < hlen)
			return;

		/* So... This guy found something strange INSIDE encapsulated
		   packet. Well, he is fool, but what can we do ?
		 */
		rel_type = ICMPV6_PARAMPROB;
		rel_info = skb->h.icmph->un.gateway - hlen;
		break;

	case ICMP_DEST_UNREACH:
		switch (code) {
		case ICMP_SR_FAILED:
		case ICMP_PORT_UNREACH:
			/* Impossible event. */
			return;
		case ICMP_FRAG_NEEDED:
			/* Too complicated case ... */
			return;
		default:
			/* All others are translated to HOST_UNREACH.
			   rfc2003 contains "deep thoughts" about NET_UNREACH,
			   I believe, it is just ether pollution. --ANK
			 */
			rel_type = ICMPV6_DEST_UNREACH;
			rel_code = ICMPV6_ADDR_UNREACH;
			break;
		}
		break;
	case ICMP_TIME_EXCEEDED:
		if (code != ICMP_EXC_TTL)
			return;
		rel_type = ICMPV6_TIME_EXCEED;
		rel_code = ICMPV6_EXC_HOPLIMIT;
		break;
	}

	/* Prepare fake skb to feed it to icmpv6_send */
	skb2 = skb_clone(skb, GFP_ATOMIC);
	if (skb2 == NULL)
		return;
	dst_release(skb2->dst);
	skb2->dst = NULL;
	skb_pull(skb2, skb->data - (u8*)iph6);
	skb2->nh.raw = skb2->data;

	/* Try to guess incoming interface */
	rt6i = rt6_lookup(&iph6->saddr, NULL, NULL, 0);
	if (rt6i && rt6i->rt6i_dev) {
		skb2->dev = rt6i->rt6i_dev;

		rt6i = rt6_lookup(&iph6->daddr, &iph6->saddr, NULL, 0);

		if (rt6i && rt6i->rt6i_dev && rt6i->rt6i_dev->type == ARPHRD_SIT) {
			struct ip_tunnel * t = (struct ip_tunnel*)rt6i->rt6i_dev->priv;
			if (rel_type == ICMPV6_TIME_EXCEED && t->parms.iph.ttl) {
				rel_type = ICMPV6_DEST_UNREACH;
				rel_code = ICMPV6_ADDR_UNREACH;
			}
			icmpv6_send(skb2, rel_type, rel_code, rel_info, skb2->dev);
		}
	}
	kfree_skb(skb2);
	return;
#endif
}

static inline void ipip6_ecn_decapsulate(struct iphdr *iph, struct sk_buff *skb)
{
	if (INET_ECN_is_ce(iph->tos) &&
	    INET_ECN_is_not_ce(ip6_get_dsfield(skb->nh.ipv6h)))
		IP6_ECN_set_ce(skb->nh.ipv6h);
}

int ipip6_rcv(struct sk_buff *skb)
{
	struct iphdr *iph;
	struct ip_tunnel *tunnel;

	if (!pskb_may_pull(skb, sizeof(struct ipv6hdr)))
		goto out;

	iph = skb->nh.iph;

	read_lock(&ipip6_lock);
	if ((tunnel = ipip6_tunnel_lookup(iph->saddr, iph->daddr)) != NULL) {
		skb->mac.raw = skb->nh.raw;
		skb->nh.raw = skb->data;
		memset(&(IPCB(skb)->opt), 0, sizeof(struct ip_options));
		skb->protocol = htons(ETH_P_IPV6);
		skb->pkt_type = PACKET_HOST;
		tunnel->stat.rx_packets++;
		tunnel->stat.rx_bytes += skb->len;
		skb->dev = tunnel->dev;
		dst_release(skb->dst);
		skb->dst = NULL;
#ifdef CONFIG_NETFILTER
		nf_conntrack_put(skb->nfct);
		skb->nfct = NULL;
#ifdef CONFIG_NETFILTER_DEBUG
		skb->nf_debug = 0;
#endif
#endif
		ipip6_ecn_decapsulate(iph, skb);
		netif_rx(skb);
		read_unlock(&ipip6_lock);
		return 0;
	}

	icmp_send(skb, ICMP_DEST_UNREACH, ICMP_PROT_UNREACH, 0);
	kfree_skb(skb);
	read_unlock(&ipip6_lock);
out:
	return 0;
}

/* Need this wrapper because NF_HOOK takes the function address */
static inline int do_ip_send(struct sk_buff *skb)
{
	return ip_send(skb);
}


/* Returns the embedded IPv4 address if the IPv6 address
   comes from 6to4 (draft-ietf-ngtrans-6to4-04) addr space */

static inline u32 try_6to4(struct in6_addr *v6dst)
{
	u32 dst = 0;

	if (v6dst->s6_addr16[0] == htons(0x2002)) {
	        /* 6to4 v6 addr has 16 bits prefix, 32 v4addr, 16 SLA, ... */
		memcpy(&dst, &v6dst->s6_addr16[1], 4);
	}
	return dst;
}

/*
 *	This function assumes it is being called from dev_queue_xmit()
 *	and that skb is filled properly by that function.
 */

static int ipip6_tunnel_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct ip_tunnel *tunnel = (struct ip_tunnel*)dev->priv;
	struct net_device_stats *stats = &tunnel->stat;
	struct iphdr  *tiph = &tunnel->parms.iph;
	struct ipv6hdr *iph6 = skb->nh.ipv6h;
	u8     tos = tunnel->parms.iph.tos;
	struct rtable *rt;     			/* Route to the other host */
	struct net_device *tdev;			/* Device to other host */
	struct iphdr  *iph;			/* Our new IP header */
	int    max_headroom;			/* The extra header space needed */
	u32    dst = tiph->daddr;
	int    mtu;
	struct in6_addr *addr6;	
	int addr_type;

	if (tunnel->recursion++) {
		tunnel->stat.collisions++;
		goto tx_error;
	}

	if (skb->protocol != htons(ETH_P_IPV6))
		goto tx_error;

	if (!dst)
		dst = try_6to4(&iph6->daddr);

	if (!dst) {
		struct neighbour *neigh = NULL;

		if (skb->dst)
			neigh = skb->dst->neighbour;

		if (neigh == NULL) {
			if (net_ratelimit())
				printk(KERN_DEBUG "sit: nexthop == NULL\n");
			goto tx_error;
		}

		addr6 = (struct in6_addr*)&neigh->primary_key;
		addr_type = ipv6_addr_type(addr6);

		if (addr_type == IPV6_ADDR_ANY) {
			addr6 = &skb->nh.ipv6h->daddr;
			addr_type = ipv6_addr_type(addr6);
		}

		if ((addr_type & IPV6_ADDR_COMPATv4) == 0)
			goto tx_error_icmp;

		dst = addr6->s6_addr32[3];
	}

	if (ip_route_output(&rt, dst, tiph->saddr, RT_TOS(tos), tunnel->parms.link)) {
		tunnel->stat.tx_carrier_errors++;
		goto tx_error_icmp;
	}
	if (rt->rt_type != RTN_UNICAST) {
		tunnel->stat.tx_carrier_errors++;
		goto tx_error_icmp;
	}
	tdev = rt->u.dst.dev;

	if (tdev == dev) {
		ip_rt_put(rt);
		tunnel->stat.collisions++;
		goto tx_error;
	}

	if (tiph->frag_off)
		mtu = rt->u.dst.pmtu - sizeof(struct iphdr);
	else
		mtu = skb->dst ? skb->dst->pmtu : dev->mtu;

	if (mtu < 68) {
		tunnel->stat.collisions++;
		ip_rt_put(rt);
		goto tx_error;
	}
	if (mtu < IPV6_MIN_MTU)
		mtu = IPV6_MIN_MTU;
	if (skb->dst && mtu < skb->dst->pmtu) {
		struct rt6_info *rt6 = (struct rt6_info*)skb->dst;
		if (mtu < rt6->u.dst.pmtu) {
			if (tunnel->parms.iph.daddr || rt6->rt6i_dst.plen == 128) {
				rt6->rt6i_flags |= RTF_MODIFIED;
				rt6->u.dst.pmtu = mtu;
			}
		}
	}
	if (skb->len > mtu) {
		icmpv6_send(skb, ICMPV6_PKT_TOOBIG, 0, mtu, dev);
		ip_rt_put(rt);
		goto tx_error;
	}

	if (tunnel->err_count > 0) {
		if (jiffies - tunnel->err_time < IPTUNNEL_ERR_TIMEO) {
			tunnel->err_count--;
			dst_link_failure(skb);
		} else
			tunnel->err_count = 0;
	}

	/*
	 * Okay, now see if we can stuff it in the buffer as-is.
	 */
	max_headroom = (((tdev->hard_header_len+15)&~15)+sizeof(struct iphdr));

	if (skb_headroom(skb) < max_headroom || skb_cloned(skb) || skb_shared(skb)) {
		struct sk_buff *new_skb = skb_realloc_headroom(skb, max_headroom);
		if (!new_skb) {
			ip_rt_put(rt);
  			stats->tx_dropped++;
			dev_kfree_skb(skb);
			tunnel->recursion--;
			return 0;
		}
		if (skb->sk)
			skb_set_owner_w(new_skb, skb->sk);
		dev_kfree_skb(skb);
		skb = new_skb;
		iph6 = skb->nh.ipv6h;
	}

	skb->h.raw = skb->nh.raw;
	skb->nh.raw = skb_push(skb, sizeof(struct iphdr));
	memset(&(IPCB(skb)->opt), 0, sizeof(IPCB(skb)->opt));
	dst_release(skb->dst);
	skb->dst = &rt->u.dst;

	/*
	 *	Push down and install the IPIP header.
	 */

	iph 			=	skb->nh.iph;
	iph->version		=	4;
	iph->ihl		=	sizeof(struct iphdr)>>2;
	if (mtu > IPV6_MIN_MTU)
		iph->frag_off	=	htons(IP_DF);
	else
		iph->frag_off	=	0;

	iph->protocol		=	IPPROTO_IPV6;
	iph->tos		=	INET_ECN_encapsulate(tos, ip6_get_dsfield(iph6));
	iph->daddr		=	rt->rt_dst;
	iph->saddr		=	rt->rt_src;

	if ((iph->ttl = tiph->ttl) == 0)
		iph->ttl	=	iph6->hop_limit;

#ifdef CONFIG_NETFILTER
	nf_conntrack_put(skb->nfct);
	skb->nfct = NULL;
#ifdef CONFIG_NETFILTER_DEBUG
	skb->nf_debug = 0;
#endif
#endif

	IPTUNNEL_XMIT();
	tunnel->recursion--;
	return 0;

tx_error_icmp:
	dst_link_failure(skb);
tx_error:
	stats->tx_errors++;
	dev_kfree_skb(skb);
	tunnel->recursion--;
	return 0;
}

static int
ipip6_tunnel_ioctl (struct net_device *dev, struct ifreq *ifr, int cmd)
{
	int err = 0;
	struct ip_tunnel_parm p;
	struct ip_tunnel *t;

	MOD_INC_USE_COUNT;

	switch (cmd) {
	case SIOCGETTUNNEL:
		t = NULL;
		if (dev == &ipip6_fb_tunnel_dev) {
			if (copy_from_user(&p, ifr->ifr_ifru.ifru_data, sizeof(p))) {
				err = -EFAULT;
				break;
			}
			t = ipip6_tunnel_locate(&p, 0);
		}
		if (t == NULL)
			t = (struct ip_tunnel*)dev->priv;
		memcpy(&p, &t->parms, sizeof(p));
		if (copy_to_user(ifr->ifr_ifru.ifru_data, &p, sizeof(p)))
			err = -EFAULT;
		break;

	case SIOCADDTUNNEL:
	case SIOCCHGTUNNEL:
		err = -EPERM;
		if (!capable(CAP_NET_ADMIN))
			goto done;

		err = -EFAULT;
		if (copy_from_user(&p, ifr->ifr_ifru.ifru_data, sizeof(p)))
			goto done;

		err = -EINVAL;
		if (p.iph.version != 4 || p.iph.protocol != IPPROTO_IPV6 ||
		    p.iph.ihl != 5 || (p.iph.frag_off&htons(~IP_DF)))
			goto done;
		if (p.iph.ttl)
			p.iph.frag_off |= htons(IP_DF);

		t = ipip6_tunnel_locate(&p, cmd == SIOCADDTUNNEL);

		if (dev != &ipip6_fb_tunnel_dev && cmd == SIOCCHGTUNNEL &&
		    t != &ipip6_fb_tunnel) {
			if (t != NULL) {
				if (t->dev != dev) {
					err = -EEXIST;
					break;
				}
			} else {
				if (((dev->flags&IFF_POINTOPOINT) && !p.iph.daddr) ||
				    (!(dev->flags&IFF_POINTOPOINT) && p.iph.daddr)) {
					err = -EINVAL;
					break;
				}
				t = (struct ip_tunnel*)dev->priv;
				ipip6_tunnel_unlink(t);
				t->parms.iph.saddr = p.iph.saddr;
				t->parms.iph.daddr = p.iph.daddr;
				memcpy(dev->dev_addr, &p.iph.saddr, 4);
				memcpy(dev->broadcast, &p.iph.daddr, 4);
				ipip6_tunnel_link(t);
				netdev_state_change(dev);
			}
		}

		if (t) {
			err = 0;
			if (cmd == SIOCCHGTUNNEL) {
				t->parms.iph.ttl = p.iph.ttl;
				t->parms.iph.tos = p.iph.tos;
			}
			if (copy_to_user(ifr->ifr_ifru.ifru_data, &t->parms, sizeof(p)))
				err = -EFAULT;
		} else
			err = (cmd == SIOCADDTUNNEL ? -ENOBUFS : -ENOENT);
		break;

	case SIOCDELTUNNEL:
		err = -EPERM;
		if (!capable(CAP_NET_ADMIN))
			goto done;

		if (dev == &ipip6_fb_tunnel_dev) {
			err = -EFAULT;
			if (copy_from_user(&p, ifr->ifr_ifru.ifru_data, sizeof(p)))
				goto done;
			err = -ENOENT;
			if ((t = ipip6_tunnel_locate(&p, 0)) == NULL)
				goto done;
			err = -EPERM;
			if (t == &ipip6_fb_tunnel)
				goto done;
			dev = t->dev;
		}
		err = unregister_netdevice(dev);
		break;

	default:
		err = -EINVAL;
	}

done:
	MOD_DEC_USE_COUNT;
	return err;
}

static struct net_device_stats *ipip6_tunnel_get_stats(struct net_device *dev)
{
	return &(((struct ip_tunnel*)dev->priv)->stat);
}

static int ipip6_tunnel_change_mtu(struct net_device *dev, int new_mtu)
{
	if (new_mtu < IPV6_MIN_MTU || new_mtu > 0xFFF8 - sizeof(struct iphdr))
		return -EINVAL;
	dev->mtu = new_mtu;
	return 0;
}

static void ipip6_tunnel_init_gen(struct net_device *dev)
{
	struct ip_tunnel *t = (struct ip_tunnel*)dev->priv;

	dev->destructor		= ipip6_tunnel_destructor;
	dev->uninit		= ipip6_tunnel_uninit;
	dev->hard_start_xmit	= ipip6_tunnel_xmit;
	dev->get_stats		= ipip6_tunnel_get_stats;
	dev->do_ioctl		= ipip6_tunnel_ioctl;
	dev->change_mtu		= ipip6_tunnel_change_mtu;

	dev->type		= ARPHRD_SIT;
	dev->hard_header_len 	= LL_MAX_HEADER + sizeof(struct iphdr);
	dev->mtu		= 1500 - sizeof(struct iphdr);
	dev->flags		= IFF_NOARP;
	dev->iflink		= 0;
	dev->addr_len		= 4;
	memcpy(dev->dev_addr, &t->parms.iph.saddr, 4);
	memcpy(dev->broadcast, &t->parms.iph.daddr, 4);
}

static int ipip6_tunnel_init(struct net_device *dev)
{
	struct net_device *tdev = NULL;
	struct ip_tunnel *tunnel;
	struct iphdr *iph;

	tunnel = (struct ip_tunnel*)dev->priv;
	iph = &tunnel->parms.iph;

	ipip6_tunnel_init_gen(dev);

	if (iph->daddr) {
		struct rtable *rt;
		if (!ip_route_output(&rt, iph->daddr, iph->saddr, RT_TOS(iph->tos), tunnel->parms.link)) {
			tdev = rt->u.dst.dev;
			ip_rt_put(rt);
		}
		dev->flags |= IFF_POINTOPOINT;
	}

	if (!tdev && tunnel->parms.link)
		tdev = __dev_get_by_index(tunnel->parms.link);

	if (tdev) {
		dev->hard_header_len = tdev->hard_header_len + sizeof(struct iphdr);
		dev->mtu = tdev->mtu - sizeof(struct iphdr);
		if (dev->mtu < IPV6_MIN_MTU)
			dev->mtu = IPV6_MIN_MTU;
	}
	dev->iflink = tunnel->parms.link;

	return 0;
}

#ifdef MODULE
static int ipip6_fb_tunnel_open(struct net_device *dev)
{
	MOD_INC_USE_COUNT;
	return 0;
}

static int ipip6_fb_tunnel_close(struct net_device *dev)
{
	MOD_DEC_USE_COUNT;
	return 0;
}
#endif

int __init ipip6_fb_tunnel_init(struct net_device *dev)
{
	struct iphdr *iph;

	ipip6_tunnel_init_gen(dev);
#ifdef MODULE
	dev->open		= ipip6_fb_tunnel_open;
	dev->stop		= ipip6_fb_tunnel_close;
#endif

	iph = &ipip6_fb_tunnel.parms.iph;
	iph->version		= 4;
	iph->protocol		= IPPROTO_IPV6;
	iph->ihl		= 5;
	iph->ttl		= 64;

	dev_hold(dev);
	tunnels_wc[0]		= &ipip6_fb_tunnel;
	return 0;
}

static struct inet_protocol sit_protocol = {
	ipip6_rcv,
	ipip6_err,
	0,
	IPPROTO_IPV6,
	0,
	NULL,
	"IPv6"
};

#ifdef MODULE
void sit_cleanup(void)
{
	inet_del_protocol(&sit_protocol);
	unregister_netdev(&ipip6_fb_tunnel_dev);
}
#endif

int __init sit_init(void)
{
	printk(KERN_INFO "IPv6 over IPv4 tunneling driver\n");

	ipip6_fb_tunnel_dev.priv = (void*)&ipip6_fb_tunnel;
	strcpy(ipip6_fb_tunnel_dev.name, ipip6_fb_tunnel.parms.name);
	register_netdev(&ipip6_fb_tunnel_dev);
	inet_add_protocol(&sit_protocol);
	return 0;
}
