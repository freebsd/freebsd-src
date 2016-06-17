/*
 * This is the 1999 rewrite of IP Firewalling, aiming for kernel 2.3.x.
 *
 * Copyright (C) 1999 Paul `Rusty' Russell & Michael J. Neuling
 *
 * Extended to all five netfilter hooks by Brad Chapman & Harald Welte
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <net/route.h>
#include <linux/ip.h>

#define MANGLE_VALID_HOOKS ((1 << NF_IP_PRE_ROUTING) | \
			    (1 << NF_IP_LOCAL_IN) | \
			    (1 << NF_IP_FORWARD) | \
			    (1 << NF_IP_LOCAL_OUT) | \
			    (1 << NF_IP_POST_ROUTING))

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

/* Ouch - five different hooks? Maybe this should be a config option..... -- BC */
static struct
{
	struct ipt_replace repl;
	struct ipt_standard entries[5];
	struct ipt_error term;
} initial_table __initdata
= { { "mangle", MANGLE_VALID_HOOKS, 6,
      sizeof(struct ipt_standard) * 5 + sizeof(struct ipt_error),
      { [NF_IP_PRE_ROUTING] 	0,
	[NF_IP_LOCAL_IN] 	sizeof(struct ipt_standard),
	[NF_IP_FORWARD] 	sizeof(struct ipt_standard) * 2,
	[NF_IP_LOCAL_OUT] 	sizeof(struct ipt_standard) * 3,
	[NF_IP_POST_ROUTING] 	sizeof(struct ipt_standard) * 4 },
      { [NF_IP_PRE_ROUTING] 	0,
	[NF_IP_LOCAL_IN] 	sizeof(struct ipt_standard),
	[NF_IP_FORWARD] 	sizeof(struct ipt_standard) * 2,
	[NF_IP_LOCAL_OUT] 	sizeof(struct ipt_standard) * 3,
	[NF_IP_POST_ROUTING]	sizeof(struct ipt_standard) * 4 },
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
	    /* LOCAL_IN */
 	    { { { { 0 }, { 0 }, { 0 }, { 0 }, "", "", { 0 }, { 0 }, 0, 0, 0 },
		0,
		sizeof(struct ipt_entry),
		sizeof(struct ipt_standard),
		0, { 0, 0 }, { } },
	      { { { { IPT_ALIGN(sizeof(struct ipt_standard_target)), "" } }, { } },
		-NF_ACCEPT - 1 } },
	    /* FORWARD */
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
		-NF_ACCEPT - 1 } },
	    /* POST_ROUTING */
	    { { { { 0 }, { 0 }, { 0 }, { 0 }, "", "", { 0 }, { 0 }, 0, 0, 0 },
		0,
		sizeof(struct ipt_entry),
		sizeof(struct ipt_standard),
		0, { 0, 0 }, { } },
	      { { { { IPT_ALIGN(sizeof(struct ipt_standard_target)), "" } }, { } },
		-NF_ACCEPT - 1 } },
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

static struct ipt_table packet_mangler
= { { NULL, NULL }, "mangle", &initial_table.repl,
    MANGLE_VALID_HOOKS, RW_LOCK_UNLOCKED, NULL, THIS_MODULE };

/* The work comes in here from netfilter.c. */
static unsigned int
ipt_route_hook(unsigned int hook,
	 struct sk_buff **pskb,
	 const struct net_device *in,
	 const struct net_device *out,
	 int (*okfn)(struct sk_buff *))
{
	return ipt_do_table(pskb, hook, in, out, &packet_mangler, NULL);
}

static unsigned int
ipt_local_hook(unsigned int hook,
		   struct sk_buff **pskb,
		   const struct net_device *in,
		   const struct net_device *out,
		   int (*okfn)(struct sk_buff *))
{
	unsigned int ret;
	u_int8_t tos;
	u_int32_t saddr, daddr;
	unsigned long nfmark;

	/* root is playing with raw sockets. */
	if ((*pskb)->len < sizeof(struct iphdr)
	    || (*pskb)->nh.iph->ihl * 4 < sizeof(struct iphdr)) {
		if (net_ratelimit())
			printk("ipt_hook: happy cracking.\n");
		return NF_ACCEPT;
	}

	/* Save things which could affect route */
	nfmark = (*pskb)->nfmark;
	saddr = (*pskb)->nh.iph->saddr;
	daddr = (*pskb)->nh.iph->daddr;
	tos = (*pskb)->nh.iph->tos;

	ret = ipt_do_table(pskb, hook, in, out, &packet_mangler, NULL);
	/* Reroute for ANY change. */
	if (ret != NF_DROP && ret != NF_STOLEN && ret != NF_QUEUE
	    && ((*pskb)->nh.iph->saddr != saddr
		|| (*pskb)->nh.iph->daddr != daddr
		|| (*pskb)->nfmark != nfmark
		|| (*pskb)->nh.iph->tos != tos))
		return ip_route_me_harder(pskb) == 0 ? ret : NF_DROP;

	return ret;
}

static struct nf_hook_ops ipt_ops[]
= { { { NULL, NULL }, ipt_route_hook, PF_INET, NF_IP_PRE_ROUTING, 
	NF_IP_PRI_MANGLE },
    { { NULL, NULL }, ipt_route_hook, PF_INET, NF_IP_LOCAL_IN,
	NF_IP_PRI_MANGLE },
    { { NULL, NULL }, ipt_route_hook, PF_INET, NF_IP_FORWARD,
	NF_IP_PRI_MANGLE },
    { { NULL, NULL }, ipt_local_hook, PF_INET, NF_IP_LOCAL_OUT,
	NF_IP_PRI_MANGLE },
    { { NULL, NULL }, ipt_route_hook, PF_INET, NF_IP_POST_ROUTING,
	NF_IP_PRI_MANGLE }
};

static int __init init(void)
{
	int ret;

	/* Register table */
	ret = ipt_register_table(&packet_mangler);
	if (ret < 0)
		return ret;

	/* Register hooks */
	ret = nf_register_hook(&ipt_ops[0]);
	if (ret < 0)
		goto cleanup_table;

	ret = nf_register_hook(&ipt_ops[1]);
	if (ret < 0)
		goto cleanup_hook0;

	ret = nf_register_hook(&ipt_ops[2]);
	if (ret < 0)
		goto cleanup_hook1;

	ret = nf_register_hook(&ipt_ops[3]);
	if (ret < 0)
		goto cleanup_hook2;

	ret = nf_register_hook(&ipt_ops[4]);
	if (ret < 0)
		goto cleanup_hook3;

	return ret;

 cleanup_hook3:
        nf_unregister_hook(&ipt_ops[3]);
 cleanup_hook2:
        nf_unregister_hook(&ipt_ops[2]);
 cleanup_hook1:
	nf_unregister_hook(&ipt_ops[1]);
 cleanup_hook0:
	nf_unregister_hook(&ipt_ops[0]);
 cleanup_table:
	ipt_unregister_table(&packet_mangler);

	return ret;
}

static void __exit fini(void)
{
	unsigned int i;

	for (i = 0; i < sizeof(ipt_ops)/sizeof(struct nf_hook_ops); i++)
		nf_unregister_hook(&ipt_ops[i]);

	ipt_unregister_table(&packet_mangler);
}

module_init(init);
module_exit(fini);
MODULE_LICENSE("GPL");
