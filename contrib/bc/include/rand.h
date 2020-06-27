/*
 * *****************************************************************************
 *
 * Copyright (c) 2018-2019 Gavin D. Howard and contributors.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
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
 * Parts of this code are also under the following license:
 *
 * Copyright (c) 2014-2017 Melissa O'Neill and PCG Project contributors
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

#if BC_ENABLE_EXTRA_MATH

#include <stdint.h>
#include <inttypes.h>

#include <vector.h>
#include <num.h>

typedef ulong (*BcRandUlong)(void*);

#if BC_LONG_BIT >= 64

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

typedef uint64_t BcRand;

#define BC_RAND_ROTC (63)

#if BC_RAND_BUILTIN

typedef __uint128_t BcRandState;

#define bc_rand_mul(a, b) (((BcRandState) (a)) * ((BcRandState) (b)))
#define bc_rand_add(a, b) (((BcRandState) (a)) + ((BcRandState) (b)))

#define bc_rand_mul2(a, b) (((BcRandState) (a)) * ((BcRandState) (b)))
#define bc_rand_add2(a, b) (((BcRandState) (a)) + ((BcRandState) (b)))

#define BC_RAND_NOTMODIFIED(r) (((r)->inc & 1UL) == 0)
#define BC_RAND_ZERO(r) (!(r)->state)

#define BC_RAND_CONSTANT(h, l) ((((BcRandState) (h)) << 64) + (BcRandState) (l))

#define BC_RAND_TRUNC(s) ((uint64_t) (s))
#define BC_RAND_CHOP(s) ((uint64_t) ((s) >> 64UL))
#define BC_RAND_ROTAMT(s) ((unsigned int) ((s) >> 122UL))

#else // BC_RAND_BUILTIN

typedef struct BcRandState {

	uint_fast64_t lo;
	uint_fast64_t hi;

} BcRandState;

#define bc_rand_mul(a, b) (bc_rand_multiply((a), (b)))
#define bc_rand_add(a, b) (bc_rand_addition((a), (b)))

#define bc_rand_mul2(a, b) (bc_rand_multiply2((a), (b)))
#define bc_rand_add2(a, b) (bc_rand_addition2((a), (b)))

#define BC_RAND_NOTMODIFIED(r) (((r)->inc.lo & 1) == 0)
#define BC_RAND_ZERO(r) (!(r)->state.lo && !(r)->state.hi)

#define BC_RAND_CONSTANT(h, l) { .lo = (l), .hi = (h) }

#define BC_RAND_TRUNC(s) ((s).lo)
#define BC_RAND_CHOP(s) ((s).hi)
#define BC_RAND_ROTAMT(s) ((unsigned int) ((s).hi >> 58UL))

#define BC_RAND_BOTTOM32 (((uint_fast64_t) 0xffffffffULL))
#define BC_RAND_TRUNC32(n) ((n) & BC_RAND_BOTTOM32)
#define BC_RAND_CHOP32(n) ((n) >> 32)

#endif // BC_RAND_BUILTIN

#define BC_RAND_MULTIPLIER \
	BC_RAND_CONSTANT(2549297995355413924ULL, 4865540595714422341ULL)

#define BC_RAND_FOLD(s) ((BcRand) (BC_RAND_CHOP(s) ^ BC_RAND_TRUNC(s)))

#else // BC_LONG_BIT >= 64

#undef BC_RAND_BUILTIN
#define BC_RAND_BUILTIN (1)

typedef uint32_t BcRand;

#define BC_RAND_ROTC (31)

typedef uint_fast64_t BcRandState;

#define bc_rand_mul(a, b) (((BcRandState) (a)) * ((BcRandState) (b)))
#define bc_rand_add(a, b) (((BcRandState) (a)) + ((BcRandState) (b)))

#define bc_rand_mul2(a, b) (((BcRandState) (a)) * ((BcRandState) (b)))
#define bc_rand_add2(a, b) (((BcRandState) (a)) + ((BcRandState) (b)))

#define BC_RAND_NOTMODIFIED(r) (((r)->inc & 1UL) == 0)
#define BC_RAND_ZERO(r) (!(r)->state)

#define BC_RAND_CONSTANT UINT64_C
#define BC_RAND_MULTIPLIER BC_RAND_CONSTANT(6364136223846793005)

#define BC_RAND_TRUNC(s) ((uint32_t) (s))
#define BC_RAND_CHOP(s) ((uint32_t) ((s) >> 32UL))
#define BC_RAND_ROTAMT(s) ((unsigned int) ((s) >> 59UL))

#define BC_RAND_FOLD(s) ((BcRand) ((((s) >> 18U) ^ (s)) >> 27U))

#endif // BC_LONG_BIT >= 64

#define BC_RAND_ROT(v, r) \
	((BcRand) (((v) >> (r)) | ((v) << ((0 - (r)) & BC_RAND_ROTC))))

#define BC_RAND_BITS (sizeof(BcRand) * CHAR_BIT)
#define BC_RAND_STATE_BITS (sizeof(BcRandState) * CHAR_BIT)

#define BC_RAND_NUM_SIZE (BC_NUM_BIGDIG_LOG10 * 2 + 2)

#define BC_RAND_SRAND_BITS ((1 << CHAR_BIT) - 1)

typedef struct BcRNGData {

	BcRandState state;
	BcRandState inc;

} BcRNGData;

typedef struct BcRNG {

	BcVec v;

} BcRNG;

void bc_rand_init(BcRNG *r);
#ifndef NDEBUG
void bc_rand_free(BcRNG *r);
#endif // NDEBUG

BcRand bc_rand_int(BcRNG *r);
BcRand bc_rand_bounded(BcRNG *r, BcRand bound);
void bc_rand_seed(BcRNG *r, ulong state1, ulong state2, ulong inc1, ulong inc2);
void bc_rand_push(BcRNG *r);
void bc_rand_pop(BcRNG *r, bool reset);
void bc_rand_getRands(BcRNG *r, BcRand *s1, BcRand *s2, BcRand *i1, BcRand *i2);

extern const BcRandState bc_rand_multiplier;

#endif // BC_ENABLE_EXTRA_MATH

#endif // BC_RAND_H
