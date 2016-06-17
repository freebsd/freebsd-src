/*
 *	ROSE release 003
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
 *	ROSE 001	Jonathan(G4KLX)	Cloned from nr_timer.c
 *	ROSE 003	Jonathan(G4KLX)	New timer architecture.
 *					Implemented idle timer.
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
#include <net/ax25.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <asm/segment.h>
#include <asm/system.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <net/rose.h>

static void rose_heartbeat_expiry(unsigned long);
static void rose_timer_expiry(unsigned long);
static void rose_idletimer_expiry(unsigned long);

void rose_start_heartbeat(struct sock *sk)
{
	del_timer(&sk->timer);

	sk->timer.data     = (unsigned long)sk;
	sk->timer.function = &rose_heartbeat_expiry;
	sk->timer.expires  = jiffies + 5 * HZ;

	add_timer(&sk->timer);
}

void rose_start_t1timer(struct sock *sk)
{
	del_timer(&sk->protinfo.rose->timer);

	sk->protinfo.rose->timer.data     = (unsigned long)sk;
	sk->protinfo.rose->timer.function = &rose_timer_expiry;
	sk->protinfo.rose->timer.expires  = jiffies + sk->protinfo.rose->t1;

	add_timer(&sk->protinfo.rose->timer);
}

void rose_start_t2timer(struct sock *sk)
{
	del_timer(&sk->protinfo.rose->timer);

	sk->protinfo.rose->timer.data     = (unsigned long)sk;
	sk->protinfo.rose->timer.function = &rose_timer_expiry;
	sk->protinfo.rose->timer.expires  = jiffies + sk->protinfo.rose->t2;

	add_timer(&sk->protinfo.rose->timer);
}

void rose_start_t3timer(struct sock *sk)
{
	del_timer(&sk->protinfo.rose->timer);

	sk->protinfo.rose->timer.data     = (unsigned long)sk;
	sk->protinfo.rose->timer.function = &rose_timer_expiry;
	sk->protinfo.rose->timer.expires  = jiffies + sk->protinfo.rose->t3;

	add_timer(&sk->protinfo.rose->timer);
}

void rose_start_hbtimer(struct sock *sk)
{
	del_timer(&sk->protinfo.rose->timer);

	sk->protinfo.rose->timer.data     = (unsigned long)sk;
	sk->protinfo.rose->timer.function = &rose_timer_expiry;
	sk->protinfo.rose->timer.expires  = jiffies + sk->protinfo.rose->hb;

	add_timer(&sk->protinfo.rose->timer);
}

void rose_start_idletimer(struct sock *sk)
{
	del_timer(&sk->protinfo.rose->idletimer);

	if (sk->protinfo.rose->idle > 0) {
		sk->protinfo.rose->idletimer.data     = (unsigned long)sk;
		sk->protinfo.rose->idletimer.function = &rose_idletimer_expiry;
		sk->protinfo.rose->idletimer.expires  = jiffies + sk->protinfo.rose->idle;

		add_timer(&sk->protinfo.rose->idletimer);
	}
}

void rose_stop_heartbeat(struct sock *sk)
{
	del_timer(&sk->timer);
}

void rose_stop_timer(struct sock *sk)
{
	del_timer(&sk->protinfo.rose->timer);
}

void rose_stop_idletimer(struct sock *sk)
{
	del_timer(&sk->protinfo.rose->idletimer);
}

static void rose_heartbeat_expiry(unsigned long param)
{
	struct sock *sk = (struct sock *)param;

	switch (sk->protinfo.rose->state) {

		case ROSE_STATE_0:
			/* Magic here: If we listen() and a new link dies before it
			   is accepted() it isn't 'dead' so doesn't get removed. */
			if (sk->destroy || (sk->state == TCP_LISTEN && sk->dead)) {
				rose_destroy_socket(sk);
				return;
			}
			break;

		case ROSE_STATE_3:
			/*
			 * Check for the state of the receive buffer.
			 */
			if (atomic_read(&sk->rmem_alloc) < (sk->rcvbuf / 2) &&
			    (sk->protinfo.rose->condition & ROSE_COND_OWN_RX_BUSY)) {
				sk->protinfo.rose->condition &= ~ROSE_COND_OWN_RX_BUSY;
				sk->protinfo.rose->condition &= ~ROSE_COND_ACK_PENDING;
				sk->protinfo.rose->vl         = sk->protinfo.rose->vr;
				rose_write_internal(sk, ROSE_RR);
				rose_stop_timer(sk);	/* HB */
				break;
			}
			break;
	}

	rose_start_heartbeat(sk);
}

static void rose_timer_expiry(unsigned long param)
{
	struct sock *sk = (struct sock *)param;

	switch (sk->protinfo.rose->state) {

		case ROSE_STATE_1:	/* T1 */
		case ROSE_STATE_4:	/* T2 */
			rose_write_internal(sk, ROSE_CLEAR_REQUEST);
			sk->protinfo.rose->state = ROSE_STATE_2;
			rose_start_t3timer(sk);
			break;

		case ROSE_STATE_2:	/* T3 */
			sk->protinfo.rose->neighbour->use--;
			rose_disconnect(sk, ETIMEDOUT, -1, -1);
			break;

		case ROSE_STATE_3:	/* HB */
			if (sk->protinfo.rose->condition & ROSE_COND_ACK_PENDING) {
				sk->protinfo.rose->condition &= ~ROSE_COND_ACK_PENDING;
				rose_enquiry_response(sk);
			}
			break;
	}
}

static void rose_idletimer_expiry(unsigned long param)
{
	struct sock *sk = (struct sock *)param;

	rose_clear_queues(sk);

	rose_write_internal(sk, ROSE_CLEAR_REQUEST);
	sk->protinfo.rose->state = ROSE_STATE_2;

	rose_start_t3timer(sk);

	sk->state     = TCP_CLOSE;
	sk->err       = 0;
	sk->shutdown |= SEND_SHUTDOWN;	

	if (!sk->dead)
		sk->state_change(sk);

	sk->dead = 1;
}
