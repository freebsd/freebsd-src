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
#ifndef __FP_MONTGOMERY_H__
#define __FP_MONTGOMERY_H__

#include <libecc/fp/fp.h>
#include <libecc/fp/fp_add.h>
#include <libecc/fp/fp_mul.h>
#include <libecc/fp/fp_mul_redc1.h>

ATTRIBUTE_WARN_UNUSED_RET int fp_add_monty(fp_t out, fp_src_t in1, fp_src_t in2);
ATTRIBUTE_WARN_UNUSED_RET int fp_sub_monty(fp_t out, fp_src_t in1, fp_src_t in2);
ATTRIBUTE_WARN_UNUSED_RET int fp_mul_monty(fp_t out, fp_src_t in1, fp_src_t in2);
ATTRIBUTE_WARN_UNUSED_RET int fp_sqr_monty(fp_t out, fp_src_t in);
ATTRIBUTE_WARN_UNUSED_RET int fp_div_monty(fp_t out, fp_src_t in1, fp_src_t in2);

#endif /* __FP_MONTGOMERY_H__ */
