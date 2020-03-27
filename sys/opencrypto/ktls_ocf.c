/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Netflix Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/counter.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/ktls.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <opencrypto/cryptodev.h>

struct ocf_session {
	crypto_session_t sid;
	struct mtx lock;
};

struct ocf_operation {
	struct ocf_session *os;
	bool done;
	struct iovec iov[0];
};

static MALLOC_DEFINE(M_KTLS_OCF, "ktls_ocf", "OCF KTLS");

SYSCTL_DECL(_kern_ipc_tls);
SYSCTL_DECL(_kern_ipc_tls_stats);

static SYSCTL_NODE(_kern_ipc_tls_stats, OID_AUTO, ocf,
    CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "Kernel TLS offload via OCF stats");

static counter_u64_t ocf_tls12_gcm_crypts;
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats_ocf, OID_AUTO, tls12_gcm_crypts,
    CTLFLAG_RD, &ocf_tls12_gcm_crypts,
    "Total number of OCF TLS 1.2 GCM encryption operations");

static counter_u64_t ocf_tls13_gcm_crypts;
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats_ocf, OID_AUTO, tls13_gcm_crypts,
    CTLFLAG_RD, &ocf_tls13_gcm_crypts,
    "Total number of OCF TLS 1.3 GCM encryption operations");

static counter_u64_t ocf_retries;
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats_ocf, OID_AUTO, retries, CTLFLAG_RD,
    &ocf_retries,
    "Number of OCF encryption operation retries");

static int
ktls_ocf_callback(struct cryptop *crp)
{
	struct ocf_operation *oo;

	oo = crp->crp_opaque;
	mtx_lock(&oo->os->lock);
	oo->done = true;
	mtx_unlock(&oo->os->lock);
	wakeup(oo);
	return (0);
}

static int
ktls_ocf_tls12_gcm_encrypt(struct ktls_session *tls,
    const struct tls_record_layer *hdr, uint8_t *trailer, struct iovec *iniov,
    struct iovec *outiov, int iovcnt, uint64_t seqno,
    uint8_t record_type __unused)
{
	struct uio uio;
	struct tls_aead_data ad;
	struct cryptop *crp;
	struct ocf_session *os;
	struct ocf_operation *oo;
	struct iovec *iov;
	int i, error;
	uint16_t tls_comp_len;

	os = tls->cipher;

	oo = malloc(sizeof(*oo) + (iovcnt + 2) * sizeof(*iov), M_KTLS_OCF,
	    M_WAITOK | M_ZERO);
	oo->os = os;
	iov = oo->iov;

	crp = crypto_getreq(os->sid, M_WAITOK);

	/* Setup the IV. */
	memcpy(crp->crp_iv, tls->params.iv, TLS_AEAD_GCM_LEN);
	memcpy(crp->crp_iv + TLS_AEAD_GCM_LEN, hdr + 1, sizeof(uint64_t));

	/* Setup the AAD. */
	tls_comp_len = ntohs(hdr->tls_length) -
	    (AES_GMAC_HASH_LEN + sizeof(uint64_t));
	ad.seq = htobe64(seqno);
	ad.type = hdr->tls_type;
	ad.tls_vmajor = hdr->tls_vmajor;
	ad.tls_vminor = hdr->tls_vminor;
	ad.tls_length = htons(tls_comp_len);
	iov[0].iov_base = &ad;
	iov[0].iov_len = sizeof(ad);
	uio.uio_resid = sizeof(ad);

	/*
	 * OCF always does encryption in place, so copy the data if
	 * needed.  Ugh.
	 */
	for (i = 0; i < iovcnt; i++) {
		iov[i + 1] = outiov[i];
		if (iniov[i].iov_base != outiov[i].iov_base)
			memcpy(outiov[i].iov_base, iniov[i].iov_base,
			    outiov[i].iov_len);
		uio.uio_resid += outiov[i].iov_len;
	}

	iov[iovcnt + 1].iov_base = trailer;
	iov[iovcnt + 1].iov_len = AES_GMAC_HASH_LEN;
	uio.uio_resid += AES_GMAC_HASH_LEN;

	uio.uio_iov = iov;
	uio.uio_iovcnt = iovcnt + 2;
	uio.uio_offset = 0;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_td = curthread;

	crp->crp_op = CRYPTO_OP_ENCRYPT | CRYPTO_OP_COMPUTE_DIGEST;
	crp->crp_flags = CRYPTO_F_CBIMM | CRYPTO_F_IV_SEPARATE;
	crp->crp_buf_type = CRYPTO_BUF_UIO;
	crp->crp_uio = &uio;
	crp->crp_ilen = uio.uio_resid;
	crp->crp_opaque = oo;
	crp->crp_callback = ktls_ocf_callback;

	crp->crp_aad_start = 0;
	crp->crp_aad_length = sizeof(ad);
	crp->crp_payload_start = sizeof(ad);
	crp->crp_payload_length = crp->crp_ilen -
	    (sizeof(ad) + AES_GMAC_HASH_LEN);
	crp->crp_digest_start = crp->crp_ilen - AES_GMAC_HASH_LEN;

	counter_u64_add(ocf_tls12_gcm_crypts, 1);
	for (;;) {
		error = crypto_dispatch(crp);
		if (error)
			break;

		mtx_lock(&os->lock);
		while (!oo->done)
			mtx_sleep(oo, &os->lock, 0, "ocfktls", 0);
		mtx_unlock(&os->lock);

		if (crp->crp_etype != EAGAIN) {
			error = crp->crp_etype;
			break;
		}

		crp->crp_etype = 0;
		crp->crp_flags &= ~CRYPTO_F_DONE;
		oo->done = false;
		counter_u64_add(ocf_retries, 1);
	}

	crypto_freereq(crp);
	free(oo, M_KTLS_OCF);
	return (error);
}

static int
ktls_ocf_tls13_gcm_encrypt(struct ktls_session *tls,
    const struct tls_record_layer *hdr, uint8_t *trailer, struct iovec *iniov,
    struct iovec *outiov, int iovcnt, uint64_t seqno, uint8_t record_type)
{
	struct uio uio;
	struct tls_aead_data_13 ad;
	char nonce[12];
	struct cryptop *crp;
	struct ocf_session *os;
	struct ocf_operation *oo;
	struct iovec *iov;
	int i, error;

	os = tls->cipher;

	oo = malloc(sizeof(*oo) + (iovcnt + 2) * sizeof(*iov), M_KTLS_OCF,
	    M_WAITOK | M_ZERO);
	oo->os = os;
	iov = oo->iov;

	crp = crypto_getreq(os->sid, M_WAITOK);

	/* Setup the nonce. */
	memcpy(nonce, tls->params.iv, tls->params.iv_len);
	*(uint64_t *)(nonce + 4) ^= htobe64(seqno);

	/* Setup the AAD. */
	ad.type = hdr->tls_type;
	ad.tls_vmajor = hdr->tls_vmajor;
	ad.tls_vminor = hdr->tls_vminor;
	ad.tls_length = hdr->tls_length;
	iov[0].iov_base = &ad;
	iov[0].iov_len = sizeof(ad);
	uio.uio_resid = sizeof(ad);

	/*
	 * OCF always does encryption in place, so copy the data if
	 * needed.  Ugh.
	 */
	for (i = 0; i < iovcnt; i++) {
		iov[i + 1] = outiov[i];
		if (iniov[i].iov_base != outiov[i].iov_base)
			memcpy(outiov[i].iov_base, iniov[i].iov_base,
			    outiov[i].iov_len);
		uio.uio_resid += outiov[i].iov_len;
	}

	trailer[0] = record_type;
	iov[iovcnt + 1].iov_base = trailer;
	iov[iovcnt + 1].iov_len = AES_GMAC_HASH_LEN + 1;
	uio.uio_resid += AES_GMAC_HASH_LEN + 1;

	uio.uio_iov = iov;
	uio.uio_iovcnt = iovcnt + 2;
	uio.uio_offset = 0;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_td = curthread;

	crp->crp_op = CRYPTO_OP_ENCRYPT | CRYPTO_OP_COMPUTE_DIGEST;
	crp->crp_flags = CRYPTO_F_CBIMM | CRYPTO_F_IV_SEPARATE;
	crp->crp_buf_type = CRYPTO_BUF_UIO;
	crp->crp_uio = &uio;
	crp->crp_ilen = uio.uio_resid;
	crp->crp_opaque = oo;
	crp->crp_callback = ktls_ocf_callback;

	crp->crp_aad_start = 0;
	crp->crp_aad_length = sizeof(ad);
	crp->crp_payload_start = sizeof(ad);
	crp->crp_payload_length = crp->crp_ilen -
	    (sizeof(ad) + AES_GMAC_HASH_LEN);
	crp->crp_digest_start = crp->crp_ilen - AES_GMAC_HASH_LEN;
	memcpy(crp->crp_iv, nonce, sizeof(nonce));

	counter_u64_add(ocf_tls13_gcm_crypts, 1);
	for (;;) {
		error = crypto_dispatch(crp);
		if (error)
			break;

		mtx_lock(&os->lock);
		while (!oo->done)
			mtx_sleep(oo, &os->lock, 0, "ocfktls", 0);
		mtx_unlock(&os->lock);

		if (crp->crp_etype != EAGAIN) {
			error = crp->crp_etype;
			break;
		}

		crp->crp_etype = 0;
		crp->crp_flags &= ~CRYPTO_F_DONE;
		oo->done = false;
		counter_u64_add(ocf_retries, 1);
	}

	crypto_freereq(crp);
	free(oo, M_KTLS_OCF);
	return (error);
}

static void
ktls_ocf_free(struct ktls_session *tls)
{
	struct ocf_session *os;

	os = tls->cipher;
	crypto_freesession(os->sid);
	mtx_destroy(&os->lock);
	explicit_bzero(os, sizeof(*os));
	free(os, M_KTLS_OCF);
}

static int
ktls_ocf_try(struct socket *so, struct ktls_session *tls)
{
	struct crypto_session_params csp;
	struct ocf_session *os;
	int error;

	memset(&csp, 0, sizeof(csp));

	switch (tls->params.cipher_algorithm) {
	case CRYPTO_AES_NIST_GCM_16:
		switch (tls->params.cipher_key_len) {
		case 128 / 8:
		case 256 / 8:
			break;
		default:
			return (EINVAL);
		}
		csp.csp_mode = CSP_MODE_AEAD;
		csp.csp_cipher_alg = CRYPTO_AES_NIST_GCM_16;
		csp.csp_cipher_key = tls->params.cipher_key;
		csp.csp_cipher_klen = tls->params.cipher_key_len;
		csp.csp_ivlen = AES_GCM_IV_LEN;
		break;
	default:
		return (EPROTONOSUPPORT);
	}

	/* Only TLS 1.2 and 1.3 are supported. */
	if (tls->params.tls_vmajor != TLS_MAJOR_VER_ONE ||
	    tls->params.tls_vminor < TLS_MINOR_VER_TWO ||
	    tls->params.tls_vminor > TLS_MINOR_VER_THREE)
		return (EPROTONOSUPPORT);

	os = malloc(sizeof(*os), M_KTLS_OCF, M_NOWAIT | M_ZERO);
	if (os == NULL)
		return (ENOMEM);

	error = crypto_newsession(&os->sid, &csp,
	    CRYPTO_FLAG_HARDWARE | CRYPTO_FLAG_SOFTWARE);
	if (error) {
		free(os, M_KTLS_OCF);
		return (error);
	}

	mtx_init(&os->lock, "ktls_ocf", NULL, MTX_DEF);
	tls->cipher = os;
	if (tls->params.tls_vminor == TLS_MINOR_VER_THREE)
		tls->sw_encrypt = ktls_ocf_tls13_gcm_encrypt;
	else
		tls->sw_encrypt = ktls_ocf_tls12_gcm_encrypt;
	tls->free = ktls_ocf_free;
	return (0);
}

struct ktls_crypto_backend ocf_backend = {
	.name = "OCF",
	.prio = 5,
	.api_version = KTLS_API_VERSION,
	.try = ktls_ocf_try,
};

static int
ktls_ocf_modevent(module_t mod, int what, void *arg)
{
	int error;

	switch (what) {
	case MOD_LOAD:
		ocf_tls12_gcm_crypts = counter_u64_alloc(M_WAITOK);
		ocf_tls13_gcm_crypts = counter_u64_alloc(M_WAITOK);
		ocf_retries = counter_u64_alloc(M_WAITOK);
		return (ktls_crypto_backend_register(&ocf_backend));
	case MOD_UNLOAD:
		error = ktls_crypto_backend_deregister(&ocf_backend);
		if (error)
			return (error);
		counter_u64_free(ocf_tls12_gcm_crypts);
		counter_u64_free(ocf_tls13_gcm_crypts);
		counter_u64_free(ocf_retries);
		return (0);
	default:
		return (EOPNOTSUPP);
	}
}

static moduledata_t ktls_ocf_moduledata = {
	"ktls_ocf",
	ktls_ocf_modevent,
	NULL
};

DECLARE_MODULE(ktls_ocf, ktls_ocf_moduledata, SI_SUB_PROTO_END, SI_ORDER_ANY);
