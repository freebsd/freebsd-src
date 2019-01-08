/*
 * Copyright (c) 2006 Mellanox Technologies Ltd.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * $Id$
 */
#include "sdp.h"

#ifdef CONFIG_INFINIBAND_SDP_DEBUG_DATA
void _dump_packet(const char *func, int line, struct sock *sk, char *str,
		struct sk_buff *skb, const struct sdp_bsdh *h)
{
	struct sdp_hh *hh;
	struct sdp_hah *hah;
	struct sdp_chrecvbuf *req_size;
	struct sdp_rrch *rrch;
	struct sdp_srcah *srcah;
	int len = 0;
	char buf[256];
	len += snprintf(buf, 255-len, "mid: %-20s flags: 0x%x "
			"bufs: 0x%x len: 0x%x mseq: 0x%x mseq_ack: 0x%x | ",
			mid2str(h->mid), h->flags,
			ntohs(h->bufs), ntohl(h->len), ntohl(h->mseq),
			ntohl(h->mseq_ack));

	switch (h->mid) {
	case SDP_MID_HELLO:
		hh = (struct sdp_hh *)h;
		len += snprintf(buf + len, 255-len,
				"max_adverts: %d  majv_minv: 0x%x "
				"localrcvsz: 0x%x desremrcvsz: 0x%x |",
				hh->max_adverts, hh->majv_minv,
				ntohl(hh->localrcvsz),
				ntohl(hh->desremrcvsz));
		break;
	case SDP_MID_HELLO_ACK:
		hah = (struct sdp_hah *)h;
		len += snprintf(buf + len, 255-len, "actrcvz: 0x%x |",
				ntohl(hah->actrcvsz));
		break;
	case SDP_MID_CHRCVBUF:
	case SDP_MID_CHRCVBUF_ACK:
		req_size = (struct sdp_chrecvbuf *)(h+1);
		len += snprintf(buf + len, 255-len, "req_size: 0x%x |",
				ntohl(req_size->size));
		break;
	case SDP_MID_DATA:
		len += snprintf(buf + len, 255-len, "data_len: 0x%zx |",
			ntohl(h->len) - sizeof(struct sdp_bsdh));
		break;
	case SDP_MID_RDMARDCOMPL:
		rrch = (struct sdp_rrch *)(h+1);

		len += snprintf(buf + len, 255-len, " | len: 0x%x |",
				ntohl(rrch->len));
		break;
	case SDP_MID_SRCAVAIL:
		srcah = (struct sdp_srcah *)(h+1);

		len += snprintf(buf + len, 255-len, " | payload: 0x%zx, "
				"len: 0x%x, rkey: 0x%x, vaddr: 0x%llx |",
				ntohl(h->len) - sizeof(struct sdp_bsdh) -
				sizeof(struct sdp_srcah),
				ntohl(srcah->len), ntohl(srcah->rkey),
				be64_to_cpu(srcah->vaddr));
		break;
	default:
		break;
	}
	buf[len] = 0;
	_sdp_printk(func, line, KERN_WARNING, sk, "%s: %s\n", str, buf);
	_sdp_prf(sk, skb, func, line, "%s: %s", str, buf);
}
#endif

static inline void update_send_head(struct sock *sk, struct sk_buff *skb)
{
	struct page *page;
	sk->sk_send_head = skb->next;
	if (sk->sk_send_head == (struct sk_buff *)&sk->sk_write_queue) {
		sk->sk_send_head = NULL;
		page = sk->sk_sndmsg_page;
		if (page) {
			put_page(page);
			sk->sk_sndmsg_page = NULL;
		}
	}
}

static inline int sdp_nagle_off(struct sdp_sock *ssk, struct sk_buff *skb)
{
	struct sdp_bsdh *h = (struct sdp_bsdh *)skb_transport_header(skb);
	int send_now =
		BZCOPY_STATE(skb) ||
		unlikely(h->mid != SDP_MID_DATA) ||
		(ssk->nonagle & TCP_NAGLE_OFF) ||
		!ssk->nagle_last_unacked ||
		skb->next != (struct sk_buff *)&sk_ssk(ssk)->sk_write_queue ||
		skb->len + sizeof(struct sdp_bsdh) >= ssk->xmit_size_goal ||
		(SDP_SKB_CB(skb)->flags & TCPCB_FLAG_PSH) ||
		(SDP_SKB_CB(skb)->flags & TCPCB_FLAG_URG);

	if (send_now) {
		unsigned long mseq = ring_head(ssk->tx_ring);
		ssk->nagle_last_unacked = mseq;
	} else {
		if (!timer_pending(&ssk->nagle_timer) && ssk->qp_active) {
			mod_timer(&ssk->nagle_timer,
					jiffies + SDP_NAGLE_TIMEOUT);
			sdp_dbg_data(sk_ssk(ssk), "Starting nagle timer\n");
		}
	}
	sdp_dbg_data(sk_ssk(ssk), "send_now = %d last_unacked = %u\n",
		send_now, ssk->nagle_last_unacked);

	return send_now;
}

void sdp_nagle_timeout(unsigned long data)
{
	struct sdp_sock *ssk = (struct sdp_sock *)data;
	struct sock *sk = sk_ssk(ssk);

	SDPSTATS_COUNTER_INC(nagle_timer);
	sdp_dbg_data(sk, "last_unacked = %u\n", ssk->nagle_last_unacked);

	if (!ssk->nagle_last_unacked)
		goto out2;

	/* Only process if the socket is not in use */
	bh_lock_sock(sk);
	if (sock_owned_by_user(sk)) {
		sdp_dbg_data(sk, "socket is busy - will try later\n");
		goto out;
	}

	if (sk->sk_state == TCP_CLOSE) {
		bh_unlock_sock(sk);
		return;
	}

	ssk->nagle_last_unacked = 0;
	sdp_post_sends(ssk, GFP_ATOMIC);

	if (sk->sk_sleep && waitqueue_active(sk->sk_sleep))
		sk_stream_write_space(sk);
out:
	bh_unlock_sock(sk);
out2:
	if (sk->sk_send_head && ssk->qp_active) {
		/* If has pending sends - rearm */
		mod_timer(&ssk->nagle_timer, jiffies + SDP_NAGLE_TIMEOUT);
	}
}

static inline int sdp_should_rearm(struct sock *sk)
{
	return sk->sk_state != TCP_ESTABLISHED || sdp_sk(sk)->tx_sa ||
		somebody_is_waiting(sk);
}

void sdp_post_sends(struct sdp_sock *ssk, gfp_t gfp)
{
	/* TODO: nonagle? */
	struct sk_buff *skb;
	int post_count = 0;
	struct sock *sk = sk_ssk(ssk);

	if (unlikely(!ssk->id)) {
		if (sk->sk_send_head) {
			sdp_dbg(sk, "Send on socket without cmid ECONNRESET\n");
			/* TODO: flush send queue? */
			sdp_reset(sk);
		}
		return;
	}
again:
	if (sdp_tx_ring_slots_left(ssk) < SDP_TX_SIZE / 2)
		sdp_xmit_poll(ssk, 1);

	/* Run out of credits, check if got a credit update */
	if (unlikely(tx_credits(ssk) <= SDP_MIN_TX_CREDITS)) {
		sdp_poll_rx_cq(ssk);

		if (unlikely(sdp_should_rearm(sk) || !posts_handler(ssk)))
			sdp_arm_rx_cq(sk);
	}

	if (ssk->recv_request &&
	    ring_tail(ssk->rx_ring) >= ssk->recv_request_head &&
	    tx_credits(ssk) >= SDP_MIN_TX_CREDITS &&
	    sdp_tx_ring_slots_left(ssk)) {
		skb = sdp_alloc_skb_chrcvbuf_ack(sk,
				ssk->recv_frags * PAGE_SIZE, gfp);
		if (likely(skb)) {
			ssk->recv_request = 0;
			sdp_post_send(ssk, skb);
			post_count++;
		}
	}

	if (tx_credits(ssk) <= SDP_MIN_TX_CREDITS &&
	       sdp_tx_ring_slots_left(ssk) &&
	       sk->sk_send_head &&
		sdp_nagle_off(ssk, sk->sk_send_head)) {
		SDPSTATS_COUNTER_INC(send_miss_no_credits);
	}

	while (tx_credits(ssk) > SDP_MIN_TX_CREDITS &&
	       sdp_tx_ring_slots_left(ssk) &&
	       (skb = sk->sk_send_head) &&
		sdp_nagle_off(ssk, skb)) {
		update_send_head(sk, skb);
		__skb_dequeue(&sk->sk_write_queue);

		sdp_post_send(ssk, skb);

		post_count++;
	}

	if (credit_update_needed(ssk) &&
	    likely((1 << sk->sk_state) &
		    (TCPF_ESTABLISHED | TCPF_FIN_WAIT1))) {

		skb = sdp_alloc_skb_data(sk, 0, gfp);
		if (likely(skb)) {
			sdp_post_send(ssk, skb);
			SDPSTATS_COUNTER_INC(post_send_credits);
			post_count++;
		}
	}

	/* send DisConn if needed
	 * Do not send DisConn if there is only 1 credit. Compliance with CA4-82
	 * If one credit is available, an implementation shall only send SDP
	 * messages that provide additional credits and also do not contain ULP
	 * payload. */
	if (unlikely(ssk->sdp_disconnect) &&
			!sk->sk_send_head &&
			tx_credits(ssk) > 1) {
		skb = sdp_alloc_skb_disconnect(sk, gfp);
		if (likely(skb)) {
			ssk->sdp_disconnect = 0;
			sdp_post_send(ssk, skb);
			post_count++;
		}
	}

	if (!sdp_tx_ring_slots_left(ssk) || post_count) {
		if (sdp_xmit_poll(ssk, 1))
			goto again;
	}
}
