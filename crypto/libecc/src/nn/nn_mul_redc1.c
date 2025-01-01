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
#include <libecc/nn/nn_mul_redc1.h>
#include <libecc/nn/nn_mul_public.h>
#include <libecc/nn/nn_add.h>
#include <libecc/nn/nn_logical.h>
#include <libecc/nn/nn_div_public.h>
#include <libecc/nn/nn_modinv.h>
#include <libecc/nn/nn.h>

/*
 * Given an odd number p, compute Montgomery coefficients r, r_square
 * as well as mpinv so that:
 *
 *	- r = 2^p_rounded_bitlen mod (p), where
 *        p_rounded_bitlen = BIT_LEN_WORDS(p) (i.e. bit length of
 *        minimum number of words required to store p)
 *	- r_square = r^2 mod (p)
 *	- mpinv = -p^-1 mod (2^WORDSIZE).
 *
 * Aliasing of outputs with the input is possible since p_in is
 * copied in local p at the beginning of the function.
 *
 * The function returns 0 on success, -1 on error. out parameters 'r',
 * 'r_square' and 'mpinv' are only meaningful on success.
 */
int nn_compute_redc1_coefs(nn_t r, nn_t r_square, nn_src_t p_in, word_t *mpinv)
{
	bitcnt_t p_rounded_bitlen;
	nn p, tmp_nn1, tmp_nn2;
	word_t _mpinv;
	int ret, isodd;
	p.magic = tmp_nn1.magic = tmp_nn2.magic = WORD(0);

	ret = nn_check_initialized(p_in); EG(ret, err);
	ret = nn_init(&p, 0); EG(ret, err);
	ret = nn_copy(&p, p_in); EG(ret, err);
	MUST_HAVE((mpinv != NULL), ret, err);

	/*
	 * In order for our reciprocal division routines to work, it is
	 * expected that the bit length (including leading zeroes) of
	 * input prime p is >= 2 * wlen where wlen is the number of bits
	 * of a word size.
	 */
	if (p.wlen < 2) {
		ret = nn_set_wlen(&p, 2); EG(ret, err);
	}

	ret = nn_init(r, 0); EG(ret, err);
	ret = nn_init(r_square, 0); EG(ret, err);
	ret = nn_init(&tmp_nn1, 0); EG(ret, err);
	ret = nn_init(&tmp_nn2, 0); EG(ret, err);

	/* p_rounded_bitlen = bitlen of p rounded to word size */
	p_rounded_bitlen = (bitcnt_t)(WORD_BITS * p.wlen);

	/* _mpinv = 2^wlen - (modinv(prime, 2^wlen)) */
	ret = nn_set_wlen(&tmp_nn1, 2); EG(ret, err);
	tmp_nn1.val[1] = WORD(1);
	ret = nn_copy(&tmp_nn2, &tmp_nn1); EG(ret, err);
	ret = nn_modinv_2exp(&tmp_nn1, &p, WORD_BITS, &isodd); EG(ret, err);
	ret = nn_sub(&tmp_nn1, &tmp_nn2, &tmp_nn1); EG(ret, err);
	_mpinv = tmp_nn1.val[0];

	/* r = (0x1 << p_rounded_bitlen) (p) */
	ret = nn_one(r); EG(ret, err);
	ret = nn_lshift(r, r, p_rounded_bitlen); EG(ret, err);
	ret = nn_mod(r, r, &p); EG(ret, err);

	/*
	 * r_square = (0x1 << (2*p_rounded_bitlen)) (p)
	 * We are supposed to handle NN numbers of size  at least two times
	 * the biggest prime we use. Thus, we should be able to compute r_square
	 * with a multiplication followed by a reduction. (NB: we cannot use our
	 * Montgomery primitives at this point since we are computing its
	 * constants!)
	 */
	/* Check we have indeed enough space for our r_square computation */
	MUST_HAVE(!(NN_MAX_BIT_LEN < (2 * p_rounded_bitlen)), ret, err);

	ret = nn_sqr(r_square, r); EG(ret, err);
	ret = nn_mod(r_square, r_square, &p); EG(ret, err);

	(*mpinv) = _mpinv;

err:
	nn_uninit(&p);
	nn_uninit(&tmp_nn1);
	nn_uninit(&tmp_nn2);

	return ret;
}

/*
 * Perform Montgomery multiplication, that is usual multiplication
 * followed by reduction modulo p.
 *
 * Inputs are supposed to be < p (i.e. taken modulo p).
 *
 * This uses the CIOS algorithm from Koc et al.
 *
 * The p input is the modulo number of the Montgomery multiplication,
 * and mpinv is -p^(-1) mod (2^WORDSIZE).
 *
 * The function does not check input parameters are initialized. This
 * MUST be done by the caller.
 *
 * The function returns 0 on success, -1 on error.
 */
ATTRIBUTE_WARN_UNUSED_RET static int _nn_mul_redc1(nn_t out, nn_src_t in1, nn_src_t in2, nn_src_t p,
			 word_t mpinv)
{
	word_t prod_high, prod_low, carry, acc, m;
	unsigned int i, j, len, len_mul;
	/* a and b inputs such that len(b) <= len(a) */
	nn_src_t a, b;
	int ret, cmp;
	u8 old_wlen;

	/*
	 * These comparisons are input hypothesis and does not "break"
	 * the following computation. However performance loss exists
	 * when this check is always done, this is why we use our
	 * SHOULD_HAVE primitive.
	 */
	SHOULD_HAVE((!nn_cmp(in1, p, &cmp)) && (cmp < 0), ret, err);
	SHOULD_HAVE((!nn_cmp(in2, p, &cmp)) && (cmp < 0), ret, err);

	ret = nn_init(out, 0); EG(ret, err);

	/* Check which one of in1 or in2 is the biggest */
	a = (in1->wlen <= in2->wlen) ? in2 : in1;
	b = (in1->wlen <= in2->wlen) ? in1 : in2;

	/*
	 * The inputs might have been reduced due to trimming
	 * because of leading zeroes. It is important for our
	 * Montgomery algorithm to work on sizes consistent with
	 * out prime p real bit size. Thus, we expand the output
	 * size to the size of p.
	 */
	ret = nn_set_wlen(out, p->wlen); EG(ret, err);

	len = out->wlen;
	len_mul = b->wlen;
	/*
	 * We extend out to store carries. We first check that we
	 * do not have an overflow on the NN size.
	 */
	MUST_HAVE(((WORD_BITS * (out->wlen + 1)) <= NN_MAX_BIT_LEN), ret, err);
	old_wlen = out->wlen;
	out->wlen = (u8)(out->wlen + 1);

	/*
	 * This can be skipped if the first iteration of the for loop
	 * is separated.
	 */
	for (i = 0; i < out->wlen; i++) {
		out->val[i] = 0;
	}
	for (i = 0; i < len; i++) {
		carry = WORD(0);
		for (j = 0; j < len_mul; j++) {
			WORD_MUL(prod_high, prod_low, a->val[i], b->val[j]);
			prod_low  = (word_t)(prod_low + carry);
			prod_high = (word_t)(prod_high + (prod_low < carry));
			out->val[j] = (word_t)(out->val[j] + prod_low);
			carry = (word_t)(prod_high + (out->val[j] < prod_low));
		}
		for (; j < len; j++) {
			out->val[j] = (word_t)(out->val[j] + carry);
			carry = (word_t)(out->val[j] < carry);
		}
		out->val[j] = (word_t)(out->val[j] + carry);
		acc = (word_t)(out->val[j] < carry);

		m = (word_t)(out->val[0] * mpinv);
		WORD_MUL(prod_high, prod_low, m, p->val[0]);
		prod_low = (word_t)(prod_low + out->val[0]);
		carry = (word_t)(prod_high + (prod_low < out->val[0]));
		for (j = 1; j < len; j++) {
			WORD_MUL(prod_high, prod_low, m, p->val[j]);
			prod_low  = (word_t)(prod_low + carry);
			prod_high = (word_t)(prod_high + (prod_low < carry));
			out->val[j - 1] = (word_t)(prod_low + out->val[j]);
			carry = (word_t)(prod_high + (out->val[j - 1] < prod_low));
		}
		out->val[j - 1] = (word_t)(carry + out->val[j]);
		carry = (word_t)(out->val[j - 1] < out->val[j]);
		out->val[j] = (word_t)(acc + carry);
	}
	/*
	 * Note that at this stage the msw of out is either 0 or 1.
	 * If out > p we need to subtract p from out.
	 */
	ret = nn_cmp(out, p, &cmp); EG(ret, err);
	ret = nn_cnd_sub(cmp >= 0, out, out, p); EG(ret, err);
	MUST_HAVE((!nn_cmp(out, p, &cmp)) && (cmp < 0), ret, err);
	/* We restore out wlen. */
	out->wlen = old_wlen;

err:
	return ret;
}

/*
 * Wrapper for previous function handling aliasing of one of the input
 * paramter with out, through a copy. The function does not check
 * input parameters are initialized. This MUST be done by the caller.
 */
ATTRIBUTE_WARN_UNUSED_RET static int _nn_mul_redc1_aliased(nn_t out, nn_src_t in1, nn_src_t in2,
				 nn_src_t p, word_t mpinv)
{
	nn out_cpy;
	int ret;
	out_cpy.magic = WORD(0);

	ret = _nn_mul_redc1(&out_cpy, in1, in2, p, mpinv); EG(ret, err);
	ret = nn_init(out, out_cpy.wlen); EG(ret, err);
	ret = nn_copy(out, &out_cpy);

err:
	nn_uninit(&out_cpy);

	return ret;
}

/*
 * Public version, handling possible aliasing of out parameter with
 * one of the input parameters.
 */
int nn_mul_redc1(nn_t out, nn_src_t in1, nn_src_t in2, nn_src_t p,
		 word_t mpinv)
{
	int ret;

	ret = nn_check_initialized(in1); EG(ret, err);
	ret = nn_check_initialized(in2); EG(ret, err);
	ret = nn_check_initialized(p); EG(ret, err);

	/* Handle possible output aliasing */
	if ((out == in1) || (out == in2) || (out == p)) {
		ret = _nn_mul_redc1_aliased(out, in1, in2, p, mpinv);
	} else {
		ret = _nn_mul_redc1(out, in1, in2, p, mpinv);
	}

err:
	return ret;
}

/*
 * Compute in1 * in2 mod p where in1 and in2 are numbers < p.
 * When p is an odd number, the function redcifies in1 and in2
 * parameters, does the computation and then unredcifies the
 * result. When p is an even number, we use an unoptimized mul
 * then mod operations sequence.
 *
 * From a mathematical standpoint, the computation is equivalent
 * to performing:
 *
 *   nn_mul(&tmp2, in1, in2);
 *   nn_mod(&out, &tmp2, q);
 *
 * but the modular reduction is done progressively during
 * Montgomery reduction when p is odd (which brings more efficiency).
 *
 * Inputs are supposed to be < p (i.e. taken modulo p).
 *
 * The function returns 0 on success, -1 on error.
 */
int nn_mod_mul(nn_t out, nn_src_t in1, nn_src_t in2, nn_src_t p_in)
{
	nn r_square, p;
	nn in1_tmp, in2_tmp;
	word_t mpinv;
	int ret, isodd;
	r_square.magic = in1_tmp.magic = in2_tmp.magic = p.magic = WORD(0);

	/* When p_in is even, we cannot work with Montgomery multiplication */
	ret = nn_isodd(p_in, &isodd); EG(ret, err);
	if(!isodd){
		/* When p_in is even, we fallback to less efficient mul then mod */
		ret = nn_mul(out, in1, in2); EG(ret, err);
		ret = nn_mod(out, out, p_in); EG(ret, err);
	}
	else{
		/* Here, p_in is odd and we can use redcification */
		ret = nn_copy(&p, p_in); EG(ret, err);

		/*
		 * In order for our reciprocal division routines to work, it is
		 * expected that the bit length (including leading zeroes) of
		 * input prime p is >= 2 * wlen where wlen is the number of bits
		 * of a word size.
		 */
		if (p.wlen < 2) {
			ret = nn_set_wlen(&p, 2); EG(ret, err);
		}

		/* Compute Mongtomery coefs.
		 * NOTE: in1_tmp holds a dummy value here after the operation.
		 */
		ret = nn_compute_redc1_coefs(&in1_tmp, &r_square, &p, &mpinv); EG(ret, err);

		/* redcify in1 and in2 */
		ret = nn_mul_redc1(&in1_tmp, in1, &r_square, &p, mpinv); EG(ret, err);
		ret = nn_mul_redc1(&in2_tmp, in2, &r_square, &p, mpinv); EG(ret, err);

		/* Compute in1 * in2 mod p in montgomery world.
		 * NOTE: r_square holds the result after the operation.
		 */
		ret = nn_mul_redc1(&r_square, &in1_tmp, &in2_tmp, &p, mpinv); EG(ret, err);

		/* Come back to real world by unredcifying result */
		ret = nn_init(&in1_tmp, 0); EG(ret, err);
		ret = nn_one(&in1_tmp); EG(ret, err);
		ret = nn_mul_redc1(out, &r_square, &in1_tmp, &p, mpinv); EG(ret, err);
	}

err:
	nn_uninit(&p);
	nn_uninit(&r_square);
	nn_uninit(&in1_tmp);
	nn_uninit(&in2_tmp);

	return ret;
}
