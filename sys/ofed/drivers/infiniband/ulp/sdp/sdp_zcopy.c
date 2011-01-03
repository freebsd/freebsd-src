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
 */
#include <linux/tcp.h>
#include <asm/ioctls.h>
#include <linux/workqueue.h>
#include <linux/net.h>
#include <linux/socket.h>
#include <net/protocol.h>
#include <net/inet_common.h>
#include <rdma/rdma_cm.h>
#include <rdma/ib_verbs.h>
#include <rdma/ib_fmr_pool.h>
#include <rdma/ib_umem.h>
#include <net/tcp.h> /* for memcpy_toiovec */
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include "sdp.h"

static int sdp_post_srcavail(struct sock *sk, struct tx_srcavail_state *tx_sa)
{
	struct sdp_sock *ssk = sdp_sk(sk);
	struct sk_buff *skb;
	int payload_len;
	struct page *payload_pg;
	int off, len;
	struct ib_umem_chunk *chunk;

	if (ssk->tx_sa) {
		/* ssk->tx_sa might already be there in a case of
		 * multithreading: user thread initiated Zcopy and went to
		 * sleep, and now another user thread tries to ZCopy.
		 * Fallback to BCopy - data might be mixed.
		 * TODO: Fix it. fallback to BCopy is not enough because recv
		 * side has seq warnings.
		 */
		sdp_dbg_data(sk, "user already initiated ZCopy transmission\n");
		return -EAGAIN;
	}

	BUG_ON(!tx_sa);
	BUG_ON(!tx_sa->fmr || !tx_sa->fmr->fmr->lkey);
	BUG_ON(!tx_sa->umem);
	BUG_ON(!tx_sa->umem->chunk_list.next);

	chunk = list_entry(tx_sa->umem->chunk_list.next, struct ib_umem_chunk, list);
	BUG_ON(!chunk->nmap);

	off = tx_sa->umem->offset;
	len = tx_sa->umem->length;

	tx_sa->bytes_sent = tx_sa->bytes_acked = 0;

	skb = sdp_alloc_skb_srcavail(sk, len, tx_sa->fmr->fmr->lkey, off, 0);
	if (!skb) {
		return -ENOMEM;
	}
	sdp_dbg_data(sk, "sending SrcAvail\n");

	TX_SRCAVAIL_STATE(skb) = tx_sa; /* tx_sa is hanged on the skb
					 * but continue to live after skb is freed */
	ssk->tx_sa = tx_sa;

	/* must have payload inlined in SrcAvail packet in combined mode */
	payload_len = MIN(tx_sa->umem->page_size - off, len);
	payload_len = MIN(payload_len, ssk->xmit_size_goal - sizeof(struct sdp_srcah));
	payload_pg  = sg_page(&chunk->page_list[0]);
	get_page(payload_pg);

	sdp_dbg_data(sk, "payload: off: 0x%x, pg: %p, len: 0x%x\n",
		off, payload_pg, payload_len);

	skb_fill_page_desc(skb, skb_shinfo(skb)->nr_frags,
			payload_pg, off, payload_len);
	/* Need to increase mem_usage counter even thought this page was not
	 * allocated.
	 * The reason is that when freeing this skb, we are decreasing the same
	 * counter according to nr_frags. we don't want to check h->mid since
	 * h->mid is not always a valid value.
	 */
	atomic_add(skb_shinfo(skb)->nr_frags, &sdp_current_mem_usage);

	skb->len             += payload_len;
	skb->data_len         = payload_len;
	skb->truesize        += payload_len;

	sdp_skb_entail(sk, skb);

	ssk->write_seq += payload_len;
	SDP_SKB_CB(skb)->end_seq += payload_len;

	tx_sa->bytes_sent = tx_sa->umem->length;
	tx_sa->bytes_acked = payload_len;

	/* TODO: pushing the skb into the tx_queue should be enough */

	return 0;
}

static int sdp_post_srcavail_cancel(struct sock *sk)
{
	struct sdp_sock *ssk = sdp_sk(sk);
	struct sk_buff *skb;

	sdp_dbg_data(sk_ssk(ssk), "Posting srcavail cancel\n");

	skb = sdp_alloc_skb_srcavail_cancel(sk, 0);
	if (unlikely(!skb))
		return -ENOMEM;

	sdp_skb_entail(sk, skb);

	sdp_post_sends(ssk, 0);

	return 0;
}

static int sdp_wait_rdmardcompl(struct sdp_sock *ssk, long *timeo_p,
		int ignore_signals)
{
	struct sock *sk = sk_ssk(ssk);
	int err = 0;
	long current_timeo = *timeo_p;
	struct tx_srcavail_state *tx_sa = ssk->tx_sa;
	DEFINE_WAIT(wait);

	sdp_dbg_data(sk, "sleep till RdmaRdCompl. timeo = %ld.\n", *timeo_p);
	sdp_prf1(sk, NULL, "Going to sleep");
	while (ssk->qp_active) {
		prepare_to_wait(sk->sk_sleep, &wait, TASK_INTERRUPTIBLE);

		if (unlikely(!*timeo_p)) {
			err = -ETIME;
			tx_sa->abort_flags |= TX_SA_TIMEDOUT;
			sdp_prf1(sk, NULL, "timeout");
			SDPSTATS_COUNTER_INC(zcopy_tx_timeout);
			break;
		}

		else if (tx_sa->bytes_acked > tx_sa->bytes_sent) {
			err = -EINVAL;
			sdp_dbg_data(sk, "acked bytes > sent bytes\n");
			tx_sa->abort_flags |= TX_SA_ERROR;
			break;
		}

		if (tx_sa->abort_flags & TX_SA_SENDSM) {
			sdp_prf1(sk, NULL, "Aborting SrcAvail sending");
			SDPSTATS_COUNTER_INC(zcopy_tx_aborted);
			err = -EAGAIN;
			break ;
		}

		if (!ignore_signals) {
			if (signal_pending(current)) {
				err = -EINTR;
				sdp_prf1(sk, NULL, "signalled");
				tx_sa->abort_flags |= TX_SA_INTRRUPTED;
				break;
			}

			if (ssk->rx_sa && (tx_sa->bytes_acked < tx_sa->bytes_sent)) {
				sdp_dbg_data(sk, "Crossing SrcAvail - aborting this\n");
				tx_sa->abort_flags |= TX_SA_CROSS_SEND;
				SDPSTATS_COUNTER_INC(zcopy_cross_send);
				err = -ETIME;
				break ;
			}
		}

		posts_handler_put(ssk, 0);

		sk_wait_event(sk, &current_timeo,
				tx_sa->abort_flags &&
				ssk->rx_sa &&
				(tx_sa->bytes_acked < tx_sa->bytes_sent));
		sdp_prf(sk_ssk(ssk), NULL, "woke up sleepers");

		posts_handler_get(ssk);

		if (tx_sa->bytes_acked == tx_sa->bytes_sent)
			break;

		*timeo_p = current_timeo;
	}

	finish_wait(sk->sk_sleep, &wait);

	sdp_dbg_data(sk, "Finished waiting - RdmaRdCompl: %d/%d bytes, flags: 0x%x\n",
			tx_sa->bytes_acked, tx_sa->bytes_sent, tx_sa->abort_flags);

	if (!ssk->qp_active) {
		sdp_dbg(sk, "QP destroyed while waiting\n");
		return -EINVAL;
	}
	return err;
}

static int sdp_wait_rdma_wr_finished(struct sdp_sock *ssk)
{
	struct sock *sk = sk_ssk(ssk);
	long timeo = SDP_RDMA_READ_TIMEOUT;
	int rc = 0;
	DEFINE_WAIT(wait);

	sdp_dbg_data(sk, "Sleep till RDMA wr finished.\n");
	while (1) {
		prepare_to_wait(sk->sk_sleep, &wait, TASK_UNINTERRUPTIBLE);

		if (!ssk->tx_ring.rdma_inflight->busy) {
			sdp_dbg_data(sk, "got rdma cqe\n");
			if (sk->sk_err == ECONNRESET)
				rc = -EPIPE;
			break;
		}

		if (!ssk->qp_active) {
			sdp_dbg_data(sk, "QP destroyed\n");
			rc = -EPIPE;
			break;
		}

		if (!timeo) {
			sdp_warn(sk, "Fatal: no RDMA read completion\n");
			rc = -EIO;
			sdp_set_error(sk, rc);
			break;
		}

		posts_handler_put(ssk, 0);

		sdp_prf1(sk, NULL, "Going to sleep");
		sk_wait_event(sk, &timeo,
			!ssk->tx_ring.rdma_inflight->busy ||
			!ssk->qp_active);
		sdp_prf1(sk, NULL, "Woke up");
		sdp_dbg_data(sk_ssk(ssk), "woke up sleepers\n");

		posts_handler_get(ssk);
	}

	finish_wait(sk->sk_sleep, &wait);

	sdp_dbg_data(sk, "Finished waiting\n");
	return rc;
}

int sdp_post_rdma_rd_compl(struct sock *sk, struct rx_srcavail_state *rx_sa)
{
	struct sk_buff *skb;
	int unreported = rx_sa->copied - rx_sa->reported;

	if (rx_sa->copied <= rx_sa->reported)
		return 0;

	skb = sdp_alloc_skb_rdmardcompl(sk, unreported, 0);
	if (unlikely(!skb))
		return -ENOMEM;

	sdp_skb_entail(sk, skb);

	rx_sa->reported += unreported;

	sdp_post_sends(sdp_sk(sk), 0);

	return 0;
}

int sdp_post_sendsm(struct sock *sk)
{
	struct sk_buff *skb = sdp_alloc_skb_sendsm(sk, 0);

	if (unlikely(!skb))
		return -ENOMEM;

	sdp_skb_entail(sk, skb);

	sdp_post_sends(sdp_sk(sk), 0);

	return 0;
}

static int sdp_update_iov_used(struct sock *sk, struct iovec *iov, int len)
{
	sdp_dbg_data(sk, "updating consumed 0x%x bytes from iov\n", len);
	while (len > 0) {
		if (iov->iov_len) {
			int copy = min_t(unsigned int, iov->iov_len, len);
			len -= copy;
			iov->iov_len -= copy;
			iov->iov_base += copy;
		}
		iov++;
	}

	return 0;
}

static inline int sge_bytes(struct ib_sge *sge, int sge_cnt)
{
	int bytes = 0;

	while (sge_cnt > 0) {
		bytes += sge->length;
		sge++;
		sge_cnt--;
	}

	return bytes;
}
void sdp_handle_sendsm(struct sdp_sock *ssk, u32 mseq_ack)
{
	struct sock *sk = sk_ssk(ssk);
	unsigned long flags;

	spin_lock_irqsave(&ssk->tx_sa_lock, flags);

	if (!ssk->tx_sa) {
		sdp_prf1(sk, NULL, "SendSM for cancelled/finished SrcAvail");
		goto out;
	}

	if (after(ssk->tx_sa->mseq, mseq_ack)) {
		sdp_dbg_data(sk, "SendSM arrived for old SrcAvail. "
			"SendSM mseq_ack: 0x%x, SrcAvail mseq: 0x%x\n",
			mseq_ack, ssk->tx_sa->mseq);
		goto out;
	}

	sdp_dbg_data(sk, "Got SendSM - aborting SrcAvail\n");

	ssk->tx_sa->abort_flags |= TX_SA_SENDSM;
	wake_up(sk->sk_sleep);
	sdp_dbg_data(sk, "woke up sleepers\n");

out:
	spin_unlock_irqrestore(&ssk->tx_sa_lock, flags);
}

void sdp_handle_rdma_read_compl(struct sdp_sock *ssk, u32 mseq_ack,
		u32 bytes_completed)
{
	struct sock *sk = sk_ssk(ssk);
	unsigned long flags;

	sdp_prf1(sk, NULL, "RdmaRdCompl ssk=%p tx_sa=%p", ssk, ssk->tx_sa);
	sdp_dbg_data(sk, "RdmaRdCompl ssk=%p tx_sa=%p\n", ssk, ssk->tx_sa);

	spin_lock_irqsave(&ssk->tx_sa_lock, flags);

	if (!ssk->tx_sa) {
		sdp_dbg_data(sk, "Got RdmaRdCompl for aborted SrcAvail\n");
		goto out;
	}

	if (after(ssk->tx_sa->mseq, mseq_ack)) {
		sdp_dbg_data(sk, "RdmaRdCompl arrived for old SrcAvail. "
			"SendSM mseq_ack: 0x%x, SrcAvail mseq: 0x%x\n",
			mseq_ack, ssk->tx_sa->mseq);
		goto out;
	}

	ssk->tx_sa->bytes_acked += bytes_completed;

	wake_up(sk->sk_sleep);
	sdp_dbg_data(sk, "woke up sleepers\n");

out:
	spin_unlock_irqrestore(&ssk->tx_sa_lock, flags);
}

static unsigned long sdp_get_max_memlockable_bytes(unsigned long offset)
{
	unsigned long avail;
	unsigned long lock_limit;

	if (capable(CAP_IPC_LOCK))
		return ULONG_MAX;

	lock_limit = current->signal->rlim[RLIMIT_MEMLOCK].rlim_cur;
	avail = lock_limit - (current->mm->locked_vm << PAGE_SHIFT);

	return avail < offset ? 0 : avail - offset;
}

static int sdp_alloc_fmr(struct sock *sk, void *uaddr, size_t len,
	struct ib_pool_fmr **_fmr, struct ib_umem **_umem, int access, int min_len)
{
	struct ib_pool_fmr *fmr;
	struct ib_umem *umem;
	struct ib_device *dev = sdp_sk(sk)->ib_device;
	u64 *pages;
	struct ib_umem_chunk *chunk;
	int n = 0, j, k;
	int rc = 0;
	unsigned long max_lockable_bytes;

	if (unlikely(len > SDP_MAX_RDMA_READ_LEN)) {
		sdp_dbg_data(sk, "len:0x%zx > FMR_SIZE: 0x%lx\n",
			len, SDP_MAX_RDMA_READ_LEN);
		len = SDP_MAX_RDMA_READ_LEN;
	}

	max_lockable_bytes = sdp_get_max_memlockable_bytes((unsigned long)uaddr & ~PAGE_MASK);
	if (unlikely(len > max_lockable_bytes)) {
		sdp_dbg_data(sk, "len:0x%zx > RLIMIT_MEMLOCK available: 0x%lx\n",
			len, max_lockable_bytes);
		len = max_lockable_bytes;
	}

	if (unlikely(len <= min_len))
		return -EAGAIN;

	sdp_dbg_data(sk, "user buf: %p, len:0x%zx max_lockable_bytes: 0x%lx\n",
			uaddr, len, max_lockable_bytes);

	umem = ib_umem_get(&sdp_sk(sk)->context, (unsigned long)uaddr, len,
		access, 0);

	if (IS_ERR(umem)) {
		rc = -EAGAIN;
		sdp_dbg_data(sk, "Error doing umem_get 0x%zx bytes: %ld\n", len, PTR_ERR(umem));
		sdp_dbg_data(sk, "RLIMIT_MEMLOCK: 0x%lx[cur] 0x%lx[max] CAP_IPC_LOCK: %d\n",
				current->signal->rlim[RLIMIT_MEMLOCK].rlim_cur,
				current->signal->rlim[RLIMIT_MEMLOCK].rlim_max,
				capable(CAP_IPC_LOCK));
		goto err_umem_get;
	}

	sdp_dbg_data(sk, "umem->offset = 0x%x, length = 0x%zx\n",
		umem->offset, umem->length);

	pages = (u64 *) __get_free_page(GFP_KERNEL);
	if (!pages) {
		rc = -ENOMEM;
		goto err_pages_alloc;
	}

	list_for_each_entry(chunk, &umem->chunk_list, list) {
		for (j = 0; j < chunk->nmap; ++j) {
			unsigned len2;
			len2 = ib_sg_dma_len(dev,
					&chunk->page_list[j]) >> PAGE_SHIFT;
			
			SDP_WARN_ON(len2 > len);
			len -= len2;

			for (k = 0; k < len2; ++k) {
				pages[n++] = ib_sg_dma_address(dev,
						&chunk->page_list[j]) +
					umem->page_size * k;
				BUG_ON(n >= SDP_FMR_SIZE);
			}
		}
	}

	fmr = ib_fmr_pool_map_phys(sdp_sk(sk)->sdp_dev->fmr_pool, pages, n, 0);
	if (IS_ERR(fmr)) {
		sdp_dbg_data(sk, "Error allocating fmr: %ld\n", PTR_ERR(fmr));
		SDPSTATS_COUNTER_INC(fmr_alloc_error);
		rc = PTR_ERR(fmr);
		goto err_fmr_alloc;
	}

	free_page((unsigned long) pages);

	*_umem = umem;
	*_fmr = fmr;

	return 0;

err_fmr_alloc:
	free_page((unsigned long) pages);

err_pages_alloc:
	ib_umem_release(umem);

err_umem_get:

	return rc;
}

static inline void sdp_free_fmr(struct sock *sk, struct ib_pool_fmr **_fmr,
		struct ib_umem **_umem)
{
	if (*_fmr) {
		ib_fmr_pool_unmap(*_fmr);
		*_fmr = NULL;
	}

	if (*_umem) {
		ib_umem_release(*_umem);
		*_umem = NULL;
	}
}

static int sdp_post_rdma_read(struct sock *sk, struct rx_srcavail_state *rx_sa,
		u32 offset)
{
	struct sdp_sock *ssk = sdp_sk(sk);
	struct ib_send_wr *bad_wr;
	struct ib_send_wr wr = { NULL };
	struct ib_sge sge;
	int rc;

	wr.opcode = IB_WR_RDMA_READ;
	wr.next = NULL;
	wr.wr_id = SDP_OP_RDMA;
	wr.wr.rdma.rkey = rx_sa->rkey;
	wr.send_flags = 0;

	ssk->tx_ring.rdma_inflight = rx_sa;

	sge.addr = rx_sa->umem->offset;
	sge.length = rx_sa->umem->length;
	sge.lkey = rx_sa->fmr->fmr->lkey;

	wr.wr.rdma.remote_addr = rx_sa->vaddr + offset;
	wr.num_sge = 1;
	wr.sg_list = &sge;
	rx_sa->busy++;

	wr.send_flags = IB_SEND_SIGNALED;

	rc = ib_post_send(ssk->qp, &wr, &bad_wr);
	if (unlikely(rc)) {
		rx_sa->busy--;
		ssk->tx_ring.rdma_inflight = NULL;
	}

	return rc;
}

int sdp_rdma_to_iovec(struct sock *sk, struct iovec *iov, int msg_iovlen,
	       	struct sk_buff *skb, unsigned long *used, u32 offset)
{
	struct sdp_sock *ssk = sdp_sk(sk);
	struct rx_srcavail_state *rx_sa = RX_SRCAVAIL_STATE(skb);
	int rc = 0;
	int len = *used;
	int copied;
	int i = 0;

	if (unlikely(!ssk->ib_device))
		return -ENODEV;

	while (!iov->iov_len) {
		++iov;
		i++;
	}
	WARN_ON(i >= msg_iovlen);

	sdp_dbg_data(sk_ssk(ssk), "preparing RDMA read."
		" len: 0x%x. buffer len: 0x%zx\n", len, iov->iov_len);

	sock_hold(sk, SOCK_REF_RDMA_RD);

	if (len > rx_sa->len) {
		sdp_warn(sk, "len:0x%x > rx_sa->len: 0x%x\n", len, rx_sa->len);
		SDP_WARN_ON(1);
		len = rx_sa->len;
	}

	rc = sdp_alloc_fmr(sk, iov->iov_base, len, &rx_sa->fmr, &rx_sa->umem,
			IB_ACCESS_LOCAL_WRITE, 0);
	if (rc) {
		sdp_dbg_data(sk, "Error allocating fmr: %d\n", rc);
		goto err_alloc_fmr;
	}

	rc = sdp_post_rdma_read(sk, rx_sa, offset);
	if (unlikely(rc)) {
		sdp_warn(sk, "ib_post_send failed with status %d.\n", rc);
		sdp_set_error(sk_ssk(ssk), -ECONNRESET);
		goto err_post_send;
	}

	sdp_prf(sk, skb, "Finished posting, now to wait");
	sdp_arm_tx_cq(sk);

	rc = sdp_wait_rdma_wr_finished(ssk);
	if (unlikely(rc))
		goto err_wait;

	copied = rx_sa->umem->length;

	sdp_update_iov_used(sk, iov, copied);
	atomic_add(copied, &ssk->rcv_nxt);
	*used = copied;
	rx_sa->copied += copied;

err_wait:
	ssk->tx_ring.rdma_inflight = NULL;

err_post_send:
	sdp_free_fmr(sk, &rx_sa->fmr, &rx_sa->umem);

err_alloc_fmr:
	sock_put(sk, SOCK_REF_RDMA_RD);

	return rc;
}

static inline int wait_for_sndbuf(struct sock *sk, long *timeo_p)
{
	struct sdp_sock *ssk = sdp_sk(sk);
	int ret = 0;
	int credits_needed = 1;

	sdp_dbg_data(sk, "Wait for mem\n");

	set_bit(SOCK_NOSPACE, &sk->sk_socket->flags);

	SDPSTATS_COUNTER_INC(send_wait_for_mem);

	sdp_do_posts(ssk);

	if (sdp_xmit_poll(ssk, 1))
		sdp_post_sends(ssk, 0);

	ret = sdp_tx_wait_memory(ssk, timeo_p, &credits_needed);

	return ret;
}

static int do_sdp_sendmsg_zcopy(struct sock *sk, struct tx_srcavail_state *tx_sa,
		struct iovec *iov, long *timeo)
{
	struct sdp_sock *ssk = sdp_sk(sk);
	int rc = 0;
	unsigned long lock_flags;

	rc = sdp_alloc_fmr(sk, iov->iov_base, iov->iov_len,
			&tx_sa->fmr, &tx_sa->umem, IB_ACCESS_REMOTE_READ, sdp_zcopy_thresh);
	if (unlikely(rc)) {
		sdp_dbg_data(sk, "Error allocating fmr: %d\n", rc);
		goto err_alloc_fmr;
	}

	if (tx_slots_free(ssk) == 0) {
		rc = wait_for_sndbuf(sk, timeo);
		if (unlikely(rc)) {
			sdp_warn(sk, "Couldn't get send buffer\n");
			goto err_no_tx_slots;
		}
	}

	rc = sdp_post_srcavail(sk, tx_sa);
	if (unlikely(rc)) {
		sdp_dbg(sk, "Error posting SrcAvail: %d\n", rc);
		goto err_abort_send;
	}

	rc = sdp_wait_rdmardcompl(ssk, timeo, 0);
	if (unlikely(rc)) {
		enum tx_sa_flag f = tx_sa->abort_flags;

		if (f & TX_SA_SENDSM) {
			sdp_dbg_data(sk, "Got SendSM. use SEND verb.\n");
		} else if (f & TX_SA_ERROR) {
			sdp_dbg_data(sk, "SrcAvail error completion\n");
			sdp_reset(sk);
			SDPSTATS_COUNTER_INC(zcopy_tx_error);
		} else if (ssk->qp_active) {
			sdp_post_srcavail_cancel(sk);

			/* Wait for RdmaRdCompl/SendSM to
			 * finish the transaction */
			*timeo = SDP_SRCAVAIL_CANCEL_TIMEOUT;
			rc = sdp_wait_rdmardcompl(ssk, timeo, 1);
			if (unlikely(rc == -ETIME || rc == -EINVAL)) {
				/* didn't get RdmaRdCompl/SendSM after sending
				 * SrcAvailCancel - There is a connection
				 * problem. */
				sdp_reset(sk);
				rc = -sk->sk_err;
			}
		} else {
			sdp_dbg_data(sk, "QP was destroyed while waiting\n");
		}
	} else {
		sdp_dbg_data(sk, "got RdmaRdCompl\n");
	}

	spin_lock_irqsave(&ssk->tx_sa_lock, lock_flags);
	ssk->tx_sa = NULL;
	spin_unlock_irqrestore(&ssk->tx_sa_lock, lock_flags);

err_abort_send:
	sdp_update_iov_used(sk, iov, tx_sa->bytes_acked);

err_no_tx_slots:
	sdp_free_fmr(sk, &tx_sa->fmr, &tx_sa->umem);

err_alloc_fmr:
	return rc;
}

int sdp_sendmsg_zcopy(struct kiocb *iocb, struct sock *sk, struct iovec *iov)
{
	struct sdp_sock *ssk = sdp_sk(sk);
	int rc = 0;
	long timeo = SDP_SRCAVAIL_ADV_TIMEOUT;
	struct tx_srcavail_state *tx_sa;
	size_t bytes_to_copy = iov->iov_len;
	int copied = 0;

	sdp_dbg_data(sk, "Sending ZCopy iov: %p, iov_len: 0x%zx\n",
			iov->iov_base, iov->iov_len);
	if (ssk->rx_sa) {
		/* Don't want both sides to send SrcAvail because both of them
		 * will wait on sendmsg() until timeout.
		 */
		sdp_dbg_data(sk, "Deadlock prevent: crossing SrcAvail\n");
		return 0;
	}

	sock_hold(sk_ssk(ssk), SOCK_REF_ZCOPY);
	SDPSTATS_COUNTER_INC(sendmsg_zcopy_segment);

	/* Ok commence sending. */

	tx_sa = kmalloc(sizeof(struct tx_srcavail_state), GFP_KERNEL);
	if (!tx_sa) {
		sdp_warn(sk, "Error allocating zcopy context\n");
		rc = -EAGAIN; /* Buffer too big - fallback to bcopy */
		goto err_alloc_tx_sa;
	}

	do {
		tx_sa_reset(tx_sa);

		rc = do_sdp_sendmsg_zcopy(sk, tx_sa, iov, &timeo);

		if (iov->iov_len && iov->iov_len < sdp_zcopy_thresh) {
			sdp_dbg_data(sk, "0x%zx bytes left, switching to bcopy\n",
				iov->iov_len);
			break;
		}
	} while (!rc && iov->iov_len > 0 && !tx_sa->abort_flags);

	kfree(tx_sa);
err_alloc_tx_sa:
	copied = bytes_to_copy - iov->iov_len;

	sdp_prf1(sk, NULL, "sdp_sendmsg_zcopy end rc: %d copied: %d", rc, copied);

	sock_put(sk_ssk(ssk), SOCK_REF_ZCOPY);

	if (rc < 0 && rc != -EAGAIN && rc != -ETIME)
		return rc;

	return copied;
}

void sdp_abort_srcavail(struct sock *sk)
{
	struct sdp_sock *ssk = sdp_sk(sk);
	struct tx_srcavail_state *tx_sa = ssk->tx_sa;
	unsigned long flags;

	if (!tx_sa)
		return;

	spin_lock_irqsave(&ssk->tx_sa_lock, flags);

	sdp_free_fmr(sk, &tx_sa->fmr, &tx_sa->umem);

	ssk->tx_sa = NULL;

	spin_unlock_irqrestore(&ssk->tx_sa_lock, flags);
}

void sdp_abort_rdma_read(struct sock *sk)
{
	struct sdp_sock *ssk = sdp_sk(sk);
	struct rx_srcavail_state *rx_sa;

	rx_sa = ssk->rx_sa;
	if (!rx_sa)
		return;

	sdp_free_fmr(sk, &rx_sa->fmr, &rx_sa->umem);

	/* kfree(rx_sa) and posting SendSM will be handled in the nornal
	 * flows.
	 */
}
