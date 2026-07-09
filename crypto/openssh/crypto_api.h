/* $OpenBSD: crypto_api.h,v 1.11 2026/06/14 03:59:34 djm Exp $ */

/*
 * Assembled from generated headers and source files by Markus Friedl.
 * Placed in the public domain.
 */

#ifndef crypto_api_h
#define crypto_api_h

#include "includes.h"

#include <stdint.h>
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

#ifdef WITH_OPENSSL
#include <openssl/evp.h>
static inline int
crypto_hash_sha512(unsigned char *out, const unsigned char *in,
    unsigned long long inlen)
{

	if (!EVP_Digest(in, inlen, out, NULL, EVP_sha512(), NULL))
		return -1;
	return 0;
}
#else /* WITH_OPENSSL */
# ifdef HAVE_SHA2_H
#  include <sha2.h>
# endif
static inline int
crypto_hash_sha512(unsigned char *out, const unsigned char *in,
    unsigned long long inlen)
{

	SHA2_CTX ctx;

	SHA512Init(&ctx);
	SHA512Update(&ctx, in, inlen);
	SHA512Final(out, &ctx);
	return 0;
}
#endif /* WITH_OPENSSL */

#define crypto_sign_ed25519_SECRETKEYBYTES 64U
#define crypto_sign_ed25519_PUBLICKEYBYTES 32U
#define crypto_sign_ed25519_SEEDBYTES 32U
#define crypto_sign_ed25519_BYTES 64U

int	crypto_sign_ed25519(unsigned char *, unsigned long long *,
    const unsigned char *, unsigned long long, const unsigned char *);
int	crypto_sign_ed25519_open(unsigned char *, unsigned long long *,
    const unsigned char *, unsigned long long, const unsigned char *);
int	crypto_sign_ed25519_keypair(unsigned char *, unsigned char *);
int	crypto_sign_ed25519_keypair_from_seed(unsigned char *, unsigned char *,
    const unsigned char *);

#define crypto_kem_sntrup761_PUBLICKEYBYTES 1158
#define crypto_kem_sntrup761_SECRETKEYBYTES 1763
#define crypto_kem_sntrup761_CIPHERTEXTBYTES 1039
#define crypto_kem_sntrup761_BYTES 32
#define crypto_kem_mlkem768_ENCSEEDBYTES 32
#define crypto_kem_mlkem768_KEYPAIRSEEDBYTES 64

int	crypto_kem_sntrup761_enc(unsigned char *cstr, unsigned char *k,
    const unsigned char *pk);
int	crypto_kem_sntrup761_dec(unsigned char *k,
    const unsigned char *cstr, const unsigned char *sk);
int	crypto_kem_sntrup761_keypair(unsigned char *pk, unsigned char *sk);

/* ML-KEM-768 */
#define crypto_kem_mlkem768_PUBLICKEYBYTES 1184
#define crypto_kem_mlkem768_SECRETKEYBYTES 2400
#define crypto_kem_mlkem768_CIPHERTEXTBYTES 1088
#define crypto_kem_mlkem768_BYTES 32

/* Aliases for MLKEM768 */
#define MLKEM768_PUBLICKEYBYTES crypto_kem_mlkem768_PUBLICKEYBYTES
#define MLKEM768_SECRETKEYBYTES crypto_kem_mlkem768_SECRETKEYBYTES
#define MLKEM768_CIPHERTEXTBYTES crypto_kem_mlkem768_CIPHERTEXTBYTES
#define MLKEM768_BYTES crypto_kem_mlkem768_BYTES

int crypto_kem_mlkem768_keypair(uint8_t pk[crypto_kem_mlkem768_PUBLICKEYBYTES],
    uint8_t sk[crypto_kem_mlkem768_SECRETKEYBYTES]);
int crypto_kem_mlkem768_enc(uint8_t ct[crypto_kem_mlkem768_CIPHERTEXTBYTES],
    uint8_t shared_secret[crypto_kem_mlkem768_BYTES],
    const uint8_t pk[crypto_kem_mlkem768_PUBLICKEYBYTES]);
int crypto_kem_mlkem768_dec(uint8_t shared_secret[crypto_kem_mlkem768_BYTES],
    const uint8_t ct[crypto_kem_mlkem768_CIPHERTEXTBYTES],
    const uint8_t sk[crypto_kem_mlkem768_SECRETKEYBYTES]);
int crypto_kem_mlkem768_keypair_seeded(uint8_t pk[crypto_kem_mlkem768_PUBLICKEYBYTES],
    uint8_t sk[crypto_kem_mlkem768_SECRETKEYBYTES], const uint8_t seed[64]);
int crypto_kem_mlkem768_enc_seeded(uint8_t ct[crypto_kem_mlkem768_CIPHERTEXTBYTES],
    uint8_t shared_secret[crypto_kem_mlkem768_BYTES],
    const uint8_t pk[crypto_kem_mlkem768_PUBLICKEYBYTES],
    const uint8_t seed[32]);

/* ML-DSA-44 */
#define MLDSA44_PUBLICKEYBYTES 1312
#define MLDSA44_SECRETKEYBYTES 2560
#define MLDSA44_SIGBYTES 2420
#define MLDSA44_SEEDBYTES 32

int crypto_sign_mldsa44_keypair(uint8_t pk[MLDSA44_PUBLICKEYBYTES],
    uint8_t sk[MLDSA44_SECRETKEYBYTES]);
int crypto_sign_mldsa44(uint8_t sig[MLDSA44_SIGBYTES],
    const uint8_t *msg, size_t msglen,
    const uint8_t *ctx, size_t ctxlen,
    const uint8_t sk[MLDSA44_SECRETKEYBYTES]);
int crypto_sign_mldsa44_verify(const uint8_t sig[MLDSA44_SIGBYTES],
    const uint8_t *msg, size_t msglen,
    const uint8_t *ctx, size_t ctxlen,
    const uint8_t pk[MLDSA44_PUBLICKEYBYTES]);
int crypto_sign_mldsa44_keypair_seeded(uint8_t pk[MLDSA44_PUBLICKEYBYTES],
    uint8_t sk[MLDSA44_SECRETKEYBYTES], const uint8_t seed[MLDSA44_SEEDBYTES]);
int crypto_sign_mldsa44_seeded(uint8_t sig[MLDSA44_SIGBYTES],
    const uint8_t *msg, size_t msglen,
    const uint8_t *ctx, size_t ctxlen,
    const uint8_t sk[MLDSA44_SECRETKEYBYTES],
    const uint8_t seed[MLDSA44_SEEDBYTES]);

/* ML-DSA-65 */
#define MLDSA65_PUBLICKEYBYTES 1952
#define MLDSA65_SECRETKEYBYTES 4032
#define MLDSA65_SIGBYTES 3309
#define MLDSA65_SEEDBYTES 32

#if 0
int crypto_sign_mldsa65_keypair(uint8_t pk[MLDSA65_PUBLICKEYBYTES],
    uint8_t sk[MLDSA65_SECRETKEYBYTES]);
int crypto_sign_mldsa65(uint8_t sig[MLDSA65_SIGBYTES],
    const uint8_t *msg, size_t msglen,
    const uint8_t *ctx, size_t ctxlen,
    const uint8_t sk[MLDSA65_SECRETKEYBYTES]);
int crypto_sign_mldsa65_verify(const uint8_t sig[MLDSA65_SIGBYTES],
    const uint8_t *msg, size_t msglen,
    const uint8_t *ctx, size_t ctxlen,
    const uint8_t pk[MLDSA65_PUBLICKEYBYTES]);
int crypto_sign_mldsa65_keypair_seeded(uint8_t pk[MLDSA65_PUBLICKEYBYTES],
    uint8_t sk[MLDSA65_SECRETKEYBYTES], const uint8_t seed[MLDSA65_SEEDBYTES]);
int crypto_sign_mldsa65_seeded(uint8_t sig[MLDSA65_SIGBYTES],
    const uint8_t *msg, size_t msglen,
    const uint8_t *ctx, size_t ctxlen,
    const uint8_t sk[MLDSA65_SECRETKEYBYTES],
    const uint8_t seed[MLDSA65_SEEDBYTES]);
#endif

/* ML-DSA-87 */
#define MLDSA87_PUBLICKEYBYTES 2592
#define MLDSA87_SECRETKEYBYTES 4896
#define MLDSA87_SIGBYTES 4627
#define MLDSA87_SEEDBYTES 32

int crypto_sign_mldsa87_keypair(uint8_t pk[MLDSA87_PUBLICKEYBYTES],
    uint8_t sk[MLDSA87_SECRETKEYBYTES]);
int crypto_sign_mldsa87(uint8_t sig[MLDSA87_SIGBYTES],
    const uint8_t *msg, size_t msglen,
    const uint8_t *ctx, size_t ctxlen,
    const uint8_t sk[MLDSA87_SECRETKEYBYTES]);
int crypto_sign_mldsa87_verify(const uint8_t sig[MLDSA87_SIGBYTES],
    const uint8_t *msg, size_t msglen,
    const uint8_t *ctx, size_t ctxlen,
    const uint8_t pk[MLDSA87_PUBLICKEYBYTES]);
int crypto_sign_mldsa87_keypair_seeded(uint8_t pk[MLDSA87_PUBLICKEYBYTES],
    uint8_t sk[MLDSA87_SECRETKEYBYTES], const uint8_t seed[MLDSA87_SEEDBYTES]);
int crypto_sign_mldsa87_seeded(uint8_t sig[MLDSA87_SIGBYTES],
    const uint8_t *msg, size_t msglen,
    const uint8_t *ctx, size_t ctxlen,
    const uint8_t sk[MLDSA87_SECRETKEYBYTES],
    const uint8_t seed[MLDSA87_SEEDBYTES]);

/* MLDSA44-Ed25519-SHA512 */
#define MLDSA44_ED25519_PK_SZ (MLDSA44_PUBLICKEYBYTES + crypto_sign_ed25519_PUBLICKEYBYTES)
#define MLDSA44_ED25519_SK_SZ (MLDSA44_SEEDBYTES + crypto_sign_ed25519_SEEDBYTES)
#define MLDSA44_ED25519_SIG_SZ (MLDSA44_SIGBYTES + crypto_sign_ed25519_BYTES)

int crypto_sign_mldsa44_ed25519_keygen(uint8_t pk[MLDSA44_ED25519_PK_SZ],
    uint8_t sk[MLDSA44_ED25519_SK_SZ]);
int crypto_sign_mldsa44_ed25519_keygen_seeded(uint8_t pk[MLDSA44_ED25519_PK_SZ],
    uint8_t sk[MLDSA44_ED25519_SK_SZ],
    const uint8_t mldsa_seed[MLDSA44_SEEDBYTES],
    const uint8_t ed25519_seed[crypto_sign_ed25519_SEEDBYTES]);
int crypto_sign_mldsa44_ed25519_sign(uint8_t sig[MLDSA44_ED25519_SIG_SZ],
    const uint8_t *msg, size_t msglen,
    const uint8_t *ctx, size_t ctxlen,
    const uint8_t sk[MLDSA44_ED25519_SK_SZ]);
int crypto_sign_mldsa44_ed25519_verify(const uint8_t sig[MLDSA44_ED25519_SIG_SZ],
    const uint8_t *msg, size_t msglen,
    const uint8_t *ctx, size_t ctxlen,
    const uint8_t pk[MLDSA44_ED25519_PK_SZ]);

/* Utility */
void sha3_256(uint8_t digest[32], const uint8_t *data, size_t len);
void sha3_512(uint8_t digest[64], const uint8_t *data, size_t len);

#endif /* crypto_api_h */
