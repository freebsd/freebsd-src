/***************************************************************************
 *
 *   BSD LICENSE
 *
 *   Copyright(c) 2007-2023 Intel Corporation. All rights reserved.
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
 * @file cpa_cy_kpt.h
 *
 * @defgroup cpaCyKpt Intel(R) Key Protection Technology (KPT) Cryptographic API
 *
 * @ingroup cpaCy
 *
 * @description
 *     These functions specify the APIs for Key Protection Technology (KPT)
 *     Cryptographic services.
 *
 * @note
 *     These functions implement the KPT Cryptographic API.
 *     This API is experimental and subject to change.
 *
 *****************************************************************************/

#ifndef __CPA_CY_KPT_H__
#define __CPA_CY_KPT_H__

#ifdef __cplusplus
extern "C" {
#endif
#include "cpa_cy_common.h"
#include "cpa_cy_rsa.h"
#include "cpa_cy_ecdsa.h"
#include "cpa_cy_ec.h"

/**
 *****************************************************************************
 * @ingroup cpaCyKpt
 *      KPT wrapping key handle
 *
 * @description
 *      Handle to a unique wrapping key in wrapping key table. Application
 *      creates it in KPT key transfer phase and maintains it for KPT Crypto
 *      service. For each KPT Crypto service API invocation, this handle will
 *      be used to get a SWK(Symmetric Wrapping Key) to unwrap
 *      WPK(Wrapped Private Key) before performing the requested crypto
 *      service.
 *
 *****************************************************************************/
typedef Cpa64U CpaCyKptHandle;

/**
 *****************************************************************************
 * @ingroup cpaCyKpt
 *      Return Status
 * @description
 *      This enumeration lists all the possible return status after completing
 *      KPT APIs.
 *
 *****************************************************************************/
typedef enum CpaCyKptKeyManagementStatus_t
{
    CPA_CY_KPT_SUCCESS = 0,
    /**< Generic success status for all KPT wrapping key handling functions*/
    CPA_CY_KPT_LOADKEY_FAIL_QUOTA_EXCEEDED_PER_VFID,
    /**< SWK count exceeds the configured maxmium value per VFID*/
    CPA_CY_KPT_LOADKEY_FAIL_QUOTA_EXCEEDED_PER_PASID,
    /**< SWK count exceeds the configured maxmium value per PASID*/
    CPA_CY_KPT_LOADKEY_FAIL_QUOTA_EXCEEDED,
    /**< SWK count exceeds the configured maxmium value when not scoped to
    * VFID or PASID*/
    CPA_CY_KPT_SWK_FAIL_NOT_FOUND,
    /**< Unable to find SWK entry by handle */
    CPA_CY_KPT_FAILED,
} CpaCyKptKeyManagementStatus;

/**
 *****************************************************************************
 * @ingroup cpaCyKpt
 *      PKCS#1 v2.2 RSA-3K signature output length in bytes.
 * @see  CpaCyKptValidationKey
 *
 *****************************************************************************/
#define CPA_CY_RSA3K_SIG_SIZE_INBYTES 384

/**
 *****************************************************************************
 * @ingroup cpaCyKpt
 *      KPT device credentials key certificate
 * @description
 *      This structure defines the key format for use with KPT.
 * @see
 *      cpaCyKptQueryDeviceCredentials
 *
 *****************************************************************************/
typedef struct CpaCyKptValidationKey_t
{
    CpaCyRsaPublicKey publicKey;
        /**< Key */
    Cpa8U signature[CPA_CY_RSA3K_SIG_SIZE_INBYTES];
        /**< Signature of key */
} CpaCyKptValidationKey;

/**
 *****************************************************************************
 * @ingroup cpaCyKpt
 *      Cipher algorithms used to generate a wrapped private key (WPK) from
 *      the clear private key.
 *
 * @description
 *      This enumeration lists supported cipher algorithms and modes.
 *
 *****************************************************************************/
typedef enum CpaCyKptWrappingKeyType_t
{
    CPA_CY_KPT_WRAPPING_KEY_TYPE_AES256_GCM = 0
} CpaCyKptWrappingKeyType;

/**
 *****************************************************************************
 * @ingroup cpaCyKpt
 *      KPT Loading key format specification.
 * @description
 *      This structure defines the format of the symmetric wrapping key to be
 *      loaded into KPT. Application sets these parameters through the
 *      cpaCyKptLoadKey calls.
 *
 *****************************************************************************/
typedef struct CpaCyKptLoadKey_t
{
    CpaFlatBuffer eSWK;
    /**< Encrypted SWK */
    CpaCyKptWrappingKeyType wrappingAlgorithm;
    /**< Symmetric wrapping algorithm */
} CpaCyKptLoadKey;

/**
 *****************************************************************************
 * @ingroup cpaCyKpt
 *      Max length of initialization vector
 * @description
 *      Defines the permitted max iv length in bytes that may be used in
 *      private key wrapping/unwrapping.For AEC-GCM, iv length is 12 bytes.
 *
 *@see  cpaCyKptUnwrapContext
 *
 *****************************************************************************/
#define CPA_CY_KPT_MAX_IV_LENGTH  (12)

/**
 *****************************************************************************
 * @ingroup cpaCyKpt
 *      Max length of Additional Authenticated Data
 * @description
 *      Defines the permitted max aad length in bytes that may be used in
 *      private key wrapping/unwrapping.
 *
 *@see  cpaCyKptUnwrapContext
 *
 *****************************************************************************/
#define CPA_CY_KPT_MAX_AAD_LENGTH  (16)

/**
 *****************************************************************************
 * @file cpa_cy_kpt.h
 * @ingroup cpaCyKpt
 *      Structure of KPT unwrapping context.
 * @description
 *      This structure is a parameter of KPT crypto APIs, it contains data
 *      relating to KPT WPK unwrapping, the application needs to fill in this
 *      information.
 *
 *****************************************************************************/
typedef struct CpaCyKptUnwrapContext_t
{
    CpaCyKptHandle kptHandle;
    /**< This is application's unique handle that identifies its
     * (symmetric) wrapping key*/
    Cpa8U iv[CPA_CY_KPT_MAX_IV_LENGTH];
    /**< Initialization Vector */
    Cpa8U additionalAuthData[CPA_CY_KPT_MAX_AAD_LENGTH];
    /**< A buffer holding the Additional Authenticated Data.*/
    Cpa32U aadLenInBytes;
    /**< Number of bytes representing the size of AAD within additionalAuthData
     *   buffer. */
} CpaCyKptUnwrapContext;

/**
 *****************************************************************************
 * @file cpa_cy_kpt.h
 * @ingroup cpaCyKpt
 *      RSA Private Key Structure For Representation 1.
 * @description
 *      This structure contains the first representation that can be used for
 *      describing the RSA private key, represented by the tuple of the
 *      modulus (N) and the private exponent (D).
 *      The representation is encrypted as follows:
 *      Encrypt - AES-256-GCM (Key, AAD, Input)
 *      "||" - denotes concatenation
 *      Key = SWK
 *      AAD = DER(OID)
 *      Input = (D || N)
 *      Encrypt (SWK, AAD, (D || N))
 *      Output (AuthTag, (D || N)')
 *      EncryptedRSAKey = (D || N)'
 *
 *      privateKey = (EncryptedRSAKey || AuthTag)
 *
 *      OID's that shall be supported by KPT implementation:
 *            OID                  DER(OID)
 *            1.2.840.113549.1.1   06 08 2A 86 48 86 F7 0D 01 01
 *
 *      Permitted lengths for N and D are:
 *      - 512 bits (64 bytes),
 *      - 1024 bits (128 bytes),
 *      - 1536 bits (192 bytes),
 *      - 2048 bits (256 bytes),
 *      - 3072 bits (384 bytes),
 *      - 4096 bits (512 bytes), or
 *      - 8192 bits (1024 bytes).
 *
 *      AuthTag is 128 bits (16 bytes)
 *
 * @note It is important that the value D is big enough. It is STRONGLY
 *      recommended that this value is at least half the length of the modulus
 *      N to protect against the Wiener attack.
 *
 *****************************************************************************/
typedef struct CpaCyKptRsaPrivateKeyRep1_t
{
    CpaFlatBuffer privateKey;
    /**< The EncryptedRSAKey concatenated with AuthTag */
} CpaCyKptRsaPrivateKeyRep1;

/**
 *****************************************************************************
 * @file cpa_cy_kpt.h
 * @ingroup cpaCyKpt
 *      KPT RSA Private Key Structure For Representation 2.
 * @description
 *      This structure contains the second representation that can be used for
 *      describing the RSA private key. The quintuple of p, q, dP, dQ, and qInv
 *      (explained below and in the spec) are required for the second
 *      representation. For KPT the parameters are Encrypted
 *      with the assoicated SWK as follows:
 *      Encrypt - AES-256-GCM (Key, AAD, Input)
 *      "||" - denotes concatenation
 *      Key = SWK
 *      AAD = DER(OID)
 *      Input = (P || Q || dP || dQ || Qinv || publicExponentE)
 *      Expanded Description:
 *         Encrypt (SWK, AAD,
 *                     (P || Q || dP || dQ || Qinv || publicExponentE))
 *         EncryptedRSAKey = (P || Q || dP || dQ || Qinv || publicExponentE)'
 *      Output (AuthTag, EncryptedRSAKey)
 *
 *      privateKey = EncryptedRSAKey || AuthTag
 *
 *      OID's that shall be supported by KPT implementation:
 *            OID                  DER(OID)
 *            1.2.840.113549.1.1   06 08 2A 86 48 86 F7 0D 01 01
 *
 *      All of the encrypted parameters will be of equal size. The length of
 *      each will be equal to keySize in bytes/2.
 *      For example for a key size of 256 Bytes (2048 bits), the length of
 *      P, Q, dP, dQ, and Qinv are all 128 Bytes, plus the
 *      publicExponentE of 256 Bytes, giving a total size for
 *      EncryptedRSAKey of 896 Bytes.
 *
 *      AuthTag is 128 bits (16 bytes)
 *
 *      Permitted Key Sizes are:
 *      - 512 bits (64 bytes),
 *      - 1024 bits (128 bytes),
 *      - 1536 bits (192 bytes),
 *      - 2048 bits (256 bytes),
 *      - 3072 bits (384 bytes),
 *      - 4096 bits (512 bytes), or
 *      - 8192 bits (1024 bytes).
 *
 *****************************************************************************/
typedef struct CpaCyKptRsaPrivateKeyRep2_t
{
    CpaFlatBuffer privateKey;
    /**< RSA private key representation 2 is built up from the
     *   tuple of p, q, dP, dQ, qInv, publicExponentE and AuthTag.
     */
} CpaCyKptRsaPrivateKeyRep2;

/**
 *****************************************************************************
 * @file cpa_cy_kpt.h
 * @ingroup cpaCyKpt
 *      RSA Private Key Structure.
 * @description
 *      This structure contains the two representations that can be used for
 *      describing the RSA private key. The privateKeyRepType will be used to
 *      identify which representation is to be used. Typically, using the
 *      second representation results in faster decryption operations.
 *
 *****************************************************************************/
typedef struct CpaCyKptRsaPrivateKey_t
{
    CpaCyRsaVersion version;
    /**< Indicates the version of the PKCS #1 specification that is
     * supported.
     * Note that this applies to both representations. */
    CpaCyRsaPrivateKeyRepType privateKeyRepType;
    /**< This value is used to identify which of the private key
     * representation types in this structure is relevant.
     * When performing key generation operations for Type 2 representations,
     * memory must also be allocated for the type 1 representations, and values
     * for both will be returned. */
    CpaCyKptRsaPrivateKeyRep1 privateKeyRep1;
    /**< This is the first representation of the RSA private key as
     * defined in the PKCS #1 V2.2 specification. */
    CpaCyKptRsaPrivateKeyRep2 privateKeyRep2;
    /**< This is the second representation of the RSA private key as
     * defined in the PKCS #1 V2.2 specification. */
} CpaCyKptRsaPrivateKey;

/**
 *****************************************************************************
 * @file cpa_cy_kpt.h
 * @ingroup cpaCyKpt
 *      KPT RSA Decryption Primitive Operation Data
 * @description
 *      This structure lists the different items that are required in the
 *      cpaCyKptRsaDecrypt function. As the RSA decryption primitive and
 *      signature primitive operations are mathematically identical this
 *      structure may also be used to perform an RSA signature primitive
 *      operation.
 *      When performing an RSA decryption primitive operation, the input data
 *      is the cipher text and the output data is the message text.
 *      When performing an RSA signature primitive operation, the input data
 *      is the message and the output data is the signature.
 *      The client MUST allocate the memory for this structure. When the
 *      structure is passed into the function, ownership of the memory passes
 *      to he function. Ownership of the memory returns to the client when
 *      this structure is returned in the CpaCyGenFlatBufCbFunc
 *      callback function.
 *
 * @note
 *      If the client modifies or frees the memory referenced in this structure
 *      after it has been submitted to the cpaCyKptRsaDecrypt function, and
 *      before it has been returned in the callback, undefined behavior will
 *      result.
 *      All values in this structure are required to be in Most Significant Byte
 *      first order, e.g. inputData.pData[0] = MSB.
 *
 *****************************************************************************/
typedef struct CpaCyKptRsaDecryptOpData_t
{
    CpaCyKptRsaPrivateKey *pRecipientPrivateKey;
    /**< Pointer to the recipient's RSA private key. */
    CpaFlatBuffer inputData;
    /**< The input data that the RSA decryption primitive operation is
     * performed on. The data pointed to is an integer that MUST be in big-
     * endian order. The value MUST be between 0 and the modulus n - 1. */
} CpaCyKptRsaDecryptOpData;


/**
 *****************************************************************************
 * @file cpa_cy_kpt.h
 * @ingroup cpaCyKpt
 *      KPT ECDSA Sign R & S Operation Data.
 *
 * @description
 *      This structure contains the operation data for the cpaCyKptEcdsaSignRS
 *      function. The client MUST allocate the memory for this structure and the
 *      items pointed to by this structure. When the structure is passed into
 *      the function, ownership of the memory passes to the function. Ownership
 *      of the memory returns to the client when this structure is returned in
 *      the callback function.
 *      This key structure is encrypted when passed into cpaCyKptEcdsaSignRS
 *      Encrypt - AES-256-GCM (Key, AAD, Input)
 *      "||" - denotes concatenation
 *
 *      Key = SWK
 *      AAD = DER(OID)
 *      Input = (d)
 *      Encrypt (SWK, AAD, (d))
 *      Output (AuthTag, EncryptedECKey)
 *
 *      privatekey == EncryptedECKey || AuthTag
 *
 *      OID's that shall be supported by KPT implementation:
 *          Curve      OID                  DER(OID)
 *          secp256r1  1.2.840.10045.3.1.7  06 08 2A 86 48 CE 3D 03 01 07
 *          secp384r1  1.3.132.0.34         06 05 2B 81 04 00 22
 *          secp521r1  1.3.132.0.35         06 05 2B 81 04 00 23
 *
 *      Expected private key (d) sizes:
 *          secp256r1   256 bits
 *          secp384r1   384 bits
 *          secp521r1   576 bits (rounded up to a multiple of 64-bit quadword)
 *
 *      AuthTag is 128 bits (16 bytes)
 *
 *      For optimal performance all data buffers SHOULD be 8-byte aligned.
 *
 * @note
 *      If the client modifies or frees the memory referenced in this
 *      structure after it has been submitted to the cpaCyKptEcdsaSignRS
 *      function, and before it has been returned in the callback, undefined
 *      behavior will result.
 *
 * @see
 *      cpaCyEcdsaSignRS()
 *
 *****************************************************************************/
typedef struct CpaCyKptEcdsaSignRSOpData_t
{
    CpaFlatBuffer privateKey;
    /**< Encrypted private key data of the form
     * EncryptECKey || AuthTag */
    CpaFlatBuffer m;
    /**< digest of the message to be signed */
} CpaCyKptEcdsaSignRSOpData;

/**
 *****************************************************************************
 * Discovery and Provisioning APIs for KPT
 *
 *****************************************************************************/

 /**
  *****************************************************************************
  * @file cpa_cy_kpt.h
  * @ingroup cpaCyKpt
  *      Query KPT's issuing public key(R_Pu) and signature from QAT driver.
  * @description
  *      This function is to query the RSA3K issuing key and its
  *      PKCS#1 v2.2 SHA-384 signature from the QAT driver.
  * @context
  *      This function may sleep, and  MUST NOT be called in interrupt context.
  * @assumptions
  *      None
  * @sideEffects
  *      None
  * @blocking
  *      This function is synchronous and blocking.
  * @param[in]  instanceHandle       Instance handle.
  * @param[out] pIssueCert           KPT-2.0 Issuing certificate in PEM format
                                     as defined in RFC#7468
  * @param[out] pKptStatus           One of the status codes denoted in the
  *                                  enumerate type CpaCyKptKeyManagementStatus
  *              CPA_CY_KPT_SUCCESS  Issuing key retrieved successfully
  *              CPA_CY_KPT_FAILED   Operation failed
  *
  * @retval CPA_STATUS_SUCCESS       Function executed successfully.
  * @retval CPA_STATUS_INVALID_PARAM Invalid parameter passed in.
  * @retval CPA_STATUS_FAIL          Function failed. Suggested course of action
  *                                  is to shutdown and restart.
  * @retval CPA_STATUS_UNSUPPORTED   Function is not supported.
  * @retval CPA_STATUS_RESTARTING    API implementation is restarting.
  *                                  Resubmit the request.
  *
  * @pre
  *      The component has been initialized via cpaCyStartInstance function.
  * @post
  *      None
  * @note
  *      Note that this is a synchronous function and has no completion callback
  *      associated with it.
  * @see
  *
  *****************************************************************************/
 CpaStatus
 cpaCyKptQueryIssuingKeys(const CpaInstanceHandle instanceHandle,
        CpaFlatBuffer *pPublicX509IssueCert,
        CpaCyKptKeyManagementStatus *pKptStatus);

 /**
  *****************************************************************************
  * @file cpa_cy_kpt.h
  * @ingroup cpaCyKpt
  *      Query KPT's Per-Part public key(I_pu) and signature from QAT
  *      device
  * @description
  *      This function is to query RSA3K Per-Part public key and its
  *      PKCS#1 v2.2 SHA-384 signature from the QAT device.
  * @context
  *      This function may sleep, and  MUST NOT be called in interrupt context.
  * @assumptions
  *      None
  * @sideEffects
  *      None
  * @blocking
  *      This function is synchronous and blocking.
  * @param[in]  instanceHandle       Instance handle.
  * @param[out] pDevCredential       Device Per-Part public key
  * @param[out] pKptStatus           One of the status codes denoted in the
  *                                  enumerate type CpaCyKptKeyManagementStatus
  *              CPA_CY_KPT_SUCCESS  Device credentials retrieved successfully
  *              CPA_CY_KPT_FAILED   Operation failed
  *
  * @retval CPA_STATUS_SUCCESS       Function executed successfully.
  * @retval CPA_STATUS_INVALID_PARAM Invalid parameter passed in.
  * @retval CPA_STATUS_FAIL          Function failed. Suggested course of action
  *                                  is to shutdown and restart.
  * @retval CPA_STATUS_UNSUPPORTED   Function is not supported.
  * @retval CPA_STATUS_RESTARTING    API implementation is restarting.
  *                                  Resubmit the request.
  *
  * @pre
  *      The component has been initialized via cpaCyStartInstance function.
  * @post
  *      None
  * @note
  *      Note that this is a synchronous function and has no completion callback
  *      associated with it.
  * @see
  *
  *****************************************************************************/
 CpaStatus
 cpaCyKptQueryDeviceCredentials(const CpaInstanceHandle instanceHandle,
        CpaCyKptValidationKey *pDevCredential,
        CpaCyKptKeyManagementStatus *pKptStatus);

 /**
  *****************************************************************************
  * @file cpa_cy_kpt.h
  * @ingroup cpaCyKpt
  *      Perform KPT key loading function.
  *
  * @description
  *      This function is invoked by a QAT application to load an encrypted
  *      symmetric wrapping key.
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
  * @param[in]  instanceHandle      QAT service instance handle.
  * @param[in]  pSWK                Encrypted SWK
  * @param[out] keyHandle           A 64-bit handle value created by KPT
  * @param[out] pKptStatus          One of the status codes denoted in the
  *                                 enumerate type CpaCyKptKeyManagementStatus
  *   CPA_CY_KPT_SUCCESS  Key Loaded successfully
  *   CPA_CY_KPT_LOADKEY_FAIL_QUOTA_EXCEEDED_PER_VFID
  *       SWK count exceeds the configured maxmium value per VFID
  *   CPA_CY_KPT_LOADKEY_FAIL_QUOTA_EXCEEDED_PER_PASID
  *       SWK count exceeds the configured maxmium value per PASID
  *   CPA_CY_KPT_LOADKEY_FAIL_QUOTA_EXCEEDED
  *       SWK count exceeds the configured maxmium value when not scoped to
  *       VFID or PASID
  *   CPA_CY_KPT_FAILED   Operation failed due to unspecified reason
  *
  * @retval CPA_STATUS_SUCCESS       Function executed successfully.
  * @retval CPA_STATUS_FAIL          Function failed.
  * @retval CPA_STATUS_INVALID_PARAM Invalid parameter passed in.
  * @retval CPA_STATUS_RESOURCE      Error related to system resources.
  * @retval CPA_STATUS_RESTARTING    API implementation is restarting.
  *                                  Resubmit the request.
  * @retval CPA_STATUS_UNSUPPORTED   KPT-2.0 is not supported.
  *
  * @pre
  *      Component has been initialized.
  * @post
  *      None
  * @note
  *      None
  * @see
  *      None
  *****************************************************************************/
 CpaStatus
 cpaCyKptLoadKey(CpaInstanceHandle instanceHandle,
    CpaCyKptLoadKey *pSWK,
    CpaCyKptHandle *keyHandle,
    CpaCyKptKeyManagementStatus *pKptStatus);

/**
 *****************************************************************************
 * @file cpa_cy_kpt.h
 * @ingroup cpaCyKpt
 *      Perform KPT delete keys function according to key handle
 *
 * @description
 *      Before closing a QAT session(instance), an application that has
 *      previously stored its wrapping key in a QAT device using the KPT
 *      framework executes this call to delete its wrapping key in the QAT
 *      device.
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
 * @param[in]  instanceHandle       QAT service instance handle.
 * @param[in]  keyHandle            A 64-bit handle value
 * @param[out] pkptstatus           One of the status codes denoted in the
 *                                  enumerate type CpaCyKptKeyManagementStatus
 *   CPA_CY_KPT_SUCCESS  Key Deleted successfully
 *   CPA_CY_KPT_SWK_FAIL_NOT_FOUND For any reason the input handle cannot be
 *        found.
 *   CPA_CY_KPT_FAILED   Operation failed due to unspecified reason
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE       Error related to system resources.
 * @retval CPA_STATUS_RESTARTING     API implementation is restarting.
 *                                   Resubmit the request.
 * @pre
 *      Component has been initialized.
 * @post
 *      None
 * @note
 *      None
 * @see
 *      None
 *****************************************************************************/
CpaStatus
cpaCyKptDeleteKey(CpaInstanceHandle instanceHandle,
        CpaCyKptHandle keyHandle,
        CpaCyKptKeyManagementStatus *pKptStatus);

/**
*****************************************************************************
* Usage APIs for KPT
*
*****************************************************************************/

/**
 *****************************************************************************
 * @file cpa_cy_kpt.h
 * @ingroup cpaCyKpt
 *      Perform KPT-2.0 mode RSA decrypt primitive operation on the input data.
 *
 * @description
 *      This function is a variant of cpaCyRsaDecrypt, which will perform
 *      an RSA decryption primitive operation on the input data using the
 *      specified RSA private key which are encrypted. As the RSA decryption
 *      primitive and signing primitive operations are mathematically
 *      identical this function may also be used to perform an RSA signing
 *      primitive operation.
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
 * @param[in]  pRsaDecryptCb      Pointer to callback function to be invoked
 *                                when the operation is complete. If this is
 *                                set to a NULL value the function will operate
 *                                synchronously.
 * @param[in]  pCallbackTag       Opaque User Data for this specific call.
 *                                Will be returned unchanged in the callback.
 * @param[in]  pDecryptOpData     Structure containing all the data needed to
 *                                perform the RSA decrypt operation. The
 *                                client code allocates the memory for this
 *                                structure. This component takes ownership
 *                                of the memory until it is returned in the
 *                                callback.
 * @param[out] pOutputData        Pointer to structure into which the result of
 *                                the RSA decryption primitive is written. The
 *                                client MUST allocate this memory. The data
 *                                pointed to is an integer in big-endian order.
 *                                The value will be between 0 and the modulus
 *                                n - 1.
 *                                On invocation the callback function will
 *                                contain this parameter in the pOut parameter.
 * @param[in]  pKptUnwrapContext  Pointer of structure into which the content
 *                                of KptUnwrapContext is kept. The client MUST
 *                                allocate this memory and copy structure
 *                                KptUnwrapContext into this flat buffer.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_RETRY          Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE       Error related to system resources.
 * @retval CPA_STATUS_RESTARTING     API implementation is restarting.Resubmit
 *                                   the request.
 * @pre
 *      The component has been initialized via cpaCyStartInstance function.
 * @post
 *      None
 * @note
 *      By virtue of invoking cpaSyKptRsaDecrypt, the implementation understands
 *      that pDecryptOpData contains an encrypted private key that requires
 *      unwrapping. KptUnwrapContext contains a 'KptHandle' field that points
 *      to the unwrapping key in the WKT.
 *      When pRsaDecryptCb is non-NULL an asynchronous callback is generated in
 *      response to this function call.
 *      Any errors generated during processing are reported as part of the
 *      callback status code. For optimal performance, data pointers SHOULD be
 *      8-byte aligned.
 *      In KPT release, private key field in CpaCyKptRsaDecryptOpData is a
 *      concatenation of cipher text and hash tag.
 *      For optimal performance, data pointers SHOULD be 8-byte aligned.
 * @see
 *      CpaCyKptRsaDecryptOpData,
 *      CpaCyGenFlatBufCbFunc,
 *
 *****************************************************************************/
CpaStatus
cpaCyKptRsaDecrypt(const CpaInstanceHandle instanceHandle,
        const CpaCyGenFlatBufCbFunc pRsaDecryptCb,
        void *pCallbackTag,
        const CpaCyKptRsaDecryptOpData *pDecryptOpData,
        CpaFlatBuffer *pOutputData,
        CpaCyKptUnwrapContext *pKptUnwrapContext);

/**
 *****************************************************************************
 * @ingroup cpaCyKpt
 *      Generate ECDSA Signature R & S.
 * @description
 *      This function is a variant of cpaCyEcdsaSignRS, it generates ECDSA
 *      signature R & S as per ANSI X9.62 2005 section 7.3.
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
 * @param[in]  pCb                Callback function pointer. If this is set to
 *                                a NULL value the function will operate
 *                                synchronously.
 * @param[in]  pCallbackTag       User-supplied value to help identify request.
 * @param[in]  pOpData            Structure containing all the data needed to
 *                                perform the operation. The client code
 *                                allocates the memory for this structure. This
 *                                component takes ownership of the memory until
 *                                it is returned in the callback.
 * @param[out] pSignStatus        In synchronous mode, the multiply output is
 *                                valid (CPA_TRUE) or the output is invalid
 *                                (CPA_FALSE).
 * @param[out] pR                 ECDSA message signature r.
 * @param[out] pS                 ECDSA message signature s.
 * @param[in]  pKptUnwrapContext  Pointer of structure into which the content
 *                                of KptUnwrapContext is kept,The client MUST
 *                                allocate this memory and copy structure
 *                                KptUnwrapContext into this flat buffer.
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
 *      The component has been initialized via cpaCyStartInstance function.
 * @post
 *      None
 * @note
 *      By virtue of invoking the cpaCyKptEcdsaSignRS, the implementation
 *      understands CpaCyEcdsaSignRSOpData contains an encrypted private key that
 *      requires unwrapping. KptUnwrapContext contains a 'KptHandle' field
 *      that points to the unwrapping key in the WKT.
 *      When pCb is non-NULL an asynchronous callback of type
 *      CpaCyEcdsaSignRSCbFunc generated in response to this function
 *      call.
 *      In KPT release, private key field in CpaCyEcdsaSignRSOpData is a
 *      concatenation of cipher text and hash tag.
 * @see
 *      None
 *****************************************************************************/
CpaStatus
cpaCyKptEcdsaSignRS(const CpaInstanceHandle instanceHandle,
        const CpaCyEcdsaSignRSCbFunc pCb,
        void *pCallbackTag,
        const CpaCyKptEcdsaSignRSOpData *pOpData,
        CpaBoolean *pSignStatus,
        CpaFlatBuffer *pR,
        CpaFlatBuffer *pS,
        CpaCyKptUnwrapContext *pKptUnwrapContext);

#ifdef __cplusplus
} /* close the extern "C" { */
#endif
#endif
