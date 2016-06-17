/*
 *	IPv6 output functions
 *	Linux INET6 implementation 
 *
 *	Authors:
 *	Pedro Roque		<roque@di.fc.ul.pt>	
 *
 *	$Id: ip6_output.c,v 1.33 2001/09/20 00:35:35 davem Exp $
 *
 *	Based on linux/net/ipv4/ip_output.c
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *	Changes:
 *	A.N.Kuznetsov	:	airthmetics in fragmentation.
 *				extension headers are implemented.
 *				route changes now work.
 *				ip6_forward does not confuse sniffers.
 *				etc.
 *
 *      H. von Brand    :       Added missing #include <linux/string.h>
 *	Imran Patel	: 	frag id should be in NBO
 */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/in6.h>
#include <linux/route.h>

#include <linux/netfilter.h>
#include <linux/netfilter_ipv6.h>

#include <net/sock.h>
#include <net/snmp.h>

#include <net/ipv6.h>
#include <net/ndisc.h>
#include <net/protocol.h>
#include <net/ip6_route.h>
#include <net/addrconf.h>
#include <net/rawv6.h>
#include <net/icmp.h>

static __inline__ void ipv6_select_ident(struct sk_buff *skb, struct frag_hdr *fhdr)
{
	static u32 ipv6_fragmentation_id = 1;
	static spinlock_t ip6_id_lock = SPIN_LOCK_UNLOCKED;

	spin_lock_bh(&ip6_id_lock);
	fhdr->identification = htonl(ipv6_fragmentation_id);
	if (++ipv6_fragmentation_id == 0)
		ipv6_fragmentation_id = 1;
	spin_unlock_bh(&ip6_id_lock);
}

static inline int ip6_output_finish(struct sk_buff *skb)
{

	struct dst_entry *dst = skb->dst;
	struct hh_cache *hh = dst->hh;

	if (hh) {
		int hh_alen;

		read_lock_bh(&hh->hh_lock);
		hh_alen = HH_DATA_ALIGN(hh->hh_len);
		memcpy(skb->data - hh_alen, hh->hh_data, hh_alen);
		read_unlock_bh(&hh->hh_lock);
	        skb_push(skb, hh->hh_len);
		return hh->hh_output(skb);
	} else if (dst->neighbour)
		return dst->neighbour->output(skb);

	kfree_skb(skb);
	return -EINVAL;

}

/* dev_loopback_xmit for use with netfilter. */
static int ip6_dev_loopback_xmit(struct sk_buff *newskb)
{
	newskb->mac.raw = newskb->data;
	__skb_pull(newskb, newskb->nh.raw - newskb->data);
	newskb->pkt_type = PACKET_LOOPBACK;
	newskb->ip_summed = CHECKSUM_UNNECESSARY;
	BUG_TRAP(newskb->dst);

	netif_rx(newskb);
	return 0;
}


int ip6_output(struct sk_buff *skb)
{
	struct dst_entry *dst = skb->dst;
	struct net_device *dev = dst->dev;

	skb->protocol = htons(ETH_P_IPV6);
	skb->dev = dev;

	if (ipv6_addr_is_multicast(&skb->nh.ipv6h->daddr)) {
		if (!(dev->flags&IFF_LOOPBACK) &&
		    (skb->sk == NULL || skb->sk->net_pinfo.af_inet6.mc_loop) &&
		    ipv6_chk_mcast_addr(dev, &skb->nh.ipv6h->daddr,
				&skb->nh.ipv6h->saddr)) {
			struct sk_buff *newskb = skb_clone(skb, GFP_ATOMIC);

			/* Do not check for IFF_ALLMULTI; multicast routing
			   is not supported in any case.
			 */
			if (newskb)
				NF_HOOK(PF_INET6, NF_IP6_POST_ROUTING, newskb, NULL,
					newskb->dev,
					ip6_dev_loopback_xmit);

			if (skb->nh.ipv6h->hop_limit == 0) {
				kfree_skb(skb);
				return 0;
			}
		}

		IP6_INC_STATS(Ip6OutMcastPkts);
	}

	return NF_HOOK(PF_INET6, NF_IP6_POST_ROUTING, skb,NULL, skb->dev,ip6_output_finish);
}


#ifdef CONFIG_NETFILTER
int ip6_route_me_harder(struct sk_buff *skb)
{
	struct ipv6hdr *iph = skb->nh.ipv6h;
	struct dst_entry *dst;
	struct flowi fl;

	fl.proto = iph->nexthdr;
	fl.fl6_dst = &iph->daddr;
	fl.fl6_src = &iph->saddr;
	fl.oif = skb->sk ? skb->sk->bound_dev_if : 0;
	fl.fl6_flowlabel = 0;
	fl.uli_u.ports.dport = 0;
	fl.uli_u.ports.sport = 0;

	dst = ip6_route_output(skb->sk, &fl);

	if (dst->error) {
		if (net_ratelimit())
			printk(KERN_DEBUG "ip6_route_me_harder: No more route.\n");
		dst_release(dst);
		return -EINVAL;
	}

	/* Drop old route. */
	dst_release(skb->dst);

	skb->dst = dst;
	return 0;
}
#endif

static inline int ip6_maybe_reroute(struct sk_buff *skb)
{
#ifdef CONFIG_NETFILTER
	if (skb->nfcache & NFC_ALTERED){
		if (ip6_route_me_harder(skb) != 0){
			kfree_skb(skb);
			return -EINVAL;
		}
	}
#endif /* CONFIG_NETFILTER */
	return skb->dst->output(skb);
}

/*
 *	xmit an sk_buff (used by TCP)
 */

int ip6_xmit(struct sock *sk, struct sk_buff *skb, struct flowi *fl,
	     struct ipv6_txoptions *opt)
{
	struct ipv6_pinfo * np = sk ? &sk->net_pinfo.af_inet6 : NULL;
	struct in6_addr *first_hop = fl->nl_u.ip6_u.daddr;
	struct dst_entry *dst = skb->dst;
	struct ipv6hdr *hdr;
	u8  proto = fl->proto;
	int seg_len = skb->len;
	int hlimit;

	if (opt) {
		int head_room;

		/* First: exthdrs may take lots of space (~8K for now)
		   MAX_HEADER is not enough.
		 */
		head_room = opt->opt_nflen + opt->opt_flen;
		seg_len += head_room;
		head_room += sizeof(struct ipv6hdr) + ((dst->dev->hard_header_len + 15)&~15);

		if (skb_headroom(skb) < head_room) {
			struct sk_buff *skb2 = skb_realloc_headroom(skb, head_room);
			kfree_skb(skb);
			skb = skb2;
			if (skb == NULL)
				return -ENOBUFS;
			if (sk)
				skb_set_owner_w(skb, sk);
		}
		if (opt->opt_flen)
			ipv6_push_frag_opts(skb, opt, &proto);
		if (opt->opt_nflen)
			ipv6_push_nfrag_opts(skb, opt, &proto, &first_hop);
	}

	hdr = skb->nh.ipv6h = (struct ipv6hdr*)skb_push(skb, sizeof(struct ipv6hdr));

	/*
	 *	Fill in the IPv6 header
	 */

	*(u32*)hdr = htonl(0x60000000) | fl->fl6_flowlabel;
	hlimit = -1;
	if (np)
		hlimit = np->hop_limit;
	if (hlimit < 0)
		hlimit = ((struct rt6_info*)dst)->rt6i_hoplimit;

	hdr->payload_len = htons(seg_len);
	hdr->nexthdr = proto;
	hdr->hop_limit = hlimit;

	ipv6_addr_copy(&hdr->saddr, fl->nl_u.ip6_u.saddr);
	ipv6_addr_copy(&hdr->daddr, first_hop);

	if (skb->len <= dst->pmtu) {
		IP6_INC_STATS(Ip6OutRequests);
		return NF_HOOK(PF_INET6, NF_IP6_LOCAL_OUT, skb, NULL, dst->dev, ip6_maybe_reroute);
	}

	if (net_ratelimit())
		printk(KERN_DEBUG "IPv6: sending pkt_too_big to self\n");
	skb->dev = dst->dev;
	icmpv6_send(skb, ICMPV6_PKT_TOOBIG, 0, dst->pmtu, skb->dev);
	kfree_skb(skb);
	return -EMSGSIZE;
}

/*
 *	To avoid extra problems ND packets are send through this
 *	routine. It's code duplication but I really want to avoid
 *	extra checks since ipv6_build_header is used by TCP (which
 *	is for us performace critical)
 */

int ip6_nd_hdr(struct sock *sk, struct sk_buff *skb, struct net_device *dev,
	       struct in6_addr *saddr, struct in6_addr *daddr,
	       int proto, int len)
{
	struct ipv6_pinfo *np = &sk->net_pinfo.af_inet6;
	struct ipv6hdr *hdr;
	int totlen;

	skb->protocol = htons(ETH_P_IPV6);
	skb->dev = dev;

	totlen = len + sizeof(struct ipv6hdr);

	hdr = (struct ipv6hdr *) skb_put(skb, sizeof(struct ipv6hdr));
	skb->nh.ipv6h = hdr;

	*(u32*)hdr = htonl(0x60000000);

	hdr->payload_len = htons(len);
	hdr->nexthdr = proto;
	hdr->hop_limit = np->hop_limit;

	ipv6_addr_copy(&hdr->saddr, saddr);
	ipv6_addr_copy(&hdr->daddr, daddr);

	return 0;
}

static struct ipv6hdr * ip6_bld_1(struct sock *sk, struct sk_buff *skb, struct flowi *fl,
				  int hlimit, unsigned pktlength)
{
	struct ipv6hdr *hdr;
	
	skb->nh.raw = skb_put(skb, sizeof(struct ipv6hdr));
	hdr = skb->nh.ipv6h;
	
	*(u32*)hdr = fl->fl6_flowlabel | htonl(0x60000000);

	hdr->payload_len = htons(pktlength - sizeof(struct ipv6hdr));
	hdr->hop_limit = hlimit;
	hdr->nexthdr = fl->proto;

	ipv6_addr_copy(&hdr->saddr, fl->nl_u.ip6_u.saddr);
	ipv6_addr_copy(&hdr->daddr, fl->nl_u.ip6_u.daddr);
	return hdr;
}

static __inline__ u8 * ipv6_build_fraghdr(struct sk_buff *skb, u8* prev_hdr, unsigned offset)
{
	struct frag_hdr *fhdr;

	fhdr = (struct frag_hdr *) skb_put(skb, sizeof(struct frag_hdr));

	fhdr->nexthdr  = *prev_hdr;
	*prev_hdr = NEXTHDR_FRAGMENT;
	prev_hdr = &fhdr->nexthdr;

	fhdr->reserved = 0;
	fhdr->frag_off = htons(offset);
	ipv6_select_ident(skb, fhdr);
	return &fhdr->nexthdr;
}

static int ip6_frag_xmit(struct sock *sk, inet_getfrag_t getfrag,
			 const void *data, struct dst_entry *dst,
			 struct flowi *fl, struct ipv6_txoptions *opt,
			 struct in6_addr *final_dst,
			 int hlimit, int flags, unsigned length, int mtu)
{
	struct ipv6hdr *hdr;
	struct sk_buff *last_skb;
	u8 *prev_hdr;
	int unfrag_len;
	int frag_len;
	int last_len;
	int nfrags;
	int fhdr_dist;
	int frag_off;
	int data_off;
	int err;

	/*
	 *	Fragmentation
	 *
	 *	Extension header order:
	 *	Hop-by-hop -> Dest0 -> Routing -> Fragment -> Auth -> Dest1 -> rest (...)
	 *	
	 *	We must build the non-fragmented part that
	 *	will be in every packet... this also means
	 *	that other extension headers (Dest, Auth, etc)
	 *	must be considered in the data to be fragmented
	 */

	unfrag_len = sizeof(struct ipv6hdr) + sizeof(struct frag_hdr);
	last_len = length;

	if (opt) {
		unfrag_len += opt->opt_nflen;
		last_len += opt->opt_flen;
	}

	/*
	 *	Length of fragmented part on every packet but 
	 *	the last must be an:
	 *	"integer multiple of 8 octects".
	 */

	frag_len = (mtu - unfrag_len) & ~0x7;

	/* Unfragmentable part exceeds mtu. */
	if (frag_len <= 0) {
		ipv6_local_error(sk, EMSGSIZE, fl, mtu);
		return -EMSGSIZE;
	}

	nfrags = last_len / frag_len;

	/*
	 *	We must send from end to start because of 
	 *	UDP/ICMP checksums. We do a funny trick:
	 *	fill the last skb first with the fixed
	 *	header (and its data) and then use it
	 *	to create the following segments and send it
	 *	in the end. If the peer is checking the M_flag
	 *	to trigger the reassembly code then this 
	 *	might be a good idea.
	 */

	frag_off = nfrags * frag_len;
	last_len -= frag_off;

	if (last_len == 0) {
		last_len = frag_len;
		frag_off -= frag_len;
		nfrags--;
	}
	data_off = frag_off;

	/* And it is implementation problem: for now we assume, that
	   all the exthdrs will fit to the first fragment.
	 */
	if (opt) {
		if (frag_len < opt->opt_flen) {
			ipv6_local_error(sk, EMSGSIZE, fl, mtu);
			return -EMSGSIZE;
		}
		data_off = frag_off - opt->opt_flen;
	}

	if (flags&MSG_PROBE)
		return 0;

	last_skb = sock_alloc_send_skb(sk, unfrag_len + frag_len +
				       dst->dev->hard_header_len + 15,
				       flags & MSG_DONTWAIT, &err);

	if (last_skb == NULL)
		return err;

	last_skb->dst = dst_clone(dst);

	skb_reserve(last_skb, (dst->dev->hard_header_len + 15) & ~15);

	hdr = ip6_bld_1(sk, last_skb, fl, hlimit, frag_len+unfrag_len);
	prev_hdr = &hdr->nexthdr;

	if (opt && opt->opt_nflen)
		prev_hdr = ipv6_build_nfrag_opts(last_skb, prev_hdr, opt, final_dst, 0);

	prev_hdr = ipv6_build_fraghdr(last_skb, prev_hdr, frag_off);
	fhdr_dist = prev_hdr - last_skb->data;

	err = getfrag(data, &hdr->saddr, last_skb->tail, data_off, last_len);

	if (!err) {
		while (nfrags--) {
			struct sk_buff *skb;
			
			struct frag_hdr *fhdr2;
				
			skb = skb_copy(last_skb, sk->allocation);

			if (skb == NULL) {
				IP6_INC_STATS(Ip6FragFails);
				kfree_skb(last_skb);
				return -ENOMEM;
			}

			frag_off -= frag_len;
			data_off -= frag_len;

			fhdr2 = (struct frag_hdr *) (skb->data + fhdr_dist);

			/* more flag on */
			fhdr2->frag_off = htons(frag_off | 1);

			/* Write fragmentable exthdrs to the first chunk */
			if (nfrags == 0 && opt && opt->opt_flen) {
				ipv6_build_frag_opts(skb, &fhdr2->nexthdr, opt);
				frag_len -= opt->opt_flen;
				data_off = 0;
			}

			err = getfrag(data, &hdr->saddr,skb_put(skb, frag_len),
				      data_off, frag_len);

			if (err) {
				kfree_skb(skb);
				break;
			}

			IP6_INC_STATS(Ip6FragCreates);
			IP6_INC_STATS(Ip6OutRequests);
			err = NF_HOOK(PF_INET6,NF_IP6_LOCAL_OUT, skb, NULL, dst->dev, ip6_maybe_reroute);
			if (err) {
				kfree_skb(last_skb);
				return err;
			}
		}
	}

	if (err) {
		IP6_INC_STATS(Ip6FragFails);
		kfree_skb(last_skb);
		return -EFAULT;
	}

	hdr->payload_len = htons(unfrag_len + last_len - sizeof(struct ipv6hdr));

	/*
	 *	update last_skb to reflect the getfrag we did
	 *	on start.
	 */

	skb_put(last_skb, last_len);

	IP6_INC_STATS(Ip6FragCreates);
	IP6_INC_STATS(Ip6FragOKs);
	IP6_INC_STATS(Ip6OutRequests);
	return NF_HOOK(PF_INET6, NF_IP6_LOCAL_OUT, last_skb, NULL,dst->dev, ip6_maybe_reroute);
}

int ip6_build_xmit(struct sock *sk, inet_getfrag_t getfrag, const void *data,
		   struct flowi *fl, unsigned length,
		   struct ipv6_txoptions *opt, int hlimit, int flags)
{
	struct ipv6_pinfo *np = &sk->net_pinfo.af_inet6;
	struct in6_addr *final_dst = NULL;
	struct dst_entry *dst;
	int err = 0;
	unsigned int pktlength, jumbolen, mtu;
	struct in6_addr saddr;

	if (opt && opt->srcrt) {
		struct rt0_hdr *rt0 = (struct rt0_hdr *) opt->srcrt;
		final_dst = fl->fl6_dst;
		fl->fl6_dst = rt0->addr;
	}

	if (!fl->oif && ipv6_addr_is_multicast(fl->nl_u.ip6_u.daddr))
		fl->oif = np->mcast_oif;

	dst = __sk_dst_check(sk, np->dst_cookie);
	if (dst) {
		struct rt6_info *rt = (struct rt6_info*)dst;

			/* Yes, checking route validity in not connected
			   case is not very simple. Take into account,
			   that we do not support routing by source, TOS,
			   and MSG_DONTROUTE 		--ANK (980726)

			   1. If route was host route, check that
			      cached destination is current.
			      If it is network route, we still may
			      check its validity using saved pointer
			      to the last used address: daddr_cache.
			      We do not want to save whole address now,
			      (because main consumer of this service
			       is tcp, which has not this problem),
			      so that the last trick works only on connected
			      sockets.
			   2. oif also should be the same.
			 */

		if (((rt->rt6i_dst.plen != 128 ||
		      ipv6_addr_cmp(fl->fl6_dst, &rt->rt6i_dst.addr))
		     && (np->daddr_cache == NULL ||
			 ipv6_addr_cmp(fl->fl6_dst, np->daddr_cache)))
		    || (fl->oif && fl->oif != dst->dev->ifindex)) {
			dst = NULL;
		} else
			dst_hold(dst);
	}

	if (dst == NULL)
		dst = ip6_route_output(sk, fl);

	if (dst->error) {
		IP6_INC_STATS(Ip6OutNoRoutes);
		dst_release(dst);
		return -ENETUNREACH;
	}

	if (fl->fl6_src == NULL) {
		err = ipv6_get_saddr(dst, fl->fl6_dst, &saddr);

		if (err) {
#if IP6_DEBUG >= 2
			printk(KERN_DEBUG "ip6_build_xmit: "
			       "no available source address\n");
#endif
			goto out;
		}
		fl->fl6_src = &saddr;
	}
	pktlength = length;

	if (hlimit < 0) {
		if (ipv6_addr_is_multicast(fl->fl6_dst))
			hlimit = np->mcast_hops;
		else
			hlimit = np->hop_limit;
		if (hlimit < 0)
			hlimit = ((struct rt6_info*)dst)->rt6i_hoplimit;
	}

	jumbolen = 0;

	if (!sk->protinfo.af_inet.hdrincl) {
		pktlength += sizeof(struct ipv6hdr);
		if (opt)
			pktlength += opt->opt_flen + opt->opt_nflen;

		if (pktlength > 0xFFFF + sizeof(struct ipv6hdr)) {
			/* Jumbo datagram.
			   It is assumed, that in the case of hdrincl
			   jumbo option is supplied by user.
			 */
			pktlength += 8;
			jumbolen = pktlength - sizeof(struct ipv6hdr);
		}
	}

	mtu = dst->pmtu;
	if (np->frag_size < mtu) {
		if (np->frag_size)
			mtu = np->frag_size;
		else if (np->pmtudisc == IPV6_PMTUDISC_DONT)
			mtu = IPV6_MIN_MTU;
	}

	/* Critical arithmetic overflow check.
	   FIXME: may gcc optimize it out? --ANK (980726)
	 */
	if (pktlength < length) {
		ipv6_local_error(sk, EMSGSIZE, fl, mtu);
		err = -EMSGSIZE;
		goto out;
	}

	if (flags&MSG_CONFIRM)
		dst_confirm(dst);

	if (pktlength <= mtu) {
		struct sk_buff *skb;
		struct ipv6hdr *hdr;
		struct net_device *dev = dst->dev;

		err = 0;
		if (flags&MSG_PROBE)
			goto out;

		skb = sock_alloc_send_skb(sk, pktlength + 15 +
					  dev->hard_header_len,
					  flags & MSG_DONTWAIT, &err);

		if (skb == NULL) {
			IP6_INC_STATS(Ip6OutDiscards);
			goto out;
		}

		skb->dst = dst_clone(dst);

		skb_reserve(skb, (dev->hard_header_len + 15) & ~15);

		hdr = (struct ipv6hdr *) skb->tail;
		skb->nh.ipv6h = hdr;

		if (!sk->protinfo.af_inet.hdrincl) {
			ip6_bld_1(sk, skb, fl, hlimit,
				  jumbolen ? sizeof(struct ipv6hdr) : pktlength);

			if (opt || jumbolen) {
				u8 *prev_hdr = &hdr->nexthdr;
				prev_hdr = ipv6_build_nfrag_opts(skb, prev_hdr, opt, final_dst, jumbolen);
				if (opt && opt->opt_flen)
					ipv6_build_frag_opts(skb, prev_hdr, opt);
			}
		}

		skb_put(skb, length);
		err = getfrag(data, &hdr->saddr,
			      ((char *) hdr) + (pktlength - length),
			      0, length);

		if (!err) {
			IP6_INC_STATS(Ip6OutRequests);
			err = NF_HOOK(PF_INET6, NF_IP6_LOCAL_OUT, skb, NULL, dst->dev, ip6_maybe_reroute);
		} else {
			err = -EFAULT;
			kfree_skb(skb);
		}
	} else {
		if (sk->protinfo.af_inet.hdrincl || jumbolen ||
		    np->pmtudisc == IPV6_PMTUDISC_DO) {
			ipv6_local_error(sk, EMSGSIZE, fl, mtu);
			err = -EMSGSIZE;
			goto out;
		}

		err = ip6_frag_xmit(sk, getfrag, data, dst, fl, opt, final_dst, hlimit,
				    flags, length, mtu);
	}

	/*
	 *	cleanup
	 */
out:
	ip6_dst_store(sk, dst, fl->nl_u.ip6_u.daddr == &np->daddr ? &np->daddr : NULL);
	if (err > 0)
		err = np->recverr ? net_xmit_errno(err) : 0;
	return err;
}

int ip6_call_ra_chain(struct sk_buff *skb, int sel)
{
	struct ip6_ra_chain *ra;
	struct sock *last = NULL;

	read_lock(&ip6_ra_lock);
	for (ra = ip6_ra_chain; ra; ra = ra->next) {
		struct sock *sk = ra->sk;
		if (sk && ra->sel == sel) {
			if (last) {
				struct sk_buff *skb2 = skb_clone(skb, GFP_ATOMIC);
				if (skb2)
					rawv6_rcv(last, skb2);
			}
			last = sk;
		}
	}

	if (last) {
		rawv6_rcv(last, skb);
		read_unlock(&ip6_ra_lock);
		return 1;
	}
	read_unlock(&ip6_ra_lock);
	return 0;
}

static inline int ip6_forward_finish(struct sk_buff *skb)
{
	return skb->dst->output(skb);
}

int ip6_forward(struct sk_buff *skb)
{
	struct dst_entry *dst = skb->dst;
	struct ipv6hdr *hdr = skb->nh.ipv6h;
	struct inet6_skb_parm *opt =(struct inet6_skb_parm*)skb->cb;
	
	if (ipv6_devconf.forwarding == 0)
		goto error;

	skb->ip_summed = CHECKSUM_NONE;

	/*
	 *	We DO NOT make any processing on
	 *	RA packets, pushing them to user level AS IS
	 *	without ane WARRANTY that application will be able
	 *	to interpret them. The reason is that we
	 *	cannot make anything clever here.
	 *
	 *	We are not end-node, so that if packet contains
	 *	AH/ESP, we cannot make anything.
	 *	Defragmentation also would be mistake, RA packets
	 *	cannot be fragmented, because there is no warranty
	 *	that different fragments will go along one path. --ANK
	 */
	if (opt->ra) {
		u8 *ptr = skb->nh.raw + opt->ra;
		if (ip6_call_ra_chain(skb, (ptr[2]<<8) + ptr[3]))
			return 0;
	}

	/*
	 *	check and decrement ttl
	 */
	if (hdr->hop_limit <= 1) {
		/* Force OUTPUT device used as source address */
		skb->dev = dst->dev;
		icmpv6_send(skb, ICMPV6_TIME_EXCEED, ICMPV6_EXC_HOPLIMIT,
			    0, skb->dev);

		kfree_skb(skb);
		return -ETIMEDOUT;
	}

	/* IPv6 specs say nothing about it, but it is clear that we cannot
	   send redirects to source routed frames.
	 */
	if (skb->dev == dst->dev && dst->neighbour && opt->srcrt == 0) {
		struct in6_addr *target = NULL;
		struct rt6_info *rt;
		struct neighbour *n = dst->neighbour;

		/*
		 *	incoming and outgoing devices are the same
		 *	send a redirect.
		 */

		rt = (struct rt6_info *) dst;
		if ((rt->rt6i_flags & RTF_GATEWAY))
			target = (struct in6_addr*)&n->primary_key;
		else
			target = &hdr->daddr;

		/* Limit redirects both by destination (here)
		   and by source (inside ndisc_send_redirect)
		 */
		if (xrlim_allow(dst, 1*HZ))
			ndisc_send_redirect(skb, n, target);
	} else if (ipv6_addr_type(&hdr->saddr)&(IPV6_ADDR_MULTICAST|IPV6_ADDR_LOOPBACK
						|IPV6_ADDR_LINKLOCAL)) {
		/* This check is security critical. */
		goto error;
	}

	if (skb->len > dst->pmtu) {
		/* Again, force OUTPUT device used as source address */
		skb->dev = dst->dev;
		icmpv6_send(skb, ICMPV6_PKT_TOOBIG, 0, dst->pmtu, skb->dev);
		IP6_INC_STATS_BH(Ip6InTooBigErrors);
		kfree_skb(skb);
		return -EMSGSIZE;
	}

	if (skb_cow(skb, dst->dev->hard_header_len))
		goto drop;

	hdr = skb->nh.ipv6h;

	/* Mangling hops number delayed to point after skb COW */
 
	hdr->hop_limit--;

	IP6_INC_STATS_BH(Ip6OutForwDatagrams);
	return NF_HOOK(PF_INET6,NF_IP6_FORWARD, skb, skb->dev, dst->dev, ip6_forward_finish);

error:
	IP6_INC_STATS_BH(Ip6InAddrErrors);
drop:
	kfree_skb(skb);
	return -EINVAL;
}
