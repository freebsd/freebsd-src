/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */

/**
 ***************************************************************************
 * @file lac_sal_types_crypto.h
 *
 * @ingroup SalCtrl
 *
 * Generic crypto instance type definition
 *
 ***************************************************************************/

#ifndef LAC_SAL_TYPES_CRYPTO_H_
#define LAC_SAL_TYPES_CRYPTO_H_

#include "lac_sym_qat_hash_defs_lookup.h"
#include "lac_sym_key.h"
#include "cpa_cy_sym_dp.h"

#include "icp_adf_debug.h"
#include "lac_sal_types.h"
#include "icp_adf_transport.h"
#include "lac_mem_pools.h"

#define LAC_PKE_FLOW_ID_TAG 0xFFFFFFFC
#define LAC_PKE_ACCEL_ID_BIT_POS 1
#define LAC_PKE_SLICE_ID_BIT_POS 0

/**
 *****************************************************************************
 * @ingroup SalCtrl
 *      Crypto specific Service Container
 *
 * @description
 *      Contains information required per crypto service instance.
 *
 *****************************************************************************/
typedef struct sal_crypto_service_s {
	sal_service_t generic_service_info;
	/**< An instance of the Generic Service Container */

	lac_memory_pool_id_t lac_sym_cookie_pool;
	/**< Memory pool ID used for symmetric operations */
	lac_memory_pool_id_t lac_ec_pool;
	/**< Memory pool ID used for asymmetric operations */
	lac_memory_pool_id_t lac_prime_pool;
	/**< Memory pool ID used for asymmetric operations */
	lac_memory_pool_id_t lac_pke_req_pool;
	/**< Memory pool ID used for asymmetric operations */
	lac_memory_pool_id_t lac_pke_align_pool;
	/**< Memory pool ID used for asymmetric operations */

	QatUtilsAtomic *pLacSymStatsArr;
	/**< pointer to an array of atomic stats for symmetric */

	QatUtilsAtomic *pLacKeyStats;
	/**< pointer to an array of atomic stats for key */

	QatUtilsAtomic *pLacDhStatsArr;
	/**< pointer to an array of atomic stats for DH */

	QatUtilsAtomic *pLacDsaStatsArr;
	/**< pointer to an array of atomic stats for Dsa */

	QatUtilsAtomic *pLacRsaStatsArr;
	/**< pointer to an array of atomic stats for Rsa */

	QatUtilsAtomic *pLacEcStatsArr;
	/**< pointer to an array of atomic stats for Ecc */

	QatUtilsAtomic *pLacEcdhStatsArr;
	/**< pointer to an array of atomic stats for Ecc DH */

	QatUtilsAtomic *pLacEcdsaStatsArr;
	/**< pointer to an array of atomic stats for Ecc DSA */

	QatUtilsAtomic *pLacPrimeStatsArr;
	/**< pointer to an array of atomic stats for prime */

	QatUtilsAtomic *pLacLnStatsArr;
	/**< pointer to an array of atomic stats for large number */

	QatUtilsAtomic *pLacDrbgStatsArr;
	/**< pointer to an array of atomic stats for DRBG */

	Cpa32U pkeFlowId;
	/**< Flow ID for all pke requests from this instance - identifies
	 accelerator
	 and execution engine to use */

	icp_comms_trans_handle trans_handle_sym_tx;
	icp_comms_trans_handle trans_handle_sym_rx;

	icp_comms_trans_handle trans_handle_asym_tx;
	icp_comms_trans_handle trans_handle_asym_rx;

	icp_comms_trans_handle trans_handle_nrbg_tx;
	icp_comms_trans_handle trans_handle_nrbg_rx;

	Cpa32U maxNumSymReqBatch;
	/**< Maximum number of requests that can be placed on the sym tx ring
	      for any one batch request (DP api) */

	Cpa16U acceleratorNum;
	Cpa16U bankNum;
	Cpa16U pkgID;
	Cpa8U isPolled;
	Cpa8U executionEngine;
	Cpa32U coreAffinity;
	Cpa32U nodeAffinity;
	/**< Config Info */

	CpaCySymDpCbFunc pSymDpCb;
	/**< Sym DP Callback */

	lac_sym_qat_hash_defs_t **pLacHashLookupDefs;
	/**< table of pointers to standard defined information for all hash
	     algorithms. We support an extra hash algo that is not exported by
	     cy api which is why we need the extra +1 */
	Cpa8U **ppHmacContentDesc;
	/**< table of pointers to CD for Hmac precomputes - used at session init
	 */

	Cpa8U *pSslLabel;
	/**< pointer to memory holding the standard SSL label ABBCCC.. */

	lac_sym_key_tls_labels_t *pTlsLabel;
	/**< pointer to memory holding the 4 standard TLS labels */

	QatUtilsAtomic drbgErrorState;
	/**< DRBG related variables */

	lac_sym_key_tls_hkdf_sub_labels_t *pTlsHKDFSubLabel;
	/**< pointer to memory holding the 4 HKDFLabels sublabels */

	debug_file_info_t *debug_file;
/**< Statistics handler */
} sal_crypto_service_t;

/*************************************************************************
 * @ingroup cpaCyCommon
 * @description
 *  This function returns a valid asym/sym/crypto instance handle for the
 *  system if it exists. When requesting an instance handle of type sym or
 *  asym, if either is not found then a crypto instance handle is returned
 *  if found, since a crypto handle supports both sym and asym services.
 *  Similarly when requesting a crypto instance handle, if it is not found
 *  then an asym or sym crypto instance handle is returned.
 *
 *  @performance
 *    To avoid calling this function the user of the QA api should not use
 *    instanceHandle = CPA_INSTANCE_HANDLE_SINGLE.
 *
 * @context
 *    This function is called whenever instanceHandle =
 *CPA_INSTANCE_HANDLE_SINGLE
 *    at the QA Cy api.
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]  svc_type        Type of crypto service requested.
 *
 * @retval   Pointer to first crypto instance handle or NULL if no crypto
 *           instances in the system.
 *
 *************************************************************************/

CpaInstanceHandle Lac_GetFirstHandle(sal_service_type_t svc_type);

#endif /*LAC_SAL_TYPES_CRYPTO_H_*/
