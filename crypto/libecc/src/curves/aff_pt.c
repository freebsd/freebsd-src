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
#include <libecc/curves/aff_pt.h>

#define AFF_PT_MAGIC ((word_t)(0x4c82a9bcd0d9ffabULL))

/*
 * Verify that an affine point has already been initialized. Return 0 on
 * success, -1 otherwise.
 */
int aff_pt_check_initialized(aff_pt_src_t in)
{
	int ret;

	MUST_HAVE(((in != NULL) && (in->magic == AFF_PT_MAGIC)), ret, err);
	ret = ec_shortw_crv_check_initialized(in->crv);

err:
	return ret;
}

/*
 * Initialize pointed aff_pt structure to make it usable by library
 * function on given curve. Return 0 on success, -1 on error.
 */
int aff_pt_init(aff_pt_t in, ec_shortw_crv_src_t curve)
{
	int ret;

	MUST_HAVE((in != NULL), ret, err);
	MUST_HAVE((curve != NULL), ret, err);

	ret = ec_shortw_crv_check_initialized(curve); EG(ret, err);
	ret = fp_init(&(in->x), curve->a.ctx); EG(ret, err);
	ret = fp_init(&(in->y), curve->a.ctx); EG(ret, err);

	in->crv = curve;
	in->magic = AFF_PT_MAGIC;

err:
	return ret;
}

/*
 * Initialize given point 'in' on given curve 'curve' and set its coordinates to
 * 'xcoord' and 'ycoord'. Return 0 on success, -1 on error.
 */
int aff_pt_init_from_coords(aff_pt_t in,
			    ec_shortw_crv_src_t curve,
			    fp_src_t xcoord, fp_src_t ycoord)
{
	int ret;

	ret = aff_pt_init(in, curve); EG(ret, err);
	ret = fp_copy(&(in->x), xcoord); EG(ret, err);
	ret = fp_copy(&(in->y), ycoord);

err:
	return ret;
}

/*
 * Uninitialize pointed affine point 'in' to prevent further use (magic field
 * in the structure is zeroized) and zeroize associated storage space. Note
 * that the curve context pointed to by the point element (passed during init)
 * is left untouched.
 */
void aff_pt_uninit(aff_pt_t in)
{
	if((in != NULL) && (in->magic == AFF_PT_MAGIC) && (in->crv != NULL)){
		in->crv = NULL;
		in->magic = WORD(0);

		fp_uninit(&(in->x));
		fp_uninit(&(in->y));
	}

	return;
}

/*
 * Recover the two possible y coordinates from one x on a given
 * curve.
 * The two outputs y1 and y2 are initialized in the function.
 *
 * The function returns -1 on error, 0 on success.
 *
 */
int aff_pt_y_from_x(fp_t y1, fp_t y2, fp_src_t x, ec_shortw_crv_src_t curve)
{
	int ret;

	MUST_HAVE((y1 != NULL) && (y2 != NULL), ret, err);
	ret = ec_shortw_crv_check_initialized(curve); EG(ret, err);
	ret = fp_check_initialized(x);  EG(ret, err);
	/* Aliasing is not supported */
	MUST_HAVE((y1 != y2) && (y1 != x), ret, err);


	/* Initialize our elements */
	ret = fp_copy(y1, x); EG(ret, err);
	ret = fp_copy(y2, x); EG(ret, err);

	/* Compute x^3 + ax + b */
	ret = fp_sqr(y1, y1); EG(ret, err);
	ret = fp_mul(y1, y1, x); EG(ret, err);
	ret = fp_mul(y2, y2, &(curve->a)); EG(ret, err);
	ret = fp_add(y1, y1, y2); EG(ret, err);
	ret = fp_add(y1, y1,  &(curve->b)); EG(ret, err);

	/* Now compute the two possible square roots
	 * realizing y^2 = x^3 + ax + b
	 */
	ret = fp_sqrt(y1, y2, y1);

err:
	return ret;
}

/*
 * Check if given point of coordinate ('x', 'y') is on given curve 'curve' (i.e.
 * if it verifies curve equation y^2 = x^3 + ax + b). On success, the verdict is
 * given using 'on_curve' out parameter (1 if on curve, 0 if not). On error,
 * the function returns -1 and 'on_curve' is left unmodified.
 */
int is_on_shortw_curve(fp_src_t x, fp_src_t y, ec_shortw_crv_src_t curve, int *on_curve)
{
	fp tmp1, tmp2;
	int ret, cmp;
	tmp1.magic = tmp2.magic = WORD(0);

	ret = ec_shortw_crv_check_initialized(curve); EG(ret, err);
	ret = fp_check_initialized(x);  EG(ret, err);
	ret = fp_check_initialized(y);  EG(ret, err);
	MUST_HAVE((on_curve != NULL), ret, err);

	MUST_HAVE((x->ctx == y->ctx), ret, err);
	MUST_HAVE((x->ctx == curve->a.ctx), ret, err);

	/* Note: to optimize local variables, we instead check that
	 * (y^2 - b) = (x^2 + a) * x
	 */

	/* Compute y^2 - b */
	ret = fp_init(&tmp1, x->ctx); EG(ret, err);
	ret = fp_sqr(&tmp1, y); EG(ret, err);
	ret = fp_sub(&tmp1, &tmp1, &(curve->b)); EG(ret, err);

	/* Compute (x^2 + a) * x */
	ret = fp_init(&tmp2, x->ctx); EG(ret, err);
	ret = fp_sqr(&tmp2, x); EG(ret, err);
	ret = fp_add(&tmp2, &tmp2, &(curve->a)); EG(ret, err);
	ret = fp_mul(&tmp2, &tmp2, x); EG(ret, err);

	/* Now check*/
	ret = fp_cmp(&tmp1, &tmp2, &cmp); EG(ret, err);

	(*on_curve) = (!cmp);

err:
	fp_uninit(&tmp1);
        fp_uninit(&tmp2);

        return ret;
}

/*
 * Same as previous but using an affine point instead of pair of coordinates
 * and a curve
 */
int aff_pt_is_on_curve(aff_pt_src_t pt, int *on_curve)
{
	int ret;

	MUST_HAVE((on_curve != NULL), ret, err);
	ret = aff_pt_check_initialized(pt); EG(ret, err);
	ret = is_on_shortw_curve(&(pt->x), &(pt->y), pt->crv, on_curve);

err:
	return ret;
}

/*
 * Copy 'in' affine point into 'out'. 'out' is initialized by the function.
 * 0 is returned on success, -1 on error.
 */
int ec_shortw_aff_copy(aff_pt_t out, aff_pt_src_t in)
{
	int ret;

	ret = aff_pt_check_initialized(in); EG(ret, err);
	ret = aff_pt_init(out, in->crv); EG(ret, err);
	ret = fp_copy(&(out->x), &(in->x)); EG(ret, err);
	ret = fp_copy(&(out->y), &(in->y));

err:
	return ret;
}

/*
 * Compare affine points 'in1' and 'in2'. On success, 0 is returned and
 * comparison value is given using 'cmp' (0 if equal, a non-zero value
 * if they are different). -1 is returned on error.
 */
int ec_shortw_aff_cmp(aff_pt_src_t in1, aff_pt_src_t in2, int *cmp)
{
	int ret, cmp_x, cmp_y;

	MUST_HAVE((cmp != NULL), ret, err);

	ret = aff_pt_check_initialized(in1); EG(ret, err);
	ret = aff_pt_check_initialized(in2); EG(ret, err);

	MUST_HAVE((in1->crv == in2->crv), ret, err);

	ret = fp_cmp(&(in1->x), &(in2->x), &cmp_x); EG(ret, err);
	ret = fp_cmp(&(in1->y), &(in2->y), &cmp_y); EG(ret, err);

	(*cmp) = (cmp_x | cmp_y);

err:
	return ret;
}

/*
 * Check if given affine points 'in1' and 'in2' on the same curve are equal
 * or opposite. On success, 0 is returned and 'aff_is_eq_or_opp' contains:
 *  - 1 if points are equal or opposite
 *  - 0 if not
 * The function returns -1 on error, in which case 'aff_is_eq_or_opp'
 * is left untouched.
 */
int ec_shortw_aff_eq_or_opp(aff_pt_src_t in1, aff_pt_src_t in2,
			    int *aff_is_eq_or_opp)
{
	int ret, cmp, eq_or_opp;

	ret = aff_pt_check_initialized(in1); EG(ret, err);
	ret = aff_pt_check_initialized(in2); EG(ret, err);
	MUST_HAVE((in1->crv == in2->crv), ret, err);
	MUST_HAVE((aff_is_eq_or_opp != NULL), ret, err);

	ret = fp_cmp(&(in1->x), &(in2->x), &cmp); EG(ret, err);
	ret = fp_eq_or_opp(&(in1->y), &(in2->y), &eq_or_opp); EG(ret, err);

	(*aff_is_eq_or_opp) = ((cmp == 0) & eq_or_opp);

err:
	return ret;
}

/*
 * Import an affine point from a buffer with the following layout; the 2
 * coordinates (elements of Fp) are each encoded on p_len bytes, where p_len
 * is the size of p in bytes (e.g. 66 for a prime p of 521 bits). Each
 * coordinate is encoded in big endian. Size of buffer must exactly match
 * 2 * p_len. The function returns 0 on success, -1 on error.
 */
int aff_pt_import_from_buf(aff_pt_t pt,
			   const u8 *pt_buf,
			   u16 pt_buf_len, ec_shortw_crv_src_t crv)
{
	fp_ctx_src_t ctx;
	u16 coord_len;
	int ret, on_curve;

	MUST_HAVE((pt_buf != NULL), ret, err);
	MUST_HAVE((pt != NULL), ret, err);
	ret = ec_shortw_crv_check_initialized(crv); EG(ret, err);

	ctx = crv->a.ctx;
	coord_len = (u16)BYTECEIL(ctx->p_bitlen);

	MUST_HAVE((pt_buf_len == (2 * coord_len)), ret, err);

	ret = fp_init_from_buf(&(pt->x), ctx, pt_buf, coord_len); EG(ret, err);
	ret = fp_init_from_buf(&(pt->y), ctx, pt_buf + coord_len, coord_len); EG(ret, err);

	/* Set the curve */
	pt->crv = crv;

	/* Mark the point as initialized */
	pt->magic = AFF_PT_MAGIC;

	/*
	 * Check that the point is indeed on provided curve, uninitialize it
	 * if this is not the case.
	 */
	ret = aff_pt_is_on_curve(pt, &on_curve); EG(ret, err);

	if (!on_curve) {
		aff_pt_uninit(pt);
		ret = -1;
	} else {
		ret = 0;
	}

err:
	PTR_NULLIFY(ctx);

	return ret;
}


/*
 * Export an affine point 'pt' to a buffer with the following layout; the 2
 * coordinates (elements of Fp) are each encoded on p_len bytes, where p_len
 * is the size of p in bytes (e.g. 66 for a prime p of 521 bits). Each
 * coordinate is encoded in big endian. Size of buffer must exactly match
 * 2 * p_len.
 */
int aff_pt_export_to_buf(aff_pt_src_t pt, u8 *pt_buf, u32 pt_buf_len)
{
	u16 coord_len;
	int ret, on_curve;

	MUST_HAVE((pt_buf != NULL), ret, err);

	/* The point to be exported must be on the curve */
	ret = aff_pt_is_on_curve(pt, &on_curve); EG(ret, err);
	MUST_HAVE((on_curve), ret, err);

	/* buffer size must match 2 * p_len */
	coord_len = (u16)BYTECEIL(pt->crv->a.ctx->p_bitlen);
	MUST_HAVE((pt_buf_len == (2 * coord_len)), ret, err);

	/* Export the two coordinates */
	ret = fp_export_to_buf(pt_buf, coord_len, &(pt->x)); EG(ret, err);
	ret = fp_export_to_buf(pt_buf + coord_len, coord_len, &(pt->y));

err:
	return ret;
}
