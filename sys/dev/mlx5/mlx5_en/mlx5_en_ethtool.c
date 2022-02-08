/*-
 * Copyright (c) 2015-2021 Mellanox Technologies. All rights reserved.
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

#include "opt_rss.h"
#include "opt_ratelimit.h"

#include <dev/mlx5/mlx5_en/en.h>
#include <dev/mlx5/mlx5_en/port_buffer.h>

void
mlx5e_create_stats(struct sysctl_ctx_list *ctx,
    struct sysctl_oid_list *parent, const char *buffer,
    const char **desc, unsigned num, u64 * arg)
{
	struct sysctl_oid *node;
	unsigned x;

	sysctl_ctx_init(ctx);

	node = SYSCTL_ADD_NODE(ctx, parent, OID_AUTO,
	    buffer, CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "Statistics");
	if (node == NULL)
		return;
	for (x = 0; x != num; x++) {
		SYSCTL_ADD_UQUAD(ctx, SYSCTL_CHILDREN(node), OID_AUTO,
		    desc[2 * x], CTLFLAG_RD, arg + x, desc[2 * x + 1]);
	}
}

void
mlx5e_create_counter_stats(struct sysctl_ctx_list *ctx,
    struct sysctl_oid_list *parent, const char *buffer,
    const char **desc, unsigned num, counter_u64_t *arg)
{
	struct sysctl_oid *node;
	unsigned x;

	sysctl_ctx_init(ctx);

	node = SYSCTL_ADD_NODE(ctx, parent, OID_AUTO,
	    buffer, CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "Statistics");
	if (node == NULL)
		return;
	for (x = 0; x != num; x++) {
		SYSCTL_ADD_COUNTER_U64(ctx, SYSCTL_CHILDREN(node), OID_AUTO,
		    desc[2 * x], CTLFLAG_RD, arg + x, desc[2 * x + 1]);
	}
}

static void
mlx5e_ethtool_sync_tx_completion_fact(struct mlx5e_priv *priv)
{
	/*
	 * Limit the maximum distance between completion events to
	 * half of the currently set TX queue size.
	 *
	 * The maximum number of queue entries a single IP packet can
	 * consume is given by MLX5_SEND_WQE_MAX_WQEBBS.
	 *
	 * The worst case max value is then given as below:
	 */
	uint64_t max = priv->params_ethtool.tx_queue_size /
	    (2 * MLX5_SEND_WQE_MAX_WQEBBS);

	/*
	 * Update the maximum completion factor value in case the
	 * tx_queue_size field changed. Ensure we don't overflow
	 * 16-bits.
	 */
	if (max < 1)
		max = 1;
	else if (max > 65535)
		max = 65535;
	priv->params_ethtool.tx_completion_fact_max = max;

	/*
	 * Verify that the current TX completion factor is within the
	 * given limits:
	 */
	if (priv->params_ethtool.tx_completion_fact < 1)
		priv->params_ethtool.tx_completion_fact = 1;
	else if (priv->params_ethtool.tx_completion_fact > max)
		priv->params_ethtool.tx_completion_fact = max;
}

static int
mlx5e_getmaxrate(struct mlx5e_priv *priv)
{
	struct mlx5_core_dev *mdev = priv->mdev;
	u8 max_bw_unit[IEEE_8021QAZ_MAX_TCS];
	u8 max_bw_value[IEEE_8021QAZ_MAX_TCS];
	int err;
	int i;

	PRIV_LOCK(priv);
	err = -mlx5_query_port_tc_rate_limit(mdev, max_bw_value, max_bw_unit);
	if (err)
		goto done;

	for (i = 0; i <= mlx5_max_tc(mdev); i++) {
		switch (max_bw_unit[i]) {
		case MLX5_100_MBPS_UNIT:
			priv->params_ethtool.max_bw_value[i] = max_bw_value[i] * MLX5E_100MB;
			break;
		case MLX5_GBPS_UNIT:
			priv->params_ethtool.max_bw_value[i] = max_bw_value[i] * MLX5E_1GB;
			break;
		case MLX5_BW_NO_LIMIT:
			priv->params_ethtool.max_bw_value[i] = 0;
			break;
		default:
			priv->params_ethtool.max_bw_value[i] = -1;
			WARN_ONCE(true, "non-supported BW unit");
			break;
		}
	}
done:
	PRIV_UNLOCK(priv);
	return (err);
}

static int
mlx5e_get_max_alloc(struct mlx5e_priv *priv)
{
	struct mlx5_core_dev *mdev = priv->mdev;
	int err;
	int x;

	PRIV_LOCK(priv);
	err = -mlx5_query_port_tc_bw_alloc(mdev, priv->params_ethtool.max_bw_share);
	if (err == 0) {
		/* set default value */
		for (x = 0; x != IEEE_8021QAZ_MAX_TCS; x++) {
			priv->params_ethtool.max_bw_share[x] =
			    100 / IEEE_8021QAZ_MAX_TCS;
		}
		err = -mlx5_set_port_tc_bw_alloc(mdev,
		    priv->params_ethtool.max_bw_share);
	}
	PRIV_UNLOCK(priv);

	return (err);
}

static int
mlx5e_get_dscp(struct mlx5e_priv *priv)
{
	struct mlx5_core_dev *mdev = priv->mdev;
	int err;

	if (MLX5_CAP_GEN(mdev, qcam_reg) == 0 ||
	    MLX5_CAP_QCAM_REG(mdev, qpts) == 0 ||
	    MLX5_CAP_QCAM_REG(mdev, qpdpm) == 0)
		return (EOPNOTSUPP);

	PRIV_LOCK(priv);
	err = -mlx5_query_dscp2prio(mdev, priv->params_ethtool.dscp2prio);
	if (err)
		goto done;

	err = -mlx5_query_trust_state(mdev, &priv->params_ethtool.trust_state);
	if (err)
		goto done;
done:
	PRIV_UNLOCK(priv);
	return (err);
}

static void
mlx5e_tc_get_parameters(struct mlx5e_priv *priv,
    u64 *new_bw_value, u8 *max_bw_value, u8 *max_bw_unit)
{
	const u64 upper_limit_mbps = 255 * MLX5E_100MB;
	const u64 upper_limit_gbps = 255 * MLX5E_1GB;
	u64 temp;
	int i;

	memset(max_bw_value, 0, IEEE_8021QAZ_MAX_TCS);
	memset(max_bw_unit, 0, IEEE_8021QAZ_MAX_TCS);

	for (i = 0; i <= mlx5_max_tc(priv->mdev); i++) {
		temp = (new_bw_value != NULL) ?
		    new_bw_value[i] : priv->params_ethtool.max_bw_value[i];

		if (!temp) {
			max_bw_unit[i] = MLX5_BW_NO_LIMIT;
		} else if (temp > upper_limit_gbps) {
			max_bw_unit[i] = MLX5_BW_NO_LIMIT;
		} else if (temp <= upper_limit_mbps) {
			max_bw_value[i] = howmany(temp, MLX5E_100MB);
			max_bw_unit[i]  = MLX5_100_MBPS_UNIT;
		} else {
			max_bw_value[i] = howmany(temp, MLX5E_1GB);
			max_bw_unit[i]  = MLX5_GBPS_UNIT;
		}
	}
}

static int
mlx5e_tc_maxrate_handler(SYSCTL_HANDLER_ARGS)
{
	struct mlx5e_priv *priv = arg1;
	struct mlx5_core_dev *mdev = priv->mdev;
	u8 max_bw_unit[IEEE_8021QAZ_MAX_TCS];
	u8 max_bw_value[IEEE_8021QAZ_MAX_TCS];
	u64 new_bw_value[IEEE_8021QAZ_MAX_TCS];
	u8 max_rates = mlx5_max_tc(mdev) + 1;
	u8 x;
	int err;

	PRIV_LOCK(priv);
	err = SYSCTL_OUT(req, priv->params_ethtool.max_bw_value,
	    sizeof(priv->params_ethtool.max_bw_value[0]) * max_rates);
	if (err || !req->newptr)
		goto done;
	err = SYSCTL_IN(req, new_bw_value,
	    sizeof(new_bw_value[0]) * max_rates);
	if (err)
		goto done;

	/* range check input value */
	for (x = 0; x != max_rates; x++) {
		if (new_bw_value[x] % MLX5E_100MB) {
			err = ERANGE;
			goto done;
		}
	}

	mlx5e_tc_get_parameters(priv, new_bw_value, max_bw_value, max_bw_unit);

	err = -mlx5_modify_port_tc_rate_limit(mdev, max_bw_value, max_bw_unit);
	if (err)
		goto done;

	memcpy(priv->params_ethtool.max_bw_value, new_bw_value,
	    sizeof(priv->params_ethtool.max_bw_value));
done:
	PRIV_UNLOCK(priv);
	return (err);
}

static int
mlx5e_tc_rate_share_handler(SYSCTL_HANDLER_ARGS)
{
	struct mlx5e_priv *priv = arg1;
	struct mlx5_core_dev *mdev = priv->mdev;
	u8 max_bw_share[IEEE_8021QAZ_MAX_TCS];
	u8 max_rates = mlx5_max_tc(mdev) + 1;
	int i;
	int err;
	int sum;

	PRIV_LOCK(priv);
	err = SYSCTL_OUT(req, priv->params_ethtool.max_bw_share, max_rates);
	if (err || !req->newptr)
		goto done;
	err = SYSCTL_IN(req, max_bw_share, max_rates);
	if (err)
		goto done;

	/* range check input value */
	for (sum = i = 0; i != max_rates; i++) {
		if (max_bw_share[i] < 1 || max_bw_share[i] > 100) {
			err = ERANGE;
			goto done;
		}
		sum += max_bw_share[i];
	}

	/* sum of values should be as close to 100 as possible */
	if (sum < (100 - max_rates + 1) || sum > 100) {
		err = ERANGE;
		goto done;
	}

	err = -mlx5_set_port_tc_bw_alloc(mdev, max_bw_share);
	if (err)
		goto done;

	memcpy(priv->params_ethtool.max_bw_share, max_bw_share,
	    sizeof(priv->params_ethtool.max_bw_share));
done:
	PRIV_UNLOCK(priv);
	return (err);
}

static int
mlx5e_get_prio_tc(struct mlx5e_priv *priv)
{
	struct mlx5_core_dev *mdev = priv->mdev;
	int err = 0;
	int i;

	PRIV_LOCK(priv);
	if (!MLX5_CAP_GEN(priv->mdev, ets)) {
		PRIV_UNLOCK(priv);
		return (EOPNOTSUPP);
	}

	for (i = 0; i != MLX5E_MAX_PRIORITY; i++) {
		err = -mlx5_query_port_prio_tc(mdev, i, priv->params_ethtool.prio_tc + i);
		if (err)
			break;
	}
	PRIV_UNLOCK(priv);
	return (err);
}

static int
mlx5e_prio_to_tc_handler(SYSCTL_HANDLER_ARGS)
{
	struct mlx5e_priv *priv = arg1;
	struct mlx5_core_dev *mdev = priv->mdev;
	uint8_t temp[MLX5E_MAX_PRIORITY];
	int err;
	int i;

	PRIV_LOCK(priv);
	err = SYSCTL_OUT(req, priv->params_ethtool.prio_tc, MLX5E_MAX_PRIORITY);
	if (err || !req->newptr)
		goto done;
	err = SYSCTL_IN(req, temp, MLX5E_MAX_PRIORITY);
	if (err)
		goto done;

	for (i = 0; i != MLX5E_MAX_PRIORITY; i++) {
		if (temp[i] > mlx5_max_tc(mdev)) {
			err = ERANGE;
			goto done;
		}
	}

	for (i = 0; i != MLX5E_MAX_PRIORITY; i++) {
		if (temp[i] == priv->params_ethtool.prio_tc[i])
			continue;
		err = -mlx5_set_port_prio_tc(mdev, i, temp[i]);
		if (err)
			goto done;
		/* update cached value */
		priv->params_ethtool.prio_tc[i] = temp[i];
	}
done:
	PRIV_UNLOCK(priv);
	return (err);
}

int
mlx5e_fec_update(struct mlx5e_priv *priv)
{
	struct mlx5_core_dev *mdev = priv->mdev;
	u32 in[MLX5_ST_SZ_DW(pplm_reg)] = {};
	const int sz = MLX5_ST_SZ_BYTES(pplm_reg);
	int err;

	if (!MLX5_CAP_GEN(mdev, pcam_reg))
		return (EOPNOTSUPP);

	if (!MLX5_CAP_PCAM_REG(mdev, pplm))
		return (EOPNOTSUPP);

	MLX5_SET(pplm_reg, in, local_port, 1);

	err = -mlx5_core_access_reg(mdev, in, sz, in, sz, MLX5_REG_PPLM, 0, 0);
	if (err)
		return (err);

	/* get 10x..25x mask */
	priv->params_ethtool.fec_mask_10x_25x[0] =
	    MLX5_GET(pplm_reg, in, fec_override_admin_10g_40g);
	priv->params_ethtool.fec_mask_10x_25x[1] =
	    MLX5_GET(pplm_reg, in, fec_override_admin_25g) &
	    MLX5_GET(pplm_reg, in, fec_override_admin_50g);
	priv->params_ethtool.fec_mask_10x_25x[2] =
	    MLX5_GET(pplm_reg, in, fec_override_admin_56g);
	priv->params_ethtool.fec_mask_10x_25x[3] =
	    MLX5_GET(pplm_reg, in, fec_override_admin_100g);

	/* get 10x..25x available bits */
	priv->params_ethtool.fec_avail_10x_25x[0] =
	    MLX5_GET(pplm_reg, in, fec_override_cap_10g_40g);
	priv->params_ethtool.fec_avail_10x_25x[1] =
	    MLX5_GET(pplm_reg, in, fec_override_cap_25g) &
	    MLX5_GET(pplm_reg, in, fec_override_cap_50g);
	priv->params_ethtool.fec_avail_10x_25x[2] =
	    MLX5_GET(pplm_reg, in, fec_override_cap_56g);
	priv->params_ethtool.fec_avail_10x_25x[3] =
	    MLX5_GET(pplm_reg, in, fec_override_cap_100g);

	/* get 50x mask */
	priv->params_ethtool.fec_mask_50x[0] =
	    MLX5_GET(pplm_reg, in, fec_override_admin_50g_1x);
	priv->params_ethtool.fec_mask_50x[1] =
	    MLX5_GET(pplm_reg, in, fec_override_admin_100g_2x);
	priv->params_ethtool.fec_mask_50x[2] =
	    MLX5_GET(pplm_reg, in, fec_override_admin_200g_4x);
	priv->params_ethtool.fec_mask_50x[3] =
	    MLX5_GET(pplm_reg, in, fec_override_admin_400g_8x);

	/* get 50x available bits */
	priv->params_ethtool.fec_avail_50x[0] =
	    MLX5_GET(pplm_reg, in, fec_override_cap_50g_1x);
	priv->params_ethtool.fec_avail_50x[1] =
	    MLX5_GET(pplm_reg, in, fec_override_cap_100g_2x);
	priv->params_ethtool.fec_avail_50x[2] =
	    MLX5_GET(pplm_reg, in, fec_override_cap_200g_4x);
	priv->params_ethtool.fec_avail_50x[3] =
	    MLX5_GET(pplm_reg, in, fec_override_cap_400g_8x);

	/* get current FEC mask */
	priv->params_ethtool.fec_mode_active =
	    MLX5_GET(pplm_reg, in, fec_mode_active);

	return (0);
}

static int
mlx5e_fec_mask_10x_25x_handler(SYSCTL_HANDLER_ARGS)
{
	struct mlx5e_priv *priv = arg1;
	struct mlx5_core_dev *mdev = priv->mdev;
	u32 out[MLX5_ST_SZ_DW(pplm_reg)] = {};
	u32 in[MLX5_ST_SZ_DW(pplm_reg)] = {};
	const int sz = MLX5_ST_SZ_BYTES(pplm_reg);
	u8 fec_mask_10x_25x[MLX5E_MAX_FEC_10X_25X];
	u8 fec_cap_changed = 0;
	u8 x;
	int err;

	PRIV_LOCK(priv);
	err = SYSCTL_OUT(req, priv->params_ethtool.fec_mask_10x_25x,
	    sizeof(priv->params_ethtool.fec_mask_10x_25x));
	if (err || !req->newptr)
		goto done;

	err = SYSCTL_IN(req, fec_mask_10x_25x,
	    sizeof(fec_mask_10x_25x));
	if (err)
		goto done;

	if (!MLX5_CAP_GEN(mdev, pcam_reg)) {
		err = EOPNOTSUPP;
		goto done;
	}

	if (!MLX5_CAP_PCAM_REG(mdev, pplm)) {
		err = EOPNOTSUPP;
		goto done;
	}

	MLX5_SET(pplm_reg, in, local_port, 1);

	err = -mlx5_core_access_reg(mdev, in, sz, in, sz, MLX5_REG_PPLM, 0, 0);
	if (err)
		goto done;

	/* range check input value */
	for (x = 0; x != MLX5E_MAX_FEC_10X_25X; x++) {
		/* check only one bit is set, if any */
		if (fec_mask_10x_25x[x] & (fec_mask_10x_25x[x] - 1)) {
			err = ERANGE;
			goto done;
		}
		/* check a supported bit is set, if any */
		if (fec_mask_10x_25x[x] &
		    ~priv->params_ethtool.fec_avail_10x_25x[x]) {
			err = ERANGE;
			goto done;
		}
		fec_cap_changed |= (fec_mask_10x_25x[x] ^
		    priv->params_ethtool.fec_mask_10x_25x[x]);
	}

	/* check for no changes */
	if (fec_cap_changed == 0)
		goto done;

	memset(in, 0, sizeof(in));

	MLX5_SET(pplm_reg, in, local_port, 1);

	/* set new values */
	MLX5_SET(pplm_reg, in, fec_override_admin_10g_40g, fec_mask_10x_25x[0]);
	MLX5_SET(pplm_reg, in, fec_override_admin_25g, fec_mask_10x_25x[1]);
	MLX5_SET(pplm_reg, in, fec_override_admin_50g, fec_mask_10x_25x[1]);
	MLX5_SET(pplm_reg, in, fec_override_admin_56g, fec_mask_10x_25x[2]);
	MLX5_SET(pplm_reg, in, fec_override_admin_100g, fec_mask_10x_25x[3]);

	/* preserve other values */
	MLX5_SET(pplm_reg, in, fec_override_admin_50g_1x, priv->params_ethtool.fec_mask_50x[0]);
	MLX5_SET(pplm_reg, in, fec_override_admin_100g_2x, priv->params_ethtool.fec_mask_50x[1]);
	MLX5_SET(pplm_reg, in, fec_override_admin_200g_4x, priv->params_ethtool.fec_mask_50x[2]);
	MLX5_SET(pplm_reg, in, fec_override_admin_400g_8x, priv->params_ethtool.fec_mask_50x[3]);

	/* send new value to the firmware */
	err = -mlx5_core_access_reg(mdev, in, sz, out, sz, MLX5_REG_PPLM, 0, 1);
	if (err)
		goto done;

	memcpy(priv->params_ethtool.fec_mask_10x_25x, fec_mask_10x_25x,
	    sizeof(priv->params_ethtool.fec_mask_10x_25x));

	mlx5_toggle_port_link(priv->mdev);
done:
	PRIV_UNLOCK(priv);
	return (err);
}

static int
mlx5e_fec_avail_10x_25x_handler(SYSCTL_HANDLER_ARGS)
{
	struct mlx5e_priv *priv = arg1;
	int err;

	PRIV_LOCK(priv);
	err = SYSCTL_OUT(req, priv->params_ethtool.fec_avail_10x_25x,
	    sizeof(priv->params_ethtool.fec_avail_10x_25x));
	PRIV_UNLOCK(priv);
	return (err);
}

static int
mlx5e_fec_mask_50x_handler(SYSCTL_HANDLER_ARGS)
{
	struct mlx5e_priv *priv = arg1;
	struct mlx5_core_dev *mdev = priv->mdev;
	u32 out[MLX5_ST_SZ_DW(pplm_reg)] = {};
	u32 in[MLX5_ST_SZ_DW(pplm_reg)] = {};
	const int sz = MLX5_ST_SZ_BYTES(pplm_reg);
	u16 fec_mask_50x[MLX5E_MAX_FEC_50X];
	u16 fec_cap_changed = 0;
	u8 x;
	int err;

	PRIV_LOCK(priv);
	err = SYSCTL_OUT(req, priv->params_ethtool.fec_mask_50x,
	    sizeof(priv->params_ethtool.fec_mask_50x));
	if (err || !req->newptr)
		goto done;

	err = SYSCTL_IN(req, fec_mask_50x,
	    sizeof(fec_mask_50x));
	if (err)
		goto done;

	if (!MLX5_CAP_GEN(mdev, pcam_reg)) {
		err = EOPNOTSUPP;
		goto done;
	}

	if (!MLX5_CAP_PCAM_REG(mdev, pplm)) {
		err = EOPNOTSUPP;
		goto done;
	}

	MLX5_SET(pplm_reg, in, local_port, 1);

	err = -mlx5_core_access_reg(mdev, in, sz, in, sz, MLX5_REG_PPLM, 0, 0);
	if (err)
		goto done;

	/* range check input value */
	for (x = 0; x != MLX5E_MAX_FEC_50X; x++) {
		/* check only one bit is set, if any */
		if (fec_mask_50x[x] & (fec_mask_50x[x] - 1)) {
			err = ERANGE;
			goto done;
		}
		/* check a supported bit is set, if any */
		if (fec_mask_50x[x] &
		    ~priv->params_ethtool.fec_avail_50x[x]) {
			err = ERANGE;
			goto done;
		}
		fec_cap_changed |= (fec_mask_50x[x] ^
		    priv->params_ethtool.fec_mask_50x[x]);
	}

	/* check for no changes */
	if (fec_cap_changed == 0)
		goto done;

	memset(in, 0, sizeof(in));

	MLX5_SET(pplm_reg, in, local_port, 1);

	/* set new values */
	MLX5_SET(pplm_reg, in, fec_override_admin_50g_1x, fec_mask_50x[0]);
	MLX5_SET(pplm_reg, in, fec_override_admin_100g_2x, fec_mask_50x[1]);
	MLX5_SET(pplm_reg, in, fec_override_admin_200g_4x, fec_mask_50x[2]);
	MLX5_SET(pplm_reg, in, fec_override_admin_400g_8x, fec_mask_50x[3]);

	/* preserve other values */
	MLX5_SET(pplm_reg, in, fec_override_admin_10g_40g, priv->params_ethtool.fec_mask_10x_25x[0]);
	MLX5_SET(pplm_reg, in, fec_override_admin_25g, priv->params_ethtool.fec_mask_10x_25x[1]);
	MLX5_SET(pplm_reg, in, fec_override_admin_50g, priv->params_ethtool.fec_mask_10x_25x[1]);
	MLX5_SET(pplm_reg, in, fec_override_admin_56g, priv->params_ethtool.fec_mask_10x_25x[2]);
	MLX5_SET(pplm_reg, in, fec_override_admin_100g, priv->params_ethtool.fec_mask_10x_25x[3]);

	/* send new value to the firmware */
	err = -mlx5_core_access_reg(mdev, in, sz, out, sz, MLX5_REG_PPLM, 0, 1);
	if (err)
		goto done;

	memcpy(priv->params_ethtool.fec_mask_50x, fec_mask_50x,
	    sizeof(priv->params_ethtool.fec_mask_50x));

	mlx5_toggle_port_link(priv->mdev);
done:
	PRIV_UNLOCK(priv);
	return (err);
}

static int
mlx5e_fec_avail_50x_handler(SYSCTL_HANDLER_ARGS)
{
	struct mlx5e_priv *priv = arg1;
	int err;

	PRIV_LOCK(priv);
	err = SYSCTL_OUT(req, priv->params_ethtool.fec_avail_50x,
	    sizeof(priv->params_ethtool.fec_avail_50x));
	PRIV_UNLOCK(priv);
	return (err);
}

static int
mlx5e_trust_state_handler(SYSCTL_HANDLER_ARGS)
{
	struct mlx5e_priv *priv = arg1;
	struct mlx5_core_dev *mdev = priv->mdev;
	int err;
	u8 result;

	PRIV_LOCK(priv);
	result = priv->params_ethtool.trust_state;
	err = sysctl_handle_8(oidp, &result, 0, req);
	if (err || !req->newptr ||
	    result == priv->params_ethtool.trust_state)
		goto done;

	switch (result) {
	case MLX5_QPTS_TRUST_PCP:
	case MLX5_QPTS_TRUST_DSCP:
		break;
	case MLX5_QPTS_TRUST_BOTH:
		if (!MLX5_CAP_QCAM_FEATURE(mdev, qpts_trust_both)) {
			err = EOPNOTSUPP;
			goto done;
		}
		break;
	default:
		err = ERANGE;
		goto done;
	}

	err = -mlx5_set_trust_state(mdev, result);
	if (err)
		goto done;

	priv->params_ethtool.trust_state = result;

	/* update inline mode */
	mlx5e_refresh_sq_inline(priv);
#ifdef RATELIMIT
	mlx5e_rl_refresh_sq_inline(&priv->rl);
#endif
done:
	PRIV_UNLOCK(priv);
	return (err);
}

static int
mlx5e_dscp_prio_handler(SYSCTL_HANDLER_ARGS)
{
	struct mlx5e_priv *priv = arg1;
	int prio_index = arg2;
	struct mlx5_core_dev *mdev = priv->mdev;
	uint8_t dscp2prio[MLX5_MAX_SUPPORTED_DSCP];
	uint8_t x;
	int err;

	PRIV_LOCK(priv);
	err = SYSCTL_OUT(req, priv->params_ethtool.dscp2prio + prio_index,
	    sizeof(priv->params_ethtool.dscp2prio) / 8);
	if (err || !req->newptr)
		goto done;

	memcpy(dscp2prio, priv->params_ethtool.dscp2prio, sizeof(dscp2prio));
	err = SYSCTL_IN(req, dscp2prio + prio_index, sizeof(dscp2prio) / 8);
	if (err)
		goto done;
	for (x = 0; x != MLX5_MAX_SUPPORTED_DSCP; x++) {
		if (dscp2prio[x] > 7) {
			err = ERANGE;
			goto done;
		}
	}
	err = -mlx5_set_dscp2prio(mdev, dscp2prio);
	if (err)
		goto done;

	/* update local array */
	memcpy(priv->params_ethtool.dscp2prio, dscp2prio,
	    sizeof(priv->params_ethtool.dscp2prio));
done:
	PRIV_UNLOCK(priv);
	return (err);
}

int
mlx5e_update_buf_lossy(struct mlx5e_priv *priv)
{
	struct ieee_pfc pfc;

	PRIV_ASSERT_LOCKED(priv);
	bzero(&pfc, sizeof(pfc));
	pfc.pfc_en = priv->params.rx_priority_flow_control;
	return (-mlx5e_port_manual_buffer_config(priv, MLX5E_PORT_BUFFER_PFC,
	    priv->params_ethtool.hw_mtu, &pfc, NULL, NULL));
}

static int
mlx5e_buf_size_handler(SYSCTL_HANDLER_ARGS)
{
	struct mlx5e_priv *priv;
	u32 buf_size[MLX5E_MAX_BUFFER];
	struct mlx5e_port_buffer port_buffer;
	int error, i;

	priv = arg1;
	PRIV_LOCK(priv);
	error = -mlx5e_port_query_buffer(priv, &port_buffer);
	if (error != 0)
		goto done;
	for (i = 0; i < nitems(buf_size); i++)
		buf_size[i] = port_buffer.buffer[i].size;
	error = SYSCTL_OUT(req, buf_size, sizeof(buf_size));
	if (error != 0 || req->newptr == NULL)
		goto done;
	error = SYSCTL_IN(req, buf_size, sizeof(buf_size));
	if (error != 0)
		goto done;
	error = -mlx5e_port_manual_buffer_config(priv, MLX5E_PORT_BUFFER_SIZE,
	    priv->params_ethtool.hw_mtu, NULL, buf_size, NULL);
done:
	PRIV_UNLOCK(priv);
	return (error);
}

static int
mlx5e_buf_prio_handler(SYSCTL_HANDLER_ARGS)
{
	struct mlx5e_priv *priv;
	struct mlx5_core_dev *mdev;
	u8 buffer[MLX5E_MAX_BUFFER];
	int error;

	priv = arg1;
	mdev = priv->mdev;
	PRIV_LOCK(priv);
	error = -mlx5e_port_query_priority2buffer(mdev, buffer);
	if (error != 0)
		goto done;
	error = SYSCTL_OUT(req, buffer, MLX5E_MAX_BUFFER);
	if (error != 0 || req->newptr == NULL)
		goto done;
	error = SYSCTL_IN(req, buffer, MLX5E_MAX_BUFFER);
	if (error != 0)
		goto done;
	error = -mlx5e_port_manual_buffer_config(priv,
	    MLX5E_PORT_BUFFER_PRIO2BUFFER,
	    priv->params_ethtool.hw_mtu, NULL, NULL, buffer);
	if (error == 0)
		error = mlx5e_update_buf_lossy(priv);
done:
	PRIV_UNLOCK(priv);
	return (error);
}

static int
mlx5e_cable_length_handler(SYSCTL_HANDLER_ARGS)
{
	struct mlx5e_priv *priv;
	u_int cable_len;
	int error;

	priv = arg1;
	PRIV_LOCK(priv);
	cable_len = priv->dcbx.cable_len;
	error = sysctl_handle_int(oidp, &cable_len, 0, req);
	if (error == 0 && req->newptr != NULL &&
	    cable_len != priv->dcbx.cable_len) {
		error = -mlx5e_port_manual_buffer_config(priv,
		    MLX5E_PORT_BUFFER_CABLE_LEN, priv->params_ethtool.hw_mtu,
		    NULL, NULL, NULL);
		if (error == 0)
			priv->dcbx.cable_len = cable_len;
	}
	PRIV_UNLOCK(priv);
	return (error);
}

static int
mlx5e_hw_temperature_handler(SYSCTL_HANDLER_ARGS)
{
	struct mlx5e_priv *priv = arg1;
	int err;

	PRIV_LOCK(priv);
	err = SYSCTL_OUT(req, priv->params_ethtool.hw_val_temp,
	    sizeof(priv->params_ethtool.hw_val_temp[0]) *
	    priv->params_ethtool.hw_num_temp);
	if (err == 0 && req->newptr != NULL)
		err = EOPNOTSUPP;
	PRIV_UNLOCK(priv);
	return (err);
}

int
mlx5e_hw_temperature_update(struct mlx5e_priv *priv)
{
	int err;
	u32 x;

	if (priv->params_ethtool.hw_num_temp == 0) {
		u32 out_cap[MLX5_ST_SZ_DW(mtcap)] = {};
		const int sz_cap = MLX5_ST_SZ_BYTES(mtcap);
		u32 value;

		err = -mlx5_core_access_reg(priv->mdev, NULL, 0, out_cap, sz_cap,
		    MLX5_ACCESS_REG_SUMMARY_CTRL_ID_MTCAP, 0, 0);
		if (err)
			goto done;
		value = MLX5_GET(mtcap, out_cap, sensor_count);
		if (value == 0)
			return (0);
		if (value > MLX5_MAX_TEMPERATURE)
			value = MLX5_MAX_TEMPERATURE;
		/* update number of temperature sensors */
		priv->params_ethtool.hw_num_temp = value;
	}

	for (x = 0; x != priv->params_ethtool.hw_num_temp; x++) {
		u32 out_sensor[MLX5_ST_SZ_DW(mtmp_reg)] = {};
		const int sz_sensor = MLX5_ST_SZ_BYTES(mtmp_reg);

		MLX5_SET(mtmp_reg, out_sensor, sensor_index, x);

		err = -mlx5_core_access_reg(priv->mdev, out_sensor, sz_sensor,
		    out_sensor, sz_sensor,
		    MLX5_ACCESS_REG_SUMMARY_CTRL_ID_MTMP, 0, 0);
		if (err)
			goto done;
		/* convert from 0.125 celsius to millicelsius */
		priv->params_ethtool.hw_val_temp[x] =
		    (s16)MLX5_GET(mtmp_reg, out_sensor, temperature) * 125;
	}
done:
	return (err);
}

#define	MLX5_PARAM_OFFSET(n)				\
    __offsetof(struct mlx5e_priv, params_ethtool.n)

static int
mlx5e_ethtool_handler(SYSCTL_HANDLER_ARGS)
{
	struct mlx5e_priv *priv = arg1;
	uint64_t value;
	int mode_modify;
	int was_opened;
	int error;

	PRIV_LOCK(priv);
	value = priv->params_ethtool.arg[arg2];
	if (req != NULL) {
		error = sysctl_handle_64(oidp, &value, 0, req);
		if (error || req->newptr == NULL ||
		    value == priv->params_ethtool.arg[arg2])
			goto done;

		/* assign new value */
		priv->params_ethtool.arg[arg2] = value;
	} else {
		error = 0;
	}
	/* check if device is gone */
	if (priv->gone) {
		error = ENXIO;
		goto done;
	}
	was_opened = test_bit(MLX5E_STATE_OPENED, &priv->state);
	mode_modify = MLX5_CAP_GEN(priv->mdev, cq_period_mode_modify);

	switch (MLX5_PARAM_OFFSET(arg[arg2])) {
	case MLX5_PARAM_OFFSET(rx_coalesce_usecs):
		/* import RX coal time */
		if (priv->params_ethtool.rx_coalesce_usecs < 1)
			priv->params_ethtool.rx_coalesce_usecs = 0;
		else if (priv->params_ethtool.rx_coalesce_usecs >
		    MLX5E_FLD_MAX(cqc, cq_period)) {
			priv->params_ethtool.rx_coalesce_usecs =
			    MLX5E_FLD_MAX(cqc, cq_period);
		}
		priv->params.rx_cq_moderation_usec =
		    priv->params_ethtool.rx_coalesce_usecs;

		/* check to avoid down and up the network interface */
		if (was_opened)
			error = mlx5e_refresh_channel_params(priv);
		break;

	case MLX5_PARAM_OFFSET(rx_coalesce_pkts):
		/* import RX coal pkts */
		if (priv->params_ethtool.rx_coalesce_pkts < 1)
			priv->params_ethtool.rx_coalesce_pkts = 0;
		else if (priv->params_ethtool.rx_coalesce_pkts >
		    MLX5E_FLD_MAX(cqc, cq_max_count)) {
			priv->params_ethtool.rx_coalesce_pkts =
			    MLX5E_FLD_MAX(cqc, cq_max_count);
		}
		priv->params.rx_cq_moderation_pkts =
		    priv->params_ethtool.rx_coalesce_pkts;

		/* check to avoid down and up the network interface */
		if (was_opened)
			error = mlx5e_refresh_channel_params(priv);
		break;

	case MLX5_PARAM_OFFSET(tx_coalesce_usecs):
		/* import TX coal time */
		if (priv->params_ethtool.tx_coalesce_usecs < 1)
			priv->params_ethtool.tx_coalesce_usecs = 0;
		else if (priv->params_ethtool.tx_coalesce_usecs >
		    MLX5E_FLD_MAX(cqc, cq_period)) {
			priv->params_ethtool.tx_coalesce_usecs =
			    MLX5E_FLD_MAX(cqc, cq_period);
		}
		priv->params.tx_cq_moderation_usec =
		    priv->params_ethtool.tx_coalesce_usecs;

		/* check to avoid down and up the network interface */
		if (was_opened)
			error = mlx5e_refresh_channel_params(priv);
		break;

	case MLX5_PARAM_OFFSET(tx_coalesce_pkts):
		/* import TX coal pkts */
		if (priv->params_ethtool.tx_coalesce_pkts < 1)
			priv->params_ethtool.tx_coalesce_pkts = 0;
		else if (priv->params_ethtool.tx_coalesce_pkts >
		    MLX5E_FLD_MAX(cqc, cq_max_count)) {
			priv->params_ethtool.tx_coalesce_pkts =
			    MLX5E_FLD_MAX(cqc, cq_max_count);
		}
		priv->params.tx_cq_moderation_pkts =
		    priv->params_ethtool.tx_coalesce_pkts;

		/* check to avoid down and up the network interface */
		if (was_opened)
			error = mlx5e_refresh_channel_params(priv);
		break;

	case MLX5_PARAM_OFFSET(tx_queue_size):
		/* network interface must be down */
		if (was_opened)
			mlx5e_close_locked(priv->ifp);

		/* import TX queue size */
		if (priv->params_ethtool.tx_queue_size <
		    (1 << MLX5E_PARAMS_MINIMUM_LOG_SQ_SIZE)) {
			priv->params_ethtool.tx_queue_size =
			    (1 << MLX5E_PARAMS_MINIMUM_LOG_SQ_SIZE);
		} else if (priv->params_ethtool.tx_queue_size >
		    priv->params_ethtool.tx_queue_size_max) {
			priv->params_ethtool.tx_queue_size =
			    priv->params_ethtool.tx_queue_size_max;
		}
		/* store actual TX queue size */
		priv->params.log_sq_size =
		    order_base_2(priv->params_ethtool.tx_queue_size);
		priv->params_ethtool.tx_queue_size =
		    1 << priv->params.log_sq_size;

		/* verify TX completion factor */
		mlx5e_ethtool_sync_tx_completion_fact(priv);

		/* restart network interface, if any */
		if (was_opened)
			mlx5e_open_locked(priv->ifp);
		break;

	case MLX5_PARAM_OFFSET(rx_queue_size):
		/* network interface must be down */
		if (was_opened)
			mlx5e_close_locked(priv->ifp);

		/* import RX queue size */
		if (priv->params_ethtool.rx_queue_size <
		    (1 << MLX5E_PARAMS_MINIMUM_LOG_RQ_SIZE)) {
			priv->params_ethtool.rx_queue_size =
			    (1 << MLX5E_PARAMS_MINIMUM_LOG_RQ_SIZE);
		} else if (priv->params_ethtool.rx_queue_size >
		    priv->params_ethtool.rx_queue_size_max) {
			priv->params_ethtool.rx_queue_size =
			    priv->params_ethtool.rx_queue_size_max;
		}
		/* store actual RX queue size */
		priv->params.log_rq_size =
		    order_base_2(priv->params_ethtool.rx_queue_size);
		priv->params_ethtool.rx_queue_size =
		    1 << priv->params.log_rq_size;

		/* update least number of RX WQEs */
		priv->params.min_rx_wqes = min(
		    priv->params_ethtool.rx_queue_size - 1,
		    MLX5E_PARAMS_DEFAULT_MIN_RX_WQES);

		/* restart network interface, if any */
		if (was_opened)
			mlx5e_open_locked(priv->ifp);
		break;

	case MLX5_PARAM_OFFSET(channels_rsss):
		/* network interface must be down */
		if (was_opened)
			mlx5e_close_locked(priv->ifp);

		/* import number of channels */
		if (priv->params_ethtool.channels_rsss < 1)
			priv->params_ethtool.channels_rsss = 1;
		else if (priv->params_ethtool.channels_rsss > 128)
			priv->params_ethtool.channels_rsss = 128;

		priv->params.channels_rsss = priv->params_ethtool.channels_rsss;

		/* restart network interface, if any */
		if (was_opened)
			mlx5e_open_locked(priv->ifp);
		break;

	case MLX5_PARAM_OFFSET(channels):
		/* network interface must be down */
		if (was_opened)
			mlx5e_close_locked(priv->ifp);

		/* import number of channels */
		if (priv->params_ethtool.channels < 1)
			priv->params_ethtool.channels = 1;
		else if (priv->params_ethtool.channels >
		    (u64) priv->mdev->priv.eq_table.num_comp_vectors) {
			priv->params_ethtool.channels =
			    (u64) priv->mdev->priv.eq_table.num_comp_vectors;
		}
		priv->params.num_channels = priv->params_ethtool.channels;

		/* restart network interface, if any */
		if (was_opened)
			mlx5e_open_locked(priv->ifp);
		break;

	case MLX5_PARAM_OFFSET(rx_coalesce_mode):
		/* network interface must be down */
		if (was_opened != 0 && mode_modify == 0)
			mlx5e_close_locked(priv->ifp);

		/* import RX coalesce mode */
		if (priv->params_ethtool.rx_coalesce_mode > 3)
			priv->params_ethtool.rx_coalesce_mode = 3;
		priv->params.rx_cq_moderation_mode =
		    priv->params_ethtool.rx_coalesce_mode;

		/* restart network interface, if any */
		if (was_opened != 0) {
			if (mode_modify == 0)
				mlx5e_open_locked(priv->ifp);
			else
				error = mlx5e_refresh_channel_params(priv);
		}
		break;

	case MLX5_PARAM_OFFSET(tx_coalesce_mode):
		/* network interface must be down */
		if (was_opened != 0 && mode_modify == 0)
			mlx5e_close_locked(priv->ifp);

		/* import TX coalesce mode */
		if (priv->params_ethtool.tx_coalesce_mode != 0)
			priv->params_ethtool.tx_coalesce_mode = 1;
		priv->params.tx_cq_moderation_mode =
		    priv->params_ethtool.tx_coalesce_mode;

		/* restart network interface, if any */
		if (was_opened != 0) {
			if (mode_modify == 0)
				mlx5e_open_locked(priv->ifp);
			else
				error = mlx5e_refresh_channel_params(priv);
		}
		break;

	case MLX5_PARAM_OFFSET(hw_lro):
		/* network interface must be down */
		if (was_opened)
			mlx5e_close_locked(priv->ifp);

		/* import HW LRO mode */
		if (priv->params_ethtool.hw_lro != 0 &&
		    MLX5_CAP_ETH(priv->mdev, lro_cap)) {
			priv->params_ethtool.hw_lro = 1;
			/* check if feature should actually be enabled */
			if (priv->ifp->if_capenable & IFCAP_LRO) {
				priv->params.hw_lro_en = true;
			} else {
				priv->params.hw_lro_en = false;

				mlx5_en_warn(priv->ifp, "To enable HW LRO "
				    "please also enable LRO via ifconfig(8).\n");
			}
		} else {
			/* return an error if HW does not support this feature */
			if (priv->params_ethtool.hw_lro != 0)
				error = EINVAL;
			priv->params.hw_lro_en = false;
			priv->params_ethtool.hw_lro = 0;
		}
		/* restart network interface, if any */
		if (was_opened)
			mlx5e_open_locked(priv->ifp);
		break;

	case MLX5_PARAM_OFFSET(cqe_zipping):
		/* network interface must be down */
		if (was_opened)
			mlx5e_close_locked(priv->ifp);

		/* import CQE zipping mode */
		if (priv->params_ethtool.cqe_zipping &&
		    MLX5_CAP_GEN(priv->mdev, cqe_compression)) {
			priv->params.cqe_zipping_en = true;
			priv->params_ethtool.cqe_zipping = 1;
		} else {
			priv->params.cqe_zipping_en = false;
			priv->params_ethtool.cqe_zipping = 0;
		}
		/* restart network interface, if any */
		if (was_opened)
			mlx5e_open_locked(priv->ifp);
		break;

	case MLX5_PARAM_OFFSET(tx_completion_fact):
		/* network interface must be down */
		if (was_opened)
			mlx5e_close_locked(priv->ifp);

		/* verify parameter */
		mlx5e_ethtool_sync_tx_completion_fact(priv);

		/* restart network interface, if any */
		if (was_opened)
			mlx5e_open_locked(priv->ifp);
		break;

	case MLX5_PARAM_OFFSET(modify_tx_dma):
		/* check if network interface is opened */
		if (was_opened) {
			priv->params_ethtool.modify_tx_dma =
			    priv->params_ethtool.modify_tx_dma ? 1 : 0;
			/* modify tx according to value */
			mlx5e_modify_tx_dma(priv, value != 0);
		} else {
			/* if closed force enable tx */
			priv->params_ethtool.modify_tx_dma = 0;
		}
		break;

	case MLX5_PARAM_OFFSET(modify_rx_dma):
		/* check if network interface is opened */
		if (was_opened) {
			priv->params_ethtool.modify_rx_dma =
			    priv->params_ethtool.modify_rx_dma ? 1 : 0;
			/* modify rx according to value */
			mlx5e_modify_rx_dma(priv, value != 0);
		} else {
			/* if closed force enable rx */
			priv->params_ethtool.modify_rx_dma = 0;
		}
		break;

	case MLX5_PARAM_OFFSET(diag_pci_enable):
		priv->params_ethtool.diag_pci_enable =
		    priv->params_ethtool.diag_pci_enable ? 1 : 0;

		error = -mlx5_core_set_diagnostics_full(priv->mdev,
		    priv->params_ethtool.diag_pci_enable,
		    priv->params_ethtool.diag_general_enable);
		break;

	case MLX5_PARAM_OFFSET(diag_general_enable):
		priv->params_ethtool.diag_general_enable =
		    priv->params_ethtool.diag_general_enable ? 1 : 0;

		error = -mlx5_core_set_diagnostics_full(priv->mdev,
		    priv->params_ethtool.diag_pci_enable,
		    priv->params_ethtool.diag_general_enable);
		break;

	case MLX5_PARAM_OFFSET(mc_local_lb):
		priv->params_ethtool.mc_local_lb =
		    priv->params_ethtool.mc_local_lb ? 1 : 0;

		if (MLX5_CAP_GEN(priv->mdev, disable_local_lb)) {
			error = mlx5_nic_vport_modify_local_lb(priv->mdev,
			    MLX5_LOCAL_MC_LB, priv->params_ethtool.mc_local_lb);
		} else {
			error = EOPNOTSUPP;
		}
		break;

	case MLX5_PARAM_OFFSET(uc_local_lb):
		priv->params_ethtool.uc_local_lb =
		    priv->params_ethtool.uc_local_lb ? 1 : 0;

		if (MLX5_CAP_GEN(priv->mdev, disable_local_lb)) {
			error = mlx5_nic_vport_modify_local_lb(priv->mdev,
			    MLX5_LOCAL_UC_LB, priv->params_ethtool.uc_local_lb);
		} else {
			error = EOPNOTSUPP;
		}
		break;

	case MLX5_PARAM_OFFSET(irq_cpu_base):
	case MLX5_PARAM_OFFSET(irq_cpu_stride):
		if (was_opened) {
			/* network interface must toggled */
			mlx5e_close_locked(priv->ifp);
			mlx5e_open_locked(priv->ifp);
		}
		break;

	default:
		break;
	}
done:
	PRIV_UNLOCK(priv);
	return (error);
}

static const char *mlx5e_params_desc[] = {
	MLX5E_PARAMS(MLX5E_STATS_DESC)
};

static const char *mlx5e_port_stats_debug_desc[] = {
	MLX5E_PORT_STATS_DEBUG(MLX5E_STATS_DESC)
};

static int
mlx5e_ethtool_debug_channel_info(SYSCTL_HANDLER_ARGS)
{
	struct mlx5e_priv *priv;
	struct sbuf sb;
	struct mlx5e_channel *c;
	struct mlx5e_sq *sq;
	struct mlx5e_rq *rq;
	int error, i, tc;
	bool opened;

	priv = arg1;
	error = sysctl_wire_old_buffer(req, 0);
	if (error != 0)
		return (error);
	if (sbuf_new_for_sysctl(&sb, NULL, 1024, req) == NULL)
		return (ENOMEM);
	sbuf_clear_flags(&sb, SBUF_INCLUDENUL);

	PRIV_LOCK(priv);
	opened = test_bit(MLX5E_STATE_OPENED, &priv->state);

	sbuf_printf(&sb, "pages irq %d\n",
	    priv->mdev->priv.msix_arr[MLX5_EQ_VEC_PAGES].vector);
	sbuf_printf(&sb, "command irq %d\n",
	    priv->mdev->priv.msix_arr[MLX5_EQ_VEC_CMD].vector);
	sbuf_printf(&sb, "async irq %d\n",
	    priv->mdev->priv.msix_arr[MLX5_EQ_VEC_ASYNC].vector);

	for (i = 0; i != priv->params.num_channels; i++) {
		int eqn_not_used = -1;
		int irqn = MLX5_EQ_VEC_COMP_BASE;

		if (mlx5_vector2eqn(priv->mdev, i, &eqn_not_used, &irqn) != 0)
			continue;

		c = opened ? &priv->channel[i] : NULL;
		rq = opened ? &c->rq : NULL;
		sbuf_printf(&sb, "channel %d rq %d cq %d irq %d\n", i,
		    opened ? rq->rqn : -1,
		    opened ? rq->cq.mcq.cqn : -1,
		    priv->mdev->priv.msix_arr[irqn].vector);

		for (tc = 0; tc != priv->num_tc; tc++) {
			sq = opened ? &c->sq[tc] : NULL;
			sbuf_printf(&sb, "channel %d tc %d sq %d cq %d irq %d\n",
			    i, tc,
			    opened ? sq->sqn : -1,
			    opened ? sq->cq.mcq.cqn : -1,
			    priv->mdev->priv.msix_arr[irqn].vector);
		}
	}
	PRIV_UNLOCK(priv);
	error = sbuf_finish(&sb);
	sbuf_delete(&sb);
	return (error);
}

static int
mlx5e_ethtool_debug_stats(SYSCTL_HANDLER_ARGS)
{
	struct mlx5e_priv *priv = arg1;
	int sys_debug;
	int error;

	PRIV_LOCK(priv);
	if (priv->gone != 0) {
		error = ENODEV;
		goto done;
	}
	sys_debug = priv->sysctl_debug;
	error = sysctl_handle_int(oidp, &sys_debug, 0, req);
	if (error != 0 || !req->newptr)
		goto done;
	sys_debug = sys_debug ? 1 : 0;
	if (sys_debug == priv->sysctl_debug)
		goto done;

	if ((priv->sysctl_debug = sys_debug)) {
		mlx5e_create_stats(&priv->stats.port_stats_debug.ctx,
		    SYSCTL_CHILDREN(priv->sysctl_ifnet), "debug_stats",
		    mlx5e_port_stats_debug_desc, MLX5E_PORT_STATS_DEBUG_NUM,
		    priv->stats.port_stats_debug.arg);
		SYSCTL_ADD_PROC(&priv->stats.port_stats_debug.ctx,
		    SYSCTL_CHILDREN(priv->sysctl_ifnet), OID_AUTO,
		    "hw_ctx_debug",
		    CTLFLAG_RD | CTLFLAG_MPSAFE | CTLTYPE_STRING, priv, 0,
		    mlx5e_ethtool_debug_channel_info, "S", "");
	} else {
		sysctl_ctx_free(&priv->stats.port_stats_debug.ctx);
	}
done:
	PRIV_UNLOCK(priv);
	return (error);
}

static void
mlx5e_create_diagnostics(struct mlx5e_priv *priv)
{
	struct mlx5_core_diagnostics_entry entry;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *node;
	int x;

	/* sysctl context we are using */
	ctx = &priv->sysctl_ctx;

	/* create root node */
	node = SYSCTL_ADD_NODE(ctx,
	    SYSCTL_CHILDREN(priv->sysctl_ifnet), OID_AUTO,
	    "diagnostics", CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "Diagnostics");
	if (node == NULL)
		return;

	/* create PCI diagnostics */
	for (x = 0; x != MLX5_CORE_PCI_DIAGNOSTICS_NUM; x++) {
		entry = mlx5_core_pci_diagnostics_table[x];
		if (mlx5_core_supports_diagnostics(priv->mdev, entry.counter_id) == 0)
			continue;
		SYSCTL_ADD_UQUAD(ctx, SYSCTL_CHILDREN(node), OID_AUTO,
		    entry.desc, CTLFLAG_RD, priv->params_pci.array + x,
		    "PCI diagnostics counter");
	}

	/* create general diagnostics */
	for (x = 0; x != MLX5_CORE_GENERAL_DIAGNOSTICS_NUM; x++) {
		entry = mlx5_core_general_diagnostics_table[x];
		if (mlx5_core_supports_diagnostics(priv->mdev, entry.counter_id) == 0)
			continue;
		SYSCTL_ADD_UQUAD(ctx, SYSCTL_CHILDREN(node), OID_AUTO,
		    entry.desc, CTLFLAG_RD, priv->params_general.array + x,
		    "General diagnostics counter");
	}
}

void
mlx5e_create_ethtool(struct mlx5e_priv *priv)
{
	struct sysctl_oid *fec_node;
	struct sysctl_oid *qos_node;
	struct sysctl_oid *node;
	const char *pnameunit;
	struct mlx5e_port_buffer port_buffer;
	unsigned x;
	int i;

	/* set some defaults */
	priv->params_ethtool.irq_cpu_base = -1;	/* disabled */
	priv->params_ethtool.irq_cpu_stride = 1;
	priv->params_ethtool.tx_queue_size_max = 1 << MLX5E_PARAMS_MAXIMUM_LOG_SQ_SIZE;
	priv->params_ethtool.rx_queue_size_max = 1 << MLX5E_PARAMS_MAXIMUM_LOG_RQ_SIZE;
	priv->params_ethtool.tx_queue_size = 1 << priv->params.log_sq_size;
	priv->params_ethtool.rx_queue_size = 1 << priv->params.log_rq_size;
	priv->params_ethtool.channels = priv->params.num_channels;
	priv->params_ethtool.channels_rsss = priv->params.channels_rsss;
	priv->params_ethtool.coalesce_pkts_max = MLX5E_FLD_MAX(cqc, cq_max_count);
	priv->params_ethtool.coalesce_usecs_max = MLX5E_FLD_MAX(cqc, cq_period);
	priv->params_ethtool.rx_coalesce_mode = priv->params.rx_cq_moderation_mode;
	priv->params_ethtool.rx_coalesce_usecs = priv->params.rx_cq_moderation_usec;
	priv->params_ethtool.rx_coalesce_pkts = priv->params.rx_cq_moderation_pkts;
	priv->params_ethtool.tx_coalesce_mode = priv->params.tx_cq_moderation_mode;
	priv->params_ethtool.tx_coalesce_usecs = priv->params.tx_cq_moderation_usec;
	priv->params_ethtool.tx_coalesce_pkts = priv->params.tx_cq_moderation_pkts;
	priv->params_ethtool.hw_lro = priv->params.hw_lro_en;
	priv->params_ethtool.cqe_zipping = priv->params.cqe_zipping_en;
	mlx5e_ethtool_sync_tx_completion_fact(priv);

	/* get default values for local loopback, if any */
	if (MLX5_CAP_GEN(priv->mdev, disable_local_lb)) {
		int err;
		u8 val;

		err = mlx5_nic_vport_query_local_lb(priv->mdev, MLX5_LOCAL_MC_LB, &val);
		if (err == 0)
			priv->params_ethtool.mc_local_lb = val;

		err = mlx5_nic_vport_query_local_lb(priv->mdev, MLX5_LOCAL_UC_LB, &val);
		if (err == 0)
			priv->params_ethtool.uc_local_lb = val;
	}

	/* create root node */
	node = SYSCTL_ADD_NODE(&priv->sysctl_ctx,
	    SYSCTL_CHILDREN(priv->sysctl_ifnet), OID_AUTO,
	    "conf", CTLFLAG_RW | CTLFLAG_MPSAFE, NULL, "Configuration");
	if (node == NULL)
		return;
	for (x = 0; x != MLX5E_PARAMS_NUM; x++) {
		/* check for read-only parameter */
		if (strstr(mlx5e_params_desc[2 * x], "_max") != NULL ||
		    strstr(mlx5e_params_desc[2 * x], "_mtu") != NULL) {
			SYSCTL_ADD_PROC(&priv->sysctl_ctx, SYSCTL_CHILDREN(node), OID_AUTO,
			    mlx5e_params_desc[2 * x], CTLTYPE_U64 | CTLFLAG_RD |
			    CTLFLAG_MPSAFE, priv, x, &mlx5e_ethtool_handler, "QU",
			    mlx5e_params_desc[2 * x + 1]);
		} else {
			/*
			 * NOTE: In FreeBSD-11 and newer the
			 * CTLFLAG_RWTUN flag will take care of
			 * loading default sysctl value from the
			 * kernel environment, if any:
			 */
			SYSCTL_ADD_PROC(&priv->sysctl_ctx, SYSCTL_CHILDREN(node), OID_AUTO,
			    mlx5e_params_desc[2 * x], CTLTYPE_U64 | CTLFLAG_RWTUN |
			    CTLFLAG_MPSAFE, priv, x, &mlx5e_ethtool_handler, "QU",
			    mlx5e_params_desc[2 * x + 1]);
		}
	}

	/* create fec node */
	fec_node = SYSCTL_ADD_NODE(&priv->sysctl_ctx,
	    SYSCTL_CHILDREN(node), OID_AUTO,
	    "fec", CTLFLAG_RW | CTLFLAG_MPSAFE, NULL,
	    "Forward Error Correction");
	if (fec_node == NULL)
		return;

	if (mlx5e_fec_update(priv) == 0) {
		SYSCTL_ADD_U32(&priv->sysctl_ctx, SYSCTL_CHILDREN(fec_node), OID_AUTO,
		    "mode_active", CTLFLAG_RD | CTLFLAG_MPSAFE,
		    &priv->params_ethtool.fec_mode_active, 0,
		    "Current FEC mode bit, if any.");

		SYSCTL_ADD_PROC(&priv->sysctl_ctx, SYSCTL_CHILDREN(fec_node), OID_AUTO,
		    "mask_10x_25x", CTLTYPE_U8 | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
		    priv, 0, &mlx5e_fec_mask_10x_25x_handler, "CU",
		    "Set FEC masks for 10G_40G, 25G_50G, 56G, 100G respectivly. "
		    "0:Auto "
		    "1:NOFEC "
		    "2:FIRECODE "
		    "4:RS");

		SYSCTL_ADD_PROC(&priv->sysctl_ctx, SYSCTL_CHILDREN(fec_node), OID_AUTO,
		    "avail_10x_25x", CTLTYPE_U8 | CTLFLAG_RD | CTLFLAG_MPSAFE,
		    priv, 0, &mlx5e_fec_avail_10x_25x_handler, "CU",
		    "Get available FEC bits for 10G_40G, 25G_50G, 56G, 100G respectivly. "
		    "0:Auto "
		    "1:NOFEC "
		    "2:FIRECODE "
		    "4:RS");

		SYSCTL_ADD_PROC(&priv->sysctl_ctx, SYSCTL_CHILDREN(fec_node), OID_AUTO,
		    "mask_50x", CTLTYPE_U16 | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
		    priv, 0, &mlx5e_fec_mask_50x_handler, "SU",
		    "Set FEC masks for 50G 1x, 100G 2x, 200G 4x, 400G 8x respectivly. "
		    "0:Auto "
		    "128:RS "
		    "512:LL RS");

		SYSCTL_ADD_PROC(&priv->sysctl_ctx, SYSCTL_CHILDREN(fec_node), OID_AUTO,
		    "avail_50x", CTLTYPE_U16 | CTLFLAG_RD | CTLFLAG_MPSAFE,
		    priv, 0, &mlx5e_fec_avail_50x_handler, "SU",
		    "Get available FEC bits for 50G 1x, 100G 2x, 200G 4x, 400G 8x respectivly. "
		    "0:Auto "
		    "128:RS "
		    "512:LL RS");
	}

	SYSCTL_ADD_PROC(&priv->sysctl_ctx, SYSCTL_CHILDREN(node), OID_AUTO,
	    "debug_stats", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, priv,
	    0, &mlx5e_ethtool_debug_stats, "I", "Extended debug statistics");

	pnameunit = device_get_nameunit(priv->mdev->pdev->dev.bsddev);

	SYSCTL_ADD_STRING(&priv->sysctl_ctx, SYSCTL_CHILDREN(node),
	    OID_AUTO, "device_name", CTLFLAG_RD,
	    __DECONST(void *, pnameunit), 0,
	    "PCI device name");

	/* Diagnostics support */
	mlx5e_create_diagnostics(priv);

	/* create qos node */
	qos_node = SYSCTL_ADD_NODE(&priv->sysctl_ctx,
	    SYSCTL_CHILDREN(node), OID_AUTO,
	    "qos", CTLFLAG_RW | CTLFLAG_MPSAFE, NULL,
	    "Quality Of Service configuration");
	if (qos_node == NULL)
		return;

	/* Priority rate limit support */
	if (mlx5e_getmaxrate(priv) == 0) {
		SYSCTL_ADD_PROC(&priv->sysctl_ctx, SYSCTL_CHILDREN(qos_node),
		    OID_AUTO, "tc_max_rate", CTLTYPE_U64 | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
		    priv, 0, mlx5e_tc_maxrate_handler, "QU",
		    "Max rate for priority, specified in kilobits, where kilo=1000, "
		    "max_rate must be divisible by 100000");
	}

	/* Bandwidth limiting by ratio */
	if (mlx5e_get_max_alloc(priv) == 0) {
		SYSCTL_ADD_PROC(&priv->sysctl_ctx, SYSCTL_CHILDREN(qos_node),
		    OID_AUTO, "tc_rate_share", CTLTYPE_U8 | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
		    priv, 0, mlx5e_tc_rate_share_handler, "QU",
		    "Specify bandwidth ratio from 1 to 100 "
		    "for the available traffic classes");
	}

	/* Priority to traffic class mapping */
	if (mlx5e_get_prio_tc(priv) == 0) {
		SYSCTL_ADD_PROC(&priv->sysctl_ctx, SYSCTL_CHILDREN(qos_node),
		    OID_AUTO, "prio_0_7_tc", CTLTYPE_U8 | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
		    priv, 0, mlx5e_prio_to_tc_handler, "CU",
		    "Set traffic class 0 to 7 for priority 0 to 7 inclusivly");
	}

	/* DSCP support */
	if (mlx5e_get_dscp(priv) == 0) {
		for (i = 0; i != MLX5_MAX_SUPPORTED_DSCP; i += 8) {
			char name[32];
			snprintf(name, sizeof(name), "dscp_%d_%d_prio", i, i + 7);
			SYSCTL_ADD_PROC(&priv->sysctl_ctx, SYSCTL_CHILDREN(qos_node),
				OID_AUTO, name, CTLTYPE_U8 | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
				priv, i, mlx5e_dscp_prio_handler, "CU",
				"Set DSCP to priority mapping, 0..7");
		}
#define	A	"Set trust state, 1:PCP 2:DSCP"
#define	B	" 3:BOTH"
		SYSCTL_ADD_PROC(&priv->sysctl_ctx, SYSCTL_CHILDREN(qos_node),
		    OID_AUTO, "trust_state", CTLTYPE_U8 | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
		    priv, 0, mlx5e_trust_state_handler, "CU",
		    MLX5_CAP_QCAM_FEATURE(priv->mdev, qpts_trust_both) ?
		    A B : A);
#undef B
#undef A
	}

	if (mlx5e_port_query_buffer(priv, &port_buffer) == 0) {
		SYSCTL_ADD_PROC(&priv->sysctl_ctx, SYSCTL_CHILDREN(qos_node),
		    OID_AUTO, "buffers_size",
		    CTLTYPE_U32 | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
		    priv, 0, mlx5e_buf_size_handler, "IU",
		    "Set buffers sizes");
		SYSCTL_ADD_PROC(&priv->sysctl_ctx, SYSCTL_CHILDREN(qos_node),
		    OID_AUTO, "buffers_prio",
		    CTLTYPE_U8 | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
		    priv, 0, mlx5e_buf_prio_handler, "CU",
		    "Set prio to buffers mapping");
		SYSCTL_ADD_PROC(&priv->sysctl_ctx, SYSCTL_CHILDREN(qos_node),
		    OID_AUTO, "cable_length",
		    CTLTYPE_UINT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
		    priv, 0, mlx5e_cable_length_handler, "IU",
		    "Set cable length in meters for xoff threshold calculation");
	}

	if (mlx5e_hw_temperature_update(priv) == 0) {
		SYSCTL_ADD_PROC(&priv->sysctl_ctx, SYSCTL_CHILDREN(priv->sysctl_ifnet),
		    OID_AUTO, "hw_temperature",
		    CTLTYPE_S32 | CTLFLAG_RD | CTLFLAG_MPSAFE,
		    priv, 0, mlx5e_hw_temperature_handler, "I",
		    "HW temperature in millicelsius");
	}
}
