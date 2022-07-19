/***************************************************************************
 *
 *   BSD LICENSE
 * 
 *   Copyright(c) 2007-2022 Intel Corporation. All rights reserved.
 *   All rights reserved.
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
 * 
 *
 ***************************************************************************/

/*
 *****************************************************************************
 * Doxygen group definitions
 ****************************************************************************/

/**
 *****************************************************************************
 * @file cpa_cy_dsa.h
 *
 * @defgroup cpaCyDsa Digital Signature Algorithm (DSA) API
 *
 * @ingroup cpaCy
 *
 * @description
 *      These functions specify the API for Public Key Encryption
 *      (Cryptography) Digital Signature Algorithm (DSA) operations.
 *
 *      Support is provided for FIPS PUB 186-2 with Change Notice 1
 *      specification, and optionally for FIPS PUB 186-3.  If an
 *      implementation does not support FIPS PUB 186-3, then the
 *      corresponding functions may return a status of @ref
 *      CPA_STATUS_FAIL.
 *
 *      Support for FIPS PUB 186-2 with Change Notice 1 implies supporting
 *      the following choice for the pair L and N:
 *      - L = 1024, N = 160
 *
 *      Support for FIPS PUB 186-3 implies supporting the following choices
 *      for the pair L and N:
 *
 *      - L = 1024, N = 160
 *      - L = 2048, N = 224
 *      - L = 2048, N = 256
 *      - L = 3072, N = 256
 *
 *      Only the modular math aspects of DSA parameter generation and message
 *      signature generation and verification are implemented here.  For full
 *      DSA support, this DSA API SHOULD be used in conjunction with other
 *      parts of this overall Cryptographic API. In particular the Symmetric
 *      functions (for hashing), the Random Number Generation functions, and
 *      the Prime Number Test functions will be required.
 *
 * @note
 *      Large numbers are represented on the QuickAssist API as described
 *      in the Large Number API (@ref cpaCyLn).
 *****************************************************************************/

#ifndef CPA_CY_DSA_H
#define CPA_CY_DSA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cpa_cy_common.h"

/**
 *****************************************************************************
 * @ingroup cpaCyDsa
 *      DSA P Parameter Generation Operation Data.
 * @description
 *      This structure contains the operation data for the cpaCyDsaGenPParam
 *      function. The client MUST allocate the memory for this structure and the
 *      items pointed to by this structure. When the structure is passed into
 *      the function, ownership of the memory passes to the function. Ownership
 *      of the memory returns to the client when this structure is returned in
 *      the callback function.
 *
 *      For optimal performance all data buffers SHOULD be 8-byte aligned.
 *
 *      All values in this structure are required to be in Most Significant Byte
 *      first order, e.g. X.pData[0] = MSB.
 *
 * @note
 *      If the client modifies or frees the memory referenced in this
 *      structure after it has been submitted to the cpaCyDsaGenPParam
 *      function, and before it has been returned in the callback, undefined
 *      behavior will result.
 *
 * @see
 *      cpaCyDsaGenPParam()
 *
 *****************************************************************************/
typedef struct _CpaCyDsaPParamGenOpData {
    CpaFlatBuffer X;
    /**< 2^(L-1) <= X < 2^L (from FIPS 186-3) */
    CpaFlatBuffer Q;
    /**< DSA group parameter q */
} CpaCyDsaPParamGenOpData;

/**
 *****************************************************************************
 * @ingroup cpaCyDsa
 *      DSA G Parameter Generation Operation Data.
 * @description
 *      This structure contains the operation data for the cpaCyDsaGenGParam
 *      function. The client MUST allocate the memory for this structure and the
 *      items pointed to by this structure. When the structure is passed into
 *      the function, ownership of the memory passes to the function. Ownership
 *      of the memory returns to the client when this structure is returned in
 *      the callback function.
 *
 *      All values in this structure are required to be in Most Significant Byte
 *      first order, e.g. P.pData[0] = MSB.
 *
 *      All numbers MUST be stored in big-endian order.
 *
 * @note
 *      If the client modifies or frees the memory referenced in this
 *      structure after it has been submitted to the cpaCyDsaGenGParam
 *      function, and before it has been returned in the callback, undefined
 *      behavior will result.
 *
 * @see
 *      cpaCyDsaGenGParam()
 *
 *****************************************************************************/
typedef struct _CpaCyDsaGParamGenOpData {
    CpaFlatBuffer P;
    /**< DSA group parameter p */
    CpaFlatBuffer Q;
    /**< DSA group parameter q */
    CpaFlatBuffer H;
    /**< any integer with 1 < h < p - 1 */
} CpaCyDsaGParamGenOpData;

/**
 *****************************************************************************
 * @ingroup cpaCyDsa
 *      DSA Y Parameter Generation Operation Data.
 * @description
 *      This structure contains the operation data for the cpaCyDsaGenYParam
 *      function. The client MUST allocate the memory for this structure and the
 *      items pointed to by this structure. When the structure is passed into
 *      the function, ownership of the memory passes to the function. Ownership
 *      of the memory returns to the client when this structure is returned in
 *      the callback function.
 *
 *      For optimal performance all data SHOULD be 8-byte aligned.
 *
 *      All values in this structure are required to be in Most Significant Byte
 *      first order, e.g. P.pData[0] = MSB.
 *
 * @note
 *      If the client modifies or frees the memory referenced in this
 *      structure after it has been submitted to the cpaCyDsaGenYParam
 *      function, and before it has been returned in the callback, undefined
 *      behavior will result.
 *
 * @see
 *      cpaCyDsaGenYParam()
 *
 *****************************************************************************/
typedef struct _CpaCyDsaYParamGenOpData {
    CpaFlatBuffer P;
    /**< DSA group parameter p */
    CpaFlatBuffer G;
    /**< DSA group parameter g */
    CpaFlatBuffer X;
    /**< DSA private key x */
} CpaCyDsaYParamGenOpData;

/**
 *****************************************************************************
 * @ingroup cpaCyDsa
 *      DSA R Sign Operation Data.
 * @description
 *      This structure contains the operation data for the cpaCyDsaSignR
 *      function. The client MUST allocate the memory for this structure and the
 *      items pointed to by this structure. When the structure is passed into
 *      the function, ownership of the memory passes to the function. Ownership
 *      of the memory returns to the client when this structure is returned in
 *      the callback function.
 *
 *      For optimal performance all data SHOULD be 8-byte aligned.
 *
 *      All values in this structure are required to be in Most Significant Byte
 *      first order, e.g. P.pData[0] = MSB.
 *
 * @note
 *      If the client modifies or frees the memory referenced in this
 *      structure after it has been submitted to the cpaCyDsaSignR
 *      function, and before it has been returned in the callback, undefined
 *      behavior will result.
 *
 * @see
 *      cpaCyDsaSignR()
 *
 *****************************************************************************/
typedef struct _CpaCyDsaRSignOpData {
    CpaFlatBuffer P;
    /**< DSA group parameter p */
    CpaFlatBuffer Q;
    /**< DSA group parameter q */
    CpaFlatBuffer G;
    /**< DSA group parameter g */
    CpaFlatBuffer K;
    /**< DSA secret parameter k for signing */
} CpaCyDsaRSignOpData;

/**
 *****************************************************************************
 * @ingroup cpaCyDsa
 *      DSA S Sign Operation Data.
 * @description
 *      This structure contains the operation data for the cpaCyDsaSignS
 *      function. The client MUST allocate the memory for this structure and
 *      the items pointed to by this structure. When the structure is passed
 *      into the function, ownership of the memory passes to the function.
 *      Ownership of the memory returns to the client when this structure is
 *      returned in the callback function.
 *
 *      For optimal performance all data SHOULD be 8-byte aligned.
 *
 *      All values in this structure are required to be in Most Significant Byte
 *      first order, e.g. Q.pData[0] = MSB.
 *
 * @note
 *      If the client modifies or frees the memory referenced in this
 *      structure after it has been submitted to the cpaCyDsaSignS
 *      function, and before it has been returned in the callback, undefined
 *      behavior will result.
 *
 * @see
 *      cpaCyDsaSignS()
 *
 *****************************************************************************/
typedef struct _CpaCyDsaSSignOpData {
    CpaFlatBuffer Q;
    /**< DSA group parameter q */
    CpaFlatBuffer X;
    /**< DSA private key x */
    CpaFlatBuffer K;
    /**< DSA secret parameter k for signing */
    CpaFlatBuffer R;
    /**< DSA message signature r */
    CpaFlatBuffer Z;
    /**< The leftmost min(N, outlen) bits of Hash(M), where:
     * - N is the bit length of q
     * - outlen is the bit length of the hash function output block
     * - M is the message to be signed
     */
} CpaCyDsaSSignOpData;

/**
 *****************************************************************************
 * @ingroup cpaCyDsa
 *      DSA R & S Sign Operation Data.
 * @description
 *      This structure contains the operation data for the cpaCyDsaSignRS
 *      function. The client MUST allocate the memory for this structure and the
 *      items pointed to by this structure. When the structure is passed into
 *      the function, ownership of the memory passes to the function. Ownership
 *      of the memory returns to the client when this structure is returned in
 *      the callback function.
 *
 *      For optimal performance all data SHOULD be 8-byte aligned.
 *
 *      All values in this structure are required to be in Most Significant Byte
 *      first order, e.g. P.pData[0] = MSB.
 *
 * @note
 *      If the client modifies or frees the memory referenced in this
 *      structure after it has been submitted to the cpaCyDsaSignRS
 *      function, and before it has been returned in the callback, undefined
 *      behavior will result.
 *
 * @see
 *      cpaCyDsaSignRS()
 *
 *****************************************************************************/
typedef struct _CpaCyDsaRSSignOpData {
    CpaFlatBuffer P;
    /**< DSA group parameter p */
    CpaFlatBuffer Q;
    /**< DSA group parameter q */
    CpaFlatBuffer G;
    /**< DSA group parameter g */
    CpaFlatBuffer X;
    /**< DSA private key x */
    CpaFlatBuffer K;
    /**< DSA secret parameter k for signing */
    CpaFlatBuffer Z;
    /**< The leftmost min(N, outlen) bits of Hash(M), where:
     * - N is the bit length of q
     * - outlen is the bit length of the hash function output block
     * - M is the message to be signed
     */
} CpaCyDsaRSSignOpData;

/**
 *****************************************************************************
 * @ingroup cpaCyDsa
 *      DSA Verify Operation Data.
 * @description
 *      This structure contains the operation data for the cpaCyDsaVerify
 *      function. The client MUST allocate the memory for this structure and the
 *      items pointed to by this structure. When the structure is passed into
 *      the function, ownership of the memory passes to the function. Ownership
 *      of the memory returns to the client when this structure is returned in
 *      the callback function.
 *
 *      For optimal performance all data SHOULD be 8-byte aligned.
 *
 *      All values in this structure are required to be in Most Significant Byte
 *      first order, e.g. P.pData[0] = MSB.
 *
 * @note
 *      If the client modifies or frees the memory referenced in this
 *      structure after it has been submitted to the cpaCyDsaVerify
 *      function, and before it has been returned in the callback, undefined
 *      behavior will result.
 *
 * @see
 *      cpaCyDsaVerify()
 *
 *****************************************************************************/
typedef struct _CpaCyDsaVerifyOpData {
    CpaFlatBuffer P;
    /**< DSA group parameter p */
    CpaFlatBuffer Q;
    /**< DSA group parameter q */
    CpaFlatBuffer G;
    /**< DSA group parameter g */
    CpaFlatBuffer Y;
    /**< DSA public key y */
    CpaFlatBuffer Z;
    /**< The leftmost min(N, outlen) bits of Hash(M'), where:
     * - N is the bit length of q
     * - outlen is the bit length of the hash function output block
     * - M is the message to be signed
     */
    CpaFlatBuffer R;
    /**< DSA message signature r */
    CpaFlatBuffer S;
    /**< DSA message signature s */
} CpaCyDsaVerifyOpData;

/**
 *****************************************************************************
 * @ingroup cpaCyDsa
 *      Cryptographic DSA Statistics.
 * @deprecated
 *      As of v1.3 of the Crypto API, this structure has been deprecated,
 *      replaced by @ref CpaCyDsaStats64.
 * @description
 *      This structure contains statistics on the Cryptographic DSA
 *      operations. Statistics are set to zero when the component is
 *      initialized, and are collected per instance.
 ****************************************************************************/
typedef struct _CpaCyDsaStats {
    Cpa32U numDsaPParamGenRequests;
    /**< Total number of successful DSA P parameter generation requests. */
    Cpa32U numDsaPParamGenRequestErrors;
    /**< Total number of DSA P parameter generation requests that had an
     * error and could not be processed. */
    Cpa32U numDsaPParamGenCompleted;
    /**< Total number of DSA P parameter generation operations that
     * completed successfully. */
    Cpa32U numDsaPParamGenCompletedErrors;
    /**< Total number of DSA P parameter generation operations that could
     * not be completed successfully due to errors. */
    Cpa32U numDsaGParamGenRequests;
    /**< Total number of successful DSA G parameter generation requests. */
    Cpa32U numDsaGParamGenRequestErrors;
    /**< Total number of DSA G parameter generation requests that had an
     * error and could not be processed. */
    Cpa32U numDsaGParamGenCompleted;
    /**< Total number of DSA G parameter generation operations that
     * completed successfully. */
    Cpa32U numDsaGParamGenCompletedErrors;
    /**< Total number of DSA G parameter generation operations that could
     * not be completed successfully due to errors. */
    Cpa32U numDsaYParamGenRequests;
    /**< Total number of successful DSA Y parameter generation requests. */
    Cpa32U numDsaYParamGenRequestErrors;
    /**< Total number of DSA Y parameter generation requests that had an
     * error and could not be processed. */
    Cpa32U numDsaYParamGenCompleted;
    /**< Total number of DSA Y parameter generation operations that
     * completed successfully. */
    Cpa32U numDsaYParamGenCompletedErrors;
    /**< Total number of DSA Y parameter generation operations that could
     * not be completed successfully due to errors. */
    Cpa32U numDsaRSignRequests;
    /**< Total number of successful DSA R sign generation requests. */
    Cpa32U numDsaRSignRequestErrors;
    /**< Total number of DSA R sign requests that had an error and could
     * not be processed. */
    Cpa32U numDsaRSignCompleted;
    /**< Total number of DSA R sign operations that completed
     * successfully. */
    Cpa32U numDsaRSignCompletedErrors;
    /**< Total number of DSA R sign operations that could not be completed
     * successfully due to errors. */
    Cpa32U numDsaSSignRequests;
    /**< Total number of successful DSA S sign generation requests. */
    Cpa32U numDsaSSignRequestErrors;
    /**< Total number of DSA S sign requests that had an error and could
     * not be processed. */
    Cpa32U numDsaSSignCompleted;
    /**< Total number of DSA S sign operations that completed
     * successfully. */
    Cpa32U numDsaSSignCompletedErrors;
    /**< Total number of DSA S sign operations that could not be completed
     * successfully due to errors. */
    Cpa32U numDsaRSSignRequests;
    /**< Total number of successful DSA RS sign generation requests. */
    Cpa32U numDsaRSSignRequestErrors;
    /**< Total number of DSA RS sign requests that had an error and could
     * not be processed. */
    Cpa32U numDsaRSSignCompleted;
    /**< Total number of DSA RS sign operations that completed
     * successfully. */
    Cpa32U numDsaRSSignCompletedErrors;
    /**< Total number of DSA RS sign operations that could not be completed
     * successfully due to errors. */
    Cpa32U numDsaVerifyRequests;
    /**< Total number of successful DSA verify generation requests. */
    Cpa32U numDsaVerifyRequestErrors;
    /**< Total number of DSA verify requests that had an error and could
     * not be processed. */
    Cpa32U numDsaVerifyCompleted;
    /**< Total number of DSA verify operations that completed
     * successfully. */
    Cpa32U numDsaVerifyCompletedErrors;
    /**< Total number of DSA verify operations that could not be completed
     * successfully due to errors. */
    Cpa32U numDsaVerifyFailures;
    /**< Total number of DSA verify operations that executed successfully
     * but the outcome of the test was that the verification failed.
     * Note that this does not indicate an error. */
} CpaCyDsaStats CPA_DEPRECATED;

/**
 *****************************************************************************
 * @ingroup cpaCyDsa
 *      Cryptographic DSA Statistics (64-bit version).
 * @description
 *      This structure contains 64-bit version of the statistics on the
 *      Cryptographic DSA operations.
 *      Statistics are set to zero when the component is
 *      initialized, and are collected per instance.
 ****************************************************************************/
typedef struct _CpaCyDsaStats64 {
    Cpa64U numDsaPParamGenRequests;
    /**< Total number of successful DSA P parameter generation requests. */
    Cpa64U numDsaPParamGenRequestErrors;
    /**< Total number of DSA P parameter generation requests that had an
     * error and could not be processed. */
    Cpa64U numDsaPParamGenCompleted;
    /**< Total number of DSA P parameter generation operations that
     * completed successfully. */
    Cpa64U numDsaPParamGenCompletedErrors;
    /**< Total number of DSA P parameter generation operations that could
     * not be completed successfully due to errors. */
    Cpa64U numDsaGParamGenRequests;
    /**< Total number of successful DSA G parameter generation requests. */
    Cpa64U numDsaGParamGenRequestErrors;
    /**< Total number of DSA G parameter generation requests that had an
     * error and could not be processed. */
    Cpa64U numDsaGParamGenCompleted;
    /**< Total number of DSA G parameter generation operations that
     * completed successfully. */
    Cpa64U numDsaGParamGenCompletedErrors;
    /**< Total number of DSA G parameter generation operations that could
     * not be completed successfully due to errors. */
    Cpa64U numDsaYParamGenRequests;
    /**< Total number of successful DSA Y parameter generation requests. */
    Cpa64U numDsaYParamGenRequestErrors;
    /**< Total number of DSA Y parameter generation requests that had an
     * error and could not be processed. */
    Cpa64U numDsaYParamGenCompleted;
    /**< Total number of DSA Y parameter generation operations that
     * completed successfully. */
    Cpa64U numDsaYParamGenCompletedErrors;
    /**< Total number of DSA Y parameter generation operations that could
     * not be completed successfully due to errors. */
    Cpa64U numDsaRSignRequests;
    /**< Total number of successful DSA R sign generation requests. */
    Cpa64U numDsaRSignRequestErrors;
    /**< Total number of DSA R sign requests that had an error and could
     * not be processed. */
    Cpa64U numDsaRSignCompleted;
    /**< Total number of DSA R sign operations that completed
     * successfully. */
    Cpa64U numDsaRSignCompletedErrors;
    /**< Total number of DSA R sign operations that could not be completed
     * successfully due to errors. */
    Cpa64U numDsaSSignRequests;
    /**< Total number of successful DSA S sign generation requests. */
    Cpa64U numDsaSSignRequestErrors;
    /**< Total number of DSA S sign requests that had an error and could
     * not be processed. */
    Cpa64U numDsaSSignCompleted;
    /**< Total number of DSA S sign operations that completed
     * successfully. */
    Cpa64U numDsaSSignCompletedErrors;
    /**< Total number of DSA S sign operations that could not be completed
     * successfully due to errors. */
    Cpa64U numDsaRSSignRequests;
    /**< Total number of successful DSA RS sign generation requests. */
    Cpa64U numDsaRSSignRequestErrors;
    /**< Total number of DSA RS sign requests that had an error and could
     * not be processed. */
    Cpa64U numDsaRSSignCompleted;
    /**< Total number of DSA RS sign operations that completed
     * successfully. */
    Cpa64U numDsaRSSignCompletedErrors;
    /**< Total number of DSA RS sign operations that could not be completed
     * successfully due to errors. */
    Cpa64U numDsaVerifyRequests;
    /**< Total number of successful DSA verify generation requests. */
    Cpa64U numDsaVerifyRequestErrors;
    /**< Total number of DSA verify requests that had an error and could
     * not be processed. */
    Cpa64U numDsaVerifyCompleted;
    /**< Total number of DSA verify operations that completed
     * successfully. */
    Cpa64U numDsaVerifyCompletedErrors;
    /**< Total number of DSA verify operations that could not be completed
     * successfully due to errors. */
    Cpa64U numDsaVerifyFailures;
    /**< Total number of DSA verify operations that executed successfully
     * but the outcome of the test was that the verification failed.
     * Note that this does not indicate an error. */
} CpaCyDsaStats64;

/**
 *****************************************************************************
 * @ingroup cpaCyDsa
 *      Definition of a generic callback function invoked for a number of the
 *      DSA API functions..
 *
 * @description
 *      This is the prototype for the cpaCyDsaGenCbFunc callback function.
 *
 * @context
 *      This callback function can be executed in a context that DOES NOT
 *      permit sleeping to occur.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in] pCallbackTag     User-supplied value to help identify request.
 * @param[in] status           Status of the operation. Valid values are
 *                             CPA_STATUS_SUCCESS, CPA_STATUS_FAIL and
 *                             CPA_STATUS_UNSUPPORTED.
 * @param[in] pOpData          Opaque pointer to Operation data supplied in
 *                             request.
 * @param[in] protocolStatus   The result passes/fails the DSA protocol
 *                             related checks.
 * @param[in] pOut             Output data from the request.
 *
 * @retval
 *      None
 * @pre
 *      Component has been initialized.
 * @post
 *      None
 * @note
 *      None
 * @see
 *      cpaCyDsaGenPParam()
 *         cpaCyDsaGenGParam()
 *         cpaCyDsaSignR()
 *         cpaCyDsaSignS()
 *
 *****************************************************************************/
typedef void (*CpaCyDsaGenCbFunc)(void *pCallbackTag,
        CpaStatus status,
        void *pOpData,
        CpaBoolean protocolStatus,
        CpaFlatBuffer *pOut);

/**
 *****************************************************************************
 * @ingroup cpaCyDsa
 *      Definition of callback function invoked for cpaCyDsaSignRS
 *      requests.
 *
 * @description
 *      This is the prototype for the cpaCyDsaSignRS callback function, which
 *      will provide the DSA message signature r and s parameters.
 *
 * @context
 *      This callback function can be executed in a context that DOES NOT
 *      permit sleeping to occur.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in] pCallbackTag     User-supplied value to help identify request.
 * @param[in] status           Status of the operation. Valid values are
 *                             CPA_STATUS_SUCCESS, CPA_STATUS_FAIL and
 *                             CPA_STATUS_UNSUPPORTED.
 * @param[in] pOpData          Operation data pointer supplied in request.
 * @param[in] protocolStatus   The result passes/fails the DSA protocol
 *                             related checks.
 * @param[in] pR               DSA message signature r.
 * @param[in] pS               DSA message signature s.
 *
 *
 * @retval
 *      None
 * @pre
 *      Component has been initialized.
 * @post
 *      None
 * @note
 *      None
 * @see
 *      cpaCyDsaSignRS()
 *
 *****************************************************************************/
typedef void (*CpaCyDsaRSSignCbFunc)(void *pCallbackTag,
        CpaStatus status,
        void *pOpData,
        CpaBoolean protocolStatus,
        CpaFlatBuffer *pR,
        CpaFlatBuffer *pS);

/**
 *****************************************************************************
 * @ingroup cpaCyDsa
 *      Definition of callback function invoked for cpaCyDsaVerify
 *      requests.
 *
 * @description
 *      This is the prototype for the cpaCyDsaVerify callback function.
 *
 * @context
 *      This callback function can be executed in a context that DOES NOT
 *      permit sleeping to occur.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in] pCallbackTag     User-supplied value to help identify request.
 * @param[in] status           Status of the operation. Valid values are
 *                             CPA_STATUS_SUCCESS, CPA_STATUS_FAIL and
 *                             CPA_STATUS_UNSUPPORTED.
 * @param[in] pOpData          Operation data pointer supplied in request.
 * @param[in] verifyStatus     The verification passed or failed.
 *
 * @retval
 *      None
 * @pre
 *      Component has been initialized.
 * @post
 *      None
 * @note
 *      None
 * @see
 *      cpaCyDsaVerify()
 *
 *****************************************************************************/
typedef void (*CpaCyDsaVerifyCbFunc)(void *pCallbackTag,
        CpaStatus status,
        void *pOpData,
        CpaBoolean verifyStatus);

/**
 *****************************************************************************
 * @ingroup cpaCyDsa
 *      Generate DSA P Parameter.
 *
 * @description
 *
 *     This function performs FIPS 186-3 Appendix A.1.1.2 steps 11.4 and 11.5,
 *     and part of step 11.7:
 *
 *         11.4. c = X mod 2q.
 *         11.5. p = X - (c - 1).
 *         11.7. Test whether or not p is prime as specified in Appendix C.3.
 *               [Note that a GCD test against ~1400 small primes is performed
 *               on p to eliminate ~94% of composites - this is NOT a "robust"
 *               primality test, as specified in Appendix C.3.]
 *
 *     The protocol status, returned in the callback function as parameter
 *     protocolStatus (or, in the case of synchronous invocation, in the
 *     parameter *pProtocolStatus) is used to indicate whether the value p is
 *     in the right range and has passed the limited primality test.
 *
 *     Specifically, (protocolStatus == CPA_TRUE) means p is in the right range
 *     and  SHOULD be subjected to a robust primality test as specified in
 *     FIPS 186-3 Appendix C.3 (for example, 40 rounds of Miller-Rabin).
 *     Meanwhile, (protocolStatus == CPA_FALSE) means p is either composite,
 *     or p < 2^(L-1), in which case the value of p gets set to zero.
 *
 * @context
 *      When called as an asynchronous function it cannot sleep. It can be
 *      executed in a context that does not permit sleeping.
 *      When called as a synchronous function it may sleep. It MUST NOT be
 *      executed in a context that DOES NOT permit sleeping.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      Yes when configured to operate in synchronous mode.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]  instanceHandle     Instance handle.
 * @param[in]  pCb                Callback function pointer. If this is
 *                                set to a NULL value the function will
 *                                operate synchronously.
 * @param[in]  pCallbackTag       User-supplied value to help identify request.
 * @param[in]  pOpData            Structure containing all the data needed to
 *                                perform the operation. The client code
 *                                allocates the memory for this structure. This
 *                                component takes ownership of the memory until
 *                                it is returned in the callback.
 * @param[out] pProtocolStatus    The result passes/fails the DSA protocol
 *                                related checks.
 * @param[out] pP                 Candidate for DSA parameter p, p odd and
 *                                2^(L-1) < p < X
 *                                On invocation the callback function will
 *                                contain this parameter in the pOut parameter.
 *
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully.
 * @retval CPA_STATUS_FAIL          Function failed.
 * @retval CPA_STATUS_RETRY         Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE      Error related to system resources.
 * @retval CPA_STATUS_RESTARTING    API implementation is restarting. Resubmit
 *                                  the request.
 * @retval CPA_STATUS_UNSUPPORTED   Function is not supported.
 *
 * @pre
 *      The component has been initialized.
 * @post
 *      None
 * @note
 *      When pCb is non-NULL an asynchronous callback of type
 *      CpaCyDsaPParamGenCbFunc is generated in response to this
 *      function call.
 *      For optimal performance, data pointers SHOULD be 8-byte aligned.
 *
 * @see
 *      CpaCyDsaPParamGenOpData,
 *      CpaCyDsaGenCbFunc
 *
 *****************************************************************************/
CpaStatus
cpaCyDsaGenPParam(const CpaInstanceHandle instanceHandle,
        const CpaCyDsaGenCbFunc pCb,
        void * pCallbackTag,
        const CpaCyDsaPParamGenOpData *pOpData,
        CpaBoolean *pProtocolStatus,
        CpaFlatBuffer *pP);

/**
 *****************************************************************************
 * @ingroup cpaCyDsa
 *      Generate DSA G Parameter.
 *
 * @description
 *     This function performs FIPS 186-3 Appendix A.2.1, steps 1 and 3,
 *     and part of step 4:
 *
 *         1. e = (p - 1)/q.
 *         3. Set g = h^e mod p.
 *         4. If (g = 1), then go to step 2.
 *            Here, the implementation will check for g == 1, and return
 *            status accordingly.
 *
 *
 *     The protocol status, returned in the callback function as parameter
 *     protocolStatus (or, in the case of synchronous invocation, in the
 *     parameter *pProtocolStatus) is used to indicate whether the value g is
 *     acceptable.
 *
 *     Specifically, (protocolStatus == CPA_TRUE) means g is acceptable.
 *     Meanwhile, (protocolStatus == CPA_FALSE) means g == 1, so a
 *     different value of h SHOULD be used to generate another value of g.
 *
 * @context
 *      When called as an asynchronous function it cannot sleep. It can be
 *      executed in a context that does not permit sleeping.
 *      When called as a synchronous function it may sleep. It MUST NOT be
 *      executed in a context that DOES NOT permit sleeping.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      Yes when configured to operate in synchronous mode.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]  instanceHandle   Instance handle.
 * @param[in]  pCb              Callback function pointer. If this is set to a
 *                              NULL value the function will operate
 *                              synchronously.
 * @param[in]  pCallbackTag     User-supplied value to help identify request.
 * @param[in]  pOpData          Structure containing all the data needed to
 *                              perform the operation. The client code
 *                              allocates the memory for this structure. This
 *                              component takes ownership of the memory until
 *                              it is returned in the callback.
 * @param[out] pProtocolStatus  The result passes/fails the DSA protocol
 *                              related checks.
 * @param[out] pG               g = h^((p-1)/q) mod p.
 *                              On invocation the callback function will
 *                              contain this parameter in the pOut parameter.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_RETRY          Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE       Error related to system resources.
 * @retval CPA_STATUS_RESTARTING     API implementation is restarting. Resubmit
 *                                   the request.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      The component has been initialized via cpaCyStartInstance function.
 * @post
 *      None
 * @note
 *      When pCb is non-NULL an asynchronous callback of type
 *      CpaCyDsaGParamGenCbFunc is generated in response to this
 *      function call.
 *      For optimal performance, data pointers SHOULD be 8-byte aligned.
 *
 * @see
 *      CpaCyDsaGParamGenOpData,
 *      CpaCyDsaGenCbFunc
 *
 *****************************************************************************/
CpaStatus
cpaCyDsaGenGParam(const CpaInstanceHandle instanceHandle,
        const CpaCyDsaGenCbFunc pCb,
        void *pCallbackTag,
        const CpaCyDsaGParamGenOpData *pOpData,
        CpaBoolean *pProtocolStatus,
        CpaFlatBuffer *pG);

/**
 *****************************************************************************
 * @ingroup cpaCyDsa
 *      Generate DSA Y Parameter.
 *
 * @description
 *
 *     This function performs modular exponentiation to generate y as
 *     described in FIPS 186-3 section 4.1:
 *         y = g^x mod p
 *
 * @context
 *      When called as an asynchronous function it cannot sleep. It can be
 *      executed in a context that does not permit sleeping.
 *      When called as a synchronous function it may sleep. It MUST NOT be
 *      executed in a context that DOES NOT permit sleeping.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      Yes when configured to operate in synchronous mode.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]  instanceHandle   Instance handle.
 * @param[in]  pCb              Callback function pointer. If this is set to a
 *                              NULL value the function will operate
 *                              synchronously.
 * @param[in]  pCallbackTag     User-supplied value to help identify request.
 * @param[in]  pOpData          Structure containing all the data needed to
 *                              perform the operation. The client code
 *                              allocates the memory for this structure. This
 *                              component takes ownership of the memory until
 *                              it is returned in the callback.
 * @param[out] pProtocolStatus  The result passes/fails the DSA protocol
 *                              related checks.
 * @param[out] pY               y = g^x mod p*
 *                              On invocation the callback function will
 *                              contain this parameter in the pOut parameter.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_RETRY          Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE       Error related to system resources.
 * @retval CPA_STATUS_RESTARTING     API implementation is restarting. Resubmit
 *                                   the request.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      The component has been initialized via cpaCyStartInstance function.
 * @post
 *      None
 * @note
 *      When pCb is non-NULL an asynchronous callback of type
 *      CpaCyDsaYParamGenCbFunc is generated in response to this
 *      function call.
 *      For optimal performance, data pointers SHOULD be 8-byte aligned.
 *
 * @see
 *      CpaCyDsaYParamGenOpData,
 *      CpaCyDsaGenCbFunc
 *
 *****************************************************************************/
CpaStatus
cpaCyDsaGenYParam(const CpaInstanceHandle instanceHandle,
        const CpaCyDsaGenCbFunc pCb,
        void *pCallbackTag,
        const CpaCyDsaYParamGenOpData *pOpData,
        CpaBoolean *pProtocolStatus,
        CpaFlatBuffer *pY);

/**
 *****************************************************************************
 * @ingroup cpaCyDsa
 *      Generate DSA R Signature.
 *
 * @description
 *     This function generates the DSA R signature as described in FIPS 186-3
 *     Section 4.6:
 *         r = (g^k mod p) mod q
 *
 *     The protocol status, returned in the callback function as parameter
 *     protocolStatus (or, in the case of synchronous invocation, in the
 *     parameter *pProtocolStatus) is used to indicate whether the value r == 0.
 *
 *     Specifically, (protocolStatus == CPA_TRUE) means r != 0, while
 *     (protocolStatus == CPA_FALSE) means r == 0.
 *
 *     Generation of signature r does not depend on the content of the message
 *     being signed, so this operation can be done in advance for different
 *     values of k.  Then once each message becomes available only the
 *     signature s needs to be generated.
 *
 * @context
 *      When called as an asynchronous function it cannot sleep. It can be
 *      executed in a context that does not permit sleeping.
 *      When called as a synchronous function it may sleep. It MUST NOT be
 *      executed in a context that DOES NOT permit sleeping.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      Yes when configured to operate in synchronous mode.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]  instanceHandle   Instance handle.
 * @param[in]  pCb              Callback function pointer. If this is set to a
 *                              NULL value the function will operate
 *                              synchronously.
 * @param[in]  pCallbackTag     User-supplied value to help identify request.
 * @param[in]  pOpData          Structure containing all the data needed to
 *                              perform the operation. The client code
 *                              allocates the memory for this structure. This
 *                              component takes ownership of the memory until
 *                              it is returned in the callback.
 * @param[out] pProtocolStatus  The result passes/fails the DSA protocol
 *                              related checks.
 * @param[out] pR               DSA message signature r.
 *                              On invocation the callback function will
 *                              contain this parameter in the pOut parameter.
 *
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_RETRY          Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE       Error related to system resources.
 * @retval CPA_STATUS_RESTARTING     API implementation is restarting. Resubmit
 *                                   the request.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      The component has been initialized via cpaCyStartInstance function.
 * @post
 *      None
 * @note
 *      When pCb is non-NULL an asynchronous callback of type
 *      CpaCyDsaRSignCbFunc is generated in response to this function
 *      call.
 *      For optimal performance, data pointers SHOULD be 8-byte aligned.
 *
 * @see
 *      CpaCyDsaRSignOpData,
 *      CpaCyDsaGenCbFunc,
 *      cpaCyDsaSignS(),
 *      cpaCyDsaSignRS()
 *
 *****************************************************************************/
CpaStatus
cpaCyDsaSignR(const CpaInstanceHandle instanceHandle,
        const CpaCyDsaGenCbFunc pCb,
        void *pCallbackTag,
        const CpaCyDsaRSignOpData *pOpData,
        CpaBoolean *pProtocolStatus,
        CpaFlatBuffer *pR);

/**
 *****************************************************************************
 * @ingroup cpaCyDsa
 *      Generate DSA S Signature.
 *
 * @description
 *     This function generates the DSA S signature as described in FIPS 186-3
 *     Section 4.6:
 *          s = (k^-1(z + xr)) mod q
 *
 *     Here, z = the leftmost min(N, outlen) bits of Hash(M).  This function
 *     does not perform the SHA digest; z is computed by the caller and
 *     passed as a parameter in the pOpData field.
 *
 *     The protocol status, returned in the callback function as parameter
 *     protocolStatus (or, in the case of synchronous invocation, in the
 *     parameter *pProtocolStatus) is used to indicate whether the value s == 0.
 *
 *     Specifically, (protocolStatus == CPA_TRUE) means s != 0, while
 *     (protocolStatus == CPA_FALSE) means s == 0.
 *
 *     If signature r has been generated in advance, then this function can be
 *     used to generate the signature s once the message becomes available.
 *
 * @context
 *      When called as an asynchronous function it cannot sleep. It can be
 *      executed in a context that does not permit sleeping.
 *      When called as a synchronous function it may sleep. It MUST NOT be
 *      executed in a context that DOES NOT permit sleeping.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      Yes when configured to operate in synchronous mode.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]  instanceHandle   Instance handle.
 * @param[in]  pCb              Callback function pointer. If this is set to a
 *                              NULL value the function will operate
 *                              synchronously.
 * @param[in]  pCallbackTag     User-supplied value to help identify request.
 * @param[in]  pOpData          Structure containing all the data needed to
 *                              perform the operation. The client code
 *                              allocates the memory for this structure. This
 *                              component takes ownership of the memory until
 *                              it is returned in the callback.
 * @param[out] pProtocolStatus  The result passes/fails the DSA protocol
 *                              related checks.
 * @param[out] pS               DSA message signature s.
 *                              On invocation the callback function will
 *                              contain this parameter in the pOut parameter.
 *
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_RETRY          Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE       Error related to system resources.
 * @retval CPA_STATUS_RESTARTING     API implementation is restarting. Resubmit
 *                                   the request.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      The component has been initialized via cpaCyStartInstance function.
 * @post
 *      None
 * @note
 *      When pCb is non-NULL an asynchronous callback of type
 *      CpaCyDsaSSignCbFunc is generated in response to this function
 *      call.
 *      For optimal performance, data pointers SHOULD be 8-byte aligned.
 *
 * @see
 *      CpaCyDsaSSignOpData,
 *      CpaCyDsaGenCbFunc,
 *      cpaCyDsaSignR(),
 *      cpaCyDsaSignRS()
 *
 *****************************************************************************/
CpaStatus
cpaCyDsaSignS(const CpaInstanceHandle instanceHandle,
        const CpaCyDsaGenCbFunc pCb,
        void *pCallbackTag,
        const CpaCyDsaSSignOpData *pOpData,
        CpaBoolean *pProtocolStatus,
        CpaFlatBuffer *pS);

/**
 *****************************************************************************
 * @ingroup cpaCyDsa
 *      Generate DSA R and S Signatures.
 *
 * @description
 *     This function generates the DSA R and S signatures as described in
 *     FIPS 186-3 Section 4.6:
 *
 *         r = (g^k mod p) mod q
 *         s = (k^-1(z + xr)) mod q
 *
 *     Here, z = the leftmost min(N, outlen) bits of Hash(M).  This function
 *     does not perform the SHA digest; z is computed by the caller and
 *     passed as a parameter in the pOpData field.
 *
 *     The protocol status, returned in the callback function as parameter
 *     protocolStatus (or, in the case of synchronous invocation, in the
 *     parameter *pProtocolStatus) is used to indicate whether either of
 *     the values r or s are zero.
 *
 *     Specifically, (protocolStatus == CPA_TRUE) means neither is zero (i.e.
 *     (r != 0) && (s != 0)), while (protocolStatus == CPA_FALSE) means that at
 *     least one of r or s is zero (i.e. (r == 0) || (s == 0)).
 *
 * @context
 *      When called as an asynchronous function it cannot sleep. It can be
 *      executed in a context that does not permit sleeping.
 *      When called as a synchronous function it may sleep. It MUST NOT be
 *      executed in a context that DOES NOT permit sleeping.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      Yes when configured to operate in synchronous mode.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]  instanceHandle    Instance handle.
 * @param[in]  pCb               Callback function pointer. If this is  set to
 *                               a NULL value the function will operate
 *                               synchronously.
 * @param[in]  pCallbackTag      User-supplied value to help identify request.
 * @param[in]  pOpData           Structure containing all the data needed to
 *                               perform the operation. The client code
 *                               allocates the memory for this structure. This
 *                               component takes ownership of the memory until
 *                               it is returned in the callback.
 * @param[out] pProtocolStatus   The result passes/fails the DSA protocol
 *                               related checks.
 * @param[out] pR                DSA message signature r.
 * @param[out] pS                DSA message signature s.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_RETRY          Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE       Error related to system resources.
 * @retval CPA_STATUS_RESTARTING     API implementation is restarting. Resubmit
 *                                   the request.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      The component has been initialized via cpaCyStartInstance function.
 * @post
 *      None
 * @note
 *      When pCb is non-NULL an asynchronous callback of type
 *      CpaCyDsaRSSignCbFunc is generated in response to this function
 *      call.
 *      For optimal performance, data pointers SHOULD be 8-byte aligned.
 *
 * @see
 *      CpaCyDsaRSSignOpData,
 *      CpaCyDsaRSSignCbFunc,
 *      cpaCyDsaSignR(),
 *      cpaCyDsaSignS()
 *
 *****************************************************************************/
CpaStatus
cpaCyDsaSignRS(const CpaInstanceHandle instanceHandle,
        const CpaCyDsaRSSignCbFunc pCb,
        void *pCallbackTag,
        const CpaCyDsaRSSignOpData *pOpData,
        CpaBoolean *pProtocolStatus,
        CpaFlatBuffer *pR,
        CpaFlatBuffer *pS);

/**
 *****************************************************************************
 * @ingroup cpaCyDsa
 *      Verify DSA R and S signatures.
 *
 * @description
 *     This function performs FIPS 186-3 Section 4.7:
 *         w = (s')^-1 mod q
 *         u1 = (zw) mod q
 *         u2 = ((r')w) mod q
 *         v = (((g)^u1 (y)^u2) mod p) mod q
 *
 *     Here, z = the leftmost min(N, outlen) bits of Hash(M').  This function
 *     does not perform the SHA digest; z is computed by the caller and
 *     passed as a parameter in the pOpData field.
 *
 *     A response status of ok (verifyStatus == CPA_TRUE) means v = r'.
 *     A response status of not ok (verifyStatus == CPA_FALSE) means v != r'.
 *
 * @context
 *      When called as an asynchronous function it cannot sleep. It can be
 *      executed in a context that does not permit sleeping.
 *      When called as a synchronous function it may sleep. It MUST NOT be
 *      executed in a context that DOES NOT permit sleeping.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      Yes when configured to operate in synchronous mode.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]  instanceHandle  Instance handle.
 * @param[in]  pCb             Callback function pointer. If this is set to
 *                             a NULL value the function will operate
 *                             synchronously.
 * @param[in]  pCallbackTag    User-supplied value to help identify request.
 * @param[in]  pOpData         Structure containing all the data needed to
 *                             perform the operation. The client code
 *                             allocates the memory for this structure. This
 *                             component takes ownership of the memory until
 *                             it is returned in the callback.
 * @param[out] pVerifyStatus   The verification passed or failed.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_RETRY          Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE       Error related to system resources.
 * @retval CPA_STATUS_RESTARTING     API implementation is restarting. Resubmit
 *                                   the request.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      The component has been initialized via cpaCyStartInstance function.
 * @post
 *      None
 * @note
 *      When pCb is non-NULL an asynchronous callback of type
 *      CpaCyDsaVerifyCbFunc is generated in response to this function
 *      call.
 *      For optimal performance, data pointers SHOULD be 8-byte aligned.
 *
 * @see
 *      CpaCyDsaVerifyOpData,
 *      CpaCyDsaVerifyCbFunc
 *
 *****************************************************************************/
CpaStatus
cpaCyDsaVerify(const CpaInstanceHandle instanceHandle,
        const CpaCyDsaVerifyCbFunc pCb,
        void *pCallbackTag,
        const CpaCyDsaVerifyOpData *pOpData,
        CpaBoolean *pVerifyStatus);

/**
 *****************************************************************************
 * @ingroup cpaCyDsa
 *      Query statistics for a specific DSA instance.
 *
 * @deprecated
 *      As of v1.3 of the Crypto API, this function has been deprecated,
 *      replaced by @ref cpaCyDsaQueryStats64().
 *
 * @description
 *      This function will query a specific instance of the DSA implementation
 *      for statistics. The user MUST allocate the CpaCyDsaStats structure
 *      and pass the reference to that structure into this function call. This
 *      function writes the statistic results into the passed in
 *      CpaCyDsaStats structure.
 *
 *      Note: statistics returned by this function do not interrupt current data
 *      processing and as such can be slightly out of sync with operations that
 *      are in progress during the statistics retrieval process.
 *
 * @context
 *      This is a synchronous function and it can sleep. It MUST NOT be
 *      executed in a context that DOES NOT permit sleeping.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      This function is synchronous and blocking.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]  instanceHandle        Instance handle.
 * @param[out] pDsaStats             Pointer to memory into which the statistics
 *                                   will be written.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE       Error related to system resources.
 * @retval CPA_STATUS_RESTARTING     API implementation is restarting. Resubmit
 *                                   the request.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      Component has been initialized.
 * @post
 *      None
 * @note
 *      This function operates in a synchronous manner and no asynchronous
 *      callback will be generated.
 * @see
 *      CpaCyDsaStats
 *****************************************************************************/
CpaStatus CPA_DEPRECATED
cpaCyDsaQueryStats(const CpaInstanceHandle instanceHandle,
        struct _CpaCyDsaStats *pDsaStats);

/**
 *****************************************************************************
 * @ingroup cpaCyDsa
 *      Query 64-bit statistics for a specific DSA instance.
 *
 * @description
 *      This function will query a specific instance of the DSA implementation
 *      for 64-bit statistics. The user MUST allocate the CpaCyDsaStats64
 *      structure and pass the reference to that structure into this function.
 *      This function writes the statistic results into the passed in
 *      CpaCyDsaStats64 structure.
 *
 *      Note: statistics returned by this function do not interrupt current data
 *      processing and as such can be slightly out of sync with operations that
 *      are in progress during the statistics retrieval process.
 *
 * @context
 *      This is a synchronous function and it can sleep. It MUST NOT be
 *      executed in a context that DOES NOT permit sleeping.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      This function is synchronous and blocking.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]  instanceHandle        Instance handle.
 * @param[out] pDsaStats             Pointer to memory into which the statistics
 *                                   will be written.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE       Error related to system resources.
 * @retval CPA_STATUS_RESTARTING     API implementation is restarting. Resubmit
 *                                   the request.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      Component has been initialized.
 * @post
 *      None
 * @note
 *      This function operates in a synchronous manner and no asynchronous
 *      callback will be generated.
 * @see
 *      CpaCyDsaStats
 *****************************************************************************/
CpaStatus
cpaCyDsaQueryStats64(const CpaInstanceHandle instanceHandle,
        CpaCyDsaStats64 *pDsaStats);

/*****************************************************************************/

#ifdef __cplusplus
} /* close the extern "C" { */
#endif

#endif /* CPA_CY_DSA_H */
