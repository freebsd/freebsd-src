
libpkgconf `tuple` module
=========================

The `tuple` module provides key-value mappings backed by a linked list.  The key-value
mapping is mainly used for variable substitution when parsing .pc files.

There are two sets of mappings: a ``pkgconf_pkg_t`` specific mapping, and a `global` mapping.
The `tuple` module provides convenience wrappers for managing the `global` mapping, which is
attached to a given client object.

.. c:function:: void pkgconf_tuple_add_global(pkgconf_client_t *client, const char *key, const char *value)

   Defines a global variable, replacing the previous declaration if one was set.

   :param pkgconf_client_t* client: The pkgconf client object to modify.
   :param char* key: The key for the mapping (variable name).
   :param char* value: The value for the mapped entry.
   :return: nothing

.. c:function:: void pkgconf_tuple_find_global(const pkgconf_client_t *client, const char *key)

   Looks up a global variable.

   :param pkgconf_client_t* client: The pkgconf client object to access.
   :param char* key: The key or variable name to look up.
   :return: the contents of the variable or ``NULL``
   :rtype: char *

.. c:function:: void pkgconf_tuple_free_global(pkgconf_client_t *client)

   Delete all global variables associated with a pkgconf client object.

   :param pkgconf_client_t* client: The pkgconf client object to modify.
   :return: nothing

.. c:function:: void pkgconf_tuple_define_global(pkgconf_client_t *client, const char *kv)

   Parse and define a global variable.

   :param pkgconf_client_t* client: The pkgconf client object to modify.
   :param char* kv: The variable in the form of ``key=value``.
   :return: nothing

.. c:function:: pkgconf_tuple_t *pkgconf_tuple_add(const pkgconf_client_t *client, pkgconf_list_t *list, const char *key, const char *value, bool parse)

   Optionally parse and then define a variable.

   :param pkgconf_client_t* client: The pkgconf client object to access.
   :param pkgconf_list_t* list: The variable list to add the new variable to.
   :param char* key: The name of the variable being added.
   :param char* value: The value of the variable being added.
   :param bool parse: Whether or not to parse the value for variable substitution.
   :return: a variable object
   :rtype: pkgconf_tuple_t *

.. c:function:: char *pkgconf_tuple_find(const pkgconf_client_t *client, pkgconf_list_t *list, const char *key)

   Look up a variable in a variable list.

   :param pkgconf_client_t* client: The pkgconf client object to access.
   :param pkgconf_list_t* list: The variable list to search.
   :param char* key: The variable name to search for.
   :return: the value of the variable or ``NULL``
   :rtype: char *

.. c:function:: char *pkgconf_tuple_parse(const pkgconf_client_t *client, pkgconf_list_t *vars, const char *value, unsigned int flags)

   Parse an expression for variable substitution.

   :param pkgconf_client_t* client: The pkgconf client object to access.
   :param pkgconf_list_t* list: The variable list to search for variables (along side the global variable list).
   :param char* value: The ``key=value`` string to parse.
   :param uint flags: Any flags to consider while parsing.
   :return: the variable data with any variables substituted
   :rtype: char *

.. c:function:: void pkgconf_tuple_free_entry(pkgconf_tuple_t *tuple, pkgconf_list_t *list)

   Deletes a variable object, removing it from any variable lists and releasing any memory associated
   with it.

   :param pkgconf_tuple_t* tuple: The variable object to release.
   :param pkgconf_list_t* list: The variable list the variable object is attached to.
   :return: nothing

.. c:function:: void pkgconf_tuple_free(pkgconf_list_t *list)

   Deletes a variable list and any variables attached to it.

   :param pkgconf_list_t* list: The variable list to delete.
   :return: nothing
