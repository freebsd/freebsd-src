/* Kernel module to match one of a list of TCP/UDP ports: ports are in
   the same place so we can treat them as equal. */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/udp.h>
#include <linux/skbuff.h>
#include <linux/in.h>

#include <linux/netfilter_ipv6/ip6t_multiport.h>
#include <linux/netfilter_ipv6/ip6_tables.h>

#if 0
#define duprintf(format, args...) printk(format , ## args)
#else
#define duprintf(format, args...)
#endif

/* Returns 1 if the port is matched by the test, 0 otherwise. */
static inline int
ports_match(const u_int16_t *portlist, enum ip6t_multiport_flags flags,
	    u_int8_t count, u_int16_t src, u_int16_t dst)
{
	unsigned int i;
	for (i=0; i<count; i++) {
		if (flags != IP6T_MULTIPORT_DESTINATION
		    && portlist[i] == src)
			return 1;

		if (flags != IP6T_MULTIPORT_SOURCE
		    && portlist[i] == dst)
			return 1;
	}

	return 0;
}

static int
match(const struct sk_buff *skb,
      const struct net_device *in,
      const struct net_device *out,
      const void *matchinfo,
      int offset,
      const void *hdr,
      u_int16_t datalen,
      int *hotdrop)
{
	const struct udphdr *udp = hdr;
	const struct ip6t_multiport *multiinfo = matchinfo;

	/* Must be big enough to read ports. */
	if (offset == 0 && datalen < sizeof(struct udphdr)) {
		/* We've been asked to examine this packet, and we
		   can't.  Hence, no choice but to drop. */
			duprintf("ip6t_multiport:"
				 " Dropping evil offset=0 tinygram.\n");
			*hotdrop = 1;
			return 0;
	}

	/* Must not be a fragment. */
	return !offset
		&& ports_match(multiinfo->ports,
			       multiinfo->flags, multiinfo->count,
			       ntohs(udp->source), ntohs(udp->dest));
}

/* Called when user tries to insert an entry of this type. */
static int
checkentry(const char *tablename,
	   const struct ip6t_ip6 *ip,
	   void *matchinfo,
	   unsigned int matchsize,
	   unsigned int hook_mask)
{
	const struct ip6t_multiport *multiinfo = matchinfo;

	/* Must specify proto == TCP/UDP, no unknown flags or bad count */
	return (ip->proto == IPPROTO_TCP || ip->proto == IPPROTO_UDP)
		&& !(ip->flags & IP6T_INV_PROTO)
		&& matchsize == IP6T_ALIGN(sizeof(struct ip6t_multiport))
		&& (multiinfo->flags == IP6T_MULTIPORT_SOURCE
		    || multiinfo->flags == IP6T_MULTIPORT_DESTINATION
		    || multiinfo->flags == IP6T_MULTIPORT_EITHER)
		&& multiinfo->count <= IP6T_MULTI_PORTS;
}

static struct ip6t_match multiport_match
= { { NULL, NULL }, "multiport", &match, &checkentry, NULL, THIS_MODULE };

static int __init init(void)
{
	return ip6t_register_match(&multiport_match);
}

static void __exit fini(void)
{
	ip6t_unregister_match(&multiport_match);
}

module_init(init);
module_exit(fini);
