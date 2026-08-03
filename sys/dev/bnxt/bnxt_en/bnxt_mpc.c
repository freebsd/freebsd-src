/*
 * Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2026 Broadcom, All Rights Reserved.
 * The term Broadcom refers to Broadcom Limited and/or its subsidiaries
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/pciio.h>
#include <sys/time.h>
#include <machine/atomic.h>
#include <machine/resource.h>
#include <dev/pci/pcivar.h>

#include "bnxt.h"
#include "bnxt_mpc.h"
#include "bnxt_hwrm.h"
#include "bnxt_log.h"

/*
 * bnxt_ktls_mpc_cmp() is defined in bnxt_ktls.c, not yet part of this
 * branch; forward-declared here so this file builds standalone.
 */
void bnxt_ktls_mpc_cmp(struct bnxt_softc *softc, uint32_t client,
    unsigned long handle, struct bnxt_cmpl_entry *cmpl_entry_arr,
    uint32_t cmpl_num);

void bnxt_alloc_mpc_info(struct bnxt_softc *softc, uint8_t mpc_chnls_cap)
{
	if (mpc_chnls_cap) {
		if (!softc->mpc_info)
			softc->mpc_info = malloc(sizeof(*softc->mpc_info),
						  M_DEVBUF, M_NOWAIT | M_ZERO);
	} else {
		bnxt_free_mpc_info(softc);
	}
	if (softc->mpc_info)
		softc->mpc_info->mpc_chnls_cap = mpc_chnls_cap;
}

void bnxt_free_mpc_info(struct bnxt_softc *softc)
{
	if (softc->mpc_info) {
		free(softc->mpc_info, M_DEVBUF);
		softc->mpc_info = NULL;
	}
}

static void __bnxt_set_dflt_mpc_rings(struct bnxt_softc *softc,
    enum bnxt_mpc_type type, int *avail, int avail_cp)
{
	struct bnxt_mpc_info *mpc = softc->mpc_info;
	int dflt1, dflt2;
	int idx1, idx2;
	int min1, min2;
	int val1, val2;

	if (type == BNXT_MPC_CRYPTO) {
		min1 = BNXT_MIN_MPC_TCE;
		min2 = BNXT_MIN_MPC_RCE;
		dflt1 = BNXT_DFLT_MPC_TCE;
		dflt2 = BNXT_DFLT_MPC_RCE;
		idx1 = BNXT_MPC_TCE_TYPE;
		idx2 = BNXT_MPC_RCE_TYPE;
	} else {
		min1 = BNXT_MIN_MPC_TE_CFA;
		min2 = BNXT_MIN_MPC_RE_CFA;
		dflt1 = BNXT_DFLT_MPC_TE_CFA;
		dflt2 = BNXT_DFLT_MPC_RE_CFA;
		idx1 = BNXT_MPC_TE_CFA_TYPE;
		idx2 = BNXT_MPC_RE_CFA_TYPE;
	}
	if (*avail < (min1 + min2))
		return;

	val1 = min(*avail / 2, softc->ntxqsets);
	val2 = val1;

	val1 = min(val1, dflt1);
	val2 = min(val2, dflt2);

	if (avail_cp < min1 || avail_cp < min2)
		return;

	val1 = min(val1, avail_cp);
	val2 = min(val2, avail_cp);

	mpc->mpc_ring_count[idx1] = val1;
	mpc->mpc_ring_count[idx2] = val2;

	*avail = *avail - val1 - val2;
}

void bnxt_set_dflt_mpc_rings(struct bnxt_softc *softc)
{
	struct bnxt_func_qcfg *fn_qcfg = &softc->fn_qcfg;
	struct bnxt_mpc_info *mpc = softc->mpc_info;
	int avail, mpc_cp, i;
	int avail_cp;

	if (!mpc)
		return;

	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++)
		mpc->mpc_ring_count[i] = 0;
	mpc->mpc_cp_rings_count = 0;

	avail = fn_qcfg->orig_alloc_tx_rings - softc->ntxqsets;

	avail_cp = fn_qcfg->orig_alloc_completion_rings - softc->ntxqsets;
	avail_cp = min(avail_cp,
	    ((pci_msix_count(softc->dev) - 1) - softc->ntxqsets));

	if (BNXT_MPC_CRYPTO_CAPABLE(softc))
		__bnxt_set_dflt_mpc_rings(softc, BNXT_MPC_CRYPTO, &avail,
		    avail_cp);

	for (i = 0, mpc_cp = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		if (mpc_cp < mpc->mpc_ring_count[i])
			mpc_cp = mpc->mpc_ring_count[i];
	}
	mpc->mpc_cp_rings_count = mpc_cp;
}

int bnxt_alloc_mpcs(struct bnxt_softc *softc)
{
	struct bnxt_mpc_info *mpc = softc->mpc_info;
	int i;

	if (!BNXT_MPC_CRYPTO_CAPABLE(softc))
		return 0;

	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		int num;
		struct bnxt_ring *txr;

		/* Currently only KTLS TX is supported */
		if (i != BNXT_MPC_TCE_TYPE)
			continue;

		num = mpc->mpc_ring_count[i];
		if (!num)
			continue;
		txr = malloc(num * sizeof(*txr), M_DEVBUF, M_NOWAIT | M_ZERO);
		if (!txr) {
			device_printf(softc->dev,
			    "%s: Failed to allocate %d MPC rings\n",
			    __func__, num);
			return ENOMEM;
		}
		mpc->mpc_rings[i] = txr;
	}

	mpc->mpc_cp_rings =
	    malloc(mpc->mpc_cp_rings_count * sizeof(struct bnxt_cp_ring),
		M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!mpc->mpc_cp_rings) {
		device_printf(softc->dev,
		    "%s: Failed to allocate %d MPC completion rings\n",
		    __func__, mpc->mpc_cp_rings_count);
		return ENOMEM;
	}

	mpc->mpc_nq_rings =
	    malloc(mpc->mpc_cp_rings_count * sizeof(struct bnxt_cp_ring),
		M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!mpc->mpc_nq_rings) {
		device_printf(softc->dev,
		    "%s: Failed to allocate %d MPC notification rings\n",
		    __func__, mpc->mpc_cp_rings_count);
		return ENOMEM;
	}
	return 0;
}

void bnxt_free_mpcs(struct bnxt_softc *softc)
{
	struct bnxt_mpc_info *mpc = softc->mpc_info;
	int i;

	if (!mpc)
		return;

	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		/* Currently only KTLS TX is supported */
		if (i != BNXT_MPC_TCE_TYPE)
			continue;

		if (mpc->mpc_rings[i]) {
			free(mpc->mpc_rings[i], M_DEVBUF);
			mpc->mpc_rings[i] = NULL;
		}
	}

	if (mpc->mpc_cp_rings) {
		free(mpc->mpc_cp_rings, M_DEVBUF);
		mpc->mpc_cp_rings = NULL;
	}

	if (mpc->mpc_nq_rings) {
		free(mpc->mpc_nq_rings, M_DEVBUF);
		mpc->mpc_nq_rings = NULL;
	}
}

int bnxt_alloc_mpc_rings(struct bnxt_softc *softc)
{
	struct bnxt_mpc_info *mpc = softc->mpc_info;
	int i, j, mpc_idx = 0;
	int mpc_base_ring_id =
	    (softc->scctx->isc_nrxqsets * 2) + 1 + softc->ntxqsets;

	if (!mpc)
		return 0;

	for (i = 0; i < mpc->mpc_cp_rings_count; i++) {
		struct bnxt_cp_ring *cpr = &mpc->mpc_cp_rings[i];
		int rc;

		cpr->ring.queue_id = BNXT_MPC_QUEUE_ID;

		/* Set up the completion ring */
		cpr->stats_ctx_id = HWRM_NA_SIGNATURE;
		cpr->ring.phys_id = (uint16_t)HWRM_NA_SIGNATURE;
		cpr->ring.softc = softc;
		cpr->ring.idx = i;
		cpr->ring.id = mpc_base_ring_id + i;
		cpr->ring.doorbell = softc->legacy_db_size;
		cpr->ring.ring_size = softc->scctx->isc_ntxd[0];
		cpr->ring.db_ring_mask = cpr->ring.ring_size - 1;

		rc = iflib_dma_alloc(softc->ctx,
		    sizeof(struct cmpl_base) * cpr->ring.ring_size,
		    &cpr->ring.ring_mem, 0);
		if (rc) {
			device_printf(softc->dev,
			    "%s: Failed to allocate DMA memory for completion ring %d (size=%zu), rc=%d\n",
			    __func__, i, sizeof(struct cmpl_base) * cpr->ring.ring_size, rc);
			return rc;
		}

		cpr->ring.vaddr = cpr->ring.ring_mem.idi_vaddr;
		cpr->ring.paddr = cpr->ring.ring_mem.idi_paddr;
	}

	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		int num = mpc->mpc_ring_count[i], rc;

		/* Currently only KTLS TX is supported */
		if (i != BNXT_MPC_TCE_TYPE)
			continue;

		for (j = 0; j < num; j++) {
			struct bnxt_ring *txr = &mpc->mpc_rings[i][j];
			/* tx_stats[] is indexed across all MPC types, so use
			 * the flat mpc_idx, not the intra-type counter j. */
			int stat_idx = mpc_idx;

			txr->queue_id = BNXT_MPC_QUEUE_ID;
			txr->mpc_chnl_type = i;

			/* alloc mpc TX ring */
			txr->phys_id = (uint16_t)HWRM_NA_SIGNATURE;
			txr->softc = softc;
			txr->idx = i << BNXT_RING_MPC_IDX_TYPE_SFT | j;
			txr->id = mpc_base_ring_id + mpc_idx++;
			txr->doorbell = softc->legacy_db_size;
			txr->ring_size = softc->scctx->isc_ntxd[1];
			txr->db_ring_mask = txr->ring_size - 1;

			/* Init before any early return: bnxt_free_mpc_rings()
			 * unconditionally mtx_destroy()s every txr. */
			mtx_init(&txr->tx_lock, "MPC TX lock", NULL, MTX_SPIN);

			rc = iflib_dma_alloc(softc->ctx,
			    sizeof(struct tx_bd_short) * txr->ring_size,
			    &txr->ring_mem, 0);
			if (rc) {
				device_printf(softc->dev,
				    "%s: Failed to allocate DMA memory for TX ring %d (size=%zu), rc=%d\n",
				    __func__, j, sizeof(struct tx_bd_short) * txr->ring_size, rc);
				return rc;
			}

			txr->vaddr = txr->ring_mem.idi_vaddr;
			txr->paddr = txr->ring_mem.idi_paddr;

			txr->tx_mpc_buf_ring =
			    malloc(txr->ring_size * sizeof(struct bnxt_sw_mpc_tx_bd),
				M_DEVBUF, M_NOWAIT | M_ZERO);
			if (!txr->tx_mpc_buf_ring) {
				device_printf(softc->dev,
				    "%s: Failed to allocate buffer ring for TX ring %d (size=%zu)\n",
				    __func__, j, txr->ring_size * sizeof(struct bnxt_sw_mpc_tx_bd));
				return ENOMEM;
			}

			rc = iflib_dma_alloc(softc->ctx, sizeof(struct ctx_hw_stats),
			    &mpc->tx_stats[stat_idx], 0);
			if (rc) {
				device_printf(softc->dev,
				    "%s: Failed to allocate DMA memory for TX stats ring %d, rc=%d\n",
				    __func__, j, rc);
				return rc;
			}
			bus_dmamap_sync(mpc->tx_stats[stat_idx].idi_tag,
			    mpc->tx_stats[stat_idx].idi_map, BUS_DMASYNC_PREREAD);
		}
	}

	/* Set up the Notification ring (NQ) */
	for (i = 0; i < mpc->mpc_cp_rings_count; i++) {
		struct bnxt_cp_ring *nqr = &mpc->mpc_nq_rings[i];
		int rc;

		nqr->ring.queue_id = BNXT_MPC_QUEUE_ID;

		nqr->stats_ctx_id = HWRM_NA_SIGNATURE;
		nqr->ring.phys_id = (uint16_t)HWRM_NA_SIGNATURE;
		nqr->ring.softc = softc;
		nqr->ring.idx = i;
		nqr->ring.id = softc->ntxqsets + i;
		nqr->ring.doorbell = softc->legacy_db_size;
		nqr->ring.ring_size = softc->scctx->isc_ntxd[2];
		nqr->ring.db_ring_mask = nqr->ring.ring_size - 1;

		rc = iflib_dma_alloc(softc->ctx,
		    sizeof(struct cmpl_base) * nqr->ring.ring_size,
		    &nqr->ring.ring_mem, 0);
		if (rc) {
			device_printf(softc->dev,
			    "%s: Failed to allocate DMA memory for notification ring %d (size=%zu), rc=%d\n",
			    __func__, i, sizeof(struct cmpl_base) * nqr->ring.ring_size, rc);
			return rc;
		}

		nqr->ring.vaddr = nqr->ring.ring_mem.idi_vaddr;
		nqr->ring.paddr = nqr->ring.ring_mem.idi_paddr;
		nqr->type = TX_CP_NQ;
	}

	return 0;
}

void bnxt_free_mpc_rings(struct bnxt_softc *softc)
{
	struct bnxt_mpc_info *mpc = softc->mpc_info;
	int i, j, mpc_idx = 0;

	if (!mpc)
		return;

	/* A partial alloc failure can leave these NULL; guard each array. */
	if (mpc->mpc_nq_rings) {
		for (i = 0; i < mpc->mpc_cp_rings_count; i++)
			iflib_dma_free(&mpc->mpc_nq_rings[i].ring.ring_mem);
	}

	if (mpc->mpc_cp_rings) {
		for (i = 0; i < mpc->mpc_cp_rings_count; i++)
			iflib_dma_free(&mpc->mpc_cp_rings[i].ring.ring_mem);
	}

	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		int num = mpc->mpc_ring_count[i];

		/* Currently only KTLS TX is supported */
		if (i != BNXT_MPC_TCE_TYPE)
			continue;

		/*
		 * Same partial-allocation case as above: mpc_ring_count[i] can
		 * be nonzero while mpc_rings[i] itself failed to allocate.
		 */
		if (!mpc->mpc_rings[i])
			continue;

		for (j = 0; j < num; j++) {
			struct bnxt_ring *txr = &mpc->mpc_rings[i][j];

			/* TCE ring vaddr holds cleartext AES-GCM keys; scrub
			 * before freeing the pages. */
			if (i == BNXT_MPC_TCE_TYPE && txr->ring_mem.idi_vaddr)
				explicit_bzero(txr->ring_mem.idi_vaddr,
				    txr->ring_mem.idi_size);

			iflib_dma_free(&txr->ring_mem);
			if (txr->tx_mpc_buf_ring) {
				free(txr->tx_mpc_buf_ring, M_DEVBUF);
				txr->tx_mpc_buf_ring = NULL;
			}
			/* A ring past a partial alloc failure never had its
			 * tx_lock mtx_init()'d; guard the destroy. */
			if (mtx_initialized(&txr->tx_lock))
				mtx_destroy(&txr->tx_lock);
			/* Must match the flat stat_idx bnxt_alloc_mpc_rings() used. */
			iflib_dma_free(&mpc->tx_stats[mpc_idx++]);
		}
	}
}

void bnxt_clear_mpc_rings_ids(struct bnxt_softc *softc)
{
	struct bnxt_mpc_info *mpc = softc->mpc_info;
	int i, j;

	if (!mpc)
		return;

	for (i = 0; i < mpc->mpc_cp_rings_count; i++) {
		mpc->mpc_nq_rings[i].ring.phys_id = (uint16_t)HWRM_NA_SIGNATURE;
		mpc->mpc_cp_rings[i].ring.phys_id = (uint16_t)HWRM_NA_SIGNATURE;
		mpc->mpc_cp_rings[i].stats_ctx_id = HWRM_NA_SIGNATURE;
	}

	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		int num = mpc->mpc_ring_count[i];

		/* Currently only KTLS TX is supported */
		if (i != BNXT_MPC_TCE_TYPE)
			continue;

		for (j = 0; j < num; j++) {
			struct bnxt_ring *txr = &mpc->mpc_rings[i][j];

			txr->phys_id = (uint16_t)HWRM_NA_SIGNATURE;
		}
	}
}

static void bnxt_map_tx_to_cp(struct bnxt_softc *softc)
{
	struct bnxt_mpc_info *mpc = softc->mpc_info;
	int cp_count = mpc->mpc_cp_rings_count;
	int i, j;

	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		int num = mpc->mpc_ring_count[i];

		if (i != BNXT_MPC_TCE_TYPE)
			continue;

		for (j = 0; j < num; j++) {
			struct bnxt_ring *txr = &mpc->mpc_rings[i][j];
			int cp_idx = j % cp_count;

			txr->cp_ring = &mpc->mpc_cp_rings[cp_idx];
		}
	}
}

struct bnxt_ring *bnxt_select_mpc_ring(struct bnxt_mpc_info *mpc)
{
	int idx;

	/* Callers should gate on a non-zero TCE ring count already, but
	 * guard the divide/index here too since this feeds a live TX path. */
	if (!mpc || !mpc->mpc_ring_count[BNXT_MPC_TCE_TYPE])
		return NULL;

	idx = PCPU_GET(cpuid) % mpc->mpc_ring_count[BNXT_MPC_TCE_TYPE];

	return &mpc->mpc_rings[BNXT_MPC_TCE_TYPE][idx];
}

int bnxt_hwrm_mpc_ring_alloc(struct bnxt_softc *softc)
{
	struct bnxt_mpc_info *mpc = softc->mpc_info;
	int i, j, rc;

	if (!mpc)
		return 0;

	bnxt_map_tx_to_cp(softc);
	for (i = 0; i < mpc->mpc_cp_rings_count; i++) {
		struct bnxt_cp_ring *nqr = &mpc->mpc_nq_rings[i];

		nqr->cons = 0;
		nqr->v_bit = 1;
		nqr->raw_cons = 0;
		nqr->toggle = 0;
		nqr->last_idx = UINT32_MAX;
		bnxt_mark_cpr_invalid(nqr);

		rc = bnxt_hwrm_ring_alloc(
		    softc, HWRM_RING_ALLOC_INPUT_RING_TYPE_NQ, &nqr->ring);

		bnxt_set_db_mask(softc, &nqr->ring,
		    HWRM_RING_ALLOC_INPUT_RING_TYPE_NQ);
		if (rc) {
			device_printf(softc->dev,
			    "%s: Failed to allocate HWRM notification ring %d, rc=%d\n",
			    __func__, i, rc);
			return rc;
		}

		softc->db_ops.bnxt_db_nq(nqr, 1);
	}

	for (i = 0; i < mpc->mpc_cp_rings_count; i++) {
		struct bnxt_cp_ring *cpr = &mpc->mpc_cp_rings[i];

		rc = bnxt_hwrm_stat_ctx_alloc(softc, cpr,
		    mpc->tx_stats[i].idi_paddr);
		if (rc) {
			device_printf(softc->dev,
			    "%s: Failed to allocate HWRM stats context for completion ring %d, rc=%d\n",
			    __func__, i, rc);
			return rc;
		}

		cpr->v_bit = 1;
		cpr->cons = 0;
		cpr->raw_cons = 0;
		cpr->toggle = 0;
		bnxt_mark_cpr_invalid(cpr);
		rc = bnxt_hwrm_ring_alloc(
		    softc, HWRM_RING_ALLOC_INPUT_RING_TYPE_L2_CMPL, &cpr->ring);
		bnxt_set_db_mask(softc, &cpr->ring,
		    HWRM_RING_ALLOC_INPUT_RING_TYPE_L2_CMPL);
		if (rc) {
			device_printf(softc->dev,
			    "%s: Failed to allocate HWRM completion ring %d, rc=%d\n",
			    __func__, i, rc);
			return rc;
		}

		softc->db_ops.bnxt_db_tx_cq(cpr, 1);
	}

	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		int num = mpc->mpc_ring_count[i];

		/* Currently only KTLS TX is supported */
		if (i != BNXT_MPC_TCE_TYPE)
			continue;

		for (j = 0; j < num; j++) {
			struct bnxt_ring *txr = &mpc->mpc_rings[i][j];

			txr->prod = 0;
			txr->cons = 0;
			txr->epoch_bit = false;
			memset(txr->tx_mpc_buf_ring, 0,
			    txr->ring_size * sizeof(struct bnxt_sw_mpc_tx_bd));

			rc = bnxt_hwrm_ring_alloc(
			    softc, HWRM_RING_ALLOC_INPUT_RING_TYPE_TX, txr);
			bnxt_set_db_mask(softc, txr,
			    HWRM_RING_ALLOC_INPUT_RING_TYPE_TX);
			if (rc) {
				device_printf(softc->dev,
				    "%s: Failed to allocate HWRM TX ring %d, rc=%d\n",
				    __func__, j, rc);
				return rc;
			}
			softc->db_ops.bnxt_db_tx(txr, 0);
		}
	}
	return 0;
}

int bnxt_hwrm_mpc_ring_free(struct bnxt_softc *softc)
{
	struct bnxt_mpc_info *mpc = softc->mpc_info;
	int i, j, rc;

	if (!mpc)
		return 0;

	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		int num = mpc->mpc_ring_count[i];

		/* Currently only KTLS TX is supported */
		if (i != BNXT_MPC_TCE_TYPE)
			continue;

		for (j = 0; j < num; j++) {
			struct bnxt_ring *txr = &mpc->mpc_rings[i][j];
			struct bnxt_cp_ring *cpr = txr->cp_ring;

			if (!cpr)
				continue;

			rc = bnxt_hwrm_ring_free(
			    softc, HWRM_RING_ALLOC_INPUT_RING_TYPE_TX, txr,
			    cpr->ring.phys_id);
			if (rc) {
				device_printf(softc->dev,
				    "%s: Failed to free HWRM TX ring %d (ring_id=%d, phys_id=%d), rc=%d\n",
				    __func__, j, txr->id, cpr->ring.phys_id, rc);
				return rc;
			}
			txr->phys_id = (uint16_t)HWRM_NA_SIGNATURE;
		}
	}

	for (i = 0; i < mpc->mpc_cp_rings_count; i++) {
		struct bnxt_cp_ring *cpr = &mpc->mpc_cp_rings[i];

		rc = bnxt_hwrm_ring_free(
		    softc, HWRM_RING_ALLOC_INPUT_RING_TYPE_L2_CMPL,
		    &cpr->ring, (uint16_t)HWRM_NA_SIGNATURE);
		if (rc) {
			device_printf(softc->dev,
			    "%s: Failed to free HWRM completion ring %d (ring_id=%d, phys_id=%d), rc=%d\n",
			    __func__, i, cpr->ring.id, cpr->ring.phys_id, rc);
			return rc;
		}

		rc = bnxt_hwrm_stat_ctx_free(softc, cpr);
		if (rc) {
			device_printf(softc->dev,
			    "%s: Failed to free HWRM stats context id %d for completion ring %d, rc=%d\n",
			    __func__, cpr->stats_ctx_id, i, rc);
			return rc;
		}

		cpr->ring.phys_id = (uint16_t)HWRM_NA_SIGNATURE;
	}

	for (i = 0; i < mpc->mpc_cp_rings_count; i++) {
		struct bnxt_cp_ring *nqr = &mpc->mpc_nq_rings[i];

		rc = bnxt_hwrm_ring_free(
		    softc, HWRM_RING_ALLOC_INPUT_RING_TYPE_NQ, &nqr->ring,
		    (uint16_t)HWRM_NA_SIGNATURE);
		if (rc) {
			device_printf(softc->dev,
			    "%s: Failed to free HWRM notification ring %d (ring_id=%d, phys_id=%d), rc=%d\n",
			    __func__, i, nqr->ring.id, nqr->ring.phys_id, rc);
			return rc;
		}
		nqr->ring.phys_id = (uint16_t)HWRM_NA_SIGNATURE;
	}
	return 0;
}

static void bnxt_mpc_process_cpr(struct bnxt_cp_ring *cpr)
{
	struct bnxt_softc *softc = cpr->ring.softc;

	/* Handle completions on the mpc completion ring */
	uint32_t raw_cons = cpr->raw_cons;
	uint32_t cons = cpr->cons;
	struct cmpl_base *cmpl;
	bool v_bit = cpr->v_bit;
	bool last_v_bit;
	uint32_t last_cons;
	uint16_t type;

	for (;;) {
		last_cons = cons;
		last_v_bit = v_bit;
		cmpl = &((struct cmpl_base *)cpr->ring.vaddr)[cons];

		if (!CMP_VALID(cmpl, v_bit))
			break;

		type = le16toh(cmpl->type) & CMPL_BASE_TYPE_MASK;
		switch (type) {
		case CMPL_BASE_TYPE_MID_PATH_SHORT:
		case CMPL_BASE_TYPE_MID_PATH_LONG: {
			int rc = bnxt_mpc_cmp(softc, cpr, &raw_cons);

			/* EBUSY: 2nd half of a MID_PATH_LONG completion hasn't
			 * landed yet; stop and retry later. Any other error is
			 * non-transient and already logged/skipped, so fall
			 * through and advance like the success case. */
			if (rc == EBUSY)
				goto done;
			break;
		}
		case CMPL_BASE_TYPE_HWRM_DONE:
			/* ignore this completion */
			break;
		default: {
			/* Rate-limit prints so a stuck ring feeding
			 * unrecognized types can't flood the console.
			 * Statics are unlocked; a best-effort limiter. */
			static struct timeval last;
			static int curpps;

			if (ppsratecheck(&last, &curpps, 5))
				device_printf(softc->dev,
				    "%s: Unknown completion type %u on ring %d (raw_cons=%u, cons=%u)\n",
				    __func__, type, cpr->ring.idx, raw_cons, cons);
			break;
		}
		}
		raw_cons++;
		/* Recompute from raw_cons rather than NEXT_CP_CONS_V(): a
		 * MID_PATH_LONG completion can wrap raw_cons inside
		 * bnxt_mpc_cmp(), which would desync v_bit otherwise. */
		cons = MPC_RING_CMP(cpr, raw_cons);
		v_bit = !(raw_cons & cpr->ring.ring_size);
	}
done:
	cpr->cons = last_cons;
	cpr->raw_cons = raw_cons;
	cpr->v_bit = last_v_bit;
	softc->db_ops.bnxt_db_tx_cq(cpr, 1);
}

static void bnxt_mpc_irq(void *arg)
{
	struct bnxt_cp_ring *cpr = arg;
	struct bnxt_softc *softc = cpr->ring.softc;
	nq_cn_t *cmp = (nq_cn_t *)cpr->ring.vaddr;
	uint32_t raw_cons = cpr->raw_cons;
	uint16_t nq_type, nqe_cnt = 0;
	uint32_t cons = cpr->cons;
	bool v_bit = cpr->v_bit;

	while (1) {
		if (!NQ_VALID(&cmp[cons], v_bit))
			goto done;

		nq_type = NQ_CN_TYPE_MASK & cmp[cons].type;

		if (NQE_CN_TYPE(nq_type) == NQ_CN_TYPE_CQ_NOTIFICATION) {
			struct bnxt_cp_ring *cpr2 =
			    &softc->mpc_info->mpc_cp_rings[cpr->ring.idx];
			cpr2->toggle = NQE_CN_TOGGLE(cmp[cons].type);
			bnxt_mpc_process_cpr(cpr2);
		}

		NEXT_CP_CONS_V(&cpr->ring, cons, v_bit);
		raw_cons++;
		nqe_cnt++;
	}
done:
	if (nqe_cnt) {
		cpr->cons = cons;
		cpr->raw_cons = raw_cons;
		cpr->v_bit = v_bit;
	}

	softc->db_ops.bnxt_db_nq(cpr, 1);
}

int bnxt_mpc_irq_setup(struct bnxt_softc *softc)
{
	int mpc_base_idx = softc->scctx->isc_ntxqsets;
	struct bnxt_mpc_info *mpc = softc->mpc_info;
	int mpc_cp_rings_count = 0;
	int rc, i, j;

	if (mpc)
		mpc_cp_rings_count = mpc->mpc_cp_rings_count;

	rc = bnxt_populate_irq(softc, mpc_cp_rings_count);
	if (rc) {
		device_printf(softc->dev,
		    "%s: Failed to populate IRQs for %d MPC completion rings, rc=%d\n",
		    __func__, mpc_cp_rings_count, rc);
		return rc;
	}

	for (i = mpc_base_idx, j = 0; j < mpc_cp_rings_count; i++, j++) {
		struct bnxt_cp_ring *nqr = &mpc->mpc_nq_rings[j];
		struct resource *irq_res;
		int rid;

		nqr->msix_vec = softc->irq_tbl[i].vector;

		rid = 1 + i;
		irq_res = bus_alloc_resource_any(softc->dev, SYS_RES_IRQ, &rid,
		    RF_ACTIVE);
		if (irq_res == NULL) {
			/* Release any IRQs already set up for j' < j. */
			bnxt_mpc_irq_cleanup(softc);
			return ENOMEM;
		}

		nqr->irq_res = irq_res;

		rc = bus_setup_intr(softc->dev, irq_res, INTR_TYPE_NET | INTR_MPSAFE,
		    NULL, bnxt_mpc_irq, nqr, &nqr->irq_cookie);
		if (rc) {
			device_printf(softc->dev,
			    "%s: Failed to setup IRQ for msix_vec %d, MPC notification ring %d, rc=%d\n",
			    __func__, nqr->msix_vec, j, rc);
			bus_release_resource(softc->dev, SYS_RES_IRQ, rid, irq_res);
			nqr->irq_res = NULL;
			/* Release any IRQs already set up for j' < j. */
			bnxt_mpc_irq_cleanup(softc);
			return rc;
		}
	}

	return 0;
}

void bnxt_mpc_irq_cleanup(struct bnxt_softc *softc)
{
	struct bnxt_mpc_info *mpc = softc->mpc_info;
	int j;

	if (!mpc)
		return;

	for (j = 0; j < mpc->mpc_cp_rings_count; j++) {
		struct bnxt_cp_ring *nqr = &mpc->mpc_nq_rings[j];
		int rid;

		if (nqr->irq_res != NULL) {
			bus_teardown_intr(softc->dev, nqr->irq_res, nqr->irq_cookie);
			nqr->irq_cookie = NULL;
			rid = rman_get_rid(nqr->irq_res);
			bus_release_resource(softc->dev, SYS_RES_IRQ, rid,
			    nqr->irq_res);
			nqr->irq_res = NULL;
		}
	}
}

static inline uint32_t bnxt_tx_avail(struct bnxt_softc *softc,
    struct bnxt_ring *txr)
{
	uint32_t used, prod, cons;

	prod = atomic_load_acq_16(&txr->prod);
	cons = atomic_load_acq_16(&txr->cons);

	if (prod >= cons)
		used = prod - cons;
	else {
		used = (txr->ring_size - cons) + prod;
	}

	return (txr->ring_size - 1 - used);
}

int bnxt_start_xmit_mpc(struct bnxt_softc *softc, struct bnxt_ring *txr,
    void *data, uint len, unsigned long handle)
{
	uint32_t bds, total_bds, bd_space, free_size;
	struct bnxt_sw_mpc_tx_bd *tx_buf;
	struct tx_bd_long *txbd;
	uint16_t prod;

	/* Caller must hold txr->tx_lock across this sequence; assert rather
	 * than acquire since bnxt_xmit_crypto_cmd() needs it held longer. */
	mtx_assert(&txr->tx_lock, MA_OWNED);

	bds = howmany(len, sizeof(*txbd));
	total_bds = bds + 1;
	free_size = bnxt_tx_avail(softc, txr);
	if (free_size < total_bds)
		return EBUSY;

	prod = txr->prod;
	/* Wraps the ring at most once since total_bds < ring_size; toggle
	 * epoch_bit to match, as prod advances directly here, not per-BD. */
	if ((uint32_t)prod + total_bds >= txr->ring_size)
		txr->epoch_bit = !txr->epoch_bit;
	txbd = &((struct tx_bd_long *)txr->vaddr)[prod];

	tx_buf = &txr->tx_mpc_buf_ring[prod];
	tx_buf->handle = handle;
	tx_buf->inline_bds = total_bds;

	txbd->flags_type =
	    htole16(TX_BD_MP_CMD_TYPE_TX_BD_MP_CMD |
		(total_bds << TX_BD_LONG_FLAGS_BD_CNT_SFT));
	txbd->len = htole16(len);
	txbd->opaque = SET_TX_OPAQUE(txr, prod, total_bds);

	prod = NEXT_TX(txr, prod);
	txbd = &((struct tx_bd_long *)txr->vaddr)[prod];

	bd_space = txr->ring_size - prod;
	if (bd_space < bds) {
		uint32_t len0 = bd_space * sizeof(*txbd);

		memcpy(txbd, data, len0);
		prod = ADVANCE_TX_BY(txr, prod, bd_space);
		txbd = &((struct tx_bd_long *)txr->vaddr)[prod];
		bds -= bd_space;
		len -= len0;
		data = (uint8_t *)data + len0;
	}
	memcpy(txbd, data, len);
	prod = ADVANCE_TX_BY(txr, prod, bds);
	/* Release-store pairs with the atomic_load_acq_16(&txr->prod) in
	 * bnxt_tx_avail() and bnxt_adv_mpc_cons(). */
	atomic_store_rel_16(&txr->prod, prod);

	softc->db_ops.bnxt_db_tx(txr, prod);

	return 0;
}

static bool bnxt_mpc_unsolicit(struct mpc_cmp *mpcmp)
{
	uint32_t client = MPC_CMP_CLIENT_TYPE(mpcmp);

	if (client != MPC_CMP_CLIENT_TCE && client != MPC_CMP_CLIENT_RCE &&
	    client != MPC_CMP_CLIENT_TE_CFA && client != MPC_CMP_CLIENT_RE_CFA)
		return false;
	return MPC_CMP_UNSOLICIT_SUBTYPE(mpcmp);
}

static void bnxt_adv_mpc_cons(struct bnxt_softc *softc, struct bnxt_ring *txr)
{
	struct tx_bd_long *bd_base = (struct tx_bd_long *)txr->vaddr;
	struct bnxt_sw_mpc_tx_bd *mpc_buf;
	uint16_t tx_cons = txr->cons;
	uint16_t tail_bds;
	uint8_t inline_bds;

	mpc_buf = &txr->tx_mpc_buf_ring[tx_cons];
	do {
		inline_bds = mpc_buf->inline_bds;
		/* Scrub the inline-BD region (held CE_ADD command +
		 * AES-GCM key) before the slot is reused. */
		if (inline_bds > 0) {
			tail_bds = txr->ring_size - tx_cons;
			if (tail_bds >= inline_bds) {
				explicit_bzero(&bd_base[tx_cons],
				    inline_bds * sizeof(*bd_base));
			} else {
				explicit_bzero(&bd_base[tx_cons],
				    tail_bds * sizeof(*bd_base));
				explicit_bzero(&bd_base[0],
				    (inline_bds - tail_bds) * sizeof(*bd_base));
			}
		}
		tx_cons = ADVANCE_TX_BY(txr, tx_cons, mpc_buf->inline_bds);
		/* Pairs with bnxt_tx_avail()'s atomic_load_acq_16(&txr->cons). */
		atomic_store_rel_16(&txr->cons, tx_cons);
		if (tx_cons == atomic_load_acq_16(&txr->prod))
			break;
		mpc_buf = &txr->tx_mpc_buf_ring[tx_cons];
	} while (mpc_buf->handle == BNXT_INV_MPC_HDL);
}

int bnxt_mpc_cmp(struct bnxt_softc *softc, struct bnxt_cp_ring *cpr,
    uint32_t *raw_cons)
{
	struct bnxt_mpc_info *mpc = softc->mpc_info;
	struct bnxt_cmpl_entry cmpl_entry_arr[2];
	uint16_t cons = MPC_RING_CMP(cpr, *raw_cons);
	struct mpc_cmp *mpcmp, *mpcmp1;
	uint32_t tmp_raw_cons = *raw_cons;
	unsigned long handle = 0;
	uint32_t client, cmpl_num;
	uint8_t type;

	mpcmp = &((struct mpc_cmp *)cpr->ring.vaddr)[cons];
	type = MPC_CMP_CMP_TYPE(mpcmp);
	cmpl_entry_arr[0].cmpl = mpcmp;
	cmpl_entry_arr[0].len = sizeof(*mpcmp);
	cmpl_num = 1;

	if (type == MPC_CMP_TYPE_MID_PATH_LONG) {
		tmp_raw_cons = tmp_raw_cons + 1;
		cons = MPC_RING_CMP(cpr, tmp_raw_cons);
		mpcmp1 = &((struct mpc_cmp *)cpr->ring.vaddr)[cons];
		if (!MPC_CMP_VALID(cpr, mpcmp1, tmp_raw_cons)) {
			/* Expected/transient: caller retries this entry later.
			 * Rate-limit (unlocked statics, best-effort) so a
			 * slow second half can't flood the console. */
			static struct timeval last;
			static int curpps;

			if (ppsratecheck(&last, &curpps, 5))
				device_printf(softc->dev,
				    "%s: Invalid completion entry on ring %d (raw_cons=%u, cons=%u, v_bit=%d)\n",
				    __func__, cpr->ring.idx, tmp_raw_cons, cons, cpr->v_bit);
			return EBUSY;
		}
		/*
		 * The valid test of the entry must be done first before
		 * reading any further.
		 */
		atomic_thread_fence_acq();
		if (mpcmp1 == mpcmp + 1) {
			cmpl_entry_arr[cmpl_num - 1].len += sizeof(*mpcmp1);
		} else {
			cmpl_entry_arr[cmpl_num].cmpl = mpcmp1;
			cmpl_entry_arr[cmpl_num].len = sizeof(*mpcmp1);
			cmpl_num++;
		}
	}

	client = MPC_CMP_CLIENT_TYPE(mpcmp) >> MPC_CMP_CLIENT_SFT;

	if (!bnxt_mpc_unsolicit(mpcmp)) {
		uint16_t ring_idx, mpc_type;
		struct bnxt_sw_mpc_tx_bd *mpc_buf;
		struct bnxt_ring *txr;
		uint16_t tx_cons, buf_idx;
		uint32_t opaque;

		opaque = mpcmp->mpc_cmp_opaque;
		ring_idx = TX_OPAQUE_RING(opaque) & BNXT_RING_MPC_IDX_RING_MASK;
		mpc_type = TX_OPAQUE_RING(opaque) >> BNXT_RING_MPC_IDX_TYPE_SFT;
		if (mpc_type != client) {
			/*
			 * Unlocked statics: see the same note in
			 * bnxt_mpc_process_cpr()'s unknown-completion-type case.
			 */
			static struct timeval last;
			static int curpps;

			if (ppsratecheck(&last, &curpps, 5))
				device_printf(softc->dev,
				    "%s: Wrong opaque 0x%x on completion ring %d, "
				    "expected client type 0x%x, actual type 0x%x (ring_idx=%d)\n",
				    __func__, opaque, cpr->ring.idx, client, mpc_type, ring_idx);
			/* Corrupt, not pending: advance past both halves so
			 * bnxt_mpc_process_cpr() doesn't spin on it forever. */
			*raw_cons = tmp_raw_cons;
			return EINVAL;
		}
		if (mpc_type >= BNXT_MPC_TYPE_MAX || mpc->mpc_rings[mpc_type] == NULL) {
			bnxt_log_live(softc, BNXT_LOGGER_L2,
			    "%s: Invalid mpc_type %d, completion ring %d, opaque=0x%x, client_type=0x%x\n",
			    __func__, mpc_type, cpr->ring.idx, opaque, client);
			*raw_cons = tmp_raw_cons;
			return EINVAL;
		}
		if (ring_idx >= mpc->mpc_ring_count[mpc_type]) {
			bnxt_log_live(softc, BNXT_LOGGER_L2,
			    "%s: Invalid ring_idx %d (max=%d), completion ring %d, opaque=0x%x, client_type=0x%x\n",
			    __func__, ring_idx, mpc->mpc_ring_count[mpc_type], cpr->ring.idx, opaque, client);
			*raw_cons = tmp_raw_cons;
			return EINVAL;
		}

		txr = &mpc->mpc_rings[mpc_type][ring_idx];
		tx_cons = txr->cons;
		buf_idx = TX_OPAQUE_IDX(opaque);
		if (buf_idx >= txr->ring_size) {
			bnxt_log_live(softc, BNXT_LOGGER_L2,
			    "%s: Invalid buf_idx %d (ring_size=%d), completion ring %d, opaque=0x%x\n",
			    __func__, buf_idx, txr->ring_size, cpr->ring.idx, opaque);
			*raw_cons = tmp_raw_cons;
			return EINVAL;
		}
		mpc_buf = &txr->tx_mpc_buf_ring[buf_idx];
		handle = mpc_buf->handle;
		mpc_buf->handle = BNXT_INV_MPC_HDL;

		if (client == BNXT_MPC_TCE_TYPE || client == BNXT_MPC_RCE_TYPE)
			bnxt_ktls_mpc_cmp(softc, client, handle, cmpl_entry_arr,
			    cmpl_num);

		if (RING_TX(txr, tx_cons) == buf_idx)
			bnxt_adv_mpc_cons(softc, txr);
	}
	*raw_cons = tmp_raw_cons;
	return 0;
}
