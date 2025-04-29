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
#ifndef __NN_MUL_REDC1_H__
#define __NN_MUL_REDC1_H__
#include <libecc/nn/nn.h>

ATTRIBUTE_WARN_UNUSED_RET int nn_compute_redc1_coefs(nn_t r, nn_t r_square, nn_src_t p_in,
			      word_t *mpinv);
ATTRIBUTE_WARN_UNUSED_RET int nn_mul_redc1(nn_t out, nn_src_t in1, nn_src_t in2, nn_src_t p,
		  word_t mpinv);
ATTRIBUTE_WARN_UNUSED_RET int nn_mod_mul(nn_t out, nn_src_t in1, nn_src_t in2, nn_src_t p);

#endif /* __NN_MUL_REDC1_H__ */
