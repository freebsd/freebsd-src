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
 *	Most of this code is based on the SDL diagrams published in the 7th
 *	ARRL Computer Networking Conference papers. The diagrams have mistakes
 *	in them, but are mostly correct. Before you modify the code could you
 *	read the SDL diagrams as the code is not obvious and probably very
 *	easy to break;
 *
 *	History
 *	ROSE 001	Jonathan(G4KLX)	Cloned from nr_in.c
 *	ROSE 002	Jonathan(G4KLX)	Return cause and diagnostic codes from Clear Requests.
 *	ROSE 003	Jonathan(G4KLX)	New timer architecture.
 *					Removed M bit processing.
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
#include <net/ip.h>			/* For ip_rcv */
#include <asm/segment.h>
#include <asm/system.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <net/rose.h>

/*
 * State machine for state 1, Awaiting Call Accepted State.
 * The handling of the timer(s) is in file rose_timer.c.
 * Handling of state 0 and connection release is in af_rose.c.
 */
static int rose_state1_machine(struct sock *sk, struct sk_buff *skb, int frametype)
{
	switch (frametype) {

		case ROSE_CALL_ACCEPTED:
			rose_stop_timer(sk);
			rose_start_idletimer(sk);
			sk->protinfo.rose->condition = 0x00;
			sk->protinfo.rose->vs        = 0;
			sk->protinfo.rose->va        = 0;
			sk->protinfo.rose->vr        = 0;
			sk->protinfo.rose->vl        = 0;
			sk->protinfo.rose->state     = ROSE_STATE_3;
			sk->state                    = TCP_ESTABLISHED;
			if (!sk->dead)
				sk->state_change(sk);
			break;

		case ROSE_CLEAR_REQUEST:
			rose_write_internal(sk, ROSE_CLEAR_CONFIRMATION);
			rose_disconnect(sk, ECONNREFUSED, skb->data[3], skb->data[4]);
			sk->protinfo.rose->neighbour->use--;
			break;

		default:
			break;
	}

	return 0;
}

/*
 * State machine for state 2, Awaiting Clear Confirmation State.
 * The handling of the timer(s) is in file rose_timer.c
 * Handling of state 0 and connection release is in af_rose.c.
 */
static int rose_state2_machine(struct sock *sk, struct sk_buff *skb, int frametype)
{
	switch (frametype) {

		case ROSE_CLEAR_REQUEST:
			rose_write_internal(sk, ROSE_CLEAR_CONFIRMATION);
			rose_disconnect(sk, 0, skb->data[3], skb->data[4]);
			sk->protinfo.rose->neighbour->use--;
			break;

		case ROSE_CLEAR_CONFIRMATION:
			rose_disconnect(sk, 0, -1, -1);
			sk->protinfo.rose->neighbour->use--;
			break;

		default:
			break;
	}

	return 0;
}

/*
 * State machine for state 3, Connected State.
 * The handling of the timer(s) is in file rose_timer.c
 * Handling of state 0 and connection release is in af_rose.c.
 */
static int rose_state3_machine(struct sock *sk, struct sk_buff *skb, int frametype, int ns, int nr, int q, int d, int m)
{
	int queued = 0;

	switch (frametype) {

		case ROSE_RESET_REQUEST:
			rose_stop_timer(sk);
			rose_start_idletimer(sk);
			rose_write_internal(sk, ROSE_RESET_CONFIRMATION);
			sk->protinfo.rose->condition = 0x00;
			sk->protinfo.rose->vs        = 0;
			sk->protinfo.rose->vr        = 0;
			sk->protinfo.rose->va        = 0;
			sk->protinfo.rose->vl        = 0;
			rose_requeue_frames(sk);
			break;

		case ROSE_CLEAR_REQUEST:
			rose_write_internal(sk, ROSE_CLEAR_CONFIRMATION);
			rose_disconnect(sk, 0, skb->data[3], skb->data[4]);
			sk->protinfo.rose->neighbour->use--;
			break;

		case ROSE_RR:
		case ROSE_RNR:
			if (!rose_validate_nr(sk, nr)) {
				rose_write_internal(sk, ROSE_RESET_REQUEST);
				sk->protinfo.rose->condition = 0x00;
				sk->protinfo.rose->vs        = 0;
				sk->protinfo.rose->vr        = 0;
				sk->protinfo.rose->va        = 0;
				sk->protinfo.rose->vl        = 0;
				sk->protinfo.rose->state     = ROSE_STATE_4;
				rose_start_t2timer(sk);
				rose_stop_idletimer(sk);
			} else {
				rose_frames_acked(sk, nr);
				if (frametype == ROSE_RNR) {
					sk->protinfo.rose->condition |= ROSE_COND_PEER_RX_BUSY;
				} else {
					sk->protinfo.rose->condition &= ~ROSE_COND_PEER_RX_BUSY;
				}
			}
			break;

		case ROSE_DATA:	/* XXX */
			sk->protinfo.rose->condition &= ~ROSE_COND_PEER_RX_BUSY;
			if (!rose_validate_nr(sk, nr)) {
				rose_write_internal(sk, ROSE_RESET_REQUEST);
				sk->protinfo.rose->condition = 0x00;
				sk->protinfo.rose->vs        = 0;
				sk->protinfo.rose->vr        = 0;
				sk->protinfo.rose->va        = 0;
				sk->protinfo.rose->vl        = 0;
				sk->protinfo.rose->state     = ROSE_STATE_4;
				rose_start_t2timer(sk);
				rose_stop_idletimer(sk);
				break;
			}
			rose_frames_acked(sk, nr);
			if (ns == sk->protinfo.rose->vr) {
				rose_start_idletimer(sk);
				if (sock_queue_rcv_skb(sk, skb) == 0) {
					sk->protinfo.rose->vr = (sk->protinfo.rose->vr + 1) % ROSE_MODULUS;
					queued = 1;
				} else {
					/* Should never happen ! */
					rose_write_internal(sk, ROSE_RESET_REQUEST);
					sk->protinfo.rose->condition = 0x00;
					sk->protinfo.rose->vs        = 0;
					sk->protinfo.rose->vr        = 0;
					sk->protinfo.rose->va        = 0;
					sk->protinfo.rose->vl        = 0;
					sk->protinfo.rose->state     = ROSE_STATE_4;
					rose_start_t2timer(sk);
					rose_stop_idletimer(sk);
					break;
				}
				if (atomic_read(&sk->rmem_alloc) > (sk->rcvbuf / 2))
					sk->protinfo.rose->condition |= ROSE_COND_OWN_RX_BUSY;
			}
			/*
			 * If the window is full, ack the frame, else start the
			 * acknowledge hold back timer.
			 */
			if (((sk->protinfo.rose->vl + sysctl_rose_window_size) % ROSE_MODULUS) == sk->protinfo.rose->vr) {
				sk->protinfo.rose->condition &= ~ROSE_COND_ACK_PENDING;
				rose_stop_timer(sk);
				rose_enquiry_response(sk);
			} else {
				sk->protinfo.rose->condition |= ROSE_COND_ACK_PENDING;
				rose_start_hbtimer(sk);
			}
			break;

		default:
			printk(KERN_WARNING "ROSE: unknown %02X in state 3\n", frametype);
			break;
	}

	return queued;
}

/*
 * State machine for state 4, Awaiting Reset Confirmation State.
 * The handling of the timer(s) is in file rose_timer.c
 * Handling of state 0 and connection release is in af_rose.c.
 */
static int rose_state4_machine(struct sock *sk, struct sk_buff *skb, int frametype)
{
	switch (frametype) {

		case ROSE_RESET_REQUEST:
			rose_write_internal(sk, ROSE_RESET_CONFIRMATION);
		case ROSE_RESET_CONFIRMATION:
			rose_stop_timer(sk);
			rose_start_idletimer(sk);
			sk->protinfo.rose->condition = 0x00;
			sk->protinfo.rose->va        = 0;
			sk->protinfo.rose->vr        = 0;
			sk->protinfo.rose->vs        = 0;
			sk->protinfo.rose->vl        = 0;
			sk->protinfo.rose->state     = ROSE_STATE_3;
			rose_requeue_frames(sk);
			break;

		case ROSE_CLEAR_REQUEST:
			rose_write_internal(sk, ROSE_CLEAR_CONFIRMATION);
			rose_disconnect(sk, 0, skb->data[3], skb->data[4]);
			sk->protinfo.rose->neighbour->use--;
			break;

		default:
			break;
	}

	return 0;
}

/*
 * State machine for state 5, Awaiting Call Acceptance State.
 * The handling of the timer(s) is in file rose_timer.c
 * Handling of state 0 and connection release is in af_rose.c.
 */
static int rose_state5_machine(struct sock *sk, struct sk_buff *skb, int frametype)
{
	if (frametype == ROSE_CLEAR_REQUEST) {
		rose_write_internal(sk, ROSE_CLEAR_CONFIRMATION);
		rose_disconnect(sk, 0, skb->data[3], skb->data[4]);
		sk->protinfo.rose->neighbour->use--;
	}

	return 0;
}

/* Higher level upcall for a LAPB frame */
int rose_process_rx_frame(struct sock *sk, struct sk_buff *skb)
{
	int queued = 0, frametype, ns, nr, q, d, m;

	if (sk->protinfo.rose->state == ROSE_STATE_0)
		return 0;

	frametype = rose_decode(skb, &ns, &nr, &q, &d, &m);

	switch (sk->protinfo.rose->state) {
		case ROSE_STATE_1:
			queued = rose_state1_machine(sk, skb, frametype);
			break;
		case ROSE_STATE_2:
			queued = rose_state2_machine(sk, skb, frametype);
			break;
		case ROSE_STATE_3:
			queued = rose_state3_machine(sk, skb, frametype, ns, nr, q, d, m);
			break;
		case ROSE_STATE_4:
			queued = rose_state4_machine(sk, skb, frametype);
			break;
		case ROSE_STATE_5:
			queued = rose_state5_machine(sk, skb, frametype);
			break;
	}

	rose_kick(sk);

	return queued;
}
