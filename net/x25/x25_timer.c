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
 *	X.25 001	Jonathan Naylor	Started coding.
 *	X.25 002	Jonathan Naylor	New timer architecture.
 *					Centralised disconnection processing.
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
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <net/x25.h>

static void x25_heartbeat_expiry(unsigned long);
static void x25_timer_expiry(unsigned long);

void x25_start_heartbeat(struct sock *sk)
{
	del_timer(&sk->timer);

	sk->timer.data     = (unsigned long)sk;
	sk->timer.function = &x25_heartbeat_expiry;
	sk->timer.expires  = jiffies + 5 * HZ;

	add_timer(&sk->timer);
}

void x25_stop_heartbeat(struct sock *sk)
{
	del_timer(&sk->timer);
}

void x25_start_t2timer(struct sock *sk)
{
	del_timer(&sk->protinfo.x25->timer);

	sk->protinfo.x25->timer.data     = (unsigned long)sk;
	sk->protinfo.x25->timer.function = &x25_timer_expiry;
	sk->protinfo.x25->timer.expires  = jiffies + sk->protinfo.x25->t2;

	add_timer(&sk->protinfo.x25->timer);
}

void x25_start_t21timer(struct sock *sk)
{
	del_timer(&sk->protinfo.x25->timer);

	sk->protinfo.x25->timer.data     = (unsigned long)sk;
	sk->protinfo.x25->timer.function = &x25_timer_expiry;
	sk->protinfo.x25->timer.expires  = jiffies + sk->protinfo.x25->t21;

	add_timer(&sk->protinfo.x25->timer);
}

void x25_start_t22timer(struct sock *sk)
{
	del_timer(&sk->protinfo.x25->timer);

	sk->protinfo.x25->timer.data     = (unsigned long)sk;
	sk->protinfo.x25->timer.function = &x25_timer_expiry;
	sk->protinfo.x25->timer.expires  = jiffies + sk->protinfo.x25->t22;

	add_timer(&sk->protinfo.x25->timer);
}

void x25_start_t23timer(struct sock *sk)
{
	del_timer(&sk->protinfo.x25->timer);

	sk->protinfo.x25->timer.data     = (unsigned long)sk;
	sk->protinfo.x25->timer.function = &x25_timer_expiry;
	sk->protinfo.x25->timer.expires  = jiffies + sk->protinfo.x25->t23;

	add_timer(&sk->protinfo.x25->timer);
}

void x25_stop_timer(struct sock *sk)
{
	del_timer(&sk->protinfo.x25->timer);
}

unsigned long x25_display_timer(struct sock *sk)
{
	if (!timer_pending(&sk->protinfo.x25->timer))
		return 0;

	return sk->protinfo.x25->timer.expires - jiffies;
}

static void x25_heartbeat_expiry(unsigned long param)
{
	struct sock *sk = (struct sock *)param;

        bh_lock_sock(sk);
        if (sk->lock.users) { /* can currently only occur in state 3 */ 
		goto restart_heartbeat;
	}

	switch (sk->protinfo.x25->state) {

		case X25_STATE_0:
			/* Magic here: If we listen() and a new link dies before it
			   is accepted() it isn't 'dead' so doesn't get removed. */
			if (sk->destroy || (sk->state == TCP_LISTEN && sk->dead)) {
				x25_destroy_socket(sk);
				goto unlock;
			}
			break;

		case X25_STATE_3:
			/*
			 * Check for the state of the receive buffer.
			 */
			x25_check_rbuf(sk);
			break;
	}
 restart_heartbeat:
	x25_start_heartbeat(sk);
 unlock:
	bh_unlock_sock(sk);
}

/*
 *	Timer has expired, it may have been T2, T21, T22, or T23. We can tell
 *	by the state machine state.
 */
static inline void x25_do_timer_expiry(struct sock * sk)
{
	switch (sk->protinfo.x25->state) {

		case X25_STATE_3:	/* T2 */
			if (sk->protinfo.x25->condition & X25_COND_ACK_PENDING) {
				sk->protinfo.x25->condition &= ~X25_COND_ACK_PENDING;
				x25_enquiry_response(sk);
			}
			break;

		case X25_STATE_1:	/* T21 */
		case X25_STATE_4:	/* T22 */
			x25_write_internal(sk, X25_CLEAR_REQUEST);
			sk->protinfo.x25->state = X25_STATE_2;
			x25_start_t23timer(sk);
			break;

		case X25_STATE_2:	/* T23 */
			x25_disconnect(sk, ETIMEDOUT, 0, 0);
			break;
	}
}

static void x25_timer_expiry(unsigned long param)
{
	struct sock *sk = (struct sock *)param;

	bh_lock_sock(sk);
	if (sk->lock.users) { /* can currently only occur in state 3 */
		if (sk->protinfo.x25->state == X25_STATE_3) {
			x25_start_t2timer(sk);
		}
	} else {
		x25_do_timer_expiry(sk);
	}
	bh_unlock_sock(sk);
}
