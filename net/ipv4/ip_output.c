/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		The Internet Protocol (IP) output module.
 *
 * Version:	$Id: ip_output.c,v 1.99.2.1 2002/03/10 04:26:08 davem Exp $
 *
 * Authors:	Ross Biro, <bir7@leland.Stanford.Edu>
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *		Donald Becker, <becker@super.org>
 *		Alan Cox, <Alan.Cox@linux.org>
 *		Richard Underwood
 *		Stefan Becker, <stefanb@yello.ping.de>
 *		Jorge Cwik, <jorge@laser.satlink.net>
 *		Arnt Gulbrandsen, <agulbra@nvg.unit.no>
 *
 *	See ip_input.c for original log
 *
 *	Fixes:
 *		Alan Cox	:	Missing nonblock feature in ip_build_xmit.
 *		Mike Kilburn	:	htons() missing in ip_build_xmit.
 *		Bradford Johnson:	Fix faulty handling of some frames when 
 *					no route is found.
 *		Alexander Demenshin:	Missing sk/skb free in ip_queue_xmit
 *					(in case if packet not accepted by
 *					output firewall rules)
 *		Mike McLagan	:	Routing by source
 *		Alexey Kuznetsov:	use new route cache
 *		Andi Kleen:		Fix broken PMTU recovery and remove
 *					some redundant tests.
 *	Vitaly E. Lavrov	:	Transparent proxy revived after year coma.
 *		Andi Kleen	: 	Replace ip_reply with ip_send_reply.
 *		Andi Kleen	:	Split fast and slow ip_build_xmit path 
 *					for decreased register pressure on x86 
 *					and more readibility. 
 *		Marc Boucher	:	When call_out_firewall returns FW_QUEUE,
 *					silently drop skb instead of failing with -EPERM.
 *		Detlev Wengorz	:	Copy protocol for fragments.
 */

#include <asm/uaccess.h>
#include <asm/system.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/config.h>

#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/proc_fs.h>
#include <linux/stat.h>
#include <linux/init.h>

#include <net/snmp.h>
#include <net/ip.h>
#include <net/protocol.h>
#include <net/route.h>
#include <net/tcp.h>
#include <net/udp.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <net/arp.h>
#include <net/icmp.h>
#include <net/raw.h>
#include <net/checksum.h>
#include <net/inetpeer.h>
#include <linux/igmp.h>
#include <linux/netfilter_ipv4.h>
#include <linux/mroute.h>
#include <linux/netlink.h>

/*
 *      Shall we try to damage output packets if routing dev changes?
 */

int sysctl_ip_dynaddr = 0;
int sysctl_ip_default_ttl = IPDEFTTL;

/* Generate a checksum for an outgoing IP datagram. */
__inline__ void ip_send_check(struct iphdr *iph)
{
	iph->check = 0;
	iph->check = ip_fast_csum((unsigned char *)iph, iph->ihl);
}

/* dev_loopback_xmit for use with netfilter. */
static int ip_dev_loopback_xmit(struct sk_buff *newskb)
{
	newskb->mac.raw = newskb->data;
	__skb_pull(newskb, newskb->nh.raw - newskb->data);
	newskb->pkt_type = PACKET_LOOPBACK;
	newskb->ip_summed = CHECKSUM_UNNECESSARY;
	BUG_TRAP(newskb->dst);

#ifdef CONFIG_NETFILTER_DEBUG
	nf_debug_ip_loopback_xmit(newskb);
#endif
	netif_rx(newskb);
	return 0;
}

/* Don't just hand NF_HOOK skb->dst->output, in case netfilter hook
   changes route */
static inline int
output_maybe_reroute(struct sk_buff *skb)
{
	return skb->dst->output(skb);
}

/* 
 *		Add an ip header to a skbuff and send it out.
 */
int ip_build_and_send_pkt(struct sk_buff *skb, struct sock *sk,
			  u32 saddr, u32 daddr, struct ip_options *opt)
{
	struct rtable *rt = (struct rtable *)skb->dst;
	struct iphdr *iph;

	/* Build the IP header. */
	if (opt)
		iph=(struct iphdr *)skb_push(skb,sizeof(struct iphdr) + opt->optlen);
	else
		iph=(struct iphdr *)skb_push(skb,sizeof(struct iphdr));

	iph->version  = 4;
	iph->ihl      = 5;
	iph->tos      = sk->protinfo.af_inet.tos;
	if (ip_dont_fragment(sk, &rt->u.dst))
		iph->frag_off = htons(IP_DF);
	else
		iph->frag_off = 0;
	iph->ttl      = sk->protinfo.af_inet.ttl;
	iph->daddr    = rt->rt_dst;
	iph->saddr    = rt->rt_src;
	iph->protocol = sk->protocol;
	iph->tot_len  = htons(skb->len);
	ip_select_ident(iph, &rt->u.dst, sk);
	skb->nh.iph   = iph;

	if (opt && opt->optlen) {
		iph->ihl += opt->optlen>>2;
		ip_options_build(skb, opt, daddr, rt, 0);
	}
	ip_send_check(iph);

	/* Send it out. */
	return NF_HOOK(PF_INET, NF_IP_LOCAL_OUT, skb, NULL, rt->u.dst.dev,
		       output_maybe_reroute);
}

static inline int ip_finish_output2(struct sk_buff *skb)
{
	struct dst_entry *dst = skb->dst;
	struct hh_cache *hh = dst->hh;

#ifdef CONFIG_NETFILTER_DEBUG
	nf_debug_ip_finish_output2(skb);
#endif /*CONFIG_NETFILTER_DEBUG*/

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

	if (net_ratelimit())
		printk(KERN_DEBUG "ip_finish_output2: No header cache and no neighbour!\n");
	kfree_skb(skb);
	return -EINVAL;
}

__inline__ int ip_finish_output(struct sk_buff *skb)
{
	struct net_device *dev = skb->dst->dev;

	skb->dev = dev;
	skb->protocol = htons(ETH_P_IP);

	return NF_HOOK(PF_INET, NF_IP_POST_ROUTING, skb, NULL, dev,
		       ip_finish_output2);
}

int ip_mc_output(struct sk_buff *skb)
{
	struct sock *sk = skb->sk;
	struct rtable *rt = (struct rtable*)skb->dst;
	struct net_device *dev = rt->u.dst.dev;

	/*
	 *	If the indicated interface is up and running, send the packet.
	 */
	IP_INC_STATS(IpOutRequests);
#ifdef CONFIG_IP_ROUTE_NAT
	if (rt->rt_flags & RTCF_NAT)
		ip_do_nat(skb);
#endif

	skb->dev = dev;
	skb->protocol = htons(ETH_P_IP);

	/*
	 *	Multicasts are looped back for other local users
	 */

	if (rt->rt_flags&RTCF_MULTICAST) {
		if ((!sk || sk->protinfo.af_inet.mc_loop)
#ifdef CONFIG_IP_MROUTE
		/* Small optimization: do not loopback not local frames,
		   which returned after forwarding; they will be  dropped
		   by ip_mr_input in any case.
		   Note, that local frames are looped back to be delivered
		   to local recipients.

		   This check is duplicated in ip_mr_input at the moment.
		 */
		    && ((rt->rt_flags&RTCF_LOCAL) || !(IPCB(skb)->flags&IPSKB_FORWARDED))
#endif
		) {
			struct sk_buff *newskb = skb_clone(skb, GFP_ATOMIC);
			if (newskb)
				NF_HOOK(PF_INET, NF_IP_POST_ROUTING, newskb, NULL,
					newskb->dev, 
					ip_dev_loopback_xmit);
		}

		/* Multicasts with ttl 0 must not go beyond the host */

		if (skb->nh.iph->ttl == 0) {
			kfree_skb(skb);
			return 0;
		}
	}

	if (rt->rt_flags&RTCF_BROADCAST) {
		struct sk_buff *newskb = skb_clone(skb, GFP_ATOMIC);
		if (newskb)
			NF_HOOK(PF_INET, NF_IP_POST_ROUTING, newskb, NULL,
				newskb->dev, ip_dev_loopback_xmit);
	}

	return ip_finish_output(skb);
}

int ip_output(struct sk_buff *skb)
{
#ifdef CONFIG_IP_ROUTE_NAT
	struct rtable *rt = (struct rtable*)skb->dst;
#endif

	IP_INC_STATS(IpOutRequests);

#ifdef CONFIG_IP_ROUTE_NAT
	if (rt->rt_flags&RTCF_NAT)
		ip_do_nat(skb);
#endif

	return ip_finish_output(skb);
}

/* Queues a packet to be sent, and starts the transmitter if necessary.  
 * This routine also needs to put in the total length and compute the 
 * checksum.  We use to do this in two stages, ip_build_header() then
 * this, but that scheme created a mess when routes disappeared etc.
 * So we do it all here, and the TCP send engine has been changed to
 * match. (No more unroutable FIN disasters, etc. wheee...)  This will
 * most likely make other reliable transport layers above IP easier
 * to implement under Linux.
 */
static inline int ip_queue_xmit2(struct sk_buff *skb)
{
	struct sock *sk = skb->sk;
	struct rtable *rt = (struct rtable *)skb->dst;
	struct net_device *dev;
	struct iphdr *iph = skb->nh.iph;

	dev = rt->u.dst.dev;

	/* This can happen when the transport layer has segments queued
	 * with a cached route, and by the time we get here things are
	 * re-routed to a device with a different MTU than the original
	 * device.  Sick, but we must cover it.
	 */
	if (skb_headroom(skb) < dev->hard_header_len && dev->hard_header) {
		struct sk_buff *skb2;

		skb2 = skb_realloc_headroom(skb, (dev->hard_header_len + 15) & ~15);
		kfree_skb(skb);
		if (skb2 == NULL)
			return -ENOMEM;
		if (sk)
			skb_set_owner_w(skb2, sk);
		skb = skb2;
		iph = skb->nh.iph;
	}

	if (skb->len > rt->u.dst.pmtu)
		goto fragment;

	ip_select_ident(iph, &rt->u.dst, sk);

	/* Add an IP checksum. */
	ip_send_check(iph);

	skb->priority = sk->priority;
	return skb->dst->output(skb);

fragment:
	if (ip_dont_fragment(sk, &rt->u.dst)) {
		/* Reject packet ONLY if TCP might fragment
		 * it itself, if were careful enough.
		 */
		NETDEBUG(printk(KERN_DEBUG "sending pkt_too_big (len[%u] pmtu[%u]) to self\n",
				skb->len, rt->u.dst.pmtu));

		icmp_send(skb, ICMP_DEST_UNREACH, ICMP_FRAG_NEEDED,
			  htonl(rt->u.dst.pmtu));
		kfree_skb(skb);
		return -EMSGSIZE;
	}
	ip_select_ident(iph, &rt->u.dst, sk);
	if (skb->ip_summed == CHECKSUM_HW &&
	    (skb = skb_checksum_help(skb)) == NULL)
		return -ENOMEM;
	return ip_fragment(skb, skb->dst->output);
}

int ip_queue_xmit(struct sk_buff *skb, int ipfragok)
{
	struct sock *sk = skb->sk;
	struct ip_options *opt = sk->protinfo.af_inet.opt;
	struct rtable *rt;
	struct iphdr *iph;

	/* Skip all of this if the packet is already routed,
	 * f.e. by something like SCTP.
	 */
	rt = (struct rtable *) skb->dst;
	if (rt != NULL)
		goto packet_routed;

	/* Make sure we can route this packet. */
	rt = (struct rtable *)__sk_dst_check(sk, 0);
	if (rt == NULL) {
		u32 daddr;

		/* Use correct destination address if we have options. */
		daddr = sk->daddr;
		if(opt && opt->srr)
			daddr = opt->faddr;

		/* If this fails, retransmit mechanism of transport layer will
		 * keep trying until route appears or the connection times itself
		 * out.
		 */
		if (ip_route_output(&rt, daddr, sk->saddr,
				    RT_CONN_FLAGS(sk),
				    sk->bound_dev_if))
			goto no_route;
		__sk_dst_set(sk, &rt->u.dst);
		sk->route_caps = rt->u.dst.dev->features;
	}
	skb->dst = dst_clone(&rt->u.dst);

packet_routed:
	if (opt && opt->is_strictroute && rt->rt_dst != rt->rt_gateway)
		goto no_route;

	/* OK, we know where to send it, allocate and build IP header. */
	iph = (struct iphdr *) skb_push(skb, sizeof(struct iphdr) + (opt ? opt->optlen : 0));
	*((__u16 *)iph)	= htons((4 << 12) | (5 << 8) | (sk->protinfo.af_inet.tos & 0xff));
	iph->tot_len = htons(skb->len);
	if (ip_dont_fragment(sk, &rt->u.dst) && !ipfragok)
		iph->frag_off = htons(IP_DF);
	else
		iph->frag_off = 0;
	iph->ttl      = sk->protinfo.af_inet.ttl;
	iph->protocol = sk->protocol;
	iph->saddr    = rt->rt_src;
	iph->daddr    = rt->rt_dst;
	skb->nh.iph   = iph;
	/* Transport layer set skb->h.foo itself. */

	if(opt && opt->optlen) {
		iph->ihl += opt->optlen >> 2;
		ip_options_build(skb, opt, sk->daddr, rt, 0);
	}

	return NF_HOOK(PF_INET, NF_IP_LOCAL_OUT, skb, NULL, rt->u.dst.dev,
		       ip_queue_xmit2);

no_route:
	IP_INC_STATS(IpOutNoRoutes);
	kfree_skb(skb);
	return -EHOSTUNREACH;
}

/*
 *	Build and send a packet, with as little as one copy
 *
 *	Doesn't care much about ip options... option length can be
 *	different for fragment at 0 and other fragments.
 *
 *	Note that the fragment at the highest offset is sent first,
 *	so the getfrag routine can fill in the TCP/UDP checksum header
 *	field in the last fragment it sends... actually it also helps
 * 	the reassemblers, they can put most packets in at the head of
 *	the fragment queue, and they know the total size in advance. This
 *	last feature will measurably improve the Linux fragment handler one
 *	day.
 *
 *	The callback has five args, an arbitrary pointer (copy of frag),
 *	the source IP address (may depend on the routing table), the 
 *	destination address (char *), the offset to copy from, and the
 *	length to be copied.
 */

static int ip_build_xmit_slow(struct sock *sk,
		  int getfrag (const void *,
			       char *,
			       unsigned int,	
			       unsigned int),
		  const void *frag,
		  unsigned length,
		  struct ipcm_cookie *ipc,
		  struct rtable *rt,
		  int flags)
{
	unsigned int fraglen, maxfraglen, fragheaderlen;
	int err;
	int offset, mf;
	int mtu;
	u16 id;

	int hh_len = (rt->u.dst.dev->hard_header_len + 15)&~15;
	int nfrags=0;
	struct ip_options *opt = ipc->opt;
	int df = 0;

	mtu = rt->u.dst.pmtu;
	if (ip_dont_fragment(sk, &rt->u.dst))
		df = htons(IP_DF);

	length -= sizeof(struct iphdr);

	if (opt) {
		fragheaderlen = sizeof(struct iphdr) + opt->optlen;
		maxfraglen = ((mtu-sizeof(struct iphdr)-opt->optlen) & ~7) + fragheaderlen;
	} else {
		fragheaderlen = sizeof(struct iphdr);

		/*
		 *	Fragheaderlen is the size of 'overhead' on each buffer. Now work
		 *	out the size of the frames to send.
		 */

		maxfraglen = ((mtu-sizeof(struct iphdr)) & ~7) + fragheaderlen;
	}

	if (length + fragheaderlen > 0xFFFF) {
		ip_local_error(sk, EMSGSIZE, rt->rt_dst, sk->dport, mtu);
		return -EMSGSIZE;
	}

	/*
	 *	Start at the end of the frame by handling the remainder.
	 */

	offset = length - (length % (maxfraglen - fragheaderlen));

	/*
	 *	Amount of memory to allocate for final fragment.
	 */

	fraglen = length - offset + fragheaderlen;

	if (length-offset==0) {
		fraglen = maxfraglen;
		offset -= maxfraglen-fragheaderlen;
	}

	/*
	 *	The last fragment will not have MF (more fragments) set.
	 */

	mf = 0;

	/*
	 *	Don't fragment packets for path mtu discovery.
	 */

	if (offset > 0 && sk->protinfo.af_inet.pmtudisc==IP_PMTUDISC_DO) { 
		ip_local_error(sk, EMSGSIZE, rt->rt_dst, sk->dport, mtu);
 		return -EMSGSIZE;
	}
	if (flags&MSG_PROBE)
		goto out;

	/*
	 *	Begin outputting the bytes.
	 */

	id = sk->protinfo.af_inet.id++;

	do {
		char *data;
		struct sk_buff * skb;

		/*
		 *	Get the memory we require with some space left for alignment.
		 */
		if (!(flags & MSG_DONTWAIT) || nfrags == 0) {
			skb = sock_alloc_send_skb(sk, fraglen + hh_len + 15,
						  (flags & MSG_DONTWAIT), &err);
		} else {
			/* On a non-blocking write, we check for send buffer
			 * usage on the first fragment only.
			 */
			skb = sock_wmalloc(sk, fraglen + hh_len + 15, 1,
					   sk->allocation);
			if (!skb)
				err = -ENOBUFS;
		}
		if (skb == NULL)
			goto error;

		/*
		 *	Fill in the control structures
		 */

		skb->priority = sk->priority;
		skb->dst = dst_clone(&rt->u.dst);
		skb_reserve(skb, hh_len);

		/*
		 *	Find where to start putting bytes.
		 */

		data = skb_put(skb, fraglen);
		skb->nh.iph = (struct iphdr *)data;

		/*
		 *	Only write IP header onto non-raw packets 
		 */

		{
			struct iphdr *iph = (struct iphdr *)data;

			iph->version = 4;
			iph->ihl = 5;
			if (opt) {
				iph->ihl += opt->optlen>>2;
				ip_options_build(skb, opt,
						 ipc->addr, rt, offset);
			}
			iph->tos = sk->protinfo.af_inet.tos;
			iph->tot_len = htons(fraglen - fragheaderlen + iph->ihl*4);
			iph->frag_off = htons(offset>>3)|mf|df;
			iph->id = id;
			if (!mf) {
				if (offset || !df) {
					/* Select an unpredictable ident only
					 * for packets without DF or having
					 * been fragmented.
					 */
					__ip_select_ident(iph, &rt->u.dst);
					id = iph->id;
				}

				/*
				 *	Any further fragments will have MF set.
				 */
				mf = htons(IP_MF);
			}
			if (rt->rt_type == RTN_MULTICAST)
				iph->ttl = sk->protinfo.af_inet.mc_ttl;
			else
				iph->ttl = sk->protinfo.af_inet.ttl;
			iph->protocol = sk->protocol;
			iph->check = 0;
			iph->saddr = rt->rt_src;
			iph->daddr = rt->rt_dst;
			iph->check = ip_fast_csum((unsigned char *)iph, iph->ihl);
			data += iph->ihl*4;
		}

		/*
		 *	User data callback
		 */

		if (getfrag(frag, data, offset, fraglen-fragheaderlen)) {
			err = -EFAULT;
			kfree_skb(skb);
			goto error;
		}

		offset -= (maxfraglen-fragheaderlen);
		fraglen = maxfraglen;

		nfrags++;

		err = NF_HOOK(PF_INET, NF_IP_LOCAL_OUT, skb, NULL, 
			      skb->dst->dev, output_maybe_reroute);
		if (err) {
			if (err > 0)
				err = sk->protinfo.af_inet.recverr ? net_xmit_errno(err) : 0;
			if (err)
				goto error;
		}
	} while (offset >= 0);

	if (nfrags>1)
		ip_statistics[smp_processor_id()*2 + !in_softirq()].IpFragCreates += nfrags;
out:
	return 0;

error:
	IP_INC_STATS(IpOutDiscards);
	if (nfrags>1)
		ip_statistics[smp_processor_id()*2 + !in_softirq()].IpFragCreates += nfrags;
	return err; 
}

/*
 *	Fast path for unfragmented packets.
 */
int ip_build_xmit(struct sock *sk, 
		  int getfrag (const void *,
			       char *,
			       unsigned int,	
			       unsigned int),
		  const void *frag,
		  unsigned length,
		  struct ipcm_cookie *ipc,
		  struct rtable *rt,
		  int flags)
{
	int err;
	struct sk_buff *skb;
	int df;
	struct iphdr *iph;

	/*
	 *	Try the simple case first. This leaves fragmented frames, and by
	 *	choice RAW frames within 20 bytes of maximum size(rare) to the long path
	 */

	if (!sk->protinfo.af_inet.hdrincl) {
		length += sizeof(struct iphdr);

		/*
		 * 	Check for slow path.
		 */
		if (length > rt->u.dst.pmtu || ipc->opt != NULL)  
			return ip_build_xmit_slow(sk,getfrag,frag,length,ipc,rt,flags); 
	} else {
		if (length > rt->u.dst.dev->mtu) {
			ip_local_error(sk, EMSGSIZE, rt->rt_dst, sk->dport, rt->u.dst.dev->mtu);
			return -EMSGSIZE;
		}
	}
	if (flags&MSG_PROBE)
		goto out;

	/*
	 *	Do path mtu discovery if needed.
	 */
	df = 0;
	if (ip_dont_fragment(sk, &rt->u.dst))
		df = htons(IP_DF);

	/* 
	 *	Fast path for unfragmented frames without options. 
	 */ 
	{
	int hh_len = (rt->u.dst.dev->hard_header_len + 15)&~15;

	skb = sock_alloc_send_skb(sk, length+hh_len+15,
				  flags&MSG_DONTWAIT, &err);
	if(skb==NULL)
		goto error; 
	skb_reserve(skb, hh_len);
	}

	skb->priority = sk->priority;
	skb->dst = dst_clone(&rt->u.dst);

	skb->nh.iph = iph = (struct iphdr *)skb_put(skb, length);

	if(!sk->protinfo.af_inet.hdrincl) {
		iph->version=4;
		iph->ihl=5;
		iph->tos=sk->protinfo.af_inet.tos;
		iph->tot_len = htons(length);
		iph->frag_off = df;
		iph->ttl=sk->protinfo.af_inet.mc_ttl;
		ip_select_ident(iph, &rt->u.dst, sk);
		if (rt->rt_type != RTN_MULTICAST)
			iph->ttl=sk->protinfo.af_inet.ttl;
		iph->protocol=sk->protocol;
		iph->saddr=rt->rt_src;
		iph->daddr=rt->rt_dst;
		iph->check=0;
		iph->check = ip_fast_csum((unsigned char *)iph, iph->ihl);
		err = getfrag(frag, ((char *)iph)+iph->ihl*4,0, length-iph->ihl*4);
	}
	else
		err = getfrag(frag, (void *)iph, 0, length);

	if (err)
		goto error_fault;

	err = NF_HOOK(PF_INET, NF_IP_LOCAL_OUT, skb, NULL, rt->u.dst.dev,
		      output_maybe_reroute);
	if (err > 0)
		err = sk->protinfo.af_inet.recverr ? net_xmit_errno(err) : 0;
	if (err)
		goto error;
out:
	return 0;

error_fault:
	err = -EFAULT;
	kfree_skb(skb);
error:
	IP_INC_STATS(IpOutDiscards);
	return err; 
}

/*
 *	This IP datagram is too large to be sent in one piece.  Break it up into
 *	smaller pieces (each of size equal to IP header plus
 *	a block of the data of the original IP data part) that will yet fit in a
 *	single device frame, and queue such a frame for sending.
 *
 *	Yes this is inefficient, feel free to submit a quicker one.
 */

int ip_fragment(struct sk_buff *skb, int (*output)(struct sk_buff*))
{
	struct iphdr *iph;
	int raw = 0;
	int ptr;
	struct net_device *dev;
	struct sk_buff *skb2;
	unsigned int mtu, hlen, left, len; 
	int offset;
	int not_last_frag;
	struct rtable *rt = (struct rtable*)skb->dst;
	int err = 0;

	dev = rt->u.dst.dev;

	/*
	 *	Point into the IP datagram header.
	 */

	iph = skb->nh.iph;

	/*
	 *	Setup starting values.
	 */

	hlen = iph->ihl * 4;
	left = skb->len - hlen;		/* Space per frame */
	mtu = rt->u.dst.pmtu - hlen;	/* Size of data space */
	ptr = raw + hlen;		/* Where to start from */

	/*
	 *	Fragment the datagram.
	 */

	offset = (ntohs(iph->frag_off) & IP_OFFSET) << 3;
	not_last_frag = iph->frag_off & htons(IP_MF);

	/*
	 *	Keep copying data until we run out.
	 */

	while(left > 0)	{
		len = left;
		/* IF: it doesn't fit, use 'mtu' - the data space left */
		if (len > mtu)
			len = mtu;
		/* IF: we are not sending upto and including the packet end
		   then align the next start on an eight byte boundary */
		if (len < left)	{
			len &= ~7;
		}
		/*
		 *	Allocate buffer.
		 */

		if ((skb2 = alloc_skb(len+hlen+dev->hard_header_len+15,GFP_ATOMIC)) == NULL) {
			NETDEBUG(printk(KERN_INFO "IP: frag: no memory for new fragment!\n"));
			err = -ENOMEM;
			goto fail;
		}

		/*
		 *	Set up data on packet
		 */

		skb2->pkt_type = skb->pkt_type;
		skb2->priority = skb->priority;
		skb_reserve(skb2, (dev->hard_header_len+15)&~15);
		skb_put(skb2, len + hlen);
		skb2->nh.raw = skb2->data;
		skb2->h.raw = skb2->data + hlen;
		skb2->protocol = skb->protocol;
		skb2->security = skb->security;

		/*
		 *	Charge the memory for the fragment to any owner
		 *	it might possess
		 */

		if (skb->sk)
			skb_set_owner_w(skb2, skb->sk);
		skb2->dst = dst_clone(skb->dst);
		skb2->dev = skb->dev;

		/*
		 *	Copy the packet header into the new buffer.
		 */

		memcpy(skb2->nh.raw, skb->data, hlen);

		/*
		 *	Copy a block of the IP datagram.
		 */
		if (skb_copy_bits(skb, ptr, skb2->h.raw, len))
			BUG();
		left -= len;

		/*
		 *	Fill in the new header fields.
		 */
		iph = skb2->nh.iph;
		iph->frag_off = htons((offset >> 3));

		/* ANK: dirty, but effective trick. Upgrade options only if
		 * the segment to be fragmented was THE FIRST (otherwise,
		 * options are already fixed) and make it ONCE
		 * on the initial skb, so that all the following fragments
		 * will inherit fixed options.
		 */
		if (offset == 0)
			ip_options_fragment(skb);

		/* Copy the flags to each fragment. */
		IPCB(skb2)->flags = IPCB(skb)->flags;

		/*
		 *	Added AC : If we are fragmenting a fragment that's not the
		 *		   last fragment then keep MF on each bit
		 */
		if (left > 0 || not_last_frag)
			iph->frag_off |= htons(IP_MF);
		ptr += len;
		offset += len;

#ifdef CONFIG_NET_SCHED
		skb2->tc_index = skb->tc_index;
#endif
#ifdef CONFIG_NETFILTER
		skb2->nfmark = skb->nfmark;
		skb2->nfcache = skb->nfcache;
		/* Connection association is same as pre-frag packet */
		skb2->nfct = skb->nfct;
		nf_conntrack_get(skb2->nfct);
#ifdef CONFIG_NETFILTER_DEBUG
		skb2->nf_debug = skb->nf_debug;
#endif
#endif

		/*
		 *	Put this fragment into the sending queue.
		 */

		IP_INC_STATS(IpFragCreates);

		iph->tot_len = htons(len + hlen);

		ip_send_check(iph);

		err = output(skb2);
		if (err)
			goto fail;
	}
	kfree_skb(skb);
	IP_INC_STATS(IpFragOKs);
	return err;

fail:
	kfree_skb(skb); 
	IP_INC_STATS(IpFragFails);
	return err;
}

/*
 *	Fetch data from kernel space and fill in checksum if needed.
 */
static int ip_reply_glue_bits(const void *dptr, char *to, unsigned int offset, 
			      unsigned int fraglen)
{
        struct ip_reply_arg *dp = (struct ip_reply_arg*)dptr;
	u16 *pktp = (u16 *)to;
	struct iovec *iov; 
	int len; 
	int hdrflag = 1; 

	iov = &dp->iov[0]; 
	if (offset >= iov->iov_len) { 
		offset -= iov->iov_len;
		iov++; 
		hdrflag = 0; 
	}
	len = iov->iov_len - offset;
	if (fraglen > len) { /* overlapping. */ 
		dp->csum = csum_partial_copy_nocheck(iov->iov_base+offset, to, len,
					     dp->csum);
		offset = 0;
		fraglen -= len; 
		to += len; 
		iov++;
	}

	dp->csum = csum_partial_copy_nocheck(iov->iov_base+offset, to, fraglen, 
					     dp->csum); 

	if (hdrflag && dp->csumoffset)
		*(pktp + dp->csumoffset) = csum_fold(dp->csum); /* fill in checksum */
	return 0;	       
}

/* 
 *	Generic function to send a packet as reply to another packet.
 *	Used to send TCP resets so far. ICMP should use this function too.
 *
 *	Should run single threaded per socket because it uses the sock 
 *     	structure to pass arguments.
 */
void ip_send_reply(struct sock *sk, struct sk_buff *skb, struct ip_reply_arg *arg,
		   unsigned int len)
{
	struct {
		struct ip_options	opt;
		char			data[40];
	} replyopts;
	struct ipcm_cookie ipc;
	u32 daddr;
	struct rtable *rt = (struct rtable*)skb->dst;

	if (ip_options_echo(&replyopts.opt, skb))
		return;

	daddr = ipc.addr = rt->rt_src;
	ipc.opt = NULL;

	if (replyopts.opt.optlen) {
		ipc.opt = &replyopts.opt;

		if (ipc.opt->srr)
			daddr = replyopts.opt.faddr;
	}

	if (ip_route_output(&rt, daddr, rt->rt_spec_dst, RT_TOS(skb->nh.iph->tos), 0))
		return;

	/* And let IP do all the hard work.

	   This chunk is not reenterable, hence spinlock.
	   Note that it uses the fact, that this function is called
	   with locally disabled BH and that sk cannot be already spinlocked.
	 */
	bh_lock_sock(sk);
	sk->protinfo.af_inet.tos = skb->nh.iph->tos;
	sk->priority = skb->priority;
	sk->protocol = skb->nh.iph->protocol;
	ip_build_xmit(sk, ip_reply_glue_bits, arg, len, &ipc, rt, MSG_DONTWAIT);
	bh_unlock_sock(sk);

	ip_rt_put(rt);
}

/*
 *	IP protocol layer initialiser
 */

static struct packet_type ip_packet_type =
{
	__constant_htons(ETH_P_IP),
	NULL,	/* All devices */
	ip_rcv,
	(void*)1,
	NULL,
};

/*
 *	IP registers the packet type and then calls the subprotocol initialisers
 */

void __init ip_init(void)
{
	dev_add_pack(&ip_packet_type);

	ip_rt_init();
	inet_initpeers();

#ifdef CONFIG_IP_MULTICAST
	proc_net_create("igmp", 0, ip_mc_procinfo);
#endif
	proc_net_create("mcfilter", 0, ip_mcf_procinfo);
}
