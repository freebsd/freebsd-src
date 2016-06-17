/*
 *	Generic address resolution entity
 *
 *	Authors:
 *	Pedro Roque		<roque@di.fc.ul.pt>
 *	Alexey Kuznetsov	<kuznet@ms2.inr.ac.ru>
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *	Fixes:
 *	Vitaly E. Lavrov	releasing NULL neighbor in neigh_add.
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/socket.h>
#include <linux/sched.h>
#include <linux/netdevice.h>
#ifdef CONFIG_SYSCTL
#include <linux/sysctl.h>
#endif
#include <net/neighbour.h>
#include <net/dst.h>
#include <net/sock.h>
#include <linux/rtnetlink.h>

#define NEIGH_DEBUG 1

#define NEIGH_PRINTK(x...) printk(x)
#define NEIGH_NOPRINTK(x...) do { ; } while(0)
#define NEIGH_PRINTK0 NEIGH_PRINTK
#define NEIGH_PRINTK1 NEIGH_NOPRINTK
#define NEIGH_PRINTK2 NEIGH_NOPRINTK

#if NEIGH_DEBUG >= 1
#undef NEIGH_PRINTK1
#define NEIGH_PRINTK1 NEIGH_PRINTK
#endif
#if NEIGH_DEBUG >= 2
#undef NEIGH_PRINTK2
#define NEIGH_PRINTK2 NEIGH_PRINTK
#endif

static void neigh_timer_handler(unsigned long arg);
#ifdef CONFIG_ARPD
static void neigh_app_notify(struct neighbour *n);
#endif
static int pneigh_ifdown(struct neigh_table *tbl, struct net_device *dev);
void neigh_changeaddr(struct neigh_table *tbl, struct net_device *dev);

static int neigh_glbl_allocs;
static struct neigh_table *neigh_tables;

/*
   Neighbour hash table buckets are protected with rwlock tbl->lock.

   - All the scans/updates to hash buckets MUST be made under this lock.
   - NOTHING clever should be made under this lock: no callbacks
     to protocol backends, no attempts to send something to network.
     It will result in deadlocks, if backend/driver wants to use neighbour
     cache.
   - If the entry requires some non-trivial actions, increase
     its reference count and release table lock.
 
   Neighbour entries are protected:
   - with reference count.
   - with rwlock neigh->lock

   Reference count prevents destruction.

   neigh->lock mainly serializes ll address data and its validity state.
   However, the same lock is used to protect another entry fields:
    - timer
    - resolution queue

   Again, nothing clever shall be made under neigh->lock,
   the most complicated procedure, which we allow is dev->hard_header.
   It is supposed, that dev->hard_header is simplistic and does
   not make callbacks to neighbour tables.

   The last lock is neigh_tbl_lock. It is pure SMP lock, protecting
   list of neighbour tables. This list is used only in process context,
 */

static rwlock_t neigh_tbl_lock = RW_LOCK_UNLOCKED;

static int neigh_blackhole(struct sk_buff *skb)
{
	kfree_skb(skb);
	return -ENETDOWN;
}

/*
 * It is random distribution in the interval (1/2)*base...(3/2)*base.
 * It corresponds to default IPv6 settings and is not overridable,
 * because it is really reasonable choice.
 */

unsigned long neigh_rand_reach_time(unsigned long base)
{
	return (net_random() % base) + (base>>1);
}


static int neigh_forced_gc(struct neigh_table *tbl)
{
	int shrunk = 0;
	int i;

	for (i=0; i<=NEIGH_HASHMASK; i++) {
		struct neighbour *n, **np;

		np = &tbl->hash_buckets[i];
		write_lock_bh(&tbl->lock);
		while ((n = *np) != NULL) {
			/* Neighbour record may be discarded if:
			   - nobody refers to it.
			   - it is not permanent
			   - (NEW and probably wrong)
			     INCOMPLETE entries are kept at least for
			     n->parms->retrans_time, otherwise we could
			     flood network with resolution requests.
			     It is not clear, what is better table overflow
			     or flooding.
			 */
			write_lock(&n->lock);
			if (atomic_read(&n->refcnt) == 1 &&
			    !(n->nud_state&NUD_PERMANENT) &&
			    (n->nud_state != NUD_INCOMPLETE ||
			     jiffies - n->used > n->parms->retrans_time)) {
				*np = n->next;
				n->dead = 1;
				shrunk = 1;
				write_unlock(&n->lock);
				neigh_release(n);
				continue;
			}
			write_unlock(&n->lock);
			np = &n->next;
		}
		write_unlock_bh(&tbl->lock);
	}
	
	tbl->last_flush = jiffies;
	return shrunk;
}

static int neigh_del_timer(struct neighbour *n)
{
	if (n->nud_state & NUD_IN_TIMER) {
		if (del_timer(&n->timer)) {
			neigh_release(n);
			return 1;
		}
	}
	return 0;
}

static void pneigh_queue_purge(struct sk_buff_head *list)
{
	struct sk_buff *skb;

	while ((skb = skb_dequeue(list)) != NULL) {
		dev_put(skb->dev);
		kfree_skb(skb);
	}
}

void neigh_changeaddr(struct neigh_table *tbl, struct net_device *dev)
{
	int i;

	write_lock_bh(&tbl->lock);

	for (i=0; i <= NEIGH_HASHMASK; i++) {
		struct neighbour *n, **np;

		np = &tbl->hash_buckets[i];
		while ((n = *np) != NULL) {
			if (dev && n->dev != dev) {
				np = &n->next;
				continue;
			}
			*np = n->next;
			write_lock_bh(&n->lock);
			n->dead = 1;
			neigh_del_timer(n);
			write_unlock_bh(&n->lock);
			neigh_release(n);
		}
	}

        write_unlock_bh(&tbl->lock);
}

int neigh_ifdown(struct neigh_table *tbl, struct net_device *dev)
{
	int i;

	write_lock_bh(&tbl->lock);

	for (i=0; i<=NEIGH_HASHMASK; i++) {
		struct neighbour *n, **np;

		np = &tbl->hash_buckets[i];
		while ((n = *np) != NULL) {
			if (dev && n->dev != dev) {
				np = &n->next;
				continue;
			}
			*np = n->next;
			write_lock(&n->lock);
			neigh_del_timer(n);
			n->dead = 1;

			if (atomic_read(&n->refcnt) != 1) {
				/* The most unpleasant situation.
				   We must destroy neighbour entry,
				   but someone still uses it.

				   The destroy will be delayed until
				   the last user releases us, but
				   we must kill timers etc. and move
				   it to safe state.
				 */
				n->parms = &tbl->parms;
				skb_queue_purge(&n->arp_queue);
				n->output = neigh_blackhole;
				if (n->nud_state&NUD_VALID)
					n->nud_state = NUD_NOARP;
				else
					n->nud_state = NUD_NONE;
				NEIGH_PRINTK2("neigh %p is stray.\n", n);
			}
			write_unlock(&n->lock);
			neigh_release(n);
		}
	}

	pneigh_ifdown(tbl, dev);
	write_unlock_bh(&tbl->lock);

	del_timer_sync(&tbl->proxy_timer);
	pneigh_queue_purge(&tbl->proxy_queue);
	return 0;
}

static struct neighbour *neigh_alloc(struct neigh_table *tbl)
{
	struct neighbour *n;
	unsigned long now = jiffies;

	if (tbl->entries > tbl->gc_thresh3 ||
	    (tbl->entries > tbl->gc_thresh2 &&
	     now - tbl->last_flush > 5*HZ)) {
		if (neigh_forced_gc(tbl) == 0 &&
		    tbl->entries > tbl->gc_thresh3)
			return NULL;
	}

	n = kmem_cache_alloc(tbl->kmem_cachep, SLAB_ATOMIC);
	if (n == NULL)
		return NULL;

	memset(n, 0, tbl->entry_size);

	skb_queue_head_init(&n->arp_queue);
	n->lock = RW_LOCK_UNLOCKED;
	n->updated = n->used = now;
	n->nud_state = NUD_NONE;
	n->output = neigh_blackhole;
	n->parms = &tbl->parms;
	init_timer(&n->timer);
	n->timer.function = neigh_timer_handler;
	n->timer.data = (unsigned long)n;
	tbl->stats.allocs++;
	neigh_glbl_allocs++;
	tbl->entries++;
	n->tbl = tbl;
	atomic_set(&n->refcnt, 1);
	n->dead = 1;
	return n;
}

struct neighbour *neigh_lookup(struct neigh_table *tbl, const void *pkey,
			       struct net_device *dev)
{
	struct neighbour *n;
	u32 hash_val;
	int key_len = tbl->key_len;

	hash_val = tbl->hash(pkey, dev);

	read_lock_bh(&tbl->lock);
	for (n = tbl->hash_buckets[hash_val]; n; n = n->next) {
		if (dev == n->dev &&
		    memcmp(n->primary_key, pkey, key_len) == 0) {
			neigh_hold(n);
			break;
		}
	}
	read_unlock_bh(&tbl->lock);
	return n;
}

struct neighbour * neigh_create(struct neigh_table *tbl, const void *pkey,
				struct net_device *dev)
{
	struct neighbour *n, *n1;
	u32 hash_val;
	int key_len = tbl->key_len;
	int error;

	n = neigh_alloc(tbl);
	if (n == NULL)
		return ERR_PTR(-ENOBUFS);

	memcpy(n->primary_key, pkey, key_len);
	n->dev = dev;
	dev_hold(dev);

	/* Protocol specific setup. */
	if (tbl->constructor &&	(error = tbl->constructor(n)) < 0) {
		neigh_release(n);
		return ERR_PTR(error);
	}

	/* Device specific setup. */
	if (n->parms->neigh_setup &&
	    (error = n->parms->neigh_setup(n)) < 0) {
		neigh_release(n);
		return ERR_PTR(error);
	}

	n->confirmed = jiffies - (n->parms->base_reachable_time<<1);

	hash_val = tbl->hash(pkey, dev);

	write_lock_bh(&tbl->lock);
	for (n1 = tbl->hash_buckets[hash_val]; n1; n1 = n1->next) {
		if (dev == n1->dev &&
		    memcmp(n1->primary_key, pkey, key_len) == 0) {
			neigh_hold(n1);
			write_unlock_bh(&tbl->lock);
			neigh_release(n);
			return n1;
		}
	}

	n->next = tbl->hash_buckets[hash_val];
	tbl->hash_buckets[hash_val] = n;
	n->dead = 0;
	neigh_hold(n);
	write_unlock_bh(&tbl->lock);
	NEIGH_PRINTK2("neigh %p is created.\n", n);
	return n;
}

struct pneigh_entry * pneigh_lookup(struct neigh_table *tbl, const void *pkey,
				    struct net_device *dev, int creat)
{
	struct pneigh_entry *n;
	u32 hash_val;
	int key_len = tbl->key_len;

	hash_val = *(u32*)(pkey + key_len - 4);
	hash_val ^= (hash_val>>16);
	hash_val ^= hash_val>>8;
	hash_val ^= hash_val>>4;
	hash_val &= PNEIGH_HASHMASK;

	read_lock_bh(&tbl->lock);

	for (n = tbl->phash_buckets[hash_val]; n; n = n->next) {
		if (memcmp(n->key, pkey, key_len) == 0 &&
		    (n->dev == dev || !n->dev)) {
			read_unlock_bh(&tbl->lock);
			return n;
		}
	}
	read_unlock_bh(&tbl->lock);
	if (!creat)
		return NULL;

	n = kmalloc(sizeof(*n) + key_len, GFP_KERNEL);
	if (n == NULL)
		return NULL;

	memcpy(n->key, pkey, key_len);
	n->dev = dev;

	if (tbl->pconstructor && tbl->pconstructor(n)) {
		kfree(n);
		return NULL;
	}

	write_lock_bh(&tbl->lock);
	n->next = tbl->phash_buckets[hash_val];
	tbl->phash_buckets[hash_val] = n;
	write_unlock_bh(&tbl->lock);
	return n;
}


int pneigh_delete(struct neigh_table *tbl, const void *pkey, struct net_device *dev)
{
	struct pneigh_entry *n, **np;
	u32 hash_val;
	int key_len = tbl->key_len;

	hash_val = *(u32*)(pkey + key_len - 4);
	hash_val ^= (hash_val>>16);
	hash_val ^= hash_val>>8;
	hash_val ^= hash_val>>4;
	hash_val &= PNEIGH_HASHMASK;

	for (np = &tbl->phash_buckets[hash_val]; (n=*np) != NULL; np = &n->next) {
		if (memcmp(n->key, pkey, key_len) == 0 && n->dev == dev) {
			write_lock_bh(&tbl->lock);
			*np = n->next;
			write_unlock_bh(&tbl->lock);
			if (tbl->pdestructor)
				tbl->pdestructor(n);
			kfree(n);
			return 0;
		}
	}
	return -ENOENT;
}

static int pneigh_ifdown(struct neigh_table *tbl, struct net_device *dev)
{
	struct pneigh_entry *n, **np;
	u32 h;

	for (h=0; h<=PNEIGH_HASHMASK; h++) {
		np = &tbl->phash_buckets[h]; 
		while ((n=*np) != NULL) {
			if (n->dev == dev || dev == NULL) {
				*np = n->next;
				if (tbl->pdestructor)
					tbl->pdestructor(n);
				kfree(n);
				continue;
			}
			np = &n->next;
		}
	}
	return -ENOENT;
}


/*
 *	neighbour must already be out of the table;
 *
 */
void neigh_destroy(struct neighbour *neigh)
{	
	struct hh_cache *hh;

	if (!neigh->dead) {
		printk("Destroying alive neighbour %p\n", neigh);
		dump_stack();
		return;
	}

	if (neigh_del_timer(neigh))
		printk("Impossible event.\n");

	while ((hh = neigh->hh) != NULL) {
		neigh->hh = hh->hh_next;
		hh->hh_next = NULL;
		write_lock_bh(&hh->hh_lock);
		hh->hh_output = neigh_blackhole;
		write_unlock_bh(&hh->hh_lock);
		if (atomic_dec_and_test(&hh->hh_refcnt))
			kfree(hh);
	}

	if (neigh->ops && neigh->ops->destructor)
		(neigh->ops->destructor)(neigh);

	skb_queue_purge(&neigh->arp_queue);

	dev_put(neigh->dev);

	NEIGH_PRINTK2("neigh %p is destroyed.\n", neigh);

	neigh_glbl_allocs--;
	neigh->tbl->entries--;
	kmem_cache_free(neigh->tbl->kmem_cachep, neigh);
}

/* Neighbour state is suspicious;
   disable fast path.

   Called with write_locked neigh.
 */
static void neigh_suspect(struct neighbour *neigh)
{
	struct hh_cache *hh;

	NEIGH_PRINTK2("neigh %p is suspected.\n", neigh);

	neigh->output = neigh->ops->output;

	for (hh = neigh->hh; hh; hh = hh->hh_next)
		hh->hh_output = neigh->ops->output;
}

/* Neighbour state is OK;
   enable fast path.

   Called with write_locked neigh.
 */
static void neigh_connect(struct neighbour *neigh)
{
	struct hh_cache *hh;

	NEIGH_PRINTK2("neigh %p is connected.\n", neigh);

	neigh->output = neigh->ops->connected_output;

	for (hh = neigh->hh; hh; hh = hh->hh_next)
		hh->hh_output = neigh->ops->hh_output;
}

/*
   Transitions NUD_STALE <-> NUD_REACHABLE do not occur
   when fast path is built: we have no timers associated with
   these states, we do not have time to check state when sending.
   neigh_periodic_timer check periodically neigh->confirmed
   time and moves NUD_REACHABLE -> NUD_STALE.

   If a routine wants to know TRUE entry state, it calls
   neigh_sync before checking state.

   Called with write_locked neigh.
 */

static void neigh_sync(struct neighbour *n)
{
	unsigned long now = jiffies;
	u8 state = n->nud_state;

	if (state&(NUD_NOARP|NUD_PERMANENT))
		return;
	if (state&NUD_REACHABLE) {
		if (now - n->confirmed > n->parms->reachable_time) {
			n->nud_state = NUD_STALE;
			neigh_suspect(n);
		}
	} else if (state&NUD_VALID) {
		if (now - n->confirmed < n->parms->reachable_time) {
			neigh_del_timer(n);
			n->nud_state = NUD_REACHABLE;
			neigh_connect(n);
		}
	}
}

static void SMP_TIMER_NAME(neigh_periodic_timer)(unsigned long arg)
{
	struct neigh_table *tbl = (struct neigh_table*)arg;
	unsigned long now = jiffies;
	int i;


	write_lock(&tbl->lock);

	/*
	 *	periodicly recompute ReachableTime from random function
	 */
	
	if (now - tbl->last_rand > 300*HZ) {
		struct neigh_parms *p;
		tbl->last_rand = now;
		for (p=&tbl->parms; p; p = p->next)
			p->reachable_time = neigh_rand_reach_time(p->base_reachable_time);
	}

	for (i=0; i <= NEIGH_HASHMASK; i++) {
		struct neighbour *n, **np;

		np = &tbl->hash_buckets[i];
		while ((n = *np) != NULL) {
			unsigned state;

			write_lock(&n->lock);

			state = n->nud_state;
			if (state&(NUD_PERMANENT|NUD_IN_TIMER)) {
				write_unlock(&n->lock);
				goto next_elt;
			}

			if ((long)(n->used - n->confirmed) < 0)
				n->used = n->confirmed;

			if (atomic_read(&n->refcnt) == 1 &&
			    (state == NUD_FAILED || now - n->used > n->parms->gc_staletime)) {
				*np = n->next;
				n->dead = 1;
				write_unlock(&n->lock);
				neigh_release(n);
				continue;
			}

			if (n->nud_state&NUD_REACHABLE &&
			    now - n->confirmed > n->parms->reachable_time) {
				n->nud_state = NUD_STALE;
				neigh_suspect(n);
			}
			write_unlock(&n->lock);

next_elt:
			np = &n->next;
		}
	}

	mod_timer(&tbl->gc_timer, now + tbl->gc_interval);
	write_unlock(&tbl->lock);
}

#ifdef CONFIG_SMP
static void neigh_periodic_timer(unsigned long arg)
{
	struct neigh_table *tbl = (struct neigh_table*)arg;
	
	tasklet_schedule(&tbl->gc_task);
}
#endif

static __inline__ int neigh_max_probes(struct neighbour *n)
{
	struct neigh_parms *p = n->parms;
	return p->ucast_probes + p->app_probes + p->mcast_probes;
}


/* Called when a timer expires for a neighbour entry. */

static void neigh_timer_handler(unsigned long arg) 
{
	unsigned long now = jiffies;
	struct neighbour *neigh = (struct neighbour*)arg;
	unsigned state;
	int notify = 0;

	write_lock(&neigh->lock);

	state = neigh->nud_state;

	if (!(state&NUD_IN_TIMER)) {
#ifndef CONFIG_SMP
		printk("neigh: timer & !nud_in_timer\n");
#endif
		goto out;
	}

	if ((state&NUD_VALID) &&
	    now - neigh->confirmed < neigh->parms->reachable_time) {
		neigh->nud_state = NUD_REACHABLE;
		NEIGH_PRINTK2("neigh %p is still alive.\n", neigh);
		neigh_connect(neigh);
		goto out;
	}
	if (state == NUD_DELAY) {
		NEIGH_PRINTK2("neigh %p is probed.\n", neigh);
		neigh->nud_state = NUD_PROBE;
		atomic_set(&neigh->probes, 0);
	}

	if (atomic_read(&neigh->probes) >= neigh_max_probes(neigh)) {
		struct sk_buff *skb;

		neigh->nud_state = NUD_FAILED;
		notify = 1;
		neigh->tbl->stats.res_failed++;
		NEIGH_PRINTK2("neigh %p is failed.\n", neigh);

		/* It is very thin place. report_unreachable is very complicated
		   routine. Particularly, it can hit the same neighbour entry!
		   
		   So that, we try to be accurate and avoid dead loop. --ANK
		 */
		while(neigh->nud_state==NUD_FAILED && (skb=__skb_dequeue(&neigh->arp_queue)) != NULL) {
			write_unlock(&neigh->lock);
			neigh->ops->error_report(neigh, skb);
			write_lock(&neigh->lock);
		}
		skb_queue_purge(&neigh->arp_queue);
		goto out;
	}

	neigh->timer.expires = now + neigh->parms->retrans_time;
	add_timer(&neigh->timer);
	write_unlock(&neigh->lock);

	neigh->ops->solicit(neigh, skb_peek(&neigh->arp_queue));
	atomic_inc(&neigh->probes);
	return;

out:
	write_unlock(&neigh->lock);
#ifdef CONFIG_ARPD
	if (notify && neigh->parms->app_probes)
		neigh_app_notify(neigh);
#endif
	neigh_release(neigh);
}

int __neigh_event_send(struct neighbour *neigh, struct sk_buff *skb)
{
	write_lock_bh(&neigh->lock);
	if (!(neigh->nud_state&(NUD_CONNECTED|NUD_DELAY|NUD_PROBE))) {
		if (!(neigh->nud_state&(NUD_STALE|NUD_INCOMPLETE))) {
			if (neigh->parms->mcast_probes + neigh->parms->app_probes) {
				atomic_set(&neigh->probes, neigh->parms->ucast_probes);
				neigh->nud_state = NUD_INCOMPLETE;
				neigh_hold(neigh);
				neigh->timer.expires = jiffies + neigh->parms->retrans_time;
				add_timer(&neigh->timer);
				write_unlock_bh(&neigh->lock);
				neigh->ops->solicit(neigh, skb);
				atomic_inc(&neigh->probes);
				write_lock_bh(&neigh->lock);
			} else {
				neigh->nud_state = NUD_FAILED;
				write_unlock_bh(&neigh->lock);

				if (skb)
					kfree_skb(skb);
				return 1;
			}
		}
		if (neigh->nud_state == NUD_INCOMPLETE) {
			if (skb) {
				if (skb_queue_len(&neigh->arp_queue) >= neigh->parms->queue_len) {
					struct sk_buff *buff;
					buff = neigh->arp_queue.next;
					__skb_unlink(buff, &neigh->arp_queue);
					kfree_skb(buff);
				}
				__skb_queue_tail(&neigh->arp_queue, skb);
			}
			write_unlock_bh(&neigh->lock);
			return 1;
		}
		if (neigh->nud_state == NUD_STALE) {
			NEIGH_PRINTK2("neigh %p is delayed.\n", neigh);
			neigh_hold(neigh);
			neigh->nud_state = NUD_DELAY;
			neigh->timer.expires = jiffies + neigh->parms->delay_probe_time;
			add_timer(&neigh->timer);
		}
	}
	write_unlock_bh(&neigh->lock);
	return 0;
}

static __inline__ void neigh_update_hhs(struct neighbour *neigh)
{
	struct hh_cache *hh;
	void (*update)(struct hh_cache*, struct net_device*, unsigned char*) =
		neigh->dev->header_cache_update;

	if (update) {
		for (hh=neigh->hh; hh; hh=hh->hh_next) {
			write_lock_bh(&hh->hh_lock);
			update(hh, neigh->dev, neigh->ha);
			write_unlock_bh(&hh->hh_lock);
		}
	}
}



/* Generic update routine.
   -- lladdr is new lladdr or NULL, if it is not supplied.
   -- new    is new state.
   -- override==1 allows to override existing lladdr, if it is different.
   -- arp==0 means that the change is administrative.

   Caller MUST hold reference count on the entry.
 */

int neigh_update(struct neighbour *neigh, const u8 *lladdr, u8 new, int override, int arp)
{
	u8 old;
	int err;
	int notify = 0;
	struct net_device *dev = neigh->dev;

	write_lock_bh(&neigh->lock);
	old = neigh->nud_state;

	err = -EPERM;
	if (arp && (old&(NUD_NOARP|NUD_PERMANENT)))
		goto out;

	if (!(new&NUD_VALID)) {
		neigh_del_timer(neigh);
		if (old&NUD_CONNECTED)
			neigh_suspect(neigh);
		neigh->nud_state = new;
		err = 0;
		notify = old&NUD_VALID;
		goto out;
	}

	/* Compare new lladdr with cached one */
	if (dev->addr_len == 0) {
		/* First case: device needs no address. */
		lladdr = neigh->ha;
	} else if (lladdr) {
		/* The second case: if something is already cached
		   and a new address is proposed:
		   - compare new & old
		   - if they are different, check override flag
		 */
		if (old&NUD_VALID) {
			if (memcmp(lladdr, neigh->ha, dev->addr_len) == 0)
				lladdr = neigh->ha;
			else if (!override)
				goto out;
		}
	} else {
		/* No address is supplied; if we know something,
		   use it, otherwise discard the request.
		 */
		err = -EINVAL;
		if (!(old&NUD_VALID))
			goto out;
		lladdr = neigh->ha;
	}

	neigh_sync(neigh);
	old = neigh->nud_state;
	if (new&NUD_CONNECTED)
		neigh->confirmed = jiffies;
	neigh->updated = jiffies;

	/* If entry was valid and address is not changed,
	   do not change entry state, if new one is STALE.
	 */
	err = 0;
	if (old&NUD_VALID) {
		if (lladdr == neigh->ha)
			if (new == old || (new == NUD_STALE && (old&NUD_CONNECTED)))
				goto out;
	}
	neigh_del_timer(neigh);
	neigh->nud_state = new;
	if (lladdr != neigh->ha) {
		memcpy(&neigh->ha, lladdr, dev->addr_len);
		neigh_update_hhs(neigh);
		if (!(new&NUD_CONNECTED))
			neigh->confirmed = jiffies - (neigh->parms->base_reachable_time<<1);
#ifdef CONFIG_ARPD
		notify = 1;
#endif
	}
	if (new == old)
		goto out;
	if (new&NUD_CONNECTED)
		neigh_connect(neigh);
	else
		neigh_suspect(neigh);
	if (!(old&NUD_VALID)) {
		struct sk_buff *skb;

		/* Again: avoid dead loop if something went wrong */

		while (neigh->nud_state&NUD_VALID &&
		       (skb=__skb_dequeue(&neigh->arp_queue)) != NULL) {
			struct neighbour *n1 = neigh;
			write_unlock_bh(&neigh->lock);
			/* On shaper/eql skb->dst->neighbour != neigh :( */
			if (skb->dst && skb->dst->neighbour)
				n1 = skb->dst->neighbour;
			n1->output(skb);
			write_lock_bh(&neigh->lock);
		}
		skb_queue_purge(&neigh->arp_queue);
	}
out:
	write_unlock_bh(&neigh->lock);
#ifdef CONFIG_ARPD
	if (notify && neigh->parms->app_probes)
		neigh_app_notify(neigh);
#endif
	return err;
}

struct neighbour * neigh_event_ns(struct neigh_table *tbl,
				  u8 *lladdr, void *saddr,
				  struct net_device *dev)
{
	struct neighbour *neigh;

	neigh = __neigh_lookup(tbl, saddr, dev, lladdr || !dev->addr_len);
	if (neigh)
		neigh_update(neigh, lladdr, NUD_STALE, 1, 1);
	return neigh;
}

static void neigh_hh_init(struct neighbour *n, struct dst_entry *dst, u16 protocol)
{
	struct hh_cache	*hh = NULL;
	struct net_device *dev = dst->dev;

	for (hh=n->hh; hh; hh = hh->hh_next)
		if (hh->hh_type == protocol)
			break;

	if (!hh && (hh = kmalloc(sizeof(*hh), GFP_ATOMIC)) != NULL) {
		memset(hh, 0, sizeof(struct hh_cache));
		hh->hh_lock = RW_LOCK_UNLOCKED;
		hh->hh_type = protocol;
		atomic_set(&hh->hh_refcnt, 0);
		hh->hh_next = NULL;
		if (dev->hard_header_cache(n, hh)) {
			kfree(hh);
			hh = NULL;
		} else {
			atomic_inc(&hh->hh_refcnt);
			hh->hh_next = n->hh;
			n->hh = hh;
			if (n->nud_state&NUD_CONNECTED)
				hh->hh_output = n->ops->hh_output;
			else
				hh->hh_output = n->ops->output;
		}
	}
	if (hh)	{
		atomic_inc(&hh->hh_refcnt);
		dst->hh = hh;
	}
}

/* This function can be used in contexts, where only old dev_queue_xmit
   worked, f.e. if you want to override normal output path (eql, shaper),
   but resolution is not made yet.
 */

int neigh_compat_output(struct sk_buff *skb)
{
	struct net_device *dev = skb->dev;

	__skb_pull(skb, skb->nh.raw - skb->data);

	if (dev->hard_header &&
	    dev->hard_header(skb, dev, ntohs(skb->protocol), NULL, NULL, skb->len) < 0 &&
	    dev->rebuild_header(skb))
		return 0;

	return dev_queue_xmit(skb);
}

/* Slow and careful. */

int neigh_resolve_output(struct sk_buff *skb)
{
	struct dst_entry *dst = skb->dst;
	struct neighbour *neigh;

	if (!dst || !(neigh = dst->neighbour))
		goto discard;

	__skb_pull(skb, skb->nh.raw - skb->data);

	if (neigh_event_send(neigh, skb) == 0) {
		int err;
		struct net_device *dev = neigh->dev;
		if (dev->hard_header_cache && dst->hh == NULL) {
			write_lock_bh(&neigh->lock);
			if (dst->hh == NULL)
				neigh_hh_init(neigh, dst, dst->ops->protocol);
			err = dev->hard_header(skb, dev, ntohs(skb->protocol), neigh->ha, NULL, skb->len);
			write_unlock_bh(&neigh->lock);
		} else {
			read_lock_bh(&neigh->lock);
			err = dev->hard_header(skb, dev, ntohs(skb->protocol), neigh->ha, NULL, skb->len);
			read_unlock_bh(&neigh->lock);
		}
		if (err >= 0)
			return neigh->ops->queue_xmit(skb);
		kfree_skb(skb);
		return -EINVAL;
	}
	return 0;

discard:
	NEIGH_PRINTK1("neigh_resolve_output: dst=%p neigh=%p\n", dst, dst ? dst->neighbour : NULL);
	kfree_skb(skb);
	return -EINVAL;
}

/* As fast as possible without hh cache */

int neigh_connected_output(struct sk_buff *skb)
{
	int err;
	struct dst_entry *dst = skb->dst;
	struct neighbour *neigh = dst->neighbour;
	struct net_device *dev = neigh->dev;

	__skb_pull(skb, skb->nh.raw - skb->data);

	read_lock_bh(&neigh->lock);
	err = dev->hard_header(skb, dev, ntohs(skb->protocol), neigh->ha, NULL, skb->len);
	read_unlock_bh(&neigh->lock);
	if (err >= 0)
		return neigh->ops->queue_xmit(skb);
	kfree_skb(skb);
	return -EINVAL;
}

static void neigh_proxy_process(unsigned long arg)
{
	struct neigh_table *tbl = (struct neigh_table *)arg;
	long sched_next = 0;
	unsigned long now = jiffies;
	struct sk_buff *skb;

	spin_lock(&tbl->proxy_queue.lock);

	skb = tbl->proxy_queue.next;

	while (skb != (struct sk_buff*)&tbl->proxy_queue) {
		struct sk_buff *back = skb;
		long tdif = back->stamp.tv_usec - now;

		skb = skb->next;
		if (tdif <= 0) {
			struct net_device *dev = back->dev;
			__skb_unlink(back, &tbl->proxy_queue);
			if (tbl->proxy_redo && netif_running(dev))
				tbl->proxy_redo(back);
			else
				kfree_skb(back);

			dev_put(dev);
		} else if (!sched_next || tdif < sched_next)
			sched_next = tdif;
	}
	del_timer(&tbl->proxy_timer);
	if (sched_next)
		mod_timer(&tbl->proxy_timer, jiffies + sched_next);
	spin_unlock(&tbl->proxy_queue.lock);
}

void pneigh_enqueue(struct neigh_table *tbl, struct neigh_parms *p,
		    struct sk_buff *skb)
{
	unsigned long now = jiffies;
	long sched_next = net_random()%p->proxy_delay;

	if (tbl->proxy_queue.qlen > p->proxy_qlen) {
		kfree_skb(skb);
		return;
	}
	skb->stamp.tv_sec = 0;
	skb->stamp.tv_usec = now + sched_next;

	spin_lock(&tbl->proxy_queue.lock);
	if (del_timer(&tbl->proxy_timer)) {
		long tval = tbl->proxy_timer.expires - now;
		if (tval < sched_next)
			sched_next = tval;
	}
	dst_release(skb->dst);
	skb->dst = NULL;
	dev_hold(skb->dev);
	__skb_queue_tail(&tbl->proxy_queue, skb);
	mod_timer(&tbl->proxy_timer, now + sched_next);
	spin_unlock(&tbl->proxy_queue.lock);
}


struct neigh_parms *neigh_parms_alloc(struct net_device *dev, struct neigh_table *tbl)
{
	struct neigh_parms *p;
	p = kmalloc(sizeof(*p), GFP_KERNEL);
	if (p) {
		memcpy(p, &tbl->parms, sizeof(*p));
		p->tbl = tbl;
		p->reachable_time = neigh_rand_reach_time(p->base_reachable_time);
		if (dev && dev->neigh_setup) {
			if (dev->neigh_setup(dev, p)) {
				kfree(p);
				return NULL;
			}
		}
		p->sysctl_table = NULL;
		write_lock_bh(&tbl->lock);
		p->next = tbl->parms.next;
		tbl->parms.next = p;
		write_unlock_bh(&tbl->lock);
	}
	return p;
}

void neigh_parms_release(struct neigh_table *tbl, struct neigh_parms *parms)
{
	struct neigh_parms **p;
	
	if (parms == NULL || parms == &tbl->parms)
		return;
	write_lock_bh(&tbl->lock);
	for (p = &tbl->parms.next; *p; p = &(*p)->next) {
		if (*p == parms) {
			*p = parms->next;
			write_unlock_bh(&tbl->lock);
#ifdef CONFIG_SYSCTL
			neigh_sysctl_unregister(parms);
#endif
			kfree(parms);
			return;
		}
	}
	write_unlock_bh(&tbl->lock);
	NEIGH_PRINTK1("neigh_parms_release: not found\n");
}


void neigh_table_init(struct neigh_table *tbl)
{
	unsigned long now = jiffies;

	tbl->parms.reachable_time = neigh_rand_reach_time(tbl->parms.base_reachable_time);

	if (tbl->kmem_cachep == NULL)
		tbl->kmem_cachep = kmem_cache_create(tbl->id,
						     (tbl->entry_size+15)&~15,
						     0, SLAB_HWCACHE_ALIGN,
						     NULL, NULL);

#ifdef CONFIG_SMP
	tasklet_init(&tbl->gc_task, SMP_TIMER_NAME(neigh_periodic_timer), (unsigned long)tbl);
#endif
	init_timer(&tbl->gc_timer);
	tbl->lock = RW_LOCK_UNLOCKED;
	tbl->gc_timer.data = (unsigned long)tbl;
	tbl->gc_timer.function = neigh_periodic_timer;
	tbl->gc_timer.expires = now + tbl->gc_interval + tbl->parms.reachable_time;
	add_timer(&tbl->gc_timer);

	init_timer(&tbl->proxy_timer);
	tbl->proxy_timer.data = (unsigned long)tbl;
	tbl->proxy_timer.function = neigh_proxy_process;
	skb_queue_head_init(&tbl->proxy_queue);

	tbl->last_flush = now;
	tbl->last_rand = now + tbl->parms.reachable_time*20;
	write_lock(&neigh_tbl_lock);
	tbl->next = neigh_tables;
	neigh_tables = tbl;
	write_unlock(&neigh_tbl_lock);
}

int neigh_table_clear(struct neigh_table *tbl)
{
	struct neigh_table **tp;

	/* It is not clean... Fix it to unload IPv6 module safely */
	del_timer_sync(&tbl->gc_timer);
	tasklet_kill(&tbl->gc_task);
	del_timer_sync(&tbl->proxy_timer);
	pneigh_queue_purge(&tbl->proxy_queue);
	neigh_ifdown(tbl, NULL);
	if (tbl->entries)
		printk(KERN_CRIT "neighbour leakage\n");
	write_lock(&neigh_tbl_lock);
	for (tp = &neigh_tables; *tp; tp = &(*tp)->next) {
		if (*tp == tbl) {
			*tp = tbl->next;
			break;
		}
	}
	write_unlock(&neigh_tbl_lock);
#ifdef CONFIG_SYSCTL
	neigh_sysctl_unregister(&tbl->parms);
#endif
	return 0;
}

int neigh_delete(struct sk_buff *skb, struct nlmsghdr *nlh, void *arg)
{
	struct ndmsg *ndm = NLMSG_DATA(nlh);
	struct rtattr **nda = arg;
	struct neigh_table *tbl;
	struct net_device *dev = NULL;
	int err = 0;

	if (ndm->ndm_ifindex) {
		if ((dev = dev_get_by_index(ndm->ndm_ifindex)) == NULL)
			return -ENODEV;
	}

	read_lock(&neigh_tbl_lock);
	for (tbl=neigh_tables; tbl; tbl = tbl->next) {
		struct neighbour *n;

		if (tbl->family != ndm->ndm_family)
			continue;
		read_unlock(&neigh_tbl_lock);

		err = -EINVAL;
		if (nda[NDA_DST-1] == NULL ||
		    nda[NDA_DST-1]->rta_len != RTA_LENGTH(tbl->key_len))
			goto out;

		if (ndm->ndm_flags&NTF_PROXY) {
			err = pneigh_delete(tbl, RTA_DATA(nda[NDA_DST-1]), dev);
			goto out;
		}

		if (dev == NULL)
			return -EINVAL;

		n = neigh_lookup(tbl, RTA_DATA(nda[NDA_DST-1]), dev);
		if (n) {
			err = neigh_update(n, NULL, NUD_FAILED, 1, 0);
			neigh_release(n);
		}
out:
		if (dev)
			dev_put(dev);
		return err;
	}
	read_unlock(&neigh_tbl_lock);

	if (dev)
		dev_put(dev);

	return -EADDRNOTAVAIL;
}

int neigh_add(struct sk_buff *skb, struct nlmsghdr *nlh, void *arg)
{
	struct ndmsg *ndm = NLMSG_DATA(nlh);
	struct rtattr **nda = arg;
	struct neigh_table *tbl;
	struct net_device *dev = NULL;

	if (ndm->ndm_ifindex) {
		if ((dev = dev_get_by_index(ndm->ndm_ifindex)) == NULL)
			return -ENODEV;
	}

	read_lock(&neigh_tbl_lock);
	for (tbl=neigh_tables; tbl; tbl = tbl->next) {
		int err = 0;
		int override = 1;
		struct neighbour *n;

		if (tbl->family != ndm->ndm_family)
			continue;
		read_unlock(&neigh_tbl_lock);

		err = -EINVAL;
		if (nda[NDA_DST-1] == NULL ||
		    nda[NDA_DST-1]->rta_len != RTA_LENGTH(tbl->key_len))
			goto out;
		if (ndm->ndm_flags&NTF_PROXY) {
			err = -ENOBUFS;
			if (pneigh_lookup(tbl, RTA_DATA(nda[NDA_DST-1]), dev, 1))
				err = 0;
			goto out;
		}
		if (dev == NULL)
			return -EINVAL;
		err = -EINVAL;
		if (nda[NDA_LLADDR-1] != NULL &&
		    nda[NDA_LLADDR-1]->rta_len != RTA_LENGTH(dev->addr_len))
			goto out;
		err = 0;
		n = neigh_lookup(tbl, RTA_DATA(nda[NDA_DST-1]), dev);
		if (n) {
			if (nlh->nlmsg_flags&NLM_F_EXCL)
				err = -EEXIST;
			override = nlh->nlmsg_flags&NLM_F_REPLACE;
		} else if (!(nlh->nlmsg_flags&NLM_F_CREATE))
			err = -ENOENT;
		else {
			n = __neigh_lookup_errno(tbl, RTA_DATA(nda[NDA_DST-1]), dev);
			if (IS_ERR(n)) {
				err = PTR_ERR(n);
				n = NULL;
			}
		}
		if (err == 0) {
			err = neigh_update(n, nda[NDA_LLADDR-1] ? RTA_DATA(nda[NDA_LLADDR-1]) : NULL,
					   ndm->ndm_state,
					   override, 0);
		}
		if (n)
			neigh_release(n);
out:
		if (dev)
			dev_put(dev);
		return err;
	}
	read_unlock(&neigh_tbl_lock);

	if (dev)
		dev_put(dev);
	return -EADDRNOTAVAIL;
}


static int neigh_fill_info(struct sk_buff *skb, struct neighbour *n,
			   u32 pid, u32 seq, int event)
{
	unsigned long now = jiffies;
	struct ndmsg *ndm;
	struct nlmsghdr  *nlh;
	unsigned char	 *b = skb->tail;
	struct nda_cacheinfo ci;
	int locked = 0;

	nlh = NLMSG_PUT(skb, pid, seq, event, sizeof(*ndm));
	ndm = NLMSG_DATA(nlh);
	ndm->ndm_family = n->ops->family;
	ndm->ndm_flags = n->flags;
	ndm->ndm_type = n->type;
	ndm->ndm_ifindex = n->dev->ifindex;
	RTA_PUT(skb, NDA_DST, n->tbl->key_len, n->primary_key);
	read_lock_bh(&n->lock);
	locked=1;
	ndm->ndm_state = n->nud_state;
	if (n->nud_state&NUD_VALID)
		RTA_PUT(skb, NDA_LLADDR, n->dev->addr_len, n->ha);
	ci.ndm_used = now - n->used;
	ci.ndm_confirmed = now - n->confirmed;
	ci.ndm_updated = now - n->updated;
	ci.ndm_refcnt = atomic_read(&n->refcnt) - 1;
	read_unlock_bh(&n->lock);
	locked=0;
	RTA_PUT(skb, NDA_CACHEINFO, sizeof(ci), &ci);
	nlh->nlmsg_len = skb->tail - b;
	return skb->len;

nlmsg_failure:
rtattr_failure:
	if (locked)
		read_unlock_bh(&n->lock);
	skb_trim(skb, b - skb->data);
	return -1;
}


static int neigh_dump_table(struct neigh_table *tbl, struct sk_buff *skb, struct netlink_callback *cb)
{
	struct neighbour *n;
	int h, s_h;
	int idx, s_idx;

	s_h = cb->args[1];
	s_idx = idx = cb->args[2];
	for (h=0; h <= NEIGH_HASHMASK; h++) {
		if (h < s_h) continue;
		if (h > s_h)
			s_idx = 0;
		read_lock_bh(&tbl->lock);
		for (n = tbl->hash_buckets[h], idx = 0; n;
		     n = n->next, idx++) {
			if (idx < s_idx)
				continue;
			if (neigh_fill_info(skb, n, NETLINK_CB(cb->skb).pid,
					    cb->nlh->nlmsg_seq, RTM_NEWNEIGH) <= 0) {
				read_unlock_bh(&tbl->lock);
				cb->args[1] = h;
				cb->args[2] = idx;
				return -1;
			}
		}
		read_unlock_bh(&tbl->lock);
	}

	cb->args[1] = h;
	cb->args[2] = idx;
	return skb->len;
}

int neigh_dump_info(struct sk_buff *skb, struct netlink_callback *cb)
{
	int t;
	int s_t;
	struct neigh_table *tbl;
	int family = ((struct rtgenmsg*)NLMSG_DATA(cb->nlh))->rtgen_family;

	s_t = cb->args[0];

	read_lock(&neigh_tbl_lock);
	for (tbl=neigh_tables, t=0; tbl; tbl = tbl->next, t++) {
		if (t < s_t) continue;
		if (family && tbl->family != family)
			continue;
		if (t > s_t)
			memset(&cb->args[1], 0, sizeof(cb->args)-sizeof(cb->args[0]));
		if (neigh_dump_table(tbl, skb, cb) < 0) 
			break;
	}
	read_unlock(&neigh_tbl_lock);

	cb->args[0] = t;

	return skb->len;
}

#ifdef CONFIG_ARPD
void neigh_app_ns(struct neighbour *n)
{
	struct sk_buff *skb;
	struct nlmsghdr  *nlh;
	int size = NLMSG_SPACE(sizeof(struct ndmsg)+256);

	skb = alloc_skb(size, GFP_ATOMIC);
	if (!skb)
		return;

	if (neigh_fill_info(skb, n, 0, 0, RTM_GETNEIGH) < 0) {
		kfree_skb(skb);
		return;
	}
	nlh = (struct nlmsghdr*)skb->data;
	nlh->nlmsg_flags = NLM_F_REQUEST;
	NETLINK_CB(skb).dst_groups = RTMGRP_NEIGH;
	netlink_broadcast(rtnl, skb, 0, RTMGRP_NEIGH, GFP_ATOMIC);
}

static void neigh_app_notify(struct neighbour *n)
{
	struct sk_buff *skb;
	struct nlmsghdr  *nlh;
	int size = NLMSG_SPACE(sizeof(struct ndmsg)+256);

	skb = alloc_skb(size, GFP_ATOMIC);
	if (!skb)
		return;

	if (neigh_fill_info(skb, n, 0, 0, RTM_NEWNEIGH) < 0) {
		kfree_skb(skb);
		return;
	}
	nlh = (struct nlmsghdr*)skb->data;
	NETLINK_CB(skb).dst_groups = RTMGRP_NEIGH;
	netlink_broadcast(rtnl, skb, 0, RTMGRP_NEIGH, GFP_ATOMIC);
}

#endif /* CONFIG_ARPD */

#ifdef CONFIG_SYSCTL

struct neigh_sysctl_table
{
	struct ctl_table_header *sysctl_header;
	ctl_table neigh_vars[17];
	ctl_table neigh_dev[2];
	ctl_table neigh_neigh_dir[2];
	ctl_table neigh_proto_dir[2];
	ctl_table neigh_root_dir[2];
} neigh_sysctl_template = {
	NULL,
        {{NET_NEIGH_MCAST_SOLICIT, "mcast_solicit",
         NULL, sizeof(int), 0644, NULL,
         &proc_dointvec},
	{NET_NEIGH_UCAST_SOLICIT, "ucast_solicit",
         NULL, sizeof(int), 0644, NULL,
         &proc_dointvec},
	{NET_NEIGH_APP_SOLICIT, "app_solicit",
         NULL, sizeof(int), 0644, NULL,
         &proc_dointvec},
	{NET_NEIGH_RETRANS_TIME, "retrans_time",
         NULL, sizeof(int), 0644, NULL,
         &proc_dointvec},
	{NET_NEIGH_REACHABLE_TIME, "base_reachable_time",
         NULL, sizeof(int), 0644, NULL,
         &proc_dointvec_jiffies},
	{NET_NEIGH_DELAY_PROBE_TIME, "delay_first_probe_time",
         NULL, sizeof(int), 0644, NULL,
         &proc_dointvec_jiffies},
	{NET_NEIGH_GC_STALE_TIME, "gc_stale_time",
         NULL, sizeof(int), 0644, NULL,
         &proc_dointvec_jiffies},
	{NET_NEIGH_UNRES_QLEN, "unres_qlen",
         NULL, sizeof(int), 0644, NULL,
         &proc_dointvec},
	{NET_NEIGH_PROXY_QLEN, "proxy_qlen",
         NULL, sizeof(int), 0644, NULL,
         &proc_dointvec},
	{NET_NEIGH_ANYCAST_DELAY, "anycast_delay",
         NULL, sizeof(int), 0644, NULL,
         &proc_dointvec},
	{NET_NEIGH_PROXY_DELAY, "proxy_delay",
         NULL, sizeof(int), 0644, NULL,
         &proc_dointvec},
	{NET_NEIGH_LOCKTIME, "locktime",
         NULL, sizeof(int), 0644, NULL,
         &proc_dointvec},
	{NET_NEIGH_GC_INTERVAL, "gc_interval",
         NULL, sizeof(int), 0644, NULL,
         &proc_dointvec_jiffies},
	{NET_NEIGH_GC_THRESH1, "gc_thresh1",
         NULL, sizeof(int), 0644, NULL,
         &proc_dointvec},
	{NET_NEIGH_GC_THRESH2, "gc_thresh2",
         NULL, sizeof(int), 0644, NULL,
         &proc_dointvec},
	{NET_NEIGH_GC_THRESH3, "gc_thresh3",
         NULL, sizeof(int), 0644, NULL,
         &proc_dointvec},
	 {0}},

	{{NET_PROTO_CONF_DEFAULT, "default", NULL, 0, 0555, NULL},{0}},
	{{0, "neigh", NULL, 0, 0555, NULL},{0}},
	{{0, NULL, NULL, 0, 0555, NULL},{0}},
	{{CTL_NET, "net", NULL, 0, 0555, NULL},{0}}
};

int neigh_sysctl_register(struct net_device *dev, struct neigh_parms *p,
			  int p_id, int pdev_id, char *p_name)
{
	struct neigh_sysctl_table *t;

	t = kmalloc(sizeof(*t), GFP_KERNEL);
	if (t == NULL)
		return -ENOBUFS;
	memcpy(t, &neigh_sysctl_template, sizeof(*t));
	t->neigh_vars[0].data = &p->mcast_probes;
	t->neigh_vars[1].data = &p->ucast_probes;
	t->neigh_vars[2].data = &p->app_probes;
	t->neigh_vars[3].data = &p->retrans_time;
	t->neigh_vars[4].data = &p->base_reachable_time;
	t->neigh_vars[5].data = &p->delay_probe_time;
	t->neigh_vars[6].data = &p->gc_staletime;
	t->neigh_vars[7].data = &p->queue_len;
	t->neigh_vars[8].data = &p->proxy_qlen;
	t->neigh_vars[9].data = &p->anycast_delay;
	t->neigh_vars[10].data = &p->proxy_delay;
	t->neigh_vars[11].data = &p->locktime;
	if (dev) {
		t->neigh_dev[0].procname = dev->name;
		t->neigh_dev[0].ctl_name = dev->ifindex;
		memset(&t->neigh_vars[12], 0, sizeof(ctl_table));
	} else {
		t->neigh_vars[12].data = (int*)(p+1);
		t->neigh_vars[13].data = (int*)(p+1) + 1;
		t->neigh_vars[14].data = (int*)(p+1) + 2;
		t->neigh_vars[15].data = (int*)(p+1) + 3;
	}
	t->neigh_neigh_dir[0].ctl_name = pdev_id;

	t->neigh_proto_dir[0].procname = p_name;
	t->neigh_proto_dir[0].ctl_name = p_id;

	t->neigh_dev[0].child = t->neigh_vars;
	t->neigh_neigh_dir[0].child = t->neigh_dev;
	t->neigh_proto_dir[0].child = t->neigh_neigh_dir;
	t->neigh_root_dir[0].child = t->neigh_proto_dir;

	t->sysctl_header = register_sysctl_table(t->neigh_root_dir, 0);
	if (t->sysctl_header == NULL) {
		kfree(t);
		return -ENOBUFS;
	}
	p->sysctl_table = t;
	return 0;
}

void neigh_sysctl_unregister(struct neigh_parms *p)
{
	if (p->sysctl_table) {
		struct neigh_sysctl_table *t = p->sysctl_table;
		p->sysctl_table = NULL;
		unregister_sysctl_table(t->sysctl_header);
		kfree(t);
	}
}

#endif	/* CONFIG_SYSCTL */
