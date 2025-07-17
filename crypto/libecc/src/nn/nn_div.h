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
#ifndef __NN_DIV_H__
#define __NN_DIV_H__
#include <libecc/nn/nn.h>
#include <libecc/nn/nn_div_public.h>

/* Compute quotient q and remainder r for given a and b such that a = q*b + r */
/* ATTRIBUTE_WARN_UNUSED_RET int nn_divrem(nn_t q, nn_t r, nn_src_t a, nn_src_t b); (declared in public header) */
ATTRIBUTE_WARN_UNUSED_RET int nn_divrem_notrim(nn_t q, nn_t r, nn_src_t a, nn_src_t b);
ATTRIBUTE_WARN_UNUSED_RET int nn_divrem_unshifted(nn_t q, nn_t r, nn_src_t a, nn_src_t b, word_t v,
			 bitcnt_t cnt);
ATTRIBUTE_WARN_UNUSED_RET int nn_divrem_normalized(nn_t q, nn_t r, nn_src_t a, nn_src_t b, word_t v);

/* Compute r = a mod b */
/* ATTRIBUTE_WARN_UNUSED_RET int nn_mod(nn_t r, nn_src_t a, nn_src_t b); (declared in public header) */
ATTRIBUTE_WARN_UNUSED_RET int nn_mod_notrim(nn_t r, nn_src_t a, nn_src_t b);
ATTRIBUTE_WARN_UNUSED_RET int nn_mod_unshifted(nn_t r, nn_src_t a, nn_src_t b, word_t v, bitcnt_t cnt);
ATTRIBUTE_WARN_UNUSED_RET int nn_mod_normalized(nn_t r, nn_src_t a, nn_src_t b, word_t v);

/* Compute floor(B^3/(d+1)) - B. */
ATTRIBUTE_WARN_UNUSED_RET int wreciprocal(word_t dh, word_t dl, word_t *reciprocal);
ATTRIBUTE_WARN_UNUSED_RET int nn_compute_div_coefs(nn_t p_normalized, word_t *p_shift,
			  word_t *p_reciprocal, nn_src_t p_in);

/* Compute gcd of a and b */
/* ATTRIBUTE_WARN_UNUSED_RET int nn_gcd(nn_t d, nn_src_t a, nn_src_t b, int *sign); (declared in public header) */
/* ATTRIBUTE_WARN_UNUSED_RET int nn_xgcd(nn_t g, nn_t u, nn_t v, nn_src_t a, nn_src_t b, int *sign); (declared in public header) */

#endif /* __NN_DIV_H__ */
