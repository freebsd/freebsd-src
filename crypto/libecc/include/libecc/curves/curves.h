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
#ifndef __CURVES_H__
#define __CURVES_H__

#include <libecc/curves/ec_params.h>

ATTRIBUTE_WARN_UNUSED_RET int ec_get_curve_params_by_name(const u8 *ec_name, u8 ec_name_len,
				const ec_str_params **ec_params);
ATTRIBUTE_WARN_UNUSED_RET int ec_get_curve_params_by_type(ec_curve_type ec_type,
				const ec_str_params **ec_params);
ATTRIBUTE_WARN_UNUSED_RET int ec_get_curve_type_by_name(const u8 *ec_name, u8 ec_name_len,
			      ec_curve_type *ec_type);
ATTRIBUTE_WARN_UNUSED_RET int ec_get_curve_name_by_type(const ec_curve_type ec_type, u8 *out, u8 outlen);
ATTRIBUTE_WARN_UNUSED_RET int ec_check_curve_type_and_name(const ec_curve_type ec_type,
				 const u8 *ec_name, u8 ec_name_len);

#endif /* __CURVES_H__ */
