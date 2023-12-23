/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef __OSSL_AARCH64__
#define __OSSL_AARCH64__

#include <crypto/openssl/ossl.h>
#include <crypto/openssl/ossl_cipher.h>
#include <crypto/openssl/arm_arch.h>

/* aesv8-armx.S */
ossl_cipher_encrypt_t aes_v8_cbc_encrypt;
/* vpaes-armv8.S */
ossl_cipher_encrypt_t vpaes_cbc_encrypt;

static void
AES_CBC_ENCRYPT(const unsigned char *in, unsigned char *out,
    size_t length, const void *key, unsigned char *iv, int encrypt)
{
	if (OPENSSL_armcap_P & ARMV8_AES)
		aes_v8_cbc_encrypt(in, out, length, key, iv, encrypt);
	else
		vpaes_cbc_encrypt(in, out, length, key, iv, encrypt);
}
#endif
