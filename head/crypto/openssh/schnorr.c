/* $OpenBSD: schnorr.c,v 1.5 2010/12/03 23:49:26 djm Exp $ */
/* $FreeBSD$ */
/*
 * Copyright (c) 2008 Damien Miller.  All rights reserved.
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

/*
 * Implementation of Schnorr signatures / zero-knowledge proofs, based on
 * description in:
 * 	
 * F. Hao, P. Ryan, "Password Authenticated Key Exchange by Juggling",
 * 16th Workshop on Security Protocols, Cambridge, April 2008
 *
 * http://grouper.ieee.org/groups/1363/Research/contributions/hao-ryan-2008.pdf
 */

#include "includes.h"

#include <sys/types.h>

#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include <openssl/evp.h>
#include <openssl/bn.h>

#include "xmalloc.h"
#include "buffer.h"
#include "log.h"

#include "schnorr.h"

#ifdef JPAKE

#include "openbsd-compat/openssl-compat.h"

/* #define SCHNORR_DEBUG */		/* Privacy-violating debugging */
/* #define SCHNORR_MAIN */		/* Include main() selftest */

#ifndef SCHNORR_DEBUG
# define SCHNORR_DEBUG_BN(a)
# define SCHNORR_DEBUG_BUF(a)
#else
# define SCHNORR_DEBUG_BN(a)	debug3_bn a
# define SCHNORR_DEBUG_BUF(a)	debug3_buf a
#endif /* SCHNORR_DEBUG */

/*
 * Calculate hash component of Schnorr signature H(g || g^v || g^x || id)
 * using the hash function defined by "evp_md". Returns signature as
 * bignum or NULL on error.
 */
static BIGNUM *
schnorr_hash(const BIGNUM *p, const BIGNUM *q, const BIGNUM *g,
    const EVP_MD *evp_md, const BIGNUM *g_v, const BIGNUM *g_x,
    const u_char *id, u_int idlen)
{
	u_char *digest;
	u_int digest_len;
	BIGNUM *h;
	Buffer b;
	int success = -1;

	if ((h = BN_new()) == NULL) {
		error("%s: BN_new", __func__);
		return NULL;
	}

	buffer_init(&b);

	/* h = H(g || p || q || g^v || g^x || id) */
	buffer_put_bignum2(&b, g);
	buffer_put_bignum2(&b, p);
	buffer_put_bignum2(&b, q);
	buffer_put_bignum2(&b, g_v);
	buffer_put_bignum2(&b, g_x);
	buffer_put_string(&b, id, idlen);

	SCHNORR_DEBUG_BUF((buffer_ptr(&b), buffer_len(&b),
	    "%s: hashblob", __func__));
	if (hash_buffer(buffer_ptr(&b), buffer_len(&b), evp_md,
	    &digest, &digest_len) != 0) {
		error("%s: hash_buffer", __func__);
		goto out;
	}
	if (BN_bin2bn(digest, (int)digest_len, h) == NULL) {
		error("%s: BN_bin2bn", __func__);
		goto out;
	}
	success = 0;
	SCHNORR_DEBUG_BN((h, "%s: h = ", __func__));
 out:
	buffer_free(&b);
	bzero(digest, digest_len);
	xfree(digest);
	digest_len = 0;
	if (success == 0)
		return h;
	BN_clear_free(h);
	return NULL;
}

/*
 * Generate Schnorr signature to prove knowledge of private value 'x' used
 * in public exponent g^x, under group defined by 'grp_p', 'grp_q' and 'grp_g'
 * using the hash function "evp_md".
 * 'idlen' bytes from 'id' will be included in the signature hash as an anti-
 * replay salt.
 * 
 * On success, 0 is returned. The signature values are returned as *e_p
 * (g^v mod p) and *r_p (v - xh mod q). The caller must free these values.
 * On failure, -1 is returned.
 */
int
schnorr_sign(const BIGNUM *grp_p, const BIGNUM *grp_q, const BIGNUM *grp_g,
    const EVP_MD *evp_md, const BIGNUM *x, const BIGNUM *g_x,
    const u_char *id, u_int idlen, BIGNUM **r_p, BIGNUM **e_p)
{
	int success = -1;
	BIGNUM *h, *tmp, *v, *g_v, *r;
	BN_CTX *bn_ctx;

	SCHNORR_DEBUG_BN((x, "%s: x = ", __func__));
	SCHNORR_DEBUG_BN((g_x, "%s: g_x = ", __func__));

	/* Avoid degenerate cases: g^0 yields a spoofable signature */
	if (BN_cmp(g_x, BN_value_one()) <= 0) {
		error("%s: g_x < 1", __func__);
		return -1;
	}
	if (BN_cmp(g_x, grp_p) >= 0) {
		error("%s: g_x > g", __func__);
		return -1;
	}

	h = g_v = r = tmp = v = NULL;
	if ((bn_ctx = BN_CTX_new()) == NULL) {
		error("%s: BN_CTX_new", __func__);
		goto out;
	}
	if ((g_v = BN_new()) == NULL ||
	    (r = BN_new()) == NULL ||
	    (tmp = BN_new()) == NULL) {
		error("%s: BN_new", __func__);
		goto out;
	}

	/*
	 * v must be a random element of Zq, so 1 <= v < q
	 * we also exclude v = 1, since g^1 looks dangerous
	 */
	if ((v = bn_rand_range_gt_one(grp_p)) == NULL) {
		error("%s: bn_rand_range2", __func__);
		goto out;
	}
	SCHNORR_DEBUG_BN((v, "%s: v = ", __func__));

	/* g_v = g^v mod p */
	if (BN_mod_exp(g_v, grp_g, v, grp_p, bn_ctx) == -1) {
		error("%s: BN_mod_exp (g^v mod p)", __func__);
		goto out;
	}
	SCHNORR_DEBUG_BN((g_v, "%s: g_v = ", __func__));

	/* h = H(g || g^v || g^x || id) */
	if ((h = schnorr_hash(grp_p, grp_q, grp_g, evp_md, g_v, g_x,
	    id, idlen)) == NULL) {
		error("%s: schnorr_hash failed", __func__);
		goto out;
	}

	/* r = v - xh mod q */
	if (BN_mod_mul(tmp, x, h, grp_q, bn_ctx) == -1) {
		error("%s: BN_mod_mul (tmp = xv mod q)", __func__);
		goto out;
	}
	if (BN_mod_sub(r, v, tmp, grp_q, bn_ctx) == -1) {
		error("%s: BN_mod_mul (r = v - tmp)", __func__);
		goto out;
	}
	SCHNORR_DEBUG_BN((g_v, "%s: e = ", __func__));
	SCHNORR_DEBUG_BN((r, "%s: r = ", __func__));

	*e_p = g_v;
	*r_p = r;

	success = 0;
 out:
	BN_CTX_free(bn_ctx);
	if (h != NULL)
		BN_clear_free(h);
	if (v != NULL)
		BN_clear_free(v);
	BN_clear_free(tmp);

	return success;
}

/*
 * Generate Schnorr signature to prove knowledge of private value 'x' used
 * in public exponent g^x, under group defined by 'grp_p', 'grp_q' and 'grp_g'
 * using a SHA256 hash.
 * 'idlen' bytes from 'id' will be included in the signature hash as an anti-
 * replay salt.
 * On success, 0 is returned and *siglen bytes of signature are returned in
 * *sig (caller to free). Returns -1 on failure.
 */
int
schnorr_sign_buf(const BIGNUM *grp_p, const BIGNUM *grp_q, const BIGNUM *grp_g,
    const BIGNUM *x, const BIGNUM *g_x, const u_char *id, u_int idlen,
    u_char **sig, u_int *siglen)
{
	Buffer b;
	BIGNUM *r, *e;

	if (schnorr_sign(grp_p, grp_q, grp_g, EVP_sha256(),
	    x, g_x, id, idlen, &r, &e) != 0)
		return -1;

	/* Signature is (e, r) */
	buffer_init(&b);
	/* XXX sigtype-hash as string? */
	buffer_put_bignum2(&b, e);
	buffer_put_bignum2(&b, r);
	*siglen = buffer_len(&b);
	*sig = xmalloc(*siglen);
	memcpy(*sig, buffer_ptr(&b), *siglen);
	SCHNORR_DEBUG_BUF((buffer_ptr(&b), buffer_len(&b),
	    "%s: sigblob", __func__));
	buffer_free(&b);

	BN_clear_free(r);
	BN_clear_free(e);

	return 0;
}

/*
 * Verify Schnorr signature { r (v - xh mod q), e (g^v mod p) } against
 * public exponent g_x (g^x) under group defined by 'grp_p', 'grp_q' and
 * 'grp_g' using hash "evp_md".
 * Signature hash will be salted with 'idlen' bytes from 'id'.
 * Returns -1 on failure, 0 on incorrect signature or 1 on matching signature.
 */
int
schnorr_verify(const BIGNUM *grp_p, const BIGNUM *grp_q, const BIGNUM *grp_g,
    const EVP_MD *evp_md, const BIGNUM *g_x, const u_char *id, u_int idlen,
    const BIGNUM *r, const BIGNUM *e)
{
	int success = -1;
	BIGNUM *h = NULL, *g_xh = NULL, *g_r = NULL, *gx_q = NULL;
	BIGNUM *expected = NULL;
	BN_CTX *bn_ctx;

	SCHNORR_DEBUG_BN((g_x, "%s: g_x = ", __func__));

	/* Avoid degenerate cases: g^0 yields a spoofable signature */
	if (BN_cmp(g_x, BN_value_one()) <= 0) {
		error("%s: g_x <= 1", __func__);
		return -1;
	}
	if (BN_cmp(g_x, grp_p) >= 0) {
		error("%s: g_x >= p", __func__);
		return -1;
	}

	h = g_xh = g_r = expected = NULL;
	if ((bn_ctx = BN_CTX_new()) == NULL) {
		error("%s: BN_CTX_new", __func__);
		goto out;
	}
	if ((g_xh = BN_new()) == NULL ||
	    (g_r = BN_new()) == NULL ||
	    (gx_q = BN_new()) == NULL ||
	    (expected = BN_new()) == NULL) {
		error("%s: BN_new", __func__);
		goto out;
	}

	SCHNORR_DEBUG_BN((e, "%s: e = ", __func__));
	SCHNORR_DEBUG_BN((r, "%s: r = ", __func__));

	/* gx_q = (g^x)^q must === 1 mod p */
	if (BN_mod_exp(gx_q, g_x, grp_q, grp_p, bn_ctx) == -1) {
		error("%s: BN_mod_exp (g_x^q mod p)", __func__);
		goto out;
	}
	if (BN_cmp(gx_q, BN_value_one()) != 0) {
		error("%s: Invalid signature (g^x)^q != 1 mod p", __func__);
		goto out;
	}

	SCHNORR_DEBUG_BN((g_xh, "%s: g_xh = ", __func__));
	/* h = H(g || g^v || g^x || id) */
	if ((h = schnorr_hash(grp_p, grp_q, grp_g, evp_md, e, g_x,
	    id, idlen)) == NULL) {
		error("%s: schnorr_hash failed", __func__);
		goto out;
	}

	/* g_xh = (g^x)^h */
	if (BN_mod_exp(g_xh, g_x, h, grp_p, bn_ctx) == -1) {
		error("%s: BN_mod_exp (g_x^h mod p)", __func__);
		goto out;
	}
	SCHNORR_DEBUG_BN((g_xh, "%s: g_xh = ", __func__));

	/* g_r = g^r */
	if (BN_mod_exp(g_r, grp_g, r, grp_p, bn_ctx) == -1) {
		error("%s: BN_mod_exp (g_x^h mod p)", __func__);
		goto out;
	}
	SCHNORR_DEBUG_BN((g_r, "%s: g_r = ", __func__));

	/* expected = g^r * g_xh */
	if (BN_mod_mul(expected, g_r, g_xh, grp_p, bn_ctx) == -1) {
		error("%s: BN_mod_mul (expected = g_r mod p)", __func__);
		goto out;
	}
	SCHNORR_DEBUG_BN((expected, "%s: expected = ", __func__));

	/* Check e == expected */
	success = BN_cmp(expected, e) == 0;
 out:
	BN_CTX_free(bn_ctx);
	if (h != NULL)
		BN_clear_free(h);
	if (gx_q != NULL)
		BN_clear_free(gx_q);
	if (g_xh != NULL)
		BN_clear_free(g_xh);
	if (g_r != NULL)
		BN_clear_free(g_r);
	if (expected != NULL)
		BN_clear_free(expected);
	return success;
}

/*
 * Verify Schnorr signature 'sig' of length 'siglen' against public exponent
 * g_x (g^x) under group defined by 'grp_p', 'grp_q' and 'grp_g' using a
 * SHA256 hash.
 * Signature hash will be salted with 'idlen' bytes from 'id'.
 * Returns -1 on failure, 0 on incorrect signature or 1 on matching signature.
 */
int
schnorr_verify_buf(const BIGNUM *grp_p, const BIGNUM *grp_q,
    const BIGNUM *grp_g,
    const BIGNUM *g_x, const u_char *id, u_int idlen,
    const u_char *sig, u_int siglen)
{
	Buffer b;
	int ret = -1;
	u_int rlen;
	BIGNUM *r, *e;

	e = r = NULL;
	if ((e = BN_new()) == NULL ||
	    (r = BN_new()) == NULL) {
		error("%s: BN_new", __func__);
		goto out;
	}

	/* Extract g^v and r from signature blob */
	buffer_init(&b);
	buffer_append(&b, sig, siglen);
	SCHNORR_DEBUG_BUF((buffer_ptr(&b), buffer_len(&b),
	    "%s: sigblob", __func__));
	buffer_get_bignum2(&b, e);
	buffer_get_bignum2(&b, r);
	rlen = buffer_len(&b);
	buffer_free(&b);
	if (rlen != 0) {
		error("%s: remaining bytes in signature %d", __func__, rlen);
		goto out;
	}

	ret = schnorr_verify(grp_p, grp_q, grp_g, EVP_sha256(),
	    g_x, id, idlen, r, e);
 out:
	BN_clear_free(e);
	BN_clear_free(r);

	return ret;
}

/* Helper functions */

/*
 * Generate uniformly distributed random number in range (1, high).
 * Return number on success, NULL on failure.
 */
BIGNUM *
bn_rand_range_gt_one(const BIGNUM *high)
{
	BIGNUM *r, *tmp;
	int success = -1;

	if ((tmp = BN_new()) == NULL) {
		error("%s: BN_new", __func__);
		return NULL;
	}
	if ((r = BN_new()) == NULL) {
		error("%s: BN_new failed", __func__);
		goto out;
	}
	if (BN_set_word(tmp, 2) != 1) {
		error("%s: BN_set_word(tmp, 2)", __func__);
		goto out;
	}
	if (BN_sub(tmp, high, tmp) == -1) {
		error("%s: BN_sub failed (tmp = high - 2)", __func__);
		goto out;
	}
	if (BN_rand_range(r, tmp) == -1) {
		error("%s: BN_rand_range failed", __func__);
		goto out;
	}
	if (BN_set_word(tmp, 2) != 1) {
		error("%s: BN_set_word(tmp, 2)", __func__);
		goto out;
	}
	if (BN_add(r, r, tmp) == -1) {
		error("%s: BN_add failed (r = r + 2)", __func__);
		goto out;
	}
	success = 0;
 out:
	BN_clear_free(tmp);
	if (success == 0)
		return r;
	BN_clear_free(r);
	return NULL;
}

/*
 * Hash contents of buffer 'b' with hash 'md'. Returns 0 on success,
 * with digest via 'digestp' (caller to free) and length via 'lenp'.
 * Returns -1 on failure.
 */
int
hash_buffer(const u_char *buf, u_int len, const EVP_MD *md,
    u_char **digestp, u_int *lenp)
{
	u_char digest[EVP_MAX_MD_SIZE];
	u_int digest_len;
	EVP_MD_CTX evp_md_ctx;
	int success = -1;

	EVP_MD_CTX_init(&evp_md_ctx);

	if (EVP_DigestInit_ex(&evp_md_ctx, md, NULL) != 1) {
		error("%s: EVP_DigestInit_ex", __func__);
		goto out;
	}
	if (EVP_DigestUpdate(&evp_md_ctx, buf, len) != 1) {
		error("%s: EVP_DigestUpdate", __func__);
		goto out;
	}
	if (EVP_DigestFinal_ex(&evp_md_ctx, digest, &digest_len) != 1) {
		error("%s: EVP_DigestFinal_ex", __func__);
		goto out;
	}
	*digestp = xmalloc(digest_len);
	*lenp = digest_len;
	memcpy(*digestp, digest, *lenp);
	success = 0;
 out:
	EVP_MD_CTX_cleanup(&evp_md_ctx);
	bzero(digest, sizeof(digest));
	digest_len = 0;
	return success;
}

/* print formatted string followed by bignum */
void
debug3_bn(const BIGNUM *n, const char *fmt, ...)
{
	char *out, *h;
	va_list args;

	out = NULL;
	va_start(args, fmt);
	vasprintf(&out, fmt, args);
	va_end(args);
	if (out == NULL)
		fatal("%s: vasprintf failed", __func__);

	if (n == NULL)
		debug3("%s(null)", out);
	else {
		h = BN_bn2hex(n);
		debug3("%s0x%s", out, h);
		free(h);
	}
	free(out);
}

/* print formatted string followed by buffer contents in hex */
void
debug3_buf(const u_char *buf, u_int len, const char *fmt, ...)
{
	char *out, h[65];
	u_int i, j;
	va_list args;

	out = NULL;
	va_start(args, fmt);
	vasprintf(&out, fmt, args);
	va_end(args);
	if (out == NULL)
		fatal("%s: vasprintf failed", __func__);

	debug3("%s length %u%s", out, len, buf == NULL ? " (null)" : "");
	free(out);
	if (buf == NULL)
		return;

	*h = '\0';
	for (i = j = 0; i < len; i++) {
		snprintf(h + j, sizeof(h) - j, "%02x", buf[i]);
		j += 2;
		if (j >= sizeof(h) - 1 || i == len - 1) {
			debug3("    %s", h);
			*h = '\0';
			j = 0;
		}
	}
}

/*
 * Construct a MODP group from hex strings p (which must be a safe
 * prime) and g, automatically calculating subgroup q as (p / 2)
 */
struct modp_group *
modp_group_from_g_and_safe_p(const char *grp_g, const char *grp_p)
{
	struct modp_group *ret;

	ret = xmalloc(sizeof(*ret));
	ret->p = ret->q = ret->g = NULL;
	if (BN_hex2bn(&ret->p, grp_p) == 0 ||
	    BN_hex2bn(&ret->g, grp_g) == 0)
		fatal("%s: BN_hex2bn", __func__);
	/* Subgroup order is p/2 (p is a safe prime) */
	if ((ret->q = BN_new()) == NULL)
		fatal("%s: BN_new", __func__);
	if (BN_rshift1(ret->q, ret->p) != 1)
		fatal("%s: BN_rshift1", __func__);

	return ret;
}

void
modp_group_free(struct modp_group *grp)
{
	if (grp->g != NULL)
		BN_clear_free(grp->g);
	if (grp->p != NULL)
		BN_clear_free(grp->p);
	if (grp->q != NULL)
		BN_clear_free(grp->q);
	bzero(grp, sizeof(*grp));
	xfree(grp);
}

/* main() function for self-test */

#ifdef SCHNORR_MAIN
static void
schnorr_selftest_one(const BIGNUM *grp_p, const BIGNUM *grp_q,
    const BIGNUM *grp_g, const BIGNUM *x)
{
	BIGNUM *g_x;
	u_char *sig;
	u_int siglen;
	BN_CTX *bn_ctx;

	if ((bn_ctx = BN_CTX_new()) == NULL)
		fatal("%s: BN_CTX_new", __func__);
	if ((g_x = BN_new()) == NULL)
		fatal("%s: BN_new", __func__);

	if (BN_mod_exp(g_x, grp_g, x, grp_p, bn_ctx) == -1)
		fatal("%s: g_x", __func__);
	if (schnorr_sign_buf(grp_p, grp_q, grp_g, x, g_x, "junk", 4,
	    &sig, &siglen))
		fatal("%s: schnorr_sign", __func__);
	if (schnorr_verify_buf(grp_p, grp_q, grp_g, g_x, "junk", 4,
	    sig, siglen) != 1)
		fatal("%s: verify fail", __func__);
	if (schnorr_verify_buf(grp_p, grp_q, grp_g, g_x, "JUNK", 4,
	    sig, siglen) != 0)
		fatal("%s: verify should have failed (bad ID)", __func__);
	sig[4] ^= 1;
	if (schnorr_verify_buf(grp_p, grp_q, grp_g, g_x, "junk", 4,
	    sig, siglen) != 0)
		fatal("%s: verify should have failed (bit error)", __func__);
	xfree(sig);
	BN_free(g_x);
	BN_CTX_free(bn_ctx);
}

static void
schnorr_selftest(void)
{
	BIGNUM *x;
	struct modp_group *grp;
	u_int i;
	char *hh;

	grp = jpake_default_group();
	if ((x = BN_new()) == NULL)
		fatal("%s: BN_new", __func__);
	SCHNORR_DEBUG_BN((grp->p, "%s: grp->p = ", __func__));
	SCHNORR_DEBUG_BN((grp->q, "%s: grp->q = ", __func__));
	SCHNORR_DEBUG_BN((grp->g, "%s: grp->g = ", __func__));

	/* [1, 20) */
	for (i = 1; i < 20; i++) {
		printf("x = %u\n", i);
		fflush(stdout);
		if (BN_set_word(x, i) != 1)
			fatal("%s: set x word", __func__);
		schnorr_selftest_one(grp->p, grp->q, grp->g, x);
	}

	/* 100 x random [0, p) */
	for (i = 0; i < 100; i++) {
		if (BN_rand_range(x, grp->p) != 1)
			fatal("%s: BN_rand_range", __func__);
		hh = BN_bn2hex(x);
		printf("x = (random) 0x%s\n", hh);
		free(hh);
		fflush(stdout);
		schnorr_selftest_one(grp->p, grp->q, grp->g, x);
	}

	/* [q-20, q) */
	if (BN_set_word(x, 20) != 1)
		fatal("%s: BN_set_word (x = 20)", __func__);
	if (BN_sub(x, grp->q, x) != 1)
		fatal("%s: BN_sub (q - x)", __func__);
	for (i = 0; i < 19; i++) {
		hh = BN_bn2hex(x);
		printf("x = (q - %d) 0x%s\n", 20 - i, hh);
		free(hh);
		fflush(stdout);
		schnorr_selftest_one(grp->p, grp->q, grp->g, x);
		if (BN_add(x, x, BN_value_one()) != 1)
			fatal("%s: BN_add (x + 1)", __func__);
	}
	BN_free(x);
}

int
main(int argc, char **argv)
{
	log_init(argv[0], SYSLOG_LEVEL_DEBUG3, SYSLOG_FACILITY_USER, 1);

	schnorr_selftest();
	return 0;
}
#endif

#endif
