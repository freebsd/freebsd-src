/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
/**
 *****************************************************************************
 * @file sal_statistics.c
 *
 * @defgroup SalStats  Sal Statistics
 *
 * @ingroup SalStats
 *
 * @description
 *    This file contains implementation of statistic related functions
 *
 *****************************************************************************/

#include "cpa.h"
#include "lac_common.h"
#include "lac_mem.h"
#include "icp_adf_cfg.h"
#include "icp_accel_devices.h"
#include "sal_statistics.h"

#include "icp_adf_debug.h"
#include "lac_sal_types.h"
#include "lac_sal.h"

/**
 ******************************************************************************
 * @ingroup SalStats
 *      Reads from the config file if the given statistic is enabled
 *
 * @description
 *      Reads from the config file if the given statistic is enabled
 *
 * @param[in]  device           Pointer to an acceleration device structure
 * @param[in]  statsName        Name of the config value to read the value from
 * @param[out] pIsEnabled       Pointer to a variable where information if the
 *                              given stat is enabled or disabled will be stored
 *
 * @retval  CPA_STATUS_SUCCESS          Operation successful
 * @retval  CPA_STATUS_INVALID_PARAM    Invalid param provided
 * @retval  CPA_STATUS_FAIL             Operation failed
 *
 ******************************************************************************/
static CpaStatus
SalStatistics_GetStatEnabled(icp_accel_dev_t *device,
			     const char *statsName,
			     CpaBoolean *pIsEnabled)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	char param_value[ADF_CFG_MAX_VAL_LEN_IN_BYTES] = { 0 };

	LAC_CHECK_NULL_PARAM(pIsEnabled);
	LAC_CHECK_NULL_PARAM(statsName);

	status = icp_adf_cfgGetParamValue(device,
					  LAC_CFG_SECTION_GENERAL,
					  statsName,
					  param_value);

	if (CPA_STATUS_SUCCESS != status) {
		QAT_UTILS_LOG("Failed to get %s from configuration.\n",
			      statsName);
		return status;
	}

	if (0 == strncmp(param_value,
			 SAL_STATISTICS_STRING_OFF,
			 strlen(SAL_STATISTICS_STRING_OFF))) {
		*pIsEnabled = CPA_FALSE;
	} else {
		*pIsEnabled = CPA_TRUE;
	}

	return status;
}

/* @ingroup SalStats */
CpaStatus
SalStatistics_InitStatisticsCollection(icp_accel_dev_t *device)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	sal_statistics_collection_t *pStatsCollection = NULL;
	Cpa32U enabled_services = 0;

	LAC_CHECK_NULL_PARAM(device);

	pStatsCollection = LAC_OS_MALLOC(sizeof(sal_statistics_collection_t));
	if (NULL == pStatsCollection) {
		QAT_UTILS_LOG("Failed to allocate memory for statistic.\n");
		return CPA_STATUS_RESOURCE;
	}
	device->pQatStats = pStatsCollection;

	status = SalStatistics_GetStatEnabled(device,
					      SAL_STATS_CFG_ENABLED,
					      &pStatsCollection->bStatsEnabled);
	LAC_CHECK_STATUS(status);

	if (CPA_FALSE == pStatsCollection->bStatsEnabled) {
		pStatsCollection->bDcStatsEnabled = CPA_FALSE;
		pStatsCollection->bDhStatsEnabled = CPA_FALSE;
		pStatsCollection->bDsaStatsEnabled = CPA_FALSE;
		pStatsCollection->bEccStatsEnabled = CPA_FALSE;
		pStatsCollection->bKeyGenStatsEnabled = CPA_FALSE;
		pStatsCollection->bLnStatsEnabled = CPA_FALSE;
		pStatsCollection->bPrimeStatsEnabled = CPA_FALSE;
		pStatsCollection->bRsaStatsEnabled = CPA_FALSE;
		pStatsCollection->bSymStatsEnabled = CPA_FALSE;

		return status;
	}

	/* What services are enabled */
	status = SalCtrl_GetEnabledServices(device, &enabled_services);
	if (CPA_STATUS_SUCCESS != status) {
		QAT_UTILS_LOG("Failed to get enabled services.\n");
		return CPA_STATUS_FAIL;
	}

	/* Check if the compression service is enabled */
	if (SalCtrl_IsServiceEnabled(enabled_services,
				     SAL_SERVICE_TYPE_COMPRESSION)) {
		status = SalStatistics_GetStatEnabled(
		    device,
		    SAL_STATS_CFG_DC,
		    &pStatsCollection->bDcStatsEnabled);
		LAC_CHECK_STATUS(status);
	}
	/* Check if the asym service is enabled */
	if (SalCtrl_IsServiceEnabled(enabled_services,
				     SAL_SERVICE_TYPE_CRYPTO_ASYM) ||
	    SalCtrl_IsServiceEnabled(enabled_services,
				     SAL_SERVICE_TYPE_CRYPTO)) {
		status = SalStatistics_GetStatEnabled(
		    device,
		    SAL_STATS_CFG_DH,
		    &pStatsCollection->bDhStatsEnabled);
		LAC_CHECK_STATUS(status);

		status = SalStatistics_GetStatEnabled(
		    device,
		    SAL_STATS_CFG_DSA,
		    &pStatsCollection->bDsaStatsEnabled);
		LAC_CHECK_STATUS(status);

		status = SalStatistics_GetStatEnabled(
		    device,
		    SAL_STATS_CFG_ECC,
		    &pStatsCollection->bEccStatsEnabled);
		LAC_CHECK_STATUS(status);

		status = SalStatistics_GetStatEnabled(
		    device,
		    SAL_STATS_CFG_KEYGEN,
		    &pStatsCollection->bKeyGenStatsEnabled);
		LAC_CHECK_STATUS(status);

		status = SalStatistics_GetStatEnabled(
		    device,
		    SAL_STATS_CFG_LN,
		    &pStatsCollection->bLnStatsEnabled);
		LAC_CHECK_STATUS(status);

		status = SalStatistics_GetStatEnabled(
		    device,
		    SAL_STATS_CFG_PRIME,
		    &pStatsCollection->bPrimeStatsEnabled);
		LAC_CHECK_STATUS(status);

		status = SalStatistics_GetStatEnabled(
		    device,
		    SAL_STATS_CFG_RSA,
		    &pStatsCollection->bRsaStatsEnabled);
		LAC_CHECK_STATUS(status);
	}

	/* Check if the sym service is enabled */
	if (SalCtrl_IsServiceEnabled(enabled_services,
				     SAL_SERVICE_TYPE_CRYPTO_SYM) ||
	    SalCtrl_IsServiceEnabled(enabled_services,
				     SAL_SERVICE_TYPE_CRYPTO)) {
		status = SalStatistics_GetStatEnabled(
		    device,
		    SAL_STATS_CFG_SYM,
		    &pStatsCollection->bSymStatsEnabled);
		LAC_CHECK_STATUS(status);
	}
	return status;
};

/* @ingroup SalStats */
CpaStatus
SalStatistics_CleanStatisticsCollection(icp_accel_dev_t *device)
{
	sal_statistics_collection_t *pStatsCollection = NULL;
	LAC_CHECK_NULL_PARAM(device);
	pStatsCollection = (sal_statistics_collection_t *)device->pQatStats;
	LAC_OS_FREE(pStatsCollection);
	device->pQatStats = NULL;
	return CPA_STATUS_SUCCESS;
}
