/*
 * *****************************************************************************
 *
 * Parts of this code are adapted from the following:
 *
 * PCG, A Family of Better Random Number Generators.
 *
 * You can find the original source code at:
 *   https://github.com/imneme/pcg-c
 *
 * -----------------------------------------------------------------------------
 *
 * This code is under the following license:
 *
 * Copyright (c) 2014-2017 Melissa O'Neill and PCG Project contributors
 * Copyright (c) 2018-2021 Gavin D. Howard and contributors.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * *****************************************************************************
 *
 * Code for the RNG.
 *
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>

#ifndef _WIN32
#include <unistd.h>
#else // _WIN32
#include <Windows.h>
#include <bcrypt.h>
#endif // _WIN32

#include <status.h>
#include <rand.h>
#include <vm.h>

#if BC_ENABLE_EXTRA_MATH

#if !BC_RAND_BUILTIN

/**
 * Adds two 64-bit values and preserves the overflow.
 * @param a  The first operand.
 * @param b  The second operand.
 * @return   The sum, including overflow.
 */
static BcRandState bc_rand_addition(uint_fast64_t a, uint_fast64_t b) {

	BcRandState res;

	res.lo = a + b;
	res.hi = (res.lo < a);

	return res;
}

/**
 * Adds two 128-bit values and discards the overflow.
 * @param a  The first operand.
 * @param b  The second operand.
 * @return   The sum, without overflow.
 */
static BcRandState bc_rand_addition2(BcRandState a, BcRandState b) {

	BcRandState temp, res;

	res = bc_rand_addition(a.lo, b.lo);
	temp = bc_rand_addition(a.hi, b.hi);
	res.hi += temp.lo;

	return res;
}

/**
 * Multiplies two 64-bit values and preserves the overflow.
 * @param a  The first operand.
 * @param b  The second operand.
 * @return   The product, including overflow.
 */
static BcRandState bc_rand_multiply(uint_fast64_t a, uint_fast64_t b) {

	uint_fast64_t al, ah, bl, bh, c0, c1, c2, c3;
	BcRandState carry, res;

	al = BC_RAND_TRUNC32(a);
	ah = BC_RAND_CHOP32(a);
	bl = BC_RAND_TRUNC32(b);
	bh = BC_RAND_CHOP32(b);

	c0 = al * bl;
	c1 = al * bh;
	c2 = ah * bl;
	c3 = ah * bh;

	carry = bc_rand_addition(c1, c2);

	res = bc_rand_addition(c0, (BC_RAND_TRUNC32(carry.lo)) << 32);
	res.hi += BC_RAND_CHOP32(carry.lo) + c3 + (carry.hi << 32);

	return res;
}

/**
 * Multiplies two 128-bit values and discards the overflow.
 * @param a  The first operand.
 * @param b  The second operand.
 * @return   The product, without overflow.
 */
static BcRandState bc_rand_multiply2(BcRandState a, BcRandState b) {

	BcRandState c0, c1, c2, carry;

	c0 = bc_rand_multiply(a.lo, b.lo);
	c1 = bc_rand_multiply(a.lo, b.hi);
	c2 = bc_rand_multiply(a.hi, b.lo);

	carry = bc_rand_addition2(c1, c2);
	carry.hi = carry.lo;
	carry.lo = 0;

	return bc_rand_addition2(c0, carry);
}

#endif // BC_RAND_BUILTIN

/**
 * Marks a PRNG as modified. This is important for properly maintaining the
 * stack of PRNG's.
 * @param r  The PRNG to mark as modified.
 */
static void bc_rand_setModified(BcRNGData *r) {

#if BC_RAND_BUILTIN
	r->inc |= (BcRandState) 1UL;
#else // BC_RAND_BUILTIN
	r->inc.lo |= (uint_fast64_t) 1UL;
#endif // BC_RAND_BUILTIN
}

/**
 * Marks a PRNG as not modified. This is important for properly maintaining the
 * stack of PRNG's.
 * @param r  The PRNG to mark as not modified.
 */
static void bc_rand_clearModified(BcRNGData *r) {

#if BC_RAND_BUILTIN
	r->inc &= ~((BcRandState) 1UL);
#else // BC_RAND_BUILTIN
	r->inc.lo &= ~(1UL);
#endif // BC_RAND_BUILTIN
}

/**
 * Copies a PRNG to another and marks the copy as modified if it already was or
 * marks it modified if it already was.
 * @param d  The destination PRNG.
 * @param s  The source PRNG.
 */
static void bc_rand_copy(BcRNGData *d, BcRNGData *s) {
	bool unmod = BC_RAND_NOTMODIFIED(d);
	memcpy(d, s, sizeof(BcRNGData));
	if (!unmod) bc_rand_setModified(d);
	else if (!BC_RAND_NOTMODIFIED(s)) bc_rand_clearModified(d);
}

#ifndef _WIN32

/**
 * Reads random data from a file.
 * @param ptr  A pointer to the file, as a void pointer.
 * @return     The random data as an unsigned long.
 */
static ulong bc_rand_frand(void* ptr) {

	ulong buf[1];
	int fd;
	ssize_t nread;

	assert(ptr != NULL);

	fd = *((int*)ptr);

	nread = read(fd, buf, sizeof(ulong));

	if (BC_ERR(nread != sizeof(ulong))) bc_vm_fatalError(BC_ERR_FATAL_IO_ERR);

	return *((ulong*)buf);
}
#else // _WIN32

/**
 * Reads random data from BCryptGenRandom().
 * @param ptr  An unused parameter.
 * @return     The random data as an unsigned long.
 */
static ulong bc_rand_winrand(void *ptr) {

	ulong buf[1];
	NTSTATUS s;

	BC_UNUSED(ptr);

	buf[0] = 0;

	s = BCryptGenRandom(NULL, (char*) buf, sizeof(ulong),
	                    BCRYPT_USE_SYSTEM_PREFERRED_RNG);

	if (BC_ERR(!BCRYPT_SUCCESS(s))) buf[0] = 0;

	return buf[0];
}
#endif // _WIN32

/**
 * Reads random data from rand(), byte-by-byte because rand() is only guaranteed
 * to return 15 bits of random data. This is the final fallback and is not
 * preferred as it is possible to access cryptographically-secure PRNG's on most
 * systems.
 * @param ptr  An unused parameter.
 * @return     The random data as an unsigned long.
 */
static ulong bc_rand_rand(void *ptr) {

	size_t i;
	ulong res = 0;

	BC_UNUSED(ptr);

	// Fill up the unsigned long byte-by-byte.
	for (i = 0; i < sizeof(ulong); ++i)
		res |= ((ulong) (rand() & BC_RAND_SRAND_BITS)) << (i * CHAR_BIT);

	return res;
}

/**
 * Returns the actual increment of the PRNG, including the required last odd
 * bit.
 * @param r  The PRNG.
 * @return   The increment of the PRNG, including the last odd bit.
 */
static BcRandState bc_rand_inc(BcRNGData *r) {

	BcRandState inc;

#if BC_RAND_BUILTIN
	inc = r->inc | 1;
#else // BC_RAND_BUILTIN
	inc.lo = r->inc.lo | 1;
	inc.hi = r->inc.hi;
#endif // BC_RAND_BUILTIN

	return inc;
}

/**
 * Sets up the increment for the PRNG.
 * @param r  The PRNG whose increment will be set up.
 */
static void bc_rand_setupInc(BcRNGData *r) {

#if BC_RAND_BUILTIN
	r->inc <<= 1UL;
#else // BC_RAND_BUILTIN
	r->inc.hi <<= 1UL;
	r->inc.hi |= (r->inc.lo & (1UL << (BC_LONG_BIT - 1))) >> (BC_LONG_BIT - 1);
	r->inc.lo <<= 1UL;
#endif // BC_RAND_BUILTIN
}

/**
 * Seeds the state of a PRNG.
 * @param state  The return parameter; the state to seed.
 * @param val1   The lower half of the state.
 * @param val2   The upper half of the state.
 */
static void bc_rand_seedState(BcRandState *state, ulong val1, ulong val2) {

#if BC_RAND_BUILTIN
	*state = ((BcRandState) val1) | ((BcRandState) val2) << (BC_LONG_BIT);
#else // BC_RAND_BUILTIN
	state->lo = val1;
	state->hi = val2;
#endif // BC_RAND_BUILTIN
}

/**
 * Seeds a PRNG.
 * @param r       The return parameter; the PRNG to seed.
 * @param state1  The lower half of the state.
 * @param state2  The upper half of the state.
 * @param inc1    The lower half of the increment.
 * @param inc2    The upper half of the increment.
 */
static void bc_rand_seedRNG(BcRNGData *r, ulong state1, ulong state2,
                            ulong inc1, ulong inc2)
{
	bc_rand_seedState(&r->state, state1, state2);
	bc_rand_seedState(&r->inc, inc1, inc2);
	bc_rand_setupInc(r);
}

/**
 * Fills a PRNG with random data to seed it.
 * @param r       The PRNG.
 * @param fulong  The function to fill an unsigned long.
 * @param ptr     The parameter to pass to @a fulong.
 */
static void bc_rand_fill(BcRNGData *r, BcRandUlong fulong, void *ptr) {

	ulong state1, state2, inc1, inc2;

	state1 = fulong(ptr);
	state2 = fulong(ptr);

	inc1 = fulong(ptr);
	inc2 = fulong(ptr);

	bc_rand_seedRNG(r, state1, state2, inc1, inc2);
}

/**
 * Executes the "step" portion of a PCG udpate.
 * @param r  The PRNG.
 */
static void bc_rand_step(BcRNGData *r) {
	BcRandState temp = bc_rand_mul2(r->state, bc_rand_multiplier);
	r->state = bc_rand_add2(temp, bc_rand_inc(r));
}

/**
 * Returns the new output of PCG.
 * @param r  The PRNG.
 * @return   The new output from the PRNG.
 */
static BcRand bc_rand_output(BcRNGData *r) {
	return BC_RAND_ROT(BC_RAND_FOLD(r->state), BC_RAND_ROTAMT(r->state));
}

/**
 * Seeds every PRNG on the PRNG stack between the top and @a idx that has not
 * been seeded.
 * @param r    The PRNG stack.
 * @param rng  The PRNG on the top of the stack. Must have been seeded.
 */
static void bc_rand_seedZeroes(BcRNG *r, BcRNGData *rng, size_t idx) {

	BcRNGData *rng2;

	// Just return if there are none to do.
	if (r->v.len <= idx) return;

	// Get the first PRNG that might need to be seeded.
	rng2 = bc_vec_item_rev(&r->v, idx);

	// Does it need seeding? Then it, and maybe more, do.
	if (BC_RAND_ZERO(rng2)) {

		size_t i;

		// Seed the ones that need seeding.
		for (i = 1; i < r->v.len; ++i)
			bc_rand_copy(bc_vec_item_rev(&r->v, i), rng);
	}
}

void bc_rand_srand(BcRNGData *rng) {

	int fd = 0;

	BC_SIG_LOCK;

#ifndef _WIN32

	// Try /dev/urandom first.
	fd = open("/dev/urandom", O_RDONLY);

	if (BC_NO_ERR(fd >= 0)) {
		bc_rand_fill(rng, bc_rand_frand, &fd);
		close(fd);
	}
	else {

		// Try /dev/random second.
		fd = open("/dev/random", O_RDONLY);

		if (BC_NO_ERR(fd >= 0)) {
			bc_rand_fill(rng, bc_rand_frand, &fd);
			close(fd);
		}
	}
#else // _WIN32
	// Try BCryptGenRandom first.
	bc_rand_fill(rng, bc_rand_winrand, NULL);
#endif // _WIN32

	// Fallback to rand() until the thing is seeded.
	while (BC_ERR(BC_RAND_ZERO(rng))) bc_rand_fill(rng, bc_rand_rand, NULL);

	BC_SIG_UNLOCK;
}

/**
 * Propagates a change to the PRNG to all PRNG's in the stack that should have
 * it. The ones that should have it are laid out in the manpages.
 * @param r    The PRNG stack.
 * @param rng  The PRNG that will be used to seed the others.
 */
static void bc_rand_propagate(BcRNG *r, BcRNGData *rng) {

	// Just return if there are none to do.
	if (r->v.len <= 1) return;

	// If the PRNG has not been modified...
	if (BC_RAND_NOTMODIFIED(rng)) {

		size_t i;
		bool go = true;

		// Find the first PRNG that is modified and seed the others.
		for (i = 1; go && i < r->v.len; ++i) {

			BcRNGData *rng2 = bc_vec_item_rev(&r->v, i);

			go = BC_RAND_NOTMODIFIED(rng2);

			bc_rand_copy(rng2, rng);
		}

		// Seed everything else.
		bc_rand_seedZeroes(r, rng, i);
	}
	// Seed everything.
	else bc_rand_seedZeroes(r, rng, 1);
}

BcRand bc_rand_int(BcRNG *r) {

	// Get the actual PRNG.
	BcRNGData *rng = bc_vec_top(&r->v);
	BcRand res;

	// Make sure the PRNG is seeded.
	if (BC_ERR(BC_RAND_ZERO(rng))) bc_rand_srand(rng);

	BC_SIG_LOCK;

	// This is the important part of the PRNG. This is the stuff from PCG.
	bc_rand_step(rng);
	bc_rand_propagate(r, rng);
	res = bc_rand_output(rng);

	BC_SIG_UNLOCK;

	return res;
}

BcRand bc_rand_bounded(BcRNG *r, BcRand bound) {

	// Calculate the threshold below which we have to try again.
	BcRand rand, threshold = (0 - bound) % bound;

	do {
		rand = bc_rand_int(r);
	} while (rand < threshold);

	return rand % bound;
}

void bc_rand_seed(BcRNG *r, ulong state1, ulong state2, ulong inc1, ulong inc2)
{
	// Get the actual PRNG.
	BcRNGData *rng = bc_vec_top(&r->v);

	// Seed and set up the PRNG's increment.
	bc_rand_seedState(&rng->inc, inc1, inc2);
	bc_rand_setupInc(rng);
	bc_rand_setModified(rng);

	// If the state is 0, use the increment as the state. Otherwise, seed it
	// with the state.
	if (!state1 && !state2) {
		memcpy(&rng->state, &rng->inc, sizeof(BcRandState));
		bc_rand_step(rng);
	}
	else bc_rand_seedState(&rng->state, state1, state2);

	// Propagate the change to PRNG's that need it.
	bc_rand_propagate(r, rng);
}

/**
 * Returns the increment in the PRNG *without* the odd bit and also with being
 * shifted one bit down.
 * @param r  The PRNG.
 * @return   The increment without the odd bit and with being shifted one bit
 *           down.
 */
static BcRandState bc_rand_getInc(BcRNGData *r) {

	BcRandState res;

#if BC_RAND_BUILTIN
	res = r->inc >> 1;
#else // BC_RAND_BUILTIN
	res = r->inc;
	res.lo >>= 1;
	res.lo |= (res.hi & 1) << (BC_LONG_BIT - 1);
	res.hi >>= 1;
#endif // BC_RAND_BUILTIN

	return res;
}

void bc_rand_getRands(BcRNG *r, BcRand *s1, BcRand *s2, BcRand *i1, BcRand *i2)
{
	BcRandState inc;
	BcRNGData *rng = bc_vec_top(&r->v);

	if (BC_ERR(BC_RAND_ZERO(rng))) bc_rand_srand(rng);

	// Get the increment.
	inc = bc_rand_getInc(rng);

	// Chop the state.
	*s1 = BC_RAND_TRUNC(rng->state);
	*s2 = BC_RAND_CHOP(rng->state);

	// Chop the increment.
	*i1 = BC_RAND_TRUNC(inc);
	*i2 = BC_RAND_CHOP(inc);
}

void bc_rand_push(BcRNG *r) {

	BcRNGData *rng = bc_vec_pushEmpty(&r->v);

	// Make sure the PRNG is properly zeroed because that marks it as needing to
	// be seeded.
	memset(rng, 0, sizeof(BcRNGData));

	// If there is another item, copy it too.
	if (r->v.len > 1) bc_rand_copy(rng, bc_vec_item_rev(&r->v, 1));
}

void bc_rand_pop(BcRNG *r, bool reset) {
	bc_vec_npop(&r->v, reset ? r->v.len - 1 : 1);
}

void bc_rand_init(BcRNG *r) {
	BC_SIG_ASSERT_LOCKED;
	bc_vec_init(&r->v, sizeof(BcRNGData), BC_DTOR_NONE);
	bc_rand_push(r);
}

#if BC_RAND_USE_FREE
void bc_rand_free(BcRNG *r) {
	BC_SIG_ASSERT_LOCKED;
	bc_vec_free(&r->v);
}
#endif // BC_RAND_USE_FREE

#endif // BC_ENABLE_EXTRA_MATH
