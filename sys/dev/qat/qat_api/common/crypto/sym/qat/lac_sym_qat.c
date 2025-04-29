/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */

/**
 *****************************************************************************
 * @file lac_sym_qat.c Interfaces for populating the symmetric qat structures
 *
 * @ingroup LacSymQat
 *
 *****************************************************************************/

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
#include "icp_accel_devices.h"
#include "icp_adf_cfg.h"
#include "lac_log.h"
#include "lac_sym.h"
#include "lac_sym_qat.h"
#include "lac_sal_types_crypto.h"
#include "sal_string_parse.h"
#include "lac_sym_key.h"
#include "lac_sym_qat_hash_defs_lookup.h"
#include "lac_sym_qat_constants_table.h"
#include "lac_sym_qat_cipher.h"
#include "lac_sym_qat_hash.h"

#define EMBEDDED_CIPHER_KEY_MAX_SIZE 16
static void
LacSymQat_SymLogSliceHangError(icp_qat_fw_la_cmd_id_t symCmdId)
{
	Cpa8U cmdId = symCmdId;

	switch (cmdId) {
	case ICP_QAT_FW_LA_CMD_CIPHER:
	case ICP_QAT_FW_LA_CMD_CIPHER_PRE_COMP:
		LAC_LOG_ERROR("slice hang detected on CPM cipher slice.");
		break;

	case ICP_QAT_FW_LA_CMD_AUTH:
	case ICP_QAT_FW_LA_CMD_AUTH_PRE_COMP:
		LAC_LOG_ERROR("slice hang detected on CPM auth slice.");
		break;

	case ICP_QAT_FW_LA_CMD_CIPHER_HASH:
	case ICP_QAT_FW_LA_CMD_HASH_CIPHER:
	case ICP_QAT_FW_LA_CMD_SSL3_KEY_DERIVE:
	case ICP_QAT_FW_LA_CMD_TLS_V1_1_KEY_DERIVE:
	case ICP_QAT_FW_LA_CMD_TLS_V1_2_KEY_DERIVE:
	case ICP_QAT_FW_LA_CMD_MGF1:
	default:
		LAC_LOG_ERROR(
		    "slice hang detected on CPM cipher or auth slice.");
	}
	return;
}

/* sym crypto response handlers */
static sal_qat_resp_handler_func_t
    respHandlerSymTbl[ICP_QAT_FW_LA_CMD_DELIMITER];

void
LacSymQat_SymRespHandler(void *pRespMsg)
{
	Cpa8U lacCmdId = 0;
	void *pOpaqueData = NULL;
	icp_qat_fw_la_resp_t *pRespMsgFn = NULL;
	Cpa8U opStatus = ICP_QAT_FW_COMN_STATUS_FLAG_OK;
	Cpa8U comnErr = ERR_CODE_NO_ERROR;

	pRespMsgFn = (icp_qat_fw_la_resp_t *)pRespMsg;
	LAC_MEM_SHARED_READ_TO_PTR(pRespMsgFn->opaque_data, pOpaqueData);

	lacCmdId = pRespMsgFn->comn_resp.cmd_id;
	opStatus = pRespMsgFn->comn_resp.comn_status;
	comnErr = pRespMsgFn->comn_resp.comn_error.s.comn_err_code;

	/* log the slice hang and endpoint push/pull error inside the response
	 */
	if (ERR_CODE_SSM_ERROR == (Cpa8S)comnErr) {
		LacSymQat_SymLogSliceHangError(lacCmdId);
	} else if (ERR_CODE_ENDPOINT_ERROR == (Cpa8S)comnErr) {
		LAC_LOG_ERROR("The PCIe End Point Push/Pull or"
			      " TI/RI Parity error detected.");
	}

	/* call the response message handler registered for the command ID */
	respHandlerSymTbl[lacCmdId]((icp_qat_fw_la_cmd_id_t)lacCmdId,
				    pOpaqueData,
				    (icp_qat_fw_comn_flags)opStatus);
}

CpaStatus
LacSymQat_Init(CpaInstanceHandle instanceHandle)
{
	CpaStatus status = CPA_STATUS_SUCCESS;

	/* Initialize the SHRAM constants table */
	LacSymQat_ConstantsInitLookupTables(instanceHandle);

	/* Initialise the Hash lookup table */
	status = LacSymQat_HashLookupInit(instanceHandle);

	return status;
}

void
LacSymQat_RespHandlerRegister(icp_qat_fw_la_cmd_id_t lacCmdId,
			      sal_qat_resp_handler_func_t pCbHandler)
{
	if (lacCmdId >= ICP_QAT_FW_LA_CMD_DELIMITER) {
		QAT_UTILS_LOG("Invalid Command ID\n");
		return;
	}

	/* set the response handler for the command ID */
	respHandlerSymTbl[lacCmdId] = pCbHandler;
}

void
LacSymQat_LaPacketCommandFlagSet(Cpa32U qatPacketType,
				 icp_qat_fw_la_cmd_id_t laCmdId,
				 CpaCySymCipherAlgorithm cipherAlgorithm,
				 Cpa16U *pLaCommandFlags,
				 Cpa32U ivLenInBytes)
{
	/* For SM4/Chacha ciphers set command flag as partial none to proceed
	 * with stateless processing */
	if (LAC_CIPHER_IS_SM4(cipherAlgorithm) ||
	    LAC_CIPHER_IS_CHACHA(cipherAlgorithm)) {
		ICP_QAT_FW_LA_PARTIAL_SET(*pLaCommandFlags,
					  ICP_QAT_FW_LA_PARTIAL_NONE);
		return;
	}
	ICP_QAT_FW_LA_PARTIAL_SET(*pLaCommandFlags, qatPacketType);

	/* For ECB-mode ciphers, IV is NULL so update-state flag
	 * must be disabled always.
	 * For all other ciphers and auth
	 * update state is disabled for full packets and final partials */
	if ((ICP_QAT_FW_LA_PARTIAL_NONE == qatPacketType) ||
	    (ICP_QAT_FW_LA_PARTIAL_END == qatPacketType) ||
	    ((laCmdId != ICP_QAT_FW_LA_CMD_AUTH) &&
	     LAC_CIPHER_IS_ECB_MODE(cipherAlgorithm))) {
		ICP_QAT_FW_LA_UPDATE_STATE_SET(*pLaCommandFlags,
					       ICP_QAT_FW_LA_NO_UPDATE_STATE);
	}
	/* For first or middle partials set the update state command flag */
	else {
		ICP_QAT_FW_LA_UPDATE_STATE_SET(*pLaCommandFlags,
					       ICP_QAT_FW_LA_UPDATE_STATE);

		if (laCmdId == ICP_QAT_FW_LA_CMD_AUTH) {
			/* For hash only partial - verify and return auth result
			 * are
			 * disabled */
			ICP_QAT_FW_LA_RET_AUTH_SET(
			    *pLaCommandFlags, ICP_QAT_FW_LA_NO_RET_AUTH_RES);

			ICP_QAT_FW_LA_CMP_AUTH_SET(
			    *pLaCommandFlags, ICP_QAT_FW_LA_NO_CMP_AUTH_RES);
		}
	}

	if ((LAC_CIPHER_IS_GCM(cipherAlgorithm)) &&
	    (LAC_CIPHER_IV_SIZE_GCM_12 == ivLenInBytes))

	{
		ICP_QAT_FW_LA_GCM_IV_LEN_FLAG_SET(
		    *pLaCommandFlags, ICP_QAT_FW_LA_GCM_IV_LEN_12_OCTETS);
	}
}

void
LacSymQat_packetTypeGet(CpaCySymPacketType packetType,
			CpaCySymPacketType packetState,
			Cpa32U *pQatPacketType)
{
	switch (packetType) {
	/* partial */
	case CPA_CY_SYM_PACKET_TYPE_PARTIAL:
		/* if the previous state was full, then this is the first packet
		 */
		if (CPA_CY_SYM_PACKET_TYPE_FULL == packetState) {
			*pQatPacketType = ICP_QAT_FW_LA_PARTIAL_START;
		} else {
			*pQatPacketType = ICP_QAT_FW_LA_PARTIAL_MID;
		}
		break;

	/* final partial */
	case CPA_CY_SYM_PACKET_TYPE_LAST_PARTIAL:
		*pQatPacketType = ICP_QAT_FW_LA_PARTIAL_END;
		break;

	/* full packet - CPA_CY_SYM_PACKET_TYPE_FULL */
	default:
		*pQatPacketType = ICP_QAT_FW_LA_PARTIAL_NONE;
	}
}

void
LacSymQat_LaSetDefaultFlags(icp_qat_fw_serv_specif_flags *laCmdFlags,
			    CpaCySymOp symOp)
{

	ICP_QAT_FW_LA_PARTIAL_SET(*laCmdFlags, ICP_QAT_FW_LA_PARTIAL_NONE);

	ICP_QAT_FW_LA_UPDATE_STATE_SET(*laCmdFlags,
				       ICP_QAT_FW_LA_NO_UPDATE_STATE);

	if (symOp != CPA_CY_SYM_OP_CIPHER) {
		ICP_QAT_FW_LA_RET_AUTH_SET(*laCmdFlags,
					   ICP_QAT_FW_LA_RET_AUTH_RES);
	} else {
		ICP_QAT_FW_LA_RET_AUTH_SET(*laCmdFlags,
					   ICP_QAT_FW_LA_NO_RET_AUTH_RES);
	}

	ICP_QAT_FW_LA_CMP_AUTH_SET(*laCmdFlags, ICP_QAT_FW_LA_NO_CMP_AUTH_RES);

	ICP_QAT_FW_LA_GCM_IV_LEN_FLAG_SET(
	    *laCmdFlags, ICP_QAT_FW_LA_GCM_IV_LEN_NOT_12_OCTETS);
}

CpaBoolean
LacSymQat_UseSymConstantsTable(lac_session_desc_t *pSession,
			       Cpa8U *pCipherOffset,
			       Cpa8U *pHashOffset)
{

	CpaBoolean useOptimisedContentDesc = CPA_FALSE;
	CpaBoolean useSHRAMConstants = CPA_FALSE;

	*pCipherOffset = 0;
	*pHashOffset = 0;

	/* for chaining can we use the optimised content descriptor */
	if (pSession->laCmdId == ICP_QAT_FW_LA_CMD_CIPHER_HASH ||
	    pSession->laCmdId == ICP_QAT_FW_LA_CMD_HASH_CIPHER) {
		useOptimisedContentDesc =
		    LacSymQat_UseOptimisedContentDesc(pSession);
	}

	/* Cipher-only case or chaining */
	if (pSession->laCmdId == ICP_QAT_FW_LA_CMD_CIPHER ||
	    useOptimisedContentDesc) {
		icp_qat_hw_cipher_algo_t algorithm;
		icp_qat_hw_cipher_mode_t mode;
		icp_qat_hw_cipher_dir_t dir;
		icp_qat_hw_cipher_convert_t key_convert;

		if (pSession->cipherKeyLenInBytes >
		    sizeof(icp_qat_fw_comn_req_hdr_cd_pars_t)) {
			return CPA_FALSE;
		}

		LacSymQat_CipherGetCfgData(
		    pSession, &algorithm, &mode, &dir, &key_convert);

		/* Check if cipher config is available in table. */
		LacSymQat_ConstantsGetCipherOffset(pSession->pInstance,
						   algorithm,
						   mode,
						   dir,
						   key_convert,
						   pCipherOffset);
		if (*pCipherOffset > 0) {
			useSHRAMConstants = CPA_TRUE;
		} else {
			useSHRAMConstants = CPA_FALSE;
		}
	}

	/* hash only case or when chaining, cipher must be found in SHRAM table
	 * for
	 * optimised CD case */
	if (pSession->laCmdId == ICP_QAT_FW_LA_CMD_AUTH ||
	    (useOptimisedContentDesc && useSHRAMConstants)) {
		icp_qat_hw_auth_algo_t algorithm;
		CpaBoolean nested;

		if (pSession->digestVerify) {
			return CPA_FALSE;
		}

		if ((!(useOptimisedContentDesc && useSHRAMConstants)) &&
		    (pSession->qatHashMode == ICP_QAT_HW_AUTH_MODE1)) {
			/* we can only use the SHA1-mode1 in the SHRAM constants
			 * table when
			 * we are using the optimised content desc */
			return CPA_FALSE;
		}

		LacSymQat_HashGetCfgData(pSession->pInstance,
					 pSession->qatHashMode,
					 pSession->hashMode,
					 pSession->hashAlgorithm,
					 &algorithm,
					 &nested);

		/* Check if config data is available in table. */
		LacSymQat_ConstantsGetAuthOffset(pSession->pInstance,
						 algorithm,
						 pSession->qatHashMode,
						 nested,
						 pHashOffset);
		if (*pHashOffset > 0) {
			useSHRAMConstants = CPA_TRUE;
		} else {
			useSHRAMConstants = CPA_FALSE;
		}
	}

	return useSHRAMConstants;
}

CpaBoolean
LacSymQat_UseOptimisedContentDesc(lac_session_desc_t *pSession)
{
	return CPA_FALSE;
}
