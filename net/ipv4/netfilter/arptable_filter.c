/*
 * Filtering ARP tables module.
 *
 * Copyright (C) 2002 David S. Miller (davem@redhat.com)
 *
 */

#include <linux/module.h>
#include <linux/netfilter_arp/arp_tables.h>

#define FILTER_VALID_HOOKS ((1 << NF_ARP_IN) | (1 << NF_ARP_OUT))

/* Standard entry. */
struct arpt_standard
{
	struct arpt_entry entry;
	struct arpt_standard_target target;
};

struct arpt_error_target
{
	struct arpt_entry_target target;
	char errorname[ARPT_FUNCTION_MAXNAMELEN];
};

struct arpt_error
{
	struct arpt_entry entry;
	struct arpt_error_target target;
};

static struct
{
	struct arpt_replace repl;
	struct arpt_standard entries[2];
	struct arpt_error term;
} initial_table __initdata
= { { "filter", FILTER_VALID_HOOKS, 3,
      sizeof(struct arpt_standard) * 2 + sizeof(struct arpt_error),
      { [NF_ARP_IN] 0,
	[NF_ARP_OUT] sizeof(struct arpt_standard) },
      { [NF_ARP_IN] 0,
	[NF_ARP_OUT] sizeof(struct arpt_standard), },
      0, NULL, { } },
    {
	    /* ARP_IN */
	    {
		    {
			    {
				    { 0 }, { 0 }, { 0 }, { 0 },
				    0, 0,
				    { { 0, }, { 0, } },
				    { { 0, }, { 0, } },
				    0, 0,
				    0, 0,
				    0, 0,
				    "", "", { 0 }, { 0 },
				    0, 0
			    },
			    sizeof(struct arpt_entry),
			    sizeof(struct arpt_standard),
			    0,
			    { 0, 0 }, { } },
		    { { { { ARPT_ALIGN(sizeof(struct arpt_standard_target)), "" } }, { } },
		      -NF_ACCEPT - 1 }
	    },
	    /* ARP_OUT */
	    {
		    {
			    {
				    { 0 }, { 0 }, { 0 }, { 0 },
				    0, 0,
				    { { 0, }, { 0, } },
				    { { 0, }, { 0, } },
				    0, 0,
				    0, 0,
				    0, 0,
				    "", "", { 0 }, { 0 },
				    0, 0
			    },
			    sizeof(struct arpt_entry),
			    sizeof(struct arpt_standard),
			    0,
			    { 0, 0 }, { } },
		    { { { { ARPT_ALIGN(sizeof(struct arpt_standard_target)), "" } }, { } },
		      -NF_ACCEPT - 1 }
	    }
    },
    /* ERROR */
    {
	    {
		    {
			    { 0 }, { 0 }, { 0 }, { 0 },
			    0, 0,
			    { { 0, }, { 0, } },
			    { { 0, }, { 0, } },
			    0, 0,
			    0, 0,
			    0, 0,
			    "", "", { 0 }, { 0 },
			    0, 0
		    },
		    sizeof(struct arpt_entry),
		    sizeof(struct arpt_error),
		    0,
		    { 0, 0 }, { } },
	    { { { { ARPT_ALIGN(sizeof(struct arpt_error_target)), ARPT_ERROR_TARGET } },
		{ } },
	      "ERROR"
	    }
    }
};

static struct arpt_table packet_filter
= { { NULL, NULL }, "filter", &initial_table.repl,
    FILTER_VALID_HOOKS, RW_LOCK_UNLOCKED, NULL, THIS_MODULE };

/* The work comes in here from netfilter.c */
static unsigned int arpt_hook(unsigned int hook,
			      struct sk_buff **pskb,
			      const struct net_device *in,
			      const struct net_device *out,
			      int (*okfn)(struct sk_buff *))
{
	return arpt_do_table(pskb, hook, in, out, &packet_filter, NULL);
}

static struct nf_hook_ops arpt_ops[]
= { { { NULL, NULL }, arpt_hook, NF_ARP, NF_ARP_IN, 0 },
    { { NULL, NULL }, arpt_hook, NF_ARP, NF_ARP_OUT, 0 }
};

static int __init init(void)
{
	int ret;

	/* Register table */
	ret = arpt_register_table(&packet_filter);
	if (ret < 0)
		return ret;

	/* Register hooks */
	ret = nf_register_hook(&arpt_ops[0]);
	if (ret < 0)
		goto cleanup_table;

	ret = nf_register_hook(&arpt_ops[1]);
	if (ret < 0)
		goto cleanup_hook0;

	return ret;

cleanup_hook0:
	nf_unregister_hook(&arpt_ops[0]);

cleanup_table:
	arpt_unregister_table(&packet_filter);

	return ret;
}

static void __exit fini(void)
{
	unsigned int i;

	for (i = 0; i < sizeof(arpt_ops)/sizeof(struct nf_hook_ops); i++)
		nf_unregister_hook(&arpt_ops[i]);

	arpt_unregister_table(&packet_filter);
}

module_init(init);
module_exit(fini);
MODULE_LICENSE("GPL");
