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
 *	NET/ROM 001	Jonathan(G4KLX)	Cloned from ax25_timer.c
 *	NET/ROM 007	Jonathan(G4KLX)	New timer architecture.
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
#include <asm/uaccess.h>
#include <asm/system.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <net/netrom.h>

static void nr_heartbeat_expiry(unsigned long);
static void nr_t1timer_expiry(unsigned long);
static void nr_t2timer_expiry(unsigned long);
static void nr_t4timer_expiry(unsigned long);
static void nr_idletimer_expiry(unsigned long);

void nr_start_t1timer(struct sock *sk)
{
	del_timer(&sk->protinfo.nr->t1timer);

	sk->protinfo.nr->t1timer.data     = (unsigned long)sk;
	sk->protinfo.nr->t1timer.function = &nr_t1timer_expiry;
	sk->protinfo.nr->t1timer.expires  = jiffies + sk->protinfo.nr->t1;

	add_timer(&sk->protinfo.nr->t1timer);
}

void nr_start_t2timer(struct sock *sk)
{
	del_timer(&sk->protinfo.nr->t2timer);

	sk->protinfo.nr->t2timer.data     = (unsigned long)sk;
	sk->protinfo.nr->t2timer.function = &nr_t2timer_expiry;
	sk->protinfo.nr->t2timer.expires  = jiffies + sk->protinfo.nr->t2;

	add_timer(&sk->protinfo.nr->t2timer);
}

void nr_start_t4timer(struct sock *sk)
{
	del_timer(&sk->protinfo.nr->t4timer);

	sk->protinfo.nr->t4timer.data     = (unsigned long)sk;
	sk->protinfo.nr->t4timer.function = &nr_t4timer_expiry;
	sk->protinfo.nr->t4timer.expires  = jiffies + sk->protinfo.nr->t4;

	add_timer(&sk->protinfo.nr->t4timer);
}

void nr_start_idletimer(struct sock *sk)
{
	del_timer(&sk->protinfo.nr->idletimer);

	if (sk->protinfo.nr->idle > 0) {
		sk->protinfo.nr->idletimer.data     = (unsigned long)sk;
		sk->protinfo.nr->idletimer.function = &nr_idletimer_expiry;
		sk->protinfo.nr->idletimer.expires  = jiffies + sk->protinfo.nr->idle;

		add_timer(&sk->protinfo.nr->idletimer);
	}
}

void nr_start_heartbeat(struct sock *sk)
{
	del_timer(&sk->timer);

	sk->timer.data     = (unsigned long)sk;
	sk->timer.function = &nr_heartbeat_expiry;
	sk->timer.expires  = jiffies + 5 * HZ;

	add_timer(&sk->timer);
}

void nr_stop_t1timer(struct sock *sk)
{
	del_timer(&sk->protinfo.nr->t1timer);
}

void nr_stop_t2timer(struct sock *sk)
{
	del_timer(&sk->protinfo.nr->t2timer);
}

void nr_stop_t4timer(struct sock *sk)
{
	del_timer(&sk->protinfo.nr->t4timer);
}

void nr_stop_idletimer(struct sock *sk)
{
	del_timer(&sk->protinfo.nr->idletimer);
}

void nr_stop_heartbeat(struct sock *sk)
{
	del_timer(&sk->timer);
}

int nr_t1timer_running(struct sock *sk)
{
	return timer_pending(&sk->protinfo.nr->t1timer);
}

static void nr_heartbeat_expiry(unsigned long param)
{
	struct sock *sk = (struct sock *)param;

	switch (sk->protinfo.nr->state) {

		case NR_STATE_0:
			/* Magic here: If we listen() and a new link dies before it
			   is accepted() it isn't 'dead' so doesn't get removed. */
			if (sk->destroy || (sk->state == TCP_LISTEN && sk->dead)) {
				nr_destroy_socket(sk);
				return;
			}
			break;

		case NR_STATE_3:
			/*
			 * Check for the state of the receive buffer.
			 */
			if (atomic_read(&sk->rmem_alloc) < (sk->rcvbuf / 2) &&
			    (sk->protinfo.nr->condition & NR_COND_OWN_RX_BUSY)) {
				sk->protinfo.nr->condition &= ~NR_COND_OWN_RX_BUSY;
				sk->protinfo.nr->condition &= ~NR_COND_ACK_PENDING;
				sk->protinfo.nr->vl         = sk->protinfo.nr->vr;
				nr_write_internal(sk, NR_INFOACK);
				break;
			}
			break;
	}

	nr_start_heartbeat(sk);
}

static void nr_t2timer_expiry(unsigned long param)
{
	struct sock *sk = (struct sock *)param;

	if (sk->protinfo.nr->condition & NR_COND_ACK_PENDING) {
		sk->protinfo.nr->condition &= ~NR_COND_ACK_PENDING;
		nr_enquiry_response(sk);
	}
}

static void nr_t4timer_expiry(unsigned long param)
{
	struct sock *sk = (struct sock *)param;

	sk->protinfo.nr->condition &= ~NR_COND_PEER_RX_BUSY;
}

static void nr_idletimer_expiry(unsigned long param)
{
	struct sock *sk = (struct sock *)param;

	nr_clear_queues(sk);

	sk->protinfo.nr->n2count = 0;
	nr_write_internal(sk, NR_DISCREQ);
	sk->protinfo.nr->state = NR_STATE_2;

	nr_start_t1timer(sk);
	nr_stop_t2timer(sk);
	nr_stop_t4timer(sk);

	sk->state     = TCP_CLOSE;
	sk->err       = 0;
	sk->shutdown |= SEND_SHUTDOWN;

	if (!sk->dead)
		sk->state_change(sk);

	sk->dead = 1;
}

static void nr_t1timer_expiry(unsigned long param)
{
	struct sock *sk = (struct sock *)param;

	switch (sk->protinfo.nr->state) {

		case NR_STATE_1: 
			if (sk->protinfo.nr->n2count == sk->protinfo.nr->n2) {
				nr_disconnect(sk, ETIMEDOUT);
				return;
			} else {
				sk->protinfo.nr->n2count++;
				nr_write_internal(sk, NR_CONNREQ);
			}
			break;

		case NR_STATE_2:
			if (sk->protinfo.nr->n2count == sk->protinfo.nr->n2) {
				nr_disconnect(sk, ETIMEDOUT);
				return;
			} else {
				sk->protinfo.nr->n2count++;
				nr_write_internal(sk, NR_DISCREQ);
			}
			break;

		case NR_STATE_3:
			if (sk->protinfo.nr->n2count == sk->protinfo.nr->n2) {
				nr_disconnect(sk, ETIMEDOUT);
				return;
			} else {
				sk->protinfo.nr->n2count++;
				nr_requeue_frames(sk);
			}
			break;
	}

	nr_start_t1timer(sk);
}
