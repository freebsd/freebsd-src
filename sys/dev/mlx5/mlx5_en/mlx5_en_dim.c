/*-
 * Copyright (c) 2018 Mellanox Technologies. All rights reserved.
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
 *
 * $FreeBSD$
 */

#include "en.h"

void
mlx5e_dim_build_cq_param(struct mlx5e_priv *priv,
    struct mlx5e_cq_param *param)
{
	struct net_dim_cq_moder prof;
	void *cqc = param->cqc;

	if (priv->params.rx_cq_moderation_mode < 2)
		return;

	switch (MLX5_GET(cqc, cqc, cq_period_mode)) {
	case MLX5_CQ_PERIOD_MODE_START_FROM_CQE:
		prof = net_dim_profile[NET_DIM_CQ_PERIOD_MODE_START_FROM_CQE]
		    [NET_DIM_DEF_PROFILE_CQE];
		MLX5_SET(cqc, cqc, cq_period, prof.usec);
		MLX5_SET(cqc, cqc, cq_max_count, prof.pkts);
		break;

	case MLX5_CQ_PERIOD_MODE_START_FROM_EQE:
		prof = net_dim_profile[NET_DIM_CQ_PERIOD_MODE_START_FROM_EQE]
		    [NET_DIM_DEF_PROFILE_EQE];
		MLX5_SET(cqc, cqc, cq_period, prof.usec);
		MLX5_SET(cqc, cqc, cq_max_count, prof.pkts);
		break;
	default:
		break;
	}
}

void
mlx5e_dim_work(struct work_struct *work)
{
	struct net_dim *dim = container_of(work, struct net_dim, work);
	struct mlx5e_rq *rq = container_of(dim, struct mlx5e_rq, dim);
	struct mlx5e_channel *c = container_of(rq, struct mlx5e_channel, rq);
	struct net_dim_cq_moder cur_profile;
	u8 profile_ix;
	u8 mode;

	/* copy current auto moderation settings and set new state */
	mtx_lock(&rq->mtx);
	profile_ix = dim->profile_ix;
	mode = dim->mode;
	dim->state = NET_DIM_START_MEASURE;
	mtx_unlock(&rq->mtx);

	/* check for invalid mode */
	if (mode == 255)
		return;

	/* get current profile */
	cur_profile = net_dim_profile[mode][profile_ix];

	/* apply LRO restrictions */
	if (c->priv->params.hw_lro_en &&
	    cur_profile.pkts > MLX5E_DIM_MAX_RX_CQ_MODERATION_PKTS_WITH_LRO) {
		cur_profile.pkts = MLX5E_DIM_MAX_RX_CQ_MODERATION_PKTS_WITH_LRO;
	}

	/* modify CQ */
	mlx5_core_modify_cq_moderation(c->priv->mdev, &rq->cq.mcq,
	    cur_profile.usec, cur_profile.pkts);
}
