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
#include <libecc/libarith.h>

/* Declare our Miller-Rabin test implemented
 * in another module.
 */
ATTRIBUTE_WARN_UNUSED_RET int miller_rabin(nn_src_t n, const unsigned int t, int *check);

#ifdef FP_EXAMPLE
int main(int argc, char *argv[])
{
	nn p;
	fp x, x_sqrt1, x_sqrt2;
	fp_ctx ctx;
	int ret, ret_sqr, isone, check, cmp;
	x.magic = x_sqrt1.magic = x_sqrt2.magic = WORD(0);
	ctx.magic = WORD(0);
	FORCE_USED_VAR(argc);
 	FORCE_USED_VAR(argv);

	while (1) {
		/* Get a random prime p of maximum 521 bits */
		ret = nn_init(&p, 0); EG(ret, err);
		while (1) {
			/* x = random with max size ~= (NN_MAX_BIT_LEN / 3) bytes.
			 * This size limit is infered from the NN arithmetic primitives
			 * maximum working size. See nn.h for more information about this.
			 */
			ret = nn_get_random_maxlen
			    (&p, (u16)((NN_MAX_BIT_LEN / 3) / 8)); EG(ret, err);

			/* p = 1 is a marginal prime we don't want to deal with */
			ret = nn_isone(&p, &isone); EG(ret, err);
			if(isone){
				continue;
			}

			/* Check primality of p, and choose it if it is prime */
			ret = miller_rabin(&p, 100, &check); EG(ret, err);
			if(check == 1){
				break;
			}
		}
		nn_print("Prime p", &p);
		/* Initialize our Fp context from p */
		ret = fp_ctx_init_from_p(&ctx, &p); EG(ret, err);
		/* Initialize x and its square roots */
		ret = fp_init(&x, &ctx); EG(ret, err);
		ret = fp_init(&x_sqrt1, &ctx); EG(ret, err);
		ret = fp_init(&x_sqrt2, &ctx); EG(ret, err);

		/* Get a random value in Fp */
		ret = fp_get_random(&x, &ctx); EG(ret, err);
		/* Compute its square in Fp */
		ext_printf("Random before squaring:\n");
		fp_print("x", &x);
		ext_printf("Random after squaring:\n");
		ret = fp_sqr(&x, &x); EG(ret, err);
		nn_print("x^2", &(x.fp_val));

		ret_sqr = fp_sqrt(&x_sqrt1, &x_sqrt2, &x);

		if (ret_sqr == 0) {
			/* Square roots found!, check them! */
			fp_print("sqrt1", &x_sqrt1);
			ret = fp_sqr(&x_sqrt1, &x_sqrt1); EG(ret, err);
			ret = fp_cmp(&x, &x_sqrt1, &cmp); EG(ret, err);
			if (cmp == 0) {
				ext_printf("First found square OK!\n");
			} else {
				ext_printf("First found square NOK: square "
					   "is not the expected value ...\n");
			}
			fp_print("sqrt2", &x_sqrt2);
			ret = fp_sqr(&x_sqrt2, &x_sqrt2); EG(ret, err);
			ret = fp_cmp(&x, &x_sqrt2, &cmp); EG(ret, err);
			if (cmp == 0) {
				ext_printf("Second found square OK!\n");
			} else {
				ext_printf("Second found square NOK: square "
					   "is not the expected value ...\n");
			}

		} else {
			if (ret_sqr == -1) {
				/* This should not happen since we have forged our square */
				ext_printf("Value n has no square over Fp\n");
				ext_printf("(Note: this error can be due to "
					   "Miller-Rabin providing a false "
					   "positive prime ...)\n");
				ext_printf("(though this should happen with "
					   "negligible probability))\n");
				nn_print("Check primality of p =", &p);
				/* Get out of the main loop */
				break;
			} else {
				/* This should not happen since we have forged our square */
				ext_printf("Tonelli-Shanks algorithm unkown "
					   "error ...\n");
				ext_printf("(Note: this error can be due to "
					   "Miller-Rabin providing a false "
					   "positive prime ...)\n");
				ext_printf("(though this should happen with "
					   "negligible probability))\n");
				nn_print("Check primality of p =", &p);
				/* Get out of the main loop */
				break;
			}
		}
	}

	return 0;
err:
	ext_printf("Error: unkown error ...\n");
	return -1;
}
#endif
