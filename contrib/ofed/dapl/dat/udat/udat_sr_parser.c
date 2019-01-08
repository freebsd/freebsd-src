/*
 * Copyright (c) 2002-2005, Network Appliance, Inc. All rights reserved.
 *
 * This Software is licensed under one of the following licenses:
 *
 * 1) under the terms of the "Common Public License 1.0" a copy of which is
 *    in the file LICENSE.txt in the root directory. The license is also
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/cpl.php.
 *
 * 2) under the terms of the "The BSD License" a copy of which is in the file
 *    LICENSE2.txt in the root directory. The license is also available from
 *    the Open Source Initiative, see
 *    http://www.opensource.org/licenses/bsd-license.php.
 *
 * 3) under the terms of the "GNU General Public License (GPL) Version 2" a 
 *    copy of which is in the file LICENSE3.txt in the root directory. The 
 *    license is also available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/gpl-license.php.
 *
 * Licensee has the right to choose one of the above licenses.
 *
 * Redistributions of source code must retain the above copyright
 * notice and one of the license notices.
 *
 * Redistributions in binary form must reproduce both the above copyright
 * notice, one of the license notices in the documentation
 * and/or other materials provided with the distribution.
 */

/**********************************************************************
 *
 * MODULE: dat_sr_parser.c
 *
 * PURPOSE: static registry parser
 *
 * $Id: udat_sr_parser.c,v 1.6 2005/03/24 05:58:36 jlentini Exp $
 **********************************************************************/

#include <dat2/udat.h>
#include "udat_sr_parser.h"
#include "dat_sr.h"

/*********************************************************************
 *                                                                   *
 * Constants                                                         *
 *                                                                   *
 *********************************************************************/

#define DAT_SR_CONF_ENV 		"DAT_OVERRIDE"
#if defined(_WIN32) || defined(_WIN64)
#define DAT_SR_CONF_DEFAULT 		"C:\\DAT\\dat.conf"
#else
#define DAT_SR_CONF_DEFAULT 		"/etc/dat.conf"
#endif

#define DAT_SR_TOKEN_THREADSAFE 	"threadsafe"
#define DAT_SR_TOKEN_NONTHREADSAFE 	"nonthreadsafe"
#define DAT_SR_TOKEN_DEFAULT 		"default"
#define DAT_SR_TOKEN_NONDEFAULT 	"nondefault"

#define DAT_SR_CHAR_NEWLINE 		'\n'
#define DAT_SR_CHAR_COMMENT 		'#'
#define DAT_SR_CHAR_QUOTE 		'"'
#define DAT_SR_CHAR_BACKSLASH 		'\\'

/*********************************************************************
 *                                                                   *
 * Enumerations                                                      *
 *                                                                   *
 *********************************************************************/

typedef enum {
	DAT_SR_TOKEN_STRING,	/* text field (both quoted or unquoted) */
	DAT_SR_TOKEN_EOR,	/* end of record (newline)              */
	DAT_SR_TOKEN_EOF	/* end of file                          */
} DAT_SR_TOKEN_TYPE;

typedef enum {
	DAT_SR_API_UDAT,
	DAT_SR_API_KDAT
} DAT_SR_API_TYPE;

/*********************************************************************
 *                                                                   *
 * Structures                                                        *
 *                                                                   *
 *********************************************************************/

typedef struct {
	DAT_SR_TOKEN_TYPE type;
	char *value;		/* valid if type is DAT_SR_TOKEN_STRING */
	DAT_OS_SIZE value_len;
} DAT_SR_TOKEN;

typedef struct DAT_SR_STACK_NODE {
	DAT_SR_TOKEN token;
	struct DAT_SR_STACK_NODE *next;
} DAT_SR_STACK_NODE;

typedef struct {
	DAT_UINT32 major;
	DAT_UINT32 minor;
} DAT_SR_VERSION;

typedef struct {
	char *id;
	DAT_SR_VERSION version;
} DAT_SR_PROVIDER_VERSION;

typedef struct {
	DAT_SR_API_TYPE type;
	DAT_SR_VERSION version;
} DAT_SR_API_VERSION;

typedef struct {
	char *ia_name;
	DAT_SR_API_VERSION api_version;
	DAT_BOOLEAN is_thread_safe;
	DAT_BOOLEAN is_default;
	char *lib_path;
	DAT_SR_PROVIDER_VERSION provider_version;
	char *ia_params;
	char *platform_params;
} DAT_SR_CONF_ENTRY;

/*********************************************************************
 *                                                                   *
 * Internal Function Declarations                                    *
 *                                                                   *
 *********************************************************************/

static DAT_RETURN dat_sr_load_entry(DAT_SR_CONF_ENTRY * entry);

static DAT_BOOLEAN dat_sr_is_valid_entry(DAT_SR_CONF_ENTRY * entry);

static char *dat_sr_type_to_str(DAT_SR_TOKEN_TYPE type);

static DAT_RETURN dat_sr_parse_eof(DAT_OS_FILE * file);

static DAT_RETURN dat_sr_parse_entry(DAT_OS_FILE * file);

static DAT_RETURN
dat_sr_parse_ia_name(DAT_OS_FILE * file, DAT_SR_CONF_ENTRY * entry);

static DAT_RETURN
dat_sr_parse_api(DAT_OS_FILE * file, DAT_SR_CONF_ENTRY * entry);

static DAT_RETURN
dat_sr_parse_thread_safety(DAT_OS_FILE * file, DAT_SR_CONF_ENTRY * entry);

static DAT_RETURN
dat_sr_parse_default(DAT_OS_FILE * file, DAT_SR_CONF_ENTRY * entry);

static DAT_RETURN
dat_sr_parse_lib_path(DAT_OS_FILE * file, DAT_SR_CONF_ENTRY * entry);

static DAT_RETURN
dat_sr_parse_provider_version(DAT_OS_FILE * file, DAT_SR_CONF_ENTRY * entry);

static DAT_RETURN
dat_sr_parse_ia_params(DAT_OS_FILE * file, DAT_SR_CONF_ENTRY * entry);

static DAT_RETURN
dat_sr_parse_platform_params(DAT_OS_FILE * file, DAT_SR_CONF_ENTRY * entry);

static DAT_RETURN
dat_sr_parse_eoe(DAT_OS_FILE * file, DAT_SR_CONF_ENTRY * entry);

static DAT_RETURN
dat_sr_convert_api(char *str, DAT_SR_API_VERSION * api_version);

static DAT_RETURN
dat_sr_convert_thread_safety(char *str, DAT_BOOLEAN * is_thread_safe);

static DAT_RETURN dat_sr_convert_default(char *str, DAT_BOOLEAN * is_default);

static DAT_RETURN
dat_sr_convert_provider_version(char *str,
				DAT_SR_PROVIDER_VERSION * provider_version);

static DAT_RETURN dat_sr_get_token(DAT_OS_FILE * file, DAT_SR_TOKEN * token);

static DAT_RETURN dat_sr_put_token(DAT_OS_FILE * file, DAT_SR_TOKEN * token);

static DAT_RETURN dat_sr_read_token(DAT_OS_FILE * file, DAT_SR_TOKEN * token);

static DAT_RETURN
dat_sr_read_str(DAT_OS_FILE * file,
		DAT_SR_TOKEN * token, DAT_OS_SIZE token_len);

static DAT_RETURN
dat_sr_read_quoted_str(DAT_OS_FILE * file,
		       DAT_SR_TOKEN * token,
		       DAT_OS_SIZE token_len, DAT_COUNT num_escape_seq);

static void dat_sr_read_comment(DAT_OS_FILE * file);

/*********************************************************************
 *                                                                   *
 * Global Variables                                                  *
 *                                                                   *
 *********************************************************************/

static DAT_SR_STACK_NODE *g_token_stack = NULL;

/*********************************************************************
 *                                                                   *
 * External Function Definitions                                     *
 *                                                                   *
 *********************************************************************/

/***********************************************************************
 * Function: dat_sr_load
 ***********************************************************************/

DAT_RETURN dat_sr_load(void)
{
	char *sr_path;
	DAT_OS_FILE *sr_file;

	sr_path = dat_os_getenv(DAT_SR_CONF_ENV);

	/* environment override */
	if ((sr_path != NULL) && ((sr_file = dat_os_fopen(sr_path)) == NULL)) {
		dat_os_dbg_print(DAT_OS_DBG_TYPE_ERROR,
				 "DAT Registry: DAT_OVERRIDE, "
				 "bad filename - %s, aborting\n", sr_path);
		goto bail;
	}

	if (sr_path == NULL) {

#ifdef DAT_CONF
		sr_path = DAT_CONF;
#else
		sr_path = DAT_SR_CONF_DEFAULT;
#endif
		sr_file = dat_os_fopen(sr_path);
		if (sr_file == NULL) {
#ifdef DAT_CONF
			dat_os_dbg_print(DAT_OS_DBG_TYPE_SR,
					 "DAT Registry: sysconfdir, "
					 "bad filename - %s, retry default at %s\n",
					 sr_path, DAT_SR_CONF_DEFAULT);
			/* try default after sysconfdir fails */
			sr_path = DAT_SR_CONF_DEFAULT;
			sr_file = dat_os_fopen(sr_path);
			if (sr_file == NULL) {
#endif
				dat_os_dbg_print(DAT_OS_DBG_TYPE_ERROR,
						 "DAT Registry: default, "
						 "bad filename - %s, aborting\n",
						 sr_path);
				goto bail;
#ifdef DAT_CONF
			}
#endif
		}
	}

	dat_os_dbg_print(DAT_OS_DBG_TYPE_GENERIC,
			 "DAT Registry: using config file %s\n", sr_path);

	for (;;) {
		if (DAT_SUCCESS == dat_sr_parse_eof(sr_file)) {
			break;
		} else if (DAT_SUCCESS == dat_sr_parse_entry(sr_file)) {
			continue;
		} else {
			dat_os_dbg_print(DAT_OS_DBG_TYPE_ERROR,
					 "DAT Registry: ERROR parsing - %s\n",
					 sr_path);
			goto cleanup;
		}
	}

	if (0 != dat_os_fclose(sr_file)) {
		dat_os_dbg_print(DAT_OS_DBG_TYPE_ERROR,
				 "DAT Registry: ERROR closing - %s\n", sr_path);
		goto bail;
	}

	return DAT_SUCCESS;

      cleanup:
	dat_os_fclose(sr_file);
      bail:
	return DAT_INTERNAL_ERROR;

}

/*********************************************************************
 *                                                                   *
 * Internal Function Definitions                                     *
 *                                                                   *
 *********************************************************************/

/***********************************************************************
 * Function: dat_sr_is_valid_entry
 ***********************************************************************/

DAT_BOOLEAN dat_sr_is_valid_entry(DAT_SR_CONF_ENTRY * entry)
{
	if ((DAT_SR_API_UDAT == entry->api_version.type) && (entry->is_default)) {
		return DAT_TRUE;
	} else {
		return DAT_FALSE;
	}
}

/***********************************************************************
 * Function: dat_sr_load_entry
 ***********************************************************************/

DAT_RETURN dat_sr_load_entry(DAT_SR_CONF_ENTRY * conf_entry)
{
	DAT_SR_ENTRY entry;

	if (DAT_NAME_MAX_LENGTH < (strlen(conf_entry->ia_name) + 1)) {
		dat_os_dbg_print(DAT_OS_DBG_TYPE_SR,
				 "DAT Registry: ia name %s is longer than "
				 "DAT_NAME_MAX_LENGTH (%i)\n",
				 conf_entry->ia_name, DAT_NAME_MAX_LENGTH);

		return DAT_INSUFFICIENT_RESOURCES;
	}

	dat_os_strncpy(entry.info.ia_name, conf_entry->ia_name,
		       DAT_NAME_MAX_LENGTH);
	entry.info.dapl_version_major = conf_entry->api_version.version.major;
	entry.info.dapl_version_minor = conf_entry->api_version.version.minor;
	entry.info.is_thread_safe = conf_entry->is_thread_safe;
	entry.lib_path = conf_entry->lib_path;
	entry.ia_params = conf_entry->ia_params;
	entry.lib_handle = NULL;
	entry.ref_count = 0;

	dat_os_dbg_print(DAT_OS_DBG_TYPE_SR,
			 "DAT Registry: loading provider for %s\n",
			 conf_entry->ia_name);

	return dat_sr_insert(&entry.info, &entry);
}

/***********************************************************************
 * Function: dat_sr_type_to_str
 ***********************************************************************/

char *dat_sr_type_to_str(DAT_SR_TOKEN_TYPE type)
{
	static char *str_array[] = { "string", "eor", "eof" };

	if ((type < 0) || (2 < type)) {
		return "error: invalid token type";
	}

	return str_array[type];
}

/***********************************************************************
 * Function: dat_sr_parse_eof
 ***********************************************************************/

DAT_RETURN dat_sr_parse_eof(DAT_OS_FILE * file)
{
	DAT_SR_TOKEN token;

	if (DAT_SUCCESS != dat_sr_get_token(file, &token)) {
		return DAT_INTERNAL_ERROR;
	}

	if (DAT_SR_TOKEN_EOF == token.type) {
		return DAT_SUCCESS;
	} else {
		dat_sr_put_token(file, &token);
		return DAT_INTERNAL_ERROR;
	}
}

/***********************************************************************
 * Function: dat_sr_parse_ia_name
 ***********************************************************************/

DAT_RETURN dat_sr_parse_entry(DAT_OS_FILE * file)
{
	DAT_SR_CONF_ENTRY entry;
	DAT_RETURN status;

	dat_os_memset(&entry, 0, sizeof(DAT_SR_CONF_ENTRY));

	if ((DAT_SUCCESS == dat_sr_parse_ia_name(file, &entry)) &&
	    (DAT_SUCCESS == dat_sr_parse_api(file, &entry)) &&
	    (DAT_SUCCESS == dat_sr_parse_thread_safety(file, &entry)) &&
	    (DAT_SUCCESS == dat_sr_parse_default(file, &entry)) &&
	    (DAT_SUCCESS == dat_sr_parse_lib_path(file, &entry)) &&
	    (DAT_SUCCESS == dat_sr_parse_provider_version(file, &entry)) &&
	    (DAT_SUCCESS == dat_sr_parse_ia_params(file, &entry)) &&
	    (DAT_SUCCESS == dat_sr_parse_platform_params(file, &entry)) &&
	    (DAT_SUCCESS == dat_sr_parse_eoe(file, &entry))) {
		dat_os_dbg_print(DAT_OS_DBG_TYPE_SR,
				 "\n"
				 "DAT Registry: entry \n"
				 " ia_name %s\n"
				 " api_version\n"
				 "     type 0x%X\n"
				 "     major.minor %d.%d\n"
				 " is_thread_safe %d\n"
				 " is_default %d\n"
				 " lib_path %s\n"
				 " provider_version\n"
				 "     id %s\n"
				 "     major.minor %d.%d\n"
				 " ia_params %s\n"
				 "\n",
				 entry.ia_name,
				 entry.api_version.type,
				 entry.api_version.version.major,
				 entry.api_version.version.minor,
				 entry.is_thread_safe,
				 entry.is_default,
				 entry.lib_path,
				 entry.provider_version.id,
				 entry.provider_version.version.major,
				 entry.provider_version.version.minor,
				 entry.ia_params);

		if (DAT_TRUE == dat_sr_is_valid_entry(&entry)) {
			/*
			 * The static registry configuration file may have multiple
			 * entries with the same IA name. The first entry will be
			 * installed in the static registry causing subsequent attempts
			 * to register the same IA name to fail. Therefore the return code
			 * from dat_sr_load_entry() is ignored.
			 */
			(void)dat_sr_load_entry(&entry);
		}

		status = DAT_SUCCESS;
	} else {		/* resync */

		DAT_SR_TOKEN token;

		/*
		 * The static registry format is specified in the DAT specification.
		 * While the registry file's contents may change between revisions of
		 * the specification, there is no way to determine the specification
		 * version to which the configuration file conforms. If an entry is
		 * found that does not match the expected format, the entry is discarded
		 * and the parsing of the file continues. There is no way to determine if
		 * the entry was an error or an entry confirming to an alternate version
		 * of specification.
		 */

		for (;;) {
			if (DAT_SUCCESS != dat_sr_get_token(file, &token)) {
				status = DAT_INTERNAL_ERROR;
				break;
			}

			if (DAT_SR_TOKEN_STRING != token.type) {
				status = DAT_SUCCESS;
				break;
			} else {
				dat_os_free(token.value,
					    (sizeof(char) *
					     dat_os_strlen(token.value)) + 1);
				continue;
			}
		}
	}

	/* free resources */
	if (NULL != entry.ia_name) {
		dat_os_free(entry.ia_name,
			    sizeof(char) * (dat_os_strlen(entry.ia_name) + 1));
	}
	if (NULL != entry.lib_path) {
		dat_os_free(entry.lib_path,
			    sizeof(char) * (dat_os_strlen(entry.lib_path) + 1));
	}

	if (NULL != entry.provider_version.id) {
		dat_os_free(entry.provider_version.id,
			    sizeof(char) *
			    (dat_os_strlen(entry.provider_version.id) + 1));
	}

	if (NULL != entry.ia_params) {
		dat_os_free(entry.ia_params,
			    sizeof(char) * (dat_os_strlen(entry.ia_params) +
					    1));
	}

	return status;
}

/***********************************************************************
 * Function: dat_sr_parse_ia_name
 ***********************************************************************/

DAT_RETURN dat_sr_parse_ia_name(DAT_OS_FILE * file, DAT_SR_CONF_ENTRY * entry)
{
	DAT_SR_TOKEN token;

	if (DAT_SUCCESS != dat_sr_get_token(file, &token))
		goto bail;

	if (DAT_SR_TOKEN_STRING != token.type) {
		dat_sr_put_token(file, &token);
		goto bail;
	}
	entry->ia_name = token.value;
	return DAT_SUCCESS;

      bail:
	return DAT_INTERNAL_ERROR;
}

/***********************************************************************
 * Function: dat_sr_parse_ia_name
 ***********************************************************************/

DAT_RETURN dat_sr_parse_api(DAT_OS_FILE * file, DAT_SR_CONF_ENTRY * entry)
{
	DAT_SR_TOKEN token;

	if (DAT_SUCCESS != dat_sr_get_token(file, &token))
		goto bail;

	if (DAT_SR_TOKEN_STRING != token.type)
		goto cleanup;

	if (DAT_SUCCESS != dat_sr_convert_api(token.value, &entry->api_version))
		goto cleanup;

	dat_os_free(token.value,
		    (sizeof(char) * dat_os_strlen(token.value)) + 1);
	return DAT_SUCCESS;

      cleanup:
	dat_sr_put_token(file, &token);
      bail:
	dat_os_dbg_print(DAT_OS_DBG_TYPE_ERROR,
			 " ERR: corrupt dat.conf entry field:"
			 " api_ver, file offset=%ld\n", ftell(file));
	return DAT_INTERNAL_ERROR;
}

/***********************************************************************
 * Function: dat_sr_parse_thread_safety
 ***********************************************************************/

static DAT_RETURN
dat_sr_parse_thread_safety(DAT_OS_FILE * file, DAT_SR_CONF_ENTRY * entry)
{
	DAT_SR_TOKEN token;

	if (DAT_SUCCESS != dat_sr_get_token(file, &token))
		goto bail;

	if (DAT_SR_TOKEN_STRING != token.type)
		goto cleanup;

	if (DAT_SUCCESS !=
	    dat_sr_convert_thread_safety(token.value, &entry->is_thread_safe))
		goto cleanup;

	dat_os_free(token.value,
		    (sizeof(char) * dat_os_strlen(token.value)) + 1);
	return DAT_SUCCESS;

      cleanup:
	dat_sr_put_token(file, &token);
      bail:
	dat_os_dbg_print(DAT_OS_DBG_TYPE_ERROR,
			 " ERR: corrupt dat.conf entry field:"
			 " thread_safety, file offset=%ld\n", ftell(file));
	return DAT_INTERNAL_ERROR;
}

/***********************************************************************
 * Function: dat_sr_parse_default
 ***********************************************************************/

DAT_RETURN dat_sr_parse_default(DAT_OS_FILE * file, DAT_SR_CONF_ENTRY * entry)
{
	DAT_SR_TOKEN token;

	if (DAT_SUCCESS != dat_sr_get_token(file, &token))
		goto bail;

	if (DAT_SR_TOKEN_STRING != token.type)
		goto cleanup;

	if (DAT_SUCCESS !=
	    dat_sr_convert_default(token.value, &entry->is_default))
		goto cleanup;

	dat_os_free(token.value,
		    (sizeof(char) * dat_os_strlen(token.value)) + 1);
	return DAT_SUCCESS;

      cleanup:
	dat_sr_put_token(file, &token);
      bail:
	dat_os_dbg_print(DAT_OS_DBG_TYPE_ERROR,
			 " ERR: corrupt dat.conf entry field:"
			 " default section, file offset=%ld\n", ftell(file));
	return DAT_INTERNAL_ERROR;
}

/***********************************************************************
 * Function: dat_sr_parse_lib_path
 ***********************************************************************/

DAT_RETURN dat_sr_parse_lib_path(DAT_OS_FILE * file, DAT_SR_CONF_ENTRY * entry)
{
	DAT_SR_TOKEN token;

	if (DAT_SUCCESS != dat_sr_get_token(file, &token))
		goto bail;

	if (DAT_SR_TOKEN_STRING != token.type) {
		dat_sr_put_token(file, &token);
		goto bail;
	}
	entry->lib_path = token.value;
	return DAT_SUCCESS;

      bail:
	dat_os_dbg_print(DAT_OS_DBG_TYPE_ERROR,
			 " ERR: corrupt dat.conf entry field:"
			 " lib_path, file offset=%ld\n", ftell(file));
	return DAT_INTERNAL_ERROR;
}

/***********************************************************************
 * Function: dat_sr_parse_provider_version
 ***********************************************************************/

DAT_RETURN
dat_sr_parse_provider_version(DAT_OS_FILE * file, DAT_SR_CONF_ENTRY * entry)
{
	DAT_SR_TOKEN token;

	if (DAT_SUCCESS != dat_sr_get_token(file, &token))
		goto bail;

	if (DAT_SR_TOKEN_STRING != token.type)
		goto cleanup;

	if (DAT_SUCCESS !=
	    dat_sr_convert_provider_version(token.value,
					    &entry->provider_version))
		goto cleanup;

	dat_os_free(token.value,
		    (sizeof(char) * dat_os_strlen(token.value)) + 1);
	return DAT_SUCCESS;

      cleanup:
	dat_sr_put_token(file, &token);
      bail:
	dat_os_dbg_print(DAT_OS_DBG_TYPE_ERROR,
			 " ERR: corrupt dat.conf entry field:"
			 " provider_ver, file offset=%ld\n", ftell(file));
	return DAT_INTERNAL_ERROR;
}

/***********************************************************************
 * Function: dat_sr_parse_ia_params
 ***********************************************************************/

DAT_RETURN dat_sr_parse_ia_params(DAT_OS_FILE * file, DAT_SR_CONF_ENTRY * entry)
{
	DAT_SR_TOKEN token;

	if (DAT_SUCCESS != dat_sr_get_token(file, &token))
		goto bail;

	if (DAT_SR_TOKEN_STRING != token.type) {
		dat_sr_put_token(file, &token);
		goto bail;
	}

	entry->ia_params = token.value;
	return DAT_SUCCESS;

      bail:
	dat_os_dbg_print(DAT_OS_DBG_TYPE_ERROR,
			 " ERR: corrupt dat.conf entry field:"
			 " ia_params, file offset=%ld\n", ftell(file));
	return DAT_INTERNAL_ERROR;
}

/***********************************************************************
 * Function: dat_sr_parse_platform_params
 ***********************************************************************/

DAT_RETURN
dat_sr_parse_platform_params(DAT_OS_FILE * file, DAT_SR_CONF_ENTRY * entry)
{
	DAT_SR_TOKEN token;

	if (DAT_SUCCESS != dat_sr_get_token(file, &token))
		goto bail;

	if (DAT_SR_TOKEN_STRING != token.type) {
		dat_sr_put_token(file, &token);
		goto bail;
	}

	entry->platform_params = token.value;
	return DAT_SUCCESS;

      bail:
	dat_os_dbg_print(DAT_OS_DBG_TYPE_ERROR,
			 " ERR: corrupt dat.conf entry field:"
			 " platform_params, file offset=%ld\n", ftell(file));
	return DAT_INTERNAL_ERROR;
}

/***********************************************************************
 * Function: dat_sr_parse_eoe
 ***********************************************************************/

DAT_RETURN dat_sr_parse_eoe(DAT_OS_FILE * file, DAT_SR_CONF_ENTRY * entry)
{
	DAT_SR_TOKEN token;

	if (DAT_SUCCESS != dat_sr_get_token(file, &token))
		goto bail;

	if ((DAT_SR_TOKEN_EOF != token.type) &&
	    (DAT_SR_TOKEN_EOR != token.type)) {
		dat_sr_put_token(file, &token);
		goto bail;
	}

	return DAT_SUCCESS;

      bail:
	dat_os_dbg_print(DAT_OS_DBG_TYPE_ERROR,
			 " ERR: corrupt dat.conf entry field:"
			 " EOR, EOF, file offset=%ld\n", ftell(file));
	return DAT_INTERNAL_ERROR;
}

/***********************************************************************
 * Function: dat_sr_convert_api
 ***********************************************************************/

DAT_RETURN dat_sr_convert_api(char *str, DAT_SR_API_VERSION * api_version)
{
	int i;
	int minor_i;

	if (dat_os_strlen(str) <= 0)
		return DAT_INTERNAL_ERROR;

	if ('u' == str[0]) {
		api_version->type = DAT_SR_API_UDAT;
	} else if ('k' == str[0]) {
		api_version->type = DAT_SR_API_KDAT;
	} else {
		return DAT_INTERNAL_ERROR;
	}

	for (i = 1 /* move past initial [u|k] */ ; '\0' != str[i]; i++) {
		if ('.' == str[i]) {
			break;
		} else if (DAT_TRUE != dat_os_isdigit(str[i])) {
			return DAT_INTERNAL_ERROR;
		}
	}

	api_version->version.major =
	    (DAT_UINT32) dat_os_strtol(str + 1, NULL, 10);

	/* move past '.' */
	minor_i = ++i;

	for (; '\0' != str[i]; i++) {
		if (DAT_TRUE != dat_os_isdigit(str[i])) {
			return DAT_INTERNAL_ERROR;
		}
	}

	api_version->version.minor =
	    (DAT_UINT32) dat_os_strtol(str + minor_i, NULL, 10);

	if ('\0' != str[i]) {
		return DAT_INTERNAL_ERROR;
	}

	return DAT_SUCCESS;
}

/***********************************************************************
 * Function: dat_sr_convert_thread_safety
 ***********************************************************************/

static DAT_RETURN
dat_sr_convert_thread_safety(char *str, DAT_BOOLEAN * is_thread_safe)
{
	if (!dat_os_strncmp(str,
			    DAT_SR_TOKEN_THREADSAFE,
			    dat_os_strlen(DAT_SR_TOKEN_THREADSAFE))) {
		*is_thread_safe = DAT_TRUE;
		return DAT_SUCCESS;
	} else if (!dat_os_strncmp(str,
				   DAT_SR_TOKEN_NONTHREADSAFE,
				   dat_os_strlen(DAT_SR_TOKEN_NONTHREADSAFE))) {
		*is_thread_safe = DAT_FALSE;
		return DAT_SUCCESS;
	} else {
		return DAT_INTERNAL_ERROR;
	}
}

/***********************************************************************
 * Function: dat_sr_convert_default
 ***********************************************************************/

static DAT_RETURN dat_sr_convert_default(char *str, DAT_BOOLEAN * is_default)
{
	if (!dat_os_strncmp(str,
			    DAT_SR_TOKEN_DEFAULT,
			    dat_os_strlen(DAT_SR_TOKEN_DEFAULT))) {
		*is_default = DAT_TRUE;
		return DAT_SUCCESS;
	} else if (!dat_os_strncmp(str,
				   DAT_SR_TOKEN_NONDEFAULT,
				   dat_os_strlen(DAT_SR_TOKEN_NONDEFAULT))) {
		*is_default = DAT_FALSE;
		return DAT_SUCCESS;
	} else {
		return DAT_INTERNAL_ERROR;
	}
}

/***********************************************************************
 * Function: dat_sr_convert_provider_version
 ***********************************************************************/

DAT_RETURN
dat_sr_convert_provider_version(char *str,
				DAT_SR_PROVIDER_VERSION * provider_version)
{
	DAT_RETURN status;
	int i;
	int decimal_i;

	if ((dat_os_strlen(str) <= 0) || (NULL != provider_version->id))
		return DAT_INTERNAL_ERROR;

	status = DAT_SUCCESS;

	for (i = 0; '\0' != str[i]; i++) {
		if ('.' == str[i]) {
			break;
		}
	}

	/* if no id value was found */
	if (0 == i) {
		status = DAT_INTERNAL_ERROR;
		goto exit;
	}

	if (NULL ==
	    (provider_version->id = dat_os_alloc(sizeof(char) * (i + 1)))) {
		status =
		    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, DAT_RESOURCE_MEMORY);
		goto exit;
	}

	dat_os_strncpy(provider_version->id, str, i);
	provider_version->id[i] = '\0';

	/* move past '.' */
	decimal_i = ++i;

	for (; '\0' != str[i]; i++) {
		if ('.' == str[i]) {
			break;
		} else if (DAT_TRUE != dat_os_isdigit(str[i])) {
			status = DAT_INTERNAL_ERROR;
			goto exit;
		}
	}

	/* if no version value was found */
	if (decimal_i == i) {
		status = DAT_INTERNAL_ERROR;
		goto exit;
	}

	provider_version->version.major = (DAT_UINT32)
	    dat_os_strtol(str + decimal_i, NULL, 10);

	/* move past '.' */
	decimal_i = ++i;

	for (; '\0' != str[i]; i++) {
		if (DAT_TRUE != dat_os_isdigit(str[i])) {
			status = DAT_INTERNAL_ERROR;
			goto exit;
		}
	}

	/* if no version value was found */
	if (decimal_i == i) {
		status = DAT_INTERNAL_ERROR;
		goto exit;
	}

	provider_version->version.minor = (DAT_UINT32)
	    dat_os_strtol(str + decimal_i, NULL, 10);

	if ('\0' != str[i]) {
		status = DAT_INTERNAL_ERROR;
		goto exit;
	}

      exit:
	if (DAT_SUCCESS != status) {
		if (NULL != provider_version->id) {
			dat_os_free(provider_version->id,
				    sizeof(char) *
				    (dat_os_strlen(provider_version->id) + 1));
			provider_version->id = NULL;
		}
	}

	return status;
}

/***********************************************************************
 * Function: dat_sr_get_token
 ***********************************************************************/

DAT_RETURN dat_sr_get_token(DAT_OS_FILE * file, DAT_SR_TOKEN * token)
{
	if (NULL == g_token_stack) {
		return dat_sr_read_token(file, token);
	} else {
		DAT_SR_STACK_NODE *top;

		top = g_token_stack;

		*token = top->token;
		g_token_stack = top->next;

		dat_os_free(top, sizeof(DAT_SR_STACK_NODE));

		return DAT_SUCCESS;
	}
}

/***********************************************************************
 * Function: dat_sr_put_token
 ***********************************************************************/

DAT_RETURN dat_sr_put_token(DAT_OS_FILE * file, DAT_SR_TOKEN * token)
{
	DAT_SR_STACK_NODE *top;

	if (NULL == (top = dat_os_alloc(sizeof(DAT_SR_STACK_NODE)))) {
		return DAT_INSUFFICIENT_RESOURCES | DAT_RESOURCE_MEMORY;
	}

	top->token = *token;
	top->next = g_token_stack;
	g_token_stack = top;

	return DAT_SUCCESS;
}

/***********************************************************************
 * Function: dat_sr_read_token
 ***********************************************************************/

DAT_RETURN dat_sr_read_token(DAT_OS_FILE * file, DAT_SR_TOKEN * token)
{
	DAT_OS_FILE_POS pos;
	DAT_OS_SIZE token_len;
	DAT_COUNT num_escape_seq;
	DAT_BOOLEAN is_quoted_str;
	DAT_BOOLEAN is_prev_char_backslash;

	/*
	 * The DAT standard does not specify a maximum size for quoted strings.
	 * Therefore the tokenizer must be able to read in a token of arbitrary
	 * size. Instead of allocating a fixed length buffer, the tokenizer first
	 * scans the input a single character at a time looking for the begining
	 * and end of the token. Once the these positions are found, the entire
	 * token is read into memory. By using this algorithm,the implementation
	 * does not place an arbitrary maximum on the token size.
	 */

	token_len = 0;
	num_escape_seq = 0;
	is_quoted_str = DAT_FALSE;
	is_prev_char_backslash = DAT_FALSE;

	for (;;) {
		DAT_OS_FILE_POS cur_pos;
		int c;

		/* if looking for start of the token */
		if (0 == token_len) {
			if (DAT_SUCCESS != dat_os_fgetpos(file, &cur_pos)) {
				return DAT_INTERNAL_ERROR;
			}
		}

		c = dat_os_fgetc(file);

		/* if looking for start of the token */
		if (0 == token_len) {
			if (EOF == c) {
				token->type = DAT_SR_TOKEN_EOF;
				token->value = NULL;
				token->value_len = 0;
				goto success;
			} else if (DAT_SR_CHAR_NEWLINE == c) {
				token->type = DAT_SR_TOKEN_EOR;
				token->value = NULL;
				token->value_len = 0;
				goto success;
			} else if (dat_os_isblank(c)) {
				continue;
			} else if (DAT_SR_CHAR_COMMENT == c) {
				dat_sr_read_comment(file);
				continue;
			} else {
				if (DAT_SR_CHAR_QUOTE == c) {
					is_quoted_str = DAT_TRUE;
				}

				pos = cur_pos;
				token_len++;
			}
		} else {	/* looking for the end of the token */

			if (EOF == c) {
				break;
			} else if (DAT_SR_CHAR_NEWLINE == c) {
				/* put back the newline */
				dat_os_fputc(file, c);
				break;
			} else if (!is_quoted_str && dat_os_isblank(c)) {
				break;
			} else {
				token_len++;

				if ((DAT_SR_CHAR_QUOTE == c)
				    && !is_prev_char_backslash) {
					break;
				} else if ((DAT_SR_CHAR_BACKSLASH == c)
					   && !is_prev_char_backslash) {
					is_prev_char_backslash = DAT_TRUE;
					num_escape_seq++;
				} else {
					is_prev_char_backslash = DAT_FALSE;
				}
			}
		}
	}

	/* the token was a string */
	if (DAT_SUCCESS != dat_os_fsetpos(file, &pos)) {
		return DAT_INTERNAL_ERROR;
	}

	if (is_quoted_str) {
		if (DAT_SUCCESS != dat_sr_read_quoted_str(file,
							  token,
							  token_len,
							  num_escape_seq)) {
			return DAT_INTERNAL_ERROR;
		}
	} else {
		if (DAT_SUCCESS != dat_sr_read_str(file, token, token_len)) {
			return DAT_INTERNAL_ERROR;
		}
	}

      success:
	dat_os_dbg_print(DAT_OS_DBG_TYPE_SR,
			 "\n"
			 "DAT Registry: token\n"
			 " type  %s\n"
			 " value <%s>\n"
			 "\n",
			 dat_sr_type_to_str(token->type),
			 ((DAT_SR_TOKEN_STRING ==
			   token->type) ? token->value : ""));

	return DAT_SUCCESS;
}

/***********************************************************************
 * Function: dat_sr_read_str
 ***********************************************************************/

DAT_RETURN
dat_sr_read_str(DAT_OS_FILE * file, DAT_SR_TOKEN * token, DAT_OS_SIZE token_len)
{
	token->type = DAT_SR_TOKEN_STRING;
	token->value_len = sizeof(char) * (token_len + 1);	/* +1 for null termination */
	if (NULL == (token->value = dat_os_alloc(token->value_len))) {
		return DAT_INSUFFICIENT_RESOURCES | DAT_RESOURCE_MEMORY;
	}

	if (token_len != dat_os_fread(file, token->value, token_len)) {
		dat_os_free(token->value, token->value_len);
		token->value = NULL;

		return DAT_INTERNAL_ERROR;
	}

	token->value[token->value_len - 1] = '\0';

	return DAT_SUCCESS;
}

/***********************************************************************
 * Function: dat_sr_read_quoted_str
 ***********************************************************************/

DAT_RETURN
dat_sr_read_quoted_str(DAT_OS_FILE * file,
		       DAT_SR_TOKEN * token,
		       DAT_OS_SIZE token_len, DAT_COUNT num_escape_seq)
{
	DAT_OS_SIZE str_len;
	DAT_OS_SIZE i;
	DAT_OS_SIZE j;
	int c;
	DAT_RETURN status;
	DAT_BOOLEAN is_prev_char_backslash;

	str_len = token_len - 2;	/* minus 2 " characters */
	is_prev_char_backslash = DAT_FALSE;
	status = DAT_SUCCESS;

	token->type = DAT_SR_TOKEN_STRING;
	/* +1 for null termination */
	token->value_len = sizeof(char) * (str_len - num_escape_seq + 1);

	if (NULL == (token->value = dat_os_alloc(token->value_len))) {
		status = DAT_INSUFFICIENT_RESOURCES | DAT_RESOURCE_MEMORY;
		goto exit;
	}

	/* throw away " */
	if (DAT_SR_CHAR_QUOTE != dat_os_fgetc(file)) {
		status = DAT_INTERNAL_ERROR;
		goto exit;
	}

	for (i = 0, j = 0; i < str_len; i++) {
		c = dat_os_fgetc(file);

		if (EOF == c) {
			status = DAT_INTERNAL_ERROR;
			goto exit;
		} else if ((DAT_SR_CHAR_BACKSLASH == c)
			   && !is_prev_char_backslash) {
			is_prev_char_backslash = DAT_TRUE;
		} else {
			token->value[j] = (char)c;
			j++;

			is_prev_char_backslash = DAT_FALSE;
		}
	}

	/* throw away " */
	if (DAT_SR_CHAR_QUOTE != dat_os_fgetc(file)) {
		status = DAT_INTERNAL_ERROR;
		goto exit;
	}

	token->value[token->value_len - 1] = '\0';

      exit:
	if (DAT_SUCCESS != status) {
		if (NULL != token->value) {
			dat_os_free(token->value, token->value_len);
			token->value = NULL;
		}
	}

	return status;
}

/***********************************************************************
 * Function: dat_sr_read_comment
 ***********************************************************************/

void dat_sr_read_comment(DAT_OS_FILE * file)
{
	int c;

	/* read up to an EOR or EOF to move past the comment */
	do {
		c = dat_os_fgetc(file);
	} while ((DAT_SR_CHAR_NEWLINE != c) && (EOF != c));

	/* put back the newline */
	dat_os_ungetc(file, c);
}
