/*
 * Copyright (c) 2007 Cisco, Inc.  All rights reserved.
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

#include "mlx4.h"
#include "doorbell.h"
#include "wqe.h"

static void *get_wqe(struct mlx4_srq *srq, int n)
{
	return srq->buf.buf + (n << srq->wqe_shift);
}

void mlx4_free_srq_wqe(struct mlx4_srq *srq, int ind)
{
	struct mlx4_wqe_srq_next_seg *next;

	pthread_spin_lock(&srq->lock);

	next = get_wqe(srq, srq->tail);
	next->next_wqe_index = htons(ind);
	srq->tail = ind;

	pthread_spin_unlock(&srq->lock);
}

int mlx4_post_srq_recv(struct ibv_srq *ibsrq,
		       struct ibv_recv_wr *wr,
		       struct ibv_recv_wr **bad_wr)
{
	struct mlx4_srq *srq = to_msrq(ibsrq);
	struct mlx4_wqe_srq_next_seg *next;
	struct mlx4_wqe_data_seg *scat;
	int err = 0;
	int nreq;
	int i;

	pthread_spin_lock(&srq->lock);

	for (nreq = 0; wr; ++nreq, wr = wr->next) {
		if (wr->num_sge > srq->max_gs) {
			err = -1;
			*bad_wr = wr;
			break;
		}

		if (srq->head == srq->tail) {
			/* SRQ is full*/
			err = -1;
			*bad_wr = wr;
			break;
		}

		srq->wrid[srq->head] = wr->wr_id;

		next      = get_wqe(srq, srq->head);
		srq->head = ntohs(next->next_wqe_index);
		scat      = (struct mlx4_wqe_data_seg *) (next + 1);

		for (i = 0; i < wr->num_sge; ++i) {
			scat[i].byte_count = htonl(wr->sg_list[i].length);
			scat[i].lkey       = htonl(wr->sg_list[i].lkey);
			scat[i].addr       = htonll(wr->sg_list[i].addr);
		}

		if (i < srq->max_gs) {
			scat[i].byte_count = 0;
			scat[i].lkey       = htonl(MLX4_INVALID_LKEY);
			scat[i].addr       = 0;
		}
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

int mlx4_alloc_srq_buf(struct ibv_pd *pd, struct ibv_srq_attr *attr,
		       struct mlx4_srq *srq)
{
	struct mlx4_wqe_srq_next_seg *next;
	struct mlx4_wqe_data_seg *scatter;
	int size;
	int buf_size;
	int i;

	srq->wrid = malloc(srq->max * sizeof (uint64_t));
	if (!srq->wrid)
		return -1;

	size = sizeof (struct mlx4_wqe_srq_next_seg) +
		srq->max_gs * sizeof (struct mlx4_wqe_data_seg);

	for (srq->wqe_shift = 5; 1 << srq->wqe_shift < size; ++srq->wqe_shift)
		; /* nothing */

	buf_size = srq->max << srq->wqe_shift;

	if (mlx4_alloc_buf(&srq->buf, buf_size,
			   to_mdev(pd->context->device)->page_size)) {
		free(srq->wrid);
		return -1;
	}

	memset(srq->buf.buf, 0, buf_size);

	/*
	 * Now initialize the SRQ buffer so that all of the WQEs are
	 * linked into the list of free WQEs.
	 */

	for (i = 0; i < srq->max; ++i) {
		next = get_wqe(srq, i);
		next->next_wqe_index = htons((i + 1) & (srq->max - 1));

		for (scatter = (void *) (next + 1);
		     (void *) scatter < (void *) next + (1 << srq->wqe_shift);
		     ++scatter)
			scatter->lkey = htonl(MLX4_INVALID_LKEY);
	}

	srq->head = 0;
	srq->tail = srq->max - 1;

	return 0;
}

struct mlx4_srq *mlx4_find_xrc_srq(struct mlx4_context *ctx, uint32_t xrc_srqn)
{
	int tind = (xrc_srqn & (ctx->num_xrc_srqs - 1)) >> ctx->xrc_srq_table_shift;

	if (ctx->xrc_srq_table[tind].refcnt)
		return ctx->xrc_srq_table[tind].table[xrc_srqn & ctx->xrc_srq_table_mask];
	else
		return NULL;
}

int mlx4_store_xrc_srq(struct mlx4_context *ctx, uint32_t xrc_srqn,
		       struct mlx4_srq *srq)
{
	int tind = (xrc_srqn & (ctx->num_xrc_srqs - 1)) >> ctx->xrc_srq_table_shift;
	int ret = 0;

	pthread_mutex_lock(&ctx->xrc_srq_table_mutex);

	if (!ctx->xrc_srq_table[tind].refcnt) {
		ctx->xrc_srq_table[tind].table = calloc(ctx->xrc_srq_table_mask + 1,
							sizeof(struct mlx4_srq *));
		if (!ctx->xrc_srq_table[tind].table) {
			ret = -1;
			goto out;
		}
	}

	++ctx->xrc_srq_table[tind].refcnt;
	ctx->xrc_srq_table[tind].table[xrc_srqn & ctx->xrc_srq_table_mask] = srq;

out:
	pthread_mutex_unlock(&ctx->xrc_srq_table_mutex);
	return ret;
}

void mlx4_clear_xrc_srq(struct mlx4_context *ctx, uint32_t xrc_srqn)
{
	int tind = (xrc_srqn & (ctx->num_xrc_srqs - 1)) >> ctx->xrc_srq_table_shift;

	pthread_mutex_lock(&ctx->xrc_srq_table_mutex);

	if (!--ctx->xrc_srq_table[tind].refcnt)
		free(ctx->xrc_srq_table[tind].table);
	else
		ctx->xrc_srq_table[tind].table[xrc_srqn & ctx->xrc_srq_table_mask] = NULL;

	pthread_mutex_unlock(&ctx->xrc_srq_table_mutex);
}

