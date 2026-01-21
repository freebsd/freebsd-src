/*
 * client.c
 * libpkgconf consumer lifecycle management
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

/*
 * !doc
 *
 * libpkgconf `client` module
 * ==========================
 *
 * The libpkgconf `client` module implements the `pkgconf_client_t` "client" object.
 * Client objects store all necessary state for libpkgconf allowing for multiple instances to run
 * in parallel.
 *
 * Client objects are not thread safe, in other words, a client object should not be shared across
 * thread boundaries.
 */

static void
trace_path_list(const pkgconf_client_t *client, const char *desc, pkgconf_list_t *list)
{
	const pkgconf_node_t *n;

	PKGCONF_TRACE(client, "%s:", desc);
	PKGCONF_FOREACH_LIST_ENTRY(list->head, n)
	{
		const pkgconf_path_t *p = n->data;

		PKGCONF_TRACE(client, "  - '%s'", p->path);
	}
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_client_dir_list_build(pkgconf_client_t *client)
 *
 *    Bootstraps the package search paths.  If the ``PKGCONF_PKG_PKGF_ENV_ONLY`` `flag` is set on the client,
 *    then only the ``PKG_CONFIG_PATH`` environment variable will be used, otherwise both the
 *    ``PKG_CONFIG_PATH`` and ``PKG_CONFIG_LIBDIR`` environment variables will be used.
 *
 *    :param pkgconf_client_t* client: The pkgconf client object to bootstrap.
 *    :return: nothing
 */
void
pkgconf_client_dir_list_build(pkgconf_client_t *client, const pkgconf_cross_personality_t *personality)
{
	pkgconf_path_build_from_environ("PKG_CONFIG_PATH", NULL, &client->dir_list, true);

	if (!(client->flags & PKGCONF_PKG_PKGF_ENV_ONLY))
	{
		pkgconf_list_t dir_list = PKGCONF_LIST_INITIALIZER;
		const pkgconf_list_t *prepend_list = &personality->dir_list;

		if (getenv("PKG_CONFIG_LIBDIR") != NULL)
		{
			/* PKG_CONFIG_LIBDIR= should empty the search path entirely. */
			(void) pkgconf_path_build_from_environ("PKG_CONFIG_LIBDIR", NULL, &dir_list, true);
			prepend_list = &dir_list;
		}

		pkgconf_path_copy_list(&client->dir_list, prepend_list);
		pkgconf_path_free(&dir_list);
	}
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_client_init(pkgconf_client_t *client, pkgconf_error_handler_func_t error_handler, void *error_handler_data, const pkgconf_cross_personality_t *personality)
 *
 *    Initialise a pkgconf client object.
 *
 *    :param pkgconf_client_t* client: The client to initialise.
 *    :param pkgconf_error_handler_func_t error_handler: An optional error handler to use for logging errors.
 *    :param void* error_handler_data: user data passed to optional error handler
 *    :param pkgconf_cross_personality_t* personality: the cross-compile personality to use for defaults
 *    :return: nothing
 */
void
pkgconf_client_init(pkgconf_client_t *client, pkgconf_error_handler_func_t error_handler, void *error_handler_data, const pkgconf_cross_personality_t *personality)
{
	client->error_handler_data = error_handler_data;
	client->error_handler = error_handler;
	client->auditf = NULL;
	client->cache_table = NULL;
	client->cache_count = 0;

#ifndef PKGCONF_LITE
	if (client->trace_handler == NULL)
		pkgconf_client_set_trace_handler(client, NULL, NULL);
#endif

	pkgconf_client_set_error_handler(client, error_handler, error_handler_data);
	pkgconf_client_set_warn_handler(client, NULL, NULL);

	pkgconf_client_set_sysroot_dir(client, personality->sysroot_dir);
	pkgconf_client_set_buildroot_dir(client, NULL);
	pkgconf_client_set_prefix_varname(client, NULL);

	if(getenv("PKG_CONFIG_SYSTEM_LIBRARY_PATH") == NULL)
		pkgconf_path_copy_list(&client->filter_libdirs, &personality->filter_libdirs);
	else
		pkgconf_path_build_from_environ("PKG_CONFIG_SYSTEM_LIBRARY_PATH", NULL, &client->filter_libdirs, false);

	if(getenv("PKG_CONFIG_SYSTEM_INCLUDE_PATH") == NULL)
		pkgconf_path_copy_list(&client->filter_includedirs, &personality->filter_includedirs);
	else
		pkgconf_path_build_from_environ("PKG_CONFIG_SYSTEM_INCLUDE_PATH", NULL, &client->filter_includedirs, false);

	/* GCC uses these environment variables to define system include paths, so we should check them. */
#ifdef __HAIKU__
	pkgconf_path_build_from_environ("BELIBRARIES", NULL, &client->filter_libdirs, false);
#else
	pkgconf_path_build_from_environ("LIBRARY_PATH", NULL, &client->filter_libdirs, false);
#endif
	pkgconf_path_build_from_environ("CPATH", NULL, &client->filter_includedirs, false);
	pkgconf_path_build_from_environ("C_INCLUDE_PATH", NULL, &client->filter_includedirs, false);
	pkgconf_path_build_from_environ("CPLUS_INCLUDE_PATH", NULL, &client->filter_includedirs, false);
	pkgconf_path_build_from_environ("OBJC_INCLUDE_PATH", NULL, &client->filter_includedirs, false);

#ifdef _WIN32
	/* also use the path lists that MSVC uses on windows */
	pkgconf_path_build_from_environ("INCLUDE", NULL, &client->filter_includedirs, false);
#endif

	PKGCONF_TRACE(client, "initialized client @%p", client);

	trace_path_list(client, "filtered library paths", &client->filter_libdirs);
	trace_path_list(client, "filtered include paths", &client->filter_includedirs);
}

/*
 * !doc
 *
 * .. c:function:: pkgconf_client_t* pkgconf_client_new(pkgconf_error_handler_func_t error_handler, void *error_handler_data, const pkgconf_cross_personality_t *personality)
 *
 *    Allocate and initialise a pkgconf client object.
 *
 *    :param pkgconf_error_handler_func_t error_handler: An optional error handler to use for logging errors.
 *    :param void* error_handler_data: user data passed to optional error handler
 *    :param pkgconf_cross_personality_t* personality: cross-compile personality to use
 *    :return: A pkgconf client object.
 *    :rtype: pkgconf_client_t*
 */
pkgconf_client_t *
pkgconf_client_new(pkgconf_error_handler_func_t error_handler, void *error_handler_data, const pkgconf_cross_personality_t *personality)
{
	pkgconf_client_t *out = calloc(1, sizeof(pkgconf_client_t));
	pkgconf_client_init(out, error_handler, error_handler_data, personality);
	return out;
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_client_deinit(pkgconf_client_t *client)
 *
 *    Release resources belonging to a pkgconf client object.
 *
 *    :param pkgconf_client_t* client: The client to deinitialise.
 *    :return: nothing
 */
void
pkgconf_client_deinit(pkgconf_client_t *client)
{
	PKGCONF_TRACE(client, "deinit @%p", client);

	if (client->prefix_varname != NULL)
		free(client->prefix_varname);

	if (client->sysroot_dir != NULL)
		free(client->sysroot_dir);

	if (client->buildroot_dir != NULL)
		free(client->buildroot_dir);

	pkgconf_path_free(&client->filter_libdirs);
	pkgconf_path_free(&client->filter_includedirs);

	pkgconf_tuple_free_global(client);
	pkgconf_path_free(&client->dir_list);
	pkgconf_cache_free(client);
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_client_free(pkgconf_client_t *client)
 *
 *    Release resources belonging to a pkgconf client object and then free the client object itself.
 *
 *    :param pkgconf_client_t* client: The client to deinitialise and free.
 *    :return: nothing
 */
void
pkgconf_client_free(pkgconf_client_t *client)
{
	pkgconf_client_deinit(client);
	free(client);
}

/*
 * !doc
 *
 * .. c:function:: const char *pkgconf_client_get_sysroot_dir(const pkgconf_client_t *client)
 *
 *    Retrieves the client's sysroot directory (if any).
 *
 *    :param pkgconf_client_t* client: The client object being accessed.
 *    :return: A string containing the sysroot directory or NULL.
 *    :rtype: const char *
 */
const char *
pkgconf_client_get_sysroot_dir(const pkgconf_client_t *client)
{
	return client->sysroot_dir;
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_client_set_sysroot_dir(pkgconf_client_t *client, const char *sysroot_dir)
 *
 *    Sets or clears the sysroot directory on a client object.  Any previous sysroot directory setting is
 *    automatically released if one was previously set.
 *
 *    Additionally, the global tuple ``$(pc_sysrootdir)`` is set as appropriate based on the new setting.
 *
 *    :param pkgconf_client_t* client: The client object being modified.
 *    :param char* sysroot_dir: The sysroot directory to set or NULL to unset.
 *    :return: nothing
 */
void
pkgconf_client_set_sysroot_dir(pkgconf_client_t *client, const char *sysroot_dir)
{
	if (client->sysroot_dir != NULL)
		free(client->sysroot_dir);

	client->sysroot_dir = sysroot_dir != NULL ? strdup(sysroot_dir) : NULL;

	PKGCONF_TRACE(client, "set sysroot_dir to: %s", client->sysroot_dir != NULL ? client->sysroot_dir : "<default>");

	pkgconf_tuple_add_global(client, "pc_sysrootdir", client->sysroot_dir != NULL ? client->sysroot_dir : "/");
}

/*
 * !doc
 *
 * .. c:function:: const char *pkgconf_client_get_buildroot_dir(const pkgconf_client_t *client)
 *
 *    Retrieves the client's buildroot directory (if any).
 *
 *    :param pkgconf_client_t* client: The client object being accessed.
 *    :return: A string containing the buildroot directory or NULL.
 *    :rtype: const char *
 */
const char *
pkgconf_client_get_buildroot_dir(const pkgconf_client_t *client)
{
	return client->buildroot_dir;
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_client_set_buildroot_dir(pkgconf_client_t *client, const char *buildroot_dir)
 *
 *    Sets or clears the buildroot directory on a client object.  Any previous buildroot directory setting is
 *    automatically released if one was previously set.
 *
 *    Additionally, the global tuple ``$(pc_top_builddir)`` is set as appropriate based on the new setting.
 *
 *    :param pkgconf_client_t* client: The client object being modified.
 *    :param char* buildroot_dir: The buildroot directory to set or NULL to unset.
 *    :return: nothing
 */
void
pkgconf_client_set_buildroot_dir(pkgconf_client_t *client, const char *buildroot_dir)
{
	if (client->buildroot_dir != NULL)
		free(client->buildroot_dir);

	client->buildroot_dir = buildroot_dir != NULL ? strdup(buildroot_dir) : NULL;

	PKGCONF_TRACE(client, "set buildroot_dir to: %s", client->buildroot_dir != NULL ? client->buildroot_dir : "<default>");

	pkgconf_tuple_add_global(client, "pc_top_builddir", client->buildroot_dir != NULL ? client->buildroot_dir : "$(top_builddir)");
}

/*
 * !doc
 *
 * .. c:function:: bool pkgconf_error(const pkgconf_client_t *client, const char *format, ...)
 *
 *    Report an error to a client-registered error handler.
 *
 *    :param pkgconf_client_t* client: The pkgconf client object to report the error to.
 *    :param char* format: A printf-style format string to use for formatting the error message.
 *    :return: true if the error handler processed the message, else false.
 *    :rtype: bool
 */
bool
pkgconf_error(const pkgconf_client_t *client, const char *format, ...)
{
	char errbuf[PKGCONF_BUFSIZE];
	va_list va;

	va_start(va, format);
	vsnprintf(errbuf, sizeof errbuf, format, va);
	va_end(va);

	return client->error_handler(errbuf, client, client->error_handler_data);
}

/*
 * !doc
 *
 * .. c:function:: bool pkgconf_warn(const pkgconf_client_t *client, const char *format, ...)
 *
 *    Report an error to a client-registered warn handler.
 *
 *    :param pkgconf_client_t* client: The pkgconf client object to report the error to.
 *    :param char* format: A printf-style format string to use for formatting the warning message.
 *    :return: true if the warn handler processed the message, else false.
 *    :rtype: bool
 */
bool
pkgconf_warn(const pkgconf_client_t *client, const char *format, ...)
{
	char errbuf[PKGCONF_BUFSIZE];
	va_list va;

	va_start(va, format);
	vsnprintf(errbuf, sizeof errbuf, format, va);
	va_end(va);

	return client->warn_handler(errbuf, client, client->warn_handler_data);
}

/*
 * !doc
 *
 * .. c:function:: bool pkgconf_trace(const pkgconf_client_t *client, const char *filename, size_t len, const char *funcname, const char *format, ...)
 *
 *    Report a message to a client-registered trace handler.
 *
 *    :param pkgconf_client_t* client: The pkgconf client object to report the trace message to.
 *    :param char* filename: The file the function is in.
 *    :param size_t lineno: The line number currently being executed.
 *    :param char* funcname: The function name to use.
 *    :param char* format: A printf-style format string to use for formatting the trace message.
 *    :return: true if the trace handler processed the message, else false.
 *    :rtype: bool
 */
bool
pkgconf_trace(const pkgconf_client_t *client, const char *filename, size_t lineno, const char *funcname, const char *format, ...)
{
	char errbuf[PKGCONF_BUFSIZE];
	size_t len;
	va_list va;

	if (client == NULL || client->trace_handler == NULL)
		return false;

	len = snprintf(errbuf, sizeof errbuf, "%s:" SIZE_FMT_SPECIFIER " [%s]: ", filename, lineno, funcname);

	va_start(va, format);
	vsnprintf(errbuf + len, sizeof(errbuf) - len, format, va);
	va_end(va);

	pkgconf_strlcat(errbuf, "\n", sizeof errbuf);

	return client->trace_handler(errbuf, client, client->trace_handler_data);
}

/*
 * !doc
 *
 * .. c:function:: bool pkgconf_default_error_handler(const char *msg, const pkgconf_client_t *client, const void *data)
 *
 *    The default pkgconf error handler.
 *
 *    :param char* msg: The error message to handle.
 *    :param pkgconf_client_t* client: The client object the error originated from.
 *    :param void* data: An opaque pointer to extra data associated with the client for error handling.
 *    :return: true (the function does nothing to process the message)
 *    :rtype: bool
 */
bool
pkgconf_default_error_handler(const char *msg, const pkgconf_client_t *client, void *data)
{
	(void) msg;
	(void) client;
	(void) data;

	return true;
}

/*
 * !doc
 *
 * .. c:function:: unsigned int pkgconf_client_get_flags(const pkgconf_client_t *client)
 *
 *    Retrieves resolver-specific flags associated with a client object.
 *
 *    :param pkgconf_client_t* client: The client object to retrieve the resolver-specific flags from.
 *    :return: a bitfield of resolver-specific flags
 *    :rtype: uint
 */
unsigned int
pkgconf_client_get_flags(const pkgconf_client_t *client)
{
	return client->flags;
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_client_set_flags(pkgconf_client_t *client, unsigned int flags)
 *
 *    Sets resolver-specific flags associated with a client object.
 *
 *    :param pkgconf_client_t* client: The client object to set the resolver-specific flags on.
 *    :return: nothing
 */
void
pkgconf_client_set_flags(pkgconf_client_t *client, unsigned int flags)
{
	client->flags = flags;
}

/*
 * !doc
 *
 * .. c:function:: const char *pkgconf_client_get_prefix_varname(const pkgconf_client_t *client)
 *
 *    Retrieves the name of the variable that should contain a module's prefix.
 *    In some cases, it is necessary to override this variable to allow proper path relocation.
 *
 *    :param pkgconf_client_t* client: The client object to retrieve the prefix variable name from.
 *    :return: the prefix variable name as a string
 *    :rtype: const char *
 */
const char *
pkgconf_client_get_prefix_varname(const pkgconf_client_t *client)
{
	return client->prefix_varname;
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_client_set_prefix_varname(pkgconf_client_t *client, const char *prefix_varname)
 *
 *    Sets the name of the variable that should contain a module's prefix.
 *    If the variable name is ``NULL``, then the default variable name (``prefix``) is used.
 *
 *    :param pkgconf_client_t* client: The client object to set the prefix variable name on.
 *    :param char* prefix_varname: The prefix variable name to set.
 *    :return: nothing
 */
void
pkgconf_client_set_prefix_varname(pkgconf_client_t *client, const char *prefix_varname)
{
	if (prefix_varname == NULL)
		prefix_varname = "prefix";

	if (client->prefix_varname != NULL)
		free(client->prefix_varname);

	client->prefix_varname = strdup(prefix_varname);

	PKGCONF_TRACE(client, "set prefix_varname to: %s", client->prefix_varname);
}

/*
 * !doc
 *
 * .. c:function:: pkgconf_client_get_warn_handler(const pkgconf_client_t *client)
 *
 *    Returns the warning handler if one is set, else ``NULL``.
 *
 *    :param pkgconf_client_t* client: The client object to get the warn handler from.
 *    :return: a function pointer to the warn handler or ``NULL``
 */
pkgconf_error_handler_func_t
pkgconf_client_get_warn_handler(const pkgconf_client_t *client)
{
	return client->warn_handler;
}

/*
 * !doc
 *
 * .. c:function:: pkgconf_client_set_warn_handler(pkgconf_client_t *client, pkgconf_error_handler_func_t warn_handler, void *warn_handler_data)
 *
 *    Sets a warn handler on a client object or uninstalls one if set to ``NULL``.
 *
 *    :param pkgconf_client_t* client: The client object to set the warn handler on.
 *    :param pkgconf_error_handler_func_t warn_handler: The warn handler to set.
 *    :param void* warn_handler_data: Optional data to associate with the warn handler.
 *    :return: nothing
 */
void
pkgconf_client_set_warn_handler(pkgconf_client_t *client, pkgconf_error_handler_func_t warn_handler, void *warn_handler_data)
{
	client->warn_handler = warn_handler;
	client->warn_handler_data = warn_handler_data;

	if (client->warn_handler == NULL)
	{
		PKGCONF_TRACE(client, "installing default warn handler");
		client->warn_handler = pkgconf_default_error_handler;
	}
}

/*
 * !doc
 *
 * .. c:function:: pkgconf_client_get_error_handler(const pkgconf_client_t *client)
 *
 *    Returns the error handler if one is set, else ``NULL``.
 *
 *    :param pkgconf_client_t* client: The client object to get the error handler from.
 *    :return: a function pointer to the error handler or ``NULL``
 */
pkgconf_error_handler_func_t
pkgconf_client_get_error_handler(const pkgconf_client_t *client)
{
	return client->error_handler;
}

/*
 * !doc
 *
 * .. c:function:: pkgconf_client_set_error_handler(pkgconf_client_t *client, pkgconf_error_handler_func_t error_handler, void *error_handler_data)
 *
 *    Sets a warn handler on a client object or uninstalls one if set to ``NULL``.
 *
 *    :param pkgconf_client_t* client: The client object to set the error handler on.
 *    :param pkgconf_error_handler_func_t error_handler: The error handler to set.
 *    :param void* error_handler_data: Optional data to associate with the error handler.
 *    :return: nothing
 */
void
pkgconf_client_set_error_handler(pkgconf_client_t *client, pkgconf_error_handler_func_t error_handler, void *error_handler_data)
{
	client->error_handler = error_handler;
	client->error_handler_data = error_handler_data;

	if (client->error_handler == NULL)
	{
		PKGCONF_TRACE(client, "installing default error handler");
		client->error_handler = pkgconf_default_error_handler;
	}
}

#ifndef PKGCONF_LITE
/*
 * !doc
 *
 * .. c:function:: pkgconf_client_get_trace_handler(const pkgconf_client_t *client)
 *
 *    Returns the error handler if one is set, else ``NULL``.
 *
 *    :param pkgconf_client_t* client: The client object to get the error handler from.
 *    :return: a function pointer to the error handler or ``NULL``
 */
pkgconf_error_handler_func_t
pkgconf_client_get_trace_handler(const pkgconf_client_t *client)
{
	return client->trace_handler;
}

/*
 * !doc
 *
 * .. c:function:: pkgconf_client_set_trace_handler(pkgconf_client_t *client, pkgconf_error_handler_func_t trace_handler, void *trace_handler_data)
 *
 *    Sets a warn handler on a client object or uninstalls one if set to ``NULL``.
 *
 *    :param pkgconf_client_t* client: The client object to set the error handler on.
 *    :param pkgconf_error_handler_func_t trace_handler: The error handler to set.
 *    :param void* trace_handler_data: Optional data to associate with the error handler.
 *    :return: nothing
 */
void
pkgconf_client_set_trace_handler(pkgconf_client_t *client, pkgconf_error_handler_func_t trace_handler, void *trace_handler_data)
{
	client->trace_handler = trace_handler;
	client->trace_handler_data = trace_handler_data;

	if (client->trace_handler == NULL)
	{
		client->trace_handler = pkgconf_default_error_handler;
		PKGCONF_TRACE(client, "installing default trace handler");
	}
}
#endif
