/* $OpenBSD: schnorr.h,v 1.1 2009/03/05 07:18:19 djm Exp $ */
/*
 * Copyright (c) 2009 Damien Miller.  All rights reserved.
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

#ifndef SCHNORR_H
#define SCHNORR_H

#include <sys/types.h>

#include <openssl/bn.h>

struct modp_group {
	BIGNUM *p, *q, *g;
};

BIGNUM *bn_rand_range_gt_one(const BIGNUM *high);
int hash_buffer(const u_char *, u_int, const EVP_MD *, u_char **, u_int *);
void debug3_bn(const BIGNUM *, const char *, ...)
    __attribute__((__nonnull__ (2)))
    __attribute__((format(printf, 2, 3)));
void debug3_buf(const u_char *, u_int, const char *, ...)
    __attribute__((__nonnull__ (3)))
    __attribute__((format(printf, 3, 4)));
struct modp_group *modp_group_from_g_and_safe_p(const char *, const char *);
void modp_group_free(struct modp_group *);

/* Signature and verification functions */
int
schnorr_sign(const BIGNUM *grp_p, const BIGNUM *grp_q, const BIGNUM *grp_g,
    const EVP_MD *evp_md, const BIGNUM *x, const BIGNUM *g_x,
    const u_char *id, u_int idlen, BIGNUM **r_p, BIGNUM **e_p);
int
schnorr_sign_buf(const BIGNUM *grp_p, const BIGNUM *grp_q, const BIGNUM *grp_g,
    const BIGNUM *x, const BIGNUM *g_x, const u_char *id, u_int idlen,
    u_char **sig, u_int *siglen);
int
schnorr_verify(const BIGNUM *grp_p, const BIGNUM *grp_q, const BIGNUM *grp_g,
    const EVP_MD *evp_md, const BIGNUM *g_x, const u_char *id, u_int idlen,
    const BIGNUM *r, const BIGNUM *e);
int
schnorr_verify_buf(const BIGNUM *grp_p, const BIGNUM *grp_q,
    const BIGNUM *grp_g,
    const BIGNUM *g_x, const u_char *id, u_int idlen,
    const u_char *sig, u_int siglen);

#endif /* JPAKE_H */

