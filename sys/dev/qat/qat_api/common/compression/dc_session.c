/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/**
 *****************************************************************************
 * @file dc_session.c
 *
 * @ingroup Dc_DataCompression
 *
 * @description
 *      Implementation of the Data Compression session operations.
 *
 *****************************************************************************/

/*
 *******************************************************************************
 * Include public/global header files
 *******************************************************************************
 */
#include "cpa.h"
#include "cpa_dc.h"

#include "icp_qat_fw.h"
#include "icp_qat_fw_comp.h"
#include "icp_qat_hw.h"
#include "icp_qat_hw_20_comp.h"

/*
 *******************************************************************************
 * Include private header files
 *******************************************************************************
 */
#include "dc_session.h"
#include "dc_datapath.h"
#include "lac_mem_pools.h"
#include "sal_types_compression.h"
#include "lac_buffer_desc.h"
#include "sal_service_state.h"
#include "sal_qat_cmn_msg.h"
#include "sal_hw_gen.h"

/**
 *****************************************************************************
 * @ingroup Dc_DataCompression
 *      Check that pSessionData is valid
 *
 * @description
 *      Check that all the parameters defined in the pSessionData are valid
 *
 * @param[in]       pSessionData     Pointer to a user instantiated structure
 *                                   containing session data
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully
 * @retval CPA_STATUS_FAIL           Function failed to find device
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in
 * @retval CPA_STATUS_UNSUPPORTED    Unsupported algorithm/feature
 *
 *****************************************************************************/
CpaStatus
dcCheckSessionData(const CpaDcSessionSetupData *pSessionData,
		   CpaInstanceHandle dcInstance)
{
	CpaDcInstanceCapabilities instanceCapabilities = { 0 };

	cpaDcQueryCapabilities(dcInstance, &instanceCapabilities);

	if ((pSessionData->compLevel < CPA_DC_L1) ||
	    (pSessionData->compLevel > CPA_DC_L12)) {
		QAT_UTILS_LOG("Invalid compLevel value\n");
		return CPA_STATUS_INVALID_PARAM;
	}

	if ((pSessionData->autoSelectBestHuffmanTree < CPA_DC_ASB_DISABLED) ||
	    (pSessionData->autoSelectBestHuffmanTree > CPA_DC_ASB_ENABLED)) {
		QAT_UTILS_LOG("Invalid autoSelectBestHuffmanTree value\n");
		return CPA_STATUS_INVALID_PARAM;
	}
	if (pSessionData->compType != CPA_DC_DEFLATE) {
		QAT_UTILS_LOG("Invalid compType value\n");
		return CPA_STATUS_INVALID_PARAM;
	}

	if ((pSessionData->huffType < CPA_DC_HT_STATIC) ||
	    (pSessionData->huffType > CPA_DC_HT_FULL_DYNAMIC) ||
	    (CPA_DC_HT_PRECOMP == pSessionData->huffType)) {
		QAT_UTILS_LOG("Invalid huffType value\n");
		return CPA_STATUS_INVALID_PARAM;
	}

	if ((pSessionData->sessDirection < CPA_DC_DIR_COMPRESS) ||
	    (pSessionData->sessDirection > CPA_DC_DIR_COMBINED)) {
		QAT_UTILS_LOG("Invalid sessDirection value\n");
		return CPA_STATUS_INVALID_PARAM;
	}

	if ((pSessionData->sessState < CPA_DC_STATEFUL) ||
	    (pSessionData->sessState > CPA_DC_STATELESS)) {
		QAT_UTILS_LOG("Invalid sessState value\n");
		return CPA_STATUS_INVALID_PARAM;
	}

	if ((pSessionData->checksum < CPA_DC_NONE) ||
	    (pSessionData->checksum > CPA_DC_ADLER32)) {
		QAT_UTILS_LOG("Invalid checksum value\n");
		return CPA_STATUS_INVALID_PARAM;
	}

	return CPA_STATUS_SUCCESS;
}

/**
 *****************************************************************************
 * @ingroup Dc_DataCompression
 *      Populate the compression hardware block
 *
 * @description
 *      This function will populate the compression hardware block and update
 *      the size in bytes of the block
 *
 * @param[in]   pSessionDesc            Pointer to the session descriptor
 * @param[in]   pCompConfig             Pointer to slice config word
 * @param[in]   compDecomp              Direction of the operation
 * @param[in]   enableDmm               Delayed Match Mode
 *
 *****************************************************************************/
static void
dcCompHwBlockPopulate(sal_compression_service_t *pService,
		      dc_session_desc_t *pSessionDesc,
		      icp_qat_hw_compression_config_t *pCompConfig,
		      dc_request_dir_t compDecomp)
{
	icp_qat_hw_compression_direction_t dir =
	    ICP_QAT_HW_COMPRESSION_DIR_COMPRESS;
	icp_qat_hw_compression_algo_t algo =
	    ICP_QAT_HW_COMPRESSION_ALGO_DEFLATE;
	icp_qat_hw_compression_depth_t depth = ICP_QAT_HW_COMPRESSION_DEPTH_1;
	icp_qat_hw_compression_file_type_t filetype =
	    ICP_QAT_HW_COMPRESSION_FILE_TYPE_0;
	icp_qat_hw_compression_delayed_match_t dmm;

	/* Set the direction */
	if (DC_COMPRESSION_REQUEST == compDecomp) {
		dir = ICP_QAT_HW_COMPRESSION_DIR_COMPRESS;
	} else {
		dir = ICP_QAT_HW_COMPRESSION_DIR_DECOMPRESS;
	}

	if (CPA_DC_DEFLATE == pSessionDesc->compType) {
		algo = ICP_QAT_HW_COMPRESSION_ALGO_DEFLATE;
	} else {
		QAT_UTILS_LOG("Algorithm not supported for Compression\n");
	}

	/* Set delay match mode */
	if (CPA_TRUE == pService->comp_device_data.enableDmm) {
		dmm = ICP_QAT_HW_COMPRESSION_DELAYED_MATCH_ENABLED;
	} else {
		dmm = ICP_QAT_HW_COMPRESSION_DELAYED_MATCH_DISABLED;
	}

	/* Set the depth */
	if (DC_DECOMPRESSION_REQUEST == compDecomp) {
		depth = ICP_QAT_HW_COMPRESSION_DEPTH_1;
	} else {
		switch (pSessionDesc->compLevel) {
		case CPA_DC_L1:
			depth = ICP_QAT_HW_COMPRESSION_DEPTH_1;
			break;
		case CPA_DC_L2:
			depth = ICP_QAT_HW_COMPRESSION_DEPTH_4;
			break;
		case CPA_DC_L3:
			depth = ICP_QAT_HW_COMPRESSION_DEPTH_8;
			break;
		case CPA_DC_L4:
			depth = ICP_QAT_HW_COMPRESSION_DEPTH_16;
			break;
		default:
			depth = pService->comp_device_data
				    .highestHwCompressionDepth;
			break;
		}
	}

	/* The file type is set to ICP_QAT_HW_COMPRESSION_FILE_TYPE_0. The other
	 * modes will be used in the future for precompiled huffman trees */
	filetype = ICP_QAT_HW_COMPRESSION_FILE_TYPE_0;

	pCompConfig->lower_val = ICP_QAT_HW_COMPRESSION_CONFIG_BUILD(
	    dir, dmm, algo, depth, filetype);

	/* Upper 32-bits of the configuration word do not need to be
	 * configured with legacy devices.
	 */
	pCompConfig->upper_val = 0;
}

static void
dcCompHwBlockPopulateGen4(sal_compression_service_t *pService,
			  dc_session_desc_t *pSessionDesc,
			  icp_qat_hw_compression_config_t *pCompConfig,
			  dc_request_dir_t compDecomp)
{
	/* Compression related */
	if (DC_COMPRESSION_REQUEST == compDecomp) {
		icp_qat_hw_comp_20_config_csr_upper_t hw_comp_upper_csr;
		icp_qat_hw_comp_20_config_csr_lower_t hw_comp_lower_csr;

		memset(&hw_comp_upper_csr, 0, sizeof hw_comp_upper_csr);
		memset(&hw_comp_lower_csr, 0, sizeof hw_comp_lower_csr);

		/* Disable Literal + Length Limit Block Drop by default and
		 * enable it only for dynamic deflate compression.
		 */
		hw_comp_lower_csr.lllbd =
		    ICP_QAT_HW_COMP_20_LLLBD_CTRL_LLLBD_DISABLED;

		switch (pSessionDesc->compType) {
		case CPA_DC_DEFLATE:
			/* DEFLATE algorithm settings */
			hw_comp_lower_csr.skip_ctrl =
			    ICP_QAT_HW_COMP_20_BYTE_SKIP_3BYTE_LITERAL;

			if (CPA_DC_HT_FULL_DYNAMIC == pSessionDesc->huffType) {
				hw_comp_lower_csr.algo =
				    ICP_QAT_HW_COMP_20_HW_COMP_FORMAT_ILZ77;
			} else /* Static DEFLATE */
			{
				hw_comp_lower_csr.algo =
				    ICP_QAT_HW_COMP_20_HW_COMP_FORMAT_DEFLATE;
				hw_comp_upper_csr.scb_ctrl =
				    ICP_QAT_HW_COMP_20_SCB_CONTROL_DISABLE;
			}

			if (CPA_DC_STATEFUL == pSessionDesc->sessState) {
				hw_comp_upper_csr.som_ctrl =
				    ICP_QAT_HW_COMP_20_SOM_CONTROL_REPLAY_MODE;
			}
			break;
		default:
			QAT_UTILS_LOG("Compression algorithm not supported\n");
			break;
		}
		/* Set the search depth */
		switch (pSessionDesc->compLevel) {
		case CPA_DC_L1:
		case CPA_DC_L2:
		case CPA_DC_L3:
		case CPA_DC_L4:
		case CPA_DC_L5:
			hw_comp_lower_csr.sd =
			    ICP_QAT_HW_COMP_20_SEARCH_DEPTH_LEVEL_1;
			hw_comp_lower_csr.hash_col =
			    ICP_QAT_HW_COMP_20_SKIP_HASH_COLLISION_DONT_ALLOW;
			break;
		case CPA_DC_L6:
		case CPA_DC_L7:
		case CPA_DC_L8:
			hw_comp_lower_csr.sd =
			    ICP_QAT_HW_COMP_20_SEARCH_DEPTH_LEVEL_6;
			break;
		case CPA_DC_L9:
			hw_comp_lower_csr.sd =
			    ICP_QAT_HW_COMP_20_SEARCH_DEPTH_LEVEL_9;
			break;
		default:
			hw_comp_lower_csr.sd = pService->comp_device_data
						   .highestHwCompressionDepth;
			if ((CPA_DC_HT_FULL_DYNAMIC ==
			     pSessionDesc->huffType) &&
			    (CPA_DC_DEFLATE == pSessionDesc->compType)) {
				/* Enable Literal + Length Limit Block Drop
				 * with dynamic deflate compression when
				 * highest compression levels are selected.
				 */
				hw_comp_lower_csr.lllbd =
				    ICP_QAT_HW_COMP_20_LLLBD_CTRL_LLLBD_ENABLED;
			}
			break;
		}
		/* Same for all algorithms */
		hw_comp_lower_csr.abd = ICP_QAT_HW_COMP_20_ABD_ABD_DISABLED;
		hw_comp_lower_csr.hash_update =
		    ICP_QAT_HW_COMP_20_SKIP_HASH_UPDATE_DONT_ALLOW;
		hw_comp_lower_csr.edmm =
		    (CPA_TRUE == pService->comp_device_data.enableDmm) ?
		    ICP_QAT_HW_COMP_20_EXTENDED_DELAY_MATCH_MODE_EDMM_ENABLED :
		    ICP_QAT_HW_COMP_20_EXTENDED_DELAY_MATCH_MODE_EDMM_DISABLED;

		/* Hard-coded HW-specific values */
		hw_comp_upper_csr.nice =
		    ICP_QAT_HW_COMP_20_CONFIG_CSR_NICE_PARAM_DEFAULT_VAL;
		hw_comp_upper_csr.lazy =
		    ICP_QAT_HW_COMP_20_CONFIG_CSR_LAZY_PARAM_DEFAULT_VAL;

		pCompConfig->upper_val =
		    ICP_QAT_FW_COMP_20_BUILD_CONFIG_UPPER(hw_comp_upper_csr);

		pCompConfig->lower_val =
		    ICP_QAT_FW_COMP_20_BUILD_CONFIG_LOWER(hw_comp_lower_csr);
	} else /* Decompress */
	{
		icp_qat_hw_decomp_20_config_csr_lower_t hw_decomp_lower_csr;

		memset(&hw_decomp_lower_csr, 0, sizeof hw_decomp_lower_csr);

		/* Set the algorithm */
		if (CPA_DC_DEFLATE == pSessionDesc->compType) {
			hw_decomp_lower_csr.algo =
			    ICP_QAT_HW_DECOMP_20_HW_DECOMP_FORMAT_DEFLATE;
		} else {
			QAT_UTILS_LOG("Algorithm not supported for "
				      "Decompression\n");
		}

		pCompConfig->upper_val = 0;
		pCompConfig->lower_val =
		    ICP_QAT_FW_DECOMP_20_BUILD_CONFIG_LOWER(
			hw_decomp_lower_csr);
	}
}

/**
 *****************************************************************************
 * @ingroup Dc_DataCompression
 *      Populate the compression content descriptor
 *
 * @description
 *      This function will populate the compression content descriptor
 *
 * @param[in]   pService                Pointer to the service
 * @param[in]   pSessionDesc            Pointer to the session descriptor
 * @param[in]   contextBufferAddrPhys   Physical address of the context buffer
 * @param[out]  pMsg                    Pointer to the compression message
 * @param[in]   nextSlice               Next slice
 * @param[in]   compDecomp              Direction of the operation
 *
 *****************************************************************************/
static void
dcCompContentDescPopulate(sal_compression_service_t *pService,
			  dc_session_desc_t *pSessionDesc,
			  CpaPhysicalAddr contextBufferAddrPhys,
			  icp_qat_fw_comp_req_t *pMsg,
			  icp_qat_fw_slice_t nextSlice,
			  dc_request_dir_t compDecomp)
{

	icp_qat_fw_comp_cd_hdr_t *pCompControlBlock = NULL;
	icp_qat_hw_compression_config_t *pCompConfig = NULL;
	CpaBoolean bankEnabled = CPA_FALSE;

	pCompControlBlock = (icp_qat_fw_comp_cd_hdr_t *)&(pMsg->comp_cd_ctrl);
	pCompConfig =
	    (icp_qat_hw_compression_config_t *)(pMsg->cd_pars.sl
						    .comp_slice_cfg_word);

	ICP_QAT_FW_COMN_NEXT_ID_SET(pCompControlBlock, nextSlice);
	ICP_QAT_FW_COMN_CURR_ID_SET(pCompControlBlock, ICP_QAT_FW_SLICE_COMP);

	pCompControlBlock->comp_cfg_offset = 0;

	if ((CPA_DC_STATEFUL == pSessionDesc->sessState) &&
	    (CPA_DC_DEFLATE == pSessionDesc->compType) &&
	    (DC_DECOMPRESSION_REQUEST == compDecomp)) {
		/* Enable A, B, C, D, and E (CAMs).  */
		pCompControlBlock->ram_bank_flags =
		    ICP_QAT_FW_COMP_RAM_FLAGS_BUILD(
			ICP_QAT_FW_COMP_BANK_DISABLED, /* Bank I */
			ICP_QAT_FW_COMP_BANK_DISABLED, /* Bank H */
			ICP_QAT_FW_COMP_BANK_DISABLED, /* Bank G */
			ICP_QAT_FW_COMP_BANK_DISABLED, /* Bank F */
			ICP_QAT_FW_COMP_BANK_ENABLED,  /* Bank E */
			ICP_QAT_FW_COMP_BANK_ENABLED,  /* Bank D */
			ICP_QAT_FW_COMP_BANK_ENABLED,  /* Bank C */
			ICP_QAT_FW_COMP_BANK_ENABLED,  /* Bank B */
			ICP_QAT_FW_COMP_BANK_ENABLED); /* Bank A */
		bankEnabled = CPA_TRUE;
	} else {
		/* Disable all banks */
		pCompControlBlock->ram_bank_flags =
		    ICP_QAT_FW_COMP_RAM_FLAGS_BUILD(
			ICP_QAT_FW_COMP_BANK_DISABLED,  /* Bank I */
			ICP_QAT_FW_COMP_BANK_DISABLED,  /* Bank H */
			ICP_QAT_FW_COMP_BANK_DISABLED,  /* Bank G */
			ICP_QAT_FW_COMP_BANK_DISABLED,  /* Bank F */
			ICP_QAT_FW_COMP_BANK_DISABLED,  /* Bank E */
			ICP_QAT_FW_COMP_BANK_DISABLED,  /* Bank D */
			ICP_QAT_FW_COMP_BANK_DISABLED,  /* Bank C */
			ICP_QAT_FW_COMP_BANK_DISABLED,  /* Bank B */
			ICP_QAT_FW_COMP_BANK_DISABLED); /* Bank A */
	}

	if (DC_COMPRESSION_REQUEST == compDecomp) {
		LAC_MEM_SHARED_WRITE_VIRT_TO_PHYS_PTR_EXTERNAL(
		    pService->generic_service_info,
		    pCompControlBlock->comp_state_addr,
		    pSessionDesc->stateRegistersComp);
	} else {
		LAC_MEM_SHARED_WRITE_VIRT_TO_PHYS_PTR_EXTERNAL(
		    pService->generic_service_info,
		    pCompControlBlock->comp_state_addr,
		    pSessionDesc->stateRegistersDecomp);
	}

	if (CPA_TRUE == bankEnabled) {
		pCompControlBlock->ram_banks_addr = contextBufferAddrPhys;
	} else {
		pCompControlBlock->ram_banks_addr = 0;
	}

	pCompControlBlock->resrvd = 0;

	/* Populate Compression Hardware Setup Block */
	if (isDcGen4x(pService)) {
		dcCompHwBlockPopulateGen4(pService,
					  pSessionDesc,
					  pCompConfig,
					  compDecomp);
	} else if (isDcGen2x(pService)) {
		dcCompHwBlockPopulate(pService,
				      pSessionDesc,
				      pCompConfig,
				      compDecomp);
	} else {
		QAT_UTILS_LOG("Invalid QAT generation value\n");
	}
}

/**
 *****************************************************************************
 * @ingroup Dc_DataCompression
 *      Populate the translator content descriptor
 *
 * @description
 *      This function will populate the translator content descriptor
 *
 * @param[out]  pMsg                     Pointer to the compression message
 * @param[in]   nextSlice                Next slice
 *
 *****************************************************************************/
void
dcTransContentDescPopulate(icp_qat_fw_comp_req_t *pMsg,
			   icp_qat_fw_slice_t nextSlice)
{

	icp_qat_fw_xlt_cd_hdr_t *pTransControlBlock = NULL;
	pTransControlBlock = (icp_qat_fw_xlt_cd_hdr_t *)&(pMsg->u2.xlt_cd_ctrl);

	ICP_QAT_FW_COMN_NEXT_ID_SET(pTransControlBlock, nextSlice);
	ICP_QAT_FW_COMN_CURR_ID_SET(pTransControlBlock, ICP_QAT_FW_SLICE_XLAT);

	pTransControlBlock->resrvd1 = 0;
	pTransControlBlock->resrvd2 = 0;
	pTransControlBlock->resrvd3 = 0;
}

/**
 *****************************************************************************
 * @ingroup Dc_DataCompression
 *      Get the context size and the history size
 *
 * @description
 *      This function will get the size of the context buffer and the history
 *      buffer. The history buffer is a subset of the context buffer and its
 *      size is needed for stateful compression.

 * @param[in]   dcInstance         DC Instance Handle
 *
 * @param[in]   pSessionData       Pointer to a user instantiated
 *                                 structure containing session data
 * @param[out]  pContextSize       Pointer to the context size
 *
 * @retval CPA_STATUS_SUCCESS      Function executed successfully
 *
 *
 *****************************************************************************/
static CpaStatus
dcGetContextSize(CpaInstanceHandle dcInstance,
		 CpaDcSessionSetupData *pSessionData,
		 Cpa32U *pContextSize)
{
	sal_compression_service_t *pCompService = NULL;

	pCompService = (sal_compression_service_t *)dcInstance;

	*pContextSize = 0;
	if ((CPA_DC_STATEFUL == pSessionData->sessState) &&
	    (CPA_DC_DIR_COMPRESS != pSessionData->sessDirection)) {
		switch (pSessionData->compType) {
		case CPA_DC_DEFLATE:
			*pContextSize =
			    pCompService->comp_device_data.inflateContextSize;
			break;
		default:
			QAT_UTILS_LOG("Invalid compression algorithm.");
			return CPA_STATUS_FAIL;
		}
	}
	return CPA_STATUS_SUCCESS;
}

CpaStatus
dcGetCompressCommandId(sal_compression_service_t *pService,
		       CpaDcSessionSetupData *pSessionData,
		       Cpa8U *pDcCmdId)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	LAC_CHECK_NULL_PARAM(pService);
	LAC_CHECK_NULL_PARAM(pSessionData);
	LAC_CHECK_NULL_PARAM(pDcCmdId);

	switch (pSessionData->compType) {
	case CPA_DC_DEFLATE:
		*pDcCmdId = (CPA_DC_HT_FULL_DYNAMIC == pSessionData->huffType) ?
		    ICP_QAT_FW_COMP_CMD_DYNAMIC :
		    ICP_QAT_FW_COMP_CMD_STATIC;
		break;
	default:
		QAT_UTILS_LOG("Algorithm not supported for "
			      "compression\n");
		status = CPA_STATUS_UNSUPPORTED;
		break;
	}

	return status;
}

CpaStatus
dcGetDecompressCommandId(sal_compression_service_t *pService,
			 CpaDcSessionSetupData *pSessionData,
			 Cpa8U *pDcCmdId)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	LAC_CHECK_NULL_PARAM(pService);
	LAC_CHECK_NULL_PARAM(pSessionData);
	LAC_CHECK_NULL_PARAM(pDcCmdId);

	switch (pSessionData->compType) {
	case CPA_DC_DEFLATE:
		*pDcCmdId = ICP_QAT_FW_COMP_CMD_DECOMPRESS;
		break;
	default:
		QAT_UTILS_LOG("Algorithm not supported for "
			      "decompression\n");
		status = CPA_STATUS_UNSUPPORTED;
		break;
	}

	return status;
}

CpaStatus
dcInitSession(CpaInstanceHandle dcInstance,
	      CpaDcSessionHandle pSessionHandle,
	      CpaDcSessionSetupData *pSessionData,
	      CpaBufferList *pContextBuffer,
	      CpaDcCallbackFn callbackFn)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	sal_compression_service_t *pService = NULL;
	icp_qat_fw_comp_req_t *pReqCache = NULL;
	dc_session_desc_t *pSessionDesc = NULL;
	CpaPhysicalAddr contextAddrPhys = 0;
	CpaPhysicalAddr physAddress = 0;
	CpaPhysicalAddr physAddressAligned = 0;
	Cpa32U minContextSize = 0, historySize = 0;
	Cpa32U rpCmdFlags = 0;
	icp_qat_fw_serv_specif_flags cmdFlags = 0;
	Cpa8U secureRam = ICP_QAT_FW_COMP_ENABLE_SECURE_RAM_USED_AS_INTMD_BUF;
	Cpa8U sessType = ICP_QAT_FW_COMP_STATELESS_SESSION;
	Cpa8U autoSelectBest = ICP_QAT_FW_COMP_NOT_AUTO_SELECT_BEST;
	Cpa8U enhancedAutoSelectBest = ICP_QAT_FW_COMP_NOT_ENH_AUTO_SELECT_BEST;
	Cpa8U disableType0EnhancedAutoSelectBest =
	    ICP_QAT_FW_COMP_NOT_DISABLE_TYPE0_ENH_AUTO_SELECT_BEST;
	icp_qat_fw_la_cmd_id_t dcCmdId =
	    (icp_qat_fw_la_cmd_id_t)ICP_QAT_FW_COMP_CMD_STATIC;
	icp_qat_fw_comn_flags cmnRequestFlags = 0;
	dc_integrity_crc_fw_t *pDataIntegrityCrcs = NULL;

	cmnRequestFlags =
	    ICP_QAT_FW_COMN_FLAGS_BUILD(DC_DEFAULT_QAT_PTR_TYPE,
					QAT_COMN_CD_FLD_TYPE_16BYTE_DATA);

	pService = (sal_compression_service_t *)dcInstance;

	secureRam = pService->comp_device_data.useDevRam;

	LAC_CHECK_NULL_PARAM(pSessionHandle);
	LAC_CHECK_NULL_PARAM(pSessionData);

	/* Check that the parameters defined in the pSessionData are valid for
	 * the
	 * device */
	if (CPA_STATUS_SUCCESS !=
	    dcCheckSessionData(pSessionData, dcInstance)) {
		return CPA_STATUS_INVALID_PARAM;
	}

	if ((CPA_DC_STATEFUL == pSessionData->sessState) &&
	    (CPA_DC_DIR_DECOMPRESS != pSessionData->sessDirection)) {
		QAT_UTILS_LOG("Stateful sessions are not supported.\n");
		return CPA_STATUS_UNSUPPORTED;
	}

	/* Check for Gen4 and stateful, return error if both exist */
	if ((isDcGen4x(pService)) &&
	    (CPA_DC_STATEFUL == pSessionData->sessState &&
	     CPA_DC_DIR_DECOMPRESS != pSessionData->sessDirection)) {
		QAT_UTILS_LOG("Stateful sessions are not supported for "
			      "compression direction");
		return CPA_STATUS_UNSUPPORTED;
	}

	if ((isDcGen2x(pService)) &&
	    (CPA_DC_HT_FULL_DYNAMIC == pSessionData->huffType)) {
		/* Test if DRAM is available for the intermediate buffers */
		if ((NULL == pService->pInterBuffPtrsArray) &&
		    (0 == pService->pInterBuffPtrsArrayPhyAddr)) {
			if (CPA_DC_ASB_STATIC_DYNAMIC ==
			    pSessionData->autoSelectBestHuffmanTree) {
				/* Define the Huffman tree as static */
				pSessionData->huffType = CPA_DC_HT_STATIC;
			} else {
				QAT_UTILS_LOG(
				    "No buffer defined for this instance - "
				    "see cpaDcStartInstance.\n");
				return CPA_STATUS_RESOURCE;
			}
		}
	}

	if ((CPA_DC_STATEFUL == pSessionData->sessState) &&
	    (CPA_DC_DEFLATE == pSessionData->compType)) {
		/* Get the size of the context buffer */
		status =
		    dcGetContextSize(dcInstance, pSessionData, &minContextSize);

		if (CPA_STATUS_SUCCESS != status) {
			QAT_UTILS_LOG(
			    "Unable to get the context size of the session.\n");
			return CPA_STATUS_FAIL;
		}

		/* If the minContextSize is zero it means we will not save or
		 * restore
		 * any history */
		if (0 != minContextSize) {
			Cpa64U contextBuffSize = 0;

			LAC_CHECK_NULL_PARAM(pContextBuffer);

			if (LacBuffDesc_BufferListVerify(
				pContextBuffer,
				&contextBuffSize,
				LAC_NO_ALIGNMENT_SHIFT) != CPA_STATUS_SUCCESS) {
				return CPA_STATUS_INVALID_PARAM;
			}

			/* Ensure that the context buffer size is greater or
			 * equal
			 * to minContextSize */
			if (contextBuffSize < minContextSize) {
				QAT_UTILS_LOG(
				    "Context buffer size should be greater or equal to %d.\n",
				    minContextSize);
				return CPA_STATUS_INVALID_PARAM;
			}
		}
	}

	/* Re-align the session structure to 64 byte alignment */
	physAddress =
	    LAC_OS_VIRT_TO_PHYS_EXTERNAL(pService->generic_service_info,
					 (Cpa8U *)pSessionHandle +
					     sizeof(void *));

	if (physAddress == 0) {
		QAT_UTILS_LOG(
		    "Unable to get the physical address of the session.\n");
		return CPA_STATUS_FAIL;
	}

	physAddressAligned =
	    (CpaPhysicalAddr)LAC_ALIGN_POW2_ROUNDUP(physAddress,
						    LAC_64BYTE_ALIGNMENT);

	pSessionDesc = (dc_session_desc_t *)
	    /* Move the session pointer by the physical offset
	    between aligned and unaligned memory */
	    ((Cpa8U *)pSessionHandle + sizeof(void *) +
	     (physAddressAligned - physAddress));

	/* Save the aligned pointer in the first bytes (size of LAC_ARCH_UINT)
	 * of the session memory */
	*((LAC_ARCH_UINT *)pSessionHandle) = (LAC_ARCH_UINT)pSessionDesc;

	/* Zero the compression session */
	LAC_OS_BZERO(pSessionDesc, sizeof(dc_session_desc_t));

	/* Write the buffer descriptor for context/history */
	if (0 != minContextSize) {
		status = LacBuffDesc_BufferListDescWrite(
		    pContextBuffer,
		    &contextAddrPhys,
		    CPA_FALSE,
		    &(pService->generic_service_info));

		if (status != CPA_STATUS_SUCCESS) {
			return status;
		}

		pSessionDesc->pContextBuffer = pContextBuffer;
		pSessionDesc->historyBuffSize = historySize;
	}

	pSessionDesc->cumulativeConsumedBytes = 0;

	/* Initialise pSessionDesc */
	pSessionDesc->requestType = DC_REQUEST_FIRST;
	pSessionDesc->huffType = pSessionData->huffType;
	pSessionDesc->compType = pSessionData->compType;
	pSessionDesc->checksumType = pSessionData->checksum;
	pSessionDesc->autoSelectBestHuffmanTree =
	    pSessionData->autoSelectBestHuffmanTree;
	pSessionDesc->sessDirection = pSessionData->sessDirection;
	pSessionDesc->sessState = pSessionData->sessState;
	pSessionDesc->compLevel = pSessionData->compLevel;
	pSessionDesc->isDcDp = CPA_FALSE;
	pSessionDesc->minContextSize = minContextSize;
	pSessionDesc->isSopForCompressionProcessed = CPA_FALSE;
	pSessionDesc->isSopForDecompressionProcessed = CPA_FALSE;

	if (CPA_DC_ADLER32 == pSessionDesc->checksumType) {
		pSessionDesc->previousChecksum = 1;
	} else {
		pSessionDesc->previousChecksum = 0;
	}

	if (CPA_DC_STATEFUL == pSessionData->sessState) {
		/* Init the spinlock used to lock the access to the number of
		 * stateful
		 * in-flight requests */
		status = LAC_SPINLOCK_INIT(&(pSessionDesc->sessionLock));
		if (CPA_STATUS_SUCCESS != status) {
			QAT_UTILS_LOG(
			    "Spinlock init failed for sessionLock.\n");
			return CPA_STATUS_RESOURCE;
		}
	}

	/* For asynchronous - use the user supplied callback
	 * for synchronous - use the internal synchronous callback */
	pSessionDesc->pCompressionCb = ((void *)NULL != (void *)callbackFn) ?
	    callbackFn :
	    LacSync_GenWakeupSyncCaller;

	/* Reset the pending callback counters */
	qatUtilsAtomicSet(0, &pSessionDesc->pendingStatelessCbCount);
	qatUtilsAtomicSet(0, &pSessionDesc->pendingStatefulCbCount);
	pSessionDesc->pendingDpStatelessCbCount = 0;

	if (CPA_DC_DIR_DECOMPRESS != pSessionData->sessDirection) {
		if ((isDcGen2x(pService)) &&
		    CPA_DC_HT_FULL_DYNAMIC == pSessionData->huffType) {
			/* Populate the compression section of the content
			 * descriptor */
			dcCompContentDescPopulate(pService,
						  pSessionDesc,
						  contextAddrPhys,
						  &(pSessionDesc->reqCacheComp),
						  ICP_QAT_FW_SLICE_XLAT,
						  DC_COMPRESSION_REQUEST);

			/* Populate the translator section of the content
			 * descriptor */
			dcTransContentDescPopulate(
			    &(pSessionDesc->reqCacheComp),
			    ICP_QAT_FW_SLICE_DRAM_WR);

			if (0 != pService->pInterBuffPtrsArrayPhyAddr) {
				pReqCache = &(pSessionDesc->reqCacheComp);

				pReqCache->u1.xlt_pars.inter_buff_ptr =
				    pService->pInterBuffPtrsArrayPhyAddr;
			}
		} else {
			dcCompContentDescPopulate(pService,
						  pSessionDesc,
						  contextAddrPhys,
						  &(pSessionDesc->reqCacheComp),
						  ICP_QAT_FW_SLICE_DRAM_WR,
						  DC_COMPRESSION_REQUEST);
		}
	}

	/* Populate the compression section of the content descriptor for
	 * the decompression case or combined */
	if (CPA_DC_DIR_COMPRESS != pSessionData->sessDirection) {
		dcCompContentDescPopulate(pService,
					  pSessionDesc,
					  contextAddrPhys,
					  &(pSessionDesc->reqCacheDecomp),
					  ICP_QAT_FW_SLICE_DRAM_WR,
					  DC_DECOMPRESSION_REQUEST);
	}

	if (CPA_DC_STATEFUL == pSessionData->sessState) {
		sessType = ICP_QAT_FW_COMP_STATEFUL_SESSION;

		LAC_OS_BZERO(&pSessionDesc->stateRegistersComp,
			     sizeof(pSessionDesc->stateRegistersComp));

		LAC_OS_BZERO(&pSessionDesc->stateRegistersDecomp,
			     sizeof(pSessionDesc->stateRegistersDecomp));
	}

	/* Get physical address of E2E CRC buffer */
	pSessionDesc->physDataIntegrityCrcs = (icp_qat_addr_width_t)
	    LAC_OS_VIRT_TO_PHYS_EXTERNAL(pService->generic_service_info,
					 &pSessionDesc->dataIntegrityCrcs);
	if (0 == pSessionDesc->physDataIntegrityCrcs) {
		QAT_UTILS_LOG(
		    "Unable to get the physical address of Data Integrity buffer.\n");
		return CPA_STATUS_FAIL;
	}
	/* Initialize default CRC parameters */
	pDataIntegrityCrcs = &pSessionDesc->dataIntegrityCrcs;
	pDataIntegrityCrcs->crc32 = 0;
	pDataIntegrityCrcs->adler32 = 1;

	if (isDcGen2x(pService)) {
		pDataIntegrityCrcs->oCrc32Cpr = DC_INVALID_CRC;
		pDataIntegrityCrcs->iCrc32Cpr = DC_INVALID_CRC;
		pDataIntegrityCrcs->oCrc32Xlt = DC_INVALID_CRC;
		pDataIntegrityCrcs->iCrc32Xlt = DC_INVALID_CRC;
		pDataIntegrityCrcs->xorFlags = DC_XOR_FLAGS_DEFAULT;
		pDataIntegrityCrcs->crcPoly = DC_CRC_POLY_DEFAULT;
		pDataIntegrityCrcs->xorOut = DC_XOR_OUT_DEFAULT;
	} else {
		pDataIntegrityCrcs->iCrc64Cpr = DC_INVALID_CRC;
		pDataIntegrityCrcs->oCrc64Cpr = DC_INVALID_CRC;
		pDataIntegrityCrcs->iCrc64Xlt = DC_INVALID_CRC;
		pDataIntegrityCrcs->oCrc64Xlt = DC_INVALID_CRC;
		pDataIntegrityCrcs->crc64Poly = DC_CRC64_POLY_DEFAULT;
		pDataIntegrityCrcs->xor64Out = DC_XOR64_OUT_DEFAULT;
	}

	/* Initialise seed checksums.
	 * It initializes swCrc32I, swCrc32O, too(union).
	 */
	pSessionDesc->seedSwCrc.swCrc64I = 0;
	pSessionDesc->seedSwCrc.swCrc64O = 0;

	/* Populate the cmdFlags */
	switch (pSessionDesc->autoSelectBestHuffmanTree) {
	case CPA_DC_ASB_DISABLED:
		break;
	case CPA_DC_ASB_STATIC_DYNAMIC:
		autoSelectBest = ICP_QAT_FW_COMP_AUTO_SELECT_BEST;
		break;
	case CPA_DC_ASB_UNCOMP_STATIC_DYNAMIC_WITH_STORED_HDRS:
		autoSelectBest = ICP_QAT_FW_COMP_AUTO_SELECT_BEST;
		enhancedAutoSelectBest = ICP_QAT_FW_COMP_ENH_AUTO_SELECT_BEST;
		break;
	case CPA_DC_ASB_UNCOMP_STATIC_DYNAMIC_WITH_NO_HDRS:
		autoSelectBest = ICP_QAT_FW_COMP_AUTO_SELECT_BEST;
		enhancedAutoSelectBest = ICP_QAT_FW_COMP_ENH_AUTO_SELECT_BEST;
		disableType0EnhancedAutoSelectBest =
		    ICP_QAT_FW_COMP_DISABLE_TYPE0_ENH_AUTO_SELECT_BEST;
		break;
	case CPA_DC_ASB_ENABLED:
		if (pService->comp_device_data.asbEnableSupport == CPA_FALSE) {
			autoSelectBest = ICP_QAT_FW_COMP_AUTO_SELECT_BEST;
			enhancedAutoSelectBest =
			    ICP_QAT_FW_COMP_ENH_AUTO_SELECT_BEST;
		}
		break;
	default:
		break;
	}

	rpCmdFlags = ICP_QAT_FW_COMP_REQ_PARAM_FLAGS_BUILD(
	    ICP_QAT_FW_COMP_SOP,
	    ICP_QAT_FW_COMP_EOP,
	    ICP_QAT_FW_COMP_BFINAL,
	    ICP_QAT_FW_COMP_NO_CNV,
	    ICP_QAT_FW_COMP_NO_CNV_RECOVERY,
	    ICP_QAT_FW_COMP_NO_CNV_DFX,
	    ICP_QAT_FW_COMP_CRC_MODE_LEGACY);

	cmdFlags =
	    ICP_QAT_FW_COMP_FLAGS_BUILD(sessType,
					autoSelectBest,
					enhancedAutoSelectBest,
					disableType0EnhancedAutoSelectBest,
					secureRam);

	if (CPA_DC_DIR_DECOMPRESS != pSessionData->sessDirection) {
		status = dcGetCompressCommandId(pService,
						pSessionData,
						(Cpa8U *)&dcCmdId);
		if (CPA_STATUS_SUCCESS != status) {
			QAT_UTILS_LOG(
			    "Couldn't get compress command ID for current "
			    "session data.");

			return status;
		}
		pReqCache = &(pSessionDesc->reqCacheComp);
		pReqCache->comp_pars.req_par_flags = rpCmdFlags;
		pReqCache->comp_pars.crc.legacy.initial_adler = 1;
		pReqCache->comp_pars.crc.legacy.initial_crc32 = 0;

		/* Populate header of the common request message */
		SalQatMsg_CmnHdrWrite((icp_qat_fw_comn_req_t *)pReqCache,
				      ICP_QAT_FW_COMN_REQ_CPM_FW_COMP,
				      (uint8_t)dcCmdId,
				      cmnRequestFlags,
				      cmdFlags);
	}

	if (CPA_DC_DIR_COMPRESS != pSessionData->sessDirection) {
		status = dcGetDecompressCommandId(pService,
						  pSessionData,
						  (Cpa8U *)&dcCmdId);
		if (CPA_STATUS_SUCCESS != status) {
			QAT_UTILS_LOG(
			    "Couldn't get decompress command ID for current "
			    "session data.");

			return status;
		}
		pReqCache = &(pSessionDesc->reqCacheDecomp);
		pReqCache->comp_pars.req_par_flags = rpCmdFlags;
		pReqCache->comp_pars.crc.legacy.initial_adler = 1;
		pReqCache->comp_pars.crc.legacy.initial_crc32 = 0;

		/* Populate header of the common request message */
		SalQatMsg_CmnHdrWrite((icp_qat_fw_comn_req_t *)pReqCache,
				      ICP_QAT_FW_COMN_REQ_CPM_FW_COMP,
				      (uint8_t)dcCmdId,
				      cmnRequestFlags,
				      cmdFlags);
	}

	return status;
}

CpaStatus
cpaDcInitSession(CpaInstanceHandle dcInstance,
		 CpaDcSessionHandle pSessionHandle,
		 CpaDcSessionSetupData *pSessionData,
		 CpaBufferList *pContextBuffer,
		 CpaDcCallbackFn callbackFn)
{
	CpaInstanceHandle insHandle = NULL;
	sal_compression_service_t *pService = NULL;

	if (CPA_INSTANCE_HANDLE_SINGLE == dcInstance) {
		insHandle = dcGetFirstHandle();
	} else {
		insHandle = dcInstance;
	}

	LAC_CHECK_INSTANCE_HANDLE(insHandle);
	SAL_CHECK_INSTANCE_TYPE(insHandle, SAL_SERVICE_TYPE_COMPRESSION);

	pService = (sal_compression_service_t *)insHandle;

	/* Check if SAL is initialised otherwise return an error */
	SAL_RUNNING_CHECK(pService);

	return dcInitSession(insHandle,
			     pSessionHandle,
			     pSessionData,
			     pContextBuffer,
			     callbackFn);
}

CpaStatus
cpaDcResetSession(const CpaInstanceHandle dcInstance,
		  CpaDcSessionHandle pSessionHandle)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	CpaInstanceHandle insHandle = NULL;
	sal_compression_service_t *pService = NULL;
	dc_session_desc_t *pSessionDesc = NULL;
	Cpa64U numPendingStateless = 0;
	Cpa64U numPendingStateful = 0;
	icp_comms_trans_handle trans_handle = NULL;
	dc_integrity_crc_fw_t *pDataIntegrityCrcs = NULL;
	dc_sw_checksums_t *pSwCrcs = NULL;

	LAC_CHECK_NULL_PARAM(pSessionHandle);
	pSessionDesc = DC_SESSION_DESC_FROM_CTX_GET(pSessionHandle);
	LAC_CHECK_NULL_PARAM(pSessionDesc);

	if (CPA_TRUE == pSessionDesc->isDcDp) {
		insHandle = dcInstance;
	} else {
		if (CPA_INSTANCE_HANDLE_SINGLE == dcInstance) {
			insHandle = dcGetFirstHandle();
		} else {
			insHandle = dcInstance;
		}
	}
	LAC_CHECK_NULL_PARAM(insHandle);
	SAL_CHECK_INSTANCE_TYPE(insHandle, SAL_SERVICE_TYPE_COMPRESSION);
	/* Check if SAL is running otherwise return an error */
	SAL_RUNNING_CHECK(insHandle);
	if (CPA_TRUE == pSessionDesc->isDcDp) {
		trans_handle = ((sal_compression_service_t *)insHandle)
				   ->trans_handle_compression_tx;
		if (CPA_TRUE == icp_adf_queueDataToSend(trans_handle)) {
			/* Process the remaining messages on the ring */
			SalQatMsg_updateQueueTail(trans_handle);
			QAT_UTILS_LOG(
			    "There are remaining messages on the ring\n");
			return CPA_STATUS_RETRY;
		}

		/* Check if there are stateless pending requests */
		if (0 != pSessionDesc->pendingDpStatelessCbCount) {
			QAT_UTILS_LOG(
			    "There are %llu stateless DP requests pending.\n",
			    (unsigned long long)
				pSessionDesc->pendingDpStatelessCbCount);
			return CPA_STATUS_RETRY;
		}
	} else {
		numPendingStateless =
		    qatUtilsAtomicGet(&(pSessionDesc->pendingStatelessCbCount));
		numPendingStateful =
		    qatUtilsAtomicGet(&(pSessionDesc->pendingStatefulCbCount));
		/* Check if there are stateless pending requests */
		if (0 != numPendingStateless) {
			QAT_UTILS_LOG(
			    "There are %llu stateless requests pending.\n",
			    (unsigned long long)numPendingStateless);
			return CPA_STATUS_RETRY;
		}
		/* Check if there are stateful pending requests */
		if (0 != numPendingStateful) {
			QAT_UTILS_LOG(
			    "There are %llu stateful requests pending.\n",
			    (unsigned long long)numPendingStateful);
			return CPA_STATUS_RETRY;
		}

		/* Reset pSessionDesc */
		pSessionDesc->requestType = DC_REQUEST_FIRST;
		pSessionDesc->cumulativeConsumedBytes = 0;
		if (CPA_DC_ADLER32 == pSessionDesc->checksumType) {
			pSessionDesc->previousChecksum = 1;
		} else {
			pSessionDesc->previousChecksum = 0;
		}
		pSessionDesc->cnvErrorInjection = ICP_QAT_FW_COMP_NO_CNV_DFX;

		/* Reset integrity CRCs to default parameters. */
		pDataIntegrityCrcs = &pSessionDesc->dataIntegrityCrcs;
		memset(pDataIntegrityCrcs, 0, sizeof(dc_integrity_crc_fw_t));
		pDataIntegrityCrcs->adler32 = 1;

		pService = (sal_compression_service_t *)insHandle;
		if (isDcGen2x(pService)) {
			pDataIntegrityCrcs->xorFlags = DC_XOR_FLAGS_DEFAULT;
			pDataIntegrityCrcs->crcPoly = DC_CRC_POLY_DEFAULT;
			pDataIntegrityCrcs->xorOut = DC_XOR_OUT_DEFAULT;
		} else {
			pDataIntegrityCrcs->crc64Poly = DC_CRC64_POLY_DEFAULT;
			pDataIntegrityCrcs->xor64Out = DC_XOR64_OUT_DEFAULT;
		}

		/* Reset seed SW checksums. */
		pSwCrcs = &pSessionDesc->seedSwCrc;
		memset(pSwCrcs, 0, sizeof(dc_sw_checksums_t));

		/* Reset integrity SW checksums. */
		pSwCrcs = &pSessionDesc->integritySwCrc;
		memset(pSwCrcs, 0, sizeof(dc_sw_checksums_t));
	}

	/* Reset the pending callback counters */
	qatUtilsAtomicSet(0, &pSessionDesc->pendingStatelessCbCount);
	qatUtilsAtomicSet(0, &pSessionDesc->pendingStatefulCbCount);
	pSessionDesc->pendingDpStatelessCbCount = 0;
	if (CPA_DC_STATEFUL == pSessionDesc->sessState) {
		LAC_OS_BZERO(&pSessionDesc->stateRegistersComp,
			     sizeof(pSessionDesc->stateRegistersComp));
		LAC_OS_BZERO(&pSessionDesc->stateRegistersDecomp,
			     sizeof(pSessionDesc->stateRegistersDecomp));
	}
	return status;
}

CpaStatus
cpaDcResetXXHashState(const CpaInstanceHandle dcInstance,
		      CpaDcSessionHandle pSessionHandle)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaDcUpdateSession(const CpaInstanceHandle dcInstance,
		   CpaDcSessionHandle pSessionHandle,
		   CpaDcSessionUpdateData *pUpdateSessionData)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaDcRemoveSession(const CpaInstanceHandle dcInstance,
		   CpaDcSessionHandle pSessionHandle)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	CpaInstanceHandle insHandle = NULL;
	dc_session_desc_t *pSessionDesc = NULL;
	Cpa64U numPendingStateless = 0;
	Cpa64U numPendingStateful = 0;
	icp_comms_trans_handle trans_handle = NULL;

	LAC_CHECK_NULL_PARAM(pSessionHandle);
	pSessionDesc = DC_SESSION_DESC_FROM_CTX_GET(pSessionHandle);
	LAC_CHECK_NULL_PARAM(pSessionDesc);

	if (CPA_TRUE == pSessionDesc->isDcDp) {
		insHandle = dcInstance;
	} else {
		if (CPA_INSTANCE_HANDLE_SINGLE == dcInstance) {
			insHandle = dcGetFirstHandle();
		} else {
			insHandle = dcInstance;
		}
	}

	LAC_CHECK_NULL_PARAM(insHandle);
	SAL_CHECK_INSTANCE_TYPE(insHandle, SAL_SERVICE_TYPE_COMPRESSION);

	/* Check if SAL is running otherwise return an error */
	SAL_RUNNING_CHECK(insHandle);

	if (CPA_TRUE == pSessionDesc->isDcDp) {
		trans_handle = ((sal_compression_service_t *)insHandle)
				   ->trans_handle_compression_tx;

		if (CPA_TRUE == icp_adf_queueDataToSend(trans_handle)) {
			/* Process the remaining messages on the ring */
			SalQatMsg_updateQueueTail(trans_handle);
			QAT_UTILS_LOG(
			    "There are remaining messages on the ring.\n");
			return CPA_STATUS_RETRY;
		}

		/* Check if there are stateless pending requests */
		if (0 != pSessionDesc->pendingDpStatelessCbCount) {
			QAT_UTILS_LOG(
			    "There are %llu stateless DP requests pending.\n",
			    (unsigned long long)
				pSessionDesc->pendingDpStatelessCbCount);
			return CPA_STATUS_RETRY;
		}
	} else {
		numPendingStateless =
		    qatUtilsAtomicGet(&(pSessionDesc->pendingStatelessCbCount));
		numPendingStateful =
		    qatUtilsAtomicGet(&(pSessionDesc->pendingStatefulCbCount));

		/* Check if there are stateless pending requests */
		if (0 != numPendingStateless) {
			QAT_UTILS_LOG(
			    "There are %llu stateless requests pending.\n",
			    (unsigned long long)numPendingStateless);
			status = CPA_STATUS_RETRY;
		}

		/* Check if there are stateful pending requests */
		if (0 != numPendingStateful) {
			QAT_UTILS_LOG(
			    "There are %llu stateful requests pending.\n",
			    (unsigned long long)numPendingStateful);
			status = CPA_STATUS_RETRY;
		}
		if ((CPA_DC_STATEFUL == pSessionDesc->sessState) &&
		    (CPA_STATUS_SUCCESS == status)) {
			LAC_SPINLOCK_DESTROY(&(pSessionDesc->sessionLock));
		}
	}

	return status;
}

CpaStatus
dcGetSessionSize(CpaInstanceHandle dcInstance,
		 CpaDcSessionSetupData *pSessionData,
		 Cpa32U *pSessionSize,
		 Cpa32U *pContextSize)
{

	CpaStatus status = CPA_STATUS_SUCCESS;
	CpaInstanceHandle insHandle = NULL;

	if (CPA_INSTANCE_HANDLE_SINGLE == dcInstance) {
		insHandle = dcGetFirstHandle();
	} else {
		insHandle = dcInstance;
	}

	/* Check parameters */
	LAC_CHECK_NULL_PARAM(insHandle);
	LAC_CHECK_NULL_PARAM(pSessionData);
	LAC_CHECK_NULL_PARAM(pSessionSize);

	if (dcCheckSessionData(pSessionData, insHandle) != CPA_STATUS_SUCCESS) {
		return CPA_STATUS_INVALID_PARAM;
	}

	/* Get session size for session data */
	*pSessionSize = sizeof(dc_session_desc_t) + LAC_64BYTE_ALIGNMENT +
	    sizeof(LAC_ARCH_UINT);

	if (NULL != pContextSize) {
		status =
		    dcGetContextSize(insHandle, pSessionData, pContextSize);

		if (CPA_STATUS_SUCCESS != status) {
			QAT_UTILS_LOG(
			    "Unable to get the context size of the session.\n");
			return CPA_STATUS_FAIL;
		}
	}

	return CPA_STATUS_SUCCESS;
}

CpaStatus
cpaDcGetSessionSize(CpaInstanceHandle dcInstance,
		    CpaDcSessionSetupData *pSessionData,
		    Cpa32U *pSessionSize,
		    Cpa32U *pContextSize)
{

	LAC_CHECK_NULL_PARAM(pContextSize);

	return dcGetSessionSize(dcInstance,
				pSessionData,
				pSessionSize,
				pContextSize);
}

CpaStatus
dcSetCnvError(CpaInstanceHandle dcInstance, CpaDcSessionHandle pSessionHandle)
{
	LAC_CHECK_NULL_PARAM(pSessionHandle);

	dc_session_desc_t *pSessionDesc = NULL;
	CpaInstanceHandle insHandle = NULL;
	sal_compression_service_t *pService = NULL;

	if (CPA_INSTANCE_HANDLE_SINGLE == dcInstance) {
		insHandle = dcGetFirstHandle();
	} else {
		insHandle = dcInstance;
	}

	pService = (sal_compression_service_t *)insHandle;

	if (isDcGen2x(pService)) {
		QAT_UTILS_LOG("Unsupported compression feature.\n");
		return CPA_STATUS_UNSUPPORTED;
	}
	pSessionDesc = DC_SESSION_DESC_FROM_CTX_GET(pSessionHandle);

	LAC_CHECK_NULL_PARAM(pSessionDesc);

	pSessionDesc->cnvErrorInjection = ICP_QAT_FW_COMP_CNV_DFX;

	return CPA_STATUS_SUCCESS;
}
