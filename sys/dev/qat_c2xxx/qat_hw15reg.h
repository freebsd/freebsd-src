/* SPDX-License-Identifier: BSD-2-Clause AND BSD-3-Clause */
/*	$NetBSD: qat_hw15reg.h,v 1.1 2019/11/20 09:37:46 hikaru Exp $	*/

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
 *   Copyright(c) 2007-2013 Intel Corporation. All rights reserved.
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


#ifndef _DEV_PCI_QAT_HW15REG_H_
#define _DEV_PCI_QAT_HW15REG_H_

/* Default message size in bytes */
#define FW_REQ_DEFAULT_SZ_HW15		64
#define FW_RESP_DEFAULT_SZ_HW15		64

#define ADMIN_RING_SIZE		256
#define RING_NUM_ADMIN_TX	0
#define RING_NUM_ADMIN_RX	1

/* -------------------------------------------------------------------------- */
/* accel */

#define ARCH_IF_FLAGS_VALID_FLAG		__BIT(7)
#define ARCH_IF_FLAGS_RESP_RING_TYPE		__BITS(4, 3)
#define  ARCH_IF_FLAGS_RESP_RING_TYPE_SHIFT	3
#define  ARCH_IF_FLAGS_RESP_RING_TYPE_SCRATCH	(0 << ARCH_IF_FLAGS_RESP_RING_TYPE_SHIFT)
#define  ARCH_IF_FLAGS_RESP_RING_TYPE_NN	(1 << ARCH_IF_FLAGS_RESP_RING_TYPE_SHIFT)
#define  ARCH_IF_FLAGS_RESP_RING_TYPE_ET	(2 << ARCH_IF_FLAGS_RESP_RING_TYPE_SHIFT)
#define ARCH_IF_FLAGS_RESP_TYPE			__BITS(2, 0)
#define  ARCH_IF_FLAGS_RESP_TYPE_SHIFT		0
#define   ARCH_IF_FLAGS_RESP_TYPE_A		(0 << ARCH_IF_FLAGS_RESP_TYPE_SHIFT)
#define   ARCH_IF_FLAGS_RESP_TYPE_B		(1 << ARCH_IF_FLAGS_RESP_TYPE_SHIFT)
#define   ARCH_IF_FLAGS_RESP_TYPE_C		(2 << ARCH_IF_FLAGS_RESP_TYPE_SHIFT)
#define   ARCH_IF_FLAGS_RESP_TYPE_S		(3 << ARCH_IF_FLAGS_RESP_TYPE_SHIFT)

enum arch_if_req {
	ARCH_IF_REQ_NULL,		/* NULL request type */

	/* QAT-AE Service Request Type IDs - 01 to 20 */
	ARCH_IF_REQ_QAT_FW_INIT,	/* QAT-FW Initialization Request */
	ARCH_IF_REQ_QAT_FW_ADMIN,	/* QAT-FW Administration Request */
	ARCH_IF_REQ_QAT_FW_PKE,		/* QAT-FW PKE Request */
	ARCH_IF_REQ_QAT_FW_LA,		/* QAT-FW Lookaside Request */
	ARCH_IF_REQ_QAT_FW_IPSEC,	/* QAT-FW IPSec Request */
	ARCH_IF_REQ_QAT_FW_SSL,		/* QAT-FW SSL Request */
	ARCH_IF_REQ_QAT_FW_DMA,		/* QAT-FW DMA Request */
	ARCH_IF_REQ_QAT_FW_STORAGE,	/* QAT-FW Storage Request */
	ARCH_IF_REQ_QAT_FW_COMPRESS,	/* QAT-FW Compression Request */
	ARCH_IF_REQ_QAT_FW_PATMATCH,	/* QAT-FW Pattern Matching Request */

	/* IP Service (Range Match and Exception) Blocks Request Type IDs 21 - 30 */
	ARCH_IF_REQ_RM_FLOW_MISS = 21,	/* RM flow miss request */
	ARCH_IF_REQ_RM_FLOW_TIMER_EXP,	/* RM flow timer exp Request */
	ARCH_IF_REQ_IP_SERVICES_RFC_LOOKUP_UPDATE, /* RFC Lookup request */
	ARCH_IF_REQ_IP_SERVICES_CONFIG_UPDATE,	/* Config Update request */
	ARCH_IF_REQ_IP_SERVICES_FCT_CONFIG,	/* FCT Config request */
	ARCH_IF_REQ_IP_SERVICES_NEXT_HOP_TIMER_EXPIRY, /* NH Timer expiry request */
	ARCH_IF_REQ_IP_SERVICES_EXCEPTION,	/* Exception processign request */
	ARCH_IF_REQ_IP_SERVICES_STACK_DRIVER,	/* Send to SD request */
	ARCH_IF_REQ_IP_SERVICES_ACTION_HANDLER,	/* Send to AH request */
	ARCH_IF_REQ_IP_SERVICES_EVENT_HANDLER,	/* Send to EH request */
	ARCH_IF_REQ_DELIMITER			/* End delimiter */
};

struct arch_if_req_hdr {
	uint8_t resp_dest_id;
	/* Opaque identifier passed from the request to response to allow
	 * response handler perform any further processing */
	uint8_t resp_pipe_id;
	/* Response pipe to write the response associated with this request to */
	uint8_t req_type;
	/* Definition of the service described by the request */
	uint8_t flags;
	/* Request and response control flags */
};

struct arch_if_resp_hdr {
	uint8_t dest_id;
	/* Opaque identifier passed from the request to response to allow
	 * response handler perform any further processing */
	uint8_t serv_id;
	/* Definition of the service id generating the response */
	uint8_t resp_type;
	/* Definition of the service described by the request */
	uint8_t flags;
	/* Request and response control flags */
};

struct fw_comn_req_hdr {
	struct arch_if_req_hdr arch_if;
	/* Common arch fields used by all ICP interface requests. Remaining
	 * fields are specific to the common QAT FW service. */
	uint16_t comn_req_flags;
	/* Flags used to describe common processing required by the request and
	 * the meaning of parameters in it i.e. differentiating between a buffer
	 * descriptor and a flat buffer pointer in the source (src) and destination
	 * (dest) data address fields. Full definition of the fields is given
	 * below */
	uint8_t content_desc_params_sz;
	/* Size of the content descriptor parameters in quad words. These
	 * parameters describe the session setup configuration info for the
	 * slices that this request relies upon i.e. the configuration word and
	 * cipher key needed by the cipher slice if there is a request for cipher
	 * processing. The format of the parameters are contained in icp_qat_hw.h
	 * and vary depending on the algorithm and mode being used. It is the
	 * clients responsibility to ensure this structure is correctly packed */
	uint8_t content_desc_hdr_sz;
	/* Size of the content descriptor header in quad words. This information
	 * is read into the QAT AE xfr registers */
	uint64_t content_desc_addr;
	/* Address of the content descriptor containing both the content header
	 * the size of which is defined by content_desc_hdr_sz followed by the
	 * content parameters whose size is described bycontent_desc_params_sz
	 */
};

struct fw_comn_req_mid {
	uint64_t opaque_data;
	/* Opaque data passed unmodified from the request to response messages
	 * by firmware (fw) */
	uint64_t src_data_addr;
	/* Generic definition of the source data supplied to the QAT AE. The
	 * common flags are used to further describe the attributes of this
	 * field */
	uint64_t dest_data_addr;
	/* Generic definition of the destination data supplied to the QAT AE.
	 * The common flags are used to further describe the attributes of this
	 * field */
};

union fw_comn_req_ftr {
	uint64_t next_request_addr;
	/* Overloaded field, for stateful requests, this field is the pointer to 
	   next request descriptor */
	struct {
		uint32_t src_length;
		/* Length of source flat buffer incase src buffer type is flat */
		uint32_t dst_length;
		/* Length of source flat buffer incase dst buffer type is flat */
	} s;
};

union fw_comn_error {
	struct {
		uint8_t resrvd;		/* 8 bit reserved field */
		uint8_t comn_err_code;	/**< 8 bit common error code */
	} s;
	/* Structure which is used for non-compression responses */

	struct {
		uint8_t xlat_err_code;	/* 8 bit translator error field */
		uint8_t cmp_err_code;	/* 8 bit compression error field */
	} s1;
	/* Structure which is used for compression responses */
};

struct fw_comn_resp_hdr {
	struct arch_if_resp_hdr arch_if;
	/* Common arch fields used by all ICP interface response messages. The
	 * remaining fields are specific to the QAT FW */
	union fw_comn_error comn_error;
	/* This field is overloaded to allow for one 8 bit common error field
	 * or two 8 bit error fields from compression and translator  */
	uint8_t comn_status;
	/* Status field which specifies which slice(s) report an error */
	uint8_t serv_cmd_id;
	/* For services that define multiple commands this field represents the
	 * command. If only 1 command is supported then this field will be 0 */
	uint64_t opaque_data;
	/* Opaque data passed from the request to the response message */
};


#define RING_MASK_TABLE_ENTRY_LOG_SZ			(5)

#define FW_INIT_RING_MASK_SET(table, id)			\
		table->firt_ring_mask[id >> RING_MASK_TABLE_ENTRY_LOG_SZ] =\
		table->firt_ring_mask[id >> RING_MASK_TABLE_ENTRY_LOG_SZ] | \
		(1 << (id & 0x1f))

struct fw_init_ring_params {
	uint8_t firp_curr_weight;	/* Current ring weight (working copy),
					 * has to be equal to init_weight */
	uint8_t firp_init_weight;	/* Initial ring weight: -1 ... 0
					 * -1 is equal to FF, -2 is equal to FE,
					 * the weighting uses negative logic
					 * where FF means poll the ring once,
					 * -2 is poll the ring twice,
					 * 0 is poll the ring 255 times */
	uint8_t firp_ring_pvl;		/* Ring Privilege Level. */
	uint8_t firp_reserved;		/* Reserved field which must be set
					 * to 0 by the client */
};

#define INIT_RING_TABLE_SZ		128
#define INIT_RING_TABLE_LW_SZ		4

struct fw_init_ring_table {
	struct fw_init_ring_params firt_bulk_rings[INIT_RING_TABLE_SZ];
					/* array of ring parameters */
	uint32_t firt_ring_mask[INIT_RING_TABLE_LW_SZ];
					/* Structure to hold the bit masks for
					 * 128 rings. */
};

struct fw_init_set_ae_info_hdr {
	uint16_t init_slice_mask;	/* Init time flags to set the ownership of the slices */
	uint16_t resrvd;		/* Reserved field and must be set to 0 by the client */
	uint8_t init_qat_id;		/* Init time qat id described in the request */
	uint8_t init_ring_cluster_id;	/* Init time ring cluster Id */
	uint8_t init_trgt_id;		/* Init time target AE id described in the request */
	uint8_t init_cmd_id;		/* Init time command that is described in the request */
};

struct fw_init_set_ae_info {
	uint64_t init_shram_mask;	/* Init time shram mask to set the page ownership in page pool of AE*/
	uint64_t resrvd;		/* Reserved field and must be set to 0 by the client */
};

struct fw_init_set_ring_info_hdr {
	uint32_t resrvd;		/* Reserved field and must be set to 0 by the client */
	uint16_t init_ring_tbl_sz;	/* Init time information to state size of the ring table */
	uint8_t init_trgt_id;		/* Init time target AE id described in the request */
	uint8_t init_cmd_id;		/* Init time command that is described in the request */
};

struct fw_init_set_ring_info {
	uint64_t init_ring_table_ptr;	/* Pointer to weighting information for 128 rings  */
	uint64_t resrvd;		/* Reserved field and must be set to 0 by the client */
};

struct fw_init_trng_hdr {
	uint32_t resrvd;	/* Reserved field and must be set to 0 by the client */
	union {
		uint8_t resrvd;	/* Reserved field set to if cmd type is trng disable */
		uint8_t init_trng_cfg_sz;	/* Size of the trng config word in QW*/
	} u;
	uint8_t resrvd1;	/* Reserved field and must be set to 0 by the client */
	uint8_t init_trgt_id;	/* Init time target AE id described in the request */
	uint8_t init_cmd_id;	/* Init time command that is described in the request */
};

struct fw_init_trng {
	union {
		uint64_t resrvd;	/* Reserved field set to 0 if cmd type is trng disable */
		uint64_t init_trng_cfg_ptr;	/* Pointer to TRNG Slice config word*/
	} u;
	uint64_t resrvd;		/* Reserved field and must be set to 0 by the client */
};

struct fw_init_req {
	struct fw_comn_req_hdr comn_hdr;	/* Common request header */
	union {
		struct fw_init_set_ae_info_hdr set_ae_info;
		/* INIT SET_AE_INFO request header structure */
		struct fw_init_set_ring_info_hdr set_ring_info;
		/* INIT SET_RING_INFO request header structure */
		struct fw_init_trng_hdr init_trng;
		/* INIT TRNG ENABLE/DISABLE request header structure */
	} u;
	struct fw_comn_req_mid comn_mid;	/* Common request middle section */
	union {
		struct fw_init_set_ae_info set_ae_info;
		/* INIT SET_AE_INFO request data structure */
		struct fw_init_set_ring_info set_ring_info;
		/* INIT SET_RING_INFO request data structure */
		struct fw_init_trng init_trng;
		/* INIT TRNG ENABLE/DISABLE request data structure */
	} u1;
};

enum fw_init_cmd_id {
	FW_INIT_CMD_SET_AE_INFO,	/* Setup AE Info command type */
	FW_INIT_CMD_SET_RING_INFO,	/* Setup Ring Info command type */
	FW_INIT_CMD_TRNG_ENABLE,	/* TRNG Enable command type */
	FW_INIT_CMD_TRNG_DISABLE,	/* TRNG Disable command type */
	FW_INIT_CMD_DELIMITER		/* Delimiter type */
};

struct fw_init_resp {
	struct fw_comn_resp_hdr comn_resp;	/* Common interface response */
	uint8_t resrvd[64 - sizeof(struct fw_comn_resp_hdr)];
	/* XXX FW_RESP_DEFAULT_SZ_HW15 */
	/* Reserved padding out to the default response size */
};

/* -------------------------------------------------------------------------- */
/* look aside */

#define COMN_REQ_ORD				UINT16_C(0x8000)
#define  COMN_REQ_ORD_SHIFT			15
#define   COMN_REQ_ORD_NONE			(0 << COMN_REQ_ORD_SHIFT)
#define   COMN_REQ_ORD_STRICT			(1 << COMN_REQ_ORD_SHIFT)
#define COMN_REQ_PTR_TYPE			UINT16_C(0x4000)
#define  COMN_REQ_PTR_TYPE_SHIFT		14
#define   COMN_REQ_PTR_TYPE_FLAT		(0 << COMN_REQ_PTR_TYPE_SHIFT)
#define   COMN_REQ_PTR_TYPE_SGL			(1 << COMN_REQ_PTR_TYPE_SHIFT)
#define COMN_REQ_RESERVED			UINT16_C(0x2000)
#define COMN_REQ_SHRAM_INIT			UINT16_C(0x1000)
#define  COMN_REQ_SHRAM_INIT_SHIFT		12
#define  COMN_REQ_SHRAM_INIT_REQUIRED		(1 << COMN_REQ_SHRAM_INIT_SHIFT)
#define COMN_REQ_REGEX_SLICE			UINT16_C(0x0800)
#define  COMN_REQ_REGEX_SLICE_SHIFT		11
#define   COMN_REQ_REGEX_SLICE_REQUIRED		(1 << COMN_REQ_REGEX_SLICE_SHIFT)
#define COMN_REQ_XLAT_SLICE			UINT16_C(0x0400)
#define  COMN_REQ_XLAT_SLICE_SHIFT		10
#define   COMN_REQ_XLAT_SLICE_REQUIRED		(1 << COMN_REQ_XLAT_SLICE_SHIFT)
#define COMN_REQ_CPR_SLICE			UINT16_C(0x0200)
#define  COMN_REQ_CPR_SLICE_SHIFT		9
#define   COMN_REQ_CPR_SLICE_REQUIRED		(1 << COMN_REQ_CPR_SLICE_SHIFT)
#define COMN_REQ_BULK_SLICE			UINT16_C(0x0100)
#define  COMN_REQ_BULK_SLICE_SHIFT		8
#define   COMN_REQ_BULK_SLICE_REQUIRED		(1 << COMN_REQ_BULK_SLICE_SHIFT)
#define COMN_REQ_STORAGE_SLICE			UINT16_C(0x0080)
#define  COMN_REQ_STORAGE_SLICE_SHIFT		7
#define   COMN_REQ_STORAGE_SLICE_REQUIRED	(1 << COMN_REQ_STORAGE_SLICE_SHIFT)
#define COMN_REQ_RND_SLICE			UINT16_C(0x0040)
#define  COMN_REQ_RND_SLICE_SHIFT		6
#define   COMN_REQ_RND_SLICE_REQUIRED		(1 << COMN_REQ_RND_SLICE_SHIFT)
#define COMN_REQ_PKE1_SLICE			UINT16_C(0x0020)
#define  COMN_REQ_PKE1_SLICE_SHIFT		5
#define   COMN_REQ_PKE1_SLICE_REQUIRED		(1 << COMN_REQ_PKE1_SLICE_SHIFT)
#define COMN_REQ_PKE0_SLICE			UINT16_C(0x0010)
#define  COMN_REQ_PKE0_SLICE_SHIFT		4
#define   COMN_REQ_PKE0_SLICE_REQUIRED		(1 << COMN_REQ_PKE0_SLICE_SHIFT)
#define COMN_REQ_AUTH1_SLICE			UINT16_C(0x0008)
#define  COMN_REQ_AUTH1_SLICE_SHIFT		3
#define   COMN_REQ_AUTH1_SLICE_REQUIRED		(1 << COMN_REQ_AUTH1_SLICE_SHIFT)
#define COMN_REQ_AUTH0_SLICE			UINT16_C(0x0004)
#define  COMN_REQ_AUTH0_SLICE_SHIFT		2
#define   COMN_REQ_AUTH0_SLICE_REQUIRED		(1 << COMN_REQ_AUTH0_SLICE_SHIFT)
#define COMN_REQ_CIPHER1_SLICE			UINT16_C(0x0002)
#define  COMN_REQ_CIPHER1_SLICE_SHIFT		1
#define   COMN_REQ_CIPHER1_SLICE_REQUIRED	(1 << COMN_REQ_CIPHER1_SLICE_SHIFT)
#define COMN_REQ_CIPHER0_SLICE			UINT16_C(0x0001)
#define  COMN_REQ_CIPHER0_SLICE_SHIFT		0
#define   COMN_REQ_CIPHER0_SLICE_REQUIRED	(1 << COMN_REQ_CIPHER0_SLICE_SHIFT)

#define COMN_REQ_CY0_ONLY(shram)			\
		COMN_REQ_ORD_STRICT |			\
		COMN_REQ_PTR_TYPE_FLAT |		\
		(shram) |				\
		COMN_REQ_RND_SLICE_REQUIRED | 		\
		COMN_REQ_PKE0_SLICE_REQUIRED |		\
		COMN_REQ_AUTH0_SLICE_REQUIRED |		\
		COMN_REQ_CIPHER0_SLICE_REQUIRED;
#define COMN_REQ_CY1_ONLY(shram)			\
		COMN_REQ_ORD_STRICT |			\
		COMN_REQ_PTR_TYPE_FLAT |		\
		(shram) |				\
		COMN_REQ_PKE1_SLICE_REQUIRED |		\
		COMN_REQ_AUTH1_SLICE_REQUIRED |		\
		COMN_REQ_CIPHER1_SLICE_REQUIRED;

#define COMN_RESP_CRYPTO_STATUS		__BIT(7)
#define COMN_RESP_PKE_STATUS		__BIT(6)
#define COMN_RESP_CMP_STATUS		__BIT(5)
#define COMN_RESP_XLAT_STATUS		__BIT(4)
#define COMN_RESP_PM_STATUS		__BIT(3)
#define COMN_RESP_INIT_ADMIN_STATUS	__BIT(2)

#define COMN_STATUS_FLAG_OK		0
#define COMN_STATUS_FLAG_ERROR		1

struct fw_la_ssl_tls_common {
	uint8_t out_len;	/* Number of bytes of key material to output. */
	uint8_t label_len;	/* Number of bytes of label for SSL and bytes
				 * for TLS key generation  */
};

struct fw_la_mgf_common {
	uint8_t hash_len;
	/* Number of bytes of hash output by the QAT per iteration */
	uint8_t seed_len;
	/* Number of bytes of seed provided in src buffer for MGF1 */
};

struct fw_cipher_hdr {
	uint8_t state_sz;
	/* State size in quad words of the cipher algorithm used in this session.
	 * Set to zero if the algorithm doesnt provide any state */
	uint8_t offset;
	/* Quad word offset from the content descriptor parameters address i.e.
	 * (content_address + (cd_hdr_sz << 3)) to the parameters for the cipher
	 * processing */
	uint8_t curr_id;
	/* Initialised with the cipher slice type */
	uint8_t next_id;
	/* Set to the next slice to pass the ciphered data through.
	 * Set to ICP_QAT_FW_SLICE_DRAM_WR if the data is not to go through
	 * anymore slices after cipher */
	uint16_t resrvd;
	/* Reserved padding byte to bring the struct to the word boundary. MUST be
	 * set to 0 */
	uint8_t state_padding_sz;
	/* State padding size in quad words. Set to 0 if no padding is required. */
	uint8_t key_sz;
	/* Key size in quad words of the cipher algorithm used in this session */
};

struct fw_auth_hdr {
	uint8_t hash_flags;
	/* General flags defining the processing to perform. 0 is normal processing
	 * and 1 means there is a nested hash processing loop to go through */
	uint8_t offset;
	/* Quad word offset from the content descriptor parameters address to the
	 * parameters for the auth processing */
	uint8_t curr_id;
	/* Initialised with the auth slice type */
	uint8_t next_id;
	/* Set to the next slice to pass data through.
	 * Set to ICP_QAT_FW_SLICE_DRAM_WR if the data is not to go through
	 * anymore slices after auth */
	union {
		uint8_t inner_prefix_sz;
		/* Size in bytes of the inner prefix data */
		uint8_t aad_sz;
		/* Size in bytes of padded AAD data to prefix to the packet for CCM
		 *  or GCM processing */
	} u;

	uint8_t outer_prefix_sz;
	/* Size in bytes of outer prefix data */
	uint8_t final_sz;
	/* Size in bytes of digest to be returned to the client if requested */
	uint8_t inner_res_sz;
	/* Size in bytes of the digest from the inner hash algorithm */
	uint8_t resrvd;
	/* This field is unused, assumed value is zero. */
	uint8_t inner_state1_sz;
	/* Size in bytes of inner hash state1 data. Must be a qword multiple */
	uint8_t inner_state2_off;
	/* Quad word offset from the content descriptor parameters pointer to the
	 * inner state2 value */
	uint8_t inner_state2_sz;
	/* Size in bytes of inner hash state2 data. Must be a qword multiple */
	uint8_t outer_config_off;
	/* Quad word offset from the content descriptor parameters pointer to the
	 * outer configuration information */
	uint8_t outer_state1_sz;
	/* Size in bytes of the outer state1 value */
	uint8_t outer_res_sz;
	/* Size in bytes of digest from the outer auth algorithm */
	uint8_t outer_prefix_off;
	/* Quad word offset from the start of the inner prefix data to the outer
	 * prefix information. Should equal the rounded inner prefix size, converted
	 * to qwords  */
};

#define FW_AUTH_HDR_FLAG_DO_NESTED	1
#define FW_AUTH_HDR_FLAG_NO_NESTED	0

struct fw_la_comn_req {
	union {
		uint16_t la_flags;
		/* Definition of the common LA processing flags used for the
		 * bulk processing */
		union {
			struct fw_la_ssl_tls_common ssl_tls_common;
			/* For TLS or SSL Key Generation, this field is
			 * overloaded with ssl_tls common information */
			struct fw_la_mgf_common mgf_common;
			/* For MGF Key Generation, this field is overloaded with
			   mgf information */
		} u;
	} u;

	union {
		uint8_t resrvd;
		/* If not useRd by a request this field must be set to 0 */
		uint8_t tls_seed_len;
		/* Byte Len of tls seed */
		uint8_t req_params_blk_sz;
		/* For bulk processing this field represents the request
		 * parameters block size */
		uint8_t trng_cfg_sz;
		/* This field is used for TRNG_ENABLE requests to indicate the
		 * size of the TRNG Slice configuration word. Size is in QW's */
	} u1;
	uint8_t la_cmd_id;
	/* Definition of the LA command defined by this request */
};

#define LA_FLAGS_GCM_IV_LEN_FLAG	__BIT(9)
#define LA_FLAGS_PROTO			__BITS(8, 6)
#define LA_FLAGS_PROTO_SNOW_3G		__SHIFTIN(4, LA_FLAGS_PROTO)
#define LA_FLAGS_PROTO_GCM		__SHIFTIN(2, LA_FLAGS_PROTO)
#define LA_FLAGS_PROTO_CCM		__SHIFTIN(1, LA_FLAGS_PROTO)
#define LA_FLAGS_PROTO_NO		__SHIFTIN(0, LA_FLAGS_PROTO)
#define LA_FLAGS_DIGEST_IN_BUFFER	__BIT(5)
#define LA_FLAGS_CMP_AUTH_RES		__BIT(4)
#define LA_FLAGS_RET_AUTH_RES		__BIT(3)
#define LA_FLAGS_UPDATE_STATE		__BIT(2)
#define LA_FLAGS_PARTIAL		__BITS(1, 0)

struct fw_la_bulk_req {
	struct fw_comn_req_hdr comn_hdr;
	/* Common request header */
	uint32_t flow_id;
	/* Field used by Firmware to limit the number of stateful requests
	 * for a session being processed at a given point of time */
	struct fw_la_comn_req comn_la_req;
	/* Common LA request parameters */
	struct fw_comn_req_mid comn_mid;
	/* Common request middle section */
	uint64_t req_params_addr;
	/* Memory address of the request parameters */
	union fw_comn_req_ftr comn_ftr;
	/* Common request footer */
};

struct fw_la_resp {
	struct fw_comn_resp_hdr comn_resp;
	uint8_t resrvd[64 - sizeof(struct fw_comn_resp_hdr)];
	/* FW_RESP_DEFAULT_SZ_HW15 */
};

struct fw_la_cipher_req_params {
	uint8_t resrvd;
	/* Reserved field and assumed set to 0 */
	uint8_t cipher_state_sz;
	/* Number of quad words of state data for the cipher algorithm */
	uint8_t curr_id;
	/* Initialised with the cipher slice type */
	uint8_t next_id;
	/* Set to the next slice to pass the ciphered data through.
	 * Set to ICP_QAT_FW_SLICE_DRAM_WR if the data is not to go through
	 * anymore slices after cipher */
	uint16_t resrvd1;
	/* Reserved field, should be set to zero*/
	uint8_t resrvd2;
	/* Reserved field, should be set to zero*/
	uint8_t next_offset;
	/* Offset in bytes to the next request parameter block */
	uint32_t cipher_off;
	/* Byte offset from the start of packet to the cipher data region */
	uint32_t cipher_len;
	/* Byte length of the cipher data region */
	uint64_t state_address;
	/* Flat buffer address in memory of the cipher state information. Unused
	 * if the state size is 0 */
};

struct fw_la_auth_req_params {
	uint8_t auth_res_sz;
	/* Size in quad words of digest information to validate */
	uint8_t hash_state_sz;
	/* Number of quad words of inner and outer hash prefix data to process */
	uint8_t curr_id;
	/* Initialised with the auth slice type */
	uint8_t next_id;
	/* Set to the next slice to pass the auth data through.
	 * Set to ICP_QAT_FW_SLICE_NULL for in-place auth-only requests
	 * Set to ICP_QAT_FW_SLICE_DRAM_WR for all other request types
	 * if the data is not to go through anymore slices after auth */
	union {
		uint16_t resrvd;
		/* Reserved field should be set to zero for bulk services */
		uint16_t tls_secret_len;
		/* Length of Secret information for TLS.   */
	} u;
	uint8_t resrvd;
	/* Reserved field, should be set to zero*/
	uint8_t next_offset;
	/* offset in bytes to the next request parameter block */
	uint32_t auth_off;
	/* Byte offset from the start of packet to the auth data region */
	uint32_t auth_len;
	/* Byte length of the auth data region */
	union {
		uint64_t prefix_addr;
		/* Address of the prefix information */
		uint64_t aad_addr;
		/* Address of the AAD info in DRAM. Used for the CCM and GCM
		 * protocols */
	} u1;
	uint64_t auth_res_address;
	/* Address of the auth result information to validate or the location to
	 * writeback the digest information to */
};

#endif
