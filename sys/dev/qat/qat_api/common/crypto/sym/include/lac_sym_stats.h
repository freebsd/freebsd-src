/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */

/**
 ***************************************************************************
 * @file lac_sym_stats.h
 *
 * @defgroup LacSymCommon Symmetric Common
 *
 * @ingroup LacSym
 *
 * Symmetric Common consists of common statistics, buffer and partial packet
 * functionality.
 *
 ***************************************************************************/

/**
 ***************************************************************************
 * @defgroup LacSymStats Statistics
 *
 * @ingroup LacSymCommon
 *
 * definitions and prototypes for LAC symmetric statistics.
 *
 * @lld_start
 *      In the LAC API the stats fields are defined as Cpa32U but
 *      QatUtilsAtomic is the type that the atomic API supports. Therefore we
 *      need to define a structure internally with the same fields as the API
 *      stats structure, but each field must be of type QatUtilsAtomic.
 *
 *      - <b>Incrementing Statistics:</b>\n
 *      Atomically increment the statistic on the internal stats structure.
 *
 *      - <b>Providing a copy of the stats back to the user:</b>\n
 *      Use atomicGet to read the atomic variable for each stat field in the
 *      local internal stat structure. These values are saved in structure
 *      (as defined by the LAC API) that the client will provide a pointer
 *      to as a parameter.
 *
 *      - <b>Stats Show:</b>\n
 *      Use atomicGet to read the atomic variables for each field in the local
 *      internal stat structure and print to the screen
 *
 *      - <b>Stats Array:</b>\n
 *      A macro is used to get the offset off the stat in the structure. This
 *      offset is passed to a function which uses it to increment the stat
 *      at that offset.
 *
 * @lld_end
 *
 ***************************************************************************/

/***************************************************************************/

#ifndef LAC_SYM_STATS_H
#define LAC_SYM_STATS_H

/*
******************************************************************************
* Include public/global header files
******************************************************************************
*/

#include "cpa.h"
#include "cpa_cy_sym.h"
#include "cpa_cy_common.h"

/*
*******************************************************************************
* Include private header files
*******************************************************************************
*/

/**
*******************************************************************************
* @ingroup LacSymStats
*      increment a symmetric statistic
*
* @description
*      Increment the statistics
*
* @param statistic  IN The field in the symmetric statistics structure to be
*                      incremented
* @param instanceHandle  IN engine Id Number
*
* @retval None
*
*****************************************************************************/
#define LAC_SYM_STAT_INC(statistic, instanceHandle)                            \
	LacSym_StatsInc(offsetof(CpaCySymStats64, statistic), instanceHandle)

/**
*******************************************************************************
* @ingroup LacSymStats
*      initialises the symmetric stats
*
* @description
*      This function allocates and initialises the stats array to 0
*
* @param instanceHandle    Instance Handle
*
* @retval CPA_STATUS_SUCCESS   initialisation successful
* @retval CPA_STATUS_RESOURCE  array allocation failed
*
*****************************************************************************/
CpaStatus LacSym_StatsInit(CpaInstanceHandle instanceHandle);

/**
*******************************************************************************
* @ingroup LacSymStats
*      Frees the symmetric stats
*
* @description
*      This function frees the stats array
*
* @param instanceHandle    Instance Handle
*
* @retval None
*
*****************************************************************************/
void LacSym_StatsFree(CpaInstanceHandle instanceHandle);

/**
*******************************************************************************
* @ingroup LacSymStats
*      Increment a stat
*
* @description
*      This function incrementes a stat for a specific engine.
*
* @param offset     IN  offset of stat field in structure
* @param instanceHandle  IN  qat Handle
*
* @retval None
*
*****************************************************************************/
void LacSym_StatsInc(Cpa32U offset, CpaInstanceHandle instanceHandle);

/**
*******************************************************************************
* @ingroup LacSymStats
*      Copy the contents of the statistics structure for an engine
*
* @description
*      This function copies the 32bit symmetric statistics structure for
*      a specific engine into an address supplied as a parameter.
*
* @param instanceHandle  IN     engine Id Number
* @param pSymStats  OUT stats structure to copy the stats for the into
*
* @retval None
*
*****************************************************************************/
void LacSym_Stats32CopyGet(CpaInstanceHandle instanceHandle,
			   struct _CpaCySymStats *const pSymStats);

/**
*******************************************************************************
* @ingroup LacSymStats
*      Copy the contents of the statistics structure for an engine
*
* @description
*      This function copies the 64bit symmetric statistics structure for
*      a specific engine into an address supplied as a parameter.
*
* @param instanceHandle  IN     engine Id Number
* @param pSymStats  OUT stats structure to copy the stats for the into
*
* @retval None
*
*****************************************************************************/
void LacSym_Stats64CopyGet(CpaInstanceHandle instanceHandle,
			   CpaCySymStats64 *const pSymStats);

/**
*******************************************************************************
* @ingroup LacSymStats
*      print the symmetric stats to standard output
*
* @description
*      The statistics for symmetric are printed to standard output.
*
* @retval None
*
* @see LacSym_StatsCopyGet()
*
*****************************************************************************/
void LacSym_StatsShow(CpaInstanceHandle instanceHandle);

#endif /*LAC_SYM_STATS_H_*/
