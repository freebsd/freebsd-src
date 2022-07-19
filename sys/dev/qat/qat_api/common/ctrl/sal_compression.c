/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
/**
 *****************************************************************************
 * @file sal_compression.c
 *
 * @ingroup SalCtrl
 *
 * @description
 *    This file contains the sal implementation for compression.
 *
 *****************************************************************************/

/* QAT-API includes */
#include "cpa.h"
#include "cpa_dc.h"

/* QAT utils includes */
#include "qat_utils.h"

/* ADF includes */
#include "icp_adf_init.h"
#include "icp_adf_transport.h"
#include "icp_accel_devices.h"
#include "icp_adf_cfg.h"
#include "icp_adf_accel_mgr.h"
#include "icp_adf_poll.h"
#include "icp_adf_debug.h"
#include "icp_adf_esram.h"
#include "icp_qat_hw.h"

/* SAL includes */
#include "lac_mem.h"
#include "lac_common.h"
#include "lac_mem_pools.h"
#include "sal_statistics.h"
#include "lac_list.h"
#include "icp_sal_poll.h"
#include "sal_types_compression.h"
#include "dc_session.h"
#include "dc_datapath.h"
#include "dc_stats.h"
#include "lac_sal.h"
#include "lac_sal_ctrl.h"
#include "sal_string_parse.h"
#include "sal_service_state.h"
#include "lac_buffer_desc.h"
#include "icp_qat_fw_comp.h"
#include "icp_sal_versions.h"

/* C string null terminator size */
#define SAL_NULL_TERM_SIZE 1

/* Type to access extended features bit fields */
typedef struct dc_extended_features_s {
	unsigned is_cnv : 1; /* Bit<0> */
	unsigned padding : 7;
	unsigned is_cnvnr : 1; /* Bit<8> */
	unsigned not_used : 23;
} dc_extd_ftrs_t;

/*
 * Prints statistics for a compression instance
 */
static int
SalCtrl_CompresionDebug(void *private_data, char *data, int size, int offset)
{
	sal_compression_service_t *pCompressionService =
	    (sal_compression_service_t *)private_data;
	CpaStatus status = CPA_STATUS_SUCCESS;
	CpaDcStats dcStats = { 0 };
	Cpa32S len = 0;

	status = cpaDcGetStats(pCompressionService, &dcStats);
	if (status != CPA_STATUS_SUCCESS) {
		QAT_UTILS_LOG("cpaDcGetStats returned error.\n");
		return (-1);
	}

	/* Engine Info */
	if (NULL != pCompressionService->debug_file) {
		len += snprintf(data + len,
				size - len,
				SEPARATOR BORDER
				" Statistics for Instance %24s | \n" SEPARATOR,
				pCompressionService->debug_file->name);
	}

	/* Perform Info */
	len += snprintf(data + len,
			size - len,
			BORDER " DC comp Requests:               %16llu " BORDER
			       "\n" BORDER
			       " DC comp Request Errors:         %16llu " BORDER
			       "\n" BORDER
			       " DC comp Completed:              %16llu " BORDER
			       "\n" BORDER
			       " DC comp Completed Errors:       %16llu " BORDER
			       "\n" SEPARATOR,
			(long long unsigned int)dcStats.numCompRequests,
			(long long unsigned int)dcStats.numCompRequestsErrors,
			(long long unsigned int)dcStats.numCompCompleted,
			(long long unsigned int)dcStats.numCompCompletedErrors);

	/* Perform Info */
	len += snprintf(
	    data + len,
	    size - len,
	    BORDER " DC decomp Requests:             %16llu " BORDER "\n" BORDER
		   " DC decomp Request Errors:       %16llu " BORDER "\n" BORDER
		   " DC decomp Completed:            %16llu " BORDER "\n" BORDER
		   " DC decomp Completed Errors:     %16llu " BORDER
		   "\n" SEPARATOR,
	    (long long unsigned int)dcStats.numDecompRequests,
	    (long long unsigned int)dcStats.numDecompRequestsErrors,
	    (long long unsigned int)dcStats.numDecompCompleted,
	    (long long unsigned int)dcStats.numDecompCompletedErrors);
	return 0;
}

/* Initialise device specific information needed by compression service */
static CpaStatus
SalCtrl_CompressionInit_CompData(icp_accel_dev_t *device,
				 sal_compression_service_t *pCompService)
{
	switch (device->deviceType) {
	case DEVICE_DH895XCC:
	case DEVICE_DH895XCCVF:
		pCompService->generic_service_info.integrityCrcCheck =
		    CPA_FALSE;
		pCompService->numInterBuffs =
		    DC_QAT_MAX_NUM_INTER_BUFFERS_6COMP_SLICES;
		pCompService->comp_device_data.minOutputBuffSize =
		    DC_DEST_BUFFER_STA_MIN_SIZE;
		pCompService->comp_device_data.oddByteDecompNobFinal = CPA_TRUE;
		pCompService->comp_device_data.oddByteDecompInterim = CPA_FALSE;
		pCompService->comp_device_data.translatorOverflow = CPA_FALSE;
		pCompService->comp_device_data.useDevRam =
		    ICP_QAT_FW_COMP_ENABLE_SECURE_RAM_USED_AS_INTMD_BUF;
		pCompService->comp_device_data.enableDmm =
		    ICP_QAT_HW_COMPRESSION_DELAYED_MATCH_DISABLED;

		pCompService->comp_device_data.inflateContextSize =
		    DC_INFLATE_CONTEXT_SIZE;
		pCompService->comp_device_data.highestHwCompressionDepth =
		    ICP_QAT_HW_COMPRESSION_DEPTH_16;

		pCompService->comp_device_data.windowSizeMask =
		    (1 << DC_8K_WINDOW_SIZE | 1 << DC_32K_WINDOW_SIZE);
		pCompService->comp_device_data.cnvnrSupported = CPA_FALSE;
		break;
	case DEVICE_C3XXX:
	case DEVICE_C3XXXVF:
	case DEVICE_200XX:
	case DEVICE_200XXVF:
		pCompService->generic_service_info.integrityCrcCheck =
		    CPA_FALSE;
		pCompService->numInterBuffs =
		    DC_QAT_MAX_NUM_INTER_BUFFERS_6COMP_SLICES;
		pCompService->comp_device_data.oddByteDecompNobFinal =
		    CPA_FALSE;
		pCompService->comp_device_data.oddByteDecompInterim = CPA_TRUE;
		pCompService->comp_device_data.translatorOverflow = CPA_FALSE;
		pCompService->comp_device_data.useDevRam =
		    ICP_QAT_FW_COMP_DISABLE_SECURE_RAM_USED_AS_INTMD_BUF;
		pCompService->comp_device_data.inflateContextSize =
		    DC_INFLATE_EH_CONTEXT_SIZE;
		pCompService->comp_device_data.highestHwCompressionDepth =
		    ICP_QAT_HW_COMPRESSION_DEPTH_16;
		pCompService->comp_device_data.windowSizeMask =
		    (1 << DC_16K_WINDOW_SIZE | 1 << DC_32K_WINDOW_SIZE);
		pCompService->comp_device_data.minOutputBuffSize =
		    DC_DEST_BUFFER_STA_MIN_SIZE;
		pCompService->comp_device_data.enableDmm =
		    ICP_QAT_HW_COMPRESSION_DELAYED_MATCH_ENABLED;

		pCompService->comp_device_data.cnvnrSupported = CPA_TRUE;
		break;
	case DEVICE_C62X:
	case DEVICE_C62XVF:
		pCompService->generic_service_info.integrityCrcCheck =
		    CPA_FALSE;
		pCompService->numInterBuffs =
		    DC_QAT_MAX_NUM_INTER_BUFFERS_10COMP_SLICES;
		pCompService->comp_device_data.oddByteDecompNobFinal =
		    CPA_FALSE;
		pCompService->comp_device_data.oddByteDecompInterim = CPA_TRUE;
		pCompService->comp_device_data.translatorOverflow = CPA_FALSE;
		pCompService->comp_device_data.useDevRam =
		    ICP_QAT_FW_COMP_ENABLE_SECURE_RAM_USED_AS_INTMD_BUF;
		pCompService->comp_device_data.inflateContextSize =
		    DC_INFLATE_EH_CONTEXT_SIZE;
		pCompService->comp_device_data.windowSizeMask =
		    (1 << DC_16K_WINDOW_SIZE | 1 << DC_32K_WINDOW_SIZE);
		pCompService->comp_device_data.minOutputBuffSize =
		    DC_DEST_BUFFER_STA_MIN_SIZE;
		pCompService->comp_device_data.enableDmm =
		    ICP_QAT_HW_COMPRESSION_DELAYED_MATCH_ENABLED;
		pCompService->comp_device_data.cnvnrSupported = CPA_TRUE;
		break;
	case DEVICE_C4XXX:
	case DEVICE_C4XXXVF:
		pCompService->generic_service_info.integrityCrcCheck = CPA_TRUE;
		pCompService->numInterBuffs =
		    DC_QAT_MAX_NUM_INTER_BUFFERS_24COMP_SLICES;
		pCompService->comp_device_data.minOutputBuffSize =
		    DC_DEST_BUFFER_MIN_SIZE;
		pCompService->comp_device_data.oddByteDecompNobFinal = CPA_TRUE;
		pCompService->comp_device_data.oddByteDecompInterim = CPA_TRUE;
		pCompService->comp_device_data.translatorOverflow = CPA_TRUE;
		if (pCompService->generic_service_info.capabilitiesMask &
		    ICP_ACCEL_CAPABILITIES_INLINE) {
			pCompService->comp_device_data.useDevRam =
			    ICP_QAT_FW_COMP_DISABLE_SECURE_RAM_USED_AS_INTMD_BUF;
		} else {
			pCompService->comp_device_data.useDevRam =
			    ICP_QAT_FW_COMP_ENABLE_SECURE_RAM_USED_AS_INTMD_BUF;
		}
		pCompService->comp_device_data.enableDmm =
		    ICP_QAT_HW_COMPRESSION_DELAYED_MATCH_ENABLED;
		pCompService->comp_device_data.inflateContextSize =
		    DC_INFLATE_EH_CONTEXT_SIZE;
		pCompService->comp_device_data.highestHwCompressionDepth =
		    ICP_QAT_HW_COMPRESSION_DEPTH_128;
		pCompService->comp_device_data.windowSizeMask =
		    (1 << DC_16K_WINDOW_SIZE | 1 << DC_32K_WINDOW_SIZE);
		pCompService->comp_device_data.cnvnrSupported = CPA_TRUE;
		break;
	default:
		QAT_UTILS_LOG("Unknown device type! - %d.\n",
			      device->deviceType);
		return CPA_STATUS_FAIL;
	}
	return CPA_STATUS_SUCCESS;
}

CpaStatus
SalCtrl_CompressionInit(icp_accel_dev_t *device, sal_service_t *service)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	Cpa32U numCompConcurrentReq = 0;
	Cpa32U request_ring_id = 0;
	Cpa32U response_ring_id = 0;

	char adfGetParam[ADF_CFG_MAX_VAL_LEN_IN_BYTES] = { 0 };
	char compMemPool[SAL_CFG_MAX_VAL_LEN_IN_BYTES] = { 0 };
	char temp_string[SAL_CFG_MAX_VAL_LEN_IN_BYTES] = { 0 };
	char temp_string2[SAL_CFG_MAX_VAL_LEN_IN_BYTES] = { 0 };
	char *instance_name = NULL;
	sal_statistics_collection_t *pStatsCollection =
	    (sal_statistics_collection_t *)device->pQatStats;
	icp_resp_deliv_method rx_resp_type = ICP_RESP_TYPE_IRQ;
	sal_compression_service_t *pCompressionService =
	    (sal_compression_service_t *)service;
	Cpa32U msgSize = 0;
	char *section = DYN_SEC;

	SAL_SERVICE_GOOD_FOR_INIT(pCompressionService);

	pCompressionService->generic_service_info.state =
	    SAL_SERVICE_STATE_INITIALIZING;

	if (CPA_FALSE == pCompressionService->generic_service_info.is_dyn) {
		section = icpGetProcessName();
	}

	if (pStatsCollection == NULL) {
		return CPA_STATUS_FAIL;
	}

	/* Get Config Info: Accel Num, bank Num, packageID,
				    coreAffinity, nodeAffinity and response mode
	   */

	pCompressionService->acceleratorNum = 0;

	/* Initialise device specific compression data */
	SalCtrl_CompressionInit_CompData(device, pCompressionService);

	status = Sal_StringParsing(
	    "Dc",
	    pCompressionService->generic_service_info.instance,
	    "BankNumber",
	    temp_string);
	LAC_CHECK_STATUS(status);
	status =
	    icp_adf_cfgGetParamValue(device, section, temp_string, adfGetParam);
	if (CPA_STATUS_SUCCESS != status) {
		QAT_UTILS_LOG("Failed to get %s from configuration.\n",
			      temp_string);
		return status;
	}

	pCompressionService->bankNum =
	    Sal_Strtoul(adfGetParam, NULL, SAL_CFG_BASE_DEC);

	status = Sal_StringParsing(
	    "Dc",
	    pCompressionService->generic_service_info.instance,
	    "IsPolled",
	    temp_string);
	LAC_CHECK_STATUS(status);
	status =
	    icp_adf_cfgGetParamValue(device, section, temp_string, adfGetParam);
	if (CPA_STATUS_SUCCESS != status) {
		QAT_UTILS_LOG("Failed to get %s from configuration.\n",
			      temp_string);
		return status;
	}
	pCompressionService->isPolled =
	    (Cpa8U)Sal_Strtoul(adfGetParam, NULL, SAL_CFG_BASE_DEC);

	/* User instances only support poll and epoll mode */
	if (SAL_RESP_POLL_CFG_FILE != pCompressionService->isPolled) {
		QAT_UTILS_LOG(
		    "IsPolled %u is not supported for user instance %s.\n",
		    pCompressionService->isPolled,
		    temp_string);
		return CPA_STATUS_FAIL;
	}

	if (SAL_RESP_POLL_CFG_FILE == pCompressionService->isPolled) {
		rx_resp_type = ICP_RESP_TYPE_POLL;
	}

	status = icp_adf_cfgGetParamValue(device,
					  LAC_CFG_SECTION_GENERAL,
					  ADF_DEV_PKG_ID,
					  adfGetParam);
	if (CPA_STATUS_SUCCESS != status) {
		QAT_UTILS_LOG("Failed to get %s from configuration.\n",
			      ADF_DEV_PKG_ID);
		return status;
	}
	pCompressionService->pkgID =
	    (Cpa16U)Sal_Strtoul(adfGetParam, NULL, SAL_CFG_BASE_DEC);

	status = icp_adf_cfgGetParamValue(device,
					  LAC_CFG_SECTION_GENERAL,
					  ADF_DEV_NODE_ID,
					  adfGetParam);
	if (CPA_STATUS_SUCCESS != status) {
		QAT_UTILS_LOG("Failed to get %s from configuration.\n",
			      ADF_DEV_NODE_ID);
		return status;
	}
	pCompressionService->nodeAffinity =
	    (Cpa32U)Sal_Strtoul(adfGetParam, NULL, SAL_CFG_BASE_DEC);

	/* In case of interrupt instance, use the bank affinity set by adf_ctl
	 * Otherwise, use the instance affinity for backwards compatibility */
	if (SAL_RESP_POLL_CFG_FILE != pCompressionService->isPolled) {
		/* Next need to read the [AcceleratorX] section of the config
		 * file */
		status = Sal_StringParsing("Accelerator",
					   pCompressionService->acceleratorNum,
					   "",
					   temp_string2);
		LAC_CHECK_STATUS(status);

		status = Sal_StringParsing("Bank",
					   pCompressionService->bankNum,
					   "CoreAffinity",
					   temp_string);
		LAC_CHECK_STATUS(status);
	} else {
		strncpy(temp_string2,
			section,
			sizeof(temp_string2) - SAL_NULL_TERM_SIZE);
		temp_string2[SAL_CFG_MAX_VAL_LEN_IN_BYTES -
			     SAL_NULL_TERM_SIZE] = '\0';

		status = Sal_StringParsing(
		    "Dc",
		    pCompressionService->generic_service_info.instance,
		    "CoreAffinity",
		    temp_string);
		LAC_CHECK_STATUS(status);
	}

	status = icp_adf_cfgGetParamValue(device,
					  temp_string2,
					  temp_string,
					  adfGetParam);
	if (CPA_STATUS_SUCCESS != status) {
		QAT_UTILS_LOG("Failed to get %s from configuration.\n",
			      temp_string);
		return status;
	}
	pCompressionService->coreAffinity =
	    (Cpa32U)Sal_Strtoul(adfGetParam, NULL, SAL_CFG_BASE_DEC);

	status = Sal_StringParsing(
	    "Dc",
	    pCompressionService->generic_service_info.instance,
	    "NumConcurrentRequests",
	    temp_string);
	LAC_CHECK_STATUS(status);
	status =
	    icp_adf_cfgGetParamValue(device, section, temp_string, adfGetParam);
	if (CPA_STATUS_SUCCESS != status) {
		QAT_UTILS_LOG("Failed to get %s from configuration.\n",
			      temp_string);
		return status;
	}

	numCompConcurrentReq =
	    (Cpa32U)Sal_Strtoul(adfGetParam, NULL, SAL_CFG_BASE_DEC);
	if (validateConcurrRequest(numCompConcurrentReq)) {
		QAT_UTILS_LOG(
		    "Invalid NumConcurrentRequests, valid values are: {64, 128, 256, ... 32768, 65536}.\n");
		return CPA_STATUS_FAIL;
	}

	/* ADF does not allow us to completely fill the ring for batch requests
	 */
	pCompressionService->maxNumCompConcurrentReq =
	    (numCompConcurrentReq - SAL_BATCH_SUBMIT_FREE_SPACE);

	/* 1. Create transport handles */
	status = Sal_StringParsing(
	    "Dc",
	    pCompressionService->generic_service_info.instance,
	    "RingTx",
	    temp_string);
	LAC_CHECK_STATUS(status);

	msgSize = LAC_QAT_DC_REQ_SZ_LW * LAC_LONG_WORD_IN_BYTES;
	status = icp_adf_transCreateHandle(
	    device,
	    ICP_TRANS_TYPE_ETR,
	    section,
	    pCompressionService->acceleratorNum,
	    pCompressionService->bankNum,
	    temp_string,
	    lac_getRingType(SAL_RING_TYPE_DC),
	    NULL,
	    ICP_RESP_TYPE_NONE,
	    numCompConcurrentReq,
	    msgSize,
	    (icp_comms_trans_handle *)&(
		pCompressionService->trans_handle_compression_tx));
	LAC_CHECK_STATUS(status);

	if (icp_adf_transGetRingNum(
		pCompressionService->trans_handle_compression_tx,
		&request_ring_id) != CPA_STATUS_SUCCESS) {
		icp_adf_transReleaseHandle(
		    pCompressionService->trans_handle_compression_tx);

		QAT_UTILS_LOG("Failed to get DC TX ring number.\n");
		return CPA_STATUS_FAIL;
	}

	status = Sal_StringParsing(
	    "Dc",
	    pCompressionService->generic_service_info.instance,
	    "RingRx",
	    temp_string);
	if (CPA_STATUS_SUCCESS != status) {
		icp_adf_transReleaseHandle(
		    pCompressionService->trans_handle_compression_tx);
		return status;
	}

	msgSize = LAC_QAT_DC_RESP_SZ_LW * LAC_LONG_WORD_IN_BYTES;
	status = icp_adf_transCreateHandle(
	    device,
	    ICP_TRANS_TYPE_ETR,
	    section,
	    pCompressionService->acceleratorNum,
	    pCompressionService->bankNum,
	    temp_string,
	    lac_getRingType(SAL_RING_TYPE_NONE),
	    (icp_trans_callback)dcCompression_ProcessCallback,
	    rx_resp_type,
	    numCompConcurrentReq,
	    msgSize,
	    (icp_comms_trans_handle *)&(
		pCompressionService->trans_handle_compression_rx));
	if (CPA_STATUS_SUCCESS != status) {
		icp_adf_transReleaseHandle(
		    pCompressionService->trans_handle_compression_tx);
		return status;
	}

	if (icp_adf_transGetRingNum(
		pCompressionService->trans_handle_compression_rx,
		&response_ring_id) != CPA_STATUS_SUCCESS) {
		icp_adf_transReleaseHandle(
		    pCompressionService->trans_handle_compression_tx);

		icp_adf_transReleaseHandle(
		    pCompressionService->trans_handle_compression_rx);

		QAT_UTILS_LOG("Failed to get DC RX ring number.\n");
		return CPA_STATUS_FAIL;
	}

	/* 2. Allocates memory pools */

	/* Valid initialisation value for a pool ID */
	pCompressionService->compression_mem_pool = LAC_MEM_POOL_INIT_POOL_ID;

	status = Sal_StringParsing(
	    "Comp",
	    pCompressionService->generic_service_info.instance,
	    "_MemPool",
	    compMemPool);
	if (CPA_STATUS_SUCCESS != status) {
		icp_adf_transReleaseHandle(
		    pCompressionService->trans_handle_compression_tx);

		icp_adf_transReleaseHandle(
		    pCompressionService->trans_handle_compression_rx);

		return status;
	}

	status = Lac_MemPoolCreate(&pCompressionService->compression_mem_pool,
				   compMemPool,
				   (numCompConcurrentReq + 1),
				   sizeof(dc_compression_cookie_t),
				   LAC_64BYTE_ALIGNMENT,
				   CPA_FALSE,
				   pCompressionService->nodeAffinity);
	if (CPA_STATUS_SUCCESS != status) {
		icp_adf_transReleaseHandle(
		    pCompressionService->trans_handle_compression_tx);

		icp_adf_transReleaseHandle(
		    pCompressionService->trans_handle_compression_rx);

		return status;
	}

	/* Init compression statistics */
	status = dcStatsInit(pCompressionService);
	if (CPA_STATUS_SUCCESS != status) {
		Lac_MemPoolDestroy(pCompressionService->compression_mem_pool);

		icp_adf_transReleaseHandle(
		    pCompressionService->trans_handle_compression_tx);

		icp_adf_transReleaseHandle(
		    pCompressionService->trans_handle_compression_rx);

		return status;
	}
	if (CPA_TRUE == pStatsCollection->bDcStatsEnabled) {
		/* Get instance name for stats */
		instance_name = LAC_OS_MALLOC(ADF_CFG_MAX_VAL_LEN_IN_BYTES);
		if (NULL == instance_name) {
			Lac_MemPoolDestroy(
			    pCompressionService->compression_mem_pool);

			icp_adf_transReleaseHandle(
			    pCompressionService->trans_handle_compression_tx);

			icp_adf_transReleaseHandle(
			    pCompressionService->trans_handle_compression_rx);

			return CPA_STATUS_RESOURCE;
		}

		status = Sal_StringParsing(
		    "Dc",
		    pCompressionService->generic_service_info.instance,
		    "Name",
		    temp_string);
		if (CPA_STATUS_SUCCESS != status) {
			Lac_MemPoolDestroy(
			    pCompressionService->compression_mem_pool);

			icp_adf_transReleaseHandle(
			    pCompressionService->trans_handle_compression_tx);

			icp_adf_transReleaseHandle(
			    pCompressionService->trans_handle_compression_rx);
			LAC_OS_FREE(instance_name);
			return status;
		}
		status = icp_adf_cfgGetParamValue(device,
						  section,
						  temp_string,
						  adfGetParam);
		if (CPA_STATUS_SUCCESS != status) {
			QAT_UTILS_LOG("Failed to get %s from configuration.\n",
				      temp_string);

			Lac_MemPoolDestroy(
			    pCompressionService->compression_mem_pool);

			icp_adf_transReleaseHandle(
			    pCompressionService->trans_handle_compression_tx);

			icp_adf_transReleaseHandle(
			    pCompressionService->trans_handle_compression_rx);
			LAC_OS_FREE(instance_name);
			return status;
		}

		snprintf(instance_name,
			 ADF_CFG_MAX_VAL_LEN_IN_BYTES,
			 "%s",
			 adfGetParam);

		pCompressionService->debug_file =
		    LAC_OS_MALLOC(sizeof(debug_file_info_t));
		if (NULL == pCompressionService->debug_file) {
			Lac_MemPoolDestroy(
			    pCompressionService->compression_mem_pool);

			icp_adf_transReleaseHandle(
			    pCompressionService->trans_handle_compression_tx);

			icp_adf_transReleaseHandle(
			    pCompressionService->trans_handle_compression_rx);
			LAC_OS_FREE(instance_name);
			return CPA_STATUS_RESOURCE;
		}

		memset(pCompressionService->debug_file,
		       0,
		       sizeof(debug_file_info_t));
		pCompressionService->debug_file->name = instance_name;
		pCompressionService->debug_file->seq_read =
		    SalCtrl_CompresionDebug;
		pCompressionService->debug_file->private_data =
		    pCompressionService;
		pCompressionService->debug_file->parent =
		    pCompressionService->generic_service_info.debug_parent_dir;

		status = icp_adf_debugAddFile(device,
					      pCompressionService->debug_file);
		if (CPA_STATUS_SUCCESS != status) {
			Lac_MemPoolDestroy(
			    pCompressionService->compression_mem_pool);

			icp_adf_transReleaseHandle(
			    pCompressionService->trans_handle_compression_tx);

			icp_adf_transReleaseHandle(
			    pCompressionService->trans_handle_compression_rx);
			LAC_OS_FREE(instance_name);
			LAC_OS_FREE(pCompressionService->debug_file);
			return status;
		}
	}
	pCompressionService->generic_service_info.stats = pStatsCollection;
	pCompressionService->generic_service_info.state =
	    SAL_SERVICE_STATE_INITIALIZED;

	return status;
}

CpaStatus
SalCtrl_CompressionStart(icp_accel_dev_t *device, sal_service_t *service)
{
	CpaStatus status = CPA_STATUS_SUCCESS;

	sal_compression_service_t *pCompressionService =
	    (sal_compression_service_t *)service;

	if (SAL_SERVICE_STATE_INITIALIZED !=
	    pCompressionService->generic_service_info.state) {
		QAT_UTILS_LOG("Not in the correct state to call start.\n");
		return CPA_STATUS_FAIL;
	}
	/**************************************************************/
	/* Obtain Extended Features. I.e. Compress And Verify         */
	/**************************************************************/
	pCompressionService->generic_service_info.dcExtendedFeatures =
	    device->dcExtendedFeatures;
	pCompressionService->generic_service_info.state =
	    SAL_SERVICE_STATE_RUNNING;

	return status;
}

CpaStatus
SalCtrl_CompressionStop(icp_accel_dev_t *device, sal_service_t *service)
{
	sal_compression_service_t *pCompressionService =
	    (sal_compression_service_t *)service;

	if (SAL_SERVICE_STATE_RUNNING !=
	    pCompressionService->generic_service_info.state) {
		QAT_UTILS_LOG("Not in the correct state to call stop.\n");
		return CPA_STATUS_FAIL;
	}

	if (icp_adf_is_dev_in_reset(device)) {
		pCompressionService->generic_service_info.state =
		    SAL_SERVICE_STATE_RESTARTING;
		return CPA_STATUS_SUCCESS;
	}

	pCompressionService->generic_service_info.state =
	    SAL_SERVICE_STATE_SHUTTING_DOWN;
	return CPA_STATUS_RETRY;
}

CpaStatus
SalCtrl_CompressionShutdown(icp_accel_dev_t *device, sal_service_t *service)
{
	CpaStatus status = CPA_STATUS_SUCCESS;

	sal_compression_service_t *pCompressionService =
	    (sal_compression_service_t *)service;
	sal_statistics_collection_t *pStatsCollection =
	    (sal_statistics_collection_t *)device->pQatStats;

	if ((SAL_SERVICE_STATE_INITIALIZED !=
	     pCompressionService->generic_service_info.state) &&
	    (SAL_SERVICE_STATE_SHUTTING_DOWN !=
	     pCompressionService->generic_service_info.state) &&
	    (SAL_SERVICE_STATE_RESTARTING !=
	     pCompressionService->generic_service_info.state)) {
		QAT_UTILS_LOG("Not in the correct state to call shutdown.\n");
		return CPA_STATUS_FAIL;
	}

	Lac_MemPoolDestroy(pCompressionService->compression_mem_pool);

	status = icp_adf_transReleaseHandle(
	    pCompressionService->trans_handle_compression_tx);
	LAC_CHECK_STATUS(status);

	status = icp_adf_transReleaseHandle(
	    pCompressionService->trans_handle_compression_rx);
	LAC_CHECK_STATUS(status);

	if (CPA_TRUE == pStatsCollection->bDcStatsEnabled) {
		/* Clean stats */
		if (NULL != pCompressionService->debug_file) {
			icp_adf_debugRemoveFile(
			    pCompressionService->debug_file);
			LAC_OS_FREE(pCompressionService->debug_file->name);
			LAC_OS_FREE(pCompressionService->debug_file);
			pCompressionService->debug_file = NULL;
		}
	}
	pCompressionService->generic_service_info.stats = NULL;
	dcStatsFree(pCompressionService);

	if (icp_adf_is_dev_in_reset(device)) {
		pCompressionService->generic_service_info.state =
		    SAL_SERVICE_STATE_RESTARTING;
		return CPA_STATUS_SUCCESS;
	}
	pCompressionService->generic_service_info.state =
	    SAL_SERVICE_STATE_SHUTDOWN;
	return status;
}

CpaStatus
cpaDcGetStatusText(const CpaInstanceHandle dcInstance,
		   const CpaStatus errStatus,
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

CpaStatus
cpaDcGetNumIntermediateBuffers(CpaInstanceHandle dcInstance,
			       Cpa16U *pNumBuffers)
{
	CpaInstanceHandle insHandle = NULL;
	sal_compression_service_t *pService = NULL;

	if (CPA_INSTANCE_HANDLE_SINGLE == dcInstance) {
		insHandle = dcGetFirstHandle();
	} else {
		insHandle = dcInstance;
	}

	LAC_CHECK_NULL_PARAM(insHandle);
	LAC_CHECK_NULL_PARAM(pNumBuffers);

	pService = (sal_compression_service_t *)insHandle;
	*pNumBuffers = pService->numInterBuffs;

	return CPA_STATUS_SUCCESS;
}

CpaStatus
cpaDcStartInstance(CpaInstanceHandle instanceHandle,
		   Cpa16U numBuffers,
		   CpaBufferList **pIntermediateBufferPtrsArray)
{
	icp_qat_addr_width_t *pInterBuffPtrsArray = NULL;
	icp_qat_addr_width_t pArrayBufferListDescPhyAddr = 0;
	icp_qat_addr_width_t bufListDescPhyAddr;
	icp_qat_addr_width_t bufListAlignedPhyAddr;
	CpaFlatBuffer *pClientCurrFlatBuffer = NULL;
	icp_buffer_list_desc_t *pBufferListDesc = NULL;
	icp_flat_buffer_desc_t *pCurrFlatBufDesc = NULL;
	CpaInstanceInfo2 info = { 0 };
	icp_accel_dev_t *dev = NULL;
	CpaStatus status = CPA_STATUS_SUCCESS;
	sal_compression_service_t *pService = NULL;
	CpaInstanceHandle insHandle = NULL;
	Cpa16U bufferIndex = 0;
	Cpa32U numFlatBuffers = 0;
	Cpa64U clientListSize = 0;
	CpaBufferList *pClientCurrentIntermediateBuffer = NULL;
	Cpa32U bufferIndex2 = 0;
	CpaBufferList **pTempIntermediateBufferPtrsArray;
	Cpa64U lastClientListSize = 0;

	if (CPA_INSTANCE_HANDLE_SINGLE == instanceHandle) {
		insHandle = dcGetFirstHandle();
	} else {
		insHandle = instanceHandle;
	}
	LAC_CHECK_NULL_PARAM(insHandle);

	status = cpaDcInstanceGetInfo2(insHandle, &info);
	if (CPA_STATUS_SUCCESS != status) {
		QAT_UTILS_LOG("Can not get instance info.\n");
		return status;
	}

	dev = icp_adf_getAccelDevByAccelId(info.physInstId.packageId);
	if (NULL == dev) {
		QAT_UTILS_LOG("Can not find device for the instance\n");
		return CPA_STATUS_FAIL;
	}

	if (NULL == pIntermediateBufferPtrsArray) {
		/* Increment dev ref counter and return - DRAM is not used */
		icp_qa_dev_get(dev);
		return CPA_STATUS_SUCCESS;
	}

	if (0 == numBuffers) {
		/* Increment dev ref counter and return - DRAM is not used */
		icp_qa_dev_get(dev);
		return CPA_STATUS_SUCCESS;
	}

	pService = (sal_compression_service_t *)insHandle;

	LAC_CHECK_NULL_PARAM(insHandle);

	if ((numBuffers > 0) && (NULL == pIntermediateBufferPtrsArray)) {
		QAT_UTILS_LOG("Invalid Intermediate Buffers Array pointer\n");
		return CPA_STATUS_INVALID_PARAM;
	}

	/* Check number of intermediate buffers allocated by user */
	if ((pService->numInterBuffs != numBuffers)) {
		QAT_UTILS_LOG("Invalid number of buffers\n");
		return CPA_STATUS_INVALID_PARAM;
	}

	pTempIntermediateBufferPtrsArray = pIntermediateBufferPtrsArray;
	for (bufferIndex = 0; bufferIndex < numBuffers; bufferIndex++) {
		if (NULL == *pTempIntermediateBufferPtrsArray) {
			QAT_UTILS_LOG(
			    "Intermediate Buffer - Invalid Buffer List pointer\n");
			return CPA_STATUS_INVALID_PARAM;
		}

		if (NULL == (*pTempIntermediateBufferPtrsArray)->pBuffers) {
			QAT_UTILS_LOG(
			    "Intermediate Buffer - Invalid Flat Buffer descriptor pointer\n");
			return CPA_STATUS_INVALID_PARAM;
		}

		if (NULL ==
		    (*pTempIntermediateBufferPtrsArray)->pPrivateMetaData) {
			QAT_UTILS_LOG(
			    "Intermediate Buffer - Invalid Private MetaData descriptor pointer\n");
			return CPA_STATUS_INVALID_PARAM;
		}

		clientListSize = 0;
		for (bufferIndex2 = 0; bufferIndex2 <
		     (*pTempIntermediateBufferPtrsArray)->numBuffers;
		     bufferIndex2++) {

			if ((0 !=
			     (*pTempIntermediateBufferPtrsArray)
				 ->pBuffers[bufferIndex2]
				 .dataLenInBytes) &&
			    NULL ==
				(*pTempIntermediateBufferPtrsArray)
				    ->pBuffers[bufferIndex2]
				    .pData) {
				QAT_UTILS_LOG(
				    "Intermediate Buffer - Invalid Flat Buffer pointer\n");
				return CPA_STATUS_INVALID_PARAM;
			}

			clientListSize += (*pTempIntermediateBufferPtrsArray)
					      ->pBuffers[bufferIndex2]
					      .dataLenInBytes;
		}

		if (bufferIndex != 0) {
			if (lastClientListSize != clientListSize) {
				QAT_UTILS_LOG(
				    "SGLs have to be of the same size.\n");
				return CPA_STATUS_INVALID_PARAM;
			}
		} else {
			lastClientListSize = clientListSize;
		}
		pTempIntermediateBufferPtrsArray++;
	}

	/* Allocate array of physical pointers to icp_buffer_list_desc_t */
	status = LAC_OS_CAMALLOC(&pInterBuffPtrsArray,
				 (numBuffers * sizeof(icp_qat_addr_width_t)),
				 LAC_64BYTE_ALIGNMENT,
				 pService->nodeAffinity);
	if (CPA_STATUS_SUCCESS != status) {
		QAT_UTILS_LOG("Can not allocate Intermediate Buffers array.\n");
		return status;
	}

	/* Get physical address of the intermediate buffer pointers array */
	pArrayBufferListDescPhyAddr = LAC_MEM_CAST_PTR_TO_UINT64(
	    LAC_OS_VIRT_TO_PHYS_INTERNAL(pInterBuffPtrsArray));

	pService->pInterBuffPtrsArray = pInterBuffPtrsArray;
	pService->pInterBuffPtrsArrayPhyAddr = pArrayBufferListDescPhyAddr;

	/* Get the full size of the buffer list */
	/* Assumption: all the SGLs allocated by the user have the same size */
	clientListSize = 0;
	for (bufferIndex = 0;
	     bufferIndex < (*pIntermediateBufferPtrsArray)->numBuffers;
	     bufferIndex++) {
		clientListSize += ((*pIntermediateBufferPtrsArray)
				       ->pBuffers[bufferIndex]
				       .dataLenInBytes);
	}
	pService->minInterBuffSizeInBytes = clientListSize;

	for (bufferIndex = 0; bufferIndex < numBuffers; bufferIndex++) {

		/* Get pointer to the client Intermediate Buffer List
		 * (CpaBufferList) */
		pClientCurrentIntermediateBuffer =
		    *pIntermediateBufferPtrsArray;

		/* Get number of flat buffers in the buffer list */
		numFlatBuffers = pClientCurrentIntermediateBuffer->numBuffers;

		/* Get pointer to the client array of CpaFlatBuffers */
		pClientCurrFlatBuffer =
		    pClientCurrentIntermediateBuffer->pBuffers;

		/* Calculate Physical address of current private SGL */
		bufListDescPhyAddr = LAC_OS_VIRT_TO_PHYS_EXTERNAL(
		    (*pService),
		    pClientCurrentIntermediateBuffer->pPrivateMetaData);
		if (bufListDescPhyAddr == 0) {
			QAT_UTILS_LOG(
			    "Unable to get the physical address of the metadata.\n");
			return CPA_STATUS_FAIL;
		}

		/* Align SGL physical address */
		bufListAlignedPhyAddr =
		    LAC_ALIGN_POW2_ROUNDUP(bufListDescPhyAddr,
					   ICP_DESCRIPTOR_ALIGNMENT_BYTES);

		/* Set physical address of the Intermediate Buffer SGL in the
		 * SGLs array
		 */
		*pInterBuffPtrsArray =
		    LAC_MEM_CAST_PTR_TO_UINT64(bufListAlignedPhyAddr);

		/* Calculate (virtual) offset to the buffer list descriptor */
		pBufferListDesc =
		    (icp_buffer_list_desc_t
			 *)((LAC_ARCH_UINT)pClientCurrentIntermediateBuffer
				->pPrivateMetaData +
			    (LAC_ARCH_UINT)(bufListAlignedPhyAddr -
					    bufListDescPhyAddr));

		/* Set number of flat buffers in the physical Buffer List
		 * descriptor */
		pBufferListDesc->numBuffers = numFlatBuffers;

		/* Go past the Buffer List descriptor to the list of buffer
		 * descriptors
		 */
		pCurrFlatBufDesc =
		    (icp_flat_buffer_desc_t *)((pBufferListDesc->phyBuffers));

		/* Loop for each flat buffer in the SGL */
		while (0 != numFlatBuffers) {
			/* Set length of the current flat buffer */
			pCurrFlatBufDesc->dataLenInBytes =
			    pClientCurrFlatBuffer->dataLenInBytes;

			/* Set physical address of the flat buffer */
			pCurrFlatBufDesc->phyBuffer =
			    LAC_MEM_CAST_PTR_TO_UINT64(
				LAC_OS_VIRT_TO_PHYS_EXTERNAL(
				    (*pService), pClientCurrFlatBuffer->pData));

			if (pCurrFlatBufDesc->phyBuffer == 0) {
				QAT_UTILS_LOG(
				    "Unable to get the physical address of the flat buffer.\n");
				return CPA_STATUS_FAIL;
			}

			pCurrFlatBufDesc++;
			pClientCurrFlatBuffer++;
			numFlatBuffers--;
		}
		pIntermediateBufferPtrsArray++;
		pInterBuffPtrsArray++;
	}

	pService->generic_service_info.isInstanceStarted = CPA_TRUE;

	/* Increment dev ref counter */
	icp_qa_dev_get(dev);
	return CPA_STATUS_SUCCESS;
}

CpaStatus
cpaDcStopInstance(CpaInstanceHandle instanceHandle)
{
	CpaInstanceHandle insHandle = NULL;
	CpaInstanceInfo2 info = { 0 };
	icp_accel_dev_t *dev = NULL;
	CpaStatus status = CPA_STATUS_SUCCESS;
	sal_compression_service_t *pService = NULL;

	if (CPA_INSTANCE_HANDLE_SINGLE == instanceHandle) {
		insHandle = dcGetFirstHandle();
	} else {
		insHandle = instanceHandle;
	}

	LAC_CHECK_NULL_PARAM(insHandle);
	pService = (sal_compression_service_t *)insHandle;

	/* Free Intermediate Buffer Pointers Array */
	if (pService->pInterBuffPtrsArray != NULL) {
		LAC_OS_CAFREE(pService->pInterBuffPtrsArray);
		pService->pInterBuffPtrsArray = 0;
	}

	pService->pInterBuffPtrsArrayPhyAddr = 0;

	status = cpaDcInstanceGetInfo2(insHandle, &info);
	if (CPA_STATUS_SUCCESS != status) {
		QAT_UTILS_LOG("Can not get instance info.\n");
		return status;
	}
	dev = icp_adf_getAccelDevByAccelId(info.physInstId.packageId);
	if (NULL == dev) {
		QAT_UTILS_LOG("Can not find device for the instance.\n");
		return CPA_STATUS_FAIL;
	}

	pService->generic_service_info.isInstanceStarted = CPA_FALSE;

	/* Decrement dev ref counter */
	icp_qa_dev_put(dev);
	return CPA_STATUS_SUCCESS;
}

CpaStatus
cpaDcGetNumInstances(Cpa16U *pNumInstances)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	icp_accel_dev_t **pAdfInsts = NULL;
	icp_accel_dev_t *dev_addr = NULL;
	sal_t *base_addr = NULL;
	sal_list_t *list_temp = NULL;
	Cpa16U num_accel_dev = 0;
	Cpa16U num = 0;
	Cpa16U i = 0;

	LAC_CHECK_NULL_PARAM(pNumInstances);

	/* Get the number of accel_dev in the system */
	status = icp_amgr_getNumInstances(&num_accel_dev);
	LAC_CHECK_STATUS(status);

	/* Allocate memory to store addr of accel_devs */
	pAdfInsts =
	    malloc(num_accel_dev * sizeof(icp_accel_dev_t *), M_QAT, M_WAITOK);
	num_accel_dev = 0;

	/* Get ADF to return accel_devs with dc enabled */
	status = icp_amgr_getAllAccelDevByCapabilities(
	    ICP_ACCEL_CAPABILITIES_COMPRESSION, pAdfInsts, &num_accel_dev);
	if (CPA_STATUS_SUCCESS == status) {
		for (i = 0; i < num_accel_dev; i++) {
			dev_addr = (icp_accel_dev_t *)pAdfInsts[i];
			if (NULL != dev_addr) {
				base_addr = dev_addr->pSalHandle;
				if (NULL != base_addr) {
					list_temp =
					    base_addr->compression_services;
					while (NULL != list_temp) {
						num++;
						list_temp =
						    SalList_next(list_temp);
					}
				}
			}
		}

		*pNumInstances = num;
	}

	free(pAdfInsts, M_QAT);

	return status;
}

CpaStatus
cpaDcGetInstances(Cpa16U numInstances, CpaInstanceHandle *dcInstances)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	icp_accel_dev_t **pAdfInsts = NULL;
	icp_accel_dev_t *dev_addr = NULL;
	sal_t *base_addr = NULL;
	sal_list_t *list_temp = NULL;
	Cpa16U num_accel_dev = 0;
	Cpa16U index = 0;
	Cpa16U i = 0;

	LAC_CHECK_NULL_PARAM(dcInstances);
	if (0 == numInstances) {
		QAT_UTILS_LOG("numInstances is 0.\n");
		return CPA_STATUS_INVALID_PARAM;
	}

	/* Get the number of accel_dev in the system */
	status = icp_amgr_getNumInstances(&num_accel_dev);
	LAC_CHECK_STATUS(status);

	/* Allocate memory to store addr of accel_devs */
	pAdfInsts =
	    malloc(num_accel_dev * sizeof(icp_accel_dev_t *), M_QAT, M_WAITOK);

	num_accel_dev = 0;
	/* Get ADF to return accel_devs with dc enabled */
	status = icp_amgr_getAllAccelDevByCapabilities(
	    ICP_ACCEL_CAPABILITIES_COMPRESSION, pAdfInsts, &num_accel_dev);

	if (CPA_STATUS_SUCCESS == status) {
		/* First check the number of instances in the system */
		for (i = 0; i < num_accel_dev; i++) {
			dev_addr = (icp_accel_dev_t *)pAdfInsts[i];
			if (NULL != dev_addr) {
				base_addr = dev_addr->pSalHandle;
				if (NULL != base_addr) {
					list_temp =
					    base_addr->compression_services;
					while (NULL != list_temp) {
						if (index >
						    (numInstances - 1)) {
							break;
						}

						dcInstances[index] =
						    SalList_getObject(
							list_temp);
						list_temp =
						    SalList_next(list_temp);
						index++;
					}
				}
			}
		}

		if (numInstances > index) {
			QAT_UTILS_LOG("Only %d dc instances available.\n",
				      index);
			status = CPA_STATUS_RESOURCE;
		}
	}

	if (CPA_STATUS_SUCCESS == status) {
		index = 0;
		for (i = 0; i < num_accel_dev; i++) {
			dev_addr = (icp_accel_dev_t *)pAdfInsts[i];
			/* Note dev_addr cannot be NULL here as numInstances=0
			   is not valid and if dev_addr=NULL then index=0 (which
			   is less than numInstances and status is set to
			   _RESOURCE
			   above */
			base_addr = dev_addr->pSalHandle;
			if (NULL != base_addr) {
				list_temp = base_addr->compression_services;
				while (NULL != list_temp) {
					if (index > (numInstances - 1)) {
						break;
					}

					dcInstances[index] =
					    SalList_getObject(list_temp);
					list_temp = SalList_next(list_temp);
					index++;
				}
			}
		}
	}

	free(pAdfInsts, M_QAT);

	return status;
}

CpaStatus
cpaDcInstanceGetInfo2(const CpaInstanceHandle instanceHandle,
		      CpaInstanceInfo2 *pInstanceInfo2)
{
	sal_compression_service_t *pCompressionService = NULL;
	CpaInstanceHandle insHandle = NULL;
	icp_accel_dev_t *dev = NULL;
	CpaStatus status = CPA_STATUS_SUCCESS;
	char keyStr[ADF_CFG_MAX_KEY_LEN_IN_BYTES] = { 0 };
	char valStr[ADF_CFG_MAX_VAL_LEN_IN_BYTES] = { 0 };
	char *section = DYN_SEC;

	if (CPA_INSTANCE_HANDLE_SINGLE == instanceHandle) {
		insHandle = dcGetFirstHandle();
	} else {
		insHandle = instanceHandle;
	}

	LAC_CHECK_NULL_PARAM(insHandle);
	SAL_CHECK_INSTANCE_TYPE(insHandle, SAL_SERVICE_TYPE_COMPRESSION);
	LAC_CHECK_NULL_PARAM(pInstanceInfo2);

	LAC_OS_BZERO(pInstanceInfo2, sizeof(CpaInstanceInfo2));
	pInstanceInfo2->accelerationServiceType =
	    CPA_ACC_SVC_TYPE_DATA_COMPRESSION;

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

	/* Note we can safely read the contents of the compression service
	   instance
	   here because icp_amgr_getAccelDevByCapabilities() only returns devs
	   that have started */
	pCompressionService = (sal_compression_service_t *)insHandle;
	pInstanceInfo2->physInstId.packageId = pCompressionService->pkgID;
	pInstanceInfo2->physInstId.acceleratorId =
	    pCompressionService->acceleratorNum;
	pInstanceInfo2->physInstId.executionEngineId = 0;
	pInstanceInfo2->physInstId.busAddress =
	    icp_adf_get_busAddress(pInstanceInfo2->physInstId.packageId);

	/* set coreAffinity to zero before use */
	LAC_OS_BZERO(pInstanceInfo2->coreAffinity,
		     sizeof(pInstanceInfo2->coreAffinity));
	CPA_BITMAP_BIT_SET(pInstanceInfo2->coreAffinity,
			   pCompressionService->coreAffinity);

	pInstanceInfo2->nodeAffinity = pCompressionService->nodeAffinity;

	if (CPA_TRUE ==
	    pCompressionService->generic_service_info.isInstanceStarted) {
		pInstanceInfo2->operState = CPA_OPER_STATE_UP;
	} else {
		pInstanceInfo2->operState = CPA_OPER_STATE_DOWN;
	}

	pInstanceInfo2->requiresPhysicallyContiguousMemory = CPA_TRUE;

	if (SAL_RESP_POLL_CFG_FILE == pCompressionService->isPolled) {
		pInstanceInfo2->isPolled = CPA_TRUE;
	} else {
		pInstanceInfo2->isPolled = CPA_FALSE;
	}

	pInstanceInfo2->isOffloaded = CPA_TRUE;
	/* Get the instance name and part name from the config file */
	dev = icp_adf_getAccelDevByAccelId(pCompressionService->pkgID);
	if (NULL == dev) {
		QAT_UTILS_LOG("Can not find device for the instance.\n");
		LAC_OS_BZERO(pInstanceInfo2, sizeof(CpaInstanceInfo2));
		return CPA_STATUS_FAIL;
	}
	snprintf((char *)pInstanceInfo2->partName,
		 CPA_INST_PART_NAME_SIZE,
		 SAL_INFO2_PART_NAME,
		 dev->deviceName);
	pInstanceInfo2->partName[CPA_INST_PART_NAME_SIZE - 1] = '\0';

	if (CPA_FALSE == pCompressionService->generic_service_info.is_dyn) {
		section = icpGetProcessName();
	}

	status = Sal_StringParsing(
	    "Dc",
	    pCompressionService->generic_service_info.instance,
	    "Name",
	    keyStr);
	LAC_CHECK_STATUS(status);
	status = icp_adf_cfgGetParamValue(dev, section, keyStr, valStr);
	LAC_CHECK_STATUS(status);
	strncpy((char *)pInstanceInfo2->instName,
		valStr,
		sizeof(pInstanceInfo2->instName) - 1);
	pInstanceInfo2->instName[CPA_INST_NAME_SIZE - 1] = '\0';

#if __GNUC__ >= 7
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif
	snprintf((char *)pInstanceInfo2->instID,
		 CPA_INST_ID_SIZE,
		 "%s_%s",
		 section,
		 valStr);
#if __GNUC__ >= 7
#pragma GCC diagnostic pop
#endif

	return CPA_STATUS_SUCCESS;
}

CpaStatus
cpaDcQueryCapabilities(CpaInstanceHandle dcInstance,
		       CpaDcInstanceCapabilities *pInstanceCapabilities)
{
	CpaInstanceHandle insHandle = NULL;
	sal_compression_service_t *pService = NULL;
	Cpa32U capabilitiesMask = 0;
	dc_extd_ftrs_t *pExtendedFtrs = NULL;

	if (CPA_INSTANCE_HANDLE_SINGLE == dcInstance) {
		insHandle = dcGetFirstHandle();
		if (NULL == insHandle) {
			QAT_UTILS_LOG("Can not get the instance.\n");
			return CPA_STATUS_FAIL;
		}
	} else {
		insHandle = dcInstance;
	}

	pService = (sal_compression_service_t *)insHandle;

	LAC_CHECK_NULL_PARAM(insHandle);
	SAL_CHECK_INSTANCE_TYPE(insHandle, SAL_SERVICE_TYPE_COMPRESSION);
	LAC_CHECK_NULL_PARAM(pInstanceCapabilities);

	memset(pInstanceCapabilities, 0, sizeof(CpaDcInstanceCapabilities));

	capabilitiesMask = pService->generic_service_info.capabilitiesMask;

	/* Set compression capabilities */
	if (capabilitiesMask & ICP_ACCEL_CAPABILITIES_CNV_INTEGRITY) {
		pInstanceCapabilities->integrityCrcs = CPA_TRUE;
	}

	pInstanceCapabilities->endOfLastBlock = CPA_TRUE;
	pInstanceCapabilities->statefulDeflateCompression = CPA_FALSE;
	pInstanceCapabilities->statefulDeflateDecompression = CPA_TRUE;
	pInstanceCapabilities->statelessDeflateCompression = CPA_TRUE;
	pInstanceCapabilities->statelessDeflateDecompression = CPA_TRUE;
	pInstanceCapabilities->checksumCRC32 = CPA_TRUE;
	pInstanceCapabilities->checksumAdler32 = CPA_TRUE;
	pInstanceCapabilities->dynamicHuffman = CPA_TRUE;
	pInstanceCapabilities->precompiledHuffman = CPA_FALSE;
	pInstanceCapabilities->dynamicHuffmanBufferReq = CPA_TRUE;
	pInstanceCapabilities->autoSelectBestHuffmanTree = CPA_TRUE;

	pInstanceCapabilities->validWindowSizeMaskCompression =
	    pService->comp_device_data.windowSizeMask;
	pInstanceCapabilities->validWindowSizeMaskDecompression =
	    pService->comp_device_data.windowSizeMask;
	pExtendedFtrs = (dc_extd_ftrs_t *)&(
	    ((sal_service_t *)insHandle)->dcExtendedFeatures);
	pInstanceCapabilities->batchAndPack = CPA_FALSE;
	pInstanceCapabilities->compressAndVerify =
	    (CpaBoolean)pExtendedFtrs->is_cnv;
	pInstanceCapabilities->compressAndVerifyStrict = CPA_TRUE;
	pInstanceCapabilities->compressAndVerifyAndRecover =
	    (CpaBoolean)pExtendedFtrs->is_cnvnr;
	return CPA_STATUS_SUCCESS;
}

CpaStatus
cpaDcSetAddressTranslation(const CpaInstanceHandle instanceHandle,
			   CpaVirtualToPhysical virtual2Physical)
{
	sal_service_t *pService = NULL;
	CpaInstanceHandle insHandle = NULL;

	if (CPA_INSTANCE_HANDLE_SINGLE == instanceHandle) {
		insHandle = dcGetFirstHandle();
	} else {
		insHandle = instanceHandle;
	}

	LAC_CHECK_NULL_PARAM(insHandle);
	SAL_CHECK_INSTANCE_TYPE(insHandle, SAL_SERVICE_TYPE_COMPRESSION);
	LAC_CHECK_NULL_PARAM(virtual2Physical);

	pService = (sal_service_t *)insHandle;

	pService->virt2PhysClient = virtual2Physical;

	return CPA_STATUS_SUCCESS;
}

/**
 ******************************************************************************
 * @ingroup cpaDcCommon
 * Data compression specific polling function which polls a DC instance.
 *****************************************************************************/

CpaStatus
icp_sal_DcPollInstance(CpaInstanceHandle instanceHandle_in,
		       Cpa32U response_quota)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	sal_compression_service_t *dc_handle = NULL;
	sal_service_t *gen_handle = NULL;
	icp_comms_trans_handle trans_hndTable[DC_NUM_RX_RINGS];

	if (CPA_INSTANCE_HANDLE_SINGLE == instanceHandle_in) {
		dc_handle = (sal_compression_service_t *)dcGetFirstHandle();
	} else {
		dc_handle = (sal_compression_service_t *)instanceHandle_in;
	}

	LAC_CHECK_NULL_PARAM(dc_handle);
	SAL_RUNNING_CHECK(dc_handle);

	gen_handle = &(dc_handle->generic_service_info);
	if (SAL_SERVICE_TYPE_COMPRESSION != gen_handle->type) {
		QAT_UTILS_LOG("Instance handle type is incorrect.\n");
		return CPA_STATUS_FAIL;
	}

	/*
	 * From the instanceHandle we must get the trans_handle and send
	 * down to adf for polling.
	 * Populate our trans handle table with the appropriate handles.
	 */
	trans_hndTable[0] = dc_handle->trans_handle_compression_rx;

	/* Call adf to do the polling. */
	status = icp_adf_pollInstance(trans_hndTable,
				      DC_NUM_RX_RINGS,
				      response_quota);
	return status;
}

/**
 ******************************************************************************
 * @ingroup cpaDcCommon
 *****************************************************************************/
CpaStatus
cpaDcInstanceSetNotificationCb(
    const CpaInstanceHandle instanceHandle,
    const CpaDcInstanceNotificationCbFunc pInstanceNotificationCb,
    void *pCallbackTag)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	sal_service_t *gen_handle = instanceHandle;

	LAC_CHECK_NULL_PARAM(gen_handle);
	gen_handle->notification_cb = pInstanceNotificationCb;
	gen_handle->cb_tag = pCallbackTag;
	return status;
}

CpaInstanceHandle
dcGetFirstHandle(void)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	static icp_accel_dev_t *adfInsts[ADF_MAX_DEVICES] = { 0 };
	CpaInstanceHandle dcInst = NULL;
	icp_accel_dev_t *dev_addr = NULL;
	sal_t *base_addr = NULL;
	sal_list_t *list_temp = NULL;
	Cpa16U i, num_dc = 0;

	/* Only need 1 dev with compression enabled - so check all devices */
	status = icp_amgr_getAllAccelDevByCapabilities(
	    ICP_ACCEL_CAPABILITIES_COMPRESSION, adfInsts, &num_dc);
	if ((0 == num_dc) || (CPA_STATUS_SUCCESS != status)) {
		QAT_UTILS_LOG(
		    "No compression devices enabled in the system.\n");
		return dcInst;
	}

	for (i = 0; i < num_dc; i++) {
		dev_addr = (icp_accel_dev_t *)adfInsts[i];
		if (NULL != dev_addr) {
			base_addr = dev_addr->pSalHandle;
			if (NULL != base_addr) {
				list_temp = base_addr->compression_services;
				if (NULL != list_temp) {
					dcInst = SalList_getObject(list_temp);
					break;
				}
			}
		}
	}
	return dcInst;
}
