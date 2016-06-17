/* Header for use in defining a given protocol for connection tracking. */
#ifndef _IP_CONNTRACK_PROTOCOL_H
#define _IP_CONNTRACK_PROTOCOL_H
#include <linux/netfilter_ipv4/ip_conntrack.h>

struct ip_conntrack_protocol
{
	/* Next pointer. */
	struct list_head list;

	/* Protocol number. */
	u_int8_t proto;

	/* Protocol name */
	const char *name;

	/* Try to fill in the third arg; return true if possible. */
	int (*pkt_to_tuple)(const void *datah, size_t datalen,
			    struct ip_conntrack_tuple *tuple);

	/* Invert the per-proto part of the tuple: ie. turn xmit into reply.
	 * Some packets can't be inverted: return 0 in that case.
	 */
	int (*invert_tuple)(struct ip_conntrack_tuple *inverse,
			    const struct ip_conntrack_tuple *orig);

	/* Print out the per-protocol part of the tuple. */
	unsigned int (*print_tuple)(char *buffer,
				    const struct ip_conntrack_tuple *);

	/* Print out the private part of the conntrack. */
	unsigned int (*print_conntrack)(char *buffer,
					const struct ip_conntrack *);

	/* Returns verdict for packet, or -1 for invalid. */
	int (*packet)(struct ip_conntrack *conntrack,
		      struct iphdr *iph, size_t len,
		      enum ip_conntrack_info ctinfo);

	/* Called when a new connection for this protocol found;
	 * returns TRUE if it's OK.  If so, packet() called next. */
	int (*new)(struct ip_conntrack *conntrack, struct iphdr *iph,
		   size_t len);

	/* Called when a conntrack entry is destroyed */
	void (*destroy)(struct ip_conntrack *conntrack);

	/* Has to decide if a expectation matches one packet or not */
	int (*exp_matches_pkt)(struct ip_conntrack_expect *exp,
			       struct sk_buff **pskb);

	/* Module (if any) which this is connected to. */
	struct module *me;
};

/* Protocol registration. */
extern int ip_conntrack_protocol_register(struct ip_conntrack_protocol *proto);
extern void ip_conntrack_protocol_unregister(struct ip_conntrack_protocol *proto);

/* Existing built-in protocols */
extern struct ip_conntrack_protocol ip_conntrack_protocol_tcp;
extern struct ip_conntrack_protocol ip_conntrack_protocol_udp;
extern struct ip_conntrack_protocol ip_conntrack_protocol_icmp;
extern int ip_conntrack_protocol_tcp_init(void);
#endif /*_IP_CONNTRACK_PROTOCOL_H*/
