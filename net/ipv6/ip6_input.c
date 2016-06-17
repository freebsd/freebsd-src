/*
 *	IPv6 input
 *	Linux INET6 implementation 
 *
 *	Authors:
 *	Pedro Roque		<roque@di.fc.ul.pt>
 *	Ian P. Morris		<I.P.Morris@soton.ac.uk>
 *
 *	$Id: ip6_input.c,v 1.19 2000/12/13 18:31:50 davem Exp $
 *
 *	Based in linux/net/ipv4/ip_input.c
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/sched.h>
#include <linux/net.h>
#include <linux/netdevice.h>
#include <linux/in6.h>
#include <linux/icmpv6.h>

#include <linux/netfilter.h>
#include <linux/netfilter_ipv6.h>

#include <net/sock.h>
#include <net/snmp.h>

#include <net/ipv6.h>
#include <net/protocol.h>
#include <net/transp_v6.h>
#include <net/rawv6.h>
#include <net/ndisc.h>
#include <net/ip6_route.h>
#include <net/addrconf.h>



static inline int ip6_rcv_finish( struct sk_buff *skb) 
{
	if (skb->dst == NULL)
		ip6_route_input(skb);

	return skb->dst->input(skb);
}

int ipv6_rcv(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt)
{
	struct ipv6hdr *hdr;
	u32 		pkt_len;

	if (skb->pkt_type == PACKET_OTHERHOST)
		goto drop;

	IP6_INC_STATS_BH(Ip6InReceives);

	if ((skb = skb_share_check(skb, GFP_ATOMIC)) == NULL)
		goto out;

	/* Store incoming device index. When the packet will
	   be queued, we cannot refer to skb->dev anymore.
	 */
	((struct inet6_skb_parm *)skb->cb)->iif = dev->ifindex;

	if (skb->len < sizeof(struct ipv6hdr))
		goto err;

	if (!pskb_may_pull(skb, sizeof(struct ipv6hdr)))
		goto drop;

	hdr = skb->nh.ipv6h;

	if (hdr->version != 6)
		goto err;

	pkt_len = ntohs(hdr->payload_len);

	/* pkt_len may be zero if Jumbo payload option is present */
	if (pkt_len || hdr->nexthdr != NEXTHDR_HOP) {
		if (pkt_len + sizeof(struct ipv6hdr) > skb->len)
			goto truncated;
		if (pkt_len + sizeof(struct ipv6hdr) < skb->len) {
			if (__pskb_trim(skb, pkt_len + sizeof(struct ipv6hdr)))
				goto drop;
			hdr = skb->nh.ipv6h;
			if (skb->ip_summed == CHECKSUM_HW)
				skb->ip_summed = CHECKSUM_NONE;
		}
	}

	if (hdr->nexthdr == NEXTHDR_HOP) {
		skb->h.raw = (u8*)(hdr+1);
		if (ipv6_parse_hopopts(skb, offsetof(struct ipv6hdr, nexthdr)) < 0) {
			IP6_INC_STATS_BH(Ip6InHdrErrors);
			return 0;
		}
		hdr = skb->nh.ipv6h;
	}

	return NF_HOOK(PF_INET6,NF_IP6_PRE_ROUTING, skb, dev, NULL, ip6_rcv_finish);
truncated:
	IP6_INC_STATS_BH(Ip6InTruncatedPkts);
err:
	IP6_INC_STATS_BH(Ip6InHdrErrors);
drop:
	kfree_skb(skb);
out:
	return 0;
}

/*
 *	Deliver the packet to the host
 */


static inline int ip6_input_finish(struct sk_buff *skb)
{
	struct ipv6hdr *hdr = skb->nh.ipv6h;
	struct inet6_protocol *ipprot;
	struct sock *raw_sk;
	int nhoff;
	int nexthdr;
	int found = 0;
	u8 hash;

	skb->h.raw = skb->nh.raw + sizeof(struct ipv6hdr);

	/*
	 *	Parse extension headers
	 */

	nexthdr = hdr->nexthdr;
	nhoff = offsetof(struct ipv6hdr, nexthdr);

	/* Skip  hop-by-hop options, they are already parsed. */
	if (nexthdr == NEXTHDR_HOP) {
		nhoff = sizeof(struct ipv6hdr);
		nexthdr = skb->h.raw[0];
		skb->h.raw += (skb->h.raw[1]+1)<<3;
	}

	/* This check is sort of optimization.
	   It would be stupid to detect for optional headers,
	   which are missing with probability of 200%
	 */
	if (nexthdr != IPPROTO_TCP && nexthdr != IPPROTO_UDP) {
		nhoff = ipv6_parse_exthdrs(&skb, nhoff);
		if (nhoff < 0)
			return 0;
		nexthdr = skb->nh.raw[nhoff];
		hdr = skb->nh.ipv6h;
	}

	if (!pskb_pull(skb, skb->h.raw - skb->data))
		goto discard;

	if (skb->ip_summed == CHECKSUM_HW)
		skb->csum = csum_sub(skb->csum,
				     csum_partial(skb->nh.raw, skb->h.raw-skb->nh.raw, 0));

	raw_sk = raw_v6_htable[nexthdr&(MAX_INET_PROTOS-1)];
	if (raw_sk)
		raw_sk = ipv6_raw_deliver(skb, nexthdr);

	hash = nexthdr & (MAX_INET_PROTOS - 1);
	for (ipprot = (struct inet6_protocol *) inet6_protos[hash]; 
	     ipprot != NULL; 
	     ipprot = (struct inet6_protocol *) ipprot->next) {
		struct sk_buff *buff = skb;

		if (ipprot->protocol != nexthdr)
			continue;

		if (ipprot->copy || raw_sk)
			buff = skb_clone(skb, GFP_ATOMIC);

		if (buff)
			ipprot->handler(buff);
		found = 1;
	}

	if (raw_sk) {
		rawv6_rcv(raw_sk, skb);
		sock_put(raw_sk);
		found = 1;
	}

	/*
	 *	not found: send ICMP parameter problem back
	 */
	if (!found) {
		IP6_INC_STATS_BH(Ip6InUnknownProtos);
		icmpv6_param_prob(skb, ICMPV6_UNK_NEXTHDR, nhoff);
	}

	return 0;

discard:
	kfree_skb(skb);
	return 0;
}


int ip6_input(struct sk_buff *skb)
{
	return NF_HOOK(PF_INET6,NF_IP6_LOCAL_IN, skb, skb->dev, NULL, ip6_input_finish);
}

int ip6_mc_input(struct sk_buff *skb)
{
	struct ipv6hdr *hdr;	
	int deliver = 0;
	int discard = 1;

	IP6_INC_STATS_BH(Ip6InMcastPkts);

	hdr = skb->nh.ipv6h;
	if (ipv6_chk_mcast_addr(skb->dev, &hdr->daddr, &hdr->saddr))
		deliver = 1;

	/*
	 *	IPv6 multicast router mode isnt currently supported.
	 */
#if 0
	if (ipv6_config.multicast_route) {
		int addr_type;

		addr_type = ipv6_addr_type(&hdr->daddr);

		if (!(addr_type & (IPV6_ADDR_LOOPBACK | IPV6_ADDR_LINKLOCAL))) {
			struct sk_buff *skb2;
			struct dst_entry *dst;

			dst = skb->dst;
			
			if (deliver) {
				skb2 = skb_clone(skb, GFP_ATOMIC);
			} else {
				discard = 0;
				skb2 = skb;
			}

			dst->output(skb2);
		}
	}
#endif

	if (deliver) {
		discard = 0;
		ip6_input(skb);
	}

	if (discard)
		kfree_skb(skb);

	return 0;
}
