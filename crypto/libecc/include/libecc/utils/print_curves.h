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
#ifndef __PRINT_CURVES_H__
#define __PRINT_CURVES_H__

#include <libecc/curves/curves.h>
#include <libecc/utils/print_fp.h>

void ec_point_print(const char *msg, prj_pt_src_t prj_pt);

void ec_montgomery_point_print(const char *msg, aff_pt_montgomery_src_t pt);

void ec_edwards_point_print(const char *msg, aff_pt_edwards_src_t pt);

#endif /* __PRINT_CURVES_H__ */
