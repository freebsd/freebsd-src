/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 *​ Copyright (c) 2025 The FreeBSD Foundation
 *​
 *​ Portions of this software were developed by
 * Tuukka Pasanen <tuukka.pasanen@ilmi.fi> under sponsorship from
 * the FreeBSD Foundation
 */

#ifndef CLI__SPDXTOOL__CORE_H
#define CLI__SPDXTOOL__CORE_H

#include <stdlib.h>
#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

spdxtool_core_agent_t *
spdxtool_core_agent_new(pkgconf_client_t *client, char *creation_id, char *name);

void
spdxtool_core_agent_free(spdxtool_core_agent_t *agent_struct);

void
spdxtool_core_agent_serialize(pkgconf_client_t *client, pkgconf_buffer_t *buffer, spdxtool_core_agent_t *agent_struct, bool last);

spdxtool_core_creation_info_t *
spdxtool_core_creation_info_new(pkgconf_client_t *client, char *agent_id, char *id, char *time);

void
spdxtool_core_creation_info_free(spdxtool_core_creation_info_t *creation_struct);

void
spdxtool_core_creation_info_serialize(pkgconf_client_t *client, pkgconf_buffer_t *buffer, spdxtool_core_creation_info_t *creation_struct, bool last);

spdxtool_core_spdx_document_t *
spdxtool_core_spdx_document_new(pkgconf_client_t *client, char *spdx_id, char *creation_id);

bool
spdxtool_core_spdx_document_is_license(pkgconf_client_t *client, spdxtool_core_spdx_document_t *spdx_struct, char *license);


void
spdxtool_core_spdx_document_add_license(pkgconf_client_t *client, spdxtool_core_spdx_document_t *spdx_struct, char *license);

void
spdxtool_core_spdx_document_add_element(pkgconf_client_t *client, spdxtool_core_spdx_document_t *spdx_struct, char *element);


void
spdxtool_core_spdx_document_free(spdxtool_core_spdx_document_t *spdx_struct);

void
spdxtool_core_spdx_document_serialize(pkgconf_client_t *client, pkgconf_buffer_t *buffer, spdxtool_core_spdx_document_t *spdx_struct, bool last);


spdxtool_core_relationship_t *
spdxtool_core_relationship_new(pkgconf_client_t *client, char *creation_info_id, char *spdx_id, char *from, char *to, char *relationship_type);

void
spdxtool_core_relationship_free(spdxtool_core_relationship_t *relationship_struct);

void
spdxtool_core_relationship_serialize(pkgconf_client_t *client, pkgconf_buffer_t *buffer, spdxtool_core_relationship_t *relationship_struct, bool last);

#ifdef __cplusplus
}
#endif

#endif
