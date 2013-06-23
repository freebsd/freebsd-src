/*
 * util/random.c - thread safe random generator, which is reasonably secure.
 * 
 * Copyright (c) 2007, NLnet Labs. All rights reserved.
 * 
 * This software is open source.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * Neither the name of the NLNET LABS nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * \file
 * Thread safe random functions. Similar to arc4random() with an explicit
 * initialisation routine.
 *
 * The code in this file is based on arc4random from
 * openssh-4.0p1/openbsd-compat/bsd-arc4random.c
 * That code is also BSD licensed. Here is their statement:
 *
 * Copyright (c) 1996, David Mazieres <dm@uun.org>
 * Copyright (c) 2008, Damien Miller <djm@openbsd.org>
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
#include "config.h"
#include "util/random.h"
#include "util/log.h"
#ifdef HAVE_SSL
#include <openssl/rand.h>
#include <openssl/rc4.h>
#include <openssl/err.h>
#elif defined(HAVE_NSS)
/* nspr4 */
#include "prerror.h"
/* nss3 */
#include "secport.h"
#include "pk11pub.h"
#endif

/** 
 * Max random value.  Similar to RAND_MAX, but more portable
 * (mingw uses only 15 bits random).
 */
#define MAX_VALUE 0x7fffffff

#ifdef HAVE_SSL
/**
 * Struct with per-thread random state.
 * Keeps SSL types away from the header file.
 */
struct ub_randstate {
	/** key used for arc4random generation */
	RC4_KEY rc4;
	/** keeps track of key usage */
	int rc4_ready;
};

/** Size of key to use (must be multiple of 8) */
#define SEED_SIZE 24

/** Number of bytes to reseed after */
#define REKEY_BYTES	(1 << 24)

/* (re)setup system seed */
void
ub_systemseed(unsigned int seed)
{
	/* RAND_ is threadsafe, by the way */
	if(!RAND_status()) {
		/* try to seed it */
		unsigned char buf[256];
		unsigned int v = seed;
		size_t i;
		for(i=0; i<256/sizeof(seed); i++) {
			memmove(buf+i*sizeof(seed), &v, sizeof(seed));
			v = v*seed + (unsigned int)i;
		}
		RAND_seed(buf, 256);
		if(!RAND_status()) {
			log_err("Random generator has no entropy "
				"(error %ld)", ERR_get_error());
		} else {
			verbose(VERB_OPS, "openssl has no entropy, "
				"seeding with time and pid");
		}
	}
}

/** reseed random generator */
static void
ub_arc4random_stir(struct ub_randstate* s, struct ub_randstate* from)
{
	/* not as unsigned char, but longerint so that it is
	   aligned properly on alignment sensitive platforms */
	uint64_t rand_buf[SEED_SIZE/sizeof(uint64_t)];
	int i;

	memset(&s->rc4, 0, sizeof(s->rc4));
	memset(rand_buf, 0xc, sizeof(rand_buf));
	if (from) {
		uint8_t* rbuf = (uint8_t*)rand_buf;
		for(i=0; i<SEED_SIZE; i++)
			rbuf[i] = (uint8_t)ub_random(from);
	} else {
		if(!RAND_status())
			ub_systemseed((unsigned)getpid()^(unsigned)time(NULL));
		if (RAND_bytes((unsigned char*)rand_buf,
			(int)sizeof(rand_buf)) <= 0) {
			/* very unlikely that this happens, since we seeded
			 * above, if it does; complain and keep going */
			log_err("Couldn't obtain random bytes (error %ld)",
				    ERR_get_error());
			s->rc4_ready = 256;
			return;
		}
	}
#ifdef HAVE_FIPS_MODE
	if(FIPS_mode()) {
		/* RC4 is not allowed, get some trustworthy randomness */
		/* double certainty here, this routine should not be
		 * called in FIPS_mode */
		memset(rand_buf, 0, sizeof(rand_buf));
		s->rc4_ready = REKEY_BYTES;
		return;
	}
#endif /* FIPS_MODE */
	RC4_set_key(&s->rc4, SEED_SIZE, (unsigned char*)rand_buf);

	/*
	 * Discard early keystream, as per recommendations in:
	 * http://www.wisdom.weizmann.ac.il/~itsik/RC4/Papers/Rc4_ksa.ps
	 */
	for(i = 0; i <= 256; i += sizeof(rand_buf))
		RC4(&s->rc4, sizeof(rand_buf), (unsigned char*)rand_buf,
			(unsigned char*)rand_buf);

	memset(rand_buf, 0, sizeof(rand_buf));

	s->rc4_ready = REKEY_BYTES;
}

struct ub_randstate* 
ub_initstate(unsigned int seed, struct ub_randstate* from)
{
	struct ub_randstate* s = (struct ub_randstate*)calloc(1, sizeof(*s));
	if(!s) {
		log_err("malloc failure in random init");
		return NULL;
	}
	ub_systemseed(seed);
#ifdef HAVE_FIPS_MODE
	if(!FIPS_mode())
#endif
	ub_arc4random_stir(s, from);
	return s;
}

long int 
ub_random(struct ub_randstate* s)
{
	unsigned int r = 0;
#ifdef HAVE_FIPS_MODE
	if(FIPS_mode()) {
		/* RC4 is not allowed, get some trustworthy randomness */
		/* we use pseudo bytes: it tries to return secure randomness
		 * but returns 'something' if that fails.  We need something
		 * else if it fails, because we cannot block here */
		if(RAND_pseudo_bytes((unsigned char*)&r, (int)sizeof(r))
			== -1) {
			log_err("FIPSmode, no arc4random but RAND failed "
				"(error %ld)", ERR_get_error());
		}
		return (long int)((r) % (((unsigned)MAX_VALUE + 1)));
	}
#endif /* FIPS_MODE */
	if (s->rc4_ready <= 0) {
		ub_arc4random_stir(s, NULL);
	}

	RC4(&s->rc4, sizeof(r), 
		(unsigned char *)&r, (unsigned char *)&r);
	s->rc4_ready -= sizeof(r);
	return (long int)((r) % (((unsigned)MAX_VALUE + 1)));
}

#elif defined(HAVE_NSS)

/* not much to remember for NSS since we use its pk11_random, placeholder */
struct ub_randstate {
	int ready;
};

void ub_systemseed(unsigned int ATTR_UNUSED(seed))
{
}

struct ub_randstate* ub_initstate(unsigned int ATTR_UNUSED(seed), 
	struct ub_randstate* ATTR_UNUSED(from))
{
	struct ub_randstate* s = (struct ub_randstate*)calloc(1, sizeof(*s));
	if(!s) {
		log_err("malloc failure in random init");
		return NULL;
	}
	return s;
}

long int ub_random(struct ub_randstate* ATTR_UNUSED(state))
{
	long int x;
	/* random 31 bit value. */
	SECStatus s = PK11_GenerateRandom((unsigned char*)&x, (int)sizeof(x));
	if(s != SECSuccess) {
		log_err("PK11_GenerateRandom error: %s",
			PORT_ErrorToString(PORT_GetError()));
	}
	return x & MAX_VALUE;
}

#endif /* HAVE_SSL or HAVE_NSS */

long int
ub_random_max(struct ub_randstate* state, long int x)
{
	/* make sure we fetch in a range that is divisible by x. ignore
	 * values from d .. MAX_VALUE, instead draw a new number */
	long int d = MAX_VALUE - (MAX_VALUE % x); /* d is divisible by x */
	long int v = ub_random(state);
	while(d <= v)
		v = ub_random(state);
	return (v % x);
}

void 
ub_randfree(struct ub_randstate* s)
{
	if(s)
		free(s);
	/* user app must do RAND_cleanup(); */
}
