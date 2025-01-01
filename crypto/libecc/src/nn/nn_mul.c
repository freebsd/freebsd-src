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
/* Use internal API header */
#include "nn_mul.h"

/*
 * Compute out = (in1 * in2) & (2^(WORD_BYTES * wlimits) - 1).
 *
 * The function is constant time for all sets of parameters of given
 * lengths.
 *
 * Implementation: while most generic library implement some advanced
 * algorithm (Karatsuba, Toom-Cook, or FFT based algorithms)
 * which provide a performance advantage for large numbers, the code
 * below is mainly oriented towards simplicity and readibility. It is
 * a direct writing of the naive multiplication algorithm one has
 * learned in school.
 *
 * Portability: in order for the code to be portable, all word by
 * word multiplication are actually performed by an helper macro
 * on half words.
 *
 * Note: 'out' is initialized by the function (caller can omit it)
 *
 * Internal use only. Check on input nn left to the caller.
 *
 * The function returns 0 on succes, -1 on error.
 */
ATTRIBUTE_WARN_UNUSED_RET static int _nn_mul_low(nn_t out, nn_src_t in1, nn_src_t in2,
			u8 wlimit)
{
	word_t carry, prod_high, prod_low;
	u8 i, j, pos;
	int ret;

	/* We have to check that wlimit does not exceed our NN_MAX_WORD_LEN */
	MUST_HAVE(((wlimit * WORD_BYTES) <= NN_MAX_BYTE_LEN), ret, err);

	ret = nn_init(out, (u16)(wlimit * WORD_BYTES)); EG(ret, err);

	for (i = 0; i < in1->wlen; i++) {
		carry = 0;
		pos = 0;

		for (j = 0; j < in2->wlen; j++) {
			pos = (u8)(i + j);

			/*
			 * size of the result provided by the caller may not
			 * be large enough for what multiplication may
			 * generate.
			 */
			if (pos >= wlimit) {
				continue;
			}

			/*
			 * Compute the result of the multiplication of
			 * two words.
			 */
			WORD_MUL(prod_high, prod_low,
				 in1->val[i], in2->val[j]);
			/*
			 * And add previous carry.
			 */
			prod_low  = (word_t)(prod_low + carry);
			prod_high = (word_t)(prod_high + (prod_low < carry));

			/*
			 * Add computed word to what we can currently
			 * find at current position in result.
			 */
			out->val[pos] = (word_t)(out->val[pos] + prod_low);
			carry = (word_t)(prod_high + (out->val[pos] < prod_low));
		}

		/*
		 * What remains in acc_high at end of previous loop should
		 * be added to next word after pos in result.
		 */
		if ((pos + 1) < wlimit) {
			out->val[pos + 1] = (word_t)(out->val[pos + 1] + carry);
		}
	}

err:
	return ret;
}

/* Aliased version. Internal use only. Check on input nn left to the caller */
ATTRIBUTE_WARN_UNUSED_RET static int _nn_mul_low_aliased(nn_t out, nn_src_t in1, nn_src_t in2,
			       u8 wlimit)
{
	nn out_cpy;
	int ret;
	out_cpy.magic = WORD(0);

	ret = _nn_mul_low(&out_cpy, in1, in2, wlimit); EG(ret, err);
	ret = nn_init(out, out_cpy.wlen); EG(ret, err);
	ret = nn_copy(out, &out_cpy);

err:
	nn_uninit(&out_cpy);

	return ret;
}

/* Public version supporting aliasing. */
int nn_mul_low(nn_t out, nn_src_t in1, nn_src_t in2, u8 wlimit)
{
	int ret;

	ret = nn_check_initialized(in1); EG(ret, err);
	ret = nn_check_initialized(in2); EG(ret, err);

	/* Handle output aliasing */
	if ((out == in1) || (out == in2)) {
		ret = _nn_mul_low_aliased(out, in1, in2, wlimit);
	} else {
		ret = _nn_mul_low(out, in1, in2, wlimit);
	}

err:
	return ret;
}

/*
 * Compute out = in1 * in2. 'out' is initialized by the function.
 * The function returns 0 on success, -1 on error.
 *
 * Aliasing supported.
 */
int nn_mul(nn_t out, nn_src_t in1, nn_src_t in2)
{
	int ret;

	ret = nn_check_initialized(in1); EG(ret, err);
	ret = nn_check_initialized(in2); EG(ret, err);
	ret = nn_mul_low(out, in1, in2, (u8)(in1->wlen + in2->wlen));

err:
	return ret;
}

int nn_sqr_low(nn_t out, nn_src_t in, u8 wlimit)
{
	return nn_mul_low(out, in, in, wlimit);
}

/*
 * Compute out = in * in. 'out' is initialized by the function.
 * The function returns 0 on success, -1 on error.
 *
 * Aliasing supported.
 */
int nn_sqr(nn_t out, nn_src_t in)
{
	return nn_mul(out, in, in);
}

/*
 * Multiply a multiprecision number by a word, i.e. out = in * w. The function
 * returns 0 on success, -1 on error.
 *
 * Aliasing supported.
 */
int nn_mul_word(nn_t out, nn_src_t in, word_t w)
{
	nn w_nn;
	int ret;
	w_nn.magic = WORD(0);

	ret = nn_check_initialized(in); EG(ret, err);
	ret = nn_init(&w_nn, WORD_BYTES); EG(ret, err);
	w_nn.val[0] = w;
	ret = nn_mul(out, in, &w_nn);

err:
	nn_uninit(&w_nn);

	return ret;
}
