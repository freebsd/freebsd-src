/* $OpenBSD: ssh-rsa.c,v 1.79 2023/03/05 05:34:09 dtucker Exp $ */
/*
 * Copyright (c) 2000, 2003 Markus Friedl <markus@openbsd.org>
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

#include "includes.h"

#ifdef WITH_OPENSSL

#include <sys/types.h>

#include <openssl/evp.h>
#include <openssl/err.h>

#include <stdarg.h>
#include <string.h>

#include "sshbuf.h"
#include "ssherr.h"
#define SSHKEY_INTERNAL
#include "sshkey.h"
#include "digest.h"
#include "log.h"

#include "openbsd-compat/openssl-compat.h"

static int openssh_RSA_verify(int, u_char *, size_t, u_char *, size_t, RSA *);

static u_int
ssh_rsa_size(const struct sshkey *key)
{
	const BIGNUM *rsa_n;

	if (key->rsa == NULL)
		return 0;
	RSA_get0_key(key->rsa, &rsa_n, NULL, NULL);
	return BN_num_bits(rsa_n);
}

static int
ssh_rsa_alloc(struct sshkey *k)
{
	if ((k->rsa = RSA_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	return 0;
}

static void
ssh_rsa_cleanup(struct sshkey *k)
{
	RSA_free(k->rsa);
	k->rsa = NULL;
}

static int
ssh_rsa_equal(const struct sshkey *a, const struct sshkey *b)
{
	const BIGNUM *rsa_e_a, *rsa_n_a;
	const BIGNUM *rsa_e_b, *rsa_n_b;

	if (a->rsa == NULL || b->rsa == NULL)
		return 0;
	RSA_get0_key(a->rsa, &rsa_n_a, &rsa_e_a, NULL);
	RSA_get0_key(b->rsa, &rsa_n_b, &rsa_e_b, NULL);
	if (rsa_e_a == NULL || rsa_e_b == NULL)
		return 0;
	if (rsa_n_a == NULL || rsa_n_b == NULL)
		return 0;
	if (BN_cmp(rsa_e_a, rsa_e_b) != 0)
		return 0;
	if (BN_cmp(rsa_n_a, rsa_n_b) != 0)
		return 0;
	return 1;
}

static int
ssh_rsa_serialize_public(const struct sshkey *key, struct sshbuf *b,
    enum sshkey_serialize_rep opts)
{
	int r;
	const BIGNUM *rsa_n, *rsa_e;

	if (key->rsa == NULL)
		return SSH_ERR_INVALID_ARGUMENT;
	RSA_get0_key(key->rsa, &rsa_n, &rsa_e, NULL);
	if ((r = sshbuf_put_bignum2(b, rsa_e)) != 0 ||
	    (r = sshbuf_put_bignum2(b, rsa_n)) != 0)
		return r;

	return 0;
}

static int
ssh_rsa_serialize_private(const struct sshkey *key, struct sshbuf *b,
    enum sshkey_serialize_rep opts)
{
	int r;
	const BIGNUM *rsa_n, *rsa_e, *rsa_d, *rsa_iqmp, *rsa_p, *rsa_q;

	RSA_get0_key(key->rsa, &rsa_n, &rsa_e, &rsa_d);
	RSA_get0_factors(key->rsa, &rsa_p, &rsa_q);
	RSA_get0_crt_params(key->rsa, NULL, NULL, &rsa_iqmp);

	if (!sshkey_is_cert(key)) {
		/* Note: can't reuse ssh_rsa_serialize_public: e, n vs. n, e */
		if ((r = sshbuf_put_bignum2(b, rsa_n)) != 0 ||
		    (r = sshbuf_put_bignum2(b, rsa_e)) != 0)
			return r;
	}
	if ((r = sshbuf_put_bignum2(b, rsa_d)) != 0 ||
	    (r = sshbuf_put_bignum2(b, rsa_iqmp)) != 0 ||
	    (r = sshbuf_put_bignum2(b, rsa_p)) != 0 ||
	    (r = sshbuf_put_bignum2(b, rsa_q)) != 0)
		return r;

	return 0;
}

static int
ssh_rsa_generate(struct sshkey *k, int bits)
{
	RSA *private = NULL;
	BIGNUM *f4 = NULL;
	int ret = SSH_ERR_INTERNAL_ERROR;

	if (bits < SSH_RSA_MINIMUM_MODULUS_SIZE ||
	    bits > SSHBUF_MAX_BIGNUM * 8)
		return SSH_ERR_KEY_LENGTH;
	if ((private = RSA_new()) == NULL || (f4 = BN_new()) == NULL) {
		ret = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if (!BN_set_word(f4, RSA_F4) ||
	    !RSA_generate_key_ex(private, bits, f4, NULL)) {
		ret = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	k->rsa = private;
	private = NULL;
	ret = 0;
 out:
	RSA_free(private);
	BN_free(f4);
	return ret;
}

static int
ssh_rsa_copy_public(const struct sshkey *from, struct sshkey *to)
{
	const BIGNUM *rsa_n, *rsa_e;
	BIGNUM *rsa_n_dup = NULL, *rsa_e_dup = NULL;
	int r = SSH_ERR_INTERNAL_ERROR;

	RSA_get0_key(from->rsa, &rsa_n, &rsa_e, NULL);
	if ((rsa_n_dup = BN_dup(rsa_n)) == NULL ||
	    (rsa_e_dup = BN_dup(rsa_e)) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if (!RSA_set0_key(to->rsa, rsa_n_dup, rsa_e_dup, NULL)) {
		r = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	rsa_n_dup = rsa_e_dup = NULL; /* transferred */
	/* success */
	r = 0;
 out:
	BN_clear_free(rsa_n_dup);
	BN_clear_free(rsa_e_dup);
	return r;
}

static int
ssh_rsa_deserialize_public(const char *ktype, struct sshbuf *b,
    struct sshkey *key)
{
	int ret = SSH_ERR_INTERNAL_ERROR;
	BIGNUM *rsa_n = NULL, *rsa_e = NULL;

	if (sshbuf_get_bignum2(b, &rsa_e) != 0 ||
	    sshbuf_get_bignum2(b, &rsa_n) != 0) {
		ret = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if (!RSA_set0_key(key->rsa, rsa_n, rsa_e, NULL)) {
		ret = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	rsa_n = rsa_e = NULL; /* transferred */
	if ((ret = sshkey_check_rsa_length(key, 0)) != 0)
		goto out;
#ifdef DEBUG_PK
	RSA_print_fp(stderr, key->rsa, 8);
#endif
	/* success */
	ret = 0;
 out:
	BN_clear_free(rsa_n);
	BN_clear_free(rsa_e);
	return ret;
}

static int
ssh_rsa_deserialize_private(const char *ktype, struct sshbuf *b,
    struct sshkey *key)
{
	int r;
	BIGNUM *rsa_n = NULL, *rsa_e = NULL, *rsa_d = NULL;
	BIGNUM *rsa_iqmp = NULL, *rsa_p = NULL, *rsa_q = NULL;

	/* Note: can't reuse ssh_rsa_deserialize_public: e, n vs. n, e */
	if (!sshkey_is_cert(key)) {
		if ((r = sshbuf_get_bignum2(b, &rsa_n)) != 0 ||
		    (r = sshbuf_get_bignum2(b, &rsa_e)) != 0)
			goto out;
		if (!RSA_set0_key(key->rsa, rsa_n, rsa_e, NULL)) {
			r = SSH_ERR_LIBCRYPTO_ERROR;
			goto out;
		}
		rsa_n = rsa_e = NULL; /* transferred */
	}
	if ((r = sshbuf_get_bignum2(b, &rsa_d)) != 0 ||
	    (r = sshbuf_get_bignum2(b, &rsa_iqmp)) != 0 ||
	    (r = sshbuf_get_bignum2(b, &rsa_p)) != 0 ||
	    (r = sshbuf_get_bignum2(b, &rsa_q)) != 0)
		goto out;
	if (!RSA_set0_key(key->rsa, NULL, NULL, rsa_d)) {
		r = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	rsa_d = NULL; /* transferred */
	if (!RSA_set0_factors(key->rsa, rsa_p, rsa_q)) {
		r = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	rsa_p = rsa_q = NULL; /* transferred */
	if ((r = sshkey_check_rsa_length(key, 0)) != 0)
		goto out;
	if ((r = ssh_rsa_complete_crt_parameters(key, rsa_iqmp)) != 0)
		goto out;
	if (RSA_blinding_on(key->rsa, NULL) != 1) {
		r = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	/* success */
	r = 0;
 out:
	BN_clear_free(rsa_n);
	BN_clear_free(rsa_e);
	BN_clear_free(rsa_d);
	BN_clear_free(rsa_p);
	BN_clear_free(rsa_q);
	BN_clear_free(rsa_iqmp);
	return r;
}

static const char *
rsa_hash_alg_ident(int hash_alg)
{
	switch (hash_alg) {
	case SSH_DIGEST_SHA1:
		return "ssh-rsa";
	case SSH_DIGEST_SHA256:
		return "rsa-sha2-256";
	case SSH_DIGEST_SHA512:
		return "rsa-sha2-512";
	}
	return NULL;
}

/*
 * Returns the hash algorithm ID for a given algorithm identifier as used
 * inside the signature blob,
 */
static int
rsa_hash_id_from_ident(const char *ident)
{
	if (strcmp(ident, "ssh-rsa") == 0)
		return SSH_DIGEST_SHA1;
	if (strcmp(ident, "rsa-sha2-256") == 0)
		return SSH_DIGEST_SHA256;
	if (strcmp(ident, "rsa-sha2-512") == 0)
		return SSH_DIGEST_SHA512;
	return -1;
}

/*
 * Return the hash algorithm ID for the specified key name. This includes
 * all the cases of rsa_hash_id_from_ident() but also the certificate key
 * types.
 */
static int
rsa_hash_id_from_keyname(const char *alg)
{
	int r;

	if ((r = rsa_hash_id_from_ident(alg)) != -1)
		return r;
	if (strcmp(alg, "ssh-rsa-cert-v01@openssh.com") == 0)
		return SSH_DIGEST_SHA1;
	if (strcmp(alg, "rsa-sha2-256-cert-v01@openssh.com") == 0)
		return SSH_DIGEST_SHA256;
	if (strcmp(alg, "rsa-sha2-512-cert-v01@openssh.com") == 0)
		return SSH_DIGEST_SHA512;
	return -1;
}

static int
rsa_hash_alg_nid(int type)
{
	switch (type) {
	case SSH_DIGEST_SHA1:
		return NID_sha1;
	case SSH_DIGEST_SHA256:
		return NID_sha256;
	case SSH_DIGEST_SHA512:
		return NID_sha512;
	default:
		return -1;
	}
}

int
ssh_rsa_complete_crt_parameters(struct sshkey *key, const BIGNUM *iqmp)
{
	const BIGNUM *rsa_p, *rsa_q, *rsa_d;
	BIGNUM *aux = NULL, *d_consttime = NULL;
	BIGNUM *rsa_dmq1 = NULL, *rsa_dmp1 = NULL, *rsa_iqmp = NULL;
	BN_CTX *ctx = NULL;
	int r;

	if (key == NULL || key->rsa == NULL ||
	    sshkey_type_plain(key->type) != KEY_RSA)
		return SSH_ERR_INVALID_ARGUMENT;

	RSA_get0_key(key->rsa, NULL, NULL, &rsa_d);
	RSA_get0_factors(key->rsa, &rsa_p, &rsa_q);

	if ((ctx = BN_CTX_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if ((aux = BN_new()) == NULL ||
	    (rsa_dmq1 = BN_new()) == NULL ||
	    (rsa_dmp1 = BN_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if ((d_consttime = BN_dup(rsa_d)) == NULL ||
	    (rsa_iqmp = BN_dup(iqmp)) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	BN_set_flags(aux, BN_FLG_CONSTTIME);
	BN_set_flags(d_consttime, BN_FLG_CONSTTIME);

	if ((BN_sub(aux, rsa_q, BN_value_one()) == 0) ||
	    (BN_mod(rsa_dmq1, d_consttime, aux, ctx) == 0) ||
	    (BN_sub(aux, rsa_p, BN_value_one()) == 0) ||
	    (BN_mod(rsa_dmp1, d_consttime, aux, ctx) == 0)) {
		r = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	if (!RSA_set0_crt_params(key->rsa, rsa_dmp1, rsa_dmq1, rsa_iqmp)) {
		r = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	rsa_dmp1 = rsa_dmq1 = rsa_iqmp = NULL; /* transferred */
	/* success */
	r = 0;
 out:
	BN_clear_free(aux);
	BN_clear_free(d_consttime);
	BN_clear_free(rsa_dmp1);
	BN_clear_free(rsa_dmq1);
	BN_clear_free(rsa_iqmp);
	BN_CTX_free(ctx);
	return r;
}

/* RSASSA-PKCS1-v1_5 (PKCS #1 v2.0 signature) with SHA1 */
static int
ssh_rsa_sign(struct sshkey *key,
    u_char **sigp, size_t *lenp,
    const u_char *data, size_t datalen,
    const char *alg, const char *sk_provider, const char *sk_pin, u_int compat)
{
	const BIGNUM *rsa_n;
	u_char digest[SSH_DIGEST_MAX_LENGTH], *sig = NULL;
	size_t slen = 0;
	u_int hlen, len;
	int nid, hash_alg, ret = SSH_ERR_INTERNAL_ERROR;
	struct sshbuf *b = NULL;

	if (lenp != NULL)
		*lenp = 0;
	if (sigp != NULL)
		*sigp = NULL;

	if (alg == NULL || strlen(alg) == 0)
		hash_alg = SSH_DIGEST_SHA1;
	else
		hash_alg = rsa_hash_id_from_keyname(alg);
	if (key == NULL || key->rsa == NULL || hash_alg == -1 ||
	    sshkey_type_plain(key->type) != KEY_RSA)
		return SSH_ERR_INVALID_ARGUMENT;
	RSA_get0_key(key->rsa, &rsa_n, NULL, NULL);
	if (BN_num_bits(rsa_n) < SSH_RSA_MINIMUM_MODULUS_SIZE)
		return SSH_ERR_KEY_LENGTH;
	slen = RSA_size(key->rsa);
	if (slen <= 0 || slen > SSHBUF_MAX_BIGNUM)
		return SSH_ERR_INVALID_ARGUMENT;

	/* hash the data */
	nid = rsa_hash_alg_nid(hash_alg);
	if ((hlen = ssh_digest_bytes(hash_alg)) == 0)
		return SSH_ERR_INTERNAL_ERROR;
	if ((ret = ssh_digest_memory(hash_alg, data, datalen,
	    digest, sizeof(digest))) != 0)
		goto out;

	if ((sig = malloc(slen)) == NULL) {
		ret = SSH_ERR_ALLOC_FAIL;
		goto out;
	}

	if (RSA_sign(nid, digest, hlen, sig, &len, key->rsa) != 1) {
		ret = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	if (len < slen) {
		size_t diff = slen - len;
		memmove(sig + diff, sig, len);
		explicit_bzero(sig, diff);
	} else if (len > slen) {
		ret = SSH_ERR_INTERNAL_ERROR;
		goto out;
	}
	/* encode signature */
	if ((b = sshbuf_new()) == NULL) {
		ret = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((ret = sshbuf_put_cstring(b, rsa_hash_alg_ident(hash_alg))) != 0 ||
	    (ret = sshbuf_put_string(b, sig, slen)) != 0)
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
	freezero(sig, slen);
	sshbuf_free(b);
	return ret;
}

static int
ssh_rsa_verify(const struct sshkey *key,
    const u_char *sig, size_t siglen,
    const u_char *data, size_t dlen, const char *alg, u_int compat,
    struct sshkey_sig_details **detailsp)
{
	const BIGNUM *rsa_n;
	char *sigtype = NULL;
	int hash_alg, want_alg, ret = SSH_ERR_INTERNAL_ERROR;
	size_t len = 0, diff, modlen, hlen;
	struct sshbuf *b = NULL;
	u_char digest[SSH_DIGEST_MAX_LENGTH], *osigblob, *sigblob = NULL;

	if (key == NULL || key->rsa == NULL ||
	    sshkey_type_plain(key->type) != KEY_RSA ||
	    sig == NULL || siglen == 0)
		return SSH_ERR_INVALID_ARGUMENT;
	RSA_get0_key(key->rsa, &rsa_n, NULL, NULL);
	if (BN_num_bits(rsa_n) < SSH_RSA_MINIMUM_MODULUS_SIZE)
		return SSH_ERR_KEY_LENGTH;

	if ((b = sshbuf_from(sig, siglen)) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if (sshbuf_get_cstring(b, &sigtype, NULL) != 0) {
		ret = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if ((hash_alg = rsa_hash_id_from_ident(sigtype)) == -1) {
		ret = SSH_ERR_KEY_TYPE_MISMATCH;
		goto out;
	}
	/*
	 * Allow ssh-rsa-cert-v01 certs to generate SHA2 signatures for
	 * legacy reasons, but otherwise the signature type should match.
	 */
	if (alg != NULL && strcmp(alg, "ssh-rsa-cert-v01@openssh.com") != 0) {
		if ((want_alg = rsa_hash_id_from_keyname(alg)) == -1) {
			ret = SSH_ERR_INVALID_ARGUMENT;
			goto out;
		}
		if (hash_alg != want_alg) {
			ret = SSH_ERR_SIGNATURE_INVALID;
			goto out;
		}
	}
	if (sshbuf_get_string(b, &sigblob, &len) != 0) {
		ret = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if (sshbuf_len(b) != 0) {
		ret = SSH_ERR_UNEXPECTED_TRAILING_DATA;
		goto out;
	}
	/* RSA_verify expects a signature of RSA_size */
	modlen = RSA_size(key->rsa);
	if (len > modlen) {
		ret = SSH_ERR_KEY_BITS_MISMATCH;
		goto out;
	} else if (len < modlen) {
		diff = modlen - len;
		osigblob = sigblob;
		if ((sigblob = realloc(sigblob, modlen)) == NULL) {
			sigblob = osigblob; /* put it back for clear/free */
			ret = SSH_ERR_ALLOC_FAIL;
			goto out;
		}
		memmove(sigblob + diff, sigblob, len);
		explicit_bzero(sigblob, diff);
		len = modlen;
	}
	if ((hlen = ssh_digest_bytes(hash_alg)) == 0) {
		ret = SSH_ERR_INTERNAL_ERROR;
		goto out;
	}
	if ((ret = ssh_digest_memory(hash_alg, data, dlen,
	    digest, sizeof(digest))) != 0)
		goto out;

	ret = openssh_RSA_verify(hash_alg, digest, hlen, sigblob, len,
	    key->rsa);
 out:
	freezero(sigblob, len);
	free(sigtype);
	sshbuf_free(b);
	explicit_bzero(digest, sizeof(digest));
	return ret;
}

/*
 * See:
 * http://www.rsasecurity.com/rsalabs/pkcs/pkcs-1/
 * ftp://ftp.rsasecurity.com/pub/pkcs/pkcs-1/pkcs-1v2-1.asn
 */

/*
 * id-sha1 OBJECT IDENTIFIER ::= { iso(1) identified-organization(3)
 *	oiw(14) secsig(3) algorithms(2) 26 }
 */
static const u_char id_sha1[] = {
	0x30, 0x21, /* type Sequence, length 0x21 (33) */
	0x30, 0x09, /* type Sequence, length 0x09 */
	0x06, 0x05, /* type OID, length 0x05 */
	0x2b, 0x0e, 0x03, 0x02, 0x1a, /* id-sha1 OID */
	0x05, 0x00, /* NULL */
	0x04, 0x14  /* Octet string, length 0x14 (20), followed by sha1 hash */
};

/*
 * See http://csrc.nist.gov/groups/ST/crypto_apps_infra/csor/algorithms.html
 * id-sha256 OBJECT IDENTIFIER ::= { joint-iso-itu-t(2) country(16) us(840)
 *      organization(1) gov(101) csor(3) nistAlgorithm(4) hashAlgs(2)
 *      id-sha256(1) }
 */
static const u_char id_sha256[] = {
	0x30, 0x31, /* type Sequence, length 0x31 (49) */
	0x30, 0x0d, /* type Sequence, length 0x0d (13) */
	0x06, 0x09, /* type OID, length 0x09 */
	0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, /* id-sha256 */
	0x05, 0x00, /* NULL */
	0x04, 0x20  /* Octet string, length 0x20 (32), followed by sha256 hash */
};

/*
 * See http://csrc.nist.gov/groups/ST/crypto_apps_infra/csor/algorithms.html
 * id-sha512 OBJECT IDENTIFIER ::= { joint-iso-itu-t(2) country(16) us(840)
 *      organization(1) gov(101) csor(3) nistAlgorithm(4) hashAlgs(2)
 *      id-sha256(3) }
 */
static const u_char id_sha512[] = {
	0x30, 0x51, /* type Sequence, length 0x51 (81) */
	0x30, 0x0d, /* type Sequence, length 0x0d (13) */
	0x06, 0x09, /* type OID, length 0x09 */
	0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03, /* id-sha512 */
	0x05, 0x00, /* NULL */
	0x04, 0x40  /* Octet string, length 0x40 (64), followed by sha512 hash */
};

static int
rsa_hash_alg_oid(int hash_alg, const u_char **oidp, size_t *oidlenp)
{
	switch (hash_alg) {
	case SSH_DIGEST_SHA1:
		*oidp = id_sha1;
		*oidlenp = sizeof(id_sha1);
		break;
	case SSH_DIGEST_SHA256:
		*oidp = id_sha256;
		*oidlenp = sizeof(id_sha256);
		break;
	case SSH_DIGEST_SHA512:
		*oidp = id_sha512;
		*oidlenp = sizeof(id_sha512);
		break;
	default:
		return SSH_ERR_INVALID_ARGUMENT;
	}
	return 0;
}

static int
openssh_RSA_verify(int hash_alg, u_char *hash, size_t hashlen,
    u_char *sigbuf, size_t siglen, RSA *rsa)
{
	size_t rsasize = 0, oidlen = 0, hlen = 0;
	int ret, len, oidmatch, hashmatch;
	const u_char *oid = NULL;
	u_char *decrypted = NULL;

	if ((ret = rsa_hash_alg_oid(hash_alg, &oid, &oidlen)) != 0)
		return ret;
	ret = SSH_ERR_INTERNAL_ERROR;
	hlen = ssh_digest_bytes(hash_alg);
	if (hashlen != hlen) {
		ret = SSH_ERR_INVALID_ARGUMENT;
		goto done;
	}
	rsasize = RSA_size(rsa);
	if (rsasize <= 0 || rsasize > SSHBUF_MAX_BIGNUM ||
	    siglen == 0 || siglen > rsasize) {
		ret = SSH_ERR_INVALID_ARGUMENT;
		goto done;
	}
	if ((decrypted = malloc(rsasize)) == NULL) {
		ret = SSH_ERR_ALLOC_FAIL;
		goto done;
	}
	if ((len = RSA_public_decrypt(siglen, sigbuf, decrypted, rsa,
	    RSA_PKCS1_PADDING)) < 0) {
		ret = SSH_ERR_LIBCRYPTO_ERROR;
		goto done;
	}
	if (len < 0 || (size_t)len != hlen + oidlen) {
		ret = SSH_ERR_INVALID_FORMAT;
		goto done;
	}
	oidmatch = timingsafe_bcmp(decrypted, oid, oidlen) == 0;
	hashmatch = timingsafe_bcmp(decrypted + oidlen, hash, hlen) == 0;
	if (!oidmatch || !hashmatch) {
		ret = SSH_ERR_SIGNATURE_INVALID;
		goto done;
	}
	ret = 0;
done:
	freezero(decrypted, rsasize);
	return ret;
}

static const struct sshkey_impl_funcs sshkey_rsa_funcs = {
	/* .size = */		ssh_rsa_size,
	/* .alloc = */		ssh_rsa_alloc,
	/* .cleanup = */	ssh_rsa_cleanup,
	/* .equal = */		ssh_rsa_equal,
	/* .ssh_serialize_public = */ ssh_rsa_serialize_public,
	/* .ssh_deserialize_public = */ ssh_rsa_deserialize_public,
	/* .ssh_serialize_private = */ ssh_rsa_serialize_private,
	/* .ssh_deserialize_private = */ ssh_rsa_deserialize_private,
	/* .generate = */	ssh_rsa_generate,
	/* .copy_public = */	ssh_rsa_copy_public,
	/* .sign = */		ssh_rsa_sign,
	/* .verify = */		ssh_rsa_verify,
};

const struct sshkey_impl sshkey_rsa_impl = {
	/* .name = */		"ssh-rsa",
	/* .shortname = */	"RSA",
	/* .sigalg = */		NULL,
	/* .type = */		KEY_RSA,
	/* .nid = */		0,
	/* .cert = */		0,
	/* .sigonly = */	0,
	/* .keybits = */	0,
	/* .funcs = */		&sshkey_rsa_funcs,
};

const struct sshkey_impl sshkey_rsa_cert_impl = {
	/* .name = */		"ssh-rsa-cert-v01@openssh.com",
	/* .shortname = */	"RSA-CERT",
	/* .sigalg = */		NULL,
	/* .type = */		KEY_RSA_CERT,
	/* .nid = */		0,
	/* .cert = */		1,
	/* .sigonly = */	0,
	/* .keybits = */	0,
	/* .funcs = */		&sshkey_rsa_funcs,
};

/* SHA2 signature algorithms */

const struct sshkey_impl sshkey_rsa_sha256_impl = {
	/* .name = */		"rsa-sha2-256",
	/* .shortname = */	"RSA",
	/* .sigalg = */		NULL,
	/* .type = */		KEY_RSA,
	/* .nid = */		0,
	/* .cert = */		0,
	/* .sigonly = */	1,
	/* .keybits = */	0,
	/* .funcs = */		&sshkey_rsa_funcs,
};

const struct sshkey_impl sshkey_rsa_sha512_impl = {
	/* .name = */		"rsa-sha2-512",
	/* .shortname = */	"RSA",
	/* .sigalg = */		NULL,
	/* .type = */		KEY_RSA,
	/* .nid = */		0,
	/* .cert = */		0,
	/* .sigonly = */	1,
	/* .keybits = */	0,
	/* .funcs = */		&sshkey_rsa_funcs,
};

const struct sshkey_impl sshkey_rsa_sha256_cert_impl = {
	/* .name = */		"rsa-sha2-256-cert-v01@openssh.com",
	/* .shortname = */	"RSA-CERT",
	/* .sigalg = */		"rsa-sha2-256",
	/* .type = */		KEY_RSA_CERT,
	/* .nid = */		0,
	/* .cert = */		1,
	/* .sigonly = */	1,
	/* .keybits = */	0,
	/* .funcs = */		&sshkey_rsa_funcs,
};

const struct sshkey_impl sshkey_rsa_sha512_cert_impl = {
	/* .name = */		"rsa-sha2-512-cert-v01@openssh.com",
	/* .shortname = */	"RSA-CERT",
	/* .sigalg = */		"rsa-sha2-512",
	/* .type = */		KEY_RSA_CERT,
	/* .nid = */		0,
	/* .cert = */		1,
	/* .sigonly = */	1,
	/* .keybits = */	0,
	/* .funcs = */		&sshkey_rsa_funcs,
};
#endif /* WITH_OPENSSL */
