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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>

#include "desktop/gui.h"
#include "utils/url.h"
#include "utils/nsurl.h"
#include "utils/filepath.h"

#include "monkey/filetype.h"
#include "monkey/fetch.h"

extern char **respaths;


static char *path_to_url(const char *path)
{
  int urllen;
  char *url;

  if (path == NULL) {
    return NULL;
  }

  urllen = strlen(path) + FILE_SCHEME_PREFIX_LEN + 1;

  url = malloc(urllen);
  if (url == NULL) {
    return NULL;
  }

  if (*path == '/') {
    path++; /* file: paths are already absolute */
  }

  snprintf(url, urllen, "%s%s", FILE_SCHEME_PREFIX, path);

  return url;
}

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

static nsurl *gui_get_resource_url(const char *path)
{
  char buf[PATH_MAX];
  char *raw;
  nsurl *url = NULL;

  raw = path_to_url(filepath_sfind(respaths, buf, path));
  if (raw != NULL) {
    nsurl_create(raw, &url);
    free(raw);
  }

  return url;
}

static struct gui_fetch_table fetch_table = {
  .filename_from_path = filename_from_path,
  .path_add_part = path_add_part,
  .filetype = monkey_fetch_filetype,
  .path_to_url = path_to_url,
  .url_to_path = url_to_path,

  .get_resource_url = gui_get_resource_url,
};

struct gui_fetch_table *monkey_fetch_table = &fetch_table;
