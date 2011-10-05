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

#ifndef _HEIM_RSA_H
#define _HEIM_RSA_H 1

/* symbol renaming */
#define RSA_null_method hc_RSA_null_method
#define RSA_ltm_method hc_RSA_ltm_method
#define RSA_gmp_method hc_RSA_gmp_method
#define RSA_tfm_method hc_RSA_tfm_method
#define RSA_new hc_RSA_new
#define RSA_new_method hc_RSA_new_method
#define RSA_free hc_RSA_free
#define RSA_up_ref hc_RSA_up_ref
#define RSA_set_default_method hc_RSA_set_default_method
#define RSA_get_default_method hc_RSA_get_default_method
#define RSA_set_method hc_RSA_set_method
#define RSA_get_method hc_RSA_get_method
#define RSA_set_app_data hc_RSA_set_app_data
#define RSA_get_app_data hc_RSA_get_app_data
#define RSA_check_key hc_RSA_check_key
#define RSA_size hc_RSA_size
#define RSA_public_encrypt hc_RSA_public_encrypt
#define RSA_public_decrypt hc_RSA_public_decrypt
#define RSA_private_encrypt hc_RSA_private_encrypt
#define RSA_private_decrypt hc_RSA_private_decrypt
#define RSA_sign hc_RSA_sign
#define RSA_verify hc_RSA_verify
#define RSA_generate_key_ex hc_RSA_generate_key_ex
#define d2i_RSAPrivateKey hc_d2i_RSAPrivateKey
#define i2d_RSAPrivateKey hc_i2d_RSAPrivateKey
#define i2d_RSAPublicKey hc_i2d_RSAPublicKey
#define d2i_RSAPublicKey hc_d2i_RSAPublicKey

/*
 *
 */

typedef struct RSA RSA;
typedef struct RSA_METHOD RSA_METHOD;

#include <hcrypto/bn.h>
#include <hcrypto/engine.h>

struct RSA_METHOD {
    const char *name;
    int (*rsa_pub_enc)(int,const unsigned char *, unsigned char *, RSA *,int);
    int (*rsa_pub_dec)(int,const unsigned char *, unsigned char *, RSA *,int);
    int (*rsa_priv_enc)(int,const unsigned char *, unsigned char *, RSA *,int);
    int (*rsa_priv_dec)(int,const unsigned char *, unsigned char *, RSA *,int);
    void *rsa_mod_exp;
    void *bn_mod_exp;
    int (*init)(RSA *rsa);
    int (*finish)(RSA *rsa);
    int flags;
    char *app_data;
    int (*rsa_sign)(int, const unsigned char *, unsigned int,
		    unsigned char *, unsigned int *, const RSA *);
    int (*rsa_verify)(int, const unsigned char *, unsigned int,
		      unsigned char *, unsigned int, const RSA *);
    int (*rsa_keygen)(RSA *, int, BIGNUM *, BN_GENCB *);
};

struct RSA {
    int pad;
    long version;
    const RSA_METHOD *meth;
    void *engine;
    BIGNUM *n;
    BIGNUM *e;
    BIGNUM *d;
    BIGNUM *p;
    BIGNUM *q;
    BIGNUM *dmp1;
    BIGNUM *dmq1;
    BIGNUM *iqmp;
    struct rsa_CRYPTO_EX_DATA {
	void *sk;
	int dummy;
    } ex_data;
    int references;
    int flags;
    void *_method_mod_n;
    void *_method_mod_p;
    void *_method_mod_q;

    char *bignum_data;
    void *blinding;
    void *mt_blinding;
};

#define RSA_FLAG_NO_BLINDING		0x0080

#define RSA_PKCS1_PADDING		1
#define RSA_PKCS1_OAEP_PADDING		4
#define RSA_PKCS1_PADDING_SIZE		11

/*
 *
 */

const RSA_METHOD *RSA_null_method(void);
const RSA_METHOD *RSA_gmp_method(void);
const RSA_METHOD *RSA_tfm_method(void);
const RSA_METHOD *RSA_ltm_method(void);

/*
 *
 */

RSA *	RSA_new(void);
RSA *	RSA_new_method(ENGINE *);
void	RSA_free(RSA *);
int	RSA_up_ref(RSA *);

void	RSA_set_default_method(const RSA_METHOD *);
const RSA_METHOD * RSA_get_default_method(void);

const RSA_METHOD * RSA_get_method(const RSA *);
int RSA_set_method(RSA *, const RSA_METHOD *);

int	RSA_set_app_data(RSA *, void *arg);
void *	RSA_get_app_data(const RSA *);

int	RSA_check_key(const RSA *);
int	RSA_size(const RSA *);

int	RSA_public_encrypt(int,const unsigned char*,unsigned char*,RSA *,int);
int	RSA_private_encrypt(int,const unsigned char*,unsigned char*,RSA *,int);
int	RSA_public_decrypt(int,const unsigned char*,unsigned char*,RSA *,int);
int	RSA_private_decrypt(int,const unsigned char*,unsigned char*,RSA *,int);

int RSA_sign(int, const unsigned char *, unsigned int,
	     unsigned char *, unsigned int *, RSA *);
int RSA_verify(int, const unsigned char *, unsigned int,
	       unsigned char *, unsigned int, RSA *);

int	RSA_generate_key_ex(RSA *, int, BIGNUM *, BN_GENCB *);

RSA *	d2i_RSAPrivateKey(RSA *, const unsigned char **, size_t);
int	i2d_RSAPrivateKey(RSA *, unsigned char **);

int	i2d_RSAPublicKey(RSA *, unsigned char **);
RSA *	d2i_RSAPublicKey(RSA *, const unsigned char **, size_t);

#endif /* _HEIM_RSA_H */
