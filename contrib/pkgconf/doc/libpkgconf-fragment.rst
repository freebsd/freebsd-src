
libpkgconf `fragment` module
============================

The `fragment` module provides low-level management and rendering of fragment lists.  A
`fragment list` contains various `fragments` of text (such as ``-I /usr/include``) in a matter
which is composable, mergeable and reorderable.

.. c:function:: void pkgconf_fragment_add(const pkgconf_client_t *client, pkgconf_list_t *list, const char *string, unsigned int flags)

   Adds a `fragment` of text to a `fragment list`, possibly modifying the fragment if a sysroot is set.

   :param pkgconf_client_t* client: The pkgconf client being accessed.
   :param pkgconf_list_t* list: The fragment list.
   :param char* string: The string of text to add as a fragment to the fragment list.
   :param uint flags: Parsing-related flags for the package.
   :return: nothing

.. c:function:: bool pkgconf_fragment_has_system_dir(const pkgconf_client_t *client, const pkgconf_fragment_t *frag)

   Checks if a `fragment` contains a `system path`.  System paths are detected at compile time and optionally overridden by
   the ``PKG_CONFIG_SYSTEM_INCLUDE_PATH`` and ``PKG_CONFIG_SYSTEM_LIBRARY_PATH`` environment variables.

   :param pkgconf_client_t* client: The pkgconf client object the fragment belongs to.
   :param pkgconf_fragment_t* frag: The fragment being checked.
   :return: true if the fragment contains a system path, else false
   :rtype: bool

.. c:function:: void pkgconf_fragment_copy(const pkgconf_client_t *client, pkgconf_list_t *list, const pkgconf_fragment_t *base, bool is_private)

   Copies a `fragment` to another `fragment list`, possibly removing a previous copy of the `fragment`
   in a process known as `mergeback`.

   :param pkgconf_client_t* client: The pkgconf client being accessed.
   :param pkgconf_list_t* list: The list the fragment is being added to.
   :param pkgconf_fragment_t* base: The fragment being copied.
   :param bool is_private: Whether the fragment list is a `private` fragment list (static linking).
   :return: nothing

.. c:function:: void pkgconf_fragment_copy_list(const pkgconf_client_t *client, pkgconf_list_t *list, const pkgconf_list_t *base)

   Copies a `fragment list` to another `fragment list`, possibly removing a previous copy of the fragments
   in a process known as `mergeback`.

   :param pkgconf_client_t* client: The pkgconf client being accessed.
   :param pkgconf_list_t* list: The list the fragments are being added to.
   :param pkgconf_list_t* base: The list the fragments are being copied from.
   :return: nothing

.. c:function:: void pkgconf_fragment_filter(const pkgconf_client_t *client, pkgconf_list_t *dest, pkgconf_list_t *src, pkgconf_fragment_filter_func_t filter_func)

   Copies a `fragment list` to another `fragment list` which match a user-specified filtering function.

   :param pkgconf_client_t* client: The pkgconf client being accessed.
   :param pkgconf_list_t* dest: The destination list.
   :param pkgconf_list_t* src: The source list.
   :param pkgconf_fragment_filter_func_t filter_func: The filter function to use.
   :param void* data: Optional data to pass to the filter function.
   :return: nothing

.. c:function:: size_t pkgconf_fragment_render_len(const pkgconf_list_t *list, bool escape, const pkgconf_fragment_render_ops_t *ops)

   Calculates the required memory to store a `fragment list` when rendered as a string.

   :param pkgconf_list_t* list: The `fragment list` being rendered.
   :param bool escape: Whether or not to escape special shell characters (deprecated).
   :param pkgconf_fragment_render_ops_t* ops: An optional ops structure to use for custom renderers, else ``NULL``.
   :return: the amount of bytes required to represent the `fragment list` when rendered
   :rtype: size_t

.. c:function:: void pkgconf_fragment_render_buf(const pkgconf_list_t *list, char *buf, size_t buflen, bool escape, const pkgconf_fragment_render_ops_t *ops)

   Renders a `fragment list` into a buffer.

   :param pkgconf_list_t* list: The `fragment list` being rendered.
   :param char* buf: The buffer to render the fragment list into.
   :param size_t buflen: The length of the buffer.
   :param bool escape: Whether or not to escape special shell characters (deprecated).
   :param pkgconf_fragment_render_ops_t* ops: An optional ops structure to use for custom renderers, else ``NULL``.
   :return: nothing

.. c:function:: char *pkgconf_fragment_render(const pkgconf_list_t *list)

   Allocate memory and render a `fragment list` into it.

   :param pkgconf_list_t* list: The `fragment list` being rendered.
   :param bool escape: Whether or not to escape special shell characters (deprecated).
   :param pkgconf_fragment_render_ops_t* ops: An optional ops structure to use for custom renderers, else ``NULL``.
   :return: An allocated string containing the rendered `fragment list`.
   :rtype: char *

.. c:function:: void pkgconf_fragment_delete(pkgconf_list_t *list, pkgconf_fragment_t *node)

   Delete a `fragment node` from a `fragment list`.

   :param pkgconf_list_t* list: The `fragment list` to delete from.
   :param pkgconf_fragment_t* node: The `fragment node` to delete.
   :return: nothing

.. c:function:: void pkgconf_fragment_free(pkgconf_list_t *list)

   Delete an entire `fragment list`.

   :param pkgconf_list_t* list: The `fragment list` to delete.
   :return: nothing

.. c:function:: bool pkgconf_fragment_parse(const pkgconf_client_t *client, pkgconf_list_t *list, pkgconf_list_t *vars, const char *value)

   Parse a string into a `fragment list`.

   :param pkgconf_client_t* client: The pkgconf client being accessed.
   :param pkgconf_list_t* list: The `fragment list` to add the fragment entries to.
   :param pkgconf_list_t* vars: A list of variables to use for variable substitution.
   :param uint flags: Any parsing flags to be aware of.
   :param char* value: The string to parse into fragments.
   :return: true on success, false on parse error
