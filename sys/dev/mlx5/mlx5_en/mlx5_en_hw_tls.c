/*-
 * Copyright (c) 2019-2021 Mellanox Technologies. All rights reserved.
 * Copyright (c) 2022 NVIDIA corporation & affiliates.
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

#include "opt_kern_tls.h"
#include "opt_rss.h"
#include "opt_ratelimit.h"

#include <dev/mlx5/mlx5_en/en.h>

#include <dev/mlx5/tls.h>
#include <dev/mlx5/crypto.h>

#include <linux/delay.h>
#include <sys/ktls.h>
#include <opencrypto/cryptodev.h>

#ifdef KERN_TLS

#ifdef RATELIMIT
static if_snd_tag_modify_t mlx5e_tls_rl_snd_tag_modify;
#endif
static if_snd_tag_query_t mlx5e_tls_snd_tag_query;
static if_snd_tag_free_t mlx5e_tls_snd_tag_free;

static const struct if_snd_tag_sw mlx5e_tls_snd_tag_sw = {
	.snd_tag_query = mlx5e_tls_snd_tag_query,
	.snd_tag_free = mlx5e_tls_snd_tag_free,
	.type = IF_SND_TAG_TYPE_TLS
};

#ifdef RATELIMIT
static const struct if_snd_tag_sw mlx5e_tls_rl_snd_tag_sw = {
	.snd_tag_modify = mlx5e_tls_rl_snd_tag_modify,
	.snd_tag_query = mlx5e_tls_snd_tag_query,
	.snd_tag_free = mlx5e_tls_snd_tag_free,
	.type = IF_SND_TAG_TYPE_TLS_RATE_LIMIT
};
#endif

MALLOC_DEFINE(M_MLX5E_TLS, "MLX5E_TLS", "MLX5 ethernet HW TLS");

/* software TLS context */
struct mlx5_ifc_sw_tls_cntx_bits {
	struct mlx5_ifc_tls_static_params_bits param;
	struct mlx5_ifc_tls_progress_params_bits progress;
	struct {
		uint8_t key_data[8][0x20];
		uint8_t key_len[0x20];
	} key;
};

CTASSERT(MLX5_ST_SZ_BYTES(sw_tls_cntx) <= sizeof(((struct mlx5e_tls_tag *)0)->crypto_params));
CTASSERT(MLX5_ST_SZ_BYTES(mkc) == sizeof(((struct mlx5e_tx_umr_wqe *)0)->mkc));

static const char *mlx5e_tls_stats_desc[] = {
	MLX5E_TLS_STATS(MLX5E_STATS_DESC)
};

static void mlx5e_tls_work(struct work_struct *);

/*
 * Expand the tls tag UMA zone in a sleepable context
 */

static void
mlx5e_prealloc_tags(struct mlx5e_priv *priv, int nitems)
{
	struct mlx5e_tls_tag **tags;
	int i;

	tags = malloc(sizeof(tags[0]) * nitems,
	    M_MLX5E_TLS, M_WAITOK);
	for (i = 0; i < nitems; i++)
		tags[i] = uma_zalloc(priv->tls.zone, M_WAITOK);
	__compiler_membar();
	for (i = 0; i < nitems; i++)
		uma_zfree(priv->tls.zone, tags[i]);
	free(tags, M_MLX5E_TLS);
}

static int
mlx5e_tls_tag_import(void *arg, void **store, int cnt, int domain, int flags)
{
	struct mlx5e_tls_tag *ptag;
	struct mlx5e_priv *priv = arg;
	int err, i;

	/*
	 * mlx5_tls_open_tis() sleeps on a firmware command, so
	 * zone allocations must be done from a sleepable context.
	 * Note that the uma_zalloc() in mlx5e_tls_snd_tag_alloc()
	 * is done with M_NOWAIT so that hitting the zone limit does
	 * not cause the allocation to pause forever.
	 */

	for (i = 0; i != cnt; i++) {
		ptag = malloc_domainset(sizeof(*ptag), M_MLX5E_TLS,
		    mlx5_dev_domainset(priv->mdev), flags | M_ZERO);
		if (ptag == NULL)
			return (i);
		ptag->tls = &priv->tls;
		mtx_init(&ptag->mtx, "mlx5-tls-tag-mtx", NULL, MTX_DEF);
		INIT_WORK(&ptag->work, mlx5e_tls_work);
		err = mlx5_tls_open_tis(priv->mdev, 0, priv->tdn,
		    priv->pdn, &ptag->tisn);
		if (err) {
			MLX5E_TLS_STAT_INC(ptag, tx_error, 1);
			free(ptag, M_MLX5E_TLS);
			return (i);
		}

		store[i] = ptag;
	}
	return (i);
}

static void
mlx5e_tls_tag_release(void *arg, void **store, int cnt)
{
	struct mlx5e_tls_tag *ptag;
	struct mlx5e_priv *priv;
	struct mlx5e_tls *ptls;
	int i;

	for (i = 0; i != cnt; i++) {
		ptag = store[i];
		ptls = ptag->tls;
		priv = container_of(ptls, struct mlx5e_priv, tls);

		flush_work(&ptag->work);

		if (ptag->tisn != 0) {
			mlx5_tls_close_tis(priv->mdev, ptag->tisn);
		}

		mtx_destroy(&ptag->mtx);

		free(ptag, M_MLX5E_TLS);
	}
}

static void
mlx5e_tls_tag_zfree(struct mlx5e_tls_tag *ptag)
{
	/* make sure any unhandled taskqueue events are ignored */
	ptag->state = MLX5E_TLS_ST_FREED;

	/* reset some variables */
	ptag->dek_index = 0;
	ptag->dek_index_ok = 0;

	/* avoid leaking keys */
	memset(ptag->crypto_params, 0, sizeof(ptag->crypto_params));

	/* return tag to UMA */
	uma_zfree(ptag->tls->zone, ptag);
}

static int
mlx5e_max_tag_proc(SYSCTL_HANDLER_ARGS)
{
	struct mlx5e_priv *priv = (struct mlx5e_priv *)arg1;
	struct mlx5e_tls *ptls = &priv->tls;
	int err;
	unsigned int max_tags;

	max_tags = ptls->zone_max;
	err = sysctl_handle_int(oidp, &max_tags, arg2, req);
	if (err != 0 || req->newptr == NULL )
		return err;
	if (max_tags == ptls->zone_max)
		return 0;
	if (max_tags > priv->tls.max_resources || max_tags == 0)
		return (EINVAL);
	ptls->zone_max = max_tags;
	uma_zone_set_max(ptls->zone, ptls->zone_max);
	return 0;
}

int
mlx5e_tls_init(struct mlx5e_priv *priv)
{
	struct mlx5e_tls *ptls = &priv->tls;
	struct sysctl_oid *node;
	uint32_t max_dek, max_tis, x;
	int zone_max = 0, prealloc_tags = 0;

	if (MLX5_CAP_GEN(priv->mdev, tls_tx) == 0 ||
	    MLX5_CAP_GEN(priv->mdev, log_max_dek) == 0)
		return (0);

	ptls->wq = create_singlethread_workqueue("mlx5-tls-wq");
	if (ptls->wq == NULL)
		return (ENOMEM);

	sysctl_ctx_init(&ptls->ctx);

	snprintf(ptls->zname, sizeof(ptls->zname),
	    "mlx5_%u_tls", device_get_unit(priv->mdev->pdev->dev.bsddev));


	TUNABLE_INT_FETCH("hw.mlx5.tls_max_tags", &zone_max);
	TUNABLE_INT_FETCH("hw.mlx5.tls_prealloc_tags", &prealloc_tags);

	ptls->zone = uma_zcache_create(ptls->zname,
	     sizeof(struct mlx5e_tls_tag), NULL, NULL, NULL, NULL,
	     mlx5e_tls_tag_import, mlx5e_tls_tag_release, priv,
	     UMA_ZONE_UNMANAGED | (prealloc_tags ? UMA_ZONE_NOFREE : 0));

	/* shared between RX and TX TLS */
	max_dek = 1U << (MLX5_CAP_GEN(priv->mdev, log_max_dek) - 1);
	max_tis = 1U << (MLX5_CAP_GEN(priv->mdev, log_max_tis) - 1);
	ptls->max_resources = MIN(max_dek, max_tis);

	if (zone_max != 0) {
		ptls->zone_max = zone_max;
		if (ptls->zone_max > priv->tls.max_resources)
			ptls->zone_max = priv->tls.max_resources;
	} else {
		ptls->zone_max = priv->tls.max_resources;
	}

	uma_zone_set_max(ptls->zone, ptls->zone_max);
	if (prealloc_tags != 0)
		mlx5e_prealloc_tags(priv, ptls->zone_max);

	for (x = 0; x != MLX5E_TLS_STATS_NUM; x++)
		ptls->stats.arg[x] = counter_u64_alloc(M_WAITOK);

	ptls->init = 1;

	node = SYSCTL_ADD_NODE(&priv->sysctl_ctx,
	    SYSCTL_CHILDREN(priv->sysctl_ifnet), OID_AUTO,
	    "tls", CTLFLAG_RW | CTLFLAG_MPSAFE, NULL, "Hardware TLS offload");
	if (node == NULL)
		return (0);

	SYSCTL_ADD_PROC(&priv->sysctl_ctx, SYSCTL_CHILDREN(node), OID_AUTO, "tls_max_tag",
	    CTLFLAG_RW | CTLTYPE_UINT | CTLFLAG_MPSAFE, priv, 0, mlx5e_max_tag_proc,
	    "IU", "Max number of TLS offload session tags");

	mlx5e_create_counter_stats(&ptls->ctx,
	    SYSCTL_CHILDREN(node), "stats",
	    mlx5e_tls_stats_desc, MLX5E_TLS_STATS_NUM,
	    ptls->stats.arg);

	return (0);
}

void
mlx5e_tls_cleanup(struct mlx5e_priv *priv)
{
	struct mlx5e_tls *ptls = &priv->tls;
	uint32_t x;

	if (ptls->init == 0)
		return;

	ptls->init = 0;
	flush_workqueue(ptls->wq);
	sysctl_ctx_free(&ptls->ctx);
	uma_zdestroy(ptls->zone);
	destroy_workqueue(ptls->wq);

	for (x = 0; x != MLX5E_TLS_STATS_NUM; x++)
		counter_u64_free(ptls->stats.arg[x]);
}


static int
mlx5e_tls_st_init(struct mlx5e_priv *priv, struct mlx5e_tls_tag *ptag)
{
	int err;

	/* try to open TIS, if not present */
	if (ptag->tisn == 0) {
		err = mlx5_tls_open_tis(priv->mdev, 0, priv->tdn,
		    priv->pdn, &ptag->tisn);
		if (err) {
			MLX5E_TLS_STAT_INC(ptag, tx_error, 1);
			return (-err);
		}
	}
	MLX5_SET(sw_tls_cntx, ptag->crypto_params, progress.pd, ptag->tisn);

	/* try to allocate a DEK context ID */
	err = mlx5_encryption_key_create(priv->mdev, priv->pdn,
	    MLX5_GENERAL_OBJECT_TYPE_ENCRYPTION_KEY_TYPE_TLS,
	    MLX5_ADDR_OF(sw_tls_cntx, ptag->crypto_params, key.key_data),
	    MLX5_GET(sw_tls_cntx, ptag->crypto_params, key.key_len),
	    &ptag->dek_index);
	if (err) {
		MLX5E_TLS_STAT_INC(ptag, tx_error, 1);
		return (-err);
	}

	MLX5_SET(sw_tls_cntx, ptag->crypto_params, param.dek_index, ptag->dek_index);

	ptag->dek_index_ok = 1;

	MLX5E_TLS_TAG_LOCK(ptag);
	if (ptag->state == MLX5E_TLS_ST_INIT)
		ptag->state = MLX5E_TLS_ST_SETUP;
	MLX5E_TLS_TAG_UNLOCK(ptag);
	return (0);
}

static void
mlx5e_tls_work(struct work_struct *work)
{
	struct mlx5e_tls_tag *ptag;
	struct mlx5e_priv *priv;

	ptag = container_of(work, struct mlx5e_tls_tag, work);
	priv = container_of(ptag->tls, struct mlx5e_priv, tls);

	switch (ptag->state) {
	case MLX5E_TLS_ST_INIT:
		(void)mlx5e_tls_st_init(priv, ptag);
		break;

	case MLX5E_TLS_ST_RELEASE:
		/* try to destroy DEK context by ID */
		if (ptag->dek_index_ok)
			(void)mlx5_encryption_key_destroy(priv->mdev, ptag->dek_index);

		/* free tag */
		mlx5e_tls_tag_zfree(ptag);
		break;

	default:
		break;
	}
}

static int
mlx5e_tls_set_params(void *ctx, const struct tls_session_params *en)
{

	MLX5_SET(sw_tls_cntx, ctx, param.const_2, 2);
	if (en->tls_vminor == TLS_MINOR_VER_TWO)
		MLX5_SET(sw_tls_cntx, ctx, param.tls_version, 2); /* v1.2 */
	else
		MLX5_SET(sw_tls_cntx, ctx, param.tls_version, 3); /* v1.3 */
	MLX5_SET(sw_tls_cntx, ctx, param.const_1, 1);
	MLX5_SET(sw_tls_cntx, ctx, param.encryption_standard, 1); /* TLS */

	/* copy the initial vector in place */
	switch (en->iv_len) {
	case MLX5_FLD_SZ_BYTES(sw_tls_cntx, param.gcm_iv):
	case MLX5_FLD_SZ_BYTES(sw_tls_cntx, param.gcm_iv) +
	     MLX5_FLD_SZ_BYTES(sw_tls_cntx, param.implicit_iv):
		memcpy(MLX5_ADDR_OF(sw_tls_cntx, ctx, param.gcm_iv),
		    en->iv, en->iv_len);
		break;
	default:
		return (EINVAL);
	}

	if (en->cipher_key_len <= MLX5_FLD_SZ_BYTES(sw_tls_cntx, key.key_data)) {
		memcpy(MLX5_ADDR_OF(sw_tls_cntx, ctx, key.key_data),
		    en->cipher_key, en->cipher_key_len);
		MLX5_SET(sw_tls_cntx, ctx, key.key_len, en->cipher_key_len);
	} else {
		return (EINVAL);
	}
	return (0);
}

/* Verify zero default */
CTASSERT(MLX5E_TLS_ST_INIT == 0);

int
mlx5e_tls_snd_tag_alloc(if_t ifp, union if_snd_tag_alloc_params *params,
    struct m_snd_tag **ppmt)
{
	union if_snd_tag_alloc_params rl_params;
	const struct if_snd_tag_sw *snd_tag_sw;
	struct mlx5e_priv *priv;
	struct mlx5e_tls_tag *ptag;
	const struct tls_session_params *en;
	int error;

	priv = if_getsoftc(ifp);

	if (priv->gone != 0 || priv->tls.init == 0)
		return (EOPNOTSUPP);

	ptag = uma_zalloc(priv->tls.zone, M_NOWAIT);
 	if (ptag == NULL)
 		return (ENOMEM);

	/* sanity check default values */
	MPASS(ptag->dek_index == 0);
	MPASS(ptag->dek_index_ok == 0);

	/* check if there is no TIS context */
	KASSERT(ptag->tisn != 0, ("ptag %p w/0 tisn", ptag));

	en = &params->tls.tls->params;

	/* only TLS v1.2 and v1.3 is currently supported */
	if (en->tls_vmajor != TLS_MAJOR_VER_ONE ||
	    (en->tls_vminor != TLS_MINOR_VER_TWO
#ifdef TLS_MINOR_VER_THREE
	     && en->tls_vminor != TLS_MINOR_VER_THREE
#endif
	     )) {
		error = EPROTONOSUPPORT;
		goto failure;
	}

	switch (en->cipher_algorithm) {
	case CRYPTO_AES_NIST_GCM_16:
		switch (en->cipher_key_len) {
		case 128 / 8:
			if (en->tls_vminor == TLS_MINOR_VER_TWO) {
				if (MLX5_CAP_TLS(priv->mdev, tls_1_2_aes_gcm_128) == 0) {
					error = EPROTONOSUPPORT;
					goto failure;
				}
			} else {
				if (MLX5_CAP_TLS(priv->mdev, tls_1_3_aes_gcm_128) == 0) {
					error = EPROTONOSUPPORT;
					goto failure;
				}
			}
			error = mlx5e_tls_set_params(ptag->crypto_params, en);
			if (error)
				goto failure;
			break;

		case 256 / 8:
			if (en->tls_vminor == TLS_MINOR_VER_TWO) {
				if (MLX5_CAP_TLS(priv->mdev, tls_1_2_aes_gcm_256) == 0) {
					error = EPROTONOSUPPORT;
					goto failure;
				}
			} else {
				if (MLX5_CAP_TLS(priv->mdev, tls_1_3_aes_gcm_256) == 0) {
					error = EPROTONOSUPPORT;
					goto failure;
				}
			}
			error = mlx5e_tls_set_params(ptag->crypto_params, en);
			if (error)
				goto failure;
			break;

		default:
			error = EINVAL;
			goto failure;
		}
		break;
	default:
		error = EPROTONOSUPPORT;
		goto failure;
	}

	memset(&rl_params, 0, sizeof(rl_params));
	rl_params.hdr = params->hdr;
	switch (params->hdr.type) {
#ifdef RATELIMIT
	case IF_SND_TAG_TYPE_TLS_RATE_LIMIT:
		rl_params.hdr.type = IF_SND_TAG_TYPE_RATE_LIMIT;
		rl_params.rate_limit.max_rate = params->tls_rate_limit.max_rate;
		snd_tag_sw = &mlx5e_tls_rl_snd_tag_sw;
		break;
#endif
	case IF_SND_TAG_TYPE_TLS:
		rl_params.hdr.type = IF_SND_TAG_TYPE_UNLIMITED;
		snd_tag_sw = &mlx5e_tls_snd_tag_sw;
		break;
	default:
		error = EOPNOTSUPP;
		goto failure;
	}

	error = m_snd_tag_alloc(ifp, &rl_params, &ptag->rl_tag);
	if (error)
		goto failure;

	/* store pointer to mbuf tag */
	MPASS(ptag->tag.refcount == 0);
	m_snd_tag_init(&ptag->tag, ifp, snd_tag_sw);
	*ppmt = &ptag->tag;

	/* reset state */
	ptag->state = MLX5E_TLS_ST_INIT;

	error = mlx5e_tls_st_init(priv, ptag);
	if (error != 0)
		goto failure;

	return (0);

failure:
	mlx5e_tls_tag_zfree(ptag);
	return (error);
}

#ifdef RATELIMIT
static int
mlx5e_tls_rl_snd_tag_modify(struct m_snd_tag *pmt, union if_snd_tag_modify_params *params)
{
	union if_snd_tag_modify_params rl_params;
	struct mlx5e_tls_tag *ptag =
	    container_of(pmt, struct mlx5e_tls_tag, tag);
	int error;

	memset(&rl_params, 0, sizeof(rl_params));
	rl_params.rate_limit.max_rate = params->tls_rate_limit.max_rate;
	error = ptag->rl_tag->sw->snd_tag_modify(ptag->rl_tag, &rl_params);
	return (error);
}
#endif

static int
mlx5e_tls_snd_tag_query(struct m_snd_tag *pmt, union if_snd_tag_query_params *params)
{
	struct mlx5e_tls_tag *ptag =
	    container_of(pmt, struct mlx5e_tls_tag, tag);

	return (ptag->rl_tag->sw->snd_tag_query(ptag->rl_tag, params));
}

static void
mlx5e_tls_snd_tag_free(struct m_snd_tag *pmt)
{
	struct mlx5e_tls_tag *ptag =
	    container_of(pmt, struct mlx5e_tls_tag, tag);
	struct mlx5e_priv *priv;

	m_snd_tag_rele(ptag->rl_tag);

	MLX5E_TLS_TAG_LOCK(ptag);
	ptag->state = MLX5E_TLS_ST_RELEASE;
	MLX5E_TLS_TAG_UNLOCK(ptag);

	priv = if_getsoftc(ptag->tag.ifp);
	queue_work(priv->tls.wq, &ptag->work);
}

CTASSERT((MLX5_FLD_SZ_BYTES(sw_tls_cntx, param) % 16) == 0);

static void
mlx5e_tls_send_static_parameters(struct mlx5e_sq *sq, struct mlx5e_tls_tag *ptag)
{
	const u32 ds_cnt = DIV_ROUND_UP(sizeof(struct mlx5e_tx_umr_wqe) +
	    MLX5_FLD_SZ_BYTES(sw_tls_cntx, param), MLX5_SEND_WQE_DS);
	struct mlx5e_tx_umr_wqe *wqe;
	u16 pi;

	pi = sq->pc & sq->wq.sz_m1;
	wqe = mlx5_wq_cyc_get_wqe(&sq->wq, pi);

	memset(wqe, 0, sizeof(*wqe));

	wqe->ctrl.opmod_idx_opcode = cpu_to_be32((sq->pc << 8) |
	    MLX5_OPCODE_UMR | (MLX5_OPCODE_MOD_UMR_TLS_TIS_STATIC_PARAMS << 24));
	wqe->ctrl.qpn_ds = cpu_to_be32((sq->sqn << 8) | ds_cnt);
	wqe->ctrl.imm = cpu_to_be32(ptag->tisn << 8);

	if (mlx5e_do_send_cqe(sq))
		wqe->ctrl.fm_ce_se = MLX5_WQE_CTRL_CQ_UPDATE | MLX5_FENCE_MODE_INITIATOR_SMALL;
	else
		wqe->ctrl.fm_ce_se = MLX5_FENCE_MODE_INITIATOR_SMALL;

	/* fill out UMR control segment */
	wqe->umr.flags = 0x80;	/* inline data */
	wqe->umr.bsf_octowords = cpu_to_be16(MLX5_FLD_SZ_BYTES(sw_tls_cntx, param) / 16);

	/* copy in the static crypto parameters */
	memcpy(wqe + 1, MLX5_ADDR_OF(sw_tls_cntx, ptag->crypto_params, param),
	    MLX5_FLD_SZ_BYTES(sw_tls_cntx, param));

	/* copy data for doorbell */
	memcpy(sq->doorbell.d32, &wqe->ctrl, sizeof(sq->doorbell.d32));

	sq->mbuf[pi].mbuf = NULL;
	sq->mbuf[pi].num_bytes = 0;
	sq->mbuf[pi].num_wqebbs = DIV_ROUND_UP(ds_cnt, MLX5_SEND_WQEBB_NUM_DS);
	sq->mbuf[pi].mst = m_snd_tag_ref(&ptag->tag);

	sq->pc += sq->mbuf[pi].num_wqebbs;
}

CTASSERT(MLX5_FLD_SZ_BYTES(sw_tls_cntx, progress) ==
    sizeof(((struct mlx5e_tx_psv_wqe *)0)->psv));

static void
mlx5e_tls_send_progress_parameters(struct mlx5e_sq *sq, struct mlx5e_tls_tag *ptag)
{
	const u32 ds_cnt = DIV_ROUND_UP(sizeof(struct mlx5e_tx_psv_wqe),
	    MLX5_SEND_WQE_DS);
	struct mlx5e_tx_psv_wqe *wqe;
	u16 pi;

	pi = sq->pc & sq->wq.sz_m1;
	wqe = mlx5_wq_cyc_get_wqe(&sq->wq, pi);

	memset(wqe, 0, sizeof(*wqe));

	wqe->ctrl.opmod_idx_opcode = cpu_to_be32((sq->pc << 8) |
	    MLX5_OPCODE_SET_PSV | (MLX5_OPCODE_MOD_PSV_TLS_TIS_PROGRESS_PARAMS << 24));
	wqe->ctrl.qpn_ds = cpu_to_be32((sq->sqn << 8) | ds_cnt);

	if (mlx5e_do_send_cqe(sq))
		wqe->ctrl.fm_ce_se = MLX5_WQE_CTRL_CQ_UPDATE;

	/* copy in the PSV control segment */
	memcpy(&wqe->psv, MLX5_ADDR_OF(sw_tls_cntx, ptag->crypto_params, progress),
	    sizeof(wqe->psv));

	/* copy data for doorbell */
	memcpy(sq->doorbell.d32, &wqe->ctrl, sizeof(sq->doorbell.d32));

	sq->mbuf[pi].mbuf = NULL;
	sq->mbuf[pi].num_bytes = 0;
	sq->mbuf[pi].num_wqebbs = DIV_ROUND_UP(ds_cnt, MLX5_SEND_WQEBB_NUM_DS);
	sq->mbuf[pi].mst = m_snd_tag_ref(&ptag->tag);

	sq->pc += sq->mbuf[pi].num_wqebbs;
}

static void
mlx5e_tls_send_nop(struct mlx5e_sq *sq, struct mlx5e_tls_tag *ptag)
{
	const u32 ds_cnt = MLX5_SEND_WQEBB_NUM_DS;
	struct mlx5e_tx_wqe *wqe;
	u16 pi;

	pi = sq->pc & sq->wq.sz_m1;
	wqe = mlx5_wq_cyc_get_wqe(&sq->wq, pi);

	memset(&wqe->ctrl, 0, sizeof(wqe->ctrl));

	wqe->ctrl.opmod_idx_opcode = cpu_to_be32((sq->pc << 8) | MLX5_OPCODE_NOP);
	wqe->ctrl.qpn_ds = cpu_to_be32((sq->sqn << 8) | ds_cnt);
	if (mlx5e_do_send_cqe(sq))
		wqe->ctrl.fm_ce_se = MLX5_WQE_CTRL_CQ_UPDATE | MLX5_FENCE_MODE_INITIATOR_SMALL;
	else
		wqe->ctrl.fm_ce_se = MLX5_FENCE_MODE_INITIATOR_SMALL;

	/* Copy data for doorbell */
	memcpy(sq->doorbell.d32, &wqe->ctrl, sizeof(sq->doorbell.d32));

	sq->mbuf[pi].mbuf = NULL;
	sq->mbuf[pi].num_bytes = 0;
	sq->mbuf[pi].num_wqebbs = DIV_ROUND_UP(ds_cnt, MLX5_SEND_WQEBB_NUM_DS);
	sq->mbuf[pi].mst = m_snd_tag_ref(&ptag->tag);

	sq->pc += sq->mbuf[pi].num_wqebbs;
}

#define	SBTLS_MBUF_NO_DATA ((struct mbuf *)1)

static struct mbuf *
sbtls_recover_record(struct mbuf *mb, int wait, uint32_t tcp_old, uint32_t *ptcp_seq, bool *pis_start)
{
	struct mbuf *mr, *top;
	uint32_t offset;
	uint32_t delta;

	/* check format of incoming mbuf */
	if (mb->m_next == NULL ||
	    (mb->m_next->m_flags & (M_EXTPG | M_EXT)) != (M_EXTPG | M_EXT)) {
		top = NULL;
		goto done;
	}

	/* get unmapped data offset */
	offset = mtod(mb->m_next, uintptr_t);

	/* check if we don't need to re-transmit anything */
	if (offset == 0) {
		top = SBTLS_MBUF_NO_DATA;
		*pis_start = true;
		goto done;
	}

	/* try to get a new  packet header */
	top = m_gethdr(wait, MT_DATA);
	if (top == NULL)
		goto done;

	mr = m_get(wait, MT_DATA);
	if (mr == NULL) {
		m_free(top);
		top = NULL;
		goto done;
	}

	top->m_next = mr;

	mb_dupcl(mr, mb->m_next);

	/* the beginning of the TLS record */
	mr->m_data = NULL;

	/* setup packet header length */
	top->m_pkthdr.len = mr->m_len = offset;
	top->m_len = 0;

	/* check for partial re-transmit */
	delta = *ptcp_seq - tcp_old;

	if (delta < offset) {
		m_adj(top, offset - delta);
		offset = delta;

		/* continue where we left off */
		*pis_start = false;
	} else {
		*pis_start = true;
	}

	/*
	 * Rewind the TCP sequence number by the amount of data
	 * retransmitted:
	 */
	*ptcp_seq -= offset;
done:
	return (top);
}

static int
mlx5e_sq_tls_populate(struct mbuf *mb, uint64_t *pseq)
{

	for (; mb != NULL; mb = mb->m_next) {
		if (!(mb->m_flags & M_EXTPG))
			continue;
		*pseq = mb->m_epg_seqno;
		return (1);
	}
	return (0);
}

int
mlx5e_sq_tls_xmit(struct mlx5e_sq *sq, struct mlx5e_xmit_args *parg, struct mbuf **ppmb)
{
	struct mlx5e_tls_tag *ptls_tag;
	struct m_snd_tag *ptag;
	const struct tcphdr *th;
	struct mbuf *mb = *ppmb;
	u64 rcd_sn;
	u32 header_size;
	u32 mb_seq;

	if ((mb->m_pkthdr.csum_flags & CSUM_SND_TAG) == 0)
		return (MLX5E_TLS_CONTINUE);

	ptag = mb->m_pkthdr.snd_tag;

	if (
#ifdef RATELIMIT
	    ptag->sw->type != IF_SND_TAG_TYPE_TLS_RATE_LIMIT &&
#endif
	    ptag->sw->type != IF_SND_TAG_TYPE_TLS)
		return (MLX5E_TLS_CONTINUE);

	ptls_tag = container_of(ptag, struct mlx5e_tls_tag, tag);

	header_size = mlx5e_get_full_header_size(mb, &th);
	if (unlikely(header_size == 0 || th == NULL))
		return (MLX5E_TLS_FAILURE);

	/*
	 * Send non-TLS TCP packets AS-IS:
	 */
	if (header_size == mb->m_pkthdr.len ||
	    mlx5e_sq_tls_populate(mb, &rcd_sn) == 0) {
		parg->tisn = 0;
		parg->ihs = header_size;
		return (MLX5E_TLS_CONTINUE);
	}

	mb_seq = ntohl(th->th_seq);

	MLX5E_TLS_TAG_LOCK(ptls_tag);
	switch (ptls_tag->state) {
	case MLX5E_TLS_ST_INIT:
		MLX5E_TLS_TAG_UNLOCK(ptls_tag);
		return (MLX5E_TLS_FAILURE);
	case MLX5E_TLS_ST_SETUP:
		ptls_tag->state = MLX5E_TLS_ST_TXRDY;
		ptls_tag->expected_seq = ~mb_seq;	/* force setup */
	default:
		MLX5E_TLS_TAG_UNLOCK(ptls_tag);
		break;
	}

	if (unlikely(ptls_tag->expected_seq != mb_seq)) {
		bool is_start;
		struct mbuf *r_mb;
		uint32_t tcp_seq = mb_seq;

		r_mb = sbtls_recover_record(mb, M_NOWAIT, ptls_tag->expected_seq, &tcp_seq, &is_start);
		if (r_mb == NULL) {
			MLX5E_TLS_STAT_INC(ptls_tag, tx_error, 1);
			return (MLX5E_TLS_FAILURE);
		}

		MLX5E_TLS_STAT_INC(ptls_tag, tx_packets_ooo, 1);

		/* check if this is the first fragment of a TLS record */
		if (is_start) {
			/* setup TLS static parameters */
			MLX5_SET64(sw_tls_cntx, ptls_tag->crypto_params,
			    param.initial_record_number, rcd_sn);

			/*
			 * NOTE: The sendqueue should have enough room to
			 * carry both the static and the progress parameters
			 * when we get here!
			 */
			mlx5e_tls_send_static_parameters(sq, ptls_tag);
			mlx5e_tls_send_progress_parameters(sq, ptls_tag);

			if (r_mb == SBTLS_MBUF_NO_DATA) {
				mlx5e_tls_send_nop(sq, ptls_tag);
				ptls_tag->expected_seq = mb_seq;
				return (MLX5E_TLS_LOOP);
			}
		}

		MLX5E_TLS_STAT_INC(ptls_tag, tx_bytes_ooo, r_mb->m_pkthdr.len);

		/* setup transmit arguments */
		parg->tisn = ptls_tag->tisn;
		parg->mst = &ptls_tag->tag;

		/* try to send DUMP data */
		if (mlx5e_sq_dump_xmit(sq, parg, &r_mb) != 0) {
			m_freem(r_mb);
			ptls_tag->expected_seq = tcp_seq;
			return (MLX5E_TLS_FAILURE);
		} else {
			ptls_tag->expected_seq = mb_seq;
			return (MLX5E_TLS_LOOP);
		}
	} else {
		MLX5E_TLS_STAT_INC(ptls_tag, tx_packets, 1);
		MLX5E_TLS_STAT_INC(ptls_tag, tx_bytes, mb->m_pkthdr.len);
	}
	ptls_tag->expected_seq += mb->m_pkthdr.len - header_size;

	parg->tisn = ptls_tag->tisn;
	parg->ihs = header_size;
	parg->mst = &ptls_tag->tag;
	return (MLX5E_TLS_CONTINUE);
}

#else

int
mlx5e_tls_init(struct mlx5e_priv *priv)
{

	return (0);
}

void
mlx5e_tls_cleanup(struct mlx5e_priv *priv)
{
	/* NOP */
}

#endif		/* KERN_TLS */
