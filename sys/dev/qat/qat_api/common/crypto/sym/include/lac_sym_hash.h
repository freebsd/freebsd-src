/***************************************************************************
 *
 * <COPYRIGHT_TAG>
 *
 ***************************************************************************/

/**
 *****************************************************************************
 * @file lac_sym_hash.h
 *
 * @defgroup  LacHash  Hash
 *
 * @ingroup LacSym
 *
 * API functions of the Hash component
 *
 * @lld_start
 * @lld_overview
 * There is a single \ref cpaCySym "Symmetric LAC API" for hash, cipher,
 * auth encryption and algorithm chaining. This API is implemented by the
 * \ref LacSym "Symmetric" module. It demultiplexes calls to this API into
 * their basic operation and does some common parameter checking and deals
 * with accesses to the session table.
 *
 * The hash component supports hashing in 3 modes. PLAIN, AUTH and NESTED.
 * Plain mode is used to provide data integrity while auth mode is used to
 * provide integrity as well as its authenticity. Nested mode is inteded
 * for use by non standard HMAC like algorithms such as for the SSL master
 * key secret. Partial packets is supported for both plain and auth modes.
 * In-place and out-of-place processing is supported for all modes. The
 * verify operation is supported for PLAIN and AUTH modes only.
 *
 * The hash component is responsible for implementing the hash specific
 * functionality for initialising a session and for a perform operation.
 * Statistics are maintained in the symmetric \ref CpaCySymStats64 "stats"
 * structure. This module has been seperated out into two. The hash QAT module
 * deals entirely with QAT data structures. The hash module itself has minimal
 * exposure to the QAT data structures.
 *
 * @lld_dependencies
 * - \ref LacCommon
 * - \ref LacSymQat "Symmetric QAT": Hash uses the lookup table provided by
 *   this module to validate user input. Hash also uses this module to build
 *   the hash QAT request message, request param structure, populate the
 *   content descriptor, allocate and populate the hash state prefix buffer.
 *   Hash also registers its function to process the QAT response with this
 *   module.
 * - OSAL : For memory functions, atomics and locking
 *
 * @lld_module_algorithms
 * <b>a. HMAC Precomputes</b>\n
 * HMAC algorithm is specified as follows:
 * \f$ HMAC(msg) = hash((key \oplus opad) \parallel
 * hash((key \oplus ipad) \parallel msg ))\f$.
 * The key is fixed per session, and is padded up to the block size of the
 * algorithm if necessary and xored with the ipad/opad. The following portion
 * of the operation can be precomputed: \f$ hash(key \oplus ipad) \f$ as the
 * output of this intermediate hash will be the same for every perform
 * operation. This intermediate state is the intermediate state of a partial
 * partial packet. It is used as the initialiser state to \f$ hash(msg) \f$.
 * The same applies to \f$ hash(key \oplus ipad) \f$. There is a saving in
 * the data path by the length of time it takes to do two hashes on a block
 * size of data. Note: a partial packet operation generates an intermediate
 * state. The final operation on a partial packet or when a full packet is
 * used applies padding and gives the final hash result. Esentially for the
 * inner hash, a partial packet final is issued on the data, using the
 * precomputed intermediate state and returns the digest.
 *
 * For the HMAC precomputes, \ref LacSymHash_HmacPreCompute(), there are two
 * hash operations done using a internal content descriptor to configure the
 * QAT. A first partial packet is specified as the packet type for the
 * pre-computes as we need the state that uses the initialiser constants
 * specific to the algorithm. The resulting output is copied from the hash
 * state prefix buffer into the QAT content descriptor for the session being
 * initialised. The state is used each perform operation as the initialiser
 * to the algorithm
 *
 * <b>b. AES XCBC Precomputes</b>\n
 * A similar technique to HMAC will be used to generate the precomputes for
 * AES XCBC. In this case a cipher operation will be used to generate the
 * precomputed result. The Pre-compute operation involves deriving 3 128-bit
 *  keys (K1, K2 and K3) from the 128-bit secret key K.
 *
 * - K1 = 0x01010101010101010101010101010101 encrypted with Key K
 * - K2 = 0x02020202020202020202020202020202 encrypted with Key K
 * - K3 = 0x03030303030303030303030303030303 encrypted with Key K
 *
 * A content descriptor  is created with the cipher algorithm set to AES
 * in ECB mode and with the keysize set to 128 bits. The 3 constants, 16 bytes
 * each, are copied into the src buffer and an in-place cipher operation is
 * performed on the 48 bytes. ECB mode does not maintain the state, therefore
 * the 3 keys can be encrypted in one perform. The encrypted result is used by
 * the state2 field in the hash setup block of the content descriptor.
 *
 * The precompute operations use a different lac command ID and thus have a
 * different route in the response path to the symmetric code. In this
 * precompute callback function the output of the precompute operation is
 * copied into the content descriptor for the session being registered.
 *
 * <b>c. AES CCM Precomputes</b>\n
 * The precomputes for AES CCM are trivial, i.e. there is no need to perform
 * a cipher or a digest operation.  Instead, the key is stored directly in
 * the state2 field.
 *
 * <b>d. AES GCM Precomputes</b>\n
 * As with AES XCBC precomputes, a cipher operation will be used to generate
 * the precomputed result for AES GCM.  In this case the Galois Hash
 * Multiplier (H) must be derived and stored in the state2 field.  H is
 * derived by encrypting a 16-byte block of zeroes with the
 * cipher/authentication key, using AES in ECB mode.
 *
 * <b>Key size for Auth algorithms</b>\n
 * <i>Min Size</i>\n
 * RFC 2104 states "The key for HMAC can be of any length. However, less than
 *  L bytes is strongly discouraged as it would decrease the security strength
 *  of the function."
 *
 * FIPS 198a states "The size of the key, K, shall be equal to or greater than
 * L/2, where L is the size of the hash function output."
 *
 * RFC 4434 states "If the key has fewer than 128 bits, lengthen it to exactly
 * 128 bits by padding it on the right with zero bits.
 *
 * A key length of 0 upwards is accepted. It is up to the client to pass in a
 * key that complies with the standard they wish to support.
 *
 * <i>Max Size</i>\n
 * RFC 2104 section 2 states : "Applications that use keys longer than B bytes
 * will first hash the key using H and then use the resultant L byte string
 * as the actual key to HMAC
 *
 * RFC 4434 section 2 states:
 * "If the key is 129 bits or longer, shorten it to exactly 128 bits
 *  by performing the steps in AES-XCBC-PRF-128 (that is, the
 *  algorithm described in this document).  In that re-application of
 *  this algorithm, the key is 128 zero bits; the message is the
 *  too-long current key."
 *
 * We push this up to the client. They need to do the hash operation through
 * the LAC API if the key is greater than the block size of the algorithm. This
 * will reduce the key size to the digest size of the algorithm.
 *
 * RFC 3566 section 4 states:
 * AES-XCBC-MAC-96 is a secret key algorithm.  For use with either ESP or
 * AH a fixed key length of 128-bits MUST be supported.  Key lengths
 * other than 128-bits MUST NOT be supported (i.e., only 128-bit keys are
 * to be used by AES-XCBC-MAC-96).
 *
 * In this case it is up to the client to provide a key that complies with
 * the standards. i.e. exactly 128 bits in size.
 *
 *
 * <b>HMAC-MD5-96 and HMAC-SHA1-96</b>\n
 * HMAC-MD5-96 and HMAC-SHA1-96 are defined as requirements by Look Aside
 * IPsec. The differences between HMAC-SHA1 and HMAC-SHA1-96 are that the
 * digest produced is truncated and there are strict requirements on the
 * size of the key that is used.
 *
 * They are supported in LAC by HMAC-MD5 and HMAC-SHA1. The field
 * \ref CpaCySymHashSetupData::digestResultLenInBytes in the LAC API in
 * bytes needs to be set to 12 bytes. There are also requirements regarding
 * the keysize. It is up to the client to ensure the key size meets the
 * requirements of the standards they are using.
 *
 * RFC 2403: HMAC-MD5-96 Key lengths other than 128-bits MUST NOT be supported.
 * HMAC-MD5-96 produces a 128-bit authenticator value. For use with either
 * ESP or AH, a truncated value using the first 96 bits MUST be supported.
 *
 * RFC2404: HMAC-SHA1-96 Key lengths other than 160- bits MUST NOT be supported
 * HMAC-SHA-1-96 produces a 160-bit authenticator value. For use with either
 * ESP or AH, a truncated value using the first 96 bits MUST be supported.
 *
 * <b>Out of place operations</b>
 * When verify is disabled, the digest will be written to the destination
 * buffer. When verify is enabled, the digest calculated is compared to the
 * digest stored in the source buffer.
 *
 * <b>Partial Packets</b>
 * Partial packets are handled in the \ref LacSym "Symmetric" component for
 * the request. The hash callback function handles the update of the state
 * in the callback.
 *
 *
 * @lld_process_context
 *
 * Session Register Sequence Diagram: For hash modes plain and nested.
 * \msc
 *  APP [label="Application"], SYM [label="Symmetric LAC"],
 *  Achain [label="Alg chain"], Hash, SQAT [label="Symmetric QAT"];
 *
 *  APP=>SYM [ label = "cpaCySymInitSession(cbFunc)",
 *             URL="\ref cpaCySymInitSession()"] ;
 *  SYM=>SYM [ label = "LacSymSession_ParamCheck()",
 *             URL="\ref LacSymSession_ParamCheck()"];
 *  SYM=>Achain [ label = "LacAlgChain_SessionInit()",
 *                URL="\ref LacAlgChain_SessionInit()"];
 *  Achain=>Hash [ label = "LacHash_HashContextCheck()",
 *               URL="\ref LacHash_HashContextCheck()"];
 *  Achain<<Hash [ label="return"];
 *  Achain=>SQAT [ label = "LacSymQat_HashContentDescInit()",
 *               URL="\ref LacSymQat_HashContentDescInit()"];
 *  Achain<<SQAT [ label="return"];
 *  Achain=>Hash [ label = "LacHash_StatePrefixAadBufferInit()",
 *               URL="\ref LacHash_StatePrefixAadBufferInit()"];
 *  Hash=>SQAT [ label = "LacSymQat_HashStatePrefixAadBufferSizeGet()",
 *               URL="\ref LacSymQat_HashStatePrefixAadBufferSizeGet()"];
 *  Hash<<SQAT [ label="return"];
 *  Hash=>SQAT [ label = "LacSymQat_HashStatePrefixAadBufferPopulate()",
 *               URL="\ref LacSymQat_HashStatePrefixAadBufferPopulate()"];
 *  Hash<<SQAT [ label="return"];
 *  Achain<<Hash [ label="return"];
 *  SYM<<Achain [ label = "status" ];
 *  SYM=>SYM [label = "LAC_SYM_STAT_INC", URL="\ref LAC_SYM_STAT_INC"];
 *  APP<<SYM [label = "status"];
 * \endmsc
 *
 * Perform Sequence Diagram: For all 3 modes, full packets and in-place.
 * \msc
 *  APP [label="Application"], SYM [label="Symmetric LAC"],
 *  Achain [label="Alg chain"], Hash, SQAT [label="Symmetric QAT"],
 *  QATCOMMS [label="QAT Comms"];
 *
 *  APP=>SYM [ label = "cpaCySymPerformOp()",
 *             URL="\ref cpaCySymPerformOp()"] ;
 *  SYM=>SYM [ label = "LacSymPerform_BufferParamCheck()",
 *              URL="\ref LacSymPerform_BufferParamCheck()"];
 *  SYM=>Achain [ label = "LacAlgChain_Perform()",
 *                URL="\ref LacAlgChain_Perform()"];
 *  Achain=>Achain [ label = "Lac_MemPoolEntryAlloc()",
 *                  URL="\ref Lac_MemPoolEntryAlloc()"];
 *  Achain=>SQAT [ label = "LacSymQat_packetTypeGet()",
 *               URL="\ref LacSymQat_packetTypeGet()"];
 *  Achain<<SQAT [ label="return"];
 *  Achain=>Achain [ label = "LacBuffDesc_BufferListTotalSizeGet()",
 *                  URL="\ref LacBuffDesc_BufferListTotalSizeGet()"];
 *  Achain=>Hash [ label = "LacHash_PerformParamCheck()",
 *                  URL = "\ref LacHash_PerformParamCheck()"];
 *  Achain<<Hash [ label="status"];
 *  Achain=>SQAT [ label = "LacSymQat_HashRequestParamsPopulate()",
 *               URL="\ref LacSymQat_HashRequestParamsPopulate()"];
 *  Achain<<SQAT [ label="return"];
 *  Achain<<SQAT [ label="cmdFlags"];
 *
 *  Achain=>Achain [ label = "LacBuffDesc_BufferListDescWrite()",
 *               URL="\ref LacBuffDesc_BufferListDescWrite()"];
 *  Achain=>SQAT [ label = "SalQatMsg_CmnMsgAndReqParamsPopulate()",
 *               URL="\ref SalQatMsg_CmnMsgAndReqParamsPopulate()"];
 *  Achain<<SQAT [ label="return"];
 *  Achain=>SYM [ label = "LacSymQueue_RequestSend()",
 *                URL="\ref LacSymQueue_RequestSend()"];
 *  SYM=>QATCOMMS [ label = "QatComms_MsgSend()",
 *                   URL="\ref QatComms_MsgSend()"];
 *  SYM<<QATCOMMS [ label="status"];
 *  Achain<<SYM   [ label="status"];
 *  SYM<<Achain [ label="status"];
 *  SYM=>SYM [label = "LAC_SYM_STAT_INC", URL="\ref LAC_SYM_STAT_INC"];
 *  APP<<SYM [label = "status"];
 *  ... [label = "QAT processing the request and generates response.
 *       Callback in Bottom Half Context"];
 *  ...;
 *  QATCOMMS=>QATCOMMS [label ="QatComms_ResponseMsgHandler()",
 *                       URL="\ref QatComms_ResponseMsgHandler()"];
 *  QATCOMMS=>SQAT [label ="LacSymQat_SymRespHandler()",
 *                   URL="\ref LacSymQat_SymRespHandler()"];
 *  SQAT=>SYM [label="LacSymCb_ProcessCallback()",
 *              URL="\ref LacSymCb_ProcessCallback()"];
 *  SYM=>SYM [label = "LacSymCb_ProcessCallbackInternal()",
 *            URL="\ref LacSymCb_ProcessCallbackInternal()"];
 *  SYM=>SYM [label = "Lac_MemPoolEntryFree()",
 *            URL="\ref Lac_MemPoolEntryFree()"];
 *  SYM=>SYM [label = "LAC_SYM_STAT_INC", URL="\ref LAC_SYM_STAT_INC"];
 *  SYM=>APP [label="cbFunc"];
 *  APP>>SYM [label="return"];
 *  SYM>>SQAT [label="return"];
 *  SQAT>>QATCOMMS [label="return"];
 * \endmsc
 *
 * @lld_end
 *
 *****************************************************************************/

/*****************************************************************************/

#ifndef LAC_SYM_HASH_H
#define LAC_SYM_HASH_H

/*
******************************************************************************
* Include public/global header files
******************************************************************************
*/

#include "cpa.h"
#include "cpa_cy_sym.h"

/*
*******************************************************************************
* Include private header files
*******************************************************************************
*/

#include "lac_session.h"
#include "lac_buffer_desc.h"

/**
 *****************************************************************************
 * @ingroup LacHash
 *      Definition of callback function.
 *
 * @description
 *      This is the callback function prototype. The callback function is
 *      invoked when a hash precompute operation completes.
 *
 * @param[in] pCallbackTag  Opaque value provided by user while making
 *                         individual function call.
 *
 * @retval
 *      None
 *****************************************************************************/
typedef void (*lac_hash_precompute_done_cb_t)(void *pCallbackTag);

/*
 * WARNING: There are no checks done on the parameters of the functions in
 * this file. The expected values of the parameters are documented and it is
 * up to the caller to provide valid values.
 */

/**
*******************************************************************************
* @ingroup LacHash
*      validate the hash context
*
* @description
*      The client populates the hash context in the session context structure
*      This is passed as parameter to the session register API function and
*      needs to be validated.
*
* @param[in] pHashSetupData      pointer to hash context structure
*
* @retval CPA_STATUS_SUCCESS        Success
* @retval CPA_STATUS_INVALID_PARAM  Invalid parameter
*
*****************************************************************************/
CpaStatus LacHash_HashContextCheck(CpaInstanceHandle instanceHandle,
				   const CpaCySymHashSetupData *pHashSetupData);

/**
 ******************************************************************************
 * @ingroup LacHash
 *      Populate the hash pre-compute data.
 *
 * @description
 *      This function populates the state1 and state2 fields with the hash
 *      pre-computes.  This is only done for authentication.  The state1
 *      and state2 pointers must be set to point to the correct locations
 *      in the content descriptor where the precompute result(s) will be
 *      written, before this function is called.
 *
 * @param[in] instanceHandle    Instance Handle
 * @param[in] pSessionSetup     pointer to session setup data
 * @param[in] callbackFn        Callback function which is invoked when
 *                              the precompute operation is completed
 * @param[in] pCallbackTag       Opaque data which is passed back to the user
 *                              as a parameter in the callback function
 * @param[out] pWorkingBuffer   Pointer to working buffer, sufficient memory
 *                              must be allocated by the caller for this.
 *                              Assumption that this is 8 byte aligned.
 * @param[out] pState1          pointer to State 1 in content descriptor
 * @param[out] pState2          pointer to State 2 in content descriptor
 *
 * @retval CPA_STATUS_SUCCESS    Success
 * @retval CPA_STATUS_RETRY      Retry the operation.
 * @retval CPA_STATUS_RESOURCE   Error Allocating memory
 * @retval CPA_STATUS_FAIL       Operation Failed
 *
 *****************************************************************************/
CpaStatus LacHash_PrecomputeDataCreate(const CpaInstanceHandle instanceHandle,
				       CpaCySymSessionSetupData *pSessionSetup,
				       lac_hash_precompute_done_cb_t callbackFn,
				       void *pCallbackTag,
				       Cpa8U *pWorkingBuffer,
				       Cpa8U *pState1,
				       Cpa8U *pState2);

/**
 ******************************************************************************
 * @ingroup LacHash
 *      populate the hash state prefix aad buffer.
 *
 * @description
 *      This function populates the hash state prefix aad buffer. This function
 *      is not called for CCM/GCM operations as the AAD data varies per request
 *      and is stored in the cookie as opposed to the session descriptor.
 *
 * @param[in] pHashSetupData        pointer to hash setup structure
 * @param[in] pHashControlBlock     pointer to hash control block
 * @param[in] qatHashMode           QAT Mode for hash
 * @param[in] pHashStateBuffer      pointer to hash state prefix aad buffer
 * @param[in] pHashStateBufferInfo  Pointer to hash state prefix buffer info
 *
 * @retval CPA_STATUS_SUCCESS       Success
 * @retval CPA_STATUS_FAIL          Operation Failed
 *
 *****************************************************************************/
CpaStatus LacHash_StatePrefixAadBufferInit(
    sal_service_t *pService,
    const CpaCySymHashSetupData *pHashSetupData,
    icp_qat_la_bulk_req_ftr_t *pHashControlBlock,
    icp_qat_hw_auth_mode_t qatHashMode,
    Cpa8U *pHashStateBuffer,
    lac_sym_qat_hash_state_buffer_info_t *pHashStateBufferInfo);

/**
*******************************************************************************
* @ingroup LacHash
*      Check parameters for a hash perform operation
*
* @description
*      This function checks the parameters for a hash perform operation.
*
* @param[in] pSessionDesc        Pointer to session descriptor.
* @param[in] pOpData             Pointer to request parameters.
* @param[in] srcPktSize          Total size of the Buffer List
* @param[in] pVerifyResult       Pointer to user flag
*
* @retval CPA_STATUS_SUCCESS       Success
* @retval CPA_STATUS_INVALID_PARAM Invalid Parameter
*
*****************************************************************************/
CpaStatus LacHash_PerformParamCheck(CpaInstanceHandle instanceHandle,
				    lac_session_desc_t *pSessionDesc,
				    const CpaCySymOpData *pOpData,
				    Cpa64U srcPktSize,
				    const CpaBoolean *pVerifyResult);

/**
*******************************************************************************
* @ingroup LacHash
*      Perform hash precompute operation for HMAC
*
* @description
*      This function sends 2 requests to the CPM for the hmac precompute
*      operations. The results of the ipad and opad state calculation
*      is copied into pState1 and pState2 (e.g. these may be the state1 and
*      state2 buffers in a hash content desciptor) and when
*      the final operation has completed the condition passed as a param to
*      this function is set to true.
*
*      This function performs the XORing of the IPAD and OPAD constants to
*      the key (which was padded to the block size of the algorithm)
*
* @param[in]  instanceHandle       Instance Handle
* @param[in]  hashAlgorithm        Hash Algorithm
* @param[in]  authKeyLenInBytes    Length of Auth Key
* @param[in]  pAuthKey             Pointer to Auth Key
* @param[out] pWorkingMemory       Pointer to working memory that is carved
*                                  up and used in the pre-compute operations.
*                                  Assumption that this is 8 byte aligned.
* @param[out] pState1              Pointer to State 1 in content descriptor
* @param[out] pState2              Pointer to State 2 in content descriptor
* @param[in]  callbackFn           Callback function which is invoked when
*                                  the precompute operation is completed
* @param[in]  pCallbackTag         Opaque data which is passed back to the user
*                                  as a parameter in the callback function
*
* @retval CPA_STATUS_SUCCESS       Success
* @retval CPA_STATUS_RETRY         Retry the operation.
* @retval CPA_STATUS_FAIL          Operation Failed
*
*****************************************************************************/
CpaStatus LacSymHash_HmacPreComputes(CpaInstanceHandle instanceHandle,
				     CpaCySymHashAlgorithm hashAlgorithm,
				     Cpa32U authKeyLenInBytes,
				     Cpa8U *pAuthKey,
				     Cpa8U *pWorkingMemory,
				     Cpa8U *pState1,
				     Cpa8U *pState2,
				     lac_hash_precompute_done_cb_t callbackFn,
				     void *pCallbackTag);

/**
*******************************************************************************
 * @ingroup LacHash
 *      Perform hash precompute operation for XCBC MAC and GCM
 *
 * @description
 *      This function sends 1 request to the CPM for the precompute operation
 *      based on an AES ECB cipher. The results of the calculation is copied
 *      into pState (this may be a pointer to the State 2 buffer in a Hash
 *      content descriptor) and when the operation has completed the condition
 *      passed as a param to this function is set to true.
 *
 * @param[in]  instanceHandle       Instance Handle
 * @param[in]  hashAlgorithm        Hash Algorithm
 * @param[in]  authKeyLenInBytes    Length of Auth Key
 * @param[in]  pAuthKey             Auth Key
 * @param[out] pWorkingMemory       Pointer to working memory that is carved
 *                                  up and used in the pre-compute operations.
 *                                  Assumption that this is 8 byte aligned.
 * @param[out] pState               Pointer to output state
 * @param[in]  callbackFn           Callback function which is invoked when
 *                                  the precompute operation is completed
 * @param[in]  pCallbackTag         Opaque data which is passed back to the user
 *                                  as a parameter in the callback function

 *
 * @retval CPA_STATUS_SUCCESS       Success
 * @retval CPA_STATUS_RETRY         Retry the operation.
 * @retval CPA_STATUS_FAIL          Operation Failed
 *
 *****************************************************************************/
CpaStatus LacSymHash_AesECBPreCompute(CpaInstanceHandle instanceHandle,
				      CpaCySymHashAlgorithm hashAlgorithm,
				      Cpa32U authKeyLenInBytes,
				      Cpa8U *pAuthKey,
				      Cpa8U *pWorkingMemory,
				      Cpa8U *pState,
				      lac_hash_precompute_done_cb_t callbackFn,
				      void *pCallbackTag);

/**
*******************************************************************************
* @ingroup LacHash
*      initialise data structures for the hash precompute operations
*
* @description
*      This function registers the precompute callback handler function, which
*      is different to the default one used by symmetric. Content desciptors
*      are preallocted for the hmac precomputes as they are constant for these
*      operations.
*
* @retval CPA_STATUS_SUCCESS       Success
* @retval CPA_STATUS_RESOURCE      Error allocating memory
*
*****************************************************************************/
CpaStatus LacSymHash_HmacPrecompInit(CpaInstanceHandle instanceHandle);

/**
*******************************************************************************
* @ingroup LacHash
*      free resources allocated for the precompute operations
*
* @description
*      free up the memory allocated on init time for the content descriptors
*      that were allocated for the HMAC precompute operations.
*
* @return none
*
*****************************************************************************/
void LacSymHash_HmacPrecompShutdown(CpaInstanceHandle instanceHandle);

void LacSync_GenBufListVerifyCb(void *pCallbackTag,
				CpaStatus status,
				CpaCySymOp operationType,
				void *pOpData,
				CpaBufferList *pDstBuffer,
				CpaBoolean opResult);

#endif /* LAC_SYM_HASH_H */
