/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */
/**
 *****************************************************************************
 * @file sal_qat_cmn_msg.c
 *
 * @ingroup SalQatCmnMessage
 *
 * @description
 *    Implementation for populating the common (across services) QAT structures.
 *
 *****************************************************************************/
#ifndef SAL_QAT_CMN_MSG_H
#define SAL_QAT_CMN_MSG_H
/*
 *******************************************************************************
 * Include public/global header files
 *******************************************************************************
 */
#include "cpa.h"

/*
 *******************************************************************************
 * Include private header files
 *******************************************************************************
 */
#include "lac_common.h"
#include "icp_accel_devices.h"
#include "qat_utils.h"

#include "cpa_cy_sym.h"
#include "lac_mem.h"
#include "lac_mem_pools.h"
#include "lac_list.h"
#include "icp_adf_transport.h"
#include "icp_adf_transport_dp.h"

#include "icp_qat_hw.h"
#include "icp_qat_fw.h"
#include "icp_qat_fw_la.h"

/**
 ******************************************************************************
 * @ingroup SalQatCmnMessage
 *      content descriptor info structure
 *
 * @description
 *      This structure contains generic information on the content descriptor
 *
 *****************************************************************************/
typedef struct sal_qat_content_desc_info_s {
	CpaPhysicalAddr hardwareSetupBlockPhys;
	/**< Physical address of hardware setup block of the content descriptor
	 */
	void *pData;
	/**< Virtual Pointer to the hardware setup block of the content
	 * descriptor */
	Cpa8U hwBlkSzQuadWords;
	/**< Hardware Setup Block size in quad words */
} sal_qat_content_desc_info_t;

/**
 *******************************************************************************
 * @ingroup SalQatCmnMessage
 *      Lookaside response handler function type
 *
 * @description
 *      This type definition specifies the function prototype for handling the
 *      response messages for a specific symmetric operation
 *
 * @param[in] lacCmdId      Look Aside Command ID
 *
 * @param[in] pOpaqueData   Pointer to Opaque Data
 *
 * @param[in] cmnRespFlags  Common Response flags
 *
 * @return void
 *
 *****************************************************************************/
typedef void (*sal_qat_resp_handler_func_t)(icp_qat_fw_la_cmd_id_t lacCmdId,
					    void *pOpaqueData,
					    icp_qat_fw_comn_flags cmnRespFlags);

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
void SalQatMsg_CmnHdrWrite(icp_qat_fw_comn_req_t *pMsg,
			   icp_qat_fw_comn_request_id_t serviceType,
			   uint8_t serviceCmdId,
			   icp_qat_fw_comn_flags cmnFlags,
			   icp_qat_fw_serv_specif_flags serviceCmdFlags);

/********************************************************************
 * @ingroup SalQatMsg_CmnMidWrite
 *
 * @description
 *      This function fills in all fields in the icp_qat_fw_comn_req_mid_t
 *      section of the Request Msg and the corresponding SGL/Flat flag
 *      in the Hdr.
 *
 * @param[in]   pReq            Pointer to 128B Request Msg buffer
 * @param[in]   pOpaqueData     Pointer to opaque data used by callback
 * @param[in]   bufferFormat    src and dst Buffers are either SGL or Flat
 format
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
void SalQatMsg_CmnMidWrite(icp_qat_fw_la_bulk_req_t *pReq,
			   const void *pOpaqueData,
			   Cpa8U bufferFormat,
			   Cpa64U srcBuffer,
			   Cpa64U dstBuffer,
			   Cpa32U srcLength,
			   Cpa32U dstLength);

/********************************************************************
 * @ingroup SalQatMsg_ContentDescHdrWrite
 *
 * @description
 *      This function fills in all fields in the
 *icp_qat_fw_comn_req_hdr_cd_pars_t
 *      section of the Request Msg.
 *
 * @param[in]   pMsg            Pointer to 128B Request Msg buffer.
 * @param[in]   pContentDescInfo content descriptor info.
 *
 * @return
 *      none
 *
 *****************************************/
void SalQatMsg_ContentDescHdrWrite(
    icp_qat_fw_comn_req_t *pMsg,
    const sal_qat_content_desc_info_t *pContentDescInfo);

/********************************************************************
 * @ingroup SalQatMsg_CtrlBlkSetToReserved
 *
 * @description
 *      This function set the whole contrle block to a reserved state.
 *
 * @param[in]   _pMsg            Pointer to 128B Request Msg buffer.
 *
 * @return
 *      none
 *
 *****************************************/
void SalQatMsg_CtrlBlkSetToReserved(icp_qat_fw_comn_req_t *_pMsg);

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
CpaStatus SalQatMsg_transPutMsg(icp_comms_trans_handle trans_handle,
				void *pqat_msg,
				Cpa32U size_in_lws,
				Cpa8U service);

/********************************************************************
 * @ingroup SalQatMsg_updateQueueTail
 *
 * @description
 *
 *
 * @param[in]   trans_handle
 *
 *
 * @return
 *      CpaStatus
 *
 *****************************************/
void SalQatMsg_updateQueueTail(icp_comms_trans_handle trans_hnd);
#endif
