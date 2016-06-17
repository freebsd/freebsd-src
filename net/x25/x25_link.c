/*
 *	X.25 Packet Layer release 002
 *
 *	This is ALPHA test software. This code may break your machine, randomly fail to work with new 
 *	releases, misbehave and/or generally screw up. It might even work. 
 *
 *	This code REQUIRES 2.1.15 or higher
 *
 *	This module:
 *		This module is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 *	History
 *	X.25 001	Jonathan Naylor	  Started coding.
 *	X.25 002	Jonathan Naylor	  New timer architecture.
 *	mar/20/00	Daniela Squassoni Disabling/enabling of facilities 
 *					  negotiation.
 *	2000-09-04	Henner Eisen	  dev_hold() / dev_put() for x25_neigh.
 */

#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/sockios.h>
#include <linux/net.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <asm/segment.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <net/x25.h>

static struct x25_neigh *x25_neigh_list /* = NULL initially */;

static void x25_t20timer_expiry(unsigned long);

/*
 *	Linux set/reset timer routines
 */
static void x25_start_t20timer(struct x25_neigh *neigh)
{
	del_timer(&neigh->t20timer);

	neigh->t20timer.data     = (unsigned long)neigh;
	neigh->t20timer.function = &x25_t20timer_expiry;
	neigh->t20timer.expires  = jiffies + neigh->t20;

	add_timer(&neigh->t20timer);
}

static void x25_t20timer_expiry(unsigned long param)
{
	struct x25_neigh *neigh = (struct x25_neigh *)param;

	x25_transmit_restart_request(neigh);

	x25_start_t20timer(neigh);
}

static void x25_stop_t20timer(struct x25_neigh *neigh)
{
	del_timer(&neigh->t20timer);
}

static int x25_t20timer_pending(struct x25_neigh *neigh)
{
	return timer_pending(&neigh->t20timer);
}

/*
 *	This handles all restart and diagnostic frames.
 */
void x25_link_control(struct sk_buff *skb, struct x25_neigh *neigh, unsigned short frametype)
{
	struct sk_buff *skbn;
	int confirm;

	switch (frametype) {
		case X25_RESTART_REQUEST:
			confirm = !x25_t20timer_pending(neigh);
			x25_stop_t20timer(neigh);
			neigh->state = X25_LINK_STATE_3;
			if (confirm) x25_transmit_restart_confirmation(neigh);
			break;

		case X25_RESTART_CONFIRMATION:
			x25_stop_t20timer(neigh);
			neigh->state = X25_LINK_STATE_3;
			break;

		case X25_DIAGNOSTIC:
			printk(KERN_WARNING "x25: diagnostic #%d - %02X %02X %02X\n", skb->data[3], skb->data[4], skb->data[5], skb->data[6]);
			break;
			
		default:
			printk(KERN_WARNING "x25: received unknown %02X with LCI 000\n", frametype);
			break;
	}

	if (neigh->state == X25_LINK_STATE_3) {
		while ((skbn = skb_dequeue(&neigh->queue)) != NULL)
			x25_send_frame(skbn, neigh);
	}
}

/*
 *	This routine is called when a Restart Request is needed
 */
void x25_transmit_restart_request(struct x25_neigh *neigh)
{
	struct sk_buff *skb;
	unsigned char *dptr;
	int len;

	len = X25_MAX_L2_LEN + X25_STD_MIN_LEN + 2;

	if ((skb = alloc_skb(len, GFP_ATOMIC)) == NULL)
		return;

	skb_reserve(skb, X25_MAX_L2_LEN);

	dptr = skb_put(skb, X25_STD_MIN_LEN + 2);

	*dptr++ = (neigh->extended) ? X25_GFI_EXTSEQ : X25_GFI_STDSEQ;
	*dptr++ = 0x00;
	*dptr++ = X25_RESTART_REQUEST;
	*dptr++ = 0x00;
	*dptr++ = 0;

	skb->sk = NULL;

	x25_send_frame(skb, neigh);
}

/*
 * This routine is called when a Restart Confirmation is needed
 */
void x25_transmit_restart_confirmation(struct x25_neigh *neigh)
{
	struct sk_buff *skb;
	unsigned char *dptr;
	int len;

	len = X25_MAX_L2_LEN + X25_STD_MIN_LEN;

	if ((skb = alloc_skb(len, GFP_ATOMIC)) == NULL)
		return;

	skb_reserve(skb, X25_MAX_L2_LEN);

	dptr = skb_put(skb, X25_STD_MIN_LEN);

	*dptr++ = (neigh->extended) ? X25_GFI_EXTSEQ : X25_GFI_STDSEQ;
	*dptr++ = 0x00;
	*dptr++ = X25_RESTART_CONFIRMATION;

	skb->sk = NULL;

	x25_send_frame(skb, neigh);
}

/*
 * This routine is called when a Diagnostic is required.
 */
void x25_transmit_diagnostic(struct x25_neigh *neigh, unsigned char diag)
{
	struct sk_buff *skb;
	unsigned char *dptr;
	int len;

	len = X25_MAX_L2_LEN + X25_STD_MIN_LEN + 1;

	if ((skb = alloc_skb(len, GFP_ATOMIC)) == NULL)
		return;

	skb_reserve(skb, X25_MAX_L2_LEN);

	dptr = skb_put(skb, X25_STD_MIN_LEN + 1);

	*dptr++ = (neigh->extended) ? X25_GFI_EXTSEQ : X25_GFI_STDSEQ;
	*dptr++ = 0x00;
	*dptr++ = X25_DIAGNOSTIC;
	*dptr++ = diag;

	skb->sk = NULL;

	x25_send_frame(skb, neigh);
}

/*
 *	This routine is called when a Clear Request is needed outside of the context
 *	of a connected socket.
 */
void x25_transmit_clear_request(struct x25_neigh *neigh, unsigned int lci, unsigned char cause)
{
	struct sk_buff *skb;
	unsigned char *dptr;
	int len;

	len = X25_MAX_L2_LEN + X25_STD_MIN_LEN + 2;

	if ((skb = alloc_skb(len, GFP_ATOMIC)) == NULL)
		return;

	skb_reserve(skb, X25_MAX_L2_LEN);

	dptr = skb_put(skb, X25_STD_MIN_LEN + 2);

	*dptr++ = ((lci >> 8) & 0x0F) | (neigh->extended ? X25_GFI_EXTSEQ : X25_GFI_STDSEQ);
	*dptr++ = ((lci >> 0) & 0xFF);
	*dptr++ = X25_CLEAR_REQUEST;
	*dptr++ = cause;
	*dptr++ = 0x00;

	skb->sk = NULL;

	x25_send_frame(skb, neigh);
}

void x25_transmit_link(struct sk_buff *skb, struct x25_neigh *neigh)
{
	switch (neigh->state) {
		case X25_LINK_STATE_0:
			skb_queue_tail(&neigh->queue, skb);
			neigh->state = X25_LINK_STATE_1;
			x25_establish_link(neigh);
			break;
		case X25_LINK_STATE_1:
		case X25_LINK_STATE_2:
			skb_queue_tail(&neigh->queue, skb);
			break;
		case X25_LINK_STATE_3:
			x25_send_frame(skb, neigh);
			break;
	}
}

/*
 *	Called when the link layer has become established.
 */
void x25_link_established(struct x25_neigh *neigh)
{
	switch (neigh->state) {
		case X25_LINK_STATE_0:
			neigh->state = X25_LINK_STATE_2;
			break;
		case X25_LINK_STATE_1:
			x25_transmit_restart_request(neigh);
			neigh->state = X25_LINK_STATE_2;
			x25_start_t20timer(neigh);
			break;
	}
}

/*
 *	Called when the link layer has terminated, or an establishment
 *	request has failed.
 */

void x25_link_terminated(struct x25_neigh *neigh)
{
	neigh->state = X25_LINK_STATE_0;
	/* Out of order: clear existing virtual calls (X.25 03/93 4.6.3) */
	x25_kill_by_neigh(neigh);
}

/*
 *	Add a new device.
 */
void x25_link_device_up(struct net_device *dev)
{
	struct x25_neigh *x25_neigh;
	unsigned long flags;

	if ((x25_neigh = kmalloc(sizeof(*x25_neigh), GFP_ATOMIC)) == NULL)
		return;

	skb_queue_head_init(&x25_neigh->queue);

	init_timer(&x25_neigh->t20timer);

	dev_hold(dev);
	x25_neigh->dev      = dev;
	x25_neigh->state    = X25_LINK_STATE_0;
	x25_neigh->extended = 0;
	x25_neigh->global_facil_mask = (X25_MASK_REVERSE | X25_MASK_THROUGHPUT | X25_MASK_PACKET_SIZE | X25_MASK_WINDOW_SIZE); /* enables negotiation */
	x25_neigh->t20      = sysctl_x25_restart_request_timeout;

	save_flags(flags); cli();
	x25_neigh->next = x25_neigh_list;
	x25_neigh_list  = x25_neigh;
	restore_flags(flags);
}

static void x25_remove_neigh(struct x25_neigh *x25_neigh)
{
	struct x25_neigh *s;
	unsigned long flags;

	skb_queue_purge(&x25_neigh->queue);

	x25_stop_t20timer(x25_neigh);

	save_flags(flags); cli();

	if ((s = x25_neigh_list) == x25_neigh) {
		x25_neigh_list = x25_neigh->next;
		restore_flags(flags);
		kfree(x25_neigh);
		return;
	}

	while (s != NULL && s->next != NULL) {
		if (s->next == x25_neigh) {
			s->next = x25_neigh->next;
			restore_flags(flags);
			kfree(x25_neigh);
			return;
		}

		s = s->next;
	}

	restore_flags(flags);
}

/*
 *	A device has been removed, remove its links.
 */
void x25_link_device_down(struct net_device *dev)
{
	struct x25_neigh *neigh, *x25_neigh = x25_neigh_list;

	while (x25_neigh != NULL) {
		neigh     = x25_neigh;
		x25_neigh = x25_neigh->next;

		if (neigh->dev == dev){
			x25_remove_neigh(neigh);
			dev_put(dev);
		}
	}
}

/*
 *	Given a device, return the neighbour address.
 */
struct x25_neigh *x25_get_neigh(struct net_device *dev)
{
	struct x25_neigh *x25_neigh;

	for (x25_neigh = x25_neigh_list; x25_neigh != NULL; x25_neigh = x25_neigh->next)
		if (x25_neigh->dev == dev)
			return x25_neigh;

	return NULL;
}

/*
 *	Handle the ioctls that control the subscription functions.
 */
int x25_subscr_ioctl(unsigned int cmd, void *arg)
{
	struct x25_subscrip_struct x25_subscr;
	struct x25_neigh *x25_neigh;
	struct net_device *dev;

	switch (cmd) {

		case SIOCX25GSUBSCRIP:
			if (copy_from_user(&x25_subscr, arg, sizeof(struct x25_subscrip_struct)))
				return -EFAULT;
			if ((dev = x25_dev_get(x25_subscr.device)) == NULL)
				return -EINVAL;
			if ((x25_neigh = x25_get_neigh(dev)) == NULL) {
				dev_put(dev);
				return -EINVAL;
			}
			dev_put(dev);
			x25_subscr.extended = x25_neigh->extended;
			x25_subscr.global_facil_mask = x25_neigh->global_facil_mask;
			if (copy_to_user(arg, &x25_subscr, sizeof(struct x25_subscrip_struct)))
				return -EFAULT;
			break;

		case SIOCX25SSUBSCRIP:
			if (copy_from_user(&x25_subscr, arg, sizeof(struct x25_subscrip_struct)))
				return -EFAULT;
			if ((dev = x25_dev_get(x25_subscr.device)) == NULL)
				return -EINVAL;
			if ((x25_neigh = x25_get_neigh(dev)) == NULL) {
				dev_put(dev);
				return -EINVAL;
			}
			dev_put(dev);
			if (x25_subscr.extended != 0 && x25_subscr.extended != 1)
				return -EINVAL;
			x25_neigh->extended = x25_subscr.extended;
			x25_neigh->global_facil_mask = x25_subscr.global_facil_mask;
			break;

		default:
			return -EINVAL;
	}

	return 0;
}


/*
 *	Release all memory associated with X.25 neighbour structures.
 */
void __exit x25_link_free(void)
{
	struct x25_neigh *neigh, *x25_neigh = x25_neigh_list;

	while (x25_neigh != NULL) {
		neigh     = x25_neigh;
		x25_neigh = x25_neigh->next;

		x25_remove_neigh(neigh);
	}
}
