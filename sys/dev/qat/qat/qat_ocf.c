/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
/* System headers */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>

/* Cryptodev headers */
#include <opencrypto/cryptodev.h>
#include "cryptodev_if.h"

/* QAT specific headers */
#include "cpa.h"
#include "cpa_cy_im.h"
#include "cpa_cy_sym_dp.h"
#include "adf_accel_devices.h"
#include "adf_common_drv.h"
#include "lac_sym_hash_defs.h"
#include "lac_sym_qat_hash_defs_lookup.h"

/* To get only IRQ instances */
#include "icp_accel_devices.h"
#include "icp_adf_accel_mgr.h"
#include "lac_sal_types.h"

/* To disable AEAD HW MAC verification */
#include "icp_sal_user.h"

/* QAT OCF specific headers */
#include "qat_ocf_mem_pool.h"
#include "qat_ocf_utils.h"

#define QAT_OCF_MAX_INSTANCES (256)
#define QAT_OCF_SESSION_WAIT_TIMEOUT_MS (1000)

MALLOC_DEFINE(M_QAT_OCF, "qat_ocf", "qat_ocf(4) memory allocations");

/* QAT OCF internal structures */
struct qat_ocf_softc {
	device_t sc_dev;
	struct sysctl_oid *rc;
	uint32_t enabled;
	int32_t cryptodev_id;
	struct qat_ocf_instance cyInstHandles[QAT_OCF_MAX_INSTANCES];
	int32_t numCyInstances;
};

/* Function definitions */
static void qat_ocf_freesession(device_t dev, crypto_session_t cses);
static int qat_ocf_probesession(device_t dev,
				const struct crypto_session_params *csp);
static int qat_ocf_newsession(device_t dev,
			      crypto_session_t cses,
			      const struct crypto_session_params *csp);
static int qat_ocf_attach(device_t dev);
static int qat_ocf_detach(device_t dev);

static void
symDpCallback(CpaCySymDpOpData *pOpData,
	      CpaStatus result,
	      CpaBoolean verifyResult)
{
	struct qat_ocf_cookie *qat_cookie;
	struct cryptop *crp;
	struct qat_ocf_dsession *qat_dsession = NULL;
	struct qat_ocf_session *qat_session = NULL;
	struct qat_ocf_instance *qat_instance = NULL;
	CpaStatus status;
	int rc = 0;

	qat_cookie = (struct qat_ocf_cookie *)pOpData->pCallbackTag;
	if (!qat_cookie)
		return;

	crp = qat_cookie->crp_op;

	qat_dsession = crypto_get_driver_session(crp->crp_session);
	qat_instance = qat_dsession->qatInstance;

	status = qat_ocf_cookie_dma_post_sync(crp, pOpData);
	if (CPA_STATUS_SUCCESS != status) {
		rc = EIO;
		goto exit;
	}

	status = qat_ocf_cookie_dma_unload(crp, pOpData);
	if (CPA_STATUS_SUCCESS != status) {
		rc = EIO;
		goto exit;
	}

	/* Verify result */
	if (CPA_STATUS_SUCCESS != result) {
		rc = EBADMSG;
		goto exit;
	}

	/* Verify digest by FW (GCM and CCM only) */
	if (CPA_TRUE != verifyResult) {
		rc = EBADMSG;
		goto exit;
	}

	if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op))
		qat_session = &qat_dsession->encSession;
	else
		qat_session = &qat_dsession->decSession;

	/* Copy back digest result if it's stored in separated buffer */
	if (pOpData->digestResult && qat_session->authLen > 0) {
		if ((crp->crp_op & CRYPTO_OP_VERIFY_DIGEST) != 0) {
			char icv[QAT_OCF_MAX_DIGEST] = { 0 };
			crypto_copydata(crp,
					crp->crp_digest_start,
					qat_session->authLen,
					icv);
			if (timingsafe_bcmp(icv,
					    qat_cookie->qat_ocf_digest,
					    qat_session->authLen) != 0) {
				rc = EBADMSG;
				goto exit;
			}
		} else {
			crypto_copyback(crp,
					crp->crp_digest_start,
					qat_session->authLen,
					qat_cookie->qat_ocf_digest);
		}
	}

exit:
	qat_ocf_cookie_free(qat_instance, qat_cookie);
	crp->crp_etype = rc;
	crypto_done(crp);

	return;
}

static inline CpaPhysicalAddr
qatVirtToPhys(void *virtAddr)
{
	return (CpaPhysicalAddr)vtophys(virtAddr);
}

static int
qat_ocf_probesession(device_t dev, const struct crypto_session_params *csp)
{
	if ((csp->csp_flags & ~(CSP_F_SEPARATE_OUTPUT | CSP_F_SEPARATE_AAD)) !=
	    0) {
		return EINVAL;
	}

	switch (csp->csp_mode) {
	case CSP_MODE_CIPHER:
		switch (csp->csp_cipher_alg) {
		case CRYPTO_AES_CBC:
		case CRYPTO_AES_ICM:
			if (csp->csp_ivlen != AES_BLOCK_LEN)
				return EINVAL;
			break;
		case CRYPTO_AES_XTS:
			if (csp->csp_ivlen != AES_XTS_IV_LEN)
				return EINVAL;
			break;
		default:
			return EINVAL;
		}
		break;
	case CSP_MODE_DIGEST:
		switch (csp->csp_auth_alg) {
		case CRYPTO_SHA1:
		case CRYPTO_SHA1_HMAC:
		case CRYPTO_SHA2_256:
		case CRYPTO_SHA2_256_HMAC:
		case CRYPTO_SHA2_384:
		case CRYPTO_SHA2_384_HMAC:
		case CRYPTO_SHA2_512:
		case CRYPTO_SHA2_512_HMAC:
			break;
		case CRYPTO_AES_NIST_GMAC:
			if (csp->csp_ivlen != AES_GCM_IV_LEN)
				return EINVAL;
			break;
		default:
			return EINVAL;
		}
		break;
	case CSP_MODE_AEAD:
		switch (csp->csp_cipher_alg) {
		case CRYPTO_AES_NIST_GCM_16:
			if (csp->csp_ivlen != AES_GCM_IV_LEN)
				return EINVAL;
			break;
		default:
			return EINVAL;
		}
		break;
	case CSP_MODE_ETA:
		switch (csp->csp_auth_alg) {
		case CRYPTO_SHA1_HMAC:
		case CRYPTO_SHA2_256_HMAC:
		case CRYPTO_SHA2_384_HMAC:
		case CRYPTO_SHA2_512_HMAC:
			switch (csp->csp_cipher_alg) {
			case CRYPTO_AES_CBC:
			case CRYPTO_AES_ICM:
				if (csp->csp_ivlen != AES_BLOCK_LEN)
					return EINVAL;
				break;
			case CRYPTO_AES_XTS:
				if (csp->csp_ivlen != AES_XTS_IV_LEN)
					return EINVAL;
				break;
			default:
				return EINVAL;
			}
			break;
		default:
			return EINVAL;
		}
		break;
	default:
		return EINVAL;
	}

	return CRYPTODEV_PROBE_HARDWARE;
}

static CpaStatus
qat_ocf_session_init(device_t dev,
		     struct cryptop *crp,
		     struct qat_ocf_instance *qat_instance,
		     struct qat_ocf_session *qat_ssession)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	/* Crytpodev structures */
	crypto_session_t cses;
	const struct crypto_session_params *csp;
	/* DP API Session configuration */
	CpaCySymSessionSetupData sessionSetupData = { 0 };
	CpaCySymSessionCtx sessionCtx = NULL;
	Cpa32U sessionCtxSize = 0;

	cses = crp->crp_session;
	if (NULL == cses) {
		device_printf(dev, "no crypto session in cryptodev request\n");
		return CPA_STATUS_FAIL;
	}

	csp = crypto_get_params(cses);
	if (NULL == csp) {
		device_printf(dev, "no session in cryptodev session\n");
		return CPA_STATUS_FAIL;
	}

	/* Common fields */
	sessionSetupData.sessionPriority = CPA_CY_PRIORITY_HIGH;
	/* Cipher key */
	if (crp->crp_cipher_key)
		sessionSetupData.cipherSetupData.pCipherKey =
		    crp->crp_cipher_key;
	else
		sessionSetupData.cipherSetupData.pCipherKey =
		    csp->csp_cipher_key;
	sessionSetupData.cipherSetupData.cipherKeyLenInBytes =
	    csp->csp_cipher_klen;

	/* Auth key */
	if (crp->crp_auth_key)
		sessionSetupData.hashSetupData.authModeSetupData.authKey =
		    crp->crp_auth_key;
	else
		sessionSetupData.hashSetupData.authModeSetupData.authKey =
		    csp->csp_auth_key;
	sessionSetupData.hashSetupData.authModeSetupData.authKeyLenInBytes =
	    csp->csp_auth_klen;

	qat_ssession->aadLen = crp->crp_aad_length;
	if (CPA_TRUE == is_sep_aad_supported(csp))
		sessionSetupData.hashSetupData.authModeSetupData.aadLenInBytes =
		    crp->crp_aad_length;
	else
		sessionSetupData.hashSetupData.authModeSetupData.aadLenInBytes =
		    0;

	/* Just setup algorithm - regardless of mode */
	if (csp->csp_cipher_alg) {
		sessionSetupData.symOperation = CPA_CY_SYM_OP_CIPHER;

		switch (csp->csp_cipher_alg) {
		case CRYPTO_AES_CBC:
			sessionSetupData.cipherSetupData.cipherAlgorithm =
			    CPA_CY_SYM_CIPHER_AES_CBC;
			break;
		case CRYPTO_AES_ICM:
			sessionSetupData.cipherSetupData.cipherAlgorithm =
			    CPA_CY_SYM_CIPHER_AES_CTR;
			break;
		case CRYPTO_AES_XTS:
			sessionSetupData.cipherSetupData.cipherAlgorithm =
			    CPA_CY_SYM_CIPHER_AES_XTS;
			break;
		case CRYPTO_AES_NIST_GCM_16:
			sessionSetupData.cipherSetupData.cipherAlgorithm =
			    CPA_CY_SYM_CIPHER_AES_GCM;
			sessionSetupData.hashSetupData.hashAlgorithm =
			    CPA_CY_SYM_HASH_AES_GCM;
			sessionSetupData.hashSetupData.hashMode =
			    CPA_CY_SYM_HASH_MODE_AUTH;
			break;
		default:
			device_printf(dev,
				      "cipher_alg: %d not supported\n",
				      csp->csp_cipher_alg);
			status = CPA_STATUS_UNSUPPORTED;
			goto fail;
		}
	}

	if (csp->csp_auth_alg) {
		switch (csp->csp_auth_alg) {
		case CRYPTO_SHA1_HMAC:
			sessionSetupData.hashSetupData.hashAlgorithm =
			    CPA_CY_SYM_HASH_SHA1;
			sessionSetupData.hashSetupData.hashMode =
			    CPA_CY_SYM_HASH_MODE_AUTH;
			break;
		case CRYPTO_SHA1:
			sessionSetupData.hashSetupData.hashAlgorithm =
			    CPA_CY_SYM_HASH_SHA1;
			sessionSetupData.hashSetupData.hashMode =
			    CPA_CY_SYM_HASH_MODE_PLAIN;
			break;

		case CRYPTO_SHA2_256_HMAC:
			sessionSetupData.hashSetupData.hashAlgorithm =
			    CPA_CY_SYM_HASH_SHA256;
			sessionSetupData.hashSetupData.hashMode =
			    CPA_CY_SYM_HASH_MODE_AUTH;
			break;
		case CRYPTO_SHA2_256:
			sessionSetupData.hashSetupData.hashAlgorithm =
			    CPA_CY_SYM_HASH_SHA256;
			sessionSetupData.hashSetupData.hashMode =
			    CPA_CY_SYM_HASH_MODE_PLAIN;
			break;

		case CRYPTO_SHA2_224_HMAC:
			sessionSetupData.hashSetupData.hashAlgorithm =
			    CPA_CY_SYM_HASH_SHA224;
			sessionSetupData.hashSetupData.hashMode =
			    CPA_CY_SYM_HASH_MODE_AUTH;
			break;
		case CRYPTO_SHA2_224:
			sessionSetupData.hashSetupData.hashAlgorithm =
			    CPA_CY_SYM_HASH_SHA224;
			sessionSetupData.hashSetupData.hashMode =
			    CPA_CY_SYM_HASH_MODE_PLAIN;
			break;

		case CRYPTO_SHA2_384_HMAC:
			sessionSetupData.hashSetupData.hashAlgorithm =
			    CPA_CY_SYM_HASH_SHA384;
			sessionSetupData.hashSetupData.hashMode =
			    CPA_CY_SYM_HASH_MODE_AUTH;
			break;
		case CRYPTO_SHA2_384:
			sessionSetupData.hashSetupData.hashAlgorithm =
			    CPA_CY_SYM_HASH_SHA384;
			sessionSetupData.hashSetupData.hashMode =
			    CPA_CY_SYM_HASH_MODE_PLAIN;
			break;

		case CRYPTO_SHA2_512_HMAC:
			sessionSetupData.hashSetupData.hashAlgorithm =
			    CPA_CY_SYM_HASH_SHA512;
			sessionSetupData.hashSetupData.hashMode =
			    CPA_CY_SYM_HASH_MODE_AUTH;
			break;
		case CRYPTO_SHA2_512:
			sessionSetupData.hashSetupData.hashAlgorithm =
			    CPA_CY_SYM_HASH_SHA512;
			sessionSetupData.hashSetupData.hashMode =
			    CPA_CY_SYM_HASH_MODE_PLAIN;
			break;
		case CRYPTO_AES_NIST_GMAC:
			sessionSetupData.hashSetupData.hashAlgorithm =
			    CPA_CY_SYM_HASH_AES_GMAC;
			break;
		default:
			status = CPA_STATUS_UNSUPPORTED;
			goto fail;
		}
	} /* csp->csp_auth_alg */

	/* Setting digest-length if no cipher-only mode is set */
	if (csp->csp_mode != CSP_MODE_CIPHER) {
		lac_sym_qat_hash_defs_t *pHashDefsInfo = NULL;
		if (csp->csp_auth_mlen) {
			sessionSetupData.hashSetupData.digestResultLenInBytes =
			    csp->csp_auth_mlen;
			qat_ssession->authLen = csp->csp_auth_mlen;
		} else {
			LacSymQat_HashDefsLookupGet(
			    qat_instance->cyInstHandle,
			    sessionSetupData.hashSetupData.hashAlgorithm,
			    &pHashDefsInfo);
			if (NULL == pHashDefsInfo) {
				device_printf(
				    dev,
				    "unable to find corresponding hash data\n");
				status = CPA_STATUS_UNSUPPORTED;
				goto fail;
			}
			sessionSetupData.hashSetupData.digestResultLenInBytes =
			    pHashDefsInfo->algInfo->digestLength;
			qat_ssession->authLen =
			    pHashDefsInfo->algInfo->digestLength;
		}
		sessionSetupData.verifyDigest = CPA_FALSE;
	}

	switch (csp->csp_mode) {
	case CSP_MODE_AEAD:
	case CSP_MODE_ETA:
		sessionSetupData.symOperation =
		    CPA_CY_SYM_OP_ALGORITHM_CHAINING;
		/* Place the digest result in a buffer unrelated to srcBuffer */
		sessionSetupData.digestIsAppended = CPA_FALSE;
		/* Due to FW limitation to verify only appended MACs */
		sessionSetupData.verifyDigest = CPA_FALSE;
		if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op)) {
			sessionSetupData.cipherSetupData.cipherDirection =
			    CPA_CY_SYM_CIPHER_DIRECTION_ENCRYPT;
			sessionSetupData.algChainOrder =
			    CPA_CY_SYM_ALG_CHAIN_ORDER_CIPHER_THEN_HASH;
		} else {
			sessionSetupData.cipherSetupData.cipherDirection =
			    CPA_CY_SYM_CIPHER_DIRECTION_DECRYPT;
			sessionSetupData.algChainOrder =
			    CPA_CY_SYM_ALG_CHAIN_ORDER_HASH_THEN_CIPHER;
		}
		break;
	case CSP_MODE_CIPHER:
		if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op)) {
			sessionSetupData.cipherSetupData.cipherDirection =
			    CPA_CY_SYM_CIPHER_DIRECTION_ENCRYPT;
		} else {
			sessionSetupData.cipherSetupData.cipherDirection =
			    CPA_CY_SYM_CIPHER_DIRECTION_DECRYPT;
		}
		sessionSetupData.symOperation = CPA_CY_SYM_OP_CIPHER;
		break;
	case CSP_MODE_DIGEST:
		sessionSetupData.symOperation = CPA_CY_SYM_OP_HASH;
		if (csp->csp_auth_alg == CRYPTO_AES_NIST_GMAC) {
			sessionSetupData.symOperation =
			    CPA_CY_SYM_OP_ALGORITHM_CHAINING;
			/* GMAC is always encrypt */
			sessionSetupData.cipherSetupData.cipherDirection =
			    CPA_CY_SYM_CIPHER_DIRECTION_ENCRYPT;
			sessionSetupData.algChainOrder =
			    CPA_CY_SYM_ALG_CHAIN_ORDER_CIPHER_THEN_HASH;
			sessionSetupData.cipherSetupData.cipherAlgorithm =
			    CPA_CY_SYM_CIPHER_AES_GCM;
			sessionSetupData.hashSetupData.hashAlgorithm =
			    CPA_CY_SYM_HASH_AES_GMAC;
			sessionSetupData.hashSetupData.hashMode =
			    CPA_CY_SYM_HASH_MODE_AUTH;
			/* Same key for cipher and auth */
			sessionSetupData.cipherSetupData.pCipherKey =
			    csp->csp_auth_key;
			sessionSetupData.cipherSetupData.cipherKeyLenInBytes =
			    csp->csp_auth_klen;
			/* Generated GMAC stored in separated buffer */
			sessionSetupData.digestIsAppended = CPA_FALSE;
			/* Digest verification not allowed in GMAC case */
			sessionSetupData.verifyDigest = CPA_FALSE;
			/* No AAD allowed */
			sessionSetupData.hashSetupData.authModeSetupData
			    .aadLenInBytes = 0;
		} else {
			sessionSetupData.cipherSetupData.cipherDirection =
			    CPA_CY_SYM_CIPHER_DIRECTION_ENCRYPT;
			sessionSetupData.symOperation = CPA_CY_SYM_OP_HASH;
			sessionSetupData.digestIsAppended = CPA_FALSE;
		}
		break;
	default:
		device_printf(dev,
			      "%s: unhandled crypto algorithm %d, %d\n",
			      __func__,
			      csp->csp_cipher_alg,
			      csp->csp_auth_alg);
		status = CPA_STATUS_FAIL;
		goto fail;
	}

	/* Extracting session size */
	status = cpaCySymSessionCtxGetSize(qat_instance->cyInstHandle,
					   &sessionSetupData,
					   &sessionCtxSize);
	if (CPA_STATUS_SUCCESS != status) {
		device_printf(dev, "unable to get session size\n");
		goto fail;
	}

	/* Allocating contiguous memory for session */
	sessionCtx = contigmalloc(sessionCtxSize,
				  M_QAT_OCF,
				  M_NOWAIT,
				  0,
				  ~1UL,
				  1 << (bsrl(sessionCtxSize - 1) + 1),
				  0);
	if (NULL == sessionCtx) {
		device_printf(dev, "unable to allocate memory for session\n");
		status = CPA_STATUS_RESOURCE;
		goto fail;
	}

	status = cpaCySymDpInitSession(qat_instance->cyInstHandle,
				       &sessionSetupData,
				       sessionCtx);
	if (CPA_STATUS_SUCCESS != status) {
		device_printf(dev, "session initialization failed\n");
		goto fail;
	}

	/* NOTE: lets keep double session (both directions) approach to overcome
	 * lack of direction update in FBSD QAT.
	 */
	qat_ssession->sessionCtx = sessionCtx;
	qat_ssession->sessionCtxSize = sessionCtxSize;

	return CPA_STATUS_SUCCESS;

fail:
	/* Release resources if any */
	if (sessionCtx)
		contigfree(sessionCtx, sessionCtxSize, M_QAT_OCF);

	return status;
}

static int
qat_ocf_newsession(device_t dev,
		   crypto_session_t cses,
		   const struct crypto_session_params *csp)
{
	/* Cryptodev QAT structures */
	struct qat_ocf_softc *qat_softc;
	struct qat_ocf_dsession *qat_dsession;
	struct qat_ocf_instance *qat_instance;
	u_int cpu_id = PCPU_GET(cpuid);

	/* Create cryptodev session */
	qat_softc = device_get_softc(dev);
	if (qat_softc->numCyInstances > 0) {
		qat_instance =
		    &qat_softc
			 ->cyInstHandles[cpu_id % qat_softc->numCyInstances];
		qat_dsession = crypto_get_driver_session(cses);
		if (NULL == qat_dsession) {
			device_printf(dev, "Unable to create new session\n");
			return (EINVAL);
		}

		/* Add only instance at this point remaining operations moved to
		 * lazy session init */
		qat_dsession->qatInstance = qat_instance;
	} else {
		return ENXIO;
	}

	return 0;
}

static CpaStatus
qat_ocf_remove_session(device_t dev,
		       CpaInstanceHandle cyInstHandle,
		       struct qat_ocf_session *qat_session)
{
	CpaStatus status = CPA_STATUS_SUCCESS;

	if (NULL == qat_session->sessionCtx)
		return CPA_STATUS_SUCCESS;

	/* User callback is executed right before decrementing pending
	 * callback atomic counter. To avoid removing session rejection
	 * we have to wait a very short while for counter update
	 * after call back execution. */
	status = qat_ocf_wait_for_session(qat_session->sessionCtx,
					  QAT_OCF_SESSION_WAIT_TIMEOUT_MS);
	if (CPA_STATUS_SUCCESS != status) {
		device_printf(dev, "waiting for session un-busy failed\n");
		return CPA_STATUS_FAIL;
	}

	status = cpaCySymDpRemoveSession(cyInstHandle, qat_session->sessionCtx);
	if (CPA_STATUS_SUCCESS != status) {
		device_printf(dev, "error while removing session\n");
		return CPA_STATUS_FAIL;
	}

	explicit_bzero(qat_session->sessionCtx, qat_session->sessionCtxSize);
	contigfree(qat_session->sessionCtx,
		   qat_session->sessionCtxSize,
		   M_QAT_OCF);
	qat_session->sessionCtx = NULL;
	qat_session->sessionCtxSize = 0;

	return CPA_STATUS_SUCCESS;
}

static void
qat_ocf_freesession(device_t dev, crypto_session_t cses)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	struct qat_ocf_dsession *qat_dsession = NULL;
	struct qat_ocf_instance *qat_instance = NULL;

	qat_dsession = crypto_get_driver_session(cses);
	qat_instance = qat_dsession->qatInstance;
	mtx_lock(&qat_instance->cyInstMtx);
	status = qat_ocf_remove_session(dev,
					qat_dsession->qatInstance->cyInstHandle,
					&qat_dsession->encSession);
	if (CPA_STATUS_SUCCESS != status)
		device_printf(dev, "unable to remove encrypt session\n");
	status = qat_ocf_remove_session(dev,
					qat_dsession->qatInstance->cyInstHandle,
					&qat_dsession->decSession);
	if (CPA_STATUS_SUCCESS != status)
		device_printf(dev, "unable to remove decrypt session\n");
	mtx_unlock(&qat_instance->cyInstMtx);
}

/* QAT GCM/CCM FW API are only algorithms which support separated AAD. */
static CpaStatus
qat_ocf_load_aad_gcm(struct cryptop *crp, struct qat_ocf_cookie *qat_cookie)
{
	CpaCySymDpOpData *pOpData;

	pOpData = &qat_cookie->pOpdata;

	if (NULL != crp->crp_aad)
		memcpy(qat_cookie->qat_ocf_gcm_aad,
		       crp->crp_aad,
		       crp->crp_aad_length);
	else
		crypto_copydata(crp,
				crp->crp_aad_start,
				crp->crp_aad_length,
				qat_cookie->qat_ocf_gcm_aad);

	pOpData->pAdditionalAuthData = qat_cookie->qat_ocf_gcm_aad;
	pOpData->additionalAuthData = qat_cookie->qat_ocf_gcm_aad_paddr;

	return CPA_STATUS_SUCCESS;
}

static CpaStatus
qat_ocf_load_aad(struct cryptop *crp, struct qat_ocf_cookie *qat_cookie)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	const struct crypto_session_params *csp;
	CpaCySymDpOpData *pOpData;
	struct qat_ocf_load_cb_arg args;

	pOpData = &qat_cookie->pOpdata;
	pOpData->pAdditionalAuthData = NULL;
	pOpData->additionalAuthData = 0UL;

	if (crp->crp_aad_length == 0)
		return CPA_STATUS_SUCCESS;

	if (crp->crp_aad_length > ICP_QAT_FW_CCM_GCM_AAD_SZ_MAX)
		return CPA_STATUS_FAIL;

	csp = crypto_get_params(crp->crp_session);

	/* Handle GCM/CCM case */
	if (CPA_TRUE == is_sep_aad_supported(csp))
		return qat_ocf_load_aad_gcm(crp, qat_cookie);

	if (NULL == crp->crp_aad) {
		/* AAD already embedded in source buffer */
		pOpData->messageLenToCipherInBytes = crp->crp_payload_length;
		pOpData->cryptoStartSrcOffsetInBytes = crp->crp_payload_start;

		pOpData->messageLenToHashInBytes =
		    crp->crp_aad_length + crp->crp_payload_length;
		pOpData->hashStartSrcOffsetInBytes = crp->crp_aad_start;

		return CPA_STATUS_SUCCESS;
	}

	/* Separated AAD not supported by QAT - lets place the content
	 * of ADD buffer at the very beginning of source SGL */
	args.crp_op = crp;
	args.qat_cookie = qat_cookie;
	args.pOpData = pOpData;
	args.error = 0;
	status = bus_dmamap_load(qat_cookie->gcm_aad_dma_mem.dma_tag,
				 qat_cookie->gcm_aad_dma_mem.dma_map,
				 crp->crp_aad,
				 crp->crp_aad_length,
				 qat_ocf_crypto_load_aadbuf_cb,
				 &args,
				 BUS_DMA_NOWAIT);
	qat_cookie->is_sep_aad_used = CPA_TRUE;

	/* Right after this step we have AAD placed in the first flat buffer
	 * in source SGL */
	pOpData->messageLenToCipherInBytes = crp->crp_payload_length;
	pOpData->cryptoStartSrcOffsetInBytes =
	    crp->crp_aad_length + crp->crp_aad_start + crp->crp_payload_start;

	pOpData->messageLenToHashInBytes =
	    crp->crp_aad_length + crp->crp_payload_length;
	pOpData->hashStartSrcOffsetInBytes = crp->crp_aad_start;

	return status;
}

static CpaStatus
qat_ocf_load(struct cryptop *crp, struct qat_ocf_cookie *qat_cookie)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	CpaCySymDpOpData *pOpData;
	struct qat_ocf_load_cb_arg args;
	/* cryptodev internals */
	const struct crypto_session_params *csp;

	pOpData = &qat_cookie->pOpdata;

	csp = crypto_get_params(crp->crp_session);

	/* Load IV buffer if present */
	if (csp->csp_ivlen > 0) {
		memset(qat_cookie->qat_ocf_iv_buf,
		       0,
		       sizeof(qat_cookie->qat_ocf_iv_buf));
		crypto_read_iv(crp, qat_cookie->qat_ocf_iv_buf);
		pOpData->iv = qat_cookie->qat_ocf_iv_buf_paddr;
		pOpData->pIv = qat_cookie->qat_ocf_iv_buf;
		pOpData->ivLenInBytes = csp->csp_ivlen;
	}

	/* GCM/CCM - load AAD to separated buffer
	 * AES+SHA - load AAD to first flat in SGL */
	status = qat_ocf_load_aad(crp, qat_cookie);
	if (CPA_STATUS_SUCCESS != status)
		goto fail;

	/* Load source buffer */
	args.crp_op = crp;
	args.qat_cookie = qat_cookie;
	args.pOpData = pOpData;
	args.error = 0;
	status = bus_dmamap_load_crp_buffer(qat_cookie->src_dma_mem.dma_tag,
					    qat_cookie->src_dma_mem.dma_map,
					    &crp->crp_buf,
					    qat_ocf_crypto_load_buf_cb,
					    &args,
					    BUS_DMA_NOWAIT);
	if (CPA_STATUS_SUCCESS != status)
		goto fail;
	pOpData->srcBuffer = qat_cookie->src_buffer_list_paddr;
	pOpData->srcBufferLen = CPA_DP_BUFLIST;

	/* Load destination buffer */
	if (CRYPTO_HAS_OUTPUT_BUFFER(crp)) {
		status =
		    bus_dmamap_load_crp_buffer(qat_cookie->dst_dma_mem.dma_tag,
					       qat_cookie->dst_dma_mem.dma_map,
					       &crp->crp_obuf,
					       qat_ocf_crypto_load_obuf_cb,
					       &args,
					       BUS_DMA_NOWAIT);
		if (CPA_STATUS_SUCCESS != status)
			goto fail;
		pOpData->dstBuffer = qat_cookie->dst_buffer_list_paddr;
		pOpData->dstBufferLen = CPA_DP_BUFLIST;
	} else {
		pOpData->dstBuffer = pOpData->srcBuffer;
		pOpData->dstBufferLen = pOpData->srcBufferLen;
	}

	if (CPA_TRUE == is_use_sep_digest(csp))
		pOpData->digestResult = qat_cookie->qat_ocf_digest_paddr;
	else
		pOpData->digestResult = 0UL;

	/* GMAC - aka zero length buffer */
	if (CPA_TRUE == is_gmac_exception(csp))
		pOpData->messageLenToCipherInBytes = 0;

fail:
	return status;
}

static int
qat_ocf_check_input(device_t dev, struct cryptop *crp)
{
	const struct crypto_session_params *csp;
	csp = crypto_get_params(crp->crp_session);

	if (crypto_buffer_len(&crp->crp_buf) > QAT_OCF_MAX_LEN)
		return E2BIG;

	if (CPA_TRUE == is_sep_aad_supported(csp) &&
	    (crp->crp_aad_length > ICP_QAT_FW_CCM_GCM_AAD_SZ_MAX))
		return EBADMSG;

	return 0;
}

static int
qat_ocf_process(device_t dev, struct cryptop *crp, int hint)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	int rc = 0;
	struct qat_ocf_dsession *qat_dsession = NULL;
	struct qat_ocf_session *qat_session = NULL;
	struct qat_ocf_instance *qat_instance = NULL;
	CpaCySymDpOpData *pOpData = NULL;
	struct qat_ocf_cookie *qat_cookie = NULL;
	CpaBoolean memLoaded = CPA_FALSE;

	rc = qat_ocf_check_input(dev, crp);
	if (rc)
		goto fail;

	qat_dsession = crypto_get_driver_session(crp->crp_session);

	if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op))
		qat_session = &qat_dsession->encSession;
	else
		qat_session = &qat_dsession->decSession;
	qat_instance = qat_dsession->qatInstance;

	status = qat_ocf_cookie_alloc(qat_instance, &qat_cookie);
	if (CPA_STATUS_SUCCESS != status) {
		rc = EAGAIN;
		goto fail;
	}

	qat_cookie->crp_op = crp;

	/* Common request fields */
	pOpData = &qat_cookie->pOpdata;
	pOpData->instanceHandle = qat_instance->cyInstHandle;
	pOpData->sessionCtx = NULL;

	/* Cipher fields */
	pOpData->cryptoStartSrcOffsetInBytes = crp->crp_payload_start;
	pOpData->messageLenToCipherInBytes = crp->crp_payload_length;
	/* Digest fields - any exceptions from this basic rules are covered
	 * in qat_ocf_load */
	pOpData->hashStartSrcOffsetInBytes = crp->crp_payload_start;
	pOpData->messageLenToHashInBytes = crp->crp_payload_length;

	status = qat_ocf_load(crp, qat_cookie);
	if (CPA_STATUS_SUCCESS != status) {
		device_printf(dev,
			      "unable to load OCF buffers to QAT DMA "
			      "transaction\n");
		rc = EIO;
		goto fail;
	}
	memLoaded = CPA_TRUE;

	status = qat_ocf_cookie_dma_pre_sync(crp, pOpData);
	if (CPA_STATUS_SUCCESS != status) {
		device_printf(dev, "unable to sync DMA buffers\n");
		rc = EIO;
		goto fail;
	}

	mtx_lock(&qat_instance->cyInstMtx);
	/* Session initialization at the first request. It's done
	 * in such way to overcome missing QAT specific session data
	 * such like AAD length and limited possibility to update
	 * QAT session while handling traffic.
	 */
	if (NULL == qat_session->sessionCtx) {
		status =
		    qat_ocf_session_init(dev, crp, qat_instance, qat_session);
		if (CPA_STATUS_SUCCESS != status) {
			mtx_unlock(&qat_instance->cyInstMtx);
			device_printf(dev, "unable to init session\n");
			rc = EIO;
			goto fail;
		}
	} else {
		status = qat_ocf_handle_session_update(qat_dsession, crp);
		if (CPA_STATUS_RESOURCE == status) {
			mtx_unlock(&qat_instance->cyInstMtx);
			rc = EAGAIN;
			goto fail;
		} else if (CPA_STATUS_SUCCESS != status) {
			mtx_unlock(&qat_instance->cyInstMtx);
			rc = EIO;
			goto fail;
		}
	}
	pOpData->sessionCtx = qat_session->sessionCtx;
	status = cpaCySymDpEnqueueOp(pOpData, CPA_TRUE);
	mtx_unlock(&qat_instance->cyInstMtx);
	if (CPA_STATUS_SUCCESS != status) {
		if (CPA_STATUS_RETRY == status) {
			rc = EAGAIN;
			goto fail;
		}
		device_printf(dev,
			      "unable to send request. Status: %d\n",
			      status);
		rc = EIO;
		goto fail;
	}

	return 0;
fail:
	if (qat_cookie) {
		if (memLoaded)
			qat_ocf_cookie_dma_unload(crp, pOpData);
		qat_ocf_cookie_free(qat_instance, qat_cookie);
	}
	crp->crp_etype = rc;
	crypto_done(crp);

	return 0;
}

static void
qat_ocf_identify(driver_t *drv, device_t parent)
{
	if (device_find_child(parent, "qat_ocf", -1) == NULL &&
	    BUS_ADD_CHILD(parent, 200, "qat_ocf", -1) == 0)
		device_printf(parent, "qat_ocf: could not attach!");
}

static int
qat_ocf_probe(device_t dev)
{
	device_set_desc(dev, "QAT engine");
	return (BUS_PROBE_NOWILDCARD);
}

static CpaStatus
qat_ocf_get_irq_instances(CpaInstanceHandle *cyInstHandles,
			  Cpa16U cyInstHandlesSize,
			  Cpa16U *foundInstances)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	icp_accel_dev_t **pAdfInsts = NULL;
	icp_accel_dev_t *dev_addr = NULL;
	sal_t *baseAddr = NULL;
	sal_list_t *listTemp = NULL;
	CpaInstanceHandle cyInstHandle;
	CpaInstanceInfo2 info;
	Cpa16U numDevices;
	Cpa32U instCtr = 0;
	Cpa32U i;

	/* Get the number of devices */
	status = icp_amgr_getNumInstances(&numDevices);
	if (CPA_STATUS_SUCCESS != status)
		return status;

	/* Allocate memory to store addr of accel_devs */
	pAdfInsts =
	    malloc(numDevices * sizeof(icp_accel_dev_t *), M_QAT_OCF, M_WAITOK);

	/* Get ADF to return all accel_devs that support either
	 * symmetric or asymmetric crypto */
	status = icp_amgr_getAllAccelDevByCapabilities(
	    (ICP_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC), pAdfInsts, &numDevices);
	if (CPA_STATUS_SUCCESS != status) {
		free(pAdfInsts, M_QAT_OCF);
		return status;
	}

	for (i = 0; i < numDevices; i++) {
		dev_addr = (icp_accel_dev_t *)pAdfInsts[i];
		baseAddr = dev_addr->pSalHandle;
		if (NULL == baseAddr)
			continue;
		listTemp = baseAddr->sym_services;
		if (NULL == listTemp) {
			listTemp = baseAddr->crypto_services;
		}

		while (NULL != listTemp) {
			cyInstHandle = SalList_getObject(listTemp);
			status = cpaCyInstanceGetInfo2(cyInstHandle, &info);
			if (CPA_STATUS_SUCCESS != status)
				continue;
			listTemp = SalList_next(listTemp);
			if (CPA_TRUE == info.isPolled)
				continue;
			if (instCtr >= cyInstHandlesSize)
				break;
			cyInstHandles[instCtr++] = cyInstHandle;
		}
	}
	free(pAdfInsts, M_QAT_OCF);
	*foundInstances = instCtr;

	return CPA_STATUS_SUCCESS;
}

static CpaStatus
qat_ocf_start_instances(struct qat_ocf_softc *qat_softc, device_t dev)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	Cpa16U numInstances = 0;
	CpaInstanceHandle cyInstHandles[QAT_OCF_MAX_INSTANCES] = { 0 };
	CpaInstanceHandle cyInstHandle = NULL;
	Cpa32U startedInstances = 0;
	Cpa32U i;

	qat_softc->numCyInstances = 0;
	status = qat_ocf_get_irq_instances(cyInstHandles,
					   QAT_OCF_MAX_INSTANCES,
					   &numInstances);
	if (CPA_STATUS_SUCCESS != status)
		return status;

	for (i = 0; i < numInstances; i++) {
		struct qat_ocf_instance *qat_ocf_instance;

		cyInstHandle = cyInstHandles[i];
		if (!cyInstHandle)
			continue;

		/* Starting instance */
		status = cpaCyStartInstance(cyInstHandle);
		if (CPA_STATUS_SUCCESS != status) {
			device_printf(qat_softc->sc_dev,
				      "unable to get start instance\n");
			continue;
		}

		qat_ocf_instance = &qat_softc->cyInstHandles[startedInstances];
		qat_ocf_instance->cyInstHandle = cyInstHandle;
		mtx_init(&qat_ocf_instance->cyInstMtx,
			 "Instance MTX",
			 NULL,
			 MTX_DEF);

		status =
		    cpaCySetAddressTranslation(cyInstHandle, qatVirtToPhys);
		if (CPA_STATUS_SUCCESS != status) {
			device_printf(qat_softc->sc_dev,
				      "unable to add virt to phys callback\n");
			goto fail;
		}

		status = cpaCySymDpRegCbFunc(cyInstHandle, symDpCallback);
		if (CPA_STATUS_SUCCESS != status) {
			device_printf(qat_softc->sc_dev,
				      "unable to add user callback\n");
			goto fail;
		}

		/* Initialize cookie pool */
		status = qat_ocf_cookie_pool_init(qat_ocf_instance, dev);
		if (CPA_STATUS_SUCCESS != status) {
			device_printf(qat_softc->sc_dev,
				      "unable to create cookie pool\n");
			goto fail;
		}

		/* Disable forcing HW MAC validation for AEAD */
		status = icp_sal_setForceAEADMACVerify(cyInstHandle, CPA_FALSE);
		if (CPA_STATUS_SUCCESS != status) {
			device_printf(
			    qat_softc->sc_dev,
			    "unable to disable AEAD HW MAC verification\n");
			goto fail;
		}

		qat_ocf_instance->driver_id = qat_softc->cryptodev_id;

		startedInstances++;
		continue;
	fail:
		mtx_destroy(&qat_ocf_instance->cyInstMtx);

		/* Stop instance */
		status = cpaCyStopInstance(cyInstHandle);
		if (CPA_STATUS_SUCCESS != status)
			device_printf(qat_softc->sc_dev,
				      "unable to stop the instance\n");
	}
	qat_softc->numCyInstances = startedInstances;

	return CPA_STATUS_SUCCESS;
}

static CpaStatus
qat_ocf_stop_instances(struct qat_ocf_softc *qat_softc)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	int i;

	for (i = 0; i < qat_softc->numCyInstances; i++) {
		struct qat_ocf_instance *qat_instance;

		qat_instance = &qat_softc->cyInstHandles[i];
		status = cpaCyStopInstance(qat_instance->cyInstHandle);
		if (CPA_STATUS_SUCCESS != status) {
			pr_err("QAT: stopping instance id: %d failed\n", i);
			continue;
		}
		qat_ocf_cookie_pool_deinit(qat_instance);
		mtx_destroy(&qat_instance->cyInstMtx);
	}

	qat_softc->numCyInstances = 0;

	return status;
}

static int
qat_ocf_deinit(struct qat_ocf_softc *qat_softc)
{
	int status = 0;
	CpaStatus cpaStatus;

	if (qat_softc->cryptodev_id >= 0) {
		crypto_unregister_all(qat_softc->cryptodev_id);
		qat_softc->cryptodev_id = -1;
	}

	/* Stop QAT instances */
	cpaStatus = qat_ocf_stop_instances(qat_softc);
	if (CPA_STATUS_SUCCESS != cpaStatus) {
		device_printf(qat_softc->sc_dev, "unable to stop instances\n");
		status = EIO;
	}

	return status;
}

static int
qat_ocf_init(struct qat_ocf_softc *qat_softc)
{
	int32_t cryptodev_id;

	/* Starting instances for OCF */
	if (qat_ocf_start_instances(qat_softc, qat_softc->sc_dev)) {
		device_printf(qat_softc->sc_dev,
			      "unable to get QAT IRQ instances\n");
		goto fail;
	}

	/* Register only if instances available */
	if (qat_softc->numCyInstances) {
		cryptodev_id =
		    crypto_get_driverid(qat_softc->sc_dev,
					sizeof(struct qat_ocf_dsession),
					CRYPTOCAP_F_HARDWARE);
		if (cryptodev_id < 0) {
			device_printf(qat_softc->sc_dev,
				      "cannot initialize!\n");
			goto fail;
		}
		qat_softc->cryptodev_id = cryptodev_id;
	}

	return 0;
fail:
	qat_ocf_deinit(qat_softc);

	return ENXIO;
}

static int qat_ocf_sysctl_handle(SYSCTL_HANDLER_ARGS)
{
	struct qat_ocf_softc *qat_softc = NULL;
	int ret = 0;
	device_t dev = arg1;
	u_int enabled;

	qat_softc = device_get_softc(dev);
	enabled = qat_softc->enabled;

	ret = sysctl_handle_int(oidp, &enabled, 0, req);
	if (ret || !req->newptr)
		return (ret);

	if (qat_softc->enabled != enabled) {
		if (enabled) {
			ret = qat_ocf_init(qat_softc);

		} else {
			ret = qat_ocf_deinit(qat_softc);
		}

		if (!ret)
			qat_softc->enabled = enabled;
	}

	return ret;
}

static int
qat_ocf_attach(device_t dev)
{
	int status;
	struct qat_ocf_softc *qat_softc;

	qat_softc = device_get_softc(dev);
	qat_softc->sc_dev = dev;
	qat_softc->cryptodev_id = -1;
	qat_softc->enabled = 1;

	qat_softc->rc =
	    SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
			    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
			    OID_AUTO,
			    "enable",
			    CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
			    dev,
			    0,
			    qat_ocf_sysctl_handle,
			    "I",
			    "QAT OCF support enablement");

	if (!qat_softc->rc)
		return ENOMEM;
	if (qat_softc->enabled) {
		status = qat_ocf_init(qat_softc);
		if (status) {
			device_printf(dev, "qat_ocf init failed\n");
			goto fail;
		}
	}

	return 0;
fail:
	qat_ocf_deinit(qat_softc);

	return (ENXIO);
}

static int
qat_ocf_detach(device_t dev)
{
	struct qat_ocf_softc *qat_softc = device_get_softc(dev);

	return qat_ocf_deinit(qat_softc);
}

static device_method_t qat_ocf_methods[] =
    { DEVMETHOD(device_identify, qat_ocf_identify),
      DEVMETHOD(device_probe, qat_ocf_probe),
      DEVMETHOD(device_attach, qat_ocf_attach),
      DEVMETHOD(device_detach, qat_ocf_detach),

      /* Cryptodev interface */
      DEVMETHOD(cryptodev_probesession, qat_ocf_probesession),
      DEVMETHOD(cryptodev_newsession, qat_ocf_newsession),
      DEVMETHOD(cryptodev_freesession, qat_ocf_freesession),
      DEVMETHOD(cryptodev_process, qat_ocf_process),

      DEVMETHOD_END };

static driver_t qat_ocf_driver = {
	.name = "qat_ocf",
	.methods = qat_ocf_methods,
	.size = sizeof(struct qat_ocf_softc),
};


DRIVER_MODULE_ORDERED(qat,
		      nexus,
		      qat_ocf_driver,
		      NULL,
		      NULL,
		      SI_ORDER_ANY);
MODULE_VERSION(qat, 1);
MODULE_DEPEND(qat, qat_c62x, 1, 1, 1);
MODULE_DEPEND(qat, qat_200xx, 1, 1, 1);
MODULE_DEPEND(qat, qat_c3xxx, 1, 1, 1);
MODULE_DEPEND(qat, qat_c4xxx, 1, 1, 1);
MODULE_DEPEND(qat, qat_dh895xcc, 1, 1, 1);
MODULE_DEPEND(qat, qat_4xxx, 1, 1, 1);
MODULE_DEPEND(qat, crypto, 1, 1, 1);
MODULE_DEPEND(qat, qat_common, 1, 1, 1);
MODULE_DEPEND(qat, qat_api, 1, 1, 1);
MODULE_DEPEND(qat, linuxkpi, 1, 1, 1);
