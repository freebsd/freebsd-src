/*
 * net/sched/sch_delay.c	Simple constant delay
 *
 * 		This program is free software; you can redistribute it and/or
 * 		modify it under the terms of the GNU General Public License
 * 		as published by the Free Software Foundation; either version
 * 		2 of the License, or (at your option) any later version.
 *
 * Authors:	Stephen Hemminger <shemminger@osdl.org>
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>

#include <linux/string.h>
#include <linux/mm.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/in.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/if_ether.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/notifier.h>
#include <net/ip.h>
#include <net/route.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <net/pkt_sched.h>

/*	Network delay simulator
	This scheduler adds a fixed delay to all packets.
	Similar to NISTnet and BSD Dummynet.

	It uses byte fifo underneath similar to TBF */
struct dly_sched_data {
	u32	latency;
	u32	limit;
	struct timer_list timer;
	struct Qdisc *qdisc;
};

/* Time stamp put into socket buffer control block */
struct dly_skb_cb {
	psched_time_t	queuetime;
};

/* Enqueue packets with underlying discipline (fifo)
 * but mark them with current time first.
 */
static int dly_enqueue(struct sk_buff *skb, struct Qdisc *sch)
{
	struct dly_sched_data *q = (struct dly_sched_data *)sch->data;
	struct dly_skb_cb *cb = (struct dly_skb_cb *)skb->cb;
	int ret;

	PSCHED_GET_TIME(cb->queuetime);

	/* Queue to underlying scheduler */
	ret = q->qdisc->enqueue(skb, q->qdisc);
	if (ret)
		sch->stats.drops++;
	else {
		sch->q.qlen++;
		sch->stats.bytes += skb->len;
		sch->stats.packets++;
	}
	return 0;
}

/* Requeue packets but don't change time stamp */
static int dly_requeue(struct sk_buff *skb, struct Qdisc *sch)
{
	struct dly_sched_data *q = (struct dly_sched_data *)sch->data;
	int ret;

	ret = q->qdisc->ops->requeue(skb, q->qdisc);
	if (ret == 0)
		sch->q.qlen++;
	return ret;
}

static unsigned int dly_drop(struct Qdisc *sch)
{
	struct dly_sched_data *q = (struct dly_sched_data *)sch->data;
	unsigned int len;

	len = q->qdisc->ops->drop(q->qdisc);
	if (len) {
		sch->q.qlen--;
		sch->stats.drops++;
	}
	return len;
}

/* Dequeue packet.
 * If packet needs to be held up, then stop the
 * queue and set timer to wakeup later.
 */
static struct sk_buff *dly_dequeue(struct Qdisc *sch)
{
	struct dly_sched_data *q = (struct dly_sched_data *)sch->data;
	struct sk_buff *skb = q->qdisc->dequeue(q->qdisc);

	if (skb) {
		struct dly_skb_cb *cb = (struct dly_skb_cb *)skb->cb;
		psched_time_t now;
		long diff;

		PSCHED_GET_TIME(now);
		diff = q->latency - PSCHED_TDIFF(now, cb->queuetime);

		if (diff <= 0) {
			sch->q.qlen--;
			sch->flags &= ~TCQ_F_THROTTLED;
			return skb;
		}

		if (!netif_queue_stopped(sch->dev)) {
			long delay = PSCHED_US2JIFFIE(diff);
			if (delay <= 0)
				delay = 1;
			mod_timer(&q->timer, jiffies+delay);
		}

		if (q->qdisc->ops->requeue(skb, q->qdisc) != NET_XMIT_SUCCESS) {
			sch->q.qlen--;
			sch->stats.drops++;
		}
		sch->flags |= TCQ_F_THROTTLED;
	}
	return NULL;
}

static void dly_reset(struct Qdisc *sch)
{
	struct dly_sched_data *q = (struct dly_sched_data *)sch->data;

	qdisc_reset(q->qdisc);
	sch->q.qlen = 0;
	sch->flags &= ~TCQ_F_THROTTLED;
	del_timer(&q->timer);
}

static void dly_timer(unsigned long arg)
{
	struct Qdisc *sch = (struct Qdisc *)arg;

	sch->flags &= ~TCQ_F_THROTTLED;
	netif_schedule(sch->dev);
}

/* Tell Fifo the new limit. */
static int change_limit(struct Qdisc *q, u32 limit)
{
	struct rtattr *rta;
	int ret;

	rta = kmalloc(RTA_LENGTH(sizeof(struct tc_fifo_qopt)), GFP_KERNEL);
	if (!rta)
		return -ENOMEM;

	rta->rta_type = RTM_NEWQDISC;
	((struct tc_fifo_qopt *)RTA_DATA(rta))->limit = limit;
	ret = q->ops->change(q, rta);
	kfree(rta);

	return ret;
}

/* Setup underlying FIFO discipline */
static int dly_change(struct Qdisc *sch, struct rtattr *opt)
{
	struct dly_sched_data *q = (struct dly_sched_data *)sch->data;
	struct tc_dly_qopt *qopt = RTA_DATA(opt);
	int err;

	if (q->qdisc == &noop_qdisc) {
		struct Qdisc *child
			= qdisc_create_dflt(sch->dev, &bfifo_qdisc_ops);
		if (!child)
			return -EINVAL;
		q->qdisc = child;
	}

	err = change_limit(q->qdisc, qopt->limit);
	if (err) {
		qdisc_destroy(q->qdisc);
		q->qdisc = &noop_qdisc;
	} else {
		q->latency = qopt->latency;
		q->limit = qopt->limit;
	}
	return err;
}

static int dly_init(struct Qdisc *sch, struct rtattr *opt)
{
	struct dly_sched_data *q = (struct dly_sched_data *)sch->data;
	int err;

	if (!opt)
		return -EINVAL;

	MOD_INC_USE_COUNT;

	init_timer(&q->timer);
	q->timer.function = dly_timer;
	q->timer.data = (unsigned long) sch;
	q->qdisc = &noop_qdisc;

	err = dly_change(sch, opt);
	if (err)
		MOD_DEC_USE_COUNT;

	return err;
}

static void dly_destroy(struct Qdisc *sch)
{
	struct dly_sched_data *q = (struct dly_sched_data *)sch->data;

	del_timer(&q->timer);
	qdisc_destroy(q->qdisc);
	q->qdisc = &noop_qdisc;

	MOD_DEC_USE_COUNT;
}

static int dly_dump(struct Qdisc *sch, struct sk_buff *skb)
{
	struct dly_sched_data *q = (struct dly_sched_data *)sch->data;
	unsigned char	 *b = skb->tail;
	struct tc_dly_qopt qopt;

	qopt.latency = q->latency;
	qopt.limit = q->limit;

	RTA_PUT(skb, TCA_OPTIONS, sizeof(qopt), &qopt);

	return skb->len;

rtattr_failure:
	skb_trim(skb, b - skb->data);
	return -1;
}

struct Qdisc_ops dly_qdisc_ops = {
	.id		=	"delay",
	.priv_size	=	sizeof(struct dly_sched_data),
	.enqueue	=	dly_enqueue,
	.dequeue	=	dly_dequeue,
	.requeue	=	dly_requeue,
	.drop		=	dly_drop,
	.init		=	dly_init,
	.reset		=	dly_reset,
	.destroy	=	dly_destroy,
	.change		=	dly_change,
	.dump		=	dly_dump,
};

#ifdef MODULE
int init_module(void)
{
	return register_qdisc(&dly_qdisc_ops);
}

void cleanup_module(void)
{
	unregister_qdisc(&dly_qdisc_ops);
}
#endif
MODULE_LICENSE("GPL");
