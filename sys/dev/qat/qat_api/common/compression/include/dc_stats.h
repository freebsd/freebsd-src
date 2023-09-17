/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/**
 *****************************************************************************
 * @file dc_stats.h
 *
 * @ingroup Dc_DataCompression
 *
 * @description
 *      Definition of the Data Compression stats parameters.
 *
 *****************************************************************************/
#ifndef DC_STATS_H_
#define DC_STATS_H_

/* Number of Compression statistics */
#define COMPRESSION_NUM_STATS (sizeof(CpaDcStats) / sizeof(Cpa64U))

#define COMPRESSION_STAT_INC(statistic, pService)                              \
	do {                                                                   \
		if (CPA_TRUE ==                                                \
		    pService->generic_service_info.stats->bDcStatsEnabled) {   \
			qatUtilsAtomicInc(                                     \
			    &pService->pCompStatsArr[offsetof(CpaDcStats,      \
							      statistic) /     \
						     sizeof(Cpa64U)]);         \
		}                                                              \
	} while (0)

/* Macro to get all Compression stats (from internal array of atomics) */
#define COMPRESSION_STATS_GET(compStats, pService)                             \
	do {                                                                   \
		int i;                                                         \
		for (i = 0; i < COMPRESSION_NUM_STATS; i++) {                  \
			((Cpa64U *)compStats)[i] =                             \
			    qatUtilsAtomicGet(&pService->pCompStatsArr[i]);    \
		}                                                              \
	} while (0)

/* Macro to reset all Compression stats */
#define COMPRESSION_STATS_RESET(pService)                                      \
	do {                                                                   \
		int i;                                                         \
		for (i = 0; i < COMPRESSION_NUM_STATS; i++) {                  \
			qatUtilsAtomicSet(0, &pService->pCompStatsArr[i]);     \
		}                                                              \
	} while (0)

/**
*******************************************************************************
* @ingroup Dc_DataCompression
*      Initialises the compression stats
*
* @description
*      This function allocates and initialises the stats array to 0
*
* @param[in] pService          Pointer to a compression service structure
*
* @retval CPA_STATUS_SUCCESS   initialisation successful
* @retval CPA_STATUS_RESOURCE  array allocation failed
*
*****************************************************************************/
CpaStatus dcStatsInit(sal_compression_service_t *pService);

/**
*******************************************************************************
* @ingroup Dc_DataCompression
*      Frees the compression stats
*
* @description
*      This function frees the stats array
*
* @param[in] pService          Pointer to a compression service structure
*
* @retval None
*
*****************************************************************************/
void dcStatsFree(sal_compression_service_t *pService);

#endif /* DC_STATS_H_ */
