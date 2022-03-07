/*
 * *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2023 Gavin D. Howard and contributors.
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
 * Code for the number type.
 *
 */

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <limits.h>

#include <num.h>
#include <rand.h>
#include <vm.h>
#if BC_ENABLE_LIBRARY
#include <library.h>
#endif // BC_ENABLE_LIBRARY

// Before you try to understand this code, see the development manual
// (manuals/development.md#numbers).

static void
bc_num_m(BcNum* a, BcNum* b, BcNum* restrict c, size_t scale);

/**
 * Multiply two numbers and throw a math error if they overflow.
 * @param a  The first operand.
 * @param b  The second operand.
 * @return   The product of the two operands.
 */
static inline size_t
bc_num_mulOverflow(size_t a, size_t b)
{
	size_t res = a * b;
	if (BC_ERR(BC_VM_MUL_OVERFLOW(a, b, res))) bc_err(BC_ERR_MATH_OVERFLOW);
	return res;
}

/**
 * Conditionally negate @a n based on @a neg. Algorithm taken from
 * https://graphics.stanford.edu/~seander/bithacks.html#ConditionalNegate .
 * @param n    The value to turn into a signed value and negate.
 * @param neg  The condition to negate or not.
 */
static inline ssize_t
bc_num_neg(size_t n, bool neg)
{
	return (((ssize_t) n) ^ -((ssize_t) neg)) + neg;
}

/**
 * Compare a BcNum against zero.
 * @param n  The number to compare.
 * @return   -1 if the number is less than 0, 1 if greater, and 0 if equal.
 */
ssize_t
bc_num_cmpZero(const BcNum* n)
{
	return bc_num_neg((n)->len != 0, BC_NUM_NEG(n));
}

/**
 * Return the number of integer limbs in a BcNum. This is the opposite of rdx.
 * @param n  The number to return the amount of integer limbs for.
 * @return   The amount of integer limbs in @a n.
 */
static inline size_t
bc_num_int(const BcNum* n)
{
	return n->len ? n->len - BC_NUM_RDX_VAL(n) : 0;
}

/**
 * Expand a number's allocation capacity to at least req limbs.
 * @param n    The number to expand.
 * @param req  The number limbs to expand the allocation capacity to.
 */
static void
bc_num_expand(BcNum* restrict n, size_t req)
{
	assert(n != NULL);

	req = req >= BC_NUM_DEF_SIZE ? req : BC_NUM_DEF_SIZE;

	if (req > n->cap)
	{
		BC_SIG_LOCK;

		n->num = bc_vm_realloc(n->num, BC_NUM_SIZE(req));
		n->cap = req;

		BC_SIG_UNLOCK;
	}
}

/**
 * Set a number to 0 with the specified scale.
 * @param n      The number to set to zero.
 * @param scale  The scale to set the number to.
 */
static inline void
bc_num_setToZero(BcNum* restrict n, size_t scale)
{
	assert(n != NULL);
	n->scale = scale;
	n->len = n->rdx = 0;
}

void
bc_num_zero(BcNum* restrict n)
{
	bc_num_setToZero(n, 0);
}

void
bc_num_one(BcNum* restrict n)
{
	bc_num_zero(n);
	n->len = 1;
	n->num[0] = 1;
}

/**
 * "Cleans" a number, which means reducing the length if the most significant
 * limbs are zero.
 * @param n  The number to clean.
 */
static void
bc_num_clean(BcNum* restrict n)
{
	// Reduce the length.
	while (BC_NUM_NONZERO(n) && !n->num[n->len - 1])
	{
		n->len -= 1;
	}

	// Special cases.
	if (BC_NUM_ZERO(n)) n->rdx = 0;
	else
	{
		// len must be at least as much as rdx.
		size_t rdx = BC_NUM_RDX_VAL(n);
		if (n->len < rdx) n->len = rdx;
	}
}

/**
 * Returns the log base 10 of @a i. I could have done this with floating-point
 * math, and in fact, I originally did. However, that was the only
 * floating-point code in the entire codebase, and I decided I didn't want any.
 * This is fast enough. Also, it might handle larger numbers better.
 * @param i  The number to return the log base 10 of.
 * @return   The log base 10 of @a i.
 */
static size_t
bc_num_log10(size_t i)
{
	size_t len;

	for (len = 1; i; i /= BC_BASE, ++len)
	{
		continue;
	}

	assert(len - 1 <= BC_BASE_DIGS + 1);

	return len - 1;
}

/**
 * Returns the number of decimal digits in a limb that are zero starting at the
 * most significant digits. This basically returns how much of the limb is used.
 * @param n  The number.
 * @return   The number of decimal digits that are 0 starting at the most
 *           significant digits.
 */
static inline size_t
bc_num_zeroDigits(const BcDig* n)
{
	assert(*n >= 0);
	assert(((size_t) *n) < BC_BASE_POW);
	return BC_BASE_DIGS - bc_num_log10((size_t) *n);
}

/**
 * Return the total number of integer digits in a number. This is the opposite
 * of scale, like bc_num_int() is the opposite of rdx.
 * @param n  The number.
 * @return   The number of integer digits in @a n.
 */
static size_t
bc_num_intDigits(const BcNum* n)
{
	size_t digits = bc_num_int(n) * BC_BASE_DIGS;
	if (digits > 0) digits -= bc_num_zeroDigits(n->num + n->len - 1);
	return digits;
}

/**
 * Returns the number of limbs of a number that are non-zero starting at the
 * most significant limbs. This expects that there are *no* integer limbs in the
 * number because it is specifically to figure out how many zero limbs after the
 * decimal place to ignore. If there are zero limbs after non-zero limbs, they
 * are counted as non-zero limbs.
 * @param n  The number.
 * @return   The number of non-zero limbs after the decimal point.
 */
static size_t
bc_num_nonZeroLen(const BcNum* restrict n)
{
	size_t i, len = n->len;

	assert(len == BC_NUM_RDX_VAL(n));

	for (i = len - 1; i < len && !n->num[i]; --i)
	{
		continue;
	}

	assert(i + 1 > 0);

	return i + 1;
}

/**
 * Performs a one-limb add with a carry.
 * @param a      The first limb.
 * @param b      The second limb.
 * @param carry  An in/out parameter; the carry in from the previous add and the
 *               carry out from this add.
 * @return       The resulting limb sum.
 */
static BcDig
bc_num_addDigits(BcDig a, BcDig b, bool* carry)
{
	assert(((BcBigDig) BC_BASE_POW) * 2 == ((BcDig) BC_BASE_POW) * 2);
	assert(a < BC_BASE_POW && a >= 0);
	assert(b < BC_BASE_POW && b >= 0);

	a += b + *carry;
	*carry = (a >= BC_BASE_POW);
	if (*carry) a -= BC_BASE_POW;

	assert(a >= 0);
	assert(a < BC_BASE_POW);

	return a;
}

/**
 * Performs a one-limb subtract with a carry.
 * @param a      The first limb.
 * @param b      The second limb.
 * @param carry  An in/out parameter; the carry in from the previous subtract
 *               and the carry out from this subtract.
 * @return       The resulting limb difference.
 */
static BcDig
bc_num_subDigits(BcDig a, BcDig b, bool* carry)
{
	assert(a < BC_BASE_POW && a >= 0);
	assert(b < BC_BASE_POW && b >= 0);

	b += *carry;
	*carry = (a < b);
	if (*carry) a += BC_BASE_POW;

	assert(a - b >= 0);
	assert(a - b < BC_BASE_POW);

	return a - b;
}

/**
 * Add two BcDig arrays and store the result in the first array.
 * @param a    The first operand and out array.
 * @param b    The second operand.
 * @param len  The length of @a b.
 */
static void
bc_num_addArrays(BcDig* restrict a, const BcDig* restrict b, size_t len)
{
	size_t i;
	bool carry = false;

	for (i = 0; i < len; ++i)
	{
		a[i] = bc_num_addDigits(a[i], b[i], &carry);
	}

	// Take care of the extra limbs in the bigger array.
	for (; carry; ++i)
	{
		a[i] = bc_num_addDigits(a[i], 0, &carry);
	}
}

/**
 * Subtract two BcDig arrays and store the result in the first array.
 * @param a    The first operand and out array.
 * @param b    The second operand.
 * @param len  The length of @a b.
 */
static void
bc_num_subArrays(BcDig* restrict a, const BcDig* restrict b, size_t len)
{
	size_t i;
	bool carry = false;

	for (i = 0; i < len; ++i)
	{
		a[i] = bc_num_subDigits(a[i], b[i], &carry);
	}

	// Take care of the extra limbs in the bigger array.
	for (; carry; ++i)
	{
		a[i] = bc_num_subDigits(a[i], 0, &carry);
	}
}

/**
 * Multiply a BcNum array by a one-limb number. This is a faster version of
 * multiplication for when we can use it.
 * @param a  The BcNum to multiply by the one-limb number.
 * @param b  The one limb of the one-limb number.
 * @param c  The return parameter.
 */
static void
bc_num_mulArray(const BcNum* restrict a, BcBigDig b, BcNum* restrict c)
{
	size_t i;
	BcBigDig carry = 0;

	assert(b <= BC_BASE_POW);

	// Make sure the return parameter is big enough.
	if (a->len + 1 > c->cap) bc_num_expand(c, a->len + 1);

	// We want the entire return parameter to be zero for cleaning later.
	// NOLINTNEXTLINE
	memset(c->num, 0, BC_NUM_SIZE(c->cap));

	// Actual multiplication loop.
	for (i = 0; i < a->len; ++i)
	{
		BcBigDig in = ((BcBigDig) a->num[i]) * b + carry;
		c->num[i] = in % BC_BASE_POW;
		carry = in / BC_BASE_POW;
	}

	assert(carry < BC_BASE_POW);

	// Finishing touches.
	c->num[i] = (BcDig) carry;
	assert(c->num[i] >= 0 && c->num[i] < BC_BASE_POW);
	c->len = a->len;
	c->len += (carry != 0);

	bc_num_clean(c);

	// Postconditions.
	assert(!BC_NUM_NEG(c) || BC_NUM_NONZERO(c));
	assert(BC_NUM_RDX_VAL(c) <= c->len || !c->len);
	assert(!c->len || c->num[c->len - 1] || BC_NUM_RDX_VAL(c) == c->len);
}

/**
 * Divide a BcNum array by a one-limb number. This is a faster version of divide
 * for when we can use it.
 * @param a    The BcNum to multiply by the one-limb number.
 * @param b    The one limb of the one-limb number.
 * @param c    The return parameter for the quotient.
 * @param rem  The return parameter for the remainder.
 */
static void
bc_num_divArray(const BcNum* restrict a, BcBigDig b, BcNum* restrict c,
                BcBigDig* rem)
{
	size_t i;
	BcBigDig carry = 0;

	assert(c->cap >= a->len);

	// Actual division loop.
	for (i = a->len - 1; i < a->len; --i)
	{
		BcBigDig in = ((BcBigDig) a->num[i]) + carry * BC_BASE_POW;
		assert(in / b < BC_BASE_POW);
		c->num[i] = (BcDig) (in / b);
		assert(c->num[i] >= 0 && c->num[i] < BC_BASE_POW);
		carry = in % b;
	}

	// Finishing touches.
	c->len = a->len;
	bc_num_clean(c);
	*rem = carry;

	// Postconditions.
	assert(!BC_NUM_NEG(c) || BC_NUM_NONZERO(c));
	assert(BC_NUM_RDX_VAL(c) <= c->len || !c->len);
	assert(!c->len || c->num[c->len - 1] || BC_NUM_RDX_VAL(c) == c->len);
}

/**
 * Compare two BcDig arrays and return >0 if @a b is greater, <0 if @a b is
 * less, and 0 if equal. Both @a a and @a b must have the same length.
 * @param a    The first array.
 * @param b    The second array.
 * @param len  The minimum length of the arrays.
 */
static ssize_t
bc_num_compare(const BcDig* restrict a, const BcDig* restrict b, size_t len)
{
	size_t i;
	BcDig c = 0;
	for (i = len - 1; i < len && !(c = a[i] - b[i]); --i)
	{
		continue;
	}
	return bc_num_neg(i + 1, c < 0);
}

ssize_t
bc_num_cmp(const BcNum* a, const BcNum* b)
{
	size_t i, min, a_int, b_int, diff, ardx, brdx;
	BcDig* max_num;
	BcDig* min_num;
	bool a_max, neg = false;
	ssize_t cmp;

	assert(a != NULL && b != NULL);

	// Same num? Equal.
	if (a == b) return 0;

	// Easy cases.
	if (BC_NUM_ZERO(a)) return bc_num_neg(b->len != 0, !BC_NUM_NEG(b));
	if (BC_NUM_ZERO(b)) return bc_num_cmpZero(a);
	if (BC_NUM_NEG(a))
	{
		if (BC_NUM_NEG(b)) neg = true;
		else return -1;
	}
	else if (BC_NUM_NEG(b)) return 1;

	// Get the number of int limbs in each number and get the difference.
	a_int = bc_num_int(a);
	b_int = bc_num_int(b);
	a_int -= b_int;

	// If there's a difference, then just return the comparison.
	if (a_int) return neg ? -((ssize_t) a_int) : (ssize_t) a_int;

	// Get the rdx's and figure out the max.
	ardx = BC_NUM_RDX_VAL(a);
	brdx = BC_NUM_RDX_VAL(b);
	a_max = (ardx > brdx);

	// Set variables based on the above.
	if (a_max)
	{
		min = brdx;
		diff = ardx - brdx;
		max_num = a->num + diff;
		min_num = b->num;
	}
	else
	{
		min = ardx;
		diff = brdx - ardx;
		max_num = b->num + diff;
		min_num = a->num;
	}

	// Do a full limb-by-limb comparison.
	cmp = bc_num_compare(max_num, min_num, b_int + min);

	// If we found a difference, return it based on state.
	if (cmp) return bc_num_neg((size_t) cmp, !a_max == !neg);

	// If there was no difference, then the final step is to check which number
	// has greater or lesser limbs beyond the other's.
	for (max_num -= diff, i = diff - 1; i < diff; --i)
	{
		if (max_num[i]) return bc_num_neg(1, !a_max == !neg);
	}

	return 0;
}

void
bc_num_truncate(BcNum* restrict n, size_t places)
{
	size_t nrdx, places_rdx;

	if (!places) return;

	// Grab these needed values; places_rdx is the rdx equivalent to places like
	// rdx is to scale.
	nrdx = BC_NUM_RDX_VAL(n);
	places_rdx = nrdx ? nrdx - BC_NUM_RDX(n->scale - places) : 0;

	// We cannot truncate more places than we have.
	assert(places <= n->scale && (BC_NUM_ZERO(n) || places_rdx <= n->len));

	n->scale -= places;
	BC_NUM_RDX_SET(n, nrdx - places_rdx);

	// Only when the number is nonzero do we need to do the hard stuff.
	if (BC_NUM_NONZERO(n))
	{
		size_t pow;

		// This calculates how many decimal digits are in the least significant
		// limb.
		pow = n->scale % BC_BASE_DIGS;
		pow = pow ? BC_BASE_DIGS - pow : 0;
		pow = bc_num_pow10[pow];

		n->len -= places_rdx;

		// We have to move limbs to maintain invariants. The limbs must begin at
		// the beginning of the BcNum array.
		// NOLINTNEXTLINE
		memmove(n->num, n->num + places_rdx, BC_NUM_SIZE(n->len));

		// Clear the lower part of the last digit.
		if (BC_NUM_NONZERO(n)) n->num[0] -= n->num[0] % (BcDig) pow;

		bc_num_clean(n);
	}
}

void
bc_num_extend(BcNum* restrict n, size_t places)
{
	size_t nrdx, places_rdx;

	if (!places) return;

	// Easy case with zero; set the scale.
	if (BC_NUM_ZERO(n))
	{
		n->scale += places;
		return;
	}

	// Grab these needed values; places_rdx is the rdx equivalent to places like
	// rdx is to scale.
	nrdx = BC_NUM_RDX_VAL(n);
	places_rdx = BC_NUM_RDX(places + n->scale) - nrdx;

	// This is the hard case. We need to expand the number, move the limbs, and
	// set the limbs that were just cleared.
	if (places_rdx)
	{
		bc_num_expand(n, bc_vm_growSize(n->len, places_rdx));
		// NOLINTNEXTLINE
		memmove(n->num + places_rdx, n->num, BC_NUM_SIZE(n->len));
		// NOLINTNEXTLINE
		memset(n->num, 0, BC_NUM_SIZE(places_rdx));
	}

	// Finally, set scale and rdx.
	BC_NUM_RDX_SET(n, nrdx + places_rdx);
	n->scale += places;
	n->len += places_rdx;

	assert(BC_NUM_RDX_VAL(n) == BC_NUM_RDX(n->scale));
}

/**
 * Retires (finishes) a multiplication or division operation.
 */
static void
bc_num_retireMul(BcNum* restrict n, size_t scale, bool neg1, bool neg2)
{
	// Make sure scale is correct.
	if (n->scale < scale) bc_num_extend(n, scale - n->scale);
	else bc_num_truncate(n, n->scale - scale);

	bc_num_clean(n);

	// We need to ensure rdx is correct.
	if (BC_NUM_NONZERO(n)) n->rdx = BC_NUM_NEG_VAL(n, !neg1 != !neg2);
}

/**
 * Splits a number into two BcNum's. This is used in Karatsuba.
 * @param n    The number to split.
 * @param idx  The index to split at.
 * @param a    An out parameter; the low part of @a n.
 * @param b    An out parameter; the high part of @a n.
 */
static void
bc_num_split(const BcNum* restrict n, size_t idx, BcNum* restrict a,
             BcNum* restrict b)
{
	// We want a and b to be clear.
	assert(BC_NUM_ZERO(a));
	assert(BC_NUM_ZERO(b));

	// The usual case.
	if (idx < n->len)
	{
		// Set the fields first.
		b->len = n->len - idx;
		a->len = idx;
		a->scale = b->scale = 0;
		BC_NUM_RDX_SET(a, 0);
		BC_NUM_RDX_SET(b, 0);

		assert(a->cap >= a->len);
		assert(b->cap >= b->len);

		// Copy the arrays. This is not necessary for safety, but it is faster,
		// for some reason.
		// NOLINTNEXTLINE
		memcpy(b->num, n->num + idx, BC_NUM_SIZE(b->len));
		// NOLINTNEXTLINE
		memcpy(a->num, n->num, BC_NUM_SIZE(idx));

		bc_num_clean(b);
	}
	// If the index is weird, just skip the split.
	else bc_num_copy(a, n);

	bc_num_clean(a);
}

/**
 * Copies a number into another, but shifts the rdx so that the result number
 * only sees the integer part of the source number.
 * @param n  The number to copy.
 * @param r  The result number with a shifted rdx, len, and num.
 */
static void
bc_num_shiftRdx(const BcNum* restrict n, BcNum* restrict r)
{
	size_t rdx = BC_NUM_RDX_VAL(n);

	r->len = n->len - rdx;
	r->cap = n->cap - rdx;
	r->num = n->num + rdx;

	BC_NUM_RDX_SET_NEG(r, 0, BC_NUM_NEG(n));
	r->scale = 0;
}

/**
 * Shifts a number so that all of the least significant limbs of the number are
 * skipped. This must be undone by bc_num_unshiftZero().
 * @param n  The number to shift.
 */
static size_t
bc_num_shiftZero(BcNum* restrict n)
{
	// This is volatile to quiet a GCC warning about longjmp() clobbering.
	volatile size_t i;

	// If we don't have an integer, that is a problem, but it's also a bug
	// because the caller should have set everything up right.
	assert(!BC_NUM_RDX_VAL(n) || BC_NUM_ZERO(n));

	for (i = 0; i < n->len && !n->num[i]; ++i)
	{
		continue;
	}

	n->len -= i;
	n->num += i;

	return i;
}

/**
 * Undo the damage done by bc_num_unshiftZero(). This must be called like all
 * cleanup functions: after a label used by setjmp() and longjmp().
 * @param n           The number to unshift.
 * @param places_rdx  The amount the number was originally shift.
 */
static void
bc_num_unshiftZero(BcNum* restrict n, size_t places_rdx)
{
	n->len += places_rdx;
	n->num -= places_rdx;
}

/**
 * Shifts the digits right within a number by no more than BC_BASE_DIGS - 1.
 * This is the final step on shifting numbers with the shift operators. It
 * depends on the caller to shift the limbs properly because all it does is
 * ensure that the rdx point is realigned to be between limbs.
 * @param n    The number to shift digits in.
 * @param dig  The number of places to shift right.
 */
static void
bc_num_shift(BcNum* restrict n, BcBigDig dig)
{
	size_t i, len = n->len;
	BcBigDig carry = 0, pow;
	BcDig* ptr = n->num;

	assert(dig < BC_BASE_DIGS);

	// Figure out the parameters for division.
	pow = bc_num_pow10[dig];
	dig = bc_num_pow10[BC_BASE_DIGS - dig];

	// Run a series of divisions and mods with carries across the entire number
	// array. This effectively shifts everything over.
	for (i = len - 1; i < len; --i)
	{
		BcBigDig in, temp;
		in = ((BcBigDig) ptr[i]);
		temp = carry * dig;
		carry = in % pow;
		ptr[i] = ((BcDig) (in / pow)) + (BcDig) temp;
		assert(ptr[i] >= 0 && ptr[i] < BC_BASE_POW);
	}

	assert(!carry);
}

/**
 * Shift a number left by a certain number of places. This is the workhorse of
 * the left shift operator.
 * @param n       The number to shift left.
 * @param places  The amount of places to shift @a n left by.
 */
static void
bc_num_shiftLeft(BcNum* restrict n, size_t places)
{
	BcBigDig dig;
	size_t places_rdx;
	bool shift;

	if (!places) return;

	// Make sure to grow the number if necessary.
	if (places > n->scale)
	{
		size_t size = bc_vm_growSize(BC_NUM_RDX(places - n->scale), n->len);
		if (size > SIZE_MAX - 1) bc_err(BC_ERR_MATH_OVERFLOW);
	}

	// If zero, we can just set the scale and bail.
	if (BC_NUM_ZERO(n))
	{
		if (n->scale >= places) n->scale -= places;
		else n->scale = 0;
		return;
	}

	// When I changed bc to have multiple digits per limb, this was the hardest
	// code to change. This and shift right. Make sure you understand this
	// before attempting anything.

	// This is how many limbs we will shift.
	dig = (BcBigDig) (places % BC_BASE_DIGS);
	shift = (dig != 0);

	// Convert places to a rdx value.
	places_rdx = BC_NUM_RDX(places);

	// If the number is not an integer, we need special care. The reason an
	// integer doesn't is because left shift would only extend the integer,
	// whereas a non-integer might have its fractional part eliminated or only
	// partially eliminated.
	if (n->scale)
	{
		size_t nrdx = BC_NUM_RDX_VAL(n);

		// If the number's rdx is bigger, that's the hard case.
		if (nrdx >= places_rdx)
		{
			size_t mod = n->scale % BC_BASE_DIGS, revdig;

			// We want mod to be in the range [1, BC_BASE_DIGS], not
			// [0, BC_BASE_DIGS).
			mod = mod ? mod : BC_BASE_DIGS;

			// We need to reverse dig to get the actual number of digits.
			revdig = dig ? BC_BASE_DIGS - dig : 0;

			// If the two overflow BC_BASE_DIGS, we need to move an extra place.
			if (mod + revdig > BC_BASE_DIGS) places_rdx = 1;
			else places_rdx = 0;
		}
		else places_rdx -= nrdx;
	}

	// If this is non-zero, we need an extra place, so expand, move, and set.
	if (places_rdx)
	{
		bc_num_expand(n, bc_vm_growSize(n->len, places_rdx));
		// NOLINTNEXTLINE
		memmove(n->num + places_rdx, n->num, BC_NUM_SIZE(n->len));
		// NOLINTNEXTLINE
		memset(n->num, 0, BC_NUM_SIZE(places_rdx));
		n->len += places_rdx;
	}

	// Set the scale appropriately.
	if (places > n->scale)
	{
		n->scale = 0;
		BC_NUM_RDX_SET(n, 0);
	}
	else
	{
		n->scale -= places;
		BC_NUM_RDX_SET(n, BC_NUM_RDX(n->scale));
	}

	// Finally, shift within limbs.
	if (shift) bc_num_shift(n, BC_BASE_DIGS - dig);

	bc_num_clean(n);
}

void
bc_num_shiftRight(BcNum* restrict n, size_t places)
{
	BcBigDig dig;
	size_t places_rdx, scale, scale_mod, int_len, expand;
	bool shift;

	if (!places) return;

	// If zero, we can just set the scale and bail.
	if (BC_NUM_ZERO(n))
	{
		n->scale += places;
		bc_num_expand(n, BC_NUM_RDX(n->scale));
		return;
	}

	// Amount within a limb we have to shift by.
	dig = (BcBigDig) (places % BC_BASE_DIGS);
	shift = (dig != 0);

	scale = n->scale;

	// Figure out how the scale is affected.
	scale_mod = scale % BC_BASE_DIGS;
	scale_mod = scale_mod ? scale_mod : BC_BASE_DIGS;

	// We need to know the int length and rdx for places.
	int_len = bc_num_int(n);
	places_rdx = BC_NUM_RDX(places);

	// If we are going to shift past a limb boundary or not, set accordingly.
	if (scale_mod + dig > BC_BASE_DIGS)
	{
		expand = places_rdx - 1;
		places_rdx = 1;
	}
	else
	{
		expand = places_rdx;
		places_rdx = 0;
	}

	// Clamp expanding.
	if (expand > int_len) expand -= int_len;
	else expand = 0;

	// Extend, expand, and zero.
	bc_num_extend(n, places_rdx * BC_BASE_DIGS);
	bc_num_expand(n, bc_vm_growSize(expand, n->len));
	// NOLINTNEXTLINE
	memset(n->num + n->len, 0, BC_NUM_SIZE(expand));

	// Set the fields.
	n->len += expand;
	n->scale = 0;
	BC_NUM_RDX_SET(n, 0);

	// Finally, shift within limbs.
	if (shift) bc_num_shift(n, dig);

	n->scale = scale + places;
	BC_NUM_RDX_SET(n, BC_NUM_RDX(n->scale));

	bc_num_clean(n);

	assert(BC_NUM_RDX_VAL(n) <= n->len && n->len <= n->cap);
	assert(BC_NUM_RDX_VAL(n) == BC_NUM_RDX(n->scale));
}

/**
 * Tests if a number is a integer with scale or not. Returns true if the number
 * is not an integer. If it is, its integer shifted form is copied into the
 * result parameter for use where only integers are allowed.
 * @param n  The integer to test and shift.
 * @param r  The number to store the shifted result into. This number should
 *           *not* be allocated.
 * @return   True if the number is a non-integer, false otherwise.
 */
static bool
bc_num_nonInt(const BcNum* restrict n, BcNum* restrict r)
{
	bool zero;
	size_t i, rdx = BC_NUM_RDX_VAL(n);

	if (!rdx)
	{
		// NOLINTNEXTLINE
		memcpy(r, n, sizeof(BcNum));
		return false;
	}

	zero = true;

	for (i = 0; zero && i < rdx; ++i)
	{
		zero = (n->num[i] == 0);
	}

	if (BC_ERR(!zero)) return true;

	bc_num_shiftRdx(n, r);

	return false;
}

#if BC_ENABLE_EXTRA_MATH

/**
 * Execute common code for an operater that needs an integer for the second
 * operand and return the integer operand as a BcBigDig.
 * @param a  The first operand.
 * @param b  The second operand.
 * @param c  The result operand.
 * @return   The second operand as a hardware integer.
 */
static BcBigDig
bc_num_intop(const BcNum* a, const BcNum* b, BcNum* restrict c)
{
	BcNum temp;

#if BC_GCC
	temp.len = 0;
	temp.rdx = 0;
	temp.num = NULL;
#endif // BC_GCC

	if (BC_ERR(bc_num_nonInt(b, &temp))) bc_err(BC_ERR_MATH_NON_INTEGER);

	bc_num_copy(c, a);

	return bc_num_bigdig(&temp);
}
#endif // BC_ENABLE_EXTRA_MATH

/**
 * This is the actual implementation of add *and* subtract. Since this function
 * doesn't need to use scale (per the bc spec), I am hijacking it to say whether
 * it's doing an add or a subtract. And then I convert substraction to addition
 * of negative second operand. This is a BcNumBinOp function.
 * @param a    The first operand.
 * @param b    The second operand.
 * @param c    The return parameter.
 * @param sub  Non-zero for a subtract, zero for an add.
 */
static void
bc_num_as(BcNum* a, BcNum* b, BcNum* restrict c, size_t sub)
{
	BcDig* ptr_c;
	BcDig* ptr_l;
	BcDig* ptr_r;
	size_t i, min_rdx, max_rdx, diff, a_int, b_int, min_len, max_len, max_int;
	size_t len_l, len_r, ardx, brdx;
	bool b_neg, do_sub, do_rev_sub, carry, c_neg;

	if (BC_NUM_ZERO(b))
	{
		bc_num_copy(c, a);
		return;
	}

	if (BC_NUM_ZERO(a))
	{
		bc_num_copy(c, b);
		c->rdx = BC_NUM_NEG_VAL(c, BC_NUM_NEG(b) != sub);
		return;
	}

	// Invert sign of b if it is to be subtracted. This operation must
	// precede the tests for any of the operands being zero.
	b_neg = (BC_NUM_NEG(b) != sub);

	// Figure out if we will actually add the numbers if their signs are equal
	// or subtract.
	do_sub = (BC_NUM_NEG(a) != b_neg);

	a_int = bc_num_int(a);
	b_int = bc_num_int(b);
	max_int = BC_MAX(a_int, b_int);

	// Figure out which number will have its last limbs copied (for addition) or
	// subtracted (for subtraction).
	ardx = BC_NUM_RDX_VAL(a);
	brdx = BC_NUM_RDX_VAL(b);
	min_rdx = BC_MIN(ardx, brdx);
	max_rdx = BC_MAX(ardx, brdx);
	diff = max_rdx - min_rdx;

	max_len = max_int + max_rdx;

	if (do_sub)
	{
		// Check whether b has to be subtracted from a or a from b.
		if (a_int != b_int) do_rev_sub = (a_int < b_int);
		else if (ardx > brdx)
		{
			do_rev_sub = (bc_num_compare(a->num + diff, b->num, b->len) < 0);
		}
		else do_rev_sub = (bc_num_compare(a->num, b->num + diff, a->len) <= 0);
	}
	else
	{
		// The result array of the addition might come out one element
		// longer than the bigger of the operand arrays.
		max_len += 1;
		do_rev_sub = (a_int < b_int);
	}

	assert(max_len <= c->cap);

	// Cache values for simple code later.
	if (do_rev_sub)
	{
		ptr_l = b->num;
		ptr_r = a->num;
		len_l = b->len;
		len_r = a->len;
	}
	else
	{
		ptr_l = a->num;
		ptr_r = b->num;
		len_l = a->len;
		len_r = b->len;
	}

	ptr_c = c->num;
	carry = false;

	// This is true if the numbers have a different number of limbs after the
	// decimal point.
	if (diff)
	{
		// If the rdx values of the operands do not match, the result will
		// have low end elements that are the positive or negative trailing
		// elements of the operand with higher rdx value.
		if ((ardx > brdx) != do_rev_sub)
		{
			// !do_rev_sub && ardx > brdx || do_rev_sub && brdx > ardx
			// The left operand has BcDig values that need to be copied,
			// either from a or from b (in case of a reversed subtraction).
			// NOLINTNEXTLINE
			memcpy(ptr_c, ptr_l, BC_NUM_SIZE(diff));
			ptr_l += diff;
			len_l -= diff;
		}
		else
		{
			// The right operand has BcDig values that need to be copied
			// or subtracted from zero (in case of a subtraction).
			if (do_sub)
			{
				// do_sub (do_rev_sub && ardx > brdx ||
				// !do_rev_sub && brdx > ardx)
				for (i = 0; i < diff; i++)
				{
					ptr_c[i] = bc_num_subDigits(0, ptr_r[i], &carry);
				}
			}
			else
			{
				// !do_sub && brdx > ardx
				// NOLINTNEXTLINE
				memcpy(ptr_c, ptr_r, BC_NUM_SIZE(diff));
			}

			// Future code needs to ignore the limbs we just did.
			ptr_r += diff;
			len_r -= diff;
		}

		// The return value pointer needs to ignore what we just did.
		ptr_c += diff;
	}

	// This is the length that can be directly added/subtracted.
	min_len = BC_MIN(len_l, len_r);

	// After dealing with possible low array elements that depend on only one
	// operand above, the actual add or subtract can be performed as if the rdx
	// of both operands was the same.
	//
	// Inlining takes care of eliminating constant zero arguments to
	// addDigit/subDigit (checked in disassembly of resulting bc binary
	// compiled with gcc and clang).
	if (do_sub)
	{
		// Actual subtraction.
		for (i = 0; i < min_len; ++i)
		{
			ptr_c[i] = bc_num_subDigits(ptr_l[i], ptr_r[i], &carry);
		}

		// Finishing the limbs beyond the direct subtraction.
		for (; i < len_l; ++i)
		{
			ptr_c[i] = bc_num_subDigits(ptr_l[i], 0, &carry);
		}
	}
	else
	{
		// Actual addition.
		for (i = 0; i < min_len; ++i)
		{
			ptr_c[i] = bc_num_addDigits(ptr_l[i], ptr_r[i], &carry);
		}

		// Finishing the limbs beyond the direct addition.
		for (; i < len_l; ++i)
		{
			ptr_c[i] = bc_num_addDigits(ptr_l[i], 0, &carry);
		}

		// Addition can create an extra limb. We take care of that here.
		ptr_c[i] = bc_num_addDigits(0, 0, &carry);
	}

	assert(carry == false);

	// The result has the same sign as a, unless the operation was a
	// reverse subtraction (b - a).
	c_neg = BC_NUM_NEG(a) != (do_sub && do_rev_sub);
	BC_NUM_RDX_SET_NEG(c, max_rdx, c_neg);
	c->len = max_len;
	c->scale = BC_MAX(a->scale, b->scale);

	bc_num_clean(c);
}

/**
 * The simple multiplication that karatsuba dishes out to when the length of the
 * numbers gets low enough. This doesn't use scale because it treats the
 * operands as though they are integers.
 * @param a  The first operand.
 * @param b  The second operand.
 * @param c  The return parameter.
 */
static void
bc_num_m_simp(const BcNum* a, const BcNum* b, BcNum* restrict c)
{
	size_t i, alen = a->len, blen = b->len, clen;
	BcDig* ptr_a = a->num;
	BcDig* ptr_b = b->num;
	BcDig* ptr_c;
	BcBigDig sum = 0, carry = 0;

	assert(sizeof(sum) >= sizeof(BcDig) * 2);
	assert(!BC_NUM_RDX_VAL(a) && !BC_NUM_RDX_VAL(b));

	// Make sure c is big enough.
	clen = bc_vm_growSize(alen, blen);
	bc_num_expand(c, bc_vm_growSize(clen, 1));

	// If we don't memset, then we might have uninitialized data use later.
	ptr_c = c->num;
	// NOLINTNEXTLINE
	memset(ptr_c, 0, BC_NUM_SIZE(c->cap));

	// This is the actual multiplication loop. It uses the lattice form of long
	// multiplication (see the explanation on the web page at
	// https://knilt.arcc.albany.edu/What_is_Lattice_Multiplication or the
	// explanation at Wikipedia).
	for (i = 0; i < clen; ++i)
	{
		ssize_t sidx = (ssize_t) (i - blen + 1);
		size_t j, k;

		// These are the start indices.
		j = (size_t) BC_MAX(0, sidx);
		k = BC_MIN(i, blen - 1);

		// On every iteration of this loop, a multiplication happens, then the
		// sum is automatically calculated.
		for (; j < alen && k < blen; ++j, --k)
		{
			sum += ((BcBigDig) ptr_a[j]) * ((BcBigDig) ptr_b[k]);

			if (sum >= ((BcBigDig) BC_BASE_POW) * BC_BASE_POW)
			{
				carry += sum / BC_BASE_POW;
				sum %= BC_BASE_POW;
			}
		}

		// Calculate the carry.
		if (sum >= BC_BASE_POW)
		{
			carry += sum / BC_BASE_POW;
			sum %= BC_BASE_POW;
		}

		// Store and set up for next iteration.
		ptr_c[i] = (BcDig) sum;
		assert(ptr_c[i] < BC_BASE_POW);
		sum = carry;
		carry = 0;
	}

	// This should always be true because there should be no carry on the last
	// digit; multiplication never goes above the sum of both lengths.
	assert(!sum);

	c->len = clen;
}

/**
 * Does a shifted add or subtract for Karatsuba below. This calls either
 * bc_num_addArrays() or bc_num_subArrays().
 * @param n      An in/out parameter; the first operand and return parameter.
 * @param a      The second operand.
 * @param shift  The amount to shift @a n by when adding/subtracting.
 * @param op     The function to call, either bc_num_addArrays() or
 *               bc_num_subArrays().
 */
static void
bc_num_shiftAddSub(BcNum* restrict n, const BcNum* restrict a, size_t shift,
                   BcNumShiftAddOp op)
{
	assert(n->len >= shift + a->len);
	assert(!BC_NUM_RDX_VAL(n) && !BC_NUM_RDX_VAL(a));
	op(n->num + shift, a->num, a->len);
}

/**
 * Implements the Karatsuba algorithm.
 */
static void
bc_num_k(const BcNum* a, const BcNum* b, BcNum* restrict c)
{
	size_t max, max2, total;
	BcNum l1, h1, l2, h2, m2, m1, z0, z1, z2, temp;
	BcDig* digs;
	BcDig* dig_ptr;
	BcNumShiftAddOp op;
	bool aone = BC_NUM_ONE(a);
#if BC_ENABLE_LIBRARY
	BcVm* vm = bcl_getspecific();
#endif // BC_ENABLE_LIBRARY

	assert(BC_NUM_ZERO(c));

	if (BC_NUM_ZERO(a) || BC_NUM_ZERO(b)) return;

	if (aone || BC_NUM_ONE(b))
	{
		bc_num_copy(c, aone ? b : a);
		if ((aone && BC_NUM_NEG(a)) || BC_NUM_NEG(b)) BC_NUM_NEG_TGL(c);
		return;
	}

	// Shell out to the simple algorithm with certain conditions.
	if (a->len < BC_NUM_KARATSUBA_LEN || b->len < BC_NUM_KARATSUBA_LEN)
	{
		bc_num_m_simp(a, b, c);
		return;
	}

	// We need to calculate the max size of the numbers that can result from the
	// operations.
	max = BC_MAX(a->len, b->len);
	max = BC_MAX(max, BC_NUM_DEF_SIZE);
	max2 = (max + 1) / 2;

	// Calculate the space needed for all of the temporary allocations. We do
	// this to just allocate once.
	total = bc_vm_arraySize(BC_NUM_KARATSUBA_ALLOCS, max);

	BC_SIG_LOCK;

	// Allocate space for all of the temporaries.
	digs = dig_ptr = bc_vm_malloc(BC_NUM_SIZE(total));

	// Set up the temporaries.
	bc_num_setup(&l1, dig_ptr, max);
	dig_ptr += max;
	bc_num_setup(&h1, dig_ptr, max);
	dig_ptr += max;
	bc_num_setup(&l2, dig_ptr, max);
	dig_ptr += max;
	bc_num_setup(&h2, dig_ptr, max);
	dig_ptr += max;
	bc_num_setup(&m1, dig_ptr, max);
	dig_ptr += max;
	bc_num_setup(&m2, dig_ptr, max);

	// Some temporaries need the ability to grow, so we allocate them
	// separately.
	max = bc_vm_growSize(max, 1);
	bc_num_init(&z0, max);
	bc_num_init(&z1, max);
	bc_num_init(&z2, max);
	max = bc_vm_growSize(max, max) + 1;
	bc_num_init(&temp, max);

	BC_SETJMP_LOCKED(vm, err);

	BC_SIG_UNLOCK;

	// First, set up c.
	bc_num_expand(c, max);
	c->len = max;
	// NOLINTNEXTLINE
	memset(c->num, 0, BC_NUM_SIZE(c->len));

	// Split the parameters.
	bc_num_split(a, max2, &l1, &h1);
	bc_num_split(b, max2, &l2, &h2);

	// Do the subtraction.
	bc_num_sub(&h1, &l1, &m1, 0);
	bc_num_sub(&l2, &h2, &m2, 0);

	// The if statements below are there for efficiency reasons. The best way to
	// understand them is to understand the Karatsuba algorithm because now that
	// the ollocations and splits are done, the algorithm is pretty
	// straightforward.

	if (BC_NUM_NONZERO(&h1) && BC_NUM_NONZERO(&h2))
	{
		assert(BC_NUM_RDX_VALID_NP(h1));
		assert(BC_NUM_RDX_VALID_NP(h2));

		bc_num_m(&h1, &h2, &z2, 0);
		bc_num_clean(&z2);

		bc_num_shiftAddSub(c, &z2, max2 * 2, bc_num_addArrays);
		bc_num_shiftAddSub(c, &z2, max2, bc_num_addArrays);
	}

	if (BC_NUM_NONZERO(&l1) && BC_NUM_NONZERO(&l2))
	{
		assert(BC_NUM_RDX_VALID_NP(l1));
		assert(BC_NUM_RDX_VALID_NP(l2));

		bc_num_m(&l1, &l2, &z0, 0);
		bc_num_clean(&z0);

		bc_num_shiftAddSub(c, &z0, max2, bc_num_addArrays);
		bc_num_shiftAddSub(c, &z0, 0, bc_num_addArrays);
	}

	if (BC_NUM_NONZERO(&m1) && BC_NUM_NONZERO(&m2))
	{
		assert(BC_NUM_RDX_VALID_NP(m1));
		assert(BC_NUM_RDX_VALID_NP(m1));

		bc_num_m(&m1, &m2, &z1, 0);
		bc_num_clean(&z1);

		op = (BC_NUM_NEG_NP(m1) != BC_NUM_NEG_NP(m2)) ?
		         bc_num_subArrays :
		         bc_num_addArrays;
		bc_num_shiftAddSub(c, &z1, max2, op);
	}

err:
	BC_SIG_MAYLOCK;
	free(digs);
	bc_num_free(&temp);
	bc_num_free(&z2);
	bc_num_free(&z1);
	bc_num_free(&z0);
	BC_LONGJMP_CONT(vm);
}

/**
 * Does checks for Karatsuba. It also changes things to ensure that the
 * Karatsuba and simple multiplication can treat the numbers as integers. This
 * is a BcNumBinOp function.
 * @param a      The first operand.
 * @param b      The second operand.
 * @param c      The return parameter.
 * @param scale  The current scale.
 */
static void
bc_num_m(BcNum* a, BcNum* b, BcNum* restrict c, size_t scale)
{
	BcNum cpa, cpb;
	size_t ascale, bscale, ardx, brdx, zero, len, rscale;
	// These are meant to quiet warnings on GCC about longjmp() clobbering.
	// The problem is real here.
	size_t scale1, scale2, realscale;
	// These are meant to quiet the GCC longjmp() clobbering, even though it
	// does not apply here.
	volatile size_t azero;
	volatile size_t bzero;
#if BC_ENABLE_LIBRARY
	BcVm* vm = bcl_getspecific();
#endif // BC_ENABLE_LIBRARY

	assert(BC_NUM_RDX_VALID(a));
	assert(BC_NUM_RDX_VALID(b));

	bc_num_zero(c);

	ascale = a->scale;
	bscale = b->scale;

	// This sets the final scale according to the bc spec.
	scale1 = BC_MAX(scale, ascale);
	scale2 = BC_MAX(scale1, bscale);
	rscale = ascale + bscale;
	realscale = BC_MIN(rscale, scale2);

	// If this condition is true, we can use bc_num_mulArray(), which would be
	// much faster.
	if ((a->len == 1 || b->len == 1) && !a->rdx && !b->rdx)
	{
		BcNum* operand;
		BcBigDig dig;

		// Set the correct operands.
		if (a->len == 1)
		{
			dig = (BcBigDig) a->num[0];
			operand = b;
		}
		else
		{
			dig = (BcBigDig) b->num[0];
			operand = a;
		}

		bc_num_mulArray(operand, dig, c);

		// Need to make sure the sign is correct.
		if (BC_NUM_NONZERO(c))
		{
			c->rdx = BC_NUM_NEG_VAL(c, BC_NUM_NEG(a) != BC_NUM_NEG(b));
		}

		return;
	}

	assert(BC_NUM_RDX_VALID(a));
	assert(BC_NUM_RDX_VALID(b));

	BC_SIG_LOCK;

	// We need copies because of all of the mutation needed to make Karatsuba
	// think the numbers are integers.
	bc_num_init(&cpa, a->len + BC_NUM_RDX_VAL(a));
	bc_num_init(&cpb, b->len + BC_NUM_RDX_VAL(b));

	BC_SETJMP_LOCKED(vm, init_err);

	BC_SIG_UNLOCK;

	bc_num_copy(&cpa, a);
	bc_num_copy(&cpb, b);

	assert(BC_NUM_RDX_VALID_NP(cpa));
	assert(BC_NUM_RDX_VALID_NP(cpb));

	BC_NUM_NEG_CLR_NP(cpa);
	BC_NUM_NEG_CLR_NP(cpb);

	assert(BC_NUM_RDX_VALID_NP(cpa));
	assert(BC_NUM_RDX_VALID_NP(cpb));

	// These are what makes them appear like integers.
	ardx = BC_NUM_RDX_VAL_NP(cpa) * BC_BASE_DIGS;
	bc_num_shiftLeft(&cpa, ardx);

	brdx = BC_NUM_RDX_VAL_NP(cpb) * BC_BASE_DIGS;
	bc_num_shiftLeft(&cpb, brdx);

	// We need to reset the jump here because azero and bzero are used in the
	// cleanup, and local variables are not guaranteed to be the same after a
	// jump.
	BC_SIG_LOCK;

	BC_UNSETJMP(vm);

	// We want to ignore zero limbs.
	azero = bc_num_shiftZero(&cpa);
	bzero = bc_num_shiftZero(&cpb);

	BC_SETJMP_LOCKED(vm, err);

	BC_SIG_UNLOCK;

	bc_num_clean(&cpa);
	bc_num_clean(&cpb);

	bc_num_k(&cpa, &cpb, c);

	// The return parameter needs to have its scale set. This is the start. It
	// also needs to be shifted by the same amount as a and b have limbs after
	// the decimal point.
	zero = bc_vm_growSize(azero, bzero);
	len = bc_vm_growSize(c->len, zero);

	bc_num_expand(c, len);

	// Shift c based on the limbs after the decimal point in a and b.
	bc_num_shiftLeft(c, (len - c->len) * BC_BASE_DIGS);
	bc_num_shiftRight(c, ardx + brdx);

	bc_num_retireMul(c, realscale, BC_NUM_NEG(a), BC_NUM_NEG(b));

err:
	BC_SIG_MAYLOCK;
	bc_num_unshiftZero(&cpb, bzero);
	bc_num_unshiftZero(&cpa, azero);
init_err:
	BC_SIG_MAYLOCK;
	bc_num_free(&cpb);
	bc_num_free(&cpa);
	BC_LONGJMP_CONT(vm);
}

/**
 * Returns true if the BcDig array has non-zero limbs, false otherwise.
 * @param a    The array to test.
 * @param len  The length of the array.
 * @return     True if @a has any non-zero limbs, false otherwise.
 */
static bool
bc_num_nonZeroDig(BcDig* restrict a, size_t len)
{
	size_t i;

	for (i = len - 1; i < len; --i)
	{
		if (a[i] != 0) return true;
	}

	return false;
}

/**
 * Compares a BcDig array against a BcNum. This is especially suited for
 * division. Returns >0 if @a a is greater than @a b, <0 if it is less, and =0
 * if they are equal.
 * @param a    The array.
 * @param b    The number.
 * @param len  The length to assume the arrays are. This is always less than the
 *             actual length because of how this is implemented.
 */
static ssize_t
bc_num_divCmp(const BcDig* a, const BcNum* b, size_t len)
{
	ssize_t cmp;

	if (b->len > len && a[len]) cmp = bc_num_compare(a, b->num, len + 1);
	else if (b->len <= len)
	{
		if (a[len]) cmp = 1;
		else cmp = bc_num_compare(a, b->num, len);
	}
	else cmp = -1;

	return cmp;
}

/**
 * Extends the two operands of a division by BC_BASE_DIGS minus the number of
 * digits in the divisor estimate. In other words, it is shifting the numbers in
 * order to force the divisor estimate to fill the limb.
 * @param a        The first operand.
 * @param b        The second operand.
 * @param divisor  The divisor estimate.
 */
static void
bc_num_divExtend(BcNum* restrict a, BcNum* restrict b, BcBigDig divisor)
{
	size_t pow;

	assert(divisor < BC_BASE_POW);

	pow = BC_BASE_DIGS - bc_num_log10((size_t) divisor);

	bc_num_shiftLeft(a, pow);
	bc_num_shiftLeft(b, pow);
}

/**
 * Actually does division. This is a rewrite of my original code by Stefan Esser
 * from FreeBSD.
 * @param a      The first operand.
 * @param b      The second operand.
 * @param c      The return parameter.
 * @param scale  The current scale.
 */
static void
bc_num_d_long(BcNum* restrict a, BcNum* restrict b, BcNum* restrict c,
              size_t scale)
{
	BcBigDig divisor;
	size_t i, rdx;
	// This is volatile and len 2 and reallen exist to quiet the GCC warning
	// about clobbering on longjmp(). This one is possible, I think.
	volatile size_t len;
	size_t len2, reallen;
	// This is volatile and realend exists to quiet the GCC warning about
	// clobbering on longjmp(). This one is possible, I think.
	volatile size_t end;
	size_t realend;
	BcNum cpb;
	// This is volatile and realnonzero exists to quiet the GCC warning about
	// clobbering on longjmp(). This one is possible, I think.
	volatile bool nonzero;
	bool realnonzero;
#if BC_ENABLE_LIBRARY
	BcVm* vm = bcl_getspecific();
#endif // BC_ENABLE_LIBRARY

	assert(b->len < a->len);

	len = b->len;
	end = a->len - len;

	assert(len >= 1);

	// This is a final time to make sure c is big enough and that its array is
	// properly zeroed.
	bc_num_expand(c, a->len);
	// NOLINTNEXTLINE
	memset(c->num, 0, c->cap * sizeof(BcDig));

	// Setup.
	BC_NUM_RDX_SET(c, BC_NUM_RDX_VAL(a));
	c->scale = a->scale;
	c->len = a->len;

	// This is pulling the most significant limb of b in order to establish a
	// good "estimate" for the actual divisor.
	divisor = (BcBigDig) b->num[len - 1];

	// The entire bit of code in this if statement is to tighten the estimate of
	// the divisor. The condition asks if b has any other non-zero limbs.
	if (len > 1 && bc_num_nonZeroDig(b->num, len - 1))
	{
		// This takes a little bit of understanding. The "10*BC_BASE_DIGS/6+1"
		// results in either 16 for 64-bit 9-digit limbs or 7 for 32-bit 4-digit
		// limbs. Then it shifts a 1 by that many, which in both cases, puts the
		// result above *half* of the max value a limb can store. Basically,
		// this quickly calculates if the divisor is greater than half the max
		// of a limb.
		nonzero = (divisor > 1 << ((10 * BC_BASE_DIGS) / 6 + 1));

		// If the divisor is *not* greater than half the limb...
		if (!nonzero)
		{
			// Extend the parameters by the number of missing digits in the
			// divisor.
			bc_num_divExtend(a, b, divisor);

			// Check bc_num_d(). In there, we grow a again and again. We do it
			// again here; we *always* want to be sure it is big enough.
			len2 = BC_MAX(a->len, b->len);
			bc_num_expand(a, len2 + 1);

			// Make a have a zero most significant limb to match the len.
			if (len2 + 1 > a->len) a->len = len2 + 1;

			// Grab the new divisor estimate, new because the shift has made it
			// different.
			reallen = b->len;
			realend = a->len - reallen;
			divisor = (BcBigDig) b->num[reallen - 1];

			realnonzero = bc_num_nonZeroDig(b->num, reallen - 1);
		}
		else
		{
			realend = end;
			realnonzero = nonzero;
		}
	}
	else
	{
		realend = end;
		realnonzero = false;
	}

	// If b has other nonzero limbs, we want the divisor to be one higher, so
	// that it is an upper bound.
	divisor += realnonzero;

	// Make sure c can fit the new length.
	bc_num_expand(c, a->len);
	// NOLINTNEXTLINE
	memset(c->num, 0, BC_NUM_SIZE(c->cap));

	assert(c->scale >= scale);
	rdx = BC_NUM_RDX_VAL(c) - BC_NUM_RDX(scale);

	BC_SIG_LOCK;

	bc_num_init(&cpb, len + 1);

	BC_SETJMP_LOCKED(vm, err);

	BC_SIG_UNLOCK;

	// This is the actual division loop.
	for (i = realend - 1; i < realend && i >= rdx && BC_NUM_NONZERO(a); --i)
	{
		ssize_t cmp;
		BcDig* n;
		BcBigDig result;

		n = a->num + i;
		assert(n >= a->num);
		result = 0;

		cmp = bc_num_divCmp(n, b, len);

		// This is true if n is greater than b, which means that division can
		// proceed, so this inner loop is the part that implements one instance
		// of the division.
		while (cmp >= 0)
		{
			BcBigDig n1, dividend, quotient;

			// These should be named obviously enough. Just imagine that it's a
			// division of one limb. Because that's what it is.
			n1 = (BcBigDig) n[len];
			dividend = n1 * BC_BASE_POW + (BcBigDig) n[len - 1];
			quotient = (dividend / divisor);

			// If this is true, then we can just subtract. Remember: setting
			// quotient to 1 is not bad because we already know that n is
			// greater than b.
			if (quotient <= 1)
			{
				quotient = 1;
				bc_num_subArrays(n, b->num, len);
			}
			else
			{
				assert(quotient <= BC_BASE_POW);

				// We need to multiply and subtract for a quotient above 1.
				bc_num_mulArray(b, (BcBigDig) quotient, &cpb);
				bc_num_subArrays(n, cpb.num, cpb.len);
			}

			// The result is the *real* quotient, by the way, but it might take
			// multiple trips around this loop to get it.
			result += quotient;
			assert(result <= BC_BASE_POW);

			// And here's why it might take multiple trips: n might *still* be
			// greater than b. So we have to loop again. That's what this is
			// setting up for: the condition of the while loop.
			if (realnonzero) cmp = bc_num_divCmp(n, b, len);
			else cmp = -1;
		}

		assert(result < BC_BASE_POW);

		// Store the actual limb quotient.
		c->num[i] = (BcDig) result;
	}

err:
	BC_SIG_MAYLOCK;
	bc_num_free(&cpb);
	BC_LONGJMP_CONT(vm);
}

/**
 * Implements division. This is a BcNumBinOp function.
 * @param a      The first operand.
 * @param b      The second operand.
 * @param c      The return parameter.
 * @param scale  The current scale.
 */
static void
bc_num_d(BcNum* a, BcNum* b, BcNum* restrict c, size_t scale)
{
	size_t len, cpardx;
	BcNum cpa, cpb;
#if BC_ENABLE_LIBRARY
	BcVm* vm = bcl_getspecific();
#endif // BC_ENABLE_LIBRARY

	if (BC_NUM_ZERO(b)) bc_err(BC_ERR_MATH_DIVIDE_BY_ZERO);

	if (BC_NUM_ZERO(a))
	{
		bc_num_setToZero(c, scale);
		return;
	}

	if (BC_NUM_ONE(b))
	{
		bc_num_copy(c, a);
		bc_num_retireMul(c, scale, BC_NUM_NEG(a), BC_NUM_NEG(b));
		return;
	}

	// If this is true, we can use bc_num_divArray(), which would be faster.
	if (!BC_NUM_RDX_VAL(a) && !BC_NUM_RDX_VAL(b) && b->len == 1 && !scale)
	{
		BcBigDig rem;
		bc_num_divArray(a, (BcBigDig) b->num[0], c, &rem);
		bc_num_retireMul(c, scale, BC_NUM_NEG(a), BC_NUM_NEG(b));
		return;
	}

	len = bc_num_divReq(a, b, scale);

	BC_SIG_LOCK;

	// Initialize copies of the parameters. We want the length of the first
	// operand copy to be as big as the result because of the way the division
	// is implemented.
	bc_num_init(&cpa, len);
	bc_num_copy(&cpa, a);
	bc_num_createCopy(&cpb, b);

	BC_SETJMP_LOCKED(vm, err);

	BC_SIG_UNLOCK;

	len = b->len;

	// Like the above comment, we want the copy of the first parameter to be
	// larger than the second parameter.
	if (len > cpa.len)
	{
		bc_num_expand(&cpa, bc_vm_growSize(len, 2));
		bc_num_extend(&cpa, (len - cpa.len) * BC_BASE_DIGS);
	}

	cpardx = BC_NUM_RDX_VAL_NP(cpa);
	cpa.scale = cpardx * BC_BASE_DIGS;

	// This is just setting up the scale in preparation for the division.
	bc_num_extend(&cpa, b->scale);
	cpardx = BC_NUM_RDX_VAL_NP(cpa) - BC_NUM_RDX(b->scale);
	BC_NUM_RDX_SET_NP(cpa, cpardx);
	cpa.scale = cpardx * BC_BASE_DIGS;

	// Once again, just setting things up, this time to match scale.
	if (scale > cpa.scale)
	{
		bc_num_extend(&cpa, scale);
		cpardx = BC_NUM_RDX_VAL_NP(cpa);
		cpa.scale = cpardx * BC_BASE_DIGS;
	}

	// Grow if necessary.
	if (cpa.cap == cpa.len) bc_num_expand(&cpa, bc_vm_growSize(cpa.len, 1));

	// We want an extra zero in front to make things simpler.
	cpa.num[cpa.len++] = 0;

	// Still setting things up. Why all of these things are needed is not
	// something that can be easily explained, but it has to do with making the
	// actual algorithm easier to understand because it can assume a lot of
	// things. Thus, you should view all of this setup code as establishing
	// assumptions for bc_num_d_long(), where the actual division happens.
	if (cpardx == cpa.len) cpa.len = bc_num_nonZeroLen(&cpa);
	if (BC_NUM_RDX_VAL_NP(cpb) == cpb.len) cpb.len = bc_num_nonZeroLen(&cpb);
	cpb.scale = 0;
	BC_NUM_RDX_SET_NP(cpb, 0);

	bc_num_d_long(&cpa, &cpb, c, scale);

	bc_num_retireMul(c, scale, BC_NUM_NEG(a), BC_NUM_NEG(b));

err:
	BC_SIG_MAYLOCK;
	bc_num_free(&cpb);
	bc_num_free(&cpa);
	BC_LONGJMP_CONT(vm);
}

/**
 * Implements divmod. This is the actual modulus function; since modulus
 * requires a division anyway, this returns the quotient and modulus. Either can
 * be thrown out as desired.
 * @param a      The first operand.
 * @param b      The second operand.
 * @param c      The return parameter for the quotient.
 * @param d      The return parameter for the modulus.
 * @param scale  The current scale.
 * @param ts     The scale that the operation should be done to. Yes, it's not
 *               necessarily the same as scale, per the bc spec.
 */
static void
bc_num_r(BcNum* a, BcNum* b, BcNum* restrict c, BcNum* restrict d, size_t scale,
         size_t ts)
{
	BcNum temp;
	// realscale is meant to quiet a warning on GCC about longjmp() clobbering.
	// This one is real.
	size_t realscale;
	bool neg;
#if BC_ENABLE_LIBRARY
	BcVm* vm = bcl_getspecific();
#endif // BC_ENABLE_LIBRARY

	if (BC_NUM_ZERO(b)) bc_err(BC_ERR_MATH_DIVIDE_BY_ZERO);

	if (BC_NUM_ZERO(a))
	{
		bc_num_setToZero(c, ts);
		bc_num_setToZero(d, ts);
		return;
	}

	BC_SIG_LOCK;

	bc_num_init(&temp, d->cap);

	BC_SETJMP_LOCKED(vm, err);

	BC_SIG_UNLOCK;

	// Division.
	bc_num_d(a, b, c, scale);

	// We want an extra digit so we can safely truncate.
	if (scale) realscale = ts + 1;
	else realscale = scale;

	assert(BC_NUM_RDX_VALID(c));
	assert(BC_NUM_RDX_VALID(b));

	// Implement the rest of the (a - (a / b) * b) formula.
	bc_num_m(c, b, &temp, realscale);
	bc_num_sub(a, &temp, d, realscale);

	// Extend if necessary.
	if (ts > d->scale && BC_NUM_NONZERO(d)) bc_num_extend(d, ts - d->scale);

	neg = BC_NUM_NEG(d);
	bc_num_retireMul(d, ts, BC_NUM_NEG(a), BC_NUM_NEG(b));
	d->rdx = BC_NUM_NEG_VAL(d, BC_NUM_NONZERO(d) ? neg : false);

err:
	BC_SIG_MAYLOCK;
	bc_num_free(&temp);
	BC_LONGJMP_CONT(vm);
}

/**
 * Implements modulus/remainder. (Yes, I know they are different, but not in the
 * context of bc.) This is a BcNumBinOp function.
 * @param a      The first operand.
 * @param b      The second operand.
 * @param c      The return parameter.
 * @param scale  The current scale.
 */
static void
bc_num_rem(BcNum* a, BcNum* b, BcNum* restrict c, size_t scale)
{
	BcNum c1;
	size_t ts;
#if BC_ENABLE_LIBRARY
	BcVm* vm = bcl_getspecific();
#endif // BC_ENABLE_LIBRARY

	ts = bc_vm_growSize(scale, b->scale);
	ts = BC_MAX(ts, a->scale);

	BC_SIG_LOCK;

	// Need a temp for the quotient.
	bc_num_init(&c1, bc_num_mulReq(a, b, ts));

	BC_SETJMP_LOCKED(vm, err);

	BC_SIG_UNLOCK;

	bc_num_r(a, b, &c1, c, scale, ts);

err:
	BC_SIG_MAYLOCK;
	bc_num_free(&c1);
	BC_LONGJMP_CONT(vm);
}

/**
 * Implements power (exponentiation). This is a BcNumBinOp function.
 * @param a      The first operand.
 * @param b      The second operand.
 * @param c      The return parameter.
 * @param scale  The current scale.
 */
static void
bc_num_p(BcNum* a, BcNum* b, BcNum* restrict c, size_t scale)
{
	BcNum copy, btemp;
	BcBigDig exp;
	// realscale is meant to quiet a warning on GCC about longjmp() clobbering.
	// This one is real.
	size_t powrdx, resrdx, realscale;
	bool neg;
#if BC_ENABLE_LIBRARY
	BcVm* vm = bcl_getspecific();
#endif // BC_ENABLE_LIBRARY

	// This is here to silence a warning from GCC.
#if BC_GCC
	btemp.len = 0;
	btemp.rdx = 0;
	btemp.num = NULL;
#endif // BC_GCC

	if (BC_ERR(bc_num_nonInt(b, &btemp))) bc_err(BC_ERR_MATH_NON_INTEGER);

	assert(btemp.len == 0 || btemp.num != NULL);

	if (BC_NUM_ZERO(&btemp))
	{
		bc_num_one(c);
		return;
	}

	if (BC_NUM_ZERO(a))
	{
		if (BC_NUM_NEG_NP(btemp)) bc_err(BC_ERR_MATH_DIVIDE_BY_ZERO);
		bc_num_setToZero(c, scale);
		return;
	}

	if (BC_NUM_ONE(&btemp))
	{
		if (!BC_NUM_NEG_NP(btemp)) bc_num_copy(c, a);
		else bc_num_inv(a, c, scale);
		return;
	}

	neg = BC_NUM_NEG_NP(btemp);
	BC_NUM_NEG_CLR_NP(btemp);

	exp = bc_num_bigdig(&btemp);

	BC_SIG_LOCK;

	bc_num_createCopy(&copy, a);

	BC_SETJMP_LOCKED(vm, err);

	BC_SIG_UNLOCK;

	// If this is true, then we do not have to do a division, and we need to
	// set scale accordingly.
	if (!neg)
	{
		size_t max = BC_MAX(scale, a->scale), scalepow;
		scalepow = bc_num_mulOverflow(a->scale, exp);
		realscale = BC_MIN(scalepow, max);
	}
	else realscale = scale;

	// This is only implementing the first exponentiation by squaring, until it
	// reaches the first time where the square is actually used.
	for (powrdx = a->scale; !(exp & 1); exp >>= 1)
	{
		powrdx <<= 1;
		assert(BC_NUM_RDX_VALID_NP(copy));
		bc_num_mul(&copy, &copy, &copy, powrdx);
	}

	// Make c a copy of copy for the purpose of saving the squares that should
	// be saved.
	bc_num_copy(c, &copy);
	resrdx = powrdx;

	// Now finish the exponentiation by squaring, this time saving the squares
	// as necessary.
	while (exp >>= 1)
	{
		powrdx <<= 1;
		assert(BC_NUM_RDX_VALID_NP(copy));
		bc_num_mul(&copy, &copy, &copy, powrdx);

		// If this is true, we want to save that particular square. This does
		// that by multiplying c with copy.
		if (exp & 1)
		{
			resrdx += powrdx;
			assert(BC_NUM_RDX_VALID(c));
			assert(BC_NUM_RDX_VALID_NP(copy));
			bc_num_mul(c, &copy, c, resrdx);
		}
	}

	// Invert if necessary.
	if (neg) bc_num_inv(c, c, realscale);

	// Truncate if necessary.
	if (c->scale > realscale) bc_num_truncate(c, c->scale - realscale);

	bc_num_clean(c);

err:
	BC_SIG_MAYLOCK;
	bc_num_free(&copy);
	BC_LONGJMP_CONT(vm);
}

#if BC_ENABLE_EXTRA_MATH
/**
 * Implements the places operator. This is a BcNumBinOp function.
 * @param a      The first operand.
 * @param b      The second operand.
 * @param c      The return parameter.
 * @param scale  The current scale.
 */
static void
bc_num_place(BcNum* a, BcNum* b, BcNum* restrict c, size_t scale)
{
	BcBigDig val;

	BC_UNUSED(scale);

	val = bc_num_intop(a, b, c);

	// Just truncate or extend as appropriate.
	if (val < c->scale) bc_num_truncate(c, c->scale - val);
	else if (val > c->scale) bc_num_extend(c, val - c->scale);
}

/**
 * Implements the left shift operator. This is a BcNumBinOp function.
 */
static void
bc_num_left(BcNum* a, BcNum* b, BcNum* restrict c, size_t scale)
{
	BcBigDig val;

	BC_UNUSED(scale);

	val = bc_num_intop(a, b, c);

	bc_num_shiftLeft(c, (size_t) val);
}

/**
 * Implements the right shift operator. This is a BcNumBinOp function.
 */
static void
bc_num_right(BcNum* a, BcNum* b, BcNum* restrict c, size_t scale)
{
	BcBigDig val;

	BC_UNUSED(scale);

	val = bc_num_intop(a, b, c);

	if (BC_NUM_ZERO(c)) return;

	bc_num_shiftRight(c, (size_t) val);
}
#endif // BC_ENABLE_EXTRA_MATH

/**
 * Prepares for, and calls, a binary operator function. This is probably the
 * most important function in the entire file because it establishes assumptions
 * that make the rest of the code so easy. Those assumptions include:
 *
 * - a is not the same pointer as c.
 * - b is not the same pointer as c.
 * - there is enough room in c for the result.
 *
 * Without these, this whole function would basically have to be duplicated for
 * *all* binary operators.
 *
 * @param a      The first operand.
 * @param b      The second operand.
 * @param c      The return parameter.
 * @param scale  The current scale.
 * @param req    The number of limbs needed to fit the result.
 */
static void
bc_num_binary(BcNum* a, BcNum* b, BcNum* c, size_t scale, BcNumBinOp op,
              size_t req)
{
	BcNum* ptr_a;
	BcNum* ptr_b;
	BcNum num2;
#if BC_ENABLE_LIBRARY
	BcVm* vm = NULL;
#endif // BC_ENABLE_LIBRARY

	assert(a != NULL && b != NULL && c != NULL && op != NULL);

	assert(BC_NUM_RDX_VALID(a));
	assert(BC_NUM_RDX_VALID(b));

	BC_SIG_LOCK;

	ptr_a = c == a ? &num2 : a;
	ptr_b = c == b ? &num2 : b;

	// Actually reallocate. If we don't reallocate, we want to expand at the
	// very least.
	if (c == a || c == b)
	{
#if BC_ENABLE_LIBRARY
		vm = bcl_getspecific();
#endif // BC_ENABLE_LIBRARY

		// NOLINTNEXTLINE
		memcpy(&num2, c, sizeof(BcNum));

		bc_num_init(c, req);

		// Must prepare for cleanup. We want this here so that locals that got
		// set stay set since a longjmp() is not guaranteed to preserve locals.
		BC_SETJMP_LOCKED(vm, err);
		BC_SIG_UNLOCK;
	}
	else
	{
		BC_SIG_UNLOCK;
		bc_num_expand(c, req);
	}

	// It is okay for a and b to be the same. If a binary operator function does
	// need them to be different, the binary operator function is responsible
	// for that.

	// Call the actual binary operator function.
	op(ptr_a, ptr_b, c, scale);

	assert(!BC_NUM_NEG(c) || BC_NUM_NONZERO(c));
	assert(BC_NUM_RDX_VAL(c) <= c->len || !c->len);
	assert(BC_NUM_RDX_VALID(c));
	assert(!c->len || c->num[c->len - 1] || BC_NUM_RDX_VAL(c) == c->len);

err:
	// Cleanup only needed if we initialized c to a new number.
	if (c == a || c == b)
	{
		BC_SIG_MAYLOCK;
		bc_num_free(&num2);
		BC_LONGJMP_CONT(vm);
	}
}

/**
 * Tests a number string for validity. This function has a history; I originally
 * wrote it because I did not trust my parser. Over time, however, I came to
 * trust it, so I was able to relegate this function to debug builds only, and I
 * used it in assert()'s. But then I created the library, and well, I can't
 * trust users, so I reused this for yelling at users.
 * @param val  The string to check to see if it's a valid number string.
 * @return     True if the string is a valid number string, false otherwise.
 */
bool
bc_num_strValid(const char* restrict val)
{
	bool radix = false;
	size_t i, len = strlen(val);

	// Notice that I don't check if there is a negative sign. That is not part
	// of a valid number, except in the library. The library-specific code takes
	// care of that part.

	// Nothing in the string is okay.
	if (!len) return true;

	// Loop through the characters.
	for (i = 0; i < len; ++i)
	{
		BcDig c = val[i];

		// If we have found a radix point...
		if (c == '.')
		{
			// We don't allow two radices.
			if (radix) return false;

			radix = true;
			continue;
		}

		// We only allow digits and uppercase letters.
		if (!(isdigit(c) || isupper(c))) return false;
	}

	return true;
}

/**
 * Parses one character and returns the digit that corresponds to that
 * character according to the base.
 * @param c     The character to parse.
 * @param base  The base.
 * @return      The character as a digit.
 */
static BcBigDig
bc_num_parseChar(char c, size_t base)
{
	assert(isupper(c) || isdigit(c));

	// If a letter...
	if (isupper(c))
	{
#if BC_ENABLE_LIBRARY
		BcVm* vm = bcl_getspecific();
#endif // BC_ENABLE_LIBRARY

		// This returns the digit that directly corresponds with the letter.
		c = BC_NUM_NUM_LETTER(c);

		// If the digit is greater than the base, we clamp.
		if (BC_DIGIT_CLAMP)
		{
			c = ((size_t) c) >= base ? (char) base - 1 : c;
		}
	}
	// Straight convert the digit to a number.
	else c -= '0';

	return (BcBigDig) (uchar) c;
}

/**
 * Parses a string as a decimal number. This is separate because it's going to
 * be the most used, and it can be heavily optimized for decimal only.
 * @param n    The number to parse into and return. Must be preallocated.
 * @param val  The string to parse.
 */
static void
bc_num_parseDecimal(BcNum* restrict n, const char* restrict val)
{
	size_t len, i, temp, mod;
	const char* ptr;
	bool zero = true, rdx;
#if BC_ENABLE_LIBRARY
	BcVm* vm = bcl_getspecific();
#endif // BC_ENABLE_LIBRARY

	// Eat leading zeroes.
	for (i = 0; val[i] == '0'; ++i)
	{
		continue;
	}

	val += i;
	assert(!val[0] || isalnum(val[0]) || val[0] == '.');

	// All 0's. We can just return, since this procedure expects a virgin
	// (already 0) BcNum.
	if (!val[0]) return;

	// The length of the string is the length of the number, except it might be
	// one bigger because of a decimal point.
	len = strlen(val);

	// Find the location of the decimal point.
	ptr = strchr(val, '.');
	rdx = (ptr != NULL);

	// We eat leading zeroes again. These leading zeroes are different because
	// they will come after the decimal point if they exist, and since that's
	// the case, they must be preserved.
	for (i = 0; i < len && (zero = (val[i] == '0' || val[i] == '.')); ++i)
	{
		continue;
	}

	// Set the scale of the number based on the location of the decimal point.
	// The casts to uintptr_t is to ensure that bc does not hit undefined
	// behavior when doing math on the values.
	n->scale = (size_t) (rdx *
	                     (((uintptr_t) (val + len)) - (((uintptr_t) ptr) + 1)));

	// Set rdx.
	BC_NUM_RDX_SET(n, BC_NUM_RDX(n->scale));

	// Calculate length. First, the length of the integer, then the number of
	// digits in the last limb, then the length.
	i = len - (ptr == val ? 0 : i) - rdx;
	temp = BC_NUM_ROUND_POW(i);
	mod = n->scale % BC_BASE_DIGS;
	i = mod ? BC_BASE_DIGS - mod : 0;
	n->len = ((temp + i) / BC_BASE_DIGS);

	// Expand and zero. The plus extra is in case the lack of clamping causes
	// the number to overflow the original bounds.
	bc_num_expand(n, n->len + !BC_DIGIT_CLAMP);
	// NOLINTNEXTLINE
	memset(n->num, 0, BC_NUM_SIZE(n->len + !BC_DIGIT_CLAMP));

	if (zero)
	{
		// I think I can set rdx directly to zero here because n should be a
		// new number with sign set to false.
		n->len = n->rdx = 0;
	}
	else
	{
		// There is actually stuff to parse if we make it here. Yay...
		BcBigDig exp, pow;

		assert(i <= BC_NUM_BIGDIG_MAX);

		// The exponent and power.
		exp = (BcBigDig) i;
		pow = bc_num_pow10[exp];

		// Parse loop. We parse backwards because numbers are stored little
		// endian.
		for (i = len - 1; i < len; --i, ++exp)
		{
			char c = val[i];

			// Skip the decimal point.
			if (c == '.') exp -= 1;
			else
			{
				// The index of the limb.
				size_t idx = exp / BC_BASE_DIGS;
				BcBigDig dig;

				if (isupper(c))
				{
					// Clamp for the base.
					if (!BC_DIGIT_CLAMP) c = BC_NUM_NUM_LETTER(c);
					else c = 9;
				}
				else c -= '0';

				// Add the digit to the limb. This takes care of overflow from
				// lack of clamping.
				dig = ((BcBigDig) n->num[idx]) + ((BcBigDig) c) * pow;
				if (dig >= BC_BASE_POW)
				{
					// We cannot go over BC_BASE_POW with clamping.
					assert(!BC_DIGIT_CLAMP);

					n->num[idx + 1] = (BcDig) (dig / BC_BASE_POW);
					n->num[idx] = (BcDig) (dig % BC_BASE_POW);
					assert(n->num[idx] >= 0 && n->num[idx] < BC_BASE_POW);
					assert(n->num[idx + 1] >= 0 &&
					       n->num[idx + 1] < BC_BASE_POW);
				}
				else
				{
					n->num[idx] = (BcDig) dig;
					assert(n->num[idx] >= 0 && n->num[idx] < BC_BASE_POW);
				}

				// Adjust the power and exponent.
				if ((exp + 1) % BC_BASE_DIGS == 0) pow = 1;
				else pow *= BC_BASE;
			}
		}
	}

	// Make sure to add one to the length if needed from lack of clamping.
	n->len += (!BC_DIGIT_CLAMP && n->num[n->len] != 0);
}

/**
 * Parse a number in any base (besides decimal).
 * @param n     The number to parse into and return. Must be preallocated.
 * @param val   The string to parse.
 * @param base  The base to parse as.
 */
static void
bc_num_parseBase(BcNum* restrict n, const char* restrict val, BcBigDig base)
{
	BcNum temp, mult1, mult2, result1, result2;
	BcNum* m1;
	BcNum* m2;
	BcNum* ptr;
	char c = 0;
	bool zero = true;
	BcBigDig v;
	size_t digs, len = strlen(val);
	// This is volatile to quiet a warning on GCC about longjmp() clobbering.
	volatile size_t i;
#if BC_ENABLE_LIBRARY
	BcVm* vm = bcl_getspecific();
#endif // BC_ENABLE_LIBRARY

	// If zero, just return because the number should be virgin (already 0).
	for (i = 0; zero && i < len; ++i)
	{
		zero = (val[i] == '.' || val[i] == '0');
	}
	if (zero) return;

	BC_SIG_LOCK;

	bc_num_init(&temp, BC_NUM_BIGDIG_LOG10);
	bc_num_init(&mult1, BC_NUM_BIGDIG_LOG10);

	BC_SETJMP_LOCKED(vm, int_err);

	BC_SIG_UNLOCK;

	// We split parsing into parsing the integer and parsing the fractional
	// part.

	// Parse the integer part. This is the easy part because we just multiply
	// the number by the base, then add the digit.
	for (i = 0; i < len && (c = val[i]) && c != '.'; ++i)
	{
		// Convert the character to a digit.
		v = bc_num_parseChar(c, base);

		// Multiply the number.
		bc_num_mulArray(n, base, &mult1);

		// Convert the digit to a number and add.
		bc_num_bigdig2num(&temp, v);
		bc_num_add(&mult1, &temp, n, 0);
	}

	// If this condition is true, then we are done. We still need to do cleanup
	// though.
	if (i == len && !val[i]) goto int_err;

	// If we get here, we *must* be at the radix point.
	assert(val[i] == '.');

	BC_SIG_LOCK;

	// Unset the jump to reset in for these new initializations.
	BC_UNSETJMP(vm);

	bc_num_init(&mult2, BC_NUM_BIGDIG_LOG10);
	bc_num_init(&result1, BC_NUM_DEF_SIZE);
	bc_num_init(&result2, BC_NUM_DEF_SIZE);
	bc_num_one(&mult1);

	BC_SETJMP_LOCKED(vm, err);

	BC_SIG_UNLOCK;

	// Pointers for easy switching.
	m1 = &mult1;
	m2 = &mult2;

	// Parse the fractional part. This is the hard part.
	for (i += 1, digs = 0; i < len && (c = val[i]); ++i, ++digs)
	{
		size_t rdx;

		// Convert the character to a digit.
		v = bc_num_parseChar(c, base);

		// We keep growing result2 according to the base because the more digits
		// after the radix, the more significant the digits close to the radix
		// should be.
		bc_num_mulArray(&result1, base, &result2);

		// Convert the digit to a number.
		bc_num_bigdig2num(&temp, v);

		// Add the digit into the fraction part.
		bc_num_add(&result2, &temp, &result1, 0);

		// Keep growing m1 and m2 for use after the loop.
		bc_num_mulArray(m1, base, m2);

		rdx = BC_NUM_RDX_VAL(m2);

		if (m2->len < rdx) m2->len = rdx;

		// Switch.
		ptr = m1;
		m1 = m2;
		m2 = ptr;
	}

	// This one cannot be a divide by 0 because mult starts out at 1, then is
	// multiplied by base, and base cannot be 0, so mult cannot be 0. And this
	// is the reason we keep growing m1 and m2; this division is what converts
	// the parsed fractional part from an integer to a fractional part.
	bc_num_div(&result1, m1, &result2, digs * 2);

	// Pretruncate.
	bc_num_truncate(&result2, digs);

	// The final add of the integer part to the fractional part.
	bc_num_add(n, &result2, n, digs);

	// Basic cleanup.
	if (BC_NUM_NONZERO(n))
	{
		if (n->scale < digs) bc_num_extend(n, digs - n->scale);
	}
	else bc_num_zero(n);

err:
	BC_SIG_MAYLOCK;
	bc_num_free(&result2);
	bc_num_free(&result1);
	bc_num_free(&mult2);
int_err:
	BC_SIG_MAYLOCK;
	bc_num_free(&mult1);
	bc_num_free(&temp);
	BC_LONGJMP_CONT(vm);
}

/**
 * Prints a backslash+newline combo if the number of characters needs it. This
 * is really a convenience function.
 */
static inline void
bc_num_printNewline(void)
{
#if !BC_ENABLE_LIBRARY
	if (vm->nchars >= vm->line_len - 1 && vm->line_len)
	{
		bc_vm_putchar('\\', bc_flush_none);
		bc_vm_putchar('\n', bc_flush_err);
	}
#endif // !BC_ENABLE_LIBRARY
}

/**
 * Prints a character after a backslash+newline, if needed.
 * @param c       The character to print.
 * @param bslash  Whether to print a backslash+newline.
 */
static void
bc_num_putchar(int c, bool bslash)
{
	if (c != '\n' && bslash) bc_num_printNewline();
	bc_vm_putchar(c, bc_flush_save);
}

#if !BC_ENABLE_LIBRARY

/**
 * Prints a character for a number's digit. This is for printing for dc's P
 * command. This function does not need to worry about radix points. This is a
 * BcNumDigitOp.
 * @param n       The "digit" to print.
 * @param len     The "length" of the digit, or number of characters that will
 *                need to be printed for the digit.
 * @param rdx     True if a decimal (radix) point should be printed.
 * @param bslash  True if a backslash+newline should be printed if the character
 *                limit for the line is reached, false otherwise.
 */
static void
bc_num_printChar(size_t n, size_t len, bool rdx, bool bslash)
{
	BC_UNUSED(rdx);
	BC_UNUSED(len);
	BC_UNUSED(bslash);
	assert(len == 1);
	bc_vm_putchar((uchar) n, bc_flush_save);
}

#endif // !BC_ENABLE_LIBRARY

/**
 * Prints a series of characters for large bases. This is for printing in bases
 * above hexadecimal. This is a BcNumDigitOp.
 * @param n       The "digit" to print.
 * @param len     The "length" of the digit, or number of characters that will
 *                need to be printed for the digit.
 * @param rdx     True if a decimal (radix) point should be printed.
 * @param bslash  True if a backslash+newline should be printed if the character
 *                limit for the line is reached, false otherwise.
 */
static void
bc_num_printDigits(size_t n, size_t len, bool rdx, bool bslash)
{
	size_t exp, pow;

	// If needed, print the radix; otherwise, print a space to separate digits.
	bc_num_putchar(rdx ? '.' : ' ', true);

	// Calculate the exponent and power.
	for (exp = 0, pow = 1; exp < len - 1; ++exp, pow *= BC_BASE)
	{
		continue;
	}

	// Print each character individually.
	for (exp = 0; exp < len; pow /= BC_BASE, ++exp)
	{
		// The individual subdigit.
		size_t dig = n / pow;

		// Take the subdigit away.
		n -= dig * pow;

		// Print the subdigit.
		bc_num_putchar(((uchar) dig) + '0', bslash || exp != len - 1);
	}
}

/**
 * Prints a character for a number's digit. This is for printing in bases for
 * hexadecimal and below because they always print only one character at a time.
 * This is a BcNumDigitOp.
 * @param n       The "digit" to print.
 * @param len     The "length" of the digit, or number of characters that will
 *                need to be printed for the digit.
 * @param rdx     True if a decimal (radix) point should be printed.
 * @param bslash  True if a backslash+newline should be printed if the character
 *                limit for the line is reached, false otherwise.
 */
static void
bc_num_printHex(size_t n, size_t len, bool rdx, bool bslash)
{
	BC_UNUSED(len);
	BC_UNUSED(bslash);

	assert(len == 1);

	if (rdx) bc_num_putchar('.', true);

	bc_num_putchar(bc_num_hex_digits[n], bslash);
}

/**
 * Prints a decimal number. This is specially written for optimization since
 * this will be used the most and because bc's numbers are already in decimal.
 * @param n        The number to print.
 * @param newline  Whether to print backslash+newlines on long enough lines.
 */
static void
bc_num_printDecimal(const BcNum* restrict n, bool newline)
{
	size_t i, j, rdx = BC_NUM_RDX_VAL(n);
	bool zero = true;
	size_t buffer[BC_BASE_DIGS];

	// Print loop.
	for (i = n->len - 1; i < n->len; --i)
	{
		BcDig n9 = n->num[i];
		size_t temp;
		bool irdx = (i == rdx - 1);

		// Calculate the number of digits in the limb.
		zero = (zero & !irdx);
		temp = n->scale % BC_BASE_DIGS;
		temp = i || !temp ? 0 : BC_BASE_DIGS - temp;

		// NOLINTNEXTLINE
		memset(buffer, 0, BC_BASE_DIGS * sizeof(size_t));

		// Fill the buffer with individual digits.
		for (j = 0; n9 && j < BC_BASE_DIGS; ++j)
		{
			buffer[j] = ((size_t) n9) % BC_BASE;
			n9 /= BC_BASE;
		}

		// Print the digits in the buffer.
		for (j = BC_BASE_DIGS - 1; j < BC_BASE_DIGS && j >= temp; --j)
		{
			// Figure out whether to print the decimal point.
			bool print_rdx = (irdx & (j == BC_BASE_DIGS - 1));

			// The zero variable helps us skip leading zero digits in the limb.
			zero = (zero && buffer[j] == 0);

			if (!zero)
			{
				// While the first three arguments should be self-explanatory,
				// the last needs explaining. I don't want to print a newline
				// when the last digit to be printed could take the place of the
				// backslash rather than being pushed, as a single character, to
				// the next line. That's what that last argument does for bc.
				bc_num_printHex(buffer[j], 1, print_rdx,
				                !newline || (j > temp || i != 0));
			}
		}
	}
}

#if BC_ENABLE_EXTRA_MATH

/**
 * Prints a number in scientific or engineering format. When doing this, we are
 * always printing in decimal.
 * @param n        The number to print.
 * @param eng      True if we are in engineering mode.
 * @param newline  Whether to print backslash+newlines on long enough lines.
 */
static void
bc_num_printExponent(const BcNum* restrict n, bool eng, bool newline)
{
	size_t places, mod, nrdx = BC_NUM_RDX_VAL(n);
	bool neg = (n->len <= nrdx);
	BcNum temp, exp;
	BcDig digs[BC_NUM_BIGDIG_LOG10];
#if BC_ENABLE_LIBRARY
	BcVm* vm = bcl_getspecific();
#endif // BC_ENABLE_LIBRARY

	BC_SIG_LOCK;

	bc_num_createCopy(&temp, n);

	BC_SETJMP_LOCKED(vm, exit);

	BC_SIG_UNLOCK;

	// We need to calculate the exponents, and they change based on whether the
	// number is all fractional or not, obviously.
	if (neg)
	{
		// Figure out how many limbs after the decimal point is zero.
		size_t i, idx = bc_num_nonZeroLen(n) - 1;

		places = 1;

		// Figure out how much in the last limb is zero.
		for (i = BC_BASE_DIGS - 1; i < BC_BASE_DIGS; --i)
		{
			if (bc_num_pow10[i] > (BcBigDig) n->num[idx]) places += 1;
			else break;
		}

		// Calculate the combination of zero limbs and zero digits in the last
		// limb.
		places += (nrdx - (idx + 1)) * BC_BASE_DIGS;
		mod = places % 3;

		// Calculate places if we are in engineering mode.
		if (eng && mod != 0) places += 3 - mod;

		// Shift the temp to the right place.
		bc_num_shiftLeft(&temp, places);
	}
	else
	{
		// This is the number of digits that we are supposed to put behind the
		// decimal point.
		places = bc_num_intDigits(n) - 1;

		// Calculate the true number based on whether engineering mode is
		// activated.
		mod = places % 3;
		if (eng && mod != 0) places -= 3 - (3 - mod);

		// Shift the temp to the right place.
		bc_num_shiftRight(&temp, places);
	}

	// Print the shifted number.
	bc_num_printDecimal(&temp, newline);

	// Print the e.
	bc_num_putchar('e', !newline);

	// Need to explicitly print a zero exponent.
	if (!places)
	{
		bc_num_printHex(0, 1, false, !newline);
		goto exit;
	}

	// Need to print sign for the exponent.
	if (neg) bc_num_putchar('-', true);

	// Create a temporary for the exponent...
	bc_num_setup(&exp, digs, BC_NUM_BIGDIG_LOG10);
	bc_num_bigdig2num(&exp, (BcBigDig) places);

	/// ..and print it.
	bc_num_printDecimal(&exp, newline);

exit:
	BC_SIG_MAYLOCK;
	bc_num_free(&temp);
	BC_LONGJMP_CONT(vm);
}
#endif // BC_ENABLE_EXTRA_MATH

/**
 * Converts a number from limbs with base BC_BASE_POW to base @a pow, where
 * @a pow is obase^N.
 * @param n    The number to convert.
 * @param rem  BC_BASE_POW - @a pow.
 * @param pow  The power of obase we will convert the number to.
 * @param idx  The index of the number to start converting at. Doing the
 *             conversion is O(n^2); we have to sweep through starting at the
 *             least significant limb
 */
static void
bc_num_printFixup(BcNum* restrict n, BcBigDig rem, BcBigDig pow, size_t idx)
{
	size_t i, len = n->len - idx;
	BcBigDig acc;
	BcDig* a = n->num + idx;

	// Ignore if there's just one limb left. This is the part that requires the
	// extra loop after the one calling this function in bc_num_printPrepare().
	if (len < 2) return;

	// Loop through the remaining limbs and convert. We start at the second limb
	// because we pull the value from the previous one as well.
	for (i = len - 1; i > 0; --i)
	{
		// Get the limb and add it to the previous, along with multiplying by
		// the remainder because that's the proper overflow. "acc" means
		// "accumulator," by the way.
		acc = ((BcBigDig) a[i]) * rem + ((BcBigDig) a[i - 1]);

		// Store a value in base pow in the previous limb.
		a[i - 1] = (BcDig) (acc % pow);

		// Divide by the base and accumulate the remaining value in the limb.
		acc /= pow;
		acc += (BcBigDig) a[i];

		// If the accumulator is greater than the base...
		if (acc >= BC_BASE_POW)
		{
			// Do we need to grow?
			if (i == len - 1)
			{
				// Grow.
				len = bc_vm_growSize(len, 1);
				bc_num_expand(n, bc_vm_growSize(len, idx));

				// Update the pointer because it may have moved.
				a = n->num + idx;

				// Zero out the last limb.
				a[len - 1] = 0;
			}

			// Overflow into the next limb since we are over the base.
			a[i + 1] += acc / BC_BASE_POW;
			acc %= BC_BASE_POW;
		}

		assert(acc < BC_BASE_POW);

		// Set the limb.
		a[i] = (BcDig) acc;
	}

	// We may have grown the number, so adjust the length.
	n->len = len + idx;
}

/**
 * Prepares a number for printing in a base that is not a divisor of
 * BC_BASE_POW. This basically converts the number from having limbs of base
 * BC_BASE_POW to limbs of pow, where pow is obase^N.
 * @param n    The number to prepare for printing.
 * @param rem  The remainder of BC_BASE_POW when divided by a power of the base.
 * @param pow  The power of the base.
 */
static void
bc_num_printPrepare(BcNum* restrict n, BcBigDig rem, BcBigDig pow)
{
	size_t i;

	// Loop from the least significant limb to the most significant limb and
	// convert limbs in each pass.
	for (i = 0; i < n->len; ++i)
	{
		bc_num_printFixup(n, rem, pow, i);
	}

	// bc_num_printFixup() does not do everything it is supposed to, so we do
	// the last bit of cleanup here. That cleanup is to ensure that each limb
	// is less than pow and to expand the number to fit new limbs as necessary.
	for (i = 0; i < n->len; ++i)
	{
		assert(pow == ((BcBigDig) ((BcDig) pow)));

		// If the limb needs fixing...
		if (n->num[i] >= (BcDig) pow)
		{
			// Do we need to grow?
			if (i + 1 == n->len)
			{
				// Grow the number.
				n->len = bc_vm_growSize(n->len, 1);
				bc_num_expand(n, n->len);

				// Without this, we might use uninitialized data.
				n->num[i + 1] = 0;
			}

			assert(pow < BC_BASE_POW);

			// Overflow into the next limb.
			n->num[i + 1] += n->num[i] / ((BcDig) pow);
			n->num[i] %= (BcDig) pow;
		}
	}
}

static void
bc_num_printNum(BcNum* restrict n, BcBigDig base, size_t len,
                BcNumDigitOp print, bool newline)
{
	BcVec stack;
	BcNum intp, fracp1, fracp2, digit, flen1, flen2;
	BcNum* n1;
	BcNum* n2;
	BcNum* temp;
	BcBigDig dig = 0, acc, exp;
	BcBigDig* ptr;
	size_t i, j, nrdx, idigits;
	bool radix;
	BcDig digit_digs[BC_NUM_BIGDIG_LOG10 + 1];
#if BC_ENABLE_LIBRARY
	BcVm* vm = bcl_getspecific();
#endif // BC_ENABLE_LIBRARY

	assert(base > 1);

	// Easy case. Even with scale, we just print this.
	if (BC_NUM_ZERO(n))
	{
		print(0, len, false, !newline);
		return;
	}

	// This function uses an algorithm that Stefan Esser <se@freebsd.org> came
	// up with to print the integer part of a number. What it does is convert
	// intp into a number of the specified base, but it does it directly,
	// instead of just doing a series of divisions and printing the remainders
	// in reverse order.
	//
	// Let me explain in a bit more detail:
	//
	// The algorithm takes the current least significant limb (after intp has
	// been converted to an integer) and the next to least significant limb, and
	// it converts the least significant limb into one of the specified base,
	// putting any overflow into the next to least significant limb. It iterates
	// through the whole number, from least significant to most significant,
	// doing this conversion. At the end of that iteration, the least
	// significant limb is converted, but the others are not, so it iterates
	// again, starting at the next to least significant limb. It keeps doing
	// that conversion, skipping one more limb than the last time, until all
	// limbs have been converted. Then it prints them in reverse order.
	//
	// That is the gist of the algorithm. It leaves out several things, such as
	// the fact that limbs are not always converted into the specified base, but
	// into something close, basically a power of the specified base. In
	// Stefan's words, "You could consider BcDigs to be of base 10^BC_BASE_DIGS
	// in the normal case and obase^N for the largest value of N that satisfies
	// obase^N <= 10^BC_BASE_DIGS. [This means that] the result is not in base
	// "obase", but in base "obase^N", which happens to be printable as a number
	// of base "obase" without consideration for neighbouring BcDigs." This fact
	// is what necessitates the existence of the loop later in this function.
	//
	// The conversion happens in bc_num_printPrepare() where the outer loop
	// happens and bc_num_printFixup() where the inner loop, or actual
	// conversion, happens. In other words, bc_num_printPrepare() is where the
	// loop that starts at the least significant limb and goes to the most
	// significant limb. Then, on every iteration of its loop, it calls
	// bc_num_printFixup(), which has the inner loop of actually converting
	// the limbs it passes into limbs of base obase^N rather than base
	// BC_BASE_POW.

	nrdx = BC_NUM_RDX_VAL(n);

	BC_SIG_LOCK;

	// The stack is what allows us to reverse the digits for printing.
	bc_vec_init(&stack, sizeof(BcBigDig), BC_DTOR_NONE);
	bc_num_init(&fracp1, nrdx);

	// intp will be the "integer part" of the number, so copy it.
	bc_num_createCopy(&intp, n);

	BC_SETJMP_LOCKED(vm, err);

	BC_SIG_UNLOCK;

	// Make intp an integer.
	bc_num_truncate(&intp, intp.scale);

	// Get the fractional part out.
	bc_num_sub(n, &intp, &fracp1, 0);

	// If the base is not the same as the last base used for printing, we need
	// to update the cached exponent and power. Yes, we cache the values of the
	// exponent and power. That is to prevent us from calculating them every
	// time because printing will probably happen multiple times on the same
	// base.
	if (base != vm->last_base)
	{
		vm->last_pow = 1;
		vm->last_exp = 0;

		// Calculate the exponent and power.
		while (vm->last_pow * base <= BC_BASE_POW)
		{
			vm->last_pow *= base;
			vm->last_exp += 1;
		}

		// Also, the remainder and base itself.
		vm->last_rem = BC_BASE_POW - vm->last_pow;
		vm->last_base = base;
	}

	exp = vm->last_exp;

	// If vm->last_rem is 0, then the base we are printing in is a divisor of
	// BC_BASE_POW, which is the easy case because it means that BC_BASE_POW is
	// a power of obase, and no conversion is needed. If it *is* 0, then we have
	// the hard case, and we have to prepare the number for the base.
	if (vm->last_rem != 0)
	{
		bc_num_printPrepare(&intp, vm->last_rem, vm->last_pow);
	}

	// After the conversion comes the surprisingly easy part. From here on out,
	// this is basically naive code that I wrote, adjusted for the larger bases.

	// Fill the stack of digits for the integer part.
	for (i = 0; i < intp.len; ++i)
	{
		// Get the limb.
		acc = (BcBigDig) intp.num[i];

		// Turn the limb into digits of base obase.
		for (j = 0; j < exp && (i < intp.len - 1 || acc != 0); ++j)
		{
			// This condition is true if we are not at the last digit.
			if (j != exp - 1)
			{
				dig = acc % base;
				acc /= base;
			}
			else
			{
				dig = acc;
				acc = 0;
			}

			assert(dig < base);

			// Push the digit onto the stack.
			bc_vec_push(&stack, &dig);
		}

		assert(acc == 0);
	}

	// Go through the stack backwards and print each digit.
	for (i = 0; i < stack.len; ++i)
	{
		ptr = bc_vec_item_rev(&stack, i);

		assert(ptr != NULL);

		// While the first three arguments should be self-explanatory, the last
		// needs explaining. I don't want to print a newline when the last digit
		// to be printed could take the place of the backslash rather than being
		// pushed, as a single character, to the next line. That's what that
		// last argument does for bc.
		print(*ptr, len, false,
		      !newline || (n->scale != 0 || i == stack.len - 1));
	}

	// We are done if there is no fractional part.
	if (!n->scale) goto err;

	BC_SIG_LOCK;

	// Reset the jump because some locals are changing.
	BC_UNSETJMP(vm);

	bc_num_init(&fracp2, nrdx);
	bc_num_setup(&digit, digit_digs, sizeof(digit_digs) / sizeof(BcDig));
	bc_num_init(&flen1, BC_NUM_BIGDIG_LOG10);
	bc_num_init(&flen2, BC_NUM_BIGDIG_LOG10);

	BC_SETJMP_LOCKED(vm, frac_err);

	BC_SIG_UNLOCK;

	bc_num_one(&flen1);

	radix = true;

	// Pointers for easy switching.
	n1 = &flen1;
	n2 = &flen2;

	fracp2.scale = n->scale;
	BC_NUM_RDX_SET_NP(fracp2, BC_NUM_RDX(fracp2.scale));

	// As long as we have not reached the scale of the number, keep printing.
	while ((idigits = bc_num_intDigits(n1)) <= n->scale)
	{
		// These numbers will keep growing.
		bc_num_expand(&fracp2, fracp1.len + 1);
		bc_num_mulArray(&fracp1, base, &fracp2);

		nrdx = BC_NUM_RDX_VAL_NP(fracp2);

		// Ensure an invariant.
		if (fracp2.len < nrdx) fracp2.len = nrdx;

		// fracp is guaranteed to be non-negative and small enough.
		dig = bc_num_bigdig2(&fracp2);

		// Convert the digit to a number and subtract it from the number.
		bc_num_bigdig2num(&digit, dig);
		bc_num_sub(&fracp2, &digit, &fracp1, 0);

		// While the first three arguments should be self-explanatory, the last
		// needs explaining. I don't want to print a newline when the last digit
		// to be printed could take the place of the backslash rather than being
		// pushed, as a single character, to the next line. That's what that
		// last argument does for bc.
		print(dig, len, radix, !newline || idigits != n->scale);

		// Update the multipliers.
		bc_num_mulArray(n1, base, n2);

		radix = false;

		// Switch.
		temp = n1;
		n1 = n2;
		n2 = temp;
	}

frac_err:
	BC_SIG_MAYLOCK;
	bc_num_free(&flen2);
	bc_num_free(&flen1);
	bc_num_free(&fracp2);
err:
	BC_SIG_MAYLOCK;
	bc_num_free(&fracp1);
	bc_num_free(&intp);
	bc_vec_free(&stack);
	BC_LONGJMP_CONT(vm);
}

/**
 * Prints a number in the specified base, or rather, figures out which function
 * to call to print the number in the specified base and calls it.
 * @param n        The number to print.
 * @param base     The base to print in.
 * @param newline  Whether to print backslash+newlines on long enough lines.
 */
static void
bc_num_printBase(BcNum* restrict n, BcBigDig base, bool newline)
{
	size_t width;
	BcNumDigitOp print;
	bool neg = BC_NUM_NEG(n);

	// Clear the sign because it makes the actual printing easier when we have
	// to do math.
	BC_NUM_NEG_CLR(n);

	// Bases at hexadecimal and below are printed as one character, larger bases
	// are printed as a series of digits separated by spaces.
	if (base <= BC_NUM_MAX_POSIX_IBASE)
	{
		width = 1;
		print = bc_num_printHex;
	}
	else
	{
		assert(base <= BC_BASE_POW);
		width = bc_num_log10(base - 1);
		print = bc_num_printDigits;
	}

	// Print.
	bc_num_printNum(n, base, width, print, newline);

	// Reset the sign.
	n->rdx = BC_NUM_NEG_VAL(n, neg);
}

#if !BC_ENABLE_LIBRARY

void
bc_num_stream(BcNum* restrict n)
{
	bc_num_printNum(n, BC_NUM_STREAM_BASE, 1, bc_num_printChar, false);
}

#endif // !BC_ENABLE_LIBRARY

void
bc_num_setup(BcNum* restrict n, BcDig* restrict num, size_t cap)
{
	assert(n != NULL);
	n->num = num;
	n->cap = cap;
	bc_num_zero(n);
}

void
bc_num_init(BcNum* restrict n, size_t req)
{
	BcDig* num;

	BC_SIG_ASSERT_LOCKED;

	assert(n != NULL);

	// BC_NUM_DEF_SIZE is set to be about the smallest allocation size that
	// malloc() returns in practice, so just use it.
	req = req >= BC_NUM_DEF_SIZE ? req : BC_NUM_DEF_SIZE;

	// If we can't use a temp, allocate.
	if (req != BC_NUM_DEF_SIZE) num = bc_vm_malloc(BC_NUM_SIZE(req));
	else
	{
		num = bc_vm_getTemp() == NULL ? bc_vm_malloc(BC_NUM_SIZE(req)) :
		                                bc_vm_takeTemp();
	}

	bc_num_setup(n, num, req);
}

void
bc_num_clear(BcNum* restrict n)
{
	n->num = NULL;
	n->cap = 0;
}

void
bc_num_free(void* num)
{
	BcNum* n = (BcNum*) num;

	BC_SIG_ASSERT_LOCKED;

	assert(n != NULL);

	if (n->cap == BC_NUM_DEF_SIZE) bc_vm_addTemp(n->num);
	else free(n->num);
}

void
bc_num_copy(BcNum* d, const BcNum* s)
{
	assert(d != NULL && s != NULL);

	if (d == s) return;

	bc_num_expand(d, s->len);
	d->len = s->len;

	// I can just copy directly here because the sign *and* rdx will be
	// properly preserved.
	d->rdx = s->rdx;
	d->scale = s->scale;
	// NOLINTNEXTLINE
	memcpy(d->num, s->num, BC_NUM_SIZE(d->len));
}

void
bc_num_createCopy(BcNum* d, const BcNum* s)
{
	BC_SIG_ASSERT_LOCKED;
	bc_num_init(d, s->len);
	bc_num_copy(d, s);
}

void
bc_num_createFromBigdig(BcNum* restrict n, BcBigDig val)
{
	BC_SIG_ASSERT_LOCKED;
	bc_num_init(n, BC_NUM_BIGDIG_LOG10);
	bc_num_bigdig2num(n, val);
}

size_t
bc_num_scale(const BcNum* restrict n)
{
	return n->scale;
}

size_t
bc_num_len(const BcNum* restrict n)
{
	size_t len = n->len;

	// Always return at least 1.
	if (BC_NUM_ZERO(n)) return n->scale ? n->scale : 1;

	// If this is true, there is no integer portion of the number.
	if (BC_NUM_RDX_VAL(n) == len)
	{
		// We have to take into account the fact that some of the digits right
		// after the decimal could be zero. If that is the case, we need to
		// ignore them until we hit the first non-zero digit.

		size_t zero, scale;

		// The number of limbs with non-zero digits.
		len = bc_num_nonZeroLen(n);

		// Get the number of digits in the last limb.
		scale = n->scale % BC_BASE_DIGS;
		scale = scale ? scale : BC_BASE_DIGS;

		// Get the number of zero digits.
		zero = bc_num_zeroDigits(n->num + len - 1);

		// Calculate the true length.
		len = len * BC_BASE_DIGS - zero - (BC_BASE_DIGS - scale);
	}
	// Otherwise, count the number of int digits and return that plus the scale.
	else len = bc_num_intDigits(n) + n->scale;

	return len;
}

void
bc_num_parse(BcNum* restrict n, const char* restrict val, BcBigDig base)
{
#ifndef NDEBUG
#if BC_ENABLE_LIBRARY
	BcVm* vm = bcl_getspecific();
#endif // BC_ENABLE_LIBRARY
#endif // NDEBUG

	assert(n != NULL && val != NULL && base);
	assert(base >= BC_NUM_MIN_BASE && base <= vm->maxes[BC_PROG_GLOBALS_IBASE]);
	assert(bc_num_strValid(val));

	// A one character number is *always* parsed as though the base was the
	// maximum allowed ibase, per the bc spec.
	if (!val[1])
	{
		BcBigDig dig = bc_num_parseChar(val[0], BC_NUM_MAX_LBASE);
		bc_num_bigdig2num(n, dig);
	}
	else if (base == BC_BASE) bc_num_parseDecimal(n, val);
	else bc_num_parseBase(n, val, base);

	assert(BC_NUM_RDX_VALID(n));
}

void
bc_num_print(BcNum* restrict n, BcBigDig base, bool newline)
{
	assert(n != NULL);
	assert(BC_ENABLE_EXTRA_MATH || base >= BC_NUM_MIN_BASE);

	// We may need a newline, just to start.
	bc_num_printNewline();

	if (BC_NUM_NONZERO(n))
	{
#if BC_ENABLE_LIBRARY
		BcVm* vm = bcl_getspecific();
#endif // BC_ENABLE_LIBRARY

		// Print the sign.
		if (BC_NUM_NEG(n)) bc_num_putchar('-', true);

		// Print the leading zero if necessary.
		if (BC_Z && BC_NUM_RDX_VAL(n) == n->len)
		{
			bc_num_printHex(0, 1, false, !newline);
		}
	}

	// Short-circuit 0.
	if (BC_NUM_ZERO(n)) bc_num_printHex(0, 1, false, !newline);
	else if (base == BC_BASE) bc_num_printDecimal(n, newline);
#if BC_ENABLE_EXTRA_MATH
	else if (base == 0 || base == 1)
	{
		bc_num_printExponent(n, base != 0, newline);
	}
#endif // BC_ENABLE_EXTRA_MATH
	else bc_num_printBase(n, base, newline);

	if (newline) bc_num_putchar('\n', false);
}

BcBigDig
bc_num_bigdig2(const BcNum* restrict n)
{
#ifndef NDEBUG
#if BC_ENABLE_LIBRARY
	BcVm* vm = bcl_getspecific();
#endif // BC_ENABLE_LIBRARY
#endif // NDEBUG

	// This function returns no errors because it's guaranteed to succeed if
	// its preconditions are met. Those preconditions include both n needs to
	// be non-NULL, n being non-negative, and n being less than vm->max. If all
	// of that is true, then we can just convert without worrying about negative
	// errors or overflow.

	BcBigDig r = 0;
	size_t nrdx = BC_NUM_RDX_VAL(n);

	assert(n != NULL);
	assert(!BC_NUM_NEG(n));
	assert(bc_num_cmp(n, &vm->max) < 0);
	assert(n->len - nrdx <= 3);

	// There is a small speed win from unrolling the loop here, and since it
	// only adds 53 bytes, I decided that it was worth it.
	switch (n->len - nrdx)
	{
		case 3:
		{
			r = (BcBigDig) n->num[nrdx + 2];

			// Fallthrough.
			BC_FALLTHROUGH
		}

		case 2:
		{
			r = r * BC_BASE_POW + (BcBigDig) n->num[nrdx + 1];

			// Fallthrough.
			BC_FALLTHROUGH
		}

		case 1:
		{
			r = r * BC_BASE_POW + (BcBigDig) n->num[nrdx];
		}
	}

	return r;
}

BcBigDig
bc_num_bigdig(const BcNum* restrict n)
{
#if BC_ENABLE_LIBRARY
	BcVm* vm = bcl_getspecific();
#endif // BC_ENABLE_LIBRARY

	assert(n != NULL);

	// This error checking is extremely important, and if you do not have a
	// guarantee that converting a number will always succeed in a particular
	// case, you *must* call this function to get these error checks. This
	// includes all instances of numbers inputted by the user or calculated by
	// the user. Otherwise, you can call the faster bc_num_bigdig2().
	if (BC_ERR(BC_NUM_NEG(n))) bc_err(BC_ERR_MATH_NEGATIVE);
	if (BC_ERR(bc_num_cmp(n, &vm->max) >= 0)) bc_err(BC_ERR_MATH_OVERFLOW);

	return bc_num_bigdig2(n);
}

void
bc_num_bigdig2num(BcNum* restrict n, BcBigDig val)
{
	BcDig* ptr;
	size_t i;

	assert(n != NULL);

	bc_num_zero(n);

	// Already 0.
	if (!val) return;

	// Expand first. This is the only way this function can fail, and it's a
	// fatal error.
	bc_num_expand(n, BC_NUM_BIGDIG_LOG10);

	// The conversion is easy because numbers are laid out in little-endian
	// order.
	for (ptr = n->num, i = 0; val; ++i, val /= BC_BASE_POW)
	{
		ptr[i] = val % BC_BASE_POW;
	}

	n->len = i;
}

#if BC_ENABLE_EXTRA_MATH

void
bc_num_rng(const BcNum* restrict n, BcRNG* rng)
{
	BcNum temp, temp2, intn, frac;
	BcRand state1, state2, inc1, inc2;
	size_t nrdx = BC_NUM_RDX_VAL(n);
#if BC_ENABLE_LIBRARY
	BcVm* vm = bcl_getspecific();
#endif // BC_ENABLE_LIBRARY

	// This function holds the secret of how I interpret a seed number for the
	// PRNG. Well, it's actually in the development manual
	// (manuals/development.md#pseudo-random-number-generator), so look there
	// before you try to understand this.

	BC_SIG_LOCK;

	bc_num_init(&temp, n->len);
	bc_num_init(&temp2, n->len);
	bc_num_init(&frac, nrdx);
	bc_num_init(&intn, bc_num_int(n));

	BC_SETJMP_LOCKED(vm, err);

	BC_SIG_UNLOCK;

	assert(BC_NUM_RDX_VALID_NP(vm->max));

	// NOLINTNEXTLINE
	memcpy(frac.num, n->num, BC_NUM_SIZE(nrdx));
	frac.len = nrdx;
	BC_NUM_RDX_SET_NP(frac, nrdx);
	frac.scale = n->scale;

	assert(BC_NUM_RDX_VALID_NP(frac));
	assert(BC_NUM_RDX_VALID_NP(vm->max2));

	// Multiply the fraction and truncate so that it's an integer. The
	// truncation is what clamps it, by the way.
	bc_num_mul(&frac, &vm->max2, &temp, 0);
	bc_num_truncate(&temp, temp.scale);
	bc_num_copy(&frac, &temp);

	// Get the integer.
	// NOLINTNEXTLINE
	memcpy(intn.num, n->num + nrdx, BC_NUM_SIZE(bc_num_int(n)));
	intn.len = bc_num_int(n);

	// This assert is here because it has to be true. It is also here to justify
	// some optimizations.
	assert(BC_NUM_NONZERO(&vm->max));

	// If there *was* a fractional part...
	if (BC_NUM_NONZERO(&frac))
	{
		// This divmod splits frac into the two state parts.
		bc_num_divmod(&frac, &vm->max, &temp, &temp2, 0);

		// frac is guaranteed to be smaller than vm->max * vm->max (pow).
		// This means that when dividing frac by vm->max, as above, the
		// quotient and remainder are both guaranteed to be less than vm->max,
		// which means we can use bc_num_bigdig2() here and not worry about
		// overflow.
		state1 = (BcRand) bc_num_bigdig2(&temp2);
		state2 = (BcRand) bc_num_bigdig2(&temp);
	}
	else state1 = state2 = 0;

	// If there *was* an integer part...
	if (BC_NUM_NONZERO(&intn))
	{
		// This divmod splits intn into the two inc parts.
		bc_num_divmod(&intn, &vm->max, &temp, &temp2, 0);

		// Because temp2 is the mod of vm->max, from above, it is guaranteed
		// to be small enough to use bc_num_bigdig2().
		inc1 = (BcRand) bc_num_bigdig2(&temp2);

		// Clamp the second inc part.
		if (bc_num_cmp(&temp, &vm->max) >= 0)
		{
			bc_num_copy(&temp2, &temp);
			bc_num_mod(&temp2, &vm->max, &temp, 0);
		}

		// The if statement above ensures that temp is less than vm->max, which
		// means that we can use bc_num_bigdig2() here.
		inc2 = (BcRand) bc_num_bigdig2(&temp);
	}
	else inc1 = inc2 = 0;

	bc_rand_seed(rng, state1, state2, inc1, inc2);

err:
	BC_SIG_MAYLOCK;
	bc_num_free(&intn);
	bc_num_free(&frac);
	bc_num_free(&temp2);
	bc_num_free(&temp);
	BC_LONGJMP_CONT(vm);
}

void
bc_num_createFromRNG(BcNum* restrict n, BcRNG* rng)
{
	BcRand s1, s2, i1, i2;
	BcNum conv, temp1, temp2, temp3;
	BcDig temp1_num[BC_RAND_NUM_SIZE], temp2_num[BC_RAND_NUM_SIZE];
	BcDig conv_num[BC_NUM_BIGDIG_LOG10];
#if BC_ENABLE_LIBRARY
	BcVm* vm = bcl_getspecific();
#endif // BC_ENABLE_LIBRARY

	BC_SIG_LOCK;

	bc_num_init(&temp3, 2 * BC_RAND_NUM_SIZE);

	BC_SETJMP_LOCKED(vm, err);

	BC_SIG_UNLOCK;

	bc_num_setup(&temp1, temp1_num, sizeof(temp1_num) / sizeof(BcDig));
	bc_num_setup(&temp2, temp2_num, sizeof(temp2_num) / sizeof(BcDig));
	bc_num_setup(&conv, conv_num, sizeof(conv_num) / sizeof(BcDig));

	// This assert is here because it has to be true. It is also here to justify
	// the assumption that vm->max is not zero.
	assert(BC_NUM_NONZERO(&vm->max));

	// Because this is true, we can just ignore math errors that would happen
	// otherwise.
	assert(BC_NUM_NONZERO(&vm->max2));

	bc_rand_getRands(rng, &s1, &s2, &i1, &i2);

	// Put the second piece of state into a number.
	bc_num_bigdig2num(&conv, (BcBigDig) s2);

	assert(BC_NUM_RDX_VALID_NP(conv));

	// Multiply by max to make room for the first piece of state.
	bc_num_mul(&conv, &vm->max, &temp1, 0);

	// Add in the first piece of state.
	bc_num_bigdig2num(&conv, (BcBigDig) s1);
	bc_num_add(&conv, &temp1, &temp2, 0);

	// Divide to make it an entirely fractional part.
	bc_num_div(&temp2, &vm->max2, &temp3, BC_RAND_STATE_BITS);

	// Now start on the increment parts. It's the same process without the
	// divide, so put the second piece of increment into a number.
	bc_num_bigdig2num(&conv, (BcBigDig) i2);

	assert(BC_NUM_RDX_VALID_NP(conv));

	// Multiply by max to make room for the first piece of increment.
	bc_num_mul(&conv, &vm->max, &temp1, 0);

	// Add in the first piece of increment.
	bc_num_bigdig2num(&conv, (BcBigDig) i1);
	bc_num_add(&conv, &temp1, &temp2, 0);

	// Now add the two together.
	bc_num_add(&temp2, &temp3, n, 0);

	assert(BC_NUM_RDX_VALID(n));

err:
	BC_SIG_MAYLOCK;
	bc_num_free(&temp3);
	BC_LONGJMP_CONT(vm);
}

void
bc_num_irand(BcNum* restrict a, BcNum* restrict b, BcRNG* restrict rng)
{
	BcNum atemp;
	size_t i, len;

	assert(a != b);

	if (BC_ERR(BC_NUM_NEG(a))) bc_err(BC_ERR_MATH_NEGATIVE);

	// If either of these are true, then the numbers are integers.
	if (BC_NUM_ZERO(a) || BC_NUM_ONE(a)) return;

#if BC_GCC
	// This is here in GCC to quiet the "maybe-uninitialized" warning.
	atemp.num = NULL;
	atemp.len = 0;
#endif // BC_GCC

	if (BC_ERR(bc_num_nonInt(a, &atemp))) bc_err(BC_ERR_MATH_NON_INTEGER);

	assert(atemp.num != NULL);
	assert(atemp.len);

	len = atemp.len - 1;

	// Just generate a random number for each limb.
	for (i = 0; i < len; ++i)
	{
		b->num[i] = (BcDig) bc_rand_bounded(rng, BC_BASE_POW);
	}

	// Do the last digit explicitly because the bound must be right. But only
	// do it if the limb does not equal 1. If it does, we have already hit the
	// limit.
	if (atemp.num[i] != 1)
	{
		b->num[i] = (BcDig) bc_rand_bounded(rng, (BcRand) atemp.num[i]);
		b->len = atemp.len;
	}
	// We want 1 less len in the case where we skip the last limb.
	else b->len = len;

	bc_num_clean(b);

	assert(BC_NUM_RDX_VALID(b));
}
#endif // BC_ENABLE_EXTRA_MATH

size_t
bc_num_addReq(const BcNum* a, const BcNum* b, size_t scale)
{
	size_t aint, bint, ardx, brdx;

	// Addition and subtraction require the max of the length of the two numbers
	// plus 1.

	BC_UNUSED(scale);

	ardx = BC_NUM_RDX_VAL(a);
	aint = bc_num_int(a);
	assert(aint <= a->len && ardx <= a->len);

	brdx = BC_NUM_RDX_VAL(b);
	bint = bc_num_int(b);
	assert(bint <= b->len && brdx <= b->len);

	ardx = BC_MAX(ardx, brdx);
	aint = BC_MAX(aint, bint);

	return bc_vm_growSize(bc_vm_growSize(ardx, aint), 1);
}

size_t
bc_num_mulReq(const BcNum* a, const BcNum* b, size_t scale)
{
	size_t max, rdx;

	// Multiplication requires the sum of the lengths of the numbers.

	rdx = bc_vm_growSize(BC_NUM_RDX_VAL(a), BC_NUM_RDX_VAL(b));

	max = BC_NUM_RDX(scale);

	max = bc_vm_growSize(BC_MAX(max, rdx), 1);
	rdx = bc_vm_growSize(bc_vm_growSize(bc_num_int(a), bc_num_int(b)), max);

	return rdx;
}

size_t
bc_num_divReq(const BcNum* a, const BcNum* b, size_t scale)
{
	size_t max, rdx;

	// Division requires the length of the dividend plus the scale.

	rdx = bc_vm_growSize(BC_NUM_RDX_VAL(a), BC_NUM_RDX_VAL(b));

	max = BC_NUM_RDX(scale);

	max = bc_vm_growSize(BC_MAX(max, rdx), 1);
	rdx = bc_vm_growSize(bc_num_int(a), max);

	return rdx;
}

size_t
bc_num_powReq(const BcNum* a, const BcNum* b, size_t scale)
{
	BC_UNUSED(scale);
	return bc_vm_growSize(bc_vm_growSize(a->len, b->len), 1);
}

#if BC_ENABLE_EXTRA_MATH
size_t
bc_num_placesReq(const BcNum* a, const BcNum* b, size_t scale)
{
	BC_UNUSED(scale);
	return a->len + b->len - BC_NUM_RDX_VAL(a) - BC_NUM_RDX_VAL(b);
}
#endif // BC_ENABLE_EXTRA_MATH

void
bc_num_add(BcNum* a, BcNum* b, BcNum* c, size_t scale)
{
	assert(BC_NUM_RDX_VALID(a));
	assert(BC_NUM_RDX_VALID(b));
	bc_num_binary(a, b, c, false, bc_num_as, bc_num_addReq(a, b, scale));
}

void
bc_num_sub(BcNum* a, BcNum* b, BcNum* c, size_t scale)
{
	assert(BC_NUM_RDX_VALID(a));
	assert(BC_NUM_RDX_VALID(b));
	bc_num_binary(a, b, c, true, bc_num_as, bc_num_addReq(a, b, scale));
}

void
bc_num_mul(BcNum* a, BcNum* b, BcNum* c, size_t scale)
{
	assert(BC_NUM_RDX_VALID(a));
	assert(BC_NUM_RDX_VALID(b));
	bc_num_binary(a, b, c, scale, bc_num_m, bc_num_mulReq(a, b, scale));
}

void
bc_num_div(BcNum* a, BcNum* b, BcNum* c, size_t scale)
{
	assert(BC_NUM_RDX_VALID(a));
	assert(BC_NUM_RDX_VALID(b));
	bc_num_binary(a, b, c, scale, bc_num_d, bc_num_divReq(a, b, scale));
}

void
bc_num_mod(BcNum* a, BcNum* b, BcNum* c, size_t scale)
{
	assert(BC_NUM_RDX_VALID(a));
	assert(BC_NUM_RDX_VALID(b));
	bc_num_binary(a, b, c, scale, bc_num_rem, bc_num_divReq(a, b, scale));
}

void
bc_num_pow(BcNum* a, BcNum* b, BcNum* c, size_t scale)
{
	assert(BC_NUM_RDX_VALID(a));
	assert(BC_NUM_RDX_VALID(b));
	bc_num_binary(a, b, c, scale, bc_num_p, bc_num_powReq(a, b, scale));
}

#if BC_ENABLE_EXTRA_MATH
void
bc_num_places(BcNum* a, BcNum* b, BcNum* c, size_t scale)
{
	assert(BC_NUM_RDX_VALID(a));
	assert(BC_NUM_RDX_VALID(b));
	bc_num_binary(a, b, c, scale, bc_num_place, bc_num_placesReq(a, b, scale));
}

void
bc_num_lshift(BcNum* a, BcNum* b, BcNum* c, size_t scale)
{
	assert(BC_NUM_RDX_VALID(a));
	assert(BC_NUM_RDX_VALID(b));
	bc_num_binary(a, b, c, scale, bc_num_left, bc_num_placesReq(a, b, scale));
}

void
bc_num_rshift(BcNum* a, BcNum* b, BcNum* c, size_t scale)
{
	assert(BC_NUM_RDX_VALID(a));
	assert(BC_NUM_RDX_VALID(b));
	bc_num_binary(a, b, c, scale, bc_num_right, bc_num_placesReq(a, b, scale));
}
#endif // BC_ENABLE_EXTRA_MATH

void
bc_num_sqrt(BcNum* restrict a, BcNum* restrict b, size_t scale)
{
	BcNum num1, num2, half, f, fprime;
	BcNum* x0;
	BcNum* x1;
	BcNum* temp;
	// realscale is meant to quiet a warning on GCC about longjmp() clobbering.
	// This one is real.
	size_t pow, len, rdx, req, resscale, realscale;
	BcDig half_digs[1];
#if BC_ENABLE_LIBRARY
	BcVm* vm = bcl_getspecific();
#endif // BC_ENABLE_LIBRARY

	assert(a != NULL && b != NULL && a != b);

	if (BC_ERR(BC_NUM_NEG(a))) bc_err(BC_ERR_MATH_NEGATIVE);

	// We want to calculate to a's scale if it is bigger so that the result will
	// truncate properly.
	if (a->scale > scale) realscale = a->scale;
	else realscale = scale;

	// Set parameters for the result.
	len = bc_vm_growSize(bc_num_intDigits(a), 1);
	rdx = BC_NUM_RDX(realscale);

	// Square root needs half of the length of the parameter.
	req = bc_vm_growSize(BC_MAX(rdx, BC_NUM_RDX_VAL(a)), len >> 1);

	BC_SIG_LOCK;

	// Unlike the binary operators, this function is the only single parameter
	// function and is expected to initialize the result. This means that it
	// expects that b is *NOT* preallocated. We allocate it here.
	bc_num_init(b, bc_vm_growSize(req, 1));

	BC_SIG_UNLOCK;

	assert(a != NULL && b != NULL && a != b);
	assert(a->num != NULL && b->num != NULL);

	// Easy case.
	if (BC_NUM_ZERO(a))
	{
		bc_num_setToZero(b, realscale);
		return;
	}

	// Another easy case.
	if (BC_NUM_ONE(a))
	{
		bc_num_one(b);
		bc_num_extend(b, realscale);
		return;
	}

	// Set the parameters again.
	rdx = BC_NUM_RDX(realscale);
	rdx = BC_MAX(rdx, BC_NUM_RDX_VAL(a));
	len = bc_vm_growSize(a->len, rdx);

	BC_SIG_LOCK;

	bc_num_init(&num1, len);
	bc_num_init(&num2, len);
	bc_num_setup(&half, half_digs, sizeof(half_digs) / sizeof(BcDig));

	// There is a division by two in the formula. We setup a number that's 1/2
	// so that we can use multiplication instead of heavy division.
	bc_num_one(&half);
	half.num[0] = BC_BASE_POW / 2;
	half.len = 1;
	BC_NUM_RDX_SET_NP(half, 1);
	half.scale = 1;

	bc_num_init(&f, len);
	bc_num_init(&fprime, len);

	BC_SETJMP_LOCKED(vm, err);

	BC_SIG_UNLOCK;

	// Pointers for easy switching.
	x0 = &num1;
	x1 = &num2;

	// Start with 1.
	bc_num_one(x0);

	// The power of the operand is needed for the estimate.
	pow = bc_num_intDigits(a);

	// The code in this if statement calculates the initial estimate. First, if
	// a is less than 0, then 0 is a good estimate. Otherwise, we want something
	// in the same ballpark. That ballpark is pow.
	if (pow)
	{
		// An odd number is served by starting with 2^((pow-1)/2), and an even
		// number is served by starting with 6^((pow-2)/2). Why? Because math.
		if (pow & 1) x0->num[0] = 2;
		else x0->num[0] = 6;

		pow -= 2 - (pow & 1);
		bc_num_shiftLeft(x0, pow / 2);
	}

	// I can set the rdx here directly because neg should be false.
	x0->scale = x0->rdx = 0;
	resscale = (realscale + BC_BASE_DIGS) + 2;

	// This is the calculation loop. This compare goes to 0 eventually as the
	// difference between the two numbers gets smaller than resscale.
	while (bc_num_cmp(x1, x0))
	{
		assert(BC_NUM_NONZERO(x0));

		// This loop directly corresponds to the iteration in Newton's method.
		// If you know the formula, this loop makes sense. Go study the formula.

		bc_num_div(a, x0, &f, resscale);
		bc_num_add(x0, &f, &fprime, resscale);

		assert(BC_NUM_RDX_VALID_NP(fprime));
		assert(BC_NUM_RDX_VALID_NP(half));

		bc_num_mul(&fprime, &half, x1, resscale);

		// Switch.
		temp = x0;
		x0 = x1;
		x1 = temp;
	}

	// Copy to the result and truncate.
	bc_num_copy(b, x0);
	if (b->scale > realscale) bc_num_truncate(b, b->scale - realscale);

	assert(!BC_NUM_NEG(b) || BC_NUM_NONZERO(b));
	assert(BC_NUM_RDX_VALID(b));
	assert(BC_NUM_RDX_VAL(b) <= b->len || !b->len);
	assert(!b->len || b->num[b->len - 1] || BC_NUM_RDX_VAL(b) == b->len);

err:
	BC_SIG_MAYLOCK;
	bc_num_free(&fprime);
	bc_num_free(&f);
	bc_num_free(&num2);
	bc_num_free(&num1);
	BC_LONGJMP_CONT(vm);
}

void
bc_num_divmod(BcNum* a, BcNum* b, BcNum* c, BcNum* d, size_t scale)
{
	size_t ts, len;
	BcNum *ptr_a, num2;
	// This is volatile to quiet a warning on GCC about clobbering with
	// longjmp().
	volatile bool init = false;
#if BC_ENABLE_LIBRARY
	BcVm* vm = bcl_getspecific();
#endif // BC_ENABLE_LIBRARY

	// The bulk of this function is just doing what bc_num_binary() does for the
	// binary operators. However, it assumes that only c and a can be equal.

	// Set up the parameters.
	ts = BC_MAX(scale + b->scale, a->scale);
	len = bc_num_mulReq(a, b, ts);

	assert(a != NULL && b != NULL && c != NULL && d != NULL);
	assert(c != d && a != d && b != d && b != c);

	// Initialize or expand as necessary.
	if (c == a)
	{
		// NOLINTNEXTLINE
		memcpy(&num2, c, sizeof(BcNum));
		ptr_a = &num2;

		BC_SIG_LOCK;

		bc_num_init(c, len);

		init = true;

		BC_SETJMP_LOCKED(vm, err);

		BC_SIG_UNLOCK;
	}
	else
	{
		ptr_a = a;
		bc_num_expand(c, len);
	}

	// Do the quick version if possible.
	if (BC_NUM_NONZERO(a) && !BC_NUM_RDX_VAL(a) && !BC_NUM_RDX_VAL(b) &&
	    b->len == 1 && !scale)
	{
		BcBigDig rem;

		bc_num_divArray(ptr_a, (BcBigDig) b->num[0], c, &rem);

		assert(rem < BC_BASE_POW);

		d->num[0] = (BcDig) rem;
		d->len = (rem != 0);
	}
	// Do the slow method.
	else bc_num_r(ptr_a, b, c, d, scale, ts);

	assert(!BC_NUM_NEG(c) || BC_NUM_NONZERO(c));
	assert(BC_NUM_RDX_VALID(c));
	assert(BC_NUM_RDX_VAL(c) <= c->len || !c->len);
	assert(!c->len || c->num[c->len - 1] || BC_NUM_RDX_VAL(c) == c->len);
	assert(!BC_NUM_NEG(d) || BC_NUM_NONZERO(d));
	assert(BC_NUM_RDX_VALID(d));
	assert(BC_NUM_RDX_VAL(d) <= d->len || !d->len);
	assert(!d->len || d->num[d->len - 1] || BC_NUM_RDX_VAL(d) == d->len);

err:
	// Only cleanup if we initialized.
	if (init)
	{
		BC_SIG_MAYLOCK;
		bc_num_free(&num2);
		BC_LONGJMP_CONT(vm);
	}
}

void
bc_num_modexp(BcNum* a, BcNum* b, BcNum* c, BcNum* restrict d)
{
	BcNum base, exp, two, temp, atemp, btemp, ctemp;
	BcDig two_digs[2];
#if BC_ENABLE_LIBRARY
	BcVm* vm = bcl_getspecific();
#endif // BC_ENABLE_LIBRARY

	assert(a != NULL && b != NULL && c != NULL && d != NULL);
	assert(a != d && b != d && c != d);

	if (BC_ERR(BC_NUM_ZERO(c))) bc_err(BC_ERR_MATH_DIVIDE_BY_ZERO);
	if (BC_ERR(BC_NUM_NEG(b))) bc_err(BC_ERR_MATH_NEGATIVE);

#ifndef NDEBUG
	// This is entirely for quieting a useless scan-build error.
	btemp.len = 0;
	ctemp.len = 0;
#endif // NDEBUG

	// Eliminate fractional parts that are zero or error if they are not zero.
	if (BC_ERR(bc_num_nonInt(a, &atemp) || bc_num_nonInt(b, &btemp) ||
	           bc_num_nonInt(c, &ctemp)))
	{
		bc_err(BC_ERR_MATH_NON_INTEGER);
	}

	bc_num_expand(d, ctemp.len);

	BC_SIG_LOCK;

	bc_num_init(&base, ctemp.len);
	bc_num_setup(&two, two_digs, sizeof(two_digs) / sizeof(BcDig));
	bc_num_init(&temp, btemp.len + 1);
	bc_num_createCopy(&exp, &btemp);

	BC_SETJMP_LOCKED(vm, err);

	BC_SIG_UNLOCK;

	bc_num_one(&two);
	two.num[0] = 2;
	bc_num_one(d);

	// We already checked for 0.
	bc_num_rem(&atemp, &ctemp, &base, 0);

	// If you know the algorithm I used, the memory-efficient method, then this
	// loop should be self-explanatory because it is the calculation loop.
	while (BC_NUM_NONZERO(&exp))
	{
		// Num two cannot be 0, so no errors.
		bc_num_divmod(&exp, &two, &exp, &temp, 0);

		if (BC_NUM_ONE(&temp) && !BC_NUM_NEG_NP(temp))
		{
			assert(BC_NUM_RDX_VALID(d));
			assert(BC_NUM_RDX_VALID_NP(base));

			bc_num_mul(d, &base, &temp, 0);

			// We already checked for 0.
			bc_num_rem(&temp, &ctemp, d, 0);
		}

		assert(BC_NUM_RDX_VALID_NP(base));

		bc_num_mul(&base, &base, &temp, 0);

		// We already checked for 0.
		bc_num_rem(&temp, &ctemp, &base, 0);
	}

err:
	BC_SIG_MAYLOCK;
	bc_num_free(&exp);
	bc_num_free(&temp);
	bc_num_free(&base);
	BC_LONGJMP_CONT(vm);
	assert(!BC_NUM_NEG(d) || d->len);
	assert(BC_NUM_RDX_VALID(d));
	assert(!d->len || d->num[d->len - 1] || BC_NUM_RDX_VAL(d) == d->len);
}

#if BC_DEBUG_CODE
void
bc_num_printDebug(const BcNum* n, const char* name, bool emptyline)
{
	bc_file_puts(&vm->fout, bc_flush_none, name);
	bc_file_puts(&vm->fout, bc_flush_none, ": ");
	bc_num_printDecimal(n, true);
	bc_file_putchar(&vm->fout, bc_flush_err, '\n');
	if (emptyline) bc_file_putchar(&vm->fout, bc_flush_err, '\n');
	vm->nchars = 0;
}

void
bc_num_printDigs(const BcDig* n, size_t len, bool emptyline)
{
	size_t i;

	for (i = len - 1; i < len; --i)
	{
		bc_file_printf(&vm->fout, " %lu", (unsigned long) n[i]);
	}

	bc_file_putchar(&vm->fout, bc_flush_err, '\n');
	if (emptyline) bc_file_putchar(&vm->fout, bc_flush_err, '\n');
	vm->nchars = 0;
}

void
bc_num_printWithDigs(const BcNum* n, const char* name, bool emptyline)
{
	bc_file_puts(&vm->fout, bc_flush_none, name);
	bc_file_printf(&vm->fout, " len: %zu, rdx: %zu, scale: %zu\n", name, n->len,
	               BC_NUM_RDX_VAL(n), n->scale);
	bc_num_printDigs(n->num, n->len, emptyline);
}

void
bc_num_dump(const char* varname, const BcNum* n)
{
	ulong i, scale = n->scale;

	bc_file_printf(&vm->ferr, "\n%s = %s", varname,
	               n->len ? (BC_NUM_NEG(n) ? "-" : "+") : "0 ");

	for (i = n->len - 1; i < n->len; --i)
	{
		if (i + 1 == BC_NUM_RDX_VAL(n))
		{
			bc_file_puts(&vm->ferr, bc_flush_none, ". ");
		}

		if (scale / BC_BASE_DIGS != BC_NUM_RDX_VAL(n) - i - 1)
		{
			bc_file_printf(&vm->ferr, "%lu ", (unsigned long) n->num[i]);
		}
		else
		{
			int mod = scale % BC_BASE_DIGS;
			int d = BC_BASE_DIGS - mod;
			BcDig div;

			if (mod != 0)
			{
				div = n->num[i] / ((BcDig) bc_num_pow10[(ulong) d]);
				bc_file_printf(&vm->ferr, "%lu", (unsigned long) div);
			}

			div = n->num[i] % ((BcDig) bc_num_pow10[(ulong) d]);
			bc_file_printf(&vm->ferr, " ' %lu ", (unsigned long) div);
		}
	}

	bc_file_printf(&vm->ferr, "(%zu | %zu.%zu / %zu) %lu\n", n->scale, n->len,
	               BC_NUM_RDX_VAL(n), n->cap, (unsigned long) (void*) n->num);

	bc_file_flush(&vm->ferr, bc_flush_err);
}
#endif // BC_DEBUG_CODE
