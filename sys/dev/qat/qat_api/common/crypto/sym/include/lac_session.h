/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */

/**
 *****************************************************************************
 * @file lac_session.h
 *
 * @defgroup LacSym_Session Session
 *
 * @ingroup LacSym
 *
 * Definition of symmetric session descriptor structure
 *
 * @lld_start
 *
 * @lld_overview
 * A session is required for each symmetric operation. The session descriptor
 * holds information about the session from when the session is initialised to
 * when the session is removed. The session descriptor is used in the
 * subsequent perform operations in the paths for both sending the request and
 * receiving the response. The session descriptor and any other state
 * information required for processing responses from the QAT are stored in an
 * internal cookie. A pointer to this cookie is stored in the opaque data
 * field of the QAT request.
 *
 * The user allocates the memory for the session using the size returned from
 * \ref cpaCySymSessionCtxGetSize(). Internally this memory is re-aligned on a
 * 64 byte boundary for use by the QAT engine. The aligned pointer is saved in
 * the first bytes (size of void *) of the session memory. This address
 * is then dereferenced in subsequent performs to get access to the session
 * descriptor.
 *
 * <b>LAC Session Init</b>\n The session descriptor is re-aligned and
 * populated. This includes populating the content descriptor which contains
 * the hardware setup for the QAT engine. The content descriptor is a read
 * only structure after session init and a pointer to it is sent to the QAT
 * for each perform operation.
 *
 * <b>LAC Perform </b>\n
 * The address for the session descriptor is got by dereferencing the first
 * bytes of the session memory (size of void *). For each successful
 * request put on the ring, the pendingCbCount for the session is incremented.
 *
 * <b>LAC Callback </b>\n
 * For each successful response the pendingCbCount for the session is
 * decremented. See \ref LacSymCb_ProcessCallbackInternal()
 *
 * <b>LAC Session Remove </b>\n
 * The address for the session descriptor is got by dereferencing the first
 * bytes of the session memory (size of void *).
 * The pendingCbCount for the session is checked to see if it is 0. If it is
 * non 0 then there are requests in flight. An error is returned to the user.
 *
 * <b>Concurrency</b>\n
 * A reference count is used to prevent the descriptor being removed
 * while there are requests in flight.
 *
 * <b>Reference Count</b>\n
 * - The perform funcion increments the reference count for the session.
 * - The callback function decrements the reference count for the session.
 * - The Remove function checks the reference count to ensure that it is 0.
 *
 * @lld_dependencies
 * - \ref LacMem "Memory" - Inline memory functions
 * - QatUtils: logging, locking & virt to phys translations.
 *
 * @lld_initialisation
 *
 * @lld_module_algorithms
 *
 * @lld_process_context
 *
 * @lld_end
 *
 *****************************************************************************/

/***************************************************************************/

#ifndef LAC_SYM_SESSION_H
#define LAC_SYM_SESSION_H

/*
 * Common alignment attributes to ensure
 * hashStatePrefixBuffer is 64-byte aligned
 */
#define ALIGN_START(x)
#define ALIGN_END(x) __attribute__((__aligned__(x)))
/*
******************************************************************************
* Include public/global header files
******************************************************************************
*/

#include "cpa.h"
#include "icp_accel_devices.h"
#include "lac_list.h"
#include "lac_sal_types.h"
#include "sal_qat_cmn_msg.h"
#include "lac_sym_cipher_defs.h"
#include "lac_sym.h"
#include "lac_sym_hash_defs.h"
#include "lac_sym_qat_hash.h"

/*
*******************************************************************************
* Include private header files
*******************************************************************************
*/
/**
*****************************************************************************
* @ingroup LacSym
*      Spc state
*
* @description
*      This enum is used to indicate the Spc state.
*
*****************************************************************************/
typedef enum lac_single_pass_state_e {
	NON_SPC,    /* Algorithms other than CHACHA-POLY and AES-GCM */
	LIKELY_SPC, /* AES-GCM - Likely to handle it as single pass  */
	SPC	    /* CHACHA-POLY and AES-GCM */
} lac_single_pass_state_t;

/**
*******************************************************************************
* @ingroup LacSym_Session
*      Symmetric session descriptor
* @description
*      This structure stores information about a session
*      Note: struct types lac_session_d1_s and lac_session_d2_s are subsets of
*      this structure. Elements in all three should retain the same order
*      Only this structure is used in the session init call. The other two are
*      for determining the size of memory to allocate.
*      The comments section of each of the other two structures below show
*      the conditions that determine which session context memory size to use.
*****************************************************************************/
typedef struct lac_session_desc_s {
	Cpa8U contentDescriptor[LAC_SYM_QAT_CONTENT_DESC_MAX_SIZE];
	/**< QAT Content Descriptor for this session.
	 * NOTE: Field must be correctly aligned in memory for access by QAT
	 * engine
	 */
	Cpa8U contentDescriptorOptimised[LAC_SYM_OPTIMISED_CD_SIZE];
	/**< QAT Optimised Content Descriptor for this session.
	 * NOTE: Field must be correctly aligned in memory for access by QAT
	 * engine
	 */
	CpaCySymOp symOperation;
	/**< type of command to be performed */
	sal_qat_content_desc_info_t contentDescInfo;
	/**< info on the content descriptor */
	sal_qat_content_desc_info_t contentDescOptimisedInfo;
	/**< info on the optimised content descriptor */
	icp_qat_fw_la_cmd_id_t laCmdId;
	/**<Command Id for the QAT FW */
	lac_sym_qat_hash_state_buffer_info_t hashStateBufferInfo;
	/**< info on the hash state prefix buffer */
	CpaCySymHashAlgorithm hashAlgorithm;
	/**< hash algorithm */
	Cpa32U authKeyLenInBytes;
	/**< Authentication key length in bytes */
	CpaCySymHashMode hashMode;
	/**< Mode of the hash operation. plain, auth or nested */
	Cpa32U hashResultSize;
	/**< size of the digest produced/compared in bytes */
	CpaCySymCipherAlgorithm cipherAlgorithm;
	/**< Cipher algorithm and mode */
	Cpa32U cipherKeyLenInBytes;
	/**< Cipher key length in bytes */
	CpaCySymCipherDirection cipherDirection;
	/**< This parameter determines if the cipher operation is an encrypt or
	 * a decrypt operation. */
	CpaCySymPacketType partialState;
	/**< state of the partial packet. This can be written to by the perform
	 * because the SpinLock pPartialInFlightSpinlock guarantees that the
	 * state is accessible in only one place at a time. */
	icp_qat_la_bulk_req_hdr_t reqCacheHdr;
	icp_qat_fw_la_key_gen_common_t reqCacheMid;
	icp_qat_la_bulk_req_ftr_t reqCacheFtr;
	/**< Cache as much as possible of the bulk request in a pre built
	 * request (header, mid & footer). */
	CpaCySymCbFunc pSymCb;
	/**< symmetric function callback pointer */
	union {
		QatUtilsAtomic pendingCbCount;
		/**< Keeps track of number of pending requests.  */
		QatUtilsAtomic pendingDpCbCount;
		/**< Keeps track of number of pending DP requests (not thread
		 * safe)*/
	} u;
	struct lac_sym_bulk_cookie_s *pRequestQueueHead;
	/**< A fifo list of queued QAT requests. Head points to first queue
	 * entry */
	struct lac_sym_bulk_cookie_s *pRequestQueueTail;
	/**< A fifo list of queued QAT requests. Tail points to last queue entry
	 */
	struct mtx requestQueueLock;
	/**< A lock to protect accesses to the above request queue  */
	CpaInstanceHandle pInstance;
	/**< Pointer to Crypto instance running this session. */
	CpaBoolean isAuthEncryptOp : 1;
	/**< if the algorithm chaining operation is auth encrypt */
	CpaBoolean nonBlockingOpsInProgress : 1;
	/**< Flag is set if a non blocking operation is in progress for a
	 * session.
	 * If set to false, new requests will be queued until the condition is
	 * cleared.
	 * ASSUMPTION: Only one blocking condition per session can exist at any
	 * time
	 */
	CpaBoolean internalSession : 1;
	/**< Flag which is set if the session was set up internally for DRBG */
	CpaBoolean isDPSession : 1;
	/**< Flag which is set if the session was set up for Data Plane */
	CpaBoolean digestVerify : 1;
	/**< Session digest verify for data plane and for CCM/GCM for trad
	 * api. For other cases on trad api this flag is set in each performOp
	 */
	CpaBoolean digestIsAppended : 1;
	/**< Flag indicating whether the digest is appended immediately
	 * following
	 * the region over which the digest is computed */
	CpaBoolean isCipher : 1;
	/**< Flag indicating whether symOperation includes a cipher operation */
	CpaBoolean isAuth : 1;
	/**< Flag indicating whether symOperation includes an auth operation */
	CpaBoolean useSymConstantsTable : 1;
	/**< Flag indicating whether the SymConstantsTable can be used or not */
	CpaBoolean useOptimisedContentDesc : 1;
	/**< Flag indicating whether to use the optimised CD or not */
	CpaBoolean isPartialSupported : 1;
	/**< Flag indicating whether symOperation support partial packet */
	CpaBoolean useStatefulSha3ContentDesc : 1;
	/**< Flag indicating whether to use the stateful SHA3 CD or not */
	icp_qat_la_bulk_req_hdr_t shramReqCacheHdr;
	icp_qat_fw_la_key_gen_common_t shramReqCacheMid;
	icp_qat_la_bulk_req_ftr_t shramReqCacheFtr;
	/**< Alternative pre-built request (header, mid & footer)
	 * for use with symConstantsTable. */
	lac_single_pass_state_t singlePassState;
	/**< Flag indicating whether symOperation support single pass */
	icp_qat_fw_serv_specif_flags laCmdFlags;
	/**< Common request - Service specific flags type  */
	icp_qat_fw_comn_flags cmnRequestFlags;
	/**< Common request flags type  */
	icp_qat_fw_ext_serv_specif_flags laExtCmdFlags;
	/**< Common request - Service specific flags type  */
	icp_qat_la_bulk_req_hdr_t reqSpcCacheHdr;
	icp_qat_la_bulk_req_ftr_t reqSpcCacheFtr;
	/**< request (header & footer)for use with Single Pass. */
	icp_qat_hw_auth_mode_t qatHashMode;
	/**< Hash Mode for the qat slices. Not to be confused with QA-API
	 * hashMode
	 */
	Cpa32U cipherSliceType;
	/**< Cipher slice type to be used, set at init session time */
	Cpa8U cipherAesXtsKey1Forward[LAC_CIPHER_AES_XTS_KEY_MAX_LENGTH];
	/**< Cached AES XTS Forward key
	 * For CPM2.0 AES XTS key convertion need to be done in SW.
	 * Because use can update session direction at any time,
	 * also forward key needs to be cached
	 */
	Cpa8U cipherAesXtsKey1Reverse[LAC_CIPHER_AES_XTS_KEY_MAX_LENGTH];
	/**< AES XTS Reverse key
	 * For CPM2.0 AES XTS key convertion need to be done in SW.
	 * Reverse key always will be calcilated at session setup time and
	 * cached to be used when needed */
	Cpa8U cipherAesXtsKey2[LAC_CIPHER_AES_XTS_KEY_MAX_LENGTH];
	/**< For AES XTS session need to store Key2 value in order to generate
	 * tweak
	 */
	void *writeRingMsgFunc;
	/**< function which will be called to write ring message */
	Cpa32U aadLenInBytes;
	/**< For CCM,GCM and Snow3G cases, this parameter holds the AAD size,
	 * otherwise it is set to zero */
	ALIGN_START(64)
	Cpa8U hashStatePrefixBuffer[LAC_MAX_AAD_SIZE_BYTES] ALIGN_END(64);
	/**< hash state prefix buffer used for hash operations - AAD only
	 * NOTE: Field must be correctly aligned in memory for access by QAT
	 * engine
	 */
	Cpa8U hashStatePrefixBufferExt[LAC_MAX_HASH_STATE_BUFFER_SIZE_BYTES -
				       LAC_MAX_AAD_SIZE_BYTES];
	/**< hash state prefix buffer used for hash operations - Remainder of
	 * array.
	 * NOTE: Field must be correctly aligned in memory for access by QAT
	 * engine
	 */
	Cpa8U cipherPartialOpState[LAC_CIPHER_STATE_SIZE_MAX];
	/**< Buffer to hold the cipher state for the session (for partial ops).
	 * NOTE: Field must be correctly aligned in memory for access by QAT
	 * engine
	 */
	Cpa8U cipherARC4InitialState[LAC_CIPHER_ARC4_STATE_LEN_BYTES];
	/**< Buffer to hold the initial ARC4 cipher state for the session, which
	 * is derived from the user-supplied base key during session
	 * registration.
	 * NOTE: Field must be correctly aligned in memory for access by QAT
	 * engine
	 */
	CpaPhysicalAddr cipherARC4InitialStatePhysAddr;
	/**< The physical address of the ARC4 initial state, set at init
	** session time .
	*/
} lac_session_desc_t;

/**
*******************************************************************************
* @ingroup LacSym_Session
*      Symmetric session descriptor - d1
* @description
*      This structure stores information about a specific session which
*       assumes the following:
*      - cipher algorithm is not ARC4 or Snow3G
*      - partials not used
*      - not AuthEncrypt operation
*      - hash mode not Auth or Nested
*      - no hashStatePrefixBuffer required
*      It is therefore a subset of the standard symmetric session descriptor,
*       with a smaller memory footprint
*****************************************************************************/
typedef struct lac_session_desc_d1_s {
	Cpa8U contentDescriptor[LAC_SYM_QAT_CONTENT_DESC_MAX_SIZE];
	/**< QAT Content Descriptor for this session.
	 * NOTE: Field must be correctly aligned in memory for access by QAT
	 * engine
	 */
	Cpa8U contentDescriptorOptimised[LAC_SYM_OPTIMISED_CD_SIZE];
	/**< QAT Optimised Content Descriptor for this session.
	 * NOTE: Field must be correctly aligned in memory for access by QAT
	 * engine
	 */
	CpaCySymOp symOperation;
	/**< type of command to be performed */
	sal_qat_content_desc_info_t contentDescInfo;
	/**< info on the content descriptor */
	sal_qat_content_desc_info_t contentDescOptimisedInfo;
	/**< info on the optimised content descriptor */
	icp_qat_fw_la_cmd_id_t laCmdId;
	/**<Command Id for the QAT FW */
	lac_sym_qat_hash_state_buffer_info_t hashStateBufferInfo;
	/**< info on the hash state prefix buffer */
	CpaCySymHashAlgorithm hashAlgorithm;
	/**< hash algorithm */
	Cpa32U authKeyLenInBytes;
	/**< Authentication key length in bytes */
	CpaCySymHashMode hashMode;
	/**< Mode of the hash operation. plain, auth or nested */
	Cpa32U hashResultSize;
	/**< size of the digest produced/compared in bytes */
	CpaCySymCipherAlgorithm cipherAlgorithm;
	/**< Cipher algorithm and mode */
	Cpa32U cipherKeyLenInBytes;
	/**< Cipher key length in bytes */
	CpaCySymCipherDirection cipherDirection;
	/**< This parameter determines if the cipher operation is an encrypt or
	 * a decrypt operation. */
	CpaCySymPacketType partialState;
	/**< state of the partial packet. This can be written to by the perform
	 * because the SpinLock pPartialInFlightSpinlock guarantees that the
	 * state is accessible in only one place at a time. */
	icp_qat_la_bulk_req_hdr_t reqCacheHdr;
	icp_qat_fw_la_key_gen_common_t reqCacheMid;
	icp_qat_la_bulk_req_ftr_t reqCacheFtr;
	/**< Cache as much as possible of the bulk request in a pre built
	 * request (header, mid & footer). */
	CpaCySymCbFunc pSymCb;
	/**< symmetric function callback pointer */
	union {
		QatUtilsAtomic pendingCbCount;
		/**< Keeps track of number of pending requests.  */
		Cpa64U pendingDpCbCount;
		/**< Keeps track of number of pending DP requests (not thread
		 * safe)*/
	} u;
	struct lac_sym_bulk_cookie_s *pRequestQueueHead;
	/**< A fifo list of queued QAT requests. Head points to first queue
	 * entry */
	struct lac_sym_bulk_cookie_s *pRequestQueueTail;
	/**< A fifo list of queued QAT requests. Tail points to last queue entry
	 */
	struct mtx requestQueueLock;
	/**< A lock to protect accesses to the above request queue  */
	CpaInstanceHandle pInstance;
	/**< Pointer to Crypto instance running this session. */
	CpaBoolean isAuthEncryptOp : 1;
	/**< if the algorithm chaining operation is auth encrypt */
	CpaBoolean nonBlockingOpsInProgress : 1;
	/**< Flag is set if a non blocking operation is in progress for a
	 * session.
	 * If set to false, new requests will be queued until the condition is
	 * cleared.
	 * ASSUMPTION: Only one blocking condition per session can exist at any
	 * time
	 */
	CpaBoolean internalSession : 1;
	/**< Flag which is set if the session was set up internally for DRBG */
	CpaBoolean isDPSession : 1;
	/**< Flag which is set if the session was set up for Data Plane */
	CpaBoolean digestVerify : 1;
	/**< Session digest verify for data plane and for CCM/GCM for trad
	 * api. For other cases on trad api this flag is set in each performOp
	 */
	CpaBoolean digestIsAppended : 1;
	/**< Flag indicating whether the digest is appended immediately
	 * following
	 * the region over which the digest is computed */
	CpaBoolean isCipher : 1;
	/**< Flag indicating whether symOperation includes a cipher operation */
	CpaBoolean isAuth : 1;
	/**< Flag indicating whether symOperation includes an auth operation */
	CpaBoolean useSymConstantsTable : 1;
	/**< Flag indicating whether the SymConstantsTable can be used or not */
	CpaBoolean useOptimisedContentDesc : 1;
	/**< Flag indicating whether to use the optimised CD or not */
	CpaBoolean isPartialSupported : 1;
	/**< Flag indicating whether symOperation support partial packet */
	CpaBoolean useStatefulSha3ContentDesc : 1;
	/**< Flag indicating whether to use the stateful SHA3 CD or not */
	icp_qat_la_bulk_req_hdr_t shramReqCacheHdr;
	icp_qat_fw_la_key_gen_common_t shramReqCacheMid;
	icp_qat_la_bulk_req_ftr_t shramReqCacheFtr;
	/**< Alternative pre-built request (header, mid & footer)
	 * for use with symConstantsTable. */
	lac_single_pass_state_t singlePassState;
	/**< Flag indicating whether symOperation support single pass */
	icp_qat_fw_serv_specif_flags laCmdFlags;
	/**< Common request - Service specific flags type  */
	icp_qat_fw_comn_flags cmnRequestFlags;
	/**< Common request flags type  */
	icp_qat_fw_ext_serv_specif_flags laExtCmdFlags;
	/**< Common request - Service specific flags type  */
	icp_qat_la_bulk_req_hdr_t reqSpcCacheHdr;
	icp_qat_la_bulk_req_ftr_t reqSpcCacheFtr;
	/**< request (header & footer)for use with Single Pass. */
	icp_qat_hw_auth_mode_t qatHashMode;
	/**< Hash Mode for the qat slices. Not to be confused with QA-API
	 * hashMode
	 */
	Cpa32U cipherSliceType;
	/**< Cipher slice type to be used, set at init session time */
	Cpa8U cipherAesXtsKey1Forward[LAC_CIPHER_AES_XTS_KEY_MAX_LENGTH];
	/**< Cached AES XTS Forward key
	 * For CPM2.0 AES XTS key convertion need to be done in SW.
	 * Because use can update session direction at any time,
	 * also forward key needs to be cached
	 */
	Cpa8U cipherAesXtsKey1Reverse[LAC_CIPHER_AES_XTS_KEY_MAX_LENGTH];
	/**< AES XTS Reverse key
	 * For CPM2.0 AES XTS key convertion need to be done in SW.
	 * Reverse key always will be calcilated at session setup time and
	 * cached to be used when needed */
	Cpa8U cipherAesXtsKey2[LAC_CIPHER_AES_XTS_KEY_MAX_LENGTH];
	/**< For AES XTS session need to store Key2 value in order to generate
	 * tweak
	 */
	void *writeRingMsgFunc;
	/**< function which will be called to write ring message */
} lac_session_desc_d1_t;

/**
*******************************************************************************
* @ingroup LacSym_Session
*      Symmetric session descriptor - d2
* @description
*      This structure stores information about a specific session which
*       assumes the following:
*      - authEncrypt only
*      - partials not used
*      - hasStatePrefixBuffer just contains AAD
*      It is therefore a subset of the standard symmetric session descriptor,
*       with a smaller memory footprint
*****************************************************************************/
typedef struct lac_session_desc_d2_s {
	Cpa8U contentDescriptor[LAC_SYM_QAT_CONTENT_DESC_MAX_SIZE];
	/**< QAT Content Descriptor for this session.
	 * NOTE: Field must be correctly aligned in memory for access by QAT
	 * engine
	 */
	Cpa8U contentDescriptorOptimised[LAC_SYM_OPTIMISED_CD_SIZE];
	/**< QAT Optimised Content Descriptor for this session.
	 * NOTE: Field must be correctly aligned in memory for access by QAT
	 * engine
	 */
	CpaCySymOp symOperation;
	/**< type of command to be performed */
	sal_qat_content_desc_info_t contentDescInfo;
	/**< info on the content descriptor */
	sal_qat_content_desc_info_t contentDescOptimisedInfo;
	/**< info on the optimised content descriptor */
	icp_qat_fw_la_cmd_id_t laCmdId;
	/**<Command Id for the QAT FW */
	lac_sym_qat_hash_state_buffer_info_t hashStateBufferInfo;
	/**< info on the hash state prefix buffer */
	CpaCySymHashAlgorithm hashAlgorithm;
	/**< hash algorithm */
	Cpa32U authKeyLenInBytes;
	/**< Authentication key length in bytes */
	CpaCySymHashMode hashMode;
	/**< Mode of the hash operation. plain, auth or nested */
	Cpa32U hashResultSize;
	/**< size of the digest produced/compared in bytes */
	CpaCySymCipherAlgorithm cipherAlgorithm;
	/**< Cipher algorithm and mode */
	Cpa32U cipherKeyLenInBytes;
	/**< Cipher key length in bytes */
	CpaCySymCipherDirection cipherDirection;
	/**< This parameter determines if the cipher operation is an encrypt or
	 * a decrypt operation. */
	CpaCySymPacketType partialState;
	/**< state of the partial packet. This can be written to by the perform
	 * because the SpinLock pPartialInFlightSpinlock guarantees that the
	 * state is accessible in only one place at a time. */
	icp_qat_la_bulk_req_hdr_t reqCacheHdr;
	icp_qat_fw_la_key_gen_common_t reqCacheMid;
	icp_qat_la_bulk_req_ftr_t reqCacheFtr;
	/**< Cache as much as possible of the bulk request in a pre built
	 * request (header. mid & footer). */
	CpaCySymCbFunc pSymCb;
	/**< symmetric function callback pointer */
	union {
		QatUtilsAtomic pendingCbCount;
		/**< Keeps track of number of pending requests.  */
		Cpa64U pendingDpCbCount;
		/**< Keeps track of number of pending DP requests (not thread
		 * safe)*/
	} u;
	struct lac_sym_bulk_cookie_s *pRequestQueueHead;
	/**< A fifo list of queued QAT requests. Head points to first queue
	 * entry */
	struct lac_sym_bulk_cookie_s *pRequestQueueTail;
	/**< A fifo list of queued QAT requests. Tail points to last queue entry
	 */
	struct mtx requestQueueLock;
	/**< A lock to protect accesses to the above request queue  */
	CpaInstanceHandle pInstance;
	/**< Pointer to Crypto instance running this session. */
	CpaBoolean isAuthEncryptOp : 1;
	/**< if the algorithm chaining operation is auth encrypt */
	CpaBoolean nonBlockingOpsInProgress : 1;
	/**< Flag is set if a non blocking operation is in progress for a
	 * session.
	 * If set to false, new requests will be queued until the condition is
	 * cleared.
	 * ASSUMPTION: Only one blocking condition per session can exist at any
	 * time
	 */
	CpaBoolean internalSession : 1;
	/**< Flag which is set if the session was set up internally for DRBG */
	CpaBoolean isDPSession : 1;
	/**< Flag which is set if the session was set up for Data Plane */
	CpaBoolean digestVerify : 1;
	/**< Session digest verify for data plane and for CCM/GCM for trad
	 * api. For other cases on trad api this flag is set in each performOp
	 */
	CpaBoolean digestIsAppended : 1;
	/**< Flag indicating whether the digest is appended immediately
	 * following
	 * the region over which the digest is computed */
	CpaBoolean isCipher : 1;
	/**< Flag indicating whether symOperation includes a cipher operation */
	CpaBoolean isAuth : 1;
	/**< Flag indicating whether symOperation includes an auth operation */
	CpaBoolean useSymConstantsTable : 1;
	/**< Flag indicating whether the SymConstantsTable can be used or not */
	CpaBoolean useOptimisedContentDesc : 1;
	/**< Flag indicating whether to use the optimised CD or not */
	CpaBoolean isPartialSupported : 1;
	/**< Flag indicating whether symOperation support partial packet */
	CpaBoolean useStatefulSha3ContentDesc : 1;
	/**< Flag indicating whether to use the stateful SHA3 CD or not */
	icp_qat_la_bulk_req_hdr_t shramReqCacheHdr;
	icp_qat_fw_la_key_gen_common_t shramReqCacheMid;
	icp_qat_la_bulk_req_ftr_t shramReqCacheFtr;
	/**< Alternative pre-built request (header. mid & footer)
	 * for use with symConstantsTable. */
	lac_single_pass_state_t singlePassState;
	/**< Flag indicating whether symOperation support single pass */
	icp_qat_fw_serv_specif_flags laCmdFlags;
	/**< Common request - Service specific flags type  */
	icp_qat_fw_comn_flags cmnRequestFlags;
	/**< Common request flags type  */
	icp_qat_fw_ext_serv_specif_flags laExtCmdFlags;
	/**< Common request - Service specific flags type  */
	icp_qat_la_bulk_req_hdr_t reqSpcCacheHdr;
	icp_qat_la_bulk_req_ftr_t reqSpcCacheFtr;
	/**< request (header & footer)for use with Single Pass. */
	icp_qat_hw_auth_mode_t qatHashMode;
	/**< Hash Mode for the qat slices. Not to be confused with QA-API
	 * hashMode
	 */
	Cpa32U cipherSliceType;
	/**< Cipher slice type to be used, set at init session time */
	Cpa8U cipherAesXtsKey1Forward[LAC_CIPHER_AES_XTS_KEY_MAX_LENGTH];
	/**< Cached AES XTS Forward key
	 * For CPM2.0 AES XTS key convertion need to be done in SW.
	 * Because use can update session direction at any time,
	 * also forward key needs to be cached
	 */
	Cpa8U cipherAesXtsKey1Reverse[LAC_CIPHER_AES_XTS_KEY_MAX_LENGTH];
	/**< AES XTS Reverse key
	 * For CPM2.0 AES XTS key convertion need to be done in SW.
	 * Reverse key always will be calcilated at session setup time and
	 * cached to be used when needed */
	Cpa8U cipherAesXtsKey2[LAC_CIPHER_AES_XTS_KEY_MAX_LENGTH];
	/**< For AES XTS session need to store Key2 value in order to generate
	 * tweak
	 */
	void *writeRingMsgFunc;
	/**< function which will be called to write ring message */
	Cpa32U aadLenInBytes;
	/**< For CCM,GCM and Snow3G cases, this parameter holds the AAD size,
	 * otherwise it is set to zero */
	ALIGN_START(64)
	Cpa8U hashStatePrefixBuffer[LAC_MAX_AAD_SIZE_BYTES] ALIGN_END(64);
	/**< hash state prefix buffer used for hash operations - AAD only
	 * NOTE: Field must be correctly aligned in memory for access by QAT
	 * engine
	 */
} lac_session_desc_d2_t;

#define LAC_SYM_SESSION_SIZE                                                   \
	(sizeof(lac_session_desc_t) + LAC_64BYTE_ALIGNMENT +                   \
	 sizeof(LAC_ARCH_UINT))
/**< @ingroup LacSym_Session
 * Size of the memory that the client has to allocate for a session. Extra
 * memory is needed to internally re-align the data. The pointer to the algined
 * data is stored at the start of the user allocated memory hence the extra
 * space for an LAC_ARCH_UINT */

#define LAC_SYM_SESSION_D1_SIZE                                                \
	(sizeof(lac_session_desc_d1_t) + LAC_64BYTE_ALIGNMENT +                \
	 sizeof(LAC_ARCH_UINT))
/**< @ingroup LacSym_Session
**  Size of the memory that the client has to allocate for a session where :
*     - cipher algorithm not ARC4 or Snow3G, no Partials, nonAuthEncrypt.
* Extra memory is needed to internally re-align the data. The pointer to the
* aligned data is stored at the start of the user allocated memory hence the
* extra space for an LAC_ARCH_UINT */

#define LAC_SYM_SESSION_D2_SIZE                                                \
	(sizeof(lac_session_desc_d2_t) + LAC_64BYTE_ALIGNMENT +                \
	 sizeof(LAC_ARCH_UINT))
/**< @ingroup LacSym_Session
**  Size of the memory that the client has to allocate for a session where :
*     - authEncrypt, no Partials - so hashStatePrefixBuffer is only AAD
* Extra memory is needed to internally re-align the data. The pointer to the
* aligned data is stored at the start of the user allocated memory hence the
* extra space for an LAC_ARCH_UINT */

#define LAC_SYM_SESSION_DESC_FROM_CTX_GET(pSession)                            \
	(lac_session_desc_t *)(*(LAC_ARCH_UINT *)pSession)
/**< @ingroup LacSym_Session
 * Retrieve the session descriptor pointer from the session context structure
 * that the user allocates. The pointer to the internally realigned address
 * is stored at the start of the session context that the user allocates */

/**
*******************************************************************************
* @ingroup LacSym_Session
*      This function initializes a session
*
* @description
*      This function is called from the LAC session register API functions.
*      It validates all input parameters. If an invalid parameter is passed,
*      an error is returned to the calling function. If all parameters are valid
*      a symmetric session is initialized
*
* @param[in] instanceHandle_in    Instance Handle
* @param[in] pSymCb               callback function
* @param[in] pSessionSetupData    pointer to the strucutre containing the setup
*data
* @param[in] isDpSession          CPA_TRUE for a data plane session
* @param[out] pSessionCtx         Pointer to session context
*
*
* @retval CPA_STATUS_SUCCESS        Function executed successfully.
* @retval CPA_STATUS_FAIL           Function failed.
* @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
* @retval CPA_STATUS_RESOURCE       Error related to system resources.
*
*/

CpaStatus LacSym_InitSession(const CpaInstanceHandle instanceHandle_in,
			     const CpaCySymCbFunc pSymCb,
			     const CpaCySymSessionSetupData *pSessionSetupData,
			     const CpaBoolean isDpSession,
			     CpaCySymSessionCtx pSessionCtx);

#endif /* LAC_SYM_SESSION_H */
