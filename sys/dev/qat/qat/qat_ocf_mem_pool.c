/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* System headers */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <machine/bus.h>

/* Cryptodev headers */
#include <opencrypto/cryptodev.h>
#include <opencrypto/xform.h>

/* QAT specific headers */
#include "qat_ocf_mem_pool.h"
#include "qat_ocf_utils.h"
#include "cpa.h"

/* Private functions */
static void
qat_ocf_alloc_single_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct qat_ocf_dma_mem *dma_mem;

	if (error != 0)
		return;

	dma_mem = arg;
	dma_mem->dma_seg = segs[0];
}

static int
qat_ocf_populate_buf_list_cb(struct qat_ocf_buffer_list *buffers,
			     bus_dma_segment_t *segs,
			     int niseg,
			     int skip_seg,
			     int skip_bytes)
{
	CpaPhysFlatBuffer *flatBuffer;
	bus_addr_t segment_addr;
	bus_size_t segment_len;
	int iseg, oseg;

	for (iseg = 0, oseg = skip_seg;
	     iseg < niseg && oseg < QAT_OCF_MAX_FLATS;
	     iseg++) {
		segment_addr = segs[iseg].ds_addr;
		segment_len = segs[iseg].ds_len;

		if (skip_bytes > 0) {
			if (skip_bytes < segment_len) {
				segment_addr += skip_bytes;
				segment_len -= skip_bytes;
				skip_bytes = 0;
			} else {
				skip_bytes -= segment_len;
				continue;
			}
		}
		flatBuffer = &buffers->flatBuffers[oseg++];
		flatBuffer->dataLenInBytes = (Cpa32U)segment_len;
		flatBuffer->bufferPhysAddr = (CpaPhysicalAddr)segment_addr;
	};
	buffers->numBuffers = oseg;

	return iseg < niseg ? E2BIG : 0;
}

void
qat_ocf_crypto_load_aadbuf_cb(void *_arg,
			      bus_dma_segment_t *segs,
			      int nseg,
			      int error)
{
	struct qat_ocf_load_cb_arg *arg;
	struct qat_ocf_cookie *qat_cookie;

	arg = _arg;
	if (error != 0) {
		arg->error = error;
		return;
	}

	qat_cookie = arg->qat_cookie;
	arg->error = qat_ocf_populate_buf_list_cb(
	    &qat_cookie->src_buffers, segs, nseg, 0, 0);
}

void
qat_ocf_crypto_load_buf_cb(void *_arg,
			   bus_dma_segment_t *segs,
			   int nseg,
			   int error)
{
	struct qat_ocf_cookie *qat_cookie;
	struct qat_ocf_load_cb_arg *arg;
	int start_segment = 0, skip_bytes = 0;

	arg = _arg;
	if (error != 0) {
		arg->error = error;
		return;
	}

	qat_cookie = arg->qat_cookie;

	skip_bytes = 0;
	start_segment = qat_cookie->src_buffers.numBuffers;

	arg->error = qat_ocf_populate_buf_list_cb(
	    &qat_cookie->src_buffers, segs, nseg, start_segment, skip_bytes);
}

void
qat_ocf_crypto_load_obuf_cb(void *_arg,
			    bus_dma_segment_t *segs,
			    int nseg,
			    int error)
{
	struct qat_ocf_load_cb_arg *arg;
	struct cryptop *crp;
	struct qat_ocf_cookie *qat_cookie;
	const struct crypto_session_params *csp;
	int osegs = 0, to_copy = 0;

	arg = _arg;
	if (error != 0) {
		arg->error = error;
		return;
	}

	crp = arg->crp_op;
	qat_cookie = arg->qat_cookie;
	csp = crypto_get_params(crp->crp_session);

	/*
	 * The payload must start at the same offset in the output SG list as in
	 * the input SG list.  Copy over SG entries from the input corresponding
	 * to the AAD buffer.
	 */
	if (crp->crp_aad_length == 0 ||
	    (CPA_TRUE == is_sep_aad_supported(csp) && crp->crp_aad)) {
		arg->error =
		    qat_ocf_populate_buf_list_cb(&qat_cookie->dst_buffers,
						 segs,
						 nseg,
						 0,
						 crp->crp_payload_output_start);
		return;
	}

	/* Copy AAD from source SGL to keep payload in the same position in
	 * destination buffers */
	if (NULL == crp->crp_aad)
		to_copy = crp->crp_payload_start - crp->crp_aad_start;
	else
		to_copy = crp->crp_aad_length;

	for (; osegs < qat_cookie->src_buffers.numBuffers; osegs++) {
		CpaPhysFlatBuffer *src_flat;
		CpaPhysFlatBuffer *dst_flat;
		int data_len;

		if (to_copy <= 0)
			break;

		src_flat = &qat_cookie->src_buffers.flatBuffers[osegs];
		dst_flat = &qat_cookie->dst_buffers.flatBuffers[osegs];

		dst_flat->bufferPhysAddr = src_flat->bufferPhysAddr;
		data_len = imin(src_flat->dataLenInBytes, to_copy);
		dst_flat->dataLenInBytes = data_len;
		to_copy -= data_len;
	}

	arg->error =
	    qat_ocf_populate_buf_list_cb(&qat_cookie->dst_buffers,
					 segs,
					 nseg,
					 osegs,
					 crp->crp_payload_output_start);
}

static int
qat_ocf_alloc_dma_mem(device_t dev,
		      struct qat_ocf_dma_mem *dma_mem,
		      int nseg,
		      bus_size_t size,
		      bus_size_t alignment)
{
	int error;

	error = bus_dma_tag_create(bus_get_dma_tag(dev),
				   alignment,
				   0,		      /* alignment, boundary */
				   BUS_SPACE_MAXADDR, /* lowaddr */
				   BUS_SPACE_MAXADDR, /* highaddr */
				   NULL,
				   NULL,	     /* filter, filterarg */
				   size,	     /* maxsize */
				   nseg,	     /* nsegments */
				   size,	     /* maxsegsize */
				   BUS_DMA_COHERENT, /* flags */
				   NULL,
				   NULL, /* lockfunc, lockarg */
				   &dma_mem->dma_tag);
	if (error != 0) {
		device_printf(dev,
			      "couldn't create DMA tag, error = %d\n",
			      error);
		return error;
	}

	error =
	    bus_dmamem_alloc(dma_mem->dma_tag,
			     &dma_mem->dma_vaddr,
			     BUS_DMA_NOWAIT | BUS_DMA_ZERO | BUS_DMA_COHERENT,
			     &dma_mem->dma_map);
	if (error != 0) {
		device_printf(dev,
			      "couldn't allocate dmamem, error = %d\n",
			      error);
		goto fail_0;
	}

	error = bus_dmamap_load(dma_mem->dma_tag,
				dma_mem->dma_map,
				dma_mem->dma_vaddr,
				size,
				qat_ocf_alloc_single_cb,
				dma_mem,
				BUS_DMA_NOWAIT);
	if (error) {
		device_printf(dev,
			      "couldn't load dmamem map, error = %d\n",
			      error);
		goto fail_1;
	}

	return 0;
fail_1:
	bus_dmamem_free(dma_mem->dma_tag, dma_mem->dma_vaddr, dma_mem->dma_map);
fail_0:
	bus_dma_tag_destroy(dma_mem->dma_tag);

	return error;
}

static void
qat_ocf_free_dma_mem(struct qat_ocf_dma_mem *qdm)
{
	if (qdm->dma_tag != NULL && qdm->dma_vaddr != NULL) {
		bus_dmamap_unload(qdm->dma_tag, qdm->dma_map);
		bus_dmamem_free(qdm->dma_tag, qdm->dma_vaddr, qdm->dma_map);
		bus_dma_tag_destroy(qdm->dma_tag);
		explicit_bzero(qdm, sizeof(*qdm));
	}
}

static int
qat_ocf_dma_tag_and_map(device_t dev,
			struct qat_ocf_dma_mem *dma_mem,
			bus_size_t size,
			bus_size_t segs)
{
	int error;

	error = bus_dma_tag_create(bus_get_dma_tag(dev),
				   1,
				   0,		      /* alignment, boundary */
				   BUS_SPACE_MAXADDR, /* lowaddr */
				   BUS_SPACE_MAXADDR, /* highaddr */
				   NULL,
				   NULL,	     /* filter, filterarg */
				   size,	     /* maxsize */
				   segs,	     /* nsegments */
				   size,	     /* maxsegsize */
				   BUS_DMA_COHERENT, /* flags */
				   NULL,
				   NULL, /* lockfunc, lockarg */
				   &dma_mem->dma_tag);
	if (error != 0)
		return error;

	error = bus_dmamap_create(dma_mem->dma_tag,
				  BUS_DMA_COHERENT,
				  &dma_mem->dma_map);
	if (error != 0)
		return error;

	return 0;
}

static void
qat_ocf_clear_cookie(struct qat_ocf_cookie *qat_cookie)
{
	qat_cookie->src_buffers.numBuffers = 0;
	qat_cookie->dst_buffers.numBuffers = 0;
	qat_cookie->is_sep_aad_used = CPA_FALSE;
	explicit_bzero(qat_cookie->qat_ocf_iv_buf,
		       sizeof(qat_cookie->qat_ocf_iv_buf));
	explicit_bzero(qat_cookie->qat_ocf_digest,
		       sizeof(qat_cookie->qat_ocf_digest));
	explicit_bzero(qat_cookie->qat_ocf_gcm_aad,
		       sizeof(qat_cookie->qat_ocf_gcm_aad));
	qat_cookie->crp_op = NULL;
}

/* Public functions */
CpaStatus
qat_ocf_cookie_dma_pre_sync(struct cryptop *crp, CpaCySymDpOpData *pOpData)
{
	struct qat_ocf_cookie *qat_cookie;

	if (NULL == pOpData->pCallbackTag)
		return CPA_STATUS_FAIL;

	qat_cookie = (struct qat_ocf_cookie *)pOpData->pCallbackTag;

	if (CPA_TRUE == qat_cookie->is_sep_aad_used) {
		bus_dmamap_sync(qat_cookie->gcm_aad_dma_mem.dma_tag,
				qat_cookie->gcm_aad_dma_mem.dma_map,
				BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	}

	bus_dmamap_sync(qat_cookie->src_dma_mem.dma_tag,
			qat_cookie->src_dma_mem.dma_map,
			BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	if (CRYPTO_HAS_OUTPUT_BUFFER(crp)) {
		bus_dmamap_sync(qat_cookie->dst_dma_mem.dma_tag,
				qat_cookie->dst_dma_mem.dma_map,
				BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	}
	bus_dmamap_sync(qat_cookie->dma_tag,
			qat_cookie->dma_map,
			BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

	return CPA_STATUS_SUCCESS;
}

CpaStatus
qat_ocf_cookie_dma_post_sync(struct cryptop *crp, CpaCySymDpOpData *pOpData)
{
	struct qat_ocf_cookie *qat_cookie;

	if (NULL == pOpData->pCallbackTag)
		return CPA_STATUS_FAIL;

	qat_cookie = (struct qat_ocf_cookie *)pOpData->pCallbackTag;

	bus_dmamap_sync(qat_cookie->src_dma_mem.dma_tag,
			qat_cookie->src_dma_mem.dma_map,
			BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	if (CRYPTO_HAS_OUTPUT_BUFFER(crp)) {
		bus_dmamap_sync(qat_cookie->dst_dma_mem.dma_tag,
				qat_cookie->dst_dma_mem.dma_map,
				BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	}
	bus_dmamap_sync(qat_cookie->dma_tag,
			qat_cookie->dma_map,
			BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	if (qat_cookie->is_sep_aad_used)
		bus_dmamap_sync(qat_cookie->gcm_aad_dma_mem.dma_tag,
				qat_cookie->gcm_aad_dma_mem.dma_map,
				BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	return CPA_STATUS_SUCCESS;
}

CpaStatus
qat_ocf_cookie_dma_unload(struct cryptop *crp, CpaCySymDpOpData *pOpData)
{
	struct qat_ocf_cookie *qat_cookie;

	qat_cookie = pOpData->pCallbackTag;

	if (NULL == qat_cookie)
		return CPA_STATUS_FAIL;

	bus_dmamap_unload(qat_cookie->src_dma_mem.dma_tag,
			  qat_cookie->src_dma_mem.dma_map);
	if (CRYPTO_HAS_OUTPUT_BUFFER(crp))
		bus_dmamap_unload(qat_cookie->dst_dma_mem.dma_tag,
				  qat_cookie->dst_dma_mem.dma_map);
	if (qat_cookie->is_sep_aad_used)
		bus_dmamap_unload(qat_cookie->gcm_aad_dma_mem.dma_tag,
				  qat_cookie->gcm_aad_dma_mem.dma_map);

	return CPA_STATUS_SUCCESS;
}

CpaStatus
qat_ocf_cookie_pool_init(struct qat_ocf_instance *instance, device_t dev)
{
	int i, error = 0;

	mtx_init(&instance->cookie_pool_mtx,
		 "QAT cookie pool MTX",
		 NULL,
		 MTX_DEF);
	instance->free_cookie_ptr = 0;
	for (i = 0; i < QAT_OCF_MEM_POOL_SIZE; i++) {
		struct qat_ocf_cookie *qat_cookie;
		struct qat_ocf_dma_mem *entry_dma_mem;

		entry_dma_mem = &instance->cookie_dmamem[i];

		/* Allocate DMA segment for cache entry.
		 * Cache has to be stored in DMAable mem due to
		 * it contains i.a src and dst flat buffer
		 * lists.
		 */
		error = qat_ocf_alloc_dma_mem(dev,
					      entry_dma_mem,
					      1,
					      sizeof(struct qat_ocf_cookie),
					      (1 << 6));
		if (error)
			break;

		qat_cookie = entry_dma_mem->dma_vaddr;
		instance->cookie_pool[i] = qat_cookie;

		qat_cookie->dma_map = entry_dma_mem->dma_map;
		qat_cookie->dma_tag = entry_dma_mem->dma_tag;

		qat_ocf_clear_cookie(qat_cookie);

		/* Physical address of IV buffer */
		qat_cookie->qat_ocf_iv_buf_paddr =
		    entry_dma_mem->dma_seg.ds_addr +
		    offsetof(struct qat_ocf_cookie, qat_ocf_iv_buf);

		/* Physical address of digest buffer */
		qat_cookie->qat_ocf_digest_paddr =
		    entry_dma_mem->dma_seg.ds_addr +
		    offsetof(struct qat_ocf_cookie, qat_ocf_digest);

		/* Physical address of AAD buffer */
		qat_cookie->qat_ocf_gcm_aad_paddr =
		    entry_dma_mem->dma_seg.ds_addr +
		    offsetof(struct qat_ocf_cookie, qat_ocf_gcm_aad);

		/* We already got physical address of src and dest SGL header */
		qat_cookie->src_buffer_list_paddr =
		    entry_dma_mem->dma_seg.ds_addr +
		    offsetof(struct qat_ocf_cookie, src_buffers);

		qat_cookie->dst_buffer_list_paddr =
		    entry_dma_mem->dma_seg.ds_addr +
		    offsetof(struct qat_ocf_cookie, dst_buffers);

		/* We already have physical address of pOpdata */
		qat_cookie->pOpData_paddr = entry_dma_mem->dma_seg.ds_addr +
		    offsetof(struct qat_ocf_cookie, pOpdata);
		/* Init QAT DP API OP data with const values */
		qat_cookie->pOpdata.pCallbackTag = (void *)qat_cookie;
		qat_cookie->pOpdata.thisPhys =
		    (CpaPhysicalAddr)qat_cookie->pOpData_paddr;

		error = qat_ocf_dma_tag_and_map(dev,
						&qat_cookie->src_dma_mem,
						QAT_OCF_MAXLEN,
						QAT_OCF_MAX_FLATS);
		if (error)
			break;

		error = qat_ocf_dma_tag_and_map(dev,
						&qat_cookie->dst_dma_mem,
						QAT_OCF_MAXLEN,
						QAT_OCF_MAX_FLATS);
		if (error)
			break;

		/* Max one flat buffer for embedded AAD if provided as separated
		 * by OCF and it's not supported by QAT */
		error = qat_ocf_dma_tag_and_map(dev,
						&qat_cookie->gcm_aad_dma_mem,
						QAT_OCF_MAXLEN,
						1);
		if (error)
			break;

		instance->free_cookie[i] = qat_cookie;
		instance->free_cookie_ptr++;
	}

	return error;
}

CpaStatus
qat_ocf_cookie_alloc(struct qat_ocf_instance *qat_instance,
		     struct qat_ocf_cookie **cookie_out)
{
	mtx_lock(&qat_instance->cookie_pool_mtx);
	if (qat_instance->free_cookie_ptr == 0) {
		mtx_unlock(&qat_instance->cookie_pool_mtx);
		return CPA_STATUS_FAIL;
	}
	*cookie_out =
	    qat_instance->free_cookie[--qat_instance->free_cookie_ptr];
	mtx_unlock(&qat_instance->cookie_pool_mtx);

	return CPA_STATUS_SUCCESS;
}

void
qat_ocf_cookie_free(struct qat_ocf_instance *qat_instance,
		    struct qat_ocf_cookie *cookie)
{
	qat_ocf_clear_cookie(cookie);
	mtx_lock(&qat_instance->cookie_pool_mtx);
	qat_instance->free_cookie[qat_instance->free_cookie_ptr++] = cookie;
	mtx_unlock(&qat_instance->cookie_pool_mtx);
}

void
qat_ocf_cookie_pool_deinit(struct qat_ocf_instance *qat_instance)
{
	int i;

	for (i = 0; i < QAT_OCF_MEM_POOL_SIZE; i++) {
		struct qat_ocf_cookie *cookie;
		struct qat_ocf_dma_mem *cookie_dma;

		cookie = qat_instance->cookie_pool[i];
		if (NULL == cookie)
			continue;

		/* Destroy tag and map for source SGL */
		if (cookie->src_dma_mem.dma_tag) {
			bus_dmamap_destroy(cookie->src_dma_mem.dma_tag,
					   cookie->src_dma_mem.dma_map);
			bus_dma_tag_destroy(cookie->src_dma_mem.dma_tag);
		}

		/* Destroy tag and map for dest SGL */
		if (cookie->dst_dma_mem.dma_tag) {
			bus_dmamap_destroy(cookie->dst_dma_mem.dma_tag,
					   cookie->dst_dma_mem.dma_map);
			bus_dma_tag_destroy(cookie->dst_dma_mem.dma_tag);
		}

		/* Destroy tag and map for separated AAD */
		if (cookie->gcm_aad_dma_mem.dma_tag) {
			bus_dmamap_destroy(cookie->gcm_aad_dma_mem.dma_tag,
					   cookie->gcm_aad_dma_mem.dma_map);
			bus_dma_tag_destroy(cookie->gcm_aad_dma_mem.dma_tag);
		}

		/* Free DMA memory */
		cookie_dma = &qat_instance->cookie_dmamem[i];
		qat_ocf_free_dma_mem(cookie_dma);
		qat_instance->cookie_pool[i] = NULL;
	}
	mtx_destroy(&qat_instance->cookie_pool_mtx);

	return;
}
