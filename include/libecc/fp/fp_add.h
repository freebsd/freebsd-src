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
#ifndef __FP_ADD_H__
#define __FP_ADD_H__
#include <libecc/fp/fp.h>

ATTRIBUTE_WARN_UNUSED_RET int fp_add(fp_t out, fp_src_t in1, fp_src_t in2);
ATTRIBUTE_WARN_UNUSED_RET int fp_inc(fp_t out, fp_src_t in);
ATTRIBUTE_WARN_UNUSED_RET int fp_sub(fp_t out, fp_src_t in1, fp_src_t in2);
ATTRIBUTE_WARN_UNUSED_RET int fp_dec(fp_t out, fp_src_t in);
ATTRIBUTE_WARN_UNUSED_RET int fp_neg(fp_t out, fp_src_t in);

#endif /* __FP_ADD_H__ */
