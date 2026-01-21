/*
 * main.c
 * main() routine, printer functions
 *
 * Copyright (c) 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
 *     pkgconf authors (see AUTHORS).
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#include "libpkgconf/config.h"
#include <libpkgconf/stdinc.h>
#include <libpkgconf/libpkgconf.h>
#include "getopt_long.h"
#ifndef PKGCONF_LITE
#include "renderer-msvc.h"
#endif
#ifdef _WIN32
#include <io.h>     /* for _setmode() */
#include <fcntl.h>
#endif

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

static pkgconf_client_t pkg_client;
static const pkgconf_fragment_render_ops_t *want_render_ops = NULL;

static uint64_t want_flags;
static int verbosity = 0;
static int maximum_traverse_depth = 2000;
static size_t maximum_package_count = 0;

static char *want_variable = NULL;
static char *want_fragment_filter = NULL;

FILE *error_msgout = NULL;
FILE *logfile_out = NULL;

static bool
error_handler(const char *msg, const pkgconf_client_t *client, void *data)
{
	(void) client;
	(void) data;
	fprintf(error_msgout, "%s", msg);
	return true;
}

static bool
print_list_entry(const pkgconf_pkg_t *entry, void *data)
{
	(void) data;

	if (entry->flags & PKGCONF_PKG_PROPF_UNINSTALLED)
		return false;

	printf("%-30s %s - %s\n", entry->id, entry->realname, entry->description);

	return false;
}

static bool
print_package_entry(const pkgconf_pkg_t *entry, void *data)
{
	(void) data;

	if (entry->flags & PKGCONF_PKG_PROPF_UNINSTALLED)
		return false;

	printf("%s\n", entry->id);

	return false;
}

static bool
filter_cflags(const pkgconf_client_t *client, const pkgconf_fragment_t *frag, void *data)
{
	int got_flags = 0;
	(void) client;
	(void) data;

	if (!(want_flags & PKG_KEEP_SYSTEM_CFLAGS) && pkgconf_fragment_has_system_dir(client, frag))
		return false;

	if (want_fragment_filter != NULL && (strchr(want_fragment_filter, frag->type) == NULL || !frag->type))
		return false;

	if (frag->type == 'I')
		got_flags = PKG_CFLAGS_ONLY_I;
	else
		got_flags = PKG_CFLAGS_ONLY_OTHER;

	return (want_flags & got_flags) != 0;
}

static bool
filter_libs(const pkgconf_client_t *client, const pkgconf_fragment_t *frag, void *data)
{
	int got_flags = 0;
	(void) client;
	(void) data;

	if (!(want_flags & PKG_KEEP_SYSTEM_LIBS) && pkgconf_fragment_has_system_dir(client, frag))
		return false;

	if (want_fragment_filter != NULL && (strchr(want_fragment_filter, frag->type) == NULL || !frag->type))
		return false;

	switch (frag->type)
	{
		case 'L': got_flags = PKG_LIBS_ONLY_LDPATH; break;
		case 'l': got_flags = PKG_LIBS_ONLY_LIBNAME; break;
		default: got_flags = PKG_LIBS_ONLY_OTHER; break;
	}

	return (want_flags & got_flags) != 0;
}

static void
print_variables(pkgconf_pkg_t *pkg)
{
	pkgconf_node_t *node;

	PKGCONF_FOREACH_LIST_ENTRY(pkg->vars.head, node)
	{
		pkgconf_tuple_t *tuple = node->data;

		printf("%s\n", tuple->key);
	}
}

static void
print_requires(pkgconf_pkg_t *pkg)
{
	pkgconf_node_t *node;

	PKGCONF_FOREACH_LIST_ENTRY(pkg->required.head, node)
	{
		pkgconf_dependency_t *dep = node->data;

		printf("%s", dep->package);

		if (dep->version != NULL)
			printf(" %s %s", pkgconf_pkg_get_comparator(dep), dep->version);

		printf("\n");
	}
}

static void
print_requires_private(pkgconf_pkg_t *pkg)
{
	pkgconf_node_t *node;

	PKGCONF_FOREACH_LIST_ENTRY(pkg->requires_private.head, node)
	{
		pkgconf_dependency_t *dep = node->data;

		printf("%s", dep->package);

		if (dep->version != NULL)
			printf(" %s %s", pkgconf_pkg_get_comparator(dep), dep->version);

		printf("\n");
	}
}

static void
print_provides(pkgconf_pkg_t *pkg)
{
	pkgconf_node_t *node;

	PKGCONF_FOREACH_LIST_ENTRY(pkg->provides.head, node)
	{
		pkgconf_dependency_t *dep = node->data;

		printf("%s", dep->package);

		if (dep->version != NULL)
			printf(" %s %s", pkgconf_pkg_get_comparator(dep), dep->version);

		printf("\n");
	}
}

static bool
apply_provides(pkgconf_client_t *client, pkgconf_pkg_t *world, void *unused, int maxdepth)
{
	pkgconf_node_t *iter;
	(void) client;
	(void) unused;
	(void) maxdepth;

	PKGCONF_FOREACH_LIST_ENTRY(world->required.head, iter)
	{
		pkgconf_dependency_t *dep = iter->data;
		pkgconf_pkg_t *pkg = dep->match;

		print_provides(pkg);
	}

	return true;
}

#ifndef PKGCONF_LITE
static void
print_digraph_node(pkgconf_client_t *client, pkgconf_pkg_t *pkg, void *data)
{
	pkgconf_node_t *node;
	(void) client;
	pkgconf_pkg_t **last_seen = data;

	if(pkg->flags & PKGCONF_PKG_PROPF_VIRTUAL)
		return;

	if (pkg->flags & PKGCONF_PKG_PROPF_VISITED_PRIVATE)
		printf("\"%s\" [fontname=Sans fontsize=8 fontcolor=gray color=gray]\n", pkg->id);
	else
		printf("\"%s\" [fontname=Sans fontsize=8]\n", pkg->id);

	if (last_seen != NULL)
	{
		if (*last_seen != NULL)
			printf("\"%s\" -> \"%s\" [fontname=Sans fontsize=8 color=red]\n", (*last_seen)->id, pkg->id);

		*last_seen = pkg;
	}

	PKGCONF_FOREACH_LIST_ENTRY(pkg->required.head, node)
	{
		pkgconf_dependency_t *dep = node->data;
		const char *dep_id = (dep->match != NULL) ? dep->match->id : dep->package;

		if ((dep->flags & PKGCONF_PKG_DEPF_PRIVATE) == 0)
			printf("\"%s\" -> \"%s\" [fontname=Sans fontsize=8]\n", pkg->id, dep_id);
		else
			printf("\"%s\" -> \"%s\" [fontname=Sans fontsize=8 color=gray]\n", pkg->id, dep_id);
	}

	PKGCONF_FOREACH_LIST_ENTRY(pkg->requires_private.head, node)
	{
		pkgconf_dependency_t *dep = node->data;
		const char *dep_id = (dep->match != NULL) ? dep->match->id : dep->package;

		printf("\"%s\" -> \"%s\" [fontname=Sans fontsize=8 color=gray]\n", pkg->id, dep_id);
	}
}

static bool
apply_digraph(pkgconf_client_t *client, pkgconf_pkg_t *world, void *data, int maxdepth)
{
	int eflag;
	pkgconf_list_t *list = data;
	pkgconf_pkg_t *last_seen = NULL;
	pkgconf_node_t *iter;

	printf("digraph deptree {\n");
	printf("edge [color=blue len=7.5 fontname=Sans fontsize=8]\n");
	printf("node [fontname=Sans fontsize=8]\n");
	printf("\"user:request\" [fontname=Sans fontsize=8]\n");

	PKGCONF_FOREACH_LIST_ENTRY(list->head, iter)
	{
		pkgconf_queue_t *pkgq = iter->data;
		pkgconf_pkg_t *pkg = pkgconf_pkg_find(client, pkgq->package);
		printf("\"user:request\" -> \"%s\" [fontname=Sans fontsize=8]\n", pkg == NULL ? pkgq->package : pkg->id);
		if (pkg != NULL)
			pkgconf_pkg_unref(client, pkg);
	}

	eflag = pkgconf_pkg_traverse(client, world, print_digraph_node, &last_seen, maxdepth, 0);

	if (eflag != PKGCONF_PKG_ERRF_OK)
		return false;

	printf("}\n");
	return true;
}

static void
print_solution_node(pkgconf_client_t *client, pkgconf_pkg_t *pkg, void *unused)
{
	(void) client;
	(void) unused;

	printf("%s (%"PRIu64")%s\n", pkg->id, pkg->identifier, (pkg->flags & PKGCONF_PKG_PROPF_VISITED_PRIVATE) == PKGCONF_PKG_PROPF_VISITED_PRIVATE ? " [private]" : "");
}

static bool
apply_print_solution(pkgconf_client_t *client, pkgconf_pkg_t *world, void *unused, int maxdepth)
{
	int eflag;

	eflag = pkgconf_pkg_traverse(client, world, print_solution_node, unused, maxdepth, 0);

	return eflag == PKGCONF_PKG_ERRF_OK;
}
#endif

static bool
apply_modversion(pkgconf_client_t *client, pkgconf_pkg_t *world, void *data, int maxdepth)
{
	pkgconf_node_t *queue_iter;
	pkgconf_list_t *pkgq = data;
	(void) client;
	(void) maxdepth;

	PKGCONF_FOREACH_LIST_ENTRY(pkgq->head, queue_iter)
	{
		pkgconf_node_t *world_iter;
		pkgconf_queue_t *queue_node = queue_iter->data;

		PKGCONF_FOREACH_LIST_ENTRY(world->required.head, world_iter)
		{
			pkgconf_dependency_t *dep = world_iter->data;
			pkgconf_pkg_t *pkg = dep->match;

			const size_t name_len = strlen(pkg->why);
			if (name_len > strlen(queue_node->package) ||
			    strncmp(pkg->why, queue_node->package, name_len) ||
			    (queue_node->package[name_len] != 0 &&
			     !isspace((unsigned char)queue_node->package[name_len]) &&
			     !PKGCONF_IS_OPERATOR_CHAR(queue_node->package[name_len])))
				continue;

			if (pkg->version != NULL) {
				if (verbosity)
					printf("%s: ", pkg->id);

				printf("%s\n", pkg->version);
			}

			break;
		}
	}

	return true;
}

static bool
apply_variables(pkgconf_client_t *client, pkgconf_pkg_t *world, void *unused, int maxdepth)
{
	pkgconf_node_t *iter;
	(void) client;
	(void) unused;
	(void) maxdepth;

	PKGCONF_FOREACH_LIST_ENTRY(world->required.head, iter)
	{
		pkgconf_dependency_t *dep = iter->data;
		pkgconf_pkg_t *pkg = dep->match;

		print_variables(pkg);
	}

	return true;
}

static bool
apply_path(pkgconf_client_t *client, pkgconf_pkg_t *world, void *unused, int maxdepth)
{
	pkgconf_node_t *iter;
	(void) client;
	(void) unused;
	(void) maxdepth;

	PKGCONF_FOREACH_LIST_ENTRY(world->required.head, iter)
	{
		pkgconf_dependency_t *dep = iter->data;
		pkgconf_pkg_t *pkg = dep->match;

		/* a module entry with no filename is either virtual, static (builtin) or synthesized. */
		if (pkg->filename != NULL)
			printf("%s\n", pkg->filename);
	}

	return true;
}

static bool
apply_variable(pkgconf_client_t *client, pkgconf_pkg_t *world, void *variable, int maxdepth)
{
	pkgconf_node_t *iter;
	(void) maxdepth;

	PKGCONF_FOREACH_LIST_ENTRY(world->required.head, iter)
	{
		pkgconf_dependency_t *dep = iter->data;
		pkgconf_pkg_t *pkg = dep->match;
		const char *var;

		var = pkgconf_tuple_find(client, &pkg->vars, variable);

		if (var != NULL)
			printf("%s%s", iter->prev != NULL ? " " : "", var);
	}

	printf("\n");

	return true;
}

static bool
apply_env_var(const char *prefix, pkgconf_client_t *client, pkgconf_pkg_t *world, int maxdepth,
	unsigned int (*collect_fn)(pkgconf_client_t *client, pkgconf_pkg_t *world, pkgconf_list_t *list, int maxdepth),
	bool (*filter_fn)(const pkgconf_client_t *client, const pkgconf_fragment_t *frag, void *data),
	void (*postprocess_fn)(pkgconf_client_t *client, pkgconf_pkg_t *world, pkgconf_list_t *fragment_list))
{
	pkgconf_list_t unfiltered_list = PKGCONF_LIST_INITIALIZER;
	pkgconf_list_t filtered_list = PKGCONF_LIST_INITIALIZER;
	unsigned int eflag;
	char *render_buf;

	eflag = collect_fn(client, world, &unfiltered_list, maxdepth);
	if (eflag != PKGCONF_PKG_ERRF_OK)
		return false;

	pkgconf_fragment_filter(client, &filtered_list, &unfiltered_list, filter_fn, NULL);

	if (postprocess_fn != NULL)
		postprocess_fn(client, world, &filtered_list);

	if (filtered_list.head == NULL)
		goto out;

	render_buf = pkgconf_fragment_render(&filtered_list, true, want_render_ops);
	printf("%s='%s'\n", prefix, render_buf);
	free(render_buf);

out:
	pkgconf_fragment_free(&unfiltered_list);
	pkgconf_fragment_free(&filtered_list);

	return true;
}

static void
maybe_add_module_definitions(pkgconf_client_t *client, pkgconf_pkg_t *world, pkgconf_list_t *fragment_list)
{
	pkgconf_node_t *world_iter;

	if ((want_flags & PKG_EXISTS_CFLAGS) != PKG_EXISTS_CFLAGS)
		return;

	PKGCONF_FOREACH_LIST_ENTRY(world->required.head, world_iter)
	{
		pkgconf_dependency_t *dep = world_iter->data;
		char havebuf[PKGCONF_ITEM_SIZE];
		char *p;

		if ((dep->flags & PKGCONF_PKG_DEPF_QUERY) != PKGCONF_PKG_DEPF_QUERY)
			continue;

		if (dep->match == NULL)
			continue;

		snprintf(havebuf, sizeof havebuf, "HAVE_%s", dep->match->id);

		for (p = havebuf; *p; p++)
		{
			switch (*p)
			{
				case ' ':
				case '-':
					*p = '_';
					break;

				default:
					*p = toupper((unsigned char) *p);
			}
		}

		pkgconf_fragment_insert(client, fragment_list, 'D', havebuf, false);
	}
}

static void
apply_env_variables(pkgconf_client_t *client, pkgconf_pkg_t *world, const char *env_prefix)
{
	(void) client;
	pkgconf_node_t *world_iter;

	PKGCONF_FOREACH_LIST_ENTRY(world->required.head, world_iter)
	{
		pkgconf_dependency_t *dep = world_iter->data;
		pkgconf_pkg_t *pkg = dep->match;
		pkgconf_node_t *tuple_iter;

		if ((dep->flags & PKGCONF_PKG_DEPF_QUERY) != PKGCONF_PKG_DEPF_QUERY)
			continue;

		if (dep->match == NULL)
			continue;

		PKGCONF_FOREACH_LIST_ENTRY(pkg->vars.head, tuple_iter)
		{
			pkgconf_tuple_t *tuple = tuple_iter->data;
			char havebuf[PKGCONF_ITEM_SIZE];
			char *p;

			if (want_variable != NULL && strcmp(want_variable, tuple->key))
				continue;

			snprintf(havebuf, sizeof havebuf, "%s_%s", env_prefix, tuple->key);

			for (p = havebuf; *p; p++)
			{
				switch (*p)
				{
					case ' ':
					case '-':
						*p = '_';
						break;

					default:
						*p = toupper((unsigned char) *p);
				}
			}

			printf("%s='%s'\n", havebuf, tuple->value);
		}
	}
}

static bool
apply_env(pkgconf_client_t *client, pkgconf_pkg_t *world, void *env_prefix_p, int maxdepth)
{
	const char *want_env_prefix = env_prefix_p, *it;
	char workbuf[PKGCONF_ITEM_SIZE];

	for (it = want_env_prefix; *it != '\0'; it++)
		if (!isalpha((unsigned char)*it) &&
		    !isdigit((unsigned char)*it))
			return false;

	snprintf(workbuf, sizeof workbuf, "%s_CFLAGS", want_env_prefix);
	if (!apply_env_var(workbuf, client, world, maxdepth, pkgconf_pkg_cflags, filter_cflags, maybe_add_module_definitions))
		return false;

	snprintf(workbuf, sizeof workbuf, "%s_LIBS", want_env_prefix);
	if (!apply_env_var(workbuf, client, world, maxdepth, pkgconf_pkg_libs, filter_libs, NULL))
		return false;

	if ((want_flags & PKG_VARIABLES) == PKG_VARIABLES || want_variable != NULL)
		apply_env_variables(client, world, want_env_prefix);

	return true;
}

static bool
apply_cflags(pkgconf_client_t *client, pkgconf_pkg_t *world, void *unused, int maxdepth)
{
	pkgconf_list_t unfiltered_list = PKGCONF_LIST_INITIALIZER;
	pkgconf_list_t filtered_list = PKGCONF_LIST_INITIALIZER;
	int eflag;
	char *render_buf;
	(void) unused;

	eflag = pkgconf_pkg_cflags(client, world, &unfiltered_list, maxdepth);
	if (eflag != PKGCONF_PKG_ERRF_OK)
		return false;

	pkgconf_fragment_filter(client, &filtered_list, &unfiltered_list, filter_cflags, NULL);
	maybe_add_module_definitions(client, world, &filtered_list);

	if (filtered_list.head == NULL)
		goto out;

	render_buf = pkgconf_fragment_render(&filtered_list, true, want_render_ops);
	printf("%s", render_buf);
	free(render_buf);

out:
	pkgconf_fragment_free(&unfiltered_list);
	pkgconf_fragment_free(&filtered_list);

	return true;
}

static bool
apply_libs(pkgconf_client_t *client, pkgconf_pkg_t *world, void *unused, int maxdepth)
{
	pkgconf_list_t unfiltered_list = PKGCONF_LIST_INITIALIZER;
	pkgconf_list_t filtered_list = PKGCONF_LIST_INITIALIZER;
	int eflag;
	char *render_buf;
	(void) unused;

	eflag = pkgconf_pkg_libs(client, world, &unfiltered_list, maxdepth);
	if (eflag != PKGCONF_PKG_ERRF_OK)
		return false;

	pkgconf_fragment_filter(client, &filtered_list, &unfiltered_list, filter_libs, NULL);

	if (filtered_list.head == NULL)
		goto out;

	render_buf = pkgconf_fragment_render(&filtered_list, true, want_render_ops);
	printf("%s", render_buf);
	free(render_buf);

out:
	pkgconf_fragment_free(&unfiltered_list);
	pkgconf_fragment_free(&filtered_list);

	return true;
}

static bool
apply_requires(pkgconf_client_t *client, pkgconf_pkg_t *world, void *unused, int maxdepth)
{
	pkgconf_node_t *iter;
	(void) client;
	(void) unused;
	(void) maxdepth;

	PKGCONF_FOREACH_LIST_ENTRY(world->required.head, iter)
	{
		pkgconf_dependency_t *dep = iter->data;
		pkgconf_pkg_t *pkg = dep->match;

		print_requires(pkg);
	}

	return true;
}

static bool
apply_requires_private(pkgconf_client_t *client, pkgconf_pkg_t *world, void *unused, int maxdepth)
{
	pkgconf_node_t *iter;
	(void) client;
	(void) unused;
	(void) maxdepth;

	PKGCONF_FOREACH_LIST_ENTRY(world->required.head, iter)
	{
		pkgconf_dependency_t *dep = iter->data;
		pkgconf_pkg_t *pkg = dep->match;

		print_requires_private(pkg);
	}
	return true;
}

static void
check_uninstalled(pkgconf_client_t *client, pkgconf_pkg_t *pkg, void *data)
{
	int *retval = data;
	(void) client;

	if (pkg->flags & PKGCONF_PKG_PROPF_UNINSTALLED)
		*retval = EXIT_SUCCESS;
}

static bool
apply_uninstalled(pkgconf_client_t *client, pkgconf_pkg_t *world, void *data, int maxdepth)
{
	int eflag;

	eflag = pkgconf_pkg_traverse(client, world, check_uninstalled, data, maxdepth, 0);

	if (eflag != PKGCONF_PKG_ERRF_OK)
		return false;

	return true;
}

#ifndef PKGCONF_LITE
static void
print_graph_node(pkgconf_client_t *client, pkgconf_pkg_t *pkg, void *data)
{
	pkgconf_node_t *n;

	(void) client;
	(void) data;

	printf("node '%s' {\n", pkg->id);

	if (pkg->version != NULL)
		printf("    version = '%s';\n", pkg->version);

	PKGCONF_FOREACH_LIST_ENTRY(pkg->required.head, n)
	{
		pkgconf_dependency_t *dep = n->data;
		printf("    dependency '%s'", dep->package);
		if (dep->compare != PKGCONF_CMP_ANY)
		{
			printf(" {\n");
			printf("        comparator = '%s';\n", pkgconf_pkg_get_comparator(dep));
			printf("        version = '%s';\n", dep->version);
			printf("    };\n");
		}
		else
			printf(";\n");
	}

	printf("};\n");
}

static bool
apply_simulate(pkgconf_client_t *client, pkgconf_pkg_t *world, void *data, int maxdepth)
{
	int eflag;

	eflag = pkgconf_pkg_traverse(client, world, print_graph_node, data, maxdepth, 0);

	if (eflag != PKGCONF_PKG_ERRF_OK)
		return false;

	return true;
}
#endif

static void
print_fragment_tree_branch(pkgconf_list_t *fragment_list, int indent)
{
	pkgconf_node_t *iter;

	PKGCONF_FOREACH_LIST_ENTRY(fragment_list->head, iter)
	{
		pkgconf_fragment_t *frag = iter->data;

		if (frag->type)
			printf("%*s'-%c%s' [type %c]\n", indent, "", frag->type, frag->data, frag->type);
		else
			printf("%*s'%s' [untyped]\n", indent, "", frag->data);

		print_fragment_tree_branch(&frag->children, indent + 2);
	}

	if (fragment_list->head != NULL)
		printf("\n");
}

static bool
apply_fragment_tree(pkgconf_client_t *client, pkgconf_pkg_t *world, void *data, int maxdepth)
{
	pkgconf_list_t unfiltered_list = PKGCONF_LIST_INITIALIZER;
	int eflag;

	(void) data;

	eflag = pkgconf_pkg_cflags(client, world, &unfiltered_list, maxdepth);
	if (eflag != PKGCONF_PKG_ERRF_OK)
		return false;

	eflag = pkgconf_pkg_libs(client, world, &unfiltered_list, maxdepth);
	if (eflag != PKGCONF_PKG_ERRF_OK)
		return false;

	print_fragment_tree_branch(&unfiltered_list, 0);
	pkgconf_fragment_free(&unfiltered_list);

	return true;
}

static void
print_license(pkgconf_client_t *client, pkgconf_pkg_t *pkg, void *data)
{
	(void) client;
	(void) data;

	if (pkg->flags & PKGCONF_PKG_PROPF_VIRTUAL)
		return;

	/* NOASSERTION is the default when the license is unknown, per SPDX spec § 3.15 */
	printf("%s: %s\n", pkg->id, pkg->license != NULL ? pkg->license : "NOASSERTION");
}

static bool
apply_license(pkgconf_client_t *client, pkgconf_pkg_t *world, void *data, int maxdepth)
{
	int eflag;

	eflag = pkgconf_pkg_traverse(client, world, print_license, data, maxdepth, 0);

	if (eflag != PKGCONF_PKG_ERRF_OK)
		return false;

	return true;
}

static void
version(void)
{
	printf("%s\n", PACKAGE_VERSION);
}

static void
about(void)
{
	printf("%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
	printf("Copyright (c) 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019, 2020, 2021\n");
	printf("    pkgconf authors (see AUTHORS in documentation directory).\n\n");
	printf("Permission to use, copy, modify, and/or distribute this software for any\n");
	printf("purpose with or without fee is hereby granted, provided that the above\n");
	printf("copyright notice and this permission notice appear in all copies.\n\n");
	printf("This software is provided 'as is' and without any warranty, express or\n");
	printf("implied.  In no event shall the authors be liable for any damages arising\n");
	printf("from the use of this software.\n\n");
	printf("Report bugs at <%s>.\n", PACKAGE_BUGREPORT);
}

static void
usage(void)
{
	printf("usage: %s [OPTIONS] [LIBRARIES]\n", PACKAGE_NAME);

	printf("\nbasic options:\n\n");

	printf("  --help                            this message\n");
	printf("  --about                           print pkgconf version and license to stdout\n");
	printf("  --version                         print supported pkg-config version to stdout\n");
	printf("  --verbose                         print additional information\n");
	printf("  --atleast-pkgconfig-version       check whether or not pkgconf is compatible\n");
	printf("                                    with a specified pkg-config version\n");
	printf("  --errors-to-stdout                print all errors on stdout instead of stderr\n");
	printf("  --print-errors                    ensure all errors are printed\n");
	printf("  --short-errors                    be less verbose about some errors\n");
	printf("  --silence-errors                  explicitly be silent about errors\n");
	printf("  --list-all                        list all known packages\n");
	printf("  --list-package-names              list all known package names\n");
#ifndef PKGCONF_LITE
	printf("  --simulate                        simulate walking the calculated dependency graph\n");
#endif
	printf("  --no-cache                        do not cache already seen packages when\n");
	printf("                                    walking the dependency graph\n");
	printf("  --log-file=filename               write an audit log to a specified file\n");
	printf("  --with-path=path                  adds a directory to the search path\n");
	printf("  --define-prefix                   override the prefix variable with one that is guessed based on\n");
	printf("                                    the location of the .pc file\n");
	printf("  --dont-define-prefix              do not override the prefix variable under any circumstances\n");
	printf("  --prefix-variable=varname         sets the name of the variable that pkgconf considers\n");
	printf("                                    to be the package prefix\n");
	printf("  --relocate=path                   relocates a path and exits (mostly for testsuite)\n");
	printf("  --dont-relocate-paths             disables path relocation support\n");

#ifndef PKGCONF_LITE
	printf("\ncross-compilation personality support:\n\n");
	printf("  --personality=triplet|filename    sets the personality to 'triplet' or a file named 'filename'\n");
	printf("  --dump-personality                dumps details concerning selected personality\n");
#endif

	printf("\nchecking specific pkg-config database entries:\n\n");

	printf("  --atleast-version                 require a specific version of a module\n");
	printf("  --exact-version                   require an exact version of a module\n");
	printf("  --max-version                     require a maximum version of a module\n");
	printf("  --exists                          check whether or not a module exists\n");
	printf("  --uninstalled                     check whether or not an uninstalled module will be used\n");
	printf("  --no-uninstalled                  never use uninstalled modules when satisfying dependencies\n");
	printf("  --no-provides                     do not use 'provides' rules to resolve dependencies\n");
	printf("  --maximum-traverse-depth          maximum allowed depth for dependency graph\n");
	printf("  --static                          be more aggressive when computing dependency graph\n");
	printf("                                    (for static linking)\n");
	printf("  --shared                          use a simplified dependency graph (usually default)\n");
	printf("  --pure                            optimize a static dependency graph as if it were a normal\n");
	printf("                                    dependency graph\n");
	printf("  --env-only                        look only for package entries in PKG_CONFIG_PATH\n");
	printf("  --ignore-conflicts                ignore 'conflicts' rules in modules\n");
	printf("  --validate                        validate specific .pc files for correctness\n");

	printf("\nquerying specific pkg-config database fields:\n\n");

	printf("  --define-variable=varname=value   define variable 'varname' as 'value'\n");
	printf("  --variable=varname                print specified variable entry to stdout\n");
	printf("  --cflags                          print required CFLAGS to stdout\n");
	printf("  --cflags-only-I                   print required include-dir CFLAGS to stdout\n");
	printf("  --cflags-only-other               print required non-include-dir CFLAGS to stdout\n");
	printf("  --libs                            print required linker flags to stdout\n");
	printf("  --libs-only-L                     print required LDPATH linker flags to stdout\n");
	printf("  --libs-only-l                     print required LIBNAME linker flags to stdout\n");
	printf("  --libs-only-other                 print required other linker flags to stdout\n");
	printf("  --print-requires                  print required dependency frameworks to stdout\n");
	printf("  --print-requires-private          print required dependency frameworks for static\n");
	printf("                                    linking to stdout\n");
	printf("  --print-provides                  print provided dependencies to stdout\n");
	printf("  --print-variables                 print all known variables in module to stdout\n");
#ifndef PKGCONF_LITE
	printf("  --digraph                         print entire dependency graph in graphviz 'dot' format\n");
	printf("  --solution                        print dependency graph solution in a simple format\n");
#endif
	printf("  --keep-system-cflags              keep -I%s entries in cflags output\n", SYSTEM_INCLUDEDIR);
	printf("  --keep-system-libs                keep -L%s entries in libs output\n", SYSTEM_LIBDIR);
	printf("  --path                            show the exact filenames for any matching .pc files\n");
	printf("  --modversion                      print the specified module's version to stdout\n");
	printf("  --internal-cflags                 do not filter 'internal' cflags from output\n");
	printf("  --license                         print the specified module's license to stdout if known\n");
	printf("  --exists-cflags                   add -DHAVE_FOO fragments to cflags for each found module\n");

	printf("\nfiltering output:\n\n");
#ifndef PKGCONF_LITE
	printf("  --msvc-syntax                     print translatable fragments in MSVC syntax\n");
#endif
	printf("  --fragment-filter=types           filter output fragments to the specified types\n");
	printf("  --env=prefix                      print output as shell-compatible environmental variables\n");
	printf("  --fragment-tree                   visualize printed CFLAGS/LIBS fragments as a tree\n");

	printf("\nreport bugs to <%s>.\n", PACKAGE_BUGREPORT);
}

static void
relocate_path(const char *path)
{
	char buf[PKGCONF_BUFSIZE];

	pkgconf_strlcpy(buf, path, sizeof buf);
	pkgconf_path_relocate(buf, sizeof buf);

	printf("%s\n", buf);
}

#ifndef PKGCONF_LITE
static void
dump_personality(const pkgconf_cross_personality_t *p)
{
	pkgconf_node_t *n;

	printf("Triplet: %s\n", p->name);

	if (p->sysroot_dir)
		printf("SysrootDir: %s\n", p->sysroot_dir);

	printf("DefaultSearchPaths: ");
	PKGCONF_FOREACH_LIST_ENTRY(p->dir_list.head, n)
	{
		pkgconf_path_t *pn = n->data;
		printf("%s ", pn->path);
	}

	printf("\n");
	printf("SystemIncludePaths: ");
	PKGCONF_FOREACH_LIST_ENTRY(p->filter_includedirs.head, n)
	{
		pkgconf_path_t *pn = n->data;
		printf("%s ", pn->path);
	}

	printf("\n");
	printf("SystemLibraryPaths: ");
	PKGCONF_FOREACH_LIST_ENTRY(p->filter_libdirs.head, n)
	{
		pkgconf_path_t *pn = n->data;
		printf("%s ", pn->path);
	}

	printf("\n");
}

static pkgconf_cross_personality_t *
deduce_personality(char *argv[])
{
	const char *argv0 = argv[0];
	char *i, *prefix;
	pkgconf_cross_personality_t *out;

	i = strrchr(argv0, '/');
	if (i != NULL)
		argv0 = i + 1;

#if defined(_WIN32) || defined(_WIN64)
	i = strrchr(argv0, '\\');
	if (i != NULL)
		argv0 = i + 1;
#endif

	i = strstr(argv0, "-pkg");
	if (i == NULL)
		return pkgconf_cross_personality_default();

	prefix = pkgconf_strndup(argv0, i - argv0);
	out = pkgconf_cross_personality_find(prefix);
	free(prefix);
	if (out == NULL)
		return pkgconf_cross_personality_default();

	return out;
}
#endif

int
main(int argc, char *argv[])
{
	int ret;
	pkgconf_list_t pkgq = PKGCONF_LIST_INITIALIZER;
	pkgconf_list_t dir_list = PKGCONF_LIST_INITIALIZER;
	char *builddir;
	char *sysroot_dir;
	char *env_traverse_depth;
	char *required_pkgconfig_version = NULL;
	char *required_exact_module_version = NULL;
	char *required_max_module_version = NULL;
	char *required_module_version = NULL;
	char *logfile_arg = NULL;
	char *want_env_prefix = NULL;
	unsigned int want_client_flags = PKGCONF_PKG_PKGF_NONE;
	pkgconf_cross_personality_t *personality = NULL;
	bool opened_error_msgout = false;
	pkgconf_pkg_t world = {
		.id = "virtual:world",
		.realname = "virtual world package",
		.flags = PKGCONF_PKG_PROPF_STATIC | PKGCONF_PKG_PROPF_VIRTUAL,
	};

	want_flags = 0;

#ifdef _WIN32
	/* When running regression tests in cygwin, and building native
	 * executable, tests fail unless native executable outputs unix
	 * line endings.  Come to think of it, this will probably help
	 * real people who use cygwin build environments but native pkgconf, too.
	 */
	_setmode(fileno(stdout), O_BINARY);
	_setmode(fileno(stderr), O_BINARY);
#endif

	struct pkg_option options[] = {
		{ "version", no_argument, &want_flags, PKG_VERSION|PKG_PRINT_ERRORS, },
		{ "about", no_argument, &want_flags, PKG_ABOUT|PKG_PRINT_ERRORS, },
		{ "atleast-version", required_argument, NULL, 2, },
		{ "atleast-pkgconfig-version", required_argument, NULL, 3, },
		{ "libs", no_argument, &want_flags, PKG_LIBS|PKG_PRINT_ERRORS, },
		{ "cflags", no_argument, &want_flags, PKG_CFLAGS|PKG_PRINT_ERRORS, },
		{ "modversion", no_argument, &want_flags, PKG_MODVERSION|PKG_PRINT_ERRORS, },
		{ "variable", required_argument, NULL, 7, },
		{ "exists", no_argument, &want_flags, PKG_EXISTS, },
		{ "print-errors", no_argument, &want_flags, PKG_PRINT_ERRORS, },
		{ "short-errors", no_argument, &want_flags, PKG_SHORT_ERRORS, },
		{ "maximum-traverse-depth", required_argument, NULL, 11, },
		{ "static", no_argument, &want_flags, PKG_STATIC, },
		{ "shared", no_argument, &want_flags, PKG_SHARED, },
		{ "pure", no_argument, &want_flags, PKG_PURE, },
		{ "print-requires", no_argument, &want_flags, PKG_REQUIRES, },
		{ "print-variables", no_argument, &want_flags, PKG_VARIABLES|PKG_PRINT_ERRORS, },
#ifndef PKGCONF_LITE
		{ "digraph", no_argument, &want_flags, PKG_DIGRAPH, },
		{ "solution", no_argument, &want_flags, PKG_SOLUTION, },
#endif
		{ "help", no_argument, &want_flags, PKG_HELP, },
		{ "env-only", no_argument, &want_flags, PKG_ENV_ONLY, },
		{ "print-requires-private", no_argument, &want_flags, PKG_REQUIRES_PRIVATE, },
		{ "cflags-only-I", no_argument, &want_flags, PKG_CFLAGS_ONLY_I|PKG_PRINT_ERRORS, },
		{ "cflags-only-other", no_argument, &want_flags, PKG_CFLAGS_ONLY_OTHER|PKG_PRINT_ERRORS, },
		{ "libs-only-L", no_argument, &want_flags, PKG_LIBS_ONLY_LDPATH|PKG_PRINT_ERRORS, },
		{ "libs-only-l", no_argument, &want_flags, PKG_LIBS_ONLY_LIBNAME|PKG_PRINT_ERRORS, },
		{ "libs-only-other", no_argument, &want_flags, PKG_LIBS_ONLY_OTHER|PKG_PRINT_ERRORS, },
		{ "uninstalled", no_argument, &want_flags, PKG_UNINSTALLED, },
		{ "no-uninstalled", no_argument, &want_flags, PKG_NO_UNINSTALLED, },
		{ "keep-system-cflags", no_argument, &want_flags, PKG_KEEP_SYSTEM_CFLAGS, },
		{ "keep-system-libs", no_argument, &want_flags, PKG_KEEP_SYSTEM_LIBS, },
		{ "define-variable", required_argument, NULL, 27, },
		{ "exact-version", required_argument, NULL, 28, },
		{ "max-version", required_argument, NULL, 29, },
		{ "ignore-conflicts", no_argument, &want_flags, PKG_IGNORE_CONFLICTS, },
		{ "errors-to-stdout", no_argument, &want_flags, PKG_ERRORS_ON_STDOUT, },
		{ "silence-errors", no_argument, &want_flags, PKG_SILENCE_ERRORS, },
		{ "list-all", no_argument, &want_flags, PKG_LIST|PKG_PRINT_ERRORS, },
		{ "list-package-names", no_argument, &want_flags, PKG_LIST_PACKAGE_NAMES|PKG_PRINT_ERRORS, },
#ifndef PKGCONF_LITE
		{ "simulate", no_argument, &want_flags, PKG_SIMULATE, },
#endif
		{ "no-cache", no_argument, &want_flags, PKG_NO_CACHE, },
		{ "print-provides", no_argument, &want_flags, PKG_PROVIDES, },
		{ "no-provides", no_argument, &want_flags, PKG_NO_PROVIDES, },
		{ "debug", no_argument, &want_flags, PKG_DEBUG|PKG_PRINT_ERRORS, },
		{ "validate", no_argument, &want_flags, PKG_VALIDATE|PKG_PRINT_ERRORS|PKG_ERRORS_ON_STDOUT },
		{ "log-file", required_argument, NULL, 40 },
		{ "path", no_argument, &want_flags, PKG_PATH },
		{ "with-path", required_argument, NULL, 42 },
		{ "prefix-variable", required_argument, NULL, 43 },
		{ "define-prefix", no_argument, &want_flags, PKG_DEFINE_PREFIX },
		{ "relocate", required_argument, NULL, 45 },
		{ "dont-define-prefix", no_argument, &want_flags, PKG_DONT_DEFINE_PREFIX },
		{ "dont-relocate-paths", no_argument, &want_flags, PKG_DONT_RELOCATE_PATHS },
		{ "env", required_argument, NULL, 48 },
#ifndef PKGCONF_LITE
		{ "msvc-syntax", no_argument, &want_flags, PKG_MSVC_SYNTAX },
#endif
		{ "fragment-filter", required_argument, NULL, 50 },
		{ "internal-cflags", no_argument, &want_flags, PKG_INTERNAL_CFLAGS },
#ifndef PKGCONF_LITE
		{ "dump-personality", no_argument, &want_flags, PKG_DUMP_PERSONALITY },
		{ "personality", required_argument, NULL, 53 },
#endif
		{ "license", no_argument, &want_flags, PKG_DUMP_LICENSE },
		{ "verbose", no_argument, NULL, 55 },
		{ "exists-cflags", no_argument, &want_flags, PKG_EXISTS_CFLAGS },
		{ "fragment-tree", no_argument, &want_flags, PKG_FRAGMENT_TREE },
		{ NULL, 0, NULL, 0 }
	};

#ifndef PKGCONF_LITE
	if (getenv("PKG_CONFIG_EARLY_TRACE"))
	{
		error_msgout = stderr;
		pkgconf_client_set_trace_handler(&pkg_client, error_handler, NULL);
	}
#endif

	while ((ret = pkg_getopt_long_only(argc, argv, "", options, NULL)) != -1)
	{
		switch (ret)
		{
		case 2:
			required_module_version = pkg_optarg;
			break;
		case 3:
			required_pkgconfig_version = pkg_optarg;
			break;
		case 7:
			want_variable = pkg_optarg;
			break;
		case 11:
			maximum_traverse_depth = atoi(pkg_optarg);
			break;
		case 27:
			pkgconf_tuple_define_global(&pkg_client, pkg_optarg);
			break;
		case 28:
			required_exact_module_version = pkg_optarg;
			break;
		case 29:
			required_max_module_version = pkg_optarg;
			break;
		case 40:
			logfile_arg = pkg_optarg;
			break;
		case 42:
			pkgconf_path_prepend(pkg_optarg, &dir_list, true);
			break;
		case 43:
			pkgconf_client_set_prefix_varname(&pkg_client, pkg_optarg);
			break;
		case 45:
			relocate_path(pkg_optarg);
			return EXIT_SUCCESS;
		case 48:
			want_env_prefix = pkg_optarg;
			break;
		case 50:
			want_fragment_filter = pkg_optarg;
			break;
#ifndef PKGCONF_LITE
		case 53:
			personality = pkgconf_cross_personality_find(pkg_optarg);
			break;
#endif
		case 55:
			verbosity++;
			break;
		case '?':
		case ':':
			ret = EXIT_FAILURE;
			goto out;
		default:
			break;
		}
	}

	if (personality == NULL) {
#ifndef PKGCONF_LITE
		personality = deduce_personality(argv);
#else
		personality = pkgconf_cross_personality_default();
#endif
	}

	pkgconf_path_copy_list(&personality->dir_list, &dir_list);
	pkgconf_path_free(&dir_list);

#ifndef PKGCONF_LITE
	if ((want_flags & PKG_DUMP_PERSONALITY) == PKG_DUMP_PERSONALITY)
	{
		dump_personality(personality);
		return EXIT_SUCCESS;
	}
#endif

	/* now, bring up the client.  settings are preserved since the client is prealloced */
	pkgconf_client_init(&pkg_client, error_handler, NULL, personality);

#ifndef PKGCONF_LITE
	if ((want_flags & PKG_MSVC_SYNTAX) == PKG_MSVC_SYNTAX || getenv("PKG_CONFIG_MSVC_SYNTAX") != NULL)
		want_render_ops = msvc_renderer_get();
#endif

	if ((env_traverse_depth = getenv("PKG_CONFIG_MAXIMUM_TRAVERSE_DEPTH")) != NULL)
		maximum_traverse_depth = atoi(env_traverse_depth);

	if ((want_flags & PKG_PRINT_ERRORS) != PKG_PRINT_ERRORS)
		want_flags |= (PKG_SILENCE_ERRORS);

	if ((want_flags & PKG_SILENCE_ERRORS) == PKG_SILENCE_ERRORS && !getenv("PKG_CONFIG_DEBUG_SPEW"))
		want_flags |= (PKG_SILENCE_ERRORS);
	else
		want_flags &= ~(PKG_SILENCE_ERRORS);

	if (getenv("PKG_CONFIG_DONT_RELOCATE_PATHS"))
		want_flags |= (PKG_DONT_RELOCATE_PATHS);

	if ((want_flags & PKG_VALIDATE) == PKG_VALIDATE || (want_flags & PKG_DEBUG) == PKG_DEBUG)
		pkgconf_client_set_warn_handler(&pkg_client, error_handler, NULL);

#ifndef PKGCONF_LITE
	if ((want_flags & PKG_DEBUG) == PKG_DEBUG)
		pkgconf_client_set_trace_handler(&pkg_client, error_handler, NULL);
#endif

	if ((want_flags & PKG_ABOUT) == PKG_ABOUT)
	{
		about();

		ret = EXIT_SUCCESS;
		goto out;
	}

	if ((want_flags & PKG_VERSION) == PKG_VERSION)
	{
		version();

		ret = EXIT_SUCCESS;
		goto out;
	}

	if ((want_flags & PKG_HELP) == PKG_HELP)
	{
		usage();

		ret = EXIT_SUCCESS;
		goto out;
	}

	if (getenv("PKG_CONFIG_FDO_SYSROOT_RULES"))
		want_client_flags |= PKGCONF_PKG_PKGF_FDO_SYSROOT_RULES;

	if (getenv("PKG_CONFIG_PKGCONF1_SYSROOT_RULES"))
		want_client_flags |= PKGCONF_PKG_PKGF_PKGCONF1_SYSROOT_RULES;

	if ((want_flags & PKG_SHORT_ERRORS) == PKG_SHORT_ERRORS)
		want_client_flags |= PKGCONF_PKG_PKGF_SIMPLIFY_ERRORS;

	if ((want_flags & PKG_DONT_RELOCATE_PATHS) == PKG_DONT_RELOCATE_PATHS)
		want_client_flags |= PKGCONF_PKG_PKGF_DONT_RELOCATE_PATHS;

	error_msgout = stderr;
	if ((want_flags & PKG_ERRORS_ON_STDOUT) == PKG_ERRORS_ON_STDOUT)
		error_msgout = stdout;
	if ((want_flags & PKG_SILENCE_ERRORS) == PKG_SILENCE_ERRORS) {
		error_msgout = fopen(PATH_DEV_NULL, "w");
		opened_error_msgout = true;
	}

	if ((want_flags & PKG_IGNORE_CONFLICTS) == PKG_IGNORE_CONFLICTS || getenv("PKG_CONFIG_IGNORE_CONFLICTS") != NULL)
		want_client_flags |= PKGCONF_PKG_PKGF_SKIP_CONFLICTS;

	if ((want_flags & PKG_STATIC) == PKG_STATIC || personality->want_default_static)
		want_client_flags |= (PKGCONF_PKG_PKGF_SEARCH_PRIVATE | PKGCONF_PKG_PKGF_MERGE_PRIVATE_FRAGMENTS);

	if ((want_flags & PKG_SHARED) == PKG_SHARED)
		want_client_flags &= ~(PKGCONF_PKG_PKGF_SEARCH_PRIVATE | PKGCONF_PKG_PKGF_MERGE_PRIVATE_FRAGMENTS);

	/* if --static and --pure are both specified, then disable merge-back.
	 * this allows for a --static which searches private modules, but has the same fragment behaviour as if
	 * --static were disabled.  see <https://github.com/pkgconf/pkgconf/issues/83> for rationale.
	 */
	if ((want_flags & PKG_PURE) == PKG_PURE || getenv("PKG_CONFIG_PURE_DEPGRAPH") != NULL || personality->want_default_pure)
		want_client_flags &= ~PKGCONF_PKG_PKGF_MERGE_PRIVATE_FRAGMENTS;

	if ((want_flags & PKG_ENV_ONLY) == PKG_ENV_ONLY)
		want_client_flags |= PKGCONF_PKG_PKGF_ENV_ONLY;

	if ((want_flags & PKG_NO_CACHE) == PKG_NO_CACHE)
		want_client_flags |= PKGCONF_PKG_PKGF_NO_CACHE;

/* On Windows we want to always redefine the prefix by default
 * but allow that behavior to be manually disabled */
#if !defined(_WIN32) && !defined(_WIN64)
	if ((want_flags & PKG_DEFINE_PREFIX) == PKG_DEFINE_PREFIX || getenv("PKG_CONFIG_RELOCATE_PATHS") != NULL)
#endif
		want_client_flags |= PKGCONF_PKG_PKGF_REDEFINE_PREFIX;

	if ((want_flags & PKG_NO_UNINSTALLED) == PKG_NO_UNINSTALLED || getenv("PKG_CONFIG_DISABLE_UNINSTALLED") != NULL)
		want_client_flags |= PKGCONF_PKG_PKGF_NO_UNINSTALLED;

	if ((want_flags & PKG_NO_PROVIDES) == PKG_NO_PROVIDES)
		want_client_flags |= PKGCONF_PKG_PKGF_SKIP_PROVIDES;

	if ((want_flags & PKG_DONT_DEFINE_PREFIX) == PKG_DONT_DEFINE_PREFIX  || getenv("PKG_CONFIG_DONT_DEFINE_PREFIX") != NULL)
		want_client_flags &= ~PKGCONF_PKG_PKGF_REDEFINE_PREFIX;

	if ((want_flags & PKG_INTERNAL_CFLAGS) == PKG_INTERNAL_CFLAGS)
		want_client_flags |= PKGCONF_PKG_PKGF_DONT_FILTER_INTERNAL_CFLAGS;

	/* if these selectors are used, it means that we are querying metadata.
	 * so signal to libpkgconf that we only want to walk the flattened dependency set.
	 */
	if ((want_flags & PKG_MODVERSION) == PKG_MODVERSION ||
	    (want_flags & PKG_REQUIRES) == PKG_REQUIRES ||
	    (want_flags & PKG_REQUIRES_PRIVATE) == PKG_REQUIRES_PRIVATE ||
	    (want_flags & PKG_PROVIDES) == PKG_PROVIDES ||
	    (want_flags & PKG_VARIABLES) == PKG_VARIABLES ||
	    (want_flags & PKG_PATH) == PKG_PATH ||
	    want_variable != NULL)
		maximum_traverse_depth = 1;

	/* if we are asking for a variable, path or list of variables, this only makes sense
	 * for a single package.
	 */
	if ((want_flags & PKG_VARIABLES) == PKG_VARIABLES ||
	    (want_flags & PKG_PATH) == PKG_PATH ||
	    want_variable != NULL)
		maximum_package_count = 1;

	if (getenv("PKG_CONFIG_ALLOW_SYSTEM_CFLAGS") != NULL)
		want_flags |= PKG_KEEP_SYSTEM_CFLAGS;

	if (getenv("PKG_CONFIG_ALLOW_SYSTEM_LIBS") != NULL)
		want_flags |= PKG_KEEP_SYSTEM_LIBS;

	if ((builddir = getenv("PKG_CONFIG_TOP_BUILD_DIR")) != NULL)
		pkgconf_client_set_buildroot_dir(&pkg_client, builddir);

	if ((want_flags & PKG_REQUIRES_PRIVATE) == PKG_REQUIRES_PRIVATE ||
		(want_flags & PKG_CFLAGS))
	{
		want_client_flags |= PKGCONF_PKG_PKGF_SEARCH_PRIVATE;
	}

	if ((sysroot_dir = getenv("PKG_CONFIG_SYSROOT_DIR")) != NULL)
	{
		const char *destdir;

		pkgconf_client_set_sysroot_dir(&pkg_client, sysroot_dir);

		if ((destdir = getenv("DESTDIR")) != NULL)
		{
			if (!strcmp(destdir, sysroot_dir))
				want_client_flags |= PKGCONF_PKG_PKGF_FDO_SYSROOT_RULES;
		}
	}

	/* we have determined what features we want most likely.  in some cases, we override later. */
	pkgconf_client_set_flags(&pkg_client, want_client_flags);

	/* at this point, want_client_flags should be set, so build the dir list */
	pkgconf_client_dir_list_build(&pkg_client, personality);

	if (required_pkgconfig_version != NULL)
	{
		if (pkgconf_compare_version(PACKAGE_VERSION, required_pkgconfig_version) >= 0)
			ret = EXIT_SUCCESS;
		else
			ret = EXIT_FAILURE;

		goto out;
	}

	if ((want_flags & PKG_LIST) == PKG_LIST)
	{
		pkgconf_scan_all(&pkg_client, NULL, print_list_entry);
		ret = EXIT_SUCCESS;
		goto out;
	}

	if ((want_flags & PKG_LIST_PACKAGE_NAMES) == PKG_LIST_PACKAGE_NAMES)
	{
		pkgconf_scan_all(&pkg_client, NULL, print_package_entry);
		ret = EXIT_SUCCESS;
		goto out;
	}

	if (logfile_arg == NULL)
		logfile_arg = getenv("PKG_CONFIG_LOG");

	if (logfile_arg != NULL)
	{
		logfile_out = fopen(logfile_arg, "w");
		pkgconf_audit_set_log(&pkg_client, logfile_out);
	}

	if (required_module_version != NULL)
	{
		pkgconf_pkg_t *pkg = NULL;
		pkgconf_node_t *node;
		pkgconf_list_t deplist = PKGCONF_LIST_INITIALIZER;

		while (argv[pkg_optind])
		{
			pkgconf_dependency_parse_str(&pkg_client, &deplist, argv[pkg_optind], 0);
			pkg_optind++;
		}

		PKGCONF_FOREACH_LIST_ENTRY(deplist.head, node)
		{
			pkgconf_dependency_t *pkgiter = node->data;

			pkg = pkgconf_pkg_find(&pkg_client, pkgiter->package);
			if (pkg == NULL)
			{
				if (want_flags & PKG_PRINT_ERRORS)
					pkgconf_error(&pkg_client, "Package '%s' was not found\n", pkgiter->package);

				ret = EXIT_FAILURE;
				goto cleanup;
			}

			if (pkgconf_compare_version(pkg->version, required_module_version) >= 0)
			{
				ret = EXIT_SUCCESS;
				goto cleanup;
			}
		}

		ret = EXIT_FAILURE;
cleanup:
		if (pkg != NULL)
			pkgconf_pkg_unref(&pkg_client, pkg);
		pkgconf_dependency_free(&deplist);
		goto out;
	}
	else if (required_exact_module_version != NULL)
	{
		pkgconf_pkg_t *pkg = NULL;
		pkgconf_node_t *node;
		pkgconf_list_t deplist = PKGCONF_LIST_INITIALIZER;

		while (argv[pkg_optind])
		{
			pkgconf_dependency_parse_str(&pkg_client, &deplist, argv[pkg_optind], 0);
			pkg_optind++;
		}

		PKGCONF_FOREACH_LIST_ENTRY(deplist.head, node)
		{
			pkgconf_dependency_t *pkgiter = node->data;

			pkg = pkgconf_pkg_find(&pkg_client, pkgiter->package);
			if (pkg == NULL)
			{
				if (want_flags & PKG_PRINT_ERRORS)
					pkgconf_error(&pkg_client, "Package '%s' was not found\n", pkgiter->package);

				ret = EXIT_FAILURE;
				goto cleanup2;
			}

			if (pkgconf_compare_version(pkg->version, required_exact_module_version) == 0)
			{
				ret = EXIT_SUCCESS;
				goto cleanup2;
			}
		}

		ret = EXIT_FAILURE;
cleanup2:
		if (pkg != NULL)
			pkgconf_pkg_unref(&pkg_client, pkg);
		pkgconf_dependency_free(&deplist);
		goto out;
	}
	else if (required_max_module_version != NULL)
	{
		pkgconf_pkg_t *pkg = NULL;
		pkgconf_node_t *node;
		pkgconf_list_t deplist = PKGCONF_LIST_INITIALIZER;

		while (argv[pkg_optind])
		{
			pkgconf_dependency_parse_str(&pkg_client, &deplist, argv[pkg_optind], 0);
			pkg_optind++;
		}

		PKGCONF_FOREACH_LIST_ENTRY(deplist.head, node)
		{
			pkgconf_dependency_t *pkgiter = node->data;

			pkg = pkgconf_pkg_find(&pkg_client, pkgiter->package);
			if (pkg == NULL)
			{
				if (want_flags & PKG_PRINT_ERRORS)
					pkgconf_error(&pkg_client, "Package '%s' was not found\n", pkgiter->package);

				ret = EXIT_FAILURE;
				goto cleanup3;
			}

			if (pkgconf_compare_version(pkg->version, required_max_module_version) <= 0)
			{
				ret = EXIT_SUCCESS;
				goto cleanup3;
			}
		}

		ret = EXIT_FAILURE;
cleanup3:
		if (pkg != NULL)
			pkgconf_pkg_unref(&pkg_client, pkg);
		pkgconf_dependency_free(&deplist);
		goto out;
	}

	while (1)
	{
		char *package = argv[pkg_optind];
		char *end;

		if (package == NULL)
			break;

		/* check if there is a limit to the number of packages allowed to be included, if so and we have hit
		 * the limit, stop adding packages to the queue.
		 */
		if (maximum_package_count > 0 && pkgq.length >= maximum_package_count)
			break;

		while (isspace((unsigned char)package[0]))
			package++;

		/* skip empty packages */
		if (package[0] == '\0') {
			pkg_optind++;
			continue;
		}

		end = package + strlen(package) - 1;
		while(end > package && isspace((unsigned char)end[0])) end--;
		end[1] = '\0';

		if (argv[pkg_optind + 1] == NULL || !PKGCONF_IS_OPERATOR_CHAR(*(argv[pkg_optind + 1])))
		{
			pkgconf_queue_push(&pkgq, package);
			pkg_optind++;
		}
		else if (argv[pkg_optind + 2] == NULL)
		{
			char packagebuf[PKGCONF_BUFSIZE];

			snprintf(packagebuf, sizeof packagebuf, "%s %s", package, argv[pkg_optind + 1]);
			pkg_optind += 2;

			pkgconf_queue_push(&pkgq, packagebuf);
		}
		else
		{
			char packagebuf[PKGCONF_BUFSIZE];

			snprintf(packagebuf, sizeof packagebuf, "%s %s %s", package, argv[pkg_optind + 1], argv[pkg_optind + 2]);
			pkg_optind += 3;

			pkgconf_queue_push(&pkgq, packagebuf);
		}
	}

	if (pkgq.head == NULL)
	{
		fprintf(stderr, "Please specify at least one package name on the command line.\n");
		ret = EXIT_FAILURE;
		goto out;
	}

	ret = EXIT_SUCCESS;

	if (!pkgconf_queue_solve(&pkg_client, &pkgq, &world, maximum_traverse_depth))
	{
		ret = EXIT_FAILURE;
		goto out;
	}

#ifndef PKGCONF_LITE
	if ((want_flags & PKG_SIMULATE) == PKG_SIMULATE)
	{
		want_flags &= ~(PKG_CFLAGS|PKG_LIBS);

		pkgconf_client_set_flags(&pkg_client, want_client_flags | PKGCONF_PKG_PKGF_SKIP_ERRORS);
		apply_simulate(&pkg_client, &world, NULL, -1);
	}
#endif

	if ((want_flags & PKG_VALIDATE) == PKG_VALIDATE)
		goto out;

	if ((want_flags & PKG_DUMP_LICENSE) == PKG_DUMP_LICENSE)
	{
		apply_license(&pkg_client, &world, &ret, 2);
		goto out;
	}

	if ((want_flags & PKG_UNINSTALLED) == PKG_UNINSTALLED)
	{
		ret = EXIT_FAILURE;
		apply_uninstalled(&pkg_client, &world, &ret, 2);
		goto out;
	}

	if (want_env_prefix != NULL)
	{
		apply_env(&pkg_client, &world, want_env_prefix, 2);
		goto out;
	}

	if ((want_flags & PKG_PROVIDES) == PKG_PROVIDES)
	{
		want_flags &= ~(PKG_CFLAGS|PKG_LIBS);
		apply_provides(&pkg_client, &world, NULL, 2);
	}

#ifndef PKGCONF_LITE
	if ((want_flags & PKG_DIGRAPH) == PKG_DIGRAPH)
	{
		want_flags &= ~(PKG_CFLAGS|PKG_LIBS);
		apply_digraph(&pkg_client, &world, &pkgq, 2);
	}

	if ((want_flags & PKG_SOLUTION) == PKG_SOLUTION)
	{
		want_flags &= ~(PKG_CFLAGS|PKG_LIBS);
		apply_print_solution(&pkg_client, &world, NULL, 2);
	}
#endif

	if ((want_flags & PKG_MODVERSION) == PKG_MODVERSION)
	{
		want_flags &= ~(PKG_CFLAGS|PKG_LIBS);
		apply_modversion(&pkg_client, &world, &pkgq, 2);
	}

	if ((want_flags & PKG_PATH) == PKG_PATH)
	{
		want_flags &= ~(PKG_CFLAGS|PKG_LIBS);

		pkgconf_client_set_flags(&pkg_client, want_client_flags | PKGCONF_PKG_PKGF_SKIP_ROOT_VIRTUAL);
		apply_path(&pkg_client, &world, NULL, 2);
	}

	if ((want_flags & PKG_VARIABLES) == PKG_VARIABLES)
	{
		want_flags &= ~(PKG_CFLAGS|PKG_LIBS);
		apply_variables(&pkg_client, &world, NULL, 2);
	}

	if (want_variable)
	{
		want_flags &= ~(PKG_CFLAGS|PKG_LIBS);

		pkgconf_client_set_flags(&pkg_client, want_client_flags | PKGCONF_PKG_PKGF_SKIP_ROOT_VIRTUAL);
		apply_variable(&pkg_client, &world, want_variable, 2);
	}

	if ((want_flags & PKG_REQUIRES) == PKG_REQUIRES)
	{
		want_flags &= ~(PKG_CFLAGS|PKG_LIBS);
		apply_requires(&pkg_client, &world, NULL, 2);
	}

	if ((want_flags & PKG_REQUIRES_PRIVATE) == PKG_REQUIRES_PRIVATE)
	{
		want_flags &= ~(PKG_CFLAGS|PKG_LIBS);

		apply_requires_private(&pkg_client, &world, NULL, 2);
	}

	if ((want_flags & PKG_FRAGMENT_TREE))
	{
		want_flags &= ~(PKG_CFLAGS|PKG_LIBS);

		apply_fragment_tree(&pkg_client, &world, NULL, 2);
	}

	if ((want_flags & PKG_CFLAGS))
	{
		apply_cflags(&pkg_client, &world, NULL, 2);
	}

	if ((want_flags & PKG_LIBS))
	{
		if (want_flags & PKG_CFLAGS)
			printf(" ");

		if (!(want_flags & PKG_STATIC))
			pkgconf_client_set_flags(&pkg_client, pkg_client.flags & ~PKGCONF_PKG_PKGF_SEARCH_PRIVATE);

		apply_libs(&pkg_client, &world, NULL, 2);
	}

	if (want_flags & (PKG_CFLAGS|PKG_LIBS))
		printf("\n");

out:
	pkgconf_solution_free(&pkg_client, &world);
	pkgconf_queue_free(&pkgq);
	pkgconf_cross_personality_deinit(personality);
	pkgconf_client_deinit(&pkg_client);

	if (logfile_out != NULL)
		fclose(logfile_out);
	if (opened_error_msgout)
		fclose(error_msgout);

	return ret;
}
