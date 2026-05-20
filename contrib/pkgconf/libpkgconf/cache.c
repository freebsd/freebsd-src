/*
 * cache.c
 * package object cache
 *
 * Copyright (c) 2013 pkgconf authors (see AUTHORS).
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#include <libpkgconf/stdinc.h>
#include <libpkgconf/libpkgconf.h>

#include <assert.h>

/*
 * !doc
 *
 * libpkgconf `cache` module
 * =========================
 *
 * The libpkgconf `cache` module manages a package/module object cache, allowing it to
 * avoid loading duplicate copies of a package/module.
 *
 * A cache is tied to a specific pkgconf client object, so package objects should not
 * be shared across threads.
 */

static int
cache_member_cmp(const void *a, const void *b)
{
	const char *key = a;
	const pkgconf_pkg_t *pkg = *(void **) b;

	return strcmp(key, pkg->id);
}

static int
cache_member_sort_cmp(const void *a, const void *b)
{
	const pkgconf_pkg_t *pkgA = *(void **) a;
	const pkgconf_pkg_t *pkgB = *(void **) b;

	if (pkgA == NULL)
		return 1;

	if (pkgB == NULL)
		return -1;

	return strcmp(pkgA->id, pkgB->id);
}

static void
cache_dump(const pkgconf_client_t *client)
{
	size_t i;

	PKGCONF_TRACE(client, "dumping package cache contents");

	for (i = 0; i < client->cache_count; i++)
	{
		const pkgconf_pkg_t *pkg = client->cache_table[i];

		PKGCONF_TRACE(client, SIZE_FMT_SPECIFIER": %p(%s)",
			i, pkg, pkg == NULL ? "NULL" : pkg->id);
	}
}

/*
 * !doc
 *
 * .. c:function:: pkgconf_pkg_t *pkgconf_cache_lookup(const pkgconf_client_t *client, const char *id)
 *
 *    Looks up a package in the cache given an `id` atom,
 *    such as ``gtk+-3.0`` and returns the already loaded version
 *    if present.
 *
 *    :param pkgconf_client_t* client: The client object to access.
 *    :param char* id: The package atom to look up in the client object's cache.
 *    :return: A package object if present, else ``NULL``.
 *    :rtype: pkgconf_pkg_t *
 */
pkgconf_pkg_t *
pkgconf_cache_lookup(pkgconf_client_t *client, const char *id)
{
	if (client->cache_table == NULL)
		return NULL;

	pkgconf_pkg_t **pkg;

	pkg = bsearch(id, client->cache_table,
		client->cache_count, sizeof (void *),
		cache_member_cmp);

	if (pkg != NULL)
	{
		PKGCONF_TRACE(client, "found: %s @%p", id, *pkg);
		return pkgconf_pkg_ref(client, *pkg);
	}

	PKGCONF_TRACE(client, "miss: %s", id);
	return NULL;
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_cache_add(pkgconf_client_t *client, pkgconf_pkg_t *pkg)
 *
 *    Adds an entry for the package to the package cache.
 *    The cache entry must be removed if the package is freed.
 *
 *    :param pkgconf_client_t* client: The client object to modify.
 *    :param pkgconf_pkg_t* pkg: The package object to add to the client object's cache.
 *    :return: nothing
 */
void
pkgconf_cache_add(pkgconf_client_t *client, pkgconf_pkg_t *pkg)
{
	if (pkg == NULL)
		return;

	pkgconf_pkg_ref(client, pkg);

	PKGCONF_TRACE(client, "added @%p to cache", pkg);

	/* mark package as cached */
	pkg->flags |= PKGCONF_PKG_PROPF_CACHED;

	++client->cache_count;
	client->cache_table = pkgconf_reallocarray(client->cache_table,
		client->cache_count, sizeof (void *));
	client->cache_table[client->cache_count - 1] = pkg;

	qsort(client->cache_table, client->cache_count,
		sizeof(void *), cache_member_sort_cmp);
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_cache_remove(pkgconf_client_t *client, pkgconf_pkg_t *pkg)
 *
 *    Deletes a package from the client object's package cache.
 *
 *    :param pkgconf_client_t* client: The client object to modify.
 *    :param pkgconf_pkg_t* pkg: The package object to remove from the client object's cache.
 *    :return: nothing
 */
void
pkgconf_cache_remove(pkgconf_client_t *client, pkgconf_pkg_t *pkg)
{
	if (client->cache_table == NULL)
		return;

	if (pkg == NULL)
		return;

	if (!(pkg->flags & PKGCONF_PKG_PROPF_CACHED))
		return;

	PKGCONF_TRACE(client, "removed @%p from cache", pkg);

	pkgconf_pkg_t **slot;

	slot = bsearch(pkg->id, client->cache_table,
		client->cache_count, sizeof (void *),
		cache_member_cmp);

	if (slot == NULL)
		return;

	(*slot)->flags &= ~PKGCONF_PKG_PROPF_CACHED;
	pkgconf_pkg_unref(client, *slot);
	*slot = NULL;

	qsort(client->cache_table, client->cache_count,
		sizeof(void *), cache_member_sort_cmp);

	if (client->cache_table[client->cache_count - 1] != NULL)
	{
		PKGCONF_TRACE(client, "end of cache table refers to %p, not NULL",
			client->cache_table[client->cache_count - 1]);
		cache_dump(client);
		abort();
	}

	client->cache_count--;
	if (client->cache_count > 0)
	{
		client->cache_table = pkgconf_reallocarray(client->cache_table,
			client->cache_count, sizeof(void *));
	}
	else
	{
		free(client->cache_table);
		client->cache_table = NULL;
	}
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_cache_free(pkgconf_client_t *client)
 *
 *    Releases all resources related to a client object's package cache.
 *    This function should only be called to clear a client object's package cache,
 *    as it may release any package in the cache.
 *
 *    :param pkgconf_client_t* client: The client object to modify.
 */
void
pkgconf_cache_free(pkgconf_client_t *client)
{
	if (client->cache_table == NULL)
		return;

	while (client->cache_count > 0)
		pkgconf_cache_remove(client, client->cache_table[0]);

	free(client->cache_table);
	client->cache_table = NULL;
	client->cache_count = 0;

	PKGCONF_TRACE(client, "cleared package cache");
}
