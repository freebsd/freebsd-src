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
#ifndef __EC_SHORTW_H__
#define __EC_SHORTW_H__

#include <libecc/nn/nn.h>
#include <libecc/fp/fp.h>
#include <libecc/fp/fp_add.h>
#include <libecc/fp/fp_mul.h>
#include <libecc/fp/fp_mul_redc1.h>

typedef struct {
	fp a;
	fp b;
	fp a_monty;
#ifndef NO_USE_COMPLETE_FORMULAS
	fp b3;
	fp b_monty;
	fp b3_monty;
#endif
	nn order; /* curve order */
	word_t magic;
} ec_shortw_crv;

typedef ec_shortw_crv *ec_shortw_crv_t;
typedef const ec_shortw_crv *ec_shortw_crv_src_t;

ATTRIBUTE_WARN_UNUSED_RET int ec_shortw_crv_check_initialized(ec_shortw_crv_src_t crv);
ATTRIBUTE_WARN_UNUSED_RET int ec_shortw_crv_init(ec_shortw_crv_t crv, fp_src_t a, fp_src_t b, nn_src_t order);
void ec_shortw_crv_uninit(ec_shortw_crv_t crv);

#endif /* __EC_SHORTW_H__ */
