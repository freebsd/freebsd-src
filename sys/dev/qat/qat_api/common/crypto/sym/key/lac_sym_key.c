/***************************************************************************
 *
 * <COPYRIGHT_TAG>
 *
 ***************************************************************************/

/**
 *****************************************************************************
 * @file lac_sym_key.c
 *
 * @ingroup LacSymKey
 *
 * This file contains the implementation of all keygen functionality
 *
 *****************************************************************************/

/*
*******************************************************************************
* Include public/global header files
*******************************************************************************
*/
#include "cpa.h"
#include "cpa_cy_key.h"
#include "cpa_cy_im.h"

/*
*******************************************************************************
* Include private header files
*******************************************************************************
*/
#include "icp_accel_devices.h"
#include "icp_adf_debug.h"
#include "icp_adf_init.h"
#include "icp_adf_transport.h"

#include "qat_utils.h"

#include "lac_log.h"
#include "lac_hooks.h"
#include "lac_sym.h"
#include "lac_sym_qat_hash_defs_lookup.h"
#include "lac_sym_qat.h"
#include "lac_sal.h"
#include "lac_sym_key.h"
#include "lac_sal_types_crypto.h"
#include "sal_service_state.h"
#include "lac_sym_qat_key.h"
#include "lac_sym_hash_defs.h"
#include "sal_statistics.h"

/* Number of statistics */
#define LAC_KEY_NUM_STATS (sizeof(CpaCyKeyGenStats64) / sizeof(Cpa64U))

#define LAC_KEY_STAT_INC(statistic, instanceHandle)                            \
	do {                                                                   \
		sal_crypto_service_t *pService = NULL;                         \
		pService = (sal_crypto_service_t *)instanceHandle;             \
		if (CPA_TRUE ==                                                \
		    pService->generic_service_info.stats                       \
			->bKeyGenStatsEnabled) {                               \
			qatUtilsAtomicInc(                                     \
			    &pService                                          \
				 ->pLacKeyStats[offsetof(CpaCyKeyGenStats64,   \
							 statistic) /          \
						sizeof(Cpa64U)]);              \
		}                                                              \
	} while (0)
/**< Macro to increment a Key stat (derives offset into array of atomics) */

#define LAC_KEY_STATS32_GET(keyStats, instanceHandle)                          \
	do {                                                                   \
		int i;                                                         \
		sal_crypto_service_t *pService =                               \
		    (sal_crypto_service_t *)instanceHandle;                    \
		for (i = 0; i < LAC_KEY_NUM_STATS; i++) {                      \
			((Cpa32U *)&(keyStats))[i] =                           \
			    (Cpa32U)qatUtilsAtomicGet(                         \
				&pService->pLacKeyStats[i]);                   \
		}                                                              \
	} while (0)
/**< Macro to get all 32bit Key stats (from internal array of atomics) */

#define LAC_KEY_STATS64_GET(keyStats, instanceHandle)                          \
	do {                                                                   \
		int i;                                                         \
		sal_crypto_service_t *pService =                               \
		    (sal_crypto_service_t *)instanceHandle;                    \
		for (i = 0; i < LAC_KEY_NUM_STATS; i++) {                      \
			((Cpa64U *)&(keyStats))[i] =                           \
			    qatUtilsAtomicGet(&pService->pLacKeyStats[i]);     \
		}                                                              \
	} while (0)
/**< Macro to get all 64bit Key stats (from internal array of atomics) */

#define IS_HKDF_UNSUPPORTED(cmdId, hkdfSupported)                              \
	((ICP_QAT_FW_LA_CMD_HKDF_EXTRACT <= cmdId &&                           \
	  ICP_QAT_FW_LA_CMD_HKDF_EXTRACT_AND_EXPAND_LABEL >= cmdId) &&         \
	 !hkdfSupported) /**< macro to check whether the HKDF algorithm can be \
			    supported on the device */

/* Sublabel for HKDF TLS Key Generation, as defined in RFC8446. */
const static Cpa8U key256[HKDF_SUB_LABEL_KEY_LENGTH] = { 0,   16,  9,   't',
							 'l', 's', '1', '3',
							 ' ', 'k', 'e', 'y',
							 0 };
const static Cpa8U key384[HKDF_SUB_LABEL_KEY_LENGTH] = { 0,   32,  9,   't',
							 'l', 's', '1', '3',
							 ' ', 'k', 'e', 'y',
							 0 };
const static Cpa8U keyChaChaPoly[HKDF_SUB_LABEL_KEY_LENGTH] = { 0,   32,  9,
								't', 'l', 's',
								'1', '3', ' ',
								'k', 'e', 'y',
								0 };
/* Sublabel for HKDF TLS IV key Generation, as defined in RFC8446. */
const static Cpa8U iv256[HKDF_SUB_LABEL_IV_LENGTH] = { 0,   12,  8,   't',
						       'l', 's', '1', '3',
						       ' ', 'i', 'v', 0 };
const static Cpa8U iv384[HKDF_SUB_LABEL_IV_LENGTH] = { 0,   12,  8,   't',
						       'l', 's', '1', '3',
						       ' ', 'i', 'v', 0 };
/* Sublabel for HKDF TLS RESUMPTION key Generation, as defined in RFC8446. */
const static Cpa8U resumption256[HKDF_SUB_LABEL_RESUMPTION_LENGTH] =
    { 0,   32,  16,  't', 'l', 's', '1', '3', ' ', 'r',
      'e', 's', 'u', 'm', 'p', 't', 'i', 'o', 'n', 0 };
const static Cpa8U resumption384[HKDF_SUB_LABEL_RESUMPTION_LENGTH] =
    { 0,   48,  16,  't', 'l', 's', '1', '3', ' ', 'r',
      'e', 's', 'u', 'm', 'p', 't', 'i', 'o', 'n', 0 };
/* Sublabel for HKDF TLS FINISHED key Generation, as defined in RFC8446. */
const static Cpa8U finished256[HKDF_SUB_LABEL_FINISHED_LENGTH] =
    { 0,   32,  14,  't', 'l', 's', '1', '3', ' ',
      'f', 'i', 'n', 'i', 's', 'h', 'e', 'd', 0 };
const static Cpa8U finished384[HKDF_SUB_LABEL_FINISHED_LENGTH] =
    { 0,   48,  14,  't', 'l', 's', '1', '3', ' ',
      'f', 'i', 'n', 'i', 's', 'h', 'e', 'd', 0 };

/**
 ******************************************************************************
 * @ingroup LacSymKey
 *      SSL/TLS stat type
 *
 * @description
 *      This enum determines which stat should be incremented
 *****************************************************************************/
typedef enum {
	LAC_KEY_REQUESTS = 0,
	/**< Key requests sent */
	LAC_KEY_REQUEST_ERRORS,
	/**< Key requests errors */
	LAC_KEY_COMPLETED,
	/**< Key requests which received responses */
	LAC_KEY_COMPLETED_ERRORS
	/**< Key requests which received responses with errors */
} lac_key_stat_type_t;

/*** Local functions prototypes ***/
static void
LacSymKey_MgfHandleResponse(icp_qat_fw_la_cmd_id_t lacCmdId,
			    void *pOpaqueData,
			    icp_qat_fw_comn_flags cmnRespFlags);

static CpaStatus
LacSymKey_MgfSync(const CpaInstanceHandle instanceHandle,
		  const CpaCyGenFlatBufCbFunc pKeyGenCb,
		  void *pCallbackTag,
		  const void *pKeyGenMgfOpData,
		  CpaFlatBuffer *pGeneratedMaskBuffer,
		  CpaBoolean bIsExtRequest);

static void
LacSymKey_SslTlsHandleResponse(icp_qat_fw_la_cmd_id_t lacCmdId,
			       void *pOpaqueData,
			       icp_qat_fw_comn_flags cmnRespFlags);

static CpaStatus
LacSymKey_SslTlsSync(CpaInstanceHandle instanceHandle,
		     const CpaCyGenFlatBufCbFunc pKeyGenCb,
		     void *pCallbackTag,
		     icp_qat_fw_la_cmd_id_t lacCmdId,
		     void *pKeyGenSslTlsOpData,
		     Cpa8U hashAlgorithm,
		     CpaFlatBuffer *pKeyGenOutpuData);

/*** Implementation ***/

/**
 ******************************************************************************
 * @ingroup LacSymKey
 *      Get the instance handle. Support single handle.
 * @param[in] instanceHandle_in        user supplied handle.
 * @retval    CpaInstanceHandle        the instance handle
 */
static CpaInstanceHandle
LacKey_GetHandle(CpaInstanceHandle instanceHandle_in)
{
	CpaInstanceHandle instanceHandle = NULL;
	if (CPA_INSTANCE_HANDLE_SINGLE == instanceHandle_in) {
		instanceHandle =
		    Lac_GetFirstHandle(SAL_SERVICE_TYPE_CRYPTO_SYM);
	} else {
		instanceHandle = instanceHandle_in;
	}
	return instanceHandle;
}

/**
*******************************************************************************
* @ingroup LacSymKey
*      Perform SSL/TLS key gen operation
*
* @description
*      Perform SSL/TLS key gen operation
*
* @param[in] instanceHandle        QAT device handle.
* @param[in] pKeyGenCb             Pointer to callback function to be invoked
*                                  when the operation is complete.
* @param[in] pCallbackTag          Opaque User Data for this specific call.
* @param[in] lacCmdId              Lac command ID (identify SSL & TLS ops)
* @param[in] pKeyGenSslTlsOpData   Structure containing all the data needed to
*                                  perform the SSL/TLS key generation
*                                  operation.
* @param[in]  hashAlgorithm        Specifies the hash algorithm to use.
*                                  According to RFC5246, this should be
*                                  "SHA-256 or a stronger standard hash
*                                  function."
* @param[out] pKeyGenOutputData    pointer to where output result should be
*                                  written
*
* @retval CPA_STATUS_SUCCESS       Function executed successfully.
* @retval CPA_STATUS_FAIL           Function failed.
* @retval CPA_STATUS_RETRY          Function should be retried.
* @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
* @retval CPA_STATUS_RESOURCE       Error related to system resources.
*
*****************************************************************************/
static CpaStatus
LacSymKey_KeyGenSslTls_GenCommon(CpaInstanceHandle instanceHandle,
				 const CpaCyGenFlatBufCbFunc pKeyGenCb,
				 void *pCallbackTag,
				 icp_qat_fw_la_cmd_id_t lacCmdId,
				 void *pKeyGenSslTlsOpData,
				 Cpa8U hashAlgorithm,
				 CpaFlatBuffer *pKeyGenOutputData);

/**
 ******************************************************************************
 * @ingroup LacSymKey
 *      Increment stat for TLS or SSL operation
 *
 * @description
 *      This is a generic function to update the stats for either a TLS or SSL
 *      operation.
 *
 * @param[in] lacCmdId          Indicate SSL or TLS operations
 * @param[in] statType          Statistics Type
 * @param[in] instanceHandle    Instance Handle
 *
 * @return None
 *
 *****************************************************************************/
static void
LacKey_StatsInc(icp_qat_fw_la_cmd_id_t lacCmdId,
		lac_key_stat_type_t statType,
		CpaInstanceHandle instanceHandle)
{
	if (ICP_QAT_FW_LA_CMD_SSL3_KEY_DERIVE == lacCmdId) {
		switch (statType) {
		case LAC_KEY_REQUESTS:
			LAC_KEY_STAT_INC(numSslKeyGenRequests, instanceHandle);
			break;
		case LAC_KEY_REQUEST_ERRORS:
			LAC_KEY_STAT_INC(numSslKeyGenRequestErrors,
					 instanceHandle);
			break;
		case LAC_KEY_COMPLETED:
			LAC_KEY_STAT_INC(numSslKeyGenCompleted, instanceHandle);
			break;
		case LAC_KEY_COMPLETED_ERRORS:
			LAC_KEY_STAT_INC(numSslKeyGenCompletedErrors,
					 instanceHandle);
			break;
		default:
			QAT_UTILS_LOG("Invalid statistics type\n");
			break;
		}
	} else /* TLS v1.0/1.1 and 1.2 */
	{
		switch (statType) {
		case LAC_KEY_REQUESTS:
			LAC_KEY_STAT_INC(numTlsKeyGenRequests, instanceHandle);
			break;
		case LAC_KEY_REQUEST_ERRORS:
			LAC_KEY_STAT_INC(numTlsKeyGenRequestErrors,
					 instanceHandle);
			break;
		case LAC_KEY_COMPLETED:
			LAC_KEY_STAT_INC(numTlsKeyGenCompleted, instanceHandle);
			break;
		case LAC_KEY_COMPLETED_ERRORS:
			LAC_KEY_STAT_INC(numTlsKeyGenCompletedErrors,
					 instanceHandle);
			break;
		default:
			QAT_UTILS_LOG("Invalid statistics type\n");
			break;
		}
	}
}

void
LacKeygen_StatsShow(CpaInstanceHandle instanceHandle)
{
	CpaCyKeyGenStats64 keyStats = { 0 };

	LAC_KEY_STATS64_GET(keyStats, instanceHandle);

	QAT_UTILS_LOG(SEPARATOR BORDER
		      "                  Key Stats:                " BORDER
		      "\n" SEPARATOR);

	QAT_UTILS_LOG(BORDER " SSL Key Requests:               %16llu " BORDER
			     "\n" BORDER
			     " SSL Key Request Errors:         %16llu " BORDER
			     "\n" BORDER
			     " SSL Key Completed               %16llu " BORDER
			     "\n" BORDER
			     " SSL Key Complete Errors:        %16llu " BORDER
			     "\n" SEPARATOR,
		      (unsigned long long)keyStats.numSslKeyGenRequests,
		      (unsigned long long)keyStats.numSslKeyGenRequestErrors,
		      (unsigned long long)keyStats.numSslKeyGenCompleted,
		      (unsigned long long)keyStats.numSslKeyGenCompletedErrors);

	QAT_UTILS_LOG(BORDER " TLS Key Requests:               %16llu " BORDER
			     "\n" BORDER
			     " TLS Key Request Errors:         %16llu " BORDER
			     "\n" BORDER
			     " TLS Key Completed               %16llu " BORDER
			     "\n" BORDER
			     " TLS Key Complete Errors:        %16llu " BORDER
			     "\n" SEPARATOR,
		      (unsigned long long)keyStats.numTlsKeyGenRequests,
		      (unsigned long long)keyStats.numTlsKeyGenRequestErrors,
		      (unsigned long long)keyStats.numTlsKeyGenCompleted,
		      (unsigned long long)keyStats.numTlsKeyGenCompletedErrors);

	QAT_UTILS_LOG(BORDER " MGF Key Requests:               %16llu " BORDER
			     "\n" BORDER
			     " MGF Key Request Errors:         %16llu " BORDER
			     "\n" BORDER
			     " MGF Key Completed               %16llu " BORDER
			     "\n" BORDER
			     " MGF Key Complete Errors:        %16llu " BORDER
			     "\n" SEPARATOR,
		      (unsigned long long)keyStats.numMgfKeyGenRequests,
		      (unsigned long long)keyStats.numMgfKeyGenRequestErrors,
		      (unsigned long long)keyStats.numMgfKeyGenCompleted,
		      (unsigned long long)keyStats.numMgfKeyGenCompletedErrors);
}

/** @ingroup LacSymKey */
CpaStatus
cpaCyKeyGenQueryStats(CpaInstanceHandle instanceHandle_in,
		      struct _CpaCyKeyGenStats *pSymKeyStats)
{
	CpaInstanceHandle instanceHandle = NULL;


	if (CPA_INSTANCE_HANDLE_SINGLE == instanceHandle_in) {
		instanceHandle =
		    Lac_GetFirstHandle(SAL_SERVICE_TYPE_CRYPTO_SYM);
	} else {
		instanceHandle = instanceHandle_in;
	}

	LAC_CHECK_INSTANCE_HANDLE(instanceHandle);
	SAL_CHECK_INSTANCE_TYPE(instanceHandle,
				(SAL_SERVICE_TYPE_CRYPTO |
				 SAL_SERVICE_TYPE_CRYPTO_SYM));
	LAC_CHECK_NULL_PARAM(pSymKeyStats);

	SAL_RUNNING_CHECK(instanceHandle);

	LAC_KEY_STATS32_GET(*pSymKeyStats, instanceHandle);

	return CPA_STATUS_SUCCESS;
}

/** @ingroup LacSymKey */
CpaStatus
cpaCyKeyGenQueryStats64(CpaInstanceHandle instanceHandle_in,
			CpaCyKeyGenStats64 *pSymKeyStats)
{
	CpaInstanceHandle instanceHandle = NULL;


	if (CPA_INSTANCE_HANDLE_SINGLE == instanceHandle_in) {
		instanceHandle =
		    Lac_GetFirstHandle(SAL_SERVICE_TYPE_CRYPTO_SYM);
	} else {
		instanceHandle = instanceHandle_in;
	}

	LAC_CHECK_INSTANCE_HANDLE(instanceHandle);
	SAL_CHECK_INSTANCE_TYPE(instanceHandle,
				(SAL_SERVICE_TYPE_CRYPTO |
				 SAL_SERVICE_TYPE_CRYPTO_SYM));
	LAC_CHECK_NULL_PARAM(pSymKeyStats);

	SAL_RUNNING_CHECK(instanceHandle);

	LAC_KEY_STATS64_GET(*pSymKeyStats, instanceHandle);

	return CPA_STATUS_SUCCESS;
}

/**
 ******************************************************************************
 * @ingroup LacSymKey
 *      Return the size of the digest for a specific hash algorithm.
 * @description
 *      Return the expected digest size based on the sha algorithm submitted.
 *      The only supported value are sha256, sha384 and sha512.
 *
 * @param[in]  hashAlgorithm        either sha256, sha384 or sha512.
 * @return the expected size or 0 for an invalid hash.
 *
 *****************************************************************************/
static Cpa32U
getDigestSizeFromHashAlgo(CpaCySymHashAlgorithm hashAlgorithm)
{
	switch (hashAlgorithm) {
	case CPA_CY_SYM_HASH_SHA256:
		return LAC_HASH_SHA256_DIGEST_SIZE;
	case CPA_CY_SYM_HASH_SHA384:
		return LAC_HASH_SHA384_DIGEST_SIZE;
	case CPA_CY_SYM_HASH_SHA512:
		return LAC_HASH_SHA512_DIGEST_SIZE;
	case CPA_CY_SYM_HASH_SM3:
		return LAC_HASH_SM3_DIGEST_SIZE;
	default:
		return 0;
	}
}

/**
 ******************************************************************************
 * @ingroup LacSymKey
 *      Return the hash algorithm for a specific cipher.
 * @description
 *      Return the hash algorithm related to the cipher suite.
 *      Supported hash's are SHA256, and SHA384.
 *
 * @param[in]  cipherSuite AES_128_GCM, AES_256_GCM, AES_128_CCM,
 *             and CHACHA20_POLY1305.
 * @return the expected hash algorithm or 0 for an invalid cipher.
 *
 *****************************************************************************/
static CpaCySymHashAlgorithm
getHashAlgorithmFromCipherSuiteHKDF(CpaCyKeyHKDFCipherSuite cipherSuite)
{
	switch (cipherSuite) {
	case CPA_CY_HKDF_TLS_AES_128_GCM_SHA256: /* Fall through */
	case CPA_CY_HKDF_TLS_CHACHA20_POLY1305_SHA256:
	case CPA_CY_HKDF_TLS_AES_128_CCM_SHA256:
	case CPA_CY_HKDF_TLS_AES_128_CCM_8_SHA256:
		return CPA_CY_SYM_HASH_SHA256;
	case CPA_CY_HKDF_TLS_AES_256_GCM_SHA384:
		return CPA_CY_SYM_HASH_SHA384;
	default:
		return 0;
	}
}

/**
 ******************************************************************************
 * @ingroup LacSymKey
 *      Return the digest size of cipher.
 * @description
 *      Return the output key size of specific cipher, for specified sub label
 *
 * @param[in]  cipherSuite = AES_128_GCM, AES_256_GCM, AES_128_CCM,
 *             and CHACHA20_POLY1305.
 *             subLabels = KEY, IV, RESUMPTION, and FINISHED.
 * @return the expected digest size of the cipher.
 *
 *****************************************************************************/
static const Cpa32U cipherSuiteHKDFHashSizes
    [LAC_KEY_HKDF_CIPHERS_MAX][LAC_KEY_HKDF_SUBLABELS_MAX] = {
	    {},			    /* Not used */
	    { 32, 16, 12, 32, 32 }, /* AES_128_GCM_SHA256 */
	    { 48, 32, 12, 48, 48 }, /* AES_256_GCM_SHA384 */
	    { 32, 32, 12, 32, 32 }, /* CHACHA20_POLY1305_SHA256 */
	    { 32, 16, 12, 32, 32 }, /* AES_128_CCM_SHA256 */
	    { 32, 16, 12, 32, 32 }  /* AES_128_CCM_8_SHA256 */
    };

/**
 ******************************************************************************
 * @ingroup LacSymKey
 *      Key Generation MGF response handler
 *
 * @description
 *      Handles Key Generation MGF response messages from the QAT.
 *
 * @param[in] lacCmdId       Command id of the original request
 * @param[in] pOpaqueData    Pointer to opaque data that was in request
 * @param[in] cmnRespFlags   Indicates whether request succeeded
 *
 * @return void
 *
 *****************************************************************************/
static void
LacSymKey_MgfHandleResponse(icp_qat_fw_la_cmd_id_t lacCmdId,
			    void *pOpaqueData,
			    icp_qat_fw_comn_flags cmnRespFlags)
{
	CpaCyKeyGenMgfOpData *pMgfOpData = NULL;
	lac_sym_key_cookie_t *pCookie = NULL;
	CpaCyGenFlatBufCbFunc pKeyGenMgfCb = NULL;
	void *pCallbackTag = NULL;
	CpaFlatBuffer *pGeneratedKeyBuffer = NULL;
	CpaStatus status = CPA_STATUS_SUCCESS;
	CpaBoolean respStatusOk =
	    (ICP_QAT_FW_COMN_STATUS_FLAG_OK ==
	     ICP_QAT_FW_COMN_RESP_CRYPTO_STAT_GET(cmnRespFlags)) ?
	    CPA_TRUE :
	    CPA_FALSE;

	pCookie = (lac_sym_key_cookie_t *)pOpaqueData;

	if (CPA_TRUE == respStatusOk) {
		status = CPA_STATUS_SUCCESS;
		LAC_KEY_STAT_INC(numMgfKeyGenCompleted,
				 pCookie->instanceHandle);
	} else {
		status = CPA_STATUS_FAIL;
		LAC_KEY_STAT_INC(numMgfKeyGenCompletedErrors,
				 pCookie->instanceHandle);
	}

	pKeyGenMgfCb = (CpaCyGenFlatBufCbFunc)(pCookie->pKeyGenCb);

	pMgfOpData = pCookie->pKeyGenOpData;
	pCallbackTag = pCookie->pCallbackTag;
	pGeneratedKeyBuffer = pCookie->pKeyGenOutputData;

	Lac_MemPoolEntryFree(pCookie);

	(*pKeyGenMgfCb)(pCallbackTag, status, pMgfOpData, pGeneratedKeyBuffer);
}

/**
 ******************************************************************************
 * @ingroup LacSymKey
 *      Synchronous mode of operation wrapper function
 *
 * @description
 *      Wrapper function to implement synchronous mode of operation for
 *      cpaCyKeyGenMgf and cpaCyKeyGenMgfExt function.
 *
 * @param[in] instanceHandle       Instance handle
 * @param[in] pKeyGenCb            Internal callback function pointer
 * @param[in] pCallbackTag         Callback tag
 * @param[in] pKeyGenMgfOpData     Pointer to user provided Op Data structure
 * @param[in] pGeneratedMaskBuffer Pointer to a buffer where generated mask
 *                                 will be stored
 * @param[in] bIsExtRequest        Indicates origin of function call;
 *                                 if CPA_TRUE then the call comes from
 *                                 cpaCyKeyGenMgfExt function, otherwise
 *                                 from cpaCyKeyGenMgf
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_RETRY          Function should be retried.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE       Error related to system resources.
 *
 *****************************************************************************/
static CpaStatus
LacSymKey_MgfSync(const CpaInstanceHandle instanceHandle,
		  const CpaCyGenFlatBufCbFunc pKeyGenCb,
		  void *pCallbackTag,
		  const void *pKeyGenMgfOpData,
		  CpaFlatBuffer *pGeneratedMaskBuffer,
		  CpaBoolean bIsExtRequest)
{
	CpaStatus status = CPA_STATUS_SUCCESS;

	lac_sync_op_data_t *pSyncCallbackData = NULL;

	status = LacSync_CreateSyncCookie(&pSyncCallbackData);

	if (CPA_STATUS_SUCCESS == status) {
		if (CPA_TRUE == bIsExtRequest) {
			status = cpaCyKeyGenMgfExt(
			    instanceHandle,
			    LacSync_GenFlatBufCb,
			    pSyncCallbackData,
			    (const CpaCyKeyGenMgfOpDataExt *)pKeyGenMgfOpData,
			    pGeneratedMaskBuffer);
		} else {
			status = cpaCyKeyGenMgf(instanceHandle,
						LacSync_GenFlatBufCb,
						pSyncCallbackData,
						(const CpaCyKeyGenMgfOpData *)
						    pKeyGenMgfOpData,
						pGeneratedMaskBuffer);
		}
	} else {
		/* Failure allocating sync cookie */
		LAC_KEY_STAT_INC(numMgfKeyGenRequestErrors, instanceHandle);
		return status;
	}

	if (CPA_STATUS_SUCCESS == status) {
		CpaStatus syncStatus = CPA_STATUS_SUCCESS;

		syncStatus =
		    LacSync_WaitForCallback(pSyncCallbackData,
					    LAC_SYM_SYNC_CALLBACK_TIMEOUT,
					    &status,
					    NULL);

		/* If callback doesn't come back */
		if (CPA_STATUS_SUCCESS != syncStatus) {
			LAC_KEY_STAT_INC(numMgfKeyGenCompletedErrors,
					 instanceHandle);
			LAC_LOG_ERROR("Callback timed out");
			status = syncStatus;
		}
	} else {
		/* As the Request was not sent the Callback will never
		 * be called, so need to indicate that we're finished
		 * with cookie so it can be destroyed.
		 */
		LacSync_SetSyncCookieComplete(pSyncCallbackData);
	}

	LacSync_DestroySyncCookie(&pSyncCallbackData);

	return status;
}

/**
 ******************************************************************************
 * @ingroup LacSymKey
 *      Perform MGF key gen operation
 *
 * @description
 *      This function performs MGF key gen operation. It is common for requests
 *      coming from both cpaCyKeyGenMgf and cpaCyKeyGenMgfExt QAT API
 *      functions.
 *
 * @param[in] instanceHandle       Instance handle
 * @param[in] pKeyGenCb            Pointer to callback function to be invoked
 *                                 when the operation is complete.
 * @param[in] pCallbackTag         Opaque User Data for this specific call.
 * @param[in] pOpData              Pointer to the Op Data structure provided by
 *                                 the user in API function call. For calls
 *                                 originating from cpaCyKeyGenMgfExt it will
 *                                 point to CpaCyKeyGenMgfOpDataExt type of
 *                                 structure while for calls originating from
 *                                 cpaCyKeyGenMgf it will point to
 *                                 CpaCyKeyGenMgfOpData type of structure.
 * @param[in] pKeyGenMgfOpData     Pointer to the user provided
 *                                 CpaCyKeyGenMgfOpData structure. For calls
 *                                 originating from cpaCyKeyGenMgf it will
 *                                 point to the same structure as pOpData
 *                                 parameter; for calls originating from
 *                                 cpaCyKeyGenMgfExt it will point to the
 *                                 baseOpData member of the
 *                                 CpaCyKeyGenMgfOpDataExt structure passed in
 *                                 as a parameter to the API function call.
 * @param[in] pGeneratedMaskBuffer Pointer to a buffer where generated mask
 *                                 will be stored
 * @param[in] hashAlgorithm        Indicates which hash algorithm is to be used
 *                                 to perform MGF key gen operation. For calls
 *                                 originating from cpaCyKeyGenMgf it will
 *                                 always be CPA_CY_SYM_HASH_SHA1.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_RETRY          Function should be retried.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE       Error related to system resources.
 *
 *****************************************************************************/
static CpaStatus
LacSymKey_MgfCommon(const CpaInstanceHandle instanceHandle,
		    const CpaCyGenFlatBufCbFunc pKeyGenCb,
		    void *pCallbackTag,
		    const void *pOpData,
		    const CpaCyKeyGenMgfOpData *pKeyGenMgfOpData,
		    CpaFlatBuffer *pGeneratedMaskBuffer,
		    CpaCySymHashAlgorithm hashAlgorithm)
{
	CpaStatus status = CPA_STATUS_SUCCESS;

	icp_qat_fw_la_bulk_req_t keyGenReq = { { 0 } };
	icp_qat_la_bulk_req_hdr_t keyGenReqHdr = { { 0 } };
	icp_qat_fw_la_key_gen_common_t keyGenReqMid = { { 0 } };
	icp_qat_la_bulk_req_ftr_t keyGenReqFtr = { { { 0 } } };
	Cpa8U *pMsgDummy = NULL;
	Cpa8U *pCacheDummyHdr = NULL;
	Cpa8U *pCacheDummyMid = NULL;
	Cpa8U *pCacheDummyFtr = NULL;
	sal_qat_content_desc_info_t contentDescInfo = { 0 };
	lac_sym_key_cookie_t *pCookie = NULL;
	lac_sym_cookie_t *pSymCookie = NULL;
	sal_crypto_service_t *pService = NULL;
	Cpa64U inputPhysAddr = 0;
	Cpa64U outputPhysAddr = 0;
/* Structure initializer is supported by C99, but it is
 * not supported by some former Intel compiler.
 */
	CpaCySymHashSetupData hashSetupData = { 0 };
	Cpa32U hashBlkSizeInBytes = 0;
	lac_sym_qat_hash_alg_info_t *pHashAlgInfo = NULL;
	icp_qat_fw_serv_specif_flags laCmdFlags = 0;
	icp_qat_fw_comn_flags cmnRequestFlags =
	    ICP_QAT_FW_COMN_FLAGS_BUILD(QAT_COMN_PTR_TYPE_FLAT,
					QAT_COMN_CD_FLD_TYPE_64BIT_ADR);

	pService = (sal_crypto_service_t *)instanceHandle;
	LAC_CHECK_INSTANCE_HANDLE(instanceHandle);
	SAL_CHECK_INSTANCE_TYPE(instanceHandle,
				(SAL_SERVICE_TYPE_CRYPTO |
				 SAL_SERVICE_TYPE_CRYPTO_SYM));

	SAL_RUNNING_CHECK(instanceHandle);
	LAC_CHECK_NULL_PARAM(pOpData);
	LAC_CHECK_NULL_PARAM(pKeyGenMgfOpData);
	LAC_CHECK_NULL_PARAM(pGeneratedMaskBuffer);
	LAC_CHECK_NULL_PARAM(pGeneratedMaskBuffer->pData);
	LAC_CHECK_NULL_PARAM(pKeyGenMgfOpData->seedBuffer.pData);

	/* Maximum seed length for MGF1 request */
	if (pKeyGenMgfOpData->seedBuffer.dataLenInBytes >
	    ICP_QAT_FW_LA_MGF_SEED_LEN_MAX) {
		LAC_INVALID_PARAM_LOG("seedBuffer.dataLenInBytes");
		return CPA_STATUS_INVALID_PARAM;
	}

	/* Maximum mask length for MGF1 request */
	if (pKeyGenMgfOpData->maskLenInBytes > ICP_QAT_FW_LA_MGF_MASK_LEN_MAX) {
		LAC_INVALID_PARAM_LOG("maskLenInBytes");
		return CPA_STATUS_INVALID_PARAM;
	}

	/* check for enough space in the flat buffer */
	if (pKeyGenMgfOpData->maskLenInBytes >
	    pGeneratedMaskBuffer->dataLenInBytes) {
		LAC_INVALID_PARAM_LOG("pGeneratedMaskBuffer.dataLenInBytes");
		return CPA_STATUS_INVALID_PARAM;
	}

	/* Get hash alg info */
	LacSymQat_HashAlgLookupGet(instanceHandle,
				   hashAlgorithm,
				   &pHashAlgInfo);

	/* Allocate the cookie */
	pCookie = (lac_sym_key_cookie_t *)Lac_MemPoolEntryAlloc(
	    pService->lac_sym_cookie_pool);
	if (NULL == pCookie) {
		LAC_LOG_ERROR("Cannot get mem pool entry");
		status = CPA_STATUS_RESOURCE;
	} else if ((void *)CPA_STATUS_RETRY == pCookie) {
		pCookie = NULL;
		status = CPA_STATUS_RETRY;
	} else {
		pSymCookie = (lac_sym_cookie_t *)pCookie;
	}

	if (CPA_STATUS_SUCCESS == status) {
		/* populate the cookie */
		pCookie->instanceHandle = instanceHandle;
		pCookie->pCallbackTag = pCallbackTag;
		pCookie->pKeyGenOpData = (void *)LAC_CONST_PTR_CAST(pOpData);
		pCookie->pKeyGenCb = pKeyGenCb;
		pCookie->pKeyGenOutputData = pGeneratedMaskBuffer;
		hashSetupData.hashAlgorithm = hashAlgorithm;
		hashSetupData.hashMode = CPA_CY_SYM_HASH_MODE_PLAIN;
		hashSetupData.digestResultLenInBytes =
		    pHashAlgInfo->digestLength;

		/* Populate the CD ctrl Block (LW 27 - LW 31)
		 * and the CD Hash HW setup block
		 */
		LacSymQat_HashContentDescInit(
		    &(keyGenReqFtr),
		    instanceHandle,
		    &hashSetupData,
		    /* point to base of hw setup block */
		    (Cpa8U *)pCookie->contentDesc,
		    LAC_SYM_KEY_NO_HASH_BLK_OFFSET_QW,
		    ICP_QAT_FW_SLICE_DRAM_WR,
		    ICP_QAT_HW_AUTH_MODE0, /* just a plain hash */
		    CPA_FALSE, /* Not using sym Constants Table in Shared SRAM
				*/
		    CPA_FALSE, /* not using the optimised Content Desc */
		    CPA_FALSE, /* Not using the stateful SHA3 Content Desc */
		    NULL,
		    &hashBlkSizeInBytes);

		/* Populate the Req param LW 14-26 */
		LacSymQat_KeyMgfRequestPopulate(
		    &keyGenReqHdr,
		    &keyGenReqMid,
		    pKeyGenMgfOpData->seedBuffer.dataLenInBytes,
		    pKeyGenMgfOpData->maskLenInBytes,
		    (Cpa8U)pHashAlgInfo->digestLength);

		contentDescInfo.pData = pCookie->contentDesc;
		contentDescInfo.hardwareSetupBlockPhys =
		    LAC_MEM_CAST_PTR_TO_UINT64(
			pSymCookie->keyContentDescPhyAddr);
		contentDescInfo.hwBlkSzQuadWords =
		    LAC_BYTES_TO_QUADWORDS(hashBlkSizeInBytes);

		/* Populate common request fields */
		inputPhysAddr =
		    LAC_MEM_CAST_PTR_TO_UINT64(LAC_OS_VIRT_TO_PHYS_EXTERNAL(
			pService->generic_service_info,
			pKeyGenMgfOpData->seedBuffer.pData));

		if (inputPhysAddr == 0) {
			LAC_LOG_ERROR(
			    "Unable to get the seed buffer physical address");
			status = CPA_STATUS_FAIL;
		}
		outputPhysAddr = LAC_MEM_CAST_PTR_TO_UINT64(
		    LAC_OS_VIRT_TO_PHYS_EXTERNAL(pService->generic_service_info,
						 pGeneratedMaskBuffer->pData));
		if (outputPhysAddr == 0) {
			LAC_LOG_ERROR(
			    "Unable to get the physical address of the mask");
			status = CPA_STATUS_FAIL;
		}
	}

	if (CPA_STATUS_SUCCESS == status) {
		/* Make up the full keyGenReq struct from its constituents */
		pMsgDummy = (Cpa8U *)&(keyGenReq);
		pCacheDummyHdr = (Cpa8U *)&(keyGenReqHdr);
		pCacheDummyMid = (Cpa8U *)&(keyGenReqMid);
		pCacheDummyFtr = (Cpa8U *)&(keyGenReqFtr);

		memcpy(pMsgDummy,
		       pCacheDummyHdr,
		       (LAC_LONG_WORD_IN_BYTES * LAC_SIZE_OF_CACHE_HDR_IN_LW));
		memset((pMsgDummy +
			(LAC_LONG_WORD_IN_BYTES * LAC_SIZE_OF_CACHE_HDR_IN_LW)),
		       0,
		       (LAC_LONG_WORD_IN_BYTES *
			LAC_SIZE_OF_CACHE_TO_CLEAR_IN_LW));
		memcpy(pMsgDummy + (LAC_LONG_WORD_IN_BYTES *
				    LAC_START_OF_CACHE_MID_IN_LW),
		       pCacheDummyMid,
		       (LAC_LONG_WORD_IN_BYTES * LAC_SIZE_OF_CACHE_MID_IN_LW));
		memcpy(pMsgDummy + (LAC_LONG_WORD_IN_BYTES *
				    LAC_START_OF_CACHE_FTR_IN_LW),
		       pCacheDummyFtr,
		       (LAC_LONG_WORD_IN_BYTES * LAC_SIZE_OF_CACHE_FTR_IN_LW));

		SalQatMsg_ContentDescHdrWrite((icp_qat_fw_comn_req_t *)&(
						  keyGenReq),
					      &(contentDescInfo));

		SalQatMsg_CmnHdrWrite((icp_qat_fw_comn_req_t *)&keyGenReq,
				      ICP_QAT_FW_COMN_REQ_CPM_FW_LA,
				      ICP_QAT_FW_LA_CMD_MGF1,
				      cmnRequestFlags,
				      laCmdFlags);

		/*
		 * MGF uses a flat buffer but we can use zero for source and
		 * dest length because the firmware will use the seed length,
		 * hash length and mask length to find source length.
		 */
		SalQatMsg_CmnMidWrite((icp_qat_fw_la_bulk_req_t *)&(keyGenReq),
				      pCookie,
				      LAC_SYM_KEY_QAT_PTR_TYPE,
				      inputPhysAddr,
				      outputPhysAddr,
				      0,
				      0);

		/* Send to QAT */
		status = icp_adf_transPutMsg(pService->trans_handle_sym_tx,
					     (void *)&(keyGenReq),
					     LAC_QAT_SYM_REQ_SZ_LW);
	}
	if (CPA_STATUS_SUCCESS == status) {
		/* Update stats */
		LAC_KEY_STAT_INC(numMgfKeyGenRequests, instanceHandle);
	} else {
		LAC_KEY_STAT_INC(numMgfKeyGenRequestErrors, instanceHandle);
		/* clean up memory */
		if (NULL != pCookie) {
			Lac_MemPoolEntryFree(pCookie);
		}
	}
	return status;
}

/**
 * cpaCyKeyGenMgf
 */
CpaStatus
cpaCyKeyGenMgf(const CpaInstanceHandle instanceHandle_in,
	       const CpaCyGenFlatBufCbFunc pKeyGenCb,
	       void *pCallbackTag,
	       const CpaCyKeyGenMgfOpData *pKeyGenMgfOpData,
	       CpaFlatBuffer *pGeneratedMaskBuffer)
{
	CpaInstanceHandle instanceHandle = NULL;


	if (CPA_INSTANCE_HANDLE_SINGLE == instanceHandle_in) {
		instanceHandle =
		    Lac_GetFirstHandle(SAL_SERVICE_TYPE_CRYPTO_SYM);
	} else {
		instanceHandle = instanceHandle_in;
	}

	/* If synchronous Operation */
	if (NULL == pKeyGenCb) {
		return LacSymKey_MgfSync(instanceHandle,
					 pKeyGenCb,
					 pCallbackTag,
					 (const void *)pKeyGenMgfOpData,
					 pGeneratedMaskBuffer,
					 CPA_FALSE);
	}
	/* Asynchronous Operation */
	return LacSymKey_MgfCommon(instanceHandle,
				   pKeyGenCb,
				   pCallbackTag,
				   (const void *)pKeyGenMgfOpData,
				   pKeyGenMgfOpData,
				   pGeneratedMaskBuffer,
				   CPA_CY_SYM_HASH_SHA1);
}

/**
 * cpaCyKeyGenMgfExt
 */
CpaStatus
cpaCyKeyGenMgfExt(const CpaInstanceHandle instanceHandle_in,
		  const CpaCyGenFlatBufCbFunc pKeyGenCb,
		  void *pCallbackTag,
		  const CpaCyKeyGenMgfOpDataExt *pKeyGenMgfOpDataExt,
		  CpaFlatBuffer *pGeneratedMaskBuffer)
{
	CpaInstanceHandle instanceHandle = NULL;


	if (CPA_INSTANCE_HANDLE_SINGLE == instanceHandle_in) {
		instanceHandle =
		    Lac_GetFirstHandle(SAL_SERVICE_TYPE_CRYPTO_SYM);
	} else {
		instanceHandle = instanceHandle_in;
	}

	/* If synchronous Operation */
	if (NULL == pKeyGenCb) {
		return LacSymKey_MgfSync(instanceHandle,
					 pKeyGenCb,
					 pCallbackTag,
					 (const void *)pKeyGenMgfOpDataExt,
					 pGeneratedMaskBuffer,
					 CPA_TRUE);
	}

	/* Param check specific for Ext function, rest of parameters validated
	 * in LacSymKey_MgfCommon
	 */
	LAC_CHECK_NULL_PARAM(pKeyGenMgfOpDataExt);
	if (CPA_CY_SYM_HASH_MD5 > pKeyGenMgfOpDataExt->hashAlgorithm ||
	    CPA_CY_SYM_HASH_SHA512 < pKeyGenMgfOpDataExt->hashAlgorithm) {
		LAC_INVALID_PARAM_LOG("hashAlgorithm");
		return CPA_STATUS_INVALID_PARAM;
	}

	/* Asynchronous Operation */
	return LacSymKey_MgfCommon(instanceHandle,
				   pKeyGenCb,
				   pCallbackTag,
				   (const void *)pKeyGenMgfOpDataExt,
				   &pKeyGenMgfOpDataExt->baseOpData,
				   pGeneratedMaskBuffer,
				   pKeyGenMgfOpDataExt->hashAlgorithm);
}

/**
 ******************************************************************************
 * @ingroup LacSymKey
 *      Key Generation SSL & TLS response handler
 *
 * @description
 *      Handles Key Generation SSL & TLS response messages from the QAT.
 *
 * @param[in] lacCmdId        Command id of the original request
 * @param[in] pOpaqueData     Pointer to opaque data that was in request
 * @param[in] cmnRespFlags    LA response flags
 *
 * @return void
 *
 *****************************************************************************/
static void
LacSymKey_SslTlsHandleResponse(icp_qat_fw_la_cmd_id_t lacCmdId,
			       void *pOpaqueData,
			       icp_qat_fw_comn_flags cmnRespFlags)
{
	void *pSslTlsOpData = NULL;
	CpaCyGenFlatBufCbFunc pKeyGenSslTlsCb = NULL;
	lac_sym_key_cookie_t *pCookie = NULL;
	void *pCallbackTag = NULL;
	CpaFlatBuffer *pGeneratedKeyBuffer = NULL;
	CpaStatus status = CPA_STATUS_SUCCESS;

	CpaBoolean respStatusOk =
	    (ICP_QAT_FW_COMN_STATUS_FLAG_OK ==
	     ICP_QAT_FW_COMN_RESP_CRYPTO_STAT_GET(cmnRespFlags)) ?
	    CPA_TRUE :
	    CPA_FALSE;

	pCookie = (lac_sym_key_cookie_t *)pOpaqueData;

	pSslTlsOpData = pCookie->pKeyGenOpData;

	if (CPA_TRUE == respStatusOk) {
		LacKey_StatsInc(lacCmdId,
				LAC_KEY_COMPLETED,
				pCookie->instanceHandle);
	} else {
		status = CPA_STATUS_FAIL;
		LacKey_StatsInc(lacCmdId,
				LAC_KEY_COMPLETED_ERRORS,
				pCookie->instanceHandle);
	}

	pKeyGenSslTlsCb = (CpaCyGenFlatBufCbFunc)(pCookie->pKeyGenCb);

	pCallbackTag = pCookie->pCallbackTag;
	pGeneratedKeyBuffer = pCookie->pKeyGenOutputData;

	Lac_MemPoolEntryFree(pCookie);

	(*pKeyGenSslTlsCb)(pCallbackTag,
			   status,
			   pSslTlsOpData,
			   pGeneratedKeyBuffer);
}

/**
*******************************************************************************
* @ingroup LacSymKey
*      Synchronous mode of operation function wrapper for performing SSL/TLS
*      key gen operation
*
* @description
*      Synchronous mode of operation function wrapper for performing SSL/TLS
*      key gen operation
*
* @param[in] instanceHandle        QAT device handle.
* @param[in] pKeyGenCb             Pointer to callback function to be invoked
*                                  when the operation is complete.
* @param[in] pCallbackTag          Opaque User Data for this specific call.
* @param[in] lacCmdId              Lac command ID (identify SSL & TLS ops)
* @param[in] pKeyGenSslTlsOpData   Structure containing all the data needed to
*                                  perform the SSL/TLS key generation
*                                  operation.
* @param[in]  hashAlgorithm        Specifies the hash algorithm to use.
*                                  According to RFC5246, this should be
*                                  "SHA-256 or a stronger standard hash
*                                  function."
* @param[out] pKeyGenOutputData    pointer to where output result should be
*                                  written
*
* @retval CPA_STATUS_SUCCESS        Function executed successfully.
* @retval CPA_STATUS_FAIL           Function failed.
* @retval CPA_STATUS_RETRY          Function should be retried.
* @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
* @retval CPA_STATUS_RESOURCE       Error related to system resources.
*
*****************************************************************************/
static CpaStatus
LacSymKey_SslTlsSync(CpaInstanceHandle instanceHandle,
		     const CpaCyGenFlatBufCbFunc pKeyGenCb,
		     void *pCallbackTag,
		     icp_qat_fw_la_cmd_id_t lacCmdId,
		     void *pKeyGenSslTlsOpData,
		     Cpa8U hashAlgorithm,
		     CpaFlatBuffer *pKeyGenOutpuData)
{
	lac_sync_op_data_t *pSyncCallbackData = NULL;
	CpaStatus status = CPA_STATUS_SUCCESS;

	status = LacSync_CreateSyncCookie(&pSyncCallbackData);
	if (CPA_STATUS_SUCCESS == status) {
		status = LacSymKey_KeyGenSslTls_GenCommon(instanceHandle,
							  pKeyGenCb,
							  pSyncCallbackData,
							  lacCmdId,
							  pKeyGenSslTlsOpData,
							  hashAlgorithm,
							  pKeyGenOutpuData);
	} else {
		/* Failure allocating sync cookie */
		LacKey_StatsInc(lacCmdId,
				LAC_KEY_REQUEST_ERRORS,
				instanceHandle);
		return status;
	}

	if (CPA_STATUS_SUCCESS == status) {
		CpaStatus syncStatus = CPA_STATUS_SUCCESS;

		syncStatus =
		    LacSync_WaitForCallback(pSyncCallbackData,
					    LAC_SYM_SYNC_CALLBACK_TIMEOUT,
					    &status,
					    NULL);

		/* If callback doesn't come back */
		if (CPA_STATUS_SUCCESS != syncStatus) {
			LacKey_StatsInc(lacCmdId,
					LAC_KEY_COMPLETED_ERRORS,
					instanceHandle);
			LAC_LOG_ERROR("Callback timed out");
			status = syncStatus;
		}
	} else {
		/* As the Request was not sent the Callback will never
		 * be called, so need to indicate that we're finished
		 * with cookie so it can be destroyed.
		 */
		LacSync_SetSyncCookieComplete(pSyncCallbackData);
	}

	LacSync_DestroySyncCookie(&pSyncCallbackData);

	return status;
}

static CpaStatus
computeHashKey(CpaFlatBuffer *secret,
	       CpaFlatBuffer *hash,
	       CpaCySymHashAlgorithm *hashAlgorithm)
{
	CpaStatus status = CPA_STATUS_SUCCESS;

	switch (*hashAlgorithm) {
	case CPA_CY_SYM_HASH_MD5:
		status = qatUtilsHashMD5Full(secret->pData,
					     hash->pData,
					     secret->dataLenInBytes);
		break;
	case CPA_CY_SYM_HASH_SHA1:
		status = qatUtilsHashSHA1Full(secret->pData,
					      hash->pData,
					      secret->dataLenInBytes);
		break;
	case CPA_CY_SYM_HASH_SHA256:
		status = qatUtilsHashSHA256Full(secret->pData,
						hash->pData,
						secret->dataLenInBytes);
		break;
	case CPA_CY_SYM_HASH_SHA384:
		status = qatUtilsHashSHA384Full(secret->pData,
						hash->pData,
						secret->dataLenInBytes);
		break;
	case CPA_CY_SYM_HASH_SHA512:
		status = qatUtilsHashSHA512Full(secret->pData,
						hash->pData,
						secret->dataLenInBytes);
		break;
	default:
		status = CPA_STATUS_FAIL;
	}
	return status;
}

static CpaStatus
LacSymKey_KeyGenSslTls_GenCommon(CpaInstanceHandle instanceHandle,
				 const CpaCyGenFlatBufCbFunc pKeyGenCb,
				 void *pCallbackTag,
				 icp_qat_fw_la_cmd_id_t lacCmdId,
				 void *pKeyGenSslTlsOpData,
				 Cpa8U hashAlgCipher,
				 CpaFlatBuffer *pKeyGenOutputData)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	CpaBoolean precompute = CPA_FALSE;
	icp_qat_fw_la_bulk_req_t keyGenReq = { { 0 } };
	icp_qat_la_bulk_req_hdr_t keyGenReqHdr = { { 0 } };
	icp_qat_fw_la_key_gen_common_t keyGenReqMid = { { 0 } };
	icp_qat_la_bulk_req_ftr_t keyGenReqFtr = { { { 0 } } };
	Cpa8U *pMsgDummy = NULL;
	Cpa8U *pCacheDummyHdr = NULL;
	Cpa8U *pCacheDummyMid = NULL;
	Cpa8U *pCacheDummyFtr = NULL;
	lac_sym_key_cookie_t *pCookie = NULL;
	lac_sym_cookie_t *pSymCookie = NULL;
	Cpa64U inputPhysAddr = 0;
	Cpa64U outputPhysAddr = 0;
/* Structure initializer is supported by C99, but it is
 * not supported by some former Intel compiler.
 */
	CpaCySymHashSetupData hashSetupData = { 0 };
	sal_qat_content_desc_info_t contentDescInfo = { 0 };
	Cpa32U hashBlkSizeInBytes = 0;
	Cpa32U tlsPrefixLen = 0;

	CpaFlatBuffer inputSecret = { 0 };
	CpaFlatBuffer hashKeyOutput = { 0 };
	Cpa32U uSecretLen = 0;
	CpaCySymHashNestedModeSetupData *pNestedModeSetupData =
	    &(hashSetupData.nestedModeSetupData);
	icp_qat_fw_serv_specif_flags laCmdFlags = 0;
	icp_qat_fw_comn_flags cmnRequestFlags =
	    ICP_QAT_FW_COMN_FLAGS_BUILD(QAT_COMN_PTR_TYPE_FLAT,
					QAT_COMN_CD_FLD_TYPE_64BIT_ADR);

	sal_crypto_service_t *pService = (sal_crypto_service_t *)instanceHandle;

	/* If synchronous Operation */
	if (NULL == pKeyGenCb) {
		return LacSymKey_SslTlsSync(instanceHandle,
					    LacSync_GenFlatBufCb,
					    pCallbackTag,
					    lacCmdId,
					    pKeyGenSslTlsOpData,
					    hashAlgCipher,
					    pKeyGenOutputData);
	}
	/* Allocate the cookie */
	pCookie = (lac_sym_key_cookie_t *)Lac_MemPoolEntryAlloc(
	    pService->lac_sym_cookie_pool);
	if (NULL == pCookie) {
		LAC_LOG_ERROR("Cannot get mem pool entry");
		status = CPA_STATUS_RESOURCE;
	} else if ((void *)CPA_STATUS_RETRY == pCookie) {
		pCookie = NULL;
		status = CPA_STATUS_RETRY;
	} else {
		pSymCookie = (lac_sym_cookie_t *)pCookie;
	}

	if (CPA_STATUS_SUCCESS == status) {
		icp_qat_hw_auth_mode_t qatHashMode = 0;

		if (ICP_QAT_FW_LA_CMD_SSL3_KEY_DERIVE == lacCmdId) {
			qatHashMode = ICP_QAT_HW_AUTH_MODE0;
		} else /* TLS v1.1, v1.2, v1.3 */
		{
			qatHashMode = ICP_QAT_HW_AUTH_MODE2;
		}

		pCookie->instanceHandle = pService;
		pCookie->pCallbackTag = pCallbackTag;
		pCookie->pKeyGenCb = pKeyGenCb;
		pCookie->pKeyGenOpData = pKeyGenSslTlsOpData;
		pCookie->pKeyGenOutputData = pKeyGenOutputData;
		hashSetupData.hashMode = CPA_CY_SYM_HASH_MODE_NESTED;

		/* SSL3 */
		if (ICP_QAT_FW_LA_CMD_SSL3_KEY_DERIVE == lacCmdId) {
			hashSetupData.hashAlgorithm = CPA_CY_SYM_HASH_SHA1;
			hashSetupData.digestResultLenInBytes =
			    LAC_HASH_MD5_DIGEST_SIZE;
			pNestedModeSetupData->outerHashAlgorithm =
			    CPA_CY_SYM_HASH_MD5;

			pNestedModeSetupData->pInnerPrefixData = NULL;
			pNestedModeSetupData->innerPrefixLenInBytes = 0;
			pNestedModeSetupData->pOuterPrefixData = NULL;
			pNestedModeSetupData->outerPrefixLenInBytes = 0;
		}
		/* TLS v1.1 */
		else if (ICP_QAT_FW_LA_CMD_TLS_V1_1_KEY_DERIVE == lacCmdId) {
			CpaCyKeyGenTlsOpData *pKeyGenTlsOpData =
			    (CpaCyKeyGenTlsOpData *)pKeyGenSslTlsOpData;

			hashSetupData.hashAlgorithm = CPA_CY_SYM_HASH_SHA1;
			hashSetupData.digestResultLenInBytes =
			    LAC_HASH_MD5_DIGEST_SIZE;
			pNestedModeSetupData->outerHashAlgorithm =
			    CPA_CY_SYM_HASH_MD5;

			uSecretLen = pKeyGenTlsOpData->secret.dataLenInBytes;

			/* We want to handle pre_master_secret > 128 bytes
			 * therefore we
			 * only verify if the current operation is Master Secret
			 * Derive.
			 * The other operations remain unchanged.
			 */
			if ((uSecretLen >
			     ICP_QAT_FW_LA_TLS_V1_1_SECRET_LEN_MAX) &&
			    (CPA_CY_KEY_TLS_OP_MASTER_SECRET_DERIVE ==
				 pKeyGenTlsOpData->tlsOp ||
			     CPA_CY_KEY_TLS_OP_USER_DEFINED ==
				 pKeyGenTlsOpData->tlsOp)) {
				CpaCySymHashAlgorithm hashAlgorithm =
				    (CpaCySymHashAlgorithm)hashAlgCipher;
				/* secret = [s1 | s2 ]
				 * s1 = outer prefix, s2 = inner prefix
				 * length of s1 and s2 = ceil(secret_length / 2)
				 * (secret length + 1)/2 will always give the
				 * ceil as
				 * division by 2
				 * (>>1) will give the smallest integral value
				 * not less than
				 * arg
				 */
				tlsPrefixLen =
				    (pKeyGenTlsOpData->secret.dataLenInBytes +
				     1) >>
				    1;
				inputSecret.dataLenInBytes = tlsPrefixLen;
				inputSecret.pData =
				    pKeyGenTlsOpData->secret.pData;

				/* Since the pre_master_secret is > 128, we
				 * split the input
				 * pre_master_secret in 2 halves and compute the
				 * MD5 of the
				 * first half and the SHA1 on the second half.
				 */
				hashAlgorithm = CPA_CY_SYM_HASH_MD5;

				/* Initialize pointer where MD5 key will go. */
				hashKeyOutput.pData =
				    &pCookie->hashKeyBuffer[0];
				hashKeyOutput.dataLenInBytes =
				    LAC_HASH_MD5_DIGEST_SIZE;
				computeHashKey(&inputSecret,
					       &hashKeyOutput,
					       &hashAlgorithm);

				pNestedModeSetupData->pOuterPrefixData =
				    &pCookie->hashKeyBuffer[0];
				pNestedModeSetupData->outerPrefixLenInBytes =
				    LAC_HASH_MD5_DIGEST_SIZE;

				/* Point to the second half of the
				 * pre_master_secret */
				inputSecret.pData =
				    pKeyGenTlsOpData->secret.pData +
				    (pKeyGenTlsOpData->secret.dataLenInBytes -
				     tlsPrefixLen);

				/* Compute SHA1 on the second half of the
				 * pre_master_secret
				 */
				hashAlgorithm = CPA_CY_SYM_HASH_SHA1;
				/* Initialize pointer where SHA1 key will go. */
				hashKeyOutput.pData =
				    &pCookie->hashKeyBuffer
					 [LAC_HASH_MD5_DIGEST_SIZE];
				hashKeyOutput.dataLenInBytes =
				    LAC_HASH_SHA1_DIGEST_SIZE;
				computeHashKey(&inputSecret,
					       &hashKeyOutput,
					       &hashAlgorithm);

				pNestedModeSetupData->pInnerPrefixData =
				    &pCookie->hashKeyBuffer
					 [LAC_HASH_MD5_DIGEST_SIZE];
				pNestedModeSetupData->innerPrefixLenInBytes =
				    LAC_HASH_SHA1_DIGEST_SIZE;
			} else {
				/* secret = [s1 | s2 ]
				 * s1 = outer prefix, s2 = inner prefix
				 * length of s1 and s2 = ceil(secret_length / 2)
				 * (secret length + 1)/2 will always give the
				 * ceil as
				 * division by 2
				 * (>>1) will give the smallest integral value
				 * not less than
				 * arg
				 */
				tlsPrefixLen =
				    (pKeyGenTlsOpData->secret.dataLenInBytes +
				     1) >>
				    1;
				/* last byte of s1 will be first byte of s2 if
				 * Length is odd
				 */
				pNestedModeSetupData->pInnerPrefixData =
				    pKeyGenTlsOpData->secret.pData +
				    (pKeyGenTlsOpData->secret.dataLenInBytes -
				     tlsPrefixLen);

				pNestedModeSetupData->pOuterPrefixData =
				    pKeyGenTlsOpData->secret.pData;

				pNestedModeSetupData->innerPrefixLenInBytes =
				    pNestedModeSetupData
					->outerPrefixLenInBytes = tlsPrefixLen;
			}
		}
		/* TLS v1.2 */
		else if (ICP_QAT_FW_LA_CMD_TLS_V1_2_KEY_DERIVE == lacCmdId) {
			CpaCyKeyGenTlsOpData *pKeyGenTlsOpData =
			    (CpaCyKeyGenTlsOpData *)pKeyGenSslTlsOpData;
			CpaCySymHashAlgorithm hashAlgorithm =
			    (CpaCySymHashAlgorithm)hashAlgCipher;

			uSecretLen = pKeyGenTlsOpData->secret.dataLenInBytes;

			hashSetupData.hashAlgorithm =
			    (CpaCySymHashAlgorithm)hashAlgorithm;
			hashSetupData.digestResultLenInBytes =
			    (Cpa32U)getDigestSizeFromHashAlgo(hashAlgorithm);
			pNestedModeSetupData->outerHashAlgorithm =
			    (CpaCySymHashAlgorithm)hashAlgorithm;
			if (CPA_CY_KEY_TLS_OP_MASTER_SECRET_DERIVE ==
				pKeyGenTlsOpData->tlsOp ||
			    CPA_CY_KEY_TLS_OP_USER_DEFINED ==
				pKeyGenTlsOpData->tlsOp) {
				switch (hashAlgorithm) {
				case CPA_CY_SYM_HASH_SM3:
					precompute = CPA_FALSE;
					break;
				case CPA_CY_SYM_HASH_SHA256:
					if (uSecretLen >
					    ICP_QAT_FW_LA_TLS_V1_2_SECRET_LEN_MAX) {
						precompute = CPA_TRUE;
					}
					break;
				case CPA_CY_SYM_HASH_SHA384:
				case CPA_CY_SYM_HASH_SHA512:
					if (uSecretLen >
					    ICP_QAT_FW_LA_TLS_SECRET_LEN_MAX) {
						precompute = CPA_TRUE;
					}
					break;
				default:
					break;
				}
			}
			if (CPA_TRUE == precompute) {
				/* Case when secret > algorithm block size
				 * RFC 4868: For SHA-256 Block size is 512 bits,
				 * for SHA-384
				 * and SHA-512 Block size is 1024 bits
				 * Initialize pointer
				 * where SHAxxx key will go.
				 */
				hashKeyOutput.pData =
				    &pCookie->hashKeyBuffer[0];
				hashKeyOutput.dataLenInBytes =
				    hashSetupData.digestResultLenInBytes;
				computeHashKey(&pKeyGenTlsOpData->secret,
					       &hashKeyOutput,
					       &hashSetupData.hashAlgorithm);

				/* Outer prefix = secret , inner prefix = secret
				 * secret < 64 bytes
				 */
				pNestedModeSetupData->pInnerPrefixData =
				    hashKeyOutput.pData;
				pNestedModeSetupData->pOuterPrefixData =
				    hashKeyOutput.pData;
				pNestedModeSetupData->innerPrefixLenInBytes =
				    hashKeyOutput.dataLenInBytes;
				pNestedModeSetupData->outerPrefixLenInBytes =
				    hashKeyOutput.dataLenInBytes;
			} else {
				/* Outer prefix = secret , inner prefix = secret
				 * secret <= 64 bytes
				 */
				pNestedModeSetupData->pInnerPrefixData =
				    pKeyGenTlsOpData->secret.pData;

				pNestedModeSetupData->pOuterPrefixData =
				    pKeyGenTlsOpData->secret.pData;

				pNestedModeSetupData->innerPrefixLenInBytes =
				    pKeyGenTlsOpData->secret.dataLenInBytes;
				pNestedModeSetupData->outerPrefixLenInBytes =
				    pKeyGenTlsOpData->secret.dataLenInBytes;
			}
		}
		/* TLS v1.3 */
		else if ((ICP_QAT_FW_LA_CMD_HKDF_EXTRACT <= lacCmdId) &&
			 (ICP_QAT_FW_LA_CMD_HKDF_EXTRACT_AND_EXPAND_LABEL >=
			  lacCmdId)) {
			CpaCyKeyGenHKDFOpData *pKeyGenTlsOpData =
			    (CpaCyKeyGenHKDFOpData *)pKeyGenSslTlsOpData;
			CpaCySymHashAlgorithm hashAlgorithm =
			    getHashAlgorithmFromCipherSuiteHKDF(hashAlgCipher);

			/* Set HASH data */
			hashSetupData.hashAlgorithm = hashAlgorithm;
			/* Calculate digest length from the HASH type */
			hashSetupData.digestResultLenInBytes =
			    cipherSuiteHKDFHashSizes[hashAlgCipher]
						    [LAC_KEY_HKDF_DIGESTS];
			/* Outer Hash type is the same as inner hash type */
			pNestedModeSetupData->outerHashAlgorithm =
			    hashAlgorithm;

			/* EXPAND (PRK):
			 * Outer prefix = secret, inner prefix = secret
			 * EXTRACT (SEED/SALT):
			 * Outer prefix = seed, inner prefix = seed
			 * Secret <= 64 Bytes
			 * We do not pre compute as secret can't be larger than
			 * 64 bytes
			 */

			if ((ICP_QAT_FW_LA_CMD_HKDF_EXPAND == lacCmdId) ||
			    (ICP_QAT_FW_LA_CMD_HKDF_EXPAND_LABEL == lacCmdId)) {
				pNestedModeSetupData->pInnerPrefixData =
				    pKeyGenTlsOpData->secret;
				pNestedModeSetupData->pOuterPrefixData =
				    pKeyGenTlsOpData->secret;
				pNestedModeSetupData->innerPrefixLenInBytes =
				    pKeyGenTlsOpData->secretLen;
				pNestedModeSetupData->outerPrefixLenInBytes =
				    pKeyGenTlsOpData->secretLen;
			} else {
				pNestedModeSetupData->pInnerPrefixData =
				    pKeyGenTlsOpData->seed;
				pNestedModeSetupData->pOuterPrefixData =
				    pKeyGenTlsOpData->seed;
				pNestedModeSetupData->innerPrefixLenInBytes =
				    pKeyGenTlsOpData->seedLen;
				pNestedModeSetupData->outerPrefixLenInBytes =
				    pKeyGenTlsOpData->seedLen;
			}
		}

		/* Set the footer Data.
		 * Note that following function doesn't look at inner/outer
		 * prefix pointers in nested digest ctx
		 */
		LacSymQat_HashContentDescInit(
		    &keyGenReqFtr,
		    instanceHandle,
		    &hashSetupData,
		    pCookie
			->contentDesc, /* Pointer to base of hw setup block */
		    LAC_SYM_KEY_NO_HASH_BLK_OFFSET_QW,
		    ICP_QAT_FW_SLICE_DRAM_WR,
		    qatHashMode,
		    CPA_FALSE, /* Not using sym Constants Table in Shared SRAM
				*/
		    CPA_FALSE, /* not using the optimised content Desc */
		    CPA_FALSE, /* Not using the stateful SHA3 Content Desc */
		    NULL,      /* precompute data */
		    &hashBlkSizeInBytes);

		/* SSL3 */
		if (ICP_QAT_FW_LA_CMD_SSL3_KEY_DERIVE == lacCmdId) {
			CpaCyKeyGenSslOpData *pKeyGenSslOpData =
			    (CpaCyKeyGenSslOpData *)pKeyGenSslTlsOpData;
			Cpa8U *pLabel = NULL;
			Cpa32U labelLen = 0;
			Cpa8U iterations = 0;
			Cpa64U labelPhysAddr = 0;

			/* Iterations = ceiling of output required / output per
			 * iteration Ceiling of a / b = (a + (b-1)) / b
			 */
			iterations =
			    (pKeyGenSslOpData->generatedKeyLenInBytes +
			     (LAC_SYM_QAT_KEY_SSL_BYTES_PER_ITERATION - 1)) >>
			    LAC_SYM_QAT_KEY_SSL_ITERATIONS_SHIFT;

			if (CPA_CY_KEY_SSL_OP_USER_DEFINED ==
			    pKeyGenSslOpData->sslOp) {
				pLabel = pKeyGenSslOpData->userLabel.pData;
				labelLen =
				    pKeyGenSslOpData->userLabel.dataLenInBytes;
				labelPhysAddr = LAC_OS_VIRT_TO_PHYS_EXTERNAL(
				    pService->generic_service_info, pLabel);

				if (labelPhysAddr == 0) {
					LAC_LOG_ERROR(
					    "Unable to get the physical address of the"
					    " label");
					status = CPA_STATUS_FAIL;
				}
			} else {
				pLabel = pService->pSslLabel;

				/* Calculate label length.
				 * eg. 3 iterations is ABBCCC so length is 6
				 */
				labelLen =
				    ((iterations * iterations) + iterations) >>
				    1;
				labelPhysAddr =
				    LAC_OS_VIRT_TO_PHYS_INTERNAL(pLabel);
			}

			LacSymQat_KeySslRequestPopulate(
			    &keyGenReqHdr,
			    &keyGenReqMid,
			    pKeyGenSslOpData->generatedKeyLenInBytes,
			    labelLen,
			    pKeyGenSslOpData->secret.dataLenInBytes,
			    iterations);

			LacSymQat_KeySslKeyMaterialInputPopulate(
			    &(pService->generic_service_info),
			    &(pCookie->u.sslKeyInput),
			    pKeyGenSslOpData->seed.pData,
			    labelPhysAddr,
			    pKeyGenSslOpData->secret.pData);

			inputPhysAddr = LAC_MEM_CAST_PTR_TO_UINT64(
			    pSymCookie->keySslKeyInputPhyAddr);
		}
		/* TLS v1.1, v1.2 */
		else if (ICP_QAT_FW_LA_CMD_TLS_V1_1_KEY_DERIVE == lacCmdId ||
			 ICP_QAT_FW_LA_CMD_TLS_V1_2_KEY_DERIVE == lacCmdId) {
			CpaCyKeyGenTlsOpData *pKeyGenTlsOpData =
			    (CpaCyKeyGenTlsOpData *)pKeyGenSslTlsOpData;
			lac_sym_qat_hash_state_buffer_info_t
			    hashStateBufferInfo = { 0 };
			CpaBoolean hashStateBuffer = CPA_FALSE;
			icp_qat_fw_auth_cd_ctrl_hdr_t *pHashControlBlock =
			    (icp_qat_fw_auth_cd_ctrl_hdr_t *)&(
				keyGenReqFtr.cd_ctrl);
			icp_qat_la_auth_req_params_t *pHashReqParams = NULL;
			Cpa8U *pLabel = NULL;
			Cpa32U labelLen = 0;
			Cpa64U labelPhysAddr = 0;
			hashStateBufferInfo.pData = pCookie->hashStateBuffer;
			hashStateBufferInfo.pDataPhys =
			    LAC_MEM_CAST_PTR_TO_UINT64(
				pSymCookie->keyHashStateBufferPhyAddr);
			hashStateBufferInfo.stateStorageSzQuadWords = 0;

			LacSymQat_HashSetupReqParamsMetaData(&(keyGenReqFtr),
							     instanceHandle,
							     &(hashSetupData),
							     hashStateBuffer,
							     qatHashMode,
							     CPA_FALSE);

			pHashReqParams = (icp_qat_la_auth_req_params_t *)&(
			    keyGenReqFtr.serv_specif_rqpars);

			hashStateBufferInfo.prefixAadSzQuadWords =
			    LAC_BYTES_TO_QUADWORDS(
				pHashReqParams->u2.inner_prefix_sz +
				pHashControlBlock->outer_prefix_sz);

			/* Copy prefix data into hash state buffer */
			pMsgDummy = (Cpa8U *)&(keyGenReq);
			pCacheDummyHdr = (Cpa8U *)&(keyGenReqHdr);
			pCacheDummyMid = (Cpa8U *)&(keyGenReqMid);
			pCacheDummyFtr = (Cpa8U *)&(keyGenReqFtr);
			memcpy(pMsgDummy,
			       pCacheDummyHdr,
			       (LAC_LONG_WORD_IN_BYTES *
				LAC_SIZE_OF_CACHE_HDR_IN_LW));
			memcpy(pMsgDummy + (LAC_LONG_WORD_IN_BYTES *
					    LAC_START_OF_CACHE_MID_IN_LW),
			       pCacheDummyMid,
			       (LAC_LONG_WORD_IN_BYTES *
				LAC_SIZE_OF_CACHE_MID_IN_LW));
			memcpy(pMsgDummy + (LAC_LONG_WORD_IN_BYTES *
					    LAC_START_OF_CACHE_FTR_IN_LW),
			       pCacheDummyFtr,
			       (LAC_LONG_WORD_IN_BYTES *
				LAC_SIZE_OF_CACHE_FTR_IN_LW));

			LacSymQat_HashStatePrefixAadBufferPopulate(
			    &hashStateBufferInfo,
			    &keyGenReqFtr,
			    pNestedModeSetupData->pInnerPrefixData,
			    pNestedModeSetupData->innerPrefixLenInBytes,
			    pNestedModeSetupData->pOuterPrefixData,
			    pNestedModeSetupData->outerPrefixLenInBytes);

			/* Firmware only looks at hash state buffer pointer and
			 * the
			 * hash state buffer size so all other fields are set to
			 * 0
			 */
			LacSymQat_HashRequestParamsPopulate(
			    &(keyGenReq),
			    0, /* Auth offset */
			    0, /* Auth length */
			    &(pService->generic_service_info),
			    &hashStateBufferInfo, /* Hash state prefix buffer */
			    ICP_QAT_FW_LA_PARTIAL_NONE,
			    0, /* Hash result size */
			    CPA_FALSE,
			    NULL,
			    CPA_CY_SYM_HASH_NONE, /* Hash algorithm */
			    NULL);		  /* HKDF only */

			/* Set up the labels and their length */
			if (CPA_CY_KEY_TLS_OP_USER_DEFINED ==
			    pKeyGenTlsOpData->tlsOp) {
				pLabel = pKeyGenTlsOpData->userLabel.pData;
				labelLen =
				    pKeyGenTlsOpData->userLabel.dataLenInBytes;
				labelPhysAddr = LAC_OS_VIRT_TO_PHYS_EXTERNAL(
				    pService->generic_service_info, pLabel);

				if (labelPhysAddr == 0) {
					LAC_LOG_ERROR(
					    "Unable to get the physical address of the"
					    " label");
					status = CPA_STATUS_FAIL;
				}
			} else if (CPA_CY_KEY_TLS_OP_MASTER_SECRET_DERIVE ==
				   pKeyGenTlsOpData->tlsOp) {
				pLabel = pService->pTlsLabel->masterSecret;
				labelLen =
				    sizeof(
					LAC_SYM_KEY_TLS_MASTER_SECRET_LABEL) -
				    1;
				labelPhysAddr =
				    LAC_OS_VIRT_TO_PHYS_INTERNAL(pLabel);
			} else if (CPA_CY_KEY_TLS_OP_KEY_MATERIAL_DERIVE ==
				   pKeyGenTlsOpData->tlsOp) {
				pLabel = pService->pTlsLabel->keyMaterial;
				labelLen =
				    sizeof(LAC_SYM_KEY_TLS_KEY_MATERIAL_LABEL) -
				    1;
				labelPhysAddr =
				    LAC_OS_VIRT_TO_PHYS_INTERNAL(pLabel);
			} else if (CPA_CY_KEY_TLS_OP_CLIENT_FINISHED_DERIVE ==
				   pKeyGenTlsOpData->tlsOp) {
				pLabel = pService->pTlsLabel->clientFinished;
				labelLen =
				    sizeof(LAC_SYM_KEY_TLS_CLIENT_FIN_LABEL) -
				    1;
				labelPhysAddr =
				    LAC_OS_VIRT_TO_PHYS_INTERNAL(pLabel);
			} else {
				pLabel = pService->pTlsLabel->serverFinished;
				labelLen =
				    sizeof(LAC_SYM_KEY_TLS_SERVER_FIN_LABEL) -
				    1;
				labelPhysAddr =
				    LAC_OS_VIRT_TO_PHYS_INTERNAL(pLabel);
			}
			LacSymQat_KeyTlsRequestPopulate(
			    &keyGenReqMid,
			    pKeyGenTlsOpData->generatedKeyLenInBytes,
			    labelLen,
			    pKeyGenTlsOpData->secret.dataLenInBytes,
			    pKeyGenTlsOpData->seed.dataLenInBytes,
			    lacCmdId);

			LacSymQat_KeyTlsKeyMaterialInputPopulate(
			    &(pService->generic_service_info),
			    &(pCookie->u.tlsKeyInput),
			    pKeyGenTlsOpData->seed.pData,
			    labelPhysAddr);

			inputPhysAddr = LAC_MEM_CAST_PTR_TO_UINT64(
			    pSymCookie->keyTlsKeyInputPhyAddr);
		}
		/* TLS v1.3 */
		else if (ICP_QAT_FW_LA_CMD_HKDF_EXTRACT <= lacCmdId &&
			 ICP_QAT_FW_LA_CMD_HKDF_EXTRACT_AND_EXPAND >=
			     lacCmdId) {
			CpaCyKeyGenHKDFOpData *pKeyGenTlsOpData =
			    (CpaCyKeyGenHKDFOpData *)pKeyGenSslTlsOpData;
			lac_sym_qat_hash_state_buffer_info_t
			    hashStateBufferInfo = { 0 };
			CpaBoolean hashStateBuffer = CPA_FALSE;
			icp_qat_fw_auth_cd_ctrl_hdr_t *pHashControlBlock =
			    (icp_qat_fw_auth_cd_ctrl_hdr_t *)&(
				keyGenReqFtr.cd_ctrl);
			icp_qat_la_auth_req_params_t *pHashReqParams = NULL;
			hashStateBufferInfo.pData = pCookie->hashStateBuffer;
			hashStateBufferInfo.pDataPhys =
			    LAC_MEM_CAST_PTR_TO_UINT64(
				pSymCookie->keyHashStateBufferPhyAddr);
			hashStateBufferInfo.stateStorageSzQuadWords = 0;

			LacSymQat_HashSetupReqParamsMetaData(&(keyGenReqFtr),
							     instanceHandle,
							     &(hashSetupData),
							     hashStateBuffer,
							     qatHashMode,
							     CPA_FALSE);

			pHashReqParams = (icp_qat_la_auth_req_params_t *)&(
			    keyGenReqFtr.serv_specif_rqpars);

			hashStateBufferInfo.prefixAadSzQuadWords =
			    LAC_BYTES_TO_QUADWORDS(
				pHashReqParams->u2.inner_prefix_sz +
				pHashControlBlock->outer_prefix_sz);

			/* Copy prefix data into hash state buffer */
			pMsgDummy = (Cpa8U *)&(keyGenReq);
			pCacheDummyHdr = (Cpa8U *)&(keyGenReqHdr);
			pCacheDummyMid = (Cpa8U *)&(keyGenReqMid);
			pCacheDummyFtr = (Cpa8U *)&(keyGenReqFtr);
			memcpy(pMsgDummy,
			       pCacheDummyHdr,
			       (LAC_LONG_WORD_IN_BYTES *
				LAC_SIZE_OF_CACHE_HDR_IN_LW));
			memcpy(pMsgDummy + (LAC_LONG_WORD_IN_BYTES *
					    LAC_START_OF_CACHE_MID_IN_LW),
			       pCacheDummyMid,
			       (LAC_LONG_WORD_IN_BYTES *
				LAC_SIZE_OF_CACHE_MID_IN_LW));
			memcpy(pMsgDummy + (LAC_LONG_WORD_IN_BYTES *
					    LAC_START_OF_CACHE_FTR_IN_LW),
			       pCacheDummyFtr,
			       (LAC_LONG_WORD_IN_BYTES *
				LAC_SIZE_OF_CACHE_FTR_IN_LW));

			LacSymQat_HashStatePrefixAadBufferPopulate(
			    &hashStateBufferInfo,
			    &keyGenReqFtr,
			    pNestedModeSetupData->pInnerPrefixData,
			    pNestedModeSetupData->innerPrefixLenInBytes,
			    pNestedModeSetupData->pOuterPrefixData,
			    pNestedModeSetupData->outerPrefixLenInBytes);

			/* Firmware only looks at hash state buffer pointer and
			 * the
			 * hash state buffer size so all other fields are set to
			 * 0
			 */
			LacSymQat_HashRequestParamsPopulate(
			    &(keyGenReq),
			    0, /* Auth offset */
			    0, /* Auth length */
			    &(pService->generic_service_info),
			    &hashStateBufferInfo, /* Hash state prefix buffer */
			    ICP_QAT_FW_LA_PARTIAL_NONE,
			    0, /* Hash result size */
			    CPA_FALSE,
			    NULL,
			    CPA_CY_SYM_HASH_NONE,      /* Hash algorithm */
			    pKeyGenTlsOpData->secret); /* IKM or PRK */

			LacSymQat_KeyTlsRequestPopulate(
			    &keyGenReqMid,
			    cipherSuiteHKDFHashSizes[hashAlgCipher]
						    [LAC_KEY_HKDF_DIGESTS],
			    /* For EXTRACT, EXPAND, FW expects info to be passed
			       as label */
			    pKeyGenTlsOpData->infoLen,
			    pKeyGenTlsOpData->secretLen,
			    pKeyGenTlsOpData->seedLen,
			    lacCmdId);

			LacSymQat_KeyTlsHKDFKeyMaterialInputPopulate(
			    &(pService->generic_service_info),
			    &(pCookie->u.tlsHKDFKeyInput),
			    pKeyGenTlsOpData,
			    0,	 /* No subLabels used */
			    lacCmdId); /* Pass op being performed */

			inputPhysAddr = LAC_MEM_CAST_PTR_TO_UINT64(
			    pSymCookie->keyTlsKeyInputPhyAddr);
		}
		/* TLS v1.3 LABEL */
		else if (ICP_QAT_FW_LA_CMD_HKDF_EXPAND_LABEL == lacCmdId ||
			 ICP_QAT_FW_LA_CMD_HKDF_EXTRACT_AND_EXPAND_LABEL ==
			     lacCmdId) {
			CpaCyKeyGenHKDFOpData *pKeyGenTlsOpData =
			    (CpaCyKeyGenHKDFOpData *)pKeyGenSslTlsOpData;
			Cpa64U subLabelsPhysAddr = 0;
			lac_sym_qat_hash_state_buffer_info_t
			    hashStateBufferInfo = { 0 };
			CpaBoolean hashStateBuffer = CPA_FALSE;
			icp_qat_fw_auth_cd_ctrl_hdr_t *pHashControlBlock =
			    (icp_qat_fw_auth_cd_ctrl_hdr_t *)&(
				keyGenReqFtr.cd_ctrl);
			icp_qat_la_auth_req_params_t *pHashReqParams = NULL;
			hashStateBufferInfo.pData = pCookie->hashStateBuffer;
			hashStateBufferInfo.pDataPhys =
			    LAC_MEM_CAST_PTR_TO_UINT64(
				pSymCookie->keyHashStateBufferPhyAddr);
			hashStateBufferInfo.stateStorageSzQuadWords = 0;

			LacSymQat_HashSetupReqParamsMetaData(&(keyGenReqFtr),
							     instanceHandle,
							     &(hashSetupData),
							     hashStateBuffer,
							     qatHashMode,
							     CPA_FALSE);

			pHashReqParams = (icp_qat_la_auth_req_params_t *)&(
			    keyGenReqFtr.serv_specif_rqpars);

			hashStateBufferInfo.prefixAadSzQuadWords =
			    LAC_BYTES_TO_QUADWORDS(
				pHashReqParams->u2.inner_prefix_sz +
				pHashControlBlock->outer_prefix_sz);

			/* Copy prefix data into hash state buffer */
			pMsgDummy = (Cpa8U *)&(keyGenReq);
			pCacheDummyHdr = (Cpa8U *)&(keyGenReqHdr);
			pCacheDummyMid = (Cpa8U *)&(keyGenReqMid);
			pCacheDummyFtr = (Cpa8U *)&(keyGenReqFtr);
			memcpy(pMsgDummy,
			       pCacheDummyHdr,
			       (LAC_LONG_WORD_IN_BYTES *
				LAC_SIZE_OF_CACHE_HDR_IN_LW));
			memcpy(pMsgDummy + (LAC_LONG_WORD_IN_BYTES *
					    LAC_START_OF_CACHE_MID_IN_LW),
			       pCacheDummyMid,
			       (LAC_LONG_WORD_IN_BYTES *
				LAC_SIZE_OF_CACHE_MID_IN_LW));
			memcpy(pMsgDummy + (LAC_LONG_WORD_IN_BYTES *
					    LAC_START_OF_CACHE_FTR_IN_LW),
			       pCacheDummyFtr,
			       (LAC_LONG_WORD_IN_BYTES *
				LAC_SIZE_OF_CACHE_FTR_IN_LW));

			LacSymQat_HashStatePrefixAadBufferPopulate(
			    &hashStateBufferInfo,
			    &keyGenReqFtr,
			    pNestedModeSetupData->pInnerPrefixData,
			    pNestedModeSetupData->innerPrefixLenInBytes,
			    pNestedModeSetupData->pOuterPrefixData,
			    pNestedModeSetupData->outerPrefixLenInBytes);

			/* Firmware only looks at hash state buffer pointer and
			 * the
			 * hash state buffer size so all other fields are set to
			 * 0
			 */
			LacSymQat_HashRequestParamsPopulate(
			    &(keyGenReq),
			    0, /* Auth offset */
			    0, /* Auth length */
			    &(pService->generic_service_info),
			    &hashStateBufferInfo, /* Hash state prefix buffer */
			    ICP_QAT_FW_LA_PARTIAL_NONE,
			    0, /* Hash result size */
			    CPA_FALSE,
			    NULL,
			    CPA_CY_SYM_HASH_NONE,      /* Hash algorithm */
			    pKeyGenTlsOpData->secret); /* IKM or PRK */

			LacSymQat_KeyTlsRequestPopulate(
			    &keyGenReqMid,
			    cipherSuiteHKDFHashSizes[hashAlgCipher]
						    [LAC_KEY_HKDF_DIGESTS],
			    pKeyGenTlsOpData->numLabels, /* Number of Labels */
			    pKeyGenTlsOpData->secretLen,
			    pKeyGenTlsOpData->seedLen,
			    lacCmdId);

			/* Get physical address of subLabels */
			switch (hashAlgCipher) {
			case CPA_CY_HKDF_TLS_AES_128_GCM_SHA256: /* Fall Through
								    */
			case CPA_CY_HKDF_TLS_AES_128_CCM_SHA256:
			case CPA_CY_HKDF_TLS_AES_128_CCM_8_SHA256:
				subLabelsPhysAddr = pService->pTlsHKDFSubLabel
							->sublabelPhysAddr256;
				break;
			case CPA_CY_HKDF_TLS_CHACHA20_POLY1305_SHA256:
				subLabelsPhysAddr =
				    pService->pTlsHKDFSubLabel
					->sublabelPhysAddrChaChaPoly;
				break;
			case CPA_CY_HKDF_TLS_AES_256_GCM_SHA384:
				subLabelsPhysAddr = pService->pTlsHKDFSubLabel
							->sublabelPhysAddr384;
				break;
			default:
				break;
			}

			LacSymQat_KeyTlsHKDFKeyMaterialInputPopulate(
			    &(pService->generic_service_info),
			    &(pCookie->u.tlsHKDFKeyInput),
			    pKeyGenTlsOpData,
			    subLabelsPhysAddr,
			    lacCmdId); /* Pass op being performed */

			inputPhysAddr = LAC_MEM_CAST_PTR_TO_UINT64(
			    pSymCookie->keyTlsKeyInputPhyAddr);
		}

		outputPhysAddr = LAC_MEM_CAST_PTR_TO_UINT64(
		    LAC_OS_VIRT_TO_PHYS_EXTERNAL(pService->generic_service_info,
						 pKeyGenOutputData->pData));

		if (outputPhysAddr == 0) {
			LAC_LOG_ERROR(
			    "Unable to get the physical address of the"
			    " output buffer");
			status = CPA_STATUS_FAIL;
		}
	}
	if (CPA_STATUS_SUCCESS == status) {
		Cpa8U lw26[4];
		char *tmp = NULL;
		unsigned char a;
		int n = 0;
		/* Make up the full keyGenReq struct from its constituents
		 * before calling the SalQatMsg functions below.
		 * Note: The full cache struct has been reduced to a
		 * header, mid and footer for memory size reduction
		 */
		pMsgDummy = (Cpa8U *)&(keyGenReq);
		pCacheDummyHdr = (Cpa8U *)&(keyGenReqHdr);
		pCacheDummyMid = (Cpa8U *)&(keyGenReqMid);
		pCacheDummyFtr = (Cpa8U *)&(keyGenReqFtr);

		memcpy(pMsgDummy,
		       pCacheDummyHdr,
		       (LAC_LONG_WORD_IN_BYTES * LAC_SIZE_OF_CACHE_HDR_IN_LW));
		memcpy(pMsgDummy + (LAC_LONG_WORD_IN_BYTES *
				    LAC_START_OF_CACHE_MID_IN_LW),
		       pCacheDummyMid,
		       (LAC_LONG_WORD_IN_BYTES * LAC_SIZE_OF_CACHE_MID_IN_LW));
		memcpy(&lw26,
		       pMsgDummy + (LAC_LONG_WORD_IN_BYTES *
				    LAC_START_OF_CACHE_FTR_IN_LW),
		       LAC_LONG_WORD_IN_BYTES);
		memcpy(pMsgDummy + (LAC_LONG_WORD_IN_BYTES *
				    LAC_START_OF_CACHE_FTR_IN_LW),
		       pCacheDummyFtr,
		       (LAC_LONG_WORD_IN_BYTES * LAC_SIZE_OF_CACHE_FTR_IN_LW));
		tmp = (char *)(pMsgDummy + (LAC_LONG_WORD_IN_BYTES *
					    LAC_START_OF_CACHE_FTR_IN_LW));

		/* Copy LW26, or'd with what's already there, into the Msg, for
		 * TLS */
		for (n = 0; n < LAC_LONG_WORD_IN_BYTES; n++) {
			a = (unsigned char)*(tmp + n);
			lw26[n] = lw26[n] | a;
		}
		memcpy(pMsgDummy + (LAC_LONG_WORD_IN_BYTES *
				    LAC_START_OF_CACHE_FTR_IN_LW),
		       &lw26,
		       LAC_LONG_WORD_IN_BYTES);

		contentDescInfo.pData = pCookie->contentDesc;
		contentDescInfo.hardwareSetupBlockPhys =
		    LAC_MEM_CAST_PTR_TO_UINT64(
			pSymCookie->keyContentDescPhyAddr);
		contentDescInfo.hwBlkSzQuadWords =
		    LAC_BYTES_TO_QUADWORDS(hashBlkSizeInBytes);

		/* Populate common request fields */
		SalQatMsg_ContentDescHdrWrite((icp_qat_fw_comn_req_t *)&(
						  keyGenReq),
					      &(contentDescInfo));

		SalQatMsg_CmnHdrWrite((icp_qat_fw_comn_req_t *)&keyGenReq,
				      ICP_QAT_FW_COMN_REQ_CPM_FW_LA,
				      lacCmdId,
				      cmnRequestFlags,
				      laCmdFlags);

		SalQatMsg_CmnMidWrite((icp_qat_fw_la_bulk_req_t *)&(keyGenReq),
				      pCookie,
				      LAC_SYM_KEY_QAT_PTR_TYPE,
				      inputPhysAddr,
				      outputPhysAddr,
				      0,
				      0);

		/* Send to QAT */
		status = icp_adf_transPutMsg(pService->trans_handle_sym_tx,
					     (void *)&(keyGenReq),
					     LAC_QAT_SYM_REQ_SZ_LW);
	}
	if (CPA_STATUS_SUCCESS == status) {
		/* Update stats */
		LacKey_StatsInc(lacCmdId,
				LAC_KEY_REQUESTS,
				pCookie->instanceHandle);
	} else {
		/* Clean up cookie memory */
		if (NULL != pCookie) {
			LacKey_StatsInc(lacCmdId,
					LAC_KEY_REQUEST_ERRORS,
					pCookie->instanceHandle);
			Lac_MemPoolEntryFree(pCookie);
		}
	}
	return status;
}

/**
 * @ingroup LacSymKey
 *      Parameters check for TLS v1.0/1.1, v1.2, v1.3 and SSL3
 * @description
 *      Check user parameters against the firmware/spec requirements.
 *
 * @param[in] pKeyGenOpData              Pointer to a structure containing all
 *                                       the data needed to perform the key
 *                                       generation operation.
 * @param[in]  hashAlgCipher             Specifies the hash algorithm,
 *                                       or cipher we are using.
 *                                       According to RFC5246, this should be
 *                                       "SHA-256 or a stronger standard hash
 *                                       function."
 * @param[in] pGeneratedKeyBuffer        User output buffers.
 * @param[in] cmdId                      Keygen operation to perform.
 */
static CpaStatus
LacSymKey_CheckParamSslTls(const void *pKeyGenOpData,
			   Cpa8U hashAlgCipher,
			   const CpaFlatBuffer *pGeneratedKeyBuffer,
			   icp_qat_fw_la_cmd_id_t cmdId)
{
	/* Api max value */
	Cpa32U maxSecretLen = 0;
	Cpa32U maxSeedLen = 0;
	Cpa32U maxOutputLen = 0;
	Cpa32U maxInfoLen = 0;
	Cpa32U maxLabelLen = 0;

	/* User info */
	Cpa32U uSecretLen = 0;
	Cpa32U uSeedLen = 0;
	Cpa32U uOutputLen = 0;

	LAC_CHECK_NULL_PARAM(pKeyGenOpData);
	LAC_CHECK_NULL_PARAM(pGeneratedKeyBuffer);
	LAC_CHECK_NULL_PARAM(pGeneratedKeyBuffer->pData);

	if (ICP_QAT_FW_LA_CMD_SSL3_KEY_DERIVE == cmdId) {
		CpaCyKeyGenSslOpData *opData =
		    (CpaCyKeyGenSslOpData *)pKeyGenOpData;

		/* User info */
		uSecretLen = opData->secret.dataLenInBytes;
		uSeedLen = opData->seed.dataLenInBytes;
		uOutputLen = opData->generatedKeyLenInBytes;

		/* Api max value */
		maxSecretLen = ICP_QAT_FW_LA_SSL_SECRET_LEN_MAX;
		maxSeedLen = ICP_QAT_FW_LA_SSL_SEED_LEN_MAX;
		maxOutputLen = ICP_QAT_FW_LA_SSL_OUTPUT_LEN_MAX;

		/* Check user buffers */
		LAC_CHECK_NULL_PARAM(opData->secret.pData);
		LAC_CHECK_NULL_PARAM(opData->seed.pData);

		/* Check operation */
		if ((Cpa32U)opData->sslOp > CPA_CY_KEY_SSL_OP_USER_DEFINED) {
			LAC_INVALID_PARAM_LOG("opData->sslOp");
			return CPA_STATUS_INVALID_PARAM;
		}
		if ((Cpa32U)opData->sslOp == CPA_CY_KEY_SSL_OP_USER_DEFINED) {
			LAC_CHECK_NULL_PARAM(opData->userLabel.pData);
			/* Maximum label length for SSL Key Gen request */
			if (opData->userLabel.dataLenInBytes >
			    ICP_QAT_FW_LA_SSL_LABEL_LEN_MAX) {
				LAC_INVALID_PARAM_LOG(
				    "userLabel.dataLenInBytes");
				return CPA_STATUS_INVALID_PARAM;
			}
		}

		/* Only seed length for SSL3 Key Gen request */
		if (maxSeedLen != uSeedLen) {
			LAC_INVALID_PARAM_LOG("seed.dataLenInBytes");
			return CPA_STATUS_INVALID_PARAM;
		}

		/* Maximum output length for SSL3 Key Gen request */
		if (uOutputLen > maxOutputLen) {
			LAC_INVALID_PARAM_LOG("generatedKeyLenInBytes");
			return CPA_STATUS_INVALID_PARAM;
		}
	}
	/* TLS v1.1 or TLS v.12 */
	else if (ICP_QAT_FW_LA_CMD_TLS_V1_1_KEY_DERIVE == cmdId ||
		 ICP_QAT_FW_LA_CMD_TLS_V1_2_KEY_DERIVE == cmdId) {
		CpaCyKeyGenTlsOpData *opData =
		    (CpaCyKeyGenTlsOpData *)pKeyGenOpData;

		/* User info */
		uSecretLen = opData->secret.dataLenInBytes;
		uSeedLen = opData->seed.dataLenInBytes;
		uOutputLen = opData->generatedKeyLenInBytes;

		if (ICP_QAT_FW_LA_CMD_TLS_V1_1_KEY_DERIVE == cmdId) {
			/* Api max value */
			/* ICP_QAT_FW_LA_TLS_V1_1_SECRET_LEN_MAX needs to be
			 * multiplied
			 * by 4 in order to verifiy the 512 conditions. We did
			 * not change
			 * ICP_QAT_FW_LA_TLS_V1_1_SECRET_LEN_MAX as it
			 * represents
			 * the max value tha firmware can handle.
			 */
			maxSecretLen =
			    ICP_QAT_FW_LA_TLS_V1_1_SECRET_LEN_MAX * 4;
		} else {
			/* Api max value */
			/* ICP_QAT_FW_LA_TLS_V1_2_SECRET_LEN_MAX needs to be
			 * multiplied
			 * by 8 in order to verifiy the 512 conditions. We did
			 * not change
			 * ICP_QAT_FW_LA_TLS_V1_2_SECRET_LEN_MAX as it
			 * represents
			 * the max value tha firmware can handle.
			 */
			maxSecretLen =
			    ICP_QAT_FW_LA_TLS_V1_2_SECRET_LEN_MAX * 8;

			/* Check Hash algorithm */
			if (0 == getDigestSizeFromHashAlgo(hashAlgCipher)) {
				LAC_INVALID_PARAM_LOG("hashAlgorithm");
				return CPA_STATUS_INVALID_PARAM;
			}
		}
		maxSeedLen = ICP_QAT_FW_LA_TLS_SEED_LEN_MAX;
		maxOutputLen = ICP_QAT_FW_LA_TLS_OUTPUT_LEN_MAX;
		/* Check user buffers */
		LAC_CHECK_NULL_PARAM(opData->secret.pData);
		LAC_CHECK_NULL_PARAM(opData->seed.pData);

		/* Check operation */
		if ((Cpa32U)opData->tlsOp > CPA_CY_KEY_TLS_OP_USER_DEFINED) {
			LAC_INVALID_PARAM_LOG("opData->tlsOp");
			return CPA_STATUS_INVALID_PARAM;
		} else if ((Cpa32U)opData->tlsOp ==
			   CPA_CY_KEY_TLS_OP_USER_DEFINED) {
			LAC_CHECK_NULL_PARAM(opData->userLabel.pData);
			/* Maximum label length for TLS Key Gen request */
			if (opData->userLabel.dataLenInBytes >
			    ICP_QAT_FW_LA_TLS_LABEL_LEN_MAX) {
				LAC_INVALID_PARAM_LOG(
				    "userLabel.dataLenInBytes");
				return CPA_STATUS_INVALID_PARAM;
			}
		}

		/* Maximum/only seed length for TLS Key Gen request */
		if (((Cpa32U)opData->tlsOp !=
		     CPA_CY_KEY_TLS_OP_MASTER_SECRET_DERIVE) &&
		    ((Cpa32U)opData->tlsOp !=
		     CPA_CY_KEY_TLS_OP_KEY_MATERIAL_DERIVE)) {
			if (uSeedLen > maxSeedLen) {
				LAC_INVALID_PARAM_LOG("seed.dataLenInBytes");
				return CPA_STATUS_INVALID_PARAM;
			}
		} else {
			if (maxSeedLen != uSeedLen) {
				LAC_INVALID_PARAM_LOG("seed.dataLenInBytes");
				return CPA_STATUS_INVALID_PARAM;
			}
		}

		/* Maximum output length for TLS Key Gen request */
		if (uOutputLen > maxOutputLen) {
			LAC_INVALID_PARAM_LOG("generatedKeyLenInBytes");
			return CPA_STATUS_INVALID_PARAM;
		}
	}
	/* TLS v1.3 */
	else if (cmdId >= ICP_QAT_FW_LA_CMD_HKDF_EXTRACT &&
		 cmdId <= ICP_QAT_FW_LA_CMD_HKDF_EXTRACT_AND_EXPAND_LABEL) {
		CpaCyKeyGenHKDFOpData *HKDF_Data =
		    (CpaCyKeyGenHKDFOpData *)pKeyGenOpData;
		CpaCyKeyHKDFCipherSuite cipherSuite = hashAlgCipher;
		CpaCySymHashAlgorithm hashAlgorithm =
		    getHashAlgorithmFromCipherSuiteHKDF(cipherSuite);
		maxSeedLen =
		    cipherSuiteHKDFHashSizes[cipherSuite][LAC_KEY_HKDF_DIGESTS];
		maxSecretLen = CPA_CY_HKDF_KEY_MAX_SECRET_SZ;
		maxInfoLen = CPA_CY_HKDF_KEY_MAX_INFO_SZ;
		maxLabelLen = CPA_CY_HKDF_KEY_MAX_LABEL_SZ;

		uSecretLen = HKDF_Data->secretLen;

		/* Check using supported hash function */
		if (0 ==
		    (uOutputLen = getDigestSizeFromHashAlgo(hashAlgorithm))) {
			LAC_INVALID_PARAM_LOG("Hash function not supported");
			return CPA_STATUS_INVALID_PARAM;
		}

		/* Number of labels does not exceed the MAX */
		if (HKDF_Data->numLabels > CPA_CY_HKDF_KEY_MAX_LABEL_COUNT) {
			LAC_INVALID_PARAM_LOG(
			    "CpaCyKeyGenHKDFOpData.numLabels");
			return CPA_STATUS_INVALID_PARAM;
		}

		switch (cmdId) {
		case ICP_QAT_FW_LA_CMD_HKDF_EXTRACT:
			if (maxSeedLen < HKDF_Data->seedLen) {
				LAC_INVALID_PARAM_LOG(
				    "CpaCyKeyGenHKDFOpData.seedLen");
				return CPA_STATUS_INVALID_PARAM;
			}
			break;
		case ICP_QAT_FW_LA_CMD_HKDF_EXPAND:
			maxSecretLen =
			    cipherSuiteHKDFHashSizes[cipherSuite]
						    [LAC_KEY_HKDF_DIGESTS];

			if (maxInfoLen < HKDF_Data->infoLen) {
				LAC_INVALID_PARAM_LOG(
				    "CpaCyKeyGenHKDFOpData.infoLen");
				return CPA_STATUS_INVALID_PARAM;
			}
			break;
		case ICP_QAT_FW_LA_CMD_HKDF_EXTRACT_AND_EXPAND:
			uOutputLen *= 2;
			if (maxSeedLen < HKDF_Data->seedLen) {
				LAC_INVALID_PARAM_LOG(
				    "CpaCyKeyGenHKDFOpData.seedLen");
				return CPA_STATUS_INVALID_PARAM;
			}
			if (maxInfoLen < HKDF_Data->infoLen) {
				LAC_INVALID_PARAM_LOG(
				    "CpaCyKeyGenHKDFOpData.infoLen");
				return CPA_STATUS_INVALID_PARAM;
			}
			break;
		case ICP_QAT_FW_LA_CMD_HKDF_EXPAND_LABEL: /* Fall through */
		case ICP_QAT_FW_LA_CMD_HKDF_EXTRACT_AND_EXPAND_LABEL: {
			Cpa8U subl_mask = 0, subl_number = 1;
			Cpa8U i = 0;

			if (maxSeedLen < HKDF_Data->seedLen) {
				LAC_INVALID_PARAM_LOG(
				    "CpaCyKeyGenHKDFOpData.seedLen");
				return CPA_STATUS_INVALID_PARAM;
			}

			/* If EXPAND set uOutputLen to zero */
			if (ICP_QAT_FW_LA_CMD_HKDF_EXPAND_LABEL == cmdId) {
				uOutputLen = 0;
				maxSecretLen = cipherSuiteHKDFHashSizes
				    [cipherSuite][LAC_KEY_HKDF_DIGESTS];
			}

			for (i = 0; i < HKDF_Data->numLabels; i++) {
				/* Check that the labelLen does not overflow */
				if (maxLabelLen <
				    HKDF_Data->label[i].labelLen) {
					LAC_INVALID_PARAM_LOG1(
					    "CpaCyKeyGenHKDFOpData.label[%d].labelLen",
					    i);
					return CPA_STATUS_INVALID_PARAM;
				}

				if (HKDF_Data->label[i].sublabelFlag &
				    ~HKDF_SUB_LABELS_ALL) {
					LAC_INVALID_PARAM_LOG1(
					    "CpaCyKeyGenHKDFOpData.label[%d]."
					    "subLabelFlag",
					    i);
					return CPA_STATUS_INVALID_PARAM;
				}

				/* Calculate the appended subLabel output
				 * lengths and
				 * check that the output buffer that the user
				 * has
				 * supplied is the correct length.
				 */
				uOutputLen += cipherSuiteHKDFHashSizes
				    [cipherSuite][LAC_KEY_HKDF_DIGESTS];
				/* Get mask of subLabel */
				subl_mask = HKDF_Data->label[i].sublabelFlag;

				for (subl_number = 1;
				     subl_number <= LAC_KEY_HKDF_SUBLABELS_NUM;
				     subl_number++) {
					/* Add the used subLabel key lengths */
					if (subl_mask & 1) {
						uOutputLen +=
						    cipherSuiteHKDFHashSizes
							[cipherSuite]
							[subl_number];
					}
					subl_mask >>= 1;
				}
			}
		} break;
		default:
			break;
		}
	} else {
		LAC_INVALID_PARAM_LOG("TLS/SSL operation");
		return CPA_STATUS_INVALID_PARAM;
	}

	/* Maximum secret length for TLS/SSL Key Gen request */
	if (uSecretLen > maxSecretLen) {
		LAC_INVALID_PARAM_LOG("HKFD.secretLen/secret.dataLenInBytes");
		return CPA_STATUS_INVALID_PARAM;
	}

	/* Check for enough space in the flat buffer */
	if (uOutputLen > pGeneratedKeyBuffer->dataLenInBytes) {
		LAC_INVALID_PARAM_LOG("pGeneratedKeyBuffer->dataLenInBytes");
		return CPA_STATUS_INVALID_PARAM;
	}
	return CPA_STATUS_SUCCESS;
}

/**
 *
 */
/**
 * @ingroup LacSymKey
 *      Common Keygen Code for TLS v1.0/1.1, v1.2 and SSL3.
 * @description
 *      Check user parameters and perform the required operation.
 *
 * @param[in] instanceHandle_in          Instance handle.
 * @param[in] pKeyGenCb                  Pointer to callback function to be
 *                                       invoked when the operation is complete.
 *                                       If this is set to a NULL value the
 *                                       function will operate synchronously.
 * @param[in] pCallbackTag               Opaque User Data for this specific
 *                                       call. Will be returned unchanged in the
 *                                       callback.
 * @param[in] pKeyGenOpData              Pointer to a structure containing all
 *                                       the data needed to perform the key
 *                                       generation operation.
 * @param[in]  hashAlgorithm             Specifies the hash algorithm to use.
 *                                       According to RFC5246, this should be
 *                                       "SHA-256 or a stronger standard hash
 *                                       function."
 * @param[out] pGeneratedKeyBuffer       User output buffer.
 * @param[in] cmdId                      Keygen operation to perform.
 */
static CpaStatus
LacSymKey_KeyGenSslTls(const CpaInstanceHandle instanceHandle_in,
		       const CpaCyGenFlatBufCbFunc pKeyGenCb,
		       void *pCallbackTag,
		       const void *pKeyGenOpData,
		       Cpa8U hashAlgorithm,
		       CpaFlatBuffer *pGeneratedKeyBuffer,
		       icp_qat_fw_la_cmd_id_t cmdId)
{
	CpaStatus status = CPA_STATUS_FAIL;
	CpaInstanceHandle instanceHandle = LacKey_GetHandle(instanceHandle_in);

	LAC_CHECK_INSTANCE_HANDLE(instanceHandle);
	SAL_CHECK_INSTANCE_TYPE(instanceHandle,
				(SAL_SERVICE_TYPE_CRYPTO |
				 SAL_SERVICE_TYPE_CRYPTO_SYM));
	SAL_RUNNING_CHECK(instanceHandle);

	status = LacSymKey_CheckParamSslTls(pKeyGenOpData,
					    hashAlgorithm,
					    pGeneratedKeyBuffer,
					    cmdId);
	if (CPA_STATUS_SUCCESS != status)
		return status;
	return LacSymKey_KeyGenSslTls_GenCommon(instanceHandle,
						pKeyGenCb,
						pCallbackTag,
						cmdId,
						LAC_CONST_PTR_CAST(
						    pKeyGenOpData),
						hashAlgorithm,
						pGeneratedKeyBuffer);
}

/**
 * @ingroup LacSymKey
 *      SSL Key Generation Function.
 * @description
 *      This function is used for SSL key generation.  It implements the key
 *      generation function defined in section 6.2.2 of the SSL 3.0
 *      specification as described in
 *      http://www.mozilla.org/projects/security/pki/nss/ssl/draft302.txt.
 *
 *      The input seed is taken as a flat buffer and the generated key is
 *      returned to caller in a flat destination data buffer.
 *
 * @param[in] instanceHandle_in          Instance handle.
 * @param[in] pKeyGenCb                  Pointer to callback function to be
 *                                       invoked when the operation is complete.
 *                                       If this is set to a NULL value the
 *                                       function will operate synchronously.
 * @param[in] pCallbackTag               Opaque User Data for this specific
 *                                       call. Will be returned unchanged in the
 *                                       callback.
 * @param[in] pKeyGenSslOpData           Pointer to a structure containing all
 *                                       the data needed to perform the SSL key
 *                                       generation operation. The client code
 *                                       allocates the memory for this
 *                                       structure. This component takes
 *                                       ownership of the memory until it is
 *                                       returned in the callback.
 * @param[out] pGeneratedKeyBuffer       Caller MUST allocate a sufficient
 *                                       buffer to hold the key generation
 *                                       output. The data pointer SHOULD be
 *                                       aligned on an 8-byte boundary. The
 *                                       length field passed in represents the
 *                                       size of the buffer in bytes. The value
 *                                       that is returned is the size of the
 *                                       result key in bytes.
 *                                       On invocation the callback function
 *                                       will contain this parameter in the
 *                                       pOut parameter.
 *
 * @retval CPA_STATUS_SUCCESS            Function executed successfully.
 * @retval CPA_STATUS_FAIL               Function failed.
 * @retval CPA_STATUS_RETRY              Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM      Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE           Error related to system resources.
 */
CpaStatus
cpaCyKeyGenSsl(const CpaInstanceHandle instanceHandle_in,
	       const CpaCyGenFlatBufCbFunc pKeyGenCb,
	       void *pCallbackTag,
	       const CpaCyKeyGenSslOpData *pKeyGenSslOpData,
	       CpaFlatBuffer *pGeneratedKeyBuffer)
{
	CpaInstanceHandle instanceHandle = NULL;

	if (CPA_INSTANCE_HANDLE_SINGLE == instanceHandle_in) {
		instanceHandle =
		    Lac_GetFirstHandle(SAL_SERVICE_TYPE_CRYPTO_SYM);
	} else {
		instanceHandle = instanceHandle_in;
	}

	return LacSymKey_KeyGenSslTls(instanceHandle,
				      pKeyGenCb,
				      pCallbackTag,
				      LAC_CONST_PTR_CAST(pKeyGenSslOpData),
				      CPA_CY_SYM_HASH_NONE, /* Hash algorithm */
				      pGeneratedKeyBuffer,
				      ICP_QAT_FW_LA_CMD_SSL3_KEY_DERIVE);
}

/**
 * @ingroup LacSymKey
 *      TLS Key Generation Function.
 * @description
 *      This function is used for TLS key generation.  It implements the
 *      TLS PRF (Pseudo Random Function) as defined by RFC2246 (TLS v1.0)
 *      and RFC4346 (TLS v1.1).
 *
 *      The input seed is taken as a flat buffer and the generated key is
 *      returned to caller in a flat destination data buffer.
 *
 * @param[in]  instanceHandle_in         Instance handle.
 * @param[in]  pKeyGenCb                 Pointer to callback function to be
 *                                       invoked when the operation is complete.
 *                                       If this is set to a NULL value the
 *                                       function will operate synchronously.
 * @param[in]  pCallbackTag              Opaque User Data for this specific
 *                                       call. Will be returned unchanged in the
 *                                       callback.
 * @param[in]  pKeyGenTlsOpData          Pointer to a structure containing all
 *                                       the data needed to perform the TLS key
 *                                       generation operation. The client code
 *                                       allocates the memory for this
 *                                       structure. This component takes
 *                                       ownership of the memory until it is
 *                                       returned in the callback.
 * @param[out] pGeneratedKeyBuffer       Caller MUST allocate a sufficient
 *                                       buffer to hold the key generation
 *                                       output. The data pointer SHOULD be
 *                                       aligned on an 8-byte boundary. The
 *                                       length field passed in represents the
 *                                       size of the buffer in bytes. The value
 *                                       that is returned is the size of the
 *                                       result key in bytes.
 *                                       On invocation the callback function
 *                                       will contain this parameter in the
 *                                       pOut parameter.
 *
 * @retval CPA_STATUS_SUCCESS            Function executed successfully.
 * @retval CPA_STATUS_FAIL               Function failed.
 * @retval CPA_STATUS_RETRY              Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM      Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE           Error related to system resources.
 *
 */
CpaStatus
cpaCyKeyGenTls(const CpaInstanceHandle instanceHandle_in,
	       const CpaCyGenFlatBufCbFunc pKeyGenCb,
	       void *pCallbackTag,
	       const CpaCyKeyGenTlsOpData *pKeyGenTlsOpData,
	       CpaFlatBuffer *pGeneratedKeyBuffer)
{
	CpaInstanceHandle instanceHandle = NULL;


	if (CPA_INSTANCE_HANDLE_SINGLE == instanceHandle_in) {
		instanceHandle =
		    Lac_GetFirstHandle(SAL_SERVICE_TYPE_CRYPTO_SYM);
	} else {
		instanceHandle = instanceHandle_in;
	}

	return LacSymKey_KeyGenSslTls(instanceHandle,
				      pKeyGenCb,
				      pCallbackTag,
				      LAC_CONST_PTR_CAST(pKeyGenTlsOpData),
				      CPA_CY_SYM_HASH_NONE, /* Hash algorithm */
				      pGeneratedKeyBuffer,
				      ICP_QAT_FW_LA_CMD_TLS_V1_1_KEY_DERIVE);
}

/**
 * @ingroup LacSymKey
 * @description
 *      This function is used for TLS key generation.  It implements the
 *      TLS PRF (Pseudo Random Function) as defined by RFC5246 (TLS v1.2).
 *
 *      The input seed is taken as a flat buffer and the generated key is
 *      returned to caller in a flat destination data buffer.
 *
 * @param[in]  instanceHandle_in         Instance handle.
 * @param[in]  pKeyGenCb                 Pointer to callback function to be
 *                                       invoked when the operation is complete.
 *                                       If this is set to a NULL value the
 *                                       function will operate synchronously.
 * @param[in]  pCallbackTag              Opaque User Data for this specific
 *                                       call. Will be returned unchanged in the
 *                                       callback.
 * @param[in]  pKeyGenTlsOpData          Pointer to a structure containing all
 *                                       the data needed to perform the TLS key
 *                                       generation operation. The client code
 *                                       allocates the memory for this
 *                                       structure. This component takes
 *                                       ownership of the memory until it is
 *                                       returned in the callback.
 * @param[in]  hashAlgorithm             Specifies the hash algorithm to use.
 *                                       According to RFC5246, this should be
 *                                       "SHA-256 or a stronger standard hash
 *                                       function."
 * @param[out] pGeneratedKeyBuffer       Caller MUST allocate a sufficient
 *                                       buffer to hold the key generation
 *                                       output. The data pointer SHOULD be
 *                                       aligned on an 8-byte boundary. The
 *                                       length field passed in represents the
 *                                       size of the buffer in bytes. The value
 *                                       that is returned is the size of the
 *                                       result key in bytes.
 *                                       On invocation the callback function
 *                                       will contain this parameter in the
 *                                       pOut parameter.
 *
 * @retval CPA_STATUS_SUCCESS            Function executed successfully.
 * @retval CPA_STATUS_FAIL               Function failed.
 * @retval CPA_STATUS_RETRY              Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM      Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE           Error related to system resources.
 */
CpaStatus
cpaCyKeyGenTls2(const CpaInstanceHandle instanceHandle_in,
		const CpaCyGenFlatBufCbFunc pKeyGenCb,
		void *pCallbackTag,
		const CpaCyKeyGenTlsOpData *pKeyGenTlsOpData,
		CpaCySymHashAlgorithm hashAlgorithm,
		CpaFlatBuffer *pGeneratedKeyBuffer)
{
	CpaInstanceHandle instanceHandle = NULL;


	if (CPA_INSTANCE_HANDLE_SINGLE == instanceHandle_in) {
		instanceHandle =
		    Lac_GetFirstHandle(SAL_SERVICE_TYPE_CRYPTO_SYM);
	} else {
		instanceHandle = instanceHandle_in;
	}

	return LacSymKey_KeyGenSslTls(instanceHandle,
				      pKeyGenCb,
				      pCallbackTag,
				      LAC_CONST_PTR_CAST(pKeyGenTlsOpData),
				      hashAlgorithm,
				      pGeneratedKeyBuffer,
				      ICP_QAT_FW_LA_CMD_TLS_V1_2_KEY_DERIVE);
}

/**
 * @ingroup LacSymKey
 * @description
 *      This function is used for TLS1.3  HKDF key generation.  It implements
 *      the "extract-then-expand" paradigm as defined by RFC 5869.
 *
 *      The input seed/secret/info is taken as a flat buffer and the generated
 *      key(s)/labels are returned to caller in a flat data buffer.
 *
 * @param[in]  instanceHandle_in         Instance handle.
 * @param[in]  pKeyGenCb                 Pointer to callback function to be
 *                                       invoked when the operation is complete.
 *                                       If this is set to a NULL value the
 *                                       function will operate synchronously.
 * @param[in]  pCallbackTag              Opaque User Data for this specific
 *                                       call. Will be returned unchanged in the
 *                                       callback.
 * @param[in]  pKeyGenTlsOpData          Pointer to a structure containing
 *                                       the data needed to perform the HKDF key
 *                                       generation operation.
 *                                       The client code allocates the memory
 *                                       for this structure as contiguous
 *                                       pinned memory.
 *                                       This component takes ownership of the
 *                                       memory until it is returned in the
 *                                       callback.
 * @param[in]  hashAlgorithm             Specifies the hash algorithm to use.
 *                                       According to RFC5246, this should be
 *                                       "SHA-256 or a stronger standard hash
 *                                       function."
 * @param[out] pGeneratedKeyBuffer       Caller MUST allocate a sufficient
 *                                       buffer to hold the key generation
 *                                       output. The data pointer SHOULD be
 *                                       aligned on an 8-byte boundary. The
 *                                       length field passed in represents the
 *                                       size of the buffer in bytes. The value
 *                                       that is returned is the size of the
 *                                       result key in bytes.
 *                                       On invocation the callback function
 *                                       will contain this parameter in the
 *                                       pOut parameter.
 *
 * @retval CPA_STATUS_SUCCESS            Function executed successfully.
 * @retval CPA_STATUS_FAIL               Function failed.
 * @retval CPA_STATUS_RETRY              Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM      Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE           Error related to system resources.
 */
CpaStatus
cpaCyKeyGenTls3(const CpaInstanceHandle instanceHandle_in,
		const CpaCyGenFlatBufCbFunc pKeyGenCb,
		void *pCallbackTag,
		const CpaCyKeyGenHKDFOpData *pKeyGenTlsOpData,
		CpaCyKeyHKDFCipherSuite cipherSuite,
		CpaFlatBuffer *pGeneratedKeyBuffer)
{

	LAC_CHECK_NULL_PARAM(pKeyGenTlsOpData);
	switch (pKeyGenTlsOpData->hkdfKeyOp) {
	case CPA_CY_HKDF_KEY_EXTRACT: /* Fall through */
	case CPA_CY_HKDF_KEY_EXPAND:
	case CPA_CY_HKDF_KEY_EXTRACT_EXPAND:
	case CPA_CY_HKDF_KEY_EXPAND_LABEL:
	case CPA_CY_HKDF_KEY_EXTRACT_EXPAND_LABEL:
		break;
	default:
		LAC_INVALID_PARAM_LOG("HKDF operation not supported");
		return CPA_STATUS_INVALID_PARAM;
	}


	return LacSymKey_KeyGenSslTls(instanceHandle_in,
				      pKeyGenCb,
				      pCallbackTag,
				      LAC_CONST_PTR_CAST(pKeyGenTlsOpData),
				      cipherSuite,
				      pGeneratedKeyBuffer,
				      (icp_qat_fw_la_cmd_id_t)
					  pKeyGenTlsOpData->hkdfKeyOp);
}

/*
 * LacSymKey_Init
 */
CpaStatus
LacSymKey_Init(CpaInstanceHandle instanceHandle_in)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	CpaInstanceHandle instanceHandle = LacKey_GetHandle(instanceHandle_in);
	sal_crypto_service_t *pService = NULL;

	LAC_CHECK_INSTANCE_HANDLE(instanceHandle);

	pService = (sal_crypto_service_t *)instanceHandle;

	pService->pLacKeyStats =
	    LAC_OS_MALLOC(LAC_KEY_NUM_STATS * sizeof(QatUtilsAtomic));

	if (NULL != pService->pLacKeyStats) {
		LAC_OS_BZERO((void *)pService->pLacKeyStats,
			     LAC_KEY_NUM_STATS * sizeof(QatUtilsAtomic));

		status = LAC_OS_CAMALLOC(&pService->pSslLabel,
					 ICP_QAT_FW_LA_SSL_LABEL_LEN_MAX,
					 LAC_8BYTE_ALIGNMENT,
					 pService->nodeAffinity);
	} else {
		status = CPA_STATUS_RESOURCE;
	}

	if (CPA_STATUS_SUCCESS == status) {
		Cpa32U i = 0;
		Cpa32U offset = 0;

		/* Initialise SSL label ABBCCC..... */
		for (i = 0; i < ICP_QAT_FW_LA_SSL_ITERATES_LEN_MAX; i++) {
			memset(pService->pSslLabel + offset, 'A' + i, i + 1);
			offset += (i + 1);
		}

		/* Allocate memory for TLS labels */
		status = LAC_OS_CAMALLOC(&pService->pTlsLabel,
					 sizeof(lac_sym_key_tls_labels_t),
					 LAC_8BYTE_ALIGNMENT,
					 pService->nodeAffinity);
	}

	if (CPA_STATUS_SUCCESS == status) {
		/* Allocate memory for HKDF sub_labels */
		status =
		    LAC_OS_CAMALLOC(&pService->pTlsHKDFSubLabel,
				    sizeof(lac_sym_key_tls_hkdf_sub_labels_t),
				    LAC_8BYTE_ALIGNMENT,
				    pService->nodeAffinity);
	}

	if (CPA_STATUS_SUCCESS == status) {
		LAC_OS_BZERO(pService->pTlsLabel,
			     sizeof(lac_sym_key_tls_labels_t));

		/* Copy the TLS v1.2 labels into the dynamically allocated
		 * structure */
		memcpy(pService->pTlsLabel->masterSecret,
		       LAC_SYM_KEY_TLS_MASTER_SECRET_LABEL,
		       sizeof(LAC_SYM_KEY_TLS_MASTER_SECRET_LABEL) - 1);

		memcpy(pService->pTlsLabel->keyMaterial,
		       LAC_SYM_KEY_TLS_KEY_MATERIAL_LABEL,
		       sizeof(LAC_SYM_KEY_TLS_KEY_MATERIAL_LABEL) - 1);

		memcpy(pService->pTlsLabel->clientFinished,
		       LAC_SYM_KEY_TLS_CLIENT_FIN_LABEL,
		       sizeof(LAC_SYM_KEY_TLS_CLIENT_FIN_LABEL) - 1);

		memcpy(pService->pTlsLabel->serverFinished,
		       LAC_SYM_KEY_TLS_SERVER_FIN_LABEL,
		       sizeof(LAC_SYM_KEY_TLS_SERVER_FIN_LABEL) - 1);

		LAC_OS_BZERO(pService->pTlsHKDFSubLabel,
			     sizeof(lac_sym_key_tls_hkdf_sub_labels_t));

		/* Copy the TLS v1.3 subLabels into the dynamically allocated
		 * struct */
		/* KEY SHA-256 */
		memcpy(&pService->pTlsHKDFSubLabel->keySublabel256,
		       &key256,
		       HKDF_SUB_LABEL_KEY_LENGTH);
		pService->pTlsHKDFSubLabel->keySublabel256.labelLen =
		    HKDF_SUB_LABEL_KEY_LENGTH;
		pService->pTlsHKDFSubLabel->keySublabel256.sublabelFlag = 1
		    << QAT_FW_HKDF_INNER_SUBLABEL_16_BYTE_OKM_BITPOS;
		/* KEY SHA-384 */
		memcpy(&pService->pTlsHKDFSubLabel->keySublabel384,
		       &key384,
		       HKDF_SUB_LABEL_KEY_LENGTH);
		pService->pTlsHKDFSubLabel->keySublabel384.labelLen =
		    HKDF_SUB_LABEL_KEY_LENGTH;
		pService->pTlsHKDFSubLabel->keySublabel384.sublabelFlag = 1
		    << QAT_FW_HKDF_INNER_SUBLABEL_32_BYTE_OKM_BITPOS;
		/* KEY CHACHAPOLY */
		memcpy(&pService->pTlsHKDFSubLabel->keySublabelChaChaPoly,
		       &keyChaChaPoly,
		       HKDF_SUB_LABEL_KEY_LENGTH);
		pService->pTlsHKDFSubLabel->keySublabelChaChaPoly.labelLen =
		    HKDF_SUB_LABEL_KEY_LENGTH;
		pService->pTlsHKDFSubLabel->keySublabelChaChaPoly.sublabelFlag =
		    1 << QAT_FW_HKDF_INNER_SUBLABEL_32_BYTE_OKM_BITPOS;
		/* IV SHA-256 */
		memcpy(&pService->pTlsHKDFSubLabel->ivSublabel256,
		       &iv256,
		       HKDF_SUB_LABEL_IV_LENGTH);
		pService->pTlsHKDFSubLabel->ivSublabel256.labelLen =
		    HKDF_SUB_LABEL_IV_LENGTH;
		pService->pTlsHKDFSubLabel->ivSublabel256.sublabelFlag = 1
		    << QAT_FW_HKDF_INNER_SUBLABEL_12_BYTE_OKM_BITPOS;
		/* IV SHA-384 */
		memcpy(&pService->pTlsHKDFSubLabel->ivSublabel384,
		       &iv384,
		       HKDF_SUB_LABEL_IV_LENGTH);
		pService->pTlsHKDFSubLabel->ivSublabel384.labelLen =
		    HKDF_SUB_LABEL_IV_LENGTH;
		pService->pTlsHKDFSubLabel->ivSublabel384.sublabelFlag = 1
		    << QAT_FW_HKDF_INNER_SUBLABEL_12_BYTE_OKM_BITPOS;
		/* IV CHACHAPOLY */
		memcpy(&pService->pTlsHKDFSubLabel->ivSublabelChaChaPoly,
		       &iv256,
		       HKDF_SUB_LABEL_IV_LENGTH);
		pService->pTlsHKDFSubLabel->ivSublabelChaChaPoly.labelLen =
		    HKDF_SUB_LABEL_IV_LENGTH;
		pService->pTlsHKDFSubLabel->ivSublabelChaChaPoly.sublabelFlag =
		    1 << QAT_FW_HKDF_INNER_SUBLABEL_12_BYTE_OKM_BITPOS;
		/* RESUMPTION SHA-256 */
		memcpy(&pService->pTlsHKDFSubLabel->resumptionSublabel256,
		       &resumption256,
		       HKDF_SUB_LABEL_RESUMPTION_LENGTH);
		pService->pTlsHKDFSubLabel->resumptionSublabel256.labelLen =
		    HKDF_SUB_LABEL_RESUMPTION_LENGTH;
		/* RESUMPTION SHA-384 */
		memcpy(&pService->pTlsHKDFSubLabel->resumptionSublabel384,
		       &resumption384,
		       HKDF_SUB_LABEL_RESUMPTION_LENGTH);
		pService->pTlsHKDFSubLabel->resumptionSublabel384.labelLen =
		    HKDF_SUB_LABEL_RESUMPTION_LENGTH;
		/* RESUMPTION CHACHAPOLY */
		memcpy(
		    &pService->pTlsHKDFSubLabel->resumptionSublabelChaChaPoly,
		    &resumption256,
		    HKDF_SUB_LABEL_RESUMPTION_LENGTH);
		pService->pTlsHKDFSubLabel->resumptionSublabelChaChaPoly
		    .labelLen = HKDF_SUB_LABEL_RESUMPTION_LENGTH;
		/* FINISHED SHA-256 */
		memcpy(&pService->pTlsHKDFSubLabel->finishedSublabel256,
		       &finished256,
		       HKDF_SUB_LABEL_FINISHED_LENGTH);
		pService->pTlsHKDFSubLabel->finishedSublabel256.labelLen =
		    HKDF_SUB_LABEL_FINISHED_LENGTH;
		/* FINISHED SHA-384 */
		memcpy(&pService->pTlsHKDFSubLabel->finishedSublabel384,
		       &finished384,
		       HKDF_SUB_LABEL_FINISHED_LENGTH);
		pService->pTlsHKDFSubLabel->finishedSublabel384.labelLen =
		    HKDF_SUB_LABEL_FINISHED_LENGTH;
		/* FINISHED CHACHAPOLY */
		memcpy(&pService->pTlsHKDFSubLabel->finishedSublabelChaChaPoly,
		       &finished256,
		       HKDF_SUB_LABEL_FINISHED_LENGTH);
		pService->pTlsHKDFSubLabel->finishedSublabelChaChaPoly
		    .labelLen = HKDF_SUB_LABEL_FINISHED_LENGTH;

		/* Set physical address of sublabels */
		pService->pTlsHKDFSubLabel->sublabelPhysAddr256 =
		    LAC_OS_VIRT_TO_PHYS_INTERNAL(
			&pService->pTlsHKDFSubLabel->keySublabel256);
		pService->pTlsHKDFSubLabel->sublabelPhysAddr384 =
		    LAC_OS_VIRT_TO_PHYS_INTERNAL(
			&pService->pTlsHKDFSubLabel->keySublabel384);
		pService->pTlsHKDFSubLabel->sublabelPhysAddrChaChaPoly =
		    LAC_OS_VIRT_TO_PHYS_INTERNAL(
			&pService->pTlsHKDFSubLabel->keySublabelChaChaPoly);

		/* Register request handlers */
		LacSymQat_RespHandlerRegister(ICP_QAT_FW_LA_CMD_SSL3_KEY_DERIVE,
					      LacSymKey_SslTlsHandleResponse);

		LacSymQat_RespHandlerRegister(
		    ICP_QAT_FW_LA_CMD_TLS_V1_1_KEY_DERIVE,
		    LacSymKey_SslTlsHandleResponse);

		LacSymQat_RespHandlerRegister(
		    ICP_QAT_FW_LA_CMD_TLS_V1_2_KEY_DERIVE,
		    LacSymKey_SslTlsHandleResponse);

		LacSymQat_RespHandlerRegister(ICP_QAT_FW_LA_CMD_HKDF_EXTRACT,
					      LacSymKey_SslTlsHandleResponse);

		LacSymQat_RespHandlerRegister(ICP_QAT_FW_LA_CMD_HKDF_EXPAND,
					      LacSymKey_SslTlsHandleResponse);

		LacSymQat_RespHandlerRegister(
		    ICP_QAT_FW_LA_CMD_HKDF_EXTRACT_AND_EXPAND,
		    LacSymKey_SslTlsHandleResponse);

		LacSymQat_RespHandlerRegister(
		    ICP_QAT_FW_LA_CMD_HKDF_EXPAND_LABEL,
		    LacSymKey_SslTlsHandleResponse);

		LacSymQat_RespHandlerRegister(
		    ICP_QAT_FW_LA_CMD_HKDF_EXTRACT_AND_EXPAND_LABEL,
		    LacSymKey_SslTlsHandleResponse);

		LacSymQat_RespHandlerRegister(ICP_QAT_FW_LA_CMD_MGF1,
					      LacSymKey_MgfHandleResponse);
	}

	if (CPA_STATUS_SUCCESS != status) {
		LAC_OS_FREE(pService->pLacKeyStats);
		LAC_OS_CAFREE(pService->pSslLabel);
		LAC_OS_CAFREE(pService->pTlsLabel);
		LAC_OS_CAFREE(pService->pTlsHKDFSubLabel);
	}

	return status;
}

/*
 * LacSymKey_Shutdown
 */
CpaStatus
LacSymKey_Shutdown(CpaInstanceHandle instanceHandle_in)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	CpaInstanceHandle instanceHandle = LacKey_GetHandle(instanceHandle_in);
	sal_crypto_service_t *pService = NULL;

	LAC_CHECK_INSTANCE_HANDLE(instanceHandle);

	pService = (sal_crypto_service_t *)instanceHandle;

	if (NULL != pService->pLacKeyStats) {
		LAC_OS_FREE(pService->pLacKeyStats);
	}

	LAC_OS_CAFREE(pService->pSslLabel);
	LAC_OS_CAFREE(pService->pTlsLabel);
	LAC_OS_CAFREE(pService->pTlsHKDFSubLabel);

	return status;
}
