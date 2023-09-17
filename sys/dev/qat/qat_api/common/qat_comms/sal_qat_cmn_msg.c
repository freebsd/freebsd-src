/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/**
 *****************************************************************************
 * @file sal_qat_cmn_msg.h
 *
 * @defgroup SalQatCmnMessage
 *
 * @ingroup SalQatCmnMessage
 *
 * Interfaces for populating the common QAT structures for a lookaside
 * operation.
 *
 *****************************************************************************/

/*
******************************************************************************
* Include public/global header files
******************************************************************************
*/
#include "cpa.h"

/*
*******************************************************************************
* Include private header files
*******************************************************************************
*/
#include "qat_utils.h"
#include "icp_accel_devices.h"
#include "icp_qat_fw_la.h"
#include "icp_qat_hw.h"
#include "lac_common.h"
#include "lac_mem.h"
#include "sal_qat_cmn_msg.h"

/********************************************************************
 * @ingroup SalQatMsg_CmnHdrWrite
 *
 * @description
 *      This function fills in all fields in the icp_qat_fw_comn_req_hdr_t
 *      section of the Request Msg. Build LW0 + LW1 -
 *      service part of the request
 *
 * @param[in]   pMsg            Pointer to 128B Request Msg buffer
 * @param[in]   serviceType     type of service request
 * @param[in]   serviceCmdId    id for the type of service request
 * @param[in]   cmnFlags        common request flags
 * @param[in]   serviceCmdFlags service command flahgs
 *
 * @return
 *      None
 *
 *****************************************/
void
SalQatMsg_CmnHdrWrite(icp_qat_fw_comn_req_t *pMsg,
		      icp_qat_fw_comn_request_id_t serviceType,
		      uint8_t serviceCmdId,
		      icp_qat_fw_comn_flags cmnFlags,
		      icp_qat_fw_serv_specif_flags serviceCmdFlags)
{
	icp_qat_fw_comn_req_hdr_t *pHeader = &(pMsg->comn_hdr);

	/* LW0 */
	pHeader->hdr_flags =
	    ICP_QAT_FW_COMN_HDR_FLAGS_BUILD(ICP_QAT_FW_COMN_REQ_FLAG_SET);
	pHeader->service_type = (uint8_t)serviceType;
	pHeader->service_cmd_id = serviceCmdId;
	pHeader->resrvd1 = 0;
	/* LW1 */
	pHeader->comn_req_flags = cmnFlags;
	pHeader->serv_specif_flags = serviceCmdFlags;
}

/********************************************************************
 * @ingroup SalQatCmnMessage
 *
 * @description
 *      This function fills in all fields in the icp_qat_fw_comn_req_mid_t
 *      section of the Request Msg and the corresponding SGL/Flat flag
 *      in the Hdr.
 *
 * @param[in]   pReq            Pointer to 128B Request Msg buffer
 * @param[in]   pOpaqueData     Pointer to opaque data used by callback
 * @param[in]   bufferFormat    src and dst Buffers are either SGL or Flat
 *                              format
 * @param[in]   pSrcBuffer      Address of source buffer
 * @param[in]   pDstBuffer      Address of destination buffer
 * @param[in]   pSrcLength      Length of source buffer
 * @param[in]   pDstLength      Length of destination buffer
 *

 * @assumptions
 *      All fields in mid section are zero before fn is called

 * @return
 *      None
 *
 *****************************************/
void inline SalQatMsg_CmnMidWrite(icp_qat_fw_la_bulk_req_t *pReq,
				  const void *pOpaqueData,
				  Cpa8U bufferFormat,
				  Cpa64U srcBuffer,
				  Cpa64U dstBuffer,
				  Cpa32U srcLength,
				  Cpa32U dstLength)
{
	icp_qat_fw_comn_req_mid_t *pMid = &(pReq->comn_mid);

	LAC_MEM_SHARED_WRITE_FROM_PTR(pMid->opaque_data, pOpaqueData);
	pMid->src_data_addr = srcBuffer;

	/* In place */
	if (0 == dstBuffer) {
		pMid->dest_data_addr = srcBuffer;
	}
	/* Out of place */
	else {
		pMid->dest_data_addr = dstBuffer;
	}

	if (bufferFormat == QAT_COMN_PTR_TYPE_SGL) {
		/* Using ScatterGatherLists so set flag in header */
		ICP_QAT_FW_COMN_PTR_TYPE_SET(pReq->comn_hdr.comn_req_flags,
					     QAT_COMN_PTR_TYPE_SGL);

		/* Assumption: No need to set src and dest length in this case
		 * as not
		 * used */

	} else {
		/* Using Flat buffers so set flag in header */
		ICP_QAT_FW_COMN_PTR_TYPE_SET(pReq->comn_hdr.comn_req_flags,
					     QAT_COMN_PTR_TYPE_FLAT);

		pMid->src_length = srcLength;
		pMid->dst_length = dstLength;
	}
}

/********************************************************************
 * @ingroup SalQatMsg_ContentDescHdrWrite
 *
 * @description
 *      This function fills in all fields in the
 *      icp_qat_fw_comn_req_hdr_cd_pars_t section of the Request Msg.
 *
 * @param[in]   pMsg             Pointer to 128B Request Msg buffer.
 * @param[in]   pContentDescInfo content descripter info.
 *
 * @return
 *      none
 *
 *****************************************/
void
SalQatMsg_ContentDescHdrWrite(
    icp_qat_fw_comn_req_t *pMsg,
    const sal_qat_content_desc_info_t *pContentDescInfo)
{
	icp_qat_fw_comn_req_hdr_cd_pars_t *pCd_pars = &(pMsg->cd_pars);

	pCd_pars->s.content_desc_addr =
	    pContentDescInfo->hardwareSetupBlockPhys;
	pCd_pars->s.content_desc_params_sz = pContentDescInfo->hwBlkSzQuadWords;
	pCd_pars->s.content_desc_resrvd1 = 0;
	pCd_pars->s.content_desc_hdr_resrvd2 = 0;
	pCd_pars->s.content_desc_resrvd3 = 0;
}

/********************************************************************
 * @ingroup SalQatMsg_CtrlBlkSetToReserved
 *
 * @description
 *      This function sets the whole control block to a reserved state.
 *
 * @param[in]   _pMsg            Pointer to 128B Request Msg buffer.
 *
 * @return
 *      none
 *
 *****************************************/
void
SalQatMsg_CtrlBlkSetToReserved(icp_qat_fw_comn_req_t *pMsg)
{

	icp_qat_fw_comn_req_cd_ctrl_t *pCd_ctrl = &(pMsg->cd_ctrl);

	memset(pCd_ctrl, 0, sizeof(icp_qat_fw_comn_req_cd_ctrl_t));
}

/********************************************************************
 * @ingroup SalQatMsg_transPutMsg
 *
 * @description
 *
 *
 * @param[in]   trans_handle
 * @param[in]   pqat_msg
 * @param[in]   size_in_lws
 * @param[in]   service
 *
 * @return
 *      CpaStatus
 *
 *****************************************/
CpaStatus
SalQatMsg_transPutMsg(icp_comms_trans_handle trans_handle,
		      void *pqat_msg,
		      Cpa32U size_in_lws,
		      Cpa8U service)
{
	return icp_adf_transPutMsg(trans_handle, pqat_msg, size_in_lws);
}

void
SalQatMsg_updateQueueTail(icp_comms_trans_handle trans_handle)
{
	icp_adf_updateQueueTail(trans_handle);
}
