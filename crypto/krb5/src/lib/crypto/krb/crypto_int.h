/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/krb/crypto_int.h - Master libk5crypto internal header */
/*
 * Copyright (C) 2011 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

/* This header is the entry point for libk5crypto sources, and also documents
 * requirements for crypto modules. */

#ifndef CRYPTO_INT_H
#define CRYPTO_INT_H

#include <k5-int.h>

#if defined(CRYPTO_OPENSSL)

#include <openssl/opensslv.h>
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
/*
 * OpenSSL 3.0 relegates MD4 and RC4 to the legacy provider, which must be
 * explicitly loaded into a library context.  Performing this loading within a
 * library carries complications, so use the built-in implementations of these
 * primitives instead.  OpenSSL 3.0 also deprecates DES_set_odd_parity() with
 * no replacement.
 *
 * OpenSSL 3.0 adds KDF implementations matching the ones we use to derive
 * encryption and authentication keys from protocol keys.  It also adds
 * the EVP_MAC interface which can be used for CMAC.  (We could use the CMAC
 * interface with OpenSSL 1.1 but currently do not.)
 */
#define K5_BUILTIN_DES_KEY_PARITY
#define K5_BUILTIN_MD4
#define K5_BUILTIN_RC4
#define K5_OPENSSL_KDF
#define K5_OPENSSL_CMAC
#else
#define K5_OPENSSL_DES_KEY_PARITY
#define K5_OPENSSL_MD4
#define K5_OPENSSL_RC4
#define K5_BUILTIN_KDF
#define K5_BUILTIN_CMAC
#endif

#define K5_OPENSSL_AES
#define K5_OPENSSL_CAMELLIA
#define K5_OPENSSL_DES
#define K5_OPENSSL_HMAC
#define K5_OPENSSL_MD5
#define K5_OPENSSL_PBKDF2
#define K5_OPENSSL_SHA1
#define K5_OPENSSL_SHA2

#else

#define K5_BUILTIN_AES
#define K5_BUILTIN_CAMELLIA
#define K5_BUILTIN_CMAC
#define K5_BUILTIN_DES
#define K5_BUILTIN_DES_KEY_PARITY
#define K5_BUILTIN_HMAC
#define K5_BUILTIN_KDF
#define K5_BUILTIN_MD4
#define K5_BUILTIN_MD5
#define K5_BUILTIN_PBKDF2
#define K5_BUILTIN_RC4
#define K5_BUILTIN_SHA1
#define K5_BUILTIN_SHA2

#endif

/* Enc providers and hash providers specify well-known ciphers and hashes to be
 * implemented by the crypto module. */

struct krb5_enc_provider {
    /* keybytes is the input size to make_key;
       keylength is the output size */
    size_t block_size, keybytes, keylength;

    krb5_error_code (*encrypt)(krb5_key key, const krb5_data *cipher_state,
                               krb5_crypto_iov *data, size_t num_data);

    krb5_error_code (*decrypt)(krb5_key key, const krb5_data *cipher_state,
                               krb5_crypto_iov *data, size_t num_data);

    /* May be NULL if the cipher is not used for a cbc-mac checksum. */
    krb5_error_code (*cbc_mac)(krb5_key key, const krb5_crypto_iov *data,
                               size_t num_data, const krb5_data *ivec,
                               krb5_data *output);

    krb5_error_code (*init_state)(const krb5_keyblock *key,
                                  krb5_keyusage keyusage,
                                  krb5_data *out_state);
    void (*free_state)(krb5_data *state);

    /* May be NULL if there is no key-derived data cached.  */
    void (*key_cleanup)(krb5_key key);
};

struct krb5_hash_provider {
    char hash_name[8];
    size_t hashsize, blocksize;

    krb5_error_code (*hash)(const krb5_crypto_iov *data, size_t num_data,
                            krb5_data *output);
};

/*** RFC 3961 enctypes table ***/

#define MAX_ETYPE_ALIASES 2

struct krb5_keytypes;

typedef unsigned int (*crypto_length_func)(const struct krb5_keytypes *ktp,
                                           krb5_cryptotype type);

typedef krb5_error_code (*crypt_func)(const struct krb5_keytypes *ktp,
                                      krb5_key key, krb5_keyusage keyusage,
                                      const krb5_data *ivec,
                                      krb5_crypto_iov *data, size_t num_data);

typedef krb5_error_code (*str2key_func)(const struct krb5_keytypes *ktp,
                                        const krb5_data *string,
                                        const krb5_data *salt,
                                        const krb5_data *parm,
                                        krb5_keyblock *key);

typedef krb5_error_code (*rand2key_func)(const krb5_data *randombits,
                                         krb5_keyblock *key);

typedef krb5_error_code (*prf_func)(const struct krb5_keytypes *ktp,
                                    krb5_key key,
                                    const krb5_data *in, krb5_data *out);

struct krb5_keytypes {
    krb5_enctype etype;
    char *name;
    char *aliases[MAX_ETYPE_ALIASES];
    char *out_string;
    const struct krb5_enc_provider *enc;
    const struct krb5_hash_provider *hash;
    size_t prf_length;
    crypto_length_func crypto_length;
    crypt_func encrypt;
    crypt_func decrypt;
    str2key_func str2key;
    rand2key_func rand2key;
    prf_func prf;
    krb5_cksumtype required_ctype;
    krb5_flags flags;
    unsigned int ssf;
};

/*
 * "Weak" means the enctype is believed to be vulnerable to practical attacks,
 * and will be disabled unless allow_weak_crypto is set to true.  "Deprecated"
 * means the enctype has been deprecated by the IETF, and affects display and
 * logging.
 */
#define ETYPE_WEAK (1 << 0)
#define ETYPE_DEPRECATED (1 << 1)

extern const struct krb5_keytypes krb5int_enctypes_list[];
extern const int krb5int_enctypes_length;

/*** RFC 3961 checksum types table ***/

struct krb5_cksumtypes;

/*
 * Compute a checksum over the header, data, padding, and sign-only fields of
 * the iov array data (of size num_data).  The output buffer will already be
 * allocated with ctp->compute_size bytes available; the handler just needs to
 * fill in the contents.  If ctp->enc is not NULL, the handler can assume that
 * key is a valid-length key of an enctype which uses that enc provider.
 */
typedef krb5_error_code (*checksum_func)(const struct krb5_cksumtypes *ctp,
                                         krb5_key key, krb5_keyusage usage,
                                         const krb5_crypto_iov *data,
                                         size_t num_data,
                                         krb5_data *output);

/*
 * Verify a checksum over the header, data, padding, and sign-only fields of
 * the iov array data (of size num_data), and store the boolean result in
 * *valid.  The handler can assume that hash has length ctp->output_size.  If
 * ctp->enc is not NULL, the handler can assume that key a valid-length key of
 * an enctype which uses that enc provider.
 */
typedef krb5_error_code (*verify_func)(const struct krb5_cksumtypes *ctp,
                                       krb5_key key, krb5_keyusage usage,
                                       const krb5_crypto_iov *data,
                                       size_t num_data,
                                       const krb5_data *input,
                                       krb5_boolean *valid);

struct krb5_cksumtypes {
    krb5_cksumtype ctype;
    char *name;
    char *aliases[2];
    char *out_string;
    const struct krb5_enc_provider *enc;
    const struct krb5_hash_provider *hash;
    checksum_func checksum;
    verify_func verify;         /* NULL means recompute checksum and compare */
    unsigned int compute_size;  /* Allocation size for checksum computation */
    unsigned int output_size;   /* Possibly truncated output size */
    krb5_flags flags;
};

#define CKSUM_UNKEYED          0x0001
#define CKSUM_NOT_COLL_PROOF   0x0002

extern const struct krb5_cksumtypes krb5int_cksumtypes_list[];
extern const size_t krb5int_cksumtypes_length;

/*** Prototypes for enctype table functions ***/

/* Length */
unsigned int krb5int_raw_crypto_length(const struct krb5_keytypes *ktp,
                                       krb5_cryptotype type);
unsigned int krb5int_arcfour_crypto_length(const struct krb5_keytypes *ktp,
                                           krb5_cryptotype type);
unsigned int krb5int_dk_crypto_length(const struct krb5_keytypes *ktp,
                                      krb5_cryptotype type);
unsigned int krb5int_aes_crypto_length(const struct krb5_keytypes *ktp,
                                       krb5_cryptotype type);
unsigned int krb5int_camellia_crypto_length(const struct krb5_keytypes *ktp,
                                            krb5_cryptotype type);
unsigned int krb5int_aes2_crypto_length(const struct krb5_keytypes *ktp,
                                        krb5_cryptotype type);

/* Encrypt */
krb5_error_code krb5int_raw_encrypt(const struct krb5_keytypes *ktp,
                                    krb5_key key, krb5_keyusage usage,
                                    const krb5_data *ivec,
                                    krb5_crypto_iov *data, size_t num_data);
krb5_error_code krb5int_arcfour_encrypt(const struct krb5_keytypes *ktp,
                                        krb5_key key, krb5_keyusage usage,
                                        const krb5_data *ivec,
                                        krb5_crypto_iov *data,
                                        size_t num_data);
krb5_error_code krb5int_dk_encrypt(const struct krb5_keytypes *ktp,
                                   krb5_key key, krb5_keyusage usage,
                                   const krb5_data *ivec,
                                   krb5_crypto_iov *data, size_t num_data);
krb5_error_code krb5int_dk_cmac_encrypt(const struct krb5_keytypes *ktp,
                                        krb5_key key, krb5_keyusage usage,
                                        const krb5_data *ivec,
                                        krb5_crypto_iov *data,
                                        size_t num_data);
krb5_error_code krb5int_etm_encrypt(const struct krb5_keytypes *ktp,
                                    krb5_key key, krb5_keyusage usage,
                                    const krb5_data *ivec,
                                    krb5_crypto_iov *data, size_t num_data);

/* Decrypt */
krb5_error_code krb5int_raw_decrypt(const struct krb5_keytypes *ktp,
                                    krb5_key key, krb5_keyusage usage,
                                    const krb5_data *ivec,
                                    krb5_crypto_iov *data, size_t num_data);
krb5_error_code krb5int_arcfour_decrypt(const struct krb5_keytypes *ktp,
                                        krb5_key key, krb5_keyusage usage,
                                        const krb5_data *ivec,
                                        krb5_crypto_iov *data,
                                        size_t num_data);
krb5_error_code krb5int_dk_decrypt(const struct krb5_keytypes *ktp,
                                   krb5_key key, krb5_keyusage usage,
                                   const krb5_data *ivec,
                                   krb5_crypto_iov *data, size_t num_data);
krb5_error_code krb5int_dk_cmac_decrypt(const struct krb5_keytypes *ktp,
                                        krb5_key key, krb5_keyusage usage,
                                        const krb5_data *ivec,
                                        krb5_crypto_iov *data,
                                        size_t num_data);
krb5_error_code krb5int_etm_decrypt(const struct krb5_keytypes *ktp,
                                    krb5_key key, krb5_keyusage usage,
                                    const krb5_data *ivec,
                                    krb5_crypto_iov *data, size_t num_data);

/* String to key */
krb5_error_code krb5int_des_string_to_key(const struct krb5_keytypes *ktp,
                                          const krb5_data *string,
                                          const krb5_data *salt,
                                          const krb5_data *params,
                                          krb5_keyblock *key);
krb5_error_code krb5int_arcfour_string_to_key(const struct krb5_keytypes *ktp,
                                              const krb5_data *string,
                                              const krb5_data *salt,
                                              const krb5_data *params,
                                              krb5_keyblock *key);
krb5_error_code krb5int_dk_string_to_key(const struct krb5_keytypes *enc,
                                         const krb5_data *string,
                                         const krb5_data *salt,
                                         const krb5_data *params,
                                         krb5_keyblock *key);
krb5_error_code krb5int_aes_string_to_key(const struct krb5_keytypes *enc,
                                          const krb5_data *string,
                                          const krb5_data *salt,
                                          const krb5_data *params,
                                          krb5_keyblock *key);
krb5_error_code krb5int_camellia_string_to_key(const struct krb5_keytypes *enc,
                                               const krb5_data *string,
                                               const krb5_data *salt,
                                               const krb5_data *params,
                                               krb5_keyblock *key);
krb5_error_code krb5int_aes2_string_to_key(const struct krb5_keytypes *enc,
                                           const krb5_data *string,
                                           const krb5_data *salt,
                                           const krb5_data *params,
                                           krb5_keyblock *key);

/* Random to key */
krb5_error_code k5_rand2key_direct(const krb5_data *randombits,
                                   krb5_keyblock *keyblock);
krb5_error_code k5_rand2key_des3(const krb5_data *randombits,
                                 krb5_keyblock *keyblock);

/* Pseudo-random function */
krb5_error_code krb5int_des_prf(const struct krb5_keytypes *ktp,
                                krb5_key key, const krb5_data *in,
                                krb5_data *out);
krb5_error_code krb5int_arcfour_prf(const struct krb5_keytypes *ktp,
                                    krb5_key key, const krb5_data *in,
                                    krb5_data *out);
krb5_error_code krb5int_dk_prf(const struct krb5_keytypes *ktp, krb5_key key,
                               const krb5_data *in, krb5_data *out);
krb5_error_code krb5int_dk_cmac_prf(const struct krb5_keytypes *ktp,
                                    krb5_key key, const krb5_data *in,
                                    krb5_data *out);
krb5_error_code krb5int_aes2_prf(const struct krb5_keytypes *ktp, krb5_key key,
                                 const krb5_data *in, krb5_data *out);

/*** Prototypes for cksumtype handler functions ***/

krb5_error_code krb5int_unkeyed_checksum(const struct krb5_cksumtypes *ctp,
                                         krb5_key key, krb5_keyusage usage,
                                         const krb5_crypto_iov *data,
                                         size_t num_data,
                                         krb5_data *output);
krb5_error_code krb5int_hmacmd5_checksum(const struct krb5_cksumtypes *ctp,
                                         krb5_key key, krb5_keyusage usage,
                                         const krb5_crypto_iov *data,
                                         size_t num_data,
                                         krb5_data *output);
krb5_error_code krb5int_dk_checksum(const struct krb5_cksumtypes *ctp,
                                    krb5_key key, krb5_keyusage usage,
                                    const krb5_crypto_iov *data,
                                    size_t num_data, krb5_data *output);
krb5_error_code krb5int_dk_cmac_checksum(const struct krb5_cksumtypes *ctp,
                                         krb5_key key, krb5_keyusage usage,
                                         const krb5_crypto_iov *data,
                                         size_t num_data, krb5_data *output);
krb5_error_code krb5int_etm_checksum(const struct krb5_cksumtypes *ctp,
                                     krb5_key key, krb5_keyusage usage,
                                     const krb5_crypto_iov *data,
                                     size_t num_data, krb5_data *output);

/*** Key derivation functions ***/

enum deriv_alg {
    DERIVE_RFC3961,             /* RFC 3961 section 5.1 */
    DERIVE_SP800_108_CMAC,      /* NIST SP 800-108 with CMAC as PRF */
    DERIVE_SP800_108_HMAC       /* NIST SP 800-108 with HMAC as PRF */
};

krb5_error_code krb5int_derive_keyblock(const struct krb5_enc_provider *enc,
                                        const struct krb5_hash_provider *hash,
                                        krb5_key inkey, krb5_keyblock *outkey,
                                        const krb5_data *in_constant,
                                        enum deriv_alg alg);
krb5_error_code krb5int_derive_key(const struct krb5_enc_provider *enc,
                                   const struct krb5_hash_provider *hash,
                                   krb5_key inkey, krb5_key *outkey,
                                   const krb5_data *in_constant,
                                   enum deriv_alg alg);
krb5_error_code krb5int_derive_random(const struct krb5_enc_provider *enc,
                                      const struct krb5_hash_provider *hash,
                                      krb5_key inkey, krb5_data *outrnd,
                                      const krb5_data *in_constant,
                                      enum deriv_alg alg);

/*** Miscellaneous prototypes ***/

/* nfold algorithm from RFC 3961 */
void krb5int_nfold(unsigned int inbits, const unsigned char *in,
                   unsigned int outbits, unsigned char *out);

/* Translate an RFC 3961 key usage to a Microsoft RC4 usage. */
krb5_keyusage krb5int_arcfour_translate_usage(krb5_keyusage usage);

/* Ensure library initialization has occurred. */
int krb5int_crypto_init(void);

/* DES default state initialization handler (used by module enc providers). */
krb5_error_code krb5int_des_init_state(const krb5_keyblock *key,
                                       krb5_keyusage keyusage,
                                       krb5_data *state_out);

/* Default state cleanup handler (used by module enc providers). */
void krb5int_default_free_state(krb5_data *state);

/*** Input/output vector processing declarations **/

#define ENCRYPT_CONF_IOV(_iov)  ((_iov)->flags == KRB5_CRYPTO_TYPE_HEADER)

#define ENCRYPT_DATA_IOV(_iov)  ((_iov)->flags == KRB5_CRYPTO_TYPE_DATA || \
                                 (_iov)->flags == KRB5_CRYPTO_TYPE_PADDING)

#define ENCRYPT_IOV(_iov)       (ENCRYPT_CONF_IOV(_iov) || ENCRYPT_DATA_IOV(_iov))

#define SIGN_IOV(_iov)          (ENCRYPT_IOV(_iov) ||                   \
                                 (_iov)->flags == KRB5_CRYPTO_TYPE_SIGN_ONLY )

struct iov_cursor {
    const krb5_crypto_iov *iov; /* iov array we are iterating over */
    size_t iov_count;           /* size of iov array */
    size_t block_size;          /* size of blocks we will be obtaining */
    krb5_boolean signing;       /* should we process SIGN_ONLY blocks */
    size_t in_iov;              /* read index into iov array */
    size_t in_pos;              /* read index into iov contents */
    size_t out_iov;             /* write index into iov array */
    size_t out_pos;             /* write index into iov contents */
};

krb5_crypto_iov *krb5int_c_locate_iov(krb5_crypto_iov *data, size_t num_data,
                                      krb5_cryptotype type);

krb5_error_code krb5int_c_iov_decrypt_stream(const struct krb5_keytypes *ktp,
                                             krb5_key key,
                                             krb5_keyusage keyusage,
                                             const krb5_data *ivec,
                                             krb5_crypto_iov *data,
                                             size_t num_data);

unsigned int krb5int_c_padding_length(const struct krb5_keytypes *ktp,
                                      size_t data_length);

void k5_iov_cursor_init(struct iov_cursor *cursor, const krb5_crypto_iov *iov,
                        size_t count, size_t block_size, krb5_boolean signing);

krb5_boolean k5_iov_cursor_get(struct iov_cursor *cursor,
                               unsigned char *block);

void k5_iov_cursor_put(struct iov_cursor *cursor, unsigned char *block);

/*** Crypto module declarations ***/

/* Modules must implement the k5_sha256() function prototyped in k5-int.h. */

/* Modules must implement the following enc_providers and hash_providers: */
extern const struct krb5_enc_provider krb5int_enc_des3;
extern const struct krb5_enc_provider krb5int_enc_arcfour;
extern const struct krb5_enc_provider krb5int_enc_aes128;
extern const struct krb5_enc_provider krb5int_enc_aes256;
extern const struct krb5_enc_provider krb5int_enc_aes128_ctr;
extern const struct krb5_enc_provider krb5int_enc_aes256_ctr;
extern const struct krb5_enc_provider krb5int_enc_camellia128;
extern const struct krb5_enc_provider krb5int_enc_camellia256;

extern const struct krb5_hash_provider krb5int_hash_md4;
extern const struct krb5_hash_provider krb5int_hash_md5;
extern const struct krb5_hash_provider krb5int_hash_sha1;
extern const struct krb5_hash_provider krb5int_hash_sha256;
extern const struct krb5_hash_provider krb5int_hash_sha384;

/* Modules must implement the following functions. */

/* Set the parity bits to the correct values in keybits. */
void k5_des_fixup_key_parity(unsigned char *keybits);

/* Compute an HMAC using the provided hash function, key, and data, storing the
 * result into output (caller-allocated). */
krb5_error_code krb5int_hmac(const struct krb5_hash_provider *hash,
                             krb5_key key, const krb5_crypto_iov *data,
                             size_t num_data, krb5_data *output);

/* Compute a CMAC checksum over data. */
krb5_error_code krb5int_cmac_checksum(const struct krb5_enc_provider *enc,
                                      krb5_key key,
                                      const krb5_crypto_iov *data,
                                      size_t num_data, krb5_data *output);

/* As above, using a keyblock as the key input. */
krb5_error_code krb5int_hmac_keyblock(const struct krb5_hash_provider *hash,
                                      const krb5_keyblock *keyblock,
                                      const krb5_crypto_iov *data,
                                      size_t num_data, krb5_data *output);

/*
 * Compute the PBKDF2 (see RFC 2898) of password and salt, with the specified
 * count, using HMAC with the specified hash as the pseudo-random function,
 * storing the result into out (caller-allocated).
 */
krb5_error_code krb5int_pbkdf2_hmac(const struct krb5_hash_provider *hash,
                                    const krb5_data *out, unsigned long count,
                                    const krb5_data *password,
                                    const krb5_data *salt);

/*
 * Compute the NIST SP800-108 KDF in counter mode (section 5.1) with the
 * following parameters:
 *   - HMAC (with hash as the hash provider) is the PRF.
 *   - A block counter of four bytes is used.
 *   - Four bytes are used to encode the output length in the PRF input.
 *
 * There are no uses requiring more than a single PRF invocation.
 */
krb5_error_code
k5_sp800_108_counter_hmac(const struct krb5_hash_provider *hash,
                          krb5_key key, const krb5_data *label,
                          const krb5_data *context, krb5_data *rnd_out);

/*
 * Compute the NIST SP800-108 KDF in feedback mode (section 5.2) with the
 * following parameters:
 *   - CMAC (with enc as the enc provider) is the PRF.
 *   - A block counter of four bytes is used.
 *   - Label is the key derivation constant.
 *   - Context is empty.
 *   - Four bytes are used to encode the output length in the PRF input.
 */
krb5_error_code
k5_sp800_108_feedback_cmac(const struct krb5_enc_provider *enc, krb5_key key,
                           const krb5_data *label, krb5_data *rnd_out);

/* Compute the RFC 3961 section 5.1 KDF (which uses n-fold) with enc as E,
 * inkey as Key, and in_constant as Constant. */
krb5_error_code
k5_derive_random_rfc3961(const struct krb5_enc_provider *enc, krb5_key key,
                         const krb5_data *constant, krb5_data *rnd_out);

/* The following are used by test programs and are just handler functions from
 * the AES and Camellia enc providers. */
krb5_error_code krb5int_aes_encrypt(krb5_key key, const krb5_data *ivec,
                                    krb5_crypto_iov *data, size_t num_data);
krb5_error_code krb5int_aes_decrypt(krb5_key key, const krb5_data *ivec,
                                    krb5_crypto_iov *data, size_t num_data);
krb5_error_code krb5int_camellia_encrypt(krb5_key key, const krb5_data *ivec,
                                         krb5_crypto_iov *data,
                                         size_t num_data);

/*** Inline helper functions ***/

/* Find an enctype by number in the enctypes table. */
static inline const struct krb5_keytypes *
find_enctype(krb5_enctype enctype)
{
    int i;

    for (i = 0; i < krb5int_enctypes_length; i++) {
        if (krb5int_enctypes_list[i].etype == enctype)
            break;
    }

    if (i == krb5int_enctypes_length)
        return NULL;
    return &krb5int_enctypes_list[i];
}

/* Find a checksum type by number in the cksumtypes table. */
static inline const struct krb5_cksumtypes *
find_cksumtype(krb5_cksumtype ctype)
{
    size_t i;

    for (i = 0; i < krb5int_cksumtypes_length; i++) {
        if (krb5int_cksumtypes_list[i].ctype == ctype)
            break;
    }

    if (i == krb5int_cksumtypes_length)
        return NULL;
    return &krb5int_cksumtypes_list[i];
}

/* Verify that a key is appropriate for a checksum type. */
static inline krb5_error_code
verify_key(const struct krb5_cksumtypes *ctp, krb5_key key)
{
    const struct krb5_keytypes *ktp;

    ktp = key ? find_enctype(key->keyblock.enctype) : NULL;
    if (ctp->enc != NULL && (!ktp || ktp->enc != ctp->enc))
        return KRB5_BAD_ENCTYPE;
    if (key && (!ktp || key->keyblock.length != ktp->enc->keylength))
        return KRB5_BAD_KEYSIZE;
    return 0;
}

/* Encrypt one block of plaintext in place, for block ciphers. */
static inline krb5_error_code
encrypt_block(const struct krb5_enc_provider *enc, krb5_key key,
              krb5_data *block)
{
    krb5_crypto_iov iov;

    /* Verify that this is a block cipher and block is the right length. */
    if (block->length != enc->block_size || enc->block_size == 1)
        return EINVAL;
    iov.flags = KRB5_CRYPTO_TYPE_DATA;
    iov.data = *block;
    if (enc->cbc_mac != NULL)   /* One-block cbc-mac with no ivec. */
        return enc->cbc_mac(key, &iov, 1, NULL, block);
    else                        /* Assume cbc-mode encrypt. */
        return enc->encrypt(key, 0, &iov, 1);
}

/* Return the total length of the to-be-signed or to-be-encrypted buffers in an
 * iov chain. */
static inline size_t
iov_total_length(const krb5_crypto_iov *data, size_t num_data,
                 krb5_boolean signing)
{
    size_t i, total = 0;

    for (i = 0; i < num_data; i++) {
        if (signing ? SIGN_IOV(&data[i]) : ENCRYPT_IOV(&data[i]))
            total += data[i].data.length;
    }
    return total;
}

/*
 * Return the number of contiguous blocks available within the current input
 * IOV of the cursor c, so that the caller can do in-place encryption.
 * Do not call if c might be exhausted.
 */
static inline size_t
iov_cursor_contig_blocks(struct iov_cursor *c)
{
    return (c->iov[c->in_iov].data.length - c->in_pos) / c->block_size;
}

/* Return the current input pointer within the cursor c.  Do not call if c
 * might be exhausted. */
static inline unsigned char *
iov_cursor_ptr(struct iov_cursor *c)
{
    return (unsigned char *)&c->iov[c->in_iov].data.data[c->in_pos];
}

/*
 * Advance the input and output pointers of c by nblocks blocks.  nblocks must
 * not be greater than the return value of iov_cursor_contig_blocks, and the
 * input and output positions must be identical.
 */
static inline void
iov_cursor_advance(struct iov_cursor *c, size_t nblocks)
{
    c->in_pos += nblocks * c->block_size;
    c->out_pos += nblocks * c->block_size;
}

#endif /* CRYPTO_INT_H */
