/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 *​ Copyright (c) 2025 The FreeBSD Foundation
 *​
 *​ Portions of this software were developed by
 * Tuukka Pasanen <tuukka.pasanen@ilmi.fi> under sponsorship from
 * the FreeBSD Foundation
 */

#include <stdlib.h>
#include <string.h>
#include "util.h"

#ifndef CLI__SPDXTOOL__SIMPLELICENSING_H
#define CLI__SPDXTOOL__SIMPLELICENSING_H


#ifdef __cplusplus
extern "C" {
#endif

spdxtool_simplelicensing_license_expression_t *
spdxtool_simplelicensing_licenseExpression_new(pkgconf_client_t *client, const char *license);

void
spdxtool_simplelicensing_licenseExpression_free(spdxtool_simplelicensing_license_expression_t *expression);

spdxtool_serialize_value_t *
spdxtool_simplelicensing_licenseExpression_to_object(pkgconf_client_t *client, const char *creation_info, const spdxtool_simplelicensing_license_expression_t *expression);

#ifdef __cplusplus
}
#endif

#endif
