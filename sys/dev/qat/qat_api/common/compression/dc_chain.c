/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */

/**
 *****************************************************************************
 * @file dc_chain.c
 *
 * @ingroup Dc_Chaining
 *
 * @description
 *      Implementation of the chaining session operations.
 *
 *****************************************************************************/

/*
 *******************************************************************************
 * Include public/global header files
 *******************************************************************************
 */
#include "cpa.h"

#include "icp_qat_fw.h"
#include "icp_qat_fw_comp.h"
#include "icp_qat_hw.h"

/*
 *******************************************************************************
 * Include private header files
 *******************************************************************************
 */
#include "sal_types_compression.h"
#include "cpa_dc_chain.h"
#include "lac_session.h"
#include "dc_session.h"
#include "dc_datapath.h"
#include "dc_stats.h"
#include "lac_mem_pools.h"
#include "lac_log.h"
#include "sal_types_compression.h"
#include "lac_buffer_desc.h"
#include "sal_service_state.h"
#include "sal_qat_cmn_msg.h"
#include "lac_sym_qat_hash_defs_lookup.h"
#include "sal_string_parse.h"
#include "lac_sym.h"
#include "lac_session.h"
#include "lac_sym_qat.h"
#include "lac_sym_hash.h"
#include "lac_sym_alg_chain.h"
#include "lac_sym_auth_enc.h"

CpaStatus
cpaDcChainGetSessionSize(CpaInstanceHandle dcInstance,
			 CpaDcChainOperations operation,
			 Cpa8U numSessions,
			 CpaDcChainSessionSetupData *pSessionData,
			 Cpa32U *pSessionSize)

{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaDcChainInitSession(CpaInstanceHandle dcInstance,
		      CpaDcSessionHandle pSessionHandle,
		      CpaDcChainOperations operation,
		      Cpa8U numSessions,
		      CpaDcChainSessionSetupData *pSessionData,
		      CpaDcCallbackFn callbackFn)

{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaDcChainRemoveSession(const CpaInstanceHandle dcInstance,
			CpaDcSessionHandle pSessionHandle)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaDcChainResetSession(const CpaInstanceHandle dcInstance,
		       CpaDcSessionHandle pSessionHandle)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaDcChainPerformOp(CpaInstanceHandle dcInstance,
		    CpaDcSessionHandle pSessionHandle,
		    CpaBufferList *pSrcBuff,
		    CpaBufferList *pDestBuff,
		    CpaDcChainOperations operation,
		    Cpa8U numOpDatas,
		    CpaDcChainOpData *pChainOpData,
		    CpaDcChainRqResults *pResults,
		    void *callbackTag)
{
	return CPA_STATUS_UNSUPPORTED;
}
