/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */

/**
 ***************************************************************************
 * @file lac_sym.h
 *
 * @defgroup LacSym    Symmetric
 *
 * @ingroup Lac
 *
 * Symmetric component includes cipher, Hash, chained cipher & hash,
 * authenticated encryption and key generation.
 *
 * @lld_start
 * @lld_overview
 *
 * The symmetric component demuliplexes the following crypto operations to
 * the appropriate sub-components: cipher, hash, algorithm chaining and
 * authentication encryption. It is a common layer between the above
 * mentioned components where common resources are allocated and paramater
 * checks are done. The operation specific resource allocation and parameter
 * checks are done in the sub-component itself.
 *
 * The symmetric component demultiplexes the session register/deregister
 * and perform functions to the appropriate subcomponents.
 *
 * @lld_dependencies
 * - \ref LacSymPartial "Partial Packet Code":  This code manages the partial
 *    packet state for a session.
 * - \ref LacBufferDesc  "Common Buffer Code" : This code traverses a buffer
 *   chain to ensure it is valid.
 * - \ref LacSymStats "Statistics": Manages statistics for symmetric
 * - \ref LacSymQat "Symmetric QAT": The symmetric qat component is
 *   initialiased by the symmetric component.
 * - \ref LacCipher "Cipher" : demultiplex cipher opertions to this component.
 * - \ref LacHash "Hash" : demultiplex hash opertions to this component.
 *   to this component.
 * - \ref LacAlgChain "Algorithm Chaining": The algorithm chaining component
 * - OSAL : Memory allocation, Mutex's, atomics
 *
 * @lld_initialisation
 * This component is initialied during the LAC initialisation sequence. It
 * initialises the session table, statistics, symmetric QAT, initialises the
 * hash definitions lookup table, the hash alg supported lookup table and
 * registers a callback function with the symmetric response handler to process
 * response messages for Cipher, Hash and Algorithm-Chaining requests.
 *
 * @lld_module_algorithms
 *
 * @lld_process_context
 * Refer to \ref LacHash "Hash" and \ref LacCipher "Cipher" for sequence
 * diagrams from the symmetric component through the sub components.
 *
 * @lld_end
 *
 ***************************************************************************/

/***************************************************************************/

#ifndef LAC_SYM_H
#define LAC_SYM_H

#include "cpa.h"
#include "cpa_cy_sym.h"
#include "cpa_cy_sym_dp.h"
#include "lac_common.h"
#include "lac_mem_pools.h"
#include "lac_sym_cipher_defs.h"
#include "icp_qat_fw_la.h"

#define LAC_SYM_KEY_TLS_PREFIX_SIZE 128
/**< Hash Prefix size in bytes for TLS (128 = MAX = SHA2 (384, 512)*/

#define LAC_SYM_OPTIMISED_CD_SIZE 64
/**< The size of the optimised content desc in DRAM*/

#define LAC_SYM_KEY_MAX_HASH_STATE_BUFFER (LAC_SYM_KEY_TLS_PREFIX_SIZE * 2)
/**< hash state prefix buffer structure that holds the maximum sized secret */

#define LAC_SYM_HASH_BUFFER_LEN 64
/**< Buffer length to hold 16 byte MD5 key and 20 byte SHA1 key */

/* The ARC4 key will not be stored in the content descriptor so we only need to
 * reserve enough space for the next biggest cipher setup block.
 * Kasumi needs to store 2 keys and to have the size of 2 blocks for fw*/
#define LAC_SYM_QAT_MAX_CIPHER_SETUP_BLK_SZ                                    \
	(sizeof(icp_qat_hw_cipher_config_t) + 2 * ICP_QAT_HW_KASUMI_KEY_SZ +   \
	 2 * ICP_QAT_HW_KASUMI_BLK_SZ)
/**< @ingroup LacSymQat
 * Maximum size for the cipher setup block of the content descriptor */

#define LAC_SYM_QAT_MAX_HASH_SETUP_BLK_SZ sizeof(icp_qat_hw_auth_algo_blk_t)
/**< @ingroup LacSymQat
 * Maximum size for the hash setup block of the content descriptor */

#define LAC_SYM_QAT_CONTENT_DESC_MAX_SIZE                                      \
	LAC_ALIGN_POW2_ROUNDUP(LAC_SYM_QAT_MAX_CIPHER_SETUP_BLK_SZ +           \
				   LAC_SYM_QAT_MAX_HASH_SETUP_BLK_SZ,          \
			       (1 << LAC_64BYTE_ALIGNMENT_SHIFT))
/**< @ingroup LacSymQat
 *  Maximum size of content descriptor. This is incremented to the next multiple
 * of 64 so that it can be 64 byte aligned */

#define LAC_SYM_QAT_API_ALIGN_COOKIE_OFFSET                                    \
	(offsetof(CpaCySymDpOpData, instanceHandle))
/**< @ingroup LacSymQat
 * Size which needs to be reserved before the instanceHandle field of
 * lac_sym_bulk_cookie_s to align it to the correspondent instanceHandle
 * in CpaCySymDpOpData */

#define LAC_SIZE_OF_CACHE_HDR_IN_LW 6
/**< Size of Header part of reqCache/shramReqCache */

#define LAC_SIZE_OF_CACHE_MID_IN_LW 2
/**< Size of Mid part (LW14/15) of reqCache/shramReqCache */

#define LAC_SIZE_OF_CACHE_FTR_IN_LW 6
/**< Size of Footer part of reqCache/shramReqCache */

#define LAC_SIZE_OF_CACHE_TO_CLEAR_IN_LW 20
/**< Size of dummy reqCache/shramReqCache to clear */

#define LAC_START_OF_CACHE_MID_IN_LW 14
/**< Starting LW of reqCache/shramReqCache Mid */

#define LAC_START_OF_CACHE_FTR_IN_LW 26
/**< Starting LW of reqCache/shramReqCache Footer */

/**
 *******************************************************************************
 * @ingroup LacSym
 *      Symmetric cookie
 *
 * @description
 *      This cookie stores information for a particular symmetric perform op.
 *      This includes the request params, re-aligned Cipher IV, the request
 *      message sent to the QAT engine, and various user-supplied parameters
 *      for the operation which will be needed in our callback function.
 *      A pointer to this cookie is stored in the opaque data field of the QAT
 *      message so that it can be accessed in the asynchronous callback.
 *      Cookies for multiple operations on a given session can be linked
 *      together to allow queuing of requests using the pNext field.
 *
 *      The parameters are placed in order to match the CpaCySymDpOpData
 *structure
 *****************************************************************************/
typedef struct lac_sym_bulk_cookie_s {

	/* CpaCySymDpOpData struct so need to keep this here for correct
	 * alignment*/
	Cpa8U reserved[LAC_SYM_QAT_API_ALIGN_COOKIE_OFFSET];
	/** NOTE: Field must be correctly aligned in memory for access by QAT
	 * engine
	 */
	CpaInstanceHandle instanceHandle;
	/**< Instance handle for the operation */
	CpaCySymSessionCtx sessionCtx;
	/**< Session context */
	void *pCallbackTag;
	/**< correlator supplied by the client */
	icp_qat_fw_la_bulk_req_t qatMsg;
	/**< QAT request message */
	const CpaCySymOpData *pOpData;
	/**< pointer to the op data structure that the user supplied in the
	 * perform
	 * operation. The op data is modified in the process callback function
	 * and the pointer is returned to the user in their callback function */
	CpaBoolean updateSessionIvOnSend;
	/**< Boolean flag to indicate if the session cipher IV buffer should be
	 * updated prior to sending the request */
	CpaBoolean updateUserIvOnRecieve;
	/**< Boolean flag to indicate if the user's cipher IV buffer should be
	 * updated after receiving the response from the QAT */
	CpaBoolean updateKeySizeOnRecieve;
/**< Boolean flag to indicate if the cipher key size should be
 * updated after receiving the response from the QAT */
	CpaBufferList *pDstBuffer;
	/**< Pointer to destination buffer to hold the data output */
	struct lac_sym_bulk_cookie_s *pNext;
	/**< Pointer to next node in linked list (if request is queued) */
} lac_sym_bulk_cookie_t;

/**
*******************************************************************************
* @ingroup LacSymKey
*      symmetric Key cookie
* @description
*      This cookie stores information for a particular keygen perform op.
*      This includes a hash content descriptor, request params, hash state
*      buffer, and various user-supplied parameters for the operation which
*      will be needed in our callback function.
*      A pointer to this cookie is stored in the opaque data field of the QAT
*      message so that it can be accessed in the asynchronous callback.
*****************************************************************************/
typedef struct lac_sym_key_cookie_s {
	CpaInstanceHandle instanceHandle;
	/**< QAT device id supplied by the client */
	void *pCallbackTag;
	/**< Mechanism used. TLS, SSL or MGF */
	Cpa8U contentDesc[LAC_SYM_QAT_MAX_HASH_SETUP_BLK_SZ];
	/**< Content descriptor.
	 **< NOTE: Field must be correctly aligned in memory for access by QAT
	 * engine */
	union {
		icp_qat_fw_la_ssl_key_material_input_t sslKeyInput;
		/**< SSL key material input structure */
		icp_qat_fw_la_tls_key_material_input_t tlsKeyInput;
		/**< TLS key material input structure */
		icp_qat_fw_la_hkdf_key_material_input_t tlsHKDFKeyInput;
		/**< TLS HHKDF key material input structure */
	} u;
	/**< NOTE: Field must be correctly aligned in memory for access by QAT
	 * engine */
	Cpa8U hashStateBuffer[LAC_SYM_KEY_MAX_HASH_STATE_BUFFER];
	/**< hash state prefix buffer
	 * NOTE: Field must be correctly aligned in memory for access by QAT
	 * engine
	 */
	CpaCyGenFlatBufCbFunc pKeyGenCb;
	/**< callback function supplied by the client */
	void *pKeyGenOpData;
	/**< pointer to the (SSL/TLS) or MGF op data structure that the user
	 * supplied in the perform operation */
	CpaFlatBuffer *pKeyGenOutputData;
	/**< Output data pointer supplied by the client */
	Cpa8U hashKeyBuffer[LAC_SYM_HASH_BUFFER_LEN];
	/**< 36 byte buffer to store MD5 key and SHA1 key */
} lac_sym_key_cookie_t;

/**
*******************************************************************************
* @ingroup LacSymNrbg
*      symmetric NRBG cookie
* @description
*      This cookie stores information for a particular NRBG operation.
*      This includes various user-supplied parameters for the operation which
*      will be needed in our callback function.
*      A pointer to this cookie is stored in the opaque data field of the QAT
*      message so that it can be accessed in the asynchronous callback.
*****************************************************************************/
typedef struct lac_sym_nrbg_cookie_s {
	CpaInstanceHandle instanceHandle;
	/**< QAT device id supplied by the client */
	void *pCallbackTag;
	/**< Opaque data supplied by the client */
	icp_qat_fw_la_trng_test_result_t trngHTResult;
	/**< TRNG health test result
	 **< NOTE: Field must be correctly aligned in memory for access by QAT
	 * engine */
	icp_qat_fw_la_trng_req_t trngReq;
	/**< TRNG request message */
	CpaCyGenFlatBufCbFunc pCb;
	/**< Callback function supplied by the client */
	void *pOpData;
	/**< Op data pointer supplied by the client */
	CpaFlatBuffer *pOutputData;
	/**< Output data pointer supplied by the client */
} lac_sym_nrbg_cookie_t;

/**
*******************************************************************************
* @ingroup LacSym
*      symmetric cookie
* @description
*      used to determine the amount of memory to allocate for the symmetric
*      cookie pool. As symmetric, random and key generation shared the same
*      pool
*****************************************************************************/
typedef struct lac_sym_cookie_s {
	union {
		lac_sym_bulk_cookie_t bulkCookie;
		/**< symmetric bulk cookie */
		lac_sym_key_cookie_t keyCookie;
		/**< symmetric key cookie */
		lac_sym_nrbg_cookie_t nrbgCookie;
		/**< symmetric NRBG cookie */
	} u;
	Cpa64U keyContentDescPhyAddr;
	Cpa64U keyHashStateBufferPhyAddr;
	Cpa64U keySslKeyInputPhyAddr;
	Cpa64U keyTlsKeyInputPhyAddr;
} lac_sym_cookie_t;

typedef struct icp_qat_la_auth_req_params_s {
	/** equivalent of LW26 of icp_qat_fw_la_auth_req_params_s */
	union {
		uint8_t inner_prefix_sz;
		/**< Size in bytes of the inner prefix data */

		uint8_t aad_sz;
		/**< Size in bytes of padded AAD data to prefix to the packet
		 * for CCM
		 *  or GCM processing */
	} u2;

	uint8_t resrvd1;
	/**< reserved */

	uint8_t hash_state_sz;
	/**< Number of quad words of inner and outer hash prefix data to process
	 * Maximum size is 240 */

	uint8_t auth_res_sz;
	/**< Size in bytes of the authentication result */
} icp_qat_la_auth_req_params_t;

/* Header (LW's 0 - 5) of struct icp_qat_fw_la_bulk_req_s */
typedef struct icp_qat_la_bulk_req_hdr_s {
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
} icp_qat_la_bulk_req_hdr_t;

/** Footer (LW's 26 - 31) of struct icp_qat_fw_la_bulk_req_s */
typedef struct icp_qat_la_bulk_req_ftr_s {
	/**< LW 0 - equivalent to LW26 of icp_qat_fw_la_bulk_req_t */
	icp_qat_la_auth_req_params_t serv_specif_rqpars;
	/**< Common request service-specific parameter field */

	/**< LW's 1-5, equivalent to LWs 27-31 of icp_qat_fw_la_bulk_req_s */
	icp_qat_fw_comn_req_cd_ctrl_t cd_ctrl;
	/**< Common request content descriptor control block -
	 * this field is service-specific */
} icp_qat_la_bulk_req_ftr_t;

/**
 ***
 *******************************************************************************
 * @ingroup LacSym
 *      Compile time check of lac_sym_bulk_cookie_t
 *
 * @description
 *      Performs a compile time check of lac_sym_bulk_cookie_t to ensure IA
 *      assumptions are valid.
 *
 *****************************************************************************/
void LacSym_CompileTimeAssertions(void);

void LacDp_WriteRingMsgFull(CpaCySymDpOpData *pRequest,
			    icp_qat_fw_la_bulk_req_t *pCurrentQatMsg);
void LacDp_WriteRingMsgOpt(CpaCySymDpOpData *pRequest,
			   icp_qat_fw_la_bulk_req_t *pCurrentQatMsg);

#endif /* LAC_SYM_H */
