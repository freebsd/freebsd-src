/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */
/**
 *****************************************************************************
 * @file dc_datapath.c
 *
 * @defgroup Dc_DataCompression DC Data Compression
 *
 * @ingroup Dc_DataCompression
 *
 * @description
 *      Implementation of the Data Compression datapath operations.
 *
 *****************************************************************************/

/*
*******************************************************************************
* Include public/global header files
*******************************************************************************
*/
#include "cpa.h"
#include "cpa_dc.h"
#include "cpa_dc_dp.h"

/*
*******************************************************************************
* Include private header files
*******************************************************************************
*/
#include "dc_session.h"
#include "dc_datapath.h"
#include "sal_statistics.h"
#include "lac_common.h"
#include "lac_mem.h"
#include "lac_mem_pools.h"
#include "sal_types_compression.h"
#include "dc_stats.h"
#include "lac_buffer_desc.h"
#include "lac_sal.h"
#include "lac_log.h"
#include "lac_sync.h"
#include "sal_service_state.h"
#include "sal_qat_cmn_msg.h"
#include "sal_hw_gen.h"
#include "dc_error_counter.h"
#define DC_COMP_MAX_BUFF_SIZE (1024 * 64)

static QatUtilsAtomic dcErrorCount[MAX_DC_ERROR_TYPE];

void
dcErrorLog(CpaDcReqStatus dcError)
{
	Cpa32U absError = 0;

	absError = abs(dcError);
	if ((dcError < CPA_DC_OK) && (absError < MAX_DC_ERROR_TYPE)) {
		qatUtilsAtomicInc(&(dcErrorCount[absError]));
	}
}

Cpa64U
getDcErrorCounter(CpaDcReqStatus dcError)
{
	Cpa32U absError = 0;

	absError = abs(dcError);
	if (!(dcError >= CPA_DC_OK || dcError < CPA_DC_EMPTY_DYM_BLK)) {
		return (Cpa64U)qatUtilsAtomicGet(&dcErrorCount[absError]);
	}

	return 0;
}

static inline void
dcUpdateXltOverflowChecksumsGen4(const dc_compression_cookie_t *pCookie,
				 const icp_qat_fw_resp_comp_pars_t *pRespPars,
				 CpaDcRqResults *pDcResults)
{
	dc_session_desc_t *pSessionDesc =
	    DC_SESSION_DESC_FROM_CTX_GET(pCookie->pSessionHandle);

	/* Recompute CRC checksum when either the checksum type
	 * is CPA_DC_CRC32 or when the integrity CRCs are enabled.
	 */
	if (CPA_DC_CRC32 == pSessionDesc->checksumType) {
		pDcResults->checksum = pRespPars->crc.legacy.curr_crc32;

		/* No need to recalculate the swCrc64I here as this will get
		 * handled later in dcHandleIntegrityChecksumsGen4.
		 */
	} else if (CPA_DC_ADLER32 == pSessionDesc->checksumType) {
		pDcResults->checksum = pRespPars->crc.legacy.curr_adler_32;
	}
}

void
dcCompression_ProcessCallback(void *pRespMsg)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	icp_qat_fw_comp_resp_t *pCompRespMsg = NULL;
	void *callbackTag = NULL;
	Cpa64U *pReqData = NULL;
	CpaDcDpOpData *pResponse = NULL;
	CpaDcRqResults *pResults = NULL;
	CpaDcCallbackFn pCbFunc = NULL;
	dc_session_desc_t *pSessionDesc = NULL;
	sal_compression_service_t *pService = NULL;
	dc_compression_cookie_t *pCookie = NULL;
	CpaDcOpData *pOpData = NULL;
	CpaBoolean cmpPass = CPA_TRUE, xlatPass = CPA_TRUE;
	CpaBoolean isDcDp = CPA_FALSE;
	CpaBoolean integrityCrcCheck = CPA_FALSE;
	CpaBoolean verifyHwIntegrityCrcs = CPA_FALSE;
	Cpa8U cmpErr = ERR_CODE_NO_ERROR, xlatErr = ERR_CODE_NO_ERROR;
	dc_request_dir_t compDecomp = DC_COMPRESSION_REQUEST;
	Cpa8U opStatus = ICP_QAT_FW_COMN_STATUS_FLAG_OK;
	Cpa8U hdrFlags = 0;

	/* Cast response message to compression response message type */
	pCompRespMsg = (icp_qat_fw_comp_resp_t *)pRespMsg;
	if (!(pCompRespMsg)) {
		QAT_UTILS_LOG("pCompRespMsg is NULL\n");
		return;
	}
	/* Extract request data pointer from the opaque data */
	LAC_MEM_SHARED_READ_TO_PTR(pCompRespMsg->opaque_data, pReqData);
	if (!(pReqData)) {
		QAT_UTILS_LOG("pReqData is NULL\n");
		return;
	}

	/* Extract fields from the request data structure */
	pCookie = (dc_compression_cookie_t *)pReqData;

	pSessionDesc = DC_SESSION_DESC_FROM_CTX_GET(pCookie->pSessionHandle);
	pService = (sal_compression_service_t *)(pCookie->dcInstance);

	isDcDp = pSessionDesc->isDcDp;
	if (CPA_TRUE == isDcDp) {
		pResponse = (CpaDcDpOpData *)pReqData;
		pResults = &(pResponse->results);

		if (CPA_DC_DIR_DECOMPRESS == pSessionDesc->sessDirection) {
			compDecomp = DC_DECOMPRESSION_REQUEST;
		}
		pCookie = NULL;
	} else {
		pResults = pCookie->pResults;
		callbackTag = pCookie->callbackTag;
		pCbFunc = pCookie->pSessionDesc->pCompressionCb;
		compDecomp = pCookie->compDecomp;
		pOpData = pCookie->pDcOpData;
	}

	opStatus = pCompRespMsg->comn_resp.comn_status;

	if (NULL != pOpData) {
		verifyHwIntegrityCrcs = pOpData->verifyHwIntegrityCrcs;
		integrityCrcCheck = pOpData->integrityCrcCheck;
	}

	hdrFlags = pCompRespMsg->comn_resp.hdr_flags;

	/* Get the cmp error code */
	cmpErr = pCompRespMsg->comn_resp.comn_error.s1.cmp_err_code;
	if (ICP_QAT_FW_COMN_RESP_UNSUPPORTED_REQUEST_STAT_GET(opStatus)) {
		/* Compression not supported by firmware, set produced/consumed
		   to zero
		   and call the cb function with status CPA_STATUS_UNSUPPORTED
		   */
		QAT_UTILS_LOG("Compression feature not supported\n");
		status = CPA_STATUS_UNSUPPORTED;
		pResults->status = (Cpa8S)cmpErr;
		pResults->consumed = 0;
		pResults->produced = 0;
		if (CPA_TRUE == isDcDp) {
			if (pResponse)
				pResponse->responseStatus =
				    CPA_STATUS_UNSUPPORTED;
			(pService->pDcDpCb)(pResponse);
		} else {
			/* Free the memory pool */
			Lac_MemPoolEntryFree(pCookie);
			pCookie = NULL;

			if (NULL != pCbFunc) {
				pCbFunc(callbackTag, status);
			}
		}
		if (DC_COMPRESSION_REQUEST == compDecomp) {
			COMPRESSION_STAT_INC(numCompCompletedErrors, pService);
		} else {
			COMPRESSION_STAT_INC(numDecompCompletedErrors,
					     pService);
		}
		return;
	} else {
		/* Check compression response status */
		cmpPass =
		    (CpaBoolean)(ICP_QAT_FW_COMN_STATUS_FLAG_OK ==
				 ICP_QAT_FW_COMN_RESP_CMP_STAT_GET(opStatus));
	}

	if (isDcGen2x(pService)) {
		/* QAT1.7 and QAT 1.8 hardware */
		if (CPA_DC_INCOMPLETE_FILE_ERR == (Cpa8S)cmpErr) {
			cmpPass = CPA_TRUE;
			cmpErr = ERR_CODE_NO_ERROR;
		}
	} else {
		/* QAT2.0 hardware cancels the incomplete file errors
		 * only for DEFLATE algorithm.
		 * Decompression direction is not tested in the callback as
		 * the request does not allow it.
		 */
		if ((pSessionDesc->compType == CPA_DC_DEFLATE) &&
		    (CPA_DC_INCOMPLETE_FILE_ERR == (Cpa8S)cmpErr)) {
			cmpPass = CPA_TRUE;
			cmpErr = ERR_CODE_NO_ERROR;
		}
	}
	/* log the slice hang and endpoint push/pull error inside the response
	 */
	if (ERR_CODE_SSM_ERROR == (Cpa8S)cmpErr) {
		QAT_UTILS_LOG(
		    "Slice hang detected on the compression slice.\n");
	} else if (ERR_CODE_ENDPOINT_ERROR == (Cpa8S)cmpErr) {
		QAT_UTILS_LOG(
		    "PCIe End Point Push/Pull or TI/RI Parity error detected.\n");
	}

	/* We return the compression error code for now. We would need to update
	 * the API if we decide to return both error codes */
	pResults->status = (Cpa8S)cmpErr;

	/* Check the translator status */
	if ((DC_COMPRESSION_REQUEST == compDecomp) &&
	    (CPA_DC_HT_FULL_DYNAMIC == pSessionDesc->huffType)) {
		/* Check translator response status */
		xlatPass =
		    (CpaBoolean)(ICP_QAT_FW_COMN_STATUS_FLAG_OK ==
				 ICP_QAT_FW_COMN_RESP_XLAT_STAT_GET(opStatus));

		/* Get the translator error code */
		xlatErr = pCompRespMsg->comn_resp.comn_error.s1.xlat_err_code;

		/* Return a fatal error or a potential error in the translator
		 * slice if the compression slice did not return any error */
		if ((CPA_DC_OK == pResults->status) ||
		    (CPA_DC_FATALERR == (Cpa8S)xlatErr)) {
			pResults->status = (Cpa8S)xlatErr;
		}
	}
	/* Update dc error counter */
	dcErrorLog(pResults->status);

	if (CPA_FALSE == isDcDp) {
		/* In case of any error for an end of packet request, we need to
		 * update
		 * the request type for the following request */
		if (CPA_DC_FLUSH_FINAL == pCookie->flushFlag && cmpPass &&
		    xlatPass) {
			pSessionDesc->requestType = DC_REQUEST_FIRST;
		} else {
			pSessionDesc->requestType = DC_REQUEST_SUBSEQUENT;
		}
		if ((CPA_DC_STATEFUL == pSessionDesc->sessState) ||
		    ((CPA_DC_STATELESS == pSessionDesc->sessState) &&
		     (DC_COMPRESSION_REQUEST == compDecomp))) {
			/* Overflow is a valid use case for Traditional API
			 * only. Stateful Overflow is supported in both
			 * compression and decompression direction. Stateless
			 * Overflow is supported only in compression direction.
			 */
			if (CPA_DC_OVERFLOW == (Cpa8S)cmpErr)
				cmpPass = CPA_TRUE;

			if (CPA_DC_OVERFLOW == (Cpa8S)xlatErr) {
				if (isDcGen4x(pService) &&
				    (CPA_TRUE ==
				     pService->comp_device_data
					 .translatorOverflow)) {
					pResults->consumed =
					    pCompRespMsg->comp_resp_pars
						.input_byte_counter;

					dcUpdateXltOverflowChecksumsGen4(
					    pCookie,
					    &pCompRespMsg->comp_resp_pars,
					    pResults);
				}
				xlatPass = CPA_TRUE;
			}
		}
	} else {
		if (CPA_DC_OVERFLOW == (Cpa8S)cmpErr) {
			cmpPass = CPA_FALSE;
		}
		if (CPA_DC_OVERFLOW == (Cpa8S)xlatErr) {
			/* XLT overflow is not valid for Data Plane requests */
			xlatPass = CPA_FALSE;
		}
	}

	if ((CPA_TRUE == cmpPass) && (CPA_TRUE == xlatPass)) {
		/* Extract the response from the firmware */
		pResults->consumed =
		    pCompRespMsg->comp_resp_pars.input_byte_counter;
		pResults->produced =
		    pCompRespMsg->comp_resp_pars.output_byte_counter;
		pSessionDesc->cumulativeConsumedBytes += pResults->consumed;

		/* Handle Checksum for end to end data integrity. */
		if (CPA_TRUE ==
			pService->generic_service_info.integrityCrcCheck &&
		    CPA_TRUE == integrityCrcCheck) {
			pSessionDesc->previousChecksum =
			    pSessionDesc->seedSwCrc.swCrc32I;
		} else if (CPA_DC_OVERFLOW != (Cpa8S)xlatErr) {
			if (CPA_DC_CRC32 == pSessionDesc->checksumType) {
				pResults->checksum =
				    pCompRespMsg->comp_resp_pars.crc.legacy
					.curr_crc32;
			} else if (CPA_DC_ADLER32 ==
				   pSessionDesc->checksumType) {
				pResults->checksum =
				    pCompRespMsg->comp_resp_pars.crc.legacy
					.curr_adler_32;
			}
			pSessionDesc->previousChecksum = pResults->checksum;
		}

		if (DC_DECOMPRESSION_REQUEST == compDecomp) {
			pResults->endOfLastBlock =
			    (ICP_QAT_FW_COMN_STATUS_CMP_END_OF_LAST_BLK_FLAG_SET ==
			     ICP_QAT_FW_COMN_RESP_CMP_END_OF_LAST_BLK_FLAG_GET(
				 opStatus));
		} else {
			/* Check if returned data is a stored block
			 * in compression direction
			 */
			pResults->dataUncompressed =
			    ICP_QAT_FW_COMN_HDR_ST_BLK_FLAG_GET(hdrFlags);
		}

		/* Save the checksum for the next request */
		if ((CPA_DC_OVERFLOW != (Cpa8S)xlatErr) &&
		    (CPA_TRUE == verifyHwIntegrityCrcs)) {
			pSessionDesc->previousChecksum =
			    pSessionDesc->seedSwCrc.swCrc32I;
		}

		/* Check if a CNV recovery happened and
		 * increase stats counter
		 */
		if ((DC_COMPRESSION_REQUEST == compDecomp) &&
		    ICP_QAT_FW_COMN_HDR_CNV_FLAG_GET(hdrFlags) &&
		    ICP_QAT_FW_COMN_HDR_CNVNR_FLAG_GET(hdrFlags)) {
			COMPRESSION_STAT_INC(numCompCnvErrorsRecovered,
					     pService);
		}

		if (CPA_TRUE == isDcDp) {
			if (pResponse)
				pResponse->responseStatus = CPA_STATUS_SUCCESS;
		} else {
			if (DC_COMPRESSION_REQUEST == compDecomp) {
				COMPRESSION_STAT_INC(numCompCompleted,
						     pService);
			} else {
				COMPRESSION_STAT_INC(numDecompCompleted,
						     pService);
			}
		}
	} else {
#ifdef ICP_DC_RETURN_COUNTERS_ON_ERROR
		/* Extract the response from the firmware */
		pResults->consumed =
		    pCompRespMsg->comp_resp_pars.input_byte_counter;
		pResults->produced =
		    pCompRespMsg->comp_resp_pars.output_byte_counter;

		if (CPA_DC_STATEFUL == pSessionDesc->sessState) {
			pSessionDesc->cumulativeConsumedBytes +=
			    pResults->consumed;
		} else {
			/* In the stateless case all requests have both SOP and
			 * EOP set */
			pSessionDesc->cumulativeConsumedBytes =
			    pResults->consumed;
		}
#else
		pResults->consumed = 0;
		pResults->produced = 0;
#endif
		if (CPA_DC_OVERFLOW == pResults->status &&
		    CPA_DC_STATELESS == pSessionDesc->sessState) {
			/* This error message will be returned by Data Plane API
			 * in both
			 * compression and decompression direction. With
			 * Traditional API
			 * this error message will be returned only in stateless
			 * decompression direction */
			QAT_UTILS_LOG(
			    "Unrecoverable error: stateless overflow. You may need to increase the size of your destination buffer.\n");
		}

		if (CPA_TRUE == isDcDp) {
			if (pResponse)
				pResponse->responseStatus = CPA_STATUS_FAIL;
		} else {
			if (CPA_DC_OK != pResults->status &&
			    CPA_DC_INCOMPLETE_FILE_ERR != pResults->status) {
				status = CPA_STATUS_FAIL;
			}

			if (DC_COMPRESSION_REQUEST == compDecomp) {
				COMPRESSION_STAT_INC(numCompCompletedErrors,
						     pService);
			} else {
				COMPRESSION_STAT_INC(numDecompCompletedErrors,
						     pService);
			}
		}
	}

	if (CPA_TRUE == isDcDp) {
		/* Decrement number of stateless pending callbacks for session
		 */
		pSessionDesc->pendingDpStatelessCbCount--;
		(pService->pDcDpCb)(pResponse);
	} else {
		/* Decrement number of pending callbacks for session */
		if (CPA_DC_STATELESS == pSessionDesc->sessState) {
			qatUtilsAtomicDec(
			    &(pCookie->pSessionDesc->pendingStatelessCbCount));
		} else if (0 !=
			   qatUtilsAtomicGet(&pCookie->pSessionDesc
						  ->pendingStatefulCbCount)) {
			qatUtilsAtomicDec(
			    &(pCookie->pSessionDesc->pendingStatefulCbCount));
		}

		/* Free the memory pool */
		Lac_MemPoolEntryFree(pCookie);
		pCookie = NULL;

		if (NULL != pCbFunc) {
			pCbFunc(callbackTag, status);
		}
	}
}

/**
 *****************************************************************************
 * @ingroup Dc_DataCompression
 *      Check that all the parameters in the pOpData structure are valid
 *
 * @description
 *      Check that all the parameters in the pOpData structure are valid
 *
 * @param[in]   pService              Pointer to the compression service
 * @param[in]   pOpData               Pointer to request information structure
 *                                    holding parameters for cpaDcCompress2 and
 *                                    CpaDcDecompressData2
 * @retval CPA_STATUS_SUCCESS         Function executed successfully
 * @retval CPA_STATUS_INVALID_PARAM   Invalid parameter passed in
 *
 *****************************************************************************/
CpaStatus
dcCheckOpData(sal_compression_service_t *pService, CpaDcOpData *pOpData)
{
	CpaDcSkipMode skipMode = 0;

	if ((pOpData->flushFlag < CPA_DC_FLUSH_NONE) ||
	    (pOpData->flushFlag > CPA_DC_FLUSH_FULL)) {
		LAC_INVALID_PARAM_LOG("Invalid flushFlag value");
		return CPA_STATUS_INVALID_PARAM;
	}

	skipMode = pOpData->inputSkipData.skipMode;
	if ((skipMode < CPA_DC_SKIP_DISABLED) ||
	    (skipMode > CPA_DC_SKIP_STRIDE)) {
		LAC_INVALID_PARAM_LOG("Invalid input skip mode value");
		return CPA_STATUS_INVALID_PARAM;
	}

	skipMode = pOpData->outputSkipData.skipMode;
	if ((skipMode < CPA_DC_SKIP_DISABLED) ||
	    (skipMode > CPA_DC_SKIP_STRIDE)) {
		LAC_INVALID_PARAM_LOG("Invalid output skip mode value");
		return CPA_STATUS_INVALID_PARAM;
	}

	if (pOpData->integrityCrcCheck == CPA_FALSE &&
	    pOpData->verifyHwIntegrityCrcs == CPA_TRUE) {
		LAC_INVALID_PARAM_LOG(
		    "integrityCrcCheck must be set to true"
		    "in order to enable verifyHwIntegrityCrcs");
		return CPA_STATUS_INVALID_PARAM;
	}

	if (pOpData->integrityCrcCheck != CPA_TRUE &&
	    pOpData->integrityCrcCheck != CPA_FALSE) {
		LAC_INVALID_PARAM_LOG("Invalid integrityCrcCheck value");
		return CPA_STATUS_INVALID_PARAM;
	}

	if (pOpData->verifyHwIntegrityCrcs != CPA_TRUE &&
	    pOpData->verifyHwIntegrityCrcs != CPA_FALSE) {
		LAC_INVALID_PARAM_LOG("Invalid verifyHwIntegrityCrcs value");
		return CPA_STATUS_INVALID_PARAM;
	}

	if (pOpData->compressAndVerify != CPA_TRUE &&
	    pOpData->compressAndVerify != CPA_FALSE) {
		LAC_INVALID_PARAM_LOG("Invalid cnv decompress check value");
		return CPA_STATUS_INVALID_PARAM;
	}

	if (CPA_TRUE == pOpData->integrityCrcCheck &&
	    CPA_FALSE == pService->generic_service_info.integrityCrcCheck) {
		LAC_INVALID_PARAM_LOG("Integrity CRC check is not "
				      "supported on this device");
		return CPA_STATUS_INVALID_PARAM;
	}

	if (CPA_TRUE == pOpData->integrityCrcCheck &&
	    NULL == pOpData->pCrcData) {
		LAC_INVALID_PARAM_LOG("Integrity CRC data structure "
				      "not initialized in CpaDcOpData");
		return CPA_STATUS_INVALID_PARAM;
	}

	return CPA_STATUS_SUCCESS;
}

/**
 *****************************************************************************
 * @ingroup Dc_DataCompression
 *      Check the compression source buffer for Batch and Pack API.
 *
 * @description
 *      Check that all the parameters used for Pack compression
 *      request are valid. This function essentially checks the source buffer
 *      parameters and results structure parameters.
 *
 * @param[in]   pSessionHandle        Session handle
 * @param[in]   pSrcBuff              Pointer to data buffer for compression
 * @param[in]   pDestBuff             Pointer to buffer space allocated for
 *                                    output data
 * @param[in]   pResults              Pointer to results structure
 * @param[in]   flushFlag             Indicates the type of flush to be
 *                                    performed
 * @param[in]   srcBuffSize           Size of the source buffer
 *
 * @retval CPA_STATUS_SUCCESS         Function executed successfully
 * @retval CPA_STATUS_INVALID_PARAM   Invalid parameter passed in
 *
 *****************************************************************************/
static CpaStatus
dcCheckSourceData(CpaDcSessionHandle pSessionHandle,
		  CpaBufferList *pSrcBuff,
		  CpaBufferList *pDestBuff,
		  CpaDcRqResults *pResults,
		  CpaDcFlush flushFlag,
		  Cpa64U srcBuffSize,
		  CpaDcSkipData *skipData)
{
	dc_session_desc_t *pSessionDesc = NULL;

	LAC_CHECK_NULL_PARAM(pSessionHandle);
	LAC_CHECK_NULL_PARAM(pSrcBuff);
	LAC_CHECK_NULL_PARAM(pDestBuff);
	LAC_CHECK_NULL_PARAM(pResults);

	pSessionDesc = DC_SESSION_DESC_FROM_CTX_GET(pSessionHandle);
	if (NULL == pSessionDesc) {
		LAC_INVALID_PARAM_LOG("Session handle not as expected");
		return CPA_STATUS_INVALID_PARAM;
	}

	if ((flushFlag < CPA_DC_FLUSH_NONE) ||
	    (flushFlag > CPA_DC_FLUSH_FULL)) {
		LAC_INVALID_PARAM_LOG("Invalid flushFlag value");
		return CPA_STATUS_INVALID_PARAM;
	}

	if (pSrcBuff == pDestBuff) {
		LAC_INVALID_PARAM_LOG("In place operation not supported");
		return CPA_STATUS_INVALID_PARAM;
	}

	/* Compressing zero bytes is not supported for stateless sessions
	 * for non Batch and Pack requests */
	if ((CPA_DC_STATELESS == pSessionDesc->sessState) &&
	    (0 == srcBuffSize) && (NULL == skipData)) {
		LAC_INVALID_PARAM_LOG(
		    "The source buffer size needs to be greater than "
		    "zero bytes for stateless sessions");
		return CPA_STATUS_INVALID_PARAM;
	}

	if (srcBuffSize > DC_BUFFER_MAX_SIZE) {
		LAC_INVALID_PARAM_LOG(
		    "The source buffer size needs to be less than or "
		    "equal to 2^32-1 bytes");
		return CPA_STATUS_INVALID_PARAM;
	}

	return CPA_STATUS_SUCCESS;
}

/**
 *****************************************************************************
 * @ingroup Dc_DataCompression
 *      Check the compression or decompression function parameters.
 *
 * @description
 *      Check that all the parameters used for a Batch and Pack compression
 *      request are valid. This function essentially checks the destination
 *      buffer parameters and intermediate buffer parameters.
 *
 * @param[in]   pService              Pointer to the compression service
 * @param[in]   pSessionHandle        Session handle
 * @param[in]   pDestBuff             Pointer to buffer space allocated for
 *                                    output data
 * @param[in]   compDecomp            Direction of the operation
 *
 * @retval CPA_STATUS_SUCCESS         Function executed successfully
 * @retval CPA_STATUS_INVALID_PARAM   Invalid parameter passed in
 *
 *****************************************************************************/
static CpaStatus
dcCheckDestinationData(sal_compression_service_t *pService,
		       CpaDcSessionHandle pSessionHandle,
		       CpaBufferList *pDestBuff,
		       dc_request_dir_t compDecomp)
{
	dc_session_desc_t *pSessionDesc = NULL;
	Cpa64U destBuffSize = 0;

	LAC_CHECK_NULL_PARAM(pSessionHandle);
	LAC_CHECK_NULL_PARAM(pDestBuff);

	pSessionDesc = DC_SESSION_DESC_FROM_CTX_GET(pSessionHandle);
	if (NULL == pSessionDesc) {
		LAC_INVALID_PARAM_LOG("Session handle not as expected");
		return CPA_STATUS_INVALID_PARAM;
	}

	if (LacBuffDesc_BufferListVerify(pDestBuff,
					 &destBuffSize,
					 LAC_NO_ALIGNMENT_SHIFT) !=
	    CPA_STATUS_SUCCESS) {
		LAC_INVALID_PARAM_LOG(
		    "Invalid destination buffer list parameter");
		return CPA_STATUS_INVALID_PARAM;
	}

	if (destBuffSize > DC_BUFFER_MAX_SIZE) {
		LAC_INVALID_PARAM_LOG(
		    "The destination buffer size needs to be less "
		    "than or equal to 2^32-1 bytes");
		return CPA_STATUS_INVALID_PARAM;
	}

	if (CPA_TRUE == pSessionDesc->isDcDp) {
		LAC_INVALID_PARAM_LOG(
		    "The session type should not be data plane");
		return CPA_STATUS_INVALID_PARAM;
	}

	if (DC_COMPRESSION_REQUEST == compDecomp) {
		if (CPA_DC_HT_FULL_DYNAMIC == pSessionDesc->huffType) {

			/* Check if intermediate buffers are supported */
			if ((isDcGen2x(pService)) &&
			    ((0 == pService->pInterBuffPtrsArrayPhyAddr) ||
			     (NULL == pService->pInterBuffPtrsArray))) {
				LAC_LOG_ERROR(
				    "No intermediate buffer defined for this instance "
				    "- see cpaDcStartInstance");
				return CPA_STATUS_INVALID_PARAM;
			}

			/* Ensure that the destination buffer size is greater or
			 * equal to 128B */
			if (destBuffSize < DC_DEST_BUFFER_DYN_MIN_SIZE) {
				LAC_INVALID_PARAM_LOG(
				    "Destination buffer size should be "
				    "greater or equal to 128B");
				return CPA_STATUS_INVALID_PARAM;
			}
		} else
		{
			/* Ensure that the destination buffer size is greater or
			 * equal to devices min output buff size */
			if (destBuffSize <
			    pService->comp_device_data.minOutputBuffSize) {
				LAC_INVALID_PARAM_LOG1(
				    "Destination buffer size should be "
				    "greater or equal to %d bytes",
				    pService->comp_device_data
					.minOutputBuffSize);
				return CPA_STATUS_INVALID_PARAM;
			}
		}
	} else {
		/* Ensure that the destination buffer size is greater than
		 * 0 bytes */
		if (destBuffSize < DC_DEST_BUFFER_DEC_MIN_SIZE) {
			LAC_INVALID_PARAM_LOG(
			    "Destination buffer size should be "
			    "greater than 0 bytes");
			return CPA_STATUS_INVALID_PARAM;
		}
	}
	return CPA_STATUS_SUCCESS;
}

/**
 *****************************************************************************
 * @ingroup Dc_DataCompression
 *      Populate the compression request parameters
 *
 * @description
 *      This function will populate the compression request parameters
 *
 * @param[out]  pCompReqParams   Pointer to the compression request parameters
 * @param[in]   pCookie          Pointer to the compression cookie
 *
 *****************************************************************************/
static void
dcCompRequestParamsPopulate(icp_qat_fw_comp_req_params_t *pCompReqParams,
			    dc_compression_cookie_t *pCookie)
{
	pCompReqParams->comp_len = pCookie->srcTotalDataLenInBytes;
	pCompReqParams->out_buffer_sz = pCookie->dstTotalDataLenInBytes;
}

/**
 *****************************************************************************
 * @ingroup Dc_DataCompression
 *      Create the requests for compression or decompression
 *
 * @description
 *      Create the requests for compression or decompression. This function
 *      will update the cookie will all required information.
 *
 * @param{out]  pCookie             Pointer to the compression cookie
 * @param[in]   pService            Pointer to the compression service
 * @param[in]   pSessionDesc        Pointer to the session descriptor
 * @param[in    pSessionHandle      Session handle
 * @param[in]   pSrcBuff            Pointer to data buffer for compression
 * @param[in]   pDestBuff           Pointer to buffer space for data after
 *                                  compression
 * @param[in]   pResults            Pointer to results structure
 * @param[in]   flushFlag           Indicates the type of flush to be
 *                                  performed
 * @param[in]   pOpData             Pointer to request information structure
 *                                  holding parameters for cpaDcCompress2
 *                                  and CpaDcDecompressData2
 * @param[in]   callbackTag         Pointer to the callback tag
 * @param[in]   compDecomp          Direction of the operation
 * @param[in]   compressAndVerify   Compress and Verify
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully
 * @retval CPA_STATUS_INVALID_PARAM Invalid parameter passed in
 *
 *****************************************************************************/
static CpaStatus
dcCreateRequest(dc_compression_cookie_t *pCookie,
		sal_compression_service_t *pService,
		dc_session_desc_t *pSessionDesc,
		CpaDcSessionHandle pSessionHandle,
		CpaBufferList *pSrcBuff,
		CpaBufferList *pDestBuff,
		CpaDcRqResults *pResults,
		CpaDcFlush flushFlag,
		CpaDcOpData *pOpData,
		void *callbackTag,
		dc_request_dir_t compDecomp,
		dc_cnv_mode_t cnvMode)
{
	icp_qat_fw_comp_req_t *pMsg = NULL;
	icp_qat_fw_comp_req_params_t *pCompReqParams = NULL;
	Cpa64U srcAddrPhys = 0, dstAddrPhys = 0;
	Cpa64U srcTotalDataLenInBytes = 0, dstTotalDataLenInBytes = 0;

	Cpa32U rpCmdFlags = 0;
	Cpa8U sop = ICP_QAT_FW_COMP_SOP;
	Cpa8U eop = ICP_QAT_FW_COMP_EOP;
	Cpa8U bFinal = ICP_QAT_FW_COMP_NOT_BFINAL;
	Cpa8U crcMode = ICP_QAT_FW_COMP_CRC_MODE_LEGACY;
	Cpa8U cnvDecompReq = ICP_QAT_FW_COMP_NO_CNV;
	Cpa8U cnvRecovery = ICP_QAT_FW_COMP_NO_CNV_RECOVERY;
	CpaBoolean cnvErrorInjection = ICP_QAT_FW_COMP_NO_CNV_DFX;
	CpaBoolean integrityCrcCheck = CPA_FALSE;
	CpaStatus status = CPA_STATUS_SUCCESS;
	CpaDcFlush flush = CPA_DC_FLUSH_NONE;
	Cpa32U initial_adler = 1;
	Cpa32U initial_crc32 = 0;
	icp_qat_fw_comp_req_t *pReqCache = NULL;

	/* Write the buffer descriptors */
	status = LacBuffDesc_BufferListDescWriteAndGetSize(
	    pSrcBuff,
	    &srcAddrPhys,
	    CPA_FALSE,
	    &srcTotalDataLenInBytes,
	    &(pService->generic_service_info));
	if (status != CPA_STATUS_SUCCESS) {
		return status;
	}

	status = LacBuffDesc_BufferListDescWriteAndGetSize(
	    pDestBuff,
	    &dstAddrPhys,
	    CPA_FALSE,
	    &dstTotalDataLenInBytes,
	    &(pService->generic_service_info));
	if (status != CPA_STATUS_SUCCESS) {
		return status;
	}

	/* Populate the compression cookie */
	pCookie->dcInstance = pService;
	pCookie->pSessionHandle = pSessionHandle;
	pCookie->callbackTag = callbackTag;
	pCookie->pSessionDesc = pSessionDesc;
	pCookie->pDcOpData = pOpData;
	pCookie->pResults = pResults;
	pCookie->compDecomp = compDecomp;
	pCookie->pUserSrcBuff = NULL;
	pCookie->pUserDestBuff = NULL;

	/* Extract flush flag from either the opData or from the
	 * parameter. Opdata have been introduce with APIs
	 * cpaDcCompressData2 and cpaDcDecompressData2 */
	if (NULL != pOpData) {
		flush = pOpData->flushFlag;
		integrityCrcCheck = pOpData->integrityCrcCheck;
	} else {
		flush = flushFlag;
	}
	pCookie->flushFlag = flush;

	/* The firmware expects the length in bytes for source and destination
	 * to be Cpa32U parameters. However the total data length could be
	 * bigger as allocated by the user. We ensure that this is not the case
	 * in dcCheckSourceData and cast the values to Cpa32U here */
	pCookie->srcTotalDataLenInBytes = (Cpa32U)srcTotalDataLenInBytes;
	if ((isDcGen2x(pService)) && (DC_COMPRESSION_REQUEST == compDecomp) &&
	    (CPA_DC_HT_FULL_DYNAMIC == pSessionDesc->huffType)) {
		if (pService->minInterBuffSizeInBytes <
		    (Cpa32U)dstTotalDataLenInBytes) {
			pCookie->dstTotalDataLenInBytes =
			    (Cpa32U)(pService->minInterBuffSizeInBytes);
		} else {
			pCookie->dstTotalDataLenInBytes =
			    (Cpa32U)dstTotalDataLenInBytes;
		}
	} else
	{
		pCookie->dstTotalDataLenInBytes =
		    (Cpa32U)dstTotalDataLenInBytes;
	}

	/* Device can not decompress an odd byte decompression request
	 * if bFinal is not set
	 */
	if (CPA_TRUE != pService->comp_device_data.oddByteDecompNobFinal) {
		if ((CPA_DC_STATEFUL == pSessionDesc->sessState) &&
		    (CPA_DC_FLUSH_FINAL != flushFlag) &&
		    (DC_DECOMPRESSION_REQUEST == compDecomp) &&
		    (pCookie->srcTotalDataLenInBytes & 0x1)) {
			pCookie->srcTotalDataLenInBytes--;
		}
	}
	/* Device can not decompress odd byte interim requests */
	if (CPA_TRUE != pService->comp_device_data.oddByteDecompInterim) {
		if ((CPA_DC_STATEFUL == pSessionDesc->sessState) &&
		    (CPA_DC_FLUSH_FINAL != flushFlag) &&
		    (CPA_DC_FLUSH_FULL != flushFlag) &&
		    (DC_DECOMPRESSION_REQUEST == compDecomp) &&
		    (pCookie->srcTotalDataLenInBytes & 0x1)) {
			pCookie->srcTotalDataLenInBytes--;
		}
	}

	pMsg = (icp_qat_fw_comp_req_t *)&pCookie->request;

	if (DC_COMPRESSION_REQUEST == compDecomp) {
		pReqCache = &(pSessionDesc->reqCacheComp);
	} else {
		pReqCache = &(pSessionDesc->reqCacheDecomp);
	}

	/* Fills the msg from the template cached in the session descriptor */
	memcpy((void *)pMsg,
	       (void *)(pReqCache),
	       LAC_QAT_DC_REQ_SZ_LW * LAC_LONG_WORD_IN_BYTES);

	if (DC_REQUEST_FIRST == pSessionDesc->requestType) {
		initial_adler = 1;
		initial_crc32 = 0;

		if (CPA_DC_ADLER32 == pSessionDesc->checksumType) {
			pSessionDesc->previousChecksum = initial_adler;
		} else {
			pSessionDesc->previousChecksum = initial_crc32;
		}
	} else if (CPA_DC_STATELESS == pSessionDesc->sessState) {
		pSessionDesc->previousChecksum = pResults->checksum;

		if (CPA_DC_ADLER32 == pSessionDesc->checksumType) {
			initial_adler = pSessionDesc->previousChecksum;
		} else {
			initial_crc32 = pSessionDesc->previousChecksum;
		}
	}

	/* Backup source and destination buffer addresses,
	 * CRC calculations both for CNV and translator overflow
	 * will be performed on them in the callback function.
	 */
	pCookie->pUserSrcBuff = pSrcBuff;
	pCookie->pUserDestBuff = pDestBuff;

	/*
	 * Due to implementation of CNV support and need for backwards
	 * compatibility certain fields in the request and response structs had
	 * been changed, moved or placed in unions cnvMode flag signifies fields
	 * to be selected from req/res
	 *
	 * Doing extended crc checks makes sense only when we want to do the
	 * actual CNV
	 */
	if (CPA_TRUE == pService->generic_service_info.integrityCrcCheck &&
	    CPA_TRUE == integrityCrcCheck) {
		pMsg->comp_pars.crc.crc_data_addr =
		    pSessionDesc->physDataIntegrityCrcs;
		crcMode = ICP_QAT_FW_COMP_CRC_MODE_E2E;
	} else {
		/* Legacy request structure */
		pMsg->comp_pars.crc.legacy.initial_adler = initial_adler;
		pMsg->comp_pars.crc.legacy.initial_crc32 = initial_crc32;
		crcMode = ICP_QAT_FW_COMP_CRC_MODE_LEGACY;
	}

	/* Populate the cmdFlags */
	if (CPA_DC_STATEFUL == pSessionDesc->sessState) {
		pSessionDesc->previousRequestType = pSessionDesc->requestType;

		if (DC_REQUEST_FIRST == pSessionDesc->requestType) {
			/* Update the request type for following requests */
			pSessionDesc->requestType = DC_REQUEST_SUBSEQUENT;

			/* Reinitialise the cumulative amount of consumed bytes
			 */
			pSessionDesc->cumulativeConsumedBytes = 0;

			if (DC_COMPRESSION_REQUEST == compDecomp) {
				pSessionDesc->isSopForCompressionProcessed =
				    CPA_TRUE;
			} else if (DC_DECOMPRESSION_REQUEST == compDecomp) {
				pSessionDesc->isSopForDecompressionProcessed =
				    CPA_TRUE;
			}
		} else {
			if (DC_COMPRESSION_REQUEST == compDecomp) {
				if (CPA_TRUE ==
				    pSessionDesc
					->isSopForCompressionProcessed) {
					sop = ICP_QAT_FW_COMP_NOT_SOP;
				} else {
					pSessionDesc
					    ->isSopForCompressionProcessed =
					    CPA_TRUE;
				}
			} else if (DC_DECOMPRESSION_REQUEST == compDecomp) {
				if (CPA_TRUE ==
				    pSessionDesc
					->isSopForDecompressionProcessed) {
					sop = ICP_QAT_FW_COMP_NOT_SOP;
				} else {
					pSessionDesc
					    ->isSopForDecompressionProcessed =
					    CPA_TRUE;
				}
			}
		}

		if ((CPA_DC_FLUSH_FINAL == flush) ||
		    (CPA_DC_FLUSH_FULL == flush)) {
			/* Update the request type for following requests */
			pSessionDesc->requestType = DC_REQUEST_FIRST;
		} else {
			eop = ICP_QAT_FW_COMP_NOT_EOP;
		}
	} else {
		if (DC_REQUEST_FIRST == pSessionDesc->requestType) {
			/* Reinitialise the cumulative amount of consumed bytes
			 */
			pSessionDesc->cumulativeConsumedBytes = 0;
		}
	}

	/* (LW 14 - 15) */
	pCompReqParams = &(pMsg->comp_pars);
	dcCompRequestParamsPopulate(pCompReqParams, pCookie);
	if (CPA_DC_FLUSH_FINAL == flush) {
		bFinal = ICP_QAT_FW_COMP_BFINAL;
	}

	switch (cnvMode) {
	case DC_CNVNR:
		cnvRecovery = ICP_QAT_FW_COMP_CNV_RECOVERY;
	/* Fall through is intended here, because for CNVNR
	 * cnvDecompReq also needs to be set */
	case DC_CNV:
		cnvDecompReq = ICP_QAT_FW_COMP_CNV;
		if (isDcGen4x(pService)) {
			cnvErrorInjection = pSessionDesc->cnvErrorInjection;
		}
		break;
	case DC_NO_CNV:
		cnvDecompReq = ICP_QAT_FW_COMP_NO_CNV;
		cnvRecovery = ICP_QAT_FW_COMP_NO_CNV_RECOVERY;
		break;
	}

	/* LW 18 */
	rpCmdFlags = ICP_QAT_FW_COMP_REQ_PARAM_FLAGS_BUILD(sop,
							   eop,
							   bFinal,
							   cnvDecompReq,
							   cnvRecovery,
							   cnvErrorInjection,
							   crcMode);

	pMsg->comp_pars.req_par_flags = rpCmdFlags;

	/* Populates the QAT common request middle part of the message
	 * (LW 6 to 11) */
	SalQatMsg_CmnMidWrite((icp_qat_fw_la_bulk_req_t *)pMsg,
			      pCookie,
			      DC_DEFAULT_QAT_PTR_TYPE,
			      srcAddrPhys,
			      dstAddrPhys,
			      0,
			      0);

	return CPA_STATUS_SUCCESS;
}

/**
 *****************************************************************************
 * @ingroup Dc_DataCompression
 *      Send a compression request to QAT
 *
 * @description
 *      Send the requests for compression or decompression to QAT
 *
 * @param{in]   pCookie               Pointer to the compression cookie
 * @param[in]   pService              Pointer to the compression service
 * @param[in]   pSessionDesc          Pointer to the session descriptor
 * @param[in]   compDecomp            Direction of the operation
 *
 * @retval CPA_STATUS_SUCCESS         Function executed successfully
 * @retval CPA_STATUS_INVALID_PARAM   Invalid parameter passed in
 *
 *****************************************************************************/
static CpaStatus
dcSendRequest(dc_compression_cookie_t *pCookie,
	      sal_compression_service_t *pService,
	      dc_session_desc_t *pSessionDesc,
	      dc_request_dir_t compDecomp)
{
	CpaStatus status = CPA_STATUS_SUCCESS;

	/* Send to QAT */
	status = icp_adf_transPutMsg(pService->trans_handle_compression_tx,
				     (void *)&(pCookie->request),
				     LAC_QAT_DC_REQ_SZ_LW);

	if ((CPA_DC_STATEFUL == pSessionDesc->sessState) &&
	    (CPA_STATUS_RETRY == status)) {
		/* reset requestType after receiving an retry on
		 * the stateful request */
		pSessionDesc->requestType = pSessionDesc->previousRequestType;
	}

	return status;
}

/**
 *****************************************************************************
 * @ingroup Dc_DataCompression
 *      Process the synchronous and asynchronous case for compression or
 *      decompression
 *
 * @description
 *      Process the synchronous and asynchronous case for compression or
 *      decompression. This function will then create and send the request to
 *      the firmware.
 *
 * @param[in]   pService            Pointer to the compression service
 * @param[in]   pSessionDesc        Pointer to the session descriptor
 * @param[in]   dcInstance          Instance handle derived from discovery
 *                                  functions
 * @param[in]   pSessionHandle      Session handle
 * @param[in]   numRequests         Number of operations in the batch request
 * @param[in]   pBatchOpData        Address of the list of jobs to be processed
 * @param[in]   pSrcBuff            Pointer to data buffer for compression
 * @param[in]   pDestBuff           Pointer to buffer space for data after
 *                                  compression
 * @param[in]   pResults            Pointer to results structure
 * @param[in]   flushFlag           Indicates the type of flush to be
 *                                  performed
 * @param[in]   pOpData             Pointer to request information structure
 *                                  holding parameters for cpaDcCompress2 and
 *                                  CpaDcDecompressData2
 * @param[in]   callbackTag         Pointer to the callback tag
 * @param[in]   compDecomp          Direction of the operation
 * @param[in]   isAsyncMode         Used to know if synchronous or asynchronous
 *                                  mode
 * @param[in]   cnvMode             CNV Mode
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully
 * @retval CPA_STATUS_RETRY         Retry operation
 * @retval CPA_STATUS_FAIL          Function failed
 * @retval CPA_STATUS_RESOURCE      Resource error
 *
 *****************************************************************************/
static CpaStatus
dcCompDecompData(sal_compression_service_t *pService,
		 dc_session_desc_t *pSessionDesc,
		 CpaInstanceHandle dcInstance,
		 CpaDcSessionHandle pSessionHandle,
		 CpaBufferList *pSrcBuff,
		 CpaBufferList *pDestBuff,
		 CpaDcRqResults *pResults,
		 CpaDcFlush flushFlag,
		 CpaDcOpData *pOpData,
		 void *callbackTag,
		 dc_request_dir_t compDecomp,
		 CpaBoolean isAsyncMode,
		 dc_cnv_mode_t cnvMode)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	dc_compression_cookie_t *pCookie = NULL;

	if ((LacSync_GenWakeupSyncCaller == pSessionDesc->pCompressionCb) &&
	    isAsyncMode == CPA_TRUE) {
		lac_sync_op_data_t *pSyncCallbackData = NULL;

		status = LacSync_CreateSyncCookie(&pSyncCallbackData);

		if (CPA_STATUS_SUCCESS == status) {
			status = dcCompDecompData(pService,
						  pSessionDesc,
						  dcInstance,
						  pSessionHandle,
						  pSrcBuff,
						  pDestBuff,
						  pResults,
						  flushFlag,
						  pOpData,
						  pSyncCallbackData,
						  compDecomp,
						  CPA_FALSE,
						  cnvMode);
		} else {
			return status;
		}

		if (CPA_STATUS_SUCCESS == status) {
			CpaStatus syncStatus = CPA_STATUS_SUCCESS;

			syncStatus =
			    LacSync_WaitForCallback(pSyncCallbackData,
						    DC_SYNC_CALLBACK_TIMEOUT,
						    &status,
						    NULL);

			/* If callback doesn't come back */
			if (CPA_STATUS_SUCCESS != syncStatus) {
				if (DC_COMPRESSION_REQUEST == compDecomp) {
					COMPRESSION_STAT_INC(
					    numCompCompletedErrors, pService);
				} else {
					COMPRESSION_STAT_INC(
					    numDecompCompletedErrors, pService);
				}
				LAC_LOG_ERROR("Callback timed out");
				status = syncStatus;
			}
		} else {
			/* As the Request was not sent the Callback will never
			 * be called, so need to indicate that we're finished
			 * with cookie so it can be destroyed. */
			LacSync_SetSyncCookieComplete(pSyncCallbackData);
		}

		LacSync_DestroySyncCookie(&pSyncCallbackData);
		return status;
	}

	/* Allocate the compression cookie
	 * The memory is freed in callback or in sendRequest if an error occurs
	 */
	pCookie = (dc_compression_cookie_t *)Lac_MemPoolEntryAlloc(
	    pService->compression_mem_pool);
	if (NULL == pCookie) {
		LAC_LOG_ERROR("Cannot get mem pool entry for compression");
		status = CPA_STATUS_RESOURCE;
	} else if ((void *)CPA_STATUS_RETRY == pCookie) {
		pCookie = NULL;
		status = CPA_STATUS_RETRY;
	}

	if (CPA_STATUS_SUCCESS == status) {
		status = dcCreateRequest(pCookie,
					 pService,
					 pSessionDesc,
					 pSessionHandle,
					 pSrcBuff,
					 pDestBuff,
					 pResults,
					 flushFlag,
					 pOpData,
					 callbackTag,
					 compDecomp,
					 cnvMode);
	}

	if (CPA_STATUS_SUCCESS == status) {
		/* Increment number of pending callbacks for session */
		if (CPA_DC_STATELESS == pSessionDesc->sessState) {
			qatUtilsAtomicInc(
			    &(pSessionDesc->pendingStatelessCbCount));
		}
		status =
		    dcSendRequest(pCookie, pService, pSessionDesc, compDecomp);
	}

	if (CPA_STATUS_SUCCESS == status) {
		if (DC_COMPRESSION_REQUEST == compDecomp) {
			COMPRESSION_STAT_INC(numCompRequests, pService);
		} else {
			COMPRESSION_STAT_INC(numDecompRequests, pService);
		}
	} else {
		if (DC_COMPRESSION_REQUEST == compDecomp) {
			COMPRESSION_STAT_INC(numCompRequestsErrors, pService);
		} else {
			COMPRESSION_STAT_INC(numDecompRequestsErrors, pService);
		}

		/* Decrement number of pending callbacks for session */
		if (CPA_DC_STATELESS == pSessionDesc->sessState) {
			qatUtilsAtomicDec(
			    &(pSessionDesc->pendingStatelessCbCount));
		} else {
			qatUtilsAtomicDec(
			    &(pSessionDesc->pendingStatefulCbCount));
		}

		/* Free the memory pool */
		if (NULL != pCookie) {
			if (status != CPA_STATUS_UNSUPPORTED) {
				/* Free the memory pool */
				Lac_MemPoolEntryFree(pCookie);
				pCookie = NULL;
			}
		}
	}

	return status;
}

/**
 *****************************************************************************
 * @ingroup Dc_DataCompression
 *      Handle zero length compression or decompression requests
 *
 * @description
 *      Handle zero length compression or decompression requests
 *
 * @param[in]   pService              Pointer to the compression service
 * @param[in]   pSessionDesc          Pointer to the session descriptor
 * @param[in]   pResults              Pointer to results structure
 * @param[in]   flushFlag             Indicates the type of flush to be
 *                                    performed
 * @param[in]   callbackTag           User supplied value to help correlate
 *                                    the callback with its associated request
 * @param[in]   compDecomp            Direction of the operation
 *
 * @retval CPA_TRUE                   Zero length SOP or MOP processed
 * @retval CPA_FALSE                  Zero length EOP
 *
 *****************************************************************************/
static CpaStatus
dcZeroLengthRequests(sal_compression_service_t *pService,
		     dc_session_desc_t *pSessionDesc,
		     CpaDcRqResults *pResults,
		     CpaDcFlush flushFlag,
		     void *callbackTag,
		     dc_request_dir_t compDecomp)
{
	CpaBoolean status = CPA_FALSE;
	CpaDcCallbackFn pCbFunc = pSessionDesc->pCompressionCb;

	if (DC_REQUEST_FIRST == pSessionDesc->requestType) {
		/* Reinitialise the cumulative amount of consumed bytes */
		pSessionDesc->cumulativeConsumedBytes = 0;

		/* Zero length SOP */
		if (CPA_DC_ADLER32 == pSessionDesc->checksumType) {
			pResults->checksum = 1;
		} else {
			pResults->checksum = 0;
		}

		status = CPA_TRUE;
	} else if ((CPA_DC_FLUSH_NONE == flushFlag) ||
		   (CPA_DC_FLUSH_SYNC == flushFlag)) {
		/* Zero length MOP */
		pResults->checksum = pSessionDesc->previousChecksum;
		status = CPA_TRUE;
	}

	if (CPA_TRUE == status) {
		pResults->status = CPA_DC_OK;
		pResults->produced = 0;
		pResults->consumed = 0;

		/* Increment statistics */
		if (DC_COMPRESSION_REQUEST == compDecomp) {
			COMPRESSION_STAT_INC(numCompRequests, pService);
			COMPRESSION_STAT_INC(numCompCompleted, pService);
		} else {
			COMPRESSION_STAT_INC(numDecompRequests, pService);
			COMPRESSION_STAT_INC(numDecompCompleted, pService);
		}

		LAC_SPINUNLOCK(&(pSessionDesc->sessionLock));

		if ((NULL != pCbFunc) &&
		    (LacSync_GenWakeupSyncCaller != pCbFunc)) {
			pCbFunc(callbackTag, CPA_STATUS_SUCCESS);
		}

		return CPA_TRUE;
	}

	return CPA_FALSE;
}

static CpaStatus
dcParamCheck(CpaInstanceHandle dcInstance,
	     CpaDcSessionHandle pSessionHandle,
	     sal_compression_service_t *pService,
	     CpaBufferList *pSrcBuff,
	     CpaBufferList *pDestBuff,
	     CpaDcRqResults *pResults,
	     dc_session_desc_t *pSessionDesc,
	     CpaDcFlush flushFlag,
	     Cpa64U srcBuffSize)
{

	if (dcCheckSourceData(pSessionHandle,
			      pSrcBuff,
			      pDestBuff,
			      pResults,
			      flushFlag,
			      srcBuffSize,
			      NULL) != CPA_STATUS_SUCCESS) {
		return CPA_STATUS_INVALID_PARAM;
	}
	if (dcCheckDestinationData(
		pService, pSessionHandle, pDestBuff, DC_COMPRESSION_REQUEST) !=
	    CPA_STATUS_SUCCESS) {
		return CPA_STATUS_INVALID_PARAM;
	}
	if (CPA_DC_DIR_DECOMPRESS == pSessionDesc->sessDirection) {
		LAC_INVALID_PARAM_LOG("Invalid sessDirection value");
		return CPA_STATUS_INVALID_PARAM;
	}
	return CPA_STATUS_SUCCESS;
}

CpaStatus
cpaDcCompressData(CpaInstanceHandle dcInstance,
		  CpaDcSessionHandle pSessionHandle,
		  CpaBufferList *pSrcBuff,
		  CpaBufferList *pDestBuff,
		  CpaDcRqResults *pResults,
		  CpaDcFlush flushFlag,
		  void *callbackTag)
{
	sal_compression_service_t *pService = NULL;
	dc_session_desc_t *pSessionDesc = NULL;
	CpaInstanceHandle insHandle = NULL;
	Cpa64U srcBuffSize = 0;

	if (CPA_INSTANCE_HANDLE_SINGLE == dcInstance) {
		insHandle = dcGetFirstHandle();
	} else {
		insHandle = dcInstance;
	}

	pService = (sal_compression_service_t *)insHandle;

	LAC_CHECK_NULL_PARAM(insHandle);
	LAC_CHECK_NULL_PARAM(pSessionHandle);

	/* Check if SAL is initialised otherwise return an error */
	SAL_RUNNING_CHECK(insHandle);

	/* This check is outside the parameter checking as it is needed to
	 * manage zero length requests */
	if (LacBuffDesc_BufferListVerifyNull(pSrcBuff,
					     &srcBuffSize,
					     LAC_NO_ALIGNMENT_SHIFT) !=
	    CPA_STATUS_SUCCESS) {
		LAC_INVALID_PARAM_LOG("Invalid source buffer list parameter");
		return CPA_STATUS_INVALID_PARAM;
	}

	/* Ensure this is a compression instance */
	SAL_CHECK_INSTANCE_TYPE(insHandle, SAL_SERVICE_TYPE_COMPRESSION);

	pSessionDesc = DC_SESSION_DESC_FROM_CTX_GET(pSessionHandle);
	if (CPA_STATUS_SUCCESS !=
	    dcParamCheck(insHandle,
			 pSessionHandle,
			 pService,
			 pSrcBuff,
			 pDestBuff,
			 pResults,
			 pSessionDesc,
			 flushFlag,
			 srcBuffSize)) {
		return CPA_STATUS_INVALID_PARAM;
	}
	if (CPA_DC_STATEFUL == pSessionDesc->sessState) {
		LAC_INVALID_PARAM_LOG(
		    "Invalid session state, stateful sessions "
		    "are not supported");
		return CPA_STATUS_UNSUPPORTED;
	}

	if (!(pService->generic_service_info.dcExtendedFeatures &
	      DC_CNV_EXTENDED_CAPABILITY)) {
		LAC_INVALID_PARAM_LOG(
		    "CompressAndVerify feature not supported");
		return CPA_STATUS_UNSUPPORTED;
	}

	if (!(pService->generic_service_info.dcExtendedFeatures &
	      DC_CNVNR_EXTENDED_CAPABILITY)) {
		LAC_INVALID_PARAM_LOG(
		    "CompressAndVerifyAndRecovery feature not supported");
		return CPA_STATUS_UNSUPPORTED;
	}

	return dcCompDecompData(pService,
				pSessionDesc,
				insHandle,
				pSessionHandle,
				pSrcBuff,
				pDestBuff,
				pResults,
				flushFlag,
				NULL,
				callbackTag,
				DC_COMPRESSION_REQUEST,
				CPA_TRUE,
				DC_CNVNR);
}

CpaStatus
cpaDcCompressData2(CpaInstanceHandle dcInstance,
		   CpaDcSessionHandle pSessionHandle,
		   CpaBufferList *pSrcBuff,
		   CpaBufferList *pDestBuff,
		   CpaDcOpData *pOpData,
		   CpaDcRqResults *pResults,
		   void *callbackTag)
{
	sal_compression_service_t *pService = NULL;
	dc_session_desc_t *pSessionDesc = NULL;
	CpaInstanceHandle insHandle = NULL;
	Cpa64U srcBuffSize = 0;
	dc_cnv_mode_t cnvMode = DC_NO_CNV;

	LAC_CHECK_NULL_PARAM(pOpData);

	if (((CPA_TRUE != pOpData->compressAndVerify) &&
	     (CPA_FALSE != pOpData->compressAndVerify)) ||
	    ((CPA_FALSE != pOpData->compressAndVerifyAndRecover) &&
	     (CPA_TRUE != pOpData->compressAndVerifyAndRecover))) {
		return CPA_STATUS_INVALID_PARAM;
	}

	if ((CPA_FALSE == pOpData->compressAndVerify) &&
	    (CPA_TRUE == pOpData->compressAndVerifyAndRecover)) {
		return CPA_STATUS_INVALID_PARAM;
	}

	if ((CPA_TRUE == pOpData->compressAndVerify) &&
	    (CPA_TRUE == pOpData->compressAndVerifyAndRecover) &&
	    (CPA_FALSE == pOpData->integrityCrcCheck)) {
		return cpaDcCompressData(dcInstance,
					 pSessionHandle,
					 pSrcBuff,
					 pDestBuff,
					 pResults,
					 pOpData->flushFlag,
					 callbackTag);
	}

	if (CPA_FALSE == pOpData->compressAndVerify) {
		LAC_INVALID_PARAM_LOG(
		    "Data compression without verification not allowed");
		return CPA_STATUS_UNSUPPORTED;
	}

	if (CPA_INSTANCE_HANDLE_SINGLE == dcInstance) {
		insHandle = dcGetFirstHandle();
	} else {
		insHandle = dcInstance;
	}

	pService = (sal_compression_service_t *)insHandle;

	LAC_CHECK_NULL_PARAM(insHandle);
	LAC_CHECK_NULL_PARAM(pSessionHandle);
	LAC_CHECK_NULL_PARAM(pOpData);

	/* Check if SAL is initialised otherwise return an error */
	SAL_RUNNING_CHECK(insHandle);

	/* This check is outside the parameter checking as it is needed to
	 * manage zero length requests */
	if (LacBuffDesc_BufferListVerifyNull(pSrcBuff,
					     &srcBuffSize,
					     LAC_NO_ALIGNMENT_SHIFT) !=
	    CPA_STATUS_SUCCESS) {
		LAC_INVALID_PARAM_LOG("Invalid source buffer list parameter");
		return CPA_STATUS_INVALID_PARAM;
	}

	/* Ensure this is a compression instance */
	SAL_CHECK_INSTANCE_TYPE(insHandle, SAL_SERVICE_TYPE_COMPRESSION);

	pSessionDesc = DC_SESSION_DESC_FROM_CTX_GET(pSessionHandle);

	if (CPA_TRUE == pOpData->compressAndVerify &&
	    CPA_DC_STATEFUL == pSessionDesc->sessState) {
		LAC_INVALID_PARAM_LOG(
		    "Invalid session state, stateful sessions "
		    "not supported with CNV");
		return CPA_STATUS_UNSUPPORTED;
	}

	if (!(pService->generic_service_info.dcExtendedFeatures &
	      DC_CNV_EXTENDED_CAPABILITY) &&
	    (CPA_TRUE == pOpData->compressAndVerify)) {
		LAC_INVALID_PARAM_LOG(
		    "CompressAndVerify feature not supported");
		return CPA_STATUS_UNSUPPORTED;
	}

	if (CPA_STATUS_SUCCESS !=
	    dcParamCheck(insHandle,
			 pSessionHandle,
			 pService,
			 pSrcBuff,
			 pDestBuff,
			 pResults,
			 pSessionDesc,
			 pOpData->flushFlag,
			 srcBuffSize)) {
		return CPA_STATUS_INVALID_PARAM;
	}
	if (CPA_STATUS_SUCCESS != dcCheckOpData(pService, pOpData)) {
		return CPA_STATUS_INVALID_PARAM;
	}
	if (CPA_TRUE != pOpData->compressAndVerify) {
		if (srcBuffSize > DC_COMP_MAX_BUFF_SIZE) {
			LAC_LOG_ERROR(
			    "Compression payload greater than 64KB is "
			    "unsupported, when CnV is disabled\n");
			return CPA_STATUS_UNSUPPORTED;
		}
	}

	if (CPA_DC_STATEFUL == pSessionDesc->sessState) {
		/* Lock the session to check if there are in-flight stateful
		 * requests */
		LAC_SPINLOCK(&(pSessionDesc->sessionLock));

		/* Check if there is already one in-flight stateful request */
		if (0 !=
		    qatUtilsAtomicGet(
			&(pSessionDesc->pendingStatefulCbCount))) {
			LAC_LOG_ERROR(
			    "Only one in-flight stateful request supported");
			LAC_SPINUNLOCK(&(pSessionDesc->sessionLock));
			return CPA_STATUS_RETRY;
		}

		if (0 == srcBuffSize) {
			if (CPA_TRUE ==
			    dcZeroLengthRequests(pService,
						 pSessionDesc,
						 pResults,
						 pOpData->flushFlag,
						 callbackTag,
						 DC_COMPRESSION_REQUEST)) {
				return CPA_STATUS_SUCCESS;
			}
		}

		qatUtilsAtomicInc(&(pSessionDesc->pendingStatefulCbCount));
		LAC_SPINUNLOCK(&(pSessionDesc->sessionLock));
	}

	if (CPA_TRUE == pOpData->compressAndVerify) {
		cnvMode = DC_CNV;
	}

	return dcCompDecompData(pService,
				pSessionDesc,
				insHandle,
				pSessionHandle,
				pSrcBuff,
				pDestBuff,
				pResults,
				pOpData->flushFlag,
				pOpData,
				callbackTag,
				DC_COMPRESSION_REQUEST,
				CPA_TRUE,
				cnvMode);
}

static CpaStatus
dcDecompressDataCheck(CpaInstanceHandle insHandle,
		      CpaDcSessionHandle pSessionHandle,
		      CpaBufferList *pSrcBuff,
		      CpaBufferList *pDestBuff,
		      CpaDcRqResults *pResults,
		      CpaDcFlush flushFlag,
		      Cpa64U *srcBufferSize)
{
	sal_compression_service_t *pService = NULL;
	dc_session_desc_t *pSessionDesc = NULL;
	Cpa64U srcBuffSize = 0;

	pService = (sal_compression_service_t *)insHandle;

	LAC_CHECK_NULL_PARAM(insHandle);

	/* Check if SAL is initialised otherwise return an error */
	SAL_RUNNING_CHECK(insHandle);

	/* This check is outside the parameter checking as it is needed to
	 * manage zero length requests */
	if (LacBuffDesc_BufferListVerifyNull(pSrcBuff,
					     &srcBuffSize,
					     LAC_NO_ALIGNMENT_SHIFT) !=
	    CPA_STATUS_SUCCESS) {
		LAC_INVALID_PARAM_LOG("Invalid source buffer list parameter");
		return CPA_STATUS_INVALID_PARAM;
	}

	/* Ensure this is a compression instance */
	SAL_CHECK_INSTANCE_TYPE(insHandle, SAL_SERVICE_TYPE_COMPRESSION);

	if (dcCheckSourceData(pSessionHandle,
			      pSrcBuff,
			      pDestBuff,
			      pResults,
			      flushFlag,
			      srcBuffSize,
			      NULL) != CPA_STATUS_SUCCESS) {
		return CPA_STATUS_INVALID_PARAM;
	}
	if (dcCheckDestinationData(pService,
				   pSessionHandle,
				   pDestBuff,
				   DC_DECOMPRESSION_REQUEST) !=
	    CPA_STATUS_SUCCESS) {
		return CPA_STATUS_INVALID_PARAM;
	}
	pSessionDesc = DC_SESSION_DESC_FROM_CTX_GET(pSessionHandle);

	if (CPA_DC_DIR_COMPRESS == pSessionDesc->sessDirection) {
		LAC_INVALID_PARAM_LOG("Invalid sessDirection value");
		return CPA_STATUS_INVALID_PARAM;
	}

	*srcBufferSize = srcBuffSize;

	return CPA_STATUS_SUCCESS;
}

CpaStatus
cpaDcDecompressData(CpaInstanceHandle dcInstance,
		    CpaDcSessionHandle pSessionHandle,
		    CpaBufferList *pSrcBuff,
		    CpaBufferList *pDestBuff,
		    CpaDcRqResults *pResults,
		    CpaDcFlush flushFlag,
		    void *callbackTag)
{
	sal_compression_service_t *pService = NULL;
	dc_session_desc_t *pSessionDesc = NULL;
	CpaInstanceHandle insHandle = NULL;
	Cpa64U srcBuffSize = 0;
	CpaStatus status = CPA_STATUS_SUCCESS;

	if (CPA_INSTANCE_HANDLE_SINGLE == dcInstance) {
		insHandle = dcGetFirstHandle();
	} else {
		insHandle = dcInstance;
	}

	status = dcDecompressDataCheck(insHandle,
				       pSessionHandle,
				       pSrcBuff,
				       pDestBuff,
				       pResults,
				       flushFlag,
				       &srcBuffSize);
	if (CPA_STATUS_SUCCESS != status) {
		return status;
	}

	pService = (sal_compression_service_t *)insHandle;

	/* Check if SAL is initialised otherwise return an error */
	SAL_RUNNING_CHECK(insHandle);

	/* This check is outside the parameter checking as it is needed to
	 * manage zero length requests */
	if (CPA_STATUS_SUCCESS !=
	    LacBuffDesc_BufferListVerifyNull(pSrcBuff,
					     &srcBuffSize,
					     LAC_NO_ALIGNMENT_SHIFT)) {
		QAT_UTILS_LOG("Invalid source buffer list parameter");
		return CPA_STATUS_INVALID_PARAM;
	}

	/* Ensure this is a compression instance */
	SAL_CHECK_INSTANCE_TYPE(insHandle, SAL_SERVICE_TYPE_COMPRESSION);

	if (dcCheckSourceData(pSessionHandle,
			      pSrcBuff,
			      pDestBuff,
			      pResults,
			      flushFlag,
			      srcBuffSize,
			      NULL) != CPA_STATUS_SUCCESS) {
		return CPA_STATUS_INVALID_PARAM;
	}
	if (dcCheckDestinationData(pService,
				   pSessionHandle,
				   pDestBuff,
				   DC_DECOMPRESSION_REQUEST) !=
	    CPA_STATUS_SUCCESS) {
		return CPA_STATUS_INVALID_PARAM;
	}
	pSessionDesc = DC_SESSION_DESC_FROM_CTX_GET(pSessionHandle);

	if (CPA_DC_DIR_COMPRESS == pSessionDesc->sessDirection) {
		QAT_UTILS_LOG("Invalid sessDirection value");
		return CPA_STATUS_INVALID_PARAM;
	}

	if (CPA_DC_STATEFUL == pSessionDesc->sessState) {
		/* Lock the session to check if there are in-flight stateful
		 * requests */
		LAC_SPINLOCK(&(pSessionDesc->sessionLock));

		/* Check if there is already one in-flight stateful request */
		if (0 !=
		    qatUtilsAtomicGet(
			&(pSessionDesc->pendingStatefulCbCount))) {
			LAC_LOG_ERROR(
			    "Only one in-flight stateful request supported");
			LAC_SPINUNLOCK(&(pSessionDesc->sessionLock));
			return CPA_STATUS_RETRY;
		}

		/* Gen 4 handle 0 len requests in FW */
		if (isDcGen2x(pService)) {
			if ((0 == srcBuffSize) ||
			    ((1 == srcBuffSize) &&
			     (CPA_DC_FLUSH_FINAL != flushFlag) &&
			     (CPA_DC_FLUSH_FULL != flushFlag))) {
				if (CPA_TRUE ==
				    dcZeroLengthRequests(
					pService,
					pSessionDesc,
					pResults,
					flushFlag,
					callbackTag,
					DC_DECOMPRESSION_REQUEST)) {
					return CPA_STATUS_SUCCESS;
				}
			}
		}

		qatUtilsAtomicInc(&(pSessionDesc->pendingStatefulCbCount));
		LAC_SPINUNLOCK(&(pSessionDesc->sessionLock));
	}

	return dcCompDecompData(pService,
				pSessionDesc,
				insHandle,
				pSessionHandle,
				pSrcBuff,
				pDestBuff,
				pResults,
				flushFlag,
				NULL,
				callbackTag,
				DC_DECOMPRESSION_REQUEST,
				CPA_TRUE,
				DC_NO_CNV);
}

CpaStatus
cpaDcDecompressData2(CpaInstanceHandle dcInstance,
		     CpaDcSessionHandle pSessionHandle,
		     CpaBufferList *pSrcBuff,
		     CpaBufferList *pDestBuff,
		     CpaDcOpData *pOpData,
		     CpaDcRqResults *pResults,
		     void *callbackTag)
{
	sal_compression_service_t *pService = NULL;
	dc_session_desc_t *pSessionDesc = NULL;
	CpaInstanceHandle insHandle = NULL;
	CpaStatus status = CPA_STATUS_SUCCESS;
	Cpa64U srcBuffSize = 0;
	LAC_CHECK_NULL_PARAM(pOpData);

	if (CPA_FALSE == pOpData->integrityCrcCheck) {

		return cpaDcDecompressData(dcInstance,
					   pSessionHandle,
					   pSrcBuff,
					   pDestBuff,
					   pResults,
					   pOpData->flushFlag,
					   callbackTag);
	}

	if (CPA_INSTANCE_HANDLE_SINGLE == dcInstance) {
		insHandle = dcGetFirstHandle();
	} else {
		insHandle = dcInstance;
	}

	status = dcDecompressDataCheck(insHandle,
				       pSessionHandle,
				       pSrcBuff,
				       pDestBuff,
				       pResults,
				       pOpData->flushFlag,
				       &srcBuffSize);
	if (CPA_STATUS_SUCCESS != status) {
		return status;
	}

	pService = (sal_compression_service_t *)insHandle;

	pSessionDesc = DC_SESSION_DESC_FROM_CTX_GET(pSessionHandle);

	LAC_CHECK_NULL_PARAM(insHandle);

	/* Check if SAL is initialised otherwise return an error */
	SAL_RUNNING_CHECK(insHandle);

	/* This check is outside the parameter checking as it is needed to
	 * manage zero length requests */
	if (CPA_STATUS_SUCCESS !=
	    LacBuffDesc_BufferListVerifyNull(pSrcBuff,
					     &srcBuffSize,
					     LAC_NO_ALIGNMENT_SHIFT)) {
		QAT_UTILS_LOG("Invalid source buffer list parameter");
		return CPA_STATUS_INVALID_PARAM;
	}

	/* Ensure this is a compression instance */
	SAL_CHECK_INSTANCE_TYPE(insHandle, SAL_SERVICE_TYPE_COMPRESSION);

	if (CPA_STATUS_SUCCESS !=
	    dcCheckSourceData(pSessionHandle,
			      pSrcBuff,
			      pDestBuff,
			      pResults,
			      CPA_DC_FLUSH_NONE,
			      srcBuffSize,
			      NULL)) {
		return CPA_STATUS_INVALID_PARAM;
	}
	if (CPA_STATUS_SUCCESS !=
	    dcCheckDestinationData(pService,
				   pSessionHandle,
				   pDestBuff,
				   DC_DECOMPRESSION_REQUEST)) {
		return CPA_STATUS_INVALID_PARAM;
	}

	if (CPA_STATUS_SUCCESS != dcCheckOpData(pService, pOpData)) {
		return CPA_STATUS_INVALID_PARAM;
	}

	if (CPA_DC_DIR_COMPRESS == pSessionDesc->sessDirection) {
		QAT_UTILS_LOG("Invalid sessDirection value");
		return CPA_STATUS_INVALID_PARAM;
	}

	if (CPA_DC_STATEFUL == pSessionDesc->sessState) {
		/* Lock the session to check if there are in-flight stateful
		 * requests */
		LAC_SPINLOCK(&(pSessionDesc->sessionLock));

		/* Check if there is already one in-flight stateful request */
		if (0 !=
		    qatUtilsAtomicGet(
			&(pSessionDesc->pendingStatefulCbCount))) {
			LAC_LOG_ERROR(
			    "Only one in-flight stateful request supported");
			LAC_SPINUNLOCK(&(pSessionDesc->sessionLock));
			return CPA_STATUS_RETRY;
		}

		/* Gen 4 handle 0 len requests in FW */
		if (isDcGen2x(pService)) {
			if ((0 == srcBuffSize) ||
			    ((1 == srcBuffSize) &&
			     (CPA_DC_FLUSH_FINAL != pOpData->flushFlag) &&
			     (CPA_DC_FLUSH_FULL != pOpData->flushFlag))) {
				if (CPA_TRUE ==
				    dcZeroLengthRequests(
					pService,
					pSessionDesc,
					pResults,
					pOpData->flushFlag,
					callbackTag,
					DC_DECOMPRESSION_REQUEST)) {
					return CPA_STATUS_SUCCESS;
				}
			}
		}
		qatUtilsAtomicInc(&(pSessionDesc->pendingStatefulCbCount));
		LAC_SPINUNLOCK(&(pSessionDesc->sessionLock));
	}

	return dcCompDecompData(pService,
				pSessionDesc,
				insHandle,
				pSessionHandle,
				pSrcBuff,
				pDestBuff,
				pResults,
				pOpData->flushFlag,
				pOpData,
				callbackTag,
				DC_DECOMPRESSION_REQUEST,
				CPA_TRUE,
				DC_NO_CNV);
}
