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
#include "serialize.h"

#ifdef __cplusplus
extern "C" {
#endif

spdxtool_core_agent_t *
spdxtool_core_agent_new(pkgconf_client_t *client, const char *creation_id, const char *name);

void
spdxtool_core_agent_free(spdxtool_core_agent_t *agent);

spdxtool_serialize_value_t *
spdxtool_core_agent_to_object(pkgconf_client_t *client, const spdxtool_core_agent_t *agent);

spdxtool_core_creation_info_t *
spdxtool_core_creation_info_new(pkgconf_client_t *client, const char *agent_id, const char *id, const char *time);

void
spdxtool_core_creation_info_free(spdxtool_core_creation_info_t *creation);

spdxtool_serialize_value_t *
spdxtool_core_creation_info_to_object(pkgconf_client_t *client, const spdxtool_core_creation_info_t *creation);

spdxtool_core_spdx_document_t *
spdxtool_core_spdx_document_new(pkgconf_client_t *client, const char *spdx_id, const char *creation_id, const char *agent_id);

bool
spdxtool_core_spdx_document_is_license(pkgconf_client_t *client, const spdxtool_core_spdx_document_t *spdx, const char *license);

bool
spdxtool_core_spdx_document_add_license(pkgconf_client_t *client, spdxtool_core_spdx_document_t *spdx, const char *license);

bool
spdxtool_core_spdx_document_add_relationship(pkgconf_client_t *client, spdxtool_core_spdx_document_t *spdx, spdxtool_core_relationship_t *relationship);

bool
spdxtool_core_spdx_document_add_package(pkgconf_client_t *client, spdxtool_core_spdx_document_t *spdx, pkgconf_pkg_t *pkg);

bool
spdxtool_core_spdx_document_add_element(pkgconf_client_t *client, spdxtool_core_spdx_document_t *spdx, const char *element);

void
spdxtool_core_spdx_document_free(spdxtool_core_spdx_document_t *spdx);

spdxtool_serialize_value_t *
spdxtool_core_spdx_document_to_object(pkgconf_client_t *client, spdxtool_core_spdx_document_t *spdx);

spdxtool_core_relationship_t *
spdxtool_core_relationship_new(pkgconf_client_t *client, const char *creation_info_id, const char *spdx_id, const char *from, pkgconf_list_t *to, const char *relationship_type);

void
spdxtool_core_relationship_free(spdxtool_core_relationship_t *relationship);

spdxtool_serialize_value_t *
spdxtool_core_relationship_to_object(pkgconf_client_t *client, const spdxtool_core_relationship_t *relationship);

#ifdef __cplusplus
}
#endif

#endif
