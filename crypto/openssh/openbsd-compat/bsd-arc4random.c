/*
 * Copyright (c) 1999,2000,2004 Damien Miller <djm@mindrot.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "includes.h"

#include <sys/types.h>

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "log.h"

#ifndef HAVE_ARC4RANDOM

#include <openssl/rand.h>
#include <openssl/rc4.h>
#include <openssl/err.h>

/* Size of key to use */
#define SEED_SIZE 20

/* Number of bytes to reseed after */
#define REKEY_BYTES	(1 << 24)

static int rc4_ready = 0;
static RC4_KEY rc4;

unsigned int
arc4random(void)
{
	unsigned int r = 0;
	static int first_time = 1;

	if (rc4_ready <= 0) {
		if (first_time)
			seed_rng();
		first_time = 0;
		arc4random_stir();
	}

	RC4(&rc4, sizeof(r), (unsigned char *)&r, (unsigned char *)&r);

	rc4_ready -= sizeof(r);
	
	return(r);
}

void
arc4random_stir(void)
{
	unsigned char rand_buf[SEED_SIZE];
	int i;

	memset(&rc4, 0, sizeof(rc4));
	if (RAND_bytes(rand_buf, sizeof(rand_buf)) <= 0)
		fatal("Couldn't obtain random bytes (error %ld)",
		    ERR_get_error());
	RC4_set_key(&rc4, sizeof(rand_buf), rand_buf);

	/*
	 * Discard early keystream, as per recommendations in:
	 * http://www.wisdom.weizmann.ac.il/~itsik/RC4/Papers/Rc4_ksa.ps
	 */
	for(i = 0; i <= 256; i += sizeof(rand_buf))
		RC4(&rc4, sizeof(rand_buf), rand_buf, rand_buf);

	memset(rand_buf, 0, sizeof(rand_buf));

	rc4_ready = REKEY_BYTES;
}
#endif /* !HAVE_ARC4RANDOM */

#ifndef ARC4RANDOM_BUF
void
arc4random_buf(void *_buf, size_t n)
{
	size_t i;
	u_int32_t r = 0;
	char *buf = (char *)_buf;

	for (i = 0; i < n; i++) {
		if (i % 4 == 0)
			r = arc4random();
		buf[i] = r & 0xff;
		r >>= 8;
	}
	i = r = 0;
}
#endif /* !HAVE_ARC4RANDOM_BUF */

#ifndef ARC4RANDOM_UNIFORM
/*
 * Calculate a uniformly distributed random number less than upper_bound
 * avoiding "modulo bias".
 *
 * Uniformity is achieved by generating new random numbers until the one
 * returned is outside the range [0, 2**32 % upper_bound).  This
 * guarantees the selected random number will be inside
 * [2**32 % upper_bound, 2**32) which maps back to [0, upper_bound)
 * after reduction modulo upper_bound.
 */
u_int32_t
arc4random_uniform(u_int32_t upper_bound)
{
	u_int32_t r, min;

	if (upper_bound < 2)
		return 0;

#if (ULONG_MAX > 0xffffffffUL)
	min = 0x100000000UL % upper_bound;
#else
	/* Calculate (2**32 % upper_bound) avoiding 64-bit math */
	if (upper_bound > 0x80000000)
		min = 1 + ~upper_bound;		/* 2**32 - upper_bound */
	else {
		/* (2**32 - (x * 2)) % x == 2**32 % x when x <= 2**31 */
		min = ((0xffffffff - (upper_bound * 2)) + 1) % upper_bound;
	}
#endif

	/*
	 * This could theoretically loop forever but each retry has
	 * p > 0.5 (worst case, usually far better) of selecting a
	 * number inside the range we need, so it should rarely need
	 * to re-roll.
	 */
	for (;;) {
		r = arc4random();
		if (r >= min)
			break;
	}

	return r % upper_bound;
}
#endif /* !HAVE_ARC4RANDOM_UNIFORM */
