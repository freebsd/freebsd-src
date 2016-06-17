/*
 *	Extension Header handling for IPv6
 *	Linux INET6 implementation
 *
 *	Authors:
 *	Pedro Roque		<roque@di.fc.ul.pt>
 *	Andi Kleen		<ak@muc.de>
 *	Alexey Kuznetsov	<kuznet@ms2.inr.ac.ru>
 *
 *	$Id: exthdrs.c,v 1.13 2001/06/19 15:58:56 davem Exp $
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

/* Changes:
 *	yoshfuji		: ensure not to overrun while parsing 
 *				  tlv options.
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

#include <net/sock.h>
#include <net/snmp.h>

#include <net/ipv6.h>
#include <net/protocol.h>
#include <net/transp_v6.h>
#include <net/rawv6.h>
#include <net/ndisc.h>
#include <net/ip6_route.h>
#include <net/addrconf.h>

#include <asm/uaccess.h>

/*
 *	Parsing inbound headers.
 *
 *	Parsing function "func" returns offset wrt skb->nh of the place,
 *	where next nexthdr value is stored or NULL, if parsing
 *	failed. It should also update skb->h tp point at the next header.
 */

struct hdrtype_proc
{
	int	type;
	int	(*func) (struct sk_buff **, int offset);
};

/*
 *	Parsing tlv encoded headers.
 *
 *	Parsing function "func" returns 1, if parsing succeed
 *	and 0, if it failed.
 *	It MUST NOT touch skb->h.
 */

struct tlvtype_proc
{
	int	type;
	int	(*func) (struct sk_buff *, int offset);
};

/*********************
  Generic functions
 *********************/

/* An unknown option is detected, decide what to do */

int ip6_tlvopt_unknown(struct sk_buff *skb, int optoff)
{
	switch ((skb->nh.raw[optoff] & 0xC0) >> 6) {
	case 0: /* ignore */
		return 1;

	case 1: /* drop packet */
		break;

	case 3: /* Send ICMP if not a multicast address and drop packet */
		/* Actually, it is redundant check. icmp_send
		   will recheck in any case.
		 */
		if (ipv6_addr_is_multicast(&skb->nh.ipv6h->daddr))
			break;
	case 2: /* send ICMP PARM PROB regardless and drop packet */
		icmpv6_param_prob(skb, ICMPV6_UNK_OPTION, optoff);
		return 0;
	};

	kfree_skb(skb);
	return 0;
}

/* Parse tlv encoded option header (hop-by-hop or destination) */

static int ip6_parse_tlv(struct tlvtype_proc *procs, struct sk_buff *skb)
{
	struct tlvtype_proc *curr;
	int off = skb->h.raw - skb->nh.raw;
	int len = ((skb->h.raw[1]+1)<<3);

	if ((skb->h.raw + len) - skb->data > skb_headlen(skb))
		goto bad;

	off += 2;
	len -= 2;

	while (len > 0) {
		int optlen = skb->nh.raw[off+1]+2;

		switch (skb->nh.raw[off]) {
		case IPV6_TLV_PAD0:
			optlen = 1;
			break;

		case IPV6_TLV_PADN:
			break;

		default: /* Other TLV code so scan list */
			if (optlen > len)
				goto bad;
			for (curr=procs; curr->type >= 0; curr++) {
				if (curr->type == skb->nh.raw[off]) {
					/* type specific length/alignment 
					   checks will be perfomed in the 
					   func(). */
					if (curr->func(skb, off) == 0)
						return 0;
					break;
				}
			}
			if (curr->type < 0) {
				if (ip6_tlvopt_unknown(skb, off) == 0)
					return 0;
			}
			break;
		}
		off += optlen;
		len -= optlen;
	}
	if (len == 0)
		return 1;
bad:
	kfree_skb(skb);
	return 0;
}

/*****************************
  Destination options header.
 *****************************/

struct tlvtype_proc tlvprocdestopt_lst[] = {
	/* No destination options are defined now */
	{-1,			NULL}
};

static int ipv6_dest_opt(struct sk_buff **skb_ptr, int nhoff)
{
	struct sk_buff *skb=*skb_ptr;
	struct inet6_skb_parm *opt = (struct inet6_skb_parm *)skb->cb;

	if (!pskb_may_pull(skb, (skb->h.raw-skb->data)+8) ||
	    !pskb_may_pull(skb, (skb->h.raw-skb->data)+((skb->h.raw[1]+1)<<3))) {
		kfree_skb(skb);
		return -1;
	}

	opt->dst1 = skb->h.raw - skb->nh.raw;

	if (ip6_parse_tlv(tlvprocdestopt_lst, skb)) {
		skb->h.raw += ((skb->h.raw[1]+1)<<3);
		return opt->dst1;
	}

	return -1;
}

/********************************
  NONE header. No data in packet.
 ********************************/

static int ipv6_nodata(struct sk_buff **skb_ptr, int nhoff)
{
	kfree_skb(*skb_ptr);
	return -1;
}

/********************************
  Routing header.
 ********************************/

static int ipv6_routing_header(struct sk_buff **skb_ptr, int nhoff)
{
	struct sk_buff *skb = *skb_ptr;
	struct inet6_skb_parm *opt = (struct inet6_skb_parm *)skb->cb;
	struct in6_addr *addr;
	struct in6_addr daddr;
	int addr_type;
	int n, i;

	struct ipv6_rt_hdr *hdr;
	struct rt0_hdr *rthdr;

	if (!pskb_may_pull(skb, (skb->h.raw-skb->data)+8) ||
	    !pskb_may_pull(skb, (skb->h.raw-skb->data)+((skb->h.raw[1]+1)<<3))) {
		IP6_INC_STATS_BH(Ip6InHdrErrors);
		kfree_skb(skb);
		return -1;
	}

	hdr = (struct ipv6_rt_hdr *) skb->h.raw;

	if ((ipv6_addr_type(&skb->nh.ipv6h->daddr)&IPV6_ADDR_MULTICAST) ||
	    skb->pkt_type != PACKET_HOST) {
		kfree_skb(skb);
		return -1;
	}

looped_back:
	if (hdr->segments_left == 0) {
		opt->srcrt = skb->h.raw - skb->nh.raw;
		skb->h.raw += (hdr->hdrlen + 1) << 3;
		opt->dst0 = opt->dst1;
		opt->dst1 = 0;
		return (&hdr->nexthdr) - skb->nh.raw;
	}

	if (hdr->type != IPV6_SRCRT_TYPE_0) {
		icmpv6_param_prob(skb, ICMPV6_HDR_FIELD, (&hdr->type) - skb->nh.raw);
		return -1;
	}
	
	if (hdr->hdrlen & 0x01) {
		icmpv6_param_prob(skb, ICMPV6_HDR_FIELD, (&hdr->hdrlen) - skb->nh.raw);
		return -1;
	}

	/*
	 *	This is the routing header forwarding algorithm from
	 *	RFC 1883, page 17.
	 */

	n = hdr->hdrlen >> 1;

	if (hdr->segments_left > n) {
		icmpv6_param_prob(skb, ICMPV6_HDR_FIELD, (&hdr->segments_left) - skb->nh.raw);
		return -1;
	}

	/* We are about to mangle packet header. Be careful!
	   Do not damage packets queued somewhere.
	 */
	if (skb_cloned(skb)) {
		struct sk_buff *skb2 = skb_copy(skb, GFP_ATOMIC);
		kfree_skb(skb);
		if (skb2 == NULL)
			return -1;
		*skb_ptr = skb = skb2;
		opt = (struct inet6_skb_parm *)skb2->cb;
		hdr = (struct ipv6_rt_hdr *) skb2->h.raw;
	}

	if (skb->ip_summed == CHECKSUM_HW)
		skb->ip_summed = CHECKSUM_NONE;

	i = n - --hdr->segments_left;

	rthdr = (struct rt0_hdr *) hdr;
	addr = rthdr->addr;
	addr += i - 1;

	addr_type = ipv6_addr_type(addr);

	if (addr_type&IPV6_ADDR_MULTICAST) {
		kfree_skb(skb);
		return -1;
	}

	ipv6_addr_copy(&daddr, addr);
	ipv6_addr_copy(addr, &skb->nh.ipv6h->daddr);
	ipv6_addr_copy(&skb->nh.ipv6h->daddr, &daddr);

	dst_release(xchg(&skb->dst, NULL));
	ip6_route_input(skb);
	if (skb->dst->error) {
		skb->dst->input(skb);
		return -1;
	}
	if (skb->dst->dev->flags&IFF_LOOPBACK) {
		if (skb->nh.ipv6h->hop_limit <= 1) {
			icmpv6_send(skb, ICMPV6_TIME_EXCEED, ICMPV6_EXC_HOPLIMIT,
				    0, skb->dev);
			kfree_skb(skb);
			return -1;
		}
		skb->nh.ipv6h->hop_limit--;
		goto looped_back;
	}

	skb->dst->input(skb);
	return -1;
}

/*
   This function inverts received rthdr.
   NOTE: specs allow to make it automatically only if
   packet authenticated.

   I will not discuss it here (though, I am really pissed off at
   this stupid requirement making rthdr idea useless)

   Actually, it creates severe problems  for us.
   Embrionic requests has no associated sockets,
   so that user have no control over it and
   cannot not only to set reply options, but
   even to know, that someone wants to connect
   without success. :-(

   For now we need to test the engine, so that I created
   temporary (or permanent) backdoor.
   If listening socket set IPV6_RTHDR to 2, then we invert header.
                                                   --ANK (980729)
 */

struct ipv6_txoptions *
ipv6_invert_rthdr(struct sock *sk, struct ipv6_rt_hdr *hdr)
{
	/* Received rthdr:

	   [ H1 -> H2 -> ... H_prev ]  daddr=ME

	   Inverted result:
	   [ H_prev -> ... -> H1 ] daddr =sender

	   Note, that IP output engine will rewrire this rthdr
	   by rotating it left by one addr.
	 */

	int n, i;
	struct rt0_hdr *rthdr = (struct rt0_hdr*)hdr;
	struct rt0_hdr *irthdr;
	struct ipv6_txoptions *opt;
	int hdrlen = ipv6_optlen(hdr);

	if (hdr->segments_left ||
	    hdr->type != IPV6_SRCRT_TYPE_0 ||
	    hdr->hdrlen & 0x01)
		return NULL;

	n = hdr->hdrlen >> 1;
	opt = sock_kmalloc(sk, sizeof(*opt) + hdrlen, GFP_ATOMIC);
	if (opt == NULL)
		return NULL;
	memset(opt, 0, sizeof(*opt));
	opt->tot_len = sizeof(*opt) + hdrlen;
	opt->srcrt = (void*)(opt+1);
	opt->opt_nflen = hdrlen;

	memcpy(opt->srcrt, hdr, sizeof(*hdr));
	irthdr = (struct rt0_hdr*)opt->srcrt;
	/* Obsolete field, MBZ, when originated by us */
	irthdr->bitmap = 0;
	opt->srcrt->segments_left = n;
	for (i=0; i<n; i++)
		memcpy(irthdr->addr+i, rthdr->addr+(n-1-i), 16);
	return opt;
}

/********************************
  AUTH header.
 ********************************/

/*
   rfc1826 said, that if a host does not implement AUTH header
   it MAY ignore it. We use this hole 8)

   Actually, now we can implement OSPFv6 without kernel IPsec.
   Authentication for poors may be done in user space with the same success.

   Yes, it means, that we allow application to send/receive
   raw authentication header. Apparently, we suppose, that it knows
   what it does and calculates authentication data correctly.
   Certainly, it is possible only for udp and raw sockets, but not for tcp.

   AUTH header has 4byte granular length, which kills all the idea
   behind AUTOMATIC 64bit alignment of IPv6. Now we will lose
   cpu ticks, checking that sender did not something stupid
   and opt->hdrlen is even. Shit!		--ANK (980730)
 */

static int ipv6_auth_hdr(struct sk_buff **skb_ptr, int nhoff)
{
	struct sk_buff *skb=*skb_ptr;
	struct inet6_skb_parm *opt = (struct inet6_skb_parm *)skb->cb;
	int len;

	if (!pskb_may_pull(skb, (skb->h.raw-skb->data)+8))
		goto fail;

	/*
	 * RFC2402 2.2 Payload Length
	 * The 8-bit field specifies the length of AH in 32-bit words 
	 * (4-byte units), minus "2".
	 * -- Noriaki Takamiya @USAGI Project
	 */
	len = (skb->h.raw[1]+2)<<2;

	if (len&7)
		goto fail;

	if (!pskb_may_pull(skb, (skb->h.raw-skb->data)+len))
		goto fail;

	opt->auth = skb->h.raw - skb->nh.raw;
	skb->h.raw += len;
	return opt->auth;

fail:
	kfree_skb(skb);
	return -1;
}

/* This list MUST NOT contain entry for NEXTHDR_HOP.
   It is parsed immediately after packet received
   and if it occurs somewhere in another place we must
   generate error.
 */

struct hdrtype_proc hdrproc_lst[] = {
	{NEXTHDR_FRAGMENT,	ipv6_reassembly},
	{NEXTHDR_ROUTING,	ipv6_routing_header},
	{NEXTHDR_DEST,		ipv6_dest_opt},
	{NEXTHDR_NONE,		ipv6_nodata},
	{NEXTHDR_AUTH,		ipv6_auth_hdr},
   /*
	{NEXTHDR_ESP,		ipv6_esp_hdr},
    */
	{-1,			NULL}
};

int ipv6_parse_exthdrs(struct sk_buff **skb_in, int nhoff)
{
	struct hdrtype_proc *hdrt;
	u8 nexthdr = (*skb_in)->nh.raw[nhoff];

restart:
	for (hdrt=hdrproc_lst; hdrt->type >= 0; hdrt++) {
		if (hdrt->type == nexthdr) {
			if ((nhoff = hdrt->func(skb_in, nhoff)) >= 0) {
				nexthdr = (*skb_in)->nh.raw[nhoff];
				goto restart;
			}
			return -1;
		}
	}
	return nhoff;
}


/**********************************
  Hop-by-hop options.
 **********************************/

/* Router Alert as of draft-ietf-ipngwg-ipv6router-alert-04 */

static int ipv6_hop_ra(struct sk_buff *skb, int optoff)
{
	if (skb->nh.raw[optoff+1] == 2) {
		((struct inet6_skb_parm*)skb->cb)->ra = optoff;
		return 1;
	}
	if (net_ratelimit())
		printk(KERN_DEBUG "ipv6_hop_ra: wrong RA length %d\n", skb->nh.raw[optoff+1]);
	kfree_skb(skb);
	return 0;
}

/* Jumbo payload */

static int ipv6_hop_jumbo(struct sk_buff *skb, int optoff)
{
	u32 pkt_len;

	if (skb->nh.raw[optoff+1] != 4 || (optoff&3) != 2) {
		if (net_ratelimit())
			printk(KERN_DEBUG "ipv6_hop_jumbo: wrong jumbo opt length/alignment %d\n", skb->nh.raw[optoff+1]);
		goto drop;
	}

	pkt_len = ntohl(*(u32*)(skb->nh.raw+optoff+2));
	if (pkt_len < 0x10000) {
		icmpv6_param_prob(skb, ICMPV6_HDR_FIELD, optoff+2);
		return 0;
	}
	if (skb->nh.ipv6h->payload_len) {
		icmpv6_param_prob(skb, ICMPV6_HDR_FIELD, optoff);
		return 0;
	}

	if (pkt_len > skb->len - sizeof(struct ipv6hdr)) {
		IP6_INC_STATS_BH(Ip6InTruncatedPkts);
		goto drop;
	}
	if (pkt_len + sizeof(struct ipv6hdr) < skb->len) {
		__pskb_trim(skb, pkt_len + sizeof(struct ipv6hdr));
		if (skb->ip_summed == CHECKSUM_HW)
			skb->ip_summed = CHECKSUM_NONE;
	}
	return 1;

drop:
	kfree_skb(skb);
	return 0;
}

struct tlvtype_proc tlvprochopopt_lst[] = {
	{IPV6_TLV_ROUTERALERT,	ipv6_hop_ra},
	{IPV6_TLV_JUMBO,	ipv6_hop_jumbo},
	{-1,			NULL}
};

int ipv6_parse_hopopts(struct sk_buff *skb, int nhoff)
{
	((struct inet6_skb_parm*)skb->cb)->hop = sizeof(struct ipv6hdr);
	if (ip6_parse_tlv(tlvprochopopt_lst, skb))
		return sizeof(struct ipv6hdr);
	return -1;
}

/*
 *	Creating outbound headers.
 *
 *	"build" functions work when skb is filled from head to tail (datagram)
 *	"push"	functions work when headers are added from tail to head (tcp)
 *
 *	In both cases we assume, that caller reserved enough room
 *	for headers.
 */

u8 *ipv6_build_rthdr(struct sk_buff *skb, u8 *prev_hdr,
		     struct ipv6_rt_hdr *opt, struct in6_addr *addr)
{
	struct rt0_hdr *phdr, *ihdr;
	int hops;

	ihdr = (struct rt0_hdr *) opt;
	
	phdr = (struct rt0_hdr *) skb_put(skb, (ihdr->rt_hdr.hdrlen + 1) << 3);
	memcpy(phdr, ihdr, sizeof(struct rt0_hdr));

	hops = ihdr->rt_hdr.hdrlen >> 1;

	if (hops > 1)
		memcpy(phdr->addr, ihdr->addr + 1,
		       (hops - 1) * sizeof(struct in6_addr));

	ipv6_addr_copy(phdr->addr + (hops - 1), addr);

	phdr->rt_hdr.nexthdr = *prev_hdr;
	*prev_hdr = NEXTHDR_ROUTING;
	return &phdr->rt_hdr.nexthdr;
}

static u8 *ipv6_build_exthdr(struct sk_buff *skb, u8 *prev_hdr, u8 type, struct ipv6_opt_hdr *opt)
{
	struct ipv6_opt_hdr *h = (struct ipv6_opt_hdr *)skb_put(skb, ipv6_optlen(opt));

	memcpy(h, opt, ipv6_optlen(opt));
	h->nexthdr = *prev_hdr;
	*prev_hdr = type;
	return &h->nexthdr;
}

static u8 *ipv6_build_authhdr(struct sk_buff *skb, u8 *prev_hdr, struct ipv6_opt_hdr *opt)
{
	struct ipv6_opt_hdr *h = (struct ipv6_opt_hdr *)skb_put(skb, (opt->hdrlen+2)<<2);

	memcpy(h, opt, (opt->hdrlen+2)<<2);
	h->nexthdr = *prev_hdr;
	*prev_hdr = NEXTHDR_AUTH;
	return &h->nexthdr;
}


u8 *ipv6_build_nfrag_opts(struct sk_buff *skb, u8 *prev_hdr, struct ipv6_txoptions *opt,
			  struct in6_addr *daddr, u32 jumbolen)
{
	struct ipv6_opt_hdr *h = (struct ipv6_opt_hdr *)skb->data;

	if (opt && opt->hopopt)
		prev_hdr = ipv6_build_exthdr(skb, prev_hdr, NEXTHDR_HOP, opt->hopopt);

	if (jumbolen) {
		u8 *jumboopt = (u8 *)skb_put(skb, 8);

		if (opt && opt->hopopt) {
			*jumboopt++ = IPV6_TLV_PADN;
			*jumboopt++ = 0;
			h->hdrlen++;
		} else {
			h = (struct ipv6_opt_hdr *)jumboopt;
			h->nexthdr = *prev_hdr;
			h->hdrlen = 0;
			jumboopt += 2;
			*prev_hdr = NEXTHDR_HOP;
			prev_hdr = &h->nexthdr;
		}
		jumboopt[0] = IPV6_TLV_JUMBO;
		jumboopt[1] = 4;
		*(u32*)(jumboopt+2) = htonl(jumbolen);
	}
	if (opt) {
		if (opt->dst0opt)
			prev_hdr = ipv6_build_exthdr(skb, prev_hdr, NEXTHDR_DEST, opt->dst0opt);
		if (opt->srcrt)
			prev_hdr = ipv6_build_rthdr(skb, prev_hdr, opt->srcrt, daddr);
	}
	return prev_hdr;
}

u8 *ipv6_build_frag_opts(struct sk_buff *skb, u8 *prev_hdr, struct ipv6_txoptions *opt)
{
	if (opt->auth)
		prev_hdr = ipv6_build_authhdr(skb, prev_hdr, opt->auth);
	if (opt->dst1opt)
		prev_hdr = ipv6_build_exthdr(skb, prev_hdr, NEXTHDR_DEST, opt->dst1opt);
	return prev_hdr;
}

static void ipv6_push_rthdr(struct sk_buff *skb, u8 *proto,
			    struct ipv6_rt_hdr *opt,
			    struct in6_addr **addr_p)
{
	struct rt0_hdr *phdr, *ihdr;
	int hops;

	ihdr = (struct rt0_hdr *) opt;
	
	phdr = (struct rt0_hdr *) skb_push(skb, (ihdr->rt_hdr.hdrlen + 1) << 3);
	memcpy(phdr, ihdr, sizeof(struct rt0_hdr));

	hops = ihdr->rt_hdr.hdrlen >> 1;

	if (hops > 1)
		memcpy(phdr->addr, ihdr->addr + 1,
		       (hops - 1) * sizeof(struct in6_addr));

	ipv6_addr_copy(phdr->addr + (hops - 1), *addr_p);
	*addr_p = ihdr->addr;

	phdr->rt_hdr.nexthdr = *proto;
	*proto = NEXTHDR_ROUTING;
}

static void ipv6_push_exthdr(struct sk_buff *skb, u8 *proto, u8 type, struct ipv6_opt_hdr *opt)
{
	struct ipv6_opt_hdr *h = (struct ipv6_opt_hdr *)skb_push(skb, ipv6_optlen(opt));

	memcpy(h, opt, ipv6_optlen(opt));
	h->nexthdr = *proto;
	*proto = type;
}

static void ipv6_push_authhdr(struct sk_buff *skb, u8 *proto, struct ipv6_opt_hdr *opt)
{
	struct ipv6_opt_hdr *h = (struct ipv6_opt_hdr *)skb_push(skb, (opt->hdrlen+2)<<2);

	memcpy(h, opt, (opt->hdrlen+2)<<2);
	h->nexthdr = *proto;
	*proto = NEXTHDR_AUTH;
}

void ipv6_push_nfrag_opts(struct sk_buff *skb, struct ipv6_txoptions *opt,
			  u8 *proto,
			  struct in6_addr **daddr)
{
	if (opt->srcrt)
		ipv6_push_rthdr(skb, proto, opt->srcrt, daddr);
	if (opt->dst0opt)
		ipv6_push_exthdr(skb, proto, NEXTHDR_DEST, opt->dst0opt);
	if (opt->hopopt)
		ipv6_push_exthdr(skb, proto, NEXTHDR_HOP, opt->hopopt);
}

void ipv6_push_frag_opts(struct sk_buff *skb, struct ipv6_txoptions *opt, u8 *proto)
{
	if (opt->dst1opt)
		ipv6_push_exthdr(skb, proto, NEXTHDR_DEST, opt->dst1opt);
	if (opt->auth)
		ipv6_push_authhdr(skb, proto, opt->auth);
}

struct ipv6_txoptions *
ipv6_dup_options(struct sock *sk, struct ipv6_txoptions *opt)
{
	struct ipv6_txoptions *opt2;

	opt2 = sock_kmalloc(sk, opt->tot_len, GFP_ATOMIC);
	if (opt2) {
		long dif = (char*)opt2 - (char*)opt;
		memcpy(opt2, opt, opt->tot_len);
		if (opt2->hopopt)
			*((char**)&opt2->hopopt) += dif;
		if (opt2->dst0opt)
			*((char**)&opt2->dst0opt) += dif;
		if (opt2->dst1opt)
			*((char**)&opt2->dst1opt) += dif;
		if (opt2->auth)
			*((char**)&opt2->auth) += dif;
		if (opt2->srcrt)
			*((char**)&opt2->srcrt) += dif;
	}
	return opt2;
}


/* 
 * find out if nexthdr is a well-known extension header or a protocol
 */

int ipv6_ext_hdr(u8 nexthdr)
{
	/* 
	 * find out if nexthdr is an extension header or a protocol
	 */
	return ( (nexthdr == NEXTHDR_HOP)	||
		 (nexthdr == NEXTHDR_ROUTING)	||
		 (nexthdr == NEXTHDR_FRAGMENT)	||
		 (nexthdr == NEXTHDR_AUTH)	||
		 (nexthdr == NEXTHDR_NONE)	||
		 (nexthdr == NEXTHDR_DEST) );
}

/*
 * Skip any extension headers. This is used by the ICMP module.
 *
 * Note that strictly speaking this conflicts with RFC1883 4.0:
 * ...The contents and semantics of each extension header determine whether 
 * or not to proceed to the next header.  Therefore, extension headers must
 * be processed strictly in the order they appear in the packet; a
 * receiver must not, for example, scan through a packet looking for a
 * particular kind of extension header and process that header prior to
 * processing all preceding ones.
 * 
 * We do exactly this. This is a protocol bug. We can't decide after a
 * seeing an unknown discard-with-error flavour TLV option if it's a 
 * ICMP error message or not (errors should never be send in reply to
 * ICMP error messages).
 * 
 * But I see no other way to do this. This might need to be reexamined
 * when Linux implements ESP (and maybe AUTH) headers.
 * --AK
 *
 * This function parses (probably truncated) exthdr set "hdr"
 * of length "len". "nexthdrp" initially points to some place,
 * where type of the first header can be found.
 *
 * It skips all well-known exthdrs, and returns pointer to the start
 * of unparsable area i.e. the first header with unknown type.
 * If it is not NULL *nexthdr is updated by type/protocol of this header.
 *
 * NOTES: - if packet terminated with NEXTHDR_NONE it returns NULL.
 *        - it may return pointer pointing beyond end of packet,
 *	    if the last recognized header is truncated in the middle.
 *        - if packet is truncated, so that all parsed headers are skipped,
 *	    it returns NULL.
 *	  - First fragment header is skipped, not-first ones
 *	    are considered as unparsable.
 *	  - ESP is unparsable for now and considered like
 *	    normal payload protocol.
 *	  - Note also special handling of AUTH header. Thanks to IPsec wizards.
 *
 * --ANK (980726)
 */

int ipv6_skip_exthdr(const struct sk_buff *skb, int start, u8 *nexthdrp, int len)
{
	u8 nexthdr = *nexthdrp;

	while (ipv6_ext_hdr(nexthdr)) {
		struct ipv6_opt_hdr hdr;
		int hdrlen;

		if (len < (int)sizeof(struct ipv6_opt_hdr))
			return -1;
		if (nexthdr == NEXTHDR_NONE)
			return -1;
		if (skb_copy_bits(skb, start, &hdr, sizeof(hdr)))
			BUG();
		if (nexthdr == NEXTHDR_FRAGMENT) {
			unsigned short frag_off;
			if (skb_copy_bits(skb,
					  start+offsetof(struct frag_hdr,
							 frag_off),
					  &frag_off,
					  sizeof(frag_off))) {
				return -1;
			}

			if (ntohs(frag_off) & ~0x7)
				break;
			hdrlen = 8;
		} else if (nexthdr == NEXTHDR_AUTH)
			hdrlen = (hdr.hdrlen+2)<<2; 
		else
			hdrlen = ipv6_optlen(&hdr); 

		nexthdr = hdr.nexthdr;
		len -= hdrlen;
		start += hdrlen;
	}

	*nexthdrp = nexthdr;
	return start;
}

