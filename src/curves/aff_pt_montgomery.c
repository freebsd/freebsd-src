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

#define AFF_PT_MONTGOMERY_MAGIC ((word_t)(0x7390a9bc43d94598ULL))

/* Verify that an affine point has already been initialized.
 *
 * Returns 0 on success, -1 on error.
 */
int aff_pt_montgomery_check_initialized(aff_pt_montgomery_src_t in)
{
	int ret;

	MUST_HAVE(((in != NULL) && (in->magic == AFF_PT_MONTGOMERY_MAGIC)), ret, err);
	ret = ec_montgomery_crv_check_initialized(in->crv);

err:
	return ret;
}

/*
 * Initialize pointed aff_pt_montgomery structure to make it usable by library
 * function on given curve.
 *
 * Returns 0 on success, -1 on error.
 */
int aff_pt_montgomery_init(aff_pt_montgomery_t in, ec_montgomery_crv_src_t curve)
{
	int ret;

	MUST_HAVE((in != NULL), ret, err);
	ret = ec_montgomery_crv_check_initialized(curve); EG(ret, err);

	ret = fp_init(&(in->u), curve->A.ctx); EG(ret, err);
	ret = fp_init(&(in->v), curve->A.ctx); EG(ret, err);

	in->crv = curve;
	in->magic = AFF_PT_MONTGOMERY_MAGIC;

err:
	return ret;
}

/*
 * Initialize pointed aff_pt_montgomery structure to make it usable by library
 * function on given curve with explicit coordinates.
 *
 * Returns 0 on success, -1 on error.
 */
int aff_pt_montgomery_init_from_coords(aff_pt_montgomery_t in,
			     ec_montgomery_crv_src_t curve,
			     fp_src_t ucoord, fp_src_t vcoord)
{
	int ret;

	ret = aff_pt_montgomery_init(in, curve); EG(ret, err);
	ret = fp_copy(&(in->u), ucoord); EG(ret, err);
	ret = fp_copy(&(in->v), vcoord);

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
void aff_pt_montgomery_uninit(aff_pt_montgomery_t in)
{
	if ((in != NULL) && (in->magic == AFF_PT_MONTGOMERY_MAGIC) && (in->crv != NULL)) {
		fp_uninit(&(in->u));
		fp_uninit(&(in->v));

		in->crv = NULL;
		in->magic = WORD(0);
	}

	return;
}

/*
 * 'on_curve' set to 1 if the point of coordinates (u,v) is on the curve, i.e. if it
 * verifies curve equation B*v^2 = u^3 + A*u^2 + u. It is set to 0 otherwise.
 * 'on_curve' is not meaningful on error.
 *
 * Returns 0 on success, -1 on error.
 */
int is_on_montgomery_curve(fp_src_t u, fp_src_t v, ec_montgomery_crv_src_t curve, int *on_curve)
{
	fp Bv2, u3, Au2, tmp;
	int ret, cmp;
	Bv2.magic = u3.magic = Au2.magic = tmp.magic = WORD(0);

	MUST_HAVE((on_curve != NULL), ret, err);
	ret = ec_montgomery_crv_check_initialized(curve); EG(ret, err);

	ret = fp_check_initialized(u); EG(ret, err);
	ret = fp_check_initialized(v); EG(ret, err);

	MUST_HAVE((u->ctx == v->ctx), ret, err);
	MUST_HAVE((u->ctx == curve->A.ctx), ret, err);

	ret = fp_init(&Bv2, v->ctx); EG(ret, err);
	ret = fp_sqr(&Bv2, v); EG(ret, err);
	ret = fp_mul(&Bv2, &(curve->B), &Bv2); EG(ret, err);

	ret = fp_init(&Au2, u->ctx); EG(ret, err);
	ret = fp_sqr(&Au2, u); EG(ret, err);
	ret = fp_copy(&u3, &Au2); EG(ret, err);
	ret = fp_mul(&Au2, &(curve->A), &Au2); EG(ret, err);

	ret = fp_mul(&u3, &u3, u); EG(ret, err);

	ret = fp_init(&tmp, u->ctx); EG(ret, err);
	ret = fp_add(&tmp, &u3, &Au2); EG(ret, err);
	ret = fp_add(&tmp, &tmp, u); EG(ret, err);

	ret = fp_cmp(&tmp, &Bv2, &cmp); EG(ret, err);

	(*on_curve) = (!cmp);

err:
	fp_uninit(&Bv2);
	fp_uninit(&u3);
	fp_uninit(&Au2);
	fp_uninit(&tmp);

	return ret;
}

/* Checks if affine coordinates point is on a Montgomery curve. 'on_curve' is set to 1 if yes,
 * 0 if no. 'on_curve' is not meaningful in case of error.
 *
 * Returns 0 on success, -1 on error.
 */
int aff_pt_montgomery_is_on_curve(aff_pt_montgomery_src_t pt, int *on_curve)
{
	int ret;

	ret = aff_pt_montgomery_check_initialized(pt); EG(ret, err);

	ret = is_on_montgomery_curve(&(pt->u), &(pt->v), pt->crv, on_curve);

err:
	return ret;
}

/* Copy a Montgomery affine point in an output. The output is initialized properly.
 *
 * Returns 0 on success, -1 on error.
 */
int ec_montgomery_aff_copy(aff_pt_montgomery_t out, aff_pt_montgomery_src_t in)
{
	int ret;

	ret = aff_pt_montgomery_check_initialized(in); EG(ret, err);

	ret = aff_pt_montgomery_init(out, in->crv); EG(ret, err);
	ret = fp_copy(&(out->u), &(in->u)); EG(ret, err);
	ret = fp_copy(&(out->v), &(in->v));

err:
	return ret;
}

/*
 * Compares two given affine points on a Montgomery curve, it returns 0 in input 'cmp' if
 * they correspond or not 0 if not. 'cmp' is not meaningful on error.
 *
 * Returns 0 on success, -1 on error.
 */
int ec_montgomery_aff_cmp(aff_pt_montgomery_src_t in1, aff_pt_montgomery_src_t in2, int *cmp)
{
	int ret, cmp1, cmp2;

	MUST_HAVE((cmp != NULL), ret, err);
	ret = aff_pt_montgomery_check_initialized(in1); EG(ret, err);
	ret = aff_pt_montgomery_check_initialized(in2); EG(ret, err);
	MUST_HAVE((in1->crv == in2->crv), ret, err);

	ret = fp_cmp(&(in1->u), &(in2->u), &cmp1); EG(ret, err);
	ret = fp_cmp(&(in1->v), &(in2->v), &cmp2); EG(ret, err);

	(*cmp) = (cmp1 | cmp2);

err:
	return ret;
}

/*
 * Import an Montgomery affine point from a buffer with the following layout; the 2
 * coordinates (elements of Fp) are each encoded on p_len bytes, where p_len
 * is the size of p in bytes (e.g. 66 for a prime p of 521 bits). Each
 * coordinate is encoded in big endian. Size of buffer must exactly match
 * 2 * p_len.
 *
 * Returns 0 on success, -1 on error.
 */
int aff_pt_montgomery_import_from_buf(aff_pt_montgomery_t pt,
			   const u8 *pt_buf,
			   u16 pt_buf_len, ec_montgomery_crv_src_t crv)
{
	fp_ctx_src_t ctx;
	u16 coord_len;
	int ret, on_curve;

	ret = ec_montgomery_crv_check_initialized(crv); EG(ret, err);
	MUST_HAVE((pt_buf != NULL) && (pt != NULL), ret, err);

	ctx = crv->A.ctx;
	coord_len = (u16)BYTECEIL(ctx->p_bitlen);

	MUST_HAVE((pt_buf_len == (2 * coord_len)), ret, err);

	ret = fp_init_from_buf(&(pt->u), ctx, pt_buf, coord_len); EG(ret, err);
	ret = fp_init_from_buf(&(pt->v), ctx, pt_buf + coord_len, coord_len); EG(ret, err);

	/* Set the curve */
	pt->crv = crv;

	/* Mark the point as initialized */
	pt->magic = AFF_PT_MONTGOMERY_MAGIC;

	/* Check that the point is indeed on the provided curve, uninitialize it
	 * if this is not the case.
	 */
	ret = aff_pt_montgomery_is_on_curve(pt, &on_curve); EG(ret, err);
	if (!on_curve) {
		aff_pt_montgomery_uninit(pt);
		ret = -1;
	}

err:
	return ret;
}


/* Export an Montgomery affine point to a buffer with the following layout; the 2
 * coordinates (elements of Fp) are each encoded on p_len bytes, where p_len
 * is the size of p in bytes (e.g. 66 for a prime p of 521 bits). Each
 * coordinate is encoded in big endian. Size of buffer must exactly match
 * 2 * p_len.
 *
 * Returns 0 on success, -1 on error.
 */
int aff_pt_montgomery_export_to_buf(aff_pt_montgomery_src_t pt, u8 *pt_buf, u32 pt_buf_len)
{
	fp_ctx_src_t ctx;
	u16 coord_len;
	int ret, on_curve;

	ret = aff_pt_montgomery_check_initialized(pt); EG(ret, err);
	MUST_HAVE((pt_buf != NULL), ret, err);

	/* The point to be exported must be on the curve */
	ret = aff_pt_montgomery_is_on_curve(pt, &on_curve); EG(ret, err);
	MUST_HAVE(on_curve, ret, err);

	ctx = pt->crv->A.ctx;
	coord_len = (u16)BYTECEIL(ctx->p_bitlen);

	MUST_HAVE((pt_buf_len == (2 * coord_len)), ret, err);

	/* Export the three coordinates */
	ret = fp_export_to_buf(pt_buf, coord_len, &(pt->u)); EG(ret, err);
	ret = fp_export_to_buf(pt_buf + coord_len, coord_len, &(pt->v));

err:
	return ret;
}

/**** Mappings between curves *************/
/*
 * Mapping curves from Montgomery to short Weiertstrass.
 *
 *  M{A, B} is mapped to W{a, b} using the formula:
 *    a = (3-A^2)/(3*B^2)
 *    b = (2*A^3-9*A)/(27*B^3)
 *
 * Returns 0 on success, -1 on error.
 */
int curve_montgomery_to_shortw(ec_montgomery_crv_src_t montgomery_crv, ec_shortw_crv_t shortw_crv)
{
	fp tmp, tmp2, a, b;
	int ret;
	tmp.magic = tmp2.magic = a.magic = b.magic = WORD(0);

	ret = ec_montgomery_crv_check_initialized(montgomery_crv); EG(ret, err);

	ret = fp_init(&tmp, montgomery_crv->A.ctx); EG(ret, err);
	ret = fp_init(&tmp2, montgomery_crv->A.ctx); EG(ret, err);
	ret = fp_init(&a, montgomery_crv->A.ctx); EG(ret, err);
	ret = fp_init(&b, montgomery_crv->A.ctx); EG(ret, err);

	/* Compute a */
	ret = fp_sqr(&tmp, &(montgomery_crv->B)); EG(ret, err);
	ret = fp_set_word_value(&tmp2, WORD(3)); EG(ret, err);
	/* 3*B^2 */
	ret = fp_mul(&tmp, &tmp, &tmp2); EG(ret, err);
	/* (3*B^2)^-1 */
	ret = fp_inv(&tmp, &tmp); EG(ret, err);

	/* (3-A^2) */
	ret = fp_sqr(&tmp2, &(montgomery_crv->A)); EG(ret, err);
	ret = fp_set_word_value(&a, WORD(3)); EG(ret, err);
	ret = fp_sub(&tmp2, &a, &tmp2); EG(ret, err);

	ret = fp_mul(&a, &tmp2, &tmp); EG(ret, err);

	/* Compute b */
	ret = fp_sqr(&tmp, &(montgomery_crv->B)); EG(ret, err);
	ret = fp_mul(&tmp, &tmp, &(montgomery_crv->B)); EG(ret, err);
	ret = fp_set_word_value(&tmp2, WORD(27)); EG(ret, err);
	/* (27*B^3) */
	ret = fp_mul(&tmp, &tmp, &tmp2); EG(ret, err);
	/* (27*B^3)^-1 */
	ret = fp_inv(&tmp, &tmp); EG(ret, err);

	/* (2*A^3-9*A) */
	ret = fp_set_word_value(&tmp2, WORD(2)); EG(ret, err);
	ret = fp_mul(&tmp2, &tmp2, &(montgomery_crv->A)); EG(ret, err);
	ret = fp_mul(&tmp2, &tmp2, &(montgomery_crv->A)); EG(ret, err);
	ret = fp_mul(&tmp2, &tmp2, &(montgomery_crv->A)); EG(ret, err);

	ret = fp_set_word_value(&b, WORD(9)); EG(ret, err);
	ret = fp_mul(&b, &b, &(montgomery_crv->A)); EG(ret, err);
	ret = fp_sub(&b, &tmp2, &b); EG(ret, err);

	ret = fp_mul(&b, &b, &tmp); EG(ret, err);

	/* Initialize our short Weiertstrass curve */
	ret = ec_shortw_crv_init(shortw_crv, &a, &b, &(montgomery_crv->order));

err:
	fp_uninit(&a);
	fp_uninit(&b);
	fp_uninit(&tmp);
	fp_uninit(&tmp2);

	return ret;
}

/*
 * Checks that a short Weiertstrass curve and Montgomery curve are compatible.
 *
 * Returns 0 on success, -1 on error.
 */
int curve_montgomery_shortw_check(ec_montgomery_crv_src_t montgomery_crv,
				  ec_shortw_crv_src_t shortw_crv)
{
	int ret, cmp;
	ec_shortw_crv check;
	check.magic = WORD(0);

	ret = ec_shortw_crv_check_initialized(shortw_crv); EG(ret, err);
	ret = curve_montgomery_to_shortw(montgomery_crv, &check); EG(ret, err);

	/* Check elements */
	MUST_HAVE((!fp_cmp(&(check.a), &(shortw_crv->a), &cmp)) && (!cmp), ret, err);
	MUST_HAVE((!fp_cmp(&(check.b), &(shortw_crv->b), &cmp)) && (!cmp), ret, err);
	MUST_HAVE((!nn_cmp(&(check.order), &(shortw_crv->order), &cmp)) && (!cmp), ret, err);

err:
	ec_shortw_crv_uninit(&check);

	return ret;
}

/*
 * Mapping curves from short Weiertstrass to Montgomery
 *
 *  W{a, b} is mapped to M{A, B} using the formula:
 *    A = 3 * alpha / gamma
 *    B = 1 / gamma
 *  with gamma square root of c = a + 3 * alpha**2
 *
 * Returns 0 on success, -1 on error.
 */
int curve_shortw_to_montgomery(ec_shortw_crv_src_t shortw_crv,
			       ec_montgomery_crv_t montgomery_crv,
			       fp_src_t alpha, fp_src_t gamma)
{
	int ret, cmp;
	fp c, gamma_inv, A, tmp;
	c.magic = gamma_inv.magic = A.magic = tmp.magic = WORD(0);

	ret = ec_shortw_crv_check_initialized(shortw_crv); EG(ret, err);
	ret = fp_check_initialized(alpha); EG(ret, err);
	ret = fp_check_initialized(gamma); EG(ret, err);
	MUST_HAVE((alpha->ctx == shortw_crv->a.ctx) && (gamma->ctx == shortw_crv->a.ctx), ret, err);

	ret = fp_init(&A, shortw_crv->a.ctx); EG(ret, err);
	ret = fp_init(&gamma_inv, shortw_crv->a.ctx); EG(ret, err);
	ret = fp_init(&c, shortw_crv->a.ctx); EG(ret, err);
	ret = fp_init(&tmp, shortw_crv->a.ctx); EG(ret, err);

	/* Compute 1 / gamma */
	ret = fp_inv(&gamma_inv, gamma); EG(ret, err);

	/* Compute A */
	ret = fp_set_word_value(&A, WORD(3)); EG(ret, err);
	ret = fp_mul(&A, &A, alpha); EG(ret, err);
	ret = fp_mul(&A, &A, &gamma_inv); EG(ret, err);

	/* Sanity check on c */
	ret = fp_set_word_value(&c, WORD(3)); EG(ret, err);
	ret = fp_mul(&c, &c, alpha); EG(ret, err);
	ret = fp_mul(&c, &c, alpha); EG(ret, err);
	ret = fp_add(&c, &c, &(shortw_crv->a)); EG(ret, err);
	ret = fp_sqr(&tmp, gamma); EG(ret, err);
	/* gamma ** 2 must be equal to c */
	MUST_HAVE((!fp_cmp(&c, &tmp, &cmp)) && (!cmp), ret, err);

	/* B is simply the inverse of gamma */
	ret = ec_montgomery_crv_init(montgomery_crv, &A, &gamma_inv, &(shortw_crv->order));

err:
	fp_uninit(&A);
	fp_uninit(&gamma_inv);
	fp_uninit(&c);
	fp_uninit(&tmp);

	return ret;
}

/*
 * Mapping points from Montgomery to short Weierstrass.
 *   Point M(u, v) is mapped to W(x, y) with the formula:
 *       - (x, y) = ((u/B)+(A/3B), v/B)
 *
 * Returns 0 on success, -1 on error.
 */
int aff_pt_montgomery_to_shortw(aff_pt_montgomery_src_t in_montgomery,
				ec_shortw_crv_src_t shortw_crv, aff_pt_t out_shortw)
{
	int ret, on_curve;
	fp tmp, tmp2;
	tmp.magic = tmp2.magic = WORD(0);

	ret = ec_shortw_crv_check_initialized(shortw_crv); EG(ret, err);

	/* Check that our input point is on its curve */
	MUST_HAVE((!aff_pt_montgomery_is_on_curve(in_montgomery, &on_curve)) && on_curve, ret, err);

	ret = fp_init(&tmp, in_montgomery->crv->A.ctx); EG(ret, err);
	ret = fp_init(&tmp2, in_montgomery->crv->A.ctx); EG(ret, err);

	ret = aff_pt_montgomery_check_initialized(in_montgomery); EG(ret, err);
	ret = curve_montgomery_shortw_check(in_montgomery->crv, shortw_crv); EG(ret, err);

	ret = aff_pt_init(out_shortw, shortw_crv); EG(ret, err);

	ret = fp_inv(&tmp, &(in_montgomery->crv->B)); EG(ret, err);
	ret = fp_mul(&tmp, &tmp, &(in_montgomery->u)); EG(ret, err);

	ret = fp_set_word_value(&tmp2, WORD(3)); EG(ret, err);
	ret = fp_mul(&tmp2, &tmp2, &(in_montgomery->crv->B)); EG(ret, err);
	ret = fp_inv(&tmp2, &tmp2); EG(ret, err);
	ret = fp_mul(&tmp2, &tmp2, &(in_montgomery->crv->A)); EG(ret, err);

	ret = fp_add(&(out_shortw->x), &tmp, &tmp2); EG(ret, err);

	ret = fp_inv(&tmp, &(in_montgomery->crv->B)); EG(ret, err);
	ret = fp_mul(&(out_shortw->y), &tmp, &(in_montgomery->v)); EG(ret, err);

	/* Final check that the point is on the curve */
	MUST_HAVE((!aff_pt_is_on_curve(out_shortw, &on_curve)) && on_curve, ret, err);

err:
	fp_uninit(&tmp);
	fp_uninit(&tmp2);

	return ret;
}

/*
 * Mapping from short Weierstrass to Montgomery.
 *   Point W(x, y) is mapped to M(u, v) with the formula:
 *       - (u, v) = (((Bx)−(A/3), By)
 *
 * Returns 0 on success, -1 on error.
 */
int aff_pt_shortw_to_montgomery(aff_pt_src_t in_shortw,
				ec_montgomery_crv_src_t montgomery_crv,
				aff_pt_montgomery_t out_montgomery)
{
	int ret, on_curve;
	fp tmp, tmp2;
	tmp.magic = tmp2.magic = WORD(0);

	ret = ec_montgomery_crv_check_initialized(montgomery_crv); EG(ret, err);

	/* Check that our input point is on its curve */
	MUST_HAVE((!aff_pt_is_on_curve(in_shortw, &on_curve)) && on_curve, ret, err);

	ret = fp_init(&tmp, in_shortw->crv->a.ctx); EG(ret, err);
	ret = fp_init(&tmp2, in_shortw->crv->a.ctx); EG(ret, err);

	ret = curve_montgomery_shortw_check(montgomery_crv, in_shortw->crv); EG(ret, err);

	ret = aff_pt_montgomery_init(out_montgomery, montgomery_crv); EG(ret, err);

	/* A/3 */
	ret = fp_inv_word(&tmp, WORD(3)); EG(ret, err);
	ret = fp_mul(&tmp, &tmp, &(montgomery_crv->A)); EG(ret, err);

	/* Bx */
	ret = fp_mul(&tmp2, &(montgomery_crv->B), &(in_shortw->x)); EG(ret, err);

	/* u = (Bx) - (A/3) */
	ret = fp_sub(&(out_montgomery->u), &tmp2, &tmp); EG(ret, err);

	/* v = By */
	ret = fp_mul(&(out_montgomery->v), &(montgomery_crv->B), &(in_shortw->y)); EG(ret, err);

	/* Final check that the point is on the curve */
	MUST_HAVE((!aff_pt_montgomery_is_on_curve(out_montgomery, &on_curve)) && on_curve, ret, err);

err:
	fp_uninit(&tmp);
	fp_uninit(&tmp2);

	return ret;
}


/*
 * Recover the two possible v coordinates from one u on a given
 * curve.
 * The two outputs v1 and v2 are initialized in the function.
 *
 * The function returns -1 on error, 0 on success.
 *
 */
int aff_pt_montgomery_v_from_u(fp_t v1, fp_t v2, fp_src_t u, ec_montgomery_crv_src_t crv)
{
	int ret;

	/* Sanity checks */
	ret = fp_check_initialized(u); EG(ret, err);
	ret = ec_montgomery_crv_check_initialized(crv); EG(ret, err);
	MUST_HAVE((u->ctx == crv->A.ctx) && (u->ctx == crv->B.ctx), ret, err);
	MUST_HAVE((v1 != NULL) && (v2 != NULL), ret, err);
	/* Aliasing is not supported */
	MUST_HAVE((v1 != v2) && (v1 != u), ret, err);

	/* Initialize v1 and v2 with context */
	ret = fp_init(v1, u->ctx); EG(ret, err);
	ret = fp_init(v2, u->ctx); EG(ret, err);

	/* v must satisfy the equation B v^2 = u^3 + A u^2 + u,
	 * so we compute square root for B^-1 * (u^3 + A u^2 + u)
	 */
	ret = fp_sqr(v2, u); EG(ret, err);
	ret = fp_mul(v1, v2, u); EG(ret, err);
	ret = fp_mul(v2, v2, &(crv->A)); EG(ret, err);
	ret = fp_add(v1, v1, v2); EG(ret, err);
	ret = fp_add(v1, v1, u); EG(ret, err);
	ret = fp_inv(v2, &(crv->B)); EG(ret, err);
	ret = fp_mul(v1, v1, v2); EG(ret, err);

	/* Choose any of the two square roots as the solution */
	ret = fp_sqrt(v1, v2, v1);

err:
	return ret;
}
