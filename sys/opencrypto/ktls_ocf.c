/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Netflix Inc.
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
	crypto_session_t mac_sid;
	int mac_len;
	struct mtx lock;
	bool implicit_iv;

	/* Only used for TLS 1.0 with the implicit IV. */
#ifdef INVARIANTS
	bool in_progress;
	uint64_t next_seqno;
#endif
	char iv[AES_BLOCK_LEN];
};

struct ocf_operation {
	struct ocf_session *os;
	bool done;
};

static MALLOC_DEFINE(M_KTLS_OCF, "ktls_ocf", "OCF KTLS");

SYSCTL_DECL(_kern_ipc_tls);
SYSCTL_DECL(_kern_ipc_tls_stats);

static SYSCTL_NODE(_kern_ipc_tls_stats, OID_AUTO, ocf,
    CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "Kernel TLS offload via OCF stats");

static COUNTER_U64_DEFINE_EARLY(ocf_tls10_cbc_encrypts);
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats_ocf, OID_AUTO, tls10_cbc_encrypts,
    CTLFLAG_RD, &ocf_tls10_cbc_encrypts,
    "Total number of OCF TLS 1.0 CBC encryption operations");

static COUNTER_U64_DEFINE_EARLY(ocf_tls11_cbc_encrypts);
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats_ocf, OID_AUTO, tls11_cbc_encrypts,
    CTLFLAG_RD, &ocf_tls11_cbc_encrypts,
    "Total number of OCF TLS 1.1/1.2 CBC encryption operations");

static COUNTER_U64_DEFINE_EARLY(ocf_tls12_gcm_decrypts);
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats_ocf, OID_AUTO, tls12_gcm_decrypts,
    CTLFLAG_RD, &ocf_tls12_gcm_decrypts,
    "Total number of OCF TLS 1.2 GCM decryption operations");

static COUNTER_U64_DEFINE_EARLY(ocf_tls12_gcm_encrypts);
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats_ocf, OID_AUTO, tls12_gcm_encrypts,
    CTLFLAG_RD, &ocf_tls12_gcm_encrypts,
    "Total number of OCF TLS 1.2 GCM encryption operations");

static COUNTER_U64_DEFINE_EARLY(ocf_tls12_chacha20_decrypts);
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats_ocf, OID_AUTO, tls12_chacha20_decrypts,
    CTLFLAG_RD, &ocf_tls12_chacha20_decrypts,
    "Total number of OCF TLS 1.2 Chacha20-Poly1305 decryption operations");

static COUNTER_U64_DEFINE_EARLY(ocf_tls12_chacha20_encrypts);
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats_ocf, OID_AUTO, tls12_chacha20_encrypts,
    CTLFLAG_RD, &ocf_tls12_chacha20_encrypts,
    "Total number of OCF TLS 1.2 Chacha20-Poly1305 encryption operations");

static COUNTER_U64_DEFINE_EARLY(ocf_tls13_gcm_encrypts);
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats_ocf, OID_AUTO, tls13_gcm_encrypts,
    CTLFLAG_RD, &ocf_tls13_gcm_encrypts,
    "Total number of OCF TLS 1.3 GCM encryption operations");

static COUNTER_U64_DEFINE_EARLY(ocf_tls13_chacha20_encrypts);
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats_ocf, OID_AUTO, tls13_chacha20_encrypts,
    CTLFLAG_RD, &ocf_tls13_chacha20_encrypts,
    "Total number of OCF TLS 1.3 Chacha20-Poly1305 encryption operations");

static COUNTER_U64_DEFINE_EARLY(ocf_inplace);
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats_ocf, OID_AUTO, inplace,
    CTLFLAG_RD, &ocf_inplace,
    "Total number of OCF in-place operations");

static COUNTER_U64_DEFINE_EARLY(ocf_separate_output);
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats_ocf, OID_AUTO, separate_output,
    CTLFLAG_RD, &ocf_separate_output,
    "Total number of OCF operations with a separate output buffer");

static COUNTER_U64_DEFINE_EARLY(ocf_retries);
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
ktls_ocf_dispatch(struct ocf_session *os, struct cryptop *crp)
{
	struct ocf_operation oo;
	int error;

	oo.os = os;
	oo.done = false;

	crp->crp_opaque = &oo;
	crp->crp_callback = ktls_ocf_callback;
	for (;;) {
		error = crypto_dispatch(crp);
		if (error)
			break;

		mtx_lock(&os->lock);
		while (!oo.done)
			mtx_sleep(&oo, &os->lock, 0, "ocfktls", 0);
		mtx_unlock(&os->lock);

		if (crp->crp_etype != EAGAIN) {
			error = crp->crp_etype;
			break;
		}

		crp->crp_etype = 0;
		crp->crp_flags &= ~CRYPTO_F_DONE;
		oo.done = false;
		counter_u64_add(ocf_retries, 1);
	}
	return (error);
}

static int
ktls_ocf_tls_cbc_encrypt(struct ktls_session *tls,
    const struct tls_record_layer *hdr, uint8_t *trailer, struct iovec *iniov,
    struct iovec *outiov, int iovcnt, uint64_t seqno,
    uint8_t record_type __unused)
{
	struct uio uio, out_uio;
	struct tls_mac_data ad;
	struct cryptop crp;
	struct ocf_session *os;
	struct iovec iov[iovcnt + 2];
	struct iovec out_iov[iovcnt + 1];
	int i, error;
	uint16_t tls_comp_len;
	uint8_t pad;
	bool inplace;

	os = tls->cipher;

#ifdef INVARIANTS
	if (os->implicit_iv) {
		mtx_lock(&os->lock);
		KASSERT(!os->in_progress,
		    ("concurrent implicit IV encryptions"));
		if (os->next_seqno != seqno) {
			printf("KTLS CBC: TLS records out of order.  "
			    "Expected %ju, got %ju\n",
			    (uintmax_t)os->next_seqno, (uintmax_t)seqno);
			mtx_unlock(&os->lock);
			return (EINVAL);
		}
		os->in_progress = true;
		mtx_unlock(&os->lock);
	}
#endif

	/*
	 * Compute the payload length.
	 *
	 * XXX: This could be easily computed O(1) from the mbuf
	 * fields, but we don't have those accessible here.  Can
	 * at least compute inplace as well while we are here.
	 */
	tls_comp_len = 0;
	inplace = true;
	for (i = 0; i < iovcnt; i++) {
		tls_comp_len += iniov[i].iov_len;
		if (iniov[i].iov_base != outiov[i].iov_base)
			inplace = false;
	}

	/* Initialize the AAD. */
	ad.seq = htobe64(seqno);
	ad.type = hdr->tls_type;
	ad.tls_vmajor = hdr->tls_vmajor;
	ad.tls_vminor = hdr->tls_vminor;
	ad.tls_length = htons(tls_comp_len);

	/* First, compute the MAC. */
	iov[0].iov_base = &ad;
	iov[0].iov_len = sizeof(ad);
	memcpy(&iov[1], iniov, sizeof(*iniov) * iovcnt);
	iov[iovcnt + 1].iov_base = trailer;
	iov[iovcnt + 1].iov_len = os->mac_len;
	uio.uio_iov = iov;
	uio.uio_iovcnt = iovcnt + 2;
	uio.uio_offset = 0;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_td = curthread;
	uio.uio_resid = sizeof(ad) + tls_comp_len + os->mac_len;

	crypto_initreq(&crp, os->mac_sid);
	crp.crp_payload_start = 0;
	crp.crp_payload_length = sizeof(ad) + tls_comp_len;
	crp.crp_digest_start = crp.crp_payload_length;
	crp.crp_op = CRYPTO_OP_COMPUTE_DIGEST;
	crp.crp_flags = CRYPTO_F_CBIMM;
	crypto_use_uio(&crp, &uio);
	error = ktls_ocf_dispatch(os, &crp);

	crypto_destroyreq(&crp);
	if (error) {
#ifdef INVARIANTS
		if (os->implicit_iv) {
			mtx_lock(&os->lock);
			os->in_progress = false;
			mtx_unlock(&os->lock);
		}
#endif
		return (error);
	}

	/* Second, add the padding. */
	pad = (unsigned)(AES_BLOCK_LEN - (tls_comp_len + os->mac_len + 1)) %
	    AES_BLOCK_LEN;
	for (i = 0; i < pad + 1; i++)
		trailer[os->mac_len + i] = pad;

	/* Finally, encrypt the record. */

	/*
	 * Don't recopy the input iovec, instead just adjust the
	 * trailer length and skip over the AAD vector in the uio.
	 */
	iov[iovcnt + 1].iov_len += pad + 1;
	uio.uio_iov = iov + 1;
	uio.uio_iovcnt = iovcnt + 1;
	uio.uio_resid = tls_comp_len + iov[iovcnt + 1].iov_len;
	KASSERT(uio.uio_resid % AES_BLOCK_LEN == 0,
	    ("invalid encryption size"));

	crypto_initreq(&crp, os->sid);
	crp.crp_payload_start = 0;
	crp.crp_payload_length = uio.uio_resid;
	crp.crp_op = CRYPTO_OP_ENCRYPT;
	crp.crp_flags = CRYPTO_F_CBIMM | CRYPTO_F_IV_SEPARATE;
	if (os->implicit_iv)
		memcpy(crp.crp_iv, os->iv, AES_BLOCK_LEN);
	else
		memcpy(crp.crp_iv, hdr + 1, AES_BLOCK_LEN);
	crypto_use_uio(&crp, &uio);
	if (!inplace) {
		memcpy(out_iov, outiov, sizeof(*iniov) * iovcnt);
		out_iov[iovcnt] = iov[iovcnt + 1];
		out_uio.uio_iov = out_iov;
		out_uio.uio_iovcnt = iovcnt + 1;
		out_uio.uio_offset = 0;
		out_uio.uio_segflg = UIO_SYSSPACE;
		out_uio.uio_td = curthread;
		out_uio.uio_resid = uio.uio_resid;
		crypto_use_output_uio(&crp, &out_uio);
	}

	if (os->implicit_iv)
		counter_u64_add(ocf_tls10_cbc_encrypts, 1);
	else
		counter_u64_add(ocf_tls11_cbc_encrypts, 1);
	if (inplace)
		counter_u64_add(ocf_inplace, 1);
	else
		counter_u64_add(ocf_separate_output, 1);
	error = ktls_ocf_dispatch(os, &crp);

	crypto_destroyreq(&crp);

	if (os->implicit_iv) {
		KASSERT(os->mac_len + pad + 1 >= AES_BLOCK_LEN,
		    ("trailer too short to read IV"));
		memcpy(os->iv, trailer + os->mac_len + pad + 1 - AES_BLOCK_LEN,
		    AES_BLOCK_LEN);
#ifdef INVARIANTS
		mtx_lock(&os->lock);
		os->next_seqno = seqno + 1;
		os->in_progress = false;
		mtx_unlock(&os->lock);
#endif
	}
	return (error);
}

static int
ktls_ocf_tls12_aead_encrypt(struct ktls_session *tls,
    const struct tls_record_layer *hdr, uint8_t *trailer, struct iovec *iniov,
    struct iovec *outiov, int iovcnt, uint64_t seqno,
    uint8_t record_type __unused)
{
	struct uio uio, out_uio, *tag_uio;
	struct tls_aead_data ad;
	struct cryptop crp;
	struct ocf_session *os;
	struct iovec iov[iovcnt + 1];
	int i, error;
	uint16_t tls_comp_len;
	bool inplace;

	os = tls->cipher;

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

	crypto_initreq(&crp, os->sid);

	/* Setup the IV. */
	if (tls->params.cipher_algorithm == CRYPTO_AES_NIST_GCM_16) {
		memcpy(crp.crp_iv, tls->params.iv, TLS_AEAD_GCM_LEN);
		memcpy(crp.crp_iv + TLS_AEAD_GCM_LEN, hdr + 1,
		    sizeof(uint64_t));
	} else {
		/*
		 * Chacha20-Poly1305 constructs the IV for TLS 1.2
		 * identically to constructing the IV for AEAD in TLS
		 * 1.3.
		 */
		memcpy(crp.crp_iv, tls->params.iv, tls->params.iv_len);
		*(uint64_t *)(crp.crp_iv + 4) ^= htobe64(seqno);
	}

	/* Setup the AAD. */
	if (tls->params.cipher_algorithm == CRYPTO_AES_NIST_GCM_16)
		tls_comp_len = ntohs(hdr->tls_length) -
		    (AES_GMAC_HASH_LEN + sizeof(uint64_t));
	else
		tls_comp_len = ntohs(hdr->tls_length) - POLY1305_HASH_LEN;
	ad.seq = htobe64(seqno);
	ad.type = hdr->tls_type;
	ad.tls_vmajor = hdr->tls_vmajor;
	ad.tls_vminor = hdr->tls_vminor;
	ad.tls_length = htons(tls_comp_len);
	crp.crp_aad = &ad;
	crp.crp_aad_length = sizeof(ad);

	/* Compute payload length and determine if encryption is in place. */
	inplace = true;
	crp.crp_payload_start = 0;
	for (i = 0; i < iovcnt; i++) {
		if (iniov[i].iov_base != outiov[i].iov_base)
			inplace = false;
		crp.crp_payload_length += iniov[i].iov_len;
	}
	uio.uio_resid = crp.crp_payload_length;
	out_uio.uio_resid = crp.crp_payload_length;

	if (inplace)
		tag_uio = &uio;
	else
		tag_uio = &out_uio;

	/* Duplicate iovec and append vector for tag. */
	memcpy(iov, tag_uio->uio_iov, iovcnt * sizeof(struct iovec));
	iov[iovcnt].iov_base = trailer;
	iov[iovcnt].iov_len = tls->params.tls_tlen;
	tag_uio->uio_iov = iov;
	tag_uio->uio_iovcnt++;
	crp.crp_digest_start = tag_uio->uio_resid;
	tag_uio->uio_resid += tls->params.tls_tlen;

	crp.crp_op = CRYPTO_OP_ENCRYPT | CRYPTO_OP_COMPUTE_DIGEST;
	crp.crp_flags = CRYPTO_F_CBIMM | CRYPTO_F_IV_SEPARATE;
	crypto_use_uio(&crp, &uio);
	if (!inplace)
		crypto_use_output_uio(&crp, &out_uio);

	if (tls->params.cipher_algorithm == CRYPTO_AES_NIST_GCM_16)
		counter_u64_add(ocf_tls12_gcm_encrypts, 1);
	else
		counter_u64_add(ocf_tls12_chacha20_encrypts, 1);
	if (inplace)
		counter_u64_add(ocf_inplace, 1);
	else
		counter_u64_add(ocf_separate_output, 1);
	error = ktls_ocf_dispatch(os, &crp);

	crypto_destroyreq(&crp);
	return (error);
}

static int
ktls_ocf_tls12_aead_decrypt(struct ktls_session *tls,
    const struct tls_record_layer *hdr, struct mbuf *m, uint64_t seqno,
    int *trailer_len)
{
	struct tls_aead_data ad;
	struct cryptop crp;
	struct ocf_session *os;
	struct ocf_operation oo;
	int error;
	uint16_t tls_comp_len;

	os = tls->cipher;

	oo.os = os;
	oo.done = false;

	crypto_initreq(&crp, os->sid);

	/* Setup the IV. */
	if (tls->params.cipher_algorithm == CRYPTO_AES_NIST_GCM_16) {
		memcpy(crp.crp_iv, tls->params.iv, TLS_AEAD_GCM_LEN);
		memcpy(crp.crp_iv + TLS_AEAD_GCM_LEN, hdr + 1,
		    sizeof(uint64_t));
	} else {
		/*
		 * Chacha20-Poly1305 constructs the IV for TLS 1.2
		 * identically to constructing the IV for AEAD in TLS
		 * 1.3.
		 */
		memcpy(crp.crp_iv, tls->params.iv, tls->params.iv_len);
		*(uint64_t *)(crp.crp_iv + 4) ^= htobe64(seqno);
	}

	/* Setup the AAD. */
	if (tls->params.cipher_algorithm == CRYPTO_AES_NIST_GCM_16)
		tls_comp_len = ntohs(hdr->tls_length) -
		    (AES_GMAC_HASH_LEN + sizeof(uint64_t));
	else
		tls_comp_len = ntohs(hdr->tls_length) - POLY1305_HASH_LEN;
	ad.seq = htobe64(seqno);
	ad.type = hdr->tls_type;
	ad.tls_vmajor = hdr->tls_vmajor;
	ad.tls_vminor = hdr->tls_vminor;
	ad.tls_length = htons(tls_comp_len);
	crp.crp_aad = &ad;
	crp.crp_aad_length = sizeof(ad);

	crp.crp_payload_start = tls->params.tls_hlen;
	crp.crp_payload_length = tls_comp_len;
	crp.crp_digest_start = crp.crp_payload_start + crp.crp_payload_length;

	crp.crp_op = CRYPTO_OP_DECRYPT | CRYPTO_OP_VERIFY_DIGEST;
	crp.crp_flags = CRYPTO_F_CBIMM | CRYPTO_F_IV_SEPARATE;
	crypto_use_mbuf(&crp, m);

	if (tls->params.cipher_algorithm == CRYPTO_AES_NIST_GCM_16)
		counter_u64_add(ocf_tls12_gcm_decrypts, 1);
	else
		counter_u64_add(ocf_tls12_chacha20_decrypts, 1);
	error = ktls_ocf_dispatch(os, &crp);

	crypto_destroyreq(&crp);
	*trailer_len = tls->params.tls_tlen;
	return (error);
}

static int
ktls_ocf_tls13_aead_encrypt(struct ktls_session *tls,
    const struct tls_record_layer *hdr, uint8_t *trailer, struct iovec *iniov,
    struct iovec *outiov, int iovcnt, uint64_t seqno, uint8_t record_type)
{
	struct uio uio, out_uio;
	struct tls_aead_data_13 ad;
	char nonce[12];
	struct cryptop crp;
	struct ocf_session *os;
	struct iovec iov[iovcnt + 1], out_iov[iovcnt + 1];
	int i, error;
	bool inplace;

	os = tls->cipher;

	crypto_initreq(&crp, os->sid);

	/* Setup the nonce. */
	memcpy(nonce, tls->params.iv, tls->params.iv_len);
	*(uint64_t *)(nonce + 4) ^= htobe64(seqno);

	/* Setup the AAD. */
	ad.type = hdr->tls_type;
	ad.tls_vmajor = hdr->tls_vmajor;
	ad.tls_vminor = hdr->tls_vminor;
	ad.tls_length = hdr->tls_length;
	crp.crp_aad = &ad;
	crp.crp_aad_length = sizeof(ad);

	/* Compute payload length and determine if encryption is in place. */
	inplace = true;
	crp.crp_payload_start = 0;
	for (i = 0; i < iovcnt; i++) {
		if (iniov[i].iov_base != outiov[i].iov_base)
			inplace = false;
		crp.crp_payload_length += iniov[i].iov_len;
	}

	/* Store the record type as the first byte of the trailer. */
	trailer[0] = record_type;
	crp.crp_payload_length++;
	crp.crp_digest_start = crp.crp_payload_length;

	/*
	 * Duplicate the input iov to append the trailer.  Always
	 * include the full trailer as input to get the record_type
	 * even if only the first byte is used.
	 */
	memcpy(iov, iniov, iovcnt * sizeof(*iov));
	iov[iovcnt].iov_base = trailer;
	iov[iovcnt].iov_len = tls->params.tls_tlen;
	uio.uio_iov = iov;
	uio.uio_iovcnt = iovcnt + 1;
	uio.uio_offset = 0;
	uio.uio_resid = crp.crp_payload_length + tls->params.tls_tlen - 1;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_td = curthread;
	crypto_use_uio(&crp, &uio);

	if (!inplace) {
		/* Duplicate the output iov to append the trailer. */
		memcpy(out_iov, outiov, iovcnt * sizeof(*out_iov));
		out_iov[iovcnt] = iov[iovcnt];

		out_uio.uio_iov = out_iov;
		out_uio.uio_iovcnt = iovcnt + 1;
		out_uio.uio_offset = 0;
		out_uio.uio_resid = crp.crp_payload_length +
		    tls->params.tls_tlen - 1;
		out_uio.uio_segflg = UIO_SYSSPACE;
		out_uio.uio_td = curthread;
		crypto_use_output_uio(&crp, &out_uio);
	}

	crp.crp_op = CRYPTO_OP_ENCRYPT | CRYPTO_OP_COMPUTE_DIGEST;
	crp.crp_flags = CRYPTO_F_CBIMM | CRYPTO_F_IV_SEPARATE;

	memcpy(crp.crp_iv, nonce, sizeof(nonce));

	if (tls->params.cipher_algorithm == CRYPTO_AES_NIST_GCM_16)
		counter_u64_add(ocf_tls13_gcm_encrypts, 1);
	else
		counter_u64_add(ocf_tls13_chacha20_encrypts, 1);
	if (inplace)
		counter_u64_add(ocf_inplace, 1);
	else
		counter_u64_add(ocf_separate_output, 1);
	error = ktls_ocf_dispatch(os, &crp);

	crypto_destroyreq(&crp);
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
ktls_ocf_try(struct socket *so, struct ktls_session *tls, int direction)
{
	struct crypto_session_params csp, mac_csp;
	struct ocf_session *os;
	int error, mac_len;

	memset(&csp, 0, sizeof(csp));
	memset(&mac_csp, 0, sizeof(mac_csp));
	mac_csp.csp_mode = CSP_MODE_NONE;
	mac_len = 0;

	switch (tls->params.cipher_algorithm) {
	case CRYPTO_AES_NIST_GCM_16:
		switch (tls->params.cipher_key_len) {
		case 128 / 8:
		case 256 / 8:
			break;
		default:
			return (EINVAL);
		}

		/* Only TLS 1.2 and 1.3 are supported. */
		if (tls->params.tls_vmajor != TLS_MAJOR_VER_ONE ||
		    tls->params.tls_vminor < TLS_MINOR_VER_TWO ||
		    tls->params.tls_vminor > TLS_MINOR_VER_THREE)
			return (EPROTONOSUPPORT);

		/* TLS 1.3 is not yet supported for receive. */
		if (direction == KTLS_RX &&
		    tls->params.tls_vminor == TLS_MINOR_VER_THREE)
			return (EPROTONOSUPPORT);

		csp.csp_flags |= CSP_F_SEPARATE_OUTPUT | CSP_F_SEPARATE_AAD;
		csp.csp_mode = CSP_MODE_AEAD;
		csp.csp_cipher_alg = CRYPTO_AES_NIST_GCM_16;
		csp.csp_cipher_key = tls->params.cipher_key;
		csp.csp_cipher_klen = tls->params.cipher_key_len;
		csp.csp_ivlen = AES_GCM_IV_LEN;
		break;
	case CRYPTO_AES_CBC:
		switch (tls->params.cipher_key_len) {
		case 128 / 8:
		case 256 / 8:
			break;
		default:
			return (EINVAL);
		}

		switch (tls->params.auth_algorithm) {
		case CRYPTO_SHA1_HMAC:
			mac_len = SHA1_HASH_LEN;
			break;
		case CRYPTO_SHA2_256_HMAC:
			mac_len = SHA2_256_HASH_LEN;
			break;
		case CRYPTO_SHA2_384_HMAC:
			mac_len = SHA2_384_HASH_LEN;
			break;
		default:
			return (EINVAL);
		}

		/* Only TLS 1.0-1.2 are supported. */
		if (tls->params.tls_vmajor != TLS_MAJOR_VER_ONE ||
		    tls->params.tls_vminor < TLS_MINOR_VER_ZERO ||
		    tls->params.tls_vminor > TLS_MINOR_VER_TWO)
			return (EPROTONOSUPPORT);

		/* AES-CBC is not supported for receive. */
		if (direction == KTLS_RX)
			return (EPROTONOSUPPORT);

		csp.csp_flags |= CSP_F_SEPARATE_OUTPUT;
		csp.csp_mode = CSP_MODE_CIPHER;
		csp.csp_cipher_alg = CRYPTO_AES_CBC;
		csp.csp_cipher_key = tls->params.cipher_key;
		csp.csp_cipher_klen = tls->params.cipher_key_len;
		csp.csp_ivlen = AES_BLOCK_LEN;

		mac_csp.csp_flags |= CSP_F_SEPARATE_OUTPUT;
		mac_csp.csp_mode = CSP_MODE_DIGEST;
		mac_csp.csp_auth_alg = tls->params.auth_algorithm;
		mac_csp.csp_auth_key = tls->params.auth_key;
		mac_csp.csp_auth_klen = tls->params.auth_key_len;
		break;
	case CRYPTO_CHACHA20_POLY1305:
		switch (tls->params.cipher_key_len) {
		case 256 / 8:
			break;
		default:
			return (EINVAL);
		}

		/* Only TLS 1.2 and 1.3 are supported. */
		if (tls->params.tls_vmajor != TLS_MAJOR_VER_ONE ||
		    tls->params.tls_vminor < TLS_MINOR_VER_TWO ||
		    tls->params.tls_vminor > TLS_MINOR_VER_THREE)
			return (EPROTONOSUPPORT);

		/* TLS 1.3 is not yet supported for receive. */
		if (direction == KTLS_RX &&
		    tls->params.tls_vminor == TLS_MINOR_VER_THREE)
			return (EPROTONOSUPPORT);

		csp.csp_flags |= CSP_F_SEPARATE_OUTPUT | CSP_F_SEPARATE_AAD;
		csp.csp_mode = CSP_MODE_AEAD;
		csp.csp_cipher_alg = CRYPTO_CHACHA20_POLY1305;
		csp.csp_cipher_key = tls->params.cipher_key;
		csp.csp_cipher_klen = tls->params.cipher_key_len;
		csp.csp_ivlen = CHACHA20_POLY1305_IV_LEN;
		break;
	default:
		return (EPROTONOSUPPORT);
	}

	os = malloc(sizeof(*os), M_KTLS_OCF, M_NOWAIT | M_ZERO);
	if (os == NULL)
		return (ENOMEM);

	error = crypto_newsession(&os->sid, &csp,
	    CRYPTO_FLAG_HARDWARE | CRYPTO_FLAG_SOFTWARE);
	if (error) {
		free(os, M_KTLS_OCF);
		return (error);
	}

	if (mac_csp.csp_mode != CSP_MODE_NONE) {
		error = crypto_newsession(&os->mac_sid, &mac_csp,
		    CRYPTO_FLAG_HARDWARE | CRYPTO_FLAG_SOFTWARE);
		if (error) {
			crypto_freesession(os->sid);
			free(os, M_KTLS_OCF);
			return (error);
		}
		os->mac_len = mac_len;
	}

	mtx_init(&os->lock, "ktls_ocf", NULL, MTX_DEF);
	tls->cipher = os;
	if (tls->params.cipher_algorithm == CRYPTO_AES_NIST_GCM_16 ||
	    tls->params.cipher_algorithm == CRYPTO_CHACHA20_POLY1305) {
		if (direction == KTLS_TX) {
			if (tls->params.tls_vminor == TLS_MINOR_VER_THREE)
				tls->sw_encrypt = ktls_ocf_tls13_aead_encrypt;
			else
				tls->sw_encrypt = ktls_ocf_tls12_aead_encrypt;
		} else {
			tls->sw_decrypt = ktls_ocf_tls12_aead_decrypt;
		}
	} else {
		tls->sw_encrypt = ktls_ocf_tls_cbc_encrypt;
		if (tls->params.tls_vminor == TLS_MINOR_VER_ZERO) {
			os->implicit_iv = true;
			memcpy(os->iv, tls->params.iv, AES_BLOCK_LEN);
#ifdef INVARIANTS
			os->next_seqno = tls->next_seqno;
#endif
		}
	}
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
	switch (what) {
	case MOD_LOAD:
		return (ktls_crypto_backend_register(&ocf_backend));
	case MOD_UNLOAD:
		return (ktls_crypto_backend_deregister(&ocf_backend));
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
