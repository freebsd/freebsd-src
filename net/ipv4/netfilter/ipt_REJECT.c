/*
 * This is a module which is used for rejecting packets.
 * Added support for customized reject packets (Jozsef Kadlecsik).
 * Added support for ICMP type-3-code-13 (Maciej Soltysiak). [RFC 1812]
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/icmp.h>
#include <net/icmp.h>
#include <net/ip.h>
#include <net/tcp.h>
#include <net/route.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_REJECT.h>

#if 0
#define DEBUGP printk
#else
#define DEBUGP(format, args...)
#endif

/* If the original packet is part of a connection, but the connection
   is not confirmed, our manufactured reply will not be associated
   with it, so we need to do this manually. */
static void connection_attach(struct sk_buff *new_skb, struct nf_ct_info *nfct)
{
	void (*attach)(struct sk_buff *, struct nf_ct_info *);

	/* Avoid module unload race with ip_ct_attach being NULLed out */
	if (nfct && (attach = ip_ct_attach) != NULL)
		attach(new_skb, nfct);
}

static inline struct rtable *route_reverse(struct sk_buff *skb, int hook)
{
	struct iphdr *iph = skb->nh.iph;
	struct dst_entry *odst;
	struct rt_key key = {};
	struct rtable *rt;

	if (hook != NF_IP_FORWARD) {
		key.dst = iph->saddr;
		if (hook == NF_IP_LOCAL_IN)
			key.src = iph->daddr;
		key.tos = RT_TOS(iph->tos);

		if (ip_route_output_key(&rt, &key) != 0)
			return NULL;
	} else {
		/* non-local src, find valid iif to satisfy
		 * rp-filter when calling ip_route_input. */
		key.dst = iph->daddr;
		if (ip_route_output_key(&rt, &key) != 0)
			return NULL;

		odst = skb->dst;
		if (ip_route_input(skb, iph->saddr, iph->daddr,
		                   RT_TOS(iph->tos), rt->u.dst.dev) != 0) {
			dst_release(&rt->u.dst);
			return NULL;
		}
		dst_release(&rt->u.dst);
		rt = (struct rtable *)skb->dst;
		skb->dst = odst;
	}

	if (rt->u.dst.error) {
		dst_release(&rt->u.dst);
		rt = NULL;
	}

	return rt;
}

/* Send RST reply */
static void send_reset(struct sk_buff *oldskb, int hook)
{
	struct sk_buff *nskb;
	struct tcphdr *otcph, *tcph;
	struct rtable *rt;
	unsigned int otcplen;
	u_int16_t tmp_port;
	u_int32_t tmp_addr;
	int needs_ack;
	int hh_len;

	/* IP header checks: fragment, too short. */
	if (oldskb->nh.iph->frag_off & htons(IP_OFFSET)
	    || oldskb->len < (oldskb->nh.iph->ihl<<2) + sizeof(struct tcphdr))
		return;

	otcph = (struct tcphdr *)((u_int32_t*)oldskb->nh.iph + oldskb->nh.iph->ihl);
	otcplen = oldskb->len - oldskb->nh.iph->ihl*4;

	/* No RST for RST. */
	if (otcph->rst)
		return;

	/* Check checksum. */
	if (tcp_v4_check(otcph, otcplen, oldskb->nh.iph->saddr,
			 oldskb->nh.iph->daddr,
			 csum_partial((char *)otcph, otcplen, 0)) != 0)
		return;

	if ((rt = route_reverse(oldskb, hook)) == NULL)
		return;

	hh_len = (rt->u.dst.dev->hard_header_len + 15)&~15;


	/* Copy skb (even if skb is about to be dropped, we can't just
           clone it because there may be other things, such as tcpdump,
           interested in it). We also need to expand headroom in case
	   hh_len of incoming interface < hh_len of outgoing interface */
	nskb = skb_copy_expand(oldskb, hh_len, skb_tailroom(oldskb),
			       GFP_ATOMIC);
	if (!nskb) {
		dst_release(&rt->u.dst);
		return;
	}

	dst_release(nskb->dst);
	nskb->dst = &rt->u.dst;

	/* This packet will not be the same as the other: clear nf fields */
	nf_conntrack_put(nskb->nfct);
	nskb->nfct = NULL;
	nskb->nfcache = 0;
#ifdef CONFIG_NETFILTER_DEBUG
	nskb->nf_debug = 0;
#endif
	nskb->nfmark = 0;

	tcph = (struct tcphdr *)((u_int32_t*)nskb->nh.iph + nskb->nh.iph->ihl);

	/* Swap source and dest */
	tmp_addr = nskb->nh.iph->saddr;
	nskb->nh.iph->saddr = nskb->nh.iph->daddr;
	nskb->nh.iph->daddr = tmp_addr;
	tmp_port = tcph->source;
	tcph->source = tcph->dest;
	tcph->dest = tmp_port;

	/* Truncate to length (no data) */
	tcph->doff = sizeof(struct tcphdr)/4;
	skb_trim(nskb, nskb->nh.iph->ihl*4 + sizeof(struct tcphdr));
	nskb->nh.iph->tot_len = htons(nskb->len);

	if (tcph->ack) {
		needs_ack = 0;
		tcph->seq = otcph->ack_seq;
		tcph->ack_seq = 0;
	} else {
		needs_ack = 1;
		tcph->ack_seq = htonl(ntohl(otcph->seq) + otcph->syn + otcph->fin
				      + otcplen - (otcph->doff<<2));
		tcph->seq = 0;
	}

	/* Reset flags */
	((u_int8_t *)tcph)[13] = 0;
	tcph->rst = 1;
	tcph->ack = needs_ack;

	tcph->window = 0;
	tcph->urg_ptr = 0;

	/* Adjust TCP checksum */
	tcph->check = 0;
	tcph->check = tcp_v4_check(tcph, sizeof(struct tcphdr),
				   nskb->nh.iph->saddr,
				   nskb->nh.iph->daddr,
				   csum_partial((char *)tcph,
						sizeof(struct tcphdr), 0));

	/* Adjust IP TTL, DF */
	nskb->nh.iph->ttl = MAXTTL;
	/* Set DF, id = 0 */
	nskb->nh.iph->frag_off = htons(IP_DF);
	nskb->nh.iph->id = 0;

	/* Adjust IP checksum */
	nskb->nh.iph->check = 0;
	nskb->nh.iph->check = ip_fast_csum((unsigned char *)nskb->nh.iph, 
					   nskb->nh.iph->ihl);

	/* "Never happens" */
	if (nskb->len > nskb->dst->pmtu)
		goto free_nskb;

	connection_attach(nskb, oldskb->nfct);

	NF_HOOK(PF_INET, NF_IP_LOCAL_OUT, nskb, NULL, nskb->dst->dev,
		ip_finish_output);
	return;

 free_nskb:
	kfree_skb(nskb);
}

static void send_unreach(struct sk_buff *skb_in, int code)
{
	struct iphdr *iph;
	struct udphdr *udph;
	struct icmphdr *icmph;
	struct sk_buff *nskb;
	u32 saddr;
	u8 tos;
	int hh_len, length;
	struct rtable *rt = (struct rtable*)skb_in->dst;
	unsigned char *data;

	if (!rt)
		return;

	/* FIXME: Use sysctl number. --RR */
	if (!xrlim_allow(&rt->u.dst, 1*HZ))
		return;

	iph = skb_in->nh.iph;

	/* No replies to physical multicast/broadcast */
	if (skb_in->pkt_type!=PACKET_HOST)
		return;

	/* Now check at the protocol level */
	if (rt->rt_flags&(RTCF_BROADCAST|RTCF_MULTICAST))
		return;

	/* Only reply to fragment 0. */
	if (iph->frag_off&htons(IP_OFFSET))
		return;

	/* if UDP checksum is set, verify it's correct */
	if (iph->protocol == IPPROTO_UDP
	    && skb_in->tail-(u8*)iph >= sizeof(struct udphdr)) {
		int datalen = skb_in->len - (iph->ihl<<2);
		udph = (struct udphdr *)((char *)iph + (iph->ihl<<2));
		if (udph->check
		    && csum_tcpudp_magic(iph->saddr, iph->daddr,
		                         datalen, IPPROTO_UDP,
		                         csum_partial((char *)udph, datalen,
		                                      0)) != 0)
			return;
	}
		    
	/* If we send an ICMP error to an ICMP error a mess would result.. */
	if (iph->protocol == IPPROTO_ICMP
	    && skb_in->tail-(u8*)iph >= sizeof(struct icmphdr)) {
		icmph = (struct icmphdr *)((char *)iph + (iph->ihl<<2));
		/* Between echo-reply (0) and timestamp (13),
		   everything except echo-request (8) is an error.
		   Also, anything greater than NR_ICMP_TYPES is
		   unknown, and hence should be treated as an error... */
		if ((icmph->type < ICMP_TIMESTAMP
		     && icmph->type != ICMP_ECHOREPLY
		     && icmph->type != ICMP_ECHO)
		    || icmph->type > NR_ICMP_TYPES)
			return;
	}

	saddr = iph->daddr;
	if (!(rt->rt_flags & RTCF_LOCAL))
		saddr = 0;

	tos = (iph->tos & IPTOS_TOS_MASK) | IPTOS_PREC_INTERNETCONTROL;

	if (ip_route_output(&rt, iph->saddr, saddr, RT_TOS(tos), 0))
		return;

	/* RFC says return as much as we can without exceeding 576 bytes. */
	length = skb_in->len + sizeof(struct iphdr) + sizeof(struct icmphdr);

	if (length > rt->u.dst.pmtu)
		length = rt->u.dst.pmtu;
	if (length > 576)
		length = 576;

	hh_len = (rt->u.dst.dev->hard_header_len + 15)&~15;

	nskb = alloc_skb(hh_len+15+length, GFP_ATOMIC);
	if (!nskb) {
		ip_rt_put(rt);
		return;
	}

	nskb->priority = 0;
	nskb->dst = &rt->u.dst;
	skb_reserve(nskb, hh_len);

	/* Set up IP header */
	iph = nskb->nh.iph
		= (struct iphdr *)skb_put(nskb, sizeof(struct iphdr));
	iph->version=4;
	iph->ihl=5;
	iph->tos=tos;
	iph->tot_len = htons(length);

	/* PMTU discovery never applies to ICMP packets. */
	iph->frag_off = 0;

	iph->ttl = MAXTTL;
	ip_select_ident(iph, &rt->u.dst, NULL);
	iph->protocol=IPPROTO_ICMP;
	iph->saddr=rt->rt_src;
	iph->daddr=rt->rt_dst;
	iph->check=0;
	iph->check = ip_fast_csum((unsigned char *)iph, iph->ihl);

	/* Set up ICMP header. */
	icmph = nskb->h.icmph
		= (struct icmphdr *)skb_put(nskb, sizeof(struct icmphdr));
	icmph->type = ICMP_DEST_UNREACH;
	icmph->code = code;	
	icmph->un.gateway = 0;
	icmph->checksum = 0;
	
	/* Copy as much of original packet as will fit */
	data = skb_put(nskb,
		       length - sizeof(struct iphdr) - sizeof(struct icmphdr));
	/* FIXME: won't work with nonlinear skbs --RR */
	memcpy(data, skb_in->nh.iph,
	       length - sizeof(struct iphdr) - sizeof(struct icmphdr));
	icmph->checksum = ip_compute_csum((unsigned char *)icmph,
					  length - sizeof(struct iphdr));

	connection_attach(nskb, skb_in->nfct);

	NF_HOOK(PF_INET, NF_IP_LOCAL_OUT, nskb, NULL, nskb->dst->dev,
		ip_finish_output);
}	

static unsigned int reject(struct sk_buff **pskb,
			   unsigned int hooknum,
			   const struct net_device *in,
			   const struct net_device *out,
			   const void *targinfo,
			   void *userinfo)
{
	const struct ipt_reject_info *reject = targinfo;

	/* Our naive response construction doesn't deal with IP
           options, and probably shouldn't try. */
	if ((*pskb)->nh.iph->ihl<<2 != sizeof(struct iphdr))
		return NF_DROP;

	/* WARNING: This code causes reentry within iptables.
	   This means that the iptables jump stack is now crap.  We
	   must return an absolute verdict. --RR */
    	switch (reject->with) {
    	case IPT_ICMP_NET_UNREACHABLE:
    		send_unreach(*pskb, ICMP_NET_UNREACH);
    		break;
    	case IPT_ICMP_HOST_UNREACHABLE:
    		send_unreach(*pskb, ICMP_HOST_UNREACH);
    		break;
    	case IPT_ICMP_PROT_UNREACHABLE:
    		send_unreach(*pskb, ICMP_PROT_UNREACH);
    		break;
    	case IPT_ICMP_PORT_UNREACHABLE:
    		send_unreach(*pskb, ICMP_PORT_UNREACH);
    		break;
    	case IPT_ICMP_NET_PROHIBITED:
    		send_unreach(*pskb, ICMP_NET_ANO);
    		break;
	case IPT_ICMP_HOST_PROHIBITED:
    		send_unreach(*pskb, ICMP_HOST_ANO);
    		break;
    	case IPT_ICMP_ADMIN_PROHIBITED:
		send_unreach(*pskb, ICMP_PKT_FILTERED);
		break;
	case IPT_TCP_RESET:
		send_reset(*pskb, hooknum);
	case IPT_ICMP_ECHOREPLY:
		/* Doesn't happen. */
		break;
	}

	return NF_DROP;
}

static int check(const char *tablename,
		 const struct ipt_entry *e,
		 void *targinfo,
		 unsigned int targinfosize,
		 unsigned int hook_mask)
{
 	const struct ipt_reject_info *rejinfo = targinfo;

 	if (targinfosize != IPT_ALIGN(sizeof(struct ipt_reject_info))) {
  		DEBUGP("REJECT: targinfosize %u != 0\n", targinfosize);
  		return 0;
  	}

	/* Only allow these for packet filtering. */
	if (strcmp(tablename, "filter") != 0) {
		DEBUGP("REJECT: bad table `%s'.\n", tablename);
		return 0;
	}
	if ((hook_mask & ~((1 << NF_IP_LOCAL_IN)
			   | (1 << NF_IP_FORWARD)
			   | (1 << NF_IP_LOCAL_OUT))) != 0) {
		DEBUGP("REJECT: bad hook mask %X\n", hook_mask);
		return 0;
	}

	if (rejinfo->with == IPT_ICMP_ECHOREPLY) {
		printk("REJECT: ECHOREPLY no longer supported.\n");
		return 0;
	} else if (rejinfo->with == IPT_TCP_RESET) {
		/* Must specify that it's a TCP packet */
		if (e->ip.proto != IPPROTO_TCP
		    || (e->ip.invflags & IPT_INV_PROTO)) {
			DEBUGP("REJECT: TCP_RESET illegal for non-tcp\n");
			return 0;
		}
	}

	return 1;
}

static struct ipt_target ipt_reject_reg
= { { NULL, NULL }, "REJECT", reject, check, NULL, THIS_MODULE };

static int __init init(void)
{
	if (ipt_register_target(&ipt_reject_reg))
		return -EINVAL;
	return 0;
}

static void __exit fini(void)
{
	ipt_unregister_target(&ipt_reject_reg);
}

module_init(init);
module_exit(fini);
MODULE_LICENSE("GPL");
