#ifndef _IP6T_REJECT_H
#define _IP6T_REJECT_H

enum ip6t_reject_with {
	IP6T_ICMP_NET_UNREACHABLE,
	IP6T_ICMP_HOST_UNREACHABLE,
	IP6T_ICMP_PROT_UNREACHABLE,
	IP6T_ICMP_PORT_UNREACHABLE,
	IP6T_ICMP_ECHOREPLY
};

struct ip6t_reject_info {
	enum ip6t_reject_with with;      /* reject type */
};

#endif /*_IPT_REJECT_H*/
