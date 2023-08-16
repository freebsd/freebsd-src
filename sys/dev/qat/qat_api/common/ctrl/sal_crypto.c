/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */

/**
 ***************************************************************************
 * @file sal_crypto.c     Instance handling functions for crypto
 *
 * @ingroup SalCtrl
 *
 ***************************************************************************/

/*
*******************************************************************************
* Include public/global header files
*******************************************************************************
*/

/* QAT-API includes */
#include "cpa.h"
#include "cpa_types.h"
#include "cpa_cy_common.h"
#include "cpa_cy_im.h"
#include "cpa_cy_key.h"
#include "cpa_cy_sym.h"

#include "qat_utils.h"

/* ADF includes */
#include "icp_adf_init.h"
#include "icp_adf_transport.h"
#include "icp_accel_devices.h"
#include "icp_adf_cfg.h"
#include "icp_adf_accel_mgr.h"
#include "icp_adf_poll.h"
#include "icp_adf_debug.h"

/* SAL includes */
#include "lac_log.h"
#include "lac_mem.h"
#include "lac_mem_pools.h"
#include "sal_statistics.h"
#include "lac_common.h"
#include "lac_list.h"
#include "lac_hooks.h"
#include "lac_sym_qat_hash_defs_lookup.h"
#include "lac_sym.h"
#include "lac_sym_key.h"
#include "lac_sym_hash.h"
#include "lac_sym_cb.h"
#include "lac_sym_stats.h"
#include "lac_sal_types_crypto.h"
#include "lac_sal.h"
#include "lac_sal_ctrl.h"
#include "sal_string_parse.h"
#include "sal_service_state.h"
#include "icp_sal_poll.h"
#include "lac_sync.h"
#include "lac_sym_qat.h"
#include "icp_sal_versions.h"
#include "icp_sal_user.h"
#include "sal_hw_gen.h"

#define HMAC_MODE_1 1
#define HMAC_MODE_2 2
#define TH_CY_RX_0 0
#define TH_CY_RX_1 1
#define MAX_CY_RX_RINGS 2

#define DOUBLE_INCR 2

#define TH_SINGLE_RX 0
#define NUM_CRYPTO_SYM_RX_RINGS 1
#define NUM_CRYPTO_ASYM_RX_RINGS 1
#define NUM_CRYPTO_NRBG_RX_RINGS 1

static CpaInstanceHandle
Lac_CryptoGetFirstHandle(void)
{
	CpaInstanceHandle instHandle;
	instHandle = Lac_GetFirstHandle(SAL_SERVICE_TYPE_CRYPTO);
	if (!instHandle) {
		instHandle = Lac_GetFirstHandle(SAL_SERVICE_TYPE_CRYPTO_SYM);
		if (!instHandle) {
			instHandle =
			    Lac_GetFirstHandle(SAL_SERVICE_TYPE_CRYPTO_ASYM);
		}
	}
	return instHandle;
}


/* Function to release the sym handles. */
static CpaStatus
SalCtrl_SymReleaseTransHandle(sal_service_t *service)
{

	CpaStatus status = CPA_STATUS_SUCCESS;
	CpaStatus ret_status = CPA_STATUS_SUCCESS;
	sal_crypto_service_t *pCryptoService = (sal_crypto_service_t *)service;

	if (NULL != pCryptoService->trans_handle_sym_tx) {
		status = icp_adf_transReleaseHandle(
		    pCryptoService->trans_handle_sym_tx);
		if (CPA_STATUS_SUCCESS != status) {
			ret_status = status;
		}
	}
	if (NULL != pCryptoService->trans_handle_sym_rx) {
		status = icp_adf_transReleaseHandle(
		    pCryptoService->trans_handle_sym_rx);
		if (CPA_STATUS_SUCCESS != status) {
			ret_status = status;
		}
	}

	return ret_status;
}


/*
 * @ingroup sal_crypto
 *     Frees resources (memory and transhandles) if allocated
 *
 * @param[in]  pCryptoService       Pointer to sym service instance
 * @retval                          SUCCESS if transhandles released
 *                                  successfully.
*/
static CpaStatus
SalCtrl_SymFreeResources(sal_crypto_service_t *pCryptoService)
{

	CpaStatus status = CPA_STATUS_SUCCESS;

	/* Free memory pools if not NULL */
	Lac_MemPoolDestroy(pCryptoService->lac_sym_cookie_pool);

	/* Free misc memory if allocated */
	/* Frees memory allocated for Hmac precomputes */
	LacSymHash_HmacPrecompShutdown(pCryptoService);
	/* Free memory allocated for key labels
	   Also clears key stats  */
	LacSymKey_Shutdown(pCryptoService);
	/* Free hash lookup table if allocated */
	if (NULL != pCryptoService->pLacHashLookupDefs) {
		LAC_OS_FREE(pCryptoService->pLacHashLookupDefs);
	}

	/* Free statistics */
	LacSym_StatsFree(pCryptoService);

	/* Free transport handles */
	status = SalCtrl_SymReleaseTransHandle((sal_service_t *)pCryptoService);
	return status;
}


/**
 ***********************************************************************
 * @ingroup SalCtrl
 *   This macro verifies that the status is _SUCCESS
 *   If status is not _SUCCESS then Sym Instance resources are
 *   freed before the function returns the error
 *
 * @param[in] status    status we are checking
 *
 * @return void         status is ok (CPA_STATUS_SUCCESS)
 * @return status       The value in the status parameter is an error one
 *
 ****************************************************************************/
#define LAC_CHECK_STATUS_SYM_INIT(status)                                      \
	do {                                                                   \
		if (CPA_STATUS_SUCCESS != status) {                            \
			SalCtrl_SymFreeResources(pCryptoService);              \
			return status;                                         \
		}                                                              \
	} while (0)


/* Function that creates the Sym Handles. */
static CpaStatus
SalCtrl_SymCreateTransHandle(icp_accel_dev_t *device,
			     sal_service_t *service,
			     Cpa32U numSymRequests,
			     char *section)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	char temp_string[SAL_CFG_MAX_VAL_LEN_IN_BYTES] = { 0 };
	sal_crypto_service_t *pCryptoService = (sal_crypto_service_t *)service;
	icp_resp_deliv_method rx_resp_type = ICP_RESP_TYPE_IRQ;
	Cpa32U msgSize = 0;

	if (SAL_RESP_POLL_CFG_FILE == pCryptoService->isPolled) {
		rx_resp_type = ICP_RESP_TYPE_POLL;
	}

	if (CPA_FALSE == pCryptoService->generic_service_info.is_dyn) {
		section = icpGetProcessName();
	}

	/* Parse Sym ring details */
	status =
	    Sal_StringParsing("Cy",
			      pCryptoService->generic_service_info.instance,
			      "RingSymTx",
			      temp_string);

	/* Need to free resources in case not _SUCCESS from here */
	LAC_CHECK_STATUS_SYM_INIT(status);

	msgSize = LAC_QAT_SYM_REQ_SZ_LW * LAC_LONG_WORD_IN_BYTES;
	status =
	    icp_adf_transCreateHandle(device,
				      ICP_TRANS_TYPE_ETR,
				      section,
				      pCryptoService->acceleratorNum,
				      pCryptoService->bankNumSym,
				      temp_string,
				      lac_getRingType(SAL_RING_TYPE_A_SYM_HI),
				      NULL,
				      ICP_RESP_TYPE_NONE,
				      numSymRequests,
				      msgSize,
				      (icp_comms_trans_handle *)&(
					  pCryptoService->trans_handle_sym_tx));
	LAC_CHECK_STATUS_SYM_INIT(status);

	status =
	    Sal_StringParsing("Cy",
			      pCryptoService->generic_service_info.instance,
			      "RingSymRx",
			      temp_string);
	LAC_CHECK_STATUS_SYM_INIT(status);

	msgSize = LAC_QAT_SYM_RESP_SZ_LW * LAC_LONG_WORD_IN_BYTES;
	status = icp_adf_transCreateHandle(
	    device,
	    ICP_TRANS_TYPE_ETR,
	    section,
	    pCryptoService->acceleratorNum,
	    pCryptoService->bankNumSym,
	    temp_string,
	    lac_getRingType(SAL_RING_TYPE_NONE),
	    (icp_trans_callback)LacSymQat_SymRespHandler,
	    rx_resp_type,
	    numSymRequests,
	    msgSize,
	    (icp_comms_trans_handle *)&(pCryptoService->trans_handle_sym_rx));
	LAC_CHECK_STATUS_SYM_INIT(status);

	return status;
}

static int
SalCtrl_CryptoDebug(void *private_data, char *data, int size, int offset)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	Cpa32U len = 0;
	sal_crypto_service_t *pCryptoService =
	    (sal_crypto_service_t *)private_data;

	switch (offset) {
	case SAL_STATS_SYM: {
		CpaCySymStats64 symStats = { 0 };
		if (CPA_TRUE !=
		    pCryptoService->generic_service_info.stats
			->bSymStatsEnabled) {
			break;
		}
		status = cpaCySymQueryStats64(pCryptoService, &symStats);
		if (status != CPA_STATUS_SUCCESS) {
			LAC_LOG_ERROR("cpaCySymQueryStats64 returned error\n");
			return 0;
		}

		/* Engine Info */
		len += snprintf(
		    data + len,
		    size - len,
		    SEPARATOR BORDER
		    " Statistics for Instance %24s |\n" BORDER
		    " Symmetric Stats                                  " BORDER
		    "\n" SEPARATOR,
		    pCryptoService->debug_file->name);

		/* Session Info */
		len += snprintf(
		    data + len,
		    size - len,
		    BORDER " Sessions Initialized:           %16llu " BORDER
			   "\n" BORDER
			   " Sessions Removed:               %16llu " BORDER
			   "\n" BORDER
			   " Session Errors:                 %16llu " BORDER
			   "\n" SEPARATOR,
		    (long long unsigned int)symStats.numSessionsInitialized,
		    (long long unsigned int)symStats.numSessionsRemoved,
		    (long long unsigned int)symStats.numSessionErrors);

		/* Session info */
		len += snprintf(
		    data + len,
		    size - len,
		    BORDER " Symmetric Requests:             %16llu " BORDER
			   "\n" BORDER
			   " Symmetric Request Errors:       %16llu " BORDER
			   "\n" BORDER
			   " Symmetric Completed:            %16llu " BORDER
			   "\n" BORDER
			   " Symmetric Completed Errors:     %16llu " BORDER
			   "\n" BORDER
			   " Symmetric Verify Failures:      %16llu " BORDER
			   "\n",
		    (long long unsigned int)symStats.numSymOpRequests,
		    (long long unsigned int)symStats.numSymOpRequestErrors,
		    (long long unsigned int)symStats.numSymOpCompleted,
		    (long long unsigned int)symStats.numSymOpCompletedErrors,
		    (long long unsigned int)symStats.numSymOpVerifyFailures);
		break;
	}
	default: {
		len += snprintf(data + len, size - len, SEPARATOR);
		return 0;
	}
	}
	return ++offset;
}


static CpaStatus
SalCtrl_SymInit(icp_accel_dev_t *device, sal_service_t *service)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	Cpa32U qatHmacMode = 0;
	Cpa32U numSymConcurrentReq = 0;
	char adfGetParam[ADF_CFG_MAX_VAL_LEN_IN_BYTES] = { 0 };
	char temp_string[SAL_CFG_MAX_VAL_LEN_IN_BYTES] = { 0 };
	sal_crypto_service_t *pCryptoService = (sal_crypto_service_t *)service;
	char *section = DYN_SEC;

	/*Instance may not in the DYN section*/
	if (CPA_FALSE == pCryptoService->generic_service_info.is_dyn) {
		section = icpGetProcessName();
	}


	/* Register callbacks for the symmetric services
	* (Hash, Cipher, Algorithm-Chaining) (returns void)*/
	LacSymCb_CallbacksRegister();

	qatHmacMode = (Cpa32U)Sal_Strtoul(adfGetParam, NULL, SAL_CFG_BASE_DEC);
	switch (qatHmacMode) {
	case HMAC_MODE_1:
		pCryptoService->qatHmacMode = ICP_QAT_HW_AUTH_MODE1;
		break;
	case HMAC_MODE_2:
		pCryptoService->qatHmacMode = ICP_QAT_HW_AUTH_MODE2;
		break;
	default:
		pCryptoService->qatHmacMode = ICP_QAT_HW_AUTH_MODE1;
		break;
	}

	/* Get num concurrent requests from config file */
	status =
	    Sal_StringParsing("Cy",
			      pCryptoService->generic_service_info.instance,
			      "NumConcurrentSymRequests",
			      temp_string);
	LAC_CHECK_STATUS(status);
	status =
	    icp_adf_cfgGetParamValue(device, section, temp_string, adfGetParam);
	if (CPA_STATUS_SUCCESS != status) {
		QAT_UTILS_LOG("Failed to get %s from configuration file\n",
			      temp_string);
		return status;
	}

	numSymConcurrentReq =
	    (Cpa32U)Sal_Strtoul(adfGetParam, NULL, SAL_CFG_BASE_DEC);
	if (CPA_STATUS_FAIL == validateConcurrRequest(numSymConcurrentReq)) {
		LAC_LOG_ERROR("Invalid NumConcurrentSymRequests, valid "
			      "values {64, 128, 256, ... 32768, 65536}");
		return CPA_STATUS_FAIL;
	}

	/* ADF does not allow us to completely fill the ring for batch requests
	 */
	pCryptoService->maxNumSymReqBatch =
	    (numSymConcurrentReq - SAL_BATCH_SUBMIT_FREE_SPACE);

	/* Create transport handles */
	status = SalCtrl_SymCreateTransHandle(device,
					      service,
					      numSymConcurrentReq,
					      section);
	LAC_CHECK_STATUS(status);

	/* Allocates memory pools */

	/* Create and initialise symmetric cookie memory pool */
	pCryptoService->lac_sym_cookie_pool = LAC_MEM_POOL_INIT_POOL_ID;
	status =
	    Sal_StringParsing("Cy",
			      pCryptoService->generic_service_info.instance,
			      "SymPool",
			      temp_string);
	LAC_CHECK_STATUS_SYM_INIT(status);
	/* Note we need twice (i.e. <<1) the number of sym cookies to
	   support sym ring pairs (and some, for partials) */
	status =
	    Lac_MemPoolCreate(&pCryptoService->lac_sym_cookie_pool,
			      temp_string,
			      ((numSymConcurrentReq + numSymConcurrentReq + 1)
			       << 1),
			      sizeof(lac_sym_cookie_t),
			      LAC_64BYTE_ALIGNMENT,
			      CPA_FALSE,
			      pCryptoService->nodeAffinity);
	LAC_CHECK_STATUS_SYM_INIT(status);
	/* For all sym cookies fill out the physical address of data that
	   will be set to QAT */
	Lac_MemPoolInitSymCookiesPhyAddr(pCryptoService->lac_sym_cookie_pool);

	/* Clear stats */
	/* Clears Key stats and allocate memory of SSL and TLS labels
	    These labels are initialised to standard values */
	status = LacSymKey_Init(pCryptoService);
	LAC_CHECK_STATUS_SYM_INIT(status);

	/* Initialises the hash lookup table*/
	status = LacSymQat_Init(pCryptoService);
	LAC_CHECK_STATUS_SYM_INIT(status);

	/* Fills out content descriptor for precomputes and registers the
	   hash precompute callback */
	status = LacSymHash_HmacPrecompInit(pCryptoService);
	LAC_CHECK_STATUS_SYM_INIT(status);

	/* Init the Sym stats */
	status = LacSym_StatsInit(pCryptoService);
	LAC_CHECK_STATUS_SYM_INIT(status);

	return status;
}

static void
SalCtrl_DebugShutdown(icp_accel_dev_t *device, sal_service_t *service)
{
	sal_crypto_service_t *pCryptoService = (sal_crypto_service_t *)service;
	sal_statistics_collection_t *pStatsCollection =
	    (sal_statistics_collection_t *)device->pQatStats;

	if (CPA_TRUE == pStatsCollection->bStatsEnabled) {
		/* Clean stats */
		if (NULL != pCryptoService->debug_file) {
			icp_adf_debugRemoveFile(pCryptoService->debug_file);
			LAC_OS_FREE(pCryptoService->debug_file->name);
			LAC_OS_FREE(pCryptoService->debug_file);
			pCryptoService->debug_file = NULL;
		}
	}
	pCryptoService->generic_service_info.stats = NULL;
}

static CpaStatus
SalCtrl_DebugInit(icp_accel_dev_t *device, sal_service_t *service)
{
	char adfGetParam[ADF_CFG_MAX_VAL_LEN_IN_BYTES] = { 0 };
	char temp_string[SAL_CFG_MAX_VAL_LEN_IN_BYTES] = { 0 };
	char *instance_name = NULL;
	sal_crypto_service_t *pCryptoService = (sal_crypto_service_t *)service;
	sal_statistics_collection_t *pStatsCollection =
	    (sal_statistics_collection_t *)device->pQatStats;
	CpaStatus status = CPA_STATUS_SUCCESS;
	char *section = DYN_SEC;

	/*Instance may not in the DYN section*/
	if (CPA_FALSE == pCryptoService->generic_service_info.is_dyn) {
		section = icpGetProcessName();
	}

	if (CPA_TRUE == pStatsCollection->bStatsEnabled) {
		/* Get instance name for stats */
		instance_name = LAC_OS_MALLOC(ADF_CFG_MAX_VAL_LEN_IN_BYTES);
		if (NULL == instance_name) {
			return CPA_STATUS_RESOURCE;
		}

		status = Sal_StringParsing(
		    "Cy",
		    pCryptoService->generic_service_info.instance,
		    "Name",
		    temp_string);
		if (CPA_STATUS_SUCCESS != status) {
			LAC_OS_FREE(instance_name);
			return status;
		}
		status = icp_adf_cfgGetParamValue(device,
						  section,
						  temp_string,
						  adfGetParam);
		if (CPA_STATUS_SUCCESS != status) {
			QAT_UTILS_LOG(
			    "Failed to get %s from configuration file\n",
			    temp_string);
			LAC_OS_FREE(instance_name);
			return status;
		}
		snprintf(instance_name,
			 ADF_CFG_MAX_VAL_LEN_IN_BYTES,
			 "%s",
			 adfGetParam);

		pCryptoService->debug_file =
		    LAC_OS_MALLOC(sizeof(debug_file_info_t));
		if (NULL == pCryptoService->debug_file) {
			LAC_OS_FREE(instance_name);
			return CPA_STATUS_RESOURCE;
		}

		memset(pCryptoService->debug_file,
		       0,
		       sizeof(debug_file_info_t));
		pCryptoService->debug_file->name = instance_name;
		pCryptoService->debug_file->seq_read = SalCtrl_CryptoDebug;
		pCryptoService->debug_file->private_data = pCryptoService;
		pCryptoService->debug_file->parent =
		    pCryptoService->generic_service_info.debug_parent_dir;

		status =
		    icp_adf_debugAddFile(device, pCryptoService->debug_file);
		if (CPA_STATUS_SUCCESS != status) {
			LAC_OS_FREE(instance_name);
			LAC_OS_FREE(pCryptoService->debug_file);
			return status;
		}
	}
	pCryptoService->generic_service_info.stats = pStatsCollection;

	return status;
}

static CpaStatus
SalCtrl_GetBankNum(icp_accel_dev_t *device,
		   Cpa32U inst,
		   char *section,
		   char *bank_name,
		   Cpa16U *bank)
{
	char adfParamValue[ADF_CFG_MAX_VAL_LEN_IN_BYTES] = { 0 };
	char adfParamName[SAL_CFG_MAX_VAL_LEN_IN_BYTES] = { 0 };
	CpaStatus status = CPA_STATUS_SUCCESS;

	status = Sal_StringParsing("Cy", inst, bank_name, adfParamName);
	LAC_CHECK_STATUS(status);
	status = icp_adf_cfgGetParamValue(device,
					  section,
					  adfParamName,
					  adfParamValue);
	if (CPA_STATUS_SUCCESS != status) {
		QAT_UTILS_LOG("Failed to get %s from configuration file\n",
			      adfParamName);
		return status;
	}
	*bank = (Cpa16U)Sal_Strtoul(adfParamValue, NULL, SAL_CFG_BASE_DEC);
	return status;
}

static CpaStatus
SalCtr_InstInit(icp_accel_dev_t *device, sal_service_t *service)
{
	char adfGetParam[ADF_CFG_MAX_VAL_LEN_IN_BYTES] = { 0 };
	char temp_string[SAL_CFG_MAX_VAL_LEN_IN_BYTES] = { 0 };
	char temp_string2[SAL_CFG_MAX_VAL_LEN_IN_BYTES] = { 0 };
	sal_crypto_service_t *pCryptoService = (sal_crypto_service_t *)service;
	CpaStatus status = CPA_STATUS_SUCCESS;
	char *section = DYN_SEC;

	/*Instance may not in the DYN section*/
	if (CPA_FALSE == pCryptoService->generic_service_info.is_dyn) {
		section = icpGetProcessName();
	}


	/* Get Config Info: Accel Num, bank Num, packageID,
				coreAffinity, nodeAffinity and response mode */

	pCryptoService->acceleratorNum = 0;

	/* Gen4, a bank only has 2 rings (1 ring pair), only one type of service
	   can be assigned one time. asym and sym will be in different bank*/
	if (isCyGen4x(pCryptoService)) {
		switch (service->type) {
		case SAL_SERVICE_TYPE_CRYPTO_ASYM:
			status = SalCtrl_GetBankNum(
			    device,
			    pCryptoService->generic_service_info.instance,
			    section,
			    "BankNumberAsym",
			    &pCryptoService->bankNumAsym);
			if (CPA_STATUS_SUCCESS != status)
				return status;
			break;
		case SAL_SERVICE_TYPE_CRYPTO_SYM:
			status = SalCtrl_GetBankNum(
			    device,
			    pCryptoService->generic_service_info.instance,
			    section,
			    "BankNumberSym",
			    &pCryptoService->bankNumSym);
			if (CPA_STATUS_SUCCESS != status)
				return status;
			break;
		case SAL_SERVICE_TYPE_CRYPTO:
			status = SalCtrl_GetBankNum(
			    device,
			    pCryptoService->generic_service_info.instance,
			    section,
			    "BankNumberAsym",
			    &pCryptoService->bankNumAsym);
			if (CPA_STATUS_SUCCESS != status)
				return status;
			status = SalCtrl_GetBankNum(
			    device,
			    pCryptoService->generic_service_info.instance,
			    section,
			    "BankNumberSym",
			    &pCryptoService->bankNumSym);
			if (CPA_STATUS_SUCCESS != status)
				return status;
			break;
		default:
			return CPA_STATUS_FAIL;
		}
	} else {
		status = SalCtrl_GetBankNum(
		    device,
		    pCryptoService->generic_service_info.instance,
		    section,
		    "BankNumber",
		    &pCryptoService->bankNumSym);
		if (CPA_STATUS_SUCCESS != status)
			return status;
		pCryptoService->bankNumAsym = pCryptoService->bankNumSym;
	}

	status =
	    Sal_StringParsing("Cy",
			      pCryptoService->generic_service_info.instance,
			      "IsPolled",
			      temp_string);
	LAC_CHECK_STATUS(status);
	status =
	    icp_adf_cfgGetParamValue(device, section, temp_string, adfGetParam);
	if (CPA_STATUS_SUCCESS != status) {
		QAT_UTILS_LOG("Failed to get %s from configuration file\n",
			      temp_string);
		return status;
	}
	pCryptoService->isPolled =
	    (Cpa8U)Sal_Strtoul(adfGetParam, NULL, SAL_CFG_BASE_DEC);

	/* Kernel instances do not support epoll mode */
	if (SAL_RESP_EPOLL_CFG_FILE == pCryptoService->isPolled) {
		QAT_UTILS_LOG(
		    "IsPolled %u is not supported for kernel instance %s",
		    pCryptoService->isPolled,
		    temp_string);
		return CPA_STATUS_FAIL;
	}

	status = icp_adf_cfgGetParamValue(device,
					  LAC_CFG_SECTION_GENERAL,
					  ADF_DEV_PKG_ID,
					  adfGetParam);
	if (CPA_STATUS_SUCCESS != status) {
		QAT_UTILS_LOG("Failed to get %s from configuration file\n",
			      ADF_DEV_PKG_ID);
		return status;
	}
	pCryptoService->pkgID =
	    (Cpa16U)Sal_Strtoul(adfGetParam, NULL, SAL_CFG_BASE_DEC);

	status = icp_adf_cfgGetParamValue(device,
					  LAC_CFG_SECTION_GENERAL,
					  ADF_DEV_NODE_ID,
					  adfGetParam);
	if (CPA_STATUS_SUCCESS != status) {
		QAT_UTILS_LOG("Failed to get %s from configuration file\n",
			      ADF_DEV_NODE_ID);
		return status;
	}
	pCryptoService->nodeAffinity =
	    (Cpa32U)Sal_Strtoul(adfGetParam, NULL, SAL_CFG_BASE_DEC);
	/* In case of interrupt instance, use the bank affinity set by adf_ctl
	 * Otherwise, use the instance affinity for backwards compatibility */
	if (SAL_RESP_POLL_CFG_FILE != pCryptoService->isPolled) {
		/* Next need to read the [AcceleratorX] section of the config
		 * file */
		status = Sal_StringParsing("Accelerator",
					   pCryptoService->acceleratorNum,
					   "",
					   temp_string2);
		LAC_CHECK_STATUS(status);
		if (service->type == SAL_SERVICE_TYPE_CRYPTO_ASYM)
			status = Sal_StringParsing("Bank",
						   pCryptoService->bankNumAsym,
						   "CoreAffinity",
						   temp_string);
		else
			/* For cy service, asym bank and sym bank will set the
			   same core affinity. So Just read one*/
			status = Sal_StringParsing("Bank",
						   pCryptoService->bankNumSym,
						   "CoreAffinity",
						   temp_string);
		LAC_CHECK_STATUS(status);
	} else {
		strncpy(temp_string2, section, (strlen(section) + 1));
		status = Sal_StringParsing(
		    "Cy",
		    pCryptoService->generic_service_info.instance,
		    "CoreAffinity",
		    temp_string);
		LAC_CHECK_STATUS(status);
	}

	status = icp_adf_cfgGetParamValue(device,
					  temp_string2,
					  temp_string,
					  adfGetParam);
	if (CPA_STATUS_SUCCESS != status) {
		QAT_UTILS_LOG("Failed to get %s from configuration file\n",
			      temp_string);
		return status;
	}
	pCryptoService->coreAffinity =
	    (Cpa32U)Sal_Strtoul(adfGetParam, NULL, SAL_CFG_BASE_DEC);

	/*No Execution Engine in DH895xcc, so make sure it is zero*/
	pCryptoService->executionEngine = 0;

	return status;
}

/* This function:
 * 1. Creates sym and asym transport handles
 * 2. Allocates memory pools required by sym and asym services
.* 3. Clears the sym and asym stats counters
 * 4. In case service asym or sym is enabled then this function
 *    only allocates resources for these services. i.e if the
 *    service asym is enabled then only asym transport handles
 *    are created and vice versa.
 */
CpaStatus
SalCtrl_CryptoInit(icp_accel_dev_t *device, sal_service_t *service)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	sal_crypto_service_t *pCryptoService = (sal_crypto_service_t *)service;
	sal_service_type_t svc_type = service->type;

	SAL_SERVICE_GOOD_FOR_INIT(pCryptoService);
	pCryptoService->generic_service_info.state =
	    SAL_SERVICE_STATE_INITIALIZING;

	/* Set up the instance parameters such as bank number,
	 * coreAffinity, pkgId and node affinity etc
	 */
	status = SalCtr_InstInit(device, service);
	LAC_CHECK_STATUS(status);
	/* Create debug directory for service */
	status = SalCtrl_DebugInit(device, service);
	LAC_CHECK_STATUS(status);

	switch (svc_type) {
	case SAL_SERVICE_TYPE_CRYPTO_ASYM:
		break;
	case SAL_SERVICE_TYPE_CRYPTO_SYM:
		status = SalCtrl_SymInit(device, service);
		if (CPA_STATUS_SUCCESS != status) {
			SalCtrl_DebugShutdown(device, service);
			return status;
		}
		break;
	case SAL_SERVICE_TYPE_CRYPTO:
		status = SalCtrl_SymInit(device, service);
		if (CPA_STATUS_SUCCESS != status) {
			SalCtrl_DebugShutdown(device, service);
			return status;
		}
		break;
	default:
		LAC_LOG_ERROR("Invalid service type\n");
		status = CPA_STATUS_FAIL;
		break;
	}

	pCryptoService->generic_service_info.state =
	    SAL_SERVICE_STATE_INITIALIZED;

	return status;
}

CpaStatus
SalCtrl_CryptoStart(icp_accel_dev_t *device, sal_service_t *service)
{
	sal_crypto_service_t *pCryptoService = (sal_crypto_service_t *)service;
	CpaStatus status = CPA_STATUS_SUCCESS;

	if (pCryptoService->generic_service_info.state !=
	    SAL_SERVICE_STATE_INITIALIZED) {
		LAC_LOG_ERROR("Not in the correct state to call start\n");
		return CPA_STATUS_FAIL;
	}

	pCryptoService->generic_service_info.state = SAL_SERVICE_STATE_RUNNING;
	return status;
}

CpaStatus
SalCtrl_CryptoStop(icp_accel_dev_t *device, sal_service_t *service)
{
	sal_crypto_service_t *pCryptoService = (sal_crypto_service_t *)service;

	if (SAL_SERVICE_STATE_RUNNING !=
	    pCryptoService->generic_service_info.state) {
		LAC_LOG_ERROR("Not in the correct state to call stop");
	}

	pCryptoService->generic_service_info.state =
	    SAL_SERVICE_STATE_SHUTTING_DOWN;
	return CPA_STATUS_SUCCESS;
}

CpaStatus
SalCtrl_CryptoShutdown(icp_accel_dev_t *device, sal_service_t *service)
{
	sal_crypto_service_t *pCryptoService = (sal_crypto_service_t *)service;
	CpaStatus status = CPA_STATUS_SUCCESS;
	sal_service_type_t svc_type = service->type;

	if ((SAL_SERVICE_STATE_INITIALIZED !=
	     pCryptoService->generic_service_info.state) &&
	    (SAL_SERVICE_STATE_SHUTTING_DOWN !=
	     pCryptoService->generic_service_info.state)) {
		LAC_LOG_ERROR("Not in the correct state to call shutdown \n");
		return CPA_STATUS_FAIL;
	}


	/* Free memory and transhandles */
	switch (svc_type) {
	case SAL_SERVICE_TYPE_CRYPTO_ASYM:
		break;
	case SAL_SERVICE_TYPE_CRYPTO_SYM:
		if (SalCtrl_SymFreeResources(pCryptoService)) {
			status = CPA_STATUS_FAIL;
		}
		break;
	case SAL_SERVICE_TYPE_CRYPTO:
		if (SalCtrl_SymFreeResources(pCryptoService)) {
			status = CPA_STATUS_FAIL;
		}
		break;
	default:
		LAC_LOG_ERROR("Invalid service type\n");
		status = CPA_STATUS_FAIL;
		break;
	}

	SalCtrl_DebugShutdown(device, service);

	pCryptoService->generic_service_info.state = SAL_SERVICE_STATE_SHUTDOWN;

	return status;
}

/**
 ******************************************************************************
 * @ingroup cpaCyCommon
 *****************************************************************************/
CpaStatus
cpaCyGetStatusText(const CpaInstanceHandle instanceHandle,
		   CpaStatus errStatus,
		   Cpa8S *pStatusText)
{
	CpaStatus status = CPA_STATUS_SUCCESS;


	LAC_CHECK_NULL_PARAM(pStatusText);

	switch (errStatus) {
	case CPA_STATUS_SUCCESS:
		LAC_COPY_STRING(pStatusText, CPA_STATUS_STR_SUCCESS);
		break;
	case CPA_STATUS_FAIL:
		LAC_COPY_STRING(pStatusText, CPA_STATUS_STR_FAIL);
		break;
	case CPA_STATUS_RETRY:
		LAC_COPY_STRING(pStatusText, CPA_STATUS_STR_RETRY);
		break;
	case CPA_STATUS_RESOURCE:
		LAC_COPY_STRING(pStatusText, CPA_STATUS_STR_RESOURCE);
		break;
	case CPA_STATUS_INVALID_PARAM:
		LAC_COPY_STRING(pStatusText, CPA_STATUS_STR_INVALID_PARAM);
		break;
	case CPA_STATUS_FATAL:
		LAC_COPY_STRING(pStatusText, CPA_STATUS_STR_FATAL);
		break;
	case CPA_STATUS_UNSUPPORTED:
		LAC_COPY_STRING(pStatusText, CPA_STATUS_STR_UNSUPPORTED);
		break;
	default:
		status = CPA_STATUS_INVALID_PARAM;
		break;
	}
	return status;
}

void
SalCtrl_CyQueryCapabilities(sal_service_t *pGenericService,
			    CpaCyCapabilitiesInfo *pCapInfo)
{
	memset(pCapInfo, 0, sizeof(CpaCyCapabilitiesInfo));

	if (SAL_SERVICE_TYPE_CRYPTO == pGenericService->type ||
	    SAL_SERVICE_TYPE_CRYPTO_SYM == pGenericService->type) {
		pCapInfo->symSupported = CPA_TRUE;
		if (pGenericService->capabilitiesMask &
		    ICP_ACCEL_CAPABILITIES_EXT_ALGCHAIN) {
			pCapInfo->extAlgchainSupported = CPA_TRUE;
		}

		if (pGenericService->capabilitiesMask &
		    ICP_ACCEL_CAPABILITIES_HKDF) {
			pCapInfo->hkdfSupported = CPA_TRUE;
		}
	}

	if (pGenericService->capabilitiesMask &
	    ICP_ACCEL_CAPABILITIES_ECEDMONT) {
		pCapInfo->ecEdMontSupported = CPA_TRUE;
	}

	if (pGenericService->capabilitiesMask &
	    ICP_ACCEL_CAPABILITIES_RANDOM_NUMBER) {
		pCapInfo->nrbgSupported = CPA_TRUE;
	}

	pCapInfo->drbgSupported = CPA_FALSE;
	pCapInfo->randSupported = CPA_FALSE;
	pCapInfo->nrbgSupported = CPA_FALSE;
}

/**
 ******************************************************************************
 * @ingroup cpaCyCommon
 *****************************************************************************/
CpaStatus
cpaCyStartInstance(CpaInstanceHandle instanceHandle_in)
{
	CpaInstanceHandle instanceHandle = NULL;
/* Structure initializer is supported by C99, but it is
 * not supported by some former Intel compilers.
 */
	CpaInstanceInfo2 info = { 0 };
	icp_accel_dev_t *dev = NULL;
	CpaStatus status = CPA_STATUS_SUCCESS;
	sal_crypto_service_t *pService = NULL;


	if (CPA_INSTANCE_HANDLE_SINGLE == instanceHandle_in) {
		instanceHandle = Lac_GetFirstHandle(SAL_SERVICE_TYPE_CRYPTO);
		if (!instanceHandle) {
			instanceHandle =
			    Lac_GetFirstHandle(SAL_SERVICE_TYPE_CRYPTO_SYM);
		}
	} else {
		instanceHandle = instanceHandle_in;
	}
	LAC_CHECK_NULL_PARAM(instanceHandle);
	SAL_CHECK_INSTANCE_TYPE(instanceHandle,
				(SAL_SERVICE_TYPE_CRYPTO |
				 SAL_SERVICE_TYPE_CRYPTO_ASYM |
				 SAL_SERVICE_TYPE_CRYPTO_SYM));

	pService = (sal_crypto_service_t *)instanceHandle;

	status = cpaCyInstanceGetInfo2(instanceHandle, &info);
	if (CPA_STATUS_SUCCESS != status) {
		LAC_LOG_ERROR("Can not get instance info\n");
		return status;
	}
	dev = icp_adf_getAccelDevByAccelId(info.physInstId.packageId);
	if (NULL == dev) {
		LAC_LOG_ERROR("Can not find device for the instance\n");
		return CPA_STATUS_FAIL;
	}

	pService->generic_service_info.isInstanceStarted = CPA_TRUE;

	/* Increment dev ref counter */
	icp_qa_dev_get(dev);
	return CPA_STATUS_SUCCESS;
}

/**
 ******************************************************************************
 * @ingroup cpaCyCommon
 *****************************************************************************/
CpaStatus
cpaCyStopInstance(CpaInstanceHandle instanceHandle_in)
{
	CpaInstanceHandle instanceHandle = NULL;
/* Structure initializer is supported by C99, but it is
 * not supported by some former Intel compilers.
 */
	CpaInstanceInfo2 info = { 0 };
	icp_accel_dev_t *dev = NULL;
	CpaStatus status = CPA_STATUS_SUCCESS;
	sal_crypto_service_t *pService = NULL;


	if (CPA_INSTANCE_HANDLE_SINGLE == instanceHandle_in) {
		instanceHandle = Lac_CryptoGetFirstHandle();
	} else {
		instanceHandle = instanceHandle_in;
	}
	LAC_CHECK_NULL_PARAM(instanceHandle);
	SAL_CHECK_INSTANCE_TYPE(instanceHandle,
				(SAL_SERVICE_TYPE_CRYPTO |
				 SAL_SERVICE_TYPE_CRYPTO_ASYM |
				 SAL_SERVICE_TYPE_CRYPTO_SYM));

	status = cpaCyInstanceGetInfo2(instanceHandle, &info);
	if (CPA_STATUS_SUCCESS != status) {
		LAC_LOG_ERROR("Can not get instance info\n");
		return status;
	}
	dev = icp_adf_getAccelDevByAccelId(info.physInstId.packageId);
	if (NULL == dev) {
		LAC_LOG_ERROR("Can not find device for the instance\n");
		return CPA_STATUS_FAIL;
	}

	pService = (sal_crypto_service_t *)instanceHandle;

	pService->generic_service_info.isInstanceStarted = CPA_FALSE;

	/* Decrement dev ref counter */
	icp_qa_dev_put(dev);
	return CPA_STATUS_SUCCESS;
}

/**
 ******************************************************************************
 * @ingroup cpaCyCommon
 *****************************************************************************/
CpaStatus
cpaCyInstanceSetNotificationCb(
    const CpaInstanceHandle instanceHandle,
    const CpaCyInstanceNotificationCbFunc pInstanceNotificationCb,
    void *pCallbackTag)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	sal_service_t *gen_handle = instanceHandle;


	LAC_CHECK_NULL_PARAM(gen_handle);
	gen_handle->notification_cb = pInstanceNotificationCb;
	gen_handle->cb_tag = pCallbackTag;
	return status;
}

/**
 ******************************************************************************
 * @ingroup cpaCyCommon
 *****************************************************************************/
CpaStatus
cpaCyGetNumInstances(Cpa16U *pNumInstances)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	CpaInstanceHandle cyInstanceHandle;
	CpaInstanceInfo2 info;
	icp_accel_dev_t **pAdfInsts = NULL;
	icp_accel_dev_t *dev_addr = NULL;
	sal_t *base_addr = NULL;
	sal_list_t *list_temp = NULL;
	Cpa16U num_accel_dev = 0;
	Cpa16U num_inst = 0;
	Cpa16U i = 0;

	LAC_CHECK_NULL_PARAM(pNumInstances);

	/* Get the number of accel_dev in the system */
	status = icp_amgr_getNumInstances(&num_accel_dev);
	LAC_CHECK_STATUS(status);

	/* Allocate memory to store addr of accel_devs */
	pAdfInsts =
	    malloc(num_accel_dev * sizeof(icp_accel_dev_t *), M_QAT, M_WAITOK);
	num_accel_dev = 0;
	/* Get ADF to return all accel_devs that support either
	 * symmetric or asymmetric crypto */
	status = icp_amgr_getAllAccelDevByCapabilities(
	    (ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC |
	     ICP_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC),
	    pAdfInsts,
	    &num_accel_dev);
	if (CPA_STATUS_SUCCESS != status) {
		LAC_LOG_ERROR("No support for crypto\n");
		*pNumInstances = 0;
		free(pAdfInsts, M_QAT);
		return status;
	}

	for (i = 0; i < num_accel_dev; i++) {
		dev_addr = (icp_accel_dev_t *)pAdfInsts[i];
		if (NULL == dev_addr || NULL == dev_addr->pSalHandle) {
			continue;
		}

		base_addr = dev_addr->pSalHandle;
		list_temp = base_addr->crypto_services;
		while (NULL != list_temp) {
			cyInstanceHandle = SalList_getObject(list_temp);
			status = cpaCyInstanceGetInfo2(cyInstanceHandle, &info);
			if (CPA_STATUS_SUCCESS == status &&
			    CPA_TRUE == info.isPolled) {
				num_inst++;
			}
			list_temp = SalList_next(list_temp);
		}
		list_temp = base_addr->asym_services;
		while (NULL != list_temp) {
			cyInstanceHandle = SalList_getObject(list_temp);
			status = cpaCyInstanceGetInfo2(cyInstanceHandle, &info);
			if (CPA_STATUS_SUCCESS == status &&
			    CPA_TRUE == info.isPolled) {
				num_inst++;
			}
			list_temp = SalList_next(list_temp);
		}
		list_temp = base_addr->sym_services;
		while (NULL != list_temp) {
			cyInstanceHandle = SalList_getObject(list_temp);
			status = cpaCyInstanceGetInfo2(cyInstanceHandle, &info);
			if (CPA_STATUS_SUCCESS == status &&
			    CPA_TRUE == info.isPolled) {
				num_inst++;
			}
			list_temp = SalList_next(list_temp);
		}
	}
	*pNumInstances = num_inst;
	free(pAdfInsts, M_QAT);


	return status;
}

/**
 ******************************************************************************
 * @ingroup cpaCyCommon
 *****************************************************************************/
CpaStatus
cpaCyGetInstances(Cpa16U numInstances, CpaInstanceHandle *pCyInstances)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	CpaInstanceHandle cyInstanceHandle;
	CpaInstanceInfo2 info;
	icp_accel_dev_t **pAdfInsts = NULL;
	icp_accel_dev_t *dev_addr = NULL;
	sal_t *base_addr = NULL;
	sal_list_t *list_temp = NULL;
	Cpa16U num_accel_dev = 0;
	Cpa16U num_allocated_instances = 0;
	Cpa16U index = 0;
	Cpa16U i = 0;


	LAC_CHECK_NULL_PARAM(pCyInstances);
	if (0 == numInstances) {
		LAC_INVALID_PARAM_LOG("NumInstances is 0");
		return CPA_STATUS_INVALID_PARAM;
	}

	/* Get the number of crypto instances */
	status = cpaCyGetNumInstances(&num_allocated_instances);
	if (CPA_STATUS_SUCCESS != status) {
		return status;
	}

	if (numInstances > num_allocated_instances) {
		QAT_UTILS_LOG("Only %d crypto instances available\n",
			      num_allocated_instances);
		return CPA_STATUS_RESOURCE;
	}

	/* Get the number of accel devices in the system */
	status = icp_amgr_getNumInstances(&num_accel_dev);
	LAC_CHECK_STATUS(status);

	/* Allocate memory to store addr of accel_devs */
	pAdfInsts =
	    malloc(num_accel_dev * sizeof(icp_accel_dev_t *), M_QAT, M_WAITOK);

	num_accel_dev = 0;
	/* Get ADF to return all accel_devs that support either
	 * symmetric or asymmetric crypto */
	status = icp_amgr_getAllAccelDevByCapabilities(
	    (ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC |
	     ICP_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC),
	    pAdfInsts,
	    &num_accel_dev);
	if (CPA_STATUS_SUCCESS != status) {
		LAC_LOG_ERROR("No support for crypto\n");
		free(pAdfInsts, M_QAT);
		return status;
	}

	for (i = 0; i < num_accel_dev; i++) {
		dev_addr = (icp_accel_dev_t *)pAdfInsts[i];
		/* Note dev_addr cannot be NULL here as numInstances = 0
		 * is not valid and if dev_addr = NULL then index = 0 (which
		 * is less than numInstances and status is set to _RESOURCE
		 * above
		 */
		base_addr = dev_addr->pSalHandle;
		if (NULL == base_addr) {
			continue;
		}
		list_temp = base_addr->crypto_services;
		while (NULL != list_temp) {
			if (index > (numInstances - 1)) {
				break;
			}
			cyInstanceHandle = SalList_getObject(list_temp);
			status = cpaCyInstanceGetInfo2(cyInstanceHandle, &info);
			list_temp = SalList_next(list_temp);
			if (CPA_STATUS_SUCCESS != status ||
			    CPA_TRUE != info.isPolled) {
				continue;
			}
			pCyInstances[index] = cyInstanceHandle;
			index++;
		}
		list_temp = base_addr->asym_services;
		while (NULL != list_temp) {
			if (index > (numInstances - 1)) {
				break;
			}
			cyInstanceHandle = SalList_getObject(list_temp);
			status = cpaCyInstanceGetInfo2(cyInstanceHandle, &info);
			list_temp = SalList_next(list_temp);
			if (CPA_STATUS_SUCCESS != status ||
			    CPA_TRUE != info.isPolled) {
				continue;
			}
			pCyInstances[index] = cyInstanceHandle;
			index++;
		}
		list_temp = base_addr->sym_services;
		while (NULL != list_temp) {
			if (index > (numInstances - 1)) {
				break;
			}
			cyInstanceHandle = SalList_getObject(list_temp);
			status = cpaCyInstanceGetInfo2(cyInstanceHandle, &info);
			list_temp = SalList_next(list_temp);
			if (CPA_STATUS_SUCCESS != status ||
			    CPA_TRUE != info.isPolled) {
				continue;
			}
			pCyInstances[index] = cyInstanceHandle;
			index++;
		}
	}
	free(pAdfInsts, M_QAT);

	return status;
}

/**
 ******************************************************************************
 * @ingroup cpaCyCommon
 *****************************************************************************/
CpaStatus
cpaCyInstanceGetInfo(const CpaInstanceHandle instanceHandle_in,
		     struct _CpaInstanceInfo *pInstanceInfo)
{
	CpaInstanceHandle instanceHandle = NULL;
	sal_crypto_service_t *pCryptoService = NULL;
	sal_service_t *pGenericService = NULL;

	Cpa8U name[CPA_INST_NAME_SIZE] =
	    "Intel(R) DH89XXCC instance number: %02x, type: Crypto";

	if (CPA_INSTANCE_HANDLE_SINGLE == instanceHandle_in) {
		instanceHandle = Lac_CryptoGetFirstHandle();
	} else {
		instanceHandle = instanceHandle_in;
	}

	LAC_CHECK_NULL_PARAM(instanceHandle);
	LAC_CHECK_NULL_PARAM(pInstanceInfo);
	SAL_CHECK_INSTANCE_TYPE(instanceHandle,
				(SAL_SERVICE_TYPE_CRYPTO |
				 SAL_SERVICE_TYPE_CRYPTO_ASYM |
				 SAL_SERVICE_TYPE_CRYPTO_SYM));

	pCryptoService = (sal_crypto_service_t *)instanceHandle;

	pInstanceInfo->type = CPA_INSTANCE_TYPE_CRYPTO;

	/* According to cpa.h instance state is initialized and ready for use
	 * or shutdown. Therefore need to map our running state to initialised
	 * or shutdown */
	if (SAL_SERVICE_STATE_RUNNING ==
	    pCryptoService->generic_service_info.state) {
		pInstanceInfo->state = CPA_INSTANCE_STATE_INITIALISED;
	} else {
		pInstanceInfo->state = CPA_INSTANCE_STATE_SHUTDOWN;
	}

	pGenericService = (sal_service_t *)instanceHandle;
	snprintf((char *)pInstanceInfo->name,
		 CPA_INST_NAME_SIZE,
		 (char *)name,
		 pGenericService->instance);

	pInstanceInfo->name[CPA_INST_NAME_SIZE - 1] = '\0';

	snprintf((char *)pInstanceInfo->version,
		 CPA_INSTANCE_MAX_NAME_SIZE_IN_BYTES,
		 "%d.%d",
		 CPA_CY_API_VERSION_NUM_MAJOR,
		 CPA_CY_API_VERSION_NUM_MINOR);

	pInstanceInfo->version[CPA_INSTANCE_MAX_VERSION_SIZE_IN_BYTES - 1] =
	    '\0';
	return CPA_STATUS_SUCCESS;
}

/**
 ******************************************************************************
 * @ingroup cpaCyCommon
 *****************************************************************************/
CpaStatus
cpaCyInstanceGetInfo2(const CpaInstanceHandle instanceHandle_in,
		      CpaInstanceInfo2 *pInstanceInfo2)
{
	CpaInstanceHandle instanceHandle = NULL;
	sal_crypto_service_t *pCryptoService = NULL;
	icp_accel_dev_t *dev = NULL;
	CpaStatus status = CPA_STATUS_SUCCESS;
	char keyStr[ADF_CFG_MAX_KEY_LEN_IN_BYTES] = { 0 };
	char valStr[ADF_CFG_MAX_VAL_LEN_IN_BYTES] = { 0 };
	char *section = DYN_SEC;


	if (CPA_INSTANCE_HANDLE_SINGLE == instanceHandle_in) {
		instanceHandle = Lac_CryptoGetFirstHandle();
	} else {
		instanceHandle = instanceHandle_in;
	}

	LAC_CHECK_NULL_PARAM(instanceHandle);
	LAC_CHECK_NULL_PARAM(pInstanceInfo2);
	SAL_CHECK_INSTANCE_TYPE(instanceHandle,
				(SAL_SERVICE_TYPE_CRYPTO |
				 SAL_SERVICE_TYPE_CRYPTO_ASYM |
				 SAL_SERVICE_TYPE_CRYPTO_SYM));

	LAC_OS_BZERO(pInstanceInfo2, sizeof(CpaInstanceInfo2));
	pInstanceInfo2->accelerationServiceType = CPA_ACC_SVC_TYPE_CRYPTO;
	snprintf((char *)pInstanceInfo2->vendorName,
		 CPA_INST_VENDOR_NAME_SIZE,
		 "%s",
		 SAL_INFO2_VENDOR_NAME);
	pInstanceInfo2->vendorName[CPA_INST_VENDOR_NAME_SIZE - 1] = '\0';

	snprintf((char *)pInstanceInfo2->swVersion,
		 CPA_INST_SW_VERSION_SIZE,
		 "Version %d.%d",
		 SAL_INFO2_DRIVER_SW_VERSION_MAJ_NUMBER,
		 SAL_INFO2_DRIVER_SW_VERSION_MIN_NUMBER);
	pInstanceInfo2->swVersion[CPA_INST_SW_VERSION_SIZE - 1] = '\0';

	/* Note we can safely read the contents of the crypto service instance
	   here because icp_amgr_getAllAccelDevByCapabilities() only returns
	   devs
	   that have started */
	pCryptoService = (sal_crypto_service_t *)instanceHandle;
	pInstanceInfo2->physInstId.packageId = pCryptoService->pkgID;
	pInstanceInfo2->physInstId.acceleratorId =
	    pCryptoService->acceleratorNum;
	pInstanceInfo2->physInstId.executionEngineId =
	    pCryptoService->executionEngine;
	pInstanceInfo2->physInstId.busAddress =
	    icp_adf_get_busAddress(pInstanceInfo2->physInstId.packageId);

	/*set coreAffinity to zero before use */
	LAC_OS_BZERO(pInstanceInfo2->coreAffinity,
		     sizeof(pInstanceInfo2->coreAffinity));
	CPA_BITMAP_BIT_SET(pInstanceInfo2->coreAffinity,
			   pCryptoService->coreAffinity);
	pInstanceInfo2->nodeAffinity = pCryptoService->nodeAffinity;

	if (SAL_SERVICE_STATE_RUNNING ==
	    pCryptoService->generic_service_info.state) {
		pInstanceInfo2->operState = CPA_OPER_STATE_UP;
	} else {
		pInstanceInfo2->operState = CPA_OPER_STATE_DOWN;
	}

	pInstanceInfo2->requiresPhysicallyContiguousMemory = CPA_TRUE;
	if (SAL_RESP_POLL_CFG_FILE == pCryptoService->isPolled) {
		pInstanceInfo2->isPolled = CPA_TRUE;
	} else {
		pInstanceInfo2->isPolled = CPA_FALSE;
	}
	pInstanceInfo2->isOffloaded = CPA_TRUE;

	/* Get the instance name and part name*/
	dev = icp_adf_getAccelDevByAccelId(pCryptoService->pkgID);
	if (NULL == dev) {
		LAC_LOG_ERROR("Can not find device for the instance\n");
		LAC_OS_BZERO(pInstanceInfo2, sizeof(CpaInstanceInfo2));
		return CPA_STATUS_FAIL;
	}
	snprintf((char *)pInstanceInfo2->partName,
		 CPA_INST_PART_NAME_SIZE,
		 SAL_INFO2_PART_NAME,
		 dev->deviceName);
	pInstanceInfo2->partName[CPA_INST_PART_NAME_SIZE - 1] = '\0';

	status =
	    Sal_StringParsing("Cy",
			      pCryptoService->generic_service_info.instance,
			      "Name",
			      keyStr);
	LAC_CHECK_STATUS(status);

	if (CPA_FALSE == pCryptoService->generic_service_info.is_dyn) {
		section = icpGetProcessName();
	}

	status = icp_adf_cfgGetParamValue(dev, section, keyStr, valStr);
	LAC_CHECK_STATUS(status);

	snprintf((char *)pInstanceInfo2->instName,
		 CPA_INST_NAME_SIZE,
		 "%s",
		 valStr);
	snprintf((char *)pInstanceInfo2->instID,
		 CPA_INST_ID_SIZE,
		 "%s_%s",
		 section,
		 valStr);
	return CPA_STATUS_SUCCESS;
}

/**
 ******************************************************************************
 * @ingroup cpaCyCommon
 *****************************************************************************/

CpaStatus
cpaCyQueryCapabilities(const CpaInstanceHandle instanceHandle_in,
		       CpaCyCapabilitiesInfo *pCapInfo)
{
	/* Verify Instance exists */
	CpaInstanceHandle instanceHandle = NULL;


	if (CPA_INSTANCE_HANDLE_SINGLE == instanceHandle_in) {
		instanceHandle = Lac_CryptoGetFirstHandle();
	} else {
		instanceHandle = instanceHandle_in;
	}

	LAC_CHECK_NULL_PARAM(instanceHandle);
	SAL_CHECK_INSTANCE_TYPE(instanceHandle,
				(SAL_SERVICE_TYPE_CRYPTO |
				 SAL_SERVICE_TYPE_CRYPTO_ASYM |
				 SAL_SERVICE_TYPE_CRYPTO_SYM));
	LAC_CHECK_NULL_PARAM(pCapInfo);

	SalCtrl_CyQueryCapabilities((sal_service_t *)instanceHandle, pCapInfo);

	return CPA_STATUS_SUCCESS;
}

/**
 ******************************************************************************
 * @ingroup cpaCySym
 *****************************************************************************/
CpaStatus
cpaCySymQueryCapabilities(const CpaInstanceHandle instanceHandle_in,
			  CpaCySymCapabilitiesInfo *pCapInfo)
{
	sal_crypto_service_t *pCryptoService = NULL;
	sal_service_t *pGenericService = NULL;
	CpaInstanceHandle instanceHandle = NULL;

	/* Verify Instance exists */
	if (CPA_INSTANCE_HANDLE_SINGLE == instanceHandle_in) {
		instanceHandle = Lac_GetFirstHandle(SAL_SERVICE_TYPE_CRYPTO);
		if (!instanceHandle) {
			instanceHandle =
			    Lac_GetFirstHandle(SAL_SERVICE_TYPE_CRYPTO_SYM);
		}
	} else {
		instanceHandle = instanceHandle_in;
	}

	LAC_CHECK_NULL_PARAM(instanceHandle);
	SAL_CHECK_INSTANCE_TYPE(instanceHandle,
				(SAL_SERVICE_TYPE_CRYPTO |
				 SAL_SERVICE_TYPE_CRYPTO_ASYM |
				 SAL_SERVICE_TYPE_CRYPTO_SYM));
	LAC_CHECK_NULL_PARAM(pCapInfo);

	pCryptoService = (sal_crypto_service_t *)instanceHandle;
	pGenericService = &(pCryptoService->generic_service_info);

	memset(pCapInfo, '\0', sizeof(CpaCySymCapabilitiesInfo));
	/* An asym crypto instance does not support sym service */
	if (SAL_SERVICE_TYPE_CRYPTO_ASYM == pGenericService->type) {
		return CPA_STATUS_SUCCESS;
	}

	CPA_BITMAP_BIT_SET(pCapInfo->ciphers, CPA_CY_SYM_CIPHER_NULL);
	CPA_BITMAP_BIT_SET(pCapInfo->ciphers, CPA_CY_SYM_CIPHER_AES_ECB);
	CPA_BITMAP_BIT_SET(pCapInfo->ciphers, CPA_CY_SYM_CIPHER_AES_CBC);
	CPA_BITMAP_BIT_SET(pCapInfo->ciphers, CPA_CY_SYM_CIPHER_AES_CTR);
	CPA_BITMAP_BIT_SET(pCapInfo->ciphers, CPA_CY_SYM_CIPHER_AES_CCM);
	CPA_BITMAP_BIT_SET(pCapInfo->ciphers, CPA_CY_SYM_CIPHER_AES_GCM);
	CPA_BITMAP_BIT_SET(pCapInfo->ciphers, CPA_CY_SYM_CIPHER_AES_XTS);
	if (isCyGen2x(pCryptoService)) {
		CPA_BITMAP_BIT_SET(pCapInfo->ciphers, CPA_CY_SYM_CIPHER_ARC4);
		CPA_BITMAP_BIT_SET(pCapInfo->ciphers,
				   CPA_CY_SYM_CIPHER_DES_ECB);
		CPA_BITMAP_BIT_SET(pCapInfo->ciphers,
				   CPA_CY_SYM_CIPHER_DES_CBC);
		CPA_BITMAP_BIT_SET(pCapInfo->ciphers,
				   CPA_CY_SYM_CIPHER_3DES_ECB);
		CPA_BITMAP_BIT_SET(pCapInfo->ciphers,
				   CPA_CY_SYM_CIPHER_3DES_CBC);
		CPA_BITMAP_BIT_SET(pCapInfo->ciphers,
				   CPA_CY_SYM_CIPHER_3DES_CTR);
		CPA_BITMAP_BIT_SET(pCapInfo->ciphers,
				   CPA_CY_SYM_CIPHER_KASUMI_F8);
		CPA_BITMAP_BIT_SET(pCapInfo->ciphers,
				   CPA_CY_SYM_CIPHER_SNOW3G_UEA2);
		CPA_BITMAP_BIT_SET(pCapInfo->ciphers, CPA_CY_SYM_CIPHER_AES_F8);
	}

	CPA_BITMAP_BIT_SET(pCapInfo->hashes, CPA_CY_SYM_HASH_SHA1);
	CPA_BITMAP_BIT_SET(pCapInfo->hashes, CPA_CY_SYM_HASH_SHA224);
	CPA_BITMAP_BIT_SET(pCapInfo->hashes, CPA_CY_SYM_HASH_SHA256);
	CPA_BITMAP_BIT_SET(pCapInfo->hashes, CPA_CY_SYM_HASH_SHA384);
	CPA_BITMAP_BIT_SET(pCapInfo->hashes, CPA_CY_SYM_HASH_SHA512);
	CPA_BITMAP_BIT_SET(pCapInfo->hashes, CPA_CY_SYM_HASH_AES_XCBC);
	CPA_BITMAP_BIT_SET(pCapInfo->hashes, CPA_CY_SYM_HASH_AES_CCM);
	CPA_BITMAP_BIT_SET(pCapInfo->hashes, CPA_CY_SYM_HASH_AES_GCM);
	CPA_BITMAP_BIT_SET(pCapInfo->hashes, CPA_CY_SYM_HASH_AES_CMAC);
	CPA_BITMAP_BIT_SET(pCapInfo->hashes, CPA_CY_SYM_HASH_AES_GMAC);
	CPA_BITMAP_BIT_SET(pCapInfo->hashes, CPA_CY_SYM_HASH_AES_CBC_MAC);
	if (isCyGen2x(pCryptoService)) {
		CPA_BITMAP_BIT_SET(pCapInfo->hashes, CPA_CY_SYM_HASH_MD5);
		CPA_BITMAP_BIT_SET(pCapInfo->hashes, CPA_CY_SYM_HASH_KASUMI_F9);
		CPA_BITMAP_BIT_SET(pCapInfo->hashes,
				   CPA_CY_SYM_HASH_SNOW3G_UIA2);
	}

	if (pGenericService->capabilitiesMask &
	    ICP_ACCEL_CAPABILITIES_CRYPTO_ZUC) {
		CPA_BITMAP_BIT_SET(pCapInfo->ciphers,
				   CPA_CY_SYM_CIPHER_ZUC_EEA3);
		CPA_BITMAP_BIT_SET(pCapInfo->hashes, CPA_CY_SYM_HASH_ZUC_EIA3);
	}

	if (pGenericService->capabilitiesMask &
	    ICP_ACCEL_CAPABILITIES_CHACHA_POLY) {
		CPA_BITMAP_BIT_SET(pCapInfo->hashes, CPA_CY_SYM_HASH_POLY);
		CPA_BITMAP_BIT_SET(pCapInfo->ciphers, CPA_CY_SYM_CIPHER_CHACHA);
	}

	if (pGenericService->capabilitiesMask & ICP_ACCEL_CAPABILITIES_SM3) {
		CPA_BITMAP_BIT_SET(pCapInfo->hashes, CPA_CY_SYM_HASH_SM3);
	}

	pCapInfo->partialPacketSupported = CPA_TRUE;

	if (pGenericService->capabilitiesMask & ICP_ACCEL_CAPABILITIES_SHA3) {
		CPA_BITMAP_BIT_SET(pCapInfo->hashes, CPA_CY_SYM_HASH_SHA3_256);
		pCapInfo->partialPacketSupported = CPA_FALSE;
	}

	if (pGenericService->capabilitiesMask &
	    ICP_ACCEL_CAPABILITIES_SHA3_EXT) {
		CPA_BITMAP_BIT_SET(pCapInfo->hashes, CPA_CY_SYM_HASH_SHA3_224);
		CPA_BITMAP_BIT_SET(pCapInfo->hashes, CPA_CY_SYM_HASH_SHA3_256);
		CPA_BITMAP_BIT_SET(pCapInfo->hashes, CPA_CY_SYM_HASH_SHA3_384);
		CPA_BITMAP_BIT_SET(pCapInfo->hashes, CPA_CY_SYM_HASH_SHA3_512);
		pCapInfo->partialPacketSupported = CPA_FALSE;
	}

	if (pGenericService->capabilitiesMask & ICP_ACCEL_CAPABILITIES_SM4) {
		CPA_BITMAP_BIT_SET(pCapInfo->ciphers,
				   CPA_CY_SYM_CIPHER_SM4_ECB);
		CPA_BITMAP_BIT_SET(pCapInfo->ciphers,
				   CPA_CY_SYM_CIPHER_SM4_CBC);
		CPA_BITMAP_BIT_SET(pCapInfo->ciphers,
				   CPA_CY_SYM_CIPHER_SM4_CTR);
		pCapInfo->partialPacketSupported = CPA_FALSE;
	}

	return CPA_STATUS_SUCCESS;
}

/**
 ******************************************************************************
 * @ingroup cpaCyCommon
 *****************************************************************************/
CpaStatus
cpaCySetAddressTranslation(const CpaInstanceHandle instanceHandle_in,
			   CpaVirtualToPhysical virtual2physical)
{

	CpaInstanceHandle instanceHandle = NULL;
	sal_service_t *pService = NULL;


	if (CPA_INSTANCE_HANDLE_SINGLE == instanceHandle_in) {
		instanceHandle = Lac_CryptoGetFirstHandle();
	} else {
		instanceHandle = instanceHandle_in;
	}

	LAC_CHECK_NULL_PARAM(instanceHandle);
	SAL_CHECK_INSTANCE_TYPE(instanceHandle,
				(SAL_SERVICE_TYPE_CRYPTO |
				 SAL_SERVICE_TYPE_CRYPTO_ASYM |
				 SAL_SERVICE_TYPE_CRYPTO_SYM));
	LAC_CHECK_NULL_PARAM(virtual2physical);

	pService = (sal_service_t *)instanceHandle;

	pService->virt2PhysClient = virtual2physical;

	return CPA_STATUS_SUCCESS;
}

/**
 ******************************************************************************
 * @ingroup cpaCyCommon
 * Crypto specific polling function which polls a crypto instance.
 *****************************************************************************/
CpaStatus
icp_sal_CyPollInstance(CpaInstanceHandle instanceHandle_in,
		       Cpa32U response_quota)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	sal_crypto_service_t *crypto_handle = NULL;
	sal_service_t *gen_handle = NULL;
	icp_comms_trans_handle trans_hndTable[MAX_CY_RX_RINGS] = { 0 };
	Cpa32U num_rx_rings = 0;

	if (CPA_INSTANCE_HANDLE_SINGLE == instanceHandle_in) {
		crypto_handle =
		    (sal_crypto_service_t *)Lac_CryptoGetFirstHandle();
	} else {
		crypto_handle = (sal_crypto_service_t *)instanceHandle_in;
	}
	LAC_CHECK_NULL_PARAM(crypto_handle);
	SAL_RUNNING_CHECK(crypto_handle);
	SAL_CHECK_INSTANCE_TYPE(crypto_handle,
				(SAL_SERVICE_TYPE_CRYPTO |
				 SAL_SERVICE_TYPE_CRYPTO_ASYM |
				 SAL_SERVICE_TYPE_CRYPTO_SYM));

	gen_handle = &(crypto_handle->generic_service_info);

	/*
	 * From the instanceHandle we must get the trans_handle and send
	 * down to adf for polling.
	 * Populate our trans handle table with the appropriate handles.
	 */

	switch (gen_handle->type) {
	case SAL_SERVICE_TYPE_CRYPTO_ASYM:
		trans_hndTable[TH_CY_RX_0] =
		    crypto_handle->trans_handle_asym_rx;
		num_rx_rings = 1;
		break;
	case SAL_SERVICE_TYPE_CRYPTO_SYM:
		trans_hndTable[TH_CY_RX_0] = crypto_handle->trans_handle_sym_rx;
		num_rx_rings = 1;
		break;
	case SAL_SERVICE_TYPE_CRYPTO:
		trans_hndTable[TH_CY_RX_0] = crypto_handle->trans_handle_sym_rx;
		trans_hndTable[TH_CY_RX_1] =
		    crypto_handle->trans_handle_asym_rx;
		num_rx_rings = MAX_CY_RX_RINGS;
		break;
	default:
		break;
	}

	/* Call adf to do the polling. */
	status =
	    icp_adf_pollInstance(trans_hndTable, num_rx_rings, response_quota);

	return status;
}

/**
 ******************************************************************************
 * @ingroup cpaCyCommon
 * Crypto specific polling function which polls sym crypto ring.
 *****************************************************************************/
CpaStatus
icp_sal_CyPollSymRing(CpaInstanceHandle instanceHandle_in,
		      Cpa32U response_quota)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	sal_crypto_service_t *crypto_handle = NULL;
	icp_comms_trans_handle trans_hndTable[NUM_CRYPTO_SYM_RX_RINGS] = { 0 };

	if (CPA_INSTANCE_HANDLE_SINGLE == instanceHandle_in) {
		crypto_handle = (sal_crypto_service_t *)Lac_GetFirstHandle(
		    SAL_SERVICE_TYPE_CRYPTO_SYM);
	} else {
		crypto_handle = (sal_crypto_service_t *)instanceHandle_in;
	}
	LAC_CHECK_NULL_PARAM(crypto_handle);
	SAL_CHECK_INSTANCE_TYPE(crypto_handle,
				(SAL_SERVICE_TYPE_CRYPTO |
				 SAL_SERVICE_TYPE_CRYPTO_SYM));
	SAL_RUNNING_CHECK(crypto_handle);

	/*
	 * From the instanceHandle we must get the trans_handle and send
	 * down to adf for polling.
	 * Populate our trans handle table with the appropriate handles.
	 */
	trans_hndTable[TH_SINGLE_RX] = crypto_handle->trans_handle_sym_rx;
	/* Call adf to do the polling. */
	status = icp_adf_pollInstance(trans_hndTable,
				      NUM_CRYPTO_SYM_RX_RINGS,
				      response_quota);
	return status;
}


/**
 ******************************************************************************
 * @ingroup cpaCyCommon
 * Crypto specific polling function which polls an nrbg crypto ring.
 *****************************************************************************/
CpaStatus
icp_sal_CyPollNRBGRing(CpaInstanceHandle instanceHandle_in,
		       Cpa32U response_quota)
{
	return CPA_STATUS_UNSUPPORTED;
}

/* Returns the handle to the first asym crypto instance */
static CpaInstanceHandle
Lac_GetFirstAsymHandle(icp_accel_dev_t *adfInsts[ADF_MAX_DEVICES],
		       Cpa16U num_dev)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	icp_accel_dev_t *dev_addr = NULL;
	sal_t *base_addr = NULL;
	sal_list_t *list_temp = NULL;
	CpaInstanceHandle cyInst = NULL;
	CpaInstanceInfo2 info;
	Cpa16U i = 0;

	for (i = 0; i < num_dev; i++) {
		dev_addr = (icp_accel_dev_t *)adfInsts[i];
		base_addr = dev_addr->pSalHandle;
		if (NULL == base_addr) {
			continue;
		}
		list_temp = base_addr->asym_services;
		while (NULL != list_temp) {
			cyInst = SalList_getObject(list_temp);
			status = cpaCyInstanceGetInfo2(cyInst, &info);
			list_temp = SalList_next(list_temp);
			if (CPA_STATUS_SUCCESS != status ||
			    CPA_TRUE != info.isPolled) {
				cyInst = NULL;
				continue;
			}
			break;
		}
		if (cyInst) {
			break;
		}
	}

	return cyInst;
}

/* Returns the handle to the first sym crypto instance */
static CpaInstanceHandle
Lac_GetFirstSymHandle(icp_accel_dev_t *adfInsts[ADF_MAX_DEVICES],
		      Cpa16U num_dev)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	icp_accel_dev_t *dev_addr = NULL;
	sal_t *base_addr = NULL;
	sal_list_t *list_temp = NULL;
	CpaInstanceHandle cyInst = NULL;
	CpaInstanceInfo2 info;
	Cpa16U i = 0;

	for (i = 0; i < num_dev; i++) {
		dev_addr = (icp_accel_dev_t *)adfInsts[i];
		base_addr = dev_addr->pSalHandle;
		if (NULL == base_addr) {
			continue;
		}
		list_temp = base_addr->sym_services;
		while (NULL != list_temp) {
			cyInst = SalList_getObject(list_temp);
			status = cpaCyInstanceGetInfo2(cyInst, &info);
			list_temp = SalList_next(list_temp);
			if (CPA_STATUS_SUCCESS != status ||
			    CPA_TRUE != info.isPolled) {
				cyInst = NULL;
				continue;
			}
			break;
		}
		if (cyInst) {
			break;
		}
	}

	return cyInst;
}

/* Returns the handle to the first crypto instance
 * Note that the crypto instance in this case supports
 * both asym and sym services */
static CpaInstanceHandle
Lac_GetFirstCyHandle(icp_accel_dev_t *adfInsts[ADF_MAX_DEVICES], Cpa16U num_dev)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	icp_accel_dev_t *dev_addr = NULL;
	sal_t *base_addr = NULL;
	sal_list_t *list_temp = NULL;
	CpaInstanceHandle cyInst = NULL;
	CpaInstanceInfo2 info;
	Cpa16U i = 0;

	for (i = 0; i < num_dev; i++) {
		dev_addr = (icp_accel_dev_t *)adfInsts[i];
		base_addr = dev_addr->pSalHandle;
		if (NULL == base_addr) {
			continue;
		}
		list_temp = base_addr->crypto_services;
		while (NULL != list_temp) {
			cyInst = SalList_getObject(list_temp);
			status = cpaCyInstanceGetInfo2(cyInst, &info);
			list_temp = SalList_next(list_temp);
			if (CPA_STATUS_SUCCESS != status ||
			    CPA_TRUE != info.isPolled) {
				cyInst = NULL;
				continue;
			}
			break;
		}
		if (cyInst) {
			break;
		}
	}

	return cyInst;
}

CpaInstanceHandle
Lac_GetFirstHandle(sal_service_type_t svc_type)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	static icp_accel_dev_t *adfInsts[ADF_MAX_DEVICES] = { 0 };
	CpaInstanceHandle cyInst = NULL;
	Cpa16U num_cy_dev = 0;
	Cpa32U capabilities = 0;

	switch (svc_type) {
	case SAL_SERVICE_TYPE_CRYPTO_ASYM:
		capabilities = ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC;
		break;
	case SAL_SERVICE_TYPE_CRYPTO_SYM:
		capabilities = ICP_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC;
		break;
	case SAL_SERVICE_TYPE_CRYPTO:
		capabilities = ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC;
		capabilities |= ICP_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC;
		break;
	default:
		LAC_LOG_ERROR("Invalid service type\n");
		return NULL;
		break;
	}
	/* Only need 1 dev with crypto enabled - so check all devices*/
	status = icp_amgr_getAllAccelDevByEachCapability(capabilities,
							 adfInsts,
							 &num_cy_dev);
	if ((0 == num_cy_dev) || (CPA_STATUS_SUCCESS != status)) {
		LAC_LOG_ERROR("No crypto devices enabled in the system\n");
		return NULL;
	}

	switch (svc_type) {
	case SAL_SERVICE_TYPE_CRYPTO_ASYM:
		/* Try to find an asym only instance first */
		cyInst = Lac_GetFirstAsymHandle(adfInsts, num_cy_dev);
		/* Try to find a cy instance since it also supports asym */
		if (NULL == cyInst) {
			cyInst = Lac_GetFirstCyHandle(adfInsts, num_cy_dev);
		}
		break;
	case SAL_SERVICE_TYPE_CRYPTO_SYM:
		/* Try to find a sym only instance first */
		cyInst = Lac_GetFirstSymHandle(adfInsts, num_cy_dev);
		/* Try to find a cy instance since it also supports sym */
		if (NULL == cyInst) {
			cyInst = Lac_GetFirstCyHandle(adfInsts, num_cy_dev);
		}
		break;
	case SAL_SERVICE_TYPE_CRYPTO:
		/* Try to find a cy instance */
		cyInst = Lac_GetFirstCyHandle(adfInsts, num_cy_dev);
		break;
	default:
		break;
	}
	if (NULL == cyInst) {
		LAC_LOG_ERROR("No remaining crypto instances available\n");
	}
	return cyInst;
}

CpaStatus
icp_sal_NrbgGetInflightRequests(CpaInstanceHandle instanceHandle_in,
				Cpa32U *maxInflightRequests,
				Cpa32U *numInflightRequests)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
icp_sal_SymGetInflightRequests(CpaInstanceHandle instanceHandle,
			       Cpa32U *maxInflightRequests,
			       Cpa32U *numInflightRequests)
{
	sal_crypto_service_t *crypto_handle = NULL;

	crypto_handle = (sal_crypto_service_t *)instanceHandle;

	LAC_CHECK_NULL_PARAM(crypto_handle);
	LAC_CHECK_NULL_PARAM(maxInflightRequests);
	LAC_CHECK_NULL_PARAM(numInflightRequests);
	SAL_RUNNING_CHECK(crypto_handle);

	return icp_adf_getInflightRequests(crypto_handle->trans_handle_sym_tx,
					   maxInflightRequests,
					   numInflightRequests);
}


CpaStatus
icp_sal_dp_SymGetInflightRequests(CpaInstanceHandle instanceHandle,
				  Cpa32U *maxInflightRequests,
				  Cpa32U *numInflightRequests)
{
	sal_crypto_service_t *crypto_handle = NULL;

	crypto_handle = (sal_crypto_service_t *)instanceHandle;

	return icp_adf_dp_getInflightRequests(
	    crypto_handle->trans_handle_sym_tx,
	    maxInflightRequests,
	    numInflightRequests);
}


CpaStatus
icp_sal_setForceAEADMACVerify(CpaInstanceHandle instanceHandle,
			      CpaBoolean forceAEADMacVerify)
{
	sal_crypto_service_t *crypto_handle = NULL;

	crypto_handle = (sal_crypto_service_t *)instanceHandle;
	LAC_CHECK_NULL_PARAM(crypto_handle);
	crypto_handle->forceAEADMacVerify = forceAEADMacVerify;

	return CPA_STATUS_SUCCESS;
}
