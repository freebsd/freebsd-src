
libpkgconf `cache` module
=========================

The libpkgconf `cache` module manages a package/module object cache, allowing it to
avoid loading duplicate copies of a package/module.

A cache is tied to a specific pkgconf client object, so package objects should not
be shared across threads.

.. c:function:: pkgconf_pkg_t *pkgconf_cache_lookup(const pkgconf_client_t *client, const char *id)

   Looks up a package in the cache given an `id` atom,
   such as ``gtk+-3.0`` and returns the already loaded version
   if present.

   :param pkgconf_client_t* client: The client object to access.
   :param char* id: The package atom to look up in the client object's cache.
   :return: A package object if present, else ``NULL``.
   :rtype: pkgconf_pkg_t *

.. c:function:: void pkgconf_cache_add(pkgconf_client_t *client, pkgconf_pkg_t *pkg)

   Adds an entry for the package to the package cache.
   The cache entry must be removed if the package is freed.

   :param pkgconf_client_t* client: The client object to modify.
   :param pkgconf_pkg_t* pkg: The package object to add to the client object's cache.
   :return: nothing

.. c:function:: void pkgconf_cache_remove(pkgconf_client_t *client, pkgconf_pkg_t *pkg)

   Deletes a package from the client object's package cache.

   :param pkgconf_client_t* client: The client object to modify.
   :param pkgconf_pkg_t* pkg: The package object to remove from the client object's cache.
   :return: nothing

.. c:function:: void pkgconf_cache_free(pkgconf_client_t *client)

   Releases all resources related to a client object's package cache.
   This function should only be called to clear a client object's package cache,
   as it may release any package in the cache.

   :param pkgconf_client_t* client: The client object to modify.
