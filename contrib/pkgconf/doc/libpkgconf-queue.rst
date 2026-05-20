
libpkgconf `queue` module
=========================

The `queue` module provides an interface that allows easily building a dependency graph from an
arbitrary set of dependencies.  It also provides support for doing "preflight" checks on the entire
dependency graph prior to working with it.

Using the `queue` module functions is the recommended way of working with dependency graphs.

.. c:function:: void pkgconf_queue_push(pkgconf_list_t *list, const char *package)

   Pushes a requested dependency onto the dependency resolver's queue.

   :param pkgconf_list_t* list: the dependency resolution queue to add the package request to.
   :param char* package: the dependency atom requested
   :return: nothing

.. c:function:: bool pkgconf_queue_compile(pkgconf_client_t *client, pkgconf_pkg_t *world, pkgconf_list_t *list)

   Compile a dependency resolution queue into a dependency resolution problem if possible, otherwise report an error.

   :param pkgconf_client_t* client: The pkgconf client object to use for dependency resolution.
   :param pkgconf_pkg_t* world: The designated root of the dependency graph.
   :param pkgconf_list_t* list: The list of dependency requests to consider.
   :return: true if the built dependency resolution problem is consistent, else false
   :rtype: bool

.. c:function:: void pkgconf_queue_free(pkgconf_list_t *list)

   Release any memory related to a dependency resolution queue.

   :param pkgconf_list_t* list: The dependency resolution queue to release.
   :return: nothing

.. c:function:: void pkgconf_solution_free(pkgconf_client_t *client, pkgconf_pkg_t *world, int maxdepth)

   Removes references to package nodes contained in a solution.

   :param pkgconf_client_t* client: The pkgconf client object to use for dependency resolution.
   :param pkgconf_pkg_t* world: The root for the generated dependency graph.  Should have PKGCONF_PKG_PROPF_VIRTUAL flag.
   :returns: nothing

.. c:function:: bool pkgconf_queue_solve(pkgconf_client_t *client, pkgconf_list_t *list, pkgconf_pkg_t *world, int maxdepth)

   Solves and flattens the dependency graph for the supplied dependency list.

   :param pkgconf_client_t* client: The pkgconf client object to use for dependency resolution.
   :param pkgconf_list_t* list: The list of dependency requests to consider.
   :param pkgconf_pkg_t* world: The root for the generated dependency graph, provided by the caller.  Should have PKGCONF_PKG_PROPF_VIRTUAL flag.
   :param int maxdepth: The maximum allowed depth for the dependency resolver.  A depth of -1 means unlimited.
   :returns: true if the dependency resolver found a solution, otherwise false.
   :rtype: bool

.. c:function:: void pkgconf_queue_apply(pkgconf_client_t *client, pkgconf_list_t *list, pkgconf_queue_apply_func_t func, int maxdepth, void *data)

   Attempt to compile a dependency resolution queue into a dependency resolution problem, then attempt to solve the problem and
   feed the solution to a callback function if a complete dependency graph is found.

   This function should not be used in new code.  Use pkgconf_queue_solve instead.

   :param pkgconf_client_t* client: The pkgconf client object to use for dependency resolution.
   :param pkgconf_list_t* list: The list of dependency requests to consider.
   :param pkgconf_queue_apply_func_t func: The callback function to call if a solution is found by the dependency resolver.
   :param int maxdepth: The maximum allowed depth for the dependency resolver.  A depth of -1 means unlimited.
   :param void* data: An opaque pointer which is passed to the callback function.
   :returns: true if the dependency resolver found a solution, otherwise false.
   :rtype: bool

.. c:function:: void pkgconf_queue_validate(pkgconf_client_t *client, pkgconf_list_t *list, pkgconf_queue_apply_func_t func, int maxdepth, void *data)

   Attempt to compile a dependency resolution queue into a dependency resolution problem, then attempt to solve the problem.

   :param pkgconf_client_t* client: The pkgconf client object to use for dependency resolution.
   :param pkgconf_list_t* list: The list of dependency requests to consider.
   :param int maxdepth: The maximum allowed depth for the dependency resolver.  A depth of -1 means unlimited.
   :returns: true if the dependency resolver found a solution, otherwise false.
   :rtype: bool
