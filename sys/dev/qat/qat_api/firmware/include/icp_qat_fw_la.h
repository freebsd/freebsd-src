/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/**
 *****************************************************************************
 * @file icp_qat_fw_la.h
 * @defgroup icp_qat_fw_la ICP QAT FW Lookaside Service Interface Definitions
 * @ingroup icp_qat_fw
 * @description
 *      This file documents structs used to provided the interface to the
 *      LookAside (LA) QAT FW service
 *
 *****************************************************************************/

#ifndef _ICP_QAT_FW_LA_H_
#define _ICP_QAT_FW_LA_H_

/*
******************************************************************************
* Include local header files
******************************************************************************
*/
#include "icp_qat_fw.h"

/* ========================================================================= */
/*                           QAT FW REQUEST STRUCTURES                       */
/* ========================================================================= */

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_la
 *        Definition of the LookAside (LA) command types
 * @description
 *        Enumeration which is used to indicate the ids of functions
 *        that are exposed by the LA QAT FW service
 *
 *****************************************************************************/

typedef enum {
	ICP_QAT_FW_LA_CMD_CIPHER = 0,
	/*!< Cipher Request */

	ICP_QAT_FW_LA_CMD_AUTH = 1,
	/*!< Auth Request */

	ICP_QAT_FW_LA_CMD_CIPHER_HASH = 2,
	/*!< Cipher-Hash Request */

	ICP_QAT_FW_LA_CMD_HASH_CIPHER = 3,
	/*!< Hash-Cipher Request */

	ICP_QAT_FW_LA_CMD_TRNG_GET_RANDOM = 4,
	/*!< TRNG Get Random Request */

	ICP_QAT_FW_LA_CMD_TRNG_TEST = 5,
	/*!< TRNG Test Request */

	ICP_QAT_FW_LA_CMD_SSL3_KEY_DERIVE = 6,
	/*!< SSL3 Key Derivation Request */

	ICP_QAT_FW_LA_CMD_TLS_V1_1_KEY_DERIVE = 7,
	/*!< TLS Key Derivation Request */

	ICP_QAT_FW_LA_CMD_TLS_V1_2_KEY_DERIVE = 8,
	/*!< TLS Key Derivation Request */

	ICP_QAT_FW_LA_CMD_MGF1 = 9,
	/*!< MGF1 Request */

	ICP_QAT_FW_LA_CMD_AUTH_PRE_COMP = 10,
	/*!< Auth Pre-Compute Request */

	ICP_QAT_FW_LA_CMD_CIPHER_PRE_COMP = 11,
	/*!< Auth Pre-Compute Request */

	ICP_QAT_FW_LA_CMD_HKDF_EXTRACT = 12,
	/*!< HKDF Extract Request */

	ICP_QAT_FW_LA_CMD_HKDF_EXPAND = 13,
	/*!< HKDF Expand Request */

	ICP_QAT_FW_LA_CMD_HKDF_EXTRACT_AND_EXPAND = 14,
	/*!< HKDF Extract and Expand Request */

	ICP_QAT_FW_LA_CMD_HKDF_EXPAND_LABEL = 15,
	/*!< HKDF Expand Label Request */

	ICP_QAT_FW_LA_CMD_HKDF_EXTRACT_AND_EXPAND_LABEL = 16,
	/*!< HKDF Extract and Expand Label Request */

	ICP_QAT_FW_LA_CMD_DELIMITER = 17
	/**< Delimiter type */
} icp_qat_fw_la_cmd_id_t;

typedef struct icp_qat_fw_la_cipher_20_req_params_s {
	/**< LW 14 */
	uint32_t cipher_offset;
	/**< Cipher offset long word. */

	/**< LW 15 */
	uint32_t cipher_length;
	/**< Cipher length long word. */

	/**< LWs 16-19 */
	union {
		uint32_t cipher_IV_array[ICP_QAT_FW_NUM_LONGWORDS_4];
		/**< Cipher IV array  */

		struct {
			uint64_t cipher_IV_ptr;
			/**< Cipher IV pointer or Partial State Pointer */

			uint64_t resrvd1;
			/**< reserved */

		} s;

	} u;
	/**< LW 20 */
	uint32_t spc_aad_offset;
	/**< LW 21 */
	uint32_t spc_aad_sz;
	/**< LW 22 - 23 */
	uint64_t spc_aad_addr;
	/**< LW 24 - 25 */
	uint64_t spc_auth_res_addr;
	/**< LW 26 */
	uint8_t reserved[3];
	uint8_t spc_auth_res_sz;

} icp_qat_fw_la_cipher_20_req_params_t;

/*  For the definitions of the bits in the status field of the common
 *  response, refer to icp_qat_fw.h.
 *  The return values specific to Lookaside service are given below.
 */
#define ICP_QAT_FW_LA_ICV_VER_STATUS_PASS ICP_QAT_FW_COMN_STATUS_FLAG_OK
/**< @ingroup icp_qat_fw_la
 * Status flag indicating that the ICV verification passed */

#define ICP_QAT_FW_LA_ICV_VER_STATUS_FAIL ICP_QAT_FW_COMN_STATUS_FLAG_ERROR
/**< @ingroup icp_qat_fw_la
 * Status flag indicating that the ICV verification failed */

#define ICP_QAT_FW_LA_TRNG_STATUS_PASS ICP_QAT_FW_COMN_STATUS_FLAG_OK
/**< @ingroup icp_qat_fw_la
 * Status flag indicating that the TRNG returned valid entropy data */

#define ICP_QAT_FW_LA_TRNG_STATUS_FAIL ICP_QAT_FW_COMN_STATUS_FLAG_ERROR
/**< @ingroup icp_qat_fw_la
 * Status flag indicating that the TRNG Command Failed. */

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_la
 *        Definition of the common LA QAT FW bulk request
 * @description
 *        Definition of the full bulk processing request structure.
 *        Used for hash, cipher, hash-cipher and authentication-encryption
 *        requests etc.
 *
 *****************************************************************************/
typedef struct icp_qat_fw_la_bulk_req_s {
	/**< LWs 0-1 */
	icp_qat_fw_comn_req_hdr_t comn_hdr;
	/**< Common request header - for Service Command Id,
	 * use service-specific Crypto Command Id.
	 * Service Specific Flags - use Symmetric Crypto Command Flags
	 * (all of cipher, auth, SSL3, TLS and MGF,
	 * excluding TRNG - field unused) */

	/**< LWs 2-5 */
	icp_qat_fw_comn_req_hdr_cd_pars_t cd_pars;
	/**< Common Request content descriptor field which points either to a
	 * content descriptor
	 * parameter block or contains the service-specific data itself. */

	/**< LWs 6-13 */
	icp_qat_fw_comn_req_mid_t comn_mid;
	/**< Common request middle section */

	/**< LWs 14-26 */
	icp_qat_fw_comn_req_rqpars_t serv_specif_rqpars;
	/**< Common request service-specific parameter field */

	/**< LWs 27-31 */
	icp_qat_fw_comn_req_cd_ctrl_t cd_ctrl;
	/**< Common request content descriptor control block -
	 * this field is service-specific */

} icp_qat_fw_la_bulk_req_t;

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

/* Private defines */

/* bits 15:14  */
#define ICP_QAT_FW_LA_USE_WIRELESS_SLICE_TYPE 2
/**< @ingroup icp_qat_fw_la
 * FW Selects Wireless Cipher Slice
 *   Cipher Algorithms: AES-{F8}, Snow3G, ZUC
 *   Auth Algorithms  : Snow3G, ZUC */

#define ICP_QAT_FW_LA_USE_UCS_SLICE_TYPE 1
/**< @ingroup icp_qat_fw_la
 * FW Selects UCS Cipher Slice
 *   Cipher Algorithms: AES-{CTR/XTS}, Single Pass AES-GCM
 *   Auth Algorithms  : SHA1/ SHA{2/3}-{224/256/384/512} */

#define ICP_QAT_FW_LA_USE_LEGACY_SLICE_TYPE 0
/**< @ingroup icp_qat_fw_la
 * FW Selects Legacy Cipher/Auth Slice
 *   Cipher Algorithms: AES-{CBC/ECB}, SM4, Single Pass AES-CCM
 *   Auth Algorithms  : SHA1/ SHA{2/3}-{224/256/384/512} */

#define QAT_LA_SLICE_TYPE_BITPOS 14
/**< @ingroup icp_qat_fw_la
 * Starting bit position for the slice type selection.
 * Refer to HAS for Slice type assignment details on QAT2.0 */

#define QAT_LA_SLICE_TYPE_MASK 0x3
/**< @ingroup icp_qat_fw_la
 * Two bit mask used to determine the Slice type  */

/* bit 11 */
#define ICP_QAT_FW_LA_GCM_IV_LEN_12_OCTETS 1
/**< @ingroup icp_qat_fw_la
 * Indicates the IV Length for GCM protocol is 96 Bits (12 Octets)
 * If set FW does the padding to compute CTR0 */

#define ICP_QAT_FW_LA_GCM_IV_LEN_NOT_12_OCTETS 0
/**< @ingroup icp_qat_fw_la
 * Indicates the IV Length for GCM protocol is not 96 Bits (12 Octets)
 * If IA computes CTR0 */

#define QAT_FW_LA_ZUC_3G_PROTO_FLAG_BITPOS 12
/**< @ingroup icp_cpm_fw_la
 * Bit position defining ZUC processing for a encrypt command */

#define ICP_QAT_FW_LA_ZUC_3G_PROTO 1
/**< @ingroup icp_cpm_fw_la
 * Value indicating ZUC processing for a encrypt command */

#define QAT_FW_LA_ZUC_3G_PROTO_FLAG_MASK 0x1
/**< @ingroup icp_qat_fw_la
 * One bit mask used to determine the ZUC 3G protocol bit.
 * Must be set for Cipher-only, Cipher + Auth and Auth-only  */

#define QAT_FW_LA_SINGLE_PASS_PROTO_FLAG_BITPOS 13
/**< @ingroup icp_cpm_fw_la
 * Bit position defining SINGLE PASS processing for a encrypt command */

#define ICP_QAT_FW_LA_SINGLE_PASS_PROTO 1
/**< @ingroup icp_cpm_fw_la
 * Value indicating SINGLE PASS processing for a encrypt command */

#define QAT_FW_LA_SINGLE_PASS_PROTO_FLAG_MASK 0x1
/**< @ingroup icp_qat_fw_la
 * One bit mask used to determine the SINGLE PASS protocol bit.
 * Must be set for Cipher-only */

#define QAT_LA_GCM_IV_LEN_FLAG_BITPOS 11
/**< @ingroup icp_qat_fw_la
 * Starting bit position for GCM IV Length indication. If set
 * the IV Length is 96 Bits, clear for other IV lengths  */

#define QAT_LA_GCM_IV_LEN_FLAG_MASK 0x1
/**< @ingroup icp_qat_fw_la
 * One bit mask used to determine the GCM IV Length indication bit.
 * If set the IV Length is 96 Bits, clear for other IV lengths  */

/* bit 10 */
#define ICP_QAT_FW_LA_DIGEST_IN_BUFFER 1
/**< @ingroup icp_qat_fw_la
 * Flag representing that authentication digest is stored or is extracted
 * from the source buffer. Auth Result Pointer will be ignored in this case. */

#define ICP_QAT_FW_LA_NO_DIGEST_IN_BUFFER 0
/**< @ingroup icp_qat_fw_la
 * Flag representing that authentication digest is NOT stored or is NOT
 * extracted from the source buffer. Auth result will get stored or extracted
 * from the Auth Result Pointer. Please not that in this case digest CANNOT be
 * encrypted. */

#define QAT_LA_DIGEST_IN_BUFFER_BITPOS 10
/**< @ingroup icp_qat_fw_la
 * Starting bit position for Digest in Buffer flag */

#define QAT_LA_DIGEST_IN_BUFFER_MASK 0x1
/**< @ingroup icp_qat_fw_la
 * One bit mask used to determine the Digest in Buffer flag */

/* bits 7-9 */
#define ICP_QAT_FW_LA_SNOW_3G_PROTO 4
/**< @ingroup icp_cpm_fw_la
 * Indicates SNOW_3G processing for a encrypt command */

#define ICP_QAT_FW_LA_GCM_PROTO 2
/**< @ingroup icp_qat_fw_la
 * Indicates GCM processing for a auth_encrypt command */

#define ICP_QAT_FW_LA_CCM_PROTO 1
/**< @ingroup icp_qat_fw_la
 * Indicates CCM processing for a auth_encrypt command */

#define ICP_QAT_FW_LA_NO_PROTO 0
/**< @ingroup icp_qat_fw_la
 * Indicates no specific protocol processing for the command */

#define QAT_LA_PROTO_BITPOS 7
/**< @ingroup icp_qat_fw_la
 * Starting bit position for the Lookaside Protocols */

#define QAT_LA_PROTO_MASK 0x7
/**< @ingroup icp_qat_fw_la
 * Three bit mask used to determine the Lookaside Protocol  */

/* bit 6 */
#define ICP_QAT_FW_LA_CMP_AUTH_RES 1
/**< @ingroup icp_qat_fw_la
 * Flag representing the need to compare the auth result data to the expected
 * value in DRAM at the auth_address. */

#define ICP_QAT_FW_LA_NO_CMP_AUTH_RES 0
/**< @ingroup icp_qat_fw_la
 * Flag representing that there is no need to do a compare of the auth data
 * to the expected value */

#define QAT_LA_CMP_AUTH_RES_BITPOS 6
/**< @ingroup icp_qat_fw_la
 * Starting bit position for Auth compare digest result */

#define QAT_LA_CMP_AUTH_RES_MASK 0x1
/**< @ingroup icp_qat_fw_la
 * One bit mask used to determine the Auth compare digest result */

/* bit 5 */
#define ICP_QAT_FW_LA_RET_AUTH_RES 1
/**< @ingroup icp_qat_fw_la
 * Flag representing the need to return the auth result data to dram after the
 * request processing is complete */

#define ICP_QAT_FW_LA_NO_RET_AUTH_RES 0
/**< @ingroup icp_qat_fw_la
 * Flag representing that there is no need to return the auth result data */

#define QAT_LA_RET_AUTH_RES_BITPOS 5
/**< @ingroup icp_qat_fw_la
 * Starting bit position for Auth return digest result */

#define QAT_LA_RET_AUTH_RES_MASK 0x1
/**< @ingroup icp_qat_fw_la
 * One bit mask used to determine the Auth return digest result */

/* bit 4 */
#define ICP_QAT_FW_LA_UPDATE_STATE 1
/**< @ingroup icp_qat_fw_la
 * Flag representing the need to update the state data in dram after the
 * request processing is complete */

#define ICP_QAT_FW_LA_NO_UPDATE_STATE 0
/**< @ingroup icp_qat_fw_la
 * Flag representing that there is no need to update the state data */

#define QAT_LA_UPDATE_STATE_BITPOS 4
/**< @ingroup icp_qat_fw_la
 * Starting bit position for Update State. */

#define QAT_LA_UPDATE_STATE_MASK 0x1
/**< @ingroup icp_qat_fw_la
 * One bit mask used to determine the Update State */

/* bit 3 */
#define ICP_QAT_FW_CIPH_AUTH_CFG_OFFSET_IN_CD_SETUP 0
/**< @ingroup icp_qat_fw_la
 * Flag representing Cipher/Auth Config Offset Type, where the offset
 * is contained in CD Setup. When the SHRAM constants page
 * is not used for cipher/auth configuration, then the Content Descriptor
 * pointer field must be a pointer (as opposed to a 16-byte key), since
 * the block pointed to must contain both the slice config and the key */

#define ICP_QAT_FW_CIPH_AUTH_CFG_OFFSET_IN_SHRAM_CP 1
/**< @ingroup icp_qat_fw_la
 * Flag representing Cipher/Auth Config Offset Type, where the offset
 * is contained in SHRAM constants page. */

#define QAT_LA_CIPH_AUTH_CFG_OFFSET_BITPOS 3
/**< @ingroup icp_qat_fw_la
 * Starting bit position indicating Cipher/Auth Config
 * offset type */

#define QAT_LA_CIPH_AUTH_CFG_OFFSET_MASK 0x1
/**< @ingroup icp_qat_fw_la
 * One bit mask used to determine Cipher/Auth Config
 * offset type */

/* bit 2 */
#define ICP_QAT_FW_CIPH_IV_64BIT_PTR 0
/**< @ingroup icp_qat_fw_la
 * Flag representing Cipher IV field contents via 64-bit pointer */

#define ICP_QAT_FW_CIPH_IV_16BYTE_DATA 1
/**< @ingroup icp_qat_fw_la
 * Flag representing Cipher IV field contents as 16-byte data array */

#define QAT_LA_CIPH_IV_FLD_BITPOS 2
/**< @ingroup icp_qat_fw_la
 * Starting bit position indicating Cipher IV field
 * contents */

#define QAT_LA_CIPH_IV_FLD_MASK 0x1
/**< @ingroup icp_qat_fw_la
 * One bit mask used to determine the Cipher IV field
 * contents */

/* bits 0-1 */
#define ICP_QAT_FW_LA_PARTIAL_NONE 0
/**< @ingroup icp_qat_fw_la
 * Flag representing no need for partial processing condition i.e.
 * entire packet processed in the current command */

#define ICP_QAT_FW_LA_PARTIAL_START 1
/**< @ingroup icp_qat_fw_la
 * Flag representing the first chunk of the partial packet */

#define ICP_QAT_FW_LA_PARTIAL_MID 3
/**< @ingroup icp_qat_fw_la
 * Flag representing a middle chunk of the partial packet */

#define ICP_QAT_FW_LA_PARTIAL_END 2
/**< @ingroup icp_qat_fw_la
 * Flag representing the final/end chunk of the partial packet */

#define QAT_LA_PARTIAL_BITPOS 0
/**< @ingroup icp_qat_fw_la
 * Starting bit position indicating partial state */

#define QAT_LA_PARTIAL_MASK 0x3
/**< @ingroup icp_qat_fw_la
 * Two bit mask used to determine the partial state */

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

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 * Macro used for the generation of the Lookaside flags for a request. This
 * should always be used for the generation of the flags field. No direct sets
 * or masks should be performed on the flags data
 *
 * @param gcm_iv_len       GCM IV Length indication bit
 * @param auth_rslt        Authentication result - Digest is stored/extracted
 *                         in/from the source buffer
 *                         straight after the authenticated region
 * @param proto            Protocol handled by a command
 * @param cmp_auth         Compare auth result with the expected value
 * @param ret_auth         Return auth result to the client via DRAM
 * @param update_state     Indicate update of the crypto state information
 *                         is required
 * @param ciphIV           Cipher IV field contents
 * @param ciphcfg          Cipher/Auth Config offset type
 * @param partial          Inidicate if the packet is a partial part
 *
 *****************************************************************************/
#define ICP_QAT_FW_LA_FLAGS_BUILD(zuc_proto,                                   \
				  gcm_iv_len,                                  \
				  auth_rslt,                                   \
				  proto,                                       \
				  cmp_auth,                                    \
				  ret_auth,                                    \
				  update_state,                                \
				  ciphIV,                                      \
				  ciphcfg,                                     \
				  partial)                                     \
	(((zuc_proto & QAT_FW_LA_ZUC_3G_PROTO_FLAG_MASK)                       \
	  << QAT_FW_LA_ZUC_3G_PROTO_FLAG_BITPOS) |                             \
	 ((gcm_iv_len & QAT_LA_GCM_IV_LEN_FLAG_MASK)                           \
	  << QAT_LA_GCM_IV_LEN_FLAG_BITPOS) |                                  \
	 ((auth_rslt & QAT_LA_DIGEST_IN_BUFFER_MASK)                           \
	  << QAT_LA_DIGEST_IN_BUFFER_BITPOS) |                                 \
	 ((proto & QAT_LA_PROTO_MASK) << QAT_LA_PROTO_BITPOS) |                \
	 ((cmp_auth & QAT_LA_CMP_AUTH_RES_MASK)                                \
	  << QAT_LA_CMP_AUTH_RES_BITPOS) |                                     \
	 ((ret_auth & QAT_LA_RET_AUTH_RES_MASK)                                \
	  << QAT_LA_RET_AUTH_RES_BITPOS) |                                     \
	 ((update_state & QAT_LA_UPDATE_STATE_MASK)                            \
	  << QAT_LA_UPDATE_STATE_BITPOS) |                                     \
	 ((ciphIV & QAT_LA_CIPH_IV_FLD_MASK) << QAT_LA_CIPH_IV_FLD_BITPOS) |   \
	 ((ciphcfg & QAT_LA_CIPH_AUTH_CFG_OFFSET_MASK)                         \
	  << QAT_LA_CIPH_AUTH_CFG_OFFSET_BITPOS) |                             \
	 ((partial & QAT_LA_PARTIAL_MASK) << QAT_LA_PARTIAL_BITPOS))

/* Macros for extracting field bits */
/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *        Macro for extraction of the Cipher IV field contents (bit 2)
 *
 * @param flags        Flags to extract the Cipher IV field contents
 *
 *****************************************************************************/
#define ICP_QAT_FW_LA_CIPH_IV_FLD_FLAG_GET(flags)                              \
	QAT_FIELD_GET(flags, QAT_LA_CIPH_IV_FLD_BITPOS, QAT_LA_CIPH_IV_FLD_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *        Macro for extraction of the Cipher/Auth Config
 *        offset type (bit 3)
 *
 * @param flags        Flags to extract the Cipher/Auth Config
 *                     offset type
 *
 *****************************************************************************/
#define ICP_QAT_FW_LA_CIPH_AUTH_CFG_OFFSET_FLAG_GET(flags)                     \
	QAT_FIELD_GET(flags,                                                   \
		      QAT_LA_CIPH_AUTH_CFG_OFFSET_BITPOS,                      \
		      QAT_LA_CIPH_AUTH_CFG_OFFSET_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *        Macro for extraction of the ZUC protocol bit
 *        information (bit 11)
 *
 * @param flags        Flags to extract the ZUC protocol bit
 *
 *****************************************************************************/
#define ICP_QAT_FW_LA_ZUC_3G_PROTO_FLAG_GET(flags)                             \
	QAT_FIELD_GET(flags,                                                   \
		      QAT_FW_LA_ZUC_3G_PROTO_FLAG_BITPOS,                      \
		      QAT_FW_LA_ZUC_3G_PROTO_FLAG_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *        Macro for extraction of the GCM IV Len is 12 Octets / 96 Bits
 *        information (bit 11)
 *
 * @param flags        Flags to extract the GCM IV length
 *
 *****************************************************************************/
#define ICP_QAT_FW_LA_GCM_IV_LEN_FLAG_GET(flags)                               \
	QAT_FIELD_GET(flags,                                                   \
		      QAT_LA_GCM_IV_LEN_FLAG_BITPOS,                           \
		      QAT_LA_GCM_IV_LEN_FLAG_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *        Macro for extraction of the LA protocol state (bits 9-7)
 *
 * @param flags        Flags to extract the protocol state
 *
 *****************************************************************************/
#define ICP_QAT_FW_LA_PROTO_GET(flags)                                         \
	QAT_FIELD_GET(flags, QAT_LA_PROTO_BITPOS, QAT_LA_PROTO_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *        Macro for extraction of the "compare auth" state (bit 6)
 *
 * @param flags        Flags to extract the compare auth result state
 *
 *****************************************************************************/
#define ICP_QAT_FW_LA_CMP_AUTH_GET(flags)                                      \
	QAT_FIELD_GET(flags,                                                   \
		      QAT_LA_CMP_AUTH_RES_BITPOS,                              \
		      QAT_LA_CMP_AUTH_RES_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *        Macro for extraction of the "return auth" state (bit 5)
 *
 * @param flags        Flags to extract the return auth result state
 *
 *****************************************************************************/
#define ICP_QAT_FW_LA_RET_AUTH_GET(flags)                                      \
	QAT_FIELD_GET(flags,                                                   \
		      QAT_LA_RET_AUTH_RES_BITPOS,                              \
		      QAT_LA_RET_AUTH_RES_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *      Macro for extraction of the "digest in buffer" state (bit 10)
 *
 * @param flags     Flags to extract the digest in buffer state
 *
 *****************************************************************************/
#define ICP_QAT_FW_LA_DIGEST_IN_BUFFER_GET(flags)                              \
	QAT_FIELD_GET(flags,                                                   \
		      QAT_LA_DIGEST_IN_BUFFER_BITPOS,                          \
		      QAT_LA_DIGEST_IN_BUFFER_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *        Macro for extraction of the update content state value. (bit 4)
 *
 * @param flags        Flags to extract the update content state bit
 *
 *****************************************************************************/
#define ICP_QAT_FW_LA_UPDATE_STATE_GET(flags)                                  \
	QAT_FIELD_GET(flags,                                                   \
		      QAT_LA_UPDATE_STATE_BITPOS,                              \
		      QAT_LA_UPDATE_STATE_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *        Macro for extraction of the "partial" packet state (bits 1-0)
 *
 * @param flags        Flags to extract the partial state
 *
 *****************************************************************************/
#define ICP_QAT_FW_LA_PARTIAL_GET(flags)                                       \
	QAT_FIELD_GET(flags, QAT_LA_PARTIAL_BITPOS, QAT_LA_PARTIAL_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *        Macro for extraction of the "Use Extended Protocol Flags" flag value
 *
 * @param flags      Extended Command Flags
 * @param val        Value of the flag
 *
 *****************************************************************************/
#define ICP_QAT_FW_USE_EXTENDED_PROTOCOL_FLAGS_GET(flags)                      \
	QAT_FIELD_GET(flags,                                                   \
		      QAT_LA_USE_EXTENDED_PROTOCOL_FLAGS_BITPOS,               \
		      QAT_LA_USE_EXTENDED_PROTOCOL_FLAGS_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *        Macro for extraction of the slice type information from the flags.
 *
 * @param flags        Flags to extract the protocol state
 *
 *****************************************************************************/
#define ICP_QAT_FW_LA_SLICE_TYPE_GET(flags)                                    \
	QAT_FIELD_GET(flags, QAT_LA_SLICE_TYPE_BITPOS, QAT_LA_SLICE_TYPE_MASK)

/* Macros for setting field bits */
/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *        Macro for setting the Cipher IV field contents
 *
 * @param flags        Flags to set with the Cipher IV field contents
 * @param val          Field contents indicator value
 *
 *****************************************************************************/
#define ICP_QAT_FW_LA_CIPH_IV_FLD_FLAG_SET(flags, val)                         \
	QAT_FIELD_SET(flags,                                                   \
		      val,                                                     \
		      QAT_LA_CIPH_IV_FLD_BITPOS,                               \
		      QAT_LA_CIPH_IV_FLD_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *        Macro for setting the Cipher/Auth Config
 *        offset type
 *
 * @param flags        Flags to set the Cipher/Auth Config offset type
 * @param val          Offset type value
 *
 *****************************************************************************/
#define ICP_QAT_FW_LA_CIPH_AUTH_CFG_OFFSET_FLAG_SET(flags, val)                \
	QAT_FIELD_SET(flags,                                                   \
		      val,                                                     \
		      QAT_LA_CIPH_AUTH_CFG_OFFSET_BITPOS,                      \
		      QAT_LA_CIPH_AUTH_CFG_OFFSET_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *        Macro for setting the ZUC protocol flag
 *
 * @param flags      Flags to set the ZUC protocol flag
 * @param val        Protocol value
 *
 *****************************************************************************/
#define ICP_QAT_FW_LA_ZUC_3G_PROTO_FLAG_SET(flags, val)                        \
	QAT_FIELD_SET(flags,                                                   \
		      val,                                                     \
		      QAT_FW_LA_ZUC_3G_PROTO_FLAG_BITPOS,                      \
		      QAT_FW_LA_ZUC_3G_PROTO_FLAG_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *        Macro for setting the SINGLE PASSprotocol flag
 *
 * @param flags      Flags to set the SINGLE PASS protocol flag
 * @param val        Protocol value
 *
 *****************************************************************************/
#define ICP_QAT_FW_LA_SINGLE_PASS_PROTO_FLAG_SET(flags, val)                   \
	QAT_FIELD_SET(flags,                                                   \
		      val,                                                     \
		      QAT_FW_LA_SINGLE_PASS_PROTO_FLAG_BITPOS,                 \
		      QAT_FW_LA_SINGLE_PASS_PROTO_FLAG_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *        Macro for setting the GCM IV length flag state
 *
 * @param flags      Flags to set the GCM IV length flag state
 * @param val        Protocol value
 *
 *****************************************************************************/
#define ICP_QAT_FW_LA_GCM_IV_LEN_FLAG_SET(flags, val)                          \
	QAT_FIELD_SET(flags,                                                   \
		      val,                                                     \
		      QAT_LA_GCM_IV_LEN_FLAG_BITPOS,                           \
		      QAT_LA_GCM_IV_LEN_FLAG_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *        Macro for setting the LA protocol flag state
 *
 * @param flags        Flags to set the protocol state
 * @param val          Protocol value
 *
 *****************************************************************************/
#define ICP_QAT_FW_LA_PROTO_SET(flags, val)                                    \
	QAT_FIELD_SET(flags, val, QAT_LA_PROTO_BITPOS, QAT_LA_PROTO_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *        Macro for setting the "compare auth" flag state
 *
 * @param flags      Flags to set the compare auth result state
 * @param val        Compare Auth value
 *
 *****************************************************************************/
#define ICP_QAT_FW_LA_CMP_AUTH_SET(flags, val)                                 \
	QAT_FIELD_SET(flags,                                                   \
		      val,                                                     \
		      QAT_LA_CMP_AUTH_RES_BITPOS,                              \
		      QAT_LA_CMP_AUTH_RES_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *        Macro for setting the "return auth" flag state
 *
 * @param flags      Flags to set the return auth result state
 * @param val        Return Auth value
 *
 *****************************************************************************/
#define ICP_QAT_FW_LA_RET_AUTH_SET(flags, val)                                 \
	QAT_FIELD_SET(flags,                                                   \
		      val,                                                     \
		      QAT_LA_RET_AUTH_RES_BITPOS,                              \
		      QAT_LA_RET_AUTH_RES_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *      Macro for setting the "digest in buffer" flag state
 *
 * @param flags     Flags to set the digest in buffer state
 * @param val       Digest in buffer value
 *
 *****************************************************************************/
#define ICP_QAT_FW_LA_DIGEST_IN_BUFFER_SET(flags, val)                         \
	QAT_FIELD_SET(flags,                                                   \
		      val,                                                     \
		      QAT_LA_DIGEST_IN_BUFFER_BITPOS,                          \
		      QAT_LA_DIGEST_IN_BUFFER_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *        Macro for setting the "update state" flag value
 *
 * @param flags      Flags to set the update content state
 * @param val        Update Content State flag value
 *
 *****************************************************************************/
#define ICP_QAT_FW_LA_UPDATE_STATE_SET(flags, val)                             \
	QAT_FIELD_SET(flags,                                                   \
		      val,                                                     \
		      QAT_LA_UPDATE_STATE_BITPOS,                              \
		      QAT_LA_UPDATE_STATE_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *        Macro for setting the "partial" packet flag state
 *
 * @param flags      Flags to set the partial state
 * @param val        Partial state value
 *
 *****************************************************************************/
#define ICP_QAT_FW_LA_PARTIAL_SET(flags, val)                                  \
	QAT_FIELD_SET(flags, val, QAT_LA_PARTIAL_BITPOS, QAT_LA_PARTIAL_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *        Macro for setting the "Use Extended Protocol Flags" flag value
 *
 * @param flags      Extended Command Flags
 * @param val        Value of the flag
 *
 *****************************************************************************/
#define ICP_QAT_FW_USE_EXTENDED_PROTOCOL_FLAGS_SET(flags, val)                 \
	QAT_FIELD_SET(flags,                                                   \
		      val,                                                     \
		      QAT_LA_USE_EXTENDED_PROTOCOL_FLAGS_BITPOS,               \
		      QAT_LA_USE_EXTENDED_PROTOCOL_FLAGS_MASK)

/**
******************************************************************************
* @ingroup icp_qat_fw_la
*
* @description
*        Macro for setting the "slice type" field in la flags
*
* @param flags      Flags to set the slice type
* @param val        Value of the slice type to be set.
*
*****************************************************************************/
#define ICP_QAT_FW_LA_SLICE_TYPE_SET(flags, val)                               \
	QAT_FIELD_SET(flags,                                                   \
		      val,                                                     \
		      QAT_LA_SLICE_TYPE_BITPOS,                                \
		      QAT_LA_SLICE_TYPE_MASK)

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_la
 *        Definition of the Cipher header Content Descriptor pars block
 * @description
 *      Definition of the cipher processing header cd pars block.
 *      The structure is a service-specific implementation of the common
 *      'icp_qat_fw_comn_req_hdr_cd_pars_s' structure.
 *****************************************************************************/
typedef union icp_qat_fw_cipher_req_hdr_cd_pars_s {
	/**< LWs 2-5 */
	struct {
		uint64_t content_desc_addr;
		/**< Address of the content descriptor */

		uint16_t content_desc_resrvd1;
		/**< Content descriptor reserved field */

		uint8_t content_desc_params_sz;
		/**< Size of the content descriptor parameters in quad words.
		 * These parameters describe the session setup configuration
		 * info for the slices that this request relies upon i.e. the
		 * configuration word and cipher key needed by the cipher slice
		 * if there is a request for cipher processing. */

		uint8_t content_desc_hdr_resrvd2;
		/**< Content descriptor reserved field */

		uint32_t content_desc_resrvd3;
		/**< Content descriptor reserved field */
	} s;

	struct {
		uint32_t cipher_key_array[ICP_QAT_FW_NUM_LONGWORDS_4];
		/* Cipher Key Array */

	} s1;

} icp_qat_fw_cipher_req_hdr_cd_pars_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_la
 *        Definition of the Authentication header Content Descriptor pars block
 * @description
 *      Definition of the authentication processing header cd pars block.
 *****************************************************************************/
/* Note: Authentication uses the common 'icp_qat_fw_comn_req_hdr_cd_pars_s'
 * structure - similarly, it is also used by SSL3, TLS and MGF. Only cipher
 * and cipher + authentication require service-specific implementations of
 * the structure */

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_la
 *        Definition of the Cipher + Auth header Content Descriptor pars block
 * @description
 *      Definition of the cipher + auth processing header cd pars block.
 *      The structure is a service-specific implementation of the common
 *      'icp_qat_fw_comn_req_hdr_cd_pars_s' structure.
 *****************************************************************************/
typedef union icp_qat_fw_cipher_auth_req_hdr_cd_pars_s {
	/**< LWs 2-5 */
	struct {
		uint64_t content_desc_addr;
		/**< Address of the content descriptor */

		uint16_t content_desc_resrvd1;
		/**< Content descriptor reserved field */

		uint8_t content_desc_params_sz;
		/**< Size of the content descriptor parameters in quad words.
		 * These parameters describe the session setup configuration
		 * info for the slices that this request relies upon i.e. the
		 * configuration word and cipher key needed by the cipher slice
		 * if there is a request for cipher processing. */

		uint8_t content_desc_hdr_resrvd2;
		/**< Content descriptor reserved field */

		uint32_t content_desc_resrvd3;
		/**< Content descriptor reserved field */
	} s;

	struct {
		uint32_t cipher_key_array[ICP_QAT_FW_NUM_LONGWORDS_4];
		/* Cipher Key Array */

	} sl;

} icp_qat_fw_cipher_auth_req_hdr_cd_pars_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_la
 *      Cipher content descriptor control block (header)
 * @description
 *      Definition of the service-specific cipher control block header
 *      structure. This header forms part of the content descriptor
 *      block incorporating LWs 27-31, as defined by the common base
 *      parameters structure.
 *
 *****************************************************************************/
typedef struct icp_qat_fw_cipher_cd_ctrl_hdr_s {
	/**< LW 27 */
	uint8_t cipher_state_sz;
	/**< State size in quad words of the cipher algorithm used in this
	 * session. Set to zero if the algorithm doesnt provide any state */

	uint8_t cipher_key_sz;
	/**< Key size in quad words of the cipher algorithm used in this session
	 */

	uint8_t cipher_cfg_offset;
	/**< Quad word offset from the content descriptor parameters address
	 * i.e. (content_address + (cd_hdr_sz << 3)) to the parameters for the
	 * cipher processing */

	uint8_t next_curr_id;
	/**< This field combines the next and current id (each four bits) -
	 * the next id is the most significant nibble.
	 * Next Id:  Set to the next slice to pass the ciphered data through.
	 * Set to ICP_QAT_FW_SLICE_DRAM_WR if the data is not to go through
	 * any more slices after cipher.
	 * Current Id: Initialised with the cipher  slice type */

	/**< LW 28 */
	uint8_t cipher_padding_sz;
	/**< State padding size in quad words. Set to 0 if no padding is
	 * required.
	 */

	uint8_t resrvd1;
	uint16_t resrvd2;
	/**< Reserved bytes to bring the struct to the word boundary, used by
	 * authentication. MUST be set to 0 */

	/**< LWs 29-31 */
	uint32_t resrvd3[ICP_QAT_FW_NUM_LONGWORDS_3];
	/**< Reserved bytes used by authentication. MUST be set to 0 */

} icp_qat_fw_cipher_cd_ctrl_hdr_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_la
 *      Authentication content descriptor control block (header)
 * @description
 *      Definition of the service-specific authentication control block
 *      header structure. This header forms part of the content descriptor
 *      block incorporating LWs 27-31, as defined by the common base
 *      parameters structure, the first portion of which is reserved for
 *      cipher.
 *
 *****************************************************************************/
typedef struct icp_qat_fw_auth_cd_ctrl_hdr_s {
	/**< LW 27 */
	uint32_t resrvd1;
	/**< Reserved bytes, used by cipher only. MUST be set to 0 */

	/**< LW 28 */
	uint8_t resrvd2;
	/**< Reserved byte, used by cipher only. MUST be set to 0 */

	uint8_t hash_flags;
	/**< General flags defining the processing to perform. 0 is normal
	 * processing
	 * and 1 means there is a nested hash processing loop to go through */

	uint8_t hash_cfg_offset;
	/**< Quad word offset from the content descriptor parameters address to
	 * the parameters for the auth processing */

	uint8_t next_curr_id;
	/**< This field combines the next and current id (each four bits) -
	 * the next id is the most significant nibble.
	 * Next Id:  Set to the next slice to pass the authentication data
	 * through. Set to ICP_QAT_FW_SLICE_DRAM_WR if the data is not to go
	 * through any more slices after authentication.
	 * Current Id: Initialised with the authentication slice type */

	/**< LW 29 */
	uint8_t resrvd3;
	/**< Now a reserved field. MUST be set to 0 */

	uint8_t outer_prefix_sz;
	/**< Size in bytes of outer prefix data */

	uint8_t final_sz;
	/**< Size in bytes of digest to be returned to the client if requested
	 */

	uint8_t inner_res_sz;
	/**< Size in bytes of the digest from the inner hash algorithm */

	/**< LW 30 */
	uint8_t resrvd4;
	/**< Now a reserved field. MUST be set to zero. */

	uint8_t inner_state1_sz;
	/**< Size in bytes of inner hash state1 data. Must be a qword multiple
	 */

	uint8_t inner_state2_offset;
	/**< Quad word offset from the content descriptor parameters pointer to
	 * the inner state2 value */

	uint8_t inner_state2_sz;
	/**< Size in bytes of inner hash state2 data. Must be a qword multiple
	 */

	/**< LW 31 */
	uint8_t outer_config_offset;
	/**< Quad word offset from the content descriptor parameters pointer to
	 * the outer configuration information */

	uint8_t outer_state1_sz;
	/**< Size in bytes of the outer state1 value */

	uint8_t outer_res_sz;
	/**< Size in bytes of digest from the outer auth algorithm */

	uint8_t outer_prefix_offset;
	/**< Quad word offset from the start of the inner prefix data to the
	 * outer prefix information. Should equal the rounded inner prefix size,
	 * converted to qwords  */

} icp_qat_fw_auth_cd_ctrl_hdr_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_la
 *      Cipher + Authentication content descriptor control block header
 * @description
 *      Definition of both service-specific cipher + authentication control
 *      block header structures. This header forms part of the content
 *      descriptor block incorporating LWs 27-31, as defined by the common
 *      base  parameters structure.
 *
 *****************************************************************************/
typedef struct icp_qat_fw_cipher_auth_cd_ctrl_hdr_s {
	/**< LW 27 */
	uint8_t cipher_state_sz;
	/**< State size in quad words of the cipher algorithm used in this
	 * session. Set to zero if the algorithm doesnt provide any state */

	uint8_t cipher_key_sz;
	/**< Key size in quad words of the cipher algorithm used in this session
	 */

	uint8_t cipher_cfg_offset;
	/**< Quad word offset from the content descriptor parameters address
	 * i.e. (content_address + (cd_hdr_sz << 3)) to the parameters for the
	 * cipher processing */

	uint8_t next_curr_id_cipher;
	/**< This field combines the next and current id (each four bits) -
	 * the next id is the most significant nibble.
	 * Next Id:  Set to the next slice to pass the ciphered data through.
	 * Set to ICP_QAT_FW_SLICE_DRAM_WR if the data is not to go through
	 * any more slices after cipher.
	 * Current Id: Initialised with the cipher  slice type */

	/**< LW 28 */
	uint8_t cipher_padding_sz;
	/**< State padding size in quad words. Set to 0 if no padding is
	 * required.
	 */

	uint8_t hash_flags;
	/**< General flags defining the processing to perform. 0 is normal
	 * processing
	 * and 1 means there is a nested hash processing loop to go through */

	uint8_t hash_cfg_offset;
	/**< Quad word offset from the content descriptor parameters address to
	 * the parameters for the auth processing */

	uint8_t next_curr_id_auth;
	/**< This field combines the next and current id (each four bits) -
	 * the next id is the most significant nibble.
	 * Next Id:  Set to the next slice to pass the authentication data
	 * through. Set to ICP_QAT_FW_SLICE_DRAM_WR if the data is not to go
	 * through any more slices after authentication.
	 * Current Id: Initialised with the authentication slice type */

	/**< LW 29 */
	uint8_t resrvd1;
	/**< Reserved field. MUST be set to 0 */

	uint8_t outer_prefix_sz;
	/**< Size in bytes of outer prefix data */

	uint8_t final_sz;
	/**< Size in bytes of digest to be returned to the client if requested
	 */

	uint8_t inner_res_sz;
	/**< Size in bytes of the digest from the inner hash algorithm */

	/**< LW 30 */
	uint8_t resrvd2;
	/**< Now a reserved field. MUST be set to zero. */

	uint8_t inner_state1_sz;
	/**< Size in bytes of inner hash state1 data. Must be a qword multiple
	 */

	uint8_t inner_state2_offset;
	/**< Quad word offset from the content descriptor parameters pointer to
	 * the inner state2 value */

	uint8_t inner_state2_sz;
	/**< Size in bytes of inner hash state2 data. Must be a qword multiple
	 */

	/**< LW 31 */
	uint8_t outer_config_offset;
	/**< Quad word offset from the content descriptor parameters pointer to
	 * the outer configuration information */

	uint8_t outer_state1_sz;
	/**< Size in bytes of the outer state1 value */

	uint8_t outer_res_sz;
	/**< Size in bytes of digest from the outer auth algorithm */

	uint8_t outer_prefix_offset;
	/**< Quad word offset from the start of the inner prefix data to the
	 * outer prefix information. Should equal the rounded inner prefix size,
	 * converted to qwords  */

} icp_qat_fw_cipher_auth_cd_ctrl_hdr_t;

/*
 *  HASH FLAGS
 *
 *  + ===== + --- + --- + --- + --- + --- + --- + --- + ---- +
 *  | Bit   |  7  |  6  | 5   |  4  |  3  |  2  |  1  |   0  |
 *  + ===== + --- + --- + --- + --- + --- + --- + --- + ---- +
 *  | Flags | Rsv | Rsv | Rsv | ZUC |SNOW |SKIP |SKIP |NESTED|
 *  |       |     |     |     |EIA3 | 3G  |LOAD |LOAD |      |
 *  |       |     |     |     |     |UIA2 |OUTER|INNER|      |
 *  + ===== + --- + --- + --- + --- + --- + --- + --- + ---- +
 */

/* Bit 0 */

#define QAT_FW_LA_AUTH_HDR_NESTED_BITPOS 0
/**< @ingroup icp_qat_fw_comn
 * Bit position of the hash_flags bit to indicate the request
 * requires nested hashing
 */
#define ICP_QAT_FW_AUTH_HDR_FLAG_DO_NESTED 1
/**< @ingroup icp_qat_fw_comn
 * Definition of the hash_flags bit to indicate the request
 * requires nested hashing */

#define ICP_QAT_FW_AUTH_HDR_FLAG_NO_NESTED 0
/**< @ingroup icp_qat_fw_comn
 * Definition of the hash_flags bit for no nested hashing
 * required */

#define QAT_FW_LA_AUTH_HDR_NESTED_MASK 0x1
/**< @ingroup icp_qat_fw_comn
 * Bit mask of the hash_flags bit to indicate the request
 * requires nested hashing
 */

/* Bit 1 */

#define QAT_FW_LA_SKIP_INNER_STATE1_LOAD_BITPOS 1
/**< @ingroup icp_qat_fw_comn
 * Bit position of the Skipping Inner State1 Load bit */

#define QAT_FW_LA_SKIP_INNER_STATE1_LOAD 1
/**< @ingroup icp_qat_fw_comn
 * Value indicating the skipping of inner hash state load */

#define QAT_FW_LA_NO_SKIP_INNER_STATE1_LOAD 0
/**< @ingroup icp_qat_fw_comn
 * Value indicating the no skipping of inner hash state load */

#define QAT_FW_LA_SKIP_INNER_STATE1_LOAD_MASK 0x1
/**< @ingroup icp_qat_fw_comn
 * Bit mask of Skipping Inner State1 Load bit */

/* Bit 2 */

#define QAT_FW_LA_SKIP_OUTER_STATE1_LOAD_BITPOS 2
/**< @ingroup icp_qat_fw_comn
 * Bit position of the Skipping Outer State1 Load bit */

#define QAT_FW_LA_SKIP_OUTER_STATE1_LOAD 1
/**< @ingroup icp_qat_fw_comn
 * Value indicating the skipping of outer hash state load */

#define QAT_FW_LA_NO_SKIP_OUTER_STATE1_LOAD 0
/**< @ingroup icp_qat_fw_comn
 * Value indicating the no skipping of outer hash state load */

#define QAT_FW_LA_SKIP_OUTER_STATE1_LOAD_MASK 0x1
/**< @ingroup icp_qat_fw_comn
 * Bit mask of Skipping Outer State1 Load bit */

/* Bit 3 */

#define QAT_FW_LA_SNOW3G_UIA2_BITPOS 3
/**< @ingroup icp_cpm_fw_la
 * Bit position defining hash algorithm Snow3g-UIA2 */

#define QAT_FW_LA_SNOW3G_UIA2 1
/**< @ingroup icp_cpm_fw_la
 * Value indicating the use of hash algorithm Snow3g-UIA2 */

#define QAT_FW_LA_SNOW3G_UIA2_MASK 0x1
/**< @ingroup icp_qat_fw_la
 * One bit mask used to determine the use of hash algorithm Snow3g-UIA2 */

/* Bit 4 */

#define QAT_FW_LA_ZUC_EIA3_BITPOS 4
/**< @ingroup icp_cpm_fw_la
 * Bit position defining hash algorithm ZUC-EIA3 */

#define QAT_FW_LA_ZUC_EIA3 1
/**< @ingroup icp_cpm_fw_la
 * Value indicating the use of hash algorithm ZUC-EIA3 */

#define QAT_FW_LA_ZUC_EIA3_MASK 0x1
/**< @ingroup icp_qat_fw_la
 * One bit mask used to determine the use of hash algorithm ZUC-EIA3 */

/* Bit 5 */

#define QAT_FW_LA_MODE2_BITPOS 5
/**< @ingroup icp_qat_fw_comn
 * Bit position of the Mode 2 bit */

#define QAT_FW_LA_MODE2 1
/**< @ingroup icp_qat_fw_comn
 * Value indicating the Mode 2*/

#define QAT_FW_LA_NO_MODE2 0
/**< @ingroup icp_qat_fw_comn
 * Value indicating the no Mode 2*/

#define QAT_FW_LA_MODE2_MASK 0x1
/**< @ingroup icp_qat_fw_comn
 * Bit mask of Mode 2 */

/* Macros for extracting hash flags */

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *        Macro for extraction of the "Nested" hash flag
 *
 * @param flags      Hash Flags
 * @param val        Value of the flag
 *
 *****************************************************************************/
#define ICP_QAT_FW_HASH_FLAG_AUTH_HDR_NESTED_GET(flags)                        \
	QAT_FIELD_GET(flags,                                                   \
		      QAT_FW_LA_AUTH_HDR_NESTED_BITPOS,                        \
		      QAT_FW_LA_AUTH_HDR_NESTED_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *      Macro for extraction of the "Skipping Inner State1 Load state" hash flag
 *
 * @param flags     Hash Flags
 *
 *****************************************************************************/
#define ICP_QAT_FW_HASH_FLAG_SKIP_INNER_STATE1_LOAD_GET(flags)                 \
	QAT_FIELD_GET(flags,                                                   \
		      QAT_FW_LA_SKIP_INNER_STATE1_LOAD_BITPOS,                 \
		      QAT_FW_LA_INNER_STATE1_LOAD_MASK)

/**
 ******************************************************************************
 *        Macro for setting the "Skipping Inner State1 Load" hash flag
 *
 * @param flags      Hash Flags
 * @param val        Value of the flag
 *
 *****************************************************************************/
#define ICP_QAT_FW_HASH_FLAG_SKIP_INNER_STATE1_LOAD_SET(flags, val)            \
	QAT_FIELD_SET(flags,                                                   \
		      val,                                                     \
		      QAT_FW_LA_SKIP_INNER_STATE1_LOAD_BITPOS,                 \
		      QAT_FW_LA_SKIP_INNER_STATE1_LOAD_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *      Macro for extraction of the "Skipping Outer State1 Load state" hash flag
 *
 * @param flags     Hash Flags
 *
 *****************************************************************************/
#define ICP_QAT_FW_HASH_FLAG_SKIP_OUTER_STATE1_LOAD_GET(flags)                 \
	QAT_FIELD_GET(flags,                                                   \
		      QAT_FW_LA_SKIP_OUTER_STATE1_LOAD_BITPOS,                 \
		      QAT_FW_LA_SKIP_OUTER_STATE1_LOAD_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *        Macro for setting the "Skipping Outer State1 Load" hash flag
 *
 * @param flags      Hash Flags
 * @param val        Value of the flag
 *
 *****************************************************************************/
#define ICP_QAT_FW_HASH_FLAG_SKIP_OUTER_STATE1_LOAD_SET(flags, val)            \
	QAT_FIELD_SET(flags,                                                   \
		      val,                                                     \
		      QAT_FW_LA_SKIP_OUTER_STATE1_LOAD_BITPOS,                 \
		      QAT_FW_LA_SKIP_OUTER_STATE1_LOAD_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *        Macro for extraction of the "Snow3g-UIA2" hash flag
 *
 * @param flags      Hash Flags
 * @param val        Value of the flag
 *
 *****************************************************************************/
#define ICP_QAT_FW_HASH_FLAG_SNOW3G_UIA2_GET(flags)                            \
	QAT_FIELD_GET(flags,                                                   \
		      QAT_FW_LA_SNOW3G_UIA2_BITPOS,                            \
		      QAT_FW_LA_SNOW3G_UIA2_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *        Macro for extraction of the "ZUC-EIA3" hash flag
 *
 * @param flags      Hash Flags
 * @param val        Value of the flag
 *
 *****************************************************************************/
#define ICP_QAT_FW_HASH_FLAG_ZUC_EIA3_GET(flags)                               \
	QAT_FIELD_GET(flags, QAT_FW_LA_ZUC_EIA3_BITPOS, QAT_FW_LA_ZUC_EIA3_MASK)

/* Macros for setting hash flags */

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *        Macro for setting the "Nested" hash flag
 *
 * @param flags      Hash Flags
 * @param val        Value of the flag
 *
 *****************************************************************************/
#define ICP_QAT_FW_HASH_FLAG_AUTH_HDR_NESTED_SET(flags, val)                   \
	QAT_FIELD_SET(flags,                                                   \
		      val,                                                     \
		      QAT_FW_LA_AUTH_HDR_NESTED_BITPOS,                        \
		      QAT_FW_LA_AUTH_HDR_NESTED_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *        Macro for setting the "Skipping Inner State1 Load" hash flag
 *
 * @param flags      Hash Flags
 * @param val        Value of the flag
 *
 *****************************************************************************/
#define ICP_QAT_FW_HASH_FLAG_SKIP_INNER_STATE1_LOAD_SET(flags, val)            \
	QAT_FIELD_SET(flags,                                                   \
		      val,                                                     \
		      QAT_FW_LA_SKIP_INNER_STATE1_LOAD_BITPOS,                 \
		      QAT_FW_LA_SKIP_INNER_STATE1_LOAD_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *        Macro for setting the "Skipping Outer State1 Load" hash flag
 *
 * @param flags      Hash Flags
 * @param val        Value of the flag
 *
 *****************************************************************************/
#define ICP_QAT_FW_HASH_FLAG_SKIP_OUTER_STATE1_LOAD_SET(flags, val)            \
	QAT_FIELD_SET(flags,                                                   \
		      val,                                                     \
		      QAT_FW_LA_SKIP_OUTER_STATE1_LOAD_BITPOS,                 \
		      QAT_FW_LA_SKIP_OUTER_STATE1_LOAD_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *        Macro for setting the "Snow3g-UIA2" hash flag
 *
 * @param flags      Hash Flags
 * @param val        Value of the flag
 *
 *****************************************************************************/
#define ICP_QAT_FW_HASH_FLAG_SNOW3G_UIA2_SET(flags, val)                       \
	QAT_FIELD_SET(flags,                                                   \
		      val,                                                     \
		      QAT_FW_LA_SNOW3G_UIA2_BITPOS,                            \
		      QAT_FW_LA_SNOW3G_UIA2_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *        Macro for setting the "ZUC-EIA3" hash flag
 *
 * @param flags      Hash Flags
 * @param val        Value of the flag
 *
 *****************************************************************************/
#define ICP_QAT_FW_HASH_FLAG_ZUC_EIA3_SET(flags, val)                          \
	QAT_FIELD_SET(flags,                                                   \
		      val,                                                     \
		      QAT_FW_LA_ZUC_EIA3_BITPOS,                               \
		      QAT_FW_LA_ZUC_EIA3_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *        Macro for setting the "Mode 2" hash flag
 *
 * @param flags      Hash Flags
 * @param val        Value of the flag
 *
 *****************************************************************************/
#define ICP_QAT_FW_HASH_FLAG_MODE2_SET(flags, val)                             \
	QAT_FIELD_SET(flags, val, QAT_FW_LA_MODE2_BITPOS, QAT_FW_LA_MODE2_MASK)

#define ICP_QAT_FW_CCM_GCM_AAD_SZ_MAX 240
#define ICP_QAT_FW_SPC_AAD_SZ_MAX 0x3FFF

/**< @ingroup icp_qat_fw_comn
 * Maximum size of AAD data allowed for CCM or GCM processing. AAD data size90 -
 * is stored in 8-bit field and must be multiple of hash block size. 240 is
 * largest value which satisfy both requirements.AAD_SZ_MAX is in byte units */

/*
 * request parameter #defines
 */
#define ICP_QAT_FW_HASH_REQUEST_PARAMETERS_OFFSET (24)

/**< @ingroup icp_qat_fw_comn
 * Offset in bytes from the start of the request parameters block to the hash
 * (auth) request parameters */

#define ICP_QAT_FW_CIPHER_REQUEST_PARAMETERS_OFFSET (0)
/**< @ingroup icp_qat_fw_comn
 * Offset in bytes from the start of the request parameters block to the cipher
 * request parameters */

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_la
 *      Definition of the cipher request parameters block
 *
 * @description
 *      Definition of the cipher processing request parameters block
 *      structure, which forms part of the block incorporating LWs 14-26,
 *      as defined by the common base parameters structure.
 *      Unused fields must be set to 0.
 *
 *****************************************************************************/
/**< Pack compiler directive added to prevent the
 * compiler from padding this structure to a 64-bit boundary */
#pragma pack(push, 1)
typedef struct icp_qat_fw_la_cipher_req_params_s {
	/**< LW 14 */
	uint32_t cipher_offset;
	/**< Cipher offset long word. */

	/**< LW 15 */
	uint32_t cipher_length;
	/**< Cipher length long word. */

	/**< LWs 16-19 */
	union {
		uint32_t cipher_IV_array[ICP_QAT_FW_NUM_LONGWORDS_4];
		/**< Cipher IV array  */

		struct {
			uint64_t cipher_IV_ptr;
			/**< Cipher IV pointer or Partial State Pointer */

			uint64_t resrvd1;
			/**< reserved */

		} s;

	} u;

	/* LW 20 - 21 */
	uint64_t spc_aad_addr;
	/**< Address of the AAD info in DRAM */

	/* LW 22 - 23 */
	uint64_t spc_auth_res_addr;
	/**< Address of the authentication result information to validate or
	 * the location to which the digest information can be written back to
	 */

	/* LW 24    */
	uint16_t spc_aad_sz;
	/**< Size in bytes of AAD data to prefix to the packet
	 * for ChaChaPoly or GCM processing */
	uint8_t reserved;
	/**< reserved */
	uint8_t spc_auth_res_sz;
	/**< Size in bytes of the authentication result */
} icp_qat_fw_la_cipher_req_params_t;
#pragma pack(pop)
/**
 *****************************************************************************
 * @ingroup icp_qat_fw_la
 *      Definition of the auth request parameters block
 * @description
 *      Definition of the authentication processing request parameters block
 *      structure, which forms part of the block incorporating LWs 14-26,
 *      as defined by the common base parameters structure. Note:
 *      This structure is used by TLS only.
 *
 *****************************************************************************/
/**< Pack compiler directive added to prevent the
 * compiler from padding this structure to a 64-bit boundary */
#pragma pack(push, 1)

typedef struct icp_qat_fw_la_auth_req_params_s {

	/**< LW 20 */
	uint32_t auth_off;
	/**< Byte offset from the start of packet to the auth data region */

	/**< LW 21 */
	uint32_t auth_len;
	/**< Byte length of the auth data region */

	/**< LWs 22-23 */
	union {
		uint64_t auth_partial_st_prefix;
		/**< Address of the authentication partial state prefix
		 * information */

		uint64_t aad_adr;
		/**< Address of the AAD info in DRAM. Used for the CCM and GCM
		 * protocols */

	} u1;

	/**< LWs 24-25 */
	uint64_t auth_res_addr;
	/**< Address of the authentication result information to validate or
	 * the location to which the digest information can be written back to
	 */

	/**< LW 26 */
	union {
		uint8_t inner_prefix_sz;
		/**< Size in bytes of the inner prefix data */

		uint8_t aad_sz;
		/**< Size in bytes of padded AAD data to prefix to the packet
		 * for CCM or GCM processing */
	} u2;

	uint8_t resrvd1;
	/**< reserved */

	uint8_t hash_state_sz;
	/**< Number of quad words of inner and outer hash prefix data to process
	 * Maximum size is 240 */

	uint8_t auth_res_sz;
	/**< Size in bytes of the authentication result */

} icp_qat_fw_la_auth_req_params_t;

#pragma pack(pop)

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_la
 *      Definition of the auth request parameters block
 * @description
 *      Definition of the authentication processing request parameters block
 *      structure, which forms part of the block incorporating LWs 14-26,
 *      as defined by the common base parameters structure. Note:
 *      This structure is used by SSL3 and MGF1 only. All fields other than
 *      inner prefix/ AAD size are unused and therefore reserved.
 *
 *****************************************************************************/
typedef struct icp_qat_fw_la_auth_req_params_resrvd_flds_s {
	/**< LWs 20-25 */
	uint32_t resrvd[ICP_QAT_FW_NUM_LONGWORDS_6];

	/**< LW 26 */
	union {
		uint8_t inner_prefix_sz;
		/**< Size in bytes of the inner prefix data */

		uint8_t aad_sz;
		/**< Size in bytes of padded AAD data to prefix to the packet
		 * for CCM or GCM processing */
	} u2;

	uint8_t resrvd1;
	/**< reserved */

	uint16_t resrvd2;
	/**< reserved */

} icp_qat_fw_la_auth_req_params_resrvd_flds_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_la
 *        Definition of the shared fields within the parameter block
 *        containing SSL, TLS or MGF information.
 * @description
 *        This structure defines the shared fields for SSL, TLS or MGF
 *        within the parameter block incorporating LWs 14-26, as defined
 *        by the common base parameters structure.
 *        Unused fields must be set to 0.
 *
 *****************************************************************************/
typedef struct icp_qat_fw_la_key_gen_common_s {
	/**< LW 14 */
	union {
		/**< SSL3 */
		uint16_t secret_lgth_ssl;
		/**< Length of Secret information for SSL. In the case of TLS
		 * the secret is supplied in the content descriptor */

		/**< MGF */
		uint16_t mask_length;
		/**< Size in bytes of the desired output mask for MGF1*/

		/**< TLS */
		uint16_t secret_lgth_tls;
		/**< TLS Secret length */

	} u;

	union {
		/**< SSL3 */
		struct {
			uint8_t output_lgth_ssl;
			/**< Output length */

			uint8_t label_lgth_ssl;
			/**< Label length */

		} s1;

		/**< MGF */
		struct {
			uint8_t hash_length;
			/**< Hash length */

			uint8_t seed_length;
			/**< Seed length */

		} s2;

		/**< TLS */
		struct {
			uint8_t output_lgth_tls;
			/**< Output length */

			uint8_t label_lgth_tls;
			/**< Label length */

		} s3;

		/**< HKDF */
		struct {
			uint8_t rsrvd1;
			/**< Unused */

			uint8_t info_length;
			/**< Info length. This is plain data, not wrapped in an
			 * icp_qat_fw_hkdf_label structure.
			 */

		} hkdf;

		/**< HKDF Expand Label */
		struct {
			uint8_t rsrvd1;
			/**< Unused */

			uint8_t num_labels;
			/**< Number of labels */
		} hkdf_label;

	} u1;

	/**< LW 15 */
	union {
		/**< SSL3 */
		uint8_t iter_count;
		/**< Iteration count used by the SSL key gen request */

		/**< TLS */
		uint8_t tls_seed_length;
		/**< TLS Seed length */

		/**< HKDF */
		uint8_t hkdf_ikm_length;
		/**< Input keying material (IKM) length */

		uint8_t resrvd1;
		/**< Reserved field set to 0 for MGF1 */

	} u2;

	union {
		/**< HKDF */
		uint8_t hkdf_num_sublabels;
		/**< Number of subLabels in subLabel buffer, 0-4 */

		uint8_t resrvd2;
		/**< Reserved space - unused */
	} u3;

	uint16_t resrvd3;
	/**< Reserved space - unused */

} icp_qat_fw_la_key_gen_common_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_la
 *        Definition of the SSL3 request parameters block
 * @description
 *        This structure contains the SSL3 processing request parameters
 *        incorporating LWs 14-26, as defined by the common base
 *        parameters structure. Unused fields must be set to 0.
 *
 *****************************************************************************/
typedef struct icp_qat_fw_la_ssl3_req_params_s {
	/**< LWs 14-15 */
	icp_qat_fw_la_key_gen_common_t keygen_comn;
	/**< For other key gen processing these field holds ssl, tls or mgf
	 *   parameters */

	/**< LW 16-25 */
	uint32_t resrvd[ICP_QAT_FW_NUM_LONGWORDS_10];
	/**< Reserved */

	/**< LW 26 */
	union {
		uint8_t inner_prefix_sz;
		/**< Size in bytes of the inner prefix data */

		uint8_t aad_sz;
		/**< Size in bytes of padded AAD data to prefix to the packet
		 * for CCM or GCM processing */
	} u2;

	uint8_t resrvd1;
	/**< reserved */

	uint16_t resrvd2;
	/**< reserved */

} icp_qat_fw_la_ssl3_req_params_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_la
 *        Definition of the MGF request parameters block
 * @description
 *        This structure contains the MGF processing request parameters
 *        incorporating LWs 14-26, as defined by the common base parameters
 *        structure. Unused fields must be set to 0.
 *
 *****************************************************************************/
typedef struct icp_qat_fw_la_mgf_req_params_s {
	/**< LWs 14-15 */
	icp_qat_fw_la_key_gen_common_t keygen_comn;
	/**< For other key gen processing these field holds ssl or mgf
	 *   parameters */

	/**< LW 16-25 */
	uint32_t resrvd[ICP_QAT_FW_NUM_LONGWORDS_10];
	/**< Reserved */

	/**< LW 26 */
	union {
		uint8_t inner_prefix_sz;
		/**< Size in bytes of the inner prefix data */

		uint8_t aad_sz;
		/**< Size in bytes of padded AAD data to prefix to the packet
		 * for CCM or GCM processing */
	} u2;

	uint8_t resrvd1;
	/**< reserved */

	uint16_t resrvd2;
	/**< reserved */

} icp_qat_fw_la_mgf_req_params_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_la
 *        Definition of the TLS request parameters block
 * @description
 *        This structure contains the TLS processing request parameters
 *        incorporating LWs 14-26, as defined by the common base parameters
 *        structure. Unused fields must be set to 0.
 *
 *****************************************************************************/
typedef struct icp_qat_fw_la_tls_req_params_s {
	/**< LWs 14-15 */
	icp_qat_fw_la_key_gen_common_t keygen_comn;
	/**< For other key gen processing these field holds ssl, tls or mgf
	 *   parameters */

	/**< LW 16-19 */
	uint32_t resrvd[ICP_QAT_FW_NUM_LONGWORDS_4];
	/**< Reserved */

} icp_qat_fw_la_tls_req_params_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_la
 *      Definition of the common QAT FW request middle block for TRNG.
 * @description
 *      Common section of the request used across all of the services exposed
 *      by the QAT FW. Each of the services inherit these common fields. TRNG
 *      requires a specific implementation.
 *
 *****************************************************************************/
typedef struct icp_qat_fw_la_trng_req_mid_s {
	/**< LWs 6-13 */
	uint64_t opaque_data;
	/**< Opaque data passed unmodified from the request to response messages
	 * by firmware (fw) */

	uint64_t resrvd1;
	/**< Reserved, unused for TRNG */

	uint64_t dest_data_addr;
	/**< Generic definition of the destination data supplied to the QAT AE.
	 * The common flags are used to further describe the attributes of this
	 * field */

	uint32_t resrvd2;
	/** < Reserved, unused for TRNG */

	uint32_t entropy_length;
	/**< Size of the data in bytes to process. Used by the get_random
	 * command. Set to 0 for commands that dont need a length parameter */

} icp_qat_fw_la_trng_req_mid_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_la
 *        Definition of the common LA QAT FW TRNG request
 * @description
 *        Definition of the TRNG processing request type
 *
 *****************************************************************************/
typedef struct icp_qat_fw_la_trng_req_s {
	/**< LWs 0-1 */
	icp_qat_fw_comn_req_hdr_t comn_hdr;
	/**< Common request header */

	/**< LWs 2-5 */
	icp_qat_fw_comn_req_hdr_cd_pars_t cd_pars;
	/**< Common Request content descriptor field which points either to a
	 * content descriptor
	 * parameter block or contains the service-specific data itself. */

	/**< LWs 6-13 */
	icp_qat_fw_la_trng_req_mid_t comn_mid;
	/**< TRNG request middle section - differs from the common mid-section
	 */

	/**< LWs 14-26 */
	uint32_t resrvd1[ICP_QAT_FW_NUM_LONGWORDS_13];

	/**< LWs 27-31 */
	uint32_t resrvd2[ICP_QAT_FW_NUM_LONGWORDS_5];

} icp_qat_fw_la_trng_req_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_la
 *        Definition of the Lookaside Eagle Tail Response
 * @description
 *        This is the response delivered to the ET rings by the Lookaside
 *              QAT FW service for all commands
 *
 *****************************************************************************/
typedef struct icp_qat_fw_la_resp_s {
	/**< LWs 0-1 */
	icp_qat_fw_comn_resp_hdr_t comn_resp;
	/**< Common interface response format see icp_qat_fw.h */

	/**< LWs 2-3 */
	uint64_t opaque_data;
	/**< Opaque data passed from the request to the response message */

	/**< LWs 4-7 */
	uint32_t resrvd[ICP_QAT_FW_NUM_LONGWORDS_4];
	/**< Reserved */

} icp_qat_fw_la_resp_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_la
 *        Definition of the Lookaside TRNG Test Status Structure
 * @description
 *        As an addition to ICP_QAT_FW_LA_TRNG_STATUS Pass or Fail information
 *        in common response fields, as a response to TRNG_TEST request, Test
 *        status, Counter for failed tests and 4 entropy counter values are
 *        sent
 *        Status of test status and the fail counts.
 *
 *
 *****************************************************************************/
typedef struct icp_qat_fw_la_trng_test_result_s {
	uint32_t test_status_info;
	/**< TRNG comparator health test status& Validity information
	see Test Status Bit Fields below. */

	uint32_t test_status_fail_count;
	/**< TRNG comparator health test status, 32bit fail counter */

	uint64_t r_ent_ones_cnt;
	/**< Raw Entropy ones counter */

	uint64_t r_ent_zeros_cnt;
	/**< Raw Entropy zeros counter */

	uint64_t c_ent_ones_cnt;
	/**< Conditioned Entropy ones counter */

	uint64_t c_ent_zeros_cnt;
	/**< Conditioned Entropy zeros counter */

	uint64_t resrvd;
	/**< Reserved field must be set to zero */

} icp_qat_fw_la_trng_test_result_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_la
 *      Definition of the Lookaside SSL Key Material Input
 * @description
 *      This struct defines the layout of input parameters for the
 *      SSL3 key generation (source flat buffer format)
 *
 *****************************************************************************/
typedef struct icp_qat_fw_la_ssl_key_material_input_s {
	uint64_t seed_addr;
	/**< Pointer to seed */

	uint64_t label_addr;
	/**< Pointer to label(s) */

	uint64_t secret_addr;
	/**< Pointer to secret */

} icp_qat_fw_la_ssl_key_material_input_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_la
 *      Definition of the Lookaside TLS Key Material Input
 * @description
 *      This struct defines the layout of input parameters for the
 *      TLS key generation (source flat buffer format)
 * @note
 *      Secret state value (S split into S1 and S2 parts) is supplied via
 *      Content Descriptor. S1 is placed in an outer prefix buffer, and S2
 *      inside the inner prefix buffer.
 *
 *****************************************************************************/
typedef struct icp_qat_fw_la_tls_key_material_input_s {
	uint64_t seed_addr;
	/**< Pointer to seed */

	uint64_t label_addr;
	/**< Pointer to label(s) */

} icp_qat_fw_la_tls_key_material_input_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_la
 *      Definition of the Lookaside HKDF (TLS 1.3) Key Material Input
 * @description
 *      This structure defines the source buffer for HKDF operations, which
 *      must be provided in flat buffer format.
 *
 *      The result will be returned in the destination buffer (flat format).
 *      All generated key materials will be returned in a packed layout. Where
 *      sublabel flags are specified, the result of the child expands will
 *      immediately follow their parent.
 *
 * @note
 *      TLS 1.3 / HKDF operations require only one key (either the Extract Salt
 *      or the Expand PSK) which is placed in the inner prefix buffer.
 *
 *****************************************************************************/
typedef struct icp_qat_fw_la_hkdf_key_material_input_s {
	uint64_t ikm_addr;
	/**< Pointer to IKM (input keying material) */

	uint64_t labels_addr;
	/**< Pointer to labels buffer.
	 * For HKDF Expand (without Label) this buffer contains the Info.
	 *
	 * For TLS 1.3 / HKDF Expand-Label this buffer contains up to 4
	 * icp_qat_fw_hkdf_label structures, which will result in a
	 * corresponding number of first level Expand-Label operations.
	 *
	 * For each of these operations, the result may become an input to child
	 * Expand-Label operations as specified by the sublabel flags, where bit
	 * 0 indicates a child Expand using label 0 from the sublabels buffer,
	 * bit 1 indicates sublabel 1, and so on. In this way, up to 20
	 * Expand-Label operations may be performed in one request.
	 */

	uint64_t sublabels_addr;
	/**< Pointer to 0-4 sublabels for TLS 1.3, following the format
	 * described for label_addr above. The buffer will typically contain
	 * all 4 of the supported sublabels.
	 * The sublabel flags defined for this context are as follows:
	 *  - QAT_FW_HKDF_INNER_SUBLABEL_12_BYTE_OKM_BITPOS
	 *  - QAT_FW_HKDF_INNER_SUBLABEL_16_BYTE_OKM_BITPOS
	 *  - QAT_FW_HKDF_INNER_SUBLABEL_32_BYTE_OKM_BITPOS
	 */
} icp_qat_fw_la_hkdf_key_material_input_t;

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *      Macros using the bit position and mask to set/extract the next
 *      and current id nibbles within the next_curr_id field of the
 *      content descriptor header block, ONLY FOR CIPHER + AUTH COMBINED.
 *      Note that for cipher only or authentication only, the common macros
 *      need to be used. These are defined in the 'icp_qat_fw.h' common header
 *      file, as they are used by compression, cipher and authentication.
 *
 * @param cd_ctrl_hdr_t      Content descriptor control block header.
 * @param val                Value of the field being set.
 *
 *****************************************************************************/
/** Cipher fields within Cipher + Authentication structure */
#define ICP_QAT_FW_CIPHER_NEXT_ID_GET(cd_ctrl_hdr_t)                           \
	((((cd_ctrl_hdr_t)->next_curr_id_cipher) &                             \
	  ICP_QAT_FW_COMN_NEXT_ID_MASK) >>                                     \
	 (ICP_QAT_FW_COMN_NEXT_ID_BITPOS))

#define ICP_QAT_FW_CIPHER_NEXT_ID_SET(cd_ctrl_hdr_t, val)                      \
	(cd_ctrl_hdr_t)->next_curr_id_cipher =                                 \
	    ((((cd_ctrl_hdr_t)->next_curr_id_cipher) &                         \
	      ICP_QAT_FW_COMN_CURR_ID_MASK) |                                  \
	     ((val << ICP_QAT_FW_COMN_NEXT_ID_BITPOS) &                        \
	      ICP_QAT_FW_COMN_NEXT_ID_MASK))

#define ICP_QAT_FW_CIPHER_CURR_ID_GET(cd_ctrl_hdr_t)                           \
	(((cd_ctrl_hdr_t)->next_curr_id_cipher) & ICP_QAT_FW_COMN_CURR_ID_MASK)

#define ICP_QAT_FW_CIPHER_CURR_ID_SET(cd_ctrl_hdr_t, val)                      \
	(cd_ctrl_hdr_t)->next_curr_id_cipher =                                 \
	    ((((cd_ctrl_hdr_t)->next_curr_id_cipher) &                         \
	      ICP_QAT_FW_COMN_NEXT_ID_MASK) |                                  \
	     ((val)&ICP_QAT_FW_COMN_CURR_ID_MASK))

/** Authentication fields within Cipher + Authentication structure */
#define ICP_QAT_FW_AUTH_NEXT_ID_GET(cd_ctrl_hdr_t)                             \
	((((cd_ctrl_hdr_t)->next_curr_id_auth) &                               \
	  ICP_QAT_FW_COMN_NEXT_ID_MASK) >>                                     \
	 (ICP_QAT_FW_COMN_NEXT_ID_BITPOS))

#define ICP_QAT_FW_AUTH_NEXT_ID_SET(cd_ctrl_hdr_t, val)                        \
	(cd_ctrl_hdr_t)->next_curr_id_auth =                                   \
	    ((((cd_ctrl_hdr_t)->next_curr_id_auth) &                           \
	      ICP_QAT_FW_COMN_CURR_ID_MASK) |                                  \
	     ((val << ICP_QAT_FW_COMN_NEXT_ID_BITPOS) &                        \
	      ICP_QAT_FW_COMN_NEXT_ID_MASK))

#define ICP_QAT_FW_AUTH_CURR_ID_GET(cd_ctrl_hdr_t)                             \
	(((cd_ctrl_hdr_t)->next_curr_id_auth) & ICP_QAT_FW_COMN_CURR_ID_MASK)

#define ICP_QAT_FW_AUTH_CURR_ID_SET(cd_ctrl_hdr_t, val)                        \
	(cd_ctrl_hdr_t)->next_curr_id_auth =                                   \
	    ((((cd_ctrl_hdr_t)->next_curr_id_auth) &                           \
	      ICP_QAT_FW_COMN_NEXT_ID_MASK) |                                  \
	     ((val)&ICP_QAT_FW_COMN_CURR_ID_MASK))

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
/******************************************************************************
 * @ingroup icp_qat_fw_la
 *        Definition of the Lookaside TRNG Test Status Information received as
 *        a part of icp_qat_fw_la_trng_test_result_t
 *
 *****************************************************************************/
#define QAT_FW_LA_TRNG_TEST_STATUS_TS_BITPOS 0
/**< @ingroup icp_qat_fw_la
 * TRNG Test Result t_status field bit pos definition.*/

#define QAT_FW_LA_TRNG_TEST_STATUS_TS_MASK 0x1
/**< @ingroup icp_qat_fw_la
 * TRNG Test Result t_status field mask definition.*/

#define QAT_FW_LA_TRNG_TEST_STATUS_TV_BITPOS 1
/**< @ingroup icp_qat_fw_la
 * TRNG Test Result test results valid field bit pos definition.*/

#define QAT_FW_LA_TRNG_TEST_STATUS_TV_MASK 0x1
/**< @ingroup icp_qat_fw_la
 * TRNG Test Result test results valid field mask definition.*/

/******************************************************************************
 * @ingroup icp_qat_fw_la
 *        Definition of the Lookaside TRNG test_status values.
 *
 *
 *****************************************************************************/
#define QAT_FW_LA_TRNG_TEST_STATUS_TV_VALID 1
/**< @ingroup icp_qat_fw_la
 * TRNG TEST Response Test Results Valid Value.*/

#define QAT_FW_LA_TRNG_TEST_STATUS_TV_NOT_VALID 0
/**< @ingroup icp_qat_fw_la
 * TRNG TEST Response Test Results are NOT Valid Value.*/

#define QAT_FW_LA_TRNG_TEST_STATUS_TS_NO_FAILS 1
/**< @ingroup icp_qat_fw_la
 * Value for TRNG Test status tests have NO FAILs Value.*/

#define QAT_FW_LA_TRNG_TEST_STATUS_TS_HAS_FAILS 0
/**< @ingroup icp_qat_fw_la
 * Value for TRNG Test status tests have one or more FAILS Value.*/

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *       Macro for extraction of the Test Status Field returned in the response
 *       to TRNG TEST command.
 *
 * @param test_status        8 bit test_status value to extract the status bit
 *
 *****************************************************************************/
#define ICP_QAT_FW_LA_TRNG_TEST_STATUS_TS_FLD_GET(test_status)                 \
	QAT_FIELD_GET(test_status,                                             \
		      QAT_FW_LA_TRNG_TEST_STATUS_TS_BITPOS,                    \
		      QAT_FW_LA_TRNG_TEST_STATUS_TS_MASK)
/**
 ******************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *       Macro for extraction of the Test Results Valid Field returned in the
 *       response to TRNG TEST command.
 *
 * @param test_status        8 bit test_status value to extract the Tests
 *                           Results valid bit
 *
 *****************************************************************************/
#define ICP_QAT_FW_LA_TRNG_TEST_STATUS_TV_FLD_GET(test_status)                 \
	QAT_FIELD_GET(test_status,                                             \
		      QAT_FW_LA_TRNG_TEST_STATUS_TV_BITPOS,                    \
		      QAT_FW_LA_TRNG_TEST_STATUS_TV_MASK)

/*
 ******************************************************************************
 * MGF Max supported input parameters
 ******************************************************************************
 */
#define ICP_QAT_FW_LA_MGF_SEED_LEN_MAX 255
/**< @ingroup icp_qat_fw_la
 * Maximum seed length for MGF1 request in bytes
 * Typical values may be 48, 64, 128 bytes (or any).*/

#define ICP_QAT_FW_LA_MGF_MASK_LEN_MAX 65528
/**< @ingroup icp_qat_fw_la
 * Maximum mask length for MGF1 request in bytes
 * Typical values may be 8 (64-bit), 16 (128-bit). MUST be quad word multiple */

/*
 ******************************************************************************
 * SSL Max supported input parameters
 ******************************************************************************
 */
#define ICP_QAT_FW_LA_SSL_SECRET_LEN_MAX 512
/**< @ingroup icp_qat_fw_la
 * Maximum secret length for SSL3 Key Gen request (bytes) */

#define ICP_QAT_FW_LA_SSL_ITERATES_LEN_MAX 16
/**< @ingroup icp_qat_fw_la
 * Maximum iterations for SSL3 Key Gen request (integer) */

#define ICP_QAT_FW_LA_SSL_LABEL_LEN_MAX 136
/**< @ingroup icp_qat_fw_la
 * Maximum label length for SSL3 Key Gen request (bytes) */

#define ICP_QAT_FW_LA_SSL_SEED_LEN_MAX 64
/**< @ingroup icp_qat_fw_la
 * Maximum seed length for SSL3 Key Gen request (bytes) */

#define ICP_QAT_FW_LA_SSL_OUTPUT_LEN_MAX 248
/**< @ingroup icp_qat_fw_la
 * Maximum output length for SSL3 Key Gen request (bytes) */

/*
 ******************************************************************************
 * TLS Max supported input parameters
 ******************************************************************************
 */
#define ICP_QAT_FW_LA_TLS_SECRET_LEN_MAX 128
/**< @ingroup icp_qat_fw_la
 * Maximum secret length for TLS Key Gen request (bytes) */

#define ICP_QAT_FW_LA_TLS_V1_1_SECRET_LEN_MAX 128
/**< @ingroup icp_qat_fw_la
 * Maximum secret length for TLS Key Gen request (bytes) */

#define ICP_QAT_FW_LA_TLS_V1_2_SECRET_LEN_MAX 64
/**< @ingroup icp_qat_fw_la
 * Maximum secret length for TLS Key Gen request (bytes) */

#define ICP_QAT_FW_LA_TLS_LABEL_LEN_MAX 255
/**< @ingroup icp_qat_fw_la
 * Maximum label length for TLS Key Gen request (bytes) */

#define ICP_QAT_FW_LA_TLS_SEED_LEN_MAX 64
/**< @ingroup icp_qat_fw_la
 * Maximum seed length for TLS Key Gen request (bytes) */

#define ICP_QAT_FW_LA_TLS_OUTPUT_LEN_MAX 248
/**< @ingroup icp_qat_fw_la
 * Maximum output length for TLS Key Gen request (bytes) */

/*
 ******************************************************************************
 * HKDF input parameters
 ******************************************************************************
 */

#define QAT_FW_HKDF_LABEL_BUFFER_SZ 78
#define QAT_FW_HKDF_LABEL_LEN_SZ 1
#define QAT_FW_HKDF_LABEL_FLAGS_SZ 1

#define QAT_FW_HKDF_LABEL_STRUCT_SZ                                            \
	(QAT_FW_HKDF_LABEL_BUFFER_SZ + QAT_FW_HKDF_LABEL_LEN_SZ +              \
	 QAT_FW_HKDF_LABEL_FLAGS_SZ)

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_la
 *
 * @description
 *      Wraps an RFC 8446 HkdfLabel with metadata for use in HKDF Expand-Label
 *      operations.
 *
 *****************************************************************************/
struct icp_qat_fw_hkdf_label {
	uint8_t label[QAT_FW_HKDF_LABEL_BUFFER_SZ];
	/**< Buffer containing an HkdfLabel as specified in RFC 8446 */

	uint8_t label_length;
	/**< The size of the HkdfLabel */

	union {
		uint8_t label_flags;
		/**< For first-level labels: each bit in [0..3] will trigger a
		 * child Expand-Label operation on the corresponding sublabel.
		 * Bits [4..7] are reserved.
		 */

		uint8_t sublabel_flags;
		/**< For sublabels the following flags are defined:
		 *  - QAT_FW_HKDF_INNER_SUBLABEL_12_BYTE_OKM_BITPOS
		 *  - QAT_FW_HKDF_INNER_SUBLABEL_16_BYTE_OKM_BITPOS
		 *  - QAT_FW_HKDF_INNER_SUBLABEL_32_BYTE_OKM_BITPOS
		 */
	} u;
};

#define ICP_QAT_FW_LA_HKDF_SECRET_LEN_MAX 64
/**< Maximum secret length for HKDF request (bytes) */

#define ICP_QAT_FW_LA_HKDF_IKM_LEN_MAX 64
/**< Maximum IKM length for HKDF request (bytes) */

#define QAT_FW_HKDF_MAX_LABELS 4
/**< Maximum number of label structures allowed in the labels buffer */

#define QAT_FW_HKDF_MAX_SUBLABELS 4
/**< Maximum number of label structures allowed in the sublabels buffer */

/*
 ******************************************************************************
 * HKDF inner sublabel flags
 ******************************************************************************
 */

#define QAT_FW_HKDF_INNER_SUBLABEL_12_BYTE_OKM_BITPOS 0
/**< Limit sublabel expand output to 12 bytes -- used with the "iv" sublabel */

#define QAT_FW_HKDF_INNER_SUBLABEL_16_BYTE_OKM_BITPOS 1
/**< Limit sublabel expand output to 16 bytes -- used with SHA-256 "key" */

#define QAT_FW_HKDF_INNER_SUBLABEL_32_BYTE_OKM_BITPOS 2
/**< Limit sublabel expand output to 32 bytes -- used with SHA-384 "key" */

#endif /* _ICP_QAT_FW_LA_H_ */
