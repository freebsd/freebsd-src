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
 * Copyright (c) 2018-2023 Gavin D. Howard and contributors.
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
 * Definitions for the RNG.
 *
 */

#ifndef BC_RAND_H
#define BC_RAND_H

#include <stdint.h>
#include <inttypes.h>

#include <vector.h>
#include <num.h>

#if BC_ENABLE_EXTRA_MATH

#if BC_ENABLE_LIBRARY
#define BC_RAND_USE_FREE (1)
#else // BC_ENABLE_LIBRARY
#ifndef NDEBUG
#define BC_RAND_USE_FREE (1)
#else // NDEBUG
#define BC_RAND_USE_FREE (0)
#endif // NDEBUG
#endif // BC_ENABLE_LIBRARY

/**
 * A function to return a random unsigned long.
 * @param ptr  A void ptr to some data that will help generate the random ulong.
 * @return     The random ulong.
 */
typedef ulong (*BcRandUlong)(void* ptr);

#if BC_LONG_BIT >= 64

// If longs are 64 bits, we have the option of 128-bit integers on some
// compilers. These two sections test that.
#ifdef BC_RAND_BUILTIN
#if BC_RAND_BUILTIN
#ifndef __SIZEOF_INT128__
#undef BC_RAND_BUILTIN
#define BC_RAND_BUILTIN (0)
#endif // __SIZEOF_INT128__
#endif // BC_RAND_BUILTIN
#endif // BC_RAND_BUILTIN

#ifndef BC_RAND_BUILTIN
#ifdef __SIZEOF_INT128__
#define BC_RAND_BUILTIN (1)
#else // __SIZEOF_INT128__
#define BC_RAND_BUILTIN (0)
#endif // __SIZEOF_INT128__
#endif // BC_RAND_BUILTIN

/// The type for random integers.
typedef uint64_t BcRand;

/// A constant defined by PCG.
#define BC_RAND_ROTC (63)

#if BC_RAND_BUILTIN

/// A typedef for the PCG state.
typedef __uint128_t BcRandState;

/**
 * Multiply two integers, worrying about overflow.
 * @param a  The first integer.
 * @param b  The second integer.
 * @return   The product of the PCG states.
 */
#define bc_rand_mul(a, b) (((BcRandState) (a)) * ((BcRandState) (b)))

/**
 * Add two integers, worrying about overflow.
 * @param a  The first integer.
 * @param b  The second integer.
 * @return   The sum of the PCG states.
 */
#define bc_rand_add(a, b) (((BcRandState) (a)) + ((BcRandState) (b)))

/**
 * Multiply two PCG states.
 * @param a  The first PCG state.
 * @param b  The second PCG state.
 * @return   The product of the PCG states.
 */
#define bc_rand_mul2(a, b) (((BcRandState) (a)) * ((BcRandState) (b)))

/**
 * Add two PCG states.
 * @param a  The first PCG state.
 * @param b  The second PCG state.
 * @return   The sum of the PCG states.
 */
#define bc_rand_add2(a, b) (((BcRandState) (a)) + ((BcRandState) (b)))

/**
 * Figure out if the PRNG has been modified. Since the increment of the PRNG has
 * to be odd, we use the extra bit to store whether it has been modified or not.
 * @param r  The PRNG.
 * @return   True if the PRNG has *not* been modified, false otherwise.
 */
#define BC_RAND_NOTMODIFIED(r) (((r)->inc & 1UL) == 0)

/**
 * Return true if the PRNG has not been seeded yet.
 * @param r  The PRNG.
 * @return   True if the PRNG has not been seeded yet, false otherwise.
 */
#define BC_RAND_ZERO(r) (!(r)->state)

/**
 * Returns a constant built from @a h and @a l.
 * @param h  The high 64 bits.
 * @param l  The low 64 bits.
 * @return   The constant built from @a h and @a l.
 */
#define BC_RAND_CONSTANT(h, l) ((((BcRandState) (h)) << 64) + (BcRandState) (l))

/**
 * Truncates a PCG state to the number of bits in a random integer.
 * @param s  The state to truncate.
 * @return   The truncated state.
 */
#define BC_RAND_TRUNC(s) ((uint64_t) (s))

/**
 * Chops a PCG state in half and returns the top bits.
 * @param s  The state to chop.
 * @return   The chopped state's top bits.
 */
#define BC_RAND_CHOP(s) ((uint64_t) ((s) >> 64UL))

/**
 * Rotates a PCG state.
 * @param s  The state to rotate.
 * @return   The rotated state.
 */
#define BC_RAND_ROTAMT(s) ((unsigned int) ((s) >> 122UL))

#else // BC_RAND_BUILTIN

/// A typedef for the PCG state.
typedef struct BcRandState
{
	/// The low bits.
	uint_fast64_t lo;

	/// The high bits.
	uint_fast64_t hi;

} BcRandState;

/**
 * Multiply two integers, worrying about overflow.
 * @param a  The first integer.
 * @param b  The second integer.
 * @return   The product of the PCG states.
 */
#define bc_rand_mul(a, b) (bc_rand_multiply((a), (b)))

/**
 * Add two integers, worrying about overflow.
 * @param a  The first integer.
 * @param b  The second integer.
 * @return   The sum of the PCG states.
 */
#define bc_rand_add(a, b) (bc_rand_addition((a), (b)))

/**
 * Multiply two PCG states.
 * @param a  The first PCG state.
 * @param b  The second PCG state.
 * @return   The product of the PCG states.
 */
#define bc_rand_mul2(a, b) (bc_rand_multiply2((a), (b)))

/**
 * Add two PCG states.
 * @param a  The first PCG state.
 * @param b  The second PCG state.
 * @return   The sum of the PCG states.
 */
#define bc_rand_add2(a, b) (bc_rand_addition2((a), (b)))

/**
 * Figure out if the PRNG has been modified. Since the increment of the PRNG has
 * to be odd, we use the extra bit to store whether it has been modified or not.
 * @param r  The PRNG.
 * @return   True if the PRNG has *not* been modified, false otherwise.
 */
#define BC_RAND_NOTMODIFIED(r) (((r)->inc.lo & 1) == 0)

/**
 * Return true if the PRNG has not been seeded yet.
 * @param r  The PRNG.
 * @return   True if the PRNG has not been seeded yet, false otherwise.
 */
#define BC_RAND_ZERO(r) (!(r)->state.lo && !(r)->state.hi)

/**
 * Returns a constant built from @a h and @a l.
 * @param h  The high 64 bits.
 * @param l  The low 64 bits.
 * @return   The constant built from @a h and @a l.
 */
#define BC_RAND_CONSTANT(h, l) \
	{                          \
		.lo = (l), .hi = (h)   \
	}

/**
 * Truncates a PCG state to the number of bits in a random integer.
 * @param s  The state to truncate.
 * @return   The truncated state.
 */
#define BC_RAND_TRUNC(s) ((s).lo)

/**
 * Chops a PCG state in half and returns the top bits.
 * @param s  The state to chop.
 * @return   The chopped state's top bits.
 */
#define BC_RAND_CHOP(s) ((s).hi)

/**
 * Returns the rotate amount for a PCG state.
 * @param s  The state to rotate.
 * @return   The semi-rotated state.
 */
#define BC_RAND_ROTAMT(s) ((unsigned int) ((s).hi >> 58UL))

/// A 64-bit integer with the bottom 32 bits set.
#define BC_RAND_BOTTOM32 (((uint_fast64_t) 0xffffffffULL))

/**
 * Returns the 32-bit truncated value of @a n.
 * @param n  The integer to truncate.
 * @return   The bottom 32 bits of @a n.
 */
#define BC_RAND_TRUNC32(n) ((n) & (BC_RAND_BOTTOM32))

/**
 * Returns the second 32 bits of @a n.
 * @param n  The integer to truncate.
 * @return   The second 32 bits of @a n.
 */
#define BC_RAND_CHOP32(n) ((n) >> 32)

#endif // BC_RAND_BUILTIN

/// A constant defined by PCG.
#define BC_RAND_MULTIPLIER \
	BC_RAND_CONSTANT(2549297995355413924ULL, 4865540595714422341ULL)

/**
 * Returns the result of a PCG fold.
 * @param s  The state to fold.
 * @return   The folded state.
 */
#define BC_RAND_FOLD(s) ((BcRand) (BC_RAND_CHOP(s) ^ BC_RAND_TRUNC(s)))

#else // BC_LONG_BIT >= 64

// If we are using 32-bit longs, we need to set these so.
#undef BC_RAND_BUILTIN
#define BC_RAND_BUILTIN (1)

/// The type for random integers.
typedef uint32_t BcRand;

/// A constant defined by PCG.
#define BC_RAND_ROTC (31)

/// A typedef for the PCG state.
typedef uint_fast64_t BcRandState;

/**
 * Multiply two integers, worrying about overflow.
 * @param a  The first integer.
 * @param b  The second integer.
 * @return   The product of the PCG states.
 */
#define bc_rand_mul(a, b) (((BcRandState) (a)) * ((BcRandState) (b)))

/**
 * Add two integers, worrying about overflow.
 * @param a  The first integer.
 * @param b  The second integer.
 * @return   The sum of the PCG states.
 */
#define bc_rand_add(a, b) (((BcRandState) (a)) + ((BcRandState) (b)))

/**
 * Multiply two PCG states.
 * @param a  The first PCG state.
 * @param b  The second PCG state.
 * @return   The product of the PCG states.
 */
#define bc_rand_mul2(a, b) (((BcRandState) (a)) * ((BcRandState) (b)))

/**
 * Add two PCG states.
 * @param a  The first PCG state.
 * @param b  The second PCG state.
 * @return   The sum of the PCG states.
 */
#define bc_rand_add2(a, b) (((BcRandState) (a)) + ((BcRandState) (b)))

/**
 * Figure out if the PRNG has been modified. Since the increment of the PRNG has
 * to be odd, we use the extra bit to store whether it has been modified or not.
 * @param r  The PRNG.
 * @return   True if the PRNG has *not* been modified, false otherwise.
 */
#define BC_RAND_NOTMODIFIED(r) (((r)->inc & 1UL) == 0)

/**
 * Return true if the PRNG has not been seeded yet.
 * @param r  The PRNG.
 * @return   True if the PRNG has not been seeded yet, false otherwise.
 */
#define BC_RAND_ZERO(r) (!(r)->state)

/**
 * Returns a constant built from a number.
 * @param n  The number.
 * @return   The constant built from @a n.
 */
#define BC_RAND_CONSTANT(n) UINT64_C(n)

/// A constant defined by PCG.
#define BC_RAND_MULTIPLIER BC_RAND_CONSTANT(6364136223846793005)

/**
 * Truncates a PCG state to the number of bits in a random integer.
 * @param s  The state to truncate.
 * @return   The truncated state.
 */
#define BC_RAND_TRUNC(s) ((uint32_t) (s))

/**
 * Chops a PCG state in half and returns the top bits.
 * @param s  The state to chop.
 * @return   The chopped state's top bits.
 */
#define BC_RAND_CHOP(s) ((uint32_t) ((s) >> 32UL))

/**
 * Returns the rotate amount for a PCG state.
 * @param s  The state to rotate.
 * @return   The semi-rotated state.
 */
#define BC_RAND_ROTAMT(s) ((unsigned int) ((s) >> 59UL))

/**
 * Returns the result of a PCG fold.
 * @param s  The state to fold.
 * @return   The folded state.
 */
#define BC_RAND_FOLD(s) ((BcRand) ((((s) >> 18U) ^ (s)) >> 27U))

#endif // BC_LONG_BIT >= 64

/**
 * Rotates @a v by @a r bits.
 * @param v  The value to rotate.
 * @param r  The amount to rotate by.
 * @return   The rotated value.
 */
#define BC_RAND_ROT(v, r) \
	((BcRand) (((v) >> (r)) | ((v) << ((0 - (r)) & BC_RAND_ROTC))))

/// The number of bits in a random integer.
#define BC_RAND_BITS (sizeof(BcRand) * CHAR_BIT)

/// The number of bits in a PCG state.
#define BC_RAND_STATE_BITS (sizeof(BcRandState) * CHAR_BIT)

/// The size of a BcNum with the max random integer. This isn't exact; it's
/// actually rather crude. But it's always enough.
#define BC_RAND_NUM_SIZE (BC_NUM_BIGDIG_LOG10 * 2 + 2)

/// The mask for how many bits bc_rand_srand() can set per iteration.
#define BC_RAND_SRAND_BITS ((1 << CHAR_BIT) - 1)

/// The actual RNG data. These are the actual PRNG's.
typedef struct BcRNGData
{
	/// The state.
	BcRandState state;

	/// The increment and the modified bit.
	BcRandState inc;

} BcRNGData;

/// The public PRNG. This is just a stack of PRNG's to maintain the globals
/// stack illusion.
typedef struct BcRNG
{
	/// The stack of PRNG's.
	BcVec v;

} BcRNG;

/**
 * Initializes a BcRNG.
 * @param r  The BcRNG to initialize.
 */
void
bc_rand_init(BcRNG* r);

#if BC_RAND_USE_FREE

/**
 * Frees a BcRNG. This is only in debug builds because it would only be freed on
 * exit.
 * @param r  The BcRNG to free.
 */
void
bc_rand_free(BcRNG* r);

#endif // BC_RAND_USE_FREE

/**
 * Returns a random integer from the PRNG.
 * @param r  The PRNG.
 * @return   A random integer.
 */
BcRand
bc_rand_int(BcRNG* r);

/**
 * Returns a random integer from the PRNG bounded by @a bound. Bias is
 * eliminated.
 * @param r      The PRNG.
 * @param bound  The bound for the random integer.
 * @return       A bounded random integer.
 */
BcRand
bc_rand_bounded(BcRNG* r, BcRand bound);

/**
 * Seed the PRNG with the state in two parts and the increment in two parts.
 * @param r       The PRNG.
 * @param state1  The first part of the state.
 * @param state2  The second part of the state.
 * @param inc1    The first part of the increment.
 * @param inc2    The second part of the increment.
 */
void
bc_rand_seed(BcRNG* r, ulong state1, ulong state2, ulong inc1, ulong inc2);

/**
 * Pushes a new PRNG onto the PRNG stack.
 * @param r  The PRNG.
 */
void
bc_rand_push(BcRNG* r);

/**
 * Pops one or all but one items off of the PRNG stack.
 * @param r      The PRNG.
 * @param reset  True if all but one PRNG should be popped off the stack, false
 *               if only one should be popped.
 */
void
bc_rand_pop(BcRNG* r, bool reset);

/**
 * Returns, via pointers, the state of the PRNG in pieces.
 * @param r   The PRNG.
 * @param s1  The return value for the first part of the state.
 * @param s2  The return value for the second part of the state.
 * @param i1  The return value for the first part of the increment.
 * @param i2  The return value for the second part of the increment.
 */
void
bc_rand_getRands(BcRNG* r, BcRand* s1, BcRand* s2, BcRand* i1, BcRand* i2);

/**
 * Seed the PRNG with random data.
 * @param rng  The PRNG.
 */
void
bc_rand_srand(BcRNGData* rng);

/// A reference to a constant multiplier.
extern const BcRandState bc_rand_multiplier;

#endif // BC_ENABLE_EXTRA_MATH

#endif // BC_RAND_H
