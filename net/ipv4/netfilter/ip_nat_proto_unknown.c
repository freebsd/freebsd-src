/* The "unknown" protocol.  This is what is used for protocols we
 * don't understand.  It's returned by ip_ct_find_proto().
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/netfilter.h>
#include <linux/if.h>

#include <linux/netfilter_ipv4/ip_nat.h>
#include <linux/netfilter_ipv4/ip_nat_rule.h>
#include <linux/netfilter_ipv4/ip_nat_protocol.h>

static int unknown_in_range(const struct ip_conntrack_tuple *tuple,
			    enum ip_nat_manip_type manip_type,
			    const union ip_conntrack_manip_proto *min,
			    const union ip_conntrack_manip_proto *max)
{
	return 1;
}

static int unknown_unique_tuple(struct ip_conntrack_tuple *tuple,
				const struct ip_nat_range *range,
				enum ip_nat_manip_type maniptype,
				const struct ip_conntrack *conntrack)
{
	/* Sorry: we can't help you; if it's not unique, we can't frob
	   anything. */
	return 0;
}

static void
unknown_manip_pkt(struct iphdr *iph, size_t len,
		  const struct ip_conntrack_manip *manip,
		  enum ip_nat_manip_type maniptype)
{
	return;
}

static unsigned int
unknown_print(char *buffer,
	      const struct ip_conntrack_tuple *match,
	      const struct ip_conntrack_tuple *mask)
{
	return 0;
}

static unsigned int
unknown_print_range(char *buffer, const struct ip_nat_range *range)
{
	return 0;
}

struct ip_nat_protocol unknown_nat_protocol = {
	{ NULL, NULL }, "unknown", 0,
	unknown_manip_pkt,
	unknown_in_range,
	unknown_unique_tuple,
	unknown_print,
	unknown_print_range
};
