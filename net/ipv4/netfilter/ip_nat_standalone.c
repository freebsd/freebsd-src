/* This file contains all the functions required for the standalone
   ip_nat module.

   These are not required by the compatibility layer.
*/

/* (c) 1999 Paul `Rusty' Russell.  Licenced under the GNU General
 * Public Licence.
 *
 * 23 Apr 2001: Harald Welte <laforge@gnumonks.org>
 * 	- new API and handling of conntrack/nat helpers
 * 	- now capable of multiple expectations for one master
 * */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/ip.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/proc_fs.h>
#include <net/checksum.h>
#include <linux/spinlock.h>
#include <linux/version.h>
#include <linux/brlock.h>

#define ASSERT_READ_LOCK(x) MUST_BE_READ_LOCKED(&ip_nat_lock)
#define ASSERT_WRITE_LOCK(x) MUST_BE_WRITE_LOCKED(&ip_nat_lock)

#include <linux/netfilter_ipv4/ip_nat.h>
#include <linux/netfilter_ipv4/ip_nat_rule.h>
#include <linux/netfilter_ipv4/ip_nat_protocol.h>
#include <linux/netfilter_ipv4/ip_nat_core.h>
#include <linux/netfilter_ipv4/ip_nat_helper.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ip_conntrack_core.h>
#include <linux/netfilter_ipv4/listhelp.h>

#if 0
#define DEBUGP printk
#else
#define DEBUGP(format, args...)
#endif

#define HOOKNAME(hooknum) ((hooknum) == NF_IP_POST_ROUTING ? "POST_ROUTING"  \
			   : ((hooknum) == NF_IP_PRE_ROUTING ? "PRE_ROUTING" \
			      : ((hooknum) == NF_IP_LOCAL_OUT ? "LOCAL_OUT"  \
			         : ((hooknum) == NF_IP_LOCAL_IN ? "LOCAL_IN"  \
				    : "*ERROR*")))

static inline int call_expect(struct ip_conntrack *master,
			      struct sk_buff **pskb,
			      unsigned int hooknum,
			      struct ip_conntrack *ct,
			      struct ip_nat_info *info)
{
	return master->nat.info.helper->expect(pskb, hooknum, ct, info);
}

static unsigned int
ip_nat_fn(unsigned int hooknum,
	  struct sk_buff **pskb,
	  const struct net_device *in,
	  const struct net_device *out,
	  int (*okfn)(struct sk_buff *))
{
	struct ip_conntrack *ct;
	enum ip_conntrack_info ctinfo;
	struct ip_nat_info *info;
	/* maniptype == SRC for postrouting. */
	enum ip_nat_manip_type maniptype = HOOK2MANIP(hooknum);

	/* We never see fragments: conntrack defrags on pre-routing
	   and local-out, and ip_nat_out protects post-routing. */
	IP_NF_ASSERT(!((*pskb)->nh.iph->frag_off
		       & htons(IP_MF|IP_OFFSET)));

	(*pskb)->nfcache |= NFC_UNKNOWN;

	/* If we had a hardware checksum before, it's now invalid */
	if ((*pskb)->ip_summed == CHECKSUM_HW)
		(*pskb)->ip_summed = CHECKSUM_NONE;

	ct = ip_conntrack_get(*pskb, &ctinfo);
	/* Can't track?  It's not due to stress, or conntrack would
	   have dropped it.  Hence it's the user's responsibilty to
	   packet filter it out, or implement conntrack/NAT for that
	   protocol. 8) --RR */
	if (!ct) {
		/* Exception: ICMP redirect to new connection (not in
                   hash table yet).  We must not let this through, in
                   case we're doing NAT to the same network. */
		struct iphdr *iph = (*pskb)->nh.iph;
		struct icmphdr *hdr = (struct icmphdr *)
			((u_int32_t *)iph + iph->ihl);
		if (iph->protocol == IPPROTO_ICMP
		    && hdr->type == ICMP_REDIRECT)
			return NF_DROP;
		return NF_ACCEPT;
	}

	switch (ctinfo) {
	case IP_CT_RELATED:
	case IP_CT_RELATED+IP_CT_IS_REPLY:
		if ((*pskb)->nh.iph->protocol == IPPROTO_ICMP) {
			return icmp_reply_translation(*pskb, ct, hooknum,
						      CTINFO2DIR(ctinfo));
		}
		/* Fall thru... (Only ICMPs can be IP_CT_IS_REPLY) */
	case IP_CT_NEW:
		info = &ct->nat.info;

		WRITE_LOCK(&ip_nat_lock);
		/* Seen it before?  This can happen for loopback, retrans,
		   or local packets.. */
		if (!(info->initialized & (1 << maniptype))
#ifndef CONFIG_IP_NF_NAT_LOCAL
		    /* If this session has already been confirmed we must not
		     * touch it again even if there is no mapping set up.
		     * Can only happen on local->local traffic with
		     * CONFIG_IP_NF_NAT_LOCAL disabled.
		     */
		    && !(ct->status & IPS_CONFIRMED)
#endif
		    ) {
			unsigned int ret;

			if (ct->master
			    && master_ct(ct)->nat.info.helper
			    && master_ct(ct)->nat.info.helper->expect) {
				ret = call_expect(master_ct(ct), pskb, 
						  hooknum, ct, info);
			} else {
#ifdef CONFIG_IP_NF_NAT_LOCAL
				/* LOCAL_IN hook doesn't have a chain!  */
				if (hooknum == NF_IP_LOCAL_IN)
					ret = alloc_null_binding(ct, info,
								 hooknum);
				else
#endif
				ret = ip_nat_rule_find(pskb, hooknum, in, out,
						       ct, info);
			}

			if (ret != NF_ACCEPT) {
				WRITE_UNLOCK(&ip_nat_lock);
				return ret;
			}
		} else
			DEBUGP("Already setup manip %s for ct %p\n",
			       maniptype == IP_NAT_MANIP_SRC ? "SRC" : "DST",
			       ct);
		WRITE_UNLOCK(&ip_nat_lock);
		break;

	default:
		/* ESTABLISHED */
		IP_NF_ASSERT(ctinfo == IP_CT_ESTABLISHED
			     || ctinfo == (IP_CT_ESTABLISHED+IP_CT_IS_REPLY));
		info = &ct->nat.info;
	}

	IP_NF_ASSERT(info);
	return do_bindings(ct, ctinfo, info, hooknum, pskb);
}

static unsigned int
ip_nat_out(unsigned int hooknum,
	   struct sk_buff **pskb,
	   const struct net_device *in,
	   const struct net_device *out,
	   int (*okfn)(struct sk_buff *))
{
	/* root is playing with raw sockets. */
	if ((*pskb)->len < sizeof(struct iphdr)
	    || (*pskb)->nh.iph->ihl * 4 < sizeof(struct iphdr))
		return NF_ACCEPT;

	/* We can hit fragment here; forwarded packets get
	   defragmented by connection tracking coming in, then
	   fragmented (grr) by the forward code.

	   In future: If we have nfct != NULL, AND we have NAT
	   initialized, AND there is no helper, then we can do full
	   NAPT on the head, and IP-address-only NAT on the rest.

	   I'm starting to have nightmares about fragments.  */

	if ((*pskb)->nh.iph->frag_off & htons(IP_MF|IP_OFFSET)) {
		*pskb = ip_ct_gather_frags(*pskb);

		if (!*pskb)
			return NF_STOLEN;
	}

	return ip_nat_fn(hooknum, pskb, in, out, okfn);
}

#ifdef CONFIG_IP_NF_NAT_LOCAL
static unsigned int
ip_nat_local_fn(unsigned int hooknum,
		struct sk_buff **pskb,
		const struct net_device *in,
		const struct net_device *out,
		int (*okfn)(struct sk_buff *))
{
	u_int32_t saddr, daddr;
	unsigned int ret;

	/* root is playing with raw sockets. */
	if ((*pskb)->len < sizeof(struct iphdr)
	    || (*pskb)->nh.iph->ihl * 4 < sizeof(struct iphdr))
		return NF_ACCEPT;

	saddr = (*pskb)->nh.iph->saddr;
	daddr = (*pskb)->nh.iph->daddr;

	ret = ip_nat_fn(hooknum, pskb, in, out, okfn);
	if (ret != NF_DROP && ret != NF_STOLEN
	    && ((*pskb)->nh.iph->saddr != saddr
		|| (*pskb)->nh.iph->daddr != daddr))
		return ip_route_me_harder(pskb) == 0 ? ret : NF_DROP;
	return ret;
}
#endif

/* We must be after connection tracking and before packet filtering. */

/* Before packet filtering, change destination */
static struct nf_hook_ops ip_nat_in_ops
= { { NULL, NULL }, ip_nat_fn, PF_INET, NF_IP_PRE_ROUTING, NF_IP_PRI_NAT_DST };
/* After packet filtering, change source */
static struct nf_hook_ops ip_nat_out_ops
= { { NULL, NULL }, ip_nat_out, PF_INET, NF_IP_POST_ROUTING, NF_IP_PRI_NAT_SRC};

#ifdef CONFIG_IP_NF_NAT_LOCAL
/* Before packet filtering, change destination */
static struct nf_hook_ops ip_nat_local_out_ops
= { { NULL, NULL }, ip_nat_local_fn, PF_INET, NF_IP_LOCAL_OUT, NF_IP_PRI_NAT_DST };
/* After packet filtering, change source for reply packets of LOCAL_OUT DNAT */
static struct nf_hook_ops ip_nat_local_in_ops
= { { NULL, NULL }, ip_nat_fn, PF_INET, NF_IP_LOCAL_IN, NF_IP_PRI_NAT_SRC };
#endif

/* Protocol registration. */
int ip_nat_protocol_register(struct ip_nat_protocol *proto)
{
	int ret = 0;
	struct list_head *i;

	WRITE_LOCK(&ip_nat_lock);
	for (i = protos.next; i != &protos; i = i->next) {
		if (((struct ip_nat_protocol *)i)->protonum
		    == proto->protonum) {
			ret = -EBUSY;
			goto out;
		}
	}

	list_prepend(&protos, proto);
	MOD_INC_USE_COUNT;

 out:
	WRITE_UNLOCK(&ip_nat_lock);
	return ret;
}

/* Noone stores the protocol anywhere; simply delete it. */
void ip_nat_protocol_unregister(struct ip_nat_protocol *proto)
{
	WRITE_LOCK(&ip_nat_lock);
	LIST_DELETE(&protos, proto);
	WRITE_UNLOCK(&ip_nat_lock);

	/* Someone could be still looking at the proto in a bh. */
	br_write_lock_bh(BR_NETPROTO_LOCK);
	br_write_unlock_bh(BR_NETPROTO_LOCK);

	MOD_DEC_USE_COUNT;
}

static int init_or_cleanup(int init)
{
	int ret = 0;

	if (!init) goto cleanup;

	ret = ip_nat_rule_init();
	if (ret < 0) {
		printk("ip_nat_init: can't setup rules.\n");
		goto cleanup_nothing;
	}
	ret = ip_nat_init();
	if (ret < 0) {
		printk("ip_nat_init: can't setup rules.\n");
		goto cleanup_rule_init;
	}
	ret = nf_register_hook(&ip_nat_in_ops);
	if (ret < 0) {
		printk("ip_nat_init: can't register in hook.\n");
		goto cleanup_nat;
	}
	ret = nf_register_hook(&ip_nat_out_ops);
	if (ret < 0) {
		printk("ip_nat_init: can't register out hook.\n");
		goto cleanup_inops;
	}
#ifdef CONFIG_IP_NF_NAT_LOCAL
	ret = nf_register_hook(&ip_nat_local_out_ops);
	if (ret < 0) {
		printk("ip_nat_init: can't register local out hook.\n");
		goto cleanup_outops;
	}
	ret = nf_register_hook(&ip_nat_local_in_ops);
	if (ret < 0) {
		printk("ip_nat_init: can't register local in hook.\n");
		goto cleanup_localoutops;
	}
#endif
	return ret;

 cleanup:
#ifdef CONFIG_IP_NF_NAT_LOCAL
	nf_unregister_hook(&ip_nat_local_in_ops);
 cleanup_localoutops:
	nf_unregister_hook(&ip_nat_local_out_ops);
 cleanup_outops:
#endif
	nf_unregister_hook(&ip_nat_out_ops);
 cleanup_inops:
	nf_unregister_hook(&ip_nat_in_ops);
 cleanup_nat:
	ip_nat_cleanup();
 cleanup_rule_init:
	ip_nat_rule_cleanup();
 cleanup_nothing:
	MUST_BE_READ_WRITE_UNLOCKED(&ip_nat_lock);
	return ret;
}

static int __init init(void)
{
	return init_or_cleanup(1);
}

static void __exit fini(void)
{
	init_or_cleanup(0);
}

module_init(init);
module_exit(fini);

EXPORT_SYMBOL(ip_nat_setup_info);
EXPORT_SYMBOL(ip_nat_protocol_register);
EXPORT_SYMBOL(ip_nat_protocol_unregister);
EXPORT_SYMBOL(ip_nat_helper_register);
EXPORT_SYMBOL(ip_nat_helper_unregister);
EXPORT_SYMBOL(ip_nat_cheat_check);
EXPORT_SYMBOL(ip_nat_mangle_tcp_packet);
EXPORT_SYMBOL(ip_nat_mangle_udp_packet);
EXPORT_SYMBOL(ip_nat_used_tuple);
MODULE_LICENSE("GPL");
