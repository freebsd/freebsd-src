/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Conrad Meyer <cem@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/fail.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/random.h>
#include <sys/sdt.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <machine/cpu.h>
#include <machine/stdarg.h>

#define CHACHA_EMBED
#define KEYSTREAM_ONLY
#define CHACHA_NONCE0_CTR128
#include <crypto/chacha20/chacha.h>
#include <crypto/rijndael/rijndael-api-fst.h>
#include <crypto/sha2/sha256.h>

#include <dev/random/hash.h>
#include <dev/random/randomdev.h>
#include <dev/random/random_harvestq.h>
#include <dev/random/uint128.h>

#include <dev/random/fenestrasX/fx_hash.h>
#include <dev/random/fenestrasX/fx_priv.h>
#include <dev/random/fenestrasX/fx_rng.h>

_Static_assert(FX_CHACHA20_KEYSIZE == RANDOM_KEYSIZE, "");

#include <crypto/chacha20/chacha.c>

static void
fxrng_rng_keystream_internal(struct chacha_ctx *prf, void *buf, size_t nbytes)
{
	size_t chunklen;

	while (nbytes > 0) {
		chunklen = MIN(nbytes,
		    rounddown((size_t)UINT32_MAX, CHACHA_BLOCKLEN));

		chacha_encrypt_bytes(prf, NULL, buf, chunklen);
		buf = (uint8_t *)buf + chunklen;
		nbytes -= chunklen;
	}
}

/*
 * This subroutine pulls the counter out of Chacha, which for whatever reason
 * always encodes and decodes counters in a little endian format, and adds
 * 'addend' to it, saving the result in Chacha.
 */
static void
fxrng_chacha_nonce_add64(struct chacha_ctx *ctx, uint64_t addend)
{
	uint128_t ctr;	/* Native-endian. */
#if BYTE_ORDER == BIG_ENDIAN
	uint128_t lectr;

	chacha_ctrsave(ctx, (void *)&lectr);
	ctr = le128dec(&lectr);
#else
	chacha_ctrsave(ctx, (void *)&ctr);
#endif

	uint128_add64(&ctr, addend);

	/* chacha_ivsetup() does not modify the key, and we rely on that. */
#if BYTE_ORDER == BIG_ENDIAN
	le128enc(&lectr, ctr);
	chacha_ivsetup(ctx, NULL, (const void *)&lectr);
	explicit_bzero(&lectr, sizeof(lectr));
#else
	chacha_ivsetup(ctx, NULL, (const void *)&ctr);
#endif
	explicit_bzero(&ctr, sizeof(ctr));
}

/*
 * Generate from the unbuffered source PRNG.
 *
 * Handles fast key erasure (rekeys the PRF with a generated key under lock).
 *
 * RNG lock is required on entry.  If return_unlocked is true, RNG lock will
 * be dropped on return.
 */
void
fxrng_rng_genrandom_internal(struct fxrng_basic_rng *rng, void *buf,
    size_t nbytes, bool return_unlocked)
{
	struct chacha_ctx ctx_copy, *p_ctx;
	uint8_t newkey[FX_CHACHA20_KEYSIZE];
	size_t blockcount;

	FXRNG_RNG_ASSERT(rng);

	/* Save off the initial output of the generator for rekeying. */
	fxrng_rng_keystream_internal(&rng->rng_prf, newkey, sizeof(newkey));

	if (return_unlocked) {
		memcpy(&ctx_copy, &rng->rng_prf, sizeof(ctx_copy));
		p_ctx = &ctx_copy;

		/*
		 * Forward the Chacha counter state over the blocks we promise
		 * to generate for the caller without the lock.
		 */
		blockcount = howmany(nbytes, CHACHA_BLOCKLEN);
		fxrng_chacha_nonce_add64(&rng->rng_prf, blockcount);

		/* Re-key before dropping the lock. */
		chacha_keysetup(&rng->rng_prf, newkey, sizeof(newkey) * 8);
		explicit_bzero(newkey, sizeof(newkey));

		FXRNG_RNG_UNLOCK(rng);
	} else {
		p_ctx = &rng->rng_prf;
	}

	fxrng_rng_keystream_internal(p_ctx, buf, nbytes);

	if (return_unlocked) {
		explicit_bzero(&ctx_copy, sizeof(ctx_copy));
		FXRNG_RNG_ASSERT_NOT(rng);
	} else {
		/* Re-key before exit. */
		chacha_keysetup(&rng->rng_prf, newkey, sizeof(newkey) * 8);
		explicit_bzero(newkey, sizeof(newkey));
		FXRNG_RNG_ASSERT(rng);
	}
}

/*
 * Helper to reseed the root RNG, incorporating the existing RNG state.
 *
 * The root RNG is locked on entry and locked on return.
 */
static void
fxrng_rng_reseed_internal(struct fxrng_basic_rng *rng, bool seeded,
    const void *src, size_t sz, ...)
{
	union {
		uint8_t root_state[FX_CHACHA20_KEYSIZE];
		uint8_t hash_out[FXRNG_HASH_SZ];
	} u;
	struct fxrng_hash mix;
	va_list ap;

	_Static_assert(FX_CHACHA20_KEYSIZE <= FXRNG_HASH_SZ, "");

	FXRNG_RNG_ASSERT(rng);

	fxrng_hash_init(&mix);
	if (seeded) {
		fxrng_rng_keystream_internal(&rng->rng_prf, u.root_state,
		    sizeof(u.root_state));
		fxrng_hash_update(&mix, u.root_state, sizeof(u.root_state));
	}
	fxrng_hash_update(&mix, src, sz);

	va_start(ap, sz);
	while (true) {
		src = va_arg(ap, const void *);
		if (src == NULL)
			break;
		sz = va_arg(ap, size_t);
		fxrng_hash_update(&mix, src, sz);
	}
	va_end(ap);

	fxrng_hash_finish(&mix, u.hash_out, sizeof(u.hash_out));

	/*
	 * Take the first keysize (32) bytes of our digest (64 bytes).  It is
	 * also possible to just have Blake2 emit fewer bytes, but our wrapper
	 * API doesn't provide that functionality and there isn't anything
	 * obviously wrong with emitting more hash bytes.
	 *
	 * keysetup does not reset the embedded counter, and we rely on that
	 * property.
	 */
	chacha_keysetup(&rng->rng_prf, u.hash_out, FX_CHACHA20_KEYSIZE * 8);

	/* 'mix' zeroed by fxrng_hash_finish(). */
	explicit_bzero(u.hash_out, sizeof(u.hash_out));

	FXRNG_RNG_ASSERT(rng);
}

/*
 * Directly reseed the root RNG from a first-time entropy source,
 * incorporating the existing RNG state, called by fxrng_brng_src_reseed.
 *
 * The root RNG is locked on entry and locked on return.
 */
void
fxrng_rng_src_reseed(struct fxrng_basic_rng *rng,
    const struct harvest_event *event)
{
	fxrng_rng_reseed_internal(rng, true, &event->he_somecounter,
	    sizeof(event->he_somecounter), (const void *)event->he_entropy,
	    (size_t)event->he_size, NULL);
}

/*
 * Reseed the root RNG from pooled entropy, incorporating the existing RNG
 * state, called by fxrng_brng_reseed.
 *
 * The root RNG is locked on entry and locked on return.
 */
void
fxrng_rng_reseed(struct fxrng_basic_rng *rng, bool seeded, const void *entr,
    size_t sz)
{
	fxrng_rng_reseed_internal(rng, seeded, entr, sz, NULL);
}
