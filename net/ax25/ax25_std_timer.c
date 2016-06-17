/*
 *	AX.25 release 037
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
 *	AX.25 028a	Jonathan(G4KLX)	New state machine based on SDL diagrams.
 *	AX.25 028b	Jonathan(G4KLX)	Extracted AX25 control block from the
 *					sock structure.
 *	AX.25 029	Alan(GW4PTS)	Switched to KA9Q constant names.
 *	AX.25 031	Joerg(DL1BKE)	Added DAMA support
 *	AX.25 032	Joerg(DL1BKE)	Fixed DAMA timeout bug
 *	AX.25 033	Jonathan(G4KLX)	Modularisation functions.
 *	AX.25 035	Frederic(F1OAT)	Support for pseudo-digipeating.
 *	AX.25 036	Jonathan(G4KLX)	Split from ax25_timer.c.
 *	AX.25 037	Jonathan(G4KLX)	New timer architecture.
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

void ax25_std_heartbeat_expiry(ax25_cb *ax25)
{
	switch (ax25->state) {

		case AX25_STATE_0:
			/* Magic here: If we listen() and a new link dies before it
			   is accepted() it isn't 'dead' so doesn't get removed. */
			if (ax25->sk == NULL || ax25->sk->destroy || (ax25->sk->state == TCP_LISTEN && ax25->sk->dead)) {
				ax25_destroy_socket(ax25);
				return;
			}
			break;

		case AX25_STATE_3:
		case AX25_STATE_4:
			/*
			 * Check the state of the receive buffer.
			 */
			if (ax25->sk != NULL) {
				if (atomic_read(&ax25->sk->rmem_alloc) < (ax25->sk->rcvbuf / 2) &&
				    (ax25->condition & AX25_COND_OWN_RX_BUSY)) {
					ax25->condition &= ~AX25_COND_OWN_RX_BUSY;
					ax25->condition &= ~AX25_COND_ACK_PENDING;
					ax25_send_control(ax25, AX25_RR, AX25_POLLOFF, AX25_RESPONSE);
					break;
				}
			}
	}

	ax25_start_heartbeat(ax25);
}

void ax25_std_t2timer_expiry(ax25_cb *ax25)
{
	if (ax25->condition & AX25_COND_ACK_PENDING) {
		ax25->condition &= ~AX25_COND_ACK_PENDING;
		ax25_std_timeout_response(ax25);
	}
}

void ax25_std_t3timer_expiry(ax25_cb *ax25)
{
	ax25->n2count = 0;
	ax25_std_transmit_enquiry(ax25);
	ax25->state   = AX25_STATE_4;
}

void ax25_std_idletimer_expiry(ax25_cb *ax25)
{
	ax25_clear_queues(ax25);

	ax25->n2count = 0;
	ax25_send_control(ax25, AX25_DISC, AX25_POLLON, AX25_COMMAND);
	ax25->state   = AX25_STATE_2;

	ax25_calculate_t1(ax25);
	ax25_start_t1timer(ax25);
	ax25_stop_t2timer(ax25);
	ax25_stop_t3timer(ax25);

	if (ax25->sk != NULL) {
		ax25->sk->state     = TCP_CLOSE;
		ax25->sk->err       = 0;
		ax25->sk->shutdown |= SEND_SHUTDOWN;
		if (!ax25->sk->dead)
			ax25->sk->state_change(ax25->sk);
		ax25->sk->dead      = 1;
	}
}

void ax25_std_t1timer_expiry(ax25_cb *ax25)
{
	switch (ax25->state) {
		case AX25_STATE_1: 
			if (ax25->n2count == ax25->n2) {
				if (ax25->modulus == AX25_MODULUS) {
					ax25_disconnect(ax25, ETIMEDOUT);
					return;
				} else {
					ax25->modulus = AX25_MODULUS;
					ax25->window  = ax25->ax25_dev->values[AX25_VALUES_WINDOW];
					ax25->n2count = 0;
					ax25_send_control(ax25, AX25_SABM, AX25_POLLON, AX25_COMMAND);
				}
			} else {
				ax25->n2count++;
				if (ax25->modulus == AX25_MODULUS)
					ax25_send_control(ax25, AX25_SABM, AX25_POLLON, AX25_COMMAND);
				else
					ax25_send_control(ax25, AX25_SABME, AX25_POLLON, AX25_COMMAND);
			}
			break;

		case AX25_STATE_2:
			if (ax25->n2count == ax25->n2) {
				ax25_send_control(ax25, AX25_DISC, AX25_POLLON, AX25_COMMAND);
				ax25_disconnect(ax25, ETIMEDOUT);
				return;
			} else {
				ax25->n2count++;
				ax25_send_control(ax25, AX25_DISC, AX25_POLLON, AX25_COMMAND);
			}
			break;

		case AX25_STATE_3: 
			ax25->n2count = 1;
			ax25_std_transmit_enquiry(ax25);
			ax25->state   = AX25_STATE_4;
			break;

		case AX25_STATE_4:
			if (ax25->n2count == ax25->n2) {
				ax25_send_control(ax25, AX25_DM, AX25_POLLON, AX25_RESPONSE);
				ax25_disconnect(ax25, ETIMEDOUT);
				return;
			} else {
				ax25->n2count++;
				ax25_std_transmit_enquiry(ax25);
			}
			break;
	}

	ax25_calculate_t1(ax25);
	ax25_start_t1timer(ax25);
}
