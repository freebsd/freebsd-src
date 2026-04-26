/*
 * path.c
 * filesystem path management
 *
 * Copyright (c) 2016 pkgconf authors (see AUTHORS).
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#include <libpkgconf/config.h>
#include <libpkgconf/stdinc.h>
#include <libpkgconf/libpkgconf.h>

#if defined(HAVE_SYS_STAT_H) && ! defined(_WIN32)
# include <sys/stat.h>
# define PKGCONF_CACHE_INODES
#endif

#ifdef _WIN32
# define PKG_CONFIG_REG_KEY "Software\\pkgconfig\\PKG_CONFIG_PATH"
#endif

static bool
#ifdef PKGCONF_CACHE_INODES
path_list_contains_entry(const char *text, pkgconf_list_t *dirlist, struct stat *st)
#else
path_list_contains_entry(const char *text, pkgconf_list_t *dirlist)
#endif
{
	pkgconf_node_t *n;

	PKGCONF_FOREACH_LIST_ENTRY(dirlist->head, n)
	{
		pkgconf_path_t *pn = n->data;

#ifdef PKGCONF_CACHE_INODES
		if (pn->handle_device == (void *)(intptr_t)st->st_dev && pn->handle_path == (void *)(intptr_t)st->st_ino)
			return true;
#endif

		if (!strcmp(text, pn->path))
			return true;
	}

	return false;
}

/*
 * !doc
 *
 * libpkgconf `path` module
 * ========================
 *
 * The `path` module provides functions for manipulating lists of paths in a cross-platform manner.  Notably,
 * it is used by the `pkgconf client` to parse the ``PKG_CONFIG_PATH``, ``PKG_CONFIG_LIBDIR`` and related environment
 * variables.
 */

static pkgconf_path_t *
prepare_path_node(const char *text, pkgconf_list_t *dirlist, bool filter)
{
	pkgconf_path_t *node;
	char path[PKGCONF_ITEM_SIZE];

	pkgconf_strlcpy(path, text, sizeof path);
	pkgconf_path_relocate(path, sizeof path);

#ifdef PKGCONF_CACHE_INODES
	struct stat st;

	if (filter)
	{
		if (lstat(path, &st) == -1)
			return NULL;
		if (S_ISLNK(st.st_mode))
		{
			char pathbuf[PKGCONF_ITEM_SIZE * 4];
			char *linkdest = realpath(path, pathbuf);

			if (linkdest != NULL && stat(linkdest, &st) == -1)
				return NULL;
		}
		if (path_list_contains_entry(path, dirlist, &st))
			return NULL;
	}
#else
	if (filter && path_list_contains_entry(path, dirlist))
		return NULL;
#endif

	node = calloc(1, sizeof(pkgconf_path_t));
	if (node == NULL)
		return NULL;

	node->path = strdup(path);

#ifdef PKGCONF_CACHE_INODES
	if (filter) {
		node->handle_path = (void *)(intptr_t) st.st_ino;
		node->handle_device = (void *)(intptr_t) st.st_dev;
	}
#endif

	return node;
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_path_add(const char *text, pkgconf_list_t *dirlist)
 *
 *    Adds a path node to a path list.  If the path is already in the list, do nothing.
 *
 *    :param char* text: The path text to add as a path node.
 *    :param pkgconf_list_t* dirlist: The path list to add the path node to.
 *    :param bool filter: Whether to perform duplicate filtering.
 *    :return: nothing
 */
void
pkgconf_path_add(const char *text, pkgconf_list_t *dirlist, bool filter)
{
	pkgconf_path_t *node = prepare_path_node(text, dirlist, filter);
	if (node == NULL)
		return;

	pkgconf_node_insert_tail(&node->lnode, node, dirlist);
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_path_prepend(const char *text, pkgconf_list_t *dirlist)
 *
 *    Prepends a path node to a path list.  If the path is already in the list, do nothing.
 *
 *    :param char* text: The path text to add as a path node.
 *    :param pkgconf_list_t* dirlist: The path list to add the path node to.
 *    :param bool filter: Whether to perform duplicate filtering.
 *    :return: nothing
 */
void
pkgconf_path_prepend(const char *text, pkgconf_list_t *dirlist, bool filter)
{
	pkgconf_path_t *node = prepare_path_node(text, dirlist, filter);
	if (node == NULL)
		return;

	pkgconf_node_insert(&node->lnode, node, dirlist);
}

/*
 * !doc
 *
 * .. c:function:: size_t pkgconf_path_split(const char *text, pkgconf_list_t *dirlist)
 *
 *    Splits a given text input and inserts paths into a path list.
 *
 *    :param char* text: The path text to split and add as path nodes.
 *    :param pkgconf_list_t* dirlist: The path list to have the path nodes added to.
 *    :param bool filter: Whether to perform duplicate filtering.
 *    :return: number of path nodes added to the path list
 *    :rtype: size_t
 */
size_t
pkgconf_path_split(const char *text, pkgconf_list_t *dirlist, bool filter)
{
	size_t count = 0;
	char *workbuf, *p, *iter;

	if (text == NULL)
		return 0;

	iter = workbuf = strdup(text);
	while ((p = strtok(iter, PKG_CONFIG_PATH_SEP_S)) != NULL)
	{
		pkgconf_path_add(p, dirlist, filter);

		count++, iter = NULL;
	}
	free(workbuf);

	return count;
}

/*
 * !doc
 *
 * .. c:function:: size_t pkgconf_path_build_from_environ(const char *envvarname, const char *fallback, pkgconf_list_t *dirlist)
 *
 *    Adds the paths specified in an environment variable to a path list.  If the environment variable is not set,
 *    an optional default set of paths is added.
 *
 *    :param char* envvarname: The environment variable to look up.
 *    :param char* fallback: The fallback paths to use if the environment variable is not set.
 *    :param pkgconf_list_t* dirlist: The path list to add the path nodes to.
 *    :param bool filter: Whether to perform duplicate filtering.
 *    :return: number of path nodes added to the path list
 *    :rtype: size_t
 */
size_t
pkgconf_path_build_from_environ(const char *envvarname, const char *fallback, pkgconf_list_t *dirlist, bool filter)
{
	const char *data;

	data = getenv(envvarname);
	if (data != NULL)
		return pkgconf_path_split(data, dirlist, filter);

	if (fallback != NULL)
		return pkgconf_path_split(fallback, dirlist, filter);

	/* no fallback and no environment variable, thusly no nodes added */
	return 0;
}

/*
 * !doc
 *
 * .. c:function:: bool pkgconf_path_match_list(const char *path, const pkgconf_list_t *dirlist)
 *
 *    Checks whether a path has a matching prefix in a path list.
 *
 *    :param char* path: The path to check against a path list.
 *    :param pkgconf_list_t* dirlist: The path list to check the path against.
 *    :return: true if the path list has a matching prefix, otherwise false
 *    :rtype: bool
 */
bool
pkgconf_path_match_list(const char *path, const pkgconf_list_t *dirlist)
{
	pkgconf_node_t *n = NULL;
	char relocated[PKGCONF_ITEM_SIZE];
	const char *cpath = path;

	pkgconf_strlcpy(relocated, path, sizeof relocated);
	if (pkgconf_path_relocate(relocated, sizeof relocated))
		cpath = relocated;

	PKGCONF_FOREACH_LIST_ENTRY(dirlist->head, n)
	{
		pkgconf_path_t *pnode = n->data;

		if (!strcmp(pnode->path, cpath))
			return true;
	}

	return false;
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_path_copy_list(pkgconf_list_t *dst, const pkgconf_list_t *src)
 *
 *    Copies a path list to another path list.
 *
 *    :param pkgconf_list_t* dst: The path list to copy to.
 *    :param pkgconf_list_t* src: The path list to copy from.
 *    :return: nothing
 */
void
pkgconf_path_copy_list(pkgconf_list_t *dst, const pkgconf_list_t *src)
{
	pkgconf_node_t *n;

	PKGCONF_FOREACH_LIST_ENTRY(src->head, n)
	{
		pkgconf_path_t *srcpath = n->data, *path;

		path = calloc(1, sizeof(pkgconf_path_t));
		if (path == NULL)
			continue;

		path->path = strdup(srcpath->path);

#ifdef PKGCONF_CACHE_INODES
		path->handle_path = srcpath->handle_path;
		path->handle_device = srcpath->handle_device;
#endif

		pkgconf_node_insert_tail(&path->lnode, path, dst);
	}
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_path_prepend_list(pkgconf_list_t *dst, const pkgconf_list_t *src)
 *
 *    Copies a path list to another path list.
 *
 *    :param pkgconf_list_t* dst: The path list to copy to.
 *    :param pkgconf_list_t* src: The path list to copy from.
 *    :return: nothing
 */
void
pkgconf_path_prepend_list(pkgconf_list_t *dst, const pkgconf_list_t *src)
{
	pkgconf_node_t *n;

	PKGCONF_FOREACH_LIST_ENTRY(src->head, n)
	{
		pkgconf_path_t *srcpath = n->data, *path;

		path = calloc(1, sizeof(pkgconf_path_t));
		if (path == NULL)
			continue;

		path->path = strdup(srcpath->path);

#ifdef PKGCONF_CACHE_INODES
		path->handle_path = srcpath->handle_path;
		path->handle_device = srcpath->handle_device;
#endif

		pkgconf_node_insert(&path->lnode, path, dst);
	}
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_path_free(pkgconf_list_t *dirlist)
 *
 *    Releases any path nodes attached to the given path list.
 *
 *    :param pkgconf_list_t* dirlist: The path list to clean up.
 *    :return: nothing
 */
void
pkgconf_path_free(pkgconf_list_t *dirlist)
{
	pkgconf_node_t *n, *tn;

	PKGCONF_FOREACH_LIST_ENTRY_SAFE(dirlist->head, tn, n)
	{
		pkgconf_path_t *pnode = n->data;

		free(pnode->path);
		free(pnode);
	}

	pkgconf_list_zero(dirlist);
}

static char *
normpath(const char *path)
{
	if (!path)
		return NULL;

	char *copy = strdup(path);
	if (NULL == copy)
		return NULL;
	char *ptr = copy;

	for (int ii = 0; copy[ii]; ii++)
	{
		*ptr++ = path[ii];
		if ('/' == path[ii])
		{
			ii++;
			while ('/' == path[ii])
				ii++;
			ii--;
		}
	}
	*ptr = '\0';

	return copy;
}

/*
 * !doc
 *
 * .. c:function:: bool pkgconf_path_relocate(char *buf, size_t buflen)
 *
 *    Relocates a path, possibly calling normpath() on it.
 *
 *    :param char* buf: The path to relocate.
 *    :param size_t buflen: The buffer length the path is contained in.
 *    :return: true on success, false on error
 *    :rtype: bool
 */
bool
pkgconf_path_relocate(char *buf, size_t buflen)
{
	char *tmpbuf;

	if ((tmpbuf = normpath(buf)) != NULL)
	{
		size_t tmpbuflen = strlen(tmpbuf);
		if (tmpbuflen > buflen)
		{
			free(tmpbuf);
			return false;
		}

		pkgconf_strlcpy(buf, tmpbuf, buflen);
		free(tmpbuf);
	}

	return true;
}

#ifdef _WIN32
/*
 * !doc
 *
 * .. c:function:: void pkgconf_path_build_from_registry(HKEY hKey, pkgconf_list_t *dir_list, bool filter)
 *
 *    Adds paths to a directory list discovered from a given registry key.
 *
 *    :param HKEY hKey: The registry key to enumerate.
 *    :param pkgconf_list_t* dir_list: The directory list to append enumerated paths to.
 *    :param bool filter: Whether duplicate paths should be filtered.
 *    :return: number of path nodes added to the list
 *    :rtype: size_t
 */
size_t
pkgconf_path_build_from_registry(void *hKey, pkgconf_list_t *dir_list, bool filter)
{
	HKEY key;
	int i = 0;
	size_t added = 0;

	char buf[16384]; /* per registry limits */
	DWORD bufsize = sizeof buf;
	if (RegOpenKeyEx(hKey, PKG_CONFIG_REG_KEY,
				0, KEY_READ, &key) != ERROR_SUCCESS)
		return 0;

	while (RegEnumValue(key, i++, buf, &bufsize, NULL, NULL, NULL, NULL)
			== ERROR_SUCCESS)
	{
		char pathbuf[PKGCONF_ITEM_SIZE];
		DWORD type;
		DWORD pathbuflen = sizeof pathbuf;

		if (RegQueryValueEx(key, buf, NULL, &type, (LPBYTE) pathbuf, &pathbuflen)
				== ERROR_SUCCESS && type == REG_SZ)
		{
			pkgconf_path_add(pathbuf, dir_list, filter);
			added++;
		}

		bufsize = sizeof buf;
	}

	RegCloseKey(key);
	return added;
}
#endif
