/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2008 Isilon Inc http://www.isilon.com/
 * Authors: Doug Rabson <dfr@rabson.org>
 * Developed with Red Inc: Alfred Perlstein <alfred@freebsd.org>
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/kobj.h>
#include <sys/mbuf.h>
#include <opencrypto/cryptodev.h>

#include <kgssapi/gssapi.h>
#include <kgssapi/gssapi_impl.h>

#include "kcrypto.h"

struct aes_state {
	struct mtx	as_lock;
	crypto_session_t as_session_aes;
	crypto_session_t as_session_sha1;
};

static void
aes_init(struct krb5_key_state *ks)
{
	struct aes_state *as;

	as = malloc(sizeof(struct aes_state), M_GSSAPI, M_WAITOK|M_ZERO);
	mtx_init(&as->as_lock, "gss aes lock", NULL, MTX_DEF);
	ks->ks_priv = as;
}

static void
aes_destroy(struct krb5_key_state *ks)
{
	struct aes_state *as = ks->ks_priv;

	if (as->as_session_aes != 0)
		crypto_freesession(as->as_session_aes);
	if (as->as_session_sha1 != 0)
		crypto_freesession(as->as_session_sha1);
	mtx_destroy(&as->as_lock);
	free(ks->ks_priv, M_GSSAPI);
}

static void
aes_set_key(struct krb5_key_state *ks, const void *in)
{
	void *kp = ks->ks_key;
	struct aes_state *as = ks->ks_priv;
	struct crypto_session_params csp;

	if (kp != in)
		bcopy(in, kp, ks->ks_class->ec_keylen);

	if (as->as_session_aes != 0)
		crypto_freesession(as->as_session_aes);
	if (as->as_session_sha1 != 0)
		crypto_freesession(as->as_session_sha1);

	/*
	 * We only want the first 96 bits of the HMAC.
	 */
	memset(&csp, 0, sizeof(csp));
	csp.csp_mode = CSP_MODE_DIGEST;
	csp.csp_auth_alg = CRYPTO_SHA1_HMAC;
	csp.csp_auth_klen = ks->ks_class->ec_keybits / 8;
	csp.csp_auth_mlen = 12;
	csp.csp_auth_key = ks->ks_key;
	crypto_newsession(&as->as_session_sha1, &csp,
	    CRYPTOCAP_F_HARDWARE | CRYPTOCAP_F_SOFTWARE);

	memset(&csp, 0, sizeof(csp));
	csp.csp_mode = CSP_MODE_CIPHER;
	csp.csp_cipher_alg = CRYPTO_AES_CBC;
	csp.csp_cipher_klen = ks->ks_class->ec_keybits / 8;
	csp.csp_cipher_key = ks->ks_key;
	csp.csp_ivlen = 16;
	crypto_newsession(&as->as_session_aes, &csp,
	    CRYPTOCAP_F_HARDWARE | CRYPTOCAP_F_SOFTWARE);
}

static void
aes_random_to_key(struct krb5_key_state *ks, const void *in)
{

	aes_set_key(ks, in);
}

static int
aes_crypto_cb(struct cryptop *crp)
{
	int error;
	struct aes_state *as = (struct aes_state *) crp->crp_opaque;

	if (CRYPTO_SESS_SYNC(crp->crp_session))
		return (0);

	error = crp->crp_etype;
	if (error == EAGAIN)
		error = crypto_dispatch(crp);
	mtx_lock(&as->as_lock);
	if (error || (crp->crp_flags & CRYPTO_F_DONE))
		wakeup(crp);
	mtx_unlock(&as->as_lock);

	return (0);
}

static void
aes_encrypt_1(const struct krb5_key_state *ks, int buftype, void *buf,
    size_t skip, size_t len, void *ivec, bool encrypt)
{
	struct aes_state *as = ks->ks_priv;
	struct cryptop *crp;
	int error;

	crp = crypto_getreq(as->as_session_aes, M_WAITOK);

	crp->crp_payload_start = skip;
	crp->crp_payload_length = len;
	crp->crp_op = encrypt ? CRYPTO_OP_ENCRYPT : CRYPTO_OP_DECRYPT;
	crp->crp_flags = CRYPTO_F_CBIFSYNC | CRYPTO_F_IV_SEPARATE;
	if (ivec) {
		memcpy(crp->crp_iv, ivec, 16);
	} else {
		memset(crp->crp_iv, 0, 16);
	}

	if (buftype == CRYPTO_BUF_MBUF)
		crypto_use_mbuf(crp, buf);
	else
		crypto_use_buf(crp, buf, skip + len);
	crp->crp_opaque = as;
	crp->crp_callback = aes_crypto_cb;

	error = crypto_dispatch(crp);

	if (!CRYPTO_SESS_SYNC(as->as_session_aes)) {
		mtx_lock(&as->as_lock);
		if (!error && !(crp->crp_flags & CRYPTO_F_DONE))
			error = msleep(crp, &as->as_lock, 0, "gssaes", 0);
		mtx_unlock(&as->as_lock);
	}

	crypto_freereq(crp);
}

static void
aes_encrypt(const struct krb5_key_state *ks, struct mbuf *inout,
    size_t skip, size_t len, void *ivec, size_t ivlen)
{
	size_t blocklen = 16, plen;
	struct {
		uint8_t cn_1[16], cn[16];
	} last2;
	int i, off;

	/*
	 * AES encryption with cyphertext stealing:
	 *
	 * CTSencrypt(P[0], ..., P[n], IV, K):
	 *	len = length(P[n])
	 *	(C[0], ..., C[n-2], E[n-1]) =
	 *		CBCencrypt(P[0], ..., P[n-1], IV, K)
	 *	P = pad(P[n], 0, blocksize)
	 *	E[n] = CBCencrypt(P, E[n-1], K);
	 *	C[n-1] = E[n]
	 *	C[n] = E[n-1]{0..len-1}
	 */
	plen = len % blocklen;
	if (len == blocklen) {
		/*
		 * Note: caller will ensure len >= blocklen.
		 */
		aes_encrypt_1(ks, CRYPTO_BUF_MBUF, inout, skip, len, ivec,
		    true);
	} else if (plen == 0) {
		/*
		 * This is equivalent to CBC mode followed by swapping
		 * the last two blocks. We assume that neither of the
		 * last two blocks cross iov boundaries.
		 */
		aes_encrypt_1(ks, CRYPTO_BUF_MBUF, inout, skip, len, ivec,
		    true);
		off = skip + len - 2 * blocklen;
		m_copydata(inout, off, 2 * blocklen, (void*) &last2);
		m_copyback(inout, off, blocklen, last2.cn);
		m_copyback(inout, off + blocklen, blocklen, last2.cn_1);
	} else {
		/*
		 * This is the difficult case. We encrypt all but the
		 * last partial block first. We then create a padded
		 * copy of the last block and encrypt that using the
		 * second to last encrypted block as IV. Once we have
		 * the encrypted versions of the last two blocks, we
		 * reshuffle to create the final result.
		 */
		aes_encrypt_1(ks, CRYPTO_BUF_MBUF, inout, skip, len - plen,
		    ivec, true);

		/*
		 * Copy out the last two blocks, pad the last block
		 * and encrypt it. Rearrange to get the final
		 * result. The cyphertext for cn_1 is in cn. The
		 * cyphertext for cn is the first plen bytes of what
		 * is in cn_1 now.
		 */
		off = skip + len - blocklen - plen;
		m_copydata(inout, off, blocklen + plen, (void*) &last2);
		for (i = plen; i < blocklen; i++)
			last2.cn[i] = 0;
		aes_encrypt_1(ks, CRYPTO_BUF_CONTIG, last2.cn, 0, blocklen,
		    last2.cn_1, true);
		m_copyback(inout, off, blocklen, last2.cn);
		m_copyback(inout, off + blocklen, plen, last2.cn_1);
	}
}

static void
aes_decrypt(const struct krb5_key_state *ks, struct mbuf *inout,
    size_t skip, size_t len, void *ivec, size_t ivlen)
{
	size_t blocklen = 16, plen;
	struct {
		uint8_t cn_1[16], cn[16];
	} last2;
	int i, off, t;

	/*
	 * AES decryption with cyphertext stealing:
	 *
	 * CTSencrypt(C[0], ..., C[n], IV, K):
	 *	len = length(C[n])
	 *	E[n] = C[n-1]
	 *	X = decrypt(E[n], K)
	 *	P[n] = (X ^ C[n]){0..len-1}
	 *	E[n-1] = {C[n,0],...,C[n,len-1],X[len],...,X[blocksize-1]}
	 *	(P[0],...,P[n-1]) = CBCdecrypt(C[0],...,C[n-2],E[n-1], IV, K)
	 */
	plen = len % blocklen;
	if (len == blocklen) {
		/*
		 * Note: caller will ensure len >= blocklen.
		 */
		aes_encrypt_1(ks, CRYPTO_BUF_MBUF, inout, skip, len, ivec,
		    false);
	} else if (plen == 0) {
		/*
		 * This is equivalent to CBC mode followed by swapping
		 * the last two blocks.
		 */
		off = skip + len - 2 * blocklen;
		m_copydata(inout, off, 2 * blocklen, (void*) &last2);
		m_copyback(inout, off, blocklen, last2.cn);
		m_copyback(inout, off + blocklen, blocklen, last2.cn_1);
		aes_encrypt_1(ks, CRYPTO_BUF_MBUF, inout, skip, len, ivec,
		    false);
	} else {
		/*
		 * This is the difficult case. We first decrypt the
		 * second to last block with a zero IV to make X. The
		 * plaintext for the last block is the XOR of X and
		 * the last cyphertext block.
		 *
		 * We derive a new cypher text for the second to last
		 * block by mixing the unused bytes of X with the last
		 * cyphertext block. The result of that can be
		 * decrypted with the rest in CBC mode.
		 */
		off = skip + len - plen - blocklen;
		aes_encrypt_1(ks, CRYPTO_BUF_MBUF, inout, off, blocklen,
		    NULL, false);
		m_copydata(inout, off, blocklen + plen, (void*) &last2);

		for (i = 0; i < plen; i++) {
			t = last2.cn[i];
			last2.cn[i] ^= last2.cn_1[i];
			last2.cn_1[i] = t;
		}

		m_copyback(inout, off, blocklen + plen, (void*) &last2);
		aes_encrypt_1(ks, CRYPTO_BUF_MBUF, inout, skip, len - plen,
		    ivec, false);
	}

}

static void
aes_checksum(const struct krb5_key_state *ks, int usage,
    struct mbuf *inout, size_t skip, size_t inlen, size_t outlen)
{
	struct aes_state *as = ks->ks_priv;
	struct cryptop *crp;
	int error;

	crp = crypto_getreq(as->as_session_sha1, M_WAITOK);

	crp->crp_payload_start = skip;
	crp->crp_payload_length = inlen;
	crp->crp_digest_start = skip + inlen;
	crp->crp_flags = CRYPTO_F_CBIFSYNC;
	crypto_use_mbuf(crp, inout);
	crp->crp_opaque = as;
	crp->crp_callback = aes_crypto_cb;

	error = crypto_dispatch(crp);

	if (!CRYPTO_SESS_SYNC(as->as_session_sha1)) {
		mtx_lock(&as->as_lock);
		if (!error && !(crp->crp_flags & CRYPTO_F_DONE))
			error = msleep(crp, &as->as_lock, 0, "gssaes", 0);
		mtx_unlock(&as->as_lock);
	}

	crypto_freereq(crp);
}

struct krb5_encryption_class krb5_aes128_encryption_class = {
	"aes128-cts-hmac-sha1-96", /* name */
	ETYPE_AES128_CTS_HMAC_SHA1_96, /* etype */
	EC_DERIVED_KEYS,	/* flags */
	16,			/* blocklen */
	1,			/* msgblocklen */
	12,			/* checksumlen */
	128,			/* keybits */
	16,			/* keylen */
	aes_init,
	aes_destroy,
	aes_set_key,
	aes_random_to_key,
	aes_encrypt,
	aes_decrypt,
	aes_checksum
};

struct krb5_encryption_class krb5_aes256_encryption_class = {
	"aes256-cts-hmac-sha1-96", /* name */
	ETYPE_AES256_CTS_HMAC_SHA1_96, /* etype */
	EC_DERIVED_KEYS,	/* flags */
	16,			/* blocklen */
	1,			/* msgblocklen */
	12,			/* checksumlen */
	256,			/* keybits */
	32,			/* keylen */
	aes_init,
	aes_destroy,
	aes_set_key,
	aes_random_to_key,
	aes_encrypt,
	aes_decrypt,
	aes_checksum
};
