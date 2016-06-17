/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		The IP forwarding functionality.
 *		
 * Version:	$Id: ip_forward.c,v 1.48 2000/12/13 18:31:48 davem Exp $
 *
 * Authors:	see ip.c
 *
 * Fixes:
 *		Many		:	Split from ip.c , see ip_input.c for 
 *					history.
 *		Dave Gregorich	:	NULL ip_rt_put fix for multicast 
 *					routing.
 *		Jos Vos		:	Add call_out_firewall before sending,
 *					use output device for accounting.
 *		Jos Vos		:	Call forward firewall after routing
 *					(always use output device).
 *		Mike McLagan	:	Routing by source
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/icmp.h>
#include <linux/netdevice.h>
#include <net/sock.h>
#include <net/ip.h>
#include <net/tcp.h>
#include <net/udp.h>
#include <net/icmp.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/netfilter_ipv4.h>
#include <net/checksum.h>
#include <linux/route.h>
#include <net/route.h>

static inline int ip_forward_finish(struct sk_buff *skb)
{
	struct ip_options * opt	= &(IPCB(skb)->opt);

	IP_INC_STATS_BH(IpForwDatagrams);

	if (opt->optlen == 0) {
#ifdef CONFIG_NET_FASTROUTE
		struct rtable *rt = (struct rtable*)skb->dst;

		if (rt->rt_flags&RTCF_FAST && !netdev_fastroute_obstacles) {
			struct dst_entry *old_dst;
			unsigned h = ((*(u8*)&rt->key.dst)^(*(u8*)&rt->key.src))&NETDEV_FASTROUTE_HMASK;

			write_lock_irq(&skb->dev->fastpath_lock);
			old_dst = skb->dev->fastpath[h];
			skb->dev->fastpath[h] = dst_clone(&rt->u.dst);
			write_unlock_irq(&skb->dev->fastpath_lock);

			dst_release(old_dst);
		}
#endif
		return (ip_send(skb));
	}

	ip_forward_options(skb);
	return (ip_send(skb));
}

int ip_forward(struct sk_buff *skb)
{
	struct net_device *dev2;	/* Output device */
	struct iphdr *iph;	/* Our header */
	struct rtable *rt;	/* Route we use */
	struct ip_options * opt	= &(IPCB(skb)->opt);
	unsigned short mtu;

	if (IPCB(skb)->opt.router_alert && ip_call_ra_chain(skb))
		return NET_RX_SUCCESS;

	if (skb->pkt_type != PACKET_HOST)
		goto drop;

	skb->ip_summed = CHECKSUM_NONE;
	
	/*
	 *	According to the RFC, we must first decrease the TTL field. If
	 *	that reaches zero, we must reply an ICMP control message telling
	 *	that the packet's lifetime expired.
	 */

	iph = skb->nh.iph;
	rt = (struct rtable*)skb->dst;

	if (iph->ttl <= 1)
                goto too_many_hops;

	if (opt->is_strictroute && rt->rt_dst != rt->rt_gateway)
                goto sr_failed;

	/*
	 *	Having picked a route we can now send the frame out
	 *	after asking the firewall permission to do so.
	 */

	skb->priority = rt_tos2priority(iph->tos);
	dev2 = rt->u.dst.dev;
	mtu = rt->u.dst.pmtu;

	/*
	 *	We now generate an ICMP HOST REDIRECT giving the route
	 *	we calculated.
	 */
	if (rt->rt_flags&RTCF_DOREDIRECT && !opt->srr)
		ip_rt_send_redirect(skb);

	/* We are about to mangle packet. Copy it! */
	if (skb_cow(skb, dev2->hard_header_len))
		goto drop;
	iph = skb->nh.iph;

	/* Decrease ttl after skb cow done */
	ip_decrease_ttl(iph);

	/*
	 * We now may allocate a new buffer, and copy the datagram into it.
	 * If the indicated interface is up and running, kick it.
	 */

	if (skb->len > mtu && (ntohs(iph->frag_off) & IP_DF))
		goto frag_needed;

#ifdef CONFIG_IP_ROUTE_NAT
	if (rt->rt_flags & RTCF_NAT) {
		if (ip_do_nat(skb)) {
			kfree_skb(skb);
			return NET_RX_BAD;
		}
	}
#endif

	return NF_HOOK(PF_INET, NF_IP_FORWARD, skb, skb->dev, dev2,
		       ip_forward_finish);

frag_needed:
	IP_INC_STATS_BH(IpFragFails);
	icmp_send(skb, ICMP_DEST_UNREACH, ICMP_FRAG_NEEDED, htonl(mtu));
        goto drop;

sr_failed:
        /*
	 *	Strict routing permits no gatewaying
	 */
         icmp_send(skb, ICMP_DEST_UNREACH, ICMP_SR_FAILED, 0);
         goto drop;

too_many_hops:
        /* Tell the sender its packet died... */
        icmp_send(skb, ICMP_TIME_EXCEEDED, ICMP_EXC_TTL, 0);
drop:
	kfree_skb(skb);
	return NET_RX_DROP;
}
