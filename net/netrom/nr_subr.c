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
 *	NET/ROM 001	Jonathan(G4KLX)	Cloned from ax25_subr.c
 *	NET/ROM	003	Jonathan(G4KLX)	Added G8BPQ NET/ROM extensions.
 *	NET/ROM 007	Jonathan(G4KLX)	New timer architecture.
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

/*
 *	This routine purges all of the queues of frames.
 */
void nr_clear_queues(struct sock *sk)
{
	skb_queue_purge(&sk->write_queue);
	skb_queue_purge(&sk->protinfo.nr->ack_queue);
	skb_queue_purge(&sk->protinfo.nr->reseq_queue);
	skb_queue_purge(&sk->protinfo.nr->frag_queue);
}

/*
 * This routine purges the input queue of those frames that have been
 * acknowledged. This replaces the boxes labelled "V(a) <- N(r)" on the
 * SDL diagram.
 */
void nr_frames_acked(struct sock *sk, unsigned short nr)
{
	struct sk_buff *skb;

	/*
	 * Remove all the ack-ed frames from the ack queue.
	 */
	if (sk->protinfo.nr->va != nr) {
		while (skb_peek(&sk->protinfo.nr->ack_queue) != NULL && sk->protinfo.nr->va != nr) {
		        skb = skb_dequeue(&sk->protinfo.nr->ack_queue);
			kfree_skb(skb);
			sk->protinfo.nr->va = (sk->protinfo.nr->va + 1) % NR_MODULUS;
		}
	}
}

/*
 * Requeue all the un-ack-ed frames on the output queue to be picked
 * up by nr_kick called from the timer. This arrangement handles the
 * possibility of an empty output queue.
 */
void nr_requeue_frames(struct sock *sk)
{
	struct sk_buff *skb, *skb_prev = NULL;

	while ((skb = skb_dequeue(&sk->protinfo.nr->ack_queue)) != NULL) {
		if (skb_prev == NULL)
			skb_queue_head(&sk->write_queue, skb);
		else
			skb_append(skb_prev, skb);
		skb_prev = skb;
	}
}

/*
 *	Validate that the value of nr is between va and vs. Return true or
 *	false for testing.
 */
int nr_validate_nr(struct sock *sk, unsigned short nr)
{
	unsigned short vc = sk->protinfo.nr->va;

	while (vc != sk->protinfo.nr->vs) {
		if (nr == vc) return 1;
		vc = (vc + 1) % NR_MODULUS;
	}

	if (nr == sk->protinfo.nr->vs) return 1;

	return 0;
}

/*
 *	Check that ns is within the receive window.
 */
int nr_in_rx_window(struct sock *sk, unsigned short ns)
{
	unsigned short vc = sk->protinfo.nr->vr;
	unsigned short vt = (sk->protinfo.nr->vl + sk->protinfo.nr->window) % NR_MODULUS;

	while (vc != vt) {
		if (ns == vc) return 1;
		vc = (vc + 1) % NR_MODULUS;
	}

	return 0;
}

/* 
 *  This routine is called when the HDLC layer internally generates a
 *  control frame.
 */
void nr_write_internal(struct sock *sk, int frametype)
{
	struct sk_buff *skb;
	unsigned char  *dptr;
	int len, timeout;

	len = AX25_BPQ_HEADER_LEN + AX25_MAX_HEADER_LEN + NR_NETWORK_LEN + NR_TRANSPORT_LEN;

	switch (frametype & 0x0F) {
		case NR_CONNREQ:
			len += 17;
			break;
		case NR_CONNACK:
			len += (sk->protinfo.nr->bpqext) ? 2 : 1;
			break;
		case NR_DISCREQ:
		case NR_DISCACK:
		case NR_INFOACK:
			break;
		default:
			printk(KERN_ERR "NET/ROM: nr_write_internal - invalid frame type %d\n", frametype);
			return;
	}

	if ((skb = alloc_skb(len, GFP_ATOMIC)) == NULL)
		return;

	/*
	 *	Space for AX.25 and NET/ROM network header
	 */
	skb_reserve(skb, AX25_BPQ_HEADER_LEN + AX25_MAX_HEADER_LEN + NR_NETWORK_LEN);
	
	dptr = skb_put(skb, skb_tailroom(skb));

	switch (frametype & 0x0F) {

		case NR_CONNREQ:
			timeout  = sk->protinfo.nr->t1 / HZ;
			*dptr++  = sk->protinfo.nr->my_index;
			*dptr++  = sk->protinfo.nr->my_id;
			*dptr++  = 0;
			*dptr++  = 0;
			*dptr++  = frametype;
			*dptr++  = sk->protinfo.nr->window;
			memcpy(dptr, &sk->protinfo.nr->user_addr, AX25_ADDR_LEN);
			dptr[6] &= ~AX25_CBIT;
			dptr[6] &= ~AX25_EBIT;
			dptr[6] |= AX25_SSSID_SPARE;
			dptr    += AX25_ADDR_LEN;
			memcpy(dptr, &sk->protinfo.nr->source_addr, AX25_ADDR_LEN);
			dptr[6] &= ~AX25_CBIT;
			dptr[6] &= ~AX25_EBIT;
			dptr[6] |= AX25_SSSID_SPARE;
			dptr    += AX25_ADDR_LEN;
			*dptr++  = timeout % 256;
			*dptr++  = timeout / 256;
			break;

		case NR_CONNACK:
			*dptr++ = sk->protinfo.nr->your_index;
			*dptr++ = sk->protinfo.nr->your_id;
			*dptr++ = sk->protinfo.nr->my_index;
			*dptr++ = sk->protinfo.nr->my_id;
			*dptr++ = frametype;
			*dptr++ = sk->protinfo.nr->window;
			if (sk->protinfo.nr->bpqext) *dptr++ = sysctl_netrom_network_ttl_initialiser;
			break;

		case NR_DISCREQ:
		case NR_DISCACK:
			*dptr++ = sk->protinfo.nr->your_index;
			*dptr++ = sk->protinfo.nr->your_id;
			*dptr++ = 0;
			*dptr++ = 0;
			*dptr++ = frametype;
			break;

		case NR_INFOACK:
			*dptr++ = sk->protinfo.nr->your_index;
			*dptr++ = sk->protinfo.nr->your_id;
			*dptr++ = 0;
			*dptr++ = sk->protinfo.nr->vr;
			*dptr++ = frametype;
			break;
	}

	nr_transmit_buffer(sk, skb);
}

/*
 * This routine is called when a Connect Acknowledge with the Choke Flag
 * set is needed to refuse a connection.
 */
void nr_transmit_refusal(struct sk_buff *skb, int mine)
{
	struct sk_buff *skbn;
	unsigned char *dptr;
	int len;

	len = AX25_BPQ_HEADER_LEN + AX25_MAX_HEADER_LEN + NR_NETWORK_LEN + NR_TRANSPORT_LEN + 1;

	if ((skbn = alloc_skb(len, GFP_ATOMIC)) == NULL)
		return;

	skb_reserve(skbn, AX25_BPQ_HEADER_LEN + AX25_MAX_HEADER_LEN);

	dptr = skb_put(skbn, NR_NETWORK_LEN + NR_TRANSPORT_LEN);

	memcpy(dptr, skb->data + 7, AX25_ADDR_LEN);
	dptr[6] &= ~AX25_CBIT;
	dptr[6] &= ~AX25_EBIT;
	dptr[6] |= AX25_SSSID_SPARE;
	dptr += AX25_ADDR_LEN;
	
	memcpy(dptr, skb->data + 0, AX25_ADDR_LEN);
	dptr[6] &= ~AX25_CBIT;
	dptr[6] |= AX25_EBIT;
	dptr[6] |= AX25_SSSID_SPARE;
	dptr += AX25_ADDR_LEN;

	*dptr++ = sysctl_netrom_network_ttl_initialiser;

	if (mine) {
		*dptr++ = 0;
		*dptr++ = 0;
		*dptr++ = skb->data[15];
		*dptr++ = skb->data[16];
	} else {
		*dptr++ = skb->data[15];
		*dptr++ = skb->data[16];
		*dptr++ = 0;
		*dptr++ = 0;
	}

	*dptr++ = NR_CONNACK | NR_CHOKE_FLAG;
	*dptr++ = 0;

	if (!nr_route_frame(skbn, NULL))
		kfree_skb(skbn);
}

void nr_disconnect(struct sock *sk, int reason)
{
	nr_stop_t1timer(sk);
	nr_stop_t2timer(sk);
	nr_stop_t4timer(sk);
	nr_stop_idletimer(sk);

	nr_clear_queues(sk);

	sk->protinfo.nr->state = NR_STATE_0;

	sk->state     = TCP_CLOSE;
	sk->err       = reason;
	sk->shutdown |= SEND_SHUTDOWN;

	if (!sk->dead)
		sk->state_change(sk);

	sk->dead = 1;
}
