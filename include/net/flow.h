/*
 *
 *	Flow based forwarding rules (usage: firewalling, etc)
 *
 */

#ifndef _NET_FLOW_H
#define _NET_FLOW_H

struct flowi {
	int	proto;		/*	{TCP, UDP, ICMP}	*/

	union {
		struct {
			__u32			daddr;
			__u32			saddr;
		} ip4_u;
		
		struct {
			struct in6_addr *	daddr;
			struct in6_addr *	saddr;
			__u32			flowlabel;
		} ip6_u;
	} nl_u;
#define fl6_dst		nl_u.ip6_u.daddr
#define fl6_src		nl_u.ip6_u.saddr
#define fl6_flowlabel	nl_u.ip6_u.flowlabel
#define fl4_dst		nl_u.ip4_u.daddr
#define fl4_src		nl_u.ip4_u.saddr

	int	oif;

	union {
		struct {
			__u16	sport;
			__u16	dport;
		} ports;

		struct {
			__u8	type;
			__u8	code;
		} icmpt;

		unsigned long	data;
	} uli_u;
};

#define FLOWR_NODECISION	0	/* rule not appliable to flow	*/
#define FLOWR_SELECT		1	/* flow must follow this rule	*/
#define FLOWR_CLEAR		2	/* priority level clears flow	*/
#define FLOWR_ERROR		3

struct fl_acc_args {
	int	type;


#define FL_ARG_FORWARD	1
#define FL_ARG_ORIGIN	2

	union {
		struct sk_buff		*skb;
		struct {
			struct sock	*sk;
			struct flowi	*flow;
		} fl_o;
	} fl_u;
};


struct pkt_filter {
	atomic_t		refcnt;
	unsigned int		offset;
	__u32			value;
	__u32			mask;
	struct pkt_filter	*next;
};

#define FLR_INPUT		1
#define FLR_OUTPUT		2

struct flow_filter {
	int				type;
	union {
		struct pkt_filter	*filter;
		struct sock		*sk;
	} u;
};

struct flow_rule {
	struct flow_rule_ops		*ops;
	unsigned char			private[0];
};

struct flow_rule_ops {
	int			(*accept)(struct rt6_info *rt,
					  struct rt6_info *rule,
					  struct fl_acc_args *args,
					  struct rt6_info **nrt);
};

#endif
