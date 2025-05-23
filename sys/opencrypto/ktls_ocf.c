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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/counter.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/ktls.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>
#include <netinet/in.h>
#include <opencrypto/cryptodev.h>
#include <opencrypto/ktls.h>

struct ktls_ocf_sw {
	/* Encrypt a single outbound TLS record. */
	int	(*encrypt)(struct ktls_ocf_encrypt_state *state,
	    struct ktls_session *tls, struct mbuf *m,
	    struct iovec *outiov, int outiovcnt);

	/* Re-encrypt a received TLS record that is partially decrypted. */
	int	(*recrypt)(struct ktls_session *tls,
	    const struct tls_record_layer *hdr, struct mbuf *m,
	    uint64_t seqno);

	/* Decrypt a received TLS record. */
	int	(*decrypt)(struct ktls_session *tls,
	    const struct tls_record_layer *hdr, struct mbuf *m,
	    uint64_t seqno, int *trailer_len);
};

struct ktls_ocf_session {
	const struct ktls_ocf_sw *sw;
	crypto_session_t sid;
	crypto_session_t mac_sid;
	crypto_session_t recrypt_sid;
	struct mtx lock;
	int mac_len;
	bool implicit_iv;

	/* Only used for TLS 1.0 with the implicit IV. */
#ifdef INVARIANTS
	bool in_progress;
	uint64_t next_seqno;
#endif
	char iv[AES_BLOCK_LEN];
};

struct ocf_operation {
	struct ktls_ocf_session *os;
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

static COUNTER_U64_DEFINE_EARLY(ocf_tls11_cbc_decrypts);
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats_ocf, OID_AUTO, tls11_cbc_decrypts,
    CTLFLAG_RD, &ocf_tls11_cbc_decrypts,
    "Total number of OCF TLS 1.1/1.2 CBC decryption operations");

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

static COUNTER_U64_DEFINE_EARLY(ocf_tls12_gcm_recrypts);
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats_ocf, OID_AUTO, tls12_gcm_recrypts,
    CTLFLAG_RD, &ocf_tls12_gcm_recrypts,
    "Total number of OCF TLS 1.2 GCM re-encryption operations");

static COUNTER_U64_DEFINE_EARLY(ocf_tls12_chacha20_decrypts);
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats_ocf, OID_AUTO, tls12_chacha20_decrypts,
    CTLFLAG_RD, &ocf_tls12_chacha20_decrypts,
    "Total number of OCF TLS 1.2 Chacha20-Poly1305 decryption operations");

static COUNTER_U64_DEFINE_EARLY(ocf_tls12_chacha20_encrypts);
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats_ocf, OID_AUTO, tls12_chacha20_encrypts,
    CTLFLAG_RD, &ocf_tls12_chacha20_encrypts,
    "Total number of OCF TLS 1.2 Chacha20-Poly1305 encryption operations");

static COUNTER_U64_DEFINE_EARLY(ocf_tls13_gcm_decrypts);
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats_ocf, OID_AUTO, tls13_gcm_decrypts,
    CTLFLAG_RD, &ocf_tls13_gcm_decrypts,
    "Total number of OCF TLS 1.3 GCM decryption operations");

static COUNTER_U64_DEFINE_EARLY(ocf_tls13_gcm_encrypts);
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats_ocf, OID_AUTO, tls13_gcm_encrypts,
    CTLFLAG_RD, &ocf_tls13_gcm_encrypts,
    "Total number of OCF TLS 1.3 GCM encryption operations");

static COUNTER_U64_DEFINE_EARLY(ocf_tls13_gcm_recrypts);
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats_ocf, OID_AUTO, tls13_gcm_recrypts,
    CTLFLAG_RD, &ocf_tls13_gcm_recrypts,
    "Total number of OCF TLS 1.3 GCM re-encryption operations");

static COUNTER_U64_DEFINE_EARLY(ocf_tls13_chacha20_decrypts);
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats_ocf, OID_AUTO, tls13_chacha20_decrypts,
    CTLFLAG_RD, &ocf_tls13_chacha20_decrypts,
    "Total number of OCF TLS 1.3 Chacha20-Poly1305 decryption operations");

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
ktls_ocf_callback_sync(struct cryptop *crp __unused)
{
	return (0);
}

static int
ktls_ocf_callback_async(struct cryptop *crp)
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
ktls_ocf_dispatch(struct ktls_ocf_session *os, struct cryptop *crp)
{
	struct ocf_operation oo;
	int error;
	bool async;

	oo.os = os;
	oo.done = false;

	crp->crp_opaque = &oo;
	for (;;) {
		async = !CRYPTO_SESS_SYNC(crp->crp_session);
		crp->crp_callback = async ? ktls_ocf_callback_async :
		    ktls_ocf_callback_sync;

		error = crypto_dispatch(crp);
		if (error)
			break;
		if (async) {
			mtx_lock(&os->lock);
			while (!oo.done)
				mtx_sleep(&oo, &os->lock, 0, "ocfktls", 0);
			mtx_unlock(&os->lock);
		}

		if (crp->crp_etype != EAGAIN) {
			error = crp->crp_etype;
			break;
		}

		crp->crp_etype = 0;
		oo.done = false;
		counter_u64_add(ocf_retries, 1);
	}
	return (error);
}

static int
ktls_ocf_dispatch_async_cb(struct cryptop *crp)
{
	struct ktls_ocf_encrypt_state *state;
	int error;

	state = crp->crp_opaque;
	if (crp->crp_etype == EAGAIN) {
		crp->crp_etype = 0;
		counter_u64_add(ocf_retries, 1);
		error = crypto_dispatch(crp);
		if (error != 0) {
			crypto_destroyreq(crp);
			ktls_encrypt_cb(state, error);
		}
		return (0);
	}

	error = crp->crp_etype;
	crypto_destroyreq(crp);
	ktls_encrypt_cb(state, error);
	return (0);
}

static int
ktls_ocf_dispatch_async(struct ktls_ocf_encrypt_state *state,
    struct cryptop *crp)
{
	int error;

	crp->crp_opaque = state;
	crp->crp_callback = ktls_ocf_dispatch_async_cb;
	error = crypto_dispatch(crp);
	if (error != 0)
		crypto_destroyreq(crp);
	return (error);
}

static int
ktls_ocf_tls_cbc_encrypt(struct ktls_ocf_encrypt_state *state,
    struct ktls_session *tls, struct mbuf *m, struct iovec *outiov,
    int outiovcnt)
{
	const struct tls_record_layer *hdr;
	struct uio *uio;
	struct tls_mac_data *ad;
	struct cryptop *crp;
	struct ktls_ocf_session *os;
	struct iovec iov[m->m_epg_npgs + 2];
	u_int pgoff;
	int i, error;
	uint16_t tls_comp_len;
	uint8_t pad;

	MPASS(outiovcnt + 1 <= nitems(iov));

	os = tls->ocf_session;
	hdr = (const struct tls_record_layer *)m->m_epg_hdr;
	crp = &state->crp;
	uio = &state->uio;
	MPASS(tls->sync_dispatch);

#ifdef INVARIANTS
	if (os->implicit_iv) {
		mtx_lock(&os->lock);
		KASSERT(!os->in_progress,
		    ("concurrent implicit IV encryptions"));
		if (os->next_seqno != m->m_epg_seqno) {
			printf("KTLS CBC: TLS records out of order.  "
			    "Expected %ju, got %ju\n",
			    (uintmax_t)os->next_seqno,
			    (uintmax_t)m->m_epg_seqno);
			mtx_unlock(&os->lock);
			return (EINVAL);
		}
		os->in_progress = true;
		mtx_unlock(&os->lock);
	}
#endif

	/* Payload length. */
	tls_comp_len = m->m_len - (m->m_epg_hdrlen + m->m_epg_trllen);

	/* Initialize the AAD. */
	ad = &state->mac;
	ad->seq = htobe64(m->m_epg_seqno);
	ad->type = hdr->tls_type;
	ad->tls_vmajor = hdr->tls_vmajor;
	ad->tls_vminor = hdr->tls_vminor;
	ad->tls_length = htons(tls_comp_len);

	/* First, compute the MAC. */
	iov[0].iov_base = ad;
	iov[0].iov_len = sizeof(*ad);
	pgoff = m->m_epg_1st_off;
	for (i = 0; i < m->m_epg_npgs; i++, pgoff = 0) {
		iov[i + 1].iov_base = (void *)PHYS_TO_DMAP(m->m_epg_pa[i] +
		    pgoff);
		iov[i + 1].iov_len = m_epg_pagelen(m, i, pgoff);
	}
	iov[m->m_epg_npgs + 1].iov_base = m->m_epg_trail;
	iov[m->m_epg_npgs + 1].iov_len = os->mac_len;
	uio->uio_iov = iov;
	uio->uio_iovcnt = m->m_epg_npgs + 2;
	uio->uio_offset = 0;
	uio->uio_segflg = UIO_SYSSPACE;
	uio->uio_td = curthread;
	uio->uio_resid = sizeof(*ad) + tls_comp_len + os->mac_len;

	crypto_initreq(crp, os->mac_sid);
	crp->crp_payload_start = 0;
	crp->crp_payload_length = sizeof(*ad) + tls_comp_len;
	crp->crp_digest_start = crp->crp_payload_length;
	crp->crp_op = CRYPTO_OP_COMPUTE_DIGEST;
	crp->crp_flags = CRYPTO_F_CBIMM;
	crypto_use_uio(crp, uio);
	error = ktls_ocf_dispatch(os, crp);

	crypto_destroyreq(crp);
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
	pad = m->m_epg_trllen - os->mac_len - 1;
	for (i = 0; i < pad + 1; i++)
		m->m_epg_trail[os->mac_len + i] = pad;

	/* Finally, encrypt the record. */
	crypto_initreq(crp, os->sid);
	crp->crp_payload_start = m->m_epg_hdrlen;
	crp->crp_payload_length = tls_comp_len + m->m_epg_trllen;
	KASSERT(crp->crp_payload_length % AES_BLOCK_LEN == 0,
	    ("invalid encryption size"));
	crypto_use_single_mbuf(crp, m);
	crp->crp_op = CRYPTO_OP_ENCRYPT;
	crp->crp_flags = CRYPTO_F_CBIMM | CRYPTO_F_IV_SEPARATE;
	if (os->implicit_iv)
		memcpy(crp->crp_iv, os->iv, AES_BLOCK_LEN);
	else
		memcpy(crp->crp_iv, hdr + 1, AES_BLOCK_LEN);

	if (outiov != NULL) {
		uio->uio_iov = outiov;
		uio->uio_iovcnt = outiovcnt;
		uio->uio_offset = 0;
		uio->uio_segflg = UIO_SYSSPACE;
		uio->uio_td = curthread;
		uio->uio_resid = crp->crp_payload_length;
		crypto_use_output_uio(crp, uio);
	}

	if (os->implicit_iv)
		counter_u64_add(ocf_tls10_cbc_encrypts, 1);
	else
		counter_u64_add(ocf_tls11_cbc_encrypts, 1);
	if (outiov != NULL)
		counter_u64_add(ocf_separate_output, 1);
	else
		counter_u64_add(ocf_inplace, 1);
	error = ktls_ocf_dispatch(os, crp);

	crypto_destroyreq(crp);

	if (os->implicit_iv) {
		KASSERT(os->mac_len + pad + 1 >= AES_BLOCK_LEN,
		    ("trailer too short to read IV"));
		memcpy(os->iv, m->m_epg_trail + m->m_epg_trllen - AES_BLOCK_LEN,
		    AES_BLOCK_LEN);
#ifdef INVARIANTS
		mtx_lock(&os->lock);
		os->next_seqno = m->m_epg_seqno + 1;
		os->in_progress = false;
		mtx_unlock(&os->lock);
#endif
	}
	return (error);
}

static int
check_padding(void *arg, void *data, u_int len)
{
	uint8_t pad = *(uint8_t *)arg;
	const char *cp = data;

	while (len > 0) {
		if (*cp != pad)
			return (EBADMSG);
		cp++;
		len--;
	}
	return (0);
}

static int
ktls_ocf_tls_cbc_decrypt(struct ktls_session *tls,
    const struct tls_record_layer *hdr, struct mbuf *m, uint64_t seqno,
    int *trailer_len)
{
	struct tls_mac_data ad;
	struct cryptop crp;
	struct uio uio;
	struct ktls_ocf_session *os;
	struct iovec *iov;
	struct mbuf *n;
	u_int iovcnt;
	int i, error, skip;
	uint16_t tls_len, tls_comp_len;
	uint8_t pad;

	os = tls->ocf_session;

	/*
	 * Ensure record is a multiple of the cipher block size and
	 * contains at least an explicit IV, MAC, and at least one
	 * padding byte.
	 */
	tls_len = ntohs(hdr->tls_length);
	if (tls_len % AES_BLOCK_LEN != 0 ||
	    tls_len < AES_BLOCK_LEN + roundup2(os->mac_len + 1, AES_BLOCK_LEN))
		return (EMSGSIZE);

	/* First, decrypt the record. */
	crypto_initreq(&crp, os->sid);
	crp.crp_iv_start = sizeof(*hdr);
	crp.crp_payload_start = tls->params.tls_hlen;
	crp.crp_payload_length = tls_len - AES_BLOCK_LEN;
	crypto_use_mbuf(&crp, m);
	crp.crp_op = CRYPTO_OP_DECRYPT;
	crp.crp_flags = CRYPTO_F_CBIMM;

	counter_u64_add(ocf_tls11_cbc_decrypts, 1);

	error = ktls_ocf_dispatch(os, &crp);
	crypto_destroyreq(&crp);
	if (error)
		return (error);

	/* Verify the padding. */
	m_copydata(m, sizeof(*hdr) + tls_len - 1, 1, &pad);
	*trailer_len = os->mac_len + pad + 1;
	if (AES_BLOCK_LEN + *trailer_len > tls_len)
		return (EBADMSG);
	error = m_apply(m, sizeof(*hdr) + tls_len - (pad + 1), pad + 1,
	    check_padding, &pad);
	if (error)
		return (error);

	/* Verify the MAC. */
	tls_comp_len = tls_len - (AES_BLOCK_LEN + *trailer_len);
	memset(&uio, 0, sizeof(uio));

	/*
	 * Allocate and populate the iov.  Have to skip over the TLS
	 * header in 'm' as it is not part of the MAC input.
	 */
	iovcnt = 1;
	for (n = m; n != NULL; n = n->m_next)
		iovcnt++;
	iov = malloc(iovcnt * sizeof(*iov), M_KTLS_OCF, M_WAITOK);
	iov[0].iov_base = &ad;
	iov[0].iov_len = sizeof(ad);
	skip = sizeof(*hdr) + AES_BLOCK_LEN;
	for (i = 1, n = m; n != NULL; i++, n = n->m_next) {
		if (n->m_len < skip) {
			skip -= n->m_len;
			continue;
		}
		iov[i].iov_base = mtod(n, char *) + skip;
		iov[i].iov_len = n->m_len - skip;
		skip = 0;
	}
	uio.uio_iov = iov;
	uio.uio_iovcnt = i;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_td = curthread;
	uio.uio_resid = sizeof(ad) + tls_len - AES_BLOCK_LEN;

	/* Initialize the AAD. */
	ad.seq = htobe64(seqno);
	ad.type = hdr->tls_type;
	ad.tls_vmajor = hdr->tls_vmajor;
	ad.tls_vminor = hdr->tls_vminor;
	ad.tls_length = htons(tls_comp_len);

	crypto_initreq(&crp, os->mac_sid);
	crp.crp_payload_start = 0;
	crp.crp_payload_length = sizeof(ad) + tls_comp_len;
	crp.crp_digest_start = crp.crp_payload_length;
	crp.crp_op = CRYPTO_OP_VERIFY_DIGEST;
	crp.crp_flags = CRYPTO_F_CBIMM;
	crypto_use_uio(&crp, &uio);
	error = ktls_ocf_dispatch(os, &crp);

	crypto_destroyreq(&crp);
	free(iov, M_KTLS_OCF);
	return (error);
}

static const struct ktls_ocf_sw ktls_ocf_tls_cbc_sw = {
	.encrypt = ktls_ocf_tls_cbc_encrypt,
	.decrypt = ktls_ocf_tls_cbc_decrypt
};

static int
ktls_ocf_tls12_aead_encrypt(struct ktls_ocf_encrypt_state *state,
    struct ktls_session *tls, struct mbuf *m, struct iovec *outiov,
    int outiovcnt)
{
	const struct tls_record_layer *hdr;
	struct uio *uio;
	struct tls_aead_data *ad;
	struct cryptop *crp;
	struct ktls_ocf_session *os;
	int error;
	uint16_t tls_comp_len;

	os = tls->ocf_session;
	hdr = (const struct tls_record_layer *)m->m_epg_hdr;
	crp = &state->crp;
	uio = &state->uio;

	crypto_initreq(crp, os->sid);

	/* Setup the IV. */
	if (tls->params.cipher_algorithm == CRYPTO_AES_NIST_GCM_16) {
		memcpy(crp->crp_iv, tls->params.iv, TLS_AEAD_GCM_LEN);
		memcpy(crp->crp_iv + TLS_AEAD_GCM_LEN, hdr + 1,
		    sizeof(uint64_t));
	} else {
		/*
		 * Chacha20-Poly1305 constructs the IV for TLS 1.2
		 * identically to constructing the IV for AEAD in TLS
		 * 1.3.
		 */
		memcpy(crp->crp_iv, tls->params.iv, tls->params.iv_len);
		*(uint64_t *)(crp->crp_iv + 4) ^= htobe64(m->m_epg_seqno);
	}

	/* Setup the AAD. */
	ad = &state->aead;
	tls_comp_len = m->m_len - (m->m_epg_hdrlen + m->m_epg_trllen);
	ad->seq = htobe64(m->m_epg_seqno);
	ad->type = hdr->tls_type;
	ad->tls_vmajor = hdr->tls_vmajor;
	ad->tls_vminor = hdr->tls_vminor;
	ad->tls_length = htons(tls_comp_len);
	crp->crp_aad = ad;
	crp->crp_aad_length = sizeof(*ad);

	/* Set fields for input payload. */
	crypto_use_single_mbuf(crp, m);
	crp->crp_payload_start = m->m_epg_hdrlen;
	crp->crp_payload_length = tls_comp_len;

	if (outiov != NULL) {
		crp->crp_digest_start = crp->crp_payload_length;

		uio->uio_iov = outiov;
		uio->uio_iovcnt = outiovcnt;
		uio->uio_offset = 0;
		uio->uio_segflg = UIO_SYSSPACE;
		uio->uio_td = curthread;
		uio->uio_resid = crp->crp_payload_length + tls->params.tls_tlen;
		crypto_use_output_uio(crp, uio);
	} else
		crp->crp_digest_start = crp->crp_payload_start +
		    crp->crp_payload_length;

	crp->crp_op = CRYPTO_OP_ENCRYPT | CRYPTO_OP_COMPUTE_DIGEST;
	crp->crp_flags = CRYPTO_F_CBIMM | CRYPTO_F_IV_SEPARATE;
	if (tls->params.cipher_algorithm == CRYPTO_AES_NIST_GCM_16)
		counter_u64_add(ocf_tls12_gcm_encrypts, 1);
	else
		counter_u64_add(ocf_tls12_chacha20_encrypts, 1);
	if (outiov != NULL)
		counter_u64_add(ocf_separate_output, 1);
	else
		counter_u64_add(ocf_inplace, 1);
	if (tls->sync_dispatch) {
		error = ktls_ocf_dispatch(os, crp);
		crypto_destroyreq(crp);
	} else
		error = ktls_ocf_dispatch_async(state, crp);
	return (error);
}

static int
ktls_ocf_tls12_aead_decrypt(struct ktls_session *tls,
    const struct tls_record_layer *hdr, struct mbuf *m, uint64_t seqno,
    int *trailer_len)
{
	struct tls_aead_data ad;
	struct cryptop crp;
	struct ktls_ocf_session *os;
	int error;
	uint16_t tls_comp_len, tls_len;

	os = tls->ocf_session;

	/* Ensure record contains at least an explicit IV and tag. */
	tls_len = ntohs(hdr->tls_length);
	if (tls_len + sizeof(*hdr) < tls->params.tls_hlen +
	    tls->params.tls_tlen)
		return (EMSGSIZE);

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
		tls_comp_len = tls_len -
		    (AES_GMAC_HASH_LEN + sizeof(uint64_t));
	else
		tls_comp_len = tls_len - POLY1305_HASH_LEN;
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

/*
 * Reconstruct encrypted mbuf data in input buffer.
 */
static void
ktls_ocf_recrypt_fixup(struct mbuf *m, u_int skip, u_int len, char *buf)
{
	const char *src = buf;
	u_int todo;

	while (skip >= m->m_len) {
		skip -= m->m_len;
		m = m->m_next;
	}

	while (len > 0) {
		todo = m->m_len - skip;
		if (todo > len)
			todo = len;

		if (m->m_flags & M_DECRYPTED)
			memcpy(mtod(m, char *) + skip, src, todo);
		src += todo;
		len -= todo;
		skip = 0;
		m = m->m_next;
	}
}

static int
ktls_ocf_tls12_aead_recrypt(struct ktls_session *tls,
    const struct tls_record_layer *hdr, struct mbuf *m,
    uint64_t seqno)
{
	struct cryptop crp;
	struct ktls_ocf_session *os;
	char *buf;
	u_int payload_len;
	int error;
	uint16_t tls_len;

	os = tls->ocf_session;

	/* Ensure record contains at least an explicit IV and tag. */
	tls_len = ntohs(hdr->tls_length);
	if (tls_len < sizeof(uint64_t) + AES_GMAC_HASH_LEN)
		return (EMSGSIZE);

	crypto_initreq(&crp, os->recrypt_sid);

	KASSERT(tls->params.cipher_algorithm == CRYPTO_AES_NIST_GCM_16,
	    ("%s: only AES-GCM is supported", __func__));

	/* Setup the IV. */
	memcpy(crp.crp_iv, tls->params.iv, TLS_AEAD_GCM_LEN);
	memcpy(crp.crp_iv + TLS_AEAD_GCM_LEN, hdr + 1, sizeof(uint64_t));
	be32enc(crp.crp_iv + AES_GCM_IV_LEN, 2);

	payload_len = tls_len - (AES_GMAC_HASH_LEN + sizeof(uint64_t));
	crp.crp_op = CRYPTO_OP_ENCRYPT;
	crp.crp_flags = CRYPTO_F_CBIMM | CRYPTO_F_IV_SEPARATE;
	crypto_use_mbuf(&crp, m);
	crp.crp_payload_start = tls->params.tls_hlen;
	crp.crp_payload_length = payload_len;

	buf = malloc(payload_len, M_KTLS_OCF, M_WAITOK);
	crypto_use_output_buf(&crp, buf, payload_len);

	counter_u64_add(ocf_tls12_gcm_recrypts, 1);
	error = ktls_ocf_dispatch(os, &crp);

	crypto_destroyreq(&crp);

	if (error == 0)
		ktls_ocf_recrypt_fixup(m, tls->params.tls_hlen, payload_len,
		    buf);

	free(buf, M_KTLS_OCF);
	return (error);
}

static const struct ktls_ocf_sw ktls_ocf_tls12_aead_sw = {
	.encrypt = ktls_ocf_tls12_aead_encrypt,
	.recrypt = ktls_ocf_tls12_aead_recrypt,
	.decrypt = ktls_ocf_tls12_aead_decrypt,
};

static int
ktls_ocf_tls13_aead_encrypt(struct ktls_ocf_encrypt_state *state,
    struct ktls_session *tls, struct mbuf *m, struct iovec *outiov,
    int outiovcnt)
{
	const struct tls_record_layer *hdr;
	struct uio *uio;
	struct tls_aead_data_13 *ad;
	struct cryptop *crp;
	struct ktls_ocf_session *os;
	int error;

	os = tls->ocf_session;
	hdr = (const struct tls_record_layer *)m->m_epg_hdr;
	crp = &state->crp;
	uio = &state->uio;

	crypto_initreq(crp, os->sid);

	/* Setup the nonce. */
	memcpy(crp->crp_iv, tls->params.iv, tls->params.iv_len);
	*(uint64_t *)(crp->crp_iv + 4) ^= htobe64(m->m_epg_seqno);

	/* Setup the AAD. */
	ad = &state->aead13;
	ad->type = hdr->tls_type;
	ad->tls_vmajor = hdr->tls_vmajor;
	ad->tls_vminor = hdr->tls_vminor;
	ad->tls_length = hdr->tls_length;
	crp->crp_aad = ad;
	crp->crp_aad_length = sizeof(*ad);

	/* Set fields for input payload. */
	crypto_use_single_mbuf(crp, m);
	crp->crp_payload_start = m->m_epg_hdrlen;
	crp->crp_payload_length = m->m_len -
	    (m->m_epg_hdrlen + m->m_epg_trllen);

	/* Store the record type as the first byte of the trailer. */
	m->m_epg_trail[0] = m->m_epg_record_type;
	crp->crp_payload_length++;

	if (outiov != NULL) {
		crp->crp_digest_start = crp->crp_payload_length;

		uio->uio_iov = outiov;
		uio->uio_iovcnt = outiovcnt;
		uio->uio_offset = 0;
		uio->uio_segflg = UIO_SYSSPACE;
		uio->uio_td = curthread;
		uio->uio_resid = m->m_len - m->m_epg_hdrlen;
		crypto_use_output_uio(crp, uio);
	} else
		crp->crp_digest_start = crp->crp_payload_start +
		    crp->crp_payload_length;

	crp->crp_op = CRYPTO_OP_ENCRYPT | CRYPTO_OP_COMPUTE_DIGEST;
	crp->crp_flags = CRYPTO_F_CBIMM | CRYPTO_F_IV_SEPARATE;

	if (tls->params.cipher_algorithm == CRYPTO_AES_NIST_GCM_16)
		counter_u64_add(ocf_tls13_gcm_encrypts, 1);
	else
		counter_u64_add(ocf_tls13_chacha20_encrypts, 1);
	if (outiov != NULL)
		counter_u64_add(ocf_separate_output, 1);
	else
		counter_u64_add(ocf_inplace, 1);
	if (tls->sync_dispatch) {
		error = ktls_ocf_dispatch(os, crp);
		crypto_destroyreq(crp);
	} else
		error = ktls_ocf_dispatch_async(state, crp);
	return (error);
}

static int
ktls_ocf_tls13_aead_decrypt(struct ktls_session *tls,
    const struct tls_record_layer *hdr, struct mbuf *m, uint64_t seqno,
    int *trailer_len)
{
	struct tls_aead_data_13 ad;
	struct cryptop crp;
	struct ktls_ocf_session *os;
	int error;
	u_int tag_len;
	uint16_t tls_len;

	os = tls->ocf_session;

	tag_len = tls->params.tls_tlen - 1;

	/* Payload must contain at least one byte for the record type. */
	tls_len = ntohs(hdr->tls_length);
	if (tls_len < tag_len + 1)
		return (EMSGSIZE);

	crypto_initreq(&crp, os->sid);

	/* Setup the nonce. */
	memcpy(crp.crp_iv, tls->params.iv, tls->params.iv_len);
	*(uint64_t *)(crp.crp_iv + 4) ^= htobe64(seqno);

	/* Setup the AAD. */
	ad.type = hdr->tls_type;
	ad.tls_vmajor = hdr->tls_vmajor;
	ad.tls_vminor = hdr->tls_vminor;
	ad.tls_length = hdr->tls_length;
	crp.crp_aad = &ad;
	crp.crp_aad_length = sizeof(ad);

	crp.crp_payload_start = tls->params.tls_hlen;
	crp.crp_payload_length = tls_len - tag_len;
	crp.crp_digest_start = crp.crp_payload_start + crp.crp_payload_length;

	crp.crp_op = CRYPTO_OP_DECRYPT | CRYPTO_OP_VERIFY_DIGEST;
	crp.crp_flags = CRYPTO_F_CBIMM | CRYPTO_F_IV_SEPARATE;
	crypto_use_mbuf(&crp, m);

	if (tls->params.cipher_algorithm == CRYPTO_AES_NIST_GCM_16)
		counter_u64_add(ocf_tls13_gcm_decrypts, 1);
	else
		counter_u64_add(ocf_tls13_chacha20_decrypts, 1);
	error = ktls_ocf_dispatch(os, &crp);

	crypto_destroyreq(&crp);
	*trailer_len = tag_len;
	return (error);
}

static int
ktls_ocf_tls13_aead_recrypt(struct ktls_session *tls,
    const struct tls_record_layer *hdr, struct mbuf *m,
    uint64_t seqno)
{
	struct cryptop crp;
	struct ktls_ocf_session *os;
	char *buf;
	u_int payload_len;
	int error;
	uint16_t tls_len;

	os = tls->ocf_session;

	/* Payload must contain at least one byte for the record type. */
	tls_len = ntohs(hdr->tls_length);
	if (tls_len < AES_GMAC_HASH_LEN + 1)
		return (EMSGSIZE);

	crypto_initreq(&crp, os->recrypt_sid);

	KASSERT(tls->params.cipher_algorithm == CRYPTO_AES_NIST_GCM_16,
	    ("%s: only AES-GCM is supported", __func__));

	/* Setup the IV. */
	memcpy(crp.crp_iv, tls->params.iv, tls->params.iv_len);
	*(uint64_t *)(crp.crp_iv + 4) ^= htobe64(seqno);
	be32enc(crp.crp_iv + 12, 2);

	payload_len = tls_len - AES_GMAC_HASH_LEN;
	crp.crp_op = CRYPTO_OP_ENCRYPT;
	crp.crp_flags = CRYPTO_F_CBIMM | CRYPTO_F_IV_SEPARATE;
	crypto_use_mbuf(&crp, m);
	crp.crp_payload_start = tls->params.tls_hlen;
	crp.crp_payload_length = payload_len;

	buf = malloc(payload_len, M_KTLS_OCF, M_WAITOK);
	crypto_use_output_buf(&crp, buf, payload_len);

	counter_u64_add(ocf_tls13_gcm_recrypts, 1);
	error = ktls_ocf_dispatch(os, &crp);

	crypto_destroyreq(&crp);

	if (error == 0)
		ktls_ocf_recrypt_fixup(m, tls->params.tls_hlen, payload_len,
		    buf);

	free(buf, M_KTLS_OCF);
	return (error);
}

static const struct ktls_ocf_sw ktls_ocf_tls13_aead_sw = {
	.encrypt = ktls_ocf_tls13_aead_encrypt,
	.recrypt = ktls_ocf_tls13_aead_recrypt,
	.decrypt = ktls_ocf_tls13_aead_decrypt,
};

void
ktls_ocf_free(struct ktls_session *tls)
{
	struct ktls_ocf_session *os;

	os = tls->ocf_session;
	crypto_freesession(os->sid);
	crypto_freesession(os->mac_sid);
	crypto_freesession(os->recrypt_sid);
	mtx_destroy(&os->lock);
	zfree(os, M_KTLS_OCF);
}

int
ktls_ocf_try(struct ktls_session *tls, int direction)
{
	struct crypto_session_params csp, mac_csp, recrypt_csp;
	struct ktls_ocf_session *os;
	int error, mac_len;

	memset(&csp, 0, sizeof(csp));
	memset(&mac_csp, 0, sizeof(mac_csp));
	mac_csp.csp_mode = CSP_MODE_NONE;
	mac_len = 0;
	memset(&recrypt_csp, 0, sizeof(mac_csp));
	recrypt_csp.csp_mode = CSP_MODE_NONE;

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

		csp.csp_flags |= CSP_F_SEPARATE_OUTPUT | CSP_F_SEPARATE_AAD;
		csp.csp_mode = CSP_MODE_AEAD;
		csp.csp_cipher_alg = CRYPTO_AES_NIST_GCM_16;
		csp.csp_cipher_key = tls->params.cipher_key;
		csp.csp_cipher_klen = tls->params.cipher_key_len;
		csp.csp_ivlen = AES_GCM_IV_LEN;

		recrypt_csp.csp_flags |= CSP_F_SEPARATE_OUTPUT;
		recrypt_csp.csp_mode = CSP_MODE_CIPHER;
		recrypt_csp.csp_cipher_alg = CRYPTO_AES_ICM;
		recrypt_csp.csp_cipher_key = tls->params.cipher_key;
		recrypt_csp.csp_cipher_klen = tls->params.cipher_key_len;
		recrypt_csp.csp_ivlen = AES_BLOCK_LEN;
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

		/* AES-CBC is not supported for receive for TLS 1.0. */
		if (direction == KTLS_RX &&
		    tls->params.tls_vminor == TLS_MINOR_VER_ZERO)
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

	if (recrypt_csp.csp_mode != CSP_MODE_NONE) {
		error = crypto_newsession(&os->recrypt_sid, &recrypt_csp,
		    CRYPTO_FLAG_HARDWARE | CRYPTO_FLAG_SOFTWARE);
		if (error) {
			crypto_freesession(os->sid);
			free(os, M_KTLS_OCF);
			return (error);
		}
	}

	mtx_init(&os->lock, "ktls_ocf", NULL, MTX_DEF);
	tls->ocf_session = os;
	if (tls->params.cipher_algorithm == CRYPTO_AES_NIST_GCM_16 ||
	    tls->params.cipher_algorithm == CRYPTO_CHACHA20_POLY1305) {
		if (tls->params.tls_vminor == TLS_MINOR_VER_THREE)
			os->sw = &ktls_ocf_tls13_aead_sw;
		else
			os->sw = &ktls_ocf_tls12_aead_sw;
	} else {
		os->sw = &ktls_ocf_tls_cbc_sw;
		if (tls->params.tls_vminor == TLS_MINOR_VER_ZERO) {
			os->implicit_iv = true;
			memcpy(os->iv, tls->params.iv, AES_BLOCK_LEN);
#ifdef INVARIANTS
			os->next_seqno = tls->next_seqno;
#endif
		}
	}

	/*
	 * AES-CBC is always synchronous currently.  Asynchronous
	 * operation would require multiple callbacks and an additional
	 * iovec array in ktls_ocf_encrypt_state.
	 */
	tls->sync_dispatch = CRYPTO_SESS_SYNC(os->sid) ||
	    tls->params.cipher_algorithm == CRYPTO_AES_CBC;
	return (0);
}

int
ktls_ocf_encrypt(struct ktls_ocf_encrypt_state *state,
    struct ktls_session *tls, struct mbuf *m, struct iovec *outiov,
    int outiovcnt)
{
	return (tls->ocf_session->sw->encrypt(state, tls, m, outiov,
	    outiovcnt));
}

int
ktls_ocf_decrypt(struct ktls_session *tls, const struct tls_record_layer *hdr,
    struct mbuf *m, uint64_t seqno, int *trailer_len)
{
	return (tls->ocf_session->sw->decrypt(tls, hdr, m, seqno, trailer_len));
}

int
ktls_ocf_recrypt(struct ktls_session *tls, const struct tls_record_layer *hdr,
    struct mbuf *m, uint64_t seqno)
{
	return (tls->ocf_session->sw->recrypt(tls, hdr, m, seqno));
}

bool
ktls_ocf_recrypt_supported(struct ktls_session *tls)
{
	return (tls->ocf_session->sw->recrypt != NULL &&
	    tls->ocf_session->recrypt_sid != NULL);
}
