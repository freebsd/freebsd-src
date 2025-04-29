/*
 *  Copyright (C) 2021 - This file is part of libecc project
 *
 *  Authors:
 *      Ryad BENADJILA <ryadbenadjila@gmail.com>
 *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */
#include <libecc/curves/aff_pt.h>

/* NOTE: Edwards here implies Twisted Edwards curves
 * (these in fact include/extend basic form Edwards curves).
 */

#define AFF_PT_EDWARDS_MAGIC ((word_t)(0x8390a9bc43d9ffabULL))

/* Verify that an affine point has already been initialized
 *
 * Returns 0 on success, -1 on error.
 */
int aff_pt_edwards_check_initialized(aff_pt_edwards_src_t in)
{
	int ret;

	MUST_HAVE(((in != NULL) && (in->magic == AFF_PT_EDWARDS_MAGIC)), ret, err);
	ret = ec_edwards_crv_check_initialized(in->crv);

err:
	return ret;
}

/*
 * Initialize pointed aff_pt_edwards structure to make it usable by library
 * function on given curve.
 *
 * Returns 0 on success, -1 on error.
 */
int aff_pt_edwards_init(aff_pt_edwards_t in, ec_edwards_crv_src_t curve)
{
	int ret;

	MUST_HAVE((in != NULL), ret, err);
	ret = ec_edwards_crv_check_initialized(curve); EG(ret, err);

	ret = fp_init(&(in->x), curve->a.ctx); EG(ret, err);
	ret = fp_init(&(in->y), curve->a.ctx); EG(ret, err);

	in->crv = curve;
	in->magic = AFF_PT_EDWARDS_MAGIC;

err:
	return ret;
}

/*
 * Initialize pointed aff_pt_edwards structure to make it usable by library
 * function on given curve with explicit coordinates.
 *
 * Returns 0 on success, -1 on error.
 */
int aff_pt_edwards_init_from_coords(aff_pt_edwards_t in,
			     ec_edwards_crv_src_t curve,
			     fp_src_t xcoord, fp_src_t ycoord)
{
	int ret;

	ret = aff_pt_edwards_init(in, curve); EG(ret, err);
	ret = fp_copy(&(in->x), xcoord); EG(ret, err);
	ret = fp_copy(&(in->y), ycoord);

err:
	return ret;
}

/*
 * Uninitialize pointed affine point to prevent further use (magic field
 * in the structure is zeroized) and zeroize associated storage space.
 * Note that the curve context pointed to by the point element (passed
 * during init) is left untouched.
 *
 */
void aff_pt_edwards_uninit(aff_pt_edwards_t in)
{
	if ((in != NULL) && (in->magic == AFF_PT_EDWARDS_MAGIC) && (in->crv != NULL)) {
		fp_uninit(&(in->x));
		fp_uninit(&(in->y));

		in->crv = NULL;
		in->magic = WORD(0);
	}

	return;
}

/*
 * 'on_curve' set to 1 if the point of coordinates (u,v) is on the curve, i.e. if it
 * verifies curve equation a*x^2 + y^2 = 1 + d*x^2*y^2. It is set to 0 otherwise.
 * 'on_curve' is not meaningful on error.
 *
 * Returns 0 on success, -1 on error.
 */
int is_on_edwards_curve(fp_src_t x, fp_src_t y,
			ec_edwards_crv_src_t curve,
			int *on_curve)
{
	fp x2, y2, tmp1, tmp2;
	int ret, cmp;
	x2.magic = y2.magic = tmp1.magic = tmp2.magic = WORD(0);

	MUST_HAVE((on_curve != NULL), ret, err);
	ret = ec_edwards_crv_check_initialized(curve); EG(ret, err);

	ret = fp_check_initialized(x); EG(ret, err);
	ret = fp_check_initialized(y); EG(ret, err);

	MUST_HAVE((x->ctx == y->ctx), ret, err);
	MUST_HAVE((x->ctx == curve->a.ctx), ret, err);

	ret = fp_init(&x2, x->ctx); EG(ret, err);
	ret = fp_sqr(&x2, x); EG(ret, err);
	ret = fp_init(&y2, x->ctx); EG(ret, err);
	ret = fp_sqr(&y2, y); EG(ret, err);

	ret = fp_init(&tmp1, x->ctx); EG(ret, err);
	ret = fp_init(&tmp2, x->ctx); EG(ret, err);

	ret = fp_mul(&tmp1, &x2, &y2); EG(ret, err);
	ret = fp_mul(&tmp1, &tmp1, &(curve->d)); EG(ret, err);
	ret = fp_inc(&tmp1, &tmp1); EG(ret, err);

	ret = fp_mul(&tmp2, &x2, &(curve->a)); EG(ret, err);
	ret = fp_add(&tmp2, &tmp2, &y2); EG(ret, err);

	ret = fp_cmp(&tmp1, &tmp2, &cmp);

	if (!ret) {
		(*on_curve) = (!cmp);
	}

err:
	fp_uninit(&x2);
	fp_uninit(&y2);
	fp_uninit(&tmp1);
	fp_uninit(&tmp2);

	return ret;
}

/*
 * Checks if affine coordinates point is on an Edwards curve. 'on_curve' is set
 * to 1 if yes, 0 if no. 'on_curve' is not meaningful in case of error.
 *
 * Returns 0 on success, -1 on error.
 */
int aff_pt_edwards_is_on_curve(aff_pt_edwards_src_t pt, int *on_curve)
{
	int ret;

	ret = aff_pt_edwards_check_initialized(pt); EG(ret, err);

	ret = is_on_edwards_curve(&(pt->x), &(pt->y), pt->crv, on_curve);

err:
	return ret;
}

/*
 * Copy an Edwards affine point in an output. The output is initialized properly.
 *
 * Returns 0 on success, -1 on error.
 */
int ec_edwards_aff_copy(aff_pt_edwards_t out, aff_pt_edwards_src_t in)
{
	int ret;

	ret = aff_pt_edwards_check_initialized(in); EG(ret, err);
	ret = aff_pt_edwards_init(out, in->crv); EG(ret, err);

	ret = fp_copy(&(out->x), &(in->x)); EG(ret, err);
	ret = fp_copy(&(out->y), &(in->y));

err:
	return ret;
}

/*
 * Compares two given affine points on an Edwards curve, it returns 0 in input
 * 'cmp' if they correspond or not 0 if not. 'cmp' is not meaningful on error.
 *
 * Returns 0 on success, -1 on error.
 */
int ec_edwards_aff_cmp(aff_pt_edwards_src_t in1, aff_pt_edwards_src_t in2,
		       int *cmp)
{
	int ret, cmp1, cmp2;

	MUST_HAVE((cmp != NULL), ret, err);
	ret = aff_pt_edwards_check_initialized(in1); EG(ret, err);
	ret = aff_pt_edwards_check_initialized(in2); EG(ret, err);

	MUST_HAVE((in1->crv == in2->crv), ret, err);

	ret = fp_cmp(&(in1->x), &(in2->x), &cmp1); EG(ret, err);
	ret = fp_cmp(&(in1->y), &(in2->y), &cmp2);

	if (!ret) {
		(*cmp) = (cmp1 | cmp2);
	}

err:
	return ret;
}

/*
 * Import an Edwards affine point from a buffer with the following layout; the 2
 * coordinates (elements of Fp) are each encoded on p_len bytes, where p_len
 * is the size of p in bytes (e.g. 66 for a prime p of 521 bits). Each
 * coordinate is encoded in big endian. Size of buffer must exactly match
 * 2 * p_len.
 *
 * Returns 0 on success, -1 on error.
 */
int aff_pt_edwards_import_from_buf(aff_pt_edwards_t pt,
				   const u8 *pt_buf,
				   u16 pt_buf_len, ec_edwards_crv_src_t crv)
{
	fp_ctx_src_t ctx;
	u16 coord_len;
	int ret, on_curve;

	ret = ec_edwards_crv_check_initialized(crv); EG(ret, err);
	MUST_HAVE(((pt_buf != NULL) && (pt != NULL)), ret, err);

	ctx = crv->a.ctx;
	coord_len = (u16)BYTECEIL(ctx->p_bitlen);

	MUST_HAVE((pt_buf_len == (2 * coord_len)), ret, err);

	ret = fp_init_from_buf(&(pt->x), ctx, pt_buf, coord_len); EG(ret, err);
	ret = fp_init_from_buf(&(pt->y), ctx, pt_buf + coord_len, coord_len); EG(ret, err);

	/* Set the curve */
	pt->crv = crv;

	/* Mark the point as initialized */
	pt->magic = AFF_PT_EDWARDS_MAGIC;

	/* Check that the point is indeed on the provided curve, uninitialize it
	 * if this is not the case.
	 */
	ret = aff_pt_edwards_is_on_curve(pt, &on_curve); EG(ret, err);
	if (!on_curve) {
		aff_pt_edwards_uninit(pt);
		ret = -1;
	}

err:
	return ret;
}


/* Export an Edwards affine point to a buffer with the following layout; the 2
 * coordinates (elements of Fp) are each encoded on p_len bytes, where p_len
 * is the size of p in bytes (e.g. 66 for a prime p of 521 bits). Each
 * coordinate is encoded in big endian. Size of buffer must exactly match
 * 2 * p_len.
 *
 * Returns 0 on success, -1 on error.
 */
int aff_pt_edwards_export_to_buf(aff_pt_edwards_src_t pt,
				 u8 *pt_buf, u32 pt_buf_len)
{
	fp_ctx_src_t ctx;
	u16 coord_len;
	int ret, on_curve;

	ret = aff_pt_edwards_check_initialized(pt); EG(ret, err);
	MUST_HAVE((pt_buf != NULL), ret, err);

	/* The point to be exported must be on the curve */
	ret = aff_pt_edwards_is_on_curve(pt, &on_curve); EG(ret, err);
	MUST_HAVE(on_curve, ret, err);

	ctx = pt->crv->a.ctx;
	coord_len = (u16)BYTECEIL(ctx->p_bitlen);

	MUST_HAVE((pt_buf_len == (2 * coord_len)), ret, err);

	/* Export the three coordinates */
	ret = fp_export_to_buf(pt_buf, coord_len, &(pt->x)); EG(ret, err);
	ret = fp_export_to_buf(pt_buf + coord_len, coord_len, &(pt->y));

err:
	return ret;
}

/*
 * Mapping curves from twisted Edwards to Montgomery.
 *
 *  E{a, d} is mapped to M{A, B} using the formula:
 *    A = 2(a+d)/(a-d)
 *    B = 4/((a-d) * alpha^2)
 *
 * Returns 0 on success, -1 on error.
 */
int curve_edwards_to_montgomery(ec_edwards_crv_src_t edwards_crv,
				ec_montgomery_crv_t montgomery_crv,
				fp_src_t alpha_edwards)
{
	fp tmp1, tmp2, A, B;
	int ret;
	tmp1.magic = tmp2.magic = A.magic = B.magic = WORD(0);

	ret = ec_edwards_crv_check_initialized(edwards_crv); EG(ret, err);
	ret = fp_check_initialized(alpha_edwards); EG(ret, err);
	MUST_HAVE((edwards_crv->a.ctx == alpha_edwards->ctx), ret, err);

	ret = fp_init(&tmp1, edwards_crv->a.ctx); EG(ret, err);
	ret = fp_init(&tmp2, edwards_crv->a.ctx); EG(ret, err);
	ret = fp_init(&A, edwards_crv->a.ctx); EG(ret, err);
	ret = fp_init(&B, edwards_crv->a.ctx); EG(ret, err);


	/* Compute Z = (alpha ^ 2) et T = 2 / ((a-d) * Z)
	 * and then:
	 *   A = 2(a+d)/(a-d) = Z * (a + d) * T
	 *   B = 4/((a-d) * alpha^2) = 2 * T
	 */
	ret = fp_sqr(&tmp1, alpha_edwards); EG(ret, err);
	ret = fp_sub(&tmp2, &(edwards_crv->a), &(edwards_crv->d)); EG(ret, err);
	ret = fp_mul(&tmp2, &tmp2, &tmp1); EG(ret, err);
	ret = fp_inv(&tmp2, &tmp2); EG(ret, err);
	ret = fp_set_word_value(&B, WORD(2)); EG(ret, err);
	ret = fp_mul(&tmp2, &tmp2, &B); EG(ret, err);

	ret = fp_add(&A, &(edwards_crv->a), &(edwards_crv->d)); EG(ret, err);
	ret = fp_mul(&A, &A, &tmp1); EG(ret, err);
	ret = fp_mul(&A, &A, &tmp2); EG(ret, err);
	ret = fp_mul(&B, &B, &tmp2); EG(ret, err);

	/* Initialize our Montgomery curve */
	ret = ec_montgomery_crv_init(montgomery_crv, &A, &B, &(edwards_crv->order));

err:
	fp_uninit(&tmp1);
	fp_uninit(&tmp2);
	fp_uninit(&A);
	fp_uninit(&B);

	return ret;
}

/*
 * Checks that an Edwards curve and Montgomery curve are compatible.
 *
 * Returns 0 on success, -1 on error.
 */
int curve_edwards_montgomery_check(ec_edwards_crv_src_t e_crv,
				   ec_montgomery_crv_src_t m_crv,
				   fp_src_t alpha_edwards)
{
	int ret, cmp;
	ec_montgomery_crv check;
	check.magic = WORD(0);

	ret = ec_montgomery_crv_check_initialized(m_crv); EG(ret, err);
	ret = curve_edwards_to_montgomery(e_crv, &check, alpha_edwards); EG(ret, err);

	/* Check elements */
	MUST_HAVE((!fp_cmp(&(check.A), &(m_crv->A), &cmp)) && (!cmp), ret, err);
	MUST_HAVE((!fp_cmp(&(check.B), &(m_crv->B), &cmp)) && (!cmp), ret, err);
	MUST_HAVE((!nn_cmp(&(check.order), &(m_crv->order), &cmp)) && (!cmp), ret, err);

err:
	ec_montgomery_crv_uninit(&check);

	return ret;
}

/*
 * Mapping curves from Montgomery to twisted Edwards.
 *
 *  M{A, B} is mapped to E{a, d} using the formula:
 *    a = (A+2)/(B * alpha^2)
 *    d = (A-2)/(B * alpha^2)
 *
 *  Or the inverse (switch a and d roles).
 *
 * Returns 0 on success, -1 on error.
 */
int curve_montgomery_to_edwards(ec_montgomery_crv_src_t m_crv,
				ec_edwards_crv_t e_crv,
				fp_src_t alpha_edwards)
{
	int ret, cmp;
	fp tmp, tmp2, a, d;
	tmp.magic = tmp2.magic = a.magic = d.magic = WORD(0);

	ret = ec_montgomery_crv_check_initialized(m_crv); EG(ret, err);
	ret = fp_check_initialized(alpha_edwards); EG(ret, err);
	MUST_HAVE((m_crv->A.ctx == alpha_edwards->ctx), ret, err);

	ret = fp_init(&tmp, m_crv->A.ctx); EG(ret, err);
	ret = fp_init(&tmp2, m_crv->A.ctx); EG(ret, err);
	ret = fp_init(&a, m_crv->A.ctx); EG(ret, err);
	ret = fp_init(&d, m_crv->A.ctx); EG(ret, err);

	ret = fp_set_word_value(&tmp, WORD(2)); EG(ret, err);
	ret = fp_mul(&tmp2, &(m_crv->B), alpha_edwards); EG(ret, err);
	ret = fp_mul(&tmp2, &tmp2, alpha_edwards); EG(ret, err);
	ret = fp_inv(&tmp2, &tmp2); EG(ret, err);

	/* a = (A+2)/(B * alpha^2) */
	ret = fp_add(&a, &(m_crv->A), &tmp); EG(ret, err);
	ret = fp_mul(&a, &a, &tmp2); EG(ret, err);

	/* d = (A-2)/(B * alpha^2) */
	ret = fp_sub(&d, &(m_crv->A), &tmp); EG(ret, err);
	ret = fp_mul(&d, &d, &tmp2); EG(ret, err);

	/* Initialize our Edwards curve */
	/* Check if we have to inverse a and d */
	ret = fp_one(&tmp); EG(ret, err);
	ret = fp_cmp(&d, &tmp, &cmp); EG(ret, err);
	if (cmp == 0) {
		ret = ec_edwards_crv_init(e_crv, &d, &a, &(m_crv->order));
	} else {
		ret = ec_edwards_crv_init(e_crv, &a, &d, &(m_crv->order));
	}

err:
	fp_uninit(&tmp);
	fp_uninit(&tmp2);
	fp_uninit(&a);
	fp_uninit(&d);

	return ret;
}

/*
 * Mapping curve from Edwards to short Weierstrass.
 *
 * Returns 0 on success, -1 on error.
 */
int curve_edwards_to_shortw(ec_edwards_crv_src_t edwards_crv,
			    ec_shortw_crv_t shortw_crv,
			    fp_src_t alpha_edwards)
{
	int ret;
	ec_montgomery_crv montgomery_crv;
	montgomery_crv.magic = WORD(0);

	ret = curve_edwards_to_montgomery(edwards_crv, &montgomery_crv, alpha_edwards); EG(ret, err);
	ret = curve_montgomery_to_shortw(&montgomery_crv, shortw_crv);

err:
	ec_montgomery_crv_uninit(&montgomery_crv);

	return ret;
}

/* Checking if an Edwards curve and short Weierstrass curve are compliant (through Montgomery mapping).
 *
 * Returns 0 on success, -1 on error.
 */
int curve_edwards_shortw_check(ec_edwards_crv_src_t edwards_crv,
			       ec_shortw_crv_src_t shortw_crv,
			       fp_src_t alpha_edwards)
{
	int ret;
	ec_montgomery_crv montgomery_crv;
	montgomery_crv.magic = WORD(0);

	ret = curve_edwards_to_montgomery(edwards_crv, &montgomery_crv, alpha_edwards); EG(ret, err);

	ret = curve_montgomery_shortw_check(&montgomery_crv, shortw_crv);

err:
	ec_montgomery_crv_uninit(&montgomery_crv);

	return ret;
}

/*
 * Mapping curve from short Weierstrass to Edwards.
 *
 * Returns 0 on success, -1 on error.
 */
int curve_shortw_to_edwards(ec_shortw_crv_src_t shortw_crv,
			    ec_edwards_crv_t edwards_crv,
			    fp_src_t alpha_montgomery,
			    fp_src_t gamma_montgomery,
			    fp_src_t alpha_edwards)
{
	int ret;
	ec_montgomery_crv montgomery_crv;
	montgomery_crv.magic = WORD(0);

	ret = curve_shortw_to_montgomery(shortw_crv, &montgomery_crv, alpha_montgomery, gamma_montgomery); EG(ret, err);

	ret = curve_montgomery_to_edwards(&montgomery_crv, edwards_crv, alpha_edwards);

err:
	ec_montgomery_crv_uninit(&montgomery_crv);

	return ret;
}

/*
 * Mapping points from twisted Edwards to Montgomery.
 *   Point E(x, y) is mapped to M(u, v) with the formula:
 *       - (0, 1) mapped to the point at infinity (not possible in our affine coordinates)
 *       - (0, -1) mapped to (0, 0)
 *       - (u, v) mapped to ((1+y)/(1-y), alpha * (1+y)/((1-y)x))
 *
 * Returns 0 on success, -1 on error.
 */
int aff_pt_edwards_to_montgomery(aff_pt_edwards_src_t in_edwards,
				 ec_montgomery_crv_src_t montgomery_crv,
				 aff_pt_montgomery_t out_montgomery,
				 fp_src_t alpha_edwards)
{
	/* NOTE: we attempt to perform the (0, -1) -> (0, 0) mapping in constant time.
	 * Hence the weird table selection.
	 */
	int ret, iszero, on_curve, cmp;
	fp tmp, tmp2, x, y;
	fp tab_x[2];
	fp_src_t tab_x_t[2] = { &tab_x[0], &tab_x[1] };
	fp tab_y[2];
	fp_src_t tab_y_t[2] = { &tab_y[0], &tab_y[1] };
	u8 idx = 0;
	tmp.magic = tmp2.magic = x.magic = y.magic = WORD(0);
	tab_x[0].magic = tab_x[1].magic = WORD(0);
	tab_y[0].magic = tab_y[1].magic = WORD(0);

	ret = ec_montgomery_crv_check_initialized(montgomery_crv); EG(ret, err);

	/* Check input point is on its curve */
	ret = aff_pt_edwards_is_on_curve(in_edwards, &on_curve); EG(ret, err);
	MUST_HAVE(on_curve, ret, err);

	ret = curve_edwards_montgomery_check(in_edwards->crv, montgomery_crv, alpha_edwards); EG(ret, err);

	ret = fp_init(&tmp, in_edwards->crv->a.ctx); EG(ret, err);
	ret = fp_init(&tmp2, in_edwards->crv->a.ctx); EG(ret, err);
	ret = fp_init(&x, in_edwards->crv->a.ctx); EG(ret, err);
	ret = fp_init(&y, in_edwards->crv->a.ctx); EG(ret, err);
	ret = fp_init(&tab_x[0], in_edwards->crv->a.ctx); EG(ret, err);
	ret = fp_init(&tab_x[1], in_edwards->crv->a.ctx); EG(ret, err);
	ret = fp_init(&tab_y[0], in_edwards->crv->a.ctx); EG(ret, err);
	ret = fp_init(&tab_y[1], in_edwards->crv->a.ctx); EG(ret, err);

	ret = fp_one(&tmp); EG(ret, err);
	/* We do not handle point at infinity in affine coordinates */
	ret = fp_iszero(&(in_edwards->x), &iszero); EG(ret, err);
	ret = fp_cmp(&(in_edwards->y), &tmp, &cmp); EG(ret, err);
	MUST_HAVE(!(iszero && (cmp == 0)), ret, err);
	/* Map (0, -1) to (0, 0) */
	ret = fp_zero(&tmp2); EG(ret, err);
	ret = fp_sub(&tmp2, &tmp2, &tmp); EG(ret, err);
	/* Copy 1 as x as dummy value */
	ret = fp_one(&tab_x[0]); EG(ret, err);
	ret = fp_copy(&tab_x[1], &(in_edwards->x)); EG(ret, err);
	/* Copy -1 as y to produce (0, 0) */
	ret = fp_copy(&tab_y[0], &tmp2); EG(ret, err);
	ret = fp_copy(&tab_y[1], &(in_edwards->y)); EG(ret, err);

	ret = fp_iszero(&(in_edwards->x), &iszero); EG(ret, err);
	ret = fp_cmp(&(in_edwards->y), &tmp2, &cmp); EG(ret, err);
	idx = !(iszero && cmp);
	ret = fp_tabselect(&x, idx, tab_x_t, 2); EG(ret, err);
	ret = fp_tabselect(&y, idx, tab_y_t, 2); EG(ret, err);

	ret = aff_pt_montgomery_init(out_montgomery, montgomery_crv); EG(ret, err);
	/* Compute general case */
	ret = fp_copy(&tmp2, &tmp); EG(ret, err);
	/* Put 1/(1-y) in tmp */
	ret = fp_sub(&tmp, &tmp, &y); EG(ret, err);
	ret = fp_inv(&tmp, &tmp); EG(ret, err);
	/* Put (1+y) in tmp2 */
	ret = fp_add(&tmp2, &tmp2, &y); EG(ret, err);
	/* u = (1+y) / (1-y) */
	ret = fp_mul(&(out_montgomery->u), &tmp, &tmp2); EG(ret, err);
	/* v = alpha_edwards * (1+y)/((1-y)x) */
	ret = fp_inv(&(out_montgomery->v), &x); EG(ret, err);
	ret = fp_mul(&(out_montgomery->v), &(out_montgomery->v), alpha_edwards); EG(ret, err);
	ret = fp_mul(&(out_montgomery->v), &(out_montgomery->u), &(out_montgomery->v)); EG(ret, err);

	/* Final check that the point is on the curve */
	ret = aff_pt_montgomery_is_on_curve(out_montgomery, &on_curve); EG(ret, err);
	if (!on_curve) {
		ret = -1;
	}

err:
	fp_uninit(&tmp);
	fp_uninit(&tmp2);
	fp_uninit(&x);
	fp_uninit(&y);
	fp_uninit(&tab_x[0]);
	fp_uninit(&tab_x[1]);
	fp_uninit(&tab_y[0]);
	fp_uninit(&tab_y[1]);

	return ret;
}

/*
 * Mapping points from Montgomery to twisted Edwards.
 *   Point M(u, v) is mapped to E(x, y) with the formula:
 *       - Point at infinity mapped to (0, 1) (not possible in our affine coordinates)
 *       - (0, 0) mapped to (0, -1)
 *       - (x, y) mapped to (alpha * (u/v), (u-1)/(u+1))
 *
 * Returns 0 on success, -1 on error.
 */
int aff_pt_montgomery_to_edwards(aff_pt_montgomery_src_t in_montgomery,
				 ec_edwards_crv_src_t edwards_crv,
				 aff_pt_edwards_t out_edwards,
				 fp_src_t alpha)
{
	/* NOTE: we attempt to perform the (0, 0) -> (0, -1) mapping in constant time.
	 * Hence the weird table selection.
	 */
	int ret, iszero1, iszero2, on_curve;
	fp tmp, u, v;
	fp tab_u[2];
	fp_src_t tab_u_t[2] = { &tab_u[0], &tab_u[1] };
	fp tab_v[2];
	fp_src_t tab_v_t[2] = { &tab_v[0], &tab_v[1] };
	u8 idx = 0;
	tmp.magic = u.magic = v.magic  = 0;
	tab_u[0].magic = tab_u[1].magic = WORD(0);
	tab_v[0].magic = tab_v[1].magic = WORD(0);

	ret = ec_edwards_crv_check_initialized(edwards_crv); EG(ret, err);

	/* Check input point is on its curve */
	ret = aff_pt_montgomery_is_on_curve(in_montgomery, &on_curve); EG(ret, err);
	MUST_HAVE(on_curve, ret, err);

	ret = curve_edwards_montgomery_check(edwards_crv, in_montgomery->crv, alpha); EG(ret, err);

	ret = fp_init(&tmp, in_montgomery->crv->A.ctx); EG(ret, err);
	ret = fp_init(&u, in_montgomery->crv->A.ctx); EG(ret, err);
	ret = fp_init(&v, in_montgomery->crv->A.ctx); EG(ret, err);
	ret = fp_init(&tab_u[0], in_montgomery->crv->A.ctx); EG(ret, err);
	ret = fp_init(&tab_u[1], in_montgomery->crv->A.ctx); EG(ret, err);
	ret = fp_init(&tab_v[0], in_montgomery->crv->A.ctx); EG(ret, err);
	ret = fp_init(&tab_v[1], in_montgomery->crv->A.ctx); EG(ret, err);

	ret = fp_one(&tmp); EG(ret, err);
	/* Map (0, 0) to (0, -1) */
	/* Copy 0 as u as dummy value */
	ret = fp_zero(&tab_u[0]); EG(ret, err);
	ret = fp_copy(&tab_u[1], &(in_montgomery->u)); EG(ret, err);
	/* Copy 1 as v dummy value to produce (0, -1) */
	ret = fp_copy(&tab_v[0], &tmp); EG(ret, err);
	ret = fp_copy(&tab_v[1], &(in_montgomery->v)); EG(ret, err);

	ret = fp_iszero(&(in_montgomery->u), &iszero1); EG(ret, err);
	ret = fp_iszero(&(in_montgomery->v), &iszero2); EG(ret, err);
	idx = (iszero1 && iszero2) ? 0 : 1;
	ret = fp_tabselect(&u, idx, tab_u_t, 2); EG(ret, err);
	ret = fp_tabselect(&v, idx, tab_v_t, 2); EG(ret, err);

	ret = aff_pt_edwards_init(out_edwards, edwards_crv); EG(ret, err);
	/* x = alpha * (u / v) */
	ret = fp_inv(&(out_edwards->x), &v); EG(ret, err);
	ret = fp_mul(&(out_edwards->x), &(out_edwards->x), alpha); EG(ret, err);
	ret = fp_mul(&(out_edwards->x), &(out_edwards->x), &u); EG(ret, err);
	/* y = (u-1)/(u+1) */
	ret = fp_add(&(out_edwards->y), &u, &tmp); EG(ret, err);
	ret = fp_inv(&(out_edwards->y), &(out_edwards->y)); EG(ret, err);
	ret = fp_sub(&tmp, &u, &tmp); EG(ret, err);
	ret = fp_mul(&(out_edwards->y), &(out_edwards->y), &tmp); EG(ret, err);

	/* Final check that the point is on the curve */
	ret = aff_pt_edwards_is_on_curve(out_edwards, &on_curve); EG(ret, err);
	if (!on_curve) {
		ret = -1;
	}

err:
	fp_uninit(&tmp);
	fp_uninit(&u);
	fp_uninit(&v);
	fp_uninit(&tab_u[0]);
	fp_uninit(&tab_u[1]);
	fp_uninit(&tab_v[0]);
	fp_uninit(&tab_v[1]);

	return ret;
}


/*
 * Map points from Edwards to short Weierstrass through Montgomery (composition mapping).
 *
 * Returns 0 on success, -1 on error.
 */
int aff_pt_edwards_to_shortw(aff_pt_edwards_src_t in_edwards,
			     ec_shortw_crv_src_t shortw_crv,
			     aff_pt_t out_shortw, fp_src_t alpha_edwards)
{
	int ret;
	aff_pt_montgomery inter_montgomery;
	ec_montgomery_crv inter_montgomery_crv;
	inter_montgomery.magic = inter_montgomery_crv.magic = WORD(0);

	/* First, map from Edwards to Montgomery */
	ret = aff_pt_edwards_check_initialized(in_edwards); EG(ret, err);
	ret = curve_edwards_to_montgomery(in_edwards->crv, &inter_montgomery_crv, alpha_edwards); EG(ret, err);
	ret = aff_pt_edwards_to_montgomery(in_edwards, &inter_montgomery_crv, &inter_montgomery, alpha_edwards); EG(ret, err);

	/* Then map from Montgomery to short Weierstrass */
	ret = aff_pt_montgomery_to_shortw(&inter_montgomery, shortw_crv, out_shortw);

err:
	aff_pt_montgomery_uninit(&inter_montgomery);
	ec_montgomery_crv_uninit(&inter_montgomery_crv);

	return ret;
}

/*
 * Map points from projective short Weierstrass to Edwards through Montgomery (composition mapping).
 *
 * Returns 0 on success, -1 on error.
 */
int aff_pt_shortw_to_edwards(aff_pt_src_t in_shortw,
			     ec_edwards_crv_src_t edwards_crv,
			     aff_pt_edwards_t out_edwards,
			     fp_src_t alpha_edwards)
{
	int ret;
	aff_pt_montgomery inter_montgomery;
	ec_montgomery_crv inter_montgomery_crv;
	inter_montgomery.magic = inter_montgomery_crv.magic = WORD(0);

	/* First, map from short Weierstrass to Montgomery */
	ret = curve_edwards_to_montgomery(edwards_crv, &inter_montgomery_crv, alpha_edwards); EG(ret, err);
	ret = aff_pt_shortw_to_montgomery(in_shortw, &inter_montgomery_crv, &inter_montgomery); EG(ret, err);

	/* Then map from Montgomery to Edwards */
	ret = aff_pt_montgomery_to_edwards(&inter_montgomery, edwards_crv, out_edwards, alpha_edwards);

err:
	aff_pt_montgomery_uninit(&inter_montgomery);
	ec_montgomery_crv_uninit(&inter_montgomery_crv);

	return ret;
}

/*
 * Recover the two possible y coordinates from one x on a given
 * curve.
 * The two outputs y1 and y2 are initialized in the function.
 *
 * The function returns -1 on error, 0 on success.
 *
 */
int aff_pt_edwards_y_from_x(fp_t y1, fp_t y2, fp_src_t x, ec_edwards_crv_src_t crv)
{
        int ret;
	fp tmp;
	tmp.magic = WORD(0);

	/* Sanity checks */
	ret = fp_check_initialized(x); EG(ret, err);
	ret = ec_edwards_crv_check_initialized(crv); EG(ret, err);
	MUST_HAVE((x->ctx == crv->a.ctx) && (x->ctx == crv->d.ctx), ret, err);
	MUST_HAVE((y1 != NULL) && (y2 != NULL), ret, err);
	/* Aliasing is not supported */
	MUST_HAVE((y1 != y2) && (y1 != x), ret, err);

	ret = fp_init(y1, x->ctx); EG(ret, err);
	ret = fp_init(y2, x->ctx); EG(ret, err);
	ret = fp_init(&tmp, x->ctx); EG(ret, err);

	/* In order to find our two possible y, we have to find the square
	 * roots of (1 - a x**2) / (1 - d * x**2).
	 */
	ret = fp_one(&tmp); EG(ret, err);
	/* (1 - a x**2) */
	ret = fp_mul(y1, x, &(crv->a)); EG(ret, err);
	ret = fp_mul(y1, y1, x); EG(ret, err);
	ret = fp_sub(y1, &tmp, y1); EG(ret, err);
	/* 1 / (1 - d * x**2) */
	ret = fp_mul(y2, x, &(crv->d)); EG(ret, err);
	ret = fp_mul(y2, y2, x); EG(ret, err);
	ret = fp_sub(y2, &tmp, y2); EG(ret, err);
	ret = fp_inv(y2, y2); EG(ret, err);

	ret = fp_mul(&tmp, y1, y2); EG(ret, err);

	ret = fp_sqrt(y1, y2, &tmp);

err:
	fp_uninit(&tmp);

	return ret;
}

/*
 * Recover the two possible x coordinates from one y on a given
 * curve.
 * The two outputs x1 and x2 are initialized in the function.
 *
 * The function returns -1 on error, 0 on success.
 *
 */
int aff_pt_edwards_x_from_y(fp_t x1, fp_t x2, fp_src_t y, ec_edwards_crv_src_t crv)
{
        int ret;
	fp tmp;
	tmp.magic = WORD(0);

	/* Sanity checks */
	ret = fp_check_initialized(y); EG(ret, err);
	ret = ec_edwards_crv_check_initialized(crv); EG(ret, err);
	MUST_HAVE((y->ctx == crv->a.ctx) && (y->ctx == crv->d.ctx), ret, err);
	MUST_HAVE((x1 != NULL) && (x2 != NULL), ret, err);
	/* Aliasing is not supported */
	MUST_HAVE((x1 != x2) && (x1 != y), ret, err);

	ret = fp_init(x1, y->ctx); EG(ret, err);
	ret = fp_init(x2, y->ctx); EG(ret, err);
	ret = fp_init(&tmp, y->ctx); EG(ret, err);

	/* In order to find our two possible y, we have to find the square
	 * roots of (1 - y**2) / (a - d * y**2).
	 */
	ret = fp_one(&tmp); EG(ret, err);
	/* (1 - y**2) */
	ret = fp_mul(x1, y, y); EG(ret, err);
	ret = fp_sub(x1, &tmp, x1); EG(ret, err);
	/* 1 / (a - d * x**2) */
	ret = fp_mul(x2, y, &(crv->d)); EG(ret, err);
	ret = fp_mul(x2, x2, y); EG(ret, err);
	ret = fp_sub(x2, &(crv->a), x2); EG(ret, err);
	ret = fp_inv(x2, x2); EG(ret, err);

	ret = fp_mul(&tmp, x1, x2); EG(ret, err);

	ret = fp_sqrt(x1, x2, &tmp);

err:
	fp_uninit(&tmp);

	return ret;
}
