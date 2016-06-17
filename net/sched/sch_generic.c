/*
 * net/sched/sch_generic.c	Generic packet scheduler routines.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *              Jamal Hadi Salim, <hadi@cyberus.ca> 990601
 *              - Ingress support
 */

#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/bitops.h>
#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/in.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/rtnetlink.h>
#include <linux/init.h>
#include <net/sock.h>
#include <net/pkt_sched.h>

/* Main transmission queue. */

/* Main qdisc structure lock. 

   However, modifications
   to data, participating in scheduling must be additionally
   protected with dev->queue_lock spinlock.

   The idea is the following:
   - enqueue, dequeue are serialized via top level device
     spinlock dev->queue_lock.
   - tree walking is protected by read_lock(qdisc_tree_lock)
     and this lock is used only in process context.
   - updates to tree are made only under rtnl semaphore,
     hence this lock may be made without local bh disabling.

   qdisc_tree_lock must be grabbed BEFORE dev->queue_lock!
 */
rwlock_t qdisc_tree_lock = RW_LOCK_UNLOCKED;

/* 
   dev->queue_lock serializes queue accesses for this device
   AND dev->qdisc pointer itself.

   dev->xmit_lock serializes accesses to device driver.

   dev->queue_lock and dev->xmit_lock are mutually exclusive,
   if one is grabbed, another must be free.
 */


/* Kick device.
   Note, that this procedure can be called by a watchdog timer, so that
   we do not check dev->tbusy flag here.

   Returns:  0  - queue is empty.
            >0  - queue is not empty, but throttled.
	    <0  - queue is not empty. Device is throttled, if dev->tbusy != 0.

   NOTE: Called under dev->queue_lock with locally disabled BH.
*/

int qdisc_restart(struct net_device *dev)
{
	struct Qdisc *q = dev->qdisc;
	struct sk_buff *skb;

	/* Dequeue packet */
	if ((skb = q->dequeue(q)) != NULL) {
		if (spin_trylock(&dev->xmit_lock)) {
			/* Remember that the driver is grabbed by us. */
			dev->xmit_lock_owner = smp_processor_id();

			/* And release queue */
			spin_unlock(&dev->queue_lock);

			if (!netif_queue_stopped(dev)) {
				if (netdev_nit)
					dev_queue_xmit_nit(skb, dev);

				if (dev->hard_start_xmit(skb, dev) == 0) {
					dev->xmit_lock_owner = -1;
					spin_unlock(&dev->xmit_lock);

					spin_lock(&dev->queue_lock);
					return -1;
				}
			}

			/* Release the driver */
			dev->xmit_lock_owner = -1;
			spin_unlock(&dev->xmit_lock);
			spin_lock(&dev->queue_lock);
			q = dev->qdisc;
		} else {
			/* So, someone grabbed the driver. */

			/* It may be transient configuration error,
			   when hard_start_xmit() recurses. We detect
			   it by checking xmit owner and drop the
			   packet when deadloop is detected.
			 */
			if (dev->xmit_lock_owner == smp_processor_id()) {
				kfree_skb(skb);
				if (net_ratelimit())
					printk(KERN_DEBUG "Dead loop on netdevice %s, fix it urgently!\n", dev->name);
				return -1;
			}
			netdev_rx_stat[smp_processor_id()].cpu_collision++;
		}

		/* Device kicked us out :(
		   This is possible in three cases:

		   0. driver is locked
		   1. fastroute is enabled
		   2. device cannot determine busy state
		      before start of transmission (f.e. dialout)
		   3. device is buggy (ppp)
		 */

		q->ops->requeue(skb, q);
		netif_schedule(dev);
		return 1;
	}
	return q->q.qlen;
}

static void dev_watchdog(unsigned long arg)
{
	struct net_device *dev = (struct net_device *)arg;

	spin_lock(&dev->xmit_lock);
	if (dev->qdisc != &noop_qdisc) {
		if (netif_device_present(dev) &&
		    netif_running(dev) &&
		    netif_carrier_ok(dev)) {
			if (netif_queue_stopped(dev) &&
			    (jiffies - dev->trans_start) > dev->watchdog_timeo) {
				printk(KERN_INFO "NETDEV WATCHDOG: %s: transmit timed out\n", dev->name);
				dev->tx_timeout(dev);
			}
			if (!mod_timer(&dev->watchdog_timer, jiffies + dev->watchdog_timeo))
				dev_hold(dev);
		}
	}
	spin_unlock(&dev->xmit_lock);

	dev_put(dev);
}

static void dev_watchdog_init(struct net_device *dev)
{
	init_timer(&dev->watchdog_timer);
	dev->watchdog_timer.data = (unsigned long)dev;
	dev->watchdog_timer.function = dev_watchdog;
}

void __netdev_watchdog_up(struct net_device *dev)
{
	if (dev->tx_timeout) {
		if (dev->watchdog_timeo <= 0)
			dev->watchdog_timeo = 5*HZ;
		if (!mod_timer(&dev->watchdog_timer, jiffies + dev->watchdog_timeo))
			dev_hold(dev);
	}
}

static void dev_watchdog_up(struct net_device *dev)
{
	spin_lock_bh(&dev->xmit_lock);
	__netdev_watchdog_up(dev);
	spin_unlock_bh(&dev->xmit_lock);
}

static void dev_watchdog_down(struct net_device *dev)
{
	spin_lock_bh(&dev->xmit_lock);
	if (del_timer(&dev->watchdog_timer))
		__dev_put(dev);
	spin_unlock_bh(&dev->xmit_lock);
}

/* "NOOP" scheduler: the best scheduler, recommended for all interfaces
   under all circumstances. It is difficult to invent anything faster or
   cheaper.
 */

static int
noop_enqueue(struct sk_buff *skb, struct Qdisc * qdisc)
{
	kfree_skb(skb);
	return NET_XMIT_CN;
}

static struct sk_buff *
noop_dequeue(struct Qdisc * qdisc)
{
	return NULL;
}

static int
noop_requeue(struct sk_buff *skb, struct Qdisc* qdisc)
{
	if (net_ratelimit())
		printk(KERN_DEBUG "%s deferred output. It is buggy.\n", skb->dev->name);
	kfree_skb(skb);
	return NET_XMIT_CN;
}

struct Qdisc_ops noop_qdisc_ops =
{
	NULL,
	NULL,
	"noop",
	0,

	noop_enqueue,
	noop_dequeue,
	noop_requeue,
};

struct Qdisc noop_qdisc =
{
	noop_enqueue,
	noop_dequeue,
	TCQ_F_BUILTIN,
	&noop_qdisc_ops,	
};


struct Qdisc_ops noqueue_qdisc_ops =
{
	NULL,
	NULL,
	"noqueue",
	0,

	noop_enqueue,
	noop_dequeue,
	noop_requeue,

};

struct Qdisc noqueue_qdisc =
{
	NULL,
	noop_dequeue,
	TCQ_F_BUILTIN,
	&noqueue_qdisc_ops,
};


static const u8 prio2band[TC_PRIO_MAX+1] =
{ 1, 2, 2, 2, 1, 2, 0, 0 , 1, 1, 1, 1, 1, 1, 1, 1 };

/* 3-band FIFO queue: old style, but should be a bit faster than
   generic prio+fifo combination.
 */

static int
pfifo_fast_enqueue(struct sk_buff *skb, struct Qdisc* qdisc)
{
	struct sk_buff_head *list;

	list = ((struct sk_buff_head*)qdisc->data) +
		prio2band[skb->priority&TC_PRIO_MAX];

	if (list->qlen < qdisc->dev->tx_queue_len) {
		__skb_queue_tail(list, skb);
		qdisc->q.qlen++;
		qdisc->stats.bytes += skb->len;
		qdisc->stats.packets++;
		return 0;
	}
	qdisc->stats.drops++;
	kfree_skb(skb);
	return NET_XMIT_DROP;
}

static struct sk_buff *
pfifo_fast_dequeue(struct Qdisc* qdisc)
{
	int prio;
	struct sk_buff_head *list = ((struct sk_buff_head*)qdisc->data);
	struct sk_buff *skb;

	for (prio = 0; prio < 3; prio++, list++) {
		skb = __skb_dequeue(list);
		if (skb) {
			qdisc->q.qlen--;
			return skb;
		}
	}
	return NULL;
}

static int
pfifo_fast_requeue(struct sk_buff *skb, struct Qdisc* qdisc)
{
	struct sk_buff_head *list;

	list = ((struct sk_buff_head*)qdisc->data) +
		prio2band[skb->priority&TC_PRIO_MAX];

	__skb_queue_head(list, skb);
	qdisc->q.qlen++;
	return 0;
}

static void
pfifo_fast_reset(struct Qdisc* qdisc)
{
	int prio;
	struct sk_buff_head *list = ((struct sk_buff_head*)qdisc->data);

	for (prio=0; prio < 3; prio++)
		skb_queue_purge(list+prio);
	qdisc->q.qlen = 0;
}

static int pfifo_fast_dump(struct Qdisc *qdisc, struct sk_buff *skb)
{
	unsigned char	 *b = skb->tail;
	struct tc_prio_qopt opt;

	opt.bands = 3; 
	memcpy(&opt.priomap, prio2band, TC_PRIO_MAX+1);
	RTA_PUT(skb, TCA_OPTIONS, sizeof(opt), &opt);
	return skb->len;

rtattr_failure:
	skb_trim(skb, b - skb->data);
	return -1;
}

static int pfifo_fast_init(struct Qdisc *qdisc, struct rtattr *opt)
{
	int i;
	struct sk_buff_head *list;

	list = ((struct sk_buff_head*)qdisc->data);

	for (i=0; i<3; i++)
		skb_queue_head_init(list+i);

	return 0;
}

static struct Qdisc_ops pfifo_fast_ops =
{
	NULL,
	NULL,
	"pfifo_fast",
	3 * sizeof(struct sk_buff_head),

	pfifo_fast_enqueue,
	pfifo_fast_dequeue,
	pfifo_fast_requeue,
	NULL,

	pfifo_fast_init,
	pfifo_fast_reset,
	NULL,
	NULL,
	pfifo_fast_dump,

};

struct Qdisc * qdisc_create_dflt(struct net_device *dev, struct Qdisc_ops *ops)
{
	struct Qdisc *sch;
	int size = sizeof(*sch) + ops->priv_size;

	sch = kmalloc(size, GFP_KERNEL);
	if (!sch)
		return NULL;
	memset(sch, 0, size);

	skb_queue_head_init(&sch->q);
	sch->ops = ops;
	sch->enqueue = ops->enqueue;
	sch->dequeue = ops->dequeue;
	sch->dev = dev;
	sch->stats.lock = &dev->queue_lock;
	atomic_set(&sch->refcnt, 1);
	if (!ops->init || ops->init(sch, NULL) == 0)
		return sch;

	kfree(sch);
	return NULL;
}

/* Under dev->queue_lock and BH! */

void qdisc_reset(struct Qdisc *qdisc)
{
	struct Qdisc_ops *ops = qdisc->ops;

	if (ops->reset)
		ops->reset(qdisc);
}

/* Under dev->queue_lock and BH! */

void qdisc_destroy(struct Qdisc *qdisc)
{
	struct Qdisc_ops *ops = qdisc->ops;
	struct net_device *dev;

	if (!atomic_dec_and_test(&qdisc->refcnt))
		return;

	dev = qdisc->dev;

	if (dev) {
		struct Qdisc *q, **qp;
		for (qp = &qdisc->dev->qdisc_list; (q=*qp) != NULL; qp = &q->next) {
			if (q == qdisc) {
				*qp = q->next;
				break;
			}
		}
	}
#ifdef CONFIG_NET_ESTIMATOR
	qdisc_kill_estimator(&qdisc->stats);
#endif
	if (ops->reset)
		ops->reset(qdisc);
	if (ops->destroy)
		ops->destroy(qdisc);
	if (!(qdisc->flags&TCQ_F_BUILTIN))
		kfree(qdisc);
}


void dev_activate(struct net_device *dev)
{
	/* No queueing discipline is attached to device;
	   create default one i.e. pfifo_fast for devices,
	   which need queueing and noqueue_qdisc for
	   virtual interfaces
	 */

	if (dev->qdisc_sleeping == &noop_qdisc) {
		struct Qdisc *qdisc;
		if (dev->tx_queue_len) {
			qdisc = qdisc_create_dflt(dev, &pfifo_fast_ops);
			if (qdisc == NULL) {
				printk(KERN_INFO "%s: activation failed\n", dev->name);
				return;
			}

			write_lock(&qdisc_tree_lock);
			qdisc->next = dev->qdisc_list;
			dev->qdisc_list = qdisc;
			write_unlock(&qdisc_tree_lock);

		} else {
			qdisc =  &noqueue_qdisc;
		}
		write_lock(&qdisc_tree_lock);
		dev->qdisc_sleeping = qdisc;
		write_unlock(&qdisc_tree_lock);
	}

	spin_lock_bh(&dev->queue_lock);
	if ((dev->qdisc = dev->qdisc_sleeping) != &noqueue_qdisc) {
		dev->trans_start = jiffies;
		dev_watchdog_up(dev);
	}
	spin_unlock_bh(&dev->queue_lock);
}

void dev_deactivate(struct net_device *dev)
{
	struct Qdisc *qdisc;

	spin_lock_bh(&dev->queue_lock);
	qdisc = dev->qdisc;
	dev->qdisc = &noop_qdisc;

	qdisc_reset(qdisc);

	spin_unlock_bh(&dev->queue_lock);

	dev_watchdog_down(dev);

	while (test_bit(__LINK_STATE_SCHED, &dev->state))
		yield();

	spin_unlock_wait(&dev->xmit_lock);
}

void dev_init_scheduler(struct net_device *dev)
{
	write_lock(&qdisc_tree_lock);
	spin_lock_bh(&dev->queue_lock);
	dev->qdisc = &noop_qdisc;
	spin_unlock_bh(&dev->queue_lock);
	dev->qdisc_sleeping = &noop_qdisc;
	dev->qdisc_list = NULL;
	write_unlock(&qdisc_tree_lock);

	dev_watchdog_init(dev);
}

void dev_shutdown(struct net_device *dev)
{
	struct Qdisc *qdisc;

	write_lock(&qdisc_tree_lock);
	spin_lock_bh(&dev->queue_lock);
	qdisc = dev->qdisc_sleeping;
	dev->qdisc = &noop_qdisc;
	dev->qdisc_sleeping = &noop_qdisc;
	qdisc_destroy(qdisc);
#if defined(CONFIG_NET_SCH_INGRESS) || defined(CONFIG_NET_SCH_INGRESS_MODULE)
        if ((qdisc = dev->qdisc_ingress) != NULL) {
		dev->qdisc_ingress = NULL;
		qdisc_destroy(qdisc);
        }
#endif
	BUG_TRAP(dev->qdisc_list == NULL);
	BUG_TRAP(!timer_pending(&dev->watchdog_timer));
	dev->qdisc_list = NULL;
	spin_unlock_bh(&dev->queue_lock);
	write_unlock(&qdisc_tree_lock);
}
