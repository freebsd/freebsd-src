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
#include <libecc/nn/nn_rand.h>
#include <libecc/nn/nn_add.h>
#include <libecc/nn/nn_logical.h>
/* Include the "internal" header as we use non public API here */
#include "../nn/nn_div.h"


#include <libecc/external_deps/rand.h>

/*
 * The function initializes nn structure pointed by 'out' to a random value of
 * byte length 'len'. The resulting nn will have a uniformly random value in
 * [0, 2^(8 * len)[. Provided length 'len' parameter must be less than or equal
 * to NN_MAX_BYTE_LEN. The function returns -1 on error and 0 on success.
 */
int nn_get_random_len(nn_t out, u16 len)
{
	int ret;

	MUST_HAVE((len <= NN_MAX_BYTE_LEN), ret, err);

	ret = nn_init(out, len); EG(ret, err);
	ret = get_random((u8*) out->val, len);

err:
	return ret;
}

/*
 * The function initializes nn structure pointed by 'out' to a random value of
 * *random* byte length less than or equal to 'max_len'. Unlike the function
 * above (nn_get_random_len()), the resulting nn will have a uniformly random
 * value in in [0, 2^(8 * len)[ *with* length selected at random in
 * [0, max_len]. The function returns -1 on error and 0 on success.
 *
 * !! NOTE !!: think twice before using this function for anything other than
 * testing purposes. Its main goal is to generate nn with random length, not
 * random numbers. For instance, for a given value of max_len, the function
 * returns a nn with a value of 0 w/ probability 1/max_len.
 */
int nn_get_random_maxlen(nn_t out, u16 max_len)
{
	u16 len;
	int ret;

	MUST_HAVE((max_len <= NN_MAX_BYTE_LEN), ret, err);

	ret = get_random((u8 *)&len, 2); EG(ret, err);

	len = (u16)(len % (max_len + 1));

	ret = nn_get_random_len(out, len);

err:
	return ret;
}

/*
 * On success, the return value of the function is 0 and 'out' parameter
 * is initialized to an unbiased random value in ]0,q[. On error, the
 * function returns -1. Due to the generation process described below,
 * the size of q is limited by NN_MAX_BYTE_LEN / 2. Aliasing is supported.
 *
 * Generating a random value in ]0,q[ is done by reducing a large random
 * value modulo q. The random value is taken with a length twice the one
 * of q to ensure the reduction does not produce a biased value.
 *
 * Even if this is unlikely to happen, the reduction can produce a null
 * result; this specific case would require to repeat the whole process.
 * For that reason, the algorithm we implement works in the following
 * way:
 *
 *  1) compute q' = q - 1                   (note: q is neither 0 nor 1)
 *  2) generate a random value tmp_rand twice the size of q
 *  3) compute out = tmp_rand mod q'        (note: out is in [0, q-2])
 *  4) compute out += 1                     (note: out is in [1, q-1])
 *
 * Aliasing is supported.
 */
int nn_get_random_mod(nn_t out, nn_src_t q)
{
	nn tmp_rand, qprime;
	bitcnt_t q_bit_len, q_len;
	int ret, isone;
	qprime.magic = tmp_rand.magic = WORD(0);

	/* Check q is initialized and get its bit length */
	ret = nn_check_initialized(q); EG(ret, err);
	ret = nn_bitlen(q, &q_bit_len); EG(ret, err);
	q_len = (bitcnt_t)BYTECEIL(q_bit_len);

	/* Check q is neither 0, nor 1 and its size is ok */
	MUST_HAVE((q_len) && (q_len <= (NN_MAX_BYTE_LEN / 2)), ret, err);
	MUST_HAVE((!nn_isone(q, &isone)) && (!isone), ret, err);

	/* 1) compute q' = q - 1  */
	ret = nn_copy(&qprime, q); EG(ret, err);
	ret = nn_dec(&qprime, &qprime); EG(ret, err);

	/* 2) generate a random value tmp_rand twice the size of q */
	ret = nn_init(&tmp_rand, (u16)(2 * q_len)); EG(ret, err);
	ret = get_random((u8 *)tmp_rand.val, (u16)(2 * q_len)); EG(ret, err);

	/* 3) compute out = tmp_rand mod q' */
	ret = nn_init(out, (u16)q_len); EG(ret, err);

	/* Use nn_mod_notrim to avoid exposing the generated random length */
	ret = nn_mod_notrim(out, &tmp_rand, &qprime); EG(ret, err);

	/* 4) compute out += 1 */
	ret = nn_inc(out, out);

 err:
	nn_uninit(&qprime);
	nn_uninit(&tmp_rand);

	return ret;
}
