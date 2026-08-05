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

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/stdatomic.h>
#include <sys/ktls.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/kernel.h>
#include <sys/counter.h>
#include <sys/kthread.h>
#include <sys/bitstring.h>
#include <sys/queue.h>
#include <machine/atomic.h>
#include <vm/uma.h>

#include <net/if.h>
#include <net/ethernet.h>
#include <net/pfil.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/tcp_lro.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp_var.h>

#include "opt_kern_tls.h"
#include "opt_rss.h"
#include "opt_ratelimit.h"

#include "bnxt.h"
#include "bnxt_hwrm.h"
#include "bnxt_ktls.h"
#include "bnxt_mpc.h"

#include <opencrypto/cryptodev.h>

#ifdef KERN_TLS

static if_snd_tag_free_t bnxt_tls_snd_tag_free;

static const struct if_snd_tag_sw bnxt_tls_snd_tag_sw = {
	.snd_tag_free = bnxt_tls_snd_tag_free,
	.type = IF_SND_TAG_TYPE_TLS
};
static int bnxt_ktls_dev_add(struct bnxt_softc* softc,
			     const struct tls_session_params* en,
			     struct bnxt_ktls_offload_ctx_tx* kctx_tx);
static void bnxt_ktls_dev_del(struct bnxt_softc* softc,
			      struct bnxt_ktls_offload_ctx_tx* kctx_tx);
static int bnxt_crypto_del(struct bnxt_softc* softc,
			   struct bnxt_ktls_offload_ctx_tx* kctx_tx,
			   uint8_t type);
static void bnxt_tls_work(void *context, int pending);
static inline void
bnxt_calc_mpc_cmp_time(struct bnxt_softc* softc, unsigned long rtt);

static int
bnxt_tls_tag_import(void *arg, void **store, int cnt, int domain, int flags)
{
	struct bnxt_ktls_offload_ctx_tx *kctx_tx;
	int i;

	for (i = 0; i != cnt; i++) {
		kctx_tx = malloc(sizeof(*kctx_tx), M_DEVBUF, flags | M_ZERO);
		if (kctx_tx == NULL)
			return (i);
		TASK_INIT(&kctx_tx->work_task, 0, bnxt_tls_work, kctx_tx);
		store[i] = kctx_tx;
	}
	return (i);
}

static void
bnxt_tls_tag_release(void *arg, void **store, int cnt)
{
	struct bnxt_ktls_offload_ctx_tx *kctx_tx;
	if_ctx_t ctx;
	struct bnxt_softc* priv;
	int i;

	for (i = 0; i != cnt; i++) {
		kctx_tx = store[i];
		if (kctx_tx->tag.ifp) {
			ctx = if_getsoftc(kctx_tx->tag.ifp);
			priv = iflib_get_softc(ctx);
			if (priv->ktls_info->tq)
				taskqueue_drain(priv->ktls_info->tq, &kctx_tx->work_task);
		}
		free(kctx_tx, M_DEVBUF);
	}
}

static void
bnxt_tls_tag_zfree(struct bnxt_ktls_offload_ctx_tx *kctx_tx)
{
	/* make sure any unhandled taskqueue events are ignored */
	kctx_tx->state = BNXT_TLS_STATE_FREED;

	/* avoid leaking keys */
	memset(&kctx_tx->crypto_info, 0, sizeof(kctx_tx->crypto_info));

	/* return tag to UMA */
	uma_zfree(kctx_tx->ktls->zone, kctx_tx);
}

static void bnxt_tls_snd_tag_free(struct m_snd_tag* pmt)
{
	if_ctx_t ctx;
	struct bnxt_softc* priv;
	struct bnxt_ktls_offload_ctx_tx* kctx_tx =
		__containerof(pmt, struct bnxt_ktls_offload_ctx_tx, tag);

	kctx_tx->state = BNXT_TLS_STATE_FREE;
	/* Hold a reference until bnxt_tls_work() runs, to avoid a free-vs-read race. */
	atomic_add_32(&kctx_tx->mpc_inflight, 1);

	ctx = if_getsoftc(kctx_tx->tag.ifp);
	priv = iflib_get_softc(ctx);
	bnxt_ktls_dev_del(priv, kctx_tx);

	taskqueue_enqueue(priv->ktls_info->tq, &kctx_tx->work_task);
}

static void
bnxt_tls_work(void *context, int pending)
{
	struct bnxt_ktls_offload_ctx_tx *kctx_tx = context;

	/* Drop the reference taken in bnxt_tls_snd_tag_free(); free once last. */
	if (atomic_fetchadd_32(&kctx_tx->mpc_inflight, -1) == 1)
		bnxt_tls_tag_zfree(kctx_tx);
}

int bnxt_tls_snd_tag_alloc(if_t ifp, union if_snd_tag_alloc_params* params,
			   struct m_snd_tag** ppmt)
{
	const struct if_snd_tag_sw* snd_tag_sw;
	struct bnxt_softc* priv;
	struct bnxt_tls_info* ktls;
	if_ctx_t ctx;
	struct bnxt_ktls_offload_ctx_tx* kctx_tx = NULL;
	const struct tls_session_params* en;
	int error;

	ctx = if_getsoftc(ifp);
	priv = iflib_get_softc(ctx);
	ktls = priv->ktls_info;

	if (!bnxt_drv_state_test(priv, BNXT_STATE_OPEN))
		return (EPROTONOSUPPORT);

	if (atomic_load_acq_int(&priv->detached))
		return (EPROTONOSUPPORT);

	/*
	 * Re-check `detached` after incrementing snd_tag_alloc_ref so
	 * bnxt_detach()'s drain loop can see this ref and wait for it.
	 */
	atomic_add_32(&priv->ktls_info->snd_tag_alloc_ref, 1);

	if (atomic_load_acq_int(&priv->detached)) {
		atomic_subtract_32(&priv->ktls_info->snd_tag_alloc_ref, 1);
		return (EPROTONOSUPPORT);
	}

	if (priv->ktls_info->init == 0) {
		counter_u64_add(ktls->err_counters[BNXT_KTLS_TX_UNINITIALIZED], 1);
		atomic_subtract_32(&priv->ktls_info->snd_tag_alloc_ref, 1);
		BNXT_DEBUG(priv->dev, "%s:%d: ifp:(%pa) ktls is not initialized\n",
		    __func__, __LINE__, ifp);
		return (EPROTONOSUPPORT);
	}

	/* allocate new tag from zone, if any */
	kctx_tx = uma_zalloc(priv->ktls_info->zone, M_NOWAIT);
	if (kctx_tx == NULL) {
		counter_u64_add(ktls->err_counters[BNXT_KTLS_TX_CTX_ALLOC_FAILED], 1);
		atomic_subtract_32(&priv->ktls_info->snd_tag_alloc_ref, 1);
		BNXT_DEBUG(priv->dev, "%s:%d: ifp:(%pa) kctx_tx uma_zalloc failed\n",
		    __func__, __LINE__, ifp);
		return (ENOMEM);
	}
	/* Set before any goto failure: bnxt_tls_tag_zfree() needs it. */
	kctx_tx->ktls = priv->ktls_info;

	en = &params->tls.tls->params;

	/* only TLS v1.2 and v1.3 is currently supported */
	if (en->tls_vmajor != TLS_MAJOR_VER_ONE ||
	    (en->tls_vminor != TLS_MINOR_VER_TWO
#ifdef TLS_MINOR_VER_THREE
	     && en->tls_vminor != TLS_MINOR_VER_THREE
#endif
	     )) {
		BNXT_DEBUG(priv->dev, "kTLS snd_tag_alloc: unsupported TLS version %u.%u\n",
		    en->tls_vmajor, en->tls_vminor);
		error = EPROTONOSUPPORT;
		goto failure;
	}

	switch (en->cipher_algorithm) {
	case CRYPTO_AES_NIST_GCM_16:
		if ((en->cipher_key_len != 128 / 8) &&
		    (en->cipher_key_len != 256 / 8)) {
			BNXT_DEBUG(priv->dev, "kTLS snd_tag_alloc: unsupported cipher key len %u\n",
			    en->cipher_key_len * 8);
			error = EPROTONOSUPPORT;
			goto failure;
		}
		break;

	default:
		BNXT_DEBUG(priv->dev, "kTLS snd_tag_alloc: unsupported cipher algorithm %u\n",
		    en->cipher_algorithm);
		error = EPROTONOSUPPORT;
		goto failure;
	}

	/* Fetch TCP seq number from struct tcpcb */
	kctx_tx->tcp_seq_no = intotcpcb(params->tls.tls->inp)->snd_nxt;

	/*
	 * next_seqno currently only carries a valid TLS record seq number
	 * for TLS v1.0; kernel support for other versions is still needed.
	 */
#ifdef KTLS_REC_SEQ_NUMBER
	kctx_tx->rec_seq = params->tls.tls->next_seqno;
#endif

	kctx_tx->state = BNXT_TLS_STATE_INIT;

	error = bnxt_ktls_dev_add(priv, en, kctx_tx);
	if (error) {
		counter_u64_add(ktls->err_counters[BNXT_KTLS_TX_DEV_ADD_FAILED], 1);
		error = EINVAL;
		goto failure;
	}

	if (params->hdr.type == IF_SND_TAG_TYPE_TLS) {
		snd_tag_sw = &bnxt_tls_snd_tag_sw;
	} else {
		counter_u64_add(ktls->err_counters[BNXT_KTLS_TX_UNSUPPORTED_TLS_HDR], 1);
		BNXT_DEBUG(priv->dev, "kTLS snd_tag_alloc: unsupported snd_tag type %u\n",
		    params->hdr.type);
		bnxt_ktls_dev_del(priv, kctx_tx);
		error = EOPNOTSUPP;
		goto failure;
	}

	MPASS(kctx_tx->tag.refcount == 0);
	m_snd_tag_init(&kctx_tx->tag, ifp, snd_tag_sw);
	*ppmt = &kctx_tx->tag;
	atomic_subtract_32(&priv->ktls_info->snd_tag_alloc_ref, 1);
	BNXT_DEBUG(priv->dev, "[KID:%u]: TAG_ALLOC: tcp_seqno: %u"
	    " record_no: %lu\n", kctx_tx->kid, kctx_tx->tcp_seq_no, kctx_tx->rec_seq);

	return (0);

failure:
	kctx_tx->state = BNXT_TLS_STATE_FREE;
	/* Same reference-then-release pattern as bnxt_tls_work(), to avoid a double free. */
	atomic_add_32(&kctx_tx->mpc_inflight, 1);
	if (atomic_fetchadd_32(&kctx_tx->mpc_inflight, -1) == 1)
		bnxt_tls_tag_zfree(kctx_tx);
	atomic_subtract_32(&priv->ktls_info->snd_tag_alloc_ref, 1);
	return (error);
}

void bnxt_alloc_ktls_info(struct bnxt_softc* softc, uint16_t max_keys,
			  bool partition_cap, uint16_t ctxs_per_partition)
{
	struct bnxt_tls_info* ktls = softc->ktls_info;

	if (BNXT_VF(softc))
		return;

	if (!ktls) {
		bool partition_mode = false;
		struct bnxt_kctx* kctx;
		uint16_t batch_sz = 0;
		int i;

		ktls = malloc(sizeof(*ktls), M_DEVBUF, M_NOWAIT | M_ZERO);

		if (ktls == NULL) {
			device_printf(softc->dev, "kTLS info alloc failed\n");
			return;
		}

		if (partition_cap) {
			batch_sz = ctxs_per_partition;
			if (batch_sz && batch_sz <= BNXT_KID_BATCH_SIZE)
				partition_mode = true;
		}
		for (i = 0; i < BNXT_MAX_CRYPTO_KEY_TYPE; i++) {
			kctx = &ktls->kctx[i];
			kctx->type = i;
			kctx->max_ctx = softc->max_ktls_entries;
			TAILQ_INIT(&kctx->list);
			mtx_init(&kctx->lock, "BNXT KCTL LOCK", NULL,
				 MTX_DEF | MTX_NOWITNESS);
			cv_init(&kctx->alloc_pending_cv, "bnxt_ktls_alloc");
			atomic_store_32(&kctx->alloc_pending, 0);
			if (partition_mode) {
				int bmap_sz;

				bmap_sz = howmany(kctx->max_ctx, batch_sz);
				kctx->partition_bmap_nbits = bmap_sz;
				kctx->partition_bmap =
					bit_alloc(bmap_sz, M_DEVBUF, M_NOWAIT);
				if (kctx->partition_bmap == NULL) {
					BNXT_DEBUG(softc->dev,
					    "kTLS partition_bmap alloc failed, disabling partition mode\n");
					partition_mode = false;
				}
			}
		}
		atomic_store_32(&ktls->snd_tag_alloc_ref, 0);
		ktls->partition_mode = partition_mode;
		ktls->ctxs_per_partition = batch_sz;

		atomic_store_32(&ktls->pending, 0);

		softc->ktls_info = ktls;

		/* Create DMA descriptor TAG */
		if ((bus_dma_tag_create(
		    bus_get_dma_tag(softc->dev),
		    1,                          /* any alignment */
		    0,                          /* no boundary */
		    BUS_SPACE_MAXADDR,          /* lowaddr */
		    BUS_SPACE_MAXADDR,          /* highaddr */
		    NULL, NULL,                 /* filter, filterarg */
		    65536,
		    BNXT_MAX_NUM_SEGS,
		    16384,
		    0,                          /* flags */
		    NULL, NULL,                 /* lockfunc, lockfuncarg */
		    &ktls->dma_tag))) {
			device_printf(softc->dev, "kTLS dma tag alloc failed\n");
			/*
			 * Back out ktls_info entirely (not via bnxt_free_ktls_info(),
			 * which would call bus_dma_tag_destroy() on this NULL tag).
			 */
			softc->ktls_info = NULL;
			for (i = 0; i < BNXT_MAX_CRYPTO_KEY_TYPE; i++) {
				kctx = &ktls->kctx[i];
				cv_destroy(&kctx->alloc_pending_cv);
				mtx_destroy(&kctx->lock);
				if (kctx->partition_bmap != NULL)
					free(kctx->partition_bmap, M_DEVBUF);
			}
			free(ktls, M_DEVBUF);
			return;
		}
	}
	ktls->max_key_ctxs_alloc = max_keys;
}

void bnxt_ktls_del_all(struct bnxt_softc *softc)
{
	struct bnxt_tls_info *ktls = softc->ktls_info;
	struct bnxt_kid_info *kid;
	struct bnxt_kctx *kctx;
	int i, j;

	if (!ktls)
		return;

	/*
	 * Called during FW reset, before bnxt_clear_ktls(). FW reinitialises
	 * its key-context table, so just mark every KID free in software.
	 */
	/* Shutting down; no need to hold kctx locks. */
	for (i = 0; i < BNXT_MAX_CRYPTO_KEY_TYPE; i++) {
		kctx = &ktls->kctx[i];
		TAILQ_FOREACH(kid, &kctx->list, list) {
			for (j = 0; j < (int)kid->count; j++)
				bit_set(kid->ids, j);
		}
	}
}

void bnxt_clear_ktls(struct bnxt_softc *softc)
{
	struct bnxt_tls_info *ktls = softc->ktls_info;
	struct bnxt_kid_info *kid, *tmp;
	struct bnxt_kctx *kctx;
	int i;

	if (!ktls)
		return;

	/* Shutting down or FW reset, no need to protect the lists. */
	for (i = 0; i < BNXT_MAX_CRYPTO_KEY_TYPE; i++) {
		kctx = &ktls->kctx[i];
		TAILQ_FOREACH_SAFE(kid, &kctx->list, list, tmp) {
			TAILQ_REMOVE(&kctx->list, kid, list);
			if (kid->ids != NULL)
				free(kid->ids, M_DEVBUF);
			free(kid, M_DEVBUF);
		}
		if (kctx->partition_bmap != NULL) {
			free(kctx->partition_bmap, M_DEVBUF);
			kctx->partition_bmap = NULL;
		}
		kctx->total_alloc = 0;
	}

	/*
	 * partition_bmap is now NULL for every kctx above; without this, a
	 * later bnxt_hwrm_key_ctx_alloc() would still take the partition-mode
	 * path (nothing re-negotiates it after a reset) and bit_ffc_at() a
	 * NULL bitmap. Also bump reset_gen so bnxt_ktls_xmit() can catch
	 * READY tags whose firmware KID this reset erased.
	 */
	ktls->partition_mode = false;
	atomic_add_32(&ktls->reset_gen, 1);
}

void bnxt_free_ktls_counters(struct bnxt_softc* softc)
{
	uint8_t i;
	struct bnxt_tls_info *ktls = softc->ktls_info;

	if (!ktls)
		return;

	for (i = 0; i < BNXT_MPC_CMP_TIME_MAX_COUNTERS; i++) {
		if (ktls->mpc_cmp_time[i]) {
			counter_u64_free(ktls->mpc_cmp_time[i]);
			ktls->mpc_cmp_time[i] = NULL;
		}
	}

	for (i = 0; i < BNXT_KTLS_MAX_COUNTERS; i++) {
		if (ktls->counters[i]) {
			counter_u64_free(ktls->counters[i]);
			ktls->counters[i] = NULL;
		}
	}

	for (i = 0; i < BNXT_KTLS_ERR_MAX_COUNTERS; i++) {
		if (ktls->err_counters[i]) {
			counter_u64_free(ktls->err_counters[i]);
			ktls->err_counters[i] = NULL;
		}
	}
}

void bnxt_alloc_ktls_counters(struct bnxt_softc* softc)
{
	uint8_t i;
	struct bnxt_tls_info *ktls = softc->ktls_info;

	if (!ktls)
		return;

	for (i = 0; i < BNXT_MPC_CMP_TIME_MAX_COUNTERS; i++) {
		if (!ktls->mpc_cmp_time[i])
			ktls->mpc_cmp_time[i] = counter_u64_alloc(M_WAITOK);
	}

	for (i = 0; i < BNXT_KTLS_MAX_COUNTERS; i++) {
		if (!ktls->counters[i])
			ktls->counters[i] = counter_u64_alloc(M_WAITOK);
	}

	for (i = 0; i < BNXT_KTLS_ERR_MAX_COUNTERS; i++) {
		if (!ktls->err_counters[i])
			ktls->err_counters[i] = counter_u64_alloc(M_WAITOK);
	}

}

void bnxt_free_ktls_info(struct bnxt_softc* softc)
{
	struct bnxt_tls_info* ktls = softc->ktls_info;
	int i;

	if (!ktls)
		return;

	/* Retry a busy sysctl_ctx_free(); don't free ktls while its sysctl nodes are still live. */
	if (softc->ktls_stats_oid != NULL) {
		for (i = 0; i < 10; i++) {
			if (sysctl_ctx_free(&softc->ktls_stats) == 0) {
				softc->ktls_stats_oid = NULL;
				break;
			}
			pause("bnxtktlssysctl", hz / 10);
		}
	}
	if (softc->mpc_cmp_time_stats_oid != NULL) {
		for (i = 0; i < 10; i++) {
			if (sysctl_ctx_free(&softc->mpc_cmp_time_stats) == 0) {
				softc->mpc_cmp_time_stats_oid = NULL;
				break;
			}
			pause("bnxtktlssysctl", hz / 10);
		}
	}
	if (softc->ktls_stats_oid != NULL || softc->mpc_cmp_time_stats_oid != NULL) {
		device_printf(softc->dev,
		    "kTLS: stats sysctl busy, leaking kTLS info to avoid a use-after-free\n");
		return;
	}

	bnxt_clear_ktls(softc);
	bus_dma_tag_destroy(ktls->dma_tag);
	if (ktls->mpc_zone != NULL)
		uma_zdestroy(ktls->mpc_zone);

	if (ktls->init) {
		ktls->init = 0;
		if (ktls->tq != NULL) {
			taskqueue_quiesce(ktls->tq);
			taskqueue_free(ktls->tq);
			ktls->tq = NULL;
		}
		uma_zdestroy(ktls->zone);
	}

	for (i = 0; i < BNXT_MAX_CRYPTO_KEY_TYPE; i++) {
		cv_destroy(&ktls->kctx[i].alloc_pending_cv);
		if (mtx_initialized(&ktls->kctx[i].lock))
			mtx_destroy(&ktls->kctx[i].lock);
	}

	free(ktls, M_DEVBUF);
	softc->ktls_info = NULL;
}

void bnxt_hwrm_reserve_pf_key_ctxs(struct bnxt_softc* softc,
				   struct hwrm_func_cfg_input* req,
				   uint8_t type)
{
	struct bnxt_tls_info* tls = softc->ktls_info;
	struct bnxt_hw_resc* hw_resc = &softc->hw_resc;
	struct bnxt_hw_tls_resc* tls_resc;
	uint32_t tx, rx;

	if (!tls)
		return;

	tls_resc = &hw_resc->tls_resc[type];
	tx = min(BNXT_TCK(tls).max_ctx, tls_resc->max_tx_key_ctxs);
	rx = min(BNXT_RCK(tls).max_ctx, tls_resc->max_rx_key_ctxs);
	if (type == BNXT_CRYPTO_TYPE_KTLS) {
		req->num_ktls_tx_key_ctxs = htole32(tx);
		req->num_ktls_rx_key_ctxs = htole32(rx);
		req->enables |= htole32(
			HWRM_FUNC_CFG_INPUT_ENABLES_KTLS_TX_KEY_CTXS |
			HWRM_FUNC_CFG_INPUT_ENABLES_KTLS_RX_KEY_CTXS);
	}
}

static int __bnxt_partition_alloc(struct bnxt_kctx* kctx, uint32_t* id)
{
	int max = kctx->partition_bmap_nbits;
	ssize_t next;

	bit_ffc_at(kctx->partition_bmap, kctx->next, max, &next);
	if (next == -1)
		bit_ffc(kctx->partition_bmap, max, &next);
	if (next == -1)
		return ENOSPC;
	*id = next;
	kctx->next = next;
	return 0;
}

static int bnxt_partition_alloc(struct bnxt_kctx* kctx, uint32_t* id)
{
	int rc;

	mtx_lock(&kctx->lock);
	do {
		rc = __bnxt_partition_alloc(kctx, id);
		if (rc) {
			mtx_unlock(&kctx->lock);
			return rc;
		}
	} while (bnxt_test_and_set_bit_nonatomic(*id, kctx->partition_bmap));
	mtx_unlock(&kctx->lock);
	return 0;
}

static int bnxt_key_ctx_store(struct bnxt_softc* softc, __le32* key_buf,
			      uint32_t num, bool contig, struct bnxt_kctx* kctx,
			      uint32_t* id)
{
	struct bnxt_kid_info* kid;
	uint32_t i;
	int j;

	for (i = 0; i < num;) {
		kid = malloc(sizeof(*kid), M_DEVBUF, M_NOWAIT | M_ZERO);
		if (kid == NULL) {
			device_printf(softc->dev, "kTLS kid alloc failed\n");
			return ENOMEM;
		}
		kid->ids = bit_alloc(BNXT_KID_BATCH_SIZE, M_DEVBUF, M_NOWAIT);
		if (kid->ids == NULL) {
			free(kid, M_DEVBUF);
			device_printf(softc->dev, "kTLS kid ids alloc failed\n");
			return ENOMEM;
		}
		kid->start_id = le32toh(key_buf[i]);
		if (contig)
			kid->count = (num > BNXT_KID_BATCH_SIZE) ? BNXT_KID_BATCH_SIZE : num;
		else
			/*
			 * Non-contiguous IDs can't share one start_id+count
			 * range, so each gets its own bnxt_kid_info node.
			 */
			kid->count = 1;
		for (j = 0; j < kid->count; j++)
			bit_set(kid->ids, j);
		if (id != NULL && i == 0) {
			bit_clear(kid->ids, 0);
			*id = kid->start_id;
		}
		mtx_lock(&kctx->lock);
		TAILQ_INSERT_TAIL(&kctx->list, kid, list);
		kctx->total_alloc += kid->count;
		mtx_unlock(&kctx->lock);
		i += kid->count;
	}
	return 0;
}

int bnxt_hwrm_key_ctx_alloc(struct bnxt_softc* softc, struct bnxt_kctx* kctx,
			    uint16_t num, uint32_t* id, uint8_t type)
{
	struct bnxt_tls_info* tls = softc->ktls_info;
	struct hwrm_func_key_ctx_alloc_output* resp =
		(void*)softc->hwrm_cmd_resp.idi_vaddr;
	struct hwrm_func_key_ctx_alloc_input req = {0};
	int pending_count;
	__le32* key_buf;
	bool contig;
	int rc;
	struct iflib_dma_info dma_data = {};
	bool dma_valid = false;
	uint32_t partition_id = 0;
	bool partition_allocated = false;

	num = min(num, tls->max_key_ctxs_alloc);
	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FUNC_KEY_CTX_ALLOC);

	if (tls->partition_mode) {
		num = tls->ctxs_per_partition;
		rc = bnxt_partition_alloc(kctx, &partition_id);
		if (rc)
			return rc;
		partition_allocated = true;
		req.partition_start_xid = htole32(partition_id * num);
	} else {
		rc = iflib_dma_alloc(softc->ctx, num * 4, &dma_data,
				     BUS_DMA_NOWAIT);
		if (rc) {
			device_printf(softc->dev, "kTLS key ctx DMA alloc failed, rc=%d\n", rc);
			return rc;
		}
		dma_valid = true;
		bus_dmamap_sync(dma_data.idi_tag, dma_data.idi_map,
				BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		key_buf = (__le32*)dma_data.idi_vaddr;
		if (!key_buf) {
			device_printf(softc->dev, "kTLS key ctx DMA vaddr NULL after alloc\n");
			iflib_dma_free(&dma_data);
			dma_valid = false;
			return ENOMEM;
		}
		req.dma_bufr_size_bytes = htole32(num * 4);
		req.host_dma_addr = htole64(dma_data.idi_paddr);
	}

	req.key_ctx_type = kctx->type;
	req.num_key_ctxs = htole16(num);

	pending_count = atomic_fetchadd_32(&kctx->alloc_pending, 1) + 1;
	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	atomic_subtract_32(&kctx->alloc_pending, 1);
	if (rc) {
		device_printf(softc->dev, "kTLS HWRM key ctx alloc failed, rc=%d\n", rc);
		goto key_alloc_exit_wake;
	}

	num = le16toh(resp->num_key_ctxs_allocated);
	contig = resp->flags &
		 HWRM_FUNC_KEY_CTX_ALLOC_OUTPUT_FLAGS_KEY_CTXS_CONTIGUOUS;
	if (tls->partition_mode)
		key_buf = &resp->partition_start_xid;
	rc = bnxt_key_ctx_store(softc, key_buf, num, contig, kctx, id);
	if (rc)
		BNXT_DEBUG(softc->dev, "kTLS key_ctx_store failed, rc=%d\n", rc);

key_alloc_exit_wake:
	/*
	 * Lock ordering: BNXT_HWRM_LOCK (outer) -> kctx->lock (inner); never
	 * acquire kctx->lock before calling a HWRM function.
	 */
	if (rc && partition_allocated) {
		mtx_lock(&kctx->lock);
		bit_clear(kctx->partition_bmap, partition_id);
		mtx_unlock(&kctx->lock);
	}

	if (pending_count >= BNXT_KCTX_ALLOC_PENDING_MAX) {
		mtx_lock(&kctx->lock);
		cv_broadcast(&kctx->alloc_pending_cv);
		mtx_unlock(&kctx->lock);
	}

	BNXT_HWRM_UNLOCK(softc);
	if (dma_valid)
		iflib_dma_free(&dma_data);
	return rc;
}

static int bnxt_alloc_one_kctx(struct bnxt_kctx* kctx, uint32_t* id)
{
	struct bnxt_kid_info* kid;
	int rc = ENOMEM;

	mtx_lock(&kctx->lock);
	TAILQ_FOREACH(kid, &kctx->list, list) {
		ssize_t idx = 0;

		do {
			bit_ffs_at(kid->ids, idx, kid->count, &idx);
			if (idx == -1)
				break;
			if (bnxt_test_and_clear_bit_nonatomic(idx, kid->ids)) {
				*id = kid->start_id + idx;
				rc = 0;
				goto alloc_done;
			}
		} while (1);
	}

alloc_done:
	mtx_unlock(&kctx->lock);
	return (rc);
}

void bnxt_free_one_kctx(struct bnxt_kctx* kctx, uint32_t id, device_t dev)
{
	struct bnxt_kid_info* kid;

	mtx_lock(&kctx->lock);
	TAILQ_FOREACH(kid, &kctx->list, list) {
		if (id >= kid->start_id && id < kid->start_id + kid->count) {
			bit_set(kid->ids, id - kid->start_id);
			break;
		}
	}
	mtx_unlock(&kctx->lock);
}

#define BNXT_KCTX_ALLOC_RETRY_MAX 3

int bnxt_key_ctx_alloc_one(struct bnxt_softc* softc, struct bnxt_kctx* kctx,
			   uint32_t* id, uint8_t type)
{
	int rc, retry = 0;

	while (retry++ < BNXT_KCTX_ALLOC_RETRY_MAX) {
		rc = bnxt_alloc_one_kctx(kctx, id);
		if (!rc)
			return 0;

		if ((kctx->total_alloc + BNXT_KID_BATCH_SIZE) > kctx->max_ctx) {
			BNXT_DEBUG(softc->dev, "kTLS key_ctx_alloc_one: max_ctx reached\n");
			return ENOSPC;
		}

		if (!BNXT_KCTX_ALLOC_OK(kctx)) {
			mtx_lock(&kctx->lock);
			while (!BNXT_KCTX_ALLOC_OK(kctx))
				cv_wait(&kctx->alloc_pending_cv, &kctx->lock);
			mtx_unlock(&kctx->lock);
			continue;
		}
		rc = bnxt_hwrm_key_ctx_alloc(softc, kctx, BNXT_KID_BATCH_SIZE,
					     id, type);
		if (!rc)
			return 0;
	}
	BNXT_DEBUG(softc->dev, "kTLS key_ctx_alloc_one: retries exhausted\n");
	return EAGAIN;
}

static inline void
bnxt_calc_mpc_cmp_time(struct bnxt_softc* softc, unsigned long rtt)
{
	struct bnxt_tls_info* ktls = softc->ktls_info;

	if (rtt <= 1)
		counter_u64_add(ktls->mpc_cmp_time[BNXT_MPC_CMP_TIME_1US], 1);
	else if (rtt <= 5)
		counter_u64_add(ktls->mpc_cmp_time[BNXT_MPC_CMP_TIME_1_5US], 1);
	else if (rtt <= 10)
		counter_u64_add(ktls->mpc_cmp_time[BNXT_MPC_CMP_TIME_5_10US], 1);
	else if (rtt <= 15)
		counter_u64_add(ktls->mpc_cmp_time[BNXT_MPC_CMP_TIME_10_15US], 1);
	else if (rtt <= 20)
		counter_u64_add(ktls->mpc_cmp_time[BNXT_MPC_CMP_TIME_15_20US], 1);
	else if (rtt <= 25)
		counter_u64_add(ktls->mpc_cmp_time[BNXT_MPC_CMP_TIME_20_25US], 1);
	else if (rtt <= 50)
		counter_u64_add(ktls->mpc_cmp_time[BNXT_MPC_CMP_TIME_25_50US], 1);
	else if (rtt <= 100)
		counter_u64_add(ktls->mpc_cmp_time[BNXT_MPC_CMP_TIME_50_100US], 1);
	else if (rtt <= 200)
		counter_u64_add(ktls->mpc_cmp_time[BNXT_MPC_CMP_TIME_100_200US], 1);
	else if (rtt <= 500)
		counter_u64_add(ktls->mpc_cmp_time[BNXT_MPC_CMP_TIME_200_500US], 1);
	else if (rtt <= 1000)
		counter_u64_add(ktls->mpc_cmp_time[BNXT_MPC_CMP_TIME_500_1000US], 1);
	else if (rtt <= 1500)
		counter_u64_add(ktls->mpc_cmp_time[BNXT_MPC_CMP_TIME_1000_1500US], 1);
	else if (rtt <= 2000)
		counter_u64_add(ktls->mpc_cmp_time[BNXT_MPC_CMP_TIME_1500_2000US], 1);
	else if (rtt <= 2500)
		counter_u64_add(ktls->mpc_cmp_time[BNXT_MPC_CMP_TIME_2000_2500US], 1);
	else if (rtt <= 3000)
		counter_u64_add(ktls->mpc_cmp_time[BNXT_MPC_CMP_TIME_2500_3000US], 1);
	else if (rtt <= 3500)
		counter_u64_add(ktls->mpc_cmp_time[BNXT_MPC_CMP_TIME_3000_3500US], 1);
	else if (rtt <= 4000)
		counter_u64_add(ktls->mpc_cmp_time[BNXT_MPC_CMP_TIME_3500_4000US], 1);
	else if (rtt <= 4500)
		counter_u64_add(ktls->mpc_cmp_time[BNXT_MPC_CMP_TIME_4000_4500US], 1);
	else if (rtt <= 5000)
		counter_u64_add(ktls->mpc_cmp_time[BNXT_MPC_CMP_TIME_4500_5000US], 1);
	else
		counter_u64_add(ktls->mpc_cmp_time[BNXT_MPC_CMP_TIME_5000_PLUS_US], 1);
}


/*
 * EBUSY from bnxt_start_xmit_mpc() means the MPC TX ring is momentarily
 * full; retry with a short delay instead of failing the caller outright.
 */
#define BNXT_MPC_XMIT_RETRY_MAX	200
#define BNXT_MPC_XMIT_RETRY_DELAY_USEC	10

int bnxt_xmit_crypto_cmd(struct bnxt_softc* softc,
			 void* cmd, uint32_t len,
			 struct bnxt_ktls_offload_ctx_tx* kctx_tx)
{
	struct bnxt_mpc_info* mpc = softc->mpc_info;
	struct bnxt_tls_info* ktls = softc->ktls_info;
	struct bnxt_crypto_cmd_ctx* ctx = NULL;
	unsigned long handle = 0;
	struct bnxt_ring* txr;
	int rc, retry;

	txr = bnxt_select_mpc_ring(mpc);

	uint32_t kid = CE_CMD_KID(cmd);

	ctx = uma_zalloc(ktls->mpc_zone, M_NOWAIT);
	if (ctx == NULL) {
		device_printf(softc->dev, "kTLS MPC ctx alloc failed\n");
		return ENOMEM;
	}

	handle = (unsigned long)ctx;
	ctx->ce_cmp.opaque = BNXT_KMPC_OPAQUE(
		txr->mpc_chnl_type, kid);

	ctx->kctx_tx = kctx_tx;

	/*
	 * Mark a completion in flight before posting, so bnxt_tls_tag_zfree()
	 * is deferred until it completes; reverted below if never posted.
	 */
	atomic_add_32(&kctx_tx->mpc_inflight, 1);

	getmicrotime(&kctx_tx->start);

	for (retry = 0; ; retry++) {
		mtx_lock_spin(&txr->tx_lock);
		rc = bnxt_start_xmit_mpc(softc, txr, cmd, len, handle);
		mtx_unlock_spin(&txr->tx_lock);
		if (rc != EBUSY || retry >= BNXT_MPC_XMIT_RETRY_MAX)
			break;
		counter_u64_add(ktls->err_counters[BNXT_KTLS_TX_MPC_RING_BUSY], 1);
		/*
		 * Bounded to 2ms worst case with tx_lock released, so a
		 * short busy-wait here is fine.
		 */
		DELAY(BNXT_MPC_XMIT_RETRY_DELAY_USEC);
	}

	if (rc) {
		BNXT_DEBUG(softc->dev, "kTLS MPC xmit failed, kid=%u rc=%d retries=%d\n",
		    kid, rc, retry);
		atomic_subtract_32(&kctx_tx->mpc_inflight, 1);
		uma_zfree(ktls->mpc_zone, ctx);
	}

	return rc;
}

static int bnxt_crypto_add(struct bnxt_softc* softc,
			   struct bnxt_ktls_offload_ctx_tx* kctx_tx,
			   uint8_t type)
{
	struct tls_session_params* crypto_info = &kctx_tx->crypto_info;
	uint32_t tcp_seq_no = kctx_tx->tcp_seq_no;
	uint64_t rec_seq = kctx_tx->rec_seq;
	uint32_t kid = kctx_tx->kid;
	struct ce_add_cmd cmd = {0};
	uint32_t data;
	int rc;

	cmd.ctx_kind = CE_ADD_CMD_CTX_KIND_CK_TX;

	data = CE_ADD_CMD_OPCODE_ADD | (kid << CE_ADD_CMD_KID_SFT);
	switch (crypto_info->cipher_key_len) {
	case 128 / 8:
		data |= CE_ADD_CMD_ALGORITHM_AES_GCM_128;
#ifdef TLS_MINOR_VER_THREE
		if (crypto_info->tls_vminor == TLS_MINOR_VER_THREE) {
			data |= CE_ADD_CMD_VERSION_TLS1_3;
			memcpy(&cmd.addl_iv, crypto_info->iv + BNXT_SALT_LEN,
			    BNXT_ADDL_IV_LEN_TLS1_3);
		}
#endif
		memcpy(&cmd.session_key, crypto_info->cipher_key,
		       crypto_info->cipher_key_len);
		memcpy(&cmd.salt, crypto_info->iv, BNXT_SALT_LEN);
		memcpy(&cmd.record_seq_num[0], &rec_seq, sizeof(rec_seq));
		break;
	case 256 / 8:
		data |= CE_ADD_CMD_ALGORITHM_AES_GCM_256;
#ifdef TLS_MINOR_VER_THREE
		if (crypto_info->tls_vminor == TLS_MINOR_VER_THREE) {
			data |= CE_ADD_CMD_VERSION_TLS1_3;
			memcpy(&cmd.addl_iv, crypto_info->iv + BNXT_SALT_LEN,
			    BNXT_ADDL_IV_LEN_TLS1_3);
		}
#endif
		memcpy(&cmd.session_key, crypto_info->cipher_key,
		       crypto_info->cipher_key_len);
		memcpy(&cmd.salt, crypto_info->iv, BNXT_SALT_LEN);
		memcpy(&cmd.record_seq_num[0], &rec_seq, sizeof(rec_seq));
		break;
	}
	cmd.ver_algo_kid_opcode = htole32(data);
	cmd.pkt_tcp_seq_num = htole32(tcp_seq_no);
	cmd.tls_header_tcp_seq_num = cmd.pkt_tcp_seq_num;
	rc = bnxt_xmit_crypto_cmd(softc, &cmd, sizeof(cmd), kctx_tx);
	explicit_bzero(&cmd, sizeof(cmd));
	return rc;
}

static int bnxt_crypto_del(struct bnxt_softc* softc,
		struct bnxt_ktls_offload_ctx_tx* kctx_tx, uint8_t type)
{
	struct ce_delete_cmd cmd = {0};
	uint32_t data;
	uint32_t kid;

	if (bnxt_drv_state_test(softc, BNXT_STATE_IN_FW_RESET) &&
	    bnxt_drv_state_test(softc, BNXT_STATE_FW_FATAL_COND))
		return 0;

	kid = kctx_tx->kid;
	data = CE_DELETE_CMD_CTX_KIND_CK_TX;

	data |= CE_DELETE_CMD_OPCODE_DEL | (kid << CE_DELETE_CMD_KID_SFT);

	cmd.ctx_kind_kid_opcode = htole32(data);
	return bnxt_xmit_crypto_cmd(softc, &cmd, sizeof(cmd),
				    kctx_tx);
}

static int bnxt_ktls_dev_add(struct bnxt_softc* softc,
			     const struct tls_session_params* crypto_info,
			     struct bnxt_ktls_offload_ctx_tx* kctx_tx)
{
	struct bnxt_tls_info* ktls;
	struct bnxt_kctx* kctx;
	uint32_t kid;
	int rc = 0;

	ktls = softc->ktls_info;
	atomic_add_32(&ktls->pending, 1);

	atomic_thread_fence_seq_cst();
	if (!bnxt_drv_state_test(softc, BNXT_STATE_OPEN)) {
		BNXT_DEBUG(softc->dev, "kTLS dev_add: device not open\n");
		rc = ENODEV;
		goto exit;
	}

	memcpy(&kctx_tx->crypto_info, crypto_info, sizeof(*crypto_info));

	kctx = &BNXT_TCK(ktls);
	rc = bnxt_key_ctx_alloc_one(softc, kctx, &kid, BNXT_CRYPTO_TYPE_KTLS);
	if (rc) {
		BNXT_DEBUG(softc->dev, "kTLS dev_add: key_ctx_alloc_one failed, rc=%d\n", rc);
		goto exit;
	}

	counter_u64_add(ktls->counters[BNXT_KTLS_TX_ADD], 1);

	BNXT_DEBUG(softc->dev, "[KID:%d] KTLS device add is successful\n", kid);

	kctx_tx->kid = kid;
	kctx_tx->gen = atomic_load_32(&ktls->reset_gen);
	kctx_tx->enobufs = 0;

	rc = bnxt_crypto_add(softc, kctx_tx, BNXT_CRYPTO_TYPE_KTLS);

	if (rc) {
		device_printf(softc->dev, "kTLS crypto add failed for KID %u, rc=%d\n", kid, rc);
		bnxt_free_one_kctx(kctx, kid, softc->dev);
		counter_u64_add(ktls->counters[BNXT_KTLS_TX_DEL], 1);
		goto exit;
	}

exit:
	atomic_subtract_32(&ktls->pending, 1);
	return rc;
}

#define BNXT_RETRY_MAX 20

static void bnxt_ktls_dev_del(struct bnxt_softc* softc,
			      struct bnxt_ktls_offload_ctx_tx* kctx_tx)
{
	struct bnxt_tls_info* ktls;
	struct bnxt_kctx* kctx;
	int retry_cnt = 0;
	uint32_t kid;
	int rc;

	ktls = softc->ktls_info;
	kid = kctx_tx->kid;
	kctx = &BNXT_TCK(ktls);

	/*
	 * BNXT_STATE_OPEN is cleared (and MPC rings freed) in bnxt_stop();
	 * bail out here to avoid MPC command timeouts while unloading.
	 */
	if (!bnxt_drv_state_test(softc, BNXT_STATE_OPEN)) {
		BNXT_DEBUG(softc->dev, "[KID:%u]: KTLS dev_del: device not up, freeing KID\n", kid);
		bnxt_free_one_kctx(kctx, kid, softc->dev);
		counter_u64_add(ktls->counters[BNXT_KTLS_TX_DEL], 1);
		return;
	}

retry:
	atomic_add_32(&ktls->pending, 1);
	/* Make sure bnxt_close_nic() sees pending before we check the
	 * BNXT_STATE_OPEN flag.
	 */
	atomic_thread_fence_seq_cst();
	while (!bnxt_drv_state_test(softc, BNXT_STATE_OPEN)) {
		atomic_subtract_32(&ktls->pending, 1);
		if (retry_cnt > BNXT_RETRY_MAX) {
			device_printf(softc->dev,
				      "%s retry max %d exceeded, state %lx\n",
				      __func__, retry_cnt, softc->state);
			bnxt_free_one_kctx(kctx, kid, softc->dev);
			counter_u64_add(ktls->counters[BNXT_KTLS_TX_DEL], 1);
			return;
		}
		retry_cnt++;
		/*
		 * Sleep instead of busy-waiting: runs in ordinary thread
		 * context with no locks held here.
		 */
		pause("bnxtktlsdel", hz / 10);
		goto retry;
	}

	counter_u64_add(ktls->counters[BNXT_KTLS_TX_DEL], 1);
	rc = bnxt_crypto_del(softc, kctx_tx, BNXT_CRYPTO_TYPE_KTLS);
	if (rc) {
		/*
		 * Never posted to firmware, so the old context there is still
		 * live: recycling the KID now would let a new ADD collide with
		 * it. Leak the KID rather than risk corrupting whichever
		 * session firmware next hands it to.
		 */
		device_printf(softc->dev,
		    "kTLS crypto delete failed for KID %u, rc=%d; leaking KID\n", kid, rc);
	}
	/*
	 * Otherwise the delete is only enqueued, not yet applied by firmware;
	 * bnxt_ktls_mpc_cmp() frees the KID once the completion confirms it.
	 */

	atomic_subtract_32(&ktls->pending, 1);
	BNXT_DEBUG(softc->dev, "[KID:%d]: KTLS device delete is successful\n", kid);
}

int bnxt_set_partition_mode(struct bnxt_softc* softc)
{
	struct hwrm_func_cfg_input req = {0};
	int rc;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FUNC_CFG);
	req.fid = htole16(0xffff);
	req.enables2 =
		htole32(HWRM_FUNC_CFG_INPUT_ENABLES2_XID_PARTITION_CFG);
	req.xid_partition_cfg =
		htole16(HWRM_FUNC_CFG_INPUT_XID_PARTITION_CFG_TX_CK |
			    HWRM_FUNC_CFG_INPUT_XID_PARTITION_CFG_RX_CK);
	rc = hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		device_printf(softc->dev, "kTLS set partition mode failed, rc=%d\n", rc);
	return rc;
}

static int bnxt_pre_alloc_tx_crypto_keys(struct bnxt_softc* softc)
{
	struct bnxt_tls_info* ktls = softc->ktls_info;
	uint32_t alloc_keys = softc->max_ktls_entries;
	uint32_t allocated_keys_count = 0;
	uint16_t batch_sz;
	int rc = 0;
	unsigned long start_ticks, elapsed_ticks;

	start_ticks = ticks;
	while (alloc_keys > 0) {
		uint32_t prev_total = BNXT_TCK(ktls).total_alloc;
		uint32_t got;

		batch_sz = (uint16_t)min(alloc_keys, (uint32_t)BNXT_KID_BATCH_SIZE);

		rc = bnxt_hwrm_key_ctx_alloc(softc, &BNXT_TCK(ktls), batch_sz,
		    NULL, BNXT_CRYPTO_TYPE_KTLS);


		/*
		 * bnxt_hwrm_key_ctx_alloc() may allocate fewer contexts than
		 * requested; use total_alloc's true count, not batch_sz.
		 */
		if (rc) {
			if (!BNXT_TCK(ktls).total_alloc) {
				device_printf(softc->dev,
				    "kTLS Tx keys allocation is failed, disable the HW kTLS tx\n");
				return rc;
			} else {
				rc = 0;
				device_printf(softc->dev,
				    "HWRM key ctx allocation is failed\n");
				break;
			}
		}

		/*
		 * Decrement by what was actually recorded, not batch_sz, so a
		 * partial grant doesn't under-provision the key pool.
		 */
		got = BNXT_TCK(ktls).total_alloc - prev_total;
		if (got == 0) {
			device_printf(softc->dev,
			    "kTLS Tx key alloc made no progress (%u/%u allocated), stopping short\n",
			    BNXT_TCK(ktls).total_alloc, softc->max_ktls_entries);
			break;
		}
		alloc_keys -= got;
	}

	allocated_keys_count = BNXT_TCK(ktls).total_alloc;
	elapsed_ticks = ticks - start_ticks;

	device_printf(softc->dev,
	    "allocated kTLS Tx crypto keys count: %d time to alloc: (%d msecs)\n",
	    allocated_keys_count, (int)((elapsed_ticks * 1000) / hz));

	return rc;
}

int bnxt_ktls_init(struct bnxt_softc* softc)
{
	struct bnxt_tls_info* ktls = softc->ktls_info;
	struct bnxt_hw_resc* hw_resc = &softc->hw_resc;
	struct bnxt_hw_tls_resc* tls_resc;
	int rc = 0;

	if (!ktls)
		return 0;

	/*
	 * kTLS and MPC are gated on independent FW capabilities; MPC ring
	 * alloc can yield zero TCE rings even when crypto is enabled.
	 */
	if (!BNXT_MPC_CRYPTO_CAPABLE(softc) ||
	    softc->mpc_info->mpc_ring_count[BNXT_MPC_TCE_TYPE] == 0) {
		device_printf(softc->dev,
		    "kTLS requires an MPC TCE ring, none available; disabling HW kTLS\n");
		return EINVAL;
	}

	tls_resc = &hw_resc->tls_resc[BNXT_CRYPTO_TYPE_KTLS];
	BNXT_TCK(ktls).max_ctx = tls_resc->resv_tx_key_ctxs;

	if (!BNXT_TCK(ktls).max_ctx) {
		device_printf(softc->dev,
		    "max Tx ctx count is zero, disable HW kTLS\n");
		return EINVAL;
	}

	if (ktls->partition_mode) {
		rc = bnxt_set_partition_mode(softc);
		if (rc) {
			BNXT_DEBUG(softc->dev, "kTLS set partition mode failed, disabling partition mode\n");
			ktls->partition_mode = false;
		}
	}

	rc = bnxt_pre_alloc_tx_crypto_keys(softc);

	if (rc)
		return rc;

	/*
	 * On failure below, drop tracking via bnxt_clear_ktls() so a retry
	 * doesn't see stale entries (no HWRM_FUNC_KEY_CTX_FREE call exists).
	 */
	ktls->mpc_zone = uma_zcreate("bnxt_ktls_mpc",
	    sizeof(struct bnxt_crypto_cmd_ctx), NULL, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, 0);
	if (ktls->mpc_zone == NULL) {
		device_printf(softc->dev, "kTLS mpc_zone create failed\n");
		bnxt_clear_ktls(softc);
		return ENOMEM;
	}

	ktls->tq = taskqueue_create("bnxt_tls", M_NOWAIT,
	    taskqueue_thread_enqueue, &ktls->tq);
	if (ktls->tq == NULL) {
		device_printf(softc->dev, "kTLS taskqueue create failed\n");
		uma_zdestroy(ktls->mpc_zone);
		ktls->mpc_zone = NULL;
		bnxt_clear_ktls(softc);
		return (ENOMEM);
	}
	rc = taskqueue_start_threads(&ktls->tq, 1, PWAIT, "bnxt_tls");
	if (rc != 0) {
		device_printf(softc->dev, "kTLS taskqueue thread start failed, rc=%d\n", rc);
		taskqueue_free(ktls->tq);
		ktls->tq = NULL;
		uma_zdestroy(ktls->mpc_zone);
		ktls->mpc_zone = NULL;
		bnxt_clear_ktls(softc);
		return (rc);
	}

	snprintf(ktls->zname, sizeof(ktls->zname),
	    "bnxt_%u_tls", device_get_unit(softc->dev));

	ktls->zone = uma_zcache_create(ktls->zname,
	     sizeof(struct bnxt_ktls_offload_ctx_tx), NULL, NULL, NULL, NULL,
	     bnxt_tls_tag_import, bnxt_tls_tag_release, softc,
	     UMA_ZONE_UNMANAGED);
	if (ktls->zone == NULL) {
		device_printf(softc->dev, "kTLS UMA zone create failed\n");
		taskqueue_free(ktls->tq);
		ktls->tq = NULL;
		uma_zdestroy(ktls->mpc_zone);
		ktls->mpc_zone = NULL;
		bnxt_clear_ktls(softc);
		return (ENOMEM);
	}

	ktls->init = 1;

	return 0;
}

void bnxt_ktls_mpc_cmp(struct bnxt_softc* softc, uint32_t client,
		       unsigned long handle, struct bnxt_cmpl_entry cmpl[],
		       uint32_t entries)
{
	struct bnxt_ktls_offload_ctx_tx* kctx_tx = NULL;
	struct bnxt_tls_info* tls = softc->ktls_info;
	struct bnxt_crypto_cmd_ctx* ctx;
	unsigned long rtt_us;
	struct ce_cmpl* cmp;
	uint32_t len, kid;

	cmp = cmpl[0].cmpl;
	if (!handle || entries != 1) {
		if (!handle)
			device_printf(softc->dev,
			    "kTLS MPC cmp: invalid handle 0\n");
		else if (entries != 1) {
			device_printf(softc->dev,
				      "Invalid entries %d with handle %lx cmpl "
				      "%08x in %s()\n",
				      entries, handle, *(uint32_t*)cmp,
				      __func__);
		}
		return;
	}
	ctx = (void*)handle;
	kid = CE_CMPL_KID(cmp);
	if (ctx->ce_cmp.opaque != BNXT_KMPC_OPAQUE(client, kid)) {
		device_printf(softc->dev,
			      "Invalid CE cmpl software opaque %08x, cmpl "
			      "%08x, kid %x\n",
			      ctx->ce_cmp.opaque, *(uint32_t*)cmp, kid);

		kctx_tx = ctx->kctx_tx;
		uma_zfree(tls->mpc_zone, ctx);

		switch (kctx_tx->state) {
		case BNXT_TLS_STATE_INIT:
			if (kctx_tx->mpc_retry < MAX_MPC_RETRY) {
				kctx_tx->mpc_retry++;
				BNXT_DEBUG(softc->dev, "[KID: %u]: retry(%u) crypto add\n",
				    kctx_tx->kid, kctx_tx->mpc_retry);
				if (bnxt_crypto_add(softc, kctx_tx, BNXT_CRYPTO_TYPE_KTLS))
					kctx_tx->state = BNXT_TLS_STATE_BAD;
			} else
				kctx_tx->state = BNXT_TLS_STATE_BAD;
			break;
		case BNXT_TLS_STATE_FREE:
			if (kctx_tx->mpc_retry < MAX_MPC_RETRY) {
				kctx_tx->mpc_retry++;
				BNXT_DEBUG(softc->dev, "[KID: %u]: retry(%u) crypto delete\n",
				    kctx_tx->kid, kctx_tx->mpc_retry);
				if (bnxt_crypto_del(softc, kctx_tx, BNXT_CRYPTO_TYPE_KTLS))
					kctx_tx->state = BNXT_TLS_STATE_BAD;
			} else
				kctx_tx->state = BNXT_TLS_STATE_BAD;
			break;
		}

		/*
		 * This completion's reference is done; if it was the last one
		 * and the tag was already marked free, finish the free here.
		 */
		if (atomic_fetchadd_32(&kctx_tx->mpc_inflight, -1) == 1 &&
		    kctx_tx->state == BNXT_TLS_STATE_FREE)
			bnxt_tls_tag_zfree(kctx_tx);

		return;
	}

	len = min(cmpl[0].len, (uint32_t)sizeof(ctx->ce_cmp));
	memcpy(&ctx->ce_cmp, cmpl[0].cmpl, len);
	kctx_tx = ctx->kctx_tx;

	if (CE_CMPL_STATUS(&ctx->ce_cmp) == CE_CMPL_STATUS_OK) {
		if (kctx_tx->state == BNXT_TLS_STATE_INIT)
			kctx_tx->state = BNXT_TLS_STATE_READY;
		else if (kctx_tx->state == BNXT_TLS_STATE_FREE)
			/* Delete confirmed applied; only now is the KID safe
			 * to hand to a new session. */
			bnxt_free_one_kctx(&BNXT_TCK(tls), kctx_tx->kid, softc->dev);
	} else {
		counter_u64_add(softc->ktls_info->err_counters[BNXT_KTLS_TX_MPC_FAILED], 1);
		BNXT_DEBUG(softc->dev, "[KID: %u]: MPC failed: current state: %u cmpl_status: %lu\n",
		    kctx_tx->kid, kctx_tx->state, CE_CMPL_STATUS(&ctx->ce_cmp));
		/*
		 * Leave a FREE tag FREE on a failed DELETE completion (its KID
		 * leaks, but the tag itself still gets released below); only
		 * a live session (INIT/READY) becomes unusable here.
		 */
		if (kctx_tx->state != BNXT_TLS_STATE_FREE)
			kctx_tx->state = BNXT_TLS_STATE_BAD;
	}

	getmicrotime(&kctx_tx->end);
	timevalsub(&kctx_tx->end, &kctx_tx->start);
	rtt_us = kctx_tx->end.tv_sec * 1000000 + kctx_tx->end.tv_usec;
	bnxt_calc_mpc_cmp_time(softc, rtt_us);

	kctx_tx->start.tv_sec = 0;
	kctx_tx->start.tv_usec = 0;
	kctx_tx->end.tv_sec = 0;
	kctx_tx->end.tv_usec = 0;

	/*
	 * Reset MPC retry counter for a KID once one successful
	 * MPC completion is received for a KID.
	 */
	kctx_tx->mpc_retry = 0;
	uma_zfree(tls->mpc_zone, ctx);

	/* See the matching comment in the opaque-mismatch path above. */
	if (atomic_fetchadd_32(&kctx_tx->mpc_inflight, -1) == 1 &&
	    kctx_tx->state == BNXT_TLS_STATE_FREE)
		bnxt_tls_tag_zfree(kctx_tx);
}

static int bnxt_get_tls_seq(struct mbuf* mb, uint64_t* seq)
{

	for (; mb != NULL; mb = mb->m_next) {
		if (!(mb->m_flags & M_EXTPG))
			continue;
		*seq = mb->m_epg_seqno;
		return (1);
	}
	return (0);
}

int
bnxt_alloc_ktls_rexmit_mbuf(struct bnxt_softc *softc, struct bnxt_ring *txr)
{
	struct bnxt_tls_info *ktls = softc->ktls_info;
	int ring_sz = softc->scctx->isc_ntxd[1];
	struct bnxt_replay_pkt *pkt;
	int i, err;

	txr->replay_pkt = malloc(sizeof(struct bnxt_replay_pkt) * ring_sz,
	    M_DEVBUF, M_NOWAIT | M_ZERO);

	if (!txr->replay_pkt) {
		BNXT_DEBUG(softc->dev, "kTLS replay_pkt malloc failed\n");
		return ENOMEM;
	}

	for (i = 0; i < ring_sz; i++) {
		pkt = &txr->replay_pkt[i];

		err = bus_dmamap_create(ktls->dma_tag, 0, &pkt->dma_map);
		if (err != 0) {
			device_printf(softc->dev,
			    "kTLS replay_pkt dmamap create failed for entry %d\n", i);
			goto err;
		}
	}

	return (0);
err:
	for (i = i - 1; i >= 0; i--) {
		pkt = &txr->replay_pkt[i];
		bus_dmamap_destroy(ktls->dma_tag, pkt->dma_map);
	}

	free(txr->replay_pkt, M_DEVBUF);
	txr->replay_pkt = NULL;

	return ENOMEM;
}

inline void
bnxt_free_ktls_rexmit_mbuf(struct bnxt_ring *txr)
{
	struct bnxt_softc *softc = txr->softc;
	struct bnxt_tls_info *ktls = softc->ktls_info;
	int ring_sz = softc->scctx->isc_ntxd[1];
	struct bnxt_replay_pkt *pkt;
	int i;

	/*
	 * replay_pkt can be NULL if bnxt_alloc_ktls_rexmit_mbuf() failed;
	 * callers only guard on `if (ktls)`, not per-ring alloc success.
	 */
	if (txr->replay_pkt == NULL)
		return;

	for (i = 0; i < ring_sz; i++) {
		pkt = &txr->replay_pkt[i];
		if (pkt->mbuf) {
			bus_dmamap_sync(ktls->dma_tag, pkt->dma_map,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(ktls->dma_tag, pkt->dma_map);
			m_freem(pkt->mbuf);
			counter_u64_add(ktls->counters[BNXT_KTLS_TX_MBUF_FREES], 1);
			pkt->mbuf = NULL;
		}
		bus_dmamap_destroy(ktls->dma_tag, pkt->dma_map);
	}

	free(txr->replay_pkt, M_DEVBUF);
	txr->replay_pkt = NULL;
}

#define BNXT_REPLAY_PACKET_MSS 9000

static int bnxt_ktls_submit_mbuf(struct bnxt_softc *softc, struct bnxt_ring *txr,
    struct mbuf **mbp, struct bnxt_ktls_offload_ctx_tx *kctx_tx, int length, uint16_t hsz,
    if_pkt_info_t pi)
{
	bus_dma_segment_t segs[BNXT_MAX_NUM_SEGS];
	struct bnxt_tls_info *ktls = softc->ktls_info;
	struct bnxt_sw_tx_bd *tx_buf;
	struct tx_bd_long_hi *tbdh;
	struct tx_bd_opaque *opq;
	struct tx_bd_long *tbd;
	struct bnxt_replay_pkt *pkt;
	uint32_t kid = kctx_tx->kid;
	uint16_t flags_type;
	uint16_t lflags = 0;
	uint16_t pkt_size, i;
	uint32_t cfa_meta;
	struct mbuf *mb;
	uint32_t prod;
	int seg = 0;
	int nsegs;
	int xsegs;
	int err;

	pkt_size = length + hsz;
	prod = txr->prod;
	mb = *mbp;

	if (!txr->replay_pkt) {
		BNXT_DEBUG(softc->dev, "kTLS submit_mbuf: no replay_pkt\n");
		m_freem(mb);
		return ENOMEM;
	}

	pkt = &txr->replay_pkt[prod];
	tx_buf = &txr->tx_buf_ring[prod];

	if (__predict_false(tx_buf->is_replay == 1)) {
		BNXT_DEBUG(softc->dev, "kTLS submit_mbuf: replay slot busy KID %u\n", kid);
		m_freem(mb);
		return EINVAL;
	}

	pkt->mbuf = mb;
	/* check number of segments in mbuf */
	err = bus_dmamap_load_mbuf_sg(ktls->dma_tag, pkt->dma_map,
	    mb, segs, &nsegs, BUS_DMA_NOWAIT);

	if (err == EFBIG) {
		/* too many mbuf fragments */
		mb = m_defrag(*mbp, M_NOWAIT);
		if (mb == NULL) {
			mb = *mbp;
			pkt->mbuf = NULL;
			m_freem(mb);
			return err;
		}

		pkt->mbuf = mb;
		*mbp = mb;
		/* try again */
		err = bus_dmamap_load_mbuf_sg(ktls->dma_tag, pkt->dma_map,
		    mb, segs, &nsegs, BUS_DMA_NOWAIT);
	}

	if (err != 0) {
		BNXT_DEBUG(softc->dev, "kTLS submit_mbuf: dmamap load failed, err=%d KID %u\n",
		    err, kid);
		m_freem(mb);
		pkt->mbuf = NULL;
		return err;
	}

	bus_dmamap_sync(ktls->dma_tag, pkt->dma_map,
	    BUS_DMASYNC_PREWRITE);

	/* Calculate segments to be re-played */
	int rep_len = pkt_size;
	for (i = xsegs = 0; i != nsegs; i++) {
		if (rep_len <= 0)
			break;

		/*
		 * Tail segment: clamp its length to rep_len rather than
		 * dropping it, or trailing record bytes go missing.
		 */
		if (segs[i].ds_len > rep_len)
			segs[i].ds_len = rep_len;
		rep_len = rep_len - (uint32_t)segs[i].ds_len;

		xsegs++;
	}

	/* check if there are no segments */
	if (__predict_false(xsegs == 0)) {
		BNXT_DEBUG(softc->dev, "kTLS submit_mbuf: no segments after len adjustment KID %u\n",
		    kid);
		bus_dmamap_unload(ktls->dma_tag, pkt->dma_map);
		m_freem(mb);
		counter_u64_add(ktls->counters[BNXT_KTLS_TX_MBUF_FREES], 1);
		counter_u64_add(ktls->counters[BNXT_KTLS_TX_MBUF_ERRORS], 1);
		pkt->mbuf = NULL;
		*mbp = NULL;	/* safety clear */
		/*
		 * Must return non-zero here: bnxt_ktls_tx_ooo() treats 0 as
		 * "replay posted" and won't roll back the presync BD otherwise.
		 */
		return (ENOSPC);
	}

	tx_buf->is_replay = 1;
	lflags = htole16(TX_BD_FLAGS_CRYPTO_EN);

	/* First(header) BD of the packet*/
	tbd = &((struct tx_bd_long *)txr->vaddr)[prod];

	opq = (struct tx_bd_opaque *)&tbd->opaque;
	/* No need to byte-swap the opaque value */
	SET_TX_OPAQUE_KTLS(opq, txr, 1, prod, txr->running_bds + xsegs + 1);
	tbd->len = htole16(segs[seg].ds_len);
	tbd->addr = htole64(segs[seg++].ds_addr);
	flags_type = ((xsegs + 1) << TX_BD_SHORT_FLAGS_BD_CNT_SFT) & TX_BD_SHORT_FLAGS_BD_CNT_MASK;
	if (length  >= 2048)
		flags_type |= TX_BD_SHORT_FLAGS_LHINT_GTE2K;
	else
		flags_type |= bnxt_tx_lhint[length >> 9];

	/*Meta BD of the packet*/
	flags_type |= TX_BD_LONG_TYPE_TX_BD_LONG;

	if (softc->tx_host_coal_enable) {
		/*
		 * Must generate Tx completion for replay packet, so do not set
		 * TX_BD_LONG_FLAGS_NO_CMPL flag for replay packets.
		 */
		flags_type |= TX_BD_LONG_FLAGS_COAL_NOW;
		txr->running_bds = 0;
	}

	prod = RING_NEXT(txr, prod);
	if (prod == 0)
		txr->epoch_bit = !txr->epoch_bit;

	if (pi->ipi_tso_segsz != 0) {
		/* Preserve the connection's actual negotiated MSS. */
		mb->m_pkthdr.tso_segsz = pi->ipi_tso_segsz;
		mb->m_pkthdr.csum_flags |= CSUM_TSO;
	} else if (length >= BNXT_REPLAY_PACKET_MSS) {
		/* Original send wasn't TSO'd, so there's no MSS to preserve;
		 * this is a large one-shot replay, not a segmented stream. */
		mb->m_pkthdr.tso_segsz = BNXT_REPLAY_PACKET_MSS;
		mb->m_pkthdr.csum_flags |= CSUM_TSO;
	} else if (xsegs > 1) {
		/* DMA-fragmented but logically one small packet; size so
		 * firmware doesn't re-split it into bogus segments. */
		mb->m_pkthdr.tso_segsz = pkt_size;
		mb->m_pkthdr.csum_flags |= CSUM_TSO;
	}

	tbdh = &((struct tx_bd_long_hi *)txr->vaddr)[prod];
	tbdh->kid_or_ts_high_mss = htole32((mb->m_pkthdr.tso_segsz &  TX_BD_LONG_MSS_MASK) |
				   ((KID_HIGH(kid) << TX_BD_LONG_KID_OR_TS_HIGH_SFT) &
				     TX_BD_LONG_KID_OR_TS_HIGH_MASK));

	tbdh->kid_or_ts_low_hdr_size = htole16(((hsz >> 1) & TX_BD_LONG_HDR_SIZE_MASK) |
				((KID_LOW(kid) << TX_BD_LONG_KID_OR_TS_LOW_SFT) &
				 TX_BD_LONG_KID_OR_TS_LOW_MASK));
	tbdh->cfa_action = 0;

	cfa_meta = 0;

	if (mb->m_flags & M_VLANTAG) {
		/* ether_vtag is host order; cfa_meta as a whole is htole32'd below. */
		cfa_meta = TX_BD_LONG_CFA_META_KEY_VLAN_TAG |
			mb->m_pkthdr.ether_vtag;
		cfa_meta |= TX_BD_LONG_CFA_META_VLAN_TPID_TPID8100;
	}
	tbdh->cfa_meta = htole32(cfa_meta);

	if (mb->m_pkthdr.csum_flags & CSUM_TSO) {
		lflags |= TX_BD_LONG_LFLAGS_LSO |
		    TX_BD_LONG_LFLAGS_T_IPID;
	}
	else if(mb->m_pkthdr.csum_flags & CSUM_OFFLOAD) {
		lflags |= TX_BD_LONG_LFLAGS_TCP_UDP_CHKSUM |
		    TX_BD_LONG_LFLAGS_IP_CHKSUM;
	}
	else if(mb->m_pkthdr.csum_flags & CSUM_IP) {
		lflags |= TX_BD_LONG_LFLAGS_IP_CHKSUM;
	}

	tbdh->lflags = htole16(lflags);

	/* Payload BD*/
	for (;  seg < xsegs; seg++) {
		tbd->flags_type = htole16(flags_type);

		prod = RING_NEXT(txr, prod);
		if (prod == 0)
			txr->epoch_bit = !txr->epoch_bit;

		tbd = &((struct tx_bd_long *)txr->vaddr)[prod];
		tbd->len = htole16(segs[seg].ds_len);
		tbd->addr = htole64(segs[seg].ds_addr);
		flags_type = TX_BD_SHORT_TYPE_TX_BD_SHORT;
	}

	flags_type |= TX_BD_SHORT_FLAGS_PACKET_END;
	tbd->flags_type = htole16(flags_type);

	prod = RING_NEXT(txr, prod);
	if (prod == 0)
		txr->epoch_bit = !txr->epoch_bit;

	txr->prod = prod;

	softc->db_ops.bnxt_db_tx(txr, prod);

	return 0;
}

static void bnxt_ktls_pre_xmit(struct bnxt_softc *bp, struct bnxt_ring *txr,
			       uint32_t kid, struct crypto_prefix_cmd *pre_cmd)
{
	struct bnxt_sw_tx_bd *tx_buf;
	uint32_t bd_space, space;
	uint8_t *pcmd;
	struct tx_bd *pcmd1;
	uint16_t prod;
	struct tx_bd_presync *psbd;
	struct tx_bd_opaque *opq;
	bool need_cpl = false;

	prod = txr->prod;

	tx_buf = &txr->tx_buf_ring[prod];

	if (bp->tx_host_coal_enable) {
		/* BD field of opaque is only 15 bits; respect tx_host_coal_bds threshold */
		if ((txr->running_bds + CRYPTO_PREFIX_CMD_BDS + 1) >
		    (bp->tx_host_coal_bds - (BNXT_MAX_NUM_SEGS + 1)))
			need_cpl = true;
	}

	/* presync TX BD prep */
	psbd = &((struct tx_bd_presync *)txr->vaddr)[prod];
	psbd->tx_bd_len_flags_type = CRYPTO_PRESYNC_BD_CMD;
	psbd->tx_bd_kid = htole32(kid);

	opq = (struct tx_bd_opaque *)&psbd->tx_bd_opaque;
	SET_TX_OPAQUE_KTLS(opq, txr, 0, prod,
	    txr->running_bds + CRYPTO_PREFIX_CMD_BDS + 1);

	if (bp->tx_host_coal_enable) {
		psbd->tx_bd_len_flags_type |= TX_BD_LONG_FLAGS_COAL_NOW;
		if (need_cpl) {
			txr->running_bds = 0;
		} else {
			txr->running_bds += (CRYPTO_PREFIX_CMD_BDS + 1);
			psbd->tx_bd_len_flags_type |= TX_BD_LONG_FLAGS_NO_CMPL;
		}
	}

	prod = RING_NEXT(txr, prod);
	if (prod == 0)
		txr->epoch_bit = !txr->epoch_bit;

	/* Copy pre-cmd to TX desc ring*/
	pcmd1 = &((struct tx_bd *)txr->vaddr)[prod];
	pcmd = (uint8_t *)pcmd1;
	bd_space = txr->ring_size - prod;
	space = bd_space * sizeof(struct tx_bd);
	if (space >= CRYPTO_PREFIX_CMD_SIZE) {
		memcpy(pcmd, pre_cmd, CRYPTO_PREFIX_CMD_SIZE);
		prod += CRYPTO_PREFIX_CMD_BDS - 1;
	} else {
		memcpy(pcmd, pre_cmd, space);
		prod = 0;
		txr->epoch_bit = !txr->epoch_bit;
		pcmd1 = &((struct tx_bd *)txr->vaddr)[prod];
		pcmd = (uint8_t *)pcmd1;
		memcpy((uint8_t *)pcmd, (uint8_t *)pre_cmd + space,
		       CRYPTO_PREFIX_CMD_SIZE - space);
		prod += CRYPTO_PREFIX_CMD_BDS - bd_space - 1;
	}

	prod = RING_NEXT(txr, prod);
	if (prod == 0)
		txr->epoch_bit = !txr->epoch_bit;
	tx_buf->inline_bds = CRYPTO_PREFIX_CMD_BDS - 1;
	txr->prod = prod;
}

static int bnxt_get_tls_record_len(struct mbuf *m)
{
	uint32_t rec_len = 0;

	/*
	 * Single-page record: the whole payload is m_epg_last_len bytes,
	 * per m_epg_pagelen()'s handling of pidx == m_epg_npgs - 1 == 0.
	 */
	if (m->m_epg_npgs == 1)
		return m->m_epg_last_len;

	rec_len += PAGE_SIZE - m->m_epg_1st_off;
	if (m->m_epg_npgs > 2)
	      rec_len += PAGE_SIZE * (m->m_epg_npgs - 2);

	rec_len += m->m_epg_last_len;

	return rec_len;
}

static struct mbuf *bnxt_get_tls_record(struct mbuf *m0, device_t dev)
{
	struct mbuf *m = m0;

	/* find TLS record */
	for (; m != NULL; m = m->m_next) {
		if (!(m->m_flags & M_EXTPG))
			continue;
		else
			break;
	}

	if (__predict_false((m == NULL))) {
		/* should never happen; means we got a non-TLS mbuf */
		device_printf(dev, "kTLS: non-TLS packet in %s\n", __func__);
		return NULL;
	}
	return m;
}

static struct tcphdr *bnxt_get_tcp_seq(if_pkt_info_t pi, struct mbuf *m, struct mbuf **mp)
{
	struct tcphdr *th = NULL;
	uint32_t hlen;

	hlen = pi->ipi_ehdrlen + pi->ipi_ip_hlen + pi->ipi_tcp_hlen;
	if (m->m_len < hlen) {
		m = m_pullup(m, hlen);
		if (m == NULL)
			return NULL;
		*mp = m;
	}

	th = mtodo(m, pi->ipi_ehdrlen + pi->ipi_ip_hlen);

	return th;
}


static int bnxt_ktls_tx_replay(struct bnxt_softc *bp, struct bnxt_ring *txr,
    struct mbuf *mb, struct bnxt_ktls_offload_ctx_tx *kctx_tx, uint32_t seq,
    uint32_t hdr_tcp_seq, if_pkt_info_t pi)
{
	struct bnxt_tls_info *ktls;
	struct mbuf *r_mb, *mp, *rec_mb;
	struct tcphdr *th;
	int hsz;
	uint32_t replay_len;
	uint32_t tcp_seq = hdr_tcp_seq;

	replay_len = seq - hdr_tcp_seq;

	ktls = bp->ktls_info;

	r_mb =  m_copypacket(mb, M_NOWAIT);

	if (r_mb == NULL) {
		BNXT_DEBUG(bp->dev, "[KID:%u]: kTLS replay m_copypacket failed\n", kctx_tx->kid);
		return EINVAL;
	}

	counter_u64_add(ktls->counters[BNXT_KTLS_TX_MBUF_ALLOCS], 1);
	hsz = (pi->ipi_ehdrlen + pi->ipi_ip_hlen + pi->ipi_tcp_hlen);

	mp = r_mb;

	th = bnxt_get_tcp_seq(pi, r_mb, &mp);

	r_mb = mp;

	if (th == NULL) {
		/* m_pullup() already called m_freem on r_mb on failure.
		 * so no m_freem(r_mb) is needed again.
		 */
		BNXT_DEBUG(bp->dev, "[KID:%u]: kTLS replay get_tcp_seq failed\n", kctx_tx->kid);
		counter_u64_add(ktls->counters[BNXT_KTLS_TX_MBUF_FREES], 1);
		counter_u64_add(ktls->counters[BNXT_KTLS_TX_MBUF_ERRORS], 1);
		return EINVAL;
	}

	th->th_seq = htonl(tcp_seq);

	/*
	 * The record mbuf isn't necessarily r_mb->m_next: m_pullup() above may
	 * have reshaped the chain, so find it via bnxt_get_tls_record().
	 */
	rec_mb = bnxt_get_tls_record(r_mb, bp->dev);
	if (rec_mb == NULL) {
		BNXT_DEBUG(bp->dev, "[KID:%u]: kTLS replay missing payload mbuf\n", kctx_tx->kid);
		counter_u64_add(ktls->counters[BNXT_KTLS_TX_MBUF_ERRORS], 1);
		counter_u64_add(ktls->counters[BNXT_KTLS_TX_MBUF_FREES], 1);
		m_freem(r_mb);
		return EINVAL;
	}

	rec_mb->m_data -= replay_len;
	rec_mb->m_len = replay_len;

	r_mb->m_pkthdr.len = replay_len + hsz;

	if (bnxt_ktls_submit_mbuf(bp, txr, &r_mb, kctx_tx, replay_len, hsz, pi) != 0) {
		BNXT_DEBUG(bp->dev, "[KID:%u]: kTLS replay submit_mbuf failed\n", kctx_tx->kid);
		counter_u64_add(ktls->counters[BNXT_KTLS_TX_MBUF_FREES], 1);
		counter_u64_add(ktls->counters[BNXT_KTLS_TX_MBUF_ERRORS], 1);
		return ENOSPC;
	}

	counter_u64_add(ktls->counters[BNXT_KTLS_TX_REPLAY], 1);

	return 0;
}

static int bnxt_ktls_tx_ooo(struct bnxt_softc *bp, struct bnxt_ring *txr,
    struct mbuf *mbuf, uint32_t payload_len, uint32_t seq,
    struct bnxt_ktls_offload_ctx_tx *kctx_tx, if_pkt_info_t pi)
{
	struct tls_record_layer *tlshdr;
	struct crypto_prefix_cmd pcmd = {0};
	struct bnxt_tls_info *ktls;
	uint32_t presync_flags = 0;
	struct mbuf *m;
	uint32_t hdr_tcp_seq;
	uint8_t *hdr = NULL;
	bool fwd = false;
	int rc = 0;
	uint64_t rec_sn = 0;
	uint32_t rec_len;
	uint32_t record_end_seq;
	uint32_t offset;
	uint32_t nonce_bytes;
	uint32_t end_seq;
	bool is_replay = false;
	bool update_inorder = false;
	uint32_t tx_prod;
	uint16_t running_bds;
	bool epoch_bit;

	ktls = bp->ktls_info;

	m = bnxt_get_tls_record(mbuf, bp->dev);
	if (m == NULL) {
		device_printf(bp->dev, "kTLS OOO: no TLS record in mbuf\n");
		return EINVAL;
	}

	bnxt_get_tls_seq(m, &rec_sn);
	rec_len = bnxt_get_tls_record_len(m);

	/* m_data is a byte offset into the TLS record for M_EXTPG mbufs, not a pointer. */
	offset = (uintptr_t)m->m_data;
	tlshdr = (void *)m->m_epg_hdr;
	hdr_tcp_seq = seq - offset;
	hdr = (uint8_t*)tlshdr;

	record_end_seq =
		(hdr_tcp_seq + rec_len + m->m_epg_hdrlen + m->m_epg_trllen) - 1;

	end_seq = seq + payload_len - 1;

	if (__predict_false(after(seq, kctx_tx->tcp_seq_no) ||
		after(end_seq, kctx_tx->tcp_seq_no))) {
		counter_u64_add(ktls->counters[BNXT_KTLS_TX_SEQ_FWD], 1);
		fwd = true;

		/* Check if the seq forward packet has a TLS header*/
		if (seq <= hdr_tcp_seq && end_seq >= hdr_tcp_seq) {
			counter_u64_add(ktls->counters[BNXT_KTLS_TX_SEQ_FWD_TLS_HDR], 1);
			presync_flags = CRYPTO_PREFIX_CMD_FLAGS_UPDATE_IN_ORDER_VAR;
			update_inorder = true;
		} else {
			counter_u64_add(ktls->counters[BNXT_KTLS_TX_SEQ_FWD_SILENT_DROPS], 1);
		}
	} else
		counter_u64_add(ktls->counters[BNXT_KTLS_TX_RETRANS], 1);

	pcmd.header_tcp_seq_num = htole32(hdr_tcp_seq);
	pcmd.start_tcp_seq_num = htole32(seq);
	pcmd.end_tcp_seq_num = htole32(end_seq);

	if ((kctx_tx->crypto_info.tls_vmajor ==
		TLS_MAJOR_VER_ONE) &&
		(kctx_tx->crypto_info.tls_vminor ==
		 TLS_MINOR_VER_TWO)) {
		nonce_bytes = BNXT_EXPLICIT_NONCE_LEN;

		if (offset > 5 && (offset < (5 + nonce_bytes)))
			nonce_bytes = offset - 5;

		/* Verify we don't exceed TLS header bounds */
		if ((hdr + 5 + nonce_bytes) > ((uint8_t*)tlshdr + m->m_epg_hdrlen)) {
			BNXT_DEBUG(bp->dev, "kTLS OOO: TLS header bounds exceeded\n");
			return EINVAL;
		}

		memcpy(pcmd.explicit_nonce, hdr + 5, nonce_bytes);
	}

	/* Out-of-sequence packet includes tag(trailer) */
	if (offset && (record_end_seq  - m->m_epg_trllen) <
		   (seq + payload_len)) {
		is_replay = true;
	}

	memcpy(&pcmd.record_seq_num[0], &rec_sn, sizeof(rec_sn));

	if (update_inorder)
		presync_flags = CRYPTO_PREFIX_CMD_FLAGS_UPDATE_IN_ORDER_VAR;
	else if (is_replay)
		presync_flags = CRYPTO_PREFIX_CMD_FLAGS_FULL_REPLAY_RETRAN;
	else
		presync_flags = 0;

	pcmd.flags = htole32(presync_flags);

	/*
	 * Save producer/running_bds/epoch_bit before the presync command so
	 * they can be rolled back if the replay packet fails to post.
	 */
	tx_prod = txr->prod;
	running_bds = txr->running_bds;
	epoch_bit = txr->epoch_bit;

	bnxt_ktls_pre_xmit(bp, txr, kctx_tx->kid, &pcmd);

	if (is_replay) {
		rc = bnxt_ktls_tx_replay(bp, txr, mbuf, kctx_tx, seq, hdr_tcp_seq, pi);

		if (fwd)
			counter_u64_add(ktls->counters[BNXT_KTLS_TX_SEQ_FWD_REPLAY], 1);
		/*
		 * If replay failed, discard presync BD by rolling back the
		 * producer index, running_bds accumulator, and epoch_bit.
		 */
		if (rc) {
			txr->prod = tx_prod;
			txr->running_bds = running_bds;
			txr->epoch_bit = epoch_bit;
		}
	}

	if (update_inorder && !rc)
		kctx_tx->tcp_seq_no = end_seq + 1;

	return rc;
}

int bnxt_ktls_xmit(struct bnxt_softc* softc, struct bnxt_ring* txr,
    struct mbuf** ppmb, uint16_t* lflags, uint32_t* kid, if_pkt_info_t pi)
{
	struct bnxt_ktls_offload_ctx_tx* kctx_tx;
	struct bnxt_tls_info* ktls = softc->ktls_info;
	struct m_snd_tag* ptag;
	struct mbuf* mb = *ppmb;
	uint64_t rec_sn;
	uint32_t header_size;
	uint32_t mb_seq;
	int rc;

	if ((mb->m_pkthdr.csum_flags & CSUM_SND_TAG) == 0)
		return (0);

	ptag = mb->m_pkthdr.snd_tag;

	if (ptag == NULL || ptag->sw == NULL ||
	    ptag->sw->type != IF_SND_TAG_TYPE_TLS) {
		counter_u64_add(ktls->err_counters[BNXT_KTLS_TX_INVALID_TAG], 1);
		return (0);
	}

	kctx_tx = __containerof(ptag, struct bnxt_ktls_offload_ctx_tx, tag);

	header_size = (pi->ipi_ehdrlen + pi->ipi_ip_hlen + pi->ipi_tcp_hlen);


	if (__predict_false(header_size == 0)) {
		BNXT_DEBUG(softc->dev, "kTLS xmit: zero header size\n");
		return EINVAL;
	}

	if (header_size == mb->m_pkthdr.len) {
		counter_u64_add(ktls->err_counters[BNXT_KTLS_TX_ZERO_TCP_PAYLEN], 1);
		BNXT_DEBUG(softc->dev, "kTLS xmit: zero TCP payload\n");
		return (0);
	}

	if (bnxt_get_tls_seq(mb, &rec_sn) == 0) {
		counter_u64_add(ktls->err_counters[BNXT_KTLS_TX_INVALID_REC_SN], 1);
		BNXT_DEBUG(softc->dev, "kTLS xmit: invalid record seq\n");
		return (0);
	}

	mb_seq = ntohl(pi->ipi_tcp_seq);

	/* A FW reset since this tag's KID was programmed erased that
	 * firmware-side context; don't keep sending under a KID firmware may
	 * have since reassigned to an unrelated session. */
	if (kctx_tx->state == BNXT_TLS_STATE_READY &&
	    kctx_tx->gen != atomic_load_32(&ktls->reset_gen))
		kctx_tx->state = BNXT_TLS_STATE_BAD;

	switch(kctx_tx->state) {
		/* Add crypto MPC is not yet completed*/
		case BNXT_TLS_STATE_INIT:
			if (!kctx_tx->enobufs)
				counter_u64_add(ktls->err_counters[BNXT_KTLS_TX_TCE_NOT_READY], 1);

			kctx_tx->enobufs++;

			BNXT_DEBUG(softc->dev, "[KID:%u]: TCE INIT:"
			    " tcp_seqno: %u record_no: %lu enobufs: %u\n",
			    kctx_tx->kid, mb_seq, rec_sn, kctx_tx->enobufs);

			return ENOBUFS;
		/* Crypto connection is setup and ready to use */
		case BNXT_TLS_STATE_READY:
			if (kctx_tx->enobufs) {
				counter_u64_add(ktls->err_counters[BNXT_KTLS_TX_TCE_READY_DELAYED], 1);
				BNXT_DEBUG(softc->dev, "[KID:%u]: TCE READY:"
				    " tcp_seqno: %u record_no: %lu enobufs: %u\n",
				    kctx_tx->kid, mb_seq, rec_sn, kctx_tx->enobufs);
				kctx_tx->enobufs = 0;
			}
			break;
		/* Crypto connection is in bad state */
		case BNXT_TLS_STATE_BAD:
			counter_u64_add(ktls->err_counters[BNXT_KTLS_TX_TCE_BAD], 1);
			BNXT_DEBUG(softc->dev, "[KID:%u]: TCE BAD:"
			    " tcp_seqno: %u record_no: %lu enobufs: %u\n",
			    kctx_tx->kid, mb_seq, rec_sn, kctx_tx->enobufs);
			return EINVAL;
		case BNXT_TLS_STATE_FREE:
		case BNXT_TLS_STATE_FREED:
			counter_u64_add(ktls->err_counters[BNXT_KTLS_TX_TCE_FREE], 1);
			/* FALLTHROUGH */
		default:
			return EINVAL;
	}

	*kid = kctx_tx->kid;
	*lflags |= TX_BD_LONG_LFLAGS_CRYPTO_EN;

	if (kctx_tx->tcp_seq_no == mb_seq) {
		kctx_tx->inorder_rec_sn = rec_sn;
		kctx_tx->inorder_paylen = mb->m_pkthdr.len - header_size;
		kctx_tx->inorder_seq = kctx_tx->tcp_seq_no;
		kctx_tx->tcp_seq_no += mb->m_pkthdr.len - header_size;
		counter_u64_add(ktls->counters[BNXT_KTLS_TX_HW_PKT], 1);
	} else {
		uint32_t payload_len;

		payload_len = mb->m_pkthdr.len - header_size;

		counter_u64_add(ktls->counters[BNXT_KTLS_TX_OOO], 1);

		rc = bnxt_ktls_tx_ooo(softc, txr, mb, payload_len, mb_seq, kctx_tx, pi);
		if (rc) {
			counter_u64_add(ktls->counters[BNXT_KTLS_TX_FAILED], 1);
			BNXT_DEBUG(softc->dev, "[KID:%u]: kTLS OOO failed, rc=%d\n", kctx_tx->kid, rc);
			return EAGAIN;
		}
	}

	return (0);
}

#else  /* KERN_TLS */

void bnxt_free_ktls_counters(struct bnxt_softc* softc)
{
}

void bnxt_alloc_ktls_counters(struct bnxt_softc* softc)
{
}

void bnxt_alloc_ktls_info(struct bnxt_softc* softc, uint16_t max_keys,
			  bool partition_cap, uint16_t ctxs_per_partition)
{
}

void bnxt_free_ktls_info(struct bnxt_softc* softc)
{
}

void bnxt_hwrm_reserve_pf_key_ctxs(struct bnxt_softc* softc,
				   struct hwrm_func_cfg_input* req,
				   uint8_t type)
{
}

int bnxt_ktls_init(struct bnxt_softc* softc)
{
	return 0;
}

void bnxt_ktls_mpc_cmp(struct bnxt_softc* softc, uint32_t client,
		       unsigned long handle, struct bnxt_cmpl_entry cmpl[],
		       uint32_t entries)
{
}

int bnxt_ktls_xmit(struct bnxt_softc* softc, struct bnxt_ring* txr,
    struct mbuf** ppmb, uint16_t* lflags, uint32_t* kid, if_pkt_info_t pi)
{
	return 0;
}

void bnxt_ktls_del_all(struct bnxt_softc *softc)
{
}

void bnxt_clear_ktls(struct bnxt_softc *softc)
{
}

int
bnxt_alloc_ktls_rexmit_mbuf(struct bnxt_softc *softc, struct bnxt_ring *txr)
{
	return 0;
}

inline void
bnxt_free_ktls_rexmit_mbuf(struct bnxt_ring *txr)
{
}

#endif	/* KERN_TLS */
