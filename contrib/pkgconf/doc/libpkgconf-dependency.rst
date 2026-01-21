
libpkgconf `dependency` module
==============================

The `dependency` module provides support for building `dependency lists` (the basic component of the overall `dependency graph`) and
`dependency nodes` which store dependency information.

.. c:function:: pkgconf_dependency_t *pkgconf_dependency_add(pkgconf_list_t *list, const char *package, const char *version, pkgconf_pkg_comparator_t compare)

   Adds a parsed dependency to a dependency list as a dependency node.

   :param pkgconf_client_t* client: The client object that owns the package this dependency list belongs to.
   :param pkgconf_list_t* list: The dependency list to add a dependency node to.
   :param char* package: The package `atom` to set on the dependency node.
   :param char* version: The package `version` to set on the dependency node.
   :param pkgconf_pkg_comparator_t compare: The comparison operator to set on the dependency node.
   :param uint flags: Any flags to attach to the dependency node.
   :return: A dependency node.
   :rtype: pkgconf_dependency_t *

.. c:function:: void pkgconf_dependency_append(pkgconf_list_t *list, pkgconf_dependency_t *tail)

   Adds a dependency node to a pre-existing dependency list.

   :param pkgconf_list_t* list: The dependency list to add a dependency node to.
   :param pkgconf_dependency_t* tail: The dependency node to add to the tail of the dependency list.
   :return: nothing

.. c:function:: void pkgconf_dependency_free_one(pkgconf_dependency_t *dep)

   Frees a dependency node.

   :param pkgconf_dependency_t* dep: The dependency node to free.
   :return: nothing

.. c:function:: pkgconf_dependency_t *pkgconf_dependency_ref(pkgconf_client_t *owner, pkgconf_dependency_t *dep)

   Increases a dependency node's refcount.

   :param pkgconf_client_t* owner: The client object which owns the memory of this dependency node.
   :param pkgconf_dependency_t* dep: The dependency to increase the refcount of.
   :return: the dependency node on success, else NULL

.. c:function:: void pkgconf_dependency_unref(pkgconf_client_t *owner, pkgconf_dependency_t *dep)

   Decreases a dependency node's refcount and frees it if necessary.

   :param pkgconf_client_t* owner: The client object which owns the memory of this dependency node.
   :param pkgconf_dependency_t* dep: The dependency to decrease the refcount of.
   :return: nothing

.. c:function:: void pkgconf_dependency_free(pkgconf_list_t *list)

   Release a dependency list and its child dependency nodes.

   :param pkgconf_list_t* list: The dependency list to release.
   :return: nothing

.. c:function:: void pkgconf_dependency_parse_str(pkgconf_list_t *deplist_head, const char *depends)

   Parse a dependency declaration into a dependency list.
   Commas are counted as whitespace to allow for constructs such as ``@SUBSTVAR@, zlib`` being processed
   into ``, zlib``.

   :param pkgconf_client_t* client: The client object that owns the package this dependency list belongs to.
   :param pkgconf_list_t* deplist_head: The dependency list to populate with dependency nodes.
   :param char* depends: The dependency data to parse.
   :param uint flags: Any flags to attach to the dependency nodes.
   :return: nothing

.. c:function:: void pkgconf_dependency_parse(const pkgconf_client_t *client, pkgconf_pkg_t *pkg, pkgconf_list_t *deplist, const char *depends)

   Preprocess dependency data and then process that dependency declaration into a dependency list.
   Commas are counted as whitespace to allow for constructs such as ``@SUBSTVAR@, zlib`` being processed
   into ``, zlib``.

   :param pkgconf_client_t* client: The client object that owns the package this dependency list belongs to.
   :param pkgconf_pkg_t* pkg: The package object that owns this dependency list.
   :param pkgconf_list_t* deplist: The dependency list to populate with dependency nodes.
   :param char* depends: The dependency data to parse.
   :param uint flags: Any flags to attach to the dependency nodes.
   :return: nothing

.. c:function:: pkgconf_dependency_t *pkgconf_dependency_copy(pkgconf_client_t *client, const pkgconf_dependency_t *dep)

   Copies a dependency node to a new one.

   :param pkgconf_client_t* client: The client object that will own this dependency.
   :param pkgconf_dependency_t* dep: The dependency node to copy.
   :return: a pointer to a new dependency node, else NULL
