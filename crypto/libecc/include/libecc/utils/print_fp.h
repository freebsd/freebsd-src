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
#ifndef __PRINT_FP_H__
#define __PRINT_FP_H__

#include <libecc/fp/fp.h>
#include <libecc/utils/print_nn.h>

void fp_ctx_print(const char *msg, fp_ctx_src_t ctx);

void fp_print(const char *msg, fp_src_t a);

void fp_print_all(const char *msg, fp_src_t a);

#endif /* __PRINT_FP_H__ */
