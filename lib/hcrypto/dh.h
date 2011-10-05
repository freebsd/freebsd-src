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

#ifndef _HEIM_DH_H
#define _HEIM_DH_H 1

/* symbol renaming */
#define DH_null_method hc_DH_null_method
#define DH_tfm_method hc_DH_tfm_method
#define DH_ltm_method hc_DH_ltm_method
#define DH_new hc_DH_new
#define DH_new_method hc_DH_new_method
#define DH_free hc_DH_free
#define DH_up_ref hc_DH_up_ref
#define DH_size hc_DH_size
#define DH_set_default_method hc_DH_set_default_method
#define DH_get_default_method hc_DH_get_default_method
#define DH_set_method hc_DH_set_method
#define DH_get_method hc_DH_get_method
#define DH_set_ex_data hc_DH_set_ex_data
#define DH_get_ex_data hc_DH_get_ex_data
#define DH_generate_parameters_ex hc_DH_generate_parameters_ex
#define DH_check_pubkey hc_DH_check_pubkey
#define DH_generate_key hc_DH_generate_key
#define DH_compute_key hc_DH_compute_key
#define	i2d_DHparams hc_i2d_DHparams

/*
 *
 */

typedef struct DH DH;
typedef struct DH_METHOD DH_METHOD;

#include <hcrypto/bn.h>
#include <hcrypto/engine.h>

struct DH_METHOD {
    const char *name;
    int (*generate_key)(DH *);
    int (*compute_key)(unsigned char *,const BIGNUM *,DH *);
    int (*bn_mod_exp)(const DH *, BIGNUM *, const BIGNUM *,
		      const BIGNUM *, const BIGNUM *, BN_CTX *,
		      BN_MONT_CTX *);
    int (*init)(DH *);
    int (*finish)(DH *);
    int flags;
    void *app_data;
    int (*generate_params)(DH *, int, int, BN_GENCB *);
};

struct DH {
    int pad;
    int version;
    BIGNUM *p;
    BIGNUM *g;
    long length;
    BIGNUM *pub_key;
    BIGNUM *priv_key;
    int flags;
    void *method_mont_p;
    BIGNUM *q;
    BIGNUM *j;
    void *seed;
    int seedlen;
    BIGNUM *counter;
    int references;
    struct CRYPTO_EX_DATA {
	void *sk;
	int dummy;
    } ex_data;
    const DH_METHOD *meth;
    ENGINE *engine;
};

/* DH_check_pubkey return codes in `codes' argument. */
#define DH_CHECK_PUBKEY_TOO_SMALL 1
#define DH_CHECK_PUBKEY_TOO_LARGE 2

/*
 *
 */

const DH_METHOD *DH_null_method(void);
const DH_METHOD *DH_tfm_method(void);
const DH_METHOD *DH_ltm_method(void);

DH *	DH_new(void);
DH *	DH_new_method(ENGINE *);
void	DH_free(DH *);
int	DH_up_ref(DH *);

int	DH_size(const DH *);


void	DH_set_default_method(const DH_METHOD *);
const DH_METHOD *
	DH_get_default_method(void);
int	DH_set_method(DH *, const DH_METHOD *);

int	DH_set_ex_data(DH *, int, void *);
void *	DH_get_ex_data(DH *, int);

int	DH_generate_parameters_ex(DH *, int, int, BN_GENCB *);
int	DH_check_pubkey(const DH *, const BIGNUM *, int *);
int	DH_generate_key(DH *);
int	DH_compute_key(unsigned char *,const BIGNUM *,DH *);

int	i2d_DHparams(DH *, unsigned char **);

#endif /* _HEIM_DH_H */

