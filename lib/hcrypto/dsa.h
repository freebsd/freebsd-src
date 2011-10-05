/*
 * Copyright (c) 2006 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $Id$
 */

#ifndef _HEIM_DSA_H
#define _HEIM_DSA_H 1

#include <hcrypto/bn.h>

/* symbol renaming */
#define DSA_null_method hc_DSA_null_method
#define DSA_new hc_DSA_new
#define DSA_free hc_DSA_free
#define DSA_up_ref hc_DSA_up_ref
#define DSA_set_default_method hc_DSA_set_default_method
#define DSA_get_default_method hc_DSA_get_default_method
#define DSA_set_method hc_DSA_set_method
#define DSA_get_method hc_DSA_get_method
#define DSA_set_app_data hc_DSA_set_app_data
#define DSA_get_app_data hc_DSA_get_app_data
#define DSA_size hc_DSA_size
#define DSA_verify hc_DSA_verify

/*
 *
 */


typedef struct DSA DSA;
typedef struct DSA_METHOD DSA_METHOD;
typedef struct DSA_SIG DSA_SIG;

struct DSA_SIG {
    BIGNUM *r;
    BIGNUM *s;
};

struct DSA_METHOD {
    const char *name;
    DSA_SIG * (*dsa_do_sign)(const unsigned char *, int, DSA *);
    int (*dsa_sign_setup)(DSA *, BN_CTX *, BIGNUM **, BIGNUM **);
    int (*dsa_do_verify)(const unsigned char *, int, DSA_SIG *, DSA *);
    int (*dsa_mod_exp)(DSA *, BIGNUM *, BIGNUM *, BIGNUM *,
		       BIGNUM *, BIGNUM *, BIGNUM *, BN_CTX *,
		       BN_MONT_CTX *);
    int (*bn_mod_exp)(DSA *, BIGNUM *, BIGNUM *, const BIGNUM *,
		      const BIGNUM *, BN_CTX *,
		      BN_MONT_CTX *);
    int (*init)(DSA *);
    int (*finish)(DSA *);
    int flags;
    void *app_data;
};

struct DSA {
    int pad;
    long version;
    int write_params;
    BIGNUM *p;
    BIGNUM *q;
    BIGNUM *g;

    BIGNUM *pub_key;
    BIGNUM *priv_key;

    BIGNUM *kinv;
    BIGNUM *r;
    int flags;
    void *method_mont_p;
    int references;
    struct dsa_CRYPTO_EX_DATA {
	void *sk;
	int dummy;
    } ex_data;
    const DSA_METHOD *meth;
    void *engine;
};

/*
 *
 */

const DSA_METHOD *DSA_null_method(void);

/*
 *
 */

DSA *	DSA_new(void);
void	DSA_free(DSA *);
int	DSA_up_ref(DSA *);

void	DSA_set_default_method(const DSA_METHOD *);
const DSA_METHOD * DSA_get_default_method(void);

const DSA_METHOD * DSA_get_method(const DSA *);
int DSA_set_method(DSA *, const DSA_METHOD *);

void	DSA_set_app_data(DSA *, void *arg);
void *	DSA_get_app_data(DSA *);

int	DSA_size(const DSA *);

int	DSA_verify(int, const unsigned char *, int,
		   const unsigned char *, int, DSA *);

#endif /* _HEIM_DSA_H */
