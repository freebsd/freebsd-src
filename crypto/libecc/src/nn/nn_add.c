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
#include <libecc/nn/nn_add.h>
#include <libecc/nn/nn.h>

/*
 * This module provides conditional addition and subtraction functions between
 * two nn:
 *
 *  o out = in1 +/- in2 if cnd is not zero.
 *  o out = in1 if cnd is zero.
 *
 * The time taken by the operation does not depend on cnd value, i.e. it is
 * constant time for that specific factor, nor on the values of in1 and in2.
 * It still depends on the maximal length of in1 and in2.
 *
 * Common addition and subtraction functions are derived from those conditional
 * versions.
 */

/*
 * Conditionally adds 'in2' to 'in1' according to "cnd", storing the result
 * in "out" and returning the carry in 'carry' parameter on success. This
 * is the lowest level function for conditional addition. The function
 * returns 0 on success, -1 on error.
 *
 * Note that unlike "usual" addition, the function is *in general* not
 * commutative, i.e. "_nn_cnd_add(cnd, out, in1, in2)"  is not equivalent
 * to "_nn_cnd_add(cnd, out, in2, in1)". It is commutative though if "cnd"
 * is not zero or 'in1' == 'in2'.
 *
 * Aliasing of inputs and output is possible. "out" is initialized if needed,
 * that is if not aliased to 'in1' or 'in2'. The length of "out" is set to
 * the maximal length of 'in1' and 'in2'. Note that both 'in1' and 'in2' will
 * be read to this maximal length. As our memory managment model assumes that
 * storage arrays only contains zeros past the "wlen" index, correct results
 * will be produced. The length of 'out' is not normalized on return.
 *
 * The runtime of this function should not depend on:
 *  o the value of "cnd",
 *  o the data stored in 'in1' and 'in2'.
 * It depends on:
 *  o the maximal length of 'in1' and 'in2'.
 *
 * This function is for internal use only.
 */
ATTRIBUTE_WARN_UNUSED_RET static int _nn_cnd_add(int cnd, nn_t out, nn_src_t in1, nn_src_t in2,
		       word_t *carry)
{
	word_t tmp, carry1, carry2, _carry = WORD(0);
	word_t mask = WORD_MASK_IFNOTZERO(cnd);
	u8 i, loop_wlen;
	int ret;

	MUST_HAVE((carry != NULL), ret, err);
	ret = nn_check_initialized(in1); EG(ret, err);
	ret = nn_check_initialized(in2); EG(ret, err);

	/* Handle aliasing */
	loop_wlen = LOCAL_MAX(in1->wlen, in2->wlen);
	if ((out != in1) && (out != in2)) {
		ret = nn_init(out, (u16)(loop_wlen * WORD_BYTES)); EG(ret, err);
	} else {
		ret = nn_set_wlen(out, loop_wlen); EG(ret, err);
	}

	/* Perform addition one word at a time, propagating the carry. */
	for (i = 0; i < loop_wlen; i++) {
		tmp = (word_t)(in1->val[i] + (in2->val[i] & mask));
		carry1 = (word_t)(tmp < in1->val[i]);
		out->val[i] = (word_t)(tmp + _carry);
		carry2 = (word_t)(out->val[i] < tmp);
		/* There is at most one carry going out. */
		_carry = (word_t)(carry1 | carry2);
	}

	(*carry) = _carry;

err:
	return ret;
}

/*
 * Conditionally adds 'in2' to 'in1' according to "cnd", storing the result
 * in "out", including the potential carry overflowing past the maximal
 * length of 'in1' and 'in2'. It is user responsibility to ensure that the
 * resulting nn will not be higher than what can be supported. This is
 * for instance guaranteed if both in1->wlen and in2->wlen are less than
 * NN_MAX_WORD_LEN. Otherwise the function will error out which could leak
 * information.
 *
 * Note that the length of the output depends the lengths of the inputs,
 * but also on their values.
 * It is the user responsibility to use this function carefully when
 * constant time of an algorithm using this function is seeked.
 * This choice was preferred above unconditionally increasing
 * the length of the output by one, to ease the management of length
 * explosion when multiple additions are performed.
 * For finer carry propagation and length control the internal "_nn_cnd_add"
 * function can be used.
 *
 * See "_nn_cnd_add" documentation above for further details.
 *
 * The function returns 0 on success, -1 on error.
 */
int nn_cnd_add(int cnd, nn_t out, nn_src_t in1, nn_src_t in2)
{
	word_t carry;
	int ret;

	ret = _nn_cnd_add(cnd, out, in1, in2, &carry); EG(ret, err);

	/* We cannot allow a non-zero carry if out->wlen is at its limit */
	MUST_HAVE(((out->wlen != NN_MAX_WORD_LEN) || (!carry)), ret, err);

	if (out->wlen != NN_MAX_WORD_LEN) {
		/*
		 * To maintain constant time, we perform carry addition in all
		 * cases. If carry is 0, no change is performed in practice,
		 * neither to 'out' value, nor to its length.
		 * Note that the length of the output can vary and make
		 * the time taken by further operations on it also vary.
		 */
		out->val[out->wlen] = carry;
		out->wlen = (u8)(out->wlen + carry);
	}

err:
	return ret;
}

/*
 * Unconditionally adds 'in2' to 'in1', storing the result in "out",
 * including the potential carry overflowing past the maximal length of
 * 'in1' and 'in2'. The function returns 0 on success, -1 on error.
 *
 * Note that the length of the output depends the lengths of the inputs,
 * but also on their values.
 * It is the user responsibility to use this function carefully when
 * constant time of an algorithm using this function is seeked.
 *
 * See "_nn_cnd_add" documentation for further details.
 *
 */
int nn_add(nn_t out, nn_src_t in1, nn_src_t in2)
{
	return nn_cnd_add(1, out, in1, in2);
}

/*
 * Compute out = in1 + w where 'in1' is an initialized nn and 'w' a word. It is
 * caller responsibility to ensure that the result will fit in a nn (This is
 * for instance guaranteed if 'in1' wlen is less than NN_MAX_WORD_LEN). The
 * function returns 0 on succes, -1 on error.
 *
 * The result is stored in 'out' parameter. 'out' is initialized if needed (i.e.
 * in case aliasing is not used) and is not normalized on return.
 *
 * Note that the length of the output depends the lengths of the inputs,
 * but also on their values.
 * It is the user responsibility to use this function carefully when
 * constant time of an algorithm using this function is seeked.
 *
 * This function is for internal use only.
 */
ATTRIBUTE_WARN_UNUSED_RET static int nn_add_word(nn_t out, nn_src_t in1, word_t w)
{
	word_t carry, tmp;
	u8 i, n_wlen;
	int ret;

	ret = nn_check_initialized(in1); EG(ret, err);

	/* Handle aliasing */
	n_wlen = in1->wlen;
	if (out != in1) {
		ret = nn_init(out, (u16)(n_wlen * WORD_BYTES)); EG(ret, err);
	} else {
		ret = nn_set_wlen(out, n_wlen); EG(ret, err);
	}

	/* No matter its value, propagate the carry. */
	carry = w;
	for (i = 0; i < n_wlen; i++) {
		tmp = (word_t)(in1->val[i] + carry);
		carry = (word_t)(tmp < in1->val[i]);
		out->val[i] = tmp;
	}

	MUST_HAVE(((out->wlen != NN_MAX_WORD_LEN) || (!carry)), ret, err);
	if (out->wlen != NN_MAX_WORD_LEN) {
		/*
		 * To maintain constant time, we perform carry addition in all
		 * cases. If carry is 0, no change is performed in practice,
		 * neither to 'out' value, nor to its length.
		 * Note that the length of the output can vary and make
		 * the time taken by further operations on it will vary.
		 */
		out->val[out->wlen] = carry;
		out->wlen = (u8)(out->wlen + carry);
	}

err:
	return ret;
}

/*
 * Compute out = in1 + 1. Aliasing is supported i.e. nn_inc(in1, in1) works as
 * expected and provides in1++. It is caller responsibility to ensure that the
 * result will fit in a nn (This is for instance guaranteed if 'in1' wlen is
 * less than NN_MAX_WORD_LEN). The function returns 0 on success, -1 on error.
 *
 * Note that the length of the output depends the lengths of the inputs,
 * but also on their values.
 * It is the user responsibility to use this function carefully when
 * constant time of an algorithm using this function is seeked.
 */
int nn_inc(nn_t out, nn_src_t in1)
{
	return nn_add_word(out, in1, WORD(1));
}

/*
 * Conditionally subtracts 'in2' from 'in1' according to "cnd",
 * storing the result in "out":
 *  o out = in1 - in2 if cnd is not zero.
 *  o out = in1 if cnd is zero.
 *
 * 'in1' and 'in2' must point to initialized nn, such that the value of 'in1'
 * is larger than 'in2'. Aliasing is supported, i.e. 'out' can point to the
 * same nn as 'in1' or 'in2'. If aliasing is not used, 'out' is initialized by
 * the function. The length of 'out' is set to the length of 'in1'
 * and is not normalized on return.
 *
 * The function returns 0 on success, -1 on error.
 */
int nn_cnd_sub(int cnd, nn_t out, nn_src_t in1, nn_src_t in2)
{
	word_t tmp, borrow1, borrow2, borrow = WORD(0);
	word_t mask = WORD_MASK_IFNOTZERO(cnd);
	u8 loop_wlen, i;
	int ret;

	ret = nn_check_initialized(in1); EG(ret, err);
	ret = nn_check_initialized(in2); EG(ret, err);

	/* Handle aliasing */
	loop_wlen = LOCAL_MAX(in1->wlen, in2->wlen);
	if ((out != in1) && (out != in2)) {
		ret = nn_init(out, (u16)(loop_wlen * WORD_BYTES)); EG(ret, err);
	} else {
		ret = nn_set_wlen(out, in1->wlen); EG(ret, err);
	}

	/* Perform subtraction one word at a time, propagating the borrow. */
	for (i = 0; i < loop_wlen; i++) {
		tmp = (word_t)(in1->val[i] - (in2->val[i] & mask));
		borrow1 = (word_t)(tmp > in1->val[i]);
		out->val[i] = (word_t)(tmp - borrow);
		borrow2 = (word_t)(out->val[i] > tmp);
		/* There is at most one borrow going out. */
		borrow = (word_t)(borrow1 | borrow2);
	}

	/* We only support the in1 >= in2 case */
	ret = (borrow != WORD(0)) ? -1 : 0;

err:
	return ret;
}

/* Same as the one above, but the subtraction is performed unconditionally. */
int nn_sub(nn_t out, nn_src_t in1, nn_src_t in2)
{
	return nn_cnd_sub(1, out, in1, in2);
}

/*
 * Compute out = in1 - 1 where in1 is a *positive* integer. Aliasing is
 * supported i.e. nn_dec(A, A) works as expected and provides A -= 1.
 * The function returns 0 on success, -1 on error.
 */
int nn_dec(nn_t out, nn_src_t in1)
{
	const word_t w = WORD(1);
	word_t tmp, borrow;
	u8 n_wlen, i;
	int ret;

	ret = nn_check_initialized(in1); EG(ret, err);
	n_wlen = in1->wlen;
	ret = nn_set_wlen(out, n_wlen); EG(ret, err);

	/* Perform subtraction w/ provided word and propagate the borrow */
	borrow = w;
	for (i = 0; i < n_wlen; i++) {
		tmp = (word_t)(in1->val[i] - borrow);
		borrow = (word_t)(tmp > in1->val[i]);
		out->val[i] = tmp;
	}

	ret = (borrow != WORD(0)) ? -1 : 0;

err:
	return ret;
}

/*
 * The following functions handle modular arithmetic. Our outputs sizes do not
 * need a "normalization" since everything will be bounded by the modular number
 * size.
 *
 * Warning: the following functions are only useful when the inputs are < p,
 * i.e. we suppose that the input are already reduced modulo p. These primitives
 * are mostly useful for the Fp layer. Even though they give results when
 * applied to inputs >= p, there is no guarantee that the result is indeed < p
 * or correct whatsoever.
 */

/*
 * Compute out = in1 + in2 mod p. The function returns 0 on success, -1 on
 * error.
 *
 * Aliasing not fully supported, for internal use only.
 */
static int _nn_mod_add(nn_t out, nn_src_t in1, nn_src_t in2, nn_src_t p)
{
	int ret, larger, cmp;

	ret = nn_check_initialized(in1); EG(ret, err);
	ret = nn_check_initialized(in2); EG(ret, err);
	ret = nn_check_initialized(p); EG(ret, err);
	MUST_HAVE((p->wlen < NN_MAX_WORD_LEN), ret, err); /* otherwise carry could overflow */
	SHOULD_HAVE((!nn_cmp(in1, p, &cmp)) && (cmp < 0), ret, err); /* a SHOULD_HAVE as documented above */
	SHOULD_HAVE((!nn_cmp(in2, p, &cmp)) && (cmp < 0), ret, err); /* a SHOULD_HAVE as documented above */

	ret = nn_add(out, in1, in2); EG(ret, err);
	/*
	 * If previous addition extends out->wlen, this may have an effect on
	 * computation time of functions below. For that reason, we always
	 * normalize out->wlen to p->wlen + 1. Its length is set to that of
	 * p after the computations.
	 *
	 * We could also use _nn_cnd_add to catch the carry and deal
	 * with p's of size NN_MAX_WORD_LEN.
	 * It is still painful because we have no constraint on the lengths
	 * of in1 and in2 so getting a carry out does not necessarily mean
	 * that the sum is larger than p...
	 */
	ret = nn_set_wlen(out, (u8)(p->wlen + 1)); EG(ret, err);
	ret = nn_cmp(out, p, &cmp); EG(ret, err);
	larger = (cmp >= 0);
	ret = nn_cnd_sub(larger, out, out, p); EG(ret, err);
	ret = nn_set_wlen(out, p->wlen);

err:
	return ret;
}

/*
 * Compute out = in1 + in2 mod p. The function returns 0 on success, -1 on
 * error.
 *
 * Aliasing is supported.
 */
int nn_mod_add(nn_t out, nn_src_t in1, nn_src_t in2, nn_src_t p)
{
	int ret;

	if(out == p){
		nn p_cpy;
		p_cpy.magic = WORD(0);

		ret = nn_copy(&p_cpy, p); EG(ret, err1);
		ret = _nn_mod_add(out, in1, in2, &p_cpy);

err1:
		nn_uninit(&p_cpy);
		EG(ret, err);
	}
	else{
		ret = _nn_mod_add(out, in1, in2, p);
	}

err:
	return ret;
}

/*
 * Compute out = in1 + 1 mod p. The function returns 0 on success, -1 on error.
 *
 * Aliasing not fully supported, for internal use only.
 */
static int _nn_mod_inc(nn_t out, nn_src_t in1, nn_src_t p)
{
	int larger, ret, cmp;

	ret = nn_check_initialized(in1); EG(ret, err);
	ret = nn_check_initialized(p); EG(ret, err);
	MUST_HAVE((p->wlen < NN_MAX_WORD_LEN), ret, err); /* otherwise carry could overflow */
	SHOULD_HAVE((!nn_cmp(in1, p, &cmp)) && (cmp < 0), ret, err); /* a SHOULD_HAVE as documented above */

	ret = nn_inc(out, in1); EG(ret, err);
	ret = nn_set_wlen(out, (u8)(p->wlen + 1)); EG(ret, err); /* see comment in nn_mod_add() */
	ret = nn_cmp(out, p, &cmp); EG(ret, err);
	larger = (cmp >= 0);
	ret = nn_cnd_sub(larger, out, out, p); EG(ret, err);
	ret = nn_set_wlen(out, p->wlen);

err:
	return ret;
}

/*
 * Compute out = in1 + 1 mod p. The function returns 0 on success, -1 on error.
 *
 * Aliasing supported.
 */
int nn_mod_inc(nn_t out, nn_src_t in1, nn_src_t p)
{
	int ret;

	if(out == p){
		nn p_cpy;
		p_cpy.magic = WORD(0);

		ret = nn_copy(&p_cpy, p); EG(ret, err1);
		ret = _nn_mod_inc(out, in1, &p_cpy);

err1:
		nn_uninit(&p_cpy);
		EG(ret, err);
	}
	else{
		ret = _nn_mod_inc(out, in1, p);
	}

err:
	return ret;

}

/*
 * Compute out = in1 - in2 mod p. The function returns 0 on success, -1 on
 * error.
 *
 * Aliasing not supported, for internal use only.
 */
static int _nn_mod_sub(nn_t out, nn_src_t in1, nn_src_t in2, nn_src_t p)
{
	int smaller, ret, cmp;
	nn_src_t in2_;
	nn in2_cpy;
	in2_cpy.magic = WORD(0);

	ret = nn_check_initialized(in1); EG(ret, err);
	ret = nn_check_initialized(in2); EG(ret, err);
	ret = nn_check_initialized(p); EG(ret, err);
	MUST_HAVE((p->wlen < NN_MAX_WORD_LEN), ret, err); /* otherwise carry could overflow */
	SHOULD_HAVE((!nn_cmp(in1, p, &cmp)) && (cmp < 0), ret, err); /* a SHOULD_HAVE as documented above */
	SHOULD_HAVE((!nn_cmp(in2, p, &cmp)) && (cmp < 0), ret, err); /* a SHOULD_HAVE as documented above */

	/* Handle the case where in2 and out are aliased */
	if (in2 == out) {
		ret = nn_copy(&in2_cpy, in2); EG(ret, err);
		in2_ = &in2_cpy;
	} else {
		ret = nn_init(&in2_cpy, 0); EG(ret, err);
		in2_ = in2;
	}

	/* The below trick is used to avoid handling of "negative" numbers. */
	ret = nn_cmp(in1, in2_, &cmp); EG(ret, err);
	smaller = (cmp < 0);
	ret = nn_cnd_add(smaller, out, in1, p); EG(ret, err);
	ret = nn_set_wlen(out, (u8)(p->wlen + 1)); EG(ret, err);/* See Comment in nn_mod_add() */
	ret = nn_sub(out, out, in2_); EG(ret, err);
	ret = nn_set_wlen(out, p->wlen);

err:
	nn_uninit(&in2_cpy);

	return ret;
}

/*
 * Compute out = in1 - in2 mod p. The function returns 0 on success, -1 on
 * error.
 *
 * Aliasing supported.
 */
int nn_mod_sub(nn_t out, nn_src_t in1, nn_src_t in2, nn_src_t p)
{
	int ret;

	if(out == p){
		nn p_cpy;
		p_cpy.magic = WORD(0);

		ret = nn_copy(&p_cpy, p); EG(ret, err1);
		ret = _nn_mod_sub(out, in1, in2, &p_cpy);

err1:
		nn_uninit(&p_cpy);
		EG(ret, err);
	}
	else{
		ret = _nn_mod_sub(out, in1, in2, p);
	}

err:
	return ret;
}

/*
 * Compute out = in1 - 1 mod p. The function returns 0 on success, -1 on error
 *
 * Aliasing not supported, for internal use only.
 */
static int _nn_mod_dec(nn_t out, nn_src_t in1, nn_src_t p)
{
	int ret, iszero, cmp;

	ret = nn_check_initialized(in1); EG(ret, err);
	ret = nn_check_initialized(p); EG(ret, err);
	MUST_HAVE((p->wlen < NN_MAX_WORD_LEN), ret, err); /* otherwise carry could overflow */
	FORCE_USED_VAR(cmp); /* nop to silence possible warning with macro below */
	SHOULD_HAVE((!nn_cmp(in1, p, &cmp)) && (cmp < 0), ret, err);  /* a SHOULD_HAVE; Documented above */

	/* The below trick is used to avoid handling of "negative" numbers. */
	ret = nn_iszero(in1, &iszero); EG(ret, err);
	ret = nn_cnd_add(iszero, out, in1, p); EG(ret, err);
	ret = nn_set_wlen(out, (u8)(p->wlen + 1)); EG(ret, err); /* See Comment in nn_mod_add() */
	ret = nn_dec(out, out); EG(ret, err);
	ret = nn_set_wlen(out, p->wlen);

err:
	return ret;
}

/*
 * Compute out = in1 - 1 mod p. The function returns 0 on success, -1 on error
 *
 * Aliasing supported.
 */
int nn_mod_dec(nn_t out, nn_src_t in1, nn_src_t p)
{
	int ret;

	if(out == p){
		nn p_cpy;
		p_cpy.magic = WORD(0);

		ret = nn_copy(&p_cpy, p); EG(ret, err1);
		ret = _nn_mod_dec(out, in1, &p_cpy);

err1:
		nn_uninit(&p_cpy);
		EG(ret, err);
	}
	else{
		ret = _nn_mod_dec(out, in1, p);
	}

err:
	return ret;
}

/*
 * Compute out = -in mod p. The function returns 0 on success, -1 on error.
 * Because we only support positive integers, we compute
 * out = p - in (except when value is 0).
 *
 * We suppose that in is already reduced modulo p.
 *
 * Aliasing is supported.
 *
 */
int nn_mod_neg(nn_t out, nn_src_t in, nn_src_t p)
{
	int ret, cmp, iszero;

	FORCE_USED_VAR(cmp);

	ret = nn_check_initialized(in); EG(ret, err);
	ret = nn_check_initialized(p); EG(ret, err);

	SHOULD_HAVE((!nn_cmp(in, p, &cmp)) && (cmp < 0), ret, err);  /* a SHOULD_HAVE; Documented above */

	ret = nn_iszero(in, &iszero); EG(ret, err);
	if (iszero) {
		ret = nn_init(out, 0); EG(ret, err);
		ret = nn_zero(out); EG(ret, err);
	} else {
		ret = nn_sub(out, p, in); EG(ret, err);
	}

err:
	return ret;
}
