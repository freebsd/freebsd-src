/* Everything about the rules for NAT. */
#include <linux/types.h>
#include <linux/ip.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/module.h>
#include <linux/kmod.h>
#include <linux/skbuff.h>
#include <linux/proc_fs.h>
#include <net/checksum.h>
#include <linux/bitops.h>
#include <linux/version.h>

#define ASSERT_READ_LOCK(x) MUST_BE_READ_LOCKED(&ip_nat_lock)
#define ASSERT_WRITE_LOCK(x) MUST_BE_WRITE_LOCKED(&ip_nat_lock)

#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ip_nat.h>
#include <linux/netfilter_ipv4/ip_nat_core.h>
#include <linux/netfilter_ipv4/ip_nat_rule.h>
#include <linux/netfilter_ipv4/listhelp.h>

#if 0
#define DEBUGP printk
#else
#define DEBUGP(format, args...)
#endif

#define NAT_VALID_HOOKS ((1<<NF_IP_PRE_ROUTING) | (1<<NF_IP_POST_ROUTING) | (1<<NF_IP_LOCAL_OUT))

/* Standard entry. */
struct ipt_standard
{
	struct ipt_entry entry;
	struct ipt_standard_target target;
};

struct ipt_error_target
{
	struct ipt_entry_target target;
	char errorname[IPT_FUNCTION_MAXNAMELEN];
};

struct ipt_error
{
	struct ipt_entry entry;
	struct ipt_error_target target;
};

static struct
{
	struct ipt_replace repl;
	struct ipt_standard entries[3];
	struct ipt_error term;
} nat_initial_table __initdata
= { { "nat", NAT_VALID_HOOKS, 4,
      sizeof(struct ipt_standard) * 3 + sizeof(struct ipt_error),
      { [NF_IP_PRE_ROUTING] 0,
	[NF_IP_POST_ROUTING] sizeof(struct ipt_standard),
	[NF_IP_LOCAL_OUT] sizeof(struct ipt_standard) * 2 },
      { [NF_IP_PRE_ROUTING] 0,
	[NF_IP_POST_ROUTING] sizeof(struct ipt_standard),
	[NF_IP_LOCAL_OUT] sizeof(struct ipt_standard) * 2 },
      0, NULL, { } },
    {
	    /* PRE_ROUTING */
	    { { { { 0 }, { 0 }, { 0 }, { 0 }, "", "", { 0 }, { 0 }, 0, 0, 0 },
		0,
		sizeof(struct ipt_entry),
		sizeof(struct ipt_standard),
		0, { 0, 0 }, { } },
	      { { { { IPT_ALIGN(sizeof(struct ipt_standard_target)), "" } }, { } },
		-NF_ACCEPT - 1 } },
	    /* POST_ROUTING */
	    { { { { 0 }, { 0 }, { 0 }, { 0 }, "", "", { 0 }, { 0 }, 0, 0, 0 },
		0,
		sizeof(struct ipt_entry),
		sizeof(struct ipt_standard),
		0, { 0, 0 }, { } },
	      { { { { IPT_ALIGN(sizeof(struct ipt_standard_target)), "" } }, { } },
		-NF_ACCEPT - 1 } },
	    /* LOCAL_OUT */
	    { { { { 0 }, { 0 }, { 0 }, { 0 }, "", "", { 0 }, { 0 }, 0, 0, 0 },
		0,
		sizeof(struct ipt_entry),
		sizeof(struct ipt_standard),
		0, { 0, 0 }, { } },
	      { { { { IPT_ALIGN(sizeof(struct ipt_standard_target)), "" } }, { } },
		-NF_ACCEPT - 1 } }
    },
    /* ERROR */
    { { { { 0 }, { 0 }, { 0 }, { 0 }, "", "", { 0 }, { 0 }, 0, 0, 0 },
	0,
	sizeof(struct ipt_entry),
	sizeof(struct ipt_error),
	0, { 0, 0 }, { } },
      { { { { IPT_ALIGN(sizeof(struct ipt_error_target)), IPT_ERROR_TARGET } },
	  { } },
	"ERROR"
      }
    }
};

static struct ipt_table nat_table
= { { NULL, NULL }, "nat", &nat_initial_table.repl,
    NAT_VALID_HOOKS, RW_LOCK_UNLOCKED, NULL, THIS_MODULE };

/* Source NAT */
static unsigned int ipt_snat_target(struct sk_buff **pskb,
				    unsigned int hooknum,
				    const struct net_device *in,
				    const struct net_device *out,
				    const void *targinfo,
				    void *userinfo)
{
	struct ip_conntrack *ct;
	enum ip_conntrack_info ctinfo;

	IP_NF_ASSERT(hooknum == NF_IP_POST_ROUTING);

	ct = ip_conntrack_get(*pskb, &ctinfo);

	/* Connection must be valid and new. */
	IP_NF_ASSERT(ct && (ctinfo == IP_CT_NEW || ctinfo == IP_CT_RELATED));
	IP_NF_ASSERT(out);

	return ip_nat_setup_info(ct, targinfo, hooknum);
}

static unsigned int ipt_dnat_target(struct sk_buff **pskb,
				    unsigned int hooknum,
				    const struct net_device *in,
				    const struct net_device *out,
				    const void *targinfo,
				    void *userinfo)
{
	struct ip_conntrack *ct;
	enum ip_conntrack_info ctinfo;

#ifdef CONFIG_IP_NF_NAT_LOCAL
	IP_NF_ASSERT(hooknum == NF_IP_PRE_ROUTING
		     || hooknum == NF_IP_LOCAL_OUT);
#else
	IP_NF_ASSERT(hooknum == NF_IP_PRE_ROUTING);
#endif

	ct = ip_conntrack_get(*pskb, &ctinfo);

	/* Connection must be valid and new. */
	IP_NF_ASSERT(ct && (ctinfo == IP_CT_NEW || ctinfo == IP_CT_RELATED));

	return ip_nat_setup_info(ct, targinfo, hooknum);
}

static int ipt_snat_checkentry(const char *tablename,
			       const struct ipt_entry *e,
			       void *targinfo,
			       unsigned int targinfosize,
			       unsigned int hook_mask)
{
	struct ip_nat_multi_range *mr = targinfo;

	/* Must be a valid range */
	if (targinfosize < sizeof(struct ip_nat_multi_range)) {
		DEBUGP("SNAT: Target size %u too small\n", targinfosize);
		return 0;
	}

	if (targinfosize != IPT_ALIGN((sizeof(struct ip_nat_multi_range)
				       + (sizeof(struct ip_nat_range)
					  * (mr->rangesize - 1))))) {
		DEBUGP("SNAT: Target size %u wrong for %u ranges\n",
		       targinfosize, mr->rangesize);
		return 0;
	}

	/* Only allow these for NAT. */
	if (strcmp(tablename, "nat") != 0) {
		DEBUGP("SNAT: wrong table %s\n", tablename);
		return 0;
	}

	if (hook_mask & ~(1 << NF_IP_POST_ROUTING)) {
		DEBUGP("SNAT: hook mask 0x%x bad\n", hook_mask);
		return 0;
	}
	return 1;
}

static int ipt_dnat_checkentry(const char *tablename,
			       const struct ipt_entry *e,
			       void *targinfo,
			       unsigned int targinfosize,
			       unsigned int hook_mask)
{
	struct ip_nat_multi_range *mr = targinfo;

	/* Must be a valid range */
	if (targinfosize < sizeof(struct ip_nat_multi_range)) {
		DEBUGP("DNAT: Target size %u too small\n", targinfosize);
		return 0;
	}

	if (targinfosize != IPT_ALIGN((sizeof(struct ip_nat_multi_range)
				       + (sizeof(struct ip_nat_range)
					  * (mr->rangesize - 1))))) {
		DEBUGP("DNAT: Target size %u wrong for %u ranges\n",
		       targinfosize, mr->rangesize);
		return 0;
	}

	/* Only allow these for NAT. */
	if (strcmp(tablename, "nat") != 0) {
		DEBUGP("DNAT: wrong table %s\n", tablename);
		return 0;
	}

	if (hook_mask & ~((1 << NF_IP_PRE_ROUTING) | (1 << NF_IP_LOCAL_OUT))) {
		DEBUGP("DNAT: hook mask 0x%x bad\n", hook_mask);
		return 0;
	}
	
#ifndef CONFIG_IP_NF_NAT_LOCAL
	if (hook_mask & (1 << NF_IP_LOCAL_OUT)) {
		DEBUGP("DNAT: CONFIG_IP_NF_NAT_LOCAL not enabled\n");
		return 0;
	}
#endif

	return 1;
}

inline unsigned int
alloc_null_binding(struct ip_conntrack *conntrack,
		   struct ip_nat_info *info,
		   unsigned int hooknum)
{
	/* Force range to this IP; let proto decide mapping for
	   per-proto parts (hence not IP_NAT_RANGE_PROTO_SPECIFIED).
	   Use reply in case it's already been mangled (eg local packet).
	*/
	u_int32_t ip
		= (HOOK2MANIP(hooknum) == IP_NAT_MANIP_SRC
		   ? conntrack->tuplehash[IP_CT_DIR_REPLY].tuple.dst.ip
		   : conntrack->tuplehash[IP_CT_DIR_REPLY].tuple.src.ip);
	struct ip_nat_multi_range mr
		= { 1, { { IP_NAT_RANGE_MAP_IPS, ip, ip, { 0 }, { 0 } } } };

	DEBUGP("Allocating NULL binding for %p (%u.%u.%u.%u)\n", conntrack,
	       NIPQUAD(ip));
	return ip_nat_setup_info(conntrack, &mr, hooknum);
}

int ip_nat_rule_find(struct sk_buff **pskb,
		     unsigned int hooknum,
		     const struct net_device *in,
		     const struct net_device *out,
		     struct ip_conntrack *ct,
		     struct ip_nat_info *info)
{
	int ret;

	ret = ipt_do_table(pskb, hooknum, in, out, &nat_table, NULL);

	if (ret == NF_ACCEPT) {
		if (!(info->initialized & (1 << HOOK2MANIP(hooknum))))
			/* NUL mapping */
			ret = alloc_null_binding(ct, info, hooknum);
	}
	return ret;
}

static struct ipt_target ipt_snat_reg
= { { NULL, NULL }, "SNAT", ipt_snat_target, ipt_snat_checkentry, NULL };
static struct ipt_target ipt_dnat_reg
= { { NULL, NULL }, "DNAT", ipt_dnat_target, ipt_dnat_checkentry, NULL };

int __init ip_nat_rule_init(void)
{
	int ret;

	ret = ipt_register_table(&nat_table);
	if (ret != 0)
		return ret;
	ret = ipt_register_target(&ipt_snat_reg);
	if (ret != 0)
		goto unregister_table;

	ret = ipt_register_target(&ipt_dnat_reg);
	if (ret != 0)
		goto unregister_snat;

	return ret;

 unregister_snat:
	ipt_unregister_target(&ipt_snat_reg);
 unregister_table:
	ipt_unregister_table(&nat_table);

	return ret;
}

void ip_nat_rule_cleanup(void)
{
	ipt_unregister_target(&ipt_dnat_reg);
	ipt_unregister_target(&ipt_snat_reg);
	ipt_unregister_table(&nat_table);
}
