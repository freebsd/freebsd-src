/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/**
 *****************************************************************************
 * @file dc_dp.c
 *
 * @defgroup cpaDcDp Data Compression Data Plane API
 *
 * @ingroup cpaDcDp
 *
 * @description
 *      Implementation of the Data Compression DP operations.
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

#include "icp_qat_fw_comp.h"

/*
*******************************************************************************
* Include private header files
*******************************************************************************
*/
#include "dc_session.h"
#include "dc_datapath.h"
#include "lac_common.h"
#include "lac_mem.h"
#include "lac_mem_pools.h"
#include "sal_types_compression.h"
#include "lac_sal.h"
#include "lac_sync.h"
#include "sal_service_state.h"
#include "sal_qat_cmn_msg.h"
#include "icp_sal_poll.h"
#include "sal_hw_gen.h"

/**
 *****************************************************************************
 * @ingroup cpaDcDp
 *      Check that pOpData is valid
 *
 * @description
 *      Check that all the parameters defined in the pOpData are valid
 *
 * @param[in]       pOpData          Pointer to a structure containing the
 *                                   request parameters
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in
 *
 *****************************************************************************/
static CpaStatus
dcDataPlaneParamCheck(const CpaDcDpOpData *pOpData)
{
	sal_compression_service_t *pService = NULL;
	dc_session_desc_t *pSessionDesc = NULL;

	LAC_CHECK_NULL_PARAM(pOpData);
	LAC_CHECK_NULL_PARAM(pOpData->dcInstance);
	LAC_CHECK_NULL_PARAM(pOpData->pSessionHandle);

	/* Ensure this is a compression instance */
	SAL_CHECK_INSTANCE_TYPE(pOpData->dcInstance,
				SAL_SERVICE_TYPE_COMPRESSION);

	pService = (sal_compression_service_t *)(pOpData->dcInstance);

	pSessionDesc = DC_SESSION_DESC_FROM_CTX_GET(pOpData->pSessionHandle);
	if (NULL == pSessionDesc) {
		QAT_UTILS_LOG("Session handle not as expected.\n");
		return CPA_STATUS_INVALID_PARAM;
	}

	if (CPA_FALSE == pSessionDesc->isDcDp) {
		QAT_UTILS_LOG("The session type should be data plane.\n");
		return CPA_STATUS_INVALID_PARAM;
	}

	/* Compressing zero byte is not supported */
	if ((CPA_DC_DIR_COMPRESS == pSessionDesc->sessDirection) &&
	    (0 == pOpData->bufferLenToCompress)) {
		QAT_UTILS_LOG("The source buffer length to compress needs to "
			      "be greater than zero byte.\n");
		return CPA_STATUS_INVALID_PARAM;
	}

	if (pOpData->sessDirection > CPA_DC_DIR_DECOMPRESS) {
		QAT_UTILS_LOG("Invalid direction of operation.\n");
		return CPA_STATUS_INVALID_PARAM;
	}

	if (0 == pOpData->srcBuffer) {
		QAT_UTILS_LOG("Invalid srcBuffer\n");
		return CPA_STATUS_INVALID_PARAM;
	}
	if (0 == pOpData->destBuffer) {
		QAT_UTILS_LOG("Invalid destBuffer\n");
		return CPA_STATUS_INVALID_PARAM;
	}
	if (pOpData->srcBuffer == pOpData->destBuffer) {
		QAT_UTILS_LOG("In place operation is not supported.\n");
		return CPA_STATUS_INVALID_PARAM;
	}
	if (0 == pOpData->thisPhys) {
		QAT_UTILS_LOG("Invalid thisPhys\n");
		return CPA_STATUS_INVALID_PARAM;
	}

	if ((CPA_TRUE != pOpData->compressAndVerify) &&
	    (CPA_FALSE != pOpData->compressAndVerify)) {
		QAT_UTILS_LOG("Invalid compressAndVerify\n");
		return CPA_STATUS_INVALID_PARAM;
	}
	if ((CPA_TRUE == pOpData->compressAndVerify) &&
	    !(pService->generic_service_info.dcExtendedFeatures &
	      DC_CNV_EXTENDED_CAPABILITY)) {
		QAT_UTILS_LOG("Invalid compressAndVerify, no CNV capability\n");
		return CPA_STATUS_UNSUPPORTED;
	}
	if ((CPA_TRUE != pOpData->compressAndVerifyAndRecover) &&
	    (CPA_FALSE != pOpData->compressAndVerifyAndRecover)) {
		QAT_UTILS_LOG("Invalid compressAndVerifyAndRecover\n");
		return CPA_STATUS_INVALID_PARAM;
	}
	if ((CPA_TRUE == pOpData->compressAndVerifyAndRecover) &&
	    (CPA_FALSE == pOpData->compressAndVerify)) {
		QAT_UTILS_LOG("CnVnR option set without setting CnV\n");
		return CPA_STATUS_INVALID_PARAM;
	}
	if ((CPA_TRUE == pOpData->compressAndVerifyAndRecover) &&
	    !(pService->generic_service_info.dcExtendedFeatures &
	      DC_CNVNR_EXTENDED_CAPABILITY)) {
		QAT_UTILS_LOG(
		    "Invalid CnVnR option set and no CnVnR capability.\n");
		return CPA_STATUS_UNSUPPORTED;
	}

	if ((CPA_DP_BUFLIST == pOpData->srcBufferLen) &&
	    (CPA_DP_BUFLIST != pOpData->destBufferLen)) {
		QAT_UTILS_LOG(
		    "The source and destination buffers need to be of the same type (both flat buffers or buffer lists).\n");
		return CPA_STATUS_INVALID_PARAM;
	}
	if ((CPA_DP_BUFLIST != pOpData->srcBufferLen) &&
	    (CPA_DP_BUFLIST == pOpData->destBufferLen)) {
		QAT_UTILS_LOG(
		    "The source and destination buffers need to be of the same type (both flat buffers or buffer lists).\n");
		return CPA_STATUS_INVALID_PARAM;
	}

	if (CPA_DP_BUFLIST != pOpData->srcBufferLen) {
		if (pOpData->srcBufferLen < pOpData->bufferLenToCompress) {
			QAT_UTILS_LOG(
			    "srcBufferLen is smaller than bufferLenToCompress.\n");
			return CPA_STATUS_INVALID_PARAM;
		}

		if (pOpData->destBufferLen < pOpData->bufferLenForData) {
			QAT_UTILS_LOG(
			    "destBufferLen is smaller than bufferLenForData.\n");
			return CPA_STATUS_INVALID_PARAM;
		}
	} else {
		/* We are assuming that there is enough memory in the source and
		 * destination buffer lists. We only receive physical addresses
		 * of the buffers so we are unable to test it here */
		LAC_CHECK_8_BYTE_ALIGNMENT(pOpData->srcBuffer);
		LAC_CHECK_8_BYTE_ALIGNMENT(pOpData->destBuffer);
	}

	LAC_CHECK_8_BYTE_ALIGNMENT(pOpData->thisPhys);

	if ((CPA_DC_DIR_COMPRESS == pSessionDesc->sessDirection) ||
	    (CPA_DC_DIR_COMBINED == pSessionDesc->sessDirection)) {
		if (CPA_DC_HT_FULL_DYNAMIC == pSessionDesc->huffType) {
			/* Check if Intermediate Buffer Array pointer is NULL */
			if (isDcGen2x(pService) &&
			    ((0 == pService->pInterBuffPtrsArrayPhyAddr) ||
			     (NULL == pService->pInterBuffPtrsArray))) {
				QAT_UTILS_LOG(
				    "No intermediate buffer defined for this instance - see cpaDcStartInstance.\n");
				return CPA_STATUS_INVALID_PARAM;
			}

			/* Ensure that the destination buffer length for data is
			 * greater
			 * or equal to 128B */
			if (pOpData->bufferLenForData <
			    DC_DEST_BUFFER_DYN_MIN_SIZE) {
				QAT_UTILS_LOG(
				    "Destination buffer length for data should be greater or equal to 128B.\n");
				return CPA_STATUS_INVALID_PARAM;
			}
		} else {
			/* Ensure that the destination buffer length for data is
			 * greater
			 * or equal to min output buffsize */
			if (pOpData->bufferLenForData <
			    pService->comp_device_data.minOutputBuffSize) {
				QAT_UTILS_LOG(
				    "Destination buffer size should be greater or equal to %d bytes.\n",
				    pService->comp_device_data
					.minOutputBuffSize);
				return CPA_STATUS_INVALID_PARAM;
			}
		}
	}

	return CPA_STATUS_SUCCESS;
}

CpaStatus
cpaDcDpGetSessionSize(CpaInstanceHandle dcInstance,
		      CpaDcSessionSetupData *pSessionData,
		      Cpa32U *pSessionSize)
{
	return dcGetSessionSize(dcInstance, pSessionData, pSessionSize, NULL);
}

CpaStatus
cpaDcDpInitSession(CpaInstanceHandle dcInstance,
		   CpaDcSessionHandle pSessionHandle,
		   CpaDcSessionSetupData *pSessionData)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	dc_session_desc_t *pSessionDesc = NULL;
	sal_compression_service_t *pService = NULL;

	LAC_CHECK_INSTANCE_HANDLE(dcInstance);
	SAL_CHECK_INSTANCE_TYPE(dcInstance, SAL_SERVICE_TYPE_COMPRESSION);

	pService = (sal_compression_service_t *)dcInstance;

	/* Check if SAL is initialised otherwise return an error */
	SAL_RUNNING_CHECK(pService);

	/* Stateful is not supported */
	if (CPA_DC_STATELESS != pSessionData->sessState) {
		QAT_UTILS_LOG("Invalid sessState value\n");
		return CPA_STATUS_INVALID_PARAM;
	}

	status =
	    dcInitSession(dcInstance, pSessionHandle, pSessionData, NULL, NULL);
	if (CPA_STATUS_SUCCESS == status) {
		pSessionDesc = DC_SESSION_DESC_FROM_CTX_GET(pSessionHandle);
		pSessionDesc->isDcDp = CPA_TRUE;

		ICP_QAT_FW_COMN_PTR_TYPE_SET(
		    pSessionDesc->reqCacheDecomp.comn_hdr.comn_req_flags,
		    DC_DP_QAT_PTR_TYPE);
		ICP_QAT_FW_COMN_PTR_TYPE_SET(
		    pSessionDesc->reqCacheComp.comn_hdr.comn_req_flags,
		    DC_DP_QAT_PTR_TYPE);
	}

	return status;
}

CpaStatus
cpaDcDpRemoveSession(const CpaInstanceHandle dcInstance,
		     CpaDcSessionHandle pSessionHandle)
{
	return cpaDcRemoveSession(dcInstance, pSessionHandle);
}

CpaStatus
cpaDcDpUpdateSession(const CpaInstanceHandle dcInstance,
		     CpaDcSessionHandle pSessionHandle,
		     CpaDcSessionUpdateData *pUpdateSessionData)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaDcDpRegCbFunc(const CpaInstanceHandle dcInstance,
		 const CpaDcDpCallbackFn pNewCb)
{
	sal_compression_service_t *pService = NULL;

	LAC_CHECK_NULL_PARAM(dcInstance);
	SAL_CHECK_INSTANCE_TYPE(dcInstance, SAL_SERVICE_TYPE_COMPRESSION);
	LAC_CHECK_NULL_PARAM(pNewCb);

	/* Check if SAL is initialised otherwise return an error */
	SAL_RUNNING_CHECK(dcInstance);

	pService = (sal_compression_service_t *)dcInstance;
	pService->pDcDpCb = pNewCb;

	return CPA_STATUS_SUCCESS;
}

/**
 *****************************************************************************
 * @ingroup cpaDcDp
 *
 * @description
 *      Writes the message to the ring
 *
 * @param[in]       pOpData          Pointer to a structure containing the
 *                                   request parameters
 * @param[in]       pCurrentQatMsg   Pointer to current QAT message on the ring
 *
 *****************************************************************************/
static void
dcDpWriteRingMsg(CpaDcDpOpData *pOpData, icp_qat_fw_comp_req_t *pCurrentQatMsg)
{
	icp_qat_fw_comp_req_t *pReqCache = NULL;
	dc_session_desc_t *pSessionDesc = NULL;
	Cpa8U bufferFormat;

	Cpa8U cnvDecompReq = ICP_QAT_FW_COMP_NO_CNV;
	Cpa8U cnvnrCompReq = ICP_QAT_FW_COMP_NO_CNV_RECOVERY;
	CpaBoolean cnvErrorInjection = ICP_QAT_FW_COMP_NO_CNV_DFX;
	sal_compression_service_t *pService = NULL;

	pService = (sal_compression_service_t *)(pOpData->dcInstance);
	pSessionDesc = DC_SESSION_DESC_FROM_CTX_GET(pOpData->pSessionHandle);

	if (CPA_DC_DIR_COMPRESS == pOpData->sessDirection) {
		pReqCache = &(pSessionDesc->reqCacheComp);
		/* CNV check */
		if (CPA_TRUE == pOpData->compressAndVerify) {
			cnvDecompReq = ICP_QAT_FW_COMP_CNV;
			if (isDcGen4x(pService)) {
				cnvErrorInjection =
				    pSessionDesc->cnvErrorInjection;
			}

			/* CNVNR check */
			if (CPA_TRUE == pOpData->compressAndVerifyAndRecover) {
				cnvnrCompReq = ICP_QAT_FW_COMP_CNV_RECOVERY;
			}
		}
	} else {
		pReqCache = &(pSessionDesc->reqCacheDecomp);
	}

	/* Fills in the template DC ET ring message - cached from the
	 * session descriptor */
	memcpy((void *)pCurrentQatMsg,
	       (void *)(pReqCache),
	       (LAC_QAT_DC_REQ_SZ_LW * LAC_LONG_WORD_IN_BYTES));

	if (CPA_DP_BUFLIST == pOpData->srcBufferLen) {
		bufferFormat = QAT_COMN_PTR_TYPE_SGL;
	} else {
		bufferFormat = QAT_COMN_PTR_TYPE_FLAT;
	}

	pCurrentQatMsg->comp_pars.req_par_flags |=
	    ICP_QAT_FW_COMP_REQ_PARAM_FLAGS_BUILD(
		ICP_QAT_FW_COMP_NOT_SOP,
		ICP_QAT_FW_COMP_NOT_EOP,
		ICP_QAT_FW_COMP_NOT_BFINAL,
		cnvDecompReq,
		cnvnrCompReq,
		cnvErrorInjection,
		ICP_QAT_FW_COMP_CRC_MODE_LEGACY);

	SalQatMsg_CmnMidWrite((icp_qat_fw_la_bulk_req_t *)pCurrentQatMsg,
			      pOpData,
			      bufferFormat,
			      pOpData->srcBuffer,
			      pOpData->destBuffer,
			      pOpData->srcBufferLen,
			      pOpData->destBufferLen);

	pCurrentQatMsg->comp_pars.comp_len = pOpData->bufferLenToCompress;
	pCurrentQatMsg->comp_pars.out_buffer_sz = pOpData->bufferLenForData;
}

CpaStatus
cpaDcDpEnqueueOp(CpaDcDpOpData *pOpData, const CpaBoolean performOpNow)
{
	icp_qat_fw_comp_req_t *pCurrentQatMsg = NULL;
	icp_comms_trans_handle trans_handle = NULL;
	dc_session_desc_t *pSessionDesc = NULL;
	CpaStatus status = CPA_STATUS_SUCCESS;

	status = dcDataPlaneParamCheck(pOpData);
	if (CPA_STATUS_SUCCESS != status) {
		return status;
	}

	if ((CPA_FALSE == pOpData->compressAndVerify) &&
	    (CPA_DC_DIR_COMPRESS == pOpData->sessDirection)) {
		return CPA_STATUS_UNSUPPORTED;
	}

	/* Check if SAL is initialised otherwise return an error */
	SAL_RUNNING_CHECK(pOpData->dcInstance);

	trans_handle = ((sal_compression_service_t *)pOpData->dcInstance)
			   ->trans_handle_compression_tx;
	pSessionDesc = DC_SESSION_DESC_FROM_CTX_GET(pOpData->pSessionHandle);

	if ((CPA_DC_DIR_COMPRESS == pOpData->sessDirection) &&
	    (CPA_DC_DIR_DECOMPRESS == pSessionDesc->sessDirection)) {
		QAT_UTILS_LOG(
		    "The session does not support this direction of operation.\n");
		return CPA_STATUS_INVALID_PARAM;
	} else if ((CPA_DC_DIR_DECOMPRESS == pOpData->sessDirection) &&
		   (CPA_DC_DIR_COMPRESS == pSessionDesc->sessDirection)) {
		QAT_UTILS_LOG(
		    "The session does not support this direction of operation.\n");
		return CPA_STATUS_INVALID_PARAM;
	}

	icp_adf_getSingleQueueAddr(trans_handle, (void **)&pCurrentQatMsg);
	if (NULL == pCurrentQatMsg) {
		return CPA_STATUS_RETRY;
	}

	dcDpWriteRingMsg(pOpData, pCurrentQatMsg);
	pSessionDesc->pendingDpStatelessCbCount++;

	if (CPA_TRUE == performOpNow) {
		SalQatMsg_updateQueueTail(trans_handle);
	}

	return CPA_STATUS_SUCCESS;
}

CpaStatus
cpaDcDpEnqueueOpBatch(const Cpa32U numberRequests,
		      CpaDcDpOpData *pOpData[],
		      const CpaBoolean performOpNow)
{
	icp_qat_fw_comp_req_t *pCurrentQatMsg = NULL;
	icp_comms_trans_handle trans_handle = NULL;
	dc_session_desc_t *pSessionDesc = NULL;
	Cpa32U i = 0;
	CpaStatus status = CPA_STATUS_SUCCESS;
	sal_compression_service_t *pService = NULL;

	LAC_CHECK_NULL_PARAM(pOpData);
	LAC_CHECK_NULL_PARAM(pOpData[0]);
	LAC_CHECK_NULL_PARAM(pOpData[0]->dcInstance);

	pService = (sal_compression_service_t *)(pOpData[0]->dcInstance);
	if ((numberRequests == 0) ||
	    (numberRequests > pService->maxNumCompConcurrentReq)) {
		QAT_UTILS_LOG(
		    "The number of requests needs to be between 1 and %d.\n",
		    pService->maxNumCompConcurrentReq);
		return CPA_STATUS_INVALID_PARAM;
	}

	for (i = 0; i < numberRequests; i++) {
		status = dcDataPlaneParamCheck(pOpData[i]);
		if (CPA_STATUS_SUCCESS != status) {
			return status;
		}

		/* Check that all instance handles and session handles are the
		 * same */
		if (pOpData[i]->dcInstance != pOpData[0]->dcInstance) {
			QAT_UTILS_LOG(
			    "All instance handles should be the same in the pOpData.\n");
			return CPA_STATUS_INVALID_PARAM;
		}

		if (pOpData[i]->pSessionHandle != pOpData[0]->pSessionHandle) {
			QAT_UTILS_LOG(
			    "All session handles should be the same in the pOpData.\n");
			return CPA_STATUS_INVALID_PARAM;
		}
	}

	for (i = 0; i < numberRequests; i++) {
		if ((CPA_FALSE == pOpData[i]->compressAndVerify) &&
		    (CPA_DC_DIR_COMPRESS == pOpData[i]->sessDirection)) {
			return CPA_STATUS_UNSUPPORTED;
		}
	}

	/* Check if SAL is initialised otherwise return an error */
	SAL_RUNNING_CHECK(pOpData[0]->dcInstance);

	trans_handle = ((sal_compression_service_t *)pOpData[0]->dcInstance)
			   ->trans_handle_compression_tx;
	pSessionDesc = DC_SESSION_DESC_FROM_CTX_GET(pOpData[0]->pSessionHandle);

	for (i = 0; i < numberRequests; i++) {
		if ((CPA_DC_DIR_COMPRESS == pOpData[i]->sessDirection) &&
		    (CPA_DC_DIR_DECOMPRESS == pSessionDesc->sessDirection)) {
			QAT_UTILS_LOG(
			    "The session does not support this direction of operation.\n");
			return CPA_STATUS_INVALID_PARAM;
		} else if ((CPA_DC_DIR_DECOMPRESS ==
			    pOpData[i]->sessDirection) &&
			   (CPA_DC_DIR_COMPRESS ==
			    pSessionDesc->sessDirection)) {
			QAT_UTILS_LOG(
			    "The session does not support this direction of operation.\n");
			return CPA_STATUS_INVALID_PARAM;
		}
	}

	icp_adf_getQueueMemory(trans_handle,
			       numberRequests,
			       (void **)&pCurrentQatMsg);
	if (NULL == pCurrentQatMsg) {
		return CPA_STATUS_RETRY;
	}

	for (i = 0; i < numberRequests; i++) {
		dcDpWriteRingMsg(pOpData[i], pCurrentQatMsg);
		icp_adf_getQueueNext(trans_handle, (void **)&pCurrentQatMsg);
	}

	pSessionDesc->pendingDpStatelessCbCount += numberRequests;

	if (CPA_TRUE == performOpNow) {
		SalQatMsg_updateQueueTail(trans_handle);
	}

	return CPA_STATUS_SUCCESS;
}

CpaStatus
icp_sal_DcPollDpInstance(CpaInstanceHandle dcInstance, Cpa32U responseQuota)
{
	icp_comms_trans_handle trans_handle = NULL;

	LAC_CHECK_INSTANCE_HANDLE(dcInstance);
	SAL_CHECK_INSTANCE_TYPE(dcInstance, SAL_SERVICE_TYPE_COMPRESSION);

	/* Check if SAL is initialised otherwise return an error */
	SAL_RUNNING_CHECK(dcInstance);

	trans_handle = ((sal_compression_service_t *)dcInstance)
			   ->trans_handle_compression_rx;

	return icp_adf_pollQueue(trans_handle, responseQuota);
}

CpaStatus
cpaDcDpPerformOpNow(CpaInstanceHandle dcInstance)
{
	icp_comms_trans_handle trans_handle = NULL;

	LAC_CHECK_NULL_PARAM(dcInstance);
	SAL_CHECK_INSTANCE_TYPE(dcInstance, SAL_SERVICE_TYPE_COMPRESSION);

	/* Check if SAL is initialised otherwise return an error */
	SAL_RUNNING_CHECK(dcInstance);

	trans_handle = ((sal_compression_service_t *)dcInstance)
			   ->trans_handle_compression_tx;

	if (CPA_TRUE == icp_adf_queueDataToSend(trans_handle)) {
		SalQatMsg_updateQueueTail(trans_handle);
	}

	return CPA_STATUS_SUCCESS;
}
