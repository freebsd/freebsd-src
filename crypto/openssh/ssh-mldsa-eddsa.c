/* $OpenBSD: ssh-mldsa-eddsa.c,v 1.1 2026/06/14 03:59:34 djm Exp $ */
/*
 * Copyright (c) 2026 Damien Miller <djm@mindrot.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* draft-miller-sshm-mldsa44-ed25519-composite-sigs-00 */

#include "includes.h"

#ifdef USE_MLDSA

#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "crypto_api.h"
#include "sshbuf.h"
#include "ssherr.h"
#include "digest.h"
#define SSHKEY_INTERNAL
#include "sshkey.h"
#include "log.h"

#define COMPOSITE_PREFIX "CompositeAlgorithmSignatures2025"
#define COMPOSITE_LABEL  "COMPSIG-MLDSA44-Ed25519-SHA512"
#define SSH_MLDSA44_ED25519_ALG_NAME   "ssh-mldsa44-ed25519@openssh.com"

/*
 * raw_* functions implement the draft-ietf-lamps-pq-composite-sigs-18
 * composite signature scheme. These are exposed (i.e. not static) so
 * we can test them separately in unittests/crypto.
 */

int
crypto_sign_mldsa44_ed25519_keygen(uint8_t pk[MLDSA44_ED25519_PK_SZ],
    uint8_t sk[MLDSA44_ED25519_SK_SZ])
{
	uint8_t mldsa_seed[32], ed25519_seed[32];
	int r;

	arc4random_buf(mldsa_seed, sizeof(mldsa_seed));
	arc4random_buf(ed25519_seed, sizeof(ed25519_seed));

	r = crypto_sign_mldsa44_ed25519_keygen_seeded(pk, sk, mldsa_seed,
	    ed25519_seed);
	explicit_bzero(mldsa_seed, sizeof(mldsa_seed));
	explicit_bzero(ed25519_seed, sizeof(ed25519_seed));
	return r;
}

int
crypto_sign_mldsa44_ed25519_keygen_seeded(uint8_t pk[MLDSA44_ED25519_PK_SZ],
    uint8_t sk[MLDSA44_ED25519_SK_SZ], const uint8_t mldsa_seed[32],
    const uint8_t ed25519_seed[32])
{
	uint8_t ed25519_pk[32], ed25519_sk[64];
	uint8_t mldsa_sk[MLDSA44_SECRETKEYBYTES];
	int ret = -1;

	if (crypto_sign_mldsa44_keypair_seeded(pk, mldsa_sk, mldsa_seed) != 0)
		goto out;
	if (crypto_sign_ed25519_keypair_from_seed(ed25519_pk, ed25519_sk,
	    ed25519_seed) != 0)
		goto out;

	/* Serialize PK: mldsaPK || ed25519PK */
	memcpy(pk + MLDSA44_PUBLICKEYBYTES, ed25519_pk, 32);

	/* Serialize SK: mldsaSeed || ed25519Seed */
	memcpy(sk, mldsa_seed, 32);
	memcpy(sk + 32, ed25519_seed, 32);

	/* success */
	ret = 0;
 out:
	explicit_bzero(mldsa_sk, sizeof(mldsa_sk));
	explicit_bzero(ed25519_sk, sizeof(ed25519_sk));
	return ret;
}

static int
construct_m_prime(uint8_t **m_primep, size_t *m_prime_lenp,
    const uint8_t *msg, size_t msglen,
    const uint8_t *ctx, size_t ctxlen)
{
	int r;
	uint8_t hash[64];
	struct sshbuf *m_prime;

	*m_primep = NULL;
	*m_prime_lenp = 0;

	if (ctxlen > 255)
		return SSH_ERR_INVALID_ARGUMENT;
	if ((r = ssh_digest_memory(SSH_DIGEST_SHA512, msg, msglen,
	    hash, sizeof(hash))) != 0)
		return r;
	if ((m_prime = sshbuf_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if ((r = sshbuf_put(m_prime, COMPOSITE_PREFIX,
	    sizeof(COMPOSITE_PREFIX) - 1)) != 0 ||
	    (r = sshbuf_put(m_prime, COMPOSITE_LABEL,
	    sizeof(COMPOSITE_LABEL) - 1)) != 0 ||
	    (r = sshbuf_put_u8(m_prime, (uint8_t)ctxlen)) != 0 ||
	    (r = sshbuf_put(m_prime, ctx, ctxlen)) != 0 ||
	    (r = sshbuf_put(m_prime, hash, sizeof(hash))) != 0) {
		sshbuf_free(m_prime);
		return r;
	}
	if ((*m_primep = malloc(sshbuf_len(m_prime))) == NULL) {
		sshbuf_free(m_prime);
		return SSH_ERR_ALLOC_FAIL;
	}
	memcpy(*m_primep, sshbuf_ptr(m_prime), sshbuf_len(m_prime));
	*m_prime_lenp = sshbuf_len(m_prime);
	/* success */
	sshbuf_free(m_prime);
	return 0;
}

int
crypto_sign_mldsa44_ed25519_sign(uint8_t sig[MLDSA44_ED25519_SIG_SZ],
    const uint8_t *msg, size_t msglen,
    const uint8_t *ctx, size_t ctxlen,
    const uint8_t sk[MLDSA44_ED25519_SK_SZ])
{
	uint8_t *m_prime = NULL;
	size_t m_prime_len = 0;
	uint8_t mldsa_sk[MLDSA44_SECRETKEYBYTES];
	uint8_t mldsa_pk_dummy[MLDSA44_PUBLICKEYBYTES];
	uint8_t ed25519_pk[32], ed25519_sk[64];
	uint8_t mldsa_rnd[32];
	unsigned long long smlen;
	int r = -1;

	if (construct_m_prime(&m_prime, &m_prime_len, msg, msglen,
	    ctx, ctxlen) != 0)
		return -1;

	/* Expand ML-DSA key from seed */
	if (crypto_sign_mldsa44_keypair_seeded(mldsa_pk_dummy, mldsa_sk, sk) != 0)
		goto out;

	/* Sign with ML-DSA */
	arc4random_buf(mldsa_rnd, sizeof(mldsa_rnd));
	if (crypto_sign_mldsa44_seeded(sig, m_prime, m_prime_len,
	    (const uint8_t *)COMPOSITE_LABEL, sizeof(COMPOSITE_LABEL) - 1,
	    mldsa_sk, mldsa_rnd) != 0)
		goto out;

	/* Expand Ed25519 key from seed */
	if (crypto_sign_ed25519_keypair_from_seed(ed25519_pk, ed25519_sk,
	    sk + 32) != 0)
		goto out;

	/* Sign with Ed25519 */
	uint8_t *sm = malloc(m_prime_len + 64);
	if (sm == NULL)
		goto out;

	if (crypto_sign_ed25519(sm, &smlen, m_prime, m_prime_len,
	    ed25519_sk) != 0) {
		free(sm);
		goto out;
	}
	memcpy(sig + MLDSA44_SIGBYTES, sm, 64);
	free(sm);

	r = 0;
 out:
	free(m_prime);
	explicit_bzero(mldsa_rnd, sizeof(mldsa_rnd));
	explicit_bzero(mldsa_sk, sizeof(mldsa_sk));
	explicit_bzero(ed25519_sk, sizeof(ed25519_sk));
	return r;
}

int
crypto_sign_mldsa44_ed25519_verify(const uint8_t sig[MLDSA44_ED25519_SIG_SZ],
    const uint8_t *msg, size_t msglen,
    const uint8_t *ctx, size_t ctxlen,
    const uint8_t pk[MLDSA44_ED25519_PK_SZ])
{
	uint8_t *m_prime = NULL;
	size_t m_prime_len = 0;
	uint8_t *sm = NULL, *m = NULL;
	unsigned long long smlen, mlen;
	int r = -1;

	if (construct_m_prime(&m_prime, &m_prime_len, msg, msglen,
	    ctx, ctxlen) != 0)
		return -1;

	/* Verify ML-DSA */
	if (crypto_sign_mldsa44_verify(sig, m_prime, m_prime_len,
	    (const uint8_t *)COMPOSITE_LABEL, sizeof(COMPOSITE_LABEL) - 1,
	    pk) != 0)
		goto out;

	/* Verify Ed25519 */
	smlen = m_prime_len + 64;
	mlen = smlen;
	if ((sm = malloc(smlen)) == NULL || (m = malloc(mlen)) == NULL)
		goto out;
	memcpy(sm, sig + MLDSA44_SIGBYTES, 64);
	memcpy(sm + 64, m_prime, m_prime_len);

	if (crypto_sign_ed25519_open(m, &mlen, sm, smlen,
	    pk + MLDSA44_PUBLICKEYBYTES) != 0)
		goto out;
	if (mlen != m_prime_len)
		goto out;

	r = 0;
 out:
	free(m_prime);
	free(sm);
	free(m);
	return r;
}

/* sshkey integration */

static void
ssh_mldsa44_ed25519_cleanup(struct sshkey *k)
{
	freezero(k->mldsa_ed25519_pk, MLDSA44_ED25519_PK_SZ);
	freezero(k->mldsa_ed25519_sk, MLDSA44_ED25519_SK_SZ);
	k->mldsa_ed25519_pk = NULL;
	k->mldsa_ed25519_sk = NULL;
}

static int
ssh_mldsa44_ed25519_equal(const struct sshkey *a, const struct sshkey *b)
{
	if (a->mldsa_ed25519_pk == NULL || b->mldsa_ed25519_pk == NULL)
		return 0;
	if (memcmp(a->mldsa_ed25519_pk, b->mldsa_ed25519_pk,
	    MLDSA44_ED25519_PK_SZ) != 0)
		return 0;
	return 1;
}

static int
ssh_mldsa44_ed25519_serialize_public(const struct sshkey *key, struct sshbuf *b,
    enum sshkey_serialize_rep opts)
{
	int r;

	if (key->mldsa_ed25519_pk == NULL)
		return SSH_ERR_INVALID_ARGUMENT;
	if ((r = sshbuf_put_string(b, key->mldsa_ed25519_pk,
	    MLDSA44_ED25519_PK_SZ)) != 0)
		return r;

	return 0;
}

static int
ssh_mldsa44_ed25519_serialize_private(const struct sshkey *key, struct sshbuf *b,
    enum sshkey_serialize_rep opts)
{
	int r;

	if (key->mldsa_ed25519_sk == NULL)
		return SSH_ERR_INVALID_ARGUMENT;
	if (!sshkey_is_cert(key)) {
		if ((r = ssh_mldsa44_ed25519_serialize_public(key,
		    b, opts)) != 0)
			return r;
	}
	if ((r = sshbuf_put_string(b, key->mldsa_ed25519_sk,
	    MLDSA44_ED25519_SK_SZ)) != 0)
		return r;

	return 0;
}

static int
ssh_mldsa44_ed25519_deserialize_public(const char *ktype, struct sshbuf *b,
    struct sshkey *key)
{
	u_char *pk = NULL;
	size_t len = 0;
	int r;

	if ((r = sshbuf_get_string(b, &pk, &len)) != 0)
		return r;
	if (len != MLDSA44_ED25519_PK_SZ) {
		freezero(pk, len);
		return SSH_ERR_INVALID_FORMAT;
	}
	key->mldsa_ed25519_pk = pk;
	return 0;
}

static int
ssh_mldsa44_ed25519_deserialize_private(const char *ktype, struct sshbuf *b,
    struct sshkey *key)
{
	int r;
	size_t sklen = 0;
	u_char *sk = NULL;

	if (!sshkey_is_cert(key)) {
		if ((r = ssh_mldsa44_ed25519_deserialize_public(ktype,
		    b, key)) != 0)
			return r;
	}
	if ((r = sshbuf_get_string(b, &sk, &sklen)) != 0)
		goto out;
	if (sklen != MLDSA44_ED25519_SK_SZ) {
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	key->mldsa_ed25519_sk = sk;
	sk = NULL; /* transferred */
	r = 0;
 out:
	freezero(sk, sklen);
	return r;
}

static int
ssh_mldsa44_ed25519_generate(struct sshkey *k, int bits)
{
	free(k->mldsa_ed25519_pk);
	free(k->mldsa_ed25519_sk);
	k->mldsa_ed25519_pk = NULL;
	k->mldsa_ed25519_sk = NULL;
	if ((k->mldsa_ed25519_pk = malloc(MLDSA44_ED25519_PK_SZ)) == NULL ||
	    (k->mldsa_ed25519_sk = malloc(MLDSA44_ED25519_SK_SZ)) == NULL) {
		free(k->mldsa_ed25519_pk);
		return SSH_ERR_ALLOC_FAIL;
	}
	if (crypto_sign_mldsa44_ed25519_keygen(k->mldsa_ed25519_pk,
	    k->mldsa_ed25519_sk) != 0) {
		free(k->mldsa_ed25519_pk);
		free(k->mldsa_ed25519_sk);
		return SSH_ERR_CRYPTO_ERROR;
	}
	return 0;
}

static int
ssh_mldsa44_ed25519_copy_public(const struct sshkey *from, struct sshkey *to)
{
	if (from->mldsa_ed25519_pk == NULL)
		return SSH_ERR_INVALID_ARGUMENT;
	if ((to->mldsa_ed25519_pk = malloc(MLDSA44_ED25519_PK_SZ)) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	memcpy(to->mldsa_ed25519_pk, from->mldsa_ed25519_pk,
	    MLDSA44_ED25519_PK_SZ);
	return 0;
}

static int
ssh_mldsa44_ed25519_sign(struct sshkey *key,
    u_char **sigp, size_t *lenp, const u_char *data, size_t datalen,
    const char *alg, const char *sk_provider, const char *sk_pin,
    u_int compat)
{
	u_char sig[MLDSA44_ED25519_SIG_SZ];
	struct sshbuf *b = NULL;
	int r = SSH_ERR_INTERNAL_ERROR;

	if (lenp != NULL)
		*lenp = 0;
	if (sigp != NULL)
		*sigp = NULL;

	if (key == NULL ||
	    sshkey_type_plain(key->type) != KEY_MLDSA44_ED25519 ||
	    key->mldsa_ed25519_sk == NULL)
		return SSH_ERR_INVALID_ARGUMENT;

	if (crypto_sign_mldsa44_ed25519_sign(sig, data, datalen, NULL, 0,
	    key->mldsa_ed25519_sk) != 0) {
		r = SSH_ERR_CRYPTO_ERROR;
		goto out;
	}

	if ((b = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((r = sshbuf_put_cstring(b, SSH_MLDSA44_ED25519_ALG_NAME)) != 0 ||
	    (r = sshbuf_put_string(b, sig, sizeof(sig))) != 0)
		goto out;

	if (sigp != NULL) {
		if ((*sigp = malloc(sshbuf_len(b))) == NULL) {
			r = SSH_ERR_ALLOC_FAIL;
			goto out;
		}
		memcpy(*sigp, sshbuf_ptr(b), sshbuf_len(b));
	}
	if (lenp != NULL)
		*lenp = sshbuf_len(b);
	r = 0;
 out:
	sshbuf_free(b);
	explicit_bzero(sig, sizeof(sig));
	return r;
}

static int
ssh_mldsa44_ed25519_verify(const struct sshkey *key,
    const u_char *sig, size_t siglen, const u_char *data, size_t dlen,
    const char *alg, u_int compat, struct sshkey_sig_details **detailsp)
{
	struct sshbuf *b = NULL;
	char *ktype = NULL;
	const u_char *sigblob;
	size_t len;
	int r;

	if (key == NULL ||
	    sshkey_type_plain(key->type) != KEY_MLDSA44_ED25519 ||
	    key->mldsa_ed25519_pk == NULL ||
	    sig == NULL || siglen == 0)
		return SSH_ERR_INVALID_ARGUMENT;

	if ((b = sshbuf_from(sig, siglen)) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if ((r = sshbuf_get_cstring(b, &ktype, NULL)) != 0 ||
	    (r = sshbuf_get_string_direct(b, &sigblob, &len)) != 0)
		goto out;
	if (strcmp(SSH_MLDSA44_ED25519_ALG_NAME, ktype) != 0) {
		r = SSH_ERR_KEY_TYPE_MISMATCH;
		goto out;
	}
	if (sshbuf_len(b) != 0) {
		r = SSH_ERR_UNEXPECTED_TRAILING_DATA;
		goto out;
	}
	if (len != MLDSA44_ED25519_SIG_SZ) {
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}

	if (crypto_sign_mldsa44_ed25519_verify(sigblob, data, dlen, NULL, 0,
	    key->mldsa_ed25519_pk) != 0) {
		r = SSH_ERR_SIGNATURE_INVALID;
		goto out;
	}

	r = 0;
 out:
	sshbuf_free(b);
	free(ktype);
	return r;
}

const struct sshkey_impl_funcs sshkey_mldsa44_ed25519_funcs = {
	/* .size = */		NULL,
	/* .alloc = */		NULL,
	/* .cleanup = */	ssh_mldsa44_ed25519_cleanup,
	/* .equal = */		ssh_mldsa44_ed25519_equal,
	/* .ssh_serialize_public = */ ssh_mldsa44_ed25519_serialize_public,
	/* .ssh_deserialize_public = */ ssh_mldsa44_ed25519_deserialize_public,
	/* .ssh_serialize_private = */ ssh_mldsa44_ed25519_serialize_private,
	/* .ssh_deserialize_private = */ ssh_mldsa44_ed25519_deserialize_private,
	/* .generate = */	ssh_mldsa44_ed25519_generate,
	/* .copy_public = */	ssh_mldsa44_ed25519_copy_public,
	/* .sign = */		ssh_mldsa44_ed25519_sign,
	/* .verify = */		ssh_mldsa44_ed25519_verify,
};

const struct sshkey_impl sshkey_mldsa44_ed25519_impl = {
	/* .name = */		"ssh-mldsa44-ed25519@openssh.com",
	/* .shortname = */	"MLDSA44-ED25519",
	/* .sigalg = */		NULL,
	/* .type = */		KEY_MLDSA44_ED25519,
	/* .nid = */		0,
	/* .cert = */		0,
	/* .sigonly = */	0,
	/* .keybits = */	256,
	/* .funcs = */		&sshkey_mldsa44_ed25519_funcs,
};

const struct sshkey_impl sshkey_mldsa44_ed25519_cert_impl = {
	/* .name = */		"ssh-mldsa44-ed25519-cert-v01@openssh.com",
	/* .shortname = */	"MLDSA44-ED25519-CERT",
	/* .sigalg = */		NULL,
	/* .type = */		KEY_MLDSA44_ED25519_CERT,
	/* .nid = */		0,
	/* .cert = */		1,
	/* .sigonly = */	0,
	/* .keybits = */	256,
	/* .funcs = */		&sshkey_mldsa44_ed25519_funcs,
};
#endif /* USE_MLDSA */
