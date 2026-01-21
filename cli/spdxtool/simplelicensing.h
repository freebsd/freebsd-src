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
spdxtool_simplelicensing_licenseExpression_new(pkgconf_client_t *client, char *license);

void
spdxtool_simplelicensing_licenseExpression_free(spdxtool_simplelicensing_license_expression_t *expression_struct);


void
spdxtool_simplelicensing_licenseExpression_serialize(pkgconf_client_t *client, pkgconf_buffer_t *buffer, spdxtool_core_spdx_document_t *spdx_struct, bool last);

#ifdef __cplusplus
}
#endif

#endif
