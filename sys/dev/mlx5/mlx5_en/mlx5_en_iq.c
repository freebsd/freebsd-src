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

/*
 * The internal queue, IQ, code is more or less a stripped down copy
 * of the existing SQ managing code with exception of:
 *
 * - an optional single segment memory buffer which can be read or
 *   written as a whole by the hardware, may be provided.
 *
 * - an optional completion callback for all transmit operations, may
 *   be provided.
 *
 * - does not support mbufs.
 */

#include <dev/mlx5/mlx5_en/en.h>

static void
mlx5e_iq_poll(struct mlx5e_iq *iq, int budget)
{
	const struct mlx5_cqe64 *cqe;
	u16 ci;
	u16 iqcc;

	/*
	 * iq->cc must be updated only after mlx5_cqwq_update_db_record(),
	 * otherwise a cq overrun may occur
	 */
	iqcc = iq->cc;

	while (budget-- > 0) {

		cqe = mlx5e_get_cqe(&iq->cq);
		if (!cqe)
			break;

		mlx5_cqwq_pop(&iq->cq.wq);

		ci = iqcc & iq->wq.sz_m1;

		if (likely(iq->data[ci].dma_sync != 0)) {
			/* make sure data written by hardware is visible to CPU */
			bus_dmamap_sync(iq->dma_tag, iq->data[ci].dma_map, iq->data[ci].dma_sync);
			bus_dmamap_unload(iq->dma_tag, iq->data[ci].dma_map);

			iq->data[ci].dma_sync = 0;
		}

		if (likely(iq->data[ci].callback != NULL)) {
			iq->data[ci].callback(iq->data[ci].arg);
			iq->data[ci].callback = NULL;
		}

		if (unlikely(iq->data[ci].p_refcount != NULL)) {
			atomic_add_int(iq->data[ci].p_refcount, -1);
			iq->data[ci].p_refcount = NULL;
		}
		iqcc += iq->data[ci].num_wqebbs;
	}

	mlx5_cqwq_update_db_record(&iq->cq.wq);

	/* Ensure cq space is freed before enabling more cqes */
	atomic_thread_fence_rel();

	iq->cc = iqcc;
}

static void
mlx5e_iq_completion(struct mlx5_core_cq *mcq, struct mlx5_eqe *eqe __unused)
{
	struct mlx5e_iq *iq = container_of(mcq, struct mlx5e_iq, cq.mcq);

	mtx_lock(&iq->comp_lock);
	mlx5e_iq_poll(iq, MLX5E_BUDGET_MAX);
	mlx5e_cq_arm(&iq->cq, MLX5_GET_DOORBELL_LOCK(&iq->priv->doorbell_lock));
	mtx_unlock(&iq->comp_lock);
}

void
mlx5e_iq_send_nop(struct mlx5e_iq *iq, u32 ds_cnt)
{
	u16 pi = iq->pc & iq->wq.sz_m1;
	struct mlx5e_tx_wqe *wqe = mlx5_wq_cyc_get_wqe(&iq->wq, pi);

	mtx_assert(&iq->lock, MA_OWNED);

	memset(&wqe->ctrl, 0, sizeof(wqe->ctrl));

	wqe->ctrl.opmod_idx_opcode = cpu_to_be32((iq->pc << 8) | MLX5_OPCODE_NOP);
	wqe->ctrl.qpn_ds = cpu_to_be32((iq->sqn << 8) | ds_cnt);
	wqe->ctrl.fm_ce_se = MLX5_WQE_CTRL_CQ_UPDATE;

	/* Copy data for doorbell */
	memcpy(iq->doorbell.d32, &wqe->ctrl, sizeof(iq->doorbell.d32));

	iq->data[pi].callback = NULL;
	iq->data[pi].arg = NULL;
	iq->data[pi].num_wqebbs = DIV_ROUND_UP(ds_cnt, MLX5_SEND_WQEBB_NUM_DS);
	iq->data[pi].dma_sync = 0;
	iq->pc += iq->data[pi].num_wqebbs;
}

static void
mlx5e_iq_free_db(struct mlx5e_iq *iq)
{
	int wq_sz = mlx5_wq_cyc_get_size(&iq->wq);
	int x;

	for (x = 0; x != wq_sz; x++) {
		if (likely(iq->data[x].dma_sync != 0)) {
			bus_dmamap_unload(iq->dma_tag, iq->data[x].dma_map);
			iq->data[x].dma_sync = 0;
		}
		if (likely(iq->data[x].callback != NULL)) {
			iq->data[x].callback(iq->data[x].arg);
			iq->data[x].callback = NULL;
		}
		if (unlikely(iq->data[x].p_refcount != NULL)) {
			atomic_add_int(iq->data[x].p_refcount, -1);
			iq->data[x].p_refcount = NULL;
		}
		bus_dmamap_destroy(iq->dma_tag, iq->data[x].dma_map);
	}
	free(iq->data, M_MLX5EN);
}

static int
mlx5e_iq_alloc_db(struct mlx5e_iq *iq)
{
	int wq_sz = mlx5_wq_cyc_get_size(&iq->wq);
	int err;
	int x;

	iq->data = malloc_domainset(wq_sz * sizeof(iq->data[0]), M_MLX5EN,
	    mlx5_dev_domainset(iq->priv->mdev), M_WAITOK | M_ZERO);

	/* Create DMA descriptor maps */
	for (x = 0; x != wq_sz; x++) {
		err = -bus_dmamap_create(iq->dma_tag, 0, &iq->data[x].dma_map);
		if (err != 0) {
			while (x--)
				bus_dmamap_destroy(iq->dma_tag, iq->data[x].dma_map);
			free(iq->data, M_MLX5EN);
			return (err);
		}
	}
	return (0);
}

static int
mlx5e_iq_create(struct mlx5e_channel *c,
    struct mlx5e_sq_param *param,
    struct mlx5e_iq *iq)
{
	struct mlx5e_priv *priv = c->priv;
	struct mlx5_core_dev *mdev = priv->mdev;
	void *sqc = param->sqc;
	void *sqc_wq = MLX5_ADDR_OF(sqc, sqc, wq);
	int err;

	/* Create DMA descriptor TAG */
	if ((err = -bus_dma_tag_create(
	    bus_get_dma_tag(mdev->pdev->dev.bsddev),
	    1,				/* any alignment */
	    0,				/* no boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    PAGE_SIZE,			/* maxsize */
	    1,				/* nsegments */
	    PAGE_SIZE,			/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockfuncarg */
	    &iq->dma_tag)))
		goto done;

	iq->mkey_be = cpu_to_be32(priv->mr.key);
	iq->priv = priv;

	err = mlx5_wq_cyc_create(mdev, &param->wq, sqc_wq,
	    &iq->wq, &iq->wq_ctrl);
	if (err)
		goto err_free_dma_tag;

	iq->wq.db = &iq->wq.db[MLX5_SND_DBR];

	err = mlx5e_iq_alloc_db(iq);
	if (err)
		goto err_iq_wq_destroy;

	return (0);

err_iq_wq_destroy:
	mlx5_wq_destroy(&iq->wq_ctrl);

err_free_dma_tag:
	bus_dma_tag_destroy(iq->dma_tag);
done:
	return (err);
}

static void
mlx5e_iq_destroy(struct mlx5e_iq *iq)
{
	mlx5e_iq_free_db(iq);
	mlx5_wq_destroy(&iq->wq_ctrl);
	bus_dma_tag_destroy(iq->dma_tag);
}

static int
mlx5e_iq_enable(struct mlx5e_iq *iq, struct mlx5e_sq_param *param,
    const struct mlx5_sq_bfreg *bfreg, int tis_num)
{
	void *in;
	void *sqc;
	void *wq;
	int inlen;
	int err;
	u8 ts_format;

	inlen = MLX5_ST_SZ_BYTES(create_sq_in) +
	    sizeof(u64) * iq->wq_ctrl.buf.npages;
	in = mlx5_vzalloc(inlen);
	if (in == NULL)
		return (-ENOMEM);

	iq->uar_map = bfreg->map;

	ts_format = mlx5_get_sq_default_ts(iq->priv->mdev);
	sqc = MLX5_ADDR_OF(create_sq_in, in, ctx);
	wq = MLX5_ADDR_OF(sqc, sqc, wq);

	memcpy(sqc, param->sqc, sizeof(param->sqc));

	MLX5_SET(sqc, sqc, tis_num_0, tis_num);
	MLX5_SET(sqc, sqc, cqn, iq->cq.mcq.cqn);
	MLX5_SET(sqc, sqc, state, MLX5_SQC_STATE_RST);
	MLX5_SET(sqc, sqc, ts_format, ts_format);
	MLX5_SET(sqc, sqc, tis_lst_sz, 1);
	MLX5_SET(sqc, sqc, flush_in_error_en, 1);
	MLX5_SET(sqc, sqc, allow_swp, 1);

	/* SQ remap support requires reg_umr privileges level */
	if (MLX5_CAP_QOS(iq->priv->mdev, qos_remap_pp)) {
		MLX5_SET(sqc, sqc, qos_remap_en, 1);
		if (MLX5_CAP_ETH(iq->priv->mdev, reg_umr_sq))
			MLX5_SET(sqc, sqc, reg_umr, 1);
		 else
			mlx5_en_err(iq->priv->ifp,
			    "No reg umr SQ capability, SQ remap disabled\n");
	}

	MLX5_SET(wq, wq, wq_type, MLX5_WQ_TYPE_CYCLIC);
	MLX5_SET(wq, wq, uar_page, bfreg->index);
	MLX5_SET(wq, wq, log_wq_pg_sz, iq->wq_ctrl.buf.page_shift -
	    MLX5_ADAPTER_PAGE_SHIFT);
	MLX5_SET64(wq, wq, dbr_addr, iq->wq_ctrl.db.dma);

	mlx5_fill_page_array(&iq->wq_ctrl.buf,
	    (__be64 *) MLX5_ADDR_OF(wq, wq, pas));

	err = mlx5_core_create_sq(iq->priv->mdev, in, inlen, &iq->sqn);

	kvfree(in);

	return (err);
}

static int
mlx5e_iq_modify(struct mlx5e_iq *iq, int curr_state, int next_state)
{
	void *in;
	void *sqc;
	int inlen;
	int err;

	inlen = MLX5_ST_SZ_BYTES(modify_sq_in);
	in = mlx5_vzalloc(inlen);
	if (in == NULL)
		return (-ENOMEM);

	sqc = MLX5_ADDR_OF(modify_sq_in, in, ctx);

	MLX5_SET(modify_sq_in, in, sqn, iq->sqn);
	MLX5_SET(modify_sq_in, in, sq_state, curr_state);
	MLX5_SET(sqc, sqc, state, next_state);

	err = mlx5_core_modify_sq(iq->priv->mdev, in, inlen);

	kvfree(in);

	return (err);
}

static void
mlx5e_iq_disable(struct mlx5e_iq *iq)
{
	mlx5_core_destroy_sq(iq->priv->mdev, iq->sqn);
}

int
mlx5e_iq_open(struct mlx5e_channel *c,
    struct mlx5e_sq_param *sq_param,
    struct mlx5e_cq_param *cq_param,
    struct mlx5e_iq *iq)
{
	int err;

	err = mlx5e_open_cq(c->priv, cq_param, &iq->cq,
	    &mlx5e_iq_completion, c->ix);
	if (err)
		return (err);

	err = mlx5e_iq_create(c, sq_param, iq);
	if (err)
		goto err_close_cq;

	err = mlx5e_iq_enable(iq, sq_param, &c->bfreg, c->priv->tisn[0]);
	if (err)
		goto err_destroy_sq;

	err = mlx5e_iq_modify(iq, MLX5_SQC_STATE_RST, MLX5_SQC_STATE_RDY);
	if (err)
		goto err_disable_sq;

	WRITE_ONCE(iq->running, 1);

	return (0);

err_disable_sq:
	mlx5e_iq_disable(iq);
err_destroy_sq:
	mlx5e_iq_destroy(iq);
err_close_cq:
	mlx5e_close_cq(&iq->cq);

	return (err);
}

static void
mlx5e_iq_drain(struct mlx5e_iq *iq)
{
	struct mlx5_core_dev *mdev = iq->priv->mdev;

	/*
	 * Check if already stopped.
	 *
	 * NOTE: Serialization of this function is managed by the
	 * caller ensuring the priv's state lock is locked or in case
	 * of rate limit support, a single thread manages drain and
	 * resume of SQs. The "running" variable can therefore safely
	 * be read without any locks.
	 */
	if (READ_ONCE(iq->running) == 0)
		return;

	/* don't put more packets into the SQ */
	WRITE_ONCE(iq->running, 0);

	/* wait till SQ is empty or link is down */
	mtx_lock(&iq->lock);
	while (iq->cc != iq->pc &&
	    (iq->priv->media_status_last & IFM_ACTIVE) != 0 &&
	    mdev->state != MLX5_DEVICE_STATE_INTERNAL_ERROR &&
	    pci_channel_offline(mdev->pdev) == 0) {
		mtx_unlock(&iq->lock);
		msleep(1);
		iq->cq.mcq.comp(&iq->cq.mcq, NULL);
		mtx_lock(&iq->lock);
	}
	mtx_unlock(&iq->lock);

	/* error out remaining requests */
	(void) mlx5e_iq_modify(iq, MLX5_SQC_STATE_RDY, MLX5_SQC_STATE_ERR);

	/* wait till SQ is empty */
	mtx_lock(&iq->lock);
	while (iq->cc != iq->pc &&
	    mdev->state != MLX5_DEVICE_STATE_INTERNAL_ERROR &&
	    pci_channel_offline(mdev->pdev) == 0) {
		mtx_unlock(&iq->lock);
		msleep(1);
		iq->cq.mcq.comp(&iq->cq.mcq, NULL);
		mtx_lock(&iq->lock);
	}
	mtx_unlock(&iq->lock);
}

void
mlx5e_iq_close(struct mlx5e_iq *iq)
{
	mlx5e_iq_drain(iq);
	mlx5e_iq_disable(iq);
	mlx5e_iq_destroy(iq);
	mlx5e_close_cq(&iq->cq);
}

void
mlx5e_iq_static_init(struct mlx5e_iq *iq)
{
	mtx_init(&iq->lock, "mlx5iq",
	    MTX_NETWORK_LOCK " IQ", MTX_DEF);
	mtx_init(&iq->comp_lock, "mlx5iq_comp",
	    MTX_NETWORK_LOCK " IQ COMP", MTX_DEF);
}

void
mlx5e_iq_static_destroy(struct mlx5e_iq *iq)
{
	mtx_destroy(&iq->lock);
	mtx_destroy(&iq->comp_lock);
}

void
mlx5e_iq_notify_hw(struct mlx5e_iq *iq)
{
	mtx_assert(&iq->lock, MA_OWNED);

	/* Check if we need to write the doorbell */
	if (unlikely(iq->db_inhibit != 0 || iq->doorbell.d64 == 0))
		return;

	/* Ensure wqe is visible to device before updating doorbell record */
	wmb();

	*iq->wq.db = cpu_to_be32(iq->pc);

	/*
	 * Ensure the doorbell record is visible to device before ringing
	 * the doorbell:
	 */
	wmb();

	mlx5_write64(iq->doorbell.d32, iq->uar_map,
	    MLX5_GET_DOORBELL_LOCK(&iq->priv->doorbell_lock));

	iq->doorbell.d64 = 0;
}

static inline bool
mlx5e_iq_has_room_for(struct mlx5e_iq *iq, u16 n)
{
        u16 cc = iq->cc;
        u16 pc = iq->pc;

        return ((iq->wq.sz_m1 & (cc - pc)) >= n || cc == pc);
}

int
mlx5e_iq_get_producer_index(struct mlx5e_iq *iq)
{
	u16 pi;

	mtx_assert(&iq->lock, MA_OWNED);

	if (unlikely(iq->running == 0))
		return (-1);
	if (unlikely(!mlx5e_iq_has_room_for(iq, 2 * MLX5_SEND_WQE_MAX_WQEBBS)))
		return (-1);

	/* Align IQ edge with NOPs to avoid WQE wrap around */
	pi = ((~iq->pc) & iq->wq.sz_m1);
	if (unlikely(pi < (MLX5_SEND_WQE_MAX_WQEBBS - 1))) {
		/* Send one multi NOP message instead of many */
		mlx5e_iq_send_nop(iq, (pi + 1) * MLX5_SEND_WQEBB_NUM_DS);
		pi = ((~iq->pc) & iq->wq.sz_m1);
		if (unlikely(pi < (MLX5_SEND_WQE_MAX_WQEBBS - 1)))
			return (-1);
	}
	return (iq->pc & iq->wq.sz_m1);
}

static void
mlx5e_iq_load_memory_cb(void *arg, bus_dma_segment_t *segs,
    int nseg, int error)
{
	u64 *pdma_address = arg;

	if (unlikely(error || nseg != 1))
		panic("mlx5e_iq_load_memory_cb: error=%d nseg=%d", error, nseg);

	*pdma_address = segs[0].ds_addr;
}

CTASSERT(BUS_DMASYNC_POSTREAD != 0);
CTASSERT(BUS_DMASYNC_POSTWRITE != 0);

void
mlx5e_iq_load_memory_single(struct mlx5e_iq *iq, u16 pi, void *buffer, size_t size,
    u64 *pdma_address, u32 dma_sync)
{
	int error;

	error = bus_dmamap_load(iq->dma_tag, iq->data[pi].dma_map, buffer, size,
	    &mlx5e_iq_load_memory_cb, pdma_address, BUS_DMA_NOWAIT);
	if (unlikely(error))
		panic("mlx5e_iq_load_memory: error=%d buffer=%p size=%zd", error, buffer, size);

	switch (dma_sync) {
	case BUS_DMASYNC_PREREAD:
		iq->data[pi].dma_sync = BUS_DMASYNC_POSTREAD;
		break;
	case BUS_DMASYNC_PREWRITE:
		iq->data[pi].dma_sync = BUS_DMASYNC_POSTWRITE;
		break;
	default:
		panic("mlx5e_iq_load_memory_single: Invalid DMA sync operation(%d)", dma_sync);
	}

	/* make sure data in buffer is visible to hardware */
	bus_dmamap_sync(iq->dma_tag, iq->data[pi].dma_map, dma_sync);
}
