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
#define NN_CONSISTENCY_CHECK
#include <libecc/nn/nn.h>

/* 
 * Used for the conditional swap algorithm SCA
 * resistance, see below in the implementation of
 * nn_cnd_swap.
 */
#include <libecc/utils/utils_rand.h>

/*
 * Except otherwise specified, all functions accept *initialized* nn.
 * The WORD(NN_MAX_WORD_LEN + WORDSIZE) magic is here to detect modules
 * compiled with different WORDSIZE or NN_MAX_WORD_LEN and are binary
 * incompatible.
 */

#define NN_MAGIC ((word_t)((0xb4cf5d56e2023316ULL ^ (WORD(NN_MAX_WORD_LEN + WORDSIZE)))))

/*
 * Local helper internally used to check that the storage space
 * above wlen is made of zero words. The function does NOT check
 * if given nn has been initialized. This must have been done
 * by the caller.
 *
 * Due to its performance cost, this consistency check is used
 * in SHOULD_HAVE macros, meaning that it will only be present
 * in DEBUG mode. Hence the ATTRIBUTE_UNUSED so that no warning
 * (error in -Werror) is triggered at compilation time.
 *
 */
ATTRIBUTE_WARN_UNUSED_RET static int ATTRIBUTE_UNUSED __nn_is_wlen_consistent(nn_src_t A)
{
	word_t val = 0;
	u8 i;

	for (i = A->wlen; i < NN_MAX_WORD_LEN; i++) {
		val |= (A)->val[i];
	}

	return (val == 0);
}

/*
 * Verify that pointed nn has already been initialized. This function
 * should be used as a safety net in all function before using a nn
 * received as parameter. Returns 0 on success, -1 on error.
 */
int nn_check_initialized(nn_src_t A)
{
	int ret;

	MUST_HAVE((A != NULL), ret, err);
	MUST_HAVE((A->magic == NN_MAGIC), ret, err);
	MUST_HAVE((A->wlen <= NN_MAX_WORD_LEN), ret, err);
	SHOULD_HAVE(__nn_is_wlen_consistent(A), ret, err);

	ret = 0;

err:
	return ret;
}

/*
 * Initialize nn from expected initial byte length 'len', setting its wlen
 * to associated (ceil) value and clearing whole storage space. Return 0
 * on success, -1 on error.
 */
int nn_init(nn_t A, u16 len)
{
	int ret;
	u8 i;

	MUST_HAVE(((A != NULL) && (len <= NN_MAX_BYTE_LEN)), ret, err);

	A->wlen = (u8)BYTE_LEN_WORDS(len);
	A->magic = NN_MAGIC;

	for (i = 0; i < NN_MAX_WORD_LEN; i++) {
		A->val[i] = WORD(0);
	}

	ret = 0;

err:
	return ret;
}

/*
 * Uninitialize the pointed nn to prevent further use (magic field in the
 * structure is zeroized). The associated storage space is also zeroized. If
 * given pointer is NULL or does not point to an initialized nn, the function
 * does nothing.
 */
void nn_uninit(nn_t A)
{
	if ((A != NULL) && (A->magic == NN_MAGIC)) {
		int i;

		for (i = 0; i < NN_MAX_WORD_LEN; i++) {
			A->val[i] = WORD(0);
		}

		A->wlen = 0;
		A->magic = WORD(0);
	}

	return;
}

/*
 * Set current value of pointed initialized nn to 0. Returns 0 on success, -1
 * on error.
 */
int nn_zero(nn_t A)
{
	int ret;

	ret = nn_check_initialized(A); EG(ret, err);
	ret = nn_init(A, 0);

err:
	return ret;
}

/*
 * Set current value of pointed initialized nn to given word value. Returns 0
 * on success, -1 on error.
 */
int nn_set_word_value(nn_t A, word_t val)
{
	int ret;

	ret = nn_zero(A); EG(ret, err);

	A->val[0] = val;
	A->wlen = 1;

err:
	return ret;
}

/*
 * Set current value of pointed initialized nn to 1. Returns 0 on success, -1
 * on error.
 */
int nn_one(nn_t A)
{
	return nn_set_word_value(A, WORD(1));
}

/*
 * Conditionally swap two nn's content *in constant time*. Swapping is done
 * if 'cnd' is not zero. Nothing is done otherwise. Returns 0 on success, -1
 * on error.
 *
 * Aliasing of inputs is supported.
 */
int nn_cnd_swap(int cnd, nn_t in1, nn_t in2)
{
	word_t mask = WORD_MASK_IFNOTZERO(cnd);
	u8 len, i;
	word_t t, r;
	volatile word_t r_mask;
	int ret;

	ret = nn_check_initialized(in1); EG(ret, err);
	ret = nn_check_initialized(in2); EG(ret, err);

	MUST_HAVE((in1->wlen <= NN_MAX_WORD_LEN), ret, err);
	MUST_HAVE((in2->wlen <= NN_MAX_WORD_LEN), ret, err);

	len = (in1->wlen >= in2->wlen) ? in1->wlen : in2->wlen;

	/* Use a random word for randomly masking the delta value hamming
	 * weight as proposed in Algorithm 4 of "Nonce@once: A Single-Trace
	 * EM Side Channel Attack on Several Constant-Time Elliptic
	 * Curve Implementations in Mobile Platforms" by Alam et al.
	 */
	ret = get_unsafe_random((u8*)&r, sizeof(r)); EG(ret, err);
	r_mask = r;

	for (i = 0; i < NN_MAX_WORD_LEN; i++) {
		word_t local_mask = WORD_MASK_IFNOTZERO((i < len));
		t = ((in1->val[i] ^ in2->val[i]) & mask) ^ r_mask;
		in1->val[i] ^= ((t & local_mask) ^ (r_mask & local_mask));
		in2->val[i] ^= ((t & local_mask) ^ (r_mask & local_mask));
	}

	t = (word_t)(((in1->wlen ^ in2->wlen) & mask) ^ r_mask);
	in1->wlen ^= (u8)(t ^ r_mask);
	in2->wlen ^= (u8)(t ^ r_mask);

err:
	return ret;
}

/*
 * Adjust internal wlen attribute of given nn to new_wlen. If internal wlen
 * attribute value is reduced, words above that limit in A are zeroized.
 * new_wlen must be in [0, NN_MAX_WORD_LEN].
 * The trimming is performed in constant time wrt to the length of the
 * input to avoid leaking it.
 * Returns 0 on success, -1 on error.
 */
int nn_set_wlen(nn_t A, u8 new_wlen)
{
	int ret;
	u8 i;

	ret = nn_check_initialized(A); EG(ret, err);
	MUST_HAVE((new_wlen <= NN_MAX_WORD_LEN), ret, err);
	MUST_HAVE((A->wlen <= NN_MAX_WORD_LEN), ret, err);

	/* Trimming performed in constant time */
	for (i = 0; i < NN_MAX_WORD_LEN; i++) {
		A->val[i] = (word_t)(A->val[i] & WORD_MASK_IFZERO((i >= new_wlen)));
	}

	A->wlen = new_wlen;

err:
	return ret;
}

/*
 * The function tests if given nn value is zero. The result of the test is given
 * using 'iszero' out parameter (1 if nn is zero, 0 if it is not). The function
 * returns 0 on success, -1 on error. 'iszero' is not meaningfull on error.
 * When A is valid, check is done *in constant time*.
 */
int nn_iszero(nn_src_t A, int *iszero)
{
	int ret, notzero;
	u8 i;

	ret = nn_check_initialized(A); EG(ret, err);
	MUST_HAVE((A->wlen <= NN_MAX_WORD_LEN), ret, err);
	MUST_HAVE((iszero != NULL), ret, err);

	notzero = 0;
	for (i = 0; i < NN_MAX_WORD_LEN; i++) {
		int mask = ((i < A->wlen) ? 1 : 0);
		notzero |= ((A->val[i] != 0) & mask);
	}

	*iszero = !notzero;

err:
	return ret;
}

/*
 * The function tests if given nn value is one. The result of the test is given
 * using 'isone' out parameter (1 if nn is one, 0 if it is not). The function
 * returns 0 on success, -1 on error. 'isone' is not meaningfull on error.
 * When A is valid, check is done *in constant time*.
 */
int nn_isone(nn_src_t A, int *isone)
{
	int ret, notone;
	u8 i;

	ret = nn_check_initialized(A); EG(ret, err);
	MUST_HAVE(!(A->wlen > NN_MAX_WORD_LEN), ret, err);
	MUST_HAVE((isone != NULL), ret, err);

	/* val[0] access is ok no matter wlen value */
	notone = (A->val[0] != 1);
	for (i = 1; i < NN_MAX_WORD_LEN; i++) {
		int mask = ((i < A->wlen) ? 1 : 0);
		notone |= ((A->val[i] != 0) & mask);
	}

	*isone = !notone;

err:
	return ret;
}

/*
 * The function tests if given nn value is odd. The result of the test is given
 * using 'isodd' out parameter (1 if nn is odd, 0 if it is not). The function
 * returns 0 on success, -1 on error. 'isodd' is not meaningfull on error.
 */
int nn_isodd(nn_src_t A, int *isodd)
{
	int ret;

	ret = nn_check_initialized(A); EG(ret, err);
	MUST_HAVE((isodd != NULL), ret, err);

	*isodd = (A->wlen != 0) && (A->val[0] & 1);

err:
	return ret;
}

/*
 * Compare given nn against given word value. This is done *in constant time*
 * (only depending on the input length, not on its value or on the word value)
 * when provided nn is valid. The function returns 0 on success and provides
 * the comparison value in 'cmp' parameter. -1 is returned on error, in which
 * case 'cmp' is not meaningful.
 */
int nn_cmp_word(nn_src_t in, word_t w, int *cmp)
{
	int ret, tmp = 0;
	word_t mask;
	u8 i;

	ret = nn_check_initialized(in); EG(ret, err);
	MUST_HAVE((cmp != NULL), ret, err);

	/* No need to read, we can conclude */
	if (in->wlen == 0) {
		*cmp = -(w != 0);
		ret = 0;
		goto err;
	}

	/*
	 * Let's loop on all words above first one to see if one
	 * of those is non-zero.
	 */
	for (i = (u8)(in->wlen - 1); i > 0; i--) {
		tmp |= (in->val[i] != 0);
	}

	/*
	 * Compare first word of nn w/ w if needed. This
	 * is done w/ masking to avoid doing or not doing
	 * it based on 'tmp' (i.e. fact that a high word
	 * of nn is not zero).
	 */
	mask = WORD_MASK_IFZERO(tmp);
	tmp += (int)(((word_t)(in->val[i] > w)) & (mask));
	tmp -= (int)(((word_t)(in->val[i] < w)) & (mask));
	*cmp = tmp;

err:
	return ret;
}

/*
 * Compare given two nn 'A' and '. This is done *in constant time* (only
 * depending on the largest length of the inputs, not on their values). The
 * function returns 0 on success and provides the comparison value in
 * 'cmp' parameter (0 if A == B, -1 if A < B, +1 if A > B). -1 is returned
 * on error, in which case 'cmp' is not meaningful.
 *
 * Aliasing of inputs is supported.
 */
int nn_cmp(nn_src_t A, nn_src_t B, int *cmp)
{
	int tmp, mask, ret, i;
	u8 cmp_len;

	ret = nn_check_initialized(A); EG(ret, err);
	ret = nn_check_initialized(B); EG(ret, err);
	MUST_HAVE((cmp != NULL), ret, err);

	cmp_len = (A->wlen >= B->wlen) ? A->wlen : B->wlen;

	tmp = 0;
	for (i = (cmp_len - 1); i >= 0; i--) {	/* ok even if cmp_len is 0 */
		mask = !(tmp & 0x1);
		tmp += ((A->val[i] > B->val[i]) & mask);
		tmp -= ((A->val[i] < B->val[i]) & mask);
	}
	(*cmp) = tmp;

err:
	return ret;
}

/*
 * Copy given nn 'src_nn' value into 'dst_nn'. This is done *in constant time*.
 * 'dst_nn' must point to a declared nn, but *need not be initialized*; it will
 * be (manually) initialized by the function. 'src_nn' must have been
 * initialized prior to the call. The function returns 0 on success, -1 on error.
 *
 * Alising of input and output is supported.
 */
int nn_copy(nn_t dst_nn, nn_src_t src_nn)
{
	int ret;
	u8 i;

	MUST_HAVE((dst_nn != NULL), ret, err);
	ret = nn_check_initialized(src_nn); EG(ret, err);

	for (i = 0; i < NN_MAX_WORD_LEN; i++) {
		dst_nn->val[i] = src_nn->val[i];
	}

	dst_nn->wlen = src_nn->wlen;
	dst_nn->magic = NN_MAGIC;

err:
	return ret;
}

/*
 * Update wlen value of given nn if a set of words below wlen value are zero.
 * The function is *not constant time*, i.e. it depends on the input value.
 * The function returns 0 on sucess, -1 on error.
 */
int nn_normalize(nn_t in1)
{
	int ret;

	ret = nn_check_initialized(in1); EG(ret, err);

	while ((in1->wlen > 0) && (in1->val[in1->wlen - 1] == 0)) {
		in1->wlen--;
	}

err:
	return ret;
}

/*
 * Convert given consecutive WORD_BYTES bytes pointed by 'val' from network (big
 * endian) order to host order. 'val' needs not point to a word-aligned region.
 * The function returns 0 on success, -1 on error. On success, the result is
 * provided in 'out'. 'out' is not meaningful on error.
 */
ATTRIBUTE_WARN_UNUSED_RET static int _ntohw(const u8 *val, word_t *out)
{
	word_t res = 0;
	u8 *res_buf = (u8 *)(&res);
	int i, ret;

	MUST_HAVE(((val != NULL) && (out != NULL)), ret, err);

	if (arch_is_big_endian()) {
		/* copy bytes, one by one to avoid alignement issues */
		for (i = 0; i < WORD_BYTES; i++) {
			res_buf[i] = val[i];
		}
	} else {
		u8 tmp;

		for (i = 0; i < (WORD_BYTES / 2); i++) {
			tmp = val[i];
			res_buf[i] = val[WORD_BYTES - i - 1];
			res_buf[WORD_BYTES - i - 1] = tmp;
		}

		VAR_ZEROIFY(tmp);
	}

	*out = res;
	ret = 0;

err:
	return ret;
}

/* Same as previous function but from host to network byte order. */
ATTRIBUTE_WARN_UNUSED_RET static inline int _htonw(const u8 *val, word_t *out)
{
	return _ntohw(val, out);
}

/*
 * 'out_nn' is expected to point to the storage location of a declared nn,
 * which will be initialized by the function (i.e. given nn need not be
 * initialized). The function then imports value (expected to be in big
 * endian) from given buffer 'buf' of length 'buflen' into it. The function
 * expects (and enforces) that buflen is less than or equal to NN_MAX_BYTE_LEN.
 * The function returns 0 on success, -1 on error.
 */
int nn_init_from_buf(nn_t out_nn, const u8 *buf, u16 buflen)
{
	u8 tmp[NN_MAX_BYTE_LEN];
	u16 wpos;
	int ret;

	MUST_HAVE(((out_nn != NULL) && (buf != NULL) &&
		  (buflen <= NN_MAX_BYTE_LEN)), ret, err);

	ret = local_memset(tmp, 0, (u32)(NN_MAX_BYTE_LEN - buflen)); EG(ret, err);
	ret = local_memcpy(tmp + NN_MAX_BYTE_LEN - buflen, buf, buflen); EG(ret, err);

	ret = nn_init(out_nn, buflen); EG(ret, err);

	for (wpos = 0; wpos < NN_MAX_WORD_LEN; wpos++) {
		u16 buf_pos = (u16)((NN_MAX_WORD_LEN - wpos - 1) * WORD_BYTES);
		ret = _ntohw(tmp + buf_pos, &(out_nn->val[wpos])); EG(ret, err);
	}

	ret = local_memset(tmp, 0, NN_MAX_BYTE_LEN);

err:
	return ret;
}

/*
 * Export 'buflen' LSB bytes of given nn as a big endian buffer. If buffer
 * length is larger than effective size of input nn, padding w/ zero is
 * performed. If buffer size is smaller than input nn effective size,
 * MSB bytes are simply lost in exported buffer. The function returns 0
 * on success, -1 on error.
 */
int nn_export_to_buf(u8 *buf, u16 buflen, nn_src_t in_nn)
{
	u8 *src_word_ptr, *dst_word_ptr;
	const u8 wb = WORD_BYTES;
	u16 remain = buflen;
	int ret;
	u8 i;

	MUST_HAVE((buf != NULL), ret, err);
	ret = nn_check_initialized(in_nn); EG(ret, err);

	ret = local_memset(buf, 0, buflen); EG(ret, err);

	/*
	 * We consider each word in input nn one at a time and convert
	 * it to big endian in a temporary word. Based on remaining
	 * length of output buffer, we copy the LSB bytes of temporary
	 * word into it at current position. That way, filling of the
	 * buffer is performed from its end to its beginning, word by
	 * word, except for the last one, which may be shorten if
	 * given buffer length is not a multiple of word length.
	 */
	for (i = 0; remain && (i < in_nn->wlen); i++) {
		u16 copylen = (remain > wb) ? wb : remain;
		word_t val;

		ret = _htonw((const u8 *)&in_nn->val[i], &val); EG(ret, err);

		dst_word_ptr = (buf + buflen - (i * wb) - copylen);
		src_word_ptr = (u8 *)(&val) + wb - copylen;

		ret = local_memcpy(dst_word_ptr, src_word_ptr, copylen); EG(ret, err);
		src_word_ptr = NULL;

		remain = (u16)(remain - copylen);
	}

err:
	return ret;
}

/*
 * Given a table 'tab' pointing to a set of 'tabsize' NN elements, the
 * function copies the value of element at position idx (idx < tabsize)
 * in 'out' parameters. Masking is used to avoid leaking which element
 * was copied.
 *
 * Note that the main copying loop is done on the maximum bits for all
 * NN elements and not based on the specific effective size of each
 * NN elements in 'tab'
 *
 * Returns 0 on success, -1 on error.
 *
 * Aliasing of out and the selected element inside the tab is NOT supported.
 */
int nn_tabselect(nn_t out, u8 idx, nn_src_t *tab, u8 tabsize)
{
	u8 i, k;
	word_t mask;
	int ret;

	/* Basic sanity checks */
	MUST_HAVE(((tab != NULL) && (idx < tabsize)), ret, err);

	ret = nn_check_initialized(out); EG(ret, err);

	/* Zeroize out and enforce its size. */
	ret = nn_zero(out); EG(ret, err);

	out->wlen = 0;

	for (k = 0; k < tabsize; k++) {
		/* Check current element is initialized */
		ret = nn_check_initialized(tab[k]); EG(ret, err);

		mask = WORD_MASK_IFNOTZERO(idx == k);

		out->wlen = (u8)(out->wlen | ((tab[k]->wlen) & mask));

		for (i = 0; i < NN_MAX_WORD_LEN; i++) {
			out->val[i] |= (tab[k]->val[i] & mask);
		}
	}

err:
	return ret;
}
