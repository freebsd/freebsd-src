/*
 * Crypto-quality random number functions
 *
 * Author: Harlan Stenn, 2014
 *
 * This file is Copyright (c) 2014 by Network Time Foundation.
 * BSD terms apply: see the file COPYRIGHT in the distribution root for details.
 */

#include "config.h"
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <stdio.h>

#include <ntp_stdlib.h>
#include <ntp_random.h>
#include "safecast.h"

#ifdef USE_OPENSSL_CRYPTO_RAND
#include <openssl/err.h>
#include <openssl/rand.h>

int crypto_rand_init = 0;
#elif !defined(HAVE_ARC4RANDOM_BUF)
#include <event2/util.h>
#endif

int crypto_rand_ok = 0;

/*
 * As of late 2014, here's how we plan to provide cryptographic-quality
 * random numbers:
 *
 * - If we are building with OpenSSL, use RAND_poll() and RAND_bytes().
 * - Otherwise, use arc4random().
 *
 * Use of arc4random() can be forced using configure options 
 * --disable-openssl-random or --without-crypto.
 *
 * We can count on arc4random existing, thru the OS or thru libevent.
 * The quality of arc4random depends on the implementor.
 *
 * RAND_poll() doesn't show up until XXX.  If it's not present, we
 * need to either provide our own or use arc4random().
 */

 /*
  * ntp_crypto_srandom:
  *
  * Initialize the random number generator, if needed by the underlying
  * crypto random number generation mechanism.
  */

void
ntp_crypto_srandom(
	void
)
{
#ifdef USE_OPENSSL_CRYPTO_RAND
	if (!crypto_rand_init) {
		if (RAND_poll())
			crypto_rand_ok = 1;
		crypto_rand_init = 1;
	}
#elif HAVE_ARC4RANDOM_BUF
	/* 
	 * arc4random_buf has no error return and needs no seeding nor reseeding.
	 */
	crypto_rand_ok = 1;
#else
	/*
	 * Explicitly init libevent secure RNG to make sure it seeds.
	 * This is the only way we can tell if it can successfully get
	 * entropy from the system.
	 */
	if (!evutil_secure_rng_init())
		crypto_rand_ok = 1;
#endif
}


/*
 * ntp_crypto_random_buf:  Used by ntp-keygen
 *
 * Returns 0 on success, -1 on error.
 */
int
ntp_crypto_random_buf(
	void *buf,
	size_t nbytes
	)
{
	if (!crypto_rand_ok)
		return -1;

#if defined(USE_OPENSSL_CRYPTO_RAND)
	if (1 != RAND_bytes(buf, size2int_chk(nbytes))) {
		unsigned long err;
		char *err_str;

		err = ERR_get_error();
		err_str = ERR_error_string(err, NULL);
		msyslog(LOG_ERR, "RAND_bytes failed: %s", err_str);

		return -1;
	}
#elif defined(HAVE_ARC4RANDOM_BUF)
	arc4random_buf(buf, nbytes);
#else
	evutil_secure_rng_get_bytes(buf, nbytes);
#endif
	return 0;
}
