/*
 * libpkgconf.h
 * Global include file for everything in libpkgconf.
 *
 * Copyright (c) 2011, 2015 pkgconf authors (see AUTHORS).
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#ifndef LIBPKGCONF__LIBPKGCONF_H
#define LIBPKGCONF__LIBPKGCONF_H

#include <inttypes.h>
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <libpkgconf/libpkgconf-api.h>
#include <libpkgconf/iter.h>
#include <libpkgconf/bsdstubs.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PKGCONF_BUFSIZE	(65535)

typedef enum {
	PKGCONF_CMP_NOT_EQUAL,
	PKGCONF_CMP_ANY,
	PKGCONF_CMP_LESS_THAN,
	PKGCONF_CMP_LESS_THAN_EQUAL,
	PKGCONF_CMP_EQUAL,
	PKGCONF_CMP_GREATER_THAN,
	PKGCONF_CMP_GREATER_THAN_EQUAL
} pkgconf_pkg_comparator_t;

#define PKGCONF_CMP_COUNT 7

typedef struct pkgconf_pkg_ pkgconf_pkg_t;
typedef struct pkgconf_dependency_ pkgconf_dependency_t;
typedef struct pkgconf_tuple_ pkgconf_tuple_t;
typedef struct pkgconf_buffer_ pkgconf_buffer_t;
typedef struct pkgconf_span_ pkgconf_span_t;
typedef struct pkgconf_fragment_ pkgconf_fragment_t;
typedef struct pkgconf_path_ pkgconf_path_t;
typedef struct pkgconf_client_ pkgconf_client_t;
typedef struct pkgconf_cross_personality_ pkgconf_cross_personality_t;
typedef struct pkgconf_queue_ pkgconf_queue_t;
typedef struct pkgconf_output_ pkgconf_output_t;

#define PKGCONF_ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))

#define PKGCONF_FOREACH_LIST_ENTRY(head, value) \
	for ((value) = (head); (value) != NULL; (value) = (value)->next)

#define PKGCONF_FOREACH_LIST_ENTRY_SAFE(head, nextiter, value) \
	for ((value) = (head), (nextiter) = (head) != NULL ? (head)->next : NULL; (value) != NULL; (value) = (nextiter), (nextiter) = (nextiter) != NULL ? (nextiter)->next : NULL)

#define PKGCONF_FOREACH_LIST_ENTRY_REVERSE(tail, value) \
	for ((value) = (tail); (value) != NULL; (value) = (value)->prev)

#define LIBPKGCONF_VERSION	20501
#define LIBPKGCONF_VERSION_STR	"2.5.1"

struct pkgconf_queue_ {
	pkgconf_node_t iter;
	char *package;

	unsigned int flags;
};

struct pkgconf_fragment_ {
	pkgconf_node_t iter;

	char type;
	char *data;

	pkgconf_list_t children;
	unsigned int flags;
};

#define PKGCONF_PKG_FRAGF_TERMINATED		0x1

struct pkgconf_dependency_ {
	pkgconf_node_t iter;

	char *package;
	pkgconf_pkg_comparator_t compare;
	char *version;
	pkgconf_pkg_t *parent;
	pkgconf_pkg_t *match;

	unsigned int flags;

	int refcount;
	pkgconf_client_t *owner;

	char *why;
};

struct pkgconf_tuple_ {
	pkgconf_node_t iter;

	char *key;
	char *value;

	unsigned int flags;
};

struct pkgconf_buffer_ {
	char *base;
	char *end;
};

#define PKGCONF_PKG_TUPLEF_OVERRIDE		0x1

struct pkgconf_path_ {
	pkgconf_node_t lnode;

	char *path;
	void *handle_path;
	void *handle_device;

	unsigned int flags;
};

#define PKGCONF_PKG_PROPF_NONE			0x00
#define PKGCONF_PKG_PROPF_STATIC		0x01
#define PKGCONF_PKG_PROPF_CACHED		0x02
#define PKGCONF_PKG_PROPF_UNINSTALLED		0x08
#define PKGCONF_PKG_PROPF_VIRTUAL		0x10
#define PKGCONF_PKG_PROPF_ANCESTOR		0x20
#define PKGCONF_PKG_PROPF_VISITED_PRIVATE	0x40
#define PKGCONF_PKG_PROPF_PRELOADED		0x80

struct pkgconf_pkg_ {
	int refcount;
	char *id;
	char *filename;
	char *realname;
	char *version;
	char *description;
	char *url;
	char *pc_filedir;
	char *license;
	char *maintainer;
	char *copyright;
	char *source;
	char *license_file;
	char *why;

	pkgconf_list_t libs;
	pkgconf_list_t libs_private;
	pkgconf_list_t cflags;
	pkgconf_list_t cflags_private;

	pkgconf_list_t required;		/* this used to be requires but that is now a reserved keyword */
	pkgconf_list_t requires_private;
	pkgconf_list_t conflicts;
	pkgconf_list_t provides;

	pkgconf_list_t vars;

	unsigned int flags;

	pkgconf_client_t *owner;

	/* these resources are owned by the package and do not need special management,
	 * under no circumstance attempt to allocate or free objects belonging to these pointers
	 */
	pkgconf_tuple_t *orig_prefix;
	pkgconf_tuple_t *prefix;

	uint64_t serial;
	uint64_t identifier;

	pkgconf_node_t preload_node;
};

typedef bool (*pkgconf_pkg_iteration_func_t)(const pkgconf_pkg_t *pkg, void *data);
typedef void (*pkgconf_pkg_traverse_func_t)(pkgconf_client_t *client, pkgconf_pkg_t *pkg, void *data);
typedef bool (*pkgconf_queue_apply_func_t)(pkgconf_client_t *client, pkgconf_pkg_t *world, void *data, int maxdepth);
typedef bool (*pkgconf_error_handler_func_t)(const char *msg, const pkgconf_client_t *client, void *data);
typedef void (*pkgconf_unveil_handler_func_t)(const pkgconf_client_t *client, const char *path, const char *permissions);
typedef const char *(*pkgconf_environ_lookup_handler_func_t)(const pkgconf_client_t *client, const char *variable);

struct pkgconf_client_ {
	pkgconf_list_t dir_list;

	pkgconf_list_t filter_libdirs;
	pkgconf_list_t filter_includedirs;

	pkgconf_list_t global_vars;

	void *client_data;
	void *error_handler_data;
	void *warn_handler_data;
	void *trace_handler_data;

	pkgconf_error_handler_func_t error_handler;
	pkgconf_error_handler_func_t warn_handler;
	pkgconf_error_handler_func_t trace_handler;

	pkgconf_environ_lookup_handler_func_t environ_lookup_handler;

	FILE *auditf;

	char *sysroot_dir;
	char *buildroot_dir;

	unsigned int flags;

	char *prefix_varname;

	bool already_sent_notice;

	uint64_t serial;
	uint64_t identifier;

	pkgconf_pkg_t **cache_table;
	size_t cache_count;

	pkgconf_unveil_handler_func_t unveil_handler;

	pkgconf_list_t preloaded_pkgs;

	pkgconf_output_t *output;

	const pkgconf_cross_personality_t *personality;
};

struct pkgconf_cross_personality_ {
	char *name;

	pkgconf_list_t dir_list;

	pkgconf_list_t filter_libdirs;
	pkgconf_list_t filter_includedirs;

	char *sysroot_dir;

	bool want_default_static;
	bool want_default_pure;
};

/* client.c */
PKGCONF_API void pkgconf_client_init(pkgconf_client_t *client, pkgconf_error_handler_func_t error_handler, void *error_handler_data, const pkgconf_cross_personality_t *personality, void *client_data, pkgconf_environ_lookup_handler_func_t environ_lookup_handler);
PKGCONF_API pkgconf_client_t * pkgconf_client_new(pkgconf_error_handler_func_t error_handler, void *error_handler_data, const pkgconf_cross_personality_t *personality, void *client_data, pkgconf_environ_lookup_handler_func_t environ_lookup_handler);
PKGCONF_API void pkgconf_client_deinit(pkgconf_client_t *client);
PKGCONF_API void pkgconf_client_free(pkgconf_client_t *client);
PKGCONF_API const char *pkgconf_client_get_sysroot_dir(const pkgconf_client_t *client);
PKGCONF_API void pkgconf_client_set_sysroot_dir(pkgconf_client_t *client, const char *sysroot_dir);
PKGCONF_API const char *pkgconf_client_get_buildroot_dir(const pkgconf_client_t *client);
PKGCONF_API void pkgconf_client_set_buildroot_dir(pkgconf_client_t *client, const char *buildroot_dir);
PKGCONF_API unsigned int pkgconf_client_get_flags(const pkgconf_client_t *client);
PKGCONF_API void pkgconf_client_set_flags(pkgconf_client_t *client, unsigned int flags);
PKGCONF_API const char *pkgconf_client_get_prefix_varname(const pkgconf_client_t *client);
PKGCONF_API void pkgconf_client_set_prefix_varname(pkgconf_client_t *client, const char *prefix_varname);
PKGCONF_API pkgconf_error_handler_func_t pkgconf_client_get_warn_handler(const pkgconf_client_t *client);
PKGCONF_API void pkgconf_client_set_warn_handler(pkgconf_client_t *client, pkgconf_error_handler_func_t warn_handler, void *warn_handler_data);
PKGCONF_API pkgconf_error_handler_func_t pkgconf_client_get_error_handler(const pkgconf_client_t *client);
PKGCONF_API void pkgconf_client_set_error_handler(pkgconf_client_t *client, pkgconf_error_handler_func_t error_handler, void *error_handler_data);
PKGCONF_API pkgconf_error_handler_func_t pkgconf_client_get_trace_handler(const pkgconf_client_t *client);
PKGCONF_API void pkgconf_client_set_trace_handler(pkgconf_client_t *client, pkgconf_error_handler_func_t trace_handler, void *trace_handler_data);
PKGCONF_API pkgconf_unveil_handler_func_t pkgconf_client_get_unveil_handler(const pkgconf_client_t *client);
PKGCONF_API void pkgconf_client_set_unveil_handler(pkgconf_client_t *client, pkgconf_unveil_handler_func_t unveil_handler);
PKGCONF_API void pkgconf_client_dir_list_build(pkgconf_client_t *client, const pkgconf_cross_personality_t *personality);
PKGCONF_API bool pkgconf_client_preload_one(pkgconf_client_t *client, pkgconf_pkg_t *pkg);
PKGCONF_API bool pkgconf_client_preload_path(pkgconf_client_t *client, const char *path);
PKGCONF_API bool pkgconf_client_preload_from_environ(pkgconf_client_t *client, const char *env);
PKGCONF_API void pkgconf_client_set_output(pkgconf_client_t *client, pkgconf_output_t *output);
PKGCONF_API const char *pkgconf_client_getenv(const pkgconf_client_t *client, const char *key);

/* personality.c */
PKGCONF_API pkgconf_cross_personality_t *pkgconf_cross_personality_default(void);
PKGCONF_API pkgconf_cross_personality_t *pkgconf_cross_personality_find(const char *triplet);
PKGCONF_API void pkgconf_cross_personality_deinit(pkgconf_cross_personality_t *personality);

#define PKGCONF_IS_MODULE_SEPARATOR(c) ((c) == ',' || isspace ((unsigned char)(c)))
#define PKGCONF_IS_OPERATOR_CHAR(c) ((c) == '<' || (c) == '>' || (c) == '!' || (c) == '=')

#define PKGCONF_PKG_PKGF_NONE				0x0000
#define PKGCONF_PKG_PKGF_SEARCH_PRIVATE			0x0001
#define PKGCONF_PKG_PKGF_ENV_ONLY			0x0002
#define PKGCONF_PKG_PKGF_NO_UNINSTALLED			0x0004
#define PKGCONF_PKG_PKGF_SKIP_ROOT_VIRTUAL		0x0008
#define PKGCONF_PKG_PKGF_MERGE_PRIVATE_FRAGMENTS	0x0010
#define PKGCONF_PKG_PKGF_SKIP_CONFLICTS			0x0020
#define PKGCONF_PKG_PKGF_NO_CACHE			0x0040
#define PKGCONF_PKG_PKGF_SKIP_ERRORS			0x0080
#define PKGCONF_PKG_PKGF_ITER_PKG_IS_PRIVATE		0x0100
#define PKGCONF_PKG_PKGF_SKIP_PROVIDES			0x0200
#define PKGCONF_PKG_PKGF_REDEFINE_PREFIX		0x0400
#define PKGCONF_PKG_PKGF_DONT_RELOCATE_PATHS		0x0800
#define PKGCONF_PKG_PKGF_SIMPLIFY_ERRORS		0x1000
#define PKGCONF_PKG_PKGF_DONT_FILTER_INTERNAL_CFLAGS	0x2000
#define PKGCONF_PKG_PKGF_DONT_MERGE_SPECIAL_FRAGMENTS	0x4000
#define PKGCONF_PKG_PKGF_FDO_SYSROOT_RULES		0x8000
#define PKGCONF_PKG_PKGF_PKGCONF1_SYSROOT_RULES         0x10000
#define PKGCONF_PKG_PKGF_REQUIRE_INTERNAL		0x20000

#define PKGCONF_PKG_DEPF_INTERNAL		0x1
#define PKGCONF_PKG_DEPF_PRIVATE		0x2
#define PKGCONF_PKG_DEPF_QUERY			0x4

#define PKGCONF_PKG_ERRF_OK			0x0
#define PKGCONF_PKG_ERRF_PACKAGE_NOT_FOUND	0x1
#define PKGCONF_PKG_ERRF_PACKAGE_VER_MISMATCH	0x2
#define PKGCONF_PKG_ERRF_PACKAGE_CONFLICT	0x4
#define PKGCONF_PKG_ERRF_DEPGRAPH_BREAK		0x8

#if __GNUC__ >= 5 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4)
# define PRINTFLIKE(fmtarg, firstvararg) \
         __attribute__((__format__ (gnu_printf, fmtarg, firstvararg)))
#elif defined(__clang__) || defined(__INTEL_COMPILER) || __GNUC__ > 2 || (_GNUC__ == 2 && __GNUC_MINOR__ >= 5)
# define PRINTFLIKE(fmtarg, firstvararg) \
         __attribute__((__format__ (__printf__, fmtarg, firstvararg)))
#else
# define PRINTFLIKE(fmtarg, firstvararg)
#endif

#if defined(__clang__) || defined(__INTEL_COMPILER) || (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1))
# define DEPRECATED __attribute__((deprecated))
#elif defined(_MSC_VER)
# define DEPRECATED __declspec(deprecated)
#else
# define DEPRECATED
#endif

/* pkg.c */
PKGCONF_API bool pkgconf_error(const pkgconf_client_t *client, const char *format, ...) PRINTFLIKE(2, 3);
PKGCONF_API bool pkgconf_warn(const pkgconf_client_t *client, const char *format, ...) PRINTFLIKE(2, 3);
PKGCONF_API bool pkgconf_trace(const pkgconf_client_t *client, const char *filename, size_t lineno, const char *funcname, const char *format, ...) PRINTFLIKE(5, 6);
PKGCONF_API bool pkgconf_default_error_handler(const char *msg, const pkgconf_client_t *client, void *data);

#ifndef PKGCONF_LITE
#if defined(__GNUC__) || defined(__INTEL_COMPILER)
#define PKGCONF_TRACE(client, ...) do { \
		pkgconf_trace(client, __FILE__, __LINE__, __PRETTY_FUNCTION__, __VA_ARGS__); \
	} while (0)
#else
#define PKGCONF_TRACE(client, ...) do { \
		pkgconf_trace(client, __FILE__, __LINE__, __func__, __VA_ARGS__); \
	} while (0)
#endif
#else
#define PKGCONF_TRACE(client, ...)
#endif

PKGCONF_API pkgconf_pkg_t *pkgconf_pkg_ref(pkgconf_client_t *client, pkgconf_pkg_t *pkg);
PKGCONF_API void pkgconf_pkg_unref(pkgconf_client_t *client, pkgconf_pkg_t *pkg);
PKGCONF_API void pkgconf_pkg_free(pkgconf_client_t *client, pkgconf_pkg_t *pkg);
PKGCONF_API pkgconf_pkg_t *pkgconf_pkg_find(pkgconf_client_t *client, const char *name);
PKGCONF_API unsigned int pkgconf_pkg_traverse(pkgconf_client_t *client, pkgconf_pkg_t *root, pkgconf_pkg_traverse_func_t func, void *data, int maxdepth, unsigned int skip_flags);
PKGCONF_API unsigned int pkgconf_pkg_walk_conflicts_list(pkgconf_client_t *client, pkgconf_pkg_t *root, pkgconf_list_t *deplist);
PKGCONF_API unsigned int pkgconf_pkg_verify_graph(pkgconf_client_t *client, pkgconf_pkg_t *root, int depth);
PKGCONF_API pkgconf_pkg_t *pkgconf_pkg_verify_dependency(pkgconf_client_t *client, pkgconf_dependency_t *pkgdep, unsigned int *eflags);
PKGCONF_API const char *pkgconf_pkg_get_comparator(const pkgconf_dependency_t *pkgdep);
PKGCONF_API unsigned int pkgconf_pkg_cflags(pkgconf_client_t *client, pkgconf_pkg_t *root, pkgconf_list_t *list, int maxdepth);
PKGCONF_API unsigned int pkgconf_pkg_libs(pkgconf_client_t *client, pkgconf_pkg_t *root, pkgconf_list_t *list, int maxdepth);
PKGCONF_API pkgconf_pkg_comparator_t pkgconf_pkg_comparator_lookup_by_name(const char *name);

PKGCONF_API int pkgconf_compare_version(const char *a, const char *b);
PKGCONF_API pkgconf_pkg_t *pkgconf_scan_all(pkgconf_client_t *client, void *ptr, pkgconf_pkg_iteration_func_t func);

/* parse.c */
PKGCONF_API pkgconf_pkg_t *pkgconf_pkg_new_from_path(pkgconf_client_t *client, const char *path, unsigned int flags);
PKGCONF_API void pkgconf_dependency_parse_str(pkgconf_client_t *client, pkgconf_list_t *deplist_head, const char *depends, unsigned int flags);
PKGCONF_API void pkgconf_dependency_parse(pkgconf_client_t *client, pkgconf_pkg_t *pkg, pkgconf_list_t *deplist_head, const char *depends, unsigned int flags);
PKGCONF_API void pkgconf_dependency_append(pkgconf_list_t *list, pkgconf_dependency_t *tail);
PKGCONF_API void pkgconf_dependency_free(pkgconf_list_t *list);
PKGCONF_API void pkgconf_dependency_free_one(pkgconf_dependency_t *dep);
PKGCONF_API pkgconf_dependency_t *pkgconf_dependency_add(pkgconf_client_t *client, pkgconf_list_t *list, const char *package, const char *version, pkgconf_pkg_comparator_t compare, unsigned int flags);
PKGCONF_API pkgconf_dependency_t *pkgconf_dependency_ref(pkgconf_client_t *client, pkgconf_dependency_t *dep);
PKGCONF_API void pkgconf_dependency_unref(pkgconf_client_t *client, pkgconf_dependency_t *dep);
PKGCONF_API pkgconf_dependency_t *pkgconf_dependency_copy(pkgconf_client_t *client, const pkgconf_dependency_t *dep);

/* argvsplit.c */
PKGCONF_API int pkgconf_argv_split(const char *src, int *argc, char ***argv);
PKGCONF_API void pkgconf_argv_free(char **argv);

/* fragment.c */
typedef struct pkgconf_fragment_render_ops_ {
	size_t (*render_len)(const pkgconf_list_t *list, bool escape);
	void (*render_buf)(const pkgconf_list_t *list, pkgconf_buffer_t *buf, bool escape, char delim);
} pkgconf_fragment_render_ops_t;

typedef bool (*pkgconf_fragment_filter_func_t)(const pkgconf_client_t *client, const pkgconf_fragment_t *frag, void *data);
PKGCONF_API bool pkgconf_fragment_parse(const pkgconf_client_t *client, pkgconf_list_t *list, pkgconf_list_t *vars, const char *value, unsigned int flags);
PKGCONF_API void pkgconf_fragment_insert(const pkgconf_client_t *client, pkgconf_list_t *list, char type, const char *data, bool tail);
PKGCONF_API void pkgconf_fragment_add(const pkgconf_client_t *client, pkgconf_list_t *list, const char *string, unsigned int flags);
PKGCONF_API void pkgconf_fragment_copy(const pkgconf_client_t *client, pkgconf_list_t *list, const pkgconf_fragment_t *base, bool is_private);
PKGCONF_API void pkgconf_fragment_copy_list(const pkgconf_client_t *client, pkgconf_list_t *list, const pkgconf_list_t *base);
PKGCONF_API void pkgconf_fragment_delete(pkgconf_list_t *list, pkgconf_fragment_t *node);
PKGCONF_API void pkgconf_fragment_free(pkgconf_list_t *list);
PKGCONF_API void pkgconf_fragment_filter(const pkgconf_client_t *client, pkgconf_list_t *dest, pkgconf_list_t *src, pkgconf_fragment_filter_func_t filter_func, void *data);
PKGCONF_API size_t pkgconf_fragment_render_len(const pkgconf_list_t *list, bool escape, const pkgconf_fragment_render_ops_t *ops);
PKGCONF_API void pkgconf_fragment_render_buf(const pkgconf_list_t *list, pkgconf_buffer_t *buf, bool escape, const pkgconf_fragment_render_ops_t *ops, char delim);
PKGCONF_API bool pkgconf_fragment_has_system_dir(const pkgconf_client_t *client, const pkgconf_fragment_t *frag);

/* tuple.c */
PKGCONF_API pkgconf_tuple_t *pkgconf_tuple_add(const pkgconf_client_t *client, pkgconf_list_t *parent, const char *key, const char *value, bool parse, unsigned int flags);
PKGCONF_API char *pkgconf_tuple_find(const pkgconf_client_t *client, pkgconf_list_t *list, const char *key);
PKGCONF_API char *pkgconf_tuple_parse(const pkgconf_client_t *client, pkgconf_list_t *list, const char *value, unsigned int flags);
PKGCONF_API void pkgconf_tuple_free(pkgconf_list_t *list);
PKGCONF_API void pkgconf_tuple_free_entry(pkgconf_tuple_t *tuple, pkgconf_list_t *list);
PKGCONF_API void pkgconf_tuple_add_global(pkgconf_client_t *client, const char *key, const char *value);
PKGCONF_API char *pkgconf_tuple_find_global(const pkgconf_client_t *client, const char *key);
PKGCONF_API void pkgconf_tuple_free_global(pkgconf_client_t *client);
PKGCONF_API void pkgconf_tuple_define_global(pkgconf_client_t *client, const char *kv);

/* queue.c */
PKGCONF_API void pkgconf_queue_push(pkgconf_list_t *list, const char *package);
PKGCONF_API void pkgconf_queue_push_dependency(pkgconf_list_t *list, const pkgconf_dependency_t *dep);
PKGCONF_API bool pkgconf_queue_compile(pkgconf_client_t *client, pkgconf_pkg_t *world, pkgconf_list_t *list);
PKGCONF_API bool pkgconf_queue_solve(pkgconf_client_t *client, pkgconf_list_t *list, pkgconf_pkg_t *world, int maxdepth);
PKGCONF_API void pkgconf_queue_free(pkgconf_list_t *list);
PKGCONF_API bool pkgconf_queue_apply(pkgconf_client_t *client, pkgconf_list_t *list, pkgconf_queue_apply_func_t func, int maxdepth, void *data);
PKGCONF_API bool pkgconf_queue_validate(pkgconf_client_t *client, pkgconf_list_t *list, int maxdepth);
PKGCONF_API void pkgconf_solution_free(pkgconf_client_t *client, pkgconf_pkg_t *world);

/* cache.c */
PKGCONF_API pkgconf_pkg_t *pkgconf_cache_lookup(pkgconf_client_t *client, const char *id);
PKGCONF_API void pkgconf_cache_add(pkgconf_client_t *client, pkgconf_pkg_t *pkg);
PKGCONF_API void pkgconf_cache_remove(pkgconf_client_t *client, pkgconf_pkg_t *pkg);
PKGCONF_API void pkgconf_cache_free(pkgconf_client_t *client);

/* audit.c */
PKGCONF_API void pkgconf_audit_set_log(pkgconf_client_t *client, FILE *auditf);
PKGCONF_API void pkgconf_audit_log(pkgconf_client_t *client, const char *format, ...) PRINTFLIKE(2, 3);
PKGCONF_API void pkgconf_audit_log_dependency(pkgconf_client_t *client, const pkgconf_pkg_t *dep, const pkgconf_dependency_t *depnode);

/* path.c */
PKGCONF_API void pkgconf_path_add(const char *text, pkgconf_list_t *dirlist, bool filter);
PKGCONF_API void pkgconf_path_prepend(const char *text, pkgconf_list_t *dirlist, bool filter);
PKGCONF_API size_t pkgconf_path_split(const char *text, pkgconf_list_t *dirlist, bool filter);
PKGCONF_API size_t pkgconf_path_build_from_environ(const pkgconf_client_t *client, const char *envvarname, const char *fallback, pkgconf_list_t *dirlist, bool filter);
#ifdef _WIN32
PKGCONF_API size_t pkgconf_path_build_from_registry(/* HKEY -> HANDLE -> PVOID */ void *hKey, pkgconf_list_t *dirlist, bool filter);
#endif
PKGCONF_API bool pkgconf_path_match_list(const char *path, const pkgconf_list_t *dirlist);
PKGCONF_API void pkgconf_path_free(pkgconf_list_t *dirlist);
PKGCONF_API bool pkgconf_path_relocate(char *buf, size_t buflen);
PKGCONF_API void pkgconf_path_copy_list(pkgconf_list_t *dst, const pkgconf_list_t *src);
PKGCONF_API void pkgconf_path_prepend_list(pkgconf_list_t *dst, const pkgconf_list_t *src);

/* buffer.c */
struct pkgconf_span_ {
	unsigned char lo;
	unsigned char hi;	/* inclusive */
};

static inline bool pkgconf_span_contains(unsigned char c, const pkgconf_span_t *spans, size_t nspans) {
	for (size_t i = 0; i < nspans; i++)
		if (c >= spans[i].lo && c <= spans[i].hi)
			return true;

	return false;
}

PKGCONF_API void pkgconf_buffer_append(pkgconf_buffer_t *buffer, const char *text);
PKGCONF_API void pkgconf_buffer_append_fmt(pkgconf_buffer_t *buffer, const char *fmt, ...) PRINTFLIKE(2, 3);
PKGCONF_API void pkgconf_buffer_append_vfmt(pkgconf_buffer_t *buffer, const char *fmt, va_list va);
PKGCONF_API void pkgconf_buffer_push_byte(pkgconf_buffer_t *buffer, char byte);
PKGCONF_API void pkgconf_buffer_trim_byte(pkgconf_buffer_t *buffer);
PKGCONF_API void pkgconf_buffer_finalize(pkgconf_buffer_t *buffer);
PKGCONF_API void pkgconf_buffer_fputs(pkgconf_buffer_t *buffer, FILE *out);
PKGCONF_API void pkgconf_buffer_vjoin(pkgconf_buffer_t *buffer, char delim, va_list va);
PKGCONF_API void pkgconf_buffer_join(pkgconf_buffer_t *buffer, int delim, ...);
PKGCONF_API bool pkgconf_buffer_contains(const pkgconf_buffer_t *haystack, const pkgconf_buffer_t *needle);
PKGCONF_API bool pkgconf_buffer_contains_byte(const pkgconf_buffer_t *haystack, char needle);
PKGCONF_API bool pkgconf_buffer_match(const pkgconf_buffer_t *haystack, const pkgconf_buffer_t *needle);
PKGCONF_API void pkgconf_buffer_subst(pkgconf_buffer_t *dest, const pkgconf_buffer_t *src, const char *pattern, const char *value);
PKGCONF_API void pkgconf_buffer_escape(pkgconf_buffer_t *dest, const pkgconf_buffer_t *src, const pkgconf_span_t *spans, size_t nspans);
static inline const char *pkgconf_buffer_str(const pkgconf_buffer_t *buffer) {
	return buffer->base;
}

static inline const char *pkgconf_buffer_str_or_empty(const pkgconf_buffer_t *buffer) {
	return buffer->base != NULL ? buffer->base : "";
}

static inline size_t pkgconf_buffer_len(const pkgconf_buffer_t *buffer) {
	return (size_t)(ptrdiff_t)(buffer->end - buffer->base);
}

static inline char pkgconf_buffer_lastc(const pkgconf_buffer_t *buffer) {
	if (buffer->base == buffer->end)
		return '\0';

	return *(buffer->end - 1);
}

#define PKGCONF_BUFFER_INITIALIZER { NULL, NULL }
#define PKGCONF_BUFFER_FROM_STR(str) &(const pkgconf_buffer_t){ .base = str, .end = str + strlen(str) }

static inline void pkgconf_buffer_reset(pkgconf_buffer_t *buffer) {
	pkgconf_buffer_finalize(buffer);
	buffer->base = buffer->end = NULL;
}

static inline char *pkgconf_buffer_freeze(pkgconf_buffer_t *buffer) {
	if (buffer->base == NULL)
		return NULL;

	char *out = strdup(pkgconf_buffer_str(buffer));
	pkgconf_buffer_reset(buffer);
	return out;
}

static inline void pkgconf_buffer_copy(pkgconf_buffer_t *buffer, pkgconf_buffer_t *newptr)
{
    pkgconf_buffer_reset(newptr);
    pkgconf_buffer_append(newptr, pkgconf_buffer_str(buffer));
}

/* fileio.c */
PKGCONF_API bool pkgconf_fgetline(pkgconf_buffer_t *buffer, FILE *stream);

/* parser.c */
typedef void (*pkgconf_parser_operand_func_t)(void *data, const char *warnprefix, const char *key, const char *value);
typedef void (*pkgconf_parser_warn_func_t)(void *data, const char *fmt, ...);

PKGCONF_API void pkgconf_parser_parse_buffer(void *data, const pkgconf_parser_operand_func_t *ops, const pkgconf_parser_warn_func_t warnfunc, pkgconf_buffer_t *buffer, const char *warnprefix);
PKGCONF_API void pkgconf_parser_parse(FILE *f, void *data, const pkgconf_parser_operand_func_t *ops, const pkgconf_parser_warn_func_t warnfunc, const char *filename);

/* output.c */
typedef enum {
	PKGCONF_OUTPUT_STDOUT,
	PKGCONF_OUTPUT_STDERR,
} pkgconf_output_stream_t;

struct pkgconf_output_ {
	void *privdata;

	bool (*write)(pkgconf_output_t *output, pkgconf_output_stream_t stream, const pkgconf_buffer_t *buffer);
};

PKGCONF_API bool pkgconf_output_putbuf(pkgconf_output_t *output, pkgconf_output_stream_t stream, const pkgconf_buffer_t *buffer, bool newline);
PKGCONF_API bool pkgconf_output_puts(pkgconf_output_t *output, pkgconf_output_stream_t stream, const char *str);
PKGCONF_API bool pkgconf_output_fmt(pkgconf_output_t *output, pkgconf_output_stream_t stream, const char *fmt, ...);
PKGCONF_API bool pkgconf_output_vfmt(pkgconf_output_t *output, pkgconf_output_stream_t stream, const char *fmt, va_list va);
PKGCONF_API pkgconf_output_t *pkgconf_output_default(void);

#ifdef __cplusplus
}
#endif

#endif
