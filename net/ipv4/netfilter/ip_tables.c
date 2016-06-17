/*
 * Packet matching code.
 *
 * Copyright (C) 1999 Paul `Rusty' Russell & Michael J. Neuling
 * Copyright (C) 2009-2002 Netfilter core team <coreteam@netfilter.org>
 *
 * 19 Jan 2002 Harald Welte <laforge@gnumonks.org>
 * 	- increase module usage count as soon as we have rules inside
 * 	  a table
 */
#include <linux/config.h>
#include <linux/cache.h>
#include <linux/skbuff.h>
#include <linux/kmod.h>
#include <linux/vmalloc.h>
#include <linux/netdevice.h>
#include <linux/module.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/icmp.h>
#include <net/ip.h>
#include <asm/uaccess.h>
#include <asm/semaphore.h>
#include <linux/proc_fs.h>

#include <linux/netfilter_ipv4/ip_tables.h>

/*#define DEBUG_IP_FIREWALL*/
/*#define DEBUG_ALLOW_ALL*/ /* Useful for remote debugging */
/*#define DEBUG_IP_FIREWALL_USER*/

#ifdef DEBUG_IP_FIREWALL
#define dprintf(format, args...)  printk(format , ## args)
#else
#define dprintf(format, args...)
#endif

#ifdef DEBUG_IP_FIREWALL_USER
#define duprintf(format, args...) printk(format , ## args)
#else
#define duprintf(format, args...)
#endif

#ifdef CONFIG_NETFILTER_DEBUG
#define IP_NF_ASSERT(x)						\
do {								\
	if (!(x))						\
		printk("IP_NF_ASSERT: %s:%s:%u\n",		\
		       __FUNCTION__, __FILE__, __LINE__);	\
} while(0)
#else
#define IP_NF_ASSERT(x)
#endif
#define SMP_ALIGN(x) (((x) + SMP_CACHE_BYTES-1) & ~(SMP_CACHE_BYTES-1))

/* Mutex protects lists (only traversed in user context). */
static DECLARE_MUTEX(ipt_mutex);

/* Must have mutex */
#define ASSERT_READ_LOCK(x) IP_NF_ASSERT(down_trylock(&ipt_mutex) != 0)
#define ASSERT_WRITE_LOCK(x) IP_NF_ASSERT(down_trylock(&ipt_mutex) != 0)
#include <linux/netfilter_ipv4/lockhelp.h>
#include <linux/netfilter_ipv4/listhelp.h>

#if 0
/* All the better to debug you with... */
#define static
#define inline
#endif

/*
   We keep a set of rules for each CPU, so we can avoid write-locking
   them in the softirq when updating the counters and therefore
   only need to read-lock in the softirq; doing a write_lock_bh() in user
   context stops packets coming through and allows user context to read
   the counters or update the rules.

   To be cache friendly on SMP, we arrange them like so:
   [ n-entries ]
   ... cache-align padding ...
   [ n-entries ]

   Hence the start of any table is given by get_table() below.  */

/* The table itself */
struct ipt_table_info
{
	/* Size per table */
	unsigned int size;
	/* Number of entries: FIXME. --RR */
	unsigned int number;
	/* Initial number of entries. Needed for module usage count */
	unsigned int initial_entries;

	/* Entry points and underflows */
	unsigned int hook_entry[NF_IP_NUMHOOKS];
	unsigned int underflow[NF_IP_NUMHOOKS];

	/* ipt_entry tables: one per CPU */
	char entries[0] ____cacheline_aligned;
};

static LIST_HEAD(ipt_target);
static LIST_HEAD(ipt_match);
static LIST_HEAD(ipt_tables);
#define ADD_COUNTER(c,b,p) do { (c).bcnt += (b); (c).pcnt += (p); } while(0)

#ifdef CONFIG_SMP
#define TABLE_OFFSET(t,p) (SMP_ALIGN((t)->size)*(p))
#else
#define TABLE_OFFSET(t,p) 0
#endif

#if 0
#define down(x) do { printk("DOWN:%u:" #x "\n", __LINE__); down(x); } while(0)
#define down_interruptible(x) ({ int __r; printk("DOWNi:%u:" #x "\n", __LINE__); __r = down_interruptible(x); if (__r != 0) printk("ABORT-DOWNi:%u\n", __LINE__); __r; })
#define up(x) do { printk("UP:%u:" #x "\n", __LINE__); up(x); } while(0)
#endif

/* Returns whether matches rule or not. */
static inline int
ip_packet_match(const struct iphdr *ip,
		const char *indev,
		const char *outdev,
		const struct ipt_ip *ipinfo,
		int isfrag)
{
	size_t i;
	unsigned long ret;

#define FWINV(bool,invflg) ((bool) ^ !!(ipinfo->invflags & invflg))

	if (FWINV((ip->saddr&ipinfo->smsk.s_addr) != ipinfo->src.s_addr,
		  IPT_INV_SRCIP)
	    || FWINV((ip->daddr&ipinfo->dmsk.s_addr) != ipinfo->dst.s_addr,
		     IPT_INV_DSTIP)) {
		dprintf("Source or dest mismatch.\n");

		dprintf("SRC: %u.%u.%u.%u. Mask: %u.%u.%u.%u. Target: %u.%u.%u.%u.%s\n",
			NIPQUAD(ip->saddr),
			NIPQUAD(ipinfo->smsk.s_addr),
			NIPQUAD(ipinfo->src.s_addr),
			ipinfo->invflags & IPT_INV_SRCIP ? " (INV)" : "");
		dprintf("DST: %u.%u.%u.%u Mask: %u.%u.%u.%u Target: %u.%u.%u.%u.%s\n",
			NIPQUAD(ip->daddr),
			NIPQUAD(ipinfo->dmsk.s_addr),
			NIPQUAD(ipinfo->dst.s_addr),
			ipinfo->invflags & IPT_INV_DSTIP ? " (INV)" : "");
		return 0;
	}

	/* Look for ifname matches; this should unroll nicely. */
	for (i = 0, ret = 0; i < IFNAMSIZ/sizeof(unsigned long); i++) {
		ret |= (((const unsigned long *)indev)[i]
			^ ((const unsigned long *)ipinfo->iniface)[i])
			& ((const unsigned long *)ipinfo->iniface_mask)[i];
	}

	if (FWINV(ret != 0, IPT_INV_VIA_IN)) {
		dprintf("VIA in mismatch (%s vs %s).%s\n",
			indev, ipinfo->iniface,
			ipinfo->invflags&IPT_INV_VIA_IN ?" (INV)":"");
		return 0;
	}

	for (i = 0, ret = 0; i < IFNAMSIZ/sizeof(unsigned long); i++) {
		ret |= (((const unsigned long *)outdev)[i]
			^ ((const unsigned long *)ipinfo->outiface)[i])
			& ((const unsigned long *)ipinfo->outiface_mask)[i];
	}

	if (FWINV(ret != 0, IPT_INV_VIA_OUT)) {
		dprintf("VIA out mismatch (%s vs %s).%s\n",
			outdev, ipinfo->outiface,
			ipinfo->invflags&IPT_INV_VIA_OUT ?" (INV)":"");
		return 0;
	}

	/* Check specific protocol */
	if (ipinfo->proto
	    && FWINV(ip->protocol != ipinfo->proto, IPT_INV_PROTO)) {
		dprintf("Packet protocol %hi does not match %hi.%s\n",
			ip->protocol, ipinfo->proto,
			ipinfo->invflags&IPT_INV_PROTO ? " (INV)":"");
		return 0;
	}

	/* If we have a fragment rule but the packet is not a fragment
	 * then we return zero */
	if (FWINV((ipinfo->flags&IPT_F_FRAG) && !isfrag, IPT_INV_FRAG)) {
		dprintf("Fragment rule but not fragment.%s\n",
			ipinfo->invflags & IPT_INV_FRAG ? " (INV)" : "");
		return 0;
	}

	return 1;
}

static inline int
ip_checkentry(const struct ipt_ip *ip)
{
	if (ip->flags & ~IPT_F_MASK) {
		duprintf("Unknown flag bits set: %08X\n",
			 ip->flags & ~IPT_F_MASK);
		return 0;
	}
	if (ip->invflags & ~IPT_INV_MASK) {
		duprintf("Unknown invflag bits set: %08X\n",
			 ip->invflags & ~IPT_INV_MASK);
		return 0;
	}
	return 1;
}

static unsigned int
ipt_error(struct sk_buff **pskb,
	  unsigned int hooknum,
	  const struct net_device *in,
	  const struct net_device *out,
	  const void *targinfo,
	  void *userinfo)
{
	if (net_ratelimit())
		printk("ip_tables: error: `%s'\n", (char *)targinfo);

	return NF_DROP;
}

static inline
int do_match(struct ipt_entry_match *m,
	     const struct sk_buff *skb,
	     const struct net_device *in,
	     const struct net_device *out,
	     int offset,
	     const void *hdr,
	     u_int16_t datalen,
	     int *hotdrop)
{
	/* Stop iteration if it doesn't match */
	if (!m->u.kernel.match->match(skb, in, out, m->data,
				      offset, hdr, datalen, hotdrop))
		return 1;
	else
		return 0;
}

static inline struct ipt_entry *
get_entry(void *base, unsigned int offset)
{
	return (struct ipt_entry *)(base + offset);
}

/* Returns one of the generic firewall policies, like NF_ACCEPT. */
unsigned int
ipt_do_table(struct sk_buff **pskb,
	     unsigned int hook,
	     const struct net_device *in,
	     const struct net_device *out,
	     struct ipt_table *table,
	     void *userdata)
{
	static const char nulldevname[IFNAMSIZ] __attribute__((aligned(sizeof(long)))) = { 0 };
	u_int16_t offset;
	struct iphdr *ip;
	void *protohdr;
	u_int16_t datalen;
	int hotdrop = 0;
	/* Initializing verdict to NF_DROP keeps gcc happy. */
	unsigned int verdict = NF_DROP;
	const char *indev, *outdev;
	void *table_base;
	struct ipt_entry *e, *back;

	/* Initialization */
	ip = (*pskb)->nh.iph;
	protohdr = (u_int32_t *)ip + ip->ihl;
	datalen = (*pskb)->len - ip->ihl * 4;
	indev = in ? in->name : nulldevname;
	outdev = out ? out->name : nulldevname;
	/* We handle fragments by dealing with the first fragment as
	 * if it was a normal packet.  All other fragments are treated
	 * normally, except that they will NEVER match rules that ask
	 * things we don't know, ie. tcp syn flag or ports).  If the
	 * rule is also a fragment-specific rule, non-fragments won't
	 * match it. */
	offset = ntohs(ip->frag_off) & IP_OFFSET;

	read_lock_bh(&table->lock);
	IP_NF_ASSERT(table->valid_hooks & (1 << hook));
	table_base = (void *)table->private->entries
		+ TABLE_OFFSET(table->private,
			       cpu_number_map(smp_processor_id()));
	e = get_entry(table_base, table->private->hook_entry[hook]);

#ifdef CONFIG_NETFILTER_DEBUG
	/* Check noone else using our table */
	if (((struct ipt_entry *)table_base)->comefrom != 0xdead57ac
	    && ((struct ipt_entry *)table_base)->comefrom != 0xeeeeeeec) {
		printk("ASSERT: CPU #%u, %s comefrom(%p) = %X\n",
		       smp_processor_id(),
		       table->name,
		       &((struct ipt_entry *)table_base)->comefrom,
		       ((struct ipt_entry *)table_base)->comefrom);
	}
	((struct ipt_entry *)table_base)->comefrom = 0x57acc001;
#endif

	/* For return from builtin chain */
	back = get_entry(table_base, table->private->underflow[hook]);

	do {
		IP_NF_ASSERT(e);
		IP_NF_ASSERT(back);
		(*pskb)->nfcache |= e->nfcache;
		if (ip_packet_match(ip, indev, outdev, &e->ip, offset)) {
			struct ipt_entry_target *t;

			if (IPT_MATCH_ITERATE(e, do_match,
					      *pskb, in, out,
					      offset, protohdr,
					      datalen, &hotdrop) != 0)
				goto no_match;

			ADD_COUNTER(e->counters, ntohs(ip->tot_len), 1);

			t = ipt_get_target(e);
			IP_NF_ASSERT(t->u.kernel.target);
			/* Standard target? */
			if (!t->u.kernel.target->target) {
				int v;

				v = ((struct ipt_standard_target *)t)->verdict;
				if (v < 0) {
					/* Pop from stack? */
					if (v != IPT_RETURN) {
						verdict = (unsigned)(-v) - 1;
						break;
					}
					e = back;
					back = get_entry(table_base,
							 back->comefrom);
					continue;
				}
				if (table_base + v
				    != (void *)e + e->next_offset) {
					/* Save old back ptr in next entry */
					struct ipt_entry *next
						= (void *)e + e->next_offset;
					next->comefrom
						= (void *)back - table_base;
					/* set back pointer to next entry */
					back = next;
				}

				e = get_entry(table_base, v);
			} else {
				/* Targets which reenter must return
                                   abs. verdicts */
#ifdef CONFIG_NETFILTER_DEBUG
				((struct ipt_entry *)table_base)->comefrom
					= 0xeeeeeeec;
#endif
				verdict = t->u.kernel.target->target(pskb,
								     hook,
								     in, out,
								     t->data,
								     userdata);

#ifdef CONFIG_NETFILTER_DEBUG
				if (((struct ipt_entry *)table_base)->comefrom
				    != 0xeeeeeeec
				    && verdict == IPT_CONTINUE) {
					printk("Target %s reentered!\n",
					       t->u.kernel.target->name);
					verdict = NF_DROP;
				}
				((struct ipt_entry *)table_base)->comefrom
					= 0x57acc001;
#endif
				/* Target might have changed stuff. */
				ip = (*pskb)->nh.iph;
				protohdr = (u_int32_t *)ip + ip->ihl;
				datalen = (*pskb)->len - ip->ihl * 4;

				if (verdict == IPT_CONTINUE)
					e = (void *)e + e->next_offset;
				else
					/* Verdict */
					break;
			}
		} else {

		no_match:
			e = (void *)e + e->next_offset;
		}
	} while (!hotdrop);

#ifdef CONFIG_NETFILTER_DEBUG
	((struct ipt_entry *)table_base)->comefrom = 0xdead57ac;
#endif
	read_unlock_bh(&table->lock);

#ifdef DEBUG_ALLOW_ALL
	return NF_ACCEPT;
#else
	if (hotdrop)
		return NF_DROP;
	else return verdict;
#endif
}

/* If it succeeds, returns element and locks mutex */
static inline void *
find_inlist_lock_noload(struct list_head *head,
			const char *name,
			int *error,
			struct semaphore *mutex)
{
	void *ret;

#if 0
	duprintf("find_inlist: searching for `%s' in %s.\n",
		 name, head == &ipt_target ? "ipt_target"
		 : head == &ipt_match ? "ipt_match"
		 : head == &ipt_tables ? "ipt_tables" : "UNKNOWN");
#endif

	*error = down_interruptible(mutex);
	if (*error != 0)
		return NULL;

	ret = list_named_find(head, name);
	if (!ret) {
		*error = -ENOENT;
		up(mutex);
	}
	return ret;
}

#ifndef CONFIG_KMOD
#define find_inlist_lock(h,n,p,e,m) find_inlist_lock_noload((h),(n),(e),(m))
#else
static void *
find_inlist_lock(struct list_head *head,
		 const char *name,
		 const char *prefix,
		 int *error,
		 struct semaphore *mutex)
{
	void *ret;

	ret = find_inlist_lock_noload(head, name, error, mutex);
	if (!ret) {
		char modulename[IPT_FUNCTION_MAXNAMELEN + strlen(prefix) + 1];
		strcpy(modulename, prefix);
		strcat(modulename, name);
		duprintf("find_inlist: loading `%s'.\n", modulename);
		request_module(modulename);
		ret = find_inlist_lock_noload(head, name, error, mutex);
	}

	return ret;
}
#endif

static inline struct ipt_table *
find_table_lock(const char *name, int *error, struct semaphore *mutex)
{
	return find_inlist_lock(&ipt_tables, name, "iptable_", error, mutex);
}

static inline struct ipt_match *
find_match_lock(const char *name, int *error, struct semaphore *mutex)
{
	return find_inlist_lock(&ipt_match, name, "ipt_", error, mutex);
}

static inline struct ipt_target *
find_target_lock(const char *name, int *error, struct semaphore *mutex)
{
	return find_inlist_lock(&ipt_target, name, "ipt_", error, mutex);
}

/* All zeroes == unconditional rule. */
static inline int
unconditional(const struct ipt_ip *ip)
{
	unsigned int i;

	for (i = 0; i < sizeof(*ip)/sizeof(__u32); i++)
		if (((__u32 *)ip)[i])
			return 0;

	return 1;
}

/* Figures out from what hook each rule can be called: returns 0 if
   there are loops.  Puts hook bitmask in comefrom. */
static int
mark_source_chains(struct ipt_table_info *newinfo, unsigned int valid_hooks)
{
	unsigned int hook;

	/* No recursion; use packet counter to save back ptrs (reset
	   to 0 as we leave), and comefrom to save source hook bitmask */
	for (hook = 0; hook < NF_IP_NUMHOOKS; hook++) {
		unsigned int pos = newinfo->hook_entry[hook];
		struct ipt_entry *e
			= (struct ipt_entry *)(newinfo->entries + pos);

		if (!(valid_hooks & (1 << hook)))
			continue;

		/* Set initial back pointer. */
		e->counters.pcnt = pos;

		for (;;) {
			struct ipt_standard_target *t
				= (void *)ipt_get_target(e);

			if (e->comefrom & (1 << NF_IP_NUMHOOKS)) {
				printk("iptables: loop hook %u pos %u %08X.\n",
				       hook, pos, e->comefrom);
				return 0;
			}
			e->comefrom
				|= ((1 << hook) | (1 << NF_IP_NUMHOOKS));

			/* Unconditional return/END. */
			if (e->target_offset == sizeof(struct ipt_entry)
			    && (strcmp(t->target.u.user.name,
				       IPT_STANDARD_TARGET) == 0)
			    && t->verdict < 0
			    && unconditional(&e->ip)) {
				unsigned int oldpos, size;

				/* Return: backtrack through the last
				   big jump. */
				do {
					e->comefrom ^= (1<<NF_IP_NUMHOOKS);
#ifdef DEBUG_IP_FIREWALL_USER
					if (e->comefrom
					    & (1 << NF_IP_NUMHOOKS)) {
						duprintf("Back unset "
							 "on hook %u "
							 "rule %u\n",
							 hook, pos);
					}
#endif
					oldpos = pos;
					pos = e->counters.pcnt;
					e->counters.pcnt = 0;

					/* We're at the start. */
					if (pos == oldpos)
						goto next;

					e = (struct ipt_entry *)
						(newinfo->entries + pos);
				} while (oldpos == pos + e->next_offset);

				/* Move along one */
				size = e->next_offset;
				e = (struct ipt_entry *)
					(newinfo->entries + pos + size);
				e->counters.pcnt = pos;
				pos += size;
			} else {
				int newpos = t->verdict;

				if (strcmp(t->target.u.user.name,
					   IPT_STANDARD_TARGET) == 0
				    && newpos >= 0) {
					/* This a jump; chase it. */
					duprintf("Jump rule %u -> %u\n",
						 pos, newpos);
				} else {
					/* ... this is a fallthru */
					newpos = pos + e->next_offset;
				}
				e = (struct ipt_entry *)
					(newinfo->entries + newpos);
				e->counters.pcnt = pos;
				pos = newpos;
			}
		}
		next:
		duprintf("Finished chain %u\n", hook);
	}
	return 1;
}

static inline int
cleanup_match(struct ipt_entry_match *m, unsigned int *i)
{
	if (i && (*i)-- == 0)
		return 1;

	if (m->u.kernel.match->destroy)
		m->u.kernel.match->destroy(m->data,
					   m->u.match_size - sizeof(*m));

	if (m->u.kernel.match->me)
		__MOD_DEC_USE_COUNT(m->u.kernel.match->me);

	return 0;
}

static inline int
standard_check(const struct ipt_entry_target *t,
	       unsigned int max_offset)
{
	struct ipt_standard_target *targ = (void *)t;

	/* Check standard info. */
	if (t->u.target_size
	    != IPT_ALIGN(sizeof(struct ipt_standard_target))) {
		duprintf("standard_check: target size %u != %u\n",
			 t->u.target_size,
			 IPT_ALIGN(sizeof(struct ipt_standard_target)));
		return 0;
	}

	if (targ->verdict >= 0
	    && targ->verdict > max_offset - sizeof(struct ipt_entry)) {
		duprintf("ipt_standard_check: bad verdict (%i)\n",
			 targ->verdict);
		return 0;
	}

	if (targ->verdict < -NF_MAX_VERDICT - 1) {
		duprintf("ipt_standard_check: bad negative verdict (%i)\n",
			 targ->verdict);
		return 0;
	}
	return 1;
}

static inline int
check_match(struct ipt_entry_match *m,
	    const char *name,
	    const struct ipt_ip *ip,
	    unsigned int hookmask,
	    unsigned int *i)
{
	int ret;
	struct ipt_match *match;

	match = find_match_lock(m->u.user.name, &ret, &ipt_mutex);
	if (!match) {
		duprintf("check_match: `%s' not found\n", m->u.user.name);
		return ret;
	}
	if (match->me)
		__MOD_INC_USE_COUNT(match->me);
	m->u.kernel.match = match;
	up(&ipt_mutex);

	if (m->u.kernel.match->checkentry
	    && !m->u.kernel.match->checkentry(name, ip, m->data,
					      m->u.match_size - sizeof(*m),
					      hookmask)) {
		if (m->u.kernel.match->me)
			__MOD_DEC_USE_COUNT(m->u.kernel.match->me);
		duprintf("ip_tables: check failed for `%s'.\n",
			 m->u.kernel.match->name);
		return -EINVAL;
	}

	(*i)++;
	return 0;
}

static struct ipt_target ipt_standard_target;

static inline int
check_entry(struct ipt_entry *e, const char *name, unsigned int size,
	    unsigned int *i)
{
	struct ipt_entry_target *t;
	struct ipt_target *target;
	int ret;
	unsigned int j;

	if (!ip_checkentry(&e->ip)) {
		duprintf("ip_tables: ip check failed %p %s.\n", e, name);
		return -EINVAL;
	}

	j = 0;
	ret = IPT_MATCH_ITERATE(e, check_match, name, &e->ip, e->comefrom, &j);
	if (ret != 0)
		goto cleanup_matches;

	t = ipt_get_target(e);
	target = find_target_lock(t->u.user.name, &ret, &ipt_mutex);
	if (!target) {
		duprintf("check_entry: `%s' not found\n", t->u.user.name);
		goto cleanup_matches;
	}
	if (target->me)
		__MOD_INC_USE_COUNT(target->me);
	t->u.kernel.target = target;
	up(&ipt_mutex);

	if (t->u.kernel.target == &ipt_standard_target) {
		if (!standard_check(t, size)) {
			ret = -EINVAL;
			goto cleanup_matches;
		}
	} else if (t->u.kernel.target->checkentry
		   && !t->u.kernel.target->checkentry(name, e, t->data,
						      t->u.target_size
						      - sizeof(*t),
						      e->comefrom)) {
		if (t->u.kernel.target->me)
			__MOD_DEC_USE_COUNT(t->u.kernel.target->me);
		duprintf("ip_tables: check failed for `%s'.\n",
			 t->u.kernel.target->name);
		ret = -EINVAL;
		goto cleanup_matches;
	}

	(*i)++;
	return 0;

 cleanup_matches:
	IPT_MATCH_ITERATE(e, cleanup_match, &j);
	return ret;
}

static inline int
check_entry_size_and_hooks(struct ipt_entry *e,
			   struct ipt_table_info *newinfo,
			   unsigned char *base,
			   unsigned char *limit,
			   const unsigned int *hook_entries,
			   const unsigned int *underflows,
			   unsigned int *i)
{
	unsigned int h;

	if ((unsigned long)e % __alignof__(struct ipt_entry) != 0
	    || (unsigned char *)e + sizeof(struct ipt_entry) >= limit) {
		duprintf("Bad offset %p\n", e);
		return -EINVAL;
	}

	if (e->next_offset
	    < sizeof(struct ipt_entry) + sizeof(struct ipt_entry_target)) {
		duprintf("checking: element %p size %u\n",
			 e, e->next_offset);
		return -EINVAL;
	}

	/* Check hooks & underflows */
	for (h = 0; h < NF_IP_NUMHOOKS; h++) {
		if ((unsigned char *)e - base == hook_entries[h])
			newinfo->hook_entry[h] = hook_entries[h];
		if ((unsigned char *)e - base == underflows[h])
			newinfo->underflow[h] = underflows[h];
	}

	/* FIXME: underflows must be unconditional, standard verdicts
           < 0 (not IPT_RETURN). --RR */

	/* Clear counters and comefrom */
	e->counters = ((struct ipt_counters) { 0, 0 });
	e->comefrom = 0;

	(*i)++;
	return 0;
}

static inline int
cleanup_entry(struct ipt_entry *e, unsigned int *i)
{
	struct ipt_entry_target *t;

	if (i && (*i)-- == 0)
		return 1;

	/* Cleanup all matches */
	IPT_MATCH_ITERATE(e, cleanup_match, NULL);
	t = ipt_get_target(e);
	if (t->u.kernel.target->destroy)
		t->u.kernel.target->destroy(t->data,
					    t->u.target_size - sizeof(*t));
	if (t->u.kernel.target->me)
		__MOD_DEC_USE_COUNT(t->u.kernel.target->me);

	return 0;
}

/* Checks and translates the user-supplied table segment (held in
   newinfo) */
static int
translate_table(const char *name,
		unsigned int valid_hooks,
		struct ipt_table_info *newinfo,
		unsigned int size,
		unsigned int number,
		const unsigned int *hook_entries,
		const unsigned int *underflows)
{
	unsigned int i;
	int ret;

	newinfo->size = size;
	newinfo->number = number;

	/* Init all hooks to impossible value. */
	for (i = 0; i < NF_IP_NUMHOOKS; i++) {
		newinfo->hook_entry[i] = 0xFFFFFFFF;
		newinfo->underflow[i] = 0xFFFFFFFF;
	}

	duprintf("translate_table: size %u\n", newinfo->size);
	i = 0;
	/* Walk through entries, checking offsets. */
	ret = IPT_ENTRY_ITERATE(newinfo->entries, newinfo->size,
				check_entry_size_and_hooks,
				newinfo,
				newinfo->entries,
				newinfo->entries + size,
				hook_entries, underflows, &i);
	if (ret != 0)
		return ret;

	if (i != number) {
		duprintf("translate_table: %u not %u entries\n",
			 i, number);
		return -EINVAL;
	}

	/* Check hooks all assigned */
	for (i = 0; i < NF_IP_NUMHOOKS; i++) {
		/* Only hooks which are valid */
		if (!(valid_hooks & (1 << i)))
			continue;
		if (newinfo->hook_entry[i] == 0xFFFFFFFF) {
			duprintf("Invalid hook entry %u %u\n",
				 i, hook_entries[i]);
			return -EINVAL;
		}
		if (newinfo->underflow[i] == 0xFFFFFFFF) {
			duprintf("Invalid underflow %u %u\n",
				 i, underflows[i]);
			return -EINVAL;
		}
	}

	if (!mark_source_chains(newinfo, valid_hooks))
		return -ELOOP;

	/* Finally, each sanity check must pass */
	i = 0;
	ret = IPT_ENTRY_ITERATE(newinfo->entries, newinfo->size,
				check_entry, name, size, &i);

	if (ret != 0) {
		IPT_ENTRY_ITERATE(newinfo->entries, newinfo->size,
				  cleanup_entry, &i);
		return ret;
	}

	/* And one copy for every other CPU */
	for (i = 1; i < smp_num_cpus; i++) {
		memcpy(newinfo->entries + SMP_ALIGN(newinfo->size)*i,
		       newinfo->entries,
		       SMP_ALIGN(newinfo->size));
	}

	return ret;
}

static struct ipt_table_info *
replace_table(struct ipt_table *table,
	      unsigned int num_counters,
	      struct ipt_table_info *newinfo,
	      int *error)
{
	struct ipt_table_info *oldinfo;

#ifdef CONFIG_NETFILTER_DEBUG
	{
		struct ipt_entry *table_base;
		unsigned int i;

		for (i = 0; i < smp_num_cpus; i++) {
			table_base =
				(void *)newinfo->entries
				+ TABLE_OFFSET(newinfo, i);

			table_base->comefrom = 0xdead57ac;
		}
	}
#endif

	/* Do the substitution. */
	write_lock_bh(&table->lock);
	/* Check inside lock: is the old number correct? */
	if (num_counters != table->private->number) {
		duprintf("num_counters != table->private->number (%u/%u)\n",
			 num_counters, table->private->number);
		write_unlock_bh(&table->lock);
		*error = -EAGAIN;
		return NULL;
	}
	oldinfo = table->private;
	table->private = newinfo;
	newinfo->initial_entries = oldinfo->initial_entries;
	write_unlock_bh(&table->lock);

	return oldinfo;
}

/* Gets counters. */
static inline int
add_entry_to_counter(const struct ipt_entry *e,
		     struct ipt_counters total[],
		     unsigned int *i)
{
	ADD_COUNTER(total[*i], e->counters.bcnt, e->counters.pcnt);

	(*i)++;
	return 0;
}

static void
get_counters(const struct ipt_table_info *t,
	     struct ipt_counters counters[])
{
	unsigned int cpu;
	unsigned int i;

	for (cpu = 0; cpu < smp_num_cpus; cpu++) {
		i = 0;
		IPT_ENTRY_ITERATE(t->entries + TABLE_OFFSET(t, cpu),
				  t->size,
				  add_entry_to_counter,
				  counters,
				  &i);
	}
}

static int
copy_entries_to_user(unsigned int total_size,
		     struct ipt_table *table,
		     void *userptr)
{
	unsigned int off, num, countersize;
	struct ipt_entry *e;
	struct ipt_counters *counters;
	int ret = 0;

	/* We need atomic snapshot of counters: rest doesn't change
	   (other than comefrom, which userspace doesn't care
	   about). */
	countersize = sizeof(struct ipt_counters) * table->private->number;
	counters = vmalloc(countersize);

	if (counters == NULL)
		return -ENOMEM;

	/* First, sum counters... */
	memset(counters, 0, countersize);
	write_lock_bh(&table->lock);
	get_counters(table->private, counters);
	write_unlock_bh(&table->lock);

	/* ... then copy entire thing from CPU 0... */
	if (copy_to_user(userptr, table->private->entries, total_size) != 0) {
		ret = -EFAULT;
		goto free_counters;
	}

	/* FIXME: use iterator macros --RR */
	/* ... then go back and fix counters and names */
	for (off = 0, num = 0; off < total_size; off += e->next_offset, num++){
		unsigned int i;
		struct ipt_entry_match *m;
		struct ipt_entry_target *t;

		e = (struct ipt_entry *)(table->private->entries + off);
		if (copy_to_user(userptr + off
				 + offsetof(struct ipt_entry, counters),
				 &counters[num],
				 sizeof(counters[num])) != 0) {
			ret = -EFAULT;
			goto free_counters;
		}

		for (i = sizeof(struct ipt_entry);
		     i < e->target_offset;
		     i += m->u.match_size) {
			m = (void *)e + i;

			if (copy_to_user(userptr + off + i
					 + offsetof(struct ipt_entry_match,
						    u.user.name),
					 m->u.kernel.match->name,
					 strlen(m->u.kernel.match->name)+1)
			    != 0) {
				ret = -EFAULT;
				goto free_counters;
			}
		}

		t = ipt_get_target(e);
		if (copy_to_user(userptr + off + e->target_offset
				 + offsetof(struct ipt_entry_target,
					    u.user.name),
				 t->u.kernel.target->name,
				 strlen(t->u.kernel.target->name)+1) != 0) {
			ret = -EFAULT;
			goto free_counters;
		}
	}

 free_counters:
	vfree(counters);
	return ret;
}

static int
get_entries(const struct ipt_get_entries *entries,
	    struct ipt_get_entries *uptr)
{
	int ret;
	struct ipt_table *t;

	t = find_table_lock(entries->name, &ret, &ipt_mutex);
	if (t) {
		duprintf("t->private->number = %u\n",
			 t->private->number);
		if (entries->size == t->private->size)
			ret = copy_entries_to_user(t->private->size,
						   t, uptr->entrytable);
		else {
			duprintf("get_entries: I've got %u not %u!\n",
				 t->private->size,
				 entries->size);
			ret = -EINVAL;
		}
		up(&ipt_mutex);
	} else
		duprintf("get_entries: Can't find %s!\n",
			 entries->name);

	return ret;
}

static int
do_replace(void *user, unsigned int len)
{
	int ret;
	struct ipt_replace tmp;
	struct ipt_table *t;
	struct ipt_table_info *newinfo, *oldinfo;
	struct ipt_counters *counters;

	if (copy_from_user(&tmp, user, sizeof(tmp)) != 0)
		return -EFAULT;

	/* Hack: Causes ipchains to give correct error msg --RR */
	if (len != sizeof(tmp) + tmp.size)
		return -ENOPROTOOPT;

	/* Pedantry: prevent them from hitting BUG() in vmalloc.c --RR */
	if ((SMP_ALIGN(tmp.size) >> PAGE_SHIFT) + 2 > num_physpages)
		return -ENOMEM;

	newinfo = vmalloc(sizeof(struct ipt_table_info)
			  + SMP_ALIGN(tmp.size) * smp_num_cpus);
	if (!newinfo)
		return -ENOMEM;

	if (copy_from_user(newinfo->entries, user + sizeof(tmp),
			   tmp.size) != 0) {
		ret = -EFAULT;
		goto free_newinfo;
	}

	counters = vmalloc(tmp.num_counters * sizeof(struct ipt_counters));
	if (!counters) {
		ret = -ENOMEM;
		goto free_newinfo;
	}
	memset(counters, 0, tmp.num_counters * sizeof(struct ipt_counters));

	ret = translate_table(tmp.name, tmp.valid_hooks,
			      newinfo, tmp.size, tmp.num_entries,
			      tmp.hook_entry, tmp.underflow);
	if (ret != 0)
		goto free_newinfo_counters;

	duprintf("ip_tables: Translated table\n");

	t = find_table_lock(tmp.name, &ret, &ipt_mutex);
	if (!t)
		goto free_newinfo_counters_untrans;

	/* You lied! */
	if (tmp.valid_hooks != t->valid_hooks) {
		duprintf("Valid hook crap: %08X vs %08X\n",
			 tmp.valid_hooks, t->valid_hooks);
		ret = -EINVAL;
		goto free_newinfo_counters_untrans_unlock;
	}

	oldinfo = replace_table(t, tmp.num_counters, newinfo, &ret);
	if (!oldinfo)
		goto free_newinfo_counters_untrans_unlock;

	/* Update module usage count based on number of rules */
	duprintf("do_replace: oldnum=%u, initnum=%u, newnum=%u\n",
		oldinfo->number, oldinfo->initial_entries, newinfo->number);
	if (t->me && (oldinfo->number <= oldinfo->initial_entries) &&
 	    (newinfo->number > oldinfo->initial_entries))
		__MOD_INC_USE_COUNT(t->me);
	else if (t->me && (oldinfo->number > oldinfo->initial_entries) &&
	 	 (newinfo->number <= oldinfo->initial_entries))
		__MOD_DEC_USE_COUNT(t->me);

	/* Get the old counters. */
	get_counters(oldinfo, counters);
	/* Decrease module usage counts and free resource */
	IPT_ENTRY_ITERATE(oldinfo->entries, oldinfo->size, cleanup_entry,NULL);
	vfree(oldinfo);
	/* Silent error: too late now. */
	copy_to_user(tmp.counters, counters,
		     sizeof(struct ipt_counters) * tmp.num_counters);
	vfree(counters);
	up(&ipt_mutex);
	return 0;

 free_newinfo_counters_untrans_unlock:
	up(&ipt_mutex);
 free_newinfo_counters_untrans:
	IPT_ENTRY_ITERATE(newinfo->entries, newinfo->size, cleanup_entry,NULL);
 free_newinfo_counters:
	vfree(counters);
 free_newinfo:
	vfree(newinfo);
	return ret;
}

/* We're lazy, and add to the first CPU; overflow works its fey magic
 * and everything is OK. */
static inline int
add_counter_to_entry(struct ipt_entry *e,
		     const struct ipt_counters addme[],
		     unsigned int *i)
{
#if 0
	duprintf("add_counter: Entry %u %lu/%lu + %lu/%lu\n",
		 *i,
		 (long unsigned int)e->counters.pcnt,
		 (long unsigned int)e->counters.bcnt,
		 (long unsigned int)addme[*i].pcnt,
		 (long unsigned int)addme[*i].bcnt);
#endif

	ADD_COUNTER(e->counters, addme[*i].bcnt, addme[*i].pcnt);

	(*i)++;
	return 0;
}

static int
do_add_counters(void *user, unsigned int len)
{
	unsigned int i;
	struct ipt_counters_info tmp, *paddc;
	struct ipt_table *t;
	int ret;

	if (copy_from_user(&tmp, user, sizeof(tmp)) != 0)
		return -EFAULT;

	if (len != sizeof(tmp) + tmp.num_counters*sizeof(struct ipt_counters))
		return -EINVAL;

	paddc = vmalloc(len);
	if (!paddc)
		return -ENOMEM;

	if (copy_from_user(paddc, user, len) != 0) {
		ret = -EFAULT;
		goto free;
	}

	t = find_table_lock(tmp.name, &ret, &ipt_mutex);
	if (!t)
		goto free;

	write_lock_bh(&t->lock);
	if (t->private->number != paddc->num_counters) {
		ret = -EINVAL;
		goto unlock_up_free;
	}

	i = 0;
	IPT_ENTRY_ITERATE(t->private->entries,
			  t->private->size,
			  add_counter_to_entry,
			  paddc->counters,
			  &i);
 unlock_up_free:
	write_unlock_bh(&t->lock);
	up(&ipt_mutex);
 free:
	vfree(paddc);

	return ret;
}

static int
do_ipt_set_ctl(struct sock *sk,	int cmd, void *user, unsigned int len)
{
	int ret;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	switch (cmd) {
	case IPT_SO_SET_REPLACE:
		ret = do_replace(user, len);
		break;

	case IPT_SO_SET_ADD_COUNTERS:
		ret = do_add_counters(user, len);
		break;

	default:
		duprintf("do_ipt_set_ctl:  unknown request %i\n", cmd);
		ret = -EINVAL;
	}

	return ret;
}

static int
do_ipt_get_ctl(struct sock *sk, int cmd, void *user, int *len)
{
	int ret;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	switch (cmd) {
	case IPT_SO_GET_INFO: {
		char name[IPT_TABLE_MAXNAMELEN];
		struct ipt_table *t;

		if (*len != sizeof(struct ipt_getinfo)) {
			duprintf("length %u != %u\n", *len,
				 sizeof(struct ipt_getinfo));
			ret = -EINVAL;
			break;
		}

		if (copy_from_user(name, user, sizeof(name)) != 0) {
			ret = -EFAULT;
			break;
		}
		name[IPT_TABLE_MAXNAMELEN-1] = '\0';
		t = find_table_lock(name, &ret, &ipt_mutex);
		if (t) {
			struct ipt_getinfo info;

			info.valid_hooks = t->valid_hooks;
			memcpy(info.hook_entry, t->private->hook_entry,
			       sizeof(info.hook_entry));
			memcpy(info.underflow, t->private->underflow,
			       sizeof(info.underflow));
			info.num_entries = t->private->number;
			info.size = t->private->size;
			strcpy(info.name, name);

			if (copy_to_user(user, &info, *len) != 0)
				ret = -EFAULT;
			else
				ret = 0;

			up(&ipt_mutex);
		}
	}
	break;

	case IPT_SO_GET_ENTRIES: {
		struct ipt_get_entries get;

		if (*len < sizeof(get)) {
			duprintf("get_entries: %u < %u\n", *len, sizeof(get));
			ret = -EINVAL;
		} else if (copy_from_user(&get, user, sizeof(get)) != 0) {
			ret = -EFAULT;
		} else if (*len != sizeof(struct ipt_get_entries) + get.size) {
			duprintf("get_entries: %u != %u\n", *len,
				 sizeof(struct ipt_get_entries) + get.size);
			ret = -EINVAL;
		} else
			ret = get_entries(&get, user);
		break;
	}

	default:
		duprintf("do_ipt_get_ctl: unknown request %i\n", cmd);
		ret = -EINVAL;
	}

	return ret;
}

/* Registration hooks for targets. */
int
ipt_register_target(struct ipt_target *target)
{
	int ret;

	MOD_INC_USE_COUNT;
	ret = down_interruptible(&ipt_mutex);
	if (ret != 0) {
		MOD_DEC_USE_COUNT;
		return ret;
	}
	if (!list_named_insert(&ipt_target, target)) {
		duprintf("ipt_register_target: `%s' already in list!\n",
			 target->name);
		ret = -EINVAL;
		MOD_DEC_USE_COUNT;
	}
	up(&ipt_mutex);
	return ret;
}

void
ipt_unregister_target(struct ipt_target *target)
{
	down(&ipt_mutex);
	LIST_DELETE(&ipt_target, target);
	up(&ipt_mutex);
	MOD_DEC_USE_COUNT;
}

int
ipt_register_match(struct ipt_match *match)
{
	int ret;

	MOD_INC_USE_COUNT;
	ret = down_interruptible(&ipt_mutex);
	if (ret != 0) {
		MOD_DEC_USE_COUNT;
		return ret;
	}
	if (!list_named_insert(&ipt_match, match)) {
		duprintf("ipt_register_match: `%s' already in list!\n",
			 match->name);
		MOD_DEC_USE_COUNT;
		ret = -EINVAL;
	}
	up(&ipt_mutex);

	return ret;
}

void
ipt_unregister_match(struct ipt_match *match)
{
	down(&ipt_mutex);
	LIST_DELETE(&ipt_match, match);
	up(&ipt_mutex);
	MOD_DEC_USE_COUNT;
}

int ipt_register_table(struct ipt_table *table)
{
	int ret;
	struct ipt_table_info *newinfo;
	static struct ipt_table_info bootstrap
		= { 0, 0, 0, { 0 }, { 0 }, { } };

	MOD_INC_USE_COUNT;
	newinfo = vmalloc(sizeof(struct ipt_table_info)
			  + SMP_ALIGN(table->table->size) * smp_num_cpus);
	if (!newinfo) {
		ret = -ENOMEM;
		MOD_DEC_USE_COUNT;
		return ret;
	}
	memcpy(newinfo->entries, table->table->entries, table->table->size);

	ret = translate_table(table->name, table->valid_hooks,
			      newinfo, table->table->size,
			      table->table->num_entries,
			      table->table->hook_entry,
			      table->table->underflow);
	if (ret != 0) {
		vfree(newinfo);
		MOD_DEC_USE_COUNT;
		return ret;
	}

	ret = down_interruptible(&ipt_mutex);
	if (ret != 0) {
		vfree(newinfo);
		MOD_DEC_USE_COUNT;
		return ret;
	}

	/* Don't autoload: we'd eat our tail... */
	if (list_named_find(&ipt_tables, table->name)) {
		ret = -EEXIST;
		goto free_unlock;
	}

	/* Simplifies replace_table code. */
	table->private = &bootstrap;
	if (!replace_table(table, 0, newinfo, &ret))
		goto free_unlock;

	duprintf("table->private->number = %u\n",
		 table->private->number);
	
	/* save number of initial entries */
	table->private->initial_entries = table->private->number;

	table->lock = RW_LOCK_UNLOCKED;
	list_prepend(&ipt_tables, table);

 unlock:
	up(&ipt_mutex);
	return ret;

 free_unlock:
	vfree(newinfo);
	MOD_DEC_USE_COUNT;
	goto unlock;
}

void ipt_unregister_table(struct ipt_table *table)
{
	down(&ipt_mutex);
	LIST_DELETE(&ipt_tables, table);
	up(&ipt_mutex);

	/* Decrease module usage counts and free resources */
	IPT_ENTRY_ITERATE(table->private->entries, table->private->size,
			  cleanup_entry, NULL);
	vfree(table->private);
	MOD_DEC_USE_COUNT;
}

/* Returns 1 if the port is matched by the range, 0 otherwise */
static inline int
port_match(u_int16_t min, u_int16_t max, u_int16_t port, int invert)
{
	int ret;

	ret = (port >= min && port <= max) ^ invert;
	return ret;
}

static int
tcp_find_option(u_int8_t option,
		const struct tcphdr *tcp,
		u_int16_t datalen,
		int invert,
		int *hotdrop)
{
	unsigned int i = sizeof(struct tcphdr);
	const u_int8_t *opt = (u_int8_t *)tcp;

	duprintf("tcp_match: finding option\n");
	/* If we don't have the whole header, drop packet. */
	if (tcp->doff * 4 < sizeof(struct tcphdr) ||
	    tcp->doff * 4 > datalen) {
		*hotdrop = 1;
		return 0;
	}

	while (i < tcp->doff * 4) {
		if (opt[i] == option) return !invert;
		if (opt[i] < 2) i++;
		else i += opt[i+1]?:1;
	}

	return invert;
}

static int
tcp_match(const struct sk_buff *skb,
	  const struct net_device *in,
	  const struct net_device *out,
	  const void *matchinfo,
	  int offset,
	  const void *hdr,
	  u_int16_t datalen,
	  int *hotdrop)
{
	const struct tcphdr *tcp = hdr;
	const struct ipt_tcp *tcpinfo = matchinfo;

	/* To quote Alan:

	   Don't allow a fragment of TCP 8 bytes in. Nobody normal
	   causes this. Its a cracker trying to break in by doing a
	   flag overwrite to pass the direction checks.
	*/

	if (offset == 1) {
		duprintf("Dropping evil TCP offset=1 frag.\n");
		*hotdrop = 1;
		return 0;
	} else if (offset == 0 && datalen < sizeof(struct tcphdr)) {
		/* We've been asked to examine this packet, and we
		   can't.  Hence, no choice but to drop. */
		duprintf("Dropping evil TCP offset=0 tinygram.\n");
		*hotdrop = 1;
		return 0;
	}

	/* FIXME: Try tcp doff >> packet len against various stacks --RR */

#define FWINVTCP(bool,invflg) ((bool) ^ !!(tcpinfo->invflags & invflg))

	/* Must not be a fragment. */
	return !offset
		&& port_match(tcpinfo->spts[0], tcpinfo->spts[1],
			      ntohs(tcp->source),
			      !!(tcpinfo->invflags & IPT_TCP_INV_SRCPT))
		&& port_match(tcpinfo->dpts[0], tcpinfo->dpts[1],
			      ntohs(tcp->dest),
			      !!(tcpinfo->invflags & IPT_TCP_INV_DSTPT))
		&& FWINVTCP((((unsigned char *)tcp)[13]
			     & tcpinfo->flg_mask)
			    == tcpinfo->flg_cmp,
			    IPT_TCP_INV_FLAGS)
		&& (!tcpinfo->option
		    || tcp_find_option(tcpinfo->option, tcp, datalen,
				       tcpinfo->invflags
				       & IPT_TCP_INV_OPTION,
				       hotdrop));
}

/* Called when user tries to insert an entry of this type. */
static int
tcp_checkentry(const char *tablename,
	       const struct ipt_ip *ip,
	       void *matchinfo,
	       unsigned int matchsize,
	       unsigned int hook_mask)
{
	const struct ipt_tcp *tcpinfo = matchinfo;

	/* Must specify proto == TCP, and no unknown invflags */
	return ip->proto == IPPROTO_TCP
		&& !(ip->invflags & IPT_INV_PROTO)
		&& matchsize == IPT_ALIGN(sizeof(struct ipt_tcp))
		&& !(tcpinfo->invflags & ~IPT_TCP_INV_MASK);
}

static int
udp_match(const struct sk_buff *skb,
	  const struct net_device *in,
	  const struct net_device *out,
	  const void *matchinfo,
	  int offset,
	  const void *hdr,
	  u_int16_t datalen,
	  int *hotdrop)
{
	const struct udphdr *udp = hdr;
	const struct ipt_udp *udpinfo = matchinfo;

	if (offset == 0 && datalen < sizeof(struct udphdr)) {
		/* We've been asked to examine this packet, and we
		   can't.  Hence, no choice but to drop. */
		duprintf("Dropping evil UDP tinygram.\n");
		*hotdrop = 1;
		return 0;
	}

	/* Must not be a fragment. */
	return !offset
		&& port_match(udpinfo->spts[0], udpinfo->spts[1],
			      ntohs(udp->source),
			      !!(udpinfo->invflags & IPT_UDP_INV_SRCPT))
		&& port_match(udpinfo->dpts[0], udpinfo->dpts[1],
			      ntohs(udp->dest),
			      !!(udpinfo->invflags & IPT_UDP_INV_DSTPT));
}

/* Called when user tries to insert an entry of this type. */
static int
udp_checkentry(const char *tablename,
	       const struct ipt_ip *ip,
	       void *matchinfo,
	       unsigned int matchinfosize,
	       unsigned int hook_mask)
{
	const struct ipt_udp *udpinfo = matchinfo;

	/* Must specify proto == UDP, and no unknown invflags */
	if (ip->proto != IPPROTO_UDP || (ip->invflags & IPT_INV_PROTO)) {
		duprintf("ipt_udp: Protocol %u != %u\n", ip->proto,
			 IPPROTO_UDP);
		return 0;
	}
	if (matchinfosize != IPT_ALIGN(sizeof(struct ipt_udp))) {
		duprintf("ipt_udp: matchsize %u != %u\n",
			 matchinfosize, IPT_ALIGN(sizeof(struct ipt_udp)));
		return 0;
	}
	if (udpinfo->invflags & ~IPT_UDP_INV_MASK) {
		duprintf("ipt_udp: unknown flags %X\n",
			 udpinfo->invflags);
		return 0;
	}

	return 1;
}

/* Returns 1 if the type and code is matched by the range, 0 otherwise */
static inline int
icmp_type_code_match(u_int8_t test_type, u_int8_t min_code, u_int8_t max_code,
		     u_int8_t type, u_int8_t code,
		     int invert)
{
	return ((test_type == 0xFF) || (type == test_type && code >= min_code && code <= max_code))
		^ invert;
}

static int
icmp_match(const struct sk_buff *skb,
	   const struct net_device *in,
	   const struct net_device *out,
	   const void *matchinfo,
	   int offset,
	   const void *hdr,
	   u_int16_t datalen,
	   int *hotdrop)
{
	const struct icmphdr *icmp = hdr;
	const struct ipt_icmp *icmpinfo = matchinfo;

	if (offset == 0 && datalen < 2) {
		/* We've been asked to examine this packet, and we
		   can't.  Hence, no choice but to drop. */
		duprintf("Dropping evil ICMP tinygram.\n");
		*hotdrop = 1;
		return 0;
	}

	/* Must not be a fragment. */
	return !offset
		&& icmp_type_code_match(icmpinfo->type,
					icmpinfo->code[0],
					icmpinfo->code[1],
					icmp->type, icmp->code,
					!!(icmpinfo->invflags&IPT_ICMP_INV));
}

/* Called when user tries to insert an entry of this type. */
static int
icmp_checkentry(const char *tablename,
	   const struct ipt_ip *ip,
	   void *matchinfo,
	   unsigned int matchsize,
	   unsigned int hook_mask)
{
	const struct ipt_icmp *icmpinfo = matchinfo;

	/* Must specify proto == ICMP, and no unknown invflags */
	return ip->proto == IPPROTO_ICMP
		&& !(ip->invflags & IPT_INV_PROTO)
		&& matchsize == IPT_ALIGN(sizeof(struct ipt_icmp))
		&& !(icmpinfo->invflags & ~IPT_ICMP_INV);
}

/* The built-in targets: standard (NULL) and error. */
static struct ipt_target ipt_standard_target
= { { NULL, NULL }, IPT_STANDARD_TARGET, NULL, NULL, NULL };
static struct ipt_target ipt_error_target
= { { NULL, NULL }, IPT_ERROR_TARGET, ipt_error, NULL, NULL };

static struct nf_sockopt_ops ipt_sockopts
= { { NULL, NULL }, PF_INET, IPT_BASE_CTL, IPT_SO_SET_MAX+1, do_ipt_set_ctl,
    IPT_BASE_CTL, IPT_SO_GET_MAX+1, do_ipt_get_ctl, 0, NULL  };

static struct ipt_match tcp_matchstruct
= { { NULL, NULL }, "tcp", &tcp_match, &tcp_checkentry, NULL };
static struct ipt_match udp_matchstruct
= { { NULL, NULL }, "udp", &udp_match, &udp_checkentry, NULL };
static struct ipt_match icmp_matchstruct
= { { NULL, NULL }, "icmp", &icmp_match, &icmp_checkentry, NULL };

#ifdef CONFIG_PROC_FS
static inline int print_name(const char *i,
			     off_t start_offset, char *buffer, int length,
			     off_t *pos, unsigned int *count)
{
	if ((*count)++ >= start_offset) {
		unsigned int namelen;

		namelen = sprintf(buffer + *pos, "%s\n",
				  i + sizeof(struct list_head));
		if (*pos + namelen > length) {
			/* Stop iterating */
			return 1;
		}
		*pos += namelen;
	}
	return 0;
}

static int ipt_get_tables(char *buffer, char **start, off_t offset, int length)
{
	off_t pos = 0;
	unsigned int count = 0;

	if (down_interruptible(&ipt_mutex) != 0)
		return 0;

	LIST_FIND(&ipt_tables, print_name, void *,
		  offset, buffer, length, &pos, &count);

	up(&ipt_mutex);

	/* `start' hack - see fs/proc/generic.c line ~105 */
	*start=(char *)((unsigned long)count-offset);
	return pos;
}

static int ipt_get_targets(char *buffer, char **start, off_t offset, int length)
{
	off_t pos = 0;
	unsigned int count = 0;

	if (down_interruptible(&ipt_mutex) != 0)
		return 0;

	LIST_FIND(&ipt_target, print_name, void *,
		  offset, buffer, length, &pos, &count);
	
	up(&ipt_mutex);

	*start = (char *)((unsigned long)count - offset);
	return pos;
}

static int ipt_get_matches(char *buffer, char **start, off_t offset, int length)
{
	off_t pos = 0;
	unsigned int count = 0;

	if (down_interruptible(&ipt_mutex) != 0)
		return 0;
	
	LIST_FIND(&ipt_match, print_name, void *,
		  offset, buffer, length, &pos, &count);

	up(&ipt_mutex);

	*start = (char *)((unsigned long)count - offset);
	return pos;
}

static struct { char *name; get_info_t *get_info; } ipt_proc_entry[] =
{ { "ip_tables_names", ipt_get_tables },
  { "ip_tables_targets", ipt_get_targets },
  { "ip_tables_matches", ipt_get_matches },
  { NULL, NULL} };
#endif /*CONFIG_PROC_FS*/

static int __init init(void)
{
	int ret;

	/* Noone else will be downing sem now, so we won't sleep */
	down(&ipt_mutex);
	list_append(&ipt_target, &ipt_standard_target);
	list_append(&ipt_target, &ipt_error_target);
	list_append(&ipt_match, &tcp_matchstruct);
	list_append(&ipt_match, &udp_matchstruct);
	list_append(&ipt_match, &icmp_matchstruct);
	up(&ipt_mutex);

	/* Register setsockopt */
	ret = nf_register_sockopt(&ipt_sockopts);
	if (ret < 0) {
		duprintf("Unable to register sockopts.\n");
		return ret;
	}

#ifdef CONFIG_PROC_FS
	{
	struct proc_dir_entry *proc;
	int i;

	for (i = 0; ipt_proc_entry[i].name; i++) {
		proc = proc_net_create(ipt_proc_entry[i].name, 0,
				       ipt_proc_entry[i].get_info);
		if (!proc) {
			while (--i >= 0)
				proc_net_remove(ipt_proc_entry[i].name);
			nf_unregister_sockopt(&ipt_sockopts);
			return -ENOMEM;
		}
		proc->owner = THIS_MODULE;
	}
	}
#endif

	printk("ip_tables: (C) 2000-2002 Netfilter core team\n");
	return 0;
}

static void __exit fini(void)
{
	nf_unregister_sockopt(&ipt_sockopts);
#ifdef CONFIG_PROC_FS
	{
	int i;
	for (i = 0; ipt_proc_entry[i].name; i++)
		proc_net_remove(ipt_proc_entry[i].name);
	}
#endif
}

EXPORT_SYMBOL(ipt_register_table);
EXPORT_SYMBOL(ipt_unregister_table);
EXPORT_SYMBOL(ipt_register_match);
EXPORT_SYMBOL(ipt_unregister_match);
EXPORT_SYMBOL(ipt_do_table);
EXPORT_SYMBOL(ipt_register_target);
EXPORT_SYMBOL(ipt_unregister_target);

module_init(init);
module_exit(fini);
MODULE_LICENSE("GPL");
