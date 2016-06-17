/* Kernel module to match connection tracking information.
 * GPL (C) 1999  Rusty Russell (rusty@rustcorp.com.au).
 */
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/netfilter_ipv4/ip_conntrack.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_state.h>

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
	const struct ipt_state_info *sinfo = matchinfo;
	enum ip_conntrack_info ctinfo;
	unsigned int statebit;

	if (!ip_conntrack_get((struct sk_buff *)skb, &ctinfo))
		statebit = IPT_STATE_INVALID;
	else
		statebit = IPT_STATE_BIT(ctinfo);

	return (sinfo->statemask & statebit);
}

static int check(const char *tablename,
		 const struct ipt_ip *ip,
		 void *matchinfo,
		 unsigned int matchsize,
		 unsigned int hook_mask)
{
	if (matchsize != IPT_ALIGN(sizeof(struct ipt_state_info)))
		return 0;

	return 1;
}

static struct ipt_match state_match
= { { NULL, NULL }, "state", &match, &check, NULL, THIS_MODULE };

static int __init init(void)
{
	return ipt_register_match(&state_match);
}

static void __exit fini(void)
{
	ipt_unregister_match(&state_match);
}

module_init(init);
module_exit(fini);
MODULE_LICENSE("GPL");
