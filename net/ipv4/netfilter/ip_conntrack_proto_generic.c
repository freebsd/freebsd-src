#include <linux/types.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4/ip_conntrack_protocol.h>

unsigned long ip_ct_generic_timeout = 600*HZ;

static int generic_pkt_to_tuple(const void *datah, size_t datalen,
				struct ip_conntrack_tuple *tuple)
{
	tuple->src.u.all = 0;
	tuple->dst.u.all = 0;

	return 1;
}

static int generic_invert_tuple(struct ip_conntrack_tuple *tuple,
				const struct ip_conntrack_tuple *orig)
{
	tuple->src.u.all = 0;
	tuple->dst.u.all = 0;

	return 1;
}

/* Print out the per-protocol part of the tuple. */
static unsigned int generic_print_tuple(char *buffer,
					const struct ip_conntrack_tuple *tuple)
{
	return 0;
}

/* Print out the private part of the conntrack. */
static unsigned int generic_print_conntrack(char *buffer,
					    const struct ip_conntrack *state)
{
	return 0;
}

/* Returns verdict for packet, or -1 for invalid. */
static int established(struct ip_conntrack *conntrack,
		       struct iphdr *iph, size_t len,
		       enum ip_conntrack_info conntrackinfo)
{
	ip_ct_refresh(conntrack, ip_ct_generic_timeout);
	return NF_ACCEPT;
}

/* Called when a new connection for this protocol found. */
static int
new(struct ip_conntrack *conntrack, struct iphdr *iph, size_t len)
{
	return 1;
}

struct ip_conntrack_protocol ip_conntrack_generic_protocol
= { { NULL, NULL }, 0, "unknown",
    generic_pkt_to_tuple, generic_invert_tuple, generic_print_tuple,
    generic_print_conntrack, established, new, NULL, NULL, NULL };

