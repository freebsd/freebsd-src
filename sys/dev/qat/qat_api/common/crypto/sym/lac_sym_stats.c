/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */

/**
 ***************************************************************************
 * @file lac_sym_stats.c   Implementation of symmetric stats
 *
 * @ingroup LacSym
 *
 ***************************************************************************/

/*
*******************************************************************************
* Include public/global header files
*******************************************************************************
*/
#include "cpa.h"
#include "cpa_cy_sym.h"

/*
*******************************************************************************
* Include private header files
*******************************************************************************
*/
#include "lac_mem_pools.h"
#include "icp_adf_transport.h"
#include "icp_accel_devices.h"
#include "icp_adf_debug.h"
#include "icp_qat_fw_la.h"
#include "lac_sym_qat.h"
#include "lac_sym_stats.h"
#include "lac_sal_types_crypto.h"
#include "sal_statistics.h"

/* Number of Symmetric Crypto statistics */
#define LAC_SYM_NUM_STATS (sizeof(CpaCySymStats64) / sizeof(Cpa64U))

CpaStatus
LacSym_StatsInit(CpaInstanceHandle instanceHandle)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	sal_crypto_service_t *pService = (sal_crypto_service_t *)instanceHandle;

	pService->pLacSymStatsArr =
	    LAC_OS_MALLOC(LAC_SYM_NUM_STATS * sizeof(QatUtilsAtomic));

	if (NULL != pService->pLacSymStatsArr) {
		LAC_OS_BZERO((void *)LAC_CONST_VOLATILE_PTR_CAST(
				 pService->pLacSymStatsArr),
			     LAC_SYM_NUM_STATS * sizeof(QatUtilsAtomic));
	} else {
		status = CPA_STATUS_RESOURCE;
	}
	return status;
}

void
LacSym_StatsFree(CpaInstanceHandle instanceHandle)
{
	sal_crypto_service_t *pService = (sal_crypto_service_t *)instanceHandle;
	if (NULL != pService->pLacSymStatsArr) {
		LAC_OS_FREE(pService->pLacSymStatsArr);
	}
}

void
LacSym_StatsInc(Cpa32U offset, CpaInstanceHandle instanceHandle)
{
	sal_crypto_service_t *pService = (sal_crypto_service_t *)instanceHandle;
	if (CPA_TRUE ==
	    pService->generic_service_info.stats->bSymStatsEnabled) {
		qatUtilsAtomicInc(
		    &pService->pLacSymStatsArr[offset / sizeof(Cpa64U)]);
	}
}

void
LacSym_Stats32CopyGet(CpaInstanceHandle instanceHandle,
		      struct _CpaCySymStats *const pSymStats)
{
	int i = 0;
	sal_crypto_service_t *pService = (sal_crypto_service_t *)instanceHandle;

	for (i = 0; i < LAC_SYM_NUM_STATS; i++) {
		((Cpa32U *)pSymStats)[i] =
		    (Cpa32U)qatUtilsAtomicGet(&pService->pLacSymStatsArr[i]);
	}
}

void
LacSym_Stats64CopyGet(CpaInstanceHandle instanceHandle,
		      CpaCySymStats64 *const pSymStats)
{
	int i = 0;
	sal_crypto_service_t *pService = (sal_crypto_service_t *)instanceHandle;

	for (i = 0; i < LAC_SYM_NUM_STATS; i++) {
		((Cpa64U *)pSymStats)[i] =
		    qatUtilsAtomicGet(&pService->pLacSymStatsArr[i]);
	}
}

void
LacSym_StatsShow(CpaInstanceHandle instanceHandle)
{
	CpaCySymStats64 symStats = { 0 };

	LacSym_Stats64CopyGet(instanceHandle, &symStats);

	QAT_UTILS_LOG(SEPARATOR BORDER
		      "              Symmetric Stats               " BORDER
		      "\n" SEPARATOR);

	/* Session Info */
	QAT_UTILS_LOG(BORDER " Sessions Initialized:           %16llu " BORDER
			     "\n" BORDER
			     " Sessions Removed:               %16llu " BORDER
			     "\n" BORDER
			     " Session Errors:                 %16llu " BORDER
			     "\n" SEPARATOR,
		      (unsigned long long)symStats.numSessionsInitialized,
		      (unsigned long long)symStats.numSessionsRemoved,
		      (unsigned long long)symStats.numSessionErrors);

	/* Session info */
	QAT_UTILS_LOG(
	    BORDER " Symmetric Requests:             %16llu " BORDER "\n" BORDER
		   " Symmetric Request Errors:       %16llu " BORDER "\n" BORDER
		   " Symmetric Completed:            %16llu " BORDER "\n" BORDER
		   " Symmetric Completed Errors:     %16llu " BORDER "\n" BORDER
		   " Symmetric Verify Failures:      %16llu " BORDER
		   "\n" SEPARATOR,
	    (unsigned long long)symStats.numSymOpRequests,
	    (unsigned long long)symStats.numSymOpRequestErrors,
	    (unsigned long long)symStats.numSymOpCompleted,
	    (unsigned long long)symStats.numSymOpCompletedErrors,
	    (unsigned long long)symStats.numSymOpVerifyFailures);
}
