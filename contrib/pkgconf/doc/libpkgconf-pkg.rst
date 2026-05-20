
libpkgconf `pkg` module
=======================

The `pkg` module provides dependency resolution services and the overall `.pc` file parsing
routines.

.. c:function:: pkgconf_pkg_t *pkgconf_pkg_new_from_file(const pkgconf_client_t *client, const char *filename, FILE *f, unsigned int flags)

   Parse a .pc file into a pkgconf_pkg_t object structure.

   :param pkgconf_client_t* client: The pkgconf client object to use for dependency resolution.
   :param char* filename: The filename of the package file (including full path).
   :param FILE* f: The file object to read from.
   :param uint flags: The flags to use when parsing.
   :returns: A ``pkgconf_pkg_t`` object which contains the package data.
   :rtype: pkgconf_pkg_t *

.. c:function:: void pkgconf_pkg_free(pkgconf_client_t *client, pkgconf_pkg_t *pkg)

   Releases all releases for a given ``pkgconf_pkg_t`` object.

   :param pkgconf_client_t* client: The client which owns the ``pkgconf_pkg_t`` object, `pkg`.
   :param pkgconf_pkg_t* pkg: The package to free.
   :return: nothing

.. c:function:: pkgconf_pkg_t *pkgconf_pkg_ref(const pkgconf_client_t *client, pkgconf_pkg_t *pkg)

   Adds an additional reference to the package object.

   :param pkgconf_client_t* client: The pkgconf client object which owns the package being referenced.
   :param pkgconf_pkg_t* pkg: The package object being referenced.
   :return: The package itself with an incremented reference count.
   :rtype: pkgconf_pkg_t *

.. c:function:: void pkgconf_pkg_unref(pkgconf_client_t *client, pkgconf_pkg_t *pkg)

   Releases a reference on the package object.  If the reference count is 0, then also free the package.

   :param pkgconf_client_t* client: The pkgconf client object which owns the package being dereferenced.
   :param pkgconf_pkg_t* pkg: The package object being dereferenced.
   :return: nothing

.. c:function:: pkgconf_pkg_t *pkgconf_scan_all(pkgconf_client_t *client, void *data, pkgconf_pkg_iteration_func_t func)

   Iterates over all packages found in the `package directory list`, running ``func`` on them.  If ``func`` returns true,
   then stop iteration and return the last iterated package.

   :param pkgconf_client_t* client: The pkgconf client object to use for dependency resolution.
   :param void* data: An opaque pointer to data to provide the iteration function with.
   :param pkgconf_pkg_iteration_func_t func: A function which is called for each package to determine if the package matches,
       always return ``false`` to iterate over all packages.
   :return: A package object reference if one is found by the scan function, else ``NULL``.
   :rtype: pkgconf_pkg_t *

.. c:function:: pkgconf_pkg_t *pkgconf_pkg_find(pkgconf_client_t *client, const char *name)

   Search for a package.

   :param pkgconf_client_t* client: The pkgconf client object to use for dependency resolution.
   :param char* name: The name of the package `atom` to use for searching.
   :return: A package object reference if the package was found, else ``NULL``.
   :rtype: pkgconf_pkg_t *

.. c:function:: int pkgconf_compare_version(const char *a, const char *b)

   Compare versions using RPM version comparison rules as described in the LSB.

   :param char* a: The first version to compare in the pair.
   :param char* b: The second version to compare in the pair.
   :return: -1 if the first version is less than, 0 if both versions are equal, 1 if the second version is less than.
   :rtype: int

.. c:function:: pkgconf_pkg_t *pkgconf_builtin_pkg_get(const char *name)

   Looks up a built-in package.  The package should not be freed or dereferenced.

   :param char* name: An atom corresponding to a built-in package to search for.
   :return: the built-in package if present, else ``NULL``.
   :rtype: pkgconf_pkg_t *

.. c:function:: const char *pkgconf_pkg_get_comparator(const pkgconf_dependency_t *pkgdep)

   Returns the comparator used in a depgraph dependency node as a string.

   :param pkgconf_dependency_t* pkgdep: The depgraph dependency node to return the comparator for.
   :return: A string matching the comparator or ``"???"``.
   :rtype: char *

.. c:function:: pkgconf_pkg_comparator_t pkgconf_pkg_comparator_lookup_by_name(const char *name)

   Look up the appropriate comparator bytecode in the comparator set (defined
   in ``pkg.c``, see ``pkgconf_pkg_comparator_names`` and ``pkgconf_pkg_comparator_impls``).

   :param char* name: The comparator to look up by `name`.
   :return: The comparator bytecode if found, else ``PKGCONF_CMP_ANY``.
   :rtype: pkgconf_pkg_comparator_t

.. c:function:: pkgconf_pkg_t *pkgconf_pkg_verify_dependency(pkgconf_client_t *client, pkgconf_dependency_t *pkgdep, unsigned int *eflags)

   Verify a pkgconf_dependency_t node in the depgraph.  If the dependency is solvable,
   return the appropriate ``pkgconf_pkg_t`` object, else ``NULL``.

   :param pkgconf_client_t* client: The pkgconf client object to use for dependency resolution.
   :param pkgconf_dependency_t* pkgdep: The dependency graph node to solve.
   :param uint* eflags: An optional pointer that, if set, will be populated with an error code from the resolver.
   :return: On success, the appropriate ``pkgconf_pkg_t`` object to solve the dependency, else ``NULL``.
   :rtype: pkgconf_pkg_t *

.. c:function:: unsigned int pkgconf_pkg_verify_graph(pkgconf_client_t *client, pkgconf_pkg_t *root, int depth)

   Verify the graph dependency nodes are satisfiable by walking the tree using
   ``pkgconf_pkg_traverse()``.

   :param pkgconf_client_t* client: The pkgconf client object to use for dependency resolution.
   :param pkgconf_pkg_t* root: The root entry in the package dependency graph which should contain the top-level dependencies to resolve.
   :param int depth: The maximum allowed depth for dependency resolution.
   :return: On success, ``PKGCONF_PKG_ERRF_OK`` (0), else an error code.
   :rtype: unsigned int

.. c:function:: unsigned int pkgconf_pkg_traverse(pkgconf_client_t *client, pkgconf_pkg_t *root, pkgconf_pkg_traverse_func_t func, void *data, int maxdepth, unsigned int skip_flags)

   Walk and resolve the dependency graph up to `maxdepth` levels.

   :param pkgconf_client_t* client: The pkgconf client object to use for dependency resolution.
   :param pkgconf_pkg_t* root: The root of the dependency graph.
   :param pkgconf_pkg_traverse_func_t func: A traversal function to call for each resolved node in the dependency graph.
   :param void* data: An opaque pointer to data to be passed to the traversal function.
   :param int maxdepth: The maximum depth to walk the dependency graph for.  -1 means infinite recursion.
   :param uint skip_flags: Skip over dependency nodes containing the specified flags.  A setting of 0 skips no dependency nodes.
   :return: ``PKGCONF_PKG_ERRF_OK`` on success, else an error code.
   :rtype: unsigned int

.. c:function:: int pkgconf_pkg_cflags(pkgconf_client_t *client, pkgconf_pkg_t *root, pkgconf_list_t *list, int maxdepth)

   Walks a dependency graph and extracts relevant ``CFLAGS`` fragments.

   :param pkgconf_client_t* client: The pkgconf client object to use for dependency resolution.
   :param pkgconf_pkg_t* root: The root of the dependency graph.
   :param pkgconf_list_t* list: The fragment list to add the extracted ``CFLAGS`` fragments to.
   :param int maxdepth: The maximum allowed depth for dependency resolution.  -1 means infinite recursion.
   :return: ``PKGCONF_PKG_ERRF_OK`` if successful, otherwise an error code.
   :rtype: unsigned int

.. c:function:: int pkgconf_pkg_libs(pkgconf_client_t *client, pkgconf_pkg_t *root, pkgconf_list_t *list, int maxdepth)

   Walks a dependency graph and extracts relevant ``LIBS`` fragments.

   :param pkgconf_client_t* client: The pkgconf client object to use for dependency resolution.
   :param pkgconf_pkg_t* root: The root of the dependency graph.
   :param pkgconf_list_t* list: The fragment list to add the extracted ``LIBS`` fragments to.
   :param int maxdepth: The maximum allowed depth for dependency resolution.  -1 means infinite recursion.
   :return: ``PKGCONF_PKG_ERRF_OK`` if successful, otherwise an error code.
   :rtype: unsigned int
