/*
 *  Copyright (C) 2021 - This file is part of libecc project
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
#include <libecc/nn/nn_div_public.h>
#include <libecc/nn/nn_logical.h>
#include <libecc/nn/nn_mod_pow.h>
#include <libecc/nn/nn_rand.h>
#include <libecc/nn/nn.h>

/*
 * NOT constant time with regard to the bitlength of exp.
 *
 * Implements a left to right Montgomery Ladder for modular exponentiation.
 * This is an internal common helper and assumes that all the initialization
 * and aliasing of inputs/outputs are handled by the callers. Depending on
 * the inputs, redcification is optionally used.
 * The base is reduced if necessary.
 *
 * Montgomery Ladder is masked using Itoh et al. anti-ADPA
 * (Address-bit DPA) countermeasure.
 * See "A Practical Countermeasure against Address-Bit Differential Power Analysis"
 * by Itoh, Izu and Takenaka for more information.

 * Returns 0 on success, -1 on error.
 */
ATTRIBUTE_WARN_UNUSED_RET static int _nn_exp_monty_ladder_ltr(nn_t out, nn_src_t base, nn_src_t exp, nn_src_t mod, nn_src_t r, nn_src_t r_square, word_t mpinv)
{
	nn T[3];
	nn mask;
	bitcnt_t explen, oldexplen;
 	u8 expbit, rbit;
	int ret, cmp;
	T[0].magic = T[1].magic = T[2].magic = mask.magic = WORD(0);

	/* Initialize out */
	ret = nn_init(out, 0); EG(ret, err);

	ret = nn_init(&T[0], 0); EG(ret, err);
	ret = nn_init(&T[1], 0); EG(ret, err);
	ret = nn_init(&T[2], 0); EG(ret, err);

	/* Generate our Itoh random mask */
	ret = nn_get_random_len(&mask, NN_MAX_BYTE_LEN); EG(ret, err);

	ret = nn_bitlen(exp, &explen); EG(ret, err);


	/* From now on, since we deal with Itoh's countermeasure, we must have at
	 * least 2 bits in the exponent. We will deal with the particular cases of 0 and 1
	 * bit exponents later.
	 */
	oldexplen = explen;
	explen = (explen < 2) ? 2 : explen;

	ret = nn_getbit(&mask, (bitcnt_t)(explen - 1), &rbit); EG(ret, err);

	/* Reduce the base if necessary */
	ret = nn_cmp(base, mod, &cmp); EG(ret, err);
	if(cmp >= 0){
		/* Modular reduction */
		ret = nn_mod(&T[rbit], base, mod); EG(ret, err);
		if(r != NULL){
			/* Redcify the base if necessary */
			ret = nn_mul_redc1(&T[rbit], &T[rbit], r_square, mod, mpinv); EG(ret, err);
		}
	}
	else{
		if(r != NULL){
			/* Redcify the base if necessary */
			ret = nn_mul_redc1(&T[rbit], base, r_square, mod, mpinv); EG(ret, err);
		}
		else{
			ret = nn_copy(&T[rbit], base); EG(ret, err);
		}
	}

	/* We implement the Montgomery ladder exponentiation with Itoh masking using three
	 * registers T[0], T[1] and T[2]. The random mask is in 'mask'.
	 */
	if(r != NULL){
		ret = nn_mul_redc1(&T[1-rbit], &T[rbit], &T[rbit], mod, mpinv); EG(ret, err);
	}
	else{
		ret = nn_mod_mul(&T[1-rbit], &T[rbit], &T[rbit], mod); EG(ret, err);
	}

	/* Now proceed with the Montgomery Ladder algorithm.
	 */
	explen = (bitcnt_t)(explen - 1);
	while (explen > 0) {
		u8 rbit_next;
		explen = (bitcnt_t)(explen - 1);

		/* rbit is r[i+1], and rbit_next is r[i] */
		ret = nn_getbit(&mask, explen, &rbit_next); EG(ret, err);
		/* Get the exponent bit */
		ret = nn_getbit(exp, explen, &expbit); EG(ret, err);

		/* Square */
		if(r != NULL){
			ret = nn_mul_redc1(&T[2], &T[expbit ^ rbit], &T[expbit ^ rbit], mod, mpinv); EG(ret, err);
		}
		else{
			ret = nn_mod_mul(&T[2], &T[expbit ^ rbit], &T[expbit ^ rbit], mod); EG(ret, err);
		}
		/* Multiply */
		if(r != NULL){
			ret = nn_mul_redc1(&T[1], &T[0], &T[1], mod, mpinv); EG(ret, err);
		}
		else{
			ret = nn_mod_mul(&T[1], &T[0], &T[1], mod); EG(ret, err);
		}
		/* Copy */
		ret = nn_copy(&T[0], &T[2 - (expbit ^ rbit_next)]); EG(ret, err);
		ret = nn_copy(&T[1], &T[1 + (expbit ^ rbit_next)]); EG(ret, err);
		/* Update rbit */
		rbit = rbit_next;
	}
	ret = nn_one(&T[1 - rbit]);
	if(r != NULL){
		/* Unredcify in out */
		ret = nn_mul_redc1(&T[rbit], &T[rbit], &T[1 - rbit], mod, mpinv); EG(ret, err);
	}

	/* Deal with the particular cases of 0 and 1 bit exponents */
	/* Case with 0 bit exponent: T[1 - rbit] contains 1 modulo mod */
	ret = nn_mod(&T[1 - rbit], &T[1 - rbit], mod); EG(ret, err);
	/* Case with 1 bit exponent */
	ret = nn_mod(&T[2], base, mod); EG(ret, err);
	/* Proceed with the output */
	ret = nn_cnd_swap((oldexplen == 0), out, &T[1 - rbit]);
	ret = nn_cnd_swap((oldexplen == 1), out, &T[2]);
	ret = nn_cnd_swap(((oldexplen != 0) && (oldexplen != 1)), out, &T[rbit]);

err:
	nn_uninit(&T[0]);
	nn_uninit(&T[1]);
	nn_uninit(&T[2]);
	nn_uninit(&mask);

	return ret;
}

/*
 * NOT constant time with regard to the bitlength of exp.
 *
 * Reduces the base modulo mod if it is not already reduced,
 * which is also a small divergence wrt constant time leaking
 * the information that base <= mod or not: please use with care
 * in the callers if this information is sensitive.
 *
 * Aliasing not supported. Expects caller to check parameters
 * have been initialized. This is an internal helper.
 *
 * Compute (base ** exp) mod (mod) using a Montgomery Ladder algorithm
 * with Montgomery redcification, hence the Montgomery coefficients as input.
 * The module "mod" is expected to be odd for redcification to be used.
 *
 * Returns 0 on success, -1 on error.
 */
ATTRIBUTE_WARN_UNUSED_RET static int _nn_mod_pow_redc(nn_t out, nn_src_t base, nn_src_t exp, nn_src_t mod, nn_src_t r, nn_src_t r_square, word_t mpinv)
{
	return _nn_exp_monty_ladder_ltr(out, base, exp, mod, r, r_square, mpinv);
}

/*
 * NOT constant time with regard to the bitlength of exp.
 *
 * Reduces the base modulo mod if it is not already reduced,
 * which is also a small divergence wrt constant time leaking
 * the information that base <= mod or not: please use with care
 * in the callers if this information is sensitive.
 *
 * Aliasing is supported. Expects caller to check parameters
 * have been initialized. This is an internal helper.
 *
 * Compute (base ** exp) mod (mod) using a Montgomery Ladder algorithm.
 * This function works for all values of "mod", but is slower that the one
 * using Montgomery multiplication (which only works for odd "mod"). Hence,
 * it is only used on even "mod" by upper layers.
 *
 * Returns 0 on success, -1 on error.
 */
ATTRIBUTE_WARN_UNUSED_RET static int _nn_mod_pow(nn_t out, nn_src_t base, nn_src_t exp, nn_src_t mod)
{
	int ret;

	if ((out == base) || (out == exp) || (out == mod)) {
		nn _out;
		_out.magic = WORD(0);

		ret = nn_init(&_out, 0); EG(ret, err);
		ret = _nn_exp_monty_ladder_ltr(&_out, base, exp, mod, NULL, NULL, WORD(0)); EG(ret, err);
		ret = nn_copy(out, &_out);
	}
	else{
		ret = _nn_exp_monty_ladder_ltr(out, base, exp, mod, NULL, NULL, WORD(0));
	}

err:
	return ret;
}

/*
 * Same purpose as above but handles aliasing.
 * Expects caller to check parameters
 * have been initialized. This is an internal helper.
 */
ATTRIBUTE_WARN_UNUSED_RET static int _nn_mod_pow_redc_aliased(nn_t out, nn_src_t base, nn_src_t exp, nn_src_t mod, nn_src_t r, nn_src_t r_square, word_t mpinv)
{
	nn _out;
	int ret;
	_out.magic = WORD(0);

	ret = nn_init(&_out, 0); EG(ret, err);
	ret = _nn_mod_pow_redc(&_out, base, exp, mod, r, r_square, mpinv); EG(ret, err);
	ret = nn_copy(out, &_out);

err:
	nn_uninit(&_out);

	return ret;
}

/* Aliased version of previous one.
 * NOTE: our nn_mod_pow_redc primitives suppose that the modulo is odd for Montgomery multiplication
 * primitives to provide consistent results.
 *
 * Aliasing is supported.
 */
int nn_mod_pow_redc(nn_t out, nn_src_t base, nn_src_t exp, nn_src_t mod, nn_src_t r, nn_src_t r_square, word_t mpinv)
{
	int ret, isodd;

	ret = nn_check_initialized(base); EG(ret, err);
	ret = nn_check_initialized(exp); EG(ret, err);
	ret = nn_check_initialized(mod); EG(ret, err);
	ret = nn_check_initialized(r); EG(ret, err);
	ret = nn_check_initialized(r_square); EG(ret, err);

	/* Check that we have an odd number for our modulo */
	ret = nn_isodd(mod, &isodd); EG(ret, err);
	MUST_HAVE(isodd, ret, err);

	/* Handle the case where our prime is less than two words size.
	 * We need it to be >= 2 words size for the Montgomery multiplication to be
	 * usable.
	 */
	if(mod->wlen < 2){
		/* Local copy our modulo */
		nn _mod;
		_mod.magic = WORD(0);

		/* And set its length accordingly */
		ret = nn_copy(&_mod, mod); EG(ret, err1);
		ret = nn_set_wlen(&_mod, 2); EG(ret, err1);
		/* Handle output aliasing */
		if ((out == base) || (out == exp) || (out == mod) || (out == r) || (out == r_square)) {
			ret = _nn_mod_pow_redc_aliased(out, base, exp, &_mod, r, r_square, mpinv); EG(ret, err1);
		} else {
			ret = _nn_mod_pow_redc(out, base, exp, &_mod, r, r_square, mpinv); EG(ret, err1);
		}
err1:
		nn_uninit(&_mod);
		EG(ret, err);
	}
	else{
		/* Handle output aliasing */
		if ((out == base) || (out == exp) || (out == mod) || (out == r) || (out == r_square)) {
			ret = _nn_mod_pow_redc_aliased(out, base, exp, mod, r, r_square, mpinv);
		} else {
			ret = _nn_mod_pow_redc(out, base, exp, mod, r, r_square, mpinv);
		}
	}

err:
	return ret;
}


/*
 * NOT constant time with regard to the bitlength of exp.
 * Aliasing is supported.
 *
 * Compute (base ** exp) mod (mod) using a Montgomery Ladder algorithm.
 * Internally, this computes Montgomery coefficients and uses the redc
 * function.
 *
 * Returns 0 on success, -1 on error.
 */
int nn_mod_pow(nn_t out, nn_src_t base, nn_src_t exp, nn_src_t mod)
{
	nn r, r_square;
	word_t mpinv;
	int ret, isodd;
	r.magic = r_square.magic = WORD(0);

	/* Handle the case where our modulo is even: in this case, we cannot
	 * use our Montgomery multiplication primitives as they are only suitable
	 * for odd numbers. We fallback to less efficient regular modular exponentiation.
	 */
	ret = nn_isodd(mod, &isodd); EG(ret, err);
	if(!isodd){
		/* mod is even: use the regular unoptimized modular exponentiation */
		ret = _nn_mod_pow(out, base, exp, mod);
	}
	else{
		/* mod is odd */
		/* Compute the Montgomery coefficients */
		ret = nn_compute_redc1_coefs(&r, &r_square, mod, &mpinv); EG(ret, err);

		/* Now use the redc version */
		ret = nn_mod_pow_redc(out, base, exp, mod, &r, &r_square, mpinv);
	}

err:
	nn_uninit(&r);
	nn_uninit(&r_square);

	return ret;
}
