/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef __OSSL_PPC__
#define __OSSL_PPC__

#include <crypto/openssl/ossl.h>
#include <crypto/openssl/ossl_cipher.h>

extern unsigned int OPENSSL_ppccap_P;

/*
 * Flags' usage can appear ambiguous, because they are set rather
 * to reflect OpenSSL performance preferences than actual processor
 * capabilities.
 */
# define PPC_FPU64       (1<<0)
# define PPC_ALTIVEC     (1<<1)
# define PPC_CRYPTO207   (1<<2)
# define PPC_FPU         (1<<3)
# define PPC_MADD300     (1<<4)
# define PPC_MFTB        (1<<5)
# define PPC_MFSPR268    (1<<6)

/* aesp8-ppc.S */
ossl_cipher_encrypt_t aes_p8_cbc_encrypt;
/* vpaes-ppc.S */
ossl_cipher_encrypt_t vpaes_cbc_encrypt;

static void
AES_CBC_ENCRYPT(const unsigned char *in, unsigned char *out,
    size_t length, const void *key, unsigned char *iv, int encrypt)
{
	if (OPENSSL_ppccap_P & PPC_CRYPTO207)
		aes_p8_cbc_encrypt(in, out, length, key, iv, encrypt);
	else
		vpaes_cbc_encrypt(in, out, length, key, iv, encrypt);
}
#endif
