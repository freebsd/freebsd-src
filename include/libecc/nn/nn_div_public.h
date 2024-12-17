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
#ifndef __NN_DIV_PUBLIC_H__
#define __NN_DIV_PUBLIC_H__
#include <libecc/nn/nn.h>

/* Compute quotient q and remainder r for given a and b such that a = q*b + r */
ATTRIBUTE_WARN_UNUSED_RET int nn_divrem(nn_t q, nn_t r, nn_src_t a, nn_src_t b);

/* Compute r = a mod b */
ATTRIBUTE_WARN_UNUSED_RET int nn_mod(nn_t r, nn_src_t a, nn_src_t b);

/* Compute gcd of a and b */
ATTRIBUTE_WARN_UNUSED_RET int nn_gcd(nn_t d, nn_src_t a, nn_src_t b, int *sign);
ATTRIBUTE_WARN_UNUSED_RET int nn_xgcd(nn_t g, nn_t u, nn_t v, nn_src_t a, nn_src_t b, int *sign);

#endif /* __NN_DIV_PUBLIC_H__ */
