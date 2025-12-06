/*
 * *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2025 Gavin D. Howard and contributors.
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
 * Definitions for the num type.
 *
 */

#ifndef BC_NUM_H
#define BC_NUM_H

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <sys/types.h>

#include <status.h>
#include <vector.h>
#include <bcl.h>

/// Everything in bc is base 10..
#define BC_BASE (10)

/// Alias.
typedef unsigned long ulong;

/// This is here because BcBigDig came first, but when I created bcl, it's
/// definition has to be defined first.
typedef BclBigDig BcBigDig;

#if BC_LONG_BIT >= 64

/// The biggest number held by a BcBigDig.
#define BC_NUM_BIGDIG_MAX ((BcBigDig) UINT64_MAX)

/// The number of decimal digits in one limb.
#define BC_BASE_DIGS (9)

/// The max number + 1 that one limb can hold.
#define BC_BASE_POW (1000000000)

/// An alias for portability.
#define BC_NUM_BIGDIG_C UINT64_C

/// The max number + 1 that two limbs can hold. This is used for generating
/// numbers because the PRNG can generate a number that will fill two limbs.
#define BC_BASE_RAND_POW (BC_NUM_BIGDIG_C(1000000000000000000))

/// The actual limb type.
typedef int_least32_t BcDig;

#elif BC_LONG_BIT >= 32

/// The biggest number held by a BcBigDig.
#define BC_NUM_BIGDIG_MAX ((BcBigDig) UINT32_MAX)

/// The number of decimal digits in one limb.
#define BC_BASE_DIGS (4)

/// The max number + 1 that one limb can hold.
#define BC_BASE_POW (10000)

/// An alias for portability.
#define BC_NUM_BIGDIG_C UINT32_C

/// The max number + 1 that two limbs can hold. This is used for generating
/// numbers because the PRNG can generate a number that will fill two limbs.
#define BC_BASE_RAND_POW (UINT64_C(100000000))

/// The actual limb type.
typedef int_least16_t BcDig;

#else

/// LONG_BIT must be at least 32 on POSIX. We depend on that.
#error BC_LONG_BIT must be at least 32

#endif // BC_LONG_BIT >= 64

/// The default (and minimum) number of limbs when allocating a number.
#define BC_NUM_DEF_SIZE (8)

/// The actual number struct. This is where the magic happens.
typedef struct BcNum
{
	/// The limb array. It is restrict because *no* other item should own the
	/// array. For more information, see the development manual
	/// (manuals/development.md#numbers).
	BcDig* restrict num;

	/// The number of limbs before the decimal (radix) point. This also stores
	/// the negative bit in the least significant bit since it uses at least two
	/// bits less than scale. It is also used less than scale. See the
	/// development manual (manuals/development.md#numbers) for more info.
	size_t rdx;

	/// The actual scale of the number. This is different from rdx because there
	/// are multiple digits in one limb, and in the last limb, only some of the
	/// digits may be part of the scale. However, scale must always match rdx
	/// (except when the number is 0), or there is a bug. For more information,
	/// see the development manual (manuals/development.md#numbers).
	size_t scale;

	/// The number of valid limbs in the array. If this is 0, then the number is
	/// 0 as well.
	size_t len;

	/// The capacity of the limbs array. This is how many limbs the number could
	/// expand to without reallocation.
	size_t cap;

} BcNum;

#if BC_ENABLE_EXTRA_MATH

// Forward declaration
struct BcRNG;

#endif // BC_ENABLE_EXTRA_MATH

/// The minimum obase.
#define BC_NUM_MIN_BASE (BC_NUM_BIGDIG_C(2))

/// The maximum ibase allowed by POSIX.
#define BC_NUM_MAX_POSIX_IBASE (BC_NUM_BIGDIG_C(16))

/// The actual ibase supported by this implementation.
#define BC_NUM_MAX_IBASE (BC_NUM_BIGDIG_C(36))

/// The max base allowed by bc_num_parseChar().
#define BC_NUM_MAX_LBASE (BC_NUM_BIGDIG_C('Z' + BC_BASE + 1))

/// The default number of characters to print before a backslash newline.
#define BC_NUM_PRINT_WIDTH (BC_NUM_BIGDIG_C(69))

/// The base for printing streams from numbers.
#define BC_NUM_STREAM_BASE (256)

// This sets a default for the Karatsuba length.
#ifndef BC_NUM_KARATSUBA_LEN
#define BC_NUM_KARATSUBA_LEN (BC_NUM_BIGDIG_C(32))
#elif BC_NUM_KARATSUBA_LEN < 16
#error BC_NUM_KARATSUBA_LEN must be at least 16.
#endif // BC_NUM_KARATSUBA_LEN

// A crude, but always big enough, calculation of
// the size required for ibase and obase BcNum's.
#define BC_NUM_BIGDIG_LOG10 (BC_NUM_DEF_SIZE)

/**
 * Returns non-zero if the BcNum @a n is non-zero.
 * @param n  The number to test.
 * @return   Non-zero if @a n is non-zero, zero otherwise.
 */
#define BC_NUM_NONZERO(n) ((n)->len)

/**
 * Returns true if the BcNum @a n is zero.
 * @param n  The number to test.
 * @return   True if @a n is zero, false otherwise.
 */
#define BC_NUM_ZERO(n) (!BC_NUM_NONZERO(n))

/**
 * Returns true if the BcNum @a n is one with no scale.
 * @param n  The number to test.
 * @return   True if @a n equals 1 with no scale, false otherwise.
 */
#define BC_NUM_ONE(n) ((n)->len == 1 && (n)->rdx == 0 && (n)->num[0] == 1)

/**
 * Converts the letter @a c into a number.
 * @param c  The letter to convert.
 * @return   The number corresponding to the letter.
 */
#define BC_NUM_NUM_LETTER(c) ((c) - 'A' + BC_BASE)

/// The number of allocations done by bc_num_k(). If you change the number of
/// allocations, you must change this. This is done in order to allocate them
/// all as one allocation and just give them all pointers to different parts.
/// Works pretty well, but you have to be careful.
#define BC_NUM_KARATSUBA_ALLOCS (6)

/**
 * Rounds @a s (scale) up to the next power of BC_BASE_DIGS. This will also
 * check for overflow and gives a fatal error if that happens because we just
 * can't go over the limits we have imposed.
 * @param s  The scale to round up.
 * @return   @a s rounded up to the next power of BC_BASE_DIGS.
 */
#define BC_NUM_ROUND_POW(s) (bc_vm_growSize((s), BC_BASE_DIGS - 1))

/**
 * Returns the equivalent rdx for the scale @a s.
 * @param s  The scale to convert.
 * @return   The rdx for @a s.
 */
#define BC_NUM_RDX(s) (BC_NUM_ROUND_POW(s) / BC_BASE_DIGS)

/**
 * Returns the actual rdx of @a n. (It removes the negative bit.)
 * @param n  The number.
 * @return   The real rdx of @a n.
 */
#define BC_NUM_RDX_VAL(n) ((n)->rdx >> 1)

/**
 * Returns the actual rdx of @a n, where @a n is not a pointer. (It removes the
 * negative bit.)
 * @param n  The number.
 * @return   The real rdx of @a n.
 */
#define BC_NUM_RDX_VAL_NP(n) ((n).rdx >> 1)

/**
 * Sets the rdx of @a n to @a v.
 * @param n  The number.
 * @param v  The value to set the rdx to.
 */
#define BC_NUM_RDX_SET(n, v) \
	((n)->rdx = (((v) << 1) | ((n)->rdx & (BcBigDig) 1)))

/**
 * Sets the rdx of @a n to @a v, where @a n is not a pointer.
 * @param n  The number.
 * @param v  The value to set the rdx to.
 */
#define BC_NUM_RDX_SET_NP(n, v) \
	((n).rdx = (((v) << 1) | ((n).rdx & (BcBigDig) 1)))

/**
 * Sets the rdx of @a n to @a v and the negative bit to @a neg.
 * @param n    The number.
 * @param v    The value to set the rdx to.
 * @param neg  The value to set the negative bit to.
 */
#define BC_NUM_RDX_SET_NEG(n, v, neg) ((n)->rdx = (((v) << 1) | (neg)))

/**
 * Returns true if the rdx and scale for @a n match.
 * @param n  The number to test.
 * @return   True if the rdx and scale of @a n match, false otherwise.
 */
#define BC_NUM_RDX_VALID(n) \
	(BC_NUM_ZERO(n) || BC_NUM_RDX_VAL(n) * BC_BASE_DIGS >= (n)->scale)

/**
 * Returns true if the rdx and scale for @a n match, where @a n is not a
 * pointer.
 * @param n  The number to test.
 * @return   True if the rdx and scale of @a n match, false otherwise.
 */
#define BC_NUM_RDX_VALID_NP(n) \
	((!(n).len) || BC_NUM_RDX_VAL_NP(n) * BC_BASE_DIGS >= (n).scale)

/**
 * Returns true if @a n is negative, false otherwise.
 * @param n  The number to test.
 * @return   True if @a n is negative, false otherwise.
 */
#define BC_NUM_NEG(n) ((n)->rdx & ((BcBigDig) 1))

/**
 * Returns true if @a n is negative, false otherwise, where @a n is not a
 * pointer.
 * @param n  The number to test.
 * @return   True if @a n is negative, false otherwise.
 */
#define BC_NUM_NEG_NP(n) ((n).rdx & ((BcBigDig) 1))

/**
 * Clears the negative bit on @a n.
 * @param n  The number.
 */
#define BC_NUM_NEG_CLR(n) ((n)->rdx &= ~((BcBigDig) 1))

/**
 * Clears the negative bit on @a n, where @a n is not a pointer.
 * @param n  The number.
 */
#define BC_NUM_NEG_CLR_NP(n) ((n).rdx &= ~((BcBigDig) 1))

/**
 * Sets the negative bit on @a n.
 * @param n  The number.
 */
#define BC_NUM_NEG_SET(n) ((n)->rdx |= ((BcBigDig) 1))

/**
 * Toggles the negative bit on @a n.
 * @param n  The number.
 */
#define BC_NUM_NEG_TGL(n) ((n)->rdx ^= ((BcBigDig) 1))

/**
 * Toggles the negative bit on @a n, where @a n is not a pointer.
 * @param n  The number.
 */
#define BC_NUM_NEG_TGL_NP(n) ((n).rdx ^= ((BcBigDig) 1))

/**
 * Returns the rdx val for @a n if the negative bit is set to @a v.
 * @param n  The number.
 * @param v  The value for the negative bit.
 * @return   The value of the rdx of @a n if the negative bit were set to @a v.
 */
#define BC_NUM_NEG_VAL(n, v) (((n)->rdx & ~((BcBigDig) 1)) | (v))

/**
 * Returns the rdx val for @a n if the negative bit is set to @a v, where @a n
 * is not a pointer.
 * @param n  The number.
 * @param v  The value for the negative bit.
 * @return   The value of the rdx of @a n if the negative bit were set to @a v.
 */
#define BC_NUM_NEG_VAL_NP(n, v) (((n).rdx & ~((BcBigDig) 1)) | (v))

/**
 * Returns the size, in bytes, of limb array with @a n limbs.
 * @param n  The number.
 * @return   The size, in bytes, of a limb array with @a n limbs.
 */
#define BC_NUM_SIZE(n) ((n) * sizeof(BcDig))

// These are for debugging only.
#if BC_DEBUG_CODE
#define BC_NUM_PRINT(x) fprintf(stderr, "%s = %lu\n", #x, (unsigned long) (x))
#define DUMP_NUM bc_num_dump
#else // BC_DEBUG_CODE
#undef DUMP_NUM
#define DUMP_NUM(x, y)
#define BC_NUM_PRINT(x)
#endif // BC_DEBUG_CODE

/**
 * A function type for binary operators.
 * @param a      The first parameter.
 * @param b      The second parameter.
 * @param c      The return value.
 * @param scale  The current scale.
 */
typedef void (*BcNumBinaryOp)(BcNum* a, BcNum* b, BcNum* c, size_t scale);

/**
 * A function type for binary operators *after* @a c has been properly
 * allocated. At this point, *nothing* should be pointing to @a c (in any way
 * that matters, anyway).
 * @param a      The first operand.
 * @param b      The second operand.
 * @param c      The return parameter.
 * @param scale  The current scale.
 */
typedef void (*BcNumBinOp)(BcNum* a, BcNum* b, BcNum* restrict c, size_t scale);

/**
 * A function type for getting the allocation size needed for a binary operator.
 * Any function used for this *must* return enough space for *all* possible
 * invocations of the operator.
 * @param a      The first parameter.
 * @param b      The second parameter.
 * @param scale  The current scale.
 * @return       The size of allocation needed for the result of the operator
 *               with @a a, @a b, and @a scale.
 */
typedef size_t (*BcNumBinaryOpReq)(const BcNum* a, const BcNum* b,
                                   size_t scale);

/**
 * A function type for printing a "digit." Functions of this type will print one
 * digit in a number. Digits are printed differently based on the base, which is
 * why there is more than one implementation of this function type.
 * @param n       The "digit" to print.
 * @param len     The "length" of the digit, or number of characters that will
 *                need to be printed for the digit.
 * @param rdx     True if a decimal (radix) point should be printed.
 * @param bslash  True if a backslash+newline should be printed if the character
 *                limit for the line is reached, false otherwise.
 */
typedef void (*BcNumDigitOp)(size_t n, size_t len, bool rdx, bool bslash);

/**
 * A function type to run an operator on @a a and @a b and store the result in
 * @a a. This is used in karatsuba for faster adds and subtracts at the end.
 * @param a    The first parameter and return value.
 * @param b    The second parameter.
 * @param len  The minimum length of both arrays.
 */
typedef void (*BcNumShiftAddOp)(BcDig* restrict a, const BcDig* restrict b,
                                size_t len);

/**
 * Initializes @a n with @a req limbs in its array.
 * @param n    The number to initialize.
 * @param req  The number of limbs @a n must have in its limb array.
 */
void
bc_num_init(BcNum* restrict n, size_t req);

/**
 * Initializes (sets up) @a n with the preallocated limb array @a num that has
 * size @a cap. This is called by @a bc_num_init(), but it is also used by parts
 * of bc that use statically allocated limb arrays.
 * @param n    The number to initialize.
 * @param num  The preallocated limb array.
 * @param cap  The capacity of @a num.
 */
void
bc_num_setup(BcNum* restrict n, BcDig* restrict num, size_t cap);

/**
 * Copies @a s into @a d. This does a deep copy and requires that @a d is
 * already a valid and allocated BcNum.
 * @param d  The destination BcNum.
 * @param s  The source BcNum.
 */
void
bc_num_copy(BcNum* d, const BcNum* s);

/**
 * Creates @a d and copies @a s into @a d. This does a deep copy and requires
 * that @a d is *not* a valid or allocated BcNum.
 * @param d  The destination BcNum.
 * @param s  The source BcNum.
 */
void
bc_num_createCopy(BcNum* d, const BcNum* s);

/**
 * Creates (initializes) @a n and sets its value to the equivalent of @a val.
 * @a n must *not* be a valid or preallocated BcNum.
 * @param n    The number to initialize and set.
 * @param val  The value to set @a n's value to.
 */
void
bc_num_createFromBigdig(BcNum* restrict n, BcBigDig val);

/**
 * Makes @a n valid for holding strings. @a n must *not* be allocated; this
 * simply clears some fields, including setting the num field to NULL.
 * @param n  The number to clear.
 */
void
bc_num_clear(BcNum* restrict n);

/**
 * Frees @a num, which is a BcNum as a void pointer. This is a destructor.
 * @param num  The BcNum to free as a void pointer.
 */
void
bc_num_free(void* num);

/**
 * Returns the scale of @a n.
 * @param n  The number.
 * @return   The scale of @a n.
 */
size_t
bc_num_scale(const BcNum* restrict n);

/**
 * Returns the length (in decimal digits) of @a n. This is complicated. First,
 * if the number is zero, we always return at least one, but we also return the
 * scale if it exists. Then, If it is not zero, it opens a whole other can of
 * worms. Read the comments in the definition.
 * @param n  The number.
 * @return   The length of @a n.
 */
size_t
bc_num_len(const BcNum* restrict n);

/**
 * Convert a number to a BcBigDig (hardware integer). This version does error
 * checking, and if it finds an error, throws it. Otherwise, it calls
 * bc_num_bigdig2().
 * @param n  The number to convert.
 * @return   The number as a hardware integer.
 */
BcBigDig
bc_num_bigdig(const BcNum* restrict n);

/**
 * Convert a number to a BcBigDig (hardware integer). This version does no error
 * checking.
 * @param n  The number to convert.
 * @return   The number as a hardware integer.
 */
BcBigDig
bc_num_bigdig2(const BcNum* restrict n);

/**
 * Sets @a n to the value of @a val. @a n is expected to be a valid and
 * allocated BcNum.
 * @param n    The number to set.
 * @param val  The value to set the number to.
 */
void
bc_num_bigdig2num(BcNum* restrict n, BcBigDig val);

#if BC_ENABLE_EXTRA_MATH

/**
 * Generates a random arbitrary-size integer less than or equal to @a a and
 * returns it in @a b. This implements irand().
 * @param a    The limit for the integer to generate.
 * @param b    The return value.
 * @param rng  The pseudo-random number generator.
 */
void
bc_num_irand(BcNum* restrict a, BcNum* restrict b, struct BcRNG* restrict rng);

/**
 * Sets the seed for the PRNG @a rng from @a n.
 * @param n    The new seed for the PRNG.
 * @param rng  The PRNG to set the seed for.
 */
void
bc_num_rng(const BcNum* restrict n, struct BcRNG* rng);

/**
 * Sets @a n to the value produced by the PRNG. This implements rand().
 * @param n    The number to set.
 * @param rng  The pseudo-random number generator.
 */
void
bc_num_createFromRNG(BcNum* restrict n, struct BcRNG* rng);

#endif // BC_ENABLE_EXTRA_MATH

/**
 * The add function. This is a BcNumBinaryOp function.
 * @param a      The first parameter.
 * @param b      The second parameter.
 * @param c      The return value.
 * @param scale  The current scale.
 */
void
bc_num_add(BcNum* a, BcNum* b, BcNum* c, size_t scale);

/**
 * The subtract function. This is a BcNumBinaryOp function.
 * @param a      The first parameter.
 * @param b      The second parameter.
 * @param c      The return value.
 * @param scale  The current scale.
 */
void
bc_num_sub(BcNum* a, BcNum* b, BcNum* c, size_t scale);

/**
 * The multiply function.
 * @param a      The first parameter. This is a BcNumBinaryOp function.
 * @param b      The second parameter.
 * @param c      The return value.
 * @param scale  The current scale.
 */
void
bc_num_mul(BcNum* a, BcNum* b, BcNum* c, size_t scale);

/**
 * The division function.
 * @param a      The first parameter. This is a BcNumBinaryOp function.
 * @param b      The second parameter.
 * @param c      The return value.
 * @param scale  The current scale.
 */
void
bc_num_div(BcNum* a, BcNum* b, BcNum* c, size_t scale);

/**
 * The modulus function.
 * @param a      The first parameter. This is a BcNumBinaryOp function.
 * @param b      The second parameter.
 * @param c      The return value.
 * @param scale  The current scale.
 */
void
bc_num_mod(BcNum* a, BcNum* b, BcNum* c, size_t scale);

/**
 * The power function.
 * @param a      The first parameter. This is a BcNumBinaryOp function.
 * @param b      The second parameter.
 * @param c      The return value.
 * @param scale  The current scale.
 */
void
bc_num_pow(BcNum* a, BcNum* b, BcNum* c, size_t scale);
#if BC_ENABLE_EXTRA_MATH

/**
 * The places function (@ operator). This is a BcNumBinaryOp function.
 * @param a      The first parameter.
 * @param b      The second parameter.
 * @param c      The return value.
 * @param scale  The current scale.
 */
void
bc_num_places(BcNum* a, BcNum* b, BcNum* c, size_t scale);

/**
 * The left shift function (<< operator). This is a BcNumBinaryOp function.
 * @param a      The first parameter.
 * @param b      The second parameter.
 * @param c      The return value.
 * @param scale  The current scale.
 */
void
bc_num_lshift(BcNum* a, BcNum* b, BcNum* c, size_t scale);

/**
 * The right shift function (>> operator). This is a BcNumBinaryOp function.
 * @param a      The first parameter.
 * @param b      The second parameter.
 * @param c      The return value.
 * @param scale  The current scale.
 */
void
bc_num_rshift(BcNum* a, BcNum* b, BcNum* c, size_t scale);

#endif // BC_ENABLE_EXTRA_MATH

/**
 * Square root.
 * @param a      The first parameter.
 * @param b      The return value.
 * @param scale  The current scale.
 */
void
bc_num_sqrt(BcNum* restrict a, BcNum* restrict b, size_t scale);

/**
 * Divsion and modulus together. This is a dc extension.
 * @param a      The first parameter.
 * @param b      The second parameter.
 * @param c      The first return value (quotient).
 * @param d      The second return value (modulus).
 * @param scale  The current scale.
 */
void
bc_num_divmod(BcNum* a, BcNum* b, BcNum* c, BcNum* d, size_t scale);

/**
 * A function returning the required allocation size for an addition or a
 * subtraction. This is a BcNumBinaryOpReq function.
 * @param a      The first parameter.
 * @param b      The second parameter.
 * @param scale  The current scale.
 * @return       The size of allocation needed for the result of add or subtract
 *               with @a a, @a b, and @a scale.
 */
size_t
bc_num_addReq(const BcNum* a, const BcNum* b, size_t scale);

/**
 * A function returning the required allocation size for a multiplication. This
 * is a BcNumBinaryOpReq function.
 * @param a      The first parameter.
 * @param b      The second parameter.
 * @param scale  The current scale.
 * @return       The size of allocation needed for the result of multiplication
 *               with @a a, @a b, and @a scale.
 */
size_t
bc_num_mulReq(const BcNum* a, const BcNum* b, size_t scale);

/**
 * A function returning the required allocation size for a division or modulus.
 * This is a BcNumBinaryOpReq function.
 * @param a      The first parameter.
 * @param b      The second parameter.
 * @param scale  The current scale.
 * @return       The size of allocation needed for the result of division or
 *               modulus with @a a, @a b, and @a scale.
 */
size_t
bc_num_divReq(const BcNum* a, const BcNum* b, size_t scale);

/**
 * A function returning the required allocation size for an exponentiation. This
 * is a BcNumBinaryOpReq function.
 * @param a      The first parameter.
 * @param b      The second parameter.
 * @param scale  The current scale.
 * @return       The size of allocation needed for the result of exponentiation
 *               with @a a, @a b, and @a scale.
 */
size_t
bc_num_powReq(const BcNum* a, const BcNum* b, size_t scale);

#if BC_ENABLE_EXTRA_MATH

/**
 * A function returning the required allocation size for a places, left shift,
 * or right shift. This is a BcNumBinaryOpReq function.
 * @param a      The first parameter.
 * @param b      The second parameter.
 * @param scale  The current scale.
 * @return       The size of allocation needed for the result of places, left
 *               shift, or right shift with @a a, @a b, and @a scale.
 */
size_t
bc_num_placesReq(const BcNum* a, const BcNum* b, size_t scale);

#endif // BC_ENABLE_EXTRA_MATH

/**
 * Truncate @a n *by* @a places decimal places. This only extends places *after*
 * the decimal point.
 * @param n       The number to truncate.
 * @param places  The number of places to truncate @a n by.
 */
void
bc_num_truncate(BcNum* restrict n, size_t places);

/**
 * Extend @a n *by* @a places decimal places. This only extends places *after*
 * the decimal point.
 * @param n       The number to truncate.
 * @param places  The number of places to extend @a n by.
 */
void
bc_num_extend(BcNum* restrict n, size_t places);

/**
 * Shifts @a n right by @a places decimal places. This is the workhorse of the
 * right shift operator, and would be static to src/num.c, except that
 * src/library.c uses it for efficiency when executing its frand.
 * @param n       The number to shift right.
 * @param places  The number of decimal places to shift @a n right by.
 */
void
bc_num_shiftRight(BcNum* restrict n, size_t places);

/**
 * Compare a and b and return the result of their comparison as an ssize_t.
 * Returns >0 if @a a is greater than @a b, <0 if @a a is less than @a b, and =0
 * if a == b.
 * @param a  The first number.
 * @param b  The second number.
 * @return   The result of the comparison.
 */
ssize_t
bc_num_cmp(const BcNum* a, const BcNum* b);

/**
 * Modular exponentiation.
 * @param a      The first parameter.
 * @param b      The second parameter.
 * @param c      The third parameter.
 * @param d      The return value.
 */
void
bc_num_modexp(BcNum* a, BcNum* b, BcNum* c, BcNum* restrict d);

/**
 * Sets @a n to zero with a scale of zero.
 * @param n  The number to zero.
 */
void
bc_num_zero(BcNum* restrict n);

/**
 * Sets @a n to one with a scale of zero.
 * @param n  The number to set to one.
 */
void
bc_num_one(BcNum* restrict n);

/**
 * An efficient function to compare @a n to zero.
 * @param n  The number to compare to zero.
 * @return   The result of the comparison.
 */
ssize_t
bc_num_cmpZero(const BcNum* n);

/**
 * Check a number string for validity and return true if it is, false otherwise.
 * The library needs this to check user-supplied strings, but in bc and dc, this
 * is only used for debug asserts because the parsers should get the numbers
 * parsed right, which should ensure they are always valid.
 * @param val  The string to check.
 * @return     True if the string is a valid number, false otherwise.
 */
bool
bc_num_strValid(const char* restrict val);

/**
 * Parses a number string into the number @a n according to @a base.
 * @param n     The number to set to the parsed value.
 * @param val   The number string to parse.
 * @param base  The base to parse the number string by.
 */
void
bc_num_parse(BcNum* restrict n, const char* restrict val, BcBigDig base);

/**
 * Prints the number @a n according to @a base.
 * @param n        The number to print.
 * @param base     The base to print the number by.
 * @param newline  True if a newline should be inserted at the end, false
 *                 otherwise.
 */
void
bc_num_print(BcNum* restrict n, BcBigDig base, bool newline);

/**
 * Invert @a into @a b at the current scale.
 * @param a      The number to invert.
 * @param b      The return parameter. This must be preallocated.
 * @param scale  The current scale.
 */
#define bc_num_inv(a, b, scale) bc_num_div(&vm->one, (a), (b), (scale))

#if !BC_ENABLE_LIBRARY

/**
 * Prints a number as a character stream.
 * @param n     The number to print as a character stream.
 */
void
bc_num_stream(BcNum* restrict n);

#endif // !BC_ENABLE_LIBRARY

#if BC_DEBUG_CODE

/**
 * Print a number with a label. This is a debug-only function.
 * @param n          The number to print.
 * @param name       The label to print the number with.
 * @param emptyline  True if there should be an empty line after the number.
 */
void
bc_num_printDebug(const BcNum* n, const char* name, bool emptyline);

/**
 * Print the limbs of @a n. This is a debug-only function.
 * @param n          The number to print.
 * @param len        The length of the number.
 * @param emptyline  True if there should be an empty line after the number.
 */
void
bc_num_printDigs(const BcDig* n, size_t len, bool emptyline);

/**
 * Print debug info about @a n along with its limbs.
 * @param n          The number to print.
 * @param name       The label to print the number with.
 * @param emptyline  True if there should be an empty line after the number.
 */
void
bc_num_printWithDigs(const BcNum* n, const char* name, bool emptyline);

/**
 * Dump debug info about a BcNum variable.
 * @param varname  The variable name.
 * @param n        The number.
 */
void
bc_num_dump(const char* varname, const BcNum* n);

#endif // BC_DEBUG_CODE

/// A reference to an array of hex digits for easy conversion for printing.
extern const char bc_num_hex_digits[];

/// An array of powers of 10 for easy conversion from number of digits to
/// powers.
extern const BcBigDig bc_num_pow10[BC_BASE_DIGS + 1];

/// A reference to a constant array that is the max of a BigDig.
extern const BcDig bc_num_bigdigMax[];

/// A reference to a constant size of the above array.
extern const size_t bc_num_bigdigMax_size;

/// A reference to a constant array that is 2 times the max of a BigDig.
extern const BcDig bc_num_bigdigMax2[];

/// A reference to a constant size of the above array.
extern const size_t bc_num_bigdigMax2_size;

#endif // BC_NUM_H
