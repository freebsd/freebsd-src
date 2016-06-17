/*
 *	IPv6 Firewall
 *	Linux INET6 implementation
 *
 *	Authors:
 *	Pedro Roque		<roque@di.fc.ul.pt>	
 *
 *	$Id: ip6_fw.c,v 1.16 2001/10/31 08:17:58 davem Exp $
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/net.h>
#include <linux/route.h>
#include <linux/netdevice.h>
#include <linux/in6.h>
#include <linux/udp.h>
#include <linux/init.h>

#include <net/ipv6.h>
#include <net/ip6_route.h>
#include <net/ip6_fw.h>
#include <net/netlink.h>

static unsigned long ip6_fw_rule_cnt;
static struct ip6_fw_rule ip6_fw_rule_list = {
	{0},
	NULL, NULL,
	{0},
	IP6_FW_REJECT
};

static int ip6_fw_accept(struct dst_entry *dst, struct fl_acc_args *args);

struct flow_rule_ops ip6_fw_ops = {
	ip6_fw_accept
};


static struct rt6_info ip6_fw_null_entry = {
	{{NULL, 0, 0, NULL,
	  0, 0, 0, 0, 0, 0, 0, 0, -ENETUNREACH, NULL, NULL,
	  ip6_pkt_discard, ip6_pkt_discard, NULL}},
	NULL, {{{0}}}, 256, RTF_REJECT|RTF_NONEXTHOP, ~0UL,
	0, &ip6_fw_rule_list, {{{{0}}}, 128}, {{{{0}}}, 128}
};

static struct fib6_node ip6_fw_fib = {
	NULL, NULL, NULL, NULL,
	&ip6_fw_null_entry,
	0, RTN_ROOT|RTN_TL_ROOT, 0
};

rwlock_t ip6_fw_lock = RW_LOCK_UNLOCKED;


static void ip6_rule_add(struct ip6_fw_rule *rl)
{
	struct ip6_fw_rule *next;

	write_lock_bh(&ip6_fw_lock);
	ip6_fw_rule_cnt++;
	next = &ip6_fw_rule_list;
	rl->next = next;
	rl->prev = next->prev;
	rl->prev->next = rl;
	next->prev = rl;
	write_unlock_bh(&ip6_fw_lock);
}

static void ip6_rule_del(struct ip6_fw_rule *rl)
{
	struct ip6_fw_rule *next, *prev;

	write_lock_bh(&ip6_fw_lock);
	ip6_fw_rule_cnt--;
	next = rl->next;
	prev = rl->prev;
	next->prev = prev;
	prev->next = next;
	write_unlock_bh(&ip6_fw_lock);
}

static __inline__ struct ip6_fw_rule * ip6_fwrule_alloc(void)
{
	struct ip6_fw_rule *rl;

	rl = kmalloc(sizeof(struct ip6_fw_rule), GFP_ATOMIC);
	if (rl)
	{
		memset(rl, 0, sizeof(struct ip6_fw_rule));
		rl->flowr.ops = &ip6_fw_ops;
	}
	return rl;
}

static __inline__ void ip6_fwrule_free(struct ip6_fw_rule * rl)
{
	kfree(rl);
}

static __inline__ int port_match(int rl_port, int fl_port)
{
	int res = 0;
	if (rl_port == 0 || (rl_port == fl_port))
		res = 1;
	return res;
}

static int ip6_fw_accept_trans(struct ip6_fw_rule *rl,
			       struct fl_acc_args *args)
{
	int res = FLOWR_NODECISION;
	int proto = 0;
	int sport = 0;
	int dport = 0;

	switch (args->type) {
	case FL_ARG_FORWARD:
	{
		struct sk_buff *skb = args->fl_u.skb;
		struct ipv6hdr *hdr = skb->nh.ipv6h;
		int len;

		len = skb->len - sizeof(struct ipv6hdr);

		proto = hdr->nexthdr;

		switch (proto) {
		case IPPROTO_TCP:
		{
			struct tcphdr *th;

			if (len < sizeof(struct tcphdr)) {
				res = FLOWR_ERROR;
				goto out;
			}
			th = (struct tcphdr *)(hdr + 1);
			sport = th->source;
			dport = th->dest;
			break;
		}
		case IPPROTO_UDP:
		{
			struct udphdr *uh;

			if (len < sizeof(struct udphdr)) {
				res = FLOWR_ERROR;
				goto out;
			}
			uh = (struct udphdr *)(hdr + 1);
			sport = uh->source;
			dport = uh->dest;
			break;
		}
		default:
			goto out;
		};
		break;
	}

	case FL_ARG_ORIGIN:
	{
		proto = args->fl_u.fl_o.flow->proto;

		if (proto == IPPROTO_ICMPV6) {
			goto out;
		} else {
			sport = args->fl_u.fl_o.flow->uli_u.ports.sport;
			dport = args->fl_u.fl_o.flow->uli_u.ports.dport;
		}
		break;
	}

	if (proto == rl->info.proto &&
	    port_match(args->fl_u.fl_o.flow->uli_u.ports.sport, sport) &&
	    port_match(args->fl_u.fl_o.flow->uli_u.ports.dport, dport)) {
		if (rl->policy & IP6_FW_REJECT)
			res = FLOWR_SELECT;
		else
			res = FLOWR_CLEAR;
	}

	default:
#if IP6_FW_DEBUG >= 1
		printk(KERN_DEBUG "ip6_fw_accept: unknown arg type\n");
#endif
		goto out;
	};

out:
	return res;
}

static int ip6_fw_accept(struct dst_entry *dst, struct fl_acc_args *args)
{
	struct rt6_info *rt;
	struct ip6_fw_rule *rl;
	int proto;
	int res = FLOWR_NODECISION;

	rt = (struct rt6_info *) dst;
	rl = (struct ip6_fw_rule *) rt->rt6i_flowr;

	proto = rl->info.proto;

	switch (proto) {
	case 0:
		if (rl->policy & IP6_FW_REJECT)
			res = FLOWR_SELECT;
		else
			res = FLOWR_CLEAR;
		break;
	case IPPROTO_TCP:
	case IPPROTO_UDP:
		res = ip6_fw_accept_trans(rl, args);
		break;
	case IPPROTO_ICMPV6:
	};

	return res;
}

static struct dst_entry * ip6_fw_dup(struct dst_entry *frule,
				     struct dst_entry *rt,
				     struct fl_acc_args *args)
{
	struct ip6_fw_rule *rl;
	struct rt6_info *nrt;
	struct rt6_info *frt;

	frt = (struct rt6_info *) frule;

	rl = (struct ip6_fw_rule *) frt->rt6i_flowr;

	nrt = ip6_rt_copy((struct rt6_info *) rt);

	if (nrt) {
		nrt->u.dst.input = frule->input;
		nrt->u.dst.output = frule->output;

		nrt->rt6i_flowr = flow_clone(frt->rt6i_flowr);

		nrt->rt6i_flags |= RTF_CACHE;
		nrt->rt6i_tstamp = jiffies;
	}

	return (struct dst_entry *) nrt;
}

int ip6_fw_reject(struct sk_buff *skb)
{
#if IP6_FW_DEBUG >= 1
	printk(KERN_DEBUG "packet rejected: \n");
#endif

	icmpv6_send(skb, ICMPV6_DEST_UNREACH, ICMPV6_ADM_PROHIBITED, 0,
		    skb->dev);
	/*
	 *	send it via netlink, as (rule, skb)
	 */

	kfree_skb(skb);
	return 0;
}

int ip6_fw_discard(struct sk_buff *skb)
{
	printk(KERN_DEBUG "ip6_fw: BUG fw_reject called\n");
	kfree_skb(skb);
	return 0;
}

int ip6_fw_msg_add(struct ip6_fw_msg *msg)
{
	struct in6_rtmsg rtmsg;
	struct ip6_fw_rule *rl;
	struct rt6_info *rt;
	int err;

	ipv6_addr_copy(&rtmsg.rtmsg_dst, &msg->dst);
	ipv6_addr_copy(&rtmsg.rtmsg_src, &msg->src);
	rtmsg.rtmsg_dst_len = msg->dst_len;
	rtmsg.rtmsg_src_len = msg->src_len;
	rtmsg.rtmsg_metric = IP6_RT_PRIO_FW;

	rl = ip6_fwrule_alloc();

	if (rl == NULL)
		return -ENOMEM;

	rl->policy = msg->policy;
	rl->info.proto = msg->proto;
	rl->info.uli_u.data = msg->u.data;

	rtmsg.rtmsg_flags = RTF_NONEXTHOP|RTF_POLICY;
	err = ip6_route_add(&rtmsg);

	if (err) {
		ip6_fwrule_free(rl);
		return err;
	}

	/* The rest will not work for now. --ABK (989725) */

#ifndef notdef
	ip6_fwrule_free(rl);
	return -EPERM;
#else
	rt->u.dst.error = -EPERM;

	if (msg->policy == IP6_FW_ACCEPT) {
		/*
		 *	Accept rules are never selected
		 *	(i.e. packets use normal forwarding)
		 */
		rt->u.dst.input = ip6_fw_discard;
		rt->u.dst.output = ip6_fw_discard;
	} else {
		rt->u.dst.input = ip6_fw_reject;
		rt->u.dst.output = ip6_fw_reject;
	}

	ip6_rule_add(rl);

	rt->rt6i_flowr = flow_clone((struct flow_rule *)rl);

	return 0;
#endif
}

static int ip6_fw_msgrcv(int unit, struct sk_buff *skb)
{
	int count = 0;

	while (skb->len) {
		struct ip6_fw_msg *msg;

		if (skb->len < sizeof(struct ip6_fw_msg)) {
			count = -EINVAL;
			break;
		}

		msg = (struct ip6_fw_msg *) skb->data;
		skb_pull(skb, sizeof(struct ip6_fw_msg));
		count += sizeof(struct ip6_fw_msg);

		switch (msg->action) {
		case IP6_FW_MSG_ADD:
			ip6_fw_msg_add(msg);
			break;
		case IP6_FW_MSG_DEL:
			break;
		default:
			return -EINVAL;
		};
	}

	return count;
}

static void ip6_fw_destroy(struct flow_rule *rl)
{
	ip6_fwrule_free((struct ip6_fw_rule *)rl);
}

#ifdef MODULE
#define ip6_fw_init module_init
#endif

void __init ip6_fw_init(void)
{
	netlink_attach(NETLINK_IP6_FW, ip6_fw_msgrcv);
}

#ifdef MODULE
void cleanup_module(void)
{
	netlink_detach(NETLINK_IP6_FW);
}
#endif
