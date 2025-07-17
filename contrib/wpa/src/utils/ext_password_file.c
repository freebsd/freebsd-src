/*
 * External backend for file-backed passwords
 * Copyright (c) 2021, Patrick Steinhardt <ps@pks.im>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "utils/common.h"
#include "utils/config.h"
#include "ext_password_i.h"


/**
 * Data structure for the file-backed password backend.
 */
struct ext_password_file_data {
	char *path; /* path of the password file */
};


/**
 * ext_password_file_init - Initialize file-backed password backend
 * @params: Parameters passed by the user.
 * Returns: Pointer to the initialized backend.
 *
 * This function initializes a new file-backed password backend. The user is
 * expected to initialize this backend with the parameters being the path of
 * the file that contains the passwords.
 */
static void * ext_password_file_init(const char *params)
{
	struct ext_password_file_data *data;

	if (!params) {
		wpa_printf(MSG_ERROR, "EXT PW FILE: no path given");
		return NULL;
	}

	data = os_zalloc(sizeof(*data));
	if (!data)
		return NULL;

	data->path = os_strdup(params);
	if (!data->path) {
		os_free(data);
		return NULL;
	}

	return data;
}


/**
 * ext_password_file_deinit - Deinitialize file-backed password backend
 * @ctx: The file-backed password backend
 *
 * This function frees all data associated with the file-backed password
 * backend.
 */
static void ext_password_file_deinit(void *ctx)
{
	struct ext_password_file_data *data = ctx;

	str_clear_free(data->path);
	os_free(data);
}

/**
 * ext_password_file_get - Retrieve password from the file-backed password backend
 * @ctx: The file-backed password backend
 * @name: Name of the password to retrieve
 * Returns: Buffer containing the password if one was found or %NULL.
 *
 * This function tries to find a password identified by name in the password
 * file. The password is expected to be stored in `NAME=PASSWORD` format.
 * Comments and empty lines in the file are ignored. Invalid lines will cause
 * an error message, but will not cause the function to fail.
 */
static struct wpabuf * ext_password_file_get(void *ctx, const char *name)
{
	struct ext_password_file_data *data = ctx;
	struct wpabuf *password = NULL;
	char buf[512], *pos;
	int line = 0;
	FILE *f;

	f = fopen(data->path, "r");
	if (!f) {
		wpa_printf(MSG_ERROR,
			   "EXT PW FILE: could not open file '%s': %s",
			   data->path, strerror(errno));
		return NULL;
	}

	wpa_printf(MSG_DEBUG, "EXT PW FILE: get(%s)", name);

	while (wpa_config_get_line(buf, sizeof(buf), f, &line, &pos)) {
		char *sep = os_strchr(pos, '=');

		if (!sep) {
			wpa_printf(MSG_ERROR, "Invalid password line %d.",
				   line);
			continue;
		}

		if (!sep[1]) {
			wpa_printf(MSG_ERROR, "No password for line %d.", line);
			continue;

		}

		if (os_strncmp(name, pos, sep - pos) != 0)
			continue;

		password = wpabuf_alloc_copy(sep + 1, os_strlen(sep + 1));
		goto done;
	}

	wpa_printf(MSG_ERROR, "Password for '%s' was not found.", name);

done:
	forced_memzero(buf, sizeof(buf));
	fclose(f);
	return password;
}


const struct ext_password_backend ext_password_file = {
	.name = "file",
	.init = ext_password_file_init,
	.deinit = ext_password_file_deinit,
	.get = ext_password_file_get,
};
