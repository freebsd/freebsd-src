/* $OpenBSD: jpake.h,v 1.1 2008/11/04 08:22:13 djm Exp $ */
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

#ifndef JPAKE_H
#define JPAKE_H

#include <sys/types.h>

#include <openssl/bn.h>

/* Set JPAKE_DEBUG in CFLAGS for privacy-violating debugging */
#ifndef JPAKE_DEBUG
# define JPAKE_DEBUG_BN(a)
# define JPAKE_DEBUG_BUF(a)
# define JPAKE_DEBUG_CTX(a)
#else
# define JPAKE_DEBUG_BN(a)	jpake_debug3_bn a
# define JPAKE_DEBUG_BUF(a)	jpake_debug3_buf a
# define JPAKE_DEBUG_CTX(a)	jpake_dump a
#endif /* SCHNORR_DEBUG */

struct jpake_group {
	BIGNUM *p, *q, *g;
};

#define KZP_ID_LEN	16	/* Length of client and server IDs */

struct jpake_ctx {
	/* Parameters */
	struct jpake_group *grp;

	/* Private values shared by client and server */
	BIGNUM *s;			/* Secret (salted, crypted password) */
	BIGNUM *k;			/* Derived key */

	/* Client private values (NULL for server) */
	BIGNUM *x1;			/* random in Zq */
	BIGNUM *x2;			/* random in Z*q */

	/* Server private values (NULL for server) */
	BIGNUM *x3;			/* random in Zq */
	BIGNUM *x4;			/* random in Z*q */

	/* Step 1: C->S */
	u_char *client_id;		/* Anti-replay nonce */
	u_int client_id_len;
	BIGNUM *g_x1;			/* g^x1 */
	BIGNUM *g_x2;			/* g^x2 */

	/* Step 1: S->C */
	u_char *server_id;		/* Anti-replay nonce */
	u_int server_id_len;
	BIGNUM *g_x3;			/* g^x3 */
	BIGNUM *g_x4;			/* g^x4 */

	/* Step 2: C->S */
	BIGNUM *a;			/* g^((x1+x3+x4)*x2*s) */

	/* Step 2: S->C */
	BIGNUM *b;			/* g^((x1+x2+x3)*x4*s) */

	/* Confirmation: C->S */
	u_char *h_k_cid_sessid;		/* H(k || client_id || session_id) */
	u_int h_k_cid_sessid_len;

	/* Confirmation: S->C */
	u_char *h_k_sid_sessid;		/* H(k || server_id || session_id) */
	u_int h_k_sid_sessid_len;
};

/* jpake.c */
struct jpake_group *jpake_default_group(void);
BIGNUM *bn_rand_range_gt_one(const BIGNUM *high);
int hash_buffer(const u_char *, u_int, const EVP_MD *, u_char **, u_int *);
void jpake_debug3_bn(const BIGNUM *, const char *, ...)
    __attribute__((__nonnull__ (2)))
    __attribute__((format(printf, 2, 3)));
void jpake_debug3_buf(const u_char *, u_int, const char *, ...)
    __attribute__((__nonnull__ (3)))
    __attribute__((format(printf, 3, 4)));
void jpake_dump(struct jpake_ctx *, const char *, ...)
    __attribute__((__nonnull__ (2)))
    __attribute__((format(printf, 2, 3)));
struct jpake_ctx *jpake_new(void);
void jpake_free(struct jpake_ctx *);

void jpake_step1(struct jpake_group *, u_char **, u_int *,
    BIGNUM **, BIGNUM **, BIGNUM **, BIGNUM **,
    u_char **, u_int *, u_char **, u_int *);

void jpake_step2(struct jpake_group *, BIGNUM *,
    BIGNUM *, BIGNUM *, BIGNUM *, BIGNUM *,
    const u_char *, u_int, const u_char *, u_int,
    const u_char *, u_int, const u_char *, u_int,
    BIGNUM **, u_char **, u_int *);

void jpake_confirm_hash(const BIGNUM *,
    const u_char *, u_int,
    const u_char *, u_int,
    u_char **, u_int *);

void jpake_key_confirm(struct jpake_group *, BIGNUM *, BIGNUM *,
    BIGNUM *, BIGNUM *, BIGNUM *, BIGNUM *, BIGNUM *,
    const u_char *, u_int, const u_char *, u_int,
    const u_char *, u_int, const u_char *, u_int,
    BIGNUM **, u_char **, u_int *);

int jpake_check_confirm(const BIGNUM *, const u_char *, u_int,
    const u_char *, u_int, const u_char *, u_int);

/* schnorr.c */
int schnorr_sign(const BIGNUM *, const BIGNUM *, const BIGNUM *,
    const BIGNUM *, const BIGNUM *, const u_char *, u_int ,
    u_char **, u_int *);
int schnorr_verify(const BIGNUM *, const BIGNUM *, const BIGNUM *, 
    const BIGNUM *, const u_char *, u_int,
    const u_char *, u_int);

#endif /* JPAKE_H */

