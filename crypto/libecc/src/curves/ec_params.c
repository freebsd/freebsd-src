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
#include <libecc/curves/ec_params.h>
#include <libecc/curves/curves.h>

/*
 * Initialize (already allocated) curve parameters structure pointed by
 * ec_params using value provided in remaining parameters. The function
 * returns 0 on success, -1 on error.
 */
int import_params(ec_params *out_params, const ec_str_params *in_str_params)
{
	nn tmp_p, tmp_p_bitlen, tmp_r, tmp_r_square, tmp_mpinv, tmp_p_shift;
	nn tmp_p_normalized, tmp_p_reciprocal, tmp_curve_order, tmp_order;
	nn tmp_order_bitlen, tmp_cofactor;
	fp tmp_a, tmp_b, tmp_gx, tmp_gy, tmp_gz;
	ec_curve_type curve_type;
	int ret;
	tmp_p.magic = tmp_r.magic = tmp_r_square.magic = tmp_mpinv.magic = WORD(0);
	tmp_p_shift.magic = tmp_p_normalized.magic = tmp_p_reciprocal.magic = WORD(0);
	tmp_a.magic = tmp_b.magic = tmp_curve_order.magic = tmp_gx.magic = WORD(0);
	tmp_gy.magic = tmp_gz.magic = tmp_order.magic = tmp_cofactor.magic = WORD(0);
	tmp_order_bitlen.magic = tmp_p_bitlen.magic = WORD(0);

	MUST_HAVE(((out_params != NULL) && (in_str_params != NULL)), ret, err);

	ret = local_memset(out_params, 0, sizeof(ec_params)); EG(ret, err);

	/*
	 * We first need to import p, the prime defining Fp and associated
	 * Montgomery parameters (r, r^2 and mpinv)
	 */
	ret = nn_init_from_buf(&tmp_p, PARAM_BUF_PTR(in_str_params->p),
			PARAM_BUF_LEN(in_str_params->p)); EG(ret, err);

	ret = nn_init_from_buf(&tmp_p_bitlen,
			PARAM_BUF_PTR(in_str_params->p_bitlen),
			PARAM_BUF_LEN(in_str_params->p_bitlen)); EG(ret, err);

	ret = nn_init_from_buf(&tmp_r, PARAM_BUF_PTR(in_str_params->r),
			PARAM_BUF_LEN(in_str_params->r)); EG(ret, err);

	ret = nn_init_from_buf(&tmp_r_square,
			PARAM_BUF_PTR(in_str_params->r_square),
			PARAM_BUF_LEN(in_str_params->r_square)); EG(ret, err);

	ret = nn_init_from_buf(&tmp_mpinv,
			PARAM_BUF_PTR(in_str_params->mpinv),
			PARAM_BUF_LEN(in_str_params->mpinv)); EG(ret, err);

	ret = nn_init_from_buf(&tmp_p_shift,
			PARAM_BUF_PTR(in_str_params->p_shift),
			PARAM_BUF_LEN(in_str_params->p_shift)); EG(ret, err);

	ret = nn_init_from_buf(&tmp_p_normalized,
			PARAM_BUF_PTR(in_str_params->p_normalized),
			PARAM_BUF_LEN(in_str_params->p_normalized)); EG(ret, err);

	ret = nn_init_from_buf(&tmp_p_reciprocal,
			 PARAM_BUF_PTR(in_str_params->p_reciprocal),
			 PARAM_BUF_LEN(in_str_params->p_reciprocal)); EG(ret, err);

	/* From p, we can create global Fp context */
	ret = fp_ctx_init(&(out_params->ec_fp), &tmp_p,
		    (bitcnt_t)(tmp_p_bitlen.val[0]),
		    &tmp_r, &tmp_r_square,
		    tmp_mpinv.val[0], (bitcnt_t)tmp_p_shift.val[0],
		    &tmp_p_normalized, tmp_p_reciprocal.val[0]); EG(ret, err);

	/*
	 * Having Fp context, we can import a and b, the coefficient of
	 * of Weierstrass equation.
	 */
	ret = fp_init_from_buf(&tmp_a, &(out_params->ec_fp),
			 PARAM_BUF_PTR(in_str_params->a),
			 PARAM_BUF_LEN(in_str_params->a)); EG(ret, err);
	ret = fp_init_from_buf(&tmp_b, &(out_params->ec_fp),
			 PARAM_BUF_PTR(in_str_params->b),
			 PARAM_BUF_LEN(in_str_params->b)); EG(ret, err);

	/*
	 * Now we can store the number of points in the group generated
	 * by g and the associated cofactor (i.e. npoints / order).
	 */
	ret = nn_init_from_buf(&tmp_order,
			 PARAM_BUF_PTR(in_str_params->gen_order),
			 PARAM_BUF_LEN(in_str_params->gen_order)); EG(ret, err);
	ret = nn_init(&(out_params->ec_gen_order), (u16)(tmp_order.wlen * WORD_BYTES)); EG(ret, err);
	ret = nn_copy(&(out_params->ec_gen_order), &tmp_order); EG(ret, err);

	ret = nn_init_from_buf(&tmp_order_bitlen,
			 PARAM_BUF_PTR(in_str_params->gen_order_bitlen),
			 PARAM_BUF_LEN(in_str_params->gen_order_bitlen)); EG(ret, err);
	out_params->ec_gen_order_bitlen = (bitcnt_t)(tmp_order_bitlen.val[0]);

	ret = nn_init_from_buf(&tmp_cofactor,
			 PARAM_BUF_PTR(in_str_params->cofactor),
			 PARAM_BUF_LEN(in_str_params->cofactor)); EG(ret, err);
	ret = nn_init(&(out_params->ec_gen_cofactor),
		(u16)(tmp_cofactor.wlen * WORD_BYTES)); EG(ret, err);
	ret = nn_copy(&(out_params->ec_gen_cofactor), &tmp_cofactor); EG(ret, err);

	/* Now we can store the number of points on the curve (curve order) */
	ret = nn_init_from_buf(&tmp_curve_order,
			 PARAM_BUF_PTR(in_str_params->curve_order),
			 PARAM_BUF_LEN(in_str_params->curve_order)); EG(ret, err);

	/* Now, we can create curve context from a and b. */
	ret = ec_shortw_crv_init(&(out_params->ec_curve), &tmp_a, &tmp_b, &tmp_curve_order); EG(ret, err);

	/* Let's now import G from its affine coordinates (gx,gy) */
	ret = fp_init_from_buf(&tmp_gx, &(out_params->ec_fp),
			 PARAM_BUF_PTR(in_str_params->gx),
			 PARAM_BUF_LEN(in_str_params->gx)); EG(ret, err);
	ret = fp_init_from_buf(&tmp_gy, &(out_params->ec_fp),
			 PARAM_BUF_PTR(in_str_params->gy),
			 PARAM_BUF_LEN(in_str_params->gy)); EG(ret, err);
	ret = fp_init_from_buf(&tmp_gz, &(out_params->ec_fp),
			 PARAM_BUF_PTR(in_str_params->gz),
			 PARAM_BUF_LEN(in_str_params->gz)); EG(ret, err);
	ret = prj_pt_init_from_coords(&(out_params->ec_gen),
				&(out_params->ec_curve),
				&tmp_gx, &tmp_gy, &tmp_gz); EG(ret, err);

#if !defined(USE_SMALL_STACK)
	/* Let's get the optional alpha transfert coefficients */
	ret = fp_init_from_buf(&(out_params->ec_alpha_montgomery), &(out_params->ec_fp),
			 PARAM_BUF_PTR(in_str_params->alpha_montgomery),
			 PARAM_BUF_LEN(in_str_params->alpha_montgomery)); EG(ret, err);
	ret = fp_init_from_buf(&(out_params->ec_gamma_montgomery), &(out_params->ec_fp),
			 PARAM_BUF_PTR(in_str_params->gamma_montgomery),
			 PARAM_BUF_LEN(in_str_params->gamma_montgomery)); EG(ret, err);

	ret = fp_init_from_buf(&(out_params->ec_alpha_edwards), &(out_params->ec_fp),
			 PARAM_BUF_PTR(in_str_params->alpha_edwards),
			 PARAM_BUF_LEN(in_str_params->alpha_edwards)); EG(ret, err);
#endif

	/* Import a local copy of curve OID */
	MUST_HAVE(in_str_params->oid->buflen < MAX_CURVE_OID_LEN, ret, err);
	ret = local_memset(out_params->curve_oid, 0, MAX_CURVE_OID_LEN); EG(ret, err);
	ret = local_strncpy((char *)(out_params->curve_oid),
		      (const char *)(in_str_params->oid->buf),
		      in_str_params->oid->buflen); EG(ret, err);

	/* Import a local copy of curve name */
	MUST_HAVE(in_str_params->name->buflen < MAX_CURVE_NAME_LEN, ret, err);
	ret = local_memset(out_params->curve_name, 0, MAX_CURVE_NAME_LEN); EG(ret, err);
	ret = local_strncpy((char *)(out_params->curve_name),
		      (const char *)(in_str_params->name->buf),
		      in_str_params->name->buflen); EG(ret, err);

	/* Get the curve type */
	ret = ec_get_curve_type_by_name(in_str_params->name->buf,
					in_str_params->name->buflen,
					&curve_type); EG(ret, err);
	MUST_HAVE(curve_type != UNKNOWN_CURVE, ret, err);
	out_params->curve_type = curve_type;

err:
	/* Uninit temporary parameters */
	nn_uninit(&tmp_p_bitlen);
	nn_uninit(&tmp_order_bitlen);
	nn_uninit(&tmp_p);
	nn_uninit(&tmp_r);
	nn_uninit(&tmp_r_square);
	nn_uninit(&tmp_mpinv);
	nn_uninit(&tmp_p_shift);
	nn_uninit(&tmp_p_normalized);
	nn_uninit(&tmp_p_reciprocal);
	fp_uninit(&tmp_a);
	fp_uninit(&tmp_b);
	nn_uninit(&tmp_curve_order);
	fp_uninit(&tmp_gx);
	fp_uninit(&tmp_gy);
	fp_uninit(&tmp_gz);
	nn_uninit(&tmp_order);
	nn_uninit(&tmp_cofactor);

	return ret;
}
