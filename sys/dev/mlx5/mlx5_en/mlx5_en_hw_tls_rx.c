/*-
 * Copyright (c) 2021-2022 NVIDIA corporation & affiliates.
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

#include <dev/mlx5/crypto.h>
#include <dev/mlx5/tls.h>

#include <dev/mlx5/fs.h>
#include <dev/mlx5/mlx5_core/fs_tcp.h>

#include <sys/ktls.h>
#include <opencrypto/cryptodev.h>

#ifdef KERN_TLS

static if_snd_tag_free_t mlx5e_tls_rx_snd_tag_free;
static if_snd_tag_modify_t mlx5e_tls_rx_snd_tag_modify;
static if_snd_tag_status_str_t mlx5e_tls_rx_snd_tag_status_str;

static const struct if_snd_tag_sw mlx5e_tls_rx_snd_tag_sw = {
	.snd_tag_modify = mlx5e_tls_rx_snd_tag_modify,
	.snd_tag_free = mlx5e_tls_rx_snd_tag_free,
	.snd_tag_status_str = mlx5e_tls_rx_snd_tag_status_str,
	.type = IF_SND_TAG_TYPE_TLS_RX
};

static const char *mlx5e_tls_rx_progress_params_auth_state_str[] = {
	[MLX5E_TLS_RX_PROGRESS_PARAMS_AUTH_STATE_NO_OFFLOAD] = "no_offload",
	[MLX5E_TLS_RX_PROGRESS_PARAMS_AUTH_STATE_OFFLOAD] = "offload",
	[MLX5E_TLS_RX_PROGRESS_PARAMS_AUTH_STATE_AUTHENTICATION] =
	    "authentication",
};

static const char *mlx5e_tls_rx_progress_params_record_tracker_state_str[] = {
	[MLX5E_TLS_RX_PROGRESS_PARAMS_RECORD_TRACKER_STATE_START] = "start",
	[MLX5E_TLS_RX_PROGRESS_PARAMS_RECORD_TRACKER_STATE_TRACKING] =
	    "tracking",
	[MLX5E_TLS_RX_PROGRESS_PARAMS_RECORD_TRACKER_STATE_SEARCHING] =
	    "searching",
};

MALLOC_DEFINE(M_MLX5E_TLS_RX, "MLX5E_TLS_RX", "MLX5 ethernet HW TLS RX");

/* software TLS RX context */
struct mlx5_ifc_sw_tls_rx_cntx_bits {
	struct mlx5_ifc_tls_static_params_bits param;
	struct mlx5_ifc_tls_progress_params_bits progress;
	struct {
		uint8_t key_data[8][0x20];
		uint8_t key_len[0x20];
	} key;
};

CTASSERT(MLX5_ST_SZ_BYTES(sw_tls_rx_cntx) <= sizeof(((struct mlx5e_tls_rx_tag *)NULL)->crypto_params));
CTASSERT(MLX5_ST_SZ_BYTES(mkc) == sizeof(((struct mlx5e_tx_umr_wqe *)NULL)->mkc));

static const char *mlx5e_tls_rx_stats_desc[] = {
	MLX5E_TLS_RX_STATS(MLX5E_STATS_DESC)
};

static void mlx5e_tls_rx_work(struct work_struct *);
static bool mlx5e_tls_rx_snd_tag_find_tcp_sn_and_tls_rcd(struct mlx5e_tls_rx_tag *,
    uint32_t, uint32_t *, uint64_t *);

CTASSERT((MLX5_FLD_SZ_BYTES(sw_tls_rx_cntx, param) % 16) == 0);

static uint32_t
mlx5e_tls_rx_get_ch(struct mlx5e_priv *priv, uint32_t flowid, uint32_t flowtype)
{
	u32 ch;
#ifdef RSS
	u32 temp;
#endif

	/* keep this code synced with mlx5e_select_queue() */
	ch = priv->params.num_channels;
#ifdef RSS
	if (rss_hash2bucket(flowid, flowtype, &temp) == 0)
		ch = temp % ch;
	else
#endif
		ch = (flowid % 128) % ch;
	return (ch);
}

/*
 * This function gets a pointer to an internal queue, IQ, based on the
 * provided "flowid" and "flowtype". The IQ returned may in some rare
 * cases not be activated or running, but this is all handled by the
 * "mlx5e_iq_get_producer_index()" function.
 *
 * The idea behind this function is to spread the IQ traffic as much
 * as possible and to avoid congestion on the same IQ when processing
 * RX traffic.
 */
static struct mlx5e_iq *
mlx5e_tls_rx_get_iq(struct mlx5e_priv *priv, uint32_t flowid, uint32_t flowtype)
{
	/*
	 * NOTE: The channels array is only freed at detach
	 * and it safe to return a pointer to the send tag
	 * inside the channels structure as long as we
	 * reference the priv.
	 */
	return (&priv->channel[mlx5e_tls_rx_get_ch(priv, flowid, flowtype)].iq);
}

static void
mlx5e_tls_rx_send_static_parameters_cb(void *arg)
{
	struct mlx5e_tls_rx_tag *ptag;

	ptag = (struct mlx5e_tls_rx_tag *)arg;

	m_snd_tag_rele(&ptag->tag);
}

/*
 * This function sends the so-called TLS RX static parameters to the
 * hardware. These parameters are temporarily stored in the
 * "crypto_params" field of the TLS RX tag.  Most importantly this
 * function sets the TCP sequence number (32-bit) and TLS record
 * number (64-bit) where the decryption can resume.
 *
 * Zero is returned upon success. Else some error happend.
 */
static int
mlx5e_tls_rx_send_static_parameters(struct mlx5e_iq *iq, struct mlx5e_tls_rx_tag *ptag)
{
	const u32 ds_cnt = DIV_ROUND_UP(sizeof(struct mlx5e_tx_umr_wqe) +
	    MLX5_FLD_SZ_BYTES(sw_tls_rx_cntx, param), MLX5_SEND_WQE_DS);
	struct mlx5e_tx_umr_wqe *wqe;
	int pi;

	mtx_lock(&iq->lock);
	pi = mlx5e_iq_get_producer_index(iq);
	if (pi < 0) {
		mtx_unlock(&iq->lock);
		return (-ENOMEM);
	}
	wqe = mlx5_wq_cyc_get_wqe(&iq->wq, pi);

	memset(wqe, 0, sizeof(*wqe));

	wqe->ctrl.opmod_idx_opcode = cpu_to_be32((iq->pc << 8) |
	    MLX5_OPCODE_UMR | (MLX5_OPCODE_MOD_UMR_TLS_TIR_STATIC_PARAMS << 24));
	wqe->ctrl.qpn_ds = cpu_to_be32((iq->sqn << 8) | ds_cnt);
	wqe->ctrl.imm = cpu_to_be32(ptag->tirn << 8);
	wqe->ctrl.fm_ce_se = MLX5_WQE_CTRL_CQ_UPDATE | MLX5_FENCE_MODE_INITIATOR_SMALL;

	/* fill out UMR control segment */
	wqe->umr.flags = 0x80;	/* inline data */
	wqe->umr.bsf_octowords =
	    cpu_to_be16(MLX5_FLD_SZ_BYTES(sw_tls_rx_cntx, param) / 16);

	/* copy in the static crypto parameters */
	memcpy(wqe + 1, MLX5_ADDR_OF(sw_tls_rx_cntx, ptag->crypto_params, param),
	    MLX5_FLD_SZ_BYTES(sw_tls_rx_cntx, param));

	/* copy data for doorbell */
	memcpy(iq->doorbell.d32, &wqe->ctrl, sizeof(iq->doorbell.d32));

	iq->data[pi].num_wqebbs = DIV_ROUND_UP(ds_cnt, MLX5_SEND_WQEBB_NUM_DS);
	iq->data[pi].callback = &mlx5e_tls_rx_send_static_parameters_cb;
	iq->data[pi].arg = ptag;

	m_snd_tag_ref(&ptag->tag);

	iq->pc += iq->data[pi].num_wqebbs;

	mlx5e_iq_notify_hw(iq);

	mtx_unlock(&iq->lock);

	return (0);	/* success */
}

static void
mlx5e_tls_rx_send_progress_parameters_cb(void *arg)
{
	struct mlx5e_tls_rx_tag *ptag;

	ptag = (struct mlx5e_tls_rx_tag *)arg;

	complete(&ptag->progress_complete);
}

CTASSERT(MLX5_FLD_SZ_BYTES(sw_tls_rx_cntx, progress) ==
    sizeof(((struct mlx5e_tx_psv_wqe *)NULL)->psv));

/*
 * This function resets the state of the TIR context to start
 * searching for a valid TLS header and is used only when allocating
 * the TLS RX tag.
 *
 * Zero is returned upon success, else some error happened.
 */
static int
mlx5e_tls_rx_send_progress_parameters_sync(struct mlx5e_iq *iq,
    struct mlx5e_tls_rx_tag *ptag)
{
	const u32 ds_cnt = DIV_ROUND_UP(sizeof(struct mlx5e_tx_psv_wqe),
	    MLX5_SEND_WQE_DS);
	struct mlx5e_priv *priv;
	struct mlx5e_tx_psv_wqe *wqe;
	int pi;

	mtx_lock(&iq->lock);
	pi = mlx5e_iq_get_producer_index(iq);
	if (pi < 0) {
		mtx_unlock(&iq->lock);
		return (-ENOMEM);
	}
	wqe = mlx5_wq_cyc_get_wqe(&iq->wq, pi);

	memset(wqe, 0, sizeof(*wqe));

	wqe->ctrl.opmod_idx_opcode = cpu_to_be32((iq->pc << 8) |
	    MLX5_OPCODE_SET_PSV | (MLX5_OPCODE_MOD_PSV_TLS_TIR_PROGRESS_PARAMS << 24));
	wqe->ctrl.qpn_ds = cpu_to_be32((iq->sqn << 8) | ds_cnt);
	wqe->ctrl.fm_ce_se = MLX5_WQE_CTRL_CQ_UPDATE;

	/* copy in the PSV control segment */
	memcpy(&wqe->psv, MLX5_ADDR_OF(sw_tls_rx_cntx, ptag->crypto_params, progress),
	    sizeof(wqe->psv));

	/* copy data for doorbell */
	memcpy(iq->doorbell.d32, &wqe->ctrl, sizeof(iq->doorbell.d32));

	iq->data[pi].num_wqebbs = DIV_ROUND_UP(ds_cnt, MLX5_SEND_WQEBB_NUM_DS);
	iq->data[pi].callback = &mlx5e_tls_rx_send_progress_parameters_cb;
	iq->data[pi].arg = ptag;

	iq->pc += iq->data[pi].num_wqebbs;

	init_completion(&ptag->progress_complete);

	mlx5e_iq_notify_hw(iq);

	mtx_unlock(&iq->lock);

	while (1) {
		if (wait_for_completion_timeout(&ptag->progress_complete,
		    msecs_to_jiffies(1000)) != 0)
			break;
		priv = container_of(iq, struct mlx5e_channel, iq)->priv;
		if (priv->mdev->state == MLX5_DEVICE_STATE_INTERNAL_ERROR ||
		    pci_channel_offline(priv->mdev->pdev) != 0)
			return (-EWOULDBLOCK);
	}

	return (0);	/* success */
}

CTASSERT(MLX5E_TLS_RX_PROGRESS_BUFFER_SIZE >= MLX5_ST_SZ_BYTES(tls_progress_params));
CTASSERT(MLX5E_TLS_RX_PROGRESS_BUFFER_SIZE <= PAGE_SIZE);

struct mlx5e_get_tls_progress_params_wqe {
	struct mlx5_wqe_ctrl_seg ctrl;
	struct mlx5_seg_get_psv	 psv;
};

static void
mlx5e_tls_rx_receive_progress_parameters_cb(void *arg)
{
	struct mlx5e_tls_rx_tag *ptag;
	struct mlx5e_iq *iq;
	uint32_t tcp_curr_sn_he;
	uint32_t tcp_next_sn_he;
	uint64_t tls_rcd_num;
	void *buffer;

	ptag = (struct mlx5e_tls_rx_tag *)arg;
	buffer = mlx5e_tls_rx_get_progress_buffer(ptag);

	MLX5E_TLS_RX_TAG_LOCK(ptag);

	ptag->tcp_resync_pending = 0;

	switch (MLX5_GET(tls_progress_params, buffer, record_tracker_state)) {
	case MLX5E_TLS_RX_PROGRESS_PARAMS_RECORD_TRACKER_STATE_TRACKING:
		break;
	default:
		goto done;
	}

	switch (MLX5_GET(tls_progress_params, buffer, auth_state)) {
	case MLX5E_TLS_RX_PROGRESS_PARAMS_AUTH_STATE_NO_OFFLOAD:
		break;
	default:
		goto done;
	}

	tcp_curr_sn_he = MLX5_GET(tls_progress_params, buffer, hw_resync_tcp_sn);

	if (mlx5e_tls_rx_snd_tag_find_tcp_sn_and_tls_rcd(ptag, tcp_curr_sn_he,
	    &tcp_next_sn_he, &tls_rcd_num)) {

		MLX5_SET64(sw_tls_rx_cntx, ptag->crypto_params,
		    param.initial_record_number, tls_rcd_num);
		MLX5_SET(sw_tls_rx_cntx, ptag->crypto_params,
		    param.resync_tcp_sn, tcp_curr_sn_he);

		iq = mlx5e_tls_rx_get_iq(
		    container_of(ptag->tls_rx, struct mlx5e_priv, tls_rx),
		    ptag->flowid, ptag->flowtype);

		if (mlx5e_tls_rx_send_static_parameters(iq, ptag) != 0)
			MLX5E_TLS_RX_STAT_INC(ptag, rx_error, 1);
	}
done:
	MLX5E_TLS_RX_TAG_UNLOCK(ptag);

	m_snd_tag_rele(&ptag->tag);
}

/*
 * This function queries the hardware for the current state of the TIR
 * in question. It is typically called when encrypted data is received
 * to re-establish hardware decryption of received TLS data.
 *
 * Zero is returned upon success, else some error happened.
 */
static int
mlx5e_tls_rx_receive_progress_parameters(struct mlx5e_iq *iq,
    struct mlx5e_tls_rx_tag *ptag, mlx5e_iq_callback_t *cb)
{
	struct mlx5e_get_tls_progress_params_wqe *wqe;
	const u32 ds_cnt = DIV_ROUND_UP(sizeof(*wqe), MLX5_SEND_WQE_DS);
	u64 dma_address;
	int pi;

	mtx_lock(&iq->lock);
	pi = mlx5e_iq_get_producer_index(iq);
	if (pi < 0) {
		mtx_unlock(&iq->lock);
		return (-ENOMEM);
	}

	mlx5e_iq_load_memory_single(iq, pi,
	    mlx5e_tls_rx_get_progress_buffer(ptag),
	    MLX5E_TLS_RX_PROGRESS_BUFFER_SIZE,
	    &dma_address, BUS_DMASYNC_PREREAD);

	wqe = mlx5_wq_cyc_get_wqe(&iq->wq, pi);

	memset(wqe, 0, sizeof(*wqe));

	wqe->ctrl.opmod_idx_opcode = cpu_to_be32((iq->pc << 8) |
	    MLX5_OPCODE_GET_PSV | (MLX5_OPCODE_MOD_PSV_TLS_TIR_PROGRESS_PARAMS << 24));
	wqe->ctrl.qpn_ds = cpu_to_be32((iq->sqn << 8) | ds_cnt);
	wqe->ctrl.fm_ce_se = MLX5_WQE_CTRL_CQ_UPDATE;
	wqe->psv.num_psv = 1 << 4;
	wqe->psv.l_key = iq->mkey_be;
	wqe->psv.psv_index[0] = cpu_to_be32(ptag->tirn);
	wqe->psv.va = cpu_to_be64(dma_address);

	/* copy data for doorbell */
	memcpy(iq->doorbell.d32, &wqe->ctrl, sizeof(iq->doorbell.d32));

	iq->data[pi].num_wqebbs = DIV_ROUND_UP(ds_cnt, MLX5_SEND_WQEBB_NUM_DS);
	iq->data[pi].callback = cb;
	iq->data[pi].arg = ptag;

	m_snd_tag_ref(&ptag->tag);

	iq->pc += iq->data[pi].num_wqebbs;

	mlx5e_iq_notify_hw(iq);

	mtx_unlock(&iq->lock);

	return (0);	/* success */
}

/*
 * This is the import function for TLS RX tags.
 */
static int
mlx5e_tls_rx_tag_import(void *arg, void **store, int cnt, int domain, int flags)
{
	struct mlx5e_tls_rx_tag *ptag;
	struct mlx5_core_dev *mdev = arg;
	int i;

	for (i = 0; i != cnt; i++) {
		ptag = malloc_domainset(sizeof(*ptag), M_MLX5E_TLS_RX,
		    mlx5_dev_domainset(mdev), flags | M_ZERO);
		mtx_init(&ptag->mtx, "mlx5-tls-rx-tag-mtx", NULL, MTX_DEF);
		INIT_WORK(&ptag->work, mlx5e_tls_rx_work);
		store[i] = ptag;
	}
	return (i);
}

/*
 * This is the release function for TLS RX tags.
 */
static void
mlx5e_tls_rx_tag_release(void *arg, void **store, int cnt)
{
	struct mlx5e_tls_rx_tag *ptag;
	int i;

	for (i = 0; i != cnt; i++) {
		ptag = store[i];

		flush_work(&ptag->work);
		mtx_destroy(&ptag->mtx);
		free(ptag, M_MLX5E_TLS_RX);
	}
}

/*
 * This is a convenience function to free TLS RX tags. It resets some
 * selected fields, updates the number of resources and returns the
 * TLS RX tag to the UMA pool of free tags.
 */
static void
mlx5e_tls_rx_tag_zfree(struct mlx5e_tls_rx_tag *ptag)
{
	/* make sure any unhandled taskqueue events are ignored */
	ptag->state = MLX5E_TLS_RX_ST_FREED;

	/* reset some variables */
	ptag->dek_index = 0;
	ptag->dek_index_ok = 0;
	ptag->tirn = 0;
	ptag->flow_rule = NULL;
	ptag->tcp_resync_active = 0;
	ptag->tcp_resync_pending = 0;

	/* avoid leaking keys */
	memset(ptag->crypto_params, 0, sizeof(ptag->crypto_params));

	/* update number of resources in use */
	atomic_add_32(&ptag->tls_rx->num_resources, -1U);

	/* return tag to UMA */
	uma_zfree(ptag->tls_rx->zone, ptag);
}

/*
 * This function enables TLS RX support for the given NIC, if all
 * needed firmware capabilites are present.
 */
int
mlx5e_tls_rx_init(struct mlx5e_priv *priv)
{
	struct mlx5e_tls_rx *ptls = &priv->tls_rx;
	struct sysctl_oid *node;
	uint32_t x;

	if (MLX5_CAP_GEN(priv->mdev, tls_rx) == 0 ||
	    MLX5_CAP_GEN(priv->mdev, log_max_dek) == 0 ||
	    MLX5_CAP_FLOWTABLE_NIC_RX(priv->mdev, ft_field_support.outer_ip_version) == 0)
		return (0);

	ptls->wq = create_singlethread_workqueue("mlx5-tls-rx-wq");
	if (ptls->wq == NULL)
		return (ENOMEM);

	sysctl_ctx_init(&ptls->ctx);

	snprintf(ptls->zname, sizeof(ptls->zname),
	    "mlx5_%u_tls_rx", device_get_unit(priv->mdev->pdev->dev.bsddev));

	ptls->zone = uma_zcache_create(ptls->zname,
	    sizeof(struct mlx5e_tls_rx_tag), NULL, NULL, NULL, NULL,
	    mlx5e_tls_rx_tag_import, mlx5e_tls_rx_tag_release, priv->mdev,
	    UMA_ZONE_UNMANAGED);

	/* shared between RX and TX TLS */
	ptls->max_resources = 1U << (MLX5_CAP_GEN(priv->mdev, log_max_dek) - 1);

	for (x = 0; x != MLX5E_TLS_RX_STATS_NUM; x++)
		ptls->stats.arg[x] = counter_u64_alloc(M_WAITOK);

	ptls->init = 1;

	node = SYSCTL_ADD_NODE(&priv->sysctl_ctx,
	    SYSCTL_CHILDREN(priv->sysctl_ifnet), OID_AUTO,
	    "tls_rx", CTLFLAG_RW | CTLFLAG_MPSAFE, NULL, "Hardware TLS receive offload");
	if (node == NULL)
		return (0);

	mlx5e_create_counter_stats(&ptls->ctx,
	    SYSCTL_CHILDREN(node), "stats",
	    mlx5e_tls_rx_stats_desc, MLX5E_TLS_RX_STATS_NUM,
	    ptls->stats.arg);

	return (0);
}

/*
 * This function disables TLS RX support for the given NIC.
 */
void
mlx5e_tls_rx_cleanup(struct mlx5e_priv *priv)
{
	struct mlx5e_tls_rx *ptls = &priv->tls_rx;
	uint32_t x;

	if (ptls->init == 0)
		return;

	ptls->init = 0;
	flush_workqueue(ptls->wq);
	sysctl_ctx_free(&ptls->ctx);
	uma_zdestroy(ptls->zone);
	destroy_workqueue(ptls->wq);

	/* check if all resources are freed */
	MPASS(priv->tls_rx.num_resources == 0);

	for (x = 0; x != MLX5E_TLS_RX_STATS_NUM; x++)
		counter_u64_free(ptls->stats.arg[x]);
}

/*
 * This function is used to serialize sleeping firmware operations
 * needed in order to establish and destroy a TLS RX tag.
 */
static void
mlx5e_tls_rx_work(struct work_struct *work)
{
	struct mlx5e_tls_rx_tag *ptag;
	struct mlx5e_priv *priv;
	int err;

	ptag = container_of(work, struct mlx5e_tls_rx_tag, work);
	priv = container_of(ptag->tls_rx, struct mlx5e_priv, tls_rx);

	switch (ptag->state) {
	case MLX5E_TLS_RX_ST_INIT:
		/* try to allocate new TIR context */
		err = mlx5_tls_open_tir(priv->mdev, priv->tdn,
		    priv->channel[mlx5e_tls_rx_get_ch(priv, ptag->flowid, ptag->flowtype)].rqtn,
		    &ptag->tirn);
		if (err) {
			MLX5E_TLS_RX_STAT_INC(ptag, rx_error, 1);
			break;
		}
		MLX5_SET(sw_tls_rx_cntx, ptag->crypto_params, progress.pd, ptag->tirn);

		/* try to allocate a DEK context ID */
		err = mlx5_encryption_key_create(priv->mdev, priv->pdn,
		    MLX5_GENERAL_OBJECT_TYPE_ENCRYPTION_KEY_TYPE_TLS,
		    MLX5_ADDR_OF(sw_tls_rx_cntx, ptag->crypto_params, key.key_data),
		    MLX5_GET(sw_tls_rx_cntx, ptag->crypto_params, key.key_len),
		    &ptag->dek_index);
		if (err) {
			MLX5E_TLS_RX_STAT_INC(ptag, rx_error, 1);
			break;
		}

		MLX5_SET(sw_tls_rx_cntx, ptag->crypto_params, param.dek_index, ptag->dek_index);

		ptag->dek_index_ok = 1;

		MLX5E_TLS_RX_TAG_LOCK(ptag);
		if (ptag->state == MLX5E_TLS_RX_ST_INIT)
			ptag->state = MLX5E_TLS_RX_ST_SETUP;
		MLX5E_TLS_RX_TAG_UNLOCK(ptag);
		break;

	case MLX5E_TLS_RX_ST_RELEASE:
		/* remove flow rule for incoming traffic, if any */
		if (ptag->flow_rule != NULL)
			mlx5e_accel_fs_del_inpcb(ptag->flow_rule);

		/* try to destroy DEK context by ID */
		if (ptag->dek_index_ok)
			mlx5_encryption_key_destroy(priv->mdev, ptag->dek_index);

		/* try to destroy TIR context by ID */
		if (ptag->tirn != 0)
			mlx5_tls_close_tir(priv->mdev, ptag->tirn);

		/* free tag */
		mlx5e_tls_rx_tag_zfree(ptag);
		break;

	default:
		break;
	}
}

/*
 * This function translates the crypto parameters into the format used
 * by the firmware and hardware. Currently only AES-128 and AES-256 is
 * supported for TLS v1.2 and TLS v1.3.
 *
 * Returns zero on success, else an error happened.
 */
static int
mlx5e_tls_rx_set_params(void *ctx, struct inpcb *inp, const struct tls_session_params *en)
{
	uint32_t tcp_sn_he;
	uint64_t tls_sn_he;

	MLX5_SET(sw_tls_rx_cntx, ctx, param.const_2, 2);
	if (en->tls_vminor == TLS_MINOR_VER_TWO)
		MLX5_SET(sw_tls_rx_cntx, ctx, param.tls_version, 2); /* v1.2 */
	else
		MLX5_SET(sw_tls_rx_cntx, ctx, param.tls_version, 3); /* v1.3 */
	MLX5_SET(sw_tls_rx_cntx, ctx, param.const_1, 1);
	MLX5_SET(sw_tls_rx_cntx, ctx, param.encryption_standard, 1); /* TLS */

	/* copy the initial vector in place */
	switch (en->iv_len) {
	case MLX5_FLD_SZ_BYTES(sw_tls_rx_cntx, param.gcm_iv):
	case MLX5_FLD_SZ_BYTES(sw_tls_rx_cntx, param.gcm_iv) +
	     MLX5_FLD_SZ_BYTES(sw_tls_rx_cntx, param.implicit_iv):
		memcpy(MLX5_ADDR_OF(sw_tls_rx_cntx, ctx, param.gcm_iv),
		    en->iv, en->iv_len);
		break;
	default:
		return (EINVAL);
	}

	if (en->cipher_key_len <= MLX5_FLD_SZ_BYTES(sw_tls_rx_cntx, key.key_data)) {
		memcpy(MLX5_ADDR_OF(sw_tls_rx_cntx, ctx, key.key_data),
		    en->cipher_key, en->cipher_key_len);
		MLX5_SET(sw_tls_rx_cntx, ctx, key.key_len, en->cipher_key_len);
	} else {
		return (EINVAL);
	}

	if (__predict_false(inp == NULL ||
	    ktls_get_rx_sequence(inp, &tcp_sn_he, &tls_sn_he) != 0))
		return (EINVAL);

	MLX5_SET64(sw_tls_rx_cntx, ctx, param.initial_record_number, tls_sn_he);
	MLX5_SET(sw_tls_rx_cntx, ctx, param.resync_tcp_sn, 0);
	MLX5_SET(sw_tls_rx_cntx, ctx, progress.next_record_tcp_sn, tcp_sn_he);

	return (0);
}

/* Verify zero default */
CTASSERT(MLX5E_TLS_RX_ST_INIT == 0);

/*
 * This function is responsible for allocating a TLS RX tag. It is a
 * callback function invoked by the network stack.
 *
 * Returns zero on success else an error happened.
 */
int
mlx5e_tls_rx_snd_tag_alloc(if_t ifp,
    union if_snd_tag_alloc_params *params,
    struct m_snd_tag **ppmt)
{
	struct mlx5e_iq *iq;
	struct mlx5e_priv *priv;
	struct mlx5e_tls_rx_tag *ptag;
	struct mlx5_flow_handle *flow_rule;
	const struct tls_session_params *en;
	uint32_t value;
	int error;

	priv = if_getsoftc(ifp);

	if (unlikely(priv->gone != 0 || priv->tls_rx.init == 0 ||
	    params->hdr.flowtype == M_HASHTYPE_NONE))
		return (EOPNOTSUPP);

	/* allocate new tag from zone, if any */
	ptag = uma_zalloc(priv->tls_rx.zone, M_NOWAIT);
	if (ptag == NULL)
		return (ENOMEM);

	/* sanity check default values */
	MPASS(ptag->dek_index == 0);
	MPASS(ptag->dek_index_ok == 0);

	/* setup TLS RX tag */
	ptag->tls_rx = &priv->tls_rx;
	ptag->flowtype = params->hdr.flowtype;
	ptag->flowid = params->hdr.flowid;

	value = atomic_fetchadd_32(&priv->tls_rx.num_resources, 1U);

	/* check resource limits */
	if (value >= priv->tls_rx.max_resources) {
		error = ENOMEM;
		goto failure;
	}

	en = &params->tls_rx.tls->params;

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
			error = mlx5e_tls_rx_set_params(
			    ptag->crypto_params, params->tls_rx.inp, en);
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
			error = mlx5e_tls_rx_set_params(
			    ptag->crypto_params, params->tls_rx.inp, en);
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

	/* store pointer to mbuf tag */
	MPASS(ptag->tag.refcount == 0);
	m_snd_tag_init(&ptag->tag, ifp, &mlx5e_tls_rx_snd_tag_sw);
	*ppmt = &ptag->tag;

	/* reset state */
	ptag->state = MLX5E_TLS_RX_ST_INIT;

	queue_work(priv->tls_rx.wq, &ptag->work);
	flush_work(&ptag->work);

	/* check that worker task completed successfully */
	MLX5E_TLS_RX_TAG_LOCK(ptag);
	if (ptag->state == MLX5E_TLS_RX_ST_SETUP) {
		ptag->state = MLX5E_TLS_RX_ST_READY;
		error = 0;
	} else {
		error = ENOMEM;
	}
	MLX5E_TLS_RX_TAG_UNLOCK(ptag);

	if (unlikely(error))
		goto cleanup;

	iq = mlx5e_tls_rx_get_iq(priv, ptag->flowid, ptag->flowtype);

	/* establish connection between DEK and TIR */
	if (mlx5e_tls_rx_send_static_parameters(iq, ptag) != 0) {
		MLX5E_TLS_RX_STAT_INC(ptag, rx_error, 1);
		error = ENOMEM;
		goto cleanup;
	}

	MLX5_SET(sw_tls_rx_cntx, ptag->crypto_params, progress.auth_state,
	    MLX5E_TLS_RX_PROGRESS_PARAMS_AUTH_STATE_NO_OFFLOAD);
	MLX5_SET(sw_tls_rx_cntx, ptag->crypto_params, progress.record_tracker_state,
	    MLX5E_TLS_RX_PROGRESS_PARAMS_RECORD_TRACKER_STATE_START);

	/* reset state to all zeros */
	if (mlx5e_tls_rx_send_progress_parameters_sync(iq, ptag) != 0) {
		MLX5E_TLS_RX_STAT_INC(ptag, rx_error, 1);
		error = ENOMEM;
		goto cleanup;
	}

	if (if_getpcp(ifp) != IFNET_PCP_NONE || params->tls_rx.vlan_id != 0) {
		/* create flow rule for TLS RX traffic (tagged) */
		flow_rule = mlx5e_accel_fs_add_inpcb(priv, params->tls_rx.inp,
		    ptag->tirn, MLX5_FS_DEFAULT_FLOW_TAG, params->tls_rx.vlan_id);
	} else {
		/* create flow rule for TLS RX traffic (untagged) */
		flow_rule = mlx5e_accel_fs_add_inpcb(priv, params->tls_rx.inp,
		    ptag->tirn, MLX5_FS_DEFAULT_FLOW_TAG, MLX5E_ACCEL_FS_ADD_INPCB_NO_VLAN);
	}

	if (IS_ERR_OR_NULL(flow_rule)) {
		MLX5E_TLS_RX_STAT_INC(ptag, rx_error, 1);
		error = ENOMEM;
		goto cleanup;
	}

	ptag->flow_rule = flow_rule;
	init_completion(&ptag->progress_complete);

	return (0);

cleanup:
	m_snd_tag_rele(&ptag->tag);
	return (error);

failure:
	mlx5e_tls_rx_tag_zfree(ptag);
	return (error);
}


/*
 * This function adds the TCP sequence number and TLS record number in
 * host endian format to a small database. When TLS records have the
 * same length, they are simply accumulated by counting instead of
 * separated entries in the TLS database. The dimension of the
 * database is such that it cannot store more than 1GByte of
 * continuous TCP data to avoid issues with TCP sequence number wrap
 * around. A record length of zero bytes has special meaning and means
 * that resync completed and all data in the database can be
 * discarded. This function is called after the TCP stack has
 * re-assembled all TCP fragments due to out of order packet reception
 * and all TCP sequence numbers should be sequential.
 *
 * This function returns true if a so-called TLS RX resync operation
 * is in progress. Else no such operation is in progress.
 */
static bool
mlx5e_tls_rx_snd_tag_add_tcp_sequence(struct mlx5e_tls_rx_tag *ptag,
    uint32_t tcp_sn_he, uint32_t len, uint64_t tls_rcd)
{
	uint16_t i, j, n;

	if (ptag->tcp_resync_active == 0 ||
	    ptag->tcp_resync_next != tcp_sn_he ||
	    len == 0) {
		/* start over again or terminate */
		ptag->tcp_resync_active = (len != 0);
		ptag->tcp_resync_len[0] = len;
		ptag->tcp_resync_num[0] = 1;
		ptag->tcp_resync_pc = (len != 0);
		ptag->tcp_resync_cc = 0;
		ptag->tcp_resync_start = tcp_sn_he;
		ptag->rcd_resync_start = tls_rcd;
	} else {
		i = (ptag->tcp_resync_pc - 1) & (MLX5E_TLS_RX_RESYNC_MAX - 1);
		n = ptag->tcp_resync_pc - ptag->tcp_resync_cc;

		/* check if same length like last time */
		if (ptag->tcp_resync_len[i] == len &&
		    ptag->tcp_resync_num[i] != MLX5E_TLS_RX_NUM_MAX) {
			/* use existing entry */
			ptag->tcp_resync_num[i]++;
		} else if (n == MLX5E_TLS_RX_RESYNC_MAX) {
			j = ptag->tcp_resync_cc++ & (MLX5E_TLS_RX_RESYNC_MAX - 1);
			/* adjust starting TCP sequence number */
			ptag->rcd_resync_start += ptag->tcp_resync_num[j];
			ptag->tcp_resync_start += ptag->tcp_resync_len[j] * ptag->tcp_resync_num[j];
			i = ptag->tcp_resync_pc++ & (MLX5E_TLS_RX_RESYNC_MAX - 1);
			/* store new entry */
			ptag->tcp_resync_len[i] = len;
			ptag->tcp_resync_num[i] = 1;
		} else {
			i = ptag->tcp_resync_pc++ & (MLX5E_TLS_RX_RESYNC_MAX - 1);
			/* add new entry */
			ptag->tcp_resync_len[i] = len;
			ptag->tcp_resync_num[i] = 1;
		}
	}

	/* store next TCP SN in host endian format */
	ptag->tcp_resync_next = tcp_sn_he + len;

	return (ptag->tcp_resync_active);
}

/*
 * This function checks if the given TCP sequence number points to the
 * beginning of a valid TLS header.
 *
 * Returns true if a match is found. Else false.
 */
static bool
mlx5e_tls_rx_snd_tag_find_tcp_sn_and_tls_rcd(struct mlx5e_tls_rx_tag *ptag,
    uint32_t tcp_sn_he, uint32_t *p_next_tcp_sn_he, uint64_t *p_tls_rcd)
{
	uint16_t i, j;
	uint32_t off = 0;
	uint32_t rcd = 0;
	uint32_t delta;
	uint32_t leap;

	for (i = ptag->tcp_resync_cc; i != ptag->tcp_resync_pc; i++) {
		delta = tcp_sn_he - off - ptag->tcp_resync_start;

		/* check if subtraction went negative */
		if ((int32_t)delta < 0)
			break;

		j = i & (MLX5E_TLS_RX_RESYNC_MAX - 1);
		leap = ptag->tcp_resync_len[j] * ptag->tcp_resync_num[j];
		if (delta < leap) {
			if ((delta % ptag->tcp_resync_len[j]) == 0) {
				*p_next_tcp_sn_he = tcp_sn_he +
				    ptag->tcp_resync_len[j];
				*p_tls_rcd = ptag->rcd_resync_start +
				    (uint64_t)rcd +
				    (uint64_t)(delta / ptag->tcp_resync_len[j]);
				return (true);		/* success */
			}
			break;	/* invalid offset */
		}
		rcd += ptag->tcp_resync_num[j];
		off += leap;
	}
	return (false);	/* not found */
}

/*
 * This is a callback function from the network stack to keep track of
 * TLS RX TCP sequence numbers.
 *
 * Returns zero on success else an error happened.
 */
static int
mlx5e_tls_rx_snd_tag_modify(struct m_snd_tag *pmt, union if_snd_tag_modify_params *params)
{
	struct mlx5e_tls_rx_tag *ptag;
	struct mlx5e_priv *priv;
	struct mlx5e_iq *iq;
	int err;

	ptag = container_of(pmt, struct mlx5e_tls_rx_tag, tag);
	priv = container_of(ptag->tls_rx, struct mlx5e_priv, tls_rx);

	if (unlikely(priv->gone != 0))
		return (ENXIO);

	iq = mlx5e_tls_rx_get_iq(priv, ptag->flowid, ptag->flowtype);

	MLX5E_TLS_RX_TAG_LOCK(ptag);

	if (mlx5e_tls_rx_snd_tag_add_tcp_sequence(ptag,
	    params->tls_rx.tls_hdr_tcp_sn,
	    params->tls_rx.tls_rec_length,
	    params->tls_rx.tls_seq_number) &&
	    ptag->tcp_resync_pending == 0) {
		err = mlx5e_tls_rx_receive_progress_parameters(iq, ptag,
		    &mlx5e_tls_rx_receive_progress_parameters_cb);
		if (err != 0) {
			MLX5E_TLS_RX_STAT_INC(ptag, rx_resync_err, 1);
		} else {
			ptag->tcp_resync_pending = 1;
			MLX5E_TLS_RX_STAT_INC(ptag, rx_resync_ok, 1);
		}
	} else {
		err = 0;
	}
	MLX5E_TLS_RX_TAG_UNLOCK(ptag);

	return (-err);
}

/*
 * This function frees a TLS RX tag in a non-blocking way.
 */
static void
mlx5e_tls_rx_snd_tag_free(struct m_snd_tag *pmt)
{
	struct mlx5e_tls_rx_tag *ptag =
	    container_of(pmt, struct mlx5e_tls_rx_tag, tag);
	struct mlx5e_priv *priv;

	MLX5E_TLS_RX_TAG_LOCK(ptag);
	ptag->state = MLX5E_TLS_RX_ST_RELEASE;
	MLX5E_TLS_RX_TAG_UNLOCK(ptag);

	priv = if_getsoftc(ptag->tag.ifp);
	queue_work(priv->tls_rx.wq, &ptag->work);
}

static void
mlx5e_tls_rx_str_status_cb(void *arg)
{
	struct mlx5e_tls_rx_tag *ptag;

	ptag = (struct mlx5e_tls_rx_tag *)arg;
	complete_all(&ptag->progress_complete);
	m_snd_tag_rele(&ptag->tag);
}

static int
mlx5e_tls_rx_snd_tag_status_str(struct m_snd_tag *pmt, char *buf, size_t *sz)
{
	int err, out_size;
	struct mlx5e_iq *iq;
	void *buffer;
	uint32_t tracker_state_val;
	uint32_t auth_state_val;
	struct mlx5e_priv *priv;
	struct mlx5e_tls_rx_tag *ptag = 
	    container_of(pmt, struct mlx5e_tls_rx_tag, tag);

	if (buf == NULL)
		return (0);

	MLX5E_TLS_RX_TAG_LOCK(ptag);
	priv = container_of(ptag->tls_rx, struct mlx5e_priv, tls_rx);
	iq = mlx5e_tls_rx_get_iq(priv, ptag->flowid, ptag->flowtype);
	reinit_completion(&ptag->progress_complete);
	err = mlx5e_tls_rx_receive_progress_parameters(iq, ptag,
	    &mlx5e_tls_rx_str_status_cb);
	MLX5E_TLS_RX_TAG_UNLOCK(ptag);
	if (err != 0)
		return (err);

	for (;;) {
		if (wait_for_completion_timeout(&ptag->progress_complete,
		    msecs_to_jiffies(1000)) != 0)
			break;
		if (priv->mdev->state == MLX5_DEVICE_STATE_INTERNAL_ERROR ||
		    pci_channel_offline(priv->mdev->pdev) != 0)
			return (ENXIO);
	}
	buffer = mlx5e_tls_rx_get_progress_buffer(ptag);
	tracker_state_val = MLX5_GET(tls_progress_params, buffer,
	    record_tracker_state);
	auth_state_val = MLX5_GET(tls_progress_params, buffer, auth_state);

	/* Validate tracker state value is in range */
	if (tracker_state_val >
	    MLX5E_TLS_RX_PROGRESS_PARAMS_RECORD_TRACKER_STATE_SEARCHING)
		return (EINVAL);

	/* Validate auth state value is in range */
	if (auth_state_val >
	    MLX5E_TLS_RX_PROGRESS_PARAMS_AUTH_STATE_AUTHENTICATION)
		return (EINVAL);

	out_size = snprintf(buf, *sz, "tracker_state: %s, auth_state: %s",
	    mlx5e_tls_rx_progress_params_record_tracker_state_str[
		tracker_state_val],
	    mlx5e_tls_rx_progress_params_auth_state_str[auth_state_val]);

	if (out_size <= *sz)
		*sz = out_size;
	return (0);
}

#else

int
mlx5e_tls_rx_init(struct mlx5e_priv *priv)
{

	return (0);
}

void
mlx5e_tls_rx_cleanup(struct mlx5e_priv *priv)
{
	/* NOP */
}

#endif
