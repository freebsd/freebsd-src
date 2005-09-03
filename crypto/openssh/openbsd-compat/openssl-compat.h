/* $Id: openssl-compat.h,v 1.1 2005/06/09 11:45:11 dtucker Exp $ */

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
#include <openssl/evp.h>

#if OPENSSL_VERSION_NUMBER < 0x00906000L
# define SSH_OLD_EVP
# define EVP_CIPHER_CTX_get_app_data(e)		((e)->app_data)
#endif

#if OPENSSL_VERSION_NUMBER < 0x00907000L
# define EVP_aes_128_cbc evp_rijndael
# define EVP_aes_192_cbc evp_rijndael
# define EVP_aes_256_cbc evp_rijndael
extern const EVP_CIPHER *evp_rijndael(void);
extern void ssh_rijndael_iv(EVP_CIPHER_CTX *, int, u_char *, u_int);
#endif

#if !defined(EVP_CTRL_SET_ACSS_MODE)
# if (OPENSSL_VERSION_NUMBER >= 0x00907000L)
#  define USE_CIPHER_ACSS 1
extern const EVP_CIPHER *evp_acss(void);
#  define EVP_acss evp_acss
# else
#  define EVP_acss NULL
# endif
#endif

/*
 * insert comment here
 */
#ifdef SSH_OLD_EVP

# ifndef SSH_DONT_REDEF_EVP

#  ifdef EVP_Cipher
#   undef EVP_Cipher
#  endif

#  define EVP_CipherInit(a,b,c,d,e)	ssh_EVP_CipherInit((a),(b),(c),(d),(e))
#  define EVP_Cipher(a,b,c,d)		ssh_EVP_Cipher((a),(b),(c),(d))
#  define EVP_CIPHER_CTX_cleanup(a)	ssh_EVP_CIPHER_CTX_cleanup((a))
# endif

int ssh_EVP_CipherInit(EVP_CIPHER_CTX *, const EVP_CIPHER *, unsigned char *,
    unsigned char *, int);
int ssh_EVP_Cipher(EVP_CIPHER_CTX *, char *, char *, int);
int ssh_EVP_CIPHER_CTX_cleanup(EVP_CIPHER_CTX *);
#endif
