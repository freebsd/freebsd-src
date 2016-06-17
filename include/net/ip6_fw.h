#ifndef __NET_IP6_FW_H
#define __NET_IP6_FW_H

#define IP6_FW_LISTHEAD		0x1000
#define IP6_FW_ACCEPT		0x0001
#define IP6_FW_REJECT		0x0002

#define IP6_FW_DEBUG	2

#define IP6_FW_MSG_ADD		1
#define IP6_FW_MSG_DEL		2
#define IP6_FW_MSG_REPORT	3


/*
 *	Fast "hack" user interface
 */
struct ip6_fw_msg {
	struct in6_addr		dst;
	struct in6_addr		src;
	int			dst_len;
	int			src_len;
	int			action;
	int			policy;
	int			proto;
	union {
		struct {
			__u16	sport;
			__u16	dport;
		} transp;

		unsigned long	data;

		int		icmp_type;
	} u;

	int			msg_len;
};

#ifdef __KERNEL__

#include <net/flow.h>

struct ip6_fw_rule {
	struct flow_rule	flowr;
	struct ip6_fw_rule	*next;
	struct ip6_fw_rule	*prev;
	struct flowi		info;
	unsigned long		policy;
};

#endif

#endif
