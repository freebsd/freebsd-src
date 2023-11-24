/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/**
 *****************************************************************************
 * @file dc_stats.c
 *
 * @ingroup Dc_DataCompression
 *
 * @description
 *      Implementation of the Data Compression stats operations.
 *
 *****************************************************************************/

/*
 *******************************************************************************
 * Include public/global header files
 *******************************************************************************
 */
#include "cpa.h"
#include "cpa_dc.h"
#include "icp_accel_devices.h"
#include "icp_adf_debug.h"
/*
 *******************************************************************************
 * Include private header files
 *******************************************************************************
 */
#include "lac_common.h"
#include "icp_accel_devices.h"
#include "sal_statistics.h"
#include "dc_session.h"
#include "dc_datapath.h"
#include "lac_mem_pools.h"
#include "sal_service_state.h"
#include "sal_types_compression.h"
#include "dc_stats.h"

CpaStatus
dcStatsInit(sal_compression_service_t *pService)
{
	CpaStatus status = CPA_STATUS_SUCCESS;

	pService->pCompStatsArr =
	    LAC_OS_MALLOC(COMPRESSION_NUM_STATS * sizeof(QatUtilsAtomic));

	if (pService->pCompStatsArr == NULL) {
		status = CPA_STATUS_RESOURCE;
	}

	if (CPA_STATUS_SUCCESS == status) {
		COMPRESSION_STATS_RESET(pService);
	}

	return status;
}

void
dcStatsFree(sal_compression_service_t *pService)
{
	if (NULL != pService->pCompStatsArr) {
		LAC_OS_FREE(pService->pCompStatsArr);
	}
}

CpaStatus
cpaDcGetStats(CpaInstanceHandle dcInstance, CpaDcStats *pStatistics)
{
	sal_compression_service_t *pService = NULL;
	CpaInstanceHandle insHandle = NULL;

	if (CPA_INSTANCE_HANDLE_SINGLE == dcInstance) {
		insHandle = dcGetFirstHandle();
	} else {
		insHandle = dcInstance;
	}

	pService = (sal_compression_service_t *)insHandle;

	LAC_CHECK_NULL_PARAM(insHandle);
	LAC_CHECK_NULL_PARAM(pStatistics);
	SAL_RUNNING_CHECK(insHandle);

	SAL_CHECK_INSTANCE_TYPE(insHandle, SAL_SERVICE_TYPE_COMPRESSION);

	/* Retrieves the statistics for compression */
	COMPRESSION_STATS_GET(pStatistics, pService);

	return CPA_STATUS_SUCCESS;
}
