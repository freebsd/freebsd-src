/*
 *	LAPB release 002
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
 *	LAPB 001	Jonathan Naylor	Started Coding
 *	LAPB 002	Jonathan Naylor	New timer architecture.
 *	2000-10-29	Henner Eisen	lapb_data_indication() return status.
 */
 
#include <linux/module.h>
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
#include <linux/if_arp.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <net/lapb.h>

static lapb_cb *volatile lapb_list /* = NULL initially */;

/*
 *	Free an allocated lapb control block. This is done to centralise
 *	the MOD count code.
 */
static void lapb_free_cb(lapb_cb *lapb)
{
	kfree(lapb);

	MOD_DEC_USE_COUNT;
}

/*
 *	Socket removal during an interrupt is now safe.
 */
static void lapb_remove_cb(lapb_cb *lapb)
{
	lapb_cb *s;
	unsigned long flags;

	save_flags(flags); cli();

	if ((s = lapb_list) == lapb) {
		lapb_list = s->next;
		restore_flags(flags);
		return;
	}

	while (s != NULL && s->next != NULL) {
		if (s->next == lapb) {
			s->next = lapb->next;
			restore_flags(flags);
			return;
		}

		s = s->next;
	}

	restore_flags(flags);
}

/*
 *	Add a socket to the bound sockets list.
 */
static void lapb_insert_cb(lapb_cb *lapb)
{
	unsigned long flags;

	save_flags(flags); cli();

	lapb->next = lapb_list;
	lapb_list  = lapb;

	restore_flags(flags);
}

/*
 *	Convert the integer token used by the device driver into a pointer
 *	to a LAPB control structure.
 */
static lapb_cb *lapb_tokentostruct(void *token)
{
	lapb_cb *lapb;

	for (lapb = lapb_list; lapb != NULL; lapb = lapb->next)
		if (lapb->token == token)
			return lapb;

	return NULL;
}

/*
 *	Create an empty LAPB control block.
 */
static lapb_cb *lapb_create_cb(void)
{
	lapb_cb *lapb;

	if ((lapb = kmalloc(sizeof(*lapb), GFP_ATOMIC)) == NULL)
		return NULL;

	MOD_INC_USE_COUNT;

	memset(lapb, 0x00, sizeof(*lapb));

	skb_queue_head_init(&lapb->write_queue);
	skb_queue_head_init(&lapb->ack_queue);

	init_timer(&lapb->t1timer);
	init_timer(&lapb->t2timer);

	lapb->t1      = LAPB_DEFAULT_T1;
	lapb->t2      = LAPB_DEFAULT_T2;
	lapb->n2      = LAPB_DEFAULT_N2;
	lapb->mode    = LAPB_DEFAULT_MODE;
	lapb->window  = LAPB_DEFAULT_WINDOW;
	lapb->state   = LAPB_STATE_0;

	return lapb;
}

int lapb_register(void *token, struct lapb_register_struct *callbacks)
{
	lapb_cb *lapb;

	if (lapb_tokentostruct(token) != NULL)
		return LAPB_BADTOKEN;

	if ((lapb = lapb_create_cb()) == NULL)
		return LAPB_NOMEM;

	lapb->token     = token;
	lapb->callbacks = *callbacks;

	lapb_insert_cb(lapb);

	lapb_start_t1timer(lapb);

	return LAPB_OK;
}

int lapb_unregister(void *token)
{
	lapb_cb *lapb;

	if ((lapb = lapb_tokentostruct(token)) == NULL)
		return LAPB_BADTOKEN;

	lapb_stop_t1timer(lapb);
	lapb_stop_t2timer(lapb);

	lapb_clear_queues(lapb);

	lapb_remove_cb(lapb);

	lapb_free_cb(lapb);

	return LAPB_OK;
}

int lapb_getparms(void *token, struct lapb_parms_struct *parms)
{
	lapb_cb *lapb;

	if ((lapb = lapb_tokentostruct(token)) == NULL)
		return LAPB_BADTOKEN;

	parms->t1      = lapb->t1 / HZ;
	parms->t2      = lapb->t2 / HZ;
	parms->n2      = lapb->n2;
	parms->n2count = lapb->n2count;
	parms->state   = lapb->state;
	parms->window  = lapb->window;
	parms->mode    = lapb->mode;

	if (!timer_pending(&lapb->t1timer))
		parms->t1timer = 0;
	else
		parms->t1timer = (lapb->t1timer.expires - jiffies) / HZ;

	if (!timer_pending(&lapb->t2timer))
		parms->t2timer = 0;
	else
		parms->t2timer = (lapb->t2timer.expires - jiffies) / HZ;

	return LAPB_OK;
}

int lapb_setparms(void *token, struct lapb_parms_struct *parms)
{
	lapb_cb *lapb;

	if ((lapb = lapb_tokentostruct(token)) == NULL)
		return LAPB_BADTOKEN;

	if (parms->t1 < 1)
		return LAPB_INVALUE;

	if (parms->t2 < 1)
		return LAPB_INVALUE;

	if (parms->n2 < 1)
		return LAPB_INVALUE;

	if (lapb->state == LAPB_STATE_0) {
		if (parms->mode & LAPB_EXTENDED) {
			if (parms->window < 1 || parms->window > 127)
				return LAPB_INVALUE;
		} else {
			if (parms->window < 1 || parms->window > 7)
				return LAPB_INVALUE;
		}

		lapb->mode    = parms->mode;
		lapb->window  = parms->window;
	}

	lapb->t1    = parms->t1 * HZ;
	lapb->t2    = parms->t2 * HZ;
	lapb->n2    = parms->n2;

	return LAPB_OK;
}

int lapb_connect_request(void *token)
{
	lapb_cb *lapb;

	if ((lapb = lapb_tokentostruct(token)) == NULL)
		return LAPB_BADTOKEN;

	switch (lapb->state) {
		case LAPB_STATE_1:
			return LAPB_OK;
		case LAPB_STATE_3:
		case LAPB_STATE_4:
			return LAPB_CONNECTED;
	}

	lapb_establish_data_link(lapb);

#if LAPB_DEBUG > 0
	printk(KERN_DEBUG "lapb: (%p) S0 -> S1\n", lapb->token);
#endif

	lapb->state = LAPB_STATE_1;

	return LAPB_OK;
}

int lapb_disconnect_request(void *token)
{
	lapb_cb *lapb;

	if ((lapb = lapb_tokentostruct(token)) == NULL)
		return LAPB_BADTOKEN;

	switch (lapb->state) {
		case LAPB_STATE_0:
			return LAPB_NOTCONNECTED;

		case LAPB_STATE_1:
#if LAPB_DEBUG > 1
			printk(KERN_DEBUG "lapb: (%p) S1 TX DISC(1)\n", lapb->token);
#endif
#if LAPB_DEBUG > 0
			printk(KERN_DEBUG "lapb: (%p) S1 -> S0\n", lapb->token);
#endif
			lapb_send_control(lapb, LAPB_DISC, LAPB_POLLON, LAPB_COMMAND);
			lapb->state = LAPB_STATE_0;
			lapb_start_t1timer(lapb);
			return LAPB_NOTCONNECTED;

		case LAPB_STATE_2:
			return LAPB_OK;
	}

	lapb_clear_queues(lapb);
	lapb->n2count = 0;
	lapb_send_control(lapb, LAPB_DISC, LAPB_POLLON, LAPB_COMMAND);
	lapb_start_t1timer(lapb);
	lapb_stop_t2timer(lapb);
	lapb->state = LAPB_STATE_2;

#if LAPB_DEBUG > 1
	printk(KERN_DEBUG "lapb: (%p) S3 DISC(1)\n", lapb->token);
#endif
#if LAPB_DEBUG > 0
	printk(KERN_DEBUG "lapb: (%p) S3 -> S2\n", lapb->token);
#endif

	return LAPB_OK;
}

int lapb_data_request(void *token, struct sk_buff *skb)
{
	lapb_cb *lapb;

	if ((lapb = lapb_tokentostruct(token)) == NULL)
		return LAPB_BADTOKEN;

	if (lapb->state != LAPB_STATE_3 && lapb->state != LAPB_STATE_4)
		return LAPB_NOTCONNECTED;

	skb_queue_tail(&lapb->write_queue, skb);

	lapb_kick(lapb);

	return LAPB_OK;
}

int lapb_data_received(void *token, struct sk_buff *skb)
{
	lapb_cb *lapb;

	if ((lapb = lapb_tokentostruct(token)) == NULL)
		return LAPB_BADTOKEN;

	lapb_data_input(lapb, skb);

	return LAPB_OK;
}

void lapb_connect_confirmation(lapb_cb *lapb, int reason)
{
	if (lapb->callbacks.connect_confirmation != NULL)
		(lapb->callbacks.connect_confirmation)(lapb->token, reason);
}

void lapb_connect_indication(lapb_cb *lapb, int reason)
{
	if (lapb->callbacks.connect_indication != NULL)
		(lapb->callbacks.connect_indication)(lapb->token, reason);
}

void lapb_disconnect_confirmation(lapb_cb *lapb, int reason)
{
	if (lapb->callbacks.disconnect_confirmation != NULL)
		(lapb->callbacks.disconnect_confirmation)(lapb->token, reason);
}

void lapb_disconnect_indication(lapb_cb *lapb, int reason)
{
	if (lapb->callbacks.disconnect_indication != NULL)
		(lapb->callbacks.disconnect_indication)(lapb->token, reason);
}

int lapb_data_indication(lapb_cb *lapb, struct sk_buff *skb)
{
	if (lapb->callbacks.data_indication != NULL) {
		return (lapb->callbacks.data_indication)(lapb->token, skb);
	}
	kfree_skb(skb);
	return NET_RX_CN_HIGH; /* For now; must be != NET_RX_DROP */ 
}

int lapb_data_transmit(lapb_cb *lapb, struct sk_buff *skb)
{
	int used = 0;

	if (lapb->callbacks.data_transmit != NULL) {
		(lapb->callbacks.data_transmit)(lapb->token, skb);
		used = 1;
	}

	return used;
}

EXPORT_SYMBOL(lapb_register);
EXPORT_SYMBOL(lapb_unregister);
EXPORT_SYMBOL(lapb_getparms);
EXPORT_SYMBOL(lapb_setparms);
EXPORT_SYMBOL(lapb_connect_request);
EXPORT_SYMBOL(lapb_disconnect_request);
EXPORT_SYMBOL(lapb_data_request);
EXPORT_SYMBOL(lapb_data_received);

static char banner[] __initdata = KERN_INFO "NET4: LAPB for Linux. Version 0.01 for NET4.0\n";

static int __init lapb_init(void)
{
	printk(banner);
	return 0;
}

MODULE_AUTHOR("Jonathan Naylor <g4klx@g4klx.demon.co.uk>");
MODULE_DESCRIPTION("The X.25 Link Access Procedure B link layer protocol");
MODULE_LICENSE("GPL");

module_init(lapb_init);
