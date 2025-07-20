/*-
 * Copyright (c) 2017 W. Dean Freeman
 * Copyright (c) 2013-2015 Mark R V Murray
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

/*
 * This implementation of Fortuna is based on the descriptions found in
 * ISBN 978-0-470-47424-2 "Cryptography Engineering" by Ferguson, Schneier
 * and Kohno ("FS&K").
 */

#include <sys/param.h>
#include <sys/limits.h>

#ifdef _KERNEL
#include <sys/fail.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/random.h>
#include <sys/sdt.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <machine/cpu.h>
#else /* !_KERNEL */
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "unit_test.h"
#endif /* _KERNEL */

#include <crypto/chacha20/chacha.h>
#include <crypto/rijndael/rijndael-api-fst.h>
#include <crypto/sha2/sha256.h>

#include <dev/random/hash.h>
#include <dev/random/randomdev.h>
#ifdef _KERNEL
#include <dev/random/random_harvestq.h>
#endif
#include <dev/random/uint128.h>
#include <dev/random/fortuna.h>

/* Defined in FS&K */
#define	RANDOM_FORTUNA_MAX_READ (1 << 20)	/* Max bytes from AES before rekeying */
#define	RANDOM_FORTUNA_BLOCKS_PER_KEY (1 << 16)	/* Max blocks from AES before rekeying */
CTASSERT(RANDOM_FORTUNA_BLOCKS_PER_KEY * RANDOM_BLOCKSIZE ==
    RANDOM_FORTUNA_MAX_READ);

/*
 * The allowable range of RANDOM_FORTUNA_DEFPOOLSIZE. The default value is above.
 * Making RANDOM_FORTUNA_DEFPOOLSIZE too large will mean a long time between reseeds,
 * and too small may compromise initial security but get faster reseeds.
 */
#define	RANDOM_FORTUNA_MINPOOLSIZE 16
#define	RANDOM_FORTUNA_MAXPOOLSIZE INT_MAX 
CTASSERT(RANDOM_FORTUNA_MINPOOLSIZE <= RANDOM_FORTUNA_DEFPOOLSIZE);
CTASSERT(RANDOM_FORTUNA_DEFPOOLSIZE <= RANDOM_FORTUNA_MAXPOOLSIZE);

/* This algorithm (and code) presumes that RANDOM_KEYSIZE is twice as large as RANDOM_BLOCKSIZE */
CTASSERT(RANDOM_BLOCKSIZE == sizeof(uint128_t));
CTASSERT(RANDOM_KEYSIZE == 2*RANDOM_BLOCKSIZE);

/* Probes for dtrace(1) */
#ifdef _KERNEL
SDT_PROVIDER_DECLARE(random);
SDT_PROVIDER_DEFINE(random);
SDT_PROBE_DEFINE2(random, fortuna, event_processor, debug, "u_int", "struct fs_pool *");
#endif /* _KERNEL */

/*
 * This is the beastie that needs protecting. It contains all of the
 * state that we are excited about. Exactly one is instantiated.
 */
static struct fortuna_state {
	struct fs_pool {		/* P_i */
		u_int fsp_length;	/* Only the first one is used by Fortuna */
		struct randomdev_hash fsp_hash;
	} fs_pool[RANDOM_FORTUNA_NPOOLS];
	u_int fs_reseedcount;		/* ReseedCnt */
	uint128_t fs_counter;		/* C */
	union randomdev_key fs_key;	/* K */
	u_int fs_minpoolsize;		/* Extras */
	/* Extras for the OS */
#ifdef _KERNEL
	/* For use when 'pacing' the reseeds */
	sbintime_t fs_lasttime;
#endif
	/* Reseed lock */
	mtx_t fs_mtx;
} fortuna_state;

/*
 * This knob enables or disables the "Concurrent Reads" Fortuna feature.
 *
 * The benefit of Concurrent Reads is improved concurrency in Fortuna.  That is
 * reflected in two related aspects:
 *
 * 1. Concurrent full-rate devrandom readers can achieve similar throughput to
 *    a single reader thread (at least up to a modest number of cores; the
 *    non-concurrent design falls over at 2 readers).
 *
 * 2. The rand_harvestq process spends much less time spinning when one or more
 *    readers is processing a large request.  Partially this is due to
 *    rand_harvestq / ra_event_processor design, which only passes one event at
 *    a time to the underlying algorithm.  Each time, Fortuna must take its
 *    global state mutex, potentially blocking on a reader.  Our adaptive
 *    mutexes assume that a lock holder currently on CPU will release the lock
 *    quickly, and spin if the owning thread is currently running.
 *
 *    (There is no reason rand_harvestq necessarily has to use the same lock as
 *    the generator, or that it must necessarily drop and retake locks
 *    repeatedly, but that is the current status quo.)
 *
 * The concern is that the reduced lock scope might results in a less safe
 * random(4) design.  However, the reduced-lock scope design is still
 * fundamentally Fortuna.  This is discussed below.
 *
 * Fortuna Read() only needs mutual exclusion between readers to correctly
 * update the shared read-side state: C, the 128-bit counter; and K, the
 * current cipher/PRF key.
 *
 * In the Fortuna design, the global counter C should provide an independent
 * range of values per request.
 *
 * Under lock, we can save a copy of C on the stack, and increment the global C
 * by the number of blocks a Read request will require.
 *
 * Still under lock, we can save a copy of the key K on the stack, and then
 * perform the usual key erasure K' <- Keystream(C, K, ...).  This does require
 * generating 256 bits (32 bytes) of cryptographic keystream output with the
 * global lock held, but that's all; none of the API keystream generation must
 * be performed under lock.
 *
 * At this point, we may unlock.
 *
 * Some example timelines below (to oversimplify, all requests are in units of
 * native blocks, and the keysize happens to be equal or less to the native
 * blocksize of the underlying cipher, and the same sequence of two requests
 * arrive in the same order).  The possibly expensive consumer keystream
 * generation portion is marked with '**'.
 *
 * Status Quo fortuna_read()           Reduced-scope locking
 * -------------------------           ---------------------
 * C=C_0, K=K_0                        C=C_0, K=K_0
 * <Thr 1 requests N blocks>           <Thr 1 requests N blocks>
 * 1:Lock()                            1:Lock()
 * <Thr 2 requests M blocks>           <Thr 2 requests M blocks>
 * 1:GenBytes()                        1:stack_C := C_0
 * 1:  Keystream(C_0, K_0, N)          1:stack_K := K_0
 * 1:    <N blocks generated>**        1:C' := C_0 + N
 * 1:    C' := C_0 + N                 1:K' := Keystream(C', K_0, 1)
 * 1:    <- Keystream                  1:  <1 block generated>
 * 1:  K' := Keystream(C', K_0, 1)     1:  C'' := C' + 1
 * 1:    <1 block generated>           1:  <- Keystream
 * 1:    C'' := C' + 1                 1:Unlock()
 * 1:    <- Keystream
 * 1:  <- GenBytes()
 * 1:Unlock()
 *
 * Just prior to unlock, shared state is identical:
 * ------------------------------------------------
 * C'' == C_0 + N + 1                  C'' == C_0 + N + 1
 * K' == keystream generated from      K' == keystream generated from
 *       C_0 + N, K_0.                       C_0 + N, K_0.
 * K_0 has been erased.                K_0 has been erased.
 *
 * After both designs unlock, the 2nd reader is unblocked.
 *
 * 2:Lock()                            2:Lock()
 * 2:GenBytes()                        2:stack_C' := C''
 * 2:  Keystream(C'', K', M)           2:stack_K' := K'
 * 2:    <M blocks generated>**        2:C''' := C'' + M
 * 2:    C''' := C'' + M               2:K'' := Keystream(C''', K', 1)
 * 2:    <- Keystream                  2:  <1 block generated>
 * 2:  K'' := Keystream(C''', K', 1)   2:  C'''' := C''' + 1
 * 2:    <1 block generated>           2:  <- Keystream
 * 2:    C'''' := C''' + 1             2:Unlock()
 * 2:    <- Keystream
 * 2:  <- GenBytes()
 * 2:Unlock()
 *
 * Just prior to unlock, global state is identical:
 * ------------------------------------------------------
 *
 * C'''' == (C_0 + N + 1) + M + 1      C'''' == (C_0 + N + 1) + M + 1
 * K'' == keystream generated from     K'' == keystream generated from
 *        C_0 + N + 1 + M, K'.                C_0 + N + 1 + M, K'.
 * K' has been erased.                 K' has been erased.
 *
 * Finally, in the new design, the two consumer threads can finish the
 * remainder of the generation at any time (including simultaneously):
 *
 *                                     1:  GenBytes()
 *                                     1:    Keystream(stack_C, stack_K, N)
 *                                     1:      <N blocks generated>**
 *                                     1:    <- Keystream
 *                                     1:  <- GenBytes
 *                                     1:ExplicitBzero(stack_C, stack_K)
 *
 *                                     2:  GenBytes()
 *                                     2:    Keystream(stack_C', stack_K', M)
 *                                     2:      <M blocks generated>**
 *                                     2:    <- Keystream
 *                                     2:  <- GenBytes
 *                                     2:ExplicitBzero(stack_C', stack_K')
 *
 * The generated user keystream for both threads is identical between the two
 * implementations:
 *
 * 1: Keystream(C_0, K_0, N)           1: Keystream(stack_C, stack_K, N)
 * 2: Keystream(C'', K', M)            2: Keystream(stack_C', stack_K', M)
 *
 * (stack_C == C_0; stack_K == K_0; stack_C' == C''; stack_K' == K'.)
 */
static bool fortuna_concurrent_read __read_frequently = true;

#ifdef _KERNEL
static struct sysctl_ctx_list random_clist;
RANDOM_CHECK_UINT(fs_minpoolsize, RANDOM_FORTUNA_MINPOOLSIZE, RANDOM_FORTUNA_MAXPOOLSIZE);
#else
static uint8_t zero_region[RANDOM_ZERO_BLOCKSIZE];
#endif

static void random_fortuna_pre_read(void);
static void random_fortuna_read(uint8_t *, size_t);
static bool random_fortuna_seeded(void);
static bool random_fortuna_seeded_internal(void);
static void random_fortuna_process_event(struct harvest_event *);

static void random_fortuna_reseed_internal(uint32_t *entropy_data, u_int blockcount);

#ifdef RANDOM_LOADABLE
static
#endif
const struct random_algorithm random_alg_context = {
	.ra_ident = "Fortuna",
	.ra_pre_read = random_fortuna_pre_read,
	.ra_read = random_fortuna_read,
	.ra_seeded = random_fortuna_seeded,
	.ra_event_processor = random_fortuna_process_event,
	.ra_poolcount = RANDOM_FORTUNA_NPOOLS,
};

/* ARGSUSED */
static void
random_fortuna_init_alg(void *unused __unused)
{
	int i;
#ifdef _KERNEL
	struct sysctl_oid *random_fortuna_o;
#endif

#ifdef RANDOM_LOADABLE
	p_random_alg_context = &random_alg_context;
#endif

	RANDOM_RESEED_INIT_LOCK();
	/*
	 * Fortuna parameters. Do not adjust these unless you have
	 * have a very good clue about what they do!
	 */
	fortuna_state.fs_minpoolsize = RANDOM_FORTUNA_DEFPOOLSIZE;
#ifdef _KERNEL
	fortuna_state.fs_lasttime = 0;
	random_fortuna_o = SYSCTL_ADD_NODE(&random_clist,
		SYSCTL_STATIC_CHILDREN(_kern_random),
		OID_AUTO, "fortuna", CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
		"Fortuna Parameters");
	SYSCTL_ADD_PROC(&random_clist,
	    SYSCTL_CHILDREN(random_fortuna_o), OID_AUTO, "minpoolsize",
	    CTLTYPE_UINT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    &fortuna_state.fs_minpoolsize, RANDOM_FORTUNA_DEFPOOLSIZE,
	    random_check_uint_fs_minpoolsize, "IU",
	    "Minimum pool size necessary to cause a reseed");
	KASSERT(fortuna_state.fs_minpoolsize > 0, ("random: Fortuna threshold must be > 0 at startup"));

	SYSCTL_ADD_BOOL(&random_clist, SYSCTL_CHILDREN(random_fortuna_o),
	    OID_AUTO, "concurrent_read", CTLFLAG_RDTUN,
	    &fortuna_concurrent_read, 0, "If non-zero, enable "
	    "feature to improve concurrent Fortuna performance.");
#endif

	/*-
	 * FS&K - InitializePRNG()
	 *      - P_i = \epsilon
	 *      - ReseedCNT = 0
	 */
	for (i = 0; i < RANDOM_FORTUNA_NPOOLS; i++) {
		randomdev_hash_init(&fortuna_state.fs_pool[i].fsp_hash);
		fortuna_state.fs_pool[i].fsp_length = 0;
	}
	fortuna_state.fs_reseedcount = 0;
	/*-
	 * FS&K - InitializeGenerator()
	 *      - C = 0
	 *      - K = 0
	 */
	fortuna_state.fs_counter = UINT128_ZERO;
	explicit_bzero(&fortuna_state.fs_key, sizeof(fortuna_state.fs_key));
}
SYSINIT(random_alg, SI_SUB_RANDOM, SI_ORDER_SECOND, random_fortuna_init_alg,
    NULL);

/*-
 * FS&K - AddRandomEvent()
 * Process a single stochastic event off the harvest queue
 */
static void
random_fortuna_process_event(struct harvest_event *event)
{
	u_int pl;

	RANDOM_RESEED_LOCK();
	/*
	 * Run SP 800-90B health tests on the source if so configured.
	 */
	if (!random_harvest_healthtest(event)) {
		RANDOM_RESEED_UNLOCK();
		return;
	}
	/*-
	 * FS&K - P_i = P_i|<harvested stuff>
	 * Accumulate the event into the appropriate pool
	 * where each event carries the destination information.
	 *
	 * The hash_init() and hash_finish() calls are done in
	 * random_fortuna_pre_read().
	 *
	 * We must be locked against pool state modification which can happen
	 * during accumulation/reseeding and reading/regating.
	 */
	pl = event->he_destination % RANDOM_FORTUNA_NPOOLS;
	/*
	 * If a VM generation ID changes (clone and play or VM rewind), we want
	 * to incorporate that as soon as possible.  Override destingation pool
	 * for immediate next use.
	 */
	if (event->he_source == RANDOM_PURE_VMGENID)
		pl = 0;
	/*
	 * We ignore low entropy static/counter fields towards the end of the
	 * he_event structure in order to increase measurable entropy when
	 * conducting SP800-90B entropy analysis measurements of seed material
	 * fed into PRNG.
	 * -- wdf
	 */
	KASSERT(event->he_size <= sizeof(event->he_entropy),
	    ("%s: event->he_size: %hhu > sizeof(event->he_entropy): %zu\n",
	    __func__, event->he_size, sizeof(event->he_entropy)));
	randomdev_hash_iterate(&fortuna_state.fs_pool[pl].fsp_hash,
	    &event->he_somecounter, sizeof(event->he_somecounter));
	randomdev_hash_iterate(&fortuna_state.fs_pool[pl].fsp_hash,
	    event->he_entropy, event->he_size);

	/*-
	 * Don't wrap the length.  This is a "saturating" add.
	 * XXX: FIX!!: We don't actually need lengths for anything but fs_pool[0],
	 * but it's been useful debugging to see them all.
	 */
	fortuna_state.fs_pool[pl].fsp_length = MIN(RANDOM_FORTUNA_MAXPOOLSIZE,
	    fortuna_state.fs_pool[pl].fsp_length +
	    sizeof(event->he_somecounter) + event->he_size);
	RANDOM_RESEED_UNLOCK();
}

/*-
 * FS&K - Reseed()
 * This introduces new key material into the output generator.
 * Additionally it increments the output generator's counter
 * variable C. When C > 0, the output generator is seeded and
 * will deliver output.
 * The entropy_data buffer passed is a very specific size; the
 * product of RANDOM_FORTUNA_NPOOLS and RANDOM_KEYSIZE.
 */
static void
random_fortuna_reseed_internal(uint32_t *entropy_data, u_int blockcount)
{
	struct randomdev_hash context;
	uint8_t hash[RANDOM_KEYSIZE];
	const void *keymaterial;
	size_t keysz;
	bool seeded;

	RANDOM_RESEED_ASSERT_LOCK_OWNED();

	seeded = random_fortuna_seeded_internal();
	if (seeded) {
		randomdev_getkey(&fortuna_state.fs_key, &keymaterial, &keysz);
		KASSERT(keysz == RANDOM_KEYSIZE, ("%s: key size %zu not %u",
			__func__, keysz, (unsigned)RANDOM_KEYSIZE));
	}

	/*-
	 * FS&K - K = Hd(K|s) where Hd(m) is H(H(0^512|m))
	 *      - C = C + 1
	 */
	randomdev_hash_init(&context);
	randomdev_hash_iterate(&context, zero_region, RANDOM_ZERO_BLOCKSIZE);
	if (seeded)
		randomdev_hash_iterate(&context, keymaterial, keysz);
	randomdev_hash_iterate(&context, entropy_data, RANDOM_KEYSIZE*blockcount);
	randomdev_hash_finish(&context, hash);
	randomdev_hash_init(&context);
	randomdev_hash_iterate(&context, hash, RANDOM_KEYSIZE);
	randomdev_hash_finish(&context, hash);
	randomdev_encrypt_init(&fortuna_state.fs_key, hash);
	explicit_bzero(hash, sizeof(hash));
	/* Unblock the device if this is the first time we are reseeding. */
	if (uint128_is_zero(fortuna_state.fs_counter))
		randomdev_unblock();
	uint128_increment(&fortuna_state.fs_counter);
}

/*-
 * FS&K - RandomData() (Part 1)
 * Used to return processed entropy from the PRNG. There is a pre_read
 * required to be present (but it can be a stub) in order to allow
 * specific actions at the begin of the read.
 */
void
random_fortuna_pre_read(void)
{
#ifdef _KERNEL
	sbintime_t now;
#endif
	struct randomdev_hash context;
	uint32_t s[RANDOM_FORTUNA_NPOOLS*RANDOM_KEYSIZE_WORDS];
	uint8_t temp[RANDOM_KEYSIZE];
	u_int i;

	KASSERT(fortuna_state.fs_minpoolsize > 0, ("random: Fortuna threshold must be > 0"));
	RANDOM_RESEED_LOCK();
#ifdef _KERNEL
	/* FS&K - Use 'getsbinuptime()' to prevent reseed-spamming. */
	now = getsbinuptime();
#endif

	if (fortuna_state.fs_pool[0].fsp_length < fortuna_state.fs_minpoolsize
#ifdef _KERNEL
	    /*
	     * FS&K - Use 'getsbinuptime()' to prevent reseed-spamming, but do
	     * not block initial seeding (fs_lasttime == 0).
	     */
	    || (__predict_true(fortuna_state.fs_lasttime != 0) &&
		now - fortuna_state.fs_lasttime <= SBT_1S/10)
#endif
	) {
		RANDOM_RESEED_UNLOCK();
		return;
	}

#ifdef _KERNEL
	/*
	 * When set, pretend we do not have enough entropy to reseed yet.
	 */
	KFAIL_POINT_CODE(DEBUG_FP, random_fortuna_pre_read, {
		if (RETURN_VALUE != 0) {
			RANDOM_RESEED_UNLOCK();
			return;
		}
	});
#endif

#ifdef _KERNEL
	fortuna_state.fs_lasttime = now;
#endif

	/* FS&K - ReseedCNT = ReseedCNT + 1 */
	fortuna_state.fs_reseedcount++;
	/* s = \epsilon at start */
	for (i = 0; i < RANDOM_FORTUNA_NPOOLS; i++) {
		/* FS&K - if Divides(ReseedCnt, 2^i) ... */
		if ((fortuna_state.fs_reseedcount % (1 << i)) == 0) {
			/*-
			    * FS&K - temp = (P_i)
			    *      - P_i = \epsilon
			    *      - s = s|H(temp)
			    */
			randomdev_hash_finish(&fortuna_state.fs_pool[i].fsp_hash, temp);
			randomdev_hash_init(&fortuna_state.fs_pool[i].fsp_hash);
			fortuna_state.fs_pool[i].fsp_length = 0;
			randomdev_hash_init(&context);
			randomdev_hash_iterate(&context, temp, RANDOM_KEYSIZE);
			randomdev_hash_finish(&context, s + i*RANDOM_KEYSIZE_WORDS);
		} else
			break;
	}
#ifdef _KERNEL
	SDT_PROBE2(random, fortuna, event_processor, debug, fortuna_state.fs_reseedcount, fortuna_state.fs_pool);
#endif
	/* FS&K */
	random_fortuna_reseed_internal(s, i);
	RANDOM_RESEED_UNLOCK();

	/* Clean up and secure */
	explicit_bzero(s, sizeof(s));
	explicit_bzero(temp, sizeof(temp));
}

/*
 * This is basically GenerateBlocks() from FS&K.
 *
 * It differs in two ways:
 *
 * 1. Chacha20 is tolerant of non-block-multiple request sizes, so we do not
 * need to handle any remainder bytes specially and can just pass the length
 * directly to the PRF construction; and
 *
 * 2. Chacha20 is a 512-bit block size cipher (whereas AES has 128-bit block
 * size, regardless of key size).  This means Chacha does not require re-keying
 * every 1MiB.  This is implied by the math in FS&K 9.4 and mentioned
 * explicitly in the conclusion, "If we had a block cipher with a 256-bit [or
 * greater] block size, then the collisions would not have been an issue at
 * all" (p. 144).
 *
 * 3. In conventional ("locked") mode, we produce a maximum of PAGE_SIZE output
 * at a time before dropping the lock, to not bully the lock especially.  This
 * has been the status quo since 2015 (r284959).
 *
 * The upstream caller random_fortuna_read is responsible for zeroing out
 * sensitive buffers provided as parameters to this routine.
 */
enum {
	FORTUNA_UNLOCKED = false,
	FORTUNA_LOCKED = true
};
static void
random_fortuna_genbytes(uint8_t *buf, size_t bytecount,
    uint8_t newkey[static RANDOM_KEYSIZE], uint128_t *p_counter,
    union randomdev_key *p_key, bool locked)
{
	uint8_t remainder_buf[RANDOM_BLOCKSIZE];
	size_t chunk_size;

	if (locked)
		RANDOM_RESEED_ASSERT_LOCK_OWNED();
	else
		RANDOM_RESEED_ASSERT_LOCK_NOT_OWNED();

	/*
	 * Easy case: don't have to worry about bullying the global mutex,
	 * don't have to worry about rekeying Chacha; API is byte-oriented.
	 */
	if (!locked && random_chachamode) {
		randomdev_keystream(p_key, p_counter, buf, bytecount);
		return;
	}

	if (locked) {
		/*
		 * While holding the global lock, limit PRF generation to
		 * mitigate, but not eliminate, bullying symptoms.
		 */
		chunk_size = PAGE_SIZE;
	} else {
		/*
		* 128-bit block ciphers like AES must be re-keyed at 1MB
		* intervals to avoid unacceptable statistical differentiation
		* from true random data (FS&K 9.4, p. 143-144).
		*/
		MPASS(!random_chachamode);
		chunk_size = RANDOM_FORTUNA_MAX_READ;
	}

	chunk_size = MIN(bytecount, chunk_size);
	if (!random_chachamode)
		chunk_size = rounddown(chunk_size, RANDOM_BLOCKSIZE);

	while (bytecount >= chunk_size && chunk_size > 0) {
		randomdev_keystream(p_key, p_counter, buf, chunk_size);

		buf += chunk_size;
		bytecount -= chunk_size;

		/* We have to rekey if there is any data remaining to be
		 * generated, in two scenarios:
		 *
		 * locked: we need to rekey before we unlock and release the
		 * global state to another consumer; or
		 *
		 * unlocked: we need to rekey because we're in AES mode and are
		 * required to rekey at chunk_size==1MB.  But we do not need to
		 * rekey during the last trailing <1MB chunk.
		 */
		if (bytecount > 0) {
			if (locked || chunk_size == RANDOM_FORTUNA_MAX_READ) {
				randomdev_keystream(p_key, p_counter, newkey,
				    RANDOM_KEYSIZE);
				randomdev_encrypt_init(p_key, newkey);
			}

			/*
			 * If we're holding the global lock, yield it briefly
			 * now.
			 */
			if (locked) {
				RANDOM_RESEED_UNLOCK();
				RANDOM_RESEED_LOCK();
			}

			/*
			 * At the trailing end, scale down chunk_size from 1MB or
			 * PAGE_SIZE to all remaining full blocks (AES) or all
			 * remaining bytes (Chacha).
			 */
			if (bytecount < chunk_size) {
				if (random_chachamode)
					chunk_size = bytecount;
				else if (bytecount >= RANDOM_BLOCKSIZE)
					chunk_size = rounddown(bytecount,
					    RANDOM_BLOCKSIZE);
				else
					break;
			}
		}
	}

	/*
	 * Generate any partial AES block remaining into a temporary buffer and
	 * copy the desired substring out.
	 */
	if (bytecount > 0) {
		MPASS(!random_chachamode);

		randomdev_keystream(p_key, p_counter, remainder_buf,
		    sizeof(remainder_buf));
	}

	/*
	 * In locked mode, re-key global K before dropping the lock, which we
	 * don't need for memcpy/bzero below.
	 */
	if (locked) {
		randomdev_keystream(p_key, p_counter, newkey, RANDOM_KEYSIZE);
		randomdev_encrypt_init(p_key, newkey);
		RANDOM_RESEED_UNLOCK();
	}

	if (bytecount > 0) {
		memcpy(buf, remainder_buf, bytecount);
		explicit_bzero(remainder_buf, sizeof(remainder_buf));
	}
}


/*
 * Handle only "concurrency-enabled" Fortuna reads to simplify logic.
 *
 * Caller (random_fortuna_read) is responsible for zeroing out sensitive
 * buffers provided as parameters to this routine.
 */
static void
random_fortuna_read_concurrent(uint8_t *buf, size_t bytecount,
    uint8_t newkey[static RANDOM_KEYSIZE])
{
	union randomdev_key key_copy;
	uint128_t counter_copy;
	size_t blockcount;

	MPASS(fortuna_concurrent_read);

	/*
	 * Compute number of blocks required for the PRF request ('delta C').
	 * We will step the global counter 'C' by this number under lock, and
	 * then actually consume the counter values outside the lock.
	 *
	 * This ensures that contemporaneous but independent requests for
	 * randomness receive distinct 'C' values and thus independent PRF
	 * results.
	 */
	if (random_chachamode) {
		blockcount = howmany(bytecount, CHACHA_BLOCKLEN);
	} else {
		blockcount = howmany(bytecount, RANDOM_BLOCKSIZE);

		/*
		 * Need to account for the additional blocks generated by
		 * rekeying when updating the global fs_counter.
		 */
		blockcount += RANDOM_KEYS_PER_BLOCK *
		    (blockcount / RANDOM_FORTUNA_BLOCKS_PER_KEY);
	}

	RANDOM_RESEED_LOCK();
	KASSERT(!uint128_is_zero(fortuna_state.fs_counter), ("FS&K: C != 0"));

	/*
	 * Save the original counter and key values that will be used as the
	 * PRF for this particular consumer.
	 */
	memcpy(&counter_copy, &fortuna_state.fs_counter, sizeof(counter_copy));
	memcpy(&key_copy, &fortuna_state.fs_key, sizeof(key_copy));

	/*
	 * Step the counter as if we had generated 'bytecount' blocks for this
	 * consumer.  I.e., ensure that the next consumer gets an independent
	 * range of counter values once we drop the global lock.
	 */
	uint128_add64(&fortuna_state.fs_counter, blockcount);

	/*
	 * We still need to Rekey the global 'K' between independent calls;
	 * this is no different from conventional Fortuna.  Note that
	 * 'randomdev_keystream()' will step the fs_counter 'C' appropriately
	 * for the blocks needed for the 'newkey'.
	 *
	 * (This is part of PseudoRandomData() in FS&K, 9.4.4.)
	 */
	randomdev_keystream(&fortuna_state.fs_key, &fortuna_state.fs_counter,
	    newkey, RANDOM_KEYSIZE);
	randomdev_encrypt_init(&fortuna_state.fs_key, newkey);

	/*
	 * We have everything we need to generate a unique PRF for this
	 * consumer without touching global state.
	 */
	RANDOM_RESEED_UNLOCK();

	random_fortuna_genbytes(buf, bytecount, newkey, &counter_copy,
	    &key_copy, FORTUNA_UNLOCKED);
	RANDOM_RESEED_ASSERT_LOCK_NOT_OWNED();

	explicit_bzero(&counter_copy, sizeof(counter_copy));
	explicit_bzero(&key_copy, sizeof(key_copy));
}

/*-
 * FS&K - RandomData() (Part 2)
 * Main read from Fortuna, continued. May be called multiple times after
 * the random_fortuna_pre_read() above.
 *
 * The supplied buf MAY not be a multiple of RANDOM_BLOCKSIZE in size; it is
 * the responsibility of the algorithm to accommodate partial block reads, if a
 * block output mode is used.
 */
void
random_fortuna_read(uint8_t *buf, size_t bytecount)
{
	uint8_t newkey[RANDOM_KEYSIZE];

	if (fortuna_concurrent_read) {
		random_fortuna_read_concurrent(buf, bytecount, newkey);
		goto out;
	}

	RANDOM_RESEED_LOCK();
	KASSERT(!uint128_is_zero(fortuna_state.fs_counter), ("FS&K: C != 0"));

	random_fortuna_genbytes(buf, bytecount, newkey,
	    &fortuna_state.fs_counter, &fortuna_state.fs_key, FORTUNA_LOCKED);
	/* Returns unlocked */
	RANDOM_RESEED_ASSERT_LOCK_NOT_OWNED();

out:
	explicit_bzero(newkey, sizeof(newkey));
}

#ifdef _KERNEL
static bool block_seeded_status = false;
SYSCTL_BOOL(_kern_random, OID_AUTO, block_seeded_status, CTLFLAG_RWTUN,
    &block_seeded_status, 0,
    "If non-zero, pretend Fortuna is in an unseeded state.  By setting "
    "this as a tunable, boot can be tested as if the random device is "
    "unavailable.");
#endif

static bool
random_fortuna_seeded_internal(void)
{
	return (!uint128_is_zero(fortuna_state.fs_counter));
}

static bool
random_fortuna_seeded(void)
{

#ifdef _KERNEL
	if (block_seeded_status)
		return (false);
#endif

	if (__predict_true(random_fortuna_seeded_internal()))
		return (true);

	/*
	 * Maybe we have enough entropy in the zeroth pool but just haven't
	 * kicked the initial seed step.  Do so now.
	 */
	random_fortuna_pre_read();

	return (random_fortuna_seeded_internal());
}
