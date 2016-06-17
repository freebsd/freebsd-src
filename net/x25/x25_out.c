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
 *	2000-09-04	Henner Eisen	Prevented x25_output() skb leakage.
 *	2000-10-27	Henner Eisen	MSG_DONTWAIT for fragment allocation.
 *	2000-11-10	Henner Eisen	x25_send_iframe(): re-queued frames
 *					needed cleaned seq-number fields.
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

static int x25_pacsize_to_bytes(unsigned int pacsize)
{
	int bytes = 1;

	if (pacsize == 0)
		return 128;

	while (pacsize-- > 0)
		bytes *= 2;

	return bytes;
}

/*
 *	This is where all X.25 information frames pass.
 *
 *      Returns the amount of user data bytes sent on success
 *      or a negative error code on failure.
 */
int x25_output(struct sock *sk, struct sk_buff *skb)
{
	struct sk_buff *skbn;
	unsigned char header[X25_EXT_MIN_LEN];
	int err, frontlen, len, header_len, max_len;
	int sent=0, noblock = X25_SKB_CB(skb)->flags & MSG_DONTWAIT;

	header_len = (sk->protinfo.x25->neighbour->extended) ? X25_EXT_MIN_LEN : X25_STD_MIN_LEN;
	max_len    = x25_pacsize_to_bytes(sk->protinfo.x25->facilities.pacsize_out);

	if (skb->len - header_len > max_len) {
		/* Save a copy of the Header */
		memcpy(header, skb->data, header_len);
		skb_pull(skb, header_len);

		frontlen = skb_headroom(skb);

		while (skb->len > 0) {
			if ((skbn = sock_alloc_send_skb(sk, frontlen + max_len, noblock, &err)) == NULL){
				if(err == -EWOULDBLOCK && noblock){
					kfree_skb(skb);
					return sent;
				}
				SOCK_DEBUG(sk, "x25_output: fragment allocation failed, err=%d, %d bytes sent\n", err, sent);
				return err;
			}
				
			skb_reserve(skbn, frontlen);

			len = (max_len > skb->len) ? skb->len : max_len;

			/* Copy the user data */
			memcpy(skb_put(skbn, len), skb->data, len);
			skb_pull(skb, len);

			/* Duplicate the Header */
			skb_push(skbn, header_len);
			memcpy(skbn->data, header, header_len);

			if (skb->len > 0) {
				if (sk->protinfo.x25->neighbour->extended)
					skbn->data[3] |= X25_EXT_M_BIT;
				else
					skbn->data[2] |= X25_STD_M_BIT;
			}

			skb_queue_tail(&sk->write_queue, skbn);
			sent += len;
		}
		
		kfree_skb(skb);
	} else {
		skb_queue_tail(&sk->write_queue, skb);
		sent = skb->len - header_len;
	}
	return sent;
}

/* 
 *	This procedure is passed a buffer descriptor for an iframe. It builds
 *	the rest of the control part of the frame and then writes it out.
 */
static void x25_send_iframe(struct sock *sk, struct sk_buff *skb)
{
	if (skb == NULL)
		return;

	if (sk->protinfo.x25->neighbour->extended) {
		skb->data[2]  = (sk->protinfo.x25->vs << 1) & 0xFE;
		skb->data[3] &= X25_EXT_M_BIT;
		skb->data[3] |= (sk->protinfo.x25->vr << 1) & 0xFE;
	} else {
		skb->data[2] &= X25_STD_M_BIT;
		skb->data[2] |= (sk->protinfo.x25->vs << 1) & 0x0E;
		skb->data[2] |= (sk->protinfo.x25->vr << 5) & 0xE0;
	}

	x25_transmit_link(skb, sk->protinfo.x25->neighbour);	
}

void x25_kick(struct sock *sk)
{
	struct sk_buff *skb, *skbn;
	unsigned short start, end;
	int modulus;

	if (sk->protinfo.x25->state != X25_STATE_3)
		return;

	/*
	 *	Transmit interrupt data.
	 */
	if (!sk->protinfo.x25->intflag && skb_peek(&sk->protinfo.x25->interrupt_out_queue) != NULL) {
		sk->protinfo.x25->intflag = 1;
		skb = skb_dequeue(&sk->protinfo.x25->interrupt_out_queue);
		x25_transmit_link(skb, sk->protinfo.x25->neighbour);
	}

	if (sk->protinfo.x25->condition & X25_COND_PEER_RX_BUSY)
		return;

	if (skb_peek(&sk->write_queue) == NULL)
		return;

	modulus = (sk->protinfo.x25->neighbour->extended) ? X25_EMODULUS : X25_SMODULUS;

	start   = (skb_peek(&sk->protinfo.x25->ack_queue) == NULL) ? sk->protinfo.x25->va : sk->protinfo.x25->vs;
	end     = (sk->protinfo.x25->va + sk->protinfo.x25->facilities.winsize_out) % modulus;

	if (start == end)
		return;

	sk->protinfo.x25->vs = start;

	/*
	 * Transmit data until either we're out of data to send or
	 * the window is full.
	 */

	skb = skb_dequeue(&sk->write_queue);

	do {
		if ((skbn = skb_clone(skb, GFP_ATOMIC)) == NULL) {
			skb_queue_head(&sk->write_queue, skb);
			break;
		}

		skb_set_owner_w(skbn, sk);

		/*
		 * Transmit the frame copy.
		 */
		x25_send_iframe(sk, skbn);

		sk->protinfo.x25->vs = (sk->protinfo.x25->vs + 1) % modulus;

		/*
		 * Requeue the original data frame.
		 */
		skb_queue_tail(&sk->protinfo.x25->ack_queue, skb);

	} while (sk->protinfo.x25->vs != end && (skb = skb_dequeue(&sk->write_queue)) != NULL);

	sk->protinfo.x25->vl         = sk->protinfo.x25->vr;
	sk->protinfo.x25->condition &= ~X25_COND_ACK_PENDING;

	x25_stop_timer(sk);
}

/*
 * The following routines are taken from page 170 of the 7th ARRL Computer
 * Networking Conference paper, as is the whole state machine.
 */

void x25_enquiry_response(struct sock *sk)
{
	if (sk->protinfo.x25->condition & X25_COND_OWN_RX_BUSY)
		x25_write_internal(sk, X25_RNR);
	else
		x25_write_internal(sk, X25_RR);

	sk->protinfo.x25->vl         = sk->protinfo.x25->vr;
	sk->protinfo.x25->condition &= ~X25_COND_ACK_PENDING;

	x25_stop_timer(sk);
}
