/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 *​ Copyright (c) 2025 The FreeBSD Foundation
 *​
 *​ Portions of this software were developed by
 * Tuukka Pasanen <tuukka.pasanen@ilmi.fi> under sponsorship from
 * the FreeBSD Foundation
 */

#ifndef CLI__SPDXTOOL_SOFTWARE_H
#define CLI__SPDXTOOL_SOFTWARE_H

#include <stdlib.h>
#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

spdxtool_software_sbom_t *
spdxtool_software_sbom_new(pkgconf_client_t *client, const char *spdx_id, const char *creation_id, const char *sbom_type);

void
spdxtool_software_sbom_free(spdxtool_software_sbom_t *sbom);

spdxtool_serialize_value_t *
spdxtool_software_package_to_object(pkgconf_client_t *client, pkgconf_pkg_t *pkg, spdxtool_core_spdx_document_t *doc);

spdxtool_serialize_value_t *
spdxtool_software_sbom_to_object(pkgconf_client_t *client, spdxtool_software_sbom_t *sbom);

#ifdef __cplusplus
}
#endif

#endif
