/*
 *	NET/ROM release 007
 *
 *	This code REQUIRES 2.1.15 or higher/ NET3.038
 *
 *	This module:
 *		This module is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 *	History
 *	NET/ROM 007	Tomi(OH2BNS)	Created this file.
 *                                      Small change in nr_loopback_queue().
 *
 */

#include <linux/types.h>
#include <linux/socket.h>
#include <linux/timer.h>
#include <net/ax25.h>
#include <linux/skbuff.h>
#include <net/netrom.h>
#include <linux/init.h>

static struct sk_buff_head loopback_queue;
static struct timer_list loopback_timer;

static void nr_set_loopback_timer(void);

void nr_loopback_init(void)
{
	skb_queue_head_init(&loopback_queue);

	init_timer(&loopback_timer);
}

static int nr_loopback_running(void)
{
	return timer_pending(&loopback_timer);
}

int nr_loopback_queue(struct sk_buff *skb)
{
	struct sk_buff *skbn;

	if ((skbn = alloc_skb(skb->len, GFP_ATOMIC)) != NULL) {
		memcpy(skb_put(skbn, skb->len), skb->data, skb->len);
		skbn->h.raw = skbn->data;

		skb_queue_tail(&loopback_queue, skbn);

		if (!nr_loopback_running())
			nr_set_loopback_timer();
	}

	kfree_skb(skb);
	return 1;
}

static void nr_loopback_timer(unsigned long);

static void nr_set_loopback_timer(void)
{
	del_timer(&loopback_timer);

	loopback_timer.data     = 0;
	loopback_timer.function = &nr_loopback_timer;
	loopback_timer.expires  = jiffies + 10;

	add_timer(&loopback_timer);
}

static void nr_loopback_timer(unsigned long param)
{
	struct sk_buff *skb;
	ax25_address *nr_dest;
	struct net_device *dev;

	if ((skb = skb_dequeue(&loopback_queue)) != NULL) {
		nr_dest = (ax25_address *)(skb->data + 7);

		dev = nr_dev_get(nr_dest);

		if (dev == NULL || nr_rx_frame(skb, dev) == 0)
			kfree_skb(skb);

		if (dev != NULL)
			dev_put(dev);

		if (!skb_queue_empty(&loopback_queue) && !nr_loopback_running())
			nr_set_loopback_timer();
	}
}

void __exit nr_loopback_clear(void)
{
	del_timer(&loopback_timer);
	skb_queue_purge(&loopback_queue);
}
