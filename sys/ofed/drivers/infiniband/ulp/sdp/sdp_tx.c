/*
 * Copyright (c) 2009 Mellanox Technologies Ltd.  All rights reserved.
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
 */
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <rdma/ib_verbs.h>
#include <rdma/rdma_cm.h>
#include "sdp.h"

#define sdp_cnt(var) do { (var)++; } while (0)

SDP_MODPARAM_SINT(sdp_keepalive_probes_sent, 0,
		"Total number of keepalive probes sent.");

static int sdp_process_tx_cq(struct sdp_sock *ssk);

int sdp_xmit_poll(struct sdp_sock *ssk, int force)
{
	int wc_processed = 0;

	sdp_prf(sk_ssk(ssk), NULL, "%s", __func__);

	/* If we don't have a pending timer, set one up to catch our recent
	   post in case the interface becomes idle */
	if (likely(ssk->qp_active && sk_ssk(ssk)->sk_state != TCP_CLOSE) &&
			!timer_pending(&ssk->tx_ring.timer)) {
		mod_timer(&ssk->tx_ring.timer, jiffies + SDP_TX_POLL_TIMEOUT);
	}

	ssk->tx_compl_pending = 0;

	/* Poll the CQ every SDP_TX_POLL_MODER packets */
	if (force || (++ssk->tx_ring.poll_cnt & (SDP_TX_POLL_MODER - 1)) == 0)
		wc_processed = sdp_process_tx_cq(ssk);

	return wc_processed;
}

void sdp_post_send(struct sdp_sock *ssk, struct sk_buff *skb)
{
	struct sdp_buf *tx_req;
	struct sdp_bsdh *h = (struct sdp_bsdh *)skb_transport_header(skb);
	unsigned long mseq = ring_head(ssk->tx_ring);
	int i, rc, frags;
	u64 addr;
	struct ib_device *dev;
	struct ib_send_wr *bad_wr;

	struct ib_sge ibsge[SDP_MAX_SEND_SGES];
	struct ib_sge *sge = ibsge;
	struct ib_send_wr tx_wr = { NULL };
	u32 send_flags = IB_SEND_SIGNALED;

	SDPSTATS_COUNTER_MID_INC(post_send, h->mid);
	SDPSTATS_HIST(send_size, skb->len);

	if (!ssk->qp_active)
		goto err;

	ssk->tx_packets++;

	if (unlikely(h->mid == SDP_MID_SRCAVAIL)) {
		struct tx_srcavail_state *tx_sa = TX_SRCAVAIL_STATE(skb);
		if (ssk->tx_sa != tx_sa) {
			sdp_dbg_data(sk_ssk(ssk), "SrcAvail cancelled "
					"before being sent!\n");
			SDP_WARN_ON(1);
			sdp_free_skb(skb);
			return;
		}
		TX_SRCAVAIL_STATE(skb)->mseq = mseq;
	}

	if (unlikely(SDP_SKB_CB(skb)->flags & TCPCB_FLAG_URG))
		h->flags = SDP_OOB_PRES | SDP_OOB_PEND;
	else
		h->flags = 0;

	h->bufs = htons(rx_ring_posted(ssk));
	h->len = htonl(skb->len);
	h->mseq = htonl(mseq);
	h->mseq_ack = htonl(mseq_ack(ssk));

	sdp_prf(sk_ssk(ssk), skb, "TX: %s bufs: %d mseq:%ld ack:%d c: %d",
			mid2str(h->mid), rx_ring_posted(ssk), mseq,
			ntohl(h->mseq_ack), tx_credits(ssk));

	SDP_DUMP_PACKET(sk_ssk(ssk), "TX", skb, h);

	tx_req = &ssk->tx_ring.buffer[mseq & (SDP_TX_SIZE - 1)];
	tx_req->skb = skb;
	dev = ssk->ib_device;

	if (skb->len <= ssk->inline_thresh && !skb_shinfo(skb)->nr_frags) {
		SDPSTATS_COUNTER_INC(inline_sends);
		sge->addr = (u64) skb->data;
		sge->length = skb->len;
		sge->lkey = 0;
		frags = 0;
		tx_req->mapping[0] = 0; /* Nothing to be cleaned up by sdp_cleanup_sdp_buf() */
		send_flags |= IB_SEND_INLINE;
	} else {
		addr = ib_dma_map_single(dev, skb->data, skb->len - skb->data_len,
				DMA_TO_DEVICE);
		tx_req->mapping[0] = addr;

		/* TODO: proper error handling */
		BUG_ON(ib_dma_mapping_error(dev, addr));

		sge->addr = addr;
		sge->length = skb->len - skb->data_len;
		sge->lkey = ssk->sdp_dev->mr->lkey;
		frags = skb_shinfo(skb)->nr_frags;
		for (i = 0; i < frags; ++i) {
			++sge;
			addr = ib_dma_map_page(dev, skb_shinfo(skb)->frags[i].page,
					skb_shinfo(skb)->frags[i].page_offset,
					skb_shinfo(skb)->frags[i].size,
					DMA_TO_DEVICE);
			BUG_ON(ib_dma_mapping_error(dev, addr));
			tx_req->mapping[i + 1] = addr;
			sge->addr = addr;
			sge->length = skb_shinfo(skb)->frags[i].size;
			sge->lkey = ssk->sdp_dev->mr->lkey;
		}
	}

	tx_wr.next = NULL;
	tx_wr.wr_id = ring_head(ssk->tx_ring) | SDP_OP_SEND;
	tx_wr.sg_list = ibsge;
	tx_wr.num_sge = frags + 1;
	tx_wr.opcode = IB_WR_SEND;
	tx_wr.send_flags = send_flags;
	if (unlikely(SDP_SKB_CB(skb)->flags & TCPCB_FLAG_URG))
		tx_wr.send_flags |= IB_SEND_SOLICITED;

	rc = ib_post_send(ssk->qp, &tx_wr, &bad_wr);
	if (unlikely(rc)) {
		sdp_dbg(sk_ssk(ssk),
				"ib_post_send failed with status %d.\n", rc);

		sdp_cleanup_sdp_buf(ssk, tx_req, skb->len - skb->data_len, DMA_TO_DEVICE);

		sdp_set_error(sk_ssk(ssk), -ECONNRESET);

		goto err;
	}

	atomic_inc(&ssk->tx_ring.head);
	atomic_dec(&ssk->tx_ring.credits);
	atomic_set(&ssk->remote_credits, rx_ring_posted(ssk));

	return;

err:
	sdp_free_skb(skb);
}

static struct sk_buff *sdp_send_completion(struct sdp_sock *ssk, int mseq)
{
	struct ib_device *dev;
	struct sdp_buf *tx_req;
	struct sk_buff *skb = NULL;
	struct sdp_tx_ring *tx_ring = &ssk->tx_ring;
	if (unlikely(mseq != ring_tail(*tx_ring))) {
		printk(KERN_WARNING "Bogus send completion id %d tail %d\n",
			mseq, ring_tail(*tx_ring));
		goto out;
	}

	dev = ssk->ib_device;
	tx_req = &tx_ring->buffer[mseq & (SDP_TX_SIZE - 1)];
	skb = tx_req->skb;
	if (!skb)
		goto skip; /* This slot was used by RDMA WR */

	sdp_cleanup_sdp_buf(ssk, tx_req, skb->len - skb->data_len, DMA_TO_DEVICE);

	tx_ring->una_seq += SDP_SKB_CB(skb)->end_seq;

	/* TODO: AIO and real zcopy code; add their context support here */
	if (BZCOPY_STATE(skb))
		BZCOPY_STATE(skb)->busy--;

skip:
	atomic_inc(&tx_ring->tail);

out:
	return skb;
}

static inline void sdp_process_tx_wc(struct sdp_sock *ssk, struct ib_wc *wc)
{
	struct sock *sk = sk_ssk(ssk);

	if (likely(wc->wr_id & SDP_OP_SEND)) {
		struct sk_buff *skb;

		skb = sdp_send_completion(ssk, wc->wr_id);
		if (likely(skb))
			sk_wmem_free_skb(sk, skb);
	} else if (wc->wr_id & SDP_OP_RDMA) {
		if (ssk->tx_ring.rdma_inflight &&
				ssk->tx_ring.rdma_inflight->busy) {
			/* Only last RDMA read WR is signalled. Order is guaranteed -
			 * therefore if Last RDMA read WR is completed - all other
			 * have, too */
			ssk->tx_ring.rdma_inflight->busy = 0;
		} else {
			sdp_warn(sk, "Unexpected RDMA read completion, "
					"probably was canceled already\n");
		}

		wake_up(sk->sk_sleep);
	} else {
		/* Keepalive probe sent cleanup */
		sdp_cnt(sdp_keepalive_probes_sent);
	}

	if (likely(!wc->status) || wc->status == IB_WC_WR_FLUSH_ERR)
		return;

	sdp_warn(sk, "Send completion with error. wr_id 0x%llx Status %d\n", 
			wc->wr_id, wc->status);

	sdp_set_error(sk, -ECONNRESET);
}

static int sdp_process_tx_cq(struct sdp_sock *ssk)
{
	struct ib_wc ibwc[SDP_NUM_WC];
	int n, i;
	int wc_processed = 0;

	if (!ssk->tx_ring.cq) {
		sdp_dbg(sk_ssk(ssk), "tx irq on destroyed tx_cq\n");
		return 0;
	}

	do {
		n = ib_poll_cq(ssk->tx_ring.cq, SDP_NUM_WC, ibwc);
		for (i = 0; i < n; ++i) {
			sdp_process_tx_wc(ssk, ibwc + i);
			wc_processed++;
		}
	} while (n == SDP_NUM_WC);

	if (wc_processed) {
		struct sock *sk = sk_ssk(ssk);
		sdp_prf1(sk, NULL, "Waking sendmsg. inflight=%d",
				(u32) tx_ring_posted(ssk));
		sk_stream_write_space(sk_ssk(ssk));
		if (sk->sk_write_pending &&
				test_bit(SOCK_NOSPACE, &sk->sk_socket->flags) &&
				tx_ring_posted(ssk)) {
			/* a write is pending and still no room in tx queue,
			 * arm tx cq
			 */
			sdp_prf(sk_ssk(ssk), NULL, "pending tx - rearming");
			sdp_arm_tx_cq(sk);
		}

	}

	return wc_processed;
}

/* Select who will handle tx completion:
 * - a write is pending - wake it up and let it do the poll + post
 * - post handler is taken - taker will do the poll + post
 * else return 1 and let the caller do it
 */
static int sdp_tx_handler_select(struct sdp_sock *ssk)
{
	struct sock *sk = sk_ssk(ssk);

	if (sk->sk_write_pending) {
		/* Do the TX posts from sender context */
		if (sk->sk_sleep && waitqueue_active(sk->sk_sleep)) {
			sdp_prf1(sk, NULL, "Waking up pending sendmsg");
			wake_up_interruptible(sk->sk_sleep);
			return 0;
		} else
			sdp_prf1(sk, NULL, "Unexpected: sk_sleep=%p, "
				"waitqueue_active: %d\n",
				sk->sk_sleep, waitqueue_active(sk->sk_sleep));
	}

	if (posts_handler(ssk)) {
		/* Somebody else available to check for completion */
		sdp_prf1(sk, NULL, "Somebody else will call do_posts");
		return 0;
	}

	return 1;
}

static void sdp_poll_tx_timeout(unsigned long data)
{
	struct sdp_sock *ssk = (struct sdp_sock *)data;
	struct sock *sk = sk_ssk(ssk);
	u32 inflight, wc_processed;

	sdp_prf1(sk_ssk(ssk), NULL, "TX timeout: inflight=%d, head=%d tail=%d",
		(u32) tx_ring_posted(ssk),
		ring_head(ssk->tx_ring), ring_tail(ssk->tx_ring));

	/* Only process if the socket is not in use */
	bh_lock_sock(sk);
	if (sock_owned_by_user(sk)) {
		sdp_prf(sk_ssk(ssk), NULL, "TX comp: socket is busy");

		if (sdp_tx_handler_select(ssk) && sk->sk_state != TCP_CLOSE &&
				likely(ssk->qp_active)) {
			sdp_prf1(sk, NULL, "schedule a timer");
			mod_timer(&ssk->tx_ring.timer, jiffies + SDP_TX_POLL_TIMEOUT);
		}

		SDPSTATS_COUNTER_INC(tx_poll_busy);
		goto out;
	}

	if (unlikely(!ssk->qp || sk->sk_state == TCP_CLOSE)) {
		SDPSTATS_COUNTER_INC(tx_poll_no_op);
		goto out;
	}

	wc_processed = sdp_process_tx_cq(ssk);
	if (!wc_processed)
		SDPSTATS_COUNTER_INC(tx_poll_miss);
	else {
		sdp_post_sends(ssk, GFP_ATOMIC);
		SDPSTATS_COUNTER_INC(tx_poll_hit);
	}

	inflight = (u32) tx_ring_posted(ssk);
	sdp_prf1(sk_ssk(ssk), NULL, "finished tx proccessing. inflight = %d",
			tx_ring_posted(ssk));

	/* If there are still packets in flight and the timer has not already
	 * been scheduled by the Tx routine then schedule it here to guarantee
	 * completion processing of these packets */
	if (inflight && likely(ssk->qp_active))
		mod_timer(&ssk->tx_ring.timer, jiffies + SDP_TX_POLL_TIMEOUT);

out:
	if (ssk->tx_ring.rdma_inflight && ssk->tx_ring.rdma_inflight->busy) {
		sdp_prf1(sk, NULL, "RDMA is inflight - arming irq");
		sdp_arm_tx_cq(sk);
	}

	bh_unlock_sock(sk);
}

static void sdp_tx_irq(struct ib_cq *cq, void *cq_context)
{
	struct sock *sk = cq_context;
	struct sdp_sock *ssk = sdp_sk(sk);

	sdp_prf1(sk, NULL, "tx irq");
	sdp_dbg_data(sk, "Got tx comp interrupt\n");

	SDPSTATS_COUNTER_INC(tx_int_count);

	ssk->tx_compl_pending = 1;

	if (sdp_tx_handler_select(ssk) && likely(ssk->qp_active &&
				sk->sk_state != TCP_CLOSE)) {
		sdp_prf1(sk, NULL, "poll and post from tasklet");
		mod_timer(&ssk->tx_ring.timer, jiffies + SDP_TX_POLL_TIMEOUT);
		tasklet_schedule(&ssk->tx_ring.tasklet);
	}
}

static void sdp_tx_ring_purge(struct sdp_sock *ssk)
{
	while (ring_posted(ssk->tx_ring)) {
		struct sk_buff *skb;
		skb = sdp_send_completion(ssk, ring_tail(ssk->tx_ring));
		if (!skb)
			break;
		sdp_free_skb(skb);
	}
}

void sdp_post_keepalive(struct sdp_sock *ssk)
{
	int rc;
	struct ib_send_wr wr, *bad_wr;

	sdp_dbg(sk_ssk(ssk), "%s\n", __func__);

	memset(&wr, 0, sizeof(wr));

	wr.next    = NULL;
	wr.wr_id   = 0;
	wr.sg_list = NULL;
	wr.num_sge = 0;
	wr.opcode  = IB_WR_RDMA_WRITE;

	rc = ib_post_send(ssk->qp, &wr, &bad_wr);
	if (rc) {
		sdp_dbg(sk_ssk(ssk),
			"ib_post_keepalive failed with status %d.\n", rc);
		sdp_set_error(sk_ssk(ssk), -ECONNRESET);
	}

	sdp_cnt(sdp_keepalive_probes_sent);
}

static void sdp_tx_cq_event_handler(struct ib_event *event, void *data)
{
}

int sdp_tx_ring_create(struct sdp_sock *ssk, struct ib_device *device)
{
	struct ib_cq *tx_cq;
	int rc = 0;

	atomic_set(&ssk->tx_ring.head, 1);
	atomic_set(&ssk->tx_ring.tail, 1);

	ssk->tx_ring.buffer = kmalloc(
			sizeof *ssk->tx_ring.buffer * SDP_TX_SIZE, GFP_KERNEL);
	if (!ssk->tx_ring.buffer) {
		rc = -ENOMEM;
		sdp_warn(sk_ssk(ssk), "Can't allocate TX Ring size %zd.\n",
			 sizeof(*ssk->tx_ring.buffer) * SDP_TX_SIZE);

		goto out;
	}

	tx_cq = ib_create_cq(device, sdp_tx_irq, sdp_tx_cq_event_handler,
			  sk_ssk(ssk), SDP_TX_SIZE, IB_CQ_VECTOR_LEAST_ATTACHED);

	if (IS_ERR(tx_cq)) {
		rc = PTR_ERR(tx_cq);
		sdp_warn(sk_ssk(ssk), "Unable to allocate TX CQ: %d.\n", rc);
		goto err_cq;
	}

	ssk->tx_ring.cq = tx_cq;

	setup_timer(&ssk->tx_ring.timer, sdp_poll_tx_timeout,
			(unsigned long)ssk);
	ssk->tx_ring.poll_cnt = 0;

	tasklet_init(&ssk->tx_ring.tasklet, sdp_poll_tx_timeout,
			(unsigned long) ssk);

	setup_timer(&ssk->nagle_timer, sdp_nagle_timeout, (unsigned long) ssk);

	return 0;

err_cq:
	kfree(ssk->tx_ring.buffer);
	ssk->tx_ring.buffer = NULL;
out:
	return rc;
}

void sdp_tx_ring_destroy(struct sdp_sock *ssk)
{
	del_timer_sync(&ssk->tx_ring.timer);

	if (ssk->nagle_timer.function)
		del_timer_sync(&ssk->nagle_timer);

	if (ssk->tx_ring.buffer) {
		sdp_tx_ring_purge(ssk);

		kfree(ssk->tx_ring.buffer);
		ssk->tx_ring.buffer = NULL;
	}

	if (ssk->tx_ring.cq) {
		if (ib_destroy_cq(ssk->tx_ring.cq)) {
			sdp_warn(sk_ssk(ssk), "destroy cq(%p) failed\n",
					ssk->tx_ring.cq);
		} else {
			ssk->tx_ring.cq = NULL;
		}
	}

	tasklet_kill(&ssk->tx_ring.tasklet);
	/* tx_cq is destroyed, so no more tx_irq, so no one will schedule this
	 * tasklet. */

	SDP_WARN_ON(ring_head(ssk->tx_ring) != ring_tail(ssk->tx_ring));
}
