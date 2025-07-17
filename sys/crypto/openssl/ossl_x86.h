/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef __OSSL_X86__
#define __OSSL_X86__

#include <crypto/openssl/ossl.h>
#include <crypto/openssl/ossl_cipher.h>

/* aesni-x86_64.S, aesni-x86.S */
ossl_cipher_encrypt_t aesni_cbc_encrypt;

#define AES_CBC_ENCRYPT aesni_cbc_encrypt
#endif
