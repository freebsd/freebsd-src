/*
 * core.h
 * core, printer functions
 *
 * Copyright (c) 2011-2025 pkgconf authors (see AUTHORS).
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#ifndef __CLI_CORE_H
#define __CLI_CORE_H

#define PKG_CFLAGS_ONLY_I		(((uint64_t) 1) << 2)
#define PKG_CFLAGS_ONLY_OTHER		(((uint64_t) 1) << 3)
#define PKG_CFLAGS			(PKG_CFLAGS_ONLY_I|PKG_CFLAGS_ONLY_OTHER)
#define PKG_LIBS_ONLY_LDPATH		(((uint64_t) 1) << 5)
#define PKG_LIBS_ONLY_LIBNAME		(((uint64_t) 1) << 6)
#define PKG_LIBS_ONLY_OTHER		(((uint64_t) 1) << 7)
#define PKG_LIBS			(PKG_LIBS_ONLY_LDPATH|PKG_LIBS_ONLY_LIBNAME|PKG_LIBS_ONLY_OTHER)
#define PKG_MODVERSION			(((uint64_t) 1) << 8)
#define PKG_REQUIRES			(((uint64_t) 1) << 9)
#define PKG_REQUIRES_PRIVATE		(((uint64_t) 1) << 10)
#define PKG_VARIABLES			(((uint64_t) 1) << 11)
#define PKG_DIGRAPH			(((uint64_t) 1) << 12)
#define PKG_KEEP_SYSTEM_CFLAGS		(((uint64_t) 1) << 13)
#define PKG_KEEP_SYSTEM_LIBS		(((uint64_t) 1) << 14)
#define PKG_VERSION			(((uint64_t) 1) << 15)
#define PKG_ABOUT			(((uint64_t) 1) << 16)
#define PKG_ENV_ONLY			(((uint64_t) 1) << 17)
#define PKG_ERRORS_ON_STDOUT		(((uint64_t) 1) << 18)
#define PKG_SILENCE_ERRORS		(((uint64_t) 1) << 19)
#define PKG_IGNORE_CONFLICTS		(((uint64_t) 1) << 20)
#define PKG_STATIC			(((uint64_t) 1) << 21)
#define PKG_NO_UNINSTALLED		(((uint64_t) 1) << 22)
#define PKG_UNINSTALLED			(((uint64_t) 1) << 23)
#define PKG_LIST			(((uint64_t) 1) << 24)
#define PKG_HELP			(((uint64_t) 1) << 25)
#define PKG_PRINT_ERRORS		(((uint64_t) 1) << 26)
#define PKG_SIMULATE			(((uint64_t) 1) << 27)
#define PKG_NO_CACHE			(((uint64_t) 1) << 28)
#define PKG_PROVIDES			(((uint64_t) 1) << 29)
#define PKG_VALIDATE			(((uint64_t) 1) << 30)
#define PKG_LIST_PACKAGE_NAMES		(((uint64_t) 1) << 31)
#define PKG_NO_PROVIDES			(((uint64_t) 1) << 32)
#define PKG_PURE			(((uint64_t) 1) << 33)
#define PKG_PATH			(((uint64_t) 1) << 34)
#define PKG_DEFINE_PREFIX		(((uint64_t) 1) << 35)
#define PKG_DONT_DEFINE_PREFIX		(((uint64_t) 1) << 36)
#define PKG_DONT_RELOCATE_PATHS		(((uint64_t) 1) << 37)
#define PKG_DEBUG			(((uint64_t) 1) << 38)
#define PKG_SHORT_ERRORS		(((uint64_t) 1) << 39)
#define PKG_EXISTS			(((uint64_t) 1) << 40)
#define PKG_MSVC_SYNTAX			(((uint64_t) 1) << 41)
#define PKG_INTERNAL_CFLAGS		(((uint64_t) 1) << 42)
#define PKG_DUMP_PERSONALITY		(((uint64_t) 1) << 43)
#define PKG_SHARED			(((uint64_t) 1) << 44)
#define PKG_DUMP_LICENSE		(((uint64_t) 1) << 45)
#define PKG_SOLUTION			(((uint64_t) 1) << 46)
#define PKG_EXISTS_CFLAGS		(((uint64_t) 1) << 47)
#define PKG_FRAGMENT_TREE		(((uint64_t) 1) << 48)
#define PKG_DUMP_SOURCE			(((uint64_t) 1) << 49)
#define PKG_DUMP_LICENSE_FILE		(((uint64_t) 1) << 50)
#define PKG_NEWLINES			(((uint64_t) 1) << 51)

typedef struct {
	pkgconf_client_t pkg_client;
	pkgconf_fragment_render_ops_t *want_render_ops;

	uint64_t want_flags;
	int verbosity;
	int maximum_traverse_depth;
	size_t maximum_package_count;

	const char *want_variable;
	const char *want_fragment_filter;
	const char *want_env_prefix;

	char *required_pkgconfig_version;
	const char *required_exact_module_version;
	const char *required_max_module_version;
	const char *required_module_version;

	FILE *error_msgout;
	FILE *logfile_out;

	bool opened_error_msgout;
} pkgconf_cli_state_t;

extern void path_list_to_buffer(const pkgconf_list_t *list, pkgconf_buffer_t *buffer, char delim);
extern int pkgconf_cli_run(pkgconf_cli_state_t *state, int argc, char *argv[], int last_argc);
extern void pkgconf_cli_state_reset(pkgconf_cli_state_t *state);

#endif
