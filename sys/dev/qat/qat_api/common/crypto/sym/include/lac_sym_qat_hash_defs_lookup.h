/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */

/**
 *****************************************************************************
 * @file lac_sym_qat_hash_defs_lookup.h
 *
 * @defgroup LacSymQatHashDefsLookup  Hash Defs Lookup
 *
 * @ingroup  LacSymQatHash
 *
 * API to be used for the hash defs lookup table.
 *
 *****************************************************************************/

#ifndef LAC_SYM_QAT_HASH_DEFS_LOOKUP_P_H
#define LAC_SYM_QAT_HASH_DEFS_LOOKUP_P_H

#include "cpa.h"
#include "cpa_cy_sym.h"

/**
******************************************************************************
* @ingroup LacSymQatHashDefsLookup
*      Finishing Hash algorithm
* @description
*      This define points to the last available hash algorithm
* @NOTE: If a new algorithm is added to the api, this #define
* MUST be updated to being the last hash algorithm in the struct
* CpaCySymHashAlgorithm in the file cpa_cy_sym.h
*****************************************************************************/
#define CPA_CY_HASH_ALG_END CPA_CY_SYM_HASH_SM3

/***************************************************************************/

/**
******************************************************************************
* @ingroup LacSymQatHashDefsLookup
*      hash algorithm specific structure
* @description
*      This structure contain constants specific to an algorithm.
*****************************************************************************/
typedef struct lac_sym_qat_hash_alg_info_s {
	Cpa32U digestLength; /**< Digest length in bytes */
	Cpa32U blockLength;  /**< Block length in bytes */
	Cpa8U *initState;    /**< Initialiser state for hash algorithm */
	Cpa32U stateSize;    /**< size of above state in bytes */
} lac_sym_qat_hash_alg_info_t;

/**
******************************************************************************
* @ingroup LacSymQatHashDefsLookup
*      hash qat specific structure
* @description
*      This structure contain constants as defined by the QAT for an
*      algorithm.
*****************************************************************************/
typedef struct lac_sym_qat_hash_qat_info_s {
	Cpa32U algoEnc;      /**< QAT Algorithm encoding */
	Cpa32U authCounter;  /**< Counter value for Auth */
	Cpa32U state1Length; /**< QAT state1 length in bytes */
	Cpa32U state2Length; /**< QAT state2 length in bytes */
} lac_sym_qat_hash_qat_info_t;

/**
******************************************************************************
* @ingroup LacSymQatHashDefsLookup
*      hash defs structure
* @description
*      This type contains pointers to the hash algorithm structure and
*      to the hash qat specific structure
*****************************************************************************/
typedef struct lac_sym_qat_hash_defs_s {
	lac_sym_qat_hash_alg_info_t *algInfo;
	/**< pointer to hash info structure */
	lac_sym_qat_hash_qat_info_t *qatInfo;
	/**< pointer to hash QAT info structure */
} lac_sym_qat_hash_defs_t;

/**
*******************************************************************************
* @ingroup LacSymQatHashDefsLookup
*      initialise the hash lookup table
*
* @description
*      This function initialises the digest lookup table.
*
* @note
*      This function does not have a corresponding shutdown function.
*
* @return CPA_STATUS_SUCCESS   Operation successful
* @return CPA_STATUS_RESOURCE  Allocating of hash lookup table failed
*
*****************************************************************************/
CpaStatus LacSymQat_HashLookupInit(CpaInstanceHandle instanceHandle);

/**
*******************************************************************************
* @ingroup LacSymQatHashDefsLookup
*      get hash algorithm specific structure from lookup table
*
* @description
*      This function looks up the hash lookup array for a structure
*      containing data specific to a hash algorithm. The hashAlgorithm enum
*      value MUST be in the correct range prior to calling this function.
*
* @param[in]  hashAlgorithm     Hash Algorithm
* @param[out] ppHashAlgInfo     Hash Alg Info structure
*
* @return None
*
*****************************************************************************/
void LacSymQat_HashAlgLookupGet(CpaInstanceHandle instanceHandle,
				CpaCySymHashAlgorithm hashAlgorithm,
				lac_sym_qat_hash_alg_info_t **ppHashAlgInfo);

/**
*******************************************************************************
* @ingroup LacSymQatHashDefsLookup
*      get hash defintions from lookup table.
*
* @description
*      This function looks up the hash lookup array for a structure
*      containing data specific to a hash algorithm. This includes both
*      algorithm specific info and qat specific infro. The hashAlgorithm enum
*      value MUST be in the correct range prior to calling this function.
*
* @param[in]  hashAlgorithm     Hash Algorithm
* @param[out] ppHashDefsInfo    Hash Defs structure
*
* @return void
*
*****************************************************************************/
void LacSymQat_HashDefsLookupGet(CpaInstanceHandle instanceHandle,
				 CpaCySymHashAlgorithm hashAlgorithm,
				 lac_sym_qat_hash_defs_t **ppHashDefsInfo);

#endif /* LAC_SYM_QAT_HASH_DEFS_LOOKUP_P_H */
