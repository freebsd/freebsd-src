/* Kernel module to match MAC address parameters. */
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/if_ether.h>

#include <linux/netfilter_ipv6/ip6t_mac.h>
#include <linux/netfilter_ipv6/ip6_tables.h>

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
    const struct ip6t_mac_info *info = matchinfo;

    /* Is mac pointer valid? */
    return (skb->mac.raw >= skb->head
	    && (skb->mac.raw + ETH_HLEN) <= skb->data
	    /* If so, compare... */
	    && ((memcmp(skb->mac.ethernet->h_source, info->srcaddr, ETH_ALEN)
		== 0) ^ info->invert));
}

static int
ip6t_mac_checkentry(const char *tablename,
		   const struct ip6t_ip6 *ip,
		   void *matchinfo,
		   unsigned int matchsize,
		   unsigned int hook_mask)
{
	if (hook_mask
	    & ~((1 << NF_IP6_PRE_ROUTING) | (1 << NF_IP6_LOCAL_IN)
		| (1 << NF_IP6_FORWARD))) {
		printk("ip6t_mac: only valid for PRE_ROUTING, LOCAL_IN or"
		       " FORWARD\n");
		return 0;
	}

	if (matchsize != IP6T_ALIGN(sizeof(struct ip6t_mac_info)))
		return 0;

	return 1;
}

static struct ip6t_match mac_match
= { { NULL, NULL }, "mac", &match, &ip6t_mac_checkentry, NULL, THIS_MODULE };

static int __init init(void)
{
	return ip6t_register_match(&mac_match);
}

static void __exit fini(void)
{
	ip6t_unregister_match(&mac_match);
}

module_init(init);
module_exit(fini);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MAC address matching module for IPv6");
