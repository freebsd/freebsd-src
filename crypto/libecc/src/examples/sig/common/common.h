/* Some common helpers useful for many algorithms */
#ifndef __COMMON_H__
#define __COMMON_H__

/* Include our arithmetic layer */
#include <libecc/libarith.h>

/* I2OSP and OS2IP internal primitives */
ATTRIBUTE_WARN_UNUSED_RET static inline int _i2osp(nn_src_t x, u8 *buf, u16 buflen)
{
	int ret;
	bitcnt_t blen;

	/* Sanity checks */
	MUST_HAVE((buf != NULL), ret, err);
	ret = nn_check_initialized(x); EG(ret, err);

	/* If x >= 256^xLen (the integer does not fit in the buffer),
	 * return an error.
	 */
	ret = nn_bitlen(x, &blen); EG(ret, err);
	MUST_HAVE(((8 * buflen) >= blen), ret, err);

	/* Export to the buffer */
	ret = nn_export_to_buf(buf, buflen, x);

err:
	return ret;
}

ATTRIBUTE_WARN_UNUSED_RET static inline int _os2ip(nn_t x, const u8 *buf, u16 buflen)
{
	int ret;

	/* We do not want to exceed our computation compatible
	 * size.
	 */
	MUST_HAVE((buflen <= NN_USABLE_MAX_BYTE_LEN), ret, err);

	/* Import the NN */
	ret = nn_init_from_buf(x, buf, buflen);

err:
	return ret;
}

/* Reverses the endiannes of a buffer in place */
ATTRIBUTE_WARN_UNUSED_RET static inline int _reverse_endianness(u8 *buf, u16 buf_size)
{
	u16 i;
	u8 tmp;
	int ret;

	MUST_HAVE((buf != NULL), ret, err);

	if(buf_size > 1){
		for(i = 0; i < (buf_size / 2); i++){
			tmp = buf[i];
			buf[i] = buf[buf_size - 1 - i];
			buf[buf_size - 1 - i] = tmp;
		}
	}

	ret = 0;

err:
        return ret;
}

/* Helper to fix the MSB of a scalar using the trick in
 * https://eprint.iacr.org/2011/232.pdf
 *
 *  We distinguish three situations:
 *     - The scalar m is < q (the order), in this case we compute:
 *         -
 *        | m' = m + (2 * q) if [log(k + q)] == [log(q)],
 *        | m' = m + q otherwise.
 *         -
 *     - The scalar m is >= q and < q**2, in this case we compute:
 *         -
 *        | m' = m + (2 * (q**2)) if [log(k + (q**2))] == [log(q**2)],
 *        | m' = m + (q**2) otherwise.
 *         -
 *     - The scalar m is >= (q**2), in this case m == m'
 *  We only deal with 0 <= m < (q**2) using the countermeasure. When m >= (q**2),
 *  we stick with m' = m, accepting MSB issues (not much can be done in this case
 *  anyways).
 */
ATTRIBUTE_WARN_UNUSED_RET static inline int _fix_scalar_msb(nn_src_t m, nn_src_t q, nn_t m_msb_fixed)
{
	int ret, cmp;
	/* _m_msb_fixed to handle aliasing */
	nn q_square, _m_msb_fixed;
	q_square.magic = _m_msb_fixed.magic = WORD(0);

	/* Sanity checks */
	ret = nn_check_initialized(m); EG(ret, err);
	ret = nn_check_initialized(q); EG(ret, err);
	ret = nn_check_initialized(m_msb_fixed); EG(ret, err);

	ret = nn_init(&q_square, 0); EG(ret, err);
	ret = nn_init(&_m_msb_fixed, 0); EG(ret, err);

	/* First compute q**2 */
	ret = nn_sqr(&q_square, q); EG(ret, err);
	/* Then compute m' depending on m size */
	ret = nn_cmp(m, q, &cmp); EG(ret, err);
	if (cmp < 0){
		bitcnt_t msb_bit_len, q_bitlen;

		/* Case where m < q */
		ret = nn_add(&_m_msb_fixed, m, q); EG(ret, err);
		ret = nn_bitlen(&_m_msb_fixed, &msb_bit_len); EG(ret, err);
		ret = nn_bitlen(q, &q_bitlen); EG(ret, err);
		ret = nn_cnd_add((msb_bit_len == q_bitlen), m_msb_fixed,
				  &_m_msb_fixed, q); EG(ret, err);
	} else {
		ret = nn_cmp(m, &q_square, &cmp); EG(ret, err);
		if (cmp < 0) {
			bitcnt_t msb_bit_len, q_square_bitlen;

			/* Case where m >= q and m < (q**2) */
			ret = nn_add(&_m_msb_fixed, m, &q_square); EG(ret, err);
			ret = nn_bitlen(&_m_msb_fixed, &msb_bit_len); EG(ret, err);
			ret = nn_bitlen(&q_square, &q_square_bitlen); EG(ret, err);
			ret = nn_cnd_add((msb_bit_len == q_square_bitlen),
					m_msb_fixed, &_m_msb_fixed, &q_square); EG(ret, err);
		} else {
			/* Case where m >= (q**2) */
			ret = nn_copy(m_msb_fixed, m); EG(ret, err);
		}
	}

err:
	nn_uninit(&q_square);
	nn_uninit(&_m_msb_fixed);

	return ret;
}

/* Helper to blind the scalar.
 * Compute m_blind = m + (b * q) where b is a random value modulo q.
 * Aliasing is supported.
 */
ATTRIBUTE_WARN_UNUSED_RET static inline int _blind_scalar(nn_src_t m, nn_src_t q, nn_t m_blind)
{
        int ret;
	nn tmp;
	tmp.magic = WORD(0);

	/* Sanity checks */
        ret = nn_check_initialized(m); EG(ret, err);
        ret = nn_check_initialized(q); EG(ret, err);
        ret = nn_check_initialized(m_blind); EG(ret, err);

	ret = nn_get_random_mod(&tmp, q); EG(ret, err);

	ret = nn_mul(&tmp, &tmp, q); EG(ret, err);
	ret = nn_add(m_blind, &tmp, m);

err:
	nn_uninit(&tmp);

	return ret;
}

/*
 * NOT constant time at all and not secure against side-channels. This is
 * an internal function only used for DSA verification on public data.
 *
 * Compute (base ** exp) mod (mod) using a square and multiply algorithm.
 * Internally, this computes Montgomery coefficients and uses the redc
 * function.
 *
 * Returns 0 on success, -1 on error.
 */
ATTRIBUTE_WARN_UNUSED_RET static inline int _nn_mod_pow_insecure(nn_t out, nn_src_t base,
							  nn_src_t exp, nn_src_t mod)
{
	int ret, isodd, cmp;
	bitcnt_t explen;
	u8 expbit;
	nn r, r_square, _base, one;
	word_t mpinv;
	r.magic = r_square.magic = _base.magic = one.magic = WORD(0);

	/* Aliasing is not supported for this internal helper */
	MUST_HAVE((out != base) && (out != exp) && (out != mod), ret, err);

	/* Check initializations */
	ret = nn_check_initialized(base); EG(ret, err);
	ret = nn_check_initialized(exp); EG(ret, err);
	ret = nn_check_initialized(mod); EG(ret, err);

	ret = nn_bitlen(exp, &explen); EG(ret, err);
	/* Sanity check */
	MUST_HAVE((explen > 0), ret, err);

	/* Check that the modulo is indeed odd */
	ret = nn_isodd(mod, &isodd); EG(ret, err);
	MUST_HAVE(isodd, ret, err);

	/* Compute the Montgomery coefficients */
	ret = nn_compute_redc1_coefs(&r, &r_square, mod, &mpinv); EG(ret, err);

	/* Reduce the base if necessary */
	ret = nn_cmp(base, mod, &cmp); EG(ret, err);
	if(cmp >= 0){
		ret = nn_mod(&_base, base, mod); EG(ret, err);
	}
	else{
		ret = nn_copy(&_base, base); EG(ret, err);
	}

	ret = nn_mul_redc1(&_base, &_base, &r_square, mod, mpinv); EG(ret, err);
	ret = nn_copy(out, &r); EG(ret, err);

	ret = nn_init(&one, 0); EG(ret, err);
	ret = nn_one(&one); EG(ret, err);

	while (explen > 0) {
		explen = (bitcnt_t)(explen - 1);

		/* Get the bit */
		ret = nn_getbit(exp, explen, &expbit); EG(ret, err);

		/* Square */
		ret = nn_mul_redc1(out, out, out, mod, mpinv); EG(ret, err);

		if(expbit){
			/* Multiply */
			ret = nn_mul_redc1(out, out, &_base, mod, mpinv); EG(ret, err);
		}
	}
	/* Unredcify the output */
	ret = nn_mul_redc1(out, out, &one, mod, mpinv);

err:
	nn_uninit(&r);
	nn_uninit(&r_square);
	nn_uninit(&_base);
	nn_uninit(&one);

	return ret;
}


#endif /* __COMMON_H__ */
