/* IP tables module for matching the value of the TTL 
 *
 * ipt_ttl.c,v 1.5 2000/11/13 11:16:08 laforge Exp
 *
 * (C) 2000,2001 by Harald Welte <laforge@gnumonks.org>
 *
 * This software is distributed under the terms  GNU GPL
 */

#include <linux/module.h>
#include <linux/skbuff.h>

#include <linux/netfilter_ipv4/ipt_ttl.h>
#include <linux/netfilter_ipv4/ip_tables.h>

MODULE_AUTHOR("Harald Welte <laforge@gnumonks.org>");
MODULE_DESCRIPTION("IP tables TTL matching module");
MODULE_LICENSE("GPL");

static int match(const struct sk_buff *skb, const struct net_device *in,
		 const struct net_device *out, const void *matchinfo,
		 int offset, const void *hdr, u_int16_t datalen,
		 int *hotdrop)
{
	const struct ipt_ttl_info *info = matchinfo;
	const struct iphdr *iph = skb->nh.iph;

	switch (info->mode) {
		case IPT_TTL_EQ:
			return (iph->ttl == info->ttl);
			break;
		case IPT_TTL_NE:
			return (!(iph->ttl == info->ttl));
			break;
		case IPT_TTL_LT:
			return (iph->ttl < info->ttl);
			break;
		case IPT_TTL_GT:
			return (iph->ttl > info->ttl);
			break;
		default:
			printk(KERN_WARNING "ipt_ttl: unknown mode %d\n", 
				info->mode);
			return 0;
	}

	return 0;
}

static int checkentry(const char *tablename, const struct ipt_ip *ip,
		      void *matchinfo, unsigned int matchsize,
		      unsigned int hook_mask)
{
	if (matchsize != IPT_ALIGN(sizeof(struct ipt_ttl_info)))
		return 0;

	return 1;
}

static struct ipt_match ttl_match = { { NULL, NULL }, "ttl", &match,
		&checkentry, NULL, THIS_MODULE };

static int __init init(void)
{
	return ipt_register_match(&ttl_match);
}

static void __exit fini(void)
{
	ipt_unregister_match(&ttl_match);

}

module_init(init);
module_exit(fini);
