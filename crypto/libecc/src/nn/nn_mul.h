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
#ifndef __NN_MUL_H__
#define __NN_MUL_H__
#include <libecc/nn/nn.h>
#include <libecc/nn/nn_mul_public.h>

ATTRIBUTE_WARN_UNUSED_RET int nn_mul_low(nn_t out, nn_src_t in1, nn_src_t in2, u8 wlimit);
ATTRIBUTE_WARN_UNUSED_RET int nn_sqr_low(nn_t out, nn_src_t in, u8 wlimit);
/* (declared in public header)
ATTRIBUTE_WARN_UNUSED_RET int nn_mul(nn_t out, nn_src_t in1, nn_src_t in2);
ATTRIBUTE_WARN_UNUSED_RET int nn_sqr(nn_t out, nn_src_t in);
ATTRIBUTE_WARN_UNUSED_RET int nn_mul_word(nn_t out, nn_src_t in, word_t w);
*/

#endif /* __NN_MUL_H__ */
