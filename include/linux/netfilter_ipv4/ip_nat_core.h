#ifndef _IP_NAT_CORE_H
#define _IP_NAT_CORE_H
#include <linux/list.h>
#include <linux/netfilter_ipv4/ip_conntrack.h>

/* This header used to share core functionality between the standalone
   NAT module, and the compatibility layer's use of NAT for masquerading. */
extern int ip_nat_init(void);
extern void ip_nat_cleanup(void);

extern unsigned int do_bindings(struct ip_conntrack *ct,
				enum ip_conntrack_info conntrackinfo,
				struct ip_nat_info *info,
				unsigned int hooknum,
				struct sk_buff **pskb);

extern struct list_head protos;

extern unsigned int icmp_reply_translation(struct sk_buff *skb,
					   struct ip_conntrack *conntrack,
					   unsigned int hooknum,
					   int dir);

extern void replace_in_hashes(struct ip_conntrack *conntrack,
			      struct ip_nat_info *info);
extern void place_in_hashes(struct ip_conntrack *conntrack,
			    struct ip_nat_info *info);

/* Built-in protocols. */
extern struct ip_nat_protocol ip_nat_protocol_tcp;
extern struct ip_nat_protocol ip_nat_protocol_udp;
extern struct ip_nat_protocol ip_nat_protocol_icmp;
#endif /* _IP_NAT_CORE_H */
