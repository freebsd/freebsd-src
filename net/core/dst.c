/*
 * net/dst.c	Protocol independent destination cache.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 */

#include <linux/bitops.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/init.h>

#include <net/dst.h>

/* Locking strategy:
 * 1) Garbage collection state of dead destination cache
 *    entries is protected by dst_lock.
 * 2) GC is run only from BH context, and is the only remover
 *    of entries.
 * 3) Entries are added to the garbage list from both BH
 *    and non-BH context, so local BH disabling is needed.
 * 4) All operations modify state, so a spinlock is used.
 */
static struct dst_entry 	*dst_garbage_list;
#if RT_CACHE_DEBUG >= 2 
static atomic_t			 dst_total = ATOMIC_INIT(0);
#endif
static spinlock_t		 dst_lock = SPIN_LOCK_UNLOCKED;

static unsigned long dst_gc_timer_expires;
static unsigned long dst_gc_timer_inc = DST_GC_MAX;
static void dst_run_gc(unsigned long);

static struct timer_list dst_gc_timer =
	{ data: DST_GC_MIN, function: dst_run_gc };


static void dst_run_gc(unsigned long dummy)
{
	int    delayed = 0;
	struct dst_entry * dst, **dstp;

	if (!spin_trylock(&dst_lock)) {
		mod_timer(&dst_gc_timer, jiffies + HZ/10);
		return;
	}


	del_timer(&dst_gc_timer);
	dstp = &dst_garbage_list;
	while ((dst = *dstp) != NULL) {
		if (atomic_read(&dst->__refcnt)) {
			dstp = &dst->next;
			delayed++;
			continue;
		}
		*dstp = dst->next;
		dst_destroy(dst);
	}
	if (!dst_garbage_list) {
		dst_gc_timer_inc = DST_GC_MAX;
		goto out;
	}
	if ((dst_gc_timer_expires += dst_gc_timer_inc) > DST_GC_MAX)
		dst_gc_timer_expires = DST_GC_MAX;
	dst_gc_timer_inc += DST_GC_INC;
	dst_gc_timer.expires = jiffies + dst_gc_timer_expires;
#if RT_CACHE_DEBUG >= 2
	printk("dst_total: %d/%d %ld\n",
	       atomic_read(&dst_total), delayed,  dst_gc_timer_expires);
#endif
	add_timer(&dst_gc_timer);

out:
	spin_unlock(&dst_lock);
}

static int dst_discard(struct sk_buff *skb)
{
	kfree_skb(skb);
	return 0;
}

static int dst_blackhole(struct sk_buff *skb)
{
	kfree_skb(skb);
	return 0;
}

void * dst_alloc(struct dst_ops * ops)
{
	struct dst_entry * dst;

	if (ops->gc && atomic_read(&ops->entries) > ops->gc_thresh) {
		if (ops->gc())
			return NULL;
	}
	dst = kmem_cache_alloc(ops->kmem_cachep, SLAB_ATOMIC);
	if (!dst)
		return NULL;
	memset(dst, 0, ops->entry_size);
	atomic_set(&dst->__refcnt, 0);
	dst->ops = ops;
	dst->lastuse = jiffies;
	dst->input = dst_discard;
	dst->output = dst_blackhole;
#if RT_CACHE_DEBUG >= 2 
	atomic_inc(&dst_total);
#endif
	atomic_inc(&ops->entries);
	return dst;
}

void __dst_free(struct dst_entry * dst)
{
	spin_lock_bh(&dst_lock);

	/* The first case (dev==NULL) is required, when
	   protocol module is unloaded.
	 */
	if (dst->dev == NULL || !(dst->dev->flags&IFF_UP)) {
		dst->input = dst_discard;
		dst->output = dst_blackhole;
	}
	dst->obsolete = 2;
	dst->next = dst_garbage_list;
	dst_garbage_list = dst;
	if (dst_gc_timer_inc > DST_GC_INC) {
		dst_gc_timer_inc = DST_GC_INC;
		dst_gc_timer_expires = DST_GC_MIN;
		mod_timer(&dst_gc_timer, jiffies + dst_gc_timer_expires);
	}

	spin_unlock_bh(&dst_lock);
}

void dst_destroy(struct dst_entry * dst)
{
	struct neighbour *neigh = dst->neighbour;
	struct hh_cache *hh = dst->hh;

	dst->hh = NULL;
	if (hh && atomic_dec_and_test(&hh->hh_refcnt))
		kfree(hh);

	if (neigh) {
		dst->neighbour = NULL;
		neigh_release(neigh);
	}

	atomic_dec(&dst->ops->entries);

	if (dst->ops->destroy)
		dst->ops->destroy(dst);
	if (dst->dev)
		dev_put(dst->dev);
#if RT_CACHE_DEBUG >= 2 
	atomic_dec(&dst_total);
#endif
	kmem_cache_free(dst->ops->kmem_cachep, dst);
}

static int dst_dev_event(struct notifier_block *this, unsigned long event, void *ptr)
{
	struct net_device *dev = ptr;
	struct dst_entry *dst;

	switch (event) {
	case NETDEV_UNREGISTER:
	case NETDEV_DOWN:
		spin_lock_bh(&dst_lock);
		for (dst = dst_garbage_list; dst; dst = dst->next) {
			if (dst->dev == dev) {
				/* Dirty hack. We did it in 2.2 (in __dst_free),
				   we have _very_ good reasons not to repeat
				   this mistake in 2.3, but we have no choice
				   now. _It_ _is_ _explicit_ _deliberate_
				   _race_ _condition_.
				 */
				if (event!=NETDEV_DOWN &&
				    !(dev->features & NETIF_F_DYNALLOC) &&
				    dst->output == dst_blackhole) {
					dst->dev = &loopback_dev;
					dev_put(dev);
					dev_hold(&loopback_dev);
					dst->output = dst_discard;
					if (dst->neighbour && dst->neighbour->dev == dev) {
						dst->neighbour->dev = &loopback_dev;
						dev_put(dev);
						dev_hold(&loopback_dev);
					}
				} else {
					dst->input = dst_discard;
					dst->output = dst_blackhole;
				}
			}
		}
		spin_unlock_bh(&dst_lock);
		break;
	}
	return NOTIFY_DONE;
}

struct notifier_block dst_dev_notifier = {
	dst_dev_event,
	NULL,
	0
};

void __init dst_init(void)
{
	register_netdevice_notifier(&dst_dev_notifier);
}
