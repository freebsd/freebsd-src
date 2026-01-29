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
spdxtool_software_sbom_new(pkgconf_client_t *client, char *spdx_id, char *creation_id, char *sbom_type);

void
spdxtool_software_sbom_free(spdxtool_software_sbom_t *sbom_struct);

void
spdxtool_software_sbom_serialize(pkgconf_client_t *client, pkgconf_buffer_t *buffer, spdxtool_software_sbom_t *sbom_struct, bool last);

void
spdxtool_software_package_serialize(pkgconf_client_t *client, pkgconf_buffer_t *buffer, pkgconf_pkg_t *pkg, bool last);

#ifdef __cplusplus
}
#endif

#endif
