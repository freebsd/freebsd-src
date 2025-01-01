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
#ifndef __NN_RAND_H__
#define __NN_RAND_H__
#include <libecc/nn/nn.h>

ATTRIBUTE_WARN_UNUSED_RET int nn_get_random_len(nn_t out, u16 len);
ATTRIBUTE_WARN_UNUSED_RET int nn_get_random_maxlen(nn_t out, u16 max_len);
ATTRIBUTE_WARN_UNUSED_RET int nn_get_random_mod(nn_t out, nn_src_t q);

#endif /* __NN_RAND_H__ */
