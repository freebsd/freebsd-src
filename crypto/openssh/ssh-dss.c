/* $OpenBSD: ssh-dss.c,v 1.50 2024/01/11 01:45:36 djm Exp $ */
/*
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"

#if defined(WITH_OPENSSL) && defined(WITH_DSA)

#include <sys/types.h>

#include <openssl/bn.h>
#include <openssl/dsa.h>
#include <openssl/evp.h>

#include <stdarg.h>
#include <string.h>

#include "sshbuf.h"
#include "ssherr.h"
#include "digest.h"
#define SSHKEY_INTERNAL
#include "sshkey.h"

#include "openbsd-compat/openssl-compat.h"

#define INTBLOB_LEN	20
#define SIGBLOB_LEN	(2*INTBLOB_LEN)

static u_int
ssh_dss_size(const struct sshkey *key)
{
	const BIGNUM *dsa_p;

	if (key->dsa == NULL)
		return 0;
	DSA_get0_pqg(key->dsa, &dsa_p, NULL, NULL);
	return BN_num_bits(dsa_p);
}

static int
ssh_dss_alloc(struct sshkey *k)
{
	if ((k->dsa = DSA_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	return 0;
}

static void
ssh_dss_cleanup(struct sshkey *k)
{
	DSA_free(k->dsa);
	k->dsa = NULL;
}

static int
ssh_dss_equal(const struct sshkey *a, const struct sshkey *b)
{
	const BIGNUM *dsa_p_a, *dsa_q_a, *dsa_g_a, *dsa_pub_key_a;
	const BIGNUM *dsa_p_b, *dsa_q_b, *dsa_g_b, *dsa_pub_key_b;

	if (a->dsa == NULL || b->dsa == NULL)
		return 0;
	DSA_get0_pqg(a->dsa, &dsa_p_a, &dsa_q_a, &dsa_g_a);
	DSA_get0_pqg(b->dsa, &dsa_p_b, &dsa_q_b, &dsa_g_b);
	DSA_get0_key(a->dsa, &dsa_pub_key_a, NULL);
	DSA_get0_key(b->dsa, &dsa_pub_key_b, NULL);
	if (dsa_p_a == NULL || dsa_p_b == NULL ||
	    dsa_q_a == NULL || dsa_q_b == NULL ||
	    dsa_g_a == NULL || dsa_g_b == NULL ||
	    dsa_pub_key_a == NULL || dsa_pub_key_b == NULL)
		return 0;
	if (BN_cmp(dsa_p_a, dsa_p_b) != 0)
		return 0;
	if (BN_cmp(dsa_q_a, dsa_q_b) != 0)
		return 0;
	if (BN_cmp(dsa_g_a, dsa_g_b) != 0)
		return 0;
	if (BN_cmp(dsa_pub_key_a, dsa_pub_key_b) != 0)
		return 0;
	return 1;
}

static int
ssh_dss_serialize_public(const struct sshkey *key, struct sshbuf *b,
    enum sshkey_serialize_rep opts)
{
	int r;
	const BIGNUM *dsa_p, *dsa_q, *dsa_g, *dsa_pub_key;

	if (key->dsa == NULL)
		return SSH_ERR_INVALID_ARGUMENT;
	DSA_get0_pqg(key->dsa, &dsa_p, &dsa_q, &dsa_g);
	DSA_get0_key(key->dsa, &dsa_pub_key, NULL);
	if (dsa_p == NULL || dsa_q == NULL ||
	    dsa_g == NULL || dsa_pub_key == NULL)
		return SSH_ERR_INTERNAL_ERROR;
	if ((r = sshbuf_put_bignum2(b, dsa_p)) != 0 ||
	    (r = sshbuf_put_bignum2(b, dsa_q)) != 0 ||
	    (r = sshbuf_put_bignum2(b, dsa_g)) != 0 ||
	    (r = sshbuf_put_bignum2(b, dsa_pub_key)) != 0)
		return r;

	return 0;
}

static int
ssh_dss_serialize_private(const struct sshkey *key, struct sshbuf *b,
    enum sshkey_serialize_rep opts)
{
	int r;
	const BIGNUM *dsa_priv_key;

	DSA_get0_key(key->dsa, NULL, &dsa_priv_key);
	if (!sshkey_is_cert(key)) {
		if ((r = ssh_dss_serialize_public(key, b, opts)) != 0)
			return r;
	}
	if ((r = sshbuf_put_bignum2(b, dsa_priv_key)) != 0)
		return r;

	return 0;
}

static int
ssh_dss_generate(struct sshkey *k, int bits)
{
	DSA *private;

	if (bits != 1024)
		return SSH_ERR_KEY_LENGTH;
	if ((private = DSA_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if (!DSA_generate_parameters_ex(private, bits, NULL, 0, NULL,
	    NULL, NULL) || !DSA_generate_key(private)) {
		DSA_free(private);
		return SSH_ERR_LIBCRYPTO_ERROR;
	}
	k->dsa = private;
	return 0;
}

static int
ssh_dss_copy_public(const struct sshkey *from, struct sshkey *to)
{
	const BIGNUM *dsa_p, *dsa_q, *dsa_g, *dsa_pub_key;
	BIGNUM *dsa_p_dup = NULL, *dsa_q_dup = NULL, *dsa_g_dup = NULL;
	BIGNUM *dsa_pub_key_dup = NULL;
	int r = SSH_ERR_INTERNAL_ERROR;

	DSA_get0_pqg(from->dsa, &dsa_p, &dsa_q, &dsa_g);
	DSA_get0_key(from->dsa, &dsa_pub_key, NULL);
	if ((dsa_p_dup = BN_dup(dsa_p)) == NULL ||
	    (dsa_q_dup = BN_dup(dsa_q)) == NULL ||
	    (dsa_g_dup = BN_dup(dsa_g)) == NULL ||
	    (dsa_pub_key_dup = BN_dup(dsa_pub_key)) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if (!DSA_set0_pqg(to->dsa, dsa_p_dup, dsa_q_dup, dsa_g_dup)) {
		r = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	dsa_p_dup = dsa_q_dup = dsa_g_dup = NULL; /* transferred */
	if (!DSA_set0_key(to->dsa, dsa_pub_key_dup, NULL)) {
		r = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	dsa_pub_key_dup = NULL; /* transferred */
	/* success */
	r = 0;
 out:
	BN_clear_free(dsa_p_dup);
	BN_clear_free(dsa_q_dup);
	BN_clear_free(dsa_g_dup);
	BN_clear_free(dsa_pub_key_dup);
	return r;
}

static int
ssh_dss_deserialize_public(const char *ktype, struct sshbuf *b,
    struct sshkey *key)
{
	int ret = SSH_ERR_INTERNAL_ERROR;
	BIGNUM *dsa_p = NULL, *dsa_q = NULL, *dsa_g = NULL, *dsa_pub_key = NULL;

	if (sshbuf_get_bignum2(b, &dsa_p) != 0 ||
	    sshbuf_get_bignum2(b, &dsa_q) != 0 ||
	    sshbuf_get_bignum2(b, &dsa_g) != 0 ||
	    sshbuf_get_bignum2(b, &dsa_pub_key) != 0) {
		ret = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if (!DSA_set0_pqg(key->dsa, dsa_p, dsa_q, dsa_g)) {
		ret = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	dsa_p = dsa_q = dsa_g = NULL; /* transferred */
	if (!DSA_set0_key(key->dsa, dsa_pub_key, NULL)) {
		ret = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	dsa_pub_key = NULL; /* transferred */
#ifdef DEBUG_PK
	DSA_print_fp(stderr, key->dsa, 8);
#endif
	/* success */
	ret = 0;
 out:
	BN_clear_free(dsa_p);
	BN_clear_free(dsa_q);
	BN_clear_free(dsa_g);
	BN_clear_free(dsa_pub_key);
	return ret;
}

static int
ssh_dss_deserialize_private(const char *ktype, struct sshbuf *b,
    struct sshkey *key)
{
	int r;
	BIGNUM *dsa_priv_key = NULL;

	if (!sshkey_is_cert(key)) {
		if ((r = ssh_dss_deserialize_public(ktype, b, key)) != 0)
			return r;
	}

	if ((r = sshbuf_get_bignum2(b, &dsa_priv_key)) != 0)
		return r;
	if (!DSA_set0_key(key->dsa, NULL, dsa_priv_key)) {
		BN_clear_free(dsa_priv_key);
		return SSH_ERR_LIBCRYPTO_ERROR;
	}
	return 0;
}

static int
ssh_dss_sign(struct sshkey *key,
    u_char **sigp, size_t *lenp,
    const u_char *data, size_t datalen,
    const char *alg, const char *sk_provider, const char *sk_pin, u_int compat)
{
	DSA_SIG *sig = NULL;
	const BIGNUM *sig_r, *sig_s;
	u_char digest[SSH_DIGEST_MAX_LENGTH], sigblob[SIGBLOB_LEN];
	size_t rlen, slen, len, dlen = ssh_digest_bytes(SSH_DIGEST_SHA1);
	struct sshbuf *b = NULL;
	int ret = SSH_ERR_INVALID_ARGUMENT;

	if (lenp != NULL)
		*lenp = 0;
	if (sigp != NULL)
		*sigp = NULL;

	if (key == NULL || key->dsa == NULL ||
	    sshkey_type_plain(key->type) != KEY_DSA)
		return SSH_ERR_INVALID_ARGUMENT;
	if (dlen == 0)
		return SSH_ERR_INTERNAL_ERROR;

	if ((ret = ssh_digest_memory(SSH_DIGEST_SHA1, data, datalen,
	    digest, sizeof(digest))) != 0)
		goto out;

	if ((sig = DSA_do_sign(digest, dlen, key->dsa)) == NULL) {
		ret = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}

	DSA_SIG_get0(sig, &sig_r, &sig_s);
	rlen = BN_num_bytes(sig_r);
	slen = BN_num_bytes(sig_s);
	if (rlen > INTBLOB_LEN || slen > INTBLOB_LEN) {
		ret = SSH_ERR_INTERNAL_ERROR;
		goto out;
	}
	explicit_bzero(sigblob, SIGBLOB_LEN);
	BN_bn2bin(sig_r, sigblob + SIGBLOB_LEN - INTBLOB_LEN - rlen);
	BN_bn2bin(sig_s, sigblob + SIGBLOB_LEN - slen);

	if ((b = sshbuf_new()) == NULL) {
		ret = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((ret = sshbuf_put_cstring(b, "ssh-dss")) != 0 ||
	    (ret = sshbuf_put_string(b, sigblob, SIGBLOB_LEN)) != 0)
		goto out;

	len = sshbuf_len(b);
	if (sigp != NULL) {
		if ((*sigp = malloc(len)) == NULL) {
			ret = SSH_ERR_ALLOC_FAIL;
			goto out;
		}
		memcpy(*sigp, sshbuf_ptr(b), len);
	}
	if (lenp != NULL)
		*lenp = len;
	ret = 0;
 out:
	explicit_bzero(digest, sizeof(digest));
	DSA_SIG_free(sig);
	sshbuf_free(b);
	return ret;
}

static int
ssh_dss_verify(const struct sshkey *key,
    const u_char *sig, size_t siglen,
    const u_char *data, size_t dlen, const char *alg, u_int compat,
    struct sshkey_sig_details **detailsp)
{
	DSA_SIG *dsig = NULL;
	BIGNUM *sig_r = NULL, *sig_s = NULL;
	u_char digest[SSH_DIGEST_MAX_LENGTH], *sigblob = NULL;
	size_t len, hlen = ssh_digest_bytes(SSH_DIGEST_SHA1);
	int ret = SSH_ERR_INTERNAL_ERROR;
	struct sshbuf *b = NULL;
	char *ktype = NULL;

	if (key == NULL || key->dsa == NULL ||
	    sshkey_type_plain(key->type) != KEY_DSA ||
	    sig == NULL || siglen == 0)
		return SSH_ERR_INVALID_ARGUMENT;
	if (hlen == 0)
		return SSH_ERR_INTERNAL_ERROR;

	/* fetch signature */
	if ((b = sshbuf_from(sig, siglen)) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if (sshbuf_get_cstring(b, &ktype, NULL) != 0 ||
	    sshbuf_get_string(b, &sigblob, &len) != 0) {
		ret = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if (strcmp("ssh-dss", ktype) != 0) {
		ret = SSH_ERR_KEY_TYPE_MISMATCH;
		goto out;
	}
	if (sshbuf_len(b) != 0) {
		ret = SSH_ERR_UNEXPECTED_TRAILING_DATA;
		goto out;
	}

	if (len != SIGBLOB_LEN) {
		ret = SSH_ERR_INVALID_FORMAT;
		goto out;
	}

	/* parse signature */
	if ((dsig = DSA_SIG_new()) == NULL ||
	    (sig_r = BN_new()) == NULL ||
	    (sig_s = BN_new()) == NULL) {
		ret = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((BN_bin2bn(sigblob, INTBLOB_LEN, sig_r) == NULL) ||
	    (BN_bin2bn(sigblob + INTBLOB_LEN, INTBLOB_LEN, sig_s) == NULL)) {
		ret = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	if (!DSA_SIG_set0(dsig, sig_r, sig_s)) {
		ret = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	sig_r = sig_s = NULL; /* transferred */

	/* sha1 the data */
	if ((ret = ssh_digest_memory(SSH_DIGEST_SHA1, data, dlen,
	    digest, sizeof(digest))) != 0)
		goto out;

	switch (DSA_do_verify(digest, hlen, dsig, key->dsa)) {
	case 1:
		ret = 0;
		break;
	case 0:
		ret = SSH_ERR_SIGNATURE_INVALID;
		goto out;
	default:
		ret = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}

 out:
	explicit_bzero(digest, sizeof(digest));
	DSA_SIG_free(dsig);
	BN_clear_free(sig_r);
	BN_clear_free(sig_s);
	sshbuf_free(b);
	free(ktype);
	if (sigblob != NULL)
		freezero(sigblob, len);
	return ret;
}

static const struct sshkey_impl_funcs sshkey_dss_funcs = {
	/* .size = */		ssh_dss_size,
	/* .alloc = */		ssh_dss_alloc,
	/* .cleanup = */	ssh_dss_cleanup,
	/* .equal = */		ssh_dss_equal,
	/* .ssh_serialize_public = */ ssh_dss_serialize_public,
	/* .ssh_deserialize_public = */ ssh_dss_deserialize_public,
	/* .ssh_serialize_private = */ ssh_dss_serialize_private,
	/* .ssh_deserialize_private = */ ssh_dss_deserialize_private,
	/* .generate = */	ssh_dss_generate,
	/* .copy_public = */	ssh_dss_copy_public,
	/* .sign = */		ssh_dss_sign,
	/* .verify = */		ssh_dss_verify,
};

const struct sshkey_impl sshkey_dss_impl = {
	/* .name = */		"ssh-dss",
	/* .shortname = */	"DSA",
	/* .sigalg = */		NULL,
	/* .type = */		KEY_DSA,
	/* .nid = */		0,
	/* .cert = */		0,
	/* .sigonly = */	0,
	/* .keybits = */	0,
	/* .funcs = */		&sshkey_dss_funcs,
};

const struct sshkey_impl sshkey_dsa_cert_impl = {
	/* .name = */		"ssh-dss-cert-v01@openssh.com",
	/* .shortname = */	"DSA-CERT",
	/* .sigalg = */		NULL,
	/* .type = */		KEY_DSA_CERT,
	/* .nid = */		0,
	/* .cert = */		1,
	/* .sigonly = */	0,
	/* .keybits = */	0,
	/* .funcs = */		&sshkey_dss_funcs,
};

#endif /* WITH_OPENSSL && WITH_DSA */
