/*
 *  Copyright (C) 2017 - This file is part of libecc project
 *
 *  Authors:
 *      Ryad BENADJILA <ryadbenadjila@gmail.com>
 *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *      Jean-Pierre FLORI <jean-pierre.flori@ssi.gouv.fr>
 *
 *  Contributors:
 *      Nicolas VIVET <nicolas.vivet@ssi.gouv.fr>
 *      Karim KHALFALLAH <karim.khalfallah@ssi.gouv.fr>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */
#include <libecc/nn/nn_mul_public.h>
#include <libecc/nn/nn_logical.h>
#include <libecc/nn/nn_add.h>
#include <libecc/nn/nn.h>
/* Use internal API header */
#include "nn_div.h"

/*
 * Some helper functions to perform operations on an arbitrary part
 * of a multiprecision number.
 * This is exactly the same code as for operations on the least significant
 * part of a multiprecision number except for the starting point in the
 * array representing it.
 * Done in *constant time*.
 *
 * Operations producing an output are in place.
 */

/*
 * Compare all the bits of in2 with the same number of bits in in1 starting at
 * 'shift' position in in1. in1 must be long enough for that purpose, i.e.
 * in1->wlen >= (in2->wlen + shift). The comparison value is provided in
 * 'cmp' parameter. The function returns 0 on success, -1 on error.
 *
 * The function is an internal helper; it expects initialized nn in1 and
 * in2: it does not verify that.
 */
ATTRIBUTE_WARN_UNUSED_RET static int _nn_cmp_shift(nn_src_t in1, nn_src_t in2, u8 shift, int *cmp)
{
	int ret, mask, tmp;
	u8 i;

	MUST_HAVE((in1->wlen >= (in2->wlen + shift)), ret, err);
	MUST_HAVE((cmp != NULL), ret, err);

	tmp = 0;
	for (i = in2->wlen; i > 0; i--) {
		mask = (!(tmp & 0x1));
		tmp += ((in1->val[shift + i - 1] > in2->val[i - 1]) & mask);
		tmp -= ((in1->val[shift + i - 1] < in2->val[i - 1]) & mask);
	}
	(*cmp) = tmp;
	ret = 0;

err:
	return ret;
}

/*
 * Conditionally subtract a shifted version of in from out, i.e.:
 *   - if cnd == 1, out <- out - (in << shift)
 *   - if cnd == 0, out <- out
 * The function returns 0 on success, -1 on error. On success, 'borrow'
 * provides the possible borrow resulting from the subtraction. 'borrow'
 * is not meaningful on error.
 *
 * The function is an internal helper; it expects initialized nn out and
 * in: it does not verify that.
 */
ATTRIBUTE_WARN_UNUSED_RET static int _nn_cnd_sub_shift(int cnd, nn_t out, nn_src_t in,
			    u8 shift, word_t *borrow)
{
	word_t tmp, borrow1, borrow2, _borrow = WORD(0);
	word_t mask = WORD_MASK_IFNOTZERO(cnd);
	int ret;
	u8 i;

	MUST_HAVE((out->wlen >= (in->wlen + shift)), ret, err);
	MUST_HAVE((borrow != NULL), ret, err);

	/*
	 *  Perform subtraction one word at a time,
	 *  propagating the borrow.
	 */
	for (i = 0; i < in->wlen; i++) {
		tmp = (word_t)(out->val[shift + i] - (in->val[i] & mask));
		borrow1 = (word_t)(tmp > out->val[shift + i]);
		out->val[shift + i] = (word_t)(tmp - _borrow);
		borrow2 = (word_t)(out->val[shift + i] > tmp);
		/* There is at most one borrow going out. */
		_borrow = (word_t)(borrow1 | borrow2);
	}

	(*borrow) = _borrow;
	ret = 0;

err:
	return ret;
}

/*
 * Subtract a shifted version of 'in' multiplied by 'w' from 'out' and return
 * borrow. The function returns 0 on success, -1 on error. 'borrow' is
 * meaningful only on success.
 *
 * The function is an internal helper; it expects initialized nn out and
 * in: it does not verify that.
 */
ATTRIBUTE_WARN_UNUSED_RET static int _nn_submul_word_shift(nn_t out, nn_src_t in, word_t w, u8 shift,
				word_t *borrow)
{
	word_t _borrow = WORD(0), prod_high, prod_low, tmp;
	int ret;
	u8 i;

	MUST_HAVE((out->wlen >= (in->wlen + shift)), ret, err);
	MUST_HAVE((borrow != NULL), ret, err);

	for (i = 0; i < in->wlen; i++) {
		/*
		 * Compute the result of the multiplication of
		 * two words.
		 */
		WORD_MUL(prod_high, prod_low, in->val[i], w);

		/*
		 * And add previous borrow.
		 */
		prod_low = (word_t)(prod_low + _borrow);
		prod_high = (word_t)(prod_high + (prod_low < _borrow));

		/*
		 * Subtract computed word at current position in result.
		 */
		tmp = (word_t)(out->val[shift + i] - prod_low);
		_borrow = (word_t)(prod_high + (tmp > out->val[shift + i]));
		out->val[shift + i] = tmp;
	}

	(*borrow) = _borrow;
	ret = 0;

err:
	return ret;
}

/*
 * Compute quotient 'q' and remainder 'r' of Euclidean division of 'a' by 'b'
 * (i.e. q and r such that a = b*q + r). 'q' and 'r' are not normalized on
 * return. * Computation are performed in *constant time*, only depending on
 * the lengths of 'a' and 'b', but not on the values of 'a' and 'b'.
 *
 * This uses the above function to perform arithmetic on arbitrary parts
 * of multiprecision numbers.
 *
 * The algorithm used is schoolbook division:
 * + the quotient is computed word by word,
 * + a small division of the MSW is performed to obtain an
 *   approximation of the MSW of the quotient,
 * + the approximation is corrected to obtain the correct
 *   multiprecision MSW of the quotient,
 * + the corresponding product is subtracted from the dividend,
 * + the same procedure is used for the following word of the quotient.
 *
 * It is assumed that:
 * + b is normalized: the MSB of its MSW is 1,
 * + the most significant part of a is smaller than b,
 * + a precomputed reciprocal
 *     v = floor(B^3/(d+1)) - B
 *   where d is the MSW of the (normalized) divisor
 *   is given to perform the small 3-by-2 division.
 * + using this reciprocal, the approximated quotient is always
 *   too small and at most one multiprecision correction is needed.
 *
 * It returns 0 on sucess, -1 on error.
 *
 * CAUTION:
 *
 * - The function is expected to be used ONLY by the generic version
 *   nn_divrem_normalized() defined later in the file.
 * - All parameters must have been initialized. Unlike exported/public
 *   functions, this internal helper does not verify that nn parameters
 *   have been initialized. Again, this is expected from the caller
 *   (nn_divrem_normalized()).
 * - The function does not support aliasing of 'b' or 'q'. See
 *   _nn_divrem_normalized_aliased() for such a wrapper.
 *
 */
ATTRIBUTE_WARN_UNUSED_RET static int _nn_divrem_normalized(nn_t q, nn_t r,
				 nn_src_t a, nn_src_t b, word_t v)
{
	word_t borrow, qstar, qh, ql, rh, rl; /* for 3-by-2 div. */
	int _small, cmp, ret;
	u8 i;

	MUST_HAVE(!(b->wlen <= 0), ret, err);
	MUST_HAVE(!(a->wlen <= b->wlen), ret, err);
	MUST_HAVE(!(!((b->val[b->wlen - 1] >> (WORD_BITS - 1)) == WORD(1))), ret, err);
	MUST_HAVE(!_nn_cmp_shift(a, b, (u8)(a->wlen - b->wlen), &cmp) && (cmp < 0), ret, err);

	/* Handle trivial aliasing for a and r */
	if (r != a) {
		ret = nn_set_wlen(r, a->wlen); EG(ret, err);
		ret = nn_copy(r, a); EG(ret, err);
	}

	ret = nn_set_wlen(q, (u8)(r->wlen - b->wlen)); EG(ret, err);

	/*
	 * Compute subsequent words of the quotient one by one.
	 * Perform approximate 3-by-2 division using the precomputed
	 * reciprocal and correct afterward.
	 */
	for (i = r->wlen; i > b->wlen; i--) {
		u8 shift = (u8)(i - b->wlen - 1);

		/*
		 * Perform 3-by-2 approximate division:
		 * <qstar, qh, ql> = <rh, rl> * (v + B)
		 * We are only interested in qstar.
		 */
		rh = r->val[i - 1];
		rl = r->val[i - 2];
		/* Perform 2-by-1 multiplication. */
		WORD_MUL(qh, ql, rl, v);
		WORD_MUL(qstar, ql, rh, v);
		/* And propagate carries. */
		qh = (word_t)(qh + ql);
		qstar = (word_t)(qstar + (qh < ql));
		qh = (word_t)(qh + rl);
		rh = (word_t)(rh + (qh < rl));
		qstar = (word_t)(qstar + rh);

		/*
		 * Compute approximate quotient times divisor
		 * and subtract it from remainder:
		 * r = r - (b*qstar << B^shift)
		 */
		ret = _nn_submul_word_shift(r, b, qstar, shift, &borrow); EG(ret, err);

		/* Check the approximate quotient was indeed not too large. */
		MUST_HAVE(!(r->val[i - 1] < borrow), ret, err);
		r->val[i - 1] = (word_t)(r->val[i - 1] - borrow);

		/*
		 * Check whether the approximate quotient was too small or not.
		 * At most one multiprecision correction is needed.
		 */
		ret = _nn_cmp_shift(r, b, shift, &cmp); EG(ret, err);
		_small = ((!!(r->val[i - 1])) | (cmp >= 0));
		/* Perform conditional multiprecision correction. */
		ret = _nn_cnd_sub_shift(_small, r, b, shift, &borrow); EG(ret, err);
		MUST_HAVE(!(r->val[i - 1] != borrow), ret, err);
		r->val[i - 1] = (word_t)(r->val[i - 1] - borrow);
		/*
		 * Adjust the quotient if it was too small and set it in the
		 * multiprecision array.
		 */
		qstar = (word_t)(qstar + (word_t)_small);
		q->val[shift] = qstar;
		/*
		 * Check that the MSW of remainder was cancelled out and that
		 * we could not increase the quotient anymore.
		 */
		MUST_HAVE(!(r->val[r->wlen - 1] != WORD(0)), ret, err);

		ret = _nn_cmp_shift(r, b, shift, &cmp); EG(ret, err);
		MUST_HAVE(!(cmp >= 0), ret, err);

		ret = nn_set_wlen(r, (u8)(r->wlen - 1)); EG(ret, err);
	}

err:
	return ret;
}

/*
 * Compute quotient 'q' and remainder 'r' of Euclidean division of 'a' by 'b'
 * (i.e. q and r such that a = b*q + r). 'q' and 'r' are not normalized.
 * Compared to _nn_divrem_normalized(), this internal version
 * explicitly handle the case where 'b' and 'r' point to the same nn (i.e. 'r'
 * result is stored in 'b' on success), hence the removal of 'r' parameter from
 * function prototype compared to _nn_divrem_normalized().
 *
 * The computation is performed in *constant time*, see documentation of
 * _nn_divrem_normalized().
 *
 * Assume that 'b' is normalized (the MSB of its MSW is set), that 'v' is the
 * reciprocal of the MSW of 'b' and that the high part of 'a' is smaller than
 * 'b'.
 *
 * The function returns 0 on success, -1 on error.
 *
 * CAUTION:
 *
 * - The function is expected to be used ONLY by the generic version
 *   nn_divrem_normalized() defined later in the file.
 * - All parameters must have been initialized. Unlike exported/public
 *   functions, this internal helper does not verify that nn parameters
 *   have been initialized. Again, this is expected from the caller
 *   (nn_divrem_normalized()).
 * - The function does not support aliasing of 'a' or 'q'.
 *
 */
ATTRIBUTE_WARN_UNUSED_RET static int _nn_divrem_normalized_aliased(nn_t q, nn_src_t a, nn_t b, word_t v)
{
	int ret;
	nn r;
	r.magic = WORD(0);

	ret = nn_init(&r, 0); EG(ret, err);
	ret = _nn_divrem_normalized(q, &r, a, b, v); EG(ret, err);
	ret = nn_copy(b, &r); EG(ret, err);

err:
	nn_uninit(&r);
	return ret;
}

/*
 * Compute quotient and remainder of Euclidean division, and do not normalize
 * them. Done in *constant time*, see documentation of _nn_divrem_normalized().
 *
 * Assume that 'b' is normalized (the MSB of its MSW is set), that 'v' is the
 * reciprocal of the MSW of 'b' and that the high part of 'a' is smaller than
 * 'b'.
 *
 * Aliasing is supported for 'r' only (with 'b'), i.e. 'r' and 'b' can point
 * to the same nn.
 *
 * The function returns 0 on success, -1 on error.
 */
int nn_divrem_normalized(nn_t q, nn_t r, nn_src_t a, nn_src_t b, word_t v)
{
	int ret;

	ret = nn_check_initialized(a); EG(ret, err);
	ret = nn_check_initialized(q); EG(ret, err);
	ret = nn_check_initialized(r); EG(ret, err);

	/* Unsupported aliasings */
	MUST_HAVE((q != r) && (q != a) && (q != b), ret, err);

	if (r == b) {
		ret = _nn_divrem_normalized_aliased(q, a, r, v);
	} else {
		ret = nn_check_initialized(b); EG(ret, err);
		ret = _nn_divrem_normalized(q, r, a, b, v);
	}

err:
	return ret;
}

/*
 * Compute remainder only and do not normalize it.
 * Constant time, see documentation of _nn_divrem_normalized.
 *
 * Support aliasing of inputs and outputs.
 *
 * The function returns 0 on success, -1 on error.
 */
int nn_mod_normalized(nn_t r, nn_src_t a, nn_src_t b, word_t v)
{
	int ret;
	nn q;
	q.magic = WORD(0);

	ret = nn_init(&q, 0); EG(ret, err);
	ret = nn_divrem_normalized(&q, r, a, b, v);

err:
	nn_uninit(&q);
	return ret;
}

/*
 * Compute quotient and remainder of Euclidean division,
 * and do not normalize them.
 * Done in *constant time*,
 * only depending on the lengths of 'a' and 'b' and the value of 'cnt',
 * but not on the values of 'a' and 'b'.
 *
 * Assume that b has been normalized by a 'cnt' bit shift,
 * that v is the reciprocal of the MSW of 'b',
 * but a is not shifted yet.
 * Useful when multiple multiplication by the same b are performed,
 * e.g. at the fp level.
 *
 * All outputs MUST have been initialized. The function does not support
 * aliasing of 'b' or 'q'. It returns 0 on success, -1 on error.
 *
 * CAUTION:
 *
 * - The function is expected to be used ONLY by the generic version
 *   nn_divrem_normalized() defined later in the file.
 * - All parameters must have been initialized. Unlike exported/public
 *   functions, this internal helper does not verify that
 *   have been initialized. Again, this is expected from the caller
 *   (nn_divrem_unshifted()).
 * - The function does not support aliasing. See
 *   _nn_divrem_unshifted_aliased() for such a wrapper.
 */
ATTRIBUTE_WARN_UNUSED_RET static int _nn_divrem_unshifted(nn_t q, nn_t r, nn_src_t a, nn_src_t b_norm,
				word_t v, bitcnt_t cnt)
{
	nn a_shift;
	u8 new_wlen, b_wlen;
	int larger, ret, cmp;
	word_t borrow;
	a_shift.magic = WORD(0);

	/* Avoid overflow */
	MUST_HAVE(((a->wlen + BIT_LEN_WORDS(cnt)) < NN_MAX_WORD_LEN), ret, err);

	/* We now know that new_wlen will fit in an u8 */
	new_wlen = (u8)(a->wlen + (u8)BIT_LEN_WORDS(cnt));

	b_wlen = b_norm->wlen;
	if (new_wlen < b_wlen) { /* trivial case */
		ret = nn_copy(r, a); EG(ret, err);
		ret = nn_zero(q);
		goto err;
	}

	/* Shift a. */
	ret = nn_init(&a_shift, (u16)(new_wlen * WORD_BYTES)); EG(ret, err);
	ret = nn_set_wlen(&a_shift, new_wlen); EG(ret, err);
	ret = nn_lshift_fixedlen(&a_shift, a, cnt); EG(ret, err);
	ret = nn_set_wlen(r, new_wlen); EG(ret, err);

	if (new_wlen == b_wlen) {
		/* Ensure that a is smaller than b. */
		ret = nn_cmp(&a_shift, b_norm, &cmp); EG(ret, err);
		larger = (cmp >= 0);
		ret = nn_cnd_sub(larger, r, &a_shift, b_norm); EG(ret, err);
		MUST_HAVE(((!nn_cmp(r, b_norm, &cmp)) && (cmp < 0)), ret, err);

		/* Set MSW of quotient. */
		ret = nn_set_wlen(q, (u8)(new_wlen - b_wlen + 1)); EG(ret, err);
		q->val[new_wlen - b_wlen] = (word_t) larger;
		/* And we are done as the quotient is 0 or 1. */
	} else if (new_wlen > b_wlen) {
		/* Ensure that most significant part of a is smaller than b. */
		ret = _nn_cmp_shift(&a_shift, b_norm, (u8)(new_wlen - b_wlen), &cmp); EG(ret, err);
		larger = (cmp >= 0);
		ret = _nn_cnd_sub_shift(larger, &a_shift, b_norm, (u8)(new_wlen - b_wlen), &borrow); EG(ret, err);
		MUST_HAVE(((!_nn_cmp_shift(&a_shift, b_norm, (u8)(new_wlen - b_wlen), &cmp)) && (cmp < 0)), ret, err);

		/*
		 * Perform division with MSP of a smaller than b. This ensures
		 * that the quotient is of length a_len - b_len.
		 */
		ret = nn_divrem_normalized(q, r, &a_shift, b_norm, v); EG(ret, err);

		/* Set MSW of quotient. */
		ret = nn_set_wlen(q, (u8)(new_wlen - b_wlen + 1)); EG(ret, err);
		q->val[new_wlen - b_wlen] = (word_t) larger;
	} /* else a is smaller than b... treated above. */

	ret = nn_rshift_fixedlen(r, r, cnt); EG(ret, err);
	ret = nn_set_wlen(r, b_wlen);

err:
	nn_uninit(&a_shift);

	return ret;
}

/*
 * Same as previous but handling aliasing of 'r' with 'b_norm', i.e. on success,
 * result 'r' is passed through 'b_norm'.
 *
 * CAUTION:
 *
 * - The function is expected to be used ONLY by the generic version
 *   nn_divrem_normalized() defined later in the file.
 * - All parameter must have been initialized. Unlike exported/public
 *   functions, this internal helper does not verify that nn parameters
 *   have been initialized. Again, this is expected from the caller
 *   (nn_divrem_unshifted()).
 */
ATTRIBUTE_WARN_UNUSED_RET static int _nn_divrem_unshifted_aliased(nn_t q, nn_src_t a, nn_t b_norm,
					word_t v, bitcnt_t cnt)
{
	int ret;
	nn r;
	r.magic = WORD(0);

	ret = nn_init(&r, 0); EG(ret, err);
	ret = _nn_divrem_unshifted(q, &r, a, b_norm, v, cnt); EG(ret, err);
	ret = nn_copy(b_norm, &r); EG(ret, err);

err:
	nn_uninit(&r);
	return ret;
}

/*
 * Compute quotient and remainder and do not normalize them.
 * Constant time, see documentation of _nn_divrem_unshifted().
 *
 * Alias-supporting version of _nn_divrem_unshifted for 'r' only.
 *
 * The function returns 0 on success, -1 on error.
 */
int nn_divrem_unshifted(nn_t q, nn_t r, nn_src_t a, nn_src_t b,
			word_t v, bitcnt_t cnt)
{
	int ret;

	ret = nn_check_initialized(a); EG(ret, err);
	ret = nn_check_initialized(q); EG(ret, err);
	ret = nn_check_initialized(r); EG(ret, err);

	/* Unsupported aliasings */
	MUST_HAVE((q != r) && (q != a) && (q != b), ret, err);

	if (r == b) {
		ret = _nn_divrem_unshifted_aliased(q, a, r, v, cnt);
	} else {
		ret = nn_check_initialized(b); EG(ret, err);
		ret = _nn_divrem_unshifted(q, r, a, b, v, cnt);
	}

err:
	return ret;
}

/*
 * Compute remainder only and do not normalize it.
 * Constant time, see documentation of _nn_divrem_unshifted.
 *
 * Aliasing of inputs and outputs is possible.
 *
 * The function returns 0 on success, -1 on error.
 */
int nn_mod_unshifted(nn_t r, nn_src_t a, nn_src_t b, word_t v, bitcnt_t cnt)
{
	nn q;
	int ret;
	q.magic = WORD(0);

	ret = nn_init(&q, 0); EG(ret, err);
	ret = nn_divrem_unshifted(&q, r, a, b, v, cnt);

err:
	nn_uninit(&q);

	return ret;
}

/*
 * Helper functions for arithmetic in 2-by-1 division
 * used in the reciprocal computation.
 *
 * These are variations of the nn multiprecision functions
 * acting on arrays of fixed length, in place,
 * and returning carry/borrow.
 *
 * Done in constant time.
 */

/*
 * Comparison of two limbs numbers. Internal helper.
 * Checks left to the caller
 */
ATTRIBUTE_WARN_UNUSED_RET static int _wcmp_22(word_t a[2], word_t b[2])
{
	int mask, ret = 0;
	ret += (a[1] > b[1]);
	ret -= (a[1] < b[1]);
	mask = !(ret & 0x1);
	ret += ((a[0] > b[0]) & mask);
	ret -= ((a[0] < b[0]) & mask);
	return ret;
}

/*
 * Addition of two limbs numbers with carry returned. Internal helper.
 * Checks left to the caller.
 */
ATTRIBUTE_WARN_UNUSED_RET static word_t _wadd_22(word_t a[2], word_t b[2])
{
	word_t carry;
	a[0]  = (word_t)(a[0] + b[0]);
	carry = (word_t)(a[0] < b[0]);
	a[1]  = (word_t)(a[1] + carry);
	carry = (word_t)(a[1] < carry);
	a[1]  = (word_t)(a[1] + b[1]);
	carry = (word_t)(carry | (a[1] < b[1]));
	return carry;
}

/*
 * Subtraction of two limbs numbers with borrow returned. Internal helper.
 * Checks left to the caller.
 */
ATTRIBUTE_WARN_UNUSED_RET static word_t _wsub_22(word_t a[2], word_t b[2])
{
	word_t borrow, tmp;
	tmp    = (word_t)(a[0] - b[0]);
	borrow = (word_t)(tmp > a[0]);
	a[0]   = tmp;
	tmp    = (word_t)(a[1] - borrow);
	borrow = (word_t)(tmp > a[1]);
	a[1]   = (word_t)(tmp - b[1]);
	borrow = (word_t)(borrow | (a[1] > tmp));
	return borrow;
}

/*
 * Helper macros for conditional subtraction in 2-by-1 division
 * used in the reciprocal computation.
 *
 * Done in constant time.
 */

/* Conditional subtraction of a one limb number from a two limbs number. */
#define WORD_CND_SUB_21(cnd, ah, al, b) do {				\
		word_t tmp, mask;					\
		mask = WORD_MASK_IFNOTZERO((cnd));			\
		tmp  = (word_t)((al) - ((b) & mask));			\
		(ah) = (word_t)((ah) - (tmp > (al)));			\
		(al) = tmp;						\
	} while (0)
/* Conditional subtraction of a two limbs number from a two limbs number. */
#define WORD_CND_SUB_22(cnd, ah, al, bh, bl) do {			\
		word_t tmp, mask;					\
		mask = WORD_MASK_IFNOTZERO((cnd));			\
		tmp  = (word_t)((al) - ((bl) & mask));			\
		(ah) = (word_t)((ah) - (tmp > (al)));			\
		(al) = tmp;						\
		(ah) = (word_t)((ah) - ((bh) & mask));			\
	} while (0)

/*
 * divide two words by a normalized word using schoolbook division on half
 * words. This is only used below in the reciprocal computation. No checks
 * are performed on inputs. This is expected to be done by the caller.
 *
 * Returns 0 on success, -1 on error.
 */
ATTRIBUTE_WARN_UNUSED_RET static int _word_divrem(word_t *q, word_t *r, word_t ah, word_t al, word_t b)
{
	word_t bh, bl, qh, ql, rm, rhl[2], phl[2];
	int larger, ret;
	u8 j;

	MUST_HAVE((WRSHIFT((b), (WORD_BITS - 1)) == WORD(1)), ret, err);
	bh = WRSHIFT((b), HWORD_BITS);
	bl = WLSHIFT((b), HWORD_BITS);
	rhl[1] = ah;
	rhl[0] = al;

	/*
	 * Compute high part of the quotient. We know from
	 * MUST_HAVE() check above that bh (a word_t) is not 0
	 */

	KNOWN_FACT(bh != 0, ret, err);
	qh = (rhl[1] / bh);
	qh = WORD_MIN(qh, HWORD_MASK);
	WORD_MUL(phl[1], phl[0], qh, (b));
	phl[1] = (WLSHIFT(phl[1], HWORD_BITS) |
		  WRSHIFT(phl[0], HWORD_BITS));
	phl[0] = WLSHIFT(phl[0], HWORD_BITS);

	for (j = 0; j < 2; j++) {
		larger = (_wcmp_22(phl, rhl) > 0);
		qh = (word_t)(qh - (word_t)larger);
		WORD_CND_SUB_22(larger, phl[1], phl[0], bh, bl);
	}

	ret = (_wcmp_22(phl, rhl) > 0);
	MUST_HAVE(!(ret), ret, err);
	IGNORE_RET_VAL(_wsub_22(rhl, phl));
	MUST_HAVE((WRSHIFT(rhl[1], HWORD_BITS) == 0), ret, err);

	/* Compute low part of the quotient. */
	rm = (WLSHIFT(rhl[1], HWORD_BITS) |
	      WRSHIFT(rhl[0], HWORD_BITS));
	ql = (rm / bh);
	ql = WORD_MIN(ql, HWORD_MASK);
	WORD_MUL(phl[1], phl[0], ql, (b));

	for (j = 0; j < 2; j++) {
		larger = (_wcmp_22(phl, rhl) > 0);
		ql = (word_t) (ql - (word_t)larger);
		WORD_CND_SUB_21(larger, phl[1], phl[0], (b));
	}

	ret = _wcmp_22(phl, rhl) > 0;
	MUST_HAVE(!(ret), ret, err);
	IGNORE_RET_VAL(_wsub_22(rhl, phl));
	/* Set outputs. */
	MUST_HAVE((rhl[1] == WORD(0)), ret, err);
	MUST_HAVE(!(rhl[0] >= (b)), ret, err);
	(*q) = (WLSHIFT(qh, HWORD_BITS) | ql);
	(*r) = rhl[0];
	MUST_HAVE(!((word_t) ((*q)*(b) + (*r)) != (al)), ret, err);
	ret = 0;

err:
	return ret;
}

/*
 * Compute the reciprocal of d as
 *	floor(B^3/(d+1)) - B
 * which is used to perform approximate small division using a multiplication.
 *
 * No attempt was made to make it constant time. Indeed, such values are usually
 * precomputed in contexts where constant time is wanted, e.g. in the fp layer.
 *
 * Returns 0 on success, -1 on error.
 */
int wreciprocal(word_t dh, word_t dl, word_t *reciprocal)
{
	word_t q, carry, r[2], t[2];
	int ret;

	MUST_HAVE((reciprocal != NULL), ret, err);

	if (((word_t)(dh + WORD(1)) == WORD(0)) &&
	    ((word_t)(dl + WORD(1)) == WORD(0))) {
		(*reciprocal) = WORD(0);
		ret = 0;
		goto err;
	}

	if ((word_t)(dh + WORD(1)) == WORD(0)) {
		q = (word_t)(~dh);
		r[1] = (word_t)(~dl);
	} else {
		t[1] = (word_t)(~dh);
		t[0] = (word_t)(~dl);
		ret = _word_divrem(&q, r+1, t[1], t[0],
				   (word_t)(dh + WORD(1))); EG(ret, err);
	}

	if ((word_t)(dl + WORD(1)) == WORD(0)) {
		(*reciprocal) = q;
		ret = 0;
		goto err;
	}

	r[0] = WORD(0);

	WORD_MUL(t[1], t[0], q, (word_t)~dl);
	carry = _wadd_22(r, t);

	t[0] = (word_t)(dl + WORD(1));
	t[1] = dh;
	while (carry || (_wcmp_22(r, t) >= 0)) {
		q++;
		carry = (word_t)(carry - _wsub_22(r, t));
	}

	(*reciprocal) = q;
	ret = 0;

err:
	return ret;
}

/*
 * Given an odd number p, compute division coefficients p_normalized,
 * p_shift and p_reciprocal so that:
 *	- p_shift = p_rounded_bitlen - bitsizeof(p), where
 *          o p_rounded_bitlen = BIT_LEN_WORDS(p) (i.e. bit length of
 *            minimum number of words required to store p) and
 *          o p_bitlen is the real bit size of p
 *	- p_normalized = p << p_shift
 *	- p_reciprocal = B^3 / ((p_normalized >> (pbitlen - 2*WORDSIZE)) + 1) - B
 *	  with B = 2^WORDSIZE
 *
 * These coefficients are useful for the optimized shifted variants of NN
 * division and modular functions. Because we have two word_t outputs
 * (p_shift and p_reciprocal), these are passed through word_t pointers.
 * Aliasing of outputs with the input is possible since p_in is copied in
 * local p at the beginning of the function.
 *
 * The function does not support aliasing.
 *
 * The function returns 0 on success, -1 on error.
 */
int nn_compute_div_coefs(nn_t p_normalized, word_t *p_shift,
			  word_t *p_reciprocal, nn_src_t p_in)
{
	bitcnt_t p_rounded_bitlen, p_bitlen;
	nn p, tmp_nn;
	int ret;
	p.magic = tmp_nn.magic = WORD(0);

	ret = nn_check_initialized(p_in); EG(ret, err);

	MUST_HAVE((p_shift != NULL), ret, err);
	MUST_HAVE((p_reciprocal != NULL), ret, err);

	/* Unsupported aliasing */
	MUST_HAVE((p_normalized != p_in), ret, err);

	ret = nn_init(&p, 0); EG(ret, err);
	ret = nn_copy(&p, p_in); EG(ret, err);

	/*
	 * In order for our reciprocal division routines to work, it is expected
	 * that the bit length (including leading zeroes) of input prime
	 * p is >= 2 * wlen where wlen is the number of bits of a word size.
	 */
	if (p.wlen < 2) {
		ret = nn_set_wlen(&p, 2); EG(ret, err);
	}

	ret = nn_init(p_normalized, 0); EG(ret, err);
	ret = nn_init(&tmp_nn, 0); EG(ret, err);

	/* p_rounded_bitlen = bitlen of p rounded to word size */
	p_rounded_bitlen = (bitcnt_t)(WORD_BITS * p.wlen);

	/* p_shift */
	ret = nn_bitlen(&p, &p_bitlen); EG(ret, err);
	(*p_shift) = (word_t)(p_rounded_bitlen - p_bitlen);

	/* p_normalized = p << pshift */
	ret = nn_lshift(p_normalized, &p, (bitcnt_t)(*p_shift)); EG(ret, err);

	/* Sanity check to protect the p_reciprocal computation */
	MUST_HAVE((p_rounded_bitlen >= (2 * WORDSIZE)), ret, err);

	/*
	 * p_reciprocal = B^3 / ((p_normalized >> (p_rounded_bitlen - 2 * wlen)) + 1) - B
	 * where B = 2^wlen where wlen = word size in bits. We use our NN
	 * helper to compute it.
	 */
	ret = nn_rshift(&tmp_nn, p_normalized, (bitcnt_t)(p_rounded_bitlen - (2 * WORDSIZE))); EG(ret, err);
	ret = wreciprocal(tmp_nn.val[1], tmp_nn.val[0], p_reciprocal);

err:
	nn_uninit(&p);
	nn_uninit(&tmp_nn);

	return ret;
}

/*
 * Compute quotient remainder of Euclidean division.
 *
 * This function is a wrapper to normalize the divisor, i.e. shift it so that
 * the MSB of its MSW is set, and precompute the reciprocal of this MSW to be
 * used to perform small divisions using multiplications during the long
 * schoolbook division. It uses the helper functions/macros above.
 *
 * This is NOT constant time with regards to the word length of a and b,
 * but also the actual bitlength of b as we need to normalize b at the
 * bit level.
 * Moreover the precomputation of the reciprocal is not constant time at all.
 *
 * r need not be initialized, the function does it for the the caller.
 *
 * This function does not support aliasing. This is an internal helper, which
 * expects caller to check parameters.
 *
 * It returns 0 on sucess, -1 on error.
 */
ATTRIBUTE_WARN_UNUSED_RET static int _nn_divrem(nn_t q, nn_t r, nn_src_t a, nn_src_t b)
{
	nn b_large, b_normalized;
	bitcnt_t cnt;
	word_t v;
	nn_src_t ptr = b;
	int ret, iszero;
	b_large.magic = b_normalized.magic = WORD(0);

	ret = nn_init(r, 0); EG(ret, err);
	ret = nn_init(q, 0); EG(ret, err);
	ret = nn_init(&b_large, 0); EG(ret, err);

	MUST_HAVE(!nn_iszero(b, &iszero) && !iszero, ret, err);

	if (b->wlen == 1) {
		ret = nn_copy(&b_large, b); EG(ret, err);

		/* Expand our big number with zeroes */
		ret = nn_set_wlen(&b_large, 2); EG(ret, err);

		/*
		 * This cast could seem inappropriate, but we are
		 * sure here that we won't touch ptr since it is only
		 * given as a const parameter to sub functions.
		 */
		ptr = (nn_src_t) &b_large;
	}

	/* After this, we only handle >= 2 words big numbers */
	MUST_HAVE(!(ptr->wlen < 2), ret, err);

	ret = nn_init(&b_normalized, (u16)((ptr->wlen) * WORD_BYTES)); EG(ret, err);
	ret = nn_clz(ptr, &cnt); EG(ret, err);
	ret = nn_lshift_fixedlen(&b_normalized, ptr, cnt); EG(ret, err);
	ret = wreciprocal(b_normalized.val[ptr->wlen - 1],
			  b_normalized.val[ptr->wlen - 2],
			  &v); /* Not constant time. */ EG(ret, err);

	ret = _nn_divrem_unshifted(q, r, a, &b_normalized, v, cnt);

err:
	nn_uninit(&b_normalized);
	nn_uninit(&b_large);

	return ret;
}

/*
 * Returns 0 on succes, -1 on error. Internal helper. Checks on params
 * expected from the caller.
 */
ATTRIBUTE_WARN_UNUSED_RET static int __nn_divrem_notrim_alias(nn_t q, nn_t r, nn_src_t a, nn_src_t b)
{
	nn a_cpy, b_cpy;
	int ret;
	a_cpy.magic = b_cpy.magic = WORD(0);

	ret = nn_init(&a_cpy, 0); EG(ret, err);
	ret = nn_init(&b_cpy, 0); EG(ret, err);
	ret = nn_copy(&a_cpy, a); EG(ret, err);
	ret = nn_copy(&b_cpy, b); EG(ret, err);
	ret = _nn_divrem(q, r, &a_cpy, &b_cpy);

err:
	nn_uninit(&b_cpy);
	nn_uninit(&a_cpy);

	return ret;
}



/*
 * Compute quotient and remainder and normalize them.
 * Not constant time, see documentation of _nn_divrem.
 *
 * Aliased version of _nn_divrem. Returns 0 on success,
 * -1 on error.
 */
int nn_divrem_notrim(nn_t q, nn_t r, nn_src_t a, nn_src_t b)
{
	int ret;

	/* _nn_divrem initializes q and r */
	ret = nn_check_initialized(a); EG(ret, err);
	ret = nn_check_initialized(b); EG(ret, err);
	MUST_HAVE(((q != NULL) && (r != NULL)), ret, err);

	/*
	 * Handle aliasing whenever any of the inputs is
	 * used as an output.
	 */
	if ((a == q) || (a == r) || (b == q) || (b == r)) {
		ret = __nn_divrem_notrim_alias(q, r, a, b);
	} else {
		ret = _nn_divrem(q, r, a, b);
	}

err:
	return ret;
}

/*
 * Compute quotient and remainder and normalize them.
 * Not constant time, see documentation of _nn_divrem().
 * Returns 0 on success, -1 on error.
 *
 * Aliasing is supported.
 */
int nn_divrem(nn_t q, nn_t r, nn_src_t a, nn_src_t b)
{
	int ret;

	ret = nn_divrem_notrim(q, r, a, b); EG(ret, err);

	/* Normalize (trim) the quotient and rest to avoid size overflow */
	ret = nn_normalize(q); EG(ret, err);
	ret = nn_normalize(r);

err:
	return ret;
}

/*
 * Compute remainder only and do not normalize it. Not constant time, see
 * documentation of _nn_divrem(). Returns 0 on success, -1 on error.
 *
 * Aliasing is supported.
 */
int nn_mod_notrim(nn_t r, nn_src_t a, nn_src_t b)
{
	int ret;
	nn q;
	q.magic = WORD(0);

	/* nn_divrem() will init q. */
	ret = nn_divrem_notrim(&q, r, a, b);

	nn_uninit(&q);

	return ret;
}

/*
 * Compute remainder only and normalize it. Not constant time, see
 * documentation of _nn_divrem(). r is initialized by the function.
 * Returns 0 on success, -1 on error.
 *
 * Aliasing is supported.
 */
int nn_mod(nn_t r, nn_src_t a, nn_src_t b)
{
	int ret;
	nn q;
	q.magic = WORD(0);

	/* nn_divrem will init q. */
	ret = nn_divrem(&q, r, a, b);

	nn_uninit(&q);

	return ret;
}

/*
 * Below follow gcd and xgcd non constant time functions for the user ease.
 */

/*
 * Unaliased version of xgcd, and we suppose that a >= b. Badly non-constant
 * time per the algorithm used. The function returns 0 on success, -1 on
 * error. internal helper: expect caller to check parameters.
 */
ATTRIBUTE_WARN_UNUSED_RET static int _nn_xgcd(nn_t g, nn_t u, nn_t v, nn_src_t a, nn_src_t b,
		    int *sign)
{
	nn_t c, d, q, r, u1, v1, u2, v2;
	nn scratch[8];
	int ret, swap, iszero;
	u8 i;

	for (i = 0; i < 8; i++){
		scratch[i].magic = WORD(0);
	}

	/*
	 * Maintain:
	 * |u1 v1| |c| = |a|
	 * |u2 v2| |d|   |b|
	 * u1, v1, u2, v2 >= 0
	 * c >= d
	 *
	 * Initially:
	 * |1  0 | |a| = |a|
	 * |0  1 | |b|   |b|
	 *
	 * At each iteration:
	 * c >= d
	 * c = q*d + r
	 * |u1 v1| = |q*u1+v1 u1|
	 * |u2 v2|   |q*u2+v2 u2|
	 *
	 * Finally, after i steps:
	 * |u1 v1| |g| = |a|
	 * |u2 v2| |0| = |b|
	 *
	 * Inverting the matrix:
	 * |g| = (-1)^i | v2 -v1| |a|
	 * |0|          |-u2  u1| |b|
	 */

	/*
	 * Initialization.
	 */
	ret = nn_init(g, 0); EG(ret, err);
	ret = nn_init(u, 0); EG(ret, err);
	ret = nn_init(v, 0); EG(ret, err);
	ret = nn_iszero(b, &iszero); EG(ret, err);
	if (iszero) {
		/* gcd(0, a) = a, and 1*a + 0*b = a */
		ret = nn_copy(g, a); EG(ret, err);
		ret = nn_one(u); EG(ret, err);
		ret = nn_zero(v); EG(ret, err);
		(*sign) = 1;
		goto err;
	}

	for (i = 0; i < 8; i++){
		ret = nn_init(&scratch[i], 0); EG(ret, err);
	}

	u1 = &(scratch[0]);
	v1 = &(scratch[1]);
	u2 = &(scratch[2]);
	v2 = &(scratch[3]);
	ret = nn_one(u1); EG(ret, err);
	ret = nn_zero(v1); EG(ret, err);
	ret = nn_zero(u2); EG(ret, err);
	ret = nn_one(v2); EG(ret, err);
	c = &(scratch[4]);
	d = &(scratch[5]);
	ret = nn_copy(c, a); EG(ret, err); /* Copy could be skipped. */
	ret = nn_copy(d, b); EG(ret, err); /* Copy could be skipped. */
	q = &(scratch[6]);
	r = &(scratch[7]);
	swap = 0;

	/*
	 * Loop.
	 */
	ret = nn_iszero(d, &iszero); EG(ret, err);
	while (!iszero) {
		ret = nn_divrem(q, r, c, d); EG(ret, err);
		ret = nn_normalize(q); EG(ret, err);
		ret = nn_normalize(r); EG(ret, err);
		ret = nn_copy(c, r); EG(ret, err);
		ret = nn_mul(r, q, u1); EG(ret, err);
		ret = nn_normalize(r); EG(ret, err);
		ret = nn_add(v1, v1, r); EG(ret, err);
		ret = nn_mul(r, q, u2); EG(ret, err);
		ret = nn_normalize(r); EG(ret, err);
		ret = nn_add(v2, v2, r); EG(ret, err);
		ret = nn_normalize(v1); EG(ret, err);
		ret = nn_normalize(v2); EG(ret, err);
		swap = 1;
		ret = nn_iszero(c, &iszero); EG(ret, err);
		if (iszero) {
			break;
		}
		ret = nn_divrem(q, r, d, c); EG(ret, err);
		ret = nn_normalize(q); EG(ret, err);
		ret = nn_normalize(r); EG(ret, err);
		ret = nn_copy(d, r); EG(ret, err);
		ret = nn_mul(r, q, v1); EG(ret, err);
		ret = nn_normalize(r); EG(ret, err);
		ret = nn_add(u1, u1, r); EG(ret, err);
		ret = nn_mul(r, q, v2); EG(ret, err);
		ret = nn_normalize(r); EG(ret, err);
		ret = nn_add(u2, u2, r); EG(ret, err);
		ret = nn_normalize(u1); EG(ret, err);
		ret = nn_normalize(u2); EG(ret, err);
		swap = 0;

		/* refresh loop condition */
		ret = nn_iszero(d, &iszero); EG(ret, err);
	}

	/* Copies could be skipped. */
	if (swap) {
		ret = nn_copy(g, d); EG(ret, err);
		ret = nn_copy(u, u2); EG(ret, err);
		ret = nn_copy(v, u1); EG(ret, err);
	} else {
		ret = nn_copy(g, c); EG(ret, err);
		ret = nn_copy(u, v2); EG(ret, err);
		ret = nn_copy(v, v1); EG(ret, err);
	}

	/* swap = -1 means u <= 0; = 1 means v <= 0 */
	(*sign) = swap ? -1 : 1;
	ret = 0;

err:
	/*
	 * We uninit scratch elements in all cases, i.e. whether or not
	 * we return an error or not.
	 */
	for (i = 0; i < 8; i++){
		nn_uninit(&scratch[i]);
	}
	/* Unitialize output in case of error */
	if (ret){
		nn_uninit(v);
		nn_uninit(u);
		nn_uninit(g);
	}

	return ret;
}

/*
 * Aliased version of xgcd, and no assumption on a and b. Not constant time at
 * all. returns 0 on success, -1 on error. XXX document 'sign'
 *
 * Aliasing is supported.
 */
int nn_xgcd(nn_t g, nn_t u, nn_t v, nn_src_t a, nn_src_t b, int *sign)
{
	/* Handle aliasing
	 * Note: in order to properly handle aliasing, we accept to lose
	 * some "space" on the stack with copies.
	 */
	nn a_cpy, b_cpy;
	nn_src_t a_, b_;
	int ret, cmp, _sign;
	a_cpy.magic = b_cpy.magic = WORD(0);

	/* The internal _nn_xgcd function initializes g, u and v */
	ret = nn_check_initialized(a); EG(ret, err);
	ret = nn_check_initialized(b); EG(ret, err);
	MUST_HAVE((sign != NULL), ret, err);

	ret = nn_init(&b_cpy, 0); EG(ret, err);

	/* Aliasing of a */
	if ((g == a) || (u == a) || (v == a)){
		ret = nn_copy(&a_cpy, a); EG(ret, err);
		a_ = &a_cpy;
	} else {
		a_ = a;
	}
	/* Aliasing of b */
	if ((g == b) || (u == b) || (v == b)) {
		ret = nn_copy(&b_cpy, b); EG(ret, err);
		b_ = &b_cpy;
	} else {
		b_ = b;
	}

	ret = nn_cmp(a_, b_, &cmp); EG(ret, err);
	if (cmp < 0) {
		/* If a < b, swap the inputs */
		ret = _nn_xgcd(g, v, u, b_, a_, &_sign); EG(ret, err);
		(*sign) = -(_sign);
	} else {
		ret = _nn_xgcd(g, u, v, a_, b_, &_sign); EG(ret, err);
		(*sign) = _sign;
	}

err:
	nn_uninit(&b_cpy);
	nn_uninit(&a_cpy);

	return ret;
}

/*
 * Compute g = gcd(a, b). Internally use the xgcd and drop u and v.
 * Not constant time at all. Returns 0 on success, -1 on error.
 * XXX document 'sign'.
 *
 * Aliasing is supported.
 */
int nn_gcd(nn_t g, nn_src_t a, nn_src_t b, int *sign)
{
	nn u, v;
	int ret;
	u.magic = v.magic = WORD(0);

	/* nn_xgcd will initialize g, u and v and
	 * check if a and b are indeed initialized.
	 */
	ret = nn_xgcd(g, &u, &v, a, b, sign);

	nn_uninit(&u);
	nn_uninit(&v);

	return ret;
}
