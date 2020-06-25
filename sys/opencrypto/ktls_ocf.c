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

static counter_u64_t ocf_inplace;
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats_ocf, OID_AUTO, inplace,
    CTLFLAG_RD, &ocf_inplace,
    "Total number of OCF in-place operations");

static counter_u64_t ocf_separate_output;
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats_ocf, OID_AUTO, separate_output,
    CTLFLAG_RD, &ocf_separate_output,
    "Total number of OCF operations with a separate output buffer");

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
	struct uio uio, out_uio, *tag_uio;
	struct tls_aead_data ad;
	struct cryptop *crp;
	struct ocf_session *os;
	struct ocf_operation *oo;
	int i, error;
	uint16_t tls_comp_len;
	bool inplace;

	os = tls->cipher;

	oo = malloc(sizeof(*oo) + (iovcnt + 1) * sizeof(struct iovec),
	    M_KTLS_OCF, M_WAITOK | M_ZERO);
	oo->os = os;

	uio.uio_iov = iniov;
	uio.uio_iovcnt = iovcnt;
	uio.uio_offset = 0;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_td = curthread;

	out_uio.uio_iov = outiov;
	out_uio.uio_iovcnt = iovcnt;
	out_uio.uio_offset = 0;
	out_uio.uio_segflg = UIO_SYSSPACE;
	out_uio.uio_td = curthread;

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
	crp->crp_aad = &ad;
	crp->crp_aad_length = sizeof(ad);

	/* Compute payload length and determine if encryption is in place. */
	inplace = true;
	crp->crp_payload_start = 0;
	for (i = 0; i < iovcnt; i++) {
		if (iniov[i].iov_base != outiov[i].iov_base)
			inplace = false;
		crp->crp_payload_length += iniov[i].iov_len;
	}
	uio.uio_resid = crp->crp_payload_length;
	out_uio.uio_resid = crp->crp_payload_length;

	if (inplace)
		tag_uio = &uio;
	else
		tag_uio = &out_uio;

	/* Duplicate iovec and append vector for tag. */
	memcpy(oo->iov, tag_uio->uio_iov, iovcnt * sizeof(struct iovec));
	tag_uio->uio_iov = oo->iov;
	tag_uio->uio_iov[iovcnt].iov_base = trailer;
	tag_uio->uio_iov[iovcnt].iov_len = AES_GMAC_HASH_LEN;
	tag_uio->uio_iovcnt++;
	crp->crp_digest_start = tag_uio->uio_resid;
	tag_uio->uio_resid += AES_GMAC_HASH_LEN;

	crp->crp_op = CRYPTO_OP_ENCRYPT | CRYPTO_OP_COMPUTE_DIGEST;
	crp->crp_flags = CRYPTO_F_CBIMM | CRYPTO_F_IV_SEPARATE;
	crypto_use_uio(crp, &uio);
	if (!inplace)
		crypto_use_output_uio(crp, &out_uio);
	crp->crp_opaque = oo;
	crp->crp_callback = ktls_ocf_callback;

	counter_u64_add(ocf_tls12_gcm_crypts, 1);
	if (inplace)
		counter_u64_add(ocf_inplace, 1);
	else
		counter_u64_add(ocf_separate_output, 1);
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
	struct uio uio, out_uio;
	struct tls_aead_data_13 ad;
	char nonce[12];
	struct cryptop *crp;
	struct ocf_session *os;
	struct ocf_operation *oo;
	struct iovec *iov, *out_iov;
	int i, error;
	bool inplace;

	os = tls->cipher;

	oo = malloc(sizeof(*oo) + (iovcnt + 1) * sizeof(*iov) * 2, M_KTLS_OCF,
	    M_WAITOK | M_ZERO);
	oo->os = os;
	iov = oo->iov;
	out_iov = iov + iovcnt + 2;

	crp = crypto_getreq(os->sid, M_WAITOK);

	/* Setup the nonce. */
	memcpy(nonce, tls->params.iv, tls->params.iv_len);
	*(uint64_t *)(nonce + 4) ^= htobe64(seqno);

	/* Setup the AAD. */
	ad.type = hdr->tls_type;
	ad.tls_vmajor = hdr->tls_vmajor;
	ad.tls_vminor = hdr->tls_vminor;
	ad.tls_length = hdr->tls_length;
	crp->crp_aad = &ad;
	crp->crp_aad_length = sizeof(ad);

	/* Compute payload length and determine if encryption is in place. */
	inplace = true;
	crp->crp_payload_start = 0;
	for (i = 0; i < iovcnt; i++) {
		if (iniov[i].iov_base != outiov[i].iov_base)
			inplace = false;
		crp->crp_payload_length += iniov[i].iov_len;
	}

	/* Store the record type as the first byte of the trailer. */
	trailer[0] = record_type;
	crp->crp_payload_length++;
	crp->crp_digest_start = crp->crp_payload_length;

	/*
	 * Duplicate the input iov to append the trailer.  Always
	 * include the full trailer as input to get the record_type
	 * even if only the first byte is used.
	 */
	memcpy(iov, iniov, iovcnt * sizeof(*iov));
	iov[iovcnt].iov_base = trailer;
	iov[iovcnt].iov_len = AES_GMAC_HASH_LEN + 1;
	uio.uio_iov = iov;
	uio.uio_iovcnt = iovcnt + 1;
	uio.uio_offset = 0;
	uio.uio_resid = crp->crp_payload_length + AES_GMAC_HASH_LEN;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_td = curthread;
	crypto_use_uio(crp, &uio);

	if (!inplace) {
		/* Duplicate the output iov to append the trailer. */
		memcpy(out_iov, outiov, iovcnt * sizeof(*out_iov));
		out_iov[iovcnt] = iov[iovcnt];

		out_uio.uio_iov = out_iov;
		out_uio.uio_iovcnt = iovcnt + 1;
		out_uio.uio_offset = 0;
		out_uio.uio_resid = crp->crp_payload_length +
		    AES_GMAC_HASH_LEN;
		out_uio.uio_segflg = UIO_SYSSPACE;
		out_uio.uio_td = curthread;
		crypto_use_output_uio(crp, &out_uio);
	}

	crp->crp_op = CRYPTO_OP_ENCRYPT | CRYPTO_OP_COMPUTE_DIGEST;
	crp->crp_flags = CRYPTO_F_CBIMM | CRYPTO_F_IV_SEPARATE;
	crp->crp_opaque = oo;
	crp->crp_callback = ktls_ocf_callback;

	memcpy(crp->crp_iv, nonce, sizeof(nonce));

	counter_u64_add(ocf_tls13_gcm_crypts, 1);
	if (inplace)
		counter_u64_add(ocf_inplace, 1);
	else
		counter_u64_add(ocf_separate_output, 1);
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
	zfree(os, M_KTLS_OCF);
}

static int
ktls_ocf_try(struct socket *so, struct ktls_session *tls)
{
	struct crypto_session_params csp;
	struct ocf_session *os;
	int error;

	memset(&csp, 0, sizeof(csp));
	csp.csp_flags |= CSP_F_SEPARATE_OUTPUT | CSP_F_SEPARATE_AAD;

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
		ocf_inplace = counter_u64_alloc(M_WAITOK);
		ocf_separate_output = counter_u64_alloc(M_WAITOK);
		ocf_retries = counter_u64_alloc(M_WAITOK);
		return (ktls_crypto_backend_register(&ocf_backend));
	case MOD_UNLOAD:
		error = ktls_crypto_backend_deregister(&ocf_backend);
		if (error)
			return (error);
		counter_u64_free(ocf_tls12_gcm_crypts);
		counter_u64_free(ocf_tls13_gcm_crypts);
		counter_u64_free(ocf_inplace);
		counter_u64_free(ocf_separate_output);
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
