/* $Id: openssl-compat.c,v 1.8 2009/03/07 11:22:35 dtucker Exp $ */

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

#ifdef USE_OPENSSL_ENGINE
# include <openssl/engine.h>
#endif

#define SSH_DONT_OVERLOAD_OPENSSL_FUNCS
#include "openssl-compat.h"

#ifdef SSH_OLD_EVP
int
ssh_EVP_CipherInit(EVP_CIPHER_CTX *evp, const EVP_CIPHER *type,
    unsigned char *key, unsigned char *iv, int enc)
{
	EVP_CipherInit(evp, type, key, iv, enc);
	return 1;
}

int
ssh_EVP_Cipher(EVP_CIPHER_CTX *evp, char *dst, char *src, int len)
{
	EVP_Cipher(evp, dst, src, len);
	return 1;
}

int
ssh_EVP_CIPHER_CTX_cleanup(EVP_CIPHER_CTX *evp)
{
	EVP_CIPHER_CTX_cleanup(evp);
	return 1;
}
#endif

#ifdef OPENSSL_EVP_DIGESTUPDATE_VOID
int
ssh_EVP_DigestUpdate(EVP_MD_CTX *ctx, const void *d, unsigned int cnt)
{
	EVP_DigestUpdate(ctx, d, cnt);
	return 1;
}
#endif

#ifdef	USE_OPENSSL_ENGINE
void
ssh_SSLeay_add_all_algorithms(void)
{
	SSLeay_add_all_algorithms();

	/* Enable use of crypto hardware */
	ENGINE_load_builtin_engines();
	ENGINE_register_all_complete();
}
#endif
