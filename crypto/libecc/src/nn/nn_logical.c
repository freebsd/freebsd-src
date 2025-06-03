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
#include <libecc/nn/nn_logical.h>
#include <libecc/nn/nn.h>

/*
 * nn_lshift_fixedlen: left logical shift in N, i.e. compute out = (in << cnt).
 *
 * Aliasing is possible for 'in' and 'out', i.e. x <<= cnt can be computed
 * using nn_lshift_fixedlen(x, x, cnt).
 *
 * The function supports 'in' and 'out' parameters of differents sizes.
 *
 * The operation time of the function depends on the size of 'in' and
 * 'out' parameters and the value of 'cnt'. It does not depend on the
 * value of 'in'.
 *
 * It is to be noted that the function uses out->wlen as the
 * upper limit for its work, i.e. bits shifted above out->wlen
 * are lost (the NN size of the output is not modified).
 *
 * The function returns 0 on sucess, -1 on error.
 *
 * Aliasing supported.
 */
int nn_lshift_fixedlen(nn_t out, nn_src_t in, bitcnt_t cnt)
{
	int ipos, opos, dec, ret;
	bitcnt_t lshift, hshift;
	u8 owlen, iwlen;

	ret = nn_check_initialized(in); EG(ret, err);
	ret = nn_check_initialized(out); EG(ret, err);
	owlen = out->wlen;
	iwlen = in->wlen;

	dec = cnt / WORD_BITS;
	hshift = cnt % WORD_BITS;
	lshift = (bitcnt_t)(WORD_BITS - hshift);

	for (opos = owlen - 1; opos >= 0; opos--) {
		word_t hipart = 0, lopart = 0;

		ipos = opos - dec - 1;
		if ((ipos >= 0) && (ipos < iwlen)) {
			lopart = WRSHIFT(in->val[ipos], lshift);
		}

		ipos = opos - dec;
		if ((ipos >= 0) && (ipos < iwlen)) {
			hipart = WLSHIFT(in->val[ipos], hshift);
		}

		out->val[opos] = hipart | lopart;
	}

err:
	return ret;
}

/*
 * nn_lshift: left logical shift in N, i.e. compute out = (in << cnt).
 *
 * Aliasing is possible for 'in' and 'out', i.e. x <<= cnt can be computed
 * using nn_lshift(x, x, cnt).
 *
 * The function supports 'in' and 'out' parameters of differents sizes.
 *
 * The operation time of the function depends on the size of 'in' and
 * 'out' parameters and the value of 'cnt'. It does not depend on the
 * value of 'in'.
 *
 * It is to be noted that the function computes the output bit length
 * depending on the shift count and the input length, i.e. out bit length
 * will be roughly in bit length  plus cnt, maxed to NN_MAX_BIT_LEN.
 *
 * The function returns 0 on success, -1 on error.
 *
 * Aliasing supported.
 */
int nn_lshift(nn_t out, nn_src_t in, bitcnt_t cnt)
{
	bitcnt_t lshift, hshift, blen;
	int ipos, opos, dec, ret;
	u8 owlen, iwlen;

	ret = nn_check_initialized(in); EG(ret, err);
	iwlen = in->wlen;

	/* Initialize output if no aliasing is used */
	if (out != in) {
		ret = nn_init(out, 0); EG(ret, err);
	}

	/* Adapt output length accordingly */
	ret = nn_bitlen(in, &blen); EG(ret, err);
	owlen = (u8)LOCAL_MIN(BIT_LEN_WORDS(cnt + blen),
			BIT_LEN_WORDS(NN_MAX_BIT_LEN));
	out->wlen = owlen;

	dec = cnt / WORD_BITS;
	hshift = cnt % WORD_BITS;
	lshift = (bitcnt_t)(WORD_BITS - hshift);

	for (opos = owlen - 1; opos >= 0; opos--) {
		word_t hipart = 0, lopart = 0;

		ipos = opos - dec - 1;
		if ((ipos >= 0) && (ipos < iwlen)) {
			lopart = WRSHIFT(in->val[ipos], lshift);
		}

		ipos = opos - dec;
		if ((ipos >= 0) && (ipos < iwlen)) {
			hipart = WLSHIFT(in->val[ipos], hshift);
		}

		out->val[opos] = hipart | lopart;
	}

err:
	return ret;
}

/*
 * nn_rshift_fixedlen: right logical shift in N, i.e. compute out = (in >> cnt).
 *
 * Aliasing is possible for 'in' and 'out', i.e. x >>= cnt can be computed
 * using nn_rshift_fixedlen(x, x, cnt).
 *
 * The function supports 'in' and 'out' parameters of differents sizes.
 *
 * The operation time of the function depends on the size of 'in' and
 * 'out' parameters and the value of 'cnt'. It does not depend on the
 * value of 'in'.
 * It is to be noted that the function uses out->wlen as the
 * upper limit for its work, which means zeroes are shifted in while
 * keeping the same NN output size.
 *
 * The function returns 0 on success, -1 on error.
 *
 * Aliasing supported.
 */
int nn_rshift_fixedlen(nn_t out, nn_src_t in, bitcnt_t cnt)
{
	int ipos, opos, dec, ret;
	bitcnt_t lshift, hshift;
	u8 owlen, iwlen;

	ret = nn_check_initialized(in); EG(ret, err);
	ret = nn_check_initialized(out); EG(ret, err);
	owlen = out->wlen;
	iwlen = in->wlen;

	dec = cnt / WORD_BITS;
	lshift = cnt % WORD_BITS;
	hshift = (bitcnt_t)(WORD_BITS - lshift);

	for (opos = 0; opos < owlen; opos++) {
		word_t hipart = 0, lopart = 0;

		ipos = opos + dec;
		if ((ipos >= 0) && (ipos < iwlen)) {
			lopart = WRSHIFT(in->val[ipos], lshift);
		}

		ipos = opos + dec + 1;
		if ((ipos >= 0) && (ipos < iwlen)) {
			hipart = WLSHIFT(in->val[ipos], hshift);
		}

		out->val[opos] = hipart | lopart;
	}

err:
	return ret;
}

/*
 * nn_rshift: right logical shift in N, i.e. compute out = (in >> cnt).
 *
 * Aliasing is possible for 'in' and 'out', i.e. x >>= cnt can be computed
 * using nn_rshift_fixedlen(x, x, cnt).
 *
 * The function supports 'in' and 'out' parameters of differents sizes.
 *
 * The operation time of the function depends on the size of 'in' and
 * 'out' parameters and the value of 'cnt'. It does not depend on the
 * value of 'in'.
 * It is to be noted that the function adapts the output size to
 * the input size and the shift bit count, i.e. out bit lenth is roughly
 * equal to input bit length minus cnt.
 *
 * The function returns 0 on success, -1 on error.
 *
 * Aliasing supported.
 */
int nn_rshift(nn_t out, nn_src_t in, bitcnt_t cnt)
{
	int ipos, opos, dec, ret;
	bitcnt_t lshift, hshift;
	u8 owlen, iwlen;
	bitcnt_t blen;

	ret = nn_check_initialized(in); EG(ret, err);
	iwlen = in->wlen;
	/* Initialize output if no aliasing is used */
	if (out != in) {
		ret = nn_init(out, 0); EG(ret, err);
	}

	dec = cnt / WORD_BITS;
	lshift = cnt % WORD_BITS;
	hshift = (bitcnt_t)(WORD_BITS - lshift);

	/* Adapt output length accordingly */
	ret = nn_bitlen(in, &blen); EG(ret, err);
	if (cnt > blen) {
		owlen = 0;
	} else {
		owlen = (u8)BIT_LEN_WORDS(blen - cnt);
	}
	/* Adapt output length in out */
	out->wlen = owlen;

	for (opos = 0; opos < owlen; opos++) {
		word_t hipart = 0, lopart = 0;

		ipos = opos + dec;
		if ((ipos >= 0) && (ipos < iwlen)) {
			lopart = WRSHIFT(in->val[ipos], lshift);
		}

		ipos = opos + dec + 1;
		if ((ipos >= 0) && (ipos < iwlen)) {
			hipart = WLSHIFT(in->val[ipos], hshift);
		}

		out->val[opos] = hipart | lopart;
	}

	/*
	 * Zero the output upper part now that we don't need it anymore
	 * NB: as we cannot use our normalize helper here (since a consistency
	 * check is done on wlen and upper part), we have to do this manually
	 */
	for (opos = owlen; opos < NN_MAX_WORD_LEN; opos++) {
		out->val[opos] = 0;
	}

err:
	return ret;
}

/*
 * This function right rotates the input NN value by the value 'cnt' on the
 * bitlen basis. The function does it in the following way; right rotation
 * of x by cnt is "simply": (x >> cnt) ^ (x << (bitlen - cnt))
 *
 * The function returns 0 on success, -1 on error.
 *
 * Aliasing supported.
 */
int nn_rrot(nn_t out, nn_src_t in, bitcnt_t cnt, bitcnt_t bitlen)
{
	u8 owlen = (u8)BIT_LEN_WORDS(bitlen);
	int ret;
	nn tmp;
	tmp.magic = WORD(0);

	MUST_HAVE((bitlen <= NN_MAX_BIT_LEN), ret, err);
	MUST_HAVE((cnt < bitlen), ret, err);

	ret = nn_check_initialized(in); EG(ret, err);
	ret = nn_init(&tmp, 0); EG(ret, err);
	ret = nn_lshift(&tmp, in, (bitcnt_t)(bitlen - cnt)); EG(ret, err);
	ret = nn_set_wlen(&tmp, owlen); EG(ret, err);
	ret = nn_rshift(out, in, cnt); EG(ret, err);
	ret = nn_set_wlen(out, owlen); EG(ret, err);
	ret = nn_xor(out, out, &tmp); EG(ret, err);

	/* Mask the last word if necessary */
	if (((bitlen % WORD_BITS) != 0) && (out->wlen > 0)) {
		/* shift operation below is ok (less than WORD_BITS) */
		word_t mask = (word_t)(((word_t)(WORD(1) << (bitlen % WORD_BITS))) - 1);
		out->val[out->wlen - 1] &= mask;
	}

err:
	nn_uninit(&tmp);

	return ret;
}

/*
 * This function left rotates the input NN value by the value 'cnt' on the
 * bitlen basis. The function does it in the following way; Left rotation
 * of x by cnt is "simply": (x << cnt) ^ (x >> (bitlen - cnt))
 *
 * The function returns 0 on success, -1 on error.
 *
 * Aliasing supported.
 */
int nn_lrot(nn_t out, nn_src_t in, bitcnt_t cnt, bitcnt_t bitlen)
{
	u8 owlen = (u8)BIT_LEN_WORDS(bitlen);
	int ret;
	nn tmp;
	tmp.magic = WORD(0);

	MUST_HAVE(!(bitlen > NN_MAX_BIT_LEN), ret, err);
	MUST_HAVE(!(cnt >= bitlen), ret, err);

	ret = nn_check_initialized(in); EG(ret, err);
	ret = nn_init(&tmp, 0); EG(ret, err);
	ret = nn_lshift(&tmp, in, cnt); EG(ret, err);
	ret = nn_set_wlen(&tmp, owlen); EG(ret, err);
	ret = nn_rshift(out, in, (bitcnt_t)(bitlen - cnt)); EG(ret, err);
	ret = nn_set_wlen(out, owlen); EG(ret, err);
	ret = nn_xor(out, out, &tmp); EG(ret, err);

	/* Mask the last word if necessary */
	if (((bitlen % WORD_BITS) != 0) && (out->wlen > 0)) {
		word_t mask = (word_t)(((word_t)(WORD(1) << (bitlen % WORD_BITS))) - 1);
		out->val[out->wlen - 1] &= mask;
	}

err:
	nn_uninit(&tmp);

	return ret;
}

/*
 * Compute XOR between B and C and put the result in A. B and C must be
 * initialized. Aliasing is supported, i.e. A can be one of the parameter B or
 * C. If aliasing is not used, A will be initialized by the function. Function
 * execution time depends on the word length of larger parameter but not on its
 * particular value.
 *
 * The function returns 0 on success, -1 on error.
 *
 * Aliasing supported.
 */
int nn_xor(nn_t A, nn_src_t B, nn_src_t C)
{
	int ret;
	u8 i;

	ret = nn_check_initialized(B); EG(ret, err);
	ret = nn_check_initialized(C); EG(ret, err);

	/* Initialize the output if no aliasing is used */
	if ((A != B) && (A != C)) {
		ret = nn_init(A, 0);  EG(ret, err);
	}

	/* Set output wlen accordingly */
	A->wlen = (C->wlen < B->wlen) ? B->wlen : C->wlen;

	for (i = 0; i < A->wlen; i++) {
		A->val[i] = (B->val[i] ^ C->val[i]);
	}

err:
	return ret;
}

/*
 * Compute logical OR between B and C and put the result in A. B and C must be
 * initialized. Aliasing is supported, i.e. A can be one of the parameter B or
 * C. If aliasing is not used, A will be initialized by the function. Function
 * execution time depends on the word length of larger parameter but not on its
 * particular value.
 *
 * The function returns 0 on success, -1 on error.
 *
 * Aliasing supported.
 */
int nn_or(nn_t A, nn_src_t B, nn_src_t C)
{
	int ret;
	u8 i;

	ret = nn_check_initialized(B); EG(ret, err);
	ret = nn_check_initialized(C); EG(ret, err);

	/* Initialize the output if no aliasing is used */
	if ((A != B) && (A != C)) {
		ret = nn_init(A, 0); EG(ret, err);
	}

	/* Set output wlen accordingly */
	A->wlen = (C->wlen < B->wlen) ? B->wlen : C->wlen;

	for (i = 0; i < A->wlen; i++) {
		A->val[i] = (B->val[i] | C->val[i]);
	}

err:
	return ret;
}

/*
 * Compute logical AND between B and C and put the result in A. B and C must be
 * initialized. Aliasing is supported, i.e. A can be one of the parameter B or
 * C. If aliasing is not used, A will be initialized by the function. Function
 * execution time depends on the word length of larger parameter but not on its
 * particular value.
 *
 * The function returns 0 on success, -1 on error.
 *
 * Aliasing supported.
 */
int nn_and(nn_t A, nn_src_t B, nn_src_t C)
{
	int ret;
	u8 i;

	ret = nn_check_initialized(B); EG(ret, err);
	ret = nn_check_initialized(C); EG(ret, err);

	/* Initialize the output if no aliasing is used */
	if ((A != B) && (A != C)) {
		ret = nn_init(A, 0); EG(ret, err);
	}

	/* Set output wlen accordingly */
	A->wlen = (C->wlen < B->wlen) ? B->wlen : C->wlen;

	for (i = 0; i < A->wlen; i++) {
		A->val[i] = (B->val[i] & C->val[i]);
	}

err:
	return ret;
}

/*
 * Compute logical NOT of B and put the result in A. B must be initialized.
 * Aliasing is supported. If aliasing is not used, A will be initialized by
 * the function.
 *
 * The function returns 0 on success, -1 on error.
 *
 * Aliasing supported.
 */
int nn_not(nn_t A, nn_src_t B)
{
	int ret;
	u8 i;

	ret = nn_check_initialized(B); EG(ret, err);

	/* Initialize the output if no aliasing is used */
	if (A != B) {
		ret = nn_init(A, 0); EG(ret, err);
	}

	/* Set output wlen accordingly */
	A->wlen = B->wlen;

	for (i = 0; i < A->wlen; i++) {
		A->val[i] = (word_t)(~(B->val[i]));
	}

err:
	return ret;
}

/* Count leading zeros of a word. This is constant time */
ATTRIBUTE_WARN_UNUSED_RET static u8 wclz(word_t A)
{
	u8 cnt = WORD_BITS, over = 0;
	int i;

	for (i = (WORD_BITS - 1); i >= 0; i--) {
		/* i is less than WORD_BITS so shift operations below are ok */
		u8 mask = (u8)(((A & (WORD(1) << i)) >> i) & 0x1);
		over |= mask;
		cnt = (u8)(cnt - over);
	}

	return cnt;
}

/*
 * Count leading zeros of an initialized nn. This is NOT constant time. The
 * function returns 0 on success, -1 on error. On success, the number of
 * leading zeroes is available in 'lz'. 'lz' is not meaningful on error.
 */
int nn_clz(nn_src_t in, bitcnt_t *lz)
{
	bitcnt_t cnt = 0;
	int ret;
	u8 i;

	/* Sanity check */
	MUST_HAVE((lz != NULL), ret, err);
	ret = nn_check_initialized(in); EG(ret, err);

	for (i = in->wlen; i > 0; i--) {
		if (in->val[i - 1] == 0) {
			cnt = (bitcnt_t)(cnt +  WORD_BITS);
		} else {
			cnt = (bitcnt_t)(cnt + wclz(in->val[i - 1]));
			break;
		}
	}
	*lz = cnt;

err:
	return ret;
}

/*
 * Compute bit length of given nn. This is NOT constant-time.  The
 * function returns 0 on success, -1 on error. On success, the bit length
 * of 'in' is available in 'blen'. 'blen' is not meaningful on error.
 */
int nn_bitlen(nn_src_t in, bitcnt_t *blen)
{
	bitcnt_t _blen = 0;
	int ret;
	u8 i;

	/* Sanity check */
	MUST_HAVE((blen != NULL), ret, err);
	ret = nn_check_initialized(in); EG(ret, err);

	for (i = in->wlen; i > 0; i--) {
		if (in->val[i - 1] != 0) {
			_blen = (bitcnt_t)((i * WORD_BITS) - wclz(in->val[i - 1]));
			break;
		}
	}
	(*blen) = _blen;

err:
	return ret;
}

/*
 * On success (return value is 0), the function provides via 'bitval' the value
 * of the bit at position 'bit' in 'in' nn. 'bitval' in not meaningful error
 * (when return value is -1).
 */
int nn_getbit(nn_src_t in, bitcnt_t bit, u8 *bitval)
{
	bitcnt_t widx = bit / WORD_BITS;
	u8 bidx = bit % WORD_BITS;
	int ret;

	/* Sanity check */
	MUST_HAVE((bitval != NULL), ret, err);
	ret = nn_check_initialized(in); EG(ret, err);
	MUST_HAVE((bit < NN_MAX_BIT_LEN), ret, err);

	/* bidx is less than WORD_BITS so shift operations below are ok */
	(*bitval) = (u8)((((in->val[widx]) & (WORD(1) << bidx)) >> bidx) & 0x1);

err:
	return ret;
}
