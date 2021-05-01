/* SPDX-License-Identifier: BSD-2-Clause-NetBSD AND BSD-3-Clause */
/*	$NetBSD: qat_hw17reg.h,v 1.1 2019/11/20 09:37:46 hikaru Exp $	*/

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
 *   Copyright(c) 2014 Intel Corporation.
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

#ifndef _DEV_PCI_QAT_HW17REG_H_
#define _DEV_PCI_QAT_HW17REG_H_

/* Default message size in bytes */
#define FW_REQ_DEFAULT_SZ_HW17		128
#define FW_RESP_DEFAULT_SZ_HW17		32

/* -------------------------------------------------------------------------- */
/* accel */

enum fw_init_admin_cmd_id {
	FW_INIT_ME = 0,
	FW_TRNG_ENABLE = 1,
	FW_TRNG_DISABLE = 2,
	FW_CONSTANTS_CFG = 3,
	FW_STATUS_GET = 4,
	FW_COUNTERS_GET = 5,
	FW_LOOPBACK = 6,
	FW_HEARTBEAT_SYNC = 7,
	FW_HEARTBEAT_GET = 8,
	FW_COMP_CAPABILITY_GET = 9,
	FW_CRYPTO_CAPABILITY_GET = 10,
	FW_HEARTBEAT_TIMER_SET = 13,
};

enum fw_init_admin_resp_status {
	FW_INIT_RESP_STATUS_SUCCESS = 0,
	FW_INIT_RESP_STATUS_FAIL = 1,
	FW_INIT_RESP_STATUS_UNSUPPORTED = 4
};

struct fw_init_admin_req {
	uint16_t init_cfg_sz;
	uint8_t resrvd1;
	uint8_t init_admin_cmd_id;
	uint32_t resrvd2;
	uint64_t opaque_data;
	uint64_t init_cfg_ptr;

	union {
		struct {
			uint16_t ibuf_size_in_kb;
			uint16_t resrvd3;
		};
		uint32_t heartbeat_ticks;
	};

	uint32_t resrvd4;
};

struct fw_init_admin_resp_hdr {
	uint8_t flags;
	uint8_t resrvd1;
	uint8_t status;
	uint8_t init_admin_cmd_id;
};

enum fw_init_admin_init_flag {
	FW_INIT_FLAG_PKE_DISABLED = 0
};

struct fw_init_admin_fw_capability_resp_hdr {
	uint16_t     reserved;
	uint8_t     status;
	uint8_t     init_admin_cmd_id;
};

struct fw_init_admin_capability_resp {
	struct fw_init_admin_fw_capability_resp_hdr init_resp_hdr;
	uint32_t extended_features;
	uint64_t opaque_data;
	union {
		struct {
			uint16_t    compression_algos;
			uint16_t    checksum_algos;
			uint32_t    deflate_capabilities;
			uint32_t    resrvd1;
			uint32_t    lzs_capabilities;
		} compression;
		struct {
			uint32_t    cipher_algos;
			uint32_t    hash_algos;
			uint16_t    keygen_algos;
			uint16_t    other;
			uint16_t    public_key_algos;
			uint16_t    prime_algos;
		} crypto;
	};
};

struct fw_init_admin_resp_pars {
	union {
		uint32_t resrvd1[4];
		struct {
			uint32_t version_patch_num;
			uint8_t context_id;
			uint8_t ae_id;
			uint16_t resrvd1;
			uint64_t resrvd2;
		} s1;
		struct {
			uint64_t req_rec_count;
			uint64_t resp_sent_count;
		} s2;
	} u;
};

struct fw_init_admin_hb_cnt {
	uint16_t resp_heartbeat_cnt;
	uint16_t req_heartbeat_cnt;
};

#define QAT_NUM_THREADS 8

struct fw_init_admin_hb_stats {
	struct fw_init_admin_hb_cnt stats[QAT_NUM_THREADS];
};

struct fw_init_admin_resp {
	struct fw_init_admin_resp_hdr init_resp_hdr;
	union {
		uint32_t resrvd2;
		struct {
			uint16_t version_minor_num;
			uint16_t version_major_num;
		} s;
	} u;
	uint64_t opaque_data;
	struct fw_init_admin_resp_pars init_resp_pars;
};

#define FW_COMN_HEARTBEAT_OK 0
#define FW_COMN_HEARTBEAT_BLOCKED 1
#define FW_COMN_HEARTBEAT_FLAG_BITPOS 0
#define FW_COMN_HEARTBEAT_FLAG_MASK 0x1
#define FW_COMN_STATUS_RESRVD_FLD_MASK 0xFE
#define FW_COMN_HEARTBEAT_HDR_FLAG_GET(hdr_t) \
	FW_COMN_HEARTBEAT_FLAG_GET(hdr_t.flags)

#define FW_COMN_HEARTBEAT_HDR_FLAG_SET(hdr_t, val) \
	FW_COMN_HEARTBEAT_FLAG_SET(hdr_t, val)

#define FW_COMN_HEARTBEAT_FLAG_GET(flags) \
	QAT_FIELD_GET(flags, \
		 FW_COMN_HEARTBEAT_FLAG_BITPOS, \
		 FW_COMN_HEARTBEAT_FLAG_MASK)

/* -------------------------------------------------------------------------- */

/* Big assumptions that both bitpos and mask are constants */
#define FIELD_SET(flags, val, bitpos, mask)                                \
    (flags) =                                                                  \
        (((flags) & (~((mask) << (bitpos)))) | (((val) & (mask)) << (bitpos)))

#define FIELD_GET(flags, bitpos, mask) (((flags) >> (bitpos)) & (mask))

#define FLAG_SET(flags, bitpos) (flags) = ((flags) | (1 << (bitpos)))

#define FLAG_CLEAR(flags, bitpos) (flags) = ((flags) & (~(1 << (bitpos))))

#define FLAG_GET(flags, bitpos) (((flags) >> (bitpos)) & 1)

/* Default request and response ring size in bytes  */
#define FW_REQ_DEFAULT_SZ 128
#define FW_RESP_DEFAULT_SZ 32

#define FW_COMN_ONE_BYTE_SHIFT 8
#define FW_COMN_SINGLE_BYTE_MASK 0xFF

/* Common Request - Block sizes definitions in multiples of individual long
 * words  */
#define FW_NUM_LONGWORDS_1 1
#define FW_NUM_LONGWORDS_2 2
#define FW_NUM_LONGWORDS_3 3
#define FW_NUM_LONGWORDS_4 4
#define FW_NUM_LONGWORDS_5 5
#define FW_NUM_LONGWORDS_6 6
#define FW_NUM_LONGWORDS_7 7
#define FW_NUM_LONGWORDS_10 10
#define FW_NUM_LONGWORDS_13 13

/* Definition of the associated service Id for NULL service type.
   Note: the response is expected to use FW_COMN_RESP_SERV_CPM_FW  */
#define FW_NULL_REQ_SERV_ID 1

/*
 * Definition of the firmware interface service users, for
 * responses.
 * Enumeration which is used to indicate the ids of the services
 * for responses using the external firmware interfaces.
 */

enum fw_comn_resp_serv_id {
	FW_COMN_RESP_SERV_NULL,     /* NULL service id type */
	FW_COMN_RESP_SERV_CPM_FW,   /* CPM FW Service ID */
	FW_COMN_RESP_SERV_DELIMITER /* Delimiter service id type */
};

/*
 * Definition of the request types
 * Enumeration which is used to indicate the ids of the request
 * types used in each of the external firmware interfaces
 */

enum fw_comn_request_id {
	FW_COMN_REQ_NULL = 0,        /* NULL request type */
	FW_COMN_REQ_CPM_FW_PKE = 3,  /* CPM FW PKE Request */
	FW_COMN_REQ_CPM_FW_LA = 4,   /* CPM FW Lookaside Request */
	FW_COMN_REQ_CPM_FW_DMA = 7,  /* CPM FW DMA Request */
	FW_COMN_REQ_CPM_FW_COMP = 9, /* CPM FW Compression Request */
	FW_COMN_REQ_DELIMITER        /* End delimiter */

};

/*
 * Definition of the common QAT FW request content descriptor field -
 * points to the content descriptor parameters or itself contains service-
 * specific data. Also specifies content descriptor parameter size.
 * Contains reserved fields.
 * Common section of the request used across all of the services exposed
 * by the QAT FW. Each of the services inherit these common fields
 */
union fw_comn_req_hdr_cd_pars {
	/* LWs 2-5 */
	struct
	{
		uint64_t content_desc_addr;
		/* Address of the content descriptor */

		uint16_t content_desc_resrvd1;
		/* Content descriptor reserved field */

		uint8_t content_desc_params_sz;
		/* Size of the content descriptor parameters in quad words. These
		 * parameters describe the session setup configuration info for the
		 * slices that this request relies upon i.e. the configuration word and
		 * cipher key needed by the cipher slice if there is a request for
		 * cipher processing. */

		uint8_t content_desc_hdr_resrvd2;
		/* Content descriptor reserved field */

		uint32_t content_desc_resrvd3;
		/* Content descriptor reserved field */
	} s;

	struct
	{
		uint32_t serv_specif_fields[FW_NUM_LONGWORDS_4];

	} s1;

};

/*
 * Definition of the common QAT FW request middle block.
 * Common section of the request used across all of the services exposed
 * by the QAT FW. Each of the services inherit these common fields
 */
struct fw_comn_req_mid
{
	/* LWs 6-13 */
	uint64_t opaque_data;
	/* Opaque data passed unmodified from the request to response messages by
	 * firmware (fw) */

	uint64_t src_data_addr;
	/* Generic definition of the source data supplied to the QAT AE. The
	 * common flags are used to further describe the attributes of this
	 * field */

	uint64_t dest_data_addr;
	/* Generic definition of the destination data supplied to the QAT AE. The
	 * common flags are used to further describe the attributes of this
	 * field */

	uint32_t src_length;
	/* Length of source flat buffer incase src buffer
	 * type is flat */

	uint32_t dst_length;
	/* Length of source flat buffer incase dst buffer
	 * type is flat */

};

/*
 * Definition of the common QAT FW request content descriptor control
 * block.
 *
 * Service specific section of the request used across all of the services
 * exposed by the QAT FW. Each of the services populates this block
 * uniquely. Refer to the service-specific header structures e.g.
 * 'fw_cipher_hdr_s' (for Cipher) etc.
 */
struct fw_comn_req_cd_ctrl
{
	/* LWs 27-31 */
	uint32_t content_desc_ctrl_lw[FW_NUM_LONGWORDS_5];

};

/*
 * Definition of the common QAT FW request header.
 * Common section of the request used across all of the services exposed
 * by the QAT FW. Each of the services inherit these common fields. The
 * reserved field of 7 bits and the service command Id field are all
 * service-specific fields, along with the service specific flags.
 */
struct fw_comn_req_hdr
{
	/* LW0 */
	uint8_t resrvd1;
	/* reserved field */

	uint8_t service_cmd_id;
	/* Service Command Id  - this field is service-specific
	 * Please use service-specific command Id here e.g.Crypto Command Id
	 * or Compression Command Id etc. */

	uint8_t service_type;
	/* Service type */

	uint8_t hdr_flags;
	/* This represents a flags field for the Service Request.
	 * The most significant bit is the 'valid' flag and the only
	 * one used. All remaining bit positions are unused and
	 * are therefore reserved and need to be set to 0. */

	/* LW1 */
	uint16_t serv_specif_flags;
	/* Common Request service-specific flags
	 * e.g. Symmetric Crypto Command Flags */

	uint16_t comn_req_flags;
	/* Common Request Flags consisting of
	 * - 14 reserved bits,
	 * - 1 Content Descriptor field type bit and
	 * - 1 Source/destination pointer type bit */

};

/*
 * Definition of the common QAT FW request parameter field.
 *
 * Service specific section of the request used across all of the services
 * exposed by the QAT FW. Each of the services populates this block
 * uniquely. Refer to service-specific header structures e.g.
 * 'fw_comn_req_cipher_rqpars_s' (for Cipher) etc.
 *
 */
struct fw_comn_req_rqpars
{
	/* LWs 14-26 */
	uint32_t serv_specif_rqpars_lw[FW_NUM_LONGWORDS_13];

};

/*
 * Definition of the common request structure with service specific
 * fields
 * This is a definition of the full qat request structure used by all
 * services. Each service is free to use the service fields in its own
 * way. This struct is useful as a message passing argument before the
 * service contained within the request is determined.
 */
struct fw_comn_req
{
	/* LWs 0-1 */
	struct fw_comn_req_hdr comn_hdr;
	/* Common request header */

	/* LWs 2-5 */
	union fw_comn_req_hdr_cd_pars cd_pars;
	/* Common Request content descriptor field which points either to a
	 * content descriptor
	 * parameter block or contains the service-specific data itself. */

	/* LWs 6-13 */
	struct fw_comn_req_mid comn_mid;
	/* Common request middle section */

	/* LWs 14-26 */
	struct fw_comn_req_rqpars serv_specif_rqpars;
	/* Common request service-specific parameter field */

	/* LWs 27-31 */
	struct fw_comn_req_cd_ctrl cd_ctrl;
	/* Common request content descriptor control block -
	 * this field is service-specific */

};

/*
 * Error code field
 *
 * Overloaded field with 8 bit common error field or two
 * 8 bit compression error fields for compression and translator slices
 */
union fw_comn_error {
	struct
	{
		uint8_t resrvd;
		/* 8 bit reserved field */

		uint8_t comn_err_code;
		/* 8 bit common error code */

	} s;
	/* Structure which is used for non-compression responses */

	struct
	{
		uint8_t xlat_err_code;
		/* 8 bit translator error field */

		uint8_t cmp_err_code;
		/* 8 bit compression error field */

	} s1;
	/* Structure which is used for compression responses */

};

/*
 * Definition of the common QAT FW response header.
 * This section of the response is common across all of the services
 * that generate a firmware interface response
 */
struct fw_comn_resp_hdr
{
	/* LW0 */
	uint8_t resrvd1;
	/* Reserved field - this field is service-specific -
	 * Note: The Response Destination Id has been removed
	 * from first QWord */

	uint8_t service_id;
	/* Service Id returned by service block */

	uint8_t response_type;
	/* Response type - copied from the request to
	 * the response message */

	uint8_t hdr_flags;
	/* This represents a flags field for the Response.
	 * Bit<7> = 'valid' flag
	 * Bit<6> = 'CNV' flag indicating that CNV was executed
	 *          on the current request
	 * Bit<5> = 'CNVNR' flag indicating that a recovery happened
	 *          on the current request following a CNV error
	 * All remaining bits are unused and are therefore reserved.
	 * They must to be set to 0.
	 */

	/* LW 1 */
	union fw_comn_error comn_error;
	/* This field is overloaded to allow for one 8 bit common error field
	 *   or two 8 bit error fields from compression and translator  */

	uint8_t comn_status;
	/* Status field which specifies which slice(s) report an error */

	uint8_t cmd_id;
	/* Command Id - passed from the request to the response message */

};

/*
 * Definition of the common response structure with service specific
 * fields
 * This is a definition of the full qat response structure used by all
 * services.
 */
struct fw_comn_resp
{
	/* LWs 0-1 */
	struct fw_comn_resp_hdr comn_hdr;
	/* Common header fields */

	/* LWs 2-3 */
	uint64_t opaque_data;
	/* Opaque data passed from the request to the response message */

	/* LWs 4-7 */
	uint32_t resrvd[FW_NUM_LONGWORDS_4];
	/* Reserved */

};

/*  Common QAT FW request header - structure of LW0
 *  + ===== + ---- + ----------- + ----------- + ----------- + ----------- +
 *  |  Bit  |  31  |  30 - 24    |  21 - 16    |  15 - 8     |  7 - 0      |
 *  + ===== + ---- + ----------- + ----------- + ----------- + ----------- +
 *  | Flags |  V   |   Reserved  | Serv Type   | Serv Cmd Id |  Reserved   |
 *  + ===== + ---- + ----------- + ----------- + ----------- + ----------- +
 */

#define FW_COMN_VALID		__BIT(7)

/*  Common QAT FW response header - structure of LW0
 *  + ===== + --- + --- + ----- + ----- + --------- + ----------- + ----- +
 *  |  Bit  | 31  | 30  |   29  | 28-24 |  21 - 16  |  15 - 8     |  7-0  |
 *  + ===== + --- + ----+ ----- + ----- + --------- + ----------- + ----- +
 *  | Flags |  V  | CNV | CNVNR | Rsvd  | Serv Type | Serv Cmd Id |  Rsvd |
 *  + ===== + --- + --- + ----- + ----- + --------- + ----------- + ----- +  */
/* Macros defining the bit position and mask of 'CNV' flag
 * within the hdr_flags field of LW0 (service response only)  */
#define FW_COMN_CNV_FLAG_BITPOS 6
#define FW_COMN_CNV_FLAG_MASK 0x1

/* Macros defining the bit position and mask of CNVNR flag
 * within the hdr_flags field of LW0 (service response only)  */
#define FW_COMN_CNVNR_FLAG_BITPOS 5
#define FW_COMN_CNVNR_FLAG_MASK 0x1

/*
 * Macro for extraction of Service Type Field
 *
 * struct fw_comn_req_hdr  Structure 'fw_comn_req_hdr_t'
 *                 to extract the Service Type Field
 */
#define FW_COMN_OV_SRV_TYPE_GET(fw_comn_req_hdr_t)	\
	fw_comn_req_hdr_t.service_type

/*
 * Macro for setting of Service Type Field
 *
 * 'fw_comn_req_hdr_t' structure to set the Service
 *                  Type Field
 * val    Value of the Service Type Field
 */
#define FW_COMN_OV_SRV_TYPE_SET(fw_comn_req_hdr_t, val)	\
	fw_comn_req_hdr_t.service_type = val

/*
 * Macro for extraction of Service Command Id Field
 *
 * struct fw_comn_req_hdr  Structure 'fw_comn_req_hdr_t'
 *                 to extract the Service Command Id Field
 */
#define FW_COMN_OV_SRV_CMD_ID_GET(fw_comn_req_hdr_t)	\
	fw_comn_req_hdr_t.service_cmd_id

/*
 * Macro for setting of Service Command Id Field
 *
 * 'fw_comn_req_hdr_t' structure to set the
 *                  Service Command Id Field
 * val    Value of the Service Command Id Field
 */
#define FW_COMN_OV_SRV_CMD_ID_SET(fw_comn_req_hdr_t, val)	\
	fw_comn_req_hdr_t.service_cmd_id = val

/*
 * Extract the valid flag from the request or response's header flags.
 *
 * hdr_t  Request or Response 'hdr_t' structure to extract the valid bit
 *  from the  'hdr_flags' field.
 */
#define FW_COMN_HDR_VALID_FLAG_GET(hdr_t)	\
	FW_COMN_VALID_FLAG_GET(hdr_t.hdr_flags)

/*
 * Extract the CNVNR flag from the header flags in the response only.
 *
 * hdr_t  Response 'hdr_t' structure to extract the CNVNR bit
 *  from the  'hdr_flags' field.
 */
#define FW_COMN_HDR_CNVNR_FLAG_GET(hdr_flags)	\
	FIELD_GET(hdr_flags,	\
			      FW_COMN_CNVNR_FLAG_BITPOS,	\
			      FW_COMN_CNVNR_FLAG_MASK)

/*
 * Extract the CNV flag from the header flags in the response only.
 *
 * hdr_t  Response 'hdr_t' structure to extract the CNV bit
 *  from the  'hdr_flags' field.
 */
#define FW_COMN_HDR_CNV_FLAG_GET(hdr_flags)	\
	FIELD_GET(hdr_flags,	\
			      FW_COMN_CNV_FLAG_BITPOS,	\
			      FW_COMN_CNV_FLAG_MASK)

/*
 * Set the valid bit in the request's header flags.
 *
 * hdr_t  Request or Response 'hdr_t' structure to set the valid bit
 * val    Value of the valid bit flag.
 */
#define FW_COMN_HDR_VALID_FLAG_SET(hdr_t, val)	\
	FW_COMN_VALID_FLAG_SET(hdr_t, val)

/*
 * Common macro to extract the valid flag from the header flags field
 * within the header structure (request or response).
 *
 * hdr_t  Structure (request or response) to extract the
 *  valid bit from the 'hdr_flags' field.
 */
#define FW_COMN_VALID_FLAG_GET(hdr_flags)	\
	FIELD_GET(hdr_flags,	\
			      FW_COMN_VALID_FLAG_BITPOS,	\
			      FW_COMN_VALID_FLAG_MASK)

/*
 * Common macro to extract the remaining reserved flags from the header
 * flags field within the header structure (request or response).
 *
 * hdr_t  Structure (request or response) to extract the
 *  remaining bits from the 'hdr_flags' field (excluding the
 *  valid flag).
 */
#define FW_COMN_HDR_RESRVD_FLD_GET(hdr_flags)	\
	(hdr_flags & FW_COMN_HDR_RESRVD_FLD_MASK)

/*
 * Common macro to set the valid bit in the header flags field within
 * the header structure (request or response).
 *
 * hdr_t  Structure (request or response) containing the header
 *  flags field, to allow the valid bit to be set.
 * val    Value of the valid bit flag.
 */
#define FW_COMN_VALID_FLAG_SET(hdr_t, val)	\
	FIELD_SET((hdr_t.hdr_flags),	\
			      (val),	\
			      FW_COMN_VALID_FLAG_BITPOS,	\
			      FW_COMN_VALID_FLAG_MASK)

/*
 * Macro that must be used when building the common header flags.
 * Note that all bits reserved field bits 0-6 (LW0) need to be forced to 0.
 *
 * ptr   Value of the valid flag
 */

#define FW_COMN_HDR_FLAGS_BUILD(valid)	\
	(((valid)&FW_COMN_VALID_FLAG_MASK)	\
	 << FW_COMN_VALID_FLAG_BITPOS)

/*
 *  Common Request Flags Definition
 *  The bit offsets below are within the flags field. These are NOT relative to
 *  the memory word. Unused fields e.g. reserved bits, must be zeroed.
 *
 *  + ===== + ------ + --- + --- + --- + --- + --- + --- + --- + --- +
 *  | Bits [15:8]    |  15 |  14 |  13 |  12 |  11 |  10 |  9  |  8  |
 *  + ===== + ------ + --- + --- + --- + --- + --- + --- + --- + --- +
 *  | Flags[15:8]    | Rsv | Rsv | Rsv | Rsv | Rsv | Rsv | Rsv | Rsv |
 *  + ===== + ------ + --- + --- + --- + --- + --- + --- + --- + --- +
 *  | Bits  [7:0]    |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
 *  + ===== + ------ + --- + --- + --- + --- + --- + --- + --- + --- +
 *  | Flags [7:0]    | Rsv | Rsv | Rsv | Rsv | Rsv | BnP | Cdt | Ptr |
 *  + ===== + ------ + --- + --- + --- + --- + --- + --- + --- + --- +
 */

#define COMN_PTR_TYPE_BITPOS 0
/* Common Request Flags - Starting bit position indicating
 * Src&Dst Buffer Pointer type   */

#define COMN_PTR_TYPE_MASK 0x1
/* Common Request Flags - One bit mask used to determine
 * Src&Dst Buffer Pointer type  */

#define COMN_CD_FLD_TYPE_BITPOS 1
/* Common Request Flags - Starting bit position indicating
 * CD Field type   */

#define COMN_CD_FLD_TYPE_MASK 0x1
/* Common Request Flags - One bit mask used to determine
 * CD Field type  */

#define COMN_BNP_ENABLED_BITPOS 2
/* Common Request Flags - Starting bit position indicating
 * the source buffer contains batch of requests. if this
 * bit is set, source buffer is type of Batch And Pack OpData List
 * and the Ptr Type Bit only applies to Destination buffer.  */

#define COMN_BNP_ENABLED_MASK 0x1
/* Batch And Pack Enabled Flag Mask - One bit mask used to determine
 * the source buffer is in Batch and Pack OpData Link List Mode.  */

/* ========================================================================= */
/*                                       Pointer Type Flag definitions       */
/* ========================================================================= */
#define COMN_PTR_TYPE_FLAT 0x0
/* Constant value indicating Src&Dst Buffer Pointer type is flat
 * If Batch and Pack mode is enabled, only applies to Destination buffer. */

#define COMN_PTR_TYPE_SGL 0x1
/* Constant value indicating Src&Dst Buffer Pointer type is SGL type
 * If Batch and Pack mode is enabled, only applies to Destination buffer. */

#define COMN_PTR_TYPE_BATCH 0x2
/* Constant value indicating Src is a batch request
 * and Dst Buffer Pointer type is SGL type  */

/* ========================================================================= */
/*                                       CD Field Flag definitions           */
/* ========================================================================= */
#define COMN_CD_FLD_TYPE_64BIT_ADR 0x0
/* Constant value indicating CD Field contains 64-bit address  */

#define COMN_CD_FLD_TYPE_16BYTE_DATA 0x1
/* Constant value indicating CD Field contains 16 bytes of setup data  */

/* ========================================================================= */
/*                       Batch And Pack Enable/Disable Definitions           */
/* ========================================================================= */
#define COMN_BNP_ENABLED 0x1
/* Constant value indicating Source buffer will point to Batch And Pack OpData
 * List  */

#define COMN_BNP_DISABLED 0x0
/* Constant value indicating Source buffer will point to Batch And Pack OpData
 * List  */

/*
 * Macro that must be used when building the common request flags (for all
 * requests but comp BnP).
 * Note that all bits reserved field bits 2-15 (LW1) need to be forced to 0.
 *
 * ptr   Value of the pointer type flag
 * cdt   Value of the cd field type flag
*/
#define FW_COMN_FLAGS_BUILD(cdt, ptr)	\
	((((cdt)&COMN_CD_FLD_TYPE_MASK) << COMN_CD_FLD_TYPE_BITPOS) |	\
	 (((ptr)&COMN_PTR_TYPE_MASK) << COMN_PTR_TYPE_BITPOS))

/*
 * Macro that must be used when building the common request flags for comp
 * BnP service.
 * Note that all bits reserved field bits 3-15 (LW1) need to be forced to 0.
 *
 * ptr   Value of the pointer type flag
 * cdt   Value of the cd field type flag
 * bnp   Value of the bnp enabled flag
 */
#define FW_COMN_FLAGS_BUILD_BNP(cdt, ptr, bnp)	\
	((((cdt)&COMN_CD_FLD_TYPE_MASK) << COMN_CD_FLD_TYPE_BITPOS) |	\
	 (((ptr)&COMN_PTR_TYPE_MASK) << COMN_PTR_TYPE_BITPOS) |	\
	 (((bnp)&COMN_BNP_ENABLED_MASK) << COMN_BNP_ENABLED_BITPOS))

/*
 * Macro for extraction of the pointer type bit from the common flags
 *
 * flags      Flags to extract the pointer type bit from
 */
#define FW_COMN_PTR_TYPE_GET(flags)	\
	FIELD_GET(flags, COMN_PTR_TYPE_BITPOS, COMN_PTR_TYPE_MASK)

/*
 * Macro for extraction of the cd field type bit from the common flags
 *
 * flags      Flags to extract the cd field type type bit from
 */
#define FW_COMN_CD_FLD_TYPE_GET(flags)	\
	FIELD_GET(flags, COMN_CD_FLD_TYPE_BITPOS, COMN_CD_FLD_TYPE_MASK)

/*
 * Macro for extraction of the bnp field type bit from the common flags
 *
 * flags      Flags to extract the bnp field type type bit from
 *
 */
#define FW_COMN_BNP_ENABLED_GET(flags)	\
	FIELD_GET(flags, COMN_BNP_ENABLED_BITPOS, COMN_BNP_ENABLED_MASK)

/*
 * Macro for setting the pointer type bit in the common flags
 *
 * flags      Flags in which Pointer Type bit will be set
 * val        Value of the bit to be set in flags
 *
 */
#define FW_COMN_PTR_TYPE_SET(flags, val)	\
	FIELD_SET(flags, val, COMN_PTR_TYPE_BITPOS, COMN_PTR_TYPE_MASK)

/*
 * Macro for setting the cd field type bit in the common flags
 *
 * flags      Flags in which Cd Field Type bit will be set
 * val        Value of the bit to be set in flags
 *
 */
#define FW_COMN_CD_FLD_TYPE_SET(flags, val)	\
	FIELD_SET(	\
		flags, val, COMN_CD_FLD_TYPE_BITPOS, COMN_CD_FLD_TYPE_MASK)

/*
 * Macro for setting the bnp field type bit in the common flags
 *
 * flags      Flags in which Bnp Field Type bit will be set
 * val        Value of the bit to be set in flags
 *
 */
#define FW_COMN_BNP_ENABLE_SET(flags, val)	\
	FIELD_SET(	\
		flags, val, COMN_BNP_ENABLED_BITPOS, COMN_BNP_ENABLED_MASK)

/*
 * Macros using the bit position and mask to set/extract the next
 * and current id nibbles within the next_curr_id field of the
 * content descriptor header block. Note that these are defined
 * in the common header file, as they are used by compression, cipher
 * and authentication.
 *
 * cd_ctrl_hdr_t      Content descriptor control block header pointer.
 * val                Value of the field being set.
 */
#define FW_COMN_NEXT_ID_BITPOS 4
#define FW_COMN_NEXT_ID_MASK 0xF0
#define FW_COMN_CURR_ID_BITPOS 0
#define FW_COMN_CURR_ID_MASK 0x0F

#define FW_COMN_NEXT_ID_GET(cd_ctrl_hdr_t)	\
	((((cd_ctrl_hdr_t)->next_curr_id) & FW_COMN_NEXT_ID_MASK) >>	\
	 (FW_COMN_NEXT_ID_BITPOS))

#define FW_COMN_NEXT_ID_SET(cd_ctrl_hdr_t, val)	\
	((cd_ctrl_hdr_t)->next_curr_id) =	\
		((((cd_ctrl_hdr_t)->next_curr_id) & FW_COMN_CURR_ID_MASK) |	\
		 ((val << FW_COMN_NEXT_ID_BITPOS) &	\
		  FW_COMN_NEXT_ID_MASK))

#define FW_COMN_CURR_ID_GET(cd_ctrl_hdr_t)	\
	(((cd_ctrl_hdr_t)->next_curr_id) & FW_COMN_CURR_ID_MASK)

#define FW_COMN_CURR_ID_SET(cd_ctrl_hdr_t, val)	\
	((cd_ctrl_hdr_t)->next_curr_id) =	\
		((((cd_ctrl_hdr_t)->next_curr_id) & FW_COMN_NEXT_ID_MASK) |	\
		 ((val)&FW_COMN_CURR_ID_MASK))

/*
 *  Common Status Field Definition  The bit offsets below are within the COMMON
 *  RESPONSE status field, assumed to be 8 bits wide. In the case of the PKE
 *  response (which follows the CPM 1.5 message format), the status field is 16
 *  bits wide.
 *  The status flags are contained within the most significant byte and align
 *  with the diagram below. Please therefore refer to the service-specific PKE
 *  header file for the appropriate macro definition to extract the PKE status
 *  flag from the PKE response, which assumes that a word is passed to the
 *  macro.
 *  + ===== + ------ + --- + --- + ---- + ---- + -------- + ---- + ---------- +
 *  |  Bit  |   7    |  6  |  5  |  4   |  3   |    2     |   1  |      0     |
 *  + ===== + ------ + --- + --- + ---- + ---- + -------- + ---- + ---------- +
 *  | Flags | Crypto | Pke | Cmp | Xlat | EOLB | UnSupReq | Rsvd | XltWaApply |
 *  + ===== + ------ + --- + --- + ---- + ---- + -------- + ---- + ---------- +
 * Note:
 * For the service specific status bit definitions refer to service header files
 * Eg. Crypto Status bit refers to Symmetric Crypto, Key Generation, and NRBG
 * Requests' Status. Unused bits e.g. reserved bits need to have been forced to
 * 0.
 */

#define COMN_RESP_CRYPTO_STATUS_BITPOS 7
/* Starting bit position indicating Response for Crypto service Flag  */

#define COMN_RESP_CRYPTO_STATUS_MASK 0x1
/* One bit mask used to determine Crypto status mask  */

#define COMN_RESP_PKE_STATUS_BITPOS 6
/* Starting bit position indicating Response for PKE service Flag  */

#define COMN_RESP_PKE_STATUS_MASK 0x1
/* One bit mask used to determine PKE status mask  */

#define COMN_RESP_CMP_STATUS_BITPOS 5
/* Starting bit position indicating Response for Compression service Flag  */

#define COMN_RESP_CMP_STATUS_MASK 0x1
/* One bit mask used to determine Compression status mask  */

#define COMN_RESP_XLAT_STATUS_BITPOS 4
/* Starting bit position indicating Response for Xlat service Flag  */

#define COMN_RESP_XLAT_STATUS_MASK 0x1
/* One bit mask used to determine Translator status mask  */

#define COMN_RESP_CMP_END_OF_LAST_BLK_BITPOS 3
/* Starting bit position indicating the last block in a deflate stream for
  the compression service Flag */

#define COMN_RESP_CMP_END_OF_LAST_BLK_MASK 0x1
/* One bit mask used to determine the last block in a deflate stream
   status mask */

#define COMN_RESP_UNSUPPORTED_REQUEST_BITPOS 2
/* Starting bit position indicating when an unsupported service request Flag  */

#define COMN_RESP_UNSUPPORTED_REQUEST_MASK 0x1
/* One bit mask used to determine the unsupported service request status mask  */

#define COMN_RESP_XLT_WA_APPLIED_BITPOS 0
/* Bit position indicating a firmware workaround was applied to translation  */

#define COMN_RESP_XLT_WA_APPLIED_MASK 0x1
/* One bit mask  */

/*
 * Macro that must be used when building the status
 * for the common response
 *
 * crypto   Value of the Crypto Service status flag
 * comp     Value of the Compression Service Status flag
 * xlat     Value of the Xlator Status flag
 * eolb     Value of the Compression End of Last Block Status flag
 * unsupp   Value of the Unsupported Request flag
 * xlt_wa   Value of the Translation WA marker
 */
#define FW_COMN_RESP_STATUS_BUILD(	\
	crypto, pke, comp, xlat, eolb, unsupp, xlt_wa)	\
	((((crypto)&COMN_RESP_CRYPTO_STATUS_MASK)	\
	  << COMN_RESP_CRYPTO_STATUS_BITPOS) |	\
	 (((pke)&COMN_RESP_PKE_STATUS_MASK)	\
	  << COMN_RESP_PKE_STATUS_BITPOS) |	\
	 (((xlt_wa)&COMN_RESP_XLT_WA_APPLIED_MASK)	\
	  << COMN_RESP_XLT_WA_APPLIED_BITPOS) |	\
	 (((comp)&COMN_RESP_CMP_STATUS_MASK)	\
	  << COMN_RESP_CMP_STATUS_BITPOS) |	\
	 (((xlat)&COMN_RESP_XLAT_STATUS_MASK)	\
	  << COMN_RESP_XLAT_STATUS_BITPOS) |	\
	 (((eolb)&COMN_RESP_CMP_END_OF_LAST_BLK_MASK)	\
	  << COMN_RESP_CMP_END_OF_LAST_BLK_BITPOS) |	\
	 (((unsupp)&COMN_RESP_UNSUPPORTED_REQUEST_BITPOS)	\
	  << COMN_RESP_UNSUPPORTED_REQUEST_MASK))

/*
 * Macro for extraction of the Crypto bit from the status
 *
 * status Status to extract the status bit from
 */
#define FW_COMN_RESP_CRYPTO_STAT_GET(status)	\
	FIELD_GET(status,	\
			      COMN_RESP_CRYPTO_STATUS_BITPOS,	\
			      COMN_RESP_CRYPTO_STATUS_MASK)

/*
 * Macro for extraction of the PKE bit from the status
 *
 * status Status to extract the status bit from
 */
#define FW_COMN_RESP_PKE_STAT_GET(status)	\
	FIELD_GET(status,	\
			      COMN_RESP_PKE_STATUS_BITPOS,	\
			      COMN_RESP_PKE_STATUS_MASK)

/*
 * Macro for extraction of the Compression bit from the status
 *
 * status Status to extract the status bit from
 */
#define FW_COMN_RESP_CMP_STAT_GET(status)	\
	FIELD_GET(status,	\
			      COMN_RESP_CMP_STATUS_BITPOS,	\
			      COMN_RESP_CMP_STATUS_MASK)

/*
 * Macro for extraction of the Translator bit from the status
 *
 * status Status to extract the status bit from
 */
#define FW_COMN_RESP_XLAT_STAT_GET(status)	\
	FIELD_GET(status,	\
			      COMN_RESP_XLAT_STATUS_BITPOS,	\
			      COMN_RESP_XLAT_STATUS_MASK)

/*
 * Macro for extraction of the Translation Workaround Applied bit from the
 * status
 *
 * status Status to extract the status bit from
 */
#define FW_COMN_RESP_XLT_WA_APPLIED_GET(status)	\
	FIELD_GET(status,	\
			      COMN_RESP_XLT_WA_APPLIED_BITPOS,	\
			      COMN_RESP_XLT_WA_APPLIED_MASK)

/*
 * Macro for extraction of the end of compression block bit from the
 * status
 *
 * status
 * Status to extract the status bit from
 */
#define FW_COMN_RESP_CMP_END_OF_LAST_BLK_FLAG_GET(status)	\
	FIELD_GET(status,	\
			      COMN_RESP_CMP_END_OF_LAST_BLK_BITPOS,	\
			      COMN_RESP_CMP_END_OF_LAST_BLK_MASK)

/*
 * Macro for extraction of the Unsupported request from the status
 *
 * status
 * Status to extract the status bit from
 */
#define FW_COMN_RESP_UNSUPPORTED_REQUEST_STAT_GET(status)	\
	FIELD_GET(status,	\
			      COMN_RESP_UNSUPPORTED_REQUEST_BITPOS,	\
			      COMN_RESP_UNSUPPORTED_REQUEST_MASK)

#define FW_COMN_STATUS_FLAG_OK 0
/* Definition of successful processing of a request  */

#define FW_COMN_STATUS_FLAG_ERROR 1
/* Definition of erroneous processing of a request  */

#define FW_COMN_STATUS_CMP_END_OF_LAST_BLK_FLAG_CLR 0
/* Final Deflate block of a compression request not completed  */

#define FW_COMN_STATUS_CMP_END_OF_LAST_BLK_FLAG_SET 1
/* Final Deflate block of a compression request completed  */

#define ERR_CODE_NO_ERROR 0
/* Error Code constant value for no error  */

#define ERR_CODE_INVALID_BLOCK_TYPE -1
/* Invalid block type (type == 3)*/

#define ERR_CODE_NO_MATCH_ONES_COMP -2
/* Stored block length does not match one's complement */

#define ERR_CODE_TOO_MANY_LEN_OR_DIS -3
/* Too many length or distance codes */

#define ERR_CODE_INCOMPLETE_LEN -4
/* Code lengths codes incomplete */

#define ERR_CODE_RPT_LEN_NO_FIRST_LEN -5
/* Repeat lengths with no first length */

#define ERR_CODE_RPT_GT_SPEC_LEN -6
/* Repeat more than specified lengths */

#define ERR_CODE_INV_LIT_LEN_CODE_LEN -7
/* Invalid lit/len code lengths */

#define ERR_CODE_INV_DIS_CODE_LEN -8
/* Invalid distance code lengths */

#define ERR_CODE_INV_LIT_LEN_DIS_IN_BLK -9
/* Invalid lit/len or distance code in fixed/dynamic block */

#define ERR_CODE_DIS_TOO_FAR_BACK -10
/* Distance too far back in fixed or dynamic block */

/* Common Error code definitions */
#define ERR_CODE_OVERFLOW_ERROR -11
/* Error Code constant value for overflow error  */

#define ERR_CODE_SOFT_ERROR -12
/* Error Code constant value for soft error  */

#define ERR_CODE_FATAL_ERROR -13
/* Error Code constant value for hard/fatal error  */

#define ERR_CODE_COMP_OUTPUT_CORRUPTION -14
/* Error Code constant for compression output corruption */

#define ERR_CODE_HW_INCOMPLETE_FILE -15
/* Error Code constant value for incomplete file hardware error  */

#define ERR_CODE_SSM_ERROR -16
/* Error Code constant value for error detected by SSM e.g. slice hang  */

#define ERR_CODE_ENDPOINT_ERROR -17
/* Error Code constant value for error detected by PCIe Endpoint, e.g. push
 * data error   */

#define ERR_CODE_CNV_ERROR -18
/* Error Code constant value for cnv failure  */

#define ERR_CODE_EMPTY_DYM_BLOCK -19
/* Error Code constant value for submission of empty dynamic stored block to
 * slice   */

#define ERR_CODE_KPT_CRYPTO_SERVICE_FAIL_INVALID_HANDLE -20
/* Error Code constant for invalid handle in kpt crypto service */

#define ERR_CODE_KPT_CRYPTO_SERVICE_FAIL_HMAC_FAILED -21
/* Error Code constant for failed hmac in kpt crypto service */

#define ERR_CODE_KPT_CRYPTO_SERVICE_FAIL_INVALID_WRAPPING_ALGO -22
/* Error Code constant for invalid wrapping algo in kpt crypto service */

#define ERR_CODE_KPT_DRNG_SEED_NOT_LOAD -23
/* Error Code constant for no drng seed is not loaded in kpt ecdsa signrs
/service */

#define FW_LA_ICV_VER_STATUS_PASS FW_COMN_STATUS_FLAG_OK
/* Status flag indicating that the ICV verification passed  */

#define FW_LA_ICV_VER_STATUS_FAIL FW_COMN_STATUS_FLAG_ERROR
/* Status flag indicating that the ICV verification failed  */

#define FW_LA_TRNG_STATUS_PASS FW_COMN_STATUS_FLAG_OK
/* Status flag indicating that the TRNG returned valid entropy data  */

#define FW_LA_TRNG_STATUS_FAIL FW_COMN_STATUS_FLAG_ERROR
/* Status flag indicating that the TRNG Command Failed.  */

/* -------------------------------------------------------------------------- */

/*
 * Definition of the full bulk processing request structure.
 * Used for hash, cipher, hash-cipher and authentication-encryption
 * requests etc.
 */
struct fw_la_bulk_req
{
	/* LWs 0-1 */
	struct fw_comn_req_hdr comn_hdr;
	/* Common request header - for Service Command Id,
	 * use service-specific Crypto Command Id.
	 * Service Specific Flags - use Symmetric Crypto Command Flags
	 * (all of cipher, auth, SSL3, TLS and MGF,
	 * excluding TRNG - field unused) */

	/* LWs 2-5 */
	union fw_comn_req_hdr_cd_pars cd_pars;
	/* Common Request content descriptor field which points either to a
	 * content descriptor
	 * parameter block or contains the service-specific data itself. */

	/* LWs 6-13 */
	struct fw_comn_req_mid comn_mid;
	/* Common request middle section */

	/* LWs 14-26 */
	struct fw_comn_req_rqpars serv_specif_rqpars;
	/* Common request service-specific parameter field */

	/* LWs 27-31 */
	struct fw_comn_req_cd_ctrl cd_ctrl;
	/* Common request content descriptor control block -
	 * this field is service-specific */

};

/* clang-format off */

/*
 *  LA BULK (SYMMETRIC CRYPTO) COMMAND FLAGS
 *
 *  + ===== + ---------- + ----- + ----- + ----- + ----- + ----- + ----- + ----- + ----- + ----- + ----- +
 *  |  Bit  |   [15:13]  |  12   |  11   |  10   |  7-9  |   6   |   5   |   4   |  3    |   2   |  1-0  |
 *  + ===== + ---------- + ----- + ----- + ----- + ----- + ----- + ----- + ----- + ----- + ------+ ----- +
 *  | Flags | Resvd Bits | ZUC   | GcmIV |Digest | Prot  | Cmp   | Rtn   | Upd   | Ciph/ | CiphIV| Part- |
 *  |       |     =0     | Prot  | Len   | In Buf| flgs  | Auth  | Auth  | State | Auth  | Field |  ial  |
 *  + ===== + ---------- + ----- + ----- + ----- + ----- + ----- + ----- + ----- + ----- + ------+ ----- +
 */

/* clang-format on */

/* Private defines */

#define FW_LA_ZUC_3G_PROTO			__BIT(12)
/* Indicating ZUC processing for a encrypt command
 * Must be set for Cipher-only, Cipher + Auth and Auth-only   */

#define FW_LA_GCM_IV_LEN_12_OCTETS		__BIT(11)
/* Indicates the IV Length for GCM protocol is 96 Bits (12 Octets)
 * If set FW does the padding to compute CTR0  */

#define FW_LA_DIGEST_IN_BUFFER			__BIT(10)
/* Flag representing that authentication digest is stored or is extracted
 * from the source buffer. Auth Result Pointer will be ignored in this case.  */

#define FW_LA_PROTO				__BITS(7, 9)
#define FW_LA_PROTO_SNOW_3G			__BIT(9)
/* Indicates SNOW_3G processing for a encrypt command  */
#define FW_LA_PROTO_GCM				__BIT(8)
/* Indicates GCM processing for a auth_encrypt command  */
#define FW_LA_PROTO_CCM				__BIT(7)
/* Indicates CCM processing for a auth_encrypt command  */
#define FW_LA_PROTO_NONE			0
/* Indicates no specific protocol processing for the command  */

#define FW_LA_CMP_AUTH_RES			__BIT(6)
/* Flag representing the need to compare the auth result data to the expected
 * value in DRAM at the auth_address.  */

#define FW_LA_RET_AUTH_RES			__BIT(5)
/* Flag representing the need to return the auth result data to dram after the
 * request processing is complete  */

#define FW_LA_UPDATE_STATE			__BIT(4)
/* Flag representing the need to update the state data in dram after the
 * request processing is complete  */

#define FW_CIPH_AUTH_CFG_OFFSET_IN_SHRAM_CP	__BIT(3)
/* Flag representing Cipher/Auth Config Offset Type, where the offset
 * is contained in SHRAM constants page. When the SHRAM constants page
 * is not used for cipher/auth configuration, then the Content Descriptor
 * pointer field must be a pointer (as opposed to a 16-byte key), since
 * the block pointed to must contain both the slice config and the key  */

#define FW_CIPH_IV_16BYTE_DATA			__BIT(2)
/* Flag representing Cipher IV field contents as 16-byte data array
 * Otherwise Cipher IV field contents via 64-bit pointer */

#define FW_LA_PARTIAL				__BITS(0, 1)
#define FW_LA_PARTIAL_NONE			0
/* Flag representing no need for partial processing condition i.e.
 * entire packet processed in the current command  */
#define FW_LA_PARTIAL_START			1
/* Flag representing the first chunk of the partial packet  */
#define FW_LA_PARTIAL_MID			3
/* Flag representing a middle chunk of the partial packet  */
#define FW_LA_PARTIAL_END			2
/* Flag representing the final/end chunk of the partial packet  */

/* The table below defines the meaning of the prefix_addr & hash_state_sz in
 * the case of partial processing. See the HLD for further details
 *
 *  + ====== + ------------------------- + ----------------------- +
 *  | Parial |       Prefix Addr         |       Hash State Sz     |
 *  | State  |                           |                         |
 *  + ====== + ------------------------- + ----------------------- +
 *  |  FULL  | Points to the prefix data | Prefix size as below.   |
 *  |        |                           | No update of state      |
 *  + ====== + ------------------------- + ----------------------- +
 *  |  SOP   | Points to the prefix      | = inner prefix rounded  |
 *  |        | data. State is updated    | to qwrds + outer prefix |
 *  |        | at prefix_addr - state_sz | rounded to qwrds. The   |
 *  |        | - 8 (counter size)        | writeback state sz      |
 *  |        |                           | comes from the CD       |
 *  + ====== + ------------------------- + ----------------------- +
 *  |  MOP   | Points to the state data  | State size rounded to   |
 *  |        | Updated state written to  | num qwrds + 8 (for the  |
 *  |        | same location             | counter) + inner prefix |
 *  |        |                           | rounded to qwrds +      |
 *  |        |                           | outer prefix rounded to |
 *  |        |                           | qwrds.                  |
 *  + ====== + ------------------------- + ----------------------- +
 *  |  EOP   | Points to the state data  | State size rounded to   |
 *  |        |                           | num qwrds + 8 (for the  |
 *  |        |                           | counter) + inner prefix |
 *  |        |                           | rounded to qwrds +      |
 *  |        |                           | outer prefix rounded to |
 *  |        |                           | qwrds.                  |
 *  + ====== + ------------------------- + ----------------------- +
 *
 *  Notes:
 *
 *  - If the EOP is set it is assumed that no state update is to be performed.
 *    However it is the clients responsibility to set the update_state flag
 *    correctly i.e. not set for EOP or Full packet cases. Only set for SOP and
 *    MOP with no EOP flag
 *  - The SOP take precedence over the MOP and EOP i.e. in the calculation of
 *    the address to writeback the state.
 *  - The prefix address must be on at least the 8 byte boundary
 */

/* Macros for extracting field bits */
/*
 *   Macro for extraction of the Cipher IV field contents (bit 2)
 *
 * flags        Flags to extract the Cipher IV field contents
 *
 */
#define FW_LA_CIPH_IV_FLD_FLAG_GET(flags)	\
	FIELD_GET(flags, LA_CIPH_IV_FLD_BITPOS, LA_CIPH_IV_FLD_MASK)

/*
 *   Macro for extraction of the Cipher/Auth Config
 *   offset type (bit 3)
 *
 * flags        Flags to extract the Cipher/Auth Config offset type
 *
 */
#define FW_LA_CIPH_AUTH_CFG_OFFSET_FLAG_GET(flags)	\
	FIELD_GET(flags,	\
	    LA_CIPH_AUTH_CFG_OFFSET_BITPOS,	\
	    LA_CIPH_AUTH_CFG_OFFSET_MASK)

/*
 * Macro for extraction of the ZUC protocol bit
 * information (bit 11)
 *
 * flags        Flags to extract the ZUC protocol bit
 */
#define FW_LA_ZUC_3G_PROTO_FLAG_GET(flags)	\
	FIELD_GET(flags,	\
	    FW_LA_ZUC_3G_PROTO_FLAG_BITPOS,	\
	    FW_LA_ZUC_3G_PROTO_FLAG_MASK)

/*
 * Macro for extraction of the GCM IV Len is 12 Octets / 96 Bits
 * information (bit 11)
 *
 * flags        Flags to extract the GCM IV length
 */
#define FW_LA_GCM_IV_LEN_FLAG_GET(flags)	\
	FIELD_GET(	\
		flags, LA_GCM_IV_LEN_FLAG_BITPOS, LA_GCM_IV_LEN_FLAG_MASK)

/*
 * Macro for extraction of the LA protocol state (bits 9-7)
 *
 * flags        Flags to extract the protocol state
 */
#define FW_LA_PROTO_GET(flags)	\
	FIELD_GET(flags, LA_PROTO_BITPOS, LA_PROTO_MASK)

/*
 * Macro for extraction of the "compare auth" state (bit 6)
 *
 * flags        Flags to extract the compare auth result state
 *
 */
#define FW_LA_CMP_AUTH_GET(flags)	\
	FIELD_GET(flags, LA_CMP_AUTH_RES_BITPOS, LA_CMP_AUTH_RES_MASK)

/*
 * Macro for extraction of the "return auth" state (bit 5)
 *
 * flags        Flags to extract the return auth result state
 *
 */
#define FW_LA_RET_AUTH_GET(flags)	\
	FIELD_GET(flags, LA_RET_AUTH_RES_BITPOS, LA_RET_AUTH_RES_MASK)

/*
 * Macro for extraction of the "digest in buffer" state (bit 10)
 *
 * flags     Flags to extract the digest in buffer state
 *
 */
#define FW_LA_DIGEST_IN_BUFFER_GET(flags)	\
	FIELD_GET(	\
		flags, LA_DIGEST_IN_BUFFER_BITPOS, LA_DIGEST_IN_BUFFER_MASK)

/*
 * Macro for extraction of the update content state value. (bit 4)
 *
 * flags        Flags to extract the update content state bit
 */
#define FW_LA_UPDATE_STATE_GET(flags)	\
	FIELD_GET(flags, LA_UPDATE_STATE_BITPOS, LA_UPDATE_STATE_MASK)

/*
 * Macro for extraction of the "partial" packet state (bits 1-0)
 *
 * flags        Flags to extract the partial state
 */
#define FW_LA_PARTIAL_GET(flags)	\
	FIELD_GET(flags, LA_PARTIAL_BITPOS, LA_PARTIAL_MASK)

/* Macros for setting field bits */
/*
 *   Macro for setting the Cipher IV field contents
 *
 * flags        Flags to set with the Cipher IV field contents
 * val          Field contents indicator value
 */
#define FW_LA_CIPH_IV_FLD_FLAG_SET(flags, val)	\
	FIELD_SET(	\
		flags, val, LA_CIPH_IV_FLD_BITPOS, LA_CIPH_IV_FLD_MASK)

/*
 * Macro for setting the Cipher/Auth Config
 * offset type
 *
 * flags        Flags to set the Cipher/Auth Config offset type
 * val          Offset type value
 */
#define FW_LA_CIPH_AUTH_CFG_OFFSET_FLAG_SET(flags, val)	\
	FIELD_SET(flags,	\
	    val,	\
	    LA_CIPH_AUTH_CFG_OFFSET_BITPOS,	\
	    LA_CIPH_AUTH_CFG_OFFSET_MASK)

/*
 * Macro for setting the ZUC protocol flag
 *
 * flags      Flags to set the ZUC protocol flag
 * val        Protocol value
 */
#define FW_LA_ZUC_3G_PROTO_FLAG_SET(flags, val)	\
	FIELD_SET(flags,	\
	    val,	\
	    FW_LA_ZUC_3G_PROTO_FLAG_BITPOS,	\
	    FW_LA_ZUC_3G_PROTO_FLAG_MASK)

/*
 * Macro for setting the GCM IV length flag state
 *
 * flags      Flags to set the GCM IV length flag state
 * val        Protocol value
 */
#define FW_LA_GCM_IV_LEN_FLAG_SET(flags, val)	\
	FIELD_SET(flags,	\
	    val,	\
	    LA_GCM_IV_LEN_FLAG_BITPOS,	\
	    LA_GCM_IV_LEN_FLAG_MASK)

/*
 * Macro for setting the LA protocol flag state
 *
 * flags        Flags to set the protocol state
 * val          Protocol value
 */
#define FW_LA_PROTO_SET(flags, val)	\
	FIELD_SET(flags, val, LA_PROTO_BITPOS, LA_PROTO_MASK)

/*
 * Macro for setting the "compare auth" flag state
 *
 * flags      Flags to set the compare auth result state
 * val        Compare Auth value
 */
#define FW_LA_CMP_AUTH_SET(flags, val)	\
	FIELD_SET(	\
		flags, val, LA_CMP_AUTH_RES_BITPOS, LA_CMP_AUTH_RES_MASK)

/*
 * Macro for setting the "return auth" flag state
 *
 * flags      Flags to set the return auth result state
 * val        Return Auth value
 */
#define FW_LA_RET_AUTH_SET(flags, val)	\
	FIELD_SET(	\
		flags, val, LA_RET_AUTH_RES_BITPOS, LA_RET_AUTH_RES_MASK)

/*
 * Macro for setting the "digest in buffer" flag state
 *
 * flags     Flags to set the digest in buffer state
 * val       Digest in buffer value
 */
#define FW_LA_DIGEST_IN_BUFFER_SET(flags, val)	\
	FIELD_SET(flags,	\
	    val,	\
	    LA_DIGEST_IN_BUFFER_BITPOS,	\
	    LA_DIGEST_IN_BUFFER_MASK)

/*
 *   Macro for setting the "update state" flag value
 *
 * flags      Flags to set the update content state
 * val        Update Content State flag value
 */
#define FW_LA_UPDATE_STATE_SET(flags, val)	\
	FIELD_SET(	\
		flags, val, LA_UPDATE_STATE_BITPOS, LA_UPDATE_STATE_MASK)

/*
 *   Macro for setting the "partial" packet flag state
 *
 * flags      Flags to set the partial state
 * val        Partial state value
 */
#define FW_LA_PARTIAL_SET(flags, val)	\
	FIELD_SET(flags, val, LA_PARTIAL_BITPOS, LA_PARTIAL_MASK)

/*
 * Definition of the Cipher header Content Descriptor pars block
 * Definition of the cipher processing header cd pars block.
 * The structure is a service-specific implementation of the common
 * 'fw_comn_req_hdr_cd_pars_s' structure.
 */
union fw_cipher_req_hdr_cd_pars {
	/* LWs 2-5 */
	struct
	{
		uint64_t content_desc_addr;
		/* Address of the content descriptor */

		uint16_t content_desc_resrvd1;
		/* Content descriptor reserved field */

		uint8_t content_desc_params_sz;
		/* Size of the content descriptor parameters in quad words. These
		 * parameters describe the session setup configuration info for the
		 * slices that this request relies upon i.e. the configuration word and
		 * cipher key needed by the cipher slice if there is a request for
		 * cipher processing. */

		uint8_t content_desc_hdr_resrvd2;
		/* Content descriptor reserved field */

		uint32_t content_desc_resrvd3;
		/* Content descriptor reserved field */
	} s;

	struct
	{
		uint32_t cipher_key_array[FW_NUM_LONGWORDS_4];
		/* Cipher Key Array */

	} s1;

};

/*
 * Definition of the Authentication header Content Descriptor pars block
 * Definition of the authentication processing header cd pars block.
 */
/* Note: Authentication uses the common 'fw_comn_req_hdr_cd_pars_s'
 * structure - similarly, it is also used by SSL3, TLS and MGF. Only cipher
 * and cipher + authentication require service-specific implementations of
 * the structure  */

/*
 * Definition of the Cipher + Auth header Content Descriptor pars block
 * Definition of the cipher + auth processing header cd pars block.
 * The structure is a service-specific implementation of the common
 * 'fw_comn_req_hdr_cd_pars_s' structure.
 */
union fw_cipher_auth_req_hdr_cd_pars {
	/* LWs 2-5 */
	struct
	{
		uint64_t content_desc_addr;
		/* Address of the content descriptor */

		uint16_t content_desc_resrvd1;
		/* Content descriptor reserved field */

		uint8_t content_desc_params_sz;
		/* Size of the content descriptor parameters in quad words. These
		 * parameters describe the session setup configuration info for the
		 * slices that this request relies upon i.e. the configuration word and
		 * cipher key needed by the cipher slice if there is a request for
		 * cipher processing. */

		uint8_t content_desc_hdr_resrvd2;
		/* Content descriptor reserved field */

		uint32_t content_desc_resrvd3;
		/* Content descriptor reserved field */
	} s;

	struct
	{
		uint32_t cipher_key_array[FW_NUM_LONGWORDS_4];
		/* Cipher Key Array */

	} sl;

};

/*
 * Cipher content descriptor control block (header)
 * Definition of the service-specific cipher control block header
 * structure. This header forms part of the content descriptor
 * block incorporating LWs 27-31, as defined by the common base
 * parameters structure.
 */
struct fw_cipher_cd_ctrl_hdr
{
	/* LW 27 */
	uint8_t cipher_state_sz;
	/* State size in quad words of the cipher algorithm used in this session.
	 * Set to zero if the algorithm doesnt provide any state */

	uint8_t cipher_key_sz;
	/* Key size in quad words of the cipher algorithm used in this session */

	uint8_t cipher_cfg_offset;
	/* Quad word offset from the content descriptor parameters address i.e.
	 * (content_address + (cd_hdr_sz << 3)) to the parameters for the cipher
	 * processing */

	uint8_t next_curr_id;
	/* This field combines the next and current id (each four bits) -
	  * the next id is the most significant nibble.
	  * Next Id:  Set to the next slice to pass the ciphered data through.
	  * Set to FW_SLICE_DRAM_WR if the data is not to go through
	  * any more slices after cipher.
	  * Current Id: Initialised with the cipher  slice type */

	/* LW 28 */
	uint8_t cipher_padding_sz;
	/* State padding size in quad words. Set to 0 if no padding is required.
	 */

	uint8_t resrvd1;
	uint16_t resrvd2;
	/* Reserved bytes to bring the struct to the word boundary, used by
	 * authentication. MUST be set to 0 */

	/* LWs 29-31 */
	uint32_t resrvd3[FW_NUM_LONGWORDS_3];
	/* Reserved bytes used by authentication. MUST be set to 0 */

};

/*
 * Authentication content descriptor control block (header)
 * Definition of the service-specific authentication control block
 * header structure. This header forms part of the content descriptor
 * block incorporating LWs 27-31, as defined by the common base
 * parameters structure, the first portion of which is reserved for
 * cipher.
 */
struct fw_auth_cd_ctrl_hdr
{
	/* LW 27 */
	uint32_t resrvd1;
	/* Reserved bytes, used by cipher only. MUST be set to 0 */

	/* LW 28 */
	uint8_t resrvd2;
	/* Reserved byte, used by cipher only. MUST be set to 0 */

	uint8_t hash_flags;
	/* General flags defining the processing to perform. 0 is normal
	 * processing
	 * and 1 means there is a nested hash processing loop to go through */

	uint8_t hash_cfg_offset;
	/* Quad word offset from the content descriptor parameters address to the
	 * parameters for the auth processing */

	uint8_t next_curr_id;
	/* This field combines the next and current id (each four bits) -
	  * the next id is the most significant nibble.
	  * Next Id:  Set to the next slice to pass the authentication data through.
	  * Set to FW_SLICE_DRAM_WR if the data is not to go through
	  * any more slices after authentication.
	  * Current Id: Initialised with the authentication slice type */

	/* LW 29 */
	uint8_t resrvd3;
	/* Now a reserved field. MUST be set to 0 */

	uint8_t outer_prefix_sz;
	/* Size in bytes of outer prefix data */

	uint8_t final_sz;
	/* Size in bytes of digest to be returned to the client if requested */

	uint8_t inner_res_sz;
	/* Size in bytes of the digest from the inner hash algorithm */

	/* LW 30 */
	uint8_t resrvd4;
	/* Now a reserved field. MUST be set to zero. */

	uint8_t inner_state1_sz;
	/* Size in bytes of inner hash state1 data. Must be a qword multiple */

	uint8_t inner_state2_offset;
	/* Quad word offset from the content descriptor parameters pointer to the
	 * inner state2 value */

	uint8_t inner_state2_sz;
	/* Size in bytes of inner hash state2 data. Must be a qword multiple */

	/* LW 31 */
	uint8_t outer_config_offset;
	/* Quad word offset from the content descriptor parameters pointer to the
	 * outer configuration information */

	uint8_t outer_state1_sz;
	/* Size in bytes of the outer state1 value */

	uint8_t outer_res_sz;
	/* Size in bytes of digest from the outer auth algorithm */

	uint8_t outer_prefix_offset;
	/* Quad word offset from the start of the inner prefix data to the outer
	 * prefix information. Should equal the rounded inner prefix size, converted
	 * to qwords  */

};

/*
 * Cipher + Authentication content descriptor control block header
 * Definition of both service-specific cipher + authentication control
 * block header structures. This header forms part of the content
 * descriptor block incorporating LWs 27-31, as defined by the common
 * base  parameters structure.
 */
struct fw_cipher_auth_cd_ctrl_hdr
{
	/* LW 27 */
	uint8_t cipher_state_sz;
	/* State size in quad words of the cipher algorithm used in this session.
	 * Set to zero if the algorithm doesnt provide any state */

	uint8_t cipher_key_sz;
	/* Key size in quad words of the cipher algorithm used in this session */

	uint8_t cipher_cfg_offset;
	/* Quad word offset from the content descriptor parameters address i.e.
	 * (content_address + (cd_hdr_sz << 3)) to the parameters for the cipher
	 * processing */

	uint8_t next_curr_id_cipher;
	/* This field combines the next and current id (each four bits) -
	  * the next id is the most significant nibble.
	  * Next Id:  Set to the next slice to pass the ciphered data through.
	  * Set to FW_SLICE_DRAM_WR if the data is not to go through
	  * any more slices after cipher.
	  * Current Id: Initialised with the cipher  slice type */

	/* LW 28 */
	uint8_t cipher_padding_sz;
	/* State padding size in quad words. Set to 0 if no padding is required.
	 */

	uint8_t hash_flags;
	/* General flags defining the processing to perform. 0 is normal
	 * processing
	 * and 1 means there is a nested hash processing loop to go through */

	uint8_t hash_cfg_offset;
	/* Quad word offset from the content descriptor parameters address to the
	 * parameters for the auth processing */

	uint8_t next_curr_id_auth;
	/* This field combines the next and current id (each four bits) -
	  * the next id is the most significant nibble.
	  * Next Id:  Set to the next slice to pass the authentication data through.
	  * Set to FW_SLICE_DRAM_WR if the data is not to go through
	  * any more slices after authentication.
	  * Current Id: Initialised with the authentication slice type */

	/* LW 29 */
	uint8_t resrvd1;
	/* Reserved field. MUST be set to 0 */

	uint8_t outer_prefix_sz;
	/* Size in bytes of outer prefix data */

	uint8_t final_sz;
	/* Size in bytes of digest to be returned to the client if requested */

	uint8_t inner_res_sz;
	/* Size in bytes of the digest from the inner hash algorithm */

	/* LW 30 */
	uint8_t resrvd2;
	/* Now a reserved field. MUST be set to zero. */

	uint8_t inner_state1_sz;
	/* Size in bytes of inner hash state1 data. Must be a qword multiple */

	uint8_t inner_state2_offset;
	/* Quad word offset from the content descriptor parameters pointer to the
	 * inner state2 value */

	uint8_t inner_state2_sz;
	/* Size in bytes of inner hash state2 data. Must be a qword multiple */

	/* LW 31 */
	uint8_t outer_config_offset;
	/* Quad word offset from the content descriptor parameters pointer to the
	 * outer configuration information */

	uint8_t outer_state1_sz;
	/* Size in bytes of the outer state1 value */

	uint8_t outer_res_sz;
	/* Size in bytes of digest from the outer auth algorithm */

	uint8_t outer_prefix_offset;
	/* Quad word offset from the start of the inner prefix data to the outer
	 * prefix information. Should equal the rounded inner prefix size, converted
	 * to qwords  */

};

#define FW_AUTH_HDR_FLAG_DO_NESTED 1
/* Definition of the hash_flags bit of the auth_hdr to indicate the request
 * requires nested hashing  */

#define FW_AUTH_HDR_FLAG_NO_NESTED 0
/* Definition of the hash_flags bit of the auth_hdr for no nested hashing
 * required  */

#define FW_CCM_GCM_AAD_SZ_MAX 240
/* Maximum size of AAD data allowed for CCM or GCM processing. AAD data size90 -
 * is stored in 8-bit field and must be multiple of hash block size. 240 is
 * largest value which satisfy both requirements.AAD_SZ_MAX is in byte units  */

/*
 * request parameter #defines
 */
#define FW_HASH_REQUEST_PARAMETERS_OFFSET	\
	(sizeof(fw_la_cipher_req_params_t))
/* Offset in bytes from the start of the request parameters block to the hash
 * (auth) request parameters  */

#define FW_CIPHER_REQUEST_PARAMETERS_OFFSET (0)
/* Offset in bytes from the start of the request parameters block to the cipher
 * request parameters  */

/*
 * Definition of the cipher request parameters block
 *
 * Definition of the cipher processing request parameters block
 * structure, which forms part of the block incorporating LWs 14-26,
 * as defined by the common base parameters structure.
 * Unused fields must be set to 0.
 */
struct fw_la_cipher_req_params {
	/* LW 14 */
	uint32_t cipher_offset;
	/* Cipher offset long word. */

	/* LW 15 */
	uint32_t cipher_length;
	/* Cipher length long word. */

	/* LWs 16-19 */
	union {
		uint32_t cipher_IV_array[FW_NUM_LONGWORDS_4];
		/* Cipher IV array  */

		struct
		{
			uint64_t cipher_IV_ptr;
			/* Cipher IV pointer or Partial State Pointer */

			uint64_t resrvd1;
			/* reserved */

		} s;

	} u;

};

/*
 * Definition of the auth request parameters block
 * Definition of the authentication processing request parameters block
 * structure, which forms part of the block incorporating LWs 14-26,
 * as defined by the common base parameters structure. Note:
 * This structure is used by TLS only.
 */
struct fw_la_auth_req_params {
	/* LW 20 */
	uint32_t auth_off;
	/* Byte offset from the start of packet to the auth data region */

	/* LW 21 */
	uint32_t auth_len;
	/* Byte length of the auth data region */

	/* LWs 22-23 */
	union {
		uint64_t auth_partial_st_prefix;
		/* Address of the authentication partial state prefix
		 * information */

		uint64_t aad_adr;
		/* Address of the AAD info in DRAM. Used for the CCM and GCM
		 * protocols */

	} u1;

	/* LWs 24-25 */
	uint64_t auth_res_addr;
	/* Address of the authentication result information to validate or
	 * the location to which the digest information can be written back to */

	/* LW 26 */
	union {
		uint8_t inner_prefix_sz;
		/* Size in bytes of the inner prefix data */

		uint8_t aad_sz;
		/* Size in bytes of padded AAD data to prefix to the packet for CCM
		 *  or GCM processing */
	} u2;

	uint8_t resrvd1;
	/* reserved */

	uint8_t hash_state_sz;
	/* Number of quad words of inner and outer hash prefix data to process
	 * Maximum size is 240 */

	uint8_t auth_res_sz;
	/* Size in bytes of the authentication result */

} __packed;

/*
 * Definition of the auth request parameters block
 * Definition of the authentication processing request parameters block
 * structure, which forms part of the block incorporating LWs 14-26,
 * as defined by the common base parameters structure. Note:
 * This structure is used by SSL3 and MGF1 only. All fields other than
 * inner prefix/ AAD size are unused and therefore reserved.
 */
struct fw_la_auth_req_params_resrvd_flds {
	/* LWs 20-25 */
	uint32_t resrvd[FW_NUM_LONGWORDS_6];

	/* LW 26 */
	union {
		uint8_t inner_prefix_sz;
		/* Size in bytes of the inner prefix data */

		uint8_t aad_sz;
		/* Size in bytes of padded AAD data to prefix to the packet for CCM
		 *  or GCM processing */
	} u2;

	uint8_t resrvd1;
	/* reserved */

	uint16_t resrvd2;
	/* reserved */
};

/*
 *   Definition of the shared fields within the parameter block
 *   containing SSL, TLS or MGF information.
 *   This structure defines the shared fields for SSL, TLS or MGF
 *   within the parameter block incorporating LWs 14-26, as defined
 *   by the common base parameters structure.
 *   Unused fields must be set to 0.
 */
struct fw_la_key_gen_common {
	/* LW 14 */
	union {
		/* SSL3 */
		uint16_t secret_lgth_ssl;
		/* Length of Secret information for SSL. In the case of TLS the
		* secret is supplied in the content descriptor */

		/* MGF */
		uint16_t mask_length;
		/* Size in bytes of the desired output mask for MGF1*/

		/* TLS */
		uint16_t secret_lgth_tls;
		/* TLS Secret length */

	} u;

	union {
		/* SSL3 */
		struct
		{
			uint8_t output_lgth_ssl;
			/* Output length */

			uint8_t label_lgth_ssl;
			/* Label length */

		} s1;

		/* MGF */
		struct
		{
			uint8_t hash_length;
			/* Hash length */

			uint8_t seed_length;
			/* Seed length */

		} s2;

		/* TLS */
		struct
		{
			uint8_t output_lgth_tls;
			/* Output length */

			uint8_t label_lgth_tls;
			/* Label length */

		} s3;

	} u1;

	/* LW 15 */
	union {
		/* SSL3 */
		uint8_t iter_count;
		/* Iteration count used by the SSL key gen request */

		/* TLS */
		uint8_t tls_seed_length;
		/* TLS Seed length */

		uint8_t resrvd1;
		/* Reserved field set to 0 for MGF1 */

	} u2;

	uint8_t resrvd2;
	uint16_t resrvd3;
	/* Reserved space - unused */

};

/*
 *   Definition of the SSL3 request parameters block
 *   This structure contains the the SSL3 processing request parameters
 *   incorporating LWs 14-26, as defined by the common base
 *   parameters structure. Unused fields must be set to 0.
 */
struct fw_la_ssl3_req_params {
	/* LWs 14-15 */
	struct fw_la_key_gen_common keygen_comn;
	/* For other key gen processing these field holds ssl, tls or mgf
	 *   parameters */

	/* LW 16-25 */
	uint32_t resrvd[FW_NUM_LONGWORDS_10];
	/* Reserved */

	/* LW 26 */
	union {
		uint8_t inner_prefix_sz;
		/* Size in bytes of the inner prefix data */

		uint8_t aad_sz;
		/* Size in bytes of padded AAD data to prefix to the packet for CCM
		 *  or GCM processing */
	} u2;

	uint8_t resrvd1;
	/* reserved */

	uint16_t resrvd2;
	/* reserved */

};

/*
 * Definition of the MGF request parameters block
 * This structure contains the the MGF processing request parameters
 * incorporating LWs 14-26, as defined by the common base parameters
 * structure. Unused fields must be set to 0.
 */
struct fw_la_mgf_req_params {
	/* LWs 14-15 */
	struct fw_la_key_gen_common keygen_comn;
	/* For other key gen processing these field holds ssl or mgf
	 *   parameters */

	/* LW 16-25 */
	uint32_t resrvd[FW_NUM_LONGWORDS_10];
	/* Reserved */

	/* LW 26 */
	union {
		uint8_t inner_prefix_sz;
		/* Size in bytes of the inner prefix data */

		uint8_t aad_sz;
		/* Size in bytes of padded AAD data to prefix to the packet for CCM
		 *  or GCM processing */
	} u2;

	uint8_t resrvd1;
	/* reserved */

	uint16_t resrvd2;
	/* reserved */

};

/*
 * Definition of the TLS request parameters block
 * This structure contains the the TLS processing request parameters
 * incorporating LWs 14-26, as defined by the common base parameters
 * structure. Unused fields must be set to 0.
 */
struct fw_la_tls_req_params {
	/* LWs 14-15 */
	struct fw_la_key_gen_common keygen_comn;
	/* For other key gen processing these field holds ssl, tls or mgf
	 *   parameters */

	/* LW 16-19 */
	uint32_t resrvd[FW_NUM_LONGWORDS_4];
	/* Reserved */

};

/*
 * Definition of the common QAT FW request middle block for TRNG.
 * Common section of the request used across all of the services exposed
 * by the QAT FW. Each of the services inherit these common fields. TRNG
 * requires a specific implementation.
 */
struct fw_la_trng_req_mid {
	/* LWs 6-13 */
	uint64_t opaque_data;
	/* Opaque data passed unmodified from the request to response messages by
	 * firmware (fw) */

	uint64_t resrvd1;
	/* Reserved, unused for TRNG */

	uint64_t dest_data_addr;
	/* Generic definition of the destination data supplied to the QAT AE. The
	 * common flags are used to further describe the attributes of this
	 * field */

	uint32_t resrvd2;
	/* Reserved, unused for TRNG */

	uint32_t entropy_length;
	/* Size of the data in bytes to process. Used by the get_random
	 * command. Set to 0 for commands that dont need a length parameter */

};

/*
 * Definition of the common LA QAT FW TRNG request
 * Definition of the TRNG processing request type
 */
struct fw_la_trng_req {
	/* LWs 0-1 */
	struct fw_comn_req_hdr comn_hdr;
	/* Common request header */

	/* LWs 2-5 */
	union fw_comn_req_hdr_cd_pars cd_pars;
	/* Common Request content descriptor field which points either to a
	 * content descriptor
	 * parameter block or contains the service-specific data itself. */

	/* LWs 6-13 */
	struct fw_la_trng_req_mid comn_mid;
	/* TRNG request middle section - differs from the common mid-section */

	/* LWs 14-26 */
	uint32_t resrvd1[FW_NUM_LONGWORDS_13];

	/* LWs 27-31 */
	uint32_t resrvd2[FW_NUM_LONGWORDS_5];

};

/*
 * Definition of the Lookaside Eagle Tail Response
 * This is the response delivered to the ET rings by the Lookaside
 * QAT FW service for all commands
 */
struct fw_la_resp {
	/* LWs 0-1 */
	struct fw_comn_resp_hdr comn_resp;
	/* Common interface response format see fw.h */

	/* LWs 2-3 */
	uint64_t opaque_data;
	/* Opaque data passed from the request to the response message */

	/* LWs 4-7 */
	uint32_t resrvd[FW_NUM_LONGWORDS_4];
	/* Reserved */

};

/*
 *   Definition of the Lookaside TRNG Test Status Structure
 *   As an addition to FW_LA_TRNG_STATUS Pass or Fail information
 *   in common response fields, as a response to TRNG_TEST request, Test
 *   status, Counter for failed tests and 4 entropy counter values are
 *   sent
 *   Status of test status and the fail counts.
 */
struct fw_la_trng_test_result {
	uint32_t test_status_info;
	/* TRNG comparator health test status& Validity information
	see Test Status Bit Fields below. */

	uint32_t test_status_fail_count;
	/* TRNG comparator health test status, 32bit fail counter */

	uint64_t r_ent_ones_cnt;
	/* Raw Entropy ones counter */

	uint64_t r_ent_zeros_cnt;
	/* Raw Entropy zeros counter */

	uint64_t c_ent_ones_cnt;
	/* Conditioned Entropy ones counter */

	uint64_t c_ent_zeros_cnt;
	/* Conditioned Entropy zeros counter */

	uint64_t resrvd;
	/* Reserved field must be set to zero */

};

/*
 * Definition of the Lookaside SSL Key Material Input
 * This struct defines the layout of input parameters for the
 * SSL3 key generation (source flat buffer format)
 */
struct fw_la_ssl_key_material_input {
	uint64_t seed_addr;
	/* Pointer to seed */

	uint64_t label_addr;
	/* Pointer to label(s) */

	uint64_t secret_addr;
	/* Pointer to secret */

};

/*
 * Definition of the Lookaside TLS Key Material Input
 * This struct defines the layout of input parameters for the
 * TLS key generation (source flat buffer format)
 * NOTE:
 * Secret state value (S split into S1 and S2 parts) is supplied via
 * Content Descriptor. S1 is placed in an outer prefix buffer, and S2
 * inside the inner prefix buffer.
 */
struct fw_la_tls_key_material_input {
	uint64_t seed_addr;
	/* Pointer to seed */

	uint64_t label_addr;
	/* Pointer to label(s) */

};

/*
 * Macros using the bit position and mask to set/extract the next
 * and current id nibbles within the next_curr_id field of the
 * content descriptor header block, ONLY FOR CIPHER + AUTH COMBINED.
 * Note that for cipher only or authentication only, the common macros
 * need to be used. These are defined in the 'fw.h' common header
 * file, as they are used by compression, cipher and authentication.
 *
 * cd_ctrl_hdr_t      Content descriptor control block header.
 * val                Value of the field being set.
 */
/* Cipher fields within Cipher + Authentication structure */
#define FW_CIPHER_NEXT_ID_GET(cd_ctrl_hdr_t)	\
	((((cd_ctrl_hdr_t)->next_curr_id_cipher) &	\
	  FW_COMN_NEXT_ID_MASK) >>	\
	 (FW_COMN_NEXT_ID_BITPOS))

#define FW_CIPHER_NEXT_ID_SET(cd_ctrl_hdr_t, val)	\
	(cd_ctrl_hdr_t)->next_curr_id_cipher =	\
		((((cd_ctrl_hdr_t)->next_curr_id_cipher) &	\
		  FW_COMN_CURR_ID_MASK) |	\
		 ((val << FW_COMN_NEXT_ID_BITPOS) &	\
		  FW_COMN_NEXT_ID_MASK))

#define FW_CIPHER_CURR_ID_GET(cd_ctrl_hdr_t)	\
	(((cd_ctrl_hdr_t)->next_curr_id_cipher) & FW_COMN_CURR_ID_MASK)

#define FW_CIPHER_CURR_ID_SET(cd_ctrl_hdr_t, val)	\
	(cd_ctrl_hdr_t)->next_curr_id_cipher =	\
		((((cd_ctrl_hdr_t)->next_curr_id_cipher) &	\
		  FW_COMN_NEXT_ID_MASK) |	\
		 ((val)&FW_COMN_CURR_ID_MASK))

/* Authentication fields within Cipher + Authentication structure */
#define FW_AUTH_NEXT_ID_GET(cd_ctrl_hdr_t)	\
	((((cd_ctrl_hdr_t)->next_curr_id_auth) & FW_COMN_NEXT_ID_MASK) >>	\
	 (FW_COMN_NEXT_ID_BITPOS))

#define FW_AUTH_NEXT_ID_SET(cd_ctrl_hdr_t, val)	\
	(cd_ctrl_hdr_t)->next_curr_id_auth =	\
		((((cd_ctrl_hdr_t)->next_curr_id_auth) &	\
		  FW_COMN_CURR_ID_MASK) |	\
		 ((val << FW_COMN_NEXT_ID_BITPOS) &	\
		  FW_COMN_NEXT_ID_MASK))

#define FW_AUTH_CURR_ID_GET(cd_ctrl_hdr_t)	\
	(((cd_ctrl_hdr_t)->next_curr_id_auth) & FW_COMN_CURR_ID_MASK)

#define FW_AUTH_CURR_ID_SET(cd_ctrl_hdr_t, val)	\
	(cd_ctrl_hdr_t)->next_curr_id_auth =	\
		((((cd_ctrl_hdr_t)->next_curr_id_auth) &	\
		  FW_COMN_NEXT_ID_MASK) |	\
		 ((val)&FW_COMN_CURR_ID_MASK))

/*  Definitions of the bits in the test_status_info of the TRNG_TEST response.
 *  The values returned by the Lookaside service are given below
 *  The Test result and Test Fail Count values are only valid if the Test
 *  Results Valid (Tv) is set.
 *
 *  TRNG Test Status Info
 *  + ===== + ------------------------------------------------ + --- + --- +
 *  |  Bit  |                   31 - 2                         |  1  |  0  |
 *  + ===== + ------------------------------------------------ + --- + --- +
 *  | Flags |                 RESERVED = 0                     | Tv  | Ts  |
 *  + ===== + ------------------------------------------------------------ +
 */
/*
 *   Definition of the Lookaside TRNG Test Status Information received as
 *   a part of fw_la_trng_test_result_t
 *
 */
#define FW_LA_TRNG_TEST_STATUS_TS_BITPOS 0
/* TRNG Test Result t_status field bit pos definition. */

#define FW_LA_TRNG_TEST_STATUS_TS_MASK 0x1
/* TRNG Test Result t_status field mask definition. */

#define FW_LA_TRNG_TEST_STATUS_TV_BITPOS 1
/* TRNG Test Result test results valid field bit pos definition. */

#define FW_LA_TRNG_TEST_STATUS_TV_MASK 0x1
/* TRNG Test Result test results valid field mask definition. */

/*
 *   Definition of the Lookaside TRNG test_status values.
 *
 *
 */
#define FW_LA_TRNG_TEST_STATUS_TV_VALID 1
/* TRNG TEST Response Test Results Valid Value. */

#define FW_LA_TRNG_TEST_STATUS_TV_NOT_VALID 0
/* TRNG TEST Response Test Results are NOT Valid Value. */

#define FW_LA_TRNG_TEST_STATUS_TS_NO_FAILS 1
/* Value for TRNG Test status tests have NO FAILs Value. */

#define FW_LA_TRNG_TEST_STATUS_TS_HAS_FAILS 0
/* Value for TRNG Test status tests have one or more FAILS Value. */

/*
 *  Macro for extraction of the Test Status Field returned in the response
 *  to TRNG TEST command.
 *
 * test_status        8 bit test_status value to extract the status bit
 */
#define FW_LA_TRNG_TEST_STATUS_TS_FLD_GET(test_status)	\
	FIELD_GET(test_status,	\
			      FW_LA_TRNG_TEST_STATUS_TS_BITPOS,	\
			      FW_LA_TRNG_TEST_STATUS_TS_MASK)
/*
 *  Macro for extraction of the Test Results Valid Field returned in the
 *  response to TRNG TEST command.
 *
 * test_status        8 bit test_status value to extract the Tests
 *         Results valid bit
 */
#define FW_LA_TRNG_TEST_STATUS_TV_FLD_GET(test_status)	\
	FIELD_GET(test_status,	\
			      FW_LA_TRNG_TEST_STATUS_TV_BITPOS,	\
			      FW_LA_TRNG_TEST_STATUS_TV_MASK)

/*
 * MGF Max supported input parameters
 */
#define FW_LA_MGF_SEED_LEN_MAX 255
/* Maximum seed length for MGF1 request in bytes
 * Typical values may be 48, 64, 128 bytes (or any). */

#define FW_LA_MGF_MASK_LEN_MAX 65528
/* Maximum mask length for MGF1 request in bytes
 * Typical values may be 8 (64-bit), 16 (128-bit). MUST be quad word multiple  */

/*
 * SSL Max supported input parameters
 */
#define FW_LA_SSL_SECRET_LEN_MAX 512
/* Maximum secret length for SSL3 Key Gen request (bytes)  */

#define FW_LA_SSL_ITERATES_LEN_MAX 16
/* Maximum iterations for SSL3 Key Gen request (integer)  */

#define FW_LA_SSL_LABEL_LEN_MAX 136
/* Maximum label length for SSL3 Key Gen request (bytes)  */

#define FW_LA_SSL_SEED_LEN_MAX 64
/* Maximum seed length for SSL3 Key Gen request (bytes)  */

#define FW_LA_SSL_OUTPUT_LEN_MAX 248
/* Maximum output length for SSL3 Key Gen request (bytes)  */

/*
 * TLS Max supported input parameters
 */
#define FW_LA_TLS_SECRET_LEN_MAX 128
/* Maximum secret length for TLS Key Gen request (bytes)  */

#define FW_LA_TLS_V1_1_SECRET_LEN_MAX 128
/* Maximum secret length for TLS Key Gen request (bytes)  */

#define FW_LA_TLS_V1_2_SECRET_LEN_MAX 64
/* Maximum secret length for TLS Key Gen request (bytes)  */

#define FW_LA_TLS_LABEL_LEN_MAX 255
/* Maximum label length for TLS Key Gen request (bytes)  */

#define FW_LA_TLS_SEED_LEN_MAX 64
/* Maximum seed length for TLS Key Gen request (bytes)  */

#define FW_LA_TLS_OUTPUT_LEN_MAX 248
/* Maximum output length for TLS Key Gen request (bytes)  */

#endif
