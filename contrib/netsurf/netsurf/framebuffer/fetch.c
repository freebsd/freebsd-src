/*
 * Copyright 2014 Vincent Sanders <vince@netsurf-browser.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file
 * Interfaces for fetch table.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "utils/nsurl.h"
#include "utils/url.h"
#include "utils/log.h"
#include "utils/filepath.h"
#include "desktop/gui.h"

#include "framebuffer/findfile.h"
#include "framebuffer/fetch.h"

/**
 * Return the filename part of a full path
 *
 * \param path full path and filename
 * \return filename (will be freed with free())
 */
static char *filename_from_path(char *path)
{
	char *leafname;

	leafname = strrchr(path, '/');
	if (!leafname)
		leafname = path;
	else
		leafname += 1;

	return strdup(leafname);
}

/**
 * Add a path component/filename to an existing path
 *
 * \param path buffer containing path + free space
 * \param length length of buffer "path"
 * \param newpart string containing path component to add to path
 * \return true on success
 */
static bool path_add_part(char *path, int length, const char *newpart)
{
	if(path[strlen(path) - 1] != '/')
		strncat(path, "/", length);

	strncat(path, newpart, length);

	return true;
}

/**
 * Convert a pathname to a file: URL.
 *
 * \param  path  pathname
 * \return  URL, allocated on heap, or NULL on failure
 */
static char *path_to_url(const char *path)
{
	int urllen;
	char *url;

	if (path == NULL)
		return NULL;

	urllen = strlen(path) + FILE_SCHEME_PREFIX_LEN + 1;
	url = malloc(urllen);

	if (*path == '/') {
		path++; /* file: paths are already absolute */
	}

	snprintf(url, urllen, "%s%s", FILE_SCHEME_PREFIX, path);

	return url;
}

/**
 * Convert a file: URL to a pathname.
 *
 * \param  url  a file: URL
 * \return  pathname, allocated on heap, or NULL on failure
 */
static char *url_to_path(const char *url)
{
	char *path;
	char *respath;
	url_func_result res; /* result from url routines */

	res = url_path(url, &path);
	if (res != URL_FUNC_OK) {
		return NULL;
	}

	res = url_unescape(path, &respath);
	free(path);
	if (res != URL_FUNC_OK) {
		return NULL;
	}

	return respath;
}

/**
 * Translate resource to full url.
 *
 * Transforms a resource: path into a full URL. The returned URL
 * is used as the target for a redirect. The caller takes ownership of
 * the returned nsurl including unrefing it when finished with it.
 *
 * \param path The path of the resource to locate.
 * \return A string containing the full URL of the target object or
 *         NULL if no suitable resource can be found.
 */
static nsurl *get_resource_url(const char *path)
{
	char buf[PATH_MAX];
	char *raw;
	nsurl *url = NULL;

	if (strcmp(path, "favicon.ico") == 0)
		path = "favicon.png";

	raw = path_to_url(filepath_sfind(respaths, buf, path));
	if (raw != NULL) {
		nsurl_create(raw, &url);
		free(raw);
	}

	return url;
}

/**
 * filetype -- determine the MIME type of a local file
 */
static const char *fetch_filetype(const char *unix_path)
{
	int l;
	LOG(("unix path %s", unix_path));
	l = strlen(unix_path);
	if (2 < l && strcasecmp(unix_path + l - 3, "css") == 0)
		return "text/css";
	if (2 < l && strcasecmp(unix_path + l - 3, "f79") == 0)
		return "text/css";
	if (2 < l && strcasecmp(unix_path + l - 3, "jpg") == 0)
		return "image/jpeg";
	if (3 < l && strcasecmp(unix_path + l - 4, "jpeg") == 0)
		return "image/jpeg";
	if (2 < l && strcasecmp(unix_path + l - 3, "gif") == 0)
		return "image/gif";
	if (2 < l && strcasecmp(unix_path + l - 3, "png") == 0)
		return "image/png";
	if (2 < l && strcasecmp(unix_path + l - 3, "b60") == 0)
		return "image/png";
	if (2 < l && strcasecmp(unix_path + l - 3, "jng") == 0)
		return "image/jng";
	if (2 < l && strcasecmp(unix_path + l - 3, "svg") == 0)
		return "image/svg";
	return "text/html";
}


static char *fetch_mimetype(const char *ro_path)
{
	return strdup("text/plain");
}

/* table for fetch operations */
static struct gui_fetch_table fetch_table = {
	.filename_from_path = filename_from_path,
	.path_add_part = path_add_part,
	.filetype = fetch_filetype,
	.path_to_url = path_to_url,
	.url_to_path = url_to_path,

	.get_resource_url = get_resource_url,
	.mimetype = fetch_mimetype,
};

struct gui_fetch_table *framebuffer_fetch_table = &fetch_table;
