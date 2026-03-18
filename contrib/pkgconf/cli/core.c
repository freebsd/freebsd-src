/*
 * core.c
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

#include "libpkgconf/config.h"
#include <libpkgconf/stdinc.h>
#include <libpkgconf/libpkgconf.h>
#include "core.h"
#include "getopt_long.h"
#ifndef PKGCONF_LITE
#include "renderer-msvc.h"
#endif

static bool
print_list_entry(const pkgconf_pkg_t *entry, void *data)
{
	const pkgconf_node_t *n;
	pkgconf_client_t *client = data;

	if (entry->flags & PKGCONF_PKG_PROPF_UNINSTALLED)
		return false;

	pkgconf_output_fmt(client->output, PKGCONF_OUTPUT_STDOUT,
			"%-30s %s - %s\n", entry->id, entry->realname, entry->description);

	PKGCONF_FOREACH_LIST_ENTRY(entry->provides.head, n)
	{
		const pkgconf_dependency_t *dep = n->data;

		if (!strcmp(dep->package, entry->id))
			continue;

		pkgconf_output_fmt(client->output, PKGCONF_OUTPUT_STDOUT,
			"%-30s %s - %s (provided by %s)\n", dep->package, entry->realname, entry->description, entry->id);
	}

	return false;
}

static bool
print_package_entry(const pkgconf_pkg_t *entry, void *data)
{
	const pkgconf_node_t *n;
	pkgconf_client_t *client = data;

	if (entry->flags & PKGCONF_PKG_PROPF_UNINSTALLED)
		return false;

	pkgconf_output_puts(client->output, PKGCONF_OUTPUT_STDOUT, entry->id);

	PKGCONF_FOREACH_LIST_ENTRY(entry->provides.head, n)
	{
		const pkgconf_dependency_t *dep = n->data;

		if (!strcmp(dep->package, entry->id))
			continue;

		pkgconf_output_puts(client->output, PKGCONF_OUTPUT_STDOUT, dep->package);
	}

	return false;
}

static bool
filter_cflags(const pkgconf_client_t *client, const pkgconf_fragment_t *frag, void *data)
{
	int got_flags = 0;
	pkgconf_cli_state_t *state = client->client_data;
	(void) data;

	if (!(state->want_flags & PKG_KEEP_SYSTEM_CFLAGS) && pkgconf_fragment_has_system_dir(client, frag))
		return false;

	if (state->want_fragment_filter != NULL && (strchr(state->want_fragment_filter, frag->type) == NULL || !frag->type))
		return false;

	if (frag->type == 'I')
		got_flags = PKG_CFLAGS_ONLY_I;
	else
		got_flags = PKG_CFLAGS_ONLY_OTHER;

	return (state->want_flags & got_flags) != 0;
}

static bool
filter_libs(const pkgconf_client_t *client, const pkgconf_fragment_t *frag, void *data)
{
	int got_flags = 0;
	pkgconf_cli_state_t *state = client->client_data;
	(void) data;

	if (!(state->want_flags & PKG_KEEP_SYSTEM_LIBS) && pkgconf_fragment_has_system_dir(client, frag))
		return false;

	if (state->want_fragment_filter != NULL && (strchr(state->want_fragment_filter, frag->type) == NULL || !frag->type))
		return false;

	switch (frag->type)
	{
		case 'L': got_flags = PKG_LIBS_ONLY_LDPATH; break;
		case 'l': got_flags = PKG_LIBS_ONLY_LIBNAME; break;
		default: got_flags = PKG_LIBS_ONLY_OTHER; break;
	}

	return (state->want_flags & got_flags) != 0;
}

static void
print_variables(pkgconf_output_t *output, pkgconf_pkg_t *pkg)
{
	pkgconf_node_t *node;

	PKGCONF_FOREACH_LIST_ENTRY(pkg->vars.head, node)
	{
		pkgconf_tuple_t *tuple = node->data;

		pkgconf_output_puts(output, PKGCONF_OUTPUT_STDOUT, tuple->key);
	}
}

static void
print_dependency_list(pkgconf_output_t *output, pkgconf_list_t *list)
{
	pkgconf_node_t *node;

	PKGCONF_FOREACH_LIST_ENTRY(list->head, node)
	{
		pkgconf_dependency_t *dep = node->data;

		pkgconf_output_fmt(output, PKGCONF_OUTPUT_STDOUT, "%s", dep->package);

		if (dep->version != NULL)
			pkgconf_output_fmt(output, PKGCONF_OUTPUT_STDOUT, " %s %s",
				pkgconf_pkg_get_comparator(dep), dep->version);

		pkgconf_output_fmt(output, PKGCONF_OUTPUT_STDOUT, "\n");
	}
}

static bool
apply_provides(pkgconf_client_t *client, pkgconf_pkg_t *world, void *unused, int maxdepth)
{
	pkgconf_node_t *iter;
	(void) unused;
	(void) maxdepth;

	PKGCONF_FOREACH_LIST_ENTRY(world->required.head, iter)
	{
		pkgconf_dependency_t *dep = iter->data;
		pkgconf_pkg_t *pkg = dep->match;

		print_dependency_list(client->output, &pkg->provides);
	}

	return true;
}

#ifndef PKGCONF_LITE
static void
print_digraph_node(pkgconf_client_t *client, pkgconf_pkg_t *pkg, void *data)
{
	pkgconf_node_t *node;
	pkgconf_pkg_t **last_seen = data;

	if (pkg->flags & PKGCONF_PKG_PROPF_VIRTUAL)
		return;

	pkgconf_output_fmt(client->output, PKGCONF_OUTPUT_STDOUT, "\"%s\" [fontname=Sans fontsize=8", pkg->id);

	if (pkg->flags & PKGCONF_PKG_PROPF_VISITED_PRIVATE)
		pkgconf_output_fmt(client->output, PKGCONF_OUTPUT_STDOUT, " fontcolor=gray color=gray");

	pkgconf_output_puts(client->output, PKGCONF_OUTPUT_STDOUT, "]");

	if (last_seen != NULL)
	{
		if (*last_seen != NULL)
			pkgconf_output_fmt(client->output, PKGCONF_OUTPUT_STDOUT,
				"\"%s\" -> \"%s\" [fontname=Sans fontsize=8 color=red]\n", (*last_seen)->id, pkg->id);

		*last_seen = pkg;
	}

	PKGCONF_FOREACH_LIST_ENTRY(pkg->required.head, node)
	{
		pkgconf_dependency_t *dep = node->data;
		const char *dep_id = (dep->match != NULL) ? dep->match->id : dep->package;

		pkgconf_output_fmt(client->output, PKGCONF_OUTPUT_STDOUT, "\"%s\" -> \"%s\" [fontname=Sans fontsize=8",
			pkg->id, dep_id);

		if (dep->flags & PKGCONF_PKG_DEPF_PRIVATE)
			pkgconf_output_fmt(client->output, PKGCONF_OUTPUT_STDOUT, " color=gray");

		pkgconf_output_puts(client->output, PKGCONF_OUTPUT_STDOUT, "]");
	}

	PKGCONF_FOREACH_LIST_ENTRY(pkg->requires_private.head, node)
	{
		pkgconf_dependency_t *dep = node->data;
		const char *dep_id = (dep->match != NULL) ? dep->match->id : dep->package;

		pkgconf_output_fmt(client->output, PKGCONF_OUTPUT_STDOUT,
			"\"%s\" -> \"%s\" [fontname=Sans fontsize=8 color=gray]\n", pkg->id, dep_id);
	}
}

static bool
apply_digraph(pkgconf_client_t *client, pkgconf_pkg_t *world, void *data, int maxdepth)
{
	int eflag;
	pkgconf_list_t *list = data;
	pkgconf_pkg_t *last_seen = NULL;
	pkgconf_node_t *iter;

	pkgconf_output_puts(client->output, PKGCONF_OUTPUT_STDOUT,
		"digraph deptree {\n"
		"edge [color=blue len=7.5 fontname=Sans fontsize=8]\n"
		"node [fontname=Sans fontsize=8]\n"
		"\"user:request\" [fontname=Sans fontsize=8]");

	PKGCONF_FOREACH_LIST_ENTRY(list->head, iter)
	{
		pkgconf_queue_t *pkgq = iter->data;
		pkgconf_pkg_t *pkg = pkgconf_pkg_find(client, pkgq->package);

		pkgconf_output_fmt(client->output, PKGCONF_OUTPUT_STDOUT,
			"\"user:request\" -> \"%s\" [fontname=Sans fontsize=8]\n",
			pkg == NULL ? pkgq->package : pkg->id);

		if (pkg != NULL)
			pkgconf_pkg_unref(client, pkg);
	}

	eflag = pkgconf_pkg_traverse(client, world, print_digraph_node, &last_seen, maxdepth, 0);

	if (eflag != PKGCONF_PKG_ERRF_OK)
		return false;

	pkgconf_output_puts(client->output, PKGCONF_OUTPUT_STDOUT, "}");
	return true;
}

static void
print_solution_node(pkgconf_client_t *client, pkgconf_pkg_t *pkg, void *unused)
{
	(void) unused;

	pkgconf_output_fmt(client->output, PKGCONF_OUTPUT_STDOUT, "%s (%"PRIu64")%s\n",
		pkg->id, pkg->identifier, (pkg->flags & PKGCONF_PKG_PROPF_VISITED_PRIVATE) == PKGCONF_PKG_PROPF_VISITED_PRIVATE ? " [private]" : "");
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
	pkgconf_cli_state_t *state = client->client_data;
	pkgconf_node_t *queue_iter;
	pkgconf_list_t *pkgq = data;
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
				if (state->verbosity)
					pkgconf_output_fmt(client->output, PKGCONF_OUTPUT_STDOUT, "%s: ", pkg->id);

				pkgconf_output_puts(client->output, PKGCONF_OUTPUT_STDOUT, pkg->version);
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
	(void) unused;
	(void) maxdepth;

	PKGCONF_FOREACH_LIST_ENTRY(world->required.head, iter)
	{
		pkgconf_dependency_t *dep = iter->data;
		pkgconf_pkg_t *pkg = dep->match;

		print_variables(client->output, pkg);
	}

	return true;
}

static bool
apply_path(pkgconf_client_t *client, pkgconf_pkg_t *world, void *unused, int maxdepth)
{
	pkgconf_node_t *iter;
	(void) unused;
	(void) maxdepth;

	PKGCONF_FOREACH_LIST_ENTRY(world->required.head, iter)
	{
		pkgconf_dependency_t *dep = iter->data;
		pkgconf_pkg_t *pkg = dep->match;

		/* a module entry with no filename is either virtual, static (builtin) or synthesized. */
		if (pkg->filename != NULL)
			pkgconf_output_puts(client->output, PKGCONF_OUTPUT_STDOUT, pkg->filename);
	}

	return true;
}

static bool
apply_variable(pkgconf_client_t *client, pkgconf_pkg_t *world, const void *variable, int maxdepth)
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
			pkgconf_output_fmt(client->output, PKGCONF_OUTPUT_STDOUT,
				"%s%s", iter->prev != NULL ? " " : "", var);
	}

	pkgconf_output_puts(client->output, PKGCONF_OUTPUT_STDOUT, "");

	return true;
}

static bool
apply_env_var(const char *prefix, pkgconf_client_t *client, pkgconf_pkg_t *world, int maxdepth,
	unsigned int (*collect_fn)(pkgconf_client_t *client, pkgconf_pkg_t *world, pkgconf_list_t *list, int maxdepth),
	bool (*filter_fn)(const pkgconf_client_t *client, const pkgconf_fragment_t *frag, void *data),
	void (*postprocess_fn)(pkgconf_client_t *client, pkgconf_pkg_t *world, pkgconf_list_t *fragment_list))
{
	pkgconf_cli_state_t *state = client->client_data;
	pkgconf_list_t unfiltered_list = PKGCONF_LIST_INITIALIZER;
	pkgconf_list_t filtered_list = PKGCONF_LIST_INITIALIZER;
	pkgconf_buffer_t render_buf = PKGCONF_BUFFER_INITIALIZER;
	unsigned int eflag;

	eflag = collect_fn(client, world, &unfiltered_list, maxdepth);
	if (eflag != PKGCONF_PKG_ERRF_OK)
		return false;

	pkgconf_fragment_filter(client, &filtered_list, &unfiltered_list, filter_fn, NULL);

	if (postprocess_fn != NULL)
		postprocess_fn(client, world, &filtered_list);

	if (filtered_list.head == NULL)
		goto out;

	pkgconf_fragment_render_buf(&filtered_list, &render_buf, true, state->want_render_ops, (state->want_flags & PKG_NEWLINES) ? '\n' : ' ');
	pkgconf_output_fmt(client->output, PKGCONF_OUTPUT_STDOUT, "%s='%s'\n",
		prefix, pkgconf_buffer_str_or_empty(&render_buf));
	pkgconf_buffer_finalize(&render_buf);

out:
	pkgconf_fragment_free(&unfiltered_list);
	pkgconf_fragment_free(&filtered_list);

	return true;
}

static void
maybe_add_module_definitions(pkgconf_client_t *client, pkgconf_pkg_t *world, pkgconf_list_t *fragment_list)
{
	pkgconf_node_t *world_iter;
	pkgconf_cli_state_t *state = client->client_data;

	if ((state->want_flags & PKG_EXISTS_CFLAGS) != PKG_EXISTS_CFLAGS)
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
	pkgconf_node_t *world_iter;
	pkgconf_cli_state_t *state = client->client_data;

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

			if (state->want_variable != NULL && strcmp(state->want_variable, tuple->key))
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

			pkgconf_output_fmt(client->output, PKGCONF_OUTPUT_STDOUT,
				"%s='%s'\n", havebuf, tuple->value);
		}
	}
}

static bool
apply_env(pkgconf_client_t *client, pkgconf_pkg_t *world, const void *env_prefix_p, int maxdepth)
{
	pkgconf_cli_state_t *state = client->client_data;
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

	if ((state->want_flags & PKG_VARIABLES) == PKG_VARIABLES || state->want_variable != NULL)
		apply_env_variables(client, world, want_env_prefix);

	return true;
}

static bool
apply_cflags(pkgconf_client_t *client, pkgconf_pkg_t *world, pkgconf_list_t *target_list, int maxdepth)
{
	pkgconf_list_t unfiltered_list = PKGCONF_LIST_INITIALIZER;
	pkgconf_list_t filtered_list = PKGCONF_LIST_INITIALIZER;
	int eflag;

	eflag = pkgconf_pkg_cflags(client, world, &unfiltered_list, maxdepth);
	if (eflag != PKGCONF_PKG_ERRF_OK)
		return false;

	pkgconf_fragment_filter(client, &filtered_list, &unfiltered_list, filter_cflags, NULL);
	maybe_add_module_definitions(client, world, &filtered_list);

	if (filtered_list.head == NULL)
		goto out;

	pkgconf_fragment_copy_list(client, target_list, &filtered_list);

out:
	pkgconf_fragment_free(&unfiltered_list);
	pkgconf_fragment_free(&filtered_list);

	return true;
}

static bool
apply_libs(pkgconf_client_t *client, pkgconf_pkg_t *world, pkgconf_list_t *target_list, int maxdepth)
{
	pkgconf_list_t unfiltered_list = PKGCONF_LIST_INITIALIZER;
	pkgconf_list_t filtered_list = PKGCONF_LIST_INITIALIZER;
	int eflag;

	eflag = pkgconf_pkg_libs(client, world, &unfiltered_list, maxdepth);
	if (eflag != PKGCONF_PKG_ERRF_OK)
		return false;

	pkgconf_fragment_filter(client, &filtered_list, &unfiltered_list, filter_libs, NULL);

	if (filtered_list.head == NULL)
		goto out;

	pkgconf_fragment_copy_list(client, target_list, &filtered_list);

out:
	pkgconf_fragment_free(&unfiltered_list);
	pkgconf_fragment_free(&filtered_list);

	return true;
}

static bool
apply_requires(pkgconf_client_t *client, pkgconf_pkg_t *world, void *unused, int maxdepth)
{
	pkgconf_node_t *iter;
	(void) unused;
	(void) maxdepth;

	PKGCONF_FOREACH_LIST_ENTRY(world->required.head, iter)
	{
		pkgconf_dependency_t *dep = iter->data;
		pkgconf_pkg_t *pkg = dep->match;

		print_dependency_list(client->output, &pkg->required);
	}

	return true;
}

static bool
apply_requires_private(pkgconf_client_t *client, pkgconf_pkg_t *world, void *unused, int maxdepth)
{
	pkgconf_node_t *iter;
	(void) unused;
	(void) maxdepth;

	PKGCONF_FOREACH_LIST_ENTRY(world->required.head, iter)
	{
		pkgconf_dependency_t *dep = iter->data;
		pkgconf_pkg_t *pkg = dep->match;

		print_dependency_list(client->output, &pkg->requires_private);
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

	(void) data;

	pkgconf_output_fmt(client->output, PKGCONF_OUTPUT_STDOUT, "node '%s' {\n", pkg->id);

	if (pkg->version != NULL)
		pkgconf_output_fmt(client->output, PKGCONF_OUTPUT_STDOUT, "    version = '%s';\n", pkg->version);

	PKGCONF_FOREACH_LIST_ENTRY(pkg->required.head, n)
	{
		pkgconf_dependency_t *dep = n->data;

		pkgconf_output_fmt(client->output, PKGCONF_OUTPUT_STDOUT, "    dependency '%s'", dep->package);

		if (dep->compare != PKGCONF_CMP_ANY)
		{
			pkgconf_output_fmt(client->output, PKGCONF_OUTPUT_STDOUT,
				" {\n"
				"        comparator = '%s';\n"
				"        version = '%s';\n"
				"    };\n",
				pkgconf_pkg_get_comparator(dep), dep->version);
		}
		else
			pkgconf_output_puts(client->output, PKGCONF_OUTPUT_STDOUT, ";");
	}

	pkgconf_output_puts(client->output, PKGCONF_OUTPUT_STDOUT, "};");
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
print_fragment_tree_branch(pkgconf_output_t *output, pkgconf_list_t *fragment_list, int indent)
{
	pkgconf_node_t *iter;

	PKGCONF_FOREACH_LIST_ENTRY(fragment_list->head, iter)
	{
		pkgconf_fragment_t *frag = iter->data;

		if (frag->type)
			pkgconf_output_fmt(output, PKGCONF_OUTPUT_STDOUT,
				"%*s'-%c%s' [type %c]\n", indent, "", frag->type, frag->data, frag->type);
		else
			pkgconf_output_fmt(output, PKGCONF_OUTPUT_STDOUT,
				"%*s'%s' [untyped]\n", indent, "", frag->data);

		print_fragment_tree_branch(output, &frag->children, indent + 2);
	}

	if (fragment_list->head != NULL)
		pkgconf_output_puts(output, PKGCONF_OUTPUT_STDOUT, "");
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

	print_fragment_tree_branch(client->output, &unfiltered_list, 0);
	pkgconf_fragment_free(&unfiltered_list);

	return true;
}

static void
print_license(pkgconf_client_t *client, pkgconf_pkg_t *pkg, void *data)
{
	(void) data;

	if (pkg->flags & PKGCONF_PKG_PROPF_VIRTUAL)
		return;

	/* NOASSERTION is the default when the license is unknown, per SPDX spec § 3.15 */
	pkgconf_output_fmt(client->output, PKGCONF_OUTPUT_STDOUT, "%s: %s\n",
		pkg->id, pkg->license != NULL ? pkg->license : "NOASSERTION");
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
print_license_file(pkgconf_client_t *client, pkgconf_pkg_t *pkg, void *data)
{
	(void) data;

	if (pkg->flags & PKGCONF_PKG_PROPF_VIRTUAL)
		return;

	/* If license file location is not available then just print empty */
	pkgconf_output_fmt(client->output, PKGCONF_OUTPUT_STDOUT, "%s: %s\n",
		pkg->id, pkg->license_file != NULL ? pkg->license_file : "");
}

static bool
apply_license_file(pkgconf_client_t *client, pkgconf_pkg_t *world, void *data, int maxdepth)
{
	int eflag;

	eflag = pkgconf_pkg_traverse(client, world, print_license_file, data, maxdepth, 0);

	if (eflag != PKGCONF_PKG_ERRF_OK)
		return false;

	return true;
}

static void
print_source(pkgconf_client_t *client, pkgconf_pkg_t *pkg, void *data)
{
	(void) data;

	if (pkg->flags & PKGCONF_PKG_PROPF_VIRTUAL)
		return;

	/* If source is empty then empty string is printed otherwise URL */
	pkgconf_output_fmt(client->output, PKGCONF_OUTPUT_STDOUT, "%s: %s\n",
		pkg->id, pkg->source != NULL ? pkg->source : "");
}

static bool
apply_source(pkgconf_client_t *client, pkgconf_pkg_t *world, void *data, int maxdepth)
{
	int eflag;

	eflag = pkgconf_pkg_traverse(client, world, print_source, data, maxdepth, 0);

	if (eflag != PKGCONF_PKG_ERRF_OK)
		return false;

	return true;
}

void
path_list_to_buffer(const pkgconf_list_t *list, pkgconf_buffer_t *buffer, char delim)
{
	pkgconf_node_t *n;

	PKGCONF_FOREACH_LIST_ENTRY(list->head, n)
	{
		pkgconf_path_t *pn = n->data;

		if (n != list->head)
			pkgconf_buffer_push_byte(buffer, delim);

		pkgconf_buffer_append(buffer, pn->path);
	}
}

static void
unveil_handler(const pkgconf_client_t *client, const char *path, const char *permissions)
{
	(void) client;

	if (pkgconf_unveil(path, permissions) == -1)
	{
		fprintf(stderr, "pkgconf: unveil failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
}

static bool
unveil_search_paths(pkgconf_client_t *client, const pkgconf_cross_personality_t *personality)
{
	pkgconf_node_t *n;

	if (pkgconf_unveil("/dev/null", "rwc") == -1)
		return false;

	PKGCONF_FOREACH_LIST_ENTRY(client->dir_list.head, n)
	{
		pkgconf_path_t *pn = n->data;

		if (pkgconf_unveil(pn->path, "r") == -1 && errno != ENOENT)
			return false;
	}

	PKGCONF_FOREACH_LIST_ENTRY(personality->dir_list.head, n)
	{
		pkgconf_path_t *pn = n->data;

		if (pkgconf_unveil(pn->path, "r") == -1 && errno != ENOENT)
			return false;
	}

	pkgconf_client_set_unveil_handler(client, unveil_handler);

	return true;
}

/* SAFETY: pkgconf_client_t takes ownership of these package objects */
static void
register_builtins(pkgconf_client_t *client, const pkgconf_cross_personality_t *personality)
{
	pkgconf_buffer_t pc_path_buf = PKGCONF_BUFFER_INITIALIZER;
	path_list_to_buffer(&personality->dir_list, &pc_path_buf, ':');

	pkgconf_buffer_t pc_system_libdirs_buf = PKGCONF_BUFFER_INITIALIZER;
	path_list_to_buffer(&personality->filter_libdirs, &pc_system_libdirs_buf, ':');

	pkgconf_buffer_t pc_system_includedirs_buf = PKGCONF_BUFFER_INITIALIZER;
	path_list_to_buffer(&personality->filter_includedirs, &pc_system_includedirs_buf, ':');

	pkgconf_pkg_t *pkg_config_virtual = calloc(1, sizeof(pkgconf_pkg_t));
	if (pkg_config_virtual == NULL)
	{
		goto error;
	}

	pkg_config_virtual->owner = client;
	pkg_config_virtual->id = strdup("pkg-config");
	pkg_config_virtual->realname = strdup("pkg-config");
	pkg_config_virtual->description = strdup("virtual package defining pkgconf API version supported");
	pkg_config_virtual->url = strdup(PACKAGE_BUGREPORT);
	pkg_config_virtual->version = strdup(PACKAGE_VERSION);

	pkgconf_tuple_add(client, &pkg_config_virtual->vars, "pc_system_libdirs", pkgconf_buffer_str_or_empty(&pc_system_libdirs_buf), false, 0);
	pkgconf_tuple_add(client, &pkg_config_virtual->vars, "pc_system_includedirs", pkgconf_buffer_str_or_empty(&pc_system_includedirs_buf), false, 0);
	pkgconf_tuple_add(client, &pkg_config_virtual->vars, "pc_path", pkgconf_buffer_str_or_empty(&pc_path_buf), false, 0);

	if (!pkgconf_client_preload_one(client, pkg_config_virtual))
	{
		goto error;
	}

	pkgconf_pkg_t *pkgconf_virtual = calloc(1, sizeof(pkgconf_pkg_t));
	if (pkgconf_virtual == NULL)
	{
		goto error;
	}

	pkgconf_virtual->owner = client;
	pkgconf_virtual->id = strdup("pkgconf");
	pkgconf_virtual->realname = strdup("pkgconf");
	pkgconf_virtual->description = strdup("virtual package defining pkgconf API version supported");
	pkgconf_virtual->url = strdup(PACKAGE_BUGREPORT);
	pkgconf_virtual->version = strdup(PACKAGE_VERSION);

	pkgconf_tuple_add(client, &pkgconf_virtual->vars, "pc_system_libdirs", pkgconf_buffer_str_or_empty(&pc_system_libdirs_buf), false, 0);
	pkgconf_tuple_add(client, &pkgconf_virtual->vars, "pc_system_includedirs", pkgconf_buffer_str_or_empty(&pc_system_includedirs_buf), false, 0);
	pkgconf_tuple_add(client, &pkgconf_virtual->vars, "pc_path", pkgconf_buffer_str_or_empty(&pc_path_buf), false, 0);

	if (!pkgconf_client_preload_one(client, pkgconf_virtual))
	{
		goto error;
	}

error:
	pkgconf_buffer_finalize(&pc_path_buf);
	pkgconf_buffer_finalize(&pc_system_libdirs_buf);
	pkgconf_buffer_finalize(&pc_system_includedirs_buf);
}

#ifndef PKGCONF_LITE
static void
dump_personality(const pkgconf_cross_personality_t *p)
{
	pkgconf_buffer_t pc_path_buf = PKGCONF_BUFFER_INITIALIZER;
	path_list_to_buffer(&p->dir_list, &pc_path_buf, ':');

	pkgconf_buffer_t pc_system_libdirs_buf = PKGCONF_BUFFER_INITIALIZER;
	path_list_to_buffer(&p->filter_libdirs, &pc_system_libdirs_buf, ':');

	pkgconf_buffer_t pc_system_includedirs_buf = PKGCONF_BUFFER_INITIALIZER;
	path_list_to_buffer(&p->filter_includedirs, &pc_system_includedirs_buf, ':');

	printf("Triplet: %s\n", p->name);

	if (p->sysroot_dir)
		printf("SysrootDir: %s\n", p->sysroot_dir);

	printf("DefaultSearchPaths: %s\n", pc_path_buf.base);
	printf("SystemIncludePaths: %s\n", pc_system_includedirs_buf.base);
	printf("SystemLibraryPaths: %s\n", pc_system_libdirs_buf.base);

	pkgconf_buffer_finalize(&pc_path_buf);
	pkgconf_buffer_finalize(&pc_system_libdirs_buf);
	pkgconf_buffer_finalize(&pc_system_includedirs_buf);
}
#endif

int
pkgconf_cli_run(pkgconf_cli_state_t *state, int argc, char *argv[], int last_argc)
{
	(void) argc;

	int ret = EXIT_SUCCESS;
	unsigned int want_client_flags = PKGCONF_PKG_PKGF_NONE;
	const char *builddir;
	const char *sysroot_dir;
	pkgconf_list_t pkgq = PKGCONF_LIST_INITIALIZER;
	pkgconf_list_t deplist = PKGCONF_LIST_INITIALIZER;
	pkgconf_node_t *node;
	pkgconf_buffer_t queryparams = PKGCONF_BUFFER_INITIALIZER;
	pkgconf_pkg_t world = {
		.id = "virtual:world",
		.realname = "virtual world package",
		.flags = PKGCONF_PKG_PROPF_STATIC | PKGCONF_PKG_PROPF_VIRTUAL,
	};

#ifndef PKGCONF_LITE
	if ((state->want_flags & PKG_DUMP_PERSONALITY) == PKG_DUMP_PERSONALITY)
	{
		dump_personality(state->pkg_client.personality);

		ret = EXIT_SUCCESS;
		goto out;
	}
#endif

#ifndef PKGCONF_LITE
	if ((state->want_flags & PKG_MSVC_SYNTAX) == PKG_MSVC_SYNTAX)
		state->want_render_ops = msvc_renderer_get();
#endif

	if (pkgconf_client_getenv(&state->pkg_client, "PKG_CONFIG_FDO_SYSROOT_RULES"))
		want_client_flags |= PKGCONF_PKG_PKGF_FDO_SYSROOT_RULES;

	if (pkgconf_client_getenv(&state->pkg_client, "PKG_CONFIG_PKGCONF1_SYSROOT_RULES"))
		want_client_flags |= PKGCONF_PKG_PKGF_PKGCONF1_SYSROOT_RULES;

	if ((state->want_flags & PKG_SHORT_ERRORS) == PKG_SHORT_ERRORS)
		want_client_flags |= PKGCONF_PKG_PKGF_SIMPLIFY_ERRORS;

	if ((state->want_flags & PKG_DONT_RELOCATE_PATHS) == PKG_DONT_RELOCATE_PATHS)
		want_client_flags |= PKGCONF_PKG_PKGF_DONT_RELOCATE_PATHS;

	state->error_msgout = stderr;
	if ((state->want_flags & PKG_ERRORS_ON_STDOUT) == PKG_ERRORS_ON_STDOUT)
		state->error_msgout = stdout;
	if ((state->want_flags & PKG_SILENCE_ERRORS) == PKG_SILENCE_ERRORS) {
		state->error_msgout = fopen(PATH_DEV_NULL, "w");
		state->opened_error_msgout = true;
	}

	if ((state->want_flags & PKG_IGNORE_CONFLICTS) == PKG_IGNORE_CONFLICTS || pkgconf_client_getenv(&state->pkg_client, "PKG_CONFIG_IGNORE_CONFLICTS") != NULL)
		want_client_flags |= PKGCONF_PKG_PKGF_SKIP_CONFLICTS;

	if ((state->want_flags & PKG_STATIC) == PKG_STATIC || state->pkg_client.personality->want_default_static)
		want_client_flags |= (PKGCONF_PKG_PKGF_SEARCH_PRIVATE | PKGCONF_PKG_PKGF_MERGE_PRIVATE_FRAGMENTS);

	if ((state->want_flags & PKG_SHARED) == PKG_SHARED)
		want_client_flags &= ~(PKGCONF_PKG_PKGF_SEARCH_PRIVATE | PKGCONF_PKG_PKGF_MERGE_PRIVATE_FRAGMENTS);

	/* if --static and --pure are both specified, then disable merge-back.
	 * this allows for a --static which searches private modules, but has the same fragment behaviour as if
	 * --static were disabled.  see <https://github.com/pkgconf/pkgconf/issues/83> for rationale.
	 */
	if ((state->want_flags & PKG_PURE) == PKG_PURE || pkgconf_client_getenv(&state->pkg_client, "PKG_CONFIG_PURE_DEPGRAPH") != NULL || state->pkg_client.personality->want_default_pure)
		want_client_flags &= ~PKGCONF_PKG_PKGF_MERGE_PRIVATE_FRAGMENTS;

	if ((state->want_flags & PKG_ENV_ONLY) == PKG_ENV_ONLY)
		want_client_flags |= PKGCONF_PKG_PKGF_ENV_ONLY;

	if ((state->want_flags & PKG_NO_CACHE) == PKG_NO_CACHE)
		want_client_flags |= PKGCONF_PKG_PKGF_NO_CACHE;

/* On Windows we want to always redefine the prefix by default
 * but allow that behavior to be manually disabled */
#if !defined(_WIN32) && !defined(_WIN64)
	if ((state->want_flags & PKG_DEFINE_PREFIX) == PKG_DEFINE_PREFIX || pkgconf_client_getenv(&state->pkg_client, "PKG_CONFIG_RELOCATE_PATHS") != NULL)
#endif
		want_client_flags |= PKGCONF_PKG_PKGF_REDEFINE_PREFIX;

	if ((state->want_flags & PKG_NO_UNINSTALLED) == PKG_NO_UNINSTALLED || pkgconf_client_getenv(&state->pkg_client, "PKG_CONFIG_DISABLE_UNINSTALLED") != NULL)
		want_client_flags |= PKGCONF_PKG_PKGF_NO_UNINSTALLED;

	if ((state->want_flags & PKG_NO_PROVIDES) == PKG_NO_PROVIDES)
		want_client_flags |= PKGCONF_PKG_PKGF_SKIP_PROVIDES;

	if ((state->want_flags & PKG_DONT_DEFINE_PREFIX) == PKG_DONT_DEFINE_PREFIX  || pkgconf_client_getenv(&state->pkg_client, "PKG_CONFIG_DONT_DEFINE_PREFIX") != NULL)
		want_client_flags &= ~PKGCONF_PKG_PKGF_REDEFINE_PREFIX;

	if ((state->want_flags & PKG_INTERNAL_CFLAGS) == PKG_INTERNAL_CFLAGS)
		want_client_flags |= PKGCONF_PKG_PKGF_DONT_FILTER_INTERNAL_CFLAGS;

	/* --static --libs, --exists require the full dependency graph to be solved */
	if ((state->want_flags & (PKG_STATIC|PKG_LIBS)) == (PKG_STATIC|PKG_LIBS) || (state->want_flags & PKG_EXISTS) == PKG_EXISTS)
		want_client_flags |= PKGCONF_PKG_PKGF_REQUIRE_INTERNAL;

	/* if these selectors are used, it means that we are querying metadata.
	 * so signal to libpkgconf that we only want to walk the flattened dependency set.
	 */
	if ((state->want_flags & PKG_MODVERSION) == PKG_MODVERSION ||
	    (state->want_flags & PKG_REQUIRES) == PKG_REQUIRES ||
	    (state->want_flags & PKG_REQUIRES_PRIVATE) == PKG_REQUIRES_PRIVATE ||
	    (state->want_flags & PKG_PROVIDES) == PKG_PROVIDES ||
	    (state->want_flags & PKG_VARIABLES) == PKG_VARIABLES ||
	    (state->want_flags & PKG_PATH) == PKG_PATH ||
	    state->want_variable != NULL)
		state->maximum_traverse_depth = 1;

	/* if we are asking for a variable, path or list of variables, this only makes sense
	 * for a single package.
	 */
	if ((state->want_flags & PKG_VARIABLES) == PKG_VARIABLES ||
	    (state->want_flags & PKG_PATH) == PKG_PATH ||
	    state->want_variable != NULL)
		state->maximum_package_count = 1;

	if ((state->want_flags & PKG_REQUIRES_PRIVATE) == PKG_REQUIRES_PRIVATE ||
		(state->want_flags & PKG_CFLAGS))
	{
		want_client_flags |= PKGCONF_PKG_PKGF_SEARCH_PRIVATE;
	}

	if ((builddir = pkgconf_client_getenv(&state->pkg_client, "PKG_CONFIG_TOP_BUILD_DIR")) != NULL)
		pkgconf_client_set_buildroot_dir(&state->pkg_client, builddir);

	if ((sysroot_dir = pkgconf_client_getenv(&state->pkg_client, "PKG_CONFIG_SYSROOT_DIR")) != NULL)
	{
		const char *destdir;

		pkgconf_client_set_sysroot_dir(&state->pkg_client, sysroot_dir);

		if ((destdir = pkgconf_client_getenv(&state->pkg_client, "DESTDIR")) != NULL)
		{
			if (!strcmp(destdir, sysroot_dir))
				want_client_flags |= PKGCONF_PKG_PKGF_FDO_SYSROOT_RULES;
		}
	}

	/* we have determined what features we want most likely.  in some cases, we override later. */
	pkgconf_client_set_flags(&state->pkg_client, want_client_flags);

	/* at this point, want_client_flags should be set, so build the dir list */
	pkgconf_client_dir_list_build(&state->pkg_client, state->pkg_client.personality);

	/* unveil the entire search path now that we have loaded the personality data and built the dir list. */
	if (!unveil_search_paths(&state->pkg_client, state->pkg_client.personality))
	{
		fprintf(stderr, "pkgconf: unveil failed: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	/* register built-in packages */
	register_builtins(&state->pkg_client, state->pkg_client.personality);

	/* preload any files in PKG_CONFIG_PRELOADED_FILES */
	pkgconf_client_preload_from_environ(&state->pkg_client, "PKG_CONFIG_PRELOADED_FILES");

	if (state->required_pkgconfig_version != NULL)
	{
		if (pkgconf_compare_version(PACKAGE_VERSION, state->required_pkgconfig_version) >= 0)
			ret = EXIT_SUCCESS;
		else
			ret = EXIT_FAILURE;

		goto out;
	}

	if ((state->want_flags & PKG_LIST) == PKG_LIST)
	{
		pkgconf_scan_all(&state->pkg_client, &state->pkg_client, print_list_entry);
		ret = EXIT_SUCCESS;
		goto out;
	}

	if ((state->want_flags & PKG_LIST_PACKAGE_NAMES) == PKG_LIST_PACKAGE_NAMES)
	{
		pkgconf_scan_all(&state->pkg_client, &state->pkg_client, print_package_entry);
		ret = EXIT_SUCCESS;
		goto out;
	}

	while (last_argc < argc && argv[last_argc])
	{
		if (pkgconf_buffer_len(&queryparams) > 0)
			pkgconf_buffer_push_byte(&queryparams, ' ');

		pkgconf_buffer_append(&queryparams, argv[last_argc]);
		last_argc++;
	}

	pkgconf_dependency_parse_str(&state->pkg_client, &deplist, pkgconf_buffer_str_or_empty(&queryparams), 0);
	pkgconf_buffer_finalize(&queryparams);

	if (state->required_module_version != NULL || state->required_exact_module_version != NULL || state->required_max_module_version != NULL)
	{
		const char *target_version = NULL;
		pkgconf_pkg_comparator_t compare;

		if (state->required_module_version != NULL)
		{
			target_version = state->required_module_version;
			compare = PKGCONF_CMP_GREATER_THAN_EQUAL;
		}
		else if (state->required_exact_module_version != NULL)
		{
			target_version = state->required_exact_module_version;
			compare = PKGCONF_CMP_EQUAL;
		}
		else if (state->required_max_module_version != NULL)
		{
			target_version = state->required_max_module_version;
			compare = PKGCONF_CMP_LESS_THAN_EQUAL;
		}

		PKGCONF_FOREACH_LIST_ENTRY(deplist.head, node)
		{
			pkgconf_dependency_t *dep = node->data;

			/* already constrained at query level */
			if (dep->compare != PKGCONF_CMP_ANY)
				continue;

			dep->compare = compare;
			dep->version = strdup(target_version);
		}
	}

	PKGCONF_FOREACH_LIST_ENTRY(deplist.head, node)
	{
		pkgconf_dependency_t *dep = node->data;
		pkgconf_queue_push_dependency(&pkgq, dep);
	}

	pkgconf_dependency_free(&deplist);

	if (pkgq.head == NULL)
	{
		pkgconf_output_puts(state->pkg_client.output, PKGCONF_OUTPUT_STDERR,
			"Please specify at least one package name on the command line.");
		ret = EXIT_FAILURE;
		goto out;
	}

	ret = EXIT_SUCCESS;

	if (!pkgconf_queue_solve(&state->pkg_client, &pkgq, &world, state->maximum_traverse_depth))
	{
		ret = EXIT_FAILURE;
		goto out;
	}

	/* we shouldn't need to unveil any more filesystem accesses from this point, so lock it down */
	if (pkgconf_unveil(NULL, NULL) == -1)
	{
		fprintf(stderr, "pkgconf: unveil lockdown failed: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

#ifndef PKGCONF_LITE
	if ((state->want_flags & PKG_SIMULATE) == PKG_SIMULATE)
	{
		state->want_flags &= ~(PKG_CFLAGS|PKG_LIBS);

		pkgconf_client_set_flags(&state->pkg_client, want_client_flags | PKGCONF_PKG_PKGF_SKIP_ERRORS);
		apply_simulate(&state->pkg_client, &world, NULL, -1);
	}
#endif

	if ((state->want_flags & PKG_VALIDATE) == PKG_VALIDATE)
		goto out;

	if ((state->want_flags & PKG_DUMP_LICENSE) == PKG_DUMP_LICENSE)
	{
		apply_license(&state->pkg_client, &world, &ret, 2);
		goto out;
	}

	if ((state->want_flags & PKG_DUMP_LICENSE_FILE) == PKG_DUMP_LICENSE_FILE)
	{
		apply_license_file(&state->pkg_client, &world, &ret, 2);
		goto out;
	}

	if ((state->want_flags & PKG_DUMP_SOURCE) == PKG_DUMP_SOURCE)
	{
		apply_source(&state->pkg_client, &world, &ret, 2);
		goto out;
	}


	if ((state->want_flags & PKG_UNINSTALLED) == PKG_UNINSTALLED)
	{
		ret = EXIT_FAILURE;
		apply_uninstalled(&state->pkg_client, &world, &ret, 2);
		goto out;
	}

	if (state->want_env_prefix != NULL)
	{
		apply_env(&state->pkg_client, &world, state->want_env_prefix, 2);
		goto out;
	}

	if ((state->want_flags & PKG_PROVIDES) == PKG_PROVIDES)
	{
		state->want_flags &= ~(PKG_CFLAGS|PKG_LIBS);
		apply_provides(&state->pkg_client, &world, NULL, 2);
	}

#ifndef PKGCONF_LITE
	if ((state->want_flags & PKG_DIGRAPH) == PKG_DIGRAPH)
	{
		state->want_flags &= ~(PKG_CFLAGS|PKG_LIBS);
		apply_digraph(&state->pkg_client, &world, &pkgq, 2);
	}

	if ((state->want_flags & PKG_SOLUTION) == PKG_SOLUTION)
	{
		state->want_flags &= ~(PKG_CFLAGS|PKG_LIBS);
		apply_print_solution(&state->pkg_client, &world, NULL, 2);
	}
#endif

	if ((state->want_flags & PKG_MODVERSION) == PKG_MODVERSION)
	{
		state->want_flags &= ~(PKG_CFLAGS|PKG_LIBS);
		apply_modversion(&state->pkg_client, &world, &pkgq, 2);
	}

	if ((state->want_flags & PKG_PATH) == PKG_PATH)
	{
		state->want_flags &= ~(PKG_CFLAGS|PKG_LIBS);

		pkgconf_client_set_flags(&state->pkg_client, want_client_flags | PKGCONF_PKG_PKGF_SKIP_ROOT_VIRTUAL);
		apply_path(&state->pkg_client, &world, NULL, 2);
	}

	if ((state->want_flags & PKG_VARIABLES) == PKG_VARIABLES)
	{
		state->want_flags &= ~(PKG_CFLAGS|PKG_LIBS);
		apply_variables(&state->pkg_client, &world, NULL, 2);
	}

	if (state->want_variable != NULL)
	{
		state->want_flags &= ~(PKG_CFLAGS|PKG_LIBS);

		pkgconf_client_set_flags(&state->pkg_client, want_client_flags | PKGCONF_PKG_PKGF_SKIP_ROOT_VIRTUAL);
		apply_variable(&state->pkg_client, &world, state->want_variable, 2);
	}

	if ((state->want_flags & PKG_REQUIRES) == PKG_REQUIRES)
	{
		state->want_flags &= ~(PKG_CFLAGS|PKG_LIBS);
		apply_requires(&state->pkg_client, &world, NULL, 2);
	}

	if ((state->want_flags & PKG_REQUIRES_PRIVATE) == PKG_REQUIRES_PRIVATE)
	{
		state->want_flags &= ~(PKG_CFLAGS|PKG_LIBS);

		apply_requires_private(&state->pkg_client, &world, NULL, 2);
	}

	if ((state->want_flags & PKG_FRAGMENT_TREE))
	{
		state->want_flags &= ~(PKG_CFLAGS|PKG_LIBS);

		apply_fragment_tree(&state->pkg_client, &world, NULL, 2);
	}

	if ((state->want_flags & (PKG_CFLAGS|PKG_LIBS)))
	{
		pkgconf_list_t target_list = PKGCONF_LIST_INITIALIZER;
		pkgconf_buffer_t render_buf = PKGCONF_BUFFER_INITIALIZER;

		if ((state->want_flags & PKG_CFLAGS))
			apply_cflags(&state->pkg_client, &world, &target_list, 2);

		if ((state->want_flags & PKG_LIBS) && !(state->want_flags & PKG_STATIC))
			pkgconf_client_set_flags(&state->pkg_client, state->pkg_client.flags & ~PKGCONF_PKG_PKGF_SEARCH_PRIVATE);

		if ((state->want_flags & PKG_LIBS))
			apply_libs(&state->pkg_client, &world, &target_list, 2);

		pkgconf_fragment_render_buf(&target_list, &render_buf, true, state->want_render_ops, (state->want_flags & PKG_NEWLINES) ? '\n' : ' ');
		pkgconf_output_putbuf(state->pkg_client.output, PKGCONF_OUTPUT_STDOUT, &render_buf, true);
		pkgconf_buffer_finalize(&render_buf);

		pkgconf_fragment_free(&target_list);
	}

out:
	pkgconf_solution_free(&state->pkg_client, &world);
	pkgconf_queue_free(&pkgq);
	pkgconf_cli_state_reset(state);

	return ret;
}

void
pkgconf_cli_state_reset(pkgconf_cli_state_t *state)
{
	pkgconf_cross_personality_deinit((void *) state->pkg_client.personality);
	pkgconf_client_deinit(&state->pkg_client);

	if (state->logfile_out != NULL)
		fclose(state->logfile_out);
	if (state->opened_error_msgout)
		fclose(state->error_msgout);
}
