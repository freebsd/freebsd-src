/*-
 * Copyright (c) 2015 Mellanox Technologies. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS `AS IS' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "opt_rss.h"
#include "opt_ratelimit.h"

#include <linux/printk.h>

#include <dev/mlx5/mlx5_en/en.h>

struct mlx5_cqe64 *
mlx5e_get_cqe(struct mlx5e_cq *cq)
{
	struct mlx5_cqe64 *cqe;

	cqe = mlx5_cqwq_get_wqe(&cq->wq, mlx5_cqwq_get_ci(&cq->wq));

	if ((cqe->op_own ^ mlx5_cqwq_get_wrap_cnt(&cq->wq)) & MLX5_CQE_OWNER_MASK)
		return (NULL);

	/* ensure cqe content is read after cqe ownership bit */
	atomic_thread_fence_acq();

	return (cqe);
}

void
mlx5e_cq_error_event(struct mlx5_core_cq *mcq, int event)
{
	struct mlx5e_cq *cq = container_of(mcq, struct mlx5e_cq, mcq);

	mlx5_en_err(cq->priv->ifp, "cqn=0x%.6x event=0x%.2x\n",
	    mcq->cqn, event);
}

void
mlx5e_dump_err_cqe(struct mlx5e_cq *cq, u32 qn, const struct mlx5_err_cqe *err_cqe)
{
	u32 ci;

	/* Don't print flushed in error syndromes. */
	if (err_cqe->vendor_err_synd == 0xf9 && err_cqe->syndrome == 0x05)
		return;
	/* Don't print when the queue is set to error state by software. */
	if (err_cqe->vendor_err_synd == 0xf5 && err_cqe->syndrome == 0x05)
		return;

	ci = (cq->wq.cc - 1) & cq->wq.sz_m1;

	mlx5_en_err(cq->priv->ifp,
	    "Error CQE on CQN 0x%x, CI 0x%x, QN 0x%x, OPCODE 0x%x, SYNDROME 0x%x, VENDOR SYNDROME 0x%x\n",
	    cq->mcq.cqn, ci, qn, err_cqe->op_own >> 4,
	    err_cqe->syndrome, err_cqe->vendor_err_synd);

	print_hex_dump(NULL, NULL, DUMP_PREFIX_OFFSET,
	    16, 1, err_cqe, sizeof(*err_cqe), false);
}
