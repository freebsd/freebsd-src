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
#ifndef __NN_MODINV_H__
#define __NN_MODINV_H__
#include <libecc/nn/nn.h>

ATTRIBUTE_WARN_UNUSED_RET int nn_modinv(nn_t out, nn_src_t x, nn_src_t m);
ATTRIBUTE_WARN_UNUSED_RET int nn_modinv_2exp(nn_t out, nn_src_t in, bitcnt_t exp, int *in_isodd);
ATTRIBUTE_WARN_UNUSED_RET int nn_modinv_word(nn_t out, word_t w, nn_src_t m);
ATTRIBUTE_WARN_UNUSED_RET int nn_modinv_fermat(nn_t out, nn_src_t x, nn_src_t p);
ATTRIBUTE_WARN_UNUSED_RET int nn_modinv_fermat_redc(nn_t out, nn_src_t x, nn_src_t p, nn_src_t r, nn_src_t r_square, word_t mpinv);

#endif /* __NN_MODINV_H__ */
