/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */

/**
 *****************************************************************************
 * @file lac_sym_qat_key.c Interfaces for populating the symmetric qat key
 *  structures
 *
 * @ingroup LacSymQatKey
 *
 *****************************************************************************/

#include "cpa.h"
#include "cpa_cy_key.h"
#include "lac_mem.h"
#include "icp_qat_fw_la.h"
#include "icp_accel_devices.h"
#include "icp_adf_debug.h"
#include "lac_list.h"
#include "lac_sal_types.h"
#include "lac_sym_qat_key.h"
#include "lac_sym_hash_defs.h"

void
LacSymQat_KeySslRequestPopulate(icp_qat_la_bulk_req_hdr_t *pKeyGenReqHdr,
				icp_qat_fw_la_key_gen_common_t *pKeyGenReqMid,
				Cpa32U generatedKeyLenInBytes,
				Cpa32U labelLenInBytes,
				Cpa32U secretLenInBytes,
				Cpa32U iterations)
{
	/* Rounded to nearest 8 byte boundary */
	Cpa8U outLenRounded = 0;
	outLenRounded = LAC_ALIGN_POW2_ROUNDUP(generatedKeyLenInBytes,
					       LAC_QUAD_WORD_IN_BYTES);

	pKeyGenReqMid->u.secret_lgth_ssl = secretLenInBytes;
	pKeyGenReqMid->u1.s1.output_lgth_ssl = outLenRounded;
	pKeyGenReqMid->u1.s1.label_lgth_ssl = labelLenInBytes;
	pKeyGenReqMid->u2.iter_count = iterations;
	pKeyGenReqMid->u3.resrvd2 = 0;
	pKeyGenReqMid->resrvd3 = 0;

	/* Set up the common LA flags */
	pKeyGenReqHdr->comn_hdr.service_cmd_id =
	    ICP_QAT_FW_LA_CMD_SSL3_KEY_DERIVE;
	pKeyGenReqHdr->comn_hdr.resrvd1 = 0;
}

void
LacSymQat_KeyTlsRequestPopulate(
    icp_qat_fw_la_key_gen_common_t *pKeyGenReqParams,
    Cpa32U generatedKeyLenInBytes,
    Cpa32U labelInfo, /* Generic name, can be num of labels or label length */
    Cpa32U secretLenInBytes,
    Cpa8U seedLenInBytes,
    icp_qat_fw_la_cmd_id_t cmdId)
{
	pKeyGenReqParams->u1.s3.output_lgth_tls =
	    LAC_ALIGN_POW2_ROUNDUP(generatedKeyLenInBytes,
				   LAC_QUAD_WORD_IN_BYTES);

	/* For TLS u param of auth_req_params is set to secretLen */
	pKeyGenReqParams->u.secret_lgth_tls = secretLenInBytes;

	switch (cmdId) {
	case ICP_QAT_FW_LA_CMD_HKDF_EXTRACT:
		pKeyGenReqParams->u2.hkdf_ikm_length = secretLenInBytes;
		pKeyGenReqParams->u3.resrvd2 = 0;
		break;
	case ICP_QAT_FW_LA_CMD_HKDF_EXPAND:
		pKeyGenReqParams->u1.hkdf.info_length = labelInfo;
		break;
	case ICP_QAT_FW_LA_CMD_HKDF_EXTRACT_AND_EXPAND:
		pKeyGenReqParams->u2.hkdf_ikm_length = secretLenInBytes;
		pKeyGenReqParams->u1.hkdf.info_length = labelInfo;
		break;
	case ICP_QAT_FW_LA_CMD_HKDF_EXPAND_LABEL:
		/* Num of Labels */
		pKeyGenReqParams->u1.hkdf_label.num_labels = labelInfo;
		pKeyGenReqParams->u3.hkdf_num_sublabels = 4; /* 4 subLabels */
		break;
	case ICP_QAT_FW_LA_CMD_HKDF_EXTRACT_AND_EXPAND_LABEL:
		pKeyGenReqParams->u2.hkdf_ikm_length = secretLenInBytes;
		/* Num of Labels */
		pKeyGenReqParams->u1.hkdf_label.num_labels = labelInfo;
		pKeyGenReqParams->u3.hkdf_num_sublabels = 4; /* 4 subLabels */
		break;
	default:
		pKeyGenReqParams->u1.s3.label_lgth_tls = labelInfo;
		pKeyGenReqParams->u2.tls_seed_length = seedLenInBytes;
		pKeyGenReqParams->u3.resrvd2 = 0;
		break;
	}
	pKeyGenReqParams->resrvd3 = 0;
}

void
LacSymQat_KeyMgfRequestPopulate(icp_qat_la_bulk_req_hdr_t *pKeyGenReqHdr,
				icp_qat_fw_la_key_gen_common_t *pKeyGenReqMid,
				Cpa8U seedLenInBytes,
				Cpa16U maskLenInBytes,
				Cpa8U hashLenInBytes)
{
	pKeyGenReqHdr->comn_hdr.service_cmd_id = ICP_QAT_FW_LA_CMD_MGF1;
	pKeyGenReqMid->u.mask_length =
	    LAC_ALIGN_POW2_ROUNDUP(maskLenInBytes, LAC_QUAD_WORD_IN_BYTES);

	pKeyGenReqMid->u1.s2.hash_length = hashLenInBytes;
	pKeyGenReqMid->u1.s2.seed_length = seedLenInBytes;
}

void
LacSymQat_KeySslKeyMaterialInputPopulate(
    sal_service_t *pService,
    icp_qat_fw_la_ssl_key_material_input_t *pSslKeyMaterialInput,
    void *pSeed,
    Cpa64U labelPhysAddr,
    void *pSecret)
{
	LAC_MEM_SHARED_WRITE_VIRT_TO_PHYS_PTR_EXTERNAL(
	    (*pService), pSslKeyMaterialInput->seed_addr, pSeed);

	pSslKeyMaterialInput->label_addr = labelPhysAddr;

	LAC_MEM_SHARED_WRITE_VIRT_TO_PHYS_PTR_EXTERNAL(
	    (*pService), pSslKeyMaterialInput->secret_addr, pSecret);
}

void
LacSymQat_KeyTlsKeyMaterialInputPopulate(
    sal_service_t *pService,
    icp_qat_fw_la_tls_key_material_input_t *pTlsKeyMaterialInput,
    void *pSeed,
    Cpa64U labelPhysAddr)
{
	LAC_MEM_SHARED_WRITE_VIRT_TO_PHYS_PTR_EXTERNAL(
	    (*pService), pTlsKeyMaterialInput->seed_addr, pSeed);

	pTlsKeyMaterialInput->label_addr = labelPhysAddr;
}

void
LacSymQat_KeyTlsHKDFKeyMaterialInputPopulate(
    sal_service_t *pService,
    icp_qat_fw_la_hkdf_key_material_input_t *pTlsKeyMaterialInput,
    CpaCyKeyGenHKDFOpData *pKeyGenTlsOpData,
    Cpa64U subLabelsPhysAddr,
    icp_qat_fw_la_cmd_id_t cmdId)
{
	switch (cmdId) {
	case ICP_QAT_FW_LA_CMD_HKDF_EXTRACT:
		LAC_MEM_SHARED_WRITE_VIRT_TO_PHYS_PTR_EXTERNAL(
		    (*pService),
		    pTlsKeyMaterialInput->ikm_addr,
		    pKeyGenTlsOpData->secret);
		break;
	case ICP_QAT_FW_LA_CMD_HKDF_EXPAND:
		LAC_MEM_SHARED_WRITE_VIRT_TO_PHYS_PTR_EXTERNAL(
		    (*pService),
		    pTlsKeyMaterialInput->labels_addr,
		    pKeyGenTlsOpData->info);
		break;
	case ICP_QAT_FW_LA_CMD_HKDF_EXTRACT_AND_EXPAND:
		LAC_MEM_SHARED_WRITE_VIRT_TO_PHYS_PTR_EXTERNAL(
		    (*pService),
		    pTlsKeyMaterialInput->ikm_addr,
		    pKeyGenTlsOpData->secret);
		pTlsKeyMaterialInput->labels_addr =
		    pTlsKeyMaterialInput->ikm_addr +
		    ((uint64_t)&pKeyGenTlsOpData->info -
		     (uint64_t)&pKeyGenTlsOpData->secret);
		break;
	case ICP_QAT_FW_LA_CMD_HKDF_EXPAND_LABEL:
		pTlsKeyMaterialInput->sublabels_addr = subLabelsPhysAddr;
		LAC_MEM_SHARED_WRITE_VIRT_TO_PHYS_PTR_EXTERNAL(
		    (*pService),
		    pTlsKeyMaterialInput->labels_addr,
		    pKeyGenTlsOpData->label);
		break;
	case ICP_QAT_FW_LA_CMD_HKDF_EXTRACT_AND_EXPAND_LABEL:
		pTlsKeyMaterialInput->sublabels_addr = subLabelsPhysAddr;
		LAC_MEM_SHARED_WRITE_VIRT_TO_PHYS_PTR_EXTERNAL(
		    (*pService),
		    pTlsKeyMaterialInput->ikm_addr,
		    pKeyGenTlsOpData->secret);
		pTlsKeyMaterialInput->labels_addr =
		    pTlsKeyMaterialInput->ikm_addr +
		    ((uint64_t)&pKeyGenTlsOpData->label -
		     (uint64_t)&pKeyGenTlsOpData->secret);
		break;
	default:
		break;
	}
}
