/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 *​ Copyright (c) 2025 The FreeBSD Foundation
 *​
 *​ Portions of this software were developed by
 * Tuukka Pasanen <tuukka.pasanen@ilmi.fi> under sponsorship from
 * the FreeBSD Foundation
 */

#ifndef CLI__SPDXTOOL__UTIL_H
#define CLI__SPDXTOOL__UTIL_H

#include <libpkgconf/libpkgconf.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct spdxtool_core_agent_
{
	int refcount;
	char *spdx_id;
	char *type;
	char *creation_info;
	char *name;
} spdxtool_core_agent_t;

typedef struct spdxtool_core_creation_info_
{
	int refcount;
	char *id;
	char *type;
	char *created;
	char *created_by;
	char *created_using;
	const char *spec_version;
} spdxtool_core_creation_info_t;

typedef struct spdxtool_core_spdx_document
{
	int refcount;
	char *type;
	char *spdx_id;
	char *creation_info;
	char *agent;
	pkgconf_list_t licenses;
	pkgconf_list_t element;
	pkgconf_list_t rootElement;
} spdxtool_core_spdx_document_t;

typedef struct spdxtool_software_sbom_
{
	int refcount;
	char *spdx_id;
	char *type;
	char *creation_info;
	char *sbom_type;
	spdxtool_core_spdx_document_t *spdx_document;
	pkgconf_pkg_t *rootElement;
} spdxtool_software_sbom_t;

typedef struct spdxtool_simplelicensing_license_expression_
{
	int refcount;
	char *type;
	char *spdx_id;
	char *license_expression;
} spdxtool_simplelicensing_license_expression_t;

typedef struct spdxtool_core_relationship_
{
	int refcount;
	char *type;
	char *spdx_id;
	char *creation_info;
	char *from;
	char *to;
	char *relationship_type;
} spdxtool_core_relationship_t;

void
spdxtool_util_set_key(pkgconf_client_t *client, const char *key, const char *key_value, const char *key_default);

void
spdxtool_util_set_uri_root(pkgconf_client_t *client, const char *uri_root);

const char *
spdxtool_util_get_uri_root(pkgconf_client_t *client);

void
spdxtool_util_set_spdx_version(pkgconf_client_t *client, const char *spdx_version);

const char *
spdxtool_util_get_spdx_version(pkgconf_client_t *client);

void
spdxtool_util_set_spdx_license(pkgconf_client_t *client, const char *spdx_license);

const char *
spdxtool_util_get_spdx_license(pkgconf_client_t *client);

char *
spdxtool_util_get_spdx_id_int(pkgconf_client_t *client, char *part);

char *
spdxtool_util_get_spdx_id_string(pkgconf_client_t *client, char *part, char *string_id);

char *
spdxtool_util_get_iso8601_time(time_t *wanted_time);

char *
spdxtool_util_get_current_iso8601_time(void);

char *
spdxtool_util_string_correction(char *str);

#ifdef __cplusplus
}
#endif

#endif
