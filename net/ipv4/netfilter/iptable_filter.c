/*
 * This is the 1999 rewrite of IP Firewalling, aiming for kernel 2.3.x.
 *
 * Copyright (C) 1999 Paul `Rusty' Russell & Michael J. Neuling
 */
#include <linux/module.h>
#include <linux/netfilter_ipv4/ip_tables.h>

#define FILTER_VALID_HOOKS ((1 << NF_IP_LOCAL_IN) | (1 << NF_IP_FORWARD) | (1 << NF_IP_LOCAL_OUT))

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
} initial_table __initdata
= { { "filter", FILTER_VALID_HOOKS, 4,
      sizeof(struct ipt_standard) * 3 + sizeof(struct ipt_error),
      { [NF_IP_LOCAL_IN] 0,
	[NF_IP_FORWARD] sizeof(struct ipt_standard),
	[NF_IP_LOCAL_OUT] sizeof(struct ipt_standard) * 2 },
      { [NF_IP_LOCAL_IN] 0,
	[NF_IP_FORWARD] sizeof(struct ipt_standard),
	[NF_IP_LOCAL_OUT] sizeof(struct ipt_standard) * 2 },
      0, NULL, { } },
    {
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

static struct ipt_table packet_filter
= { { NULL, NULL }, "filter", &initial_table.repl,
    FILTER_VALID_HOOKS, RW_LOCK_UNLOCKED, NULL, THIS_MODULE };

/* The work comes in here from netfilter.c. */
static unsigned int
ipt_hook(unsigned int hook,
	 struct sk_buff **pskb,
	 const struct net_device *in,
	 const struct net_device *out,
	 int (*okfn)(struct sk_buff *))
{
	return ipt_do_table(pskb, hook, in, out, &packet_filter, NULL);
}

static unsigned int
ipt_local_out_hook(unsigned int hook,
		   struct sk_buff **pskb,
		   const struct net_device *in,
		   const struct net_device *out,
		   int (*okfn)(struct sk_buff *))
{
	/* root is playing with raw sockets. */
	if ((*pskb)->len < sizeof(struct iphdr)
	    || (*pskb)->nh.iph->ihl * 4 < sizeof(struct iphdr)) {
		if (net_ratelimit())
			printk("ipt_hook: happy cracking.\n");
		return NF_ACCEPT;
	}

	return ipt_do_table(pskb, hook, in, out, &packet_filter, NULL);
}

static struct nf_hook_ops ipt_ops[]
= { { { NULL, NULL }, ipt_hook, PF_INET, NF_IP_LOCAL_IN, NF_IP_PRI_FILTER },
    { { NULL, NULL }, ipt_hook, PF_INET, NF_IP_FORWARD, NF_IP_PRI_FILTER },
    { { NULL, NULL }, ipt_local_out_hook, PF_INET, NF_IP_LOCAL_OUT,
		NF_IP_PRI_FILTER }
};

/* Default to forward because I got too much mail already. */
static int forward = NF_ACCEPT;
MODULE_PARM(forward, "i");

static int __init init(void)
{
	int ret;

	if (forward < 0 || forward > NF_MAX_VERDICT) {
		printk("iptables forward must be 0 or 1\n");
		return -EINVAL;
	}

	/* Entry 1 is the FORWARD hook */
	initial_table.entries[1].target.verdict = -forward - 1;

	/* Register table */
	ret = ipt_register_table(&packet_filter);
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

	return ret;

 cleanup_hook1:
	nf_unregister_hook(&ipt_ops[1]);
 cleanup_hook0:
	nf_unregister_hook(&ipt_ops[0]);
 cleanup_table:
	ipt_unregister_table(&packet_filter);

	return ret;
}

static void __exit fini(void)
{
	unsigned int i;

	for (i = 0; i < sizeof(ipt_ops)/sizeof(struct nf_hook_ops); i++)
		nf_unregister_hook(&ipt_ops[i]);

	ipt_unregister_table(&packet_filter);
}

module_init(init);
module_exit(fini);
MODULE_LICENSE("GPL");
