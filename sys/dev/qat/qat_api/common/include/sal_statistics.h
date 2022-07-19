/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
/**
 *****************************************************************************
 * @file sal_statistics.h
 *
 * @ingroup SalStats
 *
 * @description
 *     Statistics related defines, structures and functions
 *
 *****************************************************************************/

#ifndef SAL_STATISTICS_H
#define SAL_STATISTICS_H

#include "sal_statistics_strings.h"

#define SAL_STATS_SYM 0
#define SAL_STATS_DSA 1
#define SAL_STATS_DSA2 2
#define SAL_STATS_RSA 3
#define SAL_STATS_DH 4
#define SAL_STATS_KEYGEN 5
#define SAL_STATS_LN 6
#define SAL_STATS_PRIME 7
#define SAL_STATS_ECC 8
#define SAL_STATS_ECDH 9
#define SAL_STATS_ECDSA 10
/**< Numeric values for crypto statistics */

#define SAL_STATISTICS_STRING_OFF "0"
/**< String representing the value for disabled statistics */

/**
*****************************************************************************
 * @ingroup SalStats
 *      Structure describing stats enabled/disabled in the system
 *
 * @description
 *      Structure describing stats enabled/disabled in the system
 *
 *****************************************************************************/
typedef struct sal_statistics_collection_s {
	CpaBoolean bStatsEnabled;
	/**< If CPA_TRUE then statistics functionality is enabled */
	CpaBoolean bDcStatsEnabled;
	/**< If CPA_TRUE then Compression statistics are enabled */
	CpaBoolean bDhStatsEnabled;
	/**< If CPA_TRUE then Diffie-Helman statistics are enabled */
	CpaBoolean bDsaStatsEnabled;
	/**< If CPA_TRUE then DSA statistics are enabled */
	CpaBoolean bEccStatsEnabled;
	/**< If CPA_TRUE then ECC statistics are enabled */
	CpaBoolean bKeyGenStatsEnabled;
	/**< If CPA_TRUE then Key Gen statistics are enabled */
	CpaBoolean bLnStatsEnabled;
	/**< If CPA_TRUE then Large Number statistics are enabled */
	CpaBoolean bPrimeStatsEnabled;
	/**< If CPA_TRUE then Prime statistics are enabled */
	CpaBoolean bRsaStatsEnabled;
	/**< If CPA_TRUE then RSA statistics are enabled */
	CpaBoolean bSymStatsEnabled;
	/**< If CPA_TRUE then Symmetric Crypto statistics are enabled */
} sal_statistics_collection_t;

/**
 ******************************************************************************
 * @ingroup SalStats
 *
 * @description
 *      Initializes structure describing which statistics
 *      are enabled for the acceleration device.
 *
 * @param[in]  device             Pointer to an acceleration device structure
 *
 * @retval  CPA_STATUS_SUCCESS          Operation successful
 * @retval  CPA_STATUS_INVALID_PARAM    Invalid param provided
 * @retval  CPA_STATUS_RESOURCE         Memory alloc failed
 * @retval  CPA_STATUS_FAIL             Operation failed
 *
 ******************************************************************************/
CpaStatus SalStatistics_InitStatisticsCollection(icp_accel_dev_t *device);

/**
 ******************************************************************************
 * @ingroup SalStats
 *
 * @description
 *      Cleans structure describing which statistics
 *      are enabled for the acceleration device.
 *
 * @param[in]  device             Pointer to an acceleration device structure
 *
 * @retval  CPA_STATUS_SUCCESS          Operation successful
 * @retval  CPA_STATUS_INVALID_PARAM    Invalid param provided
 * @retval  CPA_STATUS_FAIL             Operation failed
 *
 ******************************************************************************/
CpaStatus SalStatistics_CleanStatisticsCollection(icp_accel_dev_t *device);
#endif
