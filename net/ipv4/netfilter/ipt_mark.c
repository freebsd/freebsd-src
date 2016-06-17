/* Kernel module to match NFMARK values. */
#include <linux/module.h>
#include <linux/skbuff.h>

#include <linux/netfilter_ipv4/ipt_mark.h>
#include <linux/netfilter_ipv4/ip_tables.h>

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
	const struct ipt_mark_info *info = matchinfo;

	return ((skb->nfmark & info->mask) == info->mark) ^ info->invert;
}

static int
checkentry(const char *tablename,
           const struct ipt_ip *ip,
           void *matchinfo,
           unsigned int matchsize,
           unsigned int hook_mask)
{
	if (matchsize != IPT_ALIGN(sizeof(struct ipt_mark_info)))
		return 0;

	return 1;
}

static struct ipt_match mark_match
= { { NULL, NULL }, "mark", &match, &checkentry, NULL, THIS_MODULE };

static int __init init(void)
{
	return ipt_register_match(&mark_match);
}

static void __exit fini(void)
{
	ipt_unregister_match(&mark_match);
}

module_init(init);
module_exit(fini);
MODULE_LICENSE("GPL");
