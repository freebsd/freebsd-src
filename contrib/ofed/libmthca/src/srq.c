/*
 * Copyright (c) 2005 Cisco Systems.  All rights reserved.
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

#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>

#include "mthca.h"
#include "doorbell.h"
#include "wqe.h"

static void *get_wqe(struct mthca_srq *srq, int n)
{
	return srq->buf.buf + (n << srq->wqe_shift);
}

/*
 * Return a pointer to the location within a WQE that we're using as a
 * link when the WQE is in the free list.  We use the imm field at an
 * offset of 12 bytes because in the Tavor case, posting a WQE may
 * overwrite the next segment of the previous WQE, but a receive WQE
 * will never touch the imm field.  This avoids corrupting our free
 * list if the previous WQE has already completed and been put on the
 * free list when we post the next WQE.
 */
static inline int *wqe_to_link(void *wqe)
{
	return (int *) (wqe + 12);
}

void mthca_free_srq_wqe(struct mthca_srq *srq, int ind)
{
	struct mthca_next_seg *last_free;

	pthread_spin_lock(&srq->lock);

	last_free = get_wqe(srq, srq->last_free);
	*wqe_to_link(last_free) = ind;
	last_free->nda_op = htonl((ind << srq->wqe_shift) | 1);
	*wqe_to_link(get_wqe(srq, ind)) = -1;
	srq->last_free = ind;

	pthread_spin_unlock(&srq->lock);
}

int mthca_tavor_post_srq_recv(struct ibv_srq *ibsrq,
			      struct ibv_recv_wr *wr,
			      struct ibv_recv_wr **bad_wr)
{
	struct mthca_srq *srq = to_msrq(ibsrq);
	uint32_t doorbell[2];
	int err = 0;
	int first_ind;
	int ind;
	int next_ind;
	int nreq;
	int i;
	void *wqe;
	void *prev_wqe;

	pthread_spin_lock(&srq->lock);

	first_ind = srq->first_free;

	for (nreq = 0; wr; wr = wr->next) {
		ind	  = srq->first_free;
		wqe       = get_wqe(srq, ind);
		next_ind  = *wqe_to_link(wqe);

		if (next_ind < 0) {
			err = -1;
			*bad_wr = wr;
			break;
		}

		prev_wqe  = srq->last;
		srq->last = wqe;

		((struct mthca_next_seg *) wqe)->ee_nds = 0;
		/* flags field will always remain 0 */

		wqe += sizeof (struct mthca_next_seg);

		if (wr->num_sge > srq->max_gs) {
			err = -1;
			*bad_wr = wr;
			srq->last = prev_wqe;
			break;
		}

		for (i = 0; i < wr->num_sge; ++i) {
			((struct mthca_data_seg *) wqe)->byte_count =
				htonl(wr->sg_list[i].length);
			((struct mthca_data_seg *) wqe)->lkey =
				htonl(wr->sg_list[i].lkey);
			((struct mthca_data_seg *) wqe)->addr =
				htonll(wr->sg_list[i].addr);
			wqe += sizeof (struct mthca_data_seg);
		}

		if (i < srq->max_gs) {
			((struct mthca_data_seg *) wqe)->byte_count = 0;
			((struct mthca_data_seg *) wqe)->lkey = htonl(MTHCA_INVAL_LKEY);
			((struct mthca_data_seg *) wqe)->addr = 0;
		}

		((struct mthca_next_seg *) prev_wqe)->ee_nds =
			htonl(MTHCA_NEXT_DBD);

		srq->wrid[ind]  = wr->wr_id;
		srq->first_free = next_ind;

		if (++nreq == MTHCA_TAVOR_MAX_WQES_PER_RECV_DB) {
			nreq = 0;

			doorbell[0] = htonl(first_ind << srq->wqe_shift);
			doorbell[1] = htonl(srq->srqn << 8);

			/*
			 * Make sure that descriptors are written
			 * before doorbell is rung.
			 */
			wmb();

			mthca_write64(doorbell, to_mctx(ibsrq->context), MTHCA_RECV_DOORBELL);

			first_ind = srq->first_free;
		}
	}

	if (nreq) {
		doorbell[0] = htonl(first_ind << srq->wqe_shift);
		doorbell[1] = htonl((srq->srqn << 8) | nreq);

		/*
		 * Make sure that descriptors are written before
		 * doorbell is rung.
		 */
		wmb();

		mthca_write64(doorbell, to_mctx(ibsrq->context), MTHCA_RECV_DOORBELL);
	}

	pthread_spin_unlock(&srq->lock);
	return err;
}

int mthca_arbel_post_srq_recv(struct ibv_srq *ibsrq,
			      struct ibv_recv_wr *wr,
			      struct ibv_recv_wr **bad_wr)
{
	struct mthca_srq *srq = to_msrq(ibsrq);
	int err = 0;
	int ind;
	int next_ind;
	int nreq;
	int i;
	void *wqe;

	pthread_spin_lock(&srq->lock);

	for (nreq = 0; wr; ++nreq, wr = wr->next) {
		ind	  = srq->first_free;
		wqe       = get_wqe(srq, ind);
		next_ind  = *wqe_to_link(wqe);

		if (next_ind < 0) {
			err = -1;
			*bad_wr = wr;
			break;
		}

		((struct mthca_next_seg *) wqe)->ee_nds = 0;
		/* flags field will always remain 0 */

		wqe += sizeof (struct mthca_next_seg);

		if (wr->num_sge > srq->max_gs) {
			err = -1;
			*bad_wr = wr;
			break;
		}

		for (i = 0; i < wr->num_sge; ++i) {
			((struct mthca_data_seg *) wqe)->byte_count =
				htonl(wr->sg_list[i].length);
			((struct mthca_data_seg *) wqe)->lkey =
				htonl(wr->sg_list[i].lkey);
			((struct mthca_data_seg *) wqe)->addr =
				htonll(wr->sg_list[i].addr);
			wqe += sizeof (struct mthca_data_seg);
		}

		if (i < srq->max_gs) {
			((struct mthca_data_seg *) wqe)->byte_count = 0;
			((struct mthca_data_seg *) wqe)->lkey = htonl(MTHCA_INVAL_LKEY);
			((struct mthca_data_seg *) wqe)->addr = 0;
		}

		srq->wrid[ind]  = wr->wr_id;
		srq->first_free = next_ind;
	}

	if (nreq) {
		srq->counter += nreq;

		/*
		 * Make sure that descriptors are written before
		 * we write doorbell record.
		 */
		wmb();
		*srq->db = htonl(srq->counter);
	}

	pthread_spin_unlock(&srq->lock);
	return err;
}

int mthca_alloc_srq_buf(struct ibv_pd *pd, struct ibv_srq_attr *attr,
		       struct mthca_srq *srq)
{
	struct mthca_data_seg *scatter;
	void *wqe;
	int size;
	int i;

	srq->wrid = malloc(srq->max * sizeof (uint64_t));
	if (!srq->wrid)
		return -1;

	size = sizeof (struct mthca_next_seg) +
		srq->max_gs * sizeof (struct mthca_data_seg);

	for (srq->wqe_shift = 6; 1 << srq->wqe_shift < size; ++srq->wqe_shift)
		; /* nothing */

	srq->buf_size = srq->max << srq->wqe_shift;

	if (mthca_alloc_buf(&srq->buf,
			    align(srq->buf_size, to_mdev(pd->context->device)->page_size),
			    to_mdev(pd->context->device)->page_size)) {
		free(srq->wrid);
		return -1;
	}

	memset(srq->buf.buf, 0, srq->buf_size);

	/*
	 * Now initialize the SRQ buffer so that all of the WQEs are
	 * linked into the list of free WQEs.  In addition, set the
	 * scatter list L_Keys to the sentry value of 0x100.
	 */

	for (i = 0; i < srq->max; ++i) {
		struct mthca_next_seg *next;

		next = wqe = get_wqe(srq, i);

		if (i < srq->max - 1) {
			*wqe_to_link(wqe) = i + 1;
			next->nda_op = htonl(((i + 1) << srq->wqe_shift) | 1);
		} else {
			*wqe_to_link(wqe) = -1;
			next->nda_op = 0;
		}

		for (scatter = wqe + sizeof (struct mthca_next_seg);
		     (void *) scatter < wqe + (1 << srq->wqe_shift);
		     ++scatter)
			scatter->lkey = htonl(MTHCA_INVAL_LKEY);
	}

	srq->first_free = 0;
	srq->last_free  = srq->max - 1;
	srq->last       = get_wqe(srq, srq->max - 1);

	return 0;
}
