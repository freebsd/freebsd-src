/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 *​ Copyright (c) 2025 The FreeBSD Foundation
 *​
 *​ Portions of this software were developed by
 * Tuukka Pasanen <tuukka.pasanen@ilmi.fi> under sponsorship from
 * the FreeBSD Foundation
 */

#include <libpkgconf/libpkgconf.h>
#include <libpkgconf/stdinc.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

/*
 * !doc
 *
 * .. c:function:: void spdxtool_util_set_key(pkgconf_client_t *client, const char *key, const char *key_value, const char *key_default)
 *
 *    Set key wit default value. Default value is used if key is NULL
 *
 *    :param pkgconf_client_t* client: The pkgconf client being accessed.
 *    :param const char *key: Key to be preserved
 *    :param const char *key_value: Value for key
 *    :param const char *key_default: Default value if key_value is NULL
 *    :return: nothing
 */
void
spdxtool_util_set_key(pkgconf_client_t *client, const char *key, const char *key_value, const char *key_default)
{
	PKGCONF_TRACE(client, "set uri_root to: %s", key_value != NULL ? key_value : key_default);
	pkgconf_tuple_add_global(client, key, key_value != NULL ? key_value : key_default);
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_util_set_uri_root(pkgconf_client_t *client, const char *uri_root)
 *
 *    Set URI/URL root for spdxId. Type for this is 'xsd:anyURI' which means
 *    it can they may be absolute or relative.
 *
 *    :param pkgconf_client_t* client: The pkgconf client being accessed.
 *    :param const char *uri_root: URI root.
 *    :return: nothing
 */
void
spdxtool_util_set_uri_root(pkgconf_client_t *client, const char *uri_root)
{
	spdxtool_util_set_key(client, "spdx_uri_root", uri_root, "https://github.com/pkgconf/pkgconf");
}

/*
 * !doc
 *
 * .. c:function:: const char *spdxtool_util_get_uri_root(pkgconf_client_t *client)
 *
 *    Get current URI
 *
 *    :param pkgconf_client_t* client: The pkgconf client being accessed.
 *    :return: URI or NULL
 */
const char *
spdxtool_util_get_uri_root(pkgconf_client_t *client)
{
	return pkgconf_tuple_find_global(client, "spdx_uri_root");
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_util_set_spdx_version(pkgconf_client_t *client, const char *spdx_version)
 *
 *    Set current SPDX SBOM Spec version. If not set it's 3.0.1
 *
 *    :param pkgconf_client_t* client: The pkgconf client being accessed.
 *    :param char* spdx_version: SPDX specification version.
 *    :return: nothing
 */
void
spdxtool_util_set_spdx_version(pkgconf_client_t *client, const char *spdx_version)
{
	spdxtool_util_set_key(client, "spdx_version", spdx_version, "3.0.1");
}

/*
 * !doc
 *
 * .. c:function:: const char *spdxtool_util_get_spdx_version(pkgconf_client_t *client)
 *
 *    Get SPDX SBOM specification version in use
 *
 *    :param pkgconf_client_t* client: The pkgconf client being accessed.
 *    :return: Spec version
 */
const char *
spdxtool_util_get_spdx_version(pkgconf_client_t *client)
{
	return pkgconf_tuple_find_global(client, "spdx_version");
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_util_set_spdx_license(pkgconf_client_t *client, const char *spdx_license)
 *
 *    Under which license SBOM is released. License string should be in
 *    SPDX style. Something like: FreeBSD-DOC, CC0-1.0 or MIT
 *
 *    :param pkgconf_client_t* client: The pkgconf client being accessed.
 *    :param const char *spdx_license: SPDX compatible license
 *    :return: nothing
 */
void
spdxtool_util_set_spdx_license(pkgconf_client_t *client, const char *spdx_license)
{
	spdxtool_util_set_key(client, "spdx_license", spdx_license, "CC0-1.0");
}

/*
 * !doc
 *
 * .. c:function:: const char *spdxtool_util_get_spdx_license(pkgconf_client_t *client)
 *
 *    Get license which SBOM is release in
 *
 *    :param pkgconf_client_t* client: The pkgconf client being accessed.
 *    :return: SPDX license
 */
const char *
spdxtool_util_get_spdx_license(pkgconf_client_t *client)
{
	return pkgconf_tuple_find_global(client, "spdx_license");
}

static size_t last_id = 0;

/*
 * !doc
 *
 * .. c:function:: char *spdxtool_util_get_spdx_id_int(pkgconf_client_t *client, char *part)
 *
 *    Get spdxId with current URI.
 *    URI is lookg like https://test/part/1
 *
 *    :param pkgconf_client_t* client: The pkgconf client being accessed.
 *    :param char *part: Addition subdir part before number
 *    :return: URI
 */
char *
spdxtool_util_get_spdx_id_int(pkgconf_client_t *client, char *part)
{
	const 	char *global_xsd_any_uri = spdxtool_util_get_uri_root(client);
	pkgconf_buffer_t current_uri = PKGCONF_BUFFER_INITIALIZER;

	pkgconf_buffer_join(&current_uri, '/', global_xsd_any_uri, part, NULL);
	pkgconf_buffer_append_fmt(&current_uri, "/" SIZE_FMT_SPECIFIER, ++last_id);

	return pkgconf_buffer_freeze(&current_uri);
}

/*
 * !doc
 *
 * .. c:function:: char *spdxtool_util_get_spdx_id_string(pkgconf_client_t *client, char *part, char *string_id)
 *
 *    Get string id URI
 *    looks something like: https://test/part/string_id
 *
 *    :param pkgconf_client_t* client: The pkgconf client being accessed.
 *    :param char* part: subdir part of URI.
 *    :param char* string_id: String ID.
 *    :return: URI
 */
char *
spdxtool_util_get_spdx_id_string(pkgconf_client_t *client, char *part, char *string_id)
{
	const 	char *global_xsd_any_uri = spdxtool_util_get_uri_root(client);
	pkgconf_buffer_t current_uri = PKGCONF_BUFFER_INITIALIZER;

	pkgconf_buffer_join(&current_uri, '/', global_xsd_any_uri, part, string_id, NULL);

	return pkgconf_buffer_freeze(&current_uri);
}

/*
 * !doc
 *
 * .. c:function:: char *spdxtool_util_get_iso8601_time(time_t *wanted_time)
 *
 *    Get ISO8601 time string
 *
 *    :param time_t *wanted_time: Time to be converted
 *    :return: Time string in ISO8601 format
 */
char *
spdxtool_util_get_iso8601_time(time_t *wanted_time)
{
	char buf[PKGCONF_ITEM_SIZE];
	struct tm *tm_info = gmtime(wanted_time);

	if(!wanted_time)
		return NULL;

	/* ISO8601 time with Z at the end */
	strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", tm_info);
	return strdup(buf);
}

/*
 * !doc
 *
 * .. c:function:: char *spdxtool_util_get_current_iso8601_time()
 *
 *    Get ISO8601 current timestamp
 *
 *    :return: Time string in ISO8601 format
 */
char *
spdxtool_util_get_current_iso8601_time()
{
	time_t now;
	time(&now);
	return spdxtool_util_get_iso8601_time(&now);
}

/*
 * !doc
 *
 * .. c:function:: char *spdxtool_util_string_correction(char *str)
 *
 *    Lowercase string and change spaces to '_'
 *
 *    :param char *str: String to be converted
 *    :return: Converted string
 */
char *
spdxtool_util_string_correction(char *str)
{
	char *ptr = str;
	/* Lowecase string  and make spaces '_' */
	for ( ; *ptr; ++ptr)
	{
		*ptr = tolower(*ptr);
		if(isspace((unsigned char) *ptr))
		{
			*ptr = '_';
		}
	}

	return str;
}
