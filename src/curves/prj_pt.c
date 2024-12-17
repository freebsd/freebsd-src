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
#include <libecc/curves/ec_shortw.h>
#include <libecc/curves/prj_pt.h>
#include <libecc/nn/nn_logical.h>
#include <libecc/nn/nn_add.h>
#include <libecc/nn/nn_rand.h>
#include <libecc/fp/fp_add.h>
#include <libecc/fp/fp_mul.h>
#include <libecc/fp/fp_montgomery.h>
#include <libecc/fp/fp_rand.h>

#define PRJ_PT_MAGIC ((word_t)(0xe1cd70babb1d5afeULL))

/*
 * Check given projective point has been correctly initialized (using
 * prj_pt_init()). Returns 0 on success, -1 on error.
 */
int prj_pt_check_initialized(prj_pt_src_t in)
{
	int ret;

	MUST_HAVE(((in != NULL) && (in->magic == PRJ_PT_MAGIC)), ret, err);
	ret = ec_shortw_crv_check_initialized(in->crv);

err:
	return ret;
}

/*
 * Initialize the projective point structure on given curve as the point at
 * infinity. The function returns 0 on success, -1 on error.
 */
int prj_pt_init(prj_pt_t in, ec_shortw_crv_src_t curve)
{
	int ret;

	ret = ec_shortw_crv_check_initialized(curve); EG(ret, err);

	MUST_HAVE((in != NULL), ret, err);

	ret = fp_init(&(in->X), curve->a.ctx); EG(ret, err);
	ret = fp_init(&(in->Y), curve->a.ctx); EG(ret, err);
	ret = fp_init(&(in->Z), curve->a.ctx); EG(ret, err);
	in->crv = curve;
	in->magic = PRJ_PT_MAGIC;

err:
	return ret;
}

/*
 * Initialize the projective point structure on given curve using given
 * coordinates. The function returns 0 on success, -1 on error.
 */
int prj_pt_init_from_coords(prj_pt_t in,
			     ec_shortw_crv_src_t curve,
			     fp_src_t xcoord, fp_src_t ycoord, fp_src_t zcoord)
{
	int ret;

	ret = prj_pt_init(in, curve); EG(ret, err);
	ret = fp_copy(&(in->X), xcoord); EG(ret, err);
	ret = fp_copy(&(in->Y), ycoord); EG(ret, err);
	ret = fp_copy(&(in->Z), zcoord);

err:
	return ret;
}

/*
 * Uninit given projective point structure. The function returns 0 on success,
 * -1 on error. This is an error if passed point has not already been
 * initialized first.
 */
void prj_pt_uninit(prj_pt_t in)
{
	if((in != NULL) && (in->magic == PRJ_PT_MAGIC) && (in->crv != NULL)){
		in->crv = NULL;
		in->magic = WORD(0);

		fp_uninit(&(in->X));
		fp_uninit(&(in->Y));
		fp_uninit(&(in->Z));
	}

	return;
}

/*
 * Checks if projective point is the point at infinity (last coordinate is 0).
 * In that case, 'iszero' out parameter is set to 1. It is set to 0 if the
 * point is not the point at infinity. The function returns 0 on success, -1 on
 * error. On error, 'iszero' is not meaningful.
 */
int prj_pt_iszero(prj_pt_src_t in, int *iszero)
{
	int ret;

	ret = prj_pt_check_initialized(in); EG(ret, err);
	ret = fp_iszero(&(in->Z), iszero);

err:
	return ret;
}

/*
 * Set given projective point 'out' to the point at infinity. The functions
 * returns 0 on success, -1 on error.
 */
int prj_pt_zero(prj_pt_t out)
{
	int ret;

	ret = prj_pt_check_initialized(out); EG(ret, err);

	ret = fp_zero(&(out->X)); EG(ret, err);
	ret = fp_one(&(out->Y)); EG(ret, err);
	ret = fp_zero(&(out->Z));

err:
	return ret;
}

/*
 * Check if a projective point is indeed on its curve. The function sets
 * 'on_curve' out parameter to 1 if the point is on the curve, 0 if not.
 * The function returns 0 on success, -1 on error. 'on_curve' is not
 * meaningful on error.
 */
int prj_pt_is_on_curve(prj_pt_src_t in,  int *on_curve)
{
	int ret, cmp;

	/* In order to check that we are on the curve, we
	 * use the projective formula of the curve:
	 *
	 *   Y**2 * Z = X**3 + a * X * Z**2 + b * Z**3
	 *
	 */
	fp X, Y, Z;
	X.magic = Y.magic = Z.magic = WORD(0);

	ret = prj_pt_check_initialized(in); EG(ret, err);
	ret = ec_shortw_crv_check_initialized(in->crv); EG(ret, err);
	MUST_HAVE((on_curve != NULL), ret, err);

	ret = fp_init(&X, in->X.ctx); EG(ret, err);
	ret = fp_init(&Y, in->X.ctx); EG(ret, err);
	ret = fp_init(&Z, in->X.ctx); EG(ret, err);

	/* Compute X**3 + a * X * Z**2 + b * Z**3 on one side */
	ret = fp_sqr(&X, &(in->X)); EG(ret, err);
	ret = fp_mul(&X, &X, &(in->X)); EG(ret, err);
	ret = fp_mul(&Z, &(in->X), &(in->crv->a)); EG(ret, err);
	ret = fp_mul(&Y, &(in->crv->b), &(in->Z)); EG(ret, err);
	ret = fp_add(&Z, &Z, &Y); EG(ret, err);
	ret = fp_mul(&Z, &Z, &(in->Z)); EG(ret, err);
	ret = fp_mul(&Z, &Z, &(in->Z)); EG(ret, err);
	ret = fp_add(&X, &X, &Z); EG(ret, err);

	/* Compute Y**2 * Z on the other side */
	ret = fp_sqr(&Y, &(in->Y)); EG(ret, err);
	ret = fp_mul(&Y, &Y, &(in->Z)); EG(ret, err);

	/* Compare the two values */
	ret = fp_cmp(&X, &Y, &cmp); EG(ret, err);

	(*on_curve) = (!cmp);

err:
	fp_uninit(&X);
	fp_uninit(&Y);
	fp_uninit(&Z);

	return ret;
}

/*
 * The function copies 'in' projective point to 'out'. 'out' is initialized by
 * the function. The function returns 0 on sucess, -1 on error.
 */
int prj_pt_copy(prj_pt_t out, prj_pt_src_t in)
{
	int ret;

	ret = prj_pt_check_initialized(in); EG(ret, err);

	ret = prj_pt_init(out, in->crv); EG(ret, err);

	ret = fp_copy(&(out->X), &(in->X)); EG(ret, err);
	ret = fp_copy(&(out->Y), &(in->Y)); EG(ret, err);
	ret = fp_copy(&(out->Z), &(in->Z));

err:
	return ret;
}

/*
 * Convert given projective point 'in' to affine representation in 'out'. 'out'
 * is initialized by the function. The function returns 0 on success, -1 on
 * error. Passing the point at infinty to the function is considered as an
 * error.
 */
int prj_pt_to_aff(aff_pt_t out, prj_pt_src_t in)
{
	int ret, iszero;

	ret = prj_pt_check_initialized(in); EG(ret, err);

	ret = prj_pt_iszero(in, &iszero); EG(ret, err);
	MUST_HAVE((!iszero), ret, err);

	ret = aff_pt_init(out, in->crv); EG(ret, err);

	ret = fp_inv(&(out->x), &(in->Z)); EG(ret, err);
	ret = fp_mul(&(out->y), &(in->Y), &(out->x)); EG(ret, err);
	ret = fp_mul(&(out->x), &(in->X), &(out->x));

err:
	return ret;
}

/*
 * Get the unique Z = 1 projective point representation ("equivalent" to affine
 * point). The function returns 0 on success, -1 on error.
 */
int prj_pt_unique(prj_pt_t out, prj_pt_src_t in)
{
	int ret, iszero;

	ret = prj_pt_check_initialized(in); EG(ret, err);
	ret = prj_pt_iszero(in, &iszero); EG(ret, err);
	MUST_HAVE((!iszero), ret, err);

	if(out == in){
		/* Aliasing case */
		fp tmp;
		tmp.magic = WORD(0);

		ret = fp_init(&tmp, (in->Z).ctx); EG(ret, err);
		ret = fp_inv(&tmp, &(in->Z)); EG(ret, err1);
		ret = fp_mul(&(out->Y), &(in->Y), &tmp); EG(ret, err1);
		ret = fp_mul(&(out->X), &(in->X), &tmp); EG(ret, err1);
		ret = fp_one(&(out->Z)); EG(ret, err1);
err1:
		fp_uninit(&tmp); EG(ret, err);
	}
	else{
	        ret = prj_pt_init(out, in->crv); EG(ret, err);
		ret = fp_inv(&(out->X), &(in->Z)); EG(ret, err);
		ret = fp_mul(&(out->Y), &(in->Y), &(out->X)); EG(ret, err);
		ret = fp_mul(&(out->X), &(in->X), &(out->X)); EG(ret, err);
		ret = fp_one(&(out->Z)); EG(ret, err);
	}


err:
	return ret;
}

/*
 * Converts affine point 'in' to projective representation in 'out'. 'out' is
 * initialized by the function. The function returns 0 on success, -1 on error.
 */
int ec_shortw_aff_to_prj(prj_pt_t out, aff_pt_src_t in)
{
	int ret, on_curve;

	ret = aff_pt_check_initialized(in); EG(ret, err);

	/* The input affine point must be on the curve */
	ret = aff_pt_is_on_curve(in, &on_curve); EG(ret, err);
	MUST_HAVE(on_curve, ret, err);

	ret = prj_pt_init(out, in->crv); EG(ret, err);
	ret = fp_copy(&(out->X), &(in->x)); EG(ret, err);
	ret = fp_copy(&(out->Y), &(in->y)); EG(ret, err);
	ret = nn_one(&(out->Z).fp_val); /* Z = 1 */

err:
	return ret;
}

/*
 * Compare projective points 'in1' and 'in2'. On success, 'cmp' is set to
 * the result of the comparison (0 if in1 == in2, !0 if in1 != in2). The
 * function returns 0 on success, -1 on error.
 */
int prj_pt_cmp(prj_pt_src_t in1, prj_pt_src_t in2, int *cmp)
{
	fp X1, X2, Y1, Y2;
	int ret, x_cmp, y_cmp;
	X1.magic = X2.magic = Y1.magic = Y2.magic = WORD(0);

	MUST_HAVE((cmp != NULL), ret, err);
	ret = prj_pt_check_initialized(in1); EG(ret, err);
	ret = prj_pt_check_initialized(in2); EG(ret, err);

	MUST_HAVE((in1->crv == in2->crv), ret, err);

	ret = fp_init(&X1, (in1->X).ctx); EG(ret, err);
	ret = fp_init(&X2, (in2->X).ctx); EG(ret, err);
	ret = fp_init(&Y1, (in1->Y).ctx); EG(ret, err);
	ret = fp_init(&Y2, (in2->Y).ctx); EG(ret, err);

	/*
	 * Montgomery multiplication is used as it is faster than
	 * usual multiplication and the spurious multiplicative
	 * factor does not matter.
	 */
	ret = fp_mul_monty(&X1, &(in1->X), &(in2->Z)); EG(ret, err);
	ret = fp_mul_monty(&X2, &(in2->X), &(in1->Z)); EG(ret, err);
	ret = fp_mul_monty(&Y1, &(in1->Y), &(in2->Z)); EG(ret, err);
	ret = fp_mul_monty(&Y2, &(in2->Y), &(in1->Z)); EG(ret, err);

	ret = fp_mul_monty(&X1, &(in1->X), &(in2->Z)); EG(ret, err);
	ret = fp_mul_monty(&X2, &(in2->X), &(in1->Z)); EG(ret, err);
	ret = fp_mul_monty(&Y1, &(in1->Y), &(in2->Z)); EG(ret, err);
	ret = fp_mul_monty(&Y2, &(in2->Y), &(in1->Z)); EG(ret, err);
	ret = fp_cmp(&X1, &X2, &x_cmp); EG(ret, err);
	ret = fp_cmp(&Y1, &Y2, &y_cmp);

	if (!ret) {
		(*cmp) = (x_cmp | y_cmp);
	}

err:
	fp_uninit(&Y2);
	fp_uninit(&Y1);
	fp_uninit(&X2);
	fp_uninit(&X1);

	return ret;
}

/*
 * NOTE: this internal functions assumes that upper layer have checked that in1 and in2
 * are initialized, and that cmp is not NULL.
 */
ATTRIBUTE_WARN_UNUSED_RET static inline int _prj_pt_eq_or_opp_X(prj_pt_src_t in1, prj_pt_src_t in2, int *cmp)
{
	int ret;
	fp X1, X2;
	X1.magic = X2.magic = WORD(0);

	/*
	 * Montgomery multiplication is used as it is faster than
	 * usual multiplication and the spurious multiplicative
	 * factor does not matter.
	 */
	ret = fp_init(&X1, (in1->X).ctx); EG(ret, err);
	ret = fp_init(&X2, (in2->X).ctx); EG(ret, err);
	ret = fp_mul_monty(&X1, &(in1->X), &(in2->Z)); EG(ret, err);
	ret = fp_mul_monty(&X2, &(in2->X), &(in1->Z)); EG(ret, err);
	ret = fp_cmp(&X1, &X2, cmp);

err:
	fp_uninit(&X1);
	fp_uninit(&X2);

	return ret;
}

/*
 * NOTE: this internal functions assumes that upper layer have checked that in1 and in2
 * are initialized, and that eq_or_opp is not NULL.
 */
ATTRIBUTE_WARN_UNUSED_RET static inline int _prj_pt_eq_or_opp_Y(prj_pt_src_t in1, prj_pt_src_t in2, int *eq_or_opp)
{
	int ret;
	fp Y1, Y2;
	Y1.magic = Y2.magic = WORD(0);

	/*
	 * Montgomery multiplication is used as it is faster than
	 * usual multiplication and the spurious multiplicative
	 * factor does not matter.
	 */
	ret = fp_init(&Y1, (in1->Y).ctx); EG(ret, err);
	ret = fp_init(&Y2, (in2->Y).ctx); EG(ret, err);
	ret = fp_mul_monty(&Y1, &(in1->Y), &(in2->Z)); EG(ret, err);
	ret = fp_mul_monty(&Y2, &(in2->Y), &(in1->Z)); EG(ret, err);
	ret = fp_eq_or_opp(&Y1, &Y2, eq_or_opp);

err:
	fp_uninit(&Y1);
	fp_uninit(&Y2);

	return ret;
}

 /*
 * The functions tests if given projective points 'in1' and 'in2' are equal or
 * opposite. On success, the result of the comparison is given via 'eq_or_opp'
 * out parameter (1 if equal or opposite, 0 otherwise). The function returns
 * 0 on succes, -1 on error.
 */
int prj_pt_eq_or_opp(prj_pt_src_t in1, prj_pt_src_t in2, int *eq_or_opp)
{
	int ret, cmp, _eq_or_opp;

	ret = prj_pt_check_initialized(in1); EG(ret, err);
	ret = prj_pt_check_initialized(in2); EG(ret, err);
	MUST_HAVE((in1->crv == in2->crv), ret, err);
	MUST_HAVE((eq_or_opp != NULL), ret, err);

	ret = _prj_pt_eq_or_opp_X(in1, in2, &cmp); EG(ret, err);
	ret = _prj_pt_eq_or_opp_Y(in1, in2, &_eq_or_opp);

	if (!ret) {
		(*eq_or_opp) = ((cmp == 0) & _eq_or_opp);
	}

err:
	return ret;
}

/* Compute the opposite of a projective point. Supports aliasing.
 * Returns 0 on success, -1 on failure.
 */
int prj_pt_neg(prj_pt_t out, prj_pt_src_t in)
{
	int ret;

	ret = prj_pt_check_initialized(in); EG(ret, err);

	if (out != in) { /* Copy point if not aliased */
		ret = prj_pt_init(out, in->crv); EG(ret, err);
		ret = prj_pt_copy(out, in); EG(ret, err);
	}

	/* Then, negate Y */
	ret = fp_neg(&(out->Y), &(out->Y));

err:
	return ret;
}

/*
 * Import a projective point from a buffer with the following layout; the 3
 * coordinates (elements of Fp) are each encoded on p_len bytes, where p_len
 * is the size of p in bytes (e.g. 66 for a prime p of 521 bits). Each
 * coordinate is encoded in big endian. Size of buffer must exactly match
 * 3 * p_len. The projective point is initialized by the function.
 *
 * The function returns 0 on success, -1 on error.
 */
int prj_pt_import_from_buf(prj_pt_t pt,
			   const u8 *pt_buf,
			   u16 pt_buf_len, ec_shortw_crv_src_t crv)
{
	int on_curve, ret;
	fp_ctx_src_t ctx;
	u16 coord_len;

	ret = ec_shortw_crv_check_initialized(crv); EG(ret, err);
	MUST_HAVE((pt_buf != NULL) && (pt != NULL), ret, err);

	ctx = crv->a.ctx;
	coord_len = (u16)BYTECEIL(ctx->p_bitlen);
	MUST_HAVE((pt_buf_len == (3 * coord_len)), ret, err);

	ret = fp_init_from_buf(&(pt->X), ctx, pt_buf, coord_len); EG(ret, err);
	ret = fp_init_from_buf(&(pt->Y), ctx, pt_buf + coord_len, coord_len); EG(ret, err);
	ret = fp_init_from_buf(&(pt->Z), ctx, pt_buf + (2 * coord_len), coord_len); EG(ret, err);

	/* Set the curve */
	pt->crv = crv;

	/* Mark the point as initialized */
	pt->magic = PRJ_PT_MAGIC;

	/* Check that the point is indeed on the provided curve, uninitialize it
	 * if this is not the case.
	 */
	ret = prj_pt_is_on_curve(pt, &on_curve); EG(ret, err);
	if (!on_curve){
		prj_pt_uninit(pt);
		ret = -1;
	}

err:
	PTR_NULLIFY(ctx);

	return ret;
}

/*
 * Import a projective point from an affine point buffer with the following layout; the 2
 * coordinates (elements of Fp) are each encoded on p_len bytes, where p_len
 * is the size of p in bytes (e.g. 66 for a prime p of 521 bits). Each
 * coordinate is encoded in big endian. Size of buffer must exactly match
 * 2 * p_len. The projective point is initialized by the function.
 *
 * The function returns 0 on success, -1 on error.
 */
int prj_pt_import_from_aff_buf(prj_pt_t pt,
			   const u8 *pt_buf,
			   u16 pt_buf_len, ec_shortw_crv_src_t crv)
{
	int ret, on_curve;
	fp_ctx_src_t ctx;
	u16 coord_len;

	ret = ec_shortw_crv_check_initialized(crv); EG(ret, err);
	MUST_HAVE((pt_buf != NULL) && (pt != NULL), ret, err);

	ctx = crv->a.ctx;
	coord_len = (u16)BYTECEIL(ctx->p_bitlen);
	MUST_HAVE((pt_buf_len == (2 * coord_len)), ret, err);

	ret = fp_init_from_buf(&(pt->X), ctx, pt_buf, coord_len); EG(ret, err);
	ret = fp_init_from_buf(&(pt->Y), ctx, pt_buf + coord_len, coord_len); EG(ret, err);
	/* Z coordinate is set to 1 */
	ret = fp_init(&(pt->Z), ctx); EG(ret, err);
	ret = fp_one(&(pt->Z)); EG(ret, err);

	/* Set the curve */
	pt->crv = crv;

	/* Mark the point as initialized */
	pt->magic = PRJ_PT_MAGIC;

	/* Check that the point is indeed on the provided curve, uninitialize it
	 * if this is not the case.
	 */
	ret = prj_pt_is_on_curve(pt, &on_curve); EG(ret, err);
	if (!on_curve){
		prj_pt_uninit(pt);
		ret = -1;
	}

err:
	PTR_NULLIFY(ctx);

	return ret;
}


/* Export a projective point to a buffer with the following layout; the 3
 * coordinates (elements of Fp) are each encoded on p_len bytes, where p_len
 * is the size of p in bytes (e.g. 66 for a prime p of 521 bits). Each
 * coordinate is encoded in big endian. Size of buffer must exactly match
 * 3 * p_len.
 *
 * The function returns 0 on success, -1 on error.
 */
int prj_pt_export_to_buf(prj_pt_src_t pt, u8 *pt_buf, u32 pt_buf_len)
{
	fp_ctx_src_t ctx;
	u16 coord_len;
	int ret, on_curve;

	ret = prj_pt_check_initialized(pt); EG(ret, err);

	MUST_HAVE((pt_buf != NULL), ret, err);

	/* The point to be exported must be on the curve */
	ret = prj_pt_is_on_curve(pt, &on_curve); EG(ret, err);
	MUST_HAVE((on_curve), ret, err);

	ctx = pt->crv->a.ctx;
	coord_len = (u16)BYTECEIL(ctx->p_bitlen);
	MUST_HAVE((pt_buf_len == (3 * coord_len)), ret, err);

	/* Export the three coordinates */
	ret = fp_export_to_buf(pt_buf, coord_len, &(pt->X)); EG(ret, err);
	ret = fp_export_to_buf(pt_buf + coord_len, coord_len, &(pt->Y)); EG(ret, err);
	ret = fp_export_to_buf(pt_buf + (2 * coord_len), coord_len, &(pt->Z));

err:
	PTR_NULLIFY(ctx);

	return ret;
}

/*
 * Export a projective point to an affine point buffer with the following
 * layout; the 2 coordinates (elements of Fp) are each encoded on p_len bytes,
 * where p_len is the size of p in bytes (e.g. 66 for a prime p of 521 bits).
 * Each coordinate is encoded in big endian. Size of buffer must exactly match
 * 2 * p_len.
 *
 * The function returns 0 on success, -1 on error.
 */
int prj_pt_export_to_aff_buf(prj_pt_src_t pt, u8 *pt_buf, u32 pt_buf_len)
{
	int ret, on_curve;
	aff_pt tmp_aff;
	tmp_aff.magic = WORD(0);

	ret = prj_pt_check_initialized(pt); EG(ret, err);

	MUST_HAVE((pt_buf != NULL), ret, err);

	/* The point to be exported must be on the curve */
	ret = prj_pt_is_on_curve(pt, &on_curve); EG(ret, err);
	MUST_HAVE((on_curve), ret, err);

	/* Move to the affine unique representation */
	ret = prj_pt_to_aff(&tmp_aff, pt); EG(ret, err);

	/* Export the affine point to the buffer */
	ret = aff_pt_export_to_buf(&tmp_aff, pt_buf, pt_buf_len);

err:
	aff_pt_uninit(&tmp_aff);

	return ret;
}


#ifdef NO_USE_COMPLETE_FORMULAS

/*
 * The function is an internal one: no check is performed on parameters,
 * this MUST be done by the caller:
 *
 *  - in is initialized
 *  - in and out must not be aliased
 *
 * The function will initialize 'out'. The function returns 0 on success, -1
 * on error.
 */
ATTRIBUTE_WARN_UNUSED_RET static int __prj_pt_dbl_monty_no_cf(prj_pt_t out, prj_pt_src_t in)
{
	fp XX, ZZ, w, s, ss, sss, R, RR, B, h;
	int ret;
	XX.magic = ZZ.magic = w.magic = s.magic = WORD(0);
	ss.magic = sss.magic = R.magic = WORD(0);
	RR.magic = B.magic = h.magic = WORD(0);

	ret = prj_pt_init(out, in->crv); EG(ret, err);

	ret = fp_init(&XX, out->crv->a.ctx); EG(ret, err);
	ret = fp_init(&ZZ, out->crv->a.ctx); EG(ret, err);
	ret = fp_init(&w, out->crv->a.ctx); EG(ret, err);
	ret = fp_init(&s, out->crv->a.ctx); EG(ret, err);
	ret = fp_init(&ss, out->crv->a.ctx); EG(ret, err);
	ret = fp_init(&sss, out->crv->a.ctx); EG(ret, err);
	ret = fp_init(&R, out->crv->a.ctx); EG(ret, err);
	ret = fp_init(&RR, out->crv->a.ctx); EG(ret, err);
	ret = fp_init(&B, out->crv->a.ctx); EG(ret, err);
	ret = fp_init(&h, out->crv->a.ctx); EG(ret, err);

	/* XX = X1² */
	ret = fp_sqr_monty(&XX, &(in->X)); EG(ret, err);

	/* ZZ = Z1² */
	ret = fp_sqr_monty(&ZZ, &(in->Z)); EG(ret, err);

	/* w = a*ZZ+3*XX */
	ret = fp_mul_monty(&w, &(in->crv->a_monty), &ZZ); EG(ret, err);
	ret = fp_add_monty(&w, &w, &XX); EG(ret, err);
	ret = fp_add_monty(&w, &w, &XX); EG(ret, err);
	ret = fp_add_monty(&w, &w, &XX); EG(ret, err);

	/* s = 2*Y1*Z1 */
	ret = fp_mul_monty(&s, &(in->Y), &(in->Z)); EG(ret, err);
	ret = fp_add_monty(&s, &s, &s); EG(ret, err);

	/* ss = s² */
	ret = fp_sqr_monty(&ss, &s); EG(ret, err);

	/* sss = s*ss */
	ret = fp_mul_monty(&sss, &s, &ss); EG(ret, err);

	/* R = Y1*s */
	ret = fp_mul_monty(&R, &(in->Y), &s); EG(ret, err);

	/* RR = R² */
	ret = fp_sqr_monty(&RR, &R); EG(ret, err);

	/* B = (X1+R)²-XX-RR */
	ret = fp_add_monty(&R, &R, &(in->X)); EG(ret, err);
	ret = fp_sqr_monty(&B, &R); EG(ret, err);
	ret = fp_sub_monty(&B, &B, &XX); EG(ret, err);
	ret = fp_sub_monty(&B, &B, &RR); EG(ret, err);

	/* h = w²-2*B */
	ret = fp_sqr_monty(&h, &w); EG(ret, err);
	ret = fp_sub_monty(&h, &h, &B); EG(ret, err);
	ret = fp_sub_monty(&h, &h, &B); EG(ret, err);

	/* X3 = h*s */
	ret = fp_mul_monty(&(out->X), &h, &s); EG(ret, err);

	/* Y3 = w*(B-h)-2*RR */
	ret = fp_sub_monty(&B, &B, &h); EG(ret, err);
	ret = fp_mul_monty(&(out->Y), &w, &B); EG(ret, err);
	ret = fp_sub_monty(&(out->Y), &(out->Y), &RR); EG(ret, err);
	ret = fp_sub_monty(&(out->Y), &(out->Y), &RR); EG(ret, err);

	/* Z3 = sss */
	ret = fp_copy(&(out->Z), &sss);

err:
	fp_uninit(&XX);
	fp_uninit(&ZZ);
	fp_uninit(&w);
	fp_uninit(&s);
	fp_uninit(&ss);
	fp_uninit(&sss);
	fp_uninit(&R);
	fp_uninit(&RR);
	fp_uninit(&B);
	fp_uninit(&h);

	return ret;
}

/*
 * The function is an internal one: no check is performed on parameters,
 * this MUST be done by the caller:
 *
 *  - in1 and in2 are initialized
 *  - in1 and in2 are on the same curve
 *  - in1/in2 and out must not be aliased
 *  - in1 and in2 must not be equal, opposite or have identical value
 *
 * The function will initialize 'out'. The function returns 0 on success, -1
 * on error.
 */
ATTRIBUTE_WARN_UNUSED_RET static int ___prj_pt_add_monty_no_cf(prj_pt_t out,
							       prj_pt_src_t in1,
							       prj_pt_src_t in2)
{
	fp Y1Z2, X1Z2, Z1Z2, u, uu, v, vv, vvv, R, A;
	int ret;
	Y1Z2.magic = X1Z2.magic = Z1Z2.magic = u.magic = uu.magic = v.magic = WORD(0);
	vv.magic = vvv.magic = R.magic = A.magic = WORD(0);

	ret = prj_pt_init(out, in1->crv); EG(ret, err);

	ret = fp_init(&Y1Z2, out->crv->a.ctx); EG(ret, err);
	ret = fp_init(&X1Z2, out->crv->a.ctx); EG(ret, err);
	ret = fp_init(&Z1Z2, out->crv->a.ctx); EG(ret, err);
	ret = fp_init(&u, out->crv->a.ctx); EG(ret, err);
	ret = fp_init(&uu, out->crv->a.ctx); EG(ret, err);
	ret = fp_init(&v, out->crv->a.ctx); EG(ret, err);
	ret = fp_init(&vv, out->crv->a.ctx); EG(ret, err);
	ret = fp_init(&vvv, out->crv->a.ctx); EG(ret, err);
	ret = fp_init(&R, out->crv->a.ctx); EG(ret, err);
	ret = fp_init(&A, out->crv->a.ctx); EG(ret, err);

	/* Y1Z2 = Y1*Z2 */
	ret = fp_mul_monty(&Y1Z2, &(in1->Y), &(in2->Z)); EG(ret, err);

	/* X1Z2 = X1*Z2 */
	ret = fp_mul_monty(&X1Z2, &(in1->X), &(in2->Z)); EG(ret, err);

	/* Z1Z2 = Z1*Z2 */
	ret = fp_mul_monty(&Z1Z2, &(in1->Z), &(in2->Z)); EG(ret, err);

	/* u = Y2*Z1-Y1Z2 */
	ret = fp_mul_monty(&u, &(in2->Y), &(in1->Z)); EG(ret, err);
	ret = fp_sub_monty(&u, &u, &Y1Z2); EG(ret, err);

	/* uu = u² */
	ret = fp_sqr_monty(&uu, &u); EG(ret, err);

	/* v = X2*Z1-X1Z2 */
	ret = fp_mul_monty(&v, &(in2->X), &(in1->Z)); EG(ret, err);
	ret = fp_sub_monty(&v, &v, &X1Z2); EG(ret, err);

	/* vv = v² */
	ret = fp_sqr_monty(&vv, &v); EG(ret, err);

	/* vvv = v*vv */
	ret = fp_mul_monty(&vvv, &v, &vv); EG(ret, err);

	/* R = vv*X1Z2 */
	ret = fp_mul_monty(&R, &vv, &X1Z2); EG(ret, err);

	/* A = uu*Z1Z2-vvv-2*R */
	ret = fp_mul_monty(&A, &uu, &Z1Z2); EG(ret, err);
	ret = fp_sub_monty(&A, &A, &vvv); EG(ret, err);
	ret = fp_sub_monty(&A, &A, &R); EG(ret, err);
	ret = fp_sub_monty(&A, &A, &R); EG(ret, err);

	/* X3 = v*A */
	ret = fp_mul_monty(&(out->X), &v, &A); EG(ret, err);

	/* Y3 = u*(R-A)-vvv*Y1Z2 */
	ret = fp_sub_monty(&R, &R, &A); EG(ret, err);
	ret = fp_mul_monty(&(out->Y), &u, &R); EG(ret, err);
	ret = fp_mul_monty(&R, &vvv, &Y1Z2); EG(ret, err);
	ret = fp_sub_monty(&(out->Y), &(out->Y), &R); EG(ret, err);

	/* Z3 = vvv*Z1Z2 */
	ret = fp_mul_monty(&(out->Z), &vvv, &Z1Z2);

err:
	fp_uninit(&Y1Z2);
	fp_uninit(&X1Z2);
	fp_uninit(&Z1Z2);
	fp_uninit(&u);
	fp_uninit(&uu);
	fp_uninit(&v);
	fp_uninit(&vv);
	fp_uninit(&vvv);
	fp_uninit(&R);
	fp_uninit(&A);

	return ret;
}

/*
 * Public version of the addition w/o complete formulas to handle the case
 * where the inputs are zero or opposite. Returns 0 on success, -1 on error.
 */
ATTRIBUTE_WARN_UNUSED_RET static int __prj_pt_add_monty_no_cf(prj_pt_t out, prj_pt_src_t in1, prj_pt_src_t in2)
{
	int ret, iszero, eq_or_opp, cmp;

	ret = prj_pt_check_initialized(in1); EG(ret, err);
	ret = prj_pt_check_initialized(in2); EG(ret, err);
	MUST_HAVE((in1->crv == in2->crv), ret, err);

	ret = prj_pt_iszero(in1, &iszero); EG(ret, err);
	if (iszero) {
		/* in1 at infinity, output in2 in all cases */
		ret = prj_pt_init(out, in2->crv); EG(ret, err);
		ret = prj_pt_copy(out, in2); EG(ret, err);
	} else {
		/* in1 not at infinity, output in2 */
		ret = prj_pt_iszero(in2, &iszero); EG(ret, err);
		if (iszero) {
			/* in2 at infinity, output in1 */
			ret = prj_pt_init(out, in1->crv); EG(ret, err);
			ret = prj_pt_copy(out, in1); EG(ret, err);
		} else {
			/* enither in1, nor in2 at infinity */

			/*
			 * The following test which guarantees in1 and in2 are not
			 * equal or opposite needs to be rewritten because it
			 * has a *HUGE* impact on perf (ec_self_tests run on
			 * all test vectors takes 24 times as long with this
			 * enabled). The same exists in non monty version.
			 */
			ret = prj_pt_eq_or_opp(in1, in2, &eq_or_opp); EG(ret, err);
			if (eq_or_opp) {
				/* in1 and in2 are either equal or opposite */
				ret = prj_pt_cmp(in1, in2, &cmp); EG(ret, err);
				if (cmp == 0) {
					/* in1 == in2 => doubling w/o cf */
					ret = __prj_pt_dbl_monty_no_cf(out, in1); EG(ret, err);
				} else {
					/* in1 == -in2 => output zero (point at infinity) */
					ret = prj_pt_init(out, in1->crv); EG(ret, err);
					ret = prj_pt_zero(out); EG(ret, err);
				}
			} else {
				/*
				 * in1 and in2 are neither 0, nor equal or
				 * opposite. Use the basic monty addition
				 * implementation w/o complete formulas.
				 */
				ret = ___prj_pt_add_monty_no_cf(out, in1, in2); EG(ret, err);
			}
		}
	}

err:
	return ret;
}


#else /* NO_USE_COMPLETE_FORMULAS */


/*
 * If NO_USE_COMPLETE_FORMULAS flag is not defined addition formulas from Algorithm 3
 * of https://joostrenes.nl/publications/complete.pdf are used, otherwise
 * http://www.hyperelliptic.org/EFD/g1p/auto-shortw-projective.html#doubling-dbl-2007-bl
 */
ATTRIBUTE_WARN_UNUSED_RET static int __prj_pt_dbl_monty_cf(prj_pt_t out, prj_pt_src_t in)
{
	fp t0, t1, t2, t3;
	int ret;
	t0.magic = t1.magic = t2.magic = t3.magic = WORD(0);

	ret = prj_pt_init(out, in->crv); EG(ret, err);

	ret = fp_init(&t0, out->crv->a.ctx); EG(ret, err);
	ret = fp_init(&t1, out->crv->a.ctx); EG(ret, err);
	ret = fp_init(&t2, out->crv->a.ctx); EG(ret, err);
	ret = fp_init(&t3, out->crv->a.ctx); EG(ret, err);

	ret = fp_mul_monty(&t0, &in->X, &in->X); EG(ret, err);
	ret = fp_mul_monty(&t1, &in->Y, &in->Y); EG(ret, err);
	ret = fp_mul_monty(&t2, &in->Z, &in->Z); EG(ret, err);
	ret = fp_mul_monty(&t3, &in->X, &in->Y); EG(ret, err);
	ret = fp_add_monty(&t3, &t3, &t3); EG(ret, err);

	ret = fp_mul_monty(&out->Z, &in->X, &in->Z); EG(ret, err);
	ret = fp_add_monty(&out->Z, &out->Z, &out->Z); EG(ret, err);
	ret = fp_mul_monty(&out->X, &in->crv->a_monty, &out->Z); EG(ret, err);
	ret = fp_mul_monty(&out->Y, &in->crv->b3_monty, &t2); EG(ret, err);
	ret = fp_add_monty(&out->Y, &out->X, &out->Y); EG(ret, err);

	ret = fp_sub_monty(&out->X, &t1, &out->Y); EG(ret, err);
	ret = fp_add_monty(&out->Y, &t1, &out->Y); EG(ret, err);
	ret = fp_mul_monty(&out->Y, &out->X, &out->Y); EG(ret, err);
	ret = fp_mul_monty(&out->X, &t3, &out->X); EG(ret, err);
	ret = fp_mul_monty(&out->Z, &in->crv->b3_monty, &out->Z); EG(ret, err);

	ret = fp_mul_monty(&t2, &in->crv->a_monty, &t2); EG(ret, err);
	ret = fp_sub_monty(&t3, &t0, &t2); EG(ret, err);
	ret = fp_mul_monty(&t3, &in->crv->a_monty, &t3); EG(ret, err);
	ret = fp_add_monty(&t3, &t3, &out->Z); EG(ret, err);
	ret = fp_add_monty(&out->Z, &t0, &t0); EG(ret, err);

	ret = fp_add_monty(&t0, &out->Z, &t0); EG(ret, err);
	ret = fp_add_monty(&t0, &t0, &t2); EG(ret, err);
	ret = fp_mul_monty(&t0, &t0, &t3); EG(ret, err);
	ret = fp_add_monty(&out->Y, &out->Y, &t0); EG(ret, err);
	ret = fp_mul_monty(&t2, &in->Y, &in->Z); EG(ret, err);

	ret = fp_add_monty(&t2, &t2, &t2); EG(ret, err);
	ret = fp_mul_monty(&t0, &t2, &t3); EG(ret, err);
	ret = fp_sub_monty(&out->X, &out->X, &t0); EG(ret, err);
	ret = fp_mul_monty(&out->Z, &t2, &t1); EG(ret, err);
	ret = fp_add_monty(&out->Z, &out->Z, &out->Z); EG(ret, err);

	ret = fp_add_monty(&out->Z, &out->Z, &out->Z);

err:
	fp_uninit(&t0);
	fp_uninit(&t1);
	fp_uninit(&t2);
	fp_uninit(&t3);

	return ret;
}

/*
 * If NO_USE_COMPLETE_FORMULAS flag is not defined addition formulas from Algorithm 1
 * of https://joostrenes.nl/publications/complete.pdf are used, otherwise
 * http://www.hyperelliptic.org/EFD/g1p/auto-shortw-projective.html#addition-add-1998-cmo-2
 */

/*
 * The function is an internal one: no check is performed on parameters,
 * this MUST be done by the caller:
 *
 *  - in1 and in2 are initialized
 *  - in1 and in2 are on the same curve
 *  - in1/in2 and out must not be aliased
 *  - in1 and in2 must not be an "exceptional" pair, i.e. (in1-in2) is not a point
 *  of order exactly 2
 *
 * The function will initialize 'out'. The function returns 0 on success, -1
 * on error.
 */
ATTRIBUTE_WARN_UNUSED_RET static int __prj_pt_add_monty_cf(prj_pt_t out,
							   prj_pt_src_t in1,
							   prj_pt_src_t in2)
{
	int cmp1, cmp2;
	fp t0, t1, t2, t3, t4, t5;
	int ret;
	t0.magic = t1.magic = t2.magic = WORD(0);
	t3.magic = t4.magic = t5.magic = WORD(0);

	ret = prj_pt_init(out, in1->crv); EG(ret, err);

	ret = fp_init(&t0, out->crv->a.ctx); EG(ret, err);
	ret = fp_init(&t1, out->crv->a.ctx); EG(ret, err);
	ret = fp_init(&t2, out->crv->a.ctx); EG(ret, err);
	ret = fp_init(&t3, out->crv->a.ctx); EG(ret, err);
	ret = fp_init(&t4, out->crv->a.ctx); EG(ret, err);
	ret = fp_init(&t5, out->crv->a.ctx); EG(ret, err);

	ret = fp_mul_monty(&t0, &in1->X, &in2->X); EG(ret, err);
	ret = fp_mul_monty(&t1, &in1->Y, &in2->Y); EG(ret, err);
	ret = fp_mul_monty(&t2, &in1->Z, &in2->Z); EG(ret, err);
	ret = fp_add_monty(&t3, &in1->X, &in1->Y); EG(ret, err);
	ret = fp_add_monty(&t4, &in2->X, &in2->Y); EG(ret, err);

	ret = fp_mul_monty(&t3, &t3, &t4); EG(ret, err);
	ret = fp_add_monty(&t4, &t0, &t1); EG(ret, err);
	ret = fp_sub_monty(&t3, &t3, &t4); EG(ret, err);
	ret = fp_add_monty(&t4, &in1->X, &in1->Z); EG(ret, err);
	ret = fp_add_monty(&t5, &in2->X, &in2->Z); EG(ret, err);

	ret = fp_mul_monty(&t4, &t4, &t5); EG(ret, err);
	ret = fp_add_monty(&t5, &t0, &t2); EG(ret, err);
	ret = fp_sub_monty(&t4, &t4, &t5); EG(ret, err);
	ret = fp_add_monty(&t5, &in1->Y, &in1->Z); EG(ret, err);
	ret = fp_add_monty(&out->X, &in2->Y, &in2->Z); EG(ret, err);

	ret = fp_mul_monty(&t5, &t5, &out->X); EG(ret, err);
	ret = fp_add_monty(&out->X, &t1, &t2); EG(ret, err);
	ret = fp_sub_monty(&t5, &t5, &out->X); EG(ret, err);
	ret = fp_mul_monty(&out->Z, &in1->crv->a_monty, &t4); EG(ret, err);
	ret = fp_mul_monty(&out->X, &in1->crv->b3_monty, &t2); EG(ret, err);

	ret = fp_add_monty(&out->Z, &out->X, &out->Z); EG(ret, err);
	ret = fp_sub_monty(&out->X, &t1, &out->Z); EG(ret, err);
	ret = fp_add_monty(&out->Z, &t1, &out->Z); EG(ret, err);
	ret = fp_mul_monty(&out->Y, &out->X, &out->Z); EG(ret, err);
	ret = fp_add_monty(&t1, &t0, &t0); EG(ret, err);

	ret = fp_add_monty(&t1, &t1, &t0); EG(ret, err);
	ret = fp_mul_monty(&t2, &in1->crv->a_monty, &t2); EG(ret, err);
	ret = fp_mul_monty(&t4, &in1->crv->b3_monty, &t4); EG(ret, err);
	ret = fp_add_monty(&t1, &t1, &t2); EG(ret, err);
	ret = fp_sub_monty(&t2, &t0, &t2); EG(ret, err);

	ret = fp_mul_monty(&t2, &in1->crv->a_monty, &t2); EG(ret, err);
	ret = fp_add_monty(&t4, &t4, &t2); EG(ret, err);
	ret = fp_mul_monty(&t0, &t1, &t4); EG(ret, err);
	ret = fp_add_monty(&out->Y, &out->Y, &t0); EG(ret, err);
	ret = fp_mul_monty(&t0, &t5, &t4); EG(ret, err);

	ret = fp_mul_monty(&out->X, &t3, &out->X); EG(ret, err);
	ret = fp_sub_monty(&out->X, &out->X, &t0); EG(ret, err);
	ret = fp_mul_monty(&t0, &t3, &t1); EG(ret, err);
	ret = fp_mul_monty(&out->Z, &t5, &out->Z); EG(ret, err);
	ret = fp_add_monty(&out->Z, &out->Z, &t0);

	/* Check for "exceptional" pairs of input points with
	 * checking if Y = Z = 0 as output (see the Bosma-Lenstra
	 * article "Complete Systems of Two Addition Laws for
	 * Elliptic Curves"). This should only happen on composite
	 * order curves (i.e. not on prime order curves).
	 *
	 * In this case, we raise an error as the result is
	 * not sound. This should not happen in our nominal
	 * cases with regular signature and protocols, and if
	 * it happens this usually means that bad points have
	 * been injected.
	 *
	 * NOTE: if for some reasons you need to deal with
	 * all the possible pairs of points including these
	 * exceptional pairs of inputs with an order 2 difference,
	 * you should fallback to the incomplete formulas using the
	 * COMPLETE=0 compilation toggle. Beware that in this
	 * case, the library will be more sensitive to
	 * side-channel attacks.
	 */
	ret = fp_iszero(&(out->Z), &cmp1); EG(ret, err);
	ret = fp_iszero(&(out->Y), &cmp2); EG(ret, err);
	MUST_HAVE(!((cmp1) && (cmp2)), ret, err);

err:
	fp_uninit(&t0);
	fp_uninit(&t1);
	fp_uninit(&t2);
	fp_uninit(&t3);
	fp_uninit(&t4);
	fp_uninit(&t5);

	return ret;
}
#endif  /* NO_USE_COMPLETE_FORMULAS */

/*
 * Internal function:
 *
 *  - not supporting aliasing,
 *  - requiring caller to check in parameter is initialized
 *
 * Based on library configuration, the function either use complete formulas
 * or not.
 */
static int _prj_pt_dbl_monty(prj_pt_t out, prj_pt_src_t in)
{
	int ret;

#ifdef NO_USE_COMPLETE_FORMULAS
	int iszero;
	ret = prj_pt_iszero(in, &iszero); EG(ret, err);
	if (iszero) {
		ret = prj_pt_init(out, in->crv); EG(ret, err);
		ret = prj_pt_zero(out);
	} else {
		ret = __prj_pt_dbl_monty_no_cf(out, in);
	}
#else
	ret = __prj_pt_dbl_monty_cf(out, in); EG(ret, err);
#endif

err:
	return ret;
}

/*
 * Internal version that peform in place doubling of given val,
 * by using a temporary copy. Sanity checks on parameters must
 * be done by caller.
 */
ATTRIBUTE_WARN_UNUSED_RET static int _prj_pt_dbl_monty_aliased(prj_pt_t val)
{
	prj_pt out_cpy;
	int ret;
	out_cpy.magic = WORD(0);

	ret = _prj_pt_dbl_monty(&out_cpy, val); EG(ret, err);
	ret = prj_pt_copy(val, &out_cpy);

err:
	prj_pt_uninit(&out_cpy);

	return ret;
}

/*
 * Public function for projective point doubling. The function handles the init
 * check of 'in' parameter which must be guaranteed for internal functions.
 * 'out' parameter need not be initialized and can be aliased with 'in'
 * parameter.
 *
 * The function returns 0 on success, -1 on error.
 */
ATTRIBUTE_WARN_UNUSED_RET int prj_pt_dbl(prj_pt_t out, prj_pt_src_t in)
{
	int ret;

	ret = prj_pt_check_initialized(in); EG(ret, err);

	if (out == in) {
		ret = _prj_pt_dbl_monty_aliased(out);
	} else {
		ret = _prj_pt_dbl_monty(out, in);
	}

err:
	return ret;
}

/*
 * Internal function:
 *
 *  - not supporting aliasing,
 *  - requiring caller to check in1 and in2 parameter
 *
 * Based on library configuration, the function either use complete formulas
 * or not.
 */
ATTRIBUTE_WARN_UNUSED_RET static inline int _prj_pt_add_monty(prj_pt_t out,
							      prj_pt_src_t in1,
							      prj_pt_src_t in2)
{
#ifndef NO_USE_COMPLETE_FORMULAS
	return __prj_pt_add_monty_cf(out, in1, in2);
#else
	return __prj_pt_add_monty_no_cf(out, in1, in2);
#endif
}

/*
 * The function is an internal one that specifically handles aliasing. No check
 * is performed on parameters, this MUST be done by the caller:
 *
 *  - in1 and in2 are initialized
 *  - in1 and in2 are on the same curve
 *
 * The function will initialize 'out'. The function returns 0 on success, -1
 * on error.
 */
ATTRIBUTE_WARN_UNUSED_RET static int _prj_pt_add_monty_aliased(prj_pt_t out,
								prj_pt_src_t in1,
								prj_pt_src_t in2)
{
	int ret;
	prj_pt out_cpy;
	out_cpy.magic = WORD(0);

	ret = _prj_pt_add_monty(&out_cpy, in1, in2); EG(ret, err);
	ret = prj_pt_copy(out, &out_cpy); EG(ret, err);

err:
	prj_pt_uninit(&out_cpy);

	return ret;
}

/*
 * Public function for projective point addition. The function handles the
 * init checks of 'in1' and 'in2' parameters, along with the check they
 * use the same curve. This must be guaranteed for internal functions.
 * 'out' parameter need not be initialized and can be aliased with either
 * 'in1' or 'in2' parameter.
 *
 * The function returns 0 on success, -1 on error.
 */
int prj_pt_add(prj_pt_t out, prj_pt_src_t in1, prj_pt_src_t in2)
{
	int ret;

	ret = prj_pt_check_initialized(in1); EG(ret, err);
	ret = prj_pt_check_initialized(in2); EG(ret, err);
	MUST_HAVE((in1->crv == in2->crv), ret, err);

	if ((out == in1) || (out == in2)) {
		ret = _prj_pt_add_monty_aliased(out, in1, in2);
	} else {
		ret = _prj_pt_add_monty(out, in1, in2);
	}

err:
	return ret;
}

/*******************************************************************************/
/****** Scalar multiplication algorithms ***************************************/
/***********/
/*
 * The description below summarizes the following algorithms.
 *
 * Double-and-Add-Always and Montgomery Ladder masked using Itoh et al. anti-ADPA
 * (Address-bit DPA) countermeasure.
 * See "A Practical Countermeasure against Address-Bit Differential Power Analysis"
 * by Itoh, Izu and Takenaka for more information.
 *
 * NOTE: these masked variants of the Double-and-Add-Always and Montgomery Ladder algorithms
 * are used by default as Itoh et al. countermeasure has a very small impact on performance
 * and is inherently more robust againt DPA. The only case where we use another variant is
 * for devices with low memory as Itoh requires many temporary variables that consume many
 * temporary stack space.
 *
 * NOTE: the algorithms inherently depend on the MSB of the
 * scalar. In order to avoid leaking this MSB and fall into HNP (Hidden Number
 * Problem) issues, we use the trick described in https://eprint.iacr.org/2011/232.pdf
 * to have the MSB always set. However, since the scalar m might be less or bigger than
 * the order q of the curve, we distinguish three situations:
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
 *
 *   => We only deal with 0 <= m < (q**2) using the countermeasure. When m >= (q**2),
 *      we stick with m' = m, accepting MSB issues (not much can be done in this case
 *      anyways). In the two first cases, Double-and-Add-Always and Montgomery Ladder are
 *      performed in constant time wrt the size of the scalar m.
 */
/***********/
/*
 * Internal point blinding function: as it is internal, in is supposed to be initialized and
 * aliasing is NOT supported.
 */
ATTRIBUTE_WARN_UNUSED_RET static int _blind_projective_point(prj_pt_t out, prj_pt_src_t in)
{
	int ret;

	/* Random for projective coordinates masking */
	/* NOTE: to limit stack usage, we reuse out->Z as a temporary
	 * variable. This does not work if in == out, hence the check.
	 */
	MUST_HAVE((in != out), ret, err);

	ret = prj_pt_init(out, in->crv); EG(ret, err);

	/* Get a random value l in Fp */
	ret = fp_get_random(&(out->Z), in->X.ctx); EG(ret, err);

	/*
	 * Blind the point with projective coordinates
	 * (X, Y, Z) => (l*X, l*Y, l*Z)
	 */
	ret = fp_mul_monty(&(out->X), &(in->X), &(out->Z)); EG(ret, err);
	ret = fp_mul_monty(&(out->Y), &(in->Y), &(out->Z)); EG(ret, err);
	ret = fp_mul_monty(&(out->Z), &(in->Z), &(out->Z));

err:
	return ret;
}

/* If nothing is specified regarding the scalar multiplication algorithm, we use
 * the Montgomery Ladder. For the specific case of small stack devices, we release
 * some pressure on the stack by explicitly using double and always WITHOUT the Itoh
 * et al. countermeasure against A-DPA as it is quite consuming.
 */
#if defined(USE_SMALL_STACK) && defined(USE_MONTY_LADDER)
#error "Small stack is only compatible with USE_DOUBLE_ADD_ALWAYS while USE_MONTY_LADDER has been explicitly asked!"
#endif

#if defined(USE_SMALL_STACK)
#ifndef USE_DOUBLE_ADD_ALWAYS
#define USE_DOUBLE_ADD_ALWAYS
#endif
#endif

#if !defined(USE_DOUBLE_ADD_ALWAYS) && !defined(USE_MONTY_LADDER)
#define USE_MONTY_LADDER
#endif

#if defined(USE_DOUBLE_ADD_ALWAYS) && defined(USE_MONTY_LADDER)
#error "You can either choose USE_DOUBLE_ADD_ALWAYS or USE_MONTY_LADDER, not both!"
#endif

#if defined(USE_DOUBLE_ADD_ALWAYS) && !defined(USE_SMALL_STACK)
ATTRIBUTE_WARN_UNUSED_RET static int _prj_pt_mul_ltr_monty_dbl_add_always(prj_pt_t out, nn_src_t m, prj_pt_src_t in)
{
	/* We use Itoh et al. notations here for T and the random r */
	prj_pt T[3];
	bitcnt_t mlen;
	u8 mbit, rbit;
	/* Random for masking the Double and Add Always algorithm */
	nn r;
	/* The new scalar we will use with MSB fixed to 1 (noted m' above).
	 * This helps dealing with constant time.
	 */
	nn m_msb_fixed;
	nn_src_t curve_order;
	nn curve_order_square;
	int ret, ret_ops, cmp;
	r.magic = m_msb_fixed.magic = curve_order_square.magic = WORD(0);
	T[0].magic = T[1].magic = T[2].magic = WORD(0);

	/* Compute m' from m depending on the rule described above */
	curve_order = &(in->crv->order);
	/* First compute q**2 */
	ret = nn_sqr(&curve_order_square, curve_order); EG(ret, err);
	/* Then compute m' depending on m size */
	ret = nn_cmp(m, curve_order, &cmp); EG(ret, err);
	if (cmp < 0){
		bitcnt_t msb_bit_len, order_bitlen;

		/* Case where m < q */
		ret = nn_add(&m_msb_fixed, m, curve_order); EG(ret, err);
		ret = nn_bitlen(&m_msb_fixed, &msb_bit_len); EG(ret, err);
		ret = nn_bitlen(curve_order, &order_bitlen); EG(ret, err);
		ret = nn_cnd_add((msb_bit_len == order_bitlen), &m_msb_fixed,
				  &m_msb_fixed, curve_order); EG(ret, err);
	} else {
		ret = nn_cmp(m, &curve_order_square, &cmp); EG(ret, err);
		if (cmp < 0) {
			bitcnt_t msb_bit_len, curve_order_square_bitlen;

			/* Case where m >= q and m < (q**2) */
			ret = nn_add(&m_msb_fixed, m, &curve_order_square); EG(ret, err);
			ret = nn_bitlen(&m_msb_fixed, &msb_bit_len); EG(ret, err);
			ret = nn_bitlen(&curve_order_square, &curve_order_square_bitlen); EG(ret, err);
			ret = nn_cnd_add((msb_bit_len == curve_order_square_bitlen),
					&m_msb_fixed, &m_msb_fixed, &curve_order_square); EG(ret, err);
		} else {
			/* Case where m >= (q**2) */
			ret = nn_copy(&m_msb_fixed, m); EG(ret, err);
		}
	}
	ret = nn_bitlen(&m_msb_fixed, &mlen); EG(ret, err);
	MUST_HAVE(mlen != 0, ret, err);
	mlen--;

	/* Hide possible internal failures for double and add
	 * operations and perform the operation in constant time.
	 */
	ret_ops = 0;

	/* Get a random r with the same size of m_msb_fixed */
	ret = nn_get_random_len(&r, m_msb_fixed.wlen * WORD_BYTES); EG(ret, err);

	ret = nn_getbit(&r, mlen, &rbit); EG(ret, err);

	/* Initialize points */
	ret = prj_pt_init(&T[0], in->crv); EG(ret, err);
	ret = prj_pt_init(&T[1], in->crv); EG(ret, err);

	/*
	 * T[2] = R(P)
	 * Blind the point with projective coordinates
	 * (X, Y, Z) => (l*X, l*Y, l*Z)
	 */
	ret = _blind_projective_point(&T[2], in); EG(ret, err);

	/*  T[r[n-1]] = T[2] */
	ret = prj_pt_copy(&T[rbit], &T[2]); EG(ret, err);

	/* Main loop of Double and Add Always */
	while (mlen > 0) {
		u8 rbit_next;
		--mlen;
		/* rbit is r[i+1], and rbit_next is r[i] */
		ret = nn_getbit(&r, mlen, &rbit_next); EG(ret, err);

		/* mbit is m[i] */
		ret = nn_getbit(&m_msb_fixed, mlen, &mbit); EG(ret, err);

		/* Double: T[r[i+1]] = ECDBL(T[r[i+1]]) */
#ifndef NO_USE_COMPLETE_FORMULAS
		/*
		 * NOTE: in case of complete formulas, we use the
		 * addition for doubling, incurring a small performance hit
		 * for better SCA resistance.
		 */
		ret_ops |= prj_pt_add(&T[rbit], &T[rbit], &T[rbit]);
#else
		ret_ops |= prj_pt_dbl(&T[rbit], &T[rbit]);
#endif
		/* Add:  T[1-r[i+1]] = ECADD(T[r[i+1]],T[2]) */
		ret_ops |= prj_pt_add(&T[1-rbit], &T[rbit], &T[2]);

		/*
		 * T[r[i]] = T[d[i] ^ r[i+1]]
		 * NOTE: we use the low level nn_copy function here to avoid
		 * any possible leakage on operands with prj_pt_copy
		 */
		ret = nn_copy(&(T[rbit_next].X.fp_val), &(T[mbit ^ rbit].X.fp_val)); EG(ret, err);
		ret = nn_copy(&(T[rbit_next].Y.fp_val), &(T[mbit ^ rbit].Y.fp_val)); EG(ret, err);
		ret = nn_copy(&(T[rbit_next].Z.fp_val), &(T[mbit ^ rbit].Z.fp_val)); EG(ret, err);

		/* Update rbit */
		rbit = rbit_next;
	}
	/* Output: T[r[0]] */
	ret = prj_pt_copy(out, &T[rbit]); EG(ret, err);

	/* Take into consideration our double and add errors */
	ret |= ret_ops;

err:
	prj_pt_uninit(&T[0]);
	prj_pt_uninit(&T[1]);
	prj_pt_uninit(&T[2]);
	nn_uninit(&r);
	nn_uninit(&m_msb_fixed);
	nn_uninit(&curve_order_square);

	PTR_NULLIFY(curve_order);

	return ret;
}
#endif

#if defined(USE_DOUBLE_ADD_ALWAYS) && defined(USE_SMALL_STACK)
/* NOTE: in small stack case where we compile for low memory devices, we do not use Itoh et al. countermeasure
 * as it requires too much temporary space on the stack.
 */
ATTRIBUTE_WARN_UNUSED_RET static int _prj_pt_mul_ltr_monty_dbl_add_always(prj_pt_t out, nn_src_t m, prj_pt_src_t in)
{
	int ret, ret_ops;

	/* Hide possible internal failures for double and add
	 * operations and perform the operation in constant time.
	 */
	ret_ops = 0;

	/* Blind the input point projective coordinates */
	ret = _blind_projective_point(out, in); EG(ret, err);

	/*******************/
	{
		bitcnt_t mlen;
		u8 mbit;
		/* The new scalar we will use with MSB fixed to 1 (noted m' above).
		 * This helps dealing with constant time.
		 */
		nn m_msb_fixed;
		nn_src_t curve_order;
		int cmp;
		m_msb_fixed.magic = WORD(0);

		{
			nn curve_order_square;
			curve_order_square.magic = WORD(0);

			/* Compute m' from m depending on the rule described above */
			curve_order = &(in->crv->order);
			/* First compute q**2 */
			ret = nn_sqr(&curve_order_square, curve_order); EG(ret, err1);
			/* Then compute m' depending on m size */
			ret = nn_cmp(m, curve_order, &cmp); EG(ret, err1);
			if (cmp < 0){
				bitcnt_t msb_bit_len, order_bitlen;

				/* Case where m < q */
				ret = nn_add(&m_msb_fixed, m, curve_order); EG(ret, err1);
				ret = nn_bitlen(&m_msb_fixed, &msb_bit_len); EG(ret, err1);
				ret = nn_bitlen(curve_order, &order_bitlen); EG(ret, err1);
				ret = nn_cnd_add((msb_bit_len == order_bitlen), &m_msb_fixed,
					  &m_msb_fixed, curve_order); EG(ret, err1);
			} else {
				ret = nn_cmp(m, &curve_order_square, &cmp); EG(ret, err1);
				if (cmp < 0) {
					bitcnt_t msb_bit_len, curve_order_square_bitlen;

					/* Case where m >= q and m < (q**2) */
					ret = nn_add(&m_msb_fixed, m, &curve_order_square); EG(ret, err1);
					ret = nn_bitlen(&m_msb_fixed, &msb_bit_len); EG(ret, err1);
					ret = nn_bitlen(&curve_order_square, &curve_order_square_bitlen); EG(ret, err1);
					ret = nn_cnd_add((msb_bit_len == curve_order_square_bitlen),
							&m_msb_fixed, &m_msb_fixed, &curve_order_square); EG(ret, err1);
				} else {
					/* Case where m >= (q**2) */
					ret = nn_copy(&m_msb_fixed, m); EG(ret, err1);
				}
			}
err1:
			nn_uninit(&curve_order_square); EG(ret, err);
		}

		ret = nn_bitlen(&m_msb_fixed, &mlen); EG(ret, err);
		MUST_HAVE((mlen != 0), ret, err);
		mlen--;

		{
			prj_pt dbl;
			dbl.magic = WORD(0);

			/* Initialize temporary point */
			ret = prj_pt_init(&dbl, in->crv); EG(ret, err2);

			/* Main loop of Double and Add Always */
			while (mlen > 0) {
				--mlen;
				/* mbit is m[i] */
				ret = nn_getbit(&m_msb_fixed, mlen, &mbit); EG(ret, err2);

#ifndef NO_USE_COMPLETE_FORMULAS
				/*
				 * NOTE: in case of complete formulas, we use the
				 * addition for doubling, incurring a small performance hit
				 * for better SCA resistance.
				 */
				ret_ops |= prj_pt_add(&dbl, out, out);
#else
				ret_ops |= prj_pt_dbl(&dbl, out);
#endif
				ret_ops |= prj_pt_add(out, &dbl, in);
				/* Swap */
				ret = nn_cnd_swap(!mbit, &(out->X.fp_val), &(dbl.X.fp_val)); EG(ret, err2);
				ret = nn_cnd_swap(!mbit, &(out->Y.fp_val), &(dbl.Y.fp_val)); EG(ret, err2);
				ret = nn_cnd_swap(!mbit, &(out->Z.fp_val), &(dbl.Z.fp_val)); EG(ret, err2);
			}
err2:
			prj_pt_uninit(&dbl); EG(ret, err);
		}

err:
		nn_uninit(&m_msb_fixed);

		PTR_NULLIFY(curve_order);
	}

	/* Take into consideration our double and add errors */
	ret |= ret_ops;

	return ret;
}
#endif


#ifdef USE_MONTY_LADDER
ATTRIBUTE_WARN_UNUSED_RET static int _prj_pt_mul_ltr_monty_ladder(prj_pt_t out, nn_src_t m, prj_pt_src_t in)
{
	/* We use Itoh et al. notations here for T and the random r */
	prj_pt T[3];
	bitcnt_t mlen;
	u8 mbit, rbit;
	/* Random for masking the Montgomery Ladder algorithm */
	nn r;
	/* The new scalar we will use with MSB fixed to 1 (noted m' above).
	 * This helps dealing with constant time.
	 */
	nn m_msb_fixed;
	nn_src_t curve_order;
	nn curve_order_square;
	int ret, ret_ops, cmp;
	r.magic = m_msb_fixed.magic = curve_order_square.magic = WORD(0);
	T[0].magic = T[1].magic = T[2].magic = WORD(0);

	/* Compute m' from m depending on the rule described above */
	curve_order = &(in->crv->order);

	/* First compute q**2 */
	ret = nn_sqr(&curve_order_square, curve_order); EG(ret, err);

	/* Then compute m' depending on m size */
	ret = nn_cmp(m, curve_order, &cmp); EG(ret, err);
	if (cmp < 0) {
		bitcnt_t msb_bit_len, order_bitlen;

		/* Case where m < q */
		ret = nn_add(&m_msb_fixed, m, curve_order); EG(ret, err);
		ret = nn_bitlen(&m_msb_fixed, &msb_bit_len); EG(ret, err);
		ret = nn_bitlen(curve_order, &order_bitlen); EG(ret, err);
		ret = nn_cnd_add((msb_bit_len == order_bitlen), &m_msb_fixed,
				&m_msb_fixed, curve_order); EG(ret, err);
	} else {
		ret = nn_cmp(m, &curve_order_square, &cmp); EG(ret, err);
		if (cmp < 0) {
			bitcnt_t msb_bit_len, curve_order_square_bitlen;

			/* Case where m >= q and m < (q**2) */
			ret = nn_add(&m_msb_fixed, m, &curve_order_square); EG(ret, err);
			ret = nn_bitlen(&m_msb_fixed, &msb_bit_len); EG(ret, err);
			ret = nn_bitlen(&curve_order_square, &curve_order_square_bitlen); EG(ret, err);
			ret = nn_cnd_add((msb_bit_len == curve_order_square_bitlen),
					 &m_msb_fixed, &m_msb_fixed, &curve_order_square); EG(ret, err);
		} else {
			/* Case where m >= (q**2) */
			ret = nn_copy(&m_msb_fixed, m); EG(ret, err);
		}
	}

	ret = nn_bitlen(&m_msb_fixed, &mlen); EG(ret, err);
	MUST_HAVE((mlen != 0), ret, err);
	mlen--;

	/* Hide possible internal failures for double and add
	 * operations and perform the operation in constant time.
	 */
	ret_ops = 0;

	/* Get a random r with the same size of m_msb_fixed */
	ret = nn_get_random_len(&r, (u16)(m_msb_fixed.wlen * WORD_BYTES)); EG(ret, err);

	ret = nn_getbit(&r, mlen, &rbit); EG(ret, err);

	/* Initialize points */
	ret = prj_pt_init(&T[0], in->crv); EG(ret, err);
	ret = prj_pt_init(&T[1], in->crv); EG(ret, err);
	ret = prj_pt_init(&T[2], in->crv); EG(ret, err);

	/* Initialize T[r[n-1]] to input point */
	/*
	 * Blind the point with projective coordinates
	 * (X, Y, Z) => (l*X, l*Y, l*Z)
	 */
	ret = _blind_projective_point(&T[rbit], in); EG(ret, err);

	/* Initialize T[1-r[n-1]] with ECDBL(T[r[n-1]])) */
#ifndef NO_USE_COMPLETE_FORMULAS
	/*
	 * NOTE: in case of complete formulas, we use the
	 * addition for doubling, incurring a small performance hit
	 * for better SCA resistance.
	 */
	ret_ops |= prj_pt_add(&T[1-rbit], &T[rbit], &T[rbit]);
#else
	ret_ops |= prj_pt_dbl(&T[1-rbit], &T[rbit]);
#endif

	/* Main loop of the Montgomery Ladder */
	while (mlen > 0) {
		u8 rbit_next;
		--mlen;
		/* rbit is r[i+1], and rbit_next is r[i] */
		ret = nn_getbit(&r, mlen, &rbit_next); EG(ret, err);

		/* mbit is m[i] */
		ret = nn_getbit(&m_msb_fixed, mlen, &mbit); EG(ret, err);
		/* Double: T[2] = ECDBL(T[d[i] ^ r[i+1]]) */

#ifndef NO_USE_COMPLETE_FORMULAS
		/* NOTE: in case of complete formulas, we use the
		 * addition for doubling, incurring a small performance hit
		 * for better SCA resistance.
		 */
		ret_ops |= prj_pt_add(&T[2], &T[mbit ^ rbit], &T[mbit ^ rbit]);
#else
		ret_ops |= prj_pt_dbl(&T[2], &T[mbit ^ rbit]);
#endif

		/* Add: T[1] = ECADD(T[0],T[1]) */
		ret_ops |= prj_pt_add(&T[1], &T[0], &T[1]);

		/* T[0] = T[2-(d[i] ^ r[i])] */
		/*
		 * NOTE: we use the low level nn_copy function here to avoid
		 * any possible leakage on operands with prj_pt_copy
		 */
		ret = nn_copy(&(T[0].X.fp_val), &(T[2-(mbit ^ rbit_next)].X.fp_val)); EG(ret, err);
		ret = nn_copy(&(T[0].Y.fp_val), &(T[2-(mbit ^ rbit_next)].Y.fp_val)); EG(ret, err);
		ret = nn_copy(&(T[0].Z.fp_val), &(T[2-(mbit ^ rbit_next)].Z.fp_val)); EG(ret, err);

		/* T[1] = T[1+(d[i] ^ r[i])] */
		/* NOTE: we use the low level nn_copy function here to avoid
		 * any possible leakage on operands with prj_pt_copy
		 */
		ret = nn_copy(&(T[1].X.fp_val), &(T[1+(mbit ^ rbit_next)].X.fp_val)); EG(ret, err);
		ret = nn_copy(&(T[1].Y.fp_val), &(T[1+(mbit ^ rbit_next)].Y.fp_val)); EG(ret, err);
		ret = nn_copy(&(T[1].Z.fp_val), &(T[1+(mbit ^ rbit_next)].Z.fp_val)); EG(ret, err);

		/* Update rbit */
		rbit = rbit_next;
	}
	/* Output: T[r[0]] */
	ret = prj_pt_copy(out, &T[rbit]); EG(ret, err);

	/* Take into consideration our double and add errors */
	ret |= ret_ops;

err:
	prj_pt_uninit(&T[0]);
	prj_pt_uninit(&T[1]);
	prj_pt_uninit(&T[2]);
	nn_uninit(&r);
	nn_uninit(&m_msb_fixed);
	nn_uninit(&curve_order_square);

	PTR_NULLIFY(curve_order);

	return ret;
}
#endif

/* Main projective scalar multiplication function.
 * Depending on the preprocessing options, we use either the
 * Double and Add Always algorithm, or the Montgomery Ladder one.
 */
ATTRIBUTE_WARN_UNUSED_RET static int _prj_pt_mul_ltr_monty(prj_pt_t out, nn_src_t m, prj_pt_src_t in){
#if defined(USE_DOUBLE_ADD_ALWAYS)
	return _prj_pt_mul_ltr_monty_dbl_add_always(out, m, in);
#elif defined(USE_MONTY_LADDER)
	return _prj_pt_mul_ltr_monty_ladder(out, m, in);
#else
#error "Error: neither Double and Add Always nor Montgomery Ladder has been selected!"
#endif
}

/* version with 'm' passed via 'out'. */
ATTRIBUTE_WARN_UNUSED_RET static int _prj_pt_mul_ltr_monty_aliased(prj_pt_t out, nn_src_t m, prj_pt_src_t in)
{
	prj_pt out_cpy;
	int ret;
	out_cpy.magic = WORD(0);

	ret = prj_pt_init(&out_cpy, in->crv); EG(ret, err);
	ret = _prj_pt_mul_ltr_monty(&out_cpy, m, in); EG(ret, err);
	ret = prj_pt_copy(out, &out_cpy);

err:
	prj_pt_uninit(&out_cpy);
	return ret;
}

/* Aliased version. This is the public main interface of our
 * scalar multiplication algorithm. Checks that the input point
 * and that the output point are on the curve are performed here
 * (before and after calling the core algorithm, albeit Double and
 * Add Always or Montgomery Ladder).
 */
int prj_pt_mul(prj_pt_t out, nn_src_t m, prj_pt_src_t in)
{
	int ret, on_curve;

	ret = prj_pt_check_initialized(in); EG(ret, err);
	ret = nn_check_initialized(m); EG(ret, err);

	/* Check that the input is on the curve */
	MUST_HAVE((!prj_pt_is_on_curve(in, &on_curve)) && on_curve, ret, err);

	if (out == in) {
		ret = _prj_pt_mul_ltr_monty_aliased(out, m, in); EG(ret, err);
	} else {
		ret = _prj_pt_mul_ltr_monty(out, m, in); EG(ret, err);
	}

	/* Check that the output is on the curve */
	MUST_HAVE((!prj_pt_is_on_curve(out, &on_curve)) && on_curve, ret, err);

err:
	return ret;
}

int prj_pt_mul_blind(prj_pt_t out, nn_src_t m, prj_pt_src_t in)
{
	/* Blind the scalar m with (b*q) where q is the curve order.
	 * NOTE: the curve order and the "generator" order are
	 * usually the same (i.e. cofactor = 1) for the classical
	 * prime fields curves. However some exceptions exist
	 * (e.g. Wei25519 and Wei448), and in this case it is
	 * curcial to use the curve order for a generic blinding
	 * working on any point on the curve.
	 */
	nn b;
	nn_src_t q;
	int ret;
	b.magic = WORD(0);

	ret = prj_pt_check_initialized(in); EG(ret, err);

	q = &(in->crv->order);

	ret = nn_init(&b, 0); EG(ret, err);

	ret = nn_get_random_mod(&b, q); EG(ret, err);

	ret = nn_mul(&b, &b, q); EG(ret, err);
	ret = nn_add(&b, &b, m); EG(ret, err);

	/* NOTE: point blinding is performed in the lower functions */
	/* NOTE: check that input and output points are on the curve are
	 * performed in the lower functions.
	 */

	/* Perform the scalar multiplication */
	ret = prj_pt_mul(out, &b, in);

err:
	nn_uninit(&b);

	PTR_NULLIFY(q);

	return ret;
}

/* Naive double and add scalar multiplication.
 *
 * This scalar multiplication is used on public values and is optimized with no
 * countermeasures, and it is usually faster as scalar can be small with few bits
 * to process (e.g. cofactors, etc.).
 *
 * out is initialized by the function.
 *
 * XXX: WARNING: this function must only be used on public points!
 *
 */
static int __prj_pt_unprotected_mult(prj_pt_t out, nn_src_t scalar, prj_pt_src_t public_in)
{
        u8 expbit;
        bitcnt_t explen;
        int ret, iszero, on_curve;

        ret = prj_pt_check_initialized(public_in); EG(ret, err);
        ret = nn_check_initialized(scalar); EG(ret, err);

	/* This function does not support aliasing */
	MUST_HAVE((out != public_in), ret, err);

	/* Check that the input is on the curve */
	MUST_HAVE((!prj_pt_is_on_curve(public_in, &on_curve)) && on_curve, ret, err);

        ret = nn_iszero(scalar, &iszero); EG(ret, err);
	/* Multiplication by zero is the point at infinity */
	if(iszero){
		ret = prj_pt_zero(out); EG(ret, err);
		goto err;
	}

        ret = nn_bitlen(scalar, &explen); EG(ret, err);
        /* Sanity check */
        MUST_HAVE((explen > 0), ret, err);
        explen = (bitcnt_t)(explen - 1);
	ret = prj_pt_copy(out, public_in); EG(ret, err);

        while (explen > 0) {
                explen = (bitcnt_t)(explen - 1);
                ret = nn_getbit(scalar, explen, &expbit); EG(ret, err);
                ret = prj_pt_dbl(out, out); EG(ret, err);
                if(expbit){
                        ret = prj_pt_add(out, out, public_in); EG(ret, err);
                }
        }

	/* Check that the output is on the curve */
	MUST_HAVE((!prj_pt_is_on_curve(out, &on_curve)) && on_curve, ret, err);

err:
        VAR_ZEROIFY(expbit);
        VAR_ZEROIFY(explen);

        return ret;
}

/* Aliased version of __prj_pt_unprotected_mult */
int _prj_pt_unprotected_mult(prj_pt_t out, nn_src_t scalar, prj_pt_src_t public_in)
{
	int ret;

	if(out == public_in){
                prj_pt A;
                A.magic = WORD(0);

                ret = prj_pt_copy(&A, public_in); EG(ret, err1);
		ret = __prj_pt_unprotected_mult(out, scalar, &A);
err1:
		prj_pt_uninit(&A);
		goto err;
	}
	else{
		ret = __prj_pt_unprotected_mult(out, scalar, public_in);
	}
err:
	return ret;
}
/*
 * Check if an integer is (a multiple of) a projective point order.
 *
 * The function returns 0 on success, -1 on error. The value check is set to 1 if the projective
 * point has order in_isorder, 0 otherwise. The value is meaningless on error.
 */
int check_prj_pt_order(prj_pt_src_t in_shortw, nn_src_t in_isorder, prj_pt_sensitivity s, int *check)
{
	int ret, iszero;
	prj_pt res;
	res.magic = WORD(0);

	/* First sanity checks */
	ret = prj_pt_check_initialized(in_shortw); EG(ret, err);
	ret = nn_check_initialized(in_isorder); EG(ret, err);
	MUST_HAVE((check != NULL), ret, err);

	/* Then, perform the scalar multiplication */
	if(s == PUBLIC_PT){
		/* If this is a public point, we can use the naive scalar multiplication */
		ret = _prj_pt_unprotected_mult(&res, in_isorder, in_shortw); EG(ret, err);
	}
	else{
		/* If the point is private, it is sensitive and we proceed with the secure
		 * scalar blind multiplication.
		 */
		ret = prj_pt_mul_blind(&res, in_isorder, in_shortw); EG(ret, err);
	}

	/* Check if we have the point at infinity */
	ret = prj_pt_iszero(&res, &iszero); EG(ret, err);
	(*check) = iszero;

err:
	prj_pt_uninit(&res);

	return ret;
}

/*****************************************************************************/

/*
 * Map points from Edwards to short Weierstrass projective points through Montgomery (composition mapping).
 *     Point at infinity (0, 1) -> (0, 1, 0) is treated as an exception, which is trivially not constant time.
 *     This is OK since our mapping functions should be used at the non sensitive input and output
 *     interfaces.
 *
 * The function returns 0 on success, -1 on error.
 */
int aff_pt_edwards_to_prj_pt_shortw(aff_pt_edwards_src_t in_edwards,
				    ec_shortw_crv_src_t shortw_crv,
				    prj_pt_t out_shortw,
				    fp_src_t alpha_edwards)
{
	int ret, iszero, cmp;
	aff_pt out_shortw_aff;
	fp one;
	out_shortw_aff.magic = one.magic = WORD(0);

	/* Check the curves compatibility */
	ret = aff_pt_edwards_check_initialized(in_edwards); EG(ret, err);
	ret = curve_edwards_shortw_check(in_edwards->crv, shortw_crv, alpha_edwards); EG(ret, err);

	/* Initialize output point with curve */
	ret = prj_pt_init(out_shortw, shortw_crv); EG(ret, err);

	ret = fp_init(&one, in_edwards->x.ctx); EG(ret, err);
	ret = fp_one(&one); EG(ret, err);

	/* Check if we are the point at infinity
	 * This check induces a non consant time exception, but the current function must be called on
	 * public data anyways.
	 */
	ret = fp_iszero(&(in_edwards->x), &iszero); EG(ret, err);
	ret = fp_cmp(&(in_edwards->y), &one, &cmp); EG(ret, err);
	if(iszero && (cmp == 0)){
		ret = prj_pt_zero(out_shortw); EG(ret, err);
		ret = 0;
		goto err;
	}

	/* Use the affine mapping */
	ret = aff_pt_edwards_to_shortw(in_edwards, shortw_crv, &out_shortw_aff, alpha_edwards); EG(ret, err);
	/* And then map the short Weierstrass affine to projective coordinates */
	ret = ec_shortw_aff_to_prj(out_shortw, &out_shortw_aff);

err:
	fp_uninit(&one);
	aff_pt_uninit(&out_shortw_aff);

	return ret;
}

/*
 * Map points from short Weierstrass projective points to Edwards through Montgomery (composition mapping).
 *     Point at infinity with Z=0 (in projective coordinates) -> (0, 1) is treated as an exception, which is trivially not constant time.
 *     This is OK since our mapping functions should be used at the non sensitive input and output
 *     interfaces.
 *
 * The function returns 0 on success, -1 on error.
 */
int prj_pt_shortw_to_aff_pt_edwards(prj_pt_src_t in_shortw,
				    ec_edwards_crv_src_t edwards_crv,
				    aff_pt_edwards_t out_edwards,
				    fp_src_t alpha_edwards)
{
	int ret, iszero;
	aff_pt in_shortw_aff;
	in_shortw_aff.magic = WORD(0);

	/* Check the curves compatibility */
	ret = prj_pt_check_initialized(in_shortw); EG(ret, err);
	ret = curve_edwards_shortw_check(edwards_crv, in_shortw->crv, alpha_edwards); EG(ret, err);

	/* Initialize output point with curve */
	ret = aff_pt_init(&in_shortw_aff, in_shortw->crv); EG(ret, err);

	/* Check if we are the point at infinity
	 * This check induces a non consant time exception, but the current function must be called on
	 * public data anyways.
	 */
	ret = prj_pt_iszero(in_shortw, &iszero); EG(ret, err);
	if(iszero){
		fp zero, one;
		zero.magic = one.magic = WORD(0);

		ret = fp_init(&zero, in_shortw->X.ctx); EG(ret, err1);
		ret = fp_init(&one, in_shortw->X.ctx); EG(ret, err1);

		ret = fp_zero(&zero); EG(ret, err1);
		ret = fp_one(&one); EG(ret, err1);

		ret = aff_pt_edwards_init_from_coords(out_edwards, edwards_crv, &zero, &one);

err1:
		fp_uninit(&zero);
		fp_uninit(&one);

		goto err;
	}

	/* Map projective to affine on the short Weierstrass */
	ret = prj_pt_to_aff(&in_shortw_aff, in_shortw); EG(ret, err);
	/* Use the affine mapping */
	ret = aff_pt_shortw_to_edwards(&in_shortw_aff, edwards_crv, out_edwards, alpha_edwards);

err:
	aff_pt_uninit(&in_shortw_aff);

	return ret;
}

/*
 * Map points from Montgomery to short Weierstrass projective points.
 *
 * The function returns 0 on success, -1 on error.
 */
int aff_pt_montgomery_to_prj_pt_shortw(aff_pt_montgomery_src_t in_montgomery,
				       ec_shortw_crv_src_t shortw_crv,
				       prj_pt_t out_shortw)
{
	int ret;
	aff_pt out_shortw_aff;
	out_shortw_aff.magic = WORD(0);

	/* Check the curves compatibility */
	ret = aff_pt_montgomery_check_initialized(in_montgomery); EG(ret, err);
	ret = curve_montgomery_shortw_check(in_montgomery->crv, shortw_crv); EG(ret, err);

	/* Initialize output point with curve */
	ret = prj_pt_init(out_shortw, shortw_crv); EG(ret, err);

	/* Use the affine mapping */
	ret = aff_pt_montgomery_to_shortw(in_montgomery, shortw_crv, &out_shortw_aff); EG(ret, err);
	/* And then map the short Weierstrass affine to projective coordinates */
	ret = ec_shortw_aff_to_prj(out_shortw, &out_shortw_aff);

err:
	aff_pt_uninit(&out_shortw_aff);

	return ret;
}

/*
 * Map points from short Weierstrass projective points to Montgomery.
 *
 * The function returns 0 on success, -1 on error.
 */
int prj_pt_shortw_to_aff_pt_montgomery(prj_pt_src_t in_shortw, ec_montgomery_crv_src_t montgomery_crv, aff_pt_montgomery_t out_montgomery)
{
	int ret;
	aff_pt in_shortw_aff;
	in_shortw_aff.magic = WORD(0);

	/* Check the curves compatibility */
	ret = prj_pt_check_initialized(in_shortw); EG(ret, err);
	ret = curve_montgomery_shortw_check(montgomery_crv, in_shortw->crv); EG(ret, err);

	/* Initialize output point with curve */
	ret = aff_pt_init(&in_shortw_aff, in_shortw->crv); EG(ret, err);

	/* Map projective to affine on the short Weierstrass */
	ret = prj_pt_to_aff(&in_shortw_aff, in_shortw); EG(ret, err);
	/* Use the affine mapping */
	ret = aff_pt_shortw_to_montgomery(&in_shortw_aff, montgomery_crv, out_montgomery);

err:
	aff_pt_uninit(&in_shortw_aff);

	return ret;
}
