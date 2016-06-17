/* IP connection tracking helpers. */
#ifndef _IP_CONNTRACK_HELPER_H
#define _IP_CONNTRACK_HELPER_H
#include <linux/netfilter_ipv4/ip_conntrack.h>

struct module;

/* Reuse expectation when max_expected reached */
#define IP_CT_HELPER_F_REUSE_EXPECT	0x01

struct ip_conntrack_helper
{	
	struct list_head list; 		/* Internal use. */

	const char *name;		/* name of the module */
	unsigned char flags;		/* Flags (see above) */
	struct module *me;		/* pointer to self */
	unsigned int max_expected;	/* Maximum number of concurrent 
					 * expected connections */
	unsigned int timeout;		/* timeout for expecteds */

	/* Mask of things we will help (compared against server response) */
	struct ip_conntrack_tuple tuple;
	struct ip_conntrack_tuple mask;
	
	/* Function to call when data passes; return verdict, or -1 to
           invalidate. */
	int (*help)(const struct iphdr *, size_t len,
		    struct ip_conntrack *ct,
		    enum ip_conntrack_info conntrackinfo);
};

extern int ip_conntrack_helper_register(struct ip_conntrack_helper *);
extern void ip_conntrack_helper_unregister(struct ip_conntrack_helper *);

extern struct ip_conntrack_helper *ip_ct_find_helper(const struct ip_conntrack_tuple *tuple);

/* Add an expected connection: can have more than one per connection */
extern int ip_conntrack_expect_related(struct ip_conntrack *related_to,
				       struct ip_conntrack_expect *exp);
extern int ip_conntrack_change_expect(struct ip_conntrack_expect *expect,
				      struct ip_conntrack_tuple *newtuple);
extern void ip_conntrack_unexpect_related(struct ip_conntrack_expect *exp);

#endif /*_IP_CONNTRACK_HELPER_H*/
