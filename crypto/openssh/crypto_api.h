/* $OpenBSD: crypto_api.h,v 1.9 2024/09/02 12:13:56 djm Exp $ */

/*
 * Assembled from generated headers and source files by Markus Friedl.
 * Placed in the public domain.
 */

#ifndef crypto_api_h
#define crypto_api_h

#include "includes.h"

#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif
#include <stdlib.h>

typedef int8_t crypto_int8;
typedef uint8_t crypto_uint8;
typedef int16_t crypto_int16;
typedef uint16_t crypto_uint16;
typedef int32_t crypto_int32;
typedef uint32_t crypto_uint32;
typedef int64_t crypto_int64;
typedef uint64_t crypto_uint64;

#define randombytes(buf, buf_len) arc4random_buf((buf), (buf_len))
#define small_random32() arc4random()

#define crypto_hash_sha512_BYTES 64U

int	crypto_hash_sha512(unsigned char *, const unsigned char *,
    unsigned long long);

#define crypto_sign_ed25519_SECRETKEYBYTES 64U
#define crypto_sign_ed25519_PUBLICKEYBYTES 32U
#define crypto_sign_ed25519_BYTES 64U

int	crypto_sign_ed25519(unsigned char *, unsigned long long *,
    const unsigned char *, unsigned long long, const unsigned char *);
int	crypto_sign_ed25519_open(unsigned char *, unsigned long long *,
    const unsigned char *, unsigned long long, const unsigned char *);
int	crypto_sign_ed25519_keypair(unsigned char *, unsigned char *);

#define crypto_kem_sntrup761_PUBLICKEYBYTES 1158
#define crypto_kem_sntrup761_SECRETKEYBYTES 1763
#define crypto_kem_sntrup761_CIPHERTEXTBYTES 1039
#define crypto_kem_sntrup761_BYTES 32

int	crypto_kem_sntrup761_enc(unsigned char *cstr, unsigned char *k,
    const unsigned char *pk);
int	crypto_kem_sntrup761_dec(unsigned char *k,
    const unsigned char *cstr, const unsigned char *sk);
int	crypto_kem_sntrup761_keypair(unsigned char *pk, unsigned char *sk);

#define crypto_kem_mlkem768_PUBLICKEYBYTES 1184
#define crypto_kem_mlkem768_SECRETKEYBYTES 2400
#define crypto_kem_mlkem768_CIPHERTEXTBYTES 1088
#define crypto_kem_mlkem768_BYTES 32

#endif /* crypto_api_h */
