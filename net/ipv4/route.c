/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		ROUTE - implementation of the IP router.
 *
 * Version:	$Id: route.c,v 1.102.2.1 2002/01/12 07:43:57 davem Exp $
 *
 * Authors:	Ross Biro, <bir7@leland.Stanford.Edu>
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *		Alan Cox, <gw4pts@gw4pts.ampr.org>
 *		Linus Torvalds, <Linus.Torvalds@helsinki.fi>
 *		Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 * Fixes:
 *		Alan Cox	:	Verify area fixes.
 *		Alan Cox	:	cli() protects routing changes
 *		Rui Oliveira	:	ICMP routing table updates
 *		(rco@di.uminho.pt)	Routing table insertion and update
 *		Linus Torvalds	:	Rewrote bits to be sensible
 *		Alan Cox	:	Added BSD route gw semantics
 *		Alan Cox	:	Super /proc >4K 
 *		Alan Cox	:	MTU in route table
 *		Alan Cox	: 	MSS actually. Also added the window
 *					clamper.
 *		Sam Lantinga	:	Fixed route matching in rt_del()
 *		Alan Cox	:	Routing cache support.
 *		Alan Cox	:	Removed compatibility cruft.
 *		Alan Cox	:	RTF_REJECT support.
 *		Alan Cox	:	TCP irtt support.
 *		Jonathan Naylor	:	Added Metric support.
 *	Miquel van Smoorenburg	:	BSD API fixes.
 *	Miquel van Smoorenburg	:	Metrics.
 *		Alan Cox	:	Use __u32 properly
 *		Alan Cox	:	Aligned routing errors more closely with BSD
 *					our system is still very different.
 *		Alan Cox	:	Faster /proc handling
 *	Alexey Kuznetsov	:	Massive rework to support tree based routing,
 *					routing caches and better behaviour.
 *		
 *		Olaf Erb	:	irtt wasn't being copied right.
 *		Bjorn Ekwall	:	Kerneld route support.
 *		Alan Cox	:	Multicast fixed (I hope)
 * 		Pavel Krauz	:	Limited broadcast fixed
 *		Mike McLagan	:	Routing by source
 *	Alexey Kuznetsov	:	End of old history. Splitted to fib.c and
 *					route.c and rewritten from scratch.
 *		Andi Kleen	:	Load-limit warning messages.
 *	Vitaly E. Lavrov	:	Transparent proxy revived after year coma.
 *	Vitaly E. Lavrov	:	Race condition in ip_route_input_slow.
 *	Tobias Ringstrom	:	Uninitialized res.type in ip_route_output_slow.
 *	Vladimir V. Ivanov	:	IP rule info (flowid) is really useful.
 *		Marc Boucher	:	routing by fwmark
 *	Robert Olsson		:	Added rt_cache statistics
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */

#include <linux/config.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/bitops.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/errno.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/rtnetlink.h>
#include <linux/inetdevice.h>
#include <linux/igmp.h>
#include <linux/pkt_sched.h>
#include <linux/mroute.h>
#include <linux/netfilter_ipv4.h>
#include <linux/random.h>
#include <linux/jhash.h>
#include <net/protocol.h>
#include <net/ip.h>
#include <net/route.h>
#include <net/inetpeer.h>
#include <net/sock.h>
#include <net/ip_fib.h>
#include <net/arp.h>
#include <net/tcp.h>
#include <net/icmp.h>
#ifdef CONFIG_SYSCTL
#include <linux/sysctl.h>
#endif

#define IP_MAX_MTU	0xFFF0

#define RT_GC_TIMEOUT (300*HZ)

int ip_rt_min_delay		= 2 * HZ;
int ip_rt_max_delay		= 10 * HZ;
int ip_rt_max_size;
int ip_rt_gc_timeout		= RT_GC_TIMEOUT;
int ip_rt_gc_interval		= 60 * HZ;
int ip_rt_gc_min_interval	= HZ / 2;
int ip_rt_redirect_number	= 9;
int ip_rt_redirect_load		= HZ / 50;
int ip_rt_redirect_silence	= ((HZ / 50) << (9 + 1));
int ip_rt_error_cost		= HZ;
int ip_rt_error_burst		= 5 * HZ;
int ip_rt_gc_elasticity		= 8;
int ip_rt_mtu_expires		= 10 * 60 * HZ;
int ip_rt_min_pmtu		= 512 + 20 + 20;
int ip_rt_min_advmss		= 256;
int ip_rt_secret_interval	= 10 * 60 * HZ;
static unsigned long rt_deadline;

#define RTprint(a...)	printk(KERN_DEBUG a)

static struct timer_list rt_flush_timer;
static struct timer_list rt_periodic_timer;
static struct timer_list rt_secret_timer;

/*
 *	Interface to generic destination cache.
 */

static struct dst_entry *ipv4_dst_check(struct dst_entry *dst, u32 cookie);
static struct dst_entry *ipv4_dst_reroute(struct dst_entry *dst,
					   struct sk_buff *skb);
static void		 ipv4_dst_destroy(struct dst_entry *dst);
static struct dst_entry *ipv4_negative_advice(struct dst_entry *dst);
static void		 ipv4_link_failure(struct sk_buff *skb);
static int rt_garbage_collect(void);


struct dst_ops ipv4_dst_ops = {
	family:			AF_INET,
	protocol:		__constant_htons(ETH_P_IP),
	gc:			rt_garbage_collect,
	check:			ipv4_dst_check,
	reroute:		ipv4_dst_reroute,
	destroy:		ipv4_dst_destroy,
	negative_advice:	ipv4_negative_advice,
	link_failure:		ipv4_link_failure,
	entry_size:		sizeof(struct rtable),
};

#define ECN_OR_COST(class)	TC_PRIO_##class

__u8 ip_tos2prio[16] = {
	TC_PRIO_BESTEFFORT,
	ECN_OR_COST(FILLER),
	TC_PRIO_BESTEFFORT,
	ECN_OR_COST(BESTEFFORT),
	TC_PRIO_BULK,
	ECN_OR_COST(BULK),
	TC_PRIO_BULK,
	ECN_OR_COST(BULK),
	TC_PRIO_INTERACTIVE,
	ECN_OR_COST(INTERACTIVE),
	TC_PRIO_INTERACTIVE,
	ECN_OR_COST(INTERACTIVE),
	TC_PRIO_INTERACTIVE_BULK,
	ECN_OR_COST(INTERACTIVE_BULK),
	TC_PRIO_INTERACTIVE_BULK,
	ECN_OR_COST(INTERACTIVE_BULK)
};


/*
 * Route cache.
 */

/* The locking scheme is rather straight forward:
 *
 * 1) A BH protected rwlocks protect buckets of the central route hash.
 * 2) Only writers remove entries, and they hold the lock
 *    as they look at rtable reference counts.
 * 3) Only readers acquire references to rtable entries,
 *    they do so with atomic increments and with the
 *    lock held.
 */

struct rt_hash_bucket {
	struct rtable	*chain;
	rwlock_t	lock;
} __attribute__((__aligned__(8)));

static struct rt_hash_bucket 	*rt_hash_table;
static unsigned			rt_hash_mask;
static int			rt_hash_log;
static unsigned int		rt_hash_rnd;

struct rt_cache_stat rt_cache_stat[NR_CPUS];

static int rt_intern_hash(unsigned hash, struct rtable *rth,
				struct rtable **res);

static unsigned int rt_hash_code(u32 daddr, u32 saddr, u8 tos)
{
	return (jhash_3words(daddr, saddr, (u32) tos, rt_hash_rnd)
		& rt_hash_mask);
}

static int rt_cache_get_info(char *buffer, char **start, off_t offset,
				int length)
{
	int len = 0;
	off_t pos = 128;
	char temp[256];
	struct rtable *r;
	int i;

	if (offset < 128) {
		sprintf(buffer, "%-127s\n",
			"Iface\tDestination\tGateway \tFlags\t\tRefCnt\tUse\t"
			"Metric\tSource\t\tMTU\tWindow\tIRTT\tTOS\tHHRef\t"
			"HHUptod\tSpecDst");
		len = 128;
  	}
	
	for (i = rt_hash_mask; i >= 0; i--) {
		read_lock_bh(&rt_hash_table[i].lock);
		for (r = rt_hash_table[i].chain; r; r = r->u.rt_next) {
			/*
			 *	Spin through entries until we are ready
			 */
			pos += 128;

			if (pos <= offset) {
				len = 0;
				continue;
			}
			sprintf(temp, "%s\t%08lX\t%08lX\t%8X\t%d\t%u\t%d\t"
				"%08lX\t%d\t%u\t%u\t%02X\t%d\t%1d\t%08X",
				r->u.dst.dev ? r->u.dst.dev->name : "*",
				(unsigned long)r->rt_dst,
				(unsigned long)r->rt_gateway,
				r->rt_flags,
				atomic_read(&r->u.dst.__refcnt),
				r->u.dst.__use,
				0,
				(unsigned long)r->rt_src,
				(r->u.dst.advmss ?
				 (int) r->u.dst.advmss + 40 : 0),
				r->u.dst.window,
				(int)((r->u.dst.rtt >> 3) + r->u.dst.rttvar),
				r->key.tos,
				r->u.dst.hh ?
					atomic_read(&r->u.dst.hh->hh_refcnt) :
					-1,
				r->u.dst.hh ?
			       		(r->u.dst.hh->hh_output ==
					 dev_queue_xmit) : 0,
				r->rt_spec_dst);
			sprintf(buffer + len, "%-127s\n", temp);
			len += 128;
			if (pos >= offset+length) {
				read_unlock_bh(&rt_hash_table[i].lock);
				goto done;
			}
		}
		read_unlock_bh(&rt_hash_table[i].lock);
        }

done:
  	*start = buffer + len - (pos - offset);
  	len = pos - offset;
  	if (len > length)
  		len = length;
  	return len;
}

static int rt_cache_stat_get_info(char *buffer, char **start, off_t offset, int length)
{
	unsigned int dst_entries = atomic_read(&ipv4_dst_ops.entries);
	int i, lcpu;
	int len = 0;

        for (lcpu = 0; lcpu < smp_num_cpus; lcpu++) {
                i = cpu_logical_map(lcpu);

		len += sprintf(buffer+len, "%08x  %08x %08x %08x %08x %08x %08x %08x  %08x %08x %08x %08x %08x %08x %08x %08x %08x \n",
			       dst_entries,		       
			       rt_cache_stat[i].in_hit,
			       rt_cache_stat[i].in_slow_tot,
			       rt_cache_stat[i].in_slow_mc,
			       rt_cache_stat[i].in_no_route,
			       rt_cache_stat[i].in_brd,
			       rt_cache_stat[i].in_martian_dst,
			       rt_cache_stat[i].in_martian_src,

			       rt_cache_stat[i].out_hit,
			       rt_cache_stat[i].out_slow_tot,
			       rt_cache_stat[i].out_slow_mc, 

			       rt_cache_stat[i].gc_total,
			       rt_cache_stat[i].gc_ignored,
			       rt_cache_stat[i].gc_goal_miss,
			       rt_cache_stat[i].gc_dst_overflow,
			       rt_cache_stat[i].in_hlist_search,
			       rt_cache_stat[i].out_hlist_search

			);
	}
	len -= offset;

	if (len > length)
		len = length;
	if (len < 0)
		len = 0;

	*start = buffer + offset;
  	return len;
}
  
static __inline__ void rt_free(struct rtable *rt)
{
	dst_free(&rt->u.dst);
}

static __inline__ void rt_drop(struct rtable *rt)
{
	ip_rt_put(rt);
	dst_free(&rt->u.dst);
}

static __inline__ int rt_fast_clean(struct rtable *rth)
{
	/* Kill broadcast/multicast entries very aggresively, if they
	   collide in hash table with more useful entries */
	return (rth->rt_flags & (RTCF_BROADCAST | RTCF_MULTICAST)) &&
		rth->key.iif && rth->u.rt_next;
}

static __inline__ int rt_valuable(struct rtable *rth)
{
	return (rth->rt_flags & (RTCF_REDIRECTED | RTCF_NOTIFY)) ||
		rth->u.dst.expires;
}

static __inline__ int rt_may_expire(struct rtable *rth, unsigned long tmo1, unsigned long tmo2)
{
	unsigned long age;
	int ret = 0;

	if (atomic_read(&rth->u.dst.__refcnt))
		goto out;

	ret = 1;
	if (rth->u.dst.expires &&
	    time_after_eq(jiffies, rth->u.dst.expires))
		goto out;

	age = jiffies - rth->u.dst.lastuse;
	ret = 0;
	if ((age <= tmo1 && !rt_fast_clean(rth)) ||
	    (age <= tmo2 && rt_valuable(rth)))
		goto out;
	ret = 1;
out:	return ret;
}

/* Bits of score are:
 * 31: very valuable
 * 30: not quite useless
 * 29..0: usage counter
 */
static inline u32 rt_score(struct rtable *rt)
{
	u32 score = jiffies - rt->u.dst.lastuse;

	score = ~score & ~(3<<30);

	if (rt_valuable(rt))
		score |= (1<<31);

	if (!rt->key.iif ||
	    !(rt->rt_flags & (RTCF_BROADCAST|RTCF_MULTICAST|RTCF_LOCAL)))
		score |= (1<<30);

	return score;
}

/* This runs via a timer and thus is always in BH context. */
static void SMP_TIMER_NAME(rt_check_expire)(unsigned long dummy)
{
	static int rover;
	int i = rover, t;
	struct rtable *rth, **rthp;
	unsigned long now = jiffies;

	for (t = ip_rt_gc_interval << rt_hash_log; t >= 0;
	     t -= ip_rt_gc_timeout) {
		unsigned long tmo = ip_rt_gc_timeout;

		i = (i + 1) & rt_hash_mask;
		rthp = &rt_hash_table[i].chain;

		write_lock(&rt_hash_table[i].lock);
		while ((rth = *rthp) != NULL) {
			if (rth->u.dst.expires) {
				/* Entry is expired even if it is in use */
				if (time_before_eq(now, rth->u.dst.expires)) {
					tmo >>= 1;
					rthp = &rth->u.rt_next;
					continue;
				}
			} else if (!rt_may_expire(rth, tmo, ip_rt_gc_timeout)) {
				tmo >>= 1;
				rthp = &rth->u.rt_next;
				continue;
			}

			/* Cleanup aged off entries. */
			*rthp = rth->u.rt_next;
			rt_free(rth);
		}
		write_unlock(&rt_hash_table[i].lock);

		/* Fallback loop breaker. */
		if (time_after(jiffies, now))
			break;
	}
	rover = i;
	mod_timer(&rt_periodic_timer, now + ip_rt_gc_interval);
}

SMP_TIMER_DEFINE(rt_check_expire, rt_gc_task);

/* This can run from both BH and non-BH contexts, the latter
 * in the case of a forced flush event.
 */
static void SMP_TIMER_NAME(rt_run_flush)(unsigned long dummy)
{
	int i;
	struct rtable *rth, *next;

	rt_deadline = 0;

	get_random_bytes(&rt_hash_rnd, 4);

	for (i = rt_hash_mask; i >= 0; i--) {
		write_lock_bh(&rt_hash_table[i].lock);
		rth = rt_hash_table[i].chain;
		if (rth)
			rt_hash_table[i].chain = NULL;
		write_unlock_bh(&rt_hash_table[i].lock);

		for (; rth; rth = next) {
			next = rth->u.rt_next;
			rt_free(rth);
		}
	}
}

SMP_TIMER_DEFINE(rt_run_flush, rt_cache_flush_task);
  
static spinlock_t rt_flush_lock = SPIN_LOCK_UNLOCKED;

void rt_cache_flush(int delay)
{
	unsigned long now = jiffies;
	int user_mode = !in_softirq();

	if (delay < 0)
		delay = ip_rt_min_delay;

	spin_lock_bh(&rt_flush_lock);

	if (del_timer(&rt_flush_timer) && delay > 0 && rt_deadline) {
		long tmo = (long)(rt_deadline - now);

		/* If flush timer is already running
		   and flush request is not immediate (delay > 0):

		   if deadline is not achieved, prolongate timer to "delay",
		   otherwise fire it at deadline time.
		 */

		if (user_mode && tmo < ip_rt_max_delay-ip_rt_min_delay)
			tmo = 0;
		
		if (delay > tmo)
			delay = tmo;
	}

	if (delay <= 0) {
		spin_unlock_bh(&rt_flush_lock);
		SMP_TIMER_NAME(rt_run_flush)(0);
		return;
	}

	if (rt_deadline == 0)
		rt_deadline = now + ip_rt_max_delay;

	mod_timer(&rt_flush_timer, now+delay);
	spin_unlock_bh(&rt_flush_lock);
}

static void rt_secret_rebuild(unsigned long dummy)
{
	unsigned long now = jiffies;

	rt_cache_flush(0);
	mod_timer(&rt_secret_timer, now + ip_rt_secret_interval);
}

/*
   Short description of GC goals.

   We want to build algorithm, which will keep routing cache
   at some equilibrium point, when number of aged off entries
   is kept approximately equal to newly generated ones.

   Current expiration strength is variable "expire".
   We try to adjust it dynamically, so that if networking
   is idle expires is large enough to keep enough of warm entries,
   and when load increases it reduces to limit cache size.
 */

static int rt_garbage_collect(void)
{
	static unsigned long expire = RT_GC_TIMEOUT;
	static unsigned long last_gc;
	static int rover;
	static int equilibrium;
	struct rtable *rth, **rthp;
	unsigned long now = jiffies;
	int goal;

	/*
	 * Garbage collection is pretty expensive,
	 * do not make it too frequently.
	 */

	rt_cache_stat[smp_processor_id()].gc_total++;

	if (now - last_gc < ip_rt_gc_min_interval &&
	    atomic_read(&ipv4_dst_ops.entries) < ip_rt_max_size) {
		rt_cache_stat[smp_processor_id()].gc_ignored++;
		goto out;
	}

	/* Calculate number of entries, which we want to expire now. */
	goal = atomic_read(&ipv4_dst_ops.entries) -
		(ip_rt_gc_elasticity << rt_hash_log);
	if (goal <= 0) {
		if (equilibrium < ipv4_dst_ops.gc_thresh)
			equilibrium = ipv4_dst_ops.gc_thresh;
		goal = atomic_read(&ipv4_dst_ops.entries) - equilibrium;
		if (goal > 0) {
			equilibrium += min_t(unsigned int, goal / 2, rt_hash_mask + 1);
			goal = atomic_read(&ipv4_dst_ops.entries) - equilibrium;
		}
	} else {
		/* We are in dangerous area. Try to reduce cache really
		 * aggressively.
		 */
		goal = max_t(unsigned int, goal / 2, rt_hash_mask + 1);
		equilibrium = atomic_read(&ipv4_dst_ops.entries) - goal;
	}

	if (now - last_gc >= ip_rt_gc_min_interval)
		last_gc = now;

	if (goal <= 0) {
		equilibrium += goal;
		goto work_done;
	}

	do {
		int i, k;

		for (i = rt_hash_mask, k = rover; i >= 0; i--) {
			unsigned long tmo = expire;

			k = (k + 1) & rt_hash_mask;
			rthp = &rt_hash_table[k].chain;
			write_lock_bh(&rt_hash_table[k].lock);
			while ((rth = *rthp) != NULL) {
				if (!rt_may_expire(rth, tmo, expire)) {
					tmo >>= 1;
					rthp = &rth->u.rt_next;
					continue;
				}
				*rthp = rth->u.rt_next;
				rt_free(rth);
				goal--;
			}
			write_unlock_bh(&rt_hash_table[k].lock);
			if (goal <= 0)
				break;
		}
		rover = k;

		if (goal <= 0)
			goto work_done;

		/* Goal is not achieved. We stop process if:

		   - if expire reduced to zero. Otherwise, expire is halfed.
		   - if table is not full.
		   - if we are called from interrupt.
		   - jiffies check is just fallback/debug loop breaker.
		     We will not spin here for long time in any case.
		 */

		rt_cache_stat[smp_processor_id()].gc_goal_miss++;

		if (expire == 0)
			break;

		expire >>= 1;
#if RT_CACHE_DEBUG >= 2
		printk(KERN_DEBUG "expire>> %u %d %d %d\n", expire,
				atomic_read(&ipv4_dst_ops.entries), goal, i);
#endif

		if (atomic_read(&ipv4_dst_ops.entries) < ip_rt_max_size)
			goto out;
	} while (!in_softirq() && time_before_eq(jiffies, now));

	if (atomic_read(&ipv4_dst_ops.entries) < ip_rt_max_size)
		goto out;
	if (net_ratelimit())
		printk(KERN_WARNING "dst cache overflow\n");
	rt_cache_stat[smp_processor_id()].gc_dst_overflow++;
	return 1;

work_done:
	expire += ip_rt_gc_min_interval;
	if (expire > ip_rt_gc_timeout ||
	    atomic_read(&ipv4_dst_ops.entries) < ipv4_dst_ops.gc_thresh)
		expire = ip_rt_gc_timeout;
#if RT_CACHE_DEBUG >= 2
	printk(KERN_DEBUG "expire++ %u %d %d %d\n", expire,
			atomic_read(&ipv4_dst_ops.entries), goal, rover);
#endif
out:	return 0;
}

static int rt_intern_hash(unsigned hash, struct rtable *rt, struct rtable **rp)
{
	struct rtable	*rth, **rthp;
	unsigned long	now;
	struct rtable *cand, **candp;
	u32 		min_score;
	int		chain_length;
	int attempts = !in_softirq();

restart:
	chain_length = 0;
	min_score = ~(u32)0;
	cand = NULL;
	candp = NULL;
	now = jiffies;

	rthp = &rt_hash_table[hash].chain;

	write_lock_bh(&rt_hash_table[hash].lock);
	while ((rth = *rthp) != NULL) {
		if (memcmp(&rth->key, &rt->key, sizeof(rt->key)) == 0) {
			/* Put it first */
			*rthp = rth->u.rt_next;
			rth->u.rt_next = rt_hash_table[hash].chain;
			rt_hash_table[hash].chain = rth;

			rth->u.dst.__use++;
			dst_hold(&rth->u.dst);
			rth->u.dst.lastuse = now;
			write_unlock_bh(&rt_hash_table[hash].lock);

			rt_drop(rt);
			*rp = rth;
			return 0;
		}

		if (!atomic_read(&rth->u.dst.__refcnt)) {
			u32 score = rt_score(rth);

			if (score <= min_score) {
				cand = rth;
				candp = rthp;
				min_score = score;
			}
		}

		chain_length++;

		rthp = &rth->u.rt_next;
	}

	if (cand) {
		/* ip_rt_gc_elasticity used to be average length of chain
		 * length, when exceeded gc becomes really aggressive.
		 *
		 * The second limit is less certain. At the moment it allows
		 * only 2 entries per bucket. We will see.
		 */
		if (chain_length > ip_rt_gc_elasticity) {
			*candp = cand->u.rt_next;
			rt_free(cand);
		}
	}

	/* Try to bind route to arp only if it is output
	   route or unicast forwarding path.
	 */
	if (rt->rt_type == RTN_UNICAST || rt->key.iif == 0) {
		int err = arp_bind_neighbour(&rt->u.dst);
		if (err) {
			write_unlock_bh(&rt_hash_table[hash].lock);

			if (err != -ENOBUFS) {
				rt_drop(rt);
				return err;
			}

			/* Neighbour tables are full and nothing
			   can be released. Try to shrink route cache,
			   it is most likely it holds some neighbour records.
			 */
			if (attempts-- > 0) {
				int saved_elasticity = ip_rt_gc_elasticity;
				int saved_int = ip_rt_gc_min_interval;
				ip_rt_gc_elasticity	= 1;
				ip_rt_gc_min_interval	= 0;
				rt_garbage_collect();
				ip_rt_gc_min_interval	= saved_int;
				ip_rt_gc_elasticity	= saved_elasticity;
				goto restart;
			}

			if (net_ratelimit())
				printk(KERN_WARNING "Neighbour table overflow.\n");
			rt_drop(rt);
			return -ENOBUFS;
		}
	}

	rt->u.rt_next = rt_hash_table[hash].chain;
#if RT_CACHE_DEBUG >= 2
	if (rt->u.rt_next) {
		struct rtable *trt;
		printk(KERN_DEBUG "rt_cache @%02x: %u.%u.%u.%u", hash,
		       NIPQUAD(rt->rt_dst));
		for (trt = rt->u.rt_next; trt; trt = trt->u.rt_next)
			printk(" . %u.%u.%u.%u", NIPQUAD(trt->rt_dst));
		printk("\n");
	}
#endif
	rt_hash_table[hash].chain = rt;
	write_unlock_bh(&rt_hash_table[hash].lock);
	*rp = rt;
	return 0;
}

void rt_bind_peer(struct rtable *rt, int create)
{
	static spinlock_t rt_peer_lock = SPIN_LOCK_UNLOCKED;
	struct inet_peer *peer;

	peer = inet_getpeer(rt->rt_dst, create);

	spin_lock_bh(&rt_peer_lock);
	if (rt->peer == NULL) {
		rt->peer = peer;
		peer = NULL;
	}
	spin_unlock_bh(&rt_peer_lock);
	if (peer)
		inet_putpeer(peer);
}

/*
 * Peer allocation may fail only in serious out-of-memory conditions.  However
 * we still can generate some output.
 * Random ID selection looks a bit dangerous because we have no chances to
 * select ID being unique in a reasonable period of time.
 * But broken packet identifier may be better than no packet at all.
 */
static void ip_select_fb_ident(struct iphdr *iph)
{
	static spinlock_t ip_fb_id_lock = SPIN_LOCK_UNLOCKED;
	static u32 ip_fallback_id;
	u32 salt;

	spin_lock_bh(&ip_fb_id_lock);
	salt = secure_ip_id(ip_fallback_id ^ iph->daddr);
	iph->id = htons(salt & 0xFFFF);
	ip_fallback_id = salt;
	spin_unlock_bh(&ip_fb_id_lock);
}

void __ip_select_ident(struct iphdr *iph, struct dst_entry *dst)
{
	struct rtable *rt = (struct rtable *) dst;

	if (rt) {
		if (rt->peer == NULL)
			rt_bind_peer(rt, 1);

		/* If peer is attached to destination, it is never detached,
		   so that we need not to grab a lock to dereference it.
		 */
		if (rt->peer) {
			iph->id = htons(inet_getid(rt->peer));
			return;
		}
	} else
		printk(KERN_DEBUG "rt_bind_peer(0) @%p\n", NET_CALLER(iph));

	ip_select_fb_ident(iph);
}

static void rt_del(unsigned hash, struct rtable *rt)
{
	struct rtable **rthp;

	write_lock_bh(&rt_hash_table[hash].lock);
	ip_rt_put(rt);
	for (rthp = &rt_hash_table[hash].chain; *rthp;
	     rthp = &(*rthp)->u.rt_next)
		if (*rthp == rt) {
			*rthp = rt->u.rt_next;
			rt_free(rt);
			break;
		}
	write_unlock_bh(&rt_hash_table[hash].lock);
}

void ip_rt_redirect(u32 old_gw, u32 daddr, u32 new_gw,
		    u32 saddr, u8 tos, struct net_device *dev)
{
	int i, k;
	struct in_device *in_dev = in_dev_get(dev);
	struct rtable *rth, **rthp;
	u32  skeys[2] = { saddr, 0 };
	int  ikeys[2] = { dev->ifindex, 0 };

	tos &= IPTOS_RT_MASK;

	if (!in_dev)
		return;

	if (new_gw == old_gw || !IN_DEV_RX_REDIRECTS(in_dev)
	    || MULTICAST(new_gw) || BADCLASS(new_gw) || ZERONET(new_gw))
		goto reject_redirect;

	if (!IN_DEV_SHARED_MEDIA(in_dev)) {
		if (!inet_addr_onlink(in_dev, new_gw, old_gw))
			goto reject_redirect;
		if (IN_DEV_SEC_REDIRECTS(in_dev) && ip_fib_check_default(new_gw, dev))
			goto reject_redirect;
	} else {
		if (inet_addr_type(new_gw) != RTN_UNICAST)
			goto reject_redirect;
	}

	for (i = 0; i < 2; i++) {
		for (k = 0; k < 2; k++) {
			unsigned hash = rt_hash_code(daddr,
						     skeys[i] ^ (ikeys[k] << 5),
						     tos);

			rthp=&rt_hash_table[hash].chain;

			read_lock(&rt_hash_table[hash].lock);
			while ((rth = *rthp) != NULL) {
				struct rtable *rt;

				if (rth->key.dst != daddr ||
				    rth->key.src != skeys[i] ||
				    rth->key.tos != tos ||
				    rth->key.oif != ikeys[k] ||
				    rth->key.iif != 0) {
					rthp = &rth->u.rt_next;
					continue;
				}

				if (rth->rt_dst != daddr ||
				    rth->rt_src != saddr ||
				    rth->u.dst.error ||
				    rth->rt_gateway != old_gw ||
				    rth->u.dst.dev != dev)
					break;

				dst_hold(&rth->u.dst);
				read_unlock(&rt_hash_table[hash].lock);

				rt = dst_alloc(&ipv4_dst_ops);
				if (rt == NULL) {
					ip_rt_put(rth);
					in_dev_put(in_dev);
					return;
				}

				/* Copy all the information. */
				*rt = *rth;
				rt->u.dst.__use		= 1;
				atomic_set(&rt->u.dst.__refcnt, 1);
				if (rt->u.dst.dev)
					dev_hold(rt->u.dst.dev);
				rt->u.dst.lastuse	= jiffies;
				rt->u.dst.neighbour	= NULL;
				rt->u.dst.hh		= NULL;
				rt->u.dst.obsolete	= 0;

				rt->rt_flags		|= RTCF_REDIRECTED;

				/* Gateway is different ... */
				rt->rt_gateway		= new_gw;

				/* Redirect received -> path was valid */
				dst_confirm(&rth->u.dst);

				if (rt->peer)
					atomic_inc(&rt->peer->refcnt);

				if (arp_bind_neighbour(&rt->u.dst) ||
				    !(rt->u.dst.neighbour->nud_state &
					    NUD_VALID)) {
					if (rt->u.dst.neighbour)
						neigh_event_send(rt->u.dst.neighbour, NULL);
					ip_rt_put(rth);
					rt_drop(rt);
					goto do_next;
				}

				rt_del(hash, rth);
				if (!rt_intern_hash(hash, rt, &rt))
					ip_rt_put(rt);
				goto do_next;
			}
			read_unlock(&rt_hash_table[hash].lock);
		do_next:
			;
		}
	}
	in_dev_put(in_dev);
	return;

reject_redirect:
#ifdef CONFIG_IP_ROUTE_VERBOSE
	if (IN_DEV_LOG_MARTIANS(in_dev) && net_ratelimit())
		printk(KERN_INFO "Redirect from %u.%u.%u.%u on %s about "
			"%u.%u.%u.%u ignored.\n"
			"  Advised path = %u.%u.%u.%u -> %u.%u.%u.%u, "
			"tos %02x\n",
		       NIPQUAD(old_gw), dev->name, NIPQUAD(new_gw),
		       NIPQUAD(saddr), NIPQUAD(daddr), tos);
#endif
	in_dev_put(in_dev);
}

static struct dst_entry *ipv4_negative_advice(struct dst_entry *dst)
{
	struct rtable *rt = (struct rtable*)dst;
	struct dst_entry *ret = dst;

	if (rt) {
		if (dst->obsolete) {
			ip_rt_put(rt);
			ret = NULL;
		} else if ((rt->rt_flags & RTCF_REDIRECTED) ||
			   rt->u.dst.expires) {
			unsigned hash = rt_hash_code(rt->key.dst,
						     rt->key.src ^
							(rt->key.oif << 5),
						     rt->key.tos);
#if RT_CACHE_DEBUG >= 1
			printk(KERN_DEBUG "ip_rt_advice: redirect to "
					  "%u.%u.%u.%u/%02x dropped\n",
				NIPQUAD(rt->rt_dst), rt->key.tos);
#endif
			rt_del(hash, rt);
			ret = NULL;
		}
	}
	return ret;
}

/*
 * Algorithm:
 *	1. The first ip_rt_redirect_number redirects are sent
 *	   with exponential backoff, then we stop sending them at all,
 *	   assuming that the host ignores our redirects.
 *	2. If we did not see packets requiring redirects
 *	   during ip_rt_redirect_silence, we assume that the host
 *	   forgot redirected route and start to send redirects again.
 *
 * This algorithm is much cheaper and more intelligent than dumb load limiting
 * in icmp.c.
 *
 * NOTE. Do not forget to inhibit load limiting for redirects (redundant)
 * and "frag. need" (breaks PMTU discovery) in icmp.c.
 */

void ip_rt_send_redirect(struct sk_buff *skb)
{
	struct rtable *rt = (struct rtable*)skb->dst;
	struct in_device *in_dev = in_dev_get(rt->u.dst.dev);

	if (!in_dev)
		return;

	if (!IN_DEV_TX_REDIRECTS(in_dev))
		goto out;

	/* No redirected packets during ip_rt_redirect_silence;
	 * reset the algorithm.
	 */
	if (time_after(jiffies, rt->u.dst.rate_last + ip_rt_redirect_silence))
		rt->u.dst.rate_tokens = 0;

	/* Too many ignored redirects; do not send anything
	 * set u.dst.rate_last to the last seen redirected packet.
	 */
	if (rt->u.dst.rate_tokens >= ip_rt_redirect_number) {
		rt->u.dst.rate_last = jiffies;
		goto out;
	}

	/* Check for load limit; set rate_last to the latest sent
	 * redirect.
	 */
	if (time_after(jiffies,
		       (rt->u.dst.rate_last +
			(ip_rt_redirect_load << rt->u.dst.rate_tokens)))) {
		icmp_send(skb, ICMP_REDIRECT, ICMP_REDIR_HOST, rt->rt_gateway);
		rt->u.dst.rate_last = jiffies;
		++rt->u.dst.rate_tokens;
#ifdef CONFIG_IP_ROUTE_VERBOSE
		if (IN_DEV_LOG_MARTIANS(in_dev) &&
		    rt->u.dst.rate_tokens == ip_rt_redirect_number &&
		    net_ratelimit())
			printk(KERN_WARNING "host %u.%u.%u.%u/if%d ignores "
				"redirects for %u.%u.%u.%u to %u.%u.%u.%u.\n",
				NIPQUAD(rt->rt_src), rt->rt_iif,
				NIPQUAD(rt->rt_dst), NIPQUAD(rt->rt_gateway));
#endif
	}
out:
        in_dev_put(in_dev);
}

static int ip_error(struct sk_buff *skb)
{
	struct rtable *rt = (struct rtable*)skb->dst;
	unsigned long now;
	int code;

	switch (rt->u.dst.error) {
		case EINVAL:
		default:
			goto out;
		case EHOSTUNREACH:
			code = ICMP_HOST_UNREACH;
			break;
		case ENETUNREACH:
			code = ICMP_NET_UNREACH;
			break;
		case EACCES:
			code = ICMP_PKT_FILTERED;
			break;
	}

	now = jiffies;
	rt->u.dst.rate_tokens += now - rt->u.dst.rate_last;
	if (rt->u.dst.rate_tokens > ip_rt_error_burst)
		rt->u.dst.rate_tokens = ip_rt_error_burst;
	rt->u.dst.rate_last = now;
	if (rt->u.dst.rate_tokens >= ip_rt_error_cost) {
		rt->u.dst.rate_tokens -= ip_rt_error_cost;
		icmp_send(skb, ICMP_DEST_UNREACH, code, 0);
	}

out:	kfree_skb(skb);
	return 0;
} 

/*
 *	The last two values are not from the RFC but
 *	are needed for AMPRnet AX.25 paths.
 */

static unsigned short mtu_plateau[] =
{32000, 17914, 8166, 4352, 2002, 1492, 576, 296, 216, 128 };

static __inline__ unsigned short guess_mtu(unsigned short old_mtu)
{
	int i;
	
	for (i = 0; i < sizeof(mtu_plateau) / sizeof(mtu_plateau[0]); i++)
		if (old_mtu > mtu_plateau[i])
			return mtu_plateau[i];
	return 68;
}

unsigned short ip_rt_frag_needed(struct iphdr *iph, unsigned short new_mtu)
{
	int i;
	unsigned short old_mtu = ntohs(iph->tot_len);
	struct rtable *rth;
	u32  skeys[2] = { iph->saddr, 0, };
	u32  daddr = iph->daddr;
	u8   tos = iph->tos & IPTOS_RT_MASK;
	unsigned short est_mtu = 0;

	if (ipv4_config.no_pmtu_disc)
		return 0;

	for (i = 0; i < 2; i++) {
		unsigned hash = rt_hash_code(daddr, skeys[i], tos);

		read_lock(&rt_hash_table[hash].lock);
		for (rth = rt_hash_table[hash].chain; rth;
		     rth = rth->u.rt_next) {
			if (rth->key.dst == daddr &&
			    rth->key.src == skeys[i] &&
			    rth->rt_dst  == daddr &&
			    rth->rt_src  == iph->saddr &&
			    rth->key.tos == tos &&
			    rth->key.iif == 0 &&
			    !(rth->u.dst.mxlock & (1 << RTAX_MTU))) {
				unsigned short mtu = new_mtu;

				if (new_mtu < 68 || new_mtu >= old_mtu) {

					/* BSD 4.2 compatibility hack :-( */
					if (mtu == 0 &&
					    old_mtu >= rth->u.dst.pmtu &&
					    old_mtu >= 68 + (iph->ihl << 2))
						old_mtu -= iph->ihl << 2;

					mtu = guess_mtu(old_mtu);
				}
				if (mtu <= rth->u.dst.pmtu) {
					if (mtu < rth->u.dst.pmtu) { 
						dst_confirm(&rth->u.dst);
						if (mtu < ip_rt_min_pmtu) {
							mtu = ip_rt_min_pmtu;
							rth->u.dst.mxlock |=
								(1 << RTAX_MTU);
						}
						rth->u.dst.pmtu = mtu;
						dst_set_expires(&rth->u.dst,
							ip_rt_mtu_expires);
					}
					est_mtu = mtu;
				}
			}
		}
		read_unlock(&rt_hash_table[hash].lock);
	}
	return est_mtu ? : new_mtu;
}

void ip_rt_update_pmtu(struct dst_entry *dst, unsigned mtu)
{
	if (dst->pmtu > mtu && mtu >= 68 &&
	    !(dst->mxlock & (1 << RTAX_MTU))) {
		if (mtu < ip_rt_min_pmtu) {
			mtu = ip_rt_min_pmtu;
			dst->mxlock |= (1 << RTAX_MTU);
		}
		dst->pmtu = mtu;
		dst_set_expires(dst, ip_rt_mtu_expires);
	}
}

static struct dst_entry *ipv4_dst_check(struct dst_entry *dst, u32 cookie)
{
	dst_release(dst);
	return NULL;
}

static struct dst_entry *ipv4_dst_reroute(struct dst_entry *dst,
					  struct sk_buff *skb)
{
	return NULL;
}

static void ipv4_dst_destroy(struct dst_entry *dst)
{
	struct rtable *rt = (struct rtable *) dst;
	struct inet_peer *peer = rt->peer;

	if (peer) {
		rt->peer = NULL;
		inet_putpeer(peer);
	}
}

static void ipv4_link_failure(struct sk_buff *skb)
{
	struct rtable *rt;

	icmp_send(skb, ICMP_DEST_UNREACH, ICMP_HOST_UNREACH, 0);

	rt = (struct rtable *) skb->dst;
	if (rt)
		dst_set_expires(&rt->u.dst, 0);
}

static int ip_rt_bug(struct sk_buff *skb)
{
	printk(KERN_DEBUG "ip_rt_bug: %u.%u.%u.%u -> %u.%u.%u.%u, %s\n",
		NIPQUAD(skb->nh.iph->saddr), NIPQUAD(skb->nh.iph->daddr),
		skb->dev ? skb->dev->name : "?");
	kfree_skb(skb);
	return 0;
}

/*
   We do not cache source address of outgoing interface,
   because it is used only by IP RR, TS and SRR options,
   so that it out of fast path.

   BTW remember: "addr" is allowed to be not aligned
   in IP options!
 */

void ip_rt_get_source(u8 *addr, struct rtable *rt)
{
	u32 src;
	struct fib_result res;

	if (rt->key.iif == 0)
		src = rt->rt_src;
	else if (fib_lookup(&rt->key, &res) == 0) {
#ifdef CONFIG_IP_ROUTE_NAT
		if (res.type == RTN_NAT)
			src = inet_select_addr(rt->u.dst.dev, rt->rt_gateway,
						RT_SCOPE_UNIVERSE);
		else
#endif
			src = FIB_RES_PREFSRC(res);
		fib_res_put(&res);
	} else
		src = inet_select_addr(rt->u.dst.dev, rt->rt_gateway,
					RT_SCOPE_UNIVERSE);
	memcpy(addr, &src, 4);
}

#ifdef CONFIG_NET_CLS_ROUTE
static void set_class_tag(struct rtable *rt, u32 tag)
{
	if (!(rt->u.dst.tclassid & 0xFFFF))
		rt->u.dst.tclassid |= tag & 0xFFFF;
	if (!(rt->u.dst.tclassid & 0xFFFF0000))
		rt->u.dst.tclassid |= tag & 0xFFFF0000;
}
#endif

static void rt_set_nexthop(struct rtable *rt, struct fib_result *res, u32 itag)
{
	struct fib_info *fi = res->fi;

	if (fi) {
		if (FIB_RES_GW(*res) &&
		    FIB_RES_NH(*res).nh_scope == RT_SCOPE_LINK)
			rt->rt_gateway = FIB_RES_GW(*res);
		memcpy(&rt->u.dst.mxlock, fi->fib_metrics,
			sizeof(fi->fib_metrics));
		if (fi->fib_mtu == 0) {
			rt->u.dst.pmtu = rt->u.dst.dev->mtu;
			if (rt->u.dst.mxlock & (1 << RTAX_MTU) &&
			    rt->rt_gateway != rt->rt_dst &&
			    rt->u.dst.pmtu > 576)
				rt->u.dst.pmtu = 576;
		}
#ifdef CONFIG_NET_CLS_ROUTE
		rt->u.dst.tclassid = FIB_RES_NH(*res).nh_tclassid;
#endif
	} else
		rt->u.dst.pmtu	= rt->u.dst.dev->mtu;

	if (rt->u.dst.pmtu > IP_MAX_MTU)
		rt->u.dst.pmtu = IP_MAX_MTU;
	if (rt->u.dst.advmss == 0)
		rt->u.dst.advmss = max_t(unsigned int, rt->u.dst.dev->mtu - 40,
				       ip_rt_min_advmss);
	if (rt->u.dst.advmss > 65535 - 40)
		rt->u.dst.advmss = 65535 - 40;

#ifdef CONFIG_NET_CLS_ROUTE
#ifdef CONFIG_IP_MULTIPLE_TABLES
	set_class_tag(rt, fib_rules_tclass(res));
#endif
	set_class_tag(rt, itag);
#endif
        rt->rt_type = res->type;
}

static int ip_route_input_mc(struct sk_buff *skb, u32 daddr, u32 saddr,
				u8 tos, struct net_device *dev, int our)
{
	unsigned hash;
	struct rtable *rth;
	u32 spec_dst;
	struct in_device *in_dev = in_dev_get(dev);
	u32 itag = 0;

	/* Primary sanity checks. */

	if (in_dev == NULL)
		return -EINVAL;

	if (MULTICAST(saddr) || BADCLASS(saddr) || LOOPBACK(saddr) ||
	    skb->protocol != htons(ETH_P_IP))
		goto e_inval;

	if (ZERONET(saddr)) {
		if (!LOCAL_MCAST(daddr))
			goto e_inval;
		spec_dst = inet_select_addr(dev, 0, RT_SCOPE_LINK);
	} else if (fib_validate_source(saddr, 0, tos, 0,
					dev, &spec_dst, &itag) < 0)
		goto e_inval;

	rth = dst_alloc(&ipv4_dst_ops);
	if (!rth)
		goto e_nobufs;

	rth->u.dst.output= ip_rt_bug;

	atomic_set(&rth->u.dst.__refcnt, 1);
	rth->u.dst.flags= DST_HOST;
	rth->key.dst	= daddr;
	rth->rt_dst	= daddr;
	rth->key.tos	= tos;
#ifdef CONFIG_IP_ROUTE_FWMARK
	rth->key.fwmark	= skb->nfmark;
#endif
	rth->key.src	= saddr;
	rth->rt_src	= saddr;
#ifdef CONFIG_IP_ROUTE_NAT
	rth->rt_dst_map	= daddr;
	rth->rt_src_map	= saddr;
#endif
#ifdef CONFIG_NET_CLS_ROUTE
	rth->u.dst.tclassid = itag;
#endif
	rth->rt_iif	=
	rth->key.iif	= dev->ifindex;
	rth->u.dst.dev	= &loopback_dev;
	dev_hold(rth->u.dst.dev);
	rth->key.oif	= 0;
	rth->rt_gateway	= daddr;
	rth->rt_spec_dst= spec_dst;
	rth->rt_type	= RTN_MULTICAST;
	rth->rt_flags	= RTCF_MULTICAST;
	if (our) {
		rth->u.dst.input= ip_local_deliver;
		rth->rt_flags |= RTCF_LOCAL;
	}

#ifdef CONFIG_IP_MROUTE
	if (!LOCAL_MCAST(daddr) && IN_DEV_MFORWARD(in_dev))
		rth->u.dst.input = ip_mr_input;
#endif
	rt_cache_stat[smp_processor_id()].in_slow_mc++;

	in_dev_put(in_dev);
	hash = rt_hash_code(daddr, saddr ^ (dev->ifindex << 5), tos);
	return rt_intern_hash(hash, rth, (struct rtable**) &skb->dst);

e_nobufs:
	in_dev_put(in_dev);
	return -ENOBUFS;

e_inval:
	in_dev_put(in_dev);
	return -EINVAL;
}

/*
 *	NOTE. We drop all the packets that has local source
 *	addresses, because every properly looped back packet
 *	must have correct destination already attached by output routine.
 *
 *	Such approach solves two big problems:
 *	1. Not simplex devices are handled properly.
 *	2. IP spoofing attempts are filtered with 100% of guarantee.
 */

int ip_route_input_slow(struct sk_buff *skb, u32 daddr, u32 saddr,
			u8 tos, struct net_device *dev)
{
	struct rt_key	key;
	struct fib_result res;
	struct in_device *in_dev = in_dev_get(dev);
	struct in_device *out_dev = NULL;
	unsigned	flags = 0;
	u32		itag = 0;
	struct rtable * rth;
	unsigned	hash;
	u32		spec_dst;
	int		err = -EINVAL;
	int		free_res = 0;

	/* IP on this device is disabled. */

	if (!in_dev)
		goto out;

	key.dst		= daddr;
	key.src		= saddr;
	key.tos		= tos;
#ifdef CONFIG_IP_ROUTE_FWMARK
	key.fwmark	= skb->nfmark;
#endif
	key.iif		= dev->ifindex;
	key.oif		= 0;
	key.scope	= RT_SCOPE_UNIVERSE;

	hash = rt_hash_code(daddr, saddr ^ (key.iif << 5), tos);

	/* Check for the most weird martians, which can be not detected
	   by fib_lookup.
	 */

	if (MULTICAST(saddr) || BADCLASS(saddr) || LOOPBACK(saddr))
		goto martian_source;

	if (daddr == 0xFFFFFFFF || (saddr == 0 && daddr == 0))
		goto brd_input;

	/* Accept zero addresses only to limited broadcast;
	 * I even do not know to fix it or not. Waiting for complains :-)
	 */
	if (ZERONET(saddr))
		goto martian_source;

	if (BADCLASS(daddr) || ZERONET(daddr) || LOOPBACK(daddr))
		goto martian_destination;

	/*
	 *	Now we are ready to route packet.
	 */
	if ((err = fib_lookup(&key, &res)) != 0) {
		if (!IN_DEV_FORWARD(in_dev))
			goto e_inval;
		goto no_route;
	}
	free_res = 1;

	rt_cache_stat[smp_processor_id()].in_slow_tot++;

#ifdef CONFIG_IP_ROUTE_NAT
	/* Policy is applied before mapping destination,
	   but rerouting after map should be made with old source.
	 */

	if (1) {
		u32 src_map = saddr;
		if (res.r)
			src_map = fib_rules_policy(saddr, &res, &flags);

		if (res.type == RTN_NAT) {
			key.dst = fib_rules_map_destination(daddr, &res);
			fib_res_put(&res);
			free_res = 0;
			if (fib_lookup(&key, &res))
				goto e_inval;
			free_res = 1;
			if (res.type != RTN_UNICAST)
				goto e_inval;
			flags |= RTCF_DNAT;
		}
		key.src = src_map;
	}
#endif

	if (res.type == RTN_BROADCAST)
		goto brd_input;

	if (res.type == RTN_LOCAL) {
		int result;
		result = fib_validate_source(saddr, daddr, tos,
					     loopback_dev.ifindex,
					     dev, &spec_dst, &itag);
		if (result < 0)
			goto martian_source;
		if (result)
			flags |= RTCF_DIRECTSRC;
		spec_dst = daddr;
		goto local_input;
	}

	if (!IN_DEV_FORWARD(in_dev))
		goto e_inval;
	if (res.type != RTN_UNICAST)
		goto martian_destination;

#ifdef CONFIG_IP_ROUTE_MULTIPATH
	if (res.fi->fib_nhs > 1 && key.oif == 0)
		fib_select_multipath(&key, &res);
#endif
	out_dev = in_dev_get(FIB_RES_DEV(res));
	if (out_dev == NULL) {
		if (net_ratelimit())
			printk(KERN_CRIT "Bug in ip_route_input_slow(). "
					 "Please, report\n");
		goto e_inval;
	}

	err = fib_validate_source(saddr, daddr, tos, FIB_RES_OIF(res), dev,
				  &spec_dst, &itag);
	if (err < 0)
		goto martian_source;

	if (err)
		flags |= RTCF_DIRECTSRC;

	if (out_dev == in_dev && err && !(flags & (RTCF_NAT | RTCF_MASQ)) &&
	    (IN_DEV_SHARED_MEDIA(out_dev) ||
	     inet_addr_onlink(out_dev, saddr, FIB_RES_GW(res))))
		flags |= RTCF_DOREDIRECT;

	if (skb->protocol != htons(ETH_P_IP)) {
		/* Not IP (i.e. ARP). Do not create route, if it is
		 * invalid for proxy arp. DNAT routes are always valid.
		 */
		if (out_dev == in_dev && !(flags & RTCF_DNAT))
			goto e_inval;
	}

	rth = dst_alloc(&ipv4_dst_ops);
	if (!rth)
		goto e_nobufs;

	atomic_set(&rth->u.dst.__refcnt, 1);
	rth->u.dst.flags= DST_HOST;
	rth->key.dst	= daddr;
	rth->rt_dst	= daddr;
	rth->key.tos	= tos;
#ifdef CONFIG_IP_ROUTE_FWMARK
	rth->key.fwmark	= skb->nfmark;
#endif
	rth->key.src	= saddr;
	rth->rt_src	= saddr;
	rth->rt_gateway	= daddr;
#ifdef CONFIG_IP_ROUTE_NAT
	rth->rt_src_map	= key.src;
	rth->rt_dst_map	= key.dst;
	if (flags&RTCF_DNAT)
		rth->rt_gateway	= key.dst;
#endif
	rth->rt_iif 	=
	rth->key.iif	= dev->ifindex;
	rth->u.dst.dev	= out_dev->dev;
	dev_hold(rth->u.dst.dev);
	rth->key.oif 	= 0;
	rth->rt_spec_dst= spec_dst;

	rth->u.dst.input = ip_forward;
	rth->u.dst.output = ip_output;

	rt_set_nexthop(rth, &res, itag);

	rth->rt_flags = flags;

#ifdef CONFIG_NET_FASTROUTE
	if (netdev_fastroute && !(flags&(RTCF_NAT|RTCF_MASQ|RTCF_DOREDIRECT))) {
		struct net_device *odev = rth->u.dst.dev;
		if (odev != dev &&
		    dev->accept_fastpath &&
		    odev->mtu >= dev->mtu &&
		    dev->accept_fastpath(dev, &rth->u.dst) == 0)
			rth->rt_flags |= RTCF_FAST;
	}
#endif

intern:
	err = rt_intern_hash(hash, rth, (struct rtable**)&skb->dst);
done:
	in_dev_put(in_dev);
	if (out_dev)
		in_dev_put(out_dev);
	if (free_res)
		fib_res_put(&res);
out:	return err;

brd_input:
	if (skb->protocol != htons(ETH_P_IP))
		goto e_inval;

	if (ZERONET(saddr))
		spec_dst = inet_select_addr(dev, 0, RT_SCOPE_LINK);
	else {
		err = fib_validate_source(saddr, 0, tos, 0, dev, &spec_dst,
					  &itag);
		if (err < 0)
			goto martian_source;
		if (err)
			flags |= RTCF_DIRECTSRC;
	}
	flags |= RTCF_BROADCAST;
	res.type = RTN_BROADCAST;
	rt_cache_stat[smp_processor_id()].in_brd++;

local_input:
	rth = dst_alloc(&ipv4_dst_ops);
	if (!rth)
		goto e_nobufs;

	rth->u.dst.output= ip_rt_bug;

	atomic_set(&rth->u.dst.__refcnt, 1);
	rth->u.dst.flags= DST_HOST;
	rth->key.dst	= daddr;
	rth->rt_dst	= daddr;
	rth->key.tos	= tos;
#ifdef CONFIG_IP_ROUTE_FWMARK
	rth->key.fwmark	= skb->nfmark;
#endif
	rth->key.src	= saddr;
	rth->rt_src	= saddr;
#ifdef CONFIG_IP_ROUTE_NAT
	rth->rt_dst_map	= key.dst;
	rth->rt_src_map	= key.src;
#endif
#ifdef CONFIG_NET_CLS_ROUTE
	rth->u.dst.tclassid = itag;
#endif
	rth->rt_iif	=
	rth->key.iif	= dev->ifindex;
	rth->u.dst.dev	= &loopback_dev;
	dev_hold(rth->u.dst.dev);
	rth->key.oif 	= 0;
	rth->rt_gateway	= daddr;
	rth->rt_spec_dst= spec_dst;
	rth->u.dst.input= ip_local_deliver;
	rth->rt_flags 	= flags|RTCF_LOCAL;
	if (res.type == RTN_UNREACHABLE) {
		rth->u.dst.input= ip_error;
		rth->u.dst.error= -err;
		rth->rt_flags 	&= ~RTCF_LOCAL;
	}
	rth->rt_type	= res.type;
	goto intern;

no_route:
	rt_cache_stat[smp_processor_id()].in_no_route++;
	spec_dst = inet_select_addr(dev, 0, RT_SCOPE_UNIVERSE);
	res.type = RTN_UNREACHABLE;
	goto local_input;

	/*
	 *	Do not cache martian addresses: they should be logged (RFC1812)
	 */
martian_destination:
	rt_cache_stat[smp_processor_id()].in_martian_dst++;
#ifdef CONFIG_IP_ROUTE_VERBOSE
	if (IN_DEV_LOG_MARTIANS(in_dev) && net_ratelimit())
		printk(KERN_WARNING "martian destination %u.%u.%u.%u from "
			"%u.%u.%u.%u, dev %s\n",
			NIPQUAD(daddr), NIPQUAD(saddr), dev->name);
#endif
e_inval:
	err = -EINVAL;
	goto done;

e_nobufs:
	err = -ENOBUFS;
	goto done;

martian_source:

	rt_cache_stat[smp_processor_id()].in_martian_src++;
#ifdef CONFIG_IP_ROUTE_VERBOSE
	if (IN_DEV_LOG_MARTIANS(in_dev) && net_ratelimit()) {
		/*
		 *	RFC1812 recommendation, if source is martian,
		 *	the only hint is MAC header.
		 */
		printk(KERN_WARNING "martian source %u.%u.%u.%u from "
			"%u.%u.%u.%u, on dev %s\n",
			NIPQUAD(daddr), NIPQUAD(saddr), dev->name);
		if (dev->hard_header_len) {
			int i;
			unsigned char *p = skb->mac.raw;
			printk(KERN_WARNING "ll header: ");
			for (i = 0; i < dev->hard_header_len; i++, p++) {
				printk("%02x", *p);
				if (i < (dev->hard_header_len - 1))
					printk(":");
			}
			printk("\n");
		}
	}
#endif
	goto e_inval;
}

int ip_route_input(struct sk_buff *skb, u32 daddr, u32 saddr,
		   u8 tos, struct net_device *dev)
{
	struct rtable * rth;
	unsigned	hash;
	int iif = dev->ifindex;

	tos &= IPTOS_RT_MASK;
	hash = rt_hash_code(daddr, saddr ^ (iif << 5), tos);

	read_lock(&rt_hash_table[hash].lock);
	for (rth = rt_hash_table[hash].chain; rth; rth = rth->u.rt_next) {
		if (rth->key.dst == daddr &&
		    rth->key.src == saddr &&
		    rth->key.iif == iif &&
		    rth->key.oif == 0 &&
#ifdef CONFIG_IP_ROUTE_FWMARK
		    rth->key.fwmark == skb->nfmark &&
#endif
		    rth->key.tos == tos) {
			rth->u.dst.lastuse = jiffies;
			dst_hold(&rth->u.dst);
			rth->u.dst.__use++;
			rt_cache_stat[smp_processor_id()].in_hit++;
			read_unlock(&rt_hash_table[hash].lock);
			skb->dst = (struct dst_entry*)rth;
			return 0;
		}
		rt_cache_stat[smp_processor_id()].in_hlist_search++;
	}
	read_unlock(&rt_hash_table[hash].lock);

	/* Multicast recognition logic is moved from route cache to here.
	   The problem was that too many Ethernet cards have broken/missing
	   hardware multicast filters :-( As result the host on multicasting
	   network acquires a lot of useless route cache entries, sort of
	   SDR messages from all the world. Now we try to get rid of them.
	   Really, provided software IP multicast filter is organized
	   reasonably (at least, hashed), it does not result in a slowdown
	   comparing with route cache reject entries.
	   Note, that multicast routers are not affected, because
	   route cache entry is created eventually.
	 */
	if (MULTICAST(daddr)) {
		struct in_device *in_dev;

		read_lock(&inetdev_lock);
		if ((in_dev = __in_dev_get(dev)) != NULL) {
			int our = ip_check_mc(in_dev, daddr, saddr);
			if (our
#ifdef CONFIG_IP_MROUTE
			    || (!LOCAL_MCAST(daddr) && IN_DEV_MFORWARD(in_dev))
#endif
			    ) {
				read_unlock(&inetdev_lock);
				return ip_route_input_mc(skb, daddr, saddr,
							 tos, dev, our);
			}
		}
		read_unlock(&inetdev_lock);
		return -EINVAL;
	}
	return ip_route_input_slow(skb, daddr, saddr, tos, dev);
}

/*
 * Major route resolver routine.
 */

int ip_route_output_slow(struct rtable **rp, const struct rt_key *oldkey)
{
	struct rt_key key;
	struct fib_result res;
	unsigned flags = 0;
	struct rtable *rth;
	struct net_device *dev_out = NULL;
	unsigned hash;
	int free_res = 0;
	int err;
	u32 tos;

	tos		= oldkey->tos & (IPTOS_RT_MASK | RTO_ONLINK);
	key.dst		= oldkey->dst;
	key.src		= oldkey->src;
	key.tos		= tos & IPTOS_RT_MASK;
	key.iif		= loopback_dev.ifindex;
	key.oif		= oldkey->oif;
#ifdef CONFIG_IP_ROUTE_FWMARK
	key.fwmark	= oldkey->fwmark;
#endif
	key.scope	= (tos & RTO_ONLINK) ? RT_SCOPE_LINK :
						RT_SCOPE_UNIVERSE;
	res.fi		= NULL;
#ifdef CONFIG_IP_MULTIPLE_TABLES
	res.r		= NULL;
#endif

	if (oldkey->src) {
		err = -EINVAL;
		if (MULTICAST(oldkey->src) ||
		    BADCLASS(oldkey->src) ||
		    ZERONET(oldkey->src))
			goto out;

		/* It is equivalent to inet_addr_type(saddr) == RTN_LOCAL */
		dev_out = ip_dev_find(oldkey->src);
		if (dev_out == NULL)
			goto out;

		/* I removed check for oif == dev_out->oif here.
		   It was wrong by three reasons:
		   1. ip_dev_find(saddr) can return wrong iface, if saddr is
		      assigned to multiple interfaces.
		   2. Moreover, we are allowed to send packets with saddr
		      of another iface. --ANK
		 */

		if (oldkey->oif == 0
		    && (MULTICAST(oldkey->dst) || oldkey->dst == 0xFFFFFFFF)) {
			/* Special hack: user can direct multicasts
			   and limited broadcast via necessary interface
			   without fiddling with IP_MULTICAST_IF or IP_PKTINFO.
			   This hack is not just for fun, it allows
			   vic,vat and friends to work.
			   They bind socket to loopback, set ttl to zero
			   and expect that it will work.
			   From the viewpoint of routing cache they are broken,
			   because we are not allowed to build multicast path
			   with loopback source addr (look, routing cache
			   cannot know, that ttl is zero, so that packet
			   will not leave this host and route is valid).
			   Luckily, this hack is good workaround.
			 */

			key.oif = dev_out->ifindex;
			goto make_route;
		}
		if (dev_out)
			dev_put(dev_out);
		dev_out = NULL;
	}
	if (oldkey->oif) {
		dev_out = dev_get_by_index(oldkey->oif);
		err = -ENODEV;
		if (dev_out == NULL)
			goto out;
		if (__in_dev_get(dev_out) == NULL) {
			dev_put(dev_out);
			goto out;	/* Wrong error code */
		}

		if (LOCAL_MCAST(oldkey->dst) || oldkey->dst == 0xFFFFFFFF) {
			if (!key.src)
				key.src = inet_select_addr(dev_out, 0,
								RT_SCOPE_LINK);
			goto make_route;
		}
		if (!key.src) {
			if (MULTICAST(oldkey->dst))
				key.src = inet_select_addr(dev_out, 0,
								key.scope);
			else if (!oldkey->dst)
				key.src = inet_select_addr(dev_out, 0,
								RT_SCOPE_HOST);
		}
	}

	if (!key.dst) {
		key.dst = key.src;
		if (!key.dst)
			key.dst = key.src = htonl(INADDR_LOOPBACK);
		if (dev_out)
			dev_put(dev_out);
		dev_out = &loopback_dev;
		dev_hold(dev_out);
		key.oif = loopback_dev.ifindex;
		res.type = RTN_LOCAL;
		flags |= RTCF_LOCAL;
		goto make_route;
	}

	if (fib_lookup(&key, &res)) {
		res.fi = NULL;
		if (oldkey->oif) {
			/* Apparently, routing tables are wrong. Assume,
			   that the destination is on link.

			   WHY? DW.
			   Because we are allowed to send to iface
			   even if it has NO routes and NO assigned
			   addresses. When oif is specified, routing
			   tables are looked up with only one purpose:
			   to catch if destination is gatewayed, rather than
			   direct. Moreover, if MSG_DONTROUTE is set,
			   we send packet, ignoring both routing tables
			   and ifaddr state. --ANK


			   We could make it even if oif is unknown,
			   likely IPv6, but we do not.
			 */

			if (key.src == 0)
				key.src = inet_select_addr(dev_out, 0,
							   RT_SCOPE_LINK);
			res.type = RTN_UNICAST;
			goto make_route;
		}
		if (dev_out)
			dev_put(dev_out);
		err = -ENETUNREACH;
		goto out;
	}
	free_res = 1;

	if (res.type == RTN_NAT)
		goto e_inval;

	if (res.type == RTN_LOCAL) {
		if (!key.src)
			key.src = key.dst;
		if (dev_out)
			dev_put(dev_out);
		dev_out = &loopback_dev;
		dev_hold(dev_out);
		key.oif = dev_out->ifindex;
		if (res.fi)
			fib_info_put(res.fi);
		res.fi = NULL;
		flags |= RTCF_LOCAL;
		goto make_route;
	}

#ifdef CONFIG_IP_ROUTE_MULTIPATH
	if (res.fi->fib_nhs > 1 && key.oif == 0)
		fib_select_multipath(&key, &res);
	else
#endif
	if (!res.prefixlen && res.type == RTN_UNICAST && !key.oif)
		fib_select_default(&key, &res);

	if (!key.src)
		key.src = FIB_RES_PREFSRC(res);

	if (dev_out)
		dev_put(dev_out);
	dev_out = FIB_RES_DEV(res);
	dev_hold(dev_out);
	key.oif = dev_out->ifindex;

make_route:
	if (LOOPBACK(key.src) && !(dev_out->flags&IFF_LOOPBACK))
		goto e_inval;

	if (key.dst == 0xFFFFFFFF)
		res.type = RTN_BROADCAST;
	else if (MULTICAST(key.dst))
		res.type = RTN_MULTICAST;
	else if (BADCLASS(key.dst) || ZERONET(key.dst))
		goto e_inval;

	if (dev_out->flags & IFF_LOOPBACK)
		flags |= RTCF_LOCAL;

	if (res.type == RTN_BROADCAST) {
		flags |= RTCF_BROADCAST | RTCF_LOCAL;
		if (res.fi) {
			fib_info_put(res.fi);
			res.fi = NULL;
		}
	} else if (res.type == RTN_MULTICAST) {
		flags |= RTCF_MULTICAST|RTCF_LOCAL;
		read_lock(&inetdev_lock);
		if (!__in_dev_get(dev_out) ||
		    !ip_check_mc(__in_dev_get(dev_out),oldkey->dst,oldkey->src))
			flags &= ~RTCF_LOCAL;
		read_unlock(&inetdev_lock);
		/* If multicast route do not exist use
		   default one, but do not gateway in this case.
		   Yes, it is hack.
		 */
		if (res.fi && res.prefixlen < 4) {
			fib_info_put(res.fi);
			res.fi = NULL;
		}
	}

	rth = dst_alloc(&ipv4_dst_ops);
	if (!rth)
		goto e_nobufs;

	atomic_set(&rth->u.dst.__refcnt, 1);
	rth->u.dst.flags= DST_HOST;
	rth->key.dst	= oldkey->dst;
	rth->key.tos	= tos;
	rth->key.src	= oldkey->src;
	rth->key.iif	= 0;
	rth->key.oif	= oldkey->oif;
#ifdef CONFIG_IP_ROUTE_FWMARK
	rth->key.fwmark	= oldkey->fwmark;
#endif
	rth->rt_dst	= key.dst;
	rth->rt_src	= key.src;
#ifdef CONFIG_IP_ROUTE_NAT
	rth->rt_dst_map	= key.dst;
	rth->rt_src_map	= key.src;
#endif
	rth->rt_iif	= oldkey->oif ? : dev_out->ifindex;
	rth->u.dst.dev	= dev_out;
	dev_hold(dev_out);
	rth->rt_gateway = key.dst;
	rth->rt_spec_dst= key.src;

	rth->u.dst.output=ip_output;

	rt_cache_stat[smp_processor_id()].out_slow_tot++;

	if (flags & RTCF_LOCAL) {
		rth->u.dst.input = ip_local_deliver;
		rth->rt_spec_dst = key.dst;
	}
	if (flags & (RTCF_BROADCAST | RTCF_MULTICAST)) {
		rth->rt_spec_dst = key.src;
		if (flags & RTCF_LOCAL && !(dev_out->flags & IFF_LOOPBACK)) {
			rth->u.dst.output = ip_mc_output;
			rt_cache_stat[smp_processor_id()].out_slow_mc++;
		}
#ifdef CONFIG_IP_MROUTE
		if (res.type == RTN_MULTICAST) {
			struct in_device *in_dev = in_dev_get(dev_out);
			if (in_dev) {
				if (IN_DEV_MFORWARD(in_dev) &&
				    !LOCAL_MCAST(oldkey->dst)) {
					rth->u.dst.input = ip_mr_input;
					rth->u.dst.output = ip_mc_output;
				}
				in_dev_put(in_dev);
			}
		}
#endif
	}

	rt_set_nexthop(rth, &res, 0);

	rth->rt_flags = flags;

	hash = rt_hash_code(oldkey->dst, oldkey->src ^ (oldkey->oif << 5), tos);
	err = rt_intern_hash(hash, rth, rp);
done:
	if (free_res)
		fib_res_put(&res);
	if (dev_out)
		dev_put(dev_out);
out:	return err;

e_inval:
	err = -EINVAL;
	goto done;
e_nobufs:
	err = -ENOBUFS;
	goto done;
}

int ip_route_output_key(struct rtable **rp, const struct rt_key *key)
{
	unsigned hash;
	struct rtable *rth;

	hash = rt_hash_code(key->dst, key->src ^ (key->oif << 5), key->tos);

	read_lock_bh(&rt_hash_table[hash].lock);
	for (rth = rt_hash_table[hash].chain; rth; rth = rth->u.rt_next) {
		if (rth->key.dst == key->dst &&
		    rth->key.src == key->src &&
		    rth->key.iif == 0 &&
		    rth->key.oif == key->oif &&
#ifdef CONFIG_IP_ROUTE_FWMARK
		    rth->key.fwmark == key->fwmark &&
#endif
		    !((rth->key.tos ^ key->tos) &
			    (IPTOS_RT_MASK | RTO_ONLINK))) {
			rth->u.dst.lastuse = jiffies;
			dst_hold(&rth->u.dst);
			rth->u.dst.__use++;
			rt_cache_stat[smp_processor_id()].out_hit++;
			read_unlock_bh(&rt_hash_table[hash].lock);
			*rp = rth;
			return 0;
		}
		rt_cache_stat[smp_processor_id()].out_hlist_search++;
	}
	read_unlock_bh(&rt_hash_table[hash].lock);

	return ip_route_output_slow(rp, key);
}	

static int rt_fill_info(struct sk_buff *skb, u32 pid, u32 seq, int event,
			int nowait)
{
	struct rtable *rt = (struct rtable*)skb->dst;
	struct rtmsg *r;
	struct nlmsghdr  *nlh;
	unsigned char	 *b = skb->tail;
	struct rta_cacheinfo ci;
#ifdef CONFIG_IP_MROUTE
	struct rtattr *eptr;
#endif
	nlh = NLMSG_PUT(skb, pid, seq, event, sizeof(*r));
	r = NLMSG_DATA(nlh);
	nlh->nlmsg_flags = (nowait && pid) ? NLM_F_MULTI : 0;
	r->rtm_family	 = AF_INET;
	r->rtm_dst_len	= 32;
	r->rtm_src_len	= 0;
	r->rtm_tos	= rt->key.tos;
	r->rtm_table	= RT_TABLE_MAIN;
	r->rtm_type	= rt->rt_type;
	r->rtm_scope	= RT_SCOPE_UNIVERSE;
	r->rtm_protocol = RTPROT_UNSPEC;
	r->rtm_flags	= (rt->rt_flags & ~0xFFFF) | RTM_F_CLONED;
	if (rt->rt_flags & RTCF_NOTIFY)
		r->rtm_flags |= RTM_F_NOTIFY;
	RTA_PUT(skb, RTA_DST, 4, &rt->rt_dst);
	if (rt->key.src) {
		r->rtm_src_len = 32;
		RTA_PUT(skb, RTA_SRC, 4, &rt->key.src);
	}
	if (rt->u.dst.dev)
		RTA_PUT(skb, RTA_OIF, sizeof(int), &rt->u.dst.dev->ifindex);
#ifdef CONFIG_NET_CLS_ROUTE
	if (rt->u.dst.tclassid)
		RTA_PUT(skb, RTA_FLOW, 4, &rt->u.dst.tclassid);
#endif
	if (rt->key.iif)
		RTA_PUT(skb, RTA_PREFSRC, 4, &rt->rt_spec_dst);
	else if (rt->rt_src != rt->key.src)
		RTA_PUT(skb, RTA_PREFSRC, 4, &rt->rt_src);
	if (rt->rt_dst != rt->rt_gateway)
		RTA_PUT(skb, RTA_GATEWAY, 4, &rt->rt_gateway);
	if (rtnetlink_put_metrics(skb, &rt->u.dst.mxlock) < 0)
		goto rtattr_failure;
	ci.rta_lastuse	= jiffies - rt->u.dst.lastuse;
	ci.rta_used	= rt->u.dst.__use;
	ci.rta_clntref	= atomic_read(&rt->u.dst.__refcnt);
	if (rt->u.dst.expires)
		ci.rta_expires = rt->u.dst.expires - jiffies;
	else
		ci.rta_expires = 0;
	ci.rta_error	= rt->u.dst.error;
	ci.rta_id	= ci.rta_ts = ci.rta_tsage = 0;
	if (rt->peer) {
		ci.rta_id = rt->peer->ip_id_count;
		if (rt->peer->tcp_ts_stamp) {
			ci.rta_ts = rt->peer->tcp_ts;
			ci.rta_tsage = xtime.tv_sec - rt->peer->tcp_ts_stamp;
		}
	}
#ifdef CONFIG_IP_MROUTE
	eptr = (struct rtattr*)skb->tail;
#endif
	RTA_PUT(skb, RTA_CACHEINFO, sizeof(ci), &ci);
	if (rt->key.iif) {
#ifdef CONFIG_IP_MROUTE
		u32 dst = rt->rt_dst;

		if (MULTICAST(dst) && !LOCAL_MCAST(dst) &&
		    ipv4_devconf.mc_forwarding) {
			int err = ipmr_get_route(skb, r, nowait);
			if (err <= 0) {
				if (!nowait) {
					if (err == 0)
						return 0;
					goto nlmsg_failure;
				} else {
					if (err == -EMSGSIZE)
						goto nlmsg_failure;
					((struct rta_cacheinfo*)RTA_DATA(eptr))->rta_error = err;
				}
			}
		} else
#endif
			RTA_PUT(skb, RTA_IIF, sizeof(int), &rt->key.iif);
	}

	nlh->nlmsg_len = skb->tail - b;
	return skb->len;

nlmsg_failure:
rtattr_failure:
	skb_trim(skb, b - skb->data);
	return -1;
}

int inet_rtm_getroute(struct sk_buff *in_skb, struct nlmsghdr* nlh, void *arg)
{
	struct rtattr **rta = arg;
	struct rtmsg *rtm = NLMSG_DATA(nlh);
	struct rtable *rt = NULL;
	u32 dst = 0;
	u32 src = 0;
	int iif = 0;
	int err = -ENOBUFS;
	struct sk_buff *skb;

	skb = alloc_skb(NLMSG_GOODSIZE, GFP_KERNEL);
	if (!skb)
		goto out;

	/* Reserve room for dummy headers, this skb can pass
	   through good chunk of routing engine.
	 */
	skb->mac.raw = skb->data;
	skb_reserve(skb, MAX_HEADER + sizeof(struct iphdr));

	if (rta[RTA_SRC - 1])
		memcpy(&src, RTA_DATA(rta[RTA_SRC - 1]), 4);
	if (rta[RTA_DST - 1])
		memcpy(&dst, RTA_DATA(rta[RTA_DST - 1]), 4);
	if (rta[RTA_IIF - 1])
		memcpy(&iif, RTA_DATA(rta[RTA_IIF - 1]), sizeof(int));

	if (iif) {
		struct net_device *dev = __dev_get_by_index(iif);
		err = -ENODEV;
		if (!dev)
			goto out_free;
		skb->protocol	= htons(ETH_P_IP);
		skb->dev	= dev;
		local_bh_disable();
		err = ip_route_input(skb, dst, src, rtm->rtm_tos, dev);
		local_bh_enable();
		rt = (struct rtable*)skb->dst;
		if (!err && rt->u.dst.error)
			err = -rt->u.dst.error;
	} else {
		int oif = 0;
		if (rta[RTA_OIF - 1])
			memcpy(&oif, RTA_DATA(rta[RTA_OIF - 1]), sizeof(int));
		err = ip_route_output(&rt, dst, src, rtm->rtm_tos, oif);
	}
	if (err)
		goto out_free;

	skb->dst = &rt->u.dst;
	if (rtm->rtm_flags & RTM_F_NOTIFY)
		rt->rt_flags |= RTCF_NOTIFY;

	NETLINK_CB(skb).dst_pid = NETLINK_CB(in_skb).pid;

	err = rt_fill_info(skb, NETLINK_CB(in_skb).pid, nlh->nlmsg_seq,
				RTM_NEWROUTE, 0);
	if (!err)
		goto out_free;
	if (err < 0) {
		err = -EMSGSIZE;
		goto out_free;
	}

	err = netlink_unicast(rtnl, skb, NETLINK_CB(in_skb).pid, MSG_DONTWAIT);
	if (err > 0)
		err = 0;
out:	return err;

out_free:
	kfree_skb(skb);
	goto out;
}

int ip_rt_dump(struct sk_buff *skb,  struct netlink_callback *cb)
{
	struct rtable *rt;
	int h, s_h;
	int idx, s_idx;

	s_h = cb->args[0];
	s_idx = idx = cb->args[1];
	for (h = 0; h <= rt_hash_mask; h++) {
		if (h < s_h) continue;
		if (h > s_h)
			s_idx = 0;
		read_lock_bh(&rt_hash_table[h].lock);
		for (rt = rt_hash_table[h].chain, idx = 0; rt;
		     rt = rt->u.rt_next, idx++) {
			if (idx < s_idx)
				continue;
			skb->dst = dst_clone(&rt->u.dst);
			if (rt_fill_info(skb, NETLINK_CB(cb->skb).pid,
					 cb->nlh->nlmsg_seq,
					 RTM_NEWROUTE, 1) <= 0) {
				dst_release(xchg(&skb->dst, NULL));
				read_unlock_bh(&rt_hash_table[h].lock);
				goto done;
			}
			dst_release(xchg(&skb->dst, NULL));
		}
		read_unlock_bh(&rt_hash_table[h].lock);
	}

done:
	cb->args[0] = h;
	cb->args[1] = idx;
	return skb->len;
}

void ip_rt_multicast_event(struct in_device *in_dev)
{
	rt_cache_flush(0);
}

#ifdef CONFIG_SYSCTL
static int flush_delay;

static int ipv4_sysctl_rtcache_flush(ctl_table *ctl, int write,
					struct file *filp, void *buffer,
					size_t *lenp)
{
	if (write) {
		proc_dointvec(ctl, write, filp, buffer, lenp);
		rt_cache_flush(flush_delay);
		return 0;
	} 

	return -EINVAL;
}

static int ipv4_sysctl_rtcache_flush_strategy(ctl_table *table, int *name,
						int nlen, void *oldval,
						size_t *oldlenp, void *newval,
						size_t newlen, void **context)
{
	int delay;
	if (newlen != sizeof(int))
		return -EINVAL;
	if (get_user(delay, (int *)newval))
		return -EFAULT; 
	rt_cache_flush(delay); 
	return 0;
}

ctl_table ipv4_route_table[] = {
        {
		ctl_name:	NET_IPV4_ROUTE_FLUSH,
		procname:	"flush",
		data:		&flush_delay,
		maxlen:		sizeof(int),
		mode:		0644,
		proc_handler:	&ipv4_sysctl_rtcache_flush,
		strategy:	&ipv4_sysctl_rtcache_flush_strategy,
	},
	{
		ctl_name:	NET_IPV4_ROUTE_MIN_DELAY,
		procname:	"min_delay",
		data:		&ip_rt_min_delay,
		maxlen:		sizeof(int),
		mode:		0644,
		proc_handler:	&proc_dointvec_jiffies,
		strategy:	&sysctl_jiffies,
	},
	{
		ctl_name:	NET_IPV4_ROUTE_MAX_DELAY,
		procname:	"max_delay",
		data:		&ip_rt_max_delay,
		maxlen:		sizeof(int),
		mode:		0644,
		proc_handler:	&proc_dointvec_jiffies,
		strategy:	&sysctl_jiffies,
	},
	{
		ctl_name:	NET_IPV4_ROUTE_GC_THRESH,
		procname:	"gc_thresh",
		data:		&ipv4_dst_ops.gc_thresh,
		maxlen:		sizeof(int),
		mode:		0644,
		proc_handler:	&proc_dointvec,
	},
	{
		ctl_name:	NET_IPV4_ROUTE_MAX_SIZE,
		procname:	"max_size",
		data:		&ip_rt_max_size,
		maxlen:		sizeof(int),
		mode:		0644,
		proc_handler:	&proc_dointvec,
	},
	{
		ctl_name:	NET_IPV4_ROUTE_GC_MIN_INTERVAL,
		procname:	"gc_min_interval",
		data:		&ip_rt_gc_min_interval,
		maxlen:		sizeof(int),
		mode:		0644,
		proc_handler:	&proc_dointvec_jiffies,
		strategy:	&sysctl_jiffies,
	},
	{
		ctl_name:	NET_IPV4_ROUTE_GC_TIMEOUT,
		procname:	"gc_timeout",
		data:		&ip_rt_gc_timeout,
		maxlen:		sizeof(int),
		mode:		0644,
		proc_handler:	&proc_dointvec_jiffies,
		strategy:	&sysctl_jiffies,
	},
	{
		ctl_name:	NET_IPV4_ROUTE_GC_INTERVAL,
		procname:	"gc_interval",
		data:		&ip_rt_gc_interval,
		maxlen:		sizeof(int),
		mode:		0644,
		proc_handler:	&proc_dointvec_jiffies,
		strategy:	&sysctl_jiffies,
	},
	{
		ctl_name:	NET_IPV4_ROUTE_REDIRECT_LOAD,
		procname:	"redirect_load",
		data:		&ip_rt_redirect_load,
		maxlen:		sizeof(int),
		mode:		0644,
		proc_handler:	&proc_dointvec,
	},
	{
		ctl_name:	NET_IPV4_ROUTE_REDIRECT_NUMBER,
		procname:	"redirect_number",
		data:		&ip_rt_redirect_number,
		maxlen:		sizeof(int),
		mode:		0644,
		proc_handler:	&proc_dointvec,
	},
	{
		ctl_name:	NET_IPV4_ROUTE_REDIRECT_SILENCE,
		procname:	"redirect_silence",
		data:		&ip_rt_redirect_silence,
		maxlen:		sizeof(int),
		mode:		0644,
		proc_handler:	&proc_dointvec,
	},
	{
		ctl_name:	NET_IPV4_ROUTE_ERROR_COST,
		procname:	"error_cost",
		data:		&ip_rt_error_cost,
		maxlen:		sizeof(int),
		mode:		0644,
		proc_handler:	&proc_dointvec,
	},
	{
		ctl_name:	NET_IPV4_ROUTE_ERROR_BURST,
		procname:	"error_burst",
		data:		&ip_rt_error_burst,
		maxlen:		sizeof(int),
		mode:		0644,
		proc_handler:	&proc_dointvec,
	},
	{
		ctl_name:	NET_IPV4_ROUTE_GC_ELASTICITY,
		procname:	"gc_elasticity",
		data:		&ip_rt_gc_elasticity,
		maxlen:		sizeof(int),
		mode:		0644,
		proc_handler:	&proc_dointvec,
	},
	{
		ctl_name:	NET_IPV4_ROUTE_MTU_EXPIRES,
		procname:	"mtu_expires",
		data:		&ip_rt_mtu_expires,
		maxlen:		sizeof(int),
		mode:		0644,
		proc_handler:	&proc_dointvec_jiffies,
		strategy:	&sysctl_jiffies,
	},
	{
		ctl_name:	NET_IPV4_ROUTE_MIN_PMTU,
		procname:	"min_pmtu",
		data:		&ip_rt_min_pmtu,
		maxlen:		sizeof(int),
		mode:		0644,
		proc_handler:	&proc_dointvec,
	},
	{
		ctl_name:	NET_IPV4_ROUTE_MIN_ADVMSS,
		procname:	"min_adv_mss",
		data:		&ip_rt_min_advmss,
		maxlen:		sizeof(int),
		mode:		0644,
		proc_handler:	&proc_dointvec,
	},
	{
		ctl_name:	NET_IPV4_ROUTE_SECRET_INTERVAL,
		procname:	"secret_interval",
		data:		&ip_rt_secret_interval,
		maxlen:		sizeof(int),
		mode:		0644,
		proc_handler:	&proc_dointvec_jiffies,
		strategy:	&sysctl_jiffies,
	},
	 { 0 }
};
#endif

#ifdef CONFIG_NET_CLS_ROUTE
struct ip_rt_acct *ip_rt_acct;

/* This code sucks.  But you should have seen it before! --RR */

/* IP route accounting ptr for this logical cpu number. */
#define IP_RT_ACCT_CPU(i) (ip_rt_acct + cpu_logical_map(i) * 256)

static int ip_rt_acct_read(char *buffer, char **start, off_t offset,
			   int length, int *eof, void *data)
{
	unsigned int i;

	if ((offset & 3) || (length & 3))
		return -EIO;

	if (offset >= sizeof(struct ip_rt_acct) * 256) {
		*eof = 1;
		return 0;
	}

	if (offset + length >= sizeof(struct ip_rt_acct) * 256) {
		length = sizeof(struct ip_rt_acct) * 256 - offset;
		*eof = 1;
	}

	offset /= sizeof(u32);

	if (length > 0) {
		u32 *src = ((u32 *) IP_RT_ACCT_CPU(0)) + offset;
		u32 *dst = (u32 *) buffer;

		/* Copy first cpu. */
		*start = buffer;
		memcpy(dst, src, length);

		/* Add the other cpus in, one int at a time */
		for (i = 1; i < smp_num_cpus; i++) {
			unsigned int j;

			src = ((u32 *) IP_RT_ACCT_CPU(i)) + offset;

			for (j = 0; j < length/4; j++)
				dst[j] += src[j];
		}
	}
	return length;
}
#endif

void __init ip_rt_init(void)
{
	int i, order, goal;

	rt_hash_rnd = (int) ((num_physpages ^ (num_physpages>>8)) ^
			     (jiffies ^ (jiffies >> 7)));

#ifdef CONFIG_NET_CLS_ROUTE
	for (order = 0;
	     (PAGE_SIZE << order) < 256 * sizeof(struct ip_rt_acct) * NR_CPUS; order++)
		/* NOTHING */;
	ip_rt_acct = (struct ip_rt_acct *)__get_free_pages(GFP_KERNEL, order);
	if (!ip_rt_acct)
		panic("IP: failed to allocate ip_rt_acct\n");
	memset(ip_rt_acct, 0, PAGE_SIZE << order);
#endif

	ipv4_dst_ops.kmem_cachep = kmem_cache_create("ip_dst_cache",
						     sizeof(struct rtable),
						     0, SLAB_HWCACHE_ALIGN,
						     NULL, NULL);

	if (!ipv4_dst_ops.kmem_cachep)
		panic("IP: failed to allocate ip_dst_cache\n");

	goal = num_physpages >> (26 - PAGE_SHIFT);

	for (order = 0; (1UL << order) < goal; order++)
		/* NOTHING */;

	do {
		rt_hash_mask = (1UL << order) * PAGE_SIZE /
			sizeof(struct rt_hash_bucket);
		while (rt_hash_mask & (rt_hash_mask - 1))
			rt_hash_mask--;
		rt_hash_table = (struct rt_hash_bucket *)
			__get_free_pages(GFP_ATOMIC, order);
	} while (rt_hash_table == NULL && --order > 0);

	if (!rt_hash_table)
		panic("Failed to allocate IP route cache hash table\n");

	printk(KERN_INFO "IP: routing cache hash table of %u buckets, %ldKbytes\n",
	       rt_hash_mask,
	       (long) (rt_hash_mask * sizeof(struct rt_hash_bucket)) / 1024);

	for (rt_hash_log = 0; (1 << rt_hash_log) != rt_hash_mask; rt_hash_log++)
		/* NOTHING */;

	rt_hash_mask--;
	for (i = 0; i <= rt_hash_mask; i++) {
		rt_hash_table[i].lock = RW_LOCK_UNLOCKED;
		rt_hash_table[i].chain = NULL;
	}

	ipv4_dst_ops.gc_thresh = (rt_hash_mask + 1);
	ip_rt_max_size = (rt_hash_mask + 1) * 16;

	devinet_init();
	ip_fib_init();

	rt_flush_timer.function = rt_run_flush;
	rt_periodic_timer.function = rt_check_expire;
	rt_secret_timer.function = rt_secret_rebuild;

	/* All the timers, started at system startup tend
	   to synchronize. Perturb it a bit.
	 */
	rt_periodic_timer.expires = jiffies + net_random() % ip_rt_gc_interval +
					ip_rt_gc_interval;
	add_timer(&rt_periodic_timer);

	rt_secret_timer.expires = jiffies + net_random() % ip_rt_secret_interval +
		ip_rt_secret_interval;
	add_timer(&rt_secret_timer);

	proc_net_create ("rt_cache", 0, rt_cache_get_info);
	proc_net_create ("rt_cache_stat", 0, rt_cache_stat_get_info);
#ifdef CONFIG_NET_CLS_ROUTE
	create_proc_read_entry("net/rt_acct", 0, 0, ip_rt_acct_read, NULL);
#endif
}
