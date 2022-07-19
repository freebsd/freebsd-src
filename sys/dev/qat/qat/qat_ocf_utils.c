/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
/* System headers */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/mutex.h>
#include <sys/timespec.h>

/* QAT specific headers */
#include "qat_ocf_utils.h"
#include "cpa.h"
#include "lac_common.h"
#include "lac_log.h"
#include "lac_mem.h"
#include "lac_mem_pools.h"
#include "lac_list.h"
#include "lac_sym.h"
#include "lac_sym_qat.h"
#include "lac_sal.h"
#include "lac_sal_ctrl.h"
#include "lac_session.h"
#include "lac_sym_cipher.h"
#include "lac_sym_hash.h"
#include "lac_sym_alg_chain.h"
#include "lac_sym_stats.h"
#include "lac_sym_partial.h"
#include "lac_sym_qat_hash_defs_lookup.h"

#define QAT_OCF_AAD_NOCHANGE (-1)

CpaStatus
qat_ocf_wait_for_session(CpaCySymSessionCtx sessionCtx, Cpa32U timeoutMS)
{
	CpaBoolean sessionInUse = CPA_TRUE;
	CpaStatus status;
	struct timespec start_ts;
	struct timespec current_ts;
	struct timespec delta;
	u64 delta_ms;

	nanotime(&start_ts);
	for (;;) {
		status = cpaCySymSessionInUse(sessionCtx, &sessionInUse);
		if (CPA_STATUS_SUCCESS != status)
			return CPA_STATUS_FAIL;
		if (CPA_FALSE == sessionInUse)
			break;
		nanotime(&current_ts);
		delta = timespec_sub(current_ts, start_ts);
		delta_ms = (delta.tv_sec * 1000) +
			   (delta.tv_nsec / NSEC_PER_MSEC);
		if (delta_ms > (timeoutMS))
			return CPA_STATUS_RESOURCE;
		qatUtilsYield();
	}

	return CPA_STATUS_SUCCESS;
}

static CpaStatus
qat_ocf_session_update(struct qat_ocf_session *ocf_session,
		       Cpa8U *newCipher,
		       Cpa8U *newAuth,
		       Cpa32U newAADLength)
{
	lac_session_desc_t *pSessionDesc = NULL;
	CpaStatus status = CPA_STATUS_SUCCESS;
	CpaBoolean sessionInUse = CPA_TRUE;

	if (!ocf_session->sessionCtx)
		return CPA_STATUS_SUCCESS;

	status = cpaCySymSessionInUse(ocf_session->sessionCtx, &sessionInUse);
	if (CPA_TRUE == sessionInUse)
		return CPA_STATUS_RESOURCE;

	pSessionDesc =
	    LAC_SYM_SESSION_DESC_FROM_CTX_GET(ocf_session->sessionCtx);

	if (newAADLength != QAT_OCF_AAD_NOCHANGE) {
		ocf_session->aadLen = newAADLength;
		status =
		    LacAlgChain_SessionAADUpdate(pSessionDesc, newAADLength);
		if (CPA_STATUS_SUCCESS != status)
			return status;
	}

	if (newCipher) {
		status =
		    LacAlgChain_SessionCipherKeyUpdate(pSessionDesc, newCipher);
		if (CPA_STATUS_SUCCESS != status)
			return status;
	}

	if (newAuth) {
		status =
		    LacAlgChain_SessionAuthKeyUpdate(pSessionDesc, newAuth);
		if (CPA_STATUS_SUCCESS != status)
			return status;
	}

	return status;
}

CpaStatus
qat_ocf_handle_session_update(struct qat_ocf_dsession *ocf_dsession,
			      struct cryptop *crp)
{
	Cpa32U newAADLength = QAT_OCF_AAD_NOCHANGE;
	Cpa8U *cipherKey;
	Cpa8U *authKey;
	crypto_session_t cses;
	const struct crypto_session_params *csp;
	CpaStatus status = CPA_STATUS_SUCCESS;

	if (!ocf_dsession)
		return CPA_STATUS_FAIL;

	cses = crp->crp_session;
	if (!cses)
		return CPA_STATUS_FAIL;
	csp = crypto_get_params(cses);
	if (!csp)
		return CPA_STATUS_FAIL;

	cipherKey = crp->crp_cipher_key;
	authKey = crp->crp_auth_key;

	if (is_sep_aad_supported(csp)) {
		/* Determine if AAD has change */
		if ((ocf_dsession->encSession.sessionCtx &&
		     ocf_dsession->encSession.aadLen != crp->crp_aad_length) ||
		    (ocf_dsession->decSession.sessionCtx &&
		     ocf_dsession->decSession.aadLen != crp->crp_aad_length)) {
			newAADLength = crp->crp_aad_length;

			/* Get auth and cipher keys from session if not present
			 * in the request. Update keys is required to update
			 * AAD.
			 */
			if (!authKey)
				authKey = csp->csp_auth_key;
			if (!cipherKey)
				cipherKey = csp->csp_cipher_key;
		}
		if (!authKey)
			authKey = cipherKey;
	}

	if (crp->crp_cipher_key || crp->crp_auth_key ||
	    newAADLength != QAT_OCF_AAD_NOCHANGE) {
		/* Update encryption session */
		status = qat_ocf_session_update(&ocf_dsession->encSession,
						cipherKey,
						authKey,
						newAADLength);
		if (CPA_STATUS_SUCCESS != status)
			return status;
		/* Update decryption session */
		status = qat_ocf_session_update(&ocf_dsession->decSession,
						cipherKey,
						authKey,
						newAADLength);
		if (CPA_STATUS_SUCCESS != status)
			return status;
	}

	return status;
}
