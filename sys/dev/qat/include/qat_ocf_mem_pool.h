/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#ifndef _QAT_OCF_MEM_POOL_H_
#define _QAT_OCF_MEM_POOL_H_

/* System headers */
#include <sys/types.h>

/* QAT specific headers */
#include "cpa.h"
#include "cpa_cy_sym_dp.h"
#include "icp_qat_fw_la.h"

#define QAT_OCF_MAX_LEN (64 * 1024)
#define QAT_OCF_MAX_FLATS (32)
#define QAT_OCF_MAX_DIGEST SHA512_DIGEST_LENGTH
#define QAT_OCF_MAX_SYMREQ (256)
#define QAT_OCF_MEM_POOL_SIZE ((QAT_OCF_MAX_SYMREQ * 2 + 1) * 2)
#define QAT_OCF_MAXLEN 64 * 1024

/* Dedicated structure due to flexible arrays not allowed to be
 * allocated on stack */
struct qat_ocf_buffer_list {
	Cpa64U reserved0;
	Cpa32U numBuffers;
	Cpa32U reserved1;
	CpaPhysFlatBuffer flatBuffers[QAT_OCF_MAX_FLATS];
};

struct qat_ocf_dma_mem {
	bus_dma_tag_t dma_tag;
	bus_dmamap_t dma_map;
	bus_dma_segment_t dma_seg;
	void *dma_vaddr;
} __aligned(64);

struct qat_ocf_cookie {
	/* Source SGLs */
	struct qat_ocf_buffer_list src_buffers;
	/* Destination SGL */
	struct qat_ocf_buffer_list dst_buffers;

	/* Cache OP data */
	CpaCySymDpOpData pOpdata;

	/* IV max size taken from cryptdev */
	uint8_t qat_ocf_iv_buf[EALG_MAX_BLOCK_LEN];
	bus_addr_t qat_ocf_iv_buf_paddr;
	uint8_t qat_ocf_digest[QAT_OCF_MAX_DIGEST];
	bus_addr_t qat_ocf_digest_paddr;
	/* Used only in case of separated AAD and GCM, CCM and RC4 */
	uint8_t qat_ocf_gcm_aad[ICP_QAT_FW_CCM_GCM_AAD_SZ_MAX];
	bus_addr_t qat_ocf_gcm_aad_paddr;

	/* Source SGLs */
	struct qat_ocf_dma_mem src_dma_mem;
	bus_addr_t src_buffer_list_paddr;

	/* Destination SGL */
	struct qat_ocf_dma_mem dst_dma_mem;
	bus_addr_t dst_buffer_list_paddr;

	/* AAD - used only if separated AAD is used by OCF and HW requires
	 * to have it at the beginning of source buffer */
	struct qat_ocf_dma_mem gcm_aad_dma_mem;
	bus_addr_t gcm_aad_buffer_list_paddr;
	CpaBoolean is_sep_aad_used;

	/* Cache OP data */
	bus_addr_t pOpData_paddr;
	/* misc */
	struct cryptop *crp_op;

	/* This cookie tag and map */
	bus_dma_tag_t dma_tag;
	bus_dmamap_t dma_map;
};

struct qat_ocf_session {
	CpaCySymSessionCtx sessionCtx;
	Cpa32U sessionCtxSize;
	Cpa32U authLen;
	Cpa32U aadLen;
};

struct qat_ocf_dsession {
	struct qat_ocf_instance *qatInstance;
	struct qat_ocf_session encSession;
	struct qat_ocf_session decSession;
};

struct qat_ocf_load_cb_arg {
	struct cryptop *crp_op;
	struct qat_ocf_cookie *qat_cookie;
	CpaCySymDpOpData *pOpData;
	int error;
};

struct qat_ocf_instance {
	CpaInstanceHandle cyInstHandle;
	struct mtx cyInstMtx;
	struct qat_ocf_dma_mem cookie_dmamem[QAT_OCF_MEM_POOL_SIZE];
	struct qat_ocf_cookie *cookie_pool[QAT_OCF_MEM_POOL_SIZE];
	struct qat_ocf_cookie *free_cookie[QAT_OCF_MEM_POOL_SIZE];
	int free_cookie_ptr;
	struct mtx cookie_pool_mtx;
	int32_t driver_id;
};

/* Init/deinit */
CpaStatus qat_ocf_cookie_pool_init(struct qat_ocf_instance *instance,
				   device_t dev);
void qat_ocf_cookie_pool_deinit(struct qat_ocf_instance *instance);
/* Alloc/free */
CpaStatus qat_ocf_cookie_alloc(struct qat_ocf_instance *instance,
			       struct qat_ocf_cookie **buffers_out);
void qat_ocf_cookie_free(struct qat_ocf_instance *instance,
			 struct qat_ocf_cookie *cookie);
/* Pre/post sync */
CpaStatus qat_ocf_cookie_dma_pre_sync(struct cryptop *crp,
				      CpaCySymDpOpData *pOpData);
CpaStatus qat_ocf_cookie_dma_post_sync(struct cryptop *crp,
				       CpaCySymDpOpData *pOpData);
/* Bus DMA unload */
CpaStatus qat_ocf_cookie_dma_unload(struct cryptop *crp,
				    CpaCySymDpOpData *pOpData);
/* Bus DMA load callbacks */
void qat_ocf_crypto_load_buf_cb(void *_arg,
				bus_dma_segment_t *segs,
				int nseg,
				int error);
void qat_ocf_crypto_load_obuf_cb(void *_arg,
				 bus_dma_segment_t *segs,
				 int nseg,
				 int error);
void qat_ocf_crypto_load_aadbuf_cb(void *_arg,
				   bus_dma_segment_t *segs,
				   int nseg,
				   int error);

#endif /* _QAT_OCF_MEM_POOL_H_ */
