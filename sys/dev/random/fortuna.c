/*-
 * Copyright (c) 2013-2014 Mark R V Murray
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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
 *
 */

/* This implementation of Fortuna is based on the descriptions found in
 * ISBN 0-471-22357-3 "Practical Cryptography" by Ferguson and Schneier
 * ("F&S").
 *
 * The above book is superseded by ISBN 978-0-470-47424-2 "Cryptography
 * Engineering" by Ferguson, Schneier and Kohno ("FS&K").  The code has
 * not yet fully caught up with FS&K.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifdef _KERNEL
#include "opt_random.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/random.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <machine/cpu.h>

#include <crypto/rijndael/rijndael-api-fst.h>
#include <crypto/sha2/sha2.h>

#include <dev/random/hash.h>
#include <dev/random/randomdev.h>
#include <dev/random/random_adaptors.h>
#include <dev/random/random_harvestq.h>
#include <dev/random/uint128.h>
#include <dev/random/fortuna.h>
#else /* !_KERNEL */
#include <sys/param.h>
#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "unit_test.h"

#include <crypto/rijndael/rijndael-api-fst.h>
#include <crypto/sha2/sha2.h>

#include <dev/random/hash.h>
#include <dev/random/uint128.h>
#include <dev/random/fortuna.h>
#endif /* _KERNEL */

#if !defined(RANDOM_YARROW) && !defined(RANDOM_FORTUNA)
#define RANDOM_YARROW
#elif defined(RANDOM_YARROW) && defined(RANDOM_FORTUNA)
#error "Must define either RANDOM_YARROW or RANDOM_FORTUNA"
#endif

#if defined(RANDOM_FORTUNA)

#define NPOOLS 32
#define MINPOOLSIZE 64
#define DEFPOOLSIZE 256
#define MAXPOOLSIZE 65536

/* This algorithm (and code) presumes that KEYSIZE is twice as large as BLOCKSIZE */
CTASSERT(BLOCKSIZE == sizeof(uint128_t));
CTASSERT(KEYSIZE == 2*BLOCKSIZE);

/* This is the beastie that needs protecting. It contains all of the
 * state that we are excited about.
 * Exactly one is instantiated.
 */
static struct fortuna_state {
	/* P_i */
	struct pool {
		u_int length;
		struct randomdev_hash hash;
	} pool[NPOOLS];

	/* ReseedCnt */
	u_int reseedcount;

	/* C - 128 bits */
	union {
		uint8_t byte[BLOCKSIZE];
		uint128_t whole;
	} counter;

	/* K */
	struct randomdev_key key;

	/* Extras */
	u_int minpoolsize;

	/* Extras for the OS */

#ifdef _KERNEL
	/* For use when 'pacing' the reseeds */
	sbintime_t lasttime;
#endif
} fortuna_state;

/* The random_reseed_mtx mutex protects seeding and polling/blocking.  */
static mtx_t random_reseed_mtx;

static struct fortuna_start_cache {
	uint8_t junk[PAGE_SIZE];
	size_t length;
	struct randomdev_hash hash;
} fortuna_start_cache;

#ifdef _KERNEL
static struct sysctl_ctx_list random_clist;
RANDOM_CHECK_UINT(minpoolsize, MINPOOLSIZE, MAXPOOLSIZE);
#endif

void
random_fortuna_init_alg(void)
{
	int i;
#ifdef _KERNEL
	struct sysctl_oid *random_fortuna_o;
#endif

	memset(fortuna_start_cache.junk, 0, sizeof(fortuna_start_cache.junk));
	fortuna_start_cache.length = 0U;
	randomdev_hash_init(&fortuna_start_cache.hash);

	/* Set up a lock for the reseed process */
#ifdef _KERNEL
	mtx_init(&random_reseed_mtx, "reseed mutex", NULL, MTX_DEF);
#else /* !_KERNEL */
	mtx_init(&random_reseed_mtx, mtx_plain);
#endif /* _KERNEL */

#ifdef _KERNEL
	/* Fortuna parameters. Do not adjust these unless you have
	 * have a very good clue about what they do!
	 */
	random_fortuna_o = SYSCTL_ADD_NODE(&random_clist,
		SYSCTL_STATIC_CHILDREN(_kern_random),
		OID_AUTO, "fortuna", CTLFLAG_RW, 0,
		"Fortuna Parameters");

	SYSCTL_ADD_PROC(&random_clist,
		SYSCTL_CHILDREN(random_fortuna_o), OID_AUTO,
		"minpoolsize", CTLTYPE_UINT|CTLFLAG_RW,
		&fortuna_state.minpoolsize, DEFPOOLSIZE,
		random_check_uint_minpoolsize, "IU",
		"Minimum pool size necessary to cause a reseed automatically");

	fortuna_state.lasttime = 0U;
#endif

	fortuna_state.minpoolsize = DEFPOOLSIZE;

	/* F&S - InitializePRNG() */

	/* F&S - P_i = \epsilon */
	for (i = 0; i < NPOOLS; i++) {
		randomdev_hash_init(&fortuna_state.pool[i].hash);
		fortuna_state.pool[i].length = 0U;
	}

	/* F&S - ReseedCNT = 0 */
	fortuna_state.reseedcount = 0U;

	/* F&S - InitializeGenerator() */

	/* F&S - C = 0 */
	uint128_clear(&fortuna_state.counter.whole);

	/* F&S - K = 0 */
	memset(&fortuna_state.key, 0, sizeof(fortuna_state.key));
}

void
random_fortuna_deinit_alg(void)
{

	mtx_destroy(&random_reseed_mtx);
	memset(&fortuna_state, 0, sizeof(fortuna_state));
}

/* F&S - AddRandomEvent() */
/* Process a single stochastic event off the harvest queue */
void
random_fortuna_process_event(struct harvest_event *event)
{
	u_int pl;

	/* We must be locked for all this as plenty of state gets messed with */
	mtx_lock(&random_reseed_mtx);

	/* Accumulate the event into the appropriate pool
	 * where each event carries the destination information
	 */
	/* F&S - P_i = P_i|<harvested stuff> */
	/* The hash_init and hash_finish are done in random_fortuna_read() below */
	pl = event->he_destination % NPOOLS;
	randomdev_hash_iterate(&fortuna_state.pool[pl].hash, event, sizeof(*event));
	/* No point in counting above the outside maximum */
	fortuna_state.pool[pl].length += event->he_size;
	fortuna_state.pool[pl].length = MIN(fortuna_state.pool[pl].length, MAXPOOLSIZE);

	/* Done with state-messing */
	mtx_unlock(&random_reseed_mtx);
}

/* F&S - Reseed() */
/* Reseed Mutex is held */
static void
reseed(uint8_t *junk, u_int length)
{
	struct randomdev_hash context;
	uint8_t hash[KEYSIZE];

	KASSERT(fortuna_state.minpoolsize > 0, ("random: Fortuna threshold = 0"));
#ifdef _KERNEL
	mtx_assert(&random_reseed_mtx, MA_OWNED);
#endif

	/* FS&K - K = Hd(K|s) where Hd(m) is H(H(0^512|m)) */
	randomdev_hash_init(&context);
	randomdev_hash_iterate(&context, zero_region, 512/8);
	randomdev_hash_iterate(&context, &fortuna_state.key, sizeof(fortuna_state.key));
	randomdev_hash_iterate(&context, junk, length);
	randomdev_hash_finish(&context, hash);
	randomdev_hash_init(&context);
	randomdev_hash_iterate(&context, hash, KEYSIZE);
	randomdev_hash_finish(&context, hash);
	randomdev_encrypt_init(&fortuna_state.key, hash);
	memset(hash, 0, sizeof(hash));

	/* Unblock the device if it was blocked due to being unseeded */
	if (uint128_is_zero(fortuna_state.counter.whole))
		random_adaptor_unblock();
	/* FS&K - C = C + 1 */
	uint128_increment(&fortuna_state.counter.whole);
}

/* F&S - GenerateBlocks() */
/* Reseed Mutex is held, and buf points to a whole number of blocks. */
static __inline void
random_fortuna_genblocks(uint8_t *buf, u_int blockcount)
{
	u_int i;

	for (i = 0u; i < blockcount; i++) {
		/* F&S - r = r|E(K,C) */
		randomdev_encrypt(&fortuna_state.key, fortuna_state.counter.byte, buf, BLOCKSIZE);
		buf += BLOCKSIZE;

		/* F&S - C = C + 1 */
		uint128_increment(&fortuna_state.counter.whole);
	}
}

/* F&S - PseudoRandomData() */
/* Reseed Mutex is held, and buf points to a whole number of blocks. */
static __inline void
random_fortuna_genrandom(uint8_t *buf, u_int bytecount)
{
	static uint8_t temp[BLOCKSIZE*(KEYSIZE/BLOCKSIZE)];
	u_int blockcount;

	/* F&S - assert(n < 2^20) */
	KASSERT((bytecount <= (1 << 20)), ("invalid single read request to fortuna of %d bytes", bytecount));

	/* F&S - r = first-n-bytes(GenerateBlocks(ceil(n/16))) */
	blockcount = bytecount / BLOCKSIZE;
	random_fortuna_genblocks(buf, blockcount);
	/* TODO: FIX! remove memcpy()! */
	if (bytecount % BLOCKSIZE > 0) {
		random_fortuna_genblocks(temp, 1);
		memcpy(buf + (blockcount * BLOCKSIZE), temp, bytecount % BLOCKSIZE);
	}

	/* F&S - K = GenerateBlocks(2) */
	random_fortuna_genblocks(temp, KEYSIZE/BLOCKSIZE);
	randomdev_encrypt_init(&fortuna_state.key, temp);
	memset(temp, 0, sizeof(temp));
}

/* F&S - RandomData() */
/* Used to return processed entropy from the PRNG */
/* The argument buf points to a whole number of blocks. */
void
random_fortuna_read(uint8_t *buf, u_int bytecount)
{
#ifdef _KERNEL
	sbintime_t thistime;
#endif
	struct randomdev_hash context;
	uint8_t s[NPOOLS*KEYSIZE], temp[KEYSIZE];
	int i;
	u_int seedlength;

	/* We must be locked for all this as plenty of state gets messed with */
	mtx_lock(&random_reseed_mtx);

	/* if buf == NULL and bytecount == 0 then this is the pre-read. */
	/* if buf == NULL and bytecount != 0 then this is the post-read; ignore. */
	if (buf == NULL) {
		if (bytecount == 0) {
			if (fortuna_state.pool[0].length >= fortuna_state.minpoolsize
#ifdef _KERNEL
			/* F&S - Use 'getsbinuptime()' to prevent reseed-spamming. */
		    	&& ((thistime = getsbinuptime()) - fortuna_state.lasttime > hz/10)
#endif
		    	) {
#ifdef _KERNEL
				fortuna_state.lasttime = thistime;
#endif

				seedlength = 0U;
				/* F&S - ReseedCNT = ReseedCNT + 1 */
				fortuna_state.reseedcount++;
				/* s = \epsilon by default */
				for (i = 0; i < NPOOLS; i++) {
					/* F&S - if Divides(ReseedCnt, 2^i) ... */
					if ((fortuna_state.reseedcount % (1 << i)) == 0U) {
						seedlength += KEYSIZE;
						/* F&S - temp = (P_i) */
						randomdev_hash_finish(&fortuna_state.pool[i].hash, temp);
						/* F&S - P_i = \epsilon */
						randomdev_hash_init(&fortuna_state.pool[i].hash);
						fortuna_state.pool[i].length = 0U;
						/* F&S - s = s|H(temp) */
						randomdev_hash_init(&context);
						randomdev_hash_iterate(&context, temp, KEYSIZE);
						randomdev_hash_finish(&context, s + i*KEYSIZE);
					}
					else
						break;
				}
#ifdef RANDOM_DEBUG
				printf("random: active reseed: reseedcount [%d] ", fortuna_state.reseedcount);
				for (i = 0; i < NPOOLS; i++)
					printf(" %d", fortuna_state.pool[i].length);
				printf("\n");
#endif
				/* F&S */
				reseed(s, seedlength);

				/* Clean up */
				memset(s, 0, seedlength);
				seedlength = 0U;
				memset(temp, 0, sizeof(temp));
				memset(&context, 0, sizeof(context));
			}
		}
	}
	/* if buf != NULL do a regular read. */
	else
		random_fortuna_genrandom(buf, bytecount);

	mtx_unlock(&random_reseed_mtx);
}

/* Internal function to hand external entropy to the PRNG */
void
random_fortuna_write(uint8_t *buf, u_int count)
{
	uint8_t temp[KEYSIZE];
	int i;
	uintmax_t timestamp;

	timestamp = get_cyclecount();
	randomdev_hash_iterate(&fortuna_start_cache.hash, &timestamp, sizeof(timestamp));
	randomdev_hash_iterate(&fortuna_start_cache.hash, buf, count);
	timestamp = get_cyclecount();
	randomdev_hash_iterate(&fortuna_start_cache.hash, &timestamp, sizeof(timestamp));
	randomdev_hash_finish(&fortuna_start_cache.hash, temp);
	for (i = 0; i < KEYSIZE; i++)
		fortuna_start_cache.junk[(fortuna_start_cache.length + i)%PAGE_SIZE] ^= temp[i];
	fortuna_start_cache.length += KEYSIZE;

#ifdef RANDOM_DEBUG
	printf("random: %s - ", __func__);
	for (i = 0; i < KEYSIZE; i++)
		printf("%02X", temp[i]);
	printf("\n");
#endif

	memset(temp, 0, KEYSIZE);

	/* We must be locked for all this as plenty of state gets messed with */
	mtx_lock(&random_reseed_mtx);

	randomdev_hash_init(&fortuna_start_cache.hash);

	reseed(fortuna_start_cache.junk, MIN(PAGE_SIZE, fortuna_start_cache.length));
	memset(fortuna_start_cache.junk, 0, sizeof(fortuna_start_cache.junk));

	mtx_unlock(&random_reseed_mtx);
}

void
random_fortuna_reseed(void)
{

	/* CWOT */
}

int
random_fortuna_seeded(void)
{

	return (!uint128_is_zero(fortuna_state.counter.whole));
}

#endif /* RANDOM_FORTUNA */
