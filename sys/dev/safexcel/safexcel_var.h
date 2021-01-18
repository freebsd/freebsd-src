/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Rubicon Communications, LLC (Netgate)
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _SAFEXCEL_VAR_H_
#define	_SAFEXCEL_VAR_H_

#include <sys/counter.h>

#define	SAFEXCEL_MAX_RINGS			4
#define	SAFEXCEL_MAX_BATCH_SIZE			64
#define	SAFEXCEL_MAX_FRAGMENTS			64
#define	SAFEXCEL_MAX_IV_LEN			16
#define	SAFEXCEL_MAX_REQUEST_SIZE		65535

#define	SAFEXCEL_RING_SIZE			512
#define	SAFEXCEL_MAX_ITOKENS			4
#define	SAFEXCEL_MAX_ATOKENS			16
#define	SAFEXCEL_FETCH_COUNT			1
#define	SAFEXCEL_MAX_KEY_LEN			32
#define	SAFEXCEL_MAX_RING_AIC			14

/*
 * Context Record format.
 *
 * In this driver the context control words are always set in the control data.
 * This helps optimize fetching of the context record.  This is configured by
 * setting SAFEXCEL_OPTION_CTX_CTRL_IN_CMD.
 */
struct safexcel_context_record {
	uint32_t control0;	/* Unused. */
	uint32_t control1;	/* Unused. */
	uint32_t data[40];	/* Key material. */
} __packed;

/* Processing Engine Control Data format. */
struct safexcel_control_data {
	uint32_t packet_length	: 17;
	uint32_t options	: 13;
	uint32_t type		: 2;

	uint16_t application_id;
	uint16_t rsvd;

	uint32_t context_lo;
	uint32_t context_hi;

	uint32_t control0;
	uint32_t control1;

	/* Inline instructions or IV. */
	uint32_t token[SAFEXCEL_MAX_ITOKENS];
} __packed;

/*
 * Basic Command Descriptor.
 *
 * The Processing Engine and driver cooperate to maintain a set of command
 * rings, representing outstanding crypto operation requests.  Each descriptor
 * corresponds to an input data segment, and thus a single crypto(9) request may
 * span several contiguous command descriptors.
 *
 * The first command descriptor for a request stores the input token, which
 * encodes data specific to the requested operation, such as the encryption
 * mode.  For some operations data is passed outside the descriptor, in a
 * context record (e.g., encryption keys), or in an "additional token data"
 * region (e.g., instructions).
 */
struct safexcel_cmd_descr {
	uint32_t particle_size	: 17;
	uint32_t rsvd0		: 5;
	uint32_t last_seg	: 1;
	uint32_t first_seg	: 1;
	uint32_t additional_cdata_size : 8;
	uint32_t rsvd1;

	uint32_t data_lo;
	uint32_t data_hi;

	uint32_t atok_lo;
	uint32_t atok_hi;

	struct safexcel_control_data control_data;
} __packed;

/* Context control word 0 fields. */
#define	SAFEXCEL_CONTROL0_TYPE_NULL_OUT		0x0
#define	SAFEXCEL_CONTROL0_TYPE_NULL_IN		0x1
#define	SAFEXCEL_CONTROL0_TYPE_HASH_OUT		0x2
#define	SAFEXCEL_CONTROL0_TYPE_HASH_IN		0x3
#define	SAFEXCEL_CONTROL0_TYPE_CRYPTO_OUT	0x4
#define	SAFEXCEL_CONTROL0_TYPE_CRYPTO_IN	0x5
#define	SAFEXCEL_CONTROL0_TYPE_ENCRYPT_HASH_OUT	0x6
#define	SAFEXCEL_CONTROL0_TYPE_DECRYPT_HASH_IN	0x7
#define	SAFEXCEL_CONTROL0_TYPE_HASH_ENCRYPT_OUT	0xe
#define	SAFEXCEL_CONTROL0_TYPE_HASH_DECRYPT_IN	0xf
#define	SAFEXCEL_CONTROL0_RESTART_HASH		(1 << 4)
#define	SAFEXCEL_CONTROL0_NO_FINISH_HASH	(1 << 5)
#define	SAFEXCEL_CONTROL0_SIZE(n)		(((n) & 0xff) << 8)
#define	SAFEXCEL_CONTROL0_KEY_EN		(1 << 16)
#define	SAFEXCEL_CONTROL0_CRYPTO_ALG_AES128	(0x5 << 17)
#define	SAFEXCEL_CONTROL0_CRYPTO_ALG_AES192	(0x6 << 17)
#define	SAFEXCEL_CONTROL0_CRYPTO_ALG_AES256	(0x7 << 17)
#define	SAFEXCEL_CONTROL0_DIGEST_PRECOMPUTED	(0x1 << 21)
#define	SAFEXCEL_CONTROL0_DIGEST_CCM		(0x2 << 21)
#define	SAFEXCEL_CONTROL0_DIGEST_GMAC		(0x2 << 21)
#define	SAFEXCEL_CONTROL0_DIGEST_HMAC		(0x3 << 21)
#define	SAFEXCEL_CONTROL0_HASH_ALG_SHA1		(0x2 << 23)
#define	SAFEXCEL_CONTROL0_HASH_ALG_SHA224	(0x4 << 23)
#define	SAFEXCEL_CONTROL0_HASH_ALG_SHA256	(0x3 << 23)
#define	SAFEXCEL_CONTROL0_HASH_ALG_SHA384	(0x6 << 23)
#define	SAFEXCEL_CONTROL0_HASH_ALG_SHA512	(0x5 << 23)
#define	SAFEXCEL_CONTROL0_HASH_ALG_XCBC128	(0x1 << 23)
#define	SAFEXCEL_CONTROL0_HASH_ALG_XCBC192	(0x2 << 23)
#define	SAFEXCEL_CONTROL0_HASH_ALG_XCBC256	(0x3 << 23)
#define	SAFEXCEL_CONTROL0_HASH_ALG_GHASH	(0x4 << 23)
#define	SAFEXCEL_CONTROL0_INV_FR		(0x5 << 24)
#define	SAFEXCEL_CONTROL0_INV_TR		(0x6 << 24)

/* Context control word 1 fields. */
#define	SAFEXCEL_CONTROL1_CRYPTO_MODE_ECB	0x0
#define	SAFEXCEL_CONTROL1_CRYPTO_MODE_CBC	0x1
#define	SAFEXCEL_CONTROL1_CRYPTO_MODE_ICM	0x3
#define	SAFEXCEL_CONTROL1_CRYPTO_MODE_OFB	0x4
#define	SAFEXCEL_CONTROL1_CRYPTO_MODE_CFB128	0x5
#define	SAFEXCEL_CONTROL1_CRYPTO_MODE_CTR	0x6
#define	SAFEXCEL_CONTROL1_CRYPTO_MODE_XTS	0x7
#define	SAFEXCEL_CONTROL1_CRYPTO_MODE_CCM	(0x6 | (1 << 17))
#define	SAFEXCEL_CONTROL1_CRYPTO_MODE_GCM	(0x6 | (1 << 17))
#define	SAFEXCEL_CONTROL1_IV0			(1u << 5)
#define	SAFEXCEL_CONTROL1_IV1			(1u << 6)
#define	SAFEXCEL_CONTROL1_IV2			(1u << 7)
#define	SAFEXCEL_CONTROL1_IV3			(1u << 8)
#define	SAFEXCEL_CONTROL1_DIGEST_CNT		(1u << 9)
#define	SAFEXCEL_CONTROL1_COUNTER_MODE		(1u << 10)
#define	SAFEXCEL_CONTROL1_ENCRYPT_HASH_RES	(1u << 17)
#define	SAFEXCEL_CONTROL1_HASH_STORE		(1u << 19)

/* Control options. */
#define	SAFEXCEL_OPTION_IP			(1u << 0) /* must be set */
#define	SAFEXCEL_OPTION_CP			(1u << 1) /* 64-bit ctx addr */
#define	SAFEXCEL_OPTION_RC_AUTO			(2u << 3) /* auto ctx reuse */
#define	SAFEXCEL_OPTION_CTX_CTRL_IN_CMD		(1u << 8) /* ctx ctrl */
#define	SAFEXCEL_OPTION_4_TOKEN_IV_CMD		0xe00     /* IV in bypass */

struct safexcel_res_data {
	uint32_t packet_length	: 17;
	uint32_t error_code	: 15;

	uint32_t bypass_length	: 4;
	uint32_t e15		: 1;
	uint32_t rsvd0		: 16;
	uint32_t hash_bytes	: 1;
	uint32_t hash_length	: 6;
	uint32_t generic_bytes	: 1;
	uint32_t checksum	: 1;
	uint32_t next_header	: 1;
	uint32_t length		: 1;

	uint16_t application_id;
	uint16_t rsvd1;

	uint32_t rsvd2;
};

/* Basic Result Descriptor format */
struct safexcel_res_descr {
	uint32_t particle_size	: 17;
	uint32_t rsvd0		: 3;
	uint32_t descriptor_overflow : 1;
	uint32_t buffer_overflow : 1;
	uint32_t last_seg	: 1;
	uint32_t first_seg	: 1;
	uint32_t result_size	: 8;

	uint32_t rsvd1;

	uint32_t data_lo;
	uint32_t data_hi;

	struct safexcel_res_data result_data;
} __packed;

/* Result data error codes. */
#define	SAFEXCEL_RESULT_ERR_PACKET_LEN		(1u << 0)
#define	SAFEXCEL_RESULT_ERR_TOKEN_ERROR		(1u << 1)
#define	SAFEXCEL_RESULT_ERR_BYPASS		(1u << 2)
#define	SAFEXCEL_RESULT_ERR_CRYPTO_BLOCK_SIZE	(1u << 3)
#define	SAFEXCEL_RESULT_ERR_HASH_BLOCK_SIZE	(1u << 4)
#define	SAFEXCEL_RESULT_ERR_INVALID_CMD		(1u << 5)
#define	SAFEXCEL_RESULT_ERR_PROHIBITED_ALGO	(1u << 6)
#define	SAFEXCEL_RESULT_ERR_HASH_INPUT_OVERFLOW	(1u << 7)
#define	SAFEXCEL_RESULT_ERR_TTL_UNDERFLOW	(1u << 8)
#define	SAFEXCEL_RESULT_ERR_AUTH_FAILED		(1u << 9)
#define	SAFEXCEL_RESULT_ERR_SEQNO_CHECK_FAILED	(1u << 10)
#define	SAFEXCEL_RESULT_ERR_SPI_CHECK		(1u << 11)
#define	SAFEXCEL_RESULT_ERR_CHECKSUM		(1u << 12)
#define	SAFEXCEL_RESULT_ERR_PAD_VERIFICATION	(1u << 13)
#define	SAFEXCEL_RESULT_ERR_TIMEOUT		(1u << 14)
#define	SAFEXCEL_RESULT_ERR_OUTPUT_DMA		(1u << 15)

/*
 * The EIP-96 (crypto transform engine) is programmed using a set of
 * data processing instructions with the encodings defined below.
 */
struct safexcel_instr {
	uint32_t length : 17;		/* bytes to be processed */
	uint32_t status : 2;		/* stream status */
	uint32_t instructions : 9;
	uint32_t opcode : 4;
} __packed;

/* Type 1, operational data instructions. */
#define	SAFEXCEL_INSTR_OPCODE_DIRECTION		0x0
#define	SAFEXCEL_INSTR_OPCODE_PRE_CHECKSUM	0x1
#define	SAFEXCEL_INSTR_OPCODE_INSERT		0x2
#define	SAFEXCEL_INSTR_OPCODE_INSERT_CTX	0x9
#define	SAFEXCEL_INSTR_OPCODE_REPLACE		0x3
#define	SAFEXCEL_INSTR_OPCODE_RETRIEVE		0x4
#define	SAFEXCEL_INSTR_OPCODE_MUTE		0x5
/* Type 2, IP header instructions. */
#define	SAFEXCEL_INSTR_OPCODE_IPV4		0x7
#define	SAFEXCEL_INSTR_OPCODE_IPV4_CHECKSUM	0x6
#define	SAFEXCEL_INSTR_OPCODE_IPV6		0x8
/* Type 3, postprocessing instructions. */
#define	SAFEXCEL_INSTR_OPCODE_INSERT_REMOVE_RESULT 0xa
#define	SAFEXCEL_INSTR_OPCODE_REPLACE_BYTE	0xb
/* Type 4, result instructions. */
#define	SAFEXCEL_INSTR_OPCODE_VERIFY_FIELDS	0xd
/* Type 5, context control instructions. */
#define	SAFEXCEL_INSTR_OPCODE_CONTEXT_ACCESS	0xe
/* Type 6, context control instructions. */
#define	SAFEXCEL_INSTR_OPCODE_BYPASS_TOKEN_DATA	0xf

/* Status bits for type 1 and 2 instructions. */
#define	SAFEXCEL_INSTR_STATUS_LAST_HASH		(1u << 0)
#define	SAFEXCEL_INSTR_STATUS_LAST_PACKET	(1u << 1)
/* Status bits for type 3 instructions. */
#define	SAFEXCEL_INSTR_STATUS_NO_CKSUM_MOD	(1u << 0)

/* Instruction-dependent flags. */
#define	SAFEXCEL_INSTR_INSERT_HASH_DIGEST	0x1c
#define	SAFEXCEL_INSTR_INSERT_IMMEDIATE		0x1b
#define	SAFEXCEL_INSTR_DEST_OUTPUT		(1u << 5)
#define	SAFEXCEL_INSTR_DEST_HASH		(1u << 6)
#define	SAFEXCEL_INSTR_DEST_CRYPTO		(1u << 7)
#define	SAFEXCEL_INSTR_INS_LAST			(1u << 8)

#define	SAFEXCEL_INSTR_VERIFY_HASH		(1u << 16)
#define	SAFEXCEL_INSTR_VERIFY_PADDING		(1u << 5)

#define	SAFEXCEL_TOKEN_TYPE_BYPASS	0x0
#define	SAFEXCEL_TOKEN_TYPE_AUTONOMOUS	0x3

#define	SAFEXCEL_CONTEXT_SMALL		0x2
#define	SAFEXCEL_CONTEXT_LARGE		0x3

struct safexcel_reg_offsets {
	uint32_t hia_aic;
	uint32_t hia_aic_g;
	uint32_t hia_aic_r;
	uint32_t hia_aic_xdr;
	uint32_t hia_dfe;
	uint32_t hia_dfe_thr;
	uint32_t hia_dse;
	uint32_t hia_dse_thr;
	uint32_t hia_gen_cfg;
	uint32_t pe;
};

struct safexcel_config {
	uint32_t	hdw;		/* Host interface Data Width. */
	uint32_t	aic_rings;	/* Number of AIC rings. */
	uint32_t	pes;		/* Number of PEs. */
	uint32_t	rings;		/* Number of rings. */

	uint32_t	cd_size;	/* CDR descriptor size. */
	uint32_t	cd_offset;	/* CDR offset (size + alignment). */

	uint32_t	rd_size;	/* RDR descriptor size. */
	uint32_t	rd_offset;	/* RDR offset. */

	uint32_t	atok_offset;	/* Additional token offset. */

	uint32_t	caps;		/* Device capabilities. */
};

#define	SAFEXCEL_DPRINTF(sc, lvl, ...) do {				\
	if ((sc)->sc_debug >= (lvl))					\
		device_printf((sc)->sc_dev, __VA_ARGS__);		\
} while (0)

struct safexcel_dma_mem {
	caddr_t		vaddr;
	bus_addr_t	paddr;
	bus_dma_tag_t	tag;
	bus_dmamap_t	map;
};

struct safexcel_cmd_descr_ring {
	struct safexcel_dma_mem		dma;
	struct safexcel_cmd_descr	*desc;
	int				write;
	int				read;
};

struct safexcel_res_descr_ring {
	struct safexcel_dma_mem		dma;
	struct safexcel_res_descr	*desc;
	int				write;
	int				read;
};

struct safexcel_context_template {
	struct safexcel_context_record	ctx;
	int				len;
};

struct safexcel_session {
	crypto_session_t	cses;
	uint32_t		alg;		/* cipher algorithm */
	uint32_t		digest;		/* digest type */
	uint32_t		hash;		/* hash algorithm */
	uint32_t		mode;		/* cipher mode of operation */
	unsigned int		digestlen;	/* digest length */
	unsigned int		statelen;	/* HMAC hash state length */

	struct safexcel_context_template encctx, decctx;
};

struct safexcel_softc;

struct safexcel_request {
	STAILQ_ENTRY(safexcel_request)	link;
	bool				dmap_loaded;
	int				ringidx;
	bus_dmamap_t			dmap;
	int				error;
	int				cdescs, rdescs;
	uint8_t				iv[SAFEXCEL_MAX_IV_LEN];
	struct safexcel_cmd_descr	*cdesc;
	struct safexcel_dma_mem		ctx;
	struct safexcel_session		*sess;
	struct cryptop			*crp;
	struct safexcel_softc		*sc;
};

struct safexcel_ring {
	struct mtx			mtx;
	struct sglist			*cmd_data;
	struct safexcel_cmd_descr_ring	cdr;
	struct sglist			*res_data;
	struct safexcel_res_descr_ring	rdr;

	/* Shadows the command descriptor ring. */
	struct safexcel_request		requests[SAFEXCEL_RING_SIZE];

	/* Count of requests pending submission. */
	int				pending;
	int				pending_cdesc, pending_rdesc;

	/* Count of outstanding requests. */
	int				queued;

	/* Requests were deferred due to a resource shortage. */
	int				blocked;

	struct safexcel_dma_mem		dma_atok;
	bus_dma_tag_t   		data_dtag;

	char				lockname[32];
};

struct safexcel_intr_handle {
	struct safexcel_softc		*sc;
	void				*handle;
	int				ring;
};

struct safexcel_softc {
	device_t			sc_dev;
	uint32_t			sc_type;	/* EIP-97 or 197 */
	int				sc_debug;

	struct resource			*sc_res;
	struct resource			*sc_intr[SAFEXCEL_MAX_RINGS];
	struct safexcel_intr_handle	sc_ih[SAFEXCEL_MAX_RINGS];

	counter_u64_t			sc_req_alloc_failures;
	counter_u64_t			sc_cdesc_alloc_failures;
	counter_u64_t			sc_rdesc_alloc_failures;

	struct safexcel_ring 		sc_ring[SAFEXCEL_MAX_RINGS];

	int32_t				sc_cid;
	struct safexcel_reg_offsets 	sc_offsets;
	struct safexcel_config		sc_config;
};

#define	SAFEXCEL_WRITE(sc, off, val)	bus_write_4((sc)->sc_res, (off), (val))
#define	SAFEXCEL_READ(sc, off)		bus_read_4((sc)->sc_res, (off))

#define	SAFEXCEL_ADDR_LO(addr)		((uint64_t)(addr) & 0xffffffffu)
#define	SAFEXCEL_ADDR_HI(addr)		(((uint64_t)(addr) >> 32) & 0xffffffffu)

#endif /* _SAFEXCEL_VAR_H_ */
