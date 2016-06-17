/* iptables module for setting the IPv4 DSCP field, Version 1.8
 *
 * (C) 2002 by Harald Welte <laforge@gnumonks.org>
 * based on ipt_FTOS.c (C) 2000 by Matthew G. Marsh <mgm@paktronix.com>
 * This software is distributed under GNU GPL v2, 1991
 * 
 * See RFC2474 for a description of the DSCP field within the IP Header.
 *
 * ipt_DSCP.c,v 1.8 2002/08/06 18:41:57 laforge Exp
*/

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <net/checksum.h>

#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_DSCP.h>

MODULE_AUTHOR("Harald Welte <laforge@gnumonks.org>");
MODULE_DESCRIPTION("IP tables DSCP modification module");
MODULE_LICENSE("GPL");

static unsigned int
target(struct sk_buff **pskb,
       unsigned int hooknum,
       const struct net_device *in,
       const struct net_device *out,
       const void *targinfo,
       void *userinfo)
{
	struct iphdr *iph = (*pskb)->nh.iph;
	const struct ipt_DSCP_info *dinfo = targinfo;
	u_int8_t sh_dscp = ((dinfo->dscp << IPT_DSCP_SHIFT) & IPT_DSCP_MASK);


	if ((iph->tos & IPT_DSCP_MASK) != sh_dscp) {
		u_int16_t diffs[2];

		/* raw socket (tcpdump) may have clone of incoming
		 * skb: don't disturb it --RR */
		if (skb_cloned(*pskb) && !(*pskb)->sk) {
			struct sk_buff *nskb = skb_copy(*pskb, GFP_ATOMIC);
			if (!nskb)
				return NF_DROP;
			kfree_skb(*pskb);
			*pskb = nskb;
			iph = (*pskb)->nh.iph;
		}

		diffs[0] = htons(iph->tos) ^ 0xFFFF;
		iph->tos = (iph->tos & ~IPT_DSCP_MASK) | sh_dscp;
		diffs[1] = htons(iph->tos);
		iph->check = csum_fold(csum_partial((char *)diffs,
		                                    sizeof(diffs),
		                                    iph->check^0xFFFF));
		(*pskb)->nfcache |= NFC_ALTERED;
	}
	return IPT_CONTINUE;
}

static int
checkentry(const char *tablename,
	   const struct ipt_entry *e,
           void *targinfo,
           unsigned int targinfosize,
           unsigned int hook_mask)
{
	const u_int8_t dscp = ((struct ipt_DSCP_info *)targinfo)->dscp;

	if (targinfosize != IPT_ALIGN(sizeof(struct ipt_DSCP_info))) {
		printk(KERN_WARNING "DSCP: targinfosize %u != %Zu\n",
		       targinfosize,
		       IPT_ALIGN(sizeof(struct ipt_DSCP_info)));
		return 0;
	}

	if (strcmp(tablename, "mangle") != 0) {
		printk(KERN_WARNING "DSCP: can only be called from \"mangle\" table, not \"%s\"\n", tablename);
		return 0;
	}

	if ((dscp > IPT_DSCP_MAX)) {
		printk(KERN_WARNING "DSCP: dscp %x out of range\n", dscp);
		return 0;
	}

	return 1;
}

static struct ipt_target ipt_dscp_reg
= { { NULL, NULL }, "DSCP", target, checkentry, NULL, THIS_MODULE };

static int __init init(void)
{
	if (ipt_register_target(&ipt_dscp_reg))
		return -EINVAL;

	return 0;
}

static void __exit fini(void)
{
	ipt_unregister_target(&ipt_dscp_reg);
}

module_init(init);
module_exit(fini);
