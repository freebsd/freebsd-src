/* $Id: openssl-compat.h,v 1.24 2013/02/12 00:00:40 djm Exp $ */

/*
 * Copyright (c) 2005 Darren Tucker <dtucker@zip.com.au>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "includes.h"
#include <openssl/opensslv.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/dsa.h>

/* Only in 0.9.8 */
#ifndef OPENSSL_DSA_MAX_MODULUS_BITS
# define OPENSSL_DSA_MAX_MODULUS_BITS        10000
#endif
#ifndef OPENSSL_RSA_MAX_MODULUS_BITS
# define OPENSSL_RSA_MAX_MODULUS_BITS        16384
#endif

/* OPENSSL_free() is Free() in versions before OpenSSL 0.9.6 */
#if !defined(OPENSSL_VERSION_NUMBER) || (OPENSSL_VERSION_NUMBER < 0x0090600f)
# define OPENSSL_free(x) Free(x)
#endif

#if OPENSSL_VERSION_NUMBER < 0x00906000L
# define SSH_OLD_EVP
# define EVP_CIPHER_CTX_get_app_data(e)		((e)->app_data)
#endif

#if OPENSSL_VERSION_NUMBER < 0x10000001L
# define LIBCRYPTO_EVP_INL_TYPE unsigned int
#else
# define LIBCRYPTO_EVP_INL_TYPE size_t
#endif

#if (OPENSSL_VERSION_NUMBER < 0x00907000L) || defined(OPENSSL_LOBOTOMISED_AES)
# define USE_BUILTIN_RIJNDAEL
#endif

#ifdef USE_BUILTIN_RIJNDAEL
# include "rijndael.h"
# define AES_KEY rijndael_ctx
# define AES_BLOCK_SIZE 16
# define AES_encrypt(a, b, c)		rijndael_encrypt(c, a, b)
# define AES_set_encrypt_key(a, b, c)	rijndael_set_key(c, (char *)a, b, 1)
# define EVP_aes_128_cbc evp_rijndael
# define EVP_aes_192_cbc evp_rijndael
# define EVP_aes_256_cbc evp_rijndael
const EVP_CIPHER *evp_rijndael(void);
void ssh_rijndael_iv(EVP_CIPHER_CTX *, int, u_char *, u_int);
#endif

#ifndef OPENSSL_HAVE_EVPCTR
#define EVP_aes_128_ctr evp_aes_128_ctr
#define EVP_aes_192_ctr evp_aes_128_ctr
#define EVP_aes_256_ctr evp_aes_128_ctr
const EVP_CIPHER *evp_aes_128_ctr(void);
void ssh_aes_ctr_iv(EVP_CIPHER_CTX *, int, u_char *, size_t);
#endif

/* Avoid some #ifdef. Code that uses these is unreachable without GCM */
#if !defined(OPENSSL_HAVE_EVPGCM) && !defined(EVP_CTRL_GCM_SET_IV_FIXED)
# define EVP_CTRL_GCM_SET_IV_FIXED -1
# define EVP_CTRL_GCM_IV_GEN -1
# define EVP_CTRL_GCM_SET_TAG -1
# define EVP_CTRL_GCM_GET_TAG -1
#endif

/* Replace missing EVP_CIPHER_CTX_ctrl() with something that returns failure */
#ifndef HAVE_EVP_CIPHER_CTX_CTRL
# ifdef OPENSSL_HAVE_EVPGCM
#  error AES-GCM enabled without EVP_CIPHER_CTX_ctrl /* shouldn't happen */
# else
# define EVP_CIPHER_CTX_ctrl(a,b,c,d) (0)
# endif
#endif

#if OPENSSL_VERSION_NUMBER < 0x00907000L
#define EVP_X_STATE(evp)	&(evp).c
#define EVP_X_STATE_LEN(evp)	sizeof((evp).c)
#else
#define EVP_X_STATE(evp)	(evp).cipher_data
#define EVP_X_STATE_LEN(evp)	(evp).cipher->ctx_size
#endif

/* OpenSSL 0.9.8e returns cipher key len not context key len */
#if (OPENSSL_VERSION_NUMBER == 0x0090805fL)
# define EVP_CIPHER_CTX_key_length(c) ((c)->key_len)
#endif

#ifndef HAVE_RSA_GET_DEFAULT_METHOD
RSA_METHOD *RSA_get_default_method(void);
#endif

/*
 * We overload some of the OpenSSL crypto functions with ssh_* equivalents
 * which cater for older and/or less featureful OpenSSL version.
 *
 * In order for the compat library to call the real functions, it must
 * define SSH_DONT_OVERLOAD_OPENSSL_FUNCS before including this file and
 * implement the ssh_* equivalents.
 */
#ifndef SSH_DONT_OVERLOAD_OPENSSL_FUNCS

# ifdef SSH_OLD_EVP
#  ifdef EVP_Cipher
#   undef EVP_Cipher
#  endif
#  define EVP_CipherInit(a,b,c,d,e)	ssh_EVP_CipherInit((a),(b),(c),(d),(e))
#  define EVP_Cipher(a,b,c,d)		ssh_EVP_Cipher((a),(b),(c),(d))
#  define EVP_CIPHER_CTX_cleanup(a)	ssh_EVP_CIPHER_CTX_cleanup((a))
# endif /* SSH_OLD_EVP */

# ifdef OPENSSL_EVP_DIGESTUPDATE_VOID
#  define EVP_DigestUpdate(a,b,c)	ssh_EVP_DigestUpdate((a),(b),(c))
#  endif

# ifdef USE_OPENSSL_ENGINE
#  ifdef OpenSSL_add_all_algorithms
#   undef OpenSSL_add_all_algorithms
#  endif
#  define OpenSSL_add_all_algorithms()  ssh_OpenSSL_add_all_algorithms()
# endif

# ifndef HAVE_BN_IS_PRIME_EX
int BN_is_prime_ex(const BIGNUM *, int, BN_CTX *, void *);
# endif

# ifndef HAVE_DSA_GENERATE_PARAMETERS_EX
int DSA_generate_parameters_ex(DSA *, int, const unsigned char *, int, int *,
    unsigned long *, void *);
# endif

# ifndef HAVE_RSA_GENERATE_KEY_EX
int RSA_generate_key_ex(RSA *, int, BIGNUM *, void *);
# endif

int ssh_EVP_CipherInit(EVP_CIPHER_CTX *, const EVP_CIPHER *, unsigned char *,
    unsigned char *, int);
int ssh_EVP_Cipher(EVP_CIPHER_CTX *, char *, char *, int);
int ssh_EVP_CIPHER_CTX_cleanup(EVP_CIPHER_CTX *);
void ssh_OpenSSL_add_all_algorithms(void);

# ifndef HAVE_HMAC_CTX_INIT
#  define HMAC_CTX_init(a)
# endif

#endif	/* SSH_DONT_OVERLOAD_OPENSSL_FUNCS */

