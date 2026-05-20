
libpkgconf `path` module
========================

The `path` module provides functions for manipulating lists of paths in a cross-platform manner.  Notably,
it is used by the `pkgconf client` to parse the ``PKG_CONFIG_PATH``, ``PKG_CONFIG_LIBDIR`` and related environment
variables.

.. c:function:: void pkgconf_path_add(const char *text, pkgconf_list_t *dirlist)

   Adds a path node to a path list.  If the path is already in the list, do nothing.

   :param char* text: The path text to add as a path node.
   :param pkgconf_list_t* dirlist: The path list to add the path node to.
   :param bool filter: Whether to perform duplicate filtering.
   :return: nothing

.. c:function:: size_t pkgconf_path_split(const char *text, pkgconf_list_t *dirlist)

   Splits a given text input and inserts paths into a path list.

   :param char* text: The path text to split and add as path nodes.
   :param pkgconf_list_t* dirlist: The path list to have the path nodes added to.
   :param bool filter: Whether to perform duplicate filtering.
   :return: number of path nodes added to the path list
   :rtype: size_t

.. c:function:: size_t pkgconf_path_build_from_environ(const char *envvarname, const char *fallback, pkgconf_list_t *dirlist)

   Adds the paths specified in an environment variable to a path list.  If the environment variable is not set,
   an optional default set of paths is added.

   :param char* envvarname: The environment variable to look up.
   :param char* fallback: The fallback paths to use if the environment variable is not set.
   :param pkgconf_list_t* dirlist: The path list to add the path nodes to.
   :param bool filter: Whether to perform duplicate filtering.
   :return: number of path nodes added to the path list
   :rtype: size_t

.. c:function:: bool pkgconf_path_match_list(const char *path, const pkgconf_list_t *dirlist)

   Checks whether a path has a matching prefix in a path list.

   :param char* path: The path to check against a path list.
   :param pkgconf_list_t* dirlist: The path list to check the path against.
   :return: true if the path list has a matching prefix, otherwise false
   :rtype: bool

.. c:function:: void pkgconf_path_copy_list(pkgconf_list_t *dst, const pkgconf_list_t *src)

   Copies a path list to another path list.

   :param pkgconf_list_t* dst: The path list to copy to.
   :param pkgconf_list_t* src: The path list to copy from.
   :return: nothing

.. c:function:: void pkgconf_path_free(pkgconf_list_t *dirlist)

   Releases any path nodes attached to the given path list.

   :param pkgconf_list_t* dirlist: The path list to clean up.
   :return: nothing

.. c:function:: bool pkgconf_path_relocate(char *buf, size_t buflen)

   Relocates a path, possibly calling normpath() on it.

   :param char* buf: The path to relocate.
   :param size_t buflen: The buffer length the path is contained in.
   :return: true on success, false on error
   :rtype: bool
