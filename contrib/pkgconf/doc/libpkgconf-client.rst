
libpkgconf `client` module
==========================

The libpkgconf `client` module implements the `pkgconf_client_t` "client" object.
Client objects store all necessary state for libpkgconf allowing for multiple instances to run
in parallel.

Client objects are not thread safe, in other words, a client object should not be shared across
thread boundaries.

.. c:function:: void pkgconf_client_dir_list_build(pkgconf_client_t *client)

   Bootstraps the package search paths.  If the ``PKGCONF_PKG_PKGF_ENV_ONLY`` `flag` is set on the client,
   then only the ``PKG_CONFIG_PATH`` environment variable will be used, otherwise both the
   ``PKG_CONFIG_PATH`` and ``PKG_CONFIG_LIBDIR`` environment variables will be used.

   :param pkgconf_client_t* client: The pkgconf client object to bootstrap.
   :return: nothing

.. c:function:: void pkgconf_client_init(pkgconf_client_t *client, pkgconf_error_handler_func_t error_handler, void *error_handler_data, const pkgconf_cross_personality_t *personality)

   Initialise a pkgconf client object.

   :param pkgconf_client_t* client: The client to initialise.
   :param pkgconf_error_handler_func_t error_handler: An optional error handler to use for logging errors.
   :param void* error_handler_data: user data passed to optional error handler
   :param pkgconf_cross_personality_t* personality: the cross-compile personality to use for defaults
   :return: nothing

.. c:function:: pkgconf_client_t* pkgconf_client_new(pkgconf_error_handler_func_t error_handler, void *error_handler_data, const pkgconf_cross_personality_t *personality)

   Allocate and initialise a pkgconf client object.

   :param pkgconf_error_handler_func_t error_handler: An optional error handler to use for logging errors.
   :param void* error_handler_data: user data passed to optional error handler
   :param pkgconf_cross_personality_t* personality: cross-compile personality to use
   :return: A pkgconf client object.
   :rtype: pkgconf_client_t*

.. c:function:: void pkgconf_client_deinit(pkgconf_client_t *client)

   Release resources belonging to a pkgconf client object.

   :param pkgconf_client_t* client: The client to deinitialise.
   :return: nothing

.. c:function:: void pkgconf_client_free(pkgconf_client_t *client)

   Release resources belonging to a pkgconf client object and then free the client object itself.

   :param pkgconf_client_t* client: The client to deinitialise and free.
   :return: nothing

.. c:function:: const char *pkgconf_client_get_sysroot_dir(const pkgconf_client_t *client)

   Retrieves the client's sysroot directory (if any).

   :param pkgconf_client_t* client: The client object being accessed.
   :return: A string containing the sysroot directory or NULL.
   :rtype: const char *

.. c:function:: void pkgconf_client_set_sysroot_dir(pkgconf_client_t *client, const char *sysroot_dir)

   Sets or clears the sysroot directory on a client object.  Any previous sysroot directory setting is
   automatically released if one was previously set.

   Additionally, the global tuple ``$(pc_sysrootdir)`` is set as appropriate based on the new setting.

   :param pkgconf_client_t* client: The client object being modified.
   :param char* sysroot_dir: The sysroot directory to set or NULL to unset.
   :return: nothing

.. c:function:: const char *pkgconf_client_get_buildroot_dir(const pkgconf_client_t *client)

   Retrieves the client's buildroot directory (if any).

   :param pkgconf_client_t* client: The client object being accessed.
   :return: A string containing the buildroot directory or NULL.
   :rtype: const char *

.. c:function:: void pkgconf_client_set_buildroot_dir(pkgconf_client_t *client, const char *buildroot_dir)

   Sets or clears the buildroot directory on a client object.  Any previous buildroot directory setting is
   automatically released if one was previously set.

   Additionally, the global tuple ``$(pc_top_builddir)`` is set as appropriate based on the new setting.

   :param pkgconf_client_t* client: The client object being modified.
   :param char* buildroot_dir: The buildroot directory to set or NULL to unset.
   :return: nothing

.. c:function:: bool pkgconf_error(const pkgconf_client_t *client, const char *format, ...)

   Report an error to a client-registered error handler.

   :param pkgconf_client_t* client: The pkgconf client object to report the error to.
   :param char* format: A printf-style format string to use for formatting the error message.
   :return: true if the error handler processed the message, else false.
   :rtype: bool

.. c:function:: bool pkgconf_warn(const pkgconf_client_t *client, const char *format, ...)

   Report an error to a client-registered warn handler.

   :param pkgconf_client_t* client: The pkgconf client object to report the error to.
   :param char* format: A printf-style format string to use for formatting the warning message.
   :return: true if the warn handler processed the message, else false.
   :rtype: bool

.. c:function:: bool pkgconf_trace(const pkgconf_client_t *client, const char *filename, size_t len, const char *funcname, const char *format, ...)

   Report a message to a client-registered trace handler.

   :param pkgconf_client_t* client: The pkgconf client object to report the trace message to.
   :param char* filename: The file the function is in.
   :param size_t lineno: The line number currently being executed.
   :param char* funcname: The function name to use.
   :param char* format: A printf-style format string to use for formatting the trace message.
   :return: true if the trace handler processed the message, else false.
   :rtype: bool

.. c:function:: bool pkgconf_default_error_handler(const char *msg, const pkgconf_client_t *client, const void *data)

   The default pkgconf error handler.

   :param char* msg: The error message to handle.
   :param pkgconf_client_t* client: The client object the error originated from.
   :param void* data: An opaque pointer to extra data associated with the client for error handling.
   :return: true (the function does nothing to process the message)
   :rtype: bool

.. c:function:: unsigned int pkgconf_client_get_flags(const pkgconf_client_t *client)

   Retrieves resolver-specific flags associated with a client object.

   :param pkgconf_client_t* client: The client object to retrieve the resolver-specific flags from.
   :return: a bitfield of resolver-specific flags
   :rtype: uint

.. c:function:: void pkgconf_client_set_flags(pkgconf_client_t *client, unsigned int flags)

   Sets resolver-specific flags associated with a client object.

   :param pkgconf_client_t* client: The client object to set the resolver-specific flags on.
   :return: nothing

.. c:function:: const char *pkgconf_client_get_prefix_varname(const pkgconf_client_t *client)

   Retrieves the name of the variable that should contain a module's prefix.
   In some cases, it is necessary to override this variable to allow proper path relocation.

   :param pkgconf_client_t* client: The client object to retrieve the prefix variable name from.
   :return: the prefix variable name as a string
   :rtype: const char *

.. c:function:: void pkgconf_client_set_prefix_varname(pkgconf_client_t *client, const char *prefix_varname)

   Sets the name of the variable that should contain a module's prefix.
   If the variable name is ``NULL``, then the default variable name (``prefix``) is used.

   :param pkgconf_client_t* client: The client object to set the prefix variable name on.
   :param char* prefix_varname: The prefix variable name to set.
   :return: nothing

.. c:function:: pkgconf_client_get_warn_handler(const pkgconf_client_t *client)

   Returns the warning handler if one is set, else ``NULL``.

   :param pkgconf_client_t* client: The client object to get the warn handler from.
   :return: a function pointer to the warn handler or ``NULL``

.. c:function:: pkgconf_client_set_warn_handler(pkgconf_client_t *client, pkgconf_error_handler_func_t warn_handler, void *warn_handler_data)

   Sets a warn handler on a client object or uninstalls one if set to ``NULL``.

   :param pkgconf_client_t* client: The client object to set the warn handler on.
   :param pkgconf_error_handler_func_t warn_handler: The warn handler to set.
   :param void* warn_handler_data: Optional data to associate with the warn handler.
   :return: nothing

.. c:function:: pkgconf_client_get_error_handler(const pkgconf_client_t *client)

   Returns the error handler if one is set, else ``NULL``.

   :param pkgconf_client_t* client: The client object to get the error handler from.
   :return: a function pointer to the error handler or ``NULL``

.. c:function:: pkgconf_client_set_error_handler(pkgconf_client_t *client, pkgconf_error_handler_func_t error_handler, void *error_handler_data)

   Sets a warn handler on a client object or uninstalls one if set to ``NULL``.

   :param pkgconf_client_t* client: The client object to set the error handler on.
   :param pkgconf_error_handler_func_t error_handler: The error handler to set.
   :param void* error_handler_data: Optional data to associate with the error handler.
   :return: nothing

.. c:function:: pkgconf_client_get_trace_handler(const pkgconf_client_t *client)

   Returns the error handler if one is set, else ``NULL``.

   :param pkgconf_client_t* client: The client object to get the error handler from.
   :return: a function pointer to the error handler or ``NULL``

.. c:function:: pkgconf_client_set_trace_handler(pkgconf_client_t *client, pkgconf_error_handler_func_t trace_handler, void *trace_handler_data)

   Sets a warn handler on a client object or uninstalls one if set to ``NULL``.

   :param pkgconf_client_t* client: The client object to set the error handler on.
   :param pkgconf_error_handler_func_t trace_handler: The error handler to set.
   :param void* trace_handler_data: Optional data to associate with the error handler.
   :return: nothing
