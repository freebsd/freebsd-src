/*
 * iptables module to match on related connections
 *   (c) 2001 Martin Josefsson <gandalf@wlug.westbo.se>
 *
 * Released under the terms of GNU GPLv2.
 *
 *   19 Mar 2002 Harald Welte <laforge@gnumonks.org>:
 *   		 - Port to newnat infrastructure
 */
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/netfilter_ipv4/ip_conntrack.h>
#include <linux/netfilter_ipv4/ip_conntrack_core.h>
#include <linux/netfilter_ipv4/ip_conntrack_helper.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_helper.h>

MODULE_LICENSE("GPL");

#if 0
#define DEBUGP printk
#else
#define DEBUGP(format, args...)
#endif

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
	const struct ipt_helper_info *info = matchinfo;
	struct ip_conntrack_expect *exp;
	struct ip_conntrack *ct;
	enum ip_conntrack_info ctinfo;
	int ret = 0;
	
	ct = ip_conntrack_get((struct sk_buff *)skb, &ctinfo);
	if (!ct) {
		DEBUGP("ipt_helper: Eek! invalid conntrack?\n");
		return 0;
	}

	if (!ct->master) {
		DEBUGP("ipt_helper: conntrack %p has no master\n", ct);
		return 0;
	}

	exp = ct->master;
	READ_LOCK(&ip_conntrack_lock);
	if (!exp->expectant) {
		DEBUGP("ipt_helper: expectation %p without expectant !?!\n", 
			exp);
		goto out_unlock;
	}

	if (!exp->expectant->helper) {
		DEBUGP("ipt_helper: master ct %p has no helper\n", 
			exp->expectant);
		goto out_unlock;
	}

	DEBUGP("master's name = %s , info->name = %s\n", 
		exp->expectant->helper->name, info->name);

	ret = !strncmp(exp->expectant->helper->name, info->name, 
	               strlen(exp->expectant->helper->name)) ^ info->invert;
out_unlock:
	READ_UNLOCK(&ip_conntrack_lock);
	return ret;
}

static int check(const char *tablename,
		 const struct ipt_ip *ip,
		 void *matchinfo,
		 unsigned int matchsize,
		 unsigned int hook_mask)
{
	struct ipt_helper_info *info = matchinfo;

	info->name[29] = '\0';

	/* verify size */
	if (matchsize != IPT_ALIGN(sizeof(struct ipt_helper_info)))
		return 0;

	/* verify that we actually should match anything */
	if ( strlen(info->name) == 0 )
		return 0;
	
	return 1;
}

static struct ipt_match helper_match
= { { NULL, NULL }, "helper", &match, &check, NULL, THIS_MODULE };

static int __init init(void)
{
	return ipt_register_match(&helper_match);
}

static void __exit fini(void)
{
	ipt_unregister_match(&helper_match);
}

module_init(init);
module_exit(fini);

