/*-
 * Copyright (c) 2017 The FreeBSD Foundation
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/msan.h>
#include <sys/mutex.h>
#include <sys/random.h>
#include <sys/smp.h>
#include <sys/time.h>

#include <machine/cpu.h>

#include <crypto/chacha20/chacha.h>
#include <crypto/sha2/sha256.h>
#include <dev/random/randomdev.h>
#ifdef RANDOM_FENESTRASX
#include <dev/random/fenestrasX/fx_pub.h>
#endif

#define	CHACHA20_RESEED_BYTES	65536
#define	CHACHA20_RESEED_SECONDS	300
#define	CHACHA20_KEYBYTES	32
#define	CHACHA20_BUFFER_SIZE	64

CTASSERT(CHACHA20_KEYBYTES*8 >= CHACHA_MINKEYLEN);

#ifndef RANDOM_FENESTRASX
int arc4rand_iniseed_state = ARC4_ENTR_NONE;
#endif

MALLOC_DEFINE(M_CHACHA20RANDOM, "chacha20random", "chacha20random structures");

struct chacha20_s {
	struct mtx mtx;
	int numbytes;
	time_t t_reseed;
	uint8_t m_buffer[CHACHA20_BUFFER_SIZE];
	struct chacha_ctx ctx;
#ifdef RANDOM_FENESTRASX
	uint64_t seed_version;
#endif
} __aligned(CACHE_LINE_SIZE);

static struct chacha20_s *chacha20inst = NULL;

#define CHACHA20_FOREACH(_chacha20) \
	for (_chacha20 = &chacha20inst[0]; \
	     _chacha20 <= &chacha20inst[mp_maxid]; \
	     _chacha20++)

/*
 * Mix up the current context.
 */
static void
chacha20_randomstir(struct chacha20_s *chacha20)
{
	struct timeval tv_now;
	uint8_t key[CHACHA20_KEYBYTES];
#ifdef RANDOM_FENESTRASX
	uint64_t seed_version;

#else
	if (__predict_false(random_bypass_before_seeding && !is_random_seeded())) {
		SHA256_CTX ctx;
		uint64_t cc;
		uint32_t fver;

		if (!arc4random_bypassed_before_seeding) {
			arc4random_bypassed_before_seeding = true;
			if (!random_bypass_disable_warnings)
				printf("arc4random: WARNING: initial seeding "
				    "bypassed the cryptographic random device "
				    "because it was not yet seeded and the "
				    "knob 'bypass_before_seeding' was "
				    "enabled.\n");
		}

		/*
		 * "key" is intentionally left uninitialized here, so with KMSAN
		 * enabled the arc4random() return value may be marked
		 * uninitialized, leading to spurious reports.  Lie to KMSAN to
		 * avoid this situation.
		 */
		kmsan_mark(key, sizeof(key), KMSAN_STATE_INITED);

		/* Last ditch effort to inject something in a bad condition. */
		cc = get_cyclecount();
		SHA256_Init(&ctx);
		SHA256_Update(&ctx, key, sizeof(key));
		SHA256_Update(&ctx, &cc, sizeof(cc));
		fver = __FreeBSD_version;
		SHA256_Update(&ctx, &fver, sizeof(fver));
		_Static_assert(sizeof(key) == SHA256_DIGEST_LENGTH,
		    "make sure 256 bits is still 256 bits");
		SHA256_Final(key, &ctx);
	} else {
#endif
#ifdef RANDOM_FENESTRASX
		read_random_key(key, CHACHA20_KEYBYTES, &seed_version);
#else
		/*
		* If the loader(8) did not have an entropy stash from the
		* previous shutdown to load, then we will block.  The answer is
		* to make sure there is an entropy stash at shutdown time.
		*
		* On the other hand, if the random_bypass_before_seeding knob
		* was set and we landed in this branch, we know this won't
		* block because we know the random device is seeded.
		*/
		read_random(key, CHACHA20_KEYBYTES);
	}
#endif
	getmicrouptime(&tv_now);
	mtx_lock(&chacha20->mtx);
	chacha_keysetup(&chacha20->ctx, key, CHACHA20_KEYBYTES*8);
	chacha_ivsetup(&chacha20->ctx, (u_char *)&tv_now.tv_sec, (u_char *)&tv_now.tv_usec);
	/* Reset for next reseed cycle. */
	chacha20->t_reseed = tv_now.tv_sec + CHACHA20_RESEED_SECONDS;
	chacha20->numbytes = 0;
#ifdef RANDOM_FENESTRASX
	chacha20->seed_version = seed_version;
#endif
	mtx_unlock(&chacha20->mtx);
}

/*
 * Initialize the contexts.
 */
static void
chacha20_init(void)
{
	struct chacha20_s *chacha20;

	chacha20inst = malloc((mp_maxid + 1) * sizeof(struct chacha20_s),
			M_CHACHA20RANDOM, M_NOWAIT | M_ZERO);
	KASSERT(chacha20inst != NULL, ("chacha20_init: memory allocation error"));

	CHACHA20_FOREACH(chacha20) {
		mtx_init(&chacha20->mtx, "chacha20_mtx", NULL, MTX_DEF);
		chacha20->t_reseed = -1;
		chacha20->numbytes = 0;
		explicit_bzero(chacha20->m_buffer, CHACHA20_BUFFER_SIZE);
		explicit_bzero(&chacha20->ctx, sizeof(chacha20->ctx));
	}
}
SYSINIT(chacha20, SI_SUB_LOCK, SI_ORDER_ANY, chacha20_init, NULL);


static void
chacha20_uninit(void)
{
	struct chacha20_s *chacha20;

	CHACHA20_FOREACH(chacha20)
		mtx_destroy(&chacha20->mtx);
	free(chacha20inst, M_CHACHA20RANDOM);
}
SYSUNINIT(chacha20, SI_SUB_LOCK, SI_ORDER_ANY, chacha20_uninit, NULL);


/*
 * MPSAFE
 */
void
arc4rand(void *ptr, u_int len, int reseed)
{
	struct chacha20_s *chacha20;
	struct timeval tv;
	u_int length;
	uint8_t *p;

#ifdef RANDOM_FENESTRASX
	if (__predict_false(reseed))
#else
	if (__predict_false(reseed ||
	    (arc4rand_iniseed_state == ARC4_ENTR_HAVE &&
	    atomic_cmpset_int(&arc4rand_iniseed_state, ARC4_ENTR_HAVE, ARC4_ENTR_SEED))))
#endif
		CHACHA20_FOREACH(chacha20)
			chacha20_randomstir(chacha20);

	getmicrouptime(&tv);
	chacha20 = &chacha20inst[curcpu];
	/* We may get unlucky and be migrated off this CPU, but that is expected to be infrequent */
	if ((chacha20->numbytes > CHACHA20_RESEED_BYTES) || (tv.tv_sec > chacha20->t_reseed))
		chacha20_randomstir(chacha20);

	mtx_lock(&chacha20->mtx);
#ifdef RANDOM_FENESTRASX
	if (__predict_false(
	    atomic_load_acq_64(&fxrng_root_generation) != chacha20->seed_version
	    )) {
		mtx_unlock(&chacha20->mtx);
		chacha20_randomstir(chacha20);
		mtx_lock(&chacha20->mtx);
	}
#endif

	p = ptr;
	while (len) {
		length = MIN(CHACHA20_BUFFER_SIZE, len);
		chacha_encrypt_bytes(&chacha20->ctx, chacha20->m_buffer, p, length);
		p += length;
		len -= length;
		chacha20->numbytes += length;
		if (chacha20->numbytes > CHACHA20_RESEED_BYTES) {
			mtx_unlock(&chacha20->mtx);
			chacha20_randomstir(chacha20);
			mtx_lock(&chacha20->mtx);
		}
	}
	mtx_unlock(&chacha20->mtx);
}

uint32_t
arc4random(void)
{
	uint32_t ret;

	arc4rand(&ret, sizeof(ret), 0);
	return ret;
}

void
arc4random_buf(void *ptr, size_t len)
{

	arc4rand(ptr, len, 0);
}
