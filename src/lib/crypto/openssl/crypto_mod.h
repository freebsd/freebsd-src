/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/openssl/crypto_mod.h - OpenSSL crypto module declarations */
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

/*
 * This header is included from lib/crypto/krb/crypto_int.h to provide
 * module-specific declarations.  It is not included directly from source
 * files.
 */

#ifndef CRYPTO_MOD_H
#define CRYPTO_MOD_H

#include <openssl/crypto.h>
#include <openssl/aes.h>
#include <openssl/sha.h>

/* 1.1 standardizes constructor and destructor names, renaming
 * EVP_MD_CTX_create and EVP_MD_CTX_destroy. */
#if OPENSSL_VERSION_NUMBER < 0x10100000L
#define EVP_MD_CTX_new EVP_MD_CTX_create
#define EVP_MD_CTX_free EVP_MD_CTX_destroy
#endif

#define aes_ctx AES_KEY
#define krb5int_aes_enc_key(k, len, ctx) AES_set_encrypt_key(k, 8*(len), ctx)
#define krb5int_aes_enc_blk(in, out, ctx) AES_encrypt(in, out, ctx)
#define k5_sha256_init SHA256_Init
#define k5_sha256_update SHA256_Update
#define k5_sha256_final SHA256_Final

#endif /* CRYPTO_MOD_H */
