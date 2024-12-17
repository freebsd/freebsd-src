/*
 *  Copyright (C) 2021 - This file is part of libecc project
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
#ifndef __NN_MOD_POW_H__
#define __NN_MOD_POW_H__
#include <libecc/nn/nn.h>

ATTRIBUTE_WARN_UNUSED_RET int nn_mod_pow_redc(nn_t out, nn_src_t base, nn_src_t exp, nn_src_t mod, nn_src_t r, nn_src_t r_square, word_t mpinv);
ATTRIBUTE_WARN_UNUSED_RET int nn_mod_pow(nn_t out, nn_src_t base, nn_src_t exp, nn_src_t mod);

#endif /* __NN_MOD_POW_H__ */
