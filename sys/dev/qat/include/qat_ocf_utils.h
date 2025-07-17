/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#ifndef _QAT_OCF_UTILS_H_
#define _QAT_OCF_UTILS_H_
/* System headers */
#include <sys/types.h>
#include <sys/mbuf.h>
#include <machine/bus_dma.h>

/* Cryptodev headers */
#include <opencrypto/cryptodev.h>
#include <crypto/sha2/sha512.h>

/* QAT specific headers */
#include "qat_ocf_mem_pool.h"
#include "cpa.h"
#include "cpa_cy_sym_dp.h"

static inline CpaBoolean
is_gmac_exception(const struct crypto_session_params *csp)
{
	if (CSP_MODE_DIGEST == csp->csp_mode)
		if (CRYPTO_AES_NIST_GMAC == csp->csp_auth_alg)
			return CPA_TRUE;

	return CPA_FALSE;
}

static inline CpaBoolean
is_sep_aad_supported(const struct crypto_session_params *csp)
{
	if (CPA_TRUE == is_gmac_exception(csp))
		return CPA_FALSE;

	if (CSP_MODE_AEAD == csp->csp_mode)
		if (CRYPTO_AES_NIST_GCM_16 == csp->csp_cipher_alg ||
		    CRYPTO_AES_NIST_GMAC == csp->csp_cipher_alg)
			return CPA_TRUE;

	return CPA_FALSE;
}

static inline CpaBoolean
is_use_sep_digest(const struct crypto_session_params *csp)
{
	/* Use separated digest for all digest/hash operations,
	 * including GMAC. ETA and AEAD use separated digest
	 * due to FW limitation to specify offset to digest
	 * appended to pay-load buffer. */
	if (CSP_MODE_DIGEST == csp->csp_mode || CSP_MODE_ETA == csp->csp_mode ||
	    CSP_MODE_AEAD == csp->csp_mode)
		return CPA_TRUE;

	return CPA_FALSE;
}

int qat_ocf_handle_session_update(struct qat_ocf_dsession *ocf_dsession,
				  struct cryptop *crp);

CpaStatus qat_ocf_wait_for_session(CpaCySymSessionCtx sessionCtx,
				   Cpa32U timeoutMS);

#endif /* _QAT_OCF_UTILS_H_ */
