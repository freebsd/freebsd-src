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
#include <libecc/libec.h>
/* We include the printf external dependency for printf output */
#include <libecc/external_deps/print.h>
/* We include the time external dependency for performance measurement */
#include <libecc/external_deps/time.h>

/* The followin function picks a random Fp element x, where Fp is the
 * curve underlying prime field, and computes y in Fp such that:
 *   y^2 = x^3 + ax + b, where a and b are the input elliptic
 * curve parameters.
 *
 * This means that (x, y) are the affine coordinates of a "random"
 * point on our curve. The function then outputs the projective
 * coordinates of (x, y), i.e. the triplet (x, y, 1).
 * PS: all our operations on points are done with projective coordinates.
 *
 * Computing y means computing a quadratic residue in Fp, for which we
 * use the Tonelli-Shanks algorithm implemented in the Fp source example
 * (fp_square_residue.c).
 */
ATTRIBUTE_WARN_UNUSED_RET int get_random_point_on_curve(ec_params *curve_params, prj_pt *out_point);
int get_random_point_on_curve(ec_params *curve_params, prj_pt *out_point)
{
	nn nn_tmp;
	int ret, is_oncurve;

	/* Inside our internal representation, curve_params->ec_curve
	 * contains the curve coefficients a and b.
	 * curve_params->ec_fp is the Fp context of the curve.
	 */
	fp x, y, fp_tmp1, fp_tmp2;
	fp_ctx_src_t ctx;

	MUST_HAVE((curve_params != NULL), ret, err);

	nn_tmp.magic = WORD(0);
	x.magic = y.magic = fp_tmp1.magic = fp_tmp2.magic = WORD(0);

	/* Initialize our x value with the curve Fp context */
	ctx = &(curve_params->ec_fp);

	ret = fp_init(&x, ctx); EG(ret, err);
	ret = fp_init(&y, ctx); EG(ret, err);
	ret = fp_init(&fp_tmp1, ctx); EG(ret, err);
	ret = fp_init(&fp_tmp2, ctx); EG(ret, err);

	ret = nn_init(&nn_tmp, 0); EG(ret, err);
	ret = nn_set_word_value(&nn_tmp, WORD(3)); EG(ret, err);
	while (1) {
		/* Get a random Fp */
		ret = fp_get_random(&x, ctx); EG(ret, err);
		ret = fp_copy(&fp_tmp1, &x); EG(ret, err);
		ret = fp_copy(&fp_tmp2, &x); EG(ret, err);
		/* Compute x^3 + ax + b */
		ret = fp_pow(&fp_tmp1, &fp_tmp1, &nn_tmp); EG(ret, err);
		ret = fp_mul(&fp_tmp2, &fp_tmp2, &(curve_params->ec_curve.a)); EG(ret, err);
		ret = fp_add(&fp_tmp1, &fp_tmp1, &fp_tmp2); EG(ret, err);
		ret = fp_add(&fp_tmp1, &fp_tmp1, &(curve_params->ec_curve.b)); EG(ret, err);
		/*
		 * Get any of the two square roots, corresponding to (x, y)
		 * and (x, -y) both on the curve. If no square root exist,
		 * go to next random Fp.
		 */
		if (fp_sqrt(&y, &fp_tmp2, &fp_tmp1) == 0) {
			/* Check that we indeed satisfy the curve equation */
			ret = is_on_shortw_curve(&x, &y, &(curve_params->ec_curve), &is_oncurve); EG(ret, err);
			if (!is_oncurve) {
				/* This should not happen ... */
				ext_printf("Error: Tonelli-Shanks found a bad "
					   "solution to curve equation ...\n");
				continue;
			}
			break;
		}
	}
	/* Now initialize our point with the coordinates (x, y, 1) */
	ret = fp_one(&fp_tmp1); EG(ret, err);
	ret = prj_pt_init_from_coords(out_point, &(curve_params->ec_curve), &x, &y,
				&fp_tmp1); EG(ret, err);

err:
	fp_uninit(&x);
	fp_uninit(&y);
	fp_uninit(&fp_tmp1);
	fp_uninit(&fp_tmp2);
	nn_uninit(&nn_tmp);

	return ret;
}

#define PERF_SCALAR_MUL 40
ATTRIBUTE_WARN_UNUSED_RET int check_curve(const u8 *curve_name);
int check_curve(const u8 *curve_name)
{
	unsigned int i;
	u64 t1, t2;
	int ret, is_oncurve, isone, iszero;

	nn nn_k;
	/* libecc internal structure holding the curve parameters */
	ec_params curve_params;
	/* libecc internal structure holding projective points on curves */
	prj_pt A, B, C, D;
	prj_pt TMP;
	aff_pt T;
	u32 len;

	/* Importing a specific curve parameters from the constant static
	 * buffers describing it:
	 * It is possible to import a curves parameters by its name.
	 */
	const ec_str_params *the_curve_const_parameters;

	nn_k.magic = WORD(0);
	A.magic = B.magic = C.magic = D.magic = WORD(0);
	TMP.magic = T.magic = WORD(0);

	MUST_HAVE((curve_name != NULL), ret, err);

	ret = local_strnlen((const char *)curve_name, MAX_CURVE_NAME_LEN, &len); EG(ret, err);
	len += 1;
	MUST_HAVE((len < 256), ret, err);
	ret = ec_get_curve_params_by_name(curve_name,
					    (u8)len, &the_curve_const_parameters); EG(ret, err);


	/* Get out if getting the parameters went wrong */
	if (the_curve_const_parameters == NULL) {
		ext_printf("Error: error when importing curve %s "
			   "parameters ...\n", curve_name);
		ret = -1;
		goto err;
	}
	/* Now map the curve parameters to our libecc internal representation */
	ret = import_params(&curve_params, the_curve_const_parameters); EG(ret, err);
	/* Get two random points on the curve */
	ret = get_random_point_on_curve(&curve_params, &A); EG(ret, err);
	ret = get_random_point_on_curve(&curve_params, &B); EG(ret, err);

	/*
	 * Let's add the two points
	 * C = A + B with regular point addition
	 */
	ret = prj_pt_add(&C, &A, &B); EG(ret, err);

	/*
	 * Check that the resulting additive point C = A+B is indeed on the
	 * curve.
	 */
	ret = prj_pt_to_aff(&T, &C); EG(ret, err);
	ret = prj_pt_is_on_curve(&C, &is_oncurve); EG(ret, err);
	if (!is_oncurve) {
		ext_printf("Error: C = A+B is not on the %s curve!\n",
			   curve_params.curve_name);
		ret = -1;
		goto err;
	}
	ret = aff_pt_is_on_curve(&T, &is_oncurve); EG(ret, err);
	if (!is_oncurve) {
		ext_printf("Error: C = A+B is not on the %s curve!\n",
			   curve_params.curve_name);
		ret = -1;
		goto err;
	}
	/* Same check with doubling
	 * C = 2A = A+A
	 */
	ret = prj_pt_dbl(&C, &A); EG(ret, err);

	/* Check that the resulting point C = 2A is indeed on the curve.
	 *
	 */
	ret = prj_pt_to_aff(&T, &C); EG(ret, err);
	ret = prj_pt_is_on_curve(&C, &is_oncurve); EG(ret, err);
	if (!is_oncurve) {
		ext_printf("Error: C = A+B is not on the %s curve!\n",
			   curve_params.curve_name);
		ret = -1;
		goto err;
	}
	ret = aff_pt_is_on_curve(&T, &is_oncurve); EG(ret, err);
	if (!is_oncurve) {
		ext_printf("Error: C = A+B is not on the %s curve!\n",
			   curve_params.curve_name);
		ret = -1;
		goto err;
	}
	/*
	 * If the cofactor of the curve is 1, this means that the order of the
	 * generator is the cardinal of the curve (and hence the order of the
	 * curve points group). This means that for any point P on the curve,
	 * we should have qP = 0 (the inifinity point, i.e. the zero neutral
	 * element of the curve additive group).
	 */
	ret = prj_pt_add(&C, &A, &B); EG(ret, err);
	ret = prj_pt_dbl(&D, &A); EG(ret, err);
	ret = nn_isone(&(curve_params.ec_gen_cofactor), &isone); EG(ret, err);
	if (isone) {
		ret = prj_pt_mul(&TMP, &(curve_params.ec_gen_order), &A); EG(ret, err);
		ret = prj_pt_iszero(&TMP, &iszero); EG(ret, err);
		if (!iszero) {
			ext_printf("Error: qA is not 0! (regular mul)\n");
			ret = -1;
			goto err;
		}
		/**/
		ret = prj_pt_mul_blind(&TMP, &(curve_params.ec_gen_order), &A); EG(ret, err);
		ret = prj_pt_iszero(&TMP, &iszero); EG(ret, err);
		if (!iszero) {
			ext_printf("Error: qA is not 0! (regular blind mul)\n");
			ret = -1;
			goto err;
		}
		/**/
		ret = prj_pt_mul(&TMP, &(curve_params.ec_gen_order), &B); EG(ret, err);
		ret = prj_pt_iszero(&TMP, &iszero); EG(ret, err);
		if (!iszero) {
			ext_printf("Error: qB is not 0! (regular mul)\n");
			ret = -1;
			goto err;
		}
		/**/
		ret = prj_pt_mul_blind(&TMP, &(curve_params.ec_gen_order), &B); EG(ret, err);
		ret = prj_pt_iszero(&TMP, &iszero); EG(ret, err);
		if (!iszero) {
			ext_printf("Error: qB is not 0! (regular blind mul)\n");
			ret = -1;
			goto err;
		}
		/**/
		ret = prj_pt_mul(&TMP, &(curve_params.ec_gen_order), &C); EG(ret, err);
		ret = prj_pt_iszero(&TMP, &iszero); EG(ret, err);
		if (!iszero) {
			ext_printf("Error: qC is not 0! (regular mul)\n");
			ret = -1;
			goto err;
		}
		/**/
		ret = prj_pt_mul_blind(&TMP, &(curve_params.ec_gen_order), &C); EG(ret, err);
		ret = prj_pt_iszero(&TMP, &iszero); EG(ret, err);
		if (!iszero) {
			ext_printf("Error: qC is not 0! (regular bind mul)\n");
			ret = -1;
			goto err;
		}
		/**/
		ret = prj_pt_mul(&TMP, &(curve_params.ec_gen_order), &D); EG(ret, err);
		ret = prj_pt_iszero(&TMP, &iszero); EG(ret, err);
		if (!iszero) {
			ext_printf("Error: qD is not 0! (regular mul)\n");
			ret = -1;
			goto err;
		}
		/**/
		ret = prj_pt_mul_blind(&TMP, &(curve_params.ec_gen_order), &D); EG(ret, err);
		ret = prj_pt_iszero(&TMP, &iszero); EG(ret, err);
		if (!iszero) {
			ext_printf("Error: qD is not 0! (regular blind mul)\n");
			ret = -1;
			goto err;
		}
	}
	/* Let's do some performance tests for point addition and doubling!
	 * We compute kA many times to have a decent performance measurement,
	 * where k is chose random at each iteration. We also check that kA
	 * is indeed on the curve.
	 */
	ret = nn_init(&nn_k, 0); EG(ret, err);
	/**/
	if (get_ms_time(&t1)) {
		ext_printf("Error: cannot get time with get_ms_time\n");
		ret = -1;
		goto err;
	}
	for (i = 0; i < PERF_SCALAR_MUL; i++) {
		/* k = random mod (q) */
		ret = nn_get_random_mod(&nn_k, &(curve_params.ec_gen_order)); EG(ret, err);
		/* Compute kA with montgomery implementation w/o blinding */
		ret = prj_pt_mul(&TMP, &nn_k, &A); EG(ret, err);
		ret = prj_pt_to_aff(&T, &TMP); EG(ret, err);
		ret = prj_pt_is_on_curve(&TMP, &is_oncurve); EG(ret, err);
		if (!is_oncurve) {
			ext_printf("Error: kA is not on the %s curve!\n",
				   curve_params.curve_name);
			nn_print("k=", &nn_k);
			ret = -1;
			goto err;
		}
		ret = aff_pt_is_on_curve(&T, &is_oncurve); EG(ret, err);
		if (!is_oncurve) {
			ext_printf("Error: kA is not on the %s curve!\n",
				   curve_params.curve_name);
			nn_print("k=", &nn_k);
			ret = -1;
			goto err;
		}
	}
	if (get_ms_time(&t2)) {
		ext_printf("Error: cannot get time with get_ms_time\n");
		ret = -1;
		goto err;
	}
	ext_printf("  [*] Regular EC scalar multiplication took %f seconds "
		   "on average\n",
		   (double)(t2 - t1) / (double)(PERF_SCALAR_MUL * 1000ULL));
	/**/
	if (get_ms_time(&t1)) {
		ext_printf("Error: cannot get time with get_ms_time\n");
		ret = -1;
		goto err;
	}
	for (i = 0; i < PERF_SCALAR_MUL; i++) {
		/* k = random mod (q) */
		ret = nn_get_random_mod(&nn_k, &(curve_params.ec_gen_order)); EG(ret, err);
		/* Compute kA using montgomery implementation w/ blinding */
		ret = prj_pt_mul_blind(&TMP, &nn_k, &A); EG(ret, err);
		ret = prj_pt_to_aff(&T, &TMP); EG(ret, err);
		ret = prj_pt_is_on_curve(&TMP, &is_oncurve); EG(ret, err);
		if (!is_oncurve) {
			ext_printf("Error: kA is not on the %s curve!\n",
				   curve_params.curve_name);
			nn_print("k=", &nn_k);
			ret = -1;
			goto err;
		}
		ret = aff_pt_is_on_curve(&T, &is_oncurve); EG(ret, err);
		if (!is_oncurve) {
			ext_printf("Error: kA is not on the %s curve!\n",
				   curve_params.curve_name);
			nn_print("k=", &nn_k);
			ret = -1;
			goto err;
		}
	}
	if (get_ms_time(&t2)) {
		ext_printf("Error: cannot get time with get_ms_time\n");
		ret = -1;
		goto err;
	}
	ext_printf("  [*] Regular blind EC scalar multiplication took %f seconds "
		   "on average\n",
		   (double)(t2 - t1) / (double)(PERF_SCALAR_MUL * 1000ULL));

err:
	prj_pt_uninit(&A);
	prj_pt_uninit(&B);
	prj_pt_uninit(&C);
	prj_pt_uninit(&D);
	prj_pt_uninit(&TMP);
	aff_pt_uninit(&T);
	nn_uninit(&nn_k);

	return ret;
}

#ifdef CURVE_BASIC_EXAMPLES
int main(int argc, char *argv[])
{
	unsigned int i;
	u8 curve_name[MAX_CURVE_NAME_LEN] = { 0 };
	FORCE_USED_VAR(argc);
	FORCE_USED_VAR(argv);

	/* Traverse all the possible curves we have at our disposal (known curves and
	 * user defined curves).
	 */
	for (i = 0; i < EC_CURVES_NUM; i++) {
		/* All our possible curves are in ../curves/curves_list.h
		 * We can get the curve name from its internal type.
		 */
		if(ec_get_curve_name_by_type(ec_maps[i].type, curve_name,
					  sizeof(curve_name))){
			ext_printf("Error when treating %s\n", curve_name);
			return -1;
		}
		/* Check our curve! */
		ext_printf("[+] Checking curve %s\n", curve_name);
		if (check_curve(curve_name)) {
			ext_printf("Error: error performing check on "
				   "curve %s\n", curve_name);
			return -1;
		}
	}
	return 0;
}
#endif
