/* SPDX-License-Identifier: BSD-2-Clause AND BSD-3-Clause */
/*	$NetBSD: qatvar.h,v 1.2 2020/03/14 18:08:39 ad Exp $	*/

/*
 * Copyright (c) 2019 Internet Initiative Japan, Inc.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *   Copyright(c) 2007-2019 Intel Corporation. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* $FreeBSD$ */

#ifndef _DEV_PCI_QATVAR_H_
#define _DEV_PCI_QATVAR_H_

#include <sys/counter.h>
#include <sys/malloc.h>

#include <opencrypto/cryptodev.h>

#define QAT_NSYMREQ	256
#define QAT_NSYMCOOKIE	((QAT_NSYMREQ * 2 + 1) * 2)

#define QAT_EV_NAME_SIZE		32
#define QAT_RING_NAME_SIZE		32

#define QAT_MAXSEG			HW_MAXSEG /* max segments for sg dma */
#define QAT_MAXLEN			65535	/* IP_MAXPACKET */

#define QAT_HB_INTERVAL			500	/* heartbeat msec */
#define QAT_SSM_WDT			100

enum qat_chip_type {
	QAT_CHIP_C2XXX = 0,	/* NanoQAT: Atom C2000 */
	QAT_CHIP_C2XXX_IOV,	
	QAT_CHIP_C3XXX,		/* Atom C3000 */
	QAT_CHIP_C3XXX_IOV,
	QAT_CHIP_C62X,
	QAT_CHIP_C62X_IOV,
	QAT_CHIP_D15XX,
	QAT_CHIP_D15XX_IOV,
	QAT_CHIP_DH895XCC,
	QAT_CHIP_DH895XCC_IOV,
};

enum qat_sku {
	QAT_SKU_UNKNOWN = 0,
	QAT_SKU_1,
	QAT_SKU_2,
	QAT_SKU_3,
	QAT_SKU_4,
	QAT_SKU_VF,
};

enum qat_ae_status {
	QAT_AE_ENABLED = 1,
	QAT_AE_ACTIVE,
	QAT_AE_DISABLED
};

#define TIMEOUT_AE_RESET	100
#define TIMEOUT_AE_CHECK	10000
#define TIMEOUT_AE_CSR		500
#define AE_EXEC_CYCLE		20

#define QAT_UOF_MAX_PAGE		1
#define QAT_UOF_MAX_PAGE_REGION		1

struct qat_dmamem {
	bus_dma_tag_t qdm_dma_tag;
	bus_dmamap_t qdm_dma_map;
	bus_size_t qdm_dma_size;
	bus_dma_segment_t qdm_dma_seg;
	void *qdm_dma_vaddr;
};

/* Valid internal ring size values */
#define QAT_RING_SIZE_128 0x01
#define QAT_RING_SIZE_256 0x02
#define QAT_RING_SIZE_512 0x03
#define QAT_RING_SIZE_4K 0x06
#define QAT_RING_SIZE_16K 0x08
#define QAT_RING_SIZE_4M 0x10
#define QAT_MIN_RING_SIZE QAT_RING_SIZE_128
#define QAT_MAX_RING_SIZE QAT_RING_SIZE_4M
#define QAT_DEFAULT_RING_SIZE QAT_RING_SIZE_16K

/* Valid internal msg size values */
#define QAT_MSG_SIZE_32 0x01
#define QAT_MSG_SIZE_64 0x02
#define QAT_MSG_SIZE_128 0x04
#define QAT_MIN_MSG_SIZE QAT_MSG_SIZE_32
#define QAT_MAX_MSG_SIZE QAT_MSG_SIZE_128

/* Size to bytes conversion macros for ring and msg size values */
#define QAT_MSG_SIZE_TO_BYTES(SIZE) (SIZE << 5)
#define QAT_BYTES_TO_MSG_SIZE(SIZE) (SIZE >> 5)
#define QAT_SIZE_TO_RING_SIZE_IN_BYTES(SIZE) ((1 << (SIZE - 1)) << 7)
#define QAT_RING_SIZE_IN_BYTES_TO_SIZE(SIZE) ((1 << (SIZE - 1)) >> 7)

/* Minimum ring buffer size for memory allocation */
#define QAT_RING_SIZE_BYTES_MIN(SIZE) \
	((SIZE < QAT_SIZE_TO_RING_SIZE_IN_BYTES(QAT_RING_SIZE_4K)) ? \
		QAT_SIZE_TO_RING_SIZE_IN_BYTES(QAT_RING_SIZE_4K) : SIZE)
#define QAT_RING_SIZE_MODULO(SIZE) (SIZE + 0x6)
#define QAT_SIZE_TO_POW(SIZE) ((((SIZE & 0x4) >> 1) | ((SIZE & 0x4) >> 2) | \
				SIZE) & ~0x4)
/* Max outstanding requests */
#define QAT_MAX_INFLIGHTS(RING_SIZE, MSG_SIZE) \
	((((1 << (RING_SIZE - 1)) << 3) >> QAT_SIZE_TO_POW(MSG_SIZE)) - 1)

#define QAT_RING_PATTERN		0x7f

struct qat_softc;

typedef int (*qat_cb_t)(struct qat_softc *, void *, void *);

struct qat_ring {
	struct mtx qr_ring_mtx;   /* Lock per ring */
	bool qr_need_wakeup;
	void *qr_ring_vaddr;
	uint32_t * volatile qr_inflight;	/* tx/rx shared */
	uint32_t qr_head;
	uint32_t qr_tail;
	uint8_t qr_msg_size;
	uint8_t qr_ring_size;
	uint32_t qr_ring;	/* ring number in bank */
	uint32_t qr_bank;	/* bank number in device */
	uint32_t qr_ring_id;
	uint32_t qr_ring_mask;
	qat_cb_t qr_cb;
	void *qr_cb_arg;
	struct qat_dmamem qr_dma;
	bus_addr_t qr_ring_paddr;

	const char *qr_name;
};

struct qat_bank {
	struct qat_softc *qb_sc;	/* back pointer to softc */
	uint32_t qb_intr_mask;		/* current interrupt mask */
	uint32_t qb_allocated_rings;	/* current allocated ring bitfiled */
	uint32_t qb_coalescing_time;	/* timer in nano sec, 0: disabled */
#define COALESCING_TIME_INTERVAL_DEFAULT	10000
#define COALESCING_TIME_INTERVAL_MIN		500
#define COALESCING_TIME_INTERVAL_MAX		0xfffff
	uint32_t qb_bank;		/* bank index */
	struct mtx qb_bank_mtx;
	struct resource *qb_ih;
	void *qb_ih_cookie;

	struct qat_ring qb_et_rings[MAX_RING_PER_BANK];

};

struct qat_ap_bank {
	uint32_t qab_nf_mask;
	uint32_t qab_nf_dest;
	uint32_t qab_ne_mask;
	uint32_t qab_ne_dest;
};

struct qat_ae_page {
	struct qat_ae_page *qap_next;
	struct qat_uof_page *qap_page;
	struct qat_ae_region *qap_region;
	u_int qap_flags;
};

#define QAT_AE_PAGA_FLAG_WAITING	(1 << 0)

struct qat_ae_region {
	struct qat_ae_page *qar_loaded_page;
	STAILQ_HEAD(, qat_ae_page) qar_waiting_pages;
};

struct qat_ae_slice {
	u_int qas_assigned_ctx_mask;
	struct qat_ae_region qas_regions[QAT_UOF_MAX_PAGE_REGION];
	struct qat_ae_page qas_pages[QAT_UOF_MAX_PAGE];
	struct qat_ae_page *qas_cur_pages[MAX_AE_CTX];
	struct qat_uof_image *qas_image;
};

#define QAT_AE(sc, ae)			\
		((sc)->sc_ae[ae])

struct qat_ae {
	u_int qae_state;		/* AE state */
	u_int qae_ustore_size;		/* free micro-store address */
	u_int qae_free_addr;		/* free micro-store address */
	u_int qae_free_size;		/* free micro-store size */
	u_int qae_live_ctx_mask;	/* live context mask */
	u_int qae_ustore_dram_addr;	/* micro-store DRAM address */
	u_int qae_reload_size;		/* reloadable code size */

	/* aefw */
	u_int qae_num_slices;
	struct qat_ae_slice qae_slices[MAX_AE_CTX];
	u_int qae_reloc_ustore_dram;	/* reloadable ustore-dram address */
	u_int qae_effect_ustore_size;	/* effective AE ustore size */
	u_int qae_shareable_ustore;
};

struct qat_mof {
	void *qmf_sym;			/* SYM_OBJS in sc_fw_mof */
	size_t qmf_sym_size;
	void *qmf_uof_objs;		/* UOF_OBJS in sc_fw_mof */
	size_t qmf_uof_objs_size;
	void *qmf_suof_objs;		/* SUOF_OBJS in sc_fw_mof */
	size_t qmf_suof_objs_size;
};

struct qat_ae_batch_init {
	u_int qabi_ae;
	u_int qabi_addr;
	u_int *qabi_value;
	u_int qabi_size;
	STAILQ_ENTRY(qat_ae_batch_init) qabi_next;
};

STAILQ_HEAD(qat_ae_batch_init_list, qat_ae_batch_init);

/* overwritten struct uof_uword_block */
struct qat_uof_uword_block {
	u_int quub_start_addr;		/* start address */
	u_int quub_num_words;		/* number of microwords */
	uint64_t quub_micro_words;	/* pointer to the uwords */
};

struct qat_uof_page {
	u_int qup_page_num;		/* page number */
	u_int qup_def_page;		/* default page */
	u_int qup_page_region;		/* region of page */
	u_int qup_beg_vaddr;		/* begin virtual address */ 
	u_int qup_beg_paddr;		/* begin physical address */

	u_int qup_num_uc_var;		/* num of uC var in array */
	struct uof_uword_fixup *qup_uc_var;
					/* array of import variables */
	u_int qup_num_imp_var;		/* num of import var in array */
	struct uof_import_var *qup_imp_var;
					/* array of import variables */
	u_int qup_num_imp_expr;		/* num of import expr in array */
	struct uof_uword_fixup *qup_imp_expr;
					/* array of import expressions */
	u_int qup_num_neigh_reg;	/* num of neigh-reg in array */
	struct uof_uword_fixup *qup_neigh_reg;
					/* array of neigh-reg assignments */
	u_int qup_num_micro_words;	/* number of microwords in the seg */

	u_int qup_num_uw_blocks;	/* number of uword blocks */
	struct qat_uof_uword_block *qup_uw_blocks;
					/* array of uword blocks */
};

struct qat_uof_image {
	struct uof_image *qui_image;		/* image pointer */
	struct qat_uof_page qui_pages[QAT_UOF_MAX_PAGE];
						/* array of pages */

	u_int qui_num_ae_reg;			/* num of registers */
	struct uof_ae_reg *qui_ae_reg;		/* array of registers */

	u_int qui_num_init_reg_sym;		/* num of reg/sym init values */
	struct uof_init_reg_sym *qui_init_reg_sym;
					/* array of reg/sym init values */

	u_int qui_num_sbreak;			/* num of sbreak values */
	struct qui_sbreak *qui_sbreak;		/* array of sbreak values */

	u_int qui_num_uwords_used;
				/* highest uword addressreferenced + 1 */
};

struct qat_aefw_uof {
	size_t qafu_size;			/* uof size */
	struct uof_obj_hdr *qafu_obj_hdr;	/* UOF_OBJS */

	void *qafu_str_tab;
	size_t qafu_str_tab_size;

	u_int qafu_num_init_mem;
	struct uof_init_mem *qafu_init_mem;
	size_t qafu_init_mem_size;

	struct uof_var_mem_seg *qafu_var_mem_seg;

	struct qat_ae_batch_init_list qafu_lm_init[MAX_AE];
	size_t qafu_num_lm_init[MAX_AE];
	size_t qafu_num_lm_init_inst[MAX_AE];

	u_int qafu_num_imgs;			/* number of uof image */
	struct qat_uof_image qafu_imgs[MAX_NUM_AE * MAX_AE_CTX];
						/* uof images */
};

#define QAT_SERVICE_CRYPTO_A		(1 << 0)
#define QAT_SERVICE_CRYPTO_B		(1 << 1)

struct qat_admin_rings {
	uint32_t qadr_active_aes_per_accel;
	uint8_t qadr_srv_mask[MAX_AE_PER_ACCEL];

	struct qat_dmamem qadr_dma;
	struct fw_init_ring_table *qadr_master_ring_tbl;
	struct fw_init_ring_table *qadr_cya_ring_tbl;
	struct fw_init_ring_table *qadr_cyb_ring_tbl;

	struct qat_ring *qadr_admin_tx;
	struct qat_ring *qadr_admin_rx;
};

struct qat_accel_init_cb {
	int qaic_status;
};

struct qat_admin_comms {
	struct qat_dmamem qadc_dma;
	struct qat_dmamem qadc_const_tbl_dma;
	struct qat_dmamem qadc_hb_dma;
};

#define QAT_PID_MINOR_REV 0xf
#define QAT_PID_MAJOR_REV (0xf << 4)

struct qat_suof_image {
	char *qsi_simg_buf;
	u_long qsi_simg_len;
	char *qsi_css_header;
	char *qsi_css_key;
	char *qsi_css_signature;
	char *qsi_css_simg;
	u_long qsi_simg_size;
	u_int qsi_ae_num;
	u_int qsi_ae_mask;
	u_int qsi_fw_type;
	u_long qsi_simg_name;
	u_long qsi_appmeta_data;
	struct qat_dmamem qsi_dma;
};

struct qat_aefw_suof {
	u_int qafs_file_id;
	u_int qafs_check_sum;
	char qafs_min_ver;
	char qafs_maj_ver;
	char qafs_fw_type;
	char *qafs_suof_buf;
	u_int qafs_suof_size;
	char *qafs_sym_str;
	u_int qafs_sym_size;
	u_int qafs_num_simgs;
	struct qat_suof_image *qafs_simg;
};

enum qat_sym_hash_algorithm {
	QAT_SYM_HASH_NONE = 0,
	QAT_SYM_HASH_MD5 = 1,
	QAT_SYM_HASH_SHA1 = 2,
	QAT_SYM_HASH_SHA224 = 3,
	QAT_SYM_HASH_SHA256 = 4,
	QAT_SYM_HASH_SHA384 = 5,
	QAT_SYM_HASH_SHA512 = 6,
	QAT_SYM_HASH_AES_XCBC = 7,
	QAT_SYM_HASH_AES_CCM = 8,
	QAT_SYM_HASH_AES_GCM = 9,
	QAT_SYM_HASH_KASUMI_F9 = 10,
	QAT_SYM_HASH_SNOW3G_UIA2 = 11,
	QAT_SYM_HASH_AES_CMAC = 12,
	QAT_SYM_HASH_AES_GMAC = 13,
	QAT_SYM_HASH_AES_CBC_MAC = 14,
};

#define QAT_HASH_MD5_BLOCK_SIZE			64
#define QAT_HASH_MD5_DIGEST_SIZE		16
#define QAT_HASH_MD5_STATE_SIZE			16
#define QAT_HASH_SHA1_BLOCK_SIZE		64
#define QAT_HASH_SHA1_DIGEST_SIZE		20
#define QAT_HASH_SHA1_STATE_SIZE		20
#define QAT_HASH_SHA224_BLOCK_SIZE		64
#define QAT_HASH_SHA224_DIGEST_SIZE		28
#define QAT_HASH_SHA224_STATE_SIZE		32
#define QAT_HASH_SHA256_BLOCK_SIZE		64
#define QAT_HASH_SHA256_DIGEST_SIZE		32
#define QAT_HASH_SHA256_STATE_SIZE		32
#define QAT_HASH_SHA384_BLOCK_SIZE		128
#define QAT_HASH_SHA384_DIGEST_SIZE		48
#define QAT_HASH_SHA384_STATE_SIZE		64
#define QAT_HASH_SHA512_BLOCK_SIZE		128
#define QAT_HASH_SHA512_DIGEST_SIZE		64
#define QAT_HASH_SHA512_STATE_SIZE		64
#define QAT_HASH_XCBC_PRECOMP_KEY_NUM		3
#define QAT_HASH_XCBC_MAC_BLOCK_SIZE		16
#define QAT_HASH_XCBC_MAC_128_DIGEST_SIZE	16
#define QAT_HASH_CMAC_BLOCK_SIZE		16
#define QAT_HASH_CMAC_128_DIGEST_SIZE		16
#define QAT_HASH_AES_CCM_BLOCK_SIZE		16
#define QAT_HASH_AES_CCM_DIGEST_SIZE		16
#define QAT_HASH_AES_GCM_BLOCK_SIZE		16
#define QAT_HASH_AES_GCM_DIGEST_SIZE		16
#define QAT_HASH_AES_GCM_STATE_SIZE		16
#define QAT_HASH_KASUMI_F9_BLOCK_SIZE		8
#define QAT_HASH_KASUMI_F9_DIGEST_SIZE		4
#define QAT_HASH_SNOW3G_UIA2_BLOCK_SIZE		8
#define QAT_HASH_SNOW3G_UIA2_DIGEST_SIZE	4
#define QAT_HASH_AES_CBC_MAC_BLOCK_SIZE		16
#define QAT_HASH_AES_CBC_MAC_DIGEST_SIZE	16
#define QAT_HASH_AES_GCM_ICV_SIZE_8		8
#define QAT_HASH_AES_GCM_ICV_SIZE_12		12
#define QAT_HASH_AES_GCM_ICV_SIZE_16		16
#define QAT_HASH_AES_CCM_ICV_SIZE_MIN		4
#define QAT_HASH_AES_CCM_ICV_SIZE_MAX		16
#define QAT_HASH_IPAD_BYTE			0x36
#define QAT_HASH_OPAD_BYTE			0x5c
#define QAT_HASH_IPAD_4_BYTES			0x36363636
#define QAT_HASH_OPAD_4_BYTES			0x5c5c5c5c
#define QAT_HASH_KASUMI_F9_KEY_MODIFIER_4_BYTES	0xAAAAAAAA

#define QAT_SYM_XCBC_STATE_SIZE		((QAT_HASH_XCBC_MAC_BLOCK_SIZE) * 3)
#define QAT_SYM_CMAC_STATE_SIZE		((QAT_HASH_CMAC_BLOCK_SIZE) * 3)

struct qat_sym_hash_alg_info {
	uint32_t qshai_digest_len;		/* Digest length in bytes */
	uint32_t qshai_block_len;		/* Block length in bytes */
	uint32_t qshai_state_size;		/* size of above state in bytes */
	const uint8_t *qshai_init_state;	/* Initial state */

	const struct auth_hash *qshai_sah;	/* software auth hash */
	uint32_t qshai_state_offset;		/* offset to state in *_CTX */
	uint32_t qshai_state_word;
};

struct qat_sym_hash_qat_info {
	uint32_t qshqi_algo_enc;	/* QAT Algorithm encoding */
	uint32_t qshqi_auth_counter;	/* Counter value for Auth */
	uint32_t qshqi_state1_len;	/* QAT state1 length in bytes */
	uint32_t qshqi_state2_len;	/* QAT state2 length in bytes */
};

struct qat_sym_hash_def {
	const struct qat_sym_hash_alg_info *qshd_alg;
	const struct qat_sym_hash_qat_info *qshd_qat;
};

#define QAT_SYM_REQ_PARAMS_SIZE_MAX			(24 + 32)
/* Reserve enough space for cipher and authentication request params */
/* Basis of values are guaranteed in qat_hw*var.h with CTASSERT */

#define QAT_SYM_REQ_PARAMS_SIZE_PADDED			\
		roundup(QAT_SYM_REQ_PARAMS_SIZE_MAX, QAT_OPTIMAL_ALIGN)
/* Pad out to 64-byte multiple to ensure optimal alignment of next field */

#define QAT_SYM_KEY_TLS_PREFIX_SIZE			(128)
/* Hash Prefix size in bytes for TLS (128 = MAX = SHA2 (384, 512)*/

#define QAT_SYM_KEY_MAX_HASH_STATE_BUFFER		\
		(QAT_SYM_KEY_TLS_PREFIX_SIZE * 2)
/* hash state prefix buffer structure that holds the maximum sized secret */

#define QAT_SYM_HASH_BUFFER_LEN			QAT_HASH_SHA512_STATE_SIZE
/* Buffer length to hold 16 byte MD5 key and 20 byte SHA1 key */

#define QAT_GCM_AAD_SIZE_MAX		240
/* Maximum AAD size */

#define	QAT_AES_GCM_AAD_ALIGN		16

struct qat_sym_bulk_cookie {
	uint8_t qsbc_req_params_buf[QAT_SYM_REQ_PARAMS_SIZE_PADDED];
	/* memory block reserved for request params, QAT 1.5 only
	 * NOTE: Field must be correctly aligned in memory for access by QAT
	 * engine */
	struct qat_crypto *qsbc_crypto;
	struct qat_session *qsbc_session;
	/* Session context */
	void *qsbc_cb_tag;
	/* correlator supplied by the client */
	uint8_t qsbc_msg[QAT_MSG_SIZE_TO_BYTES(QAT_MAX_MSG_SIZE)];
	/* QAT request message */
} __aligned(QAT_OPTIMAL_ALIGN);

/* Basis of values are guaranteed in qat_hw*var.h with CTASSERT */
#define HASH_CONTENT_DESC_SIZE		176
#define CIPHER_CONTENT_DESC_SIZE	64

#define CONTENT_DESC_MAX_SIZE	roundup(				\
		HASH_CONTENT_DESC_SIZE + CIPHER_CONTENT_DESC_SIZE,	\
		QAT_OPTIMAL_ALIGN)

enum qat_sym_dma {
	QAT_SYM_DMA_AADBUF = 0,
	QAT_SYM_DMA_BUF,
	QAT_SYM_DMA_OBUF,
	QAT_SYM_DMA_COUNT,
};

struct qat_sym_dmamap {
	bus_dmamap_t qsd_dmamap;
	bus_dma_tag_t qsd_dma_tag;
};

struct qat_sym_cookie {
	struct qat_sym_bulk_cookie qsc_bulk_cookie;

	/* should be 64-byte aligned */
	struct buffer_list_desc qsc_buf_list;
	struct buffer_list_desc qsc_obuf_list;

	bus_dmamap_t qsc_self_dmamap;
	bus_dma_tag_t qsc_self_dma_tag;

	uint8_t qsc_iv_buf[EALG_MAX_BLOCK_LEN];
	uint8_t qsc_auth_res[QAT_SYM_HASH_BUFFER_LEN];
	uint8_t qsc_gcm_aad[QAT_GCM_AAD_SIZE_MAX];
	uint8_t qsc_content_desc[CONTENT_DESC_MAX_SIZE];

	struct qat_sym_dmamap qsc_dma[QAT_SYM_DMA_COUNT];

	bus_addr_t qsc_bulk_req_params_buf_paddr;
	bus_addr_t qsc_buffer_list_desc_paddr;
	bus_addr_t qsc_obuffer_list_desc_paddr;
	bus_addr_t qsc_iv_buf_paddr;
	bus_addr_t qsc_auth_res_paddr;
	bus_addr_t qsc_gcm_aad_paddr;
	bus_addr_t qsc_content_desc_paddr;
};

CTASSERT(offsetof(struct qat_sym_cookie,
    qsc_bulk_cookie.qsbc_req_params_buf) % QAT_OPTIMAL_ALIGN == 0);
CTASSERT(offsetof(struct qat_sym_cookie, qsc_buf_list) % QAT_OPTIMAL_ALIGN == 0);

#define MAX_CIPHER_SETUP_BLK_SZ						\
		(sizeof(struct hw_cipher_config) +			\
		2 * HW_KASUMI_KEY_SZ + 2 * HW_KASUMI_BLK_SZ)
#define MAX_HASH_SETUP_BLK_SZ	sizeof(union hw_auth_algo_blk)

struct qat_crypto_desc {
	uint8_t qcd_content_desc[CONTENT_DESC_MAX_SIZE]; /* must be first */
	/* using only for qat 1.5 */
	uint8_t qcd_hash_state_prefix_buf[QAT_GCM_AAD_SIZE_MAX];

	bus_addr_t qcd_desc_paddr;
	bus_addr_t qcd_hash_state_paddr;

	enum fw_slice qcd_slices[MAX_FW_SLICE + 1];
	enum fw_la_cmd_id qcd_cmd_id;
	enum hw_cipher_dir qcd_cipher_dir;

	/* content desc info */
	uint8_t qcd_hdr_sz;		/* in quad words */
	uint8_t qcd_hw_blk_sz;		/* in quad words */
	uint32_t qcd_cipher_offset;
	uint32_t qcd_auth_offset;
	/* hash info */
	uint8_t qcd_state_storage_sz;	/* in quad words */
	uint32_t qcd_gcm_aad_sz_offset1;
	uint32_t qcd_gcm_aad_sz_offset2;
	/* cipher info */
	uint16_t qcd_cipher_blk_sz;	/* in bytes */
	uint16_t qcd_auth_sz;		/* in bytes */

	uint8_t qcd_req_cache[QAT_MSG_SIZE_TO_BYTES(QAT_MAX_MSG_SIZE)];
} __aligned(QAT_OPTIMAL_ALIGN);

struct qat_session {
	struct qat_crypto_desc *qs_dec_desc;	/* should be at top of struct*/
	/* decrypt or auth then decrypt or auth */

	struct qat_crypto_desc *qs_enc_desc;
	/* encrypt or encrypt then auth */

	struct qat_dmamem qs_desc_mem;

	enum hw_cipher_algo qs_cipher_algo;
	enum hw_cipher_mode qs_cipher_mode;
	enum hw_auth_algo qs_auth_algo;
	enum hw_auth_mode qs_auth_mode;

	const uint8_t *qs_cipher_key;
	int qs_cipher_klen;
	const uint8_t *qs_auth_key;
	int qs_auth_klen;
	int qs_auth_mlen;

	uint32_t qs_status;
#define QAT_SESSION_STATUS_ACTIVE	(1 << 0)
#define QAT_SESSION_STATUS_FREEING	(1 << 1)
	uint32_t qs_inflight;
	int qs_aad_length;
	bool qs_need_wakeup;

	struct mtx qs_session_mtx;
};

struct qat_crypto_bank {
	uint16_t qcb_bank;

	struct qat_ring *qcb_sym_tx;
	struct qat_ring *qcb_sym_rx;

	struct qat_dmamem qcb_symck_dmamems[QAT_NSYMCOOKIE];
	struct qat_sym_cookie *qcb_symck_free[QAT_NSYMCOOKIE];
	uint32_t qcb_symck_free_count;

	struct mtx qcb_bank_mtx;

	char qcb_ring_names[2][QAT_RING_NAME_SIZE];	/* sym tx,rx */
};

struct qat_crypto {
	struct qat_softc *qcy_sc;
	uint32_t qcy_bank_mask;
	uint16_t qcy_num_banks;

	int32_t qcy_cid;		/* OpenCrypto driver ID */

	struct qat_crypto_bank *qcy_banks; /* array of qat_crypto_bank */

	uint32_t qcy_session_free_count;

	struct mtx qcy_crypto_mtx;
};

struct qat_hw {
	int8_t qhw_sram_bar_id;
	int8_t qhw_misc_bar_id;
	int8_t qhw_etr_bar_id;

	bus_size_t qhw_cap_global_offset;
	bus_size_t qhw_ae_offset;
	bus_size_t qhw_ae_local_offset;
	bus_size_t qhw_etr_bundle_size;

	/* crypto processing callbacks */
	size_t qhw_crypto_opaque_offset;
	void (*qhw_crypto_setup_req_params)(struct qat_crypto_bank *,
	    struct qat_session *, struct qat_crypto_desc const *,
	    struct qat_sym_cookie *, struct cryptop *);
	void (*qhw_crypto_setup_desc)(struct qat_crypto *, struct qat_session *,
	    struct qat_crypto_desc *);

	uint8_t qhw_num_banks;			/* max number of banks */
	uint8_t qhw_num_ap_banks;		/* max number of AutoPush banks */
	uint8_t qhw_num_rings_per_bank;		/* rings per bank */
	uint8_t qhw_num_accel;			/* max number of accelerators */
	uint8_t qhw_num_engines;		/* max number of accelerator engines */
	uint8_t qhw_tx_rx_gap;
	uint32_t qhw_tx_rings_mask;
	uint32_t qhw_clock_per_sec;
	bool qhw_fw_auth;
	uint32_t qhw_fw_req_size;
	uint32_t qhw_fw_resp_size;

	uint8_t qhw_ring_sym_tx;
	uint8_t qhw_ring_sym_rx;
	uint8_t qhw_ring_asym_tx;
	uint8_t qhw_ring_asym_rx;

	/* MSIx */
	uint32_t qhw_msix_ae_vec_gap;	/* gap to ae vec from bank */

	const char *qhw_mof_fwname;
	const char *qhw_mmp_fwname;

	uint32_t qhw_prod_type;		/* cpu type */

	/* setup callbacks */
	uint32_t (*qhw_get_accel_mask)(struct qat_softc *);
	uint32_t (*qhw_get_ae_mask)(struct qat_softc *);
	enum qat_sku (*qhw_get_sku)(struct qat_softc *);
	uint32_t (*qhw_get_accel_cap)(struct qat_softc *);
	const char *(*qhw_get_fw_uof_name)(struct qat_softc *);
	void (*qhw_enable_intr)(struct qat_softc *);
	void (*qhw_init_etr_intr)(struct qat_softc *, int);
	int (*qhw_init_admin_comms)(struct qat_softc *);
	int (*qhw_send_admin_init)(struct qat_softc *);
	int (*qhw_init_arb)(struct qat_softc *);
	void (*qhw_get_arb_mapping)(struct qat_softc *, const uint32_t **);
	void (*qhw_enable_error_correction)(struct qat_softc *);
	int (*qhw_check_uncorrectable_error)(struct qat_softc *);
	void (*qhw_print_err_registers)(struct qat_softc *);
	void (*qhw_disable_error_interrupts)(struct qat_softc *);
	int (*qhw_check_slice_hang)(struct qat_softc *);
	int (*qhw_set_ssm_wdtimer)(struct qat_softc *);
};


/* sc_flags */
#define QAT_FLAG_ESRAM_ENABLE_AUTO_INIT	(1 << 0)
#define QAT_FLAG_SHRAM_WAIT_READY	(1 << 1)

/* sc_accel_cap */
#define QAT_ACCEL_CAP_CRYPTO_SYMMETRIC	(1 << 0)
#define QAT_ACCEL_CAP_CRYPTO_ASYMMETRIC	(1 << 1)
#define QAT_ACCEL_CAP_CIPHER		(1 << 2)
#define QAT_ACCEL_CAP_AUTHENTICATION	(1 << 3)
#define QAT_ACCEL_CAP_REGEX		(1 << 4)
#define QAT_ACCEL_CAP_COMPRESSION	(1 << 5)
#define QAT_ACCEL_CAP_LZS_COMPRESSION	(1 << 6)
#define QAT_ACCEL_CAP_RANDOM_NUMBER	(1 << 7)
#define QAT_ACCEL_CAP_ZUC		(1 << 8)
#define QAT_ACCEL_CAP_SHA3		(1 << 9)
#define QAT_ACCEL_CAP_KPT		(1 << 10)

#define QAT_ACCEL_CAP_BITS	\
	"\177\020"	\
	"b\x0a"		"KPT\0" \
	"b\x09"		"SHA3\0" \
	"b\x08"		"ZUC\0" \
	"b\x07"		"RANDOM_NUMBER\0" \
	"b\x06"		"LZS_COMPRESSION\0" \
	"b\x05"		"COMPRESSION\0" \
	"b\x04"		"REGEX\0" \
	"b\x03"		"AUTHENTICATION\0" \
	"b\x02"		"CIPHER\0" \
	"b\x01"		"CRYPTO_ASYMMETRIC\0" \
	"b\x00"		"CRYPTO_SYMMETRIC\0"

#define QAT_HI_PRIO_RING_WEIGHT		0xfc
#define QAT_LO_PRIO_RING_WEIGHT		0xfe
#define QAT_DEFAULT_RING_WEIGHT		0xff
#define QAT_DEFAULT_PVL			0

struct firmware;
struct resource;

struct qat_softc {
	device_t sc_dev;

	struct resource *sc_res[MAX_BARS];
	int sc_rid[MAX_BARS];
	bus_space_tag_t sc_csrt[MAX_BARS];
	bus_space_handle_t sc_csrh[MAX_BARS];

	uint32_t sc_ae_num;
	uint32_t sc_ae_mask;

	struct qat_crypto sc_crypto;		/* crypto services */

	struct qat_hw sc_hw;

	uint8_t sc_rev;
	enum qat_sku sc_sku;
	uint32_t sc_flags;

	uint32_t sc_accel_num;
	uint32_t sc_accel_mask;
	uint32_t sc_accel_cap;

	struct qat_admin_rings sc_admin_rings;	/* use only for qat 1.5 */
	struct qat_admin_comms sc_admin_comms;	/* use only for qat 1.7 */

	/* ETR */
	struct qat_bank *sc_etr_banks;		/* array of etr banks */
	struct qat_ap_bank *sc_etr_ap_banks;	/* array of etr auto push banks */

	/* AE */
	struct qat_ae sc_ae[MAX_NUM_AE];

	/* Interrupt */
	struct resource *sc_ih;			/* ae cluster ih */
	void *sc_ih_cookie;			/* ae cluster ih cookie */

	/* Counters */
	counter_u64_t sc_gcm_aad_restarts;
	counter_u64_t sc_gcm_aad_updates;
	counter_u64_t sc_ring_full_restarts;
	counter_u64_t sc_sym_alloc_failures;

	/* Firmware */
	void *sc_fw_mof;			/* mof data */
	size_t sc_fw_mof_size;			/* mof size */
	struct qat_mof sc_mof;			/* mof sections */

	const char *sc_fw_uof_name;		/* uof/suof name in mof */

	void *sc_fw_uof;			/* uof head */
	size_t sc_fw_uof_size;			/* uof size */
	struct qat_aefw_uof sc_aefw_uof;	/* UOF_OBJS in uof */

	void *sc_fw_suof;			/* suof head */
	size_t sc_fw_suof_size;			/* suof size */
	struct qat_aefw_suof sc_aefw_suof;	/* suof context */

	void *sc_fw_mmp;			/* mmp data */
	size_t sc_fw_mmp_size;			/* mmp size */
};

static inline void
qat_bar_write_4(struct qat_softc *sc, int baroff, bus_size_t offset,
    uint32_t value)
{

	MPASS(baroff >= 0 && baroff < MAX_BARS);

	bus_space_write_4(sc->sc_csrt[baroff],
	    sc->sc_csrh[baroff], offset, value);
}

static inline uint32_t
qat_bar_read_4(struct qat_softc *sc, int baroff, bus_size_t offset)
{

	MPASS(baroff >= 0 && baroff < MAX_BARS);

	return bus_space_read_4(sc->sc_csrt[baroff],
	    sc->sc_csrh[baroff], offset);
}

static inline void
qat_misc_write_4(struct qat_softc *sc, bus_size_t offset, uint32_t value)
{

	qat_bar_write_4(sc, sc->sc_hw.qhw_misc_bar_id, offset, value);
}

static inline uint32_t
qat_misc_read_4(struct qat_softc *sc, bus_size_t offset)
{

	return qat_bar_read_4(sc, sc->sc_hw.qhw_misc_bar_id, offset);
}

static inline void
qat_misc_read_write_or_4(struct qat_softc *sc, bus_size_t offset,
    uint32_t value)
{
	uint32_t reg;

	reg = qat_misc_read_4(sc, offset);
	reg |= value;
	qat_misc_write_4(sc, offset, reg);
}

static inline void
qat_misc_read_write_and_4(struct qat_softc *sc, bus_size_t offset,
    uint32_t mask)
{
	uint32_t reg;

	reg = qat_misc_read_4(sc, offset);
	reg &= mask;
	qat_misc_write_4(sc, offset, reg);
}

static inline void
qat_etr_write_4(struct qat_softc *sc, bus_size_t offset, uint32_t value)
{

	qat_bar_write_4(sc, sc->sc_hw.qhw_etr_bar_id, offset, value);
}

static inline uint32_t
qat_etr_read_4(struct qat_softc *sc, bus_size_t offset)
{

	return qat_bar_read_4(sc, sc->sc_hw.qhw_etr_bar_id, offset);
}

static inline void
qat_ae_local_write_4(struct qat_softc *sc, u_char ae, bus_size_t offset,
	uint32_t value)
{

	offset = __SHIFTIN(ae & sc->sc_ae_mask, AE_LOCAL_AE_MASK) |
	    (offset & AE_LOCAL_CSR_MASK);

	qat_misc_write_4(sc, sc->sc_hw.qhw_ae_local_offset + offset,
	    value);
}

static inline uint32_t
qat_ae_local_read_4(struct qat_softc *sc, u_char ae, bus_size_t offset)
{

	offset = __SHIFTIN(ae & sc->sc_ae_mask, AE_LOCAL_AE_MASK) |
	    (offset & AE_LOCAL_CSR_MASK);

	return qat_misc_read_4(sc, sc->sc_hw.qhw_ae_local_offset + offset);
}

static inline void
qat_ae_xfer_write_4(struct qat_softc *sc, u_char ae, bus_size_t offset,
	uint32_t value)
{
	offset = __SHIFTIN(ae & sc->sc_ae_mask, AE_XFER_AE_MASK) |
	    __SHIFTIN(offset, AE_XFER_CSR_MASK);

	qat_misc_write_4(sc, sc->sc_hw.qhw_ae_offset + offset, value);
}

static inline void
qat_cap_global_write_4(struct qat_softc *sc, bus_size_t offset, uint32_t value)
{

	qat_misc_write_4(sc, sc->sc_hw.qhw_cap_global_offset + offset, value);
}

static inline uint32_t
qat_cap_global_read_4(struct qat_softc *sc, bus_size_t offset)
{

	return qat_misc_read_4(sc, sc->sc_hw.qhw_cap_global_offset + offset);
}


static inline void
qat_etr_bank_write_4(struct qat_softc *sc, int bank,
	bus_size_t offset, uint32_t value)
{

	qat_etr_write_4(sc, sc->sc_hw.qhw_etr_bundle_size * bank + offset,
	    value);
}

static inline uint32_t
qat_etr_bank_read_4(struct qat_softc *sc, int bank,
	bus_size_t offset)
{

	return qat_etr_read_4(sc,
	    sc->sc_hw.qhw_etr_bundle_size * bank + offset);
}

static inline void
qat_etr_ap_bank_write_4(struct qat_softc *sc, int ap_bank,
	bus_size_t offset, uint32_t value)
{

	qat_etr_write_4(sc, ETR_AP_BANK_OFFSET * ap_bank + offset, value);
}

static inline uint32_t
qat_etr_ap_bank_read_4(struct qat_softc *sc, int ap_bank,
	bus_size_t offset)
{

	return qat_etr_read_4(sc, ETR_AP_BANK_OFFSET * ap_bank + offset);
}


static inline void
qat_etr_bank_ring_write_4(struct qat_softc *sc, int bank, int ring,
	bus_size_t offset, uint32_t value)
{

	qat_etr_bank_write_4(sc, bank, (ring << 2) + offset, value);
}

static inline uint32_t
qat_etr_bank_ring_read_4(struct qat_softc *sc, int bank, int ring,
	bus_size_t offset)
{

	return qat_etr_bank_read_4(sc, bank, (ring << 2) * offset);
}

static inline void
qat_etr_bank_ring_base_write_8(struct qat_softc *sc, int bank, int ring,
	uint64_t value)
{
	uint32_t lo, hi;

	lo = (uint32_t)(value & 0xffffffff);
	hi = (uint32_t)((value & 0xffffffff00000000ULL) >> 32);
	qat_etr_bank_ring_write_4(sc, bank, ring, ETR_RING_LBASE, lo);
	qat_etr_bank_ring_write_4(sc, bank, ring, ETR_RING_UBASE, hi);
}

static inline void
qat_arb_ringsrvarben_write_4(struct qat_softc *sc, int index, uint32_t value)
{

	qat_etr_write_4(sc, ARB_RINGSRVARBEN_OFFSET +
	    (ARB_REG_SLOT * index), value);
}

static inline void
qat_arb_sarconfig_write_4(struct qat_softc *sc, int index, uint32_t value)
{

	qat_etr_write_4(sc, ARB_OFFSET +
	    (ARB_REG_SIZE * index), value);
}

static inline void
qat_arb_wrk_2_ser_map_write_4(struct qat_softc *sc, int index, uint32_t value)
{

	qat_etr_write_4(sc, ARB_OFFSET + ARB_WRK_2_SER_MAP_OFFSET +
	    (ARB_REG_SIZE * index), value);
}

void *		qat_alloc_mem(size_t);
void		qat_free_mem(void *);
void		qat_free_dmamem(struct qat_softc *, struct qat_dmamem *);
int		qat_alloc_dmamem(struct qat_softc *, struct qat_dmamem *, int,
		    bus_size_t, bus_size_t);

int		qat_etr_setup_ring(struct qat_softc *, int, uint32_t, uint32_t,
		    uint32_t, qat_cb_t, void *, const char *,
		    struct qat_ring **);
int		qat_etr_put_msg(struct qat_softc *, struct qat_ring *,
		    uint32_t *);

void		qat_memcpy_htobe64(void *, const void *, size_t);
void		qat_memcpy_htobe32(void *, const void *, size_t);
void		qat_memcpy_htobe(void *, const void *, size_t, uint32_t);
void		qat_crypto_gmac_precompute(const struct qat_crypto_desc *,
		    const uint8_t *key, int klen,
		    const struct qat_sym_hash_def *, uint8_t *);
void		qat_crypto_hmac_precompute(const struct qat_crypto_desc *,
		    const uint8_t *, int, const struct qat_sym_hash_def *,
		    uint8_t *, uint8_t *);
uint16_t	qat_crypto_load_cipher_session(const struct qat_crypto_desc *,
		    const struct qat_session *);
uint16_t	qat_crypto_load_auth_session(const struct qat_crypto_desc *,
		    const struct qat_session *,
		    struct qat_sym_hash_def const **);

#endif
