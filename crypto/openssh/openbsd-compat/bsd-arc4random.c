/*
 * Copyright (c) 1999-2000 Damien Miller.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"
#include "log.h"

RCSID("$Id: bsd-arc4random.c,v 1.5 2002/05/08 22:57:18 tim Exp $");

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

unsigned int arc4random(void)
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

void arc4random_stir(void)
{
	unsigned char rand_buf[SEED_SIZE];

	memset(&rc4, 0, sizeof(rc4));
	if (!RAND_bytes(rand_buf, sizeof(rand_buf)))
		fatal("Couldn't obtain random bytes (error %ld)",
		    ERR_get_error());
	RC4_set_key(&rc4, sizeof(rand_buf), rand_buf);
	memset(rand_buf, 0, sizeof(rand_buf));

	rc4_ready = REKEY_BYTES;
}
#endif /* !HAVE_ARC4RANDOM */
