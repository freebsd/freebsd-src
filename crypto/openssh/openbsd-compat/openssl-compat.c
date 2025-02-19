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

#define SSH_DONT_OVERLOAD_OPENSSL_FUNCS
#include "includes.h"

#ifdef WITH_OPENSSL

#include <stdarg.h>
#include <string.h>

#ifdef USE_OPENSSL_ENGINE
# include <openssl/engine.h>
# include <openssl/conf.h>
#endif

#include "log.h"

#include "openssl-compat.h"

/*
 * OpenSSL version numbers: MNNFFPPS: major minor fix patch status
 * Versions >=3 require only major versions to match.
 * For versions <3, we accept compatible fix versions (so we allow 1.0.1
 * to work with 1.0.0). Going backwards is only allowed within a patch series.
 * See https://www.openssl.org/policies/releasestrat.html
 */

int
ssh_compatible_openssl(long headerver, long libver)
{
	long mask, hfix, lfix;

	/* exact match is always OK */
	if (headerver == libver)
		return 1;

	/*
	 * For versions >= 3.0, only the major and status must match.
	 */
	if (headerver >= 0x3000000f) {
		mask = 0xf000000fL; /* major,status */
		return (headerver & mask) == (libver & mask);
	}

	/*
	 * For versions >= 1.0.0, but <3, major,minor,status must match and
	 * library fix version must be equal to or newer than the header.
	 */
	mask = 0xfff0000fL; /* major,minor,status */
	hfix = (headerver & 0x000ff000) >> 12;
	lfix = (libver & 0x000ff000) >> 12;
	if ( (headerver & mask) == (libver & mask) && lfix >= hfix)
		return 1;
	return 0;
}

void
ssh_libcrypto_init(void)
{
#if defined(HAVE_OPENSSL_INIT_CRYPTO) && \
      defined(OPENSSL_INIT_ADD_ALL_CIPHERS) && \
      defined(OPENSSL_INIT_ADD_ALL_DIGESTS)
	OPENSSL_init_crypto(OPENSSL_INIT_ADD_ALL_CIPHERS |
	    OPENSSL_INIT_ADD_ALL_DIGESTS, NULL);
#elif defined(HAVE_OPENSSL_ADD_ALL_ALGORITHMS)
	OpenSSL_add_all_algorithms();
#endif

#ifdef	USE_OPENSSL_ENGINE
	/* Enable use of crypto hardware */
	ENGINE_load_builtin_engines();
	ENGINE_register_all_complete();

	/* Load the libcrypto config file to pick up engines defined there */
# if defined(HAVE_OPENSSL_INIT_CRYPTO) && defined(OPENSSL_INIT_LOAD_CONFIG)
	OPENSSL_init_crypto(OPENSSL_INIT_ADD_ALL_CIPHERS |
	    OPENSSL_INIT_ADD_ALL_DIGESTS | OPENSSL_INIT_LOAD_CONFIG, NULL);
# else
	OPENSSL_config(NULL);
# endif
#endif /* USE_OPENSSL_ENGINE */
}

#ifndef HAVE_EVP_DIGESTSIGN
int
EVP_DigestSign(EVP_MD_CTX *ctx, unsigned char *sigret, size_t *siglen,
    const unsigned char *tbs, size_t tbslen)
{
	if (sigret != NULL) {
		if (EVP_DigestSignUpdate(ctx, tbs, tbslen) <= 0)
			return 0;
	}

	return EVP_DigestSignFinal(ctx, sigret, siglen);
}
#endif

#ifndef HAVE_EVP_DIGESTVERIFY
int
EVP_DigestVerify(EVP_MD_CTX *ctx, const unsigned char *sigret, size_t siglen,
    const unsigned char *tbs, size_t tbslen)
{
	if (EVP_DigestVerifyUpdate(ctx, tbs, tbslen) <= 0)
		return -1;

	return EVP_DigestVerifyFinal(ctx, sigret, siglen);
}
#endif

#endif /* WITH_OPENSSL */
